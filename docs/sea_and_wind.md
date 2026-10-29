# Wind and sea

The wind is its own thing in the world (`ULyudWindSubsystem`), and the Baltic sea (`ALyudSea`) grows its waves
from it. Both follow the game calendar, so the same date and hour always have the same weather.

## Wind

`Source/Lyudmilovsk/Private/Weather/WindModel.cpp` (plain C++), wrapped by `Public/Weather/LyudWind.h`.

- **Weather**: direction and strength wander over a day or two around the autumn south-westerly (median 6.5 m/s,
  log-normal, so calm days and gales both happen). Seed 921 gives a breezy WSW afternoon (7.2 m/s from 249°) at the
  scene's start, 2026-10-20 16:00, and a near-calm night after it.
- **Day**: the afternoon air is stirred (stronger, gustier wind); at night it settles, and on a quiet night goes
  almost still. Day length comes from the date and latitude. A sea breeze blows onshore on calm sunny afternoons.
- **Gusts**: patches of stronger and weaker air (≈500 m, 130 m and 30 m across) drift downwind and change as they
  go; the wind veers a little in each.
- **Height**: log profile over water or land (`GetWindAt(Location, HeightAboveGroundM, bOverWater)`).

API (C++, Blueprint, Python `unreal.LyudWindSubsystem.get_wind(world)`): `GetWindAt`, `GetMeanWind`,
`GetMeanSpeed`, `GetFromDegrees`, `GetGustiness`, `GetDayStir`, `GetSeaStateWind` (the last 3 hours' mean the
waves have grown in), `DebugReport`.

Materials: `/Game/Weather/MPC_LyudWeather` gets `Wind` = (dir x, dir y, speed m/s, gustiness) and `WindDrift` =
(drift x, drift y m, gust clock s, day stir) every frame. `tools/sea/sea_hlsl.py` `GUST_LIB` computes the same gust
field in HLSL (`Lyud.Gust(xy_m, Wind, WindDrift)`), for trees and grass to sway with the real gusts later.

