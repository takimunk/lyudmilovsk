# The local wind

What the air does where you stand: on a dinghy's sail, a kite's line, a gull's wing, a flag. Plain C++ in
`Source/Lyudmilovsk/Private/Weather/WindField.h/.cpp` (namespace `LyudWeather`, no Unreal types), on top of the area's
wind of [sea_and_wind.md](sea_and_wind.md): `Regime()` (the weather and the day) and `GustAt()` (the drifting gust
patches) are not changed by any of this; the sea's material still mirrors `GustAt` in HLSL and the sea and weather
suites still pass as they did.

Frame as everywhere in the models: world metres (Unreal cm / 100), north = +Y, east = -X, Z up; the sea lies toward +X.
Bearings are compass degrees the wind comes **from**: 270 is the onshore westerly, 90 blows off the land out to sea.
Speeds are m/s; the regime's speed is the wind at 10 m over open water. Everything is a pure function of its arguments
(no state, no allocation), so a save, a jump of the clock or a second machine agree.

## The layers

| Layer | Function | What it adds |
|---|---|---|
| Shifts | `ShiftAt`, `Shifted`, `Now` | the direction swings a few degrees every few minutes; the whole bay breathes (puff, lull) |
| The land and the sea | `LocalWindAt` over an `FWindTerrain` | lee of ground and cover, squeeze over crests, ridge lift and lee sink, sea or land profile, fetch, a bend along a cliff, turbulence |
| Looking ahead | `GustAhead`, `NextGust` | the gust factor that will be at a point in N seconds; the next gust or lull worth a word |
| Words and numbers | `Beaufort`, `BeaufortKey`, `CompassKey`, `ToKnots`, `Outlook`, `NextSpell` | force 0..12, string-table keys, the next hours, the next spell good for a lesson |
| Things that move | `Apparent`, `BearingFrom`, `TowardOf`, `AngleOff` | apparent wind, bearings and the angle off the bow |

### 1. Shifts

`ShiftAt(Hours, Regime, Seed)` is a sum of sines of the calendar's minutes with seeded phases, in three bands: a slow
swing (10.9, 7.3 and 17.9 min), a quick one (5.3, 3.7, 6.9 min) and the breathing (13.7, 8.9, 5.9 min). Every sine has
a zero mean, so the veer averages to nothing and the factor to 1: the year's statistics of `Regime` stand (checked).

- `RangeDeg` = how far it swings either way: 2..5 degrees in a steady fresh wind at night, up to 18 on a light, stirred
  sunny afternoon. It grows with `FWindState::Day` and `Gustiness` and shrinks with `Speed` (light under 3 m/s, steady
  over 10). `VeerDeg` (+ veered, clockwise) reaches the range now and then and never passes it; at most 0.6 degrees a
  second.
- `PeriodMin` = how long a swing (two turns) takes now: about 10.4 minutes in settled air (the slow band), 5 in stirred
  afternoon air (the quick one), between as the day mixes them. Minutes of the game's calendar.
- `Factor` = the breathing on the speed: 1 +- 0.05 in settled air to 1 +- 0.2 on a gusty light day.
- `NextTurnMin` = minutes until the veer changes sign, found by walking the swing forward in 0.1 min steps and halving
  the step it turns in (exact against a brute-force scan; 30 = no turn that soon, which does not happen in practice:
  the mean is 2.3 min). It holds the regime still; over the few minutes it looks the running regime moves the turn by
  seconds. `Phase` runs 0..0.5 through the veered half of a swing and 0.5..1 through the backed one.
- `Now(Hours, P)` = `Shifted(Regime(Hours, P), ShiftAt(...))`: the wind a game shows. Pass it to everything below.

### 2. The land and the sea

`FWindTerrain` is a coarse grid (16 m cells) of ground heights and of the height of what stands on the ground
(`Cover`: crowns, roofs). Bilinear inside; within one cell outside the grid it eases down to open sea (the sea bed,
`SeaZ` - 10), so nothing steps at the grid's edge. Give the grid several hundred metres of land beyond where anybody
asks on its landward sides (a lee is 14 obstacle heights long; terrain.py keeps 450..650 m). `Cover` is a windbreak's height: for a thing narrower than it is
tall put its width, leave lattice and open things out; its lee is a wood's (porous). A solid wall is better raised into
`Ground`.

