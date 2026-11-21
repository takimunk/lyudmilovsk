# The birds' signature behaviours

Interactives backlog item 44. Every one of the 39 species has one or two things it alone does, 63 in all, that a patient
hero sees close by or through the binoculars and that go on the species' page of the bird book ("Habits": the line and
the day first seen). Every habit of a family watched gives the family's stamp (collection `bird_habits`, six pieces).

| What | Where |
|---|---|
| The table, when a habit fits, its path, the hooks | `Private/Birds/BirdHabit.h`, `BirdHabit.cpp` (plain C++, `LyudBirds::FHabits`, owned by `FWorld`) |
| Tests | `tools/birdsim/habits_test.cpp`: `BIRD_ONLY=habits sh tools/birdsim/run.sh test` (60 checks, about a minute) |
| The game | `UBirdLifeSubsystem`: `OnBirdHabit(bird, habit, start/beat/end)`, `HabitOf`, `SetHabitWaters`, `AddMorsel`, `DevHabit`; `lyud.Birds.Habits 0` turns them off |
| The book | `ULyudBirdBook::NoteHabit`, `Habits` (saved), `HabitsOfSpecies`, `FamilyCount`; the diary's bird card lists them |
| Words | `bird.habit.<name>`, `ui.birds.habits`, `ui.birds.habit_seen`, `ui.birds.habit_unknown`, `journal.bird.habit`, `city.coll.bird_habits.title` |
| Developer mode | F7, Actions, "Bird habits": the nearest bird that can starts one. Console `lyud.Birds.Habit [name]` |

## How a habit starts

Three sources (`EHabitFrom`):

- **Dice.** A calm bird within about 70 m of the camera (more for a big bird), in the right act, kind of place, hour,
  season and weather, starts it now and then (`Rate` a minute per bird). Each habit has a budget: at most `PerDay` a day
  (3 to 8) and `Gap` seconds apart (3 to 20 minutes), and at most four dice habits run at once. The game's day is a real
  day, so in an hour's walk the hero sees a given habit once or twice.
- **Mapped.** The bird life already does it: a hooded crow cracking a walnut on the paving, jackdaws preening each other,
  a magpie hiding a shiny thing or pulling a tail, a raven tumbling, a jay hiding an acorn (all the corvids' own tasks,
  `BirdCorvid.cpp`), a cormorant's wings held out, its dive, a swan upending, a hawk's attack. The habit only announces
  it while it lasts; nothing about the bird changes, and there is no budget.
- **Heard.** The herring gull's long call (`EVoice::LongCall`) comes with its posture.

Needs (`Need*` flags): dry ground or puddles (`FHabits::Wet` rises in rain and dries in a couple of hours), a warm sunny
hour, frost, a feeder, water within reach, a rival at its place, food in the open, the group feeding, resting or at the
gathering before the roost, the player noticed calmly, standing on the ground.

## How a habit moves the bird (`EHabitMech`)

- **Still**: where it sits or stands. The bird life sees a plain act (`Base`: perch, preen, peck) for the habit's length.
- **Pair**: it sidles up to a neighbour of its kind, both are in it and end together (a fright of one ends both).
- **Run**: short fast runs on the ground with stops (the wagtail: 2.6 m/s, three runs).
- **Sally**: from its place out to a point and back: a chaffinch's flycatching, a robin's pounce to the ground, the wood
  pigeon's display flight, a herring gull's snatch, a fieldfare driving a rival off its rowan.
- **Swoop**: on the wing down to a point and on, one curve through three points whose lowest point is the target: a
  swallow's sip, the sea eagle's fish, swifts over the ridge of a roof, a martin fluttering at the bank.
- **Hang**, **Dance**: held on the wind over one spot (buzzard), a row of dives and climbs (golden eagle).

Habits that move the bird are flown by `FHabits::Advance` (the bird life's `Move` keeps off: `FBird::bHabitMoves`) and
bring it back to where the bird life left it. Anything else taking the bird (a fright, its flock leaving, the player
becoming it) ends the habit at once and the bird life goes on from wherever the bird is.

**Never in front of a wall.** Every path is checked before the habit starts: the game's line trace (`FWorld::Sight`)
along each leg, the ground under it (never below the ground or the sea), a run's end on the same level (a kerb is fine, a
ledge is not: `FWorld::Probe`). A habit that needs room (`Room`: wings spread, a bow) checks ahead and to both sides,
turns the bird round if only the way it faces is blocked, and does not start boxed in.

**No cost when far.** `FHabits::Consider` and the mapped checks leave at once for LOD tiers 2 and 3; `Step` walks only
the running habits (a handful). The tests check that no bird is even looked at while the camera is far.

## The game's side

- `BodyState` (BirdLife.cpp) plays the clip a habit names (`FHabitDef::Clip`: Peck, Preen, Look, Call, Walk, Hop,
  WingsOut) until the habit has a clip of its own. **Owed, the animations thread's**: own clips for the habits, by
  `OnBirdHabit` and `HabitOf`. The beats are the moments for a sound or a puff (a knock of the snail on the stone, a
  toss of leaves, the touch of the water).
