# The diner «Чайка» by the lower station

A 1940s American lunch car across the street from the cable car's lower station (Академгородок), where the hero can
pour himself a coffee and order food over the counter, then eat it bite by bite. Its waitresses are three young women of
the town who work shifts, go home at night and lock up.

## The prototype

**The Modern Diner, Pawtucket, Rhode Island**: a Sterling Streamliner built by the J. B. Judkins Co. of Merrimac, MA in
1940 (designed by Roland Stickney); the first diner on the US National Register of Historic Places (1978). It is a
red-and-cream porcelain-enamel car with stainless bands, shaped like a streamlined locomotive with a rounded "bullet" end
and the door in the middle of its long street side. Inside there is a mahogany-trimmed counter with stools (eleven), booths
along the windows (seven four-seat and two two-seat ones), and the back bar with the coffee urns and the grill.

Ours keeps that type at game size. It has 11 stools, six four-seat booths along the street windows, a curved bench with two
round tables in the nose, a self-service coffee table at the west end and the back bar with two urns, a grill under its
hood, a pie case and the menu board. A brick kitchen annex stands behind the west half; it is the staff's way in. The
name «Чайка» (Chaika, the seagull) is ours.

## Where

`tools/world_map.py` `DINER_LOCAL = (124, 34)` is in the Academgorodok ensemble's frame and on its grid (`diner()`,
`diner_to_map()`). That puts the car about 30 m from the lower station (map ≈ -291, 507), on the side away from the rope,
with its street front facing the station. The cable car's view line onto the ensemble stays clear. The ground there is the
town's level ground, falling about 0.3 m over the car. The floor stands 0.7 m over the highest ground under the car (floor
map Z -35.14). The steps at the street door and the back door reach the real ground (`terrain.ground_z`). Trees are kept
off by the ensemble's own tree-free ramp. The walk grid knows the car and its kitchen (`tools/animal_map.py`
`ACADEM_HOUSES` "Diner", "DinerKitchen"). Cats wait by its street steps and at the bins behind the kitchen.

## The building: `blender/diner/building.py`

The car is our own clean game mesh, not a generated one, with walls of real thickness, exact openings and our UVs:

```
Blender -b -P blender/diner/building.py     -> design/diner/game/SM_Diner_{Shell,Inside,Inside_Glass,Glass}.glb
                                               unreal/Lyudmilovsk/Content/Diner/layout.json
Blender -b -P blender/diner/tableware.py    -> SM_Diner_{Plate,Bowl,Mug,TeaGlass,...,Fork,Spoon,Load_*} and the
                                               burger's bite stages SM_Food_diner_burger_<0..5> (-- BurgerBites)
Blender -b -P blender/diner/pack.py         -> design/diner/game/diner_kit.glb (one node per mesh)
Blender -b -P blender/diner/preview.py      -> design/diner/previews/building_<view>.png
Blender -b -P blender/transport/lint_mesh.py -- design/diner/game/SM_Diner_*.glb     (faces on faces: must be 0)
```

- **Frame**: metres; origin on the floor at the middle of the street front's axis; x along the car (+x the nose), y across
  (-y the street front), z up from the floor. In Unreal the mesh's y is -Y; `ALyudDiner::ToWorld` does the conversion.
- **Materials**: slots `DN_*` (`diner_common.py` `MAT_SPEC`: colour, roughness, metallic, emissive). Pictures use
  `DN_Sign_<name>`.
- **Walls**: each wall is a grid of cells between the openings and the bands (red enamel under the sills, stainless piers,
  cream band, mahogany inside). Every window has reveals and a pane (`_Glass`). The street door, the kitchen door and the
  back door are ALyudDoor holes of 122 x 222 cm (clear 110 x 215, `CharacterStandard`).
- **Roof**: a barrel roof, with a half dome over the nose and a gable at the west end. A stainless lip runs round the eave,
  and the roof sign stands on two legs.
- **Inside**: the counter (Formica top, stainless front, foot rail, napkin holders, a register), the stools, the booths with
  jukebox selectors, the nose bench, the back bar (cupboards, the grill and hood, the urns, the pie case, mugs on the shelf),
  the coffee table (an urn with a tap and a drip tray, a tray of mugs, a sugar pourer), opal globe lamps, and the kitchen
  (range and hood, sink, fridge, shelves, a worktable).
- **Lint**: the building's meshes and every tableware mesh lint at 0 faces on faces. Signs, cupboard doors and flutes stand
  ≥ 12 mm off their faces.

## Unreal: `unreal/scripts/import_diner.py`

- `import` brings the kit to `/Game/Diner/Meshes` and the burger's stages to `/Game/Food`. It makes the flat materials
  `MI_DN_*`, clear glass, and the sign material `M_DN_Sign` (`Image`, `Glow`), and imports the sign pictures
  `/Game/Diner/Localized/<lang>/T_Sign_<name>`.
- `place(face_y)` spawns the meshes (tagged `Diner`; the shell and insides also `Roof`, the insides drawn to 60 m; the
  stools are a mesh of their own without collision, so the hero stands right at the counter where a stool is),
  `ALyudDiner` and the three doors (the street and back ones tagged `DinerOuter`). `import_world.py` calls it, so
  `build_scene01.sh` keeps the diner.

## The game: `Source/Lyudmilovsk/{Public,Private}/Diner/LyudDiner.*`

**ALyudDiner** reads `Content/Diner/layout.json`.

