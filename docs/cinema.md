# The Rassvet's newsreel: real old films before every session

Backlog item 13 (docs/interactives_backlog.md). The house of culture «Рассвет» (plot P-CLUB2) shows its fictional
features by the clock (acts `cinema_week` and `cinema`, lines `city.rassvet.film.*`). Before each of those sessions the
projectionist now runs a **newsreel of real films**: public-domain shorts and early animation of 1895–1908, with the old
lecturer's words as subtitles, the hall dark, the beam with dust, the projector whirring and the hall laughing at its places.

## What the hero sees

1. He buys a ticket at a hall seat (E on `seat_rN_bM`, 30 roubles, the act sits him) and gets a **ticket stub**
   (`ticket_stub`, a keepsake; the session's line still goes to the films collection `films/<line>` as before).
2. The hall's lamps go out (`ULyudWindowLights::SwitchLampsNear` round the hall's middle; back to the hours after).
3. A title card ("Киножурнал «Рассвета»": film, maker, year) for 4 s, then the film plays on the screen: a picture laid 2 cm
   in front of the screen's own card, a beam from the booth's porthole in the back wall.
4. The lecturer's lines (`film.<id>.l<n>`) come up as subtitles at their share of the film and stay about 7 s; the hall's
   laughs (`cinema_laugh`) play at theirs. Shares (0..1), not seconds, so any copy's timing fits.
5. After the day's films the session's own feature runs as before (the act's lines as subtitles, the minutes passing).
6. E, Esc or getting up leaves at any moment (the feature is dropped too). Each film seen to the end counts once:
   toast «Старое кино: N из 9».

The programme is by weekday (Mon factory + train, Tue sprinkler + baby, Wed Cohl + Méliès' rubber head, Thu Chomón's
electric hotel, Fri train + sprinkler, Sat Méliès' Moon, Sun the Great Train Robbery); a holiday shows the Moon. The town
paper prints the day's programme under the cinema line ("Перед каждым сеансом — старое кино: …").

## Where things are

| What | Where |
|---|---|
| Catalogue: films, the rule, lecturer's words ru/en/ja, laughs, programme, the hall's geometry | `tools/films/catalog.py` |
| Data generator → `Content/City/cinema.json`, `Content/Localization/films/strings_<lang>.json` | `tools/films/films.py` (`--check`) |
| Fetching the films (Mac: Commons + ffmpeg) → `Content/Films/<id>.mp4`, `films.lock.json`, `SOURCES.md` | `tools/films/fetch_films.py` |
| Sources and licences | `tools/films/SOURCES.md` |
| Model (plain C++: programme, cues, laughs, which act has a newsreel) | `Private/City/CinemaModel.h` |
| The engine side | `ULyudCinema` (`Public/City/LyudCinema.h`, `Private/City/LyudCinema.cpp`) |
| The hook | `ULyudCityThings::Run` (Screen branch) holds the session's show; `ReleaseHeldShow` |
| Materials `/Game/Cinema/M_FilmScreen`, `M_FilmBeam` | `unreal/scripts/make_cinema_materials.py` |
| Sounds `cinema_laugh` (new), `projector_whirr` (was registered, not generated) | `audio/sounds.json`, `tools/gen_sounds.py` |
| Offline checks | `tools/cinemasim/run.sh`, `tools/papersim/run.sh` |

## The licence rule

Every maker died in 1955 or earlier and every film was published before 1929: public domain in the USA (published before
1929), in the EU, Russia and the UK (life + 70), and in Japan (life + 70, and the old 70-years-from-release term for films).
The one exception is **Mexico** (life + 100): a build shipped there leaves `Content/Films` out, and the cinema then goes
straight to the feature with a note. `fetch_films.py` takes a Commons copy only when its page's licence is Public domain,
so a restoration that put a new licence on its music or tinting never gets in (the films are silent: the script drops any
sound track too). The library's books use the same rule (`tools/books`, memory `books-library`).

None of the chosen films has intertitles, so "subtitles for intertitles" became the lecturer: the hall's projectionist
reads short comments, as lecturers did in the silent era (a benshi in Japan). They are ours, CC BY 4.0.

## Files that are not in git

`unreal/Lyudmilovsk/Content/Films/` is ignored and staged as plain files (`DefaultGame.ini` `+DirectoriesToAlwaysStageAsNonUFS=(Path="Films")`),
like the books. Each machine runs `python3 tools/films/fetch_films.py` once (needs `ffmpeg`; under 100 MB for all nine at
640 px H.264, no sound). `films.lock.json` pins the Commons file of each film, so the next machine fetches the same copies;
`--pick <id>="File:…"` pins another one by hand.
The script sends a generic User-Agent naming no one; `FILMS_UA_CONTACT="…"` adds a contact of the runner's choosing
(Wikimedia asks automated clients for one; it is never set by default).

## Engine notes

- The film plays through `UMediaPlayer` + `UFileMediaSource` + `UMediaTexture` (module `MediaAssets`; on the Mac AvfMedia or
  Electra decode H.264). The picture and the beam are `UProceduralMeshComponent`s on a transient rig spawned for the run and
  destroyed after (no collision, no shadows, no distance-field lighting); nothing is rebuilt per frame.
- Without the materials the films still run as subtitles and the log says which script to run.
- The rig is built only while a newsreel runs; the subsystem does not tick otherwise.
- Saved: `SeenFilms` under the save key `Cinema`.
- Console: `lyud.Cinema.Status`, `lyud.Cinema.Play [film id]` (the day's newsreel or one film at the Rassvet, no session),
  `lyud.Cinema.Stop`. Dev mode: action `cinema.play` (Interiors).

## Not done yet

- The club's summer screen on festival nights (the brief's second screen): the data has a `festival` programme, but there is
  no outdoor screen thing yet; a `CINEMAS` entry with its plot, screen and acts is all it needs once one exists.
- The townsfolk's house (docs/town_life.md, "The new activities") comes to two fixed sessions, 15:00 and 19:00, whatever the
  programme says.
