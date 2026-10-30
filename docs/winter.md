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
4. **Field** (1 m): level depth x factor, none under roofs; paths cut out (below). Capped by place (Dima,
   2026-10-29): the town (within ~50 m of a roof, fading out by ~110 m) keeps at most ~15 cm so people walk freely;
   the wild up to 1.5 m, the forest keeping 2.2x what falls (no wind takes it, the sun reaches it less) with the
   deepest drifts in its lee.

The field goes to `/Game/Winter/RT_SnowDepth`; `M_Ground` lifts the landscape by it (world position offset; the
landscape's collision stays at the ground, so the player wades: deep snow off the paths slows them to 42%).
One deferred decal over the whole map (`M_SnowDecal`) whitens the ground by the field (normals from the field's slope,
thin snow in patches) and every upward face under the open sky by the level snow: roofs, benches, rails, kiosk tops.
Under the ground (the club, the tunnel) and under a roof it does nothing; people, animals, birds, plants, the sea
and the ice don't take it.

## Footprints (deformable snow)

`ALyudSnowNear`: a 30 m square of 10 cm cells round the camera (`M_SnowNear`), at what the snow lies on (the
landscape or a low surface over it, traced exactly at 1 m into `RT_NearGround`) plus the field's depth pressed by the
trail (`RT_SnowTrail`, 5 cm, a world-locked toroidal window): footprints of the player, the people's bodies and the
cats and dogs walking there (stamped by stride, left and right), and the trench the player ploughs through snow deeper
than ~22 cm. Pressed snow keeps 15% of its depth, greyer and glossier. The ground's own snow sinks under the square
(`M_Ground` reads `NearRect`) and comes back up toward its edge while the square sinks: no seam. Fresh snow fills the
prints (~5 cm hides one). The map's snow decal shades it too (receives decals), so it looks exactly like the ground's
snow round it, and it dissolves at its edge by a per-pixel dither: no ring moving with the player. No shadow of its own (a moving WPO mesh would redraw the virtual shadow map); measured
34.0 ms with winter vs 34.4 without in one process.

## Snowfall

`ALyudSnowFx`: 40k flakes in a 22 x 22 x 12 m box round the camera (`M_SnowFlakes`: a quad per flake, the fall
~1 m/s, the wind's drift, a sway, wrapped round the camera in the world position offset; ~2 cm, far ones kept a pixel
and fainter; none under a roof or below the snow's surface), as many as the weather's snowfall asks, lit as
translucency by the scene (warm under the lamps). Hidden indoors and underground. The rain's splashes, rings and
puddles are off on snow-covered ground (`ULyudRainFogSubsystem` asks `SnowOnGround`).

## Winter low water

The sea goes down smoothly by `lyud.Winter.LowWater` (70 cm) from 20 November to 5 January and back by mid April
(`ALyudSea::SetLevelOffset`: the surface, the water volume and the shader's level follow, the bed's depth map is read
again): the island railway's rails (30 cm under high water) stand dry, the ice comes right up to the bank's slopes
(the bank is no shore for the ice), open water stays only along the coast and round the island.
`lyud.Winter.LowWaterNow 0..1` forces it.

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
(ragged), pancakes for 4 m, then solid ice. Floes and leads only at the freeze's start and its break-up: from ~60%
of the weather's cover the open sea is shut. Snow lies on it in streaks. The waves die under it (`ALyudSea::WindScale`). Where it is
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
