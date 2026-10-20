# Clerk (Excel painter NPC)

Middle-aged Japanese office clerk at Station 09 of Scene 01 who paints Mt Fuji in Excel.
Modelled from scratch, procedurally: every part is a signed-distance sculpt (`sdf.py`) meshed
with OpenVDB inside Blender, skinned from the same primitives. No third-party assets; the
Excel window chrome is cropped from this repo's `textures/podyezd/screen_spreadsheet.jpg`.
License: CC BY 4.0 (see LICENSE-ASSETS); code MIT.

- `build_clerk.py` → `clerk.blend`, `blender/export/character_clerk.glb`, `textures/clerk/excel_*.png`.
  `-- --preview` renders checks to `renders/clerk_*.png` (`--anim` adds animation frames).
- `unreal/scripts/import_clerk.py` (headless) → `/Game/Characters/Clerk`: skeletal mesh, 8 s loop
  `character_clerk_Anim`, `M_ClerkExcel` (the picture fills in over 130 s, then starts again).
- Rest pose = seated pose, character space: origin on the floor under the chair-seat centre,
  facing -Y. In Unreal: SkeletalMeshActor at layout `clerk.chair`, yaw = chair_yaw - 90,
  animation mode single node, `character_clerk_Anim`, looping. The Excel plane is part of the mesh.
