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

#ifndef _CREDSTORAGE_H
#define _CREDSTORAGE_H

#include <glibmm/ustring.h>
#include <giomm/cancellable.h>
#include <sigc++/sigc++.h>

class CredentialStorage
{
public:
    CredentialStorage() : m_cancel(Gio::Cancellable::create()) { }
    ~CredentialStorage();

    static void remember_ssh_password(const Glib::ustring &ssh_user_host,
                                      const Glib::ustring &password);
    static void forget_ssh_password(const Glib::ustring &ssh_user_host);

    void fetch_ssh_password(const Glib::ustring &ssh_user_host);

    static void remember_vnc_password(const Glib::ustring &ssh_host,
                                      const Glib::ustring &vnc_host,
                                      const Glib::ustring &vnc_user,
                                      const Glib::ustring &password);
    static void forget_vnc_password(const Glib::ustring &ssh_host,
                                    const Glib::ustring &vnc_host);

    void fetch_vnc_user_password(const Glib::ustring &ssh_host,
                                 const Glib::ustring &vnc_host);

    sigc::signal<void, Glib::ustring> &
            got_ssh_password() { return m_got_ssh_password; }
    sigc::signal<void, Glib::ustring, Glib::ustring> &
            got_vnc_password() { return m_got_vnc_password; }

private:
    sigc::signal<void, Glib::ustring> m_got_ssh_password;
    sigc::signal<void, Glib::ustring, Glib::ustring> m_got_vnc_password;

    Glib::RefPtr<Gio::Cancellable> m_cancel;
};

#endif // _CREDSTORAGE_H
