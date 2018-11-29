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
#include "appsettings.h"
#include "credstorage.h"

#include <glibmm/exceptionhandler.h>
#include <glibmm/main.h>
#include <glibmm/convert.h>
#include <giomm/socketaddress.h>
#include <gtkmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/menubar.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/settings.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/aboutdialog.h>
#include <iostream>
#include <ctime>

#ifdef HAVE_PULSEAUDIO
#include <vncaudiopulse.h>
#endif

#ifdef __GNUC__
/* This is out of my control when gtk-vnc uses deprecated APIs */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static Vnc::DisplayWindow *s_instance = nullptr;

static gboolean _activate_menubar(Vnc::DisplayWindow *self, GtkAccelGroup *,
                                  GObject *, guint, GdkModifierType)
{
    self->activate_menubar();
    return TRUE;
}

Vnc::DisplayWindow::DisplayWindow()
    : m_vnc(), m_connected(false), m_accel_enabled(true), m_enable_mnemonics()
{
    if (s_instance) {
        std::cerr << "WARNING: Creating multiple Vnc::DisplayWindow instances is not supported"
                  << std::endl;
    }
    s_instance = this;

    set_default_icon_name("preferences-desktop-remote-desktop");

    m_remote_size.width = -1;
    m_remote_size.height = -1;
    m_viewport = Gtk::manage(new Gtk::ScrolledWindow);

    auto layout = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
    m_menubar = Gtk::manage(new Gtk::MenuBar);

#ifdef HAVE_PULSEAUDIO
    m_pulse_ifc = vnc_audio_pulse_new();
#endif

    set_resizable(true);

    auto remote = Gtk::manage(new Gtk::MenuItem("_Remote", true));
    m_menubar->append(*remote);

    auto submenu = Gtk::manage(new Gtk::Menu);

    m_capture_keyboard = Gtk::manage(new Gtk::CheckMenuItem("Capture All _Keyboard Input", true));
    auto send_f8 = Gtk::manage(new Gtk::MenuItem("Send F8", true));
    auto send_cad = Gtk::manage(new Gtk::MenuItem("Send Ctrl+Alt+_Del", true));
    auto screenshot = Gtk::manage(new Gtk::MenuItem("Take _Screenshot", true));
    auto appquit = Gtk::manage(new Gtk::MenuItem("_Quit", true));

    submenu->append(*m_capture_keyboard);
    submenu->append(*send_f8);
    submenu->append(*send_cad);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
    submenu->append(*screenshot);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
    submenu->append(*appquit);

    remote->set_submenu(*submenu);

    auto view = Gtk::manage(new Gtk::MenuItem("_View", true));
    m_menubar->append(*view);

    submenu = Gtk::manage(new Gtk::Menu);

    m_hide_menubar = Gtk::manage(new Gtk::CheckMenuItem("_Hide Menu Bar", true));
    m_fullscreen = Gtk::manage(new Gtk::CheckMenuItem("_Full Screen", true));
    m_scaling = Gtk::manage(new Gtk::CheckMenuItem("_Scaled Display", true));

    auto activate_menubar_accel = Gtk::AccelGroup::create();
    add_accel_group(activate_menubar_accel);
    gtk_accel_group_connect(activate_menubar_accel->gobj(), GDK_KEY_F8,
                            GdkModifierType(0), GtkAccelFlags(0),
                            g_cclosure_new_swap(G_CALLBACK(&_activate_menubar), this, nullptr));

    submenu->append(*m_hide_menubar);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
    submenu->append(*m_fullscreen);
    submenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem));
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
    m_menubar->append(*help);

    submenu = Gtk::manage(new Gtk::Menu);

    auto about = Gtk::manage(new Gtk::MenuItem("_About", true));
    submenu->append(*about);

    help->set_submenu(*submenu);

    layout->pack_start(*m_menubar, false, true);
    layout->pack_start(*m_viewport, true, true);
    add(*layout);

    // Minimal size in case a fixed size is not later requested
    set_size_request(600, 400);

    m_capture_keyboard->signal_activate().connect([this]() {
        bool enable = m_capture_keyboard->get_active();
        if (enable)
            disable_modifiers();
        else
            enable_modifiers();
        set_keyboard_grab(enable);

        AppSettings settings;
        settings.set_capture_keyboard(enable);
    });
    send_cad->signal_activate().connect([this]() {
        send_keys({ GDK_KEY_Control_L, GDK_KEY_Alt_L, GDK_KEY_Delete });
    });
    send_f8->signal_activate().connect([this]() {
        send_keys({ GDK_KEY_F8 });
    });
    screenshot->signal_activate().connect(sigc::mem_fun(this, &DisplayWindow::vnc_screenshot));

    appquit->signal_activate().connect([]() { Gtk::Main::quit(); });

    m_hide_menubar->signal_activate().connect([this]() {
        toggle_menubar();
    });
    m_fullscreen->signal_toggled().connect([this]() {
        if (m_fullscreen->get_active())
            fullscreen();
        else
            unfullscreen();
    });
    m_scaling->signal_toggled().connect([this]() {
        bool enable = m_scaling->get_active();
        on_set_scaling(enable);
        update_scrolling();

        AppSettings settings;
        settings.set_scaled_display(enable);
    });
