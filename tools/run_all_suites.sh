#!/usr/bin/env bash
# Every offline check of the game in one go (docs/testing.md): the plain-C++ host suites (tools/*/run.sh, built with the
# Mac's warnings as errors), the deterministic data generators (their committed output must not change), the data's
# cross-checks (tools/check_data.py) and the string tables (tools/check_localization.py). Prints one table; the exit code
# is the number of failed rows. Run from anywhere; no engine, no network.
#
#   tools/run_all_suites.sh             everything (~20 min; birdsim alone ~6)
#   tools/run_all_suites.sh --quick     skips the slow rows (birdsim, barksim's en/ja days, peoplesim summary,
#                                       audiosim render, underwater bed): ~6 min
#   tools/run_all_suites.sh --only peoplesim,check_data    rows whose name contains one of these words
#   tools/run_all_suites.sh --keep      leaves a generator's changed output in the tree (default: put back as committed)
#   tools/run_all_suites.sh --no-werror warnings are not errors
#
# Logs: Saved/Suites/<row>.log. Needs clang++, python3 (with PIL for underwater bed). The LFS worlds that animalsim and
# birdsim read (Content/AnimalsData/animal_world.bin, Content/BirdsData/bird_world.bin) must be pulled:
#   git lfs pull --include=unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin,unreal/Lyudmilovsk/Content/BirdsData/bird_world.bin
# Known on main: audiosim test has 2 failures (thunderstorm peak, far-storm rumble); check_localization lists items
# whose icon pictures are missing.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
LOGS="$ROOT/Saved/Suites"
mkdir -p "$LOGS"

QUICK=0
KEEP=0
WERROR=1
ONLY=""
while [ $# -gt 0 ]; do
	case "$1" in
	--quick) QUICK=1 ;;
	--keep) KEEP=1 ;;
	--no-werror) WERROR=0 ;;
	--only) shift; ONLY="${1:-}" ;;
	-h|--help) sed -n '2,20p' "$0"; exit 0 ;;
	*) echo "unknown option $1 (see --help)"; exit 2 ;;
	esac
	shift
done

export EXTRA_CXXFLAGS="-Wall -Wshadow -Wunreachable-code-loop-increment${EXTRA_CXXFLAGS:+ $EXTRA_CXXFLAGS}"
[ "$WERROR" = 1 ] && EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -Werror"

CONTENT="unreal/Lyudmilovsk/Content"
RAIL="$CONTENT/Railway/railway_built.json"
NAMES=()
RESULTS=()
TIMES=()
NOTES=()
FAILS=0

wanted() {   # wanted <row> [slow]
	if [ -n "$ONLY" ]; then
		local w
		for w in ${ONLY//,/ }; do
			case "$1" in *"$w"*) return 0 ;; esac
		done
		return 1
	fi
	[ "$QUICK" = 1 ] && [ "${2:-}" = slow ] && return 1
	return 0
}

record() {   # record <row> <PASS|FAIL> <seconds> <note>
	NAMES+=("$1"); RESULTS+=("$2"); TIMES+=("$3"); NOTES+=("$4")
	[ "$2" = FAIL ] && FAILS=$((FAILS + 1))
	printf '%-34s %-4s %5ss  %s\n' "$1" "$2" "$3" "$4"
}

# the last line of a log that says how it went (counts), for the table
summary_of() {
	local l
	l="$(grep -aiE 'pass|fail|all ok|problems|error|warning:' "$1" | tail -1)"
	[ -n "$l" ] || l="$(grep -av '^[[:space:]]*$' "$1" | tail -1)"
	printf '%s' "$l" | cut -c1-90
}

# suite <row> <slow|-> <command...>: passes when the command exits 0
suite() {
	local row="$1" slow="$2"
	shift 2
	wanted "$row" "$slow" || return 0
	local log="$LOGS/$row.log" t0=$SECONDS rc
	"$@" > "$log" 2>&1
	rc=$?
	local note
	note="$(summary_of "$log")"
	if [ $rc -eq 0 ]; then
		record "$row" PASS $((SECONDS - t0)) "$note"
	else
		record "$row" FAIL $((SECONDS - t0)) "exit $rc: $note"
	fi
}

