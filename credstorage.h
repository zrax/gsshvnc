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

class CredentialStorage
{
public:
    static void remember_ssh_password(const Glib::ustring &ssh_user_host,
                                      const Glib::ustring &password);

    static Glib::ustring fetch_ssh_password(const Glib::ustring &ssh_user_host);
};

#endif // _CREDSTORAGE_H
