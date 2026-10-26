# The cable-car metro of Lyudmilovsk

Cable cars instead of a metro: lines of ropes over the town, stations high on towers with a lift and a stair down, egg-shaped
four-seat cabins (people really ride inside them) that only run when somebody needs them and go home to their depot after. Built 2026-10-25 (worktree
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

`python3 tools/transport.py` resolves it against the terrain (`terrain.ground_z`) into `network_built.json` (not committed: each checkout makes its own from its terrain); **run it after any
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

- **Motion**: every free span of rope between supports (`network_built.json` `spans`) moves: its first two modes up and down
  are springs loaded by the cabins on it (a cabin sags its span ~0.6 % of its length, more with people aboard), the first
  sideways by the wind (`ULyudWindSubsystem`); periods of several seconds, lightly damped. The cabins hang from the moving rope
  and swing on their hangers (back as they pull away, forward as they stop, sideways in the wind, under 5 degrees). Ropes
  within ~900 m of the viewer are redrawn every frame. (`FNetwork::StepMotion`, `RopeOffset`; tested in transportsim.)
- **Vestibules**: at the foot of every station a glass pavilion round the stair and the lift, built in code and furnished by
  hand: double doors (`ALyudDoor`), the ticket kiosk, benches, the live route diagram (E: pick where to go), a mosaic of the sun
  over the sea, a station clock with the game's time, lamps, a rubber plant or a monstera (`AProceduralPlant`) in a pot, a bin;
  the station's name over the door.

Console: `Transport.Report`, `Transport.Goto <station> [vestibule|platform|outside]`, `Transport.Ride <from> <to>` (the player onto a platform, a cabin called), `Transport.Call <from>
<to> [who]`, `Transport.Diagram [station|close]`, `lyud.Transport.Speedup <x>`.

## The cabin

The hi3d egg is made a room people really ride in by `blender/transport/cabin.py` (run by `optimize.py -- cabin`, no new
generation). Nothing is cut out of the generated triangles themselves (after decimation they are big, crooked and
coloured by a texture that doesn't follow them: cutting windows by texel colour per triangle gave ragged edges and
shards inside):

- **A clean shell**: a sphere round the room's middle with every vertex moved along its ray onto the generated model's
  outer skin (rays from outside in), even quads, scaled to 4.46 m so the standard door fits under the roof. The
  model's colour and relief (trims, seams, handles) are **baked** onto it from the full 300k-triangle original
  (Cycles, selected to active: `cabin_color` + `cabin_normal`, 2k).
- **A real wall**: the shell solidified 4.5 cm inward, closed; its inner face the vinyl lining (`TX_CabinWall`).
- **Windows**: where the generated texture is window (dark, dull texels), grouped into windows, each outlined smoothly
  (convex outline, rounded, inset 2 cm) and cut through the wall with an exact boolean; the reveals are chrome
  (`TR_Chrome`); the glass (`TR_Glass`) is the outline filled and laid on the hull half a wall in, both ways.
- **The door**: a 1.10 x 2.12 m opening (`CharacterStandard`) with rounded top corners on the door painted in the
  texture (found by its frame stripes), cut the same way; the leaf (`SM_TR_CabinDoor`) is the same outline
  intersected with the wall, so it fits the opening exactly; it steps out 8 cm, then slides 1.14 m back along the side.
- **Textures inside**, no generation: procedural materials baked in Blender into colour + normal images on their own
  UVs (`TX_CabinWall` cream vinyl lining with panel joins, `TX_CabinFloor` terrazzo, `TX_CabinSeat` pleated amber
  leatherette, `TX_CabinBench`, `TX_CabinDoorIn`); the game keeps `TX_*` materials as imported.
- Inside (`SM_TR_CabinInside`): a terrazzo floor fitted to the hull, two padded benches facing each other at the ends
  (fitted by rays, wide enough for two each), a ceiling lamp, chrome grab rails. The hanger, arm and roof lantern stay
  the generated mesh, decimated (`SM_TR_CabinHanger`), the arm bent so the grip is plumb over the floor's centre.

Every part has its origin on the floor's centre under the grip (rope along X, door on Blender +Y = Unreal -Y, the left of
the way it goes). `kit.json` `cabin` holds what the game needs: `grip` (rope over the floor, 4.01 m = `network.json`
`rope_over_deck`, keep them equal), `door` (x = +0.47 m ahead of the centre, clear size, `sill_y` 1.007 m = the hull at
floor level by the door, slide, out), `sockets` (four seats with their facing, lamp, board, name plates), `collision`
(floor, walls round the room with the door gap, lintel, roof, benches: invisible boxes in `ALyudCabin`). The platform
side of this (island edge 3.43 m from the station's centre, nothing over the deck between 3.13 and 3.43 m, stop marks
at the door) is in the platform spec. Budget: shell ~22k triangles with its wall and glass, door 1.2k, hanger 1.4k, inside 0.7k;
2k colour + 2k normal. Check: `TR_DEBUG=<dir>` saves `cabin.blend`; `SCRIPT=test_cabin.py
unreal/scripts/test_transport.sh` walks the player in through the door, rides, walks out.

## Station layout v2: the vestibule on its own tower (2026-10-26)

Dima (2026-10-26): a station is its unique vestibule up at the platform's height, on a tower in the same style with the
lift and the stair inside, and a compact boarding platform built into the vestibule. (v1.1, the vestibule on the ground
beside the line with a gallery under the track, is gone.)

