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
