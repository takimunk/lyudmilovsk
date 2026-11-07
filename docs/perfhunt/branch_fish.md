# Branch review: fishing, underwater-eco (fish)

Merge base for both: 8f271189. `fishing` contains f11dd8fe (the food web) and adds fishing on top. `underwater-eco` is
f11dd8fe plus 00d3f5d2, which touches no C++. The Sea/ C++ is byte-identical on both branches (`git diff origin/underwater-eco
origin/fishing -- Private/Sea Public/Sea` is empty), so every finding marked [ECO] applies to both branches.
Line numbers are on the branch (`git show origin/<B>:<path>`).

## underwater-eco (and fishing) [ECO]

### E1 S2: Ambush is an O(N^2) scan over every fish, per frame (conf high)
Private/Sea/FishModel.cpp:806-815 (Step) -> FSim::Ambush :492-520 (`for (FFish& O : All)` at :501).
Every fish that is not a Cruiser, Swarm or Surface and is at least 5 cm long scans all of `All` on one step in 16. Herring
(School, groups of 40-110) count too, and so do smelt, sprat, roach and the bottom fish. That is N/16 scanners x N checks per
frame. With the new mysid clouds (80-250 each) plus sprat and herring schools, N is easily 800-1500, so 40k-140k
pair checks per frame, each with an `Eats()` lookup and a distance. Roughly 0.2-0.7 ms of game thread while diving, rising with
the square of the fish count (lyud.SeaLife.Density).
Fix: (a) per species, precompute once in SetDiet a `bHasFishPrey` flag and a list of prey species, and skip any fish whose
prey has no live members (a per-species count is already cheap to keep in Step). (b) Bucket `All` once per Step into a 1 m
grid (a reused `std::vector<int>` sorted by cell key, or an `unordered_map` cleared but not freed) and have Ambush look only at
the 3x3 cells round F.Pos, since Reach is under 1 m for anything that ambushes. (b) alone makes it O(N).

### E2 S2: the food web's first sync is a 6-year simulation on the game thread (conf high)
Private/Sea/LyudEcosystem.cpp:78-87 (Warm), called from Sync :100-104, called from ULyudUnderwaterSubsystem::Tick
(LyudUnderwater.cpp:402-407) on the first world tick that has a sea and a PC.
`Model->Step(Today, 6*365)` runs in quarter-day SubSteps (EcoModel.cpp:362-372): 8760 SubSteps x a 44x44 diet loop with a
division each (EcoModel.cpp:304-320), about 17M inner iterations, plus the cell-heal loop. Expect a 20-60 ms hitch on the first
frame of every new game, on `lyud.Eco.Reset`, and on loading a save that has no food web (OnGameLoaded :270-274 zeroes
SteppedTo). A catch-up after a jump (MaxCatchUpDays = 3 years) can cost half of that again.
Fix: warm the web in larger steps (`Step` with a 1-day H while warming; the base has no fast dynamics to resolve at that point),
or do it in ULyudEcosystem::Initialize / behind the loading screen. Better still, bake the warmed state once (it depends only
on the day of the year) into the base table. The same applies to big catch-ups: cap H at 1 day when Days > 30.

### E3 S3: TActorIterator<ALyudSky> in the ecosystem (the pattern perfhunt removed from Underwater) (conf high)
LyudEcosystem.cpp:93 (Sync, every 2 s from the underwater tick, and also when far from the water) and :185 (GetBiteWeights,
every 3 s while a float is out, plus every ForceBite).
perfhunt 660f49fb replaced exactly this in LyudUnderwater.cpp with a cached `TWeakObjectPtr<ALyudSky> SkyRef` and `FindSky()`.
Fix: the same cached weak pointer here, or pass the shown time and sun altitude in from the underwater subsystem, which already
has them (`Sync(const FDateTime&)` already exists).

### E4 S3: per-meal FName/FString work and an O(N) erase in the sim (conf med)
LyudUnderwater.cpp:485-490. For every FEaten, the code builds two FNames from ANSI, concatenates an FString key with "<-",
calls `MealsSeen.FindOrAdd`, and broadcasts a dynamic delegate. Ambush gives a herring school next to a mysid cloud a meal
roll every 16 steps per fish, so this can be tens of meals per second. FishModel.cpp:817-818 also runs a full
`All.erase(remove_if)` on every step that has any meal.
Fix: cache the species FNames in a `TArray<FName>` (index -> FName) built once. Key MealsSeen as `TMap<uint32,int32>`
(Predator<<16|Prey) and format the strings only in GetDebugReport.

