# The motor ship «Полдень» and the two-day cruise

Every weekend in season the motor ship «Полдень» (en "Polden", ja 「ポルデニ号」; the word means noon; home port Людмиловск)
takes the town on a two-day cruise round the Far Islands: boarding by launch from the pier head on Friday afternoon, the
Stone Brothers at sunset, the Needle's Eye at sunrise, two rounds of Bird Island, two hours ashore at the Far Light, a
night at anchor in Long Island's bay, the Seal Rocks, and home on Sunday evening. Aboard there is a cabin by ticket,
meals at one's own table, a jazz quartet that really plays, poker with the regulars, deck games, a library, a smoking
deck, a bridge tour, a radio room that sends telegrams, gulls to feed, thirty-five people living their own weekend and
fourteen quests.

**The trick.** The ship never moves. The hero's ship is a furnished city building (type `motor_ship`, plot `P-SHIP`)
standing still at the *cruise stage*, 26 km out at map (26000, 4000), on the sea datum, so everything of the town
interiors (seats, beds, sleep, shops, games, reading, staff talk, stays, link doors) works aboard unchanged. The voyage is
a pure function of the game calendar (`LyudCruise::FModel`); a seascape actor moves the islands past her by it, and a
wake, drifting waves, gulls, sounds and a slight sway make her feel under way. Between voyages a visual copy of her lies
at anchor on the roads off the town (chart 2000, 1500) and is seen sailing out and coming back. Boarding is by launch
from the pier head, which is a fade door: `P-PIER/launch_gate` <-> `P-SHIP/gangway`. Game time runs 1:1: the hero moves
through the 48 hours by sleeping, by acts that pass minutes, and by "until the next event" (deck chairs, the cabin bed).

**The prototype** is the Bulgarian-built Soviet project 543 «Алупка» type (Varna, 1958-65: Ай-Петри, Аджигол, Сарыч...;
63 x 9.3 m, about 100 berths, a small hold forward), adapted to 62.0 x 10.0 m. No real general-arrangement drawing could
be opened (every site with one refused automated access): the deck layout is reconstructed from the type's published
principal dimensions and room lists and the typology of 1955-65 small passenger ships, then widened for the game
(corridors 1.5 m, doors 1.1 x 2.1 m, gentle stairs). The research and the numbers: the R1 brief (`scratchpad/ship/R1_blueprints.md`,
not in the repo).

Nothing of the ship has been compiled or seen in Unreal yet: see [First Mac session](#first-mac-session) and [Open](#open).

## Files

Paths under `unreal/Lyudmilovsk/Source/Lyudmilovsk/` are written from `Public/` or `Private/`.

| What | Where |
|---|---|
| The ship's model (shell, decks, stairs, rails, fittings, livery, lettering) | `blender/city/buildings/motor_ship.py`, `_shipframe.py` (the ship frame, rooms, stairs, helpers: the API the furnishers import), `_shipkit.py` (fittings, `letters()`, `cloth()`, `extras()`, the pier's landing stage) |
| Furniture | `blender/city/buildings/_ship_inside_cd.py` (decks D and C), `_ship_inside_ba.py` (deck B, boat deck A, bridge) |
| Export | `blender/export/city/motor_ship.json` (+ `.glb`, not in git); the pier's landing stage is in `hyperboloid_tower.py` / its export |
| The Far Light's rock (the landing) | `blender/city/buildings/far_light.py`, `_rocknoise.py` (a static copy of the seascape kit's skerry function) -> `blender/export/city/far_light.json` |
| Plots | `tools/city_plan.py` (`motor_ship`, `far_light`, `SHIP_STAGE_X/Y`, `FARLIGHT_OFFSET`, plots `P-SHIP`, `P-FARLIGHT`) -> `Content/City/city_plan.json` |
| The cruise model (engine-free) | `Private/Cruise/CruiseModel.h`; offline `tools/cruisesim/` (`run.sh test\|table\|at <date> <time>`) |
| Its data | `tools/ship/cruise.py` -> `Content/Cruise/cruise.json`, `Content/Localization/ship/strings_{ru,en,ja}.json` (`cruise.*`), `Content/Dev/teleports/ship.json`; extra UI strings dropped in `tools/ship/strings/<name>.json` are merged |
| The cruise in the game | `Public/Cruise/LyudCruise.h`, `Private/Cruise/LyudCruise.cpp` (`ULyudCruise`, tickable world subsystem, save record `Cruise`) |
| City additions (flags, named waits) | `Private/City/CityThingsModel.h`, `Public/City/LyudCityThings.h`, `Private/City/LyudCityThings.cpp`; `tools/city/interiors.py` validates them; test `tools/citysim/flagchecks.h` |
| Scenery: islands, wake, the roads ship, gulls, sounds, hiding | `Public/Cruise/LyudCruiseScenery.h` (+ `ULyudShipSwayCamera`), `LyudCruiseSeascape.h`, `LyudShipWake.h`, `LyudMotorShip.h` and their `.cpp`; maths `Private/Cruise/SeascapeMath.h` (offline `tools/seascapesim/`), `Private/Cruise/ShipSeaMath.h` (hold, ride, sway) |
| The sea at the hull | `Public/Sea/LyudSea.h`, `Private/Sea/LyudSea.cpp` (`SetSurfaceDrift`, `SetTideHold`, `SetCalmFootprint`, `SetSeaStateCap`), `Private/Weather/WaveModel.h/.cpp` (`FCalmFootprint`), `tools/sea/sea_hlsl.py` (`Lyud.Calm`) -> `unreal/scripts/make_sea_materials.py`; offline `tools/sea/run.sh test` |
| Islands kit | `blender/ship/seascape_kit.py` -> `design/ship/game/seascape_kit.glb` (gitignored, 13 MB), `Content/Cruise/seascape.json`, `design/ship/textures/T_SS_rock_{C,N}.png` |
| Ship sounds (synthesized) | `tools/ship/ship_sounds.py` -> `audio/sfx/ship/*_01.wav` (11) |
| Import of the kit and the sounds | `unreal/scripts/import_cruise.sh`, `import_cruise.py` |
| The band | `Private/Music/ShipBandModel.h` (engine-free), `Public/Music/LyudShipBand.h`, `Private/Music/LyudShipBand.cpp`; tunes `Content/Music/band_tunes.txt`; titles `Content/Localization/shipband/`; offline `tools/bandsim/` (`run.sh`, `analyse.py`) |
| Poker | `Private/Cruise/PokerModel.h` (engine-free), `Public/Cruise/LyudShipPoker.h`, `Private/Cruise/LyudShipPoker.cpp`, `Private/Cruise/SLyudPokerPanel.h/.cpp`; strings `tools/pokersim/poker_strings.py` -> `Content/Localization/poker/`; offline `tools/pokersim/` |
| What to do aboard (acts, shops, stays, texts, topics, collections, room tone) | `tools/city/content/x_ship.json` (pier, decks C and D, cabins), `x_ship_saloon.json` (deck B inside), `x_ship_deck.json` (open decks, radio room, bridge), `x_farlight.json` (the rock); strings `tools/city/content/strings/x_ship*.json`, `x_farlight.json` -> `tools/city/interiors.py` -> `Content/City/interiors.json`, `Content/Localization/city/` |
| Quests and talk | `tools/ship/cruise_talk.py` (stages, rewards, dev blocks), `tools/ship/talk/*.txt` -> `Content/UI/quests.json`, `Content/Dialogue/npc_dialogue.txt` (`[Ship_<id>]`), `Content/UI/quest_scenes.json`, `Content/Localization/shiptalk/`, `localization/voice.json` |
| People | `tools/ship/cruise_people.py` -> `Content/PeopleData/ship_places.json`, `ship_activities.json`, `ship_people.json`, `Content/Localization/shippeople/`, rows of `manners.json`; barks `tools/barks/lines/18_cruise.txt`; `Private/People/PeopleSim.cpp/.h`; looks `blender/residents/looks.py` (`SHIP_CAST`); offline `tools/peoplesim/run.sh cruise` (`cruise_people.h`) |
| Items | `Private/LyudInventory.cpp`; icon prompts `localization/assets.json` (`icon_cruise_ticket_hold` ... `icon_cruise_diploma`) |

