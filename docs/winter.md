# Winter

The weather engine (docs/weather.md) says how much snow lies on open level ground, how fast it falls and how much of
the sea is frozen. Winter says **where**: drifts, cleared paths, the open strip along the shore, snow on roofs and
trees. Everything is driven by `ULyudWinterSubsystem` (`Public/Winter/LyudWinter.h`); the rules are plain C++ in
`Private/Winter/SnowModel.*` and run outside the engine with `tools/winter/run.sh test`.

## Snow on the ground

1. **Cover** (2 m). When winter first comes (any snow or ice), the subsystem traces the whole landscape top-down, a few
   hundred rows a frame (`lyud.Winter.TraceMs`, default 6 ms): the ground (the landscape, or a `LyudSeaBed`
   component) and the highest visible thing over it. Over 1.8 m is a roof (no snow under it); lone roofs (lamp posts,
   trunks) are dropped; roof edges are traced again at 1 m. Forest tiles (`AProceduralForest`) are marked.
2. **Shelter** (Winstral's Sx): for each of 8 winds, the steepest rise of the ground and what stands on it within
   100 m upwind. Positive = sheltered (lee of a house or a hill, a forest edge, the cliff's brow), negative = exposed.
   A tall wall 2-4 m downwind scoops the snow away at its windward foot.
3. **Drift factor**: the shelter weighed by this winter's drifting winds (their snow-carrying power, ~(u-5 m/s)^3,
   while the snow is loose: integrated from the weather's history since 1 September), blurred, normalised to a mean
   of 1 (snow moved, not made), blended in by how far the drifting has got (`Drift` 0..1). Steep slopes shed it,
   forest floors keep 72%.
4. **Field** (1 m): level depth x factor, softly capped at ~1.1 m, none under roofs; paths cut out (below).

The field goes to `/Game/Winter/RT_SnowDepth`; `M_Ground` lifts the landscape by it (world position offset; the
landscape's collision stays at the ground, so the player wades: deep snow off the paths slows them to 42%).
One deferred decal over the whole map (`M_SnowDecal`) whitens the ground by the field (normals from the field's slope,
thin snow in patches) and every upward face under the open sky by the level snow: roofs, benches, rails, kiosk tops.
Under the ground (the club, the tunnel) and under a roof it does nothing; people, animals, birds, plants, the sea
and the ice don't take it.

## Paths and shovelling

The walk grid's paved cells (`Content/AnimalsData/animal_world.bin`) plus a 1.4 m way from every doorstep are the
paths, cut into ~12 m patches. Fresh snow lies on them; residents clear them with a snow pusher (activity `shovel` in
`activities.json`: free time, daylight, ages 15-76, "active" temper): the people's life asks the winter for dirty
stretches near the camera (`SnowJobs`), takes one (`ClaimPatch`), pushes along it at 0.36 m/s
(`SetPatchProgress` sweeps it clear as they go), then the next nearby. The snow goes into berms beside the path.
Patches far from the camera are cleared by somebody unseen some hours after the snow began to lie
(`lyud.Winter.AutoClearHours`). The clip `A_Human_Push`: `blender/humans/push_clip.py` (UAL `Push_Loop`'s legs and
lean, the arms by IK onto the pusher's bar); the pusher is three basic shapes placed between the hands each frame.

## Trees and grass

`M_PlantLeaf` and `M_PlantNeedle` whiten their upper sides by `MPC_LyudWinter.SnowB.x` (snow on the branches:
fresh snow sticks, wind and thaw shake it off in a day or two); `M_PlantBark` takes the larger of its own `Snow` and
that; `AProceduralPlant`/`AProceduralVine`/`AProceduralGrass` ask `ULyudWinterSubsystem::SnowOnBranches` /
`SnowOnGround` instead of the calendar's guess where the winter runs.

## Sea ice

`ALyudSeaIce`: a flat sheet at the still water (`ALyudSea::SeaLevel()`, so it rides the tide and the surge) +
freeboard. `M_SeaIce` masks it by the distance from water shallower than 0.4 m (the shore at this tide, the railway's
bank, the island; a 3 m exact distance transform redone when the water moves 10 cm): open water for 10 m +- 3 m
(ragged), pancakes for 4 m, then solid ice; floes and leads when the cover is partial; along the railway line beyond
the grid a lead stays open. Snow lies on it in streaks. The waves die under it (`ALyudSea::WindScale`). Where it is
solid and >= 8 cm thick, box tiles round the player hold them.

## Console

`lyud.Winter 0` off. `lyud.Winter.Snow / Snowfall / Drift / DriftFrom / Age / Canopy / Ice / IceCm` force a value
(negative = the weather's). `lyud.Winter.Debug 1` on screen, `lyud.Winter.Report` to the log.

## Building and checking

- `tools/winter/run.sh test` the model's checks (lee drifts, scoured tops, mean = level, paths, berms, the strip
  following the tide, the lead along the bank); `perf` timings on a map-sized grid; `images` PGMs.
- `tools/peoplesim/run.sh test` includes the shovelling checks.
- `unreal/scripts/make_winter_materials.py` (headless) builds `/Game/Winter` and patches `M_Ground` and the plant
  materials. `import_terrain.py` adds the lift when it rebuilds the ground.
- `unreal/scripts/test_winter.sh` offscreen: 35 cm drifted snow and 25 cm sea ice forced, WINTERTEST lines and shots.
