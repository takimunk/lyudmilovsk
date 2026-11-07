# Perf/leak/crash audit: people thread

Scope: Private/People (PeopleLife, PeopleSim, LyudBodyAnim, CounterTrade), Private/Animals (AnimalLife, AnimalSim, AnimalActor,
AnimalAnimInstance, AnimalCollar, AnimalNameTags), Private/Food (LyudFood, LyudHandUse, LyudBite, LyudStalls), Private/Smoking
(LyudSmoking, SmokingKit), Private/Diner (LyudDiner), Private/NpcDialogue.cpp, Private/BoothOwner.cpp, and their Public headers.
Read only; nothing built or run.

Scale, for the cost estimates: people.json has 33 people, activities.json 37, people_places.json 73 places and 57 flats;
lyud.Animals.Bodies = 20 animal bodies at most, within 110 m (cats) or 176 m (dogs).

Summary: the two plain-C++ sims (PeopleSim, AnimalSim) are tiered and cheap. Diner, Stalls, Bite, HandUse and Smoking are
mostly clean. The real costs are on the body side: placed people animate while hidden, and their eyeballs and props move
every frame without being taken out of Lumen and distance fields. There is also one mesh rebuild in play (the blanket), and
the anim worker code does a lot of name lookups.

---

## P1 [S2] Placed club people (and the booth owner) animate fully while hidden or off-screen. Confidence: high

- `Private/People/LyudBodyAnim.cpp:962-985` (ULyudFaceSubsystem::OnWorldBeginPlay) switches every placed SkeletalMeshActor on
  SK_Human_Skeleton to ULyudBodyAnimInstance. It never sets `VisibilityBasedAnimTickOption`. The engine default (UE_5.8
  SkeletalMeshComponent.cpp:446) is `AlwaysTickPoseAndRefreshBones`. No script under unreal/scripts sets it either: grep for
  VisibilityBasedAnimTickOption only finds HeroCharacter, AnimalActor, PeopleLife (ANpcBody) and BirdActor.
- `Private/BoothOwner.cpp:24-31`: the owner's Body has the same default.
- What happens: UPeopleLifeSubsystem hides a club actor whenever its person is away from their station
  (`PeopleLife.cpp:722-733` ShowPlaced, `901-904`), and hides stray copies for good (`613-621`). The owner is hidden by
  `SetAway`. These hidden actors keep running everything, every frame, wherever the camera is (town, sea, Academgorodok):
  - TickPose, which calls ULyudBodyAnimInstance::NativeUpdateAnimation:
    - 2 foot line traces, or up to 14 desk traces (see P4);
    - 2 eye MID `SetVectorParameterValue` calls;
    - a TArray allocation (P8).
  - The worker-side Evaluate: clip sample, blend, CS-pose IK, about 25 bone-name lookups.
  - RefreshBoneTransforms, and the attached eyeball transforms (P2).
  Meanwhile the person's ANpcBody, which does use OnlyTickPoseWhenRendered, is animating the same person somewhere else. So
  club people cost double while they are away.
- Cost: 12 club seats plus the strays and the owner, all at full anim cost every frame. This is probably 1-3 ms of game and
  worker time while the player is nowhere near the club.
- Fix:
  - In ULyudFaceSubsystem::OnWorldBeginPlay, inside the conversion loop, set
    `Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;`.
  - In the ABoothOwner constructor, set `Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;`.
  - Optionally, in `UPeopleLifeSubsystem::ShowPlaced` also call `M->SetComponentTickEnabled(bShow)`, and in
    `ABoothOwner::SetAway` call `Body->SetComponentTickEnabled(!bAway)`.

## P2 [S2] Eyeballs and hand props move every frame and are still in Lumen and distance fields. Confidence: high (the flags are definitely missing; the size of the cost is an estimate)

The project's rule (perf checklist "movers out of Lumen"; Food, Diner, Smoking and Cableway all follow it) is
`bAffectDistanceFieldLighting=false` and `bAffectDynamicIndirectLighting=false` on anything that moves. These components break it:

- `Private/People/LyudBodyAnim.cpp:889-896` (MakeEyes): 4 engine-Sphere components per face body (2 balls, 2 sockets),
  attached to the `head` bone. They move with the head every frame.
  - With `r.GenerateMeshDistanceFields=True` (DefaultEngine.ini:22), each one is a moving distance-field object. That means
    an object-buffer update plus global SDF clipmap invalidation around every head, and Lumen card recapture.
  - Count: 33 people plus the placed club people, so up to about 4 x 45 = 180 moving DF primitives.
