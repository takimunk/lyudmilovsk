# Topi and the river Ural

**Topi** is the dune land east of the town: Baltic sand dunes with marram grass that go on for ever, a place the mind
cannot bear. Warning signs stand where it begins. By day the hero cannot get far: the place presses on him and he
turns back by himself. At night one path leads through the dunes to the **Ural**, a river of light that is only there
in the dark. He can walk into it; he dissolves in it and wakes at the computer in the club.

Topi lies along the whole east (inland) edge of the map: walk east out of the town along its main axis (Y 905),
~850 m from the centre. The sand starts at about map X -1175 (it shows over the ground from ~X -1240), has climbed
the inland ridge from behind and stands ~10 m over its crest. The map's landscape ends at X -1300; everything east
of it is Topi's own ground. From the crest the dunes fall to a plain ~30 m under the club's floor and go on east.

The model works in its own frame ("local": LX along the sand's edge toward the south, LY across it, the edge at
LY -775, Topi toward -LY): `FModel::ToLocal` / `FromLocal`, map X = LY - 400, map Y = 1188 - LX.

## Where

| | map (X, Y) m |
|---|---|
| The sand's edge, the signs | (-1178, 904), (-1200, 888) by the path; five more along the edge (Y 288..1638) |
| Map marker `topi` (known from the start) | (-1225, 905) |
| The path (`FModel::Path`, 1.1 km) | from (-1170, 905) east over the crest and through the dunes |
| The river Ural | along X ≈ -2240 (`FModel::RiverPoint`), runs south, endless both ways (14 km of it are drawn) |
| The path's end: the sign «Урал», the towels | (-2162, 1038) |

## The parts

- `Private/Topi/TopiModel.{h,cpp}`: the land in plain C++ (no Unreal types). `Height` / `Sample`: the great dune over
  the map's edge, dunes of five sizes under one wind (240 m, 78 m and 19 m near; 1.1 km and 5 km "mega" dunes deep
  inside, which is what the horizon is made of; a 165 m family askew to the big one; 9 m knee-high sand waves),
  marram hummocks (teardrop mounds of caught sand, up to 1.35 m, where the grass is thick, with the grass on them and bare
  lanes between: never on slip faces or the path), the river's plain and channel, marram density. `Path`,
  `RiverPoint`, `RiverDirection`, `RiverFlow`, `RiverDepth`, `Near`. `Pressure(x, y, night)` and `WayOut`: what the place does to
  the hero. Thread-safe, any x and y.
- `tools/topi.py` writes `Content/Topi/topi_edge.tbl`: the map's own ground (`world_map.height`) in the strip where
  the sand lies over the landscape. **Rerun it when `tools/world_map.py` changes the land east of X -1100 or the map's Y extent**
  (`python3 tools/topi.py --check` fails when the committed table is stale).
- `tools/topi/run.sh test | map | profile | section`: checks (the sand covers the map's edge, no steps, slopes, the
  path is walkable and ends in water deep enough, the pressure rules) and pictures (`Saved/Topi/topi_near.png`, 1 m per
  pixel with the path in red; `topi_far.png`). `run.sh view out.ppm x y tx ty [sun azimuth] [sun elevation] [eye height]`
  draws the model from a standing eye looking at (tx, ty), ray-marched with a sun and cast shadows (~40 s): the way to
  judge silhouettes and near relief without the engine.
- `ALyudTopi` (`Public/Topi/LyudTopi.h`): spawned at play by `ULyudTopiSubsystem` in any world with `ALyudSky`
  (nothing is placed in the level). The ground is a quadtree of chunks round the camera: 128 m chunks of 2 m cells
  underfoot, each level twice as coarse, out to 40 km. Chunks are built on worker threads (`lyud.Topi.Workers`),
  turned into `UProceduralMeshComponent`s within a per-frame budget (`lyud.Topi.BudgetMs`), built once and kept;
  the least recently wanted are freed past `SpareChunks`. Only the finest chunks collide (async cooking); `HoldHero`
  keeps the player on the sand while the chunk under him cooks. Marram grass: `AProceduralGrass` patches on the finest
  chunks. It also builds the river's ribbon, the curtain of glow over it (520 m high: seen from far off), the motes
  of light over the water and along the path, and three lights by the path's end.
- `ULyudTopiMind` (component "Mind"): the pressure, the post process, turning back, the stream, the dive and the wake,
  the music. `AHeroCharacter::SetSpeedScale` and `ULyudSoundscapeSubsystem::SetMusicHold` were added for it.
- `ULyudTopiProps` (component "Props"): the signs, the stakes along the path, the Ural sign, the towels.
- Materials: `unreal/scripts/make_topi_materials.py` (HLSL in `tools/topi/topi_hlsl.py`) makes `/Game/Topi/M_TopiSand`
  (wind ripples, mineral streaks, sand streaming in the wind from `MPC_LyudWeather.Wind`, and the far dunes' slow
  heave: a world position offset past ~450 m that grows with distance and with the hero's unrest), `M_TopiRiver`
  (three sizes of sparks in cells moving with the stream, each with its own shifting colour), `M_TopiGlow`,
  `M_TopiMotes`.
- Props: `blender/topi/props.py`, `tools/make_topi_signs.py`, `unreal/scripts/import_topi.sh`.
- Sound: `audio/sounds.json` ids `topi_1`, `ural_1` (music), `topi_heart`, `ural_hum` (loops).

## The rules

- **Day**: the pressure grows with every step into Topi (0 at 70 m, full ~330 m in).
- **Night** (`ALyudTopi::Night()`, from the sun's altitude: the river comes up between -1.5 and -7 degrees): no
  pressure within ~26 m of the path, full 70 m off it; calm by the river within ~230 m of the path's end; no way past
  the river. Motes of light blown from the river mark the path, and stakes with rags stand along it.
- At full pressure the hero turns round and walks out for a few seconds (`line.hero.topi.turn.1..4`), event
  `topi_turned_back`.
- In the river (night only; the channel is dry sand by day) the stream pushes him south; in to the chest he dives:
  ~9 s of light, then he wakes at `PlayerStart_WakeUp` at 07:30 the next morning (`line.hero.topi.wake.1`), event
  `ural_dissolved`, `TimesDissolved` saved.

## Console and dev mode

`lyud.Topi 0` (hide, for A/B benchmarks), `lyud.Topi.Night 1` (the river now; -1 = by the sun), `lyud.Topi.Calm 1`
(no pressure), `lyud.Topi.TurnBack`, `lyud.Topi.Dissolve`, `lyud.Topi.Debug 1`, `lyud.Topi.Heave`, `lyud.Topi.Grass`,
`lyud.Topi.Props`. Teleports: `Content/Dev/teleports/topi.json` (`topi_edge`, `topi_dunes`, `topi_path`, `ural_bank`).

## Known gaps

- The forest of the inland ridge (X -1300..-880) stands in the first ~60 m of the sand: its trees rise out of the
  dune's face, half buried.
- Past the map's north and south ends the sand's edge goes on with the ground of the map's last row.
- The M map ends at X -1300: the river is not on it (it is a secret).
- Chunks of different sizes meet with skirts, not stitched edges.
