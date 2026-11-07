# Round 2: save/load, quests, journal, bird book (r2_save)

Scope: the whole module (`unreal/Lyudmilovsk/Source/Lyudmilovsk`, paths below are relative to it), defect class "save/load
correctness and robustness" plus sync loads during play that round 1 missed. Read-only: nothing built or run. Round-1
items (UI-08 HeroSeat sequence actor, W8, T13, people/animals SetBreed, Europa music, UI-11 DiaryStyle statics, ...) are not
repeated.

How a save works today (LyudSave.cpp): `SaveToSlot` writes the player, then every ILyudSaveable actor by `GetFName()`, then
every `RegisterSaveObject` object (`WriteObject`), and **only then** broadcasts `OnBeforeSave`. `LoadSlot` opens the level.
`NotifyWorldBeginPlay` (from the menu world subsystem) schedules `ApplyPending` for the next tick. `ApplyPending` restores the
clock, the player, the actors by name, the registered objects, then broadcasts `OnAfterLoad`. In UWorld::Tick timers run before
tickable objects (LevelTick.cpp:1812-1821), so `ApplyPending` always runs before any tickable subsystem's first tick.

---

## SV-1 [S2, high] Registered objects are written before OnBeforeSave, so four systems save stale data
- `Private/LyudSave.cpp:129-140`: the `SaveObjects` loop (`Save->WriteObject`) runs, then `OnBeforeSave.Broadcast(Save)`.
- These systems copy their live state into their `UPROPERTY(SaveGame)` fields from **OnBeforeSave**, after their record has
  already been serialized:
  - `Private/Birds/BirdLife.cpp:245`: `SyncTrust(true)` fills `SavedTrust` (bird trust)
  - `Private/Animals/AnimalLife.cpp:200`: `SyncMemory(true)` fills `Saved` (cat/dog trust, bond, the names the player gave them)
  - `Private/Birds/CorvidLife.cpp:79`: `SyncFeelings(true)` fills `SavedBond` / `SavedGrudge`
  - `Private/People/PeopleLife.cpp:175`: `SyncMemory(true)` fills `Homes` / `Memory` (needs)
- What happens: each save holds the state as it was at the **previous** save in the session, or the load or start values. The
  first save of a session gets empty trust and memory, and `PeopleLife` gets an empty Homes map. Anything earned since the
  last save is lost.
- BirdFlightSubsystem is not affected, because it calls `WriteObject` itself inside its OnBeforeSave (BirdFlightSubsystem.cpp:915).
- Fix (one line): in `SaveToSlot`, move `OnBeforeSave.Broadcast(Save);` up to just before the `SaveObjects` loop, after the
  player fields. BirdFlight's PlayerTransform override still works there. Alternative: make the four classes ILyudSaveable and
  sync in `OnGameSaving`, which the loop already calls before `WriteObject`.
- Test: tame a bird or name a cat, `lyud.Save a`, `lyud.Load 0`, then `Birds.Book` / an animal describe. Today it comes back
  untamed. None of the save tests (test_bird_diary, test_doors, test_ui, test_quest_flow, test_bikes, test_unique_people)
  checks trust or animal memory.

## SV-2 [S2, high] BirdLife and AnimalLife register their save objects 0.5 s too late, so a load never reaches them
- `Private/Birds/BirdLife.cpp:241-246` and `Private/Animals/AnimalLife.cpp:196-201` call `RegisterSaveObject` (and bind
  OnAfterLoad/OnBeforeSave) inside `Start()`. `Start()` runs from Tick after `StartDelay` = 0.5 s / 0.6 s (BirdLife.cpp:163,
  AnimalLife.cpp:127).
- `ApplyPending` runs on the first tick. At that point `SaveObjects["BirdLife"]` / `["AnimalLife"]` still holds the previous
  world's subsystem as a dead weak pointer, so `ReadObject` is skipped. The OnAfterLoad lambdas are not bound yet. `Start()` then
  restores from the default (empty) `SavedTrust` / `Saved`.
- Result: on every load, bird trust and cat/dog trust, bond and **names** are reset, independently of SV-1. Book friends stay
  in the bird book, but their birds no longer trust the hero.
- PeopleLife does this right: it registers in `OnWorldBeginPlay` "so a load restores Homes and Memory before the life starts"
  (PeopleLife.cpp:162-176). Corvids also register in OnWorldBeginPlay and reapply when the sim starts (CorvidLife.cpp:119-125).
