#!/bin/sh
# One MetaHuman from start to offscreen shots (unreal/scripts/metahuman_character.py, docs in its header).
#   MH_ID=nao MH_NAME=MHC_Nao_Aoi MH_PRESET=Aoi MH_PRE=hair tools/characters/mh_build.sh
# MH_PRE: steps run in the rendering editor before the cloud steps (conform needs nothing, hair needs Slate).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE="/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
LOG="${MH_LOGDIR:-$ROOT/unreal/Lyudmilovsk/Saved/Logs}"
export MH_ID MH_NAME MH_PRESET
editor() {  # steps in an offscreen rendering editor
    rm -f "$ROOT/unreal/Lyudmilovsk/Saved/Autosaves/PackageRestoreData.json"
    MH_QUIT=1 MH_STEPS="$1" "$UE/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -RenderOffscreen -nosplash -nosound \
        -abslog="$LOG/mh_$2.log" -ExecutePythonScript="$ROOT/unreal/scripts/metahuman_character.py" >/dev/null 2>&1 || true
    grep -h "LogPython.*MHC" "$LOG/mh_$2.log" | cut -c40-260
}
cmdlet() {  # cloud steps: a commandlet (the blocking cloud requests hang a full editor); no -unattended (login)
    rm -f "$ROOT/unreal/Lyudmilovsk/Saved/Autosaves/PackageRestoreData.json"
    MH_STEPS="$1" "$UE/UnrealEditor-Cmd" "$PROJ" -run=pythonscript -script="$ROOT/unreal/scripts/metahuman_character.py $MH_ID" \
        > "$LOG/mh_$2.log" 2>&1 || true
    grep -h "LogPython.*MHC\|Auto-Rigging" "$LOG/mh_$2.log" | cut -c40-260
}
[ -n "$MH_PRE" ] && editor "$MH_PRE" pre
cmdlet "${MH_CLOUD:-rig,textures}" cloud
editor build build
rm -f "$ROOT/unreal/Lyudmilovsk/Content/MetaHumans/L_MHShot.umap"
"$UE/UnrealEditor-Cmd" "$PROJ" -run=pythonscript -script="$ROOT/unreal/scripts/make_mh_shot_level.py" -unattended >/dev/null 2>&1
S="$ROOT/unreal/Lyudmilovsk/Saved/Screenshots/MacEditor"; mkdir -p "$S"; touch "$LOG/mh_mark"
MH_WARMUP=60 "$UE/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" /Game/MetaHumans/L_MHShot -game -RenderOffscreen -ResX=1024 \
    -ResY=1024 -windowed -nosound -dpcvars=lyud.MainMenuOnStart=0 -abslog="$LOG/mh_shot.log" \
    "-ExecCmds=py $ROOT/unreal/scripts/shoot_mh.py" >/dev/null 2>&1 || true
find "$S" -newer "$LOG/mh_mark" -name "*.png" | sort
