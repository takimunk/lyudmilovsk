# Round 2: tick (per-frame cost, cross-cutting)

Read-only pass over `Source/Lyudmilovsk` on branch perfhunt, with the round-1 fixes in. I verified each hit by reading
the code. Instance counts come from `blender/export/scene01_layout.json`, `blender/export/academgorodok/layout.json`,
the import scripts and the spawn sites. I also counted log lines in a real `-game` bench log
(`~/projects/lyudmilovsk/unreal/Lyudmilovsk/Saved/Logs/bench_20261101-012000_7ecd47ba_after-brave.log`).

**Bottom line:** the module's tick setup is in good shape after round 1. Nothing new is S1 or S2.
- The many-instance actors are already event-driven or on a tick interval: doors (tick off until they move),
  plants (PL-12 interval), grass (2 s), forests (0.25 s), vending (UI-14 early-out), spare animal bodies (tick off),
  fruit (tick only while rotting).
- Every skeletal body except the hero uses `OnlyTickPoseWhenRendered`.
- No `TActorIterator<AActor>` or `TObjectIterator` is left on a per-frame path. The 0.5 s and 2 s ones were PL-07 and W7.
- I found no UE_LOG at Log/Warning level that runs every frame in normal play. All the per-frame debug logs sit behind a CVar.
  The one real log flood is event-driven (R2-T1).

What remains are S3 items, listed below by cost.

## 1. Tick inventory (play, Lvl_Scene01)

Legend: **F** = every frame, **I** = tick interval, **E** = enabled only while needed.

### Actors

| Class | Count in the main level | Rate | Gating / notes |
|---|---|---|---|
| AProceduralPlant | several hundred (321 Academ plants + gardens/trees) | I 0.2 s trees / 0.5 s herbs, F while leaves/fruit/prompt are active | PL-12 fix (`UpdateTickRate`). See R2-T4 for the Printf per re-shape. |
| AProceduralForest | a few | I 0.25 s | ok |
| AProceduralGrass | ~25 patches | I 2 s | ok |
| AProceduralVine | < 10 (VinePlantings) | F | cheap early-outs (CVar compare, transform compare); simulation is budgeted |
| AForestFloor | 1 | F | PL-01/02/03 (round 1) |
| AFallenFruit | 0..hundreds in autumn | E (only the 3 s rot), then Destroy | ok |
| ALyudDoorBase (ALyudDoor, ASwingDoor) | ~312 Academ + club/station doors | E (only while the leaf moves or waits to close, `LyudDoor.cpp:339`) | ok, a model for the rest |
| AVendingMachine | ~10 (3 in the club + 1 per lobby) | F, early-out when idle and far | UI-14 fixed |
| AHeroSeat | 1-2 | F | UI-16 still open (0.1 s interval proposed) |
| ALightFlicker | 1 (drives up to 278 garland point lights) | F | R2-T3 |
| ACurtainPusher | 1 (club) | F | R2-T6 |
| AConveyorBelt | 1 (club) | F | R2-T5 |
| ABoothOwner | 1 | F | state machine. The trade prompt is re-set with `LyudText::Get` every frame while the player stands at the window (minor). |
| AAtonComputer | 1 | I 0.1 s | `TActorIterator<ALevelSequenceActor>` until it is bound; that is a class-filtered iterator, fine |
| ANpcTalkDirector | 1 | F | traces only for candidates in range; P9/P13 (round 1) |
| AAnimalNameTags | 1 | F | seats cached for 2 s (P9). A `GetAnimal` (strings) runs per *named* animal before the distance check; only Maru is named today, so negligible. |
| AStarlingMurmuration | placed in Scene01 | off | stands down in Scene01 (`SetActorTickEnabled(false)`) |
| ALyudStationLift | 7 (one per station) | F | R2-T7 |
| ALyudRailTrack | 1 | I 0.5 s | ok |
| ALyudTrain | per train | F | T7 (round 1) |
| ALyudDiner | 1 | F; `UpdateState` 1 s | ok |
| ALyudDinerDish | per served dish | F | prompt only on change; ok |
| ALyudSky / ALyudRareSky / ALyudSea | 1 each | F | needed per frame (SKY-*, SEA-*). See R2-T2 for `UpdatePlayerChannels`. |
| ALyudRainFx | 1 | F | W13 (round 1) |
| ALyudAirlock | 1 | F always | cheap (two lid lerps) |
| ALyudEuropaSite | 1 | E (only while shown, `LyudEuropa.cpp:1049`); `UpdateLook` 0.5 s | ok |
| ALyudDiaryBook | 1 while out | F | ok |
| ALighterItem, ALyudSmokeFx, ASeedHandful, ABirdFeedItem | on demand | E | ok |
| AAnimalActor | near bodies; a spare pool | F while shown, tick off while spare (`AnimalLife.cpp:847/901`) | ok |
| ABirdActor | at most 12 (`lyud.Birds.Bodies`) | F | bodies are destroyed when dropped; ok |
| ABirdPawn | 1 while flying | F | ok |
| AHeroCharacter | 1 | F | UI-12 (FindComponentByClass ~6 per frame) still worth caching |
| APlantDevTool | 0 (console only) | F | ok |

