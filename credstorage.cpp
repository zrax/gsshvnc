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

#include "credstorage.h"

#include <libsecret/secret.h>
#include <iostream>

static void _password_stored(GObject *, GAsyncResult *result, gpointer)
{
    GError *error = nullptr;
    secret_password_store_finish(result, &error);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error saving password: " << error->message << std::endl;
        g_error_free(error);
    }
}

static void _password_cleared(GObject *, GAsyncResult *result, gpointer)
{
    GError *error = nullptr;
    secret_password_clear_finish(result, &error);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error clearing password: " << error->message << std::endl;
        g_error_free(error);
    }
}

CredentialStorage::~CredentialStorage()
{
    m_cancel->cancel();
}

static const SecretSchema *_ssh_password_schema()
{
    static SecretSchema schema = {};
    if (!schema.name) {
        schema.name = "net.zrax.gsshvnc.SshPassword";
        schema.flags = SECRET_SCHEMA_NONE;
        schema.attributes[0] = { "app_type", SECRET_SCHEMA_ATTRIBUTE_STRING };
        schema.attributes[1] = { "user_host", SECRET_SCHEMA_ATTRIBUTE_STRING };
    }
    return &schema;
}

void CredentialStorage::remember_ssh_password(const Glib::ustring &ssh_user_host,
                                              const Glib::ustring &password)
{
    secret_password_store(_ssh_password_schema(), SECRET_COLLECTION_DEFAULT,
                          "gsshvnc SSH password", password.c_str(), nullptr,
                          &_password_stored, nullptr,
                          "app_type", "gsshvnc-ssh",
                          "user_host", ssh_user_host.c_str(),
                          nullptr);
}

void CredentialStorage::forget_ssh_password(const Glib::ustring &ssh_user_host)
{
    secret_password_clear(_ssh_password_schema(), nullptr,
                          &_password_cleared, nullptr,
                          "app_type", "gsshvnc-ssh",
                          "user_host", ssh_user_host.c_str(),
                          nullptr);
}

static void _ssh_password_found(GObject *, GAsyncResult *result, gpointer user)
{
    CredentialStorage *self = reinterpret_cast<CredentialStorage *>(user);

    GError *error = nullptr;
    gchar *password = secret_password_lookup_finish(result, &error);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error retrieving saved password: " << error->message << std::endl;
        g_error_free(error);
    } else if (password) {
        Glib::ustring result(password);
        secret_password_free(password);
        self->got_ssh_password().emit(result);
    }
}

void CredentialStorage::fetch_ssh_password(const Glib::ustring &ssh_user_host)
{
    m_cancel->reset();
    secret_password_lookup(_ssh_password_schema(), m_cancel->gobj(),
                           &_ssh_password_found, this,
                           "app_type", "gsshvnc-ssh",
                           "user_host", ssh_user_host.c_str(),
                           nullptr);
}

static const SecretSchema *_vnc_password_schema()
{
    static SecretSchema schema = {};
    if (!schema.name) {
        schema.name = "net.zrax.gsshvnc.VncPassword";
        schema.flags = SECRET_SCHEMA_NONE;
        schema.attributes[0] = { "app_type", SECRET_SCHEMA_ATTRIBUTE_STRING };
        schema.attributes[1] = { "ssh_host", SECRET_SCHEMA_ATTRIBUTE_STRING };
        schema.attributes[2] = { "vnc_host", SECRET_SCHEMA_ATTRIBUTE_STRING };
    }
    return &schema;
}

void CredentialStorage::remember_vnc_password(const Glib::ustring &ssh_host,
                                              const Glib::ustring &vnc_host,
                                              const Glib::ustring &vnc_user,
                                              const Glib::ustring &password)
{
    auto storage = Glib::ustring::compose("%1@%2", vnc_user, password);
    secret_password_store(_vnc_password_schema(), SECRET_COLLECTION_DEFAULT,
                          "gsshvnc VNC password", storage.c_str(), nullptr,
                          &_password_stored, nullptr,
                          "app_type", "gsshvnc-vnc",
                          "ssh_host", ssh_host.c_str(),
                          "vnc_host", vnc_host.c_str(),
                          nullptr);
}

void CredentialStorage::forget_vnc_password(const Glib::ustring &ssh_host,
                                            const Glib::ustring &vnc_host)
{
    secret_password_clear(_vnc_password_schema(), nullptr,
                          &_password_cleared, nullptr,
                          "app_type", "gsshvnc-vnc",
                          "ssh_host", ssh_host.c_str(),
                          "vnc_host", vnc_host.c_str(),
                          nullptr);
}

static void _vnc_password_found(GObject *, GAsyncResult *result, gpointer user)
{
    CredentialStorage *self = reinterpret_cast<CredentialStorage *>(user);

    GError *error = nullptr;
    gchar *storage = secret_password_lookup_finish(result, &error);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error retrieving saved credentials: " << error->message << std::endl;
        g_error_free(error);
    } else if (storage) {
        Glib::ustring username(storage);
        Glib::ustring password;
        secret_password_free(storage);

        auto upos = username.find('@');
        if (upos == std::string::npos) {
            std::cerr << "Ignoring invalid credential storage format" << std::endl;
        } else {
            password = username.substr(upos + 1);
            username.resize(upos);
            self->got_vnc_password().emit(username, password);
        }
    }
}

void CredentialStorage::fetch_vnc_user_password(const Glib::ustring &ssh_host,
                                                const Glib::ustring &vnc_host)
{
    m_cancel->reset();
    secret_password_lookup(_vnc_password_schema(), m_cancel->gobj(),
                           &_vnc_password_found, this,
                           "app_type", "gsshvnc-vnc",
                           "ssh_host", ssh_host.c_str(),
                           "vnc_host", vnc_host.c_str(),
                           nullptr);
}
