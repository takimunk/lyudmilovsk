# Branch review: fest (review/restaurant, festivals)

Read-only review of `git diff $(merge-base origin/main origin/B) origin/B -- unreal/Lyudmilovsk/Source`.
Line numbers are on the branch (`git show origin/B:<path>`). Paths below are relative to
`unreal/Lyudmilovsk/Source/Lyudmilovsk/`.

Merge with perfhunt: `git merge-tree --write-tree origin/perfhunt origin/B` is clean for both branches (rc 0).

---

## review/restaurant (95098cef, 3be0ba5b)

New: Private/Restaurant/{LyudRestaurant.cpp, LyudRestaurantWaiter.cpp, LyudRestaurantStaff.cpp/.h, SLyudRestMenu.cpp/.h},
Public/Restaurant/LyudRestaurant.h; touches Food/LyudBite, HeroAnimInstance (Table pose), HeroCharacter (StandOffset), LyudInventory (rs_* goods).

ALyudRestaurant ticks every frame everywhere on the map (no distance gate); TickWaiter/TickHero/TickKnife run per frame,
UpdateState 1 Hz, TickProps 0.5 Hz.

### R1 S2 (high): restaurant.json read from disk and JSON-parsed every frame while open
- Private/Restaurant/LyudRestaurantWaiter.cpp:37-53 `StandinMesh()` does `FFileHelper::LoadFileToString` + `FJsonSerializer::Deserialize`.
- Called from `EnsureWaiter()` (:219-222) whenever no town-life staff gives a mesh, and `EnsureWaiter()` is called
  from `TickWaiter()` (:451) every frame while `bOpen || Job != None`.
- On main/this branch `UPeopleLifeSubsystem` has no `StaffOnShift`, so the restaurant opens by hours 11:00-23:00 (restaurant.json
  "hours") and takes this path every frame, for half of every game day, wherever the hero is.
- Fix: read the mesh name once. `LyudRest::Config()` already caches restaurant.json (LyudRestaurant.cpp:119); expose it or
  give LyudRestWaiter a `static const FString Mesh = ...`. Better: don't call EnsureWaiter per frame at all (see R2).

### R2 S2 (high, grows with town-life): EnsureWaiter staff lookup and SetPresentedExternally every frame
- LyudRestaurantWaiter.cpp:196-236. Each frame: `LyudRestaurantStaff::OnShift` (FindFunction, TFieldIterator over params,
  alloca, TArray copy-out, plus whatever StaffOnShift costs on town-life), `People->GetPerson` (8+ FString/FName builds and a
  Needs TMap, see PeopleLife.cpp GetPerson), `P.Mesh.ToString()`, and `SetPresentedExternally(Person, true)` through reflection
  every frame (it may also do work on the life side every call).
- Fix: resolve the waiter at UpdateState's 1 Hz (or when the shift changes) and cache `WaiterPerson`/`WaiterMesh`; call
  `SetPresentedExternally` only on change. TickWaiter then just checks `WaiterBody` is valid.

### R3 S2 (med): the waiter's body is driven and animated all day regardless of distance
- LyudRestaurantWaiter.cpp:360-437, 717: `PoseWaiter` every frame: `WaiterBody->Drive(...)`, `Tray->SetActorTransform`,
  `SetActorHiddenInGame`, IK pose build, every frame from 11:00 to 23:00 even when the hero is km away. The comment at :455
  ("a waiter far from the camera needs no body work every frame") is not implemented.
- Fix: when the hero is outside (say) 60 m or `!WaiterBody->WasRecentlyRendered(0.5f)`, skip PoseWaiter (just keep WaiterPos
  logic), and/or have the whole restaurant actor tick at 0.25 s when the hero is far (SetActorTickInterval by distance).
  Also don't move the hidden tray (only SetActorTransform when bCarry).

### R4 S1 (med): GC can free the bite's load meshes mid-meal -> dangling UStaticMesh*
- Public/Food/LyudBite.h:57-65: `FLyudMeal` holds raw `UStaticMesh* LoadMesh, LoadMeshBig, UtensilMesh` and
  `UMaterialInterface* LoadMaterial`, inside a non-UPROPERTY member `ULyudBiteSubsystem::Meal` (LyudBite.h:129).
- The restaurant fills them from `ALyudRestaurant::RestMesh()` = bare `LoadObject` with no other strong ref
  (LyudRestaurant.cpp:453, 462, 475, 477-478). Only the mesh currently set on `LoadC`/`UtensilMeshC` is referenced.
  LyudBite.cpp:260-263 swaps `LoadC` between `LoadMeshBig` and `LoadMesh`, so the other one is unreferenced; a GC during a
  long meal (gc runs ~every 60 s) frees it and the next swap sets a dangling pointer -> crash.
  The pattern existed for the diner, but the restaurant is the first caller passing assets nothing else holds.
