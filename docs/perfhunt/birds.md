# Perf/leak/crash audit: birds

Scope: Private/Birds, Public/Birds, Private/BirdTame.cpp, Private/BirdFeedItem.cpp (read-only review, nothing built or run).
Paths are relative to unreal/Lyudmilovsk/Source/Lyudmilovsk.

Context numbers used for the cost estimates: the species table (BirdEcology.cpp:85-140) sums to about 2000 birds at
lyud.Birds.Population 1, and 1200 of them are starlings, which all join the evening murmuration. lyud.Birds.Bodies defaults to 12.
UBirdLifeSubsystem::Tick runs every frame and calls FWorld::Step, then ABirdLifeRenderer::Draw, then ABirdNestRenderer::Draw.

---

## BIRDS-01 S2: Stop() leaks every near-bird body actor (high)
- BirdLife.cpp:358-371 (Stop). The bodies are made in BirdLife.cpp:1505-1536 (EnsureBody).
- `Stop()` destroys `Renderer` and `NestRenderer`. The renderer's `Bodies` map holds ABirdActor actors that were spawned
  separately into the world, and nothing destroys them. ABirdLifeRenderer has no EndPlay or Destroyed override (grep finds
  none). The flight encounter's claimed bodies are also left alone.
- When it happens: every Stop and restart. That covers lyud.Birds 0 then 1, the lyud.Birds.Nests toggle (Tick line ~591,
  which the comment says the benchmark passes use) and PIE world teardown. In PIE the world goes away anyway, so this
  matters for the in-session restarts.
- Cost: up to 12 orphan skeletal birds per restart. They keep ticking (ABirdActor::Tick and the anim proxy) and keep
  rendering as frozen birds in place. They pile up across benchmark passes and skew the A/B numbers.
- Fix: add `ABirdLifeRenderer::EndPlay(...)` that loops over `Bodies` and calls `Destroy()` on each one, then
  `Bodies.Reset(); BodyActs.Reset(); BodyBorn.Reset();`. Alternatively, call the same loop from `Stop()` before
  `Renderer->Destroy()`.

## BIRDS-02 S2: ISM sync re-uploads the whole high-water-mark buffer every frame (high)
- BirdLife.cpp:1237-1253 (ABirdLifeRenderer::Sync).
- The instance count only ever grows (`AddInstances` with +25% slack). Every frame that a pose has at least one bird, `Arr`
  is padded to `GetInstanceCount()` and all of it is sent through `BatchUpdateInstancesTransforms`, including the hidden
  zero-scale tail.
- After one evening murmuration, the starling Flap0/Flap1/Flap2/Glide ISMs hold about 1500 instances each. For the rest of
  the session (days of game time), any single flying starling makes about 4 x 1500 = 6000 instance transforms get
  rewritten and uploaded per frame (CPU fill plus GPU-scene upload of roughly 0.4 MB per frame), where a handful is needed.
  The same happens for any flock that was once big.
- Fix: before overwriting `Shown`, keep `const int32 Prev = Shown;` and upload only `[0, max(Want, Prev))`. The instances
  past `Prev` are already hidden. Use `ISM->BatchUpdateInstancesTransforms(0, MakeArrayView(Arr.GetData(), FMath::Max(Want, Prev)), ...)`
  or `TArrayView`. Also shrink occasionally, for example when `Have > 2*Want + 64` for more than about 10 s, by removing
  the tail (one rebuild).

## BIRDS-03 S2: Start() loads every species' pose meshes synchronously on the game thread (med)
- BirdLife.cpp:1124-1235 (ABirdLifeRenderer::Setup, called from Start() inside Tick after StartDelay).
- For every species (36) and every pose (7), times 2 for female sets: `UBirdSpeciesLibrary::LoadPose` (a LoadObject of an
  SM) runs, plus up to 3 `LoadObject<UMaterialInterface>` attempts per material slot for the Winter, Female and
  FemaleWinter variants. That is roughly 250 to 500 synchronous package loads in one frame, and about 500 ISM components
  get registered, including for species that are migrant or have population 0 this month.
