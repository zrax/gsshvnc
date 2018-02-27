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

#ifndef _APPSETTINGS_H
#define _APPSETTINGS_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <glibmm/ustring.h>

#define GSSHVNC_VERSION_STR "0.92"

namespace Glib
{

class KeyFile;

}

class AppSettings
{
public:
    AppSettings();
    ~AppSettings();

    std::vector<Glib::ustring> get_recent_hosts() const;
    void add_recent_host(const Glib::ustring &host);

    std::vector<Glib::ustring> get_recent_ssh_hosts() const;
    void add_recent_ssh_host(const Glib::ustring &host);

    std::vector<Glib::ustring> get_recent_ssh_users() const;
    void add_recent_ssh_user(const Glib::ustring &user);

    bool get_enable_tunnel() const;
    void set_enable_tunnel(bool enable);

    bool get_lossy_compression() const;
    void set_lossy_compression(bool enable);

    Glib::ustring get_color_depth() const;
    void set_color_depth(const Glib::ustring &value);

    bool get_capture_keyboard() const;
    void set_capture_keyboard(bool enable);

    bool get_scaled_display() const;
    void set_scaled_display(bool enable);

    bool get_smooth_scaling() const;
    void set_smooth_scaling(bool enable);

private:
    std::unordered_map<std::string, Glib::ustring> m_values;
    std::unordered_set<std::string> m_modified_keys;

    void read_setting(Glib::KeyFile &conf_file, const Glib::ustring &group_name,
                      const Glib::ustring &key, const Glib::ustring &default_value = "");

    std::vector<Glib::ustring> get_string_list(const std::string &key) const;
    bool get_bool(const std::string &key) const;

    void add_cycle_string_list(const std::string &key, const Glib::ustring &value);
    void set_bool(const std::string &key, bool value);
};

#endif
