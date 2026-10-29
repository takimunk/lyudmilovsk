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