- This is the same pattern as the already-fixed "36 SKs at BeginPlay" issue, but for the instanced pose meshes. That fix
  replaced only `bHasBody` with `HasSkeletalMesh`.
- Fix: create a species' `FSpeciesDraw` ISMs lazily the first time a bird of that species needs drawing. Gate it on
  `ULyudPrefetch::FolderReady(this, "/Game/Birds/<Id>", TEXT("_Hero"))` and draw nothing (or the placeholder) until then.
  Skip species with `Population == 0`.
- Related minor bug: winter or female plumage is chosen only once at Setup (`bWinter`, line 1153). After a month boundary
  in a long session the instanced birds keep the old plumage while the bodies (ApplyPlumage) switch.

## BIRDS-04 S2/S3: Draw allocates five N-sized temporary arrays every frame (high existence, med cost)
- BirdLife.cpp:1281-1405 (Draw).
- Per frame the function builds `TArray<TPair<float,int32>> Near`, `TArray<FItem> Items`, `TSet<int32> Keep`,
  `TArray<uint8> Shown`, and `TArray<float> NowPx` (Init N). `FItem` holds an FTransform and is about 112 B. `Items` is
  grown by `Add` with no Reserve, so it reallocates about 12 times up to N of about 2000, which is about 220 KB copied.
  `Shown` is allocated and initialised but never read.
- Cost: about 0.1 to 0.3 ms per frame of allocator and memcpy on the game thread.
- Fix: make them members, `Reset()` them each frame (capacity is kept) or `Reserve(NumBirds)`, and delete `Shown`.
  `Keep` can be an `TArray<int32>` with at most 12 entries, or a bit in `Why`.
- Also: PopLog uses `FString::Printf` per pop (BirdLife.cpp:1386). When a murmuration's reach eases, hundreds of birds can
  pop in one frame. Gate the string behind a debug cvar and keep only the counter.

## BIRDS-05 S3 bug: a flight-encounter bird restarts its clip every frame (high)
- BirdFlightSubsystem.cpp:1296-1336 (TickEncounter).
- `A->SetState(EBirdState::Fly / Hop / Walk / Look)` is called every frame while that branch holds, and SetState does not
  check for the same state. `StartClip` then calls `Play(..., bRestart=true)` and, for loops, sets
  `L.Time = Rng.FRandRange(0, len)`. The result is a random clip frame every tick (the answering bird jitters), `FlightPhase`
  and `FlightSegment` reset each frame, and `StateTime` never grows.
- Fix: `if (A->GetState() != X) A->SetState(X);` in each branch. A guard inside `ABirdActor::SetState` for looping states
  would also work.
- Also there: `UE_LOG(Warning)` every 0.5 s through the whole encounter (line 1290) and in scripts (line 862). Make these
  Verbose.

## BIRDS-06 S3 bug: the predator picks the first prey it finds, not the best (high)
- BirdSim.cpp:2051-2071 (PredatorThink).
- `if (D > 250.f || Best >= 0) continue;` was meant to skip the scan when `RimBird` already chose a bird. Once the loop
  finds any candidate, though, `Best >= 0` is true and every later bird is skipped. The weighting (PreyValue, distance,
  near the player, random) has no effect, so the hawk always takes the lowest-index eligible bird within 250 m.
- Fix: `const bool bRim = Best >= 0;` before the loop, then `if (bRim) skip loop` and drop `Best >= 0` from the continue.

## BIRDS-07 S3: LikeTable's one-entry static cache thrashes (high)
- BirdFeeder.cpp:102-123 (LikeTable). Callers are BirdLife.cpp:547 (every frame while offering, Unknown=1 by default) and
  BirdLife.cpp:1620 (SetFeeder, called by UBirdFeederSubsystem::Tick for each feeder every frame, Unknown=0).
- The cache key includes `Unknown`, and the two callers use different values. With the palm out and any feeder placed,
  or with two feeders holding different feeds, the cache misses at least twice per frame. Each miss constructs 63 FNames
  from UTF-8 (conversion plus name-table hash and lock) and does 2 TMap lookups.
