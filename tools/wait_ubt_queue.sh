#!/bin/sh
# Builds a checkout's editor through a strict first-come-first-served queue shared by all worktrees on this Mac.
# UnrealBuildTool's own mutex (-WaitMutex) is global per engine install but not fair: a build that has waited for hours
# can keep losing it to newer ones. Here every build takes a numbered ticket and starts only when it holds the lowest
# live one; tickets of dead processes are dropped. The build runs at the lowest priority with 4 parallel actions
# (Dima's play sessions come first) and with -WaitMutex, so a build started without this queue only delays it.
#
#   tools/wait_ubt_queue.sh                        LyudmilovskEditor Mac Development of this checkout, -NoHotReloadFromIDE
#   tools/wait_ubt_queue.sh <Build.sh args...>     those args instead (-MaxParallelActions=4 -WaitMutex are added)
#   UBT_QUEUE_MAX=<s> (default 14400) gives up waiting after that long (exit 1); UBT_QUEUE_DIR overrides the queue folder
#   tools/wait_ubt_queue.sh --list                 the queue now (ticket, pid, checkout)
# Exit code: the build's.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
Q="${UBT_QUEUE_DIR:-/tmp/lyud_ubt_queue}"
MAX="${UBT_QUEUE_MAX:-14400}"
mkdir -p "$Q"

# the queue's tickets of live processes, oldest first; dead ones are removed on the way
live() {
    for F in "$Q"/t.*; do
        [ -e "$F" ] || continue
        P=$(sed -n 1p "$F" 2>/dev/null)
        if [ -n "$P" ] && kill -0 "$P" 2>/dev/null; then
            basename "$F"
        else
            rm -f "$F"
        fi
    done | sort
}

if [ "$1" = "--list" ]; then
    for T in $(live); do echo "$T $(tr '\n' ' ' < "$Q/$T")"; done
    exit 0
fi

# a ticket: the next number, taken under a lock (mkdir is atomic)
while ! mkdir "$Q/lock" 2>/dev/null; do
    # a lock left by a killed process: older than a minute
    [ -n "$(find "$Q/lock" -maxdepth 0 -mmin +1 2>/dev/null)" ] && rmdir "$Q/lock" 2>/dev/null
    sleep 0.2
done
N=$(( $(cat "$Q/next" 2>/dev/null || echo 0) + 1 ))
echo "$N" > "$Q/next"
TICKET="$Q/t.$(printf %09d "$N")"
printf '%s\n%s\n' "$$" "$ROOT" > "$TICKET"
rmdir "$Q/lock"
trap 'rm -f "$TICKET"' EXIT
trap 'rm -f "$TICKET"; exit 130' INT TERM

T=0
while :; do
    FIRST=$(live | head -1)
    [ "$FIRST" = "$(basename "$TICKET")" ] && break
    [ "$T" -ge "$MAX" ] && { echo "wait_ubt_queue: still not first after $T s"; exit 1; }
    [ $((T % 300)) -eq 0 ] && echo "wait_ubt_queue: ticket $N waiting ($(live | wc -l | tr -d ' ') in the queue, first $FIRST)"
    sleep 5; T=$((T + 5))
done
echo "wait_ubt_queue: ticket $N building after $T s"

if [ $# -eq 0 ]; then
    set -- LyudmilovskEditor Mac Development -Project="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" -NoHotReloadFromIDE
fi
# drop any parallelism the caller asked for: 4 at most
ARGS=""
for A in "$@"; do
    case "$A" in -MaxParallelActions=*|-WaitMutex) ;; *) ARGS="$ARGS
$A" ;; esac
done
OLDIFS=$IFS; IFS='
'
# shellcheck disable=SC2086
nice -n 19 taskpolicy -b "$UE/Engine/Build/BatchFiles/Mac/Build.sh" $ARGS -MaxParallelActions=4 -WaitMutex
RC=$?
IFS=$OLDIFS
exit $RC
