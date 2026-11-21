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
| `town_gate` | Первая терраса | -213, 382 | where the town trail comes out onto the town's first terrace |

Each place has an `icon` (`south.PLACE_ICONS`: view, trail, bridge, stele, birds, boats, orchard, water, stair, lighthouse,
bunker, stone, station, gate), drawn beside its ring on the map once found. `tools/ui_map.py` adds two town places for the
walk's end: `square` (Площадь Солнца, S-SQUARE) and `market` (Рынок, its bakers' row).

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
| `props_light.py` | `lighthouse_lens`, `lighthouse_beam`, `picket_fence` (plain, with a gate), `meteo_screen` (see "Lighthouse meshes") |
| `props_alley.py` | `park_lamp` (one globe, two; `lamps`), `park_bench` (3 seats), `flower_bed`, `litter_urn`, `bike_rack`, `halt_shelter` (3 seats), `drinking_fountain` |

Words on boards and posts: `tools/south_signs.py` -> `textures/localized/<lang>/south_<id>.png`, material slots
`M_South_Text_<id>`.

In Unreal: `unreal/scripts/import_south.sh` (`STEPS="build signs materials import place"`); `import_world.py` calls
`import_south.place_safe` after the North's. Meshes `/Game/South/Meshes/SM_South_<Kind>[_V<n>]`, actors tagged `South`
in `Outside/South`. A thing whose mesh is not imported is skipped, not replaced by a cube.

## Lighthouse meshes

Backlog item 42 (the lighthouse and its keeper). `props_marks.py` `lighthouse`: the 12 lantern panes are quads in their own slot
`M_South_LanternGlass` (UV 0..1, one-sided, the game makes it see-through dark glass), the lantern floor (z 20.92) and the cap's
underside (z 22.5) are closed discs, so only the lens (`lighthouse_lens`, origin at the manifest's `lamp`/`lens`, z 21.71) is inside.
A hatch in the gallery deck on the front (-Y, x 0, y -1.765: iron coaming, dark hole, lid open on the drum, two grab stiles). The
gallery is walkable: collision boxes = deck slab (top z 20.0, no hole at the hatch), 32 rail posts to z 21.06, the lantern as two crossed
boxes from the deck up. Manifest points (mesh frame, yaw = degrees counter-clockwise from -Y): `hatch`, `gallery_view` (sea side:
map +X is the sea, so toward mesh (0.643, 0.766) at the thing's yaw 140), `door_stand` (on the first step, facing the door).
`props_light.py`: `lighthouse_lens` (glass slot `M_South_LensGlass`, panels UV 0..1, the bullseyes on +-X UV centre (0.5, 0.5);
no collision), `lighthouse_beam` (6 quads, 1 m, slot `M_South_Beam`, U 0 at the origin to 1 at the end), `picket_fence` (2.4 m along
X, variant 1 = a closed gate, not solid), `meteo_screen` (manifest `read_at`). `south.things()` rings the tower with 17 fence segments at
6.1 m (the gate, `picket_fence_1`, on the door's bearing 140 where the track comes in) and puts the screen at bearing 230, 4 m out.

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

## Things to do

Backlog item 39. `python3 tools/south_uses.py` writes `Content/South/south_uses.json` from `south.json` (`--check`); the game
spawns its spots when the level starts (`ULyudSouthUses`, `Public/South/LyudSouthUses.h`), only where the coast is placed
(any `ALyudSouthSpot` in the level), so nothing is imported in the editor. The spots are `ALyudNorthThing`s: asleep far
away, E polled only near, the nearest in reach has the prompt.

| Spot | Where | [E] |
|---|---|---|
| `spring_1` (drink) | in front of the spring's trough | bends down (the reach's crouch) and drinks from his hands, a line of what it tastes like; with an `empty_bottle` in the pocket fills it instead (`water`). Quests: `use spring` |
| `fountain_1` (drink) | the drinking fountain on the club's alley | drinks (`use fountain`) |
| `read_trail`, `read_reeds` (read) | in front of the trail's notice board and the board at the hide | the board's own words (`south.board.*`) as the first page, then more (distances; the starlings' roost and the peregrine) |
| `read_stele` (read) | in front of the stele «Людмиловск» | the terraces; what somebody scratched into it |
| `read_view` (read) | on the belvedere by the rail | what can be seen from there: the town, the sea and the bars at low tide, the cable car |
| boat seats (4) | on the edge of the two boats lying keel up at the mouth | sits down like on a bench (`ALyudSouthSpot`, cushion 62 cm) facing the sea |

Pages use the city's page widget (`SLyudCityPage`: E next / E close; a step away shuts it); quests hear `read <spot id>`.
Words: `Content/Localization/south_uses/strings_<lang>.json`. Console `lyud.South.Use [id]` (goes there and uses it),
`lyud.South.Uses`; developer mode, Actions: `south.empty_bottle`. Apples: the orchard's apple trees are the plant system's
(the `town` mix), and a live apple tree near the hero already lets him pick its fruit in season (`AProceduralPlant`
`bAllowPick`), so nothing was added for them.

Not done: the stepping stones over the brook's mouth (a misstep into the water needs a wet state the hero does not have);
the boats' seats are placed from the mesh's size, not from a seat in its manifest (check them in a shot); the seats on the
boats are not claimed against the fishermen standing by them (they stand, they never sit there).

## People

Townsfolk walk and use the coast (backlog item 37). `python3 tools/town/south_people.py` adds them to the town's data (like
`tools/town/town_activities.py`: idempotent, `--check`; `tools/town/town_life.py` runs it after itself). It reads
`south.json` (things, trails) and the meshes' manifests (`seats`, the hide's stair), and writes six places into
`Content/PeopleData/town_places.json` (just before town_activities' own), eight rows into `activities.json` and a few likes
into `townsfolk.json`:

| Place | Slots | Activity | Who, when |
|---|---|---|---|
| `S-ATON-SHED` | the shed's bench, the net racks, the boats | `mend_nets`, `boats_evening` | three old fishermen (men of 58+ who like the shore): nets 6:00-11:00 by day, the boats 16:00-19:30 |
| `S-ATON-ORCHARD` | a place at each hive, the picnic table's 4 seats | `tend_hives`, `orchard_picnic` | the beekeeper (a pensioner fond of nature, not one of the allotment's keepers) 9-17 at 13 °C and over; anybody at the table on warm Saturdays and Sundays |
| `S-ATON-HIDE` | the hide's 2 seats (in at the stair's foot, up to the landing) | `watch_starlings` | eight birdwatchers (the studious who love the open air) and the odd passer-by, only round sunset |
| `S-ATON-VIEW` | the rotunda's 4 seats, the brow benches by it and on the shoulder's last bend | `sit_belvedere` | anybody, by day, more at the weekend |
| `S-ATON-WALK` | a spot every 16 m up the `town` trail and out to the belvedere, then the same back down | `walk_aton` (a round: spot to spot in order, so the walk keeps to the trail) | anybody of 8+, by day, more at the weekend |
| `S-ATON-BROOK` | the stones by the footbridge, the spring's trough | `play_brook` | children of 6-13 after school (14:00-18:30, not Sunday) |

