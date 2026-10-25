# Birds: species, models, animation

29 Baltic species, every one built by the same generator from a table, so a new species is a few lines.

## Data

`unreal/Lyudmilovsk/Content/BirdsData/species.json` is the one table every bird system reads: id, latin name, length and
wingspan (the meshes are built to them), mass, flight style, wingbeat Hz, cruise speed, gait and stride, flock size, flee
distance, palette, calls, diet, months present in the Baltic, winter months and winter palette. Names are localization keys
`bird.<key>.name` in `strings_<lang>.json`. C++: `Public/Birds/BirdSpecies.h` (`FBirdSpecies`, `UBirdSpeciesLibrary`).

## Models (`blender/birds/`)

* `build_bird.py <Id>`: Murmuration's hero starling (rigged, card feathers, every flight feather on its own bone) made into
  a template. A species is that bird warped (torso, neck, head, throat and crown fullness), with its own beak (length, depth,
  width, roundness, curve, hook, gull's gonys angle), wing (arm and hand lengths, primary profile: rounded, pointed, sickle,
  fingered), tail (square, notched, forked with streamers, rounded, wedge), legs (shin, tarsus, toes, webs), posture, then
  scaled to the species' length and its wings solved to the species' wingspan.
* `species_art.py`: shape and plumage of each species, written like a field guide: feather styles (base colour plus layers:
  tip band, fringe, streak, drop spot, arrowhead, bars, spangle, mirror, wing patch...), a style per feather tract
  (crown, supercilium, ear coverts, malar, throat, breast, flanks, mantle, scapulars, coverts row by row, each primary, each
  tail feather) and rules for marks across tracts (the great tit's tie, the goldfinch's mask, the gull's hood).
* `plumage.py`: paints every (feather shape, style) the species uses into its 2048 atlas (albedo + alpha, normal, params:
  R roughness, G gloss), body skin coloured by tract underneath.
* Output `blender/birds/export/<Id>/` (not in git): `<Id>.glb` with LOD0..3 (`LODn_SK_<Id>`, ~15k / 10k / 7.6k / 1.2k
  triangles) and 15 clips, `<Id>_pose_<Pose>.glb` (static far poses), textures, `<Id>_rig.json`, `previews/`.

Clips (30 fps, in place): Idle Look Alert Hop Walk Peck Eat Preen Call Takeoff Flap Glide Bound Land Sleep. Flap is one
wingbeat over 1 s (play at WingbeatHz), Hop and Walk one stride over 1 s (play at speed / StrideCm). Pigeons, crows and the
wagtail bob the head when they walk, the wagtail pumps its tail, gulls soar on bent wings with spread "fingers".

## Unreal

`tools/birds_build.sh [Id ...]` builds in Blender and runs `unreal/scripts/import_birds.sh` (masters once, then per species
textures and import, one Unreal process each: Interchange imports only once per commandlet process here).

* `/Game/Birds/Materials/M_BirdPlumage` (masked, two-sided foliage: backlit feathers glow; thin-film gloss that shows at
  grazing angles, hue per species), `M_BirdBare`, `M_BirdEye`; per species `MI_<Id>_*` and `MI_<Id>_*_Winter`.
* `/Game/Birds/<Id>/SK_<Id>` (+ `SKEL_`, `PHYS_`), bones as in BirdSpecies.h, eye bones `eye_l` / `eye_r` as sockets,
  LOD screen sizes 1 / 0.16 / 0.06 / 0.02. `Anims/A_<Id>_<Clip>`. `SM_<Id>_<Pose>`: Perch Peck Hop Walk Glide Sleep Flap0-3.
* `ABirdActor` (`Public/Birds/BirdActor.h`): one bird up close, animated by state (see the header). It never moves itself.
  `Bird.Spawn <Id> [State]` in the console.

## Checking

* `tools/bird_previews.sh <refs> <out> Id ...`: Blender previews next to a reference photo (refs are not in the repo).
* `unreal/scripts/make_bird_species_level.py` builds `/Game/Dev/L_BirdSpecies` (every species perched and flying, with
  cameras), `tools/bird_shoot.sh <out> [camera prefix]` shoots it offscreen (`-game -RenderOffscreen`).
