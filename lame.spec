%define ver	3.84
%define beta	alpha
%define prefix	/usr
%define rel	1
%define name	lame

Summary: LAME Ain't an MP3 Encoder
Name: %{name}
Version: %{ver}%{beta}
Release: %{rel}
Copyright: LGPL
Packager: Keitaro Yosimura <ramsy@linux.or.jp>
Group: Applications/Multimedia
Source: http://www.sulaco.org:80/mp3/download/beta/lame%{ver}%{beta}.tar.gz
BuildRoot: /var/tmp/lame-%{ver}-root
URL: http://www.sulaco.org/mp3/
Docdir: %{prefix}/doc
Prefix: %prefix

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
rm -rf /usr/src/redhat/BUILD/%{name}-%{ver}

%files
%defattr(-, root, root)

%doc LICENSE USAGE COPYING API TODO README* HACKING
%doc doc/html/*.html
%{prefix}/bin/lame
%{prefix}/man/man1
