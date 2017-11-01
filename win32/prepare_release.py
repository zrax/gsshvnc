#!/usr/bin/env python3

### IMPORTANT:  This script should be run with msys2's python3, not mingw
###     or another native Windows python3 interpreter.

import os
import sys
import stat
import shutil
import glob
import subprocess

# The MSYS package and directory prefixes for deployed packages
pkg_prefix = 'mingw-w64-x86_64'
msys_prefix = '/mingw64'
staging_dir = '/tmp/gsshvnc-staging'

# Packages required to run gsshvnc
deploy_packages=[
    'adwaita-icon-theme',
    'atk',
    'atkmm',
    'bzip2',
    #'ca-certificates',
    'cairo',
    'cairomm',
    'cyrus-sasl',
    'expat',
    'fontconfig',
    'freetype',
    'gcc-libs',
    'gdk-pixbuf2',
    'gettext',
    'glib2',
    'glibmm',
    'gmp',
    'gnutls',
    'graphite2',
    'gtk-vnc',
    'gtk3',
    'gtkmm3',
    'harfbuzz',
    #'hicolor-icon-theme',
    'libcroco',
    'libepoxy',
    'libffi',
    'libgcrypt',
    'libgpg-error',
    'libiconv',
    'libidn2',
    'libpng',
    'librsvg',
    'libsigc++',
    'libssh',
    'libtasn1',
    'libunistring',
    'libxml2',
    'libwinpthread-git',
    'lzo2',
    'nettle',
    'openssl',
    'p11-kit',
    'pango',
    'pangomm',
    'pcre',
    'pixman',
    'shared-mime-info',
    'xz',
    'zlib',
]

# Files and directories to remove after taking a snapshot of the above packages
clean_patterns=[
    'bin/*.py',
    'bin/*.sh',
    'bin/*-config',
    'bin/*gettext*',
    'bin/asn1*.exe',
    'bin/autopoint',
    'bin/broadwayd.exe',
    'bin/bunzip2.exe',
    'bin/bz*',
    'bin/csslint*.exe',
    'bin/dumpsexp.exe',
    'bin/fc-*',
    'bin/gnutls-*.exe',
    'bin/gtk3-*.exe',
    'bin/gr2fonttest.exe',
    'bin/hb-*.exe',
    'bin/hmac256.exe',
    'bin/idn2.exe',
    'bin/lzma*.exe',
    'bin/mini*zip.exe',
    'bin/mpicalc.exe',
    'bin/msg*.exe',
    'bin/ocsptool.exe',
    'bin/openssl.exe',
    'bin/pcre*.exe',
    'bin/recode*.exe',
    'bin/rsvg-*.exe',
    'bin/sexp-conv.exe',
    'bin/srptool.exe',
    'bin/unxz.exe',
    'bin/xml*.exe',
    'bin/xz*',
    'include',
    'lib/*.a',
    'lib/*/*.a',
    'lib/*/include',
    'lib/*/proc',
    'lib/cmake',
    'lib/gdk-pixbuf-2.0/*/loaders/*.a',
    'lib/girepository-1.0',
    'lib/pkgconfig',
    'lib/python*',
    'lib/xml2Conf.sh',
    'share/aclocal',
    'share/applications',
    'share/bash-completion',
    'share/doc',
    'share/gdb',
    'share/gettext',
    'share/gir-1.0',
    'share/glib-2.0/codegen',
    'share/glib-2.0/valgrind',
    'share/graphite2/*.cmake',
    'share/gtk-doc',
    'share/icons/Adwaita/*/emotes',
    'share/icons/Adwaita/cursors',
    'share/info',
    'share/man',
    'share/pkgconfig',
    'share/vala',
    'var/cache',
]

basedir = os.path.dirname(os.path.realpath(__file__))
os.chdir(basedir)

if len(sys.argv) < 2:
    print('Usage: {} /path/to/gsshvnc.exe'.format(sys.argv[0]))
    sys.exit(1)
gsshvnc_exe = sys.argv[1]

