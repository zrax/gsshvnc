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
#include "appsettings.h"
#include "credstorage.h"

#include <gtkmm/dialog.h>
#include <gtkmm/grid.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/messagedialog.h>
#include <giomm.h>
#include <list>
#include <iostream>

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 7, 90)
// This was renamed in libssh 0.8.0
#define ssh_get_server_publickey ssh_get_publickey
#endif

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
// Known hosts handling was reworked in 0.8.0
#define ssh_session_update_known_hosts ssh_write_knownhost
#endif

#define FORWARD_BUFFER_SIZE 4096

SshTunnel::SshTunnel(Gtk::Window &parent)
    : m_parent(parent), m_ssh(), m_remote_port(), m_eof(false)
{ }

SshTunnel::~SshTunnel()
{
    disconnect();
}

bool SshTunnel::connect(const Glib::ustring &server, const Glib::ustring &username)
{
    disconnect();
    m_ssh = ssh_new();

    m_eof = false;
    m_hostname = server;
    std::string hostname_no_port = m_hostname;
    std::string port;

    auto ppos = hostname_no_port.find(':');
    if (ppos != std::string::npos) {
        port = std::to_string(std::stoi(hostname_no_port.substr(ppos + 1)));
        hostname_no_port.resize(ppos);
    } else {
        port = "22";
    }

    // Used for display and for storing credentials
    m_server_desc = Glib::ustring::compose("%1@%2", username, server);

    ssh_options_set(m_ssh, SSH_OPTIONS_HOST, hostname_no_port.c_str());
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

    if (ssh_userauth_none(m_ssh, nullptr) == SSH_AUTH_SUCCESS)
        return true;
    auto auth_methods = ssh_userauth_list(m_ssh, nullptr);
    if (auth_methods == 0) {
        // Server MAY reply to ssh_userauth_list; if it doesn't, try anyway
        auth_methods = (SSH_AUTH_METHOD_PUBLICKEY
                        | SSH_AUTH_METHOD_PASSWORD
                        | SSH_AUTH_METHOD_INTERACTIVE);
    }

    // TODO: Support SSH public keys with a passphrase
    if ((auth_methods & SSH_AUTH_METHOD_PUBLICKEY)
            && (ssh_userauth_publickey_auto(m_ssh, nullptr, "") == SSH_AUTH_SUCCESS))
        return true;

    if ((auth_methods & SSH_AUTH_METHOD_PASSWORD) && prompt_password())
        return true;

    if ((auth_methods & SSH_AUTH_METHOD_INTERACTIVE) && interactive())
        return true;

    return false;
}

void SshTunnel::disconnect()
{
    if (m_ssh) {
        m_eof = true;
        if (m_forward_thread.joinable())
            m_forward_thread.join();
        if (ssh_is_connected(m_ssh))
            ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
    }
    m_ssh = nullptr;
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
    m_forward_socket = get_local_socket(cancellable);
    if (!m_forward_socket)
        return 0;

    int ssh_fd = ssh_get_fd(m_ssh);
    if (ssh_fd < 0) {
        auto text = Glib::ustring::compose("Error getting SSH handle: %1", ssh_get_error(m_ssh));
        Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        return 0;
    }

    m_remote_host = remote_host;
    m_remote_port = remote_port;

    m_forward_socket->set_listen_backlog(5);
    try {
        m_forward_socket->listen();
    } catch (Gio::Error &err) {
        auto text = Glib::ustring::compose("Error listening on SSH forward port: %1", err.what());
        Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)dialog.run();
        return 0;
    }
    m_forward_thread = std::thread([this, cancellable, ssh_fd]() {
        tunnel_server(ssh_fd);
    });

    auto local_address = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(m_forward_socket->get_local_address());
    return local_address->get_port();
}

