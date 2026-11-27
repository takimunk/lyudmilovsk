# Soda-water automats

The late-Soviet street machine «Газированная вода». Six of them stand where people pass. The hero puts in a coin, picks
plain water or the syrup of the day, rinses the one shared glass (or does not), and drinks. The glass stays on the washer
for the next person. Residents use them on hot days.

## What the player sees

- A white enamel cabinet with a red lit head «ГАЗИРОВАННАЯ ВОДА», two buttons with plates «БЕЗ СИРОПА 1 коп.» and
  «С СИРОПОМ 3 коп.», a stainless niche with the nozzle, the drip grille and the star washer with a faceted glass upside
  down on it, and a small sign «СПОЛОСНИ СТАКАН». It stands on a concrete pad.
- Left button: plain water (1 rouble in the game's money). Right button: with syrup (3 roubles).
- The scene: the coin goes in, sometimes it sticks; the glass, rinse or not; the syrup of the day (syrup only); pour; sip;
  put the glass back.
- The syrup changes by day: cherry, «Дюшес», cream soda, tarragon, lemon, apple. A good glass of a new flavour stamps it in
  the collection `soda_syrups`. All six pay 20.
- Some days in season the machine is out of order: a cardboard sign «АВТОМАТ НЕ РАБОТАЕТ» is wedged across the niche and the
  buttons answer with a few words.
- November to April it is off: a green tarpaulin hood is tied over it.

## Where

`tools/city_plan.py` `STREET_MACHINES`, written to `city_plan.json` as `"street_machines"` (see `docs/city_plan.md`).

| id | where |
|---|---|
| `SM-SODA1` | the promenade, between two plane trees by the pier |
| `SM-SODA2` | by bus stop 4 |
| `SM-SODA3` | at the foot of the beach steps `ST-SEA-07` |
| `SM-SODA4` | the Amber cable-car station's forecourt |
| `SM-SODA5` | Sun Square |
| `SM-SODA6` | the park's avenue |

Each row has `map` (the cabinet's middle, map metres), `front` (the paving in front of it, the building's origin), `yaw`,
`terrace`, names and their Unreal points (`ue`, `front_ue`). Every spot was checked to be 4 m or more from benches, lamps
and trees.

## The mesh

- Kit piece: `interior_kit.soda_machine` (`blender/city/interior_kit.py`), so a building can stand one indoors too.
- Building type: `blender/city/buildings/soda_machine.py`. Pad 0.9 x 0.75 m, height 1.91 m.
- Things: `plain` and `syrup` (the buttons). Spot: `drink` (0.85 m in front of the cabinet's middle, facing it).
- Dress sets: `broken` (May-October, on `BROKEN_SHARE` = 12% of days) and `winter` (November-April).
- Atlas cells (`tools/city/atlas.py`, late cells): `soda_sign`, `soda_plates`, `soda_broken`, `soda_rinse`.
- Lint: shell 36 tris, detail 608 tris, dress 14 and 70 tris; no stretched or degenerate UVs.
- Previews: `renders/soda/soda_machine_three.jpg`, `renders/soda/soda_machine_front.jpg` (rendered here without the
  house textures, so the colours are flat).

## The data

`tools/city/content/x_soda.json`, strings in `tools/city/content/strings/x_soda.json`.

- Acts `soda_plain` (price 1), `soda_syrup` (price 3), `soda_closed` (the words when it is shut: winter or broken).
- Rules `soda_machine/plain` and `soda_machine/syrup`, with two new fields:
  - `"months": [5, 6, 7, 8, 9, 10]`: the thing is shut outside these months.
  - `"broken": 0.12`: the share of days it is shut. The die is the same one the dress set `broken` uses
    (`DressHash01` of `"<plot>/broken@"` and the day), so the sign and the shut buttons always agree.
    `interiors.py --check` fails if the building has no `broken` dress set, or its chance or months differ.
  - Both need a `closed` act.
- Proc steps can now carry:
  - `"chance": 0.15`: the step happens on that share of scenes, and at most once a day per thing (the coin that sticks).
  - `"stop_below": 1.0`: on a scored step, a result under this ends the scene there (no thump, no water).
- Collection `soda_syrups`, reward `pay: 20`. Sound `soda_pour` is an alias of `rest_pour` until a sound of its own exists.

The coin that sticks: a `tap` step (3 taps in 4.5 s: thump the side). Thump it and the scene goes on. Miss it and the
scene ends poor, no stamp.

Words: the coin, the rinse prompt and its two answers (three lines each), the syrup of the day, pouring, good, fair and
poor endings, the closed words for winter (some for snow) and for broken days. Some lines depend on the weather, the hour
and the festival.

## Residents

`tools/town/town_activities.py` adds:

- place `S-SODA` (kind `soda_machine`): one `stand` slot at each machine's `drink` spot, facing it;
- activity `soda_drink`: 9:00 to 21:00, by day, 22 to 40 °C, ages 6 and up, 3 to 6 minutes.

`manners.json` has a `drink` row for `soda_drink` at `soda_machine` (the cup prop). Barks: `tools/barks/lines/18_soda_machines.txt`
(sets `soda_kid`, `soda_grown`, `soda_elder`; keys `town.soda_*`).

The 22 °C floor keeps them to the season in practice. The walkers do not know a machine is broken or hooded.

## Diary

Topic `soda` in `tools/daylog/content.py`, from the events `do` and `procedure` with target `soda_plain` or `soda_syrup`.

## Dev mode (F7)

- Teleports `city.sm_soda1` .. `city.sm_soda6` (made by `interiors.py`).
- Action `interiors.soda_jam` (group interiors): the coin sticks in every scene while it is on.

## Checks

- `python3 tools/city/interiors.py --check`
- `tools/citysim/run.sh`: `tools/citysim/sodachecks.h` (the six machines, money, rinse remarks, the six syrups and their
  stamps, the stuck coin at most once a day, the dev action, winter and May, broken words, the broken days match the
  dress die, about 12% broken).
- `python3 tools/town/town_activities.py --check`, `tools/barksim/run.sh`, `tools/daylog/run.sh`

## Depends on

Branch `interiors-touch-1127` adds the act field `"scene": true`. Until it is merged, the soda scenes run as sanatorium
procedures: they count in the procedure total and send the `procedure` event. After the merge, add `"scene": true` to
both soda acts' `proc` and rerun `interiors.py`.

## Known gaps

- The people's walk grid (`people_map.py`) does not treat the machines as obstacles.
- Residents hold the cup prop, not a faceted glass.
- The machine has no sound of its own (`rest_pour` stands in), and no clunk for the coin.

## Owed on the Mac

1. The atlas: `python3 tools/city/atlas.py`, then paste the strip x 1584..3168, y 3988..4096 of the new atlas into the
   committed `textures/city/atlas_D.jpg` (the jpg is LFS here). The cell positions are in `blender/city/atlas_cells.json`.
2. The mesh: `Blender -b -P blender/city/build_city.py -- soda_machine --no-render`
3. Import: `TYPES=soda_machine unreal/scripts/import_city.sh`
4. Place: `STEPS=place unreal/scripts/import_city.sh` (`place_street_machines` in `import_city.py` puts the six in
   `City/Street`).
5. Compile (`CityThingsModel.h`, `LyudCityThings.cpp`), then shots at the six teleports, a summer and a winter day, a
   broken day, and a bench run.
