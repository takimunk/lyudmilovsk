# Perf/leak/crash audit: weather thread (Private/Weather, Private/Winter, Private/Audio + Public headers)

Auditor: weather. Read-only pass over all files in the three areas (~11.7k lines). Every finding below was checked against its call path (who calls it, how often, whether the tick is live).
Paths are relative to unreal/Lyudmilovsk/Source/Lyudmilovsk/.

Ranked by likely real cost in play.

---

## W1 [S2, high] Near-snow tile recentres every 2 m with 2304 vertical multi-traces, a full-grid rescan and a full 1.44 MB trail upload
- Where: Private/Winter/SnowNear.cpp:408-411 (Drive -> Recentre when the 2 m-snapped camera moves), 174-265 (Recentre), 221-232 (trace loop), 262 (UpdateLowered full grid), 206 + 469 (bFullUpload -> `TArray<uint8>(Trail)` copy + RHI upload).
- What: `Drive` runs every frame while snow lies more than 1 cm deep (`ULyudWinterSubsystem::StepNear`, LyudWinter.cpp:1228). The centre snaps to a 200 cm grid, so the tile recentres every 2 m the camera moves: about every 1.3 s walking, several times a second running or riding. Each recentre does:
  - 48x48 = 2304 `LineTraceMultiByObjectType` calls, 160 m long, through WorldStatic. Each one also does `Hits.Sort` and 4-6 `IsA` checks per hit (SnowNearGroundAt, lines 53-84).
  - `UpdateLowered(0,0,47,47)`: 2304 vertices x 49x49 texel scan = ~5.5 M byte reads with modulo arithmetic.
  - `bFullUpload`: copies the 1200x1200 trail (1.44 MB) into a new TArray and uploads all of it.
  - A `TActorIterator<ALandscapeProxy>`.
- Why it matters: if a multi-trace costs ~5-20 us in town or forest, that is ~12-45 ms on the game thread, plus ~3-5 ms for the scan and the 1.44 MB copy and upload, every 2 m. It shows as a regular hitch while walking in snow.
- Fix (local):
  1. Slide `Ground`/`Lowered` like the rain roof map does. Keep the 48x48 window at 1 m in world-grid coordinates, shift it by the snapped delta, and trace only the new rows and columns (2 m of movement = 2 rows + 2 columns = ~192 traces, not 2304). Spread those over frames if needed.
  2. In the trail part of Recentre, upload only the cleared columns and rows as dirty regions, not `bFullUpload`.
  3. Call `UpdateLowered` only for the new vertex strips.
  4. Cache the landscape origin once.

## W2 [S2, high] Audio render thread: two trig calls per grain per sample (up to 240 grains + 220 pings)
- Where: Private/Audio/AmbienceDSP.cpp:567-611 (grain loop inside FAmbience::Render: `PanTo(V, G.Pan, 1.f, ...)` for every live grain on every sample). Private/Audio/AmbienceWeather.cpp:84-133 (RenderPings: `PanTo` per ping per sample). `PanTo` = clamp + `std::cos` + `std::sin` (AmbienceDSP.h:244-250).
- What: grains are leaf flutters and rain drops, capped at 240. Pings are hail, umbrella, window, drips and glaze, capped at 220. A grain's pan never changes after it is born, yet its gains are recomputed with two transcendental calls on every sample. The spawn rate is `Lv*(6+9W^2)`/s and the life is ~0.1 s, so the leaf grains alone reach ~200 live at a 15 m/s wind, and rain adds up to 900/s. Storms therefore hit the cap.
- Why it matters: 240 grains x 48 kHz = 11.5 M grain-samples/s at ~30-40 ns each, which is ~35-45 % of one core on the audio render thread (~8 ms of every 21 ms buffer of 1024 frames). Pings add the same again when an umbrella or hail is active. The risk is underruns or crackle, and battery and thermal load on the Mac, exactly when the scene is heaviest (storm).
- Fix: store `GainL = cos(A)`, `GainR = sin(A)` in `FGrain`/`FPing` at `AddGrain`/`AddPing` time and do `L += V*GainL; R += V*GainR` in the loops. That removes ~70 % of the per-grain cost, and the sound stays bit-identical apart from float rounding. The same one-time precompute applies to `FCricket` (fixed pan, AmbienceDSP.cpp:481) and to `FStrike` (fixed pan, line 531).
- Audio-thread safety otherwise: clean, see the section at the end.

