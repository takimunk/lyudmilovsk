# Rain and fog on screen

What the weather looks like around the player: drops, splashes, wet ground and puddles, rings on the sea, haze and
fog, and the rain's sound on water and thin roofs. The weather itself (when it rains, how hard, the mist) is the
weather engine's; this is only how it is drawn and heard. Snow on the ground is the winter effects'.

Code: `Source/Lyudmilovsk/{Public,Private}/Weather/LyudRainFog.*`. Materials: `unreal/scripts/make_rainfog_materials.py`
with the HLSL in `tools/rainfog/rain_hlsl.py`. Test: `unreal/scripts/test_rainfog.sh`.

## Pieces

| Piece | How | Cost |
|---|---|---|
| Drops | one mesh of 14,000 quads (built at BeginPlay); the vertex shader places each streak in a 30 x 30 x 20 m box around the camera, wrapped in world space so drops stay put as you walk; slanted by the wind, stretched by speed, a camera-facing ribbon at least a pixel wide (fainter when thinner). Light rain shows fewer streaks; drizzle shows many fine slow ones. Unlit translucent, drawn after motion blur so TSR does not smear it. | 1 draw, ~28k triangles, small overdraw |
| Splashes | 3,000 quads, each a ring born again every ~0.45 s at a new place within 12 m, laid on the roof map's height | 1 draw |
| Roof map | 64 x 64 cells of 1.5 m around the camera (96 m): a downward trace per cell gives the topmost surface the rain meets (pawns ignored) and whether it is the sea; 256 traces a frame, edges first as the camera moves | ~0.1 ms CPU |
| Wet ground | one deferred (DBuffer) decal over the roof map: darker and smoother where the sky rains on it (not under roofs, not in the club); puddles by noise on flat ground, with rain rings while it rains | one decal pass |
| Sea rings | `M_LyudSea` adds rain rings to the waves' normal (`MPC_LyudWeather.Rain`) | a few ALU in the water |
| Fog | the level's `ExponentialHeightFog`: haze from the visibility (Koschmieder, density = 39.12 / visibility m), a second layer lying on the sea level, as deep as the weather's fog top (sea fog fills the sea and the valley under the upland), coloured by the sky's light | free |
| Sound | the soundscape's rain bed: more bubble plips and a brighter hiss when the listener is at the waterline or in the sea; drumming on a cabin's or a canopy's roof with open sky around | in the DSP |

Indoors is decided by the roof map: the share of cells within 12 m whose top is below the eye ("open sky"). In the
club and the tunnels it is 0, so no rain is drawn and the level's own fog comes back; in a cable-car cabin or the
train under the sky, the cells around are open, so the fog stays and the rain falls outside the windows only.

## Weather input

`ULyudRainFogSubsystem::ReadWeather()` is the only place that reads the weather (`FLyudRainFogWeather`: rain mm/h
without the snow's share, drizzle 0..1, wet, cloud, visibility, fog layer's visibility, top and floor, wind).
Today it maps `ULyudWeatherSubsystem::GetSky()`; when the weather engine's state comes (precipitation type, fog type,
visibility, fog depth) only this function changes.

## Console

- `lyud.Rain.Fx 0|1`, `lyud.Fog.Fx 0|1`: the effects off/on (A/B in one process).
- `lyud.Rain.Debug 1`: the state on screen.
- Forcing: `lyud.Weather.Rain`, `lyud.Weather.Fog`, `lyud.Weather.Wet`, `lyud.Weather.FogTop`, `lyud.Fog.Scale`.

## Building

```
UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_rainfog_materials.py -unattended
UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/make_sea_materials.py -unattended
```

`/Game/Weather` must be cooked in a packaged build (materials are loaded by path at BeginPlay).
