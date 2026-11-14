# Packaging: from the editor's `-game` to a cooked build

State on 2026-11-13: the game has only ever run as `UnrealEditor ... -game` on a Mac. It has never been cooked,
staged or built as a game target. This page is the audit of what stands between that and a package, what was
fixed in the config, what other owners have to fix in C++, and the checklist for the first cooked Mac build.

Nothing here was verified by an actual cook or a game-target compile (there is no engine on the machine the
audit ran on). Everything marked **verify** is a thing to look at in the first build's logs.

## Why a package differs from `-game`

| In `-game` (uncooked) | In a package |
| --- | --- |
| Every `.uasset` under `Content` can be loaded by path | Only what the cooker took: what the cooked maps refer to, plus `DirectoriesToAlwaysCook` |
| Every file under the repository is readable (`Content/*.json`, `tools/`, `docs/`) | Only what was staged: `DirectoriesToAlwaysStageAsNonUFS` / `AsUFS`. Nothing outside `Content` |
| The Mac's file system ignores letter case | Loose files on Linux, iOS and Android are case-sensitive (assets inside pak/IoStore are looked up without case) |
| Editor modules are linked, editor-only data is there | No `UnrealEd`, no `WITH_EDITOR`, no source models, no mesh CPU copies |
| A material missing a usage flag gets it compiled on the spot | It is drawn with the grey default material |
| The Python tools next to the project can be run | No shell-outs: there is no `tools/`, no `python3` |

The game is unusual in one way that matters for all of it: nearly everything in the town is **built in C++ at
play** from assets found by a path string (205 `/Game/...` literals, about 70 `/Engine/...` ones) and from loose
json/bin/txt under `Content` (60 files). The map itself refers to very little. So the cooker and the stager find
almost nothing by themselves, and the lists in `Config/DefaultGame.ini` are what the package is made of.

## The check: `tools/check_package_paths.py`

```bash
python3 tools/check_package_paths.py            # everything; exit 1 on any problem
python3 tools/check_package_paths.py --strict   # on the cooking machine: content made per checkout must exist too
python3 tools/check_package_paths.py --names-only   # a checkout without the imported content: case + cook/stage only
```

It reads every string literal of `Source` (comments skipped) and checks:

1. `"/Game/..."` literals exist under `Content` with exactly that case. A literal with `%s`/`%d` is matched as a
   pattern; a literal without an object name may be a folder or a name prefix (`/Game/UI/Diary/Birds/T_Bird_`).
   `LyudText::LoadTexture(TEXT("Folder"), ...)` is checked as `/Game/Folder/Localized`.
2. Loose file literals (`.json .bin .txt .csv .ttf .otf .wav .png`) that name something under `Content`.
3. The folder of every asset literal is under a `DirectoriesToAlwaysCook` (also for `/Engine/...`).
4. The folder of every loose file is under a `DirectoriesToAlwaysStageAsNonUFS` / `AsUFS`.
5. `MapsToCook` exists, and the maps and the game mode named in `DefaultEngine.ini` exist.

LFS pointer files count as existing. A missing path that git ignores is listed as "made per checkout".

Result on the audited tree (before the config fixes: 36 cook/stage problems; after):

- `--names-only`: 298 paths fine, **0 problems**. No letter-case mismatch anywhere between the code and the disk.
- default: 29 problems, all `MISSING`: content that an import script makes and that was never committed (next section).
- what the script cannot see: names that come from data at run time (a species id, a dish, a sign). Spot-checked by
  hand: `BirdsData/species.json` ids against `Content/Birds/*`, `AnimalsData/animal_bodies.json` against
  `Content/Animals/*`, `PeopleData/people.json` models against `Content/Characters/Humans/*`: all match in case.

Run it before every cook, and add it to whatever CI the project gets (`--names-only` there).

## What the config now says (`Config/DefaultGame.ini`, `Lyudmilovsk.uproject`, `Lyudmilovsk.Build.cs`)