`LocalWindAt(S, Terrain, X, Y, Z, WaterZ, OffsetX, OffsetY, Seconds)`:

- **No terrain** (null or not `Valid()`): exactly the old `GetWindAt` over land, bit for bit: speed = `S.Speed` x
  `GustAt().Factor` x `HeightFactor(Z, false)`, direction turned by the gust's veer, `Exposure` 1, no updraft, ground at
  Z = 0.
- **With a terrain** it marches upwind from the point: 30 samples from 10 m to 3 km (1.46 times further each close in,
  every 44 m from 100 m to 890 m, 1.3 times beyond), three downwind (12, 35, 90 m) and two to the sides, and works out:

| | How | Numbers |
|---|---|---|
| `Shelter`, lee | ground or cover upwind that stands H higher than the surface here: full within 2 H, fading (squared) to nothing at 14 H; only under the wake's top (soft, rising downwind). Solid ground takes 88% of the speed in the deepest lee, cover 70% of that (porous); low cover is thin cover. Under the crowns of the very spot the same. | beach under the 60 m cliff in an offshore wind: exposure 0.10; 400 m out to sea 0.68; open from ~900 m |
| crest squeeze | the surface dropping away upwind within ~450 m and not rising ahead: up to x1.42 at the ground, less with height (half at 0.7 of the drop) | the cliff's brow at 3 m: 1.35 |
| land drag | over land the lower air is 20% slower than over the sea (28% over woods and roofs), fading with height (half at 150 m) | bare land at 10 m: 0.82 of the sea's 10 m wind |
| `Exposure` | (1 - drag) x squeeze x (1 - 0.88 x shelter): everything the land does except the height profile | 0.10 dead lee .. 1 open sea .. 1.4 a brow |
| height profile | `HeightFactor`: the land's over land, the sea's over water with a few hundred metres of water upwind, blended by the share of water in the 250..600 m upwind (so the sea's wind lasts a little way inland and the land's a little way out); over cover the profile starts at 0.65 of its height | 2 m over 10 m: 0.85 at sea, 0.70 over land |
| `Updraft` = `VZ`, ridge lift | the air climbs with the ground under it and ahead of it (ground further ahead lifts only the air some way up): speed x slope, at most 0.6 x speed off a steep face; still some just past a brow; fades above the crest over 1.4 x the relief (as `BirdAir`'s) | 8 m/s onshore: +4.7 m/s 20 m off the cliff's face, +3.6 over the brow at 10 m, +0.6 at 250 m, nothing from ~70 m off the water line |
| lee sink | behind a crest or in a solid lee, from 5 m up (full at 20): at most 0.2 x speed and 1.5 m/s | 30 m over the beach in an 8 m/s offshore wind: -1.1 |
| `FetchM` | open water upwind until the first land, 0 over land, capped at 3000; the step that crosses the water's edge counts by its ends' depth and height | good to ~45 m within 900 m of the shore, to a quarter beyond |
| `FromDeg`, bend | low air slides toward the lower side of a cross slope: up to 15 degrees, fading with height (half at 25 m) | wind from 225 at the cliff's foot: backed 12 degrees to run along it |
| `Turbulence` | rms m/s the mean does not carry: gustiness x speed x (0.4 sea .. 0.6 land .. 0.85 woods and roofs), less high up and deep in a lee, plus the wake's own tumbling (about a sixth of the open wind) | open sea 0.74, bare land 1.1, the beach in the cliff's lee 1.8 (8 m/s, gustiness 0.23) |
| `HeightM`, `SurfaceZ`, `bOverWater` | height over the ground or the water (`WaterZ`, the tide's level now); at low water the bared flat is land | |

The field is continuous: every lookup is bilinear and every threshold a ramp. It is steep where the world is: a few
steps past a cliff's brow take you from the open wind into its lee (at most 0.8 m/s in 25 cm in a 10 m/s wind), and the
fetch opens by hundreds of metres when a step uncovers a headland.

What it does not do: the wind does not flow round an obstacle (a lee is a straight strip downwind, as wide as the
obstacle), there are no thermals (the birds' are in `BirdAir`), no funnelling through gaps, no sea-breeze front.

### 3. Looking ahead

The gust field drifts downwind at the mean wind (`ULyudWindSubsystem` adds `Mean x dt` to the drift offset), so
`GustAhead(S, X, Y, OffsetX, OffsetY, Seconds, In)` = `GustAt` with the offset moved by `Dir x Speed x In` and the
clock by `In`: exact while the regime holds. `NextGust` looks once a second up to `WithinSeconds` (120 at most) for the
first factor over 1 + Threshold or under 1 - Threshold (`InSeconds` < 0: none), then follows it for its peak and how
long it lasts. A graze of the threshold shorter than a second can be missed.

### 4. Words and numbers

`Beaufort(Speed)` uses WMO's table (force 4 from 5.5 m/s, 8 from 17.2, 12 from 32.7); `BeaufortKey` and `CompassKey`
give lower-case keys for the string tables ("moderate_breeze", "wsw"; 4, 8 or 16 points). `Outlook(Hours, Span, P)`
looks at `Now()` every ten minutes: min, mean, max, the strongest gust to expect, the vector-mean bearing, the trend in
m/s per hour (least squares), and `TurnDeg` = how the regime's own bearing ends against how it starts (a shift's swing
at either end is not a change of wind). `NextSpell(Hours, P, Min, Max, Hold, Within, bDaylight)` = hours until the
regime's mean speed stays within Min..Max for Hold hours (0 = it does now, -1 = not within the look-ahead), stepping
five minutes; with `bDaylight` only while `Day` > 0.

## Asking it from plain C++

```cpp
#include "WindField.h"
using namespace LyudWeather;

FWindParams P;                                   // P.Seed = the game's (ULyudWindSubsystem::Seed, 921)
const FWindState S = Now(Hours, P);              // the area's wind with its shift
const FLocalWind W = LocalWindAt(S, &Terrain, X, Y, Z, WaterZ, DriftX, DriftY, GustClock);
// W.VX, W.VY, W.VZ: the air's velocity; W.Speed, W.FromDeg, W.Exposure, W.Shelter, W.Turbulence, W.FetchM ...
float AX, AY;
Apparent(W.VX, W.VY, BoatVX, BoatVY, AX, AY);    // what the sail feels
const float Off = AngleOff(HeadingDeg, BearingFrom(AX, AY));   // 0 dead ahead, + on the starboard side
const float Soon = GustAhead(S, X, Y, DriftX, DriftY, GustClock, 20.0);   // the factor here in 20 s
const FWindShift Sh = ShiftAt(Hours, Regime(Hours, P), P.Seed);           // Sh.NextTurnMin: when to tack
```

A suite without a terrain passes `nullptr` and gets the old open-land wind; `DriftX`/`DriftY`/`GustClock` are the
engine's drift (0, 0, 0 is fine offline).

Cost (this container, `-O2`, random points over the real map; `GustAt` alone is 0.43 us here): `LocalWindAt` without a
terrain 0.47 us, with it 1.3 us (the march itself 0.9); `Regime` 0.6 us, `Regime` + `ShiftAt` 3.9 us (once a frame),
`NextGust` 10 us (on demand).

## tools/windsim

`tools/windsim/run.sh` builds `windtest.cpp` with `WindField.cpp`, `WindModel.cpp`, `WeatherModel.cpp` and
`RainModel.cpp`. The real map's relief is `Saved/Wind/terrain.bin`, made by `tools/windsim/terrain.py` when missing or
older than its sources: `tools/world_map.py`'s macro height on a 16 m grid (map X -1900..1300, Y -1350..4850), and a
cover grid from the forests (`W.forests()` thinned by `forest_cover()`, a crown height per mix: 24 m old forest, 9 m
dune pines ...) and the town's 30 plots with a solid building (`city_plan.json`). Not in it: the Academgorodok's blocks,
the wall dwellings (inside the terraces' walls, which are ground), single trees, the terrain's natural detail. Without
python3 or the map the suite runs on a built-in coast alone (103 checks instead of 134).

- `run.sh` (test): one line a check, the probes' values printed for a human, `N passed, M failed`.
- `run.sh map 90 8 2`: `Saved/Wind/exposure_090_8_2m.pgm` (speed over the open sea's at that height) and
  `updraft_090_8_2m.ppm` (red rising, blue sinking) over the real map, north up, the sea on the left.
- `run.sh at 250 905 2 14.5`: the wind at map X 250, Y 905, 2 m up, at 14:30 of the scene's day.
- `run.sh line -300 90 8 2`: a cut along X at map Y -300.
- `run.sh day 2026 7 14`: regime, shift, Beaufort and the three-hour outlook hour by hour.
- `run.sh spell 3 8 2026 5 1`: the daylight spells of 3..8 m/s over two weeks.

What the test shows (8 m/s, the real map at map Y -300, where the upland's 60 m cliff stands over the beach):

| Check | Value |
|---|---|
| no terrain = the old formula, 20 000 random calls | 0 misses, bit for bit |
| a month's mean veer / mean factor | 0.001 degrees / 1.000 |
| range over the month; fresh night; light sunny afternoon | 3.6..18.0 / 2.3 / 18.0 degrees |
| measured swing against `PeriodMin`: settled, mixed, stirred | 10.7 / 10.3, 7.3 / 7.7, 5.3 / 5.3 min |
| `NextTurnMin` against a brute-force scan (1500 moments) | all within 1 s (worst 0.3 s) |
| year 2026 with shifts against the regime: mean, median, prevailing bearing | 4.138 = 4.138 m/s, 4.058 / 4.075, 237.68 / 237.66 |
| offshore: beach under the cliff, 100 m, 400 m, 1000 m out (2 m up) | 0.54, 1.12, 4.58, 6.81 m/s (open: 6.81) |
| offshore: turbulence on the beach against the open sea's | 1.81 / 0.74 m/s |
| offshore: fetch 100, 400, 1000, 2000 m out | 97, 392, 896, 1961 m |
| onshore: lift 20 m off the face / over the brow at 10 m / at 250 m | +4.70 / +3.56 / +0.64 m/s |
| onshore: exposure on the brow at 3 m; on the beach | 1.35; 0.99 |
| onshore: under the upland's wood at 2 m; 45 m up over its crowns | 1.60; 8.42 m/s |
| onshore: 48 m behind a town block against the water's edge | 1.55 / 6.73 m/s |
| open water at 10 m against open land at 10 m | 8.00 / 6.54 m/s |
| wind from 225 at the cliff's foot | bent 12 degrees along it; 2 degrees at 150 m |
| largest change in 25 cm (10 m/s, 400 walks): speed, updraft, bearing | 0.73 m/s, 0.22 m/s, 0.24 degrees; ten times less in 2.5 cm |
| `GustAhead` against `GustAt` later with the drifted offset | exact; 5e-7 against a frame-by-frame drift |
| `NextGust` against a 0.02 s scan (400 looks) | every gust or lull of a second or more found, within 0.7 s |

## In the engine

`ULyudWindSubsystem` (`Public/Weather/LyudWind.h`, `Private/Weather/LyudWind.cpp`) wraps the model for the game; C++,
Blueprint and Python (`unreal.LyudWindSubsystem.get_wind(world)`). Not compiled or run yet when this was written: the
first Mac build checks it.

### API

| Call | Gives |
|---|---|
| `GetLocalWind(Location)` | `FLyudLocalWind` at a world point (cm): velocity with rising/sinking air in Z, speed, bearing, gust factor, `Exposure`, `Shelter`, `Updraft`, `Turbulence`, height over the surface, fetch, over water or not. What sails, kites, flags, smoke should ask. |
| `GetLocalWindAbove(Ground, HeightM)` | the same for a thing of a known height over its own ground (a flag on a 6 m pole, a mast head) |
| `GetLocalWinds(Locations, Out)` | many points in one call (Out is resized, nothing else allocates); C++ only |
| `GetApparentWind(Location, BodyVelocityCmS)` | local wind minus the body's own motion, m/s |
| `GetWindAt(Location, HeightM, bOverWater)` | the old sample (speed, gust, horizontal velocity). Now with the swing of the direction, and with the land's `Exposure` at that height over that ground multiplied in (`lyud.Wind.Relief 0` or an unread grid: exactly the old open-air answer). The height's profile and `bOverWater` stay the caller's. Over open land it is now about 0.8 of what it was (the land's drag), in a lee much less, on a brow more. |
| `GetWindsAt(Locations, HeightM, bOverWater, Out)` | `GetWindAt` batched |
| `GetMeanWind` / `GetMeanSpeed` / `GetFromDegrees` | the area's wind now, the swing included (also what `MPC_LyudWeather.Wind` gets) |
| `GetSteadyFromDegrees` / `GetShift` | the weather's direction without the swing; the swing itself (`VeerDegrees`, `Factor`, `RangeDegrees`, `PeriodMinutes`, `NextTurnMinutes`) |
| `GetSeaStateWind` | unchanged and unshifted: waves do not follow a swing of minutes |
| `GetGustAhead(Location, Seconds)` | the open-air gust factor that will be there in that many real seconds |
| `GetNextGust(Location, Within, OutIn, OutFactor, bOutLull)` | the next gust (> 1.25) or lull (< 0.75) worth a word, false if none within `Within` s (at most 120) |
| `GetBeaufort` / `BeaufortOf(Speed)` | force 0..12 |
| `BeaufortKey(Force)` / `CompassKey(FromDeg)` | string keys `wind.force.<0..12>`, `wind.from.<n\|ne\|e\|se\|s\|sw\|w\|nw>` |
| `DescribeWind()` | a localized line: "SW 4 (moderate breeze), gusts 6" (`wind.describe`, `.steady` when the gusts are no stronger, `.calm`) |
| `GetOutlook(SpanHours)` / `HoursUntilWind(Min, Max, Hold, bDaylight)` | the next hours; game hours until a spell of wind in a range (-1: not in 48 h). A forced speed (`lyud.Wind.Speed`) is taken as holding. |
| `IsReliefReady` / `RebuildRelief` / `GetWaterZ` | the grid is read; read it again; the still water's level now, cm |

Time: the swing is worked out for the very moment every frame the calendar has moved (the calendar runs at real time,
a swing takes minutes, so the direction never steps); the weather under it (`Regime`, the sea-state wind) is recomputed
once a game second as before. A forced direction (`lyud.Wind.From`) still swings, a forced speed is exact (no
breathing); `lyud.Wind.Shifts 0` pins both. Gusts keep their real-time clock and drift (`GetDrift`).

### The relief grid

`LyudWeather::FWindTerrain` filled from the level, in play only. It starts 4 s after play begins (the town's houses are
put up in code first), covers the landscapes' bounds plus 128 m of sea all round in 16 m cells (about 60 k cells, two
float arrays, under 0.5 MB; a bigger map gets bigger cells, 90 k at most), and reads a few cells a frame within
`lyud.Wind.ReliefBudgetMs` (0.6 ms; at least 8 cells a frame), so roughly a quarter of a minute at 60 fps with no hitch.
Until the last cell `IsReliefReady` is false and every answer is open flat land (`GetLocalWind` then assumes the
standard 10 m; `GetWindAt` is the old code path).

- **Ground**: one top-down multi trace of the static world per column (simple collision, the sea and its water volume
  ignored); the ground is the landscape or anything tagged `LyudSeaBed`, as the sea's depth map reads it. No ground
  (outside the landscape) = the sea's bed 10 m under the datum.
- **Cover**: the highest static hit of the column minus the ground (or minus the datum where the ground is the sea's
  bed: the pier's deck). Land cells take two columns a quarter cell either side of the middle and average them, so a
  house that fills half a cell counts for half its height and a pylon caught by one column for half. Under 1 m is
  dropped (kerbs, benches), over 45 m clamped. Forest: trees have no collision, so `ULyudTerrainMask`'s forest-floor
  channel x 16 m (an old pine and spruce canopy) is taken where it is the higher.
- **Water level**: per query, `ALyudSea::SeaLevel()` (the tide) through a weak pointer looked up every 2 s while there
  is no sea; the datum -43 m without one.

### Console and developer mode

`lyud.Wind.Speed` / `.From` / `.Gustiness` force a value (negative: the calendar's). `lyud.Wind.Shifts 0|1`.
`lyud.Wind.Relief 0|1` (only `GetWindAt`; `GetLocalWind` always has the land). `lyud.Wind.ReliefBudgetMs`.
`lyud.Wind.Debug 1` prints the wind, the swing, the force and the grid's state on screen; `2` also draws 11 x 11 arrows
of the local wind 2 m over the surface round the camera (12 m apart; red dead lee, yellow, green open, cyan sped up;
40 cm of arrow per m/s; a white tick up for rising air, magenta down for sinking, 1 m per m/s). `lyud.Wind.Report`
logs `WIND ...` lines: the report, the words, the local wind and the next gust at the camera, the 3 h outlook, the
tell-tales. `lyud.Wind.RebuildRelief`. `lyud.Wind.Tells 0|1`.

F7, group **Wind** (`Dev.Do wind.<id>`): `calendar`, `calm` (0.6 m/s), `kite` (6 m/s off the sea), `fresh` (10 m/s
WSW), `gale` (18 m/s WNW), `offshore` (7 m/s from the east), `arrows`, `report`, `tells`, `sock_here` (a windsock and a
pennant 8 m in front of the camera). Teleports `wind_sock_beach`, `wind_pier_head` (`Content/Dev/teleports/wind.json`).

### Tell-tales

`Public/Weather/LyudWindsock.h`. `ALyudWindTell` is a windsock (five red and white bands, 2.4 m) or a pennant (1.8 m
ribbon) on a staff, built in code from three shared meshes (one band, one ribbon piece, one metre of pole) and a
handful of shared material instances; per frame only its pieces' transforms change. It asks `GetLocalWind` at its top
ten times a second and eases between the answers: it weathercocks with a little overshoot (the turning pull grows with
the square of the wind), the sock stands out band by band (about 1.5 m/s a band: limp under 2 m/s, straight by 8), sags
in a lull, whips with `Turbulence`, cocks up in rising air; the pennant flies from a breath and is straight by 5 m/s.
It takes its own motion off the wind, so attached to a mast head it shows the apparent wind. No collision, out of Lumen
and distance fields, transient.

`ULyudWindTells` (world subsystem) moves the ones within 250 m of the camera every frame, those out to 600 m four
times a second, and hides the rest. At the start of play it stands a sock 3 m to seaward of each lifeguard tower's flag
mast (the town's things with the act `lg_flagstate`) and one at the pier's head (the city plan's `S-PIER-HEAD` through
the festivals' venue lookup). Your own:

```cpp
#include "Weather/LyudWindsock.h"
ALyudWindTell* Sock = ULyudWindTells::Spawn(this, FootOnTheGround);                              // 6 m pole, red and white
ALyudWindTell* Flag = ULyudWindTells::Spawn(this, Foot, ELyudWindTell::Pennant, 400.f, 2);       // 4 m staff, yellow
ALyudWindTell* Burgee = ULyudWindTells::Spawn(this, MastHead, ELyudWindTell::Pennant, 0.f, 1);   // no pole
Burgee->AttachToComponent(Mast, FAttachmentTransformRules::KeepWorldTransform);                  // apparent wind
ULyudWindTells::Remove(Sock);                                                                    // or Destroy()
```

### Who asks what

| Caller | Uses |
|---|---|
| Sea (`LyudSea`, `LyudUnderwater`) | `GetSeaStateWind`, unshifted, as before |
| Player bird (`BirdAir.cpp`) | `GetWindAt` for the horizontal wind (so: the swing, lee and speed-up); its own trace-based ridge lift and thermals stay (the model's `Updraft` is not added: no double count); turbulence is the larger of its own and the local wind's, by at most +1.2 m/s (the rotor behind the cliff) |
| Ambience (`LyudSoundscape.cpp`) | `GetLocalWind` at the listener: quiet in a lee, full on the open beach and the pier; among trees it blends to the wind 25 m up x 0.65 (what the crowns are in), which also drives the leaves' and needles' voices. Before the grid is read: the old `GetWindAt(…, 2 m)` |
| Kites (`LyudKite.cpp`), festival kites and fluff, cable-car cabins, the North's steam, people's kite hours | `GetWindAt` / `GetMeanWind` as before: they get the swing and the relief through it |
| Rain and fog, winter, forest floor, bird flocks | `GetMeanWind` / `GetGustiness`: the swing only |
| Materials (`MPC_LyudWeather.Wind`, `WindDrift`) | the shifted direction and speed; the gust field is unchanged |
| Tell-tales | `GetLocalWind` at their top |

### Cost

`State()` is one `ShiftAt` a frame (a few sines and two short scans for the next turn, some microseconds) on top of the
once-a-game-second weather. `GetLocalWind` is 20 bilinear reads of the grid and one gust sample, about a microsecond,
no allocation; `GetWindAt` with the relief costs the same again on top of what it did. The debug arrows are 121 local
winds and some hundred lines a frame, only while on. A tell-tale near the camera is one local wind per 0.1 s and five
or six component moves a frame; there are three in the town.
