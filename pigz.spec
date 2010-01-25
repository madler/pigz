Summary: pigz is a parallel implementation of gzip which utilizes multiple cores
Name: pigz
Version: 2.1.6
Release: 1
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Tools
Packager: Duncan Brown <duncan@duncanbrown.org>
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
URL: http://www.zlib.net/pigz

%description
pigz, which stands for parallel implementation of gzip, is a fully functional replacement for gzip that exploits multiple processors and multiple cores to the hilt when compressing data. pigz was written by Mark Adler, and uses the zlib and pthread libraries.

%clean
rm -rf $RPM_BUILD_ROOT
%prep
mkdir -p $RPM_BUILD_ROOT
%setup -q
%build
make
mkdir -p ${RPM_BUILD_ROOT}/usr/bin
mv pigz ${RPM_BUILD_ROOT}/usr/bin
%files
%defattr(-,root,root)
/usr/bin/pigz
