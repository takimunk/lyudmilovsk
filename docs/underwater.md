# Under the water

The Baltic off the club's beach as a diver sees it: a shelving sand bed that drops off past the tidal flat to ~15 m,
moraine reefs and boulder fields, eelgrass meadows, wrack and red weed on the stones, currents that lean the weed and
carry the motes, and sixteen kinds of fish that live by habitat, month and light. Swimming and the air are the swim
thread's (`ULyudSwimComponent`); this is what there is to see.

## The bed (`tools/world_map.py`, `TerrainGen.cpp`)

- **Depth**: past the tidal flat's knee the old 6% bed no longer stops at 9 m: it keeps falling, ever more gently, to
  `OPEN_BED` (-14.8 m under the datum): 11 m some 30 m past the knee, ~14 m 120 m on. `tools/railway.py` takes its bed past
  the map from `OPEN_BED` too.
- **Detail** (`LyudTerrain::SeabedDetail`, plain C++, on the landscape like the land's): wave ripples ~5 m apart along
  the shore on the sand 2-9 m deep; sand waves ~32 m apart across the longshore current from 10 m (a long gentle stoss,
  a steep lee to the north); moraine reefs past the drop-off: shore-parallel ridges of the till 1-3 m high in stretches
  of the coast, lumpy with boulders and their gaps; boulder and cobble fields in patches from 4 m; silt in the deep
  hollows. Reef crests stay over 2 m under the lowest spring water.
- **Layers**: stones as the Dirt layer (gravel), silt as Mud, sand elsewhere.
- **Mask**: `ULyudTerrainMask::GetSeaStone` (0..1 stone cover, 2 m cells), written by the terrain build. The seabed life and
  the fish read it; an older mask without the channel falls back to noise.
- **Kept plain**: `tools/terrain.py sea_keep` keeps the bed plain sand along the railway's bank (±22 m of its line) and round
  the Europa hatch's spot.

`tools/underwater/run.sh bed` writes fresh terrain inputs to `Saved/Underwater/bed`, runs the generator outside the engine
and draws the bed (`bed_depth.png` shaded relief, `bed_stone.png` the stones). The landscape only changes with a ground
rebuild (`unreal/scripts/build_scene01.sh`).

## The Europa hatch's spot

**Map (573.0, 100.0)**, Unreal **(57300, 10790)** cm (`world_map.EUROPA_HATCH`): plain sand on the outer slope off the club's
beach, 11 m under the datum (bed at map Z -54.0, Unreal -5400 cm), so 6.5 m deep at the lowest spring water and 9 m at
neaps' low. It lies between the railway's bank (y -60) and the next tidal channel north (y ~270); the nearest stones are a
cobble field ~40 m east. From the spring-low waterline it is a ~110 m swim, from the high-water line ~440 m. The bed is kept
plain sand 30 m round it (fading out to 45 m).

## Plants (the plant system: `Private/Plants/PlantAlga.cpp`)

A new herb form, `EPlantHerbForm::Alga` with `FPlantAlga` (ribbons, sheet, forked fronds flat or round with a midrib, air
bladders and receptacles, filaments, kelp with holdfast, stalk and fingers; a year of growth and browning tips). Species
(`PlantHerbs::SeaSpecies`, names `plant.<id>.name` in ru/en/ja):

| Species | Where (ULyudSeabedComponent) |
|---|---|
| Eelgrass (Zostera marina) | meadows on sand 1.5-7.5 m; longest in August, tips brown and shed in autumn |
| Bladderwrack (Fucus vesiculosus) | stone tops to 3.5-6 m; bladders, ripe tips Sep-Dec |
| Furcellaria lumbricalis | stones deeper than the wrack; loose tumbled mats on the deep sand |
| Cladophora glomerata | green tufts on stones in the first 3.5 m, summer |
| Sea lettuce (Ulva) | shallow stones, summer |
| Oarweed (Laminaria digitata) | a few on the biggest reef blocks past 9 m (rare in the brackish Baltic) |

Each species is grown for the calendar's day in 3 seeds (the meshes regrow in place when their state changes) and drawn in
`M_LyudSeaAlga`: lean downstream with the current (drag against buoyancy), sway with the waves' orbits, flutter in the
eddies. Rocks: rounded erratics, split slabs and leaning pairs (`SeaLifeMeshes ReefRock`), cobble patches, blue mussels in
the crevices, shells on the sand; moon jellyfish from mid-June to mid-November.

## Currents (`Private/Sea/SeaCurrent.cpp`, plain C++)

`ULyudUnderwaterSubsystem::CurrentAt / MeanCurrentAt` (cm/s): the tidal stream along the coast (north with the flood, up to
~0.33 m/s at springs at mid-tide), filling and draining across the flat; the wind's longshore current (~2% of the alongshore
sea-state wind, a jet over the bars); a drag layer under the surface; divergence-free eddies a few metres across (more over
stones); a log layer slowing it toward the bed. Materials get `MPC_LyudUnderwater` (`Current`: the mean at the eye's depth
and the turbulence, `CurrentDrift`: how far the water has carried things, `Eye`: under water, depth, level, light).
The motes (`M_LyudSuspended`: 1800 quads in a 14 m cube round the eye, placed and carried only in the vertex shader, after
the underwater fog) show the water moving.

