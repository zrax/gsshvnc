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

#include "appsettings.h"

#include <glibmm/miscutils.h>
#include <glibmm/keyfile.h>
#include <vncdisplay.h>
#include <iostream>

AppSettings::AppSettings()
{
    auto conf_path = Glib::build_filename(Glib::get_user_config_dir(), "gsshvnc", "settings.ini");
    Glib::KeyFile config_file;
    try {
        config_file.load_from_file(conf_path, Glib::KEY_FILE_NONE);
    } catch (Glib::Error &err) {
        /* Could not load file -- will use defaults instead */
    }

    read_setting(config_file, "ConnectionDialog", "RecentHosts");
    read_setting(config_file, "ConnectionDialog", "RecentSshHosts");
    read_setting(config_file, "ConnectionDialog", "RecentSshUsers");
    read_setting(config_file, "ConnectionDialog", "EnableSshTunnel", "false");
    read_setting(config_file, "ConnectionDialog", "LossyCompression", "true");
    read_setting(config_file, "ConnectionDialog", "ColorDepth",
                 std::to_string(VNC_DISPLAY_DEPTH_COLOR_DEFAULT));

    read_setting(config_file, "Main", "CaptureKeyboard", "true");
    read_setting(config_file, "Main", "ScaledDisplay", "true");
    read_setting(config_file, "Main", "SmoothScaling", "true");
    read_setting(config_file, "Main", "KeepAspectRatio", "true");
    read_setting(config_file, "Main", "AllowResize", "false");
    read_setting(config_file, "Main", "SaveSSHPassword", "false");
    read_setting(config_file, "Main", "SaveVNCCredentials", "false");
    read_setting(config_file, "Main", "WindowSize");
}

AppSettings::~AppSettings()
{
    if (m_modified_keys.empty())
        return;

    auto conf_dir = Glib::build_filename(Glib::get_user_config_dir(), "gsshvnc");
    auto conf_path = Glib::build_filename(conf_dir, "settings.ini");
    Glib::KeyFile config_file;
    try {
        config_file.load_from_file(conf_path, Glib::KEY_FILE_KEEP_COMMENTS);
    } catch (Glib::Error &err) {
        /* Could not load file -- will create new file instead */
    }
    for (const auto &modified : m_modified_keys) {
        auto value = m_values.at(modified);
        size_t split = modified.find('/');
        auto group = modified.substr(0, split);
        auto key = modified.substr(split + 1);
        config_file.set_value(group, key, value);
    }

    g_mkdir_with_parents(conf_dir.c_str(), 0700);
    config_file.save_to_file(conf_path);
}

std::vector<Glib::ustring> AppSettings::get_recent_hosts() const
{
    return get_string_list("ConnectionDialog/RecentHosts");
}

void AppSettings::add_recent_host(const Glib::ustring &host)
{
    add_cycle_string_list("ConnectionDialog/RecentHosts", host);
}

std::vector<Glib::ustring> AppSettings::get_recent_ssh_hosts() const
{
    return get_string_list("ConnectionDialog/RecentSshHosts");
}

void AppSettings::add_recent_ssh_host(const Glib::ustring &host)
{
    add_cycle_string_list("ConnectionDialog/RecentSshHosts", host);
}

std::vector<Glib::ustring> AppSettings::get_recent_ssh_users() const
{
    return get_string_list("ConnectionDialog/RecentSshUsers");
}

void AppSettings::add_recent_ssh_user(const Glib::ustring &user)
{
    add_cycle_string_list("ConnectionDialog/RecentSshUsers", user);
}

bool AppSettings::get_enable_tunnel() const
{
    return get_bool("ConnectionDialog/EnableSshTunnel");
}

void AppSettings::set_enable_tunnel(bool enable)
{
    set_bool("ConnectionDialog/EnableSshTunnel", enable);
}

bool AppSettings::get_lossy_compression() const
{
    return get_bool("ConnectionDialog/LossyCompression");
}

void AppSettings::set_lossy_compression(bool enable)
{
    set_bool("ConnectionDialog/LossyCompression", enable);
}

Glib::ustring AppSettings::get_color_depth() const
{
    return m_values.at("ConnectionDialog/ColorDepth");
}

void AppSettings::set_color_depth(const Glib::ustring &value)
{
    m_values["ConnectionDialog/ColorDepth"] = value;
    m_modified_keys.insert("ConnectionDialog/ColorDepth");
}

