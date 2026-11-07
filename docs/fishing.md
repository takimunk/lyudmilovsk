# Fishing

Rods, baits and lures bought at the Gastronom No. 7 counter (Orlova Tanya): the telescopic float rod 90 roubles, the spinning
rod 220, the sea bottom rod 380; worms 15 for ten, maggots 10 for ten, shrimp 30 for ten, dough 8 for ten; a spoon 45, a
wobbler 70; the angler's book 40. The fish caught go into the pocket as
`fish_<species>`; they are sold back at the same counter (Y: «продать улов»), priced by weight, or fried at the diner (Y there:
the cook takes the biggest fish in the pocket for 15 roubles and a plate of fried fish with potatoes comes to the counter), or
sold to the restaurant's kitchen (below).

## The rods

| rod | float / bottom cast | lure cast | strength | reel | |
|---|---|---|---|---|---|
| `fishing_rod` | 18 m float | 30 m | 1 | 1 | the basic one |
| `rod_spinning` | 20 m float | 42 m | 1.7 | 1.35 | a big fish breaks it far less; the one for sea trout and salmon |
| `rod_sea` | 45 m, a 100 g lead, no float | 32 m | 2 | 0.9 | a bottom rig: the bite shows on the tip (it knocks, then bends round; 1.5 x the time to strike); bottom fish x1.4, cod, turbot and eel x1.5, the small fish high in the water x0.25 |

Strength divides the tension a fish makes (the clutch's brake is a force, so a stronger rod's clutch holds more); the reel scales
every winding speed. `tools/fishing/run.sh` fights each with a "slow angler" (who looks at the rod every 0.25 s): a sea trout is
landed 21 % of the time on the float rod, 77 % on the spinning rod; a salmon 0 % / 21 % / 33 %. Any rod taken in the hand comes
out (`ULyudFishing::IsRod`); a bait taken in the hand brings out the rod last used (`ULyudFishingState::Rod`), or the best owned.

## Baits