#ifdef GTK_VNC_HAVE_SMOOTH_SCALING
    m_smoothing->signal_toggled().connect([this]() {
        bool enable = m_smoothing->get_active();
        on_set_smoothing(enable);

        AppSettings settings;
        settings.set_smooth_scaling(enable);
    });
#endif

    about->signal_activate().connect([this]() {
        Gtk::AboutDialog dialog;
        dialog.set_transient_for(*this);
        dialog.set_program_name("gsshvnc");
        dialog.set_version(GSSHVNC_VERSION_STR);
        dialog.set_logo_icon_name("preferences-desktop-remote-desktop");
        dialog.set_comments("gsshvnc (pronounced \"Gosh VNC\") is a simple VNC "
                            "client with built-in support for SSH tunneling");
        dialog.set_authors({"Michael Hansen <zrax0111@gmail.com>"});
        dialog.set_copyright("Copyright \u00a9 2017 Michael Hansen");
        dialog.set_license_type(Gtk::LICENSE_GPL_2_0);
        dialog.set_website("https://github.com/zrax/gsshvnc");
        (void)dialog.run();
    });

    // Don't capture keyboard when the window doesn't have focus
    add_events(Gdk::FOCUS_CHANGE_MASK);
    signal_focus_out_event().connect([this](GdkEventFocus *) -> bool {
        if (m_capture_keyboard->get_active()) {
            set_keyboard_grab(false);
            set_pointer_grab(false);
        }
        return false;
    });
    signal_focus_in_event().connect([this](GdkEventFocus *) -> bool {
        if (m_capture_keyboard->get_active()) {
            set_keyboard_grab(true);
            set_pointer_grab(true);
        }
        return false;
    });

    signal_show().connect([this]() {
        if (m_hide_menubar->get_active())
            m_menubar->hide();
    });
}

Vnc::DisplayWindow::~DisplayWindow()
{
    s_instance = nullptr;
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
    auto vnc_name = vnc_display_get_name(get_vnc());
    if (m_ssh_host.empty())
        return vnc_name;

    return Glib::ustring::compose("%1 [%2]", vnc_name, m_ssh_host);
}

