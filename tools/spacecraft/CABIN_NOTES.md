# The cabin of «Ласточка» in the engine (`ALyudSpacecraft`)

Code: `unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Spacecraft/LyudSpacecraftCabin.cpp` (every method of
`ALyudSpacecraft`; the class is declared in `Public/Spacecraft/LyudSpacecraft.h`). All of its logic sits in one nested
struct, `ALyudSpacecraft::FCabinData`, so nothing can clash with the director's file in a unity build.

Check before the Mac: `python3 tools/spacecraftsim/cabincheck.py` (`-v` lists what it found, `--no-parse` skips clang).
It compares every name the .cpp uses with `kit.json`, `import_spacecraft.py`, `FlightModel.h`, the glb and the header,
simulates the aiming from the seat and from the floor, and parses the file with clang against a stand-in engine
(`tools/spacecraftsim/cabinstub/CoreMinimal.h`: assumed signatures, NOT the real headers). Exit 1 on any bad name.

NOTHING here has run in Unreal. The file has only been parsed against the stand-in; the first real compile is on the Mac.

## What `Build()` makes

The cabin frame is the actor's frame and `kit.json`'s frame (cm, +X forward to the big porthole, +Y the hero's right).
Every component is made at runtime (`RF_Transient`), a missing mesh or material is skipped with one log line.

- Inside set: `SM_SC_CabinInner`, `Panel`, two `Couch`, `Player`, `Platter` + `Record` (child of the platter),
  `Tonearm`, 8 `Sleeve`, `Thermos`, `Cap`, `Book`, `Blanket`, 8 `Toggle`, `LaunchButton`, `LaunchCover`, `ChuteHandle`,
  `ChuteCover`, `Globe`, `Ball`, 9 `Needle`, three cabin lamps (point lights, candelas, no shadows).
- Outside set: `SM_SC_CabinOuter`, `FireShell`, the chute rig (`Chute`, `Drogue`), one light for the landing jets.
- In both: `Hatch` + `HatchWheel` (child of the hatch), the three porthole glasses (`Glass_Big`, 2 x `Glass_Side`).
- Dynamic materials, each made ONCE and shared: labels, dials, needle, ball, globe, digits, bar, lamps, CRT, glass,
  the launch cover's glass, plasma, the record's label, the lamp lenses, 8 sleeves.
- Collision: the kit exports none, so the cabin is boxed by hand (`BuildCollision`): floor, heat shield, a 16-box ring
  for the cone and one for the dome (left open at the hatch, with a sill, a lintel and a plug that follows the hatch),
  roof, console, player shelf, couch seats and backs.
- `SM_SC_Stage` is not ours (the stage actor's).

Not done, on purpose: no fire light (the director has its own at `port_big`), the fans do not turn, belts and guards
are part of their meshes (`buckle` only keeps a flag), the heat shield has no glow parameter, `PlasmaKelvin`, `SunLocal`
and `Time` of `FExterior` are not used (no material parameter takes them).

## kit.json keys read

`afloat.waterline_z`; `cabin.{ceiling_z, inner_radius_floor, inner_radius_knee, knee_z, outer_radius, jets}`;
`chute.{attach, cover.closed, cover.open, handle.rest, handle.pulled}`; `fans[].at`;
`hatch.{axis_out, centre, clear_radius, closed, open, sill_z, wheel.in_hatch, wheel.turns_to_open}`;
`interact.<id>` (17 ids); `lamps[]`; `panel.{centre, speaker, ball.frame, globe.frame, dials[], digits.windows,
toggles[], launch.button, launch.pressed, launch.cover_closed, launch.cover_open}`;
`pieces.SM_SC_Panel.bounds`, `pieces.SM_SC_Player.bounds`;
`player.{frame.at, platter, record, sleeve_rack[], tonearm.rest, tonearm.lead_in, tonearm.run_out, tonearm.lift_deg,
tonearm.length}`; `portholes.{big,left,right}.{glass, look.camera, look.direction, look.interact}`;
`props.{thermos, book, blanket_and_cap}`; `seat.{pelvis, eyes, look_direction, couch_yaw, couch_frames,
pelvis_in_couch}`; `stand.{centre, inside_hatch, outside_hatch}`.

Every one has a built-in default (the kit's value on the day of writing), so a missing key moves nothing.

## How the instruments behave (`FCabinData::Instruments`, from `ApplyState`)

- Needles: a spring (W = 13, zeta = 0.6) toward the value, so they overshoot a little; a tremble of 1.6 deg x `Shake`;
  drawn in steps of 0.1 deg. The altimeter's two hands wrap; its unit is 1 km below 100 km and 100 km above (it changes
  back at 95).
- Digits: clock HHMMSS, range, speed (km/s x 100 with the point shown, or m/s).
- Lamps: 24, in the order of `FlightModel.h`'s lamp table; the lamp test lights all. Without power all are dark except
  lamp 0, which blinks slowly in amber (stand-by).
- Backlight and CRT: fade with the power (eased, 32 steps); `sw_crt` is a momentary switch that turns the page with a
  flick of the picture.
- Attitude ball: caged (level) until the gyro switch is on, then follows roll, pitch, yaw.
- Globe: turns even without power (it is clockwork); shows the point under the craft on the planet map.
- Hull bar, CRT pages, G: straight from the state.
- Toggles 0..4 follow `FState::Switches`, 5 `bCabinLight`, 7 `bRadio`, 6 springs back.
- Cabin lamps: 22 cd each, eased; the lenses glow with them.
- Weightless: the thermos, the cap and the book drift a few centimetres on a slow noise and settle when weight returns.
- The record player: platter 200 deg/s x spin x pitch, the arm from rest over lead-in to run-out, the played record's
  sleeve stands in front of the rack.

Nothing ticks while both sets are hidden; a parameter or a transform is set only when its shown value changes.

## Not verified: the one place to flip each

Line numbers are of the day of writing; search for the name.

| What may be wrong | Where (one line) |
| --- | --- |
| Needles turn the wrong way | `NeedleTurn`: the sign of `ADeg` |
| Globe runs east instead of west | `GlobeTurn`: the sign of `LonDeg` |
| Ball pitches / yaws / rolls backwards | `BallTurn`: `PitchSign`, `YawSign`, `RollSign` |
| CRT horizon tilts or climbs backwards | `CrtAtt`: the sign of either component |
| Globe's track does not pass over the island | `ModelToMap`: `IslandLatModel`, `MapLat`, `MapLon` |
| Platter turns anticlockwise | `TickParts`: the sign of `200.f` in `PlatterDeg` |
| Hatch wheel turns the wrong way | `ApplyParts`: the sign of `WheelTurns * 360.f` |
| Stand-by lamp not wanted | `Instruments`: `bStandbyLamp = false` |
| Thermos at the mouth / book in the hands look off | `LoadKit`: `ThermosSip`, `BookHeld` (made up from the seat eye) |
| Played sleeve stands badly | `LoadKit`: `SleeveOutAt` (made up: 14 deg, in front of the rack) |
| Lamp lenses do not glow | `FParamNames`: `EmissiveStrength`, `EmissiveFactor` (names guessed for the glb's own material) |
| Cabin lit wrongly on Europa or on Earth | `ApplyLight` / `ApplyShown`: the three `SetLightingChannels` calls (Europa: 1 and 2; real world and afloat: 0) |
| Console / shelf collision on the wrong side | `BuildCollision`: the kit's piece bounds are in Blender's Y, mirrored here |
| Cabin too dark or too bright | `ApplyLight`: `22.f` cd per lamp; jets `12000.f` cd in `SetJets` |
| Chute swings the wrong way | `SetChute`: `FRotator(Swing.X, Yaw, Swing.Y)` |
| Material slot names differ after the import | the `MakeMid(..., TEXT("slot"), ...)` calls: taken from the importer's `slot_name`, never seen in the editor |

Also assumed: the hero standing at `stand.inside_hatch` overlaps the hatch lintel box by a little (as he does in the
real kit); the seated reach is NOT the 95 cm of ENGINE_NOTES (the switches are 110..142 cm from the seat's eye), each
target has its own: panel 150, near things 120, portholes 175, the net 190, hatch 165, couch and player 140.

## First run on the Mac

Once it compiles, with `lyud.Flight 1`, on Europa by the craft:

1. Outside: the cabin stands whole, the hatch is in its hole, the glasses are in, no fire shell, no chute.
2. Hatch: E opens it outward, the wheel turns with it; walk in without snagging on the sill; close and seal from inside.
3. Walk round the cabin: no falling through the floor, the console, both couches and the shelf are solid.
4. Sit, buckle. Aim at each of the eight switches, the cover, the button, the three portholes, the thermos, the book,
   the player, the chute cover: each prompt appears on the right thing and not on its neighbour.
5. Power off: the panel is dark, one amber lamp blinks. Power on: backlight and CRT fade up, needles swing to their
   values with a small overshoot; lamp test lights all 24.
6. Gyro on: the ball uncages. In flight: nose up shows sky colour rising the right way, a roll to the right tilts the
   horizon the right way on BOTH the ball and the CRT, and they agree with each other.
7. Globe: at launch the island is under the crosshair; the track then moves the way the craft flies.
8. Altimeter: the long hand goes clockwise on the climb; the unit changes at 100 km without a jump of the short hand.
9. Digits: the clock counts, range and speed read sensibly, the point shows in km/s.
10. Launch: the cover lifts, the button goes in and comes back; needles tremble under thrust.
11. Weightless: thermos, cap and book drift and come back; E on the thermos brings it to the mouth, on the book to the
    hands.
12. Player: a record on the platter turns clockwise, the arm comes over, the sleeve stands out; pitch follows `SetRecord`.
13. `sw_light`: the three lamps and their lenses go out and on; `sw_crt` flicks the page; `sw_radio` lever follows.
14. Re-entry (`lyud.Flight.Phase`): frost, scorch and the plasma on the glass, the fire shell outside; the hull bar rises.
15. Chute: cover up, handle down and back; drogue then main open over the cabin and swing the right way; jets flash.
16. Afloat: the waterline is where the water is, wet glass, the hatch opens to the sea, daylight lights the cabin.
17. Language: switch it, the panel's labels and the dials change with no hitch.
18. Leave to the island and come back: nothing of the cabin is left, and it builds again.
