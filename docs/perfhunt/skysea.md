# skysea audit: Private/Sky, Private/Sea, Private/Europa (+ Public headers)

Read-only audit of branch perfhunt. Paths below are relative to `unreal/Lyudmilovsk/Source/Lyudmilovsk/`.
Tick paths were checked: ALyudSky, ALyudRareSky, ALyudSea, ALyudAirlock and ALyudEuropaSite tick as actors.
ULyudSeabedComponent and ULyudSwimComponent tick as components. ULyudCloudSubsystem, ULyudSunGlare,
ULyudUnderwaterSubsystem and ULyudEuropaSubsystem tick as tickable world subsystems. ULyudSunActivity and
ULyudSunset run once a frame from ALyudSky::UpdateSky / UpdateSkyMaterial.
The UObject members in these files are all UPROPERTY/TObjectPtr or TWeakObjectPtr. I found no GC-dangling raw pointers.

---

## SKY-1 [S2, high] On Europa (space view) the sun, moon and sky light change state twice every frame
`Private/Sky/LyudSky.cpp:363-367, 390-392, 436-443` (ALyudSky::UpdateSky, called from Tick every frame in game)

With `bSpaceView` set, every frame:
- `SetBloomScale(Shafts)` runs because BloomScale is 0 from the previous frame (by Europa daytime, Shafts = 0.2*SunShaftBloom > 0). Then line 442 calls `SunLight->SetBloomScale(0.f)`.
  `ULightComponent::SetBloomScale` calls MarkRenderStateDirty. So the sun's light proxy is rebuilt twice a frame, and its VSM cache is dropped every frame.
- `MoonLight->SetVisibility(MoonLux > 1e-4f)` (line 390). MoonPos is the planet here, which is fixed over a tidally locked moon, so this is true while the planet is up. Line 440 then calls `SetVisibility(false)`.
  That is two visibility changes, so two render-state re-creates of a directional light every frame.
- `SkyLight->SetIntensity(SkyK)` (line 365) runs because Intensity was set to SpaceSkyLight on the previous frame. Line 441 then sets it back. Cheaper (a fast update), but pointless.

Cost: 2-4 light proxy re-creates per frame for the whole Europa visit, plus a full sun VSM re-render each frame.
Fix: work out the final values first, then apply each once.
```cpp
const float WantBloom = bSpaceView ? 0.f : Shafts;
if (!FMath::IsNearlyEqual(SunLight->BloomScale, WantBloom, 1e-3f)) { SunLight->SetBloomScale(WantBloom); SunLight->SetBloomThreshold(SunShaftThreshold); }
const bool bMoonVis = !bSpaceView && MoonLux > 1e-4f;   MoonLight->SetVisibility(bMoonVis);
const float WantSky = bSpaceView ? SpaceSkyLight : SkyK; if (!FMath::IsNearlyEqual(SkyLight->Intensity, WantSky, 1e-3f)) SkyLight->SetIntensity(WantSky);
```
Then delete the overrides in the `if (bSpaceView)` block.

## SKY-2 [S2, med] Sun and moon directional lights are rotated every frame, so the VSM cache is invalidated every frame
`Private/Sky/LyudSky.cpp:325-326` (UpdateSky from Tick, every game frame)

`SunLight->SetWorldRotation(...)` and `MoonLight->SetWorldRotation(...)` run unconditionally with the continuous game-clock direction.
r.Shadow.Virtual.Enable=1 (Config/DefaultEngine.ini:26), and any rotation of a directional light invalidates all its cached VSM pages.
So the sun's shadow clipmaps are re-rendered in full every frame, all day, instead of being cached. That is several ms of GPU in a dense scene.
A related source of render-state churn in the same function:
- line 352: `CloudScatteredLuminanceScale` + MarkRenderStateDirty at a 1e-3 threshold.
- line 362: `SetBloomScale(Shafts)` at a 1e-3 threshold. It also calls MarkRenderStateDirty, and Shafts drifts with LowSun all day in late autumn and winter.
Both re-create the sun proxy every few frames while the sun is low.

Fix: quantise the light direction. Keep `LastSunDir`/`LastMoonDir` and only call SetWorldRotation when the angle passes ~0.05 deg, or at most every 2-5 s, the way time-of-day setups usually do. The sky material's SunDir can stay continuous.
Raise the bloom and cloud-scatter thresholds to relative ~2%, or update them on the same coarse step.

