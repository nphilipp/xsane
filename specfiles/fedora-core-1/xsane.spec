%define	desktop_vendor 	endur
%define name  		xsane
%define version		0.93
%define release 	1


Summary: An X Window System front-end for the SANE scanner interface.
Name: %{name}
Version: %{version}
Release: %{release}
Source: http://www.xsane.org/download/%{name}-%{version}.tar.gz
URL: http://www.xsane.org/
License: GPL
Group: Applications/Multimedia
BuildPrereq: sane-backends-devel, gimp-devel
BuildPrereq: libpng-devel, libjpeg-devel
BuildRequires: desktop-file-utils >= 0.2.92
ExcludeArch: s390 s390x

Buildroot: %{_tmppath}/%{name}-%{version}-buildroot

%description
XSane is an X based interface for the SANE (Scanner Access Now Easy)
library, which provides access to scanners, digital cameras, and other
capture devices. XSane is written in GTK+ and provides control for
performing the scan and then manipulating the captured image.

%package gimp
Summary: A GIMP plug-in which provides the SANE scanner interface.
Group: Applications/Multimedia
Prereq: /usr/bin/awk sh-utils fileutils

%description gimp
This package provides the regular XSane frontend for the SANE scanner
interface, but it works as a GIMP plug-in. You must have GIMP
installed to use this package.

%prep
rm -rf %{buildroot}
%setup -q


%build
%{expand:%%define optflags %{optflags} -DGIMP_ENABLE_COMPAT_CRUFT=1}
%configure --with-install-root=%{buildroot}
make %{?_smp_mflags}

mv src/xsane src/xsane-gimp

make clean
%configure --with-install-root=%{buildroot} --disable-gimp
make %{?_smp_mflags}

%install
rm -rf %{buildroot}

%makeinstall
install src/xsane-gimp %{buildroot}%{_bindir}
mkdir -p %{buildroot}/etc/X11/applnk/Graphics

cat > %{name}.desktop << EOF
[Desktop Entry]
Encoding=UTF-8
Name=Scanning
Name[sv]=XSane
Comment=XSane
Type=Application
Description=A GIMP plugin which provides a scanner interface.
Description[de]=Ein GIMP-Plugin zum Scannen.
Description[sv]=Ett GIMP-insticksprogram som erbjuder ett bildläsargränssnitt.
Exec=%{name}
Icon=/usr/share/pixmaps/sane.png
EOF
mkdir %{buildroot}%{_datadir}/applications
desktop-file-install --vendor %{desktop_vendor} --delete-original \
  --dir %{buildroot}%{_datadir}/applications                      \
  --add-category X-Red-Hat-Base                                   \
  --add-category Graphics                                         \
  --add-category Application                                      \
  %{name}.desktop

%find_lang %{name}

%clean
rm -rf %{buildroot}

