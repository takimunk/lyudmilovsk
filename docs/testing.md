# Offline checks

Everything that can be checked without Unreal runs on Linux or a Mac in one command:

    tools/run_all_suites.sh            # everything, ~20 min (birdsim alone ~6)
    tools/run_all_suites.sh --quick    # skips the slow rows, ~6 min
    tools/run_all_suites.sh --only peoplesim,check_data

It prints one table (a row per check: PASS, FAIL or SKIP, its time, the last line that says how it went) and exits with
the number of failed rows. Each row's full output is in `Saved/Suites/<row>.log`. Run it before you push anything that
touches the plain-C++ models under `Source/Lyudmilovsk/Private` (people, animals, birds, weather, sea, winter, fishing,
festivals, the railway and the cable car), a generator under `tools/`, or a data file under `Content/**.json`.

Setup: `clang++`, `python3` (PIL for `underwater.bed`), and the two LFS worlds the animal and bird suites read:

    export GIT_LFS_SKIP_SMUDGE=1
    git lfs pull --include=unreal/Lyudmilovsk/Content/AnimalsData/animal_world.bin,unreal/Lyudmilovsk/Content/BirdsData/bird_world.bin

## What it runs

1. **The host suites**, every mode of every `tools/*/run.sh` (each builds the game's own model sources with a small
   driver): animalsim, audiosim, barksim, birdsim, festivalsim, fishing, peoplesim (with and without the gitignored
   railway timetable `Content/Railway/railway_built.json`), railsim, sea, transportsim, underwater, weather, winter.
   They are built with `-Wall -Wshadow -Wunreachable-code-loop-increment -Werror`, the warnings the Mac's Unreal build
   treats as errors, so a shadowed local in a model source fails here before it costs a Mac build. Every `run.sh` takes
   extra flags from `EXTRA_CXXFLAGS` (`EXTRA_CXXFLAGS=-g tools/peoplesim/run.sh test`); `--no-werror` drops `-Werror`.
2. **The generators** whose output is committed and reproducible here: `city_plan.py`, `academ_fountain.py`,
   `academ_wires.py`, `restaurant/seats.py`, `town/town_life.py`, `barks/build_barks.py` (and its PROBLEM lines: a
   condition no townsperson can meet), the string scripts (`restaurant/strings.py`, `lobby_strings.py`,
   `smoking_strings.py`, `characters/nao_strings.py`). Each is run and its committed output must not change; a changed
   output means the committed file is stale (or the generator changed and the data was not regenerated). The tree is put
   back as committed afterwards (`--keep` leaves the new output to look at or commit). A row whose output files are
   already modified in your tree is SKIPped, not judged. `railway.py --check` and `transport.py --check` run too.
   Not judged: the generators that read the built terrain (`public_spaces.py`, `people_map.py`, `animal_map.py`,
   `bird_habitats.py`, `europa/europa_site.py`): a terrain built here (`tools/underwater/run.sh bed` writes one to
   `Saved/Underwater/bed`; copy `terrain_in.json` and `terrain_out.bin` to `unreal/Lyudmilovsk/Saved/World/` for them)
   differs from the Mac's by centimetres to a couple of metres, so regenerate those on the Mac.
3. **`tools/check_data.py`**, the cross-checks between data files (exit 1 on a problem; `-v` lists every note):
   - no duplicate ids anywhere (people, places, flats, activities, festivals and their phases, quests with their stages
     and objectives, teleports, map places, dishes, fishing spots, plan plots and spots, counters) and no duplicate key in
     one JSON object (the string tables included: `json.load` would silently keep the last);
   - every reference resolves: friends, households, duties (activity, place), likes and start activities of people;
     counters' sellers (who must have that duty), activities and goods; fair stalls, menu dishes, foods, food stalls;
     activities' place kinds; festival venues; quests' reach/talk/item/festival targets and rewards; teleports' names
     and places; fishing spots' and kitchens' plan plots;
   - items are `LyudInventory.cpp`'s (plus `FishingModel.cpp`'s fish): each has `item.<id>.name`/`.desc` and an icon
     `Content/Inventory/Localized/ru/T_Icon_<id>` (what `ULyudInventoryUI::IconBrush` loads); prices >= 0, counts >= 1;
   - coordinates inside the map (`UI/map.json` frame + 50 m), heights between -80 and 250 m; a seat 0.2..1.3 m over its
     floor (0 only for stand, staff, counter, board, sit_ground, ...);
   - wake/bed and duty hours in the day, no two duties of one person overlapping on a day; festival phases at one venue
     not overlapping; fishing kitchens open before they close;
   - names: `npc.<key>.name` for every person, `place.<id>.name` for every map place and public-space area,
     `festival.<id>.name`, the quests' title/desc/log/obj keys; a story person's model (`mesh`, then `body` once made) is
     a committed `SK_<name>`; a townsperson's body is `Town/R_<key without t_>`, as `town_looks.json` says;
   - voices: every speaker with `line.<speaker>.*` lines has an entry in `localization/voice.json`.
   Add a check here when you add a data file that names ids of another.
4. **`tools/check_localization.py`**: the string tables (same keys and placeholders in ru/en/ja, every item's icon
   source picture).

Known failures on main (2026-11-13): `audiosim.test` (2: thunderstorm peak, far-storm rumble), `check_data` and
`check_localization` (the fishing gear, the fish, the diner's fish and the restaurant's dishes have no imported icon).

## In the engine

The engine's own tests are not part of this (they need the Mac): `unreal/scripts/test_*.sh` run offscreen plays
(see each script's header), and the developer mode (`docs/dev_mode.md`) jumps to any quest step, place or time to look.
