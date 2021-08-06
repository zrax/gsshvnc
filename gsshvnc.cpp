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
#include "vncconnectdialog.h"

#include <glibmm/miscutils.h>
#include <gtkmm/main.h>
#include <gtkmm/messagedialog.h>
#include <libssh/callbacks.h>
#include <fcntl.h>
#include <iostream>

static bool show_connect_dialog(Vnc::DisplayWindow &vnc, SshTunnel &ssh)
{
    Vnc::ConnectDialog dialog(vnc);
    for ( ;; ) {
        dialog.show_all();
        int response = dialog.run();
        if (response != Gtk::RESPONSE_OK)
            break;

        if (dialog.configure(vnc, ssh)) {
            vnc.show_all();
            return true;
        }

        dialog.hide();
        Gtk::MessageDialog msg_dialog(vnc, "Failed to connect to VNC server",
                                      false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_NONE);
        msg_dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        msg_dialog.add_button("_Retry", Gtk::RESPONSE_YES);
        msg_dialog.set_default_response(Gtk::RESPONSE_YES);
        response = msg_dialog.run();
        if (response != Gtk::RESPONSE_YES)
            break;
    }

    return false;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    /* Redirect console output to a file on Windows, since it would otherwise
     * be hidden from the user */
    auto log_dir = Glib::build_filename(Glib::get_user_config_dir(), "gsshvnc");
    g_mkdir_with_parents(log_dir.c_str(), 0700);

    freopen(Glib::build_filename(log_dir, "debug.out").c_str(), "w", stdout);
    freopen(Glib::build_filename(log_dir, "debug.err").c_str(), "w", stderr);
#endif

    // Tell libssh that we intend to use pthread-based threading
    ssh_threads_set_callbacks(ssh_threads_get_pthread());
    ssh_init();

    auto name = Glib::ustring::compose("- Simple SSH/VNC Client on Gtk-VNC %1",
                                       vnc_util_get_version_string());
    static const char help_msg[] = "Run 'gsshvnc --help' to see a full list of available command line options";

    /* Setup command line options */
    Glib::OptionContext context(name);
    context.add_group(Vnc::DisplayWindow::option_group());

    std::unique_ptr<Gtk::Main> appMain;
    try {
        appMain = std::make_unique<Gtk::Main>(argc, argv, context);
    } catch (Glib::OptionError &err) {
        std::cerr << err.what() << "\n" << help_msg << std::endl;
        return 1;
    }

    Vnc::DisplayWindow vnc;
    SshTunnel ssh(vnc);

    if (!show_connect_dialog(vnc, ssh))
        return 0;

    vnc.signal_delete_event().connect([](GdkEventAny *) -> bool {
        Gtk::Main::quit();
        return false;
    });
    vnc.signal_connection_lost().connect([&ssh]() {
        ssh.disconnect();
    });
    vnc.signal_want_reconnect().connect([&vnc, &ssh]() {
        vnc.hide();
        if (!show_connect_dialog(vnc, ssh))
            Gtk::Main::quit();
    });

    Gtk::Main::run();

    ssh.disconnect();
    ssh_finalize();

    return 0;
}
