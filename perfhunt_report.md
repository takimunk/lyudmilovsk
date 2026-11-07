# Perfhunt report (2026-11-06/07 night)

Branch `perfhunt` (from origin/main c983e2ad). The audit notes for each area are in `docs/perfhunt/` (findings with file:line,
what was fixed, how to check each one in a run, and the independent reviews of the diff).

## Status

- **Compiles:** the full LyudmilovskEditor Mac Development build succeeds (0838303e; only engine deprecation warnings that
  were already there).
- **Host tests pass:** audiosim (30 renders byte-identical to baseline), weather, winter, sea, transportsim, peoplesim, railsim.
  birdsim: 120 pass and 2 fail, the same 2 that fail on baseline c983e2ad (gulls on the low-tide sand; the albatross in a calm).
- **Engine runs:** blocked overnight. Xcode updated itself to 27.2 on Nov 4 and its Metal Toolchain is uninstalled, so every
  UnrealEditor run on this Mac quits at startup ("cannot execute tool 'metal' due to missing Metal Toolchain"): bench, soak,
  offscreen tests, Dima's editor, every thread. The fix is `xcodebuild -downloadComponent MetalToolchain` (Apple, ~0.7-1 GB);
  it waits for Dima's OK. (Installed 2026-11-07 morning with Dima's OK; numbers below.)
- To get them once the toolchain is in: the baseline binaries of c983e2ad are saved in
  `~/projects/lyudmilovsk-wt/perfhunt_base_bin/Binaries`. Copy them over `unreal/Lyudmilovsk/Binaries` and run
  `tools/bench.sh 3` and `tools/soak.sh <dir>`. Then rebuild perfhunt (tools/wait_ubt_queue.sh) and run the same two. Run
  them back to back, alone (`--exclusive` is built in), and check `sysctl vm.swapusage` and the idle GPU first.
- The soak tool (`tools/soak.sh` + `unreal/scripts/soak_clock.py`): the Epic bench route flown ROUNDS times (default 20,
  ~33 min) with the game clock at 60x, so one run sees day, night and dawn. It writes the process footprint every 60 s and
  SOAK lines (mean frame ms, live actors/components/MIDs/static meshes/textures/RTs/audio components) to show any growth.

## Measurements (2026-11-07 morning, M5 Max, editor -game offscreen 1280x720, Epic route, each run alone)

Once the Metal Toolchain was in: main's binaries (c983e2ad, saved before the fixes) and perfhunt's (63b7f908) were swapped in
the same checkout and content. A Brave tab held the GPU at ~60% the whole time (no UE running; perf checklist 7b), and the
machine drifted slower run after run (main alone went 20.5 -> 24.3 -> 28.2 ms). Frame and GPU times are therefore **not
conclusive**; the CPU-side numbers are consistent.

| run (in order) | route median | game thread | RHI | GPU | hall | corridor | forest | flight | mem GB |
|---|---|---|---|---|---|---|---|---|---|
| main | 20.51 | 5.49 | 2.95 | 23.5 | 15.6 | 19.6 | 67.2 | 19.7 | 20.46 |
| perfhunt (after the main soak) | 23.39 | 5.14 | 3.27 | 27.7 | 17.0 | 21.7 | 74.1 | 22.7 | 20.27 |
| main | 24.33 | 5.96 | 3.58 | 29.5 | 16.8 | 22.3 | 69.5 | 23.9 | 20.44 |
| perfhunt | 28.65 | 4.57 | 2.83 | 33.3 | 21.6 | 29.7 | 81.4 | 25.6 | 20.38 |
| main | 28.23 | 5.42 | 3.07 | 32.4 | 22.3 | 28.3 | 78.2 | 25.7 | 20.37 |
| perfhunt | 27.37 | 4.50 | 2.71 | 31.6 | 21.3 | 26.9 | 76.9 | 25.4 | 20.32 |

