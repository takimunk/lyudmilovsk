---
name: game-assets
description: Which service makes which generated asset for Lyudmilovsk (pictures and textures, voice and sound, 3D models), the fallback when credits run out, and how every paid generation is pinned and stored. Use before ANY paid generation: an image, a texture, an icon, a poster, a voice line, a sound, a 3D model or a character.
---

# Generated assets: providers, pinning, storage

## Who makes what (Dima's rule, 2026-10-25)

| Asset | Service | Fallback |
|---|---|---|
| Pictures, textures, icons, posters, concept and reference images | **Higgsfield MCP** (`generate_image`, `generate_image_batch`, `upscale_image`, `remove_background`; Dima's subscription with credits; check `balance` first) | **fal** (`FAL_API_KEY`, e.g. `google/nano-banana-2.1/edit`, `fal-ai/nano-banana-2`) when Higgsfield's credits run out or it lacks the model |
| Text to speech (NPC lines, the hero) | **fal** always (`tools/gen_voice.py`: ElevenLabs v3 or a MiniMax designed voice, see `localization/voice.json`) | none |
| Sound effects, music | fal (`tools/gen_sounds.py`) | none |
| Any 3D model (props, characters, animals) | **hi3d** (the `hi3d` MCP at agent.hi3d.ai: `quote_generation` then `create_generation`; also does 2D on Nano-Banana 2 / Pro) | Hi3D v3.0 on fal (`hitem3d/hi3d/v3.0/image-to-3d`, `.../multi-view-to-3d`), scriptable |

The fal key is in `~/projects/asmrx3000/.env` (`FAL_API_KEY`). Never commit it, never print it.
Dima approves the spend of a task when he asks for it; ask before a run much bigger than the task (dozens of models, a full reshoot).

## MCP gotcha: getting a local file in
MCP uploads (`hi3d upload_input`, Higgsfield `media_upload`) take the file through the tool call, so a big local picture is
impractical to pass inline. Prefer chains that stay inside one service: make the image in hi3d (`create_image_generation`)
and model it with `source_task_id`/`source_images`; in Higgsfield use `media_upload` (it returns a PUT URL: upload with curl,
then `media_confirm`) or `media_import_url` for a file that is already online. A fal output URL is online and can be passed on.

## Pin every generation
- The request lives in a committed spec (prompt, model/endpoint, seed, every argument), never only in a chat:
  `localization/assets.json` (pictures with words), `localization/voice.json` (voices), `audio/sounds.json`,
  `blender/characters/<id>/gen.json` (characters, run by `tools/gen_character.py`).
- After the run a **sidecar** next to the output records what was actually sent and got back: endpoint, request/task id,
  inputs (URLs or the spec name, not data URIs), the output URL, sha256, seconds, and cost if known. Sidecars are committed.
- MCP runs have no script: write the sidecar yourself (`<asset>.json`: service, tool, all arguments, task id, outputs).
- Most endpoints have no seed (TTS, 3D): the committed output is canonical; regenerate only when its spec changed.

## Every generated model is optimized before it goes in (Dima's rule, 2026-10-25)
hi3d gives ~2M triangles and 8K textures: never import that. In Blender, before the game:
1. **Clean**: weld duplicates (0.5 mm), drop loose specks, fix obvious defects (bumpy relief, holes, fused or extra
   parts, wrong-side details). Parts that must move on their own are cut off and made procedurally or taken from a
   template rig (bird legs; people's hands when the generated ones are fused or broken).
2. **Decimate to the budget** (triangles, LOD0), smooth shading, and **bake the lost detail** from a ~400k copy into a
   tangent normal map (Cycles, selected-to-active).
3. **Textures** scaled to the budget; metal/roughness maps dropped unless the material uses them.
4. In UE: LOD1/LOD2 generated (50 %, 25 %).

| What | LOD0 triangles | Textures |
|---|---|---|
| Person, NPC (adult) | 24k | base colour 2048 + normal 2048, one material |
| Child | 18k | 2048 + 2048 |
| Animal or bird seen close (the player's bird, pets) | 20k | 2048 + 2048 |
| Animal or bird at a distance, crowds | 3–5k | 1024 |
| Large prop (furniture, machine) | 8k | 2048 |
| Small prop (cup, bottle, item) | 1–2k | 512–1024 |

Characters: `blender/characters/rig_character.py` does steps 1–3 (`faces`, `texture`, `bake_normal` in
`character.json`); `unreal/scripts/import_character.py` does 4.

## Storage (git LFS budget)
- Raw outputs (multi-view images, 2M-face models, previews) stay local, ignored, refetchable from the sidecar's URL.
- Push to LFS only what ships: the finished, decimated asset and its uassets (and a small jpg preview when useful).
- Generated pictures for a language nobody asked for are not committed (see the `localization` skill).

## Related skills
`localization` (any words, voice), `game-items` (item icons), `characters` (people and NPCs from a reference to the game).
