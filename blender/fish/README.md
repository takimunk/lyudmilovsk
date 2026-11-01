# Fish builder (Lyudmilovsk underwater)

One generic generator + one parameter file per species.

    Blender -b --python blender/fish/build_fish.py -- <Id> [--lint] [--preview]
    tools/fish_build.sh [Id ...]      # 3 Blender processes at a time, --lint --preview, then export/contact.png

Files: `build_fish.py` (CLI, Blender objects, glb export, previews), `fish_geo.py` (numpy geometry + UV atlas plan),
`fish_paint.py` (numpy texture painter), `lint_fish.py`, `species/<Id>.py` (`SPEC` dict).

## Conventions (the game relies on them)
- Metres, real total length per species (nose to caudal tip). Nose +X, back +Z, fish's left side +Y, origin on the body axis at half the length.
  Flatfish (`flat=True`) lie flat in the XY plane, eyed (right) side facing +Z, nose +X.
- glTF export uses the Blender default Y-up: Blender (x, y, z) -> glTF (x, z, -y): nose stays +X, back becomes +Y, the fish's left side becomes -Z.
  Unreal's glTF importer converts from there.
- Two material slots: `Body` (opaque, single-sided; also eyes and extras) and `Fins` (alphaMode MASK cutoff 0.3, double-sided).
- One texture per species `export/<Id>_D.png`, 1024x512 RGBA sRGB; A = fin membrane opacity (1 on body/eyes/rays, 0.35-0.8 on membranes, ragged edge to 0).
- Vertex colour `Col` (exported as glTF COLOR_0, 16-bit normalised): R = t (0 nose .. 1 tail tip), G = flexibility (0 rigid head, rises toward the tail, 1 on fin edges;
  fin vertices lerp from the body value at their base to 1 at the edge), B = 1 on paired fins (pectoral, pelvic), else 0, A = 1.
- UVs: body halves mirrored onto the same texels (flatfish: eyed and blind sides in separate regions), paired-fin / eye copies mirrored; fins and eyes each in their own rect, 4 px apart.
- LOD0 (`<Id>.glb`, object `<Id>`) and LOD1 (`<Id>_LOD1.glb`, object `<Id>_LOD1`, ~30 % of the triangles, same UVs, rebuilt from the same parameters).
- Outputs in `export/`: `<Id>.glb`, `<Id>_LOD1.glb`, `<Id>_D.png`, `<Id>_preview.png` (left side, top, 3/4 front-left, belly or blind side, 10 cm scale bar),
  `<Id>_close.png` (head 3/4, head side, tail, head from below), `<Id>_uv.png` (UV layout, cyan real faces, magenta mirrored copies), `<Id>.build.log`.
- Printed: `TRIS <Id> lod0 N lod1 M`, `SIZE <Id> L x H x W m`, `LINT <Id> ok|<problem> <count> ...` (see `lint_fish.py` for the checks).

## Adding a species
Copy the closest `species/*.py` (Herring fusiform, Cod bottom fish with barbel, Flounder flatfish, Pipefish slender) to `species/<Id>.py`, set `id` to the file name, run
`tools/fish_build.sh <Id>`, read `<Id>_preview.png`, `_close.png`, `_uv.png`, iterate until LINT is ok.

SPEC keys (all lengths are fractions of the total length L, t = position 0 nose .. 1 tail tip):
- `length` m, `seed`, `tris` (LOD0 target; LOD1 is 30 %), `body_end` (t where the peduncle ends; the caudal fin runs on to t=1), `nose_len` (rounded nose cap length),
  optional `nose_len_bot`, `tail_cap`, `flat`.
- `profile`: `top`, `bot` (height above/below the axis), `wid` (half-width) = lists of (t, value) keypoints (monotone cubic), `n` superellipse exponent (2 ellipse, 2.6+ boxy) or keypoints, optional `shift` (z offset of the section centre).
  Eel-like: long `body_end`, small heights; big-headed: large values at t<0.25; round lumpsucker: wide `wid`, n 2; garfish: long `nose_len` and a thin `top/bot` snout.