- The hero has a habit in view for a second (within 12 m plus 50 m per metre of the bird's length and 22 degrees of the
  view's middle; through the binoculars 250 m and 4 degrees, nothing in between by a line trace): `NoteHabit` writes it.
- `SetHabitWaters`: fountains and other water the game names for the swallows. The brook's pools need no caller: see "The brook".
  `AddMorsel`: a snack on a bench, a fisherman's catch, for the herring gull's snatch; the snatch comes as a `gull_snatch`
  beat and `LastSnatched` names the morsel. **Owed**: the town life and the fishing calling these two.
- **Owed on the Mac**: the first compile of the game's side, a sketch per habit for the book (now the line and the date
  only), the book page's shot, the benchmark against main.

## The habits

**Crows** (stamp `bird_habits/crows`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| Rook | `rook_parliament` | still | In the evening the rooks gather on the field and caw, bowing with every call |
| Rook | `rook_probe` | still | Drives its bill into the turf up to the eyes and pulls out a grub |
| HoodedCrow | `crow_nut_drop` | mapped: the corvid's task | Drops a walnut or a shell on the paving from a height, then walks down to eat it |
| HoodedCrow | `crow_roof_slide` | still | In frost it slides down a snowy roof and walks back up to slide again |
| Jackdaw | `jackdaw_allopreen` | mapped: the corvid's task | A pair sits on a roof, each preening the back of the other's head |
| Jackdaw | `jackdaw_chimney` | still | Peers down a chimney with its head on one side |
| Magpie | `magpie_hide_shiny` | mapped: the corvid's task | Carries off a shiny thing and hides it under a leaf |
| Magpie | `magpie_tail_pull` | mapped: the corvid's task | Creeps up from behind and tugs a cat's or a dog's tail |
| Raven | `raven_tumble` | mapped: the corvid's task | In the wind it rolls onto its back and flies like that, croaking |
| Raven | `raven_bow` | still | Puffs its throat, bows and makes a sound like knocking on wood |
| Jay | `jay_acorn_cache` | mapped: the corvid's task | Carries acorns off and buries them one by one, remembering each place |
| Jay | `jay_anting` | still | Sits on an anthill with its wings spread and lets the ants run through its feathers |
| Nutcracker | `nutcracker_anvil` | still | Wedges a cone in the fork of a stump and hammers until the seeds come out |
| Nutcracker | `nutcracker_store` | mapped: the corvid's task | Fills the pouch under its tongue with seeds and carries them off to hide for the winter |
| CarrionCrow | `carrion_dunk` | still | Soaks a dry crust in the water before eating it |
| CarrionCrow | `carrion_strut` | run | Walks with a swagger and now and then jumps sideways on both feet |

**Wings** (stamp `bird_habits/wings`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| FeralPigeon | `pigeon_bow_coo` | pair | The male puffs his crop, bows and circles before the female, sweeping the ground with his tail |
| FeralPigeon | `pigeon_sunbathe` | still | On a warm noon it lies on its side and lifts one wing to the sun |
| WoodPigeon | `wood_pigeon_display` | sally | Climbs steeply, claps its wings over its back and glides down on spread wings |
| WoodPigeon | `wood_pigeon_drink` | still | Drinks from a puddle without lifting its head, as if through a straw |
| CommonSwift | `swift_screaming_party` | swoop | In the evening a pack of them races past the roofs, screaming |
| BarnSwallow | `swallow_drink` | swoop | Drinks on the wing, drawing its bill through the water |
| BarnSwallow | `swallow_wire_stretch` | still | On the wire it stretches a wing and a leg down together, tail streamers spread |
| SandMartin | `martin_bank_flutter` | swoop | Hangs fluttering at a hole in the bank, then swings away again |

**Water** (stamp `bird_habits/water`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| HerringGull | `gull_snatch` | sally | Watches for food on a bench and snatches it on the wing |
| HerringGull | `gull_long_call` | mapped: its voice | Gives its long call: head down first, then thrown right back |
| BlackHeadedGull | `black_headed_squabble` | pair | Two gulls face off, heads low and wings half raised, scolding in creaky voices |
| CommonGull | `common_gull_paddle` | still | Paddles its feet on the wet sand to bring the worms up |
| Cormorant | `cormorant_wings` | mapped: its act | After fishing it stands on the shore with its wings held out to dry |
| Cormorant | `cormorant_dive` | mapped: its act | Before a dive it jumps clear of the water and goes in head first |
| MuteSwan | `swan_busk` | still | Arches its wings like sails, lays its neck back and surges at an intruder |
| MuteSwan | `swan_upend` | mapped: its act | Tips up tail in the air to reach the food on the bottom |
| GreyHeron | `heron_stalk` | still | Wades slowly through the shallows, freezes, then strikes with its bill like a spear |
| GreyHeron | `heron_sun_delta` | still | In the morning it stands facing the sun with its wings drooped half open |
| BlackBrowedAlbatross | `albatross_sea_run` | mapped: its act | To take off it runs a long way over the water, slapping its feet |
| BlackBrowedAlbatross | `albatross_shear` | mapped: its act | In a strong wind it shears along the waves, hardly beating its wings |

**Garden** (stamp `bird_habits/garden`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| GreatTit | `tit_hammer` | still | Holds a seed under its foot on a twig and hammers it like a woodpecker |
| GreatTit | `tit_hang` | still | Hangs upside down and feeds just as easily that way |
| BlueTit | `blue_tit_acrobat` | still | Swings from the very tip of a twig, looking into every bud |
| Goldfinch | `goldfinch_cling` | still | Lands on a thistle head, bends it over and plucks out the seeds |
| Chaffinch | `chaffinch_sally` | sally | Darts from its twig after a midge and comes back to the same place |
| Bullfinch | `bullfinch_bud_nip` | still | Sits on a branch calmly nipping off buds and dropping the husks |
| HouseSparrow | `sparrow_dust_bath` | still | Bathes in dust: settles in a little hollow and shivers its wings |
| TreeSparrow | `tree_sparrow_puddle` | still | After rain it bathes in a puddle, sending up spray |

**Singers** (stamp `bird_habits/singers`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| Starling | `starling_mimic` | still | Sings on the wire and works other sounds into its song: a whistle, a car alarm |
| Starling | `starling_gape` | still | Pushes its closed bill into the ground and opens it to part the grass |
| SongThrush | `thrush_anvil` | still | Beats a snail against a stone until the shell breaks |
| Fieldfare | `fieldfare_guard` | sally | Guards a rowan tree and drives other birds off it |
| Blackbird | `blackbird_leaf_toss` | still | Flings leaves aside with its bill and looks at what was under them |
| Blackbird | `blackbird_worm_tug` | still | Listens to the ground with its head cocked, then hauls out a worm, feet braced |
| Robin | `robin_bob` | still | Seeing me, it bobs and flicks its tail but does not fly |
| Robin | `robin_pounce` | sally | Drops from its perch to the ground, grabs something and flies back up |
| WhiteWagtail | `wagtail_dash` | run | Chases flies over the wet sand in quick dashes |
| WhiteWagtail | `wagtail_leap` | still | Springs straight up and takes a midge out of the air |
| Waxwing | `waxwing_gulp` | still | Swallows rowan berries whole, one after another |
| Waxwing | `waxwing_pass` | pair | Two of them pass a berry from bill to bill, back and forth |

**Raptors** (stamp `bird_habits/raptors`)

| Species | Habit | Kind | What the hero sees |
|---|---|---|---|
| Sparrowhawk | `sparrowhawk_dash` | mapped: its attack | Dashes low along a hedge, and the sparrows scatter every way |
| Sparrowhawk | `sparrowhawk_pluck` | still | Sits on its plucking post and plucks its catch, feathers drifting off |
| Peregrine | `peregrine_stoop` | mapped: its attack | Folds its wings and falls on its prey from a height like a stone |
| Peregrine | `peregrine_bob` | still | On its tower it bobs its head up and down, judging the distance |
| WhiteTailedEagle | `sea_eagle_snatch` | swoop | Passes low over the water and lifts a fish in its feet without landing |
| GoldenEagle | `golden_eagle_dance` | dance | High in the sky it dives and climbs again and again, as if riding waves |
| CommonBuzzard | `buzzard_hang` | hang | Hangs on the wind over one spot, looking down |

## The brook

The brook's sites (`EHabitat::Brook`: its shallow edges and stones; `Mouth`: its fan across the beach; docs/bird_life.md
"The brook") are narrow patches with the water a step away, and the habits know it:

- The wagtail's dash and leap, the heron's stalk and the carrion crow's dunking happen on both kinds; the common gull's
  paddling and the black-headed gulls' squabble on the mouth's sand only (the bank is too narrow for them).
- A run (`EHabitMech::Run`) by the brook ends on the bank: every dash is pulled back onto the site's patch
  (`FWorld::KeptOnPatch`, the same rule the bird life walks by), so no wagtail runs into the stream.
- The swallows drink from the brook's pools. `FHabits::Pools` holds them: the samples of the brook's water
  (Content/South/south.json "brook" "water") wide enough to be a pool and above the tide (`FHabits::IsPool`: half width
  from 2 m, more than 2 m over the mean sea; 31 of them). The game reads them itself at each start of the bird life
  (`UBirdLifeSubsystem::LoadBrookPools`), the tests by the same rule.
- A pool lies in the brook's cut, which the ground's grid does not resolve (the grid has the ground some 0.9 m over the
  water there). So a swoop to water takes the water as its floor at the pool and the grid's ground again within a few
  metres to either side, and looks along its way to a point over both. Where the far side is higher than the swallow
  flew in, it leaves over it. **Owed on the Mac**: a look at a swallow drinking at a pool with real banks (the line
  trace keeps it out of trees and walls; a steep bank right at the pool's rim could be brushed for a frame or two).

Checks: `BIRD_ONLY=habits` "by the brook" (the bar, who does what where, a pool in reach), `BIRD_ONLY=brook` on the real
map (swallows put over the brook's stretches go down to its pools, each touches the water, none goes under).
