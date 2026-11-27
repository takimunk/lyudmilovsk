# Lived-in rooms: dressing that changes with the days

The town's interiors are furnished once (docs/city_interiors.md). Dressing is the layer of small things on top that comes
and goes: a cup left after breakfast, today's paper on the café table, wet umbrellas drying by the door on rain days, coats
on the racks in winter, a jigsaw in the sanatorium lounge that grows a little every day, the children's drawings on the
school wall that change every week, flowers in a vase that wilt and get replaced. Backlog item 25
(docs/interactives_backlog.md) and city_backlog P5 "seasonal dressing".

Out of doors the street has a layer like it that follows the weather and the date (the washing on the yards' lines,
the kvass barrel on a hot day, the leaf piles, the whitewashed trunks): docs/street_life.md.

## Files

| What | Where |
|---|---|
| The API (B.dress, conditions, the stable die) | `blender/city/citykit.py` (`Building.dress`, `dress_when`, `dress_visible`, `dress_hash01`) |
| Dressing props | `blender/city/dress_kit.py` (umbrellas, coat boards, coats for the cloakroom rails, boots, wet mat, jigsaw, wilted vase, used plate, book face down, sledge, towel and hat, satchel, mittens) |
| Showroom of the props | `python3 blender/city/dress_showroom.py -- [--no-render]` |
| Export | `blender/city/build_city.py`: one mesh per set, `SM_<type>_Dress_<id>` in `<type>.glb`; `<type>.json` "dress": id, when, tris, bounds |
| Previews by game moment | `python3 blender/city/shoot_interior.py -- <type> --dress "month=1,hour=9,weather=rain+wet" --dress-tag winter_rain` |
| Game: the condition, plain C++ | `Source/Lyudmilovsk/Private/City/CityDressModel.h` (LyudCity::FDressWhen, DressVisible, DressHash01) |
| Game: show and hide | `Public/World/LyudCityDress.h`, `Private/World/LyudCityDress.cpp` (ULyudCityDress), the arrays on `ALyudCityBuilding` |
| Unreal import and placing | `unreal/scripts/import_city.py` (the `_Dress_` meshes; `dress_*` arrays on each building) |
| Offline checks | `tools/citydress/run.sh` (C++ and Python agree on 20 000 moments; a year of days per set: never always-on by mistake, never never-on) |

## Writing a set

```python
import dress_kit as dk

with B.dress("hall_umbrellas", weather=("rain", "wet")):
    dk.umbrella_open(B, 3.1, 1.4, 0.0, 30, seed=1)
    dk.doormat_wet(B, 2.0, 0.4)
with B.dress("cloak_winter", season="cold_months"):
    dk.coats_on_rail(B, 4.0, 6.0, 0.0, 90, w=2.4, share=0.85, season="winter")
for k in range(4):                              # four sets of drawings, a week each, round and round
    with B.dress("drawings_%d" % k, phase=(28, 7 * k, 7 * k + 7)):
        kit.poster(B, 1.0, 8.0, zc=1.5, name="kids_%d" % k, w=0.42)
```

Inside the block everything the kit puts into `B.detail` goes into the set's own mesh. A set has no collision (pieces
with `solid=False`; anything in `B.m` is an error), no lamps, no things, no spots (all errors). Ids `[a-z0-9_]`, unique in
the building and stable (they name the meshes). A set must not repeat a piece that is already in the room's permanent
furniture at the same place (a coat over a coat z-fights): for a cloakroom place `kit.coat_rack(..., coats=0)` and put
the coats in sets with `dk.coats_on_rail` at the same x, y, yaw and w.

Conditions (all given keys must hold; nothing given = always):

