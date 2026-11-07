# Perf/leak/crash audit: plants (Private/Plants, Private/Terrain, Private/World, Private/Bench)

Read-only audit of origin/main as checked out in perfhunt. Paths are relative to
`unreal/Lyudmilovsk/Source/Lyudmilovsk/`. Line numbers are from that checkout.

The game calendar runs in real time by default (`UGameCalendar::TimeScale = 1`, and nothing in the game sets it
higher). That means plant re-shapes caused by the day changing (0.25 day for trees, the herb state key) are rare in
normal play. The costs that matter most are the ones that recur as the camera moves (forest floor) or every frame
(plant ticks). The findings below are ordered by likely real cost.

---

## PL-01 [S2, high] ForestFloor litter field: whole 128x128 field recomputed in one frame every 24 m of travel
`Private/Plants/ForestFloor.cpp:1547-1582` (`UpdateLitterField`), called every frame from `Tick` (`:1638`).

- **When:** the window is snapped to `LitterCell*16` = 24 m. Every time the camera crosses a 24 m line (and every
  half game day), all `LitterN^2 = 16384` texels are recomputed on the game thread in that one frame.
- **What each texel costs:** each one calls `LitterFrom`, which calls `ForTreesNear(At, 1600)`. With `TreeCell = 1000`
  that is about 4x4 `TreeGrid.Find` hash lookups, then a distance test on every tree in those cells (~40-60 at forest
  density) and an `Exp` plus `LeafLitter` for those inside the radius. When the guard is on, a `SiteSoft` lookup is
  added.
- **Estimate:** 16k x (16 TMap finds + ~50 trees) is about 1M operations, roughly 10-25 ms in one frame.
- **Who notices:** anyone walking through the woods (a hitch every ~15 s), running (every ~4 s), or flying as a bird
  (much more often).
- **Fix (pick one or more):**
  - Amortise: compute rows into a back buffer over N frames (e.g. 16 rows a frame), then upload and switch
    `LitterRect` once it is complete.
  - Shift: keep the old texels when the window moves by 16 cells and compute only the new 16-wide strip, which is
    about 1/8 of the work.
  - Off-thread: `LitterFrom` is const and only reads `TreeGrid`/`GClasses`, so a `UE::Tasks::Launch` over rows is
    safe as long as `BuildTreeIndex` does not run during it.

## PL-02 [S2, high] ForestFloor: every 6 m relayout re-commits all ~70 kinds, and each commit scans every item of every cell
`Private/Plants/ForestFloor.cpp:1662-1672` (relayout), `:1712-1719` (2 commits a frame), `:1273-1345` (`CommitKind`).

- **What happens:**
  - Once the camera has moved `Relayout` = 6 m, `DirtyKinds` gets every kind: 18 herb variants, 4 grasses,
    8 seedlings, the leaf kinds, the falling leaves and ~30 debris kinds.
  - Each `CommitKind` loops over all of `Cells` (out to `MaxReach + Margin + 2*CellSize`, a few hundred 8 m cells)
    and over every `FCellItem` in each, only to filter `It.Kind != Index`.
  - It then runs `ClearInstances` + `AddInstances` + `SetCustomData` per instance + `MarkRenderStateDirty` on that
    kind's HISM.
- **Why it costs:**
  - At 2 kinds a frame, one relayout keeps committing for ~35 frames.
  - Walking at 1.5 m/s relays out every 4 s; running or a bird relays out every ~1 s. So the floor spends most frames
    scanning the full item set twice (1e5-1e6 items) and rebuilding two HISMs, even for kinds whose 14-70 m ring
    barely changed.
- **Fix:**
  1. Store items per kind: in `FCell`, a `TArray<TArray<FCellItem>> ByKind` (or one array sorted by kind with
     offsets), so a commit touches only its own items.
  2. Dirty a kind on relayout only when the camera has moved more than about `0.2 * K.Cull` since that kind's last
     commit. Keep a `FVector CommittedAt` in `FKind`.

  Small rings (moss at 15 m) then still follow closely, and logs at 70 m are re-sent about every 14 m instead of
  every 6 m.

