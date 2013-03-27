#!/usr/bin/env sh

source config.sh

pushd libvorbis-$LIBVORBISVER || exit 1
make clean
rm -rf build/
./configure --with-ogg=../libogg-$LIBOGGVER/prefix --enable-shared=no --enable-static=yes CFLAGS="-arch i386"
make
popd