- **Staged loose folders** (`DirectoriesToAlwaysStageAsNonUFS`): `Dialogue Localization BirdsData AnimalsData UI
  Transport Fishing City Food PeopleData Dev` were there; added `Academgorodok Bikes Diner Europa Railway
  Restaurant`. Without them a package had no railway (`Railway/railway_built.json`, `kit.json`), no bicycles
  (`Bikes/bike.json`, `bikes.json`), no restaurant and diner layout or menu, the fountain's and Europa's built-in
  fallbacks. NonUFS (loose next to the pak, not inside it) is deliberate: three simulations read these folders
  with `std::ifstream`, which cannot read from a pak.
- **Always cooked** (`DirectoriesToAlwaysCook`): `Inventory Hall Voice Audio UI Transport` were there; added
  `Academgorodok Animals Bikes Birds Characters/Clerk Characters/Hero Characters/Humans City Diner Europa Food
  ForestFloor Input Plants Railway Restaurant Scene01/Sky Scene01/Terrain Sea Smoking Weather Winter`, and
  `/Engine/BasicShapes`, `/Engine/EngineSky`.
- **Maps**: `+MapsToCook=(FilePath="/Game/Scene01/Lvl_Scene01")`; `DirectoriesToNeverCook` for `/Game/Dev` and
  `/Game/Developers` (the twenty-odd preview and test levels of a working checkout).
- **Plugins**: `ModelContextProtocol`, `AllToolsets`, `PythonScriptPlugin`, `EditorScriptingUtilities` are now
  `TargetAllowList: ["Editor"]`, like `ModelingToolsEditorMode`. The editor (and its `-game`) is unchanged.