- `Private/People/LyudBodyAnim.cpp:590-594` (Gamepad), moved every frame while gaming (`613`).
- `Private/People/PeopleLife.cpp:1144-1160`: PusherBlade, PusherPole and PusherBar, moved every frame while shovelling
  (`1204-1216`).
- `Private/Animals/AnimalActor.cpp:400-408` (animal eyeballs, Maru): 2 per body.
- `Private/Animals/AnimalCollar.cpp:20-33` (collar cord and tag): the tag gets `SetWorldLocationAndRotation` every frame
  (`144`).
- Fix: add `C->bAffectDistanceFieldLighting = false; C->bAffectDynamicIndirectLighting = false;` before `RegisterComponent`
  in MakeEyes and UpdateEyes. Do the same in the `Part` lambda in ANpcBody's constructor, for Gamepad in UpdateGamepad, and
  in AnimalCollar `MakePart`. The eyeballs are a few cm across and have shadows off: nothing visible is lost.

## P3 [S2 hitch] The blanket mesh is rebuilt in place while it has render resources. Confidence: high

- `Private/People/PeopleLife.cpp:1627-1638` (ANpcBody::BuildBlanket). `BlanketMesh` is created once per body (`if (!BlanketMesh)`).
  Every later build calls `BlanketMesh->BuildFromMeshDescriptions` on that same mesh, which is still set on the `Blanket`
  component and still has render resources.
- When: once per sleeper per night, 0.6 s after a sleeping body is first driven in view. `Drive` (`1419-1430`) resets
  `bBlanket` when they wake, so every night after the first rebuilds a live mesh.
- Why it matters: this is the pattern the perf checklist bans (7c). Rebuilding a drawn UStaticMesh runs a TObjectIterator over
  all components, flushes rendering commands and waits for the GPU. It hitches every time the player walks into a flat at
  night.
- Fix: build a fresh mesh each time and let the old one go to GC. Replace the `if (!BlanketMesh)` block with an unconditional
  `BlanketMesh = NewObject<UStaticMesh>(this);` plus the material line, then build and `SetStaticMesh`. That matches what
  PlantMeshes does in game worlds. The mesh is 35x19 cells; building a fresh one is cheap.

## P4 [S2] Line traces in the per-body anim update. Confidence: med

- `Private/People/LyudBodyAnim.cpp:702-730`: hands on the desk. While HandAlpha > 0.01 (PC_Type, PC_Game and PC_Scroll
  clips), each hand traces up to 7 times (D = 0..30 step 5) until it hits. That is 2 to 14 traces per seated computer person
  per frame, for people sitting completely still. 12 club people plus the office workers comes to about 24-170 traces per
  frame.
- `Private/People/LyudBodyAnim.cpp:665-697`: feet. 2 traces per standing or walking body per frame.
- `Private/People/PeopleLife.cpp:878-888` (Present): 1 more ground trace per outdoor person within 60 m per frame, on top of
  the feet.
- Fix:
  - Desk: cache `HandOff` and only retrace when the component moved more than about 1 cm or turned, or at most every 0.25 s.
    Seated people don't move.
  - Feet: skip the traces and let FootAlpha go to 0 beyond about 25 m from the camera. Throttle to every other frame for a
    body that is standing still.
  - Present's ground trace: only for moving people, or reuse the trace result for 0.2 s.

## P5 [S1-latent, GC] The anim proxy holds clips that GC doesn't see. Confidence: low-med

- `Public/People/LyudBodyAnim.h:17,29`: `FLyudBodyClip::Seq` is a `TObjectPtr<UAnimSequence>` inside
  `FLyudBodyAnimProxy::Cur/Prev`. The proxy is not a UObject and doesn't override `AddReferencedObjects`, so GC never sees
  these references.
- Usually something else keeps the clips alive: ANpcBody::Clips (UPROPERTY), PcClips, AnimToPlay, and
  ULyudPrefetch::Held (a TArray UPROPERTY).
