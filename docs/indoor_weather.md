# Weather and night indoors

Backlog item 28 (docs/interactives_backlog.md). The town's day changes with the weather: in the rain the cafes, the
library, the clubs and the sanatorium's lounge fill up and the outdoors empties; in the heat there is a line at the
ice-cream kiosk; on a dark day the lamps in the flats go on hours earlier; the rooms sound otherwise in the rain (a full
cafe, the rain on the windows); on rainy nights the lighthouse keeper's receiver crackles with ship talk.

## Who goes where

`tools/town/weather_pull.py` writes `Content/PeopleData/weather_pull.json`, which PeopleSim reads after the town
(`FWorld::LoadWeatherPull`; the file is optional). Each row names place kinds or place ids and how strongly each weather
draws people there, on the scale of a choice's score (1 is a strong wish):

| weather | when | scale |
|---|---|---|
| rain | raining (the wrapper's `bRain`) | 0.6 in a drizzle to 1 at 4 mm/h |
| cold | below 2 C | full at -6 C |
| hot | above 22 C, sun up, dry | full at 28 C |
| storm | wind above 12 m/s | full at 20 m/s |
| dark | after sunset | flat |

The pull is added to the score of a free activity at that place (`FWorld::Score`, after the outdoors' rain penalty) and,
choosing between the places of an activity, takes `PullMeters` (160 m) of walking off per unit (`PickSlot`). A place takes
the first row naming its id, else the first naming its kind.

The rows now: the ice-cream kiosk P-KIOSK2 (hot), the cafes and the restaurant (rain, cold, storm, a little heat and
evening), the library, the clubs and the hall (rain, cold, storm, evening), the sport hall, the sanatorium (kind hotel),
shops and the market.

The file also brings its own activity, **shelter**: waiting out the weather under a roof (a cup at the counter, the papers
in the library, the club's hall), at the visit spots of those places. It is chosen only where the pull is 0.3 or more now
(`FActivity::bShelter`), so on a fine day nobody thinks of it. It is appended after activities.json's rows
(`LoadActivities(Text, true)`), so their indices stay.

`tools/peoplesim/run.sh weather` runs the town on a Saturday 11:00-16:00 four times: dry (13 C), rain (4 mm/h, 11 C), hot
(28 C) and rain without the pull file. On 2026-11-20 it gave, per sample:

| | indoors (public) | out of doors | at the ice-cream kiosk |
|---|---|---|---|
| dry | 148 | 415 | 0.3 |
| rain | 537 (cafes 102, culture clubs 194, hall 73, library 89) | 0.3 | 0 |
| hot | 146 | 426 | 1.0 |
| rain without the pull | 179 | 0.8 | 0 |

Without the pull the rain only sent people home (the outdoors' penalty); with it they go where there is company.

The kiosk has one visit spot in the town data, so the "line" is one buyer at a time plus its bench; a real queue belongs to
item 27 (counters with service).

## Lamps on dark days

The wrapper (`UPeopleLifeSubsystem`) sets `FWorld::Gloom` = 0.6 cloud + 0.35 rain (full at 4 mm/h) + 0.25 fog. A flat's
lamp (`FlatLitNow`) goes on up to three hours before sunset (and stays on as long after sunrise) as the gloom passes
0.45, and all day above 0.92. The weather test checks that no flat is lit at 14:00 under a bright sky and that 19 are on a
dark rainy day.

## Rooms in the rain

Room tones (`tones` in `tools/city/content/talk.json`, docs/city_interiors.md "Room tones") take two new fields:

- `"weather": {"rain": "<tone>"}`: while the room is open and that weather word holds (the words of `WEATHER` in
  interiors.py: rain, snow, fog, cold, warm, clear, wet, dry), that tone plays instead. `cafe_room` → `cafe_full`,
  `bar_room` → `bar_full`, `lighthouse_room` → `lighthouse_storm`.
- `"windows": 0..1`: how much of the rain on the room's windows is heard inside: the soundscape's own glass voice
  (`ULyudSoundscapeSubsystem::SetNearWindows`, which nothing called before). The library 0.7, cafes 0.5-0.6, halls 0.4,
  the lighthouse 1.

ULyudCityThings asks the weather words when it picks the tone (`TickTone`, every pick step, not every frame).

The lighthouse (P-PIER) has its own tones now: `lighthouse_room` (a hush, the keeper's cat purring, the buoy's bell, a
far foghorn) and in the rain `lighthouse_storm` (radio static, the dial, the pips, foghorns, ship horns). The keeper's
table of ships' lights (`keeper_ships` in `x_p_pier.json`) has three night lines for rainy nights: the receiver's ship talk
(the seiner «Мария» of the watch book, the harbour master's storm warning).

citysim checks the lighthouse and cafe tones dry and wet, and that every weather tone and window share is valid.

## Not done here

- The cafes' windows opening on hot days (a visual change of the buildings).
- A "storm" weather word for lines and tones: the eight weather bits are full (`EWeatherBit` is a uint8); rainy nights
  stand in for the storm.
