# Gastronom No. 7: audit (2026-11-01) and texture plan

## Audit: what the hall looks like now (offscreen -game shots, Saved/GastroTest/hall_before)
Every AC_G* material reaches Unreal as a **flat colour** (import_academgorodok._extra_materials: hex + roughness only,
no texture set), so:
- **Walls**: the "15 cm glazed tiles" are a smooth pale-blue / ivory paint, no tiles, no grout. Plaster above is flat.
- **Floor**: one flat cream sheet, glossy and over-bright (blown out under the pendants); no slabs, no wear.
- **Ceiling**: flat; cornice reads, nothing else.
- **Counters**: oak body and panels are flat orange-brown blocks; the marble tops are plain white; no wood grain.
- **Vitrinas**: glass and nickel frames are fine, but the goods inside are untextured bars: sausages are brown sticks,
  cheese is a yellow cylinder, butter and sweets are coloured slabs.
- **Back cases / shelves**: dense, but every product is a single-colour box, can or bottle with no label; reads as
  toy blocks. Sugar loaves read as blue-and-white cones.
- **Boards and posters**: maroon panels with one word ("Прейскурант"), no prices; the rules board works.
- **Cash booth**: boxy, the till is a grey wedge; no cash register detail.
- **Scales**: a teal box + disc: no dial face, no pointer you can read.
- **Lamps**: OK (opal cylinders, turquoise cups).
- **Missing for a 1970s-80s gastronom**: refrigerated display counters (white enamel, curved glass), a KKM cash
  register, a milk/juice section, price tags on the goods, posters, the abacus, a shelf of tins in pyramids,
  bread trays, a back-room door that reads as a door (the shot from behind the counter doesn't show one).
- **Style**: the module is written as an early-1950s shop (oak, brass, marble); Dima asked for 1970s-80s. The plan
  keeps the shell and turns the fittings, materials and goods towards the 70s-80s (enamel fridges, metlakh/terrazzo,
  plainer counters).

## People (Saved/GastroTest/people_trade)
- orlova_tanya stands at her counter with her **legs in a frozen stride**: the front leg is planted, the back knee bent
  and that foot up on its toes, both with StandIdle and with FoldArms. **Not local to the gastronom**: the floor under
  her is flat (a trace grid round her slot shows only the 10 cm plinths 0.6 m away), and the same pose stays with foot
  IK off (new test cvar lyud.Body.FeetIK 0). So it is the shared stand clips on her body (Orlova_tanya, mesh family
  Streamer): the anim/rig thread's area (fit_legs / retarget onto that rig).
- The customers sent to shop were still walking in (no standing customer to judge yet).

## Texture generation plan (awaiting Dima's OK)

Service: Higgsfield MCP `generate_image`, model `gpt_image_2_5` (text rendering is good, so the packaging words come out
right), 4K, 1:1. Every request is pinned in `design/grocery/<sheet>.json` (model, prompt, settings, job id, output URL,
credits), like `icons_grid.json`. `tools/grocery_assets.py` cuts the sheets. Surfaces then go through the existing
`tools/fal_academ_textures.py post` step: even light, seamless, colour matched to the slot, `_D/_N/_R` maps, tiles and
grout laid out in code. Nothing is generated per object.

Measured prices (get_cost, 2026-11-01): 4K high **4.25 credits**, 4K medium **1.25 credits**. Balance: 1164 before the test.

**Test sheet already made** (allowed as one cheap sheet): `design/grocery/surfaces_grid.png` (+ `.json`), sheet 1 below,
4.25 credits. All nine cells usable (blue and ivory 15 cm tiles with grout, metlakh chequer, terrazzo, oak, beech,
marble, ceiling plaster, enamel). So sheet 1 is paid, and the remaining total is the figure minus 4.25.

## Sheets

| # | Sheet | Grid | What is on it | Words? | Quality | Credits |
|---|---|---|---|---|---|---|
| 1 | `surfaces_grid` | 3x3 | glazed wall tile (pale blue), glazed tile (ivory), 1970s brown-beige metlakh floor tile, terrazzo floor, varnished oak, light beech worktop, white-grey marble, painted plaster ceiling, white enamel metal | no | high | 4.25 |
| 2 | `goods_grid_a` | 4x4 | front labels of Soviet 70s-80s groceries: sprats, kilka in tomato, condensed milk, stewed meat, Indian tea with the elephant, coffee, sugar, salt, flour, buckwheat, pasta, matches, Alyonka chocolate, biscuits, a cake box, a butter pack | yes, ru | high | 4.25 |
| 3 | `goods_grid_b` | 4x4 | bottle and jar labels: Buratino, Duchess, Tarkhun lemonades, Zhigulyovskoye beer, mineral water, birch-sap and tomato juice for 3-litre jars, pickles, jam, milk, kefir and smetana caps, a milk tetra-pyramid, vodka, port wine, sunflower oil, mayonnaise | yes, ru | high | 4.25 |
| 4 | `food_grid` | 4x4 | food in the vitrinas, no packaging: Doktorskaya sausage cut and side, smoked salami, ham, cheese wheel rind and cut, butter block, herring, pelmeni, eggs in a tray, bread loaf crust, rye bread, sweets in a pile, a cake top, a fish fillet, sugar cubes | no | high | 4.25 |
| 5 | `boards_grid` | 3x2 | posters and price boards: "Drink tomato juice", "Keep your money in the savings bank", "Fish day: Thursday", a price list board, opening hours, shop rules | yes, ru | high | 4.25 |
| 6 | `boards_grid_en`, `_ja` | 3x2 | sheet 5 redrawn in English and Japanese (the localization skill: words on textures are keys) | yes | medium | 2 x 1.25 |
| 7 | `goods_grid_a/b_en`, `_ja` (only if Dima wants the packaging translated; I'd keep it Russian as set dressing, see below) | 4x4 | sheets 2-3 redrawn | yes | medium | 4 x 1.25 |

**Total: 21.25 credits** for sheets 1-5 plus the en/ja boards. **26.25 credits** with translated packaging (sheet 7).
I'd budget one redo per sheet in case a cell comes out wrong (~+20). So **≤ 50 credits** in the worst case.

Optional, only if new goods go on sale (an item with an icon is the `game-items` rule): `goods_icons_grid` 3x3 +
`_en`/`_ja`, 4.25 + 2 x 1.25 = 6.75 credits for up to 9 new items (e.g. condensed milk, sprats, tea, chocolate, lemonade).

## Question for Dima
- Packaging words: keep them **Russian in every language**, as set dressing seen through glass? Or translate them like the
  boards (+5 credits)? Per the localization rule every word is still a key in `strings_*.json`.
- Sell some of the new goods (needs icons, see above)?

## Free work (no credits)
- The normal and roughness maps are made from the sheets in code, not generated.
- In Blender: refrigerated display counter, cash register (Soviet KKM), a second scale, price tags on the goods,
  product variety (lathe and box primitives with label UVs), a back-room door as an `ALyudDoor`, a UV/overlap lint, and
  close-up renders.