## SKY-3 [S3, med] The sky atmosphere's render state is re-created almost every frame
`Private/Sky/LyudSky.cpp:403-406` (`SetMieScatteringScale` guarded by an absolute 1e-6 on a value of about 0.0045) and `:408-411` (MieAnisotropy, 1e-3)

Mie depends on the eased GetSkyHazeScale and on SunAlt, so the 1e-6 guard trips nearly every frame.
Each USkyAtmosphereComponent setter calls MarkRenderStateDirty, which destroys and re-creates the atmosphere proxy.
ULyudSunset::ApplyAtmosphere (`Private/Sky/LyudSunset.cpp:143-163`) already uses a relative 2e-3 `Changed()` test.
Fix: use the same relative test here (`Changed(Atmosphere->MieScatteringScale, Mie)`).

## SEA-1 [S2, high] The seabed rebuilds its whole disc, with thousands of complex traces, every time the camera crosses an 8 m tile
`Private/Sea/LyudSeabed.cpp:557-569` (TickComponent), which calls `Rebuild` at `:257-430`

Each tile change (or each `Clear` followed by re-entry, see SEA-3) regenerates every tile within Radius: 40 m means about 100 tiles of 8 m.
Every candidate placement calls `Place` → `GroundZ` → `ALyudSea::GroundZ`, which is a **complex** (`FCollisionQueryParams(..., true)`, `LyudSea.cpp:421`) LineTraceMulti over 230 m.
Eelgrass alone tries up to 150 placements per tile (NE = Weed*150*...). That is easily 5-15k traces per rebuild.
The rebuild then runs ClearInstances + AddInstances on all 36 ISMs, which is a full ISM render rebuild.
A swimmer at 1-2 m/s crosses a tile every 4-8 s. A player bird skimming the water (bNear holds up to 8 m over the surface) crosses one every ~1 s. Each crossing is a visible hitch.
Fix, any of these, cheapest first:
- (a) Cache per-tile output in a `TMap<FIntPoint, FTileOut>`. When the tile changes, generate only the new tiles and drop the ones out of reach, then rebuild the ISMs from the cache (no traces). It is deterministic per tile, so the cache is exact.
- (b) Take the bed height from the sea's own 4 m depth map (`Sea->DepthAtM` + DatumZ) instead of a trace per plant. Keep traces only for big rocks.
- (c) Time-slice the tiles over frames.

## SEA-2 [S2, med] The sea depth map does 8000 complex multi-traces per frame after every level load
`Private/Sea/LyudSea.cpp:933` `StepDepthMap(8000)` → `:385-399` → `GroundZ` (complex LineTraceMultiByObjectType, `:416-432`)

DepthCellM = 4 m with a clamp of 1024x1024 over the landscape bounds, so up to about 1M traces run at 8000 per frame for ~130 frames.
At a few µs per complex multi-trace over 230 m, that is roughly 20-80 ms per frame for the first couple of seconds of every session. The log line "last slice … ms" measures only the final slice.
Before the first slice, StepDepthMap also builds a TArray from a TActorIterator<ALandscapeProxy> each frame, and does a one-off TObjectIterator<UPrimitiveComponent> over every primitive in the process (`:364`).
Fix:
- Budget by time instead of by cells: `while (... && FPlatformTime::Seconds() - Start < 0.002)`.
- Use `bTraceComplex=false`. Landscape collision is a heightfield either way, and the tagged bank/island can have simple collision.
- Or read heights with `ALandscapeProxy::GetHeightAtLocation` for the landscape cells.
- Replace the TObjectIterator with a TActorIterator plus a component tag check.

## SEA-3 [S2, med] The sea-life actor and the seabed are torn down and rebuilt at the "near the water" boundary, with no hysteresis
- `Private/Sea/LyudUnderwater.cpp:394-399`: `if (!bNear ...) Release()`. Release destroys ALyudSeaLife (all fish ISMs and MIDs, the mote mesh), clears the fish sim and empties `Env->Beds` (the per-metre trace cache).
  The next frame that is near again runs SpawnActor and `BuildMoteMesh` (a 1800-quad `BuildFromMeshDescriptions` at runtime), creates new ISMs and MIDs per species, re-seeds the fish, and re-traces the beds.
  bNear = `Depth > 1 m && Eye.Z < Level + 5 m`. A bird or player on a pier, or wading along the waterline, flips it repeatedly.
