# Bird models from hi3d scans

Since 2026-11-21 the birds' bodies are hi3d scans of photoreal reference pictures; the rig, wing and tail feathers,
legs, clips, LODs and the Unreal side are still `blender/birds/build_bird.py`'s. The species kept on the old procedural
body are listed below.

## What a bird is made of now

| Part | Source |
|---|---|
| Body, head, bill, painted eye | the hi3d scan mesh (wings, legs and the tail plate cut off, holes refilled), decimated to LOD0 20k / 8k / 3k / 1.3k tris; its own uvs, its texture in a 2048 cell of the 4096 atlas |
| Skeleton | the template rig carried onto the scan: `warp()` maps the template body axis onto the scan's centreline and scales each cross-section so the template skin lands on the scan skin (points outside the skin are moved, not scaled). Bone names, hierarchy, clips and the folded pose = Idle frame 0 / bind = spread are unchanged (the flight thread's SetWingPose / SetWingLevel contract) |
| Bill and tail length, wing outline | measured on the scan: bill and tail lengths, and each primary/secondary runs to the photographed wing's trailing edge. The wingspan still comes from species.json (solve_size) |
| Wing and tail feathers | procedural cards on the wing bones (they fold as before). Wing cards are painted from the scan's own wing photo (`wing_paint`, below), colour-matched to the back |
| Legs | procedural (the scan's legs are cut off) |
| Hero body | the same, LOD0, atlas capped at 4096; the scan head and the face round the bill base go to the Skull slot (hidden in first person) |

## Pipeline and commands

All from the repo root; Blender only one process at a time when the Mac is short of memory.

1. **Reference sheet** (Higgsfield `generate_image`, model `gpt_image_2_6`, quality high, 4k, 4:3, opaque): 4 species
   in columns, rows front / left side / back, standing, body ~20 degrees nose-up, both wings raised in a steep V clear of
   the body (the prompt is in `blender/birds/scans/sheets.json` with every sheet's job id, url, sha256). Long-necked
   birds: neck stretched straight and level (sheet 11).
2. **Cut**: `python3 tools/birds/cut_sheet.py blender/birds/scan_raw/sheet_N.png Id1 Id2 Id3 Id4`
   -> `blender/birds/scan_raw/<Id>/views/{front,back,left,right}.png` (right = mirrored left).
3. **3D**: `python3 tools/birds/hi3d_mcp.py bird-submit Id ...` (v3.0 high_quality, texture + PBR, 45 pts each), then
   `python3 tools/birds/hi3d_mcp.py bird-poll` until every model is downloaded to `scan_raw/<Id>/model.glb`. The client
   talks to the hi3d MCP directly with Claude Code's OAuth token from the keychain (allowed by Dima for exactly
   `Bash(python3 tools/birds/hi3d_mcp.py:*)`: no redirects, no background). Sidecars: `blender/birds/scans/<Id>/hi3d.json`.
   hi3d runs at most ~20 tasks at once (error 10030106: submit the rest later).
4. **Build**: `tools/birds/scan_build.sh [--no-prep] Id ...` (env `BIRD_FAST=1` skips the raw/blend/lint renders,
   `BIRD_LOCK=<script>` wraps each Blender run). It runs
   - `tools/birds/scan_prep.py` (Blender): orient (mirror symmetry), level the body, centreline + landmarks (body end,
     bill base), radius/colour maps -> `scans/<Id>/scan.npz` (committed); body cut + fill + LODs ->
     `scan_raw/<Id>/body.npz`, `body_tex.png`; wing photo -> `wing.npz`; whole scan decimated -> `full.npz`;
   - `build_bird.py <Id> --scan` with `BIRD_SCAN_MODE=S` -> `blender/birds/export/<Id>/` (+ previews);
   - `tools/birds/compare.py <Id>` -> `renders/birdmodels/<Id>_old_raw_new.jpg`.
   Hero: `BIRD_SCAN_MODE=S Blender -b --python blender/birds/build_bird.py -- <Id> --hero --scan --previews`.
   Whole set: `tools/birds/night_run.sh blender` and `tools/birds/night_run.sh import` (T0_FILE = a file with the start
   time; each loop takes a species once its build is newer than that).
5. **Unreal**: `tools/birds/import_queue.sh Id ... hero:Id ...` (each waits for a slot in `tools/wait_ue_queue.sh`, then
   `unreal/scripts/import_birds.sh`: textures, import onto the species' existing skeleton, hues; ~8 min per species).
6. **Sheets**: `python3 tools/birds/contact_sheet.py [old_checkout] [Id ...]` -> `renders/birdmodels/contact_N.jpg`
   (OLD previews from a checkout that still has the procedural exports, default `~/projects/lyudmilovsk-wt/birdskel`).

Raw pictures and the 2M-triangle models stay local (`blender/birds/scan_raw/`, git-ignored); urls and hashes are in the
sidecars.

## `blender/birds/scans/overrides.json`

Per species, all optional:

| Field | Used by | Meaning |
|---|---|---|
| `lateral` (`x`/`y`), `flip` | scan_prep | the model's mirror axis / forward sign when the automatic guess is wrong |
| `beak_base`, `body_end` | scan_prep | landmarks as fractions of the side section's length |
| `pitch_deg` | scan_prep | extra body levelling |
| `centreline: medial` | scan_prep | ridge of the side section's distance map instead of chord middles (S-necks) |
| `tail_cut`, `cut_margin`, `root_margin`, `tris0` | scan_prep | where the tail plate is cut (u), how far outside the cleaned body radius the cut lies (and at the wing roots), LOD0 budget |
| `wing_paint` | build_bird | `art` (default: flight feathers painted from species_art.py, coverts from the photo), `photo` (all wing cards from the photo), `art_all` (no photo: two-tone wings where matching to the back goes wrong) |
| `keep_old` | people | the reason a species stays on the old body (the build may run, but don't import it) |

## Adding a species or re-scanning one

Add it to `Content/BirdsData/species.json` and `species_art.py` as before (the procedural description still drives
the wings, legs and the plumage of art cards), make a sheet row for it, cut, submit to hi3d, poll, then
`tools/birds/scan_build.sh <Id>` and look at `renders/birdmodels/<Id>_old_raw_new.jpg` and its back view before
importing. A re-scan: replace the views, submit again, rebuild (old models move nowhere: `scan_raw/<Id>/model.glb` is
overwritten, its sidecar keeps every run).

## Kept on the old body (2026-11-21)

- **MuteSwan, GreyHeron**: the S-shaped neck. The chord centreline loses the head; the medial centreline reaches it,
  but the template's neck pose then folds the head onto the chest. Fix: re-scan with the neck straight and level
  (`blender/birds/scan_raw/sheet_11.png`, sheets.json entry 11, already cut: `cut_sheet.py ... MuteSwan GreyHeron
  Cormorant`), about 90 hi3d points; the balance was 9 points that night.
- **CommonSwift**: the scan (clinging pose) broke into debris.
- **BlueTit**: folded wing/scapular cards stick out of the scan body.
- **CommonBuzzard**: the wing-root cut is huge (the back is hollow) and the far wing sticks up.

## Known flaws

- The tail-cut cap is a smooth, slightly pale disc at the tail base on small birds; Fieldfare shows a pale arc on the
  shoulder. Shoulder bumps from the scan's raised wing roots.
- Spread wings in flight are still cards (the template's planform with the scan's outline and colours), ragged on
  small birds.
- Legs are procedural, not the scan's.
- The scan texture has hi3d's uv islands: lint reports uv_stretch on the scan body (no z-fights).

## Wanted next

- New species requested by the brook item: **Kingfisher** and **White-throated Dipper** (sheet + hi3d, ~90 points).
- MuteSwan and GreyHeron from sheet 11.
- Scan legs instead of procedural ones; scapular card row over the shoulder seam.
