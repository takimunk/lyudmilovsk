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
- **The air** is the game's real wind where the kite is: `ULyudWindSubsystem::GetLocalWind` (docs/wind.md), with its
  gusts, the profile with height, the land's lee and the speed-up over a crest. A calm lets it fall. Letting the line out
  takes it higher, where the wind is stronger.
  - `FKiteInput::Wind.Z` is rising or sinking air: ridge lift over the cliff's edge stands a kite higher and keeps a
    diamond up in 2.5 m/s; the sink behind the cliff pushes it down.
  - `FKiteInput::Turbulence` (rms m/s) is the tumbling the mean wind does not carry: a seeded shake by the kite's own
    clock, so it is the same every time. The line snatches twice as hard in 2 m/s of it.
  - The hero's kite looks at the air every frame, the others ten times a second.
- **The hand moves.** The model eases the hand's own velocity into the tether, so a flyer who runs upwind tows the kite
  through the air (2.5 m/s of wind and a 3 m/s run: 13 m up in 12 s), and one who runs downwind drops it.
- **Too much air:** above `SpinSpeed` (13 m/s diamond, 18 box) for 1.6–2.5 s the kite **spins**: its lift swings round in
  loops. It comes out once the air drops under `CalmSpeed`, usually only when it is reeled in lower down. A pull over
  `BreakN` (160 N diamond, 260 N box) **snaps** the line.
- **The line** is a tether. It is never longer than the length let out, and reeling in pulls the kite toward the hand (in
  a lull a pull makes its own air). Drawn, it sags under its own weight (`w L² / 8T`); a slack line hangs in a deep curve
  and lies on the ground.
- **States:** wound (on the reel), flying, spinning, grounded (it lies on the ground), snagged, broken (the line snapped:
  lost), held (somebody holds it up for a long launch) and loose (it came free of a branch and flutters down).
- `AutoReel` is how a practised flyer works the reel: line out slowly at launch, then toward the length they want; a pull
  when it sags; hard in while it spins. The town and the tests use it.

### The five kinds

`LyudKite::ByShape` (`Diamond`, `Box`, `Delta`, `Stunt`, `Dragon`). `FKiteDef::FlyFrom` and `FlyLine` say what each wants.

| Kind | What it is | Least wind (10 m) | Flown on | Too much |
|---|---|---|---|---|
| Diamond | the plate above, a tail of bows | 3 m/s | 25 m | spins over 13 m/s |
| Box | two cells, heavier | 5 m/s | 25 m | spins over 18 m/s |
| Delta | a big light wing (0.95 m², 85 g) | 2.5 m/s (it holds in 2.2) | 30 m | spins over 9.5 m/s |
| Stunt | two lines, 0.75 m² | 4 m/s with steps back, about 5 standing | 22 m (30 at most) | the line holds 480 N |
| Dragon | a small face, 12 m of tail | 4.5 m/s | 22 m | spins over 21 m/s |

- **The delta** stands high (69° in 5 m/s, where the diamond stands at 56°). On a slack line it takes a glider's trim
  instead of tumbling (`Glide`), so a lull lets it sail on over the flyer's head.
- **The dragon** is draggy and steady: it hangs low on its line (37° in 7 m/s), hardly sways, and wants a long launch.
- **The stunt kite** has no will of its own. It flies along its nose; `FKiteInput::Steer` (-1 the left line pulled .. 1
  the right) turns the nose, the faster the faster it flies (`TurnRadius` 3 m, `TurnMax`); a keel force makes it go where
  the nose points.
  - **The window:** low in the middle of the wind it is fast and heavy (16 m/s and 70 N in a 7 m/s wind), at the edges
    and overhead it is slow and light and at last stalls (parked at the top: 0 m/s, 17 N).
  - **Twists:** every loop winds the lines round each other (`Twist`, turns). Three slide; after that each turn takes
    steering away (`SteerGain`: 38% left after seven loops). A loop the other way undoes one.
  - **The ground:** touched gently nose-up it stands on its tail edge (a landing) and waits, leaning back, for a pull.
    Anything else is a crash and it lies on its nose: one line held for a second rolls it over, and so does walking
    round it. A nose-first crash over `SparSpeed` (24 m/s) pops a spar: it won't fly until it is wound in.
  - `FKiteInput::Pull` (-1..1) is the arms: drawn back they take in up to 0.7 m of line fast (the pull that launches
    it), thrown forward they give as much slack (how it is set down).
  - `AutoSteer` flies it as somebody who knows how: lazy eights across the window, a loop now and then when there is
    height, out of a dive in good time, a crashed kite rolled back onto its tail. In a steady 7 m/s it never touches
    the ground in five minutes; in gusty, rough air it crashes now and then.

