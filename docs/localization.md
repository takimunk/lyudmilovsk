# Localization architecture

Languages: Russian (`ru`, the source language), English (`en`), Japanese (`ja`). Any new one is a new `<lang>` code.

**One rule:** a word is never written in code, a script, a prompt or a texture. It is a *string key* in a table; everything that shows,
prints or speaks the word is derived from the table, per language, and can be regenerated.

```
strings_<lang>.json (the words) ──┬─ runtime UI / names / subtitles   LyudText::Get("key", {args})
unreal/Lyudmilovsk/Content/       ├─ graphic assets with words        tools/gen_localized.py  -> textures/localized/<lang>/<id>.jpg (+ .json prompt record)
Localization/                     └─ voice-over of line.* keys        tools/gen_voice.py      -> audio/voice/<lang>/<key>.wav (+ .json)
localization/assets.json (asset kinds, styles, prompts with <<key>> placeholders)
localization/voice.json  (speakers: voice + direction, per-line directions, TTS model)
```

## 1. Strings
* `Content/Localization/strings_<lang>.json`, a flat `"key": "text"` object, same keys in every language. `ru` is the master; a key
  missing in another language falls back to `ru`, then to the key itself (so a gap is visible in game as `ui.something`).
* Key names, lowercase, dot-separated, stable (never reuse a key for other text):
  `ui.<screen>.<what>`, `item.<id>.name|desc`, `npc.<id>.name|role`, `line.<speaker>.<scene>.<n>` (spoken lines, also voiced),
  `label.*`, `poster.*`, `sign.*` (words printed on generated textures), `lang.name|script` (descriptors for prompts).
* `{0} {1}` placeholders are filled by the caller (`FStringFormatArg`), never concatenate translated pieces; the order of
  words differs between languages, so one sentence = one key.
* Runtime: `LyudText::Get(key, args)` (FText), `LyudText::LoadTexture(folder, name)` (`/Game/<Folder>/Localized/<lang>/…`),
  `LyudText::LoadVoice(key)`. The language is the console variable `lyud.Language` (`ru` default). Strings re-read at every level start.
* Japanese: Slate's default font falls back to the engine's CJK font, so `ja` text shows without extra setup; check line breaks
  (no spaces: use `AutoWrapText` and keep lines short).
* This is deliberately not `LOCTEXT`/gather: one editable file per language is what an LLM translator, the image generator and the voice
  generator all read. If a human translation tool is wanted later, export these files to PO (a converter is trivial) or turn
  on a Localization Dashboard target (needs Config changes, ask the coordinator).

## 2. Graphic assets with words
* `localization/assets.json`: per asset `kind` (poster, label, icon, sign, newspaper), `aspect`, a `prompt` with `<<key>>` placeholders
  and a shared `styles[kind]` prefix (one art direction per kind, so a series looks the same). The prompt carries *no* literal text.
* `python3 tools/gen_localized.py --lang <lang> [ids]` fills the placeholders from `strings_<lang>.json`, calls fal, writes
  `textures/localized/<lang>/<id>.jpg` and `<id>.json` (exact prompt, strings used, model). `--list`, `--prompt id`, `--uses key`,
  `--force` after changing a word.
* `unreal/scripts/import_hall_assets.py` imports every language it finds (`/Game/Hall|Inventory/Localized/<lang>/…`).
  Blender (`build_scene01_corridor.py`) reads `textures/localized/$LYUD_LANG/<file>` (default `ru`) for what it bakes into the .glb;
  textures baked into the scene need a scene rebuild per language, textures loaded at run time switch by themselves.
* A new text-bearing asset: add its words to every `strings_*.json`, add an entry to `assets.json`, run the generator.

## 3. Dialogue, names and voice-over
* Lines are `line.<speaker>.<scene>.<n>` in the string tables, names `npc.<id>.name|role`. The dialogue *structure* (who talks to whom,
  in which order, what repeats) is language-neutral and refers to keys; the lines of every language live only in the tables.
  (`npc_dialogue.txt` is Russian-only today: the dialogue thread should keep its structure, replace the text by the keys, and read the
  names/lines through `LyudText`; speakers are the NPC ids, `hero` for the player.)
* `localization/voice.json`: per speaker a voice id and a direction ("[calm, tired, slow]", an audio tag the TTS acts out), optional
  `voice_<lang>`, optional per-line direction. `python3 tools/gen_voice.py --lang <lang>` voices what is missing, `--stale` also what changed
  (hash of model + voice + direction + text is kept in the sidecar), then `unreal/scripts/import_voice.py` imports the wavs as
  `/Game/Voice/<lang>/V_<key_with_underscores>`; the game plays `LyudText::LoadVoice(key)` with the subtitle `LyudText::Get(key)`.
* Money, dates and numbers in text: pass as arguments, format per language in the string (`{0} руб.` / `{0} rub.` / `{0}ルーブル`).

## 4. What not to do
Hard-code a word in C++, Python, Blender, a material or an image prompt; build a sentence from fragments; put a number of the
language (plural forms) in a key (use two keys or rephrase); leave a language's key out because "it's the same" (copy it, so the
translator sees it); regenerate a text-bearing image by hand instead of through `assets.json`.

See `.claude/skills/localization/SKILL.md` for the checklist agents follow.
