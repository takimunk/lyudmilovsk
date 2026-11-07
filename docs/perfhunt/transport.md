# Perf/leak/crash audit: transport

Areas: Private/Transport, Private/Railway, Private/Bikes, Private/Doors (+ Public headers), ConveyorBelt.cpp, SwingDoor.cpp,
CurtainPusher.cpp, LightFlicker.cpp. Read only. Paths are relative to unreal/Lyudmilovsk/Source/Lyudmilovsk.

Data used for the estimates: Content/Transport/network_built.json has 12 cabins and 2 lines whose rope loops have 569 + 789 points
(28 + 40 spans). Transport kit.json has 2 posters, 2 plates and 1 board per cabin. railway_built.json has trains = 1. About 300+
doors (Academgorodok alone emits ~300). NPC bodies register as door agents (PeopleLife.cpp:778).

Checked and clean (nothing to report): the cableway rope ISM (Lumen flags set, bMarkRenderStateDirty=false, fix 68e1e243 is in);
the wake ISMs (Lumen flags set, BatchUpdate with dirty=false); the bikes (Lumen flags set, sounds made once, parts posed only
when used); ALyudDoorBase (ticks only while moving or waiting to close, EndPlay unregisters); the RailTrack tick (0.5 s interval,
writes only when the lamp level changes); LevelCrossing (no tick of its own, driven by the subsystem); LightFlicker (SetIntensity
skips equal values); SwingDoor; and the UPROPERTY coverage in every header in these areas (FCar::Root / FCarMats are also held by
CarRoots / KeepMats). RideHook captures a weak pointer and is cleared in Deinitialize.

---

## T1. S2: world widget components on cabins (and every board/sign) redraw their render target every frame
- Where: Private/Transport/LyudCableway.cpp:616-631 (2 posters per cabin, 1600x900, SLyudTransitDiagram), :636-644 (cabin Board,
  600x150), :645-680 (2 plates, 560x160); the same pattern for station boards in Private/Transport/LyudStationBuild.cpp:201-220
  (`Widget()`), the club board LyudCableway.cpp:424-436, rail signs Private/Railway/LyudRailTrack.cpp:465 and the crossing sign
  Private/Railway/LyudLevelCrossing.cpp:151.
- What happens: no call anywhere in the module sets `SetRedrawTime`, `SetManuallyRedraw`/`bManuallyRedraw` or `TickWhenOffscreen`. So
  UWidgetComponent::ShouldDrawWidget draws the Slate tree into its render target on every tick in which the component was rendered
  in the last 0.5 s. Each SLyud* widget throttles its own repaint with RegisterActiveTimer(0.25-1 s), but that does not stop the
  widget renderer, which repaints the whole tree on every draw. Every OnPaint also rebuilds strings (LyudText::Get(...).ToString(),
  FStringFormatArg, Printf: SLyudTransitWidgets.cpp:186-343, 398-415, 496-508). UpdateBoards (LyudCableway.cpp:439-464) hides
  station boards beyond 250 m, but the cabins' 5 widget components are never culled by distance or turned off.
- Cost: 12 cabins x 5 widget components = 60 RT draws per frame (24 of them 1600x900 diagrams) whenever cabins are on screen,
  and cabins over the town are visible from most places. Each draw is a Slate paint (CPU), a render-target pass (GPU) and the
  string churn above. Likely the most expensive steady cost in transport, on the order of milliseconds.
- Fix: in all places above, `W->SetManuallyRedraw(true)` plus `W->RequestRedraw()` from the matching Slate widget's Repaint timer.
  If that is awkward, at least `W->SetRedrawTime(0.25f)` (diagrams, boards) and redraw once for static ones (plates,
  `bLive(false)` posters, signs). In ULyudTransportSubsystem::Tick's cabin loop (LyudTransport.cpp:636-656), do what UpdateBoards
  does: posters and board only when Dist < ~3000 cm or aboard, plates when Dist < ~15000 cm
  (`SetVisibility` + `SetTickMode(Disabled)`).
