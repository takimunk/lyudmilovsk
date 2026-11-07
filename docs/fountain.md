# The fountain

The white cast-stone two-tier fountain in the Academgorodok's square, before Novikova 1 (docs/academgorodok). Its stone
is part of the square's mesh (`blender/academgorodok/details.py` `fountain()`); the water, the splashes and the sound are
`ALyudFountain` (`Public/World/LyudFountain.h`), spawned by `ULyudFountainSubsystem` in Lvl_Scene01 and built in code. The town's Sun fountain on its square uses the same water
from its own levels (docs/city/public_spaces.md).

## The water

| Part | What it is | How it moves |
|---|---|---|
| Jet | a thin column rising ~34 cm from the fleur-de-lis | `M_FountainStream`: streaks ride with the water (the mesh carries each vertex's flight time, the pattern is sampled at flight time - now) |
| Bell | the jet opens at its top and falls round the fleur into the upper bowl, tearing into ~22 strands | the same |
| Veil | the brim-full upper bowl spills over its whole lip, a sheet breaking into ~56 strands, landing ~12 cm out in the lower basin | the same |
| Basins | single-layer water, a few cm over white stone, a touch of green | `M_FountainWater`: capillary ripples running both ways from where the water lands, a ring of foam and bubbles there, the churn, a breath of wind |
| Splashes | ~440 drops: a crown where the veil lands, drips off its strands, the bell's landing, spray off the jet's top | `M_FountainDrops`: each a quad moved along its parabola and turned to the camera in the vertex shader |
| Drained | a damp stain and drifts of fallen leaves on both floors (snow lies on them in winter: the map's snow decal) | `M_FountainLeaves` |

The shapes are ballistic: speeds and heights in `ALyudFountain::Build`, the stone's levels in
`Content/Academgorodok/fountain.json` (`python3 tools/academ_fountain.py`, from `details.fountain()`'s profiles: change
them together). Materials: `unreal/scripts/make_fountain_materials.py` (headless, `/Game/Academgorodok/Fountain`).

Cost: five small static meshes built once at the start (~12k triangles), nothing updated per frame; every moving part
is out of Lumen's GI and the distance fields and casts no shadow. The actor ticks four times a second (the sound's
distance) and checks the season every 2 s.

## The sound

`ULyudFountainSynth`: the procedural fountain (`Private/Audio/FountainDSP.*`, plain C++ like the outdoor bed): a wash of
merged drops breathing as the veil wavers, bubbles ringing at their Minnaert pitch (~550 Hz - 3.3 kHz), the slaps of
drops, a low burble of the jet on the upper bowl. Mono, spatialised from the lower basin: full within 1.5 m (all round
you at the rim), natural falloff to 50 m, the highs dulling with distance. It plays only while
the camera is within 50 m of a running fountain, at the settings' master x ambience volume, and swells in and out.
`tools/audiosim/run.sh fountain` renders and checks it outside the engine (`Saved/AudioSim/fountain.wav`).

## The season

The first-frost search (the weather engine's hours from 1 September) is shared by every fountain and goes a slice of 120
hours per tick, the fountain keeping its state meanwhile, so a jump of the date never costs one long frame. Drained or
hidden, the sound fades for 1.5 s and stops.


The town runs the fountain from **1 May** until the **first frost** of the autumn: the first hour since 1 September
when the weather engine's air at 2 m is under -0.5 C (`LyudWeather::Instant`, the same chain as the weather the player
sees, without forced values). Then it stands drained until May. Everything is a function of the game calendar, so a
save, a load or a jump of the date needs nothing of its own. In the game's first year the first frost is on
25 November 2026 (the game starts on 20 October with the fountain running).

## Console

`lyud.Fountain 0` no fountain at all (at the next start of the map), `lyud.Fountain.Force -1|0|1` by the season /
drained / running, `lyud.Fountain.Debug 1` logs the state, the first frost and the sound's level every 2 s;
`lyud.Academ 0` hides it with the ensemble.

## Test

`unreal/scripts/test_fountain.sh` (offscreen, silent speakers): shots round the fountain on 21 October (running), after
the first frost and in winter (drained), in April (drained) and May (running again), the sound near and far, and
frame times with and without it in one process (`FOUNTAIN_SHOTS=ab_on,ab_off,ab_on2,ab_off2`). Shots go to
`unreal/Lyudmilovsk/Saved/FountainShots/`.
