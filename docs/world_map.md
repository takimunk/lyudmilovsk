# The outdoor map

The whole game's outdoor map, laid out at real-world scale. Everything here is a placeholder for
now; the shape of the land and where things stand live in one file, `tools/world_map.py`, which the
Unreal build turns into geometry every time (`unreal/scripts/import_world.py`, nothing committed).
`python3 tools/render_world_map.py map.png` draws it from above. `build_scene01.sh` adds it to
Lvl_Scene01 in a second editor process (import_world.py); `make_world_preview_level.py` builds a level
with only the map.

**Biome: the Sambian coast of the Baltic** (a forested upland ending in steep clay cliffs over narrow
sand beaches, the Academgorodok in a valley where the town will be). Earlier notes said Krasnodar
Krai; the map follows the Baltic now.

## Frame

Metres, origin on the club's portal face at floor level (the spur). X is to the left of someone
walking out of the club (west, the sea), Y is straight out of the door (north), Z is up.
Unreal: `(X * 100, portal_face_y + Y * 100, Z * 100)` cm. The map runs X -1300..1300, Y -900..2200
(2.6 x 3.1 km). The sea is at Z -43, the valley at -37, the upland at +18.

## Places

| Place | Where (X, Y) | Now | Later |
|---|---|---|---|
| Upland | the whole south of the map, its north slope just behind the door (W.escarpment_y), swinging north inland | 18 m over the club's floor (61 m over the sea), under a thick old pine and spruce forest; the club's corridor runs under it | paths, clearings |
| Sea cliff | along the coast wherever the land is high | a steep clay and sand face (~53 degrees average, under 60) over a 25 m beach; W.cliff, W.cliff_top | stairs down, slumps (terrain) |
| Club | 0, 0 | a door in the upland's north slope: forecourt cut into the hillside, concrete wing walls, a canopy | lamps, a sign |
| Spur | in front of the door, Y -30..180, open to the cliff in the west | a terrace at the club's floor level; falls ~37 m to the valley on its east and north sides | |
| Alley | path from the door, 3..120 m north, bending right to the cable car | concrete slab path, 12 young poplars and maples (AProceduralPlant), a rowan and an aspen before the station | benches, a lamp post |
| Cable car | upper station -15, 133 on the spur's brow, lower station -255, 520 in the valley | two stations, a tower on the brow, two ropes, red cabins | working cabins |
| Beach under the club | X 110..135 | sand at the cliff's foot; dune pines and juniper on the spur by the cliff, sea buckthorn at its foot; meadow grass on the spur, dune grass on the beach | boats |
| Starlings | 430, 250 | the murmuration over the sea, left of the door | |
| Valley | Y ~250..1450, 6 m over the sea, open to the sea | meadows, wet alder and birch copses, reed and sedge on the wet floor below the lower station (past the Academgorodok's levelled lots) | |
| Lyudmilovsk (town) | centre -330, 900, ~1.1 km across, in the valley | only the Academgorodok (north of the lower station); the placeholder houses are gone (2026-10-25), the ground stays level, trees grow sparse over it | whatever Dima decides |
| Promenade | along the valley's beach, Y 450..1250 | a concrete strip | kiosks, railings, a pier |
| North hills | beyond the town, from Y 1450 up to +12 m | old forest | |
| Forests | upland, north hills, inland ridge, dune bank, valley copses, the town | 57 AProceduralForest actors, ~100k real trees (shared bakes) | |

The macro shape (W.height) is smooth; natural detail on top of it, the ground surface and its
collision are the terrain thread's (tools/terrain.py). W.keep_flat(x, y) says where the exact macro
height must stay: the apron and forecourt, the path, the stations and towers, the promenade, the town.

## Performance notes

Measured 2026-10-21 in an offscreen `-game` run at 1600 x 900 at the door, after the forests were
baked (all ~100k trees drawn within 30 s, 56 shared bakes): frame 21..23 ms median, 32..35 ms p95,
GPU-bound; game thread 2.5 ms. The 31 hero plants near the club are ~11M leaf triangles up close
(shrubs and dune pines switch to their summary at 50..80 m). Grass: 24 patches of 60 x 60 m near the
club only. A map this size will probably want World Partition: a shared decision for later.
