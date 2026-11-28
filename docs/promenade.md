# Summer evenings on the promenade and Sun Square

In the warm half of the year the town comes out in the evening. From late May to early September the dances move out of
the hall «Своды» onto Sun Square, in front of the stage. On summer Sundays the brass band plays on that stage, and the
town stands and listens while the children run about. On every warm dry evening families, couples and teens walk the
promenade to the pier and back at sunset, the benches by the sea fill, the photographer looks for the best light and the
lighthouse comes on. No new geometry: the stage, the square, the promenade, its benches and its lamps are the public
spaces' own (docs/city/public_spaces.md). The town has no bandstand of its own, so the square's stage (a concrete shell on
its west side, facing the sea) is the bandstand, and the paving between it and the lamp row is the dance floor.

## The conditions

Four conditions of `ULyudStreetLife` (Private/People/StreetLifeModel.h, docs/street_life.md), worked out from the date,
the hour and the weather, and from the morning's plan of the day made from the forecast:

| condition | when |
|---|---|
| `prom_evening` | 15 May to 15 September, 18:30-22:30, dry (rain under 0.2 mm/h), 14 C and over |
| `prom_dances` | 25 May to 5 September, Friday and Saturday, 19:00-23:00, when the forecast keeps the evening dry (19, 20:30 and 22 h dry and 12 C and over); else the dance is called off for the whole evening. Off while a shower lasts |
| `prom_band` | June to August, Sunday, 15:00-17:00, when the forecast keeps 15 and 16 h dry. Off while a shower lasts |
| `hall_dances` | all the year but June, July and August: the hall's own dances (`dance_evening`, `dance_watch`) only happen while it holds |

So in the last week of May and the first days of September both floors are open: the hall for those who don't like
mosquitoes. The hero's dance places in the hall carry `"months": [1-5, 9-12]` in their rules (the same switch).

## What the hero finds

