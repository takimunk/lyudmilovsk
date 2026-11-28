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
| `ActionAt(Eye, LookDir, &Where)` | every tick while the hero is aboard or within 5 m of `HatchExit()` outside | the id of the interaction point he looks at (reach: 120 cm from the eye standing, 95 cm seated to panel points: use kit `interact` and your own radii; cone: about 12 degrees, wider when nearer), or empty. Pure geometry: do NOT filter by phase or by seated/standing. Must not allocate except the returned FString |
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

No signature of `ALyudSpacecraft` has changed since the header was first written. Clarifications of what the director
actually does (2026-11-28, when `LyudSpaceflight.cpp` was finished):

- `ActionPoint(Id)` is also asked with ACTION ids as the place of their one-shot sounds: `sw_power` .. `sw_arm`,
  `sw_light`, `sw_crt`, `sw_radio`, `cover`, `launch`, `thermos`, `book`, `chute_cover`, besides the extra ids listed
  above. An unknown id falls back to the cabin's middle, so nothing breaks; a real point sounds better.
- `PlayActionAnim` ids the director sends: `hatch`, `wheel`, `cover`, `chute_cover`, `buckle` (logical states, with
  `bSnap` on a load / dev jump / the craft back on Europa), and one-shots with `bOn = true`: `launch`, `thermos`,
  `chute_handle`; `book` is open/closed.
- The inside set is shown within 11 m of the cabin's origin on Europa (not 8 m of the hatch), during the whole flight,
  and afloat within `spacecraft.json` `afloat.hero_far_cm` (40 m). `ActionAt` is asked while the hero is aboard or within
  5 m of `HatchExit()`.
- `SetRecord` is sent only when the cell, the spin or the arm (by more than 0.002) changed.
- `SetCabinLight(false)` is sent while the hero naps, `true` otherwise (on change).
- The two fans as positional loops are not used: `sc_air_hiss` sits at `fan_0`, the hum at `panel`.

From the cabin (2026-11-28, `LyudSpacecraftCabin.cpp` finished; no public signature changed, details in
`CABIN_NOTES.md`):

- Protected members added to `ALyudSpacecraft`: `SharedMeshes`, `Parts`, `JetLight`, `OnLanguageChanged`,
  `LanguageHandle`.
