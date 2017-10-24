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

#ifndef _VNCCONNECTDIALOG_H
#define _VNCCONNECTDIALOG_H

#include <gtkmm/dialog.h>

namespace Gtk
{

class Entry;
class CheckButton;
class ComboBoxText;

}

namespace Vnc
{

class DisplayWindow;

class ConnectDialog : public Gtk::Dialog
{
public:
    explicit ConnectDialog(Gtk::Window &parent);

    bool configure(Vnc::DisplayWindow &vnc);

private:
    Gtk::Entry *m_host;
    Gtk::Entry *m_ssh_host;
    Gtk::CheckButton *m_lossy_compression;
    Gtk::ComboBoxText *m_color_depth;
};

}

#endif