worms (bottom fish, cod), maggots (small fish, roach, perch), shrimp (`bait_shrimp`: cod, eelpout, flounder, turbot, sea trout),
dough (`bait_dough`: roach and vimba, whitefish now and then), the spoon (the hunters), the wobbler (`lure_wobbler`: sea trout and
salmon 1.3, garfish, perch; 0.85 of the spoon's cast). The likings are `GBaits` and `Override()` in `FishingModel.cpp`.

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

## The angler's book

«Рыбы нашего берега» (`fish_book`, `ULyudFishBookScreen`, `Private/Fishing/LyudFishBook.cpp`): N, or «Открыть книгу» on the diary's
Things page. On the left every fish one can catch here, its icon faded until the first one is caught, the count in pencil; on the
right the chosen fish's page: its picture and description, its usual length, the baits it takes (from the bait likings), what the
Gastronom pays a kilo, and in pencil the hero's own record of it from the angler's log (how many, the record grams and cm, the
day of the first). At the foot: fish caught, kinds of the 26, fish fried at the diner. N or Esc shuts it; the game goes on.

## The journal, the forecast

The book's first page is «Мой журнал»: today's forecast (`ULyudFishing::Forecast`, the strongest reason now: the dusk, a falling
glass, a gale...), the records (the five heaviest kinds' biggest), the last 30 catches of the saved journal
(`ULyudFishingState::Journal`, the last 60: when, the fish, grams, cm, the spot, the bait, the rod) and the kitchen's sales. The
spot is the boat, or the first of `Content/Fishing/fishing.json`'s spots that holds the cast's start (pier head, pier, promenade,
beach; else "the shore"). Console: `lyud.Fish.Journal`, `lyud.Fish.Mood`.

## The pier and the boat

The spots take their places from `Content/City/city_plan.json` (S-PIER-HEAD, P-PIER, S-PROMENADE, S-BEACH) by plan id, so they
follow the city plan; the pier head is deep water (more cod, flounder, turbot, garfish by the food web's suitability). A wooden
rowing boat (`ALyudRowBoat`, built in code: a lofted hull, thwarts, oars) lies moored on the pier's south side, 155 m from its
head (once the sea's depth map is read, moved out along the pier until it floats at spring low water, `LowWaterZ`; the landing moves with it). E close by hires it (30 roubles a day,
paid when taken from the mooring), the hero sits on the middle thwart facing the bow (`AHeroCharacter::SitOn` on the hull, the
seat's `SeatMove` / `SeatLeave` hooks); W/S row (2.2 m/s at most), A/D turn (one oar pulls), the hull rides the waves (the sea's
surface under its ends and sides). It goes only over 70 cm of water and within 600 m of the mooring, its hull's box swept along each move and turn (the pier's piles stop it), and never sinks under the bed (at low water it sits on the sand). Where it lies, the hire and the hero in it are saved (`ULyudFishingState` BoatPos / BoatPaidDay / bBoatAboard); a boat left elsewhere is back at the pier the next day. Space gets out at the pier
(onto the deck, the boat tied up again) or where the water is under 1.1 m deep; out at sea it says to row back. The rod works
from it; its catches are written "from the boat". Console: `lyud.Fish.Boat [board|leave|home]`.

## Time of day, weather, hunger

`GetBiteWeights` already holds who is about (the month, day or night activity). On top, every 3 s the rod reads the sky and the
weather (`FConditions`: the sun's altitude, wind, rain, thunder, cloud, the pressure's tendency; the web's bloom as the water's
murk) and `BiteMood` gives each fish's keenness: the dawn and dusk rise (hunters x1.8, bottom fish x1.2), a bright still noon
puts shy fish off, a falling glass before a front makes the hunters feed (to x1.4), a gale (from 10 m/s) and thunder put them
down, a soft rain helps, a lure is seen less at night and in the bloom's murky water (GetBloom counts the blue-greens: ~2.4 in an August bloom, a lure then 0.8 of a clear day's; the forecast says "murky" over 2). Each fish's hunger comes from the web:
its prey's numbers now against the usual (`ULyudEcosystem::GetAbundance` over its diet), `Hunger` = 1/sqrt(food), 0.6..1.7
(every 30 s). Keenness scales the bite rate even at a rich spot (the numbers saturate, the keenness does not).

## The restaurant's kitchen

`fishing.json` "kitchens": the «Янтарь»'s staff door at the back of its kitchen block (1.5 m outside it, set by the restaurant thread; its cook stands in the doorway while the hero is near). Open 11-23,
close by: «Y — сдать рыбу повару (N шт., X ₽)». It takes only the prized kinds (200 roubles a kilo and up at the Gastronom) of a
good size (0.85 of the usual length, each fish by its own weight: `ULyudFishingState::BagFish`), dearest first, 8 a day, at 1.6 x the Gastronom's price (`LyudFishing::RestaurantPrice`;
`ULyudFishingState::KitchenOffer / SellToKitchen`, the day's count saved). The restaurant's own building and cooks are another
branch's: when it lands, its kitchen can call `SellToKitchen` itself, or the door's place in `fishing.json` moves to its pass.
Console: `lyud.Fish.Kitchen [sell]`.

## Cooking the catch

The diner's waitresses (`counters.json`: `"cooks_fish": 15`) offer «Y — пожарить: <the biggest fish> (15 ₽)» while there is a
fish in the pocket. Y pays, takes that fish out of the pocket (and its grams out of `BagGrams`), and orders `diner_fish` the way a
menu dish is ordered (`ULyudInventoryUI::OnShopBought`): `ALyudDiner` serves it on a plate in front of the hero 5-9 s later, eaten
with the fork like the other dishes (`Content/Food/food.json`). The plate is `SM_Diner_FriedFish` (a whole fish floured and
browned, three potatoes with dill, a lemon wedge) and the fork's load `SM_Diner_Load_Fish`, built by `blender/diner/tableware.py`
like the rest of the diner's food (new materials `DN_FishSkin`, `DN_Potato`, `DN_Lemon`). `ULyudFishingState::Cooked` counts them.

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
  (its swimming mesh `/Game/Sea/Fish/SM_<Id>` with the fish material's wriggle, or the sea-life stand-in). Perf: built once per world
  (put away it is parked hidden, not destroyed), one material per colour (8, not one per part), the hooked fish's look loaded
  in the background from the first nibble (ULyudPrefetch; the stand-in shows until it arrives, never a load on the strike), the
  prompt rebuilt only when what it says changes; instance
  transforms are updated in place (`bMarkRenderStateDirty = false`), nothing collides, nothing goes into Lumen or distance
  fields, no allocation per frame. Meshes from blender/props/fishing can replace the stand-ins later.
- **Shop** (`Content/PeopleData/counters.json`, `CounterTrade`): the goods on Orlova Tanya's counter and `"buys_fish": true`,
  which adds the sell prompt (`ui.shop.prompt_sell_fish`) and Y.
- **Items** (`LyudInventory.cpp`): `fishing_rod`, `bait_worm`, `bait_maggot`, `lure_spoon` (hand items), and one `fish_<id>` per
  species. Names and descriptions in all three string tables (`item.*`, `ui.fish.*`).
- **Icons**: six Higgsfield pictures (five grid sheets and the fried fish; grid 6 is 3x2: the two rods, shrimp, dough, wobbler) (`design/fishing/icons_grid_*.json`, prompts and job ids pinned), cut by
  `tools/fishing_icons.py` into `textures/localized/ru/icon_<id>.jpg` (no words on them: one picture for every language), listed
  in `localization/assets.json`.
- **Sounds** (`audio/sounds.json`, group `fishing`): rod swish, float plop, reel click, the clutch's drag, the line snapping,
  splash, flapping, into the bag. Generated with `tools/gen_sounds.py`, imported as `/Game/Audio/Fishing/S_<id>_<nn>`.

## Checks

- `tools/fishing/run.sh`: the model outside the engine. Bot anglers (good, greedy, lazy, jumpy) at spots weighted like
  `GetBiteWeights`: bite rates, baits matching diets, sizes and prices, a good angler landing ~25-35 fish an hour, greedy reeling
  snapping the line on big fish, the rods (casts, a slow angler's landings, the bottom rig's catch), the new baits, the weather's
  and the dusk's moods, the restaurant's prices, the cost of a step.
- Console: `lyud.Fish.Give [rod]` (a rod and every bait), `lyud.Fish.State`, `lyud.Fish.Press 1|0`, `lyud.Fish.Click`, `lyud.Fish.Bite`
  (a take now), `lyud.Fish.Hook <Species>` (one on the line now), `lyud.Fish.Bait <item>`, `lyud.Fish.Log`, `lyud.Fish.Book [close|<Species>]`
  (gives the book and opens it); the fried fish: `lyud.Diner.Serve diner_fish`. Log lines begin
  `FISHING`.
