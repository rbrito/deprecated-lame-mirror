%define name lame
%define ver 3.86
%define quality alpha
%define rel 1
%define prefix /usr

Summary : LAME Ain't an MP3 Encoder
Name: %{name}
Version: %{ver}%{quality}
Release: %{rel}
Copyright: LGPL
Vendor: The LAME Project
Packager: cefiar <cefiar1@optushome.com.au>
URL: http://www.sulaco.org/mp3/
Group: Applications/Multimedia
Source: http://www.sulaco.org:80/mp3/download/beta/%{name}%{ver}%{quality}.tar.gz
BuildRoot: /var/tmp/lame-%{ver}-root
Prefix: %{prefix}
Docdir: %{prefix}/doc

%description
LAME is an educational tool to be used for learning about MP3 encoding.  The
goal of the LAME project is to use the open source model to improve the
psycho acoustics, noise shaping and speed of MP3.  Another goal of the LAME
project is to use these improvements for the basis of a  patent free audio
compression codec for the GNU project.

%prep

%setup -n %{name}%{ver}

%build
make

%install
rm -fr $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{prefix}/bin
mkdir -p $RPM_BUILD_ROOT%{prefix}/man/man1
install -s -m 755 lame $RPM_BUILD_ROOT%{prefix}/bin
install -m 644 doc/man/lame.1 $RPM_BUILD_ROOT%{prefix}/man/man1

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf /usr/src/redhat/BUILD/%{name}%{ver}

%files
%defattr (-,root,root)
%doc LICENSE USAGE COPYING API TODO README* HACKING
%doc doc/html/*.html
%{prefix}/bin/lame
%{prefix}/man/man1

%changelog

* Tue Aug 01 2000 Stuart Young «cefiar1@optushome.com.au»
- Updated RPM to 3.85beta.
- Modified spec file (merged George and Keitaro's specs)
- Added reasonable info to the specs to reflect the maintainer
- Renamed lame.spec (plain spec is bad, mmkay?).

* Fri Jun 30 2000 Keitaro Yosimura «ramsy@linux.or.jp»
- Updated RPM to 3.84alpha.
- Better attempt at an RPM, independant of 3.83 release.
- (This is all surmise as there was no changelog).

* Thu May 30 2000 Georges Seguin «crow@planete.net» 
- First RPM build around 3.83beta
