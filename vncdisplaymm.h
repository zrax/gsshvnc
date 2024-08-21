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

#ifndef _VNCDISPLAYMM_H
#define _VNCDISPLAYMM_H

/* TODO:  Use gmmproc to build a standard glibmm wrapper for VncDisplay,
 * instead of these "fake" wrappers */

#include <gtkmm/applicationwindow.h>
#include <vncdisplay.h>

#ifdef GTK_VNC_HAVE_VNCVERSION
    // Introduced in v1.2.0
#   include <vncversion.h>
#else
#   define VNC_CHECK_VERSION(major, minor, micro)  (0)
#endif

#include "vncgrabsequencemm.h"

namespace Gio
{

class SocketAddress;

}

namespace Gtk
{

class ScrolledWindow;
class MenuBar;
class CheckMenuItem;
class RadioMenuItem;

}

namespace Vnc
{

class DisplayWindow : public Gtk::ApplicationWindow
{
public:
    DisplayWindow();
    ~DisplayWindow() override;

    bool open_fd(int fd);
    bool open_fd(int fd, const Glib::ustring &hostname);
    bool open_addr(Gio::SocketAddress *address, const Glib::ustring &hostname);
    bool open_host(const Glib::ustring &host, const Glib::ustring &port);
    bool is_open();
    void close_vnc();

    //VncConnection *get_connection();

    // For use in credential storage.
    // NOTE: vnc_host might not match the one passed to open_host, since the
    // latter might be a local socket through an SSH tunnel.
    void set_ssh_host(const Glib::ustring &ssh_host) { m_ssh_host = ssh_host; }
    void set_vnc_host(const Glib::ustring &vnc_host) { m_vnc_host = vnc_host; }

    void send_keys(const std::vector<guint> &keys);
    void send_keys(const std::vector<guint> &keys, VncDisplayKeyEvent kind);

    void send_pointer(gint x, gint y, int buttonmask);
    void set_grab_keys(Vnc::GrabSequence &seq);
    Vnc::GrabSequence get_grab_keys();

    bool set_credential(int type, const Glib::ustring &data);

    void set_pointer_local(bool enable=true);
    bool get_pointer_local();

    void set_pointer_grab(bool enable=true);
    bool get_pointer_grab();

    void set_keyboard_grab(bool enable=true);
    bool get_keyboard_grab();

    void set_read_only(bool enable=true);
    bool get_read_only();

    Glib::RefPtr<Gdk::Pixbuf> get_pixbuf();

    int get_width();
    int get_height();
    Glib::ustring get_name();

    void client_cut_text(const std::string &text);

    void set_lossy_encoding(bool enable=true);
    bool get_lossy_encoding();

    void set_scaling(bool enable=true);
    bool get_scaling();

    void set_force_size(bool enable=true);
    bool get_force_size();

    /* gtk-vnc >= 1.2.0 */
    void set_allow_resize(bool enable=true);
    bool get_allow_resize();

    /* Not supported on gtk-vnc < 0.7.0 */
    void set_smoothing(bool enable=true);
    bool get_smoothing();

    /* gtk-vnc >= 1.2.0 */
    void set_keep_aspect_ratio(bool enable=true);
    bool get_keep_aspect_ratio();

    void set_shared_flag(bool enable=true);
    bool get_shared_flag();

    void set_depth(VncDisplayDepthColor depth);
    VncDisplayDepthColor get_depth();

    void force_grab(bool enable=true);

    bool is_pointer_absolute();

    static Glib::OptionGroup &option_group();

    sigc::signal<void> &signal_connection_lost() { return m_signal_connection_lost; }
    sigc::signal<void> &signal_want_reconnect() { return m_signal_reconnect; }

    void set_capture_keyboard(bool enable=true);
    bool get_capture_keyboard();

    void activate_menubar();

private:
    Glib::SignalProxy0<void> signal_vnc_connected();
    Glib::SignalProxy0<void> signal_vnc_initialized();
    Glib::SignalProxy0<void> signal_vnc_disconnected();
    /* NOTE: signal_vnc_error not emitted on gtk-vnc < 0.6.0 */
    Glib::SignalProxy1<void, const Glib::ustring &> signal_vnc_error();
    Glib::SignalProxy1<void, const std::vector<VncDisplayCredential> &>
        signal_vnc_auth_credential();
    Glib::SignalProxy0<void> signal_vnc_pointer_grab();
    Glib::SignalProxy0<void> signal_vnc_pointer_ungrab();
    Glib::SignalProxy0<void> signal_vnc_keyboard_grab();
    Glib::SignalProxy0<void> signal_vnc_keyboard_ungrab();
    Glib::SignalProxy2<void, gint, gint> signal_vnc_desktop_resize();
    Glib::SignalProxy1<void, const Glib::ustring &> signal_vnc_auth_failure();
    Glib::SignalProxy1<void, guint> signal_vnc_auth_unsupported();
    Glib::SignalProxy1<void, const std::string &> signal_vnc_server_cut_text();
    Glib::SignalProxy0<void> signal_vnc_bell();

    // Emitted when VNC disconnects for any reason, to signal that the
    // SSH client should also terminate.  This is emitted before handling
    // the user response and potentially requesting a reconnection.
    sigc::signal<void> m_signal_connection_lost;

    // Emitted after VNC disconnects and the user requests re-connection.
    sigc::signal<void> m_signal_reconnect;

    Gtk::Widget *m_vnc;
    Gtk::ScrolledWindow *m_viewport;
    VncDisplay *get_vnc();
    bool m_connected;

    bool m_accel_enabled;
    bool m_enable_mnemonics;
    Glib::ustring m_menu_bar_accel;

    Gtk::MenuBar *m_menubar;
    Gtk::CheckMenuItem *m_capture_keyboard;
    Gtk::CheckMenuItem *m_hide_menubar;
    Gtk::CheckMenuItem *m_fullscreen;
    Gtk::RadioMenuItem *m_resize_none;
    Gtk::RadioMenuItem *m_resize_scale;
    Gtk::RadioMenuItem *m_resize_remote;
    Gtk::CheckMenuItem *m_smoothing;
    Gtk::CheckMenuItem *m_keep_ratio;

    void *m_pulse_ifc;

    std::string m_clipboard_text;

    Glib::ustring m_vnc_host;
    Glib::ustring m_ssh_host;

    void init_vnc();
    void handle_disconnect(const Glib::ustring &connected_msg,
                           const Glib::ustring &disconnected_msg);

    void vnc_screenshot();
    void vnc_initialized();
    void update_title(bool grabbed);
    void vnc_credential(const std::vector<VncDisplayCredential> &credList);
    void on_set_scaling(bool enable=true);
    void on_set_allow_resize(bool enable=true);
    void on_set_smoothing(bool enable=true);
    void on_set_keep_aspect_ratio(bool enable=true);
    void disable_modifiers();
    void enable_modifiers();
    void toggle_menubar();

    struct { int width, height; } m_remote_size;
    void update_scrolling();

    void clipboard_text_received(const Gtk::SelectionData &selection_data);
    void remote_clipboard_text(const std::string &text);
    static void vnc_copy_handler(GtkClipboard *clipboard, GtkSelectionData *data,
                                 guint info, gpointer owner);
};

}

#endif
