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


Vnc::GrabSequenceDialog::GrabSequenceDialog(Gtk::Window &parent)
    : Gtk::Dialog("Key Recorder", parent, Gtk::DIALOG_MODAL | Gtk::DIALOG_DESTROY_WITH_PARENT),
      m_curkeys(0), m_set(false)
{
    add_button("_Ok", Gtk::RESPONSE_ACCEPT);
    add_button("_Cancel", Gtk::RESPONSE_REJECT);

    m_label = Gtk::manage(new Gtk::Label("Please press desired grab key combination"));

    signal_key_press_event().connect(sigc::mem_fun(this, &GrabSequenceDialog::on_key_press));
    signal_key_release_event().connect(sigc::mem_fun(this, &GrabSequenceDialog::on_key_release));
    set_size_request(300, 100);
    auto content_area = get_content_area();
    reinterpret_cast<Gtk::Container *>(content_area)->add(*m_label);
}

Vnc::GrabSequence Vnc::GrabSequenceDialog::get_sequence()
{
    return Vnc::GrabSequence(m_keysyms);
}

static bool dialog_key_ignore(int keyval)
{
    switch (keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_Escape:
        return true;
    default:
        return false;
    }
}

void Vnc::GrabSequenceDialog::update_keysyms()
{
    Glib::ustring keys;
    for (guint key : m_keysyms)
        keys = keys + (keys.empty() ? " " : "+") + gdk_keyval_name(key);

    m_label->set_text(keys);
}

bool Vnc::GrabSequenceDialog::on_key_press(GdkEventKey *ev)
{
    if (dialog_key_ignore(ev->keyval))
        return false;

    if (m_set && m_curkeys)
        return false;

    /* Check whether we already have keysym in array - i.e. it was handler by something else */
    bool keySymExists = false;
    for (size_t i = 0; i < m_keysyms.size(); i++) {
        if (m_keysyms[i] == ev->keyval)
            keySymExists = true;
    }

    if (!keySymExists) {
        m_keysyms.resize(m_curkeys + 1);
        m_keysyms[m_curkeys] = ev->keyval;
        ++m_curkeys;
    }

    update_keysyms();

    if (!ev->is_modifier) {
        m_set = true;
        --m_curkeys;
    }

    return false;
}

bool Vnc::GrabSequenceDialog::on_key_release(GdkEventKey *ev)
{
    if (dialog_key_ignore(ev->keyval))
        return false;

    if (m_set) {
        if (m_curkeys == 0)
            m_set = false;
        if (m_curkeys)
            --m_curkeys;
        return false;
    }

    for (size_t i = 0; i < m_curkeys; ++i) {
        if (m_keysyms[i] == ev->keyval) {
            m_keysyms[i] = m_keysyms.back();
            --m_curkeys;
            m_keysyms.resize(m_curkeys);
        }
    }

    update_keysyms();

    return false;
}
