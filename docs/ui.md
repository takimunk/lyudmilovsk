# The game's interface

One game screen with three pages, a HUD over the game, and the prompt line for things you can use. All of it is Slate in C++
(`Source/Lyudmilovsk/Private/UI`), every word a key in the string tables (the `localization` skill), ru / en / ja.

## Keys

| Key | What |
|---|---|
| Tab or I | inventory (again: close) |
| J | journal |
| M | map; on the map the wheel zooms at the cursor, dragging moves it, C finds you |
| Esc | closes the screen (F10 in the editor opens the game menu, which closes it too) |
| L | head lamp on and off (when it is worn) |
| right mouse button, held | binoculars (when they are worn) |

The screen does not pause the game: walking and looking stop and the mouse shows (as the inventory always did). The journal and
the map open as a bird too; the inventory needs the hero. Keys are polled, so nothing else loses a key.

## Pages

- **Inventory**: on the left what the hero wears, one row per slot (head, neck, outerwear, body, legs, feet, in the hand); on the
  right what is in the pockets, the chosen thing with its icon, description and what can be done with it (put on / take off, take
  in the hand, drink, eat). The money and the game date are in the header of every page.
- **Journal**: the quests (current ones, main first, then the finished ones) with the story so far and what to do now (with
  "follow" and "show on the map"); the diary tab lists every entry by day, newest first.
- **Map**: the painted map of the whole outdoor world at real scale (`tools/ui_map.py` paints it from `tools/world_map.py`; no words
  in the picture), the names of the places found (others a faint dot), diamonds for the quests' next steps (the followed quest's
  pulses), the player as a red arrow, a scale bar and north. Beside it: the nearest place and the list of places found.

## HUD

Top right: the date and time (`UGameCalendar`), the followed quest and its next step. Top left: short notes for a few seconds
(new quest, quest updated or done, new place, an item got). Hidden while the screen, the menu or a trade is open; `lyud.Hud 0`
hides it. Prompts ("[E] ...") go through `ULyudInventoryUI::SetPrompt`, never on-screen debug text.

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
binoculars), `Private/UI/SLyudGameScreen.*`, `SLyudMapView.*`, `SLyudHud.*`, `LyudUIStyle.h`; `tools/ui_map.py` →
`textures/ui/map_world.jpg`, `textures/ui/binoculars_mask.png`, `Content/UI/map.json`; `unreal/scripts/import_ui_assets.py` →
`/Game/UI/T_WorldMap`, `/Game/UI/T_Binoculars`. After the world map changes: `python3 tools/ui_map.py`, then the import.

## Checks

`unreal/scripts/test_ui.py` (offscreen `-game` in Lvl_Scene01, see its docstring): quest events, every page in three languages,
the lamp at night, the binoculars, the shop, a save and a load; QUESTTEST / SHOPTEST lines and screenshots. Dev commands:
`Screen.Open inventory|journal|diary|map [place]|none`, `Quest.Event <event> [target] [n]`, `Quest.Start|Advance <id>`,
`Quest.Dump`, `Inventory.Wear <id>` / `-<slot>`, `Screen.Toast <key>`, `lyud.BinocularsHold 1`.
