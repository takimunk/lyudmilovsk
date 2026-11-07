# Branch review: town (town-life, public-spaces, city-terrain, wt/city-buildings)

Read-only review of `git diff $(merge-base origin/main origin/B) origin/B -- unreal/Lyudmilovsk/Source`.
Line numbers are on the branch (`git show origin/B:<path>`).

The four branches are tied together: town-life drives the window lights of city-buildings (`ULyudWindowLights`)
and city-terrain (`ULyudCityWindowsSubsystem`) through reflection, so some findings only show once they are merged
together. Those are marked [X-BRANCH].

---

## town-life (3 commits: ~900 townsfolk, 1989 town places, 1734 flats with windows)

Files: Private/People/PeopleLife.cpp, PeopleSim.cpp/.h, Public/People/PeopleLife.h, Private/LyudText.cpp

### T1 S2 (plain bug + cost): pooled townsfolk bodies stay door agents, parked hidden in doorways. Confidence: med-high
- PeopleLife.cpp:883 `Doors->RegisterAgent(Body, 35.f)` on spawn. ReleaseBody (1175-1186) only hides the body and puts it
  in `TownPool`; it is never unregistered or moved.
- Townsfolk are released exactly at building doors: a shell place's slots are all at its portal
  (PeopleSim.cpp LoadTownPlaces: `Sl.Pos = Pl.bShell ? Pl.Portal : ...`), and Present releases the body when
  `InsideShell(P)` turns true, i.e. at the door.
- `ULyudDoorSubsystem::UpdateAgents` (Doors/LyudDoor.cpp:825-880) does not check `IsHidden()`. A hidden body standing in
  the doorway box keeps `bInDoorway` true, so that door is held open until the pool hands the body to someone else of
  the same look (it may never happen). Every town body ever spawned also stays in the Agents x Doors loop for the session.
- Fix: in ReleaseBody call `Doors->UnregisterAgent(Body)` (re-register in BodyFor when taken from the pool), or
  have UpdateAgents skip and release for `Actor->IsHidden()`. Moving the pooled body to a far "park" point also works.

### T2 S2: TalkPairs is O(talkers x all people) every 3 game seconds, now with ~900 people. Confidence: med
- PeopleSim.cpp:2889-2953: for each person in a talk activity, it loops over all `People` (2900) with `Has(P.Friends, Q.Key)`
  string compares for those at the same place. A holiday gathering (Mingle) puts hundreds in talk at once:
  for example 400 x 900 = 360k iterations plus friend-name compares in one Step, every 3 s at time scale 1 (more often
  at higher time scales: the Step loop runs up to 900 steps per frame, PeopleLife.cpp:1122-1128).
- Fix: once per TalkPairs, bucket candidate partners by `Place` (vector<vector<int>> sized Places.size(), reused), and
  loop only over that bucket. Turn `P.Friends` into person ids at load.

### T3 S3: DutyNow does linear string lookups per person per step. Confidence: high
- PeopleSim.cpp:1621 `return FindActivity("sleep")` for every sleeping person, and 1627 (new) `FindActivity(D.Activity)`
  for each duty on a day off. DutyNow runs for every person in `Doing` on every Step (3043), plus in Interrupt and Think.
  With 900 people and 56 activities that is roughly 50k std::string compares per frame at night or on a holiday.
- Fix: in `Index()` cache `SleepAct = FindActivity("sleep")` and `FDuty::Act` (activity index) next to `FDuty::Place`.

### T4 S3: load-time O(N^2) lookups (hitch at the life's start). Confidence: high
- LoadTownPlaces PeopleSim.cpp:1064 `FindPlace(Pl.Id)` (linear) per place: about 2000 x 2000/2 string compares;
  Index() 1177 `FindPlace` per duty; `Index()` itself runs three times (LoadTownPlaces, LoadTownsfolk, Load).
  AssignHomes: `Has(Done, Household)` on a vector<string> and the household-size loop over all people per household:
  O(N^2) over 900. town_places.json (116k lines) + townsfolk.json (39k lines) are read and parsed on the game thread in Start.
- Fix: a `std::unordered_map<std::string,int> PlaceById` built as places are added (FindPlace uses it); `Index()` only
  once, at the end of Load; an unordered_set for Done plus a precounted household-size map. Optionally parse the JSON
  on a worker before Start.

