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

#include "sshtunnel.h"

#include <glibmm/miscutils.h>
#include <gtkmm/dialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/entry.h>
#include <gtkmm/messagedialog.h>
#include <giomm.h>
#include <list>
#include <iostream>

#define FORWARD_BUFFER_SIZE 4096

SshTunnel::SshTunnel(Gtk::Window &parent)
    : m_parent(parent), m_ssh(ssh_new()), m_forward_channel(),
      m_connected(false), m_eof(false)
{ }

SshTunnel::~SshTunnel()
{
    disconnect();
    ssh_free(m_ssh);
}

bool SshTunnel::connect(const Glib::ustring &server)
{
    m_hostname = server;
    std::string username, port;

    auto upos = m_hostname.find('@');
    if (upos != std::string::npos) {
        username = m_hostname.substr(0, upos);
        m_hostname = m_hostname.substr(upos + 1);
    } else {
        username = Glib::get_user_name();
    }

    auto ppos = m_hostname.find(':');
    if (ppos != std::string::npos) {
        port = std::to_string(5900 + std::stoi(m_hostname.substr(ppos + 1)));
        m_hostname.resize(ppos);
    } else {
        port = "22";
    }

    ssh_options_set(m_ssh, SSH_OPTIONS_HOST, m_hostname.c_str());
    ssh_options_set(m_ssh, SSH_OPTIONS_PORT_STR, port.c_str());
    ssh_options_set(m_ssh, SSH_OPTIONS_USER, username.c_str());

    if (ssh_connect(m_ssh) != SSH_OK) {
        auto text = Glib::ustring::compose("Error connecting to %1: %2", m_hostname,
                                           ssh_get_error(m_ssh));
        Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        return false;
    }

    if (!verify_host())
        return false;

    // TODO: Support SSH public keys with a passphrase
    if (ssh_userauth_publickey_auto(m_ssh, nullptr, "") == SSH_AUTH_SUCCESS)
        return true;

    return prompt_password();
}

void SshTunnel::disconnect()
{
    m_eof = true;
    if (m_forward_thread.joinable())
        m_forward_thread.join();
    if (m_connected)
        ssh_disconnect(m_ssh);
}

#define TUNNEL_PORT_OFFSET 5500

static Glib::RefPtr<Gio::Socket> get_local_socket(Glib::RefPtr<Gio::Cancellable> &cancellable)
{
    Glib::RefPtr<Gio::Socket> sock;
    try {
        sock = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4, Gio::SOCKET_TYPE_STREAM,
                                   Gio::SOCKET_PROTOCOL_TCP, cancellable);
    } catch (Gio::Error &err) {
        std::cerr << err.what() << std::endl;
        return {};
    }
    if (!sock)
        return {};

    auto loop_addr = Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4);
    if (!loop_addr)
        return {};

    for (guint16 port = TUNNEL_PORT_OFFSET + 99; port > TUNNEL_PORT_OFFSET; --port) {
        auto addr = Gio::InetSocketAddress::create(loop_addr, port);
        if (!addr)
            return {};

        try {
            sock->bind(addr, false);
            return sock;
        } catch (Gio::Error &) {
            /* Fail to bind -- try next port */
        }
    }

    return {};
}

guint16 SshTunnel::forward_port(const Glib::ustring &remote_host, int remote_port)
{
    Glib::RefPtr<Gio::Cancellable> cancellable;
    auto server_sock = get_local_socket(cancellable);
    if (!server_sock)
        return 0;

    m_forward_channel = ssh_channel_new(m_ssh);
    if (!m_forward_channel)
        return 0;

    auto local_address = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(server_sock->get_local_address());
    if (ssh_channel_open_forward(m_forward_channel, remote_host.c_str(), remote_port,
                                 "127.0.0.1", local_address->get_port()) != SSH_OK) {
        ssh_channel_free(m_forward_channel);
        return 0;
    }

    int ssh_fd = ssh_get_fd(m_ssh);
    if (ssh_fd < 0) {
        auto text = Glib::ustring::compose("Error getting SSH handle: %1", ssh_get_error(m_ssh));
        Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        ssh_channel_free(m_forward_channel);
        return 0;
    }

    server_sock->set_listen_backlog(1);
    server_sock->listen();
    m_forward_thread = std::thread([this, cancellable, server_sock, ssh_fd]() {
        tunnel_proc(server_sock, ssh_fd);
        if (m_forward_channel) {
            ssh_channel_free(m_forward_channel);
            m_forward_channel = nullptr;
        }
    });

    return local_address->get_port();
}

