# Cigarettes

A pack of ten «Скворец» («Skvorets», 「スクヴォレツ」) cigarettes and a lighter: bought at the Gastronom No. 7 counter
(Orlova Tanya, 35 + 10 roubles) or at a station's press kiosk (`lobby.json` stock `press`, 38 + 12), and one started pack
(7 left) with a lighter lies on the terrace in front of the club's door (E takes both).

## Playing

Tab → the pack → «Взять в руку»: the pack comes up in the right hand, the lid flips open, one cigarette is shaken up and taken
with the lips, the pack goes back to the pocket and the hand takes the cigarette from the lips (between the index and middle
fingers). Then:

| | unlit | lit |
|---|---|---|
| LMB click | lit: to the lips, left there; the hand fetches the lighter, the flame to the tip (click, flame, the ember catches), the lighter back, the hand takes the cigarette (a first breath of smoke) | to the lips and down, nothing smoked |
| LMB hold | the same as a click | a drag: the ember brightens (emissive + a faint light), the paper burns down, a hiss; let go: a breath of smoke out of the mouth, bigger the longer the drag |
| LMB held > 3.2 s (`lyud.Smoke.CoughAfter`) | | the hero coughs (his voice, three bursts of smoke, the view jolts), the hand comes down and the button must be let go |
| RMB or Q | back into the pack | put out and flicked away: the butt flies, lands on whatever is there, smokes for a few seconds |

No lighter in the pocket: «Нечем прикурить». A lit cigarette smoulders down by itself in `lyud.Smoke.BurnMinutes` (7) minutes
between drags; burnt to the filter it is flicked away. With a cigarette in the hand the right mouse button is the cigarette's,
not the binoculars'.

## How it is built

- **Hand**: everything rides on the food thread's `ULyudHandUse` (`Public/Food/LyudHandUse.h`): the pack, the cigarette and the
  lighter are each a `FLyudHandThing` (palm grip `InPalm`, `Carry` and `Mouth` poses, the point that goes to the lips). The
  one-handed lighting is a chain of those: the cigarette at the lips is swapped for a copy the lips hold (placed every frame
  exactly where the hand had it, `MouthInLook`), the lighter's mouth point is chosen so its flame lands under the tip
  (`MouthPointFor`), and the hand comes back for the cigarette with `bHiddenTillLips`. Four small additions to `ULyudHandUse`
  for this: `bHiddenTillLips`, `Edit()` (poses and `Left` changed on the way), `Interrupt()` (the cough), `LipsInLook()`.
- **State**: `ULyudSmoking` (world subsystem, `Private/Smoking/LyudSmoking.cpp`) runs the steps; the hero's packs are the
  inventory item `cigarettes` (one per pack; the open pack's share left in `ULyudInventory::Started`, so it is saved with the
  pocket). `ULyudSmokingState` (game instance, save key `Smoking`) remembers that the terrace's pack was taken. A cigarette in the
  hand is not saved: put back (unlit) or gone (lit) when the level ends.
- **Actors** (`Private/Smoking/SmokingKit.cpp`): `ACigarettePack` (body, lid on its hinge, collar, the ten cigarettes as meshes:
  one less for each taken, `SetLeft`), `ACigaretteItem` (filter, paper scaled to what is left, the burning end with a `Glow`
  material parameter and a tiny light), `ALighterItem` (flame + light, ticks only while lit), `ACigaretteButt`.
- **Smoke**: `ALyudSmokeFx`, one per world: a fixed pool of 96 camera-facing puffs in one instanced mesh (transforms and custom
  data updated incrementally, no proxy rebuild, no allocation; hidden and not ticking when the last puff fades), material
  `M_Smoke` (lit translucent, opacity per instance, depth fade).
- **Perf**: every moving part has `bAffectDynamicIndirectLighting` and `bAffectDistanceFieldLighting` off and no collision; one
  MID per pack (its language's card) and per cigarette (the ember), owned by the actor.

## Assets

| What | Made by | Where |
|---|---|---|
| Meshes (pack body, lid, collar, cigarette whole and in three parts, butt, lighter, flame) | `blender/props/smoking/build_smoking.py` (clean low-poly, its own lint: overlapping faces, UVs; 0 issues) | export/ (built, not committed) → `/Game/Smoking/SM_*` |
| The pack's printed art (no words) | Higgsfield `gpt_image_2_5`, one 4-panel sheet (`design/smoking/pack_art_grid.json`) | `design/smoking/` |
| The pack's card per language | `tools/smoking_pack.py`: the art laid on the UV regions of the builder (`PACK_UV`) and the words of `strings_<lang>.json` (`label.cigarettes.*`) printed in the project's fonts, so the warning «Курение вредит вашему здоровью» is spelled exactly in ru, en, ja | `textures/localized/<lang>/cig_pack.png` → `/Game/Smoking/Localized/<lang>/T_CigPack` |
| Icons (pack, lighter) | Higgsfield 2x2 grid (`design/smoking/icons_grid.json`), cut by `tools/smoking_pack.py` | `textures/localized/ru/icon_*.jpg`; sketches by `tools/ui_diary_icons.py` |
| Filter, ember, smoke puff textures | drawn by `tools/smoking_pack.py` (seeded, no generation) | `textures/smoking/` |
| Sounds: pack, lighter, drag, breath out, butt | `audio/sounds.json` group `smoking` (ElevenLabs SFX via fal) | `audio/sfx/smoking/` → `/Game/Audio/Smoking/` |
| The hero's cough (3 takes × 3 languages) | `line.hero.cough.<n>` in the string tables, MiniMax speech-2.6-hd with the hero's designed voice (`localization/voice.json`; the text's `(coughs)` tags are acted out, never shown) | `audio/voice/<lang>/` → `/Game/Voice/<lang>/` |

Rebuild everything: `Blender -b -P blender/props/smoking/build_smoking.py`, `python3 tools/smoking_pack.py --uv`,
`sh unreal/scripts/import_smoking.sh`, `IMPORT_ONLY=sfx/smoking` import_audio.py, `IMPORT_ONLY=hero.cough` import_voice.py,
import_hall_assets.py (icons) and import_diary_assets.py (sketches; revert the other re-saved uassets).

## Checks

`sh unreal/scripts/test_smoking.sh` (offscreen, Lvl_Scene01): the terrace pack taken, one taken out, lit, a click, a drag, a
cough, thrown away, one put back unlit, the pack in en and ja; `SMOKETEST`/`SMOKE` lines and shots in
`Saved/SmokeTest/`; `FRAMES=1` also records the drag and the cough frame by frame at a fixed 24 fps.
Console: `lyud.Smoke.Give`, `lyud.Smoke.Take`, `lyud.Smoke.Out`, `lyud.Smoke.Pick`, `lyud.Smoke.State`, and the hand's
`lyud.Hand.Press 1|0`, `lyud.Hand.Click`.
