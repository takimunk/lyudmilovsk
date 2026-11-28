# «Ласточка»: the engine side (the director)

`Public/Spacecraft/LyudSpacecraft.h` declares four classes:

| Class | Owner | File |
|---|---|---|
| `ALyudSpacecraft` (the cabin) | the CABIN programmer | `Private/Spacecraft/LyudSpacecraftCabin.cpp` |
| `ALyudSpacecraftStage` (the landing stage on Europa) | the director | `Private/Spacecraft/LyudSpaceflight.cpp` |
| `ULyudFlightCamera` (the view in the cabin) | the director | same |
| `ULyudSpaceflightSubsystem` (the director) | the director | same |

## For the cabin programmer

You implement **every method of `ALyudSpacecraft`** declared in the header (constructor, `Build`, `ApplyState`,
`ActionAt`, `ActionPoint`, `PlayActionAnim`, `IsAnimating`, `SeatEye`, `SeatPelvis`, `PortholeView`, `HatchExit`,
`HatchInside`, `StandPoint`, `WaterlineZ`, `IsInside`, `SetExterior`, `SetChute`, `SetJets`, `SetRecord`,
`SetCabinLight`, `SetShownInside`, `SetShownOutside`, `Unbuild`, `Tick`, `EndPlay`). The director's file does not
define any of them. Add private members, components and helpers as you like (the `protected:` block and `FCabinData`
are yours; delete `Data` in `EndPlay`). Include the model yourself: `#include "Spacecraft/FlightModel.h"` (never
`using namespace LyudFlight`: it has its own `FQuat`).

**The cabin decides nothing.** No flight logic, no input, no prompt, no toast, no flight sounds, no save. It builds,
draws, animates and answers geometry questions. The director owns every logical state (hatch open, wheel sealed, cover
up, seated, buckled, the record) and every sound of SOUND_NOTES.md.

### Frame