bool AppSettings::get_capture_keyboard() const
{
    return get_bool("Main/CaptureKeyboard");
}

void AppSettings::set_capture_keyboard(bool enable)
{
    set_bool("Main/CaptureKeyboard", enable);
}

bool AppSettings::get_scaled_display() const
{
    return get_bool("Main/ScaledDisplay");
}

void AppSettings::set_scaled_display(bool enable)
{
    set_bool("Main/ScaledDisplay", enable);
}

bool AppSettings::get_smooth_scaling() const
{
    return get_bool("Main/SmoothScaling");
}

void AppSettings::set_smooth_scaling(bool enable)
{
    set_bool("Main/SmoothScaling", enable);
}

bool AppSettings::get_keep_aspect_ratio() const
{
    return get_bool("Main/KeepAspectRatio");
}

void AppSettings::set_keep_aspect_ratio(bool enable)
{
    set_bool("Main/KeepAspectRatio", enable);
}

bool AppSettings::get_save_ssh_password() const
{
    return get_bool("Main/SaveSSHPassword");
}

void AppSettings::set_allow_resize(bool enable)
{
    set_bool("Main/AllowResize", enable);
}

bool AppSettings::get_allow_resize() const
{
    return get_bool("Main/AllowResize");
}

void AppSettings::set_save_ssh_password(bool save)
{
    set_bool("Main/SaveSSHPassword", save);
}

bool AppSettings::get_save_vnc_credentials() const
{
    return get_bool("Main/SaveVNCCredentials");
}

void AppSettings::set_save_vnc_credentials(bool save)
{
    set_bool("Main/SaveVNCCredentials", save);
}

std::tuple<int, int> AppSettings::get_window_size() const
{
    auto pos_str = m_values.at("Main/WindowSize");
    auto position = std::make_tuple(-1, -1);

    size_t comma = pos_str.find(',');
    if (comma != Glib::ustring::npos) {
        std::get<0>(position) = std::strtol(pos_str.c_str(), nullptr, 0);
        std::get<1>(position) = std::strtol(pos_str.c_str() + comma + 1, nullptr, 0);
    }
    return position;
}

void AppSettings::set_window_size(int w, int h)
{
    m_values["Main/WindowSize"] = Glib::ustring::compose("%1,%2", w, h);
    m_modified_keys.insert("Main/WindowSize");
}

void AppSettings::read_setting(Glib::KeyFile &conf_file, const Glib::ustring &group_name,
                               const Glib::ustring &key, const Glib::ustring &default_value)
{
    std::string store_key = Glib::ustring::compose("%1/%2", group_name, key);
    try {
        if (conf_file.has_group(group_name) && conf_file.has_key(group_name, key)) {
            m_values[store_key] = conf_file.get_value(group_name, key);
        } else {
            m_values[store_key] = default_value;
        }
    } catch (Glib::KeyFileError &err) {
        m_values[store_key] = default_value;
    }
}

std::vector<Glib::ustring> AppSettings::get_string_list(const std::string &key) const
{
    auto value = m_values.at(key);
    if (value.empty())
        return {};

    std::vector<Glib::ustring> result;
    size_t start = 0, end = 0;
    while ((end = value.find(';', start)) != Glib::ustring::npos) {
        result.emplace_back(value.substr(start, end - start));
        start = end + 1;
    }
    result.emplace_back(value.substr(start));
    return result;
}

bool AppSettings::get_bool(const std::string &key) const
{
    auto value = m_values.at(key);
    try {
        return (value.lowercase() == "true" || std::stoi(value) != 0);
    } catch (std::invalid_argument &err) {
        return false;
    }
}

void AppSettings::add_cycle_string_list(const std::string &key,
                                        const Glib::ustring &value)
{
    auto old_values = get_string_list(key);
    std::unordered_set<std::string> seen;
    std::vector<Glib::ustring> out_values;
    out_values.reserve(old_values.size() + 1);

    out_values.emplace_back(value);
    seen.emplace(value);
    for (const auto &ov : old_values) {
        if (seen.find(ov) == seen.end()) {
            out_values.emplace_back(ov);
            seen.emplace(ov);
        }
        if (out_values.size() == 10)
            break;
    }
    m_values[key] = Glib::build_path(";", out_values);
    m_modified_keys.insert(key);
}

void AppSettings::set_bool(const std::string &key, bool value)
{
    m_values[key] = value ? "true" : "false";
    m_modified_keys.insert(key);
}