- Fix: move the `RegisterSaveObject` and both delegate bindings into `OnWorldBeginPlay` (when `bWantStart`). `Start()` keeps
  `SyncX(false)` and so picks up the loaded values.
  - This also stops the duplicate delegate bindings that pile up on each restart (`lyud.Birds 0/1`, the `lyud.Birds.Nests`
    switch at BirdLife.cpp:589-594, `lyud.Animals 0/1`).
  - While there: `Stop()` (BirdLife.cpp:358, AnimalLife.cpp:230) should `SyncTrust(true)` / `SyncMemory(true)` before
    `Sim.Reset()`. Otherwise a restart drops the trust earned since the last save (S3, dev/bench only).

## SV-3 [S1 crash, high] Quest progress from a save is not checked against quests.json
- `Private/UI/LyudQuests.cpp:490`: `int32& C = Progress[QI].Counts[I];` loops over the stage's objectives in the current
  quests.json. `Counts` comes from the save.
  - If a stage gains an objective after the save was made (quests.json is edited all the time in development), this indexes
    past the end. In dev builds that is a TArray range-check assert. In shipping it writes out of bounds.
  - The first event after loading such a save triggers it.
- `:389` (`FinishStage`) and `:364` (`BeginStage`) do `Def.Stages[P.Stage]` with no check. `Advance` (`:523`, the
  `Quest.Advance` command) reaches it with a loaded `Stage` that is out of range.
- A loaded Stage beyond the def's stages is skipped forever by HandleEvent (`:470`). The quest stays Active and cannot be
  finished (a soft-lock).
- Fix: sanitize once after load, in the existing OnAfterLoad lambda (`:275`), before `OnChanged.Broadcast()`. For each
  `Progress`:
  - If there is no def, leave it.
  - If Active and `!Stages.IsValidIndex(Stage)`, set `Stage = Stages.Num()-1`.
  - `Counts.SetNumZeroed(Stages[Stage].Objectives.Num())` (keeps existing values).
  - Also guard `Advance` with `IsValidIndex`.

## SV-4 [S2 in packaged builds / S3 in -game, med] Runtime-spawned saveable actors are keyed by auto-generated names
- The save keys actors by `GetFName()` (LyudSave.cpp:126, :252). These saveable actors are spawned at runtime with no
  `FActorSpawnParameters::Name`:
  - Forest live plants: `Private/Plants/ProceduralForest.cpp:313`. They are spawned by camera distance and destroyed again at `:277`.
  - Cableway planters: `Private/Transport/LyudStationFurnish.cpp:107`.
  - Island plants: `Private/Railway/LyudRailTrack.cpp:566`.
  - Station doors: `Private/Transport/LyudStationBuild.cpp:417`, `:1135`.
- Editor `-game`: names come from a per-level counter. They are stable only if the spawn order is the same.
  - Forest live plants follow the camera, so a "ProceduralPlant_37" record lands on a different tree after load. Its
    `PickedFruit` keys then hide fruit on the wrong tree, and the real tree regrows its fruit.
  - The live plant is also destroyed when the camera moves away (`:277`), which drops `PickedFruit` even without any save.
- Cooked/packaged builds: there is no editor-only data, so `MakeUniqueObjectName` takes the fast path (`--NameNumberUniqueIndex`,
  a global counter, Engine `UObjectGlobals.cpp:2686-2703`, `CoreGlobals.cpp:144-148`). Names never match across a level reload.
  **Every station door's open/locked state and every runtime plant's fruit is lost on load** in a shipped game.
- Fix:
  - Give them deterministic names, as the bikes do (LyudBikeSubsystem.cpp:239-241) and the airlock does (LyudEuropa.cpp:1211),
    e.g. `Door_<station>_<i>` and `Planter_<station>_<i>`.
  - For forest live plants: keep `PickedFruit` per `FTree` in the forest. Copy it out on Destroy and back in on spawn, and save
    it on the forest (make AProceduralForest ILyudSaveable with a `UPROPERTY(SaveGame) TMap<int32, FIntArray>`).
  - Skip forest-owned plants in `SaveToSlot` (`It->GetOwner()` is an AProceduralForest).

## SV-5 [S3, high] AnimalLife never clears `Saved` before writing it
- `Private/Animals/AnimalLife.cpp:532-543`: `Saved.FindOrAdd(A.Id)` only for animals with trust, bond or a name. It never
  calls `Reset()`.
