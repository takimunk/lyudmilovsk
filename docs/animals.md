# Cats and dogs

The town's cats and dogs: about 44 cats round the houses (a third of them round the Academgorodok) and 19 dogs (four
packs with their dens, yard dogs, the dog by the club's door). They have needs, a day, homes and ranges, they meet each
other and the birds, and they mind the player. The hero can make friends with them: a hand to sniff, stroking, treats,
a dog that walks along, a cat on the lap ("Friends" below).

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
| With people: household dogs and cats, the lady's round, hedgehogs; yard cats and the townsfolk | `Private/Animals/AnimalPetsSim.cpp`, `Public/Animals/LyudPets.h`, `tools/town/town_pets.py` -> `Content/AnimalsData/pets.json`, `blender/animals/hedgehog.py` |

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

**The dogs' heads (2026-10-29, Dima: the muzzle bent out of shape when the head turned).** `fit()` set the head bones by
the back's height, and the generated dogs hold their heads ~10 cm lower than the templates: the jaw pivot and the lip
line sat in the skull, so the jaw dragged the whole face. `gen/<Breed>/head.json` holds the real lip line (corner,
front), read by hand off a side close-up; `gen_fit.fit_head` moves the head bones onto it and the nose tip, and the lip
line is the mouth's (the lips' split, the cut). The skull is rigid (head and jaw take a vertex wholly where they hold most
of it), the lips are cut apart and shut at rest, the mouth parts (dark inside, teeth, tongue) are tucked under the skin,
and no fur shells grow in front of the eyes (the coarse copy's plates stood off the snout). The raw models of Laika,
Shepherd and Mutt are lost (the hi3d links expired): `unreal/scripts/export_animal_source.py` exports the body, first coat
and normal map back out of Unreal into `gen/<Breed>/ue/` (local), and `--gen` builds from that when `model.glb` is
missing (the normal map is reused, not baked). `ANIMTEST_PART=dogfaces` shoots each dog's head standing and barking.


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

## Friends

The hero can befriend any of them (the brief: `docs/interactives_backlog.md` item 1). The animals' side is in the pure
simulation, `Private/Animals/AnimalFriendSim.cpp` (`FWorld::OfferHand`, `Stroke`, `OfferFood`, `StartWalk`, `EndWalk`;
`FAnimal::Friend` is what a befriended or befriending animal is doing with the player, `EFriend`). The hero's side is
`UAnimalFriendSubsystem` (`Public/Animals/AnimalFriends.h`): keys, his hand, the treats, the name, quests, the diary.

**The hand.** Crouched (C) within 2.6 m, E holds a hand out low. An animal that trusts him enough comes and puts its
nose to the palm (`SniffHand`, event `HandSniff`); a cat needs trust `0.34 - 0.26 * Friendly`, a dog
`0.16 - 0.17 * Friendly` (Maru comes at once), half that for food; a friend always comes. One that does not come watches
the hand (`HandRefuse`) and trusts a little more for the patience, so calm company and a second try work. A shout or a
rush at one that is not yet a friend ends it and costs trust.

**Stroking.** E again once it has sniffed (it remembers for 45 s; a friend needs no sniff): one stroke is 3.4 s
(`Petted`, `Stroked`). The hero's hand lies on its head and slides back along the neck (`UHeroReachComponent::
SetOverrideHold`, the palm put on the head bone every frame); the animal leans into the hand (`FAnimal::Lean`, a roll
of the body in `AAnimalLifeRenderer::Drive`), a dog's tail goes (`Excite`), a cat purrs every 5 s. The bond grows while
the hand is on it (cat 0.022/s, dog 0.03/s, more for a friendly one). A cat that trusts the hand may roll over, a dog
that is a good friend too (`RollOver`, `Roll`; a stroke then is a belly rub). A cat has had enough after three strokes
(`Enough`: it walks a few steps off and washes, and will not be stroked for a couple of minutes); a dog never has.

**Treats.** F holds out the best thing in the pocket for that animal's kind, on the open palm (the item itself lies on
it), from up to 6 m, crouched or not. The table is `TreatTable` in `AnimalFriends.cpp`: fish for cats (smoked and dried
fish, the diner's and the restaurant's fish, every `fish_<species>` caught on the rod; milk and cheese less), meat for
dogs (sandwich, cheburek, pelmeni, steak, shashlik, burger; cheese and pies less; bread a little). It comes, sniffs,
eats it off the palm (`Eat`, `Ate`: the item leaves the pocket then) or leaves what its kind does not eat (`HandRefuse`:
a dog does not eat chocolate). A treat grows the bond by up to 0.19 (less on a full stomach). F stays the birds' key
while bird feed is in the hand.

**A friend** is an animal with `Bond >= 0.5` (`FWorld::FriendBond`, `UAnimalLifeSubsystem::IsFriend`; event
`Befriended`, once). Stroking alone takes Maru four or five strokes, a cat two or three meetings; a liked treat is
worth several strokes. The bond, the trust and the name are saved with the animal (`UAnimalLifeSubsystem::Saved`); the
diary's record (when, where, how, counts) is `ULyudAnimalFriends` (saved as "AnimalFriends").

**A friend dog walks along.** E (standing) calls it: it walks at his left heel at his pace (`Follow`, gait by his
speed, up to a run), sits when he stands and lies down on a long stop, and catches up from afar: it rejoins behind him
out of sight when he is suddenly far away (a cable car, a train) or was out of reach for 5 s. One dog at a time. At a
door it may not go through (the walk grid's blocked cells: every building) it sits down at the last walkable spot he
stood on, lies down after a while (`WaitDoor`), and is up at his heel when he comes out. In the water it paddles
beside him with its back at the surface (`Swim`) up to 30 m from where it went in, then swims back and waits on the
shore, whining once. When he sits on a bench it sits beside it. At half past ten in the evening it goes home by itself
and will not set off again before six. E on it again sends it home (`WalkEnd`).

**A friend cat never follows.** It comes when he sits on a bench within its range (30 m at most), jumps onto his lap
and sleeps there, purring (`Lap`, `LapUp`); F strokes it there; it hops down when he gets up. One cat at a time.

**The name.** Maru keeps hers. A nameless friend gets one in the diary: its card's "give a name" link shuts the diary
and opens a small box to type it in (`BeginNaming`, at most 18 letters); the name tag over its head shows it from then
on.

**The diary.** The friends' pages come after the birds' in the Birds section (K): a card each with the portrait
(`/Game/UI/Diary/Friends/T_Friend_<Breed>`, optional: the card is words alone without it), the name, what it is, the
day and place they became friends and how (stroked, or a treat), and the strokes, treats and walks so far.

**Quests and people.** The subsystem sends `animal_sniffed`, `animal_stroked`, `animal_fed`, `animal_friend`,
`animal_walk`, `animal_home`, `animal_lap`, `animal_named` with the target `Maru`, `Cat` or `Dog`, and
`animal_walk_reach <place>` for every map place the hero comes to with the dog at heel. The quest "Maru's walk"
(`maru_walk`) begins when Maru sniffs his hand: stroke her, make friends, call her along to the upper cable car
station, send her home. A resident near the hero and the dog says something about her ("Твоя собака?": town talk event
`dog_walk`, `tools/barks/lines/13_animal_friends.txt`), at most once in a couple of minutes.

**Clips.** The animations thread owns the animals' clips. The friendship acts use what there is (`SniffHand`: Sniff,
`Petted`: Sit, `RollOver`: Rest turned onto its side by the renderer, `Lap`: Sleep, `Swim`: the walk and trot gaits)
and take a clip of their own name as soon as one exists (`AAnimalActor::ClipFor` looks for SniffHand, Petted, RollOver).

**Console and developer mode.** `lyud.Pets.Do hand|stroke|feed|walk|home [id]`, `lyud.Pets.Befriend [id|maru]`,
`lyud.Pets.Name <id> [name]`, `lyud.Pets.Friends`, `lyud.Pets 0` (the player's side off). F7, group "Friends":
Maru a friend at once, the nearest animal a friend at once, treats in the pocket, Maru's walk on and off; teleports to
Maru and to the club's door.

**Cost.** Nothing is built or allocated per frame: the target is picked by a pass over the 63 animals, the prompt line
is rebuilt only when it changes, the pocket is looked through once a second while an animal is looked at. A step of the
simulation with a friend at heel costs the same as without (0.002 ms per animal in `tools/animalsim`).

## With people

Backlog item 46. Until it the cats and dogs lived on their own and no resident owned one. `tools/town/town_pets.py`
(run by `tools/town/town_life.py` after the other add-ons, idempotent, `--check`) ties them and writes
`Content/AnimalsData/pets.json`; the behaviour is in the pure model (`AnimalPetsSim.cpp`, `FAnimal::Pet`), the game
side is `ULyudPets`.

| What | How |
|---|---|
| **Ten households with a dog** | The owner has two duties, `walk_dog` 7:00 to 7:33 and `walk_dog_late` 19:00 to 19:36 (two activities: a person's duty lasts till the end of the last duty of the same activity, so one id would run the morning into the evening; `"on_time": 1` ends them at the duty's hour however long the way there took). `ULyudPets` reads the owner's activity every tick and calls `FWorld::OwnerWalk(Dog, bOut, Feet, Vel, Hand)`. On the leash (`EPet::Leash`) the dog keeps at the owner's left heel, stops to sniff (the owner's wander pauses are when it gets to), greets another dog that comes within reach (both sniff, tails going, then are led on), sits when the owner stands long. `LeashHold` keeps it within `LeashLen` (2.4 m) and, when the straight line to the owner is not walkable (a corner taken), brings it to the nearest point of the owner's trail that sees the owner: the leash is never stretched through a wall. Home again, the dog is `Indoors` (not drawn, not in the hash). |
| **The leash** | One engine cylinder stretched from the owner's `hand_l` to the dog's collar height, no shadow, no collision, out of Lumen, moved only while a dog is out and near. |
| **Fourteen flats with a cat** | Flats with a west window from `city_plan.json` (the window's place 0.14 m out of the wall is the sill). By the clock: `Sill` by day (it sits and watches, turns its head after birds and the hero), `Door` at feeding time (7:30 to 8:15 and 18:00 to 18:45: it waits at the entrance and goes in with whoever comes), else `Indoors`. A sill cat cannot be reached; a door cat can be stroked like any cat. |
| **The lady with the bowls** | Nina Korneeva (`pets.json` `lady`) has duties `feed_cats` 7:54 to 8:21 and `feed_cats_late` 18:24 to 18:51 at `S-CAT-FEEDERS`. With her in the town `FWorld::bFeedByPerson` is set and the feeders no longer fill by themselves at eight: `ULyudPets::TickLady` calls `PutFood` when she stands by one and she calls the cats (`cat_call`). In `tools/animalsim` the old clock still fills them. |
| **Hedgehogs** | Six spots (the yards, the park's orchard corner). Out from April to October, from half an hour before sunset for four and a half hours (`FWorld::HedgehogsOut`, `Month`): they shuffle about their spot, stop and sniff, curl up when the hero or a dog comes within 2.6 m (4.5 m when he runs) and uncurl a while after he has gone. A cat that sees one comes up, sniffs, gets the spines (a `Hiss` event) and trots off. Drawn as two static meshes swapped by state, `SM_Prop_hedgehog` and `SM_Prop_hedgehog_ball` (`blender/animals/hedgehog.py`: about 3 000 triangles each, no UV overlap, checked by the script; `unreal/scripts/import_props.sh hedgehog hedgehog_ball`). |
| **Words** | `tools/barks/lines/16_pets.txt`: owners talk to the dog on the walk, about it to the hero, the lady calls the cats, people remark on a sill cat and on a hedgehog (events `pet_talk`, `cat_call`, `cat_sill`, `hedgehog`). The pets' names are `animal.pet.<id>.name` in `Localization/pets/`. |
| **"Можно погладить?"** | A dog on the leash takes the hero's hand only after its owner said yes: the prompt is `ui.pets.ask`, E asks (`ULyudPets::AskToStroke`), the owner answers (`pet_stroke_yes` / `pet_stroke_no`, one owner in four says no that day), a yes is `FWorld::AllowStroke(Dog)` for 40 s and the stroke is item 1's. A household's pet never follows the hero (`StartWalk` refuses). |

Console: `lyud.Pets.Report`, `lyud.Pets.Walk [n]` (sends an owner out with the dog now), `lyud.Pets.Hedgehogs` (forces
the hedgehogs' month to July; they still wait for dusk).

Not done or not seen yet: the engine side (`LyudPets.cpp`, the prompt in `AnimalFriends.cpp`) was written in the cloud
and has not been compiled or run; the sill's offset and the leash's hand and collar offsets want a look in the level;
pets are added to the model after the strays, so a saved bond with one is not restored; there is one feeder in
`animal_places.json` (the Academgorodok's), the lady has one stop.

## Town cats

A polish round on top of "With people" (2026-11-21): the yard cats (nobody's) have doings with the townsfolk,
`FAnimal::Do` (`ECatDo`) in `AnimalPetsSim.cpp`, driven by `ULyudPets::TickTownCats` four times a second from
`pets.json` (`friends`, `stalls`, `fishmongers`, `shoo`, `shooers`, all written by `tools/town/town_pets.py`).

| What | How |
|---|---|
| **Sees a resident home** | Twelve residents of 55 and over without a pet, one a building, are each liked by the free yard cat nearest their door (`SetFavourite`). When one walks homeward within 60 m of the door, `FavouriteHome(Cat, Feet, Vel, Door)` is called: a cat that is awake, free and within 30 m falls in behind them (`Tail` event, a meow), keeps 2.2 m back at their pace, sits and looks up when they stop. They go in: it walks up to the door, sits there 18 to 38 s meowing now and then (`SeeIn`), and is its own cat again; not twice within two hours. The resident talks to it (`cat_friend`). |
| **The fish stalls** | The market's two fishmongers' places are `FWorld::Stalls`, open while a fishmonger stands there. Every 20 s an open stall with fewer than two cats calls the nearest free, awake, hungry (over 0.35) and bold (over 0.35) cat within 90 m: it walks there, sits on the buyers' side (`front` m from the seller), looks up and meows (`Beg`, `BegStall` event). Every 50 to 120 s the seller throws a scrap (`ThrowScrap`: the cat eats there, is fed, and keeps away for three hours; line `cat_scrap`), or less often shoos it (`cat_shoo`). Seven minutes of nothing and it gives up for an hour; the stall shutting sends it off. |
| **The broom** | The two cleaners and the night watchman at their work (`shooers`, activities `shoo`) call `Shoo(P, 2.2)`: a cat within reach on their side of the wall wakes, trots 14 m off to walkable ground, sits and looks back for five seconds (`Shooed` event). Not a fright: no tree, no hiss, no flight. A cat shooed from a stall keeps off it for ten minutes. |
| **The sill by the weather** | `FWorld::Weather` (rain, air temperature, sun = clear sky by day, set from `ULyudWeatherSubsystem`): a flat's cat is on its sill only in the dry, and under 8 °C only when the sun is on the glass and it is not freezing; otherwise indoors. A warm sunny sill is slept on, a grey day's is watched from. |

A fright, the hero's hand or a dog ends any of these at once (the cat's own reflexes come first). `lyud.Pets.Report`
counts them. Not seen in the engine yet: the stall's `front` (1.9 m) and its yaw (the fishmonger slot's) want a look
at the market.

## Maru's moments

The hero's dog (`FWorld::HeroDog`, set by `ULyudPets` to the dog named `animal.maru.name`) has moments of her own with
him once she is his friend (`FAnimal::Moment`, `EMoment`, in `AnimalPetsSim.cpp`; she is not on a walk with him then):

| Moment | When | What |
|---|---|---|
| Waits up | half an hour after sunset till one in the morning, he is more than 60 m from her door | goes to the door, sits, later lies awake looking the way he went, whines now and then (`WaitLate`); he comes within 25 m: she runs to him barking and greets him (`Welcome`); once an evening |
| A gift | by day, dry, he is within 12 m and not running, bond 0.7 and over, once in 20 hours | trots 7 m off, sniffs about, carries a thing back (`FAnimal::Carry`) and lays it at his feet (`Gift`, Other = `FWorld::EGift`): a stick in spring, a shell in summer, a cone in autumn, a mitten in winter, an old ball every fifth day |
| Comfort | a thunderstorm with him within 25 m, or he has sat or crouched still for 40 s | comes to his side, sits pressed against him, then lies with her head up (`Comfort`, `Lean` 1) |
| Shelter | rain and he is not near | lies under the porch at her door till it stops or he comes (`Shelter`) |
| Her spot | by day, dry, he is away | `FavouriteSpot`: the warm hatch in winter, a sunny wall in spring and autumn, a bench or lookout in summer; she walks there and lies down |

Each writes a line into the journal (`FWorld::MomentNotes`, keys `journal.maru.*`, each kind once a day). A fright,
his hand held out or a walk begun ends a moment at once. Checked by `ANIM_ONLY=pets tools/animalsim/run.sh test`.
Not there yet: the thing she carries is not drawn and is not an item, thunder is taken from a downpour
(`PrecipRate` over 4), and none of the engine side has been compiled.

## Checks

- `tools/animalsim/run.sh test`: a dog chases a cat up a tree and barks, the cat comes down later; a bold cat without a
  tree hisses and the dog stops (they never touch); a cat stalks, crouches and pounces at a sparrow; a runner frightens
  a cat; a friendly dog comes up to a player standing still and begs; a trusting cat rubs a crouching player's legs;
  two pack dogs play (a bow, roles swapping); the real map: animals on walkable ground, a third of the cats at the
  Academgorodok, paths across the town. Friends (`ANIM_ONLY=friends` runs these alone): a cat comes to the hand, is
  stroked, has enough, and a fish makes it a friend; a shy cat keeps off, a dog leaves a sweet and eats a sausage, a
  shout ends it; Maru is stroked into a friend, walks at heel at the hero's pace, sits and lies down when he stops,
  keeps up with a run, rejoins from 300 m; she waits outside a door and lies down there, goes home at half past ten;
  she swims after him and turns back at 30 m; she sits by his bench and a befriended cat sleeps on his lap; on the
  real map she walks 119 m along the spur at heel and goes back to the club's door.
  With people (`ANIM_ONLY=pets`): an owner walks a block and round a corner and the dog keeps within the leash with
  a clear line to him at every step, sniffs, greets another walked dog, goes indoors after; a flat's cat is on the
  sill by day, at the door at feeding time, indoors at night; the feeders stay empty till the lady puts the food
  down; hedgehogs come out at dusk in July, curl up by the hero, a cat sniffs one and backs off, none in December.
  Town cats (in the same run): a cat falls in behind its resident once, keeps within 6 m round the house's corner,
  never underfoot or through the wall, sits at the door when they go in, then goes its way; a shut stall draws
  nobody, an open one the hungry bold cat (not the fed one, not the shy one), a scrap feeds it and it stays away, the
  stall shutting ends the begging; a sleeping cat by the broom trots 15 m off without a fright and looks back, a
  swept counter stays clear; the sill is kept at a mild dry noon and in cold sun, left in rain, grey cold and frost,
  and slept on more in the sun.
- `tools/peoplesim/run.sh town`: the ten owners are out with the dog morning and evening (they set off about a
  quarter of an hour ahead and are back on time; a holiday on the square thins the evening's walk), never at other hours.
- `tools/animalsim/run.sh day && python3 tools/animalsim/render_animals.py`: `Saved/AnimalSim/animals_day_map.png`
  and `animals_day_acts.txt` (what they do, hour by hour); `run.sh trace <id> <hour> <hours>` follows one animal.
- `tools/birdsim/run.sh test`: birds flush from a running dog well before a walking cat, a stalking cat gets closest.
- `unreal/scripts/test_animals.sh`: offscreen, the breeds and coats in a row, faces, poses, a cat and a laika meeting.

## Eyes

Maru is kept as hi3d made her (2026-10-29, Dima: "far from the original"): anatomy `gen_lods` [28000, 12000,
4000, 1200] (+`face_tris` 12000 on LOD0) and no fur shells over her painted coat (rig.json `fur` false -> the body MI's
SkinDark 0.85 instead of the shells' 0.72).

Maru (2026-10-29, after "no lids, scary"): her generated head's own painted eyes (amber iris, pupil, dark lid rims)
are kept: no eyeball is set over them (anatomy.py `painted_eyes`), the face keeps ~9k triangles in the decimation
(`face_tris`, build_animal.decimate_face; an even collapse left the head a few hundred and the eyes sat in faceted
pits), the face fur is short and the lids bare further out (`eye_bare`), the strands finer (`strand_scale`).
M_AnimalBody is subsurface with a near-neutral scatter (soft Virtual Shadow Map penumbra on the coarse generated heads;
a skin-red scatter turned her white lids pink). Check: `ANIMTEST_PART=eyes unreal/scripts/test_animals.sh`.
Her painted eyes look about: build_animal exports `eyes_at` (middle, radius, normal per eye) -> animal_bodies.json
`painted_eyes`; AAnimalActor gives the body a dynamic M_AnimalBody whose EyeShift node reads the texture shifted inside
each eye (mask in PreSkinnedPosition, the texture step for a world-space shift from screen derivatives), toward what
she looks at, up to a quarter of the eye's radius, with small saccades. The fur shells grow from a face-dense proxy
(decimate_face 9000/4000; LOD2 as proxy grew them in flat angular plates over the head). Lids bare `eye_bare` 1.8,
shells cut 1.15 r round each eye (wider left angular bare patches).
Runtime eyeballs (AAnimalActor::UpdateEyes, `blender/animals/gen/<Breed>/eyes.json` -> animal_bodies.json "eyeballs")
remain for a breed without good painted eyes; Maru no longer has one.
