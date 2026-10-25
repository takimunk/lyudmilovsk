# Academgorodok: sources and provenance of assets

Every texture, material and render of the Academgorodok build that did not start in this repo is listed here with
where it came from. Reference photos (not shipped in the game, used only to model and colour-match) are listed
with author and licence in `docs/academgorodok/refs/exterior/manifest.json` and `refs/interior/manifest.json` (committed; the images themselves are not).

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

## Reference photos and drawings (not shipped)

Used only to model, measure and colour-match; none of these images is in the game or in the repo (they are downloaded to the
git-ignored `docs/academgorodok/refs/`; the manifests next to this file list every one with its URL). Openly licensed
(Wikimedia Commons) ones are marked with their licence; the rest are press, archive and listing photos used as
reference only.

Planning data: building footprints from OpenStreetMap (ways 27830839, 27830823, 27830844; © OpenStreetMap
contributors, ODbL), used as numbers in `blender/academgorodok/plan.py`.

| File | Author | Licence | Source |
|---|---|---|---|
| commons_Maksimova_str._8_1_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Maksimova_str._8_(1).JPG |
| commons_Maksimova_str._8_2_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Maksimova_str._8_(2).JPG |
| commons_Marshal_Novikov_str._1_1_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Marshal_Novikov_str._1_(1).jpg |
| commons_Marshal_Novikov_str._1_2_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Marshal_Novikov_str._1_(2).JPG |
| commons_Marshal_Novikov_str._1_3_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Marshal_Novikov_str._1_(3).JPG |
| commons_Marshal_Novikov_str._1_4_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Marshal_Novikov_str._1_(4).JPG |
| commons_Marshal_Novikov_str._1_gates.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Marshal_Novikov_str._1_gates.JPG |
| commons_Moscow_Marshala_Novikova_Street_18563431703_.jpg | Gennady Grachev from Moscow, Russia | CC BY 2.0 | https://commons.wikimedia.org/wiki/File:Moscow,_Marshala_Novikova_Street_(18563431703).jpg |
| commons_Moscow_Marshala_Novikova_Street_18996369890_.jpg | Gennady Grachev from Moscow, Russia | CC BY 2.0 | https://commons.wikimedia.org/wiki/File:Moscow,_Marshala_Novikova_Street_(18996369890).jpg |
| commons_Moscow_Marshal_Novikov_street_3_2025-02_437.jpg | Artyom Svetlov | CC BY 4.0 | https://commons.wikimedia.org/wiki/File:Moscow_Marshal_Novikov_street_3_2025-02_437.JPG |
| commons_Museum_of_everyday_life_of_soviet_scientists_01.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File:Museum_of_everyday_life_of_soviet_scientists_01.jpg |
| commons_Shchukino_District_Moscow_Russia_-_panoramio_54_.jpg | Svetlov Artem | CC BY 3.0 | https://commons.wikimedia.org/wiki/File:Shchukino_District,_Moscow,_Russia_-_panoramio_(54).jpg |
| commons_Shchukino_District_Moscow_Russia_-_panoramio_56_.jpg | Svetlov Artem | CC BY 3.0 | https://commons.wikimedia.org/wiki/File:Shchukino_District,_Moscow,_Russia_-_panoramio_(56).jpg |
| commons_Shchukino_District_Moscow_Russia_-_panoramio_58_.jpg | Svetlov Artem | CC BY 3.0 | https://commons.wikimedia.org/wiki/File:Shchukino_District,_Moscow,_Russia_-_panoramio_(58).jpg |
| pastvu_1983687.jpg | unknown (photo of a stand in the courtyard, probably from Kurchatov Institute archive); uploaded by oboist | not stated (pastvu: rights of original holders; local reference only, do not redistribute) | https://pastvu.com/p/1983687 |
| pastvu_528948.jpg | unknown, personal archive; uploaded by Maja | not stated (local reference only) | https://pastvu.com/p/528948 |
| commons_Museum_of_everyday_life_of_soviet_scientists_01.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_01.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_02.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_02.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_03.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_03.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_04.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_04.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_05.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_05.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_06.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_06.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_07.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_07.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_08.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_08.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_09.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_09.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_10.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_10.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_11.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_11.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_12.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_12.jpg |
| commons_Museum_of_everyday_life_of_soviet_scientists_excursion.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMuseum_of_everyday_life_of_soviet_scientists_excursion.jpg |
| commons_Maksimova_str._8_1_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMaksimova_str._8_%281%29.JPG |
| commons_Maksimova_str._8_2_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMaksimova_str._8_%282%29.JPG |
| commons_Marshal_Novikov_str._1_1_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMarshal_Novikov_str._1_%281%29.jpg |
| commons_Marshal_Novikov_str._1_2_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMarshal_Novikov_str._1_%282%29.JPG |
| commons_Marshal_Novikov_str._1_3_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMarshal_Novikov_str._1_%283%29.JPG |
| commons_Marshal_Novikov_str._1_4_.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMarshal_Novikov_str._1_%284%29.JPG |
| commons_Marshal_Novikov_str._1_gates.jpg | Andreykor | CC BY-SA 4.0 | https://commons.wikimedia.org/wiki/File%3AMarshal_Novikov_str._1_gates.JPG |
| commons_Moscow_Marshala_Novikova_Street_18563431703_.jpg | Gennady Grachev from Moscow, Russia | CC BY 2.0 | https://commons.wikimedia.org/wiki/File%3AMoscow%2C_Marshala_Novikova_Street_%2818563431703%29.jpg |
| commons_Moscow_Marshala_Novikova_Street_18996369890_.jpg | Gennady Grachev from Moscow, Russia | CC BY 2.0 | https://commons.wikimedia.org/wiki/File%3AMoscow%2C_Marshala_Novikova_Street_%2818996369890%29.jpg |
| commons_Shchukino_District_Moscow_Russia_-_panoramio_54_.jpg | Svetlov Artem | CC BY 3.0 | https://commons.wikimedia.org/wiki/File%3AShchukino_District%2C_Moscow%2C_Russia_-_panoramio_%2854%29.jpg |
| commons_Shchukino_District_Moscow_Russia_-_panoramio_56_.jpg | Svetlov Artem | CC BY 3.0 | https://commons.wikimedia.org/wiki/File%3AShchukino_District%2C_Moscow%2C_Russia_-_panoramio_%2856%29.jpg |
| commons_Moscow_Marshal_Novikov_street_3_2025-02_437.jpg | Artyom Svetlov | CC BY 4.0 | https://commons.wikimedia.org/wiki/File%3AMoscow_Marshal_Novikov_street_3_2025-02_437.JPG |
| m24_2016_flat_01.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_02.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_03.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_04.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_05.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_06.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_07.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_08.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_09.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_10.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_11.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_flat_12.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_01.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_02.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_03.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_04.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_05.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_06.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_07.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| m24_2016_shop_08.jpg | m24.ru / Александр Авилов | unknown – reference only | https://www.m24.ru/articles/muzei/20052016/105500 |
| aif_2019_01.jpg | Юлия Тутина / АиФ | unknown – reference only | https://aif.ru/my_area/schukino/kak_zhili_atomshchiki_v_shchukine_rabotaet_muzey_byta_sovetskih_uchyonyh |
| aif_2019_02.jpg | Юлия Тутина / АиФ | unknown – reference only | https://aif.ru/my_area/schukino/kak_zhili_atomshchiki_v_shchukine_rabotaet_muzey_byta_sovetskih_uchyonyh |
| kp_2019_01.webp | Павел Клоков / КП | unknown – reference only | https://www.kp.ru/daily/27032.7/4095724/ |
| m24_2015_houses_01.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_02.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_03.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_04.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_05.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_06.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_07.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_08.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_09.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_houses_10.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_01.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_02.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_03.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_04.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_05.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_06.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_07.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_08.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| m24_2015_museum_09.jpg | m24.ru | unknown – reference only | https://www.m24.ru/articles/legendy/11032015/67787 |
| tg_post34_01.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/34 |
| tg_post34_02.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/34 |
| tg_post34_03.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/34 |
| tg_post34_04.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/34 |
| tg_post132_01.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_02.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_03.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_04.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_05.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_06.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_07.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_08.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| tg_post132_09.jpg | ТСЖ / канал Академгородка Лаборатории №2 (t.me/akademgorodok_1) | unknown – reference only | https://t.me/akademgorodok_1/132 |
| move_novikova3_01.jpg | move.ru listing 9289717638 (seller) | unknown – reference only | https://move.ru/objects/prodaetsya_5-komnatnaya_kvartira_ploschadyu_160_kvm_moskva_ul_marshala_novikova_3_9289717638/ |
| mk_novikova3_160m2_01.jpg | mirkvartir.ru listing 355187427 (seller/agent) | unknown – reference only | https://www.mirkvartir.ru/355187427/ |
| mk_novikova3_160m2_02.jpg | mirkvartir.ru listing 355187427 (seller/agent) | unknown – reference only | https://www.mirkvartir.ru/355187427/ |
| fochkin2021_p17.jpg | Олег Фочкин, «Жизнь в окружении истории» (Лингва-Ф, 2021) | unknown – reference only | https://rg-dev.ru/okp_dost_book.pdf |
| fochkin2021_p18.jpg | Олег Фочкин, «Жизнь в окружении истории» (Лингва-Ф, 2021) | unknown – reference only | https://rg-dev.ru/okp_dost_book.pdf |
| schukino_moi_raion_2020_058_p36-37.jpg | Ольга Катасонова, «Щукино. Мой район» 2020 №058 | unknown – reference only | https://www.mos.ru/upload/moi-raion/press/schukino/pdf/schukino.pdf |
