#!/bin/sh
# Everything the public spaces need checked once rendering works, in one go (docs/city/public_spaces.md "Checking"):
#   1. every shot of test_public_spaces.sh (day, night, seasons, close-ups of the kit), copied to Saved/shots/
#   2. the frame-time A/B in one process, alone on the machine: everything shown vs hidden (day and night) and the
#      street lights 12 vs 0 vs 8 at night (CITY_AB=1, -ueexclusive)
#   3. the bench route at Epic with and without the city (tools/bench_systems.sh nocity), alone
# Run detached:  ( nohup unreal/scripts/public_spaces_verify.sh > unreal/Lyudmilovsk/Saved/Logs/ps_verify.txt 2>&1 & )
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/Saved/shots"
mkdir -p "$OUT"
echo "== shots"
nice -n 19 "$ROOT/unreal/scripts/test_public_spaces.sh"
cp "$ROOT"/unreal/Lyudmilovsk/Saved/CityShots/*.png "$OUT/" 2>/dev/null
grep -E "CITYTEST (FAIL|done)" "$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_public_spaces.log"
echo "== A/B (exclusive)"
CITY_AB=1 CITY_EXTRA=-ueexclusive CITY_SHOTS=ab_on,ab_off,ab_on2,ab_off2,abn_on,abn_off,abn_on2,abn_off2,abl_on,abl_off,abl_on2,abl_off2,abl8 \
    CITY_WAIT=12 nice -n 19 "$ROOT/unreal/scripts/test_public_spaces.sh" >/dev/null
grep -E "CITYSHOT ab" "$ROOT/unreal/Lyudmilovsk/Saved/Logs/test_public_spaces.log" | sed -E 's/.*CITYSHOT ([a-z0-9_]+) at.*: ([0-9]+ frames, median [0-9.]+ ms, worst [0-9.]+ ms).*/\1 \2/' \
    | tee "$OUT/ab_frames.txt"
echo "== bench route, with and without the city"
"$ROOT/tools/wait_ue_queue.sh" 7200 --exclusive && nice -n 19 "$ROOT/tools/bench_systems.sh" nocity | tee "$OUT/bench_nocity.txt"
ls "$OUT"
