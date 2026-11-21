# The hero's look

Interactives backlog item 20. The barber's chair and the department store's clothes departments were acts that changed
nothing. Now a haircut is a haircut, the clothes are worn, and the town notices both.

## Where the hero meets it

- **The barber** (any barber's chair: act `haircut`, 40 rub., the barber must be at work). The chair takes the money, the
  barber throws the sheet over him and asks how to cut. **1–3** choose: a crop with the clippers all over (под машинку),
  short back and sides (полубокс), a side parting. **Esc** is "any way you like": a trim of what he has, or short back and
  sides over a grown-out head (the chair is paid for either way). The same cut again is a trim: fresh for longer, nothing
  new for the town to say.
- **Hair grows.** A crop lasts 30 days, short back and sides 21, the parting 14; then it is his own shaggy head again and a
  line says so ("Волосы отросли. Опять лезут в глаза").
- **The clothes rail** (the department store «Маяк», first floor: `ps_clothes`). **1–4** buy and put on: a bottle-green
  windbreaker 90 rub., a yellow raincoat 120, a navy winter coat 260, a grey flat cap 45. A piece he already owns is put
  on for nothing.
- **The fitting room** (`ps_fitting_room`): every piece he owns, **1–9** put one on. The cap comes off again; an outer
  piece comes off into his own corduroy overshirt (see "What is drawn"). The inventory's worn page does the same anywhere.
- **What the town says.** For three days after a new haircut the residents who greet him add a word about it a few
  seconds after the greeting: each of them once, six people at most per haircut, not more often than every half hour
  (three lines per cut, the same person the same line). And once a day, out of doors, somebody remarks on clothes that do
  not suit the weather.

## Clothes and weather

| Piece | Slot | Rub. | Warmer by | Rain |
|---|---|---|---|---|
| overshirt (his own) | outer | – | 3° | – |
| wind_jacket | outer | 90 | 6° | – |
| raincoat | outer | 120 | 4° | keeps dry |
| winter_coat | outer | 260 | 16° | – |
| cap | head | 45 | 2° | – |

`LyudLook::Fit`: **wet** in rain (drizzle, rain, showers, a thunderstorm, sleet, freezing rain at 0.15 mm/h or more)
without the raincoat; else **cold** when the air plus what he wears is under 12°; else **hot** in the winter coat above
14°. Wet is said before cold. `tools/looksim/run.sh weather` prints the table: the overshirt, the windbreaker and the
raincoat are fine from about 10°, the winter coat from 0° to 14°. Nothing is taken from him for being cold: the remark
is the whole consequence (the winter play's own cold is the banya's and the ice hole's, `docs/winter_play.md`).

Under a roof (a trace 15 m up from him hits something) nobody speaks of the weather.

## What is drawn

The hero is seen from inside (`AHeroCharacter`): his arms and chest in view, his head only as a shadow.

- **The outer layer** is the body's own overshirt in another cloth. The hoodie under it is cut away where the overshirt
  covers it (`blender/hero/build_hero.py`), so the outer layer can change but cannot come off. `ULyudLook` finds the
  body's material slot whose name holds "overshirt" and puts a dynamic instance of its own material there: `BaseColor` a
  white texture, `Tint` and `Roughness` from the piece (`LyudLook::Clothes`), the overshirt's folds kept in the normal map
  except for the raincoat (roughness under 0.4: a flat normal, rubber has no weave). One instance at a time, made when the
  piece changes, the old one dropped; nothing per frame. His own overshirt (or an empty slot) puts the original material
  back.
- **The haircuts and the cap** are four small meshes for the shadow's head: `/Game/Characters/Hero/Look/SM_HeroHair_crop`,
  `_short`, `_parted` and `SM_HeroCap` (about 1,100 triangles each, shadow only: not in the main or depth pass, no
  collision). While a barber's cut is on, the shadow body's own hair sections (slots named "hair") are hidden and the
  cut's mesh sits on the head bone where the bind pose has it. **The assets are optional**: without them the shadow keeps
  its own head and everything else works (the log says once which mesh is missing).
- Limits, on purpose: the visible body has no head, so a haircut shows in the shadow, in the words and in the diary, not
  in a mirror; the trousers, the hoodie and the shoes do not change.

## The meshes (Mac)

```sh
/Applications/Blender.app/Contents/MacOS/Blender -b -P blender/hero/build_looks.py -- --preview
unreal/scripts/import_looks.sh           # editor closed on this checkout; one UE slot per mesh
```

`build_looks.py` reads `blender/hero/export/human_hero.glb` (the hero build's export, local only), takes the head's skin
(faces weighted to the `head` bone, without hair, brows and lashes), finds the face from the nose, and wraps a dome of
rings onto the skull: cut at a hairline (high over the forehead, above the ears, low at the nape), pushed out by the
cut's length (crop 3 mm; short 18 mm on top and 4 mm below the temples; parted 30 mm on top swept 12 mm to his left with
a fringe, 14 mm at the sides). The cap is the same dome cut higher, flattened and pulled forward, with a short peak.

Checks, in the script (it fails otherwise) and in `looks_report.json`: every UV inside 0..1, no UV triangle over another
(counted on a 384 grid), no degenerate triangle, no vertex inside the skull, nothing further from the skull than the
cut allows. `python3 blender/hero/build_looks.py --standin` (pip `bpy`) runs all of it on a stand-in head: that is what
the cloud could check. **On the real head, look at the previews** (`renders/hero/looks_*.png`): the hairline's angles
(`HAIRLINE`) may want a few degrees for the hero's ears.

## Code

- `Private/Look/LookModel.{h,cpp}` (namespace `LyudLook`, plain C++): the cuts and how long they last, the clothes table,
  `Fit`, who has spoken of a haircut, `Save`/`Load`. `tools/looksim/run.sh test` (156 checks), `weather`.
- `Public/Look/LyudLook.h`, `Private/Look/LyudLook.cpp` (`ULyudLook`, a world subsystem): the three pages (the city's
  `SLyudCityPage`, opened by the acts' `"open": "barber" | "clothes" | "fitting"` through `ULyudCityThings::OnOpen`), the
  cloth and the head meshes (compared with the pockets and the calendar twice a second), `OnGreeted` (called by
  `ULyudTownTalkSubsystem` beside the friends' hook; the word is said with `SayLine` 3.2 s later if the resident is still
  within 7 m). Saved under "Look".
- Stamps: the collection «Перед зеркалом» (`looks`: crop, short, parted, wind_jacket, raincoat, winter_coat, cap). Quest
  events as before the pages: `do haircut`, `do ps_clothes`, `do ps_fitting_room`. Diary: the first haircut, the first
  piece bought.
- Words: `tools/city/content/strings/look.json` (ru, en, ja). Content: `tools/city/content/x_look.json`,
  `activities.json` (haircut), `x_p_store.json` (the two acts). Since the acts open pages, their old lines, the haircut's
  30 minutes and the fitting room's "a showing, not a sale" lines are no longer said.
- Items: `wind_jacket`, `raincoat`, `winter_coat` (Outer), `cap` (Head) in `LyudInventory.cpp`; icons as one 2×2 grid,
  `localization/icon_grids_look.json` (generated 2026-11-21, 1 credit; cut on the Mac:
  `python3 tools/garden/icons.py --plan localization/icon_grids_look.json`, the cloud cannot reach the picture's host).
- Console: `lyud.Look.Info`, `lyud.Look.Cut [crop|short|parted]`, `lyud.Look.Wear <item>`,
  `lyud.Look.Open <barber|clothes|fitting>`. Developer mode (F7, group «Вид героя»): the next haircut, all the store's
  clothes, the next outer piece.

## Performance

Nothing ticks per frame beyond two float counters: the look is compared at 2 Hz, a material instance is made only when
the outer piece changes, the head meshes are two static mesh components at most (shadow pass only). The weather's fit is
worked out only when somebody greets him. No new actors, no new draw calls in the main pass.

## Still to do on the Mac

First compile; `build_looks.py` on the real hero and a look at its previews; `import_looks.sh`; cut the icons; shots of
each outer piece (arms in view) and of the shadow with each cut and the cap; the bench against main (expected: no change).
If the hero's overshirt or hair slot is named otherwise, `lyud.Look.Info` says "no overshirt slot".