- `head`: `gill_t`, `gill_ridge` (operculum step), `mouth_len`, `mouth_depth` (gape groove, 0 = none), `mouth_p` (gape line around the ring, 0 dorsal .. 1 ventral).
- `eyes`: list of `dict(t, q, r)` (q = height fraction from the back 0..1, r radius); mirrored to the left unless `both=False` (flatfish: two eyes on the right side).
- `fins`: `kind="median"` (`side` dorsal/anal, `t0`, `t1`, `h`, `shape` rounded/pointed/spiny/trapezoid/fringe, `peak`, `ends`, `rake`, `rays`, `segs`),
  `kind="caudal"` (`span_top`, `span_bot`, `shape` forked/emarginate/truncate/rounded/pointed, `fork`, optional `base_h`),
  `kind="paired"` (`t`, `q0`, `q1` base on the flank, `len`, `shape` rounded/pointed/fan, `ang0/ang1` ray fan angles (deg, + = down), `splay` outward angle).
  Paint keys per fin: `col`, `ray`, `alpha`, `edge`, `base`, `bands`, `spots`, `scallop` (spiny fins), `rag`. Flatfish paired fins can override the blind-side fin with `blind=dict(...)`.
- `extras`: `dict(kind="barbel", t, p, len, r, col)`.
- `paint`: `ramp` (colour stops over q), `grain`, `layers` (mottle, spots, bars, stripe, sheen, scales, lateral, rings, flush), `head` (gill tint/line), `mouth`, `eye`, and for flatfish `blind`.
- Optional per-LOD overrides `lod0=dict(m=..., rings=..., nose_rings=...)`, `lod1=...` (m = vertices per half ring).

Minimal complete example (it builds; a first draft like this still gets lint findings such as sliver fin triangles until tuned):

    SPEC = dict(
        id="Sandeel", latin="Ammodytes tobianus", length=0.16, seed=5, tris=1500,
        body_end=0.90, nose_len=0.03,
        profile=dict(top=[(0, 0), (0.05, 0.02), (0.2, 0.026), (0.6, 0.024), (0.9, 0.012)],
                     bot=[(0, 0), (0.05, 0.018), (0.2, 0.024), (0.6, 0.022), (0.9, 0.012)],
                     wid=[(0, 0), (0.05, 0.014), (0.2, 0.020), (0.6, 0.018), (0.9, 0.008)], n=2.2),
        head=dict(gill_t=0.14, gill_ridge=0.02, mouth_len=0.04, mouth_depth=0.1, mouth_p=0.55),
        eyes=[dict(t=0.05, q=0.35, r=0.012)],
        fins=[dict(name="D", kind="median", side="dorsal", t0=0.2, t1=0.88, h=0.02, shape="fringe", rays=50, segs=14, col=(190, 180, 140), alpha=0.5),
              dict(name="C", kind="caudal", span_top=0.03, span_bot=0.03, shape="forked", fork=0.3, rays=15, col=(170, 165, 130), alpha=0.6),
              dict(name="Pec", kind="paired", t=0.15, q0=0.55, q1=0.65, len=0.04, shape="rounded", splay=20, rays=10, col=(200, 195, 170), alpha=0.45)],
        paint=dict(ramp=[(0, (80, 90, 60)), (0.4, (170, 175, 140)), (0.7, (225, 225, 205)), (1, (240, 240, 232))], grain=0.05,
                   layers=[dict(type="stripe", at=0.35, w=0.05, col=(230, 225, 190), a=0.5)],
                   eye=dict(iris=(190, 170, 90))),
    )

## Known limits
Fin geometry is a flat ruled membrane (no curvature); fin outlines come from the profile/shape functions, with ray lines and a ragged margin painted in the texture (alpha).
Body cross-sections are superellipses, so strongly angular bodies (boxfish) or bodies with a drooping belly need `shift`/`n` tuning.
