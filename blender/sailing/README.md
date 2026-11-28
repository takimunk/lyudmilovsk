# Sailing school meshes (part C of docs/yacht_school.md)

Everything here is built by script from numbers: no hand-modelled state, no `.blend`. One command rebuilds it all.

## Rebuild

```sh
python3 blender/sailing/build_all.py                 # pip bpy (Blender 5.3 as a module) or:
Blender -b -P blender/sailing/build_all.py           # any Blender 5.x
python3 blender/sailing/build_all.py --previews DIR  # + the review renders (Cycles CPU, ~15 min for all)
python3 blender/sailing/build_all.py --only boats --previews DIR --shots luch,lineup
```

Output (not committed: `blender/sailing/out/` is ignored; GLBs and pictures are rebuilt on every machine):
`out/sailing_boats.glb`, `out/sailing_gear.glb`, `out/sailing_house.glb` (one node per mesh, each at its own origin),
`out/textures/*.png` (procedural pictures, `textures.py`). `textures/localized/<lang>/sail_sign_*.png` come from
`tools/make_sailing_signs.py` (words from the strings tables; commit the languages that ship, LFS).

Committed: the scripts, `sockets.json` (crew/rig points), `manifest.json` (every mesh: kit, triangles, bounds, slots,
collision, LOD link, lint result), `boathouse_layout.json` (where every prop, boat, door and lamp goes),
`atlas_cells.json` (picture cells), `sign_strings.json` (a proposal of sign keys for part G; see Words).

**On the Mac, one command** builds and imports everything into the project:
`unreal/scripts/import_sailing.sh` (build_all.py, then `import_sailing.py materials`, then one process per kit).

## Files

| file | what |
|---|---|
| `boatspec.py` | the three dinghies as numbers (hull keys, rig, foils, crew), sail geometry, `sockets.json` writer (plain Python) |
| `sailkit.py` | material slots (`MAT_SPEC`), the bmesh `Mesh` builder, the lint, the registry/export |
| `boats.py` | hulls, spars, sails, foils, LOD1s, the far boat |
| `gear.py` | trolleys, covers, sail bag, spars rack, the coach's launch + outboard, marks, pin, buoys, MOB, anchor block, rope, block |
| `boathouse.py` | the boathouse shell + glass + sliding door leaf, interior and outdoor props, flags, windsock, the layout |
| `textures.py` | the pictures (sails, digits, atlas, flags): PIL, no words |
| `preview.py`, `shots.py` | review renders |
| `build_all.py` | the one command |

## Frames and axes

* **Boat frame** (every boat part, `sockets.json`): metres, origin on the static **waterline amidships** on the
  centreline, **+X forward**, **+Y port**, +Z up (the repo's convention for script-built boats: the north's rowboat has
  its bow at +X). Exported with `export_yup`: Unreal **X = x (forward), Y = -y (Unreal +Y = starboard), Z = z**, cm.
  Blender yaw +θ (right-handed about +Z) = Unreal yaw −θ.
* **Building frame** (`boathouse_layout.json`): origin on the finished floor in the middle of the 16.5 × 9 m footprint,
  **+X to the sea** (the seaward gable), **−Y the north long wall** (both doors, the boat park side). Props: front = −Y.
* Gear and props: origin on the ground / floor under the object (marks and buoys: at the waterline), front = −Y.

## Part list and pivots (boat frame, metres)

Each part's origin IS its pivot; at rest it sits at that pivot unrotated. `sockets.json` → `<class>.parts` lists them.