### T5 S3: a clock jump or first bind sends every flat to both light sinks, one render command per flat. Confidence: high [X-BRANCH]
- PeopleLife.cpp:1272 `FlatLit.assign(..., -1)` on `bJumped` or first bind, so all 1734 flats count as "changed". For each one
  SendFlatLight runs ProcessEvent on both sinks, creates `FName(*FlatId)` (1243), and broadcasts OnFlatLight (1280).
  On the buildings' side `ULyudWindowLights::SetFlatLit` calls `B->Flush()` per flat (LyudCityBuilding.cpp:313), which
  is one `UpdateTextureRegions` with a heap copy, i.e. about 1300 render commands in one frame. On the walls' side
  SetFlatLit builds 4 Printf FNames per flat (LyudCityWindows.cpp:344), including for the 1292 "P-..." building
  flats it does not have (about 6.5k junk FNames, never freed, once per id).
- Fix: route each flat to its owner (an id with '/' goes to the buildings, "W*-D*" to the walls). Look names up with
  `FName(..., FNAME_Find)`. In ULyudWindowLights, make SetFlatLit only mark the building dirty and Flush once per building in Tick.

### T6 S3: `static double LastBind` in UpdateLights. Confidence: high
- PeopleLife.cpp:1262: this is a function-static shared by every world/PIE session. It is only a retry gate (the first
  bind is forced by `!bLightSinksBound`), so it does no harm, but it should be a member reset in Start.

### T7 S3 note: townsfolk share pool models on purpose
- BodyFor / ResolveModels exempt `bTown` from the one-model-one-person registry, and InstanceReport skips the "Townsfolk"
  tag (PeopleLife.cpp ~774). This is deliberate (docs/town_life.md), but test_unique_people now passes because of the tag skip.
  Keep that in mind when reviewing unique-people results.