## W3 [S2 hitch, med] Synchronous year-long weather integration on the game thread at start and after clock jumps (also in summer)
- Where: Private/Winter/LyudWinter.cpp:342-359 (`ReadWeather`: if `LastWeatherHours<0`, or the clock jumps back more than 1 h or forward more than 72 h, it loops `LyudWeather::Full(H, Chain, Season)` every 3 h from the last 1 September up to now, with a fresh `FSeason`). Private/Weather/WeatherModel.cpp:1137-1149 (`FSeason::AdvanceTo` integrates hour by hour from 1 Sep, and resets on any backwards step). Private/Weather/LyudWeather.cpp:461 (the weather subsystem's own season, first call).
- What: winter's history loop is ~2900 `Full` calls for a date in August, because the season started the previous September. Its fresh `FSeason` does ~8700 hourly `Step`s, each running `InstantWith`, which calls `Blended` 2-3 times plus `Profile` and `KpAt`. The weather subsystem does its own ~8700 steps on first use. This runs on the first winter Tick in every landscape world, even in summer when no snow can lie: the result only feeds `RoseHistory`, which gets zeroed anyway at snow depth 0.5 cm or less. It also runs again on any load or sleep that moves the clock more than 72 h.
- Why it matters: ~20 k `InstantWith` calls at a few us each is an estimated 50-150 ms single-frame hitch at world start and after time skips. tools/weather/run.sh perf can confirm the per-call cost outside the engine.
- Fix:
  - (a) Skip the winter history when the season has no snow. Check `W.SnowDepthCm` now, or stop at the first sample loop that sees no snow since the last thaw: iterate backwards from now in 3 h steps and stop at the last time depth was 0.5 cm or less. That also gives the same rose, because a thaw zeroes it.
  - (b) Or run it on a worker. It is pure C++ and `SharedChain()` is thread_local; use `Async` and swap in the rose when done.
  - (c) Pass the weather subsystem's already-advanced season in, not a fresh one.

## W4 [S2, med] While it snows, the whole footprint trail is faded, re-scanned and re-uploaded every 2 s
- Where: Private/Winter/SnowNear.cpp:445-458.
- What: if `Fall > 0.05 cm/h`, every 2 s it loops all 1,440,000 trail bytes, sets `bFullUpload` (another 1.44 MB TArray copy plus a render upload at line 469), and calls `UpdateLowered(0,0,47,47)` (~5.5 M reads).
- Why it matters: ~3-6 ms on the game thread every 2 s, plus 1.44 MB of upload bandwidth, during every snowfall.
- Fix: store a fill offset instead of rewriting the texels. For example, keep a global "fill" level in the MPC (NearTrail.z) that the material subtracts, and only rebase the bytes rarely. Or fade only the dirty bounding box of prints made in the last N seconds (keep a list of print rects). Keep the per-vertex max from `UpdateLowered`, but update it only where prints exist (track a bounding rect of non-zero trail).

## W5 [S2, med] Rain roof map traces 64+ cells every frame in all weather; full array copies on every 1.5 m cell crossing
- Where: Private/Weather/LyudRainFog.cpp:644 (`Tick` -> `UpdateRoofMap` unconditionally), 472-476 (slow refresh: `TracesPerFrame/4 = 64` traces every frame), 425-438 (each trace is 180 m: Camera.Z+60 m down to -120 m, with up to 4 retries past pawns), 383-384 (`TArray<FVector2f> Old = Roof; TArray<uint8> OldDirty = Dirty;`, 36 KB of allocation every 150 cm the camera crosses), 496-509 (a 32 KB malloc + texture upload on any changed cell).
- What: the open-sky map is also read by the soundscape and the fog weight, so some refresh is needed, but at 64 x 180 m traces every frame in clear weather. It is never throttled by rain or wetness. A fresh map (spawn, teleport, more than half the cells dirty) traces all 4096 cells x up to 4 tries in one frame.
- Why it matters: ~3800 long traces/s, which is ~0.2-0.6 ms per frame in town. Teleports or loads also get a 4096-trace one-frame hitch (~20-60 ms).
- Fix:
  - Make the slow refresh 8-16 per frame when `Smoothed[0] < 0.02 && Smoothed[2] < 0.02` (dry).
  - Shorten `Bottom` to Camera.Z-3000 unless flying.
  - Shift in place, or keep two persistent scratch arrays as members, instead of copying.
  - Cap the fresh-map budget to ~1024 traces per frame. Rain stays hidden until `bRoofUploaded` anyway.

## W6 [S2, med; magnitude to measure] The whole snow field is re-composed and re-uploaded every time the snow level moves 0.3 cm
- Where: Private/Winter/LyudWinter.cpp:934-948 (`StepSnow`: `|LevelCm - ComposedLevel| > 0.3 && >1 s` -> `ComposeAll` + `UploadDepth(0,0,W-1,H-1)`). Also lines 917-925 (a `PathHours` jump of more than 3 h makes it full again). `StepSnow` runs every Tick once the phase is Ready.
- What: the field is 1 m cells over the whole landscape box. A ~3x3 km map would be ~9 M cells, which means 9 M `DriftedM` evaluations, a 9 M float16 conversion and an 18 MB RHI upload. During snowfall or melt at game time scales this repeats every few real seconds to minutes.
- Why it matters: it is likely a 10-50 ms hitch plus a large upload each time. The existing Verbose log "snow composed ... in X ms" gives the exact number; turn on LogTemp Verbose in a snowfall preset.
- Fix: the level is already an MPC param (`Snow.x`). Compose the field as level-relative (depth / LevelCm, or factor x level in the shader) so a level change is only an MPC write, and keep the full compose for factor or path changes. Alternatively, compose and upload in row bands over several frames.

## W7 [S3, med] KeepOffDecals walks every actor in the world every 2 s, all year once winter has been built
- Where: Private/Winter/LyudWinter.cpp:484-489, 1308-1341. The phase never returns to Idle (it is only set in the struct's initialiser), so after the first winter session this runs in spring and summer too.
- What: it runs `TObjectIterator<USkeletalMeshComponent>` (all of them, every world) and `TActorIterator<AActor>` over all actors with 4 `IsA` checks each, plus `ForEachComponent` on every plant actor, every 2 s.
- Why it matters: thousands of town, plant and NPC actors give an estimated ~0.5-2 ms spike every 2 s.
- Fix: do it once at Ready, then on `World->AddOnActorSpawnedHandler` (unbind in Deinitialize) for new actors. For skeletal meshes, set it where people, animals and birds create their meshes, or only check actors spawned since the last sweep. Also skip the sweep while `LevelCm <= 0.2` (the decal is hidden then).

## W8 [S3, high] LoadObject every frame in near-snow Drive
- Where: Private/Winter/SnowNear.cpp:485 (`LoadObject<UMaterialParameterCollection>(nullptr, SnowNearCollectionPath)` every frame while snow lies), plus 3 FNames built from TEXT per frame.
- Why it matters: it parses the path and finds the package and object every frame, a few us. If the asset is ever missing, it is a failed load per frame (an expensive package lookup).
- Fix: cache the MPC in a `UPROPERTY(Transient) TObjectPtr<UMaterialParameterCollection>` in `BuildMesh`, or reuse `ULyudWinterSubsystem::Collection`, which is the same asset.

## W9 [S3, med] Sea-ice collision tiles: all 441 boxes are re-laid every 1.5 m moved or 2 cm of sea level
- Where: Private/Winter/LyudWinter.cpp:1634-1686 (`ALyudSeaIce::Drive`, called every Tick while ice exists).
- What: `bMoved` triggers at half a tile (1.5 m) or a 2 cm change in surface Z (tide, low-water blend). Then every tile in the 21x21 grid gets `SetBoxExtent` + `SetWorldLocation` (a physics body teleport) + `SetCollisionEnabled` and 5 `Walkable()` calls (each 1-2 `Fbm`). The grid snaps to 3 m, so half of the re-lays change nothing.
- Why it matters: up to 441 physics teleports + 2205 `IceAt` calls per re-lay, a few times a second when moving on or near ice.
- Fix: re-lay only when the snapped `CX`/`CY` changes or Z moves more than 2 cm. Recycle tiles toroidally (only the incoming row or column moves). Skip `SetBoxExtent`/`SetWorldLocation` when the value is unchanged.

## W10 [S3, med] Soundscape loads every sound and all music synchronously at OnWorldBeginPlay, with ~150 failing loads
- Where: Private/Audio/LyudSoundscape.cpp:253-321.
- What: each of the 55 pools probes `S_<id>_01..12` with `LoadObject` until the first miss, and each mood probes `MU_<mood>_1..9`. That is ~146 failed `LoadObject` calls (each a package-existence check on disk), plus a synchronous load of every music cue (full compressed audio unless it streams) at world start.
- Fix: enumerate `/Game/Audio` once through `IAssetRegistry::GetAssetsByPath` (no failed loads). Keep music as `FSoftObjectPath` and `LoadAsync` the chosen cue a few seconds before it plays.

## W11 [S3, med] SnowNear Walkers map never pruned
- Where: Private/Winter/SnowNear.cpp:353 (`Walkers.FindOrAdd(Who)`). It is a `TMap<TWeakObjectPtr<AActor>, FWalker>` that only ever grows.
- What: it gets an entry for every NPC body or animal that ever walked in snow within 15 m, and stale weak keys of destroyed actors stay. The growth is slow (bounded by the actors ever spawned) but unbounded over a long session if bodies or animals respawn.
- Fix: in `Recentre` or every N seconds, `Walkers` remove-if `!Key.IsValid()` or the actor is more than 2xHalfCm away.

## W12 [S3, low-med] Moving runtime meshes without the Lumen / distance-field opt-outs
- Where: Private/Winter/SnowFx.cpp:21-33 (Flakes: `SetActorLocation(Camera)` every frame). Private/Winter/SnowNear.cpp:87-99 (Surface: moved every 2 m, opaque WPO). Private/Winter/LyudWinter.cpp:1607-1632 (the 60 km ice Sheet, moved per 1 km).
- What: none of them set `bAffectDynamicIndirectLighting = false` / `bAffectDistanceFieldLighting = false`; RainFx does (LyudRainFog.cpp:242-243). The Flakes are translucent, so Lumen probably ignores them, but SnowNear's opaque surface may be captured into Lumen cards and recaptured when it moves.
- Fix: add the two flags in all three constructors (and `bVisibleInRayTracing = false` for flakes).

## W13 [S3, high] RainFx per-frame churn while dry
- Where: Private/Weather/LyudRainFog.cpp:641-643 (`SetActorLocation(Camera)` and then `WetDecal->SetWorldLocation` with the same point: the decal is attached with a zero relative offset, so the second call is redundant), and 552-585 (~12 MID `Set*ParameterValue` calls with FNames from TEXT every frame even when drops and decal are hidden; `Light` changes continuously, so one MI render update per frame per MID).
- Fix: drop line 643. When `!bDrops && !WetDecal->IsVisible()`, skip the MID updates (keep the MPC Rain at 0 once). Use static FNames.

## W14 [S3, med] The ambience synth renders the full DSP while muted
- Where: Private/Audio/LyudAmbienceSynth.cpp:56-79 and LyudSoundscape.cpp:541 (`P.Master = 0` for `lyud.Sound 0`, vacuum on Europa, or volume 0).
- What: `FAmbience::Render` still runs every voice, including W2's grains, when `Master == 0`. The A/B cvar `lyud.Sound 0` therefore does not remove the audio-thread cost, which also skews benchmarks.
- Fix: if `P.Master <= 0` for more than ~1 s (after the smoothing tails), memset the output and return. Or `Bed->SetActive(false)` / `Stop()` from the game thread when muted, and `Start()` again when unmuted.

## W15 [S3, low] GetStateAt / Forecast integrate a whole season per call
- Where: Private/Weather/LyudWeather.cpp:487-494 (`GetStateAt` is BlueprintCallable and makes a fresh `FSeason` each call, which means ~1-8700 hourly steps), 675-691 (`Forecast`, console only).
- What: nothing in C++ calls `GetStateAt` today. If UI, Python or BP ever calls it per frame or per day row, each call is a W3-sized hitch.
- Fix: keep a mutable cached `FSeason` for queries, reused when the date is ahead of it, or document it as expensive.

## W16 [S3, low; logic] Club typing loops are made for every placed human skeletal mesh actor in the world, frozen at their spawn spot
- Where: Private/Audio/LyudSoundscape.cpp:958-990.
- What: on the first `UpdateClub`, every `ASkeletalMeshActor` whose mesh path contains `/Characters/Humans/` gets a keyboard, mouse or console loop at its spawn location. The loop never follows the actor and is never rebuilt. Any placed person outside the club (or one PeopleLife moves or hides) leaves keyboard sounds in thin air within 25 m.
- Fix: restrict to the club's seats (PeopleLife's club_seat slots, or actors within the club volume), or attach the component to the actor.

---

## Minor notes (no action needed unless convenient)
- Private/Audio/LyudSoundscape.cpp:715-732: `CVarForce` string is copied and parsed at 10 Hz. Cheap; could early-out on an empty string.
- `FindSky` (TActorIterator) runs ~15x/s from `UpdateBed`, `SunAltitude` and `ComputeMood`. Cheap; could cache a `TWeakObjectPtr`.
- Private/Winter/LyudWinter.cpp:106: `GWinterBank` is a static `TSet` of raw component pointers that outlives the world. It is only compared, never dereferenced, and is reset per build, so it is safe. Clearing it in `Deinitialize` would be tidier.
- `ULyudSoundscapeSubsystem::Deinitialize` doesn't remove its `AddDynamic`s on UAnimalLifeSubsystem and ULyudWeatherSubsystem. Both are same-world subsystems torn down together, so this is harmless.
- `UpdateCollection` (winter, every frame): ~11 MPC vector sets with `FName(TEXT)` construction each. The MPC instance compares values, so the cost is only FName hashing.

## Audio-thread safety (Private/Audio)
Checked: the `OnGenerateAudio` of `ULyudAmbienceSynth` and `ULyudWindAudioComponent`, and every DSP path they reach.
- No UObject or game-thread state is touched from the render thread. Parameters cross via a POD copy under `FCriticalSection` (`SetParams` at 10 Hz on the game thread, a ~300-byte copy per buffer on the audio thread). The lock is short and uncontended in practice.
- `Strike`/`Footstep` go through `SynthCommand` and run on the audio thread before rendering. The wind component uses `std::atomic<float>`. One benign race: `ULyudWindAudioComponent::Gain` (a UPROPERTY float) is read unsynchronised on the render thread, which is harmless on arm64/x64.
- No allocations in steady state:
  - `Tmp`, `Left` and `Right` grow once (`EAllowShrinking::No`).
  - `Grains` and `Pings` are reserved at 256, with caps of 240 and 220 checked before `push_back`, so they never reallocate.
  - `Breaks`, `Strikes`, `Crick`, `Chirps`, `StepPool`, `Eaves` and `Echo` are sized in `Init`.
- No logging, locks or file IO inside the DSP.
- The only real problem is CPU (W2, W14).

## Fixes applied

Paths relative to unreal/Lyudmilovsk/Source/Lyudmilovsk/. Not built (lead builds).

- W1 partial: Private/Winter/SnowNear.cpp `Recentre` caches the landscape origin (members `LandOrigin`/`bHaveLandOrigin` in Public/Winter/LyudWinter.h), so no TActorIterator per recentre. The sliding Ground/Lowered window and per-strip trail upload were skipped: the ground fill (`Last - 50` fallback in scan order, 3x3 "even surface" pass) would not stay identical when only new strips are traced, and the change is well over ~80 lines. Verify: snow walks look the same; `stat game`/Insights shows no ALandscapeProxy iteration in Recentre.
- W2 done: Private/Audio/AmbienceDSP.h new `PanGains` (the same law; `PanTo` now calls it), `GainL/GainR` in FGrain, FCricket, FStrike; Private/Audio/WeatherVoices.h `GainL/GainR` in FPing. Gains set once in `FAmbience::AddGrain`, `FAmbience::Init` (crickets), `FAmbience::Strike`, `FWeatherVoices::AddPing`; `Render` grain loop, `RenderCrickets`, `RenderThunder`, `RenderPings` use them. Output is bit-identical (same cos/sin of the same angle, same multiply). Verify: tools/audiosim renders match the old WAVs; audio thread time in a storm preset drops.
- W3 safe part done: Private/Winter/LyudWinter.cpp `ReadWeather` skips the season replay when `W.SnowDepthCm <= 0.5` now (summer, after a thaw). The rose is zeroed as before, and the next `Drift` of that same state would zero it anyway. When snow lies, the replay still runs unchanged. Not done: worker thread or a shared season (bigger change). Verify: a summer start has no ~100 ms hitch on the first winter Tick (Insights, ULyudWinterSubsystem::Tick).
- W4 skipped: needs a material-side fill offset (M_SnowNear/M_Ground, MPC) or a print-rect tracker. That is an asset change plus an algorithm change, so the visual result is not provably identical.
- W5 cheap parts done: Private/Weather/LyudRainFog.cpp `UpdateRoofMap` swaps into the kept `RoofScratch`/`DirtyScratch` members (Public/Weather/LyudRainFog.h) instead of copying 36 KB per cell crossing. A fresh map (spawn, teleport, or more than half the cells dirty) now fills at 1024 cells a frame (`bFilling`), so it takes ~4 frames. Rain drops and the wet decal stay hidden until it is whole (`bRoofUploaded = !bFilling`), so rain never shows through untraced roofs. Not done: dry-weather throttle and the shorter trace (fog weight and sound read the map). Verify: `lyud.Rain.Debug 1` "roof dirty" goes 4096 -> 0 over a few frames after a teleport, with no single-frame hitch.
- W6 skipped: needs level-relative compose in M_Ground and the decal material (an asset change), or a banded compose over frames (risky with UploadDepth and the path logic).
- W7 done (safe part): Private/Winter/LyudWinter.cpp `Tick` runs `KeepOffDecals` only while `S.In.LevelCm > 0.2` (the snow decal is shown). The timer keeps running down, so the first sweep comes on the frame snow returns. People, animals and bodies already switch decals off at creation. Not done: spawn-handler-only sweeps, because plants and other actors add components after spawn and could be missed. Verify: in a spring session after winter, no KeepOffDecals spikes every 2 s in Insights.
- W8 done: Private/Winter/SnowNear.cpp `BuildMesh` loads the MPC once into `NearCollection` (UPROPERTY Transient, Public/Winter/LyudWinter.h). `Drive` uses it with static FNames. Verify: NearRect/NearGround/NearTrail still drive M_SnowNear (prints show); no LoadObject in Drive.
- W9 done (small version): Private/Winter/LyudWinter.cpp `ALyudSeaIce::Drive` re-lays only when the snapped tile centre (`LaidCX/LaidCY`) changes, the level moves more than 2 cm, or the half-thickness moves more than 1 cm. Before, it re-laid every 1.5 m walked, often to identical boxes. `SetBoxExtent` (render-state dirty plus body rebuild) is skipped when the extent is unchanged. Toroidal recycling was not done. Verify: walking on sea ice still holds (test_winter ice checks); fewer physics teleports.
- W11 done: Private/Winter/SnowNear.cpp `Recentre` prunes `Walkers` entries whose actor is gone or more than 2 x HalfCm (30 m) away. They start a fresh track on return, as after a jump.
- W12 done for Flakes and Surface: Private/Winter/SnowFx.cpp constructor (Flakes: `bAffectDynamicIndirectLighting`, `bAffectDistanceFieldLighting`, `bVisibleInRayTracing` = false); Private/Winter/SnowNear.cpp constructor (Surface: the first two). Skipped for the ice Sheet: it moves only once per 1 km, and it is the visible sea ice that Lumen reflections and GI may use, so taking it out could change the look.
- W13 done: Private/Weather/LyudRainFog.cpp `Tick` drops the redundant `WetDecal->SetWorldLocation` (attached at zero offset to the root, which SetActorLocation moves). `UpdateMaterials` sets the Drop/Splash/Wet MID parameters only while that part is visible (they are set in the same call the frame it shows) and uses static FNames, including the MPC "Rain", which is still written every frame. Verify: rain, splashes and wet ground look the same in test_rainfog; with dry weather no MID updates.
- W14 done: Private/Audio/LyudAmbienceSynth.cpp `OnGenerateAudio` (+ `Rate`, `MutedFrames`, `bAsleep` in LyudAmbienceSynth.h). After 1 s with `P.Master <= 0`, output was already exactly 0 because Master multiplies all of it. Now it skips `FAmbience::Render` entirely and zeroes the reported RMS. On unmute it builds a fresh `FAmbience` (Init with the same rate and seed, as at world start), so no stale grains, strikes or frozen filter state play out. Footsteps and strikes sent while muted are dropped (they were silent anyway). Verify: `lyud.Sound 0` and the audio render thread time falls to ~0 for the bed; `lyud.Sound 1` brings the bed back fading in from start values; `lyud.Sound.Debug` rms = 0 while muted.
- W16 skipped: not trivial. Restricting the loops to club seats needs PeopleLife's seat data or a club volume (another area), and attaching them changes when the loops play.
