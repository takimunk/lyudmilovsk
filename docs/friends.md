# Friends among the residents

Item 12 of docs/interactives_backlog.md. The townsfolk already come to know the hero (`NoteMet`, town talk's
`met = new | seen | known`, docs/town_manners.md). On top of that every named resident now has a **bond** with him that
grows, is saved, and changes what they do. Nothing is shown as a number or a bar: only lines and gestures change.

## What makes a bond grow

| What | Points | Limit |
| --- | --- | --- |
| they greet him (town talk's greeting) | 2 | once a day |
| a line of theirs said within 6 m of him | 1 | 3 a day |
| a host: his visit to their home (within 10 m of their table) | 4 | once a day |
| a host: their home quest done (`home_<host>`) | 20 | once |
| a gift (E on somebody he knows, then a pocket item) | 12 loved, 6 liked, 2 plain | one a day ("not every day!") |
| his postcard delivered to them (docs/letters.md) | 8 | each card |
| they sat with him on a bench | 6 | once a day |
| tea at a host's (at least one topic talked about) | 12 | each tea |
| he joined their family's weekend outing | 12 | each invitation |

Levels start at 10 (acquainted), 30 (friendly), 70 (a friend), 130 (a close friend). A hello every day for a month makes
somebody friendly, not a friend; hellos, a chat now and then, a gift they love every few days and a card a week make a friend
in about a week and a close friend in two (tools/mannersim checks both).

**Gifts** are pocket items the town already sells (48, `tools/friends/content.py` `GIFTS`): who loves each is matched against
the resident's persona (habits: reader, feeder, knitter, sweet, photographer...), age group and job group (sea, artist). A toy
is loved by a child and only "liked" by anybody else. Every townsperson loves at least one of them; once he has found out what
somebody loves, the diary says so. Flowers and records aren't items yet: they go into `GIFTS` once they exist. The allotment's harvest and jam are gifts too (docs/allotment.md).

## What changes

- **friendly (2):** they greet him from further off (5.5 m instead of 3.5), always, with a wave; town talk's
  `bond = friendly` greetings; they wave first from across the street (up to 22 m, once a day).
- **a friend (3):** they give him a **nickname** (by persona: «наблюдатель», «юнга», «птичник», «сосед»... 17 of them; a toast
  says so once) and call him by it in their lines (`{0}` in `bond = friend | close` sets); they call out when they wave
  (`on = friend_wave`); when he sits down on a bench and a friend is about (60 m, awake, not at work), they come over and sit
  beside him (`UPeopleLifeSubsystem::SitBeside`, the sim's `FWorld::OrderBeside`) and tell him about their day: up to three
  lines (`on = friend_bench`, by job, age, weather, the hour and the holiday).
- **a close friend (4):** their family invites him by letter (on Wednesday or Thursday, to the club's mailbox) to their weekend
  outing (`FPerson::Outing*`: the sea front, a stroll, the fair, crumbs for the birds, the cafe, a table at the restaurant);
  when he comes within 30 m of them there at their hour, they wave, say a line (`on = friend_outing`), the diary notes it.
  The next invitation three weeks later.
- His postcard on their shelf is mentioned the day it came or the next when he passes close (`on = friend_card`).

The hero has no name in the story: "calls him by name" is the nickname each friend gives him. If he gets a name one day, it can be one more row of
`NICKNAMES`.

## Tea with the ten open-home hosts

The ten open homes (docs/city_interiors.md "Stories") are the hosts: Холм, Окада, Сингх, Кравец, Окафор, Тадессе, Оконкво,
Сантос, Гирма, Аоки. Two of them are townsfolk of the town's life (Валентина Тадессе, Ясмин Оконкво); the others have no body in
the life, and their homes are visited in their absence, as before. A host's bond grows with his visits, their home quest and his
postcards (their letters' addressee `h_<host>` is the same bond). From friendly on, a host writes in the morning (a letter in the
club's mailbox the next post day): tea the day after next at their hour. He sits on the chair at their table (`friends.json`
`chair`, a plain seat of interiors.json) from half an hour before to two and a half after: the tea page opens; two glasses of
tea and gingerbread stand on the table in front of him (if a table top is found there); a host the life has sits opposite on
their own body (taken over while the tea lasts); 1-3 talk about something of their home (each answered once), 4 gives a gift,
Esc says goodbye, Esc (or E) again closes the page; getting up from the chair ends the tea too. A missed tea is forgotten; the next invitation a week after a tea or a miss.

## The diary's «Люди»

After the diary's entries (the J tab still opens on the newest entries; turn on for the people): everybody of level 1 and up,
the closest first, four to a page: name, the word for how close (знакомство... близкая дружба), what they do, where they live
(the postal address, docs/letters.md), their family, the nickname, what they love, postcards and teas, their weekend outing
once he has been, and since when they know each other.

## Files

| What | Where |
| --- | --- |
| Rules, engine-free | `Source/.../Private/People/Friends.{h,cpp}` (namespace `LyudFriends`) |
| Engine side | `Public/People/LyudFriends.h`, `Private/People/LyudFriends.cpp` (`ULyudFriends`) |
| Sit beside the hero | `FWorld::OrderBeside` (PeopleSim), `UPeopleLifeSubsystem::SitBeside` |
| Town talk | `bond` condition (`TownBarks`), `{0}` = the nickname (`ULyudTownTalkSubsystem::Show`), greetings and lines heard feed the bonds |
| Words and data | `tools/friends/content.py` → `tools/friends/build_friends.py` → `Content/PeopleData/friends.json`, `Content/Localization/friends/` |
| Town-talk lines | `tools/barks/lines/13_friends.txt` (`bond = ...`, `on = friend_wave | friend_bench | friend_card | friend_gift | friend_outing`) |
| Invitations | `ULyudLetters::PostTownLetter` (`LyudLetters::FModel::AddLetter`) |
| Diary | `SLyudDiary` `PeoplePage` |
| Tests | `tools/mannersim/run.sh friends` (also in the default run), `tools/barksim/run.sh`, `tools/peoplesim/run.sh` |

Regenerate with `python3 tools/friends/build_friends.py` (`--check` fails on stale output) and `python3 tools/barks/build_barks.py`.

## Console and developer mode

`lyud.Friends.Report`, `lyud.Friends.Befriend [key]` (the nearest townsperson or key becomes a close friend),
`lyud.Friends.Tea <host>` (an invitation for now: sit on their chair), `lyud.Friends.Invites` (every invitation due now).
F7 group "Друзья": befriend the nearest, tea at Тадессе's now, all invitations, report; teleport to Тадессе's table.

## Known gaps

- Written in the cloud: the engine side has not been compiled yet, and there are no Mac shots of a tea yet (the brief asks
  for them: `dev.tp.friends_tea_tadesse` + `friends.tea_now`, sit on the kitchen chair).
- Eight hosts have no body in the town's life: their tea is told on the page with the tea things on the table but nobody in
  the other chair. Giving them bodies is the characters skill's work (hi3d models, Dima's approval).
- The tea things are the shop goods' boxes (`tea`, `gingerbread`), not modelled glasses and a teapot.
- `on = friend_gift` lines exist but nothing says them yet (the reaction to a gift is the `friends.gift.*` line).
