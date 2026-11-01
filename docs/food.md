# Street food and eating

What you can eat in Lyudmilovsk and how: Adyghe cheese from a tent by the club's door, boiled corn from a cart in the
Academgorodok's square, and the one way of eating shared by every food, the diner's meals and the cigarettes.

## Eating: the thing in the hand (ULyudHandUse)

`Public/Food/LyudHandUse.h`. The hero holds one thing in the right hand and takes it to the mouth with the left mouse button:

| Input | What happens |
|---|---|
| click | the hand to the lips, a **small** use (a bite, a short puff), back down |
| hold (still down when the hand reaches the lips) | a **big** use (a bigger bite torn off, a long drag) |
| keep holding | it stays at the lips and is used on and on (**steady**, every `SteadyEvery` s) until the button goes up |
| Q | put it away |

Walking and looking stay free while it is carried. The hand is the reach component's override hold (exact in the look's
frame); the thing rides on the `hand_r` bone. While a reach move (buying at a machine) or a screen is up the thing is hidden
and waits; opening the diary puts it away. One thing at the mouth at a time: a meal at a table (`ULyudBiteSubsystem`, the
diner's fork, spoon and cup meals in `LyudBite.h`) and a thing in the hand end each other.

```cpp
FLyudHandThing T;
T.Actor = Spawned;                 // the caller spawns it
T.Item = TEXT("cigarette");
T.InPalm = ...;                    // the actor in the palm's frame: X the fingers, Z out of the palm, +Y the little finger's side
T.MouthFull = ...; T.MouthEmpty = ...;   // the actor's point that goes to the lips, whole and at the end
T.Carry = ...; T.Mouth = ...;      // FHeroHoldPose: carried, and the palm's turn at the lips (its Pos is computed)
T.Small = 0.08f; T.Big = 0.18f; T.SteadyEach = 0.06f; T.SteadyEvery = 0.8f;
T.OnUse = [](ELyudMouthUse Kind, float Took, float Left) { /* sound, bite marks, smoke */ };
T.OnUsedUp = [] { /* pocket */ };  T.OnPutAway = [](float Left) { /* keep the rest */ };
ULyudHandUse::Get(this)->Hold(MoveTemp(T));
```

Also `OnAtLips(dt)` (every frame at the lips: a cigarette's ember), `Push` (cm into the mouth at a use), timings, `Prompt`.
Delegate `OnUsed(Item, Kind, Left)`. Console: `lyud.Hand.Press 1|0`, `lyud.Hand.Click`, `lyud.Hand.PutAway` (logs the state),
`lyud.HandUse 0` (the button does nothing). Log lines `HANDUSE hold|use|usedup|putaway`.

## Food in the hand (ULyudFoodSubsystem)

`Public/Food/LyudFood.h`. Any `Food` item: the inventory's "Взять в руку" (or `Use`) equips it, and the food comes up out of the
pocket into the hand by itself; eaten up, the hand is empty again. A food begun and put away keeps what is left
(`ULyudInventory::Started`, saved). Every bite plays `food_bite_<texture>`, then chewing for a while (`chew_seconds` of the
whole thing, spread over the bites) and a swallow.

`Content/Food/food.json`: per food its bite stages (`stages`: meshes `/Game/Food/SM_Food_<id>_<k>`), where the bites run
(`mouth_full`, `mouth_empty`), the palm's turn at the mouth if not across it (`mouth_fingers`, `mouth_palm`: a braid eaten
from its end), the bite sizes, the bite sound's texture, chewing, `wrapped`. Foods without stages (chips, an apple) are held
by their table look and shrink as they are eaten.

Buying: what is bought on a trade screen is handed over when it shuts (`ULyudInventoryUI::OnShopBought`): it appears across
the counter and the hand takes it into the pocket. `lyud.Food.Hold <id>`, `lyud.Food.HandOver <id>` for checks.

## The stalls

`Content/Food/stalls.json` (where, which way the front looks, the seller's and customers' spots) ->
`tools/people_map.py` (`build_stalls`: places of kind `cheese_tent` / `corn_stall` in `people_places.json`, and
`Content/Food/stalls_built.json`) -> `ULyudStallSubsystem` spawns each stall's mesh on the ground at play start (worlds with
an `ALyudSky`; `lyud.Food.Stalls 0` leaves them out). Not in the level: a scene rebuild doesn't need to know them.

| Stall | Where | Seller | Hours | Goods |
|---|---|---|---|---|
| Adyghe cheese tent (red and white) | 7.8 m east of the path, 11.5 m out of the club's door, front to the path | Сусанна Хуажева (54), `work_cheese` | 10–20, closed Mondays | braid 25, small round 40, big round 110 |
| Boiled corn cart (yellow umbrella) | the Academgorodok's square, 14.5 m north of the fountain, front to it | Валентина Гусева (67), `work_corn` | 11–18 daily | corn 15 |

Sellers: `people.json` (bodies `Khuazheva_susanna`, `Guseva_valentina` don't exist yet: they are shown by the `Girl_a` and
`Girl_b` models until generated), `activities.json`, `counters.json` (T trades while they are at work), lines in the strings
and `npc_dialogue.txt`.

Note: `people_map.py` rewrites the whole `people_places.json`; with a stale `--cache` the flats move by centimetres. To add
only the stalls, keep the committed file and append the two places.

## Assets

* `blender/food/food_atlas.py` (plain Python, numpy + PIL): the kit's 2048 atlas `T_FoodKit_{C,N,R}` painted from noise
  (kernels, cob, napkin, cheese rind with the basket's weave, curd, smoked skin, fibre, tent canvas, wood, aluminium,
  oilcloth, the hand-lettered signs in Caveat, umbrella, wicker, steel).
* `blender/food/build_food.py`: the bite stages, authored in the palm's frame (Blender x the fingers, z out of the palm, +y
  the thumb's side): corn (kernels gone row by row on the side toward the mouth, stubs at the edge, the core showing), braid
  (shorter from the tip, fibre caps), small and big rounds (crescent bites, exact booleans, sharp edges kept).
* `blender/food/build_stalls.py`: the tent (frame, gable canopy, scalloped valance, back and side walls, a trestle counter
  under oilcloth, baskets of rounds, a heap of braids, braids hanging off the eave, the sign) and the cart (green enamel, two
  wheels, a pot of cobs with its lid leaning on it, a tray, napkins, salt, a beach umbrella, the sign, a stool).
  Both lint clean (`kit.lint`: no z-fighting, no faces inside faces, no bad uvs).
* `unreal/scripts/import_food.sh`: textures and `M_FoodKit`/`MI_FoodKit`, `/Game/Food/SM_Food_*`,
  `/Game/Food/Stalls/SM_Stall_*` (collision from their triangles), the sounds, the icons. Re-saves every icon and music
  asset: revert the unchanged `.uasset`s before committing.
* Sounds: `tools/food_sounds.py`, procedural (resonant noise bursts, a rubbery chirp for fresh cheese, soft squelches for
  chewing) -> `audio/sfx/food` -> `/Game/Audio/Food/S_food_*`. ElevenLabs takes could replace them later (audio/sounds.json).
* Icons: one Higgsfield 2x2 grid (`design/food/icons_grid.*`, 1 credit), cut by `tools/food_icons.py`; diary sketches by
  `tools/ui_diary_icons.py`.

## Test

`sh unreal/scripts/test_food.sh`: the tent and its trade screen, a buy handed over, a click / hold / hold-on for every food,
the corn cart. FOODTEST/HANDUSE lines, shots in `Saved/FoodTest`.
