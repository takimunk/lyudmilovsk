# The outdoor map

The whole game's outdoor map, laid out at real-world scale. Everything here is a placeholder for
now; the shape of the land and where things stand live in one file, `tools/world_map.py`, which the
Unreal build turns into geometry every time (`unreal/scripts/import_world.py`, nothing committed).
`python3 tools/render_world_map.py map.png` draws it from above. `build_scene01.sh` adds it to
Lvl_Scene01 in a second editor process (import_world.py); `make_world_preview_level.py` builds a level
with only the map.

**Biome: the Baltic coast** (sand beaches, high shores, pines and broadleaf woods, a small town with
old tiled roofs and Soviet five-storey blocks, think Svetlogorsk). Earlier notes said Krasnodar
Krai; the map follows the Baltic now.

## Frame

Metres, origin on the club's portal face at floor level (the hilltop). X is to the left of someone
walking out of the club (west, the sea), Y is straight out of the door (north), Z is up.
Unreal: `(X * 100, portal_face_y + Y * 100, Z * 100)` cm. The map runs X -1300..1300, Y -900..2200
(2.6 x 3.1 km) inside the 2.8 km sky dome. The sea is at Z -43.

## Places

| Place | Where (X, Y) | Now | Later |
|---|---|---|---|
| Club | 0, 0 | dug into a hilltop; a 7 m rampart runs back over its 470 m corridor | |
| Alley | path from the door, 3..120 m north, bending inland | concrete slab path, 12 young poplars and maples (AProceduralPlant) 18..83 m, a rowan and an aspen at its end | benches, a lamp post |
| West flank and beach | beach at X 100..135 by the club | grass sloping 22..30 degrees down to a 35 m sand beach; Scots pines on the rampart, dune pines and juniper on the flank, sea buckthorn at its foot; meadow grass (AProceduralGrass) on top and down the flank, marram and lyme grass on the beach | dunes, boats |
| Starlings | 430, 250 | the murmuration over the sea, left of the door | |
| Bluff | north of Y 300 | the shore rises into a steep 40 m bluff | woods on the slope, paths, a lift |
| Cable car | Y 860, from the town's edge down to the beach | two stations, a tower, two ropes, red cabins | working cabins |
| Promenade | foot of the bluff, Y 640..1320 | a concrete strip | kiosks, railings, a pier |
| Lyudmilovsk (town) | centre -330, 1180, about 1.2 km across | ~760 houses with tiled roofs, five-storey blocks round the square, the old church tower, trees | streets, shops, the station, people |
| Woods | ridges inland (X < -750), north (Y > 1800) and south (Y < -380), copses between | cone pines and blob broadleaf trees | Baltic pines and procedural grass (the pine thread) |

## Performance notes

- The ground is one Nanite mesh (~0.7M of the map's 0.86M triangles, 0.5 m cells at the door, 4 m along the shore,
  20 m far inland); collision is complex-as-simple. The woods have no collision.
- Procedural trees are expensive up close: the 31 plants around the club add up to ~11M leaf
  triangles, all casting (non-Nanite) shadows; shrubs and dune pines switch to their summary at
  50..80 m. The town and the woods further off are cheap placeholder trees. More real trees need
  leaf LODs/impostors from the plants side first.
- Grass: 25 patches of 60 x 60 m (~0.5M tufts) around the club only.
  Forests of real trees will need impostors/HLOD and probably World Partition: a shared decision for later.
