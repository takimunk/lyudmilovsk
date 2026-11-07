# What the townsfolk say

The townsfolk (docs/town_life.md) can't be talked to with E: their bodies are tagged `NoTalk`, and conversations belong to
the story's people (npc_dialogue.txt). But they talk on their own: a short line over the head of somebody near the
player, in the game's language, picked by who they are and what they do.

- **Greetings.** Somebody the player walks past (within 3.5 m, in view) greets him, once a day each and not everybody
  (about two in three): by the hour, their age (kids, teens, grannies), their job (the seller, the postwoman) and the
  holiday on now.
- **Lines on their own.** Every ~9 s somebody in view within 14 m says something by their job (seller, waiter off
  shift, fisherman, lifeguard, programmer, ...), their age, what they are doing and where (the benches, the computer
  club, the beach, the market, the fair), the hour, the month, the sky and the weather, their needs (hungry, tired) and
  the holiday.
- **Exchanges.** Two of them talking (the life's chat pairs) or seated side by side (a bench, a table, the club's row of
  computers) now and then have a short exchange of two to five lines: grannies on a bench, gamers in the club, parents
  at a holiday, two sellers at the market.
- **The restaurant.** The waiter and the cook speak at the moments of the player's visit: he sits down (the waiter
  welcomes him), an order is taken from the pass (the cook calls it, the waiter answers), a dish is served, the bill is
  brought, he leaves after eating.

207 sets, 594 lines in ru/en/ja (405 said alone, 54 exchanges). They are proposals until Dima approves them. They
aren't voiced yet.

## Files

| What | Where |
|---|---|
| Authoring sources: every line as `key \| ru \| en \| ja`, one file per theme | `tools/barks/lines/*.txt` |
| Builder (checks the conditions against activities, places, jobs, festivals.json) | `tools/barks/build_barks.py` |
| When each line is said (built: structure, keys only) | `Content/Dialogue/town_barks.txt` |
| The words (built, merged by `LyudText`) | `Content/Localization/barks/strings_<lang>.json` |
| Picking a line, plain C++ (no Unreal types) | `Private/People/TownBarks.{h,cpp}` |
| The lines over the heads in the game | `ULyudTownTalkSubsystem` (`Public/People/TownTalk.h`, `Private/People/TownTalk.cpp`) |
| The mouth moving while a line is up | `ANpcBody::Chatter` (`People/PeopleLife.h`) |
| Test outside the engine | `tools/barksim/run.sh` |

Edit the sources, never the built files, then:

```
python3 tools/barks/build_barks.py     # rebuilds both; PROBLEM lines name a condition value the game doesn't have
tools/barksim/run.sh                   # a simulated day of the town: who says what, and the coverage checks
tools/barksim/run.sh day en            # a day's samples printed in a language (ru, en, ja)
python3 tools/check_localization.py
```

Keys are `line.town.<scene>.<n>` (in the sources without `line.`). A Russian line whose form depends on the speaker's sex
(the past tense) is two keys in two sets with `sex = female` / `sex = male`, never both forms in one string.

## The sets

`town_barks.txt` is a list of `[sets]`: conditions, then lines. Its header documents every condition; in short:

