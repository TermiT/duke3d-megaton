#!/usr/bin/env sh

source config.sh

rm -rf ../bin-osx32/RocketCore.framework
rm -rf ../bin-osx32/RocketControls.framework
rm -rf ../bin-osx32/RocketDebugger.framework
rm -f ../bin-osx32/libRocketCore.a
rm -f ../bin-osx32/libRocketControls.a
rm -f ../bin-osx32/libRocketDebugger.a

cp -r libRocket/Build/$LRCONFIG/* ../bin-osx32
cp -f libRocket/Build/build/libRocketCore.a ../bin-osx32
cp -f libRocket/Build/build/libRocketControls.a ../bin-osx32
cp -f libRocket/Build/build/libRocketDebugger.a ../bin-osx32
