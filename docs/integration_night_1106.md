# Test-merge of the 2026-11-06 night branches (`integration/night-1106`)

A throwaway branch: main `bb083dd0` with tonight's ten branches merged into it, to see that they fit together and pass
the offline suites. Round 5 added the text fixes on `loc/night-1106`, and round 6 added the townsfolk's lines
(`dialogue/residents`) and the residents generator (PR #15). It is not meant to be merged to main as it is: each branch still goes in through its own PR or
merge. This page records what conflicted, how each conflict was resolved here, what was checked, and what is left for
the branch owners. Done in the cloud on 2026-11-07 (no Unreal build: the UE Mac is busy, and it can't run the game until
the Metal Toolchain is reinstalled).

It took seven rounds, because the owners kept pushing. Round 1 (`9be09292`) merged the heads as they were when the work
started. Round 2 merged the newer heads on top, in the same order, and the four branches that moved again while it ran
(eco, fishing, public-spaces, town-life) a second time at the end. Round 3 merged perfhunt, the restaurant and festivals
once more, after they pushed again at about 01:35 UTC. Round 4 took the small fixes that the buildings, public spaces,
town life, the restaurant, eco and perfhunt pushed by 01:55 UTC. Round 5 took the restaurant's `998223bb` (PR #11),
fishing's `92df3cec` and `loc/night-1106`. Round 6 added `dialogue/residents` (after town-life) and `residents`
(PR #15). Round 7 took what city-terrain (its bird fix), fishing and the restaurant pushed by 02:31 UTC. The branch
holds every head as of 02:31 UTC. Later pushes aren't in
it, but the order and resolutions here still apply to them.

## Order

| # | Branch | Round 1 head | Head now merged | Commits over main | Conflicts (round) |
|---|---|---|---|---|---|
| 1 | `perfhunt` (PR #13) | `07dfbbc5` | `63b7f908` | 19 | none |
| 2 | `underwater-eco` (PR #9) | `0e9e8a53` | `3615e1d3` | 9 | none |
| 3 | `fishing` (PR #10) | `a67d5961` | `118aba5b` | 26 (PR #9 up to `0e9e8a53` included) | `CounterTrade.h` (1), `DefaultGame.ini` (2) |
| 4 | `city-terrain` | `72b39c18` | `19237568` | 17 | none |
| 5 | `wt/city-buildings` | `bafbcc72` | `1f3d5861` | 26 | none |
| 6 | `public-spaces` (PR #14) | `e8968eea` | `0514738a` | 61 | none |
| 7 | `town-life` | `bde10d42` | `d974e189` | 29 | `PeopleLife.cpp` (1), `counters.json` (2) |
| 8 | `review/restaurant` (PR #11) | `9cf4e4bc` | `26b8eefc` | 19 (its commits rebased onto main, merged with the old head) | `strings_{ru,en,ja}.json`, `localization/assets.json`, `LyudSoundscape.cpp` (1); `strings_ja.json` (2); `strings_{ru,en,ja}.json` (3) |
| 9 | `festivals` | `9a3bcbec` | `1c40e985` | 16 | `strings_{ru,en,ja}.json` (1) |
| 10 | `gfx` (PR #12) | `33643981` | `e10d5ac3` | 9 | `LyudGameMenu.cpp` (1), `PeopleLife.cpp` (2) |
| 11 | `loc/night-1106` | (round 5) | `caeb8eb8` | 1 | none |
| 12 | `dialogue/residents` | (round 6) | `c31977b7` | 21 (on an older town-life, `8d16ee62`) | none |
| 13 | `residents` (PR #15) | (round 6) | `1f19b3e0` | 8 | `.gitignore` (6) |

Why this order: perfhunt first, because the restaurant needs its `e8085366` (the bite subsystem roots `FLyudMeal`'s
meshes) and its people `GroundCache` and door fix stay as they are. PR #9 before PR #10, since `fishing` contains #9.
city-terrain before the buildings and public spaces (the plan and ground they build on); town-life after the three city
branches whose data it reads; the restaurant after perfhunt and town-life; festivals after town-life, public-spaces and
the restaurant (it calls all three); gfx last. This is the order the night review found to work.

`public-spaces` carries copies of city-terrain's commits up to `3ee0e861` (it was replayed onto it), so the shared city
files (`city_plan.py`, `world_map.py`, `city_mesh.py`, `LyudCityWindows.*`, `network.json`, ...) merged by themselves
and are exactly city-terrain's versions.

## Conflicts and how they were resolved

Twelve conflicts over the six rounds, all of them mechanical (both sides added things at the same spot). No change of
behaviour was needed on either side.

Round 1:

1. **`Source/Lyudmilovsk/Public/People/CounterTrade.h`** (fishing × perfhunt). Both added private members at the end of
   `ULyudCounterTradeSubsystem`. Kept both: perfhunt's IsOpen cache (`CacheRun`, `CachePerson`, `CacheActivity`), then
   fishing's `EFishDeal Deal`, `FPromptKey PromptedKey` and `SoldUntil`. `CounterTrade.cpp` merged by itself
   (perfhunt's cached `IsOpen` plus fishing's sell and cook prompt on Y) and reads coherent.
2. **`Source/Lyudmilovsk/Private/People/PeopleLife.cpp`, `Present()`** (town-life × perfhunt). perfhunt made
   `PlacedShown` a member `TSet<AActor*>` that is reset each call; town-life put its `TownPickTimer` / `PickTownBodies`
   block just above the old local `TSet<FString> PlacedShown`. Kept town-life's block, then perfhunt's
   `PlacedShown.Reset();`, and dropped the local set (the rest of the function already uses perfhunt's actor keys).
   This is the resolution agreed in the night review.
3. **`Content/Localization/strings_{ru,en,ja}.json`** (the restaurant × fishing/eco/public-spaces/gfx, then festivals ×
   all of them). Union of keys by a 3-way JSON merge. After the restaurant: 1196 keys on main, 1329 on the merge so far,
   1286 on the restaurant, giving 1419 per language. After festivals: plus its 121 keys, giving 1540. No key was changed
   on both sides, and the three tables keep the same key order.
4. **`localization/assets.json`** (the restaurant × fishing). Union of the `assets` entries (fishing's icons and the
   restaurant's dish icons: 116 entries). The styles are untouched.
5. **`Source/Lyudmilovsk/Private/Audio/LyudSoundscape.cpp`** (the restaurant × fishing). Both appended a sound group at
   the end of the one-shot table. Kept both: `Fishing`, then `Restaurant`.
6. **`Source/Lyudmilovsk/Private/LyudGameMenu.cpp`, `Close()`** (gfx × perfhunt). perfhunt's
   `LyudSettings::FlushStored();` and gfx's `LyudGraphics::SetMenuOpen(false);` landed on the same line. Kept both
   (flush the volume sliders, then tell the graphics the menu is closed), as agreed in the night review.

Round 2:

7. **`Config/DefaultGame.ini`** (fishing × city-terrain). Both added `+DirectoriesToAlwaysStageAsNonUFS` lines after
   `Transport`: city-terrain `City`; fishing `Fishing`, `City`, `Food`, `PeopleData`. Kept fishing's four, with `City`
   once.
8. **`Content/PeopleData/counters.json`** (town-life × fishing). fishing added `"cooks_fish": 15` to the diner's counter
   (`hayashi_rina`); town-life appended its twelve fair-stall counters (`"town": 1`) after that same entry. Kept
   fishing's diner line and town-life's twelve entries after it (19 counters). This is exactly what
   `tools/town/town_life.py` writes: it replaces only the `"town": 1` entries and leaves the rest as they are.
9. **`Content/Localization/strings_ja.json`** (the restaurant's fixes to its Japanese lines × the other branches' new
   lines next to them). 3-way per-key merge as in 3. No key was changed on both sides; 1640 keys in each language, in
   the same order.
10. **`Source/Lyudmilovsk/Private/People/PeopleLife.cpp`, console variables** (gfx × town-life). gfx gave
    `lyud.People.DrawDistance` the `ECVF_Scalability` flag and a new help line; town-life added
    `lyud.People.TownBodies` and `lyud.People.TownDrawDistance` right after it. Kept gfx's line, then town-life's two.
    Townsfolk are drawn within min(`DrawDistance`, `TownDrawDistance`), so the graphics levels' 80..150 m still cap
    them. How many are drawn (`TownBodies`, 48) is not on the graphics levels.

Round 3:

11. **`Content/Localization/strings_{ru,en,ja}.json`** (the restaurant's new head × the other branches' lines). Its four
    new keys (`ui.rest.kitchen_closing`, `ui.rest.catch_of_day`, `sign.rest.plaque`, `sign.rest.hours`) and its Russian
    prices, now «руб.» where they had ₽. 3-way per-key merge as in 3. No key was changed on both sides; 1644 keys in
    each language, in the same order.

Round 6:

12. **`.gitignore`** (residents × wt/city-buildings). Both appended an ignore block at the end. Kept the buildings'
    `blender/export/city/*.glb`, then the residents' `blender/residents/{_vendor,export,atlases,cache}/` and
    `renders/residents/`.

perfhunt's five new commits (save order, doors, prefetch, small speed-ups, its report) and festivals' review fixes
merged by themselves, and so did all of rounds 4, 5 and 7 and `dialogue/residents`.

When the branches go to main one by one in this order, the same files conflict again, and the resolutions above apply
as they are. The one difference: `DefaultGame.ini` then conflicts when city-terrain goes in after fishing.

## Merged by themselves, read by hand

Files that two branches changed and git merged without a conflict, checked for meaning, not only for text:

- `Sea/LyudUnderwater.{h,cpp}` (perfhunt × eco/fishing): perfhunt hides the sea life away from the water and frees it
  after 30 s, and gives no fish counts while parked; eco's `TakeFish` also works only while active, and fishing calls only
  `TakeFish`. Consistent. The fish mesh length fallback is fishing's shared `LyudSeaLife::FishLength` (`6584273c`),
  which eco's newer commits leave alone.
- `Food/LyudBite.{h,cpp}` (perfhunt × the restaurant): `MealAssets` (perfhunt `e8085366`) roots `LoadMesh`,
  `LoadMeshBig`, `LoadMaterial` and `UtensilMesh` in `BeginMeal`. The restaurant adds no other object pointer to
  `FLyudMeal` (only `LoadOffset`) and never swaps those fields mid-meal, so its dishes are covered.
- `LyudInventory.cpp` (perfhunt × fishing × the restaurant): separate item blocks, no item id defined twice.
- `HeroCharacter.*`, `HeroAnimInstance.cpp` (perfhunt × the restaurant): perfhunt's `FindReach` / `FindBike` caches and
  bone names, the restaurant's `StandOffset` and table arms; different code.
- `LyudText.cpp` (perfhunt × town-life): perfhunt's interned `Language()` and missing-path cache, town-life's merging of
  `Localization/<sub>/strings_<lang>.json`; different functions.
- `GameMenu/SLyudGameMenu.cpp` (perfhunt × gfx): the sliders' `OnMouseCaptureEnd` flush and the Graphics page.
- `audio/sounds.json` (festivals × fishing): parses, holds fishing's group and the festivals' two music entries
  (`festival_1`, `festival_2`; its fireworks and bonfire are procedural, `tools/festival_sounds.py`).
- perfhunt's people `GroundCache` (`38ca628d`), door fixes (`49f51a0e`, `29ea4729`), meal assets (`e8085366`), bird
  sim change (`33a04151`) and save hook for late-starting animals (`63b7f908`) are in the merge as on perfhunt; no
  other branch touched the doors.
- `LyudSave.{h,cpp}` (perfhunt × eco, which fishing contains): perfhunt's `60d8e699` runs `OnBeforeSave` before the
  registered objects are written and adds guards (a new slot never overwrites one on disk, a broken slot can't feed NaN
  to the clock or the player); eco's `0e9e8a53` adds `OnNewGame`. Different spots. Only main's birds, corvids, animals
  and people listen to `OnBeforeSave`, and they now save their current state, not the one before.
- The restaurant reads fishing's `ULyudFishingState` by name (`FindPropertyByName`) for its catch of the day:
  `KitchenTaken` (the fish each kitchen took, keyed by kitchen, here `yantar`) and `KitchenDay`. Both are `UPROPERTY`s
  of the types it checks (`TMap<FName, int32>`, `int32`) on a game instance subsystem, as it expects. Only the comment
  at `LyudRestaurant.h:268` still names the older `KitchenSold`.

And a line count over everything: every line of code (C++, Python, ini, shell) that a branch adds over main is in the
merge, except lines a later branch changed again. Those are fishing's copy of eco's older code (eco has changed it
since), public-spaces' copies of city-terrain's older `city_plan.py` / `world_map.py` lines, `dialogue/residents`'
copies of town-life's older lines (it branched from `8d16ee62`), and eco's fish length fallback (above). Nothing was lost
in the resolutions.

## Cross-branch checks on the merged tree

- **Console names:** 434 `lyud.*` registrations (354 on main), none registered twice, no names that differ only by case.
- **Types:** 275 reflected types (240 on main), no name defined twice; header names unique in the module; every
  `.generated.h` include matches its file.
- **Unity builds:** no file-scope name (static, anonymous namespace, console variable, macro) is newly shared by two
  `.cpp` files. (`LYUD_HAS_BIRDLIFE` is defined the same way in `BirdTame.cpp` and `BirdFlightSubsystem.cpp`, as on
  main.)
- **Save keys:** 17, unique (`Ecosystem`, `Fishing`, `Festivals` are new).
- **Contracts between branches:**
  - town-life calls `ULyudWindowLights::SetFlatLit(FString)` (buildings) and `ULyudCityWindowsSubsystem::SetFlatLit(FName)`
    (city-terrain) by reflection: both are `UFUNCTION`s. `SetDriven` exists only on the buildings' side; the terrace
    windows have none, which town-life skips as designed.
  - festivals' `TrySetStayUp` finds town-life's `SetStayUpUntil(float)`; both mean hours from today's 00:00 (past 24
    allowed).
  - `ALyudPublicSpaces` sets the `PlanStage` / `PlanStalls` tags that festivals look for.
  - The restaurant's calls by name, `IsFestivalToday` and `IsFestivalDish`, are `UFUNCTION`s on
    `ULyudFestivalSubsystem`; every dish in `festivals.json` menus is in the restaurant's `menu.json`.
  - The spot ids festivals names (`S-FAIR`, `S-SQUARE-AUDIENCE`) exist in the plan.
  - The townsfolk's lines (`TownTalk.cpp`, `dialogue/residents`) read by name the restaurant's `WaiterBody`, `CookBody`,
    `BillFolder` (object `UPROPERTY`s), `HeroDishes` and `OnTray` (arrays), and the festivals' `ActiveFestival()` /
    `ActivePhase()` (`UFUNCTION`s returning `FName`). All of them exist in the merge with those types.
  - fishing's kitchen buyer at the Amber (`fishing.json` kitchen `yantar`) now stands at the restaurant's staff door,
    about 1.7 m from `seats.json`'s `back_outside` (fishing `2a4cfff9`, from the restaurant thread). The restaurant's
    cook-at-the-door trigger (`restaurant.json` `kitchen_door`, `26b8eefc`) has the same point, radius and hours as
    `fishing.json`: offset [-1571, 349, 0] cm from the plot, 220 cm, 11..23.
- **Generated data:**
  - `tools/city_plan.py` regenerates `Content/City/city_plan.json` byte for byte.
  - `tools/town/town_life.py` run on the merged inputs (the plan, the buildings' flats, the public spaces' spots, the
    restaurant's `seats.json`) gives `town_places.json`, `townsfolk.json`, `counters.json` and `Localization/town/`
    byte for byte: 2219 places (1828 flats), 898 people. So town-life's data already matches the merged restaurant and
    buildings, including the eight new building types; the review's "regenerate `town_places.json` after both
    merge" is already done.
  - `tools/restaurant/seats.py` regenerates `seats.json` byte for byte.
  - `tools/public_spaces.py` on the merged plan gives exactly the layout it gives on its own branch: 887 props, 353
    spots. Heights came from the macro shape, because the built terrain isn't in the cloud. Its validator finds 0
    problems, and no prop, box, tree or lamp centre lands inside a building plot, including the three new plots
    (`P-CLUB3`, `P-LIBRARY`, `P-HALL`) and the two moved riser plots.
  - Buildings: every building type on the plan has its `blender/city/buildings/<type>.py` and an export, except
    `amber_pavilion` (the restaurant's own pavilion) and `tidal_pool` (see the owners' list). The old `plain_block__*`
    exports are gone.
- **Localization** (`tools/check_localization.py`, which `dialogue/residents` extends to its `Localization/barks/`
  tables): 4035 keys in 3 languages (1645 in the main tables), no missing keys and no placeholder mismatches. Every literal text key the code asks `LyudText` for is in the tables. `loc/night-1106` adds the one that
  main lacked, `corvid.prompt.take`, and its Japanese fixes don't touch any key another branch changed. It reports 53 problems: main's 16 (the `find_*` items have no icon entries) plus 37 fishing icon pictures
  not generated yet (made on the Mac by `tools/fishing_icons.py`).
- **Mac-only compile errors:** every plain C++ file the suites build was compiled with `-Wshadow
  -Wunreachable-code-loop-increment`, which the Mac UE build treats as errors and the suites' `-Wall` misses. The game
  sources are clean. Only test drivers warn: `tools/sea/seatest.cpp` and `tools/weather/weathertest.cpp` as on main, and
  `tools/underwater/underwatertest.cpp:179,432` from eco. UBT doesn't build test drivers, so these are harmless. The
  Unreal-only files, which can't be compiled here, were searched for locals and parameters named like a field of their
  class: no real hits (the three new matches are a static method, a template argument and a lambda).

## Offline suites

Every `tools/*/run.sh` test target, run in the cloud (Linux; birdsim below says why the platform matters), on main
`bb083dd0` and on the merge: round 1 `9be09292`, round 2 `947c7191` (`peoplesim` also on `9c3d9f4a`, after town-life's
last commit), round 3 `f589d679`, and round 4 `e756a240` (`animalsim` and `winter` also on `b8d79afa`, after perfhunt's
last commit). Round 4 ran every suite except birdsim, because it changes none of the bird sim's sources or data. Round 5
ran every suite on `559683ce`. After it, fishing's last commit got `fishing` and both `underwater` targets again (on
`b8fae5b7`), and `loc/night-1106` changes only the string tables, which no suite reads. Round 6 ran every suite on
`edd4be6c`, including the new `barksim`, and round 7 on `f3817a2d`. Rounds 1 to 6 gave the same results, and the bird
sim's output was the same line for line apart from timings. In round 7, city-terrain's bird fix changes birdsim (below);
every other suite gives the same results as before.

| Suite | main | merge |
|---|---|---|
| `animalsim test` | all passed | all passed |
| `audiosim test` | 2 failed (thunderstorm peak, far-storm rumble) | the same 2 failed |
| `birdsim test` | 121 passed, 1 failed (albatross); on the UE Mac 120 / 2 | rounds 1-6: 117 passed, 5 failed (see below), on the UE Mac 119 / 3; round 7 (city-terrain `19237568`): 116 passed, 6 failed |
| `peoplesim test` | all ok without `railway_built.json`; 1 failed with it ("a bike goes back home into its entrance") | all ok both ways |
| `peoplesim town` (town-life) | n/a | all ok (both ways) |
| `railsim test` | ok | ok |
| `sea test` | passed | passed |
| `transportsim test` | all ok | all ok |
| `underwater test` | all ok | all ok |
| `underwater eco` (eco) | n/a | all ok |
| `weather test` | passed | passed |
| `winter test` | all ok | all ok |
| `fishing/run.sh` (fishing) | n/a | ALL OK |
| `festivalsim test` (festivals) | n/a | all festival checks pass |
| `barksim/run.sh` (dialogue/residents) | n/a | all ok |

**birdsim** is the one suite that gets worse. Its counts depend on the platform. All the numbers on this page are from
the cloud: Linux x86_64, Ubuntu clang 18.1.3, libstdc++ 13, glibc 2.39. On one platform the bird world is
deterministic: clean `git archive` exports of main `bb083dd0` and of the tip `775473a3`, with the LFS bird data
checked against its pointers, give the same output line for line as the worktree runs (121 / 1 and 117 / 5). Across
platforms it isn't. The perf thread gets 120 / 2 on main and 119 / 3 on the tip on the UE Mac (Apple clang, libc++).
Turning on fused multiply-adds alone (arm64 compilers use them by default) changes 52 lines of main's output here
(the feeder's food left 0.89 -> 0.00, the murmuration's distance offshore), though not which checks pass. So a check
near its threshold can pass on one platform and fail on the other. Compare counts only between runs on the same
platform. The tip is worse than main on both: by four checks here and by one on the Mac.

It is very sensitive to both tonight's changes to it, so each tree fails a different set of checks. Each branch was
also run alone (here, on Linux):

| Tree | Failed checks |
|---|---|
| main | the albatross mostly sits on the sea in a calm and flies in a blow |
| perfhunt alone | gulls go down to the sand the low tide lays bare; the albatross (as main) |
| city-terrain alone | every starling is asleep in the reed by dusk + 1 h; they leave the roost before sunrise; blackbirds: losses, repeat clutches and second broods (11 pairs, 46 lost, 0 with two broods); he sings near the nest in the morning |
| the merge | feeder: only birds that like suet in the garden come; the peregrine's attacks send escape waves through it; in a gale the small birds leave the open for the lee (388 of 1677 stay in the open, the check wants half of the calm's 676 or fewer); the two blackbird checks; (the albatross passes) |

The two blackbird checks come from city-terrain's regenerated bird world (`72b39c18`: 7661 -> 7318 habitat sites on
the terraced ground): they fail on city-terrain alone and on the merge. The other three appear only when perfhunt's
change to the bird sim (`33a04151`) meets that new world. That change is more than a speed-up: the sight line to the
player is kept for 0.15 s, and the hawk now takes the best prey by weight where it used to take the first one found.
perfhunt's later commits (rounds 3 and 4) don't touch the bird sim's sources, so this held up to round 6.

Round 7 brought city-terrain's bird fix (`19237568`: gardens back on the terraced town, 30 -> 446 Garden sites, and
the blackbird check run over five seasons). On Linux it fixes the two blackbird checks, but other checks move:

| Tree (Linux) | Failed checks |
|---|---|
| city-terrain `19237568` alone | 114 passed, 8 failed: six feeder checks (suet and grain: birds come after a couple of hours, only the right birds come, the feed goes down, a feed nobody eats stays empty), "and sleep in the reed", the albatross |
| the merge `f3817a2d` | 116 passed, 6 failed: feeder: birds come to suet after a couple of hours; the feed goes down as they peck; every starling is asleep in the reed by dusk + 1 h; they leave the roost before sunrise; and sleep in the reed; the albatross |

The peregrine, gale and blackbird checks now pass on the merge. The feeder and starling checks fail instead. These
round 7 numbers are Linux only: nobody has run them on the Mac yet.

Cloud quirks (not branch problems):
- The `run.sh` scripts were run with bash. On the Mac `/bin/sh` is bash; Ubuntu's dash rejects main's `"${@:2}"` in
  `tools/underwater/run.sh`.
- clang++ was given `-include algorithm`: `tools/sea/seatest.cpp` uses `std::sort` without including `<algorithm>`,
  which Apple's libc++ pulls in through other headers and libstdc++ doesn't.
- `peoplesim test` depends on whether `Content/Railway/railway_built.json` exists. That file is made per checkout by
  `railsim` / `tools/railway.py` and isn't committed. Main fails the bike check with it and passes without it.

## For the branch owners

None of the eleven conflicts needs its owner: each is resolved here by keeping both sides. What the owners still have
to do:

1. **city-terrain and perfhunt: birdsim.** Decide whether the new bird behaviour is right on the terraced town and
   fix it or retune the checks (above). On Linux the blackbird nest checks are city-terrain's (bird habitats on the
   new ground); the feeder, peregrine and gale checks show up only with perfhunt's hawk and sight change on that
   ground; perfhunt alone also fails the low-tide gull check. On the UE Mac, the perf thread finds one failure more
   than main (119 / 3 against 120 / 2), and that one is city-terrain's. The Mac is the platform the game is tested
   on, so its list is the one to fix against. city-terrain's fix `19237568` (round 7) makes the blackbird checks pass
   on Linux, but it breaks the feeder and starling checks (116 / 6 on the merge, 114 / 8 on city-terrain alone, above).
   Run it on the Mac before calling it done.
2. **public-spaces:** rerun `tools/public_spaces.py --strict` on the Mac with the built terrain after city-terrain is in
   main. The layout is unchanged on the merged plan, but heights from the built ground couldn't be checked here.
3. **fishing:** the 37 icon pictures (`tools/fishing_icons.py` on the Mac), already known.
4. **city-terrain (the city plan and terraces thread): the tidal pools.** `P-TIDALPOOL` (`tidal_pool`) has been on main's
   plan since the first city plan, but none of the ten branches builds it, so nothing stands there yet. By the plan's
   "Who builds what" table a plot's building is the buildings thread's. But `tidal_pool` isn't in the buildings
   catalogue (`docs/city_buildings.md`), and the pools are ground, not a building: basins in the beach and the flat,
   s -35..5, that "fill at high water (docs/tides.md)". The sea already draws water in every hollow of the terrain's
   depth map that can't drain, up to its rim, once the ebb leaves it (docs/tides.md, "Pools"). So the pools come almost
   for free if they are carved into the terrain with their rims in the depth map. The terrain, the seawall and its sea
   steps are city-terrain's (`tools/world_map.py`, `tools/city_mesh.py`), so city-terrain should own them, with the
   concrete rims built in `city_mesh.py`. The ground's pool water is ankle deep to the game (the CPU side ignores it).
   Pools deep enough to swim in would also need real water, and that part could go to the public spaces, as with the
   Cascade's water steps (city_mesh builds the steps, the public spaces put the water in).
5. **underwater-eco (optional):** `tools/underwater/underwatertest.cpp:179,432` shadow a local. This is a test driver,
   not a game build error.
6. **review/restaurant (optional, packaging only):** the game reads four files from `Content/Restaurant/`, which isn't
   in `DefaultGame.ini`'s `DirectoriesToAlwaysStageAsNonUFS`, so a packaged build won't have them. Main has the same
   gap for `Academgorodok/`, `Bikes/`, `Diner/`, `Europa/` and `Railway/`.
7. **perfhunt, town-life, review/restaurant (optional, tidy-up):** each now works around the empty asset registry of an
   uncooked `-game` run on its own: `ULyudPrefetch` scans a folder it has nothing for (perfhunt `0838303e`),
   `PeopleLife.cpp` scans `/Game/Characters/Humans/Town`, and `LyudRestaurant.cpp` scans `/Game/Restaurant/Meshes`.
   They don't clash; they could share perfhunt's one.
8. **main, not tonight's branches:** `audiosim` has 2 failures; `birdsim` fails the albatross check (on Linux; 2
   checks fail on the Mac); `peoplesim test` fails the bike check when `railway_built.json` exists;
   `tools/sea/seatest.cpp` lacks `#include <algorithm>`. The bird checks that sit near their thresholds flip
   between platforms (above), so they need more margin, or every report should name the platform it ran on.

Done since round 1: wt/city-buildings built the eight types of main's "buildings round 3" plan (`cobogo_house`,
`capsule_cluster`, `origami_row`, `pilotis_curve_slab`, `ribbon_corner_block`, `cog_club`, `capsule_library`,
`shell_chapel_hall`), and fishing moved its Amber kitchen buyer to the restaurant's staff door.

## Not checked here

- The Unreal build (UBT) and anything in game: no UE in the cloud, and the Mac is blocked by the Metal Toolchain.
- Assets that aren't in git and Mac-only imports (city glb, `M_CityProp`, festival WAVs, `/Game/Restaurant` meshes and
  sounds, fish and sealife meshes, icons).
- Benchmarks (perf checklist: frame time and memory against main before merging). They have to run alone on the Mac.
- Heights from the built terrain (`Saved/World/terrain_out.bin`) in `public_spaces.json`.
- The residents generator's own checks (`blender/residents/check_export.py`, `test_accessories.py`): they need the
  models built in Blender (`blender/residents/export`, not in git) and the vendored MakeHuman data.
