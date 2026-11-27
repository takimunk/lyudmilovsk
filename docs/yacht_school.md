# Yacht school (яхтенная школа)

Dima's ask (2026-11-27): a sailing school where schoolchildren and adults train on small training dinghies (the Soviet
«Луч» and the like): sail physics on the game's real wind, a helm the player works, lessons, races, animations, a daily
timetable. It opens in spring when it is warm (late April / May) and closes in November.

This file is the **contract** between the parts while they are written in parallel; each part's owner replaces its
section with the real description when done. Branch `yacht-school`.

## The school

- **ДЮСШ парусного спорта «Луч»** (children's and youth sailing school "Luch"; ja ヨット学校「ルーチ」). A boathouse
  (эллинг) on the beach by the SOUTH side of the pier's root (the banya is on the north side), a boat park with dinghies
  on launching trolleys, a slip of boards over the sand, a signal mast with flags, a windsock, a coach's launch.
- **Boat classes** (ids are fixed, used in code, data, mesh names):

  | id | Boat | Crew | LOA | Beam | Sail | Hull |
  |---|---|---|---|---|---|---|
  | `optimist` | «Оптимист», the children's pram, sprit sail | 1 child 7-12 | 2.30 m | 1.13 m | 3.3 m² | 35 kg |
  | `cadet` | «Кадет», two children, mainsail + jib | 2, ages 10-16 | 3.22 m | 1.27 m | 5.2 m² | 54 kg |
  | `luch` | «Луч», the single-hander (teens, adults, the hero) | 1 | 4.23 m | 1.37 m | 7.06 m² | 59 kg |

  The coach's launch `launch` is an aluminium motor boat («Казанка»-like, 4.6 m); race marks are orange inflatable
  cylinders `mark`; the start line is the launch (with a flag staff) and a pin buoy `pin`.
- **Season**: opening day = the first Saturday on or after April 25 (opening regatta and parade of sail, festival id
  `sail_opening`); closing = the first Sunday of November (closing regatta `sail_closing`, then the boats are laid up).
  Out of season the boats stand in the boathouse under covers, the coach is in on some days (theory, knots, repairs).
- **A day's go / no-go** (the signal mast shows it): daylight; mean wind 1.5-8 m/s for `optimist`, to 10 for `cadet`,
  to 12 for `luch`; no thunderstorm, no dense fog; sea ice gone. Too much wind = shore lesson (knots, theory at the
  blackboard, rigging on the grass). Tide: boats are rolled on their trolleys to the water's edge wherever the tide has
  it; sailing needs 0.5 m of water (centreboard half up under 1.0 m).
- **Groups** (real townsfolk; ids fixed): `kids` (optimists, ages 7-11), `juniors` (cadets and luches, 12-17),
  `adults` (luches, 18+), each with a coach; `race` = everyone on race days.

## Parts and owners

| Part | Files (only the owner edits them) |
|---|---|
| A. Sail physics, helmsman autopilot | `Source/Lyudmilovsk/Private/Sailing/SailModel.{h,cpp}`, `tools/sailsim/` |
| B. School rules: season, timetable, lessons, races, scoring, save; generator and data | `Private/Sailing/SailSchoolModel.{h,cpp}`, `tools/sailschoolsim/`, `tools/sailing/` (content.json, sailing.py), `Content/City/sailing.json` |
| C. Meshes | `blender/sailing/`, `unreal/scripts/import_sailing.py`, previews |
| D. Body clips and props | `blender/humans/` additions for sailing, `docs/yacht_school.md` "Clips" |
| E. Engine | `Public/Sailing/*.h`, `Private/Sailing/Lyud*.cpp` |
| F. People | activities, spots, members (idempotent add-on, never reroll the town) |
| G. Words, quests, stamps, items, shop, paper/radio, festival, dev mode, sounds | data + `tools/sailing/strings*.json` |

Rules for every part: plain-C++ models have NO Unreal types (namespace `LyudSail`), build with
`-Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Werror` under clang, are deterministic, and have a host test
`tools/<sim>/run.sh` (pattern: tools/kitesim, tools/winterplaysim). Frame: Unreal world, metres in the models, north =
+Y, the sea lies toward +X from the town's coast (compass: east = -X). The wind comes ONLY from
`LyudWeather::Regime/GustAt/HeightFactor` (model) / `ULyudWindSubsystem::GetWindAt(Location, HeightM, bOverWater=true)`
(engine): another thread is upgrading the wind tonight behind those calls, never edit Weather/ files. Waves and depth
from `ALyudSea::SampleSurface / DepthAt / SeaLevel`. Perf rules: docs/benchmark.md and the checklist (no per-frame mesh
rebuilds, moving meshes out of Lumen, pooled, nothing ticking far from the hero or out of season). Text: the
localization skill (ru/en/ja). Items: game-items skill. Dev mode: dev-mode skill.

## Shared names

- Namespace `LyudSail`; boat class enum `EBoat { Optimist, Cadet, Luch }`, ids as above.
- Engine: `ULyudSailing` (world subsystem, `Public/Sailing/LyudSailing.h`), `ALyudDinghy` (`Public/Sailing/LyudDinghy.h`),
  save object `ULyudSailingBook` ("Sailing"). Console `lyud.Sail.*`, dev group `sailing`.
- People conditions set by the engine (`UPeopleLifeSubsystem::SetCondition`): `sail_kids`, `sail_juniors`,
  `sail_adults`, `sail_race`, `sail_shore` (a shore lesson instead), `sail_season`.
- Place kind `sail_school`, plan-like ids `S-SAIL-SCHOOL` (boathouse), `S-SAIL-PARK` (boat park), `S-SAIL-SLIP`.
- Strings table `Content/Localization/sailing/strings_{ru,en,ja}.json`, keys `sail.*`; HUD/prompt keys `ui.sail.*`.
- Stamps collection `sailing`, quest ids `sail_*`, quest event `sail <what>`.
- Items: `sail_gloves`, `life_jacket` (worn), `sail_card` (the school's membership book), `knot_cord`.
- Meshes `/Game/Sailing/SM_Sail_<Name>`; sounds group `Sailing`, `audio/sfx/sailing/`.

## Sounds

All procedural and seeded (`tools/sailing/sounds.py`, numpy + scipy; nothing generated or paid for): 72 pools, 187
WAVs, ~10 min of sound, 48 kHz 16-bit mono, each with a `.json` sidecar (seed, parameters, levels, sha256, loop,
trigger, attenuation radius, priority). How they are made: cloth = trains of cloth snaps over a flutter of rustle bursts,
dacron crinkle and the sail's pressure thump; metal/wood/plastic = modal resonators (free-free tube partials for the
masts, bell partials as beating doublets, hull panel modes); water = hundreds of Minnaert bubbles over filtered noise
beds; the outboard = a two-stroke twin's firing train (two pops a turn) through an exhaust resonance, with the burble
out of the prop hub, piston rattle, gear whine and the idle's misfires. Loops are rendered on a circular buffer (events
wrap, slow curves periodic, FFT filters, whole numbers of cycles), then the seam is put where the level changes least.

