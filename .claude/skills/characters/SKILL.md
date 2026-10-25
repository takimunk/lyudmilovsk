---
name: characters
description: How to make or remake a person (NPC, club regular, the owner, any human) for Lyudmilovsk to the house standard - reference, T-pose views, hi3d model, rig on SK_Human_Skeleton, import, life schedule, lines and voice. Use whenever you add a character, regenerate one, or change a human's model, skeleton, materials, lines or voice.
---

# Making a character

Full standard and the reasoning: `docs/characters.md` (Nao is the worked example). Providers and pinning: the
`game-assets` skill. Words and voice: the `localization` skill.

1. **Folder** `blender/characters/<id>/`: `source/` (reference), `gen.json` (steps: front T-pose, back, left, right
   views from `@front`, then the hi3d model; seeds pinned), `character.json` (height m, donor, faces, skirt).
   Copy Nao's and change the prompt, never the pose/background/light wording.
2. **Generate**: pictures through Higgsfield as grids (fronts of several people in one picture, then a 4x3 turnaround grid of 4 people x back/left/right; `gpt_image_2_5` sunburst, medium, 4K, transparent background; cut with `tools/characters/cut_grid.py`), the model through hi3d. With fal:
   `python3 tools/gen_character.py <id> front` → look at it → `back left right` → look → `model`.
   Sidecars `gen/*.json` are committed; pictures and `model.glb` are not.
   Check before modelling: T-pose, whole body, same scale in all views, details on the right side (earring, braid).
3. **Optimize + rig** (one script): budget 24k triangles (kids 18k), 2048 base colour + baked 2048 normal map
   (`character.json` `faces`, `texture`, `bake_normal`); check the close-ups of face and **hands**: fused or broken fingers
   get replaced by the donor's MakeHuman hands. Then: `Blender -b -P blender/characters/rig_character.py -- <id> --preview` and look at
   `renders/characters/<id>_posed*.png` (arms down, a step): no tearing at the skirt, hands and feet follow.
   The donor glb (`blender/humans/export/human_<donor>.glb`) comes from `blender/humans/build_humans.py`.
4. **Import** (editor closed on this checkout, nothing running `-game` while Dima plays):
   `UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/import_character.py <id>" -unattended -nullrhi`.
   Must log the shared skeleton. Commit `Content/Characters/Humans/<Id>/` with `git add -f` (Humans/ is excluded locally).
5. **Check in UE**: `LIFE_CAST=<Id>,Girl_b` for `make_life_preview_level.py` (commandlet) then `shoot_life_anims.py`
   offscreen (`-game -RenderOffscreen`, `LIFESHOT_SPOTS=<Id>_Walk,<Id>_Talk,...`); copy the other humans from the main
   checkout with `cp -c` first.
6. **Life**: row in `people.json` (+ a duty row in `activities.json` if new), `tools/peoplesim/run.sh test`,
   the table in `docs/people.md`.
7. **Lines**: keys in all three `strings_*.json` (same order), `[<Id>]` in `Content/Dialogue/npc_dialogue.txt`,
   sun lines welcome; `tools/check_localization.py`. Proposals until Dima approves.
8. **Voice**: speaker in `localization/voice.json`, `tools/gen_voice.py --lang ru|en|ja <keys>`, `import_voice.py`.
9. **Placement** in Lvl_Scene01 or another level goes through the thread that owns that level (via the coordinator).

Never: re-aim a bone by moving its head/tail (breaks the roll, every clip goes wrong); a second skeleton; literal text;
commit raw generations or the 2M-face model.
