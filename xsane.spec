%define name xsane
%define version 0.76
%define release 1

Name: %{name}
Version: %{version}
Release: %{release}
Summary: XSane is a graphical frontend for SANE
Group: Graphics
URL: http://www.xsane.org
Buildroot: /var/tmp/%{name}-buildroot
Requires: sane
Source: ftp://ftp.mostang.com/pub/sane/xsane/%{name}-%{version}.tar.gz
Prefix: /usr
Copyright: GPL
BuildRequires: gtk+ >= 1.2.0 libsane1-devel >= 1.0.0

%description
XSane is a grahical scanning frontend for the SANE-library. It is based on gtk/X-Window.

%prep

%setup -q -n %{name}-%{version}

%build

CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=$RPM_BUILD_ROOT%{prefix}
uname -a | grep -qi SMP && make -j 2 || make

%install
make install prefix=$RPM_BUILD_ROOT%{prefix}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/share/sane
/usr/bin/*
/usr/man/*
%doc xsane*

