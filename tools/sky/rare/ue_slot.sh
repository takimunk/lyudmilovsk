# Sourced: waits until fewer than two heavy UE runs (offscreen games, commandlets) are going on this Mac, any checkout
# (the machine's rule: two at most). Dima's editor window doesn't count.
heavy() {
	N=0
	for P in $(pgrep -x UnrealEditor) $(pgrep -x UnrealEditor-Cmd); do
		ps -o args= -p "$P" | grep -qE " -game| -run=" && N=$((N + 1))
	done
	echo $N
}
W=0
while [ "$(heavy)" -ge 2 ]; do
	[ $W -eq 0 ] && echo "ue_slot: waiting for a free UE slot"
	sleep 15; W=$((W + 15))
done