- `python3 tools/sailing/sounds.py [words]` writes them to `audio/sfx/sailing/`; `--check` analyses every file (NaN,
  clipping, too quiet, DC, start/end clicks of one-shots, loop seams: the sample jump and the level step across the seam
  against the file's own onsets; prints peak, RMS, centroid and the low/high shares); `--table` prints the table below.
- **The WAVs are not committed from the cloud** (`*.wav` is LFS and the cloud can't push LFS objects): the branch has the
  script and the sidecars. On the Mac, regenerate them (deterministic; the sidecars' sha256 match with numpy 2.5.3 /
  scipy 1.18.1, another version may differ by a bit and that is fine), commit the WAVs to LFS, import.
- Mixing: `Private/Sailing/SailSound.h` (namespace `LyudSail::Sound`, plain C++, `tools/sailsoundsim/run.sh` checks it):
  - `FLoopMix MixLoops(const FTelemetry&)` - gain 0..1 and pitch 0.7..1.45 of the dinghy's nine loops (`ELoop`:
    luff light/mid/hard crossfaded by `FlogStrength` = luff x wind, leech, hull ghost -> chuckle -> plane, foil hum,
    cockpit slosh). `FTelemetry`: `SpeedMs` through the water, `Luff` 0..1, `HeelDeg`, `SheetLoad` 0..1, `bPlaning`,
    `WindMs`, `WaveM` (Hs round the boat), `WaterAboard` 0..1. `LoopPool(ELoop)` names the pool.
  - `unsigned StepEvents(FEventState&, const FTelemetry&, Dt, R1, R2, R3)` - the one-shots of this tick as `EEvent`
    bits (sail fill when the luff falls from a flog to drawing within 0.6 s, rope/mast creaks at most every 1.5 s, small
    clops / large slaps by speed and waves, spray with a large slap at speed in a breeze, rudder knock in a seaway);
    R1..R3 are the caller's seeded randoms; `EventPool(bit)`, `FillGain(wind)`, `SlapGain(T)`.
  - `FOutboardMix MixOutboard(rpm)` - the four engine loops (1000/1800/3000/4800 rpm) crossfaded at equal power only
    where both neighbours can be pitched (0.65..1.6) to the wanted rpm, so they meet in tune; `OutboardRpm()`.
  - `HalyardRate(wind)` - slaps a second of one parked boat's halyard (`sail_halyard_slap` one-shots, 0 under
    1.5 m/s, capped 6); `MixHalyards(wind)` - the light/fresh loops instead; `MixFlags(wind)` - the three small-flag
    loops, the windsock, their pitch.
  - Not covered by the header (the engine's own call): the gybe slam (boom angular speed), batten pop (a tack's luff
    peak), the capsize/sail-in-water/climb-in chain, the shore and the signals.
