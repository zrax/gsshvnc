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

#include "vncgrabsequencemm.h"

#include <gtkmm/label.h>

Vnc::GrabSequence::GrabSequence(std::vector<guint> keysyms)
{
    m_seq = vnc_grab_sequence_new(static_cast<guint>(keysyms.size()),
                                  keysyms.data());
    m_owned = true;
}

Vnc::GrabSequence::GrabSequence(const Glib::ustring &str)
{
    m_seq = vnc_grab_sequence_new_from_string(str.c_str());
    m_owned = true;
}

Vnc::GrabSequence::~GrabSequence()
{
    if (m_owned && m_seq)
        vnc_grab_sequence_free(m_seq);
}

Glib::ustring Vnc::GrabSequence::as_string()
{
    gchar *str = vnc_grab_sequence_as_string(m_seq);
    Glib::ustring result(str);
    g_free(str);
    return result;
}

guint Vnc::GrabSequence::get_nth(guint n)
{
    return vnc_grab_sequence_get_nth(m_seq, n);
}
