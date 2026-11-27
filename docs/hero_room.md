# The hero's own room in the club

Backlog item 19 (`docs/interactives_backlog.md`). A small room of his own behind a doorway in the club hall's left wall,
opposite the owner's booth: a bed, a desk with a record player, a pin board, nails, shelves, a flower pot, Maru's basket by
the door. What he owns can be put there by E and is saved.

## Where it is

World origin is the club's junction; the hall's left wall is the plane Unreal X = -300..-350, Y 0..750. The room's frame
(the town kit's building frame: x along the front, y into the room) stands at Unreal (-300, 100, 0) with its front to +X,
so the room is X -690..-300, Y -10..480, 2.6 m high, and the doorway is Y 40..160, 2.2 m high. The upland's ground is
6.0-6.6 m above the floor there (`tools/terrain.py`), outside the landscape's hole for the club: nothing shows outside.

## How it is built

| Part | Where |
| --- | --- |
| The room and its furniture | `blender/city/buildings/hero_room.py`, a "building" of the town's kit (`docs/city_buildings.md`), built by `blender/city/build_city.py -- hero_room` into `blender/export/city/hero_room.glb` + `hero_room.json` |
| Import | `TYPES=hero_room STEPS=import unreal/scripts/import_city.sh` -> `/Game/City/Buildings/hero_room/SM_hero_room`, `SM_hero_room_Detail` (like every town building: the glb and the uassets are not in git, each machine builds and imports them) |
| Its place and its things | `tools/city/extra_places.json` (plot `P-HOME`: a room on no plot of the town plan), read by `tools/city/interiors.py`; acts and rules `tools/city/content/x_home.json`, words `tools/city/content/strings/x_home.json` |
| The room's own data | `tools/home/home.py` -> `Content/Home/home.json` (origin, meshes, the wall and the doorway, lamps, places, trophies, plants); `--check` compares |
| Rules | `Private/Home/HomeModel.h` (plain C++), checked by `tools/homesim/run.sh` on the real `home.json` |
| In the engine | `ULyudHeroRoom` (`Public/Home/LyudHeroRoom.h`), a world subsystem |

After changing `hero_room.py`: build the glb, then `python3 tools/city/interiors.py`, `python3 tools/home/home.py`, and the
import on the Mac.

The room is not part of `Lvl_Scene01`: the subsystem spawns its two meshes at play (the shell blocks, the detail does
not), lights its two lamps, and cuts the doorway. The hall's left wall is three meshes of the club's scene (oil paint below,
a dark line, whitewash above); they are found by their bounds, hidden, and their hall face is drawn again round the doorway
with their own materials and the scene's mapping (1 m a repeat), with three collision boxes. A machine that has not run
the import gets a log line and an untouched hall. If the club's scene is one day rebuilt with the doorway in it
(`blender/scripts/build_scene01_corridor.py`), no solid wall is found and nothing is cut.

## What he does there

Everything is a thing of the town (`ULyudCityThings`, `docs/city_interiors.md`), so prompts, hours and E work as anywhere:

- **The bed**: lie down, then sleep until an hour he picks (the act `myroom_sleep`, the same chooser as a hotel bed, any hour,
  free).
- **The chair, the clock, the coast map, Maru's basket**: sit, the time, a few lines.
- **The record player**: his own records (`ULyudMusic`, `docs/music.md`).
- **The switch**: the lamps on and off.
- **Places for his things**: 6 cards on the pin board (`pin_1..6`), 3 nails (`frame_1..3`), 8 spots on the shelves
  (`shelf_1..6`, `top_1..2`). E opens a one-line chooser on the prompt line: A/D turn the choice, E takes it, S shuts it.
  The choices are "take X back" when something stands there, then everything he has that fits:
  - a card: a small flat item (6-22 cm: postcards, tickets, prints when the camera gives them);
  - a nail: a bigger flat item (14-36 cm: the sanatorium diploma, a record's sleeve, the newspaper);
  - a shelf: anything up to 36 cm high and 40 wide, and the trophies: the cups of the clubs' tournaments (stamps
    `cups/cog_chess`, `cups/san_dominoes`, `cups/tt_cup`) and the bird diary. Trophies are not items: they are offered
    while he has the stamp, and each stands in one place.
  The item leaves the pocket and stands there as the hand would hold it (`ULyudInventoryUI::SpawnHeld`, so its icon is its
  picture); taking it back returns it.
- **The flower pot**: plant a cutting of one of six house plants (the procedural plant system's indoor species), water it
  once a day, look at it, pull it out. It grows one day a day and one more for every day it was watered; its age for the
  generator is `0.12 + 0.05 * days`, set when the day changes, never per frame.

While the chooser is up it holds the inventory's "external screen" (nothing else opens, the town's things offer nothing)
and the move input; the menu or losing the hero shuts it.

## Save

`ULyudHeroRoom` is saved as "HeroRoom": what stands where (`p:<place>`), the plant (species, day planted, last watering,
days watered), the lamp. A place that a later version of the room no longer has hands its item back to the pocket on load.

## Developer mode and the console

Teleport `city.p_home` (group interiors). Actions, group "home": furnish for a look (`home.fill`), clear (`home.clear`),
grow the plant ten days (`home.plant_grow`), the lamp (`home.light`). Console: `lyud.Home.Report`, `lyud.Home.Fill`,
`lyud.Home.Clear`, `lyud.Home.Grow <days>`, `lyud.Home.Light`, `lyud.Home.Use <place> [next|prev|ok|cancel ...]`.

## Known limits

- A record put on a shelf is out of the pocket, so the player does not offer it until it is taken back.
- Camera prints (item 2) and the friendship with Maru (item 1) do not exist yet: the cards take any small flat item, the
  basket is furniture with a few lines.
- The doorway has no door leaf.
