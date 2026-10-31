# The crow family

Rooks, hooded and carrion crows, jackdaws, magpies, ravens, jays and nutcrackers live in the bird life like every other
bird (docs/bird_life.md: groups, sites, the day, fear of the player and of hawks). On top of that each of them has a small
mind, the way the town's people do: needs, a memory, feelings about the player, and from those a task now and then that
it picks for itself. What they carry, hide, drop and steal are real things in the world that the player can see in their
bills, find on the ground and pick up.

## Files

| What | Where |
|---|---|
| Minds, tasks, items (plain C++, no Unreal types) | `Source/Lyudmilovsk/Private/Birds/BirdCorvid.h/.cpp` |
| Hooks into the bird life | `Private/Birds/BirdSim.cpp` (`Corv->...` in Think, Settle, Move, Flush, Danger, PerceiveBeasts, GroupThink, SendGroup) |
| The species' ecology rows | `Private/Birds/BirdEcology.cpp`, `BirdBio.cpp` (Jay, Nutcracker, CarrionCrow at the end) |
| Game side: drawing items, E and G, toasts, save | `Public/Birds/CorvidLife.h`, `Private/Birds/CorvidLife.cpp` |
| Item meshes and icons | `blender/props/corvid_items.py` -> `/Game/Birds/Items/SM_CorvidItem_<Kind>`, `T_Icon_find_<name>` (`unreal/scripts/import_corvid_items.py`) |
| Tests | `tools/birdsim/corvids_test.cpp` (`BIRD_ONLY=corvids tools/birdsim/run.sh test`), `unreal/scripts/test_corvids.sh` |

## The species

| Species | Lives | Known for here |
|---|---|---|
| Rook | flocks on the fields, rookery in tall trees | eats and hides walnuts and acorns, drops walnuts on the road, social above all (the flock comes down to thrown feed) |
| Hooded crow | pairs on the beach and the fields | drops mussels and walnuts on the road until they crack, steals, robs stores, remembers the player best (gifts, grudges) |
| Carrion crow | a pair or two among the hooded crows (the coast is in their hybrid zone) | as the hooded crow |
| Jackdaw | flocks on the roofs and the church tower | shiny things above all, plays in a gale over the tower with the whole party |
| Magpie | pairs in gardens | the boldest thief: snatches feed from the held-out palm, pulls a cat's tail |
| Raven | a pair on the cliff | plays in the wind and with sticks, first at a carcass and calls the others to it, robs stores |
| Jay | singly in the oaks and gardens | buries acorns all October, far from where it found them |
| Nutcracker | a few in the spruces (an invasion autumn brings them to town gardens) | hides hazelnuts and cones up to 300 m off, tame as they come |

The tempers are a table in `BirdCorvid.cpp` (hoard, play, steal, shiny, clever, social, gift, crack, tease, how far it
carries a find to hide it, what it pecks out where it feeds).

## A mind

Every corvid has needs that grow by themselves: **hunger** (the bird life's own), the urge to **hoard** (strong in autumn,
from late August to the end of November, above all for jays and nutcrackers), **company** (its mate) and **play** (when it is
fed and nothing threatens). It remembers its own **stores** (up to 12; the oldest are forgotten and grow into oaks and hazels)
and stores it **watched** others make. Toward the player it has a **bond** (from food: the palm, the feeder, thrown feed,
even a snatch) and a **grudge** (from harm: running at it, throwing things, digging up its store under its eyes, taking what
it was going for). Both fade slowly over game days; a bond makes it trust the player (it fears them less than a third as much, and after a fright
its group comes back within seconds instead of keeping away two minutes), a grudge keeps it farther off, and a crow with a
grudge that has the player in its eye shouts at them instead of only watching (from up to 100 m). The others learn both from it, a grudge fastest: when a crow scolds the player, the crows round it come to
dislike them too.

Every few seconds a free corvid (awake, its group not on the way to the roost, not frightened, within 500 m of the camera)
weighs what it could do, by its needs and temper and what is round it, and picks the best if it is worth it. While on a
task the group doesn't send it elsewhere; when it's done it flies back to the group. Fear always comes first: a watching
bird watches, a fleeing bird flees (and may drop what it carries), the task waits or ends.

## The tasks

- **Fetch**: something lying about that it wants (food when hungry, something to hide, a shiny thing, a stick, anything
  the player dropped). Then it eats it, hides it, cracks it, plays with it or brings it to the player.
- **Harvest**: pecks an acorn, a nut, a cone, a mussel (low tide only) out of where its group feeds.
- **Cache**: carries what it holds to open ground (a meadow, a garden, the forest floor, the sand) up to its distance and
  digs it in. Watched by a clever neighbour it flies on and tries again elsewhere (twice), then hides it anyway, and the
  watcher remembers the place.
- **Retrieve**: digs up its own store when hungry (most of all in winter). If the store is empty and the player took it while
  it could see, that is remembered.
