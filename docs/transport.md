# The cable-car metro of Lyudmilovsk

Cable cars instead of a metro: lines of ropes over the town, stations high on towers with a lift and a stair down, egg-shaped
four-seat cabins that only run when somebody needs them and go home to their depot after. Built 2026-10-25 (worktree
`transport`, branch `wt/transport`). The skill for working on it: `.claude/skills/transport/SKILL.md`.

## The look

A 1960s retro-futurism: Soviet space-age modernism (VDNKh's Kosmos pavilion, Ostankino, the Vorobyovy Gory chairlift,
Chiatura's cabins) crossed with American Googie (Disneyland Skyway, Seattle 1962, the Palm Springs tramway, LAX's Theme
Building). One family language: ivory enamel and concrete, an amber-orange belt, chrome, terrazzo, mosaic. Concepts and the
references in `design/transport/concepts/` (`sources.json` pins every generation; `.jpg` previews committed, the 2K PNGs local).

- **Cabins**: an ivory egg with the amber belt, a panoramic glass band, a door with a porthole, a hanger arm to a twin-wheel grip,
  an amber roof lantern; two facing benches inside (4 people). Every cabin has a number and the name of a bird (the Sun line's
  are land birds, the Sea line's sea birds): `transport.cabin.c01..c12`.
- **Stations**: one tower family, each with its own crown: Aton the swept wing (on the hill, no tower), Akademgorodok the
  ribbed dome, Zenith the flying saucer (the change), Polar Star the star spire, Lighthouse the amber lantern, Amber Shore the
  parabolic arches, Horizon the folded roof.
- **Pylons**: an ivory rocket mast with amber bands and four splayed feet; at corners an A-frame lattice with a curved
  deflection sheave and a golden star.

## The network

| Line | Stops | Depot |
|---|---|---|
| 1 Sun (amber) | Aton (club) → Akademgorodok → Zenith → Polar Star | Polar Star |
| 2 Sea (teal) | Lighthouse → Amber Shore → Zenith → Horizon | Horizon |

Authored in `unreal/Lyudmilovsk/Content/Transport/network.json` (map frame of `tools/world_map.py`: metres, X west, Y north):
stations (where, the deck's height over the map's zero or `"ground"`, the crown style, the place on the route diagram),
lines (stops in order, corner pylons between two stops as `"via"`, the depot), the cabins, the club's route board.

`python3 tools/transport.py` resolves it against the terrain (`terrain.ground_z`) into `network_built.json`; **run it after any
change of the relief, the map or the network** (build_scene01.sh does, after the terrain):

- A station's deck stays at its fixed height; the lift and the stair are as long as the ground under it leaves (raise the
  ground: shorter lift). A deck that would come within 4 m of the ground is raised (reported). `"ground"`: the platform 3 m
  over the hill on a short stair (Aton).
- A station on two lines has a level per line, 6.5 m apart (Zenith); the lift serves the ground and the top deck, the stair all.
- Pylons appear where the sagging rope would come within 9 m of the ground (less near a station on the ground) or a span is
  over 240 m, as tall as needed (8..45 m). Corners are pylons where a line turns.
- Each line is one loop of rope: out on one track (9 m apart), round the last station's wheel, back on the other. Cabins stop
  5.5 m past a through station's centre, 2.5 m before an end station's wheel; the stair and lift stand in the middle (an end
  station: 7 m from the wheel).
- `--plot map.png` draws it; `--check` fails on any problem.

The map uses it too: `world_map.STATION_UP/DOWN` (Aton, Akademgorodok) come from network.json, `world_map.cable_car()` from
network_built.json (stations and pylons are kept flat and free of trees). The old placeholder cable car in world_mesh.py is gone.

## The game

`ULyudTransportSubsystem` (Public/Transport/LyudTransport.h) starts by itself in Lvl_Scene01 (`lyud.Transport 0` off, `2` in any
level), reads network_built.json, builds everything in code (`ALyudCableway`: instanced kit and the hi3d meshes, so nothing is
placed in the level), spawns the cabins (`ALyudCabin`) and runs the network:

