# Review A: birds + plants/terrain/world diff

This covers `git diff` for Private|Public/Birds, BirdTame, BirdFeedItem, Plants, Terrain and World, checked against audit/birds.md and audit/plants.md "Fixes applied". I read it only: nothing was built or run.
Paths are relative to unreal/Lyudmilovsk/Source/Lyudmilovsk.

No BLOCKERs found. The engine APIs used all exist on UE 5.8 with the signatures used:
- `BatchUpdateInstancesTransforms(int32, const TArray<FTransform>&, bool, bool, bool)`
- `UpdateInstanceTransform`
- `AddInstances(T, bool)`
- `FTickFunction::UpdateTickIntervalAndCoolDown`
- the public `TickInterval`
- `UStaticMesh::AreRenderingResourcesInitialized`
- `FCStringAnsi::Strcmp` (`FBreedKind::Id` is `const char*`)
- `ABirdActor::GetState()`

Dropping `MarkRenderStateDirty` after the ISM transform updates is safe in 5.8. `UpdateInstanceTransform` and `BatchUpdateInstancesTransforms` call `PrimitiveInstanceDataManager.TransformChanged`, which leads to `MarkChangeHelper` and then `MarkRenderInstancesDirty`. `SendRenderInstanceData_Concurrent` then flushes the change and calls `UpdateBounds`, so the update reaches the renderer and the bounds follow.

## SHOULD

1. **PL-02 relayout step uses up the whole margin.** `Private/Plants/ForestFloor.cpp:1732`
   - Kinds are only checked at relayouts, and relayouts come every `Relayout` = 6 m (`Margin` = 12 m, ForestFloor.h:73-74).
   - So a kind with Step = Clamp(0.2*Cull, 6, 9.6) = 9.6 m is dirtied at the 12 m relayout, not at 9.6 m. At that point the camera is already a full `Margin` from `CommittedAt`.
   - The commit then still waits for the pending cells (`bPending`) and for the 2-kinds-a-frame queue, which is ~35 kinds or ~18 frames.
   - During that wait every extra metre of travel leaves the front edge of that kind's cull ring with nothing laid (Reach = Cull + Margin around `CommittedAt`, `:1302`).
   - The result is pop-in at the far edge for logs, stumps, boulders and other wide-ring kinds while running or flying as a bird. The note says "no pop-in at the cull edges", but that does not hold.
   - Before the change the buffer was ~6 m plus travel; now it is ~0 m plus travel.
   - Fix: account for the relayout quantisation, for example dirty when `Dist2D(Centre, CommittedAt) + Relayout > 0.8f*Margin`. With these numbers that dirties every relayout, so the only real win is to give wide kinds their own larger margin, e.g. lay cells to `Cull + Max(Margin, 0.3*Cull)`, and clamp Step to `KindMargin - Relayout - a few m`.

## NIT

2. **PL-06: `TriedAt` is set even when the load succeeds.** `Private/Terrain/LyudTerrain.cpp:1193`
   - It is harmless today, because a loaded mask is rooted in game and `RF_Standalone` keeps it in the editor.
   - If either of those stops holding, a dropped mask would come back as nullptr for up to 60 s.
   - Fix: `TriedAt = Loaded ? -1.0 : Now;`
   - The `AddToRoot` with no `RemoveFromRoot` is fine: it is one global asset, not one per world.

3. **PL-07: lamps arrive late.** `Private/World/AcademToggle.cpp:1291`
   - Lamps or lights that appear after the cache is built (streamed or spawned) are only picked up by the next rebuild, up to 60 s later. Before, they were picked up at the next 0.5 s tick.
   - `bLit` is cached, which is fine because the `AcademLit` tag is never changed at runtime.
   - The rebuild is still one full actor walk a minute (every 5 s on sky-less maps). That is acceptable.

4. **BIRDS-15 TellBirds: murmurations spawned while the palm is out are missed.** `Private/BirdTame.cpp:365-381`
   - A murmuration that appears while the palm is already out is not in `Murmurations` until the palm goes down and up again.
   - It never got `SetFeeding` before either. But it used to get `SetFeedHand` every frame, and now it does not.
   - This is unlikely to matter.

