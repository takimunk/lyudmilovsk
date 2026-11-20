# Records, the jukebox and the town radio «Волна Людмиловска»

Interactives backlog item 10 (`docs/interactives_backlog.md`), 2026-11-20, branch `records-radio`. Music becomes something the
hero chooses: he buys records, puts them on, feeds the jukeboxes, switches radios on, and sends a request to the evening show.

## Where the hero meets it

| Where | What happens at E |
|---|---|
| The records shop in the Wave Tower (`P-WAVE/shop_b_counter`, act `record_shop`) | the shop «Пластинки «Волна»»: all 18 records, 40–60 rub. |
| The department store's books counter (`store_books`) | six of them |
| Any jukebox (8: «Волна», the department store, Club 3, the hall's dance jukebox, three promenade cafés, «Тарелка») | 5 rub.: the next record of the 18 plays from the jukebox (3D, to its end), its line on the page, a stamp of «Пластинки, которые я послушал» (`records/<cue>`, 18 pieces) |
| A record player (8: «Волна»'s counter, its listening post and booth, the library's two, the department store's, the school's, the common room of house P-T4H1) | the next of the hero's own records goes on (in turn, a record bought later slots in by its order), its sleeve note on the page, the stamp; with none of his own: the house record and a hint where to buy them |
| Any broadcast radio (46: shops, homes, kiosks, the clinic's radio point, the lifeguards' cabins...; not the radio amateurs' sets) | switches it on/off. On: the station plays from it while the hero is within 7 m on its floor, with its words as subtitles. Twelve public radios (bakery, barber, beer bar, workshops, the department store, the fair, lifeguards, the pier keeper, the clinic, the sanatorium couch, kiosk 6) are on by themselves 7:00–22:00; switched off, they stay off that day |
| The hit-parade board of «Волна» (`P-WAVE/records_hits`, prompt «хит-парад и ящик заявок») | with a postcard (4 rub. at any news kiosk): the card goes in the request box (used up) asking for the record he played last, with a dedication; it is read in the next evening's «Концерт по заявкам» at 20:00 and that record plays. Heard on a radio: a toast «По радио передали мою заявку!», quest event `radio request_heard` |

A record or a radio heard nearby holds the ambient music off (`SetMusicHold`). A record nearby wins over a radio.

## The station

A pure function of the game clock (1 game second = 1 real second), so every radio plays the same thing at the same second, and
a radio switched on comes in mid-record. Schedule (`tools/music/content.json` `station.schedule`):

06:00 sign-on (call sign chime, time pips) · 06:04 «Утренняя волна» · 07:00 news · 07:08 day music · 09:00 weather and tide ·
09:05 music for work · (Sun 10:00 «Воскресная пластинка») · 12:00 news · 12:08 the «Волна» chart (the records) · 15:00 weather ·
15:05 the electronic hour · 16:00 afternoon · 18:00 news + weather · 18:10 evening · 20:00 request show · 21:00 night ·
23:00 shipping forecast (six Baltic sea areas, wind, sea, visibility from the real weather) · 23:06 after midnight ·
01:00 sign-off · off air till 06:00 (the hiss).

Music hours pick records from the segment's pool by the day's dice (never the same twice running; in rain, snow or a
thunderstorm half come from that weather's pool). Each record is announced as it starts. The talk segments run their words
(6.5 s each) over a quiet bed record: news (three items a bulletin from 36 town items + 3 per season, a different window
every day; the morning bulletin reads the town paper's front page when `ULyudTownPaper` is compiled in, `__has_include`),
weather (sky, temperature, wind, the tide's next turn from `LyudTide`, one piece of advice), the shipping forecast, the
request show's sixteen listeners' requests. The announcer has no voice yet: TTS through fal costs money (the brief says ask
Dima first); the words come as subtitles, the call sign and pips are procedural sounds.

## Code and data

| File | What |
|---|---|
| `Private/Music/MusicModel.h` | `LyudMusic::FModel`, plain C++: roles of acts (globs), the station's timeline per segment (`Lay`, `AirAt`), players' turn, radios on/off, the request, Save/LoadState |
| `Public/Music/LyudMusic.h`, `Private/Music/LyudMusic.cpp` | `ULyudMusic` (world subsystem, saved as "Music"): the city hook, one audio component for the record and one for the radio (moved to the nearest radio that is on), the subtitle widget, the facts (weather, tide, paper) every 20 s, 4 Hz ticks |
| `Private/City/LyudCityThings.cpp` | `ActHook` (asked after an act ran, may replace the page's line), `GetThingsByAct`, `ActOfThing`, `GiveStamp` |
| `tools/music/content.json`, `tools/music/strings.json` | the source: tracks (the game's 22 music cues), 18 records, shops, act roles, the schedule; every word ru/en/ja |
| `tools/music/music.py` | writes `Content/City/music.json`, `Content/Localization/music/strings_*.json`, `tools/city/content/x_music.json` (act `jukebox_records`, which `jukebox` is like; shop `records_wave`) and its strings; `--check`, `--report` (which things are what) |
| `tools/music/sounds.py` | the procedural sounds: `radio_chime`, `radio_pips`, `radio_static` (group Radio, added to the soundscape's pools) and the acts' missing `vinyl_crackle`, `radio_tune`, `jukebox_click` (group City) |
| `tools/music/icon_grids.json`, `tools/music/icons.py` | the 18 icons: two 3x3 Higgsfield grids (generated; job ids and URLs in the file), cut into `textures/localized/ru/icon_record_*.jpg` |
| `tools/musicsim/run.sh` | the offline checks (below); `run.sh show <day>` prints a day's programme in Russian |

Items `record_<cue>` in `LyudInventory.cpp` (`Good`, flat, 31 cm sleeve). Quest events: `play <record id>`, `radio on|off|request|request_heard`.

## Checks

`tools/musicsim/run.sh`: 120 days of air every 15 s (never more than 15 s of silence on air, no record past its end, every
caption key and every word argument in ru/en/ja with enough arguments, the hero's request read the day after), the same air
at the same moment, a different 13:00 record most days, the schedule's shape (news 7:00, weather 9:00, requests 20:00,
shipping 23:00, off air 3:00, music past midnight, the Sunday show only on Sunday, the chime at sign-on), the roles of the
real act ids, the players' turn, radios and auto radios, the request rules, the save round trip. Also
`python3 tools/music/music.py --check`, `python3 tools/city/interiors.py --check`, `tools/citysim/run.sh`.

## On the Mac (not done yet: the cloud cannot build or reach the image CDN)

1. Icons: `python3 tools/music/icons.py icons_records_1 <job_id> <output_url>` and `_2` (from `tools/music/icon_grids.json`),
   look at the 18 cells, then `unreal/scripts/import_hall_assets.py`; diary sketches `python3 tools/ui_diary_icons.py record_<cue>`
   + `import_diary_assets.py`.
2. Sounds: `IMPORT_ONLY=sfx/radio` and `IMPORT_ONLY=sfx/city` with `unreal/scripts/import_audio.py`.
3. Build (`tools/wait_ubt_queue.sh`). Then in game: F7 → music → «Все пластинки в карман»; Dev teleport to P-WAVE; E at the
   listening post (a record plays, the stamp toast), E again (the next); E at the jukebox; E at a radio (`lyud.Music.Radio`
   prints the air); `Dev.Do time...` to 23:00 for the shipping forecast; «Заявка на сегодняшний вечер» and stand by a radio at
   20:00. `lyud.Language en|ja` for raw keys. Bench frame and memory against main (nothing per frame but a distance test).

Open for Dima: the announcer's voice (TTS via fal, cost to be asked); a record player in the club and in the hero's room
(item 19).
