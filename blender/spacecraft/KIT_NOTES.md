# The «Ласточка» kit: notes for the engine and the material work

Built by `blender/spacecraft/spacecraft_kit.py` (design: `docs/spacecraft.md`). Nothing here is generated: primitives,
exact boolean cuts, own UVs.

```
blender -b -P blender/spacecraft/spacecraft_kit.py -- [--preview design/spacecraft/preview] [--strict]
python3 blender/spacecraft/spacecraft_kit.py [--preview design/spacecraft/preview] [--strict]     # with `pip install bpy` (5.3)
  -> design/spacecraft/game/spacecraft_kit.glb          29 mesh objects, each at its own origin
  -> unreal/Lyudmilovsk/Content/Spacecraft/kit.json     where they go, every measure
  -> design/spacecraft/preview/kit_*.png                ten Cycles (CPU) renders with stand-in pictures on the SC_* slots
```

`--strict` fails on any `kit_lint` finding, a face seen from behind, a prop through the wall, the capsule not fitting, or
more than 150 000 triangles. Environment switches for looking closer: `SCKIT_SHOTS=04,05` (only those shots),
`SCKIT_SAMPLES=24`, `SCKIT_BACKFACE=1` (back faces glow magenta), `SCKIT_EXTRA=1` (six close check shots),
`SCKIT_LINT_LIMIT=400`, `SCKIT_DEBUG=1` (the standing capsule's clearance along the aisle).

## Frames

Blender metres, Z up. UE gets `(x, -y, z) * 100`; kit.json is already UE centimetres.

- **Cabin frame**: origin = the middle of the cabin's floor, +X forward (the panel, the big porthole), the hatch at -X.
  The hero's couch is on Blender +Y = UE -Y (his left); the record player is on that side, the stowage net on the other.
- **Stage frame**: origin on the ice under the stage's axis, the foot pads' soles at z = 0, the hatch side at -X. The
  cabin frame stands at `stage.cabin_seat` (0, 0, 275 cm), unrotated.
- **Own frames**: every piece that moves or is placed more than once is modelled round its hinge or axis. kit.json gives
  a *frame* for it: `{at, x, y, z, rot}` = where the piece's origin goes, its own axes as UE unit vectors, and the same as
  `[pitch, yaw, roll]`. For anything hinged both ends of the travel are given as frames (`closed`/`open`, `off`/`on`,
  `rest`/`pulled`): blend them, no sign to guess.

## The pieces

Triangles: 57 590 in all (the list is in kit.json `pieces`, with bounds and slot names).

| Piece | Frame, origin | What it is | Slots |
|---|---|---|---|
| `SM_SC_Stage` (8 868) | stage | octagonal truss, four legs with pads, engine bell, four tanks, foil bays, deck and cradle ring, dish on a mast, the platform at the hatch, the ladder (9 yellow rungs) | `tx_sc_metal`, `tx_sc_foil`, `tx_sc_paint_white`, `SC_dark`, `SC_yellow` |
| `SM_SC_CabinOuter` (13 304) | cabin | the hull's outside: shield cap and rim, cone, dome; porthole and hatch collars with their liners, hinge lugs, six thrusters, umbilical plate, parachute lid, riser fitting, antennas | `tx_sc_shield`, `tx_sc_paint_white`, `tx_sc_metal`, `tx_sc_rubber`, `SC_dark` |
| `SM_SC_CabinInner` (11 569) | cabin | floor, kick band, quilted wall (pillows + buttons), dome, the openings' reveals, ribs, hoop, cables, handrails, 3 lamps, 2 fans, the player's shelf, sleeve rack, stowage net, parachute handle's base and plates, foot rail | `tx_sc_floor`, `tx_sc_paint_green`, `tx_sc_quilt`, `tx_sc_metal`, `tx_sc_rubber`, `tx_sc_net`, `SC_lamp`, `SC_dark`, `SC_labels` |
| `SM_SC_Panel` (11 116) | cabin | the console: body, hood, wells, bezels, flanges, screws, dial faces, glasses, digit cells, lamp cells, CRT, bar, speaker, switch bases and guards, the ПУСК base, all plates. Two UV sets | see "The panel" |
| `SM_SC_Player` (1 402) | cabin | the suitcase player strapped to the shelf: base with cheeks, deck, knobs, handle, open lid with speaker, stay, straps | `SC_case`, `tx_sc_metal`, `tx_sc_fabric`, `tx_sc_rubber`, `SC_dark`, `SC_paper` |
| `SM_SC_FireShell` (1 536) | cabin | a cone 10 cm outside the hull's, both faces. U 0..1 round, V 0..1 up | `SC_fire` |
| `SM_SC_Hatch` (2 200) | own: on the hinge axis, X out of the hull, Z the hinge, Y to the door's middle | the door: domed outside, quilted pad inside, seal, dogs, hinge arms, handle, ОТКР/ЗАКР plates. `hatch.closed`/`open` (110 degrees, outward) | `tx_sc_paint_white`, `tx_sc_paint_green`, `tx_sc_quilt`, `tx_sc_metal`, `tx_sc_rubber`, `SC_dark`, `SC_labels` |
| `SM_SC_HatchWheel` (592) | own: hub, X = its axis (into the cabin) | `hatch.wheel.in_hatch` (child of the door) or `.closed` (cabin) | `SC_red`, `tx_sc_metal` |
| `SM_SC_Glass_Big`, `_Side` (76 each) | own: the pane's middle, X out of the hull | 16 mm panes, UV 0..1 across; `portholes.*.glass` (the side one twice) | `SC_porthole` |
| `SM_SC_Couch` (1 032) | own: on the floor under the pelvis, X forward | tube frame, cushions, headrest, belts; `seat.couch_frames` (2) | `tx_sc_metal`, `tx_sc_fabric` |
| `SM_SC_Blanket` (132), `SM_SC_Cap` (278) | the couch's | on the second couch's seat: `props.blanket_and_cap` (= couch 2's frame) | `SC_blanket`; `SC_cap`, `SC_dark`, `tx_sc_rubber`, `SC_yellow` |
| `SM_SC_Thermos` (322) | own: the middle of its foot, Z up | `props.thermos` (in the net) | `tx_sc_metal`, `tx_sc_paint_green` |
| `SM_SC_Book` (28) | own: lying on its back cover, front up, spine at -X | `props.book` (standing in the net) | `SC_book` (U 0..0.47 back, ..0.53 spine, ..1 front), `SC_paper`, `SC_dark` |
| `SM_SC_Needle` (51) | own: on its axis, X toward the hero, pointing +Z, 1 long | the one needle of every dial: `panel.dials[].needles[]` (frame + scale) | `SC_needle`, `SC_dark` |
| `SM_SC_Globe` (960), `SM_SC_Ball` (728) | own: the centre | equirectangular spheres (7 and 5.5 cm radius): `panel.globe.frame`, `panel.ball.frame` | `SC_globe`; `SC_ball` |
| `SM_SC_Toggle` (70) | own: the pivot, the lever along +X | 8 placements: `panel.toggles[].off/on` | `tx_sc_metal` |
| `SM_SC_LaunchButton` (120), `SM_SC_LaunchCover` (56) | own: the button's foot, X out of the panel; the cover's hinge | `panel.launch` | `SC_red`, `tx_sc_metal` |
| `SM_SC_ChuteHandle` (272), `SM_SC_ChuteCover` (58) | own: where it leaves its base, X = the pull; the cover's hinge | overhead, over the hero's couch: `chute.handle`, `chute.cover` | `SC_red`, `tx_sc_metal`, `SC_yellow` |
| `SM_SC_Platter` (388) | own: its axis at the deck, Z up | spins about Z: `player.platter` | `tx_sc_metal`, `tx_sc_rubber` |
| `SM_SC_Tonearm` (176) | own: its pivot, the arm along +X to the stylus | `player.tonearm.rest/lead_in/run_out` | `tx_sc_metal`, `SC_dark`, `SC_case` |
| `SM_SC_Record` (348) | own: the middle of its underside | 25 cm disc, both sides; the label (4.5 cm radius) is its own disc, UV 0..1: `player.record` | `SC_record`, `SC_record_label` |
| `SM_SC_Sleeve` (12) | own: the middle of its foot, X = its front | 26 x 26 cm, the front quad UV 0..1: `player.sleeve_rack` (8; index 0 is the front one) | `SC_sleeve`, `SC_paper` |
| `SM_SC_Chute` (1 486), `SM_SC_Drogue` (334) | own: the cabin's riser fitting (`chute.attach`), hanging +Z | canopy (24 / 8 gores), lines, riser. Main: skirt 14 m up, mouth 6.5 m across (a 9 m canopy blown). U 0..1 round (a gore = 1/24 or 1/8), V 0 skirt .. 1 vent | `SC_chute` (two-sided), `SC_cord` |

## Material slots

Tiling, metre UVs (tile sizes in kit.json `tiles`, cm):

| Slot | For | Tile |
|---|---|---|
| `tx_sc_paint_white` | the hull's cone and dome, the door's outside, tanks, the dish | 2 m |
| `tx_sc_paint_green` | the classic grey-green: dome, ribs, hoop, kick band, shelf, the console's body | 2 m |
| `tx_sc_metal` | bare metal: collars, liners, truss, legs, tube frames, rails, screws | 1 m |
| `tx_sc_foil` | the stage's gold foil (bays, deck, belly, pads' tops, instrument box) | 1 m |
| `tx_sc_quilt` | the wall padding and the door's pad. One pillow is 28.96 cm square in UV (`quilt.pillow`), seams at its multiples, 24 round with no seam at the back; the tile is 2 x 2 pillows. The mesh has the pillows as shallow pyramids (2.5 cm), sharp seams and a button at every crossing: a normal map of the same grid lands on them | 57.9 cm |
| `tx_sc_shield` | the heat shield's cap and rim (scorch goes here) | 1 m |
| `tx_sc_rubber` | black rubber: cables, seals, the hood, the platter's mat, the cap's peak | 0.5 m |
| `tx_sc_floor` | the cabin's floor mat (added: black rubber made the cabin a pit) | 0.5 m |
| `tx_sc_fabric` | couch cushions, belts, straps, the lid's lining | 0.5 m |
| `tx_sc_net` | the stowage net: ONE quad, a cutout (masked) net texture, two-sided | 0.25 m |

Flat or drawn by the game:

| Slot | For |
|---|---|
| `SC_dark` | matt black: bezels, wells, plates' rims, the bell's inside, fan blades |
| `SC_red`, `SC_yellow` | paint: wheel, covers, button, handle; rungs, the ПУСК base, the ball's aircraft, cover stripes |
| `SC_glass` | instrument glasses (dials, globe, ball): a faint reflective glass, or nothing |
| `SC_porthole` | the three panes: glass, scorch and spray |
| `SC_lamp` | the three cabin lamps' lenses (emissive; `lamps[]` has the lens and a light position) |
| `SC_dials` | the eight dial faces: one 1024 atlas, see below |
| `SC_digits` | 17 seven-segment cells |
| `SC_lamps` | 24 annunciator cells |
| `SC_bar` | the shield temperature bar, UV 0..1 (bottom = Blender V 0) |
| `SC_crt` | the screen, UV 0..1 |
| `SC_labels` | every printed plate: one 1024 atlas per language |
| `SC_globe`, `SC_ball` | equirectangular |
| `SC_needle` | needle paint |
| `SC_case` | the player's leatherette (and the arm's head shell) |
| `SC_record`, `SC_record_label`, `SC_sleeve`, `SC_paper` | vinyl; the label; a sleeve's front; card and page edges |
| `SC_book`, `SC_blanket`, `SC_cap` | the book's cover picture; the blanket; the cap's cloth |
| `SC_fire` | the plasma shell |
| `SC_chute`, `SC_cord` | the canopies (stripes from U: orange where `frac(U * gores / 2) < 0.5`); lines |