- **Game thread: -16%** (main 5.62 ms mean of 3, perfhunt 4.74), in every run and segment group. Outdoors it's -18..-27%
  (forest 6.1 -> 5.0, flight 5.5 -> 4.6). RHI thread -8%.
- **Frame / GPU: within noise.** The interleaved pairs were +4.3 and -0.9 ms against the run before; GPU-bound under the
  foreign load.
- **Memory:** the same within 0.1-0.2 GB in the bench.

Soak (`tools/soak.sh`, 20 laps of the route, ~33 min, game clock x60 from 16:00 through the night to the next evening):

| | main | perfhunt |
|---|---|---|
| lap medians: mean / median of laps (ms) | 26.16 / 27.69 | 26.88 / 28.84 |
| memory over the run (bench mem, MB) | 20370 -> 21220 | 19900 -> 21200 |
| footprint (GB, coarse) | 18 -> 21 | 18 -> 21 |
| actors first -> last | 5220 -> 5179 | 5226 -> 5176 |
| MIDs first -> last | 7669 -> 7790 (+121) | 7358 -> 7470 (+112) |
| static meshes | 1951 -> 2040 | 1928 -> 2022 |
| render targets | 82 -> 92 | 24 -> 62 |
| crashes / ensures | none | none |

- Neither build leaks over 33 min. Actor counts rise in the game's morning and fall back by evening; MIDs and meshes grow
  ~1.5% (streaming/plants), and the growth is the same in both.
- perfhunt holds ~310 fewer MIDs (shared goods materials) and ~58 fewer render targets at the start (cabin widgets drawn
  only near).
- Prefetch in -game: every folder reported a non-zero asset count ("Prefetch: /Game/Birds/Starling has 66 assets",
  Animals/Cat 58, Humans/Anims 27, ...), so the registry fallback isn't needed but is harmless.
- **To finish:** with the Brave tab closed (`ioreg ... "Device Utilization %"` near 0 while idle), run main/perfhunt/main/
  perfhunt again: `cp -cR ~/projects/lyudmilovsk-wt/perfhunt_{base,ph}_bin/Binaries/. unreal/Lyudmilovsk/Binaries/` then
  `tools/bench.sh 3`.

## Audit

Round 1: 7 Opus agents, one per area (~110 findings). Round 2: 3 cross-cutting passes (object lifetime/GC, save/load + sync
loads, everything that ticks) plus read-only reviews of tonight's 8 new branches (notes sent to the coordinator: see the end).
Every fix was reviewed by 3 independent Opus reviewers against the 5.8 headers and the stated intent. They found no blockers;
their SHOULD items are fixed. The code-review thread's re-review of 07dfbbc5 is fixed too.

## Fixed, by severity

### S1: crashes, data loss, leaks that grow

| What | Where | Fix |
|---|---|---|
| Restaurant/diner meal meshes collectable mid-meal -> next bite swaps in a freed mesh | Food/LyudBite | meal assets held in a UPROPERTY for the meal (e8085366) |
| Bird trust and the cats'/dogs' trust, names and bond never survived a load (registered after the load was applied) | Birds/BirdLife, Animals/AnimalLife | register at OnWorldBeginPlay; Stop syncs first (60d8e699) |
| Every save held the previous save's bird/animal/corvid/people state (OnBeforeSave fired after the write) | LyudSave | broadcast before writing (60d8e699) |
| Quest progress from an older save read past the end of Counts (assert / out of bounds) | UI/LyudQuests | stage and counts clamped on load and at use (60d8e699) |
| CounterTrade cache read out of range in the first frames (code review BLOCKER on the first push) | People/CounterTrade | false until the sim exists; rebuilt on count change (a5197c55) |
| Vending Take callback wrote to a destroyed machine (raw this, 2.8 s later); inventory lambdas the same | VendingMachine, LyudInventory | weak captures (80557208) |
| Booth owner's clip invisible to GC while sampled | People/LyudBodyAnim proxy | AddReferencedObjects (38ca628d) |
| Turning lyud.Birds / Nests off and on left up to 12 bird bodies ticking and drawn (and skewed A/B benches) | Birds/BirdLife renderer | EndPlay destroys them (33a04151) |
| Cable car rider list grew all session and was scanned constantly | Transport/CablewaySim | finished rider slots reused (0dcb6deb) |
| HeroSeat spawned a new sequence actor per load, never freed | HeroSeat | old one destroyed (80557208) |
| Static TStrongObjectPtr fonts/textures/drawings destroyed after the UObject system at exit | UI/LyudDiaryStyle, Birds/BirdBook | rooted raw pointers (80557208, 60d8e699) |
| Hidden (pooled) bodies held doors open and stayed in the door check | Doors/LyudDoor | hidden agents pass no door, keep one only while in the sweep (49f51a0e, 29ea4729) |

