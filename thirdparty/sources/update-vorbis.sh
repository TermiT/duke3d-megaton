#!/usr/bin/env sh

source config.sh

SRCDIR=LIBVORBIS-$LIBVORBISVER/lib/.libs
rm -f ../bin-osx32/libvorbis.a ../bin-osx32/libvorbisenc.a ../bin-osx32/libvorbisfile.a
cp $SRCDIR/libvorbis.a $SRCDIR/libvorbisenc.a $SRCDIR/libvorbisfile.a ../bin-osx32
