# Children's games in the yards

Interactives backlog item 32. After school the town's children play together in the yards in front of their houses, at the
playground, on the park lawn, on the square's north side and by the water on the beach: hopscotch on chalked squares, a
skipping rope, hide-and-seek, football with a ball that rolls, and tag. The hero can join a game of hide-and-seek, be found,
and seek in turn.

## Where they play

The city plan has no yard spots, so `tools/yard/yard_areas.py` finds them: in front of each house where children aged 5 to
13 live (homes in `townsfolk.json`), a clear disc of ground (clearance against plots, boxes, props, trees, water,
fountains, stairs, lamps and the garden site; terrace room checked against the walls' face lines). Each yard is furnished
so the game has something to hide behind: two park benches facing in, a bin, a sandpit, a flowerbed, four trees (maple,
birch, rowan, apple) and four bushes (privet, spiraea), all through the procedural plant system and the CityKit props.

Areas now: nine yards `S-YARD-01..09` (Unité, Serpentine, Wave, T3H2…), `S-PLAYGROUND`, `S-PARK-LAWN`, `S-SQUARE-NORTH`,
`S-BEACH-PLAY1`, `S-BEACH-PLAY2`. Each has the games it fits (a football field of 24 × 12 m needs room), the counting wall,
the chalk's place, the rope's place, the field, and cover points behind its objects facing away from the wall.

Generated files (rerun `python3 tools/yard/yard_areas.py`; `--check` writes nothing and lists stale files):

| File | What |
|---|---|
| `Content/PeopleData/yard_games.json` | the play areas for the game model |
| `Content/PeopleData/yard_spots.json` | people's-life spots: kind `yard` (8 stand slots, capacity 3) and `S-YARD-xx-BENCH` benches (0.45 m seats) |
| `Content/Localization/yard/strings_{ru,en,ja}.json` | from `tools/yard/strings.json`; `line.*` keys get `.m`/`.f` from `ru_f`/`ja_f` |
| `Content/Dev/teleports/yard.json` | dev teleports to every area, at 16:30 |

## How children get there

No new activity: the existing `playground` and `play_run` activities now list `yard` among their places
(`activities.json`, also `mind_child` so a parent can watch from the yard), and `PeopleSim.cpp` loads `yard_spots.json`
beside `winter_spots.json`. So the life still decides who is out playing and when; the yards are just more places to do
it, nearer home.

**Overlap with item 26** (branch `residents-activities`, thread «Научить жителей новым занятиям города»): that branch also
edits `activities.json` and `PeopleSim.cpp/.h` on an older main. Our change is three place lists in `activities.json`
and one load line in `PeopleSim::Load`; a merge keeps both sides. Item 26 does not touch `playground`, `play_run` or the
children's hours, and `tools/town/town_life.py` is never rerun (our data comes from an add-on script).

## The games (`Private/People/YardGames.h/.cpp`, plain C++, namespace `LyudYard`)

`FYard::Offer(area, kids)` is called every second with the children the life has at play near an area (doing within its
catch radius, or passing near its middle; children already in that area's game count from further away so they are
never offered to another area). When enough are free and the area's cooldown (60–150 s after the last game) has passed, a
game starts. The kind is weighted by the children's ages and the area's fit; two children prefer hopscotch. The first
child offered is the leader and calls the game (`line.yard.start.<game>`).

| Game | Players | Rules |
|---|---|---|
| Hopscotch | 2–5 | 7 rows of 0.5 m cells on the chalk; one hops at a time (a turn rotates through `Round`), a miss (≈3.5 % a hop) ends the turn with «на черту наступил!» |
| Rope | 3–7 | two turners at the rope's ends (1.7 m half span, 1.05 Hz), jumpers queue by when they arrived; a jumper who trips swaps with the more tired turner |
| Hide-and-seek | 3–8 | the seeker counts at the wall 18–24 s («Раз, два, три…»), the hiders run to cover points; the seeker searches covers, spots a hider within 1.9 m (6 m in the open); hiders may dash home; 100 s to seek, then the round ends, a 5 s break, and the first found seeks next |
| Football | 4–10 | snake-draft teams, keepers from 6 players; the ball has friction, bounces off the field's edge and scores between the goal bags (1.5 m half width); passes, dribbles and shots from 8 m or nearer |
| Tag | 3–8 | «It» aims past the prey, runs at 1.02 × and switches prey after 7 s; breath runs down and comes back; a tag within 0.9 m freezes both for 2 s, the tagged one is It, the old It is immune for 3 s |

A game lasts its length (minutes) unless the children's life calls them away: a child who leaves the area or whose
activity changes leaves the game (`Leave`); a game below its minimum ends (`End`, «Меня домой зовут!»). Each child has a
role and an act (stand, walk, run, hop, jump, turn, count, crouch, clap, kick, reach, cheer, point, look); the engine
turns acts into poses and gestures. Events (`Start`, `Join`, `Leave`, `Roles`, `Found`, `Saved`, `Tagged`, `Goal`, `Miss`,
`Swap`, `RoundEnd`, `End`) drive the shouts and the hero's toasts.

## The hero

In hide-and-seek only:

- Within the area (radius + 8 m) a prompt «E — попроситься в прятки». E joins: as a hider while the seeker counts
  («Тебя взяли в прятки…»), or from the next round. The hero hides anywhere; the seeker looks at the hero's real position
  and sees the hero if near enough or in the open. Found: «Тебя нашли!»; never found: «этот кон твой!».
- When the hero is first found, the hero seeks next: count at the wall (15 s, counter on screen), then «E — Туки-туки!»
  near a hider finds them. Time out: the rest come out and somebody else (or the hero, if nobody was found) seeks.
- Walking away (catch radius + 12 m) leaves the game.

## Engine side (`Public/People/LyudYardGames.h`, `Private/People/LyudYardGames.cpp`)

`ULyudYardGames`, a tickable world subsystem (Game and PIE only):

- Loads `yard_games.json` once the people's life runs, then builds `ALyudYardThings`: the kit props as instanced CityKit
  meshes (static, `M_CityProp`), chalk strips as one ISM, trees through `AProceduralForest::AddTree`, and bushes as
  `AProceduralPlant`s planted one a tick within 200 m of the camera.
- Every second offers children (`OfferChildren`), every tick steps the model, handles events (shouts through
  `ULyudTownTalkSubsystem::SayLine` with `.m`/`.f` by the child's sex; hero toasts), updates the hero and presents.
- Present draws up to 3 games within 150 m of the camera: their children get an `ANpcBody` taken over from the life
  (`SetPresentedExternally`), driven with a cached ground trace, a lift for hops and jumps, and gestures on the hand-goal
  layer (`SetGesture`: counting hands on the face at the wall, clapping, pointing, the turners' circling hands, reaching
  for a tag). A child whose game is no longer drawn walks back for up to 4 s and is handed back.
- Props: the rope is a pool of 36 cylinder segments along the turning arc, the ball one sphere per football game drawn
  (spun by its speed), goal bags as 12 small meshes. Moving parts are out of distance fields and Lumen; nothing instanced is
  rebuilt per frame.

Developer mode (F7 › Actions, group «Дворовые игры»): hide-and-seek by the hero, football by the hero, three games by the
hero (the bench), the hero seeks, end every game, report to the log. Console: `lyud.Yard.Start [kind] [players]`,
`lyud.Yard.Three`, `lyud.Yard.Report`. Teleports: group `yard`. Forced games bring the nearest children whatever they were
doing and keep them until the game ends.

## Tests

`tools/yardsim/run.sh` (see `docs/testing.md`): the generated data up to date, each game's rules scripted (start, roles,
turns, misses, swaps, found, tags, goals, leaving, end), the hero hiding and seeking, then a Monday and a Saturday
afternoon of the real town's children (13:00–20:30) on the real people's life: the yards are people's places and children
go out to them, games start, roles hold, nobody is in two games, every game that started ends. Last run: Monday 138 games of 4 kinds, Saturday 87 of
5 kinds, 58 and 40 children using yard places, 0 failures.

## Owed on the Mac

Compile, shots of three yards with games (hide-and-seek counting, rope, football), and the bench: `lyud.Yard.Three` by
the playground, frame time and memory against main, alone (`docs/perf.md` checklist).