- **Pilfer**: digs up a store it watched being made once the owner is away or asleep; an owner that sees chases the thief.
- **Crack**: carries a walnut or a mussel to the nearest street, flies up 6-11 m over it, lets go, dives after it; on asphalt a
  walnut cracks one drop in three, a mussel nearly one in two, on grass almost never. Up to six tries, then it eats it.
- **Steal**: chases a bird carrying something (not one of its group or its mate, not a stronger species) until it drops it,
  takes it, and hides it.
- **Snatch**: a bold one (magpies, crows, jackdaws, ravens) that half trusts the player grabs a beakful from the held-out
  palm instead of sitting on it, and is off with it to hide it. One handful leaves the pocket.
- **Gift**: one that likes the player (bond > 0.5) brings them something it found (a bottle cap, foil, a nut) and leaves it
  on the ground a few steps in front of them, looks at them, calls, flies off. At most every five minutes per bird.
- **Scold**: one with a grudge calls at the player from a roof or tree near them, again and again, and dives past their head;
  the others pick the grudge up.
- **Follow**: one with a strong bond keeps near the player a few minutes, hopping roof to tree after them.
- **Preen**: a pair at rest sits close and preens each other.
- **Play**: in a gale jackdaws, ravens and crows tumble over the cliff, the tower or a ridge into the wind, the whole party;
  a raven or crow with a stick takes it up, drops it and catches it in the air, up to three times.
- **Tease**: a magpie (or a crow) walks up behind a cat or dog lying or strolling, pecks its tail, hops back, again, a few
  times; if the animal turns on it, it flies.
- **Feast**: a dead herring on the beach (one washes up every 20-45 minutes) draws them from far (ravens from 600 m): the
  strongest eats, the rest wait round it and dart in for a bite now and then.
- **Eat** at the player's thrown feed: several at once.

## Items

`FItem`: acorn, hazelnut, walnut, pine cone, mussel, crust, a dead fish, a pile of feed, a bottle cap, foil, a ring, a
button, a coin, a key, sea glass, a stick. An item lies on the ground, is carried (in the bill: on the jaw bone of a near
body, ahead of the head of a far one), falls, is hidden in a store (not drawn), eaten, or taken by the player. The world puts
them out round the camera by habitat and season (acorns under the oaks from September to November, nuts in the gardens,
cones under the pines, mussels on the wet sand at low tide, crusts and the odd bottle cap in the streets by day, sea glass on
the beach, sticks in the wood) and takes back what nobody touched once the camera is far away. Drawn within 60 m (a fish
120 m), an instanced mesh per kind that is rewritten only when something moved.

## The player

- **E** picks up a find in front of them within reach (a present, an acorn, a shell, a bottle cap...), into the pocket as
  `find_<name>`; and digs up a store they **watched** being made (within 50 m, in view): "[E] Разрыть тайник". If its owner
  sees, it holds a grudge.
- **G** with feed in the hand throws a handful 3.5 m ahead; corvids (and their bond) come for it.
- Holding feed out (F) near a bold corvid may end in a snatch: the hand loses a handful.
- HUD notes: a snatch, a present, the first scolding in five minutes. Quest events: `corvid_snatch`, `corvid_gift`,
  `corvid_gift_taken`, `corvid_store_dug`, `corvid_find`, `corvid_feed_thrown`.
- Save: bond and grudge per bird id ("Corvids").

## Console

`lyud.Birds.Corvids 0|1` (0 = they live like the other birds, their items go; switches while running, for A/B: `tools/bench_systems.sh nocorvids base`), `lyud.Corvids.Items 0|1`,
`lyud.Corvids.ItemDistance <m>`, `lyud.Corvids.Report`, `lyud.Corvids.Spawn <Kind> [count]`, `lyud.Corvids.Feel <bond>
<grudge> [radius m]`, `lyud.Corvids.Throw`, `lyud.Corvids.Take`. `lyud.Birds.Report` ends with the corvids' report.

## Cost

Outside the engine a whole map's day (2293 birds, 178 corvids) steps in 0.07 ms with the camera at the club's door (the
corvids' share is small: tasks run only within 500 m, item searches only within 160 m). In the game the items are a few
small instanced meshes out of Lumen (perf checklist), rewritten only when an item moves.

## Checks

- `BIRD_ONLY=corvids tools/birdsim/run.sh test`: 35 checks in little scripted worlds (a jay buries an acorn and digs it up,
  a crow robs a store it watched, a walnut dropped on the road until it cracks, a crow chases a jackdaw until it drops its
  crust, a magpie snatches from the palm, a present for the player, scolding and the others learning it, the grudge from a
  dug-up store, the queue round a dead fish, a cat's tail, jackdaws in a gale, a pair preening, rooks at thrown feed), then
  a morning on the real map (stores, harvests, nobody lost) and a dusk (everybody asleep, nothing in their bills).
  `BIRD_CORVID_QUICK=1` skips the map, `BIRD_TRACE=1` prints the birds of each scene every half second.
- `sh unreal/scripts/test_corvids.sh`: the same in the game, offscreen, with screenshots.