- Cost: about 130 to 250 FName constructions per frame, roughly 20 to 50 us.
- Fix: build the species FName array once (static `TArray<FName>`). Cache per `(Feed, Unknown)` in a small TMap, or have
  `FFeeder` and the player remember the feed whose table they already filled and skip the call when it has not changed.
  SetFeeder should only fill `F.Like` when `Feed` changes.

## BIRDS-08 S2/S3: nest placement does tree-skeleton work on the game thread and its cache never shrinks (med)
- BirdNests.cpp:322-336 (up to 6 `Place()` calls per 0.5 s while new nests come within 160 m) and BirdNests.cpp:210
  (ForEachBranch). BirdPerches.cpp:30-90 (BuildCrowns), 92-122 (Branches and BranchCache), 125-141 (ForEachBranch).
- The first placement calls `BuildCrowns`, which walks every forest tree (ForEachTreeSeeded) and all plants and cableways.
  That is a one-off hitch, scaled by forest size, partway through play.
- Each placement then does a linear scan over all `Trees` (the `Grid` is not used here). For each crown within 6 m it runs
  `FPlantSkeleton::Build` plus `Evaluate`, which costs milliseconds per tree, on the game thread.
- `BranchCache` lives as long as the nest renderer (the whole BirdLife session) and is never trimmed. Its key includes
  `RoundToInt(Age*20)`, so after about 18 game days the same tree gets a new key and the old segment array stays. Memory
  grows with explored area and game time, at thousands of segments x 36 B per tree.
- Fix: in ForEachBranch, use `Grid` (cells around At) instead of all `Trees`. After `Place`, drop the cache
  (`Perches->ClearBranchCache()`), since nests are placed once. Alternatively, cap the cache with LRU or key it without age
  for nests. Spread BuildCrowns or do it in Start behind the loading screen.

## BIRDS-09 S3: SeedHandful marks render state dirty every frame; held items lack the Lumen flags (high)
- BirdFeedItem.cpp:169-200 (SetLeft), called every frame from BirdTame.cpp:813 while Eating and from Tick while falling.
- `C->MarkRenderStateDirty()` runs unconditionally on both ISMs every call, which is a full proxy rebuild each frame. It
  also does `GetInstanceTransform` for all 110 grains every frame.
- The handful (two ISMs) and the feed bag (three cubes, BirdFeedItem.cpp:27-46) ride on the hand bone and move every
  frame. The bag is out whenever feed is equipped and the palm is not offered, which is most of play with feed equipped.
  Neither sets `bAffectDynamicIndirectLighting=false` or `bAffectDistanceFieldLighting=false`.
- Fix: track `bChanged` and call `MarkRenderStateDirty()` (or better, nothing, since UpdateInstanceTransform already
  updates) only when a grain changed. Remember the last `Want` per component and early-out when it is unchanged. Set both
  Lumen flags on the bag parts and the handful ISMs in their constructors.

## BIRDS-10 S3: a feeder's heap creates a new MID and rebuilds instance by instance on every grain change (high)
- BirdFeeder.cpp:291-322 (ShowFeed), called from UBirdFeederSubsystem::Tick for each feeder every frame.
- When the grain count changes (birds eating, so `Handfuls` drops continuously and `Grains` changes every few seconds per
  feeder), the code does `Paint()`, which is a new `UMaterialInstanceDynamic` (the old one becomes garbage). Then it calls
  `ClearInstances()` and `AddInstance` once per grain, up to about 180, each a separate instance-add command.
- Fix: create the MID once in BeginPlay and only `SetVectorParameterValue` when the feed changes. Build a `TArray` and call
  `AddInstances(Xf, false)`. When only the count drops, hide the top instances (the way SeedHandful does) instead of
  rebuilding.

## BIRDS-11 S3 visual bug: near-bird bodies are not on the sun's lighting channel (med-high)
- BirdActor.cpp:21-30 (constructor). Compare BirdLife.cpp:1183 (`ISM->LightingChannels.bChannel1 = true`) and
  LyudSky.cpp:79-80 (the sun and moon light channel 1 only).
