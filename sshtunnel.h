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

#ifndef _SSHTUNNEL_H
#define _SSHTUNNEL_H

#include <glibmm/ustring.h>
#include <giomm/socket.h>
#include <libssh/libssh.h>
#include <thread>
#include <atomic>

namespace Gtk
{

class Window;

};

class SshTunnel
{
public:
    explicit SshTunnel(Gtk::Window &parent);
    ~SshTunnel();

    bool connect(const Glib::ustring &server);
    void disconnect();

    guint16 forward_port(const Glib::ustring &remote_host, int remote_port);

private:
    Gtk::Window &m_parent;
    ssh_session m_ssh;
    Glib::ustring m_hostname;
    Glib::RefPtr<Gio::Socket> m_forward_socket;
    std::thread m_forward_thread;
    Glib::ustring m_remote_host;
    int m_remote_port;
    std::atomic_bool m_eof;

    bool verify_host();
    bool prompt_password();
    void tunnel_server(int ssh_fd);
};

#endif
