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

## School rules and data (part B)

Single source: `tools/sailing/content.json` (written by hand). `python3 tools/sailing/sailing.py` places the school on the
terrace, lays the water areas and writes `Content/City/sailing.json` (the model's only input),
`Content/Dev/teleports/sailing.json` and `tools/sailing/strings_needed.json`; `--check` fails when a committed file is
stale or a footprint collides. The rules are `Private/Sailing/SailSchoolModel.h` (header only, plain C++17, namespace
`LyudSail`, no Unreal types; reads the JSON with `People/MiniJson.h`). Its names are prefixed `FSchool* / FRace* /
FLesson* / FSeason* / FTimetable*` and `School* / Race* / Season* / Lesson*` so they cannot meet part A's `SailModel`.
Boat classes are the ints `SchoolOptimist = 0, SchoolCadet = 1, SchoolLuch = 2` in `EBoat`'s order (a class set is a
bit per class, 7 = all). Tests: `tools/sailschoolsim/run.sh` (451 checks on the real data; `week Y M D` prints a week's
timetable, `season 2026` a resolved season's table).

Frame everywhere: Unreal world metres, +X to sea (compass west), +Y north. A wind is given as the vector it blows
**to**: `SchoolWindTo(fromCompassDeg)`. The frame is mirrored, so "left" of V is `(V.Y, -V.X)` and a mark left to port
is an anticlockwise turn on the map.

### Season

| | rule | 2026 | 2027 | 2028 |
|---|---|---|---|---|
| fitting out | the 14 days before the opening, 10-17 | Apr 11-24 | Apr 17-30 | Apr 15-28 |
| opening regatta | first Saturday on or after April 25 | Apr 25 | May 1 | Apr 29 |
| summer timetable | June 1 - August 31 | | | |
| children's Optimist regatta | June 1 | | | |
| Кубок Людмиловска | the weekend on or after June 20, two days | Jun 20-21 | Jun 26-27 | Jun 24-25 |
| closing regatta | first Sunday of November | Nov 1 | Nov 7 | Nov 5 |
| laying up | the 7 days after the closing | | | |

`ESeasonState { LaidUp, FitOut, Open, LayUp }`. Out of season the coach is in on Tuesday, Thursday and Saturday 15-18
(theory evenings: knots, rules). Sea ice closes the water whatever the date (`SeasonOpen(P, Date, bSeaIce)`).

### Go / no-go and the signal mast

Decided once at a session's start (`SchoolDecide`), not re-decided; afloat, `SchoolMustRecall` says when to bring them in.

| | optimist | cadet | luch |
|---|---|---|---|
| mean wind, m/s | 1.5 - 8 | 1.5 - 10 | 1.5 - 12 |
| gust keeps ashore over | 11 | 13 | 15 |
| recalled when the mean passes | 9.5 | 11.5 | 13.5 |

Also ashore: thunder, visibility under 400 m, dark (boats are ashore half an hour before sunset; lat 54.95, lon 20.3,
UTC+2, no summer time assumed), sea ice, low water (below). Storm: mean 14 or a gust of 19, the cone goes up. Luches reef
from 9 m/s (`FSchoolGo::bReef`). Low water: the trolleys are rolled over the flat to the first 0.5 m of water with no bar
shallower than 0.3 m beyond, at most 260 m from the slip's foot; the children's area needs 0.8 m, else the children are
towed out to the race area if the wind is under 6 m/s (`bTowOut`), else they stay ashore.

Mast states (`SchoolSignalAt`, flags top first, horn blasts on entering): `closed` (bare), `fitout` (flag_school),
`shore` (flag_school, flag_shore), `wait` (flag_school, flag_ap; 2), `sail` (flag_school + the class flags afloat:
flag_optimist / flag_cadet / flag_luch; 1), `race` (flag_school, flag_race + class flags; 1), `recall` (flag_school,
flag_recall; 3), `storm` (storm_cone). The coach launch's errand: `SchoolLaunchTask` (keep station; right a boat capsized
over 60 s, a child's over 20 s, children first; shepherd a boat more than 40 m out of the water's bounds; on a recall tow
the children home).

### Timetable

| | term (school days) | summer (Jun 1 - Aug 31) |
|---|---|---|
| kids (optimists, `coach_kids`) | Tue Thu 15:00-17:30, Sat 10:00-12:30 | Mon-Fri 10:00-12:30, Sat 10:00-12:30 |
| juniors (cadets + 3 luches, `coach_juniors`) | Wed Fri 15:00-17:45, Sat 13:00-16:00 | Mon-Fri 13:30-16:30, Sat 13:00-16:00 |
| adults (luches, `coach_head`) | Tue Thu 18:00-20:30, Sat 16:30-19:00 | Tue Thu 18:30-21:00, Sat 16:30-19:00 |
| Wednesday evening races (luch, 2 races, windward-leeward) | Wed 18:00-21:00 | the same |
| Sunday championship (all classes, 2 races, triangle) | Sun 11:00-15:00 | the same |

