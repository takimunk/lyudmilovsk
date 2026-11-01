# Ladders

Any ladder in the game is climbed the same way: hand over hand, the hands closing round the rungs (each finger stops
where its skin meets the rung), the feet following on the rungs below. Code: `Public/World/LyudLadder.h`,
`Private/World/LyudLadder.cpp`.

## Pieces

- **`ULyudLadderComponent`**: where a ladder's rungs are. Put it on any actor whose mesh has a ladder. Its frame: the
  origin on the floor under the middle of the rung line, +X out toward whoever climbs (they face -X), +Z up, the rungs
  along Y. `RungSpacing`, `FirstRungZ`, `RungCount`, `HalfWidth`, `RungRadius`, `BodyStandoff` (the hips out from the
  rungs), `TopExit` (where an interactive climb steps off at the top), `ClimbRungsPerSecond`, `GripSound`/`GripVolume`.
  `bInteractive` ladders are the player's (below); scripted climbs leave it off.
- **`ALyudLadder`**: a free-standing ladder that draws itself (two rails, the rungs; the engine's basic shapes as
  instances, built once in `OnConstruction`).
- **`LyudLadder::Limbs`**: the gait, a pure function of the feet's height on the ladder. Cycles of two rungs: right hand,
  left foot, left hand, right foot, each limb lifting two rungs while the body rises by them; hands open on the way and
  arc out round the rung. Going down is the same played backwards, so the feet lead. Below the lowest rung a foot stands
  on the floor.
- **`ULyudLadderClimber`**: on the character (made on first use). Something sets the feet's height (`SetFeetZ`: a script,
  or the player); it places the character, holds its body facing the ladder (`AHeroCharacter::HoldBodyYaw`) and every
  frame fills the hero's `UHeroAnimInstance::Ladder`, the same whole-body targets the bicycle uses (`FBikeRiderPose`:
  hips, lean, both feet on balls of the feet, both hands round bars with `FHand::Close` for an open hand in the air).
  The animation applies them (`LyudBikeRider::ApplyBody` before the look, `ApplyArms` after it). `Attach` blends the
  body onto the ladder, `Release` back to its own animation; a jump between two alike ladders passes 0 seconds and keeps
  every hand and foot on its rung. A hand or a boot meeting a rung plays the ladder's grip sound.
- **`ULyudLadderSubsystem`**: the prompt and E for interactive ladders (`ui.ladder.climb`): close by and looking at one,
  E takes hold; W and S climb (`AHeroCharacter::OnMove` hands the input to the climber), Space lets go; at the top the
  climb steps off onto `TopExit`, at the bottom onto the floor.

## Where it is used

The hatch to Europa (`docs/europa.md`): `ALyudAirlock::SeaLadder`, `EuropaLadder` and `ALyudEuropaSite::ShaftLadder`,
set up from the kit's measures exactly as `blender/europa/europa_kit.py` `ladder()` makes them; the scripted climbs drive
the climber while the hands still find rungs above, and move the body on by itself over the top. The jump between the two
alike shafts shifts the feet by 80 cm (not the shafts' tops' 90) so the rungs line up.

## Tests

`lyud.Ladder.Spawn [rungs] [cm ahead]` puts a ladder in front of the player, `lyud.Ladder.Grab` takes hold of the
nearest, `lyud.Ladder.AutoClimb 1|-1` climbs as if W/S were held, `lyud.Ladder.Report` logs the climb. The offscreen
`EUROPATEST_ONLY=moon unreal/scripts/test_europa.sh` climbs one on Europa's ice and shoots it from the side and from
the eyes.
