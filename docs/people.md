# The people's life

Everybody who sits in the club lives a life of their own: a profile, needs, a flat in the Academgorodok and a day by the
game's clock. In free time they choose what to do for themselves: play in the club, read in the library, walk, sit on a
bench, watch the sun go down, talk to each other, eat at home, go to town. Nights they sleep in their own beds.

## Files

| What | Where |
|---|---|
| Simulation, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/People/PeopleSim.h/.cpp` (+ `MiniJson.h`) |
| Game side: subsystem, bodies, API, console | `Public/People/PeopleLife.h`, `Private/People/PeopleLife.cpp` |
| Who lives here | `Content/PeopleData/people.json` |
| What people can do | `Content/PeopleData/activities.json` |
| Where: flats, beds, chairs, the library, the club, benches, stations | `Content/PeopleData/people_places.json` from `tools/people_map.py` |
| Run it outside the engine, tests | `tools/peoplesim/run.sh test|day|trace` |
| Offscreen check in the game | `unreal/scripts/test_people.sh` (`test_people.py`) |

Rebuild the places whenever the Academgorodok (`blender/academgorodok/*`), the club's layout or the walk grid change:
`python3 tools/people_map.py --cache Saved/PeopleMap/cache.json` (`--plot` draws every storey, the library, the club and
the town into `Saved/PeopleMap/`).

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

Nao is the first person made to the character standard (docs/characters.md): a generated model rigged on the shared
skeleton. She stands where the hero first comes out of the club, at the `club_door` place.

Temper (0..1: social, active, studious, gamer, nature, sun) weighs their choices; `friends` draws them to places where
friends are. Everybody's `sun` is there because people in this world are drawn to the sun (docs/story/motifs.md): the
ones who love it most leave what they do to see it go down.

## How it works

**Needs** (0 fine .. 1 urgent): hunger, tired, social, fun, nature (fresh air), mind, sun. They grow by the hour (each
by the person's temper; sun only by day, and being outside under it meets it), and activities meet them per hour.

**The day.** Fixed hours first: sleep (from `bed` to `wake`) and duties (`work_booth`, `work_clerk`, `work_library`,
`work_away`, `school`; they set off 20 minutes early). In free time a person scores every activity: how much of their
needs it would meet over its usual length (an urgent need counts more), how much their temper likes it, friends there,
minus the way there (an hour of walking costs 1.6; the cable car costs ~6 minutes more), plus a little chance. Free time
ends 20 minutes before bed. While doing something they look up every couple of minutes: a much better thing to do now
(the sunset) or a need that can't wait (hunger over 0.85, sleepiness over 0.96) makes them leave; a duty takes a break to
eat and goes back.

**Activities** (`activities.json`, one row each): `places` (place kinds, `home` = their own flat), `slots` (slot
kinds), `pose`, `minutes`, `hours`, `ages`, `meets`, `costs`, `base`, `trait`, and flags `daylight`, `sunset`, `talk`
(looks for somebody to talk to there), `wander` (a walk from spot to spot), `away` (leaves the world: town, school,
work), `free` (0 = only a duty). To add one, add a row; if it needs a new kind of place or seat, add it to
`tools/people_map.py`. Reading in the library is `read_library` (the 23 seats of the reading hall).

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
- **Doors:** bodies are registered with the universal door's agents (`ULyudDoorSubsystem::RegisterAgent`) when that
  door is in the build, so doors open in front of them and shut behind them.
- **Save:** `Homes` (household -> flat) and everybody's needs; after a load everybody is where they'd be at that hour.

API: `UPeopleLifeSubsystem::Get(World)`: `GetPerson(Id, Info)` (profile, home, activity, place, state, pose, needs,
partner, the actor that shows them), `FindPerson(Key)`, `Order(Id, Activity, Place)`, `SetHome(Household, Flat)`,
`Flats()`, `Report()`, `Describe(Id)`; `OnPersonEvent(Id, Event, Other)` with Start, Arrive, Leave, TalkStart, TalkEnd,
Ride, Away, Back.

Console: `lyud.People 0|1`, `lyud.People.Report [key]`, `lyud.People.Order <key> <activity> [place]`,
`lyud.People.Flats`, `lyud.People.DrawDistance`, `lyud.People.AnyMap 1`; the clock: `lyud.Birds.Time <h> [m]`.

## Cost

Outside the engine a step of the 12 people costs well under 0.001 ms; a decision scores ~20 activities over their slots. In
the game the bodies (12 at most) tick their animation only when rendered.

## Known gaps

- Novikova 1's middle stair has no way in (its street side is the library's vestibule, the yard door's headroom under the
  half landing is 1.65 m): its six flats are left empty. The yard doors of all stairs are too low (1.65 m) for people, so
  everybody uses the street doors.
- 11 of the 57 flats have no bedroom (sofas only); households go to flats with enough beds.
- The benches round the square, on the spur, the promenade and in town are places in the data, not models yet.
- The cable car's cabins don't carry people visibly: they're hidden for the ride.
- Indoors people are drawn only near the camera; the club's crowd at stations 11–13 reuses three regulars' models.
