# Benchmark

One command flies a fixed camera route through the game at a fixed date and hour, once per scalability preset,
and files the frame times with the machine and the commit:

```sh
tools/bench.sh
```

It runs the editor build as a game offscreen (`-game -RenderOffscreen`: no window, running editors untouched),
takes about six minutes on a Mac, prints a table and writes
`bench/results/<machine>/<date>_<commit>[_<label>].json`. Commit the result files you want to keep; they are small.

```sh
tools/bench.sh 3                                   # Epic only
RES=2560x1440 tools/bench.sh 2,3                   # High and Epic at 1440p
LABEL=noshadow SETUP="lyud.Bench.ForestShadows 0" tools/bench.sh 3     # a variant
python3 tools/bench_report.py bench/results/<machine>/<file>.json      # a result again
python3 tools/bench_report.py --compare --segment forest bench/results/*/*.json
tools/bench_forest.sh                              # every forest option, compared (see below)
```

## What it does

`ULyudBenchmark` (`Source/Lyudmilovsk/{Public,Private}/Bench/LyudBenchmark.*`) is a world subsystem that starts
when the game is launched with `-LyudBench` (or `lyud.Bench` in the console). Plain C++, so a packaged game on any
platform runs it the same way.

1. Sets the game calendar to the benchmark time (default 2026-10-20 16:00, the scene's start; `-LyudBenchTime=`).
2. Waits until every `AProceduralForest` has baked its trees (it logs the bake time), hides the hero and takes the
   view with its own camera (90° FOV), moving the pawn along so the sky still knows when the player is outdoors.
3. Runs the knobs and `-LyudBenchSetup` commands (a variant to compare).
4. Flies the route once at triple speed, untimed, so shaders compile and textures stream in.
5. For each preset (`scalability 0..3`: Low, Medium, High, Epic): flies the route at walking pace and records every
   frame. Each segment starts with a second standing still at its first key (not recorded), so a cut between
   segments does not count as a hitch.
6. Writes the JSON, restores the scalability it started with and quits.

The route (`ULyudBenchmark::BuildRoute`), about 66 s per preset:

| segment | what | seconds |
|---|---|---|
| hall | the entrance hall from the corridor door to the exit door | 10 |
| corridor | down the club's corridor at eye height | 15 |
| door | out of the exit door and down the path | 7 |
| terrace | standing on the terrace, looking back up at the upland forest, panning | 8 |
| forest | a walk inside the old forest on the upland (eye height over the landscape) | 16 |
| overlook | 25 m over the upland's edge, panning over the valley, the town and the far forests | 10 |
| flight | a bird's line 25 m over the ground: over the old forest on the upland, down the slope and across the town | 30 |

Outdoor keys are given in outdoor-map metres (`tools/world_map.py`) and follow the landscape. If the map moves,
fix the keys there and check them with `-LyudBenchShots` (a screenshot per segment in `Saved/Benchmark/`).

## What is recorded

Per segment and for the whole route, per preset: frame time median, p95, p99, mean, max (ms) and fps; game thread,
render thread, RHI thread and GPU time (`stat unit`'s sources: `GGameThreadTime`, `GRenderThreadTime`,
`GRHIThreadTime`, `RHIGetGPUFrameCycles`); draw calls and triangles (`GNumDrawCallsRHI`,
`GNumPrimitivesDrawnRHI`); process memory. Plus the machine (device, CPU, GPU, RHI, RAM, OS), resolution, build,
game time, number of trees and bake time.

- **On a Mac (Metal) draw calls and triangles read 0** in this engine build (the CSV profiler shows the same);
  the report prints `n/a`. They work on D3D12 and Vulkan.
- Results from the editor's `-game` include some editor overhead and use the editor's shader compile; a packaged
  Development or Test build is the honest number for a target machine (`editor_game` in the JSON says which).
- `bench.sh` warns and adds `busy=N` to the label when other Unreal processes or compilers are running: those
  share the GPU and CPU, and the numbers are noisy. For numbers you compare, run on a quiet machine.
- **Offscreen runs render at 1280x720** whatever `-ResX/-ResY` say (the engine's `r.SetRes` default wins; the
  JSON's `resolution` shows what was really drawn). `bench.sh` also passes `r.SetRes` through `-dpcvars`; check
  the JSON before trusting a `RES=`. Dima's real window is bigger, so his frames cost more than these.
- Memory: the game with Lvl_Scene01 holds about 13 GB (mostly the forests' baked trees). Never run the benchmark
  while someone plays in an editor on the same Mac: together they swap and both numbers are wrong.
- The editor's `-game` keeps resolution and scalability in `Saved/Config/MacEditor/GameUserSettings.ini`;
  `bench.sh` moves it away for the run and puts it back.

## Deeper profiles

- `-LyudBenchCsv` (in `EXTRA=`) records a `csvprofile` per preset in `Saved/Profiling/CSV` (the segments are CSV
  events); add `-csvGpuStats` for per-pass GPU timings. Open with the engine's CSVTools (`PerfReportTool`).
- Unreal Insights: `EXTRA="-trace=default,gpu -tracefile=$PWD/bench.utrace"`, open in UnrealInsights.

## Other platforms

The command-line switches are the game's, so any build can run it:

```
<game> /Game/Scene01/Lvl_Scene01 -LyudBench -LyudBenchPresets=0,1,2,3 -LyudBenchOut=<path>.json -LyudBenchLabel=<commit>
```

- Windows / Linux: a packaged Development build, the same switches; results to copy back into `bench/results/`.
- iOS / Android: put the switches in `UECommandLine.txt` (staged next to the game) and fetch the JSON from the
  app's `Saved/Benchmark` folder. Mobile renders with the mobile renderer and its own presets; a separate
  mobile route length may be needed.
- `tools/bench.sh` itself is Mac-only (editor path); a Windows `.bat` twin is a few lines when needed.

## Forest variants

`tools/bench_forest.sh [variant ...]` flies the outdoor segments at Epic with every variant in turn **in one
process**, the whole list twice (`-LyudBenchPasses`, `-LyudBenchRounds`), and prints each variant against `base`.
One process per variant does not work on a shared Mac: other editors and compiles come and go and the drift
between runs is bigger than most gains. The `lyud.Bench.*` knobs change the forests' instanced components after the bake,
without touching `AProceduralForest`, so an option can be measured before anyone builds it:

| variant | setup |
|---|---|
| base | nothing |
| noforest | `lyud.Bench.ForestHide 1`: no forests (the cost of the rest) |
| noshadow | `lyud.Bench.ForestShadows 0`: forests cast no shadow |
| skyfreeze | `lyud.Bench.SkyFreeze 1`: the sky stops turning the sun every frame |
| cull300 / cull600 | `lyud.Bench.ForestCullM 300` / `600`: no forest trees past 300 / 600 m |
| nowind / wind40 | `lyud.Bench.ForestWPODisableM 0` / `40`: no wind, or no wind past 40 m |
| minlod1 / minlod2 | `lyud.Bench.ForestMinLod 1` / `2`: never finer than LOD1 / LOD2 (a stand-in for lighter trees) |
| channel1 | `lyud.Bench.ForestChannel0 0`: forests on the outdoor lighting channel only (out of the club lights' shadows) |
| nolive | `lyud.Bench.LiveHide 1`: hide the live procedural plants (the full trees swapped in near the camera) |
| notrees | forests and live plants hidden |
| csm | `r.Shadow.Virtual.Enable 0`: classic cascaded shadow maps instead of virtual shadow maps |
| nolumen | `r.DynamicGlobalIlluminationMethod 0; r.ReflectionMethod 2`: no Lumen |
| sp67 | `r.ScreenPercentage 67`: render at 67% and upscale with TSR |

## Forest findings (2026-10-21, M5 Max, editor -game, 1280x720, Epic)

From `bench/results/Mac17-7-AppleM5Max/20261021-112444_a7c276c-dirty_forest.json` (A/B in one process, two
rounds, frame median; the machine was shared, so +-10% is noise):

| segment | base ms | no trees | forests cast no shadow | trees past 300 m culled | no Lumen | 67% screen | cascaded shadows |
|---|---|---|---|---|---|---|---|
| hall | 13.0 | -9% | ~0 | ~0 | -23% | -19% | +133% |
| corridor | 21.9 | -26% | -21% | ~0 | -14% | -16% | +159% |
| terrace | 40.2 | -74% | -5% | -15% | ~0 | -11% | +59% |
| forest | 60.7 | -86% | -24% | -9% | ~0 | -10% | +55% |
| overlook | 20.3 | -55% | -5% | -22% | -10% | -5% | +53% |

- The forests are almost the whole outdoor cost (8 ms without them inside the forest, 61 ms with).
- Hiding the live procedural trees near the camera gains nothing on the ground (-21% only from the overlook):
  the cost is the baked instances themselves, mostly the near ones drawn at LOD0 (the full tree) out to 30 m.
- The forests cost in the corridor underground too (-26% without them; -21% without their shadows): they sit on
  lighting channels 0 and 1 and their components' bounds span 280x215 m tiles over the club, so every club light
  with shadows culls their instances. `channel1` measures the fix.
- Freezing the sky's sun gained nothing (VSM cache is fine). Cascaded shadow maps are far worse than VSM here.

## Live systems and Lumen (2026-10-31)

`tools/bench_systems.sh [variant ...]` flies the whole route (or `SEGMENTS=`) at Epic with each live system switched
off in turn, in one process, like `bench_forest.sh`: `nobirds`, `nopeople`, `noanimals`, `notransport`, `norailway`,
`nosea`, `noacadem`, `nofloor`, `nosound`, `novines`, `noforest`, `nolumen`, plus Lumen and quality options
(`cardfreeze`, `captures64`, `lumenlow`, `shadowhigh`, `sp75`).

- **Run it alone.** A -game of Lvl_Scene01 holds ~22 GB; two at once on the 36 GB Mac swap (18 GB of swap seen) and
  frames read 300+ ms instead of ~40. `tools/wait_ue_queue.sh` before every run, and check `sysctl vm.swapusage`.
- Each live system alone (birds, people, animals, transport, sea, forest floor) moves the frame under 5%.
- Lumen was ~250 surface-cache card captures a frame, 95% of them the cableway's rope cylinders: anything that updates
  instances every frame with `bMarkRenderStateDirty` (or calls `MarkRenderStateDirty()` after the loop) rebuilds its
  proxy, and Lumen recaptures all its instances. Fixed in 68e1e243 (15 captures a frame; route 39.6 -> 32.3 ms at
  1080p, hall 34 -> 26, flight 41 -> 33). See it with `r.LumenScene.SurfaceCache.LogUpdates 2` in `SETUP`.
- Since cdeeade0 Epic runs Lumen GI and reflections at the engine's High settings (Config/DefaultScalability.ini) and
  renders at 75% with TSR (`r.ScreenPercentage=75` in DefaultEngine.ini): route 32.5 -> 23.7 ms, forest 79 -> 69, hall
  26 -> 18, ~0.5-1 GB less. Lumen off entirely would be another ~38%.
- Editor memory (unreal/scripts/probe_memory.sh MODE=pie): idle with Lvl_Scene01 14.0 GB (14.9 before
  ULyudNavStripSubsystem freed the meshes' nav collision), Play ~26 GB, after Play stops ~23-24.6 GB and it stays: GC,
  `mi.MemoryResetDelay 0` and `r.RenderTargetPoolMin 0` give back under 0.3 GB, the render-target pool and RHI
  resources are back to idle (+0.4 GB); what stays is Metal textures (LLM +2.5 GB) and untagged CPU memory (+1.5 GB),
  driver and engine caches. Restart the editor after long play sessions.
