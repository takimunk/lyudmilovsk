# Graphics settings

The player picks a preset (Auto, Low, Medium, High, Epic, Cinematic) or a level per group on the menu's **Graphics**
page (F10 → Settings → Graphics). A level means the same thing everywhere: the menu, `scalability N` and
`sg.ShadowQuality N` in the console, and the benchmark's presets (`tools/bench.sh 0,1,2,3`).

| what | where |
|---|---|
| the levels: the engine's `BaseScalability.ini` with the game's changes and its own `lyud.*` knobs | `Config/DefaultScalability.ini`; phones also `Config/IOS`, `Config/Android` |
| Auto detection, render resolution, TSR history, frame cap, power saving, the saved settings | `Source/.../UI/LyudGraphics.{h,cpp}` (`LyudGraphics::`, `ULyudGraphicsSubsystem`) |
| the Graphics page | `Source/.../GameMenu/SLyudGameMenu.cpp` (`MakeGraphics`), strings `ui.gfx.*` |
| checks and measurements | `tools/check_scalability.py`, `tools/bench_gfx.sh`, `tools/bench.sh` |

The settings are stored in `GameUserSettings.ini`, section `[Lyudmilovsk.Graphics]`: `Levels` (ten groups in the
page's order), `Auto`, `Resolution`, `FrameLimit`, `VSync`, `PowerSaving`, `DetectedGpu`. The game's own window (not
the editor's Play) also keeps the engine's `UGameUserSettings` in step: levels, VSync, frame limit and window mode.

## The page

- **Preset**: Auto (this machine's level, shown as "Auto (High)"), Low .. Cinematic, or Custom once a group differs.
  From Custom, ‹ › take the lowest or the highest of its levels for all groups.
- **Resolution**: Auto (by the level and the window, shown with the percentage it comes to now) or a fixed 50, 58,
  67, 75, 85 or 100% of the window.
- **Frame limit**: 30, 60 (the default), 90, 120 or none. **VSync**: off by default.
- **Window**: fullscreen (borderless, the screen's own size) or a window of three quarters of the screen. Only in
  the game's own window on desktops; the editor's Play has the editor's.
- **Power saving** (on by default): 30 fps while the menu is open over the paused world, 10 fps while the game's
  window is in the background or minimized (`lyud.Gfx.MenuFps`, `lyud.Gfx.BackgroundFps`).
- **Details**: each of the ten groups on its own (View distance, Shadows, Global illumination, Reflections, Plants,
  Sky/sea/effects, Post process, Textures, Shading, Anti-aliasing). A press past either end changes nothing (Auto
  stays).
- Below: the pixels drawn now for the window ("1440×810 for a 1920×1080 window") and the GPU and memory.

## The levels

What the game changes against the engine's levels (everything else is the engine's `BaseScalability.ini`):

| group | Low | Medium | High | Epic | Cinematic |
|---|---|---|---|---|---|
| View distance: static mesh LODs (x `r.StaticMeshLODDistanceScale`) | 1.4 | 1.2 | 1 | 1 | 1 |
| birds moving as animated bodies, drawn to (bird lengths) | 4, 400 | 8, 550 | 12, 700 | 12, 700 | 12, 700 |
| people drawn to (m); cats and dogs as bodies, drawn to (m) | 80; 8, 60 | 110; 12, 85 | 150; 20, 110 | same | same |
| Shadows | the sun's VSM at half resolution, hard (engine: no shadows) | 70%, soft | engine | engine | engine |
| Global illumination | no Lumen, distance field AO (engine: no AO, sky dimmed) | engine (Lumen) | engine | Lumen at the engine's High | engine |
| Reflections | screen-space, quality 1 (engine: sky only) | engine | engine | Lumen at the engine's High | engine |
| Textures | one mip down (engine: MipBias 16, the smallest mips) | engine | engine | engine | engine |
| Effects: clouds' ray-march samples | 0.5 | 0.7 | 0.85 | 1 | 1 |
| sea (0 phone, 1 medium, 2 desktop), fish, motes in the water | 0, 0.5, off | 1, 0.75, on | the platform's (2), 1, on | same | same |
| Post process | engine | light shafts on | engine | engine | engine |
| Plants: grass tufts, forest floor, live plants' shaping ms a frame | 0.4, 0.5, 1.5 | 0.65, 0.75, 2 | 0.85, 1, 3 | 1, 1, 3 | 1, 1, 3 |
| forest: fruit trees made live near the camera; wind out to (m) | 3; 30 | 5; 42 | 8; 64 | 10; 120 | 10; everywhere |
| Auto resolution of a 1080p window | 50% | 58% | 67% | 75% | 100% |

- Material quality stays High and detail mode Epic at every level: the game's materials have no quality switches,
  and a change of either would cost seconds of a frozen menu (every material's shaders cached again, every
  component sent to the renderer again) for nothing visible.
- A level change reaches the game's plants without a stall: one forest and one grass patch a frame take it (one
  waiting for its frame does nothing else), a lighter level sends the farthest live plants back to their bakes at
  once, and a View distance change resends the world's instanced meshes 20000 instances a frame.
- `r.ViewDistanceScale` (the engine's 0.4 / 0.6 / 0.8 / 1 / 10) also scales every draw distance: the forests' 500 m
  becomes 200 m at Low, 300 at Medium, 400 at High, 5 km at Cinematic. The wind distances in the table already
  include it (`lyud.Forest.WindDistance` 75 / 70 / 80 / 120 / 0 x the scale). Trees past the wind distance stand
  still, so the virtual shadow map keeps their shadows cached.
- **Every key a level sets is set at all five levels** (here or in the engine's file). Scalability only sets the
  keys a level lists, so a key missing at one level keeps whatever the level before left there (Epic's shadows
  would stay on Low until a restart). `tools/check_scalability.py` checks it against the engine:

- **The game's own knobs carry `ECVF_Scalability`** where Source defines them. The engine drops a scalability line
  for any variable without the flag (an ensure, and the value is ignored: `UE::ConfigUtilities::OnSetCVarFromIniEntry`);
  until 2026-11-07 eleven of the sixteen `lyud.*` knobs lacked it, so the birds, people, animals, forest floor, sea,
  sea life, clouds and plant-shaping lines did nothing.

`tools/check_scalability.py` checks both against the engine and Source:

```sh
tools/check_scalability.py                                   # the engine at $UE_ROOT
tools/check_scalability.py --cvars <DumpCVars output>        # also: every key is a console variable of this engine
```

## Phones

Phones get their levels from the device profile and never Cinematic: in 5.8 iPhone 13 and 15 Pro .. 18 Pro are Epic,
iPhone 8 .. 11 High, the first iPhone SE Low; Android goes by its GPU (Low .. Epic). The engine's
`IOSScalability.ini` and `AndroidScalability.ini` tune shadows, reflections and post process for the mobile renderer.
`Config/IOS/IOSScalability.ini` and `Config/Android/AndroidScalability.ini` give back the engine's phone values for what
the project raised for desktops: no shadows, AO or screen-space reflections at Low, Low's small texture mips, the
engine's material quality and detail mode and light shafts (on iOS at Epic only), and the phone's own sea at every
level.
Auto on a phone takes the device profile's level for each group (its `sg.*` values), without the desktops' memory
cap, and Auto resolution there is the device profile's (`r.MobileContentScaleFactor`); only a fixed share on the page
changes it. `tools/check_scalability.py --platform IOS` (or `Android`) checks a phone's levels. Nothing of it has run
on a phone yet.

## Auto

A new install, or settings that came from another machine (`DetectedGpu` differs), picks the level for this one:

| machine | level |
|---|---|
| Apple M Max, Ultra | Epic (the M5 Max: ~20-25 ms on the benchmark route at 1080p) |
| Apple M Pro | High; Epic from the M4 Pro on |
| Apple M (plain) | Medium; High from the M5 on |
| other desktops | the engine's quick CPU and GPU test (`Scalability::BenchmarkQualityLevels`), the lowest of shadows, GI, post process |
| iOS, Android | what the device profile chose, group by group (no memory cap) |
| desktops under 24 GB of memory / under 16 GB | at most High / at most Medium (the world holds 13-20 GB) |

## Render resolution and TSR's history

Auto renders at the level's share of a 1080p window and, in taller windows, at that share x sqrt(1080 / height)
(`lyud.Gfx.AutoResolution.RefHeight`): the pixels drawn grow with the window's height, not with its area. At least
40% at Low, 50% above it; Cinematic always 100%. TSR upscales to the window.

| window | Low | Medium | High | Epic | Epic, TSR history |
|---|---|---|---|---|---|
| 1280x720, 1920x1080 | 50% | 58% | 67% | 75% (1440x810) | 200% |
| 2560x1440 | 43% | 50% | 58% | 65% (1664x936) | 173% |
| 3024x1964 (fullscreen Retina) | 40% | 50% | 50% | 56% (1693x1100) | 148% |
| 3840x2160 | 40% | 50% | 50% | 53% (2035x1145) | 141% |

TSR keeps its history at the window's size x `r.TSR.History.ScreenPercentage`: the engine's Epic and Cinematic
200% is four times the window's pixels, a 6048x3928 history on a fullscreen Retina Mac. Windows taller than 1080 get
the level's value x sqrt(1080 / height), at least 100% (`lyud.Gfx.AutoTSRHistory 0`: the level's own). The game sets
it only while it wants less than the level's and gives it back to the scalability after.

## What runs where

- **The game** (a window someone watches): all of the above.
- **The editor's Play**: the settings stored in the editor's `GameUserSettings.ini` apply (set them on the page in
  Play); without stored levels Play keeps the editor's levels (the page shows them, not Auto, and a frame limit or a
  resolution set there is stored without making them the player's), and still gets Auto resolution and the 60 fps
  limit. When Play
  stops the editor gets its levels and VSync back, and the resolution, TSR history and frame cap go back to the
  editor's own values with their own priorities (`IConsoleVariable::Unset`).
- **Offscreen and unattended runs** (`-RenderOffscreen`, `-unattended`, a fixed time step, commandlets): at the first
  world's start they go to the default levels (the device profile's, Epic where it sets none) and drop a frame limit
  the engine took from `GameUserSettings.ini`. The engine applies that file's `[ScalabilityGroups]` and frame limit
  before any game code (`FEngineLoop::PreInit`, `UGameEngine::Init`) and every `-game` run saves its levels back there
  when it exits (`UGameEngine::PreExit`), so without this an offscreen test would render at the level of the last run
  of the checkout that changed it, windowed or a test's own `scalability 1`. `-ExecCmds` run at the first tick, after
  this, so a test's own levels still apply. Not the page's stored levels either (they share the checkout's
  `GameUserSettings.ini` with Play);
  `r.ScreenPercentage=75` of `DefaultEngine.ini`, their own `t.MaxFPS` and vsync. `lyud.Gfx.*` commands still change
  the run's levels, and `GameUserSettings.ini` is not written.
- **The benchmark** (`-LyudBench`): its own presets, Auto resolution by them like a player whatever the player
  stored (offscreen draws 1280x720: 50 / 58 / 67 / 75%), no frame cap; a pass's `r.ScreenPercentage` or
  `r.TSR.History.ScreenPercentage` from the console wins, and after the pass `Unset` of the console's priority gives
  them back to the game's code (the engine keeps every priority's latest value, also one it refused).

## The engine's ways (UE 5.8, Mac)

- **Priorities.** A console variable takes a value only from a source at least as strong as the one that set it:
  constructor < scalability < game setting < project settings < `[SystemSettings]` of an ini < device profile < ... <
  code < console. `r.ScreenPercentage=75` in `DefaultEngine.ini` beats every scalability level, so **the benchmarks from
  cdeeade0 (2026-10-31) until this page ran every preset at 75%**. The game sets the resolution, the history and the
  frame cap by code, so `scalability N` from the console logs one "ignored as it is lower priority" warning for the
  resolution per change (the page gives its values back while it applies the levels: no warning).
- **View distance and instanced meshes.** The engine multiplies the instances' cull and wind distances by
  `r.ViewDistanceScale` only when a component is sent to the renderer; a change of the scale alone resends nothing.
  Two frames after the scale changes the game resends every instanced mesh of the world once, a budget a frame
  (`lyud.Gfx.RefreshInstances`, `.PerFrame`), else a View distance change would only show after a restart.
- **Instance LODs.** Plain instanced meshes pick their LOD per instance on the GPU, by `r.StaticMeshLODDistanceScale`;
  `foliage.LODDistanceScale` only reaches hierarchical ones, which this game does not use (they add a CPU walk per
  view). Shadows of non-Nanite meshes take the camera's LOD; `r.Shadow.LODDistanceFactor` is the only lever.
- **Read once.** `r.SkyLight.RealTimeReflectionCapture` takes effect at start only (SkyLightComponent.cpp), so no
  level touches it.
- **Play in the editor.** A positive `r.ScreenPercentage` wins over the editor viewports' own Play resolution
  (`r.Editor.Viewport.OverridePIEScreenPercentage`, LegacyScreenPercentageDriver.cpp).
- No MetalFX upscaler and no async compute on Metal in 5.8; hardware ray tracing is compiled in but off
  (`r.RayTracing=0`). TSR is the upscaler.

## Console

| command | what |
|---|---|
| `lyud.Gfx` | prints the settings and what they come to now (resolution, history, view distance, frame cap, detection) |
| `lyud.Gfx.Preset auto` / `0..4` | sets and stores the preset as the page does |
| `lyud.Gfx.Group <group> <0..4>` | one group: `view_distance shadow gi reflection foliage effects post_process texture shading anti_aliasing` |
| `lyud.Gfx.Resolution 0` / `30..100` | Auto or a fixed share |
| `lyud.Gfx.FrameLimit <fps>` | 0 is none |
| `lyud.Gfx.MenuFps`, `lyud.Gfx.BackgroundFps` | the power-saving caps (30, 10) |
| `lyud.Gfx.Governor` | the power-saving caps: 1 on, 0 off, -1 only in a window someone watches |
| `lyud.Gfx.AutoResolution.RefHeight`, `lyud.Gfx.AutoTSRHistory`, `lyud.Gfx.RefreshInstances`, `.PerFrame` | see above |

`scalability N` and `sg.<Group>Quality N` work as in any Unreal game; the page shows its stored settings, which
the console does not change. The game's knobs in the levels: `lyud.Birds.Bodies`, `lyud.Birds.DrawDistance`,
`lyud.People.DrawDistance`, `lyud.Animals.Bodies`, `lyud.Animals.DrawDistance`, `lyud.Sky.CloudSamples`,
`lyud.Sea.Quality`, `lyud.SeaLife.Density`, `lyud.SeaLife.Motes`, `lyud.GrassDensity`, `lyud.ForestFloor.Density`,
`lyud.Plants.ShapeBudgetMs`, `lyud.Forest.MaxLive`, `lyud.Forest.WindDistance`, `lyud.Forest.DistanceScale`,
`lyud.Forest.Shadows`.

## Measuring

- `tools/bench.sh 0,1,2,3`: the route at each level as the page sets it. The JSON's `screen_percentage` is what was
  drawn and `tsr_history_percentage` is TSR's history.
- `tools/bench_gfx.sh [variant ...]`: one level (`PRESET=`, Epic by default) with each setting changed in turn, in one
  process, twice, against `base`: `tsr100` `tsr150` `sp67` `sp100` `wind0` `wind60` `wind250` `fdist70`
  `noforestshadow` `nolive` `vsmbias10` `vsmbias05` `smrt4` `shadowlod2` `lodscale125` `vdist08` `clouds075`
  `clouds05` `grass085` `floor075` `sea1` `birds8` `shafts0`. See [benchmark.md](benchmark.md).
- `-LyudBenchShots` in `EXTRA=` takes a screenshot per segment: compare the levels' looks.

Still to do, once the Mac can run Unreal again (the Metal Toolchain is missing):

- the four levels on the route at 1080p and at the Retina size, against the 2026-10-31 numbers (those ran at 75% at
  every level);
- `bench_gfx.sh` at Epic: what the TSR history, the wind distance, the VSM bias, the clouds and the grass cost, then
  tune Epic and High by it;
- screenshots of each level, Low's 200 m forests above all;
- the page and the `lyud.Gfx.*` commands in an offscreen game.
