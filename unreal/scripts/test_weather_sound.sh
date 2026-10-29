#!/bin/sh
# Offscreen play test of the weather's sound (test_weather_sound.py): never opens a window, leaves running editors alone, and
# plays nothing out loud (au.LinearGainScalarForFinalOutut=0 mutes the speakers after the mix the test records).
# Then: tools/audiosim/spectrogram.py unreal/Lyudmilovsk/Saved/WeatherSound/*.wav
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_weather_sound.log"
mkdir -p "$(dirname "$LOG")"
# at most two heavy UE runs at once (offscreen games, commandlets, builds) over all checkouts
heavy() {
    N=0
    for P in $(pgrep -x UnrealEditor) $(pgrep -x UnrealEditor-Cmd); do
        ps -o args= -p "$P" | grep -qE " -game| -run=" && N=$((N + 1))
    done
    echo $N
}
while [ "$(heavy)" -ge 2 ]; do sleep 15; done
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=640 -ResY=360 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0,au.LinearGainScalarForFinalOutut=0${DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_weather_sound.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "WSOUNDTEST" "$LOG" | sed 's/.*WSOUNDTEST/WSOUNDTEST/' || true