# generator <row> <output paths (space separated)> <command...>: passes when the generator runs and its committed output
# does not change. A path that is already modified before the run is skipped (yours to commit), not judged.
generator() {
	local row="$1" outs="$2"
	shift 2
	wanted "$row" || return 0
	local log="$LOGS/$row.log" t0=$SECONDS rc dirty
	# shellcheck disable=SC2086
	dirty="$(git status --porcelain -- $outs)"
	if [ -n "$dirty" ]; then
		record "$row" SKIP 0 "output already modified here: commit or restore it first"
		return 0
	fi
	"$@" > "$log" 2>&1
	rc=$?
	local changed
	# shellcheck disable=SC2086
	changed="$(git status --porcelain -- $outs)"
	if [ $rc -ne 0 ]; then
		record "$row" FAIL $((SECONDS - t0)) "exit $rc: $(tail -1 "$log" | cut -c1-80)"
	elif [ -n "$changed" ]; then
		# shellcheck disable=SC2086
		git diff --stat -- $outs >> "$log"
		record "$row" FAIL $((SECONDS - t0)) "committed output is stale: $(echo "$changed" | wc -l) file(s), see $row.log"
	else
		record "$row" PASS $((SECONDS - t0)) "output unchanged"
	fi
	if [ "$KEEP" = 0 ] && [ -n "$changed" ]; then
		# shellcheck disable=SC2086
		git checkout -q -- $outs 2>/dev/null
		# shellcheck disable=SC2086
		git clean -q -f -- $outs 2>/dev/null
	fi
}

echo "suites: $ROOT (EXTRA_CXXFLAGS=$EXTRA_CXXFLAGS)"
printf '%-34s %-4s %6s  %s\n' "row" "" "time" "result"

# --- 1. the host suites (tools/*/run.sh), every mode -----------------------------------------------------------------
suite animalsim.test              - bash tools/animalsim/run.sh test
suite animalsim.day               - bash tools/animalsim/run.sh day 6 2
suite audiosim.test               - bash tools/audiosim/run.sh test
suite audiosim.fountain           - bash tools/audiosim/run.sh fountain
suite audiosim.weather            - bash tools/audiosim/run.sh weather
suite audiosim.render             slow bash tools/audiosim/run.sh render
suite barksim.test                - bash tools/barksim/run.sh
suite barksim.day.ru              - bash tools/barksim/run.sh day ru
suite barksim.day.en              slow bash tools/barksim/run.sh day en
suite barksim.day.ja              slow bash tools/barksim/run.sh day ja
suite birdsim.test                slow bash tools/birdsim/run.sh test
suite festivalsim.test            - bash tools/festivalsim/run.sh test
suite festivalsim.year            - bash tools/festivalsim/run.sh year 2026
suite festivalsim.at              - bash tools/festivalsim/run.sh at 2026-08-15 20.0
suite fishing.test                - bash tools/fishing/run.sh -v
suite railsim.test                - bash tools/railsim/run.sh test
suite railsim.table               - bash tools/railsim/run.sh table
# peoplesim reads the gitignored railway timetable when it is there: once with it, once without
if wanted peoplesim; then
	[ -f "$RAIL" ] || python3 tools/railway.py > /dev/null 2>&1
fi
suite peoplesim.test              - bash tools/peoplesim/run.sh test
suite peoplesim.town              - bash tools/peoplesim/run.sh town
suite peoplesim.day               - bash tools/peoplesim/run.sh day 6 2
suite peoplesim.summary           slow bash tools/peoplesim/run.sh summary
if wanted peoplesim.test.norail && [ -f "$RAIL" ]; then
	mv "$RAIL" "$RAIL.suites"
	trap 'mv -f "$ROOT/$RAIL.suites" "$ROOT/$RAIL" 2>/dev/null' EXIT INT TERM
	suite peoplesim.test.norail   - bash tools/peoplesim/run.sh test
	mv -f "$RAIL.suites" "$RAIL"
	trap - EXIT INT TERM
