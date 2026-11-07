# Sea-life builder (non-fish underwater animals, Lyudmilovsk)

Crustaceans and the grey seal, built so the **fish import path and material work unchanged**: they are written into
`blender/fish/export/` with the fish conventions, `unreal/scripts/import_fish.py` imports them like any species and
`M_LyudFish` bends them by their vertex colours.

    Blender -b --python blender/sealife/build_sealife.py -- <Id> [--lint] [--preview]
    Blender -b --python blender/sealife/build_sealife.py -- --contact [Id ...]    # export/contact_sealife.png, 2 columns
    python3 blender/sealife/build_sealife.py <Id> --dry [--tex]                   # no Blender: geometry, atlas, lint (+ _D.png, _uv.png)

Files: `build_sealife.py` (CLI; Blender object / glb export / material patch / uv image come from `blender/fish/build_fish.py`,
loaded as a library without running its `main()`), `sealife_geo.py` (numpy kit: tubes, domes, plates, eye caps, atlas),
`sealife_paint.py` (numpy painter; noise, ramps and png writer from `fish_paint.py`), `lint_sealife.py` (reuses `lint_fish.py`'s
rasteriser / sliver threshold), `species/<Id>.py` (`SPEC` dict + `build(K)`).

| Id | animal | real size | LOD0 tris |
|---|---|---|---|
| BrownShrimp | *Crangon crangon* | 6 cm body | 1936 |
| Saduria | *Saduria entomon* | 7 cm body | 2306 |
| MudCrab | *Rhithropanopeus harrisii* | 2.5 cm carapace width | 2074 |
| Gammarus | *Gammarus* sp. | 1.8 cm along the curved body | 1638 |
| Mysid | *Neomysis integer* | 1.5 cm | 134 (<= 150, drawn in clouds) |
| GreySeal | *Halichoerus grypus*, adult male | 2.0 m nose to hind-flipper tips | 5484 |

## Conventions (same as the fish, see `blender/fish/README.md`)
- Metres, real size. Head +X, back +Z, the animal's left side +Y, origin on the body axis at half the body length.
  Arthropods sit in a resting pose: legs splayed down onto the ground plane a few mm below the body.
- Objects `<Id>` (LOD0) and `<Id>_LOD1` (~25-35 % of the triangles, rebuilt by calling `build(K)` with `K.lod = 1`, same rect keys -> same texture).
- Slots `Body` (opaque, single-sided: bodies, legs, antennae, claws, eyes) and `Fins` (alpha MASK 0.3, two-sided: thin flat cards only —
  shrimp antennal scales and uropods, isopod uropods, mysid legs and tail fan). Thin round appendages are closed tubes in `Body`.
- One 1024x512 RGBA texture `<Id>_D.png`; A = card opacity on `Fins` (1 elsewhere).
- `Col` (COLOR_0): R = clip(0.5 - x / r_len, 0, 1) (`r_len` defaults to `length`; antenna tips past the nose clamp to 0),
  G = flexibility from `SPEC["flex"]` keypoints over R plus a per-part `gadd` (crustaceans 0..0.2, more on the tail fan; seal 0 on the
  head rising to 1 at the hind-flipper tips; crab ~0), B = 1 on legs / pleopods / claws / seal fore-flippers (the shader may paddle them).
- UVs: one rect per part key at a common texel density (`dens` boost, `minpx` floor for hair-thin legs), 4 px apart. Bilateral bodies are
  half-unwrapped (+Y half real, -Y half mirrored onto the same texels, v proportional to the arc round the section so the density is even);
  left legs / eyes mirror the right ones; repeated pieces (seal claws, antennules) are instances on the same texels. Every rect is painted
  from its own part only and filled outwards with its own nearest colour (no bleeding between parts at any mip).
- Outputs in `blender/fish/export/`: `<Id>.glb`, `<Id>_LOD1.glb`, `<Id>_D.png`, `<Id>_preview.png` (left side, top, 3/4 front-left, below,
  scale bar of ten tenths of 1 cm / 10 cm / 1 m), `<Id>_close.png` (head 3/4, head from above, tail 3/4, head from below),
  `<Id>_uv.png` (texture with cyan real faces, magenta mirrors/instances), `<Id>.build.log`.
- Printed: `TRIS`, `SIZE` (bbox L x H x W plus the real-size measure of `SPEC["size"]`), `LINT <Id> ok|...`, `ATLAS` (px per metre).

## Lint (`lint_sealife.py`, both LODs)
degenerate / sliver triangles, uv area zero, uvs outside 0..1 or their rect, rects closer than 4 px, overlapping real uvs, mirror faces not
covered by their original, body texel density off the part median, z-fighting between parts, open / non-manifold edges on closed parts
(welded by position; crab carapace top+bottom welded together), inverted or folded faces, Col range / R / G jumps / B, plus in the build:
LOD0 over `tris` (+12 %, or `tris_max`), LOD1 ratio outside 18-45 %, real size off by more than 5 %.

## Kit (`sealife_geo.Kit`, all lengths in metres)
- `tube(name, ctrl, w, top, bot, n, m, nr, ends, half, ...)` lofted superellipse sections along a smooth path through `ctrl`; profiles are
  `[(s, v)]` keypoints over the arc-length s; `half=True` for bodies (path in the XZ plane), `half=False` for appendages (`m` sides,
  `up` / `roll` orient flattened sections); `ends` `apex` | `edge` (w = 0: knife edge, e.g. flipper tips) | `open`;
  `mod=saw(SEG, ...)` gives overlapping segments with grooves, `extra_s=seg_rings(SEG)` resolves the boundaries; `deform(ring, S, q)`.
- `dome(name, outline, cx, ztop, zbot, ...)` closed lens over a star-shaped plan outline (crab carapace), top / bottom planar uvs.
- `plate(name, ctrl, shape, ...)` flat card for the `Fins` slot, outline cut by the alpha from `shape`.
- `eye(name, C, r, axis, ...)` sphere cap with an azimuthal uv disc (pupil / glint painted by radius).
- `pair(part)` -> [part, mirrored copy]; `instanced(part, name, M, t)` shares texels.
- Paint dicts: `col` | `ramp` over q (0 back .. 1 belly), `grain`, `layers` of mottle / speckle (jittered chromatophores, `star`) /
  bands (segment lines, `rear` margins) / tip / stripe / streak / spot / eye, each with optional `s` / `q` windows; cards add `alpha`, `edge`, `rag`.

## Known limits
Crab carapace teeth and the seal's whiskers are not modelled (painted pores only). Glassy translucency (mysid, shrimp) can only be suggested
by pale colours: `Body` is opaque. Leg joints are smooth bends of one tube, not separate articles.
