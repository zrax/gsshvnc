# This file is part of gsshvnc.
#
# gsshvnc is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# gsshvnc is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gsshvnc.  If not, see <http://www.gnu.org/licenses/>.

project('gsshvnc', 'cpp',
        version: '0.94.0',
        license: 'GPLv2+',
        meson_version: '>=0.37',
        default_options: ['cpp_std=c++14', 'warning_level=3']
)

# GCC 8 introduces these warnings which break gtkmm/sigc++
cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'gcc' and cpp_compiler.version().version_compare('>=8.0')
    add_global_arguments('-Wno-cast-function-type', '-Wno-parentheses',
                         language: ['c', 'cpp'])
endif

gvnc_dep = dependency('gvnc-1.0')
gtk_vnc_dep = dependency('gtk-vnc-2.0')
gtkmm_dep = dependency('gtkmm-3.0')
gsshvnc_deps = [gvnc_dep, gtk_vnc_dep, gtkmm_dep]

gsshvnc_defs = []
if host_machine.system() != 'windows'
    secret_dep = dependency('libsecret-1')
    gsshvnc_deps += [secret_dep]
    gsshvnc_defs += ['-DUSE_LIBSECRET']
endif

if gtk_vnc_dep.version().version_compare('>=0.7.0')
    gsshvnc_defs += ['-DGTK_VNC_HAVE_SMOOTH_SCALING']
endif

gvncpulse_dep = dependency('gvncpulse-1.0', required: false)
if gvncpulse_dep.found()
    gsshvnc_deps += [gvncpulse_dep]
    gsshvnc_defs += ['-DHAVE_PULSEAUDIO']
endif

libssh_dep = dependency('libssh')
libssh_threads_dep = dependency('libssh_threads', required: false)
gsshvnc_deps += [libssh_dep, libssh_threads_dep]

thread_dep = dependency('threads')
gsshvnc_deps += [thread_dep]

if host_machine.system() == 'windows'
    gsshvnc_deps += cpp_compiler.find_library('ws2_32')
endif

gsshvnc_src = [
    'gsshvnc.cpp',
    'appsettings.cpp',
    'credstorage.cpp',
    'sshtunnel.cpp',
    'vncconnectdialog.cpp',
    'vncdisplaymm.cpp',
    'vncgrabsequencemm.cpp',
]

if host_machine.system() == 'windows'
    windows = import('windows')
    gsshvnc_src += windows.compile_resources('win32/gsshvnc.rc')
endif

executable('gsshvnc', gsshvnc_src,
           dependencies: gsshvnc_deps,
           cpp_args: gsshvnc_defs,
           gui_app: true,
           install: true
)