One launch, so no two sessions are afloat at once. A day off (the game's public holiday) has no lessons, only hire. The
opening, the closing, June 1 and the cup take the whole day (no lessons). A session whose time on the water does not fit
the daylight is held in the classroom (`Kind == "theory"`, `bDark`); a dark Wednesday race is simply not sailed.

A session's phases, each with a place key and an activity name for the people's part to map:

| kind | phases (minutes; `water` takes the rest) |
|---|---|
| lesson | gather 10 `changing` sail_change, rig 15 `rigging` sail_rig, launch 10 `slip_head` sail_launch, water `water:<area>` sail_drill, return 10 `slip_foot` sail_land, derig 15 `wash` sail_derig, debrief 10 `classroom_seat` sail_tea |
| shore (the decision was no-go) | gather 10 `changing`, shore `shore_pupil` sail_shore_lesson, debrief 15 `classroom_seat` |
| theory | gather 10 `boathouse_door` sail_arrive, theory `classroom_seat` sail_theory, debrief 15 `tea_table` |
| race | briefing 20 `notice_board` sail_briefing, rig 20, launch 15, water sail_race, return 15, derig 20, prizes 20 `podium` sail_prizes |
| recall (replaces the rest after the recall hour) | return 15, derig 15, shore `classroom_seat` sail_theory |
| fitout / layup | `paint_trestle` sail_paint / `boat_door` sail_layup |

The time on the water is cut into 2 drill blocks; `FSchoolNow::Drill` is one of `follow, triangle, tack_whistle,
figure8, capsize, starts, free` (by the group's list, the wind, the date; `capsize` only in water of 15 °C and wind
under 6), `Topic` on a shore lesson one of `knots, rigging, theory, rules`.

Open hire (a certificate holder, 50 a time, the boat `luch_9`): in season from 10:00 to an hour before sunset when
luches may sail, no regatta is on and no session has the luches out (`SchoolHireOpen`).

### Places

Boathouse, park and slip stand on the beach in the 25 m between the loungers and the pier, by the south side of the
pier's root, turned 4.5° with the shore. Terrace frame of `tools/city_plan.py` (s = metres inland from the high-water
line, y = map Y; UE y = map y + 7.9):

