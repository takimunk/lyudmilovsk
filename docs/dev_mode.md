# Developer mode

Everything needed to check the game quickly: jump anywhere, play any quest from any step, fill the pockets, become any bird,
change the time and the weather, and run any feature's shortcuts. One panel (F7) and the `Dev.*` console commands do the same
things, so whatever Dima clicks a test script can type.

Code: `Public/Dev/LyudDev.h` (the registry and the API), `Private/Dev/LyudDev.cpp` (teleports, quest jumps, built-in actions,
console commands), `Private/Dev/LyudDevPanel.cpp` (the panel's tabs), `Private/Plants/PlantDevTool.cpp` (the panel itself and the
World tab). Data: `Content/Dev/teleports/*.json`, the `dev` blocks in `Content/UI/quests.json`. Words: `ui.dev.*`, `dev.*` in the
string tables.

## The panel (F7)

F7 (fn+F7 on a Mac keyboard) opens it over the game; the game keeps running and keeps its keyboard. F7 again or "Close" hides it.
H shows the game's keys. F10 is the game menu. F8 is the editor's eject and stays free.

| Tab | What it does |
|---|---|
| World | date, time of day, time speed; the weather (presets, weather types, phenomena, values, Auto); one tree's age and season |
| Teleport | every teleport point by group, with a search box; "Bookmark this spot", "Copy position", "Fly through walls", "Reload". The line under the buttons is where you stand (world cm, map metres) |
| Quests | every quest in quests.json: its state, a button per stage (jumps there, the current one is lit), a ✓ button per objective of the current stage (fires the real game event), "Forget" |
| Pocket | money (+100, +1000, to zero), "One of everything", "Pocket as at the start", "Held only"; every item with +1, +10, −, Wear/Hold |
| Birds | every species: Become (on the hero's hand), Fly (in the air), Nearby (one calm bird beside you), Befriend; back to the hero, feed ×20, all species into the bird book |
| Actions | a console line (any command, Enter; the last six to click again), then every registered action by group. Lit buttons are toggles that are on |

Developer mode is always there outside Shipping builds. A Shipping build has it only with `-dev` on the command line.

## Console

| Command | |
|---|---|
| `Dev.Tp <id>` | to a teleport point |
| `Dev.TpAt <x> <y> [z] [yaw]` | to a world point in cm; without z onto the ground |
| `Dev.TpMap <x> <y> [yaw]` | to a map point in metres (docs/world_map.md frame), onto the ground |
| `Dev.TpList [group]` | logs the points (`DEVTP` lines) |
| `Dev.Where` | logs where you stand as a `Dev.TpAt` line plus map metres, and copies it to the clipboard (paste it into a bug report) |
| `Dev.Bookmark [name]`, `Dev.Bookmark remove <name>` | your own points, kept in `Saved/Dev/bookmarks.json` (not in git) |
| `Dev.Quest <quest> <stage id or number>` | jump to a stage |
| `Dev.Quest <quest> done <objective number>` | complete one objective of the current stage by its real event |
| `Dev.Quest <quest> forget` | the quest as if never begun |
| `Dev.Do <action id>`, `Dev.List` | run an action, list them (`DEVACTION` lines) |
| `Dev.Reload` | re-read the teleport files, quests.json and map.json |
| `Dev.Panel open/close/clickkey <key>` | the panel without a mouse (headless tests, `unreal/scripts/test_devpanel.sh`) |

The older per-feature commands stay (`Quest.*`, `Inventory.*`, `Bird.*`, `Birds.*`, `Transport.Goto`, `lyud.Weather.*`, ...); the panel
calls many of them.

## Hooking a feature in (every thread does this)

A feature is not done until it can be reached and tested from developer mode. For anything you add:

1. **A place**: add its teleport points to your own file `Content/Dev/teleports/<feature>.json` (one file per feature, so branches never
   conflict):

   ```json
   {"teleports": [
     {"id": "north_hut", "group": "north", "name": "dev.tp.north_hut", "x": 12.0, "y": 2400.0, "yaw": 180},
     {"id": "library_hall", "group": "interiors", "name": "dev.tp.library_hall", "x": -310.0, "y": 905.0, "z": 120, "yaw": 90},
     {"id": "ural_bank", "group": "topi", "name": "dev.tp.ural_bank", "place": "ural", "yaw": 90, "time": "23:00"}
   ]}
   ```

   `x`, `y`: map metres in the docs/world_map.md frame (X west, Y north, from the club's portal face), the same numbers as
   `Content/UI/map.json`; or `place`: a map.json id. `z`: world cm; leave it out outdoors (the point is dropped onto whatever is solid
   below, or onto the sea's surface), give it indoors (else you land on the roof). `yaw`: degrees, 0 looks west (+X), 90 north. `time`:
   sets the clock on arrival (night-only things). `name`: a string key in all three tables; `group` names a `dev.group.<group>` key.
   Places in map.json are listed on their own. `Dev.Where` gives you the numbers of where you stand.

   A point that moves (a station, a bike, an NPC) is registered in code:

   ```cpp
   #include "Dev/LyudDev.h"
   static LyudDev::FAutoTeleport GOrniTp(TEXT("orni_now"), TEXT("ornithologist"), TEXT("dev.tp.orni_now"),
       [](UWorld* W, FTransform& Out) { /* find him; Out = in front of him, facing him */ return true; });
   ```

2. **A quest**: stages need nothing extra to be jumped to. Give a stage a `dev` block in quests.json when a jump straight into it
   would otherwise leave the hero without what that stage expects (ignored by the game itself):

   ```json
   {"id": "lure", "objectives": [...], "dev": {"tp": "orni_far_meadow", "give": {"bird_feed": 10, "bird_feeder": 1}, "time": "06:00",
     "exec": ["lyud.Weather.Preset clear"]}}
   ```

   `give` tops the pocket up to those counts. Anything that needs code (a feeder already standing, a bird already tame) goes in a
   stage setup, run after the block:

   ```cpp
   static LyudDev::FAutoQuestSetup GLure(TEXT("ornithologist"), TEXT("lure"), [](UWorld* W) { ... });
   ```

   Then play the quest once through with the panel: each stage button, each ✓.

3. **A shortcut** (open a shop, start an eruption, light the Ural, skip a wait): an action, a button in the Actions tab and `Dev.Do <id>`:

   ```cpp
   static LyudDev::FAutoAction GErupt(TEXT("north.erupt"), TEXT("north"), TEXT("dev.action.north.erupt"),
       [](UWorld* W) { /* ... */ });
   // a toggle: give its state too, the button lights while it is on
   static LyudDev::FAutoAction GGlow(TEXT("topi.glow"), TEXT("topi"), TEXT("dev.action.topi.glow"),
       [](UWorld* W) { bGlow = !bGlow; }, [](UWorld* W) { return bGlow; });
   ```

   Ids are `<group>.<what>`, lowercase. Put the registrar at file scope in the feature's own .cpp (static registration, like
   `FAutoConsoleCommand`: no init order to mind). Keep the action itself cheap and safe to press twice.

4. **Words**: `dev.tp.<id>`, `dev.action.<id>`, `dev.group.<group>` in `strings_ru/en/ja.json` (localization skill). A missing key
   shows the raw id, which is fine while working but not for merging.

5. **A system with its own controls** (like the weather) can add a whole section; talk to the dev mode's owner first, and keep it in
   its own file like `PlantDevWeather.cpp`.

## Rules

- No new F-keys for debugging: register an action instead. F7 is the panel, F10 the menu, F8 the editor's eject.
- Dev actions never run on their own: nothing in developer mode ticks unless switched on (fly is the only ticker, while flying).
- Everything a dev action changes is ordinary game state (the pocket, the quests, the clock) and gets saved like it would be in play;
  use "Save (Dev)" / "Load the latest" or a fresh game to go back.
- Logs: every action prints a `DEV ...` line (`DEV tp`, `DEV do`, `DEV quest`, `DEV objective`) so test scripts can grep them.
- Tests: `Dev.*` commands work in `-ExecCmds` and in headless runs; prefer them over simulated key presses.