## PL-03 [S2, med] ForestFloor LayCell: string building, case-insensitive compares and string hashes per candidate piece
`Private/Plants/ForestFloor.cpp:1111-1270`.

- **Per kind, per cell:**
  - `K.Name.ToString().StartsWith(TEXT("Roots_"))` (`:1163`).
  - `K.Name == TEXT("PineCone") ...` (an FName compared against a literal builds an FName each time).
  - `const FString Nm = ...ToString()` (`:1230`).
- **Per candidate piece:** inside `Want(X, Y)`:
  - `HashCombineFast(Seed, GetTypeHash(Nm))` hashes the whole string.
  - Then up to ~12 sequential `Nm == TEXT("...")` compares, which are case-insensitive FString compares, plus
    `Nm.StartsWith` x4.
- **Volume:**
  - Candidates per cell are `Density * 64`: about 640 per moss kind and 85 per lingonberry variant. A band-0 or band-1
    cell is several thousand candidates, so tens of thousands of string compares per cell.
  - Leaf kinds add a `ForTreesNear(At, 900)` per candidate: 22*64 = 1408 per leaf species per cell.
  - The trace budget charges a flat 20 per `LayCell` whatever it really costs, so a frame can lay ~35 cells.
- **Fix:**
  - At `EnsureKinds`, give each kind an enum `EWant { Bilberry, Lingonberry, ..., Seedling, Cushion, Wood, Stone }`,
    a `bRoots` / `bCone` flag and a precomputed `uint32 PatchSeed`.
  - Make `Want` a switch on that enum.
  - Charge the budget by candidates (e.g. `N / 32`) instead of a flat 20.

## PL-04 [S2, high mechanism / conditional frequency] Falling leaves: 3 ISMs cleared and refilled every frame per tree while any leaf is in the air, and landed leaves re-sent every frame
`Private/Plants/ProceduralPlant.cpp:864-923` (`TickFallingLeaves`, called from `Tick` `:1096` every frame).

- **What happens:**
  - While `Falling` is non-empty, each frame builds 3 fresh `TArray<FTransform> Air[]` (no Reserve).
  - It appends every leaf landed since the last litter layout: `Air[K].Append(Landed[K])` (`:920`). `Landed` is only
    reset in `UpdateLitter`, i.e. at the next non-summary re-shape.
  - It then calls `SetInstances` (`ClearInstances` + `AddInstances`) on all 3 `FallingComponents` (`:921`). The
    instance count changes, so the ISM proxy is recreated every frame.
- **When:**
  - Leaves are released evenly over the interval to the next re-shape (`SpawnFallingLeaves`). Once a tree drops leaves
    there is nearly always one in the air (~10 s of fall per leaf), so the churn becomes continuous.
  - Drops only start from the second re-shape onward (`bSincePrev`). In real-time play that means after about 6 hours,
    or right away whenever the calendar is sped up or moved: the dev panel, `SetNow`, demo films, which all start in
    the 20 Oct leaf-fall season.
- **Cost:** N near deciduous trees x 3 ISM proxy rebuilds per frame, each with up to 400 in the air plus every landed
  leaf since the last layout. This is exactly the "no per-frame ISM proxy rebuilds" item on the perf checklist.
- **Fix:**
  - Give each bucket a fixed pool of `MaxFalling` instances made once. Each frame write the transforms with
    `BatchUpdateInstancesTransforms(0, Xfs, /*world*/false, /*dirty*/false, /*teleport*/true)`, and park unused slots
    at zero scale, so there is no count change and no proxy rebuild.
  - When a leaf lands, add it to the litter ISM once (`LitterComponents[B]->AddInstance`) instead of re-sending
    `Landed` every frame. Alternatively, batch landed leaves into the litter once a second.
  - Alternative: animate the fall in the material, as `AForestFloor`'s `FallingLeaves` kind already does
    ("the material moves them").

## PL-05 [S2, med] Every fruit tree with fruit on the ground line-traces the camera ray every frame, even when far away
`Private/Plants/ProceduralPlant.cpp:1097-1110` -> `AimedFruit()` `:963-989`.

