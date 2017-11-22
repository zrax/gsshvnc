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

#include "vncconnectdialog.h"
#include "vncdisplaymm.h"
#include "appsettings.h"

#include <glibmm/miscutils.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/switch.h>

Vnc::ConnectDialog::ConnectDialog(Gtk::Window &parent)
    : Gtk::Dialog("Connect", parent, Gtk::DIALOG_MODAL | Gtk::DIALOG_DESTROY_WITH_PARENT)
{
    AppSettings settings;

    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("Co_nnect", Gtk::RESPONSE_OK);
    set_default_response(Gtk::RESPONSE_OK);

    auto box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
    box->set_border_width(8);

    auto label = Gtk::manage(new Gtk::Label);
    label->set_markup("<b>VNC Options</b>");
    label->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    box->add(*label);

    auto linebox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    linebox->set_margin_left(15);
    label = Gtk::manage(new Gtk::Label("VNC _Server:", true));
    m_host = Gtk::manage(new Gtk::ComboBoxText(true));
    m_host->get_entry()->set_placeholder_text("hostname[:display]");
    m_host->get_entry()->set_activates_default(true);
    label->set_mnemonic_widget(*m_host);

    auto recent_hosts = settings.get_recent_hosts();
    for (const auto &host : recent_hosts)
        m_host->append(host);
    if (!recent_hosts.empty())
        m_host->set_active_text(recent_hosts.front());

    linebox->pack_start(*label, Gtk::PACK_SHRINK);
    linebox->pack_start(*m_host);
    box->add(*linebox);

    linebox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    linebox->set_margin_left(15);
    label = Gtk::manage(new Gtk::Label("Color _Depth:", true));
    label->set_alignment(Gtk::ALIGN_FILL, Gtk::ALIGN_CENTER);
    m_color_depth = Gtk::manage(new Gtk::ComboBoxText);
    m_color_depth->append(std::to_string(VNC_DISPLAY_DEPTH_COLOR_DEFAULT), "Use Server's Setting");
    m_color_depth->append(std::to_string(VNC_DISPLAY_DEPTH_COLOR_FULL), "True Color (24 bits)");
    m_color_depth->append(std::to_string(VNC_DISPLAY_DEPTH_COLOR_MEDIUM), "High Color (16 bits)");
    m_color_depth->append(std::to_string(VNC_DISPLAY_DEPTH_COLOR_LOW), "Low Color (8 bits)");
    m_color_depth->append(std::to_string(VNC_DISPLAY_DEPTH_COLOR_ULTRA_LOW), "Ultra Low Color (3 bits)");
    if (!m_color_depth->set_active_id(settings.get_color_depth()))
        m_color_depth->set_active(0);
    label->set_mnemonic_widget(*m_color_depth);

    linebox->pack_start(*label, Gtk::PACK_SHRINK);
    linebox->pack_start(*m_color_depth, Gtk::PACK_SHRINK);
    box->add(*linebox);

    m_lossy_compression = Gtk::manage(new Gtk::CheckButton("Use Lossy (_JPEG) Compression", true));
    m_lossy_compression->set_margin_left(15);
    m_lossy_compression->set_active(settings.get_lossy_compression());

    box->add(*m_lossy_compression);

    linebox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    linebox->set_margin_top(20);
    label = Gtk::manage(new Gtk::Label);
    label->set_markup_with_mnemonic("<b>SSH _Tunnel</b>");
    m_ssh_tunnel = Gtk::manage(new Gtk::Switch);
    m_ssh_tunnel->set_active(true);
    label->set_mnemonic_widget(*m_ssh_tunnel);

    linebox->pack_start(*label, Gtk::PACK_SHRINK);
    linebox->pack_end(*m_ssh_tunnel, Gtk::PACK_SHRINK);
    box->add(*linebox);

    linebox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    linebox->set_margin_left(15);
    m_ssh_detail_labels[0] = Gtk::manage(new Gtk::Label("_Host:", true));
    m_ssh_host = Gtk::manage(new Gtk::ComboBoxText(true));
    m_ssh_host->get_entry()->set_placeholder_text("hostname[:port]");
    m_ssh_host->get_entry()->set_activates_default(true);
    m_ssh_detail_labels[0]->set_mnemonic_widget(*m_ssh_host);

    auto recent_ssh_hosts = settings.get_recent_ssh_hosts();
    for (const auto &host : recent_ssh_hosts)
        m_ssh_host->append(host);
    if (!recent_ssh_hosts.empty())
        m_ssh_host->set_active_text(recent_ssh_hosts.front());

    linebox->pack_start(*m_ssh_detail_labels[0], Gtk::PACK_SHRINK);
    linebox->pack_start(*m_ssh_host);
    box->add(*linebox);

    linebox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
    linebox->set_margin_left(15);
    m_ssh_detail_labels[1] = Gtk::manage(new Gtk::Label("_User:", true));
    m_ssh_user = Gtk::manage(new Gtk::ComboBoxText(true));
    m_ssh_user->get_entry()->set_placeholder_text(Glib::get_user_name());
    m_ssh_user->get_entry()->set_activates_default(true);
    m_ssh_detail_labels[1]->set_mnemonic_widget(*m_ssh_user);

    auto recent_ssh_users = settings.get_recent_ssh_users();
    for (const auto &host : recent_ssh_users)
        m_ssh_user->append(host);
    if (!recent_ssh_users.empty())
        m_ssh_user->set_active_text(recent_ssh_users.front());

    linebox->pack_start(*m_ssh_detail_labels[1], Gtk::PACK_SHRINK);
    linebox->pack_start(*m_ssh_user);
    box->add(*linebox);

    auto vbox = get_child();
    dynamic_cast<Gtk::Container *>(vbox)->add(*box);

    // Gtkmm's signal_state_set is broken and will never fire :(
    g_signal_connect(m_ssh_tunnel->gobj(), "state-set",
                     G_CALLBACK(&Vnc::ConnectDialog::ssh_switch_activate), this);
    m_ssh_tunnel->set_active(settings.get_enable_tunnel());
}

