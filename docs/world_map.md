# The outdoor map

The whole game's outdoor map, laid out at real-world scale. Everything here is a placeholder for
now; the shape of the land and where things stand live in one file, `tools/world_map.py`, which the
Unreal build turns into geometry every time (`unreal/scripts/import_world.py`, nothing committed).
`python3 tools/render_world_map.py map.png` draws it from above. `build_scene01.sh` adds it to
Lvl_Scene01 in a second editor process (import_world.py); `make_world_preview_level.py` builds a level
with only the map.

**Biome: the Baltic coast** (sand beaches, a hill over the sea, pines and broadleaf woods, a small town
with old tiled roofs and Soviet five-storey blocks in the lowland). Earlier notes said Krasnodar
Krai; the map follows the Baltic now.

## Frame

Metres, origin on the club's portal face at floor level (the hilltop). X is to the left of someone
walking out of the club (west, the sea), Y is straight out of the door (north), Z is up.
Unreal: `(X * 100, portal_face_y + Y * 100, Z * 100)` cm. The map runs X -1300..1300, Y -900..2200
(2.6 x 3.1 km) inside the 2.8 km sky dome. The sea is at Z -43, the lowland at -37.

## Places

| Place | Where (X, Y) | Now | Later |
|---|---|---|---|
| Club | 0, 0, on the top of the hill | dug into the hilltop; the entrance is a forecourt cut into the mound with concrete wing walls and a canopy over the door; a 7.5 m rampart runs back over the 470 m corridor | lamps, a sign |
| Club hill | centre -20, -80; flat top to ~130 m north of the door, down 37 m to the lowland by ~320 m | open grassy top; forests on its slopes | |
| Alley | path from the door, 3..120 m north, bending right to the cable car | concrete slab path, 12 young poplars and maples (AProceduralPlant), a rowan and an aspen before the station | benches, a lamp post |
| Cable car | upper station -15, 133 on the brow, lower station -255, 520 at the town's edge | two stations, a tower on the brow, two ropes, red cabins | working cabins |
| West flank and beach | beach at X 100..135 by the club | grass sloping down to a 35 m sand beach; Scots pines on the rampart, dune pines and juniper on the flank, sea buckthorn at its foot; meadow grass on top and down the flank, marram and lyme grass on the beach | dunes, boats |
| Starlings | 430, 250 | the murmuration over the sea, left of the door | |
| Lowland | north of the hill, 6 m over the sea | flat meadows, wet alder and birch copses | |
| Lyudmilovsk (town) | centre -330, 900, ~1.1 km across, 37 m below the club | ~700 houses with tiled roofs, five-storey blocks round the square, the old church tower, trees between the houses | streets, shops, the station, people |
| Promenade | along the beach in front of the town, Y 450..1250 | a concrete strip | kiosks, railings, a pier |
| Forests | the hill's slopes, the inland ridge (X < -830), the north (Y > 1820) and south (Y < -560) ends, the dune bank, the town | AProceduralForest actors, ~18k real trees (off until the forest crash is fixed: LYUD_FORESTS=1) | |

The macro shape (W.height) is smooth; natural detail on top of it, the ground surface and its
collision are the terrain thread's (tools/terrain.py). W.keep_flat(x, y) says where the exact macro
height must stay: the apron and forecourt, the path, the stations and towers, the promenade, the town.

## Performance notes

- The ground is a Landscape (docs/terrain.md): 1 m grid over the whole map, heightfield collision that
  is exactly the drawn surface, a small mesh patch in its hole at the portal.
- Procedural trees are expensive up close: the 31 plants around the club add up to ~11M leaf
  triangles, all casting (non-Nanite) shadows; shrubs and dune pines switch to their summary at
  50..80 m. Every other tree is in a forest (baked 4-LOD instances, each actor bakes its own kinds,
  so there are 11 big actors rather than many small).
- Grass: 25 patches of 60 x 60 m (~0.5M tufts) around the club only.
- A map this size will probably want World Partition: a shared decision for later.
