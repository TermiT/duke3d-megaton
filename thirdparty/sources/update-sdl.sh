#!/usr/bin/env sh

source config.sh

SRCDIR=SDL-$SDLVER/build/.libs
rm -f ../bin-osx32/libSDL.a ../bin-osx32/libSDLmain.a
cp $SRCDIR/libSDL.a $SRCDIR/libSDLmain.a ../bin-osx32
