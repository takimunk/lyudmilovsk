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
for i in 0 1 2; do Blender -b -P blender/residents/build_residents.py -- --shard $i/3 & done; wait
Blender -b -P blender/residents/preview_residents.py -- --limit 12 --faces    # renders/residents/*.png
unreal/scripts/import_residents.sh                     # into /Game/Characters/Humans/Town/ (see "Unreal")
```

In the cloud `Blender -b -P x.py -- args` is `python3 run.py x.py -- args` with pip bpy 5.2 (a three-line runner that
sets sys.argv and runs the script). Build output (`blender/residents/export/`, `atlases/`, `cache/`) is local only,
like every generated model (LFS quota): the scripts rebuild it. A person takes 3-7 s, all 898 about 45 minutes on four
cores. Builds are incremental: a model is rebuilt only when its look's hash or `BUILD_VERSION` changed.

## Looks (`looks.py`, plain Python)

A look is fixed by the generator seed and the person's key: as long as townsfolk.json keeps a person's key, age and
sex, they keep their face. Regenerating the town with new people only adds models.

- **Body**: MakeHuman macros from sex and the real age (`mh_age`: 0 = 1 year, 0.1875 = 11, 0.5 = 25, 1 = 90), weight
  and muscle by age band and temper, height (bodies over 1.88 m are shrunk: `CharacterStandard.h` MaxNpcHeight),
  proportions, a belly for some heavier men, double chins, eye bags and an aged head after 45.
- **Ancestry**: MakeHuman's african/asian/caucasian mix, loosely after the origin of the surname (55 %) or the given
  name (20 %), otherwise anyone; a quarter have a grandparent from elsewhere. Skin texture by age band, ancestry and
  sex, then a personal tone.
- **Face**: 30 "genes" (nose width/length/hump/tip, lips, mouth corners, chin, jaw, cheekbones, eyes, ears, forehead,
  brows...) mapped to MakeHuman modelling targets, plus one of seven head shapes. Children's genes are their parents'
  mean plus a little of their own (parents = same household, same surname, 16+ years older), so do their ancestry,
  hair and iris colour: families look alike.
- **Hair**: style by sex and age (afro mostly with african ancestry, older women bobs), natural colour by ancestry,
  greying from 35, dyed now and then; men may go bald from 30, grow a moustache or a beard.
- **Clothes** (`wardrobe.py`): style from the job (cook, waiter, doctor's white coat, worker's overalls...), age (kid,
  elder) and temper (sporty, smart); a dress, a suit or a top with a bottom, a coat for some (the game begins in late
  October on the Baltic), shoes or winter boots. Every tinted part of a garment gets a colour from a palette (elders
  quieter, kids brighter).
- **Accessories** (`accessories.py`, our own meshes): flat caps, knit beanies, baseball caps, sun hats, headscarves,
  chef's toques, backpacks, shoulder bags, string bags (avoska), shopping bags, scarves, aprons, round/rectangular
  glasses and sunglasses, a cane, moustaches and beards; one thing on the head at most.

## A model (`build_residents.py`)

1. MPFB human with the look's macros and targets, the game rig (`blender/humans/mpfb_pipeline.add_ue_rig`: the 56
   bones of `SK_Human_Skeleton`, A-pose rest, origin on the floor), eyes, brows, lashes, teeth, tongue, hair, garments
   fitted by MPFB, accessories fitted over the clothes.
2. Shape keys folded in, the body under the clothes and MakeHuman's helper geometry deleted; garments under other
   garments lose their hidden faces and sink 4 mm under the cover (a shirt showed through the coat everywhere).
3. Budget 24k triangles (the cast's standard; MakeHuman's head alone is ~9k and is never decimated): teeth to 12 %,
   garment caps (suit 5.5k, shoes 1.6k...), then the body away from the face down to at most 55 % (arms and fingers
   collapse below that), then clothes and hair by their share.
4. Six face morphs for `ULyudBodyAnimInstance` (blink, lip-sync, chatter): `eyeBlinkLeft`, `eyeBlinkRight`,
   `jawOpen`, `mouthFunnel`, `mouthSmile`, `browUp`, from MakeHuman's expression units in the person's ancestry mix;
   brows, lashes and beards take the nearest skin's motion, the lower teeth and the tongue ride the jaw rigidly.
5. One skinned mesh with four material sections and the tint in UV channels, exported as glb (no images) with a
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

Tinted cloth in the atlas is grey detail with a linear mean of 0.5 per tint slot (so the mean of the result is the
tint), logos painted out (`clean`), texels that must stay as painted (a white shirt under a suit: `keep_texels`) have
alpha 0. The skin's tint is half its tone multiplier; hair, brows and beards are grey detail times the hair colour.
`preview_residents.py` builds the same maths in Blender nodes for the QA renders.

## Unreal

Assets: `/Game/Characters/Humans/Town/<Model>/SK_<Model>` on `SK_Human_Skeleton` with the people's LODs
(`lod_people.py`), atlases in `Town/Textures`, the masters and four `MI_Resident_*` in `Town/Materials`. The game finds
them through `town_looks.json`: town_life.py copies each model into the person's townsfolk.json `body` (the pool
model stays in `mesh` until the model is imported), PeopleLife shows a townsperson's own model when the package exists
and loads it asynchronously near the camera (branch town-life).

## Limits and next steps

- The npm assets have 512 px textures: faces are soft up close. The MakeHuman packs on the Mac have larger skins;
  `atlases.py --tile 1024` uses them (rebuild the models after changing the atlases: their UVs point into tiles).
- One outfit all year (autumn clothes); seasonal variants would be a second build per person.
- The eyeballs are real geometry on the eye bones, but nothing turns those bones yet (the cast's gaze works through
  their face material); the head still turns toward what they look at.