- **Build.cs**: `AssetRegistry` named as a dependency (`LyudRestaurant.cpp:15` includes
  `AssetRegistry/AssetRegistryModule.h`; it built through the editor target's transitive modules only).

Not changed, on purpose:

- `Scene01` is not always-cooked whole: `Lvl_TerrainPreview` and `Lvl_WorldPreview` live there. Everything else in
  it comes in through `Lvl_Scene01`'s own references.
- `Characters/Humans` is cooked whole and, on a working checkout, holds four test levels
  (`L_BodyTest`, `L_FaceTest`, `L_HumansPreview`, `L_LifeAnimsPreview`): they will be cooked too. Harmless but
  wasteful: move them to `/Game/Dev` (their `make_*_level.py` / `test_*.py` scripts name the path).
- `WebBrowser` stays in Build.cs: `AtonOS/AtonApps.cpp:13,1331` uses `SWebBrowser` (the Chosen game on the hero's
  computer). It brings CEF into desktop packages (about 200 MB) and needs the engine's `EpicWebHelper` built.
- `[/Script/Engine.AssetManagerSettings]` scans a `GameFeatureData` type in `/Game/Unused` while the GameFeatures
  plugin is off: template leftover. **verify**: if the cook warns about it, delete the line.

## What a packaging pipeline needs before it cooks

The repository alone is not enough to cook. In this order, on the cooking Mac:

1. **All of LFS**: `git lfs pull` (5149 `.uasset` are pointers otherwise; the cooker fails on the first one).
2. **The level**: `unreal/scripts/build_scene01.sh` (editor closed). `Content/Scene01/` is ignored by git and built
   from `.glb`: five headless editor runs (`import_scene01.py`, `import_terrain.py`, `import_world.py`,
   `import_sky.py`) with `tools/railway.py` and `tools/transport.py` between them. It makes, besides the map:
   - `/Game/Scene01/Terrain/DA_TerrainMask` (`Terrain/LyudTerrain.cpp:22`: grass, forest floor, snow, wetness read it),
   - `/Game/Scene01/Sky/M_LyudSkyOptics`, `M_LyudLightning` (`Sky/LyudRareSky.cpp:126,131`),
   - `Content/Transport/network_built.json` and `Content/Railway/railway_built.json` (ignored by git). In a
     game target there is no fallback: `Transport/LyudTransport.cpp:244` and `Railway/LyudRailway.cpp:103` log
     an Error and switch the cable car / the railway off (the editor-only branch that ran the Python tool is
     `#if WITH_EDITOR`).
3. **Content that only import scripts make** (ignored by git, or simply never committed). The check lists exactly
   what is absent; on the audited tree:

   | Missing | Loaded at | Made by |
   | --- | --- | --- |
   | `Characters/Hero/**` (ignored) | `HeroCharacter.cpp:56` (a `ConstructorHelpers` find: **the cook fails** without it), `HeroAnimInstance.cpp:12` | `unreal/scripts/import_hero.sh` |
   | `Characters/Humans/Town/**` (ignored, ~900 meshes) | `People/PeopleLife.cpp:771-773` | `unreal/scripts/import_residents.sh` |
   | `Bikes/Meshes/bike_kit/**` | `Bikes/LyudBike.cpp:33` | `unreal/scripts/import_bikes.sh` |
   | `Birds/Nests/**` | `Birds/BirdNests.cpp:100` | `unreal/scripts/import_nests.sh` |
   | `Audio/Festival/**` | `Festival/LyudFestival.cpp:1198,2621-2627,3109,3317` | `tools/festival_sounds.py` + `unreal/scripts/import_audio.py` |
   | `Food/Meshes/SM_Fork`, `SM_Spoon` | `Food/LyudBite.cpp:447` | `unreal/scripts/import_food.sh` |
   | `Restaurant/Meshes/**`, `Materials/MI_RS_SignPlaque`, `Localized/**` | `Restaurant/LyudRestaurant.cpp:261-288,1323,1331` | `unreal/scripts/import_restaurant.py` (docs/restaurant.md) |
   | `UI/Diary/M_DiaryPage`, `T_PaperLeft/Right`, `T_MapInk`, `Fonts/FF_*` | `UI/LyudDiaryBook.cpp:221`, `UI/LyudDiaryStyle.cpp:35,131,132,183`, `UI/LyudGameScreen.cpp:126-127` | `unreal/scripts/import_diary_assets.py` (docs/diary.md) |

   Most of this code tolerates a missing asset in `-game` (a plain box, no sound). A package made without the
   import would simply ship that way, silently. Decide per row: commit the assets to LFS, or make the import a
   step of the pipeline. Until then `--strict` is the gate.
4. `python3 tools/check_package_paths.py --strict` → 0 problems.
5. One `-game` run of the same tree, and in its log: no `needed to have bUsedWith` / `missing the usage flag`
   lines (a material the editor patched on the fly is grey in a package), no `Failed to find object`, no
   `Transport: OFF` / `Railway: OFF`.

## Checklist: the first cooked Mac build

Paths as in `README.md` (`UE=/Users/Shared/Epic Games/UE_5.8`, run from the repository root). The Mac needs the
Xcode Metal toolchain (`xcodebuild -downloadComponent MetalToolchain` on Xcode 26: it blocked the editor build
before, docs/integration_night_1106.md) and no editor open on the project.

1. Steps 1-5 of the section above.
2. **Compile the game target alone first** (it has never been compiled; this is the cheapest place to find what
   only the editor target forgave):

   ```bash
   "$UE/Engine/Build/BatchFiles/Mac/Build.sh" Lyudmilovsk Mac Development \
       -Project="$PWD/unreal/Lyudmilovsk/Lyudmilovsk.uproject"
   ```

   Expect errors of two kinds if any: an include that only resolved through an editor module, or an editor-only
   member used outside `#if WITH_EDITOR`. The audit found none by reading (see "Checked and fine"), but nothing
   replaces this compile.
3. **Build, cook, stage, package, archive**:

   ```bash
   "$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
       -project="$PWD/unreal/Lyudmilovsk/Lyudmilovsk.uproject" \
       -target=Lyudmilovsk -platform=Mac -clientconfig=Development \
       -build -cook -map=/Game/Scene01/Lvl_Scene01 \
       -stage -pak -iostore -package \
       -archive -archivedirectory="$PWD/unreal/Lyudmilovsk/Saved/Packages" \
       -nop4 -utf8output -unattended -nocompileeditor
   ```

   `-nocompileeditor` because the editor is already built (drop it on a clean machine). `-map` repeats
   `MapsToCook` so a stray command line cannot widen the cook. The first cook compiles every shader for Metal
   SM6: allow an hour or more. Add `-iterate` to later cooks. Use `-clientconfig=Shipping` only after
   Development runs: Shipping has no console, no on-screen log and no dev mode without `-dev`
   (`Dev/LyudDev.cpp:427`).
4. **Read the cook log** (`unreal/Lyudmilovsk/Saved/Logs/` and UAT's `Log.txt`). The cook must end with
   `0 error(s)`. Look for:
   - `CDO Constructor (...): Failed to find` → a `ConstructorHelpers` asset that is not on disk (step 3 of the pipeline).
   - `Unable to find package for cooking` / `Could not find` under an always-cook folder.
   - errors from the four test levels in `Characters/Humans` (move them, see above).
5. **Look at what was staged** (the `.app` is in `Saved/Packages/Mac/`):

   ```bash
   APP=unreal/Lyudmilovsk/Saved/Packages/Mac/Lyudmilovsk.app
   find "$APP" -path '*Lyudmilovsk/Content*' \( -name '*.json' -o -name '*.bin' -o -name '*.txt' \) | wc -l   # 60 + network_built + railway_built
   find "$APP" -path '*Lyudmilovsk/Content*' -name '*.uasset' | wc -l                                          # must be 0
   ```

   **verify**: the second number. UAT drops `.uasset`/`.umap` from the staged-as-is folders; if it does not in
   5.8, uncooked assets ship twice and the folders with both json and assets (`UI Transport City Food
   Academgorodok Bikes Diner Europa Railway Restaurant`) need their json moved to folders of their own.
6. **Run it** with a log and compare with `-game`:

   ```bash
   "$APP/Contents/MacOS/Lyudmilovsk" -log -windowed -ResX=1600 -ResY=900
   ```

   First things to check in the log and on screen, in this order:
   - `LogStreaming` / `LoadObject` warnings `Failed to find`/`Can't find file`: an asset loaded by a path the
     check did not see (built from data) in a folder that is not always-cooked. Add the folder.
   - `PeopleLife:`, `BirdLife:`, `AnimalLife:` warnings: the simulations' data did not open (`std::ifstream` on
     the staged folder).
   - `Transport: OFF`, `Railway: OFF`, `LyudQuests: no Content/UI/...`, `Restaurant: ... unreadable`,
     `FISHING no Content/Fishing/fishing.json`: a loose file missing from the stage.
   - grey default material anywhere (people, birds, trees, the sea, rain, snow): a usage flag.
   - walk into a station, onto a pier, through a doorway of the code-built kits: their collision is made at
     run time (`Transport/LyudMeshKit.cpp:384-457` adds convex elements and calls `CreatePhysicsMeshes()`).
     **verify** that convex collision cooks at run time in a non-editor build; if the hero falls through, the
     kit needs boxes instead of convex pieces, or `bNeverNeedsCookedCollisionData` handling.
   - the prices on the restaurant's menu card: `Restaurant/SLyudRestMenu.cpp:39` reads the engine's
     `Slate/Fonts/Roboto-Medium.ttf` by file name. **verify** it is staged (engine Slate fonts normally are).
   - the Chosen game on the hero's computer (CEF in a package).
   - save, quit, start, load. **verify** where `Saved/` is: next to the staged content in Development, the
     user's Library in Shipping (expected `~/Library/Application Support/Epic/Lyudmilovsk/Saved`).
7. Benchmark the package against `-game` (`docs/benchmark.md`, `-LyudBench`): a cooked build should be faster
   to load and no slower per frame; a large difference either way is a finding.

Windows and Linux use the same command with `-platform=Win64` / `-platform=Linux` on a Windows machine (Linux
cross-compiles from Windows with the clang toolchain). They are not expected to work before the source fixes below.

## Known blockers

Ordered by how soon they bite. `file:line` is relative to `unreal/Lyudmilovsk/Source/Lyudmilovsk/Private`.

### Every platform

| # | Blocker | Where |
| --- | --- | --- |
| B1 | The game target has never been compiled. | step 2 of the checklist |
| B2 | Content that is not in git has to be made on the cooking machine; the hero's mesh is a hard cook error without it. | table above; `HeroCharacter.cpp:56-63` |
| B3 | `Lvl_Scene01` and the two `*_built.json` exist only after `build_scene01.sh`; no fallback in a game target. | `Transport/LyudTransport.cpp:231-248`, `Railway/LyudRailway.cpp:91-106` |
| B4 | Material usage flags: the code-built and instanced meshes (24 run-time mesh builders, instanced kits, skeletal people with morph targets, Nanite, the volumetric cloud) use materials made by import scripts. The scripts set the flags they know of; anything the editor had to add at run time is grey in a package. | pipeline step 5 |
| B5 | In Shipping the hero's computer first tries `http://localhost:8787/` (a developer's local build of Chosen) and only on a load error goes to the public site: a slow start of the app for every player, and whatever a player's own machine serves on that port is shown inside the game. | `AtonComputer.cpp:68`, `AtonOS/AtonApps.cpp:1309,1320,1332` |

