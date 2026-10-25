---
name: localization
description: How to handle ANY text in the Lyudmilovsk game (UI, item and NPC names, dialogue lines, words printed on generated textures, voice-over) so it works in Russian, English and Japanese. Use whenever you add, change or show a word, a label, a poster, a sign, a prompt-generated asset with text, or a spoken line.
---

# Localization of Lyudmilovsk

Languages: `ru` (master), `en`, `ja`. Full design: `docs/localization.md`. The rule: **no literal text in code, scripts, prompts or
textures: every word is a key in `unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json`.**

## Do this for any new text

1. **Pick the key** (stable, lowercase, dots): `ui.<screen>.<what>`, `item.<id>.name|desc`, `npc.<id>.name|role`,
   `line.<speaker>.<scene>.<n>` (spoken), `label.*` / `poster.*` / `sign.*` (printed on textures). One sentence = one key; placeholders `{0}`
   for numbers and names; never join translated pieces.
2. **Add it to all three tables** (`strings_ru.json`, `strings_en.json`, `strings_ja.json`, same key order). Write real translations (Japanese:
   natural, short lines, no spaces needed). Never leave a language out.
3. **Show it** in C++ with `LyudText::Get(TEXT("key"), { FStringFormatArg(x) })` (`#include "LyudText.h"`); in Python tools read the JSON.
   Textures and sounds that vary by language: `LyudText::LoadTexture(Folder, Name)`, `LyudText::LoadVoice(key)`.
4. **Text on a generated image** (poster, label, sign, icon with a name): the prompt goes in `localization/assets.json` with `<<key>>`
   placeholders (an entry per asset, a shared style per kind so the series matches), never literal words. Generate with
   `python3 tools/gen_localized.py --lang <lang> <id>` (needs `FAL_API_KEY`; the key is in `~/projects/asmrx3000/.env`, never commit it),
   check the spelling in the picture, then `unreal/scripts/import_hall_assets.py`. A changed word: `--uses key` lists the assets, regenerate with `--force`.
5. **Spoken line**: key `line.<speaker>.<scene>.<n>`; speaker in `localization/voice.json` (voice id + direction); `python3 tools/gen_voice.py --lang <lang>`
   then `unreal/scripts/import_voice.py`. Show the subtitle from the same key.
6. **Check**: `python3 tools/check_localization.py` (same keys and placeholders in all languages; `--code` lists literal text still in C++).
7. **Test**: `lyud.Language en` / `ja` in the console (or `-ExecCmds="lyud.Language ja"`) and look for raw keys on screen: a key shown as
   itself is missing in that table.

## Items
Every in-game item also needs an icon (`icon_<id>` in `localization/assets.json`): see the `game-items` skill.

## Never
Hard-code a word or a number format; reuse a key for different text; put a plural or gendered form inside one string
(use two keys); edit a generated texture by hand (change the prompt or the strings and regenerate); commit generated images for a
language nobody asked for (LFS budget): generate on demand, commit what ships.

## Where things are
`strings_<lang>.json` · `localization/assets.json` · `localization/voice.json` · `tools/gen_localized.py` · `tools/gen_voice.py` ·
`textures/localized/<lang>/` · `audio/voice/<lang>/` · `Source/Lyudmilovsk/Public/LyudText.h`.