### E5 S3 bug: emptied schools stay alive and block new ones (conf med)
FishModel.cpp: hunts and ambushes decrement `Members` (:445, :512, Take :382) but never clear `bAlive`. Roam counts
`Have += G.bAlive` (:318), so a school eaten or fished down to 0 members still counts as present. No new school of that
species spawns until the empty group drifts out past 1.3 x Radius, and the empty group keeps swimming and spending StepGroup
time. Fix: `if (--Members <= 0) bAlive = false;` at the three decrement sites. The Roam compaction then cleans up.

### E6 S3 bug: buried crawlers come out at double speed (conf high)
FishModel.cpp:683 and :689. `F.Timer -= Dt` runs once for every bottom fish and again inside the new `if (F.bHidden)` block,
so a buried shrimp's 20-60 s halves to 10-30 s. Fix: drop the second decrement at :689.

### E7 S3: TakeFish ignores the parked or hidden state (merge note with perfhunt) (conf high)
LyudUnderwater.cpp:601-605. On perfhunt, the sea life is parked hidden for 30 s away from the water, and GetFishCount and
FindNearestFish return 0 while `!bActive`. TakeFish has no such guard, so after the merge a hooked fish
(LyudFishing.cpp:723, reach 15 m) would be taken out of the hidden, parked sim. Fix: add `&& bActive` like its siblings.

### Merge conflicts with perfhunt [ECO]
- LyudUnderwater.cpp Tick: both branches edit the same lines. perfhunt changed the early-return Release condition, the bNear
  hysteresis plus `bCollect`/`bCollectionFar`, the 30 s park (AwayTime, hidden life), and `FindSky()` in place of both
  `TActorIterator<ALyudSky>` uses. The branch adds Eco sync, Env->Eco, diet setup and the Eaten loop in the same function.
  Expect manual conflicts in Tick and in the private members of LyudUnderwater.h, where both add fields after `bool bActive`.
  Keep perfhunt's structure and insert the eco lines. The Eco->Sync block must stay before the `!bNear` early return, as it is
  on the branch.
- GetFishCount and FindNearestFish: perfhunt added `bActive`. TakeFish should get the same (E7).
- No other overlap. perfhunt does not touch FishModel or EcoModel.

## fishing (in addition to everything above)

### F1 S1/S2 crash risk: a static raw UMaterialInterface* (conf med)
Private/Fishing/FishingRod.cpp:153-157. `static UMaterialInterface* Basic` is a raw pointer to
/Engine/BasicShapes/BasicShapeMaterial, kept across rods. It is not rooted. Once the rod is put away and its MIDs are collected,
nothing may keep the material alive (the inventory only references it while something is held). GC can then free it, and the
next TakeOut uses a dangling pointer. This is the same class of bug as the "diary statics rooted" fix in perfhunt 80557208.
Fix: `static TStrongObjectPtr<UMaterialInterface>`, or keep it as a UPROPERTY on ULyudFishing and pass it in. LoadObject of
an already-loaded asset is cheap, so calling it each time also works.

### F2 S2: every TakeOut builds about 16 UStaticMeshes and about 22 MIDs (conf high)
FishingRod.cpp:169-289 (BeginPlay -> BuildStandIns -> RodLathe :32-104 with `BuildFromMeshDescriptions`).
The rod actor is spawned on every equip (LyudFishing.cpp:478-505) and destroyed on every PutAway, including Q, switching to
food or another hand item, and a bait taken in the hand. Each time it rebuilds the grips, seat, reel, spool, arm, knob,
blank segment, guide, line piece, float body and top, spoon and hook meshes, and makes one MID per part. That is a few ms of
mesh build per equip and garbage for GC each time.
Fix: build the meshes once, in a `UPROPERTY() TArray<TObjectPtr<UStaticMesh>>` on ULyudFishing (or a lazily filled cache
there), and pass them to the rod. Share one MID per tint (there are about 7 distinct colours) instead of one per component.

