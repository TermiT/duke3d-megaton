#!/usr/bin/env sh

source config.sh

pushd smpeg || exit 1*
make clean
#CCAS="gcc -m32" CC="gcc -m32" CXX="g++ -m32" ./configure --with-sdl-exec-prefix="../SDL-$SDLVER/prefix" --with-sdl-prefix="../SDL-$SDLVER/prefix" --without-x --disable-gtktest --disable-gtk-player --enable-shared=no --enable-static=yes CFLAGS="-m32" LDFLAGS="-m32"
sh ../build_smpeg_1.sh
make
popd
