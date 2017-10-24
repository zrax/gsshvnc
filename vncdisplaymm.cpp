/* This file is part of gsshvnc.
 *
 * gsshvnc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * gsshvnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gsshvnc.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "vncdisplaymm.h"

#include <glibmm/exceptionhandler.h>
#include <gtkmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/entry.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/settings.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <iostream>
#include <ctime>

#ifdef HAVE_PULSEAUDIO
#include <vncaudiopulse.h>
#endif

#ifdef __GNUC__
/* This is out of my control when gtk-vnc uses deprecated APIs */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

Vnc::DisplayWindow::DisplayWindow()
    : m_connected(false), m_accel_enabled(true), m_enable_mnemonics()
{
    m_vnc = Glib::wrap(vnc_display_new());

    auto layout = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    auto menubar = Gtk::manage(new Gtk::MenuBar);

#ifdef HAVE_PULSEAUDIO
    m_pulse_ifc = vnc_audio_pulse_new();
#endif

    set_resizable(true);

    auto remote = Gtk::manage(new Gtk::MenuItem("_Remote", true));
    menubar->append(*remote);

    auto submenu = Gtk::manage(new Gtk::Menu);

    m_capture_keyboard = Gtk::manage(new Gtk::CheckMenuItem("Capture _Keyboard", true));
    auto send_cad = Gtk::manage(new Gtk::MenuItem("Send Ctrl+Alt+_Del", true));
    auto screenshot = Gtk::manage(new Gtk::MenuItem("Take _Screenshot", true));
    auto appquit = Gtk::manage(new Gtk::MenuItem("_Quit", true));

    submenu->append(*m_capture_keyboard);
    submenu->append(*send_cad);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
    submenu->append(*screenshot);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
    submenu->append(*appquit);

    remote->set_submenu(*submenu);

    auto view = Gtk::manage(new Gtk::MenuItem("_View", true));
    menubar->append(*view);

    submenu = Gtk::manage(new Gtk::Menu);

    m_fullscreen = Gtk::manage(new Gtk::CheckMenuItem("_Full Screen", true));
    m_scaling = Gtk::manage(new Gtk::CheckMenuItem("_Scaled Display", true));

    submenu->append(*m_fullscreen);
    submenu->append(*m_scaling);

#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    m_smoothing = Gtk::manage(new Gtk::CheckMenuItem("S_mooth Scaling", true));
    m_smoothing->set_active(true);
    submenu->append(*m_smoothing);
#else
    m_smoothing = nullptr;
#endif

    view->set_submenu(*submenu);

    auto help = Gtk::manage(new Gtk::MenuItem("_Help", true));
    menubar->append(*help);

    submenu = Gtk::manage(new Gtk::Menu);

    auto about = Gtk::manage(new Gtk::MenuItem("_About", true));
    submenu->append(*about);

    help->set_submenu(*submenu);

    layout->pack_start(*menubar, false, true);
    layout->pack_start(*m_vnc, true, true);
    add(*layout);
    gtk_widget_realize(m_vnc->gobj());

    // Can't find a pre-wrapped C++ version of this anywhere...
    GSList *accels = gtk_accel_groups_from_object(G_OBJECT(gobj()));
    for ( ; accels; accels = accels->next) {
        auto group = Glib::wrap(GTK_ACCEL_GROUP(accels->data), true);
        m_accel_groups.emplace_back(group);
    }

    signal_vnc_connected().connect([this]() { m_connected = true; });
    signal_vnc_initialized().connect(sigc::mem_fun(this, &DisplayWindow::vnc_initialized));
    signal_vnc_disconnected().connect([this]() {
        if (m_connected) {
            // TODO: Offer to reconnect when connection lost unexpectedly
            Gtk::MessageDialog dialog(*this, "VNC Session Disconnected", false, Gtk::MESSAGE_INFO);
            (void)dialog.run();
        }
        m_connected = false;
        Gtk::Main::quit();
    });
    signal_vnc_error().connect([this](const Glib::ustring &message) {
        auto text = Glib::ustring::compose("VNC Error: %1", message);
        Gtk::MessageDialog dialog(*this, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        Gtk::Main::quit();
    });
    signal_vnc_auth_credential().connect(sigc::mem_fun(this, &DisplayWindow::vnc_credential));
    signal_vnc_auth_failure().connect([this](const Glib::ustring &message) {
        auto text = Glib::ustring::compose("VNC Authentication failed: %1", message);
        Gtk::MessageDialog dialog(*this, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        Gtk::Main::quit();
    });

    signal_vnc_pointer_grab().connect([this]() { update_title(true); });
    signal_vnc_pointer_ungrab().connect([this]() { update_title(false); });

    m_capture_keyboard->signal_activate().connect([this]() {
        if (m_capture_keyboard->get_active())
            disable_modifiers();
        else
            enable_modifiers();
    });
    send_cad->signal_activate().connect([this]() {
        send_keys({ GDK_KEY_Control_L, GDK_KEY_Alt_L, GDK_KEY_Delete });
    });
    screenshot->signal_activate().connect(sigc::mem_fun(this, &DisplayWindow::vnc_screenshot));

    appquit->signal_activate().connect([]() { Gtk::Main::quit(); });

    m_fullscreen->signal_toggled().connect([this]() {
        if (m_fullscreen->get_active())
            fullscreen();
        else
            unfullscreen();
    });
    m_scaling->signal_toggled().connect([this]() {
        on_set_scaling(m_scaling->get_active());
    });
#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    m_smoothing->signal_toggled().connect([this]() {
        on_set_smoothing(m_smoothing->get_active());
    });
#endif
}

