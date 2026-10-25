#!/bin/sh
# Offscreen play test of the world's sound (test_audio.py): never opens a window, leaves running editors alone, and
# plays nothing out loud (au.LinearGainScalarForFinalOutut=0 mutes the speakers after the mix the test records).
# Then: tools/audiosim/spectrogram.py unreal/Lyudmilovsk/Saved/AudioTest/*.wav
set -e
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_audio.log"
mkdir -p "$(dirname "$LOG")"
"$ROOT/tools/wait_ue_queue.sh" 2>/dev/null || true
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/Scene01/Lvl_Scene01 -game -RenderOffscreen \
    -windowed -ResX=640 -ResY=360 -unattended -nosplash -dpcvars=lyud.MainMenuOnStart=0,au.LinearGainScalarForFinalOutut=0${DPCVARS} $EXTRA \
    -abslog="$LOG" "-ExecCmds=py $ROOT/unreal/scripts/test_audio.py" > "$LOG.stdout" 2>&1 &
PID=$!
( sleep "${TIMEOUT:-900}"; kill "$PID" 2>/dev/null ) &
DOG=$!
wait "$PID" || true
kill "$DOG" 2>/dev/null || true
grep -E "AUDIOTEST" "$LOG" | sed 's/.*AUDIOTEST/AUDIOTEST/' || true
