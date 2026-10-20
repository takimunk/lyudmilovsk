#!/bin/sh
# Opens the Unreal Editor for THIS checkout (main or a worktree) with its own Unreal MCP port.
#   tools/open_editor.sh <port>        e.g. 8000 main, 8001 character, 8002 props, 8003 aton, 8004 clerk
# Builds the C++ module first if this checkout has no binaries yet, and refuses to open a
# second editor on the same project folder (two editors on one folder overwrite each other).
set -e
PORT="${1:?usage: tools/open_editor.sh <mcp-port>}"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"

if pgrep -f "UnrealEditor.app/Contents/MacOS/UnrealEditor $PROJ" >/dev/null; then
    echo "An editor for $PROJ is already running:"; pgrep -fl "UnrealEditor $PROJ"; exit 1
fi
if lsof -nP -iTCP:"$PORT" -sTCP:LISTEN >/dev/null 2>&1; then
    echo "Port $PORT is already taken"; exit 1
fi
if [ ! -f "$ROOT/unreal/Lyudmilovsk/Binaries/Mac/UnrealEditor.modules" ] || [ "$BUILD" = 1 ]; then
    "$UE/Engine/Build/BatchFiles/Mac/Build.sh" LyudmilovskEditor Mac Development -Project="$PROJ" \
        -WaitMutex -NoHotReloadFromIDE
fi
nohup "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" \
    -ModelContextProtocolPort="$PORT" -log > "$ROOT/unreal/Lyudmilovsk/editor_$PORT.log" 2>&1 &
echo "editor pid $! for $PROJ, MCP http://localhost:$PORT/mcp"
