%define version		0.6.5

%define name		jigdo
%define release		0
%define title		Jigdo
%define summary		Jigsaw Download
%define longtitle	%{summary}
%define group		Networking/File transfer
%define section		%{group}
%define icon		jigdo.png

%define source_extension bz2

# To have the RPM be buildabable with Mandrake features even on 
# non-Mandrake PCs define some Mandrake "system" defines
# Menu directories
%if !%(test "%{?_menudir:1}" = 1 && echo 1 || echo 0)
	%define _menudir %{_libdir}/menu
%endif
%if !%(test "%{?_iconsdir:1}" = 1 && echo 1 || echo 0)
	%define _iconsdir %{_datadir}/icons
	# If iconsdir is not defined, micons and licons
	# surely aren't as well
	%define _miconsdir %{_datadir}/icons/mini
	%define _liconsdir %{_datadir}/icons/large
%endif

# If _update_menus_bin is not defined, update_menus
# and clean_menaus aren't as well - I hope ;)
%if !%(test "%{?_update_menus_bin:1}" = 1 && echo 1 || echo 0)
	%define _update_menus_bin %{_bindir}/update-menus
	# Update Menu
	%define update_menus if [ -x %{_update_menus_bin} ]; then %{_update_menus_bin} || true ; fi

	# Clean Menu
	%define clean_menus if [ "$1" = "0" -a -x %{_update_menus_bin} ]; then %{_update_menus_bin} || true ; fi
%endif

# make
%if !%(test "%{?_smp_mflags:1}" = 1 && echo 1 || echo 0)
	%define _smp_mflags %([ -z "$RPM_BUILD_NCPUS" ] && RPM_BUILD_NCPUS="`/usr/bin/getconf _NPROCESSORS_ONLN`"; [ "$RPM_BUILD_NCPUS" -gt 1 ] && echo "-j$RPM_BUILD_NCPUS")
%endif
%if !%(test "%{?_make_bin:1}" = 1 && echo 1 || echo 0)
	%define _make_bin make
%endif
%if !%(test "%{?make:1}" = 1 && echo 1 || echo 0)
	%define make %{_make_bin} %_smp_mflags
%endif

%if !%(test "%{?makeinstall_std:1}" = 1 && echo 1 || echo 0)
	%define makeinstall_std make DESTDIR=%{?buildroot:%{buildroot}} install
%endif

Summary:	%{summary}
Name:		%{name}
Version:	%{version}
Release:	%{release}
Group:		%{group}
URL:		http://atterer.net/jigdo/
Source:		http://home.in.tum.de/atterer/jigdo/%{name}-%{version}.tar.%{source_extension}

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot
License:	GPL
Requires:	common-licenses
BuildRequires:	libdb3.3-devel, w3c-libwww-devel, libopenssl0-devel
Requires:	libdb3.3      , w3c-libwww      , libopenssl0
BuildRequires:	mawk, ImageMagick

%description
Jigsaw Download, or short jigdo, is an intelligent tool that can be used on the
pieces of any chopped-up big file to create a special "template" file which
makes reassembly of the file very easy for users who only have the pieces.

What makes jigdo special is that there are no restrictions on what
offsets/sizes the individual pieces have in the original big image. This makes
the program very well suited for distributing CD/DVD images (or large zip/tar
archives) because you can put the files on the CD on an FTP server - when jigdo
is presented the files along with the template you generated, it is able to
recreate the CD image.

%prep
%setup -q

%build
%configure
%make

%install
rm -rf %{buildroot}
%makeinstall_std

# Mandrake menu stuff
mkdir -p %{buildroot}%{_menudir}
cat > %buildroot%{_menudir}/%{name} << EOF
?package(%{name}): \
    command="%{_bindir}/%{name}" \
    title="%{title}" \
    longtitle="%{longtitle}" \
    section="%{section}" \
    icon="%{icon}" \
    needs="x11"
EOF

# Mandrake menu icons
mkdir -p %{buildroot}{%{_liconsdir},%{_iconsdir},%{_miconsdir}}
convert gfx/jigdo-icon.png -geometry 48 %{buildroot}%{_liconsdir}/%{icon}
convert gfx/jigdo-icon.png -geometry 32 %{buildroot}%{_iconsdir}/%{icon}
convert gfx/jigdo-icon.png -geometry 16 %{buildroot}%{_miconsdir}/%{icon}

%clean
rm -rf %{buildroot}

%post
# Update the Mandrake menus
# This will only execute, if %{_update_menus_bin} (see above)
# is executable
%{update_menus}

%postun
# Update the Mandrake menus
# This will only execute, if %{_update_menus_bin} (see above)
# is executable
%{clean_menus}

%files
%defattr(-,root,root)
%doc README VERSION doc/jigdo-file.* doc/TechDetails.txt
%{_bindir}/%{name}*
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/*
%{_mandir}/man1/%{name}*
%{_menudir}/%{name}
%{_liconsdir}/%{icon}
%{_iconsdir}/%{icon}
%{_miconsdir}/%{icon}

%changelog
* Mon Jan 26 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-4mdk
- Icons will be provided in the gfx subdirectory of the tarball

* Sun Jan 25 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-3mdk
- Jigdo compiles with gcc 2.96 now
- Only re-define the macros if they aren't yet defined

* Sat Jan 24 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-2mdk
- Make the SPEC be generic, so that it can be built on non-Mandrake
  machines

* Sat Jan 24 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.2-1mdk
- 0.6.2
- Remove patch1 - merged upstream

* Tue Jan 22 2002 Alexander Skwar <ASkwar@DigitalProjects.com> 0.6.1-1mdk
- First Mandrake release

