# Characters: the standard for every person in the game

Every human in Lyudmilovsk (club regulars, the owner, the town's people, new NPCs) is made the same way, so that one
set of clips, one skeleton and one set of rules fits them all. Nao (Нао, the girl at the club's door) is the first one
made this way and is the worked example below.

## The pipeline

| Step | Tool | In → out |
|---|---|---|
| 1. Look | a reference (photo, sketch, description) | `blender/characters/<id>/source/` |
| 2. Full-body T-pose, then back, left and right views, always as grids (4 people x 3 views in one picture) cut by `tools/characters/cut_grid.py` | Higgsfield `gpt_image_2_5` sunburst, medium, 4K, transparent background (`game-assets` skill) | `gen/front|back|left|right.png` |
| 3. 3D model from the four views | hi3d (Hi3D v3.0 multi-view, 2048quality, textured, PBR) | `gen/model.glb` (~2M faces) |
| 4. Rig on the shared skeleton | `blender/characters/rig_character.py <id>` | `export/human_<id>.glb` (24k triangles, 2K textures, 56 bones) |
| 5. Import | `unreal/scripts/import_character.py <id>` (UnrealEditor-Cmd, headless) | `/Game/Characters/Humans/<Id>/SK_<Id>` |
| 6. Life, words, voice | `people.json`, `npc_dialogue.txt`, `strings_*.json`, `voice.json` | the NPC in the game |
| 7. Check | `make_life_preview_level.py` + `shoot_life_anims.py` with `LIFE_CAST=<Id>,...`; `tools/peoplesim/run.sh test` | shots, sim checks |

Steps 2–3 are pinned in `blender/characters/<id>/gen.json` and run by `tools/gen_character.py <id> <step>`; each run
writes a committed sidecar `gen/<step>.json` (endpoint, request id, inputs, output URL, sha256). Pictures and the raw
model stay local (ignored) and are refetched with `tools/gen_character.py <id> --fetch`. In UE the finished `SK_<Id>`,
its textures and material instance are what's committed.

## Anatomy and pose (what the pictures must show)

- **Strict T-pose**: arms straight out at shoulder height, palms down, fingers together, elbows straight; feet shoulder
  width apart, flat; facing the camera; neutral face, mouth closed. The whole body from hair to soles, centred.
- **Plain light grey studio background, soft shadowless light** (the texture is baked from the pictures: no cast shadows,
  no rim light, no props, no text).
- **Same scale and height in all four views**; side views are true profiles (the arm toward the camera foreshortened).
  The views come from the front picture as edits (`@front`), so face and clothes stay the same.
- Clothes that hang free (a skirt, a coat's hem, long hair) are fine: the rig blends them toward the pelvis. Anything
  that must move on its own (a bag, a scarf's tails) is a separate prop, not part of the body.
- Heights (UE cm, feet at 0): the house rules of `blender/humans/cast_rules.json` apply in spirit: young women
  155–170, young men 168–185, kids 120–145, elders a little shorter. Set in `character.json` `height` (metres).

## The skeleton (SK_Human_Skeleton)

One skeleton for every human: `root, pelvis, spine_01..03, neck_01, head, jaw, eye_l, eye_r, clavicle/upperarm/
lowerarm/hand` and 15 finger bones per hand, `thigh/calf/foot/ball` per leg (56 bones). Rest frames and bone rolls are
the MakeHuman cast's. The rig takes them from a **donor**, a MakeHuman human of the cast built by
`blender/humans/build_humans.py` (`blender/humans/export/human_<donor>.glb`; Nao's donor is `girl_a`):

1. The donor's pose is reset to rest (the importer leaves the first clip's frame on it).
2. Each arm chain is **rotated rigidly** to point straight out along X, and that pose is applied as the new rest pose
   (body and bones together). Frames and rolls ride along, so a clip (local rotations) moves the new body exactly as it
   moves the donor. Never move bones by their heads/tails to re-aim them: that recomputes the roll.
3. The donor is fitted to the model: uniform scale to the height (`donor_height_share`, hair stands a little higher),
   then each arm stretched along X to the model's span and lifted to its arm height.
4. The model (one mesh, centred, feet at 0, facing −Y in Blender = +Y in UE like every human) is **optimized**
   (Dima's rule: generated models are far too detailed): duplicates welded and specks removed, decimated to `faces`
   (24k triangles for an adult, 18k for a child), smooth shaded, the lost detail baked from a 400k copy into a 2048
   tangent normal map, the 8K base colour scaled to `texture` (2048), metal/roughness dropped (the master sets them).
   Hands are checked in close-ups; fused or broken fingers are replaced by the donor's hands. Then it takes the donor's weights by nearest surface (`POLYINTERP_NEAREST`), at most 4 per vertex, normalised.
5. **Skirt**: between the hips and `skirt.hem_z`, cloth farther than `gap` from the donor's legs is blended over
   `blend` metres toward the pelvis (half of the leg share stays on the thighs, split evenly), so it swings with the
   hips instead of tearing between the legs.

`--preview` renders `renders/characters/<id>_front|side|posed|posed_side.png` (arms down, a step) to check the
weights before importing.

## In Unreal

- `SK_<Id>` on `SK_Human_Skeleton`; the generated texture holds skin and cloth together, so one `MI_<Id>_body` on
  `M_HumanSkin` (roughness 0.75, specular 0.35). The masters must have `used_with_skeletal_mesh` and
  `used_with_morph_targets`, or `-game` draws the default checker.
- **Retargeting**: the skeleton plays the shared clips with root = Animation, pelvis = AnimationScaled, every other bone
  = Skeleton (the mesh's own lengths), `unreal/scripts/life_retarget.py`. A character's own clips (`A_<Id>_*`) get
  their own mesh as retarget source. The shared `A_Human_*` clips keep `SK_Guy_a` as source.
- No face morph targets yet (the MakeHuman cast has 52 ARKit + 15 visemes); talking uses the body clip only.
- `ANpcBody` finds the mesh by `people.json` `mesh` (`/Game/Characters/Humans/<mesh>/SK_<mesh>`); talking finds a
  person by the mesh name `SK_<Id>` → `[<Id>]` in `npc_dialogue.txt`.

## Life, words and voice

1. **People**: a row in `Content/PeopleData/people.json` (key, mesh, age, occupation, household, temper, wake/bed,
   duties, friends, start); a new kind of duty is a row in `activities.json` (`free: 0`), a new kind of place goes
   through `tools/people_map.py`. Check with `tools/peoplesim/run.sh test` and `trace <key>`; add the person to
   `docs/people.md`.
2. **Words** (the `localization` skill): `npc.<key>.name|role` and `line.<key>.<scene>.<n>` in all three
   `strings_<lang>.json`; hero lines `line.hero.<key>_<scene>.<n>`; the order in `Content/Dialogue/npc_dialogue.txt`
   (`first`, `again`, and `sun.<day|low|night|cloudy>` lines: people here are drawn to the sun, docs/story/motifs.md).
   Lines are proposals until Dima approves them (docs/story/README.md). `python3 tools/check_localization.py`.
3. **Voice**: a speaker in `localization/voice.json`: ElevenLabs v3 premade voice + direction, or a MiniMax voice
   designed once from a prompt (`fal-ai/minimax/voice-design`, prompt and preview text recorded under `design`);
   `python3 tools/gen_voice.py --lang <ru|en|ja> <keys>` (fal), then `unreal/scripts/import_voice.py`.

## Nao (the example)

- Reference `blender/characters/nao/source/nao_reference.webp`; spec `gen.json` (Nano Banana 2.1 edit, seeds
  2660–2663, 2K; Hi3D v3.0 multi-view 2048quality); `character.json` (1.60 m, donor girl_a, 24k triangles, 2K textures + baked normal, skirt hem 0.42).
  Dima asked for a shorter skirt: `front_short` (Higgsfield edit of `front`), `*_short` views and `model_short` are what
  the game uses (`character.json` `model`).
- 19, "watches the door": a duty `watch_door` at the club's door on the forecourt (where the hero first comes out),
  10:00–21:00 every day; a new game finds her there. Lines `line.nao.*` (first, four agains, all four sun states),
  voice: MiniMax designed "young Japanese woman, soft, calm, a little dreamy".
