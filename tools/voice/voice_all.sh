#!/bin/sh
# Voices every spoken line that has no take yet, in ru/en/ja, and imports them (voice_audit.md, docs/localization.md).
#   tools/voice/voice_all.sh            design the missing MiniMax voices, voice the missing lines, import into this checkout
#   tools/voice/voice_all.sh --cost     only the estimate (free, nothing is sent)
#   STEPS="voice import" tools/voice/voice_all.sh   skip a step (design | voice | import)
# Paid (fal): run only with Dima's OK. FAL_API_KEY comes from ~/projects/asmrx3000/.env and is never written anywhere here.
# The import waits for a slot of the UE queue and runs at the lowest priority (nice 19, background QoS).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
python3 tools/gen_voice.py --cost
[ "$1" = "--cost" ] && exit 0
STEPS="${STEPS:-design voice import}"
FAL_API_KEY="${FAL_API_KEY:-$(grep '^FAL_API_KEY=' "$HOME/projects/asmrx3000/.env" | cut -d= -f2- | tr -d '"')}"
export FAL_API_KEY
[ -n "$FAL_API_KEY" ] || { echo "no FAL_API_KEY"; exit 1; }
case " $STEPS " in *" design "*) python3 tools/gen_voice.py --design ;; esac
case " $STEPS " in *" voice "*)
	for L in ru en ja; do python3 tools/gen_voice.py --lang "$L"; done
	python3 tools/gen_voice.py --list | grep -E "missing|stale" || echo "every line voiced" ;;
esac
case " $STEPS " in *" import "*)
	# only the takes that have no asset yet: everybody else's assets are left alone
	ONLY=$(for W in audio/voice/*/*.wav; do
		L=$(basename "$(dirname "$W")"); K=$(basename "$W" .wav)
		A="unreal/Lyudmilovsk/Content/Voice/$L/V_$(echo "$K" | tr . _).uasset"
		[ -f "$A" ] || echo "$L/$K."
	done | paste -sd, -)
	[ -n "$ONLY" ] || { echo "nothing to import"; exit 0; }
	UE="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
	PROJ="$ROOT/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
	QUIET=$("$ROOT/tools/ue_quiet_args.sh" 2>/dev/null || echo "-unattended -nosplash -nosound")
	"$ROOT/tools/wait_ue_queue.sh" 7200
	IMPORT_ONLY="$ONLY" nice -n 19 taskpolicy -b "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJ" -run=pythonscript \
		-script="$ROOT/unreal/scripts/import_voice.py" $QUIET -stdout | grep -E "VOICE|Error" | grep -v LogInit || true ;;
esac