- `Private/Sea/LyudSeabed.cpp:540-546`: `if (!bNear) Clear()` (ClearInstances on all ISMs) and a full `Rebuild` on return (SEA-1).
  Here bNear compares the eye with **SampleSurface's wave-displaced Z** (`Eye.Z < Here.SurfaceZ + 800`). With Hs of 1-3 m, the boundary moves with every passing wave, so an eye near 8 m over the water (bird flight, a high pier) toggles Clear/Rebuild every wave period.

Fix:
- Hysteresis: enter at 5 m / 8 m, leave at 7 m / 11 m, and use the still level (`Sea->SeaLevel()`), not the wave crest.
- Release only after ~20-30 s away. Until then just hide (`SetActorHiddenInGame` / `SetVisibility(false)`).
- Build the mote mesh once per subsystem, not once per spawned actor.

## SKY-4 [S3, high] ULyudSunActivity formats a debug FString every frame
`Private/Sky/LyudSunActivity.cpp:132` (`Update` is called every frame from ALyudSky::UpdateSkyMaterial)

`DebugLine = FString::Printf(...)` takes 12 arguments and allocates every frame whether or not anyone reads it.
Fix: format it only when `CVarSunDebug` is set or when GetDebugLine() is asked (store the numbers, build the string lazily). ULyudSunset already throttles its own debug line to 1 Hz.

## SKY-5 [S3, high] ALyudRareSky builds an FString per frame
`Private/Sky/LyudRareSky.cpp:330`: `Now.AuroraPhase = ANSI_TO_TCHAR(PhaseName(...))` heap-allocates an FString every tick.
Fix: store the enum (or a `const TCHAR*`) and convert in DebugReport only.

## SEA-4 [S3, high] ULyudUnderwaterSubsystem does per-frame work even far from the sea
`Private/Sea/LyudUnderwater.cpp:390` calls `UpdateCollection` every frame before the bNear test. It does:
- a TActorIterator<ALyudSky> (`:445`; a second one at `:432`/`:445` when near);
- `MeanCurrentAt`, which includes `HabitatAt` with two 5-9 octave noise lookups;
- `DepthAt`;
- 3 MPC writes.

`ULyudSeabedComponent::UpdateJellies` → `DayNow` (`LyudSeabed.cpp:92-98`) also runs a TActorIterator every frame.
Fix: cache `TWeakObjectPtr<ALyudSky>` (as ALyudSea does). Skip UpdateCollection while far and not under water, after one final write of `Eye.x = 0`.

## SEA-5 [S3, med] Fish sim: a current evaluation and noise lookups per fish per frame
`Private/Sea/FishModel.cpp:404` (`Env.Current(F.Pos)` for every fish each step) → `ULyudUnderwaterSubsystem::CurrentCm` (`LyudUnderwater.cpp:270-300`)

Each call does DepthAt (bilinear), `HabitatAt` (two Patch noises of 5 and 9 octaves) and LyudSeaCurrent::Mean.
Bottom fish also do 1-3 `Env.Bed` lookups (4 TMap finds each) per frame.
That is fine at tens of fish, but it scales linearly with lyud.SeaLife.Density.
Fix: evaluate the current once per group (school) or once per 6 m cell per frame, and reuse it for the members.

## SEA-6 [S3, low] `ScriptHolders.Remove(nullptr)` never removes stale weak keys
`Private/Sea/LyudSea.cpp:675` (KeepSwimmerHeadUp, every frame)

`TSet<TWeakObjectPtr<const UObject>>::Remove(nullptr)` matches only a null weak pointer. A holder that died while scripted keeps its index and serial number, so it stays in the set forever.
After that, `ScriptHolders.Num() > 0` permanently disables the sea's swimmer hold.
Today only ULyudEuropaSubsystem uses it, and it un-scripts in EndClimb, so the practical risk is low.
Fix: `for (auto It = ScriptHolders.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();`

## EUR-1 [S3, med] Leaving the world while on Europa leaves `lyud.Rain.Fx` at 0 for the rest of the process
`Private/Europa/LyudEuropa.cpp:1765-1776` saves `lyud.Rain.Fx` and sets it to 0 in SetOnEuropa(true). `Deinitialize` (`:1146-1150`) never restores it.
Loading a save, quitting to the menu, or travelling while on Europa leaves rain FX off in every later world. Movement gravity is per pawn, so it does not carry over.
Fix: in Deinitialize, `if (bOnEuropa && !SavedRainFx.IsEmpty()) Rain->Set(*SavedRainFx, ECVF_SetByCode);`

