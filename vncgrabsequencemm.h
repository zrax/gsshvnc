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

#ifndef _VNCGRABSEQUENCEMM_H
#define _VNCGRABSEQUENCEMM_H

#include <glibmm/ustring.h>
#include <gtkmm/dialog.h>
#include <vector>
#include <vncgrabsequence.h>

namespace Vnc
{

class GrabSequence
{
public:
    explicit GrabSequence(VncGrabSequence *seq, bool take_ownership = false)
        : m_seq(seq), m_owned(take_ownership) { }
    explicit GrabSequence(std::vector<guint> keysyms);
    explicit GrabSequence(const Glib::ustring &str);
    ~GrabSequence();

    // Disable copy
    GrabSequence(const GrabSequence &) = delete;
    GrabSequence &operator=(const GrabSequence &) = delete;

    GrabSequence(GrabSequence &&m) noexcept
        : m_seq(m.m_seq), m_owned(m.m_owned) { m.m_owned = false; }

    GrabSequence &operator=(GrabSequence &&m) noexcept
    {
        m_seq = m.m_seq;
        m_owned = m.m_owned;
        m.m_owned = false;
        return *this;
    }

    Glib::ustring as_string();

    guint get_nth(guint n);
    guint operator[](guint n) { return get_nth(n); }

    VncGrabSequence *gobj() { return m_seq; }
    const VncGrabSequence *gobj() const { return m_seq; }

private:
    VncGrabSequence *m_seq;
    bool m_owned;
};

}

#endif