### Components

| Component | Count | Rate | Notes |
|---|---|---|---|
| UHeroReachComponent, ULyudSwimComponent, UHeroShadowMesh | 1 each (hero) | F | shadow mesh copies the body pose each frame (by design) |
| ULyudLadderClimber | 1 | E (only while climbing) | ok |
| UAnimalCollarComponent | 1 (Maru) | F, early-out while hidden; off-screen it skips the swing but still moves the tag | ok |
| ULyudSeabedComponent | 1 | F | `SetVisibility` per kind per frame early-outs; SEA-1 |
| ULyudBuoyancyComponent | **0** (never attached anywhere) | - | dead code |
| USkeletalMeshComponent on ANpcBody / AAnimalActor / ABirdActor / ABoothOwner / placed SkeletalMeshActors | tens | component tick F; pose only when rendered | all set to `OnlyTickPoseWhenRendered` except the hero (`HeroCharacter.cpp:93`, needed) |
| UWidgetComponent (cabin boards, station signs, rail/crossing boards) | tens | F | T1 (round 1) |
| UAudioComponent | many | engine: no tick | ok |

### Tickable subsystems, tickers

- **31 `UTickableWorldSubsystem`s.**
  - These 12 have a conditional `IsTickable`, so they sleep when idle: Benchmark, Transport, Railway, Winter, CounterTrade,
    PeopleLife, AnimalLife, Bite, HandUse, BirdLife, BirdFlight, Bike.
  - These tick every frame and early-out cheaply: InventoryUI, BirdTame, BirdGuide, GameScreen, Europa, Clouds, SunGlare,
    TreeBakeCache, Smoking, Ladder, Door, Soundscape, Food, Wind, Grade, RainFog, Weather, Underwater, Corvid, BirdFeeder.
  - Each is under a few us when idle, so the dispatch total is negligible.
  - `ULyudSunGlare` ticks while paused (intentional). `ULyudCloudSubsystem` also ticks in editor worlds (`IsTickableInEditor` true).
  - `ULyudBenchmark` runs a `TActorIterator<AProceduralPlant>` per frame, but only while a bench is running.
- **FTSTicker:**
  - AcademToggle `LampTick` at 0.5 s (PL-07, now cached through `LampCaches`).
  - `BirdTame.cpp:1398` and `LyudDoorTest.cpp:345` (tests only).
- **Slate `Tick`:** SLyudHud, SLyudDiaryScreen and SAtonDesktop/AtonApps. These are UI-10 and the ui.md notes from round 1.

## 2. Per-frame iteration, string building and log sweep

