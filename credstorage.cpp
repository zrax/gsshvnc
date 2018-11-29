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

static const SecretSchema *_ssh_password_schema()
{
    static SecretSchema schema = {};
    if (!schema.name) {
        schema.name = "net.zrax.gsshvnc.SshPassword";
        schema.flags = SECRET_SCHEMA_NONE;
        schema.attributes[0] = { "user_host", SECRET_SCHEMA_ATTRIBUTE_STRING };
    }
    return &schema;
}

static void _ssh_password_stored(GObject *, GAsyncResult *result, gpointer)
{
    GError *error = nullptr;
    secret_password_store_finish(result, &error);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error saving password: " << error->message << std::endl;
        g_error_free(error);
    }
}

void CredentialStorage::remember_ssh_password(const Glib::ustring &ssh_user_host,
                                              const Glib::ustring &password)
{
    auto label = Glib::ustring::compose("gsshvnc %1", ssh_user_host);
    secret_password_store(_ssh_password_schema(), SECRET_COLLECTION_DEFAULT,
                          label.c_str(), password.c_str(), nullptr,
                          &_ssh_password_stored, nullptr,
                          "user_host", ssh_user_host.c_str(),
                          nullptr);
}

Glib::ustring CredentialStorage::fetch_ssh_password(const Glib::ustring &ssh_user_host)
{
    GError *error = nullptr;
    gchar *password = secret_password_lookup_sync(_ssh_password_schema(),
                            nullptr, &error,
                            "user_host", ssh_user_host.c_str(),
                            nullptr);
    if (error) {
        // Don't bother the UI
        std::cerr << "Error retrieving saved password: " << error->message << std::endl;
        g_error_free(error);
    } else if (password) {
        Glib::ustring result(password);
        secret_password_free(password);
        return result;
    }
    return {};
}