## EUR-2 [S3, med] Synchronous loads during play on Europa
- `Private/Europa/LyudEuropa.cpp:1881`: `LoadObject<USoundBase>(MusicPath)` runs at the moment the planet line plays. This is a sync load of the music track, a hitch right at the story beat.
- `BuildGround` (`:870-950`): `ReadJson("Europa/moon.json")`, LoadObject of every moon tile, and `LoadObject` of M_EU_LeadWater (`:974`).
  These are sync when the prefetch folder is not ready (the "built at once" path via `SetOnEuropa → SetShown → EnsureGround`).
Fix: add the music path to the ULyudPrefetch set used for /Game/Europa/Moon, or start an async load when the player enters the airlock.

## Minor (S3, not worth a ticket alone)
- ALyudSky, ULyudCloudSubsystem (`LyudClouds.cpp:505-545`) and ALyudRareSky make about 40 `SetVectorParameterValue(TEXT("..."))` calls per frame. Each builds an FName from a literal (a hash lookup) and does a linear search in the MID.
  Static FNames would trim this. `CVarCloudPreset.GetValueOnGameThread()` copies an FString each frame (`LyudClouds.cpp:340`).
- `ALyudSea::Tick` calls `EnsureMaterial()` every frame (`LyudSea.cpp:940`), re-setting 2 textures and 3 scalars that do not change. The engine early-outs, so it only costs the searches. It only needs to run when the MID or textures change.
- `ALyudSky::UpdatePlayerChannels` allocates a TArray (GetAttachedActors) and does one trace per frame (`LyudSky.cpp:753-760`). This is fine as is.
- `LyudSeaLife` statics (`SeaLifeMeshes.cpp:167`): AddToRoot meshes in raw static pointers. They are bounded (one per kind, built once) and intentional, so they are not a leak.

Checked and clean: ULyudSunGlare (traces throttled, retina upload throttled and quad-buffered); ALyudAirlock and ALyudEuropaSite ticks (lid updates only while moving; site tick is off while hidden; UpdateLook runs at 2 Hz); fish ISMs and jellies (BatchUpdateInstancesTransforms with no dirty, flagged out of Lumen/DF); wave table at 2 Hz; ALyudRareSky delegate unbound in EndPlay; SpaceObserver lambda captures a weak pointer; KitData is cached; the swim component.

## Fixes applied

Paths relative to `unreal/Lyudmilovsk/Source/Lyudmilovsk/`. Not built here (the lead builds).

