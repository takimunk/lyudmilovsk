#!/bin/sh
# Imports the clerk's Excel loop and desk set (see import_clerk.py). Close this checkout's editor first.

UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
for pass in anim desk; do
    "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$HERE/../Lyudmilovsk/Lyudmilovsk.uproject" -run=pythonscript \
        -script="$HERE/import_clerk.py $pass" -unattended -nosplash -stdout | grep -E "CLERK|Python: Error" | grep -v LogInit
done
