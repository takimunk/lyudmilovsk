#!/bin/sh
# Builds a checkout's editor through a first-come-first-served queue shared by all worktrees on this Mac.
# UnrealBuildTool's own mutex is global per engine install and not fair: a build that has waited for hours can keep
# losing it to newer ones. Here every build takes a numbered ticket and starts once it is among the first
# UBT_QUEUE_SLOTS live ones in priority order (default 3, the third only while over 8 GB of RAM is free or inactive;
# only 1 while an offscreen -game / benchmark runs, so a ~22 GB game and builds don't swap). Builds already running
# from older copies of this script take their slots too. Builds run with -NoMutex (each worktree has its own
# Intermediate and Binaries, the engine is an installed build), -NoUBA (two UBTs would share its storage and port)
# and 4 parallel actions each, at nice 19; in the background QoS (taskpolicy -b) while Dima plays.
#
#   tools/wait_ubt_queue.sh                        LyudmilovskEditor Mac Development of this checkout, -NoHotReloadFromIDE
#   tools/wait_ubt_queue.sh <Build.sh args...>     those args instead (-MaxParallelActions=4 -NoMutex -NoUBA are added)
#   tools/wait_ubt_queue.sh --list                 the queue in start order (ticket, priority, pid, checkout)
#   tools/wait_ubt_queue.sh --prio <ticket> <n>    reorder: lower priority numbers start first (default 100)
#   UBT_QUEUE_MAX=<s> (default 14400) gives up waiting after that long (exit 1); UBT_QUEUE_DIR overrides the queue folder
#   UBT_QUEUE_SLOTS=<n> (default 3) builds at once, those past 2 only with UBT_QUEUE_SLOT3_GB (8) GB free
#   UBT_QUEUE_BG=1                                 background QoS always
#   touch /tmp/lyud_dima_playing                   Dima is playing: new builds start in background QoS (rm when done);
#                                                  tools/wait_ue_queue.sh holds new offscreen runs meanwhile
# Dima playing is also detected (read-only, best effort): a -game of the main checkout that isn't offscreen, or his
# main-checkout editor with a PIE session open in its latest log.
# Exit code: the build's.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
Q="${UBT_QUEUE_DIR:-/tmp/lyud_ubt_queue}"
MAX="${UBT_QUEUE_MAX:-14400}"
SLOTS="${UBT_QUEUE_SLOTS:-3}"
SLOT3_GB="${UBT_QUEUE_SLOT3_GB:-8}"   # the slots past two only while free + inactive RAM is above this (GB)
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

# the live tickets in start order: priority (file prio.<ticket> holding a number, default 100, lower goes first), then
# ticket number. Waiters of older copies of this script ignore priorities: they start when they hold the lowest number
ordered() {
    for T in $(live); do
        PR=$(cat "$Q/prio.$T" 2>/dev/null | tr -cd '0-9-')
        printf '%s %s\n' "${PR:-100}" "$T"
    done | sort -k1,1n -k2,2 | cut -d' ' -f2
}

# free + inactive + speculative memory, whole GB (vm_stat)
mem_free_gb() {
    vm_stat 2>/dev/null | awk '/page size of/ {ps = $8} /^Pages (free|inactive|speculative):/ {gsub("[.]", "", $NF); n += $NF}
        END {printf "%d", n * (ps ? ps : 16384) / 1073741824}'
}

# a ticket's waiter is building now (its Build.sh child runs): waiters of every copy of this script
building() {
    P=$(sed -n 1p "$Q/$1" 2>/dev/null)
    [ -n "$P" ] && pgrep -P "$P" 2>/dev/null | while read -r C; do ps -o args= -p "$C" 2>/dev/null; done | grep -qE '^[^ ]*sh .*/Build[.]sh( |$)'
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
    for T in $(ordered); do
        PR=$(cat "$Q/prio.$T" 2>/dev/null | tr -cd '0-9-')
        echo "$T prio ${PR:-100} $(head -2 "$Q/$T" | tr '\n' ' ')"
    done
    for F in "$Q"/prio.t.*; do [ -e "$F" ] && [ ! -e "$Q/${F#"$Q"/prio.}" ] && rm -f "$F"; done   # of gone tickets
    exit 0
fi
# --prio <ticket number or t.NNNNNNNNN> <priority>: reorder the queue (lower starts first, default 100)
if [ "$1" = "--prio" ]; then
    T="$2"; case "$T" in t.*) ;; *) T="t.$(printf %09d "$T")" ;; esac
    [ -e "$Q/$T" ] || { echo "no ticket $T"; exit 2; }
    echo "$3" | grep -qE '^-?[0-9]+$' || { echo "priority must be a number"; exit 2; }
    echo "$3" > "$Q/prio.$T"
    exec "$0" --list
fi

# (edit this file only by replacing it, e.g. write a copy and mv it over: sh reads a running script as it goes)

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
# UBT_QUEUE_PRIO=<n>: this build's priority from the start (see --prio)
[ -n "${UBT_QUEUE_PRIO:-}" ] && echo "$UBT_QUEUE_PRIO" > "$Q/prio.$(basename "$TICKET")"
trap 'rm -f "$TICKET" "$Q/prio.$(basename "$TICKET")"' EXIT
trap 'rm -f "$TICKET" "$Q/prio.$(basename "$TICKET")"; exit 130' INT TERM

T=0
while :; do
    K=$SLOTS
    [ "$K" -gt 2 ] && [ "$(mem_free_gb)" -lt "$SLOT3_GB" ] && K=2
    game_running && K=1
    [ "$K" -lt 1 ] && K=1
    # the builds already running take their slots (older copies of this script start by ticket number alone); the
    # free ones go to the waiters in priority order
    R=0; W=""
    for X in $(ordered); do
        if building "$X"; then R=$((R + 1)); else W="$W $X"; fi
    done
    F=$((K - R))
    [ "$F" -gt 0 ] && echo "$W" | tr ' ' '\n' | grep -v '^$' | head -n "$F" | grep -qx "$(basename "$TICKET")" && break
    [ "$T" -ge "$MAX" ] && { echo "wait_ubt_queue: still not started after $T s"; exit 1; }
    [ $((T % 300)) -eq 0 ] && echo "wait_ubt_queue: ticket $N waiting ($(live | wc -l | tr -d ' ') in the queue, $R of $K slot(s) building)"
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
    case "$A" in -MaxParallelActions=*|-WaitMutex|-NoMutex|-NoUBA|-UBA) ;; *) ARGS="$ARGS
$A" ;; esac
done
OLDIFS=$IFS; IFS='
'
# shellcheck disable=SC2086
$PRIOW "$UE/Engine/Build/BatchFiles/Mac/Build.sh" $ARGS -MaxParallelActions=4 -NoMutex -NoUBA
RC=$?
IFS=$OLDIFS
exit $RC