## Fish (`Private/Sea/FishModel.cpp` plain C++; drawn by `ALyudSeaLife`)

| Species | Way |
|---|---|
| Baltic herring, sprat | schools in mid water (autumn inshore); open round a diver; bolt from hunters |
| Sand eel | schools over sand, dive into it when struck (out again in 20-60 s), nothing at night |
| Three-spined stickleback, perch | loose shoals by weed and stones |
| Cod (more at night), sea trout | lone hunters: cruise, pick a school within 12 m, strike, rest |
| Garfish | small groups under the surface, May-September only |
| Round goby (stones), sand goby (sand) | sit facing the current, hop, dart off |
| Eelpout, shorthorn sculpin, lumpsucker (Feb-Jun) | lurk among the stones |
| Flounder (sand), turbot (sand and gravel) | lie flat, glide off a few metres when you come close |
| Broadnosed pipefish | stands head up in the eelgrass, May-October |

Bottom fish are settled per 6 m cell from a hash (the same fish in the same place every visit); roamers come in from beyond
13 m (out of sight) and leave beyond 31 m. Meshes: Blender (`blender/fish/build_fish.py`, a parametric generator, one
species file each in `blender/fish/species/`), imported by `unreal/scripts/import_fish.sh` to `/Game/Sea/Fish/SM_<Id>`,
`T_<Id>_D`; a species without a mesh swims as the old code-built fish. `M_LyudFish` / `M_LyudFishFin` bend them with
per-instance data (phase, beat, bend into a turn) and the vertex colours (along the body, flexibility, paired fins).

## Cost

Nothing runs or is drawn unless the eye is under the water or within 5 m over it (8 m for the bed's life); away from the sea
the instances and the fish are freed. The weed and stones are rebuilt only when the eye crosses an 8 m tile; the fish move
their instances in place every frame (no render state rebuild), out of Lumen and the distance fields, without shadows.
Console: `lyud.SeaLife 0`, `lyud.SeaLife.Fish 0`, `lyud.SeaLife.Motes 0`, `lyud.SeaLife.Density`, `lyud.SeaLife.Debug 1`,
`lyud.Current.Speed <m/s>` (forced along the coast), `lyud.Seabed 0`, `lyud.Seabed.Radius <m>`.

## Checks

- `tools/underwater/run.sh test`: the tidal stream and the wind's current in range, slower at the bed, divergence-free
  eddies; the fish over four depths: never under the bed or out of the water, never faster than they swim, 14 of 16
  species about in October and no garfish, herring keep their school, a flounder glides off when approached.
- `unreal/scripts/test_underwater.sh` (offscreen, `UWTEST_ONLY=a,b`): a line of depths off the beach, the hatch's spot, a
  meadow, a reef, wrack, cobbles, each fish, the current still and at 0.6 m/s, from the surface, a reef at night →
  `Saved/UnderwaterTest/<step>.png`.
- `blender/fish/`: `tools/fish_build.sh` builds, lints and previews the fish (`export/contact.png`).
