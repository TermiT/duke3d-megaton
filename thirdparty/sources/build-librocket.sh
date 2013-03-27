#!/usr/bin/env sh

source config.sh

pushd libRocket/Build || exit 1
rm -rf build
mkdir -p build
cd build
CMAKE_EXE_LINKER_FLAGS=-lz CMAKE_PREFIX_PATH=../../../freetype-$FTVER/prefix cmake .. -DBUILD_SHARED_LIBS=OFF
make
popd
