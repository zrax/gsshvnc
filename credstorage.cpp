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

#include <iostream>

#if defined(USE_LIBSECRET)

#include <libsecret/secret.h>

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

CredentialStorage::CredentialStorage()
    : m_cancel(Gio::Cancellable::create())
{
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

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>

#include <locale>
#include <codecvt>

CredentialStorage::CredentialStorage() { }
CredentialStorage::~CredentialStorage() { }

static inline std::wstring _to_wstring(const Glib::ustring &ustr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(ustr);
}

static inline Glib::ustring _to_ustring(const std::wstring &wstr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(wstr);
}

void CredentialStorage::remember_ssh_password(const Glib::ustring &ssh_user_host,
                                              const Glib::ustring &password)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-ssh/%1", ssh_user_host));

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = password.size();
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(password.c_str()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    CredWriteW(&credential, 0);
}

void CredentialStorage::forget_ssh_password(const Glib::ustring &ssh_user_host)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-ssh/%1", ssh_user_host));
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

void CredentialStorage::fetch_ssh_password(const Glib::ustring &ssh_user_host)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-ssh/%1", ssh_user_host));

    CREDENTIALW *credential = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        Glib::ustring password(reinterpret_cast<const char *>(credential->CredentialBlob),
                               credential->CredentialBlobSize);
        m_got_ssh_password.emit(password);
        CredFree(credential);
    }
}

void CredentialStorage::remember_vnc_password(const Glib::ustring &ssh_host,
                                              const Glib::ustring &vnc_host,
                                              const Glib::ustring &vnc_user,
                                              const Glib::ustring &password)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-vnc/%1/%2", ssh_host, vnc_host));
    auto storage = Glib::ustring::compose("%1@%2", vnc_user, password);

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = storage.size();
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(storage.c_str()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    CredWriteW(&credential, 0);
}

void CredentialStorage::forget_vnc_password(const Glib::ustring &ssh_host,
                                            const Glib::ustring &vnc_host)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-vnc/%1/%2", ssh_host, vnc_host));
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

void CredentialStorage::fetch_vnc_user_password(const Glib::ustring &ssh_host,
                                                const Glib::ustring &vnc_host)
{
    auto target = _to_wstring(Glib::ustring::compose("gsshvnc-vnc/%1/%2", ssh_host, vnc_host));

    CREDENTIALW *credential = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        Glib::ustring storage(reinterpret_cast<const char *>(credential->CredentialBlob),
                              credential->CredentialBlobSize);

        Glib::ustring username(storage);
        Glib::ustring password;
        auto upos = username.find('@');
        if (upos == std::string::npos) {
            std::cerr << "Ignoring invalid credential storage format" << std::endl;
        } else {
            password = username.substr(upos + 1);
            username.resize(upos);
            m_got_vnc_password.emit(username, password);
        }

        CredFree(credential);
    }
}

#else

#error This platform is not yet supported

#endif
