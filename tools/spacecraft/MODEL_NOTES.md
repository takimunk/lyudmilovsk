# «Ласточка»: the flight model, for the engine side

`unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Spacecraft/FlightModel.h`, namespace `LyudFlight`, header-only, plain C++17
(includes only `../People/MiniJson.h` and std). It decides everything about the flight home from Europa
(docs/spacecraft.md) except what things look and sound like. The engine feeds it four facts and a `dt`, and draws what it says.

| File | What |
|---|---|
| `Private/Spacecraft/FlightModel.h` | the model |
| `tools/spacecraft/flight.json` | THE SOURCE of every number (times, g, chute, track, lamps, cues) |
| `tools/spacecraft/flight.py` | checks it, writes `Content/Spacecraft/flight.json`; `--check` for CI |
| `tools/spacecraftsim/run.sh` | the offline checks (23 131, exit 1 on a failure); `show [step]` prints the flight as a table; `steps`; `dump` |

The numbers are ALSO built into `FConfig` as defaults, so `FFlight F;` works with no file. `run.sh` fails when the
defaults and either json differ: a tuning change goes into `flight.json` and `FConfig`, then `flight.py`, then `run.sh`.
Loading the file in the game is optional: `FConfig C; C.LoadJson(Text, Err); FFlight F(C);`.

## The two classes

- `FProfile`: the 900 s of the flight as **pure functions of the flight time** (`Eval(T, SunF, FState&)`, `Load(T)`,
  `Range(T)`, `ChuteAlt(T)`, ...). Built once from an `FConfig` (about 0.3 MB of tables, under a millisecond). Use it
  directly for previews, dev scrubbing, the CRT's drawing.
- `FFlight`: the craft with the hero in it: the checklist, the clock, the nap, the cues, the record player, the lamps, the
  capsule after the splash, the save. It owns an `FProfile` (`F.P`). One instance lives in `ULyudSpaceflightSubsystem`.

## Call order

```cpp
LyudFlight::FFlight F;                       // once (or F(Config))
F.Load(SavedText);                           // on load; false = not a save (the craft is then cold on Europa)
F.SetRecords({{"record_sc_house", 214.0}, {"record_sc_waltz", 187.5}});   // ids + lengths in seconds; before or after Load

// every tick while the hero is on Europa near the craft, in it, or the capsule is afloat:
F.bSeated = ...; F.bBuckled = ...; F.bHatchShut = ...; F.bWheelSealed = ...;   // the hero and the hatch
F.HeroDistM = ...;                           // afloat only: metres from the capsule
F.GameDay = ...;                             // the game's day number
F.Fast = CVar lyud.Flight.Fast (1 = normal); // the whole model runs that many times faster
F.Advance(DeltaSeconds);                     // real seconds
for (const LyudFlight::FEvent& E : F.TakeEvents()) { ... }   // sounds, lines, the voice: each fires once
const LyudFlight::FState S = F.State();      // everything to draw this frame

// the hero's hands (each returns "" when it worked, otherwise a string key to show, e.g. "ui.flight.no.order.gyro"):
F.Toggle(LyudFlight::SwPower);  F.Launch();  F.ToggleLight();  F.ToggleRadio();  F.NextCrtMode();
F.StartNap();  F.EndNap();
F.RecordNext();  F.RecordPut(Index);  F.RecordNeedleDown();  F.RecordStop();
F.CanOpenHatch();  F.CanUnbuckle();  F.CanStand();  F.CanSwimOut();  F.PullChuteHandle();   // queries, same keys

SavedText = F.Save();                        // "v1;ph=3;t=412.5;..." any moment, mid-flight too
```

Before the launch set `F.SunF`: the star's direction in the flight frame F (below), from `ALyudEuropaSite::SkyAt`. It is
kept (and saved) for the whole flight.

`Advance` is safe at any tick length: the phase times, the cues and the nap's end are exact at 60 Hz and at 4 Hz.

## Phases and time

