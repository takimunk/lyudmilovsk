# Tides

The Baltic hardly has tides; Lyudmilovsk's sea does, and strong ones: at low water a wide sand flat lies bare
below the cliff, with sand bars, runnels, winding channels and pools left standing in the hollows.

## The model (`Sea/Tide.h`, `LyudTide`)

An equilibrium-like semidiurnal tide from the real sky (`Sky/Ephemeris`, `LyudEphemeris::HourAngles`): the moon's
pull plus 0.46 of the sun's, each `cos(2 x hour angle)`, felt one hour late (`LagHours`). As phasors the two add up
at new and full moon (springs) and partly cancel at the quarters (neaps); the sum's length says where between neaps
and springs the tide is, its real part where in the ~12 h 25 min cycle the water is. High water is always the
datum, the map's `SEA_Z` (-43 m, Unreal -4300 cm): the tide only goes down from it.

| | low water | range |
|---|---|---|
| neaps (quarter moons) | -2.0 m | 2.0 m |
| springs (new and full moon) | -4.5 m | 4.5 m |

`tools/world_map.py` mirrors the two lows (`TIDE_NEAP_LOW`, `TIDE_SPRING_LOW`).

The clock is the sky's (`ALyudSky::ShownTime`: the calendar plus `lyud.SkyHourOffset`), so a screenshot of a
night shows that night's tide.

- `lyud.TideOverride <m>` pins the level, metres over the datum (0 high water, -4.5 spring low); -999 or NaN = off.
- `lyud.TideSpeed <n>` runs the tide's clock n times faster than the sky's (3600 = an hour a second).
- `lyud.Sea.Debug 1` prints the tide with the sea state.

## The sea (`ALyudSea`)

The actor stays at the datum. `SeaLevel()` = datum + tide; `DatumZ()` / `HighWaterZ()` the datum, `LowWaterZ()`
the lowest spring water, `TideLowZ()` this tide's low, `WetLineZ()` the highest the water stood in the last hour,
`TideOffsetCm()`, `TideSpring()`, `IsTideRising()`.

The depth map still holds the depth under the datum; everything that uses depth adds the tide:
`SampleSurface`, `DepthAt` (now; `DepthAtLevel` for any level), `IsSwimmable`, the surface ring's Z, the water
physics volume (moved with the tide), the MPC `Sea` vector, and the sea material (`Tide` parameter, the
`Lyud.Water` lookup in `tools/sea/sea_hlsl.py`), so the waves shrink and break at the moving edge.

**Pools.** When the depth map is read, a priority flood from the water that never leaves (deeper than the lowest
springs) finds the hollows on the flat that cannot drain; the depth map's second channel holds how much water each
keeps. Once the ebb leaves a hollow, the ground (`M_Ground`, `WET_HLSL` in `unreal/scripts/import_terrain.py`) draws
the pool as water up to its rim: a sharp edge with a band of soaked sand round it, nearly black through its depth,
smooth (roughness 0.02) and ruffled by small drifting ripples, the sky mirrored in it (the sky light's cubemap along
the mirror direction, by water's Fresnel, as emissive: the landscape's own reflections stay dim on still water). The
soaked sand under the hour's wet line is smooth and glistens: grain-sized facets catch the sun and the sky near the
camera. The CPU side ignores pools (they are ankle deep). `TERRAIN_WET_ONLY=1` with `import_terrain.py` rebuilds just
this part of `M_Ground` in place.

**Seabed life** (`ULyudSeabedComponent`) is placed only where the lowest spring tide still covers it; the fish
and jellies swim in the water as it is now.

**Others that follow the tide:** wild birds (`BirdSim` `FWorld::SeaZ`, set by `UBirdLifeSubsystem` every frame),
the player bird's air and feet (`FBirdAir::WaterZ`, `ABirdPawn::WaterZ`), the surf sound (`ULyudSoundscapeSubsystem`
finds the nearest water now).

## The ground

`W.tidal_flat` (`tools/world_map.py`, the sea-bed branch of `W.height`): ~1:80 for the first ~350 m out from the
high-water line (about 340 m dry at spring low water, ~140 m at neaps), then the old 6% bed down to 9 m. On it:
bars ~70 m apart with runnels behind them, six channels winding out to sea (`TIDE_CHANNELS`), and shallow dishes
in some 110 m cells. TerrainGen's sand ripples under water (+-0.25 m) add small pools of their own.

The ground material (`M_Ground`, `unreal/scripts/import_terrain.py` `WET_HLSL`) reads MPC_LyudWeather's `Tide`
(water now, datum, wet line, springs): sand from the water's edge up to where the water stood in the last hour is
dark and glistening, damp a hand above that, and dries back as the wet line falls; over the high-water mark it stays
dry.

## Checks

`unreal/scripts/shoot_cosmic.py` (offscreen): the beach and the flat at high, mid and spring-low water from the
brow and from the beach, and the pools at low water.