- `Private/BoothOwner.cpp:72-73` is the exception: `LoadObject<UAnimSequence>(".../A_Human_StandUp|SitDown|Walk|StandIdle")`
  goes straight into `PlayClip` and is only referenced by the proxy. On a level or run where nothing else holds that clip
  (people life off, prefetch not yet done), a GC frees it while the worker thread samples it. That is a crash.
- Fix: in FLyudBodyAnimProxy, override `AddReferencedObjects(UAnimInstance*, FReferenceCollector& C)` and call
  `C.AddReferencedObject(Cur.Seq); C.AddReferencedObject(Prev.Seq);` plus Super. Alternatively, keep a
  `UPROPERTY(Transient) TObjectPtr<UAnimSequence> CurSeq, PrevSeq` on ULyudBodyAnimInstance and set it in PlayClip.

## P6 [S3] CounterTrade builds a full FPersonInfo per counter every frame. Confidence: high

- `Private/People/CounterTrade.cpp:138-169` (Tick, every frame while counters.json has rows), which calls `IsOpen` at
  `93-110`, which calls `UPeopleLifeSubsystem::FindPerson` and `GetPerson` (`PeopleLife.cpp:1018-1066`).
  - FindPerson does a TCHAR_TO_UTF8 conversion and a linear std::string search.
  - GetPerson fills about 8 FStrings from UTF8 (Key, Occupation, Household, Home, Activity, Place, State), builds 7 FNames,
    and grows a TMap of needs.
  - It then does 3 more UTF8 FString temporaries for the ClubActors lookups.
- Cost: about 25 heap allocations per counter per frame, to read 3 fields.
- Fix:
  - Cache the person id per counter once the people life has started.
  - Add a lightweight accessor such as `bool UPeopleLifeSubsystem::GetActivityState(int32 Id, FName& Activity, uint8& State, EPersonPose& Pose, FVector& Loc)`.
  - Use the same accessor in `LyudDiner.cpp:620-637` (1 Hz, fine) and `Bikes/LyudBikeSubsystem.cpp:648` [OTHER-THREAD].

## P7 [S3] Anim Evaluate does string formatting, name lookups and small allocations per bone. Confidence: high

These are worker threads, per visible body per frame.

- `Private/Animals/AnimalAnimInstance.cpp:205-206,218,258-260`: `FindBone(Bones, TEXT(...))` builds an FName from a string
  every evaluation. Line 218 does `FString::Printf(TEXT("tail_%02d"), K)` for up to 9 bones (a heap string plus a name-table
  lookup each).
- `Private/Animals/AnimalAnimInstance.cpp:147-158`: `RotateCS` allocates a `TArray<FBoneTransform>` per call, about 15 times
  per evaluation.
- `Private/People/LyudBodyAnim.cpp:187-191,206,216,266,277-322`: about 25 `Bone(TEXT(...))` FName constructions per body per
  evaluation. FName lookups take the name-pool shard lock, so they contend across anim workers.
- Fix:
  - Resolve the compact indices once per bone container in PreUpdate or on the first Evaluate. Cache them keyed on
    `RequiredBones.GetSerialNumber()`, using `static const FName` names.
  - In RotateCS, use a `TArray<FBoneTransform, TInlineAllocator<1>>` or a stack-local `TArray` reused across calls.

## P8 [S3] Small per-frame allocations in the human body update. Confidence: high

- `Private/People/LyudBodyAnim.cpp:478`: `FaceCurves = TArray<float>(Morph, 6)` heap-allocates per face body per frame.
  Make FaceCurves a `float[6]`, or `SetNumUninitialized(6)` once and memcpy.
- `Private/People/LyudBodyAnim.cpp:585`: `CurrentClip()->GetName()` allocates an FString per frame while gaming.
- `Private/People/LyudBodyAnim.cpp:700`: 3 `FName(TEXT(...))` comparisons per frame. Use `static const FName`.
- `Private/People/PeopleLife.cpp:1457` (ANpcBody::Drive, walking): `Clip("Walk")->GetName().StartsWith(...)` allocates an
  FString per walking body per frame. Cache a bool next to the clip.
- `Private/People/PeopleLife.cpp:798,817` (Present): a `TSet<FString>` plus an FString converted from UTF8 per club-seat
  person per frame. Key ClubActors by the std::string tag, or precompute the FString per slot.

## P9 [S3] TActorIterator<AHeroSeat> every frame, twice. Confidence: high

