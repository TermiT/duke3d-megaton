#!/usr/bin/env sh

source config.sh

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

PREFIX=$(realpath freetype-$FTVER/prefix)

pushd freetype-$FTVER || exit 1
rm objs/*
rm objs/.libs/*
rm installroot
./configure CFLAGS="-arch i386 -arch x86_64" --enable-shared=no --enable-static=yes --without-bzip2 --without-zlib --prefix=$PREFIX
make && make install
popd

