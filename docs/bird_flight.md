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
level however hard the body banks (only a third of the body's roll reaches the eye), and the wingbeats
never shake it.

**The wings row.** The two mouse buttons are your two wings, like the two oars of a boat.

| Input | What the bird does |
|---|---|
| Click LMB and RMB together | One wingbeat: down on press, up on release. Clicking rhythmically is flapping; the species' own wingbeat rate is the fastest that helps |
| Click one button | One beat of that wing only: a hard roll and turn away from it (rowing with one oar) |
| Hold both | Wings pulled in to the body: you fall and gain speed (a dive, or the "bound" of a finch between bursts) |
| Nothing pressed | Wings spread: you glide |
| Mouse | The head: look, and the body steers after the look |
| Space | Both wings beat; hold it to flap at the species' natural rhythm and pattern (keyboard-only play) |
| Shift | Flare: tail fanned, wings forward, feet down. Brakes, and lands on whatever is under or in front of you |
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

**What you see of yourself.** The beak tip at the bottom of the view (a bird sees its own beak), the
leading edges of both wings at the screen edges when they are spread, the primaries sweeping through the
lower corners on each downstroke, your breast and folded feet when you look down, the feet coming out on a
flare. Turn the head (C or AirPods) and you see the whole wing: separate primaries, secondaries and
coverts, the tips bending up under load and fluttering at speed, the wing folding in when you hold both
buttons. The view is wide (100 degrees, up to 108 at speed), because a bird's is.

**Comfort.** The head-stabilised camera is also what keeps this from making anyone sick: the horizon
stays put, there is no camera bob from the wings, the field of view changes only with airspeed and slowly.

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
  (from the west-south-west, 5 m/s by default) blows against the sea cliff and the upland's slopes; where
  it meets rising ground it rises (`w = wind · ∇ground`, fading out a few cliff heights above it), so you
  can hang in front of the cliff without a beat, as the gulls do. On sunny days thermals rise over open
  land and drift downwind.
- **Collisions.** A swept sphere the size of the body. Touching something slowly and from above lands;
  hitting it fast bounces you off with lost speed and a short stun (feathers ruffled, the view shakes
  once). The ground is a hard floor, and the map's edge and a ceiling turn the bird back with a headwind.
- **Landing and perching.** Shift near any walkable surface (ground, roof, rail, a wall top, a branch with
  collision) aims the bird at it: a faint ring marks the spot, the bird flares and its feet grip. Perched,
  the mouse looks around, Space or a beat takes off.
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

Speeds are real birds' (a sparrow cruises at ~8 m/s, a pigeon at ~17, a swift at ~25).

## Becoming the bird and back

The befriending thread calls `UBirdFlightSubsystem::BecomeBird(PlayerController, BirdId)` after the bird
has pecked from the hand. It claims the bird from bird life (`UBirdLifeSubsystem::ClaimBird`), and the
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
- `BirdSelfBody` — the close-up body you see in first person: beak, wings of separate feathers, breast,
  tail, feet.
- `BirdAir` — wind, cliff updrafts, thermals.
- `HeadTracker` — AirPods head pose from the helper app (Mac only; elsewhere it reports nothing).

### AirPods helper

CoreMotion's headphone motion needs an app whose Info.plist explains why it uses motion
(`NSMotionUsageDescription`), and the editor's own app has none, so the tracking runs in a tiny helper app,
`tools/headtracker/` (Swift, builds with `tools/headtracker/build.sh` into
`Saved/HeadTracker/LyudHeadTracker.app`). The game starts it while you are a bird and reads the head pose
over a localhost UDP socket; the first time macOS asks to allow motion access for it. No AirPods, no
helper, or another platform: the game simply has no head tracking and C / middle mouse does the looking.

### Testing without a window

```sh
unreal/scripts/test_bird_flight.sh            # physics checks (glide ratio, stall, takeoff, ridge lift, landing)
unreal/scripts/record_bird_flight.sh <name>   # an offscreen flight, recorded to renders/bird_flight/<name>.mp4
```

The recordings play scripted input (clicks, mouse moves, a fake head turn) through the same input layer
as the player, at a fixed 30 fps time step.

Console: `Bird.Become [Species]` (a bird in front of the hero, without befriending), `Bird.Return`,
`Bird.Script <name>` (play a scripted flight), `lyud.Bird.Wind <m/s> <from deg>`, `lyud.Bird.HeadSteer`,
`lyud.Bird.Debug 1` (airspeed, sink, updraft, wings on screen).
