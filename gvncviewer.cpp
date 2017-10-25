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

#include <gtkmm/main.h>
#include <gtkmm/messagedialog.h>
#include <libssh/callbacks.h>
#include <iostream>

static bool show_connect_dialog(Vnc::DisplayWindow &vnc, SshTunnel &ssh)
{
    Vnc::ConnectDialog dialog(vnc);
    dialog.show_all();
    for (int retries = 3; retries; --retries) {
        int response = dialog.run();
        if (response != Gtk::RESPONSE_OK)
            return true;

        if (dialog.configure(vnc, ssh))
            break;

        Gtk::MessageDialog msg_dialog(vnc, "Failed to connect to VNC server",
                                      false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_NONE);
        if (retries > 1) {
            msg_dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
            msg_dialog.add_button("_Retry", Gtk::RESPONSE_REJECT);
            response = msg_dialog.run();
            if (response == Gtk::RESPONSE_CANCEL)
                return false;
        } else {
            msg_dialog.add_button("_Ok", Gtk::RESPONSE_OK);
            (void)msg_dialog.run();
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
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
        return 1;

    vnc.signal_delete_event().connect([](GdkEventAny *) -> bool {
        Gtk::Main::quit();
        return false;
    });

    Gtk::Main::run();

    ssh.disconnect();
    ssh_finalize();

    return 0;
}
