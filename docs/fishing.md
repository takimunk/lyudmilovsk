# Fishing

A telescopic float rod with a spinning reel, worms, maggots and a spoon, bought at the Gastronom No. 7 counter (Orlova Tanya:
rod 90 roubles, worms 15 for ten, maggots 10 for ten, a spoon 45). The fish caught go into the pocket as `fish_<species>` and
are sold back at the same counter (Y: «продать улов»), priced by weight.

## Playing

Tab → the rod → «Взять в руку»: the rod comes up in the right hand. A bait taken in the hand while the rod is out goes on the
rig; X changes to the next bait in the pocket. Then, standing at the sea (the pier, the promenade's edge, the beach):

| | |
|---|---|
| LMB hold, let go | the cast: the rod goes back while the button is held (1.1 s to a full swing), forward on release. A float flies to ~18 m, a spoon to ~30 m. On land: wound back («Упало на берег…»). |
| float | wait. The float twitches (nibbles: shy fish nibble more), then goes under: LMB strikes. Too early or too late: the fish is gone, sometimes the bait too. |
| spoon | LMB held winds it in; a hit while winding hooks itself half of the time, else LMB strikes. |
| a fish on | LMB held winds the reel (the rod bends, the tension climbs); let go and the clutch gives line. Wind through a strong run and the line breaks (the fish and the hook gone, a spoon lost); leave it slack over ~2 s and the hook may come out. At 1.8 m it is lifted out, shown in the hand for a moment and goes into the pocket. |
| RMB | wind the line in (gives up a wait) |
| Q | the rod away (only with the line in) |

With the rod in the hand the right mouse button is the rod's, not the binoculars'. The prompt at the bottom says what to do now.
The first of a species and every record writes a line to the screen; the angler's log is saved.

## What bites

Not a table of its own: the food web (`ULyudEcosystem::GetBiteWeights`, docs/ecosystem.md) says how much of each catchable fish
lives where the float lies, refreshed every 3 s. Each weight is multiplied by the bait's liking for that fish (by diet: plankton
eaters take maggots, bottom fish worms, hunters the spoon; small mouths take little) and by the root of its mean school size.
Bites come at `1/BiteEvery` (100 s) at their ceiling, half of it at a summed weight of `RichWeight` (0.8): a rich spot with a liked
bait gives about one bite a minute, a bare hook almost nothing. A fish landed is taken out of the food web (`ReportCatch`) and
out of the water round the diver (`ULyudUnderwaterSubsystem::TakeFish`).

26 species (`Private/Fishing/FishingModel.cpp`, `FCatchable`): herring, sprat, sandeel, sticklebacks, perch, cod, sea trout,
salmon, garfish, gobies, eelpout, sculpins, lumpsucker, flounder, turbot, smelt, twaite shad, whitefish, roach, vimba, eel, rock
gunnel. Pipefish take nothing. A catch is 0.6..1.3 of the adult length, rarely larger; weight is Fulton's `K * L(cm)^3 / 100` g.
Its pull, stamina and run speed follow from the weight and the species' fight (a sea trout runs and jumps, a flounder is a heavy
plate).

## How it is built

- **Model** (`Private/Fishing/FishingModel.h/.cpp`, plain C++, no Unreal types): the fish and the baits, `RollFish`, `PriceOf`,
  and `FSession`, one rod at the water as a state machine (Idle, Charging, Swing, Flying, Waiting, Nibbling, Taking, Fighting,
  Landing, Winding) with events for the caller's sounds and words. Seeded random: the same seed, the same session.
- **Game** (`Private/Fishing/LyudFishing.cpp`, `ULyudFishing`, tickable world subsystem): equips the rod from the inventory,
  baits the hook from the pocket, flies the float along an arc (traced, so it can hit the pier or land ashore), keeps the water
  test (`ALyudSea::DepthAt` > 25 cm), poses the hand (`UHeroReachComponent::SetOverrideHold`: charge-back, swing, the strike's
  jerk, the fight, the landing, the fish shown), plays the sounds and the prompt.
- **Save** (`ULyudFishingState`, key `Fishing`): the bait on the rig and whether the hook is baited, the log (count, biggest by
  grams and cm, the day of the first), the total, and the grams of each species in the pocket (for the sale price).
- **Rod** (`Private/Fishing/FishingRod.cpp`, `ALyudFishingRod`): built in code from lathed profiles (cork grips, reel seat, reel
  with spool and turning handle, 8 blank segments on joints that bend toward the line's pull, guides), the line as one instanced
  mesh of 24 pieces (a sagging curve when slack, straight under water), the float, the spoon and hook, and the fish on the hook
  (its swimming mesh `/Game/Sea/Fish/SM_<Id>` with the fish material's wriggle, or the sea-life stand-in). Perf: instance
  transforms are updated in place (`bMarkRenderStateDirty = false`), nothing collides, nothing goes into Lumen or distance
  fields, no allocation per frame. Meshes from blender/props/fishing can replace the stand-ins later.
- **Shop** (`Content/PeopleData/counters.json`, `CounterTrade`): the goods on Orlova Tanya's counter and `"buys_fish": true`,
  which adds the sell prompt (`ui.shop.prompt_sell_fish`) and Y.
- **Items** (`LyudInventory.cpp`): `fishing_rod`, `bait_worm`, `bait_maggot`, `lure_spoon` (hand items), and one `fish_<id>` per
  species. Names and descriptions in all three string tables (`item.*`, `ui.fish.*`).
- **Icons**: four Higgsfield grid sheets (`design/fishing/icons_grid_*.json`, prompts and job ids pinned), cut by
  `tools/fishing_icons.py` into `textures/localized/ru/icon_<id>.jpg` (no words on them: one picture for every language), listed
  in `localization/assets.json`.
- **Sounds** (`audio/sounds.json`, group `fishing`): rod swish, float plop, reel click, the clutch's drag, the line snapping,
  splash, flapping, into the bag. Generated with `tools/gen_sounds.py`, imported as `/Game/Audio/Fishing/S_<id>_<nn>`.

## Checks

- `tools/fishing/run.sh`: the model outside the engine. Bot anglers (good, greedy, lazy, jumpy) at spots weighted like
  `GetBiteWeights`: bite rates, baits matching diets, sizes and prices, a good angler landing ~25-35 fish an hour, greedy reeling
  snapping the line on big fish, a salmon landed about 3 times in 5 with good play, the cost of a step.
- Console: `lyud.Fish.Give` (rod and baits), `lyud.Fish.State`, `lyud.Fish.Press 1|0`, `lyud.Fish.Click`, `lyud.Fish.Bite`
  (a take now), `lyud.Fish.Hook <Species>` (one on the line now), `lyud.Fish.Bait <item>`, `lyud.Fish.Log`. Log lines begin
  `FISHING`.
