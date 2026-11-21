# Bird flight

When a befriended bird eats from the hero's hand, the camera flies out of the hero's eyes into the bird's
and you play as the bird, in first person. This page is the control concept first, then how the flight
works, then the code.

## Control concept: "the head leads, the wings row"

Two ideas, both taken from how a real bird flies.

**The head leads.** A bird turns its head first and its body follows; its head stays level while the body
rolls and bobs under it. So the mouse is the bird's head. Where you look is where the bird wants to go:
the body banks and climbs or dives toward the gaze with a lag that depends on the species (a swallow
follows in a blink, a gull swings round slowly). The camera is the head, so it keeps the horizon nearly
level however hard the body banks (only a quarter of the body's roll reaches the eye), and the wingbeats
never shake it.

**The wings row.** The two mouse buttons are your two wings, like the two oars of a boat.

| Input | What the bird does |
|---|---|
| Click LMB and RMB together | One wingbeat at full power: down on press, up on release. Clicking rhythmically is flapping; clicking as fast as the species' wingbeat rate allows is its sprint (and its steepest climb), and tires it several times faster than Space |
| Click one button | One beat of that wing only: a hard roll and turn away from it (rowing with one oar) |
| Look down | A dive: from about 12 degrees below the horizon the bird folds its wings by itself, as far as its kind does, and falls faster and faster; the steeper the look, the tighter the fold |
| Look up out of a dive | The wings open as far as the air lets them, the path bends up within the g the species' wings take, and the speed zooms back into height; slowing, they spread right out and you glide |
| Hold both | Wings pulled right in to the body by hand, whatever the species: you fall and gain speed (the "bound" of a finch between bursts) |
| Hold one | That wing pulled in: a hard roll and a tight turn toward it |
| Nothing pressed | Wings spread: you glide |
| Mouse | The head: look, and the body steers after the look |
| Space | Both wings beat; hold it to flap at the species' natural rhythm and pattern, its easy travelling flight at cruise speed (keyboard-only play) |
| Shift | Flare: tail fanned, wings forward, feet down. Brakes, and lands on whatever is under or in front of you |
| Q (hold) | The landing magnet: the nearest perch ahead (a real branch of a tree, a roof, a wall top, a rail) pulls you in like a web line; the bird beats and brakes by itself and lands there, its toes closing round the branch. Nothing within reach: Q does nothing and you fly on. From the ground it takes off and goes for the next one |
| Ctrl | Wings pulled in, same as holding both buttons |
| A / D | Roll with the tail (fine trim, keyboard-only turns) |
| C (hold) or middle mouse | Free look: the head turns, the course stays |
| On the ground: WASD | Hop or walk (sparrows and tits hop, pigeons, rooks and wagtails walk, nodding) |
| On the ground: Space or a two-button click | Take off |
| E | Peck (on the ground or a perch) |
| F | Call (the species' voice) |
| R (hold 1.5 s) | Back to the hero |

So flapping, gliding and bounding are not modes you pick: they are what you do with your hands.
A finch's flight is click-click-click-hold-click-click-click-hold; a gull's is a few slow beats and then
nothing but the mouse for a minute along the cliff.

**AirPods head tracking.** With AirPods (Pro, Max, 3rd gen and later) your own head turns the bird's head on
its neck, on top of the mouse: up to 110 degrees to each side, down and up, and a tilt. It is pure looking,
it never steers, so you can fly straight along the cliff and turn your head to watch your own wing over
the sea, the way a streamer's camera would. The neck centre drifts back to straight ahead over a few
seconds, so the tracking can't wander off. When the AirPods lose tracking or are taken off, the head eases
back to centre over half a second instead of snapping. Without AirPods, C / middle mouse does the same
with the mouse. Optional (console `lyud.Bird.HeadSteer 1`): tilting your head banks the bird, for hands-free
gliding.

**What you see of yourself.** You are inside your own body: every species wears its detailed hero body
(`SK_<Id>_Hero`) and the camera sits between its two eyes, on the head bone, so it moves with the head and the neck.
Only the skull around the eyes is hidden (the material slot `Skull`): the bill stays, at the bottom of the view, small
as a bird sees its own bill, pointing where you look; the eyes' rims and the edges of the cheeks show at the sides.
The head always goes exactly where you look (the neck takes half of a turn, the head the rest, up to 170 degrees
round). Look down and the neck lifts and stretches, drawing the head up and back over the breast, so you see your
breast, belly and feet under you, joined to you through the neck; turn round and the head rises, so you see the wing
from above. Turn the head (C or AirPods) and you see the whole wing: separate primaries, secondaries and coverts, the
tips bending up under load and fluttering at speed, the wing folding in when you hold both buttons. The view is wide
(100 degrees, up to 108 at speed), because a bird's is, and widens smoothly by up to 20 more as the head turns to the
side or down (`lyud.Bird.TurnFov`).

**Seeing your wings.** A bird sees almost all the way round; a 100 degree screen does not. So the
bird's own body is drawn with a far wider first-person field of view (118 degrees, `lyud.Bird.WingFov`), and the spread wings are drawn swept a little forward from beside the eye:
the spread wings beside you reach into the left and right edges of the screen and sweep through them
with every beat. Turning the head toward a wing eases that back to the true view.

**Comfort.** The head-stabilised camera is also what keeps this from making anyone sick: the horizon
stays put (at most 8 degrees of the body's roll reach it, none while slow), there is no camera bob from the
wings, no motion blur, and the field of view changes only slowly: with airspeed, and eased in and out as the head
turns.

## Flight

The bird is a point mass with an airspeed vector, a body orientation and two wings, each with a spread
(0 pulled in .. 1 spread) and a stroke phase. Per frame:

- **Steering.** The body chases the gaze: the heading turns at up to the species' turn rate, the bank is
  the coordinated-turn bank for that rate (`atan(V·ω/g)`), plus the roll from a one-wing beat or A/D.
  The climb angle follows the gaze pitch.
- **Energy.** Along the path, `dV/dt = -g·sin(γ) - D/m + T/m`. Drag has a minimum at the species'
  best-glide speed equal to `g / glide ratio` (a glide polar), rising as V² above it and as 1/V² below
  it (induced drag). Wings pulled in cut drag to the body's and lift to nearly none, so you fall
  ballistically. Below the stall speed the nose drops on its own.
- **Lift limit.** How fast the course can bend (turn or pull up) is limited by the lift the wings can
  make at this speed (`n_max · g · (V / V_stall)²`), so a slow bird turns wide and a diving one can pull a
  hard loop.
- **Wingbeats.** Each downstroke adds thrust along the path and lift (enough to take off from standing
  and to climb slowly); a beat faster than the species' wingbeat rate is folded into the next one. A
  one-wing beat adds roll and yaw instead of much thrust.
- **Air.** The bird moves with the air: ground velocity = airspeed + wind + updraft. A steady sea wind
  (from the west-south-west, 3.5 m/s by default) blows against the sea cliff and the upland's slopes; where
  it meets rising ground it rises (`w = wind · ∇ground`, fading out a few cliff heights above it), so you
  can hang in front of the cliff without a beat, as the gulls do. On sunny days thermals rise over open
  land and drift downwind.
- **Collisions.** A swept sphere the size of the body. Touching something slowly and from above lands;
  hitting it fast bounces you off with lost speed and a short stun (feathers ruffled, the view shakes
  once). The ground is a hard floor, and the map's edge and a ceiling turn the bird back with a headwind.
- **Landing and perching.** Shift near any walkable surface (ground, roof, rail, a wall top, a branch with
  collision) aims the bird at it: a faint ring marks the spot, the bird flares and its feet grip. Perched,
  the mouse looks around, Space or a beat takes off.
- **Perches are real.** The magnet only ever lands on something that is there: a branch, from the same branch
  skeleton the tree's wood is drawn from (`FPlantSkeleton` with the tree's seed, age and season; a forest's tree near
  the camera is the live plant standing in for it, of its own age), or a surface the traces hit (roofs, wall tops,
  rails, the cable car's crossarms and railings, the ground). A branch is good when the feet can close round it (from
  0.14 to 0.55 of the body's radius), it is near level, it is a limb or the inner third of a twig (which hardly sway),
  and the body has room above it; the outer branches come first. The bird lands across it, its origin (between the
  feet) on the top of the wood; each leg reaches for the branch right above its axis (two-bone IK, knee to ankle to
  foot) and each toe closes round it joint by joint, every piece a chord of the circle round the wood (`FBirdGrip`,
  `ABirdActor::SetGrip`), in first person and from outside alike. If the bird cannot reach the perch (it stops getting
  closer for 2 s) it lets go and flies on; it never stops in the air.
- **On the ground.** Hops (or walks, by species) with WASD, the head bobs with each step; E pecks.

### Species

Each species has its own flight from the species table (`Content/Birds/species.json`: mass, wingspan,
wingbeat rate, cruise speed, flight style). The flight style sets the glide ratio, the turn rate and what
Space does when held:

| Style | Birds | Feel |
|---|---|---|
| Flapping | pigeons, crows, rooks | steady fast beats, fast and direct, modest glide |
| Bounding | finches, tits, goldfinch, wagtail | bursts of beats with wings closed between, undulating |
| Gliding (flap and glide) | starling, thrushes, fieldfare | beats then a straight glide |
| Soaring | gulls | slow beats, long glides, rides the cliff wind |
| Clapping | pigeon take-off | loud first beats, a climb at a steep angle |
| Flicker | swift, swallow, martins | very fast, very agile, flickering beats, sharp turns |

Holding Space flies level at the species table's cruise speed (a sparrow 10 m/s, a pigeon 17, a swift 15,
a gull 11): the model tunes each species' wingbeat thrust to it when the bird is made.

## A bird's life (round 3)

- **Biology.** Each species has its own stamina, top dive speed and agility (Private/Birds/BirdBio.cpp, from
  measured flight speeds and ranges): a blue tit manages about 30 s of steady flapping, a starling three minutes,
  a gull ten, a swift and a peregrine all day; a peregrine stoops at 90 m/s, a magpie at 22. Holding Space or
  clicking spends strength; gliding, soaring, standing and swimming give it back. A spent bird's beats barely lift
  it (the strength bar at the bottom turns warm, and a note says to glide down and rest).
- **Water birds** (swan, cormorant, the gulls) settle on the sea and paddle with WASD; a wingbeat takes off, a swan
  pattering along the water for seconds first. A land bird that falls into the sea is done: you are the hero again,
  in the water where it fell, and swim ashore.
- **Danger.** Flying into something too fast knocks the bird out; a cat or a dog that gets within reach on the
  ground catches it ("Danger: a predator nearby!" when one is close and hunting). Either way you are the hero again.
- **Calling the bird back.** As the hero, on dry land, hold R: your bird flies back to your hand.
- **Your kind.** F calls. A bird of your species nearby answers by chance and by sex: two males fight (it comes at
  you and pecks, three times), a male and a female court (it answers and comes closer), two females call back;
  often they only look. E pecks.
- **Sound and wind.** The bird's own sound is made as you play: the rush of air with speed and gusts, and every
  downstroke (a sparrow's whirr, a pigeon's take-off clap, the swan's singing wings). The world's wind
  (ULyudWindSubsystem) blows the bird about and is drawn as faint streaks drifting in the air around you; where they
  rise (the sea cliff, a thermal) the air lifts you.
- **The body others see** is the species' detailed hero body (SK_<Id>_Hero), hopping down off the palm onto the
  ground before you take over.

Dev scripts: `Bird.Script water:MuteSwan | drown:Starling | tire:BlueTit | call:Starling` (with
`lyud.Birds.AnyMap 1` in a test level; `lyud.Bird.ForceAnswer 3` forces a fight).

## Flying qualities (round 4)

Every species flies as well as the real one: `Private/Birds/BirdBio.cpp` holds, next to stamina, dive speed and agility,
its **sprint** (the fastest level flight at full power, m/s), its **climb** (the steepest climb by flapping, m/s: the strength
of its flight muscles), the **wind it can cope with** (the mean wind at which wild birds of it keep to shelter) and its
**glide ratio**. Sources: Alerstam et al. 2007 (cruising airspeeds), Bruderer & Boldt 2001 (radar), Pennycuick 2008
(power curves, climb, glide), Henningsson et al. 2010 (swift sprints), Tucker 1998 (stoops).

| | sprint m/s | climb m/s | wind limit m/s | glide |
|---|---|---|---|---|
| blue tit | 11 | 2.5 | 7 | 5:1 |
| house sparrow | 13 | 2.6 | 8 | 5:1 |
| starling | 20 | 2.8 | 13 | 7:1 |
| feral pigeon | 24 | 3.0 | 13 | 7:1 |
| herring gull | 18 | 1.5 | 24 | 15:1 |
| common swift | 31 | 2.5 | 18 | 12:1 |
| peregrine | 30 (stoop 90) | 4.0 | 22 | 10:1 |
| mute swan | 22 | 1.0 | 18 | 7:1 |
| white-tailed eagle | 17 | 0.9 | 22 | 15:1 |
| golden eagle | 20 (stoop 67) | 1.0 | 22 | 16:1 |
| black-browed albatross | 18 | 0.5 | 35 | 22:1 |
| grey heron | 15 | 1.3 | 15 | 9:1 |

How the model holds them (`BirdFlightModel.cpp`, fitted per species when the bird is made, ~50 ms):

- **Power, not force.** Above cruise speed a beat's thrust falls as 1/V (the muscles deliver power), so clicking flat out
  settles at the sprint. The species' rhythm on Space uses a fitted share of full power (`CruisePower`) and holds the cruise;
  a weak flier that needs everything to cruise (the eagles, the albatross) gets shorter glides between its bursts instead.
- **The climb.** What the beats put into speed and height, averaged over a second, is at most what level flight costs here
  plus lifting the bird at its climb rate (`ClimbCap`, fitted). A swan climbs at 1 m/s, a peregrine at 4; a zoom (trading
  speed for height on spread wings) costs the beats nothing. A take-off gets a few seconds of extra (anaerobic) power; heavy
  birds can't lift themselves straight up and must fly up to speed first.
- **Headwind.** The bird moves with the air, so a headwind stronger than its sprint blows it backwards over the ground.
  The wind is weaker near the ground (the HUD says so: `bird.note.headwind`).
- **Gusts.** A change of the air changes the airspeed, not the ground speed: the bird has mass, the air takes it along only
  through its drag. On top of the wind come random gusts (`FAir::Turbulence`, from the wind's gustiness, rougher over land
  and low down). A blue tit's track wanders a quarter of its speed in a gusty crosswind, a white-tailed eagle's a seventh.

Wild birds (`BirdSim.cpp`, docs/bird_life.md "Wind") use the same sprint and wind limit.

`unreal/scripts/test_bird_flight.sh` checks each species' sprint, climb, a headwind just under and just over its sprint,
and the gusts (a blue tit against an eagle), besides the glide, cruise, take-off, dive, stall, turns, ridge lift, flare
and stamina; and, for round 5, the dive by the gaze alone, the pull-out, the zoom, the span at speed, the wingbeat rates
and a peregrine's stoop against a swan's.

## Wings that trim themselves (round 5)

Nobody tells a falcon to fold its wings: the bird does it, and so does the player's. The span of the wings is a thing
the bird sets by itself every moment (`BirdFlight::WingTrim`, the state `FState::Trim`), and the drag and the lift are
computed from that span, so a dive, a pull-out and a soaring glide are not modes or animations: they are what the look
and the speed do to the wings.

- **The look down folds them.** From about 12 degrees below the horizon to 50, the wings come in as far as the species
  folds them (`BirdBio` column `tuck`): a peregrine to a teardrop (10 % of its span), a pigeon and the small birds to a
  quarter, a gull to an M (37 %), a swan hardly at all (82 %). Holding Space in a dive stops the rhythm: a diving bird
  does not flap.
- **Speed presses them back.** In a fast glide the span comes in steadily above the best glide speed (Pennycuick 1968,
  Tucker & Parrott 1970): about 70 % at twice the best glide speed. Fully spread wings also have a speed limit
  (`FParams::SpreadLimit`, well past the sprint, half the dive speed for a stooper); `FParams::MaxSpeed`, the biology's
  dive speed, is the same limit for the wings folded in. A peregrine that holds its wings out stops at 49 m/s, folded it
  reaches 86.
- **The look up spreads them.** With the gaze above the path (pulling out, or holding a height the speed no longer
  carries) the wings open as wide as the air allows, and never less than the pull needs. The pull is limited by the g the
  species' wings take (`pull`: a peregrine 8 g, a swift 6, a swan 2.2), so a swan needs 35 m and more than two seconds to
  level out of its fastest descent and a blue tit 7 m and two thirds of a second.
- **Drag by span.** The parasite drag falls with the wings in, the induced drag rises on the shorter span, so a
  half-folded bird pulling hard pays for it, but most of a dive's speed comes back as height: the test's peregrine turns
  64 m/s into 139 m.
- **Rolling.** How fast the body rolls into a turn follows the species' agility (`FParams::BankRate`): a swallow snaps
  over, a swan leans in.
- **Wingbeat rate and depth.** The species' easy rhythm (Space) beats at the species table's rate with shallower
  strokes; clicking flat out beats up to 30 % faster and deeper, and a take-off's first beats faster still
  (`BirdFlight::BeatRate`, `FState::Amp`): a pigeon 7 Hz travelling, 8.2 flat out, 9.2 off the ground; a blue tit 27, 29
  and 33; a white-tailed eagle 2.0, 2.1 and 2.3.

| | folds to | dive m/s | pull-out g |
|---|---|---|---|
| peregrine | 10 % | 90 | 8 |
| golden eagle | 15 % | 67 | 5 |
| common swift | 19 % | 45 | 6 |
| feral pigeon | 24 % | 40 | 4.5 |
| house sparrow | 10 % | 23 | 4 |
| herring gull | 37 % | 35 | 3 |
| white-tailed eagle | 46 % | 35 | 3 |
| grey heron | 73 % | 25 | 2.4 |
| mute swan | 82 % | 30 | 2.2 |

- **The rhythm works harder when it has to.** Space is the species' easy rhythm, and level at cruise it beats at the
  share of power that holds the cruise. Slow (below about 0.6 to 0.95 of cruise: a take-off, the first seconds after
  a landing was called off) or with the gaze up (from 2 to 14 degrees), the same rhythm goes up to full power
  (`BirdFlight::RhythmPower`), and since the wingbeat rate follows the power, the wings visibly beat faster. It costs
  stamina as full-power beats do. A take-off's first 0.6 s are flat-out beats at the flat-out rate. Check 18 of the
  model test flies the take-off the way the pawn does.
- **A glide too slow to last is not held.** Not beating, not flaring and with the gaze not well up, a bird below
  about 0.55 to 0.8 of its best glide speed lets its wings go light and the nose down until the speed is back, and
  settles in a slow glide near its minimum sink (a sparrow at 4.9 m/s) instead of hanging on the edge of a stall.
  With the gaze held 30 degrees up it still mushes, as asked.
- **How it is drawn.** A part-folded wing in the air is not half way to the standing bird's fold (that hangs it down the
  flank): `ABirdActor::SetWingLevel` rolls the arm back out to the side, so the fold is a sweep back in the wing's own
  plane, a falcon's "M"; right in, it closes against the body. The tail closes tighter than its bind pose in a dive
  (`TailFan` below 0), and the body no longer rolls about on a breath of side wind while it falls unloaded.

The landing magnet (Q) and the first beats of a take-off fly with the wings out (`FInput::Reflex` 0). Whoever draws the
bird gets the same numbers as before (`FState::SpreadL/R`, the phases) through `ABirdActor::SetWingPose`, plus the depth
of the stroke; a new wing rig only has to pose the wings for a spread of 0 (folded) to 1 (the bind pose).

`Bird.Script stoop:<Id>` (`unreal/scripts/record_bird_flight.sh stoop:Peregrine`) starts 420 m over the sea, looks down,
level and up, and logs `BIRD stoop <phase> ...` with the height, the airspeed, the wings and the g twice a second.
`lyud.Bird.SpeedFov` is how far the view widens from cruise speed to the fastest dive (12 degrees).

## Becoming the bird and back

The befriending thread calls `UBirdFlightSubsystem::BecomeBird(PlayerController, BirdId)` after the bird
has pecked from the hand; the bird turns round on the palm to face where the hero looks. It claims the bird from bird life (`UBirdLifeSubsystem::ClaimBird`), and the
camera flies from the hero's eyes along a short arc to the bird's eye (about 1.6 s, eased, the view
widening to the bird's), with no cut and no black frame. Then the player controls a `ABirdPawn` at the
bird's place; the hero stays where he crouched.

Holding R for 1.5 s (a ring fills) flies the camera back to the hero's eyes over 1.2 to 3 s depending on
the distance, lifted over the ground on the way, and the hero gets the input back. The bird is handed back
to bird life where it is (`ReleaseBird`) and flies about on its own.

Saving while a bird saves the game as the hero standing where he is; loading puts you back in the hero.

## Code

`unreal/Lyudmilovsk/Source/Lyudmilovsk/{Public,Private}/Birds/`:

- `BirdFlightSubsystem` — `BecomeBird`, the return, the camera flight between hero and bird, debug
  console commands.
- `BirdPawn` — the player bird: input, head, camera, and the body that shows.
- `BirdFlightModel` — the flight physics (pure C++, no engine state, so it is tested headless).
- `BirdFlightStyle` — per-species flight parameters derived from the species table.
- `BirdSelfBody` — the old close-up body of separate parts (beak, feathers, breast): only for a species that has no
  hero body with a `Skull` slot (every species has one now).
- `BirdPerches` — where the magnet lands: tree branches from their branch skeletons, surfaces by traces.
- `BirdAir` — wind, cliff updrafts, thermals.
- `HeadTracker` — AirPods head pose from the helper app (Mac only; elsewhere it reports nothing).

### AirPods helper

CoreMotion's headphone motion needs an app whose Info.plist explains why it uses motion
(`NSMotionUsageDescription`), and the editor's own app has none, so the tracking runs in a tiny helper app,
`tools/headtracker/` (Swift, builds with `tools/headtracker/build.sh` into
`Saved/HeadTracker/LyudHeadTracker.app`; the first flight on a Mac runs that build by itself). The game starts it while you are a bird and reads the head pose
over a localhost UDP socket; the first time macOS asks to allow motion access for it. No AirPods, no
helper, or another platform: the game simply has no head tracking and C / middle mouse does the looking.

### Testing without a window

```sh
unreal/scripts/test_bird_flight.sh            # physics checks (glide ratio, stall, takeoff, ridge lift, landing)
unreal/scripts/record_bird_flight.sh <name>   # an offscreen flight, recorded to renders/bird_flight/<name>.mp4
unreal/scripts/shoot_bird_fpv.sh <out> HouseSparrow MuteSwan ...     # first person: ahead, a wing, down, a beat, standing
unreal/scripts/shoot_bird_fpv.sh <out> perch:HouseSparrow            # Q onto two branches; BIRDPERCH lines check the feet
```

`Bird.Script fpv:<Id>` holds the bird still and logs `BIRD fpv <view> frame <n> eye <where the eye is on the body>`;
`Bird.Script perch:<Id>` lands on two branches of the upland forest and logs, for every foot bone, its distance from
the branch's surface (`BIRDPERCH ... worst 0.10 cm PASS`, the limit 1 cm), then shoots the feet from the eye, from
beside and from below. Tuning: `lyud.Bird.NeckUp` / `NeckBack` (how far the head goes up and back looking down),
`lyud.Bird.EyeUp` (the camera a little above the line between the eyes, so the bill is seen from above, never end-on
from its root), `lyud.Bird.TurnFov`.

The recordings play scripted input (clicks, mouse moves, a fake head turn) through the same input layer
as the player, at a fixed 30 fps time step.

Console: `Bird.Become [Species]` (a bird in front of the hero, without befriending), `Bird.Return`,
`Bird.Script <name>` (play a scripted flight), `lyud.Bird.Wind <m/s> <from deg>`, `lyud.Bird.HeadSteer`,
`lyud.Bird.Debug 1` (airspeed, sink, updraft, wings on screen).
