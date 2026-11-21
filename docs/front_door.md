# Aton: the club's front door

Backlog item 40. The Aton station is the first hall the hero walks through and stands next to the club, so it now works
like the club's front door: a buffet with a line and regulars, a departures board, a travel card to collect stamps on,
a lost and found shelf, and lamps that come on at dusk with the club's sign.

Only things and content were added. The halls' meshes are untouched (blender/transport/halls.py, halls.json).

## Where it lives

| Part | Code |
| --- | --- |
| The rules, engine-free (board, line, usual, lost and found, card) | `Private/Transport/FrontDoor.{h,cpp}`, namespace `LyudFrontDoor` |
| The state, the sign, the owners in town, the console | `ULyudFrontDoor` (`Public/Transport/LyudFrontDoor.h`), save key `FrontDoor` |
| The board and the sign widgets | `Private/Transport/SLyudFrontDoorWidgets.{h,cpp}`: `SLyudFlapBoard`, `SLyudClubSign` |
| The hall: line spots, punch, board, shelf, prompts | `ALyudStationHall` (`Private/Transport/LyudStationHall.cpp`, "front door" part) |
| The barista as a server | `ULyudCounterService::RegisterBody`, `OpenedAt`, `HandoverPointFrom`, `BodyOf` |
| The stamp set «Все станции» | `tools/city/content/x_stations.json` (+ `strings/x_stations.json`), reward: the badge |
| Words | `Content/Localization/frontdoor/strings_{ru,en,ja}.json` |
| Items | `travel_card`, `lost_umbrella`, `lost_mitten`, `lost_scarf`, `lost_glasses`, `lost_keys`, `lost_bear` (LyudInventory.cpp) |

## The buffet

- The barista is registered with the counter service as a server. T at the barista tells the service which counter opens
  (`OpenedAt("hall:<station>")`), so buying there is the usual counter scene: the cup set down on the counter, the change
  counted, the barista's word.
- **The usual.** Whatever the hero buys at one hall's counter three times is "the usual" there (`FUsual`, `Times = 3`).
  The barista's prompt then adds «E — как обычно: …», and E buys it straight over the counter without the shop window.
  When he walks in, she greets him as a regular (`line.frontdoor.barista_regular.*`).
- **The morning line.** Behind the counter place, up to four spots are found when the player first comes near: one
  direction (away from the counter, along it, or diagonally) whose spots are clear of the hall's collision and of its other
  places. Before 10:30, six in ten riders of the station life join the line instead of sitting down. The hero standing on a
  spot holds it: nobody passes him (`JoinSpot`, `StepUp`). Every half second whoever has a free spot ahead steps up
  (walking). At the front the barista turns to them; after 45 to 85 s they take the cup to the nearest free table (or a
  standing place). A regular at the front gets «Вам как обычно?» (`line.frontdoor.usual_regular.*`).

## The departures board (Aton)

The cable car has no timetable: cabins run on call. So the board shows:

- cabins standing at Aton with their doors open this way: «ПОСАДКА»;
- cabins on their way that will stop here, at their arrival time: «ПОДХОД»;
- the station life's riders waiting in the hall, at the time they will call a cabin: «СКОРО»;
- when nothing runs, one row: how long a cabin would take if called now: «ВЫЗОВ».

Rows are the time, the destination, the cabin and the status. Every 4 s the board asks the rules again; letters that
change flip through a few others before they settle, and a new flip plays `board_flap` (audio/sounds.json; the sound is
generated on the Mac). The board is drawn only within 40 m, and redrawn every frame only while it flips.

Check: `tools/transportsim/run.sh board` samples a Monday every 30 s. Rows are sorted and inside the window; boarding
cabins are always shown; riders' departures are foretold 10 minutes ahead within 2.5 minutes on average.

## The travel card

- At Aton, E at the attendant without a card: she gives one (`frontdoor.card.prompt`, `line.frontdoor.card_give.*`).
- Every hall has a punch on the attendant's desk (a small green box). E there stamps the card with that station
  (`ULyudFrontDoor::Punch`, sound `ticket_punch`). The stamp goes to the collections as `stations/<station id>`.
- All seven make the set «Все станции», which pays the badge by the collections' rules (diary page, toast).

## The lost and found (Aton)

- A rack beside the attendant's desk. Each game day 0 to 2 residents' things turn up (seeded by the day, at most 4 on the
  shelf; a thing nobody takes goes after 5 days). The owners are the station life's riders through Aton.
- E at the shelf: the attendant says whose it is and when they usually come through. The hero carries it.
- Near its owner (in town or in a hall, within 2.6 m), the prompt «E — отдать …» appears. E hands it back: a thank-you
  line, a wave and a small gift (chocolate, candy, cookies, pie, postcard or seeds).

## Dusk

When the sun is under 3 degrees (back on over 5), the club's sign «АТОН» over the route board on the forecourt lights up,
with two warm shadowless lamps under it, and Aton's hall lamps are on within 250 m instead of 45 m, so the station shows
from across the forecourt.

## Console and developer mode

- `lyud.FrontDoor status | card | stamps | lost | usual | dusk 0|1|-1` (`dusk -1` returns to the sun).
- `Transport.Hall [station]` now ends with the front door's state: the line (`P` person, `H` hero, `.` free), the punch,
  the board and its flips, the shelf.
- F7 → Actions → «Атон: парадный вход клуба»: give the card, punch all stations, a thing to the shelf and to hand,
  the usual at Aton's buffet, dusk on/off.

## Still owed on the Mac

- Compile, then shots of Aton by day and at dusk (`lyud.FrontDoor dusk 1`), and the line in the morning (before 10:30).
- Item icons (`icon_travel_card`, `icon_lost_*` in localization/assets.json) and the `board_flap` sound.
- A bench at Aton against main.
