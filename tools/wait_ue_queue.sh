#!/bin/sh
# Waits (up to $1 seconds, default 3600) until no offscreen game or commandlet of any checkout is running.
# Dima's own editor window doesn't count: only UnrealEditor processes started with -game or -run= do.
MAX="${1:-3600}"; T=0
busy() {
    for P in $(pgrep -x UnrealEditor) $(pgrep -x UnrealEditor-Cmd); do
        ps -o args= -p "$P" | grep -qE " -game| -run=" && return 0
    done
    return 1
}
while busy; do
    [ "$T" -ge "$MAX" ] && { echo "wait_ue_queue: still busy after $T s"; exit 1; }
    sleep 10; T=$((T + 10))
done
echo "wait_ue_queue: free after $T s"
