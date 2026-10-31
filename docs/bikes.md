# Bicycles

A classic 1980s steel road racer (a "Старт-Шоссе"-like lugged frame, drop bars, 700c wheels, 52/42 rings, quill pedals
with toe clips, a kickstand) the hero and the Academgorodok's residents ride. One stands by the club's door, one by the
Academgorodok's lower cable-car station, and one in the lobby of each of six entrances of the Academgorodok's houses:
those belong to households living up that stair.

## Files

| What | Where |
|---|---|
| The model (clean game mesh, built from primitives) | `blender/bikes/build_bike.py` -> `design/bikes/game/bike_kit.glb` (local) + `Content/Bikes/bike.json` (measures, pivots, sockets) |
| Import (parts, materials, LODs, sounds) | `unreal/scripts/import_bikes.sh` (`import_bikes.py`; sounds by `import_audio.py`) -> `/Game/Bikes` |
| Sounds (synthesized) | `tools/bikesim/bike_sounds.py` -> `audio/sfx/bikes/*.wav` -> `/Game/Audio/Bikes/S_*` |
| Where the bikes stand | `Content/Bikes/bikes.json` (free-standing ones by position, entrances by stair id) |
| The bike actor (parts, pose, sounds, save) | `Public/Bikes/LyudBike.h`, `Private/Bikes/LyudBike.cpp` (`ALyudBike`, `FLyudBikeGeom`) |
| A body on a bike (hips, feet, hands by IK) | `Public/Bikes/LyudBikeRider.h`, `Private/Bikes/LyudBikeRider.cpp` |
| The hero riding | `Public/Bikes/HeroBike.h`, `Private/Bikes/HeroBike.cpp` (`UHeroBikeComponent`) |
| Spawning, residents' bikes, test commands | `Public/Bikes/LyudBikeSubsystem.h`, `Private/Bikes/LyudBikeSubsystem.cpp` |
| Residents' bikes in the life sim | `Private/People/PeopleSim.h/.cpp` (`FWorld::Bikes`, `PlanBike`, `BikeStop`) |
| Checks | `tools/peoplesim/run.sh test` ("bicycles"), `unreal/scripts/test_bikes.sh` (offscreen shots) |

## The model

`build_bike.py` (Blender 5.x, headless) makes every part from tubes, sweeps and lathes, one mesh object per moving part,
each with its origin at its pivot: `SM_Bike_Frame` (frame, saddle, seatpost, derailleurs, bottle cage), `SM_Bike_Steer`
(fork, stem, bars, levers, front brake: turns about the steering axis), `SM_Bike_WheelF`, `SM_Bike_WheelR` (with the
freewheel's cogs), `SM_Bike_Crank` (both arms and the rings), `SM_Bike_PedalL/R`, `SM_Bike_Chain` (a band whose U runs one
per link pitch: `M_BikeChain` draws the links and scrolls them), `SM_Bike_Stand`, and a rack `SM_BikeRack`. No words or logos
on it. The frame's paint is recoloured per bike (`bikes.json` paints). Its build lints every part for overlapping
coplanar faces and UVs (`blender/railway/kit_lint.py`) and must come out clean. Frame of `bike.json`: Unreal cm, X forward,
Y the bike's right, origin on the ground under the bottom bracket.

## Riding (the hero)

On foot, looking at a bike within reach: **E** gets on (from its left: hands to the bars, the right leg swung over the
saddle's back, onto the saddle, the feet to the pedals), **R** takes it by the bars and walks it along on your right (R
again puts it on its stand, E gets on). On it: **W** pedals (Shift harder), **S** brakes, **A/D** steer, **E** gets off
(it brakes to a stop first, then down on the left, the stand kicked down). The camera stays at the hero's eyes; his own
hands are on the bar tops, looking down shows his knees going round with the cranks. Stopped, he puts the left foot
down and the bike leans onto it.

