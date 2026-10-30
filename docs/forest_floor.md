# The forest floor

What lies between the trunks of the woods (`AProceduralForest`): the ground's own micro-relief, the litter of needles
and leaves in the ground's material, and the 3D things on it near the player: dwarf shrubs, ferns, mosses, seedlings,
fallen leaves, cones, sticks, branches, logs, stumps, roots, stones, moss cushions. Baltic coastal pine, spruce and
birch woods (Zelenogradsk, Svetlogorsk). Everything follows the trees about, the calendar and the weather.

## Pieces

| What | Where | How |
| --- | --- | --- |
| Micro-relief | `Private/Terrain/TerrainGen.cpp` `ForestFloorRelief` | inside the forest mask only: mossy hummocks 0.2-0.45 m, shallow hollows, a windthrow (root pit ~0.45 m deep + soil mound ~0.55 m) on ~30% of 20 m cells; off tracks, protected ground and the bluff. On the 1 m landscape grid; finer bumps come from the litter photos' normals |
| Forest-floor mask | `ULyudTerrainMask::GetForestFloor` | the forest layer minus tracks, rock, sand, steep ground (2 m cells, built with the ground) |
| Litter field | `AForestFloor::LitterFrom`, `/Game/ForestFloor/RT_Litter` | 128 x 128 cells of 1.5 m round the camera: needles (pines, spruces), leaves (this autumn's + what is left of last year's), their hue, their decay; from every tree within 16 m (species, age -> height, crown radius, distance) and the calendar (each species' `LeafDropStartDay`..`BareDay`) |
| Ground litter | `M_Ground` node `ForestLitter` (`unreal/scripts/forestfloor_ground.py`) | over the forest layer: a leaf-litter photo tinted by the field's hue (birch/aspen yellow, maple/rowan orange-red, oak/alder brown), darkened and flattened by decay, feather-moss carpets where few leaves lie, warmer needles under pines; drought pales it, rime frosts it, wet flattens it (the rain's wet decal darkens and glosses it) |
| Ground cover | `AForestFloor`, plant system species | herbs `Bilberry`, `Lingonberry`, `Heather`, `Bracken`, `BucklerFern`, `WoodSorrel` (`PlantSpeciesFloor.cpp`, `FPlantSpecies::Herb`, 3 seeds each, a cheap LOD 1); grass tufts `FeatherMoss`, `HaircapMoss`, `ReindeerLichen`, `WavyHairGrass` (`GrassSpecies.cpp`); seedlings of `ScotsPine`, `NorwaySpruce`, `SilverBirch`, `Rowan` (their forest bakes, `UTreeBakeCache`, ages 2.5 and 5) |
| Litter and debris | `AForestFloor`, `Private/Plants/ForestFloorMeshes.cpp` | fallen leaves of every deciduous species in the woods (`PlantMeshes::BuildSingleLeaf`, by the field), falling leaves under the crowns in leaf fall, pine and spruce cones under their trees, birch bark curls, sticks, fallen branches, logs, stumps, roots round old trunks (pine, spruce and birch bark from `PlantMeshes::BuildBarkTextures`), granite and gneiss stones and boulders, moss cushions |

Placement: 8 m world cells aligned to the landscape's vertices, each traced at its 81 vertices once (ground height and
whether it is bare forest ground: the landscape, not a path or a yard), filled from a hash of the cell (every visit and
machine grows the same floor). What grows where comes from the stand round the cell (shares of pine, spruce, birch,
other broadleaf, and the light in the gaps) and patches over metres: lingonberry and heather under pines, wood sorrel
and buckler fern under spruce in damp ground, bracken in light birch stands, bilberry under all conifers, lichen in dry
open pine wood, feather moss carpets under the canopy. Three bands are laid out to their reach (near 18 m, mid 42 m,
far 70 m) plus a 12 m margin; when the camera has moved 6 m the kinds are re-committed a couple per frame from the
cells already laid. Kinds are hierarchical instances (HISM), each culled at its own distance with a fade.

## Season and weather

- **Calendar** (`UGameCalendar`, `DevDay` on the actor): herbs rebuild their mesh when their state changes
  (`PlantHerbs::State`: bilberry turns wine-red and drops its leaves from day 260-312, bracken browns from 255, ferns
  from 295, lingonberry/heather bronze in winter, berries in their weeks); mosses stay green; the near band (leaves,
  cones) is laid again every 3 days of the calendar; the litter field follows the day.
