#!/bin/bash

if [ -z "$SDKDIR" ]; then
    SDKDIR="/emsdk"
fi

ENVSCRIPT="$SDKDIR/emsdk_env.sh"
if [ ! -f "$ENVSCRIPT" ]; then
   echo "ERROR: This script expects the Emscripten SDK to be in '$SDKDIR'." 1>&2
   echo "ERROR: Set the \$SDKDIR environment variable to override this." 1>&2
   exit 1
fi

TARBALL="$1"
if [ -z $1 ]; then
    TARBALL=physfs-emscripten.tar.xz
fi

cd `dirname "$0"`
cd ..
PHYSFSBASE=`pwd`

echo "Setting up Emscripten SDK environment..."
source "$ENVSCRIPT"

echo "Setting up..."
cd "$PHYSFSBASE"
rm -rf buildbot
mkdir buildbot
cd buildbot

echo "Configuring..."
emcmake cmake -G "Ninja" -DPHYSFS_BUILD_SHARED=False -DCMAKE_BUILD_TYPE=MinSizeRel .. || exit $?

echo "Building..."
emmake cmake --build . --config MinSizeRel || exit $?

set -e
rm -rf "$TARBALL" physfs-emscripten
mkdir -p physfs-emscripten
echo "Archiving to '$TARBALL' ..."
cp ../src/physfs.h libphysfs.a physfs-emscripten
chmod -R a+r physfs-emscripten
chmod a+x physfs-emscripten
chmod -R go-w physfs-emscripten
tar -cJvvf "$TARBALL" physfs-emscripten
echo "Done."

exit 0

# end of emscripten-buildbot.sh ...

