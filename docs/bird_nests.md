# Bird nests: the breeding year

Every species of the bird life that breeds on this coast (36 of 38: not the waxwing, not the albatross) has its own nest,
builds it in its season, lays, sits, feeds its chicks and leads the fledglings about. The season follows the game
calendar. The corvids' own minds (docs/corvids.md) sit on top of this: their builders carry real stick items.

## Files

| What | Where |
|---|---|
| Breeding table, nest placement, each nest's year, the parents' duties, fledglings (plain C++) | `Source/Lyudmilovsk/Private/Birds/BirdNest.h/.cpp` |
| Hooks in the bird life (one next to each corvid hook) | `Private/Birds/BirdSim.cpp` |
| Game side: drawing, putting nests on real branches and walls, API, `lyud.Birds.Nest` | `Public/Birds/BirdNests.h`, `Private/Birds/BirdNests.cpp`, `Private/Birds/BirdLife.cpp` |
| Nest, egg, box, burrow and bill-load meshes | `blender/birds/nests/build_nests.py` -> `export/nest_kit.glb` |
| Chick meshes (three ages, resting and begging, per species) | `blender/birds/chicks/build_chicks.py` -> `export/chick_kit.glb` |
| Import | `unreal/scripts/import_nests.sh` -> `/Game/Birds/Nests/SM_*`, `M_BirdKit`, `MI_NestKit`, `MI_ChickKit` |
| Tests outside the engine | `tools/birdsim/nests_test.cpp` (`BIRD_ONLY=nests tools/birdsim/run.sh test`) |
| Offscreen close-ups in the game | `unreal/scripts/test_nests.sh` (`NESTTEST_ONLY=Rook,Blackbird`) |

`tools/birdsim/run.sh test` also writes `Saved/BirdSim/nest_table.json`; copy it to `blender/birds/nests/nest_table.json`
when the table changes (the art scripts read it).

## How it works

**The table** (`BirdNest.cpp` Defs): nest kind (Cup, Dome, Platform, Cavity, Eave, MudCup, Burrow, Scrape, Mound),
material, where it nests (habitat tastes and height in a tree), size, colonies (rookery, heronry, gulleries, the sand
martins' cliff, pigeons, jackdaws, sparrows, starlings, fieldfares, swifts), laying window, clutch, incubation, nestling
and dependence periods, broods, who builds and who sits, precocial or not, whether the nest lasts and is used again,
daily nest survival, how often the young are fed, how hard the pair defends it. Laying dates are for the south-east Baltic:
ravens and herons in March, crows and magpies in April, tits at the end of April, most songbirds in May, swifts at the
end of May; feral pigeons all year round.

**Nests and pairs** (`FNesting::Setup`, at Populate): each group's males and females pair up, lone birds pair with the
nearest lone bird of the other sex; a species with a single bird on the map (peregrine, eagles) nests with an unseen mate.
Colonial species pick a colony site near home and fill it; the rest keep their spacing. Nests sit in tree crowns
(platforms near the trunk, cups out on side branches), boxes and holes on trunks, under eaves and on walls, in the clay
cliff, on the ground, in the reed. Reused nests (sticks, mud, boxes, burrows) were built in earlier years and stand as old
nests through the winter; a cup nest of this year stands until the next spring.

**Each nest's year** (`MakePlan`, `StateOn`) is a pure function of the nest, the year and the day: build, lay one egg a
day (two for the bigger birds), sit, hatch, nestlings, fledged, independent; lost on some day (Mayfield daily survival)
and then a repeat clutch if the season allows; second and third broods. Clock jumps, saves and LOD cost nothing: the
same date always gives the same stage. The player can change one thing: standing within a few metres of a nest with eggs
for four minutes makes the pair give it up (they lay again elsewhere in time).

**The parents near the camera live it** (`Think`, `Arrive`, duties): song from a post near the nest in the mornings and
evenings; building trips (to a place with the material, a few pecks, back with a beakful, weaving it in; the corvids
carry real sticks a neighbour may steal); the sitter on the eggs day and night with breaks (she sits, he brings her food;
pigeons, gulls and others take turns), sitting tight (the player can come three times nearer); brooding the small
chicks; feeding trips every few minutes for songbirds, every hour or two for herons and eagles; the young gape when a
parent lands. Alarm: at a nest with eggs or young the pair perches near the player and scolds; gulls, swans and
fieldfares dive at the player's head.

**Fledglings** are birds of the bird life from a pool of 192 (bGone when unused): a brood near the camera (within 320 m)
gets its young out round the nest when it fledges, and gives them back when the camera is far (600 m) or they are on their
own. For the first half of their dependence they sit near the nest and beg; then they go with the family. Downy cygnets
and gull chicks keep by their mother, swim and walk after her and sleep by her; they are drawn with the chick kit.
Fledglings are drawn smaller (85% growing to full size) and get no body actors.

**Drawing** (`ABirdNestRenderer`): the nest at its stage (B1, B2, Full, Old), boxes and holes all year, eggs and chicks
within ~20 m, the beakful a builder carries. Instances are rewritten only when what a nest shows changes (checked twice a
second); begging chicks and moving young are moved in place (no proxy rebuild), only near the camera; all of it outside
the Lumen scene. A nest is put on the real tree's branches (`FBirdPerches::ForEachBranch`), a wall or the ground when it
first comes within 160 m.

## Console

- `lyud.Birds.Nests 0/1`: no nests / the breeding year (read when the bird life starts; A/B in one process with a restart).
- `lyud.Birds.Nest <Species> [Build|Eggs|Chicks|Fledged] [0..1]`: the calendar to a day this year that species' nearest
  nest is in that stage, the player 3 m from it.
- `lyud.Birds.Report`: the nests by stage, the duties, the young out, and the nest drawing.

## Checks

- `BIRD_ONLY=nests tools/birdsim/run.sh test`: the table against species.json, nests for every species, rookeries,
  October (old nests, only pigeons breeding), stages in order, laying windows, determinism, a blackbird sitting and
  singing and feeding, rooks flying sticks, gulls diving, thrushes scolding and deserting, the cost (A/B in one process).
- `unreal/scripts/test_nests.sh`: close-ups of the nests in their season, in the game, offscreen.
