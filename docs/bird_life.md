# Bird life

The wild birds of the outdoor map: about 2270 birds of 36 species (1200 of them starlings) that live where their species would,
keep their day by the game calendar, deal with each other and with hawks, and perceive the player. The
species' looks, sizes and animations are the species table's (`Content/BirdsData/species.json`,
`Public/Birds/BirdSpecies.h`); this is their behaviour.

## Files

| What | Where |
|---|---|
| Simulation, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/Birds/BirdSim.h/.cpp` |
| Each species' ecology and temper, its daily routine | `Private/Birds/BirdEcology.cpp` |
| The starlings' evening murmuration | `Private/Birds/BirdMurmur.h/.cpp` |
| The Academgorodok's power lines (poles, wires; the birds' wire perches) | `tools/academ_wires.py` -> `Content/BirdsData/wires.json`, `Private/World/LyudWires.cpp` |
| Game side: subsystem, drawing, API | `Public/Birds/BirdLife.h`, `Private/Birds/BirdLife.cpp` |
| Places birds can be, from the map | `tools/bird_habitats.py` -> `Content/BirdsData/bird_habitats.json`, `bird_world.bin` |
| Run it outside the engine, tests, maps | `tools/birdsim/run.sh day|test|trace`, `tools/birdsim/render_birds.py` |
| Offscreen play test | `unreal/scripts/test_birds.sh` (test_birds.py) |

Rebuild the habitats whenever `tools/world_map.py`, the terrain or the wires change: `python3 tools/academ_wires.py && python3 tools/bird_habitats.py`.

## How it works

**Places.** `bird_habitats.py` writes ~7700 sites of 13 kinds: sea, beach (wet sand), the clay cliff's face,
meadows and fields, reed, streets and yards, roof ridges and eaves (the Academgorodok's houses), the church tower, wires
(the cable car's ropes and the Academgorodok's power lines, each conductor in sagging pieces of up to 4 m), gardens,
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
wake time (thrushes and robins first, gulls late; the groups of a species leave the roost one after another over its
wake spread), a feeding peak in the morning, a rest at midday (with a bath at the water's edge and a long preen for
those that bathe), feeding again in the afternoon, a pre-roost gathering for the species that have one (rooks on a
field near the rookery, jackdaws on roofs, gulls on the beach, swallows on the wires), the last ~40 minutes on the
way to the roost, asleep after dark. Rooks, crows, jackdaws, gulls, starlings, wagtails and swallows sleep all
together at one communal roost (rooks in tall trees, gulls on the sea, starlings, wagtails and swallows in the reed,
up on the stems); the rest near home. Dawn song for the songbirds. Swifts, swallows and martins feed on the wing;
swifts sleep on the wing 400-700 m up; gulls soar now and then; cormorants dive, then stand drying their wings. The
tide (`ALyudSea::SeaLevel`, `FWorld::SeaZ`) moves gulls, crows, wagtails and starlings onto the sand it lays bare and
off it at high water; in winter (November-February) some species feed elsewhere (fieldfares and blackbirds on
berries in gardens, gulls in the town) and flock bigger, and blue tits roam with the great tits' flocks. Each
species' routine is in the table below.

**Starlings.** One species of the bird life like any other (ids, bodies near the camera, befriending, bird flight,
trust and saves, hawks), 1200 of them in late October in flocks of 20-80. They leave the reedbed behind the beach
(world_map `REEDBED`, the wettest runnels of the valley mouth, a `CommonReed` grass patch) 30 to 5 minutes before
sunrise, group after group, feed all day on the fields, the town's lawns and streets and the wrack on the beach, rest
on wires, roofs and trees, gather on wires and trees near the roost in the last half hour of the afternoon and fly to
it from 40 minutes before sunset. There they join one flock, the murmuration (`FMurmuration`, act `Murmur`): the
flocking of Dima's Murmuration demo (StarDisplay: 7 topological neighbours, separation, alignment, cohesion by
centrality, speed alignment, banked flight with lift and roll, 12% informed birds steering for a wandering goal,
turns started at the rim), flown in 1/25 s steps for every bird in it whatever its LOD. It wanders over the roost and
the land round it and may swing out over the sea along the coast, but never more than `FOptions::MurmurOffshore`
(250 m) from the shore (a firm turn back near 80% of it). The peregrine comes at dusk and stoops at the rim again and
again; each attack sends escape rolls neighbour to neighbour through the flock. From 4 minutes after sunset the birds
drop into the reed in waves (a bird follows its descending neighbours), are handed back to the bird life on the last
metres, chatter and fall asleep; by sunset + 30 minutes they are all down. `AStarlingMurmuration` is now only a
dev/test actor: in Lvl_Scene01 it stands down (`lyud.Starlings.Legacy 1` keeps it).

**Late swallows.** Barn swallows and sand martins have their months (April-September), but `species.json`
`stragglers` keeps a few on into the autumn (14 swallows leaving between 27 October and 5 November, 6 martins between
25 October and 2 November, each on its own day, out of sight). They hawk low over the meadows, the reed and the water,
sit in rows on the Academgorodok's wires (a body and a half apart, ~13 cm for a swallow, all facing the same way)
and sleep in the reed. Swifts keep their months (none in October). With `lyud.Birds.Migration 0` every species stays,
in full numbers.

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

## Daily routines

| Species | Wakes (min from sunrise) | Groups leave over | Feeds | Rests | Before the night | Roost (min from sunset) | Midday bath | Other | Winter |
|---|---|---|---|---|---|---|---|---|---|
| Rook | -25 | 20 min | fields, beach, street | crowns, wires, roofs | gathers on a field near the rookery, 50 min | rookery in tall trees (communal), +5 | rarely | mobs hawks | town refuse too, flocks ×1.5 |
| Hooded crow | -20 | 15 | beach (low tide), fields, streets | conifers, roofs, tower | tall trees, 30 min | conifers (communal), 0 | sometimes | steals food, mobs | ×1 |
| Jackdaw | -20 | 15 | fields, streets | roofs, tower | chatters on roofs and the tower, 30 min | church tower (communal), +10 | sometimes | steals, mobs | flocks ×1.4 |
| Magpie | -15 | 5 | fields, streets, gardens | gardens | — | gardens, -5 | yes | steals, mobs | — |
| Feral pigeon | 0 | 10 | streets | roofs, tower, wires | roofs, 15 min | tower, roofs, -5 | often | tame | — |
| Wood pigeon | -10 | 15 | fields, gardens, forest floor | crowns, wires | conifers, 15 min | conifers, -10 | sometimes | — | fields, flocks ×2 |
| Herring gull | -30 | 30 | sea, beach (tide), street | roofs, beach, sea | on the beach, 40 min | on the sea (communal), +20 | often | soars, steals | — |
| Black-headed gull | -30 | 30 | fields, beach (tide), sea, street | beach, sea, wires | on the beach, 40 min | on the sea (communal), +15 | often | soars, steals | the town's streets, flocks ×1.3 |
| Common gull | -30 | 30 | fields, beach (tide), sea | beach, sea, roofs | on the beach, 40 min | on the sea (communal), +15 | often | soars, steals | — |
| Cormorant | -10 | 20 | the sea (dives) | beach, cliff: stands drying its wings | on the beach, 20 min | beach (communal), 0 | — | — | — |
| Mute swan | -20 | 10 | sea, the reed's edge | beach, sea | — | on the water, +10 | sometimes | — | — |
| Great tit | -15 | 5 | gardens, crowns | gardens, crowns | — | broadleaf crowns, -20 | often | dawn song, mobs | gardens and feeders, flocks ×1.5 (the core of mixed flocks) |
| Blue tit | -15 | 5 | gardens, crowns, reed | gardens, reed | — | broadleaf crowns, -20 | often | mobs | roams with the great tits, ×1.5 |
| Goldfinch | -5 | 10 | fields (seed heads), gardens | crowns, wires | crowns and wires, 10 min | conifers, -20 | often | — | gardens, fields, reed; ×1.5 |
| Chaffinch | -15 | 10 | forest floor, fields | crowns | — | conifers, -20 | sometimes | dawn song | fields, flocks ×2 |
| Bullfinch | -5 | 10 | gardens, crowns | gardens | — | conifers, -20 | sometimes | — | berries |
| Waxwing | 0 | 10 | gardens (berries) | crowns, wires | crowns, wires, 10 min | conifers, -20 | rarely | — | gardens only, ×1.5 |
| White wagtail | 0 | 10 | beach (tide), streets, fields | roofs, wires | gathers on roofs, 20 min | the reed (communal), -10 | often | mobs | (migrant) |
| House sparrow | -10 | 5 | streets, gardens | gardens, roofs | gardens, 15 min | roofs, gardens, -20 | dust and water baths, often | — | flocks ×1.3 |
| Tree sparrow | -10 | 5 | fields, gardens | crowns, wires | gardens, 15 min | gardens, -20 | often | — | fields, ×1.5 |
| Starling | -30 | 25 | fields, town lawns and streets, beach wrack (tide) | wires, roofs, crowns | wires and trees near the roost, 35 min | **the murmuration** from -40, then the reedbed (communal), down by +30 | sometimes | mobs | — |
| Song thrush | -40 | 10 | forest floor, gardens | crowns | — | conifers, 0 | sometimes | dawn song | — |
| Fieldfare | -10 | 15 | fields, gardens | crowns | crowns, 15 min | conifers, -10 | rarely | mobs | berries in gardens, ×1.5 |
| Blackbird | -40 | 10 | gardens, forest floor | gardens, crowns | — | conifers, gardens, +15 | often | dawn and dusk song, mobs | gardens, forest floor |
| Robin | -45 | 5 | forest floor, gardens | crowns, gardens | — | crowns, +20 | often | dawn song, mobs, very tame | — |
| Common swift | +10 | 10 | hawks high over streets, fields, sea | tower, cliff | screams round the tower, 20 min | **sleeps on the wing** 400-700 m up | — | clings, never perches | (away) |
| Barn swallow | 0 | 10 | hawks low over fields, reed, water | wires, roofs | in rows on the wires, 40 min | the reed (communal), -10 | — | mobs | (away; October stragglers) |
| Sand martin | 0 | 10 | hawks over the sea, beach, reed | wires, cliff | on the wires, 30 min | the reed (communal), -10 | — | mobs | (away; October stragglers) |
| Sparrowhawk | 0 | — | hunts gardens, streets | conifers | — | conifers, -10 | — | hunts small birds | — |
| Peregrine | 0 | — | hunts over the town, beach, sea | tower, cliff | — | tower, cliff, +25 (hunts the murmuration at dusk) | — | stoops | — |

| White-tailed eagle | +10 | — | the sea, the beach, the reed (fish, waterbirds, carrion) | the cliff, conifers | — | conifers, -10 | — | soars on the cliff wind, steals from gulls | — |
| Golden eagle | +15 | — | fields, the cliff | the cliff, conifers | — | the cliff, -15 | — | soars wide and high (for the mountains to come) | — |
| Common buzzard | +20 | — | fields (voles, worms) | wires, posts, crowns | — | conifers, -20 | — | soars, sits long on a lookout | — |
| Raven | -10 | 5 | the beach, fields, the cliff | the cliff, conifers, tower | — | conifers, 0 | rarely | steals, mobs, plays in a gale | — |
| Grey heron | -20 | 10 | the reed, the sand at low tide | conifers, the reed | — | conifers (communal heronry), +10 | — | stalks the waterline | — |
| Black-browed albatross | 0 | — | the sea | on the sea | — | on the sea, +30 | — | a lost wanderer: sits on the sea in a calm, soars low over the waves in a blow | — |

## Wind

The bird life gets the weather's mean wind and gustiness (`ULyudWindSubsystem`) every frame (`FWorld::Wind`, `Gustiness`),
and every species its sprint and wind limit from `BirdBio.cpp` (docs/bird_flight.md "Flying qualities").

- **Flying birds move with the air.** A bird crabs so its track holds the course, flies faster into a headwind (up to its
  sprint) and rides a tailwind; the wind is weaker low down (over the ground, the sea or a wood's canopy). Gusts toss light
  birds about, hardly heavy ones. A bird that can't make way drops to just over the ground or the waves, and if that doesn't
  help it gives up the trip for the nearest shelter, rather downwind (`ShelterNear`).
- **A gale keeps the small birds in the lee.** Above about 0.6 of its wind limit a species leaves the open (fields, wires,
  roofs, the beach, the tower) for crowns, hedges, gardens, the reed and the streets, flies low, goes only short hops and
  sits tight longer. In a 17 m/s gale the share of small birds out in the open falls from about half to under a tenth.
- **The soarers want the wind.** Gulls, the eagles and the buzzard go up to circle more often the windier it is, and
  prefer the cliff, the beach and the sea; the albatross barely flies in a calm and soars low over the waves in a blow.
- Escapes are near the sprint, a hawk's chase at its sprint.

`tools/birdsim/run.sh test` checks it (BIRD_ONLY=wind for the gale alone; BIRD_WIND_ACTS=1 prints what everybody is doing).

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
`lyud.Birds.Migration` (1, the default: species away this month per species.json months are not created, so no
swifts, swallows or martins in October; 0 keeps everybody), `lyud.Birds.Bodies`, `lyud.Birds.DrawDistance`,
`lyud.Birds.Report`, `lyud.Birds.Scare [m]`, `lyud.Birds.DumpDay <hour> <hours> [csv]`, `lyud.Birds.AnyMap 1`,
`lyud.Birds.SpawnNear <Species> [x y z]` (a calm test bird that keeps to its spot, any level), `lyud.Birds.Time <h> [m]`,
`lyud.Birds.IgnorePlayer 1`, `lyud.Birds.TestRun <from x y z> <to x y> <cm/s> <s>` (tests). `lyud.Wires 0` leaves the
Academgorodok's power lines down (read at the start); `lyud.Academ 0` hides them with the ensemble.

Drawing without popping (the renderer, ABirdLifeRenderer::Draw): a bird shrinks to nothing over the last fifth of its
draw range (and is gone before 0.95 x FarDist, where the sim stops moving it) instead of being cut at ~1.2 px; a
murmurating bird's range eases between 1x and 8x over ~10 s; a bird with a body keeps it to 30 m and against newcomers
up to 5 m nearer (bodies are posed at once and the instance stays one more frame); late stragglers never leave from
the near ring. `lyud.Birds.Steady 0` brings back the old hard cuts for A/B; the report ends with `pops N, body spawns
N releases N` and the last pops with their reasons. Check: `STARTEST_ONLY=pop unreal/scripts/test_starlings.sh`
(walks past the swallows' wires at dusk, a feeding starling flock and the wires at noon, both ways; POPCHECK ok/FAIL).

After a fright a bird stays watchful for 45-90 s (its alertness fades over ~45 s instead of ~7) and its group keeps
away from the danger, or from the player wherever they are, for one to two minutes.

## Checks

- `tools/birdsim/run.sh test`: flight distances by species and gait, the befriending sequence (notice, turn,
  watch, approach, eat, no flush), sharp turns scare with a warning first, a runner makes a rook fly away, a
  pigeon flock goes up together; the routines: the starlings' murmuration (one flock, within the coastal strip,
  over its roost, escape waves from the peregrine, everybody asleep in the reed by dusk + 1 h, out in waves before
  sunrise, feeding on fields and in town by mid-morning), October's swallows (stragglers, hawking, rows on the wires
  no closer than 10 cm, asleep in the reed, gone by 6 November), gulls on the beach at low tide, rooks' pre-roost
  gathering, swifts asleep on the wing. `BIRD_ONLY=routines` (or `murm`) runs only those; the murmuration's cost
  is printed (~0.1 ms a frame for 1200 birds).
- `tools/birdsim/run.sh day && python3 tools/birdsim/render_birds.py`: `Saved/BirdSim/birds_day_map.png`
  (the map at eight hours, a dot per bird in its species' colour) and `birds_day_acts.png` (what they do
  over the day).
- `unreal/scripts/test_birds.sh`: the same in the game, offscreen, plus screenshots and a flush test.
