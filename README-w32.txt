# README-w32.txt $Revision: 1.1.2.1 $ $Date: 2002-05-11 00:47:50 $

                S Y L P H E E D   -   C L A W S   /   W I N 3 2
               ==================================================

These are the necessary steps to compile sylpheed-claws using Microsofts
VisualC++ 6.0 in a Win32 environment.
Please note, that these ports are in a very early development state:

 !     MANY FEATURES ARE UNAVAILABE, MAY CRASH OR EVEN DESTROY YOUR DATA     !
 !        PLEASE USE THESE PROGRAMS ONLY FOR TESTING OR DEVELOPMENT          !
 !            IF YOU USE THESE PROGRAMS, BE SURE TO MAKE BACKUPS             !

The main part of porting Sylpheed to Windows was done by Munesato Nakada
<munesato@post.co.jp>. This english installation help and the Sylpheed-Claws
port are derived from his work by Thorsten Maerz <info@netztorte.de>.


sylpheed-claws win32 branch:
----------------------------
This package contains the MSVC project files and patches for Sylpheed-claws /
Win32. Create the directory structure and install the libraries as shown
below. Then copy or checkout the sylpheed-claws sources.
-> Finally, apply the patches using "patch_claws.bat".


Get following packages:
-----------------------
All packages are mirrored at the Sylpheed-Claws homepage. Please refer
to the link section below.

* NEW MSVC project:
    sylpheed-claws-dev          (sylpheed-claws-dev-20020424.lzh)
* Gtk+ libraries:
    glib-dev                    (glib-dev-2.0.0-20020310.zip)
    gtk+-dev                    (gtk+-dev-1.3.0-20020310.zip)
    libiconv-dev                (libiconv-dev-1.7.zip)
    libintl                     (libintl-0.10.40-20020101.zip)
* Sylpheed sourcecode:
    sylpheed-sources            (sylpheed-0.7.4.tar.gz or cvs checkout)
* Support libraries:
    libjconv                    (libjconv-2.8.1.tar.gz)
    fnmatch                     (fnmatch-dev-20020306.lzh)
    libcompface                 (libcompface-dev-20020306.lzh)
    libkcc                      (libkcc-dev-20020306.lzh)
    regex                       (regex-dev-20020306.lzh)
    w32lib                      (w32lib-dev-20020323.lzh)
    gpgme                       (gpgme-dev-20020423.lzh)
    openssl                     (claws_w32_ssl_dev_020415.zip)

Create the source tree:
-----------------------

1. Create \dev
2. Extract glib-dev, gtk+-dev, libiconv-dev, libintl
   directly into \dev (this creates \dev\lib, \dev\include, etc.)
3. Extract the sylpheed-sources into \dev\lib\proj\sylpheed
4. Extract sylpeed-dev-new into \dev\proj\sylpheeed
5. Extract fnmatch, libcompface, libkcc, regex, w32lib
   to \dev\proj (creates \dev\proj\fnmatch, etc.)
6. Extract libjconv to \dev\proj\sylpheed.
7. Apply the patches using or "patch_claws.bat"


The resulting directory tree:
-----------------------------

\-dev                     (Gtk+ libs below \dev)
  +---bin
  +---include
  |   +---openssl         (extract openssl-dev to \ )
  |   +---glib-2.0
  |   +---gtk
  |   \---gdk
  +---lib
  |   +---glib-2.0
  |   +---pkgconfig
  |   \---gtk+
  +---share
  +---man
  +---doc
  \---proj                 (other libs and sylpheed below \dev\proj\)
      +---fnmatch
      |   \---src
      +---libcompface
      |   \---src
      +---libkcc
      +---regex
      |   \---src
      +---w32lib
      |   \---src
      +---gpgme
      \---sylpheed         (MSVC project below \dev\proj\sylpheed)
          +---ac
          +---faq
          +---intl
          +---libkcc      (This is from Sylpheed)
          +---libjconv    (The only support-lib installed here)
          +---manual
          +---po
          +---src
          +---files
          \---Debug
  
Hints:
------
* There are several files, that cant be build on Windows easily, as you
  need many unix utilities like autoconf, bison, etc. The necessary
  sources are provided in "src\generated.diff", so be sure to apply it.
* Ready compiled translations (and a sample gtk config) are available
  in "files\locale" and "files\etc". To compile translations on your own,
  you need either cygwin or an unix like os, and convert the source
  (.po) files to utf8 before compiling.
* libjconv and glib need patches, when compiled from source. As libjconv
  is rather small, the patched version is provided with the source.
  The patch for glib (2.0.0 and 2.0.1) is available in "files".
 

Links:
------
Sylpheed-Claws for Win32:
by Thorsten Maerz <torte@netztorte.de>
http://netztorte.de/sylpheed

Sylpheed forWin32 :
by Munesato Nakada (NAK) <munesato@post.co.jp>
http://www2.odn.ne.jp/munesato/sylpheed

Original Sylpheed :
by Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>
http://sylpheed.good-day.net

Sylpheed-Claws homepage :
http://sylpheed-claws.sourceforge.net

Gtk+ / Win32 homepage :
by Tor Lillqvist (tml) <tml@iki.fi>
http://www.gimp.org/~tml/gimp/win32

