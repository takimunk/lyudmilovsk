# Review C: Transport, Railway, Bikes, Doors, People, Animals, Food, Smoking, UI, top-level files

Scope: `git diff` of unreal/Lyudmilovsk/Source/Lyudmilovsk outside Birds/Plants/Terrain/World/Sky/Sea/Europa/Weather/Winter/Audio
and BirdTame/BirdFeedItem. Intent from audit/transport.md, people.md, ui.md ("Fixes applied"). Read only, nothing built.

No BLOCKERs found. API checks against UE 5.8 headers passed: `UWidgetComponent::SetRedrawTime/SetTickMode/ETickMode`,
`FCSPose::SafeSetCSBoneTransforms(TConstArrayView)`, `ISM::BatchUpdateInstancesTransforms(int32, TArrayView<const FTransform>, ...)`,
`FAnimInstanceProxy::AddReferencedObjects(UAnimInstance*, FReferenceCollector&)`, `FBoneContainer::GetSerialNumber()`
(uint16, never 0 after init), `SSlider::OnMouseCaptureEnd`, `IConsoleVariable::OnChangedDelegate`.

## SHOULD

1. **Private/Transport/LyudLobby.cpp:354 `MoversOutOfLumen` runs at the top of `LobbyCentre`, before the centre's own movers
   exist.** The chandelier body (Spin), the pendulum pivot with its rod and bob, and the beacon lens are added to LobbyAnims
   further down in the same function. A station's centre movers are only caught by the next station's `LobbyCentre` call, so
   the last station built keeps its centre piece in Lumen and the distance fields. The comment says "the centre's own movers
   below, at the end", but there is no second call. Fix: call it at the end of `LobbyCentre` (the early `return` is fine:
   no centre means no new movers), or once after every station is furnished.

2. **Private/LyudGoodsItem.cpp `SetShelfStock`: `bAffectDistanceFieldLighting = false` on components that are already
   registered has no effect.** Nothing re-creates their render state. In UE 5.8, `SetCullDistance` only goes through
   `SetCachedMaxDrawDistance` -> `Scene->UpdatePrimitiveDrawDistance`. It does not re-create the render state, so the comment
   is wrong. Goods and bottles already set the flag before registering (`Part`, the `ABottleItem` ctor), so only
   `ABirdFeedBag` shelf stock is affected: its DF flag stays on. The cull distance itself works. Fix: when the flag changes,
   `if (P->bAffectDistanceFieldLighting) { P->bAffectDistanceFieldLighting = false; P->MarkRenderStateDirty(); }`.

## NIT

3. **Private/Railway/LyudTrainWake.cpp `Push`: the pool's "bounds stay put" doesn't hold over time.** Slots in
   [Span, Have) keep the zero-scale transform they got at `HideAt` on the frame they went unused. That spot is the front
   wheels at that moment, and the slots are not moved again until reused. As the train runs, the ISM bounds stretch from
   those old points to the current wake (they can span the line). The effect is a little culling loss, nothing visible.
   Fix: when Span < Have, re-park [Span, Have) every few seconds, or when Wheels[0] has moved more than ~50 m since the last
   park.

4. **Private/Transport/LyudTransport.cpp:670-690 cabin widget culling is a visible change.** The hull plates now disappear
   beyond 150 m. The inside board and posters disappear beyond 30 m, even through the windows from a platform. Check that the
   plates (a line-colour panel) don't pop visibly on a cabin coming in. If they do, use a larger distance or fade them.

5. **Private/Transport/LyudTransport.cpp `UpdatePlayer` prompt key (T9).**
   - A language switch while the same prompt is up keeps the old-language text until the key changes. Add the language to
     the key, or clear `PromptShown` on change.
   - When another owner clears the shared prompt line (`SetPrompt(other, empty)` removes the widget), the transport prompt
     is missing for up to 0.25 s. Before, it came back the next frame.

6. **Private/People/CounterTrade.cpp `IsOpen` cache keyed on `People->GetSim()` pointer.**
   - `Stop()` does `Sim.Reset()` and `Start` does `Sim = MoveTemp(W)`, so a new sim can sit at the old address. The cache
     is then kept even if people or activities changed between runs (an edited json, lyud.People 0/1).
   - Same json gives the same indices, so it is harmless today.
   - Fix: a run counter bumped in `Start`, or reset the cache from `UPeopleLifeSubsystem::Start`.

7. **Private/Bikes/LyudBikeSubsystem.cpp:583 and Private/Bikes/HeroBike.cpp `GroundAt`: function-static raw `UObject*` /
   `UClass*` caches.**
   - The `LastWalk` static is shared by every NPC body, so it thrashes between their walk clips. A clip GC'd and a new one
     allocated at the same address would also give the wrong `bShared`.
   - `DriveBike` is an `ANpcBody` member, so it can use the new per-body `SharedWalk` (PeopleLife.h). Same address-reuse
     note for `LastClass` with Live Coding reinstancing.

8. **Private/People/LyudBodyAnim.cpp:1066 / Private/BoothOwner.cpp `OnlyTickPoseWhenRendered`.**
   - Correct in game, and the lip-sync world-clock fix looks right.
   - Any headless check run without rendering (`-nullrhi`, or a capture that doesn't render the club) will now see frozen
     poses, blinks and mouth curves.
   - Confirm test_faces / lyud.Face.Speak and lyud.Face.Log still run with `-RenderOffscreen` (rendered), not nullrhi.

9. **Private/Transport/LyudStationBuild.cpp:216, LyudRailTrack.cpp, LyudLevelCrossing.cpp, cabin posters/plates
   `SetRedrawTime(5)`.** After a language switch, signs keep the old text for up to 5 s. Acceptable, or call
   `RequestRenderUpdate()` on them on language change.

10. **Private/LyudGoodsItem.cpp `SharedMaterial`: rooted MIDs in the transient package.** In the editor they outlive PIE.
   They keep their base materials and label/icon textures rooted for the whole editor session, and keep old parents after a
   material reimport. Fine for -game. No caller mutates a shared MID: `LiquidMaterial` is only read, and `SetLevel` changes
   scale only.

## Checked, no issue

- T6 rider slot reuse (CablewaySim + HandleEvents): released only when Done. All events for the trip are handled before
  release. The player's PlayerAlight Alight goes out next frame. `FindRider` skips Done. Cabins' `Riders` drop the index
  before Done. Cancelled is never released.
- T2 ropes: the end point is reused only when the next segment wasn't skipped and has the same bNear.
- T3/T12 doors: the sphere bounds are conservative (scaled half-diagonal; the sweep box grown asymmetrically by
  -120/+20 z is covered by the +120 extent). The `!bNear && Holding.IsEmpty()` skip matches the old release path.
- T7 train: hidden-train readers (`GetSeat` in PeopleLife) already check `bVisible`. The first shown frame teleports with
  Old = new.
- T11 lift leaves: same as KitMesh (owner, Movable, materials, BlockAll, Root).
- P4 desk/feet caches, P5 GC refs, P7 bone caches (per proxy, serial nonzero), P8 gamepad via `PcClips`
  (PcClip only set where PcClips is filled), P9/P13 (no other People-indexed state besides Target/Talking), P11, P14 range
  write, UI-03, UI-04, UI-07, UI-08, UI-10 (`Language()` callers all copy/compare), UI-11, UI-12 (component count changes
  on NewObject/Destroy), UI-13, UI-14 early-out (Dispensing/Taking are not skipped).