- `Private/NpcDialogue.cpp:501-507` (ANpcTalkDirector::PlayerBusy, from Tick at 518) and
  `Private/Animals/AnimalNameTags.cpp:147-153` (from Tick at 174).
- Each one iterates the actors of a class every frame, just to ask whether the player is in a seat's range.
- Fix: cache the seats in BeginPlay as `TArray<TWeakObjectPtr<AHeroSeat>>`, or have AHeroSeat expose a static "the seat in
  range" pointer.

## P10 [S3] Log line per train-time query while the railway isn't running. Confidence: med

- `Private/People/PeopleLife.cpp:292-300`: when `Stops` returns null (railway off, not started yet, or the lyud.Railway A/B),
  `Sim->TrainNext` writes `UE_LOG(Log, "no train ...")` on every call.
- It is called from `PeopleSim.cpp:729` (WaySeconds). WaySeconds runs inside `Score`, which runs for every activity on every
  Think and Interrupt of every person whose best slot is on the island. It is also called every 10 s per Waiting person
  (`PeopleSim.cpp:2347`).
- The train hops exist from data alone (`Hops`/`RailStopOf`, `PeopleSim.cpp:677-693`), so this spams the log file all the
  time with the railway off.
- Fix: return false quietly, with at most one log line, for example a `static bool bSaid`, or log only when
  `R && !R->IsRunning()` changes.

## P11 [S3 latent bug] FindFood returns a reference into a static TMap that can still grow. Confidence: low-med

- `Private/Food/LyudFood.cpp:182-199`: `return Plain.Add(Id, F);` returns a reference into a `static TMap`. A later `Add` for
  another id can reallocate the map and leave that reference dangling.
- Callers hold the reference for a while: `HoldFromPocket` (`265`, used through `372`) and `ALyudFoodItem::Init` (`138`).
  It only goes wrong if something in between calls FindFood for a new id. Not seen today, but fragile.
- Fix: return by value (FLyudFoodDef is small), or fill `Plain` for every inventory food at first use.

## P12 [S3] Reusing a spare animal body with another coat reloads the whole breed. Confidence: high

- `Private/Animals/AnimalLife.cpp:896-899` calls `SetBreed` (`AnimalActor.cpp:267-300`). That does:
  - `LoadObject` of the mesh, 22 clips (each path built with Printf) and 3 materials;
  - `SetSkinnedAssetAndUpdate` plus `SetAnimInstanceClass`, which re-initialises the anim instance;
  - new body MIDs;
  - destroying and re-creating the eyeball components.
  It all happens on the frame an animal comes into view.
- The assets are already loaded (the prefetch gates the breed), so this is a small hitch (sub-ms to about 1 ms), not a load.
- Fix: when `Breed` is unchanged, only call `ApplyCoat()` and `UpdateEyes()` (UpdateEyes for its body MIDs) and skip the mesh
  and clip reload. Better still, prefer a spare with the same breed and the same coat.

## P13 [S3] The talk director's People list is never pruned. Confidence: med

- `Private/NpcDialogue.cpp:410-425` (AddPerson) only appends.
- `UPeopleLifeSubsystem::Stop` (`PeopleLife.cpp:234-259`) destroys the bodies and clears `Talkable`. After a restart,
  `Present` (`919-927`) adds the new bodies again, and the stale weak entries stay. Tick then loops over all of them twice per
  frame.
- No crash (they are TWeakObjectPtr), but it grows with every lyud.People 0/1 toggle.
- Fix: in AddPerson, `People.RemoveAll([](const FNpc& N){ return !N.Actor.IsValid(); })`.

## P14 [S3] Smoke rewrites all 96 instances every frame while any puff is alive. Confidence: high (minor)

- `Private/Smoking/SmokingKit.cpp:403-426`: dead puffs are rewritten too, with a size of 0.001, and get 2
  `SetCustomDataValue` calls each.
- It already avoids a proxy rebuild (bMarkRenderStateDirty=false), and it only runs while smoking.
- Possible fix: track the dirty range, or skip dead puffs whose instance is already hidden (keep a `bHiddenWritten` flag per
  puff).

## P15 [S3] Small leftovers. Confidence: high (all trivial)

