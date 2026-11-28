#!/bin/sh
# One species at a time, one heavy process at a time: (re)build the body from the cached scan, build its hero body,
# import the species, import the hero, commit both in ONE commit and push. A species and its hero must land together
# (the old hero on a moved reference pose breaks the player bird).
#   tools/birds/morning_run.sh [--no-rebuild Id,Id] Id ...
# An import process running past 40 min is stopped (its own pid and that pid's children only), memory is waited for,
# and it is retried once.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
B=/Applications/Blender.app/Contents/MacOS/Blender
HEROES=" HerringGull Rook FeralPigeon HouseSparrow Starling GreatTit Magpie Peregrine Blackbird Robin HoodedCrow Jackdaw WoodPigeon BlackHeadedGull CommonGull Bullfinch TreeSparrow CarrionCrow BarnSwallow SandMartin WhiteWagtail SongThrush Fieldfare Waxwing Jay Nutcracker Sparrowhawk Cormorant Goldfinch Chaffinch "
NOREBUILD=","
[ "$1" = "--no-rebuild" ] && { NOREBUILD=",$2,"; shift 2; }
LOGS=blender/birds/scan_raw/logs
mkdir -p $LOGS

memwait() { while [ "$(memory_pressure | tail -1 | grep -oE '[0-9]+')" -lt 12 ]; do sleep 20; done; }

kill_tree() {
    for c in $(pgrep -P "$1"); do kill_tree "$c"; done
    kill "$1" 2>/dev/null
}

import_one() {   # $1 = Id or hero:Id
    n=$(echo "$1" | tr ':' '_')
    for attempt in 1 2; do
        memwait
        tools/wait_ue_queue.sh 14400 > /dev/null || { echo "$1: no UE slot"; return 1; }
        nice -n 10 unreal/scripts/import_birds.sh "$1" > $LOGS/import_$n.log 2>&1 &
        pid=$!
        t=0
        while kill -0 $pid 2>/dev/null; do
            sleep 10; t=$((t + 10))
            if [ $t -ge 2400 ]; then
                echo "$1: import over 40 min, stopping pid $pid (attempt $attempt)"
                kill_tree $pid
                sleep 30
                continue 2
            fi
        done
        wait $pid
        if grep -qE "Traceback|Error: " $LOGS/import_$n.log; then
            echo "$1: import errors: $(grep -E 'Traceback|Error: ' $LOGS/import_$n.log | head -2 | tr '\n' ' ')"
            return 1
        fi
        echo "$1: imported in ${t}s"
        return 0
    done
    return 1
}

for s in "$@"; do
    case "$NOREBUILD" in
        *",$s,"*) ;;
        *) memwait; BIRD_FAST=1 tools/birds/scan_build.sh $s ;;
    esac
    hero=0
    case "$HEROES" in *" $s "*) hero=1 ;; esac
    if [ $hero = 1 ]; then
        memwait
        BIRD_SCAN_CARDS=none BIRD_SCAN_MODE=S nice -n 19 $B -b --python blender/birds/build_bird.py -- $s --hero --scan --previews > $LOGS/${s}_hero.log 2>&1
        grep -q "Traceback" $LOGS/${s}_hero.log && { echo "$s: hero build failed, species skipped"; continue; }
    fi
    case "$NOREBUILD" in
        *",$s,"*) ;;
        *) import_one $s || { echo "$s: species import failed, skipped"; continue; } ;;
    esac
    if [ $hero = 1 ]; then
        import_one hero:$s || { echo "$s: hero import failed, nothing committed"; continue; }
    fi
    git add unreal/Lyudmilovsk/Content/Birds/$s
    git commit -q -m "Birds: $s from its hi3d scan (body + hero) imported

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && echo "$s: committed $(git log --oneline -1 | cut -c1-9)"
    for i in 1 2 3 4; do git push -q 2>/dev/null && break; sleep $((i * 15)); done
done
