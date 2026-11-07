# Round 2: gc (object lifetime / crash risks), whole module

Scope: every raw `UObject*`/`AActor*`/component pointer and every `TObjectPtr` outside a reflected path, in UCLASS, USTRUCT,
plain structs, statics and singletons; static smart pointers; GameInstance subsystems and statics that hold world objects;
delegates, tickers, timers, async work and AddToRoot.

Method:
- Scripts walked every `.h` and `.cpp` and listed member and global declarations. They flagged raw UObject pointers that are not
  directly under a `UPROPERTY(...)` line, `TObjectPtr` members without UPROPERTY, and members whose type is a struct that holds
  object pointers but are not UPROPERTY themselves.
- Grep found the other patterns: `AddLambda|AddRaw|AddUObject|AddWeakLambda|AddDynamic|FTSTicker|SetTimer|CreateLambda|AddToRoot|
  TStrongObjectPtr|Async|UE::Tasks|ParallelFor|LoadPackageAsync|AddViewportWidgetContent`, static locals and namespace globals.
- I read each hit to confirm it. Round-1 items are not repeated: UI-02/UI-05 DiaryStyle statics, vending/inventory weak
  callbacks, GoodsItem rooted MIDs in the editor (review_C #10), SeaLife rooted meshes, TerrainMask root, BIRDS-01 bodies.

---

## GC-1 [S1, med-high] ULyudBiteSubsystem holds the meal's meshes as raw pointers that the GC can free mid-meal

- Where: `Public/Food/LyudBite.h:52-58` (`FLyudMeal::LoadMesh`, `LoadMeshBig`, `LoadMaterial`, `UtensilMesh` are raw `UStaticMesh*` /
  `UMaterialInterface*`). The struct is kept as `ULyudBiteSubsystem::Meal` (`LyudBite.h` ~119), which is not reflected.
- Filled by `Private/Diner/LyudDiner.cpp:342-344`. `ALyudDiner::DinerMesh` (`:164-173`) is a bare `LoadObject`, and nothing else
  holds the result. The dish actor shows only `Def.Food` / `Def.Fill` / vessel meshes, never the `Load_*` meshes.
- Used in `Private/Food/LyudBite.cpp:244-247` (Scoop phase of each bite):
  `LoadC->SetStaticMesh(bBigLoad ? Meal.LoadMeshBig : Meal.LoadMesh)`.
- What happens:
  - At BeginMeal, `SpawnUtensil` puts `LoadMesh` on `LoadC` (`:422`). That component reference keeps the small load alive.
  - `LoadMeshBig` (for example `SM_Diner_Load_Twirl` / `_Noodles`, which exist in Content/Diner/Meshes) is referenced only by the
    raw pointer until the first big bite.
  - After a big bite, `LoadC` holds the big mesh, and `LoadMesh` is referenced by nothing.
  - In `-game` (no RF_Standalone keep) a GC pass frees the unreferenced one. GC runs about every 61 s and on every level load or
    stream. A meal of 6-14 bites with chewing lasts tens of seconds to minutes, so this is likely to happen.
  - The next bite then calls `SetStaticMesh` with a freed UObject: a crash, or a garbage mesh if the memory was reused.
  - `UtensilMesh` is safe only by accident: the utensil component holds it. `LoadMaterial` is the same trap for the next caller.
- Fix (minimal):
  - Add `UPROPERTY(Transient) TArray<TObjectPtr<UObject>> MealRefs;` to ULyudBiteSubsystem.
  - In `BeginMeal`, do `MealRefs = { Meal.LoadMesh, Meal.LoadMeshBig, Meal.LoadMaterial, Meal.UtensilMesh };` (nulls are fine).
  - `MealRefs.Reset()` in EndMeal / Deinitialize.
  - Alternative: make the four fields `TObjectPtr` and give the subsystem `AddReferencedObjects`.
- Verify: start a fork dish with a Twirl big load, then `obj gc` (or `gc.CollectGarbageEveryFrame 1`) between bites. Big and small
  bites keep showing their loads, and there is no crash.

## GC-2 [S2, med-high] BirdBook drawings: function-static `TMap` of `TStrongObjectPtr<UTexture2D>`, destroyed after UObject shutdown

- Where: `Private/Birds/BirdBook.cpp:191-192` (`ULyudBirdBook::Drawing`):
  `struct FDrawing { TSharedPtr<FSlateBrush> Brush; TStrongObjectPtr<UTexture2D> Tex; }; static TMap<FString, FDrawing> Cache;`
- What happens:
  - Opening the diary's Birds tab, or the bird guide, fills the cache with one entry per species drawing (and female drawings).
  - At exit, the static destructor runs `TStrongObjectPtr`'s release after the UObject system has gone. This is the exit-time
    crash/assert that the project already removed from `LyudDiaryStyle.cpp:19-21`, whose comment states the reason. It also
    matches the memory rule "never cache UObjects in raw statics across worlds".
  - The cache also lives across PIE sessions in the editor. That is harmless for textures, but they stay pinned.
- Fix: the same pattern as DiaryStyle `Keep()`: `UTexture2D* Tex = nullptr;` plus `T->AddToRoot()` (bounded: about 35 species × 2).
  Cleaner: hold them in the ULyudBirdBook GameInstance subsystem as `UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>>`
  and keep only the brushes in the static.
- Verify: open J → Birds, page through all species, quit the game. There is no crash or assert in the shutdown log.

## GC-3 [S3, high, latent] USTRUCT members whose own UPROPERTYs are invisible because the member itself is not a UPROPERTY

- The GC only follows a UPROPERTY field inside a struct when the containing member is a UPROPERTY too. These members are not:
  - `Public/Transport/LyudCableway.h:77`: `TArray<FLyudBuiltStation> Built;`. The struct's `UPROPERTY` `Boards` (widget comps),
    `Lift` (spawned ALyudStationLift), and `Door` (ALyudDoor) are not seen. `UpdateBoards` (`LyudCableway.cpp:453-466`) and
    `LyudStationFurnish.cpp:179/229` dereference them.
  - `Public/Europa/LyudEuropa.h:93-94, 154`: `FLyudHatchLid SeaLid, InnerLid, Lid` (UPROPERTY `Pivot` / `Mesh` inside).
  - `Public/Plants/ProceduralForest.h:131/151` (`FGroup::Comp` in `Groups`), `Public/Birds/BirdNests.h:78/83` (`FDrawSet::ISM`),
    `Public/Birds/BirdLife.h:319/333` (`FSpeciesDraw::Poses`). These are plain `TObjectPtr` in non-reflected arrays.
- Today they are safe:
  - Every one points at a component owned by its actor (kept by OwnedComponents), or at a spawned world actor that is never
    destroyed.
  - Nothing nulls them, though, if one ever is destroyed. Examples: a door or lift rebuilt, a lid component destroyed, a
    `DestroyComponent` on a group ISM. The next access then reads a freed object instead of null.
- Fix:
  - Put `UPROPERTY(Transient)` on `Built`, `SeaLid`/`InnerLid`/`Lid`. They are already USTRUCTs, so this is zero cost.
  - For the plain structs, either make them USTRUCTs with UPROPERTY fields under a UPROPERTY member, or switch to
    `TWeakObjectPtr`.

## GC-4 [S3, low] `GWinterBank`: a global set of raw component pointers that outlives the world

- Where: `Private/Winter/LyudWinter.cpp:106` (`TSet<const UPrimitiveComponent*> GWinterBank`). It is filled at `:526-538` from a
  TObjectIterator and read at `:124`.
- It is only compared, never dereferenced, so it cannot crash.
- The problems:
  - It is a process global, not per world. The editor world, PIE worlds and successive maps share it.
  - It keeps stale addresses after PIE stop or `lyud.Load` until the next cover rebuild.
  - A freed component's address can be reused by a new `LyudSeaBed` component, which is then wrongly treated as "not ground".
- Fix: move it into `LyudWinter::FWinterState` (per subsystem), or make it `TSet<TWeakObjectPtr<const UPrimitiveComponent>>`.

## GC-5 [S3, low] AcademToggle binds a core ticker and PostLoadMapWithWorld from a static initializer, never removed

- Where: `Private/World/AcademToggle.cpp:251-267` (the `FAcademOnLoad AcademOnLoad` global constructor calls
  `FTSTicker::GetCoreTicker().AddTicker(...)` and `FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda(...)`).
- The callbacks are static and touch only weak data, so this does not dangle in the editor or modular `-game`.
- In a monolithic (packaged) build, the order of this initializer against `FCoreUObjectDelegates::PostLoadMapWithWorld`'s own
  static construction is unspecified. The binding can be wiped, so lamps would not be re-hidden after a map load, or it can touch
  the delegate before construction.
- There is no removal at module shutdown either.
- Fix: register in the module's `StartupModule` (or `FCoreDelegates::OnPostEngineInit`), keep both handles, and remove them in
  `ShutdownModule`.

## GC-6 [S3, low-med] BirdFlightSubsystem's hero hint widget is not removed on world teardown

- Where: `Private/Birds/BirdFlightSubsystem.cpp:1051-1078` adds `HeroHint` straight to `GEngine->GameViewport`. `Deinitialize`
  (`:886-894`) does not call `SetHeroHint(FText::GetEmpty())`. Every other viewport overlay in the module is removed in
  Deinitialize/EndPlay.
- If the world ends while a hint is up (`lyud.Load`, a map change), the SBox stays on the persistent game viewport with the old
  text. It holds no UObject, so this is cosmetic, not a crash. It does pile up with each reload while a hint shows.
- Fix: `SetHeroHint(FText::GetEmpty());` at the top of `UBirdFlightSubsystem::Deinitialize`.

---

## Checked and clean (no finding)

- **Raw-pointer members:**
  - `BirdLife.h:261 Sounds` is mirrored into the `SoundRefs` UPROPERTY (`BirdLife.cpp:741-748`).
  - Soundscape `Pools`/`Cues` (`LyudSoundscape.h:102-105`): every loaded sound goes into the `CueRefs` UPROPERTY
    (`LyudSoundscape.cpp:255-262`).
  - ForestFloor `FKind::Mesh/MIDs` are mirrored into `KeepAlive`, and KeepAlive is reset only together with the kinds.
  - TreeBakeCache `Baked`/`Assets` are mirrored into `KeepAlive`. A rebake drops the old mesh only after the new one is kept.
  - `BirdLife.h:337 BodyClass` is a native StaticClass.
  - `PeopleLife.h:194 PlacedShown` is a per-frame scratch, compared only within the same Present call.
  - `HeroBike.cpp:389 LastClass` is compared, and dereferenced only right after it is reassigned from a live component.
  - `FLyudUnderwaterEnv::Owner` is owned by its subsystem.
  - Europa climb lambdas capture `A`/`S`, which are UPROPERTY actors that are never destroyed.
- **Anim proxies:**
  - Bird, Animal and Hero proxies copy from UPROPERTY instance fields each PreUpdate. `FBirdClipLayer`/`FAnimalClipLayer::Seq` are
    UPROPERTY.
  - `FLyudBodyAnimProxy` reports `Cur/Prev.Seq` in `AddReferencedObjects`.
- **Statics:**
  - Every static object pointer is weak (cameras, GShownGoods, GActive, TerrainMask cache, GSea, bike cam), or rooted and bounded:
    SeaLife meshes, bottle meshes, diary book meshes, DiaryStyle fonts/paper/map/sketches (unrooted on a language change), and
    GoodsItem MIDs.
  - The `BasicShapeMaterial` raw static is kept alive by the rooted MIDs made from it.
  - AtonStyle static styles use only engine composite fonts.
- **GameInstance subsystems** (Inventory, Save, Quests, BirdBook, BirdGuide, SunGlareMemory, Smoking, Breath, NpcDialogue) hold no
  world object strongly. World references are all `TWeakObjectPtr`. `Save::Pending` is a SaveGame object.
- **Delegates:**
  - Save, inventory and quest publishers use AddWeakLambda / AddUObject and are removed in Deinitialize/EndPlay where it matters:
    GameScreen, Diner, Food, InventoryUI, FlightSubsystem.
  - Dynamic delegates (BirdTame, Soundscape, Clouds AddUnique, RareSky with RemoveDynamic, AtonComputer AddUnique) are weak.
  - NavStrip's OnAssetLoaded lambda is captureless and removed in Deinitialize.
  - The language cvar uses AddStatic.
  - The game menu input preprocessor holds a weak owner and is unregistered.
- **Timers and tickers:** Save uses CreateUObject with a weak world, and Swim uses a weak lambda. BirdTame and DoorTest FTSTicker
  lambdas are test-only and stop when their weak world dies.
- **Async:**
  - TreeBakeCache tasks capture `this`, and Deinitialize spins until `RunningAtomic == 0`. That is safe, but teardown can stall for
    one bake.
  - Winter `Async` jobs capture the raw state pointer, and `~FWinterState` waits on both futures.
  - LyudPrefetch's async load callback is weak and keeps its result in the `Held` UPROPERTY.
  - All ParallelFor calls are synchronous.
- **Slate:**
  - Every widget's owner is `TWeakObjectPtr`. `this` lambdas inside widgets capture the widget itself.
  - The remaining viewport overlays are removed on Deinitialize/EndPlay (apart from GC-6).
- **AddToRoot:** there is no per-world accumulation. Every rooted set is built once per process or per key (bounded).
