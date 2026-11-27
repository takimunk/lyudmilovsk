# Tick inventory, 2026-11-27

Every class in `unreal/Lyudmilovsk/Source/Lyudmilovsk` that ticks: actors (`PrimaryActorTick.bCanEverTick`), components
(`PrimaryComponentTick.bCanEverTick`), world/game-instance subsystems (`UTickableWorldSubsystem` / `FTickableGameObject`) and
`FTSTicker` tickers. Found by script (a scan of every `::Tick(float` / `::TickComponent(float` body for distance / season /
hour / hidden / timer gates, plus a count of the `SpawnActor` / `NewObject` sites and the loops round them), then each
body read by hand. Slate widgets' `Tick` (SAtonDesktop, SLyudHud, SLyudDiary, SLyudBook, inventory) are left out: they only
tick while drawn.

Instances are estimates from code (the `.umap` is LFS, not read): "1" = a singleton spawned once; "placed" = put in the
level by hand (a few); "N per X" = spawned in a loop.

Areas marked **(other agent)** were being edited in parallel tonight (People, Birds, Murmuration, Rounds, Bikes): listed,
not touched.

Verdict: **ok** = gated or cheap enough; **fixed** = changed on branch `perf1127/ticks`; **done** = gated earlier tonight on
perf-1127; **note** = a cost worth knowing, left alone (reason given).

## Actors and components

