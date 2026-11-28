#!/bin/sh
# Runs one offscreen UE command and never lets it hold its queue slot forever (the 5.8 exit hang: the game writes
# "Log file closed" and then sits in exit for hours, so tools/wait_ue_queue.sh blocks every other thread).
#   tools/ue_watchdog.sh <log> <timeout s> <command...>
# Kills only the command's own PID: TERM, then KILL after 10 s, either UE_WATCHDOG_GRACE (60) s after its log says
# "Log file closed" or at the timeout. Exit code: the command's own when it quits by itself; 0 when it hung after its
# log closed (everything, e.g. a bench JSON, was written before the close); 124 at the timeout.
# The caller keeps its queue hold for as long as it lives, so it is released as soon as the caller exits after this.
LOG="${1:?log}"; MAX="${2:?timeout s}"; shift 2
GRACE="${UE_WATCHDOG_GRACE:-60}"; STEP="${UE_WATCHDOG_STEP:-5}"
"$@" &
PID=$!
stop() {  # TERM, then KILL after 10 s if it is still there
    kill "$PID" 2>/dev/null
    I=0; while [ $I -lt 10 ] && kill -0 "$PID" 2>/dev/null; do sleep 1; I=$((I + 1)); done
    kill -9 "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
}
trap 'stop; exit 130' INT TERM HUP
T=0; CLOSED=-1
while kill -0 "$PID" 2>/dev/null; do
    sleep "$STEP"; T=$((T + STEP))
    kill -0 "$PID" 2>/dev/null || break
    if [ "$CLOSED" -lt 0 ] && grep -qa "Log file closed" "$LOG" 2>/dev/null; then CLOSED=$T; fi
    if [ "$CLOSED" -ge 0 ] && [ $((T - CLOSED)) -ge "$GRACE" ]; then
        echo "ue_watchdog: $PID still running ${GRACE}s after its log closed (exit hang), killed"; stop; exit 0
    fi
    if [ "$T" -ge "$MAX" ]; then echo "ue_watchdog: $PID over ${MAX}s, killed"; stop; exit 124; fi
done
wait "$PID"
