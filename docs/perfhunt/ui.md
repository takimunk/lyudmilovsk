# Perf/leak/crash audit: ui thread

Areas: Private/UI, Private/AtonOS, Private/Murmuration, Private/GameMenu, top-level Private/*.cpp (except BirdTame, BirdFeedItem,
NpcDialogue, BoothOwner, ConveyorBelt, SwingDoor, CurtainPusher, LightFlicker). Read-only review; nothing built or run.
Paths below are relative to unreal/Lyudmilovsk/Source/Lyudmilovsk.

Overall this code is in good shape. Nothing here does heavy per-frame work during normal play. The HUD, the hero, reach and anim
proxy are cheap. Slate screens rebuild only on events. Delegates are unbound in Deinitialize. The stood-down murmuration actor
really does stop ticking. The real costs are in the shop and vending props, which are built per object (one MID per part,
failing LoadObject calls), plus a few small leaks and hitches.

---

## UI-01 (S2, med) Every vending stock item and goods part has its own MID, with no draw distance
- `Private/BottleItem.cpp:187-204` (`ABottleItem::Init`): each bottle makes **4 MIDs** (glass, liquid, cap, label) with fixed
  per-flavour colours.
- `Private/LyudGoodsItem.cpp:22-37` (`Part`): one MID per part. `:113-130`: one more MID for the label plate.
- `Private/VendingMachine.cpp:208-258` (classic hall machine): 36 bottles (4 components each) and 6 feed bags.
  `:152-206` (universal, one per lobby, 7 lobbies in lobby.json): 2 copies × 4–8 goods × 2–4 parts each.
- Total: about 150 components in the club machine and about 300 across the lobbies. Every one has a unique MID, so none of them
  merge into instanced draws: each is its own mesh draw in every pass (base pass, depth, shadow if enabled). None has a max
  draw distance, so a lobby seen from the cable car draws all its tiny goods.
- Fix (local):
  - Cache the MIDs in a static map keyed by colour (or flavour+part), outered to the transient package and rooted, the same way
    `Meshes()` caches the bottle meshes. Only `LiquidMaterial` may need to be per bottle, and nothing animates it today
    (SetLevel only scales the mesh), so it can be shared too.
  - On shelf stock, set `C->SetCullDistance(2500.f)` (or `LDMaxDrawDistance`) and `bAffectDistanceFieldLighting=false`. These are
    5–20 cm objects.

## UI-02 (S2 hitch / S3 bug, high) `LyudText::LoadTexture` / `LoadVoice` try the "ru" path twice, so failing loads double
- `Private/LyudText.cpp:115` and `:129`: `for (const FString& Lang : { Language(), FString(TEXT("ru")) })`. When the language is
  ru (the default), the same path is tried twice.
- When the asset does not exist, that means **two failing `LoadObject` calls**. Each one is a package-existence lookup, which hits
  the file system or the asset registry and costs well over 0.1 ms in an editor `-game` build.
- Most goods have no `T_Label_<id>` (only the bottle flavours and feeds do), so
  `Private/LyudGoodsItem.cpp:101` does 2 failing loads per goods item. That is about 170 failing loads at lobby BeginPlay.
  The same happens at runtime on every `ULyudInventoryUI::Use` / vending `Buy` that spawns goods, and in `LoadVoice` for every
  missing voice line.
- Fix:
  - Skip the second pass when `Language()==TEXT("ru")`.
  - Add a static `TSet<FString>` of paths already known to be missing, so a failing path is never loaded twice.
  - Optionally check `FPackageName::DoesPackageExist` once and cache the answer.

## UI-03 (S2 hitch, high) Each universal vending machine reads and parses lobby.json at BeginPlay
- `Private/VendingMachine.cpp:47-78` (`LoadPreset`), called from `ResolveSlots` in `BeginPlay` (`:111`): `FFileHelper::LoadFileToString`
  plus a full `FJsonSerializer::Deserialize` of Content/Transport/lobby.json, **once per machine** (7 machines). The same file is
  already parsed by LyudLobby.
- Fix: a function-local static cache, e.g. `static TMap<FName, TArray<FVendingSlot>>` built on the first parse. Better still, have
  ALyudLobby pass the slots in with `M->Slots = ...` before `FinishSpawning`; it already has the JSON object at
  `Private/Transport/LyudLobby.cpp:191-199`.

## UI-04 (S1 crash risk, med-low) Vending `Take` callback captures raw `this`
- `Private/VendingMachine.cpp:422-429`: `Reach->Take(Reachable, [this, ItemId, Count]() { ... State = EState::Idle; })`. The lambda
  lives in the hero's `UHeroReachComponent::Steps`, about 2.8 s of steps, and runs from `Stow`'s `OnEnd`.
- If the machine is destroyed during that window (lobby or level teardown, PIE stop while reaching), the callback writes to freed
  memory.
- `UHeroReachComponent` also captures raw `this` in its own step lambdas (`HeroReachComponent.cpp:329,346,358,396,445`). Those are
  safe because they are owned by the component itself.
- Fix: capture `TWeakObjectPtr<AVendingMachine> Self(this)`. In the lambda, do `if (Self.IsValid()) Self->State = EState::Idle;`
  and use `ULyudInventory::Get(Self.Get())`. The same pattern applies to `ULyudInventoryUI::Use`'s `Consumed` / `Stow` lambdas
  (`Private/LyudInventory.cpp:943-962`). Those capture the world subsystem's raw `this`, which is lower risk since it lives as
  long as the hero.

## UI-05 (S2, med) ATON computer screen re-renders its 1024×768 widget every frame even when not in use
- `Private/AtonComputer.cpp:17-28`: the `UWidgetComponent` (world space, `Resolution` 1024×768) uses the default RedrawTime 0, so
  it repaints the whole SAtonDesktop into its render target **every frame** whenever it is on screen. That is a Slate paint plus
  an RT draw. The computer sits in the club where the game starts, so it is often in view.
- The desktop's clock text lambda (`Private/AtonOS/SAtonDesktop.cpp:204`) also Printf's every paint.
- An open Notes or Trash window keeps polling the disk once a second (`AtonApps.cpp:202-213`, `:335-345`) after the player leaves
  the computer, because the desktop keeps ticking while it is visible.
- Fix:
  - Call `Screen->SetRedrawTime(0.5f)` (or `SetManuallyRedraw(true)` plus `RequestRedraw()` on a 1 Hz timer) while `!bActive`.
  - Call `SetRedrawTime(0.f)` in `Activate()` and set it back in `Deactivate()`.
  - Alternatively, `SetTickMode(ETickMode::Automatic)` with a lower redraw rate.

## UI-06 (S3, high) The diary book moves every frame but stays in Lumen and distance-field lighting
- `Private/UI/LyudDiaryBook.cpp:186-198` (`AddPart`): about 14 shadow-casting parts are attached to the eyes and moved every frame
  while the diary is open (`LyudGameScreen.cpp:398`). They keep the defaults `bAffectDynamicIndirectLighting=true` and
  `bAffectDistanceFieldLighting=true`.
- The same applies to items held in the hand (`ABottleItem`, `ALyudGoodsItem`, the apple sphere in `LyudInventory.cpp:856-870`),
  which are attached to `hand_r` and move every frame.
- Fix: in `AddPart`, `ABottleItem` ctor `Make`, and `ALyudGoodsItem::Part`, set `C->bAffectDynamicIndirectLighting = false;
  C->bAffectDistanceFieldLighting = false;`, as `ConfigureBirdComponent` already does.

## UI-07 (S3, high) Settings volume slider writes the ini to disk on every value-change event
- `Private/GameMenu/SLyudGameMenu.cpp:272`: `OnValueChanged` calls `LyudSettings::SetVolume`, which runs `Store`
  (`Private/UI/LyudSettings.cpp:24-28`: `GConfig->SetString` + `GConfig->Flush(false, GGameUserSettingsIni)`) and then
  `ApplyVolumes`, which does 4 `GConfig->GetString` calls.
- A drag fires `OnValueChanged` every mouse move, so the ini is written to disk every frame while dragging. That is a stutter, and
  a real one on slow storage or iOS.
- Fix: apply volumes live, but only `SetString` without `Flush` in `SetVolume`. Flush once from `OnMouseCaptureEnd` or when the
  menu closes.

## UI-08 (S3 leak / bug, med) HeroSeat spawns a new sequence actor on every load without removing the old one
- `Private/HeroSeat.cpp:102-109` + `:127-135`: `OnGameLoaded` sets `bSitting=false` and calls `StartSit`, which always
  `SpawnActorDeferred<ALevelSequenceActor>`. The previous `SequenceActor` (paused at end, `bHidePlayer`) is never destroyed.
- Each save-load while seated leaves one more sequence actor holding the player hidden.
  `AAtonComputer::Tick` (`AtonComputer.cpp:110-124`) then binds whichever one the iterator meets first.
- Fix: in `StartSit`, if `SequenceActor` is valid, stop its player and `Destroy()` it before spawning. Optionally reuse it by
  calling `SetSequence` and `Play`.

## UI-09 (S3, high) Map view copies the transport arrays and the bird list on every paint
- `Private/UI/SLyudMapView.cpp:225`: `const TArray<FLyudTransitLine> Lines = Transport->GetLines();` copies every line with its
  `Path` array. `:236` `GetCabins()` and `:245` `GetStations()` also return by value
  (`Public/Transport/LyudTransport.h:93-95`, UFUNCTIONs).
- `:470`: `ULyudBirdGuide::BirdsNearHero` (a FindBirdsNear plus a `GetBird` copy per bird) runs every paint. The view is
  `ForceVolatile`, so it repaints every frame while the map tab or the bird guide is open.
- The bird guide rebuilds its whole legend (all rows of `SNew` widgets) every 0.5 s (`Private/UI/LyudBirdGuide.cpp:172-176`). It
  is a second `BirdsNearHero` query.
- Cost: a few hundred µs while the map is open, not in normal play.
- Fix:
  - Add `const TArray<...>& LinesRef() const` accessors on the transport subsystem (C++ only).
  - Cache the `BirdsNearHero` result in the view, refreshed at 5 Hz from `Tick`, and share it with the legend.
  - Rebuild the legend only when the (species, N, flying) rows change.

## UI-10 (S3, high) HUD clock builds about 8 FStrings, 3 table lookups and an FString::Format every frame
- `Private/UI/SLyudHud.cpp:61`: the `Text_Lambda` calls `LyudUIText::DateTime` (`Private/UI/LyudUIText.h:10-24`). That is a
  Printf key, `LyudText::Get` ×3 (each copies the language CVar FString in `Language()`, `LyudText.cpp:88-92`, and hashes an FString
  key), `ToString`, `Format` ×2, and Printf for the clock.
- The HUD is always visible. The text block does not relayout (FromString texts compare equal), so the cost is only the
  allocations: around 5–10 µs per frame.
- Fix: cache the text in the HUD. Recompute it only when `Now.GetMinute()` or the language changes, compared in `Tick`.
  - Make `LyudText::Language()` return a cached `FString` that is refreshed through the CVar's `OnChangedDelegate`. That speeds
    up every `LyudText::Get` in the game, including `SLyudHud::Tick` and `SLyudDiaryScreen::Tick`, which call `Language()` every
    frame.

## UI-11 (S3, med) The first diary open loads fonts and textures synchronously, and statics root them for the whole process
- `Private/UI/LyudDiaryStyle.cpp:15-23,33-80,94-123,173-196`: the first `Hand()`, `PaperBrush`, `MapInk` and `SketchBrush` calls
  `LoadObject` 3 font faces, the paper textures and the map ink texture on the game thread.
  - That happens on the first Tab/J/M press, giving a visible hitch on first open.
  - `SketchBrush` also does 2 failing loads per item without a sketch (see UI-02).
- These are held in function-local static `TStrongObjectPtr` and `FSlateBrush`. They live past world teardown and are destroyed
  in static destructors after the UObject system shuts down. That is a known source of exit-time crashes and asserts. Compare
  the "static UObject crash" in the unique-people notes.
- Fix:
  - Warm them in `ULyudGameScreen::OnWorldBeginPlay`, or async-load them with `FStreamableManager`.
  - Replace the static strong pointers with a `UGameInstanceSubsystem` (or the game screen) holding `UPROPERTY` references, or
    reset them on `FCoreDelegates::OnPreExit`.

## UI-12 (S3, high) The hero calls FindComponentByClass about 6 times per frame
- `Private/HeroCharacter.cpp:258` (`UHeroBikeComponent::Get` → `FindComponentByClass`, `Private/Bikes/HeroBike.cpp:47-50`).
  `:324,329` (`IsLeadingBike`/`IsOnBike`: 2 or 3 more). `:303` (`UHeroReachComponent`). `:494` (`CalcCamera`, Bike again).
  `Private/UI/LyudGameScreen.cpp:401` (`UHeroReachComponent::Get` per frame while the diary is open).
- That is a linear scan of about 15 components, 6–8 times per frame. Microseconds in all.
- Fix: cache `TWeakObjectPtr<UHeroBikeComponent>` and `<UHeroReachComponent>` on the hero, set by their `Get()` when they create
  or attach.

## UI-13 (S3, high) The hero anim proxy constructs FNames and Printf's on every evaluate
- `Private/HeroAnimInstance.cpp:67,75-76`: `KeepHandsOutsideClothes` runs on every evaluation. It does
  `Find(TEXT("pelvis"))`, `Find(TEXT("spine_03"))`, and 2× `FString::Printf("upperarm_%s"/"hand_%s")` with an FName lookup.
  `:327`: `GetPoseBoneIndexForBoneName(S.Bone)` with `const TCHAR*` gives 5 more FName constructions.
- `Private/HandReach.cpp:269,282,312,343-347,394-399`: `ApplyPosture`, `ApplyCrouch` and `ApplySit` call `Find(Bones, TEXT(...))`,
  constructing an FName from TCHAR each time. Each is a global name-table hash plus a lock, and up to about 20 run per evaluation.
- Fix: use static `const FName` for all the bone names, as HandReach already does for `Pelvis` and `Hand`. Better, cache the
  `FCompactPoseBoneIndex` set on the proxy when the bone container's serial number changes.

## UI-14 (S3, med) Every vending machine runs its full proximity logic every frame
- `Private/VendingMachine.cpp:377-488`: 8 machines tick every frame (7 lobbies plus the club). Each tick does
  `GetPlayerPawn`, `ULyudInventoryUI::Get` (world-context resolve + subsystem map), and `Hero->EyeLocation()` (2 socket
  transforms) before the distance test.
- While the player looks at a machine, `PromptFor` (`:275-301`) rebuilds the classic machine's whole `Names` array plus about 6
  `FString::Format` calls every frame.
- Fix:
  - Do the cheap `FVector::DistSquared2D(Hero, Flap) > Square(UseDistance+100)` test first and return. Or
    `SetActorTickInterval(0.25f)` while far, back to 0 when near.
  - Cache the prompt text keyed by (Flavor, Rubles, language).

## UI-15 (S3, BeginPlay hitch outside Scene01, high) StarlingMurmuration SampleGround fires about 17k line traces synchronously
- `Private/StarlingMurmuration.cpp:187-213`: with the defaults (`AreaHalfSize` 300×180 + margin 150, 6 m cells), NX=151 and NZ=111,
  giving 16,761 `LineTraceSingleByChannel` calls in `StartFlock` → `BeginPlay`.
  - Each `ProbeGround` also calls `UGameplayStatics::GetPlayerPawn` (`:225`).
  - `StartFlock` also re-runs whenever `BirdCount` changes (`:260-263`).
- The actor stands down in Lvl_Scene01 (`:114-120`), so this only hits the preview and test levels.
- Fix: in SampleGround, get the pawn once and pass it in (or build the `FCollisionQueryParams` once). Consider a 12 m cell
  (4× fewer traces), or spread the sampling over a few frames.

## UI-16 (S3, low) The HeroSeat polls each frame, and AnimalNameTags iterates all seats every tick [OTHER-THREAD]
- `Private/Animals/AnimalNameTags.cpp:147,174`: `TActorIterator<AHeroSeat>` runs every tick. That belongs to the animals thread.
- In my area, `AHeroSeat::Tick` (`Private/HeroSeat.cpp:81-100`) is cheap, but it ticks forever. Fix: a 0.1 s tick interval.

---

## Checked and clean (no finding)
- **ULyudGameScreen** (`LyudGameScreen.cpp`):
  - All delegates bound in `OnWorldBeginPlay` are removed in `Deinitialize`.
  - Render targets are created once per index and are UPROPERTY.
  - Lamp, Book and the textures are UPROPERTY. The Book's `OnTick` lambda dies with the Book.
  - `UpdateHud` and `SetVisibility` are no-ops when nothing changed. `UpdatePosition` runs at 4 Hz.
- **SLyudHud**: toasts rebuild only when dirty. **SLyudDiaryScreen**: rebuilds on events and language change only.
  `DrawSpread` (WidgetRenderer) runs only on open, close and turn.
- **HeroReachComponent**: idle `TickHold` returns early. `FReachShape` uses an inline allocator, so `Pose` copies don't allocate.
- **FHeroAnimProxy::PreUpdate**: the clip-array copies reuse capacity, so there are no per-frame heap allocations.
- **HeroShadowMesh**: one pose copy per frame, which is intended. Visibility and cast-shadow are toggled only on change.
- **LyudSave**: everything is event-driven. `SaveObjects` holds weak pointers. Timer-for-next-tick uses `CreateUObject` + a weak world.
- **LyudGameMenu**: the input pre-processor is unregistered in `Deinitialize`. `GetSlots` disk reads happen only on menu actions.
- **AStarlingMurmuration**: ISM updates use `bMarkRenderStateDirty=false` with pooled arrays. Bird components are out of Lumen.
  In Scene01 the tick really is disabled.
- **ULyudQuests**: JSON is read once per level start. `HandleEvent` runs only on gameplay events.
- **ULyudInventoryUI**: icon brushes are cached, and Icons is UPROPERTY. The prompt `SetText` with equal text doesn't relayout.

---

## Fixes applied

Paths relative to unreal/Lyudmilovsk/Source/Lyudmilovsk. Nothing built or run.

- **UI-01 done.** New `ALyudGoodsItem::SharedMaterial(Base, Color, Label)` (`Public/LyudGoodsItem.h`, `Private/LyudGoodsItem.cpp`).
  It is a static map keyed by base path, colour and label path. It holds rooted MIDs outered to the transient package, stored as
  raw pointers. Changes:
  - `ALyudGoodsItem::Part` and the label plate in `Init` use it.
  - `ABottleItem::Init` uses it for glass, liquid, cap and label, so one set of MIDs serves each flavour (and label texture).
  - The bottle's base materials are looked up once (`Bases()`), not 4–8 `LoadObject` calls per bottle.
  - New `ALyudGoodsItem::SetShelfStock(Actor)`: `SetCullDistance(2500)` and `bAffectDistanceFieldLighting=false` on every primitive.
    `AVendingMachine::StockShelves` / `StockShelvesUniversal` call it for shelf stock only (bottles, goods, feed bags). It is never
    called for bought or held items.
  - Verify: `stat scenerendering` mesh draw count in the club or a lobby goes down. Shelf goods vanish beyond 25 m. Bottles and
    labels look the same. `obj list class=MaterialInstanceDynamic` shows a few dozen MIDs, not hundreds.
- **UI-02 done.** `Private/LyudText.cpp`: `LoadTexture` / `LoadVoice` now go through `LoadLocalized`.
  - It tries "ru" only once when the language is ru.
  - A game-thread `TSet` of paths that failed to load is cleared by `LyudText::Reload()`, which runs at every level start, so an
    asset imported mid-session still shows up on the next Play.
  - Verify: lobby BeginPlay hitch is smaller. Labels and voices load as before.
- **UI-03 done.** `Private/VendingMachine.cpp` `LoadPreset`: lobby.json is parsed once into a static `FJsonObject` and parsed again
  only when the file's timestamp changes. Verify: the same slots as before in every lobby machine.
- **UI-04 done.**
  - `AVendingMachine::Tick`: the `Take` callback captures `TWeakObjectPtr<AVendingMachine>`.
  - `ULyudInventoryUI::Use` (`Private/LyudInventory.cpp`): the `Consumed` / `Stow` lambdas capture a weak `Self`.
  - Verify: stop PIE while the hand reaches into the vending tray. It should not crash.
- **UI-05 done.** `Private/AtonComputer.cpp`: `Screen->SetRedrawTime(0.5)` after `BeginPlay` and in `Deactivate`, and `0` in
  `Activate`. The idle screen repaints (and ticks Notes/Trash polling) at 2 Hz. Verify: `stat slate` / `stat game` while the club
  computer is in view but unused. The OS is still smooth once seated.
- **UI-06 done.** `bAffectDynamicIndirectLighting=false` and `bAffectDistanceFieldLighting=false` are now set in:
  - `ALyudDiaryBook::AddPart` (`Private/UI/LyudDiaryBook.cpp`)
  - the `ABottleItem` ctor `Make`
  - `ALyudGoodsItem::Part`
  - the apple sphere in `ULyudInventoryUI::SpawnHeld` (one `MarkRenderStateDirty` at spawn)
- **UI-07 done.**
  - `LyudSettings::SetVolume` stores without `Flush`.
  - New `LyudSettings::FlushStored()` (`Public/UI/LyudSettings.h`) is called from the slider's `OnMouseCaptureEnd`
    (`SLyudGameMenu.cpp`), from `ULyudGameMenuSubsystem::Close` (`Private/LyudGameMenu.cpp`), and from the
    `lyud.Settings.Volume` command.
  - Verify: dragging a volume slider has no stutter. The value is in GameUserSettings.ini after release or menu close.
- **UI-08 done.** `AHeroSeat::StartSit` stops and destroys the previous `SequenceActor` before it spawns a new one. Verify: save
  while seated, then load 3 times. `obj list class=LevelSequenceActor` (or the outliner) shows one actor.
- **UI-09 partly done (cheap part).** `SLyudBirdGuidePage::RebuildLegend` (`Private/UI/LyudBirdGuide.cpp`) keeps its widgets when
  the (language, species, N, flying, colour) rows are unchanged.
  - Not done: the transport by-reference accessors (they need `Public/Transport/LyudTransport.h`, outside my area).
  - Not done: caching `BirdsNearHero` in the map view at 5 Hz. That would make the moving birds and the hero's ring step at 5 Hz,
    which is a visual change.
- **UI-10 done.**
  - `LyudText::Language()` now returns `const FString&` to an interned, never-freed string behind an atomic pointer. The cvar's
    `OnChangedDelegate` refreshes it, so it is safe for off-game-thread readers and makes no copy per call. All callers compile
    as is: they compare or assign.
  - The HUD clock (`SLyudHud::ClockText`) caches its `FText` and rebuilds it only when the game minute or the language changes.
  - Verify: the HUD clock still ticks each game minute. Switching the language in Settings updates the clock at once.
- **UI-11 exit-crash part done.** `Private/UI/LyudDiaryStyle.cpp`: the static `TStrongObjectPtr` font faces and paper, map and
  sketch textures are now rooted raw pointers (`Keep()`). Sketches are unrooted when the language changes. No static destructor
  touches UObjects at exit any more. The first-open warm-up is not done.
- **UI-12 done.** New `AHeroCharacter::FindBike()` / `FindReach()` (`Public/HeroCharacter.h`, `Private/HeroCharacter.cpp`): a weak
  cache that rescans only when the hero's component count changes.
  - Used in `IsOnBike`, `IsLeadingBike`, `OnMove`, `Tick`, `CalcCamera`, `Hero.DumpBones`, `UHeroReachComponent::Get` (and so the
    diary's per-frame call), `AVendingMachine::Tick` and `ULyudInventoryUI::CanOpen` / `Tick`.
  - `UHeroBikeComponent::Get` (Bikes) is left as it is.
- **UI-13 done (static FNames only).**
  - `HeroAnimInstance.cpp`: `KeepHandsOutsideClothes` uses static FNames, and `FLookShare::Bone` is an `FName`.
  - `HandReach.cpp`: every `Find(Bones, TEXT(..))` uses `BoneName::*` constants.
  - The compact-index cache per bone-container serial is not done.
- **UI-14 done.**
  - `AVendingMachine::Tick` returns early when idle, the prompt is hidden and the hero is more than `UseDistance+100` away in 2D.
    This happens before `ULyudInventoryUI::Get` and `EyeLocation`.
  - The prompt text is cached by (slot, roubles, language).
  - Verify: the prompt still appears at the machine, and changes on 1..9 and after paying.
- **UI-15, UI-16 skipped** (as assigned).
