# The game's interface

One game screen, the hero's diary (an A4 planner he takes out of his bag: docs/diary.md), a HUD over the game, and the prompt line for things you can use. All of it is Slate in C++
(`Source/Lyudmilovsk/Private/UI`), every word a key in the string tables (the `localization` skill), ru / en / ja.

## Keys

| Key | What |
|---|---|
| Tab or I | the diary on its things (again: put it away) |
| J | the diary on the to-do list (the quests) |
| M | the diary on the map; on the map the wheel zooms at the cursor, dragging moves it, C finds you |
| A / D, arrows, PgUp / PgDn | turn a page (also the tabs on the fore-edge and the dog-eared corners) |
| Esc | puts the diary away (F10 in the editor opens the game menu, which puts it away too) |
| B | the bird guide (when the hero has it, docs/bird_guide.md); B or Esc shuts it |
| L | head lamp on and off (when it is worn) |
| right mouse button, held | binoculars (when they are worn) |

The diary does not pause the game: walking and looking stop and the mouse shows (as the inventory always did). The to-do list and
the map open as a bird too (no hands: the pages just fade in); the things need the hero. Keys are polled, so nothing else loses a key.

## Pages

The diary's spreads, in the order they are in the book (each section's key opens on it; docs/diary.md has the look):

- **Things**: on the left what the hero wears, one row per slot (head, neck, outerwear, body, legs, feet, in the hand); on the
  right what is in the pockets, the chosen thing with its icon, description and what can be done with it (put on / take off, take
  in the hand, drink, eat). The money and the game date are written at the top of the pages.
- **To do**: the quests (current ones, main first, then the finished ones) with the story so far and what to do now (with
  "follow" and "show on the map"). Then the **diary**: every entry by day, oldest first, over as many spreads as it takes (`Screen.Open diary`
  opens its newest spread).
- **Map**: the map drawn in pen and pencil on the paper (`tools/ui_map_ink.py`, from the same world as the painted one at real scale; no words in the picture), the names of the places found (others a faint dot), the quests' next steps circled in red pen (the followed quest's
  twice, pulsing), the player as a red arrow, a scale bar and north. A sticky note: the nearest place and the places found.

## HUD

Top right: the date and time (`UGameCalendar`), the followed quest and its next step. Top left: short notes for a few seconds
(new quest, quest updated or done, new place, an item got). Hidden while the diary, the menu or a trade is open; `lyud.Hud 0`
hides it. Prompts ("[E] ...") go through `ULyudInventoryUI::SetPrompt`, never on-screen debug text.

## Settings

The game menu (Esc / F10) has a Settings page on the main and the pause menu: the language (Русский, English, 日本語; every
menu screen is rebuilt at once), four sound volumes (master, surroundings, music, effects; `ULyudSoundscapeSubsystem::SetVolumes`,
applied as the slider moves) and the HUD on or off. They are kept per machine, not per save, in GameUserSettings.ini
(`[Lyudmilovsk.Settings]`, `LyudSettings` in `Private/UI/LyudSettings.cpp`): the language and the HUD are applied once per run, the
volumes into every new world's soundscape. Console: `lyud.Menu settings|pause|main|close`, `lyud.Settings.Volume <which> <0..1>`,
`lyud.Settings.Language ru|en|ja`; offscreen check `unreal/scripts/test_settings.py`.

## Quests

`Content/UI/quests.json`: a quest is a chain of stages, a stage a few objectives; each objective waits for a game event `Need`
times. A quest begins at a new game (`auto_start`), on an event (`start_on`), or from another quest's reward (`reward.start`);
rewards also give items and roubles. Any system reports events with `ULyudQuests::Notify(this, "event", "target")`:

| Event | Target | Sent by |
|---|---|---|
| `talk` | NPC id (`owner`, `clerk`, ...) | `UNpcDialogueSubsystem::StartTalk` |
| `leave_computer` | | `AAtonComputer` (Leave on the desktop) |
| `reach` | place id from `Content/UI/map.json` | the game screen, from the player's position (on the way in) |
| `get` / `use` | item id (`drink_*` matches by prefix) | `ULyudInventory::Add` / `Remove` (a drink drunk, a handful given) |
| `have` | item id | objective only: counts what is in the pocket |
| `wear` | item id | `ULyudInventory::Wear` / `Equip` |
| `bird` | | the game screen, when the player's pawn stops being the hero |
| `walk` | | the game screen, once per level start, 2.5 m from where the hero stood (the game starts standing) |

An objective's event may list alternatives (`leave_computer|walk`). An event that a later stage of an active quest waits for
finishes the stages before it, so a teleport, a loaded save or steps done out of order never leave a quest on an old step
(`unreal/scripts/test_quest_flow.py` checks the normal way, a teleport and a load).

Words: `quest.<id>.title|desc`, `quest.<id>.<stage>.log` (the journal entry when the stage begins), `quest.<id>.<objective>.obj`,
`quest.<id>.done`. Places: `place.<id>.name`. Progress, the journal, the places found and the followed quest are saved ("Quests").

The four quests there now (Awakening, Pigeons by the door, Soda, Around here) only use what the game has; they are placeholders,
not story canon (docs/story).

## Equipment

`ELyudItemKind::Wear` items have an `ELyudEquipSlot`; `ULyudInventory::Worn` (saved) holds one per slot, the hand stays
`Equipped`. A new game starts in the clothes from `blender/hero/build_hero.py` (overshirt, hoodie, cargo trousers, sneakers);
the hero's mesh does not change with them yet. The club owner sells a head lamp (40 rub.) and binoculars (60 rub.) at his
window. The head lamp is a spot light following the view; the binoculars zoom the view 5x behind a two-lens mask.

## Files

`Public/UI/LyudQuests.h` (quests, journal, places, map frame), `Public/UI/LyudGameScreen.h` (the screen, the HUD, the lamp and the
binoculars), `Private/UI/SLyudDiary.*`, `LyudDiaryStyle.*`, `Public/UI/LyudDiaryBook.h` (docs/diary.md), `SLyudMapView.*`, `SLyudHud.*`, `LyudUIStyle.h`; `tools/ui_map.py` →
`textures/ui/map_world.jpg`, `textures/ui/binoculars_mask.png`, `Content/UI/map.json`; `unreal/scripts/import_ui_assets.py` →
`/Game/UI/T_WorldMap`, `/Game/UI/T_Binoculars`. After the world map changes: `python3 tools/ui_map.py` and `python3 tools/ui_map_ink.py`, then both imports
(`import_diary_assets.py` for the diary's).

## Checks

`unreal/scripts/test_ui.py` (offscreen `-game` in Lvl_Scene01, see its docstring): quest events, every page in three languages,
the lamp at night, the binoculars, the shop, a save and a load; QUESTTEST / SHOPTEST lines and screenshots. Dev commands:
`Screen.Open inventory|journal|diary|map [place]|none`, `Quest.Event <event> [target] [n]`, `Quest.Start|Advance <id>`,
`Quest.Dump`, `Quest.Reset`, `Inventory.Wear <id>` / `-<slot>`, `Screen.Toast <key>`, `lyud.BinocularsHold 1`.
