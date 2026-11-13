# Residents: a model of their own for every townsperson

The town's ~900 people (tools/town/town_life.py -> `Content/PeopleData/townsfolk.json`, docs/town_life.md) each get
their own rigged MakeHuman model, made by a generator from their key: body, face, age, skin, hair, clothes, colours
and accessories. Nothing is drawn by hand or bought: MakeHuman's CC0/CC-BY assets (blender/residents/SOURCES.md),
MPFB2 in Blender, and our scripts.

## Pipeline

```sh
python3 tools/residents/mhdata_assets.py               # once: MakeHuman assets into MPFB's user data (the npm
                                                       # package where the MakeHuman packs can't be downloaded)
python3 blender/residents/looks.py [--ref origin/town-life] [--summary]   # looks.json + Content/PeopleData/town_looks.json
Blender -b -P blender/residents/wardrobe_scan.py       # once per asset set: tint slots + masks of every garment
Blender -b -P blender/residents/atlases.py -- [--tile 512]                # the four shared texture atlases
for i in 0 1 2 3; do Blender -b -P blender/residents/build_residents.py -- --shard $i/4 & done; wait
python3 blender/residents/check_export.py              # the models as the import expects them (no Blender needed)
Blender -b -P blender/residents/preview_residents.py -- --limit 12 --faces    # renders/residents/*.png
unreal/scripts/import_residents.sh                     # into /Game/Characters/Humans/Town/ (see "Unreal")
```

In the cloud `Blender -b -P x.py -- args` is `python3 tools/residents/bpyrun.py x.py -- args` with pip bpy 5.2 (it
sets sys.argv as Blender would and runs the script). Build output (`blender/residents/export/`, `atlases/`, `cache/`)
is local only, like every generated model (LFS quota): the scripts rebuild it. A person takes 3-7 s, all 898 about 20
minutes on four cores. Builds are incremental: a model is rebuilt when its look's hash, `BUILD_VERSION` or the atlas
layout changed (the sidecar's `atlas`: an id of layout.json's tiles, which the UVs point into). `atlases.py` stops
rather than write atlases without the accessories' tiles (accessories.py needs bpy: run it with Blender).
`Blender -b -P blender/residents/test_accessories.py -- --only checks` builds eight test people and fails (exit 1) on
an accessory sunk into the body, over its triangle budget, off its material or torn in a walk.

## Looks (`looks.py`, plain Python)

A look is fixed by the generator seed and the person's key: as long as townsfolk.json keeps a person's key, age and
sex, they keep their face. Regenerating the town with new people only adds models. But town_life.py's keys come from
its seed and its inputs, so a change to the plan, the buildings or the public spaces gives everybody new names: then
`looks.py` and a build again (the old models stay in `export/` until deleted, unlisted and never imported).

- **Body**: MakeHuman macros from sex and the real age (`mh_age`: 0 = 1 year, 0.1875 = 11, 0.5 = 25, 1 = 90), weight
  and muscle by age band and temper, height (bodies over 1.88 m are shrunk: `CharacterStandard.h` MaxNpcHeight),
  proportions, a belly for some heavier men, double chins, eye bags and an aged head after 45.
- **Ancestry**: MakeHuman's african/asian/caucasian mix, loosely after the origin of the surname (55 %) or the given
  name (20 %), otherwise anyone; a quarter have a grandparent from elsewhere. Skin texture by age band, ancestry and
  sex, then a personal tone: a caucasian or asian skin darker on a mixed body, an african one lighter (up to 1.7x).
  `atlases.py` matches the colour balance of MakeHuman's old african female and old asian male skins to their
  middle-aged siblings (the first was lobster red).
- **Face**: 30 "genes" (nose width/length/hump/tip, lips, mouth corners, chin, jaw, cheekbones, eyes, ears, forehead,
  brows...) mapped to MakeHuman modelling targets, plus one of seven head shapes. Children's genes are their parents'
  mean plus a little of their own (parents = same household, same surname, 16+ years older), so do their ancestry,
  hair and iris colour (brown wins over light eyes; light eyes get rarer the less european the mix): families look
  alike.