void Vnc::DisplayWindow::client_cut_text(const std::string &text)
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
    update_scrolling();
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
    using SlotType = sigc::slot<void, const std::string &>;
    auto obj = dynamic_cast<Gtk::Widget *>(Glib::ObjectBase::_get_current_wrapper((GObject *)self));
    if (obj) {
        try {
            if (const auto slot = Glib::SignalProxyNormal::data_to_slot(data))
                (*static_cast<SlotType *>(slot))(text ? std::string(text) : std::string());
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

Glib::SignalProxy1<void, const std::string &> Vnc::DisplayWindow::signal_vnc_server_cut_text()
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

void Vnc::DisplayWindow::set_capture_keyboard(bool enable)
{
    if (enable)
        disable_modifiers();
    else
        enable_modifiers();
    set_keyboard_grab(enable);
    m_capture_keyboard->set_active(enable);
}

bool Vnc::DisplayWindow::get_capture_keyboard()
{
    return m_capture_keyboard->get_active();
}

void Vnc::DisplayWindow::activate_menubar()
{
    if (m_hide_menubar->get_active()) {
        m_hide_menubar->set_active(false);
        toggle_menubar();
    }
    m_menubar->select_first();
}

VncDisplay *Vnc::DisplayWindow::get_vnc()
{
    init_vnc();
    return VNC_DISPLAY(m_vnc->gobj());
}

void Vnc::DisplayWindow::init_vnc()
{
    if (m_vnc)
        return;

    m_vnc = Glib::wrap(vnc_display_new());
    m_viewport->remove();
    m_viewport->add(*m_vnc);
    gtk_widget_realize(m_vnc->gobj());

    signal_vnc_connected().connect([this]() { m_connected = true; });
    signal_vnc_initialized().connect(sigc::mem_fun(this, &DisplayWindow::vnc_initialized));
    signal_vnc_disconnected().connect([this]() {
        handle_disconnect("VNC connection lost",
                          "Could not open VNC connection");
    });
    signal_vnc_error().connect([this](const Glib::ustring &message) {
        auto text = Glib::ustring::compose("VNC Error: %1", message);
        handle_disconnect(text, text);
    });
    signal_vnc_auth_credential().connect(sigc::mem_fun(this, &DisplayWindow::vnc_credential));
    signal_vnc_auth_failure().connect([this](const Glib::ustring &message) {
        auto text = Glib::ustring::compose("VNC Authentication failed: %1", message);
        Gtk::MessageDialog dialog(*this, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        Gtk::Main::quit();
    });

    signal_vnc_desktop_resize().connect([this](gint width, gint height) {
        m_remote_size.width = width;
        m_remote_size.height = height;
        if (m_hide_menubar->get_active())
            resize(width, height);
        else
            resize(width, height + m_menubar->get_height());
        update_scrolling();
    });

    signal_vnc_pointer_grab().connect([this]() { update_title(true); });
    signal_vnc_pointer_ungrab().connect([this]() { update_title(false); });

    auto clipboard = Gtk::Clipboard::get();
    signal_vnc_server_cut_text().connect(sigc::mem_fun(this, &DisplayWindow::remote_clipboard_text));

    static bool clipboard_connected = false;
    if (!clipboard_connected) {
        clipboard->signal_owner_change().connect([this, clipboard](GdkEventOwnerChange *) {
            clipboard->request_contents("UTF8_STRING",
                            sigc::mem_fun(this, &DisplayWindow::clipboard_text_received));
        });
        clipboard_connected = true;
    }
}

void Vnc::DisplayWindow::handle_disconnect(const Glib::ustring &connected_msg,
                                           const Glib::ustring &disconnected_msg)
{
    m_signal_connection_lost.emit();
    if (m_connected) {
        int result = Gtk::RESPONSE_CANCEL;
        {
            Gtk::MessageDialog dialog(*this, connected_msg, false,
                                      Gtk::MESSAGE_ERROR, Gtk::BUTTONS_NONE);
            dialog.add_button("Reconnect", Gtk::RESPONSE_YES);
            dialog.add_button("Close", Gtk::RESPONSE_CANCEL);
            dialog.set_default_response(Gtk::RESPONSE_YES);
            result = dialog.run();
        }
        delete m_vnc;
        m_vnc = nullptr;
        m_connected = false;

        if (result == Gtk::RESPONSE_YES)
            m_signal_reconnect.emit();
        else
            Gtk::Main::quit();
    } else {
        Gtk::MessageDialog dialog(*this, disconnected_msg, false,
                                  Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        Gtk::Main::quit();
    }
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
        title = Glib::ustring::compose("(Press %1 to release pointer) %2 - gsshvnc",
                                       seqstr, name);
    } else {
        title = Glib::ustring::compose("%1 - gsshvnc", name);
    }

    set_title(title);
}

void Vnc::DisplayWindow::vnc_credential(const std::vector<VncDisplayCredential> &credList)
{
    std::vector<std::pair<Glib::ustring, bool>> data;
    data.resize(credList.size(), {Glib::ustring(), false});

    // The user may want to change the username (and therefore probably also
    // the password) they used to log in last, and the only way to re-try a
    // password currently is to re-establish the connection.  Therefore,
    // we always show the connection dialog, but pre-populated it with the
    // saved credentials instead of trying the saved ones first with no prompt.
    auto creds = CredentialStorage::fetch_vnc_user_password(m_ssh_host, m_vnc_host);

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
        grid->set_row_spacing(10);
        grid->set_column_spacing(5);
        grid->set_border_width(5);
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
                entry[row]->set_text(creds.first);
                break;
            case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                label[row] = Gtk::manage(new Gtk::Label("Password:"));
                entry[row]->set_visibility(false);
                entry[row]->set_text(creds.second);
                entry[row]->set_activates_default(true);
                break;
            default:
                continue;
            }
            label[row]->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

            grid->attach(*label[i], 0, row, 1, 1);
            grid->attach(*entry[i], 1, row, 1, 1);
            row++;
        }

        Gtk::CheckButton *remember = Gtk::manage(new Gtk::CheckButton("_Remember password", true));
        if (!creds.second.empty())
            remember->set_active(true);
        grid->attach(*remember, 0, row, 2, 1);

        auto vbox = dialog->get_child();
        dynamic_cast<Gtk::Container *>(vbox)->add(*grid);

        dialog->show_all();
        int response = dialog->run();
        dialog->hide();

        if (response == Gtk::RESPONSE_OK) {
            int row = 0;
            Glib::ustring user, password;
            for (size_t i = 0; i < credList.size(); ++i) {
                switch (credList[i]) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                    user = entry[row]->get_text();
                    data[i] = {user, true};
                    break;
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                    password = entry[row]->get_text();
                    data[i] = {password, true};
                    break;
                default:
                    continue;
                }
                row++;
            }
            if (remember->get_active()) {
                CredentialStorage::remember_vnc_password(m_ssh_host, m_vnc_host,
                                                         user, password);
            } else {
                CredentialStorage::forget_vnc_password(m_ssh_host, m_vnc_host);
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

    /* This allows menu bar shortcuts like Alt+F == File */
    settings->property_gtk_enable_mnemonics().set_value(m_enable_mnemonics);

    m_accel_enabled = true;
}

void Vnc::DisplayWindow::toggle_menubar()
{
    auto size_alloc = get_allocation();
    if (m_hide_menubar->get_active()) {
        resize(size_alloc.get_width(), size_alloc.get_height() - m_menubar->get_height());
        m_menubar->hide();
    } else {
        m_menubar->show();
        resize(size_alloc.get_width(), size_alloc.get_height() + m_menubar->get_height());
    }
}

void Vnc::DisplayWindow::update_scrolling()
{
    bool enable_scrolling = !m_scaling->get_active();
    set_force_size(enable_scrolling);
    if (enable_scrolling)
        m_vnc->set_size_request(m_remote_size.width, m_remote_size.height);
    else
        m_vnc->set_size_request(-1, -1);
}

void Vnc::DisplayWindow::clipboard_text_received(const Gtk::SelectionData &selection_data)
{
    auto clipboard = Gtk::Clipboard::get();
    auto clip_owner = clipboard->get_owner();
    if (clip_owner && clip_owner->gobj() == G_OBJECT(gobj()))
        return;

    /* Based on gtk_clipboard_request_text */
    auto target = selection_data.get_target();
    auto text = selection_data.get_data_as_string();

    if (text.empty()) {
        if (target == "UTF8_STRING") {
            clipboard->request_contents("COMPOUND_TEXT",
                            sigc::mem_fun(this, &DisplayWindow::clipboard_text_received));
            return;
        } else if (target == "COMPOUND_TEXT") {
            clipboard->request_contents("STRING",
                            sigc::mem_fun(this, &DisplayWindow::clipboard_text_received));
            return;
        } else if (target == "STRING") {
            clipboard->request_contents("text/plain;charset=utf-8",
                            sigc::mem_fun(this, &DisplayWindow::clipboard_text_received));
            return;
        }
    }

    try {
        text = Glib::convert_with_fallback(text, "iso8859-1//TRANSLIT", "utf-8");
    } catch (Glib::ConvertError &err) {
        std::cerr << "Warning: " << err.what() << std::endl;
        return;
    }
    client_cut_text(text);
}

void Vnc::DisplayWindow::remote_clipboard_text(const std::string &text)
{
    if (text.empty())
        return;

    const GtkTargetEntry targets[] = {
        {const_cast<gchar *>("UTF8_STRING"), 0, 0},
        {const_cast<gchar *>("COMPOUND_TEXT"), 0, 0},
        {const_cast<gchar *>("TEXT"), 0, 0},
        {const_cast<gchar *>("STRING"), 0, 0},
    };

    try {
        m_clipboard_text = Glib::convert_with_fallback(text, "utf-8", "iso8859-1");
    } catch (Glib::ConvertError &err) {
        m_clipboard_text = std::string();
    }

    if (!m_clipboard_text.empty()) {
        auto clipboard = Gtk::Clipboard::get();

        // This isn't wrapped by gtkmm for some reason
        gtk_clipboard_set_with_owner(clipboard->gobj(),
                                     targets, G_N_ELEMENTS(targets),
                                     &DisplayWindow::vnc_copy_handler,
                                     nullptr,
                                     G_OBJECT(gobj()));
    }
}

void Vnc::DisplayWindow::vnc_copy_handler(GtkClipboard *clipboard,
                                          GtkSelectionData *data,
                                          guint info, gpointer owner)
{
    (void)clipboard;
    (void)info;

    if (owner == G_OBJECT(s_instance->gobj())) {
        gtk_selection_data_set_text(data, s_instance->m_clipboard_text.c_str(),
                                    s_instance->m_clipboard_text.size());
    }
}
