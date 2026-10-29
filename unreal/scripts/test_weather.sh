#!/bin/sh
# Headless in-game check of the weather engine (test_weather.py). ~1 min, -nullrhi, no window. Log: Saved/WeatherTest/game.log
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
APP="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p "$ROOT/Saved/WeatherTest"
LOG="$ROOT/Saved/WeatherTest/game.log"
"$APP" "$PROJ" /Engine/Maps/Entry -game -nullrhi -nosound -unattended -dpcvars=lyud.MainMenuOnStart=0 -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_weather.py" &
PID=$!
for i in $(seq 1 240); do kill -0 $PID 2>/dev/null || break; sleep 1; done
kill -0 $PID 2>/dev/null && { echo "watchdog: killing hung game"; kill -9 $PID; }
grep "WEATHERTEST" "$LOG" | sed 's/.*WEATHERTEST //'
grep -q "WEATHERTEST PASS" "$LOG"
