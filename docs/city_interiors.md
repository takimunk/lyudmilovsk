# Town interiors: things to do inside the buildings

The town's buildings ([city_buildings.md](city_buildings.md)) have rooms the hero can walk into. This page is the
contract between the three parts that make those rooms alive: the furniture (Blender kit), the data (what each thing
is and does) and the game code (`ULyudCityThings`). The work list is [city_backlog.md](city_backlog.md).

## 1. Furniture and things (Blender)

* `blender/city/interior_kit.py`: the shared furniture kit for all building modules (chairs, tables, shelves with
  books, counters, beds, lamps, rugs, posters, plants, kitchen and shop fittings, pianos, arcade cabinets, gym
  gear...). Building modules call it instead of drawing boxes by hand. Old helpers (`_furn1.py`, `furn_int2.py`,
  `furn_int3.py`) stay for the code that uses them.
* `CT_Atlas`: one material slot with a picture atlas (`textures/city/atlas_D.jpg`, drawn by `tools/city/atlas.py`
  with Pillow, no paid generation): book spines, posters, paintings, signs, labels, rugs, screens, clock faces, notice
  boards, menus. The kit maps UVs into named atlas cells (`interior_kit.ATLAS`).
* Collision: furniture goes to `B.detail` (no collision) by default. Anything the hero or a townsperson would walk
  through and that is taller than 0.5 m and wider than 0.4 m (counters, shelves, tables, beds, pianos) ALSO gets one
  simple box in the shell (`B.m`), slightly smaller than the visible object, so nobody walks through it. Chairs, stools,
  plants and small props never block.
* Budget per building: interior furniture adds at most ~25k triangles to `detail`; lamps stay within the number the
  type already has plus 6 (the game shows at most 24 at once, `lyud.City.LampBudget`).
* A thing the hero can use is declared in the building module with `B.thing(id, kind, pos, yaw, **props)`
  (`citykit.Building.thing`), exported as `"things"` in `blender/export/city/<type>.json`. `pos` is the point the
  hero looks at (the middle of the object, building frame, metres), `yaw` the way the object faces (kit convention:
  0 = toward the street, -y; 90 = +x), ids are unique inside the building and stable (`hall_piano`, `desk_2`).
* Preview without Unreal: `python3 blender/city/shoot_interior.py -- <type> --auto 4 --top 0,2.6` (Cycles, about 40 s
  a shot on 4 cores; look at every shot before calling a room done: floating furniture, things in walls, stretched
  UVs, z-fighting, doors blocked).

### Thing kinds

| kind | What it is | Props |
|---|---|---|
| `seat` | a chair, bench, sofa, stool the hero can sit on | `height` (seat surface over the floor, m) |
| `bed` | a bed or couch to lie down on (rest, pass time) | `height` |
| `shelf` | a bookshelf, magazine rack, record crate: browse and read | `topic` (free word: `birds`, `sea`, `science`, `poetry`, `kids`, `local`, `music`...) |
| `board` | a notice board, timetable, poster wall, exhibit plaque, painting: read or look | `topic` |
| `counter` | where a member of staff serves (shop, ticket office, reception, bar, cloakroom) | `role` (the staff role standing there), `service` (free word: `shop`, `tickets`, `reception`, `bar`, `lending`, `cloakroom`) |
| `machine` | arcade cabinet, jukebox, weighing scale, photo booth, telescope, payphone, water fountain, vending | `what` |
| `play` | piano, chess table, table tennis, billiards, darts, table football | `what` |
| `sport` | barbell, wall bars, punching bag, treadmill, pool lane, basketball hoop | `what` |
| `craft` | easel, potter's wheel, workbench, sewing machine, darkroom, school desk/blackboard | `what` |
| `screen` | cinema screen, stage, TV, aquarium, slide projector: watch for a while | `what` |
| `take` | something lying about that may be picked up once a day (a free newspaper, a leaflet, a cup of water) | `item` |
| `door` | a storey link or inner door that only needs a caption (roof, staff only) | `to` |

