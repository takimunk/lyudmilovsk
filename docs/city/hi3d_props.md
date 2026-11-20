# hi3d town props

Statues, a sundial, drinking fountains and chandeliers for the town, modelled by hi3d from our own reference sheets and
cut down to game meshes. Dima's ask (2026-11-20): interior things, chandeliers, statues, monuments, fountains, lamps
through hi3d, each model heavily simplified.

| Prop | Where | LOD0 tris / texture |
|---|---|---|
| `sundial` (bronze armillary on a limestone pedestal, 1.55 m) | Sun Square, between the fountain and the sea | 6k / 2048 |
| `girl_dove` (bronze girl releasing a dove, red granite plinth, 3.3 m) | the park pond's south shore | 9k / 2048 |
| `chandelier` (brass rings, frosted globes, 1.5 m) | workers' club cinema hall (3), department store's upper hall (4) | 8k / 2048 |
| `fisherman` (bronze fisherman hauling a net, 3.8 m) | promenade by the pier's root, looking out to sea | 9k / 2048 |
| `drink_fountain` (green cast iron, 1.05 m) | square (2), playground, promenade | 3k / 1024 |
| `astronomer_bust` (bronze on black granite, 2.6 m) | in front of the capsule library | 7k / 2048 |
| `gull_stele` (concrete seagulls on a stele, 6.5 m) | promenade by the sea, south of the pier | 6k / 2048 |
| `pioneer` (white plaster bugler, 2.3 m) | the school's forecourt | 7k / 2048 |

Spec: `blender/props_hi3d/props.json` (sheet column, views sent to hi3d, real height, origin, budget, metallic,
collision, run priority).

## Pipeline

1. **Reference sheets**: Higgsfield `gpt_image_2_6` (4K, high, opaque), 4 props x 3 views (front, left, back) per sheet;
   requests and job ids in `blender/props_hi3d/sheets.json`. On the Mac (the cloud can't download Higgsfield URLs):
   `python3 tools/props/hi3d_props.py fetch-sheets` and `cut` -> `blender/props_hi3d/raw/<id>/views/` (raw/ is gitignored).
   Mirror-symmetric props also get `right` = the left profile mirrored; asymmetric ones (statues) go with 3 views.
2. **hi3d** (v3.0 high_quality, texture + PBR, 45 points each) through `tools/birds/hi3d_mcp.py` (upload, quote,
   create, task, download); `tools/props/hi3d_props.py args/submitted/fetched` write the sidecar
   `blender/props_hi3d/<id>/hi3d.json` (task id, inputs with sha256, output URL and sha256). The bird models share the
   hi3d balance and come first: keep their reserve.
3. **Optimize** (`Blender -b -P blender/props_hi3d/optimize.py -- <id>`): weld, drop specks, real height, front down +X,
   origin at the base (chandelier: top), bake source cut to 600k, low = collapse to the budget (hi3d meshes have
   thousands of edges with 3+ faces and collapse stalls on them: then the low copy is voxel-remeshed first), smoothed by
   angle (40 deg: plinth and column edges stay hard), fresh UV layout (hi3d's islands smear under decimation), bake base
   colour (metal/transmission flattened for the bake), tangent normal, roughness, and hi3d's metallic as a mask in the
   R map's green channel (stone plinths and columns stay dielectric; props.json `metallic` scales the mask). A built
   `plinth` (props.json, when hi3d left it out) is one closed stepped solid kept out of the decimation -> `blender/props_hi3d/game/<id>.glb`, `T_Hi3d_<id>_{C,N,R}.png`, `<id>.json` (size, radius).
   ALWAYS look at `raw/<id>/preview.png` (front, left, back, three-quarter, UV layout over the colour) for seams,
   smears and holes. Wrong side in front: `--front-yaw 90|180|-90`, then put `front_yaw` in props.json.
4. **Import** (`unreal/scripts/import_hi3d_props.sh [ids]`): `/Game/City/Hi3D/SM_Hi3d_<id>`, `MI_Hi3d_<id>` of
   `M_Hi3dProp` (C/N, R = roughness + metal mask in G, Metallic = mask x scalar), Nanite off, LOD1 50 % / LOD2 25 %, 26-DOP collision for the solid ones.
5. **Place** (`python3 tools/city/hi3d_props.py`, `--check`): anchors in the public spaces layout and the city plan,
   searched for a clear paved spot (props, trees, plants, fountain basin, pond, each other, 0.8 m walking gap) ->
   `Content/City/hi3d_props.json`. Chandeliers hang round named lamps of the building exports (the tool fails if a
   building changes and the lamp is gone). Rerun after `tools/public_spaces.py` or the city plan changes.

## In the game

`ALyudPublicSpaces::Build` reads `City/hi3d_props.json` after its kit props: one instanced set per prop with the mesh's
own material; outdoor ones cull at 250 m, chandeliers at 90 m and stay out of distance fields; nothing per frame. A
prop not yet imported is skipped with a log line. `lyud.City.Show 0` hides them with the rest.
