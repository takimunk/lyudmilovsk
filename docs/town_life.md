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

## The sanatorium's guests

The sanatorium «Корона» is a place people come to (item 50 of `docs/interactives_backlog.md`): a dozen guests, two by
two, a fortnight each. They are townsfolk the generator doesn't make: `python3 tools/town/sanatorium_guests.py` adds
them to its output (idempotent; run it after every regeneration, `--check` says whether the files are up to date).

| pair | week of the cycle | table, rooms | who |
|---|---|---|---|
| tamm | 0 | chess table, 1-03 / 1-04 | Boris Tamm (81, Tallinn), Nikolai Sazonov (80, Pskov): the hero's table story `city.san.tf.*` |
| gromov | 1 | window table, 1-01 / 1-02 | Pyotr Gromov (78), Sonya Mironova (16): `city.san.tg.*` |
| krylova | 2 | chess table | Zinaida Krylova (71, Vologda, a telephone operator), Raisa Abdullina (68, Kazan, a seamstress) |
| levin | 3 | window table | Arkady Levin (74, an icebreaker's navigator) and Tamara Levina (70, a music teacher who plays the hall's piano), Murmansk |
| beketov | 4 | chess table | Yuri Beketov (66, Norilsk, a mining engineer), Haruo Matsumoto (69, Otaru, a watchmaker) |
| kovalchuk | 5 | window table | Lidia Kovalchuk (60, Irkutsk, a train conductor) and her granddaughter Asya (12) |

**The stay.** A person's `"stay": {cycle, from, days, arrive, leave}` (`FPerson::StayCycle`): they are in the town from
the hour `arrive` of day `from` of every `cycle` days till the hour `leave` `days` days later; day 0 is the Monday of
calendar week 0 (`FWorld::Week`: the game counts weeks from Monday 1 January 2001, `UPeopleLifeSubsystem::UpdateClock`;
the tools from their own Monday). The cycle is six weeks, a pair comes on the Saturday of its week and leaves on the
Saturday a fortnight later, so the pair of one table goes and the next pair of that table comes on the same Saturday and
four guests are here at any time: on the game's first day (Tuesday 20 October 2026, week 2 of the cycle) Tamm and Sazonov
are in their second week and Gromov and Sonya in their first, as the hero's table stories have them. Outside the stay
`FWorld::Gone` is true and `DutyNow` gives the away activity `stay_gone` before anything else, sleep too: they stand
hidden (state Away) at the guests' end of the railway platform (`S-RAIL-GUESTS`, kind `guest_platform`, up the beach
halt's own way), need nothing, are never interrupted, and come back there, a little hungry and tired, at the hour of
their train. A duty with `"stay_day": n` holds only on that day of the stay.

**The Saturday.** The pair that leaves has breakfast, then the duty `san_depart` (day 14, 9:27 to 10:45): the walk to
the halt with the suitcase and the wait on the platform; at 10:45 the train has taken them. The pair that comes is on
the platform at 11:00 and has the duty `san_arrive` (day 0, to 12:24): the walk to «Корона» and a stand at the
reception; lunch at 13:30 is their first meal. The suitcase is the manner `carry_bag` forced with the prop `suitcase`
(`SM_Prop_suitcase`, `blender/humans/props.py`; the bag while the mesh is missing).

**The day.** Each has a room on the sanatorium's first floor (`P-SANATORIUM/1-01..04`, one after another: the data check
holds no two stays in a room or on a chair at once), goes to bed by the curfew and gets up early. Three duties a day
(`eat_sanatorium_breakfast|lunch|supper`, one activity a meal since the life finds a duty by its activity; pose eat:
breakfast 8:30, lunch 13:30, supper 18:30, 45 minutes) at the walked place `P-SANATORIUM-DINING`: in by the sanatorium's
door, one slot a chair tagged `guest_seat_1a|1b|2a|2b` (the city things `dining_table_1`, `dining_1_c`,
`dining_table_2`, `dining_2_c`: the acts `san_seat_<name>`), the duty's role the chair of their table. The regime's
outings are two activities only the guests like enough to do: `san_promenade` (the promenade and the pier after lunch,
14:25 to 16:35, by day and not in the rain) and `san_postcards` (a kiosk before lunch on Tuesdays, Fridays and Sundays). They take the waters
(`san_cure`), play dominoes, and the rest of the day are free like anybody. A slot with `"thing"` holds that city thing
while somebody sits or sits down there (`UPeopleLifeSubsystem::UpdateThingHolds`, once a second,
`ULyudCityThings::ClaimThing` / `ReleaseThing`): the hero gets "taken" instead of sitting on a guest.

**The stories.** `tools/barks/lines/16_sanatorium_guests.txt`: every guest has a job of their own for the lines
(`job = guest_<name>`, the role of their coming and going duties) and one story in four pieces told at the table, over
the dominoes and at the pump room, a piece by the days of the week (Saturday and Sunday, Monday and Tuesday, Wednesday
and Thursday, Friday); lines for the reception, the platform, the promenade and the kiosk; a goodbye to the hero on the
platform from those who knew him (`bond`).

`tools/peoplesim/run.sh guests` runs three weeks (four here at any time, the Saturdays' platform, train, reception and
first night, no room or chair with two, lunch at their chairs, the regime); `run.sh town` checks whoever is here at lunch
on Wednesday; `check_town_data.py` checks the stays against each other.

Not done: the trains are fixed hours (10:45 out, 11:00 in), not the timetable's, and the guests appear on and vanish
from the platform instead of riding the carriage; the Aton coast's belvedere and spring are not places of the life yet,
so nobody goes there; letters home are a line at the kiosk, not the post office; the hero's table stories
(`city.san.tg.*`, `tf.*`) are still told at the two tables whoever sits there now; `SM_Prop_suitcase` is not built or
imported (Mac).

