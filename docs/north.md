# The North (Север)

Volcano country beyond the north hills: a long walk from the town (about 2.2 km from the Sun Square to the campsite).
A chain of cinder cones, one of them steaming, a lava field running to the sea, summer snowfields in the cones' shade,
grass meadows, a campsite with tents and a ranger's hut at the reserve's gate. Few trees: stone birch park woodland at the
tree line and on the Old Cone, then dwarf pine, alder thickets and shrubs.
Everything here is a **proposal**, not canon (docs/story/README.md): names, the ranger, the boards' words.

**The land is a real place.** Its heights are the cinder cones of the Northern Breakthrough of the Great Tolbachik
Fissure Eruption (Kamchatka, 1975; 55.68 N 160.24 E) and the lava plain west of them, from NASA SRTM (public domain;
fetched as Mapzen / AWS Terrain Tiles), at **half size**: 2 real metres per map metre, heights halved too, so every slope
is the real one. `tools/north_dem/make.py` fetches the tiles, mends the radar's voids, takes the map's window, removes
most of the flank's regional tilt (12 % inland in reality, 2 % kept) and writes `tools/north_dem/dem.i16` + `dem.json`
(10 m cells; committed, so nobody needs the network). `tools/north_land.py` reads them (standard library only) and derives
the rest from the heights instead of drawing it by hand:

- the cones' **cinder** = whatever stands high over the land 300 m around (`relief`),
- the **lava field** = rough ground (`rough`): the real flows' levees and channels,
- the **snowfields** = shaded hollows and lee north slopes above 62 m (`snow`), a fixed share of that land,
- nothing is an ellipse or a sine.

The map grew from Y 2200 to Y 4200 for it (`tools/world_map.py` `Y_MAX`; the landscape is 21 x 41 components) and the
landscape's Z scale is 100 (`LyudTerrain.cpp`, `terrain.LANDSCAPE_Z_STEP`): heights to +-256 m, the High Cone is 173 m.

## Places

Map metres (docs/world_map.md's frame: X west toward the sea, Y north). Chosen on the real land in `tools/north.py`.

| Place | Where (X, Y) | What |
|---|---|---|
| Ridge and its wood | Y 1450..1850 | the hills between the town and the river: old pine and spruce forest in broad masses with glades; the cable car's Polar station (-360, 1660) on its crest, where the trail starts |
| River (река) | Y ~1900..2160, from X -1185 to the sea | a braided gravel river (the Avacha above its mouth): a plain 50..370 m wide of bars, channels and alder islands in a valley 50 m deep, a steep undercut bluff on the south, terraces on the north; born as seeps under the Topi's dune, reaches the sea through a spit |
| Footbridges | X -150 | the trail's way across the plain: three log footbridges over the deeper channels, the braids are waded |
| Gate | 100, 2445 | a barrier across the track on the valley's north brow, the reserve's board |
| Ranger's hut (сторожка) | 130, 2500 | a small log hut with a stove, a bunk, a table and a radio; the ranger lives here; the Old Cone rises behind it |
| Birch groves | Y 2160..2700 | stone birch groves over grass on the valley's sunny side, rarer toward the meadows |
| Old Cone (Старая сопка) | -385, 2720 | the southernmost cone, 115 m, grassed over, stone birches on it: the landmark over the hut, a lookout |
| Meadows (луга) | west of the cones | the old lava plain under grass, 20..50 m up, low terraces and swales; dwarf pine and alder; ends at the sea in a cliff |
| Campsite (кемпинг) | 140, 3100 | six tents close round a fire ring, a table under a tarp, log benches, a food pole, an outhouse; under a low terrace |
| High Cone (Высокая сопка) | -490, 3125 | the tallest, 173 m: a closed crater 150 m across with snow in it; a path gains its rim from the saddle |
| Saddle | -455, 3345 | between the High Cone and the Cone |
| Hot spring | -296, 3502 | a warm pool with a wooden deck at the mouth of the Fumarole Valley |
| Fumarole Valley | -440, 3503 | the trough running out of the Cone's breach: bleached clay, steam vents, steaming ground, mud pots |
| The Cone (Сопка) | -560, 3535 | the live one, 150 m: a horseshoe crater open to the south-west, vents on its floor |
| Lava field | Y 3440..3830, from X -400 to the sea | blocky clinker in tongues with grass and shrub islands between them |
| Snowfields (снежники) | `north.snowfields()` | firn in the shade of the High Cone and the Cone (north.json lists them, largest first) |
| Small Cone (Малая сопка) | -650, 3900 | the northernmost, bare cinder |
| Eastern cones | X < -850 | two low grassed cones nobody needs to visit |
| Far volcanoes | 5..10 km out | three backdrop cones with snow caps, one smoking (`north.far_volcanoes()`), and the far land north of the map; never walked on |

The trail (a dirt track, `north.TRACKS`, routed by `tools/terrain.py`): the town's north track -> the gate -> the meadows
-> the camp -> the spring -> up the Fumarole Valley into the Cone's crater; from the spring also up to the saddle and the
High Cone's rim, and out onto the lava field.

East of the map lie the Topi (their thread's own ground, `tools/topi.py`): keep things that must be seen west of X -1130.

## The river, the woods, the coast (from the town's edge, Y 1450)

Everything north of Y 1450 is the North's (the east and south-east south of it are `tools/east.py`'s, the Topi thread).

- **Hills**: `north.hills(x, y)` is the height `world_map.plateau_south` raises the valley to; `north.plateau` blends
  it into the real volcano land (BLEND) and cuts the river's valley (`north_river.apply`).
- **River**: `tools/north_river.py`. A middle line (COURSE, running west all the way: a point belongs to the line's
  point of its own X), the gravel plain's half width and floor along it, two named streams (THREADS) and the braids
  (zero lines of a noise drawn out along the flow). `gravel`/`wet` feed TerrainGen's `gravel` mask (stone and sand
  mixed, silt on the beds; no layer of its own), `protect` keeps the plain's exact shape, `outlet` breaks the beach's
  berm, a tide channel continues it (world_map.TIDE_CHANNELS). It must not meet the Topi's river Ural: it starts as
  seeps west of X -1190.
