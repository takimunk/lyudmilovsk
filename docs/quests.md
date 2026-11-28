# Quests: how to write one

A quest is data and words. The engine side (`ULyudQuests`, the journal, the HUD: [ui.md](ui.md) "Quests") counts game
events; the dialogue (`npc_dialogue.txt`) says a quest's lines while it is on a stage; the quest director
(`ULyudQuestDirector`) stages the few things a quest needs in the world. The first quest written this way is
"A Waxwing for Sorokin" (`waxwing`), described at the end as the worked example.

## The five files of a quest

| What | Where |
|---|---|
| Stages, objectives, rewards, what starts it | `Content/UI/quests.json` |
| Its words: title, journal entries, objectives, spoken lines (ru, en, ja) | `Content/Localization/quests/strings_<lang>.json` |
| Who says which lines on which stage | `Content/Dialogue/npc_dialogue.txt` (`quest.<quest>.<stage> = ...`) |
| Things lying out, people reacting to a bird | `Content/UI/quest_scenes.json` |
| Places with a radius (for `reach`, map markers) | `tools/ui_map.py` `places()`, then `python3 tools/ui_map.py --places` |

The quests' words live in their own table (`Localization/quests/`), merged in by `LyudText` like the town's tables, so
quest threads do not collide in the big `strings_<lang>.json`. `tools/gen_voice.py` voices its `line.*` keys like the
rest (the speakers are in `localization/voice.json`); `tools/check_localization.py` checks it.

## quests.json

```json
{"id": "waxwing", "kind": "side", "start_on": {"event": "talk", "target": "Sorokin_oleg"},
 "stages": [
  {"id": "find", "objectives": [{"id": "talk_sorokin", "event": "talk", "target": "Sorokin_oleg", "place": "academ"}]},
  {"id": "prepare", "objectives": [{"id": "get_guide", "event": "have", "target": "bird_guide"}, ...],
   "reward": {"rubles": 0, "items": {"feed_grain": 5}, "start": ["another_quest"]},
   "dev": {"tp": "orni_library", "time": "11:00", "give": {"feeder": 1}, "exec": ["any console command"]}}
 ]}
```

- A stage is done when all its objectives are; the next begins, or the quest ends. `need` counts (default 1); `place`
  (a `map.json` place) puts the marker on the map.
- A quest begins by `auto_start`, by `start_on` (an event), by another stage's `reward.start`, or `Quest.Start`.
- An event that a **later** stage waits for finishes the stages before it: steps done out of order never leave a quest
  stuck. Two limits: a `talk` never skips ahead ("ask, do, come back and tell" quests talk to the same person twice),
  and in a side quest nothing skips over a stage that has a `talk` or `bring` objective (what is said there is the
  quest). The main quest keeps the plain rule.
- `"no_skip": true` on a stage: later stages' events never jump over it (the count's feeder must be hung in the yard
  before visitors to any feeder are counted). A quest begun by a reward does not hear the event that paid the reward
  (the talk that ends one quest is not the first talk of the next), and a quest that begins with its first stage already
  met (the thing is in the pocket) moves on at once.
- `dev` is the developer mode's (where a jump to the stage lands); the game ignores it. Teleport points are in
  `Content/Dev/teleports/<quest>.json`.
- Words: `quest.<id>.title|desc`, `quest.<id>.<stage>.log` (the journal entry when the stage begins, the hero's own
  voice, first person), `quest.<id>.<objective>.obj`, `quest.<id>.done`.

### Events