- **Hair**: style by sex and age (afro mostly with african ancestry, older women bobs), natural colour by ancestry,
  greying from 35, dyed now and then; men may go bald from 30, grow a moustache or a beard.
- **Clothes** (`wardrobe.py`): style from the job (cook, waiter, doctor's white coat, worker's overalls...), age (kid,
  elder) and temper (sporty, smart); a dress, a suit or a top with a bottom, a coat for some (the game begins in late
  October on the Baltic; everybody in straps, a crop top or a sleeveless dress gets one, children too, a third of the
  town in all), shoes or winter boots. No shorts: MakeHuman's tee-and-denim-shorts suit is swapped for the tee and
  jeans after the draw (`AUTUMN_SWAP`). Every tinted part of a garment gets a colour from a palette (elders quieter, kids brighter).
- **Accessories** (`accessories.py`, our own meshes): flat caps, knit beanies, baseball caps, sun hats, headscarves,
  chef's toques, backpacks, shoulder bags, string bags (avoska), shopping bags, scarves, aprons, round/rectangular
  glasses and sunglasses, a cane, moustaches and beards; one thing on the head at most, no glasses under a fringe
  over one eye (bob01/bob02: the frame showed over the hair). Facial hair is strand shells with the `acc/beard` tile:
  a solid root band, strands thinning out downwards; the shells map it sparse at their top edge, solid a third of the
  way down and ragged at the bottom, and moustaches taper and droop towards the corners.

## Named people (`CAST` in `looks.py`)

A named person of `people.json` whom nobody modelled by hand gets a model from the generator too. `CAST` lists them
with what their story fixes (ancestry, build, hair, clothes, accessories); the rest is drawn from their key like
anybody's. The model takes the name of their `body` (Аслан Хуажев, the cheese seller: `Khuazhev_aslan`), so the game,
their lines (`npc_dialogue.txt` goes by body) and their quests find them as before. `town_looks.json` lists them under
their key (no `t_`) with that model, and the import puts it where the game looks: `/Game/Characters/Humans/<Body>/
SK_<Body>`, local only like `Town/` (it uses Town's atlases and materials; `.gitignore` names the folder). `looks.py`
leaves out anybody whose body is a hand-made model in git, so the import never replaces one. Until the model is
imported the person lives unseen when their stand-in (`mesh`) is somebody else's face (docs/people.md).

```sh
Blender -b -P blender/residents/build_residents.py -- --only khuazhev_aslan
unreal/scripts/import_residents.sh --only khuazhev_aslan
```

## A model (`build_residents.py`)

1. MPFB human with the look's macros and targets, the game rig (`blender/humans/mpfb_pipeline.add_ue_rig`: the 56
   bones of `SK_Human_Skeleton`, A-pose rest, origin on the floor), eyes, brows, lashes, teeth, tongue, hair, garments
   fitted by MPFB, accessories fitted over the clothes.
2. Shape keys folded in, the body under the clothes (MakeHuman's delete groups, then every body face that five rays
   around its normal find under a garment) and MakeHuman's helper geometry deleted. Dresses and tops are
   smoothed (Taubin, so nothing shrinks) away from their edges: MakeHuman fits a garment vertex by vertex, so it showed
   nipples and navels. A dress also hangs below the hips as a convex skirt with its hem heights smoothed: on a girl
   it went into the crotch with a spike at the hem. Nothing comes closer than 6 mm to the body that is left.