### S2: frame cost and hitches (estimates from reading the code; to be measured)

- **Birds:** instance upload only up to the shown count instead of the padded buffer (after a murmuration that was ~6000
  transforms a frame for one starling); per-frame arrays reused; feeder like-table cached; handful/bag without render-state
  churn; movers out of Lumen.
- **Plants / forest floor:** the 128x128 litter field is built over frames into a back buffer (a 10-25 ms hitch every 24 m).
  Floor items are stored per kind, so a commit walks only its own kind. Falling leaves are updated in place instead of
  cleared and refilled every frame. Fruit trace distance-gated. Herb meshes built fresh in play (no render flush). Idle
  plants tick at 0.2/0.5 s. Terrain mask held instead of re-loaded after GC. Academ lamp list cached instead of a walk over
  all actors every 0.5 s. Prefetch FolderReady remembers ready folders.
- **Sky / sea:**
  - Space-view lights set once a frame (they were flipped twice, rebuilding light states and the sun's VSM cache every
    frame). Sun/moon rotate only past 0.01 deg.
  - The Mie setter uses a relative threshold (the atmosphere was re-made almost every frame).
  - The sea depth map is time-sliced (8000 complex traces a frame for ~130 frames after load -> 12 ms a frame,
    `lyud.Sea.DepthMapMs`).
  - Sea life and seabed get hysteresis and a 30 s park instead of a full rebuild at every wave over the boundary.
- **Weather / audio:**
  - Grain pan gains are computed once (35-45% of a core in storms -> ~20% less DSP measured in audiosim, output
    bit-identical). The muted ambience sleeps.
  - The winter season replay (~2900 weather steps on the game thread) is skipped when no snow lies.
  - The rain roof map has no per-cell 36 KB copies and is spread after teleports.
  - Winter MPC loaded once instead of per frame. Decal sweep only with snow. Sea-ice collision re-laid per tile, not per
    1.5 m.
- **Transport / doors / railway:**
  - World widgets (cabin posters/boards/plates, station boards, rail signs) redrew their render targets every frame (about
    60 a frame from the cabins alone). They now redraw at 0.25-5 s, and cabin widgets draw only when near.
  - Rope segments share end points (half the rope maths). Doors pre-filter agents with a sphere.
  - Cabins, train and lobby movers are out of Lumen. The train wake is a stable pool without add/remove per frame.
  - The hidden train skips its car layout. The prompt is rebuilt on change.
  - The lobby steam MID is now made from M_TR_Glass: the ~8 'not a valid parent' warnings per load are gone, and the
    puffs draw as glass now.
- **People / animals:**
  - Club people and the booth owner tick their pose only when rendered: they animated and traced IK at any distance.
  - About 180 moving eyeballs and props are out of Lumen.
  - The blanket mesh is built fresh (no rebuild of a drawn mesh). Desk and foot traces cached or distance-gated. Bone
    indices cached (no Printf per bone).
  - CounterTrade no longer fills a person record per counter per frame. Seat lists are cached. Smoke rewrites only
    changed puffs.