| class | file | instances | interval | gate | per-tick work | verdict |
|---|---|---|---|---|---|---|
| AProceduralPlant | Plants/ProceduralPlant.cpp | hundreds (forests, streets, gardens, stations, yards, rail) | dynamic: 0 while leaves/fruit fall or fruit near, else UpdateInterval; pinned herbs >= 5 s | distance (summary bake far), frame budget for re-shapes | falling leaves, fruit prompt, re-shape when due | ok |
| AProceduralVine | Plants/ProceduralVine.cpp | ~10 (VinePlantings table) | **was 0 for good** -> 0.25 s caught up, 0.5 s hidden, 0 while tracing/time-lapse/dev age | cvar hide | cvar, transform compare, growth target; re-shape <= 2 Hz | **fixed** |
| AProceduralGrass | Plants/ProceduralGrass.cpp | 1 per area | set | season | patch rebuild near camera | ok |
| AProceduralForest | Plants/ProceduralForest.cpp | ~5 (parks, yards, fluff) | set | done-flag early out | layout over first seconds | ok |
| AForestFloor | Plants/ForestFloor.cpp | 1 | 0 | distance, season, hidden | litter cells near camera | ok (plants agent) |
| AFallenFruit | Plants/FallenFruit.cpp | dozens in autumn | 0 | tick enabled only while rotting (3 s) | scale/sink | ok |
| APlantDevTool | Plants/PlantDevTool.cpp | dev only | 0 | disabled unless used | dev UI | ok |
| ALyudStationHall | Transport/LyudStationHall.cpp | 7 (one per station) | 0 near (people on), 0.25-0.4 s far | distance | lights, people, barks | ok |
| ALyudStationLift | Transport/LyudStationBuild.cpp | <= 7 (tall stations) | **was 0 always** -> 0.25 s idle with nobody within 6 m | early out idle+far (after its per-frame work) | pawn inverse transform, stop search, door interp | **fixed** |
| ALyudTrain | Railway/LyudTrain.cpp | 1-2 | 0 | none | StepTrain (moves visibly), seat prompt | ok (visible motion) |
| ALyudRailTrack | Railway/LyudRailTrack.cpp | 1 | 0.5 s | lamp change threshold | halts' lamps by sun | ok |
| rail halt / level-crossing sign UWidgetComponents | Railway/LyudRailTrack.cpp, LyudLevelCrossing.cpp | ~6-10 | **was 0** -> 1 s | redraw time 5 s | widget component tick (redraw check) | **fixed** |
| ALyudDoorBase (+ALyudDoor) | Doors/LyudDoor.cpp | 1 per door (stations, houses: dozens) | 0 | tick enabled only while swinging / waiting to close | openness interp | ok |
| ACurtainPusher | CurtainPusher.cpp | placed (club/rooms) | 0 | 5 Hz sweep of characters in range | push strips via MPC | done |
| ALyudDiner | Diner/LyudDiner.cpp | 1 | **was 0** -> 0.2 s when quiet and hero 25 m+ from the urn | none | coffee-urn prompt test, dish scan, pending orders | **fixed** |
| ALyudDinerDish | Diner/LyudDiner.cpp | 0-8 while eating | 0 | early outs (empty, far) | prompt / bite | ok (short-lived) |
| ALyudRestaurant | Restaurant/LyudRestaurant.cpp | 1 | 0 near / 0.5 s quiet far | distance + idle | staff, guests, kitchen | ok (done) |
| ALyudRestDish | Restaurant/LyudRestaurant.cpp | few | 0 | tick toggled | eat | ok |
| AVendingMachine | VendingMachine.cpp | placed (a few) | 0 | idle + far early out first | dispense anim | ok |
| ALightFlicker | LightFlicker.cpp | placed | 0 | far: bulbs held, return | light dips | ok |
| AConveyorBelt | ConveyorBelt.cpp | placed | 0 | idle + no overlaps early out | riders | ok |
| ABoothOwner | BoothOwner.cpp | placed | 0 | away early out | NPC state machine | ok |
| AHeroSeat | HeroSeat.cpp | placed | 0 | none | box test of the pawn, prompt | note: trivial |
| AAtonComputer | AtonComputer.cpp | 1 | 0.1 s | active early out | sequence hand-off look | ok |
| ALyudSea | Sea/LyudSea.cpp | 1 | 0 | cvar | surface follows camera, MPC, depth map steps | ok (camera-attached) |
| ULyudSeabedComponent | Sea/LyudSeabed.cpp | 1 | 0 | distance, hidden | weed near camera | ok |
| ULyudBuoyancyComponent | Sea/LyudSea.cpp | per floating body (dev crate, boats) | 0 | not simulating -> return | 8 sea samples + forces | ok (physics) |
| ULyudSwimComponent | Sea/LyudSwim.cpp | 1 (hero) | 0 | not swimming early out | swim | ok |
| ALyudRowBoat | Fishing/LyudRowBoat.cpp | 1-2 | 0 | distance | bob / row | ok (visible) |
| AAnimalActor | Animals/AnimalActor.cpp | bodies near camera only (pooled) | 0 | renderer hides/disables spares | anim overlay (look, jaw, tail) | ok (visible) |
| UAnimalCollarComponent | Animals/AnimalCollar.cpp | per pet body | 0 | distance, rendered | tag swing | ok |
| AAnimalNameTags | Animals/AnimalNameTags.cpp | 1 | 0 | bodies only | loop over sim animals (cheap skip without body) | ok |
| ANpcTalkDirector | NpcDialogue.cpp | 1 | 0 | busy -> close | best talk target (traces), labels | note: a line trace per candidate in range; fine at current counts |
| AHeroCharacter, UHeroReachComponent, UHeroShadowMesh, ULyudLadderClimber | Hero*, World/LyudLadder.cpp | 1 each | 0 | - | the player | ok |
| ALyudSky | Sky/LyudSky.cpp | 1 | 0 | - | sun/moon/sky | ok (visible) |
| ALyudRareSky | Sky/LyudRareSky.cpp | 1 | 0 | - | aurora/halo model, sky MID params, optics dome follows camera | note: model cheap, dome is camera-attached |
| ALyudRainFx | Weather/LyudRainFog.cpp | 1 | 0 | - | follows camera | ok |
| ALyudCityWindows | World/LyudCityWindows.cpp | 1 | 0.5 s | - | window lights | ok |
| ALyudFountain | World/LyudFountain.cpp | ~3 | 0.25 s | season | water | ok |
| ALyudPublicSpaces | City/LyudPublicSpaces.cpp | 1 | 0.25 s | season, hidden | spaces | ok |
| ALyudFairRides | City/LyudPublicSpaces.cpp | 1 | 0 moving+near / 0.5 s | distance | rides | ok |
| ALyudLighthouse | South/LyudLighthouse.cpp | 1 | 0 lit / 1 s | lamp hours | beam | ok |
| ALyudLighthouseStair, ALyudLighthouseLog, ALyudSouthSpot, ALyudSouthUse | South/*.cpp | ~10 | 0 | tick enabled only while hero near (sphere) | prompts | ok |
| ALyudCampfire | North/LyudCampfire.cpp | placed (north) | 0 near / 0.05 s | schedule timer | fire | ok |
| ALyudFumarole | North/LyudFumarole.cpp | placed (north valley) | 0.04 s | north subsystem wakes/sleeps | steam emit | ok |
| ALyudNorthSteam, ALyudNorthPlume, ALyudNorthRiver, ALyudNorthSpring | North/*.cpp | 1-4 each | 0-0.5 s | hidden -> tick off / bNear | steam particles, plume instances, loop | ok |
| ALyudFestivalDecor | Festival/LyudFestival.cpp | 1 per festival site | 0.25 s | tick on only during a party | stage lights | ok |
| ALyudFireworks, ALyudSkyLanterns, ALyudMeteors, ALyudBonfire, ALyudFirecrackers, ALyudColorPowder | Festival/LyudFestival.cpp | during shows only | 0 / 0.05 s | destroyed after the show | show anims | ok (short-lived) |
| ALyudKites (festival) | Festival/LyudFestivalTown.cpp | 1 during festival | set | distance | kites | ok |
| ALyudAirlock | Europa/LyudEuropa.cpp | 1 | 0 | - | two lids, water level, underwater blend | note: a few compares a frame |
| ALyudEuropaSite | Europa/LyudEuropa.cpp | 1 | 0 | tick disabled while not shown (SetShown) | ripples clock, sun at 2 Hz | ok |
| ALyudTopi, ULyudTopiMind, ULyudTopiProps | Topi/*.cpp | 1 | 0 / retry interval | hidden, distance | the robot | ok |
| ASeedHandful, ALighterItem, ALyudSmokeFx, ALyudDiaryBook | misc | 0-1 | 0 | toggled | short-lived props | ok |
| ABirdActor, ABirdPawn | Birds/*.cpp | **(other agent)** | | | | not edited |
| AStarlingMurmuration | StarlingMurmuration.cpp | **(other agent)** | accum | | | not edited |

## Subsystems and tickers

| class | file | instances | interval | gate | per-tick work | verdict |
|---|---|---|---|---|---|---|
| ULyudTransportSubsystem | Transport/LyudTransport.cpp | 1 | frame | - | sim step, 12 cabins driven, hum/lamp/widget LOD, boards, ropes | ok (cabins move visibly) |
| ULyudWinterSubsystem | Winter/LyudWinter.cpp | 1 | 4 Hz when nothing wintry shows | season/thaw | MPC, flakes, snow | done |
| ULyudWinterPlay | Winter/LyudWinterPlay.cpp | 1 | frame | cvar | rink, banya room step, body heat, ice hole, snowballs | note: year-round (banya/sea cold); winter parts are O(1) and hold state flips (hole visibility, skates) that a skip would break: left |
| ULyudCityThings | City/LyudCityThings.cpp | 1 | frame, pick at PickEvery | outside town box | pick in view (throttled), modes | ok |
| ULyudWindowLights | World/LyudCityBuilding.cpp | 1 | accum 0.25/0.5/1 s | - | window/lamp updates | ok |
| ULyudCityDress | World/LyudCityDress.cpp | 1 | accum 2 s | key unchanged | dressing | ok |
| ULyudDoorSubsystem | Doors/LyudDoor.cpp | 1 | agents 10 Hz | - | E poll, door agents | done |
| ULyudEuropaSubsystem | Europa/LyudEuropa.cpp | 1 | frame | no hero -> return | prompt look, climb | ok (cheap) |
| ULyudFestivalSubsystem | Festival/LyudFestival.cpp | 1 | 2 s | - | TickHero per frame, Update 0.5 Hz | ok |
| ULyudPoplarFluffSubsystem | Festival/LyudFestivalTown.cpp | 1 | 0.5 s | season | fluff | ok |
| ULyudKiteFlying | Kite/LyudKite.cpp | 1 | frame, town scan 2 Hz | cvar | hero kite, town kites | ok |
| ULyudWeatherSubsystem | Weather/LyudWeather.cpp | 1 | frame | - | change broadcasts | ok |
| ULyudWindSubsystem | Weather/LyudWind.cpp | 1 | frame | - | MPC wind drift (shaders sway by it) | ok (visible) |
| ULyudGradeSubsystem | Weather/LyudGrade.cpp | 1 | frame | - | sun elevation + grade maths, PP settings | note: ~1 us, eases per frame |
| ULyudRainFogSubsystem | Weather/LyudRainFog.cpp | 1 | - | returns once Fx exists | spawn once | ok |
| ULyudCloudSubsystem | Sky/LyudClouds.cpp | 1 | frame | - | cloud blend | ok (visible) |
| ULyudSunGlare | Sky/LyudSunGlare.cpp | 1 | frame | cvar | overlay | ok (UI) |
| ULyudTelescope | Sky/LyudTelescope.cpp | 1 | frame | not looking -> return | | ok |
| ULyudUnderwaterSubsystem | Sea/LyudUnderwater.cpp | 1 | frame | near water / under | sea life, collection only near | ok |
| ULyudSoundscapeSubsystem | Audio/LyudSoundscape.cpp | 1 | frame, timers | - | listener, beds | ok |
| ULyudMusic | Music/LyudMusic.cpp | 1 | 4 Hz main | - | caption fade per frame | ok |
| ULyudInstruments | Music/LyudInstruments.cpp | 1 | frame | not open -> return | | ok |
| ULyudGraphicsSubsystem | UI/LyudGraphics.cpp | 1 | frame | - | cached cvar compares | ok |
| ULyudGameScreen, ULyudInventoryUI, ULyudQuestDirector, ULyudBirdGuideScreen, ULyudBookScreen, ULyudFishBookScreen | UI, Books, Fishing | 1 each | frame | key polls, slow timers | input polls | ok (UI) |
| ULyudDayLog | UI/LyudDayLog.cpp | 1 (FTSTicker) | 4 s | - | weather sample | ok |
| AcademToggle LampTick | World/AcademToggle.cpp | 1 (FTSTicker) | 0.5 s | - | lamps | ok |
| ULyudLadderSubsystem | World/LyudLadder.cpp | 1 | frame | no hero -> return | loop over a handful of ladders | ok |
| ULyudFrontDoor | Transport/LyudFrontDoor.cpp | 1 | frame | - | dusk, owners | ok |
| ULyudRailwaySubsystem, ULyudTrainPassenger | Railway/*.cpp | 1 | frame | not started -> return | | ok |
| ULyudFishing, ULyudSmoking, ULyudHandUse, ULyudFoodSubsystem, ULyudBiteSubsystem, ULyudPhoto, ULyudReading, ULyudBenchSit, ULyudBeachHire, ULyudCinema, ULyudGarden, ULyudLetters, ULyudStallSubsystem | Fishing, Smoking, Food, Photo, Books, City, Garden, Letters | 1 each | frame (garden/letters step timers) | session/hero early outs | player-interaction polls | ok |
| ULyudAtonLab | AtonLab.cpp | 1 | 5 s scope | - | | ok |
| ULyudBenchmark | Bench/LyudBenchmark.cpp | 1 | frame | bench only | | ok |
| UTreeBakeCache | Plants/TreeBakeCache.cpp | 1 | frame | - | job hand-off | ok |
| ULyudNorthSubsystem | North/LyudNorth.cpp | 1 | 0.25 s range | not loaded -> return | wakes north actors | ok |
| ULyudTopiSubsystem | Topi/LyudTopi.cpp | 1 | - | returns after spawn | | ok |
| UAnimalLifeSubsystem, UAnimalFriendSubsystem, ULyudPets | Animals/*.cpp | 1 each | timers | cvar | sim | ok |
| UBirdTameSubsystem | BirdTame.cpp | 1 | frame | | | not edited (bird area) |
| UPeopleLifeSubsystem, ULyudCounterService, ULyudCounterTradeSubsystem, ULyudFriends, ULyudYardGames, ULyudTownTalkSubsystem | People/*.cpp | **(other agent)** | | | | not edited |
| UBirdLifeSubsystem, UBirdFlightSubsystem, UBirdFeederSubsystem, UCorvidLifeSubsystem | Birds/*.cpp | **(other agent)** | | | | not edited |
| ULyudRounds | Rounds/LyudRounds.cpp | **(other agent)** | | hour | | not edited |
| ULyudBikeSubsystem | Bikes/LyudBikeSubsystem.cpp | **(other agent)** | | | | not edited |
| Dev / test tickers (LyudDev fly, LyudDevQA, LyudDoorTest, BirdTame test) | Dev, Doors, BirdTame | only when run | | | | ok |

## Top 10 by estimated game-thread cost (outside the other agent's areas)

Rough per-frame estimates from the work in the bodies (not measured: no engine here).

| # | ticker | before | after |
|---|---|---|---|
| 1 | ULyudTransportSubsystem (12 cabins driven + boards/ropes) | ~60-120 us | same (visible motion) |
| 2 | ALyudSea (+ depth map steps until read) | ~40-80 us | same (camera-attached) |
| 3 | AProceduralPlant x hundreds (interval-managed, budgeted re-shapes) | ~30-60 us avg | same |
| 4 | ALyudStationHall x7 near one (people driven per frame) | ~20-50 us near, ~2 us far | same |
| 5 | ULyudCityThings (town pick throttled) | ~10-20 us | same |
| 6 | ANpcTalkDirector (talk target traces) | ~5-20 us | same |
| 7 | ULyudWinterPlay year-round | ~5-10 us | same (see note) |
| 8 | ALyudStationLift x<=7 | ~7-15 us every frame | ~0.5 us avg idle (4 Hz) |
| 9 | AProceduralVine x~10 | ~5-10 us every frame | ~0.5 us avg (4 Hz) |
| 10 | Rail/crossing sign widget components x~8 + ALyudDiner | ~10-20 us | ~1 us |

The large many-instance populations (plants, doors, station halls, curtains, south spots, festival actors, north actors)
were already gated by interval, tick enable/disable or distance before tonight's branch; tonight's fixes take the
remaining all-the-time per-frame tickers (lifts, vines, still signs, the diner) off the frame.
