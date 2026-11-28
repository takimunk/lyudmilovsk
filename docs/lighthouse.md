# The Mayak lighthouse and its keeper

Backlog item 42 (`docs/interactives_backlog.md`). The lighthouse stands on the headland past the Mayak station
(`south.json` thing `lighthouse_1`, map (105, -426), white with two red bands, 24 m, the keeper's annex). This page is
the light, the ways up and down, the weather log, the gallery's view, the keeper and his quest.

## Files

| What | Where |
|---|---|
| Meshes | `blender/south/props_marks.py` `lighthouse` (lantern glass slot `M_South_LanternGlass`, hatch, gallery collision), `blender/south/props_light.py` (`lighthouse_lens`, `lighthouse_beam`, `picket_fence`, `meteo_screen`); `docs/south.md` "Lighthouse meshes" |
| Placement | `tools/south.py` `things()`: the fence ring (17 pieces, gate `picket_fence_1`), `meteo_screen_1` |
| Materials | `unreal/scripts/make_lighthouse_materials.py` (`M_South_LanternGlass`, `M_South_LensGlass`, `M_South_Beam`), called by `import_south.py` |
| Rules | `Private/South/LighthouseModel.{h,cpp}` (plain C++, namespace `LyudLighthouse`) |
| Engine | `Public/South/LyudLighthouse.h`, `Private/South/LyudLighthouse.cpp` |
| Game data | `tools/lighthouse/lighthouse.py` -> `Content/South/lighthouse.json`, `Content/Localization/lighthouse/strings_*.json` (words in `tools/lighthouse/strings_src.py`) |
| Keeper | `tools/town/lighthouse_keeper.py` -> `people.json` (`vetrov_gordey`), `town_places.json` (`S-MAYAK-*`, and the cape's ground: `ground_patches` id `mayak`, a 2 m height field; the walk grid stops north of the cape), `activities.json` |
| Quest | `Content/UI/quests.json` `lighthouse_paraffin`, `Content/Dialogue/npc_dialogue.txt` `[Vetrov_gordey]`, item `paraffin` (`LyudInventory.cpp`, hardware shop stock in `tools/city/content/furnished.json`) |
| Dev | `Content/Dev/teleports/lighthouse.json`, actions `lighthouse.lamp`, `lighthouse.fog` |
| Tests | `sh tools/lighthousesim/run.sh test` (light, fog, flash, log codes), `tools/peoplesim/run.sh test` (the keeper's day), `tools/audiosim` (horn every 30 s) |

Regenerate after a change: `python3 blender/south/build_south.py props_marks lighthouse` (and `props_light`), `python3
tools/south.py`, `python3 tools/lighthouse/lighthouse.py`, `python3 tools/town/lighthouse_keeper.py` (then the later town
post-steps: `south_people.py`, `town_night.py`, `town_activities.py`). All have `--check`. Never run `town_life.py` for this.

## The light

`ALyudLighthouse`, spawned at the manifest's `lamp` by `ULyudLighthouseSubsystem` when a level with the Aton coast starts.

- **Character:** the Fresnel lens turns once in 20 s; two bull's-eye panels give a white flash every 10 s.
- **When:** from 0.15 h before sunset to 0.2 h after sunrise (the town's ephemeris, lat 54.96), and by day when the
  visibility is under 1000 m. It warms up over 25 s and dies down twice as fast.
- **Beam:** two crossed additive cards (`SM_South_Lighthouse_beam`, `M_South_Beam`), length by scale only: 650 m on a
  clear night, down to 45 m in thick fog, dimmer in fog and rain (`BeamFor`).
- **One spot light** along the brighter beam: 2.2 Mcd, 600 m, no shadows, no indirect light, no volumetric scattering.
  All the parts are out of Lumen, distance fields and ray tracing.
- **The lantern glass** (the tower's `M_South_LanternGlass` slot) glows warm and flashes when a panel faces the camera.
- **Cost:** nothing is rebuilt per frame. One scene component turns; material scalars and the card's scale are set only
  when they change. By day the actor ticks once a second.
- **Foghorn:** the soundscape's fog signal (two blasts every 30 s) sounds from the lantern (`GetHornLocation`), falling
  back to the Mayak station in levels without the lighthouse.

## The gallery

Two `ALyudLighthouseStair` prompts: "up" at the tower door, "down" at the hatch on the gallery. E: a short fade, and the
hero stands at the other end. The tower is open 8:00 to 21:00; after the paraffin quest it is open at any hour. A locked
door says so. The way down is always open. Going up sends `reach lighthouse_gallery`.

On the gallery's sea side, E holds the binoculars' view (an `ALyudSouthSpot` "view" with no stand: the 1.1 m deck is too
narrow for the coin viewer's pedestal). When the telescopes land in main it can become a `scope_coin`.

## The log

`ALyudLighthouseLog`, the tin case on the weather screen's leg. E opens it on the city's page widget, today first; E
turns back a day, seven days in all; a step away shuts it. Each page has the date, the readings at 08, 14 and 20 that are
past (wind by compass and Beaufort, sky, temperature, pressure tendency, visibility class, from
`ULyudWeatherSubsystem::GetStateAt`), when the lamp was lit and put out, the horn's hours, and the keeper's remark on
past days (gale, thunder, fog, ice, frost, calm or plain). Quests hear `read lighthouse_log`.

## The keeper

Гордей Ветров (`vetrov_gordey`, body `Vetrov_gordey`, 67), a story person who lives in the annex (`S-MAYAK-HOME`). His
duties, every day:

| Activity | When | Where |
|---|---|---|
| `lamp_out` | sunrise +0 to +0.5 h | the tower (hidden) |
| `weather_obs_8` | 8:00 to 8:12 | the weather screen |
| `lens_polish` | 9:00 to 10:45 | the tower |
| `weather_obs_14` | 14:00 to 14:12 | the weather screen |
| `lamp_light` | sunset -0.5 to +0.5 h | the tower |
| `weather_obs_20` | 20:00 to 20:12 | the weather screen |
| `lamp_watch` | 21:45 to 23:15 | the tower |

Sun-tied duties (`"sun": "set" | "rise"`, hours as offsets) are new in `PeopleSim`: `FWorld::SyncSunDuties` turns them into
clock hours whenever the day's sunrise or sunset changes. Between duties he sits on the bench by the fence.

## The quest

`lighthouse_paraffin`: talk to the keeper; he is out of paraffin and has not seen a paper in a week. Buy paraffin at the
hardware shop (25 rubles) and today's «Людмиловская правда» at a kiosk, bring both. Reward 40 rubles, and the tower is
open at any hour.

## Owed on the Mac

The meshes and materials through `import_south.sh`, the keeper's stand-in body (residents generator), `icon_paraffin`,
the C++ build, night shots from the town pier and the beach (dev teleports `lighthouse_night_town`,
`lighthouse_night_beach`), a bench with the beam on, the keeper's voice lines (paid).
