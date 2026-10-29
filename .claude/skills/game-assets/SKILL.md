---
name: game-assets
description: Which service makes which generated asset for Lyudmilovsk (pictures and textures, voice and sound, 3D models), the fallback when credits run out, and how every paid generation is pinned and stored. Use before ANY paid generation: an image, a texture, an icon, a poster, a voice line, a sound, a 3D model or a character.
---

# Generated assets: providers, pinning, storage

## Who makes what (Dima's rule, 2026-10-25)

| Asset | Service | Fallback |
|---|---|---|
| Pictures, textures, icons, posters, concept and reference images (always as grids, see below; for 3D inputs `gpt_image_2_5` sunburst medium 4K transparent) | **Higgsfield MCP** (`generate_image`, `generate_image_batch`, `upscale_image`, `remove_background`; Dima's subscription with credits; check `balance` first) | **fal** (`FAL_API_KEY`, e.g. `google/nano-banana-2.1/edit`, `fal-ai/nano-banana-2`) when Higgsfield's credits run out or it lacks the model |
| Text to speech (NPC lines, the hero) | **fal** always (`tools/gen_voice.py`: ElevenLabs v3 or a MiniMax designed voice, see `localization/voice.json`) | none |
| Sound effects, music | fal (`tools/gen_sounds.py`) | none |
| Any 3D model (props, characters, animals) | **hi3d** (the `hi3d` MCP at agent.hi3d.ai: `quote_generation` then `create_generation`; also does 2D on Nano-Banana 2 / Pro) | Hi3D v3.0 on fal (`hitem3d/hi3d/v3.0/image-to-3d`, `.../multi-view-to-3d`), scriptable |

The fal key is in `~/projects/asmrx3000/.env` (`FAL_API_KEY`). Never commit it, never print it.
Dima approves the spend of a task when he asks for it; ask before a run much bigger than the task (dozens of models, a full reshoot).

## Always a grid, never one picture per object (Dima, 2026-10-25)
Credits go fast. Put several objects or views in **one** picture as a grid and cut it into frames
(`tools/characters/cut_grid.py`; equal cells, no borders/lines/text, one subject per cell, same scale, nothing crossing
a cell edge). Examples: 4 people x (back, left, right) in a 4x3 grid at 3:4; 12 props in a 4x3 grid; 9 icons 3x3.
Say in the prompt which reference image belongs to which row. Check the cells before using them (side views often
come out three-quarter: then that person's model uses front + back only). One picture per object only when a grid
truly can't work (a single huge object), and say why in the sidecar.

**Pictures that will go to 3D generation** (fronts, turnaround grids): Higgsfield `gpt_image_2_5` with
`variant: sunburst`, `quality: medium`, `resolution: 4k`, `background: transparent`. Other pictures (textures,
posters, icons) may use other models, still as grids. Preflight the cost with `get_cost: true` and keep a run's spend
in the sidecars (`credits`).

## 3D through hi3d (the MCP), step by step
`upload_input` takes the picture as base64 text inside the tool call, so a real view (megabytes) cannot be uploaded from a
file or URL. Keep the whole chain inside hi3d instead:
1. `quote_image_generation` then `create_image_generation`: `operation: text_to_image`, `multi_view: true`,
   `model_name: "Nano-Banana 2"` (or Pro), the description and the house pose/background wording; it returns separate
   view images in one task (front, back, left, right).
2. Look at them (`get_task_outputs`); if a view is wrong, `operation: edit` with `source_task_id` + `source_image_index`.
3. `quote_generation` with `source_images: [{task_id, image_index}...]` in view order (front, back, left, right),
   `version: v3.0`, `model_type: high_quality`, `generation_type: model_with_texture`, `pbr: true`; show the quote,
   then `create_generation` with that `quote_id` and `confirmed: true`.
4. `get_task` until done, `get_task_outputs` for the GLB URL, download it to `blender/characters/<id>/gen/model.glb`
   and write the sidecar (task ids, arguments, quote, URL, sha256).
A picture made elsewhere (Higgsfield sheets) can only reach hi3d through `upload_input`, which is impractical; then hi3d
re-draws from the description, so it looks close to the sheet, not identical. Hi3D v3.0 on fal
(`hitem3d/hi3d/v3.0/multi-view-to-3d`, `tools/gen_character.py`) takes URLs, but is billed by fal, not Dima's hi3d
subscription: use it only when Dima says so.

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

## Things you enter or use: rebuilt by us, hi3d only as reference (Dima's rule, 2026-10-26)
A hi3d model is a closed skin: no openings, no rooms inside, no wall thickness, junk left inside, big crooked
triangles once decimated. Cutting windows or doors out of those triangles (e.g. by the texel colour under each one)
gives ragged edges, shards and slivers ("polygon mince" inside the first cable-car cabin). So:

- **Enterable or interactive** (cabins, vestibules, buildings, rooms, doors, furniture with shelves or drawers): build
  the game mesh ourselves in Blender: clean low-poly geometry that follows the hi3d form (shrink-wrap a clean primitive
  onto its outer skin along rays from outside in, or model it), closed walls of real thickness, openings cut with exact
  booleans from clean outlines (windows, doors to `CharacterStandard`), door leaves as their own parts, flat glass
  panes, simple collision boxes, our own UVs. The hi3d model is the **shape reference and the bake source**: its
  colour and relief go onto our mesh by a Cycles selected-to-active bake (base colour + tangent normal).
- **Organic and small props** (people, animals, birds, statues, rocks, small items): hi3d + the cleanup and
  decimation above. People's faces are then fixed in place: painted from their front picture, smoothed, real eyes
  (`characters` skill, `tools/characters/fix_hi3d.sh`); rebuilding them on MakeHuman or MetaHuman was tried and dropped.

Worked example: `blender/transport/cabin.py` (docs/transport.md "The cabin").

## Storage (git LFS budget)
- Raw outputs (multi-view images, 2M-face models, previews) stay local, ignored, refetchable from the sidecar's URL.
- Push to LFS only what ships: the finished, decimated asset and its uassets (and a small jpg preview when useful).
- Generated pictures for a language nobody asked for are not committed (see the `localization` skill).

## Related skills
`localization` (any words, voice), `game-items` (item icons), `characters` (people and NPCs from a reference to the game).
