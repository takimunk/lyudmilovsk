# Bike kit (blender/bikes)

A classic 1980s steel road racer (lugged diamond frame, chrome lugs/crown/tips, drop bars, 6-speed freewheel, double 52/42,
700c x 23 alu rims on 36 spokes, gumwall tyres, leather saddle, kickstand) plus a Soviet-style front-wheel rack, all made
procedurally from primitives (bmesh sweeps and lathes). No generated meshes, no textures, no text or logos.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/bikes/build_bike.py [-- --preview <dir>] [--strict]

Writes `design/bikes/game/bike_kit.glb` and `unreal/Lyudmilovsk/Content/Bikes/bike.json` (cm, Unreal frame, sockets and
pivots). `--preview <dir>` also renders the PNG set (side views, 3/4, top, drivetrain / cockpit / hub / saddle / pedal /
kickstand close-ups, crank 60 deg + bars 20 deg pose, bike rolled 7 deg onto its stand, stand folded, rack with a wheel).
`--strict` exits non-zero on any glitch finding.

## Frame

Blender metres: X forward, Y to the bike's left, Z up, origin on the ground under the bottom bracket. UE = (x, -y, z) * 100.
Size 56: wheelbase 1.00 m (rear axle x -0.405, front +0.595), tyre R 0.334 (both tyres touch z = 0), BB z 0.27, seat tube 73.5,
head tube 73, rake 45 mm, horizontal top tube 0.562 c-c, saddle top 0.74 m from the BB along the seat tube, crank 170 mm,
pedal centres at y = +-0.155. All derived numbers are named constants at the top of the script.

## Parts (object name = mesh name, origin = pivot, identity rotation, scale 1)

| part | pivot | tris |
|---|---|---|
| SM_Bike_Frame | ground under BB | 5252 |
| SM_Bike_Steer | steering axis at the head tube's lower end | 7724 |
| SM_Bike_WheelF | front axle | 4872 |
| SM_Bike_WheelR (incl. 6 cogs) | rear axle | 7288 |
| SM_Bike_Crank (arms, spider, 2 rings) | BB centre | 2584 |
| SM_Bike_PedalR / PedalL | spindle centre | 476 / 476 |
| SM_Bike_Chain (1.3716 m = 108 pitches) | origin | 2000 |
| SM_Bike_Stand (modelled down) | hinge on the left chain stay | 116 |
| **bike total** | | **30788** |
| SM_BikeRack (5 slots, 1.70 m) | ground, centre | 1728 |

Materials (flat Principled, no images): Bike_Paint (recolourable), Bike_Accent, Bike_Chrome, Bike_Alu, Bike_Steel,
Bike_Black, Bike_Tyre, Bike_Gumwall, Bike_Tape, Bike_Hood, Bike_Saddle, Bike_Strap, Bike_Chain (only on the chain),
Bike_Bottle, Rack_Steel. Every face has non-zero metre-scale UVs; normals are smooth under 40 deg, sharp above.

## Chain

A planar loop in the plane y = -0.051 (big ring, 18T cog = middle of the 6, two jockey wheels): two 7.5 mm bands 1 mm
thick, 5 mm apart. U = arc length / 12.7 mm (goes 0 .. 108 over the loop, one seam), V 0..1 across the band. The cage
reach was solved so the loop is exactly 108 pitches.

## Checks (run on every build)

`blender/railway/kit_lint.py` on all bike parts as assembled and on the rack: 0 findings (lugs, tape, bands sit >= 2.3 mm
proud or are turned half a facet; plates that touch have opposite normals). Own checks: no non-manifold edges, no
zero-area faces, no zero-area UVs, no inside-out pieces, identity transforms. Tyre lowest points z = 0.0000.

## Using the rig (see bike.json)

Crank: rotate about the Y axis through `bb`; pedals stay level at the crank ends. Steer: rotate SM_Bike_Steer about
`steer_axis`. Stand: fold up by `stand.up_angle_deg` about `stand.axis` (FQuat(axis, angle) on the UE leg directions
`leg_dir_down` -> `leg_dir_up`); rolling the bike 7 deg to the left about the X axis through the tyre contact points puts the
foot on the ground.
