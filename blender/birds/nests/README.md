# Nest kit (Blender, procedural)

    Blender -b --factory-startup --python blender/birds/nests/build_nests.py -- [--only <Id>] [--previews] [--no-lint] [--no-export]

Builds every mesh, lints each one with `blender/birds/lint_bird.py` (face attribute `lint_part` per twig / blob / shell; removed before export),
and writes to `export/`: `nest_kit.glb` (one `M_NestKit` material, no embedded images, every mesh a separate object named as below, no armature),
`nest_kit.json` (per mesh: tris, bounds, outer radius, origin convention, stage / species, lint counts; atlas cell rectangles; per Full nest
`eggSlots` [x, y, yawDeg] for `clutchMax` eggs resting on the floor), `T_NestKit_C.png` (sRGB), `T_NestKit_N.png` (OpenGL normal),
`T_NestKit_R.png` (roughness, linear), all 2048 x 2048. `--previews` adds `export/previews/` (EEVEE renders and contact sheets made with
`blender/birds/contact_sheet.py`). `--only Rook` builds one species (glb/json get the species suffix; `--only Boxes` the boxes, tufts, bits).
Seeds are `zlib.crc32(id [+ stage])`, so the output is deterministic. Full build with lint and previews: about 4 minutes.

Files: `build_nests.py` (driver, lint, export), `nest_atlas.py` (atlas cells + numpy painting + PNG writer), `nest_geo.py` (mesh builder: tubes,
blobs, revolved shells, per-face uvs kept inside their cell), `nest_kinds.py` (platform, dome, cup, mound, mud cup, scrape),
`nest_specs.py` (per-species recipes), `nest_misc.py` (eggs, boxes, tree hole, burrow, tufts, bill bits), `nest_preview.py`, `nest_table.json`.

## Names and origins (Blender: Z up, forward +X; glb exported with the glTF +Y-up conversion like build_bird.py; metres)
* `SM_Nest_<Id>_B1 / _B2 / _Full / _Old` for every table species whose nest is Cup, Dome, Platform, Scrape, Mound or MudCup. Origin = centre of the
  cup floor (z = 0 is where eggs lie in `_Full`; B1/B2 are lower floors). Outer XY radius = the table radius (a few sticks end exactly on it).
  B1 = first third (base / frame), B2 = two thirds, Full = lined, Old = flattened (z x 0.7), greyer cells, 35-60 % of the material, hanging strands.
  The magpie dome's entrance faces +X.
* `SM_Nest_BarnSwallow_*` (MudCup): half cup, flat open back faces -X on the plane x = `wallPlaneX` (= -0.3 x radius, see the json; a plane at
  -radius would leave no cup), cup axis vertical through the origin.
* `SM_NestBox_Tit` (32 mm hole), `SM_NestBox_Starling` (47 mm, perch, slanted roof), `SM_NestBox_Sparrow` (35 mm), `SM_TreeHole` (oval hole on a bark
  patch of a 0.175 m trunk, axis at x = -0.175, chamber 0.22 m deep), `SM_Burrow` (6 cm tunnel 0.30 m deep along -X in a 0.5 x 0.4 x 0.4 clay bank).
  Origin = centre of the entrance hole, entrance facing +X, back toward -X. Boxes are closed solids (outer skin + inner cavity joined only through
  the hole). Jackdaw uses the Starling box or the tree hole; swifts have no mesh.
* `SM_Nest_HouseSparrow_Tuft`, `SM_Nest_WhiteWagtail_Tuft`: origin at the gap, straws and feathers stick out along +X.
* `SM_Egg_<Id>` for all table species: long axis along X (tip to +X), origin under the centre at the lowest point (egg lying on its side), real size.
* `SM_NestMat_Twigs/Grass/Moss/Mud/Reed/Feathers`: a beakful for a ~20 cm bird, origin at the grip, running along +-Y.

## Atlas (`nest_atlas.CELLS`, texels, y up, v = y / 2048; the json has every rectangle)
256 px tileable cells: bark_brown, bark_grey, bark_dark, bark_silver (weathered), bark_pale (flaking), bark_green (fresh sprig), grass_dry, grass_fine,
grass_bleached, moss_green, moss_dry, lichen, mud_wet, mud_dry (pellets / cracks), reed, down, feather, guano, clay, pebbles, earth, wool, rootlet,
seaweed, leaf_dead, soot (dark hollow), wood_raw, treebark, wood_cut. 512 px: plank_blue, plank_white, plank_ochre (weathered paint).
128 px: `egg_<Id>`, one per species, painted in the egg's own (azimuth, length) space (u wraps round it, blunt end v = 0). Faces on tileable cells are
shifted by whole cells when needed, so every uv stays 1.5 texel inside its cell (lint `uv_outside` = 0).

## Quality
Lint (degenerate, uv_zero, uv_stretch, uv_outside, zfight, inside) runs on every mesh; the summary table is printed at the end of the build and the
counts are in `nest_kit.json` (`lint_*`). Egg cells are registered as parametric cells for lint (like `body` in the birds): a squeeze towards the egg's
poles is the parametrisation, checked for zero area and bleeding only.