- **TActorIterator / TObjectIterator:** 126 hits outside tests. Every per-frame one is either class-filtered and cheap, or it is cached.
  - Class-filtered, cheap: `ACharacter` in CurtainPusher, `ALevelSequenceActor` in AtonComputer (0.1 s), `APawn` in the door
    agents (0.1 s).
  - Cached:
    - `ALyudSea::Get` keeps a static weak pointer.
    - SunGlare, Underwater, RareSky, Clouds and RailTrack cache the sky.
    - `UpdateClub` looks once and stores a null sentinel.
  - The heavy ones run at BeginPlay or as console commands: `TActorIterator<AActor>` in LightFlicker, ForestFloor, Diner,
    SeaSubsystem and Save, and `TObjectIterator` in NavStrip, Winter `StartBuild` and Sea `StepDepthMap`. `StepDepthMap` reaches
    its `TObjectIterator` only on the first frame that has a landscape.
- **FindComponentByClass / GetComponents per frame:**
  - Hero lookups (UI-12, T10).
  - `ULyudLadderClimber::Get` from the ladder subsystem per frame (one FindComponentByClass).
  - `ULyudSwimComponent::Get`: pawn first, `TActorIterator<AHeroCharacter>` only while flying as a bird.
  - `GetAttachedActors` in the sky, every frame (R2-T2).
  - `GetOverlappingActors` in the conveyor (R2-T5).
- **FString::Printf / FName from text in Tick paths:** most are round-1 items (BIRDS-14/15, P7, UI-10/13, SKY-4/5, the ~40 sky MID
  FNames). New here:
  - R2-T4 (plant re-shape).
  - FName from TEXT in CurtainPusher and in Wind (two MPC names per frame each), and in Winter `StepFlakes`
    (`LyudWinter.cpp:1310`, 4 FName per frame while snow is built).
  - These hash in the FName table (about 50-100 ns each), so they are trivial. Use `static const FName` when you are in the file anyway.
- **UE_LOG:** 296 non-verbose calls outside tests. Every call inside a Tick path is one of these:
  - behind a CVar: `lyud.Face.Log`, `lyud.Bikes.Log`, `lyud.Transport.LiftLog`, the cloud/sea-life/bird body debug CVars;
  - throttled and only in scripted test runs: the BirdFlight script, HEROBIKE 1 Hz, DINERTEST/SMOKE test tags on events;
  - logged only on rare events: quests, BirdBook, Europa, doors, a plant re-shape slower than 50 ms.
  - The bench log agrees. Apart from engine material warnings, the only high-count game line is `Tame: bird N <event>`
    (283 of 2842 lines, R2-T1).
  - Not a tick item, but seen in the same log: 78 `FlushAsyncLoading` calls during play (sync loads). They belong to the loading
    audits (EUR-2, PL-10, T13, W10).

## 3. Findings

### R2-T1 [S3, high] BirdTame logs every notice/turn/watch/call of every nearby wild bird at Log level

- **Where:** `Private/BirdTame.cpp:385-397` `UBirdTameSubsystem::LogEvent`, called from `HandleNotice/Turn/Watch/Call/Approach/Eat/Flee`
  (`:439-542`) and the focus code (`:910`).
- **What happens:**
  - Every BirdLife reaction event goes through `LogEvent`, which logs any bird within 30 m of the hero. Taming does not have to be on.
  - In the bench run that was 283 lines in ~20 s near a flock, often 3-5 in the same frame (frames 264, 794).
  - Each event also calls `GetBird` (fills a full `FBirdLifeInfo`) and `UGameplayStatics::GetPlayerPawn`, and
    `IsOffering()` (FindComponentByClass on the hero), even for birds beyond 30 m.
  - The log write itself is the main cost: formatting, the output device lock, and a file write on Mac. Starling and pigeon flocks
    can produce dozens of events a second.