| Key | Meaning |
|---|---|
| `weather=("rain", "wet")` | any of these weather words now: rain, snow, fog, cold (< 2 C), warm (> 22 C), clear, wet (the ground still wet after rain), dry (nothing falling): the same words as the lines (`LyudCity::WeatherBits`) |
| `weather_not=("snow",)` | none of these |
| `months=(11, 12, 1)` or `season=` | `winter` (Dec-Feb), `spring`, `summer`, `autumn`, `cold_months` (Nov-Mar); both may be given (joined) |
| `weekdays=(0, 1, 2, 3, 4)` | Monday = 0 |
| `hours=(8, 10.5)` | from..to on a 24 h clock; to below from runs past midnight |
| `festival="any"` | a holiday today (not the Saturday market); `"none"`: not a holiday; or a festival id on now (`new_year`, `sea_festival`...) |
| `phase=(period, from, to)` | days since the game began modulo period in [from, to): weekly turns, a thing that grows by days (a jigsaw stage k: `phase=(9, k, 9)` shows from day k of every 9-day round) |
| `chance=0.6` | shown on that share of days: a stable die of (plot, set, day), the same all day and in every session |
| `open=True` / `False` | in / out of the building's public open hours (`ALyudCityBuilding` OpenHour..CloseHour: import_city.py HOURS) |
| `visit=("coffee",)`, `at=`, `keep=`, `who=` | a trace somebody left: see "Traces of a visit" below |

Exclusive variants (fresh flowers / wilting / an empty vase) are sets with complementary phases over one period: the
flowers `phase=(7, 0, 4)`, the wilting `phase=(7, 4, 6)`, the empty vase `phase=(7, 6, 7)`. Keep a set small (a few hundred
triangles: it is drawn as its own mesh) and keep the number of sets of one building under about 16.

## Traces of a visit

The date and the weather say what a room looks like on a day; a visit says who was just there. A set with `visit=` shows
for `keep` hours (default 3) after a townsperson ended one of the listed activities (`activities.json` ids) at the place
`at` of the building's plot: `""` (default) any place of the building, `"/1-17"` its flat 1-17 (the people's place id
`<plot>/1-17`), `"-LOUNGE"` the place `<plot>-LOUNGE`. `who=("teacher",)` keeps it to those occupations or person keys.
It joins the other keys (`open=True` so a trace is not on a table while the chairs are up; `months=` for term time).

```python
with B.dress("coffee_left", visit=("coffee", "eat_out"), keep=1.5, open=True):
    dk.tea_in_holder(B, x, y, top, a)
    dk.spectacles(B, x2, y2, top, a2)
```

