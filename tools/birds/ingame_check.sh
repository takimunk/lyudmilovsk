#!/bin/sh
# In-engine proof for imported birds: builds /Game/Dev/L_BirdSpecies with just these species (one commandlet), then
# shoots unreal/scripts/shoot_bird_check.py offscreen (one -game), each through tools/wait_ue_queue.sh, and puts a
# sheet per species in renders/birdmodels/ingame_<Id>.jpg.
#   tools/birds/ingame_check.sh Rook HerringGull
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
P="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
OUT="$ROOT/blender/birds/scan_raw/ingame"
mkdir -p "$OUT"
IDS=$(echo "$@" | tr ' ' ',')
tools/wait_ue_queue.sh 14400 > /dev/null
nice -n 10 "$UE" "$P" -run=pythonscript -script="$ROOT/unreal/scripts/make_bird_species_level.py $*" -unattended -nullrhi -nosplash -stdout \
    > "$OUT/make_level.log" 2>&1
grep -E "Error:|Traceback" "$OUT/make_level.log" | head -5
tools/wait_ue_queue.sh 14400 > /dev/null
BIRD_SHOTS="$OUT" BIRD_IDS="$IDS" nice -n 10 "$UE" "$P" /Game/Dev/L_BirdSpecies -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed \
    -dpcvars=lyud.MainMenuOnStart=0 -abslog="$OUT/shoot.log" "-ExecCmds=py $ROOT/unreal/scripts/shoot_bird_check.py" > /dev/null 2>&1
git checkout -q -- unreal/Lyudmilovsk/Lyudmilovsk.uproject 2>/dev/null || true
grep -E "BIRDCHECK|Error:|Traceback" "$OUT/shoot.log" | head -40
for s in "$@"; do
    python3 - "$OUT" "$s" "$ROOT/renders/birdmodels/ingame_$s.jpg" <<'EOF'
import glob, os, sys
from PIL import Image, ImageDraw
out, sid, dest = sys.argv[1:]
fs = sorted(glob.glob(os.path.join(out, sid + "_*.png")))
if not fs:
    sys.exit("no shots for " + sid)
cw, ch, cols = 480, 270, 4
rows = (len(fs) + cols - 1) // cols
sheet = Image.new("RGB", (cw * cols, ch * rows), "white")
d = ImageDraw.Draw(sheet)
for i, f in enumerate(fs):
    im = Image.open(f).convert("RGB")
    im.thumbnail((cw, ch))
    x, y = (i % cols) * cw, (i // cols) * ch
    sheet.paste(im, (x, y))
    d.text((x + 6, y + 6), os.path.basename(f)[len(sid) + 1:-4], fill="yellow")
q = 85
while True:
    sheet.save(dest, quality=q)
    if os.path.getsize(dest) < 300000 or q < 40:
        break
    q -= 8
print(dest, os.path.getsize(dest), len(fs), "shots")
EOF
done