Not as the brief suggested: there is no `tx_sc_chute` (the canopy needs gore UVs, not metres: `SC_chute`), and
`tx_sc_floor`, `tx_sc_net` were added.

## The panel

kit.json `panel`: its frame (`origin`, `u` right, `v` up the face, `n` out of it; 140 x 52 cm, leaning back 18
degrees). `SM_SC_Panel` has a second UV set (`UVData`; glTF TEXCOORD_1, UE UV channel 1) used only by the digits and lamps.

- **Dials** (`dials[]`: alt, gmeter, press, o2, co2, temp, volts, fuel): `SC_dials`, 1024 atlas, `atlas_rect` [x, y, w, h]
  from the top left: alt and gmeter 512 px, the six small ones 256 px. The face's circle is inscribed in its rect and is
  the dial's `radius` + 3 mm; the bezel shows it out to `window_radius`. Scale: `min..max` over `a_min..a_max`, degrees
  clockwise from 12 o'clock, ticks every `major` / `minor`. A needle at the angle a points along
  `cos(a) * panel.v + sin(a) * panel.u`. `SM_SC_Needle` goes at `needles[].frame`, uniformly scaled by `needles[].scale`,
  turned about its own X. `alt` has two (`long` once per 10 units, `short` once per 100; the face reads 0..9).
