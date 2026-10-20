# Humans (realistic, rigged, MakeHuman-based)

Every person in the club is built from **MakeHuman data through MPFB2** (CC0 base, CC0/CC-BY
clothes and hair, see [SOURCES.md](SOURCES.md)), adapted by our scripts, and imported into Unreal
on one shared skeleton.

## Pipeline

```sh
blender/humans/fetch_assets.sh                    # once: MPFB2 extension + MakeHuman asset packs
/Applications/Blender.app/Contents/MacOS/Blender -b -P blender/humans/build_humans.py -- [names] [--preview]
unreal/scripts/import_humans.sh [names]           # editor closed on this checkout
```

- `build_humans.py` holds one spec per character (`CHARACTERS`): MakeHuman macros (gender, age,
  weight, height, ethnicity), extra face/body targets, skin, hair, eyebrows, garments, tailoring
  rules, recolours, extras (sleeve covers, headset). Output: `blender/humans/<name>.blend` and
  `blender/humans/export/human_<name>.glb` (local only: LFS quota), `--preview` renders
  `renders/humans/<name>_{front,side,face}.png`.
- `mpfb_pipeline.py`: MPFB wrapper (human, rig, assets, face keys, export: modelling targets folded
  into the basis, all meshes joined into one skinned mesh).
- `tailor.py`: refits garments around the body (oversized hoodie, wide jeans), recolours textures,
  splits the tie, drops baked wear normal maps.
- `seat_anims.py`: four seated loops baked per character (see below).
- `unreal/scripts/import_humans.py`: `/Game/Characters/Humans/<Name>/SK_<Name>` + `Anims/A_<Name>_*`
  + materials on the masters `Humans/Materials/M_Human{Skin,Cloth,Hair,Eye}` (skin uses a
  subsurface profile), skeleton `/Game/Characters/Humans/SK_Human_Skeleton` shared by all.

## Skeleton and face

`root > pelvis > spine_01 > spine_02 > spine_03 > neck_01 > head > {jaw, eye_l, eye_r}`, arms
`clavicle_l > upperarm_l > lowerarm_l > hand_l > {thumb,index,middle,ring,pinky}_0{1,2,3}_l`, legs
`thigh_l > calf_l > foot_l > ball_l` (UE4 mannequin names, 56 bones). Retarget from Manny/Quinn with
an IK Retargeter (UE5 Manny has spine_04/05 and neck_02, map spine_03 to spine_05).

Morph targets on every mesh section that moves with the face: 52 ARKit face units (`eyeBlinkLeft`,
`jawOpen`, `mouthSmileLeft`, ...) and 15 visemes (`aa_02`, `p_b_m_21`, `sil_00`, ...).

Rest pose: standing A-pose, origin on the floor between the feet. **In UE the meshes face +Y**, so
an actor faces +X at yaw -90 (same as the mannequin).

## Seated loops (`A_<Name>_Sit_*`)

Origin = the floor under the chair-seat centre; the character faces its desk. Seat top 47 cm, desk
top 76 cm, keyboard 55 cm in front, mouse 26 cm to the right of it, monitor ~95 cm in front at
~112 cm. Place a SkeletalMeshActor at the chair position with yaw = (direction to the screen) - 90.

| Anim | Length | What |
|---|---|---|
| `Sit_KeyboardMouse` | 4 s | playing: left hand on WASD, right hand flicks/clicks the mouse |
| `Sit_Gamepad` | 6 s | gamepad over the lap (the pad is part of the mesh of characters that only game on it) |
| `Sit_Browse` | 8 s | surfing: right hand scrolls and clicks, left forearm on the desk |
| `Sit_ChinRest` | 9 s | reading with the chin on the left hand, right hand scrolls |
| `Sit_LeanBack` | 10 s | slouched back, legs stretched, lazy mouse |
| `Sit_Stream` | 14 s | streamer: plays, glances at the chat screen to her right, talks (jaw) and waves to the webcam on top of the monitor |
| `Sit_Idle` | 12 s | the owner: leaning back, hands on the desk, slowly looking around |

Every character bakes only its own loops (spec `loops`) with its own `style` (lean, pelvis tilt,
feet apart/crossed/tucked/stretched/wide, elbows, twist, gaze), so no two people sit alike. The bake
prints `ANIM <rig> <loop>: clean` or the frames where forearms/hands come closer than 7-9 cm to each
other or to the spine.

## Character editor (MakeHuman sliders + house rules)

MPFB2 is the editor: Sims-style sliders (gender, age, weight, height, ethnicity) plus ~1000 face and
body morphs, skins, hair, clothes. Each character's body lives in `presets/<name>.mpfb.json`
(written on the first build). To edit: Blender > sidebar > MPFB > load that human preset, move the
sliders, save it back to the same file, rebuild. `cast_rules.json` then clamps everyone into one
style: the same proportions model, age/height/weight/muscle ranges per group (kid, young, adult,
elder) and a cap on single morphs, so characters differ but stay in one family.

## Characters

| Name | Who | Notes |
|---|---|---|
| zoomer | the hero, ~20, Russian | oversized sage hoodie, wide jeans, white sneakers |
| clerk | Station 09 office worker, ~45, Japanese | white shirt, striped tie, black sleeve covers, glasses |
| owner | club owner, ~70, Japanese, full grey hair | dark sweater, trousers, monk-strap shoes |
| streamer | young woman, lilac hair | headset with boom mic, crop top, black jeans |
| kid_boy, kid_girl | ~12-13 | tees, jeans, sneakers |
| guy_a, guy_b, girl_a, girl_b | 20-25 | tees/polo/hoodie, jeans |