Every generated file above is rebuilt by its generator, never edited by hand (order: [Offline tests](#offline-tests-and-the-generators)).

## The ship

Ship frame (used by every ship file): metres, x from the stern (0) to the bow (62), y to starboard positive, z from the
waterline. The building frame is `bx = x - 31`, `by = 5 - y`, `bz = z - 1.5`. At the stage there is no rotation: the
ship frame's origin is at UE cm (2 596 900, 400 790, -4300), bow to +X, starboard to +Y, the waterline on the sea datum
(-43 m). The plot `P-SHIP`: kind `ship`, map (26000, 4000), yaw 90, 62 x 10 m.

**Hull and livery.** Lofted from 41 stations: raked stem, cruiser stern, sheer forward and aft, flare at the bow, keel
2.8 m under the waterline. Deep navy hull and bulwarks (`CT_ShipNavy`), red boot-topping, a white sheer stripe, white
superstructure; cream funnel, masts and ventilators (`CT_ShipCream`), the funnel's red band with a brass sun disc and a
black top; green deck paint on the steel foredeck and aft mooring deck; planked decks with the boards fore and aft. ПОЛДЕНЬ
in raised white letters on both bows, ПОЛДЕНЬ and ЛЮДМИЛОВСК round the stern, brass ПОЛДЕНЬ on the bridge-wing boards and on
all 14 life rings (`_shipkit.letters()`, glyph table `GLYPHS`: add a glyph before writing a new letter; only the ship's name
and home port are written on the mesh). An ensign in the funnel's colours, a house pennant, three signal flags (static
two-sided cards, `cloth()`); draught marks, load-line disc, freeing ports, fenders, coiled lines, mushroom vents, the fire
point, boat No. 1 (forward starboard) with its cover half rolled back.

**Five walkable levels** (z of the floor over the waterline):

| Deck | z | What is there (x from the stern) |
|---|---|---|
| D, lower | 0.30 | stair-foot lobby; cabin 15 (the 4-berth hold cabin, two steel two-tier bunks), cabins 16-20 (closed); showers (three stalls); the engine room; baggage room, hold door; crew quarters aft x 8-18 (closed box: 16 crew berths, 8 mess seats) |
| C, main | 2.80 | dining saloon x 8-18 (nine tables for four, the captain's table aft on the centreline); servery and galley x 18-27 (looked into over a half-door); the engine gallery over the diesel's well (six-cylinder diesel, two generator sets, switchboard); vestibule x 27-32 (purser's desk, key board, kiosk, notice board, post box, the `gangway` door, `launch_ashore`); cabin corridor x 32-46 (cabins 1-10; 1, 2 and 7 furnished); cargo hold and hatch; foredeck (bell on the foremast, windlass, bow rail); aft mooring deck (capstan) |
| B, promenade | 5.40 | smoking deck aft (4 benches, 2 ash urns, ring toss, `stern_rail`); lux cabins 11-14 x 12-18.5 (11 furnished); side promenades (3 benches a side); the stair lobby with the library strip (2 glazed bookcases, globe, chess table) and the notice niche (programme board, honour board, ship model); the music saloon x 33-44.7 (L-shaped bar with 5 stools, card table with 5 chairs, radiogram, parquet dance floor 4.1 x 3.2, bandstand 0.2 m high with piano, bass, drums, clarinet and sax, 24 seats); open deck round the saloon's front |
| A, boat deck | 8.00 | shuffleboard court, 8 striped deck chairs, funnel benches, lifeboats on davits, muster board by boat No. 1 (12 muster spots), radio room (served over a counter flap in its doorway), officers' house with the captain's door |
| Bridge | 10.60 | wheelhouse (wheel, engine telegraph, binnacle, radar, voice pipes, flag locker, whistle pull, clocks), chart room (chart table, chronometer), two wings with a pelorus each. Wheelhouse roof 12.90, funnel top 13.40 |

Stairs: the main stair D-C-B-A in the well x 27-31 (two flights a storey, 16 risers of about 0.16 m); outside stairs aft
C-B (starboard) and B-A (port); two ladders A-bridge. Headroom 2.17 m or more on the walk grid. Every room is a closed box
with a door hole of at least 1.10 x 2.10 m; open passages are holes, closed doors are solid wall with a leaf drawn on it
(lists in the export's notes, `OPEN_DOORS`, `CLOSED_DOORS`). Rails 1.10 m along every deck edge, stair and well (thin bars in
the collision shell: the kit has no invisible collision material). The two gangway ports have 1.1 m steel gates: one
leaves by the `gangway` thing, not over the side. The boat deck forward of the bridge ladders (x > 35.3) is closed by
rails: the crew's end (stowed ladder, lifejacket chest).

Numbers (last build): shell 23 255 triangles (exact collision), detail 81 571 (about 33 200 deck C/D furniture, 31 400
deck B/A/bridge, the rest shell fittings), 584 window cards (146 windows), 24 lamps of the kit's 24 (W2/W3 added only opal
fixtures), 223 things, 479 spots. Lint: 0 stretched UVs, 0 degenerate.

**Cabins.** 20 passenger cabins: 1-10 on C, 11-14 (lux) on B, 15-20 on D. Furnished: 1 (the geography teacher's: globe,
books, maps), 2 (бабушка Фаина and Сеня; the honeymoon dressing is one constant away, `CABIN2_STYLE`), 7 (the hero's
standard cabin: four berths, two upper with ladders, nobody's luggage), 11 (the hero's lux: twin berths, desk, washbasin),
15 (the hero's hold cabin). The others are closed doors with captions and berth spots behind them (people "are in their
cabin"). Bed things `cabin<N>_bed_<a..d>`, `lux11_bed_a/b`: the position is the mattress top; upper berths stand 1.46-1.57 m
up while the thing's `height` is capped at 1.2 (use the position's z).

**Why the decks are lifted.** R1 put deck D 1.2 m under the waterline. Nothing masks the sea inside a hull (`ALyudSea`
is one wave surface plus a water volume at sea level), so the water would have filled it. Every deck was lifted instead:
deck D's floor is 0.30 m over the waterline and the others follow (storeys 2.5-2.6 m, a little lower than R1's); the hull
still goes 2.8 m under the water, so from outside she sits as drawn. Waves higher than 0.30 m are kept out by the calm
patch (see [The sea at the hull](#the-sea-at-the-hull)).

**The pier.** On plot `P-PIER` (`hyperboloid_tower.py`): a balcony at the pier head's seaward edge, 14 steps down to a railed
landing stage 2.3 m under the pier, the ticket booth (`ship_ticket_box`), the timetable board (`ship_timetable`) and the
`launch_gate`; spots `ship_ticket_queue`, `launch_wait_1/2`.

A furnisher that throws does not break the build: the shell prints `SHIP: <module>.<function> FAILED` into the notes.
`SHIP_SHELL_ONLY=1` builds the shell alone; `KIT_CHECK=1` prints the kit's checks.

## Timetable and route

**Season** 15 April - 15 November (`cruise.json` `season`). Out of season the ship is laid up and not shown; the ticket box
and the launch gate say so. One voyage a week, every Friday whose three days lie in the season (none is ever cancelled):
boarding Friday 16:00-18:00, sails 18:00, back on the roads Sunday 18:00, landing until 20:00. 338 km at 2.5-5.8 knots
(top 2.96 m/s); `bash tools/cruisesim/run.sh table` prints the whole voyage.

**Phases** (`ELyudCruisePhase`): `Laid` (out of season), `Port` (on the roads between voyages), `Boarding`, `Underway`,
`Circling` (round Bird Island), `Anchored` (the Far Light stop and the night in Long Island's bay), `Landing`.

**The programme** (event ids are a contract; `cruise.json` `events`; one-time events stay on for 10 minutes):

| Day | Time | Event id | What | Where |
|---|---|---|---|---|
| Fri | 16:00-18:00 | `boarding` | the launch runs from the pier head | gangway |
| | 18:00 | `departure` | three long blasts, a march on the loudspeakers | deck |
| | 18:30-19:00 | `drill` | lifeboat drill, muster at the boats | boat deck |
| | 19:30-20:30 | `dinner1` | dinner | dining |
| | 20:15-20:45 | `isl_brothers` | the Stone Brothers at sunset, to starboard | deck |
| | 21:00-23:00 | `concert1` | the Noon Quartet | saloon |
| | 22:00-01:00 | `cards1` | the card table | saloon |
| | 23:00 | `lights_out` | | cabin |
| Sat | 05:15-05:45 | `isl_arch` | the Needle's Eye at sunrise, to starboard | deck |
| | 07:00 | `wake` | reveille | cabin |
| | 07:15-07:45 | `exercise` | morning exercises | boat deck |
| | 08:00-09:00 | `breakfast2` | | dining |
| | 10:00-11:30 | `isl_birds` | Bird Island, two rounds | deck |
| | 11:30-13:00 | `deckgames` | shuffleboard and ring toss tournament | boat deck |
| | 13:00-14:00 | `lunch2` | | dining |
| | 14:00-15:00 | `quiet` | quiet hour | cabin |
| | 15:00-16:00 | `bridge_tour` | the bridge is open to passengers | bridge |
| | 16:00-16:30 | `tea2` | tea | dining |
| | 16:30-18:30 | `isl_lighthouse` | at anchor off the Far Light, the launch to the rock | deck |
| | 19:00-20:30 | `captains_dinner` | the captain's dinner (soft trio) | dining |
| | 20:30-23:30 | `concert2` | the big evening with dancing | saloon |
| | 22:00-01:00 | `cards2` | the card table | saloon |
| Sun | 00:00-06:00 | `anchor` | the night anchorage in Long Island's bay | deck |
| | 07:00 | `wake` | reveille (`IsEventOn("wake")` holds on both mornings) | cabin |
| | 08:00-09:00 | `breakfast3` | | dining |
| | 10:00-10:45 | `quiz` | | saloon |
| | 10:45-11:15 | `isl_seals` | the Seal Rocks | deck |
| | 13:00-14:00 | `lunch3` | | dining |
| | 15:00-15:30 | `diplomas` | diplomas at the honour board | saloon |
| | 16:30 | `home_in_sight` | the town in sight | deck |
| | 18:00 | `arrival` | back on the roads, one blast | deck |
| | 18:00-20:00 | `landing` | the launch takes people ashore | gangway |

**The islands** (chart = the map's metres frame carried out to sea, X west, Y north; world cm = (X·100, Y·100 + 790, Z)).
All lie north of the sea railway (Y -60): the route never crosses it.

| id | Name ru / en | Chart X, Y | What | How the ship meets it |
|---|---|---|---|---|
| `home` | рейд Людмиловска / the Lyudmilovsk roads | 2000, 1500 | the anchorage off the town (the ship lies at 2012, 1451, heading 109°) | boarding and landing; the home coast is an impostor beyond the roads |
| `brothers` | Каменные Братья / the Stone Brothers | 12000, 7000 | three sea stacks up to 40 m | passed 520 m off to starboard, Fri 20:15-20:45 |
| `arch` | Игольное Ушко / the Needle's Eye | 28000, 16000 | a rock arch, 32 m; its eye is turned at run time to the low sun | passed 380 m off to starboard, Sat 05:15-05:45 |
| `birds` | остров Птичий / Bird Island | 45000, 12000 | a 70 m cliff island 600 x 260 m, seabird colony | circled twice at 800 m, Sat 10:00-11:30 |
| `lighthouse` | маяк Дальний / the Far Light | 52000, 30000 | a lighthouse on a bare rock, 38 m; its light at night | at anchor 332 m off, Sat 16:30-18:30, the landing |
| `long` | остров Долгий / Long Island | 30000, 38000 | 2.5 km long, a sheltered bay (socket at 30000, 38850) | night anchorage Sun 00:00-06:00 |
| `seals` | Тюленьи камни / the Seal Rocks | 11000, 24000 | low rocks awash, seals lying on them | passed 520 m off, Sun 10:45-11:15 |

The route is a closed belt of circles joined by tangents (`home`, `via`, `pass`, `orbit`, `anchor` stops in `cruise.json`
`route`): position and heading are continuous everywhere, speeds ramp smoothly (`ramp_s` 300), the turn rate is smoothed
over 40 s. The first leg (Friday 18:00-20:15) is slow on purpose, 1.3 m/s; if it looks dead, move the via (7000, 4800)
further out.

**The landing at the Far Light.** On Saturday the ship anchors at chart (52024, 29669), heading 0, with the rock (52000,
30000) abeam to starboard. The stop is route kind `anchor` with `"ashore": true`; its two neighbour via points mirror each
other so she lies exactly there at heading 0 (cruisesim checks it to a centimetre): do not move one without the other.
The rock is a real city building, type `far_light`, plot `P-FARLIGHT` (kind `ship`, yaw 90, 222 x 144 m), placed at a fixed
offset from the stage ship's midship (-24 m to the bow, +331 m to starboard: `FARLIGHT_OFFSET` in `tools/city_plan.py`,
`ASHORE.offset_m` in `tools/ship/cruise.py`; `cruise.py` fails if the plan disagrees, so moving `P-SHIP` moves the rock too).
At the stop the real rock stands exactly where the seascape draws its far copy (mismatch under 1 m). On it: a jetty, a
levelled path, the tower (ground room, a watch room at +18.6 m, a gallery at +22.75 m with the lantern and lens), the
keeper's furnished one-room hut, a fog bell with its clockwork, an oil store, a weather box, a flagpole, a walled
vegetable patch, a rain tank, a bird ledge (shell 12 474 triangles, detail 3 328). The keeper August Tamm talks at his hut
table (`keeper_table`, 8 topics), the log book has five pages (`farlight_log`), a carved bird is his gift, and the
collection `farlight` has five stamps (memorial plate, gallery, lens, log, bird). Content: `tools/city/content/x_farlight.json`
(33 acts `farlight_*`, 115 strings).

- The launch: thing `launch_ashore` by the gangway on deck C and `jetty_gate` on the rock are link doors to each other,
  open under `cruise_ashore_open` (from 5 minutes after anchoring to the last launch). The crossing is a fade.
- Coming back: the bosun warns three times from 18:00 (`warn_s`); the last launch is 18:20. A hero still on the rock when he
  may no longer be there is faded to the gangway with one line. Swimming off the rock for 2.5 s puts him back on the jetty.
  Man overboard and "left the ship" do not fire while he is on the rock.
- The hand-over: the real rock is hidden with collision off except (a) while the ship lies at the stop, (b) while the hero
  stands on it, (c) near a hero who is no passenger (within 1.2 km: a dev teleport). While it is shown the far copy hides
  its mesh but stays "shown", so its lamp and beam keep working (`ALyudCruiseSeascape::SetRealRock`, piece `lighthouse`).
- The rock's shape comes from `_rocknoise.py`, a copy of the kit's skerry function: change both together.

## Getting aboard

**Tickets** at the pier's ticket box (`P-PIER/ship_ticket_box`, shop `ship_tickets`, 10:00-18:00, in season, while the hero
has no ticket and is not aboard; otherwise the box says why in words):

| Item | Price | Class | Cabin | Stay |
|---|---|---|---|---|
| `cruise_ticket_hold` | 90 ₽ | hold | 15 (deck D, 4 berths) | `cruise_hold` |
| `cruise_ticket` | 150 ₽ | std | 7 (deck C, 4 berths) | `cruise_std` |
| `cruise_ticket_lux` | 300 ₽ | lux | 11 (deck B, lux, 2 berths) | `cruise_lux` |

Meals are included (every dining chair has `ship_meal`). The hero's places: `dining_chair_1` at table 1 (starboard aft by the
window) with Вера Игнатьевна, Сеня and бабушка Фаина; the fourth chair of the captain's table (`dining_chair_40`) is left
free for an invitation.

**Boarding.** The launch gate's act `ship_launch_out` holds under `cruise_can_board` (boarding hours, a ticket in the
pocket, not aboard); otherwise `ship_launch_words` says which: laid up, away, not today, landing, no ticket. When the hero
arrives through the gate, `ULyudCruise` takes one ticket (the best class in the pocket: lux, then std, then hold), gives
`cabin_key`, starts the stays `cruise` and `cruise_<class>` (3 days when boarded on Friday) and the purser greets him.
His berths need the stay `cruise` and his class flag (acts `ship_berth_hold|std|lux`); any other berth says «чужая
каюта». A teleport aboard with a ticket during a voyage counts as boarding (half a second later); without one he is a
stowaway (a line, nothing else).

**Stays.** `cruise` (all aboard), `cruise_<class>`, and `cruise_bridge` for one day: the wheelhouse things (`ship_helm`,
`ship_telegraph`, `ship_chart`, `ship_radar`, `ship_binnacle`, `ship_wing`, voice pipes, flag locker, whistle,
chronometer) need it, without it they say «Посторонним вход воспрещён». It is started by the act `ship_bridge_tour` at the
captain's door during `bridge_tour`, by the code at `bridge_tour`, while the quest `ship_captain` stands at stage `wheel`,
and by `lyud.Cruise.BridgePass`. All stays end ashore.

**Gates out.** The gangway's act `ship_gangway_out` holds only under `cruise_landing`; at other times the gangway's words
say it is shut. During the landing (Sunday 18:00-20:00) the hero leaves by the gangway; at 20:00 anybody still aboard is put
ashore with a fade and a word from the bosun (asleep in a berth he is left to wake first). Going ashore takes `cabin_key`
back, ends the stays and counts the voyage as sailed (also by `lyud.Cruise.Ashore`). A save of another week loaded aboard
puts him ashore after 5 s. If `P-PIER/launch_gate` is missing the fallback is map (311.3, 905.0) dropped onto the ground.

**Man overboard.** Feet 30 cm under the waterline, or swimming outside the hull's outline (`cruise.json` `hull.plan_cm`),
for 2 s: a fade, and he is back on the aft deck with a line. A hero standing on something above the water outside the
outline is left alone. Not judged while he is on the Far Light's rock (its own rule above).

## What to do aboard

92 acts (all `ship_*`) on 207 of the ship's 222 things plus the three pier things; the other 15 keep a generic town act
(clocks, cabin switches, the shower tap, chess seats). 546 strings in each of ru, en, ja; 24 staff topics; six texts
(timetable, programme, ship history, «Лоция островов», drill card, ship's paper). Lines change by voyage day, hour,
weather and the cruise flags (`cruise_at_<island>`, `cruise_ev_<event>`, underway, anchor, port, class).

| Where | What |
|---|---|
| Pier head | ticket box, timetable board, the launch |
| Vestibule (C) | Нонна at the purser's desk (`ship_purser`: topics ship, history, islands, crew, lost, telegram, chairs, diploma; "what's next in the programme?" passes the minutes to the next event); the kiosk `ship_kiosk` (ship postcards, postcards, stamps, bubliki, sweets, chocolate, notebook, film, lighter, cigarettes, handkerchief); the post box (`ship_post_box`, needs a `ship_postcard`, once a day); notice board, key board, barometer, route map |
| Dining saloon (C) | meals at one's chair during `dinner1`, `breakfast2/3`, `lunch2/3`, `tea2`, `captains_dinner`; the buffet hatch `ship_buffet` (Зоя, 07-21: tea, milk pyramid, kefir, vatrushka, pie, bubliki, apple) |
| Galley half-door (C) | peel potatoes with тётя Паша (15 min, once a day, a `pie`; topics `ship.cook.*`) |
| Engine gallery (C) | the diesel and its gauges (`ship_engine_gauge`), Эйнар's topics `ship.engine.*` |
| Foredeck, aft deck (C) | the ship's bell (`ship_bell`), the bow rail look-out (`ship_bow`), windlass (`ship_bosun_store`), cargo hatch (`ship_hatch_listen`: at night, meowing), hold door, capstan |
| Cabins | berths (sleep, or doze "until the next event"), porthole / lux window look-outs, washbasin, wardrobe, cabin loudspeaker (`ship_speaker`, speaks by event), radio, light switch; showers on D |
| Music saloon (B) | the bar `ship_bar` (Гиви, 10-01: milk cocktail, Буратино, Дюшес, Тархун, Байкал, kvass, tomato juice, coffee, tea, water, chocolate, ice cream, sandwich: no alcohol for the hero); the concert from a seat (`ship_concert`); the dance floor (`ship_dance`, a rally game, only during `concert2`; a waltz danced through is a win); the piano and the radiogram when the band is not on; the card table (`ship_cards`, opens the poker); the quiz (`ship_quiz` at `saloon_seat_1`, during `quiz`, a candy); sofas look out |
| Library and lobby (B) | the sea bookcase (`ship_library_sea`: reads the pilot book, the ship's history, the drill card in turn; collection `ship_books`, 3 pieces), the classics bookcase (opens the town's book reader), chess with Лев or Вера (`ship_chess`), the programme board, the honour board (`ship_diploma` gives the `cruise_diploma` during `diplomas`), the ship model in its case, the globe |
| Promenades and smoking deck (B) | benches look out (`ship_look_bench`), life ring; the smoking deck's benches and ash urns (smoking is the town's own cigarettes, `docs/smoking.md`; the kiosk sells them; the hero is offered no alcohol anywhere); ring toss (`ship_ring_toss`, an aim game); feed the gulls at `stern_rail` (`ship_gull_feed`, needs `bubliki`, one is used up; stamp `cruise/gulls`) |
| Boat deck (A) | shuffleboard (`ship_shuffleboard`, aim game against a regular), deck chairs (`ship_look_deckchair`: lie and doze until the next event, 3 h at most), the funnel bench (`ship_count_birds`: count guillemots off Bird Island), lifeboat No. 1, the muster station (`ship_muster` during `drill`), the radio desk (`ship_telegram`, 5 ₽, once a day, 09-20; Лев's topics `ship.radio.*`) |
| Bridge | the captain's door (`ship_bridge_tour` during the tour); with the bridge pass: the wheel (`ship_helm`, a rally game "держать курс"), telegraph, chart table, radar, binnacle, the wings' views |

**Collections.** `cruise`, 24 stamps (the six islands, seven meals, drill, deck games, bridge, telegram, postcard, gulls, the
two concerts, dance, stars, dawn): complete, it pays a `badge`. `ship_books` (3 texts read). `poker` (13, given by the poker's
code; complete, 50 ₽). `farlight` (5, on the rock). Deck-game stamps come only for a win during the tournament.

**Regulars** for the city games: Лев and Вера at chess, Вера, Сеня and Миша at shuffleboard. Room tone `ship_room` on
`P-SHIP` (the diesel's hum through the deck, a creak or a far door now and then).

**Items** (all in `LyudInventory.cpp`, names and descriptions in ru, en, ja): `cruise_ticket_hold`, `cruise_ticket`,
`cruise_ticket_lux`, `cabin_key`, `milk_cocktail` (a drink), `ship_postcard`, `cruise_diploma`. Their seven icons do not
exist yet (prompts pinned in `localization/assets.json`).

### Poker

At the card table in the music saloon, during `cards1` and `cards2` (22:00-01:00 on both evenings), the regulars play
five-card draw, fixed limit; the hero sits on the free chair (`card_chair_1`) and plays against three or four of them. A
real game (`LyudPoker::FTable`) in a Slate panel at the screen's foot.

The table choice: «1 — на интерес: на спички, по 50 каждому. Проиграть нечего, кроме чести.» / «2 — по маленькой: вход
30 руб., анте 1 руб., ставки 2 и 4 руб.» / «Больше 60 руб. за вечер здесь проиграть не дают.», then one line about each
regular at the table tonight. Each regular brings 40 roubles for the night; three raises a round at most; up to 24 hands a
sitting, about four game minutes each; at 01:00 the table breaks up.

The rules page (key H, `poker.help.0..6`), as the player reads it:

> Покер с обменом: правила
> Все ставят анте и получают по пять карт. Круг ставок: пас, чек или уравнять, поставить или поднять.
> Потом каждый меняет до трёх карт, второй круг ставок (ставки вдвое крупнее) и вскрытие.
> По старшинству: пара, две пары, тройка, стрит (пять подряд), флеш (пять одной масти),
> фулл-хаус (тройка и пара), каре (четыре одинаковых), стрит-флеш, флеш-рояль.
> Кому нечем ответить на ставку, идёт на все и спорит только за свою часть банка.
> Смотри не только в карты, но и на людей: каждый выдаёт себя по-своему.

Keys: 1 fold, 2 check / call, 3 bet / raise; at the draw A / D pick, W mark (up to three), E change; E the next hand (or
hurry the others); S or Esc get up (Esc in a hand folds first); H rules; Q watch the dealer's hands (quest mode). The draw's
instruction is shown on the banner row across the panel's width (it ran over the log at the hero's seat). Every line is
fitted to its width (set smaller down to 8 pt, clipped only past that).

Who sits: Альберт all evening; Тарас until 00:30; Лев Маркович until 23:30; Вера Игнатьевна until 23:00; Блюм after his
concert (23:10 on Friday, 23:40 on Saturday); Гиви after 23:00 when a place is free. Four at most. Each plays and gives
himself away his own way: Альберт plays few hands, hard, and his face says little; Гиви plays almost anything, and the worse
his cards the longer his story; Вера Игнатьевна is careful and believes a raise; Тарас is bold, bluffs often and lies badly;
Блюм plays by ear; Лев Маркович counts the odds. The line under the seats says what a man does with his hands; it is
usually, not always, true, and reading it pays (offline: a tells-reader wins 47.4 chips per 100 hands against 19.5 for the
same play without them).

The regulars never see the hero's cards. The exception is the quest: while any quest stands on a stage with the id
`watch_hands` (or after `SetQuestMode("sharp")`), Альберт marks the cards on the hands he deals and his draws come out too
well; Q while he deals catches him (`card_sharp_caught`, stamp `poker/sharp`), and he never sits at the table again.

Money: the buy-in leaves the pocket on sitting and the stack comes back on standing, on every way out (S, Esc, another
screen, the table breaking up, 24 hands, going broke) through one settlement. A save in the middle of a roubles sitting
stores the stack (`SavedStake`) and the load pays it back and takes it off the evening's loss; a hand in play at the save
counts as folded. The level ending pays the stack quietly (no quest events, no stamps). Saved as `ShipPoker`.

### The band

«Полуденный квартет» (piano, double bass, brushes, clarinet / tenor sax) is not recorded: `LyudBand::FPlayer` plays lead
sheets from `Content/Music/band_tunes.txt` live. Voices: the piano is `LyudInstr::FSynth` with its top lifted; a plucked bass
(decaying harmonics, thump, finger squeak, slides); a reed of 12 harmonics, clarinet (odd harmonics) or sax (all, a formant,
more breath), with scoops, slurs and late vibrato; brushes, ride, hi-hat foot, soft bass drum, crash from filtered noise; a
Schroeder room and a soft limiter. The arranger plays an intro, the choruses of the tune's `plan` (head, solos, trading
fours), a tag and a held last chord; walking or two-beat bass, waltz, habanera; rootless comping, stride for rags,
oom-pah-pah, tango marcato; solos in four-bar phrases hung on the tune's notes. Every bar is a pure function of (tune, seed,
bar number) and the set's position comes from the voyage clock (`LyudBand::Locate`), so after a sleep, a clock jump or a load
the band is in the bar it would have reached. 1.4-1.9 % of one core; no allocation while playing.

