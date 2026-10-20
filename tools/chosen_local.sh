#!/bin/sh
# Serves a local build of Chosen (github.com/takimunk/chosen) for the hero's computer in ATON OS.
# The "Chosen" program loads http://localhost:8787/ first and falls back to https://chosen.stickies.fun/
# when nothing answers there.
#   tools/chosen_local.sh            clone or update ~/projects/chosen, build it, serve it on :8787
#   CHOSEN_DIR=... PORT=... tools/chosen_local.sh
# Without TG_BOT_TOKEN everyone plays as a guest and the server keeps its data in a local SQLite file.
set -e
DIR="${CHOSEN_DIR:-$HOME/projects/chosen}"
PORT="${PORT:-8787}"

if [ ! -d "$DIR/.git" ]; then
    git clone https://github.com/takimunk/chosen.git "$DIR"
else
    git -C "$DIR" pull --ff-only || echo "chosen: could not update, using the current checkout"
fi
cd "$DIR"
[ -d node_modules ] || npm ci
npm run build
echo "Chosen for ATON OS on http://localhost:$PORT/"
PORT="$PORT" exec npm start