`pos`/`yaw` say where the object is; where the hero ends up (sitting, standing in front) is worked out by the game.

## 2. Data

`tools/city/interiors.py` reads the plan (`Content/City/city_plan.json`), every building export and the content
tables in `tools/city/content/*.json`, and writes:

* `Content/City/interiors.json`: every thing of every plot in world terms, joined with what it does:

```json
{"things": [
  {"id": "P-LIBRARY/shelf_birds", "plot": "P-LIBRARY", "kind": "shelf", "ue": [x, y, z], "yaw": 12.5,
   "props": {"topic": "birds"}, "act": "read_birds", "hours": [10, 20], "days": "tue-sun"}
 ],
 "acts": {
  "read_birds": {"prompt": "city.act.read.prompt", "minutes": 20, "price": 0,
                 "lines": ["city.read.birds.1", "city.read.birds.2", "city.read.birds.3"],
                 "pose": "stand", "sound": "page_turn", "event": "read", "target": "birds",
                 "give": [], "daily": 0, "needs_staff": "", "needs_item": "", "fade": false}
 }}
```

  `ue` = Unreal cm, `yaw` = Unreal degrees the object faces. `act` names an entry of `acts`; a thing without content
  gets the default act of its kind (`sit`, `look`...), so every declared thing does something.
* `Content/Dev/teleports/interiors.json`: one teleport per enterable room for the developer mode
  (`{"teleports": [{"id", "group": "interiors", "name", "x", "y", "z", "yaw"}]}`, x/y map metres, z world cm).
* strings: `city.*` keys in `Content/Localization/city/strings_{ru,en,ja}.json` (a sub-table, merged at load like
  `Localization/town/`), written from `tools/city/content/strings/*.json`.

### Acts

An act is what happens when the hero presses E at a thing:

| field | meaning |
|---|---|
| `prompt` | key of the prompt line, shown as "E — ..." |
| `lines` | keys of texts; one is shown each time, in order, wrapping round (a shelf has several pages; a board several notices) |
| `minutes` | game minutes that pass (0: none). With `fade` the screen fades out and in |
| `price` | roubles taken (the act is refused with `city.act.no_money` if the hero has less) |
| `give` | items put in the pocket: `[["item_id", n]]` |
| `needs_item` | an item the hero must have (a library card, a ticket) |
| `needs_staff` | a staff role that must be on shift at this place (the cashier for a cinema ticket) |
| `pose` | `stand`, `sit`, `lie`, or an activity of activities.json for the hero's body |
| `sound` | a one-shot of the soundscape |
| `event`/`target` | reported to the quests with `ULyudQuests::Notify` (default event `do`, target = the act id) |
| `daily` | how many times a day it gives its reward (0: no limit) |
| `shop` | opens the trade screen with this stock (`shops` table: goods with prices) when staff is on shift |
| `screen` | a full-screen picture or page to show while it lasts (film stills, an exhibit) |

Open hours and days come from the plot's staff in the plan unless the content gives its own.

## 3. Game

* `ULyudCityThings` (world subsystem, `Public/City/LyudCityThings.h`): reads `interiors.json`, finds the thing the hero
  looks at within reach (checked a few times a second, never per frame for all things: a grid by position), shows the
  prompt through `ULyudInventoryUI::SetPrompt`, runs the act on E. Remembers per-day counts and the page each shelf is
  on in the save. Nothing is spawned per thing: the furniture is part of the building mesh.
* Pure logic (which thing, may it run now, what it gives) lives in `Private/City/CityThingsModel.h` without engine
  types, tested in the cloud by `tools/citysim/run.sh` like the other sims.
* Console: `lyud.City.Things` (count, the nearest five), `lyud.City.Do <thing id>` (run it from anywhere),
  `lyud.City.Things.Show 1` (debug labels). Developer mode actions are registered when `Dev/LyudDev.h` exists.
* Perf ([perf checklist](../README.md)): no per-frame allocation, no actors or components per thing, no ticking when
  the hero is outside the town box.