### Windows

| # | Blocker | Where |
| --- | --- | --- |
| W1 | The three simulations open their data with `std::ifstream(std::string)` on a UTF-8 path. On Windows a narrow path is read in the ANSI code page: with the game installed under a non-ASCII folder (`C:\Игры\`, a Cyrillic or Japanese user name) the files do not open: no people, no birds, no animals. | `People/PeopleSim.cpp:24` (called from `:128-142`), `Birds/BirdSim.cpp:123,181`, `Animals/AnimalSim.cpp:97,132`; the paths come from `People/PeopleLife.cpp:192-194`, `Birds/BirdLife.cpp:190-191`, `Animals/AnimalLife.cpp:197-198` |
| W2 | `Birds/HeadTracker.cpp` (AirPods head tracking) is `#if PLATFORM_MAC` throughout: no head tracking elsewhere. Not a failure, a missing feature. | `Birds/HeadTracker.cpp:8-110` |

### Linux

| # | Blocker | Where |
| --- | --- | --- |
| L1 | Loose files are case-sensitive. The check finds no mismatch today; keep it in CI. | `tools/check_package_paths.py` |
| L2 | CEF (`SWebBrowser`) on Linux needs the engine's Linux CEF build and its libraries on the player's machine. | `AtonOS/AtonApps.cpp:1331` |

### iOS and Android

Not close. Listed so nobody is surprised:

| # | Blocker | Where |
| --- | --- | --- |
| M1 | `std::ifstream` cannot read staged content at all: on Android the files are inside the APK/OBB, on iOS `FPaths::ConvertRelativePathToFull` is not a real file-system path. People, birds and animals do not start. Same fix as W1. | as W1 |
| M2 | No `[/Script/IOSRuntimeSettings.IOSRuntimeSettings]` / `[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]` in `Config/DefaultEngine.ini`: no bundle id, no minimum OS, no desktop-renderer switch. The project is Lumen + virtual shadow maps + SM6 (`DefaultEngine.ini:21-26,57-63`); the mobile renderer has neither. Only `Config/IOS/IOSScalability.ini` and `Config/Android/AndroidScalability.ini` exist. | `Config/DefaultEngine.ini` |
| M3 | No touch controls: `DefaultTouchInterface=None` (`Config/DefaultInput.ini`), and play is keyboard and mouse (next section). `Content/Input/Touch/UI_TouchSimple` exists and is not used. | `Config/DefaultInput.ini` |
| M4 | The hero's computer writes notes with `IFileManager` under `ProjectSavedDir` (fine) and opens web pages (`SWebBrowser`: a native web view on mobile; App Transport Security refuses the `http://localhost` URL of B5). | `AtonOS/AtonApps.cpp:81-101,1309` |
| M5 | 60 loose files as NonUFS inflate an APK's file table and are read one by one at start (`PeopleData/town_places.json` is 1.8 MB, `city_plan.json` 0.97 MB, parsed as text). Once W1/M1 is fixed they can move to `DirectoriesToAlwaysStageAsUFS` (inside the pak). | `Config/DefaultGame.ini` |

## Source fixes for other owners

The audit could not edit C++ outside the config. Each of these is small.

**F1 (B5). No localhost in Shipping.** `AtonComputer.cpp:68`

```cpp
	AtonApps::ChosenLocalURL = ChosenLocalURL;
```

becomes

```cpp
#if UE_BUILD_SHIPPING
	// a developer's local build of Chosen (tools/chosen_local.sh) is never tried in a shipped game
	AtonApps::ChosenLocalURL.Reset();
#else
	AtonApps::ChosenLocalURL = ChosenLocalURL;
#endif
```

and `AtonOS/AtonApps.cpp:1309`

```cpp
FString AtonApps::ChosenLocalURL = TEXT("http://localhost:8787/");
```

becomes

```cpp
#if UE_BUILD_SHIPPING
FString AtonApps::ChosenLocalURL;
#else
FString AtonApps::ChosenLocalURL = TEXT("http://localhost:8787/");
#endif
```

(`AtonApps.cpp:1320` already treats an empty string as "no local build".) Verify: a Shipping build opens the
public site at once; Development still tries the local one first.

**F2 (W1, M1). The simulations take their data as text, not as a folder.** The models are plain C++ shared with
the host suites (`tools/birdsim`, `tools/animalsim`, `tools/peoplesim`), so they keep `std`; only who opens the
files changes. `PeopleSim.h:344-345,417-419` already has the by-text loaders (`LoadPlaces`, `LoadActivities`,
`LoadPeople`, `LoadTownPlaces`, `LoadTownsfolk`); `FWorld::Load(Dir, AnimalsDir)` (`PeopleSim.cpp:128-142`) is
the only part that reads files. For each of the three:

- add to the model an overload that takes the contents (`bool LoadHabitats(const std::string& HabitatsJson,
  const std::string& WorldBin)` in `Birds/BirdSim.h:347`; `bool Load(const std::string& WorldBin, const
  std::string& PlacesJson)` in `Animals/AnimalSim.h:274`; a `Load` taking the five people texts plus the animal
  grid in `People/PeopleSim.h:416`), and make the folder version read the files and call it (the host suites keep
  calling the folder version);
- in `Birds/BirdLife.cpp:190-191`, `Animals/AnimalLife.cpp:197-198`, `People/PeopleLife.cpp:192-194` read with
  `FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectContentDir() / TEXT("BirdsData/bird_world.bin")))` /
  `LoadFileToString`, and pass `std::string((const char*)Bytes.GetData(), Bytes.Num())` (for json:
  `TCHAR_TO_UTF8` of the loaded string, as `Transport/LyudTransport.cpp:250` does).

`FFileHelper` goes through the engine's file layer: Unicode paths on Windows, pak files, APK/OBB, the iOS bundle.
Verify: host suites unchanged (run `bash tools/birdsim/run.sh`, `tools/animalsim/run.sh`, `tools/peoplesim/run.sh`
before and after: same counts); in the game the three `...Life:` start lines of the log are the same as before.
After it, the three data folders no longer have to be NonUFS.

**F3 (optional, Mac Shipping). No shell from a shipped game.** `Birds/HeadTracker.cpp:64-78` runs
`/bin/sh ../../tools/headtracker/build.sh` when the helper app is not in `Saved/HeadTracker`. In a package the
script does not exist, so it is a no-op that sets the state to "no helper"; a sandboxed (App Store) build may not
start processes at all. Wrap the `if (!bBuilding) { ... }` block in `#if !UE_BUILD_SHIPPING`, and decide how the
helper ships (inside the app bundle, started with `NSWorkspace`), or leave head tracking out of the first release.

**F4 (housekeeping). Move the four test levels** out of `Content/Characters/Humans` into `/Game/Dev`
(`unreal/scripts/make_body_test_level.py`, `make_face_test_level.py`, `make_humans_preview_level.py`,
`make_life_preview_level.py` and the tests that open them): `/Game/Dev` is never cooked.

## Platform assumptions (list only)

- **Shell-outs**: `Transport/LyudTransport.cpp:239` and `Railway/LyudRailway.cpp:99` run `/usr/bin/python3` on
  `../../tools/*.py`: both inside `#if WITH_EDITOR`, absent from a game target. `Birds/HeadTracker.cpp:58,70`
  run `/usr/bin/open` and `/bin/sh`: `#if PLATFORM_MAC` (F3).
- **Reads outside `Content`**: only the two above (`tools/`), `Saved/HeadTracker`, and
  `FPaths::EngineContentDir() / "Slate/Fonts/Roboto-Medium.ttf"` (`Restaurant/SLyudRestMenu.cpp:39`). Nothing
  reads `docs/`, the repository root or an absolute path. `PLANT_MASK_DUMP` (`Plants/PlantMeshes.cpp:661`) is an
  environment variable for a developer's dump.
- **Writes**: all under `FPaths::ProjectSavedDir()` or a path the developer types into a console command:
  saves (`LyudSave.cpp:94,159`, `SaveGameToSlot`), notes (`AtonOS/AtonApps.cpp:81-82`), dev bookmarks
  (`Dev/LyudDev.cpp:123,348`), benchmark results and shots (`Bench/LyudBenchmark.cpp:194,434,543`), sim dumps
  (`Birds/BirdLife.cpp:1160,1181`, `Animals/AnimalLife.cpp:715,767`), audit and export commands
  (`Plants/ForestFloor.cpp:1493`, `City/CityKit.cpp:45,250`, `Birds/BirdFlightSubsystem.cpp:870`,
  `Smoking/LyudSmoking.cpp:1181`). `Terrain/TerrainGen.cpp:389` and `Winter/SnowModel.cpp:252` write only from
  editor-only or host-tool callers. Nothing writes under `Content`.
- **Metal**: nothing in C++ is Metal-only. The config targets SM6 on all three desktops
  (`DefaultEngine.ini:28-63`: DX12 SM6 by default on Windows, Vulkan SM6, Metal SM6). Windows also lists DX11
  with SM5 shaders (`DefaultEngine.ini:32-33`), where Lumen and virtual shadow maps do not run: the game has never
  been seen that way.
- **Developer commands**: 506 console variables and commands (`lyud.*`). Shipping has no console; the dev mode
  (`Dev/LyudDev.cpp:425-433`) is off in Shipping unless started with `-dev`; the plant dev tool
  (`Plants/PlantDevTool.cpp:72-73,138`, F7 and H) follows it. `-LyudBench` (`UI/LyudGraphics.cpp:250`) works in
  every configuration, by design. `Content/Dev/teleports/*.json` is staged in every configuration (800 bytes).
- **Keys with no gamepad or touch equivalent**: the game reads keys directly (`EKeys::` in 40 files) rather than
  through input actions; only move, look, jump are Enhanced Input (`HeroCharacter.cpp:58-63`). Gamepad is handled
  in two places: the menu (`LyudGameMenu.cpp:256`, Start) and the inventory list (`LyudInventory.cpp:905-918`).
  Keyboard or mouse only:
  E to use (22 places in 21 files: `VendingMachine.cpp`, `HeroSeat.cpp`, `NpcDialogue.cpp`, `Doors/LyudDoor.cpp`,
  `Doors/LyudDoorTest.cpp`, `Diner/LyudDiner.cpp`, `Restaurant/LyudRestaurant.cpp`, `Transport/LyudTransport.cpp`,
  `Transport/SLyudTransitWidgets.cpp`, `Railway/LyudTrain.cpp`, `World/LyudLadder.cpp`, `Fishing/LyudRowBoat.cpp`,
  `Europa/LyudEuropa.cpp`, `Festival/LyudFestival.cpp`, `Birds/BirdFeeder.cpp`, `Birds/BirdPawn.cpp`,
  `Birds/CorvidLife.cpp`, `Animals/AnimalNameTags.cpp`, `Plants/ProceduralPlant.cpp`, `Smoking/LyudSmoking.cpp`,
  `Bikes/HeroBike.cpp`);
  J / K for the diary tabs, L for the lamp, A / Left / PageUp to turn pages
  (`UI/LyudGameScreen.cpp:622,710-712,798`, 10 keys in all); B and N in the bird guide and the fish book
  (`UI/LyudBirdGuide.cpp:395`, `Fishing/LyudFishBook.cpp:446`); 1-9 on the vending machine
  (`VendingMachine.cpp:491`); mouse buttons, Q, X and Y while fishing (`Fishing/LyudFishing.cpp:977,1704-1732`);
  G for the corvids (`Birds/CorvidLife.cpp:277`); the whole bird flight (`Birds/BirdPawn.cpp:349-359`: mouse
  buttons, Space, Shift, Ctrl, C, R, Q, E, F, D); swimming (`Sea/LyudSwim.cpp:491-508`: Space, Ctrl, Shift, C);
  dragging the map (`UI/SLyudMapView.cpp:390,400`); the hero's computer (`AtonOS/SAtonDesktop.cpp`: a mouse
  desktop). The full list: `grep -rn 'EKeys::' unreal/Lyudmilovsk/Source`. `EKeys::Android_Back` opens the menu.