- **Fix:**
  - Return at the top of `LogEvent` unless `UE_LOG_ACTIVE(LogTemp, Verbose)` or a new `lyud.Tame.Log` CVar is set, and demote the line to Verbose.
  - Keep `Tame: bird %d befriended` (`:594`) at Log; it is rare and `test_bird_diary` may grep it.
  - None of the scripts parse the `notice`/`turn` lines; only the `test_bird_diary.py` docstring mentions "befriended".
- **Gain:** removes the log flood and a few us of lookups per event near flocks.

### R2-T2 [S3, high] ALyudSky::UpdatePlayerChannels runs a 50 m up-trace and allocates the attached-actor array every frame

- **Where:** `Private/Sky/LyudSky.cpp:744-791`, called from `Tick` (`:305`) in every game frame.
- **What happens:**
  - `Pawn->GetAttachedActors(Attached, true, true)` fills a heap `TArray` recursively. The pawn has held items, the bike and so on.
  - Then `AddIgnoredActors` runs, plus one `LineTraceSingleByChannel` from the head `RoofSearchHeight` (5000 cm) up.
  - The result only feeds an indoor/outdoor vote that moves at 5/s with a 0.3/0.7 hysteresis. A 10 Hz sample gives the same switching time.
- **Fix:**
  - Accumulate DeltaSeconds and trace every 0.1 s, keeping the last `bOpen` in a member. Interpolate the vote every frame or at
    the sample; either works.
  - Use `TArray<AActor*, TInlineAllocator<8>>`, or move `GetAttachedActors` inside the sample and the relight branch.
- **Gain:** about 10-30 us/frame on a large map (the trace dominates), plus one allocation per frame.

### R2-T3 [S3, med] ALightFlicker walks up to 278 garland lights every frame, even with the player kilometres away outdoors

- **Where:**
  - `Private/LightFlicker.cpp:46-83`.
  - Lights tagged by `unreal/scripts/import_scene01.py:588-597`: every garland bulb beyond the invisible wall gets FarFlicker,
    and 20 % of the near ones get Flicker. That is 278 bulbs in `scene01_layout.json`.
- **What happens:**
  - Per frame, per light: a weak-pointer resolve, the dip bookkeeping and `SetIntensity`. `SetIntensity` returns early when the
    value is unchanged (engine `LightComponent.cpp:1076`), so the cost is the loop itself, about 3-6 us.
  - It also starts a light colour/brightness render command per changing bulb.
  - The corridor is part of Lvl_Scene01, so this runs during the whole outdoor game.
- **Fix:**
  - Skip the loop while the view is more than ~60 m from the actor's bounds, and don't advance the light clocks. They are random,
    so nothing visible changes.
  - Or keep the next event time per light and set the actor tick interval to the minimum of those times (dips last 0.04-0.5 s, gaps are seconds).
  - Lights that are `bDead` could be removed from `Lights` at BeginPlay.
- **Gain:** small (single-digit us), but it is free.

### R2-T4 [S3, high] AProceduralPlant::Tick formats a debug FString on every re-shape, used only when the re-shape takes > 50 ms

- **Where:** `Private/Plants/ProceduralPlant.cpp:1237-1247`. `const FString Why = FString::Printf(TEXT("summary %d/%d age %.4f/%.4f/%.4f day %.3f/%.3f litter %d wood %d bake %d"), ...)`
  is built before `Apply` and logged only when `Ms > 50.0`.
- **What happens:** a heap string and 10 formatted numbers per re-shape, which is about 1-2 us. Re-shapes happen across hundreds
  of plants: day changes, maturity steps, wood updates. In time-lapse (dev panel, `GameCalendar` TimeScale) that can be dozens
  per frame. It lands right in the budgeted re-shape path, whose `BudgetUsedMs` it inflates.
- **Fix:** copy the handful of values into locals before `Apply` (`bShownSummary`, `ShownAge`, `WoodAge`, `ShownDay`,
  `bLitterDirty`, `bNewBake` may change inside `Apply`). Do the `Printf` inside `if (Ms > 50.0)`.
