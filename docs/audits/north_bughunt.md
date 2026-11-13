# Audit: branch `north` (the North region) over `origin/main`

Tree `/home/user/rd_north`, HEAD `9cd49a636`. Read-only, reasoned from the code (no engine, nothing compiled or run).
Scope: `git diff origin/main...HEAD -- unreal/Lyudmilovsk/Source` (32 files) plus the code those changes call into
(PeopleSim routing, AProceduralGrass, Winter, MeshKit, Soundscape). Paths are relative to `Source/Lyudmilovsk`.

No S1 (crash, data loss, growing leak) was found. Three S2, six S3. Estimated costs are estimates: nothing was measured.

---

### N1 S2: a person who walks the trail keeps the old `Island`, so the next route is a straight line across the map

Confidence: high for the mechanism (read end to end); who hits it depends on the day's choices (the ranger's
`supplies_town` trip, like 5.0 in people.json, is the designed user of the trail; townsfolk can pick `north_view`
through `sun_watch` / `watch_sunrise`).

- `Private/People/PeopleSim.cpp:2179-2191` (`FWorld::Route`, hop kind 5): the trail's points are pushed with kind 4
  and no `ToIsland`.
- `Private/People/PeopleSim.cpp:2606-2627` (the walk step): `P.Island` is only ever changed in `FinishRide` (1023, cable
  car and train), in a snapped `Start` (2282) and at 3395 (home). Walking the trail changes nothing; `Arrive` (2738) does
  not set it either.
- Consumers of the stale value: `Route` 2153 (`int FromIsland = P.Island;` when the person does not start inside an
  indoor place), the slot choice 1840/1897, `Wander` 2848/2878, the festival (`LyudFestival.cpp:881`).

What happens:
- A townsperson walks up (Island stays 0), stands at an outdoor North slot, then goes home: `Hops(0, 0)` is empty, so
  `OutdoorWay(north position, town goal)` runs; the `north` ground patch contains `From`, so it returns a straight line
  in 4 m steps (2080-2094): ~2-3 km through the river, the forest and house walls, and ~700 way points.
- The ranger walks down to town (Island stays 3). From any outdoor town slot the next route is `Hops(3, 0)`: a straight
  line from the town to the trail's NORTH end (the patch contains `To`), then down the trail again. To a North place it
  is `Hops(3, 3)` = no hops = a straight line town -> North without the trail.
- While he is in town `Wander` only offers him North places (`Pl.Island != Island`), and the festival skips him.

Fix (two lines). In `Route`, after the loop that pushes the trail (line 2188):

```cpp
				for (size_t I = 1; I < Way.size(); I++)
				{
					Push(Way[bUp ? I : Way.size() - 1 - I], 4);
				}
				Out.back().ToIsland = (int8_t)Hop.To;      // the trail's far end: on the other island from here on
				Cur = bUp ? Way.back() : Way.front();
```

In the walk step, right after the plain `P.PathI++;` at line 2627 (not the ones in the kind 2 / kind 3 branches):

```cpp
				P.PathI++;
				if (W.Kind == 4 && W.ToIsland >= 0)
				{
					P.Island = W.ToIsland;                 // walked a trail to its end (rides set it in FinishRide)
				}
```

(`W` is a reference into `P.Path`, still valid there. No other kind 4 point carries `ToIsland`: the train sets it on
its kind 3 boarding point only.) A route abandoned half way up the trail still starts from the old island; that is
right for the lower half and wrong for the upper one, and is not covered by this fix.

---

### N2 S2: the camp fire's E is polled on a 20 Hz tick, so most presses are lost

Confidence: high.

- `Private/North/LyudCampfire.cpp:35`: `PrimaryActorTick.TickInterval = 0.05f;` and it never changes.
- `Private/North/LyudCampfire.cpp:294`: `if (bShow && PC->WasInputKeyJustPressed(EKeys::E))` inside `UpdatePrompt`, called
  from `Tick`.

`WasInputKeyJustPressed` is true only during the frame of the press. At 60 fps the fire ticks every third frame, so
two presses of three do nothing (half of them at 30 fps); the prompt "[E] light the fire" is on screen and E seems
broken. `ALyudNorthSpring` already does it right (`LyudNorthSpring.cpp:409`: `SetActorTickInterval(bNear ? 0.f : 0.5f)`).

