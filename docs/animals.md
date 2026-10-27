# Cats and dogs

The town's cats and dogs: about 44 cats round the houses (a third of them round the Academgorodok) and 19 dogs (four
packs with their dens, yard dogs, the dog by the club's door). They have needs, a day, homes and ranges, they meet each
other and the birds, and they mind the player. Taming a dog (walking with the hero, giving it a name) is not built yet;
the hooks for it are listed at the end.

## Files

| What | Where |
|---|---|
| Simulation, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/Animals/AnimalSim.h/.cpp` |
| Game side: subsystem, drawing, API | `Public/Animals/AnimalLife.h`, `Private/Animals/AnimalLife.cpp` |
| A body up close (furred mesh, clips by activity) | `Public/Animals/AnimalActor.h`, `AnimalAnimInstance.h` |
| Where they can walk and what they go to | `tools/animal_map.py` -> `Content/AnimalsData/animal_world.bin`, `animal_places.json` |
| Run it outside the engine, tests, maps | `tools/animalsim/run.sh test|day|trace`, `tools/animalsim/render_animals.py` |
| Models: anatomy, coats, gaits, the build | `blender/animals/anatomy.py`, `coats.py`, `gaits.py`, `build_animal.py` |
| Import to Unreal | `unreal/scripts/import_animals.sh` (`import_animals.py`) -> `/Game/Animals/<Breed>`, `Content/AnimalsData/animal_bodies.json` |
| Offscreen checks | `unreal/scripts/make_animals_test_level.py`, `test_animals.sh` (`test_animals.py`) |
| Birds fleeing them | `Private/Birds/BirdSim.cpp` `PerceiveBeasts`, `UBirdLifeSubsystem::SetBeasts` |
| Maru: her collar, her name, E | `blender/animals/collar.py`, `Public/Animals/AnimalCollar.h`, `AnimalNameTags.h` |

Rebuild the walk map whenever `tools/world_map.py`, the terrain or the Academgorodok's layout changes:
`python3 tools/animal_map.py`.

## How it works

**Where they live.** `animal_map.py` rasterises the lived-in part of the map (the town, the Academgorodok, the valley,
its beach and promenade, the spur in front of the club) onto a 2 m grid: blocked (houses, water, ground steeper than
39 degrees), grass, paved (streets, the square, paths), yards round the houses, sand, forest floor, and a cover bit (the
bases of walls). It writes ~1300 places: a doorstep at every house and stair, cellar windows of the blocks, the
Gastronom's yard door and bins, feeders (a lady who feeds the cats morning and evening), heating hatches, benches,
sunny south walls, dog dens on empty lots and copse edges, the beach, the club's door, the stations, and refuges: trees
a cat can go up (in the game the town's real trees near where cats live replace the guessed ones). The spur is an
island for animals (its sides are too steep): the club's dog lives there.

**Who they are.** Cats get a home (a doorstep, a cellar, a feeder), a range (females ~45 m, toms ~90 m), a coat (tabby,
ginger, black, black and white, blue-grey, tortoiseshell for females only, white with tabby) and a temper (bold,
friendly to people, sociable, a hunter, playful, lively); two mothers have two half-grown kittens each that keep near
them. Dogs live in packs (the Gastronom's yard, the town square with a pup, the beach and promenade, the town's north
edge) with a den and a leader, or alone (the club's dog, the friendliest of all, the lower station's, two yard dogs).
Neighbours know each other: packmates and kittens fully, cats and dogs of the same yards well enough to leave each other
alone, strangers not at all; meeting peacefully slowly makes them acquainted.

**Needs and the day.** Hunger (a cat ~9 h to hungry, a dog ~11 h), tiredness (cats sleep in naps, ~14 h a day with
resting; dogs ~12 h, most of it at night), play (kittens and pups a lot), company (dogs miss their pack), novelty (they
roam, sniff, look at things). Cats are out at dawn and dusk and about at night; dogs in the mornings and evenings.
Every few seconds (more often near the camera) an animal weighs its options: sleep (where it sleeps: cellar, hatch,
doorstep, a sunny wall by day; the pack at its den or on a warm hatch at night), eat (the feeders at 8:00 and 18:30,
the shop's scraps at 9:30, 14:00 and 20:00, bins now and then, the beach; cats come to wait before the bowl is filled),
bask, roam, visit a friend, play, hunt (cats), rest. Pack dogs follow their leader's errand. Paths: A* on an 8 m grid
(cats prefer yards and the bases of walls, dogs streets and paths), pulled straight, steered round obstacles on the 2 m
grid, with local avoidance of each other and the player's feet.

**Meeting each other.** Packmates greet (nose to nose) and play: a play bow, then they chase each other round, the
chaser and the chased swapping. Strangers stare; close up the one less sure of itself (bolder, nearer its home) gives
way, or there's a hiss (cats) or a bark and a growl (dogs). A dog that doesn't know a cat may go for it: the cat runs
for the nearest tree away from the dog (or a cellar window) and climbs it; the dog barks under the tree and leaves; the
cat comes down when the dog is gone and washes. With no tree near, a bold cat stands its ground side on, back arched,
and hisses: a bold dog barks back from a distance, the rest give way. Nobody gets hurt.

**Birds.** Cats stalk birds on the ground (low and slow, freezing when watched), crouch with the rear wiggling, and
pounce; the bird always gets away, and the cat washes as if it didn't care. Dogs run at flocks of pigeons, gulls on the
beach, rooks on a field, to see them go up. Bird life sees them as it sees the player, by how they move: a running dog
puts birds up from 5-10 m, a walking cat from 3-4 m, a stalking cat only at about a metre; birds up a tree or on a roof
hardly mind a dog but tits, blackbirds and robins scold a cat they can see.

**The player.** Each animal sees (a field of view, a line of sight up close, a glance round now and then) and hears the
player (a runner from 22 m, a walker from 8 m), notices them, looks at them, and keeps out of reach by how they move: a
runner coming at a shy cat makes it run from ~10 m, a calm walker from 2-3 m, someone crouching hardly at all. Standing
still near them they get used to the player; the friendly and curious ones come up slowly: a cat that trusts them rubs
round their legs and purrs, others sit near and meow; a hungry dog sits and begs, whining; others sniff and wag. Unfriendly
bold dogs bark at a stranger by their den or at night. Trust grows while the player is calm near an animal and falls
when they frighten it; it is kept with the save.

## Looks

**Since 2026-10-25 the bodies are generated** (Dima's ask): hi3d multi-view pictures and v3.0 models, the requests and
outputs pinned in `blender/animals/gen/<Breed>/gen.json` and `model.json` (the raw 2M-triangle `model.glb` stays local).
`build_animal.py <Breed> --gen` cleans the model (welds, cuts fins, fills holes), turns and scales it into the breed's
frame and fits the breed's skeleton onto it (`gen_fit.py`: the paws, the leg columns, the nose, the tail along its own
centre line), decimates it to 14k (LODs 12k with shells, 1.8k, 0.7k from a 400k copy), bakes that copy's detail into a
normal map, keeps the generated texture as the first coat and paints the others carrying its fine detail, and grows 6
fur shells from the 1.8k copy. The clips, the simulation and the sculpted fallback (without `--gen`) are unchanged.


Bodies are sculpted from signed-distance primitives over each breed's skeleton (a lofted trunk, a thigh mass, the
shank, paws with four toes; the cat's round head with whisker pads; a dog's head as a lofted wedge; eyes in sockets,
ears with a hollow, a mouth slit so the jaw can open), meshed with OpenVDB and decimated to four LODs sharing one UV
layout. Coats are painted in 3-d per texel (tabby stripes round the body, bars on the legs, rings on the tail, agouti
ticking, white spotting from the belly up, a laika's pale mask and saddle, a shepherd's black saddle). Fur is shells:
12-15 copies of the body pushed out along the normals and combed back and down, each shell's height in the vertex
colour; `M_AnimalFur` shows a texel of the tiling strand map where its strand reaches the shell (two-sided, masked,
roots darker). LODs: shells (12-15) up close, half as many, 3, none.

Clips (in place, 30 fps; locomotion clips are one stride, played at speed / their stride speed): Idle, Walk, Trot, Run
(a rotary gallop for dogs, a half-bound for cats), Watch, Sniff, Eat, Sit, Rest (a cat's loaf, a dog's sphinx), Sleep
(curled on the side, nose to tail), Groom (a cat washing, a dog scratching), and for cats Stalk, Crouch, Pounce, Hiss,
Climb, Perch, Rub; for dogs Bark, PlayBow, Play, Beg. On top, `UAnimalAnimInstance` turns the head to what the animal
looks at, opens the jaw (bark, meow, hiss, panting dogs), lays the ears back in fear and wags the tail (a dog's by how
friendly and excited it is, a cat's lashing when worked up).

## LOD and cost

Animals within 50 m of the camera think every 0.15 s and see with line-of-sight traces, within 160 m every 0.5 s, within
450 m every 2 s moving in 0.25 s steps, beyond that every 5 s in 1 s steps. Outside the engine a day of the 63 animals
costs 0.008 ms a step. Drawn: the nearest 20 (lyud.Animals.Bodies) within 110 m (cats) or 176 m (dogs) as AAnimalActor
bodies from a reused pool; shadows only within 50 m; no instanced far animals (they're a few pixels).

Measured 2026-10-25 offscreen at 1600x900 Epic on the Academgorodok's square at 15:40 (20 bodies drawn, 10 animals in the
near tier), one process, 12 s each: frame 23.99 ms with the animals, 23.75 ms without (lyud.Animals 0), game thread 6.32 vs
6.25 ms. The benchmark route (tools/bench.sh 3, passes on/off, a busy Mac): 51.57 vs 51.21 ms, within the noise.

## API

`UAnimalLifeSubsystem::Get(World)`: `GetAnimal(Id, Info)` (breed, coat, place, what it does and why, needs, fear, trust,
bond, name, its body actor), `FindAnimalsNear`, `Scare`, `SpawnAnimal(Breed, Coat, Location)` (tests, any level),
`SetAnimalName`, `Report`, `Describe`; `OnAnimalEvent(AnimalId, Event)` with Notice, Watch, Approach, Flee, Bark, Growl,
Whine, Meow, Purr, Hiss, Pounce, ChaseStart, Climb, Greet, PlayStart (the sounds are not made yet: every event is there
for them).

## Console

`lyud.Animals 0|1` (off removes them at once; for A/B in one benchmark process), `lyud.Animals.Population`,
`lyud.Animals.AnyMap 1`, `lyud.Animals.Bodies`, `lyud.Animals.DrawDistance`, `lyud.Animals.IgnorePlayer 1`,
`lyud.Animals.Birds 0|1`, `lyud.Animals.Report [id]`, `lyud.Animals.SpawnNear <Breed> [coat] [x y z]`,
`lyud.Animals.DumpDay [hour hours csv]`, `lyud.Animals.Scare [m]`; `Animal.Spawn <Breed> [coat] [activity] [x y z [yaw]]`
(a body alone, for looking at the models).

## Maru, the club's dog

Dima's own dog (2026-10-26; his sheet and the photo of her collar in `blender/animals/gen/Maru/ref/`). She is the
club's dog on the spur (dog breed 3, `Maru`, female, the friendliest of all): a slim cream laika mix with folded ear
tips and a plume tail curled over her back. Her body is a hi3d model (`gen/Maru/gen.json`: the views were drawn by hi3d
from a description of the sheet, since the sheet itself can't be uploaded) on the laika's skeleton, scaled
(`anatomy.py` `_derive("Laika", "Maru", ...)`), with all the dog clips; one coat, the generated one (`cream`).
Her eyes: a generated head's eyes are not where the template's guess puts them (Maru's were 4 cm further back and 3 cm
lower), so `gen_fit.find_eyes` finds them in the generated texture (the dark irises on the face, each side on its
own); the fur is bare there, the coarse shells over them are cut away, and glossy eyeballs (hazel `EYES["Maru"]`)
sit in them, their fronts a little proud of the skin. Every `--gen` breed gets this when its eyes can be found.

**Her collar** (`collar.py`, run after `build_animal.py Maru --gen`): a 4 mm braided cord, dark red and black, laid
round her neck along the skin measured by rays (pressed 1.2 cm into the fur), drawn together under the throat by a
knot; from the knot a steel split ring, and on it a polished brass bone tag, 32 x 18 mm, with «Мару» engraved
(`label.maru.tag`; the font is Blender's Inter, the letters cut with an exact boolean). Two meshes:
`SM_Maru_Collar` follows the neck bone; `SM_Maru_Tag` is a pendulum (`UAnimalCollarComponent`, created by
`AAnimalActor` for a breed with `collar` in `animal_bodies.json`): gravity and the neck's own movement swing it,
it rests and knocks on her chest (a sphere under the throat), and a knock or a jerk hard enough clinks
(`collar_jingle`, louder for a harder knock, not beyond 15 m). 240 Hz substeps, nothing worked out when she isn't on
screen.

**Her name and E** (`AAnimalNameTags`, spawned with the animal life): every animal with a name (her `NameKey`
`animal.maru.name`, or one the player gives) has it over her head while she's in sight within 35 m, in the people's
label style; within 4.5 m and looked at it offers `ui.animal.call`, and E speaks to her:
`UAnimalLifeSubsystem::Hail` -> `FWorld::Hail`: she stops, turns to the player, barks back gladly (one `BarkHappy`
event: `dog_bark_happy`, two or three barks, the jaw snapping with each), wags, and then usually comes up to them.
Console `lyud.Animals.Hail [id]`.

Build and import: `Blender -b --python blender/animals/build_animal.py -- Maru --gen`, then
`Blender -b --python blender/animals/collar.py -- Maru [--preview]`, then `unreal/scripts/import_animals.sh Maru`
(it imports the collar when `export/Maru/Maru_Collar.glb` is there). Checks: `tools/animalsim/run.sh test` (she
lives by the club's door, turns and barks once when called), `ANIMTEST_PART=maru unreal/scripts/test_animals.sh`.

## Taming, later

What is there for it: `FAnimal::Bond` and `Name` (saved with the trust in `UAnimalLifeSubsystem::Saved`), the act
`Follow` and goal `Follow` (walk gait, a slow pace: nothing sets them yet), `SetAnimalName`, trust growing with calm
company, `FPlayer::bOffer` (food held out, as for the birds). The club's dog on the spur is the obvious first friend
(friendliness 0.92, home by the door).

## Checks

- `tools/animalsim/run.sh test`: a dog chases a cat up a tree and barks, the cat comes down later; a bold cat without a
  tree hisses and the dog stops (they never touch); a cat stalks, crouches and pounces at a sparrow; a runner frightens
  a cat; a friendly dog comes up to a player standing still and begs; a trusting cat rubs a crouching player's legs;
  two pack dogs play (a bow, roles swapping); the real map: animals on walkable ground, a third of the cats at the
  Academgorodok, paths across the town.
- `tools/animalsim/run.sh day && python3 tools/animalsim/render_animals.py`: `Saved/AnimalSim/animals_day_map.png`
  and `animals_day_acts.txt` (what they do, hour by hour); `run.sh trace <id> <hour> <hours>` follows one animal.
- `tools/birdsim/run.sh test`: birds flush from a running dog well before a walking cat, a stalking cat gets closest.
- `unreal/scripts/test_animals.sh`: offscreen, the breeds and coats in a row, faces, poses, a cat and a laika meeting.

## Eyes (runtime eyeballs)

A breed with `blender/animals/gen/<Breed>/eyes.json` (Maru) gets its eyeballs at run time (`AAnimalActor::UpdateEyes`):
two engine spheres with `M_HumanEyeball` (the people's eye material: a dark white and a big brown iris here) on the head
bone, where the generated head's own eyes are (`at`, `look`, `radius`, `push` forward along `look`, set by hand on
close-ups), and the eyeballs `build_animal.py` joins into the body are hidden. The iris turns toward what the animal
looks at (`SetLookTarget`, the same target as the head), at most 35 deg off, with small saccades. `import_animals.py
bodies` copies the file into `animal_bodies.json`. Check: `ANIMTEST_PART=eyes unreal/scripts/test_animals.sh` (front,
3/4 and each eye at a narrow field of view, then the eyes turned with `lyud.Animals.EyeFix/EyeYaw`); tune live with
`lyud.Animals.EyePush` / `lyud.Animals.EyeScale` (a new body). The joined eyeballs had been set 0.62 r into a pit of the
hi3d head and read as small, dull, half-hidden dots in daylight (2026-10-27).
