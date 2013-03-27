#!/usr/bin/env sh

source config.sh

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

PREFIX=$(realpath libogg-$LIBOGGVER/prefix)
pushd libogg-$LIBOGGVER || exit 1
#rm src/.deps/*
#rm src/.libs/*
#rm prefix
./configure --prefix=$PREFIX --enable-shared=no --enable-static=yes CFLAGS="-arch i386"
make && make install
popd

