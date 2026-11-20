# The hero's instruments: the pianos and the guitar

The hero plays. The town's pianos (the hall «Своды»'s stage and dance pianos, the school's music room, the sanatorium lobby,
two of the clubs, the library's listening pod) open a keyboard at the foot of the screen instead of the
old "played a little" line, and he sits on the stool first. His own guitar is bought at the records shop «Волна» (180 rub)
and played from the diary ("Play"), best by a bonfire. Backlog item 15 (docs/interactives_backlog.md).

## Playing

| Keys | What |
|---|---|
| A S D F G H J K L ; ' | white keys, C to the F above the next C (an octave and a half) |
| W E T Y U O P | black keys |
| Z / X | an octave down / up (C2 to C5 as the lowest key) |
| Shift | the sustain pedal (piano) |
| 1 to 8 | the guitar's chords, strummed: C G Am F Dm E Em D |
| Q | the tunes he knows (W/S or a number, E to start) |
| E or Space | in a tune: strike the note as it reaches the line |
| Esc / Tab | stop the tune, shut the list, get up |

**Tunes** are an easy rhythm game: after a count-in of four beats the notes slide to the line, and E strikes the one there.
Within 0.07 s it is "spot on", within 0.17 s "good", a note let pass stays silent ("missed"). Score = (spot on + 0.7 good -
0.15 stray presses) / notes; stars from 0.5, 0.75 and 0.92. The guitar strums the tune's chords by itself, and the piano's
left hand holds them softly. A tune with a star or more counts in `Played` and tells the quests `play_tune <id>` (and
`play_tune bonfire` by a fire); learning one tells `learn_tune <id>`.

**The tunes** are in `Content/Music/tunes.txt` (format at the top of `Private/Music/TunesModel.h`): ten melodies, every one free
(folk tunes, composers and lyricists dead by 1955; each block says who and when). Two are known from the start (Twinkle, Happy
Birthday); the rest are learned from sheet music in the town: the act ids listed in a tune's `sheet =`, today the school music
room's blackboard (`school_music_board`), the hall's two music stands (`hall_sheet_stage`, `hall_sheet_dance`) and the library's
music shelf (`rd_music_orchestra`). Using one of those things teaches the next tune there he doesn't know (the line says which,
a toast too); when he knows them all the thing says its usual words. Titles: `music.tune.<id>`.

## The sound

Nothing is recorded or imported: `Private/Music/InstrumentSynth.h` (engine-free) synthesises the notes as he plays, into one
`USoundWaveProcedural` (32 kHz mono, about 50 ms rendered ahead, no allocation per frame).

- **Piano**: up to 14 partials with the stretched tuning of a real string (inharmonicity grows up the keyboard), a second string
  a cent or two sharp on the low partials (the chorus of a unison), a two-stage decay, a hammer thump, dampers (0.05 s) and the
  pedal. 14 voices, the oldest stolen.
- **Guitar**: Karplus-Strong strings with a loop filter whose exact phase delay is cancelled by a tuned allpass (pitch within a
  tenth of a cent), a body resonance, rings until it dies or the same string is struck again.

`tools/instrumentsim/run.sh` checks it offline (514 checks): every note's pitch by autocorrelation (piano MIDI 36-89 within 4
cents of equal temperament plus its stretch, guitar 40-84 within 3 cents), no clipping, dampers and pedal, voice stealing, the
cost (a ten-note chord renders a second in about 11 ms), the tunes (range, length, chords under the melody, notes far enough apart
to strike), the rhythm judging. `run.sh wav <dir>` writes every tune played perfectly on both instruments.

## The people round him

Every 1.5 s while he has been playing for two seconds or more, the townsfolk within 10 m (11 m by a fire) may sway to it
(`ReactManner sway`, the warm more readily), and one of them may say a word (town talk `hero_plays`, sets in
`tools/barks/lines/14_instruments.txt`). A tune played with a star or more ends in applause (`clap`, more people for more
stars) and a word (`hero_played`). With the guitar by a lit bonfire (`ALyudBonfire` of the festivals, a lit `ALyudCampfire`
of the north) the people round the fire sing the tune's words with him, one line, one voice at a time (`hero_sing_<tune>`,
tunes with `sing = yes`; the sung words are public domain in their own language and our own translations elsewhere).

## Code

- `Public/Music/LyudInstruments.h`, `Private/Music/LyudInstruments.cpp`: the world subsystem (save record "Instruments": the
  tunes learned, how often each was played).
- `Private/Music/SLyudInstrument.*`: the view (keys with their letters, strings and chords, the lane, the list, the stars).
- `ULyudCityThings`: act field `"open": "piano"` (tools/city/interiors.py `OPEN_SCREENS`); an act that sits seats the hero
  before its screen opens; `AddActHook` for more listeners after `ActHook` (the sheet music).
- The guitar: `Good(TEXT("guitar"), ...)` in `LyudInventory.cpp`; on sale through `tools/music/content.json` (shop
  `records_wave`, `extra`); the diary's "Play" in `SLyudDiary.cpp`; the icon cut from `design/instruments/icon_grid.json` by
  `tools/instruments/guitar_icon.py`.

## Testing

Console `lyud.Instr piano | guitar | close | learn <id|all> | play <id> | auto <id> | list` (`auto`: every note struck on time
by itself, for a recording or the bench). Developer mode, group Instruments: piano here, guitar (given and opened), learn every
tune, to the hall's piano, a tune by itself.
