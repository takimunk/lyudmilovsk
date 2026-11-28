# The flight home from Europa: the descent craft «Ласточка»

Dima, 2026-11-27: once the hero is on the Moon (Europa, `docs/europa.md`), he can board a craft there and fly back to
the planet. About 15 minutes; through the porthole the planet grows into an ocean world with one island, Lyudmilovsk;
detailed cabin with a live instrument panel; a record player with openly licensed music; full sound; atmospheric entry
with fire. Beautiful, but cheap in memory.

> State 2026-11-28: everything below is written and passes its offline checks, but NOTHING has been compiled, imported
> or run in Unreal yet. The first Mac build, import and flight are owed (see "On the Mac").

## The craft

«Ласточка» ("Lastochka", the Swallow; ja ツバメ号): a small Soviet-flavoured two-part vehicle standing on Europa's ice
about 60 m from the hatch, on the planet's side of it.

- **Landing stage** (stays on Europa): octagonal truss, four legs with foot pads, engine bell, a ladder up to the hatch,
  gold foil, frost on the shaded side. After a flight the cabin is simply standing on it again (the world is a simulation;
  nobody explains).
- **Cabin** (the descent capsule, flies): headlight shape, heat shield below. Dream-roomy inside: inner diameter 2.6 m,
  2.05 m clear in the middle so the hero walks two steps; side hatch 1.20 m clear (character standard 1.08). Outside
  diameter about 3.1 m, height about 3.0 m.