In the game `ULyudShipBand` plays the set named after the programme event (`concert1`, `captains_dinner`, `concert2`) while
the hero is aboard, from a 3D sound at `GetShipPoint("bandstand")`, rendered in stereo and played as one mono source (the
four players' left-to-right placement is folded in): full within 6 m, falling over 50 m; half as loud and low-passed at
4.5 kHz outdoors on the same deck; 0.2 and 1.5 kHz on the boat deck; 0.28 and 900 Hz on the main deck; 0.1 and 600 Hz on the
lower deck. At the captain's dinner the trio is relayed to the point `dining` when the hero is on the main deck. A tune's
title is shown once (`ULyudCruise::Say`, speaker `band.name`) if he is in the saloon as it begins. The town's music cues are
held meanwhile.

Sets: `concert1` saints, old_folks, entertainer, st_james, oh_susanna, londonderry, korobeiniki, ochi (22 s pauses, a 10 min
interval, then again with another seed; 36 min a round). `captains_dinner` (style soft: a trio, the reed only in the last
chorus): londonderry, sakura, aura_lea, amazing_grace, greensleeves, old_folks, brahms_lullaby (27 min). `concert2`:
blue_danube, daisy_bell, choclo, korobeiniki, oh_susanna, saints, ochi, greensleeves, aura_lea, brahms_lullaby (the last
dance), auld_lang_syne (farewell) (43 min).

The tunes (the rule: traditional, or the composer dead by 1955; nothing is sung):

| id | Title | Composer | Died | Feel |
|---|---|---|---|---|
| `saints` | When the Saints Go Marching In | traditional (in print 1896) | | swing 184, clarinet |
| `entertainer` | The Entertainer (first strain) | Scott Joplin | 1917 | ragtime |
| `old_folks` | Old Folks at Home / Swanee River | Stephen Foster | 1864 | swing 126, sax |
| `oh_susanna` | Oh! Susanna | Stephen Foster | 1864 | foxtrot 176 |
| `st_james` | St. James Infirmary | traditional | | slow swing 92, sax. **To check** |
| `londonderry` | Londonderry Air | traditional (Petrie 1855) | | ballad 63 |
| `korobeiniki` | Коробейники, as a tango | traditional | | tango 132 |
| `ochi` | Очи чёрные | Florian Hermann («Valse hommage», 1884) | 1892 (as usually given, unverified) | waltz 126. **To check** |
| `sakura` | Sakura, Sakura | traditional | | ballad 72 |
| `aura_lea` | Aura Lea | George R. Poulton | 1867 | slow foxtrot 96, sax |
| `amazing_grace` | Amazing Grace ("New Britain") | anonymous, 1829 | | waltz 96 |
| `greensleeves` | Greensleeves (verse) | traditional | | waltz 150 |
| `brahms_lullaby` | Wiegenlied op. 49/4 | Johannes Brahms | 1897 | slow waltz 84 |
| `blue_danube` | The Blue Danube, waltz no. 1 | Johann Strauss II | 1899 | waltz 174 (last 8 bars simplified) |
| `daisy_bell` | Daisy Bell | Harry Dacre | 1922 | waltz 168 |
| `choclo` | El Choclo (first strain) | Ángel Villoldo | 1919 | tango 126. **To check** |
| `auld_lang_syne` | Auld Lang Syne | traditional | | slow foxtrot 100, sax |

Every sheet was written from memory; no printed score could be reached. The three **to check** have the right harmony, but a
musician must compare the melody with a score and fix their `notes =` lines (`bandsim` re-checks them). Left out because the
notes were not sure (all allowed by the rule): «На сопках Маньчжурии», «Амурские волны», «Осенний сон», La Cumparsita,
Careless Love, Frankie and Johnny, Maple Leaf Rag, Indiana.

Format of `band_tunes.txt`: `[id]` blocks with `composer`, `died`, `source`, `bpm`, `meter`, `feel`, `key`, `reed`, optional
`swing`, `plan`, `pickup`, `form = A A B`, `A: notes = E4/0.5 r/1 | ...` (bar lines checked), `A: chords = C/4 G7/2 ...`;
`[set <event id>]` with `tunes`, `pause`, `interval`, `style = soft`. Titles: `band.tune.<id>.title`.

### Deck games and the library

Ring toss (smoking deck) and shuffleboard (boat deck) are the city's aim games, any hour of the day; the dance and the wheel
are rally games. A win during `deckgames` gives the stamp `cruise/deck_games`. The sea bookcase reads the ship's own texts in
turn; the classics case opens the town's book reader (`docs/books.md`). The quiz is a narrated act, not a question screen;
the drill is one act at the muster station, not a staged scene.

## People

35 people live aboard on the weekly voyage in season: 11 crew, the quartet, 8 tourists and 6 cruising townsfolk a weekend
(12 in two sets that alternate by week). They are data (`Content/PeopleData/ship_*.json` from `tools/ship/cruise_people.py`,
which reads the ship's export and plot `P-SHIP` every run and finds every chair, berth, bench, rail and staff spot by id).
The ship is island 4 of the people's life: nobody walks between her and the town. Her places: `P-SHIP` (crew posts, slots
tagged by role: what `StaffOnShift("P-SHIP", role)` counts), `P-SHIP-DINING`, `-SALOON`, `-LIBRARY`, `-CARDS`, `-DECK`,
`-ROUND`, `-CORRIDORS`, cabins `P-SHIP/c-NN`, crew berths `P-SHIP/k-NN` (private).

**Crew:** captain Арсений Павлович Громов (`ship_gromov`, 60, calm, dry humour, ex-Arctic); chief mate Ирина Сергеевна Вайс
(`ship_vais`, 38, exact, astronavigation); bosun Тарас Нечипоренко (`ship_taras`, 52, loud, kind, knots); purser Нонна
Аркадьевна Лисицкая (`ship_nonna`, 47, knows everyone); barman Гиви Цулукидзе (`ship_givi`, 44, storyteller, never serves the
hero alcohol); stewardess Зоя Ким (`ship_zoya`, 23, studies by correspondence); cook тётя Паша (Прасковья Ильинична,
`ship_pasha`, 58); radio operator Лев Маркович Штерн (`ship_lev`, 66, Morse, telegrams); chief engineer «дед» Эйнар Каск
(`ship_einar`, 57, Estonian, taciturn); helmsman Витя Чиж (`ship_vitya`, 19, first season); entertainer Римма Вольская
(`ship_rimma`, 35). **The band** «Полуденный квартет»: Савелий Блюм, piano, leader (`ship_blum`, 61); Отар, double bass
(`ship_otar`, 40); Костя Ёж, drums (`ship_kostya`, 27); Аста Линд, clarinet and sax (`ship_asta`, 31). **Tourists:** the
retired geography teacher Вера Игнатьевна (`ship_vera`, cabin 1); the honeymooners Миша and Оля (`ship_misha`, `ship_olya`,
cabin 12); the card sharp Альберт, "the engineer from Riga" (`ship_albert`, 13); the birdwatcher Харуто Мори with a telescope
(`ship_haruto`, 14); Сеня, 10, and his бабушка Фаина (`ship_senya`, `ship_faina`, cabin 2); the photographer Инга (`ship_inga`,
3). The ship's cat Боцман lives in the hold (a quest; no cat model).

**Cruising townsfolk** are twins: `c_<name>` (in `ship_people.json`, `twin_of`, the same pooled body) lives aboard, while a
`"patch": 1` entry gives the town record `t_<name>` a fortnightly stay: it waits at the pier head (`S-PIER-CRUISE`) from about
15:50 on Friday, is gone from about 16:36 and is back on the pier Sunday 18:30-19:15. The two are never in the world at once;
`townsfolk.json` is not touched. Set A (even weeks): Карим and Колетт Озтюрк (cabin 4), Дмитрий Новотный and Ананья О'Брайен
(5), Тигран Наварро (6), Париса Лунд (8). Set B (odd weeks): Карим Ковальский and Шира Накамура (9), Инеш Зелинская (10), Акосуа
Грабовская (16), Урсула Баккер (17), Джихун Вишневский (18). None has a garden, a pet, a counter job or a station trip; to change
who cruises, edit `TWIN_SETS` in the generator.

**The day aboard.** Crew come aboard Friday 14:00, passengers 16:12-17:15 (gangway, then their cabin); everybody is at the
muster station at the 18:30 drill, at their own dining chair at every meal, at the rails at the islands, in the saloon at
the concerts and the quiz, in their own berth at night; passengers go ashore Sunday 18:12-19:10, the crew at 20:00. Between
fixed hours people choose among the decks, rails, benches, deck chairs, bar, saloon, library and smoking deck. Out of the
voyage and out of season (the life's condition `cruise_season`, sent by `ULyudCruise` every 2 s) nobody is aboard.

- Watches: Громов 08-12 and 16-19, Вайс 12-16 and 00-08, Витя at the wheel 08-12 and 18:30-00:10: not a minute of the 42 hours
  under way without a watch in the wheelhouse.
- Counters: `purser` Нонна 08-23, `barman` Гиви 10-01, `steward` Зоя 07-21 except 09:30-11:00 Saturday and Sunday (she does the
  cabins), `cook` тётя Паша 06-20, `radio` Лев 08-20; on Friday each from about 14:10. Also roles `captain`, `mate`,
  `helmsman`, `bosun`, `engineer`, `entertainer`, `band_piano|bass|drums|reed`.
- Left free for the hero, never taken: cabins 7, 11, 15; `dining_chair_1`, `dining_chair_40`; `card_chair_1`; `saloon_seat_1..4`,
  `bar_stool_1`, `deck_chair_1/2`, `library_chair_1`, `prom_bench_1`, `smoking_bench_1` (the list is `free` in `ship_places.json`).
- Reach on paper (staff spot to the nearest visitor point): Паша 1.55 m across the galley half-door, Эйнар 1.80 m on the
  gallery, Лев 1.82 m across the radio desk, Нонна, Зоя, Гиви 1.4-1.9 m; the three in the wheelhouse only from inside it,
  Вайс at the chart table only when the hero walks up to it.

Looks: `blender/residents/looks.py` draws the 23 named people (`SHIP_CAST` uniforms: captain and mate a navy suit and a white
cap; purser and radio operator navy; barman and stewardess a white jacket over black; cook whites and a headscarf; engineer
dark blue overalls and a knitted cap; the quartet black; bosun and helmsman a pale blue work top, as the wardrobe has no
striped garment). **Until those 23 models are built and imported they live unseen**: the life runs and the counters count
as staffed, but there is no body to press E on. The twins are visible at once (pooled bodies).

Talk: the 23 named people are talked to with E, dialogue section `[Ship_<id>]` (10-12 everyday entries each: first meeting,
four "again", two to four by the sun, a Saturday line, a Sunday goodbye, a second-voyage line). Their barks and the twins'
lines: `tools/barks/lines/18_cruise.txt` (job `cruise_passenger`, `doing = cruise_*` / `ship_*`). Names:
`Content/Localization/shippeople/`. 23 speakers are in `localization/voice.json` with a proposed voice and `"skip"`: nobody is
voiced yet.

## Quests

Fourteen quests in `Content/UI/quests.json` from `tools/ship/cruise_talk.py`, plus L1's `ship_farlight` from the Far Light's
content. Every stage has a dev block (board, jump, teleport, the stage's items).

- **`ship_voyage` «Рейс на «Полдне»»**, the frame. Starts at `cruise_event aboard`, five diary entries (Friday, Saturday, the
  captain's evening, Sunday, home in sight), two stages a day so a slept-through event is made good by a later one; ends at
  `ashore` with a ship postcard. Everybody's Saturday and Sunday lines hang on its stages, and its `done` gives the
  "Опять с нами?" lines of a second voyage.
- **`ship_cat` «Заяц в трюме»**. Зоя hears crying under the cargo hatch and is afraid to look, and the bosun must not know.
  The hero listens at the hatch, buys a milk pyramid at the buffet hatch and is caught by the bosun, who has been feeding the
  cat (called Боцман) for three years and hides it from Зоя, who hides it from him. Reward: a candy.
- **`ship_telegram` «Телеграмма для Ласточки»**. Лев has a telegram with no surname: «Ласточке. Сердиться перестал. Жду обоих.
  Папа». The purser reasons out who travels as two: Оля, who married without her father. The hero carries the answer back and
  hears «папа» in Morse. Reward: 10 roubles and a postcard.
- **`ship_sharp` «Шулер из Риги»** (+ `ship_sharp_word`, `ship_sharp_report`). Гиви doubts the luck of the engineer from Riga.
  The hero sits at the card table and watches Альберт's hands on his deal (the poker's own mechanic; the stage `watch_hands`
  turns the marked deals on). Then two ends, either or both, each knowing of the other: speak to Альберт himself (a
  watchmaker whose workshop closed) or tell Гиви (a quiet list at the purser's). The hero gets lemonade and a milk cocktail.
- **`ship_pilot` «Лоция Веры Игнатьевны»**. The geography teacher pointed at these islands for thirty years and never saw one.
  Begins at the first island seen; see three islands (stamps `cruise/isl_*`) and read the «Лоция островов» in the library, and
  tell her: "they are smaller and farther". Reward: a set of postcards from her school atlas.
- **`ship_waltz` «Первый вальс»**. The bridegroom cannot dance and Saturday is the captain's waltz. Блюм sends word: a waltz
  counts to three, and he will play it slowly. The hero carries the count, goes on the floor on Saturday evening
  (`ship_dance`, during `concert2`) and hears both sides afterwards. Reward: sweets from the wedding table.
- **`ship_watch` «Вахта»**. The nineteen-year-old helmsman is alone in the wheelhouse from seven till midnight and after ten
  feels he is the only one awake; he asks for one soft stroke of the bell from the bows at night. Reward: his night-watch
  chocolate. The mate, who "sleeps", lies listening to how the ship runs.
- **`ship_diesel` «Дед и дизель»**. Эйнар hears a knock nobody else hears and cannot leave the engine. The cook gives a pressed
  linen handkerchief instead of a rag; the drummer returned the 27 spanner days ago, under the log the old man knows by
  heart. The knock was real. Reward: an amber charm, and why he never goes ashore.
- **`ship_birds` «Птичий базар»**. Mr Mori counts birds off Bird Island one hour a year and has two hands for four species.
  The hero counts the guillemots from the funnel bench, then feeds the gull called Captain from the stern so Mori can draw it.
  Reward: the notebook with the drawing.
- **`ship_games` «Палубное двоеборье»**. Римма has a plan of joy, forty-two people a voyage, and the hero is the forty-first.
  Win at ring toss and shuffleboard; the diploma is presented "before the sea". Reward: 15 roubles and a candy.
- **`ship_key` «Бабушкин чемодан»**. Сеня lost the key of his grandmother's suitcase playing pirates. Three places to look; the
  key lies where he buried it as treasure. The grandmother has worn a spare since 1946 and kept quiet so he would find it
  himself. Reward: a piece of sea glass.
- **`ship_captain` «Капитанский час»**. Begun by the reward of any other story (the captain is told): an invitation to the
  bridge, a minute at the wheel (the stage `wheel` gives the bridge pass), and why the ship is called «Полдень»: forty days in
  the ice and four minutes of sun. Reward: his binoculars.
- **`ship_farlight` «Ход по радио»** (L1). Лев's chess partner, one move a week by radio for eleven years, is Tamm, the keeper
  of the Far Light. Starts at `lunch2` on Saturday: carry the sealed envelope with the move ashore by the launch, wind the fog
  bell for Tamm (forty turns, not forty-one), bring his answer back to the radio desk. Reward: 15 roubles.

Six remarks said unasked (`quest_scenes.json`, ids `ship_*`) each leave a diary line `journal.heard.ship_*`. Strings:
`Content/Localization/shiptalk/` (444 spoken line keys and 144 other strings per language).

## Scenery and sea

### The sea round the ship

`ULyudCruiseScenery` (world subsystem; tick) reads `ULyudCruise::View()` and, while the hero is aboard, places the seascape,
drifts the sea, feeds the stage ship's wake, plays the ship's sounds and flies the gulls. Ashore its tick is one comparison.

- **The seascape** (`ALyudCruiseSeascape`). A chart point P is drawn at `Stage + Rot(StageYaw - Heading) · (P - ShipChart)`.
  It sinks "hull down" by `max(0, d - sqrt(2 R eye))² / 2R` with R = 2 500 km (`lyud.Cruise.EarthRadius`: tuned so that 30 m
  stacks rise out of the sea inside the range), and is drawn within min(own range, 1.4 x visibility): islands 18 km
  (`lyud.Cruise.IslandRange`), the home coast 26 km, the three volcanoes 60 km. Islands stand on the sea's datum (rock does not
  ride the tide; skirts go to -7 m). Each island has a near and a far mesh, swapped at 4 km with hysteresis; movable, no
  collision, out of Lumen's scene and distance fields, shadows only under 1.5 km, moved at 10 Hz beyond 3 km. `cruise.json`'s
  island `x, y` override the kit's chart points; Long Island is turned and shifted so its bay socket lies on `bay` (the
  anchored ship is 77 m from it, the nearest rock 326 m). The sun, the wind and the waves' directions stay the world's: the sun
  is always on the stage ship's same side and a circle round Bird Island does not turn the shadows on deck.
- **The islands kit** (`blender/ship/seascape_kit.py`): numpy signed distance fields meshed by surface nets, colour in the
  vertex colours times a tiling rock detail (8 m a tile): the Stone Brothers, the Needle's Eye, Bird Island, the Far Light, Long
  Island, the Seal Rocks, the home coast impostor (from `tools/world_map.py` and `city_plan.json`) and three volcanoes. 17 meshes,
  212 591 triangles of a 250 000 budget. The glb is not in git; `import_cruise.sh` builds it. Rebuild after the town's outline or
  the relief changes (no `--check` covers it).
- **The hand-over of the real rock** at the Far Light: see [the landing](#timetable-and-route).
- **Night.** The Far Light's lamp (a star sprite and two beam cards, one turn in 10 s), the pier light (6 s), the town's lights
  mesh `SS_coast_lights` and the keeper's windows glow by the sun's height (`glow_lamp` / `glow_window` slots with MIDs of
  `M_TR_Glow`).
- **The wake** (`ULyudShipWake`, the train's adapted): bow rings along the V, side streaks, the screw's road astern (130 m),
  spray at the stem in a head sea over 0.8 m; pools of 900 foam and 300 drops; the foam drifts aft with the water.
- **Surface drift.** `ALyudSea::SetSurfaceDrift` carries the wave pattern aft at the ship's speed: the phases of the existing
  wave table are rewritten on the CPU as `Base - K · (dir · Drift)` and uploaded every frame while under way (from a pooled
  block: no allocation by our code once warm). Zero drift: no upload, bit for bit as before. Reset on going ashore.
- **What is hidden aboard** (and restored ashore, exactly what was hidden): `ALyudRailTrack`, `ALyudNorthPlume`, static mesh
  actors whose mesh name starts `SM_North_Far` (the far volcano impostors); looked for again 2 s after boarding and then every
  5 s (the meshes every 15 s), so late spawns after a load are caught. Chart copies of the volcanoes and the home coast take
  their place. The soundscape's surf is silenced (`SetOpenSea(true)`). The corvids' G-throw is off aboard. The real town's
  terrain is not hidden: 26 km of haze must hide it.
- **The ship on the roads** (`ALyudMotorShip`): copies the Shell, Detail and Windows meshes of the city building `P-SHIP` (a
  grey box hull if it is not found), swings head to wind on 45 m of cable, three long blasts at 18:00 Friday heard in town and
  one on return, sails out and back by `View().ChartPosM`, hidden while the hero is aboard, while laid up and beyond
  min(20 km, 1.4 x visibility); her own small wake; anchor, masthead and deck lamps at night. She stands at the real
  `SeaLevel()` and takes a small ride from the surface at bow, stern and both sides (`LyudShipSea::RideFrom`, read at 5 Hz,
  bounded to 20 cm, 0.8° and 1.5°).
- **Gulls.** Six at the stern (the train's code); `FeedGulls(From)` brings them to the thrown crumb (the act hook for acts
  `ship_gull_feed*`, and `lyud.Cruise.FeedGulls`). 16 seabirds (herring, common and black-headed gulls, cormorants) circle Bird
  Island within 3 km by day with their calls.
- **Sounds** (synthesized by `tools/ship/ship_sounds.py`, soundscape group "Ship", loaded as `/Game/Audio/Ship/S_<id>_01`):
  `ship_hum`, `ship_engine`, `ship_wash` scaled by speed; `ship_rigging`, `ship_creak` with the wind and sea; `ship_bell` every
  half hour 07:00-23:00 (four-hour watches, `BellsAt`); `ship_horn3` at `departure`, `ship_horn` at `arrival` and at each `isl_*`
  event; `ship_chain` on entering and leaving `Anchored`; `ship_telegraph`; `gulls_feed`. Nobody has listened to them yet.

### The sea at the hull

The ship the hero is aboard is a building at the sea's datum, so the sea is made to suit her while the camera is within
2.5 km of the stage (`ULyudCruiseScenery::TickSeaAtStage`):

- **Tide hold.** `ALyudSea::SetTideHold` holds the whole sea at the datum (over the mean-level offset and `lyud.TideOverride`
  too). Keyed on the camera, not on boarding (boarding is noticed 0.5 s late and never for a stowaway or a teleport): when the
  camera jumps more than 500 m in a frame (the fades, a load, a teleport) it is set or lifted at once, otherwise it eases over
  60 s. Global, and harmless: no people activity, schedule or quest depends on the tide; the town paper and the music keep the
  true one (`LyudTide::At`); `TrueTideOffsetCm()` gives it, while `TideOffsetCm()` reads 0. `lyud.Cruise.TideHold 0` frees it.
  The pier's landing stage is fixed to the pier and left alone.
- **Calm footprint.** Nothing masks the sea inside a hull and deck D is 0.30 m up. `SetCalmFootprint` lays a calm patch on the
  hull's box (62 x 10 m, a 10 m soft edge): inside, the surface stays within about 15 cm of the level in any sea (a gale with Hs
  2.44 m: 0.13 m inside, 1.8 m crests outside, 2 cm the largest step 5 cm apart). It uses the sea's unused "calm" slot of
  `Lyud.Water`, which scales the wave cap, breakers, ripples, whitecaps and surf foam: the depth stays 25 m, so no surf ring
  forms. The material (`Lyud.Calm`, parameters `CalmA`, `CalmB`) and `SampleSurface` share the numbers
  (`LyudWeather::FCalmFootprint`). Unset (`CalmB.x = 0`) and beyond the edge it returns exactly 1: heights bit for bit as
  before. **The material must be rebuilt** (`make_sea_materials.py`) for the patch to be drawn. The calm band shows as a slick
  about 10 m wide round the hull in a fresh sea: meant to read as her lee.
- **Sea-state cap.** `SetSeaStateCap` grows the waves in no more wind than gives Hs `lyud.Cruise.MaxHs` (1.2 m). The wind
  itself (rigging, rain, sounds) is not capped; `SeaWindSpeed()` reports the capped wind while it is on. Applied at once when
  the camera crosses 2.5 km.
- **Dry zone.** A dry zone over deck D's cabins (ship x 12-46 m, y within 4 m, z -3.3 to +2.7 m) keeps swimming and the
  underwater look out of them (axis-aligned: set only while the stage is not turned).
- **Sway.** `ULyudShipSwayCamera` (a camera modifier) rolls the view by at most 0.6° x `lyud.Cruise.Sway` (default 0.5) with the
  sea, two slow sines each way, none in a flat calm; only while the pawn is inside the hull's outline (3 m margin), eased over
  2 s, off while the mouse pointer is shown (poker, books, menus); on while seated.
- **Navigation lights** on the stage ship: five small unlit glow balls (`M_TR_Glow`, outside the building's 24-lamp budget) at
  the hull's lamp boxes from `_shipkit.py` (`FITTINGS`: `masthead_light`, `stern_light`, `sidelight_stbd`, `sidelight_port`):
  masthead white, port red, starboard green, stern white while under way; one white anchor light forward in every other phase.
  Dusk to dawn.

Maths: `Private/Cruise/ShipSeaMath.h`, `Private/Weather/WaveModel.h`. After changing `tools/sea/sea_hlsl.py` rerun
`unreal/scripts/make_sea_materials.py`.

## Flags, events and API for other systems

**`ULyudCruise`** (`Public/Cruise/LyudCruise.h`; everything is safe to call when the cruise is off: phase `Laid`,
`IsEventOn` false, `SecondsToNextEvent` -1):

```cpp
static ULyudCruise* Get(const UObject* WorldContext);
const FLyudCruiseView& View() const;      // Phase, bHeroAboard, Day (1..3, 0 in port), ChartPosM (m), HeadingRad (wraps),
                                          // SpeedMS, TurnRateRad (+ = to starboard, smoothed 40 s), VoyageSeconds (< 0 only while boarding)
bool IsEventOn(FName EventId) const;      // "concert1", "isl_birds", ...
double SecondsToNextEvent(FName* OutId = nullptr) const;
FTransform StageTransform() const;        // ship frame cm (x to the bow, y to starboard, z from the waterline) -> world, at the stage
FTransform RoadsTransform() const;        // the same for the visual ship on the roads (z = the sea datum)
bool GetShipPoint(FName Name, FVector& OutWorld) const;  // cruise.json "points" or any thing id of P-SHIP, world cm at the stage
FOnCruiseEvent OnCruiseEvent;             // (FName event id) when an event STARTS
bool IsRunning() const; bool IsHeroAboard() const; static bool IsHeroAboard(const UObject* WorldContext);
FName HeroClass() const;                  // "hold", "std", "lux"; None ashore
int32 HeroCabin() const;                  // 15, 7, 11; 0 ashore
int32 CruisesDone() const;                // voyages sailed to their end (saved)
FVector ToShipFrame(const FVector& World) const;  bool IsInsideHull(const FVector& World, float MarginCm = 0.f) const;
void Say(const FText& Speaker, const FText& Line, float Seconds = 7.f);   // a subtitle (the ship's radio)
double NowSeconds() const;  FString Report() const;
void DevBoardNow(); bool DevNextEvent(); bool DevJumpTo(FName EventId); void DevAshore(); void DevLanding();
bool IsRealRockShown() const; bool IsHeroOnRock() const; FVector RockWorld() const;   // the Far Light landing
void GiveBridgePass();
```

The voyage needs no notice of a clock jump: the subsystem polls the calendar. `OnCruiseEvent` after a jump of 3 h or less
fires every event that began inside it, in order; after a longer jump or a load only the events on at that moment. Tick
does not allocate; the hero and flags are looked at 4 times a second and every flag is re-sent every 2 s. Save record
`Cruise`: aboard, class, cabin, voyage, cruises sailed.

**Flags** (set by `ULyudCruise` through `ULyudCityThings::SetFlag`; never saved, always recomputed; they follow the calendar
whether or not the hero is aboard, so the town may talk of the ship being away):

| Flag | On while |
|---|---|
| `cruise_season` | 15 April - 15 November |
| `cruise_voyage` | Friday 16:00 - Sunday 20:00 of a week in season |
| `cruise_boarding`, `cruise_landing` | Friday 16:00-18:00; Sunday 18:00-20:00 |
| `cruise_has_ticket` | any of the three tickets in the pocket |
| `cruise_can_board` | boarding, a ticket, not aboard |
| `cruise_aboard` | the hero is on a voyage (from the gate to going ashore) |
| `cruise_day1`, `cruise_day2`, `cruise_day3` | Friday, Saturday, Sunday of the voyage (off in port) |
| `cruise_underway` | the ship moves (also while circling) |
| `cruise_circling` | round Bird Island |
| `cruise_anchor` | the night anchorage (Sunday 00:00-06:00): anchored and NOT at the landing stop |
| `cruise_ashore_stop` | the ship lies at the Far Light's landing stop |
| `cruise_ashore_open` | the launch to the rock runs (5 min after anchoring to the last launch, 18:20) |
| `cruise_port` | on the roads off the town (boarding, landing, between voyages, in season) |
| `cruise_class_hold`, `cruise_class_std`, `cruise_class_lux` | the hero's class while aboard |
| `cruise_night` | 23:00-07:00 of a voyage |
| `cruise_done` | he has sailed at least one whole voyage |
| `cruise_ev_<event id>` | that programme event is on (one-time events for 10 minutes) |
| `cruise_at_<island id>` | that island is within sight, 4 km (`home`, `brothers`, `arch`, `birds`, `lighthouse`, `long`, `seals`) |

**City data** (generic, for any plot): a line, topic, text page or act may carry `"flag": "name" | "!name" | [...]` (a list:
all must hold; a flag nobody set is off). An act with `"no_flag": "<act id>"` or `"<line key>"` / `"<prefix>.*"` shows that
words-only act while its flags fail (without one: the usual `city.act.closed`); developer mode's "open all hours" does not
lift a flag. `"until": "cruise_event"` makes an act last to the start of the next programme event (rounded up to the minute,
at most 180); with no next event its own `minutes` pass. Generic underneath: named waits, `ULyudCityThings::SetWaitMinutes`.
`ULyudCityThings::SetFlag / HasFlag`.

**Quest events.** `ULyudQuests::Notify(.., "cruise_event", <target>)` with targets `aboard` (came aboard with a ticket),
`ashore` (his voyage ended), `overboard`, and every programme event id when it begins while he is aboard. The city acts send
`do <act id>`, `win <act id>` for a won game, `read <text id>`, `ask <topic id>`, `stamp <set>/<piece>` (e.g.
`stamp cruise/isl_birds`: use the stamp where "saw island X" is meant).

**Poker** (`ULyudShipPoker::Get(World)`: `Open(bForce)`, `Close()`, `IsOpen()`, `SetQuestMode("sharp" | "")`, `QuestMode()`,
`IsSharpCaught()`, `HandsPlayed()`, `PotsWon()`, `BiggestPot()`, `HasBeaten("ship_givi")`, `Report()`; opened by the act field
`"open": "poker"` through `ULyudCityThings::OnOpen`, only while `cards1` or `cards2` is on):

| Notify event | Targets |
|---|---|
| `poker_sit` | `matches`, `roubles` |
| `do` | `poker` (every hand) |
| `win` | `poker`, `poker_showdown`, `poker_bluff` |
| `poker_beat` | `ship_albert`, `ship_givi`, `ship_vera`, `ship_taras`, `ship_blum`, `ship_lev` |
| `poker_hand` | `pair`, `two_pair`, `three`, `straight`, `flush`, `full_house`, `four`, `straight_flush`, `royal` |
| `poker_leave` | `up`, `down`, `even` |
| `poker_broke` | `matches`, `roubles` |
| `card_sharp_suspect`, `card_sharp_caught`, `card_sharp_missed` | `ship_albert` |

Stamps (collection `poker`): `poker/ship_albert` ... `poker/ship_lev` (beat each), `poker/straight`, `flush`, `full_house`, `four`,
`straight_flush`, `royal`, `poker/sharp`.

**Others.**
- `ULyudCruiseScenery::Get(World)`: `FeedGulls(const FVector& From)`, `AreGullsFeeding()`, `NumGulls()`, `NumColonyBirds()`,
  `GetSeascape()`, `GetMotorShip()`, `GetWake()`.
- `ALyudCruiseSeascape`: `GetIslandWorld(Id, Out, OutDistM)`, `GetSocketWorld(Island, Socket, Out)` (`lighthouse`/`lamp`,
  `birds`/`colony`, `long`/`anchorage`), `NumShown()`, `SetRealRock(bool)`. `ALyudMotorShip::Horn(int32 Blasts)`.
- `ALyudSea`: `SetSurfaceDrift(FVector2D cm/s)`, `SetTideHold(Owner, bHold, BlendSeconds)`, `IsTideHeld()`,
  `TrueTideOffsetCm()`, `SetCalmFootprint(...)`, `ClearCalmFootprint()`, `CalmAt`, `SetSeaStateCap(HsM)`, `SetDryZone`.
  `ULyudSoundscape::SetOpenSea(bool)`.
- `ULyudShipBand::Get(World)`: `IsPlaying()`, `CurrentTuneId()`, `CurrentTuneKey()`, `CurrentSet()`, `CurrentNumber()` (-1 in a
  pause: the moment for applause), `PlayTest`, `PlayNext`, `StopTest`.
- People: `UPeopleLifeSubsystem::SetCondition("cruise_season", ...)` every 2 s; counters ask `StaffOnShift("P-SHIP", role)`.
- Act hooks of the cruise: one `ship_postcard` is used up after an act whose id starts `ship_post_box`, one `bubliki` after
  `ship_gull_feed` (plot `P-SHIP`).
- `cruise.json` for anybody: `season`, `voyage`, `sight_m`, `islands[]`, `route[]`, `ashore`, `events[]` (`id`, `day`, `from`,
  `to`, `t0`/`t1` seconds from Friday 18:00, `where`), `tickets[]`, `stage`, `roads`, `hull`, `pier_fallback`, `points`.

## Console and developer mode

| Command | What |
|---|---|
| `lyud.Cruise 0\|1\|2` | off / on in `Lvl_Scene01` / also in any other game level (tests); read at the level's start |
| `lyud.Cruise.Board` | a ticket in the pocket, the clock at the next Friday 17:30, the hero at the launch gate |
| `lyud.Cruise.Next` | the clock to 5 s before the next programme event |
| `lyud.Cruise.Jump <event id>` | the clock to 5 s before that event of this voyage (else the next) |
| `lyud.Cruise.Landing` | Saturday 16:40, the hero on the Far Light's jetty |
| `lyud.Cruise.Ashore` | ashore at once |
| `lyud.Cruise.BridgePass` | the stay `cruise_bridge` for today |
| `lyud.Cruise.Clock <s>` | seconds added to the cruise's clock (a cvar: it does not set the time; people and the sky do not follow) |
| `lyud.Cruise.Report` | phase, position, events and the flags that are on |
| `lyud.Cruise.Scenery 0\|1`, `lyud.Cruise.SceneryReport` | the scenery off (to bisect; the roads ship ticks on by itself) / what is shown, drift, gulls |
| `lyud.Cruise.EarthRadius`, `lyud.Cruise.IslandRange` | the hull-down radius (2 500 km), the islands' range (18 km) |
| `lyud.Cruise.FeedGulls` | the gulls to the stern rail |
| `lyud.Cruise.TideHold 0\|1`, `lyud.Cruise.MaxHs <m>`, `lyud.Cruise.Sway <x>` | the tide hold, the sea-state cap (1.2), the sway (0.5) |
| `lyud.Sea.Debug 1` | prints `tide held 1.00 (true -4.50 m), Hs cap ..., calm patch set inner ...` |
| `lyud.Band 0\|1`, `lyud.Band.Play [<id> [soft] \| next \| stop]`, `lyud.Band.Report` | the band; `Play` alone lists the tunes; test play is 2D, anywhere |
| `lyud.Poker force\|open\|close\|report`, `lyud.Poker.Quest sharp\|off` | the table at any hour / as the act does / ... / the marked-cards mode |

Developer mode (F7), group `cruise` (Теплоход): teleports `ship_gangway`, `ship_saloon`, `ship_dining`, `ship_bridge`,
`ship_boat_deck`, `ship_smoking_deck`, `ship_foredeck`, `ship_cabin_hold`, `ship_cabin_std`, `ship_cabin_lux`,
`farlight_jetty`, `farlight_hut`, `farlight_gallery`; actions `cruise.board_now`, `cruise.landing_now`, `cruise.next_event`,
`cruise.ashore`, `cruise.report`, `cruise.my_cabin`, `cruise.ticket_box`, `cruise.ticket_hold`, `cruise.ticket_lux`,
`cruise.jump_<event id>` (one per programme event, registered at level start), `band.play_next`, `poker.open`, `poker.sharp`.
Every quest stage has a dev block (`lyud.Cruise.Board`, `lyud.Cruise.Jump <event>`, a `ship_*` teleport, the stage's items;
the sharp's stage also `lyud.Poker.Quest sharp`, the captain's wheel stage `lyud.Cruise.BridgePass`).

## Offline tests and the generators

Rerun order after a change of the ship's model, the plots or any ship content (each step reads the previous one's output):

```sh
python3 blender/city/build_city.py -- motor_ship --no-render           # export + lint (KIT_CHECK=1 for the kit's checks)
python3 blender/city/build_city.py -- far_light --no-render
python3 blender/city/build_city.py -- hyperboloid_tower --no-render    # the pier's landing stage
python3 tools/city_plan.py
python3 tools/ship/cruise.py                    # stage transform and named points come from the plan and the export
python3 tools/ship/cruise_people.py --doc <path>   # then --check; look at its "Stand-in used for" line
python3 tools/barks/build_barks.py              # when 18_cruise.txt or the ship's activities changed
python3 tools/ship/cruise_talk.py               # quests, talk, remarks; then --check
python3 tools/city/interiors.py                 # interiors.json and the city strings; then --check
```

Then the checks (numbers of the last run):

| Check | Result |
|---|---|
| `bash tools/cruisesim/run.sh` (test + stale check; `table`, `at <date> <time>`) | 63 335 checks, 0 failed: season edges, every week of two years, route geometry and keep-off distances, speed, acceleration and turn limits, events in sight of their islands, the landing stop to a centimetre, flags, the hero's rules, save round trip |
| `bash tools/seascapesim/run.sh` (`table`) | 288, 0 failed: the frame, hull down, the beam, the wake's rates, the drift's phase, the bells, the voyage against every island (60 m clear of every rock, a kilometre off the cones) |
| `bash tools/sea/run.sh test` | passed: the calm footprint's bound and edge, exactly 1 when unset, a turned box, the Hs cap, the hold's steps, the ride's and the sway's bounds |
| `bash tools/citysim/run.sh` | 541 passed: flags, `no_flag`, named waits |
| `bash tools/pokersim/run.sh test` (`play [seed] [hands] [ru\|en\|ja]`, `sharp`, `stats`) | 2 859 639 checks, 0 failed: the evaluator against a slow reference on 100 000 deals, chip conservation over 10 000 sessions, the 60-rouble cap, determinism, style bands, tells, the cheat only at the deal, lineup by the clock, save round trip, strings |
| `bash tools/bandsim/run.sh test` (slow, 1-3 min; `wav <dir> [tune [soft]]`, `stems <dir> <tune>`, `set <id>`; `python3 tools/bandsim/analyse.py <stems dir>`) | 433 checks, 0 failed: parsing, bar lines, chords, composers' years <= 1955, ranges, voices, levels, endings, determinism, joining mid-tune, the sets' timetables, speed |
| `bash tools/peoplesim/run.sh cruise` (about 2 min) | all ok: watches, meals at own chairs, berths, twins never in the world twice, season, Resettle |
| `python3 tools/ship/cruise.py --check`, `tools/ship/cruise_people.py --check`, `tools/ship/cruise_talk.py --check`, `tools/city/interiors.py --check`, `tools/ship/ship_sounds.py --check`, `tools/pokersim/poker_strings.py --check` | ok |
| `tools/city/reach_audit.py motor_ship` / `far_light` | 0 errors (warnings: yaw and line-of-sight notes on furniture, the link doors "not followed") |
| `tools/questsim/run.sh`, `check_quest_talk.py`, `quest_talk_reach.py`, `city/quest_audit.py`, `check_devdata.py` | all ok / 0 problems / 0 never said / 0 errors |
| `check_data.py`, `check_localization.py`, `tools/cityplan/run.sh` | at their baselines: 284; 77 (70 + the seven item icons); 1 failure (`S-MAYAK-HOME`, older than the ship) |

`tools/run_all_suites.sh` has the rows `cruisesim.test`, `cruisesim.table`, `pokersim.test`, `pokersim.play`, `bandsim.test`
(slow), `seascapesim.test`, `peoplesim.cruise`, `sea.test`.

## First Mac session

One pass, in this order.

1. **Build.** Nothing of the ship was ever compiled: about 2 700 lines of scenery, the cruise, the band, the poker and the sea
   edits. The likeliest breaks and their fallbacks:
   - `LyudCruiseScenery.cpp` `HideWorld`: `TStringBuilder<NAME_SIZE>` + `FName::ToString(FStringBuilderBase&)` +
     `ToView().StartsWith` -> replace the three lines by `M->GetName().StartsWith(TEXT("SM_North_Far"))`.
   - `LyudSea.cpp` `FLyudWaveUpload`: `FUpdateTextureRegion2D` needs its default constructor; `Free.Pop(EAllowShrinking::No)`
     (the form is used 33 times in the module).
   - `LyudSea.h` `TPimplPtr<LyudWeather::FCalmFootprint>` (same pattern as `Field`); the new `UCLASS` `ULyudShipSwayCamera` in
     `LyudCruiseScenery.h`; `PC->PlayerCameraManager.Get()`.
   - `for (const TPair<FString, TSharedPtr<FJsonValue>>& It : Obj->Values)` in `LyudCruiseSeascape.cpp` and `LyudCruise.cpp`: if
     5.8 changed the map's key type -> `const auto&` (`LyudFood.cpp:58` would break with them).
   - `LyudShipBand.cpp` `SetLowPassFilterEnabled` / `SetLowPassFilterFrequency` (first use in the codebase) -> set
     `bEnableLowPassFilter` / `LowPassFilterFrequency` directly (they take effect at the next `Play`); `Holder->Destroy()` in
     `Deinitialize` warns -> guard with `!World->bIsTearingDown`.
   - `SLyudPokerPanel.cpp` `FitSize`: `FSlateFontMeasure::Measure(FString, FSlateFontInfo)` (the form of `SLyudMapView.cpp:301`);
     `PushClip(FSlateClippingZone(Geo.MakeChild(FVector2f, ...)))` -> pass `FVector2D` as `SLyudDiary.cpp:182` does.
   - Format strings checked strictly: `%lld` with `long long` in `LyudShipPoker.cpp` / `LyudCruise.cpp` `Report`.
   - UHT and the user-declared destructor + `TPimplPtr` of `ULyudCruise`, `ULyudShipBand`, `ULyudShipPoker` (same as about ten
     subsystems: if it fails, it fails for all); `-Wshadow` against a file-scope name of another .cpp in the same unity blob.
2. **`unreal/scripts/make_sea_materials.py`**: rebuilds `/Game/Sea/M_LyudSea` with `CalmA`, `CalmB` (keeps `MPC_LyudWeather`).
   **Required**: without it the waves are still drawn inside deck D while `SampleSurface` already reports calm there.
3. **`unreal/scripts/import_city.py`** for `motor_ship`, `far_light` and `hyperboloid_tower` (two new material instances
   `CT_ShipNavy`, `CT_ShipCream`), then the city build.
4. **`unreal/scripts/import_cruise.sh`** (never run: expect small fixes). It builds the islands kit with Blender if
   `design/ship/game/seascape_kit.glb` is missing (about 4 min; `REBUILD_KIT=1` forces; the kit was only ever built with the
   `bpy` module, never the Blender binary), imports 17 meshes to `/Game/Cruise/Meshes/seascape_kit`, the textures to
   `/Game/Cruise/Textures`, makes `M_SS_Rock` and `M_SS_Land`, and the sounds through `import_audio.py`
   (`IMPORT_ONLY=sfx/ship/`). Check that they land as `/Game/Audio/Ship/S_ship_horn_01` and so on.
5. **The 23 ship people's models** (docs/residents.md): `python3 blender/residents/looks.py` (writes `cache/looks.json`; it also
   rewrites `town_looks.json` whole with 125 changed hashes and 13 more townsfolk that were stale before the ship: decide
   before committing that churn), then with
   `K=ship_gromov,ship_vais,ship_taras,ship_nonna,ship_givi,ship_zoya,ship_pasha,ship_lev,ship_einar,ship_vitya,ship_rimma,ship_blum,ship_otar,ship_kostya,ship_asta,ship_vera,ship_misha,ship_olya,ship_albert,ship_haruto,ship_senya,ship_faina,ship_inga`:
   `Blender -b -P blender/residents/build_residents.py -- --only $K`, `python3 blender/residents/check_export.py`,
   `unreal/scripts/import_residents.sh --only $K` (whether `--only` takes the `ship_` keys as written is untested).
6. **The seven item icons** (paid; the owner's leave): `python3 tools/gen_localized.py icon_cruise_ticket_hold` ...
   (`icon_cruise_ticket`, `icon_cruise_ticket_lux`, `icon_cabin_key`, `icon_milk_cocktail`, `icon_ship_postcard`,
   `icon_cruise_diploma`): the prompts are pinned in `localization/assets.json`; no picture exists.
7. **The generators once more** in the order above, then `bash tools/cruisesim/run.sh` (its stale check fails when `cruise.json`
   is older than the export or the plan).
8. **Look, in this order:**
   1. `lyud.Cruise.Board` at spring low water (`lyud.TideOverride -4.5` first): she must sit on her waterline when the fade
      lifts; `lyud.Sea.Debug 1`. Land again: the pier shows the low water.
   2. Walk the ship: gangway, vestibule, corridor, cabin 7; all four stairs (16 risers a storey), both bridge ladders, every open
      door; lean on the rails at every deck edge and well; try to fall overboard at the bow and stern bulwarks and to squeeze
      outboard of the bridge ladders' feet. The library strip (1.45 m aisle), behind the bar (barman only), the 0.2 m bandstand
      step, round the wheel and telegraph, the wings. Sit on a dining chair; lie in `cabin7_bed_a` (0.95 m under the upper berth:
      does the camera clip?); deck D's stairs to cabin 15 and the showers; the engine gallery view.
   3. `lyud.Wind.Speed 16` aboard: Hs must read 1.2 m; deck D's corridor, the cabins, the engine room: no water through the
      floor; over the side: the calm band (about 10 m), no foam ring.
   4. The islands at 500 m by day: vertex colours present at all after Interchange's glTF import, and their sRGB/linear handling
      (if everything is too dark or too pale: a power 2.2 / 0.45 node in `M_SS_Rock`); the rock detail's scale; the far swap at
      4 km; hull down on the horizon; the home coast from 3 and 12 km against the real town (the swap when leaving the roads was
      never seen); the lamp at night; the wake's foam; the waves running aft at 5 knots and stopping at anchor.
   5. The Far Light: `lyud.Cruise.Landing`. From the gangway the lighthouse stands 330 m to starboard and is the real rock: no jump
      when it swaps with the far copy; **the jetty against the water** (the 1.5 m kit/building frame offset, the tide hold and the
      calm footprint were never seen together). Take `launch_ashore`, walk the path, climb to the gallery, read the log, talk to
      Tamm, wind the bell; 18:00: three bosun lines; after 18:30 on the rock: fetched to the gangway; jump off the jetty and swim
      3 s; leave the stop: the real rock hides and the copy's lamp returns; ashore, not a passenger, teleport `farlight_hut`.
   6. The bridge previews rendered the chart, the chart table's note pad and one disc on the wheelhouse's aft bulkhead magenta:
      look at the chart table first.
   7. Night: masthead, sidelights and stern light under way, the single anchor light in the bay; the opal fixtures against the
      kit's 24 real lamps (bandstand and promenades may be dark).
   8. Sway: the saloon in a fresh sea, `lyud.Cruise.Sway 0` and `1`; open the poker panel: it must stop.
   9. Sounds: all 11, the horn and the bell first. The band: `lyud.Band.Play saints`, then `next` through all 17 (by ear: is each
      recognisable, above all `st_james`, `ochi`, `choclo`? clarinet hollow, sax breathy, not an organ? brushes not too hissy?
      bass audible on small speakers? endings?); `lyud.Cruise.Jump concert1`: walk saloon, lobby, promenade, boat deck, main deck
      and judge the levels (constants in `ULyudShipBand::Place`); sleep through a concert and load a save aboard (joins mid-tune
      with a quarter-second fade); go ashore (it stops); `captains_dinner` on the main deck: is the relay wanted?; the title
      caption once per tune in ja and en.
   10. Poker: `lyud.Poker force`: the panel at the screen's foot, no overlap in ru, en, ja; H, the draw (A/D, W, E), 1/2/3, Esc
       mid-hand, E for the next hand; the roubles table with 30+ roubles (money out on sitting, back on standing, four minutes a
       hand, the table breaks up after 01:00); save mid-evening and load; `lyud.Poker.Quest sharp`, Q while Альберт deals.
   11. The voyage end to end with the dev commands: the ticket at the pier box, the launch gate, one's own berth by class, a meal
       at `dining_chair_1`, the bridge tour then the wheel, each quest by its dev blocks (Паша, Эйнар, Лев and the three in the
       wheelhouse must be reachable for talk), the diploma on Sunday, the gangway at landing, 20:00 put ashore; jump into the sea
       from the aft deck (2 s, back on the aft deck); save aboard, load; load a save of another week.
   12. From the pier: the roads ship at high and low water, her ride in a breeze, the three blasts at 18:00 Friday, the stern name
       from the launch; flicker on the raised letters and flags (two-sided cards: the materials must not be two-sided); the navy
       under the game's sun (one hex in `citymats.py`); the planks' direction with the real `timber_stained` texture.

## Open

Never seen in the engine: all of it. Compiled: none of the Unreal code (only the engine-free models, offline).
Everything below is what is known to be missing, unverified or soft.

The ship and the rock
- Collision of the thin rail bars, the stairs under the character mover, lamp brightness, the window cards at night,
  texel density, the furnished rooms' fit, lying and sitting cameras, NPC use of the spots: unchecked in the engine.
  Interiors were reviewed only in Blender previews with smudged stand-in textures (the repo's textures and atlas cells are
  LFS pointers here).
- Closed and empty: cabins 3-6, 8-10, 12-14, 16-20, the crew quarters (no stair of their own), the WCs, the baggage room, the
  hold; the radio room is served over a flap, not entered; the galley is looked into; nobody can stand forward of the bridge
  ladders on deck A.
- No words on the geometry beyond the name and home port: number plates are 7-segment digits, the telegraph dial has
  sectors without the orders, «Место для курения», the lifejacket chest and the muster board need atlas cells or generated
  cards.
- Not modelled: weather-door leaves, handles and hinges, rounded window frames, bench detail, stair treads beyond the brass
  nosings, the anchor chain beyond the windlass; the hull is flat shaded (facets show at the stern); flags are static; the
  ensign is a design of our own, not an approved town flag; the lifeboats have no inside, the davits do not move, nothing aboard
  is animated; no sofa in the vestibule, no rug before the purser's desk; the galley has no lamp of its own.
- The bridge previews' magenta chart and flat wheelhouse panelling: believed a preview effect, not proven.
- The Far Light: no launch boat is drawn and there is no ride (a fade); no passenger walks the rock and Tamm has no people
  record or body (his table plays the act's own lines; the 8-topic chooser opens only with someone on shift at the plot); the far
  copy lacks the paths, the hut's furniture and the small props, so small things appear at the swap 330 m away; 16 faces of the
  rock shell report stretched UVs; no room tone for `P-FARLIGHT`; the ship reaches the stop by passing 3.4 km off and turning
  back (fine on the chart, unusual to watch). `_rocknoise.py` must follow any change of the kit's skerry function.

The cruise
- No M-map label for the roads (outside the map's frame; `cruise.ui.map.roads` exists unused).
- The radio is subtitles only (no voice, no chime).
- Overboard was never tried against the real hull collision; if a deck is ever modelled below the waterline the rule must also
  require `!IsInsideHull` (else he is "rescued" from his own cabin).
- The landing fallback (map 311.3, 905.0) was never seen. A hero asleep in his berth at Sunday 20:00 is left to wake (no limit).
  A hero who is not the `AHeroCharacter` (the bird) when the voyage ends waits until he is himself again.
- After every load aboard the events on at that moment are sent again to the quests.
- The stays `cruise_hold`, `cruise_std`, `cruise_lux` are started by the code but only `cruise` and `cruise_bridge` are declared
  in `x_ship.json`: check what `StartStay` does with an undeclared id.
- The header comment on `ELyudCruisePhase::Circling` still names the Far Light; only Bird Island is circled now.
- `docs/city_interiors.md` has no "Flags" section yet (the text is under [Flags, events and API](#flags-events-and-api-for-other-systems)).

Scenery and sea
- No surf ring or breaking foam round the islands' feet, no plume on the chart copy of the smoking volcano, the seals do not
  move, no crumbs are drawn when the gulls are fed; the roads ship shows nobody on deck.
- `ALyudTrain` is not hidden aboard (it sets its own visibility): a lit train on the causeway may show from the stage at night.
- The roads ship copies the building's meshes onto movable components: Nanite shells, static-lighting features or the Windows'
  shared day/night MID may make her look unlike the one boarded; `M_TR_Glow` / `M_TR_Glass` / `M_TR_Paint` parameter names are
  assumed from the train's use. `lyud.Cruise.Scenery 0` does not hide her.
- If the sea's GPU wave field samples a baked spectrum rather than the table, the detail waves will not drift. At speed, deck
  motion sampled from the sea sees a sea running past: possibly stronger motion. `UTexture2D::UpdateTextureRegions` still makes a
  small engine-side allocation per frame while drifting.
- Colony birds are 16 `ABirdActor`s 800 m away: they may be culled or too small to read.
- The stage wake's foam freezes, not clears, when he goes ashore under way (`lyud.Cruise.Ashore`): old foam for some seconds at
  the next boarding.
- `lyud.North 0` typed aboard and then going ashore shows the far actors again (developer only).
- `cruise.json` gives Bird Island `yaw 20`, the kit 0; the scenery reads only `x`, `y`, `bay`, so the cliff may face 20° off what
  the route's author meant (the clearances hold).
- The home coast impostor is built from this checkout's relief and plan: rebuild when they change. `seascape.json` is committed
  while the glb is not; a rebuild on other numpy/Blender versions may mesh a little differently (harmless to the C++).
- The calm band is a visible slick about 10 m wide in a fresh sea: nobody has seen it. The sea-state cap steps at once when a hero
  reaches the stage by himself (26 km of open sea). The dry zone is axis-aligned; a hero under the waterline in it falls rather
  than swims (the overboard rule fetches him). Anything later modelled below the waterline would show flat water (the fix: the
  unused `lift` slot). The sway modifier is never removed, only zeroed. The navigation lights' size (16 cm) and intensity (40)
  are guesses. While the hold is on `TideOffsetCm()` is 0 for everyone.
- The sounds were synthesized and checked for format and level only: nobody has listened to them.

The band
- Not compiled, not heard by a human ear: the numbers say in tune, in time and balanced, not "sounds good". Three sheets to
  check (`st_james`, `ochi`, `choclo`); Florian Hermann's death year is the commonly given one; no Russian waltzes.
- Played as one mono source since the audit: the four players' stereo placement is lost, also in the 2D test path.
- Rendered on the game thread like `ULyudInstruments` (0.2 s queued ahead): a game-thread hitch over 0.2 s is a gap in the music.
  Under a developer fast clock the band drifts and restarts every few seconds. If something destroys its holder actor the voice is
  not re-made.
- No applause between numbers, no instrument manners except the piano (the bass, drums and reed players sit and sway).

Poker
- Not compiled against the real engine headers; the panel was only seen as a mock; Japanese was not rendered (lines are fitted,
  not proven).
- No sounds (cards, chips), no voice; the regulars' bodies do not react, and nothing ties the panel's lineup to the people at
  the chairs (same hours, no link); the hero's chair is the act's `"pose": "sit"`; no tutorial hand.
- E deals the next hand while the regulars sit in front of him: if they are registered with `ANpcTalkDirector`, E may also
  start a talk over the panel (Enter and Space are safe). No distance check: if something moves the hero, the panel stays until
  a key. The clock is checked between hands only. A mid-sitting save does not write the regulars' purses.

Content, quests, people
- Prompts, poses, the deck chair's fade, the rally and aim games, the dev blocks: checked by the generators and citysim only.
- en and ja of the content, the talk and the poker were written by us, not read by a native speaker. No voice-over (the 23
  speakers are `skip`; an estimate at about $0.10 per 1000 characters through fal: ru about $4.4, all three languages about $11.6).
- The cabin loudspeaker plays the town's `radio_tune`, not the band. Second-voyage words are few. Silent sound ids shared with the
  town: `morse_beep`, `pencil_scratch`, `tap_water`, `ticket_punch`, `ring_clink`, `waltz`, `wind_soft`, `clock_tick`.
- `ship_count_birds`, `ship_bell` and `ship_hatch_listen` send their event at any hour (a quest can close while the text says
  otherwise); `ship_look_deckchair` is the doze act; the bridge pass comes one stage late if the wheelhouse is closed to walk into;
  `Quest.GoToPerson` is missing from the dev blocks until questsim knows `ship_people.json`; quests tied to Saturday evening or a
  night read oddly on Sunday (kept for the next voyage); after the first voyage `ship_voyage` is done for good and the Saturday and
  Sunday lines stop; side quests do not repeat; `ship_sharp_report`'s reward has no line; `ship_pilot`: whether the first island
  counts is the engine's rule, not checked.
- The walk tree aboard is hand-made: people walk straight legs, cut between dining tables and pass through cabin doors the model
  keeps shut. Гиви stays behind the bar until 01:00, so at the poker table he is a voice, not a body. A town record resettled
  mid-voyage comes back through the stay's usual slot, not the pier. The bark events `cruise_cast_off`, `cruise_island`,
  `cruise_home` have lines but no code calls `SayEvent` for them. The twins' town records move the town's dice a little even out
  of season (the checks' thresholds hold; the rule is not found).
- At the Far Light the passengers' duty `cruise_isl_lighthouse` still follows the old one-hour window (Saturday 16:33-17:30 at the
  rails) and nobody goes ashore. Several content lines still say the ship goes round the light, and the keeper's name and household
  disagree between the ship's strings and the rock's (`city.ship.see.lighthouse.1/2`, `city.ship.pilot.5`, `city.ship.chart.2`,
  `city.ask.ship.bar.story.2`, the engine-door lines' flags; the Japanese name of the light is ダーリニー灯台 in the ship's
  strings and 遠灯台 elsewhere).
- `town_looks.json` carries only the 23 `ship_*` entries; a full `looks.py` run churns 125 hashes (stale before the ship).
- `S-MAYAK-HOME`'s `"private": 1` in `town_places.json` has never been read by `LoadTownPlaces`: the town owner's call.
