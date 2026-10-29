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
   (body and bones together). Frames and rolls ride along. Never move bones by their heads/tails to re-aim them: that
   recomputes the roll. **This changes the arms' rest rotations**, so a clip made on an A-pose body must not be played
   as is (its local rotations are absolute: the wrists roll, the legs cross); clips are carried over in world space
   (see "Clips" below).
3. The donor is fitted to the model: uniform scale to the height (`donor_height_share`, hair stands a little higher),
   then each arm stretched along X to the model's span and lifted to its arm height; the whole donor shifted so its
   hips' cross-section has the model's middle (the model is centred on its bounding box, hair and all); knees and
   ankles moved into the middle of the model's legs (`fit_legs`: the generated stance is narrower than MakeHuman's,
   by 5-12 cm at the ankle).
4. The model (one mesh, centred, feet at 0, facing −Y in Blender = +Y in UE like every human) is **optimized**
   (Dima's rule: generated models are far too detailed): duplicates welded and specks removed, decimated to `faces`
   (24k triangles for an adult, 18k for a child), smooth shaded, the lost detail baked from a 400k copy into a 2048
   tangent normal map, the 8K base colour scaled to `texture` (2048), metal/roughness dropped (the master sets them).
   Hands are checked in close-ups; fused or broken fingers are replaced by the donor's hands (`character.json`
   `"hands": {}`, `replace_hands`: the mitten is cut where the sleeve ends, found by colour, the donor's MakeHuman hand
   set in 4 cm into the sleeve, its colour one even skin tone from the cut-off hand). `"protect"` zones (a box,
   optionally only its dark warm vertices, a collapse weight below 1) keep thin parts (glasses' frames) and the face
   denser through the decimation; `"no_shadow"` zones (same form) get vertex colour alpha 0, which `M_HumanFace` leaves
   out of the shadow pass (glasses' frames laid a hard ring of shade on the cheek). The adult budget counts the hands
   (~5.7k) and the face rig (~4k): Lev Zotov's `faces` is 15000 for ~23k in all. Then it takes the donor's weights by nearest surface (`POLYINTERP_NEAREST`), at most 4 per vertex, normalised.
5. **Skirt**: between the hips and `skirt.hem_z`, cloth farther than `gap` from the donor's legs is blended over
   `blend` metres toward the pelvis (half of the leg share stays on the thighs, split evenly), so it swings with the
   hips instead of tearing between the legs.
6. **Loose hems** (every character): cloth round the hips standing off the skin moves from the thighs to the pelvis
   (`relax_hem`), or sitting drags a shirt's back out like a bag; the **shoulders'** weights are smoothed within 13 cm
   of each shoulder joint, so arms hanging down don't pinch the sleeve into a ridge; the head is locked (`lock_head`).
7. **Face** (`blender/characters/face_rig.py`, below).

`--preview` renders `renders/characters/<id>_front|side|posed|posed_side.png` (arms down, a step) to check the
weights before importing.

## In Unreal

- `SK_<Id>` on `SK_Human_Skeleton`; the generated texture holds skin and cloth together, so one `MI_<Id>_body` on
  `M_HumanSkin` (roughness 0.75, specular 0.35). The masters must have `used_with_skeletal_mesh` and
  `used_with_morph_targets`, or `-game` draws the default checker.
- **Retargeting**: the skeleton plays the shared clips with root = Animation, pelvis = AnimationScaled, every other bone
  = Skeleton (the mesh's own lengths), `unreal/scripts/life_retarget.py`. A character's own clips (`A_<Id>_*`) get
  their own mesh as retarget source. The shared `A_Human_*` clips keep `SK_Guy_a` as source.
- `MI_<Id>_body` is on `M_HumanFace` (`unreal/scripts/make_face_material.py`: M_HumanSkin plus the cut eye openings),
  its eye parameters set from `eyes.json` by `import_character.py` (or `unreal/scripts/set_eyes.py` on its own).
- Bodies play through `ULyudBodyAnimInstance` (`Public/People/LyudBodyAnim.h`): clips cross-fade, the face lives (below).
  `ANpcBody` uses it; `ULyudFaceSubsystem` switches the level's placed people (SkeletalMeshActors on the human skeleton
  playing one clip) to it at begin play.
- `ANpcBody` finds the mesh by `people.json` `mesh` (`/Game/Characters/Humans/<mesh>/SK_<mesh>`); talking finds a
  person by the mesh name `SK_<Id>` → `[<Id>]` in `npc_dialogue.txt`.

## Clips

- **Shared clips** `A_Human_*` (walk, idle, talk, sit down, sit idle, sit and talk, stand up, nod, head shake, folded
  arms, phone, eat, read, lie down, sleep, get up): `blender/humans/clips.json` says which clip comes from where,
  `blender/humans/retarget_clips.py` bakes them on Guy_a's body (`Saved/LifeAnims/life_anims.glb`, a contact sheet with
  `--preview`), `unreal/scripts/import_life_anims.py` imports them, `life_retarget.py` sets the retarget sources.
  Sources: Quaternius' Universal Animation Library 1 and 2 (CC0, `blender/humans/fetch_clips.sh`) and our own older
  clips; licences in `blender/humans/SOURCES.md`. Only CC0 / CC BY clips.
- **How they're carried over**: in world space. Every bone takes the source's rotation relative to the source's
  reference pose (UAL: its `A_TPose`; ours: the rig's rest) and applies it to the target's rest turned to point where
  the source's reference bone points (joint to joint, so FBX and glTF bone frames don't matter). Bone lengths stay the
  target's; the pelvis moves `scale` (standing), `seat` (onto the house chair: pelvis (0, 1, 56.7) cm, seat 47) or `abs`
  (the bed layout). The wrists: a hand twisted past 120 degrees against the forearm is turned back (seat_anims' mouse
  hand was baked upside down) and half of the remaining twist goes to the forearm (no twist bones). `calm` tones a
  clip down toward another (Talk at 55% of UAL's gesturing).
- **Sleeping** (`LieDown`, `Sleep` loop, `GetUp`) is on the side: `blender/humans/sleep_clips.py` poses it on Guy_a
  (left side, facing out of the bed, knees bent, the lower arm under the pillow, the head ~12 cm over the mattress,
  5 s breaths; LieDown sits on the edge, props on the elbow and rolls in; GetUp is it backward) into
  `Saved/ClipSources/sleep_side.glb`, which clips.json carries over like the rest. The bed frame is life_anims.py's
  (UE: the actor beside the bed facing +Y, mattress top 50 cm, its middle at local (-5, -55), the head toward +X).
- **At a computer** (`PC_Type`, `PC_Game`, `PC_Scroll`, `PC_Read`, `PC_LeanBack`, `PC_Stretch`, `PC_Gamepad`):
  `blender/humans/sit_pc.py` poses them on Guy_a with `poser.py` (two-bone IK arms with the elbows down by the body,
  hands turned by their palms, finger taps and clicks, mouse flicks) on seat_anims' chair and desk and the shared seated
  pelvis, so SitDown -> the loops -> StandUp follow on. `ULyudBodyAnimInstance::SetComputer(style)` works through the
  person's own mix (Gamer, Browse, Reader, Office, Idle, Gamepad: `StyleOfClip` of their old seated clip's name),
  6-18 s each with cross-fades; `ANpcBody` (Computer, Work), the club's placed people and `ABoothOwner` use it. While
  `PC_Gamepad` plays, `SM_Gamepad` (`blender/humans/gamepad.py`, `unreal/scripts/import_gamepad.py`) sits between the
  hands. The old per-person `A_<Name>_Sit_*` clips stay only as the style's name. Club desks stand at their own heights
  and distances: while a hands-on-desk loop plays (Type, Game, Scroll), a trace under each hand (and up to 30 cm ahead
  of it, when the chair stands back) finds the desk and two-bone IK puts the hand on it.
- **Run**: UAL `Jog_Fwd_Loop`, ~2.6 m/s at rate 1; `ANpcBody` runs from 1.9 m/s (back to walking under 1.6).
- **On top of every clip** (`FLyudBodyAnimProxy::Procedural`): breathing (chest, shoulders; ~4 s, deeper and quicker
  after walking or running), the lean into starts, stops and turns, and on their feet (`SetOnFeet`) the feet on the
  ground: a trace under each foot, two-bone IK (`AnimationCore::SolveTwoBoneIK`), the pelvis down to the lower foot,
  the planted foot tilted to the slope. `lyud.Face.Log 1` logs it once a second; `make_body_test_level.py` +
  `test_bodies.py` shoot desks, a ramp and a step.
- **Walk speed**: the walk covers 0.92 m/s at rate 1 on Guy_a (`life_anims_report.json` `speed_m_s`); `ANpcBody` plays
  it at the person's speed / (93 cm/s x body scale). Body scale = pelvis height / 93.7 cm (Guy_a).
- **A person's own clips** (`A_<Name>_Sit_*`, the club's computers): `bake_own_clips.py` bakes seat_anims' loops on the
  person's old MakeHuman body (`<humans worktree>/blender/humans/<id>.blend`), `retarget_clips.py --own <id>` carries
  them onto the generated body, `LIFE_OWNER=<Name> LIFE_GLB=Saved/LifeAnims/own_<id>.glb import_life_anims.py`.
