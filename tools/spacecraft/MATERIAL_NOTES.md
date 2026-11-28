# «Ласточка»: textures and materials

Everything here is made by scripts and checked without Unreal; **nothing has been compiled by Unreal yet**.

| What | Made by | Where |
|---|---|---|
| tiling surfaces, dial atlas, ball, digit strip, glass masks, noise | `tools/spacecraft/textures.py` (seed 7151) | `textures/spacecraft/T_SC_*.png` |
| the island from orbit | `tools/spacecraft/island_map.py` (seed 7152, reads the real map tools) | `T_SC_island.png`, `T_SC_island_M.png` |
| pins: seed, tile sizes, SHA-1 and GPU bytes of each output, the island's extent | both scripts | `tools/spacecraft/textures.json` |
| every material, the kit import, the fetched records | `unreal/scripts/import_spacecraft.py` (+ `.sh`) | `/Game/Spacecraft/{Textures,Materials,Meshes,Localized/<lang>}` |
| numpy pictures of the shaders (the same HLSL text, executed) | `tools/spacecraft/material_preview.py` | `design/spacecraft/preview/*.png` |

```
python3 tools/spacecraft/textures.py [name ...] [--check]     # --check: fail if an output's SHA-1 moved
python3 tools/spacecraft/island_map.py [--size 2048]
python3 unreal/scripts/import_spacecraft.py                   # outside Unreal: lint + run every node once
python3 tools/spacecraft/material_preview.py [planet plasma crt panel glass record]
sh unreal/scripts/import_spacecraft.sh                        # on the Mac; IMPORT_SPACECRAFT_ONLY=materials skips the kit and audio
```

The importer logs `SPACECRAFT ...`. It ends with the list of slots it left with the glb's own flat materials: that
list should be exactly `SC_blanket SC_book SC_cap SC_case SC_cord SC_dark SC_lamp SC_paper SC_red SC_yellow`.

## Textures: 26 of mine, 11.52 MB on the GPU

Compressed with mips, as `textures.json` counts them. With the other threads' atlases that are resident at once (one
language of `T_SC_labels` 1.33 MB as G8, one of `T_SC_sleeves` 1.33 MB, `T_SC_record_labels` 0.67 MB) the craft is about
**14.9 MB**, under the 16 MB of the design. The planet and cloud maps are Europa's (`T_EU_planet_C`, `T_EU_clouds`), not
counted twice.

| Texture | Size | Channels | Notes |
|---|---|---|---|
| `T_SC_<s>_C` / `_N` for `paint_white paint_green metal foil shield rubber fabric chute` | 512 | colour + roughness in A / normal (DirectX green) | seamless; one tile = `tile_m` metres (kit.json `tiles`) |
| `T_SC_quilt_C` / `_N` | 1024 | same | 2 x 2 of the kit's 28.96 cm pillows, seams at 0 and 0.5 |
| `T_SC_net_C` | 512 | colour, A = cut-out | the stowage net, no normal map |
| `T_SC_dials` | 1024 | colour | faces drawn from kit.json `panel.dials` (rect, min/max, sweep, major/minor); digits only, no words |
| `T_SC_ball` | 512 x 256 | colour | equirectangular attitude ball |
| `T_SC_digits` | 512 x 64 | grey | a seven-segment strip 0..9; **not used** by `M_SC_Digits` (which draws its segments), kept as the stand-in atlas and for anyone who wants a textured readout |
| `T_SC_glass` | 512 | R frost arrival, G scorch arrival, B droplets, A rivulets (tiles in V) | masks, linear |
| `T_SC_noise` | 256 | four equalised noises | masks, linear; the planet's fine layers and the plasma |
| `T_SC_island` | 1024 | colour, A = blend over the sea | 25.6 km across, north up, 25 m a texel |
| `T_SC_island_M` | 1024 | R night lights, G land, B shallows | masks, linear |

`textures.json` → `island`: `extent_km` 25.6, `centre_map_m` (-800, 7000), `town_uv`, `club_uv`, `town_lat_lon` (54.96,
20.40, the planet texture's own island), `centre_lat_lon_true_scale`. The real part of the map (X -1300..1300, Y
-900..4200) is drawn from `tools/world_map.py`, `city_plan`, `north`, `railway`; the rest of the island is invented
round it (seeded).

Words: none of my textures carries a word. Captions come from `T_SC_labels_<lang>` (labels.py); the importer puts
`T_SC_x_<lang>.png` at `/Game/Spacecraft/Localized/<lang>/T_SC_x`.

## The shader subset