- **Globe, ball**: spheres in deep wells behind a glass; the crosshair and the little aircraft are in the panel mesh.
- **Digits** (`digits`): windows clock (6 cells, a gap before cells 2 and 4), range (6), speed (5); cell 1.7 x 2.8 cm. One
  quad per cell, `UVMap` 0..1 over the cell, `UVData.x = (index + 0.5) / 17` (`first_index` per window, left to right): a
  17 x 1 data texture or 17 scalars pick each cell's digit.
- **Lamps** (`lamps`): 6 x 4, one quad per lamp, index = row * 6 + col, row 0 on top, in `labels.json`'s lamp order.
  `UVMap` = the lamp's caption rect on the label atlas (the lamp material samples the same `T_SC_Labels_<lang>`),
  `UVData` = the lamp's cell middle in a 6 x 4 data texture (`cells[].data_uv`, UE convention). One material instance
  and a 24-texel texture (or 24 scalars) light them.
- **Labels** (`labels.plates[]`): the 65 plate ids of `tools/spacecraft/labels.json`, in its order, with `rect` (px from
  the top left, packed with its 6 px gutter, each class at its own size), `size_cm` as printed (41 px/cm; lamp captions
  32.8) and where each is used (`placed[]`). Not placed anywhere (free for the dial painter): `unit_ms`, `unit_g`,
  `unit_volt`, `unit_pct`, `unit_mmhg`. `mark_on` / `mark_off` are used eight times, `mark_open` / `mark_shut` on the
  door, `mark_chute` / `mark_manual` by the overhead handle.