fi
suite sea.test                    - bash tools/sea/run.sh test
suite sea.days                    - bash tools/sea/run.sh days
suite sea.field                   - bash tools/sea/run.sh field
suite transportsim.test           - bash tools/transportsim/run.sh test
suite underwater.test             - bash tools/underwater/run.sh test
suite underwater.eco              - bash tools/underwater/run.sh eco -v
suite underwater.bed              slow bash tools/underwater/run.sh bed
suite weather.test                - bash tools/weather/run.sh test
suite weather.climate             - bash tools/weather/run.sh climate
suite weather.days                - bash tools/weather/run.sh days 2026 6 1 3
suite weather.winters             - bash tools/weather/run.sh winters
suite winter.test                 - bash tools/winter/run.sh test
suite winter.perf                 - bash tools/winter/run.sh perf
suite winter.images               - bash tools/winter/run.sh images

# --- 2. deterministic generators: their committed output must come out the same ---------------------------------------
# (Generators that read the built terrain - public_spaces, people_map, animal_map, bird_habitats, europa_site, railway,
# transport - give slightly other heights on a terrain built here than on the Mac's, so they are not judged here;
# railway and transport have their own --check.)
generator gen.city_plan       "$CONTENT/City/city_plan.json"                     python3 tools/city_plan.py
generator gen.academ_fountain "$CONTENT/Academgorodok/fountain.json"             python3 tools/academ_fountain.py
generator gen.academ_wires    "$CONTENT/BirdsData/wires.json"                    python3 tools/academ_wires.py
generator gen.restaurant_seats "$CONTENT/Restaurant/seats.json"                  python3 tools/restaurant/seats.py
generator gen.town_life       "$CONTENT/PeopleData $CONTENT/Localization/town"   python3 tools/town/town_life.py
generator gen.barks           "$CONTENT/Dialogue/town_barks.txt $CONTENT/Localization/barks" python3 tools/barks/build_barks.py
generator gen.strings         "$CONTENT/Localization/strings_ru.json $CONTENT/Localization/strings_en.json $CONTENT/Localization/strings_ja.json" \
	bash -c 'python3 tools/restaurant/strings.py && python3 tools/lobby_strings.py && python3 tools/smoking_strings.py && python3 tools/characters/nao_strings.py'
if wanted barks; then
	# build_barks.py prints PROBLEM for a condition no townsperson can meet
	if grep -q '^PROBLEM' "$LOGS/gen.barks.log" 2>/dev/null; then
		record gen.barks.conditions FAIL 0 "$(grep -c '^PROBLEM' "$LOGS/gen.barks.log") PROBLEM line(s), see gen.barks.log"
	else
		record gen.barks.conditions PASS 0 "every condition is met by somebody"
	fi
fi
suite railway.check           - python3 tools/railway.py --check
suite transport.check         - python3 tools/transport.py --check

# --- 3. the data's cross-checks and the string tables -----------------------------------------------------------------
suite check_data              - python3 tools/check_data.py
suite check_localization      - python3 tools/check_localization.py

# --- the table ----------------------------------------------------------------------------------------------------------
echo
echo "=================================================================================================="
printf '%-34s %-4s %6s  %s\n' "row" "" "time" "result"
for i in "${!NAMES[@]}"; do
	printf '%-34s %-4s %5ss  %s\n' "${NAMES[$i]}" "${RESULTS[$i]}" "${TIMES[$i]}" "${NOTES[$i]}"
done
echo "=================================================================================================="
echo "${#NAMES[@]} rows, $FAILS failed (logs: Saved/Suites/)"
exit $FAILS