| part | luch | optimist | cadet | moves |
|---|---|---|---|---|
| `SM_Sail_<C>_Hull` | 0,0,0 | 0,0,0 | 0,0,0 | the boat |
| `_Mast` (foot in the step) | 1.13, 0, −0.05 | 0.70, 0, −0.07 | 0.62, 0, −0.10 | luch: may rotate about +Z (unstayed); cadet carries shrouds + forestay |
| `_Boom` (gooseneck **on the mast axis**) | 1.13, 0, 1.15 | 0.70, 0, 0.66 | 0.62, 0, 1.12 | yaw about +Z = sheeting angle; boom along −X; vang included |
| `_Main` (same pivot as the boom) | = boom | = boom | = boom | yaw with the boom; belly/flutter in the material |
| `_Sprit` (optimist, the snotter) | | 0.70, 0, 1.02 | | yaw with the main |
| `_Jib` (cadet, the tack) | | | 1.555, 0, 0.52 | rotate about the forestay axis (`axis` in sockets.json) |
| `_Rudder` (pintle axis at the waterline) | −2.155, 0, 0 | −1.18, 0, 0 | −1.64, 0, 0 | yaw about +Z |
| `_Tiller` (rudder head, child of the rudder) | −2.155, 0, 0.38 | −1.18, 0, 0.38 | −1.64, 0, 0.40 | yaw with the rudder; along +X |
| `_Ext` (universal joint at the tiller tip) | tiller + 1.02 | tiller + 0.78 | tiller + 0.95 | free (the helm's hand) |
| `_Board` | dagger: 0.30, 0, 0 slides +Z | dagger: 0.06, 0, 0 slides +Z | centreboard: pivot 0.30, 0, −0.10 pitches about +Y | |
| `_Far` | whole boat < 300 triangles (sails on the centreline) | | | far fleet |

`_LOD1` copies of every part go into LOD1 of their part on import. Sails (`_Main`, `_Jib`) are a double layer (port and
starboard faces 1 mm apart, closed at the edges: watertight), 12 × 22 grid at LOD0.

Gear: `SM_Sail_Trolley_<C>` (origin on the ground under the boat origin; the boat's origin sits at manifest
`boat_origin`), `SM_Sail_Cover_<C>` (same origin as the boat), `SM_Sail_SailBag`, `SM_Sail_SparsRack`,
`SM_Sail_Launch` (waterline amidships, +X forward) + `SM_Sail_Launch_Motor` (steering axis at the transom clamp, at
`pivot_on_launch` = −2.32, 0, 0.43: yaw = steer, pitch about +Y = tilt), `SM_Sail_Mark` (orange 1.2 m cylinder),
`SM_Sail_Pin`, `SM_Sail_Buoy`, `SM_Sail_MOB`, `SM_Sail_AnchorBlock`, `SM_Sail_Rope` (1 m along +X: stretch for sheets),
`SM_Sail_Block`.

Boathouse (60s–70s seaside modernism on part B's footprint and doors: concrete portal frame, mono-pitch roof rising to
the sea and oversailing it, clerestory band, glazed seaward wall over the tessellated mosaic, the starter's balcony at
the loft's level 2.8 with a south stair; layout `spots` → `deck_view`, `doors` → `balcony`, `footprint.roof` /
`footprint.balcony`): `SM_Sail_Boathouse` (shell, collision exact), `SM_Sail_Boathouse_Glass`, `SM_Sail_Boathouse_DoorLeaf`
(×2, slides along x: layout `sliding_doors`), `SM_Sail_Prop_*` (OptiRack, MastBracket, Workbench, ToolBoard, PaintTins,
Trestle, HullFrame, Blackboard, Desk, CoachDesk, Chair, KnotBoard, FlagChart, SeaChart, Cup, Pennant, TrophyShelf,
KettleTable (samovar), WallClock, Barometer, AnemometerDisplay, Radio, ShipBell, Photo0..3, Lockers, Bench, LifeJacket,
JacketRail, Paddle, Bailer, Shelf, Lamp, RopeCoil, BuoyNet, Stove, OarRack, Drawing0..3; outdoor: SlipBoard,
SignalMast (stays, pin rail), WindsockPole, Flagstaff, HoseStand, OutdoorBench, NoticeBoard, Bollard, Winch, LifeRing,
BikeRack, Park (slab + fence + gate, numbered bays)), `SM_Sail_LifeJacket_Worn` (origin at the base of the neck,
+X forward; adult size, scale 0.8 for children), `SM_Sail_Windsock` (origin on the pole axis at the swivel, along +X),
`SM_Sail_Flag_<id>` (origin at the hoist top, flying along +X; ids: sail_on, shore, races, storm, closed,
class_optimist, class_luch, class_cadet, prep, recall).

## Sockets (`sockets.json`)

Per class: helm / hike (and cadet crew / crew_hike / crew_low) for both sides (`_port`, `_stbd`): pelvis position,
facing yaw, feet point (under the strap when hiking); the hiking strap line(s); the mainsheet ratchet block and the
front hand on the sheet; `hand_tiller_ext` (in the `Ext` frame) and `hand_tiller` (in the `Tiller` frame); boom end and
boom block (Boom frame); the cadet crew's jib sheet hand; the painter eye; the trolley contact point. Coordinates in
Blender metres (`pos`) and Unreal cm (`ue_cm`). Regenerate with `python3 blender/sailing/boatspec.py`.

## Materials

Slots → instances (unreal/scripts/import_sailing.py, masters in make_sailing_materials.py):

* `SL_*` flat slots (`sailkit.MAT_SPEC`) → `MI_SL_<slot>` on `M_Sail_Flat`; UV0 in metres.
* `SL_Hull`, `SL_HullDeck` → `M_Sail_Hull`: COLOR_0.R = tint mask, COLOR_0.G = cavity. Tinted: the hull outside of all
  three (the luch's deck stays pale, its non-skid areas are `SL_NonSkid`, its boot stripe `SL_Stripe`; the cadet's
  decks are `SL_WoodDeck`). The tint comes from custom primitive data [3..5]. Hulls are smooth-shaded with sharp edges
  by angle (exported split normals).
* `SL_Sail` → `MI_SL_Sail_<Class>_<Main|Jib>` on `M_Sail_Cloth` (picture, number boxes from the manifest
  `number_boxes_uv`, digits T_Sail_Digits). **Cloth looks**: `…_Old` (yellowed Dacron) and `…_Patched` (repairs, a
  replaced panel) beside the default (new) instance, pictures `sail_<class>[_old|_patched].png`; the engine picks one
  per boat by setting the SL_Sail slot's material (manifest `cloth_variants`). The picture's **alpha is the light
  through the cloth** (panels thin, seams / tablings / patches / print thicker, the window clear) → subsurface colour
  (scalar `Translucency`).
* `CT_*` (the boathouse, the park, outdoor kit) → the town's `/Game/City/Materials/MI_CT_<slot>`.
* `SLP_Atlas` (pictures, `atlas_cells.json`), `SLP_Flags` (`M_Sail_Flag`), `SLP_Sign_Name`, `SLP_Sign_Door`
  (localized), `SLP_Notice` (the notice board's panel: UV 0..1 for the engine to write on).

**Custom primitive data** (one block per boat, same on all its parts): `[0]` camber −1..1 (side and depth of the
belly; + = toward the part's local +Y = port), `[1]` luff flutter 0..1, `[2]` sail number 0..999, `[3..5]` hull tint
(linear RGB; 0,0,0 = the slot's own colour). Sail mesh channels: UV1 = (s from the luff, h from the foot), UV2 =
(chord m, distance from the luff m), COLOR_0.B = 1 starboard face / 0 port / 0.5 edges and the luch's sleeve.

## Collision

Repo convention (the north's importer): Interchange glTF has no UCX, so collision comes from `manifest.json`:
`exact` (complex as simple: the boathouse shell), `box` (`collision_boxes`, mesh frame, rot_z_deg), `none` (boats, the
launch, sails, small props: the engine handles them).

## Check (RULE 3D)

`sailkit.lint` runs on every mesh: zero-area faces; picture-slot UVs inside 0..1 and no overlapping islands per material
and per side (rasterised at 256²); closed meshes (hulls of the luch, sails) without non-manifold edges and no inside-out
islands. Result in `manifest.json` (`lint`) and the build log; the last build: 0 problems. The renders in
`/mnt/project-files/yacht-school/` were looked at one by one.

## Words

The boathouse's name band and door plate read `sign.sail.school_name`, `sign.sail.door_line1`, `sign.sail.door_line2`
from `Content/Localization/sailing/strings_<lang>.json` (part G) or the main table; until G adds them,
`sign_strings.json` holds the proposed ru/en/ja. No other picture carries words (the clock's numerals only).
The Japanese sign needs a CJK font: built on the Mac (Hiragino); the cloud box has none.

## TODO

* No paid generations were made. Possible upgrades, as specs for the `game-assets` pipeline (sidecar per output):
  `sail_cloth_D/N` tiling sailcloth (weave + stitching, 1k, seamless), `alu_riveted_D/N` for the launch,
  `sand_concrete` grime for the plinth. Each would get `<asset>.json` (service, model, prompt, arguments, task id, URL,
  sha256, cost).
* The hull has no baked normal map (smooth shading only); the launch has no rivet lines.
* The engine places props from `boathouse_layout.json` (part E / interiors pipeline): not done here.
* The cadet's mast is fixed with its rigging; the luch's mast rotation is left to the engine (sleeve sail).

## Faces in one plane

`sailkit.settle()` runs on every mesh before the lint: where a face of one part lies on a face of another (two bars of a
frame crossing in one plane, a plate flush with a wall, a tube built twice), the smaller part's face steps 6 mm back into
its own body; a thin or open part steps forward and lies proud. The lint then reports any `coplanar overlap` left (faces
looking the same way within 1 mm of one plane, over 20 cm2 in all); what is left is inside one welded part and is mended
in the builder (the door leaf's rails run between its stiles, the roof's upstand between the parapets, the rope coil's
turns each off the last one's plane).