- **UI / hero:**
  - Shared goods/bottle materials (~450 MIDs before) and a 25 m cull on shelf stock.
  - About 170 failing LoadObject calls at lobby start are gone (one ru pass, missing-path cache). lobby.json is parsed
    once instead of per machine.
  - The ATON screen repaints at 2 Hz when idle. The volume ini is written on release, not on every slider move.
  - The HUD clock is built per minute. Language() is cached. Hero component lookups are cached.

### S3 (selection)

- **Bugs:**
  - The answering bird's clip restarted every frame (jitter). The hawk ignored its prey weighting.
  - The 12 near bird bodies had no direct sun (lighting channel).
  - The Academ on/off toggle re-enabled ticking on every actor.
  - Europa left lyud.Rain.Fx off. Lip-sync lagged for lines started off-screen.
  - 'no train' was logged on every query with the railway off.
- **Log spam:** a Log line for every wild bird notice within 30 m (283 of 2842 lines in a bench log) is now behind
  `lyud.Tame.Log`.
- **Small per-frame costs:** garland flicker, conveyor, curtain pusher and station lifts now return early when idle or far.
  Player light channels are sampled at 10 Hz.
- **Load hitches:** bird call sounds are cached. In an uncooked -game the prefetch scans its folder if the asset
  registry hasn't got to it yet (it logs "Prefetch: <folder> has N assets"; check that N > 0 in the first run).

## Deliberate behaviour changes to look at in the first run

- **Sea:** after a level load the sea's depth map takes ~10 s of frames, not ~2 s. Seabed, fish and pools come after
  that; fast sea tests may want `lyud.Sea.DepthMapMs 0`.
- **Lighting and looks:**
  - Near bird bodies are now sunlit like the instanced birds.
  - The lobby steam puffs now draw as faint glass (they had no material).
  - Cabin posters and boards show within 60 m, plates within 300 m.
- **Logs:** the taming log and "BIRD meet" moved to Verbose.
- **Hawk:** the hawk's random sequence changed.

## Not fixed (and why)

- Seabed rebuild at 8 m tile crossings (SEA-1): a per-tile cache wouldn't be exact. Snow footprint recentring every 2 m
  (W1, a 15-50 ms hitch in snow) and the snow trail refresh (W4/W6): need an incremental grid or material changes, too
  big to do blind.
- Lazy per-species bird pose setup (BIRDS-03, a 250-500 load hitch at bird start). Live tree swap-in on the game thread
  (PL-10). Europa music sync load (EUR-2). The first diary open loading synchronously. Typing sounds on every placed human
  (W16). Gull respawn per train pass (T13).
- Save keys built from auto-generated actor names (SV-4: forest live plants, station doors). It loses those records in a
  packaged build and needs fixed names. SV-6: bird trust keyed by sim index, which shifts with month/population.
- Editor ~9 GB kept after PIE (known, engine-side).

## Notes for the other threads (read-only reviews, details in docs/perfhunt/branch_*.md)

- **fishing:**
  - S1: FishingRod's unrooted static material.
  - Every take-out builds 16 meshes and 22 MIDs. The strike sync-loads.
- **underwater-eco:**
  - O(N^2) Ambush. A 6-year warm-up on the game thread.
  - The school bAlive bug. The crawler timer counted twice.
  - Merge with perfhunt's LyudUnderwater by hand, plus a bActive guard on TakeFish.
- **restaurant:** restaurant.json is parsed every frame while open. EnsureWaiter and the waiter animation run every frame
  at any distance. RestMesh is uncached. Stale dish/bill prompts and the Orders index bugs.
- **town-life:** TalkPairs is 900x900 every 3 s. The door side is fixed here.
- **city-terrain:** the window ISMs are rebuilt on each change, and the shop windows stay dark.
- **city-buildings:** ~1900 movable point lights. A texture upload per call. Windows freeze when town-life stops.
- **festivals:** venues re-parsed every 2 s. Firework sounds path-loaded per shell. Lantern money spent before Release
  can fail.