Every custom node is branch-free HLSL: declarations with a value, assignments, `[unroll] for` with constant bounds, one
`return` last; no `if`, `?:` or comparisons. The lint enforces it (plus balanced brackets, every input used, every name
declared, no bare integer literals) and `material_preview.py` runs the same text in numpy. The lint also compares the
constants that mirror other files: tile sizes and metalness (`textures.json`), the planet texture's island, kit.json
(every slot of every piece has a material or is a known flat one; digit windows, cell aspect, lamp grid and each
lamp's data UV, record radii, CRT aspect, gore counts) and `FlightModel.h`'s `CrtPath` constants.

## Slots → materials

By the slot's exact name (kit.json `"materials": {"slot" or "Mesh:slot": "material"}` overrides).

| Slot | Material | Slot | Material |
|---|---|---|---|
| `tx_sc_<s>` | `MI_SC_<s>` (of `M_SC_Surface`; `net` of `M_SC_Net`) | `SC_porthole` | `M_SC_Glass` |
| `SC_dials` | `M_SC_Dials` | `SC_glass` | `MI_SC_GlassCover` (of `M_SC_Glass`, `Reflect` 1.6) |
| `SC_needle` | `M_SC_Needle` | `SC_fire` | `M_SC_Plasma` |
| `SC_digits` | `M_SC_Digits` | `SC_globe` / `SC_ball` | `M_SC_Globe` / `M_SC_Ball` |
| `SC_bar` | `M_SC_Bar` | `SC_record` / `SC_record_label` | `M_SC_Record` / `M_SC_RecordLabel` |
| `SC_crt` | `M_SC_CRT` | `SC_sleeve` | `M_SC_Sleeve` |
| `SC_lamps` | `M_SC_Lamps` | `SC_chute` | `MI_SC_chute`; on `SM_SC_Drogue` `MI_SC_chute_drogue` |
| `SC_labels` | `MI_SC_Labels_ru` (`_en`, `_ja` exist too) | flat `SC_*` | the glb's own, metalness zeroed |

`M_SC_PlanetNear` and `M_SC_PlanetAir` are on no kit mesh: the director puts them on its sky rig's spheres.

## Parameters

"Who": **dir** = the director (`LyudSpaceflight.cpp`, the sky rig), **cab** = the cabin (`ALyudSpacecraft`, from
`ApplyState` / `SetExterior` / `SetRecord`), **imp** = set once by the importer, leave alone. Set MID parameters only
when the value changes. Colours are linear.

### `M_SC_PlanetNear` (unlit, masked, two-sided; on the planet's sphere)

Draws an exact analytic sphere inside the mesh (0.9985 of the mesh's bounding radius: made for the Europa kit's 128 x 64
planet sphere; any sphere whose pivot is its centre works). The mesh must be imported from a glb like the Europa kit
(the shader undoes the import's Y mirror): in the mesh's UE-local axes the point (lat, lon) of the map is
`(cos lat cos lon, -cos lat sin lon, sin lat)`, north = +Z. Works from any distance, from far to inside the air.

| Parameter | Unit, range | Who | What |
|---|---|---|---|
| `SunDir` | world unit vector toward the sun | dir | by the hour over the planet (MODEL_NOTES "The planet impostor") |
| `SunLight` | colour x illuminance / π, as `ALyudEuropaSite` gives `M_EU_Planet` | dir | |
| `Gain` | exposure factor, 2.2 x the site's `Dim` | dir | |
| `IslandLat`, `IslandLon` | degrees on the planet map; default 54.96, 20.40 (the map's own island) | dir | where the real island's centre is laid. The map's own nine-degree blot is always painted over with sea |
| `IslandSizeKm` | km across the island texture; true = 25.6 | dir | far away the true island is under a pixel: ease from about 1000 (leaving Europa) down to 25.6 by the time the planet fills the porthole, so that its apparent size only ever grows |
| `PlanetRadiusKm` | km, default 6371 | dir | must be the radius the model flies round |
| `CloudShift` | turns of longitude (0..1) | dir | the cover's drift; the same value `M_EU_Planet` had at launch, then creeping |
| `Detail` | 0..1 | dir | master of the fine layers (billows, wind streaks, glitter); they also fade in by themselves with screen size. 0 on low quality |
| `Rough` | 0.2..0.6 | dir | the sea's roughness: width of the sun's glint |
| `Glint` | 0..2 | dir | strength of the glint |
| `Haze` | 0..1.5 | dir | the air over the day side; fade to about 0.3 in the last kilometres before the whiteout |
| `Lights` | emissive scale, default 20 | dir | the town's lights on the night side (`T_SC_island_M.r`) |
| `Albedo`, `Clouds`, `Island`, `IslandM`, `Noise` | textures | imp | |

Orientation: the model wants the island along `PlanetIslandAxis` and the pole along `PlanetNorthAxis`, 72° apart
(island at latitude 18). The map's island is at 54.96. Either set `IslandLat` 18 (the island then jumps when the view
changes from `M_EU_Planet` to this material), or, better, keep 54.96 and put the pole 35.04° from the island axis in
the plane of the model's two axes: the heading over the island stays the model's, and nothing jumps.

### `M_SC_PlanetAir` (unlit, additive, two-sided; a sphere `Air.x` times the planet's, same centre)

Six steps of single scattering; draws only the limb and the twilight ring beyond the disc (the disc's own haze is in
`M_SC_PlanetNear`). `M_EU_PlanetAir` is not reused: it is one-sided, so it vanishes once the camera is inside the
shell, and it would double the haze over the disc.

| Parameter | Unit | Who | What |
|---|---|---|---|
| `SunDir`, `SunLight`, `Gain` | as above | dir | |
| `Air` | x = shell radius / planet mesh radius (1.045), y = strength (1) | dir | x must equal the shell mesh's real scale |

### `M_SC_Plasma` (unlit, additive, one-sided; `SM_SC_FireShell` has both faces)

| Parameter | Unit | Who | What |
|---|---|---|---|
| `Plasma` | 0..1 (squared inside) | cab (`E.Plasma`) | strength; hide the mesh under 0.002 |
| `Heat` | 0..1 | cab (`E.Heat`) | colour: deep red, orange, white-pink, a violet fringe over 0.55 |
| `Flow` | speed factor, 1 | cab | 0.6 early, 1.3 at the peak if wanted |
| `Gain` | emissive scale, default 40 | cab | tune against the exposure once, in the engine |
| `Noise` | texture | imp | |

### `M_SC_Glass` (unlit, translucent, one-sided; panes `SC_porthole`, covers `SC_glass` through `MI_SC_GlassCover`)

| Parameter | Unit | Who | What |
|---|---|---|---|
| `Frost` | 0..1 | cab (`E.Frost`) | crystals growing in from the rim |
| `Scorch` | 0..1 | cab (`E.Scorch`) | soot browning in from the rim |
| `Wet` | 0..1 | cab (`E.Wet`) | droplets, then running rivulets |
| `Fog` | 0..1 | cab (`E.Fog`) | the whiteout; 1 = opaque `FogColor` |
| `FogColor` | emissive colour, default (6, 6.2, 6.5) | cab | the cloud's brightness at the current exposure |
| `Light` | colour x intensity of the cabin's light on the glass | cab | (1.2, 1.0, 0.8) lit; near 0 with the lamps off |
| `Reflect` | 0..2 | imp / cab | the faint reflection of the cabin |
| `Mask` | texture | imp | |

### Instruments

| Material | Parameter | Unit | Who | What |
|---|---|---|---|---|
| `M_SC_Dials` (lit) | `Backlight` | 0..1 | cab | the print glowing (power on, panel light) |
| | `BackColor`, `Gain`, `Roughness`, `Atlas` | | imp | |
| `M_SC_Needle` (lit) | `Backlight` | 0..1 | cab | |
| | `Color`, `Gain`, `Roughness` | | imp | |
| `M_SC_Ball` (lit) | `Backlight` | 0..1 | cab | |
| `M_SC_Labels` (lit) | `Backlight` | 0..1 | cab | the captions glowing |
| | `Atlas` | texture | cab | swap to `LyudText`'s `T_SC_labels` of the language, or use `MI_SC_Labels_<lang>` |
| | `Plate`, `Ink`, `GlowColor`, `Gain`, `Roughness` | | imp | |
| `M_SC_Digits` (unlit) | `Value0`, `Value1`, `Value2` | whole numbers | cab | clock = HHMMSS as one number (`ClockDigits`), range km, speed |
| | `Win0..2` | (first cell, digits, colon after every n-th place, point after place n) | imp / cab | defaults (0,6,2,0), (6,6,0,0), (12,5,0,0). For km/s with two decimals send the speed x 100 and set `Win2.w` = 2; back to 0 in m/s |
| | `Opt0..2` | (keep leading zeros, blank, colon steady, -) | cab | blank = 1 without power; the clock keeps its zeros |
| | `Count`, `Aspect`, `Ghost`, `Color`, `Gain` | | imp | 17 cells; the unlit segments' ghost |
| `M_SC_Bar` (unlit) | `Level` | 0..1 | cab (`Hull01`) | fills from the bottom; blinks over `Hot` |
| | `Segments`, `Warn`, `Hot`, `Flip`, `Gain` | count, shares of the height | imp | 20, 0.6, 0.85 |
| `M_SC_Lamps` (unlit) | `LampsA`..`LampsF` | each component = mode + 4 x class | cab | lamp n (row x 6 + col) is component n % 4 of vector n / 4. Mode 0 off, 1 on, 2 slow blink, 3 fast blink (the model already blinks `bLit`: send 0 / 1). Class 0 green, 1 amber, 2 red, 3 white |
| | `Atlas` | texture | cab | the language's `T_SC_labels` |
| | `Gain` | | imp | |
| `M_SC_CRT` (unlit) | `Mode` | 0, 1, 2 | cab (`CrtMode`) | trajectory, horizon, load |
| | `PosEuropa`, `PosPlanet` | (x, y, radius) in the model's square, y up | cab (`CrtEuropa*`, `CrtPlanet*`) | defaults are the model's |
| | `Craft01` | 0..1 | cab (`CrtProgress`) | the dot and the solid part of the path; the path itself is `CrtPath`'s formula in the shader (no points to send) |
| | `Phase` | 0..8, -1 none | cab (`CrtPhase` as a number) | nine pips along the top |
| | `Att` | x roll radians, y pitch as a share of the screen | cab | page 1 |
| | `GLoad` | x now, y peak, in g (0..6) | cab | page 2 |
| | `CapRect` | (u0, v0, u1, v1) in the label atlas, 0 = none | cab | a caption along the bottom |
| | `Power` | 0..1 | cab | 0 = dark tube |
| | `Labels` | texture | cab | the language's `T_SC_labels` |
| | `Aspect`, `Gain` | | imp | |
| `M_SC_Globe` (lit) | `CraftLat`, `CraftLon`, `Cross` | degrees, 0/1 | cab (`GlobeLatDeg`, island's + `GlobeLonRelDeg`) | the cross-hair; or leave `Cross` 0 and turn the mesh under a fixed reticle |
| | `IslandLat`, `IslandLon`, `Mark` | degrees, 0/1 | imp | the red point |
| | `Tint`, `Roughness`, `Map` | | imp | the planet's own map |

Lamps as six vectors and not a 6 x 4 dynamic texture: six `SetVectorParameterValue` calls only when a lamp changes, no
texture update, no render-thread work, and no extra sample in the shader.

### The record player and the canopy

| Material | Parameter | Unit | Who | What |
|---|---|---|---|---|
| `M_SC_Record` (lit) | `LightDir` | world unit vector toward the cabin lamp, default up | cab | where the sheen's wedges point (with the eye). No UVs and no spin parameter: turn the mesh |
| | `RecordR`, `LabelR`, `Sheen` | cm, cm, colour | imp | 12.5, 4.5 |
| `M_SC_RecordLabel` (lit) | `Cell` | 0..7 | cab (`SetRecord`) | cell of `T_SC_record_labels` (4 x 2) |
| | `Paper`, `HoleR`, `Roughness`, `Atlas` | | imp | |
| `M_SC_Sleeve` (lit) | `Cell` | 0..7 | cab | one MID per sleeve in the rack |
| | `Atlas` | texture | cab | the language's `T_SC_sleeves` |
| | `Tint`, `Wear`, `Roughness` | | imp | |
| `M_SC_Chute` (two-sided foliage) | `Through` | 0..1 | imp | sunlight through the cloth |
| | `TileU`, `TileV` | tiles | imp | gores / 2 (12 main, 4 drogue); 3 and 1 |
| `M_SC_Surface` | `TileInv`, `Metallic`, `Tint`, `BaseTex`, `NormalTex` | 1 / m | imp | the `MI_SC_<s>` instances |
| `M_SC_Net` (masked, two-sided) | `TileInv`, `Tint`, `Roughness`, `BaseTex` | | imp | |

## For the engine (the perf checklist)

- Planet sphere, air shell, `SM_SC_FireShell`, both canopies, the panes, the needles, globe, ball, record:
  `CastShadow = false`, `bAffectDistanceFieldLighting = false`, `bAffectDynamicIndirectLighting = false`, movable. The
  sky rig's two spheres also `bNeverDistanceCull` and bounds scaled so they are never frustum-culled at the wrong time.
- The unlit instrument materials light nothing by themselves: the panel's glow in the cabin is the director's one
  small light.
- Nothing here rebuilds a texture at run time. If quality scaling is wanted, the natural knob is `Detail` (and hiding
  the air shell); a console variable for it needs `ECVF_Scalability`.
- Records fetched by `fetch_records.py` import as `/Game/Audio/Music/MU_SC_<id without record_sc_>`, streamed; none
  are fetched in this checkout.

## Known limits

- The phase's word on the CRT: the label atlas has no phase plates and there is no vector font here, so the screen
  shows nine pips (`Phase`). A word needs `flight.phase.*` plates added to `labels.py`, then `CapRect`.
- The colon and the point are drawn at the right edge of a digit's own cell (the kit's cells do not cover the gaps).
- `M_SC_Glass` is unlit: its reflection is a painted blot scaled by `Light`, not the scene.
- The Europa ice normal map (`textures/europa`) is OpenGL-style, mine are DirectX-style; not touched here.
