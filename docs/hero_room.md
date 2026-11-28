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
- **The record player**: his own records (`ULyudMusic`, `docs/music.md`): the ones in his pocket and the ones standing
  on the room's shelves and nails (`ULyudHeroRoom::Holds`). Only this player counts the room's records; a player
  elsewhere plays what he carries.
- **The hot plate**: a small table by the right wall with a one-ring plate, a cup and plates. The thing `hot_plate` has
  `what=hot_plate`, so the kitchen's own rule gives it the act `hot_plate` and E opens the cooking screen
  (`docs/kitchen.md`). No code of the room is involved.
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
- **His own photographs**: a card or a nail also takes a print of the album (`docs/photo.md`). The chooser offers the
  eight newest prints that hang nowhere yet. A print is no item: its id is `print:<roll>:<frame>`, the album keeps the
  picture and the wall shows it (15 x 10 cm pinned to the board, 30 x 20 on a nail). One print hangs in one place.
  Taking it down just frees the place. The picture is read from the game's photo folder once and kept
  (`ULyudPhoto::LoadPrint`); a print whose file is gone shows as a white card.
- **The flower pot**: plant a cutting of one of six house plants (the procedural plant system's indoor species), water it
  once a day, look at it, pull it out. It grows one day a day and one more for every day it was watered; its age for the
  generator is `0.12 + 0.05 * days`, set when the day changes, never per frame.
- **Thirst**: a plant left without water hangs its leaves: after 5 dry days, an aloe after 14, a cactus after 21
  (`FModel::Thirst`, `Droops`). "Look" then says how long it has been dry, and one watering sets it right (its own
  line, the pour sound `rest_pour`). It never dies. The plant actor gets `SetWithered`, which changes the look only for
  species that have a withered state.
- **The owner looks in**: when the plant has hung for two more days and the hero is out of the room, the owner waters
  it once (`FModel::Mind`, at most once a day). It gives no extra growth. The next time the hero walks in, a note on
  the prompt line says so (three notes in turn, `city.myroom.pot.minded.*`). Quests hear `do myroom_minded`; a watering
  of his own is `do myroom_water`.

## Weather and season

The room has no window, so the weather comes in as words. The bed, Maru's basket and the coast map have extra lines by
weather (rain, snow, fog, cold, warm) and by date (summer nights, the New Year lights in the hall, the winter bay). They
are line conditions of the acts in `tools/city/content/x_home.json`, strings `city.myroom.bed.<weather>.*`,
`city.myroom.basket.<weather>.*`, `city.myroom.map.<weather>.*`.

While the chooser is up it holds the inventory's "external screen" (nothing else opens, the town's things offer nothing)
and the move input; the menu or losing the hero shuts it.

## Save

`ULyudHeroRoom` is saved as "HeroRoom": what stands where (`p:<place>`), the plant (species, day planted, last watering,
days watered, times the owner watered it), the lamp. A print is saved by its id like any other thing. A place that a later version of the room no longer has hands its item back to the pocket on load.

## Developer mode and the console

Teleport `city.p_home` (group interiors). Actions, group "home": furnish for a look (`home.fill`), clear (`home.clear`),
grow the plant ten days (`home.plant_grow`), leave it dry for eight days (`home.plant_dry`: the leaves hang; step out of the
room and back in for the owner's note), the lamp (`home.light`). `home.fill` also hangs up to four album prints when the
album has any (developer action `photo.develop` first). Console: `lyud.Home.Report`, `lyud.Home.Fill`,
`lyud.Home.Clear`, `lyud.Home.Grow <days>`, `lyud.Home.Dry <days>`, `lyud.Home.Light`, `lyud.Home.Use <place> [next|prev|ok|cancel ...]`.

## Known limits

- Maru does not come to the basket herself: it is furniture with lines by hour and weather.
- Nobody but the owner's note visits the room: no resident walks in.
- Nothing in the room moves with the time of day (no cup left after breakfast, no wet coat): the room is spawned by
  `ULyudHeroRoom`, outside the town's dressing, and such pieces need new meshes and a Mac to look at them.
- A print on the wall is a plain card with the photograph: no frame mesh, no paper border.
- Owed on the Mac (written in the cloud, never compiled): the first compile of the prints, the room's records on the
  player, thirst and the owner's note; `TYPES=hero_room STEPS=import unreal/scripts/import_city.sh` for the hot plate
  table; shots of a furnished room with prints; a check that the photograph is the right way up on the card.
- The doorway has no door leaf.
