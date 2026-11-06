# The town's life

The town on the terraces above the beach (docs/city_plan.md) is lived in: about 900 townsfolk with mixed international
names (Томохико Амада, John McMartin, Pyotr Tolmachev, Rūta Petrauskaitė, ...), each with a flat, a household, a job or
school, a chronotype and likes. They run on the same simulation as the Academgorodok's residents (docs/people.md), so they
eat, sleep in their own beds, walk the stairs between the terraces, sit on the benches, go to the club, the restaurant,
the café, the market and the square, and switch their flats' window lights on in the evening.

## Files

| What | Where |
|---|---|
| Name database (43 origins, ru/en/ja forms, gendered surnames) | `tools/town/names.json` |
| Generator | `tools/town/town_life.py` |
| The town's places: terraces, stairs, flats, workplaces, public spots | `Content/PeopleData/town_places.json` (generated) |
| The townsfolk | `Content/PeopleData/townsfolk.json` (generated) |
| Their names and jobs in ru/en/ja | `Content/Localization/town/strings_<lang>.json` (generated, merged by `LyudText`) |
| Activities (shared with the residents) | `Content/PeopleData/activities.json` |
| Test outside the engine | `tools/peoplesim/run.sh town` |

Regenerate whenever the city plan, the buildings' flats, the public spaces or the restaurant's seats change:

```
python3 tools/town/town_life.py --summary            # inputs from the working tree
python3 tools/town/town_life.py --ref origin/wt/city-buildings --ref origin/public-spaces   # inputs not merged yet
```

Inputs: `Content/City/city_plan.json` (terraces, walls, stairs, wall dwellings, promenade units, plots with staff hints,
spots), `blender/export/city/<type>[__<plot>].json` (the buildings' flats, beds, entrances and windows),
`Content/PeopleData/town_plan.json` (squares, park, beach, promenade, fair stalls), `Content/Restaurant/seats.json`
(seats marked `hero` are left to the player) and `people.json` (the pool of models). Options: `--seed` (20261106),
`--wall-occupancy` (0.5), `--house-occupancy` (0.75), `--max-people` (900). The same seed gives the same town.

## The people

- **Households**: singles, couples, families with children, elderly couples, a parent with a child, flatmates. Capsules
  (one bed) hold one person. Members share a surname mostly, never a given name.
- **Chronotypes**: larks (up at 4:30–6, some watch the sunrise from the promenade), owls (up late, to bed after 1),
  night people (walk the town at night), the usual. Wake and bed times are jittered every day (`Spread`), and Friday and
  Saturday nights run later.
- **Jobs**: the plots' staff hints (shop, café, restaurant, clinic, office, school, workshop) and the promenade units
  become posts with shifts and rotating days off, filled preferably by matching chronotypes (bakers are larks, the bar's
  staff owls). The restaurant has pinned staff: Ilya Sokolov and Rūta Petrauskaitė (waiters), Rashid Mamedov (cook).
  Others work away (by train), at sea, or study. Children go to the town's school.
- **Likes**: grannies and grandpas sit on the benches, gamers go to the club, others fish, swim, run, paint, read.
- **Friends**: neighbours and colleagues, used for chat and gatherings.
- **Bodies**: for now the townsfolk borrow the current human models from a pool (many people of one look); a generator
  will give them their own. The story's people keep their unique bodies (one model, one person).

## How it works

- **Places**: every flat is a place with beds, a sofa, chairs and a desk; buildings without walkable interiors are
  *shells*: people inside aren't drawn and their slots sit at the entrance. Workplaces have staff slots tagged by role.
- **Ways**: inside the town box people walk on the terraces' levels and route over the 68 stairs (Dijkstra, flights
  walked as lines); outside it they use the old walk grid.
- **Window lights**: a flat is lit when someone at home is awake after dusk, plus a short bedside lamp before sleep. The
  game sends changes to whichever exists: `ULyudWindowLights::SetFlatLit(FString, bool)` (buildings, flat ids
  `<plot>/<storey-n>`) and `ULyudCityWindowsSubsystem::SetFlatLit(FName, bool)` (wall dwellings `W2-D001`), found by
  reflection, so neither branch depends on the other. `OnFlatLight` broadcasts the same.
- **Gatherings** (for the holidays): `SetGathering(Id, PlaceOrKind, FromHour, ToHour, Pull, Mode, FacePoint)` pulls
  people to a place or kind of place (`Mingle`, `Watch` facing a point, `Sit`, `Fair`); `ClearGathering(Id)`.
  `SetDayOff(true)` closes school and the offices (activities marked `"holiday": "off"`).
- **Restaurant**: `StaffOnShift("restaurant", "waiter")` lists who's working; `SetPresentedExternally(Id, true)` lets the
  restaurant drive a person's body itself (the sim keeps the schedule but stops drawing them).
- **Names**: `GetDisplayName(Id)` reads `npc.<key>.name` in the current language; `IsTownsperson(Id)`.

## In the game

- `lyud.People.TownBodies` (48): at most this many townsfolk are drawn, the nearest to the camera, from a pool per model
  (bodies are reused, not spawned and destroyed). `lyud.People.TownDrawDistance` (110 m).
- Town bodies use update-rate optimisation, are kept out of Lumen and distance fields, and are tagged `NoTalk`.
- The townsfolk aren't saved individually: the generator's seed recreates them; only the story's people's homes and
  memory go to the save.

## Tests

`tools/peoplesim/run.sh town` runs the town from Wednesday 04:00 for 36 hours and checks: everybody in their own flat,
most asleep at 03:00 but the owls up, sunrise watchers, night walkers, benches, staffed shops and offices, waiters at
noon, market sellers, pupils at school, Thursday as a day off (school empty), a gathering on the square drawing the
town, windows dark at noon, ~half lit at 21:00 and few at 03:00, meals, no overbooked slot, no way over 100 minutes and
the cost of a step (~0.3 ms for ~930 people). `run.sh test` (the residents) runs without the town.

## Known gaps

- Buildings' flats and the promenade units' windows come from branches not yet in main; rerun the generator after they
  merge.
- Townsfolk share models (Dima: use the current ones for now).
- People inside shells are invisible; only buildings with interiors show them indoors.
