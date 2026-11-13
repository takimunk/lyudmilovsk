# Topi and the river Ural

**Topi** is the dune land south-west of the town: Baltic sand dunes with marram grass that go on for ever, a place
the mind cannot bear. Warning signs stand where it begins. By day the hero cannot get far: the place presses on him and
he turns back by himself. At night one path leads through the dunes to the **Ural**, a river of light that is only
there in the dark. He can walk into it; he dissolves in it and wakes at the computer in the club.

West of the town is the sea, so Topi lies along the coast to the south-west: walk south along the beach under the
club (~800 m). The sand starts at map Y -775, has buried the end of the upland and the cliff, and has spilled ~150 m out
over the tidal flat. The map's landscape ends at Y -900; everything past it is Topi's own ground.

## Where

| | map (X, Y) m |
|---|---|
| The sand's edge, the signs on the beach | (284, -778), (300, -800); three more signs on the upland |
| Map marker `topi` (known from the start) | (300, -850) |
| The path (`FModel::Path`, 1.1 km) | along Topi's beach to (470, -985), then inland through the dunes |
| The river Ural | along Y ≈ -1840 (`FModel::RiverY`), runs west, sinks into the sand ~250 m from the coast |
| The path's end: the sign «Урал», the towels | (150, -1762) |

## The parts

- `Private/Topi/TopiModel.{h,cpp}`: the land in plain C++ (no Unreal types). `Height` / `Sample`: the great dune over
  the map's edge, dunes of five sizes under one wind (240 m, 78 m and 19 m near; 1.1 km and 5 km "mega" dunes deep
  inside and far inland, which is what the horizon is made of), Topi's beach, the river's plain and channel, marram
  density. `Path`, `RiverY`, `RiverFlow`, `RiverDepth`. `Pressure(x, y, night)` and `WayOut`: what the place does to
  the hero. Thread-safe, any x and y.
- `tools/topi.py` writes `Content/Topi/topi_edge.tbl`: the map's own ground (`world_map.height`) in the strip where
  the sand lies over the landscape. **Rerun it when `tools/world_map.py` changes the land south of Y -700**
  (`python3 tools/topi.py --check` fails when the committed table is stale).
- `tools/topi/run.sh test | map | profile | section`: checks (the sand covers the map's edge, no steps, slopes, the
  path is walkable and ends in water deep enough, the pressure rules) and pictures (`Saved/Topi/topi_near.png`, 1 m per
  pixel with the path in red; `topi_far.png`).
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
- In the river (night only; the channel is dry sand by day) the stream pushes him west; in to the chest he dives:
  ~9 s of light, then he wakes at `PlayerStart_WakeUp` at 07:30 the next morning (`line.hero.topi.wake.1`), event
  `ural_dissolved`, `TimesDissolved` saved.

## Console and dev mode

`lyud.Topi 0` (hide, for A/B benchmarks), `lyud.Topi.Night 1` (the river now; -1 = by the sun), `lyud.Topi.Calm 1`
(no pressure), `lyud.Topi.TurnBack`, `lyud.Topi.Dissolve`, `lyud.Topi.Debug 1`, `lyud.Topi.Heave`, `lyud.Topi.Grass`,
`lyud.Topi.Props`. Teleports: `Content/Dev/teleports/topi.json` (`topi_edge`, `topi_dunes`, `topi_path`, `ural_bank`).

## Known gaps

- The sea does not know Topi's coast: off Topi's beach it is drawn as open water with no surf, and big waves may poke
  through the low beach.
- The M map ends at Y -900: the river is not on it (it is a secret).
- Chunks of different sizes meet with skirts, not stitched edges.