- Collision is hand-made boxes (the kit exports none), so `SetActorEnableCollision` covers all of it.
- The cabin makes no fire light of its own; `SetJets` drives one point light at `cabin.jets`.
- `PlayActionAnim("sw_light" / "sw_radio")` does nothing: those levers follow `FState::bCabinLight` / `bRadio`.
  `sw_crt` flicks the momentary lever and the picture. `buckle` only keeps a flag (the belts are in the couch's mesh).
- `IsAnimating` is false while both sets are hidden (nothing ticks then: a move started while hidden plays on when the cabin is shown again, unless sent with `bSnap`).
- `ActionAt` reaches are per target (panel 150 cm, near things 120, portholes 175, net 190, hatch 165), not 95 / 120:
  the switches are 110..142 cm from the seat's eye in this kit.
- `ActionPoint` also knows `speaker`, `panel`, `couch`, `jets`, `fan_0`, `fan_1`, `hull_0` .. `hull_3`.

## Architecture (the director, `Private/Spacecraft/LyudSpaceflight.cpp`)

`ULyudSpaceflightSubsystem` (a tickable world subsystem, saved under the key `Spaceflight`) owns one
`LyudFlight::FFlight` inside a private `FImpl` (made in `OnWorldBeginPlay` when `lyud.Flight` is 1, freed in
`Deinitialize`). It spawns three actors once the Europa subsystem has its site (asked again once a second until then):
`ALyudSpacecraftStage`, `ALyudSpacecraft` and a plain actor that holds the sky round the cabin. Nothing is loaded at the
spawn: the stage's and the cabin's `Build()` run when `/Game/Spacecraft` and `/Game/Audio/Spacecraft` have come in
through `ULyudPrefetch::FolderReady`, which is asked from the airlock on (or at once on a load / dev jump into the
flight).

**Awake / asleep.** The tick does its work only while the hero is in the airlock, on Europa, aboard, in flight, or the
capsule is afloat. On falling asleep (`FImpl::SetActive(false)`) the loops and the music stop, the 46 sound pools are
unloaded, the cabin is `Unbuild()`-ed and both folders are let go (`ULyudPrefetch::ReleaseFolder`).

**Everything follows the state.** Each tick: `Feed` (the four facts, the game day, `lyud.Flight.Fast`, the star in the
flight frame while parked) -> `Advance` -> `TakeEvents` (`HandleEvent`: one-shots, the voice, the hero's lines, quest
events) -> `State()`. Which world the cabin is in, whose sky observer is installed, whether the real site is hidden and
which sets are shown are all derived from that state every tick, so a load or `lyud.Flight.Phase` lands right without
special cases. Then `PlaceCabin`, `ApplyState`, `TickExterior` (`SetExterior`, chute, jets, the plasma's light, the
panel's glow), `TickSky`, `TickHero`, `TickSounds`, `TickRecord`, `TickCamera`, `UpdateAction` + the polled E.

**Where the cabin is.**
- Parked: on the stage, 60 m from Europa's hatch toward the planet's azimuth (the big porthole that way, yaw only). The
  four pads are traced onto the site's ground once it is built; the stage stands level at their mean height.
- 0 s .. `SwapT` (the second the model's height reaches 3 km, about 20 s; found once from the profile): the REAL cabin
  climbs straight up over the REAL ice. The cabin is always drawn upright; the model's small nose tilt in the climb is
  not shown.
- From `SwapT` to 850 s it hangs still at that spot (inside Europa's 6 km "on Europa" radius, so the space view, the
  vacuum and the light channels stay as they are with no hook into the Europa subsystem). The site is hidden
  (`ALyudEuropaSite::SetShown(false)`), Europa becomes a sphere astern, and over `blend_seconds` (12 s) the universe is
  eased from the site's frame onto the model's: `Fix = Slerp(I, Qm, w) * Qm^-1`, `Qm` = the flight frame seen from the
  cabin (`FrameX/Z`), world direction of a model vector = `Pad * Fix * (X, -Y, Z)`. From then on the stars, the planet,
  Europa and the sun (through `ALyudSky::SetSpaceObserver`) turn round the upright cabin exactly as the model says.
- 850 s (`bRealWorld`, the glass is opaque white): the cabin is put over the splash point at `AltSeaM` over the still
  sea, coming down a 0.22 slope along the splash heading; the Europa subsystem notices the hero is gone and takes its
  space view, vacuum and gravity back by itself; its own sky observer is handed back
  (`ULyudEuropaSubsystem::InstallSpaceObserver`). The point is moved out to sea in 20 m steps while the game's own
  depth there is under 3 m.
- 888 s on: over 2.5 s the real waves take over (`ALyudSea::SampleSurface`: height, tilt from the normal, plus a slow
  roll and bob from `spacecraft.json` `afloat`). Afloat the sea keeps a dry box round the cabin (`SetDryZone`).

**The sky round the cabin** (one planet at a time: the site's own planet, air and stars are hidden while the rig is
up, i.e. while the hero is aboard or the craft flies): the planet on `SM_EU_Planet` with `M_SC_PlanetNear` at
`draw.planet_cm`, scaled to the model's angular diameter (half-angle capped at 88 degrees), oriented so the splash
point's texel lies on `PlanetIslandAxis` and the pole 35 degrees from it toward `PlanetNorthAxis` (MATERIAL_NOTES'
second option: `IslandLat/Lon` stay the map's); lit by the ephemeris' sun over the town; `M_SC_PlanetAir` shell;
Europa with `MI_EU_ice_crust`; the stars with `M_EU_Space`, starting from the site's own star sphere rotation. On the
pad the model's planet (35 degrees up) is turned onto the site's real one and that correction is eased off during the
climb, so nothing jumps when the hero boards.

**The hero.** Not aboard / aboard is geometry (`IsInside`) unless seated. Through the hatch he is moved as a scripted
arc in the cabin's frame (collision off, the sea told to leave him alone). Seated = `AHeroCharacter::SitOn` on the
cabin's root; Space asks to get up (`SeatLeave` sets a flag; the belt first, then `CanStand`). On his feet in flight his
`GravityScale` is the model's load (0.015 in the coast, with a little drag); 12 s before the approach the couch takes
him back and buckles him. The camera is a `UCameraModifier` (`ULyudFlightCamera`): eye on `SeatEye()` or a
`PortholeView`, the shake, the sink and the narrowed view under load, the eyes shut in the nap.

**E** (one prompt, `ui.flight.<key>`; polled, never consumed). What the cabin answers is resolved by the logical
state: `hatch` open -> `enter` / `exit` / `swim_out`; shut and sealed -> `wheel` (unseal); looking at the wheel with the
hatch open -> close it. Looking at nothing: `book_close`, `look_back`, or, looking down at the lap, `unbuckle` then
`stand`. Sitting does not buckle; the `buckle` point does. `launch` with the cover down lifts the cover first.

**Records.** The rack is the craft's own eight sleeves (`Content/Spacecraft/records.json`, written by
`tools/spacecraft/records_strings.py`); there are no pocket items (no icons exist), so `dev.action.flight.records`
just puts the next record on. The sound is `/Game/Audio/Music/MU_SC_<short>` if imported, else
`/Game/Spacecraft/Music/<id>`, else the record's fallback cues `MU_<cue>`, else the house cue. A fallback shows the
"ship's own record" toast, a real record its title (`flight.rec.<short>.title`). The model's `PosSec`, `Pitch` and `Wow`
drive the audio component; a drift over 0.35 s (a needle skip) seeks.

**Save.** `SavedFlight` (the model's string), where the hero was (0..3), hatch and wheel, the capsule's place afloat,
the first-landing toast. On a load the restore runs in the next tick; a save made in flight always comes back seated
and buckled.

## Hooks made in other systems (all small)

| File | What | Why |
|---|---|---|
| `Public/World/LyudPrefetch.h`, `Private/World/LyudPrefetch.cpp` | `static void ReleaseFolder(Ctx, Folder, Skip)` | lets go of what `FolderReady` held, so the craft's assets are freed after the flight |
| `Public/Audio/LyudSoundscape.h`, `Private/Audio/LyudSoundscape.cpp` | `LoadPool(Group, Id)`, `UnloadPool(Id)`, UPROPERTY `LazyRefs` | the 46 `sc_*` pools are loaded with the craft and dropped after, instead of SOUND_NOTES' resident `Groups` row (they would sit in memory in every world) |
| `Public/Europa/LyudEuropa.h`, `Private/Europa/LyudEuropa.cpp` | `InstallSpaceObserver()` (the lambda moved out of `TrySpawn`, which calls it), `GetSiteOrigin()`, `GetSpaceRadius()` | `ALyudSky` has one observer seat: the flight takes it and hands it back |

Nothing else was edited. `Lyudmilovsk.Build.cs` needs no change (no include paths: `Public/` and `Private/` roots).
Console commands, dev-mode registrations and the save key live in `LyudSpaceflight.cpp` itself. Not done by me, noted
by the words agent: `tools/gen_voice.py` `VOICED_SUBS` needs `"spacecraft"` (paid voicing), and the diary rule for
`flight_landed` in `tools/daylog/content.py`. Quest events fired: `ULyudQuests::Notify("flight", "launched")` and
`("flight", "landed")`; no quest uses them yet.

Unity builds: the file's helpers are in `namespace LyudFlightImpl` with `Sc` prefixes (`ScReadJson`, `ScPlayerHero`...)
because `LyudEuropa.cpp` has the same helpers unprefixed in its anonymous namespace.

## Console and developer mode

`lyud.Flight 0` (not spawned), `lyud.Flight.Fast <x>`, `lyud.Flight.Shake <x>`, `lyud.Flight.Detail <0..1>`
(scalability). Commands: `lyud.Flight.Use`, `.Report`, `.GoCraft`, `.GoCabin`, `.Launch`, `.Phase <id or seconds>`,
`.Reset`, `.Do <action id>`, `.Look <big|left|right|back>`, `.Record`, `.SwimOut`. F7 group `flight`: teleports
`flight_craft` (the ladder's foot), `flight_cabin` (standing in the cabin on Europa: a dev teleport stands a seated
hero up, so it cannot seat him; `flight.start` does), actions `flight.start`, `flight.phase_<nine ids>`,
`flight.fast`, `flight.records`, `flight.reset`. `tools/check_devdata.py`: 0 problems.

## Mac checklist (nothing here was compiled or run: there is no Unreal in the cloud)

1. `python3 tools/spacecraft/flight.py --check && sh tools/spacecraftsim/run.sh` (23 131 checks),
   `python3 tools/spacecraft/site.py --check` (run `site.py` again once the terrain is built on the Mac: here it used
   the macro relief, see below), `python3 tools/spacecraft/records_strings.py --check`,
   `python3 tools/spacecraft/strings.py --check`, `python3 tools/check_devdata.py`.
2. Build the kit and import it: `blender/spacecraft/spacecraft_kit.py` (KIT_NOTES), then
   `unreal/scripts/import_spacecraft.sh` (meshes, textures, materials; MATERIAL_NOTES).
3. Sounds: `python3 tools/spacecraft/sounds.py` if the wavs are not there, then the `IMPORT_ONLY=sfx/spacecraft`
   import of SOUND_NOTES ("Importing"). The pools must come out as `/Game/Audio/Spacecraft/S_sc_<id>_<nn>`.
4. Records (optional; the game falls back to its own cues): `python3 tools/spacecraft/fetch_records.py --pin`, then the
   import in RECORDS_NOTES / MATERIAL_NOTES.
5. Compile. Expect first-compile errors in `LyudSpaceflight.cpp` (3 600 lines never compiled) and
   `LyudSpacecraftCabin.cpp`; the likeliest places are listed under "Unverified".
6. `sh unreal/scripts/test_spaceflight.sh` (screenshots in `Saved/SpaceflightTest`), then again with `SOUND=1`.
   `SPACEFLIGHTTEST_ONLY=pad` stops before the launch. Then fly it once by hand at normal speed with the sound on.

## Unverified (guesses to check first)

- **Nothing compiled.** API calls were copied from call sites in this repo, except: `APlayerCameraManager::
  SetManualCameraFade` (the nap), `UAudioComponent::SetPitchMultiplier` and `Play(StartTime)` (the record),
  `FQuat::FindBetweenNormals`, `FRotationMatrix::MakeFromXZ(...).ToQuat()`, `TArray::RemoveAtSwap(i)`: from memory.
- **The ball mapping.** The planet's orientation and its sun assume MATERIAL_NOTES' statement that the mesh's local
  point is `(cos lat cos lon, -cos lat sin lon, sin lat)`. If the island comes up mirrored or the terminator is on the
  wrong side, the fix is in `FImpl::TickSky` (the sign of the `CrossProduct(Zw, Mw)` term, or of the east vector).
- **Handedness of the flight frame.** `ScUe` maps the model's (X, Y, Z) to (X, -Y, Z); if the star sphere turns the
  wrong way in the roll of the coast, `Qm` in `TickSky` is the place.
- **The swap at 3 km.** Europa's impostor has the right angular size but is `MI_EU_ice_crust` on a smooth ball: a pop in
  colour and relief is likely at `SwapT`. `blend_seconds` and `real_ascent_alt_cm` are in `spacecraft.json`.
- **Boarding.** The site's planet (`M_EU_Planet`, turned with the day) gives way to `M_SC_PlanetNear` (island fixed to
  the model) the moment the hero is inside the cabin: same place and size, but the cloud pattern and the island's
  place on the disc may differ. `CloudShift` is not matched to the site's.
- **The real world starts 364.5 m up**, not the 1200..1500 m asked: the model's checks hold the speed before the jets
  at 7 +- 1.5 m/s and the `real_world` cue at 850 s. One retune (`tau_main` 14 -> 30) was tried and reverted: 21 checks
  failed. The model and `flight.json` are untouched; `run.sh`: 23 131 passed, 0 failed.
- **The splash point** was chosen on the macro relief (`spacecraft.json` `"terrain": "macro"`): reefs and the built
  terrain are not in it. The game re-checks the depth at the swap and moves out to sea if needed.
- The ladder is the generic interactive ladder: its prompt is the ladder subsystem's, not `ui.flight.ladder` /
  `ladder_down` (those two keys are unused). Its `TopExit` semantics were taken from the kit's `top_exit`.
- Collision in the cabin while it bobs afloat, and the hero walking in it, depend on the cabin's collision set-up.
- `AHeroCharacter::SitOn` keeps the body upright with the couch's yaw; the reclined pose is not posed. The view is put
  on `SeatEye()` regardless.
- Europa's own breath loop and step sounds keep playing in the cabin while it is on or over Europa.
- A hero who turns into a bird or is dev-teleported out of a flying cabin: the flight is reset (craft back on Europa).
- The soundscape's `NearAttenuation` is used for every cabin sound and loop; the mix numbers are SOUND_NOTES', unheard.
- Subtitles use the Europa line's widget layout; the log's page stays on screen until E / a spoken line.
- `flight.records` gives no items; `place.flight_craft.name` / `place.flight_capsule.name` map markers are not placed.

## Per-frame cost

- Hero elsewhere (asleep): one `PlayerHero` lookup, two subsystem lookups, one `IsInside` test of the airlock. No
  actor of the feature ticks, nothing is loaded.
- Awake: `Advance` + `State()` (the model; `FState` carries a `std::string` record id, which allocates once a tick
  while a long-named record is on the platter: the one known per-frame allocation, besides the `FString` that
  `ActionAt` returns when the hero looks at a point), one cabin transform, `ApplyState`/`SetExterior`, up to 14 loop
  volume updates (set only on change), at most four sphere transforms and a handful of MID parameters (set only on
  change; sun and light every 0.5 s), one sea sample afloat, the dry box every 0.25 s. No traces per frame (four once,
  for the pads). Lights: two point lights without shadows on the cabin (plasma, panel), visible only when lit.
