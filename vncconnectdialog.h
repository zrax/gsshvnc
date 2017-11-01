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

#include "sshtunnel.h"

#include <gtkmm/dialog.h>

namespace Gtk
{

class CheckButton;
class ComboBoxText;
class Label;
class Switch;

}

namespace Vnc
{

class DisplayWindow;

class ConnectDialog : public Gtk::Dialog
{
public:
    explicit ConnectDialog(Gtk::Window &parent);

    bool configure(Vnc::DisplayWindow &vnc, SshTunnel &tunnel);

private:
    Gtk::ComboBoxText *m_host;
    Gtk::Switch *m_ssh_tunnel;
    Gtk::Label *m_ssh_detail_labels[2];
    Gtk::ComboBoxText *m_ssh_host;
    Gtk::ComboBoxText *m_ssh_user;
    Gtk::CheckButton *m_lossy_compression;
    Gtk::ComboBoxText *m_color_depth;

    static gboolean ssh_switch_activate(GtkSwitch *, gboolean, gpointer);
};

}

#endif
