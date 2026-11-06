# The town's holidays

Людмиловск keeps a calendar of its own. On a festival's day the town is dressed (bunting, lanterns, flags, a stage, fair
stalls, a New Year tree, wreaths, candles, birdhouses), the program runs its shows (fireworks over the sea, sky lanterns,
a bonfire, music and stage lights), the residents go there by their own schedules and tempers, and on public holidays the
offices and schools are closed. Every festival, date and program here is a **proposal (not canon)**; change them freely in
the data.

## Files

| What | Where |
|---|---|
| The festivals (dates, programs, decorations) | `Content/PeopleData/festivals.json` |
| The calendar, plain C++ (no Unreal types) | `Source/Lyudmilovsk/Private/Festival/FestivalCalendar.h/.cpp` |
| Game side: subsystem, decorations, fireworks, sky lanterns, bonfire | `Public/Festival/LyudFestival.h`, `Private/Festival/LyudFestival.cpp` |
| Run the calendar outside the engine, tests | `tools/festivalsim/run.sh test` / `year 2027` / `at 2026-10-24 21.7` |
| Words (names, the day's note, "soon" notes) | `festival.<id>.name`, `festival.<id>.today`, `festival.soon.<show>` in `strings_<lang>.json` |

## The calendar (proposal)

| Id | Name | When | Program |
|---|---|---|---|
| weekend_market | Субботняя ярмарка | every Saturday | market 8–14 at the fair |
| sea_festival | Праздник моря (the train's poster: "fireworks over the pier on Saturday") | 4th Saturday of October (2026-10-24, the game's first week); a day off | fair 10–17, concert 17–21:30 on the square, fireworks 21:30 from the pier, dancing till 00:30 |
| lantern_night | Ночь фонариков | 2nd Saturday of November | a walk on the promenade 16:30, sky lanterns from the beach 19–20:30 |
| first_snow | Праздник первого снега | the day after the first snow lies white (once a winter) | the park 12–16, hot tea stalls |
| winter_fair | Зимняя ярмарка | 18 December – 7 January | the fair 12–20 every day, the tree on the square |
| new_year | Новый год | 31 December, days off 31 Dec – 2 Jan | the tree 22:00, fireworks at midnight, dancing till 3, a walk in the park on the 1st |
| maslenitsa | Масленица | Orthodox Easter − 49 days (Sunday) | pancake fair 11–16, the straw doll burns on the beach 16–18 |
| bird_day | День птиц | 1 April | birdhouses in the park, 8–12 |
| blossom | Праздник цветения | 2nd Sunday of May | picnics in the park 11–17 |
| midsummer | Ночь середины лета | 23 June | bonfire on the beach 20–02, then everybody watches the sunrise (the sun motif, docs/story/motifs.md) |
| fisherman_day | День рыбака | 2nd Sunday of July | fish fair on the promenade, concert on the square |
| perseids | Ночь Персеид | 12 August | on the beach watching the shooting stars 22:30–02 |
| harvest_fair | Ярмарка урожая | 4th Saturday of September | fair 9–17 |

`festivals.json`: `when.rule` is `date` (month, day), `nth_weekday` (month, nth 1..5 or −1 last, weekday 0 Monday),
`easter_orthodox` / `easter_western` (offset days), `weekly` (weekdays, months) or `trigger` (the game fires it); `offset`
shifts any rule's day. `days` is its length; `holiday` closes offices and schools (`holiday_days`: which of its days).
Hours count from 00:00 of the festival's first day and may pass 24 (`26.5` = 02:30 the next night); a phase with
`"day": -1` repeats on every day. `decor.from`/`to` (from the first day, to the last; negative `from` = the day before),
`decor.kinds`, `decor.venues`. A phase: `venue`, `mode` (how people take part: `mingle`, `watch`, `sit`, `fair`), `pull`
(0..2), `face` (`stage`, `sea`, `bonfire`, `tree`, `sky`, `sunrise`), `show` (`music`, `stage_lights`, `fireworks`,
`sky_lanterns`, `bonfire`). `priority` decides who has a venue when two festivals want it at once (the Sea Festival takes
the fair square from the Saturday market).

## How it works in the game

`ULyudFestivalSubsystem` (Lvl_Scene01) looks at `UGameCalendar` every 2 s:

- **Venues.** A venue is a spot kind or id of the city plan: `Content/City/city_plan.json` "spots" first, then
  `Content/PeopleData/town_plan.json` "spots", then the people's map (`people_places.json` places of a fitting kind, e.g.
  `pier` → beach / promenade; the place nearest the middle of all of that kind). It is grounded by a trace, and the way out
  to sea is found from `ALyudSea`'s depths round it. The plan's own stage and fair stalls count once an actor in the level carries the tag `PlanStage` /
  `PlanStalls` (the public spaces' actor); until then the festival builds its own. Gatherings go to the whole kind of place
  (`square`, `fair`, `bonfire`...), a concert's watchers to `S-SQUARE-AUDIENCE`. The stage stands at the venue's far side facing its middle, the tree
  opposite it, the stalls round the near side.
- **Decorations** (`ALyudFestivalDecor`) are built once in code with the cable car's mesh kit and materials (M_TR_Paint,
  M_TR_Glow): two static meshes per venue, one solid (collision, shadows) and one of lights (no shadows, out of the
  distance fields). Bulbs, paper lanterns, candles and the star are lit by the sun's altitude (dim paper by day); the stage's
  lights chase through their colours while a `stage_lights` phase runs (MID parameters 4 times a second, no mesh rebuilt).