- `UHeroReachComponent::Get` does a FindComponentByClass on every call, several times per frame:
  - `LyudFood.cpp:410,435`
  - `LyudHandUse.cpp:296`
  - `LyudSmoking.cpp:320,853,862`
  - `LyudBite.cpp:329`
  Cache it per subsystem as a TWeakObjectPtr.
- `Public/Animals/AnimalLife.h:173-175`: the `Sight` and `ProbeTime` caches are declared but never used.
  - Tier-0 animals do an uncached `Probe` trace every frame in Move (`AnimalSim.cpp:290,2863`).
  - They also do a `Sight` trace on each 0.15 s perception tick (`AnimalSim.cpp:1811-1813`).
  - Either wire up the caches (as BirdLife does) or delete the members.
- `AnimalLife.cpp:396-410`: builds a TArray<FBirdBeast> every frame and copies it into BirdLife. Reuse a member array.

## Checked and clean (no findings)

- PeopleSim/AnimalSim steps: tiered thinking, a spatial hash, O(N^2) only every 3 s for talk pairs (N=33).
- LyudDiner: state at 1 Hz, MIDs made once, delegates removed in EndPlay, dishes destroyed after 45 s, the coffee stream out
  of Lumen.
- LyudStalls: once at BeginPlay.
- LyudBite and LyudHandUse: they only tick while active.
- LyudSmoking: butts capped at MaxButts and destroyed; the lighter only ticks while lit.
- LyudFood: delegate removed in Deinitialize; food items out of Lumen.
- AnimalCollar: pendulum work only while rendered.
- AnimalLifeRenderer: spare pool bounded by roughly Bodies x breeds; hidden spares stop ticking.
- PeopleLife and AnimalLife save delegates use AddWeakLambda; railway and winter lambdas capture TWeakObjectPtr.
- The NpcDialogue and AnimalNameTags overlays are removed in EndPlay.
- ANpcBody bodies are bounded by the number of people.

---

## Fixes applied

Nothing built or run here (the lead builds). Paths under unreal/Lyudmilovsk/Source/Lyudmilovsk.

- **P1 done.** `ULyudFaceSubsystem::OnWorldBeginPlay` (Private/People/LyudBodyAnim.cpp) sets
  `OnlyTickPoseWhenRendered` on every placed person it converts; `ABoothOwner::ABoothOwner` (Private/BoothOwner.cpp) on Body.
  Checked readers of offscreen bones: NpcDialogue `HeadOf` is only used for the person in front of the camera (rendered, so
  fresh); eyeballs/gamepad are attached and only matter when drawn; PeopleLife's bone reads (pusher, blanket) are ANpcBody,
  which already had this option. One real catch, fixed: lip-sync timed by the instance's own `Clock`, which stops while
  the body isn't ticked, so a line started unseen would lag the voice. `Speak` now stores `SpeakStartWorld` and
  `UpdateMouth` takes the frame from world time (this also fixes the same lag ANpcBody already had). Optional
  SetComponentTickEnabled not done (not needed). Verify: `stat anim` / Insights with the camera away from the club, where the
  placed people's TickPose/Evaluate should be gone; test_faces (lyud.Face.Speak) still moves the lips; club people still blink
  and look at you.
- **P2 done.** `bAffectDistanceFieldLighting=false` and `bAffectDynamicIndirectLighting=false` on: human eyeballs
  (`ULyudBodyAnimInstance::MakeEyes`), the gamepad (`UpdateGamepad`), the pusher parts (the `Part` lambda in
  `ANpcBody::ANpcBody`, PeopleLife.cpp), animal eyeballs (`AAnimalActor`, Private/Animals/AnimalActor.cpp, the eye loop at
  ~400), and the collar cord and tag (`MakePart`, AnimalCollar.cpp). Verify: `r.DistanceFields.LogAtlasStats` / the DF object
  count goes down by about 4 per face body; no visible change.
- **P3 done.** `ANpcBody::BuildBlanket` always makes a fresh `UStaticMesh` (the old one goes to GC) instead of rebuilding
  the drawn one. Verify: no FlushRenderingCommands hitch when walking into a flat at night (Insights), and the blanket still
  shows.
