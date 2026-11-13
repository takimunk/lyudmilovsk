---
name: dev-mode
description: How to hook a Lyudmilovsk feature into developer mode (F7 panel, Dev.* console) - teleport points, quest stage jumps and setups, dev actions. Use whenever you add a place, a building or interior, a region, a quest or quest stage, an NPC, or any mechanic that needs a shortcut to test.
---

# Developer mode hooks

Full guide: `docs/dev_mode.md`. API: `unreal/Lyudmilovsk/Source/Lyudmilovsk/Public/Dev/LyudDev.h`.
A feature is not done until Dima can reach and test it from the F7 panel.

## Checklist for every feature

1. **Places** → your own file `unreal/Lyudmilovsk/Content/Dev/teleports/<feature>.json` (never edit another feature's file):
   `{"teleports": [{"id": "north_hut", "group": "north", "name": "dev.tp.north_hut", "x": 12, "y": 2400, "yaw": 180}]}`
   - `x`, `y` map metres (docs/world_map.md frame, as in Content/UI/map.json) or `"place": "<map.json id>"`.
   - `z` world cm: omit outdoors (dropped onto the ground or sea surface), give it indoors (else you land on the roof).
   - `yaw` 0 = west (+X), 90 = north. `time` "HH:MM" sets the clock on arrival.
   - Get the numbers in game with `Dev.Where` (copies a `Dev.TpAt` line + map metres).
   - A moving point (NPC, vehicle): `static LyudDev::FAutoTeleport G(TEXT("id"), TEXT("group"), TEXT("dev.tp.id"), [](UWorld* W, FTransform& Out) { ...; return true; });`
2. **Quests** → every stage is jumpable automatically. Add a `"dev"` block to a stage in `Content/UI/quests.json` when jumping
   straight in would leave the hero without what it needs: `"dev": {"tp": "<teleport id>", "give": {"bird_feed": 10}, "time": "06:00", "exec": ["console cmd"]}`.
   Code-only setup: `static LyudDev::FAutoQuestSetup G(TEXT("quest"), TEXT("stage"), [](UWorld* W) { ... });`.
   Then play it through in the panel's Quests tab (stage buttons, ✓ per objective) or `Dev.Quest <quest> <stage>` / `Dev.Quest <quest> done <n>`.
3. **Shortcuts** (start an event, open a shop, force night glow): `static LyudDev::FAutoAction G(TEXT("group.what"), TEXT("group"), TEXT("dev.action.group.what"), [](UWorld* W) { ... }[, [](UWorld* W) { return bOn; }]);`
   Shows in the Actions tab and as `Dev.Do group.what`. Cheap, idempotent, no ticking.
4. **Words** → `dev.tp.<id>`, `dev.action.<id>`, `dev.group.<group>` in strings_ru/en/ja.json (localization skill).
5. **Verify** in a headless run: `-ExecCmds="Dev.TpList <group>, Dev.Tp <id>, Dev.Do <id>"` and grep `DEV ` lines in the log.

## Never
- Bind a new F-key or debug key for testing (F7 panel, F10 menu, F8 editor eject are taken): register an action.
- Edit `Private/Dev/*` or the panel for your feature's buttons: the registry covers it. Bigger custom sections: ask the dev-mode thread.
