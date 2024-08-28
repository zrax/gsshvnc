Name:		gsshvnc
Version:	0.96
Release:	1%{?dist}
Summary:	A simple VNC client with built-in SSH tunneling

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
- Add support for SSH keyboard-interactive authentication
- Port to GtkApplication
- Remember previous VNC window size
- Add .desktop file for better DE integration
- Add native packaging for RPM-based distributions
