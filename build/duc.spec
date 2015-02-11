%global name duc
%global version 1.0
%global release 1

Name:           %{name}
Version:        %{version}
Release:        %{release}%{?dist}
Summary:        Duc, a library and suite of tools for inspecting disk usage

License:        GNU General Public License
URL:            https://github.com/zevv/duc
Source0:        https://github.com/zevv/duc/archive/master.tar.gz

BuildArch:      x86_64
BuildRequires:  libtool autoconf
BuildRequires:  pango-devel cairo-devel tokyocabinet-devel

%description
Duc is a small library and a collection of tools for inspecting and visualizing
disk usage.
Duc maintains a database of accumulated sizes of directories of your file system,
and allows you to query this database with some tools,
or create fancy graphs showing you where your bytes are.

%prep
%setup -n %{name}-master

%build
autoreconf --install
./configure --libdir=%{_libdir} --bindir=%{_bindir}
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%post
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_libdir}/*
%{_bindir}/*

%changelog
* Wed Feb 11 2015 Edoardo Spadoni <edoardo.spadoni@nethesis.it> - 1.0
- first version