The actor's own frame IS kit.json's cabin frame (origin = the middle of the cabin's floor, +X forward, +Y the seated
hero's right, +Z up). All kit points are used as they are (already UE cm, left-handed). The model's vectors
(`FState::G`, `PlanetDir`...) are right-handed: UE local = (X, -Y, Z). The director moves and tilts the ACTOR (the
ascent over the real Europa, the swing under the canopy, the bobbing afloat): keep everything attached under the root
and never set world transforms of your parts.

### What the director calls, and when

| Call | When | Expectation |
|---|---|---|
| `Build()` | once, right after `SpawnActor` (the hero may be anywhere). Again after `Unbuild()` | everything made, hidden, cold. Missing meshes: log a warning (`Spacecraft: missing ...`), do not crash |
| `SetShownInside(b)` / `SetShownOutside(b)` | only on change. Inside = the hero is within ~8 m of the hatch on Europa, aboard, or the capsule is afloat and he is within 40 m. Outside = the craft is on Europa and the hero is on Europa, OR the hero is aboard (the fire shell, the chute and the glass are seen from inside), OR afloat | hidden sets: no tick work, no shadows, no collision queries needed. With both hidden the actor's tick is off (`SetActorTickEnabled(false)` is yours to call) |
| `ApplyState(State, Dt)` | every tick while either set is shown, AFTER the model's `Advance`, in the subsystem's tick (before the camera). `Dt` = real seconds | instruments per MODEL_NOTES "Instruments"/"Lamps"; `State.Switches[5]` = the five toggles' positions (animate the levers toward them yourself); `State.I.bCabinLight`, `bRadio`, `bPowered`, `bLampTest`; `State.Record` is NOT for you (see `SetRecord`). No allocation. Set MID parameters only on change |
| `SetExterior(E)` | every tick after `ApplyState` while shown | `Plasma`, `PlasmaKelvin`, `Heat` (0..1 = `I.Hull01`), `Scorch`, `Frost`, `Wet`, `Fog`, `SunLocal` (UE local unit), `Time` (flight s). The fire shell is visible only when `Plasma > 0.002`. `Fog` 1 = the glass is opaque white (the director swaps the world behind it at that frame) |
| `SetChute(Stage, Inflate, SwingDeg, YawDeg)` | every tick from the drogue (810 s) to a few seconds after the splash; `Stage` 0 once after that | 0 none, 1 drogue, 2 main. The director already tilts the cabin actor by the model's swing; `SwingDeg` is the canopy's extra lean against the cabin's own axis (the lines lag: about -0.5 x the cabin's tilt), `YawDeg` the twist |
| `SetJets(J)` | 885..888.5 s | flames/glow under the shield, 0..1 |
| `SetRecord(Cell, bSpinning, Arm01)` | on change, and every tick while a record plays (the arm creeps 1 -> 2 across the side) | `Cell` -1 = empty platter; 0..7 = records.json `cell` (label atlas cell on the disc, sleeve in the rack lifted or marked) |
| `SetCabinLight(b)` | on change (the nap dims the cabin whatever the switch says) | `false` forces the three lamps off; `true` hands them back to `ApplyState` |
| `ActionAt(Eye, LookDir, &Where)` | every tick while the hero is aboard or within 4 m of the hatch outside | the id of the interaction point he looks at (reach: 120 cm from the eye standing, 95 cm seated to panel points: use kit `interact` and your own radii; cone: about 12 degrees, wider when nearer), or empty. Pure geometry: do NOT filter by phase or by seated/standing. Must not allocate except the returned FString |
| `ActionPoint(Id)` | when a sound is played | world point; unknown id: the cabin's middle at 1 m |
| `PlayActionAnim(Id, bOn, bSnap)` | when the director changes a logical state (see the header's list) | animate; `bSnap` = no animation (a load, a dev jump, the craft back on Europa) |
| `IsAnimating("hatch")`, `IsAnimating("wheel")` | while the hero goes through the hatch | true until the part has stopped |
| `SeatEye()`, `SeatPelvis()`, `PortholeView(big/left/right)`, `HatchExit()`, `HatchInside()`, `StandPoint()` | any time after `Build` | WORLD transforms/points through the actor's current transform (it moves) |
| `WaterlineZ()` | afloat | kit `afloat.waterline_z` (local cm) |
| `IsInside(World)` | every tick near the craft | inside the pressure shell (a capsule test in local space is enough) |
| `Unbuild()` | after the capsule is gone and the hero is not on Europa | optional: drop mesh references so the GC frees them; a no-op is fine |

Action ids the director understands (anything else is ignored): `hatch`, `wheel`, `sit` (either couch's seat: the
hero always takes his own), `buckle`, `sw_power`, `sw_air`, `sw_gyro`, `sw_seal`, `sw_arm`, `cover`, `launch`,
`sw_light`, `sw_crt`, `sw_radio`, `port_big`, `port_left`, `port_right`, `player` (the platter/sleeves: next record),
`needle` (the arm), `thermos`, `book`, `nap` (the headrest or the folded blanket), `chute_cover`, `chute_handle`.
The director adds by itself, without the cabin: `stand`, `unbuckle`, `look_back`, `wake`, `book_close`, `swim_out`,
`enter`, `exit`, `ladder`, `ladder_down`.

`ActionPoint` extra ids used for sounds: `speaker`, `panel`, `couch`, `player`, `hatch`, `fan_0`, `fan_1`, `hull_0`..
`hull_3`, `port_big`, `port_left`, `port_right`, `jets`.

### Collision

The hero WALKS in the cabin (parked, the weightless coast, afloat): the inner shell needs collision the capsule
(radius 34, half height 96: kit `stand.capsule`) can stand on and not fall through, with the hatch's opening passable
is NOT required (the hero goes through the hatch as a scripted move with his collision off). Use complex-as-simple on
`SM_SC_CabinInner` (or UCX hulls if the kit has them) on `ECC_WorldStatic`, blocking `ECC_Pawn`; the outer shell needs
collision too (the hero swims against it afloat and stands by it on Europa). Collision stays ON while the actor is
hidden on Europa only if the hero is on Europa; the director calls `SetActorEnableCollision` itself.

The roof matters: `ALyudSky::IsPlayerOutdoors` traces upward, and the soundscape muffles the outside when it says
"indoors". A cabin with a blocking roof makes the sea's sound muffled inside by itself.

### Lighting rules (the project's perf checklist)

Every mesh: `SetMobility(Movable)`, `bAffectDistanceFieldLighting = false`, `bAffectDynamicIndirectLighting = false`
(the cabin moves). Cabin lamps: point lights without shadows except at most one. Lighting channels: on Europa the star
is channel 1 and the planet's fill channel 2 (`SetLightingChannels(false, true, true)` for the OUTER set as
`ALyudEuropaSite` does); the interior set stays on channel 0 + 1 so that the star shines in through the portholes.
MIDs: created once in `Build`, `UMaterialInstanceDynamic::Create(Base, this)`, kept in `Mids`. No particle systems.

### What the director does that you might expect to do

- All sounds (loops attached to your root with `ULyudSoundscapeSubsystem::AttachLoop`, one-shots at `ActionPoint`).
  If you want the two fans as positional loops, tell the director (do not attach audio yourself).
- The plasma's flickering point light inside (`FireLight`, attached to your root at the big porthole).
- The camera (seat eye, leaning to a porthole, the shake, the load's sink).
- The hero's body: `AHeroCharacter::SitOn(Cabin->GetRootComponent(), SeatLocal, SeatZ)` with `SeatPelvis()`.
- The sky outside the portholes (planet, Europa, stars: separate meshes round the cabin, not yours).

## Interface changes

(append here: date, what, why)

- none yet