Console: `lyud.Wind.Speed`, `lyud.Wind.From`, `lyud.Wind.Gustiness` force a value (negative = the calendar's);
`lyud.Wind.Debug 1` prints it. Time = calendar + `lyud.SkyHourOffset`, like the sky.

Not yet wired: the player bird's air (`BirdAir.cpp`, `lyud.Bird.WindSpeed/WindFrom`), the plant and grass
materials' own wind, and NPC lines that read `lyud.Overcast` (weather beyond wind comes later).

## Clouds and rain

Now the weather engine: see `docs/weather.md` (`ULyudWeatherSubsystem`, `Private/Weather/WeatherModel.cpp`). The
wind's weather comes from its synoptic chain (`FWindParams::bSynoptic`); `GetSky()` keeps the old short summary.

## Sea

`Public/Sea/LyudSea.h`, `Private/Weather/WaveModel.cpp` (plain C++).

- **Waves**: 64 Gerstner trains of 0.9–100 m (geometry) and 48 of 5 cm–0.9 m (normals only), fixed wavelengths
  and directions all round the circle; the sea-state wind only sets their heights (fetch-limited JONSWAP, 60 km of
  open water, spread round the wind). A change of wind never jumps a phase. 7.5 m/s gives Hs 1.25 m with a 36 m
  peak; 14 m/s Hs 2.1 m; a calm night is a mirror.
- The material gets the same trains through a 128×2 float texture, so `SampleSurface(Location)` (height, normal,
  water velocity, depth, immersion) is exactly what is on screen. Time loops every 1200 s (every frequency is a
  whole number of turns per loop).
- **Look** (`/Game/Sea/M_LyudSea`, single-layer water): refraction, absorption and scattering of a sea-wave teal
  (aquamarine) water turning dark teal-green with depth, a few metres of visibility (`make_sea_materials.py`
  ABSORPTION / SCATTERING per metre, fed to the water output per centimetre, which is what the engine's shader takes).
  In the first `SHALLOW_PALE_M` (0.25 m) the water is nearly clear and without hue (pale over the sand), then its
  coefficients grow to the open sea's at `SHALLOW_CLEAR_M` (6 m); the sky shows in it by the water's own Fresnel, detail ripples scaled by the local gusts (cat's paws, glassy calms), whitecaps where
  crests stand high or pinch, broken into tufts (a few at 7 m/s, a white-flecked sea with grey old foam drawn out
  along the wind at gale force), breakers and wash over the shallows as thin torn lines of lace foam whose widths are held in metres (over a flat,
  a band of depth or wave phase is tens of metres wide).
- **Tide**: the actor stands at the datum (high water of springs); `SeaLevel()` and every depth follow the tide,
  down to 4.5 m lower at springs, and pools stay in the hollows of the bared flat ([tides.md](tides.md)).
- **Shallows**: a depth map read from the landscape at start (4 m cells, depth under the datum plus the pools the
  ebb leaves, a few seconds of traces). The surface stays
  under the breaking height (0.4 × depth), and where the water is shallower than ~1.5 × the wave height spilling
  breakers roll in along the depth contours, in the geometry, the foam and `SampleSurface` alike; no sea east of
  `LandSideX`.
- **Mesh**: rings round the camera built in code, ~26 k vertices (desktop) / 6.5 k (phone), fine near the eye and
  coarse to 30 km.
- **Swimming**: in play a water `APhysicsVolume` covers the sea (from `LandSideX` out, 30 m under sea level up to
  it; its brush is a box body built at runtime), so a character that falls in goes to `MOVE_Swimming` (checked
  offscreen: dropped in the shallows it swims, on the beach it walks). `IsSwimmable(Location)` asks the same of a
  point; `SampleSurface` gives the wavy surface to float at. While the player's body is in water deeper than
  `SwimMinDepth` (1.2 m) the sea holds them in its water itself (the volume's flat top would let a wave crest lift
  them out, drop, swim, pop out again) and keeps the camera `SwimEyeClearance` (22 cm) over the waves, whatever put
  them in the sea (a fall, the hand-over from a drowned bird); shallower they stand and walk out. Checked offscreen
  in the shallows, the open sea at 14 m/s and on the beach (`swim_*` steps of test_sea).
- **Under water**: a post process (`PP_LyudUnderwater`) fogs the view in the water's colour, dims it with depth,
  lays caustics on the bed and shows the sky through Snell's window overhead (dark by night, from the sun's altitude).
- **Buoyancy**: `ULyudBuoyancyComponent` on a simulating body (points, volume, drag). `lyud.Sea.SpawnFloat` drops
  a 1 m crate on the water to watch it ride.
- **Quality**: `lyud.Sea.Quality` 0 phone (64 segments, 20 + 16 trains), 1, 2 desktop (128, 64 + 48); −1 = by
  platform. `lyud.Sea 0` hides the sea (A/B), `lyud.Sea.Debug 1` prints the state, `lyud.Sea.DebugFoam 5` makes the
  foam glow.

### Seabed

`ULyudSeabedComponent` on the sea, within 40 m of the camera (15 m on phones), from a hash of 8 m tiles: eelgrass
meadows on sand 1.5–7.5 m deep, boulder fields with bladderwrack (shallow) or red Furcellaria (deeper) and blue
mussels at their feet, shells, flounders that dart off when the camera comes close, a school of herring wheeling
round, moon jellyfish drifting with the wave orbits. Meshes are made in code (`Private/Sea/SeaLifeMeshes.cpp`); the
weed sways with the dominant wave's orbital motion at its depth (`MPC_LyudWeather` `Sea`, `SeaWave`). `lyud.Seabed 0`
turns it off, `lyud.Seabed.Radius` (m) sets the reach.

## Placing it

`unreal/scripts/import_sky.py` (in `build_scene01.sh`) spawns an `ALyudSea` instead of the old flat plane. A level
that still has the plane gets the sea at play start anyway (`ULyudSeaSubsystem` hides the plane and spawns the sea
at its height; `lyud.Sea.ReplacePlane 0` keeps the plane).

Materials: `unreal/scripts/make_sea_materials.py` (headless) rebuilds `/Game/Sea/*` and keeps
`/Game/Weather/MPC_LyudWeather` (never re-created: that would cut every material reading it).

## Tests

- `tools/sea/run.sh test`: wind statistics over a year (median, prevailing direction, calmer nights, calms and
  gales), smoothness, gust field, wave heights per wind, the wrapped clock, `SurfaceAt` against the displaced
  particle, no folding crests, velocity, shallow water, wind turns keep phases. `run.sh days 2026 10 20 3` dumps
  the wind hour by hour.
- `unreal/scripts/test_sea.sh` (offscreen, no window): flies over the cliff, the beach, the shallows, the open sea
  in the calendar's wind, a gale and a calm dusk, and under the water to a boulder, a meadow and the school;
  prints the wind, the sea state, the seabed and a floating crate's ride, and screenshots each spot
  (`SEATEST_ONLY=a,b` for some).