- **Inside**: two couches (the hero's, and a second one that is empty: a folded blanket and a cap on it); the main
  instrument panel in front of the couches; one big forward porthole 0.60 m over the panel and two side portholes
  0.36 m; quilted padding, handrails, cable runs, two fans, three cabin lamps, a stowage net with a thermos and a book;
  on a side shelf a strapped portable record player (lid open, spring-loaded arm, a rack of sleeves under an elastic
  band); a small radio speaker grille; the hatch wheel; a red parachute handle under a cover overhead.

## The instrument panel (everything moves from the flight model)

| Id | Instrument | Shows |
|---|---|---|
| `globe` | the «Глобус»: a small real globe behind glass with a crosshair | the point under the craft: ocean all round and one island |
| `clock` | mission clock, 6 seven-segment digits | time since launch (mm:ss, hours when asleep) |
| `range` | 6 digits | km to the planet's surface |
| `speed` | 5 digits | km/s (m/s under the parachute) |
| `alt` | round dial, two needles | altitude: over Europa on ascent, over the sea in the descent |
| `gmeter` | round dial | felt load, 0..6 g |
| `horizon` | attitude ball | roll and pitch |
| `press`, `o2`, `co2`, `temp` | four small dials | cabin air |
| `volts`, `fuel` | two small dials | battery, propellant |
| `hull` | vertical bar | heat shield temperature |
| `crt` | small vector screen | the trajectory: Europa, the planet, the path, a dot for the craft; phase name |
| `lamps` | 4 x 6 annunciator board | see the lamp list in the flight model |
| `switches` | 5 guarded toggles + the ПУСК button under a red cover + cabin light + CRT mode + radio | the prelaunch checklist and comforts |

Words printed on the panel are localized (ru/en/ja label atlases, the localization skill).

## The flight (900 s from ПУСК to the splash; `lyud.Flight.Fast` speeds it for tests)

| Phase | t, s | What |
|---|---|---|
| `parked` | | the craft stands on Europa; ladder, hatch |
| `prelaunch` | | seated: five switches in order (power, air, gyros, hatch seal, arm), then ПУСК; wrong order: a lamp and a buzzer, nothing worse |
| `ascent` | 0..75 | 1.6 g burn, the ice falls away, the stage stays behind, the sky is black |
| `coast` | 75..600 | weightless; Europa shrinks astern, the planet grows from 26 degrees to a wall; one 6 s correction burn near 300 s; records, the thermos, the portholes; the hero may nap on the couch (time runs 8x) |
| `approach` | 600..720 | a low arc round the planet that brings the island under the track whatever the hour; limb glow, cloud decks, the island comes over the horizon; the instrument section is cast off at 700 s (a bang) |
| `entry` | 720..810 | plasma: glow, streamers, sparks past the portholes, the glass scorches; up to 4.2 g, shake, radio blackout, the hull bar climbs |
| `chute` | 810..885 | drogue (a jolt), main canopy at 825; a whiteout through the cloud deck hides the move into the real world: the cabin now hangs under its canopy over the real sea off the real island and comes down to it |
| `splash` | 885..900 | soft-landing jets, the splash, spray on the glass |
| `afloat` | | bobbing on the real sea off the beach; open the hatch, swim home. The capsule drifts there till the hero is far away, then it is gone |

The planet's hour is the game's: a night arrival shows the town's lights.

## Cheap by design

- The cabin never travels. In space it sits in a far "stage" spot; the planet and Europa are two spheres (impostors) moved
  and scaled round it by the model, plus the existing star sphere. Close to the planet the material adds procedural
  detail (wave glitter, cloud billows, the island from one small baked map), no big textures.
- Fire is a material on a shell round the portholes plus one flickering light; no particle systems that allocate per frame.
- The final descent is the real level seen from above: no second world.
- Budget: meshes under 150k triangles, textures under 24 MB, sounds under 12 MB, everything loaded only on Europa
  (`ULyudPrefetch`) and freed after the flight. Nothing per frame while the hero is elsewhere.

## Names

Folder and prefix: `Spacecraft`, `SC_` (`SM_SC_*`, `M_SC_*`, `T_SC_*`), content `/Game/Spacecraft`, data
`Content/Spacecraft/*.json`, C++ `Public|Private/Spacecraft/`, namespace `LyudFlight` (engine-free model), classes
`ALyudSpacecraft` (the cabin and all in it), `ALyudSpacecraftStage` (the landing stage on Europa),
`ULyudSpaceflightSubsystem` (the flight's director). Tools `tools/spacecraft/`, checks `tools/spacecraftsim/run.sh`,
Blender `blender/spacecraft/spacecraft_kit.py`, textures `textures/spacecraft/`, sounds group `spacecraft`
(`audio/sfx/spacecraft/`), strings `ui.flight.*`, `flight.*`, items `record_sc_*`, console `lyud.Flight.*`.

## Where things are

| Piece | Files | Notes for whoever works on it |
|---|---|---|
| Flight model (engine-free) | `Private/Spacecraft/FlightModel.h`, `tools/spacecraft/flight.json` -> `Content/Spacecraft/flight.json` | `tools/spacecraft/MODEL_NOTES.md` |
| Director, stage, camera, sound, records, save, console, F7 | `Private/Spacecraft/LyudSpaceflight.cpp`, `Public/Spacecraft/LyudSpacecraft.h` | `tools/spacecraft/ENGINE_NOTES.md` |
| Cabin: instruments, controls, record player, glass, fire shell, chute | `Private/Spacecraft/LyudSpacecraftCabin.cpp` | `tools/spacecraft/CABIN_NOTES.md` |
| Meshes (29 pieces, 57.6k triangles) | `blender/spacecraft/spacecraft_kit.py` -> `design/spacecraft/game/spacecraft_kit.glb`, `Content/Spacecraft/kit.json` | `blender/spacecraft/KIT_NOTES.md` |
| Textures and materials (26 textures, 11.5 MB) | `tools/spacecraft/textures.py`, `island_map.py`, `unreal/scripts/import_spacecraft.py/.sh` | `tools/spacecraft/MATERIAL_NOTES.md` |
| Sounds (46 ids, procedural, group `spacecraft`) | `tools/spacecraft/sounds.py`, `audio/sfx/spacecraft/`, `audio/sounds.json` "procedural" | `tools/spacecraft/SOUND_NOTES.md` |
| Words ru/en/ja, label atlases | `tools/spacecraft/strings.json` + `strings.py`, `labels.py` | `tools/spacecraft/WORDS_NOTES.md` |
| Records (8 open-licence recordings, fetched per machine, not in git) | `tools/spacecraft/records.json`, `fetch_records.py`, `sleeves.py`, `records_strings.py` | `tools/spacecraft/RECORDS_NOTES.md` |
| Splash point, stage spot | `tools/spacecraft/site.py` -> `Content/Spacecraft/spacecraft.json` | map (640, 100) m, about 500 m off the beach, 8.7 m deep at spring low |

Small hooks in other systems: `ULyudPrefetch::ReleaseFolder`, `ULyudSoundscapeSubsystem::LoadPool/UnloadPool` (the craft's
sounds are resident only while it is in use), `ULyudEuropaSubsystem::InstallSpaceObserver/GetSiteOrigin/GetSpaceRadius`,
`"spacecraft"` in `tools/gen_voice.py` `VOICED_SUBS`.

Console: `lyud.Flight 0` (leave it out), `lyud.Flight.Use` (= E), `.Report`, `.GoCraft`, `.GoCabin`, `.Launch`,
`.Phase <id>`, `.Fast <x>`, `.Reset`. F7: group «flight» (teleports to the craft and into the cabin, start, jump to a
phase, fast time, next record, reset).

## Checks (cloud, all green 2026-11-28)

`sh tools/spacecraftsim/run.sh` (23 131 checks of the model; `run.sh show` prints the flight minute by minute),
`python3 tools/spacecraftsim/cabincheck.py` (every kit key, mesh, material and parameter the cabin code names exists),
`python3 tools/spacecraft/{flight,strings,labels,textures,site,records_strings}.py --check`,
`python3 unreal/scripts/import_spacecraft.py` outside Unreal (HLSL lint), `python3 tools/check_devdata.py`.

## On the Mac (owed)

The steps are in `tools/spacecraft/ENGINE_NOTES.md` "Mac checklist": import the kit, textures, materials and sounds
(`unreal/scripts/import_spacecraft.sh`), optionally fetch the records (`python3 tools/spacecraft/fetch_records.py --pin`,
then commit the pinned `records.json` and sidecars), build, `sh unreal/scripts/test_spaceflight.sh`, then fly once by
hand with the sound on and bench frame and memory against main. What to look at first, and the one-line place to flip
each guessed sign or axis, is in ENGINE_NOTES "Unverified" and CABIN_NOTES.

## Open

- The real world starts only 364.5 m above the sea (a canopy ending near 7 m/s allows no more without retuning the
  chute curve); the island fills the porthole from orbit, but the last look at the real island is short.
- Records: licences were read on mirrors, the files themselves were never downloaded in the cloud; «Clair de lune»'s
  licence page could not be opened and the Rachmaninoff is CC BY-SA (see RECORDS_NOTES). The fetch script refuses a
  file whose licence on the host is not PD/CC0/CC BY/CC BY-SA.
- The hero's 10 lines and the automatic voice's 13 callouts have text and subtitles but no voice yet (`tools/gen_voice.py`,
  paid: fal).
- Not done: records as pocket items (no icons), the map markers, a diary rule for the flight
  (`tools/daylog/content.py`; the sentences exist), fans that turn, a quest that uses `flight launched` / `flight landed`.
- The maker's plate («Людмиловский приборный завод», 1968) and the found flight log are invented flavour, not canon.