- The engine finds them through the soundscape's pools (`ULyudSoundscapeSubsystem::Pick("sail_...")`, group `Sailing`
  in `LyudSoundscape.cpp`'s list, up to 12 takes a pool); pool names all start with `sail_` because pool names are
  global. Masts: `sail_halyard_*_0N` are six masts at six pitches (372-694 Hz) - give each parked boat its own N, and the
  boat park far away one `sail_halyard_chorus_*` emitter (a dozen boats already mixed with distance).
- Import on the Mac:

  ```
  python3 tools/sailing/sounds.py && python3 tools/sailing/sounds.py --check
  IMPORT_ONLY=sfx/sailing UnrealEditor-Cmd unreal/Lyudmilovsk/Lyudmilovsk.uproject \
      -run=pythonscript -script=unreal/scripts/import_audio.py -unattended -nosplash
  git add audio/sfx/sailing/*.wav   # LFS; finished assets only
  ```

  -> `/Game/Audio/Sailing/S_<pool>_0N` (loops get `looping` from the sidecar); rebuild for the new pool names.
- Representative takes to listen to: `/mnt/project-files/yacht-school/sounds/` (the boat park's halyards, a hard flog,
  the chuckle, planing, a gybe, the bell, the outboard at 3000 rpm, a capsize).