- An animal whose trust fell back to 0 (AnimalSim.cpp:1872 lowers it) keeps its old, higher values in the map. The next save
  writes them, and the load gives the trust back.
- Fix: call `Saved.Reset();` before the loop when `bToSave`, as BirdLife.cpp:787 does.

## SV-6 [S3, med] Bird trust and friends are keyed by sim index, and the population depends on the date and on cvars
- `SavedTrust` (BirdLife.cpp:797-802) and `FLyudBirdFriend::BirdId` use `FBird::Id`, which is the index in `Birds`.
- `FWorld::Populate` (BirdSim.cpp:628-700) sets each species' count from migration, stragglers and `DayOfYear`, and from
  `lyud.Birds.Population`.
  - Winter flock sizes (`IsWinter`) change how many `Rand(R)` draws each group takes.
  - So the Ids, and each bird's `B.Rng` (sex, boldness), shift when a save is loaded in another month or season than the
    session that made it, or with another population scale.
- Trust then lands on other birds (even other species). The bird book's friend `BirdId` points at a different bird.
  `Sexes` and `bFemale` disagree with the live bird.
- Fix (cheapest): store a population signature (seed, scale, and the per-species created counts hashed) next to `SavedTrust`,
  and drop the trust when it differs. Better: key by (species, ordinal in species), and derive `B.Rng` from (Seed, species,
  ordinal) instead of the chained R.

