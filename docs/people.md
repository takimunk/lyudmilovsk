# The people's life

Everybody who sits in the club lives a life of their own: a profile, needs, a flat in the Academgorodok and a day by the
game's clock. In free time they choose what to do for themselves: play in the club, read in the library, walk, sit on a
bench, watch the sun go down, talk to each other, eat at home, go to town. Nights they sleep in their own beds. Fifteen
more people live in the Academgorodok without coming to the club much: Laboratory No. 2's staff, the kiosk's assistant,
pensioners, a family, a fisherman, a student, a painter and an ornithologist who take the sea railway's train to the
island.

## Files

| What | Where |
|---|---|
| Simulation, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/People/PeopleSim.h/.cpp` (+ `MiniJson.h`) |
| Game side: subsystem, bodies, API, console | `Public/People/PeopleLife.h`, `Private/People/PeopleLife.cpp` |
| Who lives here | `Content/PeopleData/people.json` |
| What people can do | `Content/PeopleData/activities.json` |
| Where: flats, beds, chairs, the library, the club, benches, stations | `Content/PeopleData/people_places.json` from `tools/people_map.py` |
| Run it outside the engine, tests | `tools/peoplesim/run.sh test|day|trace|summary` |
| The train's timetable (shared with the game) | `Private/Railway/RailRun.h/.cpp` on `Content/Railway/railway_built.json` (`tools/railway.py`, docs/railway.md) |
| Offscreen check in the game | `unreal/scripts/test_people.sh` (`test_people.py`) |

Rebuild the places whenever the Academgorodok (`blender/academgorodok/*`), the club's layout or the walk grid change:
`python3 tools/people_map.py --cache Saved/PeopleMap/cache.json` (`--plot` draws every storey, the library, the club and
the town into `Saved/PeopleMap/`). The club's seats come from main's `blender/export/scene01_layout.json` (`--scene`); a
worktree whose layout has no stations keeps the club of the existing `people_places.json`. Rerun it too when the railway
changes (`railway_built.json`: the halts and the island).

## The people

Ages, occupations, households and hours are **proposals** (not canon): they follow the club's lines (Sonya's mother told
her to sit here an hour; Timur came from work; the clerk's Excel was on his work computer; Olya saves every page) and can
be changed in `people.json` freely.

| Key | Name | Age | Occupation (proposal) | Household | Day (proposal) |
|---|---|---|---|---|---|
| owner | Кэндзи Огава | 61 | the club's owner | alone | up 10:30; the booth 12:00–02:00 every day |
| clerk | Ямада Хироси | 46 | office clerk, draws in Excel at station 9 | alone | 10:00–19:00 at station 9 on weekdays (it is his) |
| streamer | Ника | 23 | streams | alone | up at 11, bed at 2 |
| kid_boy | Тёма | 11 | schoolboy | his parents' flat ("they're upstairs": not people yet) | school 8:15–13:30 on weekdays, bed 21:30 |
| kid_girl | Соня | 9 | schoolgirl | with her mother Вера | school 8:15–13:00, bed 21:00 |
| guy_a | Стас | 19 | student | shares a flat with Тимур | lectures 9–14 on weekdays |
| guy_b | Тимур | 27 | warehouse worker | shares a flat with Стас | work 7:30–15:00 on weekdays |
| girl_a | Вера | 36 | engineer at Laboratory No. 2 | with her daughter Соня | work 9–15 on weekdays |
| girl_b | Оля | 25 | librarian at the Academgorodok's library | alone | the library's desk 10–15, Wednesday to Saturday |
| crowd_11 | Гоша | 31 | fisherman | alone | at sea 6–12, Monday to Saturday |
| crowd_12 | Лиза | 28 | teacher | alone | school 8–14:30 |
| crowd_13 | Кирилл | 21 | student | alone | lectures 10–15, Monday to Thursday |
| nao | Нао | 19 | watches the club's door (`watch_door`) | alone | at the club's door on the forecourt 10:00–21:00 every day (a new game finds her there) |

Everybody here is made to the character standard (docs/characters.md, 2026-10-25): a generated model (hi3d) rigged on
the shared skeleton, at the same paths `/Game/Characters/Humans/<Mesh>/SK_<Mesh>` as the MakeHuman cast it replaced
(their own seated clips kept). Nao was the first. She keeps the door where the hero first comes out of the club: her
`watch_door` wanders between the `club_door` spot and the `club_forecourt` spots (`tools/people_map.py`, six standing
spots along the alley 1.5-15 m from the door), 2.5-20 m at a time, pausing 20-70 s at each and looking about.

### The Academgorodok's residents

Fifteen people of the Academgorodok (2026-10-28). Everything about them is a **proposal** (not canon, docs/story/README.md):
names, ages, jobs, households, hours and their two lines each. Vera (girl_a) works at Laboratory No. 2 too
(`work_lab`, she is its engineer).

| Key | Name | Age | Occupation (proposal) | Household | Day (proposal) | Shown now by |
|---|---|---|---|---|---|---|
| zotov_lev | Лев Зотов | 58 | physicist, Laboratory No. 2 | with his wife Галина | the Laboratory 9–17 on weekdays | Owner |
| zotova_galina | Галина Зотова | 55 | chemist, Laboratory No. 2 | with Лев | the Laboratory 9–16 on weekdays | Girl_a |
| sato_kei | Кэй Сато | 34 | spectroscopist, Laboratory No. 2 | the Satos: with Мила and Юки | the Laboratory 10–18 on weekdays | Guy_b |
| sato_mila | Мила Сато | 33 | doctor at the clinic in town | the Satos | the clinic 8–15 on weekdays (`work_away`); walks on the island | Crowd_12 |
| sato_yuki | Юки Сато | 8 | schoolgirl | the Satos (a sofa of her own) | school 8:15–13 on weekdays, bed 21 | Kid_girl |
| korneev_ivan | Иван Корнеев | 74 | pensioner, once the Laboratory's mechanic | with his wife Нина | up at 6, fishes off the beach | Clerk |
| korneeva_nina | Нина Корнеева | 71 | pensioner | with Иван | the kiosk, the benches, talk | Girl_b |
| belov_misha | Миша Белов | 29 | technician, Laboratory No. 2 | alone | the Laboratory 8–16:30 on weekdays | Crowd_13 |
| orlova_tanya | Таня Орлова | 42 | the Gastronom kiosk's assistant | alone | behind the counter 9–19, Monday to Saturday | Streamer |
| lange_pyotr | Пётр Ланге | 47 | fisherman | alone | at sea 4:30–11 from the beach, Tuesday to Sunday (`work_sea`) | Crowd_11 |
| kimura_dasha | Даша Кимура | 20 | physics student | shares a flat with Вика | lectures 9–14 on weekdays; the island after | Girl_b |
| li_vika | Вика Ли | 27 | lab assistant, Laboratory No. 2 | with Даша | the Laboratory 9–17:30 on weekdays | Crowd_12 |
| valiev_rustam | Рустам Валиев | 45 | engineer, Laboratory No. 2 | alone | the Laboratory 8:30–17:30 on weekdays | Guy_a |
| nishimura_aya | Ая Нисимура | 39 | painter | alone | paints on the island (`paint_island`), by the morning train | Girl_a |
| sorokin_oleg | Олег Сорокин | 63 | retired ornithologist | alone | up at 5:30, watches birds on the island (`birdwatch`) | Owner |

**Bodies.** No new models are made for them here (people are moving to MetaHuman in their own branch). Each names its
own body in `people.json` `body` (`Zotov_lev`, ...; the club's people have none: theirs is `mesh`), and `mesh` is an
existing model shown until that body exists. The game loads `/Game/Characters/Humans/<body>/SK_<body>` first and falls
back to `mesh`; when the bodies move (MetaHuman), only the loader in `ANpcBody::Setup` changes, never the data. The same
`body` id is who they are in `Content/Dialogue/npc_dialogue.txt`: someone shown by Owner's model never says the owner's
lines.

**Words.** `npc.<key>.name|role` and two lines each (`line.<key>.first.1`, `line.<key>.again1.1`) are in the strings
(ru/en/ja; no voices). They are silent until `npc_dialogue.txt` has a section for them, e.g.

```
[Zotov_lev]
first = zotov_lev.first.1
again = zotov_lev.again1.1
```

**Likes.** `likes` (activity id -> a bonus to its score) makes a painter paint and an ornithologist watch birds without a
new temper; the older residents have `play_club` below zero (they don't crowd the club's computers).

Temper (0..1: social, active, studious, gamer, nature, sun) weighs their choices; `friends` draws them to places where
friends are. Everybody's `sun` is there because people in this world are drawn to the sun (docs/story/motifs.md): the
ones who love it most leave what they do to see it go down.

## How it works

**Needs** (0 fine .. 1 urgent): hunger, tired, social, fun, nature (fresh air), mind, sun. They grow by the hour (each
by the person's temper; sun only by day, and being outside under it meets it), and activities meet them per hour.

**The day.** Fixed hours first: sleep (from `bed` to `wake`) and duties (`work_booth`, `work_clerk`, `work_library`,
`work_away`, `work_lab`, `work_shop`, `work_sea`, `school`; they set off 20 minutes early). In free time a person scores
every activity: how much of their needs it would meet over its usual length (an urgent need counts more), how much their
temper likes it, their `likes`, friends there, minus the way there (an hour of the way costs 1.6: walking, the cable car
~6 minutes more, the train the wait for it and the ride), plus a little chance. Free time ends 20 minutes before bed or
the day's next duty (whatever they start must fit before it, the way back included). While doing something they look up every couple of minutes: a much better thing to do now
(the sunset) or a need that can't wait (hunger over 0.85, sleepiness over 0.96) makes them leave; a duty takes a break to
eat and goes back.

**Activities** (`activities.json`, one row each): `places` (place kinds, `home` = their own flat), `slots` (slot
kinds), `pose`, `minutes`, `hours`, `ages`, `meets`, `costs`, `base`, `trait`, and flags `daylight`, `sunset`, `talk`
(looks for somebody to talk to there), `wander` (a walk from spot to spot; `wander_m` [near, far] metres to the next
spot, default 15-220, `wander_pause` [min, max] seconds at each, default 40-130), `away` (leaves the world: town, school,
work), `free` (0 = only a duty), `days` (the weekdays it can be chosen on: the Gastronom is shut on Sundays). To add one, add a row; if it needs a new kind of place or seat, add it to
`tools/people_map.py`. Reading in the library is `read_library` (the 23 seats of the reading hall).
The residents' ones: `work_lab` (in at Laboratory No. 2's gate, away for the day), `work_shop` (behind the Gastronom's
counter), `work_sea` (out to sea from the beach), `shop` (a few minutes at the Gastronom's counter, Monday to Saturday 9–19),
`pastime_home` (crosswords, the radio, mending: fun at home for grown-ups), `sea_air` (standing on the beach),
`lookout_view` (up the cable car to the spur's lookout), `fish_shore` (the beach or the island), `island_walk`,
`paint_island`, `birdwatch` (the island's spots).

**Places off the town.** Laboratory No. 2's buildings aren't in the game: its gate
(`lab2_gate`, kind `lab`) is a spot behind the ensemble on the way into town, 85 m back from the middle house, where its
staff go in (a proposal, data only, no model). The Gastronom (`gastronom`, kind `shop`, indoors) is the grocery pavilion behind Novikova 1 (`blender/academgorodok/grocery.py`):
in by its hall door, the assistant's `counter` in the service aisle, three customers' spots in front of the counter
(`build_gastronom`); its assistant sells over the counter (`counters.json`). Both are made by `tools/people_map.py` (`build_academ_extra`, `build_gastronom`) and can be moved there.

**Homes and reservations.** A flat belongs to a household (`SetHome(household, flat)`, kept with the save); a new game
spreads the households over the houses and stairs (seeded), each into a free flat with enough beds. In the flat each
gets a bed (children their own; a double bed takes two), else a sofa. Every slot (bed, chair, computer, bench place) has
a capacity and is reserved by whoever is going to it or using it; other people's beds and flats are never used; the
clerk's station 9 is his alone (`own_slot`); people prefer their own club station and keep off others' when those might
come.

**Ways.** Outdoors along the town's 2 m walk grid (`Content/AnimalsData/animal_world.bin`, A* that prefers streets and
paths); indoors along the polylines of `people_places.json`: from a stair's street door up the flights to the landing,
through the flat's door to the seat; from the library's door to its chairs; from the club's portal through the blast
door, the hall and the partition onto the walkway to the station. The club's spur is cut off from the valley on foot, so
between them people ride the cable car (`RideSeconds`, 150 s, hidden in the cabin; the stations' doorsteps are the
stops).

**The train** (the sea railway, docs/railway.md). Its two halts are places of kind `rail_stop` (`rail_beach`,
`rail_island`; `stop` = the timetable's id): `pos` where walkers arrive on the ground, `way` from there to the stair's foot
and up onto the platform's deck, and eight `board` slots along the deck facing the track. The beach halt is south of the
walk grid, so its way runs from the beach path's south end on the grid along the sand over the level crossing to the stair
at the platform's east end. The sea island is island 2 (`sea_island: 1`), off the walk grid: its spots (`island_spot`:
a ring inland, a ring on its beach, the knoll at its west end, all south of the track) and its own ground
(`ground_patches`, a 4 m height field from `tools/railway.py` `island_height`); people walk straight between its spots.
A way to the island (or back, or on from the spur by the cable car first) walks to the halt, climbs onto the platform to a
boarding point (spread by person) and **waits** (`waiting`, standing, facing the track) until the next train towards the
other halt opens its doors (`FWorld::TrainNext`, the timetable on the sim's clock); in they get a few seconds apart,
**ride** (`riding`, `bTrain`) until the doors open at the other halt (`TrainRide`, ~4 min), come out onto its platform,
down its stair and on. On the island an activity ends in time to walk back for the train nearest the time they meant to
leave (never less than half of what they came for). Without a timetable (no railway in the level) the ride is 280 s,
hidden. The game's halts are the railway's own: once `ULyudRailwaySubsystem` runs, `SetRailStop` moves the stair's foot
and the boarding points to `GetStopEntrance` / `GetBoardingPoint`.

**Hurrying.** Someone who would reach a duty (work, school, the booth) after it begins runs the outdoor part of the way (~2.4 m/s, a brisk walk indoors); so does everybody out on their way in the rain (`ULyudWeatherSubsystem` rain over 0.25). Otherwise they walk (1.25 m/s, children 1.35).

**A new game** (the clock at the start date, Tuesday 20 October 16:00) begins as the club scene always has: the 11
regulars at their own stations, the owner in his booth, the clerk at work, Nao at the club's door. Each regular plays a while (`start.minutes`)
and then their life goes on. A clock jump (a load, `lyud.Birds.Time`, developer tools) puts everybody where they would
be at that hour.

## In the game

`UPeopleLifeSubsystem` starts by itself in Lvl_Scene01 (`lyud.People 0` turns it off and puts the club back as placed).
It runs the simulation on the game calendar (its time scale), in steps of at most a second.

- **At their own station** a regular is shown by the level's own placed actor (as before: their seated clip, the clerk's
  desk). The placed actor is hidden while they're away.
- **The owner at his booth** is the scene's `ABoothOwner` (window, talk, the T trade, as before); away, it is hidden
  (`ABoothOwner::SetAway`) and he can't be called or trade.
- **Everywhere else** a person is an `ANpcBody`: their MakeHuman model with the shared clips
  (`/Game/Characters/Humans/Anims/A_Human_<Walk|StandIdle|Talk|SitDown|StandUp|SitIdle|Eat|Read|LieDown|Sleep|GetUp>`;
  until those exist, the owner's walk, stand-up, sit-down and seated clips on the same skeleton), their own seated
  clip at a computer, walking at their pace. Drawn within 150 m outdoors (`lyud.People.DrawDistance`), 40 m indoors.
- People walking about can be talked to like in the club (the same lines; `ANpcTalkDirector::AddPerson`); the crowd
  (`crowd_*`) doesn't talk. Hidden people don't talk (the club's placed actors while their person is away).
- **On the train** a rider sits on a free seat of the train (`ALyudTrain::GetSeat`, the train standing at their halt when
  they got in) while it is drawn (`GetInfo().bVisible`, within the draw distance), hidden otherwise; waiting on a platform
  they stand at their boarding point like anybody outdoors.
- **Doors:** bodies are registered with the universal door's agents (`ULyudDoorSubsystem::RegisterAgent`) when that
  door is in the build, so doors open in front of them and shut behind them.
- **Save:** `Homes` (household -> flat) and everybody's needs; after a load everybody is where they'd be at that hour.

API: `UPeopleLifeSubsystem::Get(World)`: `GetPerson(Id, Info)` (profile, home, activity, place, state, pose, needs,
partner, the actor that shows them), `FindPerson(Key)`, `Order(Id, Activity, Place)`, `SetHome(Household, Flat)`,
`Flats()`, `Report()`, `Describe(Id)`; `OnPersonEvent(Id, Event, Other)` with Start, Arrive, Leave, TalkStart, TalkEnd,
Ride, Away, Back.

Console: `lyud.People 0|1`, `lyud.People.Report [key]`, `lyud.People.Order <key> <activity> [place]`,
`lyud.People.Flats`, `lyud.People.DrawDistance`, `lyud.People.AnyMap 1`; the clock: `lyud.Birds.Time <h> [m]`.

## Tests

`tools/peoplesim/run.sh test` runs two days in a made-up town (with its own halts and island) and on the real map, with the
railway's timetable from `railway_built.json` (`RailRun.cpp` is compiled in): homes and beds, the club scene, sleep,
school and work (Laboratory No. 2's staff away at noon, Tanya at the counter, Pyotr at sea), meals, the kiosk, train trips
to the island and back, nobody left on the island at night, nobody waiting over an hour or more than 2.5 h on a way.
`run.sh summary [start hours weekday]` prints the residents' day, one line each (default Wednesday from 04:00):

```
nishimura_aya  39 painter   04:00 sleep, 07:03 walk, 07:54 eat_home, 08:16 pastime_home, 09:11 read_home,
                            10:01 train beach>island, 10:06 paint_island, 11:47 train island>beach, 12:06 town, ...
```

## Cost

Outside the engine a step of the 28 people costs under 0.001 ms; a decision scores ~20 activities over their slots. In
the game the bodies (12 at most) tick their animation only when rendered.

## Known gaps

- Novikova 1's middle stair has no way in (its street side is the library's vestibule, the yard door's headroom under the
  half landing is 1.65 m): its six flats are left empty. The yard doors of all stairs are too low (1.65 m) for people, so
  everybody uses the street doors.
- 11 of the 57 flats have no bedroom (sofas only); households go to flats with enough beds.
- The benches round the square, on the spur, the promenade and in town are places in the data, not models yet.
- The cable car's cabins don't carry people visibly: they're hidden for the ride.
- Laboratory No. 2's gate and the Gastronom's kiosk are spots in the data, not models; the residents are shown by the club
  people's models (`mesh`) until their own bodies (`body`) are made, and are silent until `npc_dialogue.txt` lists them.
- A household doesn't go out together (the Satos each choose for themselves); children don't go to the island alone.
- The island's spots and halts are placed from `railway_built.json`; the island's real shape (blender/railway) may
  differ a little from `island_height` until people_map.py is rerun on the final data.
- Indoors people are drawn only near the camera; the crowd (Гоша, Лиза, Кирилл) have their own models
  (`SK_Crowd_11..13`, docs/characters.md) as bodies, but the level's placed actors at stations 11–13 still show three
  regulars' models until the scene is rebuilt with them.
