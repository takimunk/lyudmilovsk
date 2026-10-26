---
name: transport
description: How to work with the Lyudmilovsk cable-car metro (stations, lines, pylons, cabins, depots, the route diagram, people riding). Use whenever you add or move a station or a line, change the relief or the map near the cable car, move the club, add a cabin, touch the boards, the diagram or the cable car's sounds and words, or need NPCs or the player to ride.
---

# The cable-car metro

Full design: `docs/transport.md`. The network is **data**; the stations, pylons, ropes and cabins are built in code at play
from it. Never place cable-car meshes in a level and never hand-edit heights.

## Change the network

1. Edit `unreal/Lyudmilovsk/Content/Transport/network.json` (map metres of `tools/world_map.py`: X west, Y north):
   - a station: `{"id", "name": "transport.station.<id>", "at": [x, y], "deck": <metres over the map's zero> | "ground", "style": wing|dome|saucer|star|lantern|arches|folded, "schema": [col, row]}`;
     pick a deck height 8..30 m over the ground you see in `--plot`; the resolver raises it if the ground comes within 4 m.
   - a line: `{"id", "name": "transport.line.<id>", "color": "#RRGGBB", "number", "stops": [ids in order], "via": {"<a>-<b>": [[x, y], ...]}, "depot": <end station>}`;
     a `via` point is a corner pylon; keep a line's turn in a station under 75 degrees.
   - a cabin: `{"id": "cNN", "number", "name": "transport.cabin.cNN", "line"}` (each cabin is named after a bird).
2. `python3 tools/transport.py --plot /tmp/net.png` (writes `network_built.json`; read the PROBLEM lines; look at the picture).
3. `tools/transportsim/run.sh test` (the simulation on the real network) and `tools/peoplesim/run.sh test`.
4. New words: add rows to `localization/transport_strings.py` (ru/en/ja) and run it; voices for new announcements:
   `python3 tools/gen_voice.py --lang <l> line.transport.<what>.1` (fal), then `unreal/scripts/import_transport.sh`.
5. Commit `network.json`; `network_built.json` is made per checkout from its terrain (not committed; build_scene01.sh and import_transport.sh run transport.py).

## The relief or the map changed (or the club moved)

Rerun `python3 tools/transport.py` after the terrain is built (build_scene01.sh does it): lifts, stairs and pylons follow
`terrain.ground_z`. Moving the club: move the `aton` station's `at` and the `club_board`; `world_map.STATION_UP` follows by itself.
The map keeps stations (17 m) and pylons (4 m) flat via `world_map.keep_flat` and `world_map.cable_car()`.

## In the game

- `ULyudTransportSubsystem` (Public/Transport/LyudTransport.h): `CallCabin(from, to)`, `WaitAt`, `GetStations/GetLines/GetCabins`,
  `OpenDiagram`, `BeginRide/RiderPose` (people), `OnTransportEvent` (Launch, Arrive, Depart, Board, Alight, Done, Park).
- Simulation rules live in `Private/Transport/CablewaySim.cpp` (plain C++): change them there and test with transportsim first.
- Stations (docs/transport.md "Stations v2"): the vestibule (vestibules.json v2) on its tower between the tracks spread to
  its gap G; geometry in `Private/Transport/LyudStationBuild.cpp` (tower core, kit stacking, sheave posts, wheel, depot,
  panels) with `LyudMeshKit` (metre UVs, box collision, textures `Mat("tx_<name>")`); the rope spread in transport.py
  (`geo` per station in network_built.json). Kit sizes in `Content/Transport/kit.json` (`blender/transport/optimize.py`).
- Sounds go through `ULyudSoundscapeSubsystem` (`PlayEffect`, `AttachLoop`, `PlayVoiceAt`); new sfx in `audio/sounds.json`
  (group `transport`), `tools/gen_sounds.py --sfx <ids>`.
- Doors in stations use `ALyudDoor` (docs/doors.md); the cabins' own sliding doors are part of `ALyudCabin`.
- Console: `Transport.Report`, `Transport.Ride <from> <to>`, `Transport.Call <from> <to> [who]`, `Transport.Diagram [station|close]`.
- Check offscreen: `unreal/scripts/test_transport.sh` and `unreal/scripts/test_stations.sh` (views, probes, a lift ride on
  every station; `STATIONS=a,b`) (never a windowed worktree editor).

## New 3D for it

Images: Higgsfield MCP, all objects in ONE grid image (gpt sunburst 2.5, medium, 4K, transparent background), cut into frames.
3D: only the hi3d MCP (upload_input, quote_generation, create_generation). Then `blender/transport/optimize.py` (decimate to the
budget, textures 1-2k, game frame), `pack.py`, `unreal/scripts/import_transport.sh`. Pin every request in a sidecar.

## Never
Hard-code a station's height or a pylon; draw words into textures (boards are Slate text from the string tables); let empty
cabins run without a reason; put more than one Interchange scene import in one UE process.
