# The kitchen: cook what you grow and catch

Backlog item 34 (docs/interactives_backlog.md). It ties the allotment (docs/allotment.md), fishing (docs/fishing.md) and
food (docs/food.md): the hero cooks at a stove of the town from his own harvest and catch, eats the dish, gives it to a
friend, takes his own jar of jam to the market's contest and sells fresh vegetables to the restaurant's kitchen.

## Where the code and data live

| Part | Where |
|---|---|
| The rules (engine-free C++) | `Source/Lyudmilovsk/Private/Kitchen/KitchenModel.{h,cpp}` (namespace `LyudKitchen`): recipes, the pot, the vegetables' sale |
| The game side | `Public/Kitchen/LyudKitchen.h`, `Private/Kitchen/LyudKitchen.cpp` (`ULyudKitchen`, a tickable world subsystem, saved as "Kitchen") |
| Offline test | `tools/kitchensim/run.sh test` (every recipe against the item table, food.json, icons and the three string tables; the pot's timing and quality; the sale); `recipes` prints the table |
| Words ru/en/ja | `tools/city/content/strings/kitchen.json` (items, `ui.cook.*`, `cook.diary.*`, the cook's talk, the quest), built by `python3 tools/city/interiors.py` |
| Town content | `tools/city/content/x_kitchen.json`: act `hot_plate`, collection `cook`, the canteen cook's topics, quest `own_supper` |
| Items | `LyudInventory.cpp` (`dish_ukha`, `dish_fried_potatoes`, `dish_fried_fish`, `dish_salad`), `Content/Food/food.json`, icons in `localization/assets.json` |

## Playing it

**Y** is the kitchen's key (E stays the thing's own: the canteen's counter still sells with E).

- **At a stove**, Y opens the recipes' page: each line says what it takes and what the pockets lack. A digit puts the dish
  on the fire: the ingredients leave the pockets ("fish" is the biggest fish of the catch).
- **On the fire** the pot asks for a hand once to three times (the prompt line and a hiss: skim the froth, stir, turn the
  fish, salt): Y within five seconds. When it is done, Y takes it off. He may walk about meanwhile, but a pot that
  calls while he is away gets a toast only.
- **What comes of it:** not a slip: "splendid", and a portion more where the recipe has one; a slip or two: good or
  edible; left ten seconds after it is done: overdone (edible); fourteen more: burnt, nothing but a stamp and a diary line.
- A stove's hint ("Y: to the stove") is a toast once an approach, and only when the pockets hold a whole recipe.

| Recipe | Takes | Gives | On the fire | Steps |
|---|---|---|---|---|
| Ukha | a fish, 2 potatoes, 1 dill | 2 bowls (+1) | 40 s | skim, salt |
| Fried potatoes with dill | 3 potatoes, 1 dill | 1 pan (+1) | 30 s | stir, stir |
| Fried fish | a fish | 1 plate | 24 s | turn |
| Salad from the bed | 2 tomatoes, 2 radishes, 1 dill | 1 bowl (+1) | 8 s (no fire) | salt |
| Strawberry jam | 6 strawberries | 1 jar | 45 s, a stove only | skim, stir, skim |

The stoves are the town's own things, found by their acts (`LyudKitchenImpl::StoveActs`); nothing is placed or drawn:

| Where | Thing | Fire | Hours |
|---|---|---|---|
| The commune houses' canteens (P-T2H2, P-T3H3) | the counter (the cook lets him to the second ring) | stove | 8–19 |
| The sanatorium's dining room | the buffet (the kitchen behind it) | stove | 7–21 |
| The town hall's kitchen, Bauer's and the Unité flat's stoves | the stove | stove | any |
| The lighthouse keeper's room, the cube houses' living rooms | the ring | hot plate (no jam) | any |
| A room of his own | any thing with `what=hot_plate` gets the act `hot_plate` (`"open": "cook"`): E opens the page | hot plate | any |

The hero's own room (backlog item 19, branch `hero-room`) only has to give one of its things `"props": {"what":
"hot_plate"}`.

## What the dishes are for

- **Food:** each is a `Food` item, eaten by hand like the rest (`ULyudFoodSubsystem`, food.json).
- **Gifts:** `tools/friends/content.py` GIFTS lists the four dishes by the tastes that like them (the jam was there).
- **The jam contest:** a jar he cooked himself sets the garden's jar quality (`ULyudGarden::OwnJam`) to the worse of the
  berries' (his last picking, `HarvestQuality`) and the cooking's, so the market's Sunday contest judges his own jam.
- **The restaurant:** the «Янтарь»'s kitchen door (docs/fishing.md) takes vegetables and berries with the fish, Y there:
  12 things a day, a little over the market scales' best price (strawberries 9, tomatoes 8, dill 6, potatoes 5, radish 5).
- **Collection `cook`** («Поварская книжка», given by the engine): `ukha` `fried_potatoes` `fried_fish` `salad` `jam`
  `fine` (a dish without a slip) `burnt` `veg` (sold to the restaurant).
- **Quest `own_supper`** («Ужин своими руками»): ask the canteen's cook about her stove, cook a dish, cook three more,
  cook one without a slip and let her taste it (topic `canteen.taste`, offered with the stamp `cook/fine`).
  Quest events: `cook dish` (any), `cook more` (any after his first) and `cook <recipe id>`.

## Performance

Nothing is spawned or drawn. The subsystem looks for the nearest stove four times a second over a handful of points, steps
the pot once a frame (a few comparisons) and builds strings only when the prompt changes.

## Developer mode

F7 group «Кухня»: `kitchen.kit` (ingredients for all five dishes), `kitchen.cook` (what is on the fire is done at once
without a slip; nothing on: cooks the first dish the pockets allow). Console: `lyud.Kitchen.Info`, `lyud.Kitchen.Open`
(the page anywhere), `lyud.Kitchen.Cook <recipe>`, `lyud.Kitchen.Finish`, `lyud.Kitchen.Kit`.

## Not done yet

- The first Mac build of the engine side, a shot of the recipes' page at the commune canteen and of a pot on the fire.
- The four icons: the 2×2 Higgsfield grid is generated and pinned (`localization/icon_grids_kitchen.json`); on the Mac run
  `python3 tools/garden/icons.py --plan localization/icon_grids_kitchen.json`, look at each cell, import, diary sketches.
- A frying sound: `pan_sizzle` has no pool in the soundscape yet (frying is silent, the boil uses `steam_hiss`).
- No pot or pan is shown on the stove and the hero's hands do not move: the cooking is the prompt line, the sounds and
  the result. The pot is not saved (a load mid-cooking loses what was in it).
- No stove stands in the canteens' or the sanatorium's rooms: the counter is the way to the kitchen behind it.