- Fix (covers every caller): in `ULyudBiteSubsystem` add `UPROPERTY(Transient) TArray<TObjectPtr<UObject>> MealRefs;` and in
  `BeginMeal` fill it with LoadMesh/LoadMeshBig/UtensilMesh/LoadMaterial (Reset on Finish). Or keep a UPROPERTY mesh cache on
  ALyudRestaurant (also fixes R5).

### R5 S2/S3 (high): RestMesh does DoesPackageExist + sync LoadObject on every call, no cache
- LyudRestaurant.cpp:244-253. `ALyudRestDish::Init` (:327-363) calls it ~5 (stages) + vessel + _Glass + saucer + fill = ~9
  times per dish spawned; `StartEating` (:453-478) 3 more per meal; SpawnMenuCards/Knife/Tray/Bill too.
  `FPackageName::DoesPackageExist` is a file-system lookup, the first `LoadObject` a sync load.
- Dishes are spawned in play: by the waiter (Waiter.cpp:517), and by TickProps for every townsperson eating there (R6),
  even when the hero is far away. Also `LyudRest::Sound` (:125-136) and `LyudRestWaiter::SoundPlate` (Waiter.cpp:67-77) probe
  1-3 packages per call (each set-down plays it).
  Note: the Content/Restaurant meshes are not tracked on the branch yet, so today every probe misses.
- Fix: a `static TMap<FName, TWeakObjectPtr<UStaticMesh>>` (store misses too), plus a
  `UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> MeshCache` on ALyudRestaurant filled once in BeginPlay for menu()
  dishes (keeps them alive, fixes R4 for the restaurant). Cache the two sounds in UPROPERTYs at BeginPlay.

### R6 S3 now / S2 with town-life (high): TickProps builds a full FPersonInfo for every person every 2 s
- LyudRestaurant.cpp:1535-1609: loops `0..NumPeople`, `GetPerson(Id, P)` (all FString/FName conversions + Needs map) and
  `P.Activity.StartsWith("eat")`, every 2 s, wherever the hero is. 33 people on main (cheap); hundreds on town-life.
  Also allocates a TSet and (per new prop) a `Mains` array each time.
- This is the pattern perfhunt removed from CounterTrade (38ca628d): perfhunt adds
  `UPeopleLifeSubsystem::FindActivity` and `GetActivityState(Id, Activity, bDoing, Pose, Location)` (PeopleLife.h on perfhunt).
  After merge: precompute the eat_* activity indices once, use GetActivityState and a location test first.
  `bDoing` covers "doing" only, not "settling": either accept that or extend the accessor.
- Also gate it: if the hero is more than ~80 m from the hall, destroy props and skip the scan.

### R7 S3 (high, bug): prompt is keyed only by its string key -> stale dish name / bill amount
- LyudRestaurant.cpp:969-980 `SetPrompt` returns early when `Key == Prompted`, ignoring Args. Looking from the borscht to the
  ramen keeps "eat Borscht" (:1447, same key ui.rest.eat_prompt); the bill prompt (:1464) keeps the old tab.
- Fix: compare the formatted text (or Key + joined args), e.g. keep `FText LastPromptText` and compare with `EqualTo`.

### R8 S3 (med, crash on test/console path): order indices go stale when the table is reset with dishes on the tray
- Waiter keeps indices into `Orders` (`OnTrayOrders`, Waiter.cpp:335-341), then reads `Orders[OrderIx]` without bounds checks
  (:591, :623). `ResetHeroTable` (LyudRestaurant.cpp:1252) does `Orders.RemoveAll(non-Carried)`, shifting the indices.
  Reachable when SitHero changes tables while dishes are carried: the fallback loop at :1070-1076 (and `lyud.Restaurant.Sit <seat>`)
  ignores the "same table while there are orders" rule that SeatNearHero applies (:1008). Result: wrong order marked Served or OOB.
- Fix: store a stable order id (int32 counter) instead of an index, or keep Carried orders in their own array;
  `Orders.IsValidIndex(OrderIx)` before use; apply the table rule in SitHero itself.

### R9 S3 (med, bug): waiter can get stuck forever with the bill
- Waiter.cpp:561-581: the Bill SetDown branch only finishes if `BillFolder && HeroTableIx >= 0`, else it `break`s and stays in
  SetDown with `Job == Bill`. `ResetHeroTable` (LyudRestaurant.cpp:1253-1261; triggered by TickHero :1383-1391 after 90 s away,
  allowed because PendingCount() is 0 once everything is served) destroys BillFolder and clears HeroTableIx.
  A stuck waiter keeps `bNeeded` true: the waiter never goes off shift and the doors never lock (UpdateState :881).