def status_msg(text):
    print('\033[1;34m' + text + '\033[0m')

def pkg_get_files(pkgname):
    proc = subprocess.Popen(['pacman', '-Ql', pkgname], stdout=subprocess.PIPE)
    out_text, _ = proc.communicate()
    if proc.returncode != 0:
        sys.exit(proc.returncode)

    lines = out_text.split(b'\n')
    filenames = []
    for line in lines:
        if line.strip() == b'':
            continue
        parts = line.split(b' ')
        if len(parts) < 2:
            continue
        filenames.append(str(parts[1], 'utf-8'))

    return filenames

def run_pacman(args, root):
    proc = subprocess.Popen(['pacman', *args, '--root', root])
    proc.communicate()
    if proc.returncode != 0:
        sys.exit(proc.returncode)

def win32_permission_handler(func, path, exc_info):
    os.chmod(path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
    func(path)

def win32_remove(path):
    if os.path.isdir(path):
        try:
            shutil.rmtree(path, onerror=win32_permission_handler)
        except IOError as err:
            print("Failed to remove {}: {}".format(path, err), file=sys.stderr)
    else:
        try:
            os.chmod(path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
            os.unlink(path)
        except IOError as err:
            print("Failed to remove {}: {}".format(path, err), file=sys.stderr)

def remove_empty_dirs(path):
    if not os.path.isdir(path):
        return

    for file in os.listdir(path):
        remove_empty_dirs(os.path.join(path, file))
    if len(os.listdir(path)) == 0:
        os.chmod(path, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
        os.rmdir(path)

release_dirs = ('bin', 'etc', 'lib', 'libexec', 'sbin', 'share', 'ssl', 'var')

status_msg("Cleaning old release files")
for subdir in release_dirs:
    if os.path.exists(os.path.join(basedir, subdir)):
        win32_remove(os.path.join(basedir, subdir))

status_msg("Deploying required packages")
for pkg in deploy_packages:
    print('  - ' + pkg)
    pkg_files = pkg_get_files(pkg_prefix + '-' + pkg)
    for file in pkg_files:
        if not file.startswith(msys_prefix):
            continue
        msys_file = file
        dest_file = basedir + file[len(msys_prefix):]
        if os.path.isdir(msys_file) and not os.path.exists(dest_file):
            os.makedirs(dest_file)
        elif os.path.isfile(msys_file):
            shutil.copy2(msys_file, dest_file)

status_msg("Deploying application files")
shutil.copy2(gsshvnc_exe, os.path.join(basedir, 'bin', os.path.basename(gsshvnc_exe)))
shutil.copy2(os.path.join(basedir, os.pardir, 'COPYING'),
             os.path.join(basedir, 'COPYING'))

status_msg("Compiling GLib schemas")
proc = subprocess.Popen(['glib-compile-schemas.exe',
                         os.path.join(basedir, 'share', 'glib-2.0', 'schemas')])
proc.communicate()
if proc.returncode != 0:
    sys.exit(proc.returncode)

status_msg("Cleaning unnecessary files and directories")
for pat in clean_patterns:
    print('  - ' + pat)
    if '*' in pat:
        for file in glob.glob(os.path.join(basedir, pat)):
            win32_remove(file)
    else:
        win32_remove(os.path.join(basedir, pat))

remove_empty_dirs(basedir)

status_msg("Writing installer file list")
with open(os.path.join(basedir, 'filelist.iss'), 'w') as listfile:
    print('Source: "COPYING"; DestDir: "{{app}}\\COPYING"; Flags: ignoreversion',
          file=listfile)
    for rdir in release_dirs:
        for root, dirs, files in os.walk(os.path.join(basedir, rdir)):
            winroot = root.replace(basedir + os.sep, '').replace('/', '\\')
            for file in files:
                print('Source: "{}"; DestDir: "{{app}}\\{}"; Flags: ignoreversion' \
                      .format(winroot + '\\' + file, winroot),
                      file=listfile)