- **For the game** (`north.json`): `river` {water [[x, y, z, half]] downstream, threads, mouth}; `things` (the
  footbridges: kind `log_bridge`, variant 0/1/2 = spans to 11/16/26 m, `z` = the deck's map height, yaw 0 = the deck, the mesh's X,
  along map Y); `spring` {x, y, r, seats [{x, y, yaw}], surface_z}: the pool is a basin in the ground 1.05 m deep.
- **Woods**: `north.wood(x, y)` -> (mix, cover) draws them between Y 1450 and 2700; `world_map.forest_cover(x, y)`
  dispatches (east.py's part, the North's, 1 elsewhere); `tools/forest_cover.py` writes
  `Content/World/forest_cover.fcv` (import_world.py runs it; not in git) and AProceduralForest drops the trees the
  mask leaves out. The tiles there (`north.forests`) only say which mix grows where. The forest-floor layer follows
  the same cover (`terrain._north_floor`).
- **Coast**: `north.shore_shift(y)` moves world_map's water line north of Y 1500: coves and points, a bay at the
  river's mouth between two headlands, the lava field's cape.
- A look without Unreal: `bash tools/north_sim/run.sh ground -1000` (the whole map's ground).

## Who owns what

| What | Where |
|---|---|
| The real heights and what follows from them (cinder, lava, snow) | `tools/north_dem/` (make.py, dem.i16, dem.json), `tools/north_land.py` |
| The region: places, masks, camp layout, fumarole list, signs, forests, grass, trail anchors | `tools/north.py` (hooked from `world_map.plateau / keep_flat / forests / grass_patches / _tree_free`, `terrain._anchors / TRACKS`) |
| Data for the game | `python3 tools/north.py` -> `Content/North/north.json`, `Content/Dev/teleports/north.json` |
| Ground: lava's clinker relief, firn in the snowfields, the Snow / Scoria / Sulfur layers | `Private/Terrain/TerrainGen.cpp` on the masks `snow, scoria, lava, sulfur` that `terrain.write_inputs` appends |
| A look without Unreal | `tools/north_sim/run.sh ground` (-> `Saved/North/north_ground.png`), `python3 tools/north_preview.py out.png` |
| Props (tents, camp, boards, the spring's deck) | `blender/north/` -> `blender/export/north/*.glb` + `manifest.json` |
| The hut | `blender/north/hut*.py` -> `blender/export/north/hut*.glb` |
| In Unreal: import and placing | `unreal/scripts/import_north.py` (`import`, `place(face_y)` called from `import_world.py`) |
| Fumaroles, the campfire, the spring | `Public/North/`, `Private/North/` |
| The ranger | `Content/PeopleData/people.json` (key `ranger`), docs/people.md |

## Contracts between the parts

**Frame.** Everything in `north.json` is map metres; Unreal = `W.to_unreal(x, y, z, portal_face_y)`; Z from the built
ground (`terrain.ground_z` in scripts, a trace in the game). `yaw` is Unreal degrees of the object's **front**
(map +X is yaw 0, map +Y is yaw 90).

**Meshes.** Blender builds `blender/export/north/<kind>[_v<variant>].glb`, one file per kind and variant, origin on the
ground under the object's middle, Blender **-Y is the front** (glTF +Z forward; Unreal's importer turns that into +X...
check one mesh in the editor and record the fix in `manifest.json` `front_yaw`), Z up, metres. Unreal assets:
`/Game/North/Meshes/SM_North_<Kind>[_V<variant>]`. Kinds placed by `north.json`:

- camp: `fire_ring`, `tent_dome` (variants 0..2), `tent_ridge` (0, 1), `tent_tunnel` (0), `log_bench` (0, 1),
  `camp_table`, `tarp_shelter`, `woodpile`, `food_pole`, `clothes_line`, `board_camp`, `outhouse`
- signs: `board_reserve`, `barrier`, `sign_hot_ground`, `sign_post`
- spring: `spring_deck` (stands at `north.json` `spring`)
- the hut: `hut` (shell with exact collision), `hut_interior` (furniture), `hut_door` (the leaf for ALyudDoor)

`blender/export/north/manifest.json` lists every built mesh: `{kind, variant, file, tris, size [x, y, z], collision
("exact" | "box" | "none"), seats [[x, y, z, yaw]...], notes}` and for the hut the door's hinge, the bed, the chair,
the stove's flue, lamp positions (hut-local metres).

**Building the props.** `pip install bpy` (Blender 5.3 module), then `python3 blender/north/build_north.py [kind|kind:variant ...]`
(`--textures` first regenerates the atlas). `northtex.py` makes one 2048 trim-sheet atlas (base colour + normal, procedural,
material `M_North_Atlas`) shared by every prop; `northkit.py` has the geometry helpers (boxes, cylinders, tubes, cloth shells with
thickness, rounded stones, plates with holes) and the UV lint (every face stays inside its tile window); `props*.py` are the
kinds, `kinds.py` the list. The GLBs carry material *names* only (`M_North_Atlas`, `M_North_Text_<id>`), no images, so each is 30-175 KB: `import_north.py materials` imports the atlas once (`T_North_Atlas`, `T_North_Atlas_N`) and builds the materials, `import` assigns them by slot name. Previews and contact sheets: `python3 blender/north/preview_north.py [--sheet] [--camp]`.
Words on boards are typeset by `tools/north_signs.py` (strings `north.sign.*`, `north.board.*`; textures
`textures/localized/<lang>/north_<id>.png`; the GLBs carry the Russian ones in `M_North_Text_<id>` materials, the game swaps the
language through `LyudText::LoadTexture("North", "north_<id>")`). `sign_post` is one mesh whose planks show `sign_post_spring`;
instances for the cone and the camp swap that texture to `sign_post_cone` / `sign_post_camp` (same UVs). The manifest's `size` is
the mesh's bounding box including guy lines and pegs, `body_size` the object alone; `seats` are `[x, y, z, yaw]` in mesh space
(yaw: degrees counter-clockwise from above, 0 = facing the object's front -Y). `spring_deck` has its origin at the pool's centre
(radius 2.6), the deck on the east rim at x 2.3..4.5, top z 0.45; two steps lead down west into the pool, the game's water
surface should be near z -0.05.

**Footbridges.** `log_bridge` (`SM_North_Log_Bridge_V0..2`, `blender/north/props_bridge.py`) is a rustic log footbridge over the river's
channels: three peeled stringers, a deck of rough half-log planks 1.1 m wide, one pole handrail on the +Y side, log X-trestles
standing on the river bed (feet 2.6 m below the deck). Variants by the water they span: v0 deck 16 m (spans up to 11 m, trestles under the
two ramp tops), v1 21 m (up to 16 m, plus one in the middle), v2 31 m (up to 26 m, five trestles 6.5 m apart). The last 2.5 m at each end is a
ramp that drops 1.0 m (toes at z -1.0, stringers and a sill reach z -1.5 so they sink into gravel bars 0.3..0.9 m under the deck).
**Origin and axis:** the origin is the middle of the deck at its top walking surface (z 0 = where feet stand); the deck runs along
the mesh's local **X** axis (`"axis": "x"` in the manifest), the handrail is on local +Y. With the kit's yaw rule (front = -Y) the deck therefore
runs along map **Y at yaw 0 / 180** and along map **X at yaw 90 / 270** (check against the table above before placing). Collision is `box`: the manifest's
`collision_boxes` (deck, two ramps, handrail pieces; mesh space, Blender axes, `rot_y_deg` right-handed about +Y, positive tips +X down); trestles have no collision.

**Words.** Boards carry words: follow `.claude/skills/localization/SKILL.md` (ru/en/ja, text keys `north.sign.*`,
`place.north.*`, `dev.tp.north_*`).

**Fumaroles.** `north.json` `fumaroles`: `{id, kind: vent | steam | mudpot | spring, x, y, size}`. Ids are stable.

## Performance

Follow the perf checklist (project memory): steam, fire and snow never rebuild instanced meshes per frame, moving
meshes stay out of Lumen, nothing ticks when the hero is far (the North is > 1 km from the town: everything there sleeps
until the hero is within a few hundred metres).

## Game side

The living things of the North and the placing in Unreal (`Public/North/`, `Private/North/`, `unreal/scripts/import_north.py`).

**Classes.**

| Class | What |
|---|---|
| `ULyudNorthSubsystem` (world subsystem) | Reads `Content/North/north.json` (places, fumaroles, far volcanoes). Four times a second it measures the player (the possessed pawn: the hero or the bird he flies) to every `ALyudNorthThing` and wakes it within `lyud.North.Range` (300 m, +25 m before it sleeps again) and calls it *near* within 70 m (sound). Owns the shared steam pool and the far plume, feeds them the wind (`ULyudWindSubsystem::GetWindAt` over the valley, gusts included) and the air's density (`ULyudWeatherSubsystem`: cold and damp air keeps steam thicker and longer). Quest hooks: `GetFumaroles()`, `IsHotGround(FVector)` / `FindHotGround`, static `IsInNorth(FVector)` (map Y >= 2330), `GetPlaces()`, `HasReached(id)`, `OnRegionChanged(bEntered)`, `OnPlaceReached(PlaceId)` (first time in a play, within 30 m). Saves nothing yet. |
| `ALyudNorthThing` | The sleeper base: no tick, no sound, no particles until the manager wakes it. |
| `ALyudFumarole` | One per north.json fumarole (`FumaroleId`, `Kind` Vent / Steam / MudPot / Spring, `Size`). Vent: a crusted mound with a dark throat, a jet 6..15 m by size leaning downwind. Steam: wisps over a few m² (the ground is the terrain's sulphur layer). MudPot: a pool `Size` m across in a dried rim, bubbles swelling and plopping (`NorthModel.h`), light steam. Spring: a stone ring round still green water (r 2.6 m), thin steam. Rims and pools are `FLyudMeshKit` meshes (transient) drawn always, to 400 m; no collision (quests ask `IsHotGround`). |
| `ALyudCampfire` | The camp's fire on the ring: six charred logs, three flame tongues (the festival bonfire's way), embers, a warm flickering point light without shadows (900 cd), grey smoke into the steam pool, a crackle loop. Burns from dusk (sun under 2°) until 1:30; **E** within 2.6 m lights / puts it out (polled, never consumed; prompts `ui.north.fire_light` / `ui.north.fire_out`), the hero's choice holds until the schedule itself turns. |
| `ALyudNorthSteam` | The cigarette smoke's technique (`ALyudSmokeFx`) at valley scale: one instanced mesh of 448 camera-facing puffs moved on the CPU, only the live range written (`BatchUpdateInstancesTransforms`, no proxy rebuild), hidden and not ticking when empty; puffs drift with the wind more as they age. Material `M_North_Steam` (custom data 0 opacity, 1 seed, 2 grey), else `/Game/Smoking/M_Smoke`. |
| `ALyudNorthPlume` | The smoking far volcano's plume (north.json far_volcanoes `plume`): 12 huge unlit sprites rising ~600 m and bending downwind, positions from the clock alone, written 4×/s, colour by the sun. |
| `NorthModel.h` | The plain rules (wake ranges, bubbles, the fire's schedule, vent columns, steam density, hot radii): `tools/north_sim/run.sh model` checks them without Unreal. |

Everything moving is out of Lumen and the distance fields, casts no shadow, has no collision; no instanced mesh is rebuilt per
frame; nothing is allocated per frame.

**Sounds.** `python3 tools/north_sounds.py` -> `audio/sfx/north/north_{vent,mudpot,spring,campfire}_01.wav` (4..6 s seamless
mono loops, 48 kHz, numpy) -> `import_audio.py` (`IMPORT_ONLY=sfx/north`) -> `/Game/Audio/North/S_*`, pools of
`ULyudSoundscapeSubsystem` (`AttachLoop`, released with `bAutoDestroy` before `FadeOut`). Only things *near* (70 m) play.

**River water (game side).** `north.json` `"river": {"water": [[x, y, z, half], ...], "threads": [[[x, y], ...], ...],
"mouth": [x, y]}` (map metres; `water` is the centre line every ~15 m from the east edge to the mouth with the water's
surface z and the sheet's half width; `threads` are the channels, informational; all optional: no `river`, no water).
`ULyudNorthSubsystem` reads it in `LoadData` and spawns one `ALyudNorthRiver` (tagged North, `GetRiver()`): a flat sheet
along the centre line, flat across at each sample's z, the plan smoothed by Catmull-Rom (3 steps a sample, z and width
linear; an edge on the inside of a tight bend waits instead of folding back), built once as transient static meshes of
64 points (~320 m) each so parts off screen are culled, drawn to 1.5 km, no collision, no shadow, out of Lumen, the
distance fields and ray tracing. UV0: U across 0..1, V along = metres / the local speed factor (0.6..1.8 from the fall
over the neighbours and the narrowness), restarting at a multiple of 64 in every part (full-precision UVs); vertex
colour R = fast 0..1, G = 1 at the two ends (fade). Material `/Game/North/Materials/M_North_River`
(`import_north.py` `build_river()`, in the `materials` step or alone with `river`: lit translucent, forward shaded,
two-sided; the flow runs on the GPU clock at `Flow` V/s with noise repeating every 64 V; opacity from the water's
thickness along the view, so gravel shows through at the banks; foam and rougher glints where fast; `Shallow`/`Deep`
colours), else `M_North_Water`, else the engine's basic shape material. It sleeps like any North thing but is measured
from its water's edge (`ALyudNorthThing::RangeDistanceCm`): near (70 m) it plays `/Game/Audio/North/S_north_river_01`
(`tools/north_sounds.py` `north_river_01`: rush over gravel, gurgles, splashes; 6 s loop) from one audio component put on
the centre line's point nearest the camera four times a second (its tick while awake), full within 6 m and falling off
over 70 m, louder where fast and wide; asleep: no tick, no sound. Missing sound or material: a log line, no failure.
`IsInWater(location)` for quests and swimming. `lyud.North.Report` / `DebugReport()` end with
`river samples N length L m parts P threads T awake A near N sound S`; `test_north.py` logs it at the end.

**Console** (each a plain static of `ULyudNorthSubsystem` a dev panel can call): `lyud.North 0|1` hides / shows everything
tagged North (asleep while hidden), `lyud.North.Goto <place id>` (onto the ground by a trace), `lyud.North.Fire -1|0|1`
(schedule / out / burning), `lyud.North.SitSpring 1|0` (the hot spring, below), `lyud.North.WakeAll 0|1`, `lyud.North.Range <m>`, `lyud.North.Report` (a `NORTH` line).

**Import and placing.** `unreal/scripts/import_north.sh` (closed editor; `STEPS="sounds materials import place"`,
`FILES=...` limits the import): sounds; materials (`M_North_Steam`, `M_North_Plume`, `M_North_Mud`, `M_North_Water`,
`MI_NO_<slot>` for any `MAT_SPEC` in `blender/north/*.py`); one process per glb listed in `blender/export/north/manifest.json`,
`manifest_hut.json`, `far.json` (`tools/north_far.py` is run when `far.json` is missing) -> `/Game/North/Meshes/SM_North_<Kind>`
(`_V<n>` for kinds with variants, `SM_North_Far_<Id>` for the backdrop), Nanite off, collision by the manifest; then
`place`. `import_world.py` calls `import_north.place_safe(face_y)` too. Placing: camp things and signs as StaticMeshActors on
`terrain.ground_z` (a sign keeps its words' key as a tag `Text:<key>`), `ALyudCampfire` on the fire ring, an `ALyudFumarole`
per fumarole, the spring's deck, the hut (shell tagged Roof, furniture) with an `ALyudDoor` (manifest_hut.json `door` in
place_doors.py's format, hut-local Blender metres; else the standard hole in the middle of the front wall; `hut_door` as the
leaf mesh), the far volcanoes at `(x, y, base)` and the far land at the map's origin (no collision, no shadow, never culled,
out of Lumen and the distance fields). A kind without a mesh gets a cube of its size tagged `NorthPlaceholder` and a log line.
All tagged `North` (things on the ground also `NoGrass`, `NoTree`), folder `Outside/North`, lighting channels 0 and 1.
Facing: the actor turns by `yaw - front_yaw`, `front_yaw` from the mesh's manifest row (Unreal degrees of the mesh's front in
its own frame), default 90 (Blender -Y front -> Unreal +Y, as `import_city.py` found).

**Test.** `unreal/scripts/test_north.sh` (offscreen; `NORTHTEST_ONLY=`, `NORTHTEST_WAIT=`): every place by day, the sleep
rule (nothing awake at the gate, the valley awake in the valley, everything with WakeAll, nothing while hidden), the camp and
the valley at night (the fire by its schedule, then forced), screenshots in `Saved/Screenshots/MacEditor`, `NORTHTEST` lines
in `Saved/Logs/test_north.log`.

### Hot spring (game side)

`ALyudNorthSpring` (`Public/North/LyudNorthSpring.h`): the spring one sits in. `ULyudNorthSubsystem` spawns it (tagged North,
a sleeper like every `ALyudNorthThing`) when `north.json` has `"spring": {x, y, r, seats: [{x, y, yaw}], surface_z?}`; seats are
in the pool, `yaw` Unreal degrees of where the sitter faces; no `seats` yet: four guessed at 0.6 r facing the middle. It draws
nothing (the pool and its steam are the Spring `ALyudFumarole`'s, the deck is `spring_deck`). The water's surface: `surface_z`
(map metres) when given, else the Spring fumarole's water (its actor on the built ground + its pool height), else a trace at
the middle - 5 cm.

While the hero is near, **E** within `r + 1.5 m` (polled, never consumed; prompt `ui.north.spring_sit`) takes the nearest free
seat: he wades in (the seat's frame rides a carrier component gliding from his feet to the seat and down, 2 s) and sits on
`AHeroCharacter::SitOn`'s bench pose, the cushion `WaterOverSeat` (55 cm) under the surface so the water is about his chest;
the look turns calmly across the pool, soft steam rises round his shoulders. **E** again (`ui.north.spring_out`), a step or
Space gets him out onto the ground `r + 0.9 m` behind his seat. The legs go into the terrain under the pool (no basin is cut
yet): the water hides them. The game has no rest / time-skip mechanic yet, so sitting passes no extra time.

NPC hook: `FindFreeSeat(Near)`, `Occupy(Seat, Who)`, `Release(Who)`, `IsSeatFree`, `GetSeatWorld(Seat)` (origin on the pool's
floor under the cushion, X the sitter's facing; cushion `SeatHeight` 40 cm over it), `GetWaterZ()`, static `FindSpring(World)`.
The hero takes his seat through the same `Occupy`.

Console: `lyud.North.SitSpring 1|0` (sit from anywhere in the nearest free seat / get out; a `NORTH spring ...` line).
Test: `test_north.py` steps `spring_sit` (seated, eyes 5..90 cm over the water) and `spring_out` (standing again).

## Plants

All of it is the game's own plant system: eleven new species (six of the volcano land, five of the river valley below) in `Private/Plants/PlantSpeciesNorth.cpp`
(`PlantHerbs::NorthSpecies()`, appended to `PlantSpeciesLibrary::All()`), planted by `tools/north.py` (`FOREST_MIX`,
`forests()`, `herb_patches()`, `grass_patches()`). Names: `plant.<name>.name` (ru/en/ja). Triangles are from
`tools/plantsim/run.sh` (the model without Unreal, below); bake LODs switch at 15 / 55 / 150 m.

| Species | Latin (ru) | Look | Where | Bloom (day) | Cost |
|---|---|---|---|---|---|
| SiberianDwarfPine | *Pinus pumila* (кедровый стланик) | a sprawling evergreen 1.3..2 m high, 2..2.5 m across: limbs bowed out low from the base, needles in fives as dark blue-green brushes at the tips, small purple cones | the belt above the stone birch on the drier rises (`dwarfpine`), scattered over the upper meadows (`shrub`), a few with the rhododendron | male cones, purple, 172..200 (peak 186) | bake LOD0 3.3k (4.7k in bloom), LOD1..3 1.75k / 1.5k / 1.35k |
| DwarfAlder | *Alnus alnobetula* subsp. *fruticosa* (ольховник, ольха кустарниковая) | a many-stemmed thicket shrub 2..2.5 m, stems leaning out; toothed oval leaves, a dull yellow in late September; little woody cones kept through the winter | the hollows: the trough along the eastern mountains' foot, the swale under the Ridge's south side, the Ridge's wet north slope (`alder`) | catkins, 150..180 (peak 165) | bake LOD0 8k (16k with catkins), LOD1..3 3.3k / 1.45k / 1.3k |
| GoldenRhododendron | *Rhododendron aureum* (рододендрон золотистый) | an evergreen mat 0.35..0.5 m high, leathery oval leaves, pale yellow five-petalled flowers in clusters | round every snowfield (`rhodo`), where the snow lies late | pale yellow, 158..192 (peak 175) | bake LOD0 3k..3.9k, LOD1..3 1.4k / 230 / 170 |
| Fireweed | *Chamaenerion angustifolium* (иван-чай) | 1.3..1.4 m stems in clumps of four with a raceme of magenta flowers; leaves crimson to orange in September; gone in winter | herb patches along both edges of the lava flow, on the Cone's ash apron, round the camp | magenta, 192..242 | 2.9k / lite 500, drawn to 58 m |
| MountainAvens | *Dryas octopetala* (дриада восьмилепестная) | an evergreen woody rosette ~0.1 m, scalloped leaves, white eight-petalled flowers with a golden eye | herb patches on the cinder's edge round the Cone and by the snowfields' sides | white, 168..205 | 0.7k / lite 310, drawn to 25 m |
| CottonGrass | *Eriophorum vaginatum* (пушица влагалищная) | a tussock of fine arching leaves, 0.2 m stalks with white cotton heads; russet in autumn | herb patches in the wettest hollows (the terrain's wetness decides inside the patch) | white heads, 175..228 | 0.7k..1k / lite 320..560, drawn to 25 m |

Reused: stone birch, rowan and dwarf birch (the park woodland), juniper in the belt, MeadowGrass / RedFescue /
WavyHairGrass round the places, Lingonberry (herb patches) and ReindeerLichen (grass patches) on the Cone's apron.

**Forests** (`FOREST_MIX`, baked by `AProceduralForest`; `world_map.forest_detail(mix)` gives the extra properties
import_world sets):

| Mix | Species (weight) | Where | Density | Drawn to |
|---|---|---|---|---|
| `stonebirch` | StoneBirch .7, Rowan .18, DwarfBirch .12 | Y 2330..2700 | 0.085 | 500 m |
| `dwarfpine` | SiberianDwarfPine .6, DwarfBirch .2, Juniper .12, Rowan .08 | the belt, Y 2700..3060, dry cells | 0.2 thinning upward | 350 m |
| `alder` | DwarfAlder .65, DwarfBirch .2, Rowan .15 | the belt's wet cells (`_wet_site`: `_hollow` < -0.8 m, or a north-facing slope that is not a rise): the real land's gullies, old lava channels and the cones' shaded feet | belt x 1.25, slope 0.1 | 350 m |
| `shrub` | DwarfBirch .4, SiberianDwarfPine .3, Juniper .2, DwarfAlder .1 | the belt, Y 3060..3420, dry cells | down to 0.045 | 300 m |
| `rhodo` | GoldenRhododendron .85, SiberianDwarfPine .15 | a frame 45 m round each snowfield | 0.45 | 150 m |

The belt is sorted into 130 x 120 m cells (`BELT_CELL`) and merged into runs along X, so it is a few long actors per
row. `world_map` carves out snow, cinder, lava, steaming ground and the pads (`tree_free`). The macro shape has no small
gullies, so alder follows the big hollows only; TerrainGen's own runnels are not known to Python.

Instances: the North had 5132 (stone birch 1813, shrub 3319); now 9634 (stone birch 1813, dwarf pine 3220, alder 1157,
shrub 1142, rhodo 2302) in 103 actors. The whole map: 191 tiles / 110 715 trees before, 230 / 115 217 after
(`python3 -c "import sys; sys.path.insert(0,'tools'); import world_map as W; f=W.forests(); print(len(f), sum(r[5] for r in f))"`).

**Herb patches.** `AForestFloor` draws its herbs only in wild woods, so the meadow flowers come from `AProceduralGrass`:
when its `Species` names a plant system herb instead of a grass it scatters that herb (`HerbDensity` plants per m2
times `DensityScale`, by `HerbHabitat` on the terrain masks: none on snow, lava, full cinder or steam), as hierarchical
instances of the herb's own two-LOD mesh (`PlantHerbs::BuildMeshLods`: LOD1 is the lite build), two seeds of ages
`HerbAge` and twice that, no shadows, no collision, drawn only to `HerbDistance` (0: 45 x the herb's height, 25..70 m).
They follow the calendar like the forest floor's herbs (`M_FloorPlant`, rebuilt only when the plant's state changes,
hidden while a perennial is dormant or a low herb is under snow). Only built in a game world, and only when the
player's camera first comes within the draw distance + 150 m of the patch's rectangle (nothing at BeginPlay or far
away; near patches build and re-season one a frame; `DescribeState` says "not built yet" until then). `herb_patches()` returns
them as `grass_patches` rows; `HERB_PATCH_SETTINGS` (read through `world_map.grass_patch_settings(species)`) gives each
species' density, habitat and age, which import_world sets: 33 fireweed, 19 mountain avens, 6 lingonberry, 6 cotton
grass patches, 4 of reindeer lichen.

**River valley.** Five more species in `PlantSpeciesNorth.cpp` for the braided river (the Avacha's look): willows on
the gravel bars and islands (forest bakes), the tall-herb meadow (шеломайник) on the terraces and sedge at the wet edges
(herb patches). Ages of the sizes below: the willows 30 years (bar willow 20), the herbs 6.

| Species | Latin (ru) | Look | Where | Bloom (day) | Cost |
|---|---|---|---|---|---|
| SakhalinWillow | *Chosenia arbutifolia* (чозения толокнянколистная) | a straight grey trunk 11 m at 30 (14 m at most; 4.7 m at 8), a narrow open crown of steep limbs (reach ~1.9 m), narrow pale blue-green leaves, red young shoots; the leaves fall yellow from early September | gravel bars and islands, in narrow groves along the channels (`gallery`, young in `riverbar`) | yellow hanging catkins with the leaves, 145..170 (peak 156) | bake LOD0 61k (72k with catkins), LOD1..3 22k / 5.8k / 4.7k; young (8 y) 8.6k |
| BarWillow | *Salix udensis*, *S. schwerinii* and kin (тальник) | a thicket of straight red-brown wands 2.2..2.5 m from one root, narrow 12 cm leaves; yellow in late September; bare red wands in the snow | the bars themselves, the islands' edges (`riverbar`) | upright silver pussy catkins on the bare wands, 128..158 (peak 140) | bake LOD0 8.6k..9.3k, LOD1..3 3.2k / 1k / 0.8k; bare in winter 1.1k |
| KamchatkaMeadowsweet | *Filipendula camtschatica* (шеломайник, лабазник камчатский) | 2.3..2.5 m: three stout stems, huge palmate five-lobed leaves on long stalks, largest low down; heads of cream florets round the tops; brown in late September, gone in October | herb patches on the terraces and the islands' crowns, in drifts | cream, 192..228 | 1.2k in leaf, 5.2k in bloom / lite 0.45k..1.8k, drawn to 70 m |
| CowParsnip | *Heracleum lanatum* (борщевик сладкий) | 1.6..1.9 m: two stems with a side branch, huge three-lobed toothed leaves, flat white umbels on the tips | herb patches with the meadowsweet, thinner, and at the terraces' edges | white, 180..218 | 1.4k in leaf, 2.7k in bloom / lite 0.85k..2.2k, drawn to 70 m |
| RiverSedge | *Carex* (осока) | a tussock of long arching keeled blue-green leaves 0.5 m; straw in October | herb patches on the wet gravel at the channels' sides, the spring's runnels (habitat Wet) | (none drawn) | 336 / lite 120, drawn to 25 m |

The willows: bar willow is drawn far as its summary from 50 m like the other shrubs (`import_world.SUMMARY_DISTANCE`),
is no refuge for animals (`AnimalLife IsTree`). The herbs grow fast after the snow (up 135..145) and die back in early
October; meadowsweet is the costliest herb (its plume is a third of its triangles): plant it in patches at about half a
plant per m2, not by the thousand. Planting (`tools/north.py`, owned by the land's worker), recommended:

    FOREST_MIX["riverbar"] = [("BarWillow", 0.75, 4, 15), ("SakhalinWillow", 0.15, 3, 8), ("DwarfAlder", 0.1, 6, 20)]
    FOREST_MIX["gallery"]  = [("SakhalinWillow", 0.55, 10, 40), ("StoneBirch", 0.15, 15, 60), ("DwarfAlder", 0.15, 8, 30),
                              ("BarWillow", 0.1, 5, 15), ("Rowan", 0.05, 6, 14)]
    FOREST_DETAIL["riverbar"] = {"cull_distance": 35000.0}
    HERB_PATCH_SETTINGS["KamchatkaMeadowsweet"] = {"herb_density": 0.5, "herb_habitat": "Meadow", "herb_age": 4.0}
    HERB_PATCH_SETTINGS["CowParsnip"] = {"herb_density": 0.2, "herb_habitat": "Meadow", "herb_age": 3.0}
    HERB_PATCH_SETTINGS["RiverSedge"] = {"herb_density": 2.5, "herb_habitat": "Wet", "herb_age": 4.0}

**Checking.** Without Unreal: `tools/plantsim/run.sh` (all species, or `run.sh all`, or `run.sh Name[:age:day] ...`)
compiles the plant model (`PlantModel`, the species files, the herb builder up to its Unreal part) with clang and the
Mac's warnings (`-Wshadow`, `-Wunreachable-code-loop-increment`) against a small shim of the engine types, and prints
each species' size and triangles per LOD. `run.sh render out.png Name:age:day[:seed] ...` draws them (`tools/plantsim/render.cpp`: the
skeleton's wood, leaf clusters, catkins and fruit, or the herb's real mesh; flat shaded, no textures, a 2 m pole
red and white every half metre), panels side by side at one scale: the shape, the colours and the season to look at
before Unreal. In Unreal: `unreal/scripts/make_plant_north_level.py` builds
`/Game/Dev/L_PlantNorth` (ages in rows, a row per season, a forest of the belt's mixes, three herb patches), then

    UnrealEditor <uproject> /Game/Dev/L_PlantNorth -game -RenderOffscreen -ResX=1280 -ResY=720 -windowed \
        -dpcvars=lyud.MainMenuOnStart=0 "-ExecCmds=py unreal/scripts/shoot_plant_north.py"

writes the shots to `NORTH_SHOTS` (default `/tmp/northshots`).
## People

Everything here is a **proposal** (not canon, docs/story/README.md): names, ages, hours, lines, voice, looks.

| Key (body) | Name | Age | Who | Home | Day |
|---|---|---|---|---|---|
| ranger (Ranger) | Матвей Ильич Кречет | 58 | the reserve's ranger, thirty years at the barrier | the hut (`north_hut`) | up 5:30; the weather post 6:00; the gate 7–12 (not Saturday); the round gate → camp → spring and back 12–14:30; stove and tea, the porch; the radio hour 20–21; bed 21:45. Saturday morning down to the Gastronom for supplies (~09:00), back for the round |
| melnikova_anya | Аня Мельникова | 22 | geology student, maps the lava | tent 1 | field work 9:30–16; the fire, the spring |
| ito_ren | Рэн Ито | 23 | geology student | tent 2 | field work 9:30–16; the fire, the spring |
| savin_gleb | Глеб Савин | 34 | photographer, waits for the light on the Cone | tent 4 | up at 5 for the dawn; the view spots at sunrise and sunset |
| okada_kiyo | Киё Окада | 71 | comes to the spring every autumn (forty years) | tent 3 | the spring, the fire, short walks |

**Places** (`tools/people_map.py` `build_north`, from `tools/north.py` `people_spots()`; all `"north": 1` = island 3):
`north_hut` (a home, kind flat, `private`: never given to a town household; bunk, table chair with the radio, stool,
stove), `north_porch`, `north_gate` (4 spots), `north_weather`, `north_campfire` (two seats a log bench),
`north_camp_table`, `north_spring` (the deck's bench), `north_patrol` (the round's spots in order), `north_walk`,
`north_field`, `north_view`, `north_tent_1..4` (homes, private). Every spot is an offset from `HUT`/`GATE`/`CAMP`/
`SPRING`/`FUMAROLES_C`/`CONE`/`ANCHORS` (and `snowfields()` when the land has them), read at run time, so it follows the
land when it moves; the hut's seats and the fire's benches come from `blender/export/north/manifest.json` when it has
them.

**Getting there.** The walk grid (`tools/animal_map.py`, `Y1 = 1900`) ends south of the North. People walk the grid to
`north_trail` (a place of kind `trail` on the valley's grid, near its north edge), then the trail's way (a fixed 3D line up
the dirt track, `terrain._route` town → polar → river_south, the footbridges, river_north → north_gate) to the gate, and inside the North straight over the
`north` ground patch (10 m cells round every North place, z from the built terrain). PeopleSim: island 3, hop kind 5
(trail on foot). From the hut to the Gastronom is ~2 km (~30 min each way): the way costs ~1 h of score, so only a
strong like (`supplies_town`, Saturday) takes the ranger down; the townsfolk keep off the North's pastimes
(`campfire`, `hot_spring`, `walk_north`, `photo_cone` have low bases; the North's people like them). The North takes no
part in festivals (`LyudFestival` uses island 0).

**Words.** `npc.<key>.name|role`, the ranger's 15 lines (first meeting, three plain ones, fumaroles, bears and the food
pole, the spring, the snowfields, the Cone, the campers, the weather, four sun lines) and two lines a camper, ru/en/ja,
`[Ranger]` ... `[Okada_kiyo]` in `Content/Dialogue/npc_dialogue.txt`.

**Voice.** `localization/voice.json` `ranger`: a MiniMax voice designed from a pinned prompt (an older, weathered,
hoarse baritone; speed 0.92, pitch -2). Not designed or voiced yet (paid, ~$3 + ~$0.32); until then his lines show as
text with no sound. The campers are `skip` until accepted. On the Mac:
`python3 tools/gen_voice.py --design`, then `python3 tools/gen_voice.py --lang ru` (and `en`, `ja`; or name the
`line.ranger.*` and `line.hero.ranger_first.2` keys), then `unreal/scripts/import_voice.py`.

**Bodies.** `people.json` `body` (`Ranger`, `Melnikova_anya`, ...) is who they are; `mesh` `Town/R_<key>` is the free
stand-in from the resident generator: `python3 blender/residents/looks.py --people`, then `build_residents` and
`import_residents` for those keys (docs/residents.md). Until either exists they live unseen. The ranger's own model:
`blender/characters/ranger/` (brief, `gen.json` pinned, `character.json`); paid (hi3d views + model, quote first).

**Rerun after the land changes:** `python3 tools/people_map.py --north` (after the terrain build: the patch's z and the
trail's way come from it), then `tools/peoplesim/run.sh test` (the North's checks: the hut at night, the gate by day,
the round, never stranded, the fire with the campers, Saturday's supplies).
