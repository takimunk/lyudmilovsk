# Academgorodok: sources and provenance of assets

Every texture, material and render of the Academgorodok build that did not start in this repo is listed here with
where it came from. Reference photos (not shipped in the game, used only to model and colour-match) are listed
with author and licence in `docs/academgorodok/refs/exterior/manifest.json` and `refs/interior/`.

## Textures: `textures/academ/`

* **Generator:** fal.ai, model `google/nano-banana-2.1` (Nano Banana 2.1), `resolution: 2K`, `aspect_ratio: 1:1`, PNG, one image per
  prompt. Script: `tools/fal_academ_textures.py` (fetch step). Raw images go to `textures/academ/raw/`, which is
  git-ignored.
* **Date generated:** 2026-10-24.
* **Post-processing** (same script, run in Blender for numpy): large-scale light evened out, made seamless by
  cross-fading with a half-shifted copy, mean colour matched to the target in `blender/academgorodok/common.py`.
  For `brick_red`, `rustic_dark` and `roof_steel` the joints and seams are laid out in code (sizes below) and the
  generated photo is only the surface of each brick, block or sheet. `_N` (normal, DirectX green) and `_R`
  (R roughness, G cavity AO, B height) are derived from the result. `academ_grime_mask.jpg` is fully procedural.
* **Licence:** generated for this project; no third-party image was used as input. The textures are the
  project's own assets under CC BY 4.0 (`LICENSE-ASSETS`).
* **Common prompt prefix** (STYLE), put before each prompt below:

  > Seamless tileable texture, photographed straight on, orthographic, perfectly flat even overcast light, no cast shadows, no perspective, no vignette, no text, no watermark, no objects, detail spread evenly with no single feature in the middle, fills the whole square edge to edge, photorealistic.

| Texture files | Tile | Raw image | Prompt (after the prefix) | Layout made in code |
|---|---|---|---|---|
| `plaster_ochre_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 2.0 m | `plaster_ochre` | A 2 x 2 metre patch of an old facade wall of 1940s Moscow neoclassical housing: ochre / pale yellow lime plaster painted with lime wash, matte, subtle trowel and float marks, faint grey dirt and weathering, a few small patches where the paint has peeled showing paler grey plaster underneath, a few hairline cracks | none (photo made seamless) |
| `plaster_white_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `plaster_white` | A 1 x 1 metre patch of smooth off-white painted lime plaster of a cornice or window surround, matte chalky paint, very faint brush strokes, slight grey weathering, a few tiny chips | none (photo made seamless) |
| `rustic_dark_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `rustic_rock` | Rough rock-faced (quarry-pitched) dark grey-black stone surface, coarse chisel-split bumps and hollows, granular texture, slightly dusty, one continuous surface with no joints or edges | blocks 1.0 x 0.5 m staggered by half, 20 mm sunk joints, domed rock face |
| `quoin_vermiculated_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `quoin_vermiculated` | A 1 x 1 metre patch of the face of a vermiculated rusticated stone quoin: grey-beige cast stone carved with dense irregular worm-tracked grooves (vermiculation) all over, the raised flat islands between the winding channels, old paint, light dust in the grooves, one continuous surface with no block edges | none (photo made seamless) |
| `brick_red_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `brick_face` | The flat face of old fired red clay bricks seen very close, filling the frame with one continuous surface: sandy pitted orange-red clay, small dark burnt specks, tiny lime spots, slight soot, no mortar joints, no brick edges | running bond, 250 x 65 mm bricks, 10 mm mortar joints: 4 stretchers x 13 courses per tile |
| `roof_steel_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.2 m | `steel_paint` | A flat sheet of old painted roofing steel: light grey oil paint, slightly chalky and faded, faint brush strokes, a few tiny rust specks and small dirt streaks, no seams, no rivets | two 0.6 m panels with standing seams, one flat transverse lock seam per sheet (staggered) |
| `timber_stained_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `timber_stained` | Old planed pine boards of a roof eave stained dark brown, straight grain running horizontally, a few small knots, worn stain, slightly weathered matte surface, no gaps | none (photo made seamless) |
| `asphalt_D.jpg`, `_N.jpg`, `_R.jpg` (1024 px) | 2.0 m | `asphalt` | A 2 x 2 metre patch of old grey Soviet asphalt of a courtyard drive, fine gravel aggregate, slightly worn and faded, a few fine cracks and small dark patches | none (photo made seamless) |
| `concrete_D.jpg`, `_N.jpg`, `_R.jpg` (1024 px) | 2.0 m | `concrete` | A 2 x 2 metre patch of old cast concrete slab of a step or pavement, grey, fine pores, small aggregate showing, faint stains | none (photo made seamless) |
| `soil_bed_D.jpg`, `_N.jpg`, `_R.jpg` (1024 px) | 1.0 m | `soil_bed` | A 1 x 1 metre patch of dark brown loose garden soil of a city flowerbed, small clods, a few tiny pebbles and bits of dry leaves | none (photo made seamless) |
| `stone_cast_D.jpg`, `_N.jpg`, `_R.jpg` (2048 px) | 1.0 m | `stone_cast` | A 1 x 1 metre patch of white cast stone of a fountain bowl: off-white, fine sandy grain, faint grey weathering, a few tiny pores | none (photo made seamless) |
| `academ_grime_mask.jpg` (1024 px, greyscale) | 4.0 m | none | none: procedural (numpy, seed 7) | vertical dirt streaks fading downward plus soft blotches; white = dirt |

`textures/academ/academ_textures.json` is written by the same script (tile size, pixel size, target and measured mean
colour, roughness per texture) and read by `blender/academgorodok/materials.py` and
`unreal/scripts/academ_materials.py`.

## Renders: `renders/academgorodok/`

| File | Made by | Date |
|---|---|---|
| `materials_preview.png` | `blender -b --factory-startup --python blender/academgorodok/materials.py -- preview renders/academgorodok/materials_preview.png` (EEVEE) | 2026-10-24 |