bool SshTunnel::verify_host()
{
#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    int state = ssh_is_server_known(m_ssh);
#else
    ssh_known_hosts_e state = ssh_session_is_known_server(m_ssh);
#endif

    ssh_key server_key;
    if (ssh_get_server_publickey(m_ssh, &server_key) < 0)
        return false;

    unsigned char *hash_buf = nullptr;
    size_t hash_len;
    if (ssh_get_publickey_hash(server_key, SSH_PUBLICKEY_HASH_SHA1, &hash_buf, &hash_len) < 0)
        return false;
    char *temp = ssh_get_hexa(hash_buf, hash_len);
    Glib::ustring hash_str(temp);
    ssh_string_free_char(temp);
    ssh_clean_pubkey_hash(&hash_buf);

    switch (state) {
#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    case SSH_SERVER_KNOWN_OK:
#else
    case SSH_KNOWN_HOSTS_OK:
#endif
        break;

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    case SSH_SERVER_KNOWN_CHANGED:
#else
    case SSH_KNOWN_HOSTS_CHANGED:
#endif
        {
            auto text = Glib::ustring::compose(
                            "Host key for %1 has changed\n"
                            "New server key: %2\n\n"
                            "Connect anyway? (<b>NOT RECOMMENDED</b> unless you trust the new key)",
                            m_hostname, hash_str);
            Gtk::MessageDialog dialog(m_parent, text, true, Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO);
            int response = dialog.run();
            if (response == Gtk::RESPONSE_NO)
                return false;
        }
        break;

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    case SSH_SERVER_FOUND_OTHER:
#else
    case SSH_KNOWN_HOSTS_OTHER:
#endif
        {
            auto text = Glib::ustring::compose(
                            "The host key for %1 was not found, but another type of key exists.\n"
                            "Connect anyway? (<b>NOT RECOMMENDED</b> unless you trust the new key)",
                            m_hostname);
            Gtk::MessageDialog dialog(m_parent, text, true, Gtk::MESSAGE_QUESTION,
                                      Gtk::BUTTONS_YES_NO);
            int response = dialog.run();
            if (response == Gtk::RESPONSE_NO)
                return false;
        }
        break;

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    case SSH_SERVER_FILE_NOT_FOUND:
    case SSH_SERVER_NOT_KNOWN:
#else
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
#endif
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

            if (ssh_session_update_known_hosts(m_ssh) < 0) {
                text = Glib::ustring::compose("Error writing SSH host key: %1",
                                              ssh_get_error(m_ssh));
                Gtk::MessageDialog dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
                (void)dialog.run();
                return false;
            }
        }
        break;

#if LIBSSH_VERSION_INT < SSH_VERSION_INT(0, 8, 0)
    case SSH_SERVER_ERROR:
#else
    case SSH_KNOWN_HOSTS_ERROR:
#endif
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
    AppSettings settings;

    Gtk::Dialog dialog("SSH Authentication", m_parent);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Ok", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);

    auto *grid = Gtk::manage(new Gtk::Grid);
    grid->set_row_spacing(10);
    grid->set_column_spacing(5);
    grid->set_border_width(5);
    Gtk::Label *hint_label = Gtk::manage(new Gtk::Label("SSH Host:"));
    hint_label->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    Gtk::Label *host_hint = Gtk::manage(new Gtk::Label(m_server_desc));
    host_hint->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    Gtk::Label *label = Gtk::manage(new Gtk::Label("Password:"));
    label->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    Gtk::Entry *password = Gtk::manage(new Gtk::Entry);
    password->set_activates_default(true);
    password->set_visibility(false);
    Gtk::CheckButton *remember = Gtk::manage(new Gtk::CheckButton("_Remember password", true));
    remember->set_active(settings.get_save_ssh_password());

    grid->attach(*hint_label, 0, 0, 1, 1);
    grid->attach(*host_hint, 1, 0, 1, 1);
    grid->attach(*label, 0, 1, 1, 1);
    grid->attach(*password, 1, 1, 1, 1);
    grid->attach(*remember, 0, 2, 2, 1);

    auto vbox = dialog.get_child();
    dynamic_cast<Gtk::Container *>(vbox)->add(*grid);

    CredentialStorage creds;
    creds.got_ssh_password().connect([password, remember](const Glib::ustring &saved_password) {
        if (!saved_password.empty()) {
            password->set_text(saved_password);
            password->select_region(0, saved_password.size());
            remember->set_active(true);
        }
    });
    creds.fetch_ssh_password(m_server_desc);

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

    if (remember->get_active())
        CredentialStorage::remember_ssh_password(m_server_desc, password->get_text());
    else
        CredentialStorage::forget_ssh_password(m_server_desc);
    settings.set_save_ssh_password(remember->get_active());

    return true;
}