`F.Phase()` / `S.Phase`: `Parked` (nobody on the couch), `Prelaunch` (`bSeated`), `Ascent` 0..75, `Coast` 75..600,
`Approach` 600..720, `Entry` 720..810, `Chute` 810..885, `Splash` 885..900, `Afloat`. `PhaseName()` gives the ids of
`flight.phase.*`. `S.T` is the flight second (0 before ПУСК, stays 900 afloat), `S.PhaseProgress` 0..1, `S.bFlying`,
`S.TimeScale` (flight seconds per real second: 1, 8 in the nap, times `Fast`).

Moments inside the phases (all in `flight.json` `times`): the stage is left at 1.5; the cabin looks back at Europa 96..195;
the turn to the planet 195..225; the burn 300..306; the instrument section goes at 700 (then the cabin yaws round, heat
shield first, 702..718); drogue 810, main canopy 825; whiteout 840..860 with its peak at 850; jets 885; the water at 888.

**The nap**: `StartNap()` works in the coast only, buckled, not within 12 s before the burn or during it, not after 560 s
(`ui.flight.no.nap_now`, `.nap_unbuckled`). Flight time runs 8x; it ends by itself at 580 s exactly (events `nap_end`,
`nap_wake`) or by `EndNap()`. `F.NapSleptSec()` = flight seconds slept (for `ui.flight.toast.slept`). While asleep the
hero's lines are dropped by the model; sounds and the voice still come, with `FEvent::bAsleep` true (play them soft or not).

## The checklist

Five switches `SwPower, SwAir, SwGyro, SwSeal, SwArm` (ids `power air gyro seal arm`, `SwitchName()`), then `Launch()`.

- `Toggle(S)` on: every earlier switch must be on, else `ui.flight.no.order.<that switch>` ("Power first" etc.); `seal`
  also needs `bHatchShut` (`.hatch_open`) and `bWheelSealed` (`.hatch_unsealed`). Off: only when every later one is off
  (`ui.flight.no.order.power` for power, else `ui.flight.no.order`). Not seated: `ui.flight.no.not_seated` (silent).
- A refusal of order also fires the event `buzzer` (its `Arg` = the reason key) and blinks `master_caution` for 3 s.
- `Launch()`: `.not_seated`; not all five: `.order.launch` + buzzer; hatch: `.hatch_open`; `.not_buckled` (silent).
  On success: events `launch`, `liftoff`, `auto.launch`, and the phase is `Ascent`.
- In flight and afloat every switch and the button answer `ui.flight.no.switch_flight`.
- `CanOpenHatch()`: `.hatch_flight` in flight; `ui.flight.no.order` on the pad while the seal switch is on; free afloat.
  `CanUnbuckle()`: `.unbuckle_g` over 0.6 g. `CanStand()`: `.stand_now` outside the weightless coast (and in the burn, asleep).
  `CanSwimOut()`: `.swim_early` till afloat. `PullChuteHandle()`: always a refusal, `.chute_early` or `.chute_late`.
- Power on: every bulb glows for 1.2 s (`S.I.bLampTest`). Gyros spin up for 4 s (lamp blinks). The cabin pressure
  (`S.I.PressMmHg`) follows the hatch and the air switch with a 3 s lag: 0 on Europa with the hatch open, 760 once tight
  with the air on, sea air after the hatch opens afloat.

## FState (what `F.State()` returns)

Units: km, km/s, m, m/s, g, degrees, °C, seconds. Directions are unit vectors in the cabin frame.