- Fix: in SetDown/ToTable, if `HeroTableIx < 0 || !BillFolder`, drop the job (`Job = None; Waiter = ToHome; PlanPath(WaiterHome)`);
  or don't reset the table while `Job != None`.

### R10 S3 (low): per-frame JSON field lookups and string work in TickHero
- LyudRestaurant.cpp:1387 `LyudRest::Number(Config(), TEXT("table_reset_s"))` every frame while away from the table
  (TryGetNumberField builds an FString key); SitHero :1108 and StartJob similar but rare.
  :1447 builds `ItemName(...).ToString()` and an args array every frame while a dish is looked at.
- Fix: read table_reset_s/bread_after_s/clear_after_s into members in BeginPlay; build prompt args only when the looked-at dish changes.

### R11 S3 (low): EndPlay leaves spawned actors and the hero's Table pose behind
- LyudRestaurant.cpp:597-613 destroys menu cards and the waiter, but not HeroDishes, Props dishes, Knife, BillFolder, and does not
  clear `UHeroAnimInstance::Table` or end an active meal. Harmless at level end; it leaks if the actor is removed in play
  (lyud.Restaurant A/B only hides). Fix: destroy them and reset `Anim->Table` in EndPlay.

Checked, no issue: dish components out of Lumen (:308-311), knife/tray/bill out of Lumen; dish Tick 0.5 s, self-disabling;
UPROPERTY on all spawned actor refs; IsFestival's reflection matches festivals' `UFUNCTION bool IsFestivalToday() const`
(NumParms 1, ParmsSize 1); the menu widget's lambdas capture `this` (normal Slate); UpdateState's MID sets are no-ops when
unchanged; Dijkstra over a few dozen nodes, only on job changes.

Perfhunt overlap (restaurant):
- TickProps uses `GetPerson` in a loop: use perfhunt's `GetActivityState`/`FindActivity` after merge (R6).
- `SignLanguage != LyudText::Language()` (:898): perfhunt changed Language() to return `const FString&` (cached). Compatible.
- HeroCharacter.h/.cpp, HeroAnimInstance.cpp, LyudInventory.cpp are edited by both, in different hunks; merge-tree is clean.
  HeroAnimInstance `Table` pose is an FBikeRiderPose copied in PreUpdate like Bike/Ladder: no extra cost.
