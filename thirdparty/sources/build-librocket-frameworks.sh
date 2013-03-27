#!/usr/bin/env sh

source config.sh

pushd libRocket/Build || exit 1
rm -rf build Release Debug
xcodebuild -project Rocket.xcodeproj -target RocketCoreOSX -configuration $LRCONFIG
xcodebuild -project Rocket.xcodeproj -target RocketControlsOSX -configuration $LRCONFIG
xcodebuild -project Rocket.xcodeproj -target RocketDebuggerOSX -configuration $LRCONFIG
popd