- **Gain:** removes one allocation per re-shape. Trivial risk.

### R2-T5 [S3, low] AConveyorBelt allocates two arrays and sets its MIDs every frame with nobody on it

- **Where:** `Private/ConveyorBelt.cpp:53-110`.
- **What happens:**
  - Every frame there is a `GetOverlappingActors` into a fresh `TArray` and a second `Riders` TArray.
  - `SetScalarParameterValue(TEXT("Offset"))` runs per belt MID. It is an FName from text, and it returns early if the value is
    unchanged, which is the case while `Speed == 0`.
  - The motor volume and pitch are set every frame.
- **Fix:**
  - Return early when `Speed` is about 0 and `Zone` has no overlap. `Zone->IsOverlappingActor(Player)`, or a
    `bAny = Zone->GetOverlapInfos().Num() > 0` test, needs no allocation.
  - Use `TInlineAllocator<4>` for both arrays and a `static const FName`.
  - Better: drive tick from the Zone's Begin/EndOverlap and switch tick off once Speed reaches 0.
- **Gain:** about 1-2 us/frame. One instance.

### R2-T6 [S3, low] ACurtainPusher iterates all characters and sets two MPC vectors every frame wherever the player is

- **Where:** `Private/CurtainPusher.cpp:13-44`.
- **What happens:**
  - Per frame: `TActorIterator<ACharacter>` (class-filtered, so in practice only the hero and a bird pawn), and two
    `UKismetMaterialLibrary::SetVectorParameterValue` calls with FNames from text. Each call also does an MPC-instance lookup on the world.
  - The MPC compares values (`ParameterCollection.cpp:1022`), so there is no uniform-buffer update while nobody moves.
    But while the hero walks anywhere on the map the parameters change and the MPC uniform buffer is rebuilt.
    NPC bodies are `AActor`, not `ACharacter`, so they never push the strips anyway.
- **Fix:** when no character is within `Range`, write the "far" value once (`bWroteFar`) and skip. Cache the MPC instance
  (`GetWorld()->GetParameterCollectionInstance(Collection)`) and use static FNames.
- **Gain:** a few us, plus one MPC buffer update per frame during outdoor play.

### R2-T7 [S3, low] Every station lift ticks every frame, re-placing all landing-door leaves, even when idle and far away

- **Where:** `Private/Transport/LyudStationBuild.cpp:1831-1979`. There are 7 lifts (`:393`, one per station).
- **What happens:**
  - While the cab is idle with all doors settled, every frame still runs:
    - `InverseTransformPosition` of the pawn and `StopNear`;
    - the per-stop door FInterp;
    - for each stop and each of its 2 leaves, `SetRelativeLocationAndRotation` (the engine short-circuits a zero move, but the
      child transform maths still runs);
    - `SetCollisionEnabled` (returns early when unchanged).
  - That is about 7 lifts x (2-4 stops x 2 leaves) x a few hundred ns.
- **Fix:**
  - Return right after the `Open[]` update when `Target < 0`, no `Open[I]` changed this frame, and the pawn is outside
    `ShaftHalf + 600` cm in 2D. Track a `bLeavesDirty` flag.
  - Or set `SetActorTickInterval(0.25f)` while the pawn is more than 30 m away, and 0 when it is near.
- **Gain:** small.

### Minor and housekeeping (no ticket needed)

- **ULyudBuoyancyComponent** (`Private/Sea/LyudSea.cpp:1059-1093`) is never attached anywhere. It is dead, but it costs nothing until it is used.
- **ULyudCloudSubsystem::Tick** reads `CVarCloudPreset` as an FString copy every frame (`LyudClouds.cpp:341`). It is empty by default,
  so there is no heap allocation. `IsTickableInEditor() == true` means it also runs in editor worlds; check that this is wanted.