- **P4 done.**
  - Desk hands (`ULyudBodyAnimInstance::UpdateBody`): the last hit is cached per hand (the step D, the desk's z, the hand's
    world spot). It is retraced when the component moved or turned, when the clip's hand moved more than 2 cm from where it
    was traced, or after 0.25 s; otherwise the offset is rebuilt from the cached desk height (same formula), so the IK target
    stays identical on a flat desk.
  - Feet: traced every frame within 25 m of the camera, every 0.2 s beyond (the existing smoothing carries them between);
    alpha unchanged.
  - Present's ground trace (`UPeopleLifeSubsystem::Present`): a per-person `GroundCache`, reused while the person stays
    within 0.5 cm of the traced spot, for at most 0.25 s.
  - Verify: `stat collision`, or the trace counts in Insights (LyudHands/LyudFeet/PeopleGround) at the club.
    lyud.Face.Log 1 still prints the same hand offsets for seated typists.
- **P5 done.** `FLyudBodyAnimProxy::AddReferencedObjects` (LyudBodyAnim.h/.cpp) reports `Cur.Seq` and `Prev.Seq` to GC.
  Verify: `obj refs name=A_Human_StandIdle` shows the anim instance as a referencer.
- **P6 done.** New `UPeopleLifeSubsystem::FindActivity` and `GetActivityState` (PeopleLife.h/.cpp: no strings).
  `ULyudCounterTradeSubsystem::IsOpen` caches the person id and activity index per counter, keyed on the sim pointer and reset
  in `Load`. Same result: the activity matches, the state is "doing", and the person isn't hidden. Diner (1 Hz) and Bikes
  (not my file) were left on GetPerson. Verify: lyud.Trade.Open girl_b still logs "at work" at the library during her shift,
  and the T prompt still appears.
- **P7 done.**
  - Human `FLyudBodyAnimProxy::Procedural`: the 20 bones are resolved once per bone container (pointer + serial number;
    serial 0 means not yet) into `BoneIdx`, using static FNames.
  - Animal `FAnimalAnimProxy::Evaluate` (AnimalAnimInstance.h/.cpp): the same for head, necks, jaw, tail_01..09 and the
    ears. There is no Printf now, and `RotateCS` passes a one-element stack view to SafeSetCSBoneTransforms (no TArray).
  - Verify: poses unchanged (test_poses.sh, the animal tests); Insights shows no FName lookups under Evaluate.
- **P8 done.**
  - `FaceCurves` is now `float[6]` plus `bFaceCurves`, filled with a memcpy.
  - Gamepad: no `GetName()`. The check is whether the current clip is the PcClips entry for PC_Gamepad.
  - The desk-clip checks use static FNames.
  - `ANpcBody::Drive` works out the shared-walk flag once (`SharedWalk`).
  - Present no longer builds a `TSet<FString>` or an FString per club seat: there is `ClubSlotActors` (slot index -> its
    tag's actor, filled in BindClub) and a reused member `TSet<AActor*> PlacedShown`.
- **P9 done.** `ANpcTalkDirector::PlayerBusy` (NpcDialogue.cpp/.h) and `AAnimalNameTags::PlayerBusy` (AnimalNameTags.cpp/.h)
  keep a `TArray<TWeakObjectPtr<AHeroSeat>>` that is gathered again every 2 s instead of every frame. That way seats in
  levels that stream in later are still found.
- **P10 done.** TrainNext logs "no train" once, then stays quiet until a train answers again (a shared flag in the lambda).
  Verify: with lyud.Railway off, the log has one "PeopleLife: no train ... quiet until the trains answer again" line, not a
  stream.
- **P11 done.** `ULyudFoodSubsystem::FindFood`: the `Plain` map holds `TUniquePtr<FLyudFoodDef>`, so the references
  callers keep stay valid as the map grows. Same signature.
- **P13 done.** `ANpcTalkDirector::AddPerson` drops entries whose actor is gone. It only does this while nobody is being
  talked to, so TalkingIndex stays valid; TargetIndex is reset and recomputed in Tick. Verify: lyud.People 0/1 several
  times, and the "NpcDialogue" people count doesn't grow.
- **P14 done.** `ALyudSmokeFx` (SmokingKit.h/.cpp): each puff has a `bHiddenWritten` flag. A dead puff is written hidden
  once and then skipped, and only the touched range [Lo, Hi] goes to `BatchUpdateInstancesTransforms` (through a
  TArrayView). Verify: smoking still shows puffs, and they fade and vanish as before.
- Not assigned or not done: P12, P15; the Diner/Bikes accessor switch.