- No FindComponentByClass in the new code (perfhunt's FindReach/FindBike not needed).

---

## festivals (059c0068 .. d44615a0)

New: Private/Festival/FestivalCalendar.cpp/.h (plain C++), Private/Festival/LyudFestival.cpp, Public/Festival/LyudFestival.h.
ULyudFestivalSubsystem: TickHero every frame while a festival is active (the weekly market counts), Update every 2 s.

### F1 S2 (high, latent: worse once the city plan lands): unresolved venues re-read and re-parse the plan JSONs every 2 s
- LyudFestival.cpp:1199-1246 `FindVenue` caches only valid results (:1243 "looked for again next time"). A miss calls
  `ResolveFromPlan("City/city_plan.json")` + `ResolveFromPlan("PeopleData/town_plan.json")` (:1074-1082, full file load + JSON parse
  each) + a scan of all people places.
- Update calls FindVenue for every decor venue and phase venue each 2 s (:437, :472) and for shows (:651). Any venue kind absent from
  the plans and the people's map (sunrise_point, pier, park, ... depends on data) is retried all festival day. A hit also re-parses
  town_plan.json once more for stalls (:1219).
- Today neither city_plan.json nor town_plan.json is in Content (the load fails fast); once they exist (other threads), each miss is
  two multi-KB/MB parses every 2 s.
- Fix: parse each plan once per session (a member `TArray<TSharedPtr<FJsonObject>> PlanSpots` per file); cache misses with a
  retry time (e.g. `TMap<FName, double> VenueRetryAt`, retry every 60 s or when PeopleLife starts).

### F2 S2/S3 (high): firework sounds are sync-loaded by path per shell, and the assets don't exist on the branch
- LyudFestival.cpp:2045 (`S_firework_launch_01` every shell) and :2120-2121 (`FString::Printf` + `LoadObject` per boom).
  No /Game/Audio/Festival/* uassets are tracked on origin/festivals (and none in Content/Audio of the main checkout), so each call
  is a failing LoadObject: package-path resolve plus a disk probe. That happens every 1-4 s, and every 0.2-0.5 s in the 45 s finale.
  With the assets present, the first call is a sync load during the show.
- Fix: load the launch/boom/bonfire sounds once in `Setup` into `UPROPERTY(Transient) TObjectPtr<USoundBase>` members (null if
  absent, no retry). Same for `MU_festival_*` (:831, once per phase: fine) and the bonfire loop (:2338, fine).

### F3 S3 (high): per-frame string building in TickHero all festival day
- LyudFestival.cpp:952 `DateKey(...)` Printf every frame; :962-966 two `HeroEvent` calls per frame while in a venue, each builds
  `Day|event|target` (several concatenations, Event.ToString()) and hashes it into `TSet<FString> HeroEvents`; :991-1001 once a
  show has been watched 30 s, every frame does 2 `Split`s + 2 more HeroEvent string builds per show; :1007 formats the lantern prompt
  `LyudText::Get(..., {Price})` every frame even when unchanged. Roughly 15-30 heap allocs/frame near a festival, a few anywhere.
- Fix: cache the day key when the date changes; keep a `TSet<FName>` (or per-phase bool) of events already sent and skip before
  building strings; stop accumulating `Watched` after it passes 30 s; build the prompt FText only when `bCan` turns on.
  `HeroEvents` also never shrinks across days (tiny, but reset it on day change).

### F4 S3 (high, bug): money taken for a sky lantern that is never released
- LyudFestival.cpp:1013-1017: `Inv->Spend(Price)` first, then `Lanterns->Release(...)` returns false when all 70 lanterns are in
  the air (:2188-2204). The 30 rubles are lost with no lantern and no message.
- Fix: check for a free slot first (add `bool HasFree() const`), or give the money back when Release fails.
  Also `HeroLanternsTonight` (:948) is reset only when no festival is active, so on multi-day festivals or with the weekly
  market running it is not per night; reset it on day change.

### F5 S3 (med): 70 sky lanterns each get 2 transform updates + SetVisibility every frame
- LyudFestival.cpp:2248-2250: `SetWorldLocation` then `SetWorldRotation` (two UpdateComponentToWorld + render transform
  updates per lantern) for up to 70 components, for the whole lantern phase (lanterns live up to 1100 s).
- Fix: one `SetWorldLocationAndRotation`; drop the per-frame `SetVisibility(true)` (set in Release/spawn only). Optionally one
  ISM with `BatchUpdateInstancesTransforms(..., bMarkRenderStateDirty=false)` + one final dirty, or tick far lanterns at 10 Hz.
  Lumen flags are already off (`Lightweight`).

### F6 S3 (low confidence): three 600 m point lights stay registered at 0 intensity for the whole fireworks show
- LyudFestival.cpp:2008-2011 (`MakeLight(..., 60000.f)`), :2128-2136 `SetIntensity` each frame, 0 between bursts. A visible light
  with zero intensity may still go through light culling / the light grid over the whole town.
- Fix: `SetVisibility(Flash[L] > 0.f)` (only on change), and skip SetIntensity when the value did not change.

### F7 S3 (high): StubGather rebuilds strings for every person and place every 2 s per running phase
- LyudFestival.cpp:692-738: every Update, per phase with a valid venue, when the life has no SetGathering (true on main):
  UTF8->FString for every place kind (:695) and its id (:698), a sort, then for every person `Key.ToString() + "|" + key` (:711) and a
  TSet<FString> lookup. 33 people now; scales with town-life.
- Fix: cache `Near` per (phase key) since venues don't move; mark asked people in a `TSet<int32>` or bit array per phase instead of
  FString marks; precompute the activity's bFree per index.

### F8 S3 (med): decorations build meshes and cook collision in play
- LyudFestival.cpp:1863-1893 `Make`: `FLyudMeshKit::Build` creates a UStaticMesh at runtime, with BlockAll collision for the solid
  kit (the stage, stalls, tree, poles), when the festival's decor time comes (08:00 game time) for each venue. That is a one-off
  hitch of probably several ms per venue. Fireworks/lanterns/bonfire/meteors also build small meshes in Setup (fine).
- Fix: acceptable once a day; if it shows in bench, build decor the night before off-screen or spread venues over frames (one per
  Update), and use simple box collision (`CollideBox`) instead of complex collision for the decor.

Checked, no issue: all spawned actors/components/MIDs in UPROPERTY maps/arrays; Deinitialize destroys decor and shows; the save object
is held weakly by ULyudSaveSubsystem (TWeakObjectPtr); FCalendar is plain C++ via TPimplPtr; Update's temporary maps every 2 s are
fine; decor stage lights tick only during "stage_lights" at 4 Hz; SetLights only on a >0.02 change; movers use `Lightweight`
(no shadows, out of Lumen/DF, no ray tracing); `Announced` is capped at 120; StartShow's TActorIterator<ALyudSea> runs once per show.

Perfhunt overlap (festivals): no shared files; merge-tree clean. It follows perfhunt's movers-out-of-Lumen rule. StubGather
reads `People->GetSim()` directly (no GetPerson), which is fine. Restaurant <-> festivals: the restaurant's reflection call to
`IsFestivalToday` matches; the two features don't duplicate each other.
