#!/bin/sh
# The player bird's first-person view, offscreen (Bird.Script fpv): ahead, a wing, down, a beat in the air, then
# ahead, down and sideways standing. Copies the marked frames to <out>/<Species>_<view>.png. Another script that marks
# frames the same way ("BIRD fpv <view> frame <n>") can be given as <script>:<Species> (e.g. perch:HouseSparrow).
#   unreal/scripts/shoot_bird_fpv.sh <out dir> Species|script:Species [...]     (PRE=<console cmds>: default 1 pm)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:?out dir}"; shift
mkdir -p "$OUT"
for A in "$@"; do
    case "$A" in *:*) K="${A%%:*}"; S="${A#*:}" ;; *) K=fpv; S="$A" ;; esac
    WARM=${WARM:-60} "$ROOT/unreal/scripts/record_bird_flight.sh" "$K:$S" "${K}_$S" "${PRE-lyud.Birds.Time 13}" > "$OUT/${K}_$S.log" 2>&1 || true
    F="$ROOT/unreal/Lyudmilovsk/Saved/BirdFlight/${K}_$S"
    grep -o "BIRD fpv [a-z0-9_]* frame [0-9]*" "$F/run.log" | while read -r _ _ VIEW _ N; do
        P=$(printf "%s/f_%05d.png" "$F" $((N - 1)))
        [ -f "$P" ] && cp "$P" "$OUT/${S}_$VIEW.png"
    done
    ls "$OUT" | grep -c "^${S}_" | sed "s/^/$S shots: /"
done