- **Open** while one of its waitresses is on her `work_diner` shift. Then the lamps burn (point lights it makes from the
  layout, and the opal globes' emissive) and the street and back doors are unlocked.
- **Closing.** Once the last waitress has gone, the lamps go out and the doors close and lock. They never lock while the
  hero is inside.
- **Night.** At night the lamp over the door burns and the signs glow while the diner is open.
- **Signs** show the player's language.

**Ordering.** The menu is sold over the counter by whichever waitress is on shift (`counters.json`, `T` near her, the
usual trade screen). A bought dish is not handed over: the food thread's `ULyudFoodSubsystem` skips foods marked
`"plated"` in `Content/Food/food.json`. It is taken out of the pocket and served 5–9 s later on the counter, at the
free place nearest the hero, with a toast.

**ALyudDinerDish** is a plate, a bowl on a plate, a mug, or a tea glass in its holder, with its food or drink. When the
hero is close and looks at it, **LMB eats it**:

- **Fork and spoon** (blini, eggs and bacon, spaghetti, ukha, apple pie) use the shared bite mechanic,
  `ULyudBiteSubsystem::BeginMeal` (Public/Food/LyudBite.h). A click takes a small bite (two noodles on the fork), a hold
  a big bite (a twirl, `FLyudMeal::LoadMeshBig`), and keeping it held goes on eating. The fingers close on the fork's
  handle. The food on the plate shrinks bite by bite.
- **Cups** (tea, kompot, coffee) work the same way: the cup comes to the lips, a click is a sip, a hold is a gulp, and the
  fill goes down. Between sips the cup goes back to its place.
- **The burger** is picked up into the hand: the food thread's hand system, `ULyudFoodSubsystem::HoldFromPocket`, with
  bite-stage meshes `SM_Food_diner_burger_<k>`. Click, hold, keep holding; `Q` puts it away and the rest stays in the
  pocket.
- Eaten up, the empty dish stays 45 s, then it is cleared away.

**Self-service coffee.** Stand at the coffee table and look at the urn: `E — налить кофе (8 ₽)`. The hand takes a mug from
the tray, sets it under the tap and presses the lever while the coffee runs (a stream, the mug filling). The mug then
waits on the drip tray to be drunk.

**Console**:

| Command | What it does |
|---|---|
| `lyud.Diner.State` | Prints the diner's state. |
| `lyud.Diner.Open 1\|0\|-1` | Forces the diner open or closed; -1 goes back to the staff's hours. |
| `lyud.Diner.Serve <item>` | Serves that dish. |
| `lyud.Diner.Coffee` | Pours a coffee. |
| `lyud.Diner.Eat` | Eats the nearest dish. |
| `lyud.Bite.Take small\|big` | Takes one bite. |
| `lyud.Hand.Press 1\|0` | Holds or releases the bite button for the hand-held burger. |

## The staff and the customers

The staff are three residents (`people.json`). They have no bodies of their own yet and are shown by existing models
(`mesh`).

| Key | Name | Age | Shift | Shown by | Lives |
|---|---|---|---|---|---|
| `zhukova_lena` | Лена Жукова | 22 | 6:45–15:00, Mon–Fri | Crowd_12 | own flat |
| `frolova_masha` | Маша Фролова | 24 | 14:30–22:00, Mon–Fri | Streamer | shares a flat with Рина |
| `hayashi_rina` | Рина Хаяси | 23 | 8:00–21:00, Sat–Sun | Girl_b | shares a flat with Маша |

- `activities.json` `work_diner` (slots `counter`, `grill`). Townspeople also eat there (`eat_diner`, 8–21, the booth
  seats).
- `people_map.py` `build_diner()`: the place `diner`. It is entered by the street door up the steps; the staff's spots
  are behind the counter and at the grill, the customers' are the 24 booth seats. Its paths come from the car's own plan.

## Items, words, pictures

- **Items** (`LyudInventory.cpp`): `diner_pancakes`, `diner_eggs`, `diner_spaghetti`, `diner_soup`, `diner_pie`,
  `diner_burger`, `diner_tea`, `diner_kompot`, `diner_coffee`, with names and descriptions in ru/en/ja.
- **Icons**: one Higgsfield 3x3 grid (`design/diner/icons_grid.json`, 1 credit), cut by `tools/diner_assets.py`. The
  diary sketches come from `tools/ui_diary_icons.py`.
- **Signs**: `tools/make_diner_signs.py`, local fonts, no paid API. They are the name on the cream band, the band's words,
  the roof sign, the menu board (names and prices from `counters.json`) and the coffee card.

## Checks

`lyud.Diner 0|1` hides the whole diner and its lamps. Use it for the one-process benchmark A/B:
`tools/bench_systems.sh nodiner`.

`unreal/scripts/test_diner.sh` runs offscreen; `DINER_STEPS=outside,inside,eat,coffee,burger,night`, `DINER_LANG`. It
prints DINERTEST lines and writes shots to `Saved/DinerTest`.

First runs (2026-10-31): the opening and locking, the lamps, serving, fork and spoon bites, the coffee pour and the cup, and
the burger in the hand all ran. The shots are in `shots/` of the worktree (not committed).

## Open

- The fork's grip at the lips could look more natural.
- The waitresses' own bodies (paid hi3d generation: ask Dima).
- Sitting down on a stool or in a booth: the hero stands at the counter.
- The coffee pour sound (`S_diner_pour_01`) and the plate's clink are looked up but not generated yet; bite sounds are
  the food thread's.
- The benchmark A/B ran under heavy swap and is not usable yet: rerun `tools/bench_systems.sh nodiner` alone.