3. Layers: shoes 0.5 (under the trousers), bottoms, suits and dresses 1, tall boots 1.5 (trousers tucked in), tops 2,
   coats 3. MakeHuman's clothes are each fitted to the body, so a bulky shirt pokes out of a slim coat and a sock out
   of a trouser leg. For each vertex of the inner garment `hide_under_layers` takes its nearest point on the whole
   body and casts a ray from there along the body's normal to the cover's lowest layer: a vertex higher than 5 mm
   under that goes down to it (rays from the inner garment itself got lost in folded lapels and pockets on top of the
   coat). Faces go whose vertices all see the cover in five directions around their normal (one ray straight out hid
   the trousers just under a flared hem), and faces with the cover right above them take its tint, so what still
   shows through a seam of the cover (the coat's armpits) reads as the coat. The push runs again after the budget:
   decimation flattens the coat's shoulders.
4. Budget 24k triangles (the cast's standard; MakeHuman's head alone is ~9k and is never decimated): teeth to 12 %,
   garment caps (suit 5.5k, shoes 1.6k...), then the body away from the face down to at most 55 % (arms and fingers
   collapse below that), then clothes and hair by their share.
5. Six face morphs for `ULyudBodyAnimInstance` (blink, lip-sync, chatter): `eyeBlinkLeft`, `eyeBlinkRight`,
   `jawOpen`, `mouthFunnel`, `mouthSmile`, `browUp`, from MakeHuman's expression units in the person's ancestry mix;
   brows, lashes and beards take the nearest skin's motion, the lower teeth and the tongue ride the jaw rigidly.
6. One skinned mesh with four material sections and the tint in UV channels, exported as glb (no images) with a
   sidecar JSON (triangles, height, morphs, hash).

### Materials: four for everybody

| Section | Atlas | What's in it |
|---|---|---|
| `Skin` | T_Resident_Skin (20 skins) | body |
| `Cloth` | T_Resident_Cloth (garments, shoes, accessory tiles) | clothes, shoes, hats, bags, aprons, glasses |
| `Hair` | T_Resident_Hair (alpha) | hair, brows, lashes, beards, string bags (masked, two-sided) |
| `Eyes` | T_Resident_Eyes | irises, teeth, tongue |

Each source texture has a tile in its atlas (`atlases/layout.json`); mesh UVs are squeezed into the tile. A person's
colours ride in UV1/UV2 as linear RGB plus an amount (not in vertex colours: their sRGB/linear handling differs
between exporters and importers):

```
T = Atlas(UV0);  tint = (UV1.x, UV1.y, UV2.x);  amount = UV2.y * (Cloth ? T.a : 1)
BaseColor = lerp(T.rgb, T.rgb * 2 * tint, amount)
```

(In Blender. glTF stores every UV with V flipped and Unreal keeps glTF's convention, so in Unreal it reads
`1 - UV1.y` and `1 - UV2.y`: the masters' `TintFlipV` switch.)

Tinted cloth in the atlas is grey detail with a linear mean of 0.5 per tint slot (so the mean of the result is the
tint), logos painted out (`clean`), texels that must stay as painted (a white shirt under a suit: `keep_texels`) have
alpha 0. The skin's tint is half its tone multiplier; hair, brows and beards are grey detail times the hair colour.
`preview_residents.py` builds the same maths in Blender nodes for the QA renders.

## Unreal

`unreal/scripts/import_residents.sh` imports the built residents into `/Game/Characters/Humans/Town`, with the editor
closed on this checkout; it waits in the UE queue (`tools/wait_ue_queue.sh`), runs under `nice -n 19 taskpolicy -b`
(`PRIO=` for full speed, `JOBS=2` for two editors) and backs up `SK_Human_Skeleton` around every run. The first editor
run makes the shared assets: the atlases `Textures/T_Resident_{Skin,Cloth,Hair,Eyes}` (sRGB, BC7 with alpha, Hair
with alpha-coverage mips), the masters `Materials/M_Resident_Opaque` (subsurface, two-sided; black scatter on Cloth and
Eyes) and `M_Resident_Masked` (Hair, clip 0.5), and one instance per section, `MI_Resident_*` (roughness Skin 0.55,
Cloth 0.85, Hair 0.5, Eyes 0.2). Each following run imports up to 60 people as `Town/R_<key>/SK_R_<key>` on
`SK_Human_Skeleton` (never a new skeleton): Nanite off, the six face morphs, UV0-2, four LODs of their own
(`Town/LOD_Resident`: 100/45/18/6 % of the triangles, 24k/11k/4.3k/1.4k, from screen sizes 1/0.45/0.2/0.08; the cast's
`LOD_HumanPeople` caps LOD3 at 5000, more than a resident's LOD2) and a physics asset. The runs go on until nothing is left; a person who fails twice, or takes the editor down twice, is
left out and listed. The masters compute `lerp(T, T * 2 * tint, amount)` with tint = (UV1.x, 1 - UV1.y, UV2.x) and
amount = 1 - UV2.y, times T.a on Cloth (static switch `AlphaIsTintMask`). The `1 - y` undoes the V flip of Blender's
glTF exporter and sits behind the static switch `TintFlipV`: if people come out grey or green, flip it in the two
masters; no mesh needs re-importing.

The import is incremental: each mesh carries its sidecar's `hash/build/atlas` as the metadata tag `ResidentLook`, and
`Saved/ImportResidents/models/<Model>.json` records every import, so a rerun only imports new or rebuilt people.
`--force` re-imports, `--retry-failed` retries failures, `--only key,key` and `--limit N` select people, `--prune`
deletes folders no longer in `town_looks.json`. `python3 unreal/scripts/import_residents.py --report` gives the tally
without the engine; the log is `Saved/ImportResidents/import.log`. Before any editor starts, the wrapper runs
`blender/residents/check_export.py` (no Blender, no engine; `CHECK=0` skips it) and stops if a model is off: one skin on
the 56 bones of `SK_Human_Skeleton` with no weight on a joint past it, the four sections, the six morphs on every
section and none on by default, UV0-2, normals, skin weights that sum to 1, UV0 inside its section's atlas tiles
(their padding allowed), the tint UVs inside 0..1, no triangle of zero area. One wrapper runs at a time (a lock in
`Saved/ImportResidents`); it won't start while an import editor of a killed wrapper still runs, looks for an open
editor again before every run, and if a killed wrapper left its skeleton copy and the skeleton differs from it, it
stops and asks which one is right instead of taking the skeleton as the good copy.

```sh
python3 blender/residents/check_export.py                # no engine: 898 models checked, 0 with problems
python3 unreal/scripts/import_residents.py --report      # no engine: 898 to import
unreal/scripts/import_residents.sh --limit 3             # shared assets + 3 people: look at them in the editor first
unreal/scripts/import_residents.sh                       # the whole town; stop and restart any time, it resumes
```

Not tried in a real editor yet (the cloud has none): check with `--limit 3` that the static switches link (the prepare
run prints `MISSING` lines otherwise), that LOD1-3 keep UV1-2 (people would turn grey at a distance), that the meshes
have the six morphs (`--report`) and that the sections come in as Skin/Cloth/Hair/Eyes. The faces only move if
`SK_Human_Skeleton` has the six face curves (the cast's import adds them; the prepare run warns). Every
townsperson's `body` in townsfolk.json is `Town/R_<key>` (the key without `t_`, the name `looks.py` gives the model;
a model listed in `town_looks.json` wins: town-life 4f94ff4), and PeopleLife loads it asynchronously near the camera,
showing the pool model in `mesh` until the package exists. The package is
`/Game/Characters/Humans/Town/R_<key>/SK_R_<key>`: `PeopleModelPath` and `ANpcBody::Setup` take the model's last
segment for the `SK_` name and `PeopleModelOf` gives `Town/R_<key>` back (town-life 4c32ae3).

## Limits and next steps

- The npm assets have 512 px textures: faces are soft up close. The MakeHuman packs on the Mac have larger skins;
  `atlases.py --tile 1024` uses them (rebuild the models after changing the atlases: their UVs point into tiles).
- One outfit all year (autumn clothes); seasonal variants would be a second build per person.
- The eyeballs are real geometry on the eye bones, but nothing turns those bones yet (the cast's gaze works through
  their face material); the head still turns toward what they look at.