## Checked and fine

- **Editor-only API**: every use found is guarded. `Terrain/LyudTerrain.cpp:6-18,76-240` (landscape import,
  `SetActorLabel`) and its header `Public/Terrain/LyudTerrain.h:64-88` are `#if WITH_EDITOR`;
  `UI/LyudFontTools.cpp:10-36` is `#if WITH_EDITORONLY_DATA` with a `nullptr` return otherwise;
  `World/LyudCityBuilding.cpp:184-186,538-540` guard `MipGenSettings` and `SetActorLabel`. No `GEditor`, no
  `UnrealEd`, asset tools or `MaterialEditingLibrary` in the module. `GIsEditor` is read in a few places
  (`LyudGameMenu.cpp:91,214`, `UI/LyudGraphics.cpp:270,891`): it exists in every target.
- **Run-time mesh building**: all 24 builders use `UStaticMesh::BuildFromMeshDescriptions` with
  `bFastBuild = true`, `bCommitMeshDescription = false`, simple collision only (`CTF_UseSimpleAsComplex`), which
  is the path meant for non-editor builds. `MeshDescription` and `StaticMeshDescription` are runtime modules. No
  code reads vertex or index buffers of a cooked mesh (which a package strips): only triangle and section counts.
- **Textures**: made with `UTexture2D::CreateTransient` and filled through platform data; no `Source` access.
- **Asset registry**: `ScanPathsSynchronous` is skipped where cooked data is required
  (`People/PeopleLife.cpp:769`, `Restaurant/LyudRestaurant.cpp:282`, `World/LyudPrefetch.cpp:113`);
  `GetAssetsByPath` and `FPackageName::DoesPackageExist` work against a cooked registry.
- **Module list**: no editor module in `Lyudmilovsk.Build.cs`; both `Target.cs` are plain.
- **Console variables**: none is a cheat variable (those vanish in Shipping); the lookups by name are of the
  game's own variables.
- **Letter case**: 298 literals, no mismatch.