- ABirdActor's skeletal `Body` keeps the default channel 0. The 12 nearest birds, the ones drawn as bodies within 25 m and
  the ones you tame, get no direct sun or moon outdoors. Instanced birds do. A bird visibly changes brightness when it
  switches between ISM and body. AnimalActor.cpp:253 and PeopleLife.cpp:1131 set the channel; birds do not.
- The feed bag and handful are lit only if ALyudSky re-toggles indoor or outdoor after they were attached (LyudSky.cpp:769
  returns early when nothing changed), so a bag spawned outdoors stays channel 0.
- Fix: `Body->LightingChannels.bChannel1 = true;` in the ABirdActor constructor (bodies are outdoor birds). For the bag and
  handful, copy the hero mesh's lighting channels in AttachToPalm.

## BIRDS-12 S3: every tier-0 bird traces a sight line every frame (med)
- BirdSim.h:290 (`ThinkNear = 0`), BirdSim.cpp:2236-2241 (tier-0 birds Think every frame), BirdSim.cpp:1636-1641
  (PerceivePlayer).
- Every bird within 40 m that has the player inside its field of view fires `Sight`, which is `World->LineTraceSingleByChannel`
  plus `GetPlayerPawn`, every frame. The door flock (6) plus the flocks around the club or a feeder make that tens of
  traces per frame, at roughly 5 to 15 us each.
- Fix: cache the result per bird for 0.1 to 0.2 s (store `SeenUntil` and `bSeenCache` in FBird), or give tier 0 a small
  ThinkNear such as 0.05.

## BIRDS-13 S3: ForNear takes a std::function (med)
- BirdSim.cpp:2127, BirdSim.h:430. Called for each tier-0 or tier-1 bird on each think (PerceiveBirds at 1805 and others)
  and in FCorvids.
- The `[&]` lambdas capture several references. Above libc++'s small-buffer size, each `std::function` construction
  heap-allocates, which means hundreds of small mallocs per frame near flocks.
- Fix: make `ForNear` a template (`template<class F> void ForNear(const V3&, float, F&& Fn) const`) or use a
  `TFunctionRef`-like non-owning view.

## BIRDS-14 S3: the player's body animation does about 150 Printf/FName/bone lookups per evaluation (med)
- BirdAnimInstance.cpp:200-274 (ApplyWingsLocal), 276-325 (ApplyWingsCS), 486-560 (ApplyGrip), 381-392 (streamer), 173-180
  (RotateCS allocates a TArray per call).