## The new activities

Item 26 of `docs/interactives_backlog.md`: the town uses what the evening of 2026-11-20 added. The places, the activity
rows, the likes and the postman are put in by `tools/town/town_activities.py`, an add-on in the manner of
`sanatorium_guests.py`: it works on the generated files as they stand, is safe to run twice, `--check` says whether it
has been run, and `town_life.py` runs it at the end (regenerating the town rerolls everybody, so after a plan change
the add-on is what brings these back).

| activity | who, when | where |
|---|---|---|
| `book_club` (a duty, `"when": "book_club"`) | the five club members (`tools/reading/build_reading.py`), Thursdays 18:10–20:00 while the game holds the condition on; not on festivals or holidays | `P-LIBRARY-CLUB`: the library lounge's armchairs and sofa, a seat each (docs/books.md "The book club") |
| `cinema_day`, `cinema_evening` | anybody who likes it (the curious, the warm), going in before the 15:00 and 19:00 sessions | `P-CLUB2-CINEMA`: 145 of the hall's seats, each walked to along its aisle; six of them are the hero's seats and are held through the slot's `thing` |
| `san_cure` | 60 and over and the sanatorium's guests, mornings but Sunday | `P-SANATORIUM-PUMP`: the pump room's bench and stool, the lobby's benches; the drink manner with a cup |
| `dominoes` | 58 and over, 15:00 to 20:00 (not a talk: partners on one sofa would turn into each other) | `P-SANATORIUM-LOUNGE`: the lounge's four seats |
| `read_paper` | readers, 7:00 to 11:30 | any bench; the newspaper manner is forced |
| `skip_stones` | children first, the bold; by day, wind under 7.5 m/s, above 3 °C | `S-BEACH-STONES` (kind `stone_shore`): spots every 45 m on the wet sand's edge, 6.5 m up from the mean water line, facing the sea |
| `beach_castle` (hand-written in activities.json, docs/beach.md) | children 4 to 11, 10:00 to 19:00, by day, 17 °C and warmer; they also help the hero with his castle when he builds near them | the `beach` places, wandering a few metres while they dig |
| `tend_allotment`, `water_allotment` | the four keepers of the allotment's beds, morning and evening, not in frost | `P-T2H2-ALLOTMENT`: two slots a bed at its lane side (reached along the lane: the slot's `path`), each with its `owner` |
| `play_piano` | one in three of the musicians and teachers, one in thirty of the rest; 18:00 to 20:30, seldom | `P-HALL-STAGE`: the stool at the club hall's piano |
| `post_round` | the postwoman (`t_hernandez_lyudmila`), a duty 8:30 to 12:00 but Sunday, rain or not | `S-POST-ROUND`: 37 house doors, one after another |

What the sim learnt for them (`PeopleSim`):

- an activity's `temp: [min, max]`: outdoors it is not chosen outside these degrees (like `wind`);
- a slot's `owner`: a person's key, nobody else takes the slot (a missing person leaves it empty);
- a slot's `path`: points walked after the place's own path and before the slot's approach (a seat's aisle);
- a place's `open: 1`: a place with a door that is under the sky all the same (the allotment), so rain and cold count.

The postwoman is not the old postman of `docs/rounds.md`: he is retired and hands the hero his parcels, she has the
round of the doors now.

First shots on the Mac (2026-11-21, `unreal/scripts/shoot_residents.sh`): the cinema's house sits right; the keepers stood in
the lane and the stone throwers high up the sand (both moved since, not shot again); the hands' gestures for weeding and
the stone's throw were not seen in the frames and want a longer look.

Not done: the pianist is silent (the instruments' synth has no way to be played by somebody else yet), friends do not
walk along with the hero, and the cinema's house does not follow the programme (people sit through the two fixed
sessions whatever is on).

## The dances

`tools/town/town_dances.py` (an add-on `town_life.py` runs last; idempotent, `--check`) brings the town to the dances at the
hall «Своды» on Friday and Saturday evenings: `dance_evening` (15 to 45, pairs of standing slots on the dance room's floor)
and `dance_watch` (55 and over, the little tables and the lobby's benches) at the place `P-HALL-DANCE`, and the hero's twelve
dance partners like it. Its place and rows go at the very end, after town_activities' own. Test: `tools/peoplesim/run.sh
dances`. All of it: docs/dances.md.

## The Aton coast

Backlog item 37: the coast between the club and the town (docs/south.md "People") has its people too, added by
`tools/town/south_people.py` in the same manner (idempotent, `--check`, run by `town_life.py` after `town_activities.py`;
its places and rows go just before town_activities' own so that either script can run again). Three old fishermen at the
boat shed, a beekeeper at the old orchard, eight birdwatchers at the hide at sunset, walkers up the shoulder trail and
back (a round of spots along the trail), the belvedere's benches, children at the brook. `tools/peoplesim/run.sh town`
checks it (`AtonWatch`).

## Pets

Item 46 (`docs/animals.md` "With people"): `tools/town/town_pets.py`, an add-on `town_life.py` runs, gives ten
residents a dog and two duties a day (`walk_dog`, `walk_dog_late`), Nina Korneeva the yard cats' two rounds
(`feed_cats`, `feed_cats_late` at `S-CAT-FEEDERS`), fourteen flats a cat, and writes `Content/AnimalsData/pets.json`.
Its rows go before `town_night.py`'s in `activities.json` and `town_places.json`. `"on_time": 1` on an activity ends
a duty of it at the duty's hour (without it a duty lasts its length from the arrival).

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