## SV-7 [S3, med] The clock and the calendar are wrong for one frame after a load
- `Private/LyudSave.cpp:211-224`: when a save is pending, `NotifyWorldBeginPlay` leaves `ClockBase` as it was (the previous
  session's time, or the time spent in the menu on "Continue"). `UGameCalendar::OffsetSeconds` (a world subsystem) is still 0.
  Both are fixed only in `ApplyPending` on the next tick (`:235` and the "Calendar" record).
- Everything that reads `UGameCalendar::NowIn` in BeginPlay/OnWorldBeginPlay builds for the wrong date and time, then jumps.
  - Example: ProceduralPlant shapes in BeginPlay and does the whole `Apply` again in `OnGameLoaded`.
  - Example: weather and sky start at the wrong date.
- Fix: in `NotifyWorldBeginPlay`, if `Pending`, set `ClockBase = Pending->GameTimeSeconds - World.GetTimeSeconds()` right away.
  Then either have `UGameCalendar::OnWorldBeginPlay` read its record from the pending save (a `const ULyudSaveGame*
  GetPending()` accessor plus `ReadObject("Calendar", this)`), or apply the "Calendar" record in `NotifyWorldBeginPlay`.
  World subsystem OnWorldBeginPlay order is not fixed, so the calendar pulling it is the safe option.

## SV-8 [S3, high] A pending save is applied to whatever map opened; the version is never checked
- `LoadSlot` (LyudSave.cpp:160-172) opens `Save->Map`. If that map no longer exists (renamed or removed), the travel fails
  and the engine opens the default map.
- `NotifyWorldBeginPlay` then clears `bTravelling` and `ApplyPending` restores the save into the wrong level. The player is
  teleported to foreign coordinates, and any actor names that happen to match get the records.
- `ULyudSaveGame::Version` (LyudSave.h:60) is written and never read.
- Fix: at the top of `ApplyPending`, `if (MapOf(World) != Save->Map) { UE_LOG(Warning); return; }`. Also refuse in `LoadSlot`
  when `Save->Version > CurrentVersion`, and leave a hook for migrating older versions.

## SV-9 [S3, med] If the slot index cannot be read, new saves overwrite old slots
- `SaveToSlot` names a new slot `lyud_save_%03d` from `Index->NextId` (LyudSave.cpp:100-105). If `lyud_index` is missing or
  unreadable, `LoadIndex` returns a fresh index with `NextId = 1`, and the next save silently overwrites the existing
  `lyud_save_001`.
- Fix: `while (UGameplayStatics::DoesSaveGameExist(Slot, 0)) { Slot = Printf(..., Index->NextId++); }`. Optionally rebuild
  the index from the files that exist (desktop only).

## SV-10 [S3, med] Every procedural plant is saved, and every one is fully reshaped on load
- AProceduralPlant is ILyudSaveable (ProceduralPlant.h:27), so every plant in the level gets a record, even with nothing picked.
- `OnGameLoaded` (ProceduralPlant.cpp:1071-1080) calls the full `Apply(...)` (skeleton, wood, leaves) for each of them in the
  `ApplyPending` frame. That is a load-frame hitch of N rebuilds, on top of the BeginPlay build (and SV-7 makes the BeginPlay
  one use the wrong date).
- Fix:
  - In `OnGameLoaded`, only call `ClearFallenFruit()` and re-lay the fruit when `PickedFruit.Num() > 0`.
  - Otherwise set `LastSeasonDay = -1000.f` and let the plant's own throttled tick refresh it, which spreads the cost.
  - With SV-7 fixed, the BeginPlay build already has the right date.
  - Optionally skip saving plants with `FruitPicked == 0 && PickedFruit.IsEmpty()`. That needs a small "has state" virtual on
    the interface, checked in SaveToSlot.

## SV-11 [S3, low] Loaded values are not range-checked
- `PeopleLife::SyncMemory(false)` (PeopleLife.cpp:468-477) copies `Needs` as is.
- `AnimalLife` copies Trust and Bond, `BirdLife` copies trust, and `BirdFeeder` (`States`: Location, Handfuls) and
  `GameCalendar` (`TimeScale`, `OffsetSeconds`) are used as loaded.
- A corrupt or hand-edited slot with NaN or negative values goes straight into the sims (NaN positions or needs propagate).
- Fix: clamp floats to their real ranges (needs and trust 0..1, Handfuls >= 0, TimeScale >= 0). Drop feeder states whose
  `Location.ContainsNaN()`, and treat a NaN `OffsetSeconds` as 0.

---

## Sync loads during play that round 1 did not list

## SL-1 [S3, high] Player-bird calls do a LoadObject per call, uncached
- `Private/Birds/BirdPawn.cpp:1497-1505` (`PlayCall`, on the call key) and `Private/Birds/BirdFlightSubsystem.cpp:1277-1286`
  (the `Sound` lambda in TickEncounter): Printf of the path plus `LoadObject<USoundBase>` on every call.
- Nothing holds a reference. The first call loads synchronously from disk, the GC can drop it between calls, and a call asset
  that is listed but missing means a failing package lookup every time.
- BirdLife already has a per-species cache (`Sounds` / `SoundRefs`, BirdLife.cpp:734-750).
- Fix: expose that cache, e.g. `UBirdLifeSubsystem::CallSounds(FName Species)`, or keep a
  `TMap<FName, TArray<TObjectPtr<USoundBase>>>` in UBirdFlightSubsystem filled in `BecomeSpecies`.

## SL-2 [S3, high] Bird book drawings: a sync texture load as the page builds, plus static strong pointers
- `Private/Birds/BirdBook.cpp:190-215` (`ULyudBirdBook::Drawing`, from SLyudDiary.cpp:846 BirdCard): `LoadObject<UTexture2D>`
  of `T_Bird_<Species>[_f]` (42 files, 7.6 MB in UI/Diary/Birds) as each Birds page is built and turned to.
- The cache is a function-local `static TMap` of `TStrongObjectPtr<UTexture2D>` plus a `TSharedPtr<FSlateBrush>`. That is the
  same exit-time static UObject destruction that UI-11 fixed in LyudDiaryStyle, missed here.
- Fix: use the `Keep()` rooted-raw-pointer pattern from LyudDiaryStyle.cpp. When the diary opens (or on `NoteSeen`), request the
  seen species' drawings through ULyudPrefetch or an async load.

## SL-3 [S3, med] Winter reads 1.8 MB and parses JSON on the game thread when snow first settles
- `Private/Winter/LyudWinter.cpp:651-656` (`StepCover`, while `Refining` turns into `Building`): `LoadFileToArray` of
  AnimalsData/animal_world.bin (1.8 MB) and `LoadFileToString` + `FJsonSerializer::Deserialize` of animal_places.json, on the
  game thread mid-play.
- Fix: do both reads inside the worker job the data is handed to on the next lines, or read them once in OnWorldBeginPlay.

## SL-4 [S3, low] Interaction-time loads
- `Private/HeroSeat.cpp:113`: `SitSequence.LoadSynchronous()` when the hero sits (and on load while seated).
- `Private/UI/LyudGameScreen.cpp:553` / `:562`: `T_WorldMap` (550 KB) and `T_Binoculars` load on the first map or binoculars use.
- Fix: warm them in BeginPlay / `ULyudGameScreen::OnWorldBeginPlay`, or async-load them.

The other ~150 LoadObject / LoadSynchronous / TryLoad hits were checked:
- They are in constructors, BeginPlay, OnWorldBeginPlay or Setup/Build.
- Or they are cached after the first call:
  - LyudText (failed-path set)
  - BirdLife `Sounds`
  - BirdNests `Missing`
  - LyudBite `Sounds`
  - ULyudBodyAnimInstance `PcClips`
  - LyudWind `bCollectionLooked`
  - Winter decal (only until it exists)
  - SmokeFx (first puff only)
- Or round 1 already listed them: BoothOwner, Europa music, train gulls, SnowNear, AnimalActor SetBreed, NpcBody, bird poses
  and plumage, DiaryStyle, TerrainMask.
- One small leftover: `ULyudBodyAnimInstance::UpdateComputer` (LyudBodyAnim.cpp:602-610) retries a missing PC clip with a
  failing LoadObject every 5 s per seated NPC. Harmless while every clip exists.

---

## Checked and clean
- **Delegate lifetimes**:
  - GI subsystems (Quests, BirdBook) bind OnAfterLoad once in Initialize.
  - World subsystems bind per world with weak lambdas or handles, and remove them in Deinitialize: GameScreen, BirdFlight
    (handles), Smoking (weak, one per world).
  - No double binding on load, except the BirdLife/AnimalLife restart case in SV-2.
- **New game / load reset of GI state**:
  - Inventory: ULyudInventoryUI::OnWorldBeginPlay.
  - Quests and BirdBook: ULyudGameScreen::OnWorldBeginPlay.
  - NpcDialogue TalkCounts: UNpcTalkSpawner.
  - Smoking: ULyudSmoking.
  - SunGlare and Breath: ResetFor guards.
  - All of these run before `ApplyPending`, so nothing is restored twice or leaks from the old session.
  - BirdGuide `bMapLayer` is a preference and is rightly not reset.
- **Unbounded growth**:
  - Journal entries are bounded by content (stage logs, places, species, tamed birds).
  - Feeder states are bounded by the feeders placed.
  - PeopleLife Memory is bounded by the roster.
  - `PickedFruit` grows only as fruit is picked.
  - The slot index grows only with user saves.
- **File IO / JSON in play**:
  - No JSON parse runs per frame.
  - Data files are read once per level start or cached: quests/map, lobby (round 1), food, feeds, species, animal bodies,
    lip-sync.
  - Save, load, delete and GetSlots disk IO happens only on menu actions.
- **Order on load**: ApplyPending restores, then calls OnGameLoaded, then broadcasts OnAfterLoad. The doors, bikes, feeders and
  airlock OnGameLoaded handlers snap their state without animating.
  - Europa handles a load that lands on the far site (LyudEuropa.cpp:1286-1291).
  - The airlock and the bikes have stable names.

---

## Fixes applied

(paths relative to `unreal/Lyudmilovsk/Source/Lyudmilovsk`; nothing built or run)

- **SV-1 done.** `Private/LyudSave.cpp` `SaveToSlot`: `OnBeforeSave.Broadcast(Save)` now runs after the player fields and
  the actor records, just before the `SaveObjects` write loop. All five listeners checked: BirdLife `SyncTrust(true)`,
  AnimalLife `SyncMemory(true)`, CorvidLife `SyncFeelings(true)`, PeopleLife `SyncMemory(true)` only fill their own SaveGame
  fields; BirdFlight overrides `bHasPlayer`/`PlayerTransform`/`ControlRotation` (set before the broadcast, so its override
  still wins) and writes its own record (it is not in `SaveObjects`). None depends on running after the write.
  Verify: tame a bird / name a cat, `lyud.Save a` straight away (inside the 2 s sync timer), `lyud.Load 0`: the trust and
  the name are back. PeopleLife's first save of a session now has Homes.
- **SV-2 done.** `Private/Birds/BirdLife.cpp` `OnWorldBeginPlay` and `Private/Animals/AnimalLife.cpp` `OnWorldBeginPlay`
  now register `"BirdLife"` / `"AnimalLife"` and bind OnAfterLoad/OnBeforeSave once per world (only when the map starts the
  life, as before); removed from `Start()`. Load path checked: `ApplyPending` (next tick) `ReadObject`s into `SavedTrust` /
  `Saved` while `Sim` is null; the OnAfterLoad `SyncX(false)` returns early (no sim); `Start()` 0.5/0.6 s later calls
  `SyncTrust(false)` / `SyncMemory(false)` after `Populate()` and applies them. A save before the sim starts keeps the loaded
  values (`SyncX(true)` returns early without a sim). `Stop()` in both now syncs to the saved fields before `Sim.Reset()`, so
  `lyud.Birds 0/1`, the `lyud.Birds.Nests` restart and `lyud.Animals 0/1` keep trust/names, and no delegates pile up.
  Verify: save with a tamed bird and a named cat, `lyud.Load 0`, wait 1 s, `lyud.Animals.Report <id>` shows the name/trust,
  the tamed bird comes to the palm.
- **SV-3 done.** `Private/UI/LyudQuests.cpp`: the OnAfterLoad lambda in `Initialize` clamps each active quest's `Stage` into
  its def's stages and `Counts.SetNumZeroed(objectives)` (keeps existing counts) before `OnChanged`. `BeginStage` returns
  (counts cleared) on a bad stage; `FinishStage` clamps the stage (so the skip-ahead `while` loop always moves on) and does
  nothing for a def with no stages; `HandleEvent` resizes `Counts` to the stage's objectives before indexing; `Advance`
  skips defs without stages. Verify: edit a save's quest Stage to 99 or add an objective to the current stage in
  quests.json, load: no assert, the quest sits on its last stage and `Quest.Advance` finishes it.
- **SV-5 done.** `AnimalLife::SyncMemory`: `Saved.Reset()` before refilling when `bToSave`.
- **SV-9 done.** `SaveToSlot`: a new slot name skips any `lyud_save_%03d` that `DoesSaveGameExist` already, so a lost
  index never overwrites. Verify: delete `lyud_index.sav`, `lyud.Save x`: a new file, the old 001 is intact.
- **SV-11 partly done.** `ApplyPending`: a non-finite/negative `GameTimeSeconds` becomes 0, a NaN `ControlRotation` /
  `PlayerTransform` is not applied. BirdLife trust, AnimalLife trust (0..1) and bond (>=0), corvid bond/grudge (0..1) are
  clamped and NaN dropped on restore. Not done (not my files): PeopleLife `Needs` (PeopleLife.cpp was allowed only for SV-1),
  BirdFeeder `States`, GameCalendar `TimeScale`/`OffsetSeconds`.
- **SL-1 done.** New `UBirdLifeSubsystem::CallSound(Species, Call)` (`Public/Birds/BirdLife.h`, `Private/Birds/BirdLife.cpp`):
  loads a call once, keeps it in `SoundRefs`, caches misses as null. `ABirdPawn::PlayCall` (`Private/Birds/BirdPawn.cpp`,
  + include `Birds/BirdLife.h`) and the `Sound` lambda in `UBirdFlightSubsystem::TickEncounter` use it. Same call choice
  as before (random call / `Calls[0]`). Verify: fly a bird, press the call key repeatedly: `lyud.Birds.Report` "sounds
  loaded" grows once per call, no LoadPackage in `-LogCmds="LogStreaming Verbose"` after the first.
- **SL-2 + GC-2 done.** `Private/Birds/BirdBook.cpp` `Drawing`: the static cache keeps `UTexture2D*` rooted with
  `AddToRoot()` (the LyudDiaryStyle `Keep()` pattern), no `TStrongObjectPtr` (include dropped). `NoteSeen` for a newly met
  species starts `ULyudPrefetch::Ready` on its drawing(s) that exist (`FPackageName::DoesPackageExist` first, so no async
  warning for missing `_f`), so the Birds page usually finds them in memory. Not done: prefetching on diary open for species
  from a loaded save (needs SLyudDiary). Verify: J -> Birds, page through, quit: no shutdown assert.
- **GC-3 (BirdLife part) done.** `ABirdLifeRenderer::FSpeciesDraw::Poses` is now `TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>>`
  (`Public/Birds/BirdLife.h`); `Draw`'s sync loop and `Describe` skip a null one. ~500 weak gets per frame, negligible.

Save tests and trust: none of the save tests checks trust. `unreal/scripts/test_bird_flight_save.sh` checks only the
bird/hero transform and should stay so. `test_bird_diary.py` is the one that should: it already tames three birds
(`Birds.Meet ... tame`), saves, `Birds.Forget`s and loads; it should also log the friends' sim trust after the load
(e.g. a `lyud.Birds.Report <id>`-style line or a python read of `UBirdLifeSubsystem`) and fail if it is back to 0, with a
`lyud.Save` right after taming (before the 2 s sync) to cover SV-1. An animal case (name a cat, save, load,
`lyud.Animals.Report <id>`) belongs there or in test_ui.
