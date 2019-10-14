%global name duc
%global version 1.4.4
%global release 1

Name:           %{name}
Version:        %{version}
Release:        %{release}%{?dist}
Summary:        Duc, a library and suite of tools for inspecting disk usage

License:        GNU General Public License
URL:            https://github.com/zevv/duc
Source0:        https://github.com/zevv/duc/releases/download/%{version}/duc-%{version}.tar.gz

BuildArch:      x86_64
BuildRequires:  libtool autoconf
BuildRequires:  pango-devel cairo-devel tokyocabinet-devel ncurses-devel

%description
Duc is a small library and a collection of tools for inspecting and visualizing
disk usage.
Duc maintains a database of accumulated sizes of directories of your file system,
and allows you to query this database with some tools,
or create fancy graphs showing you where your bytes are.

%prep
%setup

%build
autoreconf --install
./configure --libdir=%{_libdir} --bindir=%{_bindir} --mandir=%{_mandir}
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%post

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/%{name}
%{_mandir}/man1/%{name}.1.gz

%changelog
* Mon Oct 14 2019 Peter Cummuskey <peterc@aetheric.co.nz> - 1.4.4
- Update to 1.4.4

* Mon Jan 02 2017 Giacomo Sanchietti <giacomo.sanchietti@nethesis.it> - 1.4.3
- Update to 1.4.3

* Fri Dec 09 2016 Giacomo Sanchietti <giacomo.sanchietti@nethesis.it> - 1.4.2
- Update to 1.4.2
- Removed devel rpm
- Cleanup spec

* Mon Jun 29 2015 James Chang - 1.3.3
- added ncurses-devel package requirement
- fixed file not found for libs
* Wed Feb 11 2015 Edoardo Spadoni <edoardo.spadoni@nethesis.it> - 1.0
- first version