bool SshTunnel::verify_host()
{
    int state = ssh_is_server_known(m_ssh);

    ssh_key server_key;
    if (ssh_get_publickey(m_ssh, &server_key) < 0)
        return false;

    unsigned char *hash_buf = nullptr;
    size_t hash_len;
    if (ssh_get_publickey_hash(server_key, SSH_PUBLICKEY_HASH_SHA1, &hash_buf, &hash_len) < 0)
        return false;
    char *temp = ssh_get_hexa(hash_buf, hash_len);
    Glib::ustring hash_str(temp);
    free(temp);
    free(hash_buf);

    switch (state) {
    case SSH_SERVER_KNOWN_OK:
        break;

    case SSH_SERVER_KNOWN_CHANGED:
        {
            auto text = Glib::ustring::compose(
                            "Host key for %1 has changed\n"
                            "New server key: %2\n\n"
                            "Connect anyway? (<b>NOT RECOMMENDED</b> unless you trust the new key)",
                            m_hostname, hash_str);
            Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO);
            int response = dialog.run();
            if (response == Gtk::RESPONSE_NO)
                return false;
        }
        break;

    case SSH_SERVER_FOUND_OTHER:
        {
            auto text = Glib::ustring::compose(
                            "The host key for %1 was not found, but another type of key exists.\n"
                            "Connect anyway? (<b>NOT RECOMMENDED</b>)",
                            m_hostname);
            Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO);
            int response = dialog.run();
            if (response == Gtk::RESPONSE_NO)
                return false;
        }
        break;

    case SSH_SERVER_FILE_NOT_FOUND:
    case SSH_SERVER_NOT_KNOWN:
        {
            auto text = Glib::ustring::compose(
                            "The host key for %1 is not known.\n"
                            "Public Key hash: %2\n\n"
                            "Do you trust the host key?",
                            m_hostname, hash_str);
            Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO);
            int response = dialog.run();
            if (response == Gtk::RESPONSE_NO)
                return false;
        }
        break;

    case SSH_SERVER_ERROR:
        {
            auto text = Glib::ustring::compose("Error connecting to %1: %2", m_hostname,
                                               ssh_get_error(m_ssh));
            Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
            (void)dialog.run();
        }
        return false;

    default:
        {
            auto text = Glib::ustring::compose("Unsupported libssh response: %1", state);
            Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
            (void)dialog.run();
        }
        return false;
    }

    return true;
}

bool SshTunnel::prompt_password()
{
    Gtk::Dialog dialog("SSH Authentication", m_parent);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Ok", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);

    auto *grid = Gtk::manage(new Gtk::Grid);
    grid->set_row_spacing(3);
    grid->set_column_spacing(3);
    grid->set_border_width(3);
    Gtk::Label *label = Gtk::manage(new Gtk::Label("Password:"));
    Gtk::Entry *password = Gtk::manage(new Gtk::Entry);
    password->set_activates_default(true);
    password->set_visibility(false);

    grid->attach(*label, 0, 0, 1, 1);
    grid->attach(*password, 1, 0, 1, 1);

    auto vbox = dialog.get_child();
    dynamic_cast<Gtk::Container *>(vbox)->add(*grid);

    dialog.show_all();
    int response = dialog.run();
    dialog.hide();

    if (response != Gtk::RESPONSE_OK)
        return false;

    int result = ssh_userauth_password(m_ssh, nullptr, password->get_text().c_str());
    if (result != SSH_AUTH_SUCCESS) {
        auto text = Glib::ustring::compose("Error connecting to %1: %2", m_hostname,
                                           ssh_get_error(m_ssh));
        Gtk::MessageDialog err_dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)err_dialog.run();
        return false;
    }

    return true;
}

