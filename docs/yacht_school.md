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
