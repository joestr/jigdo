#! /bin/sh

# This script will download all necessary libraries to compile jigdo
# and jigdo-file under Windows, or to cross-compile it for
# Windows under Linux. The links are bound to become outdated over
# time, please send updates to the jigdo-user list.

date=050707
# Dest dir. WARNING, the dir will be deleted at the beginning of this
# script! Subdirs called lib, bin, ... will be created.
inst=~/samba/gtkwin-$date

# Should I attempt to grab libs for graphics etc.?
GET_GRAPHICS=0

# Dir for downloaded software tarballs
dl=~/samba/gtkwin-$date-dl

# Sourceforge mirror
sf=netcologne.dl.sourceforge.net

if test -f ~/.jigdo-win-lib-install; then
  . ~/.jigdo-win-lib-install
fi
#______________________________________________________________________

cmd() {
    echo "$@"
    "$@"
}

cmd rm -rf "$inst/"*
cmd mkdir -p "$inst"
cmd mkdir -p "$dl"
cd "$inst"

get() {
    file="$dl/`basename $1`"
    if test -f "$file"; then
        echo "$file"
    else
        cmd wget -nc --directory-prefix="$dl" "$1"
    fi
}

buntar() {
    echo buntar "$@"
    for f; do
	bzip2 -cd "$f" | tar -xf -
    done
}

unzip() {
    cmd /usr/bin/unzip -q -o "$@"
}
#______________________________________________________________________

# GTK+ for Windows
# http://www.gimp.org/~tml/gimp/win32/downloads.html

if [ $GET_GRAPHICS = 1 ] ; then

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/glib-2.6.5.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/glib-dev-2.6.5.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/gtk+-2.6.8.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/gtk+-dev-2.6.8.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/pango-1.8.0.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/pango-dev-1.8.0.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/atk-1.9.0.zip
    unzip "$file"

    get ftp://ftp.gtk.org/pub/gtk/v2.6/win32/atk-dev-1.9.0.zip
    unzip "$file"

    #______________________________________________________________________

    # Various dependencies
    # http://www.gimp.org/~tml/gimp/win32/downloads.html

    get http://$sf/sourceforge/gnuwin32/libpng-1.2.8-lib.zip
    unzip "$file"
    get http://$sf/sourceforge/gnuwin32/libpng-1.2.8-bin.zip
    unzip "$file"

    get http://www.gimp.org/~tml/gimp/win32/pkgconfig-0.15.zip
    unzip "$file"

    get http://www.gimp.org/~tml/gimp/win32/libiconv-1.9.1.bin.woe32.zip
    unzip "$file"

    get http://www.gimp.org/~tml/gimp/win32/gettext-runtime-0.13.1.zip
    unzip "$file"
fi

#get http://www.zlib.net/zlib122-dll.zip
#unzip "$file"
#cmd mv zlib1.dll bin/
#cp lib/zdll.lib lib/libz.a # allows -lz to be used for linking

get http://$sf/sourceforge/gnuwin32/bzip2-1.0.3-lib.zip
unzip "$file"
get http://$sf/sourceforge/gnuwin32/bzip2-1.0.3-bin.zip
unzip "$file"

get http://$sf/sourceforge/gnuwin32/zlib-1.2.3-lib.zip
unzip "$file"
get http://$sf/sourceforge/gnuwin32/zlib-1.2.3-bin.zip
unzip "$file"

#______________________________________________________________________

# Still missing, but not strictly needed ATM:
# - libdb

# http://curl.haxx.se/download.html
#get http://curl.haxx.se/download/curl-7.13.0-win32-ssl-devel-mingw32.zip
#unzip "$file"
#cmd mv libcurl.a libcurldll.a lib/
#cmd mv curl.exe libcurl.dll bin/

#unzip ~/samba/curl-7.14.0-win32-ssl-devel-mingw32msbitfields-ra050711.zip

# Needs zlibwapi.dll
#get http://curl.haxx.se/download/libcurl-7.14.0-win32-msvc.zip
#unzip "$file"
#cmd mv libcurl.lib lib/
#cmd mv libcurl.dll bin/

# OpenSSL for curl
# http://curl.haxx.se/download.html

# This one only has the DLL:
#get http://curl.haxx.se/download/openssl-0.9.7e-win32-bin.zip
#unzip "$file"
#cmd mv libeay32.dll libssl32.dll openssl.exe bin/

# This one also has the developer libs, headers etc.
get http://$sf/sourceforge/gnuwin32/openssl-0.9.7c-lib.zip
unzip "$file"
get http://$sf/sourceforge/gnuwin32/openssl-0.9.7c-bin.zip
unzip "$file"

# Cross-compile:
# sh go --with-libcurl='-lcurl -lssl -lcrypto -lzdll -lwinmm'
# make X=-DCURL_STATICLIB
