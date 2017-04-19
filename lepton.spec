###############################################################################

%define _posixroot        /
%define _root             /root
%define _bin              /bin
%define _sbin             /sbin
%define _srv              /srv
%define _home             /home
%define _lib32            %{_posixroot}lib
%define _lib64            %{_posixroot}lib64
%define _libdir32         %{_prefix}%{_lib32}
%define _libdir64         %{_prefix}%{_lib64}
%define _logdir           %{_localstatedir}/log
%define _rundir           %{_localstatedir}/run
%define _lockdir          %{_localstatedir}/lock/subsys
%define _cachedir         %{_localstatedir}/cache
%define _spooldir         %{_localstatedir}/spool
%define _crondir          %{_sysconfdir}/cron.d
%define _loc_prefix       %{_prefix}/local
%define _loc_exec_prefix  %{_loc_prefix}
%define _loc_bindir       %{_loc_exec_prefix}/bin
%define _loc_libdir       %{_loc_exec_prefix}/%{_lib}
%define _loc_libdir32     %{_loc_exec_prefix}/%{_lib32}
%define _loc_libdir64     %{_loc_exec_prefix}/%{_lib64}
%define _loc_libexecdir   %{_loc_exec_prefix}/libexec
%define _loc_sbindir      %{_loc_exec_prefix}/sbin
%define _loc_bindir       %{_loc_exec_prefix}/bin
%define _loc_datarootdir  %{_loc_prefix}/share
%define _loc_includedir   %{_loc_prefix}/include
%define _loc_mandir       %{_loc_datarootdir}/man
%define _rpmstatedir      %{_sharedstatedir}/rpm-state
%define _pkgconfigdir     %{_libdir}/pkgconfig

###############################################################################

Summary:            Tool and file format for losslessly compressing JPEG
Name:               lepton
Version:            1.0
Release:            0%{?dist}
License:            APLv2
Group:              Applications/Multimedia
URL:                https://blogs.dropbox.com/tech/2016/07/lepton-image-compression-saving-22-losslessly-from-images-at-15mbs

Source0:            https://github.com/dropbox/%{name}/archive/%{version}.tar.gz

BuildRoot:          %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:      autoconf >= 2.50 automake make gcc-c++ >= 4.8.1 git

Provides:           %{name} = %{version}-%{release}

###############################################################################

%description
Lepton is a tool and file format for losslessly compressing JPEGs by an average 
of 22%. This can be used to archive large photo collections, or to serve images 
live and save 22% banwdith.

###############################################################################

%prep
%setup -q

%build
./autogen.sh
./configure

%{__make} %{?_smp_mflags}

%install
rm -rf %{buildroot}

install -dm 755 %{buildroot}%{_bindir}
install -pm 644 %{name} %{buildroot}%{_bindir}

%check
%{__make} check %{?_smp_mflags}

%clean
rm -rf %{buildroot}

###############################################################################

%files
%defattr(644,root,root,755)
%doc AUTHORS README.md license.txt 
%attr(755,root,root) %{_bindir}/%{name}

###############################################################################

%changelog
* Fri Jul 15 2016 Gleb Goncharov <inbox@gongled.ru> - 1.0-0
- Initial build
