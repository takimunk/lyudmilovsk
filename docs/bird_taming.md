# Befriending a bird

How the hero makes friends with a bird before becoming it (`UBirdTameSubsystem`, `Source/Lyudmilovsk/*/BirdTame.*`).
The birds' side (who notices, turns, watches, calls, comes, eats or flees) is bird life's (`UBirdLifeSubsystem`);
becoming the bird is the flight's (`UBirdFlightSubsystem::BecomeBird`, docs/bird_flight.md).

## Playing it

1. **Feed.** The vending machine sells a paper bag of seeds and millet in slot 5 (`5`, then `E`, 15 roubles): five
   handfuls, one per bird. The bags stand on the machine's bottom shelf; the hand takes the bag off the tray into the pocket.
2. **In the hand.** Tab, the bag, "Взять в руку". The hero carries it at the hip; the plate at the bottom left shows what
   is in the hand, how much is left and the keys.
3. **Sneak.** `C` crouches (toggle; `Ctrl` while held), `Shift` creeps slower still (110 and 55 cm/s). Running, quick
   steps, jerking the view or the hand scare birds: bird life reads the hero's speed, crouch, turn rate and the hand's speed.
4. **The palm.** `F` holds the open palm out with a handful on it (and takes it back). A bird that notices turns to the hero,
   watches the palm, moves its head, may call; the plate shows its name, its alarm and its trust. Keep still and it flies
   onto the palm and pecks. At the first peck one handful is used up and, after a moment of pecking, the view flies into
   the bird. The hero waits where he crouched; coming back (hold `R` as the bird) he stands up.

The starlings' murmuration comes to the same palm (F no longer belongs to `AStarlingMurmuration`).

Every key is polled and never consumed (E, F, Tab are shared). `lyud.Tame 0` switches all of it off (A/B benchmarks).
Words: `item.bird_feed.*`, `ui.held.*`, `ui.tame.*`; the bird's name is `bird.<species>.name`.

## Pieces

- `ULyudInventory::Equipped` (saved): the item in the hand; `ELyudItemKind::Feed` items offer "Взять в руку".
- `UHeroReachComponent::SetHold` / `ClearHold`: a pose the right hand keeps while walking and looking (nothing is locked),
  placed from the eyes in the look's frame; reaches (Take, Drink) run over it and it comes back after.
- `HandReach::ApplyCrouch`: the crouch on any locomotion pose, hips down and back, back leaning, legs bent by two-bone IK
  so each foot stays where the clip puts it; the capsule is the engine's crouch (`AHeroCharacter::CrouchHalfHeight`).
- `ABirdFeedBag`, `ASeedHandful`: the bag and the heap on the palm (engine shapes, no assets).

## Checking it without a window

```sh
rm -rf unreal/Lyudmilovsk/Content/Dev/L_TameTest* unreal/Lyudmilovsk/Content/Dev/TameMats
UnrealEditor-Cmd unreal/Lyudmilovsk/Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/make_tame_test_level.py
UnrealEditor unreal/Lyudmilovsk/Lyudmilovsk.uproject /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=640 -ResY=360 -windowed \
    -dpcvars=lyud.MainMenuOnStart=0 -abslog=/tmp/tame.log "-ExecCmds=py unreal/scripts/test_tame.py"     # TAMETEST lines
UnrealEditor ... -ResX=1280 -ResY=720 ... "-ExecCmds=py unreal/scripts/record_tame.py <vend|crouch|success|scared> [warmup s]"
python3 tools/tame_frames.py <out dir> <name>        # contact sheet + animated webp of the screenshots
```

Dev console: `Tame.Give [n]`, `Tame.Offer 1|0`, `Tame.Walk x y z speed crouch turnrate stopat | off`, `Tame.SelfTest`,
`Vending.Buy [slot]`, `Inventory.Open [0]`, `Inventory.Equip <id>|none`, `Hero.DumpBones`, `lyud.HeroDebugBones 1`.
