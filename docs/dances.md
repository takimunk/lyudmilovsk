# The dances at the hall «Своды»

On Friday and Saturday evenings the hall «Своды» (P-HALL) holds a dance in its dance room (vault C: the tile floor, the
mirror and barre, the bar, four little tables, the piano and the jukebox). The room had the floor's lines, the steps game and
the quest `hall_dance_night` before (docs/city_interiors.md, `x_p_hall.json`). This adds the evening around them: the town
comes, the hero asks girls to dance and is asked at the ladies' choice, the watchman locks up at eleven, and the town talks
about it the next day. Data only: no new C++ in the game.

## What the hero finds

| When | Where | What |
|---|---|---|
| any day the hall is open | the lobby's sign stand (`P-HALL/lobby_stand_2`) | the poster «Танцы в «Сводах»» (a paper notice, four pages): the hours, the programme, the ladies' choice at 22:00, the watchman at 23:00, and a pencilled note on how to ask (the "shy rules"). From late May to August a slip about the summer dances on the promenade; on a holiday a ribbon "the dancing is on the square" |
| the hall bar's hours (9-22), the caretaker on shift | the bar counter | three topics: «Как тут приглашают на танец?» (the shy rules again, and on a dance evening "go on, don't stand at the bar"), «Что такое «белый танец»?» (after the first; the caretaker met his wife at one in 1959), «Кто это меня пригласила?» (after the hero's ladies' choice) |
| Fri, Sat 19-23 | three places on the floor (`dance_1..3`) | **ask someone to dance**: a duel against one of eight girls of the town (15-17). She shows how she is (a tell), the hero picks how to ask: 1 hold out his hand, 2 start talking, 3 make a joke. The shy rules: a shy girl (looks at the floor) wants the hand, not talk; a giggly one (laughing with her friend) wants talk; a bold one (looks straight at him, taps her heel) wants a joke. Read right, she says yes and they dance (a waltz, a foxtrot, a slow dance); wrong, a polite no. Four a night, then his feet hum |
| Fri, Sat 22-23 | by the pillar (`dance_4`) | **the ladies' choice** («белый танец»): one of four other girls asks him, she leads, he keeps in step (a rally: E as she leads). Once a night |
| Fri, Sat 19-23 | the floor's middle, the steps on the marks | as before (x_p_hall.json); the middle now stays open till 23 like the rest of the floor |
| outside the dances | the floor's places | words by the hour: the chalk steps by day, the class on weekday evenings, the girls at the mirror at a quarter to seven, the Sunday tea dances, the watchman rattling his keys at eleven ("see the girls home instead of standing about"), the dark room after it |

Each girl he dances with is a stamp of the collection **«Партнёры по танцам»** (`dnc_partners`, 12 pieces); the whole set
pays three sweets and a paper carnation's words from the caretaker. A dance well done is also the game's usual `win` (quests)
and `beat` (the talk of the town, below).

## The quest «Белый танец» (`dnc_white_dance`)

Starts at the poster. Ask the bar how to ask (`ask hall.dnc_rules`) → learn the steps on the marks on a dance night (`win
hall_dance_steps`) → ask someone and dance (`win dnc_invite`) → dance the ladies' choice (`win dnc_white`, 22-23) → ask the
bar who it was (`ask hall.dnc_who`). 40 roubles. Every stage has a `dev` block (the teleport, the hour, `lyud.City.Weekday
sat` or `sun`).

## The town

`tools/town/town_dances.py` (an add-on run by `town_life.py`, idempotent, `--check`) adds the place **P-HALL-DANCE** (kind
`dance_floor`): ten pairs of standing slots on the floor, facing each other and clear of the hero's own places by 0.75 m;
eight seats round the little tables, the four tables' own seats (held while somebody sits) and the lobby's two benches. Two
activities, Friday and Saturday only:

| activity | who | when |
|---|---|---|
| `dance_evening` | 15 to 45, more of the sociable; the twelve partners like it (`likes` 2.2) | set off 19:00-21:20, stay 40-90 min |
| `dance_watch` | 55 and over | set off 19:00-21:35, stay 30-80 min, sitting |

So the floor is full (up to 20 dancing and 14 watching in the sim) and empty again before the watchman's eleven.
Manners (`manners.json`): the dancers sway on the floor, the knitters among the watchers knit, the others drink tea from the
buffet. The people are not drawn dancing in pairs yet: they sway facing their partner.

## Talk, rumours, paper

