# Kites

Backlog item 7 (docs/interactives_backlog.md). The hero buys a kite at a toy shop and flies it from a reel in his hand
on the beach or a cliff top. On windy afternoons the town's children fly kites too.

## The flight

`Source/Lyudmilovsk/Private/Kite/KiteModel.{h,cpp}` is plain C++, so `tools/kitesim/run.sh` can run it outside the engine.

- **The sail** is a flat plate on a bridle. Its plane stands square to the line, tipped back by the bridle angle (24°
  diamond, 20° box). The higher the kite climbs, the flatter it meets the wind and the less it lifts. It settles near 55°
  over the flyer, in the middle of the wind's "window".
- **Lift and drag:** lift is `Cl = ClMax sin 2a`, drag is `Cd = Cd0 + CdPlate sin² a`.
- **Steering:** the tail and the bow of the spars turn the nose into the wind. A kite off to one side works back toward
  downwind of the flyer. A small wobble grows with the air.
- **The air** is the game's real wind: `ULyudWindSubsystem::GetWindAt` at the kite, with its gusts and the log profile with
  height (over water near the sea). A calm lets it fall. Letting the line out takes it higher, where the wind is stronger.
- **Too much air:** above `SpinSpeed` (13 m/s diamond, 18 box) for 1.6–2.5 s the kite **spins**: its lift swings round in
  loops. It comes out once the air drops under `CalmSpeed`, usually only when it is reeled in lower down. A pull over
  `BreakN` (160 N diamond, 260 N box) **snaps** the line.
- **The line** is a tether. It is never longer than the length let out, and reeling in pulls the kite toward the hand (in
  a lull a pull makes its own air). Drawn, it sags under its own weight (`w L² / 8T`); a slack line hangs in a deep curve
  and lies on the ground.
- **States:** wound (on the reel), flying, spinning, grounded (it lies on the ground), snagged and broken. The last two
  mean the kite is lost.
- `AutoReel` is how a practised flyer works the reel: line out slowly at launch, then toward the length they want; a pull
  when it sags; hard in while it spins. The children and the tests use it.

What the test shows (`tools/kitesim/run.sh`, 10 m wind over land with gusts):

| Wind | Diamond | Box kite |
|---|---|---|
| 1.5 m/s | falls within 2 s | — |
| 2.5 m/s | — | can't stay up |
| from 3 m/s | flies, about 55°, 20 m high on 25 m of line | — |
| from 5 m/s | — | flies |
| 7 m/s, gusty | flies all the time | flies |
| 12 m/s | spins | rides it steadily |
| 15 m/s | spins. Held out it keeps spinning or crashes; reeled in, the line holds | — |
| 22 m/s, held | snaps, crashes or spins | — |

`run.sh fly 7 [box]` traces one flight second by second.

## The hero's kite

`ULyudKiteFlying` (`Public/Kite/LyudKite.h`, `Private/Kite/LyudKite.cpp`) follows the fishing rod's pattern. A kite item taken
in the hand (Tab, "take in the hand") comes out on its reel: the hand holds it up in front, and it rides on the hand bone.

| Key | While wound | While up |
|---|---|---|
| LMB | toss the kite up | held: pull the line in |
| RMB | — | held: let the line run out |
| Wheel | — | a metre out or in per click |
| Q | put it away | wind it all in, then put it away |

- While a kite is out, RMB does not raise the binoculars.
- A screen pauses the input but the kite flies on.
- Going into the water winds it in. A jump of more than 30 m (a teleport, a ride) winds it in too.

**Losing it:**
- It is lost when it flies into anything static that isn't the ground (a tree trunk, a lamp post, a wall, a roof). The
  test is a sphere sweep against WorldStatic; the landscape is ground.
- It is also lost in a tree's crown: the crowns' ellipsoids from `FBirdPerches::InCrown`, since the leaves have no
  collision.
- A snapped line or a fall into the sea loses it as well.
- A caught kite stays where it hangs for ten minutes, or until the hero is 250 m away. The item is gone from the pocket.

**Lines:** the hero's lines go through the prompt line (`ui.kite.*`): tossed, flying, fell, the wind dropped, spinning,
steady again, caught, snapped, in the sea. When nothing has been said, the prompt shows what the keys do.

**Stamps:** the set "Воздушный змей", `kites/` in `tools/city/content/collections.json`. The engine gives its pieces
(`"by"`), so `interiors.py --check` doesn't look for a giver in town.

