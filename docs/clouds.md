# Clouds

The weather engine (`ULyudWeatherSubsystem`, docs/weather.md) gives three cloud layers, low, middle and high. Each has a
genus, cover, base, top and density, and the engine adds lens clouds, shower cells, the wind aloft and what falls.
`ULyudCloudSubsystem` (`Source/.../Sky/LyudClouds.*`) draws them on `ALyudSky`'s materials. One shader handles every
genus: each genus becomes a handful of shape numbers, so a change of genus morphs over a few seconds instead of
popping.

## What is drawn where

- **Volumetric** (`M_LyudClouds`, ray-marched by the engine's volumetric cloud; `tools/sky/cloud_hlsl.py`
  `VOLUME_HLSL`): the low and the middle layer.
  - Cumulus: heaps with flat bases and rounded tops, in loose streets along the wind.
  - Cumulus congestus: towers.
  - Cumulonimbus: giant separate cells on a jittered grid, each a tower under an anvil that flares downwind at the
    top. Rain shafts hang under the cells.
  - Stratocumulus: a lumpy deck in rolls.
  - Stratus and nimbostratus: even decks; nimbostratus has a ragged base and rain shafts.
  - Altocumulus: small cloudlets in patches.
  - Altostratus: a thin sheet that shows a watery sun.
  - Lens clouds: smooth stacked saucers, fixed over the ground and long across the upper wind.
  - A lightning strike (`OnLightning`) lights the cloud round it from inside.
- **High clouds on the dome** (`M_LyudSky`, `HIGH_HLSL`): cirrus (fibres combed along the upper wind), cirrocumulus
  (grain in ripples) and cirrostratus (a veil with the 22-degree halo round the sun and Europa). They are lit through
  the atmosphere (`SkyAtmosphereLightIlluminance` at the layer) and veil the disks. They sit behind the volumetric
  clouds.

## The shape numbers (`ULyudCloudSubsystem::ShapeOf`)

| Number | What it does |
|---|---|
| cell km | size of one cloud |
| sheet | smooth deck (1) or heaps (0) |
| top rounding | how much the cover shrinks upward |
| anvil | turns on the cell path with an anvil (cumulonimbus) |
| erosion | billows on the tops, wisps at the base |
| stretch | elongation along the wind: streets, rolls |
| ragged base km | how far the base wanders down |
| cluster | isolated clumps instead of an even field |
| rain shafts | curtains under the base, scaled by the precipitation rate |
| darkness | how much sky light the bases lose |
| boil | how fast the shapes evolve |

The weather's density is how much light a deck lets through. Decks (St, Ns, Sc, As) therefore get their optical
depth from it, whatever their thickness. Each layer drifts with its own wind:

- the low layer with the wind a kilometre up (faster than at the ground, and veered);
- the middle layer between that and the upper wind;
- the high layer with the upper wind.

The volumetric slab is fitted round what is drawn, so the ray march spends its samples where the clouds are. With
nothing to draw, the volumetric clouds are off.

Under an overcast the direct sun goes as well. The cloud shadow map dims it pixel by pixel, but under a closed deck it
must be gone everywhere, so there are no crisp shadows in a snowfall or a steady rain. Each layer lets through
1 - cover x (1 - e^-tau along the sun), and that counts in proportion to how closed the decks are (sheet or anvil
clouds at 75-97 percent cover). `ALyudSky::SetOvercastSunScale` scales the sunlight, and `lyud.Clouds.Debug` prints it
as "sun". Broken heaps keep their own moving shadows and sunny gaps.

A closed deck also:

- glows from its underside with the diffuse light that gets through it (`DeckGlow`, ~1/(1 + 0.11 tau) per layer).
  `ALyudRainFx` lights the fog under it with the same light (`ULyudCloudSubsystem::GetSkyLightThrough`), so an
  overcast day is grey rather than black, and the deck and the haze over the sea horizon match;
- is traced out to where its base sinks under the earth's curve (sqrt(2 R h) + 45 km, up to 230 km), so no strip of
  open sky is left over the sea.

An altostratus is thin (vertical optical depth 0.3-1.5), so the sun shows through it as a dim washed-out disk.

## Console

- `lyud.Clouds.Preset <name|off>` forces a sky: clear, cu, cucon, cb, sc, st, ns, ac, as, lens, ci, cc, cs,
  warmfront, coldfront, showers.
- `lyud.Clouds.Cover` multiplies every layer's cover.
- `lyud.Clouds.Drift` scales the drift.
- `lyud.Clouds.Debug 1` prints the layers.
- `lyud.Weather.Preset` drives the clouds through the weather.

## Gotchas

- The engine's cloud noise volumes (`T_VolumeNoiseShape128`, `T_VolumeNoiseErosion32`) hold one channel packed into
  a narrow band (0.80..0.98 and 0.55..0.9). The shader spreads it to 0..1 (`S`, `Er`); without that, any cover turns
  into an overcast.
- A volume-domain material needs `used_with_volumetric_cloud`. Without it the cloud shaders are never compiled, and
  the renderer asserts on the fallback surface material.
- `ALyudSky` gives the clouds their material instance only once its shader map is complete. A still-compiling
  material falls back to the same assert.
- Features much thinner than the slab get skipped by the ray march, so lens clouds are kept about half a kilometre
  thick or more.
- The ray march stops `TracingMaxDistance` (60 km) from the eye. The shader fades the clouds out before that, so
  there is no hard arc in the sky.

## Checks

- `unreal/scripts/shoot_clouds.py` (offscreen) shoots every preset from the cliff's brow, toward the sun, at sunset,
  on a moonlit night and from above.
- `CLOUD_PERF=1` measures frame time with the clouds on and off under a few skies, in one process. 2026-10-29,
  offscreen 1600x900, two rounds:
  - cumulus, stratocumulus, nimbostratus and lens clouds cost about 0.2-1 ms;
  - cumulonimbus costs 2.5-4 ms;
  - a clear sky costs nothing (the clouds are off).
