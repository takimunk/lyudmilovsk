#!/bin/sh
# Waits (up to $1 seconds, default 3600) for a free slot for an offscreen game or commandlet of any checkout.
# Up to $UE_SLOTS (default 3, Dima 2026-10-31) may run at once. A benchmark or memory probe runs alone: it waits with
# --exclusive (or EXCLUSIVE=1) until nothing else runs, and while it runs (args -LyudBench or -ueexclusive) nobody starts.
# Dima's own editor window doesn't count: only UnrealEditor processes started with -game or -run= do.
#   tools/wait_ue_queue.sh [seconds] [--exclusive]
# While /tmp/lyud_dima_playing exists (Dima is playing) nothing new starts (UE_QUEUE_IGNORE_PLAYING=1 overrides).
# A waiting exclusive run is not starved: from the moment it starts waiting, and for as long as its caller lives (the
# script that called this one, e.g. bench.sh; or the process in $UE_QUEUE_HOLD_PID, e.g. a wrapper that runs several
# benches back to back), no other run starts. The hold is a file /tmp/lyud_ue_hold/<pid>; a dead pid's file is dropped.
MAX=3600; EXCL="${EXCLUSIVE:-0}"
for A in "$@"; do
    case "$A" in
        --exclusive) EXCL=1 ;;
        *) MAX="$A" ;;
    esac
done
SLOTS="${UE_SLOTS:-3}"; T=0
HOLD_DIR=/tmp/lyud_ue_hold; HOLDER="${UE_QUEUE_HOLD_PID:-$PPID}"
if [ "$EXCL" = 1 ]; then
    mkdir -p "$HOLD_DIR" && : > "$HOLD_DIR/$HOLDER"
    # if the wait fails or is interrupted the hold goes; on success it stays until the holder exits
    trap 'rm -f "$HOLD_DIR/$HOLDER"; exit 1' INT TERM HUP
fi
# somebody else holds the machine for an exclusive run (the oldest live hold wins between two exclusive waiters)
held() {
    for F in $(ls -tr "$HOLD_DIR" 2>/dev/null); do
        if kill -0 "$F" 2>/dev/null; then
            [ "$F" = "$HOLDER" ] && return 1
            return 0
        fi
        rm -f "$HOLD_DIR/$F"
    done
    return 1
}
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
    # Dima is playing (touch /tmp/lyud_dima_playing; rm when done): no new offscreen runs meanwhile
    [ -e /tmp/lyud_dima_playing ] && [ "${UE_QUEUE_IGNORE_PLAYING:-0}" != 1 ] && { N=playing; return 0; }
    count
    held && { N="$N, held for an exclusive run"; return 0; }
    [ "$X" -eq 1 ] && return 0
    if [ "$EXCL" = 1 ]; then [ "$N" -gt 0 ]; else [ "$N" -ge "$SLOTS" ]; fi
}
while busy; do
    [ "$T" -ge "$MAX" ] && { [ "$EXCL" = 1 ] && rm -f "$HOLD_DIR/$HOLDER"; echo "wait_ue_queue: still busy after $T s ($N running)"; exit 1; }
    sleep 10; T=$((T + 10))
done
echo "wait_ue_queue: free after $T s ($N running${EXCL:+, exclusive=$EXCL})"
