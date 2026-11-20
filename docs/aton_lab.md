# ATON OS in the Academgorodok: the lab network «ЛАБ-2»

Backlog item 16 (docs/interactives_backlog.md). The hero's computer in the club runs ATON OS; the Academgorodok's
laboratory has terminals of the same OS on its own network, with the lab's programs. One terminal stands in the
scientists' library (Novikova 1, ground floor), at the hall end of the last stack, beside the globe.

State on 2026-11-20: written in the cloud. The plain C++ models and all data pass `tools/atonsim/run.sh`, and the
engine side builds on the Mac (LyudmilovskEditor Mac Development). It has **not been run in Unreal yet**: no terminal
has been seen in the level. First run: `Dev.QA teleports aton`, then the list under "First run on the Mac".

## What the player gets

- **E in front of the terminal** moves the view to the screen and gives the OS the mouse and keyboard (no chair, no
  cutscene). «Отойти» on the menu bar gives them back. E is polled, never consumed.
- The machine boots into **Сеанс** (Session): you are a guest; four digits log you in as the duty shift.
- **A guest** has the weather station, the two games, the calculator and the common mail (four letters).
- **The duty shift** also has its mail (thirteen letters; a letter of a thread opens once the one before it
  was read) and the telescope queue.
- **The password** is `0314`, the hour the hero woke up at the club computer. It is never written in the mail. The
  common mail's letter "password" says the slip was last seen in the letter boxes in the lobby of the Bridge House, on
  the upper terrace; the boxes there (`P-BRIDGEHOUSE/lobby_mail`, act `bh_mail`) now show their notes one per look
  (`rotate: use`, seven notes), and the slip is the third of them. After two wrong tries the session window itself points at the
  common mail.

| Program | Where | What it is |
|---|---|---|
| Сеанс / Session | lab | guest or duty shift; log in, log out |
| Почта ЛАБ-2 / LAB-2 mail | lab | the lab's old correspondence, `Content/AtonOS/lab_mail.json` |
| Ночная вахта / Night Watch | lab + club | the staff's text adventure: 32 scenes, three endings, picked by number or click |
| Шахматы / Chess | lab + club | full rules, three levels, take-back, a score sheet, wins counted per level |
| Метеостанция / Weather station | lab + club | the game's own weather now and 6/12/18/24 h ahead |
| Телескоп / Telescope | lab (duty shift) | four slots a night for three nights; one may be the hero's |
| Калькулятор | lab + club | as before |

The club computer keeps everything it had and gains Night Watch, chess and the weather station (two columns of icons;
terminal words `open вахта|шахматы|погода`).

**The telescope queue.** A night has four two-hour slots (22, 00, 02, 04); the lab's people hold two or three of them,
by a fixed timetable (`AtonLab::ScopeNight`). The hero may book one free slot a night. When the slot has passed, the
automat's plate goes to the journal: a line about the target if the game's cloud cover in the middle of that slot was under 0.6,
"the dome stayed shut" otherwise. This is the queue only; pointing a telescope by hand is backlog item 5, which can
read the same targets (`AtonLab::ScopeTargetId`).

## Lore

The mail hints toward the ending and never says it (docs/story/inbox.md, section of 2026-11-20, **a proposal, not
canon until Dima reads it**): a solar flux that reads a little low and steadier than a star should, star field No. 14
that never changes by one star, a census that always comes to the same number, a train whose passengers nobody can
count. Night Watch plays the same notes as a joke the staff wrote for themselves. If the story changes, the words are
in `tools/aton/strings/mail.json` and `tools/aton/night_watch_src.py`.

## Files

| What | Where |
|---|---|
| Models, plain C++ (no engine) | `Source/Lyudmilovsk/Private/AtonOS/Lab/`: `LabChess`, `LabAdventure`, `LabNet` (password, letters, queue, weather sums, the saved state) |
| The programs' windows | `Private/AtonOS/AtonLabApps.cpp` (`AtonLabUI::MakeProgram`), registered in `AtonApps.cpp` with `FAtonAppInfo::Profiles` |
| Profiles | `AAtonComputer::Profile` "club" / "lab", `SAtonDesktop` `.Profile()`; `bUseSeat = false` for a machine with no chair |
| The terminal, the state, the prompt | `Public/AtonLab.h`, `Private/AtonLab.cpp`: `ALyudLabTerminal` (desk, tube monitor, keyboard, stool, built in code with CityKit), `ULyudAtonLab` (saved as "AtonLab", one line) |
| Where terminals stand | `tools/aton/terminals.py` -> `Content/AtonOS/terminals.json` + `Content/Dev/teleports/aton.json`; it builds library.py and fails if the desk or the person's place meets furniture |
| Letters | `Content/AtonOS/lab_mail.json` (ids, boxes, `after`), words in `tools/aton/strings/mail.json` |
| Night Watch | `tools/aton/night_watch_src.py` -> `Content/AtonOS/adventure.json` + `tools/aton/strings/adventure.json` |
| Interface words | `tools/aton/lab_strings_src.py` -> `tools/aton/strings/lab.json` |
| Game tables | `python3 tools/aton/strings.py` -> `Content/Localization/aton/strings_{ru,en,ja}.json` (generated: never edit) |
| Checks | `tools/atonsim/run.sh` (row `atonsim.test` of tools/run_all_suites.sh) |

Adding a terminal: a row in `TERMINALS` of tools/aton/terminals.py (house frame of blender/academgorodok), run it, run
the check. Adding a letter: an entry in lab_mail.json and its five keys in mail.json, then strings.py.

## Quests

The programs report to the quests as event `do`, targets `aton_login`, `aton_mail_all`, `aton_night_watch` (a new
ending), `aton_night_watch_best`, `aton_chess_win`, `aton_chess_win_3`, `aton_scope_book`, `aton_scope` (a plate in the
journal). No quest uses them yet.

## Developer mode

- Teleport **«Терминал ЛАБ-2 в библиотеке»** (`aton_terminal`, group АТОН ОС).
- Actions: **club computer: LAB-2 network / normal** (`lyud.Aton.Profile lab|club`: try every lab program from the
  chair in the club), **log in as the duty shift**, **reset everything**.
- Console: `lyud.Aton.Lab login|logout|reset|state`, `lyud.Aton.Use` (sit down to the nearest terminal),
  `lyud.Aton.Leave`. Log lines start with `ATONLAB`.

## Performance

Nothing is built per frame: the terminal's mesh is built once at BeginPlay and stays out of the global illumination;
its screen repaints twice a second until somebody sits down (as the club computer's). The subsystem's tick is one
distance test per terminal and a queue check every 5 s. The weather and queue windows rebuild their rows only when a
shown word changes (checked every 2 s). The chess machine thinks once per move, about 65 ms at level 3 in the host
build. No benchmark against main has been run yet (no engine in the cloud).

## First run on the Mac

1. Build (tools/wait_ubt_queue.sh): green on 2026-11-20.
2. `Dev.QA teleports aton`; look at the desk: it should stand between the last stack and the globe, its back to the
   back aisle, clear of both. Move it in tools/aton/terminals.py if it reads badly.
3. E at the terminal, the six programs, «Отойти». E is polled, so check that the hero's own E does nothing else in
   that frame. Mouse on the world screen from the terminal's own camera is the part
   most likely to need a fix.
4. The same programs from the club chair (action "club computer: LAB-2 network").
5. Bench against main (benchmarks alone), shots for the backlog.
