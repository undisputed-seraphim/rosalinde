#!/usr/bin/env sh

BASEDIR=$(realpath $(dirname "$0"))
BUILDDIR="$BASEDIR/build"
rm -rf $BUILDDIR
cmake -B $BUILDDIR -S $BASEDIR
cmake --build $BUILDDIR --config Release -j 8