- **who:** `job` (occupation or a duty's role), `age` (kid, teen, adult, elder, or years `50-120`), `sex`, `doing`
  (activities.json ids; `work` on a duty at its place, `free` off duty), `at` (place kinds or ids; `outdoors`,
  `indoors`), `pose`, `gather` (a holiday gathering's mode), `chrono`, `need` (over 0.75), `near = hero` (within 10 m);
- **when:** `hours` (`17-22`, over midnight `22-4`), `months` (`10-4`), `weekday`, `sun` (day, low, night, cloudy),
  `weather` (rain, snow, fog, cold, warm, clear, wet), `festival` (festivals.json ids, or `any`), `phase`;
- **how:** `say` (alone), `to = hero` (a greeting), `talk = a | b | a` (an exchange; `job2`, `age2`, `sex2`, `doing2`
  are conditions on the second speaker), `on = rest_sit | rest_carry | rest_serve | rest_bill | rest_bye` (a moment);
  `weight` (1).

Every condition given must hold; the values of one condition are alternatives, `!x` means not x. Of the sets that fit,
one is picked at random by `weight × (1 + 0.5 × conditions)`, so a particular set (a granny on a bench in the rain) wins
over a general one (anybody outdoors). A line said among the last 64 isn't said again while another one fits (a
greeting or a moment's line would rather repeat than say nothing). An exchange may start with either of the two.

## In the game

- Only the townsfolk speak (`lyud.Barks.Residents 1`: the story's people too), and only those up and about where they
  can be seen: not asleep, away, riding, inside a building's shell or between activities, and drawn near the camera
  (`lyud.People.TownBodies`).
- At most four lines on screen and one exchange at a time; a line on one's own every `lyud.Barks.Every` (9) s on
  average; nobody says one on their own twice within 90 s.
- A line stays up 2.5 to 7 s by its length (Japanese counts double), shows the speaker's name, fades with the distance
  (whole within 9 m, gone at 18 m), hides behind walls (a trace ten times a second per line) and goes when the speaker's
  body goes back to the pool. The speaker's mouth moves while it is up.
- Lines wrap by words, Japanese by characters without starting a row with a closing mark; four rows at most.
- The scan runs twice a second over the life's own data (no allocations once warm); only the chosen speaker's actor is
  looked up.
- The file is read at every level start (`Content/Dialogue` is staged as a plain file).

Console: `lyud.Barks 0` (off), `lyud.Barks.Residents`, `lyud.Barks.Every`, `lyud.Barks.Log 1` (log every line as
`BARKTEST ...` for offscreen tests), `lyud.Barks.Report` (what is up, the moment, how often each set was said),
`lyud.Barks.Say <key | rest_event>` (the townsperson nearest the player says it), `lyud.Barks.Reload`.

## Other threads' code, read by name

The subsystem doesn't link to the festivals' or the restaurant's code (neither was in main when it was written). It
finds them by name and simply has no such lines when they aren't in the game:

- **Festivals:** `ULyudFestivalSubsystem`'s UFUNCTIONs `ActiveFestival()` and `ActivePhase()` (an FName each, no
  parameters), looked up every 2 s.
- **Restaurant:** `ALyudRestaurant`'s UPROPERTYs `WaiterBody`, `CookBody` (ANpcBody), `BillFolder` (an actor),
  `HeroDishes` and `OnTray` (arrays of dishes), read ten times a second while the player is within 30 m of it. A dish
  coming onto the hero's table is `rest_serve`; the tray filling at the pass (while the table's dishes didn't go) is
  `rest_carry`; the folder within 1.8 m of the seated hero is `rest_bill`; sitting down within 14 m of the restaurant's
  middle is `rest_sit`; standing up after a dish was served is `rest_bye`.

  Renaming any of these properties silently stops those lines. Keep the names, or call
  `ULyudTownTalkSubsystem::Get(this)->SayEvent(Speaker, TEXT("rest_serve"), PersonId, TEXT("waiter"))` at the moment
  instead: the first such call turns the reading off.

## Voice

Not voiced. The keys are `line.*`, so `tools/gen_voice.py` can voice them like the story's lines once the townsfolk
have voices (localization/voice.json). All 594 lines come to about 51,000 characters in the three languages (ru 18.7k,
en 22.4k, ja 9.5k). That is paid generation: ask Dima first.

## Tests

`tools/barksim/run.sh` builds `TownBarks.cpp` with the people's sim and checks: the file reads clean; every key has its
words in ru, en and ja, and no table holds words for unused keys or keys the hand-written tables already have; lines fit
a bubble (ru/en 100 characters, ja 48). Then it runs the town's day from Wednesday 04:00 with gatherings on the square,
the promenade and the fair, a different sky every couple of hours and a festival evening, sampling everyone up and
about every 120 s: at least 75% have a line (95% now), at least 90% greet (100%), at least 30% of the pairs side by side
find an exchange (40%), the restaurant's staff have a line at every moment, and at most 10% of the sets fit nobody.

## Known gaps

- **Not compiled in Unreal yet:** written while the Mac's builds were blocked. `TownBarks.cpp` builds with clang and
  `-Wall -Wextra -Wshadow`; `TownTalk.cpp` passes a syntax check against stand-in engine headers. Build it before merging.
- Five sets fit nobody on the test's Wednesday: the fair's vendor and carousel staff work Friday to Sunday,
  `gamer_morning` (nobody plays at the club between 5 and 9 that day) and `waiter_sit_festival` (no holiday that day).
- Exchanges are between two people; a group at a gathering doesn't talk among itself.
- The restaurant's moments read its properties until it calls `SayEvent` itself.