| Event, target | Sent when | By |
|---|---|---|
| `talk <Npc>` | a conversation starts (`<Npc>` = the dialogue id, the body's name: `Owner`, `Sorokin_oleg`) | `UNpcDialogueSubsystem` |
| `talk <resident key>` | a talk with a townsperson opens (`t_sokolov_ilya`, the keys of townsfolk.json; docs/town_talk.md "A talk"). Their quest lines are strings `line.<resident key>.<quest>.<stage>.<n>`, not npc_dialogue.txt sections | `ULyudFriends` |
| `leave_computer`, `walk` | the hero got up; took his first steps | the club computer, the game screen |
| `reach <place>` | came within a place's radius (as the hero or as a bird) | `ULyudQuests::UpdatePosition` |
| `get`, `use`, `wear <item>`; `have <item>` | pocket changes; `have` counts what is in the pocket | `ULyudInventory` |
| `bring <item>@<Npc>` | a talk with that person while the pocket holds `need` of the item: they are taken | `ULyudQuests` (on `talk`) |
| `eat <item>` | food or drink finished: a diner's dish to the last bite, food from the hand, a bottle (`use` is sent for anything that leaves the pocket, a dish that is ordered too) | `ULyudBiteSubsystem`, `ULyudFoodSubsystem` |
| `fish_caught <fish item>` | a fish landed on the rod | `ULyudFishing` |
| `kite <what>` | the hero's kite: `flown` (20 s in the air), `high` (30 m over the hand), `landed`, `helper_launch`, `loop`, `eight`, `low_pass` (a stunt kite's), `rescue` (a caught kite got down); docs/kites.md "Quests" | `ULyudKiteFlying` |
| `photo <place \| maru \| resident key>` | a film was developed (at the photo studio or by the hero in a darkroom) with a frame taken at that map place, or with a sharp frame of the dog Maru (`maru`) or of that resident (the people sim's key, `FPersonInfo::Key`: `t_liepa_klaus`); the town's quests check the target against `map.json`, `maru` and the keys of `PeopleData/people.json` and `townsfolk.json` (`interiors.py` `photo_subjects`, `quest_audit.py`, questsim). A quest that ends by showing the print (`ask <topic>.done`) puts `"no_skip": true` on its photo stage, or the showing would jump over it (questsim checks this) | the film camera (`Private/Photo/LyudPhoto.cpp`) |
| `bird` | became a bird | the game screen |
| `bird_seen`, `bird_tamed <Species>` | first sight; a bird ate from the palm | `ULyudBirdBook` |
| `animal_sniffed`, `animal_stroked`, `animal_fed`, `animal_friend`, `animal_walk`, `animal_home`, `animal_lap`, `animal_named <Maru\|Cat\|Dog>`; `animal_walk_reach <place>` | a cat or a dog sniffed the hero's hand, was stroked, ate from his palm, became a friend, came along, went home, lay on his lap, was named; he came to a map place with the dog at heel | `UAnimalFriendSubsystem` |
| `feeder_fill <feed>`, `feeder_fill_at <feed>@<place>` | a handful went into a feeder (and in which places the hero stood) | `UBirdFeederSubsystem::Fill` |
| `feeder_visit <Species>` | a bird came down onto a feeder while the hero was within 60 m | `ULyudQuestDirector` |
| `bird_call <Species>.<Npc>` | the hero, as that bird, sat within reach of a listener and called (F) | `ULyudQuestDirector` |
| `festival*`, `corvid_*` | the holidays, the corvids' gifts | their subsystems |

A new kind of event is one line where it happens: `ULyudQuests::Notify(this, TEXT("event"), Target)`. Targets ending in
`*` match by prefix; `a|b` in an objective's event is either.

## Lines by stage (npc_dialogue.txt)

```
[Sorokin_oleg]
quest.waxwing.find = sorokin_oleg.waxwing_meet.1 | hero.waxwing_meet.2 | ...
quest.waxwing.prepare,journey = sorokin_oleg.waxwing_tips.1
quest.waxwing_after.done = sorokin_oleg.waxwing_journal.1
```

While the quest is on one of the listed stages the person says these lines instead of their everyday ones
(`.again` = every later talk on those stages). `done` is a finished quest: said once, then the everyday talks go on. The
stage is read before the talk is reported, so the talk that finishes a stage still gets that stage's lines, and a
quest that the talk itself begins counts as on its first stage. On a stage that waits for `bring ...@<this person>`
while the hero has not got the thing, the stage is `<stage>_empty` (their "well, where is it?" line). Keys are `line.<speaker>.<scene>.<n>` as everywhere;
the voice of a line is found by its key.

### Branching talk

- `quest.<quest>.<stages>.if.<cond>[+<cond>] = keys`: a variant, said instead of the plain talk of that quest and stage while
  every condition holds, wherever in the section it is written. Conditions: `night`, `low` (sunrise, sunset), `day`, `cloudy`;
  `have:<item>`, `no:<item>` (the pocket); `done:<quest>`, `on:<quest>` (another quest finished, or going on);
  `flag:<flag>` (`ULyudQuests::Flags`, a remark said: `remark.<id>`, a pickup taken: `pickup.<id>`). A variant is counted
  apart from the plain talk (its own first time, its own `.again`). A variant without `.again` lines is a word for the
  moment: said once, then the plain talk and its hints go on. It needs a plain line of the same stage to stand in for,
  and it must carry whatever the plain talk tells the player to do.
- A quest's live stage always comes before asides and after-talks, wherever they are written in the section. A variant
  that holds for ever (`done:<quest>`) needs its own `.again` lines if the plain talk has some (the checker says so). Sun
  conditions: `low` is sunrise and the hour and a half before sunset too; on 20 October `night` begins about 17:50, so a
  `night` variant is only heard from people who are still out then.
- `quest.<quest>.done` (what they say after the quest, once) steps aside once it is said: several quests' `done` lines can
  stand in one section, and a `done.if.<cond>` variant is one more after-talk for when its conditions hold. A `done` line
  with `.again` lines is said for ever, in front of the everyday talks: do not write one.
- `aside.<quest>.<stages>...` (the same shape as `quest.`, variants and `.again` too) is a bystander's word about a quest
  that is not theirs (the club's regulars while the hero is on his first walk): it waits until their first conversation
  has been held and is said through once, the lines and then each `.again`, then the everyday talks go on.
- `quest.<quest>.<stages>.again` may be written several times: later talks go through them in turn, round and round
  (a waiting stage: a hint, then an aside).
- `python3 tools/quest_talk_reach.py` plays every person's talk offline by the engine's own rules (every stage, sun state and
  condition) and lists lines that can never be said, shadowed entries and orphan keys (`--strict` fails on them).
- `python3 tools/check_quest_talk.py` checks all of it offline (quests, stages, conditions, line keys in three languages,
  the scenes file).

## Staging (quest_scenes.json)

- `pickups`: an item (`ALyudGoodsItem`) lying on whatever stands at a point while the quest is on certain stages; the
  hero looks at it and presses E. Taken once (kept in the save), and not put out while he already has one.
- `listeners`: people who notice a bird calling beside them. The first reaction that fits (species, a quest's stage,
  `air` = called on the wing) is said over their head with its voice, their eyes on the bird. Sitting within `within_m`
  also sends `bird_call`. Sleeping or unseen people hear nothing. One person answers a call: the one a quest waits to hear
  this call beside (its `bird_call` objective), else a reaction written for a quest's stage, else one for the species,
  else anybody's; among equals the nearest (every listener in reach still gets its `bird_call`). Nineteen people listen: the
  quest givers who sit outdoors (the square's benches, the corn cart, the net bench, the cheese tent).
- `remarks`: `{"id", "person", "quest", "stages", "within_m", "lines"}`: that person says the lines of their own accord,
  once, when the hero walks within `within_m` (6 by default) while the quest is on one of the stages (`done` too): the
  grocer about the rowan he came for, a neighbour who has heard of his errand. Never during a talk; unseen or sleeping
  people say nothing; kept in the save as the flag `remark.<id>`. News takes a moment to go round: a remark is ready once
  the hero has been out of its reach (1.5 × `within_m`) while its stage holds, so nobody comments on a deed the second it is
  done beside them (`"at_once": true` for the one who speaks as the stage begins). Not through a wall, one voice at a
  time, and a talk (E) cuts a remark short.
- `"diary": "<string key>"` on a remark or on a listener's reaction: the hero's diary (J) gets that line when it is heard
  (a reaction's once, the first time; `"note"` in this file is only the author's comment). Keys `journal.heard.<id>`, the hero's own voice, first person, not voiced.
- Anything else a scene needs to say: `ULyudQuestDirector::Say(person, lines)`.

A species that lives in one corner of the map only: `"range": {"x", "y", "r", "population", "flock", "homeRange"}` in
`Content/BirdsData/species.json` (map metres); the bird life settles its few birds there (`tools/birdsim/run.sh test`,
`BIRD_ONLY=ranged`).

## Developer commands

`Quest.List` (every quest, its stages and what they wait for), `Quest.Dump` (state; `Quest.Dump entries` lists the journal keys in order), `Quest.Start <id>`,
`Quest.SetStage <id> <stage id | index | done>` (jump: begins the quest if needed, no rewards for what is skipped),
`Quest.Advance <id>`, `Quest.Event <event> [target] [n]`, `Quest.Reset`, `Quest.Scenes` (pickups and listeners now),
`Quest.Say <person> <line>...`, `Quest.GoToPerson <person> [m]`, `Quest.BirdCall <Species> <person> [air]`.
In code: `ULyudQuests::SetStage`, `StageId`, `Notify`.

## The worked example: "A Waxwing for Sorokin"

Oleg Sorokin (`sorokin_oleg`, a retired ornithologist of the Academgorodok) studied waxwings for forty years and has not
seen one here. The hero brings him one: himself.

| Stage | Objective (event) | Where |
|---|---|---|
| `find` | talk to Sorokin (`talk Sorokin_oleg`) | on a bench of the Academgorodok's square, watching the birds, 8:00–12:30 and 14:30–18:30 every day (his `birdwatch_yard` duty in `people.json`) |
| `prepare` | the bird guide, dried rowan, a feeder in the pocket (`have`) | the guide lies on the librarian's desk (a pickup, free; Olya also sells it); the Gastronom sells the rest |
| `journey` | reach the old forest beyond the north hills (`reach rowan_grove`) | map -420, 1780: 1.2 km north of the Academgorodok |
| `lure` | fill a feeder with rowan there (`feeder_fill_at feed_rowan@rowan_grove`) | |
| `wait` | a waxwing on the tray (`feeder_visit Waxwing`) | by day only. The calendar runs at real time, so while a quest waits for `feeder_visit` and the hero is within 60 m, a feeder's feed ages a minute a second (`UBirdFeederSubsystem::Hurry`): found after about 90 s |
| `tame` | a waxwing eats from the palm (`bird_tamed Waxwing`): the hero becomes it | crouch, creep, the palm with rowan |
| `return` | as the waxwing, sit within 14 m of Sorokin and call (`bird_call Waxwing.Sorokin_oleg`) | he says three lines; the quest ends |

Then `waxwing_after` (talk to him as a person: his account of it, and feed from his stock).

The waxwings are the rare bird: seven of them in two or three small groups, all within 260 m of the grove
(`species.json` "range"), nowhere else on the map; dried rowan (`feed_rowan`) is the feed they like. Starts when the
first main quest ends (the upper cable car station) or by talking to Sorokin.

Not checked in the engine yet when this page was written: see the pull request for what was run.

## Two short ones, for the shape

- `evening_paper`: Nina Korneeva asks for today's paper (`talk`), the hero has one (`have daily_paper`), brings it
  (`bring daily_paper@Korneeva_nina`): ten rubles and two pies.
- `crow_gift`: Yuki Sato says the crows bring presents (`talk`); three handfuls thrown to them (`corvid_feed_thrown`),
  a gift picked up (`corvid_gift_taken`), back to tell her (`talk`).
- `fisherman_cheese`: Pyotr Lange wants a cheese braid from Aslan's tent by the club (`have`, then
  `bring cheese_braid@Lange_pyotr`): worms and a herring.
- `window_feeder`: Dasha Kimura's bet that the yard has more than pigeons: fill any feeder (`feeder_fill *`), five
  birds come down on it (`feeder_visit *`, `need` 5), back to tell her.
- `magpie_key`: Misha Belov's lost key: any three of the corvids' finds (`corvid_find|corvid_store_dug|corvid_gift_taken`,
  target `*`, `need` 3), back to tell him.
- `painter_views`: Aya Nishimura asks the hero to look at the town for her: three `reach` objectives in one stage, any order.
- `first_catch`: begun by `fisherman_cheese`'s reward (`"start"`), so its first stage is not a talk: a rod (`have`), Lange's
  advice (`talk`), any fish (`have fish_*`), back to tell.
- `sparrow_hello`: Sonya (Kid_girl) does not believe birds make friends: tame a house sparrow, call beside her as the sparrow
  (a second `listeners` entry in `quest_scenes.json`), tell her as a person.

- `soda_science` (Galina Zotova: four `use drink_*` objectives in one stage), `hot_corn` (Valentina Guseva to Ivan Korneev:
  two people speak, `bring corn_cob@Korneev_ivan`), `night_signal` (Rustam Valiev: `have` + `wear head_lamp` +
  `reach cable_upper` in one stage), `lab_lunch` (Vika Li: `eat diner_soup`).

- `autumn_count` (Sorokin's sequel, begun by `waxwing_after`'s reward), `overdue_journal` (Olya the librarian and Lev Zotov,
  begun by `waxwing`'s `prepare` reward), `owner_supper` (begun at the end of `awakening`; two `bring` objectives in one
  stage), `tent_neighbours` (Aslan: three `eat` objectives).

Two things the chained ones taught: a quest begun by a reward gets the very event that paid the reward, so its first
stage must not wait for that event (not a `talk` with the same person); and in `npc_dialogue.txt` a `quest.<id>.done`
line goes last in its section, because the first quest talk that fits is said and a finished quest fits for ever.

People a quest needs must be findable: Sorokin, Aya and Lange have a daily duty on a bench (`birdwatch_yard`, `sketch_yard`,
`mend_nets` in `PeopleData/activities.json`, duties in `people.json`), and the journal says when; the laboratory's people and
the Korneevs sit out an hour or two in the square after work (`yard_evening`).

## The town bundle (`tq_*`)

Ten small stories that send the hero from the club and the Academgorodok down to the town's newer systems. Data and
words only: no new item, no new event, no code. Every giver is one of the story's people (when they were written the
townsfolk could not be talked to; a townsperson's talk now reports `talk <resident key>`, see the events table), so each has branching talk, a bystander's aside, a remark with a diary line, and `dev` blocks on every stage. The
developer mode point `tq.wave_requests` (`Content/Dev/teleports/tq.json`) is the request box of the Wave.

| Quest | Giver, where and when | Steps (events) | System |
|---|---|---|---|
| `tq_saturn` «Кольцо Сатурна» | Tyoma (`Kid_boy`), the club after school till 21 | `do telescope`, tell | telescopes |
| `tq_reel` «Последняя бобина» | Ivan Korneev, yard bench 15:00–17:30 | `do cinema` + `have ticket_stub`, `bring ticket_stub@Korneev_ivan`; starts `tq_log` | cinema |
| `tq_request` «Концерт по заявкам» | Nika (`Streamer`), the club from noon | `radio request`, `radio request_heard`, tell | records, radio |
| `tq_round` «Три будильника» | Stas (`Guy_a`), the club after 14:00 | `round_done paper`, tell | bicycle rounds |
| `tq_quote` «Строчка у весов» | Tanya Orlova, the Gastronom kiosk 9–19, Mon–Sat | `have library_book`, `quote *`, tell | books |
| `tq_square` «Клетка за клеткой» | Yamada (`Clerk`), the club 10–19 on weekdays | `have camera`, `photo square` + `photo market` (`no_skip`), show | film camera |
| `tq_card` «Открытка до двери» | Nao, the club door 10–21 | `letter_sent nao`, tell | letters |
| `tq_lullaby` «Два такта» | Vera (`Girl_a`), the club after 15:00 | `play_tune brahms_lullaby`, tell | instruments |
| `tq_water` «По глотку, под счёт» | Timur (`Guy_b`), the club after 15:00 | `do pump_room`, `bring water@Guy_b` | sanatorium |
| `tq_log` «Дыра в семь дней» | Lev Zotov, yard bench from 17:30 for about an hour; begun by `tq_reel`'s reward | `read lighthouse_log`, tell | lighthouse |

What writing them showed:

- **Only repeatable events are safe objectives.** `observe <sight>`, `read_book` and `learn_tune` are sent once in a game
  (the first sight, the first time to the end, the first lesson). A quest waiting for one is stuck for a hero who has
  already done it. So Saturn is `do telescope` (every use of a coin, café or library telescope; the bridge house's,
  the lighthouse's and the home telescopes have their own act ids and do not count), the lullaby is `play_tune` (every
  play with a star) and the book is `have library_book` plus `quote`.
- **A talk that begins a quest also finishes its first stage.** If the same talk belongs to another quest whose lines
  stand earlier in the section, those are said and the new quest's `ask` passes unheard. So no giver here has another
  quest that begins by talking to them, and `tq_log` is begun by `tq_reel`'s reward, not by a talk with Zotov (Olya's
  `overdue_journal` sends the hero to him too). Two overlaps remain, both rare: `hot_corn`'s cob handed to Korneev and
  `overdue_journal`'s visit to Zotov on the same talk as these quests' `ask`. The stage after `ask` and its journal
  entry repeat the whole errand, so nothing is lost but the first exchange.
- **A card can go to a story person** only when a quest already begun has a `talk` objective with them
  (`ULyudLetters::BeginCard`): Nao is the addressee of the quest she gives.
- A `[Section]` written twice in `npc_dialogue.txt` starts over: new lines go at the end of the person's own section.

Not checked in the engine: the bundle was written and proven with the offline tools only.

## The second town bundle (`tq2_*`)

Seven stories on what came to the town on 2026-11-27 and had no story yet: the beach's play, the kitchen, the hero's room,
the payphones and tear-off notices, the fortune cards, the traces a visit leaves, Science Day. Data and words only, like the
first bundle: no new item, event or code. Each has branching talk (sun, weather, the pocket), a bystander's aside, a remark
with a diary line, a reward and a `dev` block on every stage. Two first-bundle quests now chain on: `first_catch`'s reward
starts `tq2_ukha`, `tq_log`'s starts `tq2_chess`; `tq2_whiteear`'s starts `tq2_jam`.

| Quest | Giver, where and when | Begins | Steps (events) | Systems |
|---|---|---|---|---|
| `tq2_whiteear` «Белое ухо» | the owner, his booth 12–2 | the puppy notice torn (`stamp strips/puppy`) | `do tch_call`; ask the owner; Maru to the beach (`animal_walk_reach beach`) + `beach skip3`; tell | notices, payphone, Maru, skipping |
| `tq2_jam` «Банка от простуды» | the owner (he caught cold waiting at the door) | `tq2_whiteear`'s reward | `have jam_strawberry` (cooked on a stove or the allotment neighbour's), `bring jam_strawberry@Owner` | kitchen, allotment |
| `tq2_plant` «Записка под горшком» | the owner | the owner watered the dry plant (`do myroom_minded`) | thank him; `do myroom_water` ×3 (once a day) + `do myroom_put` | the hero's room |
| `tq2_ukha` «Уха без лаврушки» | Pyotr Lange, the pier's net bench 12:30–17 | `first_catch`'s reward | `cook ukha`, `bring dish_ukha@Lange_pyotr` | kitchen, fishing |
| `tq2_fortune` «Дальняя дорога» | Nika (`Streamer`), the club from noon | the fourth fortune card (`stamp fortunes/4`: the bus stop scale's fourth weighing) | show her; `have sandwich` + `reach mayak_lighthouse` (`no_skip`); `eat sandwich`; `bring seashell@Streamer` | fortune cards, scales, beach finds |
| `tq2_chess` «Ход в неделю» | Lev Zotov, yard bench from 17:30 | `tq_log`'s reward | `do tch_call` (the stamp-swap number); `do chess` at the Cog, where a game is left mid-way (city_dress.md "Traces of a visit"); tell | payphone, notices, traces |
| `tq2_science` «Лекция о Солнце» | Lev Zotov | `festival science_day` | `festival_visit science_day.lecture`; tell him; `do telescope` in the evening | Science Day |

What writing them showed:

- **A reward-begun quest hears the talk that paid for it**, so `tq2_ukha` and `tq2_jam` begin on a non-talk stage, and
  `tq2_chess`'s first stage is the call: the giver's lines for that stage carry the request (his first talk after is the
  ask), and the journal entry tells the whole errand at once. A first stage that is `have` would let an item already in the
  pocket be handed over by that same talk (chain_met, then `bring`); so the ukha waits for a fresh `cook ukha` (Lange has a
  line for a bowl already in the pocket). The jam keeps `have` (the neighbour's jar is fair): a jar already carried is taken
  by the talk that ends `tq2_whiteear`, without the jam's lines.
- **Three quests share the owner.** If two of his are on a talk stage at once, the first written (`tq2_whiteear`, then
  `tq2_jam`, then `tq2_plant`; `owner_supper` before them all) is said and the other's talk passes unheard; every journal
  entry after a talk repeats the whole errand. Nika (`tq_request`, which her first talk begins) and Zotov (`overdue_journal`,
  `tq_log`) the same.
- **Keep a person's quests few.** `quest_talk_reach.py` enumerates every combination of the quests a person has talk for
  (asides count): one more quest on Olya, who already had five, took the run from a minute and a half to over ten. The
  fortune went to Nika (two quests) and its aside to Yamada instead.
- **Code-sent `do` targets.** The hero's room sends `do myroom_water|myroom_put|myroom_plant|myroom_minded` from code, not
  from an act; questsim now reads those from `ULyudQuests::Notify(..., TEXT("do"), TEXT("..."))`.
- Starts on a stamp (`strips/puppy`, `fortunes/4`) happen once in a save: a save that already has the stamp never begins
  the quest. Tonight's systems are new, so no save has them; the objectives themselves are all repeatable.
- The potatoes and the dill of the ukha, and the strawberries, only grow at the allotments: these two wait for a harvest.

Not checked in the engine: written and proven with the offline tools only (`check_quest_talk.py`, `quest_talk_reach.py`,
questsim, `check_devdata.py`, `quest_audit.py`).

## Townsfolk as quest people

Since the town's talk (docs/town_talk.md "A talk"), any of the ~900 townsfolk can give a quest or take part in one: E on a
resident reports `talk <resident key>` (`t_kovalenko_tyler`), and `bring <item>@<resident key>` works with it. Their words
are not in npc_dialogue.txt. They are strings in `Content/Localization/quests/`, said on the talk's page before the
greeting (`ULyudFriends::OpenTalk`), at most eight lines in all:

| Key | Said |
|---|---|
| `line.<resident>.<quest>.<stage>.<n>` | while the quest is on that stage (any quest: a bystander's aside is just lines for another quest's stage). A quest that begins by a talk with them counts as on its first stage; a stage that waits for `bring ...@<resident>` without the thing in the pocket is `<stage>_empty` |
| `...<stage>.again.<n>` | on later talks on that stage, instead of the plain lines (write them for every stage where they wait: the plain lines carry the errand, the `.again` lines are a hint or a word of theirs) |
| `...<stage>.if.<word>.<n>` | once, instead of the plain lines, while the word holds: `rain` `snow` `fog` `cold` `warm` `wet` `night` `low` `cloudy` `clear` `day` `morning` (5-12) `afternoon` (12-17) `evening` (17-23) (`ULyudTownTalkSubsystem::NowHolds`, the first in that order wins). It counts as the first talk too, so it must carry the errand |
| `line.<resident>.<quest>.done.<n>` | once, after the quest is done |

What was said is kept as quest flags `tcq.<resident>.<quest>.<stage>[.if.<word>]`. Remarks (`quest_scenes.json`) take a
resident's key as `person` like a story person's. A dev block reaches a resident with `"exec": ["Quest.GoToPerson <key>"]`.

**Findable.** A resident is a good quest person only if their day brings them to one place at known hours on most days,
outdoors (nobody indoors, asleep or riding can be talked to) and not behind a counter (the counter's E comes first). Most
are not: shifts rotate, likes pick a different bench every day. `tools/townchatsim/run.sh where` runs the town over seven
autumn days from Tuesday 20 October (the street's conditions from `StreetLifeModel.h`, a rainy morning, a showery
afternoon) and lists who is free for a word at one point at the same hour on three days in four; `where <key,key> [days]`
prints each one's day by day: when they are free for a word, doing what, where. The journal entry of every stage says
where and when the person is.

`tools/check_quest_talk.py` checks these keys: the resident, the quest, the stage and the word exist; lines are numbered
from 1; a variant or `.again` has plain lines; every stage that waits for a talk with a resident has their lines, and a
`bring` to one has `<stage>_empty` lines too. `quest_talk_reach.py` does not count them as orphans, questsim accepts a
resident as a `talk` or `bring` target.

## The third town bundle (`tq3_*`)

Seven small stories given by ordinary townsfolk, so the town reads as a community: the yard keeper, the milkwoman, the
postwoman and her boy, two neighbours of fifty years, a dog walker, two chess rivals on a bench, a schoolgirl with a kite.
They know each other: the helpers of one quest are the givers of the next, and their remarks and after-talks name each
other. The first three chain on (`tq3_whistle` starts `tq3_milk`, which starts `tq3_pension`, which starts `tq3_flag`);
each but the flag also begins by a talk with its giver. One new item, the whistle (a pickup); everything else is data and
words.

| Quest | Giver, where and when (simulated, `run.sh where`) | Steps (events) | Systems |
|---|---|---|---|
| `tq3_whistle` «Свисток в листьях» | Tyler Kovalenko, the yard keeper: sweeps the promenade 5:20-7:30 every day, rakes the yards' leaves 8:10-11:00 Mon-Sat (20 Sep-15 Nov) | ask; the whistle in yard 1's leaves by the dominoes table (pickup, `have keeper_whistle`); `bring keeper_whistle@t_kovalenko_tyler` | street life (sweep, leaf raking) |
| `tq3_milk` «Два пакета молока» | Nour Hoffmann, the milkwoman: the milk round 5:50-7:30 Mon-Sat, the benches by the square in the afternoons | ask (two `milk_pyramid` given); `bring` to Đức Lê (fishing on the beach 5:30-6:50 every day) and to Maarja Berntsen (the promenade's south end 8:10-10, the square's bench SQ04 in the afternoons); tell | street life (milk round), beach |
| `tq3_pension` «Пенсионный день» | Sachiko Patel (83): the benches by her house T2H2 from about 10:30 till evening, her allotment bed 15-17 | ask; `bring daily_paper@` her (the kiosk); talk to the postwoman Lyudmila Hernández (her round 8:20-12:10 Mon-Sat); tell | kiosk, the post round, allotment |
| `tq3_flag` «Флажок для Кайо» | Lyudmila Hernández, the postwoman (as above; minds her son at the playground or yard 5 about 14-16); begun by `tq3_pension` | ask; `beach castle_paper_flag` (the kiosk's `sand_flag` on a finished sandcastle); tell | beach, kiosk |
| `tq3_clock` «Сто: точное время» | Ingvild Solberg (76): walks her dog in the park 6:50-7:20 and 18:50-19:30 every day | ask; `do tch_call` (the speaking clock, 100); tell | payphones |
| `tq3_referee` «Судья для двоих» | Haoran Choi (82): the benches by his house T2H1 8:00-12:30 most days; his rival Xiaoming Virtanen there 8-11 | ask; `have daily_paper` + `do chess` (a board at the Cog); tell | kiosk, chess |
| `tq3_number` «Оторванный телефончик» | Isla Bondarenko (12): after school on weekdays her kite on the beach 14:40-15:20, then the playground till about 16:40 | ask; `do xk_notices` (the bus-stop boards) + `do tch_call`; tell | notices, payphones, kites |

Every stage has a dev block (`Quest.GoToPerson <key>` at an hour they are out, `tq3.yard_leaves` and `tq3.beach_castle`
in `Content/Dev/teleports/tq3.json`). Bystanders: Nour and Tyler on each other's errands, Maarja on her friend's pension,
Le on the castle, Alejandro Gelashvili (a dog walker too) on the clock, Aino MacLeod (Choi's wife) on the referee, Lyudmila
on Isla's guitar: each a remark with a diary line; Caio (shy till he knows the hero) and Virtanen have lines of their own.

What writing them showed:

- **Pick the giver from the simulation, not the job.** Of 900 townsfolk only a few dozen are at one point at the same hour
  on three days in four; the lighthouse keepers work inside the pier's shell and can never be talked to at work; kiosk
  and promenade sellers stand behind a counter. People on a round (the sweep, the milk, the post) are reliable by the
  hour, not the spot: the journal says the hours and the route. The old on their own yard's benches, the fishermen at
  dawn and the dog walkers at seven are the town's fixed points.
- **Autumn.** The game begins on 20 October and the calendar runs at real time: soda machines are hooded from November,
  the kvass barrel and the grandmothers' yard benches are summer things, the leaf raking ends on 15 November. So the
  objectives here are things that work all year (the payphone, the kiosk, the sandcastle, the chess board), and the
  whistle lies beside the leaf pile, not under it, so it is still there when the piles are gone.
- A resident's lines from several quests all show on one page (up to eight), unlike a story person's first-fit talk:
  keep each group to two or three lines.

Not checked in the engine: the resident lines' code (`LyudFriendsTalk.cpp`, `TownTalk.cpp` `NowHolds`) and the whistle
(`LyudInventory.cpp`) were written without Unreal; the icon `icon_keeper_whistle` is owed.