bool Vnc::DisplayWindow::open_fd(int fd)
{
    return static_cast<bool>(vnc_display_open_fd(get_vnc(), fd));
}

bool Vnc::DisplayWindow::open_fd(int fd, const Glib::ustring &hostname)
{
    return static_cast<bool>(vnc_display_open_fd_with_hostname(get_vnc(), fd, hostname.c_str()));
}

bool Vnc::DisplayWindow::open_addr(Gio::SocketAddress *addr, const Glib::ustring &hostname)
{
    return static_cast<bool>(vnc_display_open_addr(get_vnc(),
                                                   addr ? addr->gobj() : nullptr,
                                                   hostname.c_str()));
}

bool Vnc::DisplayWindow::open_host(const Glib::ustring &host, const Glib::ustring &port)
{
    return static_cast<bool>(vnc_display_open_host(get_vnc(), host.c_str(), port.c_str()));
}

bool Vnc::DisplayWindow::is_open()
{
    return static_cast<bool>(vnc_display_is_open(get_vnc()));
}

void Vnc::DisplayWindow::close_vnc()
{
    vnc_display_close(get_vnc());
}

void Vnc::DisplayWindow::send_keys(const std::vector<guint> &keys)
{
    vnc_display_send_keys(get_vnc(), keys.data(), static_cast<int>(keys.size()));
}

void Vnc::DisplayWindow::send_keys(const std::vector<guint> &keys,
                                 VncDisplayKeyEvent kind)
{
    vnc_display_send_keys_ex(get_vnc(), keys.data(), static_cast<int>(keys.size()),
                             kind);
}

void Vnc::DisplayWindow::send_pointer(gint x, gint y, int buttonmask)
{
    vnc_display_send_pointer(get_vnc(), x, y, buttonmask);
}

void Vnc::DisplayWindow::set_grab_keys(Vnc::GrabSequence &seq)
{
    vnc_display_set_grab_keys(get_vnc(), seq.gobj());
}

Vnc::GrabSequence Vnc::DisplayWindow::get_grab_keys()
{
    return Vnc::GrabSequence(vnc_display_get_grab_keys(get_vnc()));
}

bool Vnc::DisplayWindow::set_credential(int type, const Glib::ustring &data)
{
    return static_cast<bool>(vnc_display_set_credential(get_vnc(), type, data.c_str()));
}