- **Barks** (`tools/barks/lines/19_dances.txt`): the dancers (the record, the polka, "don't look at your feet"), short talks
  between partners, the old ones watching (in our day we glided; I met my husband on this parquet), the hero by the wall
  told to ask somebody, a girl of his age asking him at 22-23 (`to = hero`, `sex = female`, `age = teen`), a friend at the
  dance (`bond = friend close`, his nickname), the young ones setting off on Friday and Saturday evenings, the morning after
  (Saturday and Sunday mornings), the grannies on the benches about who walked home with whom.
- **Rumours** (`tools/rumours/rumours.py`): the kind `dance` (2 hops, 3 days). The partners are real residents named by keys
  of our own (`city.dnc.p.<id>`, copied from the town's `npc.<key>.name`), so the game's `beat` for a dance matches the rule
  `beat city.dnc.p.*` → `dance` and never the regulars' `beat npc.*`. Its words: the paper's column, the P.S. in letters, the
  counters' «Что говорят в городе?», three greeting sets in `19_dances.txt`.
- **Paper and boards** (`tools/paper/strings_src.py` NOTICES): the club boards carry «Танцы в «Сводах»» from 11 October to
  30 November and the winter dances (felt boots to the cloakroom) from 15 January to the end of February; the July notice of
  the dances on the promenade was there before.

## Files

| What | Where |
|---|---|
| Acts, rules, partners (regulars), the poster's text, the collection and its reward, the quest, dev points | `tools/city/content/x_dances.json` |
| Words ru/en/ja | `tools/city/content/strings/x_dances.json` |
| The bar's three topics | `tools/city/content/talk.json` (`buy_hall_bar`, ids `hall.dnc_*`) |
| The town's side | `tools/town/town_dances.py` → `town_places.json`, `activities.json`, `townsfolk.json`; `manners.json` (three rows) |
| Barks | `tools/barks/lines/19_dances.txt` |
| Rumour kind | `tools/rumours/rumours.py` |
| Notices | `tools/paper/strings_src.py` (`dnc_vaults`, `dnc_winter`) |

After changing the content: `python3 tools/city/interiors.py`, then `python3 tools/town/town_dances.py` (the slots follow the
hall's things). After the rumours: `python3 tools/rumours/rumours.py`, `python3 tools/barks/build_barks.py`, `python3
tools/city/interiors.py`.

## In summer

From 25 May to 5 September the dances move out onto Sun Square, in front of the stage (docs/promenade.md): the same girls,
the same shy rules and the same stamps, the ladies' choice under the lamp on the floor's corner, rain calling it off. The
hall's own dances need the condition `hall_dances` (`ULyudStreetLife`: off in June, July and August), and the hero's four
dance places in the room carry `"months": [1, 2, 3, 4, 5, 9, 10, 11, 12]`; so in late May and early September both floors
are open. The poster's summer slip says so from 20 May to 5 September.

## Developer mode

Teleports (group interiors): `city.pt.dnc_floor` (the floor, 20:00), `city.pt.dnc_white` (the pillar, 22:05: a dev point's
own `"time"`, since the thing is open one hour only), `city.pt.dnc_notice` (the poster). They set the hour, not the day. The quest's stages jump with the day set (`lyud.City.Weekday sat`).
Console: `lyud.City.Collections grant dnc_partners`, `lyud.Rumours.Add dance city.dnc.p.lan`.

## Tests

- `python3 tools/city/interiors.py --check`, `tools/citysim/run.sh` (every hour has a line; the bar's two waiting topics are
  counted).
- `tools/peoplesim/run.sh dances` (`tools/peoplesim/dance_people.h`): a Friday and a Saturday fill the floor (at least 8
  dancing, 3 watching, 2 partners), nobody before seven or after eleven, the ages and poses right, a Wednesday empty, no slot
  overbooked. `run.sh town` and `run.sh week` stay green.
- `tools/barksim/run.sh rumours` (the rule: a dance is `dance`, not `beat`), `tools/papersim/run.sh` (the column's dance line
  renders), `python3 tools/check_devdata.py`, `tools/questsim/run.sh`.

## Owed on the Mac

- Play it: `Dev.Quest dnc_white_dance steps`, dance, wait for 22:00, see the girls at the pillar; check the duel's tells read
  well on the panel and that the floor's slots do not put anybody into the tables or the piano.
- The pairs only sway facing each other; a real pair dance (two bodies, a slow turn) wants animation.
- The band on the stand (`band_stand`) and the music: the jukebox's records play as before; a live band, a record player
  that the watchman switches off at 23:00, and the waltz sound (`waltz` plays nothing yet) are not there.
- (Done: the summer dances are on Sun Square from 25 May to 5 September, docs/promenade.md.)
