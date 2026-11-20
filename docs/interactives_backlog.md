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

`[ ]` **Brief (Opus 5.5):** On rain days the cafés, the library, the clubs and the sanatorium lounge fill up; on stormy
nights the lighthouse keeper's radio crackles with ship talk; on hot days the ice cream counter has a queue and the café's
windows open. Lines exist by weather; extend to who is where (town-life weights by weather per plot kind), room tones
(`tones`, `tone_by`: rain on the roof, the wind in the lighthouse), lamps on earlier on dark days. Done: peoplesim test of
indoor counts by weather, citysim, a Mac walk on a rain day.

## 29. Night state geometry and the cleaner

`[ ]` **Brief (Opus 5.5):** Backlog P5 "night state of public rooms" has the data but no geometry: chairs on tables in
closed cafés and clubs, a single lamp left on, a cleaner at work 7-10 with a mop and a bucket (a town-life role `cleaner`,
a manner prop), the watchman in the department store at night with a torch. Use swappable pieces like item 25 (coordinate
with it: same `B.dress` mechanism, one of you writes it). Done: Blender previews, Mac shots at 23:00 and 07:30, bench.

## 30. Collections pay off

`[ ]` **Brief (Opus 5.5):** Stamp sets exist (library shelves, films, jukebox records, kiosks, chess wins, the sanatorium
card, tonight's new sets) but a complete set gives nothing (backlog P4). Give each set a reward that fits: a library reader's
card with a photo, a framed film poster for the hero's room (item 19), a record of your choice, the kiosk keepers' free
paper for a month, a chess club badge on the jacket, the sanatorium diploma. A diary page "Коллекции" with all sets, filled
and empty slots (the page `lyud.City.Collections` exists: move it into the diary). Done: citysim test of each reward,
quest audit clean.

## 31. Small touch details

`[ ]` **Brief (Opus 5.5):** A pass over houses and lobbies (P2 of `docs/city_backlog.md`) for small things that answer a
touch: mailboxes (the hero's own one has letters: item 11), light switches and wall clocks that no building declares yet
(`what=switch`, `look_clock` exist), a radio to turn on, a tap to run, a window to open (the room's tone changes), a cat on
the radiator to stroke (item 1), a lift button (the lifts' `to`/`floors` exist but no building declares a link: declare
them for the unite block, the stacked beam tower and the capsule towers). Done: reach audit and quest audit, citysim, Mac
walk tests in two buildings with lifts.

## 32. Children's games in the yards

`[ ]` **Brief (Fable 5.2):** Children of the town play together after school: hopscotch on chalked squares, a skipping
rope, hide-and-seek in the yards (one counts at a wall, others hide behind the town's own cover; the hero can join and be
found or find), football with a ball that rolls (physics, a code-built ball), tag. A small group behaviour in the people
sim (a game leader, roles, a play area from the city plan's yard spots), gestures by the hand-goal IK layer. Done: peoplesim
test of a game's start, roles and end, Mac shots, bench with three groups playing.

## 33. Photo quests and the photo contest

`[ ]` **Brief (Opus 5.5):** On top of the camera (item 2): the photo club at «Шестерня» runs a monthly contest with a theme
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

`[ ]` **Brief (Opus 5.5):** Two open items of P3: the rental unit only talks (rent deck chairs and parasols for the beach,
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
