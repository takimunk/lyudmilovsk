# The talk of the town (backlog item 22)

The town notices what the hero does. A game event that the rules call news becomes a **rumour**. People close by know it at once. The rest of the town hears it along its ties over the next days, and after a few game days it is forgotten.

## Where things are

| Part | File |
|---|---|
| Model (engine-free: ties, spread, decay, save, paper/P.S. picks) | `Private/People/TownRumours.h` (namespace `LyudRumour`) |
| Subsystem (store, save key `TownRumours`, hooks, console) | `Public/People/LyudTownRumours.h`, `Private/People/LyudTownRumours.cpp` |
| Kinds, rules, settings, words ru/en/ja | `tools/rumours/rumours.py` → `Content/PeopleData/rumours.json`, `Localization/rumours/`, `tools/city/content/x_rumours.json` |
| Barks | `tools/barks/lines/15_rumours.txt` (condition `heard_of = <kind>` / `any`) |
| Offline test | `tools/barksim/run.sh rumours` (`tools/barksim/rumourchecks.h`) |

After changing `rumours.py`, run it, then `python3 tools/barks/build_barks.py` and `python3 tools/city/interiors.py`.

## Kinds and rules

There are 15 kinds: cup, garden, beat, cure, job, tamed, radio, fine, crows, catch, kite, tune, sing, film, dance (the dances at the Vaults, docs/dances.md). Each kind has these fields:

- `hops`: how far it travels.
- `life`: days it is remembered.
- `pass`: the chance one tie passes it on.
- `seed`: made-up witnesses for town-wide news.
- `news`: the share of the town that learns it from the paper or the radio, on day 1, or on day 0 with `news_today`.
- `once`: told once per detail ever, such as the first shift at a given job.
- `rank`: its weight in the paper and the P.S.

Rules map `ULyudQuests::Notify(Event, Target)` to a kind and a detail key with `{t}` (the target) and `{p}` (its tail after `/`). Examples:

- `cup_won *` becomes cup with `city.coll.cups.{t}`.
- `beat npc.*` becomes beat with the regular's name.
- `beat city.dnc.p.*` becomes dance with the partner's name (the dance partners are regulars named by keys of their own).
- `stamp work_book/*` becomes job.

These events were added for it:

- `cup_won`, `beat`, `stamp` and `stamp_set` (LyudCityThings);
- `fined` (LyudTrainPassenger).

## How it spreads

`BuildTies` links people in three ways:

- **Household:** the same day.
- **Workmates:** a shared duty place, the same day. A group of more than 12 gets 4 hashed links each.
- **Friends:** the same day if their homes are within 200 m, else a day later.

Spreading goes hop by hop, and every hop after the first costs at least a day. So the witnesses tell their households and workmates today, and a far terrace hears it a day or two later. Who hears it is worked out once, when it happens. Each person stores the day they heard it (`HeardOn`).

The sim shows this for one beaten regular:

- 15 people know it on day 0, 41 on day 1, 151 on day 2, and nobody by day 4.
- On the same day, 5 of 9 near people know it, but only 4 of 136 far people.
- A cup reaches 349 people the next day, through the paper.

## Who says what

- **Barks** (ULyudTownTalkSubsystem):
  - Applies only when the hero is within earshot.
  - The town-wide pause (`remark_every_s`, 150 game seconds) must be over.
  - A person makes at most one remark a day, and tells each rumour once.
  - Freshness fades with age (1, 0.8, 0.5, 0.3).
- **Paper:** the column «Говорят в городе» prints up to two lines about the day before's talk.
- **Letters:** a P.S. in replies and home letters.
- **Staff:** every counter with staff talk offers the topic «Что говорят в городе?» while anything is going round. It is a model-level `everywhere` topic, and each line has `rumour: <kind>`.

## Testing

- Console: `lyud.Rumours.Status`, `lyud.Rumours.Add <kind> [detail]`, `lyud.Rumours.Clear`.
- Developer mode (group friends): `rumour.cup`, `rumour.beat`, `rumour.status`, `rumour.clear`.
- Mac check: `lyud.Rumours.Add cup city.coll.cups.cog_chess`, then `Status`. Walk up to a townsperson and wait for a remark. Open the next morning's paper.