bool SshTunnel::interactive()
{
    AppSettings settings;

    int result = ssh_userauth_kbdint(m_ssh, nullptr, nullptr);
    while (result == SSH_AUTH_INFO) {
        const char *name = ssh_userauth_kbdint_getname(m_ssh);
        const char *instruction = ssh_userauth_kbdint_getinstruction(m_ssh);
        int nprompts = ssh_userauth_kbdint_getnprompts(m_ssh);

        Gtk::Dialog dialog(name, m_parent);
        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("_Ok", Gtk::RESPONSE_OK);
        dialog.set_default_response(Gtk::RESPONSE_OK);

        auto *grid = Gtk::manage(new Gtk::Grid);
        grid->set_row_spacing(10);
        grid->set_column_spacing(5);
        grid->set_border_width(5);

        Gtk::Label *instruction_label = Gtk::manage(new Gtk::Label(instruction));
        instruction_label->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_START);
        grid->attach(*instruction_label, 0, 0, 2, 1);

        for (int i = 0; i < nprompts; ++i) {
            char echo;
            const char *prompt = ssh_userauth_kbdint_getprompt(m_ssh, i, &echo);

            Gtk::Label *prompt_label = Gtk::manage(new Gtk::Label(prompt));
            prompt_label->set_alignment(Gtk::ALIGN_START, Gtk::ALIGN_START);

            Gtk::Entry *input_entry = Gtk::manage(new Gtk::Entry);
            input_entry->set_activates_default(true);
            input_entry->set_visibility((bool)echo);

            grid->attach(*prompt_label, 0, (i + 1), 1, 1);
            grid->attach(*input_entry, 1, (i + 1), 1, 1);
        }

        if (nprompts > 0 || (instruction && instruction[0])) {
            auto vbox = dialog.get_child();
            dynamic_cast<Gtk::Container *>(vbox)->add(*grid);
            dialog.show_all();
            int response = dialog.run();
            dialog.hide();

            if (response != Gtk::RESPONSE_OK)
                return false;
        }

        for (int i = 0; i < nprompts; ++i) {
            auto input_entry = dynamic_cast<Gtk::Entry*>(grid->get_child_at(1, i + 1));
            if (input_entry) {
                ssh_userauth_kbdint_setanswer(m_ssh, i, input_entry->get_text().c_str());
            }
        }

        result = ssh_userauth_kbdint(m_ssh, nullptr, nullptr);
    }

    if (result != SSH_AUTH_SUCCESS) {
        auto text = Glib::ustring::compose("Error connecting to %1: %2", m_hostname,
                                           ssh_get_error(m_ssh));
        Gtk::MessageDialog err_dialog(m_parent, text, false, Gtk::MESSAGE_ERROR);
        (void)err_dialog.run();
        return false;
    }

    return true;
}

struct ForwardClient
{
    ssh_channel m_channel;
    Glib::RefPtr<Gio::Socket> m_socket;

    ForwardClient() : m_channel() { }

    ~ForwardClient()
    {
        if (m_channel)
            ssh_channel_free(m_channel);
    }

    ForwardClient(ForwardClient &&src) noexcept
        : m_channel(src.m_channel), m_socket(std::move(src.m_socket))
    {
        src.m_channel = nullptr;
    }

    ForwardClient &operator=(ForwardClient &&src) noexcept
    {
        m_channel = src.m_channel;
        m_socket = std::move(src.m_socket);
        src.m_channel = nullptr;
        return *this;
    }

    ForwardClient(const ForwardClient &) = delete;
    ForwardClient &operator=(const ForwardClient &) = delete;
};

