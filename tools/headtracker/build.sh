#!/bin/sh
# Builds the AirPods head-tracking helper (main.swift) into unreal/Lyudmilovsk/Saved/HeadTracker/LyudHeadTracker.app,
# where the game looks for it. Mac only; needs the Xcode command line tools.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$HERE/../../unreal/Lyudmilovsk/Saved/HeadTracker/LyudHeadTracker.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
cp "$HERE/Info.plist" "$APP/Contents/Info.plist"
swiftc -O -o "$APP/Contents/MacOS/LyudHeadTracker" "$HERE/main.swift" -framework CoreMotion
codesign --force --sign - "$APP"
echo "$APP"
