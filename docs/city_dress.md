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

Exclusive variants (fresh flowers / wilting / an empty vase) are sets with complementary phases over one period: the
flowers `phase=(7, 0, 4)`, the wilting `phase=(7, 4, 6)`, the empty vase `phase=(7, 6, 7)`. Keep a set small (a few hundred
triangles: it is drawn as its own mesh) and keep the number of sets of one building under about 16.

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
`interiors.dress_all` (every set shown; again: back to the day) and `interiors.dress_hide` (none shown). To see a set's
real moment, move the calendar and the weather from the panel's Time and World tabs.

## Checks

- `python3 blender/city/build_city.py -- <type> --no-render`: the lint prints a line per set (`dress <id>: tris, stretched
  UV, degenerate`): 0 and 0 before a push (the 3D rule: check UVs and artefacts on every asset).
- `python3 blender/city/dress_showroom.py`: the props in a room (renders in `renders/city/interior/`).
- `tools/citydress/run.sh`: the C++ condition against the Python one; every set of the exported buildings shows on some
  days and hides on some days over a game year (a typo such as `hours=(8, 8)` that makes a set always-on is reported).
- On the Mac: `unreal/scripts/import_city.sh <types>`, then `lyud.City.Dress 2` and offscreen shots; a bench of the plot
  against main (perf checklist: alone, frame and memory).
