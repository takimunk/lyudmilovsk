# The hatch to Europa

An unknown round hatch lies on the sea bed some 440 m off the beach under the club. Under it a shaft goes down into a
small airlock buried in the bed; a second ladder in the airlock climbs a shaft that comes out on **Europa**: ice to the
horizon, a black sky with the red star, and overhead the planet, which is only ocean, clouds and one small island,
Lyudmilovsk. The hero says so (`line.hero.europa_planet.1`) and the music starts. The world is a simulation: the fourth
wall shows.

## Where

| | |
|---|---|
| Hatch | map (573, 100) m, UE (57300, 10790, ≈ -5193) cm: lid seat 0.35 m over the bed |
| Depth | ~9.3 m of water at high tide, ~4.8 m at spring low water (more on the underwater world's deeper bed) |
| Map | marker `europa_hatch` ("Люк на дне" / "The hatch on the sea bed" / "海底のハッチ"), known from the start |
| Europa | UE (0, -8 000 000, 500 000) cm: 80 km south and 5 km up, off the sea mesh (30 km), inside the sky dome (200 km) |

The spot is kept clear for the hatch by the underwater world (plain sand, no reefs within 30 m); it is 160 m north of the
sea railway's bank and clear of the tidal channels. `tools/europa/europa_site.py` resolves it against the ground
(`terrain.ground_z` round the collar) and writes `Content/Europa/europa.json` (hatch, Europa's origin, the planet, the
gravity, the radii, and `terrain_holes`: a 2 x 2 m hole in the landscape under the shaft, read by `tools/terrain.py`
`europa_holes()` when Lvl_Scene01 is built). At play the bed is traced again round the collar, so a changed sea bed moves
the hatch with it.

## The flow

1. **Swim** to the hatch (`ULyudSwimComponent`: dive with C/Ctrl, W along the look). `[E] Open the hatch`: the lid swings
   open (only with the lock full: the sea on both sides of it; else "the hatch won't budge").
2. `[E] Climb down into the hatch`: a scripted climb (the sea and the swimming let go of the body meanwhile), the lid
   shuts over the head, down the ladder into the full lock; there the swimming takes over again, under water.
3. The **computer** (`[E] Airlock: press the button`): drains the lock (14 s; the water's surface goes down, the pump,
   the screen's gauge and lamp). Drained, the room is dry ground (`ALyudSea::SetDryZone`).
4. `[E] Climb the other ladder`: the inner lid swings down, up the shaft; halfway up the climb jumps to the same place in
   the identical shaft under Europa, its lid opens, and he steps out on the ice.
5. **Europa**: the sky's space view (`ALyudSky::SetSpaceView`: no air, clouds, fog, moon disk or moonlight; the star and
   the stars stay, the sky light down to 0.03), the soundscape's vacuum (`SetVacuum`: no bed, far sounds, steps or
   music), rain effects off; 0.13 g (a jump goes ~7 m up), air control up; his own breathing and steps only, through the
   body; a soft fence 3.5 km out (the ice ends at 4 km). The first look at the planet (within 24 degrees of it, from the
   ice) says the line, shows its subtitle and starts `MU_europa_1`; saved (`ALyudAirlock::bPlanetSeen`).
6. Back: `[E] Open the hatch` (only with the lock dry), `[E] Climb back down`, the jump back halfway, down into the room.
   The computer fills the lock; `[E] Climb up to the sea`: the lid opens near the top, out onto the bed, and the
   swimming takes over under water (`ULyudSwimComponent::EnterUnder`).

## Code

`Public/Europa/LyudEuropa.h`, `Private/Europa/LyudEuropa.cpp`:

- `ALyudAirlock` (named `EuropaAirlock`, saved: the water's state and whether the planet has been seen): collar, sea lid,
  room, inner lid, console (its screen a dynamic `M_EU_Screen`: `Level`, `Mode`), the water's disc, lamps, an underwater
  tint while the water goes down or up. Full, the room belongs to the sea (swimming, its underwater look); otherwise it is
  a dry zone.
- `ALyudEuropaSite`: the ground (below), the ice-crusted collar, its lid, the shaft and its ladder, the planet with its air,
  the star field, a faint blue planetshine (lighting channel 2, Europa's meshes only; as bright as the planet's lit share
  seen from here). Hidden unless the player is there.
- `ULyudEuropaSubsystem`: spawns both at play start (worlds with `ALyudSea` and `ALyudSky`; `lyud.Europa 0` leaves them
  out), polls E (never consumed) and sets the prompt, runs the climbs, switches Europa on and off by distance (a load on
  Europa comes back right), the line and the music.

Hooks in other systems (small on purpose): `ALyudSea::SetDryZone` / `IsInDryZone` / `SetPlayerScripted`;
`ULyudSwimComponent::EnterUnder` and its dry-zone check in `UpdateWater`; `ALyudSky::SetSpaceView`;
`ULyudSoundscapeSubsystem::SetVacuum`.

Console: `lyud.Europa.GoHatch`, `lyud.Europa.GoRoom`, `lyud.Europa.GoEuropa`, `lyud.Europa.Use` (= E),
`lyud.Europa.Report`, `lyud.Europa.Fast 1` (climbs, lids and water five times faster).

## The ground

`tools/europa/moon_terrain.py` bakes it from `tools/europa/moon_terrain.json` (seed 2207 and every number; the same file
gives the same ground: `Content/Europa/moon.json` pins the heights' SHA-1): a domain-warped ridged multifractal plain
with ridged hummocks, ~80 crossing double ridges (younger over older) and cycloid chains, one 72 m double ridge with
terraced flanks across the planet's side of the sky, three stepped fault scarps, a chaos field of tilted, drifted rafts
of the old plain in a brown hummocky matrix (and a giant one beyond the walk that makes the skyline with far ridged
massifs), two open leads (ice cliffs down to water), talus from thermal erosion on a 4 m grid kept mostly as fill, Europa's
curve, ~1000 faceted boulders. Meshes: nested square levels of tiles (0.5 m spacing within 256 m, 2 m to 1 km, 8 m to the
4.15 km disc, edges snapped to the coarser level, skirts) and a ring to 16 km; vertex colours carry the brown salts, fresh
ice, frost and cavity for `M_EU_Terrain` (triplanar ice, strata in the cliffs: no UV seams). `unreal/scripts/import_moon.sh`
imports them into `/Game/Europa/Moon` (Nanite; complex collision from the fallback mesh: 35 % of the near tiles'
triangles, 50 % further; none on the skyline). The glb itself is not committed (rebuilt from the json in ~2 min).

The leads' water is `M_EU_LeadWater` (translucent): the sea's own absorption per metre (clear over the bed at the
shore, black-teal where deep), Fresnel reflections of this sky (the planet's disk and glow, the star's glitter), slow
ripples for 0.13 g, a little scattered light, and a rim of new ice where it thins against the shore; `ALyudEuropaSite`
feeds it the planet's and the star's directions and light. (The sea's single-layer `M_LyudSea` drew grey foam over a
whole lead away from its depth map; it stays the fallback.) Its surface holds you up (no swimming on Europa).

## The sky from Europa

Europa keeps one face to the planet. The hatch stands at latitude 38, longitude -41 of its body frame (`moon.json`
"site"; longitude 0 faces the planet), so the planet hangs still, 36 degrees up in the south-east. `ALyudEuropaSite::SkyAt`
puts Europa where the ephemeris has the moon (`LyudEphemeris::MoonEquatorial`, the same as the coast's sky), its pole
along its orbit's, and from there gives the sun's and the planet's directions, the planet's turn (its texture's
longitudes are the Earth's, turned by sidereal time) and the stars' (the equator of the date). `ALyudSky::SetSpaceObserver`
takes that sky in the space view: the star's disk, its light and shadows (it sets with its disk, no twilight), the day
and the exposure follow Europa's own sky, which wheels round once a month. The island (`T_EU_planet_C`, at the town's
real latitude and longitude, with its night lights in `T_EU_clouds` G) faces Europa only while Europa stands over the
town's horizon; otherwise the planet shows ocean and clouds, and the hero's line waits for a look at the island.

No air: no atmosphere, clouds (`ULyudCloudSubsystem` keeps them off, and the dome's high clouds), fog, grade or the dome's
own stars. The stars are `M_EU_Space` on a 100 km sphere round the site (three procedural layers, the Milky Way band
and bulge with dark lanes, each star twinkling subtly at its own beat, none over the star's disk). The planet's air is
`M_EU_PlanetAir` on a shell 1.045 of its radius: single scattering, a blue glowing limb, a twilight ring past the
terminator, nothing in its shadow.

## Assets

- Meshes: `blender/europa/europa_kit.py` (Blender 5.3, clean primitives, booleans for the ceiling's holes, metre UVs,
  `blender/railway/kit_lint.py` on every piece: 0 findings) -> `design/europa/game/europa_kit.glb` +
  `Content/Europa/kit.json` (the measures the game reads). The clear opening of every hatch and shaft is 1.20 m (over the
  character standard's 1.08); the room is 4 m across and 2.6 m clear.
- Textures: `tools/europa/europa_textures.py` (procedural, seed 4417) -> `textures/europa/` (painted steel, steel,
  concrete, algae, ice + normal, the planet and its clouds: ocean all round, one island at lon 0, lat 18).
- `unreal/scripts/import_europa.sh`: meshes, textures and materials into `/Game/Europa`, then the sounds and the voice.
- Sounds (`audio/sounds.json`, group `europa`, ElevenLabs through fal): `hatch_open`, `hatch_close`, `airlock_drain`
  and `airlock_fill` (loops), `airlock_beep`, `ladder_step`, `europa_breath` (loop), `europa_step`. Music:
  `europa_1` (Stable Audio 2.5, seed 30101). Voice: `line.hero.europa_planet.1` (ru/en/ja, the hero's MiniMax voice,
  `localization/voice.json`).

## Test

`unreal/scripts/test_europa.sh` (offscreen Lvl_Scene01, `test_europa.py`): the whole way there and back with
`lyud.Europa.Use`, checks (spawned, each E's action, in the room, drained, on Europa, 0.13 g, space view, planet seen,
music, jump height, filled, back in the sea swimming) and screenshots into `Saved/EuropaTest`. The level must be built
with the hatch's landscape hole (`unreal/scripts/build_scene01.sh`, or only the ground of a copied level).
`EUROPATEST_ONLY=europa` (the planet only) or `EUROPATEST_ONLY=hatch` (the approach only) run a short part.

## Cost

Nothing per frame while the player is away: the site is hidden and does not tick, the airlock's tick returns at once
when nothing moves, the prompt and the water's disc are set only when they change, every moving part (lids, water,
planet) is out of the distance field and the dynamic GI. Memory: the ice disc (144k triangles), the planet (16k) and
eight textures, ~30 MB. Benchmark (Epic, one route, 2026-10-31, a busy machine: ~100 ms frames instead of ~24, so only
"no change beyond noise" can be read): lyud.Europa 0: 108.7 ms median, 21 320 MB; on: 105.3 ms, 21 180 MB
(bench/results/Mac17-7-AppleM5Max/*_europa_off/on.json).