void SshTunnel::tunnel_proc(Glib::RefPtr<Gio::Socket> server_sock, int ssh_fd)
{
    ssh_channel r_channels[] = { m_forward_channel, nullptr };
    ssh_channel w_channels[] = { nullptr,           nullptr };
    Glib::RefPtr<Gio::Socket> client_sock;
    fd_set rfds;
    struct timeval timeout;
    int server_fd = server_sock->get_fd();
    char buffer[FORWARD_BUFFER_SIZE];

    for ( ;; ) {
        FD_ZERO(&rfds);
        FD_SET(ssh_fd, &rfds);
        if (server_fd)
            FD_SET(server_fd, &rfds);
        int maxfd = std::max(ssh_fd, server_fd) + 1;
        if (client_sock) {
            int fd = client_sock->get_fd();
            FD_SET(fd, &rfds);
            maxfd = std::max(maxfd, fd + 1);
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        int result = ssh_select(r_channels, w_channels, maxfd, &rfds, &timeout);
        if (m_eof)
            return;
        if (result == EINTR || result == SSH_EINTR)
            continue;
        if (FD_ISSET(server_fd, &rfds)) {
            if (client_sock)
                std::cerr << "Got extra request for client socket.  Ignoring" << std::endl;
            else
                client_sock = server_sock->accept();
            continue;
        }
        if (client_sock && FD_ISSET(client_sock->get_fd(), &rfds)) {
            gssize in_size = client_sock->receive(buffer, FORWARD_BUFFER_SIZE);
            if (in_size == 0) {
                ssh_channel_send_eof(m_forward_channel);
                client_sock->close();
                client_sock = {};
            }
            char *bufp = buffer;
            while (in_size) {
                int out_size = ssh_channel_write(m_forward_channel, bufp, in_size);
                if (out_size < 0) {
                    std::cerr << "Error writing to SSH channel: "
                              << ssh_get_error(m_ssh) << std::endl;
                    return;
                }
                in_size -= out_size;
                bufp += out_size;
            }
        }
        if (m_forward_channel && ssh_channel_is_closed(m_forward_channel)) {
            ssh_channel_free(m_forward_channel);
            r_channels[0] = nullptr;
            m_forward_channel = nullptr;
        }
        if (w_channels[0] && client_sock) {
            for (int is_stderr : {0, 1}) {
                while (m_forward_channel && ssh_channel_is_open(m_forward_channel)
                       && ssh_channel_poll(m_forward_channel, is_stderr)) {
                    int in_size = ssh_channel_read(m_forward_channel, buffer,
                                                   FORWARD_BUFFER_SIZE, 0);
                    if (in_size < 0) {
                        std::cerr << "Error reading from SSH channel: "
                                  << ssh_get_error(m_ssh) << std::endl;
                        return;
                    }
                    if (in_size == 0) {
                        ssh_channel_free(m_forward_channel);
                        r_channels[0] = nullptr;
                        m_forward_channel = nullptr;
                    }
                    while (in_size) {
                        gchar *bufp = buffer;
                        gssize out_size = client_sock->send(bufp, in_size);
                        if (out_size < 0) {
                            std::cerr << "Error writing to local port" << std::endl;
                            return;
                        }
                        in_size -= out_size;
                        bufp += out_size;
                    }
                }
            }
        }
        if (m_forward_channel && ssh_channel_is_closed(m_forward_channel)) {
            ssh_channel_free(m_forward_channel);
            m_forward_channel = nullptr;
        }
    }
}
