#!/usr/bin/env sh

source config.sh

rm -rf ../include/*
cp -r libRocket/Include/* ../include
cp -r SDL-$SDLVER/include/* ../include
mkdir -p ../include/ogg
cp -r libogg-$LIBOGGVER/include/ogg/*.h ../include/ogg/
mkdir -p ../include/vorbis
cp -r libvorbis-$LIBVORBISVER/include/vorbis/*.h ../include/vorbis/
mkdir -p ../include/smpeg
cp -r smpeg/*.h ../include/smpeg