Fix, in `ALyudCampfire::SetAwake` right after `Super::SetAwake(bInAwake, bInNear);` (line 200):

```cpp
	// near: every frame (E is polled); else the flames' 20 Hz is enough
	SetActorTickInterval(bNear ? 0.f : 0.05f);
```

The per-frame work near the fire is 3 scale writes, 8 ember moves, one light intensity and one scalar parameter.

---

### N3 S2: 60 herb patches build their meshes on the game thread at BeginPlay and rebuild them in play, at any distance, on one beat

Confidence: high for the behaviour; the milliseconds are an estimate.

- `Private/Plants/ProceduralGrass.cpp:544-548`: `BeginPlay` -> `Rebuild` -> `EnsureHerbAssets` (141: 3 MIDs, a leaf
  texture, a 256x128 flower atlas painted on the CPU) + `ApplyHerbSeason` (183) + `Scatter`.
- `Private/Plants/ProceduralGrass.cpp:206-219`: whenever `St.Key` changes, per seed: two `PlantHerbs::Build` (the full
  and the lite plant) and `BuildMeshLods` (`PlantHerb.cpp:876`, two `FMeshDescription`s and a two-LOD
  `BuildFromMeshDescriptions`); two seeds per patch. Fireweed is 2.9k + 0.5k triangles per seed.
- `Private/Plants/ProceduralGrass.cpp:550-557`: every patch ticks every 2 s whatever the distance and calls
  `ApplySeason` when the day moved 0.1.
- `PlantHerb.cpp:742`: the key is the quantized maturity / leaf scale / present / bloom (20 steps each), so in spring,
  in the flowering weeks and in autumn it changes every day or two per patch.
- docs/north.md: 33 fireweed + 19 mountain avens + 6 lingonberry + 6 cotton grass = 64 herb patches in the level, each
  with its own seed, so nothing is shared: 128 two-LOD meshes, 192 MIDs, ~100 textures.

