%define name lame
%define ver 3.85
%define quality beta
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
 -=- MIME -=- 
This is a multi-part message in MIME format.
--------------503F7E8F497F5FD136D70133
Content-Type: text/plain; charset=us-ascii
Content-Transfer-Encoding: 7bit

I have muddled about and created a working lame.spec file for rpm
creation (and rpms) for 3.85.

Havent got the space to host stuff anywhere at the moment, so here is
the .spec file (add the .spec to the CVS, may want to change the %ver
and %quality parameters to suit 3.86 and alpha - remember to change
these for each release! Wonder if there is some way to automate this in
CVS.. hmmm).

I'll send you a .src.rpm and a .i386.rpm (built for rh 6.x) as well if
you like? If you want those, just ask, or alternatively, supply me with
ftp/scp/whatever details to somewhere I can deposit the rpms, and I'll
get them online for you. What is the status of binary releases of lame
anyway?

PS: I'm going to try and tackle "auto" endian-ness for platforms in the
makefile - if it gets it wrong, you can always use '-x' to reverse the
endian-ness of lame anwyay. Now all I gotta do is sit down and fiddle
with CVS.... oh what fun!!! *grin*


--------------503F7E8F497F5FD136D70133
Content-Type: text/plain; charset=iso-8859-1;
 name="lame.spec"
Content-Transfer-Encoding: 8bit
Content-Disposition: inline;
 filename="lame.spec"

%define name lame
%define ver 3.85
%define quality beta
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

--------------503F7E8F497F5FD136D70133--


