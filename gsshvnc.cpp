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

#include <glibmm/optioncontext.h>
#include <gtkmm/application.h>
#include <gtkmm/messagedialog.h>
#include <libssh/callbacks.h>
#include <iostream>

#ifdef _WIN32
#include <glibmm/miscutils.h>
#include <cstdio>
#endif

static bool show_connect_dialog(Vnc::DisplayWindow &vnc, SshTunnel &ssh)
{
    Vnc::ConnectDialog dialog(vnc);
    dialog.show_all();
    for ( ;; ) {
        dialog.set_sensitive(true);
        int response = dialog.run();
        dialog.set_sensitive(false);
        if (response != Gtk::RESPONSE_OK)
            break;

        if (dialog.configure(vnc, ssh)) {
            vnc.show_all();
            return true;
        }

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

class GsshvncApp : public Gtk::Application
{
public:
    GsshvncApp()
        : Gtk::Application("net.zrax.gsshvnc",
                           Gio::APPLICATION_HANDLES_COMMAND_LINE | Gio::APPLICATION_NON_UNIQUE)
    { }

    ~GsshvncApp() override
    {
        if (m_ssh)
            m_ssh->disconnect();
        if (m_vnc)
            remove_window(*m_vnc);
    }

protected:
    int on_command_line(const Glib::RefPtr<Gio::ApplicationCommandLine> &command_line) override
    {
        const auto name = Glib::ustring::compose("- Simple SSH/VNC Client on Gtk-VNC %1",
                                                 vnc_util_get_version_string());
        static const char help_msg[] = "Run 'gsshvnc --help' to see a full list of available command line options";

        Glib::OptionContext context(name);
        context.add_group(Vnc::DisplayWindow::option_group());
        Glib::OptionGroup gtk_group(gtk_get_option_group(true));
        context.add_group(gtk_group);
        context.set_help_enabled(true);

        int argc = 0;
        char **argv = command_line->get_arguments(argc);
        try {
            if (!context.parse(argc, argv))
                return 1;
        } catch (Glib::OptionError &err) {
            std::cerr << err.what() << "\n" << help_msg << std::endl;
            return 1;
        }
        activate();
        return 0;
    }

    void on_activate() override
    {
        m_vnc = std::make_unique<Vnc::DisplayWindow>();
        m_ssh = std::make_unique<SshTunnel>(*m_vnc);
        add_window(*m_vnc);

        if (!show_connect_dialog(*m_vnc, *m_ssh)) {
            quit();
            return;
        }

        m_vnc->signal_delete_event().connect([this](GdkEventAny *) -> bool {
            quit();
            return false;
        });
        m_vnc->signal_connection_lost().connect([this]() {
            m_ssh->disconnect();
        });
        m_vnc->signal_want_reconnect().connect([this]() {
            if (!show_connect_dialog(*m_vnc, *m_ssh))
                quit();
        });
    }

private:
    std::unique_ptr<Vnc::DisplayWindow> m_vnc;
    std::unique_ptr<SshTunnel> m_ssh;
};

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

    int rc = GsshvncApp().run(argc, argv);

    ssh_finalize();
    return rc;
}
