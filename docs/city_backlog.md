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
- [ ] Sitting and lying poses for the hero on any seat or bed of a building
- [ ] The reading page (title, text, next) and the "screen" (film stills, exhibits)

## P1: public buildings, first pass (the places a new player walks into)

| Plot | Building | Rooms and what to do there | Status |
|---|---|---|---|
| P-LIBRARY | capsule_library | hall with shelves by topic, lending desk (bird guide for the ornithologist quest), card catalogue, reading pods with different moods on three levels, periodicals table, children's corner, a quiet librarian's office | [ ] |
| P-STORE | department_store | two floors of departments (groceries, fabrics, clothes, shoes, toys, records and radios, household, stationery, sports and fishing), tills, a cloakroom, a café corner, a lost-and-found | [ ] |
| P-CAFE | saucer_pavilion | café «Тарелка»: bar, tables by the ring window, jukebox, an ice-cream counter, telescope on the gallery | [ ] |
| P-CLUB2 | workers_club | house of culture «Рассвет»: foyer with ticket office and buffet, cinema hall (film sessions by the clock), hobby rooms (chess, radio, dance), cloakroom, poster wall | [ ] |
| P-CLUB3 | cog_club | club «Шестерня»: drum foyer, billiards, table tennis, model-makers' workshop, photo lab, small stage, library corner | [ ] |
| P-T3SPORT | culture_block | sports hall (basketball, wall bars, mats), weights room, pool lanes if the shell allows, changing rooms, art workshops in the brick shed (easel, potter's wheel) | [ ] |
| P-SCHOOL | school | classroom (desks, blackboard), biology room with a skeleton, gym, canteen, the wall newspaper, a music room with a piano | [ ] |
| P-CLINIC | clinic | reception, waiting row, doctor's room (scales, eye chart, couch), pharmacy kiosk, health posters | [ ] |
| P-HALL | shell_chapel_hall | hall «Своды»: foyer with exhibition, stage hall, dance room with bar, kitchen | [ ] |
| P-MARKET | shell_market | 16 stalls with real goods, scales, the market office board, a tea stand | [ ] |
| P-FAIR | wave_canopy | five kiosks with stocks, benches, the fair notice board | [ ] |
| P-SANATORIUM | round_sanatorium | reception, dining hall, mineral-water pump room, a treatment room, the winter garden; one guest room to rest in | [ ] |
| P-PIER | hyperboloid_tower | keeper's room, the lamp gallery with a telescope, the log book | [ ] |
| P-KIOSK1..6 | bus_stop_kiosk | six kiosks, each its own trade (papers, ice cream, tobacco, flowers, tickets, repairs) | [ ] |
| P-LIFEGUARD1,2 | lifeguard_tower | cabin with binoculars, flags board, first-aid box, the log of water temperature | [ ] |
| P-WAVE | wave_tower (mixed) | shopping arcade under the tower: six shops, a hairdresser, a savings bank counter | [ ] |
| P-T4H2 | ribbon_corner_block (mixed) | ground-floor shop and laundry, roof solarium | [ ] |
| P-T2H2, P-T3H3 | commune_slab (mixed) | communal hall: canteen, reading corner, the house committee board | [ ] |

## P2: houses (lobbies, stairs, one open flat each)

Every residential building gets a lobby that feels lived in (mailboxes, a notice board, a concierge's chair, prams,
bikes, plants, a radio), and ONE flat with an open door: a named resident from the town-life data lives there, the
furniture tells their story, and they have something small for the hero to do.

- [ ] P-SERPENTINE serpentine_slab: lobby by lobby (6 entrances), the long gallery, a pensioner's flat with an aquarium
- [ ] P-UNITE unite_block: the shopping street at mid-height, the roof terrace (paddling pool, running track), a duplex flat
- [ ] P-CAPSULE, P-T5H1 capsule_tower: lobby, one capsule (bed, round window, tape recorder)
- [ ] P-T1H4 capsule_cluster: bridges, a paired capsule studio of a radio amateur
- [ ] P-T1H1, P-T2H1 cube_houses: one tilted cube house inside (three levels, sloping walls)
- [ ] P-T1H2 cobogo_house: garden under the pilotis, the free curved stair, a flat with a breeze-block shadow pattern
- [ ] P-T1H3, P-T3H1 twin_cylinder_house: the studio drum with the hexagon windows (a painter's workshop)
- [ ] P-T1H5 origami_row: one folded house (a family with three children)
- [ ] P-T3H2 stacked_boxes: canyon stair-streets, a roof-garden flat
- [ ] P-T4H1 pilotis_curve_slab: lobby with the azulejo panel, the caretaker's room
- [ ] P-BRIDGEHOUSE stacked_beam_tower: lobby, the gallery view room at 32 m
- [ ] P-RISER1, P-RISER2 terrace_riser_housing: terrace gardens, one maisonette
- [ ] Wall dwellings W2..W5 (520 doors): ten "open door" interiors spread along the walls

## P3: the promenade and its 32 units

The seawall W1 holds 32 shop units (cafés, a fish shop, a bakery, a bookstall, a barber, a photo studio, a workshop,
rental, souvenirs, a dumpling house, a cheburek house, a beer bar). Today they are a door and a window.

- [ ] One room behind each kind of unit (12 kinds), built once and reused with different dressing
- [ ] Stocks and acts for each kind (a haircut passes an hour; the photo studio gives a portrait for the diary)
- [ ] Rental: deck chairs, a rowing boat ticket (fishing thread), bicycles (bikes thread)

## P4: small quests and stories (after the rooms exist)

- [~] First batch of side quests from the content layer (town tour, a day at the library, a film at «Рассвет», chess at
  the club, a check-up at the clinic, the photo booth, the department store errand, mineral water, a school lesson)
- [ ] One story per public building told by its boards and shelves over several game days (notices change by date)
- [ ] Staff talk: each counter's staff has greetings and three topics (voice pinned per the voice rule)
- [ ] Festival variants of interiors (New Year tree in the store, lanterns in the café) tied to `ULyudFestivalSubsystem`
- [ ] Collections: library stamps, cinema ticket stubs, chess opponents beaten, records heard on the jukebox

## P5: quality passes that never end

- [ ] Unique wall art per building from the atlas (no picture repeated inside one room)
- [ ] Room sounds (reverb by room size, clock ticks, a radio behind a wall, the store's tannoy)
- [ ] Night state of public rooms (closed: chairs on tables, a single lamp, the cleaner at work 7-10)
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