- Confidence: high that it redraws every frame; medium on the size.

## T2. S2: UpdateRopes re-evaluates the whole rope with an O(points x spans) search every frame
- Where: Private/Transport/LyudCableway.cpp:375-407, and LyudTransport::FNetwork::RopeOffset in
  Private/Transport/CablewaySim.cpp:1437-1462. Called every frame from ULyudTransportSubsystem::Tick (LyudTransport.cpp:662).
- What happens: for each of the ~1358 loop segments within 900 m of the viewer (that is all of the town's lines), `Point()` is
  called twice. Each call runs RopeOffset, which scans every span of the line (28 or 40) with an `fmod` (`FLine::Ahead`) until it
  hits one, then runs `L.At()` (an upper_bound plus a Hermite evaluation). Each segment is then pushed with its own
  UpdateInstanceTransform. Every interior point is evaluated twice, once as segment I's end and once as I+1's start.
- Cost: about 2.7k RopeOffset calls x ~15-20 fmod each, plus 2.7k At() calls and 1.36k ISM instance updates per frame. Roughly
  0.5-1.5 ms of game thread per frame, even in calm weather when the sway is far below a pixel.
- Fix (local): precompute each loop point's span index and its sin(U)/sin(2U)/normal once (in BuildRopes). Each frame, compute
  `Offset[I]` once per point from the span's Qv/Qh, which is O(N) with no search. Then send the contiguous near range in one
  `BatchUpdateInstancesTransforms(First, Ts, true, false, true)`. Skip a line entirely when its span amplitudes changed by less than
  ~0.5 cm since the last push.
- Confidence: high on the work done; medium on the ms.

## T3. S2: ULyudDoorSubsystem::UpdateAgents tests every agent against every door every 0.1 s
- Where: Private/Doors/LyudDoor.cpp:803-889 (the loop at :845). Also :813-825, a TActorIterator<APawn> each update with a linear
  `Agents.ContainsByPredicate` per pawn, which is O(P x A).
- What happens: every 0.1 s, for every agent (each registered NPC body, PeopleLife.cpp:778, including hidden or indoor ones far
  from the player) and every door (300+), it runs `InverseTransformPosition`, `InverseTransformVector`, the virtual
  `GetDoorwayLocalBox()` and `A.Holding.Contains(Door)`.
- Cost: about 100 bodies x 300+ doors = 30k+ transform tests in one frame out of every ~6. That is a periodic 1-2 ms spike that
  grows with both people and doors.
- Fix: run the box tests only when `FVector::DistSquared2D(Door->GetActorLocation(), Pos) < FMath::Square(400.f)` (cheap, no
  virtual call). Better, keep a coarse grid (e.g. 10 m cells) of door indices built in RegisterDoor and only look at the agent's
  cell and its neighbours. Skip agents whose actor `IsHidden()` and that hold no door. Replace the pawn ContainsByPredicate with a
  `TSet<TWeakObjectPtr<AActor>>`.
- Confidence: high on the complexity; medium on the counts.

## T4. S2: moving-every-frame meshes left in Lumen and the distance fields (cabins, train, lobby props)
- Where:
  - cabins: Shell, DoorLeaf (Private/Transport/LyudCableway.cpp:529-534), InsideParts and Hanger (:571-582), the box-kit parts
    (:705-720);
  - train: every `Piece` (Private/Railway/LyudTrain.cpp:166-182: body, glass, trim, underframe, interior, bogies, pantograph,
    gangway, 8 door leaves per car) and the fallback box (:194);
  - lobby animated props: LobbyMesh (Private/Transport/LyudLobby.cpp:111-127), used for the chandelier and beacon lens (Spin),
    the pendulum and 11 fish per aquarium; steam spheres (:163-171); bubbles (:288-296);
  - the lift cab parts (LyudStationBuild.cpp:1680-1690; they move only while riding).
- What happens: these components move every frame (cabin Drive, LyudCableway.cpp:768; train Place, LyudTrain.cpp:394;
  UpdateLobbies, LyudLobby.cpp:441-505), but unlike the ropes, wake and bikes they keep `bAffectDynamicIndirectLighting` and
  `bAffectDistanceFieldLighting` true. This is what the project's perf checklist ("movers out of Lumen") warns about: per-frame
  Lumen card and global distance field updates.
- Cost: 12 cabins x ~4-10 primitives each, plus ~3 cars x ~15 primitives for the train, plus the lobby props within their Far.
- Fix: set both flags to false where these components are created (the `Mesh` lambda, `Part`, `Piece`, LobbyMesh when it is
  animated, Steam, bubbles, and Shell/DoorLeaf in the cabin constructor). If the big train body has to keep bouncing light,
  exclude at least the small parts (leaves, bogies, interior).
- Confidence: medium (follows the project rule; not measured here).

## T5. S2/S3: the train wake adds and removes ISM instances every frame while spraying
- Where: Private/Railway/LyudTrainWake.cpp:301-329 (`Push`).
- What happens: the drop and foam counts change every frame (emitted and expired), so each frame calls AddInstances (with a
  temporary TArray) or RemoveInstances (a TArray of indices) on 3 ISMs before the batch update. Adding or removing instances
  rebuilds the instance buffers and proxy data, which costs much more than a transform update. Feed also allocates 4 TArrays
  every frame (DT, FT, TT, WetWheels), plus `Pts` in LyudTrain.cpp:417.
- Cost: while the train runs wet near the camera, 1-3 ISM add/remove ops per frame on instance sets of up to MaxDrops/MaxFoam.
- Fix: size each ISM once to MaxDrops/MaxFoam and give unused slots a zero-scale transform. Each frame, run one
  BatchUpdateInstancesTransforms over `[0, max(LastUsed, NowUsed))` only. Keep DT/FT/TT/WetWheels as member arrays (Reset, not
  realloc).
- Confidence: medium.

## T6. S1 (slow growth): LyudTransport::FNetwork::Riders never shrinks
- Where: Private/Transport/CablewaySim.cpp:578 (`Travel`) and :769 (`PlayerBoard`) push_back. Done and Cancelled riders are never
  removed: indices are used as ids by PeopleRides and Events. The vector is scanned in full in Dispatch (:988-997, every 0.1 s
  substep), ArriveAt (:897, :935), Leave (:956), PlayerAlight (:807) and FindRider (:529). FindRider is called every frame while
  the player is in a cabin (LyudTransport.cpp:897).
- What happens: every ride by a person or the player adds an FRider (with its Legs std::vector) for the rest of the session.
  Memory and scan time grow linearly over a long session. The rate is low: tens to hundreds of rides per game day.
- Fix: keep a free list. In HandleEvents, after `Done` (and in Cancel), mark the slot reusable once PeopleRides no longer points
  at it, and have Travel/PlayerBoard reuse a free slot before push_back. Alternatively, compact on a timer, remapping
  PeopleRides values, when no Waiting/Riding rider points past the cut.
- Confidence: high on the growth; low on its size in practice.

## T7. S3: the hidden train is still placed every frame
- Where: Private/Railway/LyudTrain.cpp:385-405 (Place). It runs every frame from ALyudTrain::Tick -> StepTrain.
- What happens: SetWorldTransform runs on every car root before the `!bShow` early return, so for the whole time the train is in
  the tunnel or the haze it moves ~3 cars x ~35 attached components (meshes, lights, 15 collision boxes per car, audio) and their
  render proxies, though nothing is shown. `TArray<FTransform> Old` is also allocated every frame.
- Fix: when `!bShow && !bWasVisible`, update only `Cars[C].S` and return before SetWorldTransform (place the cars on the frame they
  become shown). Keep `Old` as a member, or use a TInlineAllocator<4>.
- Confidence: high on the behaviour; the cost is moderate.

## T8. S3: UpdateHallSound runs TActorIterator<ALyudCabin> inside a per-station loop every frame
- Where: Private/Transport/LyudStationFurnish.cpp:230-234, called from UpdateBoards every frame.
- What happens: 7 stations x a TActorIterator (each builds an object array via GetObjectsOfClass) x 12 cabins, every frame.
- Fix: loop over `Transport->GetCabinActors()` (already held), or compute bNear only every 0.5 s.
- Confidence: high.

## T9. S3: the transport prompt is rebuilt and pushed every frame
- Where: Private/Transport/LyudTransport.cpp:922-990 (UpdatePlayer). For example, while riding:
  `LyudText::Get("transport.prompt.ride", {..ToString(), ..ToString()})` every frame (:987), then `UI->SetPrompt` every frame
  (:990), even when the prompt is empty.
- Fix: cache the prompt key, cabin and destination, and rebuild and SetPrompt only when they change (the train already does this
  every 0.15 s: LyudTrain.cpp:887-904).
- Confidence: high; the cost is small.

## T10. S3: UHeroBikeComponent::Get is FindComponentByClass on the hero, called several times per frame
- Where: Private/Bikes/HeroBike.cpp:47-50. Called from LyudBikeSubsystem.cpp:435 every frame, and from HeroCharacter.cpp:183, 189,
  216, 258 (hero tick, [OTHER-THREAD] caller). Also HeroBike.cpp:366: `GetClass()->GetName().Contains("Landscape")` makes an
  FString on every ground trace (2-3 per frame while riding). LyudBikeSubsystem.cpp:583: `Clip("Walk")->GetName().StartsWith(...)`
  makes an FString every frame per NPC leading a bike.
- Fix: cache a TWeakObjectPtr<UHeroBikeComponent> on the hero (or in the subsystem); use `C->IsA<ALandscapeProxy>()` or
  `Cast<ULandscapeHeightfieldCollisionComponent>`; cache the bShared answer per body.
- Confidence: high; the cost is small.

## T11. S3: the lift's landing leaves build one identical runtime UStaticMesh per leaf
- Where: Private/Transport/LyudStationBuild.cpp:1766-1773. `Way->KitMesh(Lf, ...)` runs FLyudMeshKit::Build (a fresh UStaticMesh,
  BuildFromMeshDescriptions plus a body setup, LyudMeshKit.cpp:411-460) for each of the 2 x stops leaves of each lift.
- Cost: BeginPlay hitch and duplicate mesh memory: 7 lifts x ~6-8 leaves of one box mesh.
- Fix: build the leaf mesh once (a static or member cache keyed by name), then NewObject a component per leaf that shares it.
- Confidence: high.

## T12. S3: ALyudDoorBase::SomeoneInSweep rebuilds the body list every frame per moving or waiting door
- Where: Private/Doors/LyudDoor.cpp:307-322 and GetBodies :780-793. Called every tick while a door is closing or bWaitingToClose,
  which can last as long as someone stands in the sweep.
- What happens: a new TArray of the player plus all agents (100+) per call, then a transform per body.
- Fix: iterate the subsystem's Agents in place (an `ForEachBody(TFunctionRef)`), with a distance prefilter against the door.
- Confidence: high; the cost is small to moderate.

## T13. S3: train gulls respawn on every pass, with a sync LoadObject on each call
- Where: Private/Railway/LyudTrain.cpp:670-695 (5 ABirdActor spawned each time bWant turns true after the last flock was destroyed,
  :758-768) and :744-752 (an FString Printf plus LoadObject every 4-14 s per gull; only the first call per species loads from
  disk).
- Fix: keep the 5 gulls and hide or park them instead of Destroy and re-spawn. Preload the 3 call sounds in Setup into `Sounds`.
- Confidence: medium.

## T14. S3 (latent bug): the level crossing looks at train 0 only
- Where: Private/Railway/LyudLevelCrossing.cpp:232: `Run.At(Now)` uses Index = 0, so "on the crossing" and the halt check ignore
  other trains. NextPass does cover all trains. Harmless while railway_built.json has trains = 1; wrong if it is ever raised.
- Fix: loop `for (int i = 0; i < Run.Params().Trains; i++)`.
- Confidence: high on the code; no effect today.

## Minor notes (not worth a ticket alone)
- CurtainPusher.cpp:24 runs a TActorIterator<ACharacter> every frame and writes 2 MPC vectors every frame, even when no character
  is in range. Skip when nothing changed.
- ConveyorBelt.cpp:57-66 allocates 2 TArrays every frame. The Motor plays at volume 0 forever (BeginPlay :46-48). Play/Stop it on
  load instead.
- LightFlicker.cpp:15 runs a TActorIterator<AActor> over the whole world at BeginPlay. A one-off, but the world is large: use
  TActorIterator over tagged classes or a tag registry.
- ALyudCabin box-kit and room colliders (UBoxComponent BlockAll, LyudCableway.cpp:592-612) teleport with 12 moving cabins every
  frame: ~120 kinematic body updates a frame. Acceptable; noted for profiling.
- The transport build (Cableway->Build in OnWorldBeginPlay) is a large synchronous startup step (meshes, MIDs, traces, runtime mesh
  builds). Expected; noted for load-time profiling.

## Fixes applied

Paths relative to unreal/Lyudmilovsk/Source/Lyudmilovsk. Not built here (the lead builds).

- **MID parent warning** (`MID_M_TR_Glass_0 is not a valid parent for MID_MID_M_TR_Glass_*`): done. Its source was the lobby
  steam puffs, Private/Transport/LyudLobby.cpp `LobbyItem` / `Steam` lambda: `UMaterialInstanceDynamic::Create(Mat("glass"))`
  where Mat("glass") is itself a MID. Each puff's MID is now made from the glass MID's `Parent` (M_TR_Glass). The puffs
  had no parent material before (the engine refused it), so now they actually render as the intended faint glass.
  Verify: the LogMaterial "not a valid parent" lines are gone from the load log.
- **T1** done. Every world widget gets `SetRedrawTime`: the live diagrams (club board LyudCableway.cpp `BuildClubBoard`,
  vestibule stele LyudStationBuild.cpp) 0.25 s, which is their own Slate timer; the station boards 1 s; the cabin board 0.5 s;
  static things 5 s (they only change with the language): cabin posters (bLive false), cabin plates, station signs and
  kiosk labels (the default in `ALyudCableway::Widget`), the rail halt boards (LyudRailTrack.cpp) and the crossing sign
  (LyudLevelCrossing.cpp). Distance culling in ULyudTransportSubsystem::Tick's cabin loop (LyudTransport.cpp): the cabin
  board and posters are shown and ticked only when aboard or within 30 m, the plates within 150 m (SetVisibility and
  SetTickMode only when the state changes). Verify: `stat slate` / Insights show far fewer WidgetComponent draws; the boards
  still update (cabin board text, live dots on the diagrams 4 times a second).
- **T2** partly done (the safe small part). LyudCableway.cpp `UpdateRopes`: a segment's end point is reused as the next
  segment's start (it is the same `Point(I+1, bNear)`), so RopeOffset/At run about half as often, with identical output.
  Skipped: the per-point span cache and "skip when the ropes don't move" (changes the algorithm, more than a small change).
- **T3** done. LyudDoor.cpp `UpdateAgents`: each update computes once per door CanNpcsPass and a world sphere that encloses
  its approach box (with scale); an agent outside that sphere skips the transforms and the virtual box (it can't be inside
  the box), and the door is skipped entirely when the agent holds no doors. The pawn check uses a TSet instead of
  ContainsByPredicate. Same results. Verify: Door test (LyudDoorTest) passes; the 0.1 s spike in Insights is smaller.
- **T4** done. bAffectDynamicIndirectLighting/bAffectDistanceFieldLighting = false on: cabin Shell and DoorLeaf
  (ALyudCabin ctor), the `Mesh` lambda parts (InsideParts, Hanger) and the box-kit `Part`s (LyudCableway.cpp); every train
  `Piece` and the fallback box (LyudTrain.cpp `BuildCar`); the lobby movers (spin, pendulum and what hangs on it, steam,
  fish, bubbles) via `MoversOutOfLumen` in LyudLobby.cpp, once at build from `LobbyCentre`; the lift cab parts
  (LyudStationBuild.cpp `ALyudStationLift::Setup`). Verify: `r.Lumen.Visualize.CardPlacement` shows no cards on them.
- **T5** done. LyudTrainWake.cpp `Feed` + Public/Railway/LyudTrainWake.h: the ISMs are a stable pool that only grows; slots
  past the live ones get a zero-scale transform (parked at the front wheels, not the world origin, so the bounds stay put);
  each frame one BatchUpdateInstancesTransforms over [0, max(last, now)), nothing when both are 0. No RemoveInstances, no
  render-state dirty. WetWheels and the 3 transform arrays are members (Reset). Verify: the wake looks the same; no
  instance-buffer rebuilds while spraying.
- **T6** done. CablewaySim.h/.cpp: `FNetwork::AddRider` (reuses a released slot) in Travel and PlayerBoard, and
  `ReleaseRider` (only a Done trip, only with `bReuseRiders`, off by default so tools/transportsim keeps its old indices).
  The game turns it on in ULyudTransportSubsystem::Start and releases a trip's slot at the end of HandleEvents for its
  Done/Alight events, after PeopleRides dropped it (PeopleRides, events and the cabins' Riders were checked: nothing holds a
  Done index after that; Cancelled trips are never released, since PeopleRides may still point at one). Riders now grows
  only to the most trips alive at once. Verify: test_transport; the "rider %d" numbers in the log stop climbing.
- **T7** done. LyudTrain.cpp `Place`: while hidden (nobody aboard) only the cars' S is updated, no SetWorldTransform; on the
  frame it shows the cars are placed with TeleportPhysics and CarryPlayer gets the new transforms as "old" (no carry from
  the stale spot). `Old` and the wake's wheel points are members (OldCars, WakeWheels). Verify: the train comes out of the
  tunnel in the right place; riding still carries the player.
- **T8** done. LyudStationFurnish.cpp `UpdateHallSound`: one TActorIterator<ALyudCabin> per frame into a small list, then the
  per-station test over it. Same result.
- **T9** done. LyudTransport.cpp `UpdatePlayer` (+ LyudTransport.h PromptShown/PromptText/PromptRefresh): the prompt is a key
  (kind, cabin, door state or destination); its text is built only when the key changes, and pushed then and every 0.25 s
  while non-empty (in case another prompt took the line), the empty prompt once.
- **T10** done (HeroBike side). HeroBike.cpp `UHeroBikeComponent::Get`: the last hero/component kept in thread_local weak
  pointers (a hero without one is searched each time, as before); `GroundAt`: the "Landscape" name test is made once per
  hit class. LyudBikeSubsystem.cpp: the walk clip's "A_Human" test once per clip. HeroCharacter.cpp untouched.
- **T11** done. LyudStationBuild.cpp `ALyudStationLift::Setup`: the landing leaf mesh is built once (the first KitMesh); the
  other leaves are components made as KitMesh makes them, sharing that mesh and its materials.
- **T12** done. LyudDoor.cpp `SomeoneInSweep` + new `ULyudDoorSubsystem::AnyBody(TFunctionRef)` (LyudDoor.h): bodies are
  visited in place, with a sphere prefilter (sweep box grown by the body radius) before IsInSweep. Same results. GetBodies kept.
- **T13** partly done. LyudTrain.cpp `UpdateGulls`: each species' call is looked up once (GullCalls map, UPROPERTY), no Printf
  or LoadObject per call. Skipped: keeping/parking the 5 gulls instead of respawning (ABirdActor state on reuse is not
  something I can check without a run).
- **T14** not assigned, not done.