What happens: (a) the level load pays ~128 mesh builds and the textures for plants drawn only within 25..70 m of the
camera, 1.5-2.5 km from where the game starts; (b) in play, with the hero in the town, patches whose key changed
rebuild in the same frame (all patches were last shown on the same day, so they cross the 0.1 day threshold on the
same tick); a clock jump (sleep, the dev panel's date, the Aton wake-up) rebuilds all 64 in one frame. At a guessed
3-8 ms per patch that is a 50-500 ms hitch far from anything it draws. This breaks "the far region costs ~nothing"
and perf checklist items 1 and 5. (Nothing leaks: the old mesh loses its last reference and is collected.)

Fix: a herb patch does nothing until the player is near it, and near patches take turns (one a frame, the rule the
density rebuild already uses).

`Public/Plants/ProceduralGrass.h`, private section:

```cpp
	/** Herb patches: the camera is within the draw distance + 150 m of the patch's rectangle. */
	bool HerbNearPlayer(const FPlantSpecies& S) const;
	bool bHerbBuilt = false;
```

`Private/Plants/ProceduralGrass.cpp` (add `#include "Camera/PlayerCameraManager.h"` and
`#include "GameFramework/PlayerController.h"`):

```cpp
bool AProceduralGrass::HerbNearPlayer(const FPlantSpecies& S) const
{
	const UWorld* W = GetWorld();
	const APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const FVector L = GetActorTransform().InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
	const float Margin = HerbDrawDistance(S) + 15000.f;
	return FMath::Abs(L.X) < HalfExtent.X + Margin && FMath::Abs(L.Y) < HalfExtent.Y + Margin;
}
```

In `Rebuild`, herb branch: add `bHerbBuilt = true;` after `Scatter();`.

`BeginPlay`:

```cpp
	Super::BeginPlay();
	if (!GetHerb())
	{
		Rebuild();          // a herb patch is built by Tick when the player comes near
	}
```

`Tick`, replacing lines 553-567 (the interval code after it stays):

```cpp
	const float Day = CurrentDay();
	static uint64 GrassRebuildFrame = 0;
	bool bWaiting = false;
	if (const FPlantSpecies* Herb = GetHerb())
	{
		// far from the player a herb patch does nothing; ShownDay goes stale and it catches up when he comes
		if (HerbNearPlayer(*Herb))
		{
			const bool bLayout = !bHerbBuilt || CVarGrassDensity.GetValueOnGameThread() != BuiltDensity;
			bWaiting = bLayout || FMath::Abs(Day - ShownDay) > 0.1f;
			if (bWaiting && GrassRebuildFrame != GFrameCounter)
			{
				GrassRebuildFrame = GFrameCounter;      // one patch a frame, tufts and herbs alike
				if (bLayout) Rebuild(); else ApplySeason(Day);
				bWaiting = false;
			}
		}
	}
	else
	{
		if (FMath::Abs(Day - ShownDay) > 0.1f)
		{
			ApplySeason(Day);
		}
		bWaiting = BuiltDensity >= 0.f && CVarGrassDensity.GetValueOnGameThread() != BuiltDensity;
		if (bWaiting && GrassRebuildFrame != GFrameCounter)
		{
			GrassRebuildFrame = GFrameCounter;
			Rebuild();
			bWaiting = false;
		}
	}
```

(A waiting patch already drops its tick interval to 0 until it gets its frame; that code is unchanged.) The local is
named `W`, not `World`, on purpose; no member of the class has that name. A test that reads `DescribeState` right
after BeginPlay must wait one tick.

---

### N4 S3: the winter's grids are sized by the landscape, which the North makes 64 % larger

Confidence: medium (sizes computed from the code and tools/terrain.py; not measured).

- `Private/Winter/LyudWinter.cpp:510-523`: `LandBox` is the bounding box of every landscape proxy; the cover is traced
  at 2 m over all of it, the snow field (`:600`, `S.Field.Setup(S.Cover, 100.f)`) is 1 m, `:862` resizes `DepthRT` to
  the field.
- tools/terrain.py on this branch: the landscape goes from 21 x 25 to 21 x 41 components of 126 m (2646 x 3150 m ->
  2646 x 5166 m).

So: cover columns to trace 2.08 M -> 3.42 M (the build takes 64 % longer under the same per-frame budget), field cells
8.3 M -> 13.7 M (`Depth` float + `Open` byte: about +27 MB, the path mask another +5 MB while building), `DepthRT`
2646 x 5166, and every whole-field pass and the full `UploadDepth(0, 0, W - 1, H - 1)` (959, 1461) 64 % heavier. It is
paid in the town too.

The cheap fix keeps the old cost and gives up 1 m snow relief in the North (the summer snowfields are a terrain
layer and do not need the winter). In `StartBuild` after the loop at 510-513:

```cpp
	// the North (map Y >= 2330 m, ULyudNorthSubsystem::IsInNorth) keeps no snow field: the hero is rarely there
	S.LandBox.Max.Y = FMath::Min(S.LandBox.Max.Y, 790.0 + 2330.0 * 100.0);
```

Whether the North should then show winter snow some other way is the owners' call; if the full field is wanted, this
finding is only the bill.

---

### N5 S3: the winter still treats each forest tile's whole rectangle as forest

Confidence: high.

`Private/Winter/LyudWinter.cpp:593-597` marks `C +- HalfExtent` of every `AProceduralForest` as `CellForest`. This
branch thins the tiles by `ForestCover::At` (`ProceduralForest.cpp:251`) and already taught the forest floor the same
(`ForestFloor.cpp:543-546`), but not the winter: glades and the cut-back edges get the forest's sheltered snow
(`SnowModel.cpp:322`, no drifting) instead of open ground. Visual only.

Fix: `FCover::MarkForest` (`Private/Winter/SnowModel.cpp:222`, and its declaration in SnowModel.h) takes a test:

```cpp
	void FCover::MarkForest(double MinX, double MinY, double MaxX, double MaxY, const std::function<bool(double, double)>& Keep)
	...
				uint8_t& K = Kind[Index(X, Y)];
				if ((K == CellOpen || K == CellNone) && (!Keep || Keep(X0 + X * Cell, Y0 + Y * Cell)))
```

and the caller (add `#include "Plants/ForestCover.h"`):

```cpp
				const bool bCover = It->bUseForestCover;
				S.Cover.MarkForest(C.X - It->HalfExtent.X, C.Y - It->HalfExtent.Y, C.X + It->HalfExtent.X, C.Y + It->HalfExtent.Y,
					[bCover](double X, double Y) { return !bCover || ForestCover::At(FVector(X, Y, 0.0)) >= 0.35f; });
```

---

### N6 S3: synchronous loads on the first approach (steam material, river sound)

Confidence: high that they run in play; the hitch size depends on what is already in memory.

- `Private/North/LyudNorthFx.cpp:107-115`: the first `Emit` (the first fumarole or the camp fire waking, 300 m away)
  does `LoadObject` of `M_North_Steam` (or `M_Smoke`), builds the card mesh and adds 448 instances.
- `Private/North/LyudNorthRiver.cpp:346`: `StartLoop` does `LoadObject<USoundBase>` every time the river becomes near;
  when the asset is missing the failed load is repeated on every approach (only the warning is latched).

Perf checklist item 3. Fix: do both at world start, where a few ms cost nothing.

`LyudNorthFx.h` / `.cpp`: move the body of the `if (Puffs->GetInstanceCount() == 0)` block into a public
`void ALyudNorthSteam::Prepare()` (use the actor's location instead of `Where`), call it from `Emit` when the count is 0,
and at the end of `ULyudNorthSubsystem::OnWorldBeginPlay` add:

```cpp
	if (Things.Num() > 0 || Fumaroles.Num() > 0)
	{
		if (ALyudNorthSteam* S = Steam())
		{
			S->Prepare();
			S->SetActorHiddenInGame(!bShown);
		}
	}
```

River: add `UPROPERTY(Transient) TObjectPtr<USoundBase> LoopSound;` (forward-declare `USoundBase`), set it at the end
of `Setup` with the same `LoadObject`, and in `StartLoop` replace the load with `USoundBase* Sound = LoopSound;`.

---

### N7 S3: the shared steam pool (448) is too small in cold damp air; live puffs are overwritten

Confidence: medium (arithmetic from the model, north.json's 31 fumaroles; not seen running).

`Private/North/LyudNorthFx.cpp:116-117` takes `All[Next]` round robin whether or not that puff is still alive. Standing
in the valley (all 31 awake, the far ones at 0.6): at density 1 about 370 live puffs at most; `Emit` scales life by
`Lerp(1, Density, 0.6)` and the vents' and patches' rates rise with density too (`NorthModel.h:110`,
`LyudFumarole.cpp:302`), so at density 2 (cold and damp, i.e. the season steam looks best) it is 560..900. The oldest
puffs are cut off mid fade: columns shorter than the docs' 6..15 m and a visible pop on the big vents; the camp
fire's smoke and the spring's steam share the same pool.

Fix: `Public/North/LyudNorthFx.h:29` `static constexpr int32 Pool = 1024;` (the tick only writes the live range, so an
idle larger pool costs memory only: 1024 x ~170 bytes), or keep 448 and drop the life scaling to
`Lerp(1.f, Density, 0.3f)` at `LyudNorthFx.cpp:122`.

---

### N8 S3: the North spawns its river, plume and spring in every game world that is played

Confidence: high.

`Private/North/LyudNorth.cpp:159-178`: the subsystem exists in every Game/PIE world and `OnWorldBeginPlay` always
reads north.json, spawns `ALyudNorthSpring` (inside `LoadData`, line 281), the plume and the river's five meshes, and
then ticks; in a dev level (`/Game/Dev/L_*`, the first-person test map) that is a 1.6 km sheet of water, a plume that
writes 12 instances four times a second for ever (`LyudNorthFx.cpp:199, 281`, also while `lyud.North 0` hides it) and a
4 Hz range check with nothing to wake.

Fix, at the top of `OnWorldBeginPlay` after `Super` (include `EngineUtils.h` is already there):

```cpp
	// only where the level has the North (import_north.py's fumaroles)
	if (!TActorIterator<ALyudFumarole>(&InWorld))
	{
		return;         // bLoaded stays false: Tick does nothing
	}
```

and in `ULyudNorthSubsystem::SetShown` after the tag loop, so a hidden plume stops ticking:

```cpp
	for (ALyudNorthPlume* P : N->Plumes)
	{
		if (P) P->SetActorTickEnabled(bOn);
	}
```

---

### N9 S3: `/Game/North` is not cooked (config, only matters for a packaged build)

Confidence: medium (the .uasset files are not in this checkout; judged from the config and the code).

`Config/DefaultGame.ini` on the branch adds `North` and `World` to `DirectoriesToAlwaysStageAsNonUFS` (north.json,
forest_cover.fcv: right) but not to `DirectoriesToAlwaysCook`. `M_North_Steam`, `M_North_Plume`, `M_North_River`,
`M_North_Mud`, `M_North_Water` (`LyudNorthFx.cpp:17-19`, `LyudNorthRiver.cpp:20-21`, `LyudFumarole.cpp:18-19`) are
referenced only by path strings in C++, so a cook leaves them out and the code silently falls back: the river becomes
`BasicShapeMaterial` (an opaque grey sheet), the mud and the spring water become paint, the steam the cigarette smoke.
The uncooked `-game` run is not affected.

Fix: `+DirectoriesToAlwaysCook=(Path="/Game/North")` next to the other lines.

---

## Checked and fine

- **Far-away cost of the North's own actors.** Fumaroles, the camp fire, the spring and the river are
  `ALyudNorthThing`: tick off until `ULyudNorthSubsystem` wakes them (300 m, hysteresis 25 m), loops only within 70 m.
  The manager's tick is a 4 Hz loop over 34 weak pointers plus one nearest-segment search over ~320 river points, and
  an air update every 5 s while nothing is awake. The steam actor stops ticking and hides when the last puff dies. The
  fire's point light casts no shadow and is invisible unless the fire is shown.
- **Checklist 1/2.** Steam and plume write instances with `BatchUpdateInstancesTransforms(.., bMarkRenderStateDirty =
  false)` over the live range only; no ClearInstances/AddInstances, SetMaterial or MID creation per frame anywhere in
  North/. All moving parts go through `LyudNorthFx::Lightweight` (no shadow, out of Lumen and the distance fields); the
  herb HISMs are the same and are culled at 25..70 m.
- **Load cost of code-built meshes.** 16 small lathes for the fumaroles, 5 for the fire, 5 river parts of 128 vertices:
  a few ms in total at BeginPlay; none is rebuilt in play. ~50 MIDs for the fumaroles (three colours per vent that could
  be shared; not worth a finding). The terrain generator (`TerrainGen.cpp`, `LandscapeZScale` 50 -> 100, three new
  layers) runs only in `ULyudTerrainLibrary::BuildTerrain` at import time, never at BeginPlay.
- **GC / lifetime.** Every component, mesh, MID and audio component in North/ is a `UPROPERTY`; the manager holds
  things weakly and unregisters them in EndPlay; no timers, no async lambdas, no delegates bound to longer-lived
  broadcasters. Loops are released with `bAutoDestroy = true` before `FadeOut` (the AttachLoop gotcha is handled in the
  fumarole, the fire and the river).
- **Save / New Game.** `ULyudSaveSubsystem` opens the level again for both (`LyudSave.cpp:176, 190`), so the world
  subsystem and all North actors start fresh. Nothing of the North is saved (the header says so): the places reached,
  the hero's choice at the fire, a hero sitting in the spring. `OnPlaceReached` therefore fires again after a load for
  the place the hero stands in; nothing listens to it yet, so a quest that will must keep its own flag.
- **People data.** Private homes are kept out of `AssignHomes` (1492); `Hops` for island 3 recurses once at most; the
  round (`Wander` 2850-2871) releases the old slot through `Reserve`; `north` has its own ground patch, so people in the
  North get a height. Only N1 is wrong.
- **Snowfields.** They are a landscape layer and a height change made at import; nothing at run time. The herbs ask the
  winter for the snow on the ground (`ProceduralGrass.cpp:191`) the same way the grass does.
- **Mac clang.** Read every new function for `-Wshadow` (locals against members of the own class and outer locals) and
  `-Wunreachable-code-loop-increment`: nothing found. (`ALyudNorthSpring::SpawnFor` names its parameter `Data` in the
  header only, and it is a static function.) API calls into MeshKit, Soundscape, Sky, Wind, Weather, Hero and
  InventoryUI match their declarations on this branch.
- **Not verifiable here:** whether `UHierarchicalInstancedStaticMeshComponent` refreshes its cluster bounds when
  `SetStaticMesh` swaps a herb's mesh for a taller one under existing instances (`ProceduralGrass.cpp:217`). If herbs
  vanish at the screen edge in spring, that is where to look.
