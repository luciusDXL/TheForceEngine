#!/bin/sh

if [ ! -f "./install_manifest.txt" ]; then
    echo "ERROR: This needs to be run from your CMake build directory after installing." 1>&2
    exit 1
fi

xargs rm -vf < install_manifest.txt
exit 0