- **Switches** (`toggles[]`, left to right: power, air, gyro, seal, arm (red guards), then light, crt, radio): `off`, `on`
  frames and an `interact` point. `launch`: the button's frame and `pressed`, the cover `cover_closed` / `cover_open`.
- `crt`, `bar` (0..2000), `speaker`: centres and sizes.

## The rest of kit.json

- `cabin`: the hull's measures (inner radius 130 at the floor, 102 at the knee 155 up, ceiling 205; outside 310 across,
  300 tall), `jets` (four points under the rim for the soft-landing fire), `thrusters`, `chute_lid`.
- `seat`: both couches' frames, the hero's `pelvis` and `eyes` when seated on couch 1, `look_direction` / `look_at` (the
  view that has the panel below and the big porthole above), the same in the couch's frame.
- `stand`: where he stands up inside (`centre`), just inside and just outside the hatch, the capsule.
- `hatch`: centre, axis, clear radius 60, the door's `closed` / `open` frames, the wheel, `sill_z`.
- `portholes.big/left/right`: centre, outward normal, clear radius, the pane's frame, and `look` (a camera position and
  direction for leaning to it, an interact point).
- `fire_shell`, `chute` (attach point, canopy measures, the handle and its cover), `lamps`, `fans`.
- `player`: its frame, platter and record frames, the tonearm (pivot, 18.5 cm, `rest` / `lead_in` / `run_out` as angles
  at the pivot from the line to the spindle and as frames), `sleeve_rack` (8).
- `props`, `interact` (every switch, the button, light, player, sleeves, thermos, book, chute handle, hatch wheel, both
  couches, the three portholes).
- `afloat`: at 2 400 kg the sea stands at z = -10.3 (draft 49.7); the hatch sill is 46 cm over it.
- `stage`: `cabin_seat`, the four `pads`, the `platform`, the `ladder` in `ULyudLadderComponent`'s terms (`at`, `yaw`,
  30 cm rungs from 25 cm, 9 rungs, `top_exit` onto the platform), `hatch_stand`.
- `checks`: what the script measured (below).

## Collision

Nothing is exported for collision; simple shapes do it: the floor disc, a ring of boxes or a cone for the wall, a box for
the console (`pieces.SM_SC_Panel.bounds`), boxes for the couches and the shelf; on the stage a box for the platform, one
per leg, a cylinder for the truss. The hatch is 1.20 m round with its sill 31 cm over the floor: the character's
capsule (0.34 x 1.92) does not walk through it upright. The script checks instead (`checks.capsule_clearance_m`): a ball
of the capsule's radius passes the hatch's axis with 26 cm to spare, a crouched capsule reaches the standing point, the
full capsule stands at `stand.centre` with 7.5 cm round it, and the seated body's balls clear everything but the couch.
Boarding is the game's scripted move (as on Europa's hatch).

## Checked

`kit_lint` (the cabin assembled, the stage with the cabin on it, the canopies, the fire shell): 0 findings. Rays from
ten eye points inside and 25 outside: no face seen from behind. No prop through the wall. Looked at: the ten previews,
the same with back faces lit magenta, six close shots (overhead handle and its plates, the door's plates, the net, the
second couch, the left wall, the floor).

## Rough edges

- The outward door needs 1.4 m in front of the hatch, so the platform has no rail on its outer edge, only a kerb.
- The seated eye is 0.63 m left of the big porthole's axis: its reveal opens inward (16 degrees) so the view is not a
  keyhole, and `portholes.big.look` is there for leaning to it.
- Fan blades are still (the game can spin a blur disc at `fans[]`); the lamps' guards are simple.
- The quilt is shallow pyramids, not cushions: the normal map does the rest.
- The glb is not in .gitignore (europa's is in LFS): left on disk.
