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

#include <gtkmm/main.h>
#include <iostream>

struct _GVncViewerOptions : public Glib::OptionGroup
{
    Glib::OptionGroup::vecustrings m_args;

    _GVncViewerOptions()
        : Glib::OptionGroup("Main", "Command line options")
    {
        Glib::OptionEntry remaining;
        remaining.set_long_name(G_OPTION_REMAINING);
        remaining.set_arg_description("[hostname][:display]");
        add_entry(remaining, m_args);
    }
};

int main(int argc, char *argv[])
{
    auto name = Glib::ustring::compose("- Simple SSH/VNC Client on Gtk-VNC %1",
                                       vnc_util_get_version_string());
    static const char help_msg[] = "Run 'gsshvnc --help' to see a full list of available command line options";

    /* Setup command line options */
    Glib::OptionContext context(name);
    _GVncViewerOptions mainOptions;
    context.set_main_group(mainOptions);
    context.add_group(Vnc::DisplayWindow::option_group());

    std::unique_ptr<Gtk::Main> appMain;
    try {
        appMain = std::make_unique<Gtk::Main>(argc, argv, context);
    } catch (Glib::OptionError &err) {
        std::cerr << err.what() << "\n"
                  << help_msg << std::endl;
        return 1;
    }
    if (mainOptions.m_args.size() != 1) {
        std::cerr << "Usage: gsshvnc [hostname][:display]\n"
                  << help_msg << std::endl;
        return 1;
    }

    Vnc::DisplayWindow vnc;
    vnc.set_shared_flag(true);
    vnc.set_depth(VNC_DISPLAY_DEPTH_COLOR_FULL);
    vnc.set_lossy_encoding(true);

    Glib::ustring hostname;
    Glib::ustring port;

    if (mainOptions.m_args[0] == "")
        hostname = "127.0.0.1";
    else
        hostname = mainOptions.m_args[0];

    auto ppos = hostname.find(':');
    if (ppos != Glib::ustring::npos) {
        port = std::to_string(5900 + std::stoi(hostname.substr(ppos + 1)));
        hostname.resize(ppos);
    } else {
        port = "5900";
    }
    vnc.open_host(hostname, port);
    vnc.set_keyboard_grab(true);
    vnc.set_pointer_grab(true);
    vnc.set_pointer_local(true);

    if (!vnc.is_composited())
        vnc.set_scaling(true);
    vnc.set_grab_keyboard(true);

    vnc.signal_delete_event().connect([](GdkEventAny *) -> bool {
        Gtk::Main::quit();
        return false;
    });

    Gtk::Main::run();

    return 0;
}