### Checked and fine
PickTownBodies (every 0.25 s; it reuses TownNear's capacity). TownPool and TownBodies are GC-safe: TownBodies is a
UPROPERTY holding them all, and pooled bodies are checked with IsValid. Hidden pooled bodies do not animate
(`OnlyTickPoseWhenRendered`, no actor tick). Pool key: `Body->MeshName == ShownModel`. TownWay uses Dijkstra over 68 stairs,
which is about 136^2 per route and fine. LyudText walks the subfolders only when a language's table first loads.
FLightSink holds a TWeakObjectPtr.

### Conflicts with perfhunt
- PeopleLife.cpp Present: perfhunt turns `TSet<FString> PlacedShown` into a member keyed by actor, adds ClubSlotActors,
  and adds GroundCache (cached ground traces). town-life edits the same lines (TownPick, bOut/ReleaseBody just above
  the trace). Textual conflict, easy to resolve: keep perfhunt's GroundCache. With town-life the trace cache matters more
  (up to 48 townsfolk traced every frame otherwise).
- PeopleLife.h private section: both add members, likely a conflict. Stop(): both add Reset lines near ClubActors.
- LyudText.cpp: perfhunt's new namespace block (Language atomics, Missing set) starts right after the table loader that
  town-life rewrites (lines 78-104), so an adjacent-hunk conflict is likely. The two changes are independent, so keep both.

---

## public-spaces (2 commits: the Academ fountain's water + sound; spots are data only)

Files: Private/World/LyudFountain.cpp/.h, Private/Audio/FountainDSP.*, LyudFountainSynth.*

### P1 S3: first autumn check scans the weather hour by hour synchronously. Confidence: med
- LyudFountain.cpp:430 `for (; ScannedTo < End; ScannedTo += 1.0) LyudWeather::Instant(...)` runs from 1 Sep to 1 Dec
  (about 2180 Instant evaluations, each a blended synoptic chain) in one tick, the first time InSeason runs after
  1 September (Tick every 2 s), and again on each new year or game load in autumn. This is a one-off hitch of a few to
  tens of ms. It also walks the shared chain far into the future (the chain may extend its fronts).
- Fix: scan at most, say, 48 hours per call (it is already incremental through ScannedTo: just cap the loop and leave
  `FrostHours < 0` until it is done), or scan by days using daily minima.

### P2 S3: drained fountain keeps the synth running while the player is near. Confidence: high
- LyudFountain.cpp:502: Stop is only called when `Dist > HearCm + 500`. If the fountain drains, or the actor is hidden
  by lyud.Academ 0, while the camera is near, the USynthComponent keeps rendering (Flow 0, so Render does a per-sample
  `SFlow.Step` loop). It costs little, but it is an audio-thread voice for nothing all winter while the player stays at
  the square.
- Fix: `else if (!bWant && bSoundPlaying && (!bRunning || IsHidden() || Dist > HearCm + 500.f))`, or stop once Rms ~ 0.

### Checked and fine
Meshes are built once in Build (OnWorldBeginPlay), never in play. All parts set bAffectDynamicIndirectLighting and
bAffectDistanceFieldLighting false and cast no shadows. Tick runs every 0.25 s and SetVisibility is called only on a
state change. MIDs and components are UPROPERTY. Synth parameters are atomics, and the DSP has fixed-size bubble and
tick arrays (no allocation on the audio thread). lyud.Academ 0 disables its tick (perfhunt's AcademToggle keeps that,
and its TickingBeforeHide set also covers it).

---

## city-terrain (C++: ALyudCityWindows + ULyudCityWindowsSubsystem; the rest is tools/data)

### C1 S2/S3: every change rebuilds all four pane ISMs (ClearInstances + AddInstances). Confidence: high
- LyudCityWindows.cpp:226-246 RebuildInstances: on any lit/unlit change, a temporary `TArray<TArray<FTransform>>` is
  built, then `ClearInstances()` + `AddInstances()` run on all 4 ISMs (1598 windows in all). Under the schedule this
  happens at most every 2 s, and only when a window changed. Upper windows re-roll each game hour (ScheduleLit:
  `Day*7 + uint32(H)`), so many change at once. Once the town life drives it [X-BRANCH], UpdateLights sends changed
  flats every 2 s all evening, so this becomes a full instance-buffer and proxy rebuild about every 2 s.
- Fix: one instance per window, made once in Build (per tint ISM, remember each window's instance index). Mark lit or
  unlit with per-instance custom data (`NumCustomDataFloats = 1`, material multiplies emissive), or with a
  zero-scale transform. Update only the changed instances with `SetCustomDataValue(..., false)` /
  `BatchUpdateInstancesTransforms(..., bMarkRenderStateDirty=false)`, then mark render state dirty once per batch.

### C2 S3: ScheduleLit allocates an FString per window per check. Confidence: high
- LyudCityWindows.cpp:189 `HashName(W.Id.ToString())` for every window that is dark and awake, every 2 s (up to
  ~1600 FString allocations and hashes). Fix: store the window's own hash in FWindow at Build (it is already computed
  there for the tint, line ~112).

### C3 S3 (plain bug): once anyone drives a window, every undriven window goes dark for good. Confidence: high [X-BRANCH]
- LyudCityWindows.cpp:207 `W.External >= 0 ? ... : (!bExternal && ScheduleLit(...))`. The first SetWindowLit sets
  `bExternal = true` for the whole actor. The town life only knows the dwellings W2..W5 (town_places.json has windows
  for flats only), so the shop windows of the promenade units (W1-U*, bShop) never light again after the life starts.
  The same goes for any wall flat missing from town_places.
- Fix: keep the schedule for windows that were never set (`External < 0`). Drop the global gate, or apply it only to
  `!bShop` windows that belong to a flat the driver knows.

### C4 S3: Build reads and parses city_plan.json (914 KB) on the game thread at OnWorldBeginPlay. Confidence: high
- LyudCityWindows.cpp:68-76. This is a one-off hitch at map start. It is fine for now. If the plan grows, bake the
  windows into a small json or asset.

### Checked and fine
Panes cast no shadows, have no collision, and are out of Lumen and the distance fields. The actor ticks every 0.5 s with
a 2 s check. Panes and MIDs are UPROPERTY. The subsystem holds a weak ptr. ALyudSky is found with a TActorIterator every
2 s: cheap, but perfhunt replaced the same pattern in AcademToggle with a cache (see B3).

---

## wt/city-buildings (C++: ALyudCityBuilding + ULyudWindowLights)

### B1 S3: the seeded evening builds 1-2 FStrings and 5 string hashes per flat window every second. Confidence: high
- LyudCityBuilding.cpp:451 `const FString Full = Plot + "/" + Flat;` + `Hash01(B->WindowIds[S], 5)` + SeededFlat's
  Hash01 x4 (each `GetTypeHash(FString)`), for every flat window of every building, every 1 s (UpdateAll from Tick). With
  ~1300 flats and several windows each, that is thousands of allocations and hashes a second, whenever the life sim
  is not driving (lyud.People 0, other maps, before the bind).
- Fix: at Rebuild/Register, precompute per slot `RoomSeed`, `FlatHome/Bed/Early/Up` floats (the hashes do not change),
  and evaluate only floats in UpdateAll. Better still, evaluate only on an hour-bucket or day/night change
  (`LastHourBucket` is declared but unused).

### B2 S3: SetFlatLit flushes the texture per call. Confidence: high [X-BRANCH]
- LyudCityBuilding.cpp:313 and :332: `B->Flush()` inside SetFlatLit/SetWindowLit is one UpdateTextureRegions per flat (see T5).
- Fix: SetSlot only, add B to a `TSet<TWeakObjectPtr<>> DirtyBuildings`, and flush them in Tick. The 1 s accumulator
  still lets a change through within a second.

### B3 S3: UpdateAll finds the sky with TActorIterator<ALyudSky> every second. Confidence: high
- LyudCityBuilding.cpp:405. This is the pattern perfhunt replaced in AcademToggle (LampCacheFor: cached weak Sky).
- Fix: cache a `TWeakObjectPtr<ALyudSky>` and re-find it only when it is invalid.

### B4 S3 (plain bug): driven flats are never handed back, and console overrides stick. Confidence: med [X-BRANCH]
- LyudCityBuilding.cpp:448: when `bDriven`, flat windows are skipped. Nothing calls `SetDriven(false)`: town-life's
  Stop() (lyud.People 0) resets its sinks but leaves the buildings driven, so their flats freeze as last set. Also,
  after `lyud.City.Lights 1`/`2` goes back to 0 while driven, every flat keeps the forced value until the life changes
  that flat.
- Fix: town-life Stop() should call SetDriven(false) through the sink (or the subsystem should time out when no
  SetFlatLit arrives). On a Mode change back to 0 while driven, re-request all flats (town-life: `UpdateLights(true)`), or
  keep the driven value per slot and restore it.

### B5 S3 note: up to 96 movable point lights per building
- LyudCityBuilding.cpp:105-122. They are made once at BeginPlay, have no shadows, MaxDrawDistance 45 m, and are toggled
  only on change. That is fine, but with ~20 buildings there can be ~1900 light components. While visible and within
  45 m they all go through Lumen direct lighting. Consider `bAffectTranslucentLighting=false` and a lower cap for
  non-enterable floors.

### Checked and fine
No per-frame work (the tickable runs every 1 s, and texels upload only when changed). LightTex, LampComps and the MID are
UPROPERTY. Buildings are weak ptrs, and EndPlay unregisters. The Flush copy lives until the render command frees it. The CVarCity
lambda walks actors only on a cvar change. Flat ids from town_places ("P-STORE/0-01") match the FlatIndex keys
("<plot>/<flat>").

---

## Summary per branch
- town-life: T1 S2 pooled hidden bodies stay door agents and hold town doors open (PeopleLife.cpp:883/1175);
  T2 S2 TalkPairs O(talkers x 900) (PeopleSim.cpp:2889); T3 S3 FindActivity("sleep") per person per step (PeopleSim.cpp:1621/1627);
  T4 S3 O(N^2) FindPlace/AssignHomes at start; T5 S3 jump/bind sends 1734 flats to both sinks, one texture flush each;
  conflicts with perfhunt in Present/PeopleLife.h/LyudText.cpp (keep perfhunt's GroundCache).
- public-spaces: clean overall; P1 S3 2180-hour frost scan in one tick (LyudFountain.cpp:430); P2 S3 synth not
  stopped when drained near the player (:502).
- city-terrain: C1 S2/S3 ClearInstances+AddInstances on all 4 ISMs per change (LyudCityWindows.cpp:226), every ~2 s
  once driven; C2 FString per window per check (:189); C3 bug: undriven shop windows go dark forever once driven (:207).
- wt/city-buildings: B1 FString/hash per flat window per second in the seeded mode (LyudCityBuilding.cpp:451); B2 Flush
  per SetFlatLit (:313); B3 TActorIterator sky each second (:405); B4 SetDriven never reset.
