# The North (Север)

Volcano country beyond the north hills: a long walk from the town (2.3 km from the Sun Square to the campsite, 3 km from
the club). Fumaroles, summer snowfields, grass meadows, a young cinder cone with a lava flow, a campsite with tents and
a ranger's hut at the reserve's gate. Almost no trees: stone birch park woodland at the tree line, then shrubs.
Everything here is a **proposal**, not canon (docs/story/README.md): names, the ranger, the boards' words.

The map grew from Y 2200 to Y 4200 for it (`tools/world_map.py` `Y_MAX`; the landscape is 21 x 41 components now).

## Places

Map metres (docs/world_map.md's frame: X west toward the sea, Y north). All of it is authored in `tools/north.py`.

| Place | Where (X, Y) | What |
|---|---|---|
| Gate | -205, 2395 | a barrier across the track from the town, the reserve's board |
| Ranger's hut (сторожка) | -178, 2440 | a small log hut with a stove, a bunk, a table and a radio; the ranger lives here |
| Tree line | Y 2330..2700 | stone birch, rowan and dwarf birch wide apart (`FOREST_MIX["stonebirch"]`) |
| Meadows (луга) | Y 2550..3400 | a rolling plateau of grass 35..45 m over the club's floor; dwarf birch and juniper clumps; ends at the sea in an ~80 m cliff |
| Campsite (кемпинг) | 140, 3160 | six tents round a fire ring, a table under a tarp, log benches, a food pole, an outhouse |
| Hot spring | -372, 3398 | a warm pool with a wooden deck at the mouth of the Fumarole Valley |
| Fumarole Valley | -430, 3490 | a trough through the Ridge: bleached clay, 9 steam vents, 12 patches of steaming ground, 5 mud pots |
| The Ridge (Гряда) | Y ~3520, X -960..260 | a low rise whose north slope keeps snowfields all summer |
| Snowfields (снежники) | 8 of them, Y 3345..4030 | firn in hollows: on the Ridge's north slope, under the Cone, in the gullies of the northern foot |
| Lava flow | from the Cone west to 330, 3668 | a tongue of blocky clinker 50..100 m wide, 4.5 m thick |
| The Cone (Сопка) | -640, 3840 | a cinder cone 58 m high, 410 m across, its crater (21 m deep) breached to the west: the way in; two vents on the crater floor |
| Northern and eastern foot | Y > 3950, X < -880 | the land climbs out of the map toward the big volcanoes |
| Far volcanoes | 5..10 km out | three backdrop cones with snow caps, one smoking (`north.far_volcanoes()`); never walked on |

The trail (a dirt track, `north.TRACKS`, routed by `tools/terrain.py`): the town's north track -> the gate -> the camp ->
the spring -> up the Fumarole Valley -> the Cone's breach.

## Who owns what

| What | Where |
|---|---|
| The region: macro shape, masks, places, camp layout, fumarole list, signs, forests, grass, trail anchors | `tools/north.py` (hooked from `world_map.plateau / keep_flat / forests / grass_patches / _tree_free`, `terrain._anchors / TRACKS`) |
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

**Words.** Boards carry words: follow `.claude/skills/localization/SKILL.md` (ru/en/ja, text keys `north.sign.*`,
`place.north.*`, `dev.tp.north_*`).

**Fumaroles.** `north.json` `fumaroles`: `{id, kind: vent | steam | mudpot | spring, x, y, size}`. Ids are stable.

## Performance

Follow the perf checklist (project memory): steam, fire and snow never rebuild instanced meshes per frame, moving
meshes stay out of Lumen, nothing ticks when the hero is far (the North is > 1 km from the town: everything there sleeps
until the hero is within a few hundred metres).

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
the dirt track, `terrain._route` woods_north → north_gate, ~630 m) to the gate, and inside the North straight over the
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