- **What happens:**
  - `Tick` calls `AimedFruit()` whenever `GroundFruit` is non-empty. `AimedFruit` does
    `UGameplayStatics::GetPlayerController` plus a `LineTraceSingleByChannel` from the camera, then checks the
    distance afterwards.
  - `GroundFruit` is filled on the first full-detail `Apply` in autumn ("fell before we came") and is never cleared
    when the tree goes to its summary (bake) beyond 150 m.
  - So every apple or pear tree ever seen up close traces every frame for the rest of the session.
- **Cost:** N fruit trees x 1 trace per frame (~5-15 us each, mostly the same ray). 20-40 orchard trees is about
  0.2-0.5 ms a frame.
- **Fix:**
  - Early out before tracing:
    `if (FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(PickDistance + LastReach * 100.f + 200.f)) return nullptr;`
  - Optionally cache one trace result per `GFrameCounter` in a static that all trees share. The `Hit.GetActor()` cast
    already tells which tree owns the fruit.

## PL-06 [S2, med] ULyudTerrainMask::Get(): weak-only cache, so the mask can be collected and reloaded synchronously, and a missing mask is looked up on every call
`Private/Terrain/LyudTerrain.cpp:27-35`.

- **What happens:**
  - `static TWeakObjectPtr` plus `LoadObject` when it is invalid. Nothing holds the data asset strongly. In a
    `-game` process `RF_Standalone` does not keep it (`GARBAGE_COLLECTION_KEEPFLAGS` is only `RF_Standalone` in the
    editor).
  - After a GC, the next `Get()` blocks on a synchronous package load of the 5 mask arrays (W*H bytes each; tens of MB
    for a 4k landscape).
  - Callers: `AForestFloor::TraceCell` and `LayCell` (per cell while walking, `ForestFloor.cpp:1054,1128`),
    `AProceduralGrass::Scatter`, and the seabed.
  - If the asset is absent (an unbuilt map or branch), `LoadObject` runs a failed package search on every call. The
    seabed already works around this with its own once-a-minute cache (`Sea/LyudSeabed.cpp:50-60`).
