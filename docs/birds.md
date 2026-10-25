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

## The player's pigeon (`blender/birds/hero_pigeon/`)

`SK_FeralPigeon_Hero` is not built by `build_bird.py` any more: it is a generated model fitted onto the species' own
skeleton (same `SKEL_FeralPigeon`, clips and `ABirdActor`). Distant pigeons keep `SK_FeralPigeon`. Every paid generation is
pinned in `gen.json` (raw files in the ignored `gen/`).

1. Views and model on hi3d (front and back of a blue-bar pigeon with its wings spread level; side views of a spread wing
   don't come out right in image models), feather scan sheets on Higgsfield (`gpt_image_2_5`, one sheet each for the
   wing and the tail plus coverts). `cut_feathers.py` cuts the sheets into 53 alpha feathers.
2. `decimate.py` brings the 2M-triangle model down to 160k (`gen/hi3d_dec.blend`). Then `build_hero_pigeon.py`
   (Blender, needs `build_bird.py FeralPigeon --hero` once for the template rig and body):
   `fit.py` turns the model into the bird's frame. The model came out upright from the view from below, so the script
   pitches it, lifts the neck so the eyes land on `eye_l`/`eye_r`, and bends the wing roots onto the skeleton's.
   It cuts the generated wings, the tail beyond its coverts and the generated legs, and reduces the body to 30k
   triangles (normal map baked from the original, albedo 4096). Skin weights come from the template body and are
   smoothed. The flight feathers are one scanned card per feather bone (`p1-p10`, `s1-s17`, `tf.*`), the coverts are the
   template's covert cards re-mapped onto the scanned coverts, and the legs are the template's.
   Slots: `Plumage` (M_BirdHeroBody: opaque, one-sided), `Skull` (the head without the bill, hidden in first person),
   `Feathers` (M_BirdPlumage), `Bare`. About 36k triangles in all.
3. `make_morphs.py` recolours the base into the colour morphs in `morphs.json` (checker, dark checker, spread, pied,
   ash-red, barless, white, brown) by what each pixel is (bar black, neck sheen, eye and skin, blue-grey ground) and
   where it is on the body (the checker's covert cells and a pied bird's white patches are 3D patterns). `ABirdActor`
   picks a morph per bird from its seed, by how common each is in a city flock (`UBirdSpeciesLibrary::PigeonMorphs`), or
   uses `Morph` when it is set.
4. `unreal/scripts/import_hero_pigeon.sh` imports it (textures, then the mesh, one process each).

Player controls on any bird with a hero body (`ABirdActor`): `SetWingPose(SpreadL, SpreadR, StrokeL, StrokeR, StrokeAmp,
Flare, TailFan, LegsDown)` drives the wings, tail and legs directly every frame. `SetFirstPerson(true)` hides `Skull`,
holds LOD0, widens the bounds and sweeps the wings forward by `ViewSweepDeg` (45) and down by `ViewDroopRad` (0.2).
`UBirdSpeciesLibrary::HasFirstPersonBody(Id)` is true when the hero body has a `Skull` slot.
Bone names in Unreal have `_` where Blender has `.` (`humerus_L`, `tf_L_3`).

Check: `tools/pigeon_shoot.sh <out>` (builds `/Game/Dev/L_HeroPigeon`, not committed, and shoots it offscreen: every
morph close up and flying, the old far body beside the new one, and first person through a wingbeat).