- **Shows.** `ALyudFireworks`: shells from a barge 120 m out to sea, 110-streak bursts that open, droop and fade, a flash
  light, a finale in the last 45 s, the boom late by the distance at 343 m/s. `ALyudSkyLanterns`: up to 70 paper lanterns
  rising and drifting out to sea. `ALyudBonfire`: a log tipi, three flickering tongues of flame and a light; on Maslenitsa
  a straw doll that burns in the first third. All moving parts are movable components with no shadows, no Lumen, no
  collision (perf checklist), pooled and only moved (never rebuilt).
- **Sounds and music** load if present: `/Game/Audio/Festival/S_firework_launch_01`, `S_firework_boom_01..03`,
  `S_bonfire_loop_01` are procedural (`python3 tools/festival_sounds.py`, numpy, seeded; then `IMPORT_ONLY=festival
  unreal/scripts/import_audio.py`); the music `/Game/Audio/Music/MU_festival_1|2` is generated (audio/sounds.json,
  tools/gen_sounds.py --music festival_1 festival_2, about $0.40). Without them the festival is quiet.
- **People.** Each running phase becomes a gathering in the people's life: `UPeopleLifeSubsystem::SetGathering(Id, venue,
  from, to, pull, mode, face point)` / `ClearGathering(Id)` and `SetDayOff(bool)` (the town-life branch). The festival code
  calls them only if they exist (a compile-time check), so it builds on main before they land.
- **Notes.** The morning's note (`festival.<id>.today`) once a day, and "soon" notes ten minutes before fireworks, sky
  lanterns and a bonfire. `ULyudQuests::Notify("festival", id)` at each phase's start; `OnPhaseStart` / `OnPhaseEnd`.
- **Saved**: the fired triggers (first snow) and the notes already shown.

**The hero at the festival** (`TickHero`, every frame while a festival is on):

- On the beach while the sky lanterns go up: `E — зажечь небесный фонарик (30 руб.)` (`ui.festival.lantern_prompt`), up to
  three a night; his lantern rises a little faster than the others (`ALyudSkyLanterns::Release`).
- Quest events (ULyudQuests::Notify): `festival <id>` when a phase starts, `festival_visit <id>` and `<id>.<phase>` when
  he is at a running phase's venue, `festival_watch <show>` and `<id>.<show>` after 30 s in sight of a show (the fireworks
  within 400 m, the bonfire within 6 m, the lanterns within 150 m, the shooting stars anywhere), `festival_lantern
  sky_lantern` for each lantern he lets go.
- Side quests in `Content/UI/quests.json`, started by their festival: `fest_sea_festival` (the fair, the fireworks),
  `fest_lantern_night` (a lantern of his own), `fest_maslenitsa` (the fair, the doll burning), `fest_midsummer` (the
  bonfire, the sunrise), `fest_new_year` (the fireworks), `fest_perseids` (`ALyudMeteors`: shooting stars streaking away
  from the radiant in the north-east every 4-26 s).

API for other systems: `IsFestivalToday()`, `ActiveFestival()`, `ActivePhase()`, `ActiveFestivals()`, `IsDayOff()`,
`Upcoming(days)`, `FestivalName(id)`, `FindVenue(name)`. `IsFestivalToday()` is a holiday's day or its night still running
past midnight; the weekly market does not count.

Console: `lyud.Festival.List [days]`, `lyud.Festival.Report`, `lyud.Festival.Go <id> [phase]` (moves the calendar:
`lyud.Festival.Go sea_festival fireworks`), `lyud.Festival.Fire first_snow`; `lyud.Festivals 0` turns it all off.
