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
python3 tools/town/town_life.py --ref origin/review/restaurant --ref origin/public-spaces --ref origin/wt/city-buildings   # inputs not merged yet
# --refs-first: the --ref branches win over the working tree (a newer plan there)
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

## Pinned guests: the sanatorium

The sanatorium «Корона»'s four guests of the table stories (docs/city_interiors.md "Stays") are townsfolk the generator
doesn't make: `python3 tools/town/sanatorium_guests.py` adds them to its output (idempotent; run it after every
regeneration, `--check` says whether the files are up to date). Pyotr Gromov (78) and Sonya Mironova (16) at the window
table, Boris Tamm (81) and Nikolai Sazonov (80) at the chess table; each has a room on the sanatorium's first floor
(`P-SANATORIUM/1-01..04`), goes to bed by the curfew and gets up early. Three duties a day (`eat_sanatorium_breakfast|lunch|supper`, one activity a meal since the life finds a duty by its activity; pose eat:
breakfast 8:30, lunch 13:30, supper 18:30, 45 minutes) at the walked place `P-SANATORIUM-DINING`: in by the sanatorium's
door, one slot a guest at his or her own chair (the city things `dining_table_1`, `dining_1_c`, `dining_table_2`,
`dining_2_c`: the acts `san_seat_<name>`). The rest of the day they are free like anybody (benches, the sea air, the club's
chess for the two old friends). A slot with `"thing"` holds that city thing while somebody sits or sits down there
(`UPeopleLifeSubsystem::UpdateThingHolds`, once a second, `ULyudCityThings::ClaimThing` / `ReleaseThing`): the hero gets
"taken" instead of sitting on a guest. `tools/peoplesim/run.sh town` checks them at lunch on Wednesday.

## The new activities

Item 26 of `docs/interactives_backlog.md`: the town uses what the evening of 2026-11-20 added. The places, the activity
rows, the likes and the postman are put in by `tools/town/town_activities.py`, an add-on in the manner of
`sanatorium_guests.py`: it works on the generated files as they stand, is safe to run twice, `--check` says whether it
has been run, and `town_life.py` runs it at the end (regenerating the town rerolls everybody, so after a plan change
the add-on is what brings these back).

| activity | who, when | where |
|---|---|---|
| `cinema_day`, `cinema_evening` | anybody who likes it (the curious, the warm), going in before the 15:00 and 19:00 sessions | `P-CLUB2-CINEMA`: 145 of the hall's seats, each walked to along its aisle; six of them are the hero's seats and are held through the slot's `thing` |
| `san_cure` | 60 and over and the sanatorium's guests, mornings but Sunday | `P-SANATORIUM-PUMP`: the pump room's bench and stool, the lobby's benches; the drink manner with a cup |
| `dominoes` | 58 and over, 15:00 to 20:00, a talk | `P-SANATORIUM-LOUNGE`: the lounge's four seats |
| `read_paper` | readers, 7:00 to 11:30 | any bench; the newspaper manner is forced |
| `skip_stones` | children first, the bold; by day, wind under 7.5 m/s, above 3 °C | the beach's stand slots |
| `tend_allotment`, `water_allotment` | the four keepers of the allotment's beds, morning and evening, not in frost | `P-T2H2-ALLOTMENT`: two slots a bed, each with its `owner` |
| `play_piano` | one in three of the musicians and teachers, one in thirty of the rest; 18:00 to 20:30, seldom | `P-HALL-STAGE`: the stool at the club hall's piano |
| `post_round` | the postwoman (`t_hernandez_lyudmila`), a duty 8:30 to 12:00 but Sunday, rain or not | `S-POST-ROUND`: 37 house doors, one after another |

What the sim learnt for them (`PeopleSim`):

- an activity's `temp: [min, max]`: outdoors it is not chosen outside these degrees (like `wind`);
- a slot's `owner`: a person's key, nobody else takes the slot (a missing person leaves it empty);
- a slot's `path`: points walked after the place's own path and before the slot's approach (a seat's aisle);
- a place's `open: 1`: a place with a door that is under the sky all the same (the allotment), so rain and cold count.

The postwoman is not the old postman of `docs/rounds.md`: he is retired and hands the hero his parcels, she has the
round of the doors now.

Not done: the pianist is silent (the instruments' synth has no way to be played by somebody else yet), friends do not
walk along with the hero, and the cinema's house does not follow the programme (people sit through the two fixed
sessions whatever is on).

## The night jobs

Item 29 (`docs/city_night.md`): `tools/town/town_night.py`, an add-on like `town_activities.py` (and run after it by
`town_life.py`), gives three residents without work the shut rooms' jobs:

