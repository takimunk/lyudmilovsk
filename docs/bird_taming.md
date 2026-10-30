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
   onto the palm and pecks; while it comes the eyes slowly turn to the palm (unless the mouse moves). At the first peck one handful is used up and, after a moment of pecking, the view flies into
   the bird. The hero waits where he crouched; coming back (hold `R` as the bird) he stands up.

The starlings' murmuration comes to the same palm (F no longer belongs to `AStarlingMurmuration`).

Every key is polled and never consumed (E, F, Tab are shared). `lyud.Tame 0` switches all of it off (A/B benchmarks).
Words: `item.bird_feed.*`, `ui.held.*`, `ui.tame.*`; the bird's name is `bird.<species>.name`.

## Feeds and the feeder

The Academgorodok's grocery, «Гастроном № 7», is a pavilion of its own in the yard behind Novikova 1 (west of it, by the
drive from the south gate; `blender/academgorodok/grocery.py` round `gastronom.py`'s 1950s hall). Tanya Orlova sells
over its counter while she is at work (Mon–Sat 9–19; `T` when she is in front of you, `Content/PeopleData/counters.json`):

| item | what | packs of | roubles |
|---|---|---|---|
| `feed_suet` | suet cake with seeds: tits, nuthatch, woodpeckers, robin | 5 | 25 |
| `feed_niger` | niger seed: goldfinch, siskin, redpoll, greenfinch | 5 | 30 |
| `feed_rowan` | dried rowan berries: waxwing, thrushes, bullfinch | 5 | 25 |
| `feed_mealworm` | dried mealworms: robin, wagtails, wren, starling | 5 | 35 |
| `feed_grain` | grain mix: sparrows, finches, pigeons, buntings | 5 | 15 |
| `feed_peanuts` | peanuts: tits, nuthatch, jay, woodpeckers | 5 | 30 |
| `bird_feed` | the vending machine's seeds and millet | 5 | 10 |
| `feeder` | a wooden bird table on a post | 1 | 80 |
| `bread_loaf`, `kefir` | for the hero | 1 | 12, 14 |

What each species thinks of each feed is `Content/BirdsData/feeds.json` (`like` 0..1 per species, `default` for the
rest; `LyudBirdFeeds::Like`). The labels on the packs (`textures/localized/ru/label_<id>.jpg`, shown on the packet's
front by `ALyudGoodsItem`) and the icons are cut out of two Higgsfield grids by `tools/grocery_assets.py`
(`design/grocery/*_grid.json` pin the requests).

**On the palm.** Any feed can be held and offered (F); a bird that doesn't like it only looks (`ui.tame.dislike`), one
that likes it a little trusts the palm more slowly (`FPlayer::OfferLike`).

**The feeder.** Tab, the feeder, "Поставить": it stands on level open ground 1.2 m in front of the hero (not on the beach
in the sea, not within 1.5 m of another). Look at it with the feed in the hand and `E` puts in a handful (up to 5;
another feed empties the old one); `E` with an empty hand takes it back with what is left. About an hour and a half after
it is filled (`lyud.Feeder.FindHours`) the birds of the habitats round it (30 m) that like the feed start to find it and
come to the tray in small groups (bird sim: feeder sites, `FeederScore`): predators, swifts and swallows and anything
bigger than half a metre never come. They peck, a handful lasting a few birds' meals, and while the hero stays calm
within 30 m they grow used to him (`Trust` towards 0.6), so the palm next to the feeder works in seconds. Feeders and
what is in them are saved ("Feeders").

Birds feed at a feeder in the morning and through the day; one filled late in the afternoon stays empty till the next
morning (they are going to roost). The finding time is mostly the groups' own pace: with the feed "found" at once
(`lyud.Feeder.Age 3`) the first birds still take about as long.

Dev console: `lyud.Feeder.Place`, `lyud.Feeder.Fill <feed> [n] [i]`, `lyud.Feeder.Age <hours> [i]`, `lyud.Feeder.Report`
(`FEEDER` log lines: birds on each tray, food left), `lyud.Feeder.Cam [i] [cm away] [cm up] [deg round] | off` (a camera
at the tray), `lyud.Goods.Show <item> [cm] | off` (an item's goods, a packet's label, in front of the view),
`Inventory.Give <item> [n]`, `lyud.Birds.Time <hour>`. The sim's check: `BIRD_ONLY=feeders tools/birdsim/run.sh test`
(suet by the club door brings tits after ~2 h, grain brings chaffinches, a feed nobody likes stays untouched).

