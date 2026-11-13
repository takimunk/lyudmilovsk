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
* Windows from inside: `B.window` also puts a room-side card facing in (`CT_WindowIn`: a bright daylight pane by day,
  dark blue at night, after the sun; it lights nothing, so keep lighting rooms with `B.lamp`). Nothing to do in a
  module: every window gets one (`B.inside_windows`, [city_buildings.md](city_buildings.md#windows-and-lights)). Do not
  draw your own glowing panes behind windows; leave the wall's inner face open at the window so the card shows.

### Promenade units

The 32 shop units of the seawall W1 (`promenade_units` in the plan, W1-U01..32) are rooms behind the wall's face:
`blender/city/buildings/promenade_unit.py`, one type with 12 `VARIANTS` (cafe, fish, rental, workshop, dumplings,
souvenirs, bakery, photo, barber, books, beer, pasty -> `promenade_unit__<variant>`), the variant of a unit by its name in
one place: `tools/city_plan.py` `UNIT_VARIANTS` (written into the plan as `variant`, with `front`/`front_ue`: the bay's
middle on the face at the promenade's level, the room's origin; `door` stays the point 0.6 m before the doorway). Its
staff come from the same file (`UNIT_STAFF_BY_NAME`, equal to town_life's table), so a counter's role is someone who
really works there.

* Levels: T1 is only 3.3 m over the promenade. The room's floor is the promenade (+2 cm), its ceiling 3.0 m, slab to
  3.25 under the block's top at 3.32; inside 9.7 x 6.0 m (y 0.40..6.40 behind the face; the face's reveals are 0.28 deep).
  For the room to fit, W1's landscape stays level with the promenade under it and steps up to T1 at 7.9..8.3 m behind the
  face (`city_plan.W1_RAMP_START`, `W1_RAMP`), and W1's block is 9.5 m deep (`W1_DEPTH`) so the step stays hidden.
* `tools/city_mesh.py` leaves a unit's doorway open (no door panel, a concrete threshold, the plinth cut) and keeps the
  shop window's glass (`W1-Uxx-w1`, lit by `ALyudCityWindows`); from inside, the window hole looks out through it.
* `unreal/scripts/import_city.py place` puts an `ALyudCityBuilding` per unit (`place_units`: at `front`, yaw like a plot,
  tags City/NoGrass/NoTree/Roof, lamp hours from the unit's staff, no door actor).
* Each dressing declares its things with stable ids, the same in every unit of a trade (`cafe_bar`, `fish_counter`,
  `barber_chair_1`, `shelf_birds`, `photo_booth`, `photo_stool`, `darts`...); rules may name them `promenade_unit/<id>`.
  Spots: `staff_<role>[_n]` (kind `work`) where each role stands, `counter` where a customer is served, `stand`/`seat`
  for visitors; `tools/town/town_life.py` (`unit_room`) makes a built unit a walked interior: staff at their spots, in
  by the doorway (`path`: 1.2 m inside).
* One lit pendant per room (`B.lamp`), the other fixtures unlit; a sign over the window outside where the atlas has
  the trade's word (`sign_buffet`, `sign_canteen`, `sign_bread`, `sign_books`, `sign_barber`, `sign_photo`).
* Preview: `python3 blender/city/shoot_interior.py -- promenade_unit:<variant> --cam 4.4,0.8,1.65,232,-12 --lens 15`
  (from inside the doorway across the room).

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

### Using the kit

* `import interior_kit as kit`; the catalogue (every piece, its size, options, budget) is the docstring at the top of
  the file, `kit.PIECES` maps names to functions. Every piece is `kit.<name>(B, x, y, z=0.0, yaw=0.0, **options)`:
  `(x, y)` the centre of its footprint on the floor `z`, `yaw` the way its front faces (0 = -y, 90 = +x).
* Each returns a dict: `top` (height of its working surface, world z), `seats` (`[(x, y, z, yaw)]` where someone
  sits; `seat`, one world point, on pieces used standing or from a stool), `foot` (footprint polygon), `size`, `height`, `tris`, `solid`, plus piece extras
  (`screen`, `lamps`...). Use them for `B.thing(...)`, `B.spot(...)` and for stacking (`kit.cup(B, x, y, t["top"])`).
* `solid=True` (the default on counters, shelves, tables, beds, pianos) adds the hidden collision core to `B.m`.
  Lamps: only `pendant`, `lamp_green`, `floor_lamp`, `table_lamp` with `light=True` call `B.lamp`.
* Layout: `kit.row(fn, B, x0, y0, x1, y1, n, yaw)`, `kit.grid(...)`, `kit.around_table(B, x, y, w, d, yaw,
  per_side=(n_front, n_back, n_left, n_right))`, and `kit.Room(B, (x0, y0, x1, y1))` with `keep_door`, `keep`,
  `place`, `along("n", [...])`, `scatter` (seeded, nothing overlaps). Call `kit.check(B)` after furnishing: it lists
  overlapping floor pieces, pieces outside the rooms and blocked doors.
* Round-2 pieces (transport, services, shop goods, studio, sport, canteen: `bicycle`, `pram`, `mailboxes`,
  `barber_chair`, `teller_grille`, `serving_line`, `palm`, `parquet`, `court_lines`... full list in the catalogue
  docstring) follow the same rules. Hanging pieces (`model_plane`, `bird_feeder(kind="hang")`) take `top_z` like
  `punching_bag`; `[flat]` pieces (`parquet`, `court_lines`) lie on the floor z you pass. `done(footprint=(x0, x1, y0, y1))`
  reports a footprint other than the mesh bounds (a plant's pot, not its leaves). `office_desk` returns `chair` with the
  yaw of the desk (the worker looks across it, toward -y at yaw 0).
* Showroom of everything: `python3 blender/city/kit_showroom.py -- [--no-render] [--close]` (lint numbers, then
  `renders/city/interior/kit_*.jpg`).
* Adding a piece: write `def name(B, x, y, z=0.0, yaw=0.0, ..., solid=...)` with `p = _P(B, x, y, z, yaw, "name")`,
  draw in local metres (front toward -y) with `p.box/bev/cyl/lathe/rod/slab/card_front...`, add `p.core(...)` for a
  collision box, end with `return p.done(solid=solid, top=..., seats=[(u, v, h, dyaw)])`; add it to `PIECES` and the catalogue, then
  run the showroom. `done()` lifts coplanar overlapping faces 4 mm apart (`geom.separate_coplanar`) and gives
  every non-atlas face metre UVs. New flat slots for the kit: `CT_Brass`, `CT_Lacquer`, `CT_Leather`, `CT_Baize`,
  `CT_Paper`, `CT_Rubber`, `CT_Leaf`.
* Atlas cells (`blender/city/atlas_cells.json`, `kit.cell("poster", rnd)` picks one of a group) are named
  `<group>_<variant>`: `spine_00..39`, `painting_<subject>`, `poster_*`, `cinema_*`, `travel_*`, `kids_*`, `map_*`,
  `chart_*`, `board_*`, `menu_*`, `price_*`, `notice_*`, `rug_*`, `tile_*`, `clock_*`, `screen_*`, `sleeve_*`, `mag_*`,
  `news_*`, `paper_*`, `label_00..59`, `sign_<shop>`, `fabric_*` and single cells (`keys_piano`, `chess_board`...).
  A new cell: a `@cell(name, w, h)` function in `tools/city/atlas.py`, run `python3 tools/city/atlas.py --sheet`
  (rewrites the jpg and the json; all UVs come from the json, so nothing else changes). Text on cells is Russian.

## 2. Data

`tools/city/interiors.py` (python3, stdlib) reads the plan (`Content/City/city_plan.json`), every building export
(`blender/export/city/<type>[__<plot>].json`), the content tables `tools/city/content/*.json` and the strings
`tools/city/content/strings/*.json`, and writes (deterministic; run it after any export or content change):

```
python3 tools/city/interiors.py            # write everything below
python3 tools/city/interiors.py --check    # write nothing; fail on broken content; per-plot summary table
```

* `Content/City/interiors.json`, one record per line:

```json
{"things": [
  {"id": "P-LIBRARY/stack_7_a", "plot": "P-LIBRARY", "kind": "shelf", "ue": [-42162.7, 99114.6, -2070.0], "yaw": 84.4,
   "props": {"topic": "birds"}, "act": "read_birds", "hours": [10, 20], "days": "tue-sun", "auto": 1}
 ],
 "acts": {
  "read_birds": {"prompt": "city.act.read.prompt", "minutes": 15, "price": 0, "lines": ["city.read.birds.1", "..."],
                 "pose": "stand", "sound": "page_turn", "event": "do", "target": "read_birds", "give": [], "daily": 0,
                 "needs_staff": "", "needs_item": "", "fade": false, "shop": "", "screen": "", "until": -1, "any_hour": false}
 },
 "shops": {"library_desk": {"title": "city.shop.library_desk.title", "role": "librarian",
                            "goods": [{"item": "bird_guide", "price": 60, "count": 1}]}}}
```

  `ue` = Unreal cm (the building's front point + the thing's local point, storey z included), `yaw` = Unreal degrees the
  object faces (plan yaw minus the kit yaw). `hours` `[open, close)` and `days` (`mon-sun`, `tue-sun`, or `mon,wed`) come
  from the staff of the act's `needs_staff` role, else from the plot's public staff (cleaners, janitors, guards don't
  open a building), else round the clock; a rule may give its own. Every thing has an act, every act every field.
* **Staff variants.** An act whose `needs_staff` role does not work at the plot gets a copy for the role that does: the
  counter's own `role` when the plot has it, else (a counter or a shop) the plot's first selling role, e.g.
  `buy_club_buffet@seller`; at a plot with no staff at all (the shops on a residential block's ground floor, a house
  canteen) `<act>@open` without staff, open 9-20 Mon-Sat unless a rule gives hours. The copy keeps the original
  `target`, so quests see it as the original act. Any other staff mismatch is an error.
* **Auto things.** While a building declares no `things`, its `spots` become things flagged `"auto": 1`: `seat`/`bench`/
  `table` → seat, `counter` → counter (role: the plot's first selling role), `view` → board `topic=view`; a rule
  (`tools/city/content/plots.json`) may give any other spot (a `stand`, a `work` place) a kind, or `skip` one. A promenade
  unit whose variant is not built gets one counter at its door; built ones use their room's things (see "Promenade units"). As soon as a building exports one `thing`, its spots are no longer used.
* `Content/Dev/teleports/interiors.json`: one teleport per public building (1.5 m inside its main street door, facing
  in; kiosks and open halls: 2 m in front, facing them) and one per upper storey that has things; x/y map metres
  (docs/world_map.md), z world cm = the floor (feet), names `dev.tp.city.<plot>[.<storey>]` in the city strings.
* `Content/Localization/city/strings_{ru,en,ja}.json` (merged at load like `Localization/town/`): every key of
  `tools/city/content/strings/*.json` (`{"key": {"ru", "en", "ja"}}`, one file per theme) plus the teleport names. A key
  of the hand-written table may be used but never redefined (it would win).
* `Content/UI/quests.json`: the side quests of every content file's `"quests"` section (`quests.json` first, then the
  others by file name: `quests_*.json`, a building's own `x_<building>.json`; the same format as `quests.json`), one line
  each, marked `"from": "tools/city/interiors.py"`, replaced on every run (a quest dropped from the content goes away); the
  other quests stay byte for byte. A quest id in two files, or one without stages, is an error; every merged quest gets the
  same checks and the same `dev` blocks. Quest events used:
  `do <act id>`, `visit <plot id>` (sent by the game at the first use of a thing in a plot each day), `get|have|use
  <item>`, `reach <map place>`. The town's plots are not map places (`Content/UI/map.json`): the town quests name none.

`--check` fails on: an unknown kind or act, a string missing in any language (prompts, lines, titles, quest words), a
prompt with a placeholder, an unknown item (the ids are parsed from `LyudInventory.cpp`), a shop without a staff role,
a `needs_staff` role that does not work at the plot (and no staff variant fits), duplicate thing ids, a declared thing
more than 3 m outside its footprint or closer than 0.3 m to another (auto things that would fail are dropped and
counted), a quest event or target that does not exist, a quest `do` step no thing in town makes (or only things whose
staff their plot lacks), a quest `visit` of a plot without things, and an id rule (`<plot|type>/<id>`) that matches no
thing (ids change when a building is refurnished: fix or drop the rule).

### Acts

An act is what happens when the hero presses E at a thing:

| field | meaning |
|---|---|
| `prompt` | key of the whole prompt line ("E — полистать"); no price in it: the game adds it with `city.act.prompt_price` |
| `lines` | keys of texts shown in order, wrapping round (pages of a shelf, notices of a board, what happened at a game); an entry may carry conditions (below) |
| `rotate` | `use` (the default: the next line at every use, a shelf's next page) or `daily` (one line a game day, the next one on the next day the thing is read, so a running story is never skipped: notice boards, wall newspapers, price boards, the cinema's posters) |
| `title` | key of the page/trade screen title (every `buy_<shop>` act: the shop's title) |
| `minutes` | game minutes that pass (0: none). With `fade` the screen fades out and in |
| `until` | the hour the act ends at instead of `minutes` (sleep till 7; -1: none); `minutes` is the fallback |
| `price` | roubles taken (refused with `city.act.no_money` if the hero has less) |
| `give` | items put in the pocket: `[["item_id", n]]` (existing items only) |
| `needs_item` | an item the hero must have |
| `needs_staff` | a staff role that must be on shift at this plot (the projectionist for a film) |
| `any_hour` | runs outside the open hours too (seats, beds, doors, views); always written, the game defaults it for sit/lie |
| `pose` | `stand`, `sit`, `lie`, or an activity of activities.json (a seat given a standing act gets `<act>_seated`: the same act seated, same `target`) |
| `sound` | a one-shot of the soundscape (plausible ids; an unknown id plays nothing) |
| `event`/`target` | reported to the quests: always `do` + the act id |
| `daily` | how many times a day it gives its reward (0: no limit) |
| `shop` | opens the trade screen with this stock of `shops` when `needs_staff` is on shift |
| `screen` | a screen show (`film`, `stage`, `tv`, `slides`): the page in the dark wide style, the line cut into subtitles shown one after another, the minutes passing with them; a sitting show on a thing that is not a seat wants the hero seated first (a seat of the plot within 25 m) and is then offered from the seat |
| `collect` | a stamp every run gives (`"shelves/birds"`), or `"set/{line}"`: every line it shows is a piece of its own (the line's key: the films of «Рассвет»'s programme, a jukebox's records); an act `like` it inherits it (`"collect": ""` drops it) |
| `closed` | what things doing this act say while their plot is shut: an act id or a line key (see "Closed") |
| `game` | a mini-game played instead of showing `lines` (see "Mini-games"); an act `like` it inherits it (`"game": null` drops it) |
| `read` | `{"texts": ["<text id>", ...], "rotate": "use"|"daily"}`: the reading view opens one of these texts instead of the lines (in turn at each use, or one a game day); only on a `shelf`, `board`, `seat` or `take` (see "The reading page") |
| `sleep` | `true` (with `pose: lie`): the hero chooses the hour he wakes at instead of the act's `minutes`/`until` (which give the first hour offered; see "Sitting and lying") |
| `like` | content only: inherit every field of that act, then override |

A thing record of `interiors.json` has `id`, `plot`, `kind`, `ue`, `yaw`, `props`, `act`, `hours`, `days`, and when they
apply `auto`, `collect` (the stamp using it gives: a rule's `"collect": "kiosks/{plot}"` with the plot filled in) and `closed`
(the act it does while its plot is shut). A `<plot|type>/<id>` rule may give `kind`, `props`, `act`, `hours`, `days`,
`collect`, `closed`. The file's `collections` section: `{"<set>": {"title": key, "pieces": [...]}}`.

### Collections

Stamps the hero gathers, kept in the save with the rest ("CityThings": `c:<set>/<piece>`): the shelf topics he has read
(`shelves`: the `read_<topic>` acts, `"collect": "shelves/<topic>"`), the films seen at «Рассвет» (`films`: `cinema` and the
acts `like` it, `"films/{line}"`), the jukebox records heard (`records`: `jukebox` and its likes), the bus-stop kiosks
visited (`kiosks`: the rule `bus_stop_kiosk/counter`, `"kiosks/{plot}"`). Declared in a content file's `collections` section
(`tools/city/content/collections.json`): `{"<set>": {"title": "<key>", "pieces": [...]}}`; without `pieces` the set is every
piece something in town gives (a building's new lines grow it by themselves). `--check` fails on a stamp of an undeclared set
or piece, a declared piece nothing gives, a missing title. The first time a stamp comes the HUD says `city.coll.got`
(«Фильмы «Рассвета»»: 3 из 16), at the last one `city.coll.complete`. Lines may ask for stamps (`have`, `count`). Console
`lyud.City.Collections` (each set "n of m" to the log and on the page; `grant|clear <set|*>`); developer mode
`interiors.collections` (the page) and `interiors.collections_all` (toggle: every set complete / cleared).

### Closed

While a thing's plot is shut (the thing's `hours`/`days`; never with `interiors.open_all_hours`) a thing with a `closed` act
does that instead of being refused: the café's chairs say they are up on the tables, the cinema screen that the curtain is
drawn. It only shows words (no time, money, visit, stamp or sitting: a seat with a closed act can't be sat on at night) and
keeps a page of its own. Given by, first found: the thing's rule (`"closed"`), its act's `closed`, then the `closed_by`
section `{"<plot id | building type | plot kind>": {"<thing kind> | *": "<act or line key>"}}` (plot over type over plot
kind; `tools/city/content/collections.json`). A line key (or `"<prefix>.*"`) becomes an act `_closed:<key>`
(`city.act.look.prompt`, event `look`, target `closed`). Things open round the clock never get one. No geometry changes.

**Lines by the day and the hour.** In the content an entry of `lines` may be `{"k": "<key or prefix.*>", <conditions>}`;
interiors.py writes each line it gives as `{"k": key, <conditions>}` (plain keys stay strings, so old data reads the same):
`"h": [from, to]` hours (`[21, 6]` runs past midnight), `"d": "mon-fri"` weekdays, `"date": "05-09"` or `["12-20", "01-10"]`
(wrapping the year), `"after": N` (N game days after the game began, `UGameCalendar::StartDate`), `"every": [n, r]` (on the
days since the game began with `% n == r`: a notice of its own each game day, in order). A line shows only while all its
conditions hold; among those the act turns (`rotate`). Every act must have a line for every hour (citysim checks it): give
day lines `[6, 21]` and night lines `[21, 6]` together (`look_view`, `telescope`, `binoculars_post`); festival, weather and
collection lines only ever add to lines that always hold. `--check` fails on an unknown condition or a bad value (a festival
id not in `Content/PeopleData/festivals.json`, a weather word not in the list, a stamp of an undeclared collection).

| line field | meaning |
|---|---|
| `k` | the string key, or `"<prefix>.*"` for all that prefix's strings (each gets the same conditions) |
| `h` | `[from, to)` hours; `[21, 6]` runs past midnight |
| `d` | weekdays: `"mon-fri"`, `"sat,sun"`, `"fri-mon"` |
| `date` | `"MM-DD"` or `["MM-DD", "MM-DD"]` (a range may wrap the year) |
| `after` | from N whole game days after the game began (`Since >= N`) |
| `before` | until N game days after the game began (`Since < N`); with `after` a dated notice's days on the board, so notices replace each other instead of piling up (`--check` fails on `before <= after`) |
| `once` | `true`: shown once at a thing, then skipped there (remembered in the save, `o:<thing>|<key>`; per thing: another board with the same act shows it once too) |
| `every` | `[n, r]`: the days since the game began `% n == r` |
| `festival` | `"any"` (a holiday today: `ULyudFestivalSubsystem::IsFestivalToday`; the weekly market does not count), `"none"` (no holiday today), a festival id (`new_year`, `lantern_night`, `sea_festival`, `weekend_market`...: one on now, `ActiveFestivals`) or a list of ids. On a `daily` act a holding festival line takes the day (the board shows the holiday notice all day; the run of notices goes on the next day where it was, nothing skipped); on a `use` act it joins the turn |
| `weather` | `rain`, `snow`, `fog`, `cold` (< 2 C), `warm` (> 22 C), `clear`, `wet` (wet ground), `dry` (nothing falling), or a list: any of them. From `ULyudWeatherSubsystem::GetSky()` with the townsfolk's talk thresholds (`LyudCity::WeatherBits`) |
| `have` | `"set/piece"`: the hero has that stamp |
| `count` | `["set", n]`: the hero has at least n stamps of the set |
| `collect` | no condition: the stamp the hero gets when the line is shown; `"set/*"` on a `"<prefix>.*"` entry gives each line the key's last part as its piece |
| `read` | `"<text id>"`: the hero has read that text of the reading view to its end (`--check` fails on an unknown text) |

Content tables (`tools/city/content/*.json`, each with any of `acts`, `shops`, `rules`, `defaults`,
`defaults_by_plot_kind`, `quests`, `collections`, `closed_by`, `texts`; any other section is an error, and so is a key defined in two
files; a building's own content goes in `x_<building>.json`, its words in `strings/x_<building>.json`): `acts.json` (defaults per kind, generic acts,
shelves, boards), `activities.json` (machines, games, sport, crafts, screens), `counters.json` (shops, counter talk),
`plots.json` (what the buildings' spots do while there are no declared things). An act may say `"like": "<act>"` to
inherit, and `"lines": "<prefix>"` for all `<prefix>.<n>` strings, or a list of keys where `"<prefix>.*"` stands for that
prefix's strings (a board's generic notices followed by a building's own: `["city.look.club.*", "city.reg.cog.*"]`). A thing's act is found by its rules, first match
wins: `<plot>/<thing id>`, then `<type>/<thing id>` (globs allowed: `workers_club/seat*`; an exact id beats a glob; these
may also set `kind`, `props`, `act`, `hours`, `days`), then `shop=<id>` in its props (act `buy_<id>`), then
`<kind>:<prop>=<value>[,<prop>=<value>]` (most conditions win; `*:what=piano` matches any kind; a value may be a glob
and `id` is the thing's id: `machine:what=jukebox,id=*radio*`), then `take` with
`item=<id>` (`take_<id>`), then the plot kind's default (`hotel` beds sleep the night, `clinic` beds nap, `residential`
counters are closed), then the kind's default (`sit`, `bed_private`, `read_misc`, `look_misc`, `counter_talk`,
`use_misc`, `watch_misc`, `take_any`, `door_any`).

### Mini-games

An act's `game` (`tools/city/content/activities.json`, `games.json`) is played on a small panel instead of the act's page:
the hero stands (or sits: `pose: sit`) at the thing (walking 2.5 m off, getting up or any screen ends it unfinished),
and the act's `minutes` pass at the end. The
model plays it (`LyudCity::FModel` `StartGame`/`TickGame`/`PressGame`/`FinishGame`, deterministic from a seed: the thing,
the day, the uses), so citysim decides every rule. Three types:

* `aim` (darts, billiards, the gallery, ring toss, the strongman, the arcade, the hoop): a marker runs to and fro along a
  bar; E throws. Within the zone's bright middle (zone/6 round its centre) 2 points, within the zone 1, else a miss; a throw
  not made in 8 s is a miss. `rounds` throws; alone: `need` points wins, half of it is a draw; against a regular: more
  points than his (he throws by his skill).
* `rally` (table tennis, table football, badminton): the ball flies along the bar; E while it is in the window at the
  hero's end hits it back (a press in the first 40% of a flight is ignored; early or late loses the point). After each hit
  the window narrows (`shrink`) and the ball quickens (`pace`). Against a regular: first to `need` points; he returns with
  `0.55 + 0.4·skill − 0.05·hits` (0.25..0.96). Alone: `need` hits in a row wins, half of it a draw.
* `duel` (chess, checkers, dominoes): `rounds` turns; keys 1/2/3 pick one of three `moves`, each beating the next
  (`1 > 2 > 3 > 1`, the winner scores its `power`). The opponent leans to a move by his style (`bold` the first, `careful`
  the second, `tricky` the third, `steady` none) and gives a tell (`tell`, `{0}` his name) that is honest with probability
  `0.9 − 0.5·skill` (a tricky one 0.15 less; 0.35..0.85): reading it wins most games, guessing about a third.

| game field | meaning |
|---|---|
| `type` | `aim`, `rally`, `duel` |
| `title`, `keys` | the panel's title and keys line (defaults `city.game.title`, `city.game.keys.<type>`) |
| `rounds`, `need` | throws / turns, and points or hits to win (defaults by type: aim 5/5, rally 1/3, duel 3/1) |
| `speed`, `zone` | the marker's (ball's) speed in bars a second, the zone's (window's) width 0..1 |
| `shrink`, `pace` | rally: the window ×shrink (0.5..1) and the flight ÷pace (1..1.5) after each hit |
| `vs` | the regulars' game (`chess`, `darts`...): somebody who plays it sits in; none: the hero plays alone |
| `moves` | duel: three `{"k", "power" 1..5, "tell"}` |
| `win`, `lose`, `draw` | line sets (like `lines`, conditions allowed) shown on the result page, `{0}` the opponent; `--check` and citysim want one for every hour |
| `collect` | the stamp a win gives; `{opponent}` is the regular's id (`chess_wins/{opponent}`: «chess opponents beaten») |
| `prize` | items a win gives: `[["item", n]]` |
| `daily` | games a day (0: no limit); after that the act runs as before, with its own lines |
| `sounds` | `{"hit", "miss", "end"}` soundscape ids (defaults `rest_clink`/`mouse_clicks`, none, `rest_bell`) |

**Regulars** (`games.json` `regulars`, written to `interiors.json`): `{"<id>": {"name": "npc.<resident>.name", "style",
"skill" 0..1, "plays": [vs...], "plots": [...] (none: anywhere), "hello", "win", "lose"}}`: real residents of the town
(names from `Content/Localization/town`, only read), ten of them (Giuseppe, Noam, Anahit, Nils, Lars, Ursula, Kenji, Joost,
Freja, Hiroshi; lines `city.player.<id>.*`). The game picks one of those who play it on that plot or anywhere (else anyone
who plays it), by the seed. A win `Notify("win", <act target>)` for the quests besides the usual `do`. Result page:
`<thing>#<result>` keeps its own place in the lines. Words: `strings/games.json`.

### Wall clocks and light switches

* A clock: any thing with `what=clock` or `what=wall_clock`, or a `board` with `topic=clock`: the act `look_clock`, lines
  `city.clock.*` with `{time}` (the game's clock, `LyudCity::FillTime`, filled in by the engine at showing). Any line of any
  act may use `{time}`.
* A light switch: `what=switch` with `switch=room` (7 m round it), `hall` (15 m) or a number of metres (1..30); a bare
  `what=switch` is a room. E: if any room lamp (`B.lamp`) of the town's buildings within that radius and on the switch's floor
  (1.5 m under to 4 m over it) is lit, all go off, else all on, whatever the hours say (`ULyudWindowLights::SwitchLampsNear`:
  flags read by the lamp pool, no components, no windows). Back to the hours when the hero walks into another plot or starts
  over. `city.act.switch.on|off|none` on the prompt line.
* No building declares either yet: the building modules add `B.thing(..., "board", topic="clock")` or
  `B.thing(..., "machine", what="switch", switch="room")` by a door.

### Sitting and lying

What is a real pose and what is a stand-in:

* **Sitting is a real pose** (any `seat`, any thing with `height`, an act with `pose: sit`): `AHeroCharacter::SitOn` (the
  restaurant's mechanism, `HandReach::ApplySit` in the hero's anim) on one pooled anchor. The seat's frame is the floor under
  the cushion's front edge facing the thing's `yaw`; the floor is a trace down from the cushion checked by
  `LyudCity::FModel::SeatFloor` (a hit up near the cushion is the seat's own box: the floor is `height` below it; a hit more
  than 60 cm from where `height` puts the floor, a stairwell or a hole, is not believed): the feet stay on the floor. The
  spot he stands up to is found as he sits (`StandOffsetFor`): in front, beside, behind (a bed: off its side), the first
  where his capsule fits (`ECC_Pawn`) with floor under it within 40 cm of the seat's; in front when none does. Seated, the
  view is held within a cone about the seat's facing (`APlayerCameraManager::ViewYawMin/Max`; half-angle the thing's
  `cone` prop 30..170, default 110: `FModel::LookCone`), let go on every way up. Any step (or Space) stands him up.
* **Lying is a camera stand-in** (the hero has no lying pose; the townsfolk's lying clips are on another skeleton): he sits
  on the bed's edge, hidden, and the view settles over 1.6 s (ease in-out) onto a pooled camera at the pillow, looking toward
  the nearest window view of the room (a thing of the plot with `topic=view` within 6 m, 1 m under to 2.5 m over the bed:
  `FModel::WindowFrom`), else at the ceiling. Any step, Space or E gets him up.
* **Sleep** (`"sleep": true` on a lie act: the hotel's and the sanatorium's night beds, `sleep_night` and its likes): once he
  is on the pillow the page shows an hour chooser (`city.sleep.*`): A/D or the arrows change the hour (round the clock, never
  the hour it is now: `FModel::SleepStep`), E sleeps, W/S/Esc/Space get him up. The first hour offered is the act's `until`,
  else now plus its `minutes` to the next whole hour (`FModel::SleepDefaultHour`). E: the screen goes black, the clock jumps
  to the hour (`UGameCalendar::SetNow`: the people's life resettles), the view comes back, a moment of the window, he gets
  up and the bed's line shows on the page.
* **Never stuck**: one seat at a time (a new sit stands him up first); the seat's anchor gone from under him (he is no longer
  seated but was never stood up): walking and collision come back (`RecoverFromSeat`, logged); a load while seated or lying:
  everything the hero was in the middle of here is let go (the text, the chooser, a game, a show, a fade, the pillow's view),
  he stands up where he would from the seat when the save had him there, else stays where the save put him; a townsperson
  can't claim the seat or bed he is on, and he can't take one somebody holds (`city.act.taken`). While a text or the chooser
  is up the seat's keys are the view's (`AHeroCharacter::SeatMove/SeatLeave`); the hooks only ask, the tick acts.
* Console: `lyud.City.Sit [thing id]`, `lyud.City.Lie [thing id]` (the nearest free one without an id, from anywhere),
  `lyud.City.Sleep <hour>` (lying: sleep until then). Developer mode: `interiors.sit_nearest`, `interiors.lie_nearest`.

### The reading page

An act with `read` opens a text in the reading view (`Private/City/SLyudCityBook.*`, made once, collapsed between texts,
hit-test invisible) instead of its page: what it is (`city.book.kind.<kind>`), the title, one page, `page 2 of 4` and the
keys. E, D or right: next (E on the last page puts it down); A or left: back; Esc, W, S, up, down or Space: shut; walking
2.5 m off, getting up from the seat it was read on, any screen or the game menu shut it too. Standing, his steps are held
while it is up; seated (an act with `pose: sit`: the library's reading pods), the seat's keys shut it. Each page read
(the opening and every page forward) passes the text's `minutes` (the usual smooth jump); leafing back costs nothing. The
page each text was left on is remembered (`b:<text>`); a text read to its end opens at its first page again. Reaching the
last page the first time reads it: the text's `collect` stamp, the HUD's `city.book.read`, the quest event
`read`/<text id>, and the plot it was read in (`r:<text>`), shown in the Read list: under the collections on
`lyud.City.Collections`' page, `city.read.list.*`, per place (`city.plot.<plot>.name`, else its teleport name).

`texts` (any content file; `tools/city/content/reading.json`, words in `strings/reading.json`):

```
"texts": {"<id>": {"title": "<key>", "kind": "book|notice|ledger|newspaper", "minutes": 0..30 (2),
                   "collect": "<set>/<piece>", "pages": ["<key>", {"k": "<key>", <line conditions>}, [<alternatives>], ...]}}
"acts":  {"<act>": {..., "read": {"texts": ["<id>", ...], "rotate": "use|daily"}}}
"acts":  {"sleep_night": {..., "pose": "lie", "sleep": true}}
line condition: {"k": "<key>", "read": "<text id>"}
```

A page is a key, a line with conditions, or a list of alternatives: those whose conditions hold now, one of them by the
days since the game began (the newspaper's second page changes daily; a page with none holding is left out). 1..12 pages,
at least one with no conditions; a notice and a newspaper are drawn on paper. `--check` fails on an unknown text, a text
without pages, a `read` act on another kind, `sleep` without `pose: lie`. Now: six texts (`birds_starlings`,
`library_rules`, `cog_minutes`, `paper_today`, `kids_hedgehog`, `cookbook_soup`) on the library's pods, catalogue and
children's wall, the store's, clinic's, hall's and club's shelves and every daily paper; the set `reading`. Console
`lyud.City.Read <text id>|next|prev|close|list|all|clear`; developer mode `interiors.read_nearest`,
`interiors.read_all` (toggle: every text read / none), `interiors.read_list`.

### Staff talk

At a counter whose act (or thing) has `topics` and whose staff is on shift, E opens a chooser on the page instead of the
shop or the act's page: the title is the name of the staff member on shift nearest the counter (`UPeopleLifeSubsystem::
GetDisplayName`; `city.talk.speaker` when nobody is drawn), the act's line on top, then numbered entries (`city.talk.entry`,
`city.talk.entry_new` for a topic not heard yet, `▸` on the chosen one) with `city.talk.buy` first when the act sells. 1-9
pick at once, A/D (left/right, up/down) move, E asks, Esc/W/S/Space or walking 2.5 m off leave; the hero's steps are held
while it is up. An answer's lines come one per E (`city.talk.keys_answer`, the last `city.talk.keys_back`), then the chooser
again with the topics of now (a `once` topic heard is gone; a `heard` line or topic may open). A topic is offered only when
its conditions hold and at least one answer line holds now; at most 8 per counter, at most 6 lines per answer. Nobody on
shift, the plot shut (its closed words), a game, a show, a text, sitting or lying, or minutes behind a fade: no chooser,
the act runs as before. `@open` shops without staff never talk. Asking: the lines that hold now (a `once` line only the
first time), the quest event `ask`/<topic id> every time, the topic heard (`h:<topic>` in the save), and the first time
its `collect` stamp (`staff/<trade>`: the collection "Faces behind the counters", 31 pieces).

```
"acts":   {"<act>": {..., "topics": ["<topic id>" | "@<set>" | {topic}, ...]}}     (a rule's "topics" too: its things)
"topics": {"<act id>": [ {topic}, ... ]}                                           (any content file: adds to that act)
"topic_sets": {"<set>": [ {topic}, ... ]}                                          (shared: "@rassvet" = the film topic)
{topic} = {"id": "<trade>.<topic>", "k": "<title key>", "lines": [<line keys or {"k", <line conditions>}>],
           "once": 0|1, "gives": {"collect": "<set>/<piece>"}, <line conditions: the topic is offered only while they hold>}
line conditions (everywhere): h, d, date, after, before, once, every, festival, weather, have, count, read, and
  "did": "<act target>" | "!<target>" (the hero has done it at least once: watched the film, played chess, the telescope),
  "heard": "<topic id>" | "!<topic id>"
```

The model (`TalkReady`, `TopicsNow`, `TopicOffered`, `AskTopic`, `ForgetTopics`, `TalkRole`, `HasDone`) is checked in
citysim; `did` is filled by every act run (`w:<target>` in the save; an old save seeds it from its uses). Content:
`tools/city/content/talk.json` with words in `strings/talk.json` (`city.ask.<trade>.<topic>` titles, `.N` lines): 31
trades (the six kiosks, «Тарелка», the library desk, the club buffet and cloakroom, the store's tills and departments, the
market, its bird/bait/tea stalls, fish shop, bakery, «Чайка», dumplings, pasties, beer, the book stall, souvenirs, rentals,
the photo desk, the clinic, the sanatorium, the hall bar, the fair, the sports café, «Шестерня»'s lending desk, the
lifeguards and the keeper), 93 topics, 218 answer lines, ru/en/ja. They point at each other: the film of the day at
«Рассвет» (one topic shared by the buffet and the cashiers), the library's stamp diary, the potter's week at the culture
block, Kesha the parrot, the chess regulars, the morning paper. `--check` fails on an unknown act or topic, a topic without
a plain line, more than 8, an unknown `did` target or `heard` topic. Console `lyud.City.Ask <thing id> [topic id] | list |
reset` (from anywhere, whatever the hour and staff); developer mode `interiors.topics` (heard, on the page and the log),
`interiors.topics_reset` (every topic new again), `interiors.ask_nearest`.

### Room tones

Each plot may name a tone: a quiet loop and sparse one-shots. Five times a second (the pick's rate, nothing per frame) the
nearest thing within 10 m on the hero's floor names the plot; its tone (or, while the plot is shut, the tone's `closed`
one) plays; none near, past the town box or a plot without a tone: silence. One 2D `UAudioComponent` (made once with
`UGameplayStatics::CreateSound2D`, kept in a UPROPERTY, never auto-destroyed) plays the loop: a new loop stops the old and
fades in over 1.5 s, the same loop only moves its level, silence fades out over 1 s; a loop asset that does not loop by
itself is started again. Level = the tone's `volume` x the soundscape's master and ambience volumes, kept down to 20 % as
`GetOutdoors()` goes to 1 (an open kiosk). One-shots: a pool id of the soundscape every `every` seconds (random in the
range, the model's dice) via `ULyudSoundscapeSubsystem::PlayEffect` 2-5 m from the hero at `shot_volume`. The loops are
loaded once when the file is adopted (`LoadObject`, a handful).

```
"tones":   {"<id>": {"loop": "<sound id>", "volume": 0..1, "shots": ["<sound id>"], "every": [min, max] s (12..30, 2..600),
                     "shot_volume": 0..1, "closed": "<tone id>", "about": "..."}}
"tone_by": {"<plot id>" | "unit:<variant>" | "<building type>" | "<plot kind>": "<tone id>"}     (first match in that order)
"sound_alias": {"<id an act uses>": "<soundscape id>"}                                           (page_turn -> diary_page ...)
```

interiors.py writes `tones` (with the loop's asset path, `/Game/Audio/<Group>/S_<id>_01`) and `plot_tones`. Now: ten tones
(`hall_quiet`, `reading_room`, `cafe_room`, `bar_room`, `kitchen_shop`, `shop_room`, `cold_counter`, `empty_room`,
`sport_hall`, `clock_room`) on 50 plots, all from sounds that exist (station hall, restaurant room and kitchen, the
vending hum, diary pages), except `clock_tick` and `ball_bounce` (silent until made). Console `lyud.City.Tone`; developer
mode `interiors.tone`.

### Sound ids

Every act `sound` is checked against the soundscape's pools (`Private/Audio/LyudSoundscape.cpp` `Groups`) and the files
(`audio/sounds.json`, `Content/Audio`): `python3 tools/city/interiors.py` prints the ids that play nothing, `--sounds`
the table. 16 aliases map ids the acts use onto existing ones (`page_turn`/`paper_rustle` -> `diary_page(s)`,
`coin(_drop)` -> `rest_coins`, `cup_clink` -> `rest_clink`, `plate_clink` -> `rest_plate`, `water_pour` -> `rest_pour`,
`purr` -> `cat_purr`, `shop_bell`/`bell_ding`/`bell_ring` -> `rest_bell`, `fridge_hum` -> `vending_hum`, `arcade_blip` ->
`console_game_far`, `pan_sizzle` -> `rest_sizzle`, `aquarium_bubbles`/`water_bubble` -> `swim_bubbles`); the game's own
page turns use `diary_page` (its wav exists under `audio/sfx/diary`, it plays once imported). Still missing (56 ids, no
file anywhere; the acts are silent until they are made):

| group | ids (acts) |
|---|---|
| games and sport | `air_rifle` (gallery), `ball_bounce` (basketball, school), `billiard_click`, `chess_piece` (12 acts), `dart_thud`, `foosball_rattle`, `pingpong_ball`, `ring_clink`, `mat_thud`, `weights_clank`, `bike_whirr`, `footsteps_run`, `whistle_short` |
| rooms and machines | `clock_tick` (8), `radio_tune` (14), `tv_hiss`, `vinyl_crackle` (12), `tape_hiss`, `projector_whirr`, `slide_click`, `jukebox_click`, `washer_hum`, `dryer_hum`, `sewing_rattle`, `wheel_hum`, `phone_dial`, `morse_beep`, `mic_feedback`, `train_clack`, `music_box` |
| hands and paper | `drawer_slide` (10), `pencil_scratch`, `paper_tear`, `stamp_thump`, `ticket_punch`, `scissors`, `chalk`, `brush_strokes`, `plane_wood`, `scale_clunk` (9), `hanger_clink`, `camera_flash`, `sack_rustle` |
| home and weather | `sheets_rustle` (20 beds), `kettle_whistle`, `tap_water`, `water_sip`, `steam_hiss`, `fire_crackle`, `wood_creak`, `wind_soft` (14) |
| music and voices | `piano_chord`, `choir_hum`, `waltz`, `school_bell`, `stage_curtain` |

Also known: the `Fishing` group is in the soundscape's list with no files; `Bikes` and `Railway` are imported but not in
the list (their ids play nothing from an act).

### How to add content (for the furnishing agents)

1. Declare things with `B.thing(id, kind, pos, yaw, **props)`; ids stable and unique in the building; then run
   `python3 tools/city/interiors.py --check` and `python3 tools/city/interiors.py`; read the table for your plot.
2. `shelf`: `topic=` `birds` `sea` `science` `poetry` `kids` `local` (=`history`) `music` `art` `cooking` `travel`
   `chess` `sun` (=`astronomy`) `garden` (=`gardening`) `engineering` `medicine` `poetry_sale` (a bookstall's sale table).
   Anything else reads the generic pages.
3. `board`: `topic=` `timetable` `lost` `club` `wallpaper` (=`school`) `health` `cinema` `prices` (=`market`) `rules`
   `exhibit` `painting` `view` `store` `catalogue` `logbook` `fair` `birds` (the feed-the-birds poster: points to the
   library's guide and the garden department) `garden` `kids` (=`toys`) `local` (=`history`, a case of local things)
   `medicine` `sun` `travel` (=`map`).
4. `machine`/`play`/`sport`/`craft`/`screen` (any kind works): `what=` `piano` `chess` `table_tennis` `billiards`
   `darts` `table_football` `shooting_gallery` `ring_toss` `lottery` `strongman` `fortune` `arcade` `jukebox`
   `record_player` `scales` `eye_chart` `checkup` `photo_booth` `telescope` `binoculars` `payphone` `fountain`
   `pump_room` `haircut` `barbell` `wall_bars` `punching_bag` `exercise_bike` (=`treadmill`) `pool` `basketball` `track`
   `dance` `vaulting_horse` `mats` `easel` `potters_wheel` `workbench` `sewing_machine` `darkroom` `school_desk`
   `study_desk` (homework, no teacher) `cooking` `blackboard` `cinema` `stage` `choir` `microphone` `tv` `aquarium`
   `slide_projector` `stove` `sink` `washing_machine` `radio` (a broadcast set; hall radios declared `jukebox` with
   `radio` in the id get it too) `radio_set` (a radio amateur's station). Some need staff on the plot: `cinema`
   projectionist, `school_desk` teacher, `checkup` doctor, `pool` lifeguard, `haircut` barber, fair games vendor,
   `choir` choir_leader.
5. `counter`: `shop=<id>` (see `counters.json`: `store_*`, `market_*`, `kiosk_news`, `cafe_tarelka`, `pharmacy`,
   `club_buffet`, `hall_bar`, `sport_cafe`, `library_desk`, `sanatorium_kiosk`, `seagull_cafe`, `fish_shop`, `dumplings`,
   `souvenirs`, `bakery`, `book_stall`, `beer_bar`, `pasty_bar`; `stories.json`: `kiosk_icecream`, `kiosk_tobacco`;
   `furnished.json`: `fair_*`, `market_tea`, `store_cafe`,
   `sanatorium_buffet`, `cafe_ice_cream`, `canteen`, `bookshop`, `hardware` (bird feed and feeders), `fabrics`,
   `toy_shop`, `corner_grocery`) and/or `service=` `cashier` `cloakroom` `tickets` `reception` `lending` `bar`
   `butcher` `fishmonger` `rentals` `repair` `pharmacy` `newspapers` `bank` (the savings bank: a talk) `records`
   `shoes`; a fair kiosk says what it sells with `trade=` `coffee` `icecream` `press` `toys` `snacks`. `role=` the staff
   role standing there; a house desk (`service=reception`, `role=concierge|caretaker|janitor`) is a talk that needs no
   staff. A role the plot lacks gets a staff variant (above).
6. `bed`: `rest=night|nap|private` (default by plot: sanatorium night, clinic nap, elsewhere "someone's bed"; a bed in
   a family's home is `private` even if it is a mat). `take`: `item=daily_paper|postcard` (once a day). `door`:
   `to=roof|staff` (a rule may give a caption of its own: the bridge house's lift `door_gallery`).
7. Keep the ids the auto layer uses where the quests rely on them (library `stack_*`/`core_wall_*`/`pod_*`, clinic
   `patient_0..2`, school `seat_*`/`teacher`, `workers_club/seat*`, `cog_club/chess*`, sanatorium `pump_room`), or give
   the new things the same props (e.g. `shelf topic=birds`, `machine what=scales`): the quests wait for `do read_birds`,
   `read_local`, `read_sun`, `cinema`, `chess`, `scales`, `eye_chart`, `checkup`, `photo_booth`, `pump_room`,
   `school_desk`, `blackboard`.
8. New words: add a theme file `tools/city/content/strings/<theme>.json` (ru first, real en and ja), new acts in a new
   content file; never edit generated files. Only existing item ids (new items need paid icons: game-items skill).
9. Stories (`tools/city/content/stories.json`, words in `strings/stories.json` and `strings/homes.json`): per-plot rules
   give one home's or one kiosk's things their own acts (`<plot>/<thing id>`, e.g. `P-T1H2/flat_books_sea` → `holm_log`);
   story acts have their own ids so the home quests (`quests.json` `home_*`) can wait for `do <act>`. Lines
   `city.home.<who>.<object>.<n>`; a building's running story on a board: `city.reg.<building>.<n>` appended to the
   board's generic act (`look_club_cog` keeps `target: look_club`). A refurnished home that renames a thing id breaks its
   rule (the check says which): rename the rule with it.

## 3. Game

* `ULyudCityThings` (tickable world subsystem, `Public/City/LyudCityThings.h`, `Private/City/LyudCityThings.cpp`): at world
  begin play `City/interiors.json` is read and parsed on a worker (`UE::Tasks`, bytes straight to the parser; the log says the
  milliseconds) and handed over on the next tick; the things simply appear a moment later (no file: no things, no tick). A
  load that restores the save before then keeps the record and applies it when the model arrives. Five times a second, and
  only while the hero is within 30 m of the box round all things, the model picks the thing in view from a 4 m grid (reach
  2.6 m from the eyes, in front, near the line of sight, on the eye's floor); one trace checks no wall is between; a door in
  view wins (doors poll E too). Not as a bird, on a bike, seated on somebody else's seat (restaurant, train), with a screen,
  shop or menu up (`ULyudInventoryUI::CanOpen`), while the NPC talk has E (`ANpcTalkDirector::IsEngaged`: somebody in front
  to talk to, or a talk going on; one press never both talks and buys), while the clock is running on. The prompt goes
  through `ULyudInventoryUI::SetPrompt` (built only when the thing changes; with a price `city.act.prompt_price`). E
  (polled) runs the act:
  * refused (`city.act.closed|no_money|needs_item|no_staff|daily_done|sit_first`): the reason on the prompt line for 2.5 s;
  * `price` via `ULyudInventory::Spend`, `give` via `Add` (unknown items skipped, logged at load);
  * `lines`: a page (`Private/City/SLyudCityPage.*`, HUD style, hit-test invisible, takes no input) with the title, the
    line and `E — next` / `E — close`; E turns through the lines that hold now once (a `daily` act shows today's one),
    walking 2.5 m away or any screen shuts it;
  * `minutes`: up to 15 pass over about 0.6 s (at most 30 game s a frame: the people's life steps 1 s at a time and must
    not run 900 steps in one frame), the hero's moves held meanwhile; more pass at once (the people's life resettles,
    cheap); with `fade` a camera fade to black, the time, a fade back;
  * `shop`: `ULyudInventoryUI::OpenShop` with the `shops` stock, title = the act's `title` (default `city.shop.title`);
    `needs_staff` is asked of `UPeopleLifeSubsystem::StaffOnShift(plot, role)` (role `*`: anyone); the one on shift says a
    greeting over their head once a visit (`ULyudTownTalkSubsystem::SayEvent(.., "counter_hello")`, sets
    `counter_hello_*` in `Content/Dialogue/town_barks.txt`; only if their body is drawn within 6 m of the counter);
  * `pose` `sit`: `AHeroCharacter::SitOn` on one pooled anchor (an actor at the origin, made again if it went away; the
    seat frame from the thing's point, yaw, `height` and a checked floor trace), a free spot to stand up to, the look cone
    (see "Sitting and lying"). A `height` outside 0.05..1.2 m is not believed (the usual 0.45, a warning): some exports gave
    the height over the building's base;
  * `pose` `lie`: he sits on the bed's edge (the body holds there) and the view settles onto a pooled camera at the pillow
    looking toward the room's window or the ceiling (the hero hidden meanwhile); with `fade` the switch is made in the dark;
    once the time has passed he gets up by himself after 2.5 s; any step, Space or E gets him up at once; a `sleep` act
    opens the hour chooser. The view and the hero's visibility come back on every way out (standing up, the hero gone, a
    load, the level ending). No lying animation: none to reuse safely;
  * `read`: the reading view (see "The reading page");
  * `screen`: the show (see the act field), subtitles 2.8..7 s each by length, the minutes in equal shares as they come;
    over at E (`E — stop watching`), at getting up, walking off or any screen;
  * `sound`: `ULyudSoundscapeSubsystem::PlayEffect(id)` (an id without a pool plays nothing; see "Sound ids");
  * `topics` with the staff on shift: the talk's chooser (see "Staff talk");
  * quests: `Notify(event or "do", target or act id)` and `Notify("visit", plot)` at every use; also `visit` when the plot
    round the hero changes (the nearest thing within 4 m on his floor, five times a second; walking out between buildings
    keeps the last plot). A quest's last stage should wait for a `do` (or `have`), never a bare `visit`: the quest engine
    takes an event a later stage waits for as a skip-ahead (`errands.json`: `aoki_fitting`, `okonkwo_barber`).
* Default acts when a thing has none: seat sits; bed rests 2 h (`rest_hours`) with a fade; shelf reads 10 min (`read`/topic);
  board looks (`look`/topic); counter needs its `role` (`talk`/role); machine 5 min, play 20, sport 15, craft 30, screen
  30 with a fade (event = the kind's verb, target = `what`); take gives its `item` once a day; door shows a caption. Seats,
  beds and doors ignore the plot's hours; everything else keeps them.
* Rules in `Private/City/CityThingsModel.h` (`LyudCity::FModel`: `Load`, `Pick`, `Check`, `Run`, `NextLine`, `Nearest`,
  `NearestThing`, `ScreenFrom`, `NeedsSeatFirst`, `SeatHeight`, `Subtitles`, `SaveState`/`LoadState`,
  `ResetToday`/`ResetState`; also `IsShut`/`ActIndexNow` (the closed act), `GiveStamp`/`CountStamps`/`GrantSet`/`ClearSet`,
  `FindFree`/`Claim`/`Release`/`ThingsOfPlot`, `WeatherBits`), engine-free; `tools/citysim/run.sh` checks it (257 checks, the mini-games, sitting, sleeping, reading, the staff talk and the room tones included) on `tools/citysim/fixture.json`, made-up
  acts in the test itself and the real file when it is built. Save record "CityThings" (pages, daily boards' days, today's
  counts, uses, plots visited); a New Game starts it over.
* Console: `lyud.City.Things` (count, the nearest five), `lyud.City.Do <thing id>` (run it from anywhere; sitting only
  when the hero is within 4 m), `lyud.City.Things.Show 0|1` (labels within 15 m), `lyud.City.Weekday <mon..sun>` (the clock
  on to the next such day, same time), `lyud.City.Collections [grant|clear <set|*>]`, `lyud.City.Sit|Lie [thing id]`, `lyud.City.Sleep <hour>`,
  `lyud.City.Read <text id>|next|prev|close|list|all|clear`, `lyud.City.Ask <thing id> [topic id]|list|reset`, `lyud.City.Tone`. Developer mode, group `interiors`:
  things nearby, use the nearest, reset today's limits, 500 rub., `interiors.open_all_hours` (toggle: no hours, days or staff
  asked; no closed acts), `interiors.show_things` (toggle: the labels), `interiors.collections` (the collections' page),
  `interiors.collections_all` (toggle: every set complete / cleared), `interiors.game_aim|game_rally|game_duel` (the
  nearest game of that type, from anywhere), `interiors.game_win` (win the game being played), `interiors.regulars` (the
  regulars' page; the log lists each one's style, skill, games, plots), `interiors.sit_nearest`, `interiors.lie_nearest`,
  `interiors.read_nearest`, `interiors.read_all` (toggle), `interiors.read_list`, `interiors.topics`, `interiors.topics_reset`,
  `interiors.ask_nearest`, `interiors.tone`. Console `lyud.City.Game <thing id>|press [1-3]|win|quit|regulars`. Every city quest stage has a `dev` block written by interiors.py (`quest_dev`): a teleport in front of a thing
  that makes its first objective (`city.q.<quest>.<n>`), the clock an hour after it opens, `lyud.City.Weekday` to a day it is
  open, `Inventory.Earn` for its price, the items a later stage only checks; a stage's own `dev` in
  `tools/city/content/quests.json` wins key by key.
* Perf ([perf checklist](../README.md)): no per-frame allocation, no actors or components per thing, nothing past the town box,
  the file parsed off the game thread.

### API for townsfolk

For the people's life (a townsperson who sits down in the café, reads at a library shelf, waits at a counter), on
`ULyudCityThings` (`Public/City/LyudCityThings.h`), the logic in the model and checked in citysim:

* `bool FindFreeThing(FName Kind, const FVector& Near, float RadiusCm, FCityThingRef& Out) const`: the nearest thing of
  `Kind` (`seat`, `bed`, `shelf`, ...; None: any kind) within `RadiusCm` of `Near` on about its floor (2.5 m up or down) that
  nobody holds and the hero is not sitting or lying on. Reads only the model's 4 m grid cells round `Near` (no pass over the
  ~1400 things), allocates nothing but the ref's strings.
* `FCityThingRef`: `Id` (`P-CAFE/chair_3`), `Kind`, `Plot`, `Location` (the thing's point, world cm: a seat's cushion), `Yaw`
  (the way it faces: a sitter looks that way), `SeatHeightCm` (a seat's or bed's surface over its floor, 0 otherwise), `bHeld`.
* `bool ClaimThing(const FString& Id, const UObject* Who)`: `Who` (the person's actor or any object that lives as long as the
  hold) takes it; true when it was free or already theirs, false when somebody else holds it or the hero is on it. The hold is
  weak: when `Who` is destroyed it goes by itself (checked five times a second). While held, the hero's E on that seat or bed
  is refused with `city.act.taken` ("Здесь уже сидят"); other acts (a shelf, a counter) are not blocked.
* `void ReleaseThing(const FString& Id, const UObject* Who)`: lets it go (only the holder can).
* `void GetThingsOfPlot(const FString& PlotId, FName Kind, TArray<FCityThingRef>& Out) const`: the plot's things of `Kind`
  (None: all), appended; a table by plot built once at load.
* Holds are not saved (the people's life places everyone again after a load) and survive a New Game of the city's state.
  The things appear a moment after the level starts (parsed on a worker): before that every call finds nothing.

### How to test in the editor (Mac)

1. Build, run `Lvl_Scene01` (PIE or `-game`); the log says `LogLyudCity: CityThings: N things, M acts, K shops from City/interiors.json (0 unknown items)`.
2. `lyud.City.Things`: the count and the five nearest things with distances.
3. Teleport into a room (developer mode `interiors` teleports, or `BugItGo`), `lyud.City.Things.Show 1`: labels with id and act; the one in view yellow.
4. Look at a shelf: `E — полистать` at the bottom; E: the page, `E — дальше` turns, the clock jumps 10 min.
5. A chair: E sits (the view drops); a step or Space stands up.
6. A bed: E, the screen fades to black and back, the clock is 2 h on.
7. `lyud.City.Do <counter id>` with the staff away: `refused: За стойкой никого нет`; on shift: the shop opens.
8. A paid act with too little money: `Не хватает денег: нужно N руб.`; `lyud.Language en|ja`: no raw `city.*` keys.
9. Save, load: the shelf goes on at the next page (`CityThings: do ... line=` in the log).
10. A notice board twice today: the same notice; `Dev.Do time.plus_day`, again: the next one.
11. A window with a view or the promenade telescope at noon and at 23:00 (`lyud.Birds.Time 23`): sea by day, stars at night.
12. A bed: the screen goes dark, the view from the pillow at the ceiling, the clock on, he gets up by himself; again, and
    press W while lying: up at once, the view back, the hero visible.
13. The cinema (`Dev.Do interiors.open_all_hours`, a hall seat in P-CLUB2): he sits, the dark wide panel, subtitles one after
    another, the clock moving; E stops it. A home TV from standing: `Сначала надо сесть`; sit on the sofa: the TV is offered.
14. A shelf (10 min): the clock runs on over half a second, no hitch (`stat unit`), moves held meanwhile.
15. Talk to somebody in front of a shelf: E only advances the talk.
16. `P-BRIDGEHOUSE/gallery_bench_e1`: sit, stand up: on the gallery floor, not under it.
17. Developer panel, Quests: each city quest's stages jump to the right room, open, with the money.
18. Offscreen: `unreal/scripts/test_city_things.sh` (CITYTEST lines, PASS/FAIL).
19. A holiday (`lyud.Festival.Go new_year`): the club's board shows the holiday notice all day; `Dev.Do time.plus_day`: the
    next ordinary notice. `lyud.Weather.Preset rain`: the cloakroom asks for the wet raincoat.
20. Read two library shelves of different topics: the HUD says «Читательский формуляр: темы полок»: 1 из 15, then 2 из 15;
    `lyud.City.Collections`: the page with every set; save, load, the counts stay.
21. The café at 23:00 (`lyud.Birds.Time 23`): a chair says the chairs are up on the tables (no sitting), the bar is covered;
    with `interiors.open_all_hours` it sits again.
22. Darts (`Dev.Do interiors.game_aim`, or `lyud.City.Game <thing id>`): the panel with the bar, the marker running; E
    in the bright middle: `beat bull` in the log, the clink; five throws, then the result page and `game over on ...`.
23. Table tennis (`interiors.game_rally`): the ball crosses the bar, E as it reaches the window; too early: `beat early`.
    Against a regular the title says who; first to 3.
24. Chess at the club (sit at a board, E; or `interiors.game_duel`): the three moves on keys 1-3, the opponent's look under
    them; win (`interiors.game_win`): the stamp «Соперники за доской» 1 of 6, the regular's last word on the page.
25. `interiors.regulars`: the page of the ten regulars and what they play. Three chess games today, the fourth: the board's
    plain lines.
26. A switch (once a building has one) at night in a lit room: E, the room lamps go out (`switch ...: N lamps off`), again:
    on; walk into another building: back to the hours. A clock: `E — посмотреть на часы`, the page says the time.
24. Sitting (`Dev.Do interiors.sit_nearest`, or `lyud.City.Sit`): seated at the right height, feet on the floor; turn the
    mouse: the view stops about 110° either side of the seat's facing; W: up, beside or behind the chair if a table is in
    front, the view free again. Sit, save, load: standing, walking, not stuck.
25. A hotel or sanatorium night bed: E, the view settles toward the window; the chooser `Проснуться в 07:00`; D twice:
    09:00; E: dark, the clock at 9:00, the window, up, the bed's line. Again, and W in the chooser: up at once.
26. The library's reading pod (`P-LIBRARY/pod_1_01`): E sits and opens «Скворцы»: `стр. 1 из 4`; D, D, A, D, D: the last
    page, `Дочитано`, the stamp «Прочитанное: 1 из 6»; E puts it down; stand up, E again: page 1. A free newspaper: the second page
    differs tomorrow (`Dev.Do time.plus_day`). `lyud.City.Collections`: the Read list says where each was read.