| Field | Meaning |
|---|---|
| `Phase, T, PhaseProgress, TimeScale, bNap, bFlying` | above |
| `bEngine, bBurn, bBlackout` | ascent engine on; correction burn on; radio blackout (731.7..783.3 s) |
| `RangeKm` | to the planet's surface (6891 at Europa, 71 at the entry, 0 at the splash) |
| `RangeRateKms` | its rate, negative = closing; continuous |
| `AltEuropaKm` | distance from the pad (38 at the cutoff) |
| `SpeedKms` | 0 .. 1.05 at the cutoff .. 22 in the coast .. 7.9 at the entry .. 0.17 at the drogue |
| `VSpeedMs` | the alt dial's rate: + climbing from Europa till 600 s, then - falling to the sea (flips sign once, at 600) |
| `AltSeaM` | `RangeKm` in metres; from `bRealWorld` on it IS the capsule's height over the real sea |
| `G` (FVec), `GLoad` | the felt load in g and its size: the vector points where loose things fall. Pad (0,0,-0.13); ascent 1.5..1.7 along -Z; coast exactly 0; burn 0.3; entry up to 4.2 at 769 s, tilted a little to -X; under the canopy true down with the swing; splash spike 3.4 |
| `Shake` | 0..1 (ascent 0.3..0.55, entry up to 0.95, jolts) |
| `Plasma, PlasmaK` | 0..1 (722..807 s, peak 755) and its colour temperature 1800..7000 K |
| `Scorch` | 0..0.8 on the glass; stays through chute, splash and afloat; 0 again when the craft is back on Europa |
| `HullC` | heat shield, -120 in space, 1650 at 763 s, about 580 at the drogue, 60 at the water |
| `Whiteout` | 0..1: 0 at 840, 1 at 850, 0 at 860 |
| `bRealWorld` | false, then true from exactly 850.0 s (the whiteout's peak): swap to the real level there |
| `Chute, ChuteInflation` | `EChute::None/Drogue/Main`; 0..1 of the stage that is out (main: reefed 0.35, full at 831.5; falls to 0 after the splash) |
| `SwingPitchDeg, SwingRollDeg, ChuteYawDeg` | the canopy's pendulum (12° after the drogue, 7° after the main, 1.5° at rest) and the slow twist 0..35° |
| `PlanetDir, PlanetDiaDeg` | to the planet's centre; 26° .. 150° at the entry .. 180° at the water. Never shrinks |
| `PlanetIslandAxis, PlanetNorthAxis` | the planet's own axes (see "The planet impostor") |
| `EuropaDir, EuropaDiaDeg, EuropaVisible` | 180° (the ground) on the pad, 120° at the cutoff, 16° at 185 s, 2.4° at the end; `EuropaVisible` 0 when the planet hides it |
| `SunDir` | the star |
| `FrameX, FrameY, FrameZ` | the flight frame's axes seen from the cabin: the star sphere's orientation |
| `RollDeg, PitchDeg, YawDeg` | the horizon ball (not wrapped: roll runs 0..360 in the coast) |
| `SubLatDeg, SubLonRelDeg` | the point under the craft: latitude, and longitude MINUS the island's (east +) |
| `TrackArcDeg, IslandRangeKm, IslandBearingDeg` | arc and surface distance from that point to the island; compass bearing to it |
| `IslandVisible, IslandDir` | 0..1 over the horizon (0 all the coast, rises from 676 s, half at 680 s, 1 from 687 s); the direction to it |
| `I` | the instruments, below |
| `Lamps[24]` | below |
| `Switches[5]` | the toggles' positions |
| `Record` | the record player, below |

Continuous outputs are smooth (bounded per tick, checked) except at the **jolts** `F.P.Jolts()` = 700, 810, 825, 888 s,
where load, shake, volts, attitude under the chute may jump and ring for a second. `LyudFlight::Flatten(S, Out)` lists
every number of a state by name (a debug page for free).

## Frames

Both right-handed. **To Unreal's left-handed local axes: (X, -Y, Z).**

- **Cabin frame C**: X forward = out through the big porthole, Y LEFT, Z up at rest (the heat shield is at -Z, the couches
  face +X, the load pushes the hero along -Z).
- **Flight frame F** (inertial, fixed at ПУСК): the site on Europa: Z up, X horizontal toward the planet's azimuth,
  Y = Z x X. On the pad C = F, so **the craft must stand on its stage with the big porthole toward the planet's azimuth**
  (the planet is then 35° up, straight ahead, `PlanetDir` = (0.819, 0, 0.574)), or the first frame of the impostors jumps.
- The cabin never moves: put the impostors at `Dir * AnyDistance` in cabin-local space and scale them to the angular
  diameter (`radius = distance * sin(dia/2)` for a sphere seen from outside). Turn the star sphere so that its own
  X, Y, Z axes (as they were in the site's sky at launch) point along `FrameX, FrameY, FrameZ`. In Unreal terms the
  sphere's axes are `(FrameX.X, -FrameX.Y, FrameX.Z)`, `-(FrameY.X, -FrameY.Y, FrameY.Z)`, `(FrameZ.X, -FrameZ.Y, FrameZ.Z)`.
- The ball: `FRotator(PitchDeg, -YawDeg, RollDeg)` should be right (yaw is positive to the LEFT here); check by eye
  against `PlanetDir`. Reference: Europa's horizon on the pad and in the ascent, then (blended over the turn 195..225)
  the planet's local up-and-forward, so the coast reads pitch -90 (nose at the planet) with the slow roll, the approach
  -80..-31, the entry yaw 180 (heat shield first, the porthole looks back up the track), the chute pitch 0.

What the big porthole sees: the ice and the black sky (ascent, nose tilting 20° down), Europa dead ahead shrinking
(96..195 s), the planet dead ahead growing (225..590 s, with one slow roll), the limb 12° above the middle with the
island coming over it ahead (625..700 s), the limb behind and the plasma (entry), the horizon (chute).

## The planet impostor

The model's planet does not turn during the flight, and its coordinates are tied to the island (really: to the splash
point off the beach, which the engine picks; the model's "island" is that point).

- Orient the sphere so that the island's texel points along `PlanetIslandAxis` and its north pole along
  `PlanetNorthAxis` (they are 72° apart: the island is at latitude 18). That alone makes the track arrive over the
  island whatever the hour.
- Light it by the hour: the engine knows where the sun is over the planet relative to the island; `SunDir` is only the
  star sphere's disk. (The two may disagree by a few degrees; nobody can see both at once.)
- The globe instrument: crosshair at latitude `SubLatDeg`, longitude `island's + SubLonRelDeg` (also `I.Globe*`).
- The craft comes in heading about 70° (east-north-east) at the island; `flight.json` `planet.arrival_heading_deg`.

## The real world (850 s on)

At `bRealWorld` (the frame the whiteout is 1.0) hang the cabin `AltSeaM` (364.5 m) over the splash point. From there:
height = `AltSeaM` (15 m/s down at first, 7.7 m/s at 885 s, jets to 2 m/s, 0 at 888.0), tilt = `SwingPitchDeg` /
`SwingRollDeg`, twist = `ChuteYawDeg` about any heading you like. Ignore the impostor directions after the swap. At
888 the event `splash`; at 900 the phase is `Afloat` and the model stops moving the capsule: float it on the real waves.

## Instruments (`S.I`)

| Panel id | Field |
|---|---|
| all | `bPowered` (false: needles down, digits dark), `bLampTest` |
| `clock` | `ClockDigits[6]` = H H M M S S, `ClockSec` (stops at 00:15:00) |
| `range` | `RangeKm` (int) |
| `speed` | `SpeedShown` + `bSpeedMs` (km/s till the drogue, then m/s) |
| `alt` | `AltDialKm` + `bAltSea` (over Europa till 600 s, then over the sea: the needles swing across once) |
| `gmeter` | `GMeter` 0..6 |
| `horizon` | `S.RollDeg, S.PitchDeg, S.YawDeg` |
| `press o2 co2 temp` | `PressMmHg` (760; up to 787 in the entry), `O2Pct` (21 falling to 20.2, topped up every 4.5 min), `Co2Pct` (0.25 creeping to 0.9, scrubbed every 170 s), `TempC` (19, 31.5 at 815 s) |
| `volts fuel` | `Volts` (27.5; sags in burns, 26 in the plasma, dips at the pyros), `FuelPct` (100 -> 40 in the ascent, 31 after the burn, 22 at the drogue, 6 after the jets) |
| `hull` | `Hull01` (the bar) and `HullC` |
| `globe` | `GlobeLatDeg, GlobeLonRelDeg` |
| `crt` | all in the screen's 0..1 square, y up: Europa `CrtEuropaX/Y/R`, the planet `CrtPlanetX/Y/R`, the craft `CrtDotX/Y`, `CrtProgress` 0..1; draw the path with `LyudFlight::CrtPath(p, x, y)` for p 0..1 (32 points are plenty; the part before `CrtProgress` brighter); `CrtPhase` = the phase (word `flight.phase.<PhaseName>`), `CrtMode` 0..2 (the mode switch; what modes 1 and 2 show is yours) |
| comforts | `bCabinLight`, `bRadio` (only true with the power on) |

The needles are not damped by the model: lerp them a little.

## Lamps (`S.Lamps[L]`, `LampDef(L)`: id, colour, fault, rule text; 4 rows x 6, index = row * 6 + col)

Each has `bOn` (the rule holds), `bBlink`, `bLit` (the bulb right now: the model's own 0.6 s blink clock and the lamp
test applied; draw `bLit`). All dark without power.

| # | id | colour | rule |
|---|---|---|---|
| 0 | `power` | green | power switch |
| 1 | `air` | green | air switch; blinks under 700 mmHg |
| 2 | `gyro` | green | gyros switch; blinks for the 4 s spin-up |
| 3 | `hatch` | green | seal switch |
| 4 | `armed` | amber | armed, not launched; blinks |
| 5 | `engine` | white | ascent engine, 0..75 s |
| 6 | `sep` | white | 1.5..5.5 s and 700..708 s |
| 7 | `zero_g` | white | in flight before the entry, under 0.02 g |
| 8 | `burn` | amber | blinks 290..300 s, steady 300..306 s |
| 9 | `comm` | green | on with the power; off in the blackout; blinks 5 s after it |
| 10 | `blackout` | amber | the blackout |
| 11 | `g_high` | red | over 3 g |
| 12 | `hull_hot` | red | shield over 900 °C; blinks over 1400 °C |
| 13 | `drogue` | green | 810..825 s |
| 14 | `main_chute` | green | 825..893 s |
| 15 | `alt_low` | amber | under the canopy below 150 m (from 868.6 s); blinks |
| 16 | `jets` | white | 885..888.5 s |
| 17 | `splash` | white | from 888 s, afloat |
| 18 | `beacon` | white | from 890 s, afloat; blinks |
| 19 | `o2_low` | red | **fault**: O2 under 18 % |
| 20 | `co2_high` | amber | CO2 over 0.8 % (about 30 s before each scrub) |
| 21 | `batt_low` | red | **fault**: under 24.5 V |
| 22 | `master_caution` | red | any red lamp (steady); blinks 3 s after a refused switch |
| 23 | `record` | white | the FLIGHT RECORDER's tape: on for the whole flight (it is not the gramophone) |

A nominal flight lights every lamp but the two fault ones. `F.bFaultO2` / `F.bFaultBatt` (dev, saved) light those.

## Events (`F.TakeEvents()` -> `FEvent { Kind, Id, Arg, T, bAsleep }`)

`Kind` is `CueSound`, `CueSay` (Id = the hero's line key: play its voice and subtitle) or `CueAuto` (Id = `auto.<id>`:
voice `line.flightauto.<id>.1`, subtitle `flight.auto.<id>`). The timeline, in order (seconds):

`liftoff` 0, `auto.launch` 0, `stage_left` 1.5, say `liftoff` 8, `engine_cutoff` 75, `auto.cutoff` 76, `auto.weightless` 80,
say `weightless` 86, say `europa_behind` 185, `turn_start` 195, `auto.burn` 292, `correction_burn_on` 300,
`correction_burn_off` 306, say `planet_grows` 325, `auto.half` 450, say `ocean` 520, `auto.approach` 596,
`approach_start` 600, say `island` 683 (3 s after it is half over the horizon), `auto.sep` 694, `module_sep` 700,
`entry_interface` 720, `auto.entry` 721.5, `auto.blackout` 727, `blackout_start` 731.7, say `fire` 745, `peak_g` 769.2,
`blackout_end` 783.3, `drogue` 810, `main_chute` 825, `auto.chute` 827.5, say `chute` 833, `whiteout` 840, `real_world` 850,
`auto.splash` 868, `jets` 885, `splash` 888, say `home` 890, `auto.home` 896. (Say ids are `line.hero.flight_<id>.1`.)
No two spoken cues are closer than 3.5 s.

Outside the timeline: `switch_on` / `switch_off` (Arg = switch id), `switch_click` (Arg `light|radio|crt`), `buzzer`
(Arg = reason key), `auto.ready` (on arming), `launch` (the button), `nap_start`, `nap_end`, `nap_wake` (the automatic
end), `record_start` / `record_stop` / `record_side_end` / `needle_skip` (Arg = record id), `afloat` (900 s), say
`line.hero.flight_afloat.1` (once, the first tick afloat with `bHatchShut` false), `capsule_gone`, `craft_back`.

Continuous sounds (engine roar, plasma, wind under the canopy) are best driven by the state, not by events:
`bEngine`, `GLoad`, `Shake`, `Plasma`, `Chute`, `ChuteInflation`.

## The record player (`S.Record`: `Index, Id, bPlaying, PosSec, LengthSec, Pitch, Wow`)

- `SetRecords` gives the rack (ids + lengths). The record on the platter is remembered by id (through saves and a
  changed rack). An empty rack: `ui.flight.no.no_records` (the engine always gives at least the house record).
- `RecordNext()` puts the next sleeve on from the start (wraps); `RecordPut(i)` a chosen one; `RecordStop()` lifts the
  needle (position kept); `RecordNeedleDown()` drops it again (from the start after a side's end).
- A side ends: the needle lifts (`record_side_end`), nothing follows by itself.
- No start or needle-down in the `Entry` phase: `ui.flight.no.entry_player`. A record already turning plays on.
- `Pitch` (0.93..1.0: the platter drags under load) and `Wow` (0..1 flutter, from the shake): set them on the audio
  component. `PosSec` advances by real seconds x `Pitch`: start the sound at `PosSec` and resync if it drifts over ~0.3 s.
- `needle_skip`: while a record plays, at fixed flight moments (every 2.3 s) where the shake is over 0.6 (740..785 s of
  the entry: 20 in a flight) the needle jumps -3..+4 s: seek to the new `PosSec`.
- It turns in real time through the nap (a side will end while the hero sleeps).

## After the splash

`F.CapsuleAfloat()` (phase `Afloat`): the capsule is on the real sea; `F.FlightsFlown()` has gone up by one at 900 s.
It is gone (`capsule_gone`, then `craft_back`) when the hero has been over 300 m away for 60 s on end, or on the next
game day (also straight after loading a save from an earlier day). Then `F.CraftOnEuropa()` is true again: the cabin
stands cold on its stage (switches off, glass clean, platter empty). While afloat `CraftOnEuropa()` is false: draw the
stage on Europa without its cabin.

## Dev

`F.DevJump(T)`: launched, at flight second T, cues before T counted as fired (phase jumps: 0, 75, 600, 720, 810, 885;
899 for afloat in a second). `F.DevReset()`: back on Europa, cold. `F.Fast`: `lyud.Flight.Fast` (20 = the flight in 45 s;
identical results, checked). `F.bFaultO2`, `F.bFaultBatt`.

## Honest notes

- The trajectory is a dream, not orbital mechanics: the planet is 2000 km in radius, Europa is drawn 200 km in radius for
  the flight so it shrinks to "a coin" (16° at 185 s, 2.4° at the end), and the speed peaks at 22 km/s in a weightless
  coast because the planet's angular size was asked to grow at a steady relative rate (about 19 % a minute from 200 s).
  The ascent IS consistent with its load (1.6 g less Europa's 0.13 for 75 s = 1.05 km/s, 38 km), and the descent under
  the canopy is exact (height = the integral of the speed; the load = 1 g + the deceleration).
- The real world starts 364.5 m up: with a canopy that ends near 7 m/s and 35 s left, physics allows no more. Raise it by
  moving `times.whiteout_peak` earlier or `chute.tau_main` up (then the last seconds are faster than 7 m/s).
- The mission clock is plain HH:MM:SS of the flight second (during the nap it simply runs 8x).
- Open on the engine side: when to swap the real Europa site for the impostor after liftoff (`EuropaDiaDeg` is 174° at
  8 s, 153° at 30 s); what CRT modes 1 and 2 show; the hatch/wheel/belt interactions that feed the four inputs.