### Launches

- `Launch`: tossed from the hand on 3 m of line.
- `LongLaunch`: held up far downwind (`Held`), the whole line out. The holder lets go when the flyer pulls on a taut line
  (reel in, `Pull`, or steps back) or at `bRelease`. The first swing takes the kite up into stronger air, so it works in
  less wind than a toss (diamond 2.5 m/s instead of 3, dragon 4 instead of 5).
- `LayDown`: the same without a helper: stood on the sand nose-up, leaning back, until the pull. A stunt kite's own way.

### Tricks

`LyudKite::FTricks` watches a flight and `Track` names what was just finished (`ETrick`, `TrickName`):

| Trick | Kite | What |
|---|---|---|
| `high` | any | held steady over 25 m for 20 s (once a launch) |
| `save` | any | a spin, or a sag to the ground in a lull, flown out of |
| `loop_left`, `loop_right` | stunt | the nose all the way round |
| `eight` | stunt | two opposite loops in a row |
| `low_pass` | stunt | across the window under 2 m without touching |
| `landing` | stunt | set down softly nose-up at the window's edge |
| `dive_stop` | stunt | a dive from height pulled out under 3 m |

Ordinary flight (a diamond, a dragon, `AutoSteer`'s plain eights) names nothing.

### A caught kite got down

`LyudKite::FSnag` is a kite in a tree with its line hanging to the ground. `MakeSnag` gives it a hold of 0.7–1.5 by a
seed. `Tug` wears the hold down when the pull comes from downwind of the branch (`SnagSide`), so the pull and the wind
lift it off together: ten tugs in a steady wind for a hold of 1.2, three in a gust of 1.45. From across the wind it
takes several times as long. From upwind every tug drags it deeper (up to 2.5). `Free` lets it go: it rocks down like
a leaf (`Loose`) and lies.

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

| The other kinds | What the test shows |
|---|---|
| Delta, 2.2 m/s | flies (the diamond is on the ground) |
| Delta, 5 m/s | 69° over the flyer |
| Delta, 10 m/s | spins, and its line (150 N) snaps |
| Dragon, 7 m/s | steady at 37°, hardly swaying |
| Stunt, 7 m/s, `AutoSteer`, 5 min | mean 12.6 m/s, top 21 m/s; pull 60 N mean, 147 N top; 118 crossings, no crash |
| Stunt, 5.5 m/s, hard gusts, rough air, 18 min | 2 crashes, up 97% of the time |
| Stunt, a dive into the sand in 14 m/s | 26 m/s at the ground: a spar pops |

`run.sh fly 7 [box|delta|dragon|stunt]` traces one flight second by second; `run.sh trace N` prints the N-th stunt
flight of the suite four times a second.

## The hero's kite

`ULyudKiteFlying` (`Public/Kite/LyudKite.h`, `Private/Kite/LyudKite.cpp`) follows the fishing rod's pattern. A kite item taken
in the hand (Tab, "take in the hand") comes out on its reel: the hand holds it up in front, and it rides on the hand bone.

The items are `kite_diamond`, `kite_box`, `kite_delta`, `kite_stunt` and `kite_dragon`. A stunt kite comes out on two
handles, one in each hand (the left arm is not posed: the handle rides in the left fist where that arm hangs).

Single-line kites (diamond, box, delta, dragon):

| Key | While wound | While up |
|---|---|---|
| LMB | toss the kite up | held: pull the line in |
| RMB | stand it on the sand | held: let the line run out |
| Wheel | — | a metre out or in per click |
| E | near somebody out with the kites: ask them to hold it up | — |
| Q | put it away | wind it all in, then put it away |

The stunt kite:

| Key | While wound | While up |
|---|---|---|
| LMB | stand it on the sand | held: the left line (the nose turns left) |
| RMB | stand it on the sand | held: the right line |
| LMB + RMB | — | a pull on both (the launch) |
| E | ask somebody to hold it up | — |
| Q | put it away | wind it all in, then put it away |

- A crashed stunt kite (flat on its nose) is rolled over by holding one button for a second, or by walking round it. With
  a spar popped out it has to be wound in (Q).
- It is set down by walking toward it at the window's edge: the lines go slack and it settles on its tail.
- While a kite is out, RMB does not raise the binoculars.
- A screen pauses the input but the kite flies on.
- Going into the water winds it in. A jump of more than 30 m (a teleport, a ride) winds it in too.

**Launches:**
- **A toss** (single-line kites): from the hand on 3 m of line. Walking back into the wind as it goes up helps in a light
  wind: the hand's motion is in the model.
- **Off the sand:** the kite is stood 1.5 m downwind of the hero. He backs off into the wind and the line follows him with
  a little slack in it (up to the whole reel). A pull (LMB; a stunt kite: either button) sends it up. A stunt kite's
  only way; a single-line kite laid like this needs a good breeze.
- **A helper:** with the kite wound, anybody of the town within 4 m who is flying a kite or watching them (`fly_kite*`,
  `watch_kites`) can be asked with E (the prompt names them). They hold it over their head (`LongLaunch`), looking at
  the hero; he backs off and pulls; they let go. If they walk off first, the kite is back on the reel. While they hold
  it their own kite stays wound. Nobody walks anywhere for this: the people's life has no hook to send a person to a
  point, so it is the hero who makes the distance.
- A talk takes E first (`ANpcTalkDirector::UsePressed`), so somebody the hero can talk to has to be asked from a little
  to one side.

**The air in the prompt.** While a kite is out the prompt's second line reads the air 10 m over the hero
(`GetLocalWindAbove`), twice a second. The first that applies:

| Line | When |
|---|---|
| the lines are twisted, N turns | a stunt kite up with more than three twists |
| a gust (a lull) in N s | `GetNextGust` within 8 s |
| a wind shadow here | `Shelter` over 0.5 |
| rough air | `Turbulence` over 1.5 m/s |
| the air rises here | `Updraft` over 0.6 m/s |
| too light (too strong) for this kite | under 0.8 of the kind's `FlyFrom` (over its `SpinSpeed`) |
| Wind: <Beaufort name>, where from, an arrow | otherwise; the direction is told as the hero looks (eight ways) |

**Tricks:** a trick named by `FTricks` is said in the prompt (`ui.kite.trick.<name>`). Loops, an eight and a low pass
give stamps and quest events (below). No sound yet: the soundscape has no cue that fits, only the reel's click.
Somebody out with the kites within 15 m has a word for a loop or a low pass (bark events `kite_loop_cheer`,
`kite_low_pass`); a helper says `kite_hold_yes` taking the kite and `kite_hold_go` letting it go; a stunt flier's
demo ends with `kite_lesson`.

**Losing it:**
- It is caught when it flies into anything static that isn't the ground (a tree trunk, a lamp post, a wall, a roof). The
  test is a sphere sweep against WorldStatic, as wide as the kind but never wider than 0.4 of its height (a low pass
  doesn't scrape); the landscape is ground, and so is a floor it comes down on.
- It is also caught in a tree's crown: the crowns' ellipsoids from `FBirdPerches::InCrown`, since the leaves have no
  collision.
- A snapped line or a fall into the sea loses it for good.
- A caught kite stays where it hangs for ten minutes, or until the hero is 250 m away. The item is gone from the pocket
  until it is got down.

**Getting a caught kite down** (the hero's own, a child's, a test kite's):
- It hangs with its line to the ground a step downwind. Within 3 m of the line's end, with no kite of his own in the
  air, E takes the line; then every E is a tug (held: one every 0.85 s). Walking 9 m away lets go.
- The prompt says what to do: from upwind ("only drives it deeper"), from the side ("stand where the wind blows from the
  kite to me"), on the right side ("a gust in N s: that's the moment"), in a gust ("tug now").
- Freed, it flutters down and lies. The hero's own goes back into the pocket. A child's is theirs again (wound, up again
  in a little while) and they say thanks (bark event `kite_rescue_thanks`). Both give the stamp `rescue`.
- A child whose kite hangs in a tree looks up at it. If they go home first, the kite is nobody's and is gone in three
  minutes.

**Lines:** the hero's lines go through the prompt line (`ui.kite.*`): tossed, stood on the sand, held, flying, fell, the
wind dropped, crashed, a spar out, spinning, steady again, caught, snapped, in the sea, the tricks, the rescue. When
nothing has been said, the prompt shows what the keys do now and the air.

**Quest events** (`ULyudQuests::Notify`): `kite_flown` as before (20 s in the air), and `kite` with a target:

| Target | When |
|---|---|
| `flown` | 20 s in the air |
| `high` | 30 m over the hand |
| `loop` | a loop either way |
| `eight` | an eight |
| `low_pass` | a low pass |
| `landed` | a landing |
| `helper_launch` | 5 s up after somebody held it |
| `rescue` | a caught kite got down |

**Stamps:** the set "Воздушный змей", `kites/` in `tools/city/content/collections.json`. The engine gives its pieces
(`"by"`), so `interiors.py --check` doesn't look for a giver in town.

| Piece | Given for |
|---|---|
| `first_flight` | 20 s in the air |
| `high` | 30 m over the hand |
| `box` | the box kite up for 30 s |
| `storm` | a spinning kite brought back to steady flight |
| `loop` | a loop with the stunt kite |
| `eight` | an eight |
| `low_pass` | a low pass |
| `helper_launch` | a launch from somebody's hands, 5 s up |
| `rescue` | a caught kite got down |
| `lesson` | standing 4 s within 6 m of a townsperson flying a stunt kite: they fly a loop right and one back left |
| `delta_calm` | the delta up for a minute in under 3 m/s at the kite |
| `dragon` | the dragon up for a minute |

**The hum:** `ULyudKiteHumSynth` plays `LyudKiteHum::FVoice` (`KiteHum.h`) at the reel.
- An aeolian tone at `0.2 v / d` (a 2 mm line: about 800 Hz in 8 m/s) with an octave and a slow waver, over a soft thrum
  of the sail.
- Its level follows `LyudKite::Hum` (how taut, how much air). A slack line is silent.
- The reel ticks (`reel_click`) as it is wound.

**Meshes:** built in code once per shape and shared (`FLyudMeshKit`, kept rooted), coloured with shared rooted material
instances of the engine's basic material:
- a diamond 72 × 95 cm with cross spars, bowed back at the wing tips;
- a box kite with two cells on four longerons;
- a delta 156 cm across on a spine, two leading edges and a spreader, with a keel (the line ties on at its tip);
- a stunt kite: a swept wing 184 cm across in three panels a side;
- a dragon's face: a hoop 56 cm across in twelve wedges, with two eyes;
- the reel (a drum on a handle, with a knob), and a stunt kite's two padded handles;
- line pieces; a tail of nine bows that trail downwind with a wave running down them; the dragon's tail of forty ribbon
  pieces (12 m), narrowing to its end, with a long slow wave.

A stunt kite's two lines run from the two hands to 38 cm either side of its middle (32 pieces in one instanced
component).

Everything is out of Lumen and distance fields. Nothing is rebuilt: the parts are moved, and the line's 16 pieces (32)
and the tail's 9 (40) are re-placed in one batch each per frame. The colourways are our own (eight of them).

**The town's kites in the engine** (who flies what and when is "The townsfolk's kites" below):
- The kind comes from the activity: `fly_kite` a diamond (a box kite for one in three), `fly_kite_delta`,
  `fly_kite_stunt`, `fly_kite_dragon`. An activity missing from `activities.json` is simply never seen.
- Single-line kites are worked by `AutoReel`. A stunt kite is steered by `AutoSteer` from both hands, and a stunt kite
  and a dragon go up from the far end of their line (`LongLaunch`). A stunt kite that lies 4 s is wound up and set out
  again.
- A kite caught in a tree joins the caught kites (above) and can be got down for its owner.
- `lyud.Kite.Town` (12) still caps them all.

## The townsfolk's kites

Four activities of town life (`Content/PeopleData/activities.json`) put a kite in somebody's hands, and a fifth brings
people to look:

| Activity | Who | Wind, m/s | When | Where | How many |
|---|---|---|---|---|---|
| `fly_kite` | children 6–15 (a diamond; one in three a box kite) | 3–13 | 11:00–18:30 | the beach, the sunrise and sunset lookouts, the pier | anybody of that age |
| `fly_kite_stunt` | teens and the young, 12–40 | 4.5–12 | 12:00–19:00 | the beach and the lookouts (open ground: not the pier) | `regulars` 0.14: about one in seven of the active ones |
| `fly_kite_delta` | grown-ups and pensioners, 30 and over | 2–6 | 10:30–19:00 | the beach, the lookouts, the pier | `regulars` 0.08, more of those who love the open air |
| `fly_kite_dragon` | the few who own one, 16–70 | 4–10 | 12:00–18:30 | the beach only (the tail wants room) | `regulars` 0.012: half a dozen people in the town |
| `watch_kites` | anybody of 16 and over | 3.5–11 | 11:30–19:00 | the same places | anybody |

- **The wind:** `"wind": [min, max]` (`FActivity::WindMin/WindMax`, `FWorld::Wind`). Out of that range nobody chooses the
  activity; in the middle of it, it calls a little more. The game sets `FWorld::Wind` from
  `ULyudWindSubsystem::GetMeanSpeed`; `tools/peoplesim` sets it from the same calendar (`LyudWeather::Regime`, seed 921).
- **The rest is the life's own fields:** `daylight` (none in the dark), `temp` [5, 40] on the four new ones (in the frost
  only a few children are left), the rain rule (nobody chooses the open air in the rain), `day_more` on Saturday and
  Sunday (the weekend is the kite club's), `regulars` (the same handful every game: the stunt fliers, the delta
  pensioners, the dragons' owners are particular people, picked by their key and temper).
- **Parents and onlookers.** The life has no way to tie one activity to another's being on, so the onlookers are an
  activity of the same wind window at the same places: when the kites can fly, people come to look. A parent stands by
  a small child only for `playground` and `play_run` (`FWorld::ChildToMind`, in code); a kite is not one of those, so
  "parents" on a kite afternoon are onlookers of the right age (the lines `kite_parent`).
- **Losing one:** a child's kite caught in a tree stays there for the rest of their outing, unless the hero gets it
  down (the child thanks him: `kite_rescue_thanks`).

### Kite afternoons

What `tools/peoplesim/run.sh town` counts (the town's 949 people; a warm Saturday from 10:30, minute by minute between
noon and five, the wind held at a value):

| The day | Kites up at once | Of them | Watching |
|---|---|---|---|
| 7 m/s, 21 °C | 9.0 on average, 17 at most | children 3.4, stunt 4.8, dragon 0.8, no delta, no stunt kite on the pier | 8.6 on average, 14 at most |
| 3.5 m/s, 21 °C | 5.7 on average, 10 at most | deltas 5.6; no stunt kites, no dragons | |
| 1 m/s; 16 m/s | none | | none |
| 7 m/s in the rain | 0.0 on average, 1 at most | | |
| 7 m/s at −4 °C | 0.4 on average, 2 at most | children only | |

On the test's own Monday and Tuesday (19–20 October, 4.7–7.3 m/s, 12 °C) 36 hours hold 3,428 person-minutes of
children's kites, 1,612 of stunt kites, 314 of dragons, 108 of deltas and 1,052 of watching: about 14 kites at once on
the Monday afternoon after school, none in the dark, a calm or a gale. The town's other life gave up one to three per
cent of its outdoor time for it (`sit_bench` 94,825 → 94,368 minutes, `stroll` 75,761 → 74,826, `chat` 25,214 → 24,484,
`swim_look` 27,630 → 27,754, `sea_air` 22,505 → 22,474; `fly_kite` itself 3,871 → 3,428). The counts move by a kite or
two with any change to the life (it is a chaotic system), so the test asks for 6 to 16 on the breezy Saturday.

Kite Day (`kite_day`, the third Sunday of September, docs/festivals.md) is the yearly day: the show `kites` over the
beach, flags, bunting and the fair's stalls on the sand (decor only: a festival's stalls sell the day's dishes, not
items, so a kite stall on the beach needs code). The paper has three town stories of the season about the stunt kites,
the deltas and the dragon (`tools/paper/strings_src.py` `TOWN`).

**Town talk** (`tools/barks/lines/13_kites.txt`, built by `tools/barks/build_barks.py`):
- the children while they fly (`kite_kid`, `kite_kid_more`), a teen on box kites, a flier who sees the hero near;
- the stunt fliers (`kite_stunt`, `kite_stunt_hero`), the deltas of quiet days (`kite_delta`, `kite_delta_elder`), the
  dragons' owners (`kite_dragon`), all of them on the wind (`kite_wind_lore`: a gust seen on the water, a lull, the
  wind shadow under the cliff when it blows off the land) and in the cold months (`kite_cold`);
- whoever came to look (`kite_onlooker`, `kite_parent`, `kite_onlooker_elder`), and the grown-ups on the beach;
- at a moment the engine raises (`on = ...`, `ULyudKiteFlying::Bark`): `kite_hold_yes` and `kite_hold_go` (somebody
  holds the hero's kite for a long launch and lets it go), `kite_lesson` (a stunt flier has shown him a loop and its
  undoing), `kite_rescue_thanks` (a child whose kite he got down), `kite_loop_cheer`, `kite_low_pass`.

The barks know no wind, so what is said about it is said as a rule ("with the wind off the land it won't fly under the
cliff"), never as what the air does this minute.

## Shops

| Item | Shop | Price |
|---|---|---|
| `kite_diamond` | toy shop (P-WAVE, P-UNITE) | 150 ₽ |
| `kite_diamond` | the fair's toy kiosk | 160 ₽ |
| `kite_diamond` | the department store's sports counter | 150 ₽ |
| `kite_box` | toy shop (P-WAVE, P-UNITE) | 280 ₽ |
| `kite_box` | the department store's sports counter | 270 ₽ |
| `kite_delta` | toy shop (P-WAVE, P-UNITE) | 220 ₽ |
| `kite_delta` | the department store's sports counter | 220 ₽ |
| `kite_delta` | the fair's toy kiosk | 235 ₽ |
| `kite_stunt` | the department store's sports counter | 520 ₽ |
| `kite_stunt` | toy shop (P-WAVE, P-UNITE) | 540 ₽ |
| `kite_dragon` | toy shop (P-WAVE, P-UNITE), one in stock | 640 ₽ |

The department store's toy house is a board only, so its kites are at the sports counter. The fair's toy kiosk is the
only seasonal stall that sells them; a shop's stock cannot depend on the weather, so there is no kite stall on the
beach on kite afternoons. The items are in `LyudInventory.cpp` `MakeDefs` (the same rolled pack, a size and a colour
each); their words are written by `tools/kites/town.py`.

Icons: the diamond and the box kite are one 2×2 Higgsfield grid (`design/kites/icon_kites_grid.json`, done). The delta,
the stunt kite and the dragon are a second grid, **pinned and not generated** (`design/kites/icon_kites_grid_2.json`,
prompts in `localization/assets.json`): until it is run on the Mac `tools/check_localization.py` lists their three
pictures as missing and the pocket shows a grey square.

## Quests

Three side quests, one after another (`Content/UI/quests.json`; lines in `Content/Dialogue/npc_dialogue.txt`; words in
`Localization/quests/`, written by `tools/kites/town.py`; not voiced yet):

| Quest | Giver | Begins | Stages | Reward |
|---|---|---|---|---|
| `kite_lesson` "Первый урок" | Ivan Korneev (`Korneev_ivan`: 74, once the Laboratory's mechanic, has glued kites since he was a boy) | the hero gets any kite (`get kite_*`); Korneev's everyday talk also invites him | `meet` talk · `launch` `kite flown` · `climb` `kite high` · `land` `kite landed` · `tell` talk | a delta he sewed from an old raincoat (`kite_delta`); begins `kite_poplar` |
| `kite_poplar` "Змей на тополе" | Tyoma (`Kid_boy`, 11): the diamond Korneev glued for him has hung in a poplar behind Sun Square since spring | the lesson's last talk | `rescue` `kite rescue` · `return` talk | his toy car (`toy_car`), the one he never lends; begins `kite_eight` |
| `kite_eight` "Восьмёрка" | Stas (`Guy_a`, 19): one line is a stroll, flying takes two | Tyoma's last talk | `dare` talk (he gives his old `kite_stunt`) · `fly` `kite helper_launch`, `kite loop`, `kite eight`, `kite low_pass` in any order · `tell` talk | the Baikal he lost the bet for (`drink_baikal`); the kite stays |

- The lesson is what the flight asks for: read the wind off the sock by the lifeguard tower, back to the wind, toss and
  hold, line out a little at a time, wind in evenly. Korneev has a word for a slack sock, for a gale and for the dark.
- Stas has a line for a lost stunt kite (`if.no:kite_stunt`: the sports counter, 520 ₽).
- Two remarks (`Content/UI/quest_scenes.json`, each with its diary line): Nina Korneeva on her husband's spars while
  the lesson is on, Sonya on Tyoma's missing toy car afterwards.
- Developer mode: every stage has a `dev` block (the kite beach at 15:00 with the right kite in the pocket; the givers
  through `Quest.GoToPerson`); `kite_poplar` `rescue` lands at the teleport `kite_poplar`
  (`Content/Dev/teleports/kite_quests.json`: 12 m north of the third poplar of Sun Square's south row, facing it) and
  runs `lyud.Kite.Snag`, which hangs a caught kite 10 m ahead.
- **What the quests need of the engine** (`ULyudKiteFlying`): `kite landed` for a kite of any kind wound in whole after
  a flight (the lesson is flown on a diamond; a stunt kite's `landing` trick alone would leave it stuck), and, for the
  story, a caught kite hung in that poplar while `kite_poplar` is on `rescue` (without it any kite got down ends the
  stage).
- Checks: `tools/questsim/run.sh`, `python3 tools/check_quest_talk.py`, `python3 tools/quest_talk_reach.py --strict`,
  `python3 tools/check_devdata.py`.

## Console and developer mode

**Console commands:**
- `lyud.Kite.Give [diamond|box|delta|stunt|dragon]`: a kite in the pocket and in the hand.
- `lyud.Kite.Launch [toss|lay|long]`: toss it up; stand it on the sand; or have it held up 20 m downwind as a helper
  would (then pull: `lyud.Kite.Reel -1`, a stunt kite `lyud.Kite.Pull 1`). A stunt kite's toss is a lay.
- `lyud.Kite.Reel -1|0|1|off`: pull, hold or let out without the mouse; `off` gives the mouse back.
- `lyud.Kite.Steer -1..1|off`: a stunt kite's lines (-1 the left one); `off` gives the mouse back.
- `lyud.Kite.Pull 0|1`: a pull on both lines.
- `lyud.Kite.Snag`: the hero's flying kite is caught where it is. With none up, a caught kite hangs 10 m ahead.
- `lyud.Kite.Tug`: E on a caught kite's line (take it, then a tug each time).
- `lyud.Kite.Tricks`: the tricks flown with the kite in the hand, in the log (`KITE tricks ...`).
- `lyud.Kite.State`: the state in the log (`KITE ...`: the window, the air, steer, pull, twist).
- `lyud.Kite.Debug 1`: the state on screen.
- `lyud.Kite.Test N [kind]`: N kites in a row 25 m downwind of the camera, the five kinds in turn or one kind. The bench
  uses 10.
- `lyud.Kite.Town N`: at most N townsfolk's kites drawn.
- `lyud.Kite 0`: all kites off.

**Developer mode (F7):**
- group "Kites": give kites (all five); a stunt kite in the hand; a long launch; snag the kite (or hang a caught one
  ahead); ten kites downwind (the bench);
- teleport "Kite beach" (15:00).

## Tests

- `tools/kitesim/run.sh` checks:
  - the lift model: calm, breeze, gale, storm;
  - a snag;
  - the line's sag;
  - determinism;
  - each of the five kinds' least wind, and what makes each kind itself (the delta's light wind and glide, the dragon's
    low steady flight);
  - the stunt kite: steering both ways, the window's power, twists, crashes, the spar, the roll-over and relaunch,
    `AutoSteer` in steady and in rough air;
  - the air: turbulence, a lee, ridge lift;
  - launches: a toss, a run upwind and downwind, the long launch, a kite stood on the sand;
  - every trick, and that ordinary flight names none;
  - the snag: which side, a gust, a wrong pull, the fall when freed;
  - the hum DSP: silent when slack, louder and higher with air, no clipping.
- 113 checks in about 4 s with the build (`EXTRA_CXXFLAGS` takes extra warnings).
- The engine side (`LyudKite.cpp`: drawing, the controls, the prompt, the rescue, the town's kinds) has not been compiled
  or run yet.
- `tools/peoplesim/run.sh town` checks that the children fly kites by day in a breeze.
- Still to do on the Mac: shots of the hero's kite and the children's, and a bench with `lyud.Kite.Test 10` against main.
