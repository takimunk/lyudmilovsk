#!/bin/sh
# What a v3 station hall costs, in one process (test_station_hall_perf.py; HALL=zenit HOUR=8 ROUNDS=2): frame time and
# memory with everything on, without the hall people, without point lights. Runs alone (wait_ue_queue --exclusive).
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_station_hall_perf.log"
SHOTS="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"

mkdir -p "$(dirname "$LOG")"
# the network resolved against this checkout's terrain (not committed)
[ -f "$ROOT/unreal/Lyudmilovsk/Content/Transport/network_built.json" ] || python3 "$ROOT/tools/transport.py" > /dev/null
sh "$ROOT/tools/wait_ue_queue.sh" 7200 --exclusive

SND="-nosound"; [ "$SOUND" = 1 ] && SND="-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1"
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=1600 -ResY=900 -unattended -nosplash $SND -dpcvars=lyud.MainMenuOnStart=0$DPC -ueexclusive $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_station_hall_perf.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-1200}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "HALLPERF|TRANSPORTTEST|STATIONPROBE.*(FAIL|done)|Transport:|LogScript.*Error|Error:.*Transport" "$LOG" | sed 's/.*Warning: //; s/.*LogTemp: Display: //' || true
