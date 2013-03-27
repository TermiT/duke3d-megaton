#!/usr/bin/env sh

source config.sh

SRCDIR=LIBOGG-$LIBOGGVER/src/.libs
rm -f ../bin-osx32/libogg.a
cp $SRCDIR/libogg.a ../bin-osx32