| id | what | s | map y | UE centre | size |
|---|---|---|---|---|---|
| `S-SAIL-SCHOOL` | boathouse; ridge points to sea; both doors in the north wall (boat door 3.2 m at s 16.1, people's door 1.1 m at s 21.0); classroom is the inland end s 18.2-23.5; wall 3.4 m, ridge 5.6 m, floor z -42.06 | 7.0 - 23.5 | 873.7 - 882.7 | (148.95, 886.65) | 16.5 x 9.0 |
| `S-SAIL-PARK` | fenced park (1.5 m), gate 3.4 m in the seaward side; an aisle along the boathouse wall, then three rows of trolleys | 8.5 - 23.5 | 882.7 - 896.5 | (149.10, 898.07) | 15.0 x 13.8 |
| `S-SAIL-SLIP` | plank slip from the gate into the water | 8.5 - -14.0 | 882.9 - 886.3 | head (156.18, 892.50), foot (178.61, 890.74) | 22.5 x 3.4 |

The contract's 20 x 15 m park does not fit between the loungers and the pier with the boathouse: it is 15 x 13.8 and holds
all 22 hulls (`school.park_slots`: 8 optimists in two ranks, 4 cadets, 9 luches, the launch on its trolley);
`school.hall_slots` are the same boats' winter places in the boathouse (optimists racked in tiers). The pier's deck edge
(map y 899) is 1.2 m clear of the fence. `sailing.py --check` proves no footprint overlaps a plot, stair, spot, prop,
collision box, the banya, the seawall or the pier; five soft slots of the people's data fall inside and are listed in
`school.displaced` for part F to move: `S-BEACH` sit_ground at (156.01, 883.9) and (153.46, 897.2), `S-TOWEL-07` (its spot
and both lie slots, about (155.7, 892.9)).

Place keys (`school.points`, each `pos` [x, y, z] in UE metres and `yaw`; a key with several slots has `slots`):
`entrance`, `gate`, `boathouse_door`, `boat_door`, `notice_board`, `bosun`, `wash` (3), `classroom`, `classroom_seat`
(12), `blackboard`, `tea_table`, `changing` (6), `rigging`, `signal_mast` (158.30, 895.14), `coach_mast`, `windsock`
(on the ridge), `coach_slip`, `slip_head`, `slip_foot`, `shore_lawn`, `shore_pupil` (8), `shore_boat`, `coach_shore`,
`knot_bench` (4), `paint_trestle` (3), `podium` (158.78, 896.21), `spectators_beach` (10, south of the boathouse),
`spectators_pier` (16, the pier's south railing at (240.9, 907.8)), `spectators_promenade`.

Dev teleports (`Content/Dev/teleports/sailing.json`): `sailing.school`, `sailing.park`, `sailing.slip`, `sailing.mast`,
`sailing.pier_view`.

### Water

Areas are polygons in `water.areas` following the shore (d = metres seaward of the high-water line):

| id | d | map y | min depth | for |
|---|---|---|---|---|
| `corridor` | -2 - 130 | 872 - 888 | 0.5 | out from the slip and home |
| `kids` | 90 - 370 | 700 - 876 | 0.8 | optimists, the hero's first lessons |
| `race` | 360 - 850 | 480 - 880 | 1.2 | cadets, luches, the regattas |

All lie south of the pier and keep clear (`water.keep_clear`, checked by the generator and the tests): 8 m off the
pier's deck and 28 m off its head, 6 m off the rowing boats' mooring line, 20 m off the winter ice hole, 300 m off the
railway's bank, 500 m off its island, 60 m off the beach (the bathers' strip; the corridor alone crosses it). Model:
`SchoolInside`, `SchoolEdgeDistance`, `SchoolNearestInside(Area, Pt, Margin)`, `SchoolInsideWater(P, Pt, &AreaId)`,
`SchoolNearestWater`, `SchoolAreaAfloat(P, Area, TideM)`.

Launch line: from (164.66, 891.83) along (0.997, -0.078). The bed along it is stored every 5 m (`water.launch.bed`).
**It is a guess from the macro relief** (`"bed_source": "macro"`): the trolleys reach water 16 m out at high water, 91 m
at -1 m, 167 m at neap low (-2 m), 246 m at -3 m; a little lower the haul from the slip's foot passes 260 m and nobody
launches; the
children's area is dry at spring low. In the engine use `SchoolLaunchPointSampled(P, DepthAt)` with `ALyudSea::DepthAt`
so the real bed decides; `SchoolLaunchPoint(P, TideM)` is the offline answer (`FSchoolLaunch::bGuess`).
`SchoolBoardDown(P, Depth)` is how far the centreboard can go down (all the way in 1 m of water, half in 0.5 m, up in less).

### Racing

Course (`RaceSetCourse(P, Kind, Class, WindTo, WindSpeed, Boats, Seed, TideM, AreaId = "", Laps = 0)`, the same for the
same `RaceSeed(Date, Event, Race, Class)`): square to the wind, inside the class's area with 15 m to spare and the
area's depth under every mark, shrunk and moved until it fits (`bOk` false: no race). Beat at 6 m/s: optimist 130 m,
cadet 220, luch 300, scaled 0.6 + 0.4 x wind / 6. `windward_leeward`: mark 1 to windward, a leeward mark by the line,
two beats. `triangle`: windward, wing (45° off, port side), leeward. `kids_triangle`: 70 m legs, two laps.
`figure8` (drill, no line): two buoys 50 m apart across the wind, rounded opposite ways. All marks to port. The line:
1.5 boat lengths a boat, at least 24 m, the launch at the starboard end.

Start (`RaceStartSequence`): -180 s the class flag and a horn, -60 s flag P and a long horn, 0 both down and a horn.
`FRaceTracker::Step(P, Course, T, Pos, HeadingDeg)` reads one boat's track (T from the gun) and returns events:
`Ocs` (over at the gun; `OcsCleared` when wholly back), `Started`, `Rounded`, `WrongSide` (must be unwound: a winding
count), `Missed`, `Touch` (within 1.1 m of a mark: one full turn inside 90 s, `PenaltyDone`, or DSQ at the finish),
`FinishRefused`, `Finished`, `Dns` (not started 240 s after the gun). `Close()` scores what is left (DNS, DNF, OCS).
More than half the fleet over: `RaceGeneralRecall`. Time limit for the first boat 25 / 30 / 35 min by class, 10 min
after it for the rest, 10 min between races.

Scoring: low point (`RaceScore`: place = points, ties share the mean; DNF, DNS, DSQ, OCS = entries + 1). Series
(`RaceSeriesTable`): 1 discard from 4 races, 2 from 9, 3 from 16; ties by the better scores, then the last race. Series
this season: `wednesday` (luch), `sunday` (each class, the championship; final tables in the news on the closing day),
`cup_lyudmilovsk` (2 days x 3 races, entry 20, the stamp for a podium), single days `sail_opening`, `sail_closing`,
`optimist_day`. Races nobody watches: `RaceResolve(P, Seed, Class, RaceEntries(P, Class), Wind, Gust, CourseLengthM)`
(skill + luck + a taste for wind; the better sailor wins more often, not always; more retire in a blow).

### The hero

Joining: a talk with the head coach, 150, the item `sail_card` (`SchoolCanJoin`, `SchoolJoin`). Lessons in order with
the duty coach, from 10:00 to 1.5 h before sunset on a season day when luches may sail, not during a regatta
(`LessonAvailable` gives the reason when not); `shore` any day the coach is in, also out of season.

| # | id | objectives (read by `FLessonRun::Step` from `FLessonTelemetry`) | wind |
|---|---|---|---|
| 0 | `shore` | 3 knots, the boat's parts, rig the boat (`Notify(P, "knot" / "parts" / "rig")`) | any |
| 1 | `first_sail` | follow the launch within 25 m for 90 s; hold a beam reach 60 s | 1.5 - 7 |
| 2 | `tacking` | hold close-hauled 45 s; 5 tacks without hanging in irons | 2 - 8 |
| 3 | `points` | 30 s each: close-hauled, beam reach, broad reach, run | 2 - 9 |
| 4 | `gybing` | 2 gybes without a capsize; 2 figure-eights | 2 - 8 |
| 5 | `capsize` | capsize and right the boat twice, each inside 90 s (warm water only) | 1.5 - 6 |
| 6 | `irons` | make sternway 3 s; stop within 4 m of the buoy | 2 - 8 |
| 7 | `mob` | pick up the buoy twice (within 1.8 m, under 1 m/s) | 2 - 9 |
| 8 | `landing` | a lap of the small triangle; land at the slip under 1.2 m/s | 2 - 9 |
| 9 | `racing` | start within 5 s of the gun; a windward-leeward lap (needs the certificate) | 2.5 - 10 |

Ranks: `none`, `yunga` (lesson 0), `helm3` (lessons 0-8: **the certificate**, hire alone and enter races), `helm2`
(lesson 9, 5 races, 20 miles), `helm1` (15 races, 60 miles, 3 podiums). The logbook `FSchoolLog` (member, lessons,
rank, races, wins, podiums, miles, hours, capsizes, the hire boat's wear, stamps, this season's series and the last race
day's tables) saves as one line of tokens: `Save()` / `Load()` (an empty log is `v1 0 -1 0 0 0 0 0 0 0 0 0 0 -1 -1 0 0 0
0`); a new year clears the series.

### Engine API (call order)

```
FSchoolParams P;  P.Load(JsonText, Error);                       // once
// every minute or so
SchoolConditionsAt(P, Date, Hour, bDayOff, bSeaIce, &States)     // -> sail_season, sail_kids, sail_juniors, sail_adults, sail_race, sail_shore (+ bFitOut)
SchoolSessionAt(P, Date, Hour, bDayOff, &States, &Weather)       // -> FSchoolNow: group, coach, phase, place key, activity, drill, area, race number
SchoolSignalAt(P, Date, Hour, Weather, bDayOff, &States)         // -> the mast's flags and horn
// at a session's start (FSchoolNow::Key changes): decide and remember
FSchoolGo Go = SchoolDecide(P, Date, Weather, Session.Classes);  // push FSchoolSessionState{Key, Go.Go, Go.Reason, -1}
// while afloat
SchoolMustRecall(P, Weather, Go, ClassRecall)                    // != None: set RecallHour in the state; SchoolNoteRecall(P, Log) if the hero was out
SchoolLaunchTask(P, Boats, LaunchPos, Station, bRecall, Home)    // the coach launch
SchoolLaunchPointSampled(P, DepthAt)                             // where the trolleys stop today
// a race in front of the player
RaceSetCourse(...); RaceStartSequence(P, Class); FRaceTracker per boat: Step(...) each tick, Close() at the time limit
RaceScore(Results); SchoolRecordRace(P, Log, Date, Event, Class, Results, Wind)
// at the end of a race day (or the next morning): everything not sailed in front of the player
SchoolResolveDay(P, Log, Date, Weather, WindTo, bDayOff, &Events)   // idempotent; returns SchoolNewsOfDay
// the hero
SchoolCanJoin / SchoolJoin; SchoolNextLesson; LessonAvailable; FLessonRun::Begin, SetCourse, Step; SchoolCompleteLesson
SchoolCanHire / SchoolHireOpen / SchoolHire; SchoolCanEnter; SchoolAddSailing(P, Log, Metres, Hours, Capsizes); SchoolMend
```

Every log-changing call returns `FSchoolEvents { Stamps, Quest, NewRank }`: give the stamps (collection `sailing`), fire
each quest event, announce the rank.

### Hooks for the other parts

- **Radio and paper**: `SchoolForecastForSailors(P, Date, Hours, bDayOff)` (the morning line: key `sail.radio.sail /
  reef / race / shore / storm / closed`, the reason, the best hour, the groups that will sail, today's regatta) and
  `SchoolNewsOfDay(P, Log, Yesterday)` (per class: key `sail.news.result / hero_won / hero_place / abandoned / opening /
  closing / cup`, the first three slots, the hero's place, the races, the wind; `bSeries` for a final table).
- **Spectators**: `SchoolSpectatorPull(P, Date, Hour, bDayOff, &Place)`: 0.25 Wednesday, 0.5 Sunday (`spectators_pier`),
  0.9 June 1 and the closing, 1.0 the cup, 1.2 the opening (`spectators_beach`), only while the boats are out.
- **Festivals**: the opening and the closing name the festival ids `sail_opening`, `sail_closing`, June 1 names
  `children_day`; the dates come from `SeasonOpening / SeasonClosing`.
- **People (F)**: groups `kids`, `juniors`, `adults`, `race`; staff slots `coach_head` (also the race officer),
  `coach_kids`, `coach_juniors`, `bosun`; sailor slots `kids_1..8` (optimists), `jun_c1..4` with crews `jun_x1..4`
  (cadets), `jun_l1..3` and `adult_1..5` (luches), each with a skill and a taste for wind in `fleet.sailors`; `person` is
  left for F to bind. Place keys and activity names are in the phase table above; the five displaced slots are in
  `school.displaced`.
- **Words (G)**: every key the rules and data name is in `tools/sailing/strings_needed.json` (275, each with a note):
  `sail.lesson.<id>.title / intro / obj<n> / hint<n> / done / fail`, `sail.tutor.*`, `sail.signal.*`, `sail.reason.*`,
  `sail.phase.*`, `sail.drill.*`, `sail.shore.*`, `sail.group.*`, `sail.rank.*`, `sail.boat.*`, `sail.staff.*`,
  `sail.sailor.*`, `sail.race.*`, `sail.status.*`, `sail.join.*`, `sail.hire.*`, `sail.radio.*`, `sail.news.*`,
  `ui.sail.*`, `city.coll.sailing*`, `festival.sail_*`, `dev.tp.sailing.*`. When `tools/sailing/strings.json` exists,
  `sailing.py --check` fails on a key it lacks.
- **Stamps** (collection `sailing`): `joined`, `first_sail`, `capsize`, `certificate`, `helm2`, `helm1`, `first_race`,
  `race_win`, `opening`, `closing`, `cup_lyudmilovsk`, `miles_10`, `miles_100`, `recall`.
- **Quest events**: `sail join`, `sail lesson <id>`, `sail rank <id>`, `sail certificate`, `sail hire`, `sail race`,
  `sail race_win`, `sail podium`, `sail cup <series>`, `sail capsize`, `sail recall`, `sail miles <n>`, `sail opening`,
  `sail closing`.
- **Items**: only `sail_card` is named by the rules (given on joining).

### To rerun on the Mac

The bed is the macro relief's. With the built terrain (`Saved/World/terrain_out.bin`):

```
python3 tools/sailing/sailing.py --built     # ground and bed heights from the built terrain; commit the result
tools/sailschoolsim/run.sh                   # the launch distances and the areas' depths are checked again
```

Then look at the school in the editor (`sailing.school`): the boathouse floor against the sand, the slip's foot against
the water at datum, the fence against the pier's piles; and check that the steamship's approach to the pier's head (not
known to this part) stays outside `water.areas`.

## Sail physics (part A)

`Private/Sailing/SailModel.{h,cpp}`, namespace `LyudSail`, plain C++17, no Unreal types, deterministic (its own
random numbers from `Seed`), no allocations in any step. Host test `tools/sailsim/run.sh` (rows `sailsim.quick`,
`sailsim.test` (slow, 1.5 min), `sailsim.polar` in `tools/run_all_suites.sh`).

### Frame and signs

World metres, north = +Y. `Yaw` is the Unreal yaw in RADIANS: forward = (cos, sin), starboard = (-sin, cos). Every
signed side value is + to starboard: `Heel`, `Boom`, `Jib`, `Hike`, `Rudder` (+ turns her to starboard when going
ahead), wind angles. True wind angle `Twa = WrapAngle(WindFromYaw - Yaw)`: + = wind over the starboard side =
starboard tack (the boom then lies to port, negative). Wind is a velocity (blows TOWARD), m/s. Speeds m/s, forces N,
angles rad, depth and wave height m.

### What is modelled

- **Boat**: 3 DOF on the water (surge, sway, yaw) + heel; pitch and wave roll are taken from the sea normal the caller
  passes. Inner step at most 1/60 s (up to 8 substeps a call, so a 4 fps frame is safe).
- **Sail**: apparent wind at the centre of effort; the boom swings toward the weathercock angle and is stopped by the
  sheet (`Sheet` 0 = hard in, 1 = right out; boom limit `BoomMin..BoomMax`). Soft sail: nothing under 8 degrees of
  attack (it flaps: `MainLuff` -> 1), full at 16, trimmed at 18, stalls past 21 into flat-plate drag. So she cannot
  sail closer than about 40-47 degrees, is fastest on a beam / broad reach, slower and rolly dead downwind. By the lee
  past 24 degrees the boom slams across by itself (`EvGybe | EvBoomSlam`, `BoomSlamSpeed` rad/s). The jib (cadet)
  follows the mainsheet unless `JibSheet >= 0`. In light air (under about 1.2 m/s over the sail) the cloth hangs.
- **Hull**: skin friction + a wave-making hump at Froude 0.4 + planing for the cadet and the Luch (`EvPlaningStart` /
  `EvPlaningEnd`, `Planing` 0..1), extra resistance in waves (more head-on), more astern. Centreboard and rudder are
  real foils (lift, induced drag, stall; work reversed when going astern): leeway 2.5-4.5 degrees close-hauled with
  the board down, several times more with it up (optimist 16). Rudder force goes with speed squared: none when
  stopped. Heel gives weather helm. A slow current can be passed in.
- **Heel**: sail heeling moment against form stability + crew weight (`Hike` -1..1 x `HikeArm`). Past 70 degrees she
  goes over (`EvCapsize`). On a run in a blow the sail drives a slow roll (death roll) that a good crew damps and a
  green one does not. Water comes aboard past `SwampHeel` and from a capsize; a swamped boat is slow and tender until
  bailed (`bBail`) or self-bailed at speed.
- **States** (`EState`): `Ashore`, `Moored` (does not move; sails flag), `Sailing`, `Swamped`, `Capsized` -> rarely
  `Turtled` (left alone, in deep water) -> `Righting` (hold `bRight`; 11-16 s by class, x(1.7 - skill), twice from
  turtled) -> `Swamped`/`Sailing`; `Aground` (depth < draught: lift the board, `EvFloatedOff`), `Towed`.
- **Depth**: draught = hull draught + board down x board length (0.82 / 0.94 / 1.00 m board fully down;
  0.12 / 0.14 / 0.15 m board up). `Draught(B)` tells it.

### Classes (tuning numbers; `Def(EBoat)`)

| | optimist | cadet | luch |
|---|---|---|---|
| Loa / Lwl / beam, m | 2.30 / 2.15 / 1.13 | 3.22 / 3.00 / 1.27 | 4.23 / 3.85 / 1.37 |
| main + jib, m2 | 3.3 | 3.9 + 1.3 | 7.06 |
| hull, default crew, kg | 35, 35 (one child) | 54, 90 (two) | 59, 75 |
| boom length, CE height, mast from midships, m | 2.00, 1.35, 0.62 | 1.95, 1.85, 0.35 | 2.75, 2.30, 0.85 |
| boom limits, degrees | 15..88 | 12..75 (jib 14..60) | 15..90 |
| board area m2 / length m, rudder area m2 | 0.22 / 0.70, 0.085 | 0.26 / 0.80, 0.10 | 0.33 / 0.85, 0.13 |
| planing share, hike arm m, swamps past | 0.30, 0.45, 30 deg | 0.75, 0.62, 32 deg | 1.00, 0.95, 28 deg |
| righting time (good crew), mast length m | 14 s, 2.35 | 16 s, 4.9 | 11 s, 6.1 |

`CrewMassFor(Class, AgeYears)` gives the crew's weight for an age (pass 0 to `Init` for the default). Shared sail
numbers: ClMax 1.35, CdA = 0.05 + 0.16 Cl^2, hull friction Cf 0.0045, rudder 45 degrees hard over, capsize at 70
degrees. Speeds from `tools/sailsim/run.sh polar` (wind AT SAIL HEIGHT, flat water, good crew): optimist 1.5 m/s in
3, 2.2 in 5, 2.5 in 6, 3.2-3.5 in 10; cadet about 3.9 on a reach in 10; Luch 1.85 in 3, 3.0 in 5.5 (displacement),
5.8 in 10 (planing); best beating angle 47 / 44 / 41 degrees; a tack takes 2-4 s and costs half the way (more in the
Luch), full speed again in 6-8 s.

### The API and the call order

At load (any thread, once; otherwise 0.1 s a class is paid on the first far step or helmsman call): `WarmPolars()`.

Per boat, once:

```cpp
LyudSail::FBoat B;  LyudSail::FHelm H;
Init(B, EBoat::Luch, CrewMassKg /*0 = default*/, CrewSkill01, Seed);
Place(B, X, Y, Yaw, EState::Ashore);          // or Moored; PlaceSailing(B, X, Y, Yaw, Speed) to start afloat
HelmInit(H, Skill01, Seed);                   // NPC boats only
B.Assist.bAutoSheet / bAutoHike / bNoCapsize  // the player's easy mode, any time
```

Per tick, near boats (the full model):

```cpp
FEnv E;                                   // at the boat
E.WindX, E.WindY   = GetWindAt(Location, 2.0 m, bOverWater = true) in m/s, world, blowing toward (gusts included)
E.Depth            = SeaLevel - bottom, m (ALyudSea::DepthAt); <= draught grounds her
E.NormalX/Y/Z      = ALyudSea::SampleSurface normal at the boat (unit; flat = 0,0,1)
E.WaveHs           = significant wave height there, m
E.CurrentX/Y       = optional slow current, m/s

FControls C;                              // the player: from the input; an NPC: C = HelmUpdate(H, B, In, Dt)
Step(B, C, E, Dt);
// then read B.Out (FOutputs) and B.Out.Events (bits valid for this step only)
```

`FControls`: `Rudder` -1..1, `Sheet` 0..1 (0 hard in), `JibSheet` (-1 = follow the main), `Hike` -1..1 (+ = crew to
starboard), `Board` 0..1 (1 down), `bRight` (hold to right a capsized boat), `bBail`. To launch from the beach:
`Place(..., EState::Sailing)` (or `PlaceSailing`) when the crew pushes off; to land: `Place(..., EState::Ashore)`.
To tow: `StepTowed(B, HitchX, HitchY, RopeLen, E, Dt)` instead of `Step` (state `Towed`).

`FOutputs` (all the engine draws and plays): `X, Y, Yaw, VX, VY, YawRate`; `Speed` (through the water, along the
keel, negative astern), `Sog`, `Leeway`; `Heel`, `Pitch` (wave + planing trim), `Boom`, `Jib` (signed angles from the
centreline), `MainLuff`, `JibLuff` (0 full .. 1 flapping), `CamberSide` (-1/+1 which way the cloth bellies, 0
flapping), `RudderAngle` (rad), `Board`, `Hike`, `Sheet`; `AppWindSpeed/Angle`, `TrueWindSpeed/Angle`; `Point`
(`EPoint`: InIrons, CloseHauled, CloseReach, BeamReach, BroadReach, Run), `bByTheLee`; `TellWindward`, `TellLeeward`
(`ETell`: Flowing, Lifting, Stalled) and `TrimError` (-1 sheet in .. +1 ease) for the telltales and the tutor;
`SheetLoad` (N) and `SheetLoad01`, `HelmLoad` (-1..1, the pull on the tiller); `Planing`, `Wake`, `Spray` (0..1),
`SlapStrength` (with `EvHullSlap`), `BoomSlamSpeed` (with `EvBoomSlam`), `Water` (0..1 aboard), `Righting` (0..1
progress), `State`. Events (`EEvent` bits in `B.Events` / `Out.Events`): `EvTack, EvGybe, EvCapsize, EvRighted,
EvGrounded, EvPlaningStart, EvPlaningEnd, EvHullSlap, EvBoomSlam, EvTurtle, EvSwamped, EvFloatedOff, EvLuff,
EvBailed`.

Helpers: `TrueWindAngle(Yaw, WindX, WindY)`, `WindFromYaw(WindX, WindY)`, `IdealSheet(B)` (the sheet that trims her
now), `Vmg(B, TargetX, TargetY)`, `PolarSpeed(Class, Twa, Tws)`, `PolarHeel`, `BeatAngle(Class, Tws)`,
`RunAngle(Class, Tws)`, `Advice(B, E)` -> `EAdvice { None, SheetIn, Ease, BearAway, InIrons, Hike, BoardUp,
BoardDown, ByTheLee, RightBoat, Bail, FloatOff }` (one hint, the most urgent; `AdviceName` gives a short id).

**NPC helmsman**. Fill `FHelmInput In` each tick: `WindX/Y` (the same as `E`), `Depth`, optional
`DepthFn(User, X, Y)` + `MinDepth` (default 0.6 m; it is called for a few points ahead 4 times a second) and/or
`Poly` (the allowed water: x,y pairs of a simple polygon), `Neighbours` (an array of `FNeighbour { X, Y, VX, VY,
Length, Tack }` within some 60 m: other dinghies with `Tack = Out.TrueWindAngle >= 0 ? +1 : -1`, and Tack = 0 for the
launch, marks, swimmers, capsized boats = always kept clear of). Orders by `HelmOrder(H, O)`:

- `EOrder::SailTo` (X, Y, Radius; `H.bArrived` when inside: give the next mark; `Lane` = how wide it beats),
- `EOrder::Station` (jogs to and fro across the wind within Radius of X, Y; `H.bOnStation`),
- `EOrder::Follow` (the duckling file: set `LeaderX/Y/Yaw/Speed` every tick by writing `H.Order` directly, `Slot` 1..N,
  `Gap` m; the launch should lead at 1.2-2 m/s: under 0.8 m/s a dinghy does not answer her helm; when the leader
  stops (under 0.5 m/s) the boats jog about places fanned out astern of it),
- `EOrder::Ashore` (sails to X, Y on the beach, lifts the board, `H.bArrived` on touching: then `Place(..., Ashore)`),
- `EOrder::Idle` (lies beam-on with the sheet flying).

`Skill` 0..1 and the seed give reaction time, how far they hike, overstanding, wobble, timidity. The helmsman tacks
on laylines and headers, wears round instead of gybing when green and it blows, gets out of irons by backing the
rudder (a beginner late), rights the boat after a capsize, bails, keeps clear by the rules (port gives way to
starboard, windward to leeward, overtaking keeps clear, anything slower than 0.6 m/s is an obstacle). `H.Maneuver`
(`EManeuver`), `H.Tacks`, `H.Gybes` are there for the commentary.

**Far boats (LOD)**: instead of `HelmUpdate` + `Step`:

```cpp
HelmFar(H, B, In, Dt);                              // may be called a few times a second with the summed Dt
StepFar(B, H.CourseYaw, H.SpeedScale, E, Dt);       // every tick (or every few ticks with the summed Dt)
```

`StepFar` moves her at the polar's speed (x(0.72 + 0.28 skill), less in waves and in a blow), turns at a boat's rate,
keeps heel / boom / luff / events believable, capsizes beginners now and then over 5.5 m/s, and costs a third of the
full step (0.3 us against 0.9 us with the helmsman). Switching either way needs no call: the next `Step` or `StepFar`
picks the state up without a jump in place or heading (speed within 30 % over the next 2 s). The polar is in the wind
at sail height: always pass the 2 m wind.

**The coach's launch**: `FLaunch L; StepLaunch(L, Throttle -1..1, Turn -1..1, E, Dt)`; outputs in the struct (`Heel`,
`Pitch`, `Planing`, `Wake`, `EngineLoad`). `LaunchDriveTo(L, TX, TY, Cruise m/s, Radius, OutThrottle, OutTurn)` is its
simple driver. `LaunchLength` 4.6 m, `LaunchTopSpeed` 10.5 m/s.

### Tests (`tools/sailsim/run.sh test`: 241 checks)

| group | what is checked |
|---|---|
| polar | no way inside 30 degrees; beating angle; fastest on a beam or broad reach; run slower than the reach; optimist about 2 m/s in 5 and no flying in 8; Luch 2.5-3.5 in 5.5, 5-7 planing in 10; cadet planes in 10 |
| tack, irons, gybe | leeway; tack 2-4 s, way lost, back to speed in 6-8 s; stuck in irons with the sheet in, sternway, out by the reversed tiller; boom slam on the gybe, by-the-lee flag first |
| heel | pinned and not hiking in a blow: over in seconds; handled: stays up; righting times by class and skill, from turtle longer, never turtle in 2 m of water; swamped is sluggish, bailing; no-capsize assist |
| water | leeway board up against down; grounding by draught and floating off with the board up; no rudder at rest; calm: drifts with the current, sails hang; planing; waves slow her; hull slaps; pitching |
| tutor, assists | the `EAdvice` hints; easy mode: 30 min of every point of sail in a gusty blow without a capsize |
| launch | top speed, hump, turning, stopping, astern, driving to a point, towing a dinghy |
| determinism | same seed = same run bit for bit; steps of 1/30, 1/60, 1/120 s give the same course time within 1 %; a 4 fps step is stable |
| courses | 8 wind directions x 3, 6, 9 m/s (mean at 10 m, the game's gust field) x 3 classes x windward-leeward and triangle: good crews (0.9) all finish with no capsize; green crews (0.2) all finish, slower in every case, capsize in the 9 m/s gusts and never in 3 |
| fleet | 12 boats off a start line at 4 and 7 m/s: at most one touch; station within 40 m for 3 min (3 classes x 3 winds); six optimists in file behind the launch |
| LOD | far / full time round a triangle within 15 % (9 cases); 44 switches: no jump in place or heading, speed within 35 % in the next 2 s |
| sessions | 2 hours on four 2027 days (about 3, 5, 7, 9 m/s in the calendar; real wave field): out in file, station, laps, home; everyone back on the beach, nobody outside the water polygon, at most 2 groundings, at most 6 touches |
| cost | a far step under half of a full one |

`run.sh polar` prints the tables; `run.sh sail luch 6 0.2 tri 200 0.3` traces one boat; `run.sh session 2027 9 26 15`
traces a session.

### Known limits

- The polar is built in flat water with a good crew; `StepFar` corrects for waves, skill and a blow by factors.
- `RunAngle` is 180 degrees for all three classes in every wind (gybing angles never pay by 3 %), so the helmsman
  runs dead downwind; no spinnaker.
- The jib is a second sail on the same model (no backing it to help a tack, no slot effect).
- A full-strength 9 m/s day is hard on crews under skill 0.5: nine capsizes a boat in two hours in the session
  check. The school rules should keep beginners ashore well under the contract's upper limits.
- Green optimists and cadets rarely capsize on the course checks; the green Luch does (2 a run in 9 m/s).
- Boats do not collide in the model: the engine resolves hull contact. In a crowded rounding of the windward mark
  the helmsmen still touch a few times in two hours.

## Meshes (part C)

Script-built in Blender (`blender/sailing/`, one command: `python3 blender/sailing/build_all.py`; on the Mac
`unreal/scripts/import_sailing.sh` builds and imports to `/Game/Sailing/SM_Sail_<Name>`). Details, the full part list
and pivots: `blender/sailing/README.md`.

- **Boat frame**: metres, origin at the static waterline amidships, **+X forward, +Y port**, +Z up; Unreal X = x,
  Y = -y (Unreal +Y = starboard), Z = z. Every part's origin is its pivot (`blender/sailing/sockets.json` → `parts`).
- **Dinghies** (`optimist`, `cadet`, `luch`): `SM_Sail_<Optimist|Cadet|Luch>_{Hull, Mast, Boom, Main, Rudder, Tiller,
  Ext, Board}` + `SM_Sail_Optimist_Sprit`, `SM_Sail_Cadet_Jib`; LOD1 inside each part; `SM_Sail_<C>_Far` = the whole
  boat < 300 triangles. Rigged LOD0: luch ≈ 5.9k, cadet ≈ 6.1k, optimist ≈ 4.8k triangles.
- **Sockets** (`sockets.json`, both sides): helm, hike, cadet crew / crew_hike / crew_low (pelvis, facing, feet under
  the strap), hiking straps, mainsheet block and hand, tiller / extension hand grips (in the part's frame), boom end,
  painter, trolley contact.
- **Custom primitive data** for the boat's parts: [0] camber −1..1, [1] luff flutter 0..1, [2] sail number 0..999,
  [3..5] hull tint (linear RGB). Masters: `unreal/scripts/make_sailing_materials.py` (M_Sail_Cloth, M_Sail_Hull,
  M_Sail_Flag, M_Sail_Windsock).
- **Gear**: trolleys (`SM_Sail_Trolley_<C>`, the boat's origin at manifest `boat_origin`), winter covers
  (`SM_Sail_Cover_<C>`), sail bag, spars rack, the coach's launch `SM_Sail_Launch` + `SM_Sail_Launch_Motor`,
  `SM_Sail_Mark`, `SM_Sail_Pin`, `SM_Sail_Buoy`, `SM_Sail_MOB`, `SM_Sail_AnchorBlock`, `SM_Sail_Rope`, `SM_Sail_Block`.
- **Boathouse** after part B's site (16.5 × 9 m, ridge to the sea, wall 3.4, ridge 5.6, both doors in the north wall:
  boat door 3.2 m sliding (two `SM_Sail_Boathouse_DoorLeaf`), people's door ALyudDoor 1.20 × 2.18 hole; classroom in
  the landward 5.3 m, then the hall, changing room + wet store with a sail loft at the seaward end; plinth to −0.85 for
  the 7 % sand slope). `SM_Sail_Boathouse` (collision exact), `_Glass`, ~40 `SM_Sail_Prop_*`, 10 signal flags
  `SM_Sail_Flag_<id>`, windsock, park slab + fence + gate, slip boards. Placement: `blender/sailing/boathouse_layout.json`
  (building frame: origin on the floor in the middle of the footprint, +X to the sea, −Y the north wall): props with
  rooms and `interactive` kinds, boats on racks / trolleys, doors for `place_doors.py`, sliding doors, lamps, rooms,
  people's spots, the outside kit, flag hoist points.
- **Signs**: `tools/make_sailing_signs.py` → `textures/localized/<lang>/sail_sign_{name,door}.png`, keys
  `sign.sail.school_name`, `sign.sail.door_line1`, `sign.sail.door_line2` (proposed texts in
  `blender/sailing/sign_strings.json` until part G puts them in the strings tables).
- Previews: `/mnt/project-files/yacht-school/`.