- Check without the engine: `retarget_clips.py --preview` renders each clip on the target body
  (`Saved/LifeAnims/frames`).

## Face

`blender/characters/face_rig.py` (called by `rig_character.py`; `character.json` `face`: `{"rig": false}` skips it,
`{"eyes": false}` leaves the eyes alone (glasses: Clerk, Crowd_13), `{"marks": {...}}` overrides the landmarks,
`{"repaint_eyes": true, "iris": [r, g, b]}` paints clean eyes into the texture where the generated ones came out torn
(Kid_boy: an almond opening, white, iris, pupil, catchlight, lash line, the ragged lids eased to the skin; drawn per
triangle, so neighbouring UV islands are untouched)):

1. **Landmarks**: the face rendered front-on and unlit; `face_marks.py` (numpy/scipy, system python) finds the irises
   (dark iris-sized blobs, a difference of Gaussians), the openings (how far the colour stays off the cheeks'), the lip
   line (the darkest row under the nose) and the iris colour -> `blender/characters/<id>/face_marks.json`, debug
   picture `renders/characters/<id>_face.png`. Glasses fool it: give `marks` by hand.
2. The face's front is subdivided once and the eyes and mouth once more (+3-8k triangles); the eye openings and lids
   are smoothed (hi3d sculpts ragged ridges there that a key light shows as torn lids and hard rings), on the low mesh
   and on the bake's 400k copy; the **normal map is baked last**, on this final face (baked before, the subdivided
   face's tangents no longer matched it: facets and glossy streaks). `face.normal_strength` in character.json lowers
   M_HumanFace's NormalStrength (default 0.75) where the baked eye bags still read too hard (Owner 0.4, Kid_boy 0.45).
3. **Lips**: the face is cut along the lip line across the mouth and split; a dark sheet behind (UV on the texture's
   darkest 9x9 block, skinned to the head, always behind the skin) is the inside of the mouth.
4. **Morph targets**: `eyeBlinkLeft/Right` (the painted eye squeezed into the lower lid line, the upper lid's skin
   following), `jawOpen` (14 deg about a hinge under the ears; the lower lip drops most in the middle), `mouthFunnel`,
   `mouthSmile`, `browUp`.
5. **Real eyes** (2026-10-27; the model is not touched): the hi3d paint of the eyes is often torn or smeared, and sliding
   it about to make the eyes look (the old way) smeared it more. Now the paint inside each opening is cut away at run
   time and an eyeball looks out from behind it:
   - `blender/characters/eye_shots.py` (Blender, on `export/<id>.blend`) renders a close-up and a depth map of each
     painted eye; the opening is fitted **by hand** on them (an almond: middle, half width, up, low, tilt) and kept in
     `character.json` `face.eye_open` (an automatic fit came out ragged on the torn paint);
   - `blender/characters/eyes.py` (system python) adds the shell's depth, places the ball (an ellipsoid ~12 mm deep and
     tall, about as wide as the opening, turned with the face so its outer end follows the temple back, as close
     behind the opening as the shell around it allows: the faces fall away fast at the outer corner and a straight ball
     pokes out there) and a darker socket behind it, the iris colour from `face_marks.json` -> `<id>/eyes.json`, with a
     check picture;
   - `unreal/scripts/set_eyes.py` writes them into `MI_<Id>_body`; `M_HumanFace` cuts the openings with an opacity mask
     in the reference pose's mesh space (`PreSkinnedPosition`, which leaves the morphs out, so the blink morph that
     squeezes that skin into the lower lid closes the cut); `ULyudBodyAnimInstance` sets the balls (engine spheres
     with `M_HumanEyeball`, `unreal/scripts/make_eye_material.py`) on the head bone and moves the **iris** over the ball
     (the ball doesn't turn: an ellipsoid turning would push its ends through the lids). Test cvars `lyud.Face.EyeFix`,
     `lyud.Face.EyeYaw`, `lyud.Face.EyePitch`, `lyud.Face.Shut`.
   - Glasses (Clerk, Crowd_13): no `eye_open`, their painted eyes stay (the eyes are painted on the lenses' shell).
   - Skin is **subsurface** (M_HumanFace, Scatter 1): Virtual Shadow Maps give subsurface a wide penumbra; default lit,
     the 24k-triangle shells shadowed themselves in hard jagged triangles. The eyeball gets a fill set on screen
     (M_HumanEyeball Fill, exposure undone) or it goes pitch black in the socket's shade.
   Real eyeballs were tried once before (2026-10-25) set into the painted eye on a ball the size of the opening and
   looked like ping-pong balls; the shading (a grey, lid-shaded white, fibred iris, glossy cornea) and the cut
   following the painted lids are what make them read as eyes now.

At run time (`ULyudBodyAnimInstance`): blinks every 2-6 s (sometimes double, more while talking), eyes shut in sleep;
looks at the player now and then (2-6 s on, 3-8 s off, within 6 m and 70 deg), otherwise about; the head turns part of
the way on top of the clip (neck 35%, head 65%), the eyes the rest with saccades; **lip-sync**: `tools/lipsync.py` turns
every `audio/voice/<lang>/*.wav` into 30 fps curves (`Content/Dialogue/lipsync_<lang>.json`: loudness -> jaw, a low
spectral centroid -> funnel, a high one -> smile), `ANpcTalkDirector` calls `ULyudBodyAnimInstance::SpeakLine` with
the line; NPCs chatting without a voice (`EPersonPose::Talk`) move their mouths in syllable bursts with pauses. Rerun
`tools/lipsync.py` after new voice lines.

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
