# Town backlog: interiors, things to do, small quests

The work list for making the town of Lyudmilovsk alive inside its buildings. It never runs out on purpose: when a tier
is done, the next pass on the same building goes deeper (more rooms, more to do, voices, stories). How the parts fit:
[city_interiors.md](city_interiors.md). Buildings: [city_buildings.md](city_buildings.md). Plan: [city_plan.md](city_plan.md).

Status marks: `[ ]` not started, `[~]` in work, `[x]` done and checked in the Blender preview, `[U]` also checked in
Unreal on the Mac. Whoever takes an item writes their mark and the date here.

## How one building pass goes

1. **Furnish** (`blender/city/buildings/<type>.py`, the shared `interior_kit.py`): every enterable room gets furniture
   that says what the room is, dressing on the surfaces, wall pictures from the atlas. Check with
   `shoot_interior.py` (eye-level shots and a plan view) and the lint of `build_city.py` (0 stretched UVs).
2. **Things**: every object the hero would want to touch gets a `B.thing(...)`.
3. **Content**: acts, lines in ru/en/ja, shop stocks, in `tools/city/content/`; `python3 tools/city/interiors.py --check`.
4. **Life**: spots for the townsfolk (`B.spot`) match the new furniture; `tools/town/town_life.py` rerun.
5. **Unreal** (Mac): build glb, `unreal/scripts/import_city.sh`, `test_city_things.sh`, shots, a bench of the plot
   against main (perf checklist: alone, frame and memory).
6. **Later passes**: a small quest, voiced lines of the staff, sounds of the room, a festival variant.

## P0: the framework (everything else stands on it)

