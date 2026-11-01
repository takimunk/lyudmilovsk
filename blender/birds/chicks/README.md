# Nestling kit

Static chick meshes for every species in `blender/birds/nests/nest_table.json` (37 species x 3 ages x 2 poses = 222 meshes).

    Blender -b --factory-startup --python blender/birds/chicks/build_chicks.py -- [--only <Id>[,<Id>..]] [--previews] [--no-lint] [--jobs N]

- `SM_Chick_<Id>_<Hatch|Mid|Late>_<Rest|Beg>`: Rest = huddled low, head tucked; Beg = neck up, bill gaping
  (pigeons: head pushed forward, bill slightly open; gulls, swan, raptors: bill open, standing for gulls and swan).
- Blender coordinates: +X forward, Z up, metres; origin on the nest floor under the belly. glTF export is +Y up
  (`export_yup`), no rig, no embedded images (`export_image_format NONE`).
- One material `M_ChickKit`, one atlas `export/T_ChickKit_C.png` (sRGB), `_N.png` (normal, OpenGL), `_R.png` (roughness, R channel,
  linear), 4096 x 4096. Per species five 256 px tiles (skin, down, pin feathers, juvenile feathers, misc) and the misc tile holds
  four 128 px cells (gape + flange, bill, eye strips closed|slit|open, leg). Everything is painted with numpy, seeded by
  `zlib.crc32` of the species/age/pose.
- The per-species art table (colours, plumage pattern, gape colour...) is at the top of the script (`KIT`).
- `export/chick_kit.glb`, `export/chick_kit.json` (per mesh: tris, bounds, size, `billTip` = tip of the upper bill in the mesh's own
  Blender coordinates, `billBase`, `billDir`, `billUp`, `gapeDeg`, `head`), `export/lint_report.json`.
- Lint: `lint_bird.py` runs on every mesh while building (face attribute `lint_part` per body / head / bill half / flange roll / throat /
  eye / each down tuft, quill, feather card). Parts that z-fight (tufts, quills, cards, wings, flange rolls, eyes, feet) are lifted
  apart automatically and the mesh is linted again; the summary table is printed at the end. A non-zero total in
  degenerate / uv_zero / uv_stretch / uv_outside / zfight / inside means a defect.
- `--previews` renders (EEVEE, in parallel worker processes) `export/previews/<Id>_<Age>_<Pose>.png` (30 degree front-side view),
  `<Id>_gape.png` (Mid Beg, looking into the mouth), `sheet_<Id>.png` and the `ALL_*.png` contact sheets (via `contact_sheet.py`).