| Pool (files) | s | Kind | Trigger | Radius m | Priority |
|---|---|---|---|---|---|
| **Sail** | | | | | |
| `sail_luff_light` (2) | 8.0 | loop | sail luffing a little (luff 0.15-0.5) or light air; gain/pitch from LyudSail::Sound::MixLoops | 25 | normal |
| `sail_luff_mid` (2) | 8.0 | loop | sail luffing in a moderate wind (head to wind, eased right out) | 40 | normal |
| `sail_luff_hard` (2) | 8.0 | loop | hard flogging in a blow (luff > 0.6 and wind > 7 m/s, or a capsized/head-to-wind boat in a gale) | 70 | high |
| `sail_fill` (4) | 1.3 | one-shot | the sail fills (luff drops from > 0.5 to < 0.2 within ~0.5 s): a whump | 35 | normal |
| `sail_gybe_slam` (4) | 2.0 | one-shot | a gybe, the boom slams across (boom angular speed past ~2 rad/s through the centreline) | 60 | high |
| `sail_leech` (1) | 4.0 | loop | leech flutter, a fast buzz of the sail's trailing edge, at speed (gain ~ speed, wind) | 15 | normal |
| `sail_batten_pop` (4) | 0.6 | one-shot | tacking, the battens pop through to the new side (after the luff peak of a tack) | 25 | normal |
| **Rigging** | | | | | |
| `sail_halyard_light` (6) | 16.0 | loop | loop per parked boat in light wind (2-5 m/s): a halyard tinking on its mast; 6 masts = 6 pitches | 35 | normal |
| `sail_halyard_fresh` (6) | 10.0 | loop | loop per parked boat in a fresh wind (> 5 m/s): halyards slapping fast; same 6 mast pitches | 45 | normal |
| `sail_halyard_slap` (6) | 2.4 | one-shot | one slap on a mast, for rate-driven play (LyudSail::Sound::HalyardRate(wind)); variant = mast pitch | 35 | normal |
| `sail_halyard_chorus_light` (1) | 20.0 | loop | loop at the boat park in light wind: a dozen boats' halyards (one emitter for the park) | 80 | low |
| `sail_halyard_chorus_fresh` (1) | 20.0 | loop | loop at the boat park in a fresh wind: a dozen boats' halyards slapping | 100 | low |
| `sail_block_click` (5) | 0.5 | one-shot | a pull on the mainsheet through the ratchet block (sheet shortened by ~0.2 m) | 12 | normal |
| `sail_block_run` (3) | 1.2-1.6 | one-shot | sheeting in hard hand over hand (a fast ratchet run) | 15 | normal |
| `sail_sheet_run` (3) | 1.5 | one-shot | the sheet runs out through the block (eased fast or let fly) | 15 | normal |
| `sail_cleat_snap` (4) | 0.4 | one-shot | a sheet or the vang snapped into a cam cleat (or flicked out) | 12 | normal |
| `sail_rope_creak` (4) | 0.7-1.3 | one-shot | a loaded sheet creaking on the block/fairlead (sheet load rising past ~0.7, ~1 per 3 s) | 10 | normal |
| `sail_mast_creak` (4) | 1.2-1.9 | one-shot | the mast groaning in its step under load (heel or gust peak, ~1 per 5 s) | 15 | normal |
| `sail_rudder_knock` (4) | 0.5 | one-shot | the rudder's pintles knocking in the gudgeons (a wave under the stern, a sharp helm) | 12 | normal |
| `sail_dagger_down` (3) | 1.0 | one-shot | the daggerboard slid down in its case until it seats | 15 | normal |
| `sail_dagger_up` (3) | 1.2 | one-shot | the daggerboard pulled up (shallow water, coming ashore) | 15 | normal |
| **Hull & water** | | | | | |
| `sail_hull_ghost` (1) | 8.0 | loop | ghosting, speed 0.2-1.2 m/s: water trickling and lapping at the bow | 12 | normal |
| `sail_hull_chuckle` (1) | 8.0 | loop | sailing at displacement speed 1-3 m/s: the bow wave chuckling | 20 | normal |
| `sail_hull_plane` (1) | 6.0 | loop | planing (speed > ~3.2 m/s, planing flag): hiss and roar under the hull | 40 | high |
| `sail_hull_hum` (1) | 4.0 | loop | planing fast, the foils humming (gain ~ speed past 3.5 m/s, pitch ~ speed / 4.5) | 10 | normal |
| `sail_wave_slap_small` (5) | 0.8 | one-shot | a small wave clops on the hull (at anchor, ashore in the shallows, ghosting) | 15 | normal |
| `sail_wave_slap_large` (4) | 1.6 | one-shot | the bow drops into a wave (pitching hard, upwind in a chop) | 35 | normal |
| `sail_spray` (4) | 1.5 | one-shot | a sheet of spray over the bow (wave slap large at speed > 2.5 m/s, wind > 6) | 20 | normal |
| `sail_cockpit_slosh` (1) | 6.0 | loop | water sloshing in a swamped cockpit (gain ~ water aboard) | 12 | normal |
| `sail_bail` (4) | 1.7 | one-shot | one scoop of the bailer: dipped, filled, thrown over the side | 15 | normal |
| `sail_capsize` (3) | 4.5 | one-shot | the boat capsizes (heel past ~70 deg): sail hits the water, crew falls in, the big splash | 80 | high |
| `sail_hit_water` (3) | 1.3 | one-shot | the sail slaps flat onto the water (a capsize to 90 deg, a dunked boom) | 40 | normal |
| `sail_climb_in` (3) | 3.0 | one-shot | a sailor climbing back aboard, dripping (after a righting, from a swim) | 15 | normal |
| `sail_ground_scrape` (3) | 1.8 | one-shot | the hull grounds on sand / runs up the beach (depth < board or hull draft while moving) | 20 | normal |
| `sail_bump_plastic` (4) | 0.7 | one-shot | hull bumps another boat or the trolley (GRP/plastic thud) | 20 | normal |
| `sail_bump_wood` (4) | 0.7 | one-shot | hull bumps the pontoon / the slip's boards (plywood thud) | 20 | normal |
| **Shore** | | | | | |
| `sail_trolley_boards` (1) | 6.0 | loop | a launching trolley rolled over the slip's boards (pitch ~ walking speed / 1.2 m/s) | 25 | normal |
| `sail_trolley_sand` (1) | 6.0 | loop | the trolley rolled over wet sand to the water's edge | 20 | normal |
| `sail_hull_drag` (3) | 1.6-2.0 | one-shot | a hull heaved a metre over the sand (no trolley; kids pulling an optimist up) | 20 | normal |
| `sail_hose` (1) | 6.0 | loop | a hose washing the salt off a hull at the boat park (after sailing) | 20 | normal |
| `sail_sail_roll` (3) | 2.5 | one-shot | a sail being rolled up / unrolled on the grass (dacron crackle) | 12 | normal |
| `sail_mast_step` (3) | 1.6 | one-shot | rigging: the mast dropped into its step (clunk, then the shrouds and halyard rattle) | 30 | normal |
| `sail_boathouse_open` (2) | 3.6 | one-shot | the boathouse's big sliding door rolled open (morning, before the first lesson) | 50 | normal |
| `sail_boathouse_close` (2) | 3.6 | one-shot | the boathouse door rolled shut with a bang (evening) | 60 | normal |
| `sail_buckle` (4) | 0.3 | one-shot | a life jacket's buckle clicked shut (putting on / taking off) | 6 | low |
| `sail_flag_squeak` (3) | 2.2 | one-shot | a signal flag hoisted on the signal mast (the pulley squeaking hand over hand) | 25 | normal |
| `sail_flag_light` (1) | 6.0 | loop | a small flag (signal mast, the launch's staff) in light wind 2-5 m/s | 15 | normal |
| `sail_flag_mid` (1) | 6.0 | loop | a small flag in a fresh wind 5-9 m/s | 20 | normal |
| `sail_flag_strong` (1) | 6.0 | loop | a small flag snapping in a strong wind > 9 m/s | 30 | normal |
| `sail_windsock` (1) | 8.0 | loop | the windsock rustling on its pole (gain ~ wind from 2 m/s) | 12 | low |
| **Signals** | | | | | |
| `sail_whistle_short` (3) | 1.4-1.5 | one-shot | the coach's whistle, one short blast (attention, 'go') | 150 | high |
| `sail_whistle_long` (2) | 2.4-2.7 | one-shot | one long blast (everybody, come in to the slip) | 150 | high |
| `sail_whistle_recall` (2) | 2.7-2.8 | one-shot | three blasts (recall / a boat in trouble) | 150 | high |
| `sail_horn_short` (2) | 3.0-3.1 | one-shot | start signal horn, short blast (the start, a minute signal) | 400 | high |
| `sail_horn_long` (2) | 4.4-4.8 | one-shot | start signal horn, long blast (the warning signal, finish of the first boat) | 400 | high |
| `sail_bell_single` (2) | 5.5 | one-shot | the school's bell by the boathouse, one stroke (half-hours of the timetable) | 200 | high |
| `sail_bell_double` (2) | 6.0 | one-shot | the bell, a double stroke ding-ding (lesson start/end, the hours in pairs) | 200 | high |
| `sail_megaphone_click` (3) | 0.5 | one-shot | the coach's megaphone keyed (before and after the words; the words are voice lines) | 40 | normal |
| `sail_megaphone_squeal` (2) | 1.3 | one-shot | megaphone feedback squeal (pointed the wrong way; comic beat) | 80 | normal |
| `sail_start_clap` (3) | 2.6 | one-shot | the starter's clap (a starting-pistol-like report) instead of the horn | 300 | high |
| `sail_stopwatch` (1) | 2.0 | loop | the coach's stopwatch ticking (5 beats/s) for the last 10 s of a start sequence (HUD) | 3 | low |
| **Launch** | | | | | |
| `sail_outboard_idle` (1) | 4.0 | loop | the coach's launch outboard («Вихрь»-like two-stroke twin) idling, ~1000 rpm | 60 | normal |
| `sail_outboard_low` (1) | 4.0 | loop | 1800 rpm (slow along the course); crossfade by pitch, see LyudSail::Sound::MixOutboard | 100 | normal |
| `sail_outboard_mid` (1) | 4.0 | loop | 3000 rpm (cruising to a mark) | 150 | normal |
| `sail_outboard_high` (1) | 4.0 | loop | 4800 rpm (flat out, planing to a capsized boat) | 200 | high |
| `sail_outboard_pull_fail` (3) | 1.8 | one-shot | a pull of the starter cord that doesn't catch (cold engine: 1-3 of these first) | 40 | normal |
| `sail_outboard_pull_catch` (2) | 3.5 | one-shot | a pull that catches: it sputters up to idle; crossfade into sail_outboard_idle at the end | 60 | normal |
| `sail_outboard_off` (2) | 2.4 | one-shot | the kill cord pulled at idle: the last coughs, the engine coasting down | 60 | normal |
| `sail_alu_slap` (4) | 1.2 | one-shot | the launch's aluminium hull banging on a wave at speed (rate ~ speed x chop) | 80 | normal |
| **Crowd** | | | | | |
| `sail_crowd_cheer` (2) | 5.0 | one-shot | the pier cheering a finish (claps building, a couple of whistles) | 120 | high |
| `sail_prize_claps` (3) | 4.5-5.5 | one-shot | polite applause at the prize-giving (a name read out, a cup handed over) | 60 | normal |
| `sail_fanfare` (2) | 3.6 | one-shot | a brass fanfare sting (regatta opening/closing, a cup awarded) — three trumpets | 150 | high |
