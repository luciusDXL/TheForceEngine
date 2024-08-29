#!/bin/bash

# This is the script physfs-buildbot.icculus.org uses to cross-compile
#  PhysicsFS from x86 Linux to Raspberry Pi. This script was originally from
#  Simple Directmedia Layer ( https://www.libsdl.org/ ).

# The final tarball can be unpacked in the root directory of a RPi,
#  so the PhysicsFS install lands in /usr/local. Run ldconfig, and then
#  you should be able to build and run PhysicsFS-based software on your
#  Pi. Standard configure scripts should be able to find PhysicsFS and
#  build against it.

TARBALL="$1"
if [ -z $1 ]; then
    TARBALL=physfs-raspberrypi.tar.xz
fi

BUILDBOTDIR="buildbot"
PARENTDIR="$PWD"

set -e
set -x
rm -f $TARBALL
rm -rf $BUILDBOTDIR
mkdir -p $BUILDBOTDIR
pushd $BUILDBOTDIR

# the '-G "Ninja"' can be '-G "Unix Makefiles"' if you prefer to use GNU Make.
SYSROOT="/opt/rpi-sysroot"
cmake -G "Ninja" \
    -DCMAKE_C_COMPILER="/opt/rpi-tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_SYSROOT="$SYSROOT" \
    -DCMAKE_FIND_ROOT_PATH="$SYSROOT" \
    -DCMAKE_SYSTEM_NAME="Linux" \
    -DCMAKE_SYSTEM_VERSION=1 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    ..

cmake --build . --config MinSizeRel

rm -rf "$TARBALL" physfs-raspberrypi
mkdir -p physfs-raspberrypi
echo "Archiving to '$TARBALL' ..."
cp -a ../src/physfs.h libphysfs.a libphysfs.so* physfs-raspberrypi
chmod -R a+r physfs-raspberrypi
chmod a+x physfs-raspberrypi physfs-raspberrypi/*.so*
chmod -R go-w physfs-raspberrypi
tar -cJvvf "$TARBALL" physfs-raspberrypi

set +x
echo "Done."


