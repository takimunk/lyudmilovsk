# The coastal food web

Under the club's beach the sea is not a set of fish placed by habitat any more: it is a food web. 44 groups from the
plankton to the grey seals live, eat each other and come and go with the seasons, the fish round the diver come in the
numbers the web gives and hunt what it says they eat, and what the player takes out comes out of it.

Code: `Private/Sea/EcoModel.cpp` (plain C++, `LyudEco::FModel`), `ULyudEcosystem` (`Public/Sea/LyudEcosystem.h`, a game
instance subsystem, saved as "Ecosystem"), the fish in `Private/Sea/FishModel.cpp` (`LyudFish::FSim`). Underwater world:
[underwater.md](underwater.md).

## The model

Ecopath with Ecosim (Christensen & Walters 2004), cut to what the game needs:

- **A balanced base.** Every group has a biomass B (g wet weight per m2 of the coastal sea, 0-20 m), production P/B and
  consumption Q/B per year and a diet. What its predators eat of it at the base is its predation; the rest of its
  production is other mortality (M0), which goes to the detritus with the uneaten food (20% of what is eaten). The test
  checks that nothing is eaten past its production (Ecopath's EE <= 1): the worst is the amphipods at 0.92.
- **Foraging arenas.** Each day (in quarter-day steps) a predator j takes from prey i
  `Q = a v Bi Bj / (2v + a Bj)`: only the part of the prey out in the open can be taken (vulnerability K = 2, mixed
  control), which keeps the web from the boom and bust of plain Lotka-Volterra. `a` and `v` come out of the base so the
  base's flows come out at the base.
- **Producers** grow `Pmax B / (1 + B/B0)` by the light: the phytoplankton has the spring bloom in April (×3.4), the
  blue-greens in late July and August, a small autumn bloom, a dark winter; the weed grows with the warmth.
- **Warmth.** The water runs from ~1 °C in February to ~18 °C in mid August; every cold-blooded group eats by it
  (Q10 2.2, mean 1). Seals and birds eat the same all year.
- **Migrants.** Groups that are only here some months (sprat in the cold half, garfish May-September, lumpsuckers
  February-June, twaite shad May-July, smelt and whitefish for the winter, salmon in autumn, vimba April-November,
  moon jellies June-November, the sea ducks October-April) move in and out over ~12 days. The part away belongs to the
  open Baltic's stock and drifts back toward the base over a year; it neither eats nor is eaten here.
- **Local thinning.** The coast-wide numbers barely notice a few fish; the spot does. A catch thins its species in that
  40 m cell (a bottom fish by a quarter, a roamer by 8%); it heals as fish move back in: roamers in ~3 days, the ones
  that stay put (gobies, sculpins, flatfish, eelpout, shrimp) in ~3 weeks.

## Who is in it

| Level | Groups |
|---|---|
| Producers | phytoplankton, the weed (eelgrass, wrack, red and green algae) |
| Detritus | everything dead and uneaten |
| Plankton | zooplankton (copepods, cladocerans), moon jellies |
| Bed and water invertebrates | mysids, amphipods (Gammarus), worms (Hediste and the rest), Baltic clam (Macoma), blue mussels, brown shrimp (Crangon), the isopod Saduria, Harris mud crab |
| Plankton-eating fish | herring, sprat, sand eel, three- and ninespine sticklebacks, smelt, twaite shad, garfish, both pipefish |
| Bottom-feeding fish | round, sand and black gobies, flounder, roach, vimba, whitefish, eelpout, rock gunnel, sculpins (shorthorn and fourhorn), lumpsucker (jellies) |
| Fish-eating fish | cod, sea trout, salmon, perch, turbot, eel |
| Top | grey seals, fish-eating birds (cormorants, mergansers, terns), sea ducks (eiders, long-tailed ducks on the mussels) |

The diets follow the Baltic stomach studies' broad picture: herring and sprat on zooplankton (herring also mysids); cod on
the coast on Saduria, shrimp, gobies and clupeids; round gobies crush mussels; flounders take clams and mussels;
lumpsuckers eat jellies; grey seals herring, cod and flatfish; cormorants gobies, perch and herring.

What the checks show (`tools/underwater/run.sh eco -v`): thirty years of seasons from the base keep every group within
0.8-1.8 of its base (the plankton-eaters a little up, whitefish a little down), a year to the next within 1%. Fishing the
cod out for ten years lets Saduria grow by 40% and the sprat by a quarter; the grey seals switch to herring and hold. Fishing
the herring out takes the seals down by a quarter and the cod by a fifth, and doubles the mysids they ate.

## The fish round the diver

`FSim` (the fish model, see [underwater.md](underwater.md#fish)) takes from the web:

- **Numbers.** Each species' groups and settled fish come in proportion to its biomass against the base, times the local
  thinning (`FEnv::Abundance`). The months the fish model already knows (its own table): the web only scales. Bottom
  dwellers settle at their table's Abundance per 100 m2 of the best habitat (a rare one in some cells only); a rare roamer
  (under one group's worth round the eye, like a seal) comes now and then, not on every dive.
- **Who eats whom.** Hunters (cod, sea trout, salmon, grey seals) pick a school they eat within 12 m, chase and strike:
  the school bursts away, and now and then one is caught (35%, a seal 60%) and gone. Failing a school, a hunter noses
  down for something on the bed it eats: a shrimp, a goby, a flounder (half the strikes catch; a missed shrimp digs into
  the sand). A hunter on the hunt scares what it eats near it: cod scatter from a seal.
- **Ambush.** Everything else that eats fish or crustaceans snaps at what comes within reach (a quarter metre plus one and
  a half of its own lengths) if it is under 60% of its own length: a sculpin at a shrimp, a goby at an amphipod, a perch at
  a goby, sticklebacks and herring through a mysid cloud (40% catch, then 8-28 s until hungry again).
- **Grey seals** come up to breathe every one to three minutes (4-9 s at the surface) and come to look at a diver: in to
  ~4 m, round them for up to 40 s, then off for at least a minute and a half.
- Every meal is an event: `ULyudUnderwaterSubsystem::OnFishEaten(Predator, Prey, Location)` (a journal, a sound, a
  bubble cloud can hang off it); `lyud.SeaLife.Debug 1` lists the meals seen.

In a half-hour test at three spots ~200 meals: sticklebacks on mysids, sculpins on amphipods, turbot on sand gobies, eels
on round gobies, grey seals on herring...

## The new animals

| Species | Way | Where |
|---|---|---|
| European smelt (Osmerus eperlanus) | school | inshore October-May |
| Twaite shad (Alosa fallax) | school | passes in May-July |
| Baltic whitefish (Coregonus maraena) | shoal | coast for the winter |
| Atlantic salmon (Salmo salar) | hunter | September-December |
| Roach (Rutilus rutilus) | shoal | weed and stones in the shallows |
| Vimba bream (Vimba vimba) | shoal near the bed | sand, April-November, more at night |
| European eel (Anguilla anguilla) | lurker | stones by day, out at night |
| Rock gunnel (Pholis gunnellus) | lurker | stones and weed |
| Straightnose pipefish (Nerophis ophidion) | hover | wrack and eelgrass, May-October |
| Fourhorn sculpin (Myoxocephalus quadricornis) | lurker | cold deep sand from 8 m |
| Black goby (Gobius niger) | hopper | eelgrass and sand |
| Ninespine stickleback (Pungitius pungitius) | shoal | weed in the shallows |
| Brown shrimp (Crangon crangon) | crawler, burrows | sand; buried by day, digs in when frightened |
| Saduria entomon | crawler | sand from 5 m |
| Harris mud crab (Rhithropanopeus harrisii) | crawler | stones |
| Amphipods (Gammarus) | crawler | stones and weed |
| Mysids (Neomysis integer) | swarm: a drifting cloud low over the bed | everywhere |
| Grey seal (Halichoerus grypus) | hunter, breathes, curious | the open coast from 2 m |

Two new ways in the fish model: **Crawler** (settled per 6 m cell like the bottom fish; short walks of 8-43 cm, rests of
1-5 s, a dart or a dive into the sand when frightened) and **Swarm** (a wide flat cloud of 80-250 drifting with the water,
each one jittering). Meshes: the fish by `blender/fish/species/<Id>.py`, the crustaceans, mysids and the seal by
`blender/sealife/` (same export conventions, so `unreal/scripts/import_fish.sh` and `M_LyudFish` take them as they are).
Build on the Mac: `tools/fish_build.sh <Ids>` and `tools/sealife_build.sh`, then `unreal/scripts/import_fish.sh`. In a cloud
container without Blender: `pip install bpy` (+ `apt-get install libegl1 libgl1-mesa-dri` for the EEVEE previews) and run the
builders with a tiny wrapper that sets `sys.argv = ["blender", "--", ...]` and `runpy`s the script.
Names: `fish.<id>.name`, the web's other groups `eco.<id>.name`.

## For fishing

`ULyudEcosystem::GetBiteWeights(Location)`: the fish that would take a bait here and now, by weight (the web's numbers,
the local thinning, how well the spot suits each, how active it is this month and hour; no shrimp, mysids or seals).
`ReportCatch(Species, Location, Grams = -1)` takes a caught fish out of the web and thins the spot;
`ULyudUnderwaterSubsystem::TakeFish` takes a drawn fish out of the water round the eye (a hook, a net).

## Cost

The web: 44 groups, ~250 links; a game hour's step 2-3 µs, a fresh game's six-year warm-up ~40 ms once, a catch-up after
a load or a long sleep at most three years. It is stepped every two seconds of play in whole game hours, with no
allocations. The fish: the same instanced meshes moved in place every frame; the ambush checks are staggered (each fish
looks every 16th step), the tiny animals (under 4 cm) are culled past 12 m.

## Console

`lyud.Eco.Report` (every group's biomass against its base, here or away, what eats most of it), `lyud.Eco.Days <n>` (live
n days at once), `lyud.Eco.Reset`, `lyud.Eco.Scale <Id> <factor>` (`lyud.Eco.Scale Cod 0.05`: the cod fished out; then
`lyud.Eco.Days 1000` and dive to see the shrimp and Saduria everywhere).

## Checks

- `tools/underwater/run.sh eco [-v]`: the base balances, thirty years of seasons stay bounded and settle, the spring bloom
  and the migrants, the two cascades (cod, herring), save and load, the local thinning heals, the cost.
- `tools/underwater/run.sh test`: the fish as before, plus every fish species is a node of the web, nothing is eaten by
  what does not eat it, hunters and ambushers both feed, a grey seal comes up to breathe and comes to look.
- `unreal/scripts/test_underwater.sh`: a shot of each new animal (`smelt` ... `seal`).
