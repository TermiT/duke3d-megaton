#!/usr/bin/env sh

source config.sh

mkdir -p support/lib
rm support/lib/libfreetype.a
cp -f freetype-$FTVER/objs/.libs/libfreetype.a support/lib
rm -rf support/freetype
mkdir -p support/freetype
cp -r freetype-$FTVER/include support/freetype