- **Rain** (`ULyudRainFogSubsystem::ReadWeather().Wet`, `lyud.ForestFloor.Wet`): everything darker and glossier (wet stone
  the most), leaf litter matted flat; puddles on flat ground are the rain's own wet decal (`M_WetGround`).
- **Frost / glaze** (`FLyudWeatherState::Frost`, `Glaze`, `lyud.ForestFloor.Rime`): rime on upward faces of litter,
  moss, shrubs and wood.
- **Drought** (dry and warm, `lyud.ForestFloor.Dry`): litter, moss and leaves paler and rougher.
- **Wind** (`MPC_LyudWeather.Wind`): shrubs and ferns sway by their height (a third of the open wind under the canopy),
  moss and grass stir, fresh loose leaves skitter in gusts, falling leaves drift with it.
- **Snow** (`ULyudWinterSubsystem`): the landscape's snow lift and `ALyudSnowNear`'s prints cover the floor as they are;
  anything lower than the snow lies there is not drawn at all, what sticks out is whitened on top. Litter after the
  thaw is matted and dark (decay grows through the winter and spring).

Parameters (added to `/Game/Weather/MPC_LyudWeather`: a landscape material may read only two collections): `LitterRect` (x0, y0, 1/size, on), `FloorWeather` (wet, rime, drought, rain),
`FloorSnow` (snow 0..1, wind 0..1).

## Console

`lyud.ForestFloor 0` hides it all (and the ground's litter). `lyud.ForestFloor.Density` (next layout),
`lyud.ForestFloor.Shadows 0`, `lyud.ForestFloor.Debug 1`, `lyud.ForestFloor.Wet / Rime / Dry` force the weather (negative:
the weather's).

## Performance

Measured 2026-10-30 (`tools/bench_forest.sh nofloor floorhalf`, Epic, one process, 2 rounds) on a heavily loaded Mac
(4 other Unreal/compile processes, ~100 ms frames instead of the usual ~26): forest segment 122.5 ms with the floor,
130.2 ms without, 130.2 ms at half density; terrace 87.3 / 91.1 ms. The floor's cost is below that noise; measure
again on an idle machine. Around the upland forest spot it holds ~63k instances in autumn (feather moss 19k, birch
leaves 26k, bilberry 7.8k, sticks 2.7k, cones 3.3k ...), ~100 in deep snow; laying out a cell is a few ms of traces
spread over frames (`TracesPerTick`). Cheap by construction: no collision except logs, stumps and boulders (simple
boxes); shadows only from logs, stumps, boulders, branches, roots and moss cushions (static, cached by the virtual
shadow map; no WPO); moss and leaves culled at ~13-15 m, herbs at 26-42 m with a LOD 1 from ~10 m.

## Building and checking

- `unreal/scripts/make_forestfloor_materials.py` (commandlet) builds `/Game/ForestFloor`; `FLOOR_GROUND_ONLY=1` also puts
  the litter into the existing `M_Ground` without rebuilding the ground (`=2`: only that). `import_terrain.py` puts it
  in when the ground is rebuilt.
- `unreal/scripts/shoot_forestfloor.sh` (offscreen `-game`): autumn eye-height and close views, a leaf carpet, wet, rime,
  drought, the frame time with the floor on and off in one process; `FLOOR_MODE=winter` with forced snow and a walk.
  Shots in `Saved/forestfloor_shots/`. FLOORTEST lines list every kind's instances, triangles and its mesh lint.
- Mesh lint (`ForestFloorMeshes::Lint`, in `DescribeState`): degenerate triangles, UV stretch (UVs in metres), coplanar
  overlapping faces of different parts, NaNs; every debris kind must read 0.
- `tools/bench_forest.sh nofloor floorhalf`: one-process A/B.
- Terrain relief without UE: build `TerrainGen.cpp` with a small clang++ driver (docs/terrain.md).

## Known gaps

- Seedlings are the forests' own tree bakes and queue behind them: right after the level opens they come in once the
  forest bakes are done (a few minutes).
- The ground's litter is drawn from the field only within 96 m of the camera (beyond, the plain forest photo).
- Puddles in the forest's hollows are the rain's generic wet decal, not the terrain's wetness.
- Leaves blown along the ground stir in place (vertex shader); they are not carried across the floor.