5. **BIRDS-06 PredatorThink: the random sequence changes.** `Private/Birds/BirdSim.cpp:2058-2080`
   - `Rand(H.Rng)` is now drawn once per candidate prey instead of once. This is intended, but it changes the hawk's RNG stream.
   - Any birdsim test with fixed-seed expected hunt outcomes or logs may need its expected values updated.

6. **BIRDS-11: indoor bird bodies now get the sun's channel.** `Private/Birds/BirdActor.cpp:32`
   - `ABirdActor` bodies are now on channel 1 everywhere, so a body indoors gets the sun and moon channel.
   - Indoor walls on channel 0 only do not cast that light's shadow.
   - This matches the instanced birds, which already had channel 1, so it removes the instance/body brightness jump. The catch is that a bird body inside the hall or a flat may now look sunlit.
   - The BirdTame attach-time channel copy is fine: `ALyudSky` re-applies channels to the pawn's attached actors when the player goes in or out (`LyudSky.cpp:779-789`).

7. **PL-01: a field build can restart repeatedly.** `Private/Plants/ForestFloor.cpp:858-876`
   - A camera going back and forth across a 24 m snap line in under ~8 frames restarts the build at row 0 each time, so the field may lag.
   - `LitterFrom` also reads `TreeGrid` across those frames. If `BuildTreeIndex` runs mid-build, the rows can mix the old and new tree index for one field.
   - Both cases are rare.

8. **PL-08: `Held` now also holds assets that someone else loaded.** `Private/World/LyudPrefetch.cpp:1412-1419`
   - Assets loaded by others are added to `Held` once their folder is ready, which pins them until the world ends.
   - That is needed to keep `bReady` true, but it raises the PL-17 memory ceiling a little.
   - `AddUnique` over a large `Held` is O(n), but it runs only once per folder.

9. **PL-04: padding instances.** `Private/Plants/ProceduralPlant.cpp:1036-1043`
   - The padding instances sit at the component origin at scale 1e-4. They are drawn and cast shadows, which is a negligible cost.
   - The landed and air index ranges shift whenever a leaf lands or lets go. Because no previous transforms are supplied this gives no motion-vector smear, but every frame still sends all instances' transforms (`TransformChanged` per instance).
   - That is still far cheaper than the old proxy recreate.

## Checked and OK

- **BIRDS-01:** `EndPlay` is declared in the header. `Bodies` is the UPROPERTY TMap, and `IsValid(TObjectPtr)` is fine.
- **BIRDS-02, the Sync tail invariant:** instances at or past `max(Want, Prev)` are hidden, since they were either added hidden or hidden at the last sync. `Arr.Num()` is ≤ the instance count, and `D.Shown` starts at 0 on fresh ISMs.
- **BIRDS-04:** the members keep their capacity between frames. `Keep` is a TArray bounded by `lyud.Birds.Bodies` (12). The PopLog output is identical.
- **BIRDS-05:** fine.
- **BIRDS-07:** `LikeTable` runs on the game thread only (BirdLife.cpp:547, 1664). `Unknown` is 0 or 1, and the TPair key hash exists.
- **BIRDS-09:** `SetLeft`'s shown counters are consistent with `Build`, and the Spill fall works.
- **BIRDS-10:** `HeapMat` is a UPROPERTY, and the heap is never recreated.
- **BIRDS-12:** `SightUntil` is compared with `FWorld::Time`, a monotonic double.
- **BIRDS-15 (other parts):** fine.
- **BIRDS-16:** `HandleApproach` does not need the body.
- **BIRDS-17:** fine.
- **BirdNests caches:** the key fits (species*8 + age*2 + beg < 8 per species).
- **PL-02 ByKind:** the commit order matches the old one, and no other `Cell.Items` users remain.
- **PL-03:** `Ps` and the want class depend only on the kind and `Seed`, so they are the same values as before.
- **PL-05:** the early-out uses the same distance test as the final check.
- **PL-09:** materials are set on the component, so a fresh mesh keeps them. Both callers assign the returned mesh.
- **PL-12:**
  - The tick interval is restored to every frame by `ON_SCOPE_EXIT`, and the 1 ms slack works with the interval tick's accumulated delta.
  - Falling leaves, drops, fruit and the prompt all keep per-frame ticking.
  - E-pick goes through input, not tick.
  - There is no win in the leaf season, because `PendingDrops` stays non-empty.
- **PL-13:** fine.
- **PL-15:** the weak set and the stale-entry pruning are fine.