The ride is a kinematic bicycle: speed from pedalling against rolling and air drag (more on earth and grass) and the
slope; steering limited by speed (the tyres' grip); the turn rate from the wheelbase; the lean is the balance lean
(atan(v·ω/g)). The wheels turn with the distance, the cranks with the rear wheel through a 42x17 gear, and stand still
when coasting (the freewheel ticks) or level themselves after a moment. It rides over the ground's traces at both
wheels (pitch on slopes), stops at walls, steps higher than 13 cm and water deeper than 30 cm.

Sounds: the tyres (asphalt, or loose ground on the landscape), the freewheel ticking with the wheel's speed when the
cranks stand, the chain when pedalling, a rim-brake rub, all close-range (gone by ~35 m).

The bike saves itself where it was left (`ALyudBike` is `ILyudSaveable`; actors named `LyudBike_<id>`).

## The body on a bike

`LyudBikeRider` bends any of our bodies (the hero's SK_Hero and the people on SK_Human_Skeleton: mannequin bone names, the
mesh facing +Y) onto targets the bike gives every frame (`ALyudBike::RiderPose`): the pelvis onto the saddle, the trunk
leaned forward just enough for the hands to reach (searched), the neck and head back up to look ahead, each leg by two-bone
IK so the ball of the foot is on its pedal and the foot turned flat on it (the toes dip at the bottom of the stroke, and
point down further if a shorter leg needs it), each arm by IK to the bar with the palm on it and the fingers closed round
it until their skin meets it (the thumb the other way). The same choreography (keys in `RiderPose`) gives getting on and
off and leading it by the bars, for the hero and the residents alike.

## The residents

`PeopleSim` keeps the bikes (`FWorld::Bikes`): each entrance's bike has a home (a point of its stair's way in) and is
given to a household up that stair with a grown-up under 77 (`AssignBikes`; one bike a household). When someone of that
household sets off on a way that is all on the valley's ground (no cable car, no train) with 110 m or more outdoors, and
their bike stands at home (their way down passes it) or within 25 m of where they set off, `PlanBike` rewrites the way:
down to the bike, **take** it, lead it out of the door, **get on**, ride (4.4 m/s; 3.6 over sixty), **get off** a few metres
short, **park** it on its stand beside the way by the door they go in at, or lead it back into its place in the entrance
when they come home. Each stop takes its seconds (`BikeActSeconds`), and the game plays it. Far from the camera this is all
the simulation does; near it `ULyudBikeSubsystem::PresentPerson` places the bike under or beside the body and poses the
body on it. The player taking a resident's bike leaves its owner on foot until it stands near where they set off again
(or back in its entrance).

## Tests

- `tools/peoplesim/run.sh test`: the "bicycles" part (owners, takes, parks, nobody away with a bike in hand, no stop
  over 10 s, no stale reservation).
- `unreal/scripts/test_bikes.sh`: Lvl_Scene01 offscreen (copy `Content/Scene01`, `Characters/Hero`, `Characters/Humans`
  from main): shots of the club's bike, the hero getting on, riding (his view ahead and down), turning, getting off,
  leading it; a resident taking theirs out of the entrance and riding; the entrances; a save and a load.
  `BIKETEST_ONLY=hero|npc|lobby`.
- Console: `Bike.Hero mount|off|lead|letgo|ride <throttle> <steer> <s>`, `Bike.Goto <id>`, `Bike.Look <id|person> [dist yaw height fov follow]`,
  `Bike.ViewFrom x y z pitch yaw [fov] | off`, `Bike.NpcRide <entrance id> [activity]`, `Bike.Report`, `lyud.Bike.Log 1`,
  `lyud.Bikes.Log 1`, `lyud.Bikes 0` (none at the next level start), `lyud.Bikes.AnyMap 1`.

## Cost

Nothing ticks while a bike stands. A ridden or led bike moves ~10 components a frame (no instanced meshes); every part is
out of Lumen's scene and the distance fields (small, moving), drawn within 60-90 m, LODs generated at import.
