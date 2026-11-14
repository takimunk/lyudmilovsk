#!/bin/bash
# The quests outside the engine (docs/ui.md): checks Content/UI/quests.json and what it names, and plays every quest
# through a copy of ULyudQuests' state machine (tools/questsim/engine.py). Run from anywhere.
#   tools/questsim/run.sh                 this checkout
#   tools/questsim/run.sh <tree>          another checkout or exported tree (its LyudQuests.cpp rules are used)
#   tools/questsim/run.sh --ref <ref>     a branch: exports its text files to a temporary tree first
# Add -v for more detail.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
if [ "$1" = "--ref" ]; then
	REF="$2"
	shift 2
	TMP="$(mktemp -d)"
	trap 'rm -rf "$TMP"' EXIT
	PATHS=()
	for P in Content/UI Content/Localization Content/Dialogue Content/PeopleData Content/BirdsData Content/Dev Content/City Source; do
		if git -C "$ROOT" cat-file -e "$REF:unreal/Lyudmilovsk/$P" 2>/dev/null; then
			PATHS+=("unreal/Lyudmilovsk/$P")
		fi
	done
	GIT_LFS_SKIP_SMUDGE=1 git -C "$ROOT" archive "$REF" "${PATHS[@]}" | tar -x -C "$TMP"
	python3 "$HERE/questsim.py" "$TMP" "$@"
	exit $?
fi
exec python3 "$HERE/questsim.py" "$@"