- [~] Contract and tools: `docs/city_interiors.md`, `B.thing`, `shoot_interior.py` (2026-11-13)
- [~] Furniture kit `interior_kit.py` (~70 pieces) and the picture atlas `CT_Atlas`
- [~] `ULyudCityThings` + `CityThingsModel.h` + `tools/citysim`
- [~] Data compiler `tools/city/interiors.py`, default acts, first content layer, developer-mode teleports
- [ ] First Mac build and import; fix what the compiler and the editor say
- [~] Sitting and lying poses for the hero on any seat or bed of a building (2026-11-13, not compiled yet): sitting is the
  real seated pose (`SitOn`: checked floor, a free spot to stand up to, a look cone, never stuck: anchor gone, load while
  seated, re-sit, held seats); lying is still the pillow camera (no lying pose for the hero): a slow settle toward the
  window, the sleep-until-hour chooser on `sleep` beds. Open: a real lying pose (needs an animation for the hero's skeleton),
  a `cone` prop per seat where 110° is wrong
- [~] The reading page (2026-11-13, not compiled yet): `SLyudCityBook`, `texts` with pages and alternatives, E/A/D and
  arrows, the page remembered, minutes per page, a stamp per text, the `read` line condition, the Read list, six texts on
  shelves, boards and the daily paper. The "screen" (subtitles) was done earlier; film stills and exhibit pictures are not
  (no pictures in the view yet)

## P1: public buildings, first pass (the places a new player walks into)

| Plot | Building | Rooms and what to do there | Status |
|---|---|---|---|
| P-LIBRARY | capsule_library | hall with shelves by topic, lending desk (bird guide for the ornithologist quest), card catalogue, reading pods with different moods on three levels, periodicals table, children's corner, a quiet librarian's office | [x] content 2026-11-13: lending desks sell the bird guide, 12 shelf topics + pods, catalogue, rules; thin: the office is only a staff door |
| P-STORE | department_store | two floors of departments (groceries, fabrics, clothes, shoes, toys, records and radios, household, stationery, sports and fishing), tills, a cloakroom, a café corner, a lost-and-found | [x] content 2026-11-13: 8 department shops, cafeteria, shoes (talk), toy house, records/radio catalogue; thin: no clothes, toys or records to buy (no items) |
| P-CAFE | saucer_pavilion | café «Тарелка»: bar, tables by the ring window, jukebox, an ice-cream counter, telescope on the gallery | [x] content 2026-11-13: bar and ice-cream counter shops, jukebox, telescope |
| P-CLUB2 | workers_club | house of culture «Рассвет»: foyer with ticket office and buffet, cinema hall (film sessions by the clock), hobby rooms (chess, radio, dance), cloakroom, poster wall | [x] content 2026-11-13: cinema (projectionist), tickets (clerk), buffet, cloakroom, chess, dance, radio bench; thin: the film is lines only (no screen) |
| P-CLUB3 | cog_club | club «Шестерня»: drum foyer, billiards, table tennis, model-makers' workshop, photo lab, small stage, library corner | [x] content 2026-11-13: billiards, table tennis, darkroom, arcade, photo booth, stage, library desk (talk); thin: the club library lends nothing |
| P-T3SPORT | culture_block | sports hall (basketball, wall bars, mats), weights room, pool lanes if the shell allows, changing rooms, art workshops in the brick shed (easel, potter's wheel) | [x] content 2026-11-13: apparatus incl. vaulting horse and mats, weights, bikes, art shed, reception (talk), buffet; thin: no pool lanes |
| P-SCHOOL | school | classroom (desks, blackboard), biology room with a skeleton, gym, canteen, the wall newspaper, a music room with a piano | [x] content 2026-11-13: lesson (teacher), blackboards, biology room, gym, music room; thin: no canteen counter |
| P-CLINIC | clinic | reception, waiting row, doctor's room (scales, eye chart, couch), pharmacy kiosk, health posters | [x] content 2026-11-13: registry (talk), pharmacy shop, scales, eye chart, check-up, couches nap; thin: the pharmacy has no medicine items |
| P-HALL | shell_chapel_hall | hall «Своды»: foyer with exhibition, stage hall, dance room with bar, kitchen | [x] content 2026-11-13: stage, choir, pianos, dance, bar, cloakroom, kitchen (stove, sink, cooking) |
| P-MARKET | shell_market | 16 stalls with real goods, scales, the market office board, a tea stand | [x] content 2026-11-13: 16 stalls, tea counter, office (talk), scales; thin: butcher and fishmonger are talk only |
| P-FAIR | wave_canopy | five kiosks with stocks, benches, the fair notice board | [x] content 2026-11-13: five kiosk shops by trade, notice board; the fair games declared again on the kiosks (2026-11-13, stories pass): ring toss on the ice-cream kiosk's bottles, lottery on the press kiosk's shelf, fortune globe and shooting gallery at the toy kiosk, strongman by the snack kiosk's crates; thin: no strongman/shooting-gallery models of their own |
| P-SANATORIUM | round_sanatorium | reception, dining hall, mineral-water pump room, a treatment room, the winter garden; one guest room to rest in | [x] content 2026-11-13: reception shop, dining buffet, pump room, treatment couches nap, piano, cloakroom; stories pass: `couch_b` is a paid night (80 rub., the receptionist, till 7) |
| P-PIER | hyperboloid_tower | keeper's room, the lamp gallery with a telescope, the log book | [x] content 2026-11-13: log book, radio, telescope, keeper's bed (nap), gallery views |
| P-KIOSK1..6 | bus_stop_kiosk | six kiosks, each its own trade (papers, ice cream, tobacco, flowers, tickets, repairs) | [x] content 2026-11-13: papers, postcards, timetable, lost-and-found; stories pass: 1 papers, 2 ice cream (shop, freezer chest), 3 tobacco (shop), 4 flowers (talk, buckets), 5 cable-car information (talk, own timetable), 6 repairs (talk, tray of unclaimed things), each its own price board; thin: the six models still all look like the newspaper kiosk, flowers/cable car/repairs sell nothing (no items) |
| P-LIFEGUARD1,2 | lifeguard_tower | cabin with binoculars, flags board, first-aid box, the log of water temperature | [x] content 2026-11-13: binoculars, flags board, first aid; thin: the temperature log reads as a view |
| P-WAVE | wave_tower (mixed) | shopping arcade under the tower: six shops, a hairdresser, a savings bank counter | [x] content 2026-11-13: bakery, records (talk), bookshop, hardware (bird feed, feeders), savings bank (talk), salon, fabrics, toys; the plot has no staff, so they run unstaffed 9-20 Mon-Sat (`@open`) |
| P-T4H2 | ribbon_corner_block (mixed) | ground-floor shop and laundry, roof solarium | [x] content 2026-11-13: corner grocery (unstaffed 9-20), laundry, roof solarium |
| P-T2H2, P-T3H3 | commune_slab (mixed) | communal hall: canteen, reading corner, the house committee board | [x] content 2026-11-13: canteen (unstaffed 8-19 Mon-Sat), reading shelves, house committee board, laundry, gym corner |

## P2: houses (lobbies, stairs, one open flat each)

Every residential building gets a lobby that feels lived in (mailboxes, a notice board, a concierge's chair, prams,
bikes, plants, a radio), and ONE flat with an open door: a named resident from the town-life data lives there, the
furniture tells their story, and they have something small for the hero to do.

- [ ] P-SERPENTINE serpentine_slab: lobby by lobby (6 entrances), the long gallery, a pensioner's flat with an aquarium
- [ ] P-UNITE unite_block: the shopping street at mid-height, the roof terrace (paddling pool, running track), a duplex flat
- [~] P-CAPSULE, P-T5H1 capsule_tower: lobby, one capsule (bed, round window, tape recorder). Stories 2026-11-13: Ilmar Singh, radio
  amateur caretaker (quest `home_singh`, answered from the cluster); Nora Kravets, a student (quest `home_kravets`, tides from the lighthouse log)
- [~] P-T1H4 capsule_cluster: bridges, a paired capsule studio of a radio amateur. Stories 2026-11-13: Gunnar Okafor (quest
  `home_okafor`: a tune off the air found on the Saucer's jukebox)
- [ ] P-T1H1, P-T2H1 cube_houses: one tilted cube house inside (three levels, sloping walls)
- [~] P-T1H2 cobogo_house: garden under the pilotis, the free curved stair, a flat with a breeze-block shadow pattern. Stories
  2026-11-13: Viktor Sergeyevich Holm, retired navigator (quest `home_holm`: the forecast for his watch log)
- [~] P-T1H3, P-T3H1 twin_cylinder_house: the studio drum with the hexagon windows (a painter's workshop). Stories 2026-11-13:
  Valentina Tadesse's fish (quest `home_tadesse`), Mads Okonkwo's pottery and Emiliano's clay whale (quest `home_okonkwo`)
- [~] P-T1H5 origami_row: one folded house (a family with three children). Stories 2026-11-13: the Santos family (quest
  `home_santos`: Venus through the loft telescope)
- [ ] P-T3H2 stacked_boxes: canyon stair-streets, a roof-garden flat
- [~] P-T4H1 pilotis_curve_slab: lobby with the azulejo panel, the caretaker's room. Stories 2026-11-13: Lev Markovich Okada,
  retired painter (quest `home_okada`), the caretaker Toivo Asanov
- [ ] P-BRIDGEHOUSE stacked_beam_tower: lobby, the gallery view room at 32 m
- [~] P-RISER1, P-RISER2 terrace_riser_housing: terrace gardens, one maisonette. Stories 2026-11-13: the Girmas' sunspots
  (quest `home_girma`), the seamstress Rimma Aoki (quest `home_aoki`)
- [ ] Wall dwellings W2..W5 (520 doors): ten "open door" interiors spread along the walls

## P3: the promenade and its 32 units

The seawall W1 holds 32 shop units (cafés, a fish shop, a bakery, a bookstall, a barber, a photo studio, a workshop,
rental, souvenirs, a dumpling house, a cheburek house, a beer bar). Each is now a room behind the wall's face
(`promenade_unit`, [city_interiors.md](city_interiors.md) "Promenade units").

- [x] One room behind each kind of unit (12 dressings of `promenade_unit`, placed at all 32 units; 2026-11-13). Unreal
  (`[U]`) waits for the Mac: terrain + world mesh rebuild, import, place
- [~] Stocks and acts for each kind: shops and services on the counters (the life sim's roles), the barber's haircut,
  the photo studio's portrait (`photo_portrait`), the photo booth, darts, the workbench (2026-11-13); the photo desk's own
  talk (`photo_desk`), the bookstalls' sale tables (`read_poetry_sale`). Still to do: the portrait as a diary item, the
  haircut changing the hero's look
- [ ] Rental: deck chairs, a rowing boat ticket (fishing thread), bicycles (bikes thread): the room has them on show,
  the counter only talks (`rentals`)
- [ ] Unit windows: see into the lit room from the promenade (today the wall's opaque shop-glass card and the lit pane)
- [ ] Signboards for the trades the atlas has no word for (fish, rental, workshop, souvenirs: `sign_*` cells in
  `tools/city/atlas.py`)

## P4: small quests and stories (after the rooms exist)

- [~] First batch of side quests from the content layer (town tour, a day at the library, a film at «Рассвет», chess at
  the club, a check-up at the clinic, the photo booth, the department store errand, mineral water, a school lesson); ten
  home quests `home_*` (2026-11-13, one per open home, 2-3 stages, `do <story act>`/`visit`)
- [~] One story per public building told by its boards and shelves over several game days (notices change by date).
  2026-11-13: the regulars' notices (`city.reg.*`, after the generic ones) at the library (a book back from 1968), «Шестерня»
  (the chess clock), «Тарелка» (Kesha the parrot), the market (the jam contest), the department store (the yellow
  umbrella). Still open: the notices cycle, they don't change by date (needs the game to pick lines by day); the other
  public buildings; the lighthouse keepers' room has its own lines but no quest
- [ ] Staff talk: each counter's staff has greetings and three topics (voice pinned per the voice rule)
- [~] Festival variants of interiors (New Year tree in the store, lanterns in the café) tied to `ULyudFestivalSubsystem`.
  2026-11-13: lines by holiday (`"festival": "any" | "none" | <id>`) and by weather (`"weather": "rain"...`) in the model,
  citysim, interiors.py and the docs; holiday notices on 13 public boards and counters (club, cinema, prices, school wall
  newspaper, clinic, lost and found, timetable, counters, cashier, cloakroom, tickets; New Year, lantern night and sea
  festival ones), rain and snow lines at the cloakrooms and counters. Still open: dressing (no geometry), the building agents'
  own boards
- [~] Collections (2026-11-13): stamps in the model (`collect` on an act, a line or a thing; `have`/`count` line conditions;
  saved), the HUD note "3 of 12", the page `lyud.City.Collections`, developer grant/clear; four sets: library shelf topics
  (15), films at «Рассвет» (its programme), jukebox records, bus-stop kiosks (6); chess opponents beaten (`chess_wins`, 6, a won
  chess game against a regular). Still open: cinema ticket stubs as items, a reward for a complete set, a page in the diary
- [~] Mini-games (2026-11-13): `game` on an act, three types (aim, rally, duel) played in the model (citysim), a panel,
  win/lose/draw lines, stamps, prizes, a daily limit, `Notify("win")`; on chess, billiards, darts, table tennis, table
  football, the fair's gallery, ring toss and strongman, the arcade, the hoop, and new checkers, dominoes, badminton acts
  (`what=`). Ten regulars of the clubs (real residents) play against the hero, each with a style, a skill and words. Not
  built on the Mac yet. Still open: the x_ chess tables that don't `like` chess (`tarelka_chess`, `sanatorium_chess`), no
  thing says `what=checkers|dominoes|badminton` yet, the regulars are not seen sitting at the table (the life sim's side), a
  ladder or a weekly tournament

- [~] Townsfolk use the things (2026-11-13): `FindFreeThing`, `ClaimThing`/`ReleaseThing`, `GetThingsOfPlot` on
  `ULyudCityThings` (docs "API for townsfolk"); the hero is refused a held seat. The people's life side is the npc-manners
  thread's
- [x] Side quests from several content files (`quests` in any content file, `x_<building>.json`), 2026-11-13

## P5: quality passes that never end

- [ ] Unique wall art per building from the atlas (no picture repeated inside one room)
- [ ] Room sounds (reverb by room size, clock ticks, a radio behind a wall, the store's tannoy)
- [~] Wall clocks and light switches (2026-11-13): `look_clock` with `{time}` lines, `what=switch` turns the room lamps
  (`B.lamp`) round it on or off till the hero leaves the plot. Still open: no building declares a clock or a switch yet
  (the building modules' things), no ticking sound (no file), the lamp's fitting does not change, only its light
- [~] Night state of public rooms (closed: chairs on tables, a single lamp, the cleaner at work 7-10). 2026-11-13: by data
  (`closed` on a rule or an act, `closed_by` per plot, type or plot kind): the café, both clubs, the library, the market
  and the store say what is closed instead of refusing (149 things); still open: geometry (chairs really on the tables), the
  single lamp, the cleaner
- [ ] Seasonal dressing (wet umbrellas by doors on rain days, coats on racks in winter)
- [ ] Townsfolk use every new seat, table and counter (spots follow the furniture)
- [ ] New goods with their own icons (needs the paid icon generation to be approved): stationery, toys, records,
  fabrics, pharmacy goods, market vegetables, bakery, flowers
- [ ] Perf audit per plot after each pass (triangles of detail meshes, lamps in reach, things per plot)

## Notes for whoever works on a building

- Use only item ids that exist until icons for new ones are approved.
- Every text in ru, en and ja (localization skill); Russian is the source.
- Check UV overlap and visible artefacts on every asset before showing it.
- Furniture never blocks a door, a stair or a townsperson's spot; run `interior_kit.check(B)`.
- Keep ids of things stable: saves and quests refer to them.