- **ALyudEuropaSite::Tick** sets the `SeaClock` vector on its water MID every frame while shown (`LyudEuropa.cpp:1112-1117`). It changes
  every frame by design. Only `FName(TEXT)` could be made static.
- **ABoothOwner::TickTrade** (`BoothOwner.cpp:193`) calls `LyudText::Get` plus `SetPrompt` every frame while the player is at the
  window. Set it only on the rising edge, as `ALyudDinerDish::ShowPrompt` does.
- **Open from round 1:** UI-16 (the AHeroSeat 0.1 s interval) and UI-12/T10 (hero FindComponentByClass caching) are still the
  cheapest per-frame wins on the hero path.

## Top list

1. **R2-T1:** demote or gate the BirdTame `LogEvent` flood (the only high-count game log line in play; bursts of 5 a frame near flocks).
2. **R2-T2:** sample the sky's roof trace and attached-actor gather at 10 Hz instead of every frame.
3. **R2-T4:** build the plant re-shape debug string only when it is logged.
4. **R2-T3, R2-T6, R2-T7:** stop the club garland flicker, the curtain MPC writes and the idle station lifts from ticking at full
   rate when the player is nowhere near.
5. **R2-T5:** make the conveyor event-driven.

The inventory's main result is negative: no actor class with many instances still ticks every frame without a gate.
The remaining per-frame cost is in the singletons (sky, sea, forest floor, birds, people), which the round-1 domain audits covered.

## Fixes applied

Source root `unreal/Lyudmilovsk/Source/Lyudmilovsk`. Not built here (the lead builds).

- **R2-T1 done.** `Private/BirdTame.cpp`: new CVar `lyud.Tame.Log` (default 0). `UBirdTameSubsystem::LogEvent` returns
  at the top, before `GetPlayerPawn`/`GetBird`/`IsOffering`, unless the CVar is on or `UE_LOG_ACTIVE(LogTemp, Verbose)`. With the CVar
  on it logs at Log as before; otherwise the line is at Verbose. `Tame: bird %d befriended` stays at Log.
  Verify: no `Tame: bird N notice/turn/...` lines in a normal -game log near a flock; `lyud.Tame.Log 1` brings them back; befriending still logs.
- **R2-T2 done.** `Private/Sky/LyudSky.cpp` `ALyudSky::UpdatePlayerChannels`, `Public/Sky/LyudSky.h` (new `RoofSampleClock`).
  The roof trace and the attached-actor gather run every 0.1 s, or at once when the pawn changes. The vote steps by the time gathered,
  so it moves at the same 5/s with the same 0.3/0.7 hysteresis; the indoor/outdoor switch comes at the same time to within one sample.
  The relight loop reuses the `Attached` array (no copy). The inline allocator was not possible: `AActor::GetAttachedActors` and
  `FCollisionQueryParams::AddIgnoredActors` take only `TArray<AActor*>` with the default allocator, so the heap array stays, at 10 Hz now.
  Verify: walk in and out of the club and the Academ houses; `IsPlayerOutdoors` / DebugReport `outdoors` flips as before; the hero's light is the same.
- **R2-T3 done.** `Private/LightFlicker.cpp` (`BeginPlay`, `Tick`), `Public/LightFlicker.h` (`ActiveRange` = 6000 cm, `LightBox`, `bPaused`).
  BeginPlay boxes the live lights. While the player view is more than ActiveRange from that box, Tick returns: once on entering the pause it
  resets running dips and puts every live bulb at its steady brightness, and the clock does not advance. Near the corridor it behaves as before.
  Verify: in the club/corridor the garland blinks as before; outdoors (stat or Insights) the ALightFlicker tick does no loop.
- **R2-T4 done.** `Private/Plants/ProceduralPlant.cpp` `AProceduralPlant::Tick`: the values the debug string shows are copied into locals
  before `Apply`, and the `FString::Printf` is built only inside `if (Ms > 50.0)`. The log line is unchanged.
  Verify: a slow re-shape still logs `Plant: re-shaping ... (summary ...)` with the pre-Apply values.
