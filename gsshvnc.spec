Name:		gsshvnc
Version:	0.96
Release:	1%{?dist}
Summary:	Simple VNC client with built-in SSH tunneling

Group:		Applications/System
License:	GPLv2+
URL:		https://github.com/zrax/gsshvnc
Source0:	%{name}-%{version}.tar.gz

BuildRequires:	gcc-c++
BuildRequires:	meson >= 0.37
BuildRequires:	pkgconfig
BuildRequires:	pkgconfig(gvnc-1.0)
BuildRequires:	pkgconfig(gtk-vnc-2.0)
BuildRequires:	pkgconfig(gtkmm-3.0)
BuildRequires:	pkgconfig(libsecret-1)
BuildRequires:	pkgconfig(libssh)
#Requires:	

%description
Gsshvnc (pronounced "Gosh VNC") is a simple GTK3-based VNC client that can
(optionally) automatically establish and use an SSH tunnel to connect to a
VNC server.

%prep
%setup -q


%build
%meson
%meson_build


%install
%meson_install


%files
%license COPYING
%{_bindir}/gsshvnc
%{_datadir}/applications/*.desktop
%{_datadir}/pixmaps/*.png


%changelog
* Wed Aug 28 2024 Michael Hansen <zrax0111@gmail.com> - 0.96
- Add support for SSH keyboard-interactive authentication.
- Port to GtkApplication.
- Remember previous VNC window size.
- Add .desktop file for better DE integration.

* Mon Aug 09 2021 Michael Hansen <zrax0111@gmail.com> - 0.95
- Add support for keeping aspect ratio when scaling the display.
- Add a resize mode which will resize the remote display when possible.
- Fix for gsshvnc disappearing from the task bar while still running.
- Don't give up after 3 failed connection attempts.

* Tue Dec 04 2018 Michael Hansen <zrax0111@gmail.com> - 0.94
- Add support for securely saving passwords.
- Show SSH Tunnel host in the main VNC window title.
- Fix the formatting on some dialog boxes.
