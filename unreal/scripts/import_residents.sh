#!/bin/sh
# The residents' models (blender/residents/export/R_*.glb, docs/residents.md) into /Game/Characters/Humans/Town with
# import_residents.py: a first editor run makes the shared atlases, masters and instances, then runs of up to 60 people
# each follow until it reports nothing left. Each run waits its turn in the UE queue (tools/wait_ue_queue.sh) and runs
# at low priority. Stop it any time and start it again: what is in and current is skipped. Not with the editor open on
# this checkout.
#   unreal/scripts/import_residents.sh [--only key,key] [--limit N] [--force] [--retry-failed] [--prune]
#   JOBS=2 unreal/scripts/import_residents.sh     two editors at once, every other person each (two queue slots)
#   PRIO= unreal/scripts/import_residents.sh      full speed (default nice -n 19 taskpolicy -b: the efficiency cores)
#   CHECK=0 unreal/scripts/import_residents.sh    skip the exports' check (blender/residents/check_export.py, no engine)
# unreal/Lyudmilovsk/Saved/ImportResidents: import.log (what each run did), run_*.log (the last runs whole), broke_*.log
# (runs that ended without their last word: a crash, a hang). The tally any time:
#   python3 unreal/scripts/import_residents.py --report
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
CMD="$UE/Engine/Binaries/Mac/UnrealEditor-Cmd"
OUT="$ROOT/unreal/Lyudmilovsk/Saved/ImportResidents"
LOG="$OUT/import.log"
SKEL="$ROOT/unreal/Lyudmilovsk/Content/Characters/Humans/SK_Human_Skeleton.uasset"
KEEP="$OUT/SK_Human_Skeleton.keep"
PRIO="${PRIO-nice -n 19 taskpolicy -b}"
JOBS="${JOBS:-1}"
set -f                                 # ue_quiet_args.sh's -ini:...[/Script/...] words aren't globs
QUIET=$("$ROOT/tools/ue_quiet_args.sh")
# the session's start: --force and --retry-failed redo what was done before it, once (the .py's --since)
ARGS="--since $(date +%s)"
PRUNE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --force|--retry-failed) ARGS="$ARGS $1" ;;
        --prune) PRUNE="--prune" ;;
        --only|--limit|--chunk) [ $# -ge 2 ] || { echo "$1 needs a value"; exit 2; }; ARGS="$ARGS $1 $2"; shift ;;
        *) echo "unknown argument $1 (see the head of $0)"; exit 2 ;;
    esac
    shift
done
[ -x "$CMD" ] || { echo "no $CMD (UE_ROOT?)"; exit 1; }
[ -f "$SKEL" ] || { echo "no $SKEL: import the cast first"; exit 1; }
# an editor open on this checkout keeps its own copies of what the runs write, and may save them back over it
if ps -A -o args= | grep -F "$PROJ" | grep -v -e " -run=" -e " -game" -e grep | grep -q UnrealEditor; then
    echo "an editor is open on $PROJ: close it first"; exit 1
fi
mkdir -p "$OUT"
echo "== $(date '+%Y-%m-%d %H:%M') import_residents.sh $PRUNE $ARGS (jobs $JOBS)" >> "$LOG"

# the exports first, without the engine (56 bones, four sections, six morphs, UV0-2, weights): a broken model never
# gets near SK_Human_Skeleton
if [ "${CHECK:-1}" != 0 ]; then
    python3 "$ROOT/blender/residents/check_export.py" > "$OUT/check_export.log" 2>&1; C=$?
    tail -n 20 "$OUT/check_export.log"
    cat "$OUT/check_export.log" >> "$LOG"
    [ "$C" = 0 ] || { echo "the exports have problems ($OUT/check_export.log): rebuild them, or CHECK=0 to import anyway"; exit 1; }
fi

# importing a mesh onto SK_Human_Skeleton can rewrite the skeleton's reference pose from that mesh, and every shared
# clip then bends the others' arms into their bodies (fix_hi3d.sh): after each run it is put back as it was
cp -p "$SKEL" "$KEEP" || exit 1
restore() {
    cmp -s "$KEEP" "$SKEL" && return 0
    # copied beside it and moved over: another shard's editor never reads half a file
    cp -p "$KEEP" "$SKEL.residents" && mv -f "$SKEL.residents" "$SKEL" &&
        echo "${P}SK_Human_Skeleton.uasset had been changed: put back as it was" | tee -a "$LOG"
}

# a run whose log stands still for $STALL s (a hung import, a dialog nobody sees) or that takes $TIMEOUT s is ended;
# the .py finds the person it was on still "trying" and counts that as a try
watchdog() {
    T=0; S=0; Z=0
    while kill -0 "$PID" 2>/dev/null; do
        sleep 10; T=$((T + 10))
        Y=$( (wc -c < "$RUNLOG") 2>/dev/null | tr -d ' ')
        if [ "${Y:-0}" -gt "$Z" ]; then Z=$Y; S=0; else S=$((S + 10)); fi
        if [ "$S" -ge "${STALL:-1200}" ] || [ "$T" -ge "${TIMEOUT:-21600}" ]; then
            echo "${P}$(date '+%H:%M') editor ended after $T s, its log still for $S s" | tee -a "$LOG"
            kill "$PID" 2>/dev/null; sleep 30; kill -9 "$PID" 2>/dev/null
            return
        fi
    done
}

