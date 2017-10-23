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

#include <gtkmm/window.h>
#include <gtkmm/menubar.h>
#include <gtkmm/menu.h>
#include <gtkmm/checkmenuitem.h>
#include <giomm/socketaddress.h>
#include <vncdisplay.h>

#include "vncgrabsequencemm.h"

namespace Vnc
{

class DisplayWindow : public Gtk::Window
{
public:
    DisplayWindow();

    bool open_fd(int fd);
    bool open_fd(int fd, const Glib::ustring &hostname);
    bool open_addr(Gio::SocketAddress *address, const Glib::ustring &hostname);
    bool open_host(const Glib::ustring &host, const Glib::ustring &port);
    bool is_open();
    void close_vnc();

    //VncConnection *get_connection();

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

    void client_cut_text(const Glib::ustring &text);

    void set_lossy_encoding(bool enable=true);
    bool get_lossy_encoding();

    bool set_scaling(bool enable=true);
    bool get_scaling();

    void set_force_size(bool enable=true);
    bool get_force_size();

    void set_smoothing(bool enable=true);
    bool get_smoothing();

    void set_shared_flag(bool enable=true);
    bool get_shared_flag();

    void set_depth(VncDisplayDepthColor depth);
    VncDisplayDepthColor get_depth();

    void force_grab(bool enable=true);

    bool is_pointer_absolute();

    static Glib::OptionGroup &option_group();
    //static std::vector<Glib::OptionEntry> option_entries();

    Glib::SignalProxy<void> signal_vnc_connected();
    Glib::SignalProxy<void> signal_vnc_initialized();
    Glib::SignalProxy<void> signal_vnc_disconnected();
    Glib::SignalProxy<void, const Glib::ustring &> signal_vnc_error();
    Glib::SignalProxy<void, const std::vector<VncDisplayCredential> &>
        signal_vnc_auth_credential();
    Glib::SignalProxy<void> signal_vnc_pointer_grab();
    Glib::SignalProxy<void> signal_vnc_pointer_ungrab();
    Glib::SignalProxy<void> signal_vnc_keyboard_grab();
    Glib::SignalProxy<void> signal_vnc_keyboard_ungrab();
    Glib::SignalProxy<void, gint, gint> signal_vnc_desktop_resize();
    Glib::SignalProxy<void, const Glib::ustring &> signal_vnc_auth_failure();
    Glib::SignalProxy<void, guint> signal_vnc_auth_unsupported();
    Glib::SignalProxy<void, const Glib::ustring &> signal_vnc_server_cut_text();
    Glib::SignalProxy<void> signal_vnc_bell();

private:
    Gtk::Widget *m_vnc;
    VncDisplay *get_vnc();
    bool m_connected;

    bool m_accel_enabled;
    bool m_enable_mnemonics;
    Glib::ustring m_menu_bar_accel;
    std::vector<Glib::RefPtr<Gtk::AccelGroup>> m_accel_groups;

    Gtk::CheckMenuItem *m_fullscreen;
    Gtk::CheckMenuItem *m_scaling;
    Gtk::CheckMenuItem *m_smoothing;

    bool vnc_screenshot(GdkEventKey *ev);
    void vnc_initialized();
    void update_title(bool grabbed);
    void vnc_credential(const std::vector<VncDisplayCredential> &credList);
    bool on_set_scaling(bool enable=true);
    void on_set_smoothing(bool enable=true);
    void disable_modifiers();
    void enable_modifiers();
    void on_set_grab_keys();
};

}

#endif
