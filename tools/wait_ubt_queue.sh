#!/bin/sh
# Builds a checkout's editor through a first-come-first-served queue shared by all worktrees on this Mac.
# UnrealBuildTool's own mutex is global per engine install and not fair: a build that has waited for hours can keep
# losing it to newer ones. Here every build takes a numbered ticket and starts once it is among the lowest
# UBT_QUEUE_SLOTS live ones (default 2; only 1 while an offscreen -game / benchmark runs, so a ~22 GB game and two
# builds don't swap). Builds run with -NoMutex (each worktree has its own Intermediate and Binaries, the engine is an
# installed build) and 4 parallel actions each, at nice 19; in the background QoS (taskpolicy -b) while Dima plays.
#
#   tools/wait_ubt_queue.sh                        LyudmilovskEditor Mac Development of this checkout, -NoHotReloadFromIDE
#   tools/wait_ubt_queue.sh <Build.sh args...>     those args instead (-MaxParallelActions=4 -NoMutex are added)
#   tools/wait_ubt_queue.sh --list                 the queue now (ticket, pid, checkout)
#   UBT_QUEUE_MAX=<s> (default 14400) gives up waiting after that long (exit 1); UBT_QUEUE_DIR overrides the queue folder
#   UBT_QUEUE_SLOTS=<n> (default 2) builds at once; UBT_QUEUE_BG=1 always background QoS
#   touch /tmp/lyud_dima_playing                   Dima is playing: new builds start in background QoS (rm when done);
#                                                  tools/wait_ue_queue.sh holds new offscreen runs meanwhile
# Dima playing is also detected (read-only, best effort): a -game of the main checkout that isn't offscreen, or his
# main-checkout editor with a PIE session open in its latest log.
# Exit code: the build's.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
Q="${UBT_QUEUE_DIR:-/tmp/lyud_ubt_queue}"
MAX="${UBT_QUEUE_MAX:-14400}"
SLOTS="${UBT_QUEUE_SLOTS:-2}"
MAIN="${LYUD_MAIN:-$HOME/projects/lyudmilovsk}"
PLAYING_FLAG=/tmp/lyud_dima_playing
mkdir -p "$Q"

# the queue's tickets of live processes, oldest first; dead ones are removed on the way. A ticket is "pid\ncheckout"
# (older scripts) or "pid\ncheckout\nstart time" (ps -o lstart=): with a start time a reused pid doesn't keep it alive
live() {
    for F in "$Q"/t.*; do
        [ -e "$F" ] || continue
        P=$(sed -n 1p "$F" 2>/dev/null)
        S=$(sed -n 3p "$F" 2>/dev/null)
        if [ -n "$P" ] && kill -0 "$P" 2>/dev/null && { [ -z "$S" ] || [ "$(ps -o lstart= -p "$P" 2>/dev/null)" = "$S" ]; }; then
            basename "$F"
        else
            rm -f "$F"
        fi
    done | sort
}

# an offscreen -game or a benchmark of any checkout is running (it holds ~22 GB: one build at a time then)
game_running() {
    for P in $(pgrep -x UnrealEditor) $(pgrep -x UnrealEditor-Cmd); do
        ps -o args= -p "$P" 2>/dev/null | grep -qE " -game( |$)| -LyudBench( |$)| -ueexclusive" && return 0
    done
    return 1
}

# Dima is playing: the flag file, his game window (a -game of the main checkout, not offscreen), or PIE in his editor
dima_playing() {
    [ -e "$PLAYING_FLAG" ] && return 0
    ED=""
    for P in $(pgrep -x UnrealEditor); do
        A=$(ps -o args= -p "$P" 2>/dev/null)
        echo "$A" | grep -qF "$MAIN/unreal/" || continue
        if echo "$A" | grep -qE " -game( |$)"; then
            echo "$A" | grep -q -- "-RenderOffscreen" || return 0
        elif ! echo "$A" | grep -qE " -run="; then
            ED=1
        fi
    done
    [ -n "$ED" ] || return 1
    # the editor's newest log: the last PIE start after the last PIE shutdown means a PIE session is open
    L=$(ls -t "$HOME/Library/Logs/Unreal Engine/LyudmilovskEditor/"*.log 2>/dev/null | head -1)
    [ -n "$L" ] || return 1
    LAST=$(grep -a -E "LogPlayLevel: .*(PIE: Play in editor total start time|Shutting down PIE)" "$L" 2>/dev/null | tail -1)
    echo "$LAST" | grep -q "Play in editor total start time"
}

if [ "$1" = "--list" ]; then
    for T in $(live); do echo "$T $(head -2 "$Q/$T" | tr '\n' ' ')"; done
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
printf '%s\n%s\n%s\n' "$$" "$ROOT" "$(ps -o lstart= -p $$)" > "$TICKET"
rmdir "$Q/lock"
trap 'rm -f "$TICKET"' EXIT
trap 'rm -f "$TICKET"; exit 130' INT TERM

T=0
while :; do
    K=$SLOTS
    game_running && K=1
    [ "$K" -lt 1 ] && K=1
    live | head -n "$K" | grep -qx "$(basename "$TICKET")" && break
    [ "$T" -ge "$MAX" ] && { echo "wait_ubt_queue: still not started after $T s"; exit 1; }
    [ $((T % 300)) -eq 0 ] && echo "wait_ubt_queue: ticket $N waiting ($(live | wc -l | tr -d ' ') in the queue, $K slot(s), first $(live | head -1))"
    sleep 5; T=$((T + 5))
done

PRIO="nice -n 19"
if [ "${UBT_QUEUE_BG:-0}" = 1 ] || dima_playing; then
    PRIO="nice -n 19 taskpolicy -b"
fi
# (one word per line: the command below splits on newlines only)
PRIOW=$(echo "$PRIO" | tr ' ' '\n')
echo "wait_ubt_queue: ticket $N building after $T s ($PRIO)"

if [ $# -eq 0 ]; then
    set -- LyudmilovskEditor Mac Development -Project="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject" -NoHotReloadFromIDE
fi
# drop any parallelism or mutex flags the caller asked for: 4 actions at most, no global mutex
ARGS=""
for A in "$@"; do
    case "$A" in -MaxParallelActions=*|-WaitMutex|-NoMutex) ;; *) ARGS="$ARGS
$A" ;; esac
done
OLDIFS=$IFS; IFS='
'
# shellcheck disable=SC2086
$PRIOW "$UE/Engine/Build/BatchFiles/Mac/Build.sh" $ARGS -MaxParallelActions=4 -NoMutex
RC=$?
IFS=$OLDIFS
exit $RC