The life's own rules do the rest: nobody chooses the open air in the rain and whoever is caught by it goes home, nobody
is out there after dark (all but the hide's sunset are `daylight`), the frost and the cold keep the bees and the picnic
away. Lines: `tools/barks/lines/16_aton_coast.txt` (`doing = <activity>`; rebuilt by `tools/barks/build_barks.py`).
Checks: `tools/peoplesim/run.sh town` (struct `AtonWatch`): the places on the town's side of the walk grid, walkers and
sitters on a cool weekday and none of them over 15 m off the trail, fishermen only in the morning, the hide only round
sunset, children only after school, nobody between 22:00 and 05:00, no bees or picnic on a cool weekday; a warm Saturday
(bees, picnic, the trail walked) and a rainy Sunday (all but empty). `LYUD_ATON_DEBUG=1` prints whoever strays off the
trail.

Not done: the hero's seats here (`ALyudSouthSpot`) are not claimed against the townsfolk yet, so the hero can sit down on
a fisherman's bench while he is on it (the city's way is `ULyudCityThings::ClaimThing` through a slot's `"thing"`); the
ornithologist of `quest/ornithologist` has no duty at the hide; nobody walks the cliff walk or the sea trail yet.

## The first walk (backlog item 38)

The town trail is the game's natural first trip, as the quest **«Дорога в город»** (`town_road`, side). It begins with
`explore` when the awakening ends at the Aton station, and its seven steps are the places on the way, each one a journal
line in the hero's voice: `lookout` (reach `aton_belvedere`, where the view can be read as pages, item 39), `descent`
(`aton_descent`), `bridge` (`brook_bridge`), `stele` (`south_stele`), `terrace` (`town_gate`, `no_skip`: riding the cabin
into town first never finishes the walk), `square` (`square`), `bread` (`get bread_*`, the market's bakers' row is marked;
any bakery's bread counts). Out-of-order steps skip ahead as everywhere (straight down the trail skips the lookout).
While it runs the club's regulars say a word about it once (`aside.town_road.*`: the owner, Timur, Olya, who asks about
the stele and hears back after), and Aslan at his tent remarks on the hero coming back hungry (`quest_scenes.json`
`aslan_town_road`). Words in `Localization/quests/`; dev jumps land on the previous step (`Dev/teleports/town_road.json`:
the alley's end, the square, the market).

**The map** (`map.json`): the places' `icon`s, and `trails` (`south.map_trails()`: each designed trail every ~6 m with its
name `south.trail.<id>.name`). `ULyudQuests` inks a trail point once the hero passes within 6 m (`Walked`, saved);
`SLyudMapView` draws the walked stretches over the picture's dots (a brown pencil line in the diary) and writes a trail's
name beside its middle once he has set foot on it and the map is close. Dev action `south.trails_walked` inks them all or
clears them.

**The finger-posts**: a plank's distance is walked along the trail network (`south.trail_walk`, `sign_rows`) where the post
stands on a trail, in metres and minutes at 4.5 km/h (`350 м · 5 мин`, keys `south.sign.trail.dist.<m>m_<n>min`, added by
`tools/south_signs.py --strings`); posts off the trails (the boardwalk's end, the forecourt) keep their written words. The
plank textures and the posts' GLB are rebuilt on the Mac (`python3 tools/south_signs.py`, then blender/south and the import:
LFS).

**Offline**: `python3 tools/town_road_walk.py` walks the hero metre by metre through the quest engine with every place's
radius and the trail ink (the plain walk, straight down without the lookout, by cabin first), and checks the post at the
alley's end says the quest's five minutes.

## After a change here

`python3 tools/south.py --check && python3 tools/south.py`, `python3 tools/transport.py` (the pylons are resolved
against the ground), `python3 tools/ui_map.py` and `tools/ui_map_ink.py` (restore `textures/ui/binoculars_mask.png`
if the cloud rewrote it), then `python3 tools/town/south_people.py` (the townsfolk's places follow the things and trails), `python3 tools/south_uses.py`, `python3 tools/town_road_walk.py` and the level's build
(`unreal/scripts/build_scene01.sh`).

## Not done yet

See [south_backlog.md](south_backlog.md).
