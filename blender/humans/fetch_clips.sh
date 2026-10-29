#!/bin/sh
# Downloads the third-party clip sources that blender/humans/clips.json retargets (nothing of theirs is committed):
#   Quaternius, Universal Animation Library (Standard) and Universal Animation Library 2 (Standard), CC0 1.0
#   (https://quaternius.com/packs/universalanimationlibrary.html, mirrored on OpenGameArt)
# into Saved/ClipSources/ual1, ual2. Our own older clips (Saved/ClipSources/life_anims_v1.glb) come from
# blender/humans/life_anims.py's output (Saved/LifeAnims/life_anims.glb before retarget_clips.py replaced it).
set -e
cd "$(dirname "$0")/../.."
D=Saved/ClipSources
mkdir -p "$D"
for pair in "ual1 universal_animation_librarystandard.zip" "ual2 universal_animation_library_2standard.zip"; do
  set -- $pair
  if [ ! -d "$D/$1" ]; then
    curl -sSL -o "$D/$1.zip" "https://opengameart.org/sites/default/files/$2"
    mkdir -p "$D/$1" && unzip -q -o "$D/$1.zip" -d "$D/$1" && rm "$D/$1.zip"
  fi
done
ls "$D"
