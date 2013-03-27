#!/usr/bin/env sh

source config.sh

SRCDIR=smpeg/.libs
rm -f ../bin-osx32/libsmpeg.a
cp $SRCDIR/libsmpeg.a ../bin-osx32
