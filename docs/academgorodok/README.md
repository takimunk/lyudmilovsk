# Академгородок Лаборатории № 2 АН СССР

The ensemble of three houses of Laboratory No. 2 (Kurchatov's lab; Moscow, Shchukino, 1945–48; architect
L. B. Karlik under I. V. Zholtovsky) stands before the town, where the cable car brings you down: its square
opens towards the lower station, its two gates lead on into Lyudmilovsk. Every house can be entered: nine
stairs, sixty furnished flats and the ground-floor «Гастроном № 7».

## What is known and what is inferred

| | Source | Confidence |
|---|---|---|
| Footprints, the fan layout, gate gaps (14.3 / 15.4 m) | OpenStreetMap ways 27830839, 27830823, 27830844 | measured |
| Storeys (4 + 3 + 3), ceilings 3.4 m, 2 flats a landing, 60 flats | Wikipedia, press (`interior_notes.md`) | stated |
| Square facade of Novikova 1 (4 windows between three loggia bays, loggias on columns, gables with oculus and swags, dark rusticated portal, modillion eaves) | photos `commons_Marshal_Novikov_str._1_1_/_2_` | measured on photos |
| Side houses' street facade (portals with arched doors, balconies on consoles, pedimented windows, small gables, hooded ground-floor windows), the quoin and the "1947" cartouche | 1946 elevation `m24_2015_houses_06.jpg`, photos | drawing + photos |
| Korpus II is the mirror of korpus I | the 1945/46 setting-out drawing `m24_2015_houses_05.jpg` | stated on the drawing |
| Gates: scroll pylons and the arched wicket block | photos `..._gates`, `18563431703` | photos |
| Courtyard facades, end facades | single photos | partly inferred |
| Flat plans, stair cells | **inferred** (no plan is published): dog-leg stairs behind the entrances, a corridor across each flat, rooms between the window axes; sizes from listings | inferred |
| Gastronom hall | museum photos (`gastronom.py`) | photos, early-1950s fittings inferred |
| Furniture | the museum's flat of B. V. Kurchatov (`furniture.py`) | photos |

The doors are wider than the period ones where needed: every doorway is at least 1.10 × 2.20 m (the game's
standard is 1.08 × 2.10 m, `tools/character_standard.py`).

## Files

* `blender/academgorodok/` — the build (pure-Python geometry + Blender for details and export):
  `plan.py` (plan, facades), `interiors.py` (floors, stairs, flats, walking routes), `details.py` (detail meshes),
  `gastronom.py`, `furniture.py`, `materials.py` (Blender preview materials), `geom.py`, `common.py`,
  `build_academgorodok.py` (the build), `preview_*.py` (preview renders of the parts).
* `blender/export/academgorodok/layout.json` — where each part goes (ensemble frame), the square's trees, the lamps,
  the walking routes. The `.glb` it was built with is not committed (`build_academgorodok.py` makes it).
* `unreal/Lyudmilovsk/Content/Academgorodok/` — the imported meshes (Nanite, full-resolution fallback for complex
  collision), materials and textures.
* `tools/world_map.py` — `academ()`: the ensemble's place on the map; its ground is levelled and the town's houses
  keep out of its rectangle (`academ_flat`).
* `textures/academ/` — plaster, rustication, brick, roof steel, timber… (`tools/fal_academ_textures.py`).
* `docs/academgorodok/facade_notes.md`, `interior_notes.md` — the measurements and reasoning; `SOURCES.md` — every
  source and licence.

## Rebuild

```sh
/Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/build_academgorodok.py -- --no-render
unreal/scripts/import_academgorodok.sh          # import (Interchange), then place it in Lvl_Scene01
```

A full `unreal/scripts/build_scene01.sh` places it too (`import_world.py` calls `import_academgorodok.place`).
Without `--no-render` the build also renders views matched to the reference photos into `renders/academgorodok/`.

## Checks (offscreen, no window)

```sh
unreal/scripts/shoot_academgorodok.sh           # shots from the photos' viewpoints, 5..300 m, inside
unreal/scripts/test_academ_walk.sh              # the character walks every stair and into every room
LABEL=academ EXTRA="-LyudBenchPasses=on=lyud.Academ+1|off=lyud.Academ+0" tools/bench.sh 3
```

`lyud.Academ 0` hides the whole ensemble (and its collision) at run time, for A/B benchmarks.
`ACADEM_CMDS="lyud.SkyHourOffset -3.5"` before the shots moves the sky to midday.
