# Backlog: new things to do (interactives)

The reserve list of new activities for the hero, built on what the game already has. Written 2026-11-20 for the quota
push (Dima: "add interactive things we could do with the current infrastructure, as a backlog in case the quota is not
used fast enough"). Each item has a **ready-to-paste thread brief**: give it to a new thread as is.

Direction (Dima 2026-11-13): **deep, not wide**. Every item below makes something that already exists playable or richer;
none needs a new area of the map. Items already given to their own threads on 2026-11-20 are **not** here: people/animal/bird
animations, bird flight, new bird models, the Sun festival + summer town (poplar fluff, bunting), the merge to main, the
landscape and the road from Aton to town, the cable-car stations, the library of 250 public-domain books (reading on a
bench). Where an item touches one of them, its brief says so.

Marks: `[ ]` free, `[~] <thread> <date>` taken, `[x]` merged. Whoever takes an item marks it here first.

## Order

| # | Item | Value | Where it runs | Model | Size |
|---|---|---|---|---|---|
| 1 | [Maru and the town's cats become friends](#1-friends-among-the-animals) | very high | cloud + Mac shots | Fable 5.2 | L |
| 2 | [A film camera and the photo album](#2-a-film-camera-and-the-photo-album) | very high | cloud + Mac | Fable 5.2 | L |
| 3 | [A course of cure at the sanatorium «Корона»](#3-a-course-of-cure-at-the-sanatorium) | high | cloud (data) | Opus 5.5 | M |
| 4 | [Small jobs: the hero earns his roubles](#4-small-jobs) | high | cloud (data + C++) | Opus 5.5 | L |
| 5 | [Telescopes that really show the sky](#5-telescopes-that-show-the-sky) | high | cloud + Mac shots | Fable 5.2 | M |
| 6 | [The town paper and notices that follow the days](#6-the-town-paper-and-notices-by-date) | high | cloud (data + C++) | Opus 5.5 | M |
| 7 | [Kites on windy days](#7-kites-on-windy-days) | high | cloud + Mac | Opus 5.5 | M |
| 8 | [The beach: stones skipping, sandcastles, the buoy swim](#8-the-beach-at-play) | high | cloud + Mac | Opus 5.5 | M |
| 9 | [An allotment: grow, water, pick, sell](#9-an-allotment) | high | cloud + Mac | Fable 5.2 | L |
| 10 | [Records, the jukebox and the town radio](#10-records-the-jukebox-and-the-town-radio) | medium-high | cloud (+ fal, ask) | Opus 5.5 | M |
| 11 | [Letters, postcards and pen friends](#11-letters-postcards-and-pen-friends) | medium-high | cloud (data + C++) | Opus 5.5 | M |
| 12 | [Friendship with residents: tea at their homes](#12-friends-among-the-residents) | medium-high | cloud (C++ + data) | Fable 5.2 | L |
| 13 | [The cinema shows real films](#13-the-cinema-shows-real-films) | medium-high | cloud + Mac | Opus 5.5 | M |
| 14 | [Club tournaments: the regulars at the tables](#14-club-tournaments) | medium | cloud (data + C++) | Opus 5.5 | M |
| 15 | [A playable piano and a guitar by the bonfire](#15-a-playable-piano-and-guitar) | medium | cloud + Mac | Opus 5.5 | M |
| 16 | [AtonOS computers in Academgorodok](#16-atonos-computers-in-academgorodok) | medium | cloud + Mac | Fable 5.2 | M |
| 17 | [Bicycle rounds: the paper boy and the postman](#17-bicycle-rounds) | medium | cloud (data + C++) | Opus 5.5 | M |
| 18 | [Winter: skating on the pond, the banya, the walrus swim](#18-winter-play) | medium | cloud + Mac | Opus 5.5 | M |
| 19 | [The hero's own room in the club](#19-the-heros-own-room) | medium | cloud + Mac | Fable 5.2 | L |
| 20 | [The barber and the clothes shop change the hero's look](#20-haircut-and-clothes) | medium | Mac heavy | Fable 5.2 | L |
| 21 | [The train: tickets, the conductor, the window seat](#21-the-train-as-a-passenger) | low-medium | cloud + Mac | Opus 5.5 | S |

Size: S about 2 hours of one thread, M 3-5 hours, L a whole night. "Cloud" means it is written and tested offline in the
cloud (the offline suites: `tools/citysim`, `tools/peoplesim`, `tools/mannersim`, `tools/questsim`, `tools/festivalsim`,
`tools/barksim`); "Mac" means the engine side must be built, imported or shot on the UE Mac through the queues
(`tools/wait_ubt_queue.sh`, `tools/wait_ue_queue.sh`).

## Rules every brief carries (paste them with it)

- Work on a branch from current `main`; integrate by rebase, push `HEAD:main` or open a PR. Shared files
  (`city_plan.json`, `strings_*.json` generators, `quests.json` generators) are regenerated, never hand-merged.
- Every text in ru / en / ja by `.claude/skills/localization/SKILL.md`; Russian is the source.
- Every new item gets an icon; paid generation (fal, hi3d, Higgsfield) only within what Dima already allowed; ask
  otherwise. Pin every generation in a committed sidecar.
- Perf checklist (`docs/benchmark.md`, memory "perf-checklist"): nothing rebuilt per frame, moving meshes out of Lumen,
  no per-frame allocations, bench frame + memory vs main before merge, benches alone.
- Interactables never consume input; E is interact; the hand matches what it holds.
- Check every 3D asset for UV overlap and visible artefacts before showing it.
- Deep, not wide: finish one thing well (lines, sounds, a quest, a stamp set) before starting a second.

---

## 1. Friends among the animals

`[ ]` **Brief (Fable 5.2):** Make Maru and the town's cats and dogs befriendable, using what `docs/animals.md` "Taming,
later" already prepared: `FAnimal::Bond` and `Name` (saved), the act and goal `Follow`, `SetAnimalName`, trust from calm
company, `FPlayer::bOffer`. Add: crouch + E to let an animal sniff the hand, then stroke it (the hero's hand on its
head/back by an IK goal, like the residents' gestures in `Private/People/BodyGestures.cpp`; the animal leans in, tail
wags, a cat purrs and may roll over), feeding (a sausage or fish from the Gastronom and the catch from fishing; dogs and
cats want different things), naming in the diary, and a friend who follows: Maru walks beside the hero at his pace, waits
outside doors she may not enter (shops, the library) and lies down there, sits by him on a bench, swims after him near the
shore, goes home to the club at night. Cats never follow, but a befriended cat comes to the hero when he sits near its
spot, and sleeps on his lap on a bench. A diary page "Friends" (portrait, name, how we met). Quest: "Maru's walk" (a day
with her around the town). Lines for residents who notice ("Твоя собака?"). Coordinate the stroke/lean clips with the
animations thread (it owns the animals' clips); do not edit its files, add your own layer. Done: offline test of bond
growth and follow in `UAnimalLifeSubsystem`'s pure model, Mac shots of stroking a cat and Maru waiting at a door, bench.

## 2. A film camera and the photo album

`[ ]` **Brief (Fable 5.2):** Give the hero a film camera. The photo studio on the promenade (`promenade_unit__photo`,
`photo_desk`) sells a camera and 36-frame films; the hero raises it (a key, the view through a viewfinder frame with
focus by distance and a slow shutter sound), each shot is a real capture of the view (a scene capture into a small
render target, written to the save folder as a jpg, no per-frame cost when the camera is down). The film is developed
two ways: at the studio (next day, paid) or by the hero himself in the darkroom of the club «Шестерня» (the act exists:
make it a short process with the red lamp, trays, prints hanging to dry). Prints go into a new diary page "Album" (pages
of prints with a handwritten date and place, from `UGameCalendar` and the map's place names). Photo goals as a stamp set
(`collect`): each bird species (ties into `ULyudBirdBook`: a sharp photo counts as a sighting), the sunset of each weather,
each festival's show, the aurora, a fish caught, Maru. The photographer resident gives a quest "Портрет города" (photograph
five views from a list). Residents react when photographed close (pose, wave or turn away by persona,
`PeopleManners.cpp`). Done: offline test of stamps/sightings, Mac shots and a bench with the camera up and down.

## 3. A course of cure at the sanatorium

`[ ]` **Brief (Opus 5.5):** Turn the sanatorium «Корона» (P-SANATORIUM, `tools/city/content/x_p_sanatorium.json`: 33
acts, quests `sanatorium_regime`, `sanatorium_cure`, collection `san_cure`) into a stay with a daily regime. Buy a
"путёвка" at reception (3 or 7 days); the regime board then gives the hero a day of times: morning exercise in the lobby
with the instructor and the guests (follow the moves: a short rhythm mini-game of type `rally`), mineral water at the pump
room at 8, 13, 18 (each spring its own taste line; the clock matters), quartz lamp, the oxygen cocktail, the walk on the
health path, "тихий час" (sleep on the guest-room bed), the evening dance in the lobby. Each done procedure stamps the
sanatorium card (`san_cure`); a full card gives a diploma item and a small lasting effect (the hero runs a little longer
before breathing hard, if there is such a value, else just the diploma and lines). The guests are residents of the
`hotel` plot: give four of them a story told over the days at the dining table (the same table, the same seats every
meal), e.g. the retired cosmonaut-trainee, the girl with the violin, the two old friends who argue about chess. Night: the
duty nurse shoos the hero to bed after 23. Data in the city content layer + `interiors.py`; staff talk topics for the
doctor and the nurse. Done: `tools/city/interiors.py --check`, citysim, quest audit 0 errors, questsim run of the full
course.

## 4. Small jobs

`[ ]` **Brief (Opus 5.5):** Let the hero earn money with short shift jobs that use rooms and systems that exist. Each
job is a short loop of real actions with a pay by result, at most once a day: (a) the market: help at a stall on Saturday
(fetch boxes from the van, weigh goods on the scales for customers: `CounterTrade.cpp` customer pairs), (b) the lifeguard
tower: a shift of watching the water with binoculars and setting the flags by the wind and waves (`ALyudSea`, weather),
warn swimmers past the buoys, (c) the lighthouse keeper (P-PIER): the night watch, write the log book each hour (wind,
sea, ships seen), light the lamp at sunset, (d) the café «Тарелка»: wash cups and serve ice cream at the counter (a small
`aim` mini-game for scoops), (e) the cinema: tear tickets at the door before the session, (f) the kiosk 1: the morning
papers. A job board at the house committee boards and the youth centre lists today's jobs (by weekday, season, weather).
The boss is a real resident on shift (town-life roles). Lines ru/en/ja, staff talk topics, a stamp set "Трудовая книжка"
(each job once). Use the city content layer (`acts`, `rules`, `quests`, `topics`) and add a small `job` block to the
model only if the existing act/game/quest blocks cannot express it (then extend `CityThingsModel.h` + citysim). Done:
citysim + questsim tests of each job, quest audit clean.

## 5. Telescopes that show the sky

`[ ]` **Brief (Fable 5.2):** Today the telescopes (the café «Тарелка»'s gallery, the lighthouse gallery, the Santos
family's loft, `home_santos`) are a line of text. Make looking through one a view: a round eyepiece view with a slow
manual aim (mouse, inertia), focus, magnification steps, the real sky of the game (`ALyudSky`, the sun motif, the moon
phase from the festival calendar's Meeus code, the planets the sky already places, the rare sky: aurora, noctilucent
clouds, `docs/rare_sky.md`). By day: ships on the horizon, the island, birds on the cliff (a sharp look counts as a
sighting in `ULyudBirdBook`, like the binoculars). Never look at the sun (a quick white-out and a line; the sun glare code
`docs/sun.md`). An observation log in the diary: the moon's phases, Venus, Jupiter's moons, Saturn's ring, a comet on its
festival night if added; a stamp set "Небо" and the astronomer resident (the `astronomer_home` interior) with topics and a
quest "Четыре луны Юпитера" (observe them on four nights and draw their places). Done: Mac shots of moon and Jupiter
through the eyepiece, bench of the view (a scene capture only while looking).

## 6. The town paper and notices by date

`[ ]` **Brief (Opus 5.5):** The daily paper and the notice boards exist as texts (`reading.json`, `SLyudCityBook`), but
the notices cycle without dates (backlog P4 "still open"). Make the town write about itself: a newspaper «Людмиловская
правда» each morning at the kiosks (and on the café tables), whose pages are assembled from the game's own news: the
weather forecast (the weather system's next day), the tide table, festivals soon (`ULyudFestivalSubsystem::Upcoming`),
the market prices, the fish that bit best yesterday (`ULyudEcosystem` ReportCatch counts), a rare bird seen (the bird life),
the cinema's programme, a resident's birthday or wedding from the town-life data, and the hero's own deeds ("Школьник
приручил лебедя", "Рекордный судак на пирсе"). Templates with slots, in ru/en/ja, chosen by day; old issues kept in the
library's periodicals table (the read stamp for each). The notice boards get dated notices (lost cat, choir practice
Thursday, a flat swap) that appear and go by date. Done: an offline generator test for a year of days (no empty or
repeated front page two days running), citysim, the reading page shows it.

## 7. Kites on windy days

`[ ]` **Brief (Opus 5.5):** Sell kites at the toy shops (P-WAVE toys, the department store's toy house, the fair's toy
kiosk) and let the hero fly one on the beach and the cliff top. The kite is a small code-built mesh (a diamond and a box
kite, our own colours) on a string (a sagging line of a few segments), lifted by the game's real wind (`docs/sea_and_wind.md`,
gusts): too little wind and it falls, too much and it spins and must be reeled in; the mouse pulls and lets out the line,
the hold of the reel in the hero's hands. On windy afternoons children of the town fly kites too (town-life activity
`fly_kite` on the beach spots, weighed by wind like `Rain`/`TempC` weigh choices in the manners stream). A kite caught on a
tree or a lamp post is lost (or a quest to get it down for a child). Lines, a stamp ("Воздушный змей"), a sound of the
string humming in the wind. Done: an offline test of the lift model, Mac shots and a bench with ten kites in the sky.

## 8. The beach at play

`[ ]` **Brief (Opus 5.5):** Small things to do on the beach and pier, each a minute long, using the sea and tides: (a)
skipping stones: pick a flat stone on the shingle (some are flat, most not), throw with a timed release, count the skips
on the real water surface (`ALyudSea` height), the town record on the lifeguard board; (b) sandcastles: build at the
damp sand line by holding E (a few growing stages of a code-built castle, a moat, a flag from the kiosk), the tide
(`docs/tides.md`) washes it away hours later, children join in; (c) the buoy swim: a timed swim to the buoys and back
(`docs/swimming.md`), the lifeguard times it, residents who swim race the hero in summer; (d) shells and amber after a
storm along the waterline, sold at the souvenir unit or kept (the amber charm item exists). Lines ru/en/ja, stamps,
a small quest "Янтарный берег". Done: offline tests of the skip count and the tide wash, Mac shots, bench.

## 9. An allotment

`[ ]` **Brief (Fable 5.2):** Give the hero a garden bed: a strip of the allotments (or the commune slab's roof garden,
whichever the city plan has room for: ask the coordinator before touching `city_plan.json`) rented from the house
committee. Seeds from the market and the hardware shop, dig (a spade), sow, water (the watering can, the sanatorium already
has an act `sanatorium_watering`), weed, then the plants grow by the game calendar and weather with the **procedural plant
system** (`PlantSpeciesLibrary`, `docs/plants_bloom.md`; all vegetation must use it): radishes in 3 days, potatoes,
tomatoes, sunflowers, strawberries, dill. Too dry, too cold or too many weeds slow them; frost kills. Harvest goes to the
inventory: eat, give to residents (bond), sell at the market stall, or enter the market's jam contest
(`city.reg.*` market notice). The neighbour on the next bed is a resident with advice by season. Done: offline growth test
over a game season, Mac shots of three stages, bench (no per-frame plant rebuilds).

## 10. Records, the jukebox and the town radio

`[ ]` **Brief (Opus 5.5):** Make music something the hero chooses. Records to buy at the records shops (P-WAVE records,
the department store's records department; today talk only because there are no items): give records real items with
icons, each holding one track of the game's music (`/Game/Audio/Music`, `audio/sounds.json`). Play them on the café's
jukebox (`jukebox` act + the jukebox records collection), on a record player in the hero's room (if item 19 is done) or the
club. The town radio: radios in rooms already play a tone; make a station «Волна Людмиловска» with a schedule (morning
news read from item 6's paper, the weather and tide, music hours, a request show where the hero's posted request plays
the next evening, the shipping forecast at night). The announcer's voice by TTS through fal (pinned in
`localization/voice.json`; **ask Dima for the cost** before generating); until then the station plays music and chimes.
Done: an offline schedule test, the jukebox and radio play in game, a stamp set of records.

## 11. Letters, postcards and pen friends

`[x] letters 2026-11-20 (branch letters, docs/letters.md; awaits a Mac build)` **Brief (Opus 5.5):** Postcards are sold at the kiosk; let the hero send them. Write a postcard (choose the picture,
pick two phrases from a list of warm sentences, the address of a resident from the town data), drop it in a postbox
(postboxes on the street spots); the resident gets it the next day (the bond grows, they mention it in a bark, the
postcard stands on their shelf in the open home). Letters come to the hero too: a mailbox in the club (the hero's address),
residents who know him write (an invitation, a thank-you after a quest), and a pen friend in another town (a long chain of
letters over weeks, the story of a girl in a lighthouse town far north who also watches birds; her letters ask what the
hero saw, his answers are chosen from what he really did: species seen, fish caught). Letters are diary pages. Done:
offline tests of delivery by calendar days and the pen-friend chain, lines ru/en/ja.

## 12. Friends among the residents

`[ ]` **Brief (Fable 5.2):** The townsfolk already come to know the hero (`NoteMet`, `met = new|seen|known`, town-manners).
Add a friendship level per named resident (saved): it grows with talk topics heard, quests done for them, gifts (an item
they like by persona: flowers, a fish, a jar of jam, a record), postcards (item 11); a friend waves first, calls the hero by
name, sits with him on a bench and talks about their day (lines by `doing`, weather, festival), invites him to tea at their
open home (the ten `home_*` flats: the hero sits at their table, a short conversation of three choices, the tea served) and
to their family's weekend outing (`FPerson::Outing*`). A diary page "Люди" with what the hero knows of each friend. Keep it
calm: no romance, no meters on screen, only lines and gestures that change. Done: peoplesim/mannersim tests, barks rebuilt
(`tools/barks/build_barks.py`), Mac shots of a tea at a home.

## 13. The cinema shows real films

`[ ]` **Brief (Opus 5.5):** The house of culture «Рассвет» has film sessions by the clock, but the film is only lines.
Show real public-domain films on its screen (and the club's summer screen on festival nights): silent shorts and early
animation whose copyright has ended worldwide (check each title's status; keep a `SOURCES.md` with licences), encoded small
(a media texture on the screen material, a projector beam with dust, the hall lights dim, the audience laughs at the right
places). The hero buys a ticket, sits (`SitOn`), watches, may leave any time; a ticket stub item for the films collection
(backlog P4 "cinema ticket stubs as items"). Subtitles for intertitles in ru/en/ja. Coordinate with the library-books
thread (it handles public-domain sourcing too: share the licence check). Done: two films playing in game, bench in the
hall during a session.

## 14. Club tournaments

`[ ]` **Brief (Opus 5.5):** The mini-games exist (chess, checkers, dominoes, billiards, table tennis, darts: `games.json`,
ten regulars with styles) but the regulars are not seen at the tables and there is no ladder (backlog P4). Do: the
regulars sit at their tables at their hours (life-sim spots on the game things, `FindFreeThing`/`ClaimThing`), play each
other (pieces move, a clock ticks, a short cheer), and the hero can watch or challenge; a weekly tournament per club
(Saturday chess at «Шестерня», dominoes in the sanatorium lounge, table tennis at the culture block) with a bracket on the
board, the hero's results in the paper (item 6), a cup item on a shelf of the club. Fix the open data points: the x_ chess
tables that don't `like` chess, the `what=checkers|dominoes|badminton` things. Done: citysim tournament test, Mac shots
of two regulars at a chess table.

## 15. A playable piano and guitar

`[ ]` **Brief (Opus 5.5):** The pianos (the hall «Своды», the school's music room, the sanatorium lobby) are a line today.
Let the hero play: a keyboard view (a row of keys mapped to the keyboard, one octave and a half, sustain on Shift),
sampled or synthesised notes (procedural like `tools/festival_sounds.py`, seeded, no paid audio), and "known tunes" he learns
from sheet music found in the town (the music room, the library) that then play by pressing E in rhythm (an easy rhythm
game). Residents nearby stop and listen, clap. A guitar from the records shop for the beach bonfire nights (midsummer, the
festival bonfire): the same rhythm game, the residents round the fire sing along. Done: notes in tune (an offline test of the
synth frequencies), Mac recording of a tune, bench.

## 16. AtonOS computers in Academgorodok

`[ ]` **Brief (Fable 5.2):** The game has its own desktop OS (`Private/AtonOS/`: `SAtonDesktop`, `AtonApps`, the
computer in the club, `AtonComputer.cpp`). Put working terminals in Academgorodok (the consoles hall in
`docs/academgorodok/`, `academ_photo_consoles.jpg`) and give them apps that fit the lab: a mail client with the lab's old
correspondence (lore toward the ending, `docs/story/`: hints, never the answer), a text adventure the scientists wrote for
fun, a chess program to play against, the weather station's live data (the game's weather), a telescope queue (links to
item 5), and a log-in puzzle (the password is somewhere in the town). Coordinate the lore with `docs/story/inbox.md` and add
new lore there as a proposal. Done: the apps work on the club computer first (offline tests where pure), Mac shots in
Academgorodok.

## 17. Bicycle rounds

`[ ]` **Brief (Opus 5.5):** Bicycles exist (`docs/bikes.md`, the hero rides, residents ride). Add rounds: the morning paper
round (fetch the bundle at kiosk 1 at 6, throw papers to the doorsteps of the houses on a route of the city plan's
addresses, timing and aim count) and the post round (parcels to named residents' flats; ring, wait, they open: lines by
persona). Pay per round, a route map in the diary's map page, the postman resident as the boss, a stamp set by terraces.
Coordinate with item 4 (same job board). Done: questsim route test, Mac ride-through shots.

## 18. Winter play

`[ ]` **Brief (Opus 5.5):** Winter exists (`docs/winter.md`: snow, footprints, sea ice, the first-snow festival) and the park
pond freezes (`docs/city/public_spaces.md` open: "skaters on the ice"). Add: skates at the sport shop, skating on the pond
(a glide gait with momentum, falls on cracks), residents skating in the evenings and on first snow; a banya at the
sanatorium or by the beach (heat, the birch venik, then the snow or the sea: the walrus swim at the ice hole on New Year's
morning, residents who do it every Sunday); snowball fights with children (throw like the bird feed, `G`), building a
snowman. Done: offline tests where pure, Mac shots of the pond, bench with skaters.

## 19. The hero's own room

`[ ]` **Brief (Fable 5.2):** Give the hero a room of his own in the club (a small room off the hall: ask the coordinator
which), furnished from the interior kit (`blender/city/interior_kit.py`): a bed (sleep until an hour, the existing chooser),
a shelf and a wall where things he owns can be placed: prints from the camera (item 2), records and a player (item 10),
postcards (item 11), the bird book, trophies (cups, the sanatorium diploma), a plant in a pot that grows (procedural plants).
Placing is simple: E on a free spot picks from what he has. Saved. Maru's basket by the door (item 1). Done: Mac shots of
a furnished room, save/load test.

## 20. Haircut and clothes

`[ ]` **Brief (Fable 5.2):** The barber's haircut and the department store's clothes departments exist as acts, but change
nothing (backlog P3/P1 "thin"). Make them change the hero's look: three or four haircuts (hair meshes on the hero's skeleton,
`Content/Characters/Hero` is local-only: work on the Mac), clothes as items worn (the inventory's "worn" page,
`diary/screens/03_inventory_worn.png`): a jacket, a raincoat for rain, a winter coat, a cap; the hero's shadow mesh and the
first-person arms follow. Residents comment on a new haircut once. Done: Mac shots of each look, no new draw-call spike in
the bench.

## 21. The train as a passenger

`[ ]` **Brief (Opus 5.5):** The sea railway's open list has "a ticket or token at the kiosk" (`docs/railway.md`,
`docs/transport.md`). Add a ticket at the halt kiosk, a conductor who walks the carriage and punches it (lines by persona;
no ticket: a fine or a stern word), the window seat (sit, look out at the sea and gulls, `SitOn` with a look cone), the
announcements of the halts. Small and self-contained. Done: questsim test, Mac shots on the train.

---

## For the quota watcher

Start from the top; items 1-6 are the best value. Items marked Fable 5.2 are the hardest; Opus 5.5 can take the rest. Two
threads can run in parallel without touching each other's files: 1 with 3, 2 with 6, 4 with 5, 7 with 9; avoid running 4
and 17 together (same job board), 10 before 19 helps 19, and 11 before 12 helps 12. Everything in `tools/city/content/`
is shared by items 3, 4, 6, 14: they must rebase and rerun `python3 tools/city/interiors.py` before each push.
