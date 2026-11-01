#!/bin/sh
# Builds fish species in Blender (3 processes at a time) with --lint --preview, then a contact sheet.
#   tools/fish_build.sh [Id ...]     (no ids: every blender/fish/species/*.py)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B=/Applications/Blender.app/Contents/MacOS/Blender
DIR="$ROOT/blender/fish"
IDS="$*"
[ -z "$IDS" ] && IDS=$(ls "$DIR/species" | sed -n 's/\.py$//p' | grep -v '^__')
mkdir -p "$DIR/export"
n=0
for s in $IDS; do
    ( "$B" -b --python "$DIR/build_fish.py" -- "$s" --lint --preview > "$DIR/export/$s.build.log" 2>&1
      grep -E "^TRIS|^SIZE|^LINT [^ ]+ (ok|[a-z_]+ [0-9])|Traceback|Error" "$DIR/export/$s.build.log" | grep -v "^LINT [^ ]* lod[01] " ) &
    n=$((n + 1)); [ $((n % 3)) = 0 ] && wait
done
wait
python3 - "$DIR" $IDS <<'PY'
import sys, os
from PIL import Image
d, ids = sys.argv[1], sys.argv[2:]
tiles = []
for s in ids:
    p = os.path.join(d, "export", s + "_preview.png")
    if os.path.exists(p):
        im = Image.open(p).convert("RGB")
        tiles.append(im.resize((im.width // 2, im.height // 2), Image.LANCZOS))
if tiles:
    w, h = tiles[0].size
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (w * cols, h * rows), (96, 96, 96))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % cols) * w, (i // cols) * h))
    sheet.save(os.path.join(d, "export", "contact.png"))
    print("contact sheet", os.path.join(d, "export", "contact.png"))
PY