| activity | who, when | where |
|---|---|---|
| `mop_floor`, `mop_floor_late` (the day's second building) | cleaner A: the café 7:00-7:50, the Rassvet 8:05-9:55; cleaner B: the store 7:00-8:50 (Mon-Sat), the library 21:05-22:25 | `P-CAFE-CLEAN` (up the spiral stair), `P-CLUB2-CLEAN`, `P-STORE-CLEAN`, `P-LIBRARY-CLEAN`: floor spots in round order; the mop manner with the bucket by them |
| `night_watch` | the watchman (occupation `guard`), 20:30-6:30, sleeps by day | `P-STORE-WATCH`: the store's ground floor, up the runner and out to the side halls; the torch manner and its beam |

A round in an indoor place now goes from spot to spot by the place's own ways (`Route` within the place) instead of the
street grid. Who has a job is kept in townsfolk.json (`night_job`), so a rerun keeps the same people.

## How it works

- **Places**: the restaurant (its `seats.json` has an entrance) is walked: diners and staff go in by its front door to
  their seats and posts. Every flat is a place with beds, a sofa, chairs and a desk; buildings without walkable interiors are
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
- The nearest get a body, those who have one keep it a little longer (no flicker in a crowd), and a townsperson never
  shows near the story person whose model they borrow.
- After a load or a clock jump the townsfolk are placed 120 a step (no long frame); snapped sleepers have no bedside lamp.
- Town bodies use update-rate optimisation, are kept out of Lumen and distance fields, and are tagged `NoTalk`.
- They can't be talked to, but they greet the player, say lines of their own and have short exchanges over their
  heads (docs/town_talk.md).
- The townsfolk aren't saved individually: the generator's seed recreates them; only the story's people's homes and
  memory go to the save.

## Tests

`tools/peoplesim/run.sh town` runs the town from Wednesday 04:00 for 36 hours and checks: everybody in their own flat,
most asleep at 03:00 but the owls up, sunrise watchers, night walkers, benches, staffed shops and offices, waiters at
noon, market sellers, pupils at school, Thursday as a day off (school empty), a gathering on the square drawing the
town, windows dark at noon, ~half lit at 21:00 and few at 03:00, meals, no overbooked slot, no way over 100 minutes and
the cost of a step (~0.3 ms for ~930 people). `run.sh test` (the residents) runs without the town.

`tools/peoplesim/run.sh week` (~2 min) first checks the generated data (`tools/peoplesim/check_town_data.py`: unique
keys and ids, homes that exist and sleep their household, duties at places of the right kind with a staff slot for
everybody on shift, a night of 7 to 10.5 hours that no duty cuts into, friends and counter sellers who exist, names and
jobs in ru/en/ja in each one's script), then runs the town from the new game (Tuesday 20 October 16:00) to Monday 13:00
through what the game does to it (`tools/peoplesim/town_week.h`): a clock jump on Wednesday, Thursday a public holiday,
a save loaded on Friday 12:30 (twice: the same town two hours on), a gathering on Saturday cleared early, saves loaded
on Monday at 02:00 and 03:00, steps of a second and of four (a fast clock), over the night European clocks go back (the
game's calendar has no daylight saving). Every minute: no NaN, everybody near the map's places, nobody in two slots, no
slot overbooked, whoever is at an activity at its slot; every day everybody sleeps 4 h or more at home; nobody stays
at one place (not home) past its hours, idles 30 min or starts a new activity every few minutes; every job of every
workplace staffed while open, nobody at work outside their shift (a night shift's small hours belong to the day it
began); pupils at school on school days and nobody on the weekend; every trip ending at its slot; a step's cost.
`LYUD_WEEK_TRACE=<key>` prints one person's week minute by minute. Without LFS the walk grid (`animal_world.bin`) is
only a pointer: run.sh builds it into `Saved/PeopleSim/animals` first.

The townsfolk's wake and bed hours are fitted to their duties (`fit_day` in the generator); `python3
tools/town/town_life.py --refit-days` refits the town already made without making it again (its people's keys are
their models' in `town_looks.json`).

## Known gaps

- Buildings' flats and the promenade units' windows come from branches not yet in main; rerun the generator after they
  merge.
- Townsfolk share models (Dima: use the current ones for now).
- On merge with perfhunt keep its GroundCache and its door fix (hidden pooled bodies don't hold doors open).
- People inside shells are invisible; only buildings with interiors show them indoors.

How they do it (characters, habits, gestures, things in the hands, noticing the player, the weather in their choices): docs/town_manners.md.
