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
  stuck. Write stages so that this is harmless. A `talk` never skips ahead: "ask, do, come back and tell" quests talk to
  the same person on the first stage and on the last.
- `dev` is the developer mode's (where a jump to the stage lands); the game ignores it. Teleport points are in
  `Content/Dev/teleports/<quest>.json`.
- Words: `quest.<id>.title|desc`, `quest.<id>.<stage>.log` (the journal entry when the stage begins, the hero's own
  voice, first person), `quest.<id>.<objective>.obj`, `quest.<id>.done`.

### Events

| Event, target | Sent when | By |
|---|---|---|
| `talk <Npc>` | a conversation starts (`<Npc>` = the dialogue id, the body's name: `Owner`, `Sorokin_oleg`) | `UNpcDialogueSubsystem` |
| `leave_computer`, `walk` | the hero got up; took his first steps | the club computer, the game screen |
| `reach <place>` | came within a place's radius (as the hero or as a bird) | `ULyudQuests::UpdatePosition` |
| `get`, `use`, `wear <item>`; `have <item>` | pocket changes; `have` counts what is in the pocket | `ULyudInventory` |
| `bring <item>@<Npc>` | a talk with that person while the pocket holds `need` of the item: they are taken | `ULyudQuests` (on `talk`) |
| `bird` | became a bird | the game screen |
| `bird_seen`, `bird_tamed <Species>` | first sight; a bird ate from the palm | `ULyudBirdBook` |
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

## Staging (quest_scenes.json)

- `pickups`: an item (`ALyudGoodsItem`) lying on whatever stands at a point while the quest is on certain stages; the
  hero looks at it and presses E. Taken once (kept in the save), and not put out while he already has one.
- `listeners`: people who notice a bird calling beside them. The first reaction that fits (species, a quest's stage,
  `air` = called on the wing) is said over their head with its voice, their eyes on the bird. Sitting within `within_m`
  also sends `bird_call`. Sleeping or unseen people hear nothing.
- Anything else a scene needs to say: `ULyudQuestDirector::Say(person, lines)`.

A species that lives in one corner of the map only: `"range": {"x", "y", "r", "population", "flock", "homeRange"}` in
`Content/BirdsData/species.json` (map metres); the bird life settles its few birds there (`tools/birdsim/run.sh test`,
`BIRD_ONLY=ranged`).

## Developer commands

`Quest.List` (every quest, its stages and what they wait for), `Quest.Dump` (state), `Quest.Start <id>`,
`Quest.SetStage <id> <stage id | index | done>` (jump: begins the quest if needed, no rewards for what is skipped),
`Quest.Advance <id>`, `Quest.Event <event> [target] [n]`, `Quest.Reset`, `Quest.Scenes` (pickups and listeners now),
`Quest.Say <person> <line>...`, `Quest.GoToPerson <person> [m]`, `Quest.BirdCall <Species> <person> [air]`.
In code: `ULyudQuests::SetStage`, `StageId`, `Notify`.

## The worked example: "A Waxwing for Sorokin"

Oleg Sorokin (`sorokin_oleg`, a retired ornithologist of the Academgorodok) studied waxwings for forty years and has not
seen one here. The hero brings him one: himself.

| Stage | Objective (event) | Where |
|---|---|---|
| `find` | talk to Sorokin (`talk Sorokin_oleg`) | home in the Academgorodok mornings and evenings, the island by day |
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
