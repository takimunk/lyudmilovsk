# The bicycle rounds

Backlog item 17. There are two small jobs, ridden through the town's addresses on the six beach terraces: the morning
paper round from kiosk No. 1 and the post round for the old postman. Both sit beside the small jobs (item 4,
`docs/city_interiors.md` "Small jobs"): the "Help wanted" boards carry a sheet about them (`city.text.jobs.rounds`), and
their pay goes to the same purse (`ULyudInventory::Earn`).

## Playing

**The paper round.** From 6:00 to 7:30 the morning's bundle waits on kiosk 1's step (E takes it), once a day. One terrace is
served each day, in turn T1, T2, T3, T4, counted from the day the game began. The bundle holds a paper for each of its
12 subscribers plus 4 spare. Every waiting door is circled in red on the diary's map, joined by a faint line in riding
order. The left mouse button throws a paper, on foot or from the bike. The throw carries the hero's speed, so a paper
thrown while riding flies on ahead.

The paper is judged where it lands, against the nearest waiting door within 7 m:

| Where it lands | Result |
| --- | --- |
| Within 1.3 m of the door's step | On the step |
| Within 2.8 m | Near it |
| Farther, or on another storey (more than 2.2 m above or below) | In the road |

A landing farther than 7 m from every waiting door is a stray paper and counts for nobody. The round ends when every door
has its paper or the bag is empty. If neither happens, it ends an hour after the 8:00 deadline.

**The post round.** Tuesday to Saturday, from 9:00 to 12:00, the old postman gives out the day's 5 parcels at his door.
He is a retired postman: forty years with the bag, and his knees are gone now. At each flat on the round, E rings the bell.

After a short wait, whoever lives there opens the door:

- **How long the wait is.** The nervy are at the door at once, the calm take their time, and a sleeper takes longest
  (`WaitSeconds`).
- **What they say.** One of five sets of lines, by the person's manner (`warmth`, `nerve`) and whether the bell woke them:
  sleepy, nervy, warm, reserved or usual (`AnswerSet`). They take the parcel, and the hero has met them (`NoteMet`).
- **When nobody is in.** If the person is not at home (not within 25 m of the door, or hidden), a notice goes in the box.

**Pay.**

| Round | Per stop | Bonus |
| --- | --- | --- |
| Paper | 4 ₽ a paper on the step, 3 ₽ near it | 15 ₽ for the whole round by the deadline with three in four papers well placed |
| Post | 10 ₽ a parcel handed over, 2 ₽ a notice | 10 ₽ for the whole round in time |

**Stamps and the quest.** The collection «Разнос» has one piece for each terrace (`rounds/t1`..`t4`), earned by a paper
round with three in four papers well placed, and a fifth piece (`rounds/post`) for a post round done to the end. The side
quest `rounds` starts at the first bundle and has three stages: one paper round to the end, one post round to the end, and
a good round on every terrace (reward 60 ₽ and a postcard).

## Data and code

- `tools/rounds/rounds.py` writes `Content/City/rounds.json` from `city_plan.json` (the wall flats' `door_ue`, the kiosk's
  front) and `townsfolk.json`. Run it again after a plan or residents change; `--check` fails when the file is stale.
  Nothing in it names a resident by hand:
  - **The paper routes.** 12 doors spread along the stretch Y 405–780 m of each terrace, in riding order from the kiosk.
  - **The post flats.** The flats on the same stretches with somebody living there (not away, 16 or older). The parcel is
    for the eldest.
  - **The postman.** The oldest man among the pensioners living in a T1 flat.
- `Private/Rounds/RoundsModel.h` holds the rules: judging a throw, the day's route, the day's parcels, who opens and how,
  the pay and the stamps. It is plain C++ with no engine types.
- `tools/roundsim/run.sh` is the offline test. It checks the rules and rides every route as a careful, a sloppy and a lazy
  paper boy. It also plays 60 days of post and checks every round fits its window at bicycle speed.
- `ULyudRounds` (`Public/Rounds/LyudRounds.h`, a world subsystem) does the rest:
  - **Prompts.** They go through `ULyudInventoryUI::SetPrompt`, and E is read with `ANpcTalkDirector::UsePressed`, which
    never consumes it.
  - **The papers.** Six reused meshes on one transient actor, with no collision, no shadow and out of Lumen. A line trace
    finds where each paper lands.
  - **Saving and events.** The save key is `"Rounds"`. Quest events are `round_start` paper|post and `round_done`
    paper|post|t1..t4; `interiors.py` knows both.
- The diary's map draws the round under way (`SLyudMapView`, `ULyudRounds::GetMarks`).
- Strings are in `Content/Localization/rounds/` (ru/en/ja). The collection and quest are in `tools/city/content/x_rounds.json`.
- A bike (paint 5, orange) stands by kiosk 1 (`Content/Bikes/bikes.json`, id `kiosk1`).

## Developer mode and console

- Group "rounds": start the paper round, start the post round, end the round. Teleports: kiosk 1, the postman's door.
- `lyud.Rounds paper|post|end|status` starts a round at any hour or day; its deadline is the usual window from now.

## Open

- Mac: the first compile, and ride-through shots of a paper round on T1 and a post round.
- A sound for the paper landing and for the doorbell. The soundscape has `bicycle_bell` and `stamp_thump` ids with no pool yet.
- The postman standing at his door during the window (today the bundle and the parcels are proximity points).
