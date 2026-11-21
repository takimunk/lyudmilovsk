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

`[~] animal-friends 2026-11-20` (branch `animal-friends`; what was built: `docs/animals.md` "Friends") **Brief (Fable 5.2):** Make Maru and the town's cats and dogs befriendable, using what `docs/animals.md` "Taming,
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

`[~] film-camera 2026-11-20 (branch ready, docs/photo.md)` **Brief (Fable 5.2):** Give the hero a film camera. The photo studio on the promenade (`promenade_unit__photo`,
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

`[~] sanatorium-cure 2026-11-20 (branch ready, waiting for merge)` **Brief (Opus 5.5):** Turn the sanatorium «Корона» (P-SANATORIUM, `tools/city/content/x_p_sanatorium.json`: 33
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

`[~]` small-jobs 2026-11-20 (branch ready, waiting for merge; docs/city_interiors.md "Small jobs") **Brief (Opus 5.5):** Let the hero earn money with short shift jobs that use rooms and systems that exist. Each
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

`[~] «Сделать телескопы с настоящим небом» 2026-11-20 (branch `telescopes`, docs/telescopes.md)` **Brief (Fable 5.2):** Today the telescopes (the café «Тарелка»'s gallery, the lighthouse gallery, the Santos
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

`[~] town-paper 2026-11-20` (docs/town_paper.md) **Brief (Opus 5.5):** The daily paper and the notice boards exist as texts (`reading.json`, `SLyudCityBook`), but
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

`[~] kites 2026-11-20` **Brief (Opus 5.5):** Sell kites at the toy shops (P-WAVE toys, the department store's toy house, the fair's toy
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

`[x] allotment 2026-11-20 (branch allotment, docs/allotment.md; awaits a Mac build, icons cut, shots and a bench)` **Brief (Fable 5.2):** Give the hero a garden bed: a strip of the allotments (or the commune slab's roof garden,
whichever the city plan has room for: ask the coordinator before touching `city_plan.json`) rented from the house
committee. Seeds from the market and the hardware shop, dig (a spade), sow, water (the watering can, the sanatorium already
has an act `sanatorium_watering`), weed, then the plants grow by the game calendar and weather with the **procedural plant
system** (`PlantSpeciesLibrary`, `docs/plants_bloom.md`; all vegetation must use it): radishes in 3 days, potatoes,
tomatoes, sunflowers, strawberries, dill. Too dry, too cold or too many weeds slow them; frost kills. Harvest goes to the
inventory: eat, give to residents (bond), sell at the market stall, or enter the market's jam contest
(`city.reg.*` market notice). The neighbour on the next bed is a resident with advice by season. Done: offline growth test
over a game season, Mac shots of three stages, bench (no per-frame plant rebuilds).

## 10. Records, the jukebox and the town radio

`[~] records-radio 2026-11-20` **Brief (Opus 5.5):** Make music something the hero chooses. Records to buy at the records shops (P-WAVE records,
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

`[x] friends 2026-11-20 (branch friends, docs/friends.md; awaits a Mac build and shots)` **Brief (Fable 5.2):** The townsfolk already come to know the hero (`NoteMet`, `met = new|seen|known`, town-manners).
Add a friendship level per named resident (saved): it grows with talk topics heard, quests done for them, gifts (an item
they like by persona: flowers, a fish, a jar of jam, a record), postcards (item 11); a friend waves first, calls the hero by
name, sits with him on a bench and talks about their day (lines by `doing`, weather, festival), invites him to tea at their
open home (the ten `home_*` flats: the hero sits at their table, a short conversation of three choices, the tea served) and
to their family's weekend outing (`FPerson::Outing*`). A diary page "Люди" with what the hero knows of each friend. Keep it
calm: no romance, no meters on screen, only lines and gestures that change. Done: peoplesim/mannersim tests, barks rebuilt
(`tools/barks/build_barks.py`), Mac shots of a tea at a home.

## 13. The cinema shows real films

`[~] cinema 2026-11-20` (docs/cinema.md: nine public-domain shorts of 1895–1908 as the newsreel before every Rassvet session,
lecturer's subtitles ru/en/ja, dark hall, beam, laughs, ticket stub, the paper prints the programme; cinemasim. Open: the
films fetched and the materials, sounds and stub icon made on the Mac, Mac shots; the club's summer screen.)

**Brief (Opus 5.5):** The house of culture «Рассвет» has film sessions by the clock, but the film is only lines.
Show real public-domain films on its screen (and the club's summer screen on festival nights): silent shorts and early
animation whose copyright has ended worldwide (check each title's status; keep a `SOURCES.md` with licences), encoded small
(a media texture on the screen material, a projector beam with dust, the hall lights dim, the audience laughs at the right
places). The hero buys a ticket, sits (`SitOn`), watches, may leave any time; a ticket stub item for the films collection
(backlog P4 "cinema ticket stubs as items"). Subtitles for intertitles in ru/en/ja. Coordinate with the library-books
thread (it handles public-domain sourcing too: share the licence check). Done: two films playing in game, bench in the
hall during a session.

## 14. Club tournaments

`[~] tournaments 2026-11-20` (docs/city_interiors.md "Tournaments": three weekly cups, brackets, sign-up, the hero's matches,
the cup, the paper's results; citysim. Open: the regulars seen sitting at the tables, life-sim side, and the Mac shots.)

**Brief (Opus 5.5):** The mini-games exist (chess, checkers, dominoes, billiards, table tennis, darts: `games.json`,
ten regulars with styles) but the regulars are not seen at the tables and there is no ladder (backlog P4). Do: the
regulars sit at their tables at their hours (life-sim spots on the game things, `FindFreeThing`/`ClaimThing`), play each
other (pieces move, a clock ticks, a short cheer), and the hero can watch or challenge; a weekly tournament per club
(Saturday chess at «Шестерня», dominoes in the sanatorium lounge, table tennis at the culture block) with a bracket on the
board, the hero's results in the paper (item 6), a cup item on a shelf of the club. Fix the open data points: the x_ chess
tables that don't `like` chess, the `what=checkers|dominoes|badminton` things. Done: citysim tournament test, Mac shots
of two regulars at a chess table.

## 15. A playable piano and guitar

`[x]` (branch instruments, docs/instruments.md; Mac compile, recording and bench by the merge thread) **Brief (Opus 5.5):** The pianos (the hall «Своды», the school's music room, the sanatorium lobby) are a line today.
Let the hero play: a keyboard view (a row of keys mapped to the keyboard, one octave and a half, sustain on Shift),
sampled or synthesised notes (procedural like `tools/festival_sounds.py`, seeded, no paid audio), and "known tunes" he learns
from sheet music found in the town (the music room, the library) that then play by pressing E in rhythm (an easy rhythm
game). Residents nearby stop and listen, clap. A guitar from the records shop for the beach bonfire nights (midsummer, the
festival bonfire): the same rhythm game, the residents round the fire sing along. Done: notes in tune (an offline test of the
synth frequencies), Mac recording of a tune, bench.

## 16. AtonOS computers in Academgorodok

`[~]` *(branch `aton-lab`, docs/aton_lab.md: six lab programs, one terminal in the scientists' library, the password slip in the
Bridge House; the offline suite tools/atonsim passes and it builds on the Mac; the first run in the level and the shots are still owed)* **Brief (Fable 5.2):** The game has its own desktop OS (`Private/AtonOS/`: `SAtonDesktop`, `AtonApps`, the
computer in the club, `AtonComputer.cpp`). Put working terminals in Academgorodok (the consoles hall in
`docs/academgorodok/`, `academ_photo_consoles.jpg`) and give them apps that fit the lab: a mail client with the lab's old
correspondence (lore toward the ending, `docs/story/`: hints, never the answer), a text adventure the scientists wrote for
fun, a chess program to play against, the weather station's live data (the game's weather), a telescope queue (links to
item 5), and a log-in puzzle (the password is somewhere in the town). Coordinate the lore with `docs/story/inbox.md` and add
new lore there as a proposal. Done: the apps work on the club computer first (offline tests where pure), Mac shots in
Academgorodok.

## 17. Bicycle rounds

`[~] bike-rounds 2026-11-20 (branch bike-rounds, docs/rounds.md; awaits a Mac build and ride-through shots)` **Brief (Opus 5.5):** Bicycles exist (`docs/bikes.md`, the hero rides, residents ride). Add rounds: the morning paper
round (fetch the bundle at kiosk 1 at 6, throw papers to the doorsteps of the houses on a route of the city plan's
addresses, timing and aim count) and the post round (parcels to named residents' flats; ring, wait, they open: lines by
persona). Pay per round, a route map in the diary's map page, the postman resident as the boss, a stamp set by terraces.
Coordinate with item 4 (same job board). Done: questsim route test, Mac ride-through shots.

## 18. Winter play

`[~] winter-play 2026-11-20 (branch winter-play, docs/winter_play.md; awaits a Mac build, icons and shots)` **Brief (Opus 5.5):** Winter exists (`docs/winter.md`: snow, footprints, sea ice, the first-snow festival) and the park
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

`[~] train-passenger 2026-11-20` (docs/railway.md "Riding as a passenger": ticket boxes on the halts, three conductors who
walk the car and punch or fine, announcements; tools/railsim/tickets.sh. Open: Mac compile and shots, the two ticket icons.)

**Brief (Opus 5.5):** The sea railway's open list has "a ticket or token at the kiosk" (`docs/railway.md`,
`docs/transport.md`). Add a ticket at the halt kiosk, a conductor who walks the carriage and punches it (lines by persona;
no ticket: a fine or a stern word), the window seat (sit, look out at the sea and gulls, `SitOn` with a look cone), the
announcements of the halts. Small and self-contained. Done: questsim test, Mac shots on the train.

---

## For the quota watcher

Start from the top; items 1-6 are the best value. Items marked Fable 5.2 are the hardest; Opus 5.5 can take the rest. Two
threads can run in parallel without touching each other's files: 1 with 3, 2 with 6, 4 with 5, 7 with 9; avoid running 4
and 17 together (same job board), 10 before 19 helps 19, and 11 before 12 helps 12. Everything in `tools/city/content/`
is shared by items 3, 4, 6, 14: they must rebase and rerun `python3 tools/city/interiors.py` before each push.

---

# Part 2 (2026-11-20, 23:00 UTC): deepen and tie together

Written when items 1-19 and 21 of part 1 were in work or merged. Same rules as above ("Rules every brief carries":
paste them with each brief). Part 2 does not add new systems: it polishes what exists and **ties tonight's new features to
each other** (paper, radio, cinema, festivals, sanatorium, kites, beach, allotment, camera, friends, letters, jobs,
instruments, tournaments, AtonOS lab). Where a brief depends on a part-1 branch that is not in main yet, branch from that
branch (or wait for its merge) and say so in the PR.

## Order (part 2)

| # | Item | Value | Where it runs | Model | Size | Touches |
|---|---|---|---|---|---|---|
| 22 | [The town reacts to the hero's day](#22-the-town-reacts-to-the-heros-day) | very high | cloud | Fable 5.2 | L | barks, paper, friends |
| 23 | [Hero's day log: the diary writes itself](#23-the-diary-writes-itself) | very high | cloud | Opus 5.5 | M | diary UI, quests events |
| 24 | [Festival days use all the new toys](#24-festival-days-use-the-new-toys) | high | cloud + Mac shots | Opus 5.5 | M | festivals.json, content |
| 25 | [Lived-in rooms: clutter that changes](#25-lived-in-rooms) | high | cloud (Blender) + Mac | Opus 5.5 | L | blender/city buildings |
| 26 | [Residents do the new activities too](#26-residents-do-the-new-activities) | high | cloud | Fable 5.2 | L | town_life, manners |
| 27 | [Counters with real service](#27-counters-with-real-service) | high | cloud + Mac | Opus 5.5 | M | CounterTrade, content |
| 28 | [Rain, wind and night change what people do indoors](#28-weather-and-night-indoors) | medium-high | cloud | Opus 5.5 | M | content rules, tones |
| 29 | [Night state geometry and the cleaner](#29-night-state-geometry-and-the-cleaner) | medium-high | cloud (Blender) + Mac | Opus 5.5 | M | city things, town_life roles |
| 30 | [Collections pay off](#30-collections-pay-off) | medium-high | cloud | Opus 5.5 | S | collections.json, diary |
| 31 | [Small touch details everywhere](#31-small-touch-details) | medium-high | cloud + Mac | Opus 5.5 | M | city content, things |
| 32 | [Children's games in the yards](#32-childrens-games-in-the-yards) | medium | cloud + Mac | Fable 5.2 | M | town_life, manners |
| 33 | [Photo quests and the photo contest](#33-photo-quests-and-the-photo-contest) | medium | cloud | Opus 5.5 | S | camera branch, paper |
| 34 | [Allotment to kitchen: cook what you grow and catch](#34-allotment-to-kitchen) | medium | cloud + Mac | Opus 5.5 | M | food, allotment, fishing |
| 35 | [The rental unit and the promenade windows](#35-rental-and-promenade-windows) | medium | cloud (Blender) + Mac | Opus 5.5 | M | promenade units |
| 36 | [Voices for the staff (paid, already approved budget)](#36-voices-for-the-staff) | medium | cloud (fal) + Mac import | Opus 5.5 | M | voice.json, sidecars |

**Running in parallel without touching the same files:** 22 ∥ 25 ∥ 34 ∥ 36; 23 ∥ 27 ∥ 32; 24 ∥ 29 ∥ 33; 26 alone with
the people sim (it owns `tools/town/town_life.py` and `PeopleManners.cpp` for the night; 29 and 32 must wait or rebase on
it, or keep their town-life change to one data entry each); 28 and 31 both edit `tools/city/content/`: run them one after
the other, or split by building (28 public rooms, 31 houses). Everyone touching `tools/city/content/` reruns
`python3 tools/city/interiors.py` and `--check` after rebasing; whoever touches barks reruns `tools/barks/build_barks.py`.

## 22. The town reacts to the hero's day

`[~] town-talk-hero 2026-11-20 (branch ready, waiting for merge; docs/town_rumours.md)` **Brief (Fable 5.2):** Tonight added many things the hero can do (jobs, kites, beach games, cinema, tournaments,
letters, friends, the sanatorium course, instruments). Make the town notice. Collect the hero's notable events of the day
(quest events `ULyudQuests::Notify` already carry most of them: wins, catches, jobs done, festival watches, birds tamed) into
a small "talk of the town" store (saved, decays over 3-5 game days), and feed it to: barks of residents who saw it or heard
it from a friend (`TownBarks.cpp`, `tools/barks/lines/`, a new condition `heard_of = <event>`; gossip spreads along
friendships and households, so a far terrace hears it a day later), staff talk topics ("Говорят, ты вчера выиграл у
Петровича"), the town paper (item 6's slots), friends' letters (item 11). Calm and rare: at most one remark per resident per
day. Done: an offline test of spreading and decay (barksim/peoplesim), lines ru/en/ja, barks rebuilt.

## 23. The diary writes itself

`[~] diary-days 2026-11-20` **Brief (Opus 5.5):** A page "Дни" in the diary (`SLyudDiary.cpp`, `docs/diary.md`): each game day gets a short
handwritten entry made from what really happened (the weather, the festival, the people met, birds seen, fish caught, the
job, the film watched, the tune played, the letter received), in the hero's teenage voice, from templates with slots and
variants (ru/en/ja, never the same sentence two days running), with small margin doodles from the existing sketch textures
(`diary/textures/`). The camera's prints (item 2) can be glued onto a day. Done: an offline generator test over 60 days of
synthetic events, Mac shot of the page.

## 24. Festival days use the new toys

`[~] festival-toys 2026-11-20` (branch `festival-toys`; what was built: `docs/festivals.md` "The new toys"; never compiled or seen in the engine) **Brief (Opus 5.5):** Festivals (`Content/PeopleData/festivals.json`, `docs/festivals.md`) know nothing of tonight's
features. Tie them: the cinema shows a festival programme (the sea festival, New Year cartoons), the radio plays a festival
broadcast, the tournament of the week becomes a festival cup on its day, kites fly on Children's Day and the blossom
picnic, the beach games become contests on Fisherman's Day (stone skipping championship), the sanatorium has a dance
evening on holidays, the paper prints a festival issue, friends invite the hero to go together, the allotment's harvest goes
to the harvest fair (a prize for the biggest pumpkin). Data first (the festival's program, content `festival` conditions),
code only where a hook is missing. Done: festivalsim + citysim, `lyud.Festival.Go` walk-through shots on the Mac.

## 25. Lived-in rooms

`[ ]` **Brief (Opus 5.5):** Interiors are furnished but static. Add clutter that tells time passing: a cup left on the
table after a resident's breakfast, a newspaper of today on the café table (item 6), wet umbrellas by doors on rain days,
coats on racks in winter, a half-done jigsaw in the sanatorium lounge that grows day by day, children's drawings on the
school wall that change weekly, flowers in a vase that wilt and get replaced (backlog P5 "seasonal dressing"). Build as a
small set of swappable dressing pieces per room (`B.thing`-like `B.dress(...)` with a condition: weekday, weather, season,
day count), placed in Blender export and switched in the game without rebuilding meshes (visibility of instanced pieces,
never per-frame). Done: Blender previews of three rooms in two states, perf bench on the Mac.

## 26. Residents do the new activities

`[~] residents-activities 2026-11-20` (docs/town_life.md "The new activities") **Brief (Fable 5.2):** Make the townsfolk use tonight's features by persona and weather: kids fly kites and skip
stones, pensioners take the sanatorium cure and play dominoes in its lounge, the regulars go to the cinema (seats fill,
`FindFreeThing`), people read the paper on benches (the `read_bench` activity holding the new paper prop), someone plays the
piano in the hall in the evening (item 15's synth, rarely), allotment neighbours dig and water, the postman does his round,
friends walk with the hero sometimes. New activities in `tools/town/town_life.py` and the manners (`PeopleManners.cpp`,
`Content/PeopleData/manners.json`), weighed like `Rain`/`TempC`. Done: peoplesim + mannersim pass (tune seed-chaotic checks),
Mac shots of three new activities, bench (no new per-frame cost per person).

## 27. Counters with real service

`[~]` counter-service 2026-11-20 (branch ready, waiting for merge; docs/counters.md; Mac compile and queue shots by the merge thread) **Brief (Opus 5.5):** At counters the hero buys from a menu. Make it a little scene: the staff turns to him, says the
greeting by hour and by whether they know him (friends, item 12), the item is put on the counter and the hero takes it (the
hand reaches, `UHeroReachComponent::Take`), change is counted, a queue forms behind him at busy hours and residents wait
their turn (customer pairs exist: `LyudBarks::AtCounter`, `CounterTrade.cpp`). Fix P-MARKET counters never firing
(town-manners memory: give the market visit slots from `shell_market.json` counter spots). Done: citysim, Mac shots of a
queue at the bakery and the market.

## 28. Weather and night indoors

`[x] indoor-weather 2026-11-20 (branch indoor-weather, docs/indoor_weather.md; awaits a Mac build and a rain-day walk)` **Brief (Opus 5.5):** On rain days the cafés, the library, the clubs and the sanatorium lounge fill up; on stormy
nights the lighthouse keeper's radio crackles with ship talk; on hot days the ice cream counter has a queue and the café's
windows open. Lines exist by weather; extend to who is where (town-life weights by weather per plot kind), room tones
(`tones`, `tone_by`: rain on the roof, the wind in the lighthouse), lamps on earlier on dark days. Done: peoplesim test of
indoor counts by weather, citysim, a Mac walk on a rain day.

## 29. Night state geometry and the cleaner

`[~]` 2026-11-21 (branches night-state + night-people, docs/city_night.md): chairs up on the tables of the café, the
Rassvet, the cog club's buffet and the store's café after hours, the library's pushed in, a "closed" lamp kind (one lamp
left on, the windows faint), the cleaner's bucket and damp floor before opening, two cleaners and the store's night
watchman with a torch in town life (manners mop, torch; props mop, torch, bucket). Blender previews done; Mac build,
prop import, shots at 23:00 and 07:30, bench owed.

**Brief (Opus 5.5):** Backlog P5 "night state of public rooms" has the data but no geometry: chairs on tables in
closed cafés and clubs, a single lamp left on, a cleaner at work 7-10 with a mop and a bucket (a town-life role `cleaner`,
a manner prop), the watchman in the department store at night with a torch. Use swappable pieces like item 25 (coordinate
with it: same `B.dress` mechanism, one of you writes it). Done: Blender previews, Mac shots at 23:00 and 07:30, bench.

## 30. Collections pay off

`[~] collections-payoff 2026-11-21 (model, data, diary pages, citysim and quest audit done; never compiled or seen in the engine)` **Brief (Opus 5.5):** Stamp sets exist (library shelves, films, jukebox records, kiosks, chess wins, the sanatorium
card, tonight's new sets) but a complete set gives nothing (backlog P4). Give each set a reward that fits: a library reader's
card with a photo, a framed film poster for the hero's room (item 19), a record of your choice, the kiosk keepers' free
paper for a month, a chess club badge on the jacket, the sanatorium diploma. A diary page "Коллекции" with all sets, filled
and empty slots (the page `lyud.City.Collections` exists: move it into the diary). Done: citysim test of each reward,
quest audit clean.

## 31. Small touch details

`[~] touch-details 2026-11-21 (branch ready, waiting for merge; docs/city_interiors.md "Small things that answer a touch")` **Brief (Opus 5.5):** A pass over houses and lobbies (P2 of `docs/city_backlog.md`) for small things that answer a
touch: mailboxes (the hero's own one has letters: item 11), light switches and wall clocks that no building declares yet
(`what=switch`, `look_clock` exist), a radio to turn on, a tap to run, a window to open (the room's tone changes), a cat on
the radiator to stroke (item 1), a lift button (the lifts' `to`/`floors` exist but no building declares a link: declare
them for the unite block, the stacked beam tower and the capsule towers). Done: reach audit and quest audit, citysim, Mac
walk tests in two buildings with lifts.

## 32. Children's games in the yards

`[~] yard-games 2026-11-21 (branch ready, docs/yard_games.md; awaits Mac build, shots, bench)` **Brief (Fable 5.2):** Children of the town play together after school: hopscotch on chalked squares, a skipping
rope, hide-and-seek in the yards (one counts at a wall, others hide behind the town's own cover; the hero can join and be
found or find), football with a ball that rolls (physics, a code-built ball), tag. A small group behaviour in the people
sim (a game leader, roles, a play area from the city plan's yard spots), gestures by the hand-goal IK layer. Done: peoplesim
test of a game's start, roles and end, Mac shots, bench with three groups playing.

## 33. Photo quests and the photo contest

`[~] photo-contest 2026-11-20` **Brief (Opus 5.5):** On top of the camera (item 2): the photo club at «Шестерня» runs a monthly contest with a theme
(the sea, birds, people at work, the festival) announced in the paper; the hero hands in one print, residents of the club
judge (a score from what is in the frame: subject tags from the capture, light by sun height, sharpness), the winners' prints
hang in the club and the paper prints the hero's photo. Three photo quests from residents (a portrait of a pensioner for his
grandson, the old pier before it is repainted, Maru for the club's wall). Done: citysim/questsim tests with synthetic shots.

## 34. Allotment to kitchen

`[ ]` **Brief (Opus 5.5):** Tie the allotment (item 9), fishing and food: cook at the commune canteen's stove, the
sanatorium kitchen or the hero's room (a hot plate) from what he grew and caught: fish soup (ukha), fried potatoes with dill,
a salad, strawberry jam in jars (the market's jam contest). Short recipes (combine items, wait minutes, a sizzle sound), the
dish as a food item to eat (`ULyudFoodSubsystem`) or give to friends (bond). The restaurant buys fresh vegetables. Done:
game-items skill for each new item (icons), citysim of recipes, Mac shot of cooking.

## 35. Rental and promenade windows

`[x] rental-windows 2026-11-21 (branch rental-windows, docs/city_interiors.md "The rental's beach set" and "Shop windows"; never compiled or seen in the engine: awaits a Mac build, the world mesh and atlas re-import, dusk shots and a bench; bicycles stay free)` **Brief (Opus 5.5):** Two open items of P3: the rental unit only talks (rent deck chairs and parasols for the beach,
place them, lie in the sun; tie the rowing boat hire `ALyudRowBoat` and bicycles to the same counter), and the promenade
units are opaque from outside (make the lit room readable through the glass: an interior cubemap card or the real room
behind glass with cheap lighting, by distance). Signboards for fish, rental, workshop, souvenirs (`sign_*` cells in
`tools/city/atlas.py`). Done: Blender previews, Mac shots from the promenade at dusk, bench along the promenade.

## 36. Voices for the staff

`[ ]` **Brief (Opus 5.5):** Staff talk has 218 answer lines with no voice (backlog P4). Voice the greetings and the top
topics of the ten most visited counters in ru/en/ja with the pinned TTS (`localization/voice.json`, the game-assets skill,
sidecars; teen hero rules do not apply, adults by persona). Dima pre-approved the night's work (2026-11-20 19:32 UTC) and allowed fal
for the birds (about $5); keep this pass small (a few dollars), ask the coordinator for the night's fal total first, and
record the spend in the sidecars. Import on the Mac
(`import_voice.py`). Done: lines play at the counters, subtitles match, the voice list committed.

---

# Part 3 (2026-11-21, 00:10 UTC): Dima's own asks, deeper

Written for the quota thread when part 2 was all given out. Part 3 goes back to what Dima asked for himself on
2026-11-20 (the way from Aton to the town, the cable-car stations, birds, animals in the town, reading in the library,
the sanatorium) and takes each one a step deeper than tonight's threads did. Same rules ("Rules every brief carries":
paste them with each brief), same marks.

Two of tonight's branches are the ground for several items and were **not in main** when this was written: `aton-coast`
(PR #31, `docs/south.md`, `docs/south_backlog.md`, `tools/south.py`) and `stations-v3` (the dressed station halls,
`halls.json`, `unreal/scripts/test_station_halls.py`). An item that builds on one of them branches from it and says so
in its PR; the merge thread takes the base first.

**Cloud-only** in the table means the whole item is written and proven in the cloud: data, C++ and the offline suites,
no new mesh, picture or sound to push through LFS (the cloud cannot push LFS) and no shot needed to call it done. The Mac
then only compiles it in the merge thread's batch. Everything else needs the Mac for assets, import or shots.

## Order (part 3)

| # | Item | Value | Cloud-only | Model | Size | Builds on |
|---|---|---|---|---|---|---|
| 37 | [People on the Aton trails](#37-people-on-the-aton-trails) | very high | **yes** | Opus 5.5 | M | aton-coast, town_life |
| 38 | [The first walk to the town: signposts, a guide and the map's trails](#38-the-first-walk-to-the-town) | very high | no (map pictures) | Opus 5.5 | M | aton-coast, quests, ui_map |
| 39 | [What the hero can do on the way: the spring, the orchard, the boats, the boards](#39-things-to-do-on-the-way) | high | no | Opus 5.5 | M | aton-coast |
| 40 | [The Aton station as the club's front door](#40-the-aton-station-as-the-clubs-front-door) | high | no | Opus 5.5 | M | stations-v3, counters |
| 41 | [Station life: commuters, attendants, the cabins' words](#41-station-life) | high | **yes** | Fable 5.2 | L | transport, town_life |
| 42 | [A lighthouse at Mayak and its keeper](#42-a-lighthouse-at-mayak) | high | no (Blender + Mac) | Opus 5.5 | L | aton-coast, stations |
| 43 | [Birds and beasts of the brook](#43-birds-and-beasts-of-the-brook) | medium-high | **yes** | Opus 5.5 | S | aton-coast, bird life |
| 44 | [Signature behaviours of each bird species](#44-signature-behaviours-of-each-bird-species) | high | no (clips) | Fable 5.2 | L | bird life, bird book |
| 45 | [Bird voices that mean something](#45-bird-voices-that-mean-something) | high | **yes** (model) | Opus 5.5 | M | bird life, soundscape |
| 46 | [Animals with people: walked dogs, window cats, hedgehogs](#46-animals-with-people) | medium-high | no (hedgehog model) | Fable 5.2 | L | animal life, town_life |
| 47 | [Reading anywhere, and quotes in the diary](#47-reading-anywhere) | high | **yes** | Opus 5.5 | M | books, diary |
| 48 | [The library's book club and the librarian's advice](#48-the-book-club) | medium-high | **yes** | Opus 5.5 | M | books, barks, paper |
| 49 | [The sanatorium's procedures as small hands-on scenes](#49-the-sanatoriums-procedures) | high | no | Fable 5.2 | L | sanatorium-cure, guests |
| 50 | [Sanatorium guests come and go, and go out into the town](#50-sanatorium-guests-come-and-go) | medium-high | **yes** | Opus 5.5 | M | sanatorium guests, train |

**Running in parallel without touching the same files:** 37 ∥ 41 ∥ 47 ∥ 44 (four different subsystems); 38 and 39 both
edit `tools/south.py` places and words: one after the other, or 38 keeps to quests and the map and 39 to things; 43 waits
for 37 if both touch `tools/bird_habitats.py`/`people_map.py` regeneration (rerun after rebase); 45 after 44 or
rebased on it (both in the bird-life model); 46 owns `tools/animal_map.py` for the night; 49 and 50 both touch
`x_p_sanatorium.json`: 50 keeps to town-life (`tools/town/`) and the train. Everyone who changes town life runs
`tools/peoplesim/run.sh town`; everyone who changes the city content reruns `python3 tools/city/interiors.py --check`.

## 37. People on the Aton trails

`[~] aton-people 2026-11-21 (branch ready on aton-coast, docs/south.md "People")` **Cloud-only. Brief (Opus 5.5):** The Aton coast (`docs/south.md`, branch `aton-coast`) has five trails and twelve
places, and nobody walks them (`docs/south_backlog.md` "People do not walk it"). Give it people, the townsfolk we already
have, by persona, weekday and weather: a fisherman mending nets at the boat shed by the brook's mouth (`brook_mouth`)
mornings, going out in the evening; a beekeeper in the old orchard in warm months (the hives); the birdwatcher in the
hide (`bird_hide`) at dusk when the starlings come in (the ornithologist from `quest/ornithologist`, if his schedule
allows); couples and pensioners walking the shoulder trail (`town` trail) to the belvedere and back on fine afternoons,
sitting on its benches (the seats are already `ULyudPublicSpots` spots); children at the spring and the brook's stones
after school; a dog walker on the cliff walk; nobody in heavy rain, more people on festival days and Sundays. Add the
places and their slots the way `tools/town/sanatorium_guests.py` adds the guests (an idempotent post-step over
`townsfolk.json`/`town_places.json`/`activities`, because `town_life.py` does not rebuild on current inputs), with the
walking routes taken from `south.trails()` so they keep to the paths; `tools/people_map.py` must know the new ground.
A few lines for each role (ru/en/ja: the fisherman about the weather and the catch, the beekeeper about the bees,
walkers greeting). Done: `tools/peoplesim/run.sh town` with new checks (the places fill at the right hours, empty in
rain and at night, nobody leaves the trails), `--check` of the post-step, lines through the localization skill.

## 38. The first walk to the town

`[~] town-road 2026-11-21 (branch on aton-uses, docs/south.md "The first walk"; Mac: sign textures + south GLB, map build, shots)` **Brief (Opus 5.5):** Make the half-kilometre walk from the club to the town (`south.trails()` `town`) the game's
natural first trip. Signposts at the forks (the alley's end, the shoulder's foot, the footbridge) with arrows and
distances in metres and minutes ("Город 350 м", "Пляж", "Родник", "Станция «Атон»"), built from the North's kit sign
(`set: "north"`) with words from `tools/south_signs.py`. A small quest "Дорога в город" for the first day: Maru (or the
club's regular who tells the hero about the town) suggests the walk; the steps are the places on the way (the belvedere's
view of the whole town, the footbridge, the stele "Людмиловск", the first terrace), each step a line of the hero's own
thought, the end at the square with a first errand (bread at the bakery) so it leads on into the town. The map
(`tools/ui_map.py`, `tools/ui_map_ink.py`) draws the designed trails as dashed footpaths with their names, the places'
markers get small icons by kind (view, bridge, water, birds, boats), and the trail the hero walked is inked in. Done:
questsim walk-through, `south.py --check`, Mac: the map pictures rebuilt (LFS), shots of three signposts and the map.

## 39. Things to do on the way

`[~] aton-uses 2026-11-21 (branch on aton-people, docs/south.md "Things to do")` **Brief (Opus 5.5):** The Aton coast's places can only be sat at and looked through (`docs/south_backlog.md`
"Game"). Make each one do something, in the existing systems: drink at the spring with the hands (E: the hero kneels,
a sip sound, thirst/"refreshed" like the soda) and fill the empty bottle from the vending machines (a new item state
"spring water", sold to nobody, given to the sanatorium's doctor for a line); pick apples in the old orchard
in August-October (`ULyudInventory::Get(this)->Add`, a windfall under each tree that regrows by days, the plant system's
fruit if the orchard's trees use it, the allotment's kitchen recipes from item 34 take them); the info boards at the
belvedere, the hide and the stele open as a readable page (the town paper's reader layout, words in ru/en/ja: the town's
history, the birds of the reedbed, the brook); the boats at the mouth: a keel-up boat to sit on and watch the sea; the
stepping stones over the brook's mouth with a misstep into the water (wet feet: the swim system's wet state). Each new
item with an icon (one Higgsfield grid, already-allowed budget). Done: citysim-style offline checks where a model exists,
Mac shots of each interaction, bench.

## 40. The Aton station as the club's front door

`[x]` aton-front-door 2026-11-21 (docs/front_door.md; Mac compile, shots and bench owed) **Brief (Opus 5.5):** The Aton station is the first hall the hero sees and stands next to the club
(`docs/transport.md`: the swept wing, its coffee bar with `coffee` goods; `stations-v3` dresses the halls). Make it the
club's front door: the coffee bar served by its barista through item 27's counter service (a line in the morning, the
cup set on the counter, the regulars' "the usual"), a departures board that shows the real next cabins of the Sun line
(from `ULyudTransport`'s schedule, flipping like a split-flap with a clack), a ticket punch and a travel card with a
stamp per station (a collection "Все станции" in `collections.json`, paid off by item 30's rules: a badge in the diary
when all seven are stamped), a lost-and-found shelf where residents' lost things turn up and can be handed back to their
owners (a line and a small gift each), the station's lights coming on at dusk with the club's sign. Coordinate with the
stations thread (it owns the halls' meshes): add only things and content. Done: transportsim checks for the board's
times, citysim for the counter, Mac shots by day and night, bench.

## 41. Station life

`[ ]` **Cloud-only. Brief (Fable 5.2):** The cable car carries riders (`docs/transport.md`, `tools/transportsim`) but the
stations do not live by the town's day. Give them a rhythm from town life: the morning commute (Academgorodok's
scientists from the town to Akademgorodok station 7:30-9:00, back 17:30-19:00), schoolchildren to the school's stop,
pensioners going to the market on Saturday morning, beach-goers to Yantarny bereg on warm days, a crowd after the
cinema's last show and after festivals; each station's attendant on a shift (opens the hall, sweeps, closes at night,
talks to regulars), the barista and the kiosk keepers from `stations-v3`'s halls given real town schedules. The cabins'
words: an announcement of each next station in ru/en/ja (text first; the voice is item 36's job), one line about the view
("Слева — Людмиловская бухта"), the hall's barks of people waiting ("Опять ветер, кабинки встанут?" when the wind stops the
line). Riders pick the line from where they live and work (`townsfolk.json` home/work), so the same faces ride at the same
hours. Done: transportsim + peoplesim town checks (peak loads, no one stuck in a hall at closing, the wind stop handled),
lines through the localization skill, the hall staff in town data.

## 42. A lighthouse at Mayak

`[ ]` **Brief (Opus 5.5):** The south end of the Aton coast is the Mayak station on the cliff (`south.places()`
`mayak_station`, `cliff_lookout`) and there is no lighthouse ("Маяк") there (`docs/south_backlog.md` "Looks"). Build one
in Blender with the North's kit and atlas (`blender/south/`, a white tower with a red band, a gallery, the lantern room,
a keeper's cottage, a fence), place it on the headland beyond the station, and light it: a rotating lens like the
station's own (`docs/transport.md` "Mayak (lantern)") with a sweeping beam that the town and the beach see at night (one
light, out of Lumen, no per-frame mesh changes; the beam a cheap card cone), dimmed in fog with a foghorn every 30 s in
fog (the soundscape's weather mood). The keeper, a new resident with a schedule (lights the lamp at dusk, polishes the
lens, keeps a weather log he lets the hero read; a quest to bring him paraffin and the paper), the gallery as a viewpoint
(E: look; the telescope item 5's eyepiece at the coin viewer there). Check UVs and artefacts on every new mesh. Done:
Blender lint 0/0, Mac import through `import_south.sh`, night shots from the town and the beach, bench (the beam's cost).

## 43. Birds and beasts of the brook

`[ ]` **Cloud-only. Brief (Opus 5.5):** The new brook (`south.brook_cut`) and its mouth have no habitat on purpose
(`docs/south_backlog.md` "Animals"). Add a `brook` habitat kind in `tools/bird_habitats.py` (the valley bed and the
ravine's stones) and a `mouth` kind (the shallow fan over the beach), and let existing species use them by taste in
`species.json`: the grey heron standing at the mouth at low tide and in the brook's slow bends at dawn, white wagtails on
the stones, gulls bathing in the fresh water, swallows and sand martins drinking on the wing over the pools, the cormorant
drying on the boat shed's roof, rooks and jackdaws drinking on hot days. The kingfisher and dipper are new species and
need models: list them in the bird models thread's queue, do not add them here. Bees at the orchard's hives as a small
ambient sound point; frogs croaking in the reedbed's runnels on warm spring nights (the soundscape's existing points).
Done: `tools/birdsim/run.sh` checks (the heron at the mouth at low tide, no bird standing in deep water, roost counts
unchanged), habitats regenerated after rebase, the sound points as data.

## 44. Signature behaviours of each bird species

`[~] bird flight thread 2026-11-21: model, 63 habits, tests, book lines, stamps in (docs/bird_habits.md); own clips, sketches, page shot, bench on the Mac owed` **Brief (Fable 5.2):** Birds feed, rest, bathe and roost by species (`docs/bird_life.md` "Daily routines"), but they
do it alike. Give each of the 39 species one or two things it alone does, that a patient hero with binoculars sees and
writes in the bird book (`ULyudBirdBook`, `docs/bird_guide.md`): hooded crows dropping a walnut or a shell on the
promenade's paving from 5 m and walking down to eat it; herring gulls snatching food from a resident's bench and from the
fishermen; jackdaws in pairs preening each other on a roof; rooks' noisy gathering on the field; wagtails' dashing runs
after flies on the wet sand; swallows dipping to drink from the brook; blackbirds tossing leaves aside; great tits hanging
upside down at the feeders; starlings mimicking (a car alarm, a whistle) on the wires; the heron's slow stalk and strike;
the cormorant's spread wings; the sparrowhawk's dash along a hedge that scatters sparrows; magpies hiding a shiny thing.
Each is a short act in the bird-life model with its clip from the animations thread's rig (coordinate: it owns the clips;
reuse what exists first), seen at most a few times a day near the hero. The bird book gets a page "Повадки" per species:
a sketch, a line, the date first seen; finding all of one family's gives a stamp. Done: birdsim checks (rates, never in
front of a wall, no per-frame cost when far), the book page shot on the Mac, bench.

## 45. Bird voices that mean something

`[~] bird flight thread 2026-11-21 (branch bird-voices, docs/bird_voices.md; the model, the guide's line and the dev action are done, awaits a Mac build; new synth calls not done)` **Cloud-only (the model; new recordings or synth renders go through the Mac). Brief (Opus 5.5):** Bird calls come
from `tools/birdsim/bird_calls.py` and play by species. Make them follow what the bird is doing: songs only from
singing perches in the breeding months and the dawn chorus (species by species in their real order from an hour before
sunrise: robin, blackbird, song thrush, then tits and finches), quiet in midday heat and in rain; contact calls in
flocks (the more birds, the denser the murmur); alarm calls that spread: a tit that sees a cat or the sparrowhawk scolds,
its neighbours take it up, and the hero can find the cat by the sound (item 1's cats, `docs/animals.md` "Birds"); begging
young in late spring at the nests (`docs/bird_nests.md`); the starlings' chatter at the roost; gulls' long calls at the
beach in the morning. A per-bird "voice state" in the model, a voice budget near the camera (at most N voices, nearest and
loudest first, nothing per frame when far), and the bird book's "Голос" line (what the call means). Done: birdsim checks
(dawn order by sunrise over a year, alarm spread within a few seconds and not across the town, voice budget held),
the soundscape mix unchanged at night.

## 46. Animals with people

`[x] animals-with-people 2026-11-21 (model, data, lines, hedgehog model and offline checks done; engine side never compiled or seen: Mac build, hedgehog.py + import_props.sh hedgehog hedgehog_ball, shots, bench owed)` **Brief (Fable 5.2):** Cats and dogs live on their own (`docs/animals.md`) and residents never own one. Tie them:
some households have a dog that a resident walks on a leash morning and evening along their usual paths (the town-life
walk activity carries an animal id; the dog follows at the owner's heel, sniffs, greets other dogs; the leash a simple
cable between hand and collar, out of Lumen), cats that belong to a flat and sit on its window sill by day (the flats'
windows from `ULyudCityWindows`: a sill spot per lit flat) or wait at its door at feeding time, the lady who feeds the
yard cats walking out with her bowls at 8:00 and 18:30 (she exists only as a time in the model today), and hedgehogs at
dusk in the yards and the old orchard from April to October (a new small animal: Blender model with UV check, a shuffle
and a curl-up when the hero comes close; cats sniff them and back off). Residents talk to their pets and about them
(lines ru/en/ja), the hero can ask "можно погладить?" (item 1's stroke). Done: offline checks in the animal-life pure
model (owner and dog keep together, leash never stretched through a wall), peoplesim town, Mac shots, bench.

## 47. Reading anywhere

`[~] book-club 2026-11-21 (branch book-club: written and tested offline, Mac compile pending)` **Cloud-only. Brief (Opus 5.5):** The classics (`docs/books.md`, `SLyudBookReader`, `ULyudBenchSit`) can be read
only on a park bench. Let a book on loan be read wherever the hero sits or rides: any public seat (`ULyudPublicSpots`
"bench" and "chair" spots, the sanatorium lounge, the café tables), the beach towel, the cable car cabin and the train's
window seat (the clock moves as he reads, so a ride passes in pages), the hero's own room (item 19). Light matters: after
dusk outdoors the page dims and a line says it is too dark, unless the headlamp (L) is on or a street lamp is near; rain
wets the page (he refuses outdoors in rain). Quotes: select a sentence in the reader (click and drag, or a key) and it is
copied into a diary page "Цитаты" with the book and the date (ru/en/ja UI); the diary's day entry (item 23) mentions what
he read. Reading stats in the diary (books finished, pages, the longest). The librarian notices overdue books (a week)
and the hero gets a polite postcard (item 11's letters). Done: unit tests of the light rule and the quote store
(engine-free like `BookLayout.h`), questsim/citysim where they touch, strings through the localization skill.

## 48. The book club

`[~] book-club 2026-11-21 (branch book-club: written and tested offline, Mac compile pending)` **Cloud-only. Brief (Opus 5.5):** Give the library a social side. A reading circle meets on Thursday evenings in
the reading room (a town-life gathering of four or five residents by persona: the retired teacher, the student, the
librarian, a scientist from Academgorodok), each month one book from the catalogue (`tools/books/catalog.json`, in all
three languages); they sit, talk in turns (bark lines about the month's book, written for each of twelve books, ru/en/ja,
never spoiling past the first chapters), and if the hero has read that book (`read_book` notifications, the bookmark at
the end) he may join with his own lines and gets a stamp. The librarian recommends a next book from what he has read
(a small table of "if you liked X" pairs across languages), the town paper (item 6) prints the club's "книга месяца",
friends (item 12) borrow the book he recommends and later tell him what they thought. Done: peoplesim town (the club
fills on Thursdays, not on festival days), barksim for the lines, questsim for the stamp.

## 49. The sanatorium's procedures

`[~] san-procedures 2026-11-21 (branch san-procedures, docs/sanatorium_procedures.md; scenes, wing, guests and citysim done; Mac compile, shots of three procedures and bench owed)` **Brief (Fable 5.2):** The course at «Корона» (`sanatorium-cure`, `docs/city_interiors.md` "Stays") stamps the
hero's book, but the procedures happen as a word and a fade. Make each a short hands-on scene in the room where it
happens: the pump room (pick a cup, fill it at the brass tap, sip it slowly while the clock ticks; the water's taste line
changes by spring), the inhalation room (sit, a mask, breathe in time with a slow ring on the screen), the mud bath and
the Charcot shower (a nurse, a curtain, a cut to the robe and slippers), the massage (lie down, a few lines from the
masseur about the town's news from item 22), the doctor's check (weight on old scales, pressure with a cuff, the card
filled in by hand: real numbers that improve over the course), the evening dance in the hall on holidays and the film
night in the lounge (the cinema's films, item 13). The guests (`sanatorium-guests`) are at the same procedures at the
same hours and talk to him there. Done: citysim CourseChecks extended, any new prop through Blender lint 0/0, Mac shots
of three procedures, bench.

## 50. Sanatorium guests come and go

`[~] sanatorium-rotation 2026-11-21` **Cloud-only. Brief (Opus 5.5):** The four sanatorium guests (`tools/town/sanatorium_guests.py`) live there for
ever. Make the sanatorium a place people come to: a nine-day stay (the same as the hero's course) with arrivals and
departures on Saturdays by the train (item 21's passengers: they get off with a suitcase and walk to «Корона», the
departing ones wait on the platform), a pool of a dozen guests from the residents generator's spare bodies who rotate,
each with a home town and one story told in pieces over the days at the dining table and in the lounge (lines ru/en/ja),
and their outings by the day's regime: the promenade and the pier after lunch, the Aton coast's belvedere and the
spring (item 37's places), the amber shop and the kiosk for postcards, letters home from the post office (item 11), a
goodbye line to the hero on their last day if they knew him (friends' bond, item 12). Done: peoplesim town over three
weeks (rotations on time, beds never double-booked, the meals' chairs held), `sanatorium_guests.py --check`, lines through
the localization skill.
