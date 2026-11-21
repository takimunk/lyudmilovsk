# The Aton coast (Атонский берег)

The land between the club (the Aton station on its headland) and the town's south edge, and the ways from one to the
other. Before this the door opened on a rounded box of a terrace whose sides fell at 40..50 degrees onto a flat empty
plain; the only way to the town was the cable car. Now there is a walk of about half a kilometre with things to see on
it, a second way along the sea, and a side trip inland.
Everything here is a **proposal**, not canon (docs/story/README.md): the names, the boards' words.

Source of truth: `tools/south.py` (`python3 tools/south.py` writes `Content/South/south.json` and
`Content/Dev/teleports/south.json`; `--check` tests the land's own rules). It is hooked into `tools/world_map.py` the
way `tools/north.py` and `tools/east.py` are.

## The land

Map metres (docs/world_map.md: X west toward the sea, Y north, Z up from the club's floor; the valley floor is at -37).

| What | Where | How |
|---|---|---|
| **Headland** | the old spur, X -100..80, Y -30..180 | `south.spur`: the same level top (the forecourt, the alley, the Aton station and the sea line's corner pylons stand on it), but the brow wanders (`_wander`) and the flanks differ: short and steep over the reedbed, 65..75 m long in the east |
| **Shoulder** (отрог) | from the north-east brow (-88, 172) to the valley floor at (-187, 306) | `south.shoulder`: a grassy ridge in an S, 200 m long, the crest falling at 10..13 degrees, flanks at 25 (grass holds, the bluff's gullies do not start). The trail to the town walks on its crest, the Sun line's cabins pass beside it |
| **Brook** (ручей) | from the ravine's head (-650, 36) to the sea at Y 253 | `south.brook_cut`: 900 m. A ravine 8 m deep in the upland's wooded slope ("ravine"), then a bed 2..3 m wide and 0.9 m under the valley floor winding west under the town's south edge ("valley"), through the starlings' reedbed ("reed") and out through the dune bank and the beach ("bank"), where the tide channel of Y 260 takes it on. Falls about 0.5 % in the valley |
| **Stair corridor** | the sea cliff at Y 38, X 75..109 | `south.keep_flat` keeps the macro face exactly (no gullies) under the timber stair, 10 m wide; `blender/south/stair_profile.json` is that face for the stair's builder |

`world_map` hooks: `spur()`, `plateau_south()` -> `south.shape`, `_height()` -> `south.outlet`, `keep_flat`, `_tree_free`,
`forest_cover` -> `south.cover`, `forests`, `coast_plants`, `grass_patches`. `terrain.py`: `south.ANCHORS`, `TRACKS`,
`fixed_tracks()` (the old routed tracks from the alley's end to the town and the inland woods are gone: the designed
trails replace them), and the brook's bed in the `gravel` mask (`south.gravel`: silt and sand under the water).

## The ways

| Trail (`south.trails()`) | Length | |
|---|---|---|
| `town` | 350 m | the alley's end -> round the station -> down the shoulder -> the footbridge -> the stele -> the town's first terrace (then a routed track to the square) |
| `belvedere` | 70 m | the alley's end -> the lookout on the north brow |
| `sea` | 130 m | the alley's end -> the top of the stair down the cliff; the beach halt of the sea railway is 100 m south of its foot |
| `hide` | 140 m | the shoulder's foot -> the bird hide; from it the boardwalk goes 130 m through the reeds to the beach by the brook's mouth, and the promenade starts 200 m north along the sand |
| `spring` | 390 m | the shoulder's foot -> along the brook's south bank -> the old orchard -> the spring under the ravine; a routed track goes on into the inland woods |
| cliff walk (routed) | 420 m | the club's forecourt -> along the cliff top under the Sea line -> the Mayak station |

## Places (the map's markers)

`south.places()` -> `Content/UI/map.json` (through `tools/ui_map.py`), found by walking up to them; names
`place.<id>.name` and teleports `dev.tp.<id>` (group `south`) in `Content/Localization/south/strings_<lang>.json`.

| id | Name | Where | What is there |
|---|---|---|---|
| `aton_belvedere` | Смотровая площадка | 27, 176 | a rotunda on the brow over the reedbed, the town and the sea; a coin viewer; an old oak |
| `aton_descent` | Отрог | -139, 222 | the ridge walk; a bench under a birch on the last bend |
| `brook_bridge` | Мостик через ручей | -203, 324 | a plank footbridge, a black poplar |
| `south_stele` | Стела «Людмиловск» | -207, 356 | the town's name in concrete with a sun, poplars |
| `bird_hide` | Скрадок у камышей | -47, 273 | a hide on stilts looking west over the starlings' roost; the boardwalk |
| `brook_mouth` | Устье ручья | 118, 247 | a boat shed, nets drying, boats keel up, stones to step over the water |
| `old_orchard` | Старый сад | -372, 196 | old apple trees gone wild, hives, a wattle fence, a table, haystacks on the meadow before it |
| `south_spring` | Родник | -541, 133 | a stone trough under an oak where the ravine opens |
| `spring_ravine` | Родниковая балка | -590, 100 | the wooded ravine, the brook falling over stones |
| `cliff_stair` | Лестница к морю | 73, 38 | the timber stair down the 41 m cliff |
| `cliff_lookout` | Обрыв | 88, -236 | a bench and a lone pine on the cliff's edge |
| `mayak_station` | Станция «Маяк» | 88, -384 | the Sea line's southern end |
| `mayak_lighthouse` | Маяк | 105, -426 | the lighthouse past the Mayak station: white with two red bands, 24 m, a keeper's annex |
| `coast_bunker` | Старый дот | 64, -128 | a half-sunk concrete pillbox on the cliff walk, its slit to the sea, moss on the roof |
| `boat_wreck` | Остов баркаса | 116, 132 | the ribs of a fishing boat in the sand between the brook's mouth and the stair |
| `devils_stone` | Чёртов камень | -299, 256 | a glacial boulder on the meadow by the spring trail (a split one stands on the cliff walk) |

## Things

`south.json` `things`: `{id, kind, x, y, yaw, variant, set, z}`; `set: "north"` are the North's kit meshes standing here
(benches, trail marks, boats, driftwood, boulders), `set: "south"` are built by `blender/south/` (the North's kit and
atlas, no new textures but the words): `python3 blender/south/build_south.py` (or Blender `-b --python ... --`) ->
`blender/export/south/<kind>[_v<n>].glb` + `manifest_<group>.json` per `props_<group>.py`.

| Group | Kinds |
|---|---|
| `props_walk.py` | `cliff_stair`, `plank_bridge` (6 and 9 m), `boardwalk` (plain, with a rail, with a bay and bench, end ramp) |
| `props_places.py` | `belvedere`, `viewer`, `bird_hide`, `fisher_shed`, `net_rack`, `picnic_table` |
| `props_land.py` | `town_stele`, `spring_trough`, `trail_sign` (four junctions), `info_board` (two), `hay_stack`, `beehive`, `wattle_fence`, `scarecrow`, `orchard_ladder` |
| `props_marks.py` | `lighthouse` (manifest `lamp`: the lantern's centre for a future light), `bunker`, `shipwreck`, `erratic` (whole, split) |
| `props_alley.py` | `park_lamp` (one globe, two; `lamps`), `park_bench` (3 seats), `flower_bed`, `litter_urn`, `bike_rack`, `halt_shelter` (3 seats), `drinking_fountain` |

Words on boards and posts: `tools/south_signs.py` -> `textures/localized/<lang>/south_<id>.png`, material slots
`M_South_Text_<id>`.

In Unreal: `unreal/scripts/import_south.sh` (`STEPS="build signs materials import place"`); `import_world.py` calls
`import_south.place_safe` after the North's. Meshes `/Game/South/Meshes/SM_South_<Kind>[_V<n>]`, actors tagged `South`
in `Outside/South`. A thing whose mesh is not imported is skipped, not replaced by a cube.

## The alley and the halt

The club's alley (door to station, `world_map.alley_trees`) is dressed by `south.things()`: lamps in the trees' line
between the pairs, taking sides in turn; benches across the path from a lamp with an urn by each; two flower bowls; at
its end a two-globe lamp, a drinking fountain and a bike rack, all clear of the station's 9 m footprint (`--check`
tests it). A lamp and a bench at the lookout, a lamp at the stair's top. On the beach's top by the stair's foot the
halt's shelter (open to the sea) with a lamp. The lamps do not shine yet (meshes only).

## Plants

All through the procedural plant system: alders, aspens and birches along the brook in broken runs (forest tiles of the
`wet` mix strung along it, thinned by `south.cover`), the orchard and the copse under the headland's east flank (`town`
mix: apple, maple, birch, rowan), eight single trees where one stands and looks (`south.plants()`: the oak by the
lookout, the oak over the spring, the black poplar by the footbridge, the lone pine on the cliff...), meadow patches
along every trail and reed and sedge along the brook's lower reach (`south.grass_patches()`).

## Game side

- The brook's water: `south.json` `brook.water` = `[[x, y, z, half], ...]` downstream, the same samples as the North's
  river. `ULyudNorthSubsystem::SpawnBrook` spawns a second `ALyudNorthRiver` for it (`GetBrook()`), woken and put to
  sleep with the North's things.
- **Seats and the viewer**: `import_south.py place` puts an `ALyudSouthSpot` (Public/South/LyudSouthSpot.h) at every seat
  of a placed thing's manifest row (`seats`: the benches, the rotunda, the hide, the boardwalk's bay, the stair's landings,
  the picnic table, the shed's bench) and where a viewer's looker stands (`stand_at`). `[E]` sits the hero down
  (`AHeroCharacter::SitOn`), at the coin viewer it holds the binoculars' view up (`ULyudGameScreen::HoldViewer`), worn or
  not, so birds can be told from the lookout. Polled through `ANpcTalkDirector::UsePressed`, never consumed; the spots
  sleep with the North's things. Console: `lyud.South.Sit 1|0`. Prompts `ui.south.*`.
- **Words in the game's language**: actors tagged `Text:south` get `/Game/South/Localized/<lang>/T_South_Text_<id>` in
  their `M_South_Text_<id>` slots when the level starts (`ULyudNorthSubsystem::LocalizeSouthBoards`; Russian is the
  material's own texture). A change of language shows after the level is loaded again.
- Finger-posts: `south.SIGNS` says where each post stands and what each plank points at; `south.sign_planks()` turns
  that into the planks' angles for `tools/south_signs.py` `PLANKS` (words) and `props_land.py` (geometry).
- Places are plain map places (`ULyudQuests`): `reach` events and the "new place" note work as for the others.

## After a change here

`python3 tools/south.py --check && python3 tools/south.py`, `python3 tools/transport.py` (the pylons are resolved
against the ground), `python3 tools/ui_map.py` and `tools/ui_map_ink.py` (restore `textures/ui/binoculars_mask.png`
if the cloud rewrote it), then the level's build (`unreal/scripts/build_scene01.sh`).

## Not done yet

See [south_backlog.md](south_backlog.md).