- **R2-T5 done.** `Private/ConveyorBelt.cpp` `AConveyorBelt::Tick`, `Public/ConveyorBelt.h` (`bIdleWritten`).
  Returns at the top when the belt is stopped (`Speed == 0`), the stopped state was written by a full tick, and `Zone->GetOverlapInfos()`
  is empty. `Riders` uses `TInlineAllocator<4>` (`GetOverlappingActors` needs a default-allocator array). `Offset` is a static FName.
  Verify: the belt still speeds up when you run along it and coasts to a stop; the motor hum fades out as before.
- **R2-T6 done.** `Private/CurtainPusher.cpp` `ACurtainPusher::Tick`, `Public/CurtainPusher.h` (`CollectionInstance`, `bWroteFar`).
  With no character in Range, the far value is written once and then nothing is written. The MPC instance is cached as a weak pointer and
  re-fetched if it goes away or Collection changes. `Pusher0`/`Pusher1` are static FNames. The Kismet "missing parameter" PIE warning is no
  longer raised (it went through UKismetMaterialLibrary).
  Verify: the club curtain strips still bend round the hero walking through and settle when they leave.
- **R2-T7 done.** `Private/Transport/LyudStationBuild.cpp` `ALyudStationLift::Tick` and `Setup`, `Public/Transport/LyudCableway.h` (`bLeavesPlaced`).
  After the `Open[]` update, Tick returns when there is no target, no `Open[I]` changed this frame, the leaves were placed since, and the
  pawn is more than `ShaftHalf + 600` cm from the shaft in 2D (or there is no pawn). All the call/ride logic above that point still runs.
  Verify: test_stations, plus `lyud.Transport.LiftLog 1` near a lift (it still logs once a second there): calling, riding and the landing doors behave as before.
- **Booth owner prompt: skipped.** `ABoothOwner::TickTrade` re-asserts the prompt each frame, and `ULyudInventoryUI` has one prompt slot,
  last writer wins, with no public getter for its key (`LyudInventory.h`, not my file). Setting it only on the rising edge would let
  another actor's prompt, or that actor clearing its own, take the trade prompt away while the player stands at the window. It needs a
  `ULyudInventoryUI::PromptOwner()` getter first.

GC items from `audit/r2_gc.md` done in the same pass:

- **GC-3 (without BirdLife) done.**
  - `Public/Transport/LyudCableway.h`: `UPROPERTY(Transient) TArray<FLyudBuiltStation> Built` (already a USTRUCT, so `Boards`/`Lift`/`Door` are now seen).
  - `Public/Europa/LyudEuropa.h`: `UPROPERTY(Transient)` on `ALyudAirlock::SeaLid`, `InnerLid` and `ALyudEuropaSite::Lid` (already a USTRUCT; not SaveGame, so saves are unchanged).
  - `Public/Plants/ProceduralForest.h` `FGroup::Comp` and `Public/Birds/BirdNests.h` `FDrawSet::ISM`: these are nested structs (they can't be a
    USTRUCT), so they became `TWeakObjectPtr`. The components stay alive through the actor's OwnedComponents, as before. The edits for
    `TWeakObjectPtr`'s deleted bool are in `ProceduralForest.cpp` (`DescribeState`, `GroupFor`, `Clear`, `SyncBakes` skips a dead comp, `SetHidden`)
    and in `BirdNests.cpp` `Commit` (a dead ISM keeps `Xf` and returns).
  - `BirdLife.h` `FSpeciesDraw::Poses` was left alone, as assigned.
- **GC-6 done.** `Private/Birds/BirdFlightSubsystem.cpp` `UBirdFlightSubsystem::Deinitialize`: `SetHeroHint(FText::GetEmpty())` runs first. Nothing else in that file was touched.
  Verify: `lyud.Load` while a bird hint shows leaves no hint over the reloaded world.
