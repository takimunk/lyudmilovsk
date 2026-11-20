# Flowering of trees and shrubs

Every tree and shrub species of the procedural plant system (`FPlantSpecies::Blossom`, `FPlantBlossom`) has a
flowering season: catkins on birch, alder, poplar, aspen and oak, blossom on apple, rowan, maple and sea buckthorn,
flowering cones on pine, spruce and juniper. Herbs and house plants keep their own flowers (`FPlantHerb`).

## Calendar

`PlantModel::Season(species, seed, day)` reads the day of the year from `UGameCalendar` (through the plant's
`CurrentDay()`), shifts it by the tree's own phenology jitter (so neighbours bloom a few days apart) and gives
`Bloom` (how much is out), `BloomOpen` (their size), `BloomAge` (0 fresh .. 1 spent), `BloomTint` and `BloomLook`.
A flower opens from `StartDay`, is full at `PeakDay` and has gone by `EndDay`; `Bloom` falls with the square of the
age so most of them drop in the last third. Nothing is stored: it is a pure function of the date, like leaves and fruit.

Approximate peak days (Kaliningrad coast): alder 76 (mid-March), aspen 90, poplars 100 (10 April), birches 118-128
(end of April), maple 120, oak 133, apple 134 (mid-May), spruce 138, pines 150, rowan 150 (end of May).

## Drawing

* **Near** (`AProceduralPlant`, inside `SummaryDistance`): `FPlantSkeleton::Evaluate` puts `Out.Blossoms` instances on
  the twig tips (spurs on apple), at most `250000 / triangles per instance` (3000) per tree, thinning as the bloom
  goes over. One ISM (`BloomComponent`) draws them with the leaf material (`M_PlantLeaf`: sway, flutter, per-instance
  shade) and the species' atlas (`PlantMeshes::BuildBlossomAtlas`), no shadows, culled at ~1000 flower sizes.
  The thin twigs also take on the flower colour (`TwigColour`), a haze that the drawn flowers alone do not give.
* **Far** (`UTreeBakeCache`, forests and plants beyond `SummaryDistance`): the bake has a fourth mesh section
  "Bloom" on LOD0 and LOD1 only (LOD1: every third, 1.6x larger), never shadow casting; LOD2+ carry no flowers, only
  the colour: leaf and card colour and twig colour are mixed toward `BloomLook` by `FarTint * Bloom`.
  A bake is remade when the day has moved 5 days; a far plant now takes the newer bake over as soon as it is ready
  (before, at its next re-shape, which at a game-clock speed of 1 is hours later).

## Adding or tuning

Presets live in `ApplyBlossom` (PlantSpecies.cpp), keyed by species name; a new species gets a `Form`
(`Catkin`, `Blossom`, `Cone`), its days, size, colours and `FarTint`. `lyud.PlantBloom 0` switches all of it off
(plants and bakes follow when they next re-shape or re-bake). Check: `unreal/scripts/make_plant_bloom_level.py`,
then `shoot_plant_bloom.py` (offscreen; `BLOOM_ONLY`, `BLOOM_QUICK`).

## The North

The North's shrubs (`PlantSpeciesNorth.cpp`) set their `Blossom` in the species itself, not through `ApplyBlossom`;
the dates are Kamchatka's, a month or more after the coast's:

| Species | Form | Days (start..end, peak) | Look |
|---|---|---|---|
| SiberianDwarfPine | Cone | 172..200, 186 | purple-red male cones in clusters at the shoot bases |
| DwarfAlder | Catkin | 150..180, 165 | yellow-green hanging catkins as the leaves open, two to a twig |
| GoldenRhododendron | Blossom | 158..192, 175 | pale yellow five-petalled flowers, six to a cluster, as the snow goes |
| SakhalinWillow | Catkin | 145..170, 156 | small yellow hanging catkins as the leaves come, two to a twig |
| BarWillow | Cone | 128..158, 140 | upright silver-yellow pussy catkins on the bare wands, before the leaves |

The North's herbs flower through `FPlantHerb` like the rest: fireweed 192..242 (magenta), mountain avens 168..205
(white), cotton grass 175..228 (white heads); the river valley's Kamchatka meadowsweet 192..228 (cream plumes),
cow parsnip 180..218 (white umbels); river sedge draws none. Check: the bloom level lists the North's shrubs (`make_plant_bloom_level.py`,
`shoot_plant_bloom.py`); `make_plant_north_level.py` / `shoot_plant_north.py` show all eleven by age and season.

## Garden crops

The allotment's six crops (`PlantSpeciesGarden.cpp`, `PlantHerbs::GardenSpecies()`, docs/allotment.md) are herbs of the
procedural plant system like the rest, one `AProceduralPlant` to a bed cell (0.6 x 1.2 m):

| Species | Form | Look when grown |
|---|---|---|
| GardenRadish | Rosette, annual, `Berry` | low rosette (~0.12 m) of rough lyre-shaped leaves; one red ball on a 4 cm stalk: the root's shoulder showing at the soil |
| GardenDill | Bush, annual, `Disc` | 2 thin blue-green stems 0.65-0.9 m, thread-fine leaves (7 leaflet pairs), flat yellow-green umbels |
| GardenStrawberry | Rosette, perennial, `Berry` | ~0.15 m rosette of three-lobed leaves on long stalks; red berries, 4 in 10 still white flowers |
| GardenPotato | Bush, annual, `Five` | bushy 0.5-0.6 m clump, dark compound leaves, white and lilac flowers |
| GardenTomato | Bush, annual, `Berry` | 0.8-1.15 m leggy stems, compound leaves, round red fruit ~5 cm, 3 in 10 still yellow flowers |
| GardenSunflower | Bush, annual, `Daisy` | one thick stem 1.4-2 m, big heart-shaped leaves, a nodding 0.28 m head (yellow rays, broad brown disc) |

`EPlantFlowerForm::Berry` (appended to the enum) is a smooth low-poly ball (34 vertices, 64 triangles) `FlowerSize`
across, hanging under each flower's place, coloured by `FlowerColours`; `Petals` (0..10) is how many of every ten places
carry a small five-pointed flower in `CentreColour` instead. A `Daisy` over 15 cm (the sunflower) gets a seed disc of 60%
of the head and nods to the side. A grown crop is 1-11k triangles (potato the most, then tomato and dill).

**SetGrowth.** The garden pins each plant: `AProceduralPlant::SetGrowth(float Growth01, float DayOfYear)`, Growth01 0
just sprouted .. 1 full grown and ripe. A pinned herb takes `PlantHerbs::GrownState`: alive and in leaf whatever the
calendar, the size of its growth (a sprout is the young plant scaled down to 35%), flowers and fruit from 0.4, all out by
0.9; no demo growth, no aging. It is re-shaped only by the call, the mesh only when the visible state moved (~2.5% of
growth), and its Tick does nothing (it ticks every 5 s). `ClearGrowth()` hands it back to the calendar;
`SetWithered(bool)` browns it through, drops a third of its leaves and all flowers and fruit (one rebuild); `bAllowPick
= false` keeps E and the pick prompt for the garden. A garden crop: `Pot = None`, `bSnapToGround`, `bAllowPick = false`,
ideally `SetGrowth` before `FinishSpawning`. Placed on their own the crops live by the calendar: the annuals from their
sowing day (115-140) to the frost (275-290), flowering in their weeks.