void Vnc::DisplayWindow::set_pointer_local(bool enable)
{
    vnc_display_set_pointer_local(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_pointer_local()
{
    return static_cast<bool>(vnc_display_get_pointer_local(get_vnc()));
}

void Vnc::DisplayWindow::set_pointer_grab(bool enable)
{
    vnc_display_set_pointer_grab(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_pointer_grab()
{
    return static_cast<bool>(vnc_display_get_pointer_grab(get_vnc()));
}

void Vnc::DisplayWindow::set_keyboard_grab(bool enable)
{
    vnc_display_set_keyboard_grab(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_keyboard_grab()
{
    return static_cast<bool>(vnc_display_get_keyboard_grab(get_vnc()));
}

void Vnc::DisplayWindow::set_read_only(bool enable)
{
    vnc_display_set_read_only(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_read_only()
{
    return static_cast<bool>(vnc_display_get_read_only(get_vnc()));
}

Glib::RefPtr<Gdk::Pixbuf> Vnc::DisplayWindow::get_pixbuf()
{
    return Glib::wrap(vnc_display_get_pixbuf(get_vnc()));
}

int Vnc::DisplayWindow::get_width()
{
    return vnc_display_get_width(get_vnc());
}

int Vnc::DisplayWindow::get_height()
{
    return vnc_display_get_height(get_vnc());
}

Glib::ustring Vnc::DisplayWindow::get_name()
{
    return vnc_display_get_name(get_vnc());
}

void Vnc::DisplayWindow::client_cut_text(const Glib::ustring &text)
{
    vnc_display_client_cut_text(get_vnc(), text.c_str());
}

void Vnc::DisplayWindow::set_lossy_encoding(bool enable)
{
    vnc_display_set_lossy_encoding(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_lossy_encoding()
{
    return static_cast<bool>(vnc_display_get_lossy_encoding(get_vnc()));
}

bool Vnc::DisplayWindow::on_set_scaling(bool enable)
{
    // Why does this return a result?
    return static_cast<bool>(vnc_display_set_scaling(get_vnc(), enable));
}

bool Vnc::DisplayWindow::set_scaling(bool enable)
{
    bool result = on_set_scaling(enable);
    m_scaling->set_active(enable);
    return result;
}

bool Vnc::DisplayWindow::get_scaling()
{
    return static_cast<bool>(vnc_display_get_scaling(get_vnc()));
}

void Vnc::DisplayWindow::set_force_size(bool enable)
{
    vnc_display_set_force_size(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_force_size()
{
    return static_cast<bool>(vnc_display_get_force_size(get_vnc()));
}

void Vnc::DisplayWindow::on_set_smoothing(bool enable)
{
#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    vnc_display_set_smoothing(get_vnc(), enable);
#else
    (void)enable;
#endif
}

void Vnc::DisplayWindow::set_smoothing(bool enable)
{
    on_set_smoothing(enable);
#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    m_smoothing->set_active(enable);
#endif
}

bool Vnc::DisplayWindow::get_smoothing()
{
#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    return static_cast<bool>(vnc_display_get_smoothing(get_vnc()));
#else
    /* Always enabled for gtk-vnc < 0.7.0 */
    return true;
#endif
}

void Vnc::DisplayWindow::set_shared_flag(bool enable)
{
    vnc_display_set_shared_flag(get_vnc(), enable);
}

bool Vnc::DisplayWindow::get_shared_flag()
{
    return static_cast<bool>(vnc_display_get_shared_flag(get_vnc()));
}

void Vnc::DisplayWindow::set_depth(VncDisplayDepthColor depth)
{
    vnc_display_set_depth(get_vnc(), depth);
}

VncDisplayDepthColor Vnc::DisplayWindow::get_depth()
{
    return vnc_display_get_depth(get_vnc());
}

void Vnc::DisplayWindow::force_grab(bool enable)
{
    vnc_display_force_grab(get_vnc(), enable);
}

bool Vnc::DisplayWindow::is_pointer_absolute()
{
    return static_cast<bool>(vnc_display_is_pointer_absolute(get_vnc()));
}

Glib::OptionGroup &Vnc::DisplayWindow::option_group()
{
    static Glib::OptionGroup s_option_group(vnc_display_get_option_group());
    return s_option_group;
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_connected =
{
    "vnc-connected",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_connected()
{
    return {m_vnc, &VncDisplay_signal_vnc_connected};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_initialized =
{
    "vnc-initialized",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_initialized()
{
    return {m_vnc, &VncDisplay_signal_vnc_initialized};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_disconnected =
{
    "vnc-disconnected",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_disconnected()
{
    return {m_vnc, &VncDisplay_signal_vnc_disconnected};
}

static void VncDisplay_signal_vnc_error_callback(GtkWidget *self,
                                                 const gchar *message,
                                                 void *data)
{
    using SlotType = sigc::slot<void, const Glib::ustring &>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(message ? Glib::ustring(message) : Glib::ustring());
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_error =
{
    "vnc-error",
    (GCallback) &VncDisplay_signal_vnc_error_callback,
    (GCallback) &VncDisplay_signal_vnc_error_callback
};

Glib::SignalProxy1<void, const Glib::ustring &> Vnc::DisplayWindow::signal_vnc_error()
{
    return {m_vnc, &VncDisplay_signal_vnc_error};
}

static void VncDisplay_signal_vnc_auth_credential_callback(GtkWidget *self,
                                                           GValueArray *creds,
                                                           void *data)
{
    using SlotType = sigc::slot<void, const std::vector<VncDisplayCredential> &>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            std::vector<VncDisplayCredential> creds_vec;
            if (creds) {
                creds_vec.resize(creds->n_values);
                for (guint i = 0; i < creds->n_values; ++i) {
                    GValue *cred = g_value_array_get_nth(creds, i);
                    creds_vec[i] = static_cast<VncDisplayCredential>(g_value_get_enum(cred));
                }
            }
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(creds_vec);
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_auth_credential =
{
    "vnc-auth-credential",
    (GCallback) &VncDisplay_signal_vnc_auth_credential_callback,
    (GCallback) &VncDisplay_signal_vnc_auth_credential_callback
};

Glib::SignalProxy1<void, const std::vector<VncDisplayCredential> &>
Vnc::DisplayWindow::signal_vnc_auth_credential()
{
    return {m_vnc, &VncDisplay_signal_vnc_auth_credential};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_pointer_grab =
{
    "vnc-pointer-grab",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_pointer_grab()
{
    return {m_vnc, &VncDisplay_signal_vnc_pointer_grab};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_pointer_ungrab =
{
    "vnc-pointer-ungrab",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_pointer_ungrab()
{
    return {m_vnc, &VncDisplay_signal_vnc_pointer_ungrab};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_keyboard_grab =
{
    "vnc-keyboard-grab",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_keyboard_grab()
{
    return {m_vnc, &VncDisplay_signal_vnc_keyboard_grab};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_keyboard_ungrab =
{
    "vnc-keyboard-ungrab",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_keyboard_ungrab()
{
    return {m_vnc, &VncDisplay_signal_vnc_keyboard_ungrab};
}

static void VncDisplay_signal_vnc_desktop_resize_callback(GtkWidget *self,
                                                          gint width, gint height,
                                                          void *data)
{
    using SlotType = sigc::slot<void, gint, gint>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(width, height);
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_desktop_resize =
{
    "vnc-desktop-resize",
    (GCallback) &VncDisplay_signal_vnc_desktop_resize_callback,
    (GCallback) &VncDisplay_signal_vnc_desktop_resize_callback
};

Glib::SignalProxy2<void, gint, gint> Vnc::DisplayWindow::signal_vnc_desktop_resize()
{
    return {m_vnc, &VncDisplay_signal_vnc_desktop_resize};
}

static void VncDisplay_signal_vnc_auth_failure_callback(GtkWidget *self,
                                                        const gchar *message,
                                                        void *data)
{
    using SlotType = sigc::slot<void, const Glib::ustring &>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(message ? Glib::ustring(message) : Glib::ustring());
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_auth_failure =
{
    "vnc-auth-failure",
    (GCallback) &VncDisplay_signal_vnc_auth_failure_callback,
    (GCallback) &VncDisplay_signal_vnc_auth_failure_callback
};

Glib::SignalProxy1<void, const Glib::ustring &> Vnc::DisplayWindow::signal_vnc_auth_failure()
{
    return {m_vnc, &VncDisplay_signal_vnc_auth_failure};
}

static void VncDisplay_signal_vnc_auth_unsupported_callback(GtkWidget *self,
                                                            guint authType,
                                                            void *data)
{
    using SlotType = sigc::slot<void, guint>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(authType);
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_auth_unsupported =
{
    "vnc-auth-unsupported",
    (GCallback) &VncDisplay_signal_vnc_auth_unsupported_callback,
    (GCallback) &VncDisplay_signal_vnc_auth_unsupported_callback
};

Glib::SignalProxy1<void, guint> Vnc::DisplayWindow::signal_vnc_auth_unsupported()
{
    return {m_vnc, &VncDisplay_signal_vnc_auth_unsupported};
}

static void VncDisplay_signal_vnc_server_cut_text_callback(GtkWidget *self,
                                                           const gchar *text,
                                                           void *data)
{
    using SlotType = sigc::slot<void, const Glib::ustring &>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(text ? Glib::ustring(text) : Glib::ustring());
        } catch (...) {
            Glib::exception_handlers_invoke();
        }
    }
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_server_cut_text =
{
    "vnc-server-cut-text",
    (GCallback) &VncDisplay_signal_vnc_server_cut_text_callback,
    (GCallback) &VncDisplay_signal_vnc_server_cut_text_callback
};

Glib::SignalProxy1<void, const Glib::ustring &> Vnc::DisplayWindow::signal_vnc_server_cut_text()
{
    return {m_vnc, &VncDisplay_signal_vnc_server_cut_text};
}

static const Glib::SignalProxyInfo VncDisplay_signal_vnc_bell =
{
    "vnc-bell",
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback,
    (GCallback) &Glib::SignalProxyNormal::slot0_void_callback
};

Glib::SignalProxy0<void> Vnc::DisplayWindow::signal_vnc_bell()
{
    return {m_vnc, &VncDisplay_signal_vnc_bell};
}

void Vnc::DisplayWindow::set_grab_keyboard(bool enable)
{
    if (enable)
        disable_modifiers();
    else
        enable_modifiers();
    m_capture_keyboard->set_active(enable);
}

bool Vnc::DisplayWindow::get_grab_keyboard()
{
    return m_capture_keyboard->get_active();
}

VncDisplay *Vnc::DisplayWindow::get_vnc()
{
    return VNC_DISPLAY(m_vnc->gobj());
}

static Glib::ustring gen_screenshot_name(const Glib::ustring &name)
{
    char time_buf[64];
    time_t now = time(nullptr);
    struct tm *now_tm = localtime(&now);
    strftime(time_buf, 64, "%Y-%m-%d_%H%M%S", now_tm);
    std::string clean_name = name;
    std::replace_if(clean_name.begin(), clean_name.end(), [](char ch) -> bool {
        return (ch == ':' || ch == '\\' || ch == '/' || ch == '<' || ch == '>'
                || ch == '*' || ch == '?' || ch == '"' || ch == '|');
    }, '_');
    return Glib::ustring::compose("gsshvnc-%1-%2.png", clean_name, time_buf);
}

void Vnc::DisplayWindow::vnc_screenshot()
{
    /* Do this right away, in case the display changes by the time we pick
     * a filename and save it to disk. */
    auto pix = get_pixbuf();

    Gtk::FileChooserDialog dialog(*this, "Save Screenshot", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_local_only(true);
    dialog.set_do_overwrite_confirmation(true);
    dialog.set_current_name(gen_screenshot_name(get_name()));
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);

    auto filter_png = Gtk::FileFilter::create();
    filter_png->set_name("PNG Files");
    filter_png->add_mime_type("image/png");
    dialog.add_filter(filter_png);

    int response = dialog.run();
    if (response == Gtk::RESPONSE_OK) {
        auto filename = dialog.get_filename();
        if (!filename.empty()) {
            pix->save(filename, "png", {"tEXt::Generator App"}, {"gsshvnc"});
            std::cout << "Screenshot saved to " << filename << std::endl;
        }
    }
}

void Vnc::DisplayWindow::vnc_initialized()
{
    update_title(false);
    show_all();

#ifdef HAVE_PULSEAUDIO
    VncAudioFormat format = {
        VNC_AUDIO_FORMAT_RAW_S32,
        2,
        44100,
    };
    VncConnection *conn = vnc_display_get_connection(get_vnc());
    vnc_connection_set_audio_format(conn, &format);
    vnc_connection_set_audio(conn, VNC_AUDIO(m_pulse_ifc));
    vnc_connection_audio_enable(conn);
#endif
}

void Vnc::DisplayWindow::update_title(bool grabbed)
{
    const auto name = get_name();
    auto seq = get_grab_keys();
    auto seqstr = seq.as_string();
    Glib::ustring title;

    if (grabbed) {
        title = Glib::ustring::compose("(Press %1 to release pointer) %2 - GSshVnc",
                                       seqstr, name);
    } else {
        title = Glib::ustring::compose("%1 - GSshVnc", name);
    }

    set_title(title);
}

void Vnc::DisplayWindow::vnc_credential(const std::vector<VncDisplayCredential> &credList)
{
    std::vector<std::pair<Glib::ustring, bool>> data;
    data.resize(credList.size(), {Glib::ustring(), false});

    unsigned int prompt = 0;
    for (size_t i = 0; i < credList.size(); ++i) {
        switch (credList[i]) {
        case VNC_DISPLAY_CREDENTIAL_USERNAME:
        case VNC_DISPLAY_CREDENTIAL_PASSWORD:
            prompt++;
            break;
        case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
            data[i] = {"gsshvnc", true};
        default:
            break;
        }
    }

    std::unique_ptr<Gtk::Dialog> dialog;
    if (prompt) {
        dialog = std::make_unique<Gtk::Dialog>("VNC Authentication", *this);
        dialog->add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog->add_button("_Ok", Gtk::RESPONSE_OK);
        dialog->set_default_response(Gtk::RESPONSE_OK);

        auto *grid = Gtk::manage(new Gtk::Grid);
        grid->set_row_spacing(3);
        grid->set_column_spacing(3);
        grid->set_border_width(3);
        std::vector<Gtk::Label *> label;
        std::vector<Gtk::Entry *> entry;
        label.resize(prompt, nullptr);
        entry.resize(prompt, nullptr);

        int row = 0;
        for (size_t i = 0; i < credList.size(); ++i) {
            auto cred = credList[i];
            entry[row] = Gtk::manage(new Gtk::Entry);
            switch (cred) {
            case VNC_DISPLAY_CREDENTIAL_USERNAME:
                label[row] = Gtk::manage(new Gtk::Label("Username:"));
                break;
            case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                label[row] = Gtk::manage(new Gtk::Label("Password:"));
                entry[row]->set_activates_default(true);
                break;
            default:
                continue;
            }
            if (cred == VNC_DISPLAY_CREDENTIAL_PASSWORD)
                entry[row]->set_visibility(false);

            grid->attach(*label[i], 0, row, 1, 1);
            grid->attach(*entry[i], 1, row, 1, 1);
            row++;
        }

        auto vbox = dialog->get_child();
        dynamic_cast<Gtk::Container *>(vbox)->add(*grid);

        dialog->show_all();
        int response = dialog->run();
        dialog->hide();

        if (response == Gtk::RESPONSE_OK) {
            int row = 0;
            for (size_t i = 0; i < credList.size(); ++i) {
                switch (credList[i]) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                    data[i] = {entry[row]->get_text(), true};
                    break;
                default:
                    continue;
                }
                row++;
            }
        } else {
            close_vnc();
            return;
        }
    }

    for (size_t i = 0 ; i < credList.size() ; i++) {
        auto cred = credList[i];
        if (data[i].second) {
            if (set_credential(cred, data[i].first)) {
                std::cerr << "Failed to set credential type " << cred << std::endl;
                close_vnc();
            }
        } else {
            std::cerr << "Unsupported credential type " << cred << std::endl;
            close_vnc();
        }
    }
}

void Vnc::DisplayWindow::disable_modifiers()
{
    if (!m_accel_enabled)
        return;

    auto settings = Gtk::Settings::get_default();

    /* This stops F10 activating menu bar */
    m_menu_bar_accel = settings->property_gtk_menu_bar_accel().get_value();
    settings->property_gtk_menu_bar_accel().set_value("");

    /* This stops global accelerators like Ctrl+Q == Quit */
    for (const auto &accel: m_accel_groups)
        remove_accel_group(accel);

    /* This stops menu bar shortcuts like Alt+F == File */
    m_enable_mnemonics = settings->property_gtk_enable_mnemonics().get_value();
    settings->property_gtk_enable_mnemonics().set_value(false);

    m_accel_enabled = false;
}

void Vnc::DisplayWindow::enable_modifiers()
{
    if (m_accel_enabled)
        return;

    auto settings = Gtk::Settings::get_default();

    /* This allows F10 activating menu bar */
    settings->property_gtk_menu_bar_accel().set_value(m_menu_bar_accel);

    /* This allows global accelerators like Ctrl+Q == Quit */
    for (const auto &accel: m_accel_groups)
        add_accel_group(accel);

    /* This allows menu bar shortcuts like Alt+F == File */
    settings->property_gtk_enable_mnemonics().set_value(m_enable_mnemonics);

    m_accel_enabled = true;
}