```
   track ==\                                        /==  the two ropes spread round the vestibule (track gap G)
            \=====[ edge  .. stop ..  edge ]=======/       boarding edge at y = -(G/2 - 1.05 m)
                  [  terrace   ( room + core )  ]          the vestibule, floor at deck level
            /=====[ edge  .. stop ..  edge ]=======\       boarding edge at y = +(G/2 - 1.05 m)
   track ==/                 ||                     \==
                             ||  the tower: foot, shafts, neck (a kit per style), the lift and the stair inside
                           [door]  the street door at the foot
```

**Station frame** (UE cm): the origin at the boarding floor (deck level) on the line's centre line midway between the
tracks; +X the line's way out, +Y right of it, Z up. `SM_TR_Vestibule_<Style>` is authored in it (Blender (x, y, z) m ->
UE (x*100, -y*100, z*100)).

The vestibule (blender/transport/vestibules.py, our own clean model; hi3d only the shape and the bake source):
1. Turned so the line runs across its narrowest way (no longer than 22 m along X); the terrace gets a straight
   **boarding edge** along each track at y = +-(G/2 - 1.05 m), edge_length long, a tactile line along it, the railing
   0.45 m in and open over the stop zone (4.5 m + 1 m each side round `stop_x`, put where both edges are clear of
   canopy feet). Nothing above the floor within 1.35 m of a track's centre.
2. The **tower's core** comes up through `floor_hole` (5.2 x 5.2 m) on the line's axis at `tower_x`: in the middle of
   the hall (all seven now: the small rooms were made 5.4 m in radius for it), the lift door and the stair landing
   facing `core_exit`. The core's own walls stand in the hole (the procedural part).
3. The hall: 20 cm walls, windows, the Double door hole(s) (`doors`), floor, ceiling; at most 16 m tall.
4. Collision complex as simple; slots: baked outside (slot 0), `TR_Glass*`, `TR_Interior*`, `TR_Terrazzo`, `TR_Chrome`,
   and the tiled surfaces `TR_Tx_<Concrete|Enamel|Amber|Mosaic|Tactile>` on metre UVs (KitMaterials: -> `tx_<name>`).

The tower kit (blender/transport/towers.py, called by vestibules.py; clean meshes, metre UVs): per style
`SM_TR_Tower_<Style>_Foot` (3.5 m, the street door, a Double hole on `tower.door.side`), `_Shaft3` and `_Shaft1`
(stacked, largest first, never stretched), `_Neck` (1.5 m, flares out; its top is the vestibule floor's underside at
-60 cm). Every piece's origin is on the tower's axis at its own bottom; inside a clear zone of `tower.clear`
(5.26 m square; the circle styles 7.44 m). Styles: ribbed concrete (dome), 12-sided glass with ribs (lantern), ringed
with portholes (saucer), zigzag folded plates (folded), an 8-point star (star), square with pilasters and slits
(arches), square with fins (wing).

**Zenit**: the saucer's tower carries a second, lower deck for the Sun line (`tower.decks`: `SM_TR_Tower_Saucer_Deck2`,
a 3.5 m piece of the tower with an opening out onto a ring deck at its own `axis_yaw`, `floor_z` -650 under the
saucer's deck, its own `track_gap`, `edge_length`, `stop_x`, `panels`).

`Content/Transport/vestibules.json` v2, per style (UE cm, station frame): `mesh`, `tris`, `height`, `length`,
`track_gap`, `edge_length`, `stop_x`, `panels` ([{at, yaw}], one per edge), `tower_x`, `floor_hole` [x0, y0, x1, y1],
`core_exit` (+x|-x|+y|-y), `core_in_room`, `footprint`, `bounds`, `centre`, `doors` ([{at, yaw, hole}]), `room`
({centre, radius, height, box}), `spots` (kiosk, bench, diagram), `tower` ({foot, shafts, neck: {mesh, h}, clear,
door: {side, hole}, neck_top, decks}).

Checks: `blender/transport/check_vestibules.py` floods a player's walk from the core's exit: the hall, the diagram, the
inside of the door and both edges at the stop must be reached; nothing in a track's keep-clear; 22 m, 16 m.
`TR_DEBUG=<dir>` pictures. The ropes' spread, the stops and the tower's stacking are the procedural part's
(tools/transport.py, CablewaySim, LyudStationBuild.cpp).

## Assets

- Windows: `optimize.py` reads each model's base colour under every face: dark, dull texels (and the lantern's amber) become a
  second material slot `TR_Glass*`, which the game replaces with real glass (M_TR_Glass); a copy of the shell turned inward,
  drawn a little in, without the windows, gives the room inside (`TR_Interior*`, plaster) with a floor in the crowns. Check the
  split with `TR_DEBUG=<dir>` (pictures with the glass red, the room blue).
- 3D: ten Hi3D v3.0 models from the Nano Banana renders (`design/transport/gen.json`, sidecars `gen/<step>.json`; made on fal,
  see the rule below), optimized by `blender/transport/optimize.py` (the cabin made a room by `cabin.py`, see above, line
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

A ticket or token at the kiosk, night lighting of the platforms, furniture in the crowns' rooms (they are reachable only by
eye now), the depot hall's inside, more lines as the town grows.