%files -f %{name}.lang
%defattr(-,root,root)
%{_bindir}/xsane
%{_datadir}/sane
%{_datadir}/applications/*
%{_mandir}/man1/*
%doc %{name}.[A-Z]*

%files gimp
%defattr(-,root,root)
%{_bindir}/xsane-gimp

%post gimp
if [ -x /usr/bin/gimp-config ]; then
   GIMPPLUGINDIR=`/usr/bin/gimp-config --gimpplugindir`
elif [ -x /usr/bin/gimptool-* ]; then
   GIMPPLUGINDIR=`/usr/bin/gimptool-* --gimpplugindir`
fi
if [ -z "$GIMPPLUGINDIR" ]; then
  GIMPPLUGINDIR=%{_libdir}/gimp/2.0
fi
RELPATH=`echo $GIMPPLUGINDIR | awk '
  BEGIN { FS="/"; i = 1} {while (i < NF) { printf("../"); i = i + 1} }'`
if [ ! -s $GIMPPLUGINDIR/plug-ins/xsane ]; then
  ln -s $RELPATH/bin/xsane-gimp $GIMPPLUGINDIR/plug-ins/xsane
fi

%postun gimp
if [ $1 = 0 ]; then
  rm -f %{_libdir}/gimp/2.0/plug-ins/xsane
  rm -f %{_libdir}/gimp/1.3/plug-ins/xsane
  rm -f %{_libdir}/gimp/1.2/plug-ins/xsane
  rm -f %{_libdir}/gimp/1.1/plug-ins/xsane
  rm -f %{_libdir}/gimp/1.0/plug-ins/xsane
fi


%changelog
* Fri May 07 2004 Matthias Haase <matthias_haase@bennewitz.com>
- Update to 0.93
- Patch for gimp2 removed

* Sat Apr 04 2004 Matthias Haase <matthias_haase@bennewitz.com>
- Rebuild for Fedora Core 1 with patch for gimp-plugin (Gimp 2.0)
- Some minor changes for Gimp 2.0, matches now the "own style"

* Sun Nov 09 2003 Matthias Haase <matthias_haase@bennewitz.com>
- Rebuild for Fedora Core 1

* Sat Aug 30 2003 Matthias Haase <matthias_haase@bennewitz.com>
- Update to 0.92

* Thu May 09 2003 Matthias Haase <matthias_haase@bennewitz.com> 0.91-1
- Rebuild for 0.91

* Sat Apr 09 2003 Matthias Haase <matthias_haase@bennewitz.com> 0.90-1
- Rebuild of 0.90 for RH9

* Wed Dec 18 2002 Matthias Haase <matthias_haase@bennewitz.com> 0.90-1
- Rebuild for 0.90

* Tue Dec 03 2002 Matthias Haase <matthias_haase@bennewitz.com> 0.89-1
- smp flag added

* Sat Oct 26 2002 Matthias Haase <matthias_haase@bennewitz.com> 0.89-1
- Rebuild for xsane 0.89, internationalization macro and a short
  German description for the desktop link added 

* Fri Aug 30 2002 Tim Waugh <twaugh@redhat.com> 0.84-8
- Don't require gimp-devel (cf. bug #70754).

* Tue Jul 23 2002 Tim Waugh <twaugh@redhat.com> 0.84-7
- Desktop file fixes (bug #69555).

* Mon Jul 15 2002 Tim Waugh <twaugh@redhat.com> 0.84-6
- Use desktop-file-install.

* Fri Jun 21 2002 Tim Powers <timp@redhat.com> 0.84-5
- automated rebuild

* Wed Jun 12 2002 Tim Waugh <twaugh@redhat.com> 0.84-4
- Rebuild to fix bug #66132.

* Thu May 23 2002 Tim Powers <timp@redhat.com> 0.84-3
- automated rebuild

* Thu Feb 21 2002 Tim Waugh <twaugh@redhat.com> 0.84-2
- Rebuild in new environment.

* Wed Jan 23 2002 Tim Waugh <twaugh@redhat.com> 0.84-1
- 0.84.
- Remove explicit sane-backends dependency, since it is automatically
  found by rpm.

* Wed Jan 09 2002 Tim Powers <timp@redhat.com> 0.83-2
- automated rebuild

* Tue Jan  8 2002 Tim Waugh <twaugh@redhat.com> 0.83-1
- 0.83.

* Tue Dec 11 2001 Tim Waugh <twaugh@redhat.com> 0.82-3.1
- 0.82.
- Some extra patches from Oliver Rauch.
- Require sane not sane-backends since it's available throughout 7.x.
- Built for Red Hat Linux 7.1, 7.2.

* Tue Jul 24 2001 Tim Waugh <twaugh@redhat.com> 0.77-4
- Build requires libpng-devel, libjpeg-devel (#bug 49760).

* Tue Jul 17 2001 Preston Brown <pbrown@redhat.com> 0.77-3
- add an icon to the desktop entry

* Tue Jun 19 2001 Florian La Roche <Florian.LaRoche@redhat.de>
- add ExcludeArch: s390 s390x

* Mon Jun 11 2001 Tim Waugh <twaugh@redhat.com> 0.77-1
- 0.77.

* Sun Jun  3 2001 Tim Waugh <twaugh@redhat.com> 0.76-2
- Require sane-backends, not all of sane.

* Wed May 23 2001 Tim Waugh <twaugh@redhat.com> 0.76-1
- 0.76.

* Thu May  3 2001 Tim Waugh <twaugh@redhat.com> 0.75-1
- 0.75
- Fix summary/description to match specspo.

* Mon Jan  8 2001 Matt Wilson <msw@redhat.com>
- fix post script of gimp subpackage to install into the correct location

* Mon Dec 25 2000 Matt Wilson <msw@redhat.com>
- rebuilt against gimp 1.2.0

* Thu Dec 21 2000 Matt Wilson <msw@redhat.com>
- rebuilt against gimp 1.1.32
- use -DGIMP_ENABLE_COMPAT_CRUFT=1 to build with compat macros

* Thu Oct 12 2000 Than Ngo <than@redhat.com>
- 0.62

* Wed Aug 23 2000 Matt Wilson <msw@redhat.com>
- rebuilt against gimp-1.1.25

* Mon Aug 07 2000 Than Ngo <than@redhat.de>
- added swedish translation (Bug #15316)

* Fri Aug 4 2000 Than Ngo <than@redhat.de>
- fix, shows error dialogbox if no scanner exists (Bug #15445)
- update to 0.61

* Wed Aug  2 2000 Matt Wilson <msw@redhat.com>
- rebuilt against new libpng

* Thu Jul 13 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Mon Jul  3 2000 Matt Wilson <msw@redhat.com>
- rebuilt against gimp 1.1.24
- make clean before building non gimp version

* Fri Jun 30 2000 Preston Brown <pbrown@redhat.com>
- made gimp subpkg

* Wed Jun 14 2000 Preston Brown <pbrown@redhat.com>
- desktop entry added

* Tue Jun 13 2000 Preston Brown <pbrown@redhat.com>
- fixed gimp link
- FHS paths

* Tue May 30 2000 Karsten Hopp <karsten@redhat.de>
- update to 0.59

* Sat Jan 29 2000 TIm Powers <timp@redhat.com>
- fixed bug 8948

* Thu Dec 2 1999 Tim Powers <timp@redhat.com>
- updated to 0.47
- gzip man pages

* Mon Aug 30 1999 Tim Powers <timp@redhat.com>
- changed group

* Mon Jul 26 1999 Tim Powers <timp@redhat.com>
- update to 0.30
- added %defattr
- built for 6.1

* Thu Apr 22 1999 Preston Brown <pbrown@redhat.com>
- initial RPM for PowerTools 6.0