# one editor run of import_residents.py "$@" for share $J: sets RUNLOG, LEFT (empty: the run broke off) and MOVED (the
# people it imported or found current; failures don't count, or a systematic one would wear through the whole town)
N=0
run() {
    J="$1"; shift
    N=$((N + 1)); RUNLOG="$OUT/run_${J}_$((N % 6)).log"
    until "$ROOT/tools/wait_ue_queue.sh" 7200 >> "$LOG"; do :; done
    rm -f "$ROOT/unreal/Lyudmilovsk/Saved/Autosaves/PackageRestoreData.json" "$RUNLOG"
    $PRIO "$CMD" "$PROJ" -run=pythonscript -script="$HERE/import_residents.py $*" $QUIET -nullrhi \
        -abslog="$RUNLOG" > "$RUNLOG.out" 2>&1 &
    PID=$!
    echo "$PID" > "$OUT/editor_$J.pid"
    watchdog
    wait "$PID"; CODE=$?
    rm -f "$OUT/editor_$J.pid"
    restore
    grep "RESIDENTS " "$RUNLOG" 2>/dev/null | sed 's/^.*RESIDENTS /RESIDENTS /' | sed "s|^|$P|" | tee -a "$LOG"
    grep -e "Error: " -e "Fatal error" -e "Critical error" "$RUNLOG" 2>/dev/null | grep -v "RESIDENTS " |
        sed -E 's/^(\[[^]]*\])+//' | uniq | head -n 40 | sed "s|^|$P|" | tee -a "$LOG"
    LEFT=$(sed -n 's/.*RESIDENTS left \([0-9][0-9]*\).*/\1/p' "$RUNLOG" 2>/dev/null | tail -n 1)
    # (counted from the people's own lines: a run that dies after a few imports still moved them)
    MOVED=$(grep -c "RESIDENTS imported " "$RUNLOG" 2>/dev/null)
    C=$(sed -n 's/.*RESIDENTS run: .* failed, \([0-9]*\) meshes checked.*/\1/p' "$RUNLOG" 2>/dev/null | tail -n 1)
    MOVED=$((${MOVED:-0} + ${C:-0}))
    if [ -z "$LEFT" ]; then
        B="$OUT/broke_$(date +%Y%m%d_%H%M%S)_$J.log"
        cp "$RUNLOG" "$B" 2>/dev/null || cp "$RUNLOG.out" "$B" 2>/dev/null   # (no log: it died starting)
        echo "${P}the run broke off (exit $CODE): $B" | tee -a "$LOG"
    fi
}

# runs for one share until nothing of it is left; three runs in a row that move nobody (the editor won't start, every
# import fails or crashes) give up. A person that fails or takes the editor down is out after its second try (the .py's
# MAX_TRIES), so one bad person costs at most two runs
loop() {
    J="$1"; shift
    IDLE=0
    while :; do
        run "$J" "$@"
        [ "$LEFT" = 0 ] && return 0
        if [ "$MOVED" -gt 0 ]; then IDLE=0; else IDLE=$((IDLE + 1)); fi
        if [ "$IDLE" -ge 3 ]; then
            echo "${P}three runs in a row moved nobody (${LEFT:-?} left): see $RUNLOG" | tee -a "$LOG"
            return 1
        fi
    done
}

# Ctrl-C or a kill: no new runs, the editors this started end (the .py counts the person each was on as tried), and
# the EXIT trap puts the skeleton back
LOOPS=""
stop() {
    trap - INT TERM HUP
    for X in $LOOPS; do kill "$X" 2>/dev/null; done
    EDS=""
    for J in prep $(seq 0 "$((JOBS - 1))"); do
        [ -f "$OUT/editor_$J.pid" ] && EDS="$EDS $(cat "$OUT/editor_$J.pid")" && rm -f "$OUT/editor_$J.pid"
    done
    if [ -n "$EDS" ]; then
        kill $EDS 2>/dev/null; sleep 15; kill -9 $EDS 2>/dev/null
    fi
    echo "${P}stopped" | tee -a "$LOG"
    exit 130
}
trap restore EXIT
trap stop INT TERM HUP

P=""
run prep --prepare $PRUNE $ARGS
grep -q "RESIDENTS prepared" "$RUNLOG" 2>/dev/null || { echo "the shared assets weren't made: see $RUNLOG"; exit 1; }
OK=0
if [ "$LEFT" = 0 ]; then
    :                                  # nothing new since the last session
elif [ "$JOBS" -le 1 ]; then
    loop 0 $ARGS || OK=1
else
    I=0
    while [ "$I" -lt "$JOBS" ]; do
        ( P="[$I/$JOBS] "; loop "$I" $ARGS --shard "$I/$JOBS" ) &
        LOOPS="$LOOPS $!"
        I=$((I + 1))
        [ "$I" -lt "$JOBS" ] && sleep 90   # the editors' start-up (asset registry, DDC) apart
    done
    for X in $LOOPS; do wait "$X" || OK=1; done
fi
python3 "$HERE/import_residents.py" --report $ARGS | tee -a "$LOG"
exit $OK