- **SKY-1 done.** `Private/Sky/LyudSky.cpp` ALyudSky::UpdateSky: the space-view values (SkyK = SpaceSkyLight, Shafts = 0, moon light off) are worked out before the guarded setters, and the override block at the end is gone. Each light is now set once per frame and only on change. Verify: on Europa (`lyud.Europa.*` / test_europa) `stat scenerendering` light proxy churn is gone; the picture is the same (no moon light, the same sky light, no god rays).
- **SKY-2 done (small threshold only).** Same function: a `TurnLight` lambda calls SetWorldRotation only when the light's forward differs from the wanted one by more than 0.01 deg (|cross| > 1.745e-4) or points the other way. A clock jump or a big step still applies at once. The sky material's sun is unchanged (continuous). Bloom and cloud-scatter thresholds were left alone. Verify: the sun moves smoothly (shoot_sun / timelapse). With a real-time clock the VSM cache is now dropped every ~2.5 s, not every frame (`r.Shadow.Virtual.Cache.ShowStats`/`stat virtualshadowmapcache`).
- **SKY-3 done.** Same function: MieScatteringScale uses the relative 2e-3 test, the same as ULyudSunset's `Changed()`. MieAnisotropy keeps its 1e-3 test. Verify: the sky atmosphere proxy is no longer re-created every frame. The haze looks the same (a 0.2% step is invisible).
- **SKY-4 done.** `Public/Sky/LyudSunActivity.h` and `Private/Sky/LyudSunActivity.cpp`: the DebugLine member is gone. `GetDebugLine()` is now defined in the .cpp and formats from `*State` on demand (the same text). Update() formats it only when `lyud.Sun.Debug 1` is set. Verify: with `lyud.Sun.Debug 1` the orange SUNACT line shows; ALyudSky's DebugReport still includes it.
- **SKY-5 done (minor).** `Private/Sky/LyudRareSky.cpp`: `Now.AuroraPhase` is reassigned only when the phase name changes (StringCast is on the stack, and the comparison does not allocate).
- **SEA-1 skipped.** Per-tile output is not a function of the tile alone: the eelgrass density and the bNearEye gate depend on TileDist to the eye, and the output also depends on Present(season), LowWaterZ, radius and quality. An exact cache would need a rework of Rebuild. SEA-3's hysteresis plus hide-then-free removes the wave/pier rebuild storms. Rebuilds on tile crossings stay as before.
- **SEA-2 done (time budget, same values).** `Private/Sea/LyudSea.cpp` ALyudSea::StepDepthMap, `Public/Sea/LyudSea.h` (+`DepthCol`): in game worlds a slice stops after `lyud.Sea.DepthMapMs` (new cvar, default 4 ms; 0 = only the old 8000-cell cap). It checks the time every 32 cells and goes on from the next cell (DepthCol) the next frame. The same complex traces give the same cells, so the final map is identical. The editor path is unchanged (30000 cells, no time cap). The landscape TActorIterator now runs only before the first slice. The one-off TObjectIterator was kept. Note: the full map (1024x788, ~6-12 µs per trace) now takes ~1500-2500 frames instead of ~100, so about 25-40 s at 60 fps, during which the sea uses OpenSeaDepth and the seabed and fish wait. Raise the cvar if that matters (tests that settle at the sea quickly, e.g. test_underwater, may need `lyud.Sea.DepthMapMs 0`). Verify: the log line "LyudSea: depth map ... (last slice ≤ ~4 ms)", with no 50-100 ms frames after load.
- **SEA-3 done.**
  - `Private/Sea/LyudUnderwater.cpp` Tick (+header members AwayTime): near uses hysteresis. Enter at depth > 1 m and eye < level + 5 m; stay while depth > 0.5 m and eye < level + 7 m. When far, ALyudSeaLife is hidden (SetActorHiddenInGame) and the sim pauses; it is Released only after 30 s away. Coming back unhides it with no SpawnActor, mote mesh build or re-seed. lyud.SeaLife 0 or no sea still Releases at once (now also a hidden actor).
  - `Private/Sea/LyudSeabed.cpp` TickComponent (+bAwayHidden, AwayTime): bNear compares with the still `SeaLevel()`, not the wave crest. Enter at 8 m and depth > 0.5 m; stay up to 11 m and depth > 0.25 m. When far the ISMs are hidden, and Clear() runs only after 30 s.
  - Not done: building the mote mesh once per subsystem (its outer is the actor; less needed now that release is rare).
  - Verify: hover a bird or stand on a pier at the 5-8 m boundary. There are no hitches, and `lyud.SeaLife.Debug 1` stays "idle" without respawns.
- **SEA-4 done.** `Private/Sea/LyudUnderwater.cpp`: a cached `TWeakObjectPtr<ALyudSky>` (FindSky) replaces both per-frame TActorIterators. UpdateCollection runs only while under water or over water deeper than 0.2 m and less than 15 m above it (this covers where the seabed weed shows). Far away it writes one last time (Eye.x = 0) and then stops. `Private/Sea/LyudSeabed.cpp` DayNow takes an optional weak-sky cache, used by the per-frame UpdateJellies. Verify: in town, `stat game` / Insights show no UpdateCollection. At the shore the weed still leans and sways.
- **SEA-5 skipped** (not assigned).
- **SEA-6 done.** `Private/Sea/LyudSea.cpp` KeepSwimmerHeadUp: stale weak keys are removed with an iterator instead of `Remove(nullptr)`.
- **EUR-1 done.** `Private/Europa/LyudEuropa.cpp` ULyudEuropaSubsystem::Deinitialize: if the player is still on Europa, it restores `lyud.Rain.Fx` from SavedRainFx (and clears bOnEuropa). Verify: go to Europa, load a save or quit to the menu, then `lyud.Rain.Fx` in the console shows the old value.
- **EUR-2 skipped** (not trivial: it needs the prefetch set or async loading).
- **Minor:** the static FNames for ~40 SetVectorParameterValue calls, EnsureMaterial every frame (Tide changes every frame anyway; the setters early-out) and the CVarCloudPreset copy (an empty string, no allocation) were skipped as not worth the churn.