- When `Wings.Weight > 0` (the player's bird, from BirdPawn.cpp:1333 SetWingPose) and when gripping, each evaluation
  builds bone names with `FString::Printf`, converts them to FName, and calls `GetPoseBoneIndexForBoneName`. That is about
  2 x (4 + 12 + 24 + 5) + 16 + 30 calls. It also decompresses two full extra poses (`SampleAt0` for FoldSeq and TuckSeq)
  every frame.
- Cost: one bird only, so about 50 to 100 us of anim-worker time.
- Fix: cache the compact bone indices in the proxy keyed by `Bones.GetSerialNumber()`. Cache the Fold and Tuck poses when
  the sequence or bone container changes. Make RotateCS use a `TArray<FBoneTransform, TInlineAllocator<1>>`.

## BIRDS-15 S3: small per-frame items (high existence, small cost)
- BirdTame.cpp:349-377 (TellBirds): `TActorIterator<AStarlingMurmuration>` runs every frame while the palm is offered or
  eating. Cache a `TWeakObjectPtr`.
- BirdTame.cpp:860-866 (UpdateHud): `LyudText::Get` with format args, `ItemName().ToString()` and `Hud->Set` run every
  frame whenever an item is equipped. Same in BirdFeeder.cpp:510-531 (UpdatePrompt) every frame next to a feeder. Rebuild
  only when the item, count or key changes.
- BirdLife.cpp:604-608: `TActorIterator<ALyudSea>` runs every frame on maps without a sea (hall, test levels), because
  `Sea` stays invalid. Remember that the search was done (a bool, or retry once a second).
- BirdActor.cpp:358-394 (TickFlight): `Clip(TEXT("Flap"))` and `Mix(TEXT("Flap"), ...)` build 6 FNames per flying body per
  frame. Use static const FNames.
- BirdNests.cpp:564-647 (DrawMovers, every frame): a `TMap` plus an `FString::Printf` and FName per carrying bird or downy
  chick within 80 m, and `FString(TEXT("MuteSwan"))` per chick. BuildStatic (every 0.5 s) does Printf and FName per nest in
  range and rebuilds the `Nearest` debug string with Printf for each nearer nest. Cache the mesh FNames per
  (species, stage) in a table.
- BirdLife.cpp:1393 calls `ULyudPrefetch::FolderReady` per body candidate per frame until the species is loaded.
  [OTHER-THREAD] World/LyudPrefetch.cpp:109-113: every FolderReady call re-runs `FindObject` by string path plus
  `ObjectPathToPackageName` plus FName for every asset in the folder, even after all of them are loaded. Remember
  "folder complete" in `Folders` and return true at once.

## BIRDS-16 S3: an approaching bird gets its body without the prefetch gate (low-med)
- BirdLife.cpp:683-690 (DispatchEvents, EEvent::Approach) calls `Renderer->EnsureBody` directly.
- This bypasses the `FolderReady` check that Draw uses (line 1393). If the species is not loaded yet, SetSpecies does a
  synchronous LoadObject of the SK, 15 clips and plumage MIs on the frame the bird starts to the hand. The fix: check
  FolderReady there as well, and let Draw create the body a frame later.

## BIRDS-17 S3: the player bird's solid parts move every frame without the Lumen flags (med)
- BirdSelfBody.cpp:286-296 (AddPart): torso, head, beak, legs and toes, about 15 static mesh components.
- They are re-posed and moved every frame while flying or walking as the bird. The feather ISMs (line 359) have the flags;
  these parts do not. Set `bAffectDynamicIndirectLighting=false` and `bAffectDistanceFieldLighting=false` in AddPart.

---

## Checked and fine (no finding)
- ABirdNestRenderer::Commit and AnimateBroods: the hash early-out works, and UpdateInstanceTransform is called with
  bMarkRenderStateDirty=false and only on change.
- ACorvidItemsRenderer::Draw: compares against the last frame before updating. Items are capped at 4000 with a free list.
- UBirdWindStreaks, the murmuration grid and kNN, the BirdSim spatial hash, and the FNesting young pool (no growth).
- The ISMs in the BirdLife, nest, corvid and feather renderers all set the Lumen flags.
- UBirdTameSubsystem: Bag and Handful are UPROPERTY, delegates are removed in Deinitialize, and the FTSTicker is test-only
  with a weak world check.
- UBirdFlightSubsystem: the save delegates are removed in Deinitialize.
- BirdLife save lambdas use AddWeakLambda.

---

## Fixes applied
(Paths relative to unreal/Lyudmilovsk/Source/Lyudmilovsk. Nothing built or run here.)

- BIRDS-01 done. Added `ABirdLifeRenderer::EndPlay` (BirdLife.h/.cpp). On Destroyed it destroys every body in `Bodies`, then resets Bodies, BodyActs and BodyBorn. Verify: toggle `lyud.Birds 0` then `1` (or `lyud.Birds.Nests`) near birds. `stat` / the actor count should show no leftover ABirdActor, and no frozen birds should stay in place.
- BIRDS-02 done. `ABirdLifeRenderer::Sync` now uploads only `[0, max(Want, last Shown))`, because the tail past that is already hidden. Shrinking the ISMs was not done, since it needs a rebuild and is not needed for the win. Verify: after a murmuration, `stat gpuscene` / Insights should show a few instance updates per frame instead of ~6000. The flock should look the same.
- BIRDS-03 skipped. Lazy ISM creation gated on FolderReady would make species draw nothing until they are loaded, so birds would pop in at start. It also touches Setup, Draw and Describe. That is not a small, safe change. The winter/female plumage chosen once at Setup is also left as is.
- BIRDS-04 done. Near, Items (now Reserve(N)), Keep (a TArray<=12 instead of a TSet) and NowPx are members that keep their capacity between frames. The unused `Shown` array is gone. PopLog now formats only the last 12 pops of a frame, which are the only ones kept, so the report text is identical. Also BIRDS-15's FolderReady path string is cached per species (`FSpeciesDraw::Folder`). Verify: `lyud.Birds.Report` shows the same pop counts and log, with less game-thread time in Draw.
- BIRDS-05 done. In `UBirdFlightSubsystem::TickEncounter`, Fly, Hop, Walk/Hop (court) and Look call SetState only when the state differs. Peck and Call were already one-offs. The "BIRD meet" log is now Verbose. The script trace log "BIRD t=... step" is left at Warning, because it is the test scripts' readout. Verify: call a bird as the player bird. The answering bird's clip should play smoothly with no jitter.
- BIRDS-06 done. `PredatorThink` sets `bRim` once, skips the scan when a rim bird was picked, and drops `Best >= 0` from the continue. The weighting now picks the best prey. Verify: hawk hunts in the log/report should take varied prey, not the lowest index.
- BIRDS-07 done. In `LyudBirdFeeds::LikeTable` (BirdFeeder.cpp), the species FNames are built once, and a static cache keyed by (Feed, Unknown) stores 64-float rows. The feeds file is read once, so a row never goes stale. SetFeeder is left as is, since its call is now a map find plus a 256 B copy. Verify: same feeding behaviour, and no FName construction per frame with the palm out and a feeder placed.
- BIRDS-09 done. `ASeedHandful::SetLeft` remembers the grains shown per ISM (`SeedsShown` / `MilletShown`, set in Build). It returns early when nothing changed, hides only the newly gone grains, and no longer calls MarkRenderStateDirty. The bag's cubes and the handful's ISMs set bAffectDynamicIndirectLighting and bAffectDistanceFieldLighting to false. Verify: the handful still shrinks while a bird eats and still falls when spilled. No proxy rebuild per frame (`stat scenerendering` / Insights).
- BIRDS-10 done (small version). `ABirdFeeder::ShowFeed` creates the heap MID once and recolours it when the feed changes (`HeapMat`, `HeapFeed`). The grains go in through one `AddInstances`. The heap is still rebuilt on a grain-count change, because radius and height depend on the count, so hiding instances would change the look. Layout and RNG order are unchanged.
- BIRDS-11 done. The ABirdActor constructor sets `Body->LightingChannels` to channel 0 + 1. In BirdTame AttachToPalm, the bag's and handful's primitives copy the hero mesh's lighting channels when they are attached. Verify: a bird switching from instance to body outdoors no longer changes brightness, and the feed bag is sunlit outdoors.
- BIRDS-12 done. Each FBird caches its sight-line result for 0.15 s (`SightUntil`, `bSightClear` in BirdSim.h, used in PerceivePlayer). Verify: far fewer BirdSight traces (`stat collision` / Insights) near the door flock, with the same reactions.
- BIRDS-15 done (most of it). BirdTame::TellBirds caches the murmurations when the palm goes out or in, instead of a TActorIterator each frame. BirdTame::UpdateHud formats the texts only when the item, count, hint, icon or language changes. In BirdFeeder UpdatePrompt, the text is formatted only on a change, but SetPrompt is still called every frame because the prompt line is shared with other systems. In BirdLife Tick, the search for the sea is retried once a second. BirdActor::TickFlight uses static FNames. BirdNests DrawMovers uses a static nest-material name table and a cache of chick mesh names, and compares MuteSwan with Strcmp. Not done: the BuildStatic Printf (it runs every 0.5 s) and LyudPrefetch's FolderReady (another agent owns it).
- BIRDS-16 done. DispatchEvents' Approach calls EnsureBody only if the new `ABirdLifeRenderer::BodyLoaded` passes: the body already exists, the species has no SK, or FolderReady holds. Otherwise Draw makes the body a frame later, since an approaching bird is first in line. Verify: no hitch when a not-yet-loaded species flies to the palm.
- BIRDS-17 done. `UBirdSelfBody::AddPart` sets both Lumen flags to false.
- BIRDS-08, 13, 14: not assigned.