- **Fix:**
  - Keep a strong reference once loaded: `Cached->AddToRoot()`, or a `UPROPERTY` on a GameInstance subsystem.
  - Remember a miss (e.g. `static double TriedAt` with a 60 s retry, like the seabed's) so a missing mask is not
    looked up on every call.

## PL-07 [S2, med] AcademToggle LampTick walks every actor of the world twice a second
`Private/World/AcademToggle.cpp:116-162` (ticker registered at `:168`, 0.5 s).

- **What happens:** every 0.5 s `LampTick` does `TActorIterator<AActor>` over the whole game world, with an
  `ActorHasTag` on each actor, a `TInlineComponentArray<ULightComponent*>` per lamp and a `TActorIterator<ALyudSky>`.
  Scene01 has tens of thousands of actors (town, Academgorodok, people, animals, birds).
- **Cost:** about 0.5-2 ms every 0.5 s, a regular spike in the frame graph. `ApplySigns` (`:75-114`) also walks every
  actor and every material slot and runs `LyudText::LoadTexture` (a synchronous load) on the first tick of each map
  and on each language change.
- **Fix:**
  - Cache the lamps once per world: `TArray<TWeakObjectPtr<AActor>>` plus their `ULightComponent*` list, built on the
    first tick after `PostLoadMapWithWorld` and rebuilt when the world changes.
  - Cache the `ALyudSky` as a weak pointer.
  - Iterating ~hundreds of lamps instead of the whole world removes the spike.

## PL-08 [S2/S3, med] LyudPrefetch::FolderReady does a FindObject plus an FName per asset on every call, forever
`Private/World/LyudPrefetch.cpp:79-115` (and `ReadyPackage` `:41-77`).

- **Per call:**
  - Builds `Key` (an FString concat when `Skip` is given) plus a `Folders.Find` string hash.
  - For each path in the folder: `FPackageName::ObjectPathToPackageName` (an FString allocation), `FName(...)` (a
    name-table lookup), `Missing`/`Loading` lookups, and finally `FindObject<UObject>(nullptr, *ObjectPath)`, which
    parses the full path.
- **Callers (per frame):**
  - `BirdLife.cpp:1401`: per near bird still without a body, with a `FString::Printf` of the folder each time.
  - `PeopleLife.cpp:752`: per person body being made; `/Game/Characters/Humans/Anims` has 27 assets.
  - `AnimalLife.cpp:875`.
- **Why it matters:** once a folder is ready it stays ready (`Held` keeps everything), yet the full scan repeats.
  10-50 bird bodies waiting x ~30 assets is 300-1500 FindObject + FName per frame while a flock comes into range.
- **Fix:**
  - Add `TSet<FString> ReadyFolders` (or a `bool` next to `Folders[Key]`). Return true at once when the key is in it,
    and add the key when `bAll` first comes back true.
  - Store package FNames in `Folders` so `ObjectPathToPackageName` and `FName` are built once.
  - At the call sites, build the folder string once per species (e.g. a cached `FString` per species id), not per
    bird per frame. These call sites are [OTHER-THREAD] (birds/people/animals).

## PL-09 [S2 in time-lapse, S3 in real time; high] Herb meshes are rebuilt over the live mesh (the "fresh mesh" fix is missing here)
`Private/Plants/PlantHerb.cpp:794-858` (`PlantHerbs::BuildMesh`, `if (!Mesh)` at `:843`), called from
`ProceduralPlant.cpp:480`.

- **The gap:** `PlantMeshes::BuildMesh` got the fix "in play a re-shape builds a new mesh rather than over the one
  drawn" (`PlantMeshes.cpp:75-85`), because building over a mesh with render resources makes the engine walk every
  static-mesh component, flush the render thread and wait for the GPU. `PlantHerbs::BuildMesh` still rebuilds the
  existing `HerbMesh` in place.
- **Frequency:** each herb, pot plant or hedge does this whenever `PlantHerbs::State().Key` changes (maturity, leaf
  scale, presence, bloom, alive quantised). That is a few times a season in real time, but constant under the dev
  panel's day speed. Many herbs crossing a key on the same day each cause a full flush in the same frame.
- **Fix:** copy the same guard in front of the rebuild:
  `if (Mesh && Mesh->AreRenderingResourcesInitialized() && Outer && Outer->GetWorld() && Outer->GetWorld()->IsGameWorld()) Mesh = nullptr;`
  (`ForestFloor`'s `BuildHerbLods` already always makes a fresh mesh.)

## PL-10 [S2, med] A forest's live-plant swap builds a whole plant's meshes, textures and ~13 MIDs on the game thread, outside the shape budget, again on every return
`Private/Plants/ProceduralForest.cpp:300-332` (`UpdateLive`, `SpawnActorDeferred`/`FinishSpawning`) leading to
`AProceduralPlant::BeginPlay` -> `Apply` -> `EnsureMeshes` (`ProceduralPlant.cpp:287-404`).

- **What a spawn builds:**
  - The shared textures come from the bake cache, but each spawned plant still builds its own `BuildLeafCluster`,
    `BuildSingleLeaf`, `BuildFruit`, `BuildBlossom` + `BuildBlossomAtlas` (a texture), `BuildFruitDetail` (a texture),
    a skeleton, `BuildWood` (~50 ms for a grown poplar, per the code comment) and 13 MIDs. Then it runs a ground grid
    of up to 61x61 line traces (`GroundAt`) and, in autumn, spawns fallen-fruit actors with traces.
  - None of this goes through `lyud.Plants.ShapeBudgetMs`, which covers only `Tick` re-shapes. `LivePerTick` limits
    the count, not the milliseconds.
- **Repeats:** the plant is destroyed beyond 1.3 x `LiveDistance` (`:279-286`), so walking back and forth repeats the
  whole cost.
- **Fix:**
  - Cache the per-species, per-seed derived assets (leaf cluster, single leaf, fruit, blossom mesh and atlas, fruit
    detail) in `UTreeBakeCache`, next to `FSpeciesAssets`, and hand them over like `SharedBarkAlbedo`.
  - Count the live spawn's `Apply` time against the same per-frame budget (or defer `Apply` to the first budgeted
    `Tick`).
  - Or keep the last 2-4 live plants pooled and hidden instead of destroying them at once.

## PL-11 [S3, high] PlantSpeciesLibrary::Find is a linear scan of large structs, called many times per plant
`Private/Plants/PlantSpecies.cpp:1275-1278`. Used by `AProceduralPlant::GetSpecies()` (`ProceduralPlant.cpp:160`),
`IsHerb`, `PotKind`, `WantsSummary`, and every `Apply`/`Tick`. It is also used by `ProceduralForest::Interactive` per
tree in range every 0.25 s and by ForestFloor `UpdateSeason`/`BuildKind`.

- **Cost:** about 100 species of ~1-2 KB each are scanned comparing FNames, so a lookup touches ~100 cache lines.
  Several hundred plants x ~5 lookups per 0.2 s, plus the per-frame `GetSpecies()` in the fruit-prompt path.
- **Fix:** build a `TMap<FName, int32>` index once inside `All()`'s static initialiser and look up through it. Also
  cache `const FPlantSpecies*` in `AProceduralPlant` when `BuiltSpecies` changes (the library is immutable after
  init).

## PL-12 [S3, med] Every AProceduralPlant ticks every frame, herbs and potted plants included, though they need nothing per frame
`Private/Plants/ProceduralPlant.cpp:67-70`, `:1093-1185`.

- **What happens:** herbs re-check twice a second (`UpdateInterval >= 0.5`) and trees every 0.2 s. Only the falling
  leaves and fruit drops need per-frame work.
- **Why it matters:** hundreds of beds, pots, hedges and garden trees each pay the per-actor tick dispatch plus
  `TickFallingLeaves`, the fruit checks and the interval bookkeeping every frame.
- **Fix:** `SetActorTickInterval(UpdateInterval)` for herbs and for trees with no falling leaves and no pending fruit.
  Switch back to every frame (`SetActorTickInterval(0)`) only while `Falling`, `PendingDrops` or `PendingFruit` are
  non-empty (DeltaSeconds is already used correctly).

## PL-13 [S3, med] Each fallen fruit makes its own MID plus FName-from-Printf per parameter
`Private/Plants/FallenFruit.cpp:59-77`.

- **What happens:** every `AFallenFruit::Init` does `UMaterialInstanceDynamic::Create` and `CopyParameterOverrides`,
  plus `FruitDataFloats` x `SetScalarParameterValue(*FString::Printf(TEXT("Fruit%d"), I))`.
- **Why it matters:** an autumn orchard spawns dozens to hundreds, each a unique MID. That means no draw batching,
  plus game-thread spikes when a tree's whole "fell before we came" set spawns in one `Apply`.
- **Fix:**
  - Pass per-fruit data through custom primitive data (`Mesh->SetCustomPrimitiveDataFloat(I, Data[I])` with the
    material reading `PerInstanceCustomData`/`CustomPrimitiveData`) on one shared MID per tree.
  - At least make the names `static const FName FruitParams[]`.

## PL-14 [S3, med] TreeBakeCache never evicts kinds; in a time-lapse every whole year of a growing tree adds a full 4-LOD mesh plus 4 MIDs for good
`Private/Plants/TreeBakeCache.cpp:22-49`, `:110-117`.

- **What happens:** `Baked`/`LodsOf` are keyed by `AgeTenths`. A summary plant asks for `RoundToFloat(Age)` and a
  forest seedling kind for its fixed age.
  - The KeepAlive fix only drops the old mesh when the same key is re-baked.
  - A plant with `bDemoGrowth`, the dev age slider, or a long session asks for a new key every year of age, and every
    old key's mesh and MIDs stay in `KeepAlive` forever.
  - Bounded in real-time play (one new key per plant per real year), unbounded in demo films and dev time-lapses.
- **Fix:**
  - Keep a `LastUsedFrame` per `FTreeBaked`, updated in `Request`.
  - Every few seconds in `Tick`, drop entries unused for more than 60 s that no forest group uses (forest groups are
    re-requested every 0.25 s, so they stay fresh) and remove their objects from `KeepAlive`. Components still drawing
    them keep them alive until they switch.

## PL-15 [S3, med; plain bug] lyud.Academ 1 turns tick on for every Academ actor, including those that keep their tick off
`Private/World/AcademToggle.cpp:31-39`.

- **The bug:** `ApplyToWorld(World, true)` calls `SetActorTickEnabled(true)` on every Academ-tagged actor. Actors that
  start with tick disabled, or turn it off themselves (`ALyudWires` has `bCanEverTick = false`, so it is harmless
  there; other Academ props, doors, vines, NPC parts may not be), start ticking after an off/on cycle. That affects the
  bench's `on` pass, so the A/B is not like-for-like.
- **Fix:** on hide, store which actors were ticking (`TSet<TWeakObjectPtr<AActor>>`), and on show re-enable only
  those.

## PL-16 [S3, low-med] ProceduralGrass: synchronous scatter at BeginPlay
`Private/Plants/ProceduralGrass.cpp:148-306`, `:323-331`.

- **What happens:** each patch builds a `TActorIterator` ignore list of every plant, fruit and grass actor
  (`:175-180`), giving O(patches x actors). It then runs up to one trace per ground cell and adds up to 400k instances
  with no `Reserve` on `Near`/`Far`/`NearDry`/`FarDry`, plus a per-instance `SetCustomDataValue` loop. All of it runs
  at BeginPlay, so it is load-time only, but it shows up as the long first frame.
- **Fix:**
  - Trace with `FCollisionObjectQueryParams(ECC_WorldStatic)` instead of Visibility plus an ignore list (plants and
    grass are Movable or have no collision), or build the ignore list once in a static per world.
  - `Reserve` from `(X1-X0)*(Y1-Y0)`.
  - Use `AddInstances` with `PerInstanceSMCustomData` filled directly.

## PL-17 [S3, low] LyudPrefetch holds every body it ever loaded until the world ends
`Public/World/LyudPrefetch.h:36-37`. This is by design ("held until the world ends"). It is listed as a memory
ceiling to watch: every person model, animal breed folder and bird species folder (meshes, clips, coats, calls) that
ever came near stays resident. If memreport shows these dominating, release entries whose last `Ready` call is older
than a few minutes.

---

## Checked and clean (no finding)
- **UTreeBakeCache:** the worker lambda captures `this`, but `Deinitialize` waits for `RunningAtomic`. Its UObject
  pointers are kept through `KeepAlive`. The game-thread finish is rate-limited.
- **AProceduralForest:**
  - Ticks at 0.25 s.
  - `SetHidden` uses `UpdateInstanceTransform(..., dirty = true)` once per swap, not per frame.
  - Groups are `RF_Transient` components destroyed in `Clear`.
  - `TActorIterator` runs only in `Scatter`, at BeginPlay.
- **AForestFloor:**
  - `FKind` holds raw `UStaticMesh*`/MID pointers, but they are all mirrored into the `KeepAlive` UPROPERTY. Seedling
    meshes are owned by the bake cache and swapped by version.
  - `ClearKinds` in both EndPlay and BeginDestroy is safe (the array is reset).
  - `BuildSites` (file read plus a whole-world component walk) runs once after the forests have laid out their trees.
  - `UpdateWeather` sets 2 MPC vectors per frame. The smoothed values converge, and the MPC only updates on change.
- **AProceduralVine:** only a few vines, traces budgeted per frame, re-shape on 0.5 s / day change; wood goes through
  the fixed `PlantMeshes::BuildWood`. (`Apply` does `ClearInstances` + `AddInstances` on its leaf HISMs, but only on a
  day change.)
- **ULyudLadderSubsystem / ULyudLadderClimber:**
  - The tick loop only runs over the registered ladders.
  - The climber's tick is enabled only while it is attached.
  - Weak pointers are used throughout.
- **ALyudWires:** one-time JSON and mesh build at BeginPlay on Lvl_Scene01 only; never ticks.
- **ULyudNavStripSubsystem:** the `OnAssetLoaded` handle is removed in Deinitialize.
- **ULyudBenchmark:** ticks only while a run is active. `TActorIterator<AProceduralPlant>` runs per frame only while
  `lyud.Bench.LiveHide` is in use; that is bench-only and acceptable.
- **Terrain/TerrainGen:** editor-only generation, no play-time paths.
- **APlantDevTool:** always spawned and ticks every frame, but does nothing without a selected plant.

---

## Fixes applied

Paths relative to `unreal/Lyudmilovsk/Source/Lyudmilovsk/`. Not built here (the lead builds).

- **PL-01: done.** Files: `Private/Plants/ForestFloor.cpp` (`UpdateLitterField`, new `LitterRows`, `FinishLitterField`) and `Public/Plants/ForestFloor.h` (`LitterBack`, `LitterRow`, `LitterBuildCentre/Day`, `bLitterBuildGuard`).
  - A one-step move of the window (≤ 48 m, the old field still covers the camera) or a day change is now worked out 16 rows a frame into a back buffer, with the day and guard captured at the start.
  - When all rows are done it uploads once and switches `LitterRect`, so the result is the same field, about 8 frames later.
  - These still compute in one frame, as before: the first field, a jump (teleport, the floor shown again, a layout reset) and `RebuildAround`'s forced one.
  - `ResetLayout` and the hide toggle stop any build that is under way.
  - Verify: walk or fly through the woods with `stat unit` or Insights, and the 24 m hitch is gone. The ground litter looks the same (lyud.ForestFloor.Debug 1 / shoot the litter).
- **PL-02: done.** Files: `Public/Plants/ForestFloor.h` (`FCell::Items` replaced by `FCell::ByKind`, one array per kind) and `Private/Plants/ForestFloor.cpp` (`LayCell` Place, `CommitKind`, `AuditMap`, the band-0 re-lay in `Tick`).
  - `CommitKind` now walks only its own kind's items, in the same order as before.
  - `FKind::CommittedAt` is new. On a relayout a kind is dirtied only once the camera is `Clamp(0.2*Cull, Relayout, 0.8*Margin)` from its last commit, which stays inside the kind's margin.
  - The falling-leaves kind (placed to its cull with no margin) still follows every relayout.
  - Season, snow and band-0 re-lays still dirty kinds as before.
  - Verify: Insights `CommitKind` time per frame while running. Logs and stumps are re-sent about every 9.6 m instead of 6 m. No pop-in at the cull edges.
- **PL-03: partial (precompute only).** File: `Private/Plants/ForestFloor.cpp` `LayCell`.
  - The kind's want-class (an enum), the seedling share, the stick/branch flag and the patch seed `Ps` are now worked out once per kind per cell instead of per candidate. `Want` is a switch.
  - The cone and bark-curl names are static FNames, and the per-tree `K.Name == TEXT(...)` compare is hoisted.
  - Same values, same order. The budget charged by candidates is not done (it changes pacing).
- **PL-04: done (small form).** File: `Private/Plants/ProceduralPlant.cpp` `TickFallingLeaves`.
  - The air-plus-landed arrays are reserved and padded to a multiple of 32 with 1e-4-scale instances at the root.
  - When the instance count is unchanged (a landing moves a leaf from air to landed, so the count stays the same), only `BatchUpdateInstancesTransforms(..., dirty=false, teleport=true)` runs. The engine sends per-instance transform deltas (`PrimitiveInstanceDataManager.TransformChanged`), with no clear, refill or proxy rebuild.
  - Otherwise the old `SetInstances` path runs.
  - Verify: in the leaf-fall season with the calendar sped up, `stat scenerendering` / Insights shows far fewer ISM proxy recreations near deciduous trees. Leaves look the same.
- **PL-05: done.** `AimedFruit` returns early (no trace) unless some ground fruit of the tree is within `PickDistance` of the pawn, which is exactly the existing final check. Verify: the "[E] pick up" prompt still shows at an apple. Insights `FruitPick` traces drop to zero away from orchards.
- **PL-06: done.** File: `Private/Terrain/LyudTerrain.cpp` `ULyudTerrainMask::Get`.
  - It tries `FindObject` first, then `LoadObject` at most once a minute while the mask is missing.
  - In a game (`!GIsEditor`) a loaded mask is `AddToRoot`ed, so a GC no longer drops it and causes a synchronous reload.
  - Verify: no `DA_TerrainMask` package loads after the first in a long -game soak (`-LogCmds="LogStreaming Verbose"`).
- **PL-07: done.** File: `Private/World/AcademToggle.cpp` (`FLamp`, `FLampCache`, `LampCacheFor`, `LampTick`).
  - Each world's lamps (weak actor, weak lights, the Lit tag) and its `ALyudSky` are found by one world walk.
  - The cache is rebuilt once a minute (every 5 s while there is no sky) and reset on `PostLoadMapWithWorld`. `LampTick` iterates only the cached lamps.
  - `ApplySigns` is unchanged (it still runs on the first tick and on a language change).
  - Verify: the 0.5 s spike is gone (Insights `LampTick`). Flats are lit at night and the room lamp comes on near the player.
- **PL-08: done (inside LyudPrefetch).** Files: `Public/World/LyudPrefetch.h` (the new `FFolder` struct: paths, package FNames, `bReady`) and `Private/World/LyudPrefetch.cpp` `FolderReady`.
  - Package names come from `FAssetData::PackageName` once.
  - Once every asset of a folder has been seen ready, the folder answers true at once. Its found objects are added to `Held`, so the answer stays true.
  - Not done: the per-bird/person/animal `FString::Printf` at the call sites (`BirdLife.cpp:1401`, `PeopleLife.cpp:752`, `AnimalLife.cpp:875`), which are other areas.
  - Verify: bodies still appear. Insights shows no `FindObject` from `FolderReady` once flocks are loaded.
- **PL-09: done.** File: `Private/Plants/PlantHerb.cpp` `PlantHerbs::BuildMesh`.
  - In a game world a mesh with render resources is no longer rebuilt in place. A fresh one is made, the same guard as `PlantMeshes::BuildMesh`.
  - Both callers already assign the returned mesh (`ProceduralPlant.cpp:480`, `Sea/LyudSeabed.cpp:220`; the seabed's "old mesh is reused" comment is now stale).
  - Verify: dev panel day speed over herb beds gives no render-thread flush hitches. Herbs still change shape.
- **PL-10: skipped** (large; per-species asset caching in the bake cache).
- **PL-11: not assigned.**
- **PL-12: done.** Files: `Private/Plants/ProceduralPlant.cpp` (new `UpdateTickRate`, called at the end of `Tick` via `ON_SCOPE_EXIT` and after `Apply` in `BeginPlay`, `Refresh`, `ShowAt` and `OnGameLoaded`) and `Public/Plants/ProceduralPlant.h`.
  - A plant ticks every frame only while it has falling leaves, pending drops, pending fruit, landed leaves to send, a pick prompt, a ground fruit within PickDistance+15 m of the pawn, or a re-shape that is due but deferred by the shared budget.
  - Otherwise it ticks once per `UpdateInterval` (0.2 s trees, 0.5 s herbs) through `PrimaryActorTick.UpdateTickIntervalAndCoolDown`. The interval gate allows a 1 ms slack.
  - Verify: `dumpticks` shows plants with a 0.2/0.5 interval. Leaf fall, fruit drops and the pick prompt still run smoothly.
- **PL-13: partial.** File: `Private/Plants/FallenFruit.cpp` `Init`. The `Fruit%d` parameter names are a static FName table (32), not a Printf per value. The one MID per fruit stays: custom primitive data needs a material change.
- **PL-14: skipped** (not trivial: eviction with the forest-group users).
- **PL-15: done.** File: `Private/World/AcademToggle.cpp` `ApplyToWorld`. On hide it remembers (weakly) which Academ actors were ticking. On show it re-enables only those, and stale entries are dropped. Verify: `lyud.Academ 0` then `1`, and `dumpticks` matches the count before the toggle.
- **PL-16, PL-17: skipped** (not assigned; load-time / by design).