The game's check, offscreen (after `make_tame_test_level.py`):

```sh
rm -rf unreal/Lyudmilovsk/Content/Dev/L_FeederTest*
UnrealEditor-Cmd unreal/Lyudmilovsk/Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/make_feeder_test_level.py
sh unreal/scripts/test_feeder.sh          # FEEDERTEST/FEEDER lines, Saved/FeederTest/<n>_<name>.png; FEED=, MINUTES= (20)
```

It puts a feeder with suet by the garden 18 m from the club door at 9:00, the hero 9 m off, and watches the tray at
slomo 8 (the bird life steps at most 0.25 s a frame, so ~6x): on 2026-10-30 blackbirds came at 1.6 h, then a great tit,
blue tits (7 birds at once at 2.0 h), a magpie and hooded crows; 5 handfuls were 2.7 by 2.7 h. Then the six packets'
fronts, the inventory with the grocery's things, and the pavilion (`L_FeederTest` has its meshes on the lawn) outside
and in.

## Pieces

- `ULyudInventory::Equipped` (saved): the item in the hand; `ELyudItemKind::Feed` items offer "Взять в руку".
- `UHeroReachComponent::SetHold` / `ClearHold`: a pose the right hand keeps while walking and looking (nothing is locked),
  placed from the eyes in the look's frame; reaches (Take, Drink) run over it and it comes back after.
- `HandReach::ApplyCrouch`: the crouch on any locomotion pose, hips down and back, back leaning, legs bent by two-bone IK
  so each foot stays where the clip puts it; the capsule is the engine's crouch (`AHeroCharacter::CrouchHalfHeight`).
- `ABirdFeedBag`, `ASeedHandful`: the bag and the heap on the palm (engine shapes, no assets; every feed looks like the seeds for now).
- `UBirdFeederSubsystem`, `ABirdFeeder` (`Birds/BirdFeeder.*`): the feeders, their trays in bird life (`UBirdLifeSubsystem::AddFeeder`).

## Checking it without a window

```sh
rm -rf unreal/Lyudmilovsk/Content/Dev/L_TameTest* unreal/Lyudmilovsk/Content/Dev/TameMats
UnrealEditor-Cmd unreal/Lyudmilovsk/Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/make_tame_test_level.py
UnrealEditor unreal/Lyudmilovsk/Lyudmilovsk.uproject /Game/Dev/L_TameTest -game -RenderOffscreen -ResX=640 -ResY=360 -windowed \
    -dpcvars=lyud.MainMenuOnStart=0 -abslog=/tmp/tame.log "-ExecCmds=py unreal/scripts/test_tame.py"     # TAMETEST lines
UnrealEditor ... -ResX=1280 -ResY=720 -dpcvars=lyud.MainMenuOnStart=0,lyud.Birds.AnyMap=1 ... \
    "-ExecCmds=py unreal/scripts/record_tame.py <vend|crouch|success|scared> [warmup s] [species, default Robin]"
python3 tools/tame_frames.py <out dir> <name>        # contact sheet + animated webp of the screenshots
```

`success` puts a robin on the lawn (`lyud.Birds.SpawnNear`), creeps up crouched with the palm out to ~2.3 m and waits: the
robin's trust grows to 0.55 in about half a minute, it flies to the palm, eats and the view goes into it. `scared` holds
the palm out upright, jerks the view and runs at the robin: it flies off at ~4-5 m. From 12 s (success) or 4 s (scared)
every other screenshot is taken from a camera beside the hero. The log has a `Tame: bird <id> <event> at <cm>, alarm,
trust` line for each event of the birds near the hero.

Dev console: `Tame.Give [n]`, `Tame.Offer 1|0`, `Tame.Walk x y z speed crouch turnrate stopat | off`, `Tame.SelfTest`,
`Vending.Buy [slot]`, `Inventory.Open [0]`, `Inventory.Equip <id>|none`, `Hero.DumpBones`, `lyud.HeroDebugBones 1`.
