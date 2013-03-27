#!/usr/bin/env sh

source config.sh
realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
    
PREFIX=$(realpath SDL-$SDLVER/prefix)

pushd SDL-$SDLVER || exit 1
make clean
rm -rf build/
./configure --prefix=$PREFIX --disable-cdrom --disable-video-x11 --disable-x11-shared --enable-shared=no CFLAGS="-arch i386 -arch x86_64"
make && make install
popd