### F3 S2/S3: synchronous loads at the moment of the strike (conf high)
FishingRod.cpp:398-405 (SetFish, called from HandleEvents Hooked, LyudFishing.cpp:719). It runs LoadObject on
SM_<Species>, T_<Species>_D, M_LyudFish and M_LyudFishFin, plus M_SeaFish on the stand-in path (:428), at the hook moment.
From the shore the underwater life is usually not active, so these are cold loads that hitch right at the strike.
Fix: start an async load (FStreamableManager) of the likely species' mesh and texture when the float lands
(UpdateWeights already knows the candidates), or at least load M_LyudFish and M_LyudFishFin once on TakeOut.

### F4 S3 leak plus bug: SetFish MIDs pile up, and the same species keeps the old size (conf high)
FishingRod.cpp:387-442. Each hook of a different species creates 1-2 new MIDs and `Mids.Add`s them. Old ones are never
dropped while the rod stays out, so a long fishing session holds every MID ever made. Also, the early-out at :390
(`Species == FishSpecies && GetInstanceCount() == 1`) returns before the instance scale is set, so a second perch (or any
repeat species) is drawn at the first one's length.
Fix: keep the fish's MIDs in their own array and replace or reuse them per slot. In the early-out, update the instance scale
to `LengthM / Built` before returning.

### F5 S3: the counter prompt does fish bookkeeping every frame (conf high)
Private/People/CounterTrade.cpp:128-174 (SetPrompted), now called unconditionally every Tick (:212). The old early-out on
`Counter == Prompted` is gone. While the hero stands at any counter, each frame it runs: FishHeld and BiggestHeld (each loops
every catchable and calls `FishItem(C)`, which builds a std::string, converts it to TCHAR and constructs an FName, plus a second
FName from the species id, plus Inv->Count), one or two `LyudText::Get` formats, `ItemName`, and `Line.ToString()` for the
compare. That is about 100 small allocations a frame at a fish counter.
Fix: cache the fish item and species FNames once (a static `TArray<FName>` built from NumCatchable). Recompute the line only
when the counter changes or the inventory changes (an inventory revision counter or OnChanged), or at 4 Hz.
Merge: perfhunt 38ca628d edits CounterTrade.cpp IsOpen and Load and adds CacheRun/CachePerson/CacheActivity after
`int32 Prompted = -1;` in CounterTrade.h. The branch adds PromptedLine, EFishDeal, Deal and SoldUntil in the same spot.
That is a trivial textual conflict in the header; the .cpp hunks are separate.

### F6 S3: the rod prompt is formatted every frame (conf high)
LyudFishing.cpp:864-900 (Prompt, called from Tick :1190 every frame while the rod is up). Each frame it builds the bait name
FText, formats a LyudText with args (`hint_idle` with the bait name, `hint_fight` with tension), and compares with
`FText::EqualTo`. The cost is small but runs per frame for as long as the rod is out.
Fix: build the text only when the state, bait, Said, or rounded tension percent changes (keep the last key ints).

### F7 S3: small per-frame costs in ULyudFishing::Tick with no rod (conf high, negligible)
LyudFishing.cpp:1024-1047. Every frame, with any item equipped: `FindBait(TCHAR_TO_ANSI(*Equipped.ToString()))`.
Fix: compare the FName against the 3 cached bait FNames.

### Notes, not defects
- Rod components: all go through NoLumen (no collision, no Lumen or DF), and the line ISM updates 24 instances per frame.
  Both are fine.
- Delegates and timers: the fish book uses weak captures, Close() in Deinitialize, and no timers. ULyudFishing has no
  delegates. The rod is destroyed in Deinitialize and PutAway. No leaks found.
- LyudInventory.cpp, LyudSoundscape.cpp, SLyudDiary.cpp, LyudGameScreen.cpp and PlantDevTool.cpp are additive one-liners.
  The LyudInventory.cpp hunks do not overlap perfhunt's (CanOpen, Tick, SpawnHeld, Use).
- Low confidence: while the player is a bird, or with no AHeroCharacter, ULyudFishing::Tick returns at :1027 before the
  `bBusy` hide, so a rod that is out stays attached and visible on the hero's hand. Worth a quick check in play.
