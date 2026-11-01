#!/bin/sh
# Waits (up to $1 seconds, default 3600) for a free slot for an offscreen game or commandlet of any checkout.
# Up to $UE_SLOTS (default 3, Dima 2026-10-31) may run at once. A benchmark or memory probe runs alone: it waits with
# --exclusive (or EXCLUSIVE=1) until nothing else runs, and while it runs (args -LyudBench or -ueexclusive) nobody starts.
# Dima's own editor window doesn't count: only UnrealEditor processes started with -game or -run= do.
#   tools/wait_ue_queue.sh [seconds] [--exclusive]
MAX=3600; EXCL="${EXCLUSIVE:-0}"
for A in "$@"; do
    case "$A" in
        --exclusive) EXCL=1 ;;
        *) MAX="$A" ;;
    esac
done
SLOTS="${UE_SLOTS:-3}"; T=0
# how many offscreen runs now, and whether one of them wants the machine alone
count() {
    N=0; X=0
    for P in $(pgrep -x UnrealEditor) $(pgrep -x UnrealEditor-Cmd); do
        ARGS=$(ps -o args= -p "$P")
        echo "$ARGS" | grep -qE " -game| -run=" || continue
        N=$((N + 1))
        echo "$ARGS" | grep -qE " -LyudBench( |$)| -ueexclusive" && X=1
    done
}
busy() {
    count
    [ "$X" -eq 1 ] && return 0
    if [ "$EXCL" = 1 ]; then [ "$N" -gt 0 ]; else [ "$N" -ge "$SLOTS" ]; fi
}
while busy; do
    [ "$T" -ge "$MAX" ] && { echo "wait_ue_queue: still busy after $T s ($N running)"; exit 1; }
    sleep 10; T=$((T + 10))
done
echo "wait_ue_queue: free after $T s ($N running${EXCL:+, exclusive=$EXCL})"