| When | Where | What |
|---|---|---|
| always | the poster stand by the stage's south corner (`P-PROM-BOARD/pev_poster`) | **the poster** (a paper notice, four pages): the dances' rules (25 May to 5 September, Fri and Sat 19-23, the ladies' choice at 22, the volunteer switches the music off at 23, cancelled in rain); the band's programme; pencilled notes (bring a cardigan, eau de cologne for the mosquitoes, the studio's sunset portraits); and by the day a slip: "cancelled for rain", last summer's poster in the cold months, a holiday ribbon |
| always | the dance floor's north edge (`P-PROM-BOARD/pev_floor`) | **the floor's look**: words by the hour, the day, the date and the weather: chalk hopscotch by day, the volunteer stringing bulbs at six, the dance in full swing, the lamps and the midges after nine, the mosquitoes, the sea breeze and the cardigans, rain ("dancing cancelled"), the Thursday rehearsal, the band on Sunday, eleven o'clock ("see the girls home"), the night, the winter ("the dances are at the Vaults") |
| Fri, Sat 19-23 while `prom_dances` | two places on the floor (`pev_dance_1`, `pev_dance_2`) | **ask someone to dance**: the hall's duel (`vs dnc_invite`): the same eight girls, the same tells and shy rules (a shy girl wants the hand, a giggly one talk, a bold one a joke), its own words (the slabs, the stars, the breeze). Four a night |
| Fri, Sat 22-23 while `prom_dances` | the lamp on the floor's south-east corner (`pev_white`) | **the ladies' choice under the lamp** (`vs dnc_white`): a girl of the four asks him, she leads, he keeps in step. Once a night. Before ten: the girls whispering by the lamp |
| Sun 15-17 while `prom_band` | the slabs in front of the stage, among the children (`P-PROM-BAND/pev_band_spot`) | **sit and listen**: 105 minutes of the programme. Sat down by 15:35 the whole programme is heard (a stamp); later, "from the middle" |
| always | the seawall kerb north of the pier's root (`P-PROM-LIGHT/pev_light`) | **the evening light**: by day the working sea; 19:30-21:30 in summer the photographer's best light (a stamp); 21:30-23:30 the lighthouse coming on and the lamps along the sea lighting up in turn (a stamp); people walking, every bench taken; rain (the photographer under his umbrella: wet slabs reflect the lamps); the cold months' empty promenade; night |

Every girl he dances with on the square is a stamp of the hall's collection «Партнёры по танцам» (`dnc_partners`) as well.
A dance well done is the game's `win` (quests) and `beat` (the rumour kind `dance`, docs/dances.md).

**The collection «Летние вечера на набережной»** (`pev_evening`, 5 pieces): `dance` (a dance on the square), `white`
(the ladies' choice under the lamp), `band` (the whole programme), `light` (the evening light), `lighthouse` (the
lighthouse coming on). The whole set pays two choc-ices from the ice-cream lady ("one for you, one for whoever you danced
with").

**The quest «Летний вечер»** (`pev_summer_evening`): starts at the poster. Hear the band from the first note (`stamp
pev_evening/band`) → ask someone to dance on the square (`win pev_invite`) → be by the parapet when the lighthouse comes
on (`stamp pev_evening/lighthouse`). 40 roubles. The quest event `stamp` (sent by `ULyudCityThings::GiveStamp`) is now
allowed in the city content's quests (`tools/city/interiors.py` QUEST_EVENTS).

## The band's programme

All public domain as music (composers dead long ago; the one exception is the march «Прощание славянки», Vasily Agapkin,
1912, who died in 1964: the title on a poster is fine, but a recording for the game must wait for the rights or be cut):

1. Марш Преображенского полка (18th century)
2. «Амурские волны», вальс (Max Kyuss, 1903)
3. «На сопках Маньчжурии», вальс (Ilya Shatrov, 1906)
4. «Дунайские волны», вальс (Iosif Ivanovici, 1880)
5. «Осенний сон», вальс (Archibald Joyce, 1908)
6. «Трик-трак», полька (Johann Strauss II, 1858)
7. «Прощание славянки», марш (Vasily Agapkin, 1912)

## Sound hooks

There is no brass music in the game yet (docs/music.md: the 22 music cues are ambient pieces). The acts name sound ids
that play nothing until the soundscape has them (`interiors.py` lists them as "play nothing"):

| id | where | what it should be |
|---|---|---|
| `brass_band` | `pev_band_listen` (the act's loop while the hero sits) | the band in the open air, 3D from the stage: the seven pieces in turn, a pause and applause between; the town's chatter under it |
| `dance_square` | `pev_invite`, `pev_white` | a radiogram's record through two club loudspeakers by the stage: a waltz, a foxtrot, a slow dance; a little wind in it |
| `sea_evening` | `pev_light` | the evening sea at the kerb: small waves, a far gull, footsteps and voices passing |

Wanted later: the band as a sound source on the stage (`ULyudMusic`'s record component at the stage while `prom_band` holds),
the volunteer's radiogram on dance evenings switched off at 23:00, the hall's `waltz`. The tunes could also join the hero's
rhythm game (docs/instruments.md, `Content/Music/tunes.txt`) as sheet music on the stage's stand.

## The town

`tools/town/town_promenade.py` (an add-on run by `town_life.py` after `town_dances.py`, idempotent, `--check`) writes:

| place | kind | what |
|---|---|---|
| `P-PROM-DANCE` | `dance_square` | 18 pairs of dance spots facing each other on the paving between the stage and the lamp row (clear of the lamps, the flowerbeds and the hero's three places), 21 watchers' spots round the floor's sides |
| `P-PROM-BAND` | `band_square` | 57 listeners' spots in five rows in front of the stage, turned to its middle (the hero's spot on the slabs left free), 8 children's running spots behind them, the band's 8 places on the stage's top |
| `S-PROM-EVENING` | `evening_promenade` | a walked round: north by the sea from the square to the fair's end, back by the town, a spot every 16 m (four to a spot) |

Activities (each with its `"when"`):

| activity | who | when |
|---|---|---|
| `prom_dance` | 15 to 45, the sociable more; the hall's twelve partners like dancing anyway | Fri, Sat, set off 19:15-21:25, 40-90 min |
| `prom_dance_watch` | 46 and over, standing round the floor | Fri, Sat, set off 19:15-22:00, 20-60 min |
| `band_play` | the band: eight residents with no work (`"band_job": "brass"`, role «музыкант духового оркестра»), a duty on Sunday 15-17 at eight staff spots of `P-PROM-BAND` on the stage's top | while `prom_band` |
| `band_listen` | 13 and over, standing in front of the stage | Sun 15:00-16:30, 40-110 min |
| `band_kids` | 4 to 12, running about behind the listeners | Sun 15:00-16:40 |
| `prom_evening_walk` | 6 and over, families, couples, teens | 18:40-22:00, 30-65 min, the round in its order |
| `prom_sunset_bench` | 25 and over, any public bench: after the walk the nearest are the promenade's | 19:30-22:10, 20-50 min |

The couples' planned strolls (`couple_stroll`, docs/town_life.md "The days") may take the evening round too.
Manners: the dancers sway, the watchers and the listeners clap, the listeners sway a little; mosquitoes after nine (a
scratch), a chill after half past nine (arms hugged), ice creams on warm evenings and at the band, the photographers among
the walkers take pictures while the sun is low, somebody points up when the lighthouse comes on.

Barks (`tools/barks/lines/20_promenade.txt`): the dancers (the slabs, the stars, the sea wind), short talks between
partners, the old ones watching (summer on the square, winter at the Vaults; lilac against the mosquitoes), the hero by the
fountain told to ask somebody, a girl of his age asking him at 22-23, the band's players, the listeners (the pieces, the
tuba, "Slavianka" standing), the children, the walkers, families, teens, the benches at sunset, the lighthouse coming on,
the photographers (`manner = photo`: "five more minutes and the light is gone"), the couples on their stroll (`today =
stroll`), setting off on summer Fridays and Saturdays, the morning after, and setting off for the band on Sunday.

Paper (`tools/paper/strings_src.py` NOTICES): the clubs' boards carry the summer dances (20 May to 5 September; the old July
notice of "dancing on the promenade" is now this one), the band (from 25 May to the end of August), the photo studio's
sunset portraits (mid-June to August), and in June a lost-and-found notice of the music stand left by the stage. The
hall's poster's summer slip (`city.dnc.notice.summer`) now says the dances move to the square from 25 May and the hall is
shut in June, July and August.

## Files

| What | Where |
|---|---|
| The conditions, plain C++ | `Private/People/StreetLifeModel.h` (`PromEvening`, `PromDances`, `PromBand`, `HallDances`; `FDayPlan::bDanceEve`, `bBandDay`) |
| Game: the conditions to the people, the hero's places shown and hidden, `lyud.Street.Date`, dev actions | `Public/People/LyudStreetLife.h`, `Private/People/LyudStreetLife.cpp` (`PlotWhen`: P-PROM-DANCE while `prom_dances`, P-PROM-BAND while `prom_band`) |
| The town add-on | `tools/town/town_promenade.py` → `town_places.json`, `activities.json`, `townsfolk.json`, town strings, `tools/city/extra_places.json` (P-PROM-*), `Content/Dev/teleports/promenade.json` |
| Acts, rules, the poster, the collection and its reward, the quest, dev points | `tools/city/content/x_promenade_eve.json` |
| Words ru/en/ja | `tools/city/content/strings/x_promenade_eve.json` (`city.pev.*`, `quest.pev_summer_evening.*`) |
| Developer mode's words | `Content/Localization/promenade/strings_<lang>.json` |
| Barks | `tools/barks/lines/20_promenade.txt` (sets `pev_*`) |
| Manners | `Content/PeopleData/manners.json` (rows for the activities above) |
| Notices | `tools/paper/strings_src.py` (`dance`, `pev_band`, `pev_stand`, `pev_photo`) |

After changing the content: `python3 tools/town/town_promenade.py`, then `python3 tools/city/interiors.py`. After the
notices: `python3 tools/paper/paper.py`, then `interiors.py`. After the barks: `python3 tools/barks/build_barks.py`.

## Developer mode

Teleports, group «Набережная летом» (`promenade`): `promenade.dances` (20:30), `promenade.band` (15:10), `promenade.poster`,
`promenade.sunset` (20:40), `promenade.evening` (20:00); and the city points `city.pt.pev_floor` (20:30), `city.pt.pev_white`
(22:05), `city.pt.pev_band` (15:05), `city.pt.pev_poster`, `city.pt.pev_floor_look`, `city.pt.pev_light` (21:40). They set
the hour, not the date.

Actions, group «Улица»: `street.summer` (the calendar to the next 15 July, same time of day, the day planned again),
`street.prom_evening`, `street.prom_dances`, `street.prom_band`, `street.hall_dances` (each holds its condition on, again
off), `street.auto` (all back to the weather and the date). Console: `lyud.Street.Date 07-17` (any MM-DD),
`lyud.Street.Force prom_dances 1`, `lyud.Street.Report` (the plan now says "dance evening" and "band Sunday").

To see the dance: `street.summer`, `lyud.City.Weekday sat`, teleport `promenade.dances`. The quest's stages jump with the
date, the weekday and the condition forced (`lyud.Street.Date 07-15`, `lyud.City.Weekday sun|sat`, `lyud.Street.Force ...`);
`street.auto` afterwards.

## Tests

- `tools/streetsim/run.sh`: the four conditions (a fine July evening, its season's ends, the rain on the promenade, a wet
  Friday's forecast calling the dance off for the whole evening, a missed shower emptying the floor while it lasts, the
  band's Sundays and its wet afternoon, the hall's months; a year: prom_evening 124 days, prom_dances 30, prom_band 11,
  hall_dances 273).
- `tools/peoplesim/run.sh promenade` (`tools/peoplesim/promenade_people.h`): a July Friday (36 dancing, 9 watching, 3 of the
  partners, nobody before seven or after eleven, the hall empty), the same Friday called off (nobody), a band Sunday (the
  eight on the stage, 56 listening, 12 children, all gone after five), a warm Wednesday (126 on the evening round at once,
  the promenade's benches 2795 person-minutes against 2120 on the same evening without it), no slot overbooked.
- `tools/peoplesim/run.sh dances` now holds `hall_dances` on, and checks a July Friday with it off (the room stays empty).
- `python3 tools/town/town_promenade.py --check`, `python3 tools/city/interiors.py --check`, `tools/citysim/run.sh`,
  `tools/barksim/run.sh`, `tools/mannersim/run.sh`, `tools/papersim/run.sh`, `python3 tools/check_devdata.py`.

## Owed on the Mac

- A compile (StreetLifeModel.h, LyudStreetLife.h/.cpp: four conditions, `MoveToDate`, `lyud.Street.Date`, five dev actions).
- Play it: `street.summer`, `lyud.City.Weekday sat`, `promenade.dances` at 20:30: the floor full, the hero's two places and
  the lamp at ten; check the pairs stand clear of the lamps and the flowerbeds and nobody stands in the stage. A Sunday at
  15:05: the band on the stage, the rows, the hero's spot on the slabs in front. 21:40 at `promenade.sunset`: the lighthouse.
- The people are not drawn dancing in pairs (they sway facing each other), the band holds no instruments (no brass props
  yet: a trumpet, a tuba, a drum for `blender/humans/props.py`), the volunteer's bulbs and loudspeakers are words only.
- The sounds above (`brass_band`, `dance_square`, `sea_evening`): free recordings or a generation, then the soundscape's pool.
