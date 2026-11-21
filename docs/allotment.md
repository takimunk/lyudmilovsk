# The allotment

Backlog item 9 (docs/interactives_backlog.md). The hero rents a bed in the commune house's allotments, digs it, sows,
waters, weeds, covers it on cold nights, picks the crop and eats it, gives it, sells it at the market or has the
neighbour cook it into jam for the Sunday contest. The crops grow by the hour from the game's own weather and are drawn
by the procedural plant system. Nothing is shown as a number: the prompt line, toasts and the neighbour's words say what
the bed needs.

## Where the code and data live

| Part | Where |
|---|---|
| The rules (engine-free C++) | `Source/Lyudmilovsk/Private/Garden/GardenModel.{h,cpp}` (namespace `LyudGarden`) |
| The game side | `Public/Garden/LyudGarden.h`, `Private/Garden/LyudGarden.cpp` (`ULyudGarden`, a tickable world subsystem, saved as "Garden") |
| The crops drawn | `Private/Plants/PlantSpeciesGarden.cpp` (GardenRadish, GardenDill, GardenStrawberry, GardenPotato, GardenTomato, GardenSunflower); `AProceduralPlant::SetGrowth / SetWithered` |
| Structure | `tools/garden/data.py` (rules, crops, items, prices, jam, the site, beds, the neighbour, props) |
| Words ru/en/ja | `tools/garden/words.py` (Russian first; Maarja's lines go out as `line.garden.*` bubbles) |
| Generated | `tools/garden/build_garden.py` writes `Content/Garden/garden.json` and `Content/Localization/garden/strings_{ru,en,ja}.json`; `--check` only checks |
| Offline test | `tools/gardensim/run.sh` (a summer, the game's own autumn, neglect, drought, the film, actions, save/load, catch-up, the neighbour's bed) |
| Icons | `localization/icon_grids_garden.json` (two Higgsfield grids, job ids pinned), `tools/garden/icons.py` (download, sidecar, cut) |

After changing `data.py` or `words.py` run `python3 tools/garden/build_garden.py`; it checks the site against the city
plan (nothing of the plan may stand in it), every item against `LyudInventory.cpp`, every species against the plant
files, the neighbour against the townsfolk and the market's scales against `interiors.json`.

## The place

The commune house's yard (P-T2H2), behind the house on the inland side: a 14 × 26 m fenced yard with a gate, six beds
(the hero's, Maarja Berntsen's and four other residents'), the house committee's notice board at the gate, a rain barrel,
a shed and a bench. It is built from simple boxes in its own data, the city plan is untouched. The hero's bed has six
cells (2 × 3), each with its own soil box and one `AProceduralPlant`; the other beds are drawn by the day of the year
(`NeighbourGrowth`: sown in May, picked in August, dug over in October).

## Playing it

E acts (polled, never consumed). The prompt line names what E does for what he looks at:

- **The board:** rent the bed (30 roubles, his for good), then it reads the committee's notice.
- **The barrel:** fills the can (six waterings); frozen in a frost; "no can" without one.
- **A cell:** dig (spade) → sow (opens a 1–9 chooser of the seeds and seedlings he carries) → water (the whole bed with
  the can when several cells are dry) → weed → cover / uncover with film → pick → clear a dead or spoilt crop. A sack of
  compost dug into a cell makes it grow faster for two weeks. Otherwise a status line in words.
- **The neighbour** (Maarja, on weekday mornings 7:00–10:30 in dry weather): advice every minute or so by season, by the
  coming frost and by the worst need of his bed; give her six strawberries and a jar of jam stands on the bench 20 hours
  later.
- **The market's control scales** (7:00–18:00): sells the harvest by its quality; Monday to Friday takes a jar for the
  contest, judged Sunday at 16:00 (a letter with the result, prizes 50 / 30 / 15 roubles, a diary note).
- **Friends** (item 12) at level 2 and up hand him a seed packet now and then (weekly at most); the harvest and the jam
  are gifts friends like.

Seeds, the spade, the can and compost are sold at the hardware shop; seed potatoes, seedlings and radish seeds at the
market's fruit counter.

## Growing

Each cell keeps growth 0..1, soil water, weeds, frost and drought damage, care (a running mean that makes the harvest's
quality 1..3), pickings and compost. Every game hour:

- **Warmth:** the bed's air is the weather's plus the sun on open soil (1.5 C in full sun) or the film (2 C at night,
  up to 10 C in sun). Growth is zero at the crop's base, full from its best warmth, slower above hot.
- **Water:** rain wets an open bed (0.08 per mm); the crop drinks by its thirst, more in warmth, sun and wind, less
  under film. Below 0.3 it slows; 36 hours dry and it starts to hurt.
- **Weeds** grow in warm hours (none to rampant in four warm days) and slow the crop above a quarter.
- **Frost** below the crop's limit hurts it (0.25 per degree-hour; 1 kills). Strawberries are perennial and survive the
  winter.
- **Ripe** crops keep for their ripe days, then spoil over four days. Dill, strawberries and tomatoes ripen again after a
  picking.
- **Catch-up:** a save left for months is stepped at most 60 days, with the weather model run for each hour.

| Crop | Days | Notes |
|---|---|---|
| radish | 3 | five a bunch |
| dill | 5 | three pickings, 2.5 days apart |
| strawberry | 8 | seedling, perennial, four pickings |
| potato | 10 | six a cell |
| tomato | 12 | seedling, tender (hurt below 0.5 C), three pickings |
| sunflower | 12 | gives the town's own семечки |

The game starts on 20 October: in the open the autumn is too cold for most crops, so radishes under film take about nine
days and tomatoes fail. gardensim checks a kept summer (every crop picked, mostly top quality), a July radish in 2.5–4.5
days, the game's autumn and neglect.

## Performance

The model steps every 10 seconds, not per frame. The plants exist only within 90 m (removed at 110 m) and are re-shaped
only when their growth has moved by 0.025 or they die, so a bed is re-built a few times a day at most. The site's boxes
are spawned once.

## Developer mode

F7 teleport `garden_bed`; actions `garden.kit` (rent, spade, full can, a packet of each seed), `garden.grow_day`
(the bed jumps a day ahead in the weather of now), `garden.report`. Console `lyud.Garden.Report`, `lyud.Garden.Grow
<hours>`, `lyud.Garden.Kit`.

## Still to do on the Mac

The first build of the engine side and the plant changes; `python3 tools/garden/icons.py` (download, cut, look at each
icon), then `import_hall_assets.py` and the diary sketches; shots of three stages (teleport, kit, sow, grow a day at a
time); a bench run.