- **Simulation**: plain C++ `Private/Transport/CablewaySim.{h,cpp}`, run outside the engine with `tools/transportsim/run.sh test`.
  A trip (`Travel(who, from, to)`) plans one or two legs (a change at the shared station), puts the rider at the stop, and the
  dispatcher sends the cabin that gets there soonest: one parked at a station, one already going that way with room, or one
  out of the depot (it waits for a clear rope). Cabins stop only where someone gets on or off, run 6 m/s with people and
  11 m/s empty, slow to 3 m/s through stations, keep 28 m apart, dwell 14 s (longer while someone stands in the door). With
  nobody to carry they wait 90 s and go home; one cabin may wait at each line's far end. Nobody asking: nothing moves.
- **Player**: at a station's call panel (E) the route diagram opens; click where to go. The board over the panel shows each
  way's wait ("cabin in 2 min", or "on call ≈ 3 min" when none is coming). Walk in when the doors open; the cabin's board
  says where it goes and the next stop; announcements (voice + a note) on the way; walk out at the stop. Staying aboard: it
  carries you on to the next station. The lift: stand in it a moment and it goes; it comes when you wait at its door.
- **People** (UPeopleLifeSubsystem): their ride between the valley and the club's spur is a real trip now (`PeopleSim`
  `RideHook`/`EndRide`); they wait on the platform and sit in the cabin, then walk on from the other station.
- **Boards**: the enamel route board by the club's exit (E: the diagram, cabins live), each platform's wait board, the
  station's name over its entrance, the cabin's number and name on its sides; the map tab (M) draws the lines, stations and
  moving cabins.
- **Sound** through `ULyudSoundscapeSubsystem` (`PlayEffect`, `AttachLoop`, `PlayVoiceAt`): the cabin's motor while it
  moves, doors, the station chime, the lift; voices `line.transport.next_<station>.1`, `arrive_<station>.1` (speaker
  `transport` in localization/voice.json).

Console: `Transport.Report`, `Transport.Ride <from> <to>` (the player onto a platform, a cabin called), `Transport.Call <from>
<to> [who]`, `Transport.Diagram [station|close]`, `lyud.Transport.Speedup <x>`.

## Assets

- 3D: ten Hi3D v3.0 models from the Nano Banana renders (`design/transport/gen.json`, sidecars `gen/<step>.json`; made on fal,
  see the rule below), optimized by `blender/transport/optimize.py` (cabin 14k triangles with its door side cut open, line
  pylon split into feet / shaft / head so only the shaft stretches, corner pylon scaled whole, crowns 22k at 21 m; 1-2k
  textures) and packed by `pack.py` into `design/transport/game/transport_kit.glb` (local), sizes in `Content/Transport/kit.json`.
- `unreal/scripts/import_transport.sh`: the meshes and materials (`import_transport.py` -> /Game/Transport), the sounds
  (`IMPORT_ONLY=sfx/transport/ import_audio.py`), the voices (`IMPORT_ONLY=line.transport. import_voice.py`).
- Words: `localization/transport_strings.py` appends the 62 keys to the three tables (rerun after a strings merge).
- **Rule (Dima)**: images through the Higgsfield MCP, as one grid image cut into frames, gpt sunburst 2.5 medium 4K transparent
  for 3D references; 3D only through the hi3d MCP; voices and sounds through fal; every hi3d model optimized in Blender.

## Tests

- `tools/transportsim/run.sh test`: nothing runs unasked, Aton -> Akademgorodok and back, cabins go home, a change of lines,
  40 random trips (all arrive, never closer than 8 m).
- `tools/peoplesim/run.sh test` stays green with the ride hook.
- `unreal/scripts/test_transport.sh`: offscreen in Lvl_Scene01: the board, the diagram, the map tab, stations from the air,
  a ride Aton -> Akademgorodok; screenshots in Saved/TransportTest.

## Not done yet / ideas

Station interiors (kiosks, benches, mosaics are only on the crowns), a ticket or token, night lighting of the stations,
cabins swaying in the wind (ULyudWindSubsystem), the depot hall's inside, more lines as the town grows.