| Piece | Given for |
|---|---|
| `first_flight` | 20 s in the air |
| `high` | 30 m over the hand |
| `box` | the box kite up for 30 s |
| `storm` | a spinning kite brought back to steady flight |

**The hum:** `ULyudKiteHumSynth` plays `LyudKiteHum::FVoice` (`KiteHum.h`) at the reel.
- An aeolian tone at `0.2 v / d` (a 2 mm line: about 800 Hz in 8 m/s) with an octave and a slow waver, over a soft thrum
  of the sail.
- Its level follows `LyudKite::Hum` (how taut, how much air). A slack line is silent.
- The reel ticks (`reel_click`) as it is wound.

**Meshes:** built in code once per shape and shared (`FLyudMeshKit`, kept rooted), coloured with shared rooted material
instances of the engine's basic material:
- a diamond 72 × 95 cm with cross spars, bowed back at the wing tips;
- a box kite with two cells on four longerons;
- the reel (a drum on a handle, with a knob);
- line pieces, and a tail of nine bows that trail downwind with a wave running down them.

Everything is out of Lumen and distance fields. Nothing is rebuilt: the parts are moved, and the line's 16 pieces and the
tail's 9 are re-placed in one batch each per frame. The colourways are our own (five of them).

## The children's kites

- **The activity:** town life's `fly_kite` (`Content/PeopleData/activities.json`): ages 6–15, 11:00–18:30, by daylight,
  on the beach, the sunrise and sunset lookouts and the pier.
- **The wind:** `"wind": [3, 13]` is a new activity field (`FActivity::WindMin/WindMax`, `FWorld::Wind`). Out of that
  range nobody chooses the activity; in the middle of it, it calls a little more.
- **Where the wind comes from:** the game sets `FWorld::Wind` from `ULyudWindSubsystem::GetMeanSpeed`. `tools/peoplesim`
  sets it from the same calendar (`LyudWeather::Regime`, seed 921). In the town test, about 4,000 person-minutes of kites
  fall over 36 hours (about five kites at once on a breezy afternoon), and none in the dark, a calm or a gale.
- **Drawing them:** whoever is flying a kite and is drawn within 300 m of the camera gets one, up to `lyud.Kite.Town`
  (12). It is on a line from their right hand, worked by `AutoReel`. One child in three has a box kite, and the colours
  come from who they are.
- **Losing one:** a child's kite caught in a tree stays there for the rest of their outing.
- **Town talk** (`tools/barks/lines/13_kites.txt`):
  - the children while they fly;
  - a teen on box kites;
  - grown-ups on the beach watching;
  - a flier who sees the hero near.

## Shops

| Item | Shop | Price |
|---|---|---|
| `kite_diamond` | toy shop (P-WAVE, P-UNITE) | 150 ₽ |
| `kite_diamond` | the fair's toy kiosk | 160 ₽ |
| `kite_diamond` | the department store's sports counter | 150 ₽ |
| `kite_box` | toy shop (P-WAVE, P-UNITE) | 280 ₽ |
| `kite_box` | the department store's sports counter | 270 ₽ |

The department store's toy house is a board only, so its kites are at the sports counter.

Icons: one 2×2 Higgsfield grid (`design/kites/icon_kites_grid.json`). It still has to be downloaded and cut on the Mac,
because the cloud's egress refuses the CDN.

## Console and developer mode

**Console commands:**
- `lyud.Kite.Give [diamond|box]`: a kite in the pocket and in the hand.
- `lyud.Kite.Launch`: toss it up.
- `lyud.Kite.Reel -1|0|1|off`: pull, hold or let out without the mouse; `off` gives the mouse back.
- `lyud.Kite.State`: the state in the log.
- `lyud.Kite.Debug 1`: the state on screen.
- `lyud.Kite.Test N`: N kites in a row 25 m downwind of the camera. The bench uses 10.
- `lyud.Kite.Town N`: at most N children's kites drawn.
- `lyud.Kite 0`: all kites off.

**Developer mode (F7):**
- group "Kites": give kites; ten kites downwind (the bench);
- teleport "Kite beach" (15:00).

## Tests

- `tools/kitesim/run.sh` checks:
  - the lift model: calm, breeze, gale, storm;
  - a snag;
  - the line's sag;
  - determinism;
  - each kind's least wind;
  - the hum DSP: silent when slack, louder and higher with air, no clipping.
- `tools/peoplesim/run.sh town` checks that the children fly kites by day in a breeze.
- Still to do on the Mac: shots of the hero's kite and the children's, and a bench with `lyud.Kite.Test 10` against main.
