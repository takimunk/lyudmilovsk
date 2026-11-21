# The town paper and the dated notices

«Людмиловская правда» (en *Lyudmilovsk Pravda*, ja 『リュドミロフスク・プラウダ』) comes out every game day and is set from the
game's own news. The notice boards carry dated notices that go up and come down by the calendar. Interactives backlog item 6
(`docs/interactives_backlog.md`), 2026-11-20.

## Where the hero meets it

| Where | What |
|---|---|
| Every kiosk, the fair's press stall, bus-stop kiosks (`take_daily_paper`, `buy_kiosk_news`) | the day's issue in the reading view (text `paper_today`, `"gen": "town_paper"`) |
| The library's periodicals rack A (`P-LIBRARY/periodicals_rack_a`, act `library_paper_archive`) | the archive: the kept issues, newest first, each with its read mark (text `paper_archive`) |
| Every notice board of the `lost`, `club`, `health` and `wallpaper`/`school` topics, the school's gazette | the board's notices for today (texts `notices_<board>`) |

Boards whose act has building lines of its own (`look_club_cog`, `look_hall_notice`, `look_lost_club`, `look_rassvet`,
`look_lost_store`, the Cog club's five boards, `t3_club_board`) keep those lines (`"read": null`).

## An issue

Four pages, keys and arguments only (so a save shows it in the language of the moment):

1. **Front**: the masthead (number = 12001 + days since the game began, weekday, date) and one front story, chosen in this
   order: the hero tamed a mute swan or landed a record fish yesterday; a festival today (its own story when
   `tools/paper/strings_src.py` FEST has one, else a generic one; a long festival's later days say it goes on); any other
   deed of yesterday (a bird tamed, a rare bird first seen, a quest finished, a fish of 1 kg or more); a festival in 1–3 days;
   tomorrow's weather if it is news (first snow, thunder, a gale of 15 m/s+, heat 27 °C+, frost −12 °C or colder, fog); else
   a story of the season. Every kind has two or three wordings; **the front is never the same two days running** (the
   previous issue's front signature is passed in and skipped).
2. **Town chronicle**: the deeds that missed the front (at most three briefs), up to two residents' birthdays (round ones and
   children first) and one wedding anniversary (silver at 25, golden at 50) from `townsfolk.json`, three fair prices of the
   season from `fair_goods.json`, a town story (never yesterday's).
3. **Weather and sea for tomorrow**: the sky and temperatures (six looks at `ULyudWeatherSubsystem::GetStateAt`), the wind, the
   sea's temperature (with "good for a swim" from 18 °C), the northern lights if likely tonight, tomorrow's high and low waters
   (`LyudTide::At` every 6 minutes at the sky's longitude), the fish biting best (`ULyudEcosystem::GetAbundance`, 15 % over the
   usual; only for today's issue).
   Yesterday's club tournaments come first on this page: the champion and the runner-up, or "a young entrant" won, and how
   far the hero got (docs/city_interiors.md "Tournaments"; the hero's own cup is a front story, `paper.front.cup.*`, among
   his big deeds).
4. **Holidays and cinema**: today's club tournament (its start and venue: sign up at the bracket), up to three festivals of the next two weeks with their dates (`festivals.json` through
   `LyudFest::FCalendar`, weekly and trigger ones left out), the Rassvet's poster of the weekday (`city.rassvet.poster.<day>`, the
   festival one on a holiday), three small ads of the season. On the 1st of a month also the photo club's contest
   (`paper.contest.open`: the month's theme, prints until the 25th, the jury on the 26th; see "The photo contest").

The hero's deeds are read from what the game already keeps with dates: `ULyudBirdBook` (`Friends[].Tamed`, `Seen[].When`; rare
species in `town_paper.json` `rare_birds`), `ULyudFishingState::Journal` (a record when it equals the species' biggest and the
hero has caught three or more of it), `ULyudQuests::Entries` (`quest.<id>.done`), the tournaments
(`ULyudCityThings::GetCupNews`, worked out again from the hero's saved results) and the photo contest
(`ULyudPhoto::ContestNews`, on the 27th). No other system reports to the paper.

## The photo contest

The photo circle at «Шестерня» runs a contest by the month (docs/photo.md; the entries, the jury and the wall are the
camera's code). The theme goes by the calendar month, `(month - 1) % 4`: `sea`, `birds`, `work`, `festival`
(`LyudPaper::FPaper::ContestTheme`; names `paper.contest.theme.<id>`). The paper's part:

- **The 1st**: page 4 opens the contest (`paper.contest.open`: theme, the last day 25, the jury's day 26, the month).
- **The 27th** (the jury sat on the 26th): `ULyudTownPaper::BuildFacts` asks `ULyudPhoto::ContestNews(World, Year, Month,
  Place, ShotPlace)` for yesterday's month when yesterday was the 26th; true gives the deed `contest` (`N` = the hero's
  place 1..6, `What` = the map place the print was taken at, or empty). Place 1 is a front story among the hero's big deeds,
  after the swan, a record fish and a cup ("the paper prints the photograph": `paper.front.contest.1..3` with the theme and
  `place.<id>.name`, `paper.front.contest_noplace.1..2` when the place is not known); if a bigger story keeps the front it
  is a brief (`paper.brief.contest_1`). Places 2 and 3 are a brief on the town page (`paper.brief.contest_2`, `_3`, with
  the theme). Places 4 to 6 and a month the hero sat out: nothing. The archive keeps lines, not deeds, so old saves read
  as before.
- **The boards**: dated notices on every club board in town (`notices_club`) and on the circle's own sheet (text
  `notices_photo`: the notice of the day, then the rules; read at `P-CLUB3/photo_board` in turn with the lab's guide):
  the month's theme from the 1st to the 19th (`city.note.club.contest_sea|birds|work|festival`), a reminder from the 20th
  to the 25th (`contest_soon`), the jury's day on the 26th (`contest_jury`), the results to the month's end
  (`contest_done`). A notice's window in `strings_src.py` may be a list of windows (the notice comes back).

## Code and data

| File | What |
|---|---|
| `Private/City/TownPaperModel.h` | `LyudPaper::FPaper`: plain C++, the rules (`Build(FFacts)` → `FIssue`), Pack/Unpack for the save |
| `Public/City/LyudTownPaper.h`, `Private/City/LyudTownPaper.cpp` | `ULyudTownPaper` (world subsystem, saved as "TownPaper"): gathers the facts, keeps the last 40 issues and the read days |
| `Private/City/LyudCityThings.cpp` | `OpenBook`: a text with `gen` shows `ULyudTownPaper::Pages`; reaching the last page calls `FinishedReading` |
| `tools/paper/strings_src.py` | every word (ru/en/ja) and the notices |
| `tools/paper/paper.py` | writes `Content/City/town_paper.json`, `Content/Localization/paper/strings_<lang>.json`, `tools/city/content/x_notices.json` (+ its strings) |
| `tools/papersim/run.sh` | the offline checks (below); `run.sh show 30 en` prints 30 days of issues |

An issue is set the first time it is asked for, together with the missing days before it (up to 11), so the archive has no
holes and the no-repeat rule holds across them. Reading today's issue to its last page marks it read (HUD `paper.read`, quest
event `read paper_issue`). A new game forgets every issue.

Console: `lyud.Paper.Print [days]` (today's issue, or N days ahead, to the log), `lyud.Paper.Reset`. Developer mode
(Interiors group): `paper.print` (7 days to the log), `paper.reset`. `lyud.City.Read paper_today` / `paper_archive` opens them.

## Notices

`tools/paper/strings_src.py` NOTICES: (board, id, yearly window `("MM-DD", "MM-DD")` (or a list of such windows) or first
game days `(after, before)`, ru, en, ja). `paper.py` makes a text per board: a holiday page (`festival: any`), pages of dated notices (packed so that two
notices on one page never share a day: a page shows one notice or none), last the board's own five lines (one a day). The
reading view leaves out pages with nothing holding, so a board shows today's notices and the board's line. 44 notices now
(lost cat Ryzhik and its finding, keys, flat swap, the choir, the chess tournament, kites, dancing, the Perseids, flu jabs,
ticks, black ice, nest boxes, exams, the first of September, the herbarium contest, the summer camp, the photo contest's seven...).

## Checks

`tools/papersim/run.sh` (exit 1 on a failure):

- 800 made-up days (season temperatures, skies, tides, deeds by a seeded die, the real festival calendar): four pages a day,
  nothing empty, no front or town story two days running, every key and `@` argument in ru/en/ja, every placeholder has an
  argument, Pack/Unpack round trip, festivals and the hero's swan and records reach the front, birthdays sane; the photo
  contest: it opens on every 1st with the month's theme and on no other day, a made-up deed of each place on the 27th
  (the first place on the front in every month with and without the shot's place, its five wordings, the next wording
  when yesterday's front was the same, a brief when a swan keeps the front; the second and third a brief; the fourth to
  sixth nothing), an issue packed by hand in the old save's form unpacks;
- `check_notices.py`: two years of days, every board shows something, no two dated notices on one page on any day, every notice
  shows on some day, no board without a dated notice for longer than 40–70 days, the photo contest's notice of the day
  on `notices_club` and `notices_photo` (the theme by the month, the reminder, the jury's day, the results);
- `paper.py --check`: the generated files are current.

Also `python3 tools/city/interiors.py --check` and `tools/citysim/run.sh` (the boards' texts, the `gen` field).

## On the Mac

Nothing to import: the paper is text. Build, then: `lyud.Paper.Print 3`; buy a paper at a kiosk and read it (E/D through four
pages; the last page toasts "Read an issue … 1 of N"); `Dev.Do time.plus_day`, read again (a different front); the library
rack shows both issues with their marks; `lyud.Language en` / `ja` and look for raw keys.

## «Говорят в городе»

The morning paper prints up to two lines about the day before's talk of the town (ULyudTownRumours::PaperColumn, FFacts.Talk), before the birthdays. Words: tools/rumours/rumours.py (PAPER), docs/town_rumours.md.