bool Vnc::ConnectDialog::configure(Vnc::DisplayWindow &vnc, SshTunnel &tunnel)
{
    vnc.set_shared_flag(true);
    vnc.set_depth((VncDisplayDepthColor)std::stoi(m_color_depth->get_active_id()));
    vnc.set_lossy_encoding(m_lossy_compression->get_active());

    Glib::ustring hostname = m_host->get_active_text();
    Glib::ustring port;

    auto ppos = hostname.find(':');
    if (ppos != Glib::ustring::npos) {
        int port_num = std::stoi(hostname.substr(ppos + 1));
        if (port_num > 999)
            port = std::to_string(port_num);
        else
            port = std::to_string(5900 + port_num);
        hostname.resize(ppos);
    } else {
        port = "5900";
    }

    if (hostname.empty())
        hostname = "127.0.0.1";

    if (m_ssh_tunnel->get_active()) {
        auto ssh_string = m_ssh_host->get_active_text();
        auto username = m_ssh_user->get_active_text();
        if (username.empty())
            username = Glib::get_user_name();
        tunnel.disconnect();
        if (!tunnel.connect(ssh_string, username))
            return false;
        guint16 local_port = tunnel.forward_port(hostname, std::stoi(port));
        if (local_port == 0)
            return false;
        hostname = "127.0.0.1";
        port = std::to_string(local_port);
    }

    if (!vnc.open_host(hostname, port))
        return false;

    vnc.set_pointer_grab(true);
    vnc.set_pointer_local(true);

    AppSettings settings;
    vnc.set_capture_keyboard(settings.get_capture_keyboard());
    vnc.set_scaling(settings.get_scaled_display());
    vnc.set_smoothing(settings.get_smooth_scaling());

    // Save settings if the configuration was successful
    auto form_text = m_host->get_active_text();
    if (!form_text.empty())
        settings.add_recent_host(form_text);
    form_text = m_ssh_host->get_active_text();
    if (!form_text.empty())
        settings.add_recent_ssh_host(form_text);
    form_text = m_ssh_user->get_active_text();
    if (!form_text.empty())
        settings.add_recent_ssh_user(form_text);
    settings.set_enable_tunnel(m_ssh_tunnel->get_active());
    settings.set_lossy_compression(m_lossy_compression->get_active());
    settings.set_color_depth(m_color_depth->get_active_id());

    return true;
}

gboolean Vnc::ConnectDialog::ssh_switch_activate(GtkSwitch *, gboolean active,
                                                 gpointer user_data)
{
    auto self = reinterpret_cast<Vnc::ConnectDialog *>(user_data);

    self->m_ssh_detail_labels[0]->set_sensitive(active);
    self->m_ssh_host->set_sensitive(active);
    self->m_ssh_host->get_entry()->set_sensitive(active);
    self->m_ssh_detail_labels[1]->set_sensitive(active);
    self->m_ssh_user->set_sensitive(active);
    self->m_ssh_user->get_entry()->set_sensitive(active);
    return false;
}
