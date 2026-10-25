#!/bin/sh
# An offscreen scripted flight of the player bird (UBirdFlightSubsystem's Bird.Script), one frame per 1/30 s of game time,
# joined into renders/bird_flight/<name>.mp4. Waits for other Unreal -game runs; never opens a window.
#   unreal/scripts/record_bird_flight.sh showcase[:Species] [name] [extra -ExecCmds before the script, ';'-separated] [map]
#   e.g. record_bird_flight.sh showcase:HerringGull gull     record_bird_flight.sh styles styles "lyud.Bird.Debug 1"
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
SCRIPT="${1:?script}"
NAME="${2:-$(echo "$SCRIPT" | tr ':' '_')}"
PRE="${3:-}"
MAP="${4:-/Game/Scene01/Lvl_Scene01}"
FRAMES="$ROOT/unreal/Lyudmilovsk/Saved/BirdFlight/$NAME"
OUT="$ROOT/renders/bird_flight"
rm -rf "$FRAMES"; mkdir -p "$FRAMES" "$OUT"
while pgrep -f "UnrealEditor.*-game" >/dev/null; do sleep 5; done
CMDS="Bird.Script $SCRIPT record=$FRAMES warm=${WARM:-75} quit"
[ -n "$PRE" ] && CMDS="$(echo "$PRE" | tr ';' ',')",$CMDS
"$UE" "$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" "$MAP" -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed \
    -dpcvars=lyud.MainMenuOnStart=0 -abslog="$FRAMES/run.log" "-ExecCmds=$CMDS" >/dev/null 2>&1 || true
git -C "$ROOT" checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
grep -E "BIRD|Bird flight|Error:|Ensure" "$FRAMES/run.log" | grep -v "LogInit: Display" | head -40
/Applications/Blender.app/Contents/MacOS/Blender -b --python "$ROOT/tools/frames_to_mp4.py" -- "$FRAMES" "$OUT/$NAME.mp4" 30 2>&1 | grep -E "MP4|Error" || true
