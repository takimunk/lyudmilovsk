# The sea railway

A single track comes out of a tunnel in the sea cliff just south of the club, crosses the beach path at a level
crossing, stops at the beach halt on piles at the water's edge and runs straight out to sea along a low bank, its rails
a hand's breadth under the water at high tide (as in *Spirited Away*), to a small island 2.3 km off (the island halt),
and on, past the island, to the horizon, where the train fades into the haze; then it comes back. One old two-car train
of a Showa-era Japanese local line runs it, every hour, day and night. The setting sun stands over the line's far end.

## Files

| What | Where |
|---|---|
| The line, authored | `unreal/Lyudmilovsk/Content/Railway/railway.json` |
| Resolved against the ground (per checkout, not committed) | `python3 tools/railway.py` → `Content/Railway/railway_built.json` (`--plot` profile, `--check`) |
| Timetable and motion, plain C++ | `Source/Lyudmilovsk/Private/Railway/RailRun.h/.cpp`; outside the engine `tools/railsim/run.sh test|table` |
| Game: subsystem, track, train, crossing, wake | `Public/Railway/LyudRailway.h`, `LyudRailTrack.h`, `LyudTrain.h`, `LyudLevelCrossing.h`, `LyudTrainWake.h` |
| The kit (train, crossing, shelter, nameboard, mast, sleeper, island) | `blender/railway/rail_kit.py` → `design/railway/game/rail_kit.glb` + `Content/Railway/kit.json` (seats, doors, lamps) |
| Sounds (synthesized, no service) | `tools/railsim/rail_sounds.py` → `audio/sfx/railway/*.wav` |
| Import | `unreal/scripts/import_railway.sh` (kit → `/Game/Railway/Meshes/rail_kit`, sounds → `/Game/Audio/Railway`) |
| Offscreen check | `unreal/scripts/test_railway.sh` (`RAILTEST_ONLY=day,night,far`, or `riders`: residents ride back from the island; `trees`: the island's plants up close) → `Saved/RailwayTest/*.png` |

## The line

Map metres (`tools/world_map.py`'s frame: X west to the sea, Y north). The line runs along +X at Y = -60; *s* is the
distance along it from the tunnel's dark end (map x -160).

| Place | s | map x | Rail head |
|---|---|---|---|
| The tunnel's dark end (the train waits here, unseen) | 0 | -160 | -41.35 |
| The mouth: a concrete headwall in the cliff's foot, a hood over the bore | 283 | 123 | -41.35 |
| The level crossing (the beach path) | 289.5 | 129.5 | ~1 m over the sand |
| The beach halt: a timber deck on piles, a ramp down to the path by the crossing | 342 | 182 | sea + 0.47 |
| The bank goes under the water (2 % down from the mouth) | ~370 | 210 | sea - 0.3 (high tide) |
| The island halt (the island levelled along the track) | 2564 | 2404 | sea + 1.0 |
| The train is gone into the haze | 18660 | 18500 | sea - 0.3 |
| The line's end (the train lies over, then comes back) | 20160 | 20000 | |

The rail head stands 0.3 m under the sea level of the map (`SEA_Z`, which is high water: the tide thread's tide only
lowers the water), so at high tide the rails are just under the surface and at low tide the bank stands dry. Everything
that reads the water reads `ALyudSea` at run time (SampleSurface), never a fixed height.

The bank: ballast crest 5.2 m wide, armour stones at 1:1.5 down to the bed (or the beach); masts every 45 m on the north
side with the contact wire 5.3 m over the rails and its messenger sagging between them; sleepers to a little past the
island. The bank's chunks near the coast and the island carry the component tag `LyudSeaBed`, which `ALyudSea`'s depth
map counts as the bed, so the waves shrink and break over the bank and round the island. One can walk along the crest.

The tunnel's mouth needs a hole in the landscape under its hood: `tools/terrain.py`'s `railway_holes()` reads
`railway_built.json`'s `terrain_holes` (`build_scene01.sh` runs `tools/railway.py` before the terrain).

## The timetable

`RailRun` simulates one way at a time (accelerate 0.45 m/s², brake 0.6, 16 m/s on the bank, 7 by the tunnel, the
crossing and the beach, 9 on the island; 45 s at each halt, the doors open 2.5 s after it stops and shut 6 s before it
leaves) and plays the period on the game calendar (`ULyudRailwaySubsystem::NowSeconds`: seconds since 1 January 2026
on the calendar, plus a test offset). A run: out of the tunnel at minute 0, beach halt, island halt (~4 min later),
gone into the haze at ~24 min, back at the island at ~48 min, the beach, into the tunnel. A single train: the period
(3600 s) stretches if the round trip doesn't fit.

API for people and tests: `NextDoorsOpen(stop, dir, after)`, `RideSeconds(a, b, dir)`, `GetBoardingPoint(stop, slot)`,
`GetStopEntrance(stop)`; `ALyudTrain::GetSeat(i)` (the seat's frame: origin on the floor under its front edge, X the way
the sitter faces), `GetDoorPoint`, `IsInside`; `OnTrainEvent` (Depart, Arrive, DoorsOpen, DoorsClose, Appear, Vanish).

Console: `lyud.Railway 0|1|2`, `lyud.Railway.Report`, `lyud.Railway.Next <stop> [dir]` (moves the railway's clock so
the doors open there in 20 s), `lyud.Railway.Clock <s>`.

## The train

Two 16 m cars (`rail_kit.py`): cream over deep green with an orange belt, a flat cab front with two round headlamps low,
red tail lamps over them and a lit destination box, a diamond pantograph on car 1's roof, Showa lantern vents, two
doorways a side (clear 1.20 x 2.15 m: the character standard's 1.08 x 2.10 at least), wine-red moquette benches along
the walls (46 seats), stanchions, straps, a luggage rack, ceiling fans and round lamps; the cab's desk behind a
partition. The game swaps the glass and the lamps for its own materials: the cabin lamps glow warm from dusk (three
point lights a car), the leading cab's headlamps light and its beam reaches ahead in the dark, the trailing cab shows
red. The doors on the platform side (south) slide open at the halts.

The player walks in through an open door and rides: the car carries the pawn (as the cable car's cabins do); while the
player is aboard the train stays drawn even out in the haze.

Sounds: the motors' whine pitched with the speed, the rumble of the wheels, *ta-tan* over every rail joint (25 m), the
horn coming out of the tunnel and leaving a halt, the doors.

## The water and the gulls

`ULyudTrainWake`: at each wheel the sea is sampled; where the surface stands over the rail head (`bIsSea`, still-water
depth over the bank > 0, the waves' surface within 6 cm of the rail or over it) the wheel throws spray to both sides
(droplets fly and fall back as foam), the bank's flanks shove a V of foam that drifts outward and fades, and a hiss of
water follows the front bogie. Off the water (the beach, the island, a low tide) nothing. Only within 250 m of the camera.

Gulls (the bird species' own `ABirdActor` herring, black-headed and common gulls) gather over the last car while it
runs through the water by day: they hang in slow figure-eights behind it, glide with a few wingbeats now and then, call
(the species' own calls), and peel off and climb away when it stops or leaves the water.

## The level crossing

Where the beach path crosses the line by the tunnel's mouth: a post either side (yellow crossbuck, two red lamps that
blink in turn, the bell), barriers that come down across the path, a yellow plate `rail.sign.caution` in the player's
language (ru «Осторожно, поезд», en «Caution: train», ja «列車に注意»), rubber decking over the rails and timber ramps from
the sand. It warns 25 s before the train's front reaches it, the barriers come down 5 s into the warning (8 s to
fall), and everything stops when the tail has cleared it. The bell is the anime *kan-kan*: a struck electric gong
twice a second (synthesized).

## The halts

A timber deck on piles (3.2 m wide, 1.10 m over the rails, its edge 1.55 m from the centreline, a white edge line), a
railing at the back, a small wooden shelter with a bench open to the track, the nameboard (the halt's name big, the
neighbours either way: `rail.stop.*`) as a widget in the player's language, two lamp posts lit from a little before
sunset. The beach halt's ramp comes down to the beach path by the crossing; the island halt's to the island's levelled
ground.

## People on the train

The Academgorodok's residents ride it (docs/people.md): they walk to a halt's ramp, up onto the deck to a boarding
point (`GetBoardingPoint`), wait facing the track (asking the timetable again every 10 s, so they follow
`lyud.Railway.Next`), get in when the doors open and are shown seated on a free seat of the car while the train is
drawn; at the other halt they get off and go down its ramp. Checked offscreen (`RAILTEST_ONLY=riders`): two residents
ordered home from the island on Tuesday 10:41 wait at the island halt, take the 10:47 back, sit in the car (seen inside
and through the windows) and come out at the beach halt by the crossing.

## Open

- The landscape hole at the mouth needs a scene rebuild (`build_scene01.sh`) in a checkout to show.
- The wheels' foam is simple see-through discs and droplets; a real water simulation (a wake in the sea's own surface) is open.
- Gulls are actors of their own near the train (not birds of the bird life's simulation).
- The train is one set on a single track; a passing loop at the island would allow two.
