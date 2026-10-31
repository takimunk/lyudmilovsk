# Swimming and diving

The hero swims at the surface of the Baltic, dives, and swims under it. The first times under water he has a little air;
after a few near-drownings he finds out he can breathe there, and from then on the water has no time limit.

## How it plays

| Where | Keys |
|---|---|
| At the surface | WASD swims breaststroke along the look's heading (the head stays out, the sea keeps the eyes over the waves). C or Ctrl dives; so does swimming on while looking down past ~38 degrees. Falling in fast (a jump off the bank) goes under by itself. |
| Under water | W swims where you look, up and down too. Space rises, C / Ctrl sinks, Shift swims slowly. The body is almost neutral and drifts up a little when left alone. Rising to within a hand of the surface comes up. |
| In the shallows | Shallower than 1.2 m (ALyudSea::SwimMinDepth) he stands and walks out. |

**Air.** 40 s of it under water (a quarter less when swimming hard), back in about 6 s at the surface. A thin bar at the bottom
of the screen shows it while it is not full: pale sea-green, amber when low, and when it is empty it blinks red, the hero
chokes (muffled, bubbling), the body fights upward and swims at half speed. Coming up after that is a near-drowning: a
desperate gasp, then coughing up the water.

**The realisation.** The third time the air runs out (ULyudSwimComponent::NearDrownsToRealise), he does not fight up: he hangs
there choking, slowed by the spasms. After four seconds he stops, coughs, and says, surprised and quiet, that he is breathing,
right there under the water, and that it seems to be fine (`line.hero.swim_breath.1`, voiced in ru/en/ja with the hero's MiniMax
voice from localization/voice.json, subtitled). The bar fills up as he breathes, then fades away and never comes back. If he
chokes where he cannot come up at all (under the ice), the realisation comes after 12 s whatever the count.

This lasts across the whole game: ULyudBreath (a GameInstance subsystem) holds NearDrowns and bBreathFree, saved in the slot
as the record "Breath" and reset on a new game, like the pocket.

## The sea it swims in

- **Tides.** Everything is measured from ALyudSea::SampleSurface (the wavy surface over ALyudSea::SeaLevel(), the datum plus the
  tide and the winter's low water). At low water the surface he swims at is metres lower and the water over the flat is gone
  (he walks there); the depth he reports is always from the surface now.
- **Winter ice.** Where ULyudWinterSubsystem::GetIceAt is over 0.5 there is no surface: the ice's underside (sea level minus 0.9 of
  its thickness) is a ceiling the head is held under, there is no air, and swimming up into it does not surface. He gets under
  the ice by diving from open water at its edge; on thick ice he walks (the winter's solid tiles).
- **The sea's own swimming.** ALyudSea still owns the water: its physics volume makes the body swim (MOVE_Swimming) and holds
  the swimmer in it. At the surface it lifts the eyes over the waves; while diving ULyudSwimComponent calls
  ALyudSea::SetSwimmerDiving(true) and it stops lifting.

## The body

The swim clips are the cast's shared clips on SK_Human_Skeleton (the hero's skeleton too): `A_Human_SwimIdle` (treading water)
and `A_Human_SwimFwd` (the stroke), from Quaternius' UAL1 (CC0) Swim_Idle_Loop / Swim_Fwd_Loop, retargeted by
blender/humans/retarget_clips.py (clips.json `SwimIdle`, `SwimFwd`) and imported by unreal/scripts/import_life_anims.py.
UHeroAnimInstance blends them in by ULyudSwimComponent::GetBodySwim (treading still, stroke moving, played by their own clock,
faster with speed) and FHeroAnimProxy::PlaceSwimmer hangs the body from the head: the head (so the eyes and the camera) stays
where a standing head would be and the body is pitched along the way it moves under water (flat at the surface).
`lyud.Hero.ViewBehind 260` pulls the camera back and shows the head, to look at the body.

## Sounds

audio/sounds.json group `swim` (ElevenLabs SFX v2 through fal, tools/gen_sounds.py, imported with
`IMPORT_ONLY=sfx/swim` unreal/scripts/import_audio.py): swim_stroke, swim_dive, swim_surface, swim_inhale (before a dive, until the
breath is free), swim_bubbles, swim_underwater (a loop while under), swim_choke, swim_gasp, swim_cough. Played through
ULyudSoundscapeSubsystem::PlayEffect / AttachLoop at the eyes. Under water the soundscape already muffles the world
(Enclosure 0.97 when the camera is under).

## For other systems

`ULyudSwimComponent::Get(World)` is the player's (also while a bird is flown and the hero waits). Ask GetState()
(Dry, Surface, Under), IsSwimming(), IsUnderwater() (the eyes under), GetDepthCm() (eyes below the surface or the ice),
GetBottomDepthCm() (eyes to the seabed), GetEyeLocation(), IsUnderIce(), GetAir(), IsOutOfAir(), IsBreathFree(), GetTimeUnder();
listen to OnStateChanged(Old, New) and OnBreathFree. Python: `unreal.LyudSwimComponent.get_player_swim(world)`.

## Console

`lyud.Swim 0` turns diving off (the old head-up swimming), `lyud.Swim.Breath <s>` the air, `lyud.Swim.Debug 1` prints the
state, `lyud.Swim.Dive`, `lyud.Swim.Air <0..1>`, `lyud.Swim.Free` (realised at once), `lyud.Swim.Reset` (the meter is back).

## Test

`unreal/scripts/test_swim.sh` (offscreen, waits for the UE queue, quiet launch flags): floats in open water 600 m out, dives
along the look, swims level, looks at the body from behind, runs out of air three times (two near-drownings counted, then the
realisation under water, the meter gone), 20 s under on 3 s of air, low tide (the surface it swims at moves down with the
water), the ice ceiling, and the shallows. Prints SWIMTEST lines and a PASS/FAIL list; screenshots in
Saved/Screenshots/MacEditor. `SWIMTEST_ONLY=float,dive,...` runs some steps.
