#!/bin/bash

# This is used by the buildbot to cross-compile for OS/2 from Linux, using
#  OpenWatcom. In an ideal world, we wouldn't need this, but we need a few
#  things to do this "properly" ...
#
#  - OS/2 running as a VMware guest on the build machine
#  - Buildbot-worker running on that OS/2 guest
#  - CMake for OS/2 that...
#  -  ... has Open Watcom compiler support and...
#  -  ... a Watcom WMake project generator.
#
# Some of these things are doable (there is a CMake port for OS/2, you can
#  use GCC with it), but since OpenWatcom will just target OS/2 on Linux just
#  like it could on OS/2, it's easier and more efficient to just have a
#  buildbot script compile it here.
#
# Note that we just blast all the C files through the wcc386 compiler and
#  skip CMake entirely. We should fix this at some point.

set -e

ZIPFILE="$1"
if [ -z $ZIPFILE ]; then
    ZIPFILE=physfs-os2.zip
fi

export WATCOM="/usr/local/share/watcom"
export PATH="$PATH:$WATCOM/binl"

CFLAGS="-i=\"$WATCOM/h;$WATCOM/h/os2;../src\" -wx -d0 -otexan -6r -zq -bt=os2 -fo=.obj -mf"
WLIBFLAGS="-b -c -n -q -p=512"

cd `dirname "$0"`
cd ..
mkdir -p buildbot
cd buildbot

rm -f test_physfs.obj

OKAY="1"
for src in ../src/*.c ; do
    echo wcc386 $src $CFLAGS
    wcc386 $src $CFLAGS || OKAY="0"
done

if [ "$OKAY" == "1" ]; then
    echo wlib $WLIBFLAGS physfs.lib *.obj
    wlib $WLIBFLAGS physfs.lib *.obj || OKAY="0"
fi

echo wcc386 ../test/test_physfs.c $CFLAGS
wcc386 ../test/test_physfs.c $CFLAGS || OKAY="0"

if [ "$OKAY" == "1" ]; then
    LDFLAGS="name test_physfs d all sys os2v2 op m libr physfs op q op symf FIL test_physfs.obj"
    echo wlink $LDFLAGS
    wlink $LDFLAGS || OKAY="0"
fi

if [ "$OKAY" == "1" ]; then
    F=`file test_physfs.exe`
    echo "$F"
    if [ "$F" != 'test_physfs.exe: MS-DOS executable, LX for OS/2 (console) i80386' ]; then
        echo 1>&2 "ERROR: final binary doesn't appear to be OS/2 executable."
        OKAY=0
    fi
fi

if [ "$OKAY" == "1" ]; then
    echo 1>&2 "Build succeeded."
    set -e
    rm -rf "$ZIPFILE" physfs-os2
    mkdir -p physfs-os2
    echo "Zipping to '$ZIPFILE' ..."
    cp ../src/physfs.h physfs.lib physfs-os2
    chmod -R a+r physfs-os2
    chmod a+x physfs-os2
    chmod -R go-w physfs-os2
    zip -9r "$ZIPFILE" physfs-os2
    echo "Done."
    exit 0
else
    echo 1>&2 "Build failed."
    exit 1
fi