In the game `ULyudCityDress` binds to the people's life (`UPeopleLifeSubsystem::OnPersonEvent`): on `Arrive` it notes
where a person came for a listened activity, on `Leave` it matches the activity, the place and `who` against the sets that
listen (a table built when the buildings register, looked up by activity) and stamps the set with the game's hour. The
sets are looked at again at the next tick (at most 2 s later); the condition is `LyudCity::DressVisible(..., VisitAgo)`.
Nothing per frame, nothing per person per frame: only Arrive and Leave events, a map lookup each. The stamps are not
saved: after a load a trace comes with the next visit. Most furnished flats have nobody living in them in
`townsfolk.json`; a home trace only works where a household lives (Colette Bauer's 1-17, the maisonette 4-01).

The traces in the town (2026-11-27):

| Building (plot) | Set | Visit, place, keep | What is left |
|---|---|---|---|
| saucer_pavilion (P-CAFE) | `coffee_left` | coffee, eat_out; 1.5 h, open | a glass of tea in its holder, the paper, reading glasses on table 2 |
| capsule_library (P-LIBRARY) | `reader_left` | read_library; 2 h | a book face down and glasses on the second reading table |
| capsule_library (P-LIBRARY) | `bookclub_after` | book_club at -CLUB; 3 h | the club's glasses of tea and somebody's glasses on the lounge table |
| round_sanatorium (P-SANATORIUM) | `lounge_tea` | dominoes at -LOUNGE; 2 h | two glasses of tea at the ends of the lounge table |
| round_sanatorium | `pump_glasses` | san_cure at -PUMP; 1.5 h | three drinking glasses on the pump room's round table |
| workers_club (P-CLUB2) | `cinema_stubs` | cinema_evening, cinema_day at -CINEMA; 12 h | ticket stubs and sweet wrappers in the rows |
| cog_club (P-CLUB3) | `chess_left_0/1` | culture_evening; 20 h; a new position every week (phase 14) | a game left mid-way on the first chess table (its board is empty otherwise) |
| school (P-SCHOOL) | `teacher_left` | work_office, who teacher; 3 h | the class register, duster, chalk and a glass of tea on the teacher's desk |
| school | `pupils_books` | school_town; 2.5 h; term months | exercise books left open on three desks |
| serpentine_slab (P-SERPENTINE) | `bauer_tea` | eat_home, radio_hour, read_home, pastime_home at /1-17; 2.5 h | Colette's tea and glasses on her kitchen table |
| terrace_riser_housing (P-RISER1/2) | `astro_homework`, `rimma_homework` | homework at /4-01; 4 h | a child's exercise books on the coffee table |
| shell_chapel_hall (P-HALL) | `piano_music` | play_piano at -STAGE; 14 h | the evening's music left up on the grand piano |

New pieces in `dress_kit.py`: `chess_left` (six real middle-game positions, taken pieces by the board; `board=False` on a
`kit.chess_table(..., pieces=False)`), `dominoes_left`, `tea_in_holder`, `spectacles`, `exercise_books`, `class_register`,
`ticket_stubs`, `sheet_music` (all in the showroom's "traces" table). Previews: `renders/lived_rooms/` (before and half an
hour after a visit), made with `shoot_interior.py --dress "hour=21.5,visit=0.5"` (`visit=H`: every visit set's visit ended
H hours ago). The year scan (`tools/citydress/year_scan.py`) stands in for the people's life with usual end hours per
activity (`VISIT_ENDS`), so a visit set that could never show is still caught.

Not done: the hero's own room (docs/hero_room.md) is spawned by `ULyudHeroRoom`, not an `ALyudCityBuilding`, so it has no
dressing yet (it would need that subsystem to register its sets with `ULyudCityDress`).

## In the game

`import_city.py place` puts each building's sets on its actor: `DressIds`, `DressMeshes` (the `SM_<type>_Dress_<id>` meshes,
Nanite, no collision) and `DressWhen` (`FLyudDressWhen`, the condition). `ULyudCityDress` (a world subsystem) looks every
2 seconds at the game's moment (`UGameCalendar`, the weather's summary, the festivals: the same `LyudCity::FWhen` the
city things read) and, only when the moment's key changed (the 10-minute step of the clock, the day, the weather bits,
the holiday), asks every set whether it shows. A set is a static mesh component made the first time it is shown (attached
to the building's shell, no collision, out of the distance fields), then only shown or hidden: never moved, never rebuilt,
nothing per frame. Nothing is saved: the dressing follows from the date and the weather, so a loaded game shows the same.

Console: `lyud.City.Dress 1` (auto, the default), `0` (all hidden), `2` (all shown: checks and shots);
`lyud.City.Dress.Report` (each building's sets and which are shown). Developer mode (F7, Actions, group "Town interiors"):
`interiors.dress_all` (every set shown; again: back to the day), `interiors.dress_hide` (none shown) and
`interiors.dress_visits` (every trace of a visit stamped as if somebody had just left: they show for their keep). To see a set's
real moment, move the calendar and the weather from the panel's Time and World tabs.

## Checks

- `python3 blender/city/build_city.py -- <type> --no-render`: the lint prints a line per set (`dress <id>: tris, stretched
  UV, degenerate`): 0 and 0 before a push (the 3D rule: check UVs and artefacts on every asset).
- `python3 blender/city/dress_showroom.py`: the props in a room (renders in `renders/city/interior/`).
- `tools/citydress/run.sh`: the C++ condition against the Python one; every set of the exported buildings shows on some
  days and hides on some days over a game year (a typo such as `hours=(8, 8)` that makes a set always-on is reported).
- On the Mac: `unreal/scripts/import_city.sh <types>`, then `lyud.City.Dress 2` and offscreen shots; a bench of the plot
  against main (perf checklist: alone, frame and memory).
