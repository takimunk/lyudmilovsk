# Bird life

The wild birds of the outdoor map: about 1100 birds of 29 Baltic species that live where their species would,
keep their day by the game calendar, deal with each other and with hawks, and perceive the player. The
species' looks, sizes and animations are the species table's (`Content/BirdsData/species.json`,
`Public/Birds/BirdSpecies.h`); this is their behaviour.

## Files

| What | Where |
|---|---|
| Simulation, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/Birds/BirdSim.h/.cpp` |
| Each species' ecology and temper | `Private/Birds/BirdEcology.cpp` |
| Game side: subsystem, drawing, API | `Public/Birds/BirdLife.h`, `Private/Birds/BirdLife.cpp` |
| Places birds can be, from the map | `tools/bird_habitats.py` -> `Content/BirdsData/bird_habitats.json`, `bird_world.bin` |
| Run it outside the engine, tests, maps | `tools/birdsim/run.sh day|test|trace`, `tools/birdsim/render_birds.py` |
| Offscreen play test | `unreal/scripts/test_birds.sh` (test_birds.py) |

Rebuild the habitats whenever `tools/world_map.py` or the terrain changes: `python3 tools/bird_habitats.py`.

## How it works

**Places.** `bird_habitats.py` writes ~7500 sites of 13 kinds: sea, beach (wet sand), the clay cliff's face,
meadows and fields, reed, streets and yards, roof ridges, the church tower, the cable car's ropes, gardens,
broadleaf and conifer crowns, the forest floor. A site is a point or a segment (a ridge, a rope) with a patch
round it. In the game the guessed tree sites are replaced by the real crowns of the forests and hero plants
(`AProceduralForest::ForEachTree`, `PlantModel::Summary`), so birds sit in real trees. An 8 m grid of the ground
and of the tallest thing on it (crowns, roofs) keeps flights over obstacles.

**Groups.** Every species lives in groups (rook flocks of 8-24, pairs of crows, single robins and thrushes) with a
home spread out from the other groups of its species (best-of-10 farthest picks), so birds are all over the map
and a fifth of the small birds' groups live near the club's door. A group picks its next site by its species'
taste for the kind (feeding, resting and roosting tastes differ), distance from home, how crowded it is,
whether bigger birds hold it and whether it was disturbed lately.

**The day.** From sunrise and sunset (ALyudSky's ephemeris at the game's date): asleep before each species'
wake time (thrushes and robins first, gulls late), a feeding peak in the morning, a rest at midday, feeding
again in the afternoon, the last ~40 minutes on the way to the roost, asleep after dark. Rooks, crows,
jackdaws, gulls, starlings and swallows sleep all together at one communal roost (rooks in tall trees, gulls
on the sea, swallows in the reed); the rest near home. Dawn song for the songbirds. Swifts, swallows and
martins feed on the wing; gulls soar now and then; cormorants dive.

**Each other.** A bigger bird walking up makes a smaller one give way; gulls and crows chase a bird that found
something big; a sparrowhawk (2) and a peregrine (1) hunt now and then: everything small near the attack
flushes into cover, and tits, crows, fieldfares, blackbirds and swallows mob the hawk on its perch until it
leaves. When one bird flushes in fright its whole group goes up (a wave over ~0.1-0.5 s) and other birds near
it are startled.

**The player.** Each bird sees the player within its sight range and field of view (with a line-of-sight
trace near the camera) and hears footsteps (running 25 m, walking 10 m, crouching 2.5 m). The danger is
`FleeDistance × threat`, where 1 is a person walking straight at the bird looking at it: running, walking at
it, staring at it, sharp view turns (> 100°/s) and fast hand moves raise it, crouching lowers it (never below
0.35: a rook isn't fooled by a crouch), keeping still makes it fade (habituation over ~8 s), and trust shrinks
it. The bird goes through stages the player can read: **notice** (its head comes round), **turn**,
**watch** (it stops feeding; uneasy or curious), **call**, a nervous step away when the player is almost too
close, then **flee** away from the player (never past them). Flight distances when walked at:
house sparrow ~4 m, town pigeon ~2 m, robin ~3 m, great tit ~5 m, gulls ~20 m, rook and crow ~28 m.

**Befriending.** While the player holds food out (`SetOffer`), stays low and still and the bird has noticed
them, its trust grows at its species' tameness (robins and tits fast, pigeons and sparrows soon, rooks
hardly). Once trust passes 0.55, the player has kept still a while and it is hungry, the boldest such bird
flies to the palm (`OnBirdApproach`), lands on it and pecks (`OnBirdEat` at the first peck). Moving the hand
fast, a sharp view turn or standing up and walking flushes it. Trust is per bird and saved.

## LOD and cost

Birds within 40 m of the camera think every frame (perception with line of sight, each other, the hand),
within 160 m every 0.12 s, within 500 m every 0.5 s and move in 0.1 s steps, beyond that every 2 s in 0.5 s
steps. Positions in between are extrapolated for drawing. Drawn: instanced static poses (one component per
species and pose), each bird only within 700 of its own lengths (a sparrow within 105 m, a gull within 420 m),
kept at least ~1.2 px. The nearest 12 birds within 25 m become the species thread's animated `ABirdActor`
bodies when the species has a skeletal mesh. Outside the engine a day of 1083 birds costs 0.014 ms a step
(camera at the door).

## API (for befriending and bird flight)

`UBirdLifeSubsystem::Get(World)`: `SetOffer(Palm, Food, bOn)` every frame while food is held out;
`GetBird(Id, Info)` (species, location, velocity, head direction, activity, alertness, trust, body actor);
`FindBirdsNear(Location, Radius)`; `ClaimBird(Id)` / `SetClaimedBirdLocation` / `ReleaseBird(Id)` for flying
as a bird (other birds keep seeing it, and don't fear the player while they are a bird); `Scare`,
`AddPlayerThreat`; delegates `OnBirdNotice/Turn/Watch/Call/Approach/Eat/Flee(int32 BirdId)`.

## Console

`lyud.Birds 0|1` (off removes them at once; for A/B in one benchmark process), `lyud.Birds.Population`,
`lyud.Birds.Migration 1` (species away this month per species.json are not created; off by default, so
swifts and swallows are there in October), `lyud.Birds.Bodies`, `lyud.Birds.DrawDistance`,
`lyud.Birds.Report`, `lyud.Birds.Scare [m]`, `lyud.Birds.DumpDay <hour> <hours> [csv]`, `lyud.Birds.AnyMap 1`.

## Checks

- `tools/birdsim/run.sh test`: flight distances by species and gait, the befriending sequence (notice, turn,
  watch, approach, eat, no flush), sharp turns scare with a warning first, a runner makes a rook fly away, a
  pigeon flock goes up together.
- `tools/birdsim/run.sh day && python3 tools/birdsim/render_birds.py`: `Saved/BirdSim/birds_day_map.png`
  (the map at eight hours, a dot per bird in its species' colour) and `birds_day_acts.png` (what they do
  over the day).
- `unreal/scripts/test_birds.sh`: the same in the game, offscreen, plus screenshots and a flush test.
