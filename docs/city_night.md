# Night state of the public rooms

When a café, a club, the library or the department store shuts, its rooms look shut: the chairs go up on the tables
(legs in the air, backs hanging over the edges), the bar stools go up on the counter, the library's chairs are pushed in
under the reading tables, and one lamp stays on over a counter. Before the doors open in the morning a cleaner mops
the floor (a bucket, a mop, damp streaks between the tables), and the department store has a night watchman who walks
its dark ground floor with a torch. Backlog item 29 (docs/interactives_backlog.md), built on item 25's dressing
(docs/city_dress.md).

## Files

| What | Where |
|---|---|
| Night pieces | `blender/city/night_kit.py`: `chairs_up_round`, `chairs_up_rect`, `chair_up`, `stool_up`, `chairs_stacked`, `mop_bucket`, `wet_floor`, `broom_dustpan`, `torch`, `night_lamp`, `lone_lamp`, `Gather` |
| Showroom of the pieces | `python3 blender/city/night_showroom.py -- [--samples N] [--res WxH] [--out DIR]` (a 10 x 6 m room: day and night sets side by side, the cleaner's corner) |
| The buildings | `blender/city/buildings/saucer_pavilion.py` (café «Тарелка»), `workers_club.py` (Rassvet), `cog_club.py` (the buffet), `capsule_library.py`, `department_store.py` (the café upstairs) |
| The "closed" lamp | `B.lamp(..., kind="closed")` (`blender/city/citykit.py`) -> `LAMP_KINDS` in `unreal/scripts/import_city.py` -> `ALyudCityBuilding::LampKinds` 2, switched by `ULyudWindowLights` |
| Previews at a game moment | `python3 blender/city/shoot_interior.py -- saucer_pavilion --cam 0,10,5.9,90,-14 --dress "hour=23,open=0" --night` |
| The cleaners and the watchman | `tools/town/town_night.py` (an add-on of the town generator, like `town_activities.py`), activities `mop_floor` and `night_watch`, manners `mop` and `torch` |
| Their props | `blender/humans/props.py` `mop`, `torch`, `bucket` -> `/Game/Characters/Humans/Props/SM_Prop_*` (`unreal/scripts/import_props.py`) |

## How the swap works

A dress set only adds geometry (there is no "hide these pieces"), so the day's arrangement moves into a set of its own:

```python
day_g, night_g = nk.Gather(B), nk.Gather(B)
for t in tables:
    kit.table_round(B, ...)                          # the table stays: permanent, with its collision
    with day_g:
        chs = kit.around_table(B, ...)               # the chairs, cups, vases of the open hours
    with night_g:
        nk.chairs_up_round(B, x, y, top, n=3, ...)   # the same chairs up on the table
    B.spot(...)                                      # spots and things outside, as before (same ids)
day_g.dress("day_tables", open=True)
night_g.dress("night_tables", open=False)
```

`open` is the building's hours (`HOURS` in `import_city.py`; `ALyudCityBuilding` OpenHour..CloseHour). `nk.Gather` lets
the code that makes a chair and its seat's spot stay one loop; a plain `with B.dress(...)` block works where nothing
else is made in between. Whatever else sits on those tables (a newspaper, flowers on holidays) gets `open=True` too, or
it would stand inside the chairs at night.

A chair goes up by building it at the origin into a throwaway builder and turning it half a turn about its x axis
(a rotation, not a mirror: the faces keep their winding and their UVs), then onto the table top. On a round table each
chair sits 0.2 m out from the middle toward its side; two or three chairs a table fit, four don't (the seats would
overlap), so a small table takes two up and two pushed in.

The sets of item 29 in each building: `day_*` / `night_*` and `night_cleaner`. Seat things and spots keep their ids;
the hero can't sit on a chair that is up, but the rooms are locked while it is.

## The lamp left on

`nk.lone_lamp(B, x, y, z, yaw)` puts the green desk lamp on a counter and its light, kind `"closed"`: on from dusk to
dawn while the building is closed (`bNightLamp` in `ULyudWindowLights`); the room's public windows then glow at 0.12
instead of going dark, so a shut café on the promenade reads as shut, not abandoned. Outside any dress block (a set
holds no lamps). The library keeps the librarian's own desk lamp on the same way.

## The cleaner's morning

`night_cleaner` sets show `hours=(7, 10), open=False`: the bucket with the mop in it and the damp streaks
(`wet_floor`: thin crescents of `CT_Wet`, a glossy shade darker than any floor) between the tables from 7 until the
doors open. The library opens at 7, so its cleaner comes after closing (21..23). The year scan
(`tools/citydress/year_scan.py`) samples 7:30 for these.

The townsfolk (`tools/town/town_night.py`): two residents who had no work become cleaners (occupation `cleaner`), one
becomes the store's night watchman (occupation `guard`, role "сторож / night watchman / 夜警"):

| Who | Where, when |
|---|---|
| cleaner A | the café 7:00-7:50, the Rassvet's foyer 8:05-9:55, every day |
| cleaner B | the store's ground floor 7:00-8:50 (Mon-Sat), the library 21:05-22:25 every day |
| the watchman | the store 20:30-6:30 every night; sleeps 7:10-14:50 |

Each place is a walked round (`"round": 1`) of floor spots in its building, in by the street door (the café's way goes
up the spiral stair). An indoor round goes from spot to spot by the room's own ways (`FWorld::Wander` uses `Route`
within the place, not the street grid). At each spot the cleaner mops (manner `mop`: the mop swept side to side, the
bucket on the floor at their side, `ANpcBody::PlaceMannerProp`); the watchman holds the torch out ahead, its beam a
small spot light without shadows (`ANpcBody::PlaceTorchLight`, made the first time a body holds a torch).

The script keeps who has the jobs ("night_job" in townsfolk.json) and puts its places and activity rows before
`town_activities.py`'s, so either script can run last. `tools/town/town_life.py` runs it after the others.

## Checks

- `python3 blender/city/build_city.py -- saucer_pavilion,workers_club,cog_club,capsule_library,department_store --no-render`:
  every set 0 stretched UV, 0 degenerate faces; at most ~16 sets a building (the store has 16).
- `tools/citydress/run.sh`: the C++ and Python conditions agree; no set never shows.
- `python3 tools/town/town_night.py --check`, `python3 tools/peoplesim/check_town_data.py`, `tools/peoplesim/run.sh week`.

## On the Mac (owed)

- Import: `build_city.py` for the five buildings, `import_city.py` (the dress meshes, lamp kind 2), `props.py` +
  `import_props.py` (mop, torch, bucket), then a C++ build (LyudCityBuilding, PeopleManners, BodyGestures,
  PeopleLife, PeopleSim).
- Shots at 23:00 (chairs up, the one lamp, the windows faint) and 7:30 (the cleaner at the café), the watchman at night.
- A bench run against main (the dress meshes are a few thousand triangles a building, drawn only while shown; one torch
  light).
