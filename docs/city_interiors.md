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
* `Content/UI/quests.json`: the side quests of `tools/city/content/quests.json`, one line each, marked
  `"from": "tools/city/interiors.py"`, replaced on every run; the other quests stay byte for byte. Quest events used:
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
| `lines` | keys of texts shown in order, wrapping round (pages of a shelf, notices of a board, what happened at a game) |
| `title` | key of the page/trade screen title (every `buy_<shop>` act: the shop's title) |
| `minutes` | game minutes that pass (0: none). With `fade` the screen fades out and in |
| `until` | the hour the act ends at instead of `minutes` (sleep till 7; -1: none); `minutes` is the fallback. The game does not read it yet: `tools/citysim/until.patch` (model + checks) adds it |
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
| `screen` | a full-screen picture to show while it lasts (none yet) |

Content tables (`tools/city/content/*.json`, each with any of `acts`, `shops`, `rules`, `defaults`,
`defaults_by_plot_kind`; a key defined in two files is an error): `acts.json` (defaults per kind, generic acts,
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

* `ULyudCityThings` (tickable world subsystem, `Public/City/LyudCityThings.h`, `Private/City/LyudCityThings.cpp`): loads
  `City/interiors.json` at world begin play (no file: no things, no tick). Five times a second, and only while the hero is
  within 30 m of the box round all things, the model picks the thing in view from a 4 m grid (reach 2.6 m from the eyes, in
  front, near the line of sight, on the eye's floor); one trace checks no wall is between; a door in view wins (doors poll E
  too). Not as a bird, on a bike, seated (restaurant, train, a chair), with a screen, shop or menu up
  (`ULyudInventoryUI::CanOpen`). The prompt goes through `ULyudInventoryUI::SetPrompt` (built only when the thing changes;
  with a price `city.act.prompt_price`). E (polled) runs the act:
  * refused (`city.act.closed|no_money|needs_item|no_staff|daily_done`): the reason on the prompt line for 2.5 s;
  * `price` via `ULyudInventory::Spend`, `give` via `Add` (unknown items skipped, logged at load);
  * `lines`: a page (`Private/City/SLyudCityPage.*`, HUD style, hit-test invisible, takes no input) with the title, the
    line and `E — next` / `E — close`; E turns through the act's lines once, walking 2.5 m away or any screen shuts it;
  * `minutes`: `UGameCalendar::SetNow(Now + minutes)`; with `fade` a camera fade to black, the jump, a fade back;
  * `shop`: `ULyudInventoryUI::OpenShop` with the `shops` stock, title = the act's `title` (default `city.shop.title`);
    `needs_staff` is asked of `UPeopleLifeSubsystem::StaffOnShift(plot, role)` (role `*`: anyone);
  * `pose` `sit`: `AHeroCharacter::SitOn` on one pooled anchor (an actor at the origin; the seat frame from the thing's
    point, yaw, `height` and a floor trace); stands up the usual way. `lie` sits on the bed's edge (no lying pose yet:
    TODO in `Sit`). Activity poses play nothing yet; `screen` shows the lines on the page (TODO in `Run`);
  * `sound`: `ULyudSoundscapeSubsystem::PlayEffect(id)` (an id without a pool plays nothing);
  * quests: `Notify(event or "do", target or act id)`, plus `Notify("visit", plot)` at the first use in a plot each game day.
* Default acts when a thing has none: seat sits; bed rests 2 h (`rest_hours`) with a fade; shelf reads 10 min (`read`/topic);
  board looks (`look`/topic); counter needs its `role` (`talk`/role); machine 5 min, play 20, sport 15, craft 30, screen
  30 with a fade (event = the kind's verb, target = `what`); take gives its `item` once a day; door shows a caption. Seats,
  beds and doors ignore the plot's hours; everything else keeps them.
* Rules in `Private/City/CityThingsModel.h` (`LyudCity::FModel`: `Load`, `Pick`, `Check`, `Run`, `NextLine`, `Nearest`,
  `SaveState`/`LoadState`, `ResetToday`/`ResetState`), engine-free; `tools/citysim/run.sh` checks it on
  `tools/citysim/fixture.json` and on the real file when it is built. Save record "CityThings" (pages, today's counts,
  uses, plots visited); a New Game starts it over.
* Console: `lyud.City.Things` (count, the nearest five), `lyud.City.Do <thing id>` (run it from anywhere; sitting only
  when the hero is within 4 m), `lyud.City.Things.Show 0|1` (labels within 15 m). Developer mode (when `Dev/LyudDev.h`
  exists), group `interiors`: things nearby, use the nearest, reset today's limits, 500 rub.
* Perf ([perf checklist](../README.md)): no per-frame allocation, no actors or components per thing, nothing past the town box.

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
10. Offscreen: `unreal/scripts/test_city_things.sh` (CITYTEST lines).
