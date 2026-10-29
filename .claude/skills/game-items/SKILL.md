---
name: game-items
description: How to add an in-game item (anything the hero can pick up, buy, carry, drink, eat or feed) to Lyudmilovsk. Use whenever you create a new item, add something to a shop or vending machine, or make something the hero can take. Every item gets an icon.
---

# Adding an item

**Standing rule: every in-game item has an inventory icon, made when the item is created.** No icon, no item.

1. **Item table**: add an `FLyudItemDef` in `LyudInventory.cpp` (`MakeDefs`): id (`snake_case`), kind, shape (`Radius`, `HalfLength`: what the hand closes on), what it leaves behind. For goods use the `Good(...)` helper: `Look` (pack, flat, cup, glass, bottle, cone), `Size` cm, `Tint`, usual `Price`; `ALyudGoodsItem` builds the held/lying object from it with the icon as its picture (a labelled soda bottle keeps `Flavor` and `ABottleItem`). Something to wear: kind `Wear` and its `Slot` (the inventory page puts it on; what it does goes in `ULyudGameScreen::UpdateWear`, docs/ui.md).
2. **Words**: `item.<id>.name` and `item.<id>.desc` in all three `strings_*.json` (see the `localization` skill).
3. **Icon**: add `icon_<id>` to `localization/assets.json` (kind `icon`, aspect `1:1`, a prompt in the same words style as the other icons; words on the object go in as `<<key>>`), run `python3 tools/gen_localized.py --lang ru icon_<id>` (`FAL_API_KEY`; key in `~/projects/asmrx3000/.env`), look at it, then `unreal/scripts/import_hall_assets.py` (imports it as `/Game/Inventory/Localized/<lang>/T_Icon_<id>`). Commit the jpg, its json and the uasset. Then its diary sketch (the inventory is the hero's diary, docs/diary.md): `python3 tools/ui_diary_icons.py <id>` → `textures/localized/ru/sketch_<id>.png`, look at it, `unreal/scripts/import_diary_assets.py` (`T_Sketch_<id>`).
4. **In the world**: an actor for the held/lying item with `MakeReachItem()` (see `ABottleItem`, `ABirdFeedBag`), taken with `UHeroReachComponent::Take/Pick`, put in the pocket with `ULyudInventory::Get(this)->Add(id)`; a new interactable shows its prompt through `ULyudInventoryUI::SetPrompt`.
5. **Selling it**: add it to a stock in `Content/Transport/lobby.json` (`stock.<preset>`: item, price, count; the universal `AVendingMachine` takes any item of the table, docs/transport.md "Lobbies") or give a machine `Slots`; or a trader (`ABoothOwner` sells at the window with T, `ULyudInventoryUI::OpenShop`).
6. **Quests** see it by itself: `get`, `use`, `wear` events with the item id (docs/ui.md).
7. **Check**: `python3 tools/check_localization.py` fails if an `item.<id>.name` has no `icon_<id>` asset or picture.