void SshTunnel::tunnel_server(int ssh_fd)
{
    std::vector<ssh_channel> r_channels, w_channels;
    std::vector<ForwardClient> clients;
    fd_set rfds;
    struct timeval timeout{};
    int server_fd = m_forward_socket->get_fd();
    char buffer[FORWARD_BUFFER_SIZE];

    for ( ;; ) {
        FD_ZERO(&rfds);
        FD_SET(ssh_fd, &rfds);
        if (server_fd)
            FD_SET(server_fd, &rfds);
        int maxfd = std::max(ssh_fd, server_fd) + 1;
        r_channels.resize(clients.size() + 1);
        w_channels.resize(clients.size() + 1);
        for (size_t i = 0; i < clients.size(); ++i) {
            r_channels[i] = clients[i].m_channel;
            w_channels[i] = nullptr;
            int fd = clients[i].m_socket->get_fd();
            FD_SET(fd, &rfds);
            maxfd = std::max(maxfd, fd + 1);
        }
        r_channels[clients.size()] = nullptr;
        w_channels[clients.size()] = nullptr;

        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        int result = ssh_select(r_channels.data(), w_channels.data(),
                                maxfd, &rfds, &timeout);
        if (m_eof)
            return;
        if (result == EINTR || result == SSH_EINTR)
            continue;

        if (FD_ISSET(server_fd, &rfds)) {
            ForwardClient client;
            try {
                client.m_socket = m_forward_socket->accept();
            } catch (Gio::Error &err) {
                std::cerr << "Error accepting forward socket: "
                          << err.what() << std::endl;
                continue;
            }
            client.m_channel = ssh_channel_new(m_ssh);
            if (!client.m_channel) {
                std::cerr << "Error creating forwarding channel: "
                          << ssh_get_error(m_ssh) << std::endl;
                continue;
            }

            auto local_address = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(m_forward_socket->get_local_address());
            if (ssh_channel_open_forward(client.m_channel, m_remote_host.c_str(), m_remote_port,
                                         local_address->get_address()->to_string().c_str(),
                                         local_address->get_port()) != SSH_OK) {
                std::cerr << "Error opening forwarding channel: "
                          << ssh_get_error(m_ssh) << std::endl;
                continue;
            }
            clients.emplace_back(std::move(client));
            continue;
        }

        auto client = clients.begin();
        while (client != clients.end()) {
            if (!FD_ISSET(client->m_socket->get_fd(), &rfds)) {
                ++client;
                continue;
            }

            gssize in_size;
            try {
                in_size = client->m_socket->receive(buffer, FORWARD_BUFFER_SIZE);
            } catch (Gio::Error &err) {
                std::cerr << "Error reading from local socket: "
                          << err.what() << std::endl;
                client = clients.erase(client);
                break;
            }
            if (in_size == 0) {
                ssh_channel_send_eof(client->m_channel);
                client = clients.erase(client);
                continue;
            }

            char *bufp = buffer;
            while (in_size) {
                int out_size = ssh_channel_write(client->m_channel, bufp, in_size);
                if (out_size < 0) {
                    std::cerr << "Error writing to SSH channel: "
                              << ssh_get_error(m_ssh) << std::endl;
                    ssh_channel_close(client->m_channel);
                    break;
                }
                in_size -= out_size;
                bufp += out_size;
            }
            if (ssh_channel_is_closed(client->m_channel))
                client = clients.erase(client);
            else
                ++client;
        }

        client = clients.begin();
        while (client != clients.end()) {
            if (std::find(w_channels.begin(), w_channels.end(), client->m_channel) == w_channels.end()) {
                ++client;
                continue;
            }

            for (int is_stderr : {0, 1}) {
                while (ssh_channel_is_open(client->m_channel)
                       && ssh_channel_poll(client->m_channel, is_stderr)) {
                    int in_size = ssh_channel_read(client->m_channel, buffer,
                                                   FORWARD_BUFFER_SIZE, 0);
                    if (in_size < 0) {
                        std::cerr << "Error reading from SSH channel: "
                                  << ssh_get_error(m_ssh) << std::endl;
                        ssh_channel_close(client->m_channel);
                        continue;
                    }
                    if (in_size == 0)
                        ssh_channel_close(client->m_channel);
                    gchar *bufp = buffer;
                    while (in_size) {
                        gssize out_size;
                        try {
                            out_size = client->m_socket->send(bufp, in_size);
                        } catch (Gio::Error &err) {
                            std::cerr << "Error writing to local socket: "
                                      << err.what() << std::endl;
                            ssh_channel_close(client->m_channel);
                            continue;
                        }
                        in_size -= out_size;
                        bufp += out_size;
                    }
                }
            }
            if (ssh_channel_is_closed(client->m_channel))
                client = clients.erase(client);
            else
                ++client;
        }
    }
}
