# Hero (first person)

The player's own body: the realistic Zoomer (MakeHuman body from `blender/humans`, humans thread)
dressed in the clothes of the first-person body of Dima's Murmuration game (`fp_body.glb`): open
oversized camel corduroy overshirt with rolled sleeves, sage hoodie, baggy black cargo trousers,
chunky off-white sneakers. Murmuration's own idle and walk are baked onto it.

```sh
/Applications/Blender.app/Contents/MacOS/Blender -b -P blender/hero/build_hero.py -- [--preview]
unreal/scripts/import_hero.sh            # editor closed on this checkout
```

- Inputs: `$HUMANS_DIR/zoomer.blend` (default: the humans worktree) and `$MURMURATION_FP`
  (default `~/projects/_murmuration_src/murmuration/public/models/fp_body.glb`).
- Output `blender/hero/export/human_hero.glb` and the Unreal assets under `/Game/Characters/Hero`
  are local only (LFS quota), like the other humans; `--preview` writes `renders/hero/*.png`.
- Clothes: per-bone transforms from Murmuration's rig to the Zoomer A-pose, pushed outside the body
  (a smooth push field, so pockets and collars move with their panel), reskinned from the body; body
  skin under the clothes is cut away. The soles reach 4.4 cm below the body's floor
  (`AHeroCharacter::SoleDrop` lifts the mesh).
- Clips: `Idle` (4 s) and `Walk` (1.1 s, one gait cycle, in place, the planted foot moves back 90 cm
  per cycle: `UHeroAnimInstance::WalkStride`).
- In game: `AHeroCharacter` (default pawn of `BP_FirstPersonGameMode`; `unreal/scripts/set_hero_pawn.py
  template` switches back to the template character).
