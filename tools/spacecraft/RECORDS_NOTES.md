# The craft's records: what was chosen, licences, how to fetch

The portable record player in «Ласточка» (docs/spacecraft.md) plays eight real recordings whose **recordings** (not only
the compositions) are public domain, CC0 or CC BY. One is CC BY-SA (flagged below). They are fetched per machine, like
the library's books (`tools/books/`) and the Rassvet's films (`tools/films/`): the catalogue `records.json` and the
sidecars are in git; the audio never goes into git or LFS.

Checked 2026-11-27.

## The eight

| Cell | Id | Music | Recording | Licence | Length | Why |
|---|---|---|---|---|---|---|
| 0 | `record_sc_goldberg_aria` | Bach, Goldberg Variations: Aria | Kimiko Ishizaka, piano (Open Goldberg Variations, 2012), archive.org | **CC0 1.0** | ~4:55 | the quietest start; a crowd-funded recording given to the public domain |
| 1 | `record_sc_clair_de_lune` | Debussy, Clair de lune | Laurens Goedhart, piano, 2011, Wikimedia Commons | **CC BY 3.0 (not read here, see risks)** | 5:04 | moonlight on water, while the hero flies away from a moon over an ocean |
| 2 | `record_sc_gymnopedie` | Satie, Gymnopédie No. 1 | Kevin MacLeod (incompetech), Commons | **CC BY 3.0** | 3:07 | the slow weightless one |
| 3 | `record_sc_rachmaninoff_andante` | Rachmaninoff, Piano Concerto No. 1: II. Andante | Neal O'Doan, piano; University of Washington Symphony Orchestra, Robert Feist; Commons | **CC BY-SA 2.0 (share-alike)** | 6:08 | the Russian record: tender, with a real orchestra |
| 4 | `record_sc_strauss_waltzes` | Johann Strauss II, waltz medley | The United States Air Force Strings, Commons | **Public domain** (US government work) | 4:56 | the obvious wink (ships waltzing in space) |
| 5 | `record_sc_air_1920` | Bach, Air ("on the G string") | Joel Belov, violin; Robert Gayler, piano; 1920 cylinder (UCSB Cylinder Archive), Commons | **Public domain** (US recording published before 1926) | 4:20 | the old, far-off one |
| 6 | `record_sc_japanese_sandman` | Whiting, The Japanese Sandman | Paul Whiteman and His Orchestra, 1920, acoustic 78, Commons | **Public domain** (US recording published before 1924) | 2:55 | the warm early-jazz 78; about trading old days for dreams, for the nap on the couch |
| 7 | `record_sc_floating_clouds` | Arold Valda, Floating Clouds (Atmospheric C418 Style) | the composer, OpenGameArt | **CC BY 4.0** | ~4-5 min? | the ambient/electronic one |

About 36 minutes in all: more than twice the 15-minute flight (and the 8x nap eats records fast; the player loops).
Variety: three piano, one orchestra with piano, one string orchestra, one violin cylinder, one dance band 78, one synth.

Attribution lines, exactly as each licence wants them, are in `records.json` (`attribution`) and copied into each
sidecar. CC0/PD records are credited anyway. The game's credits screen should list all eight lines.

## How each licence was checked, and what could not be

The cloud sandbox's proxy refuses every music host (403 for commons.wikimedia.org, upload.wikimedia.org, archive.org,
incompetech.com, musopen.org, freemusicarchive.org, loc.gov, opengameart.org, imslp.org, zenodo.org, huggingface.co;
gutenberg.org answered "down for maintenance"). So:

- **Source pages were read** with a web reader: archive.org and OpenGameArt pages directly; Commons file pages
  through Wikipedia mirrors (download.osmand.net, wiki.riteme.site), since commons.wikimedia.org itself would not load
  in the reader either. The licence wording quoted in `records.json` (`licence_checked`) is what those pages say.
- **No file was downloaded here**, so `sha256`, `bytes` and `seconds` in `records.json` are `null`. The first run of
  `fetch_records.py --pin` on the Mac checks each download against the host's own published hash (Commons sha1 and
  size from the API, archive.org md5 from the item metadata), then writes sha256/bytes/seconds into `records.json`;
  commit that, and from then on a changed file on the host is an error, not a silent swap.
- `fetch_records.py` also reads the licence from the Commons API / archive.org metadata at fetch time and refuses
  anything not public domain / CC0 / CC BY / CC BY-SA (NC, ND, fair use, non-free are refused).

## Risks (plainly)

1. **Clair de lune (Goedhart)**: the recording's existence, performer and length are confirmed (it is the one in
   Wikipedia's *Suite bergamasque* article), but its Commons licence page could not be opened. CC BY 3.0 is what I
   expect, not what I read. The fetch script will refuse it if the API says otherwise. If it is refused, take the
   alternate Debussy: IMSLP lists a CC BY 4.0 *Le Blue Monkey Sax Quartet* arrangement (4:20) and CC BY 4.0 clarinet
   versions, but IMSLP downloads need a click-through, so pin them by hand.
2. **Rachmaninoff (CC BY-SA 2.0)**: share-alike. The processed WAV (an adaptation) must stay CC BY-SA 2.0 and carry the
   attribution; that is fine for a file shipped inside the game (the game itself is not an adaptation of the
   recording), but say so in the credits. Swap it for a PD/CC BY Russian piece if the project wants no SA at all.
3. **The Japanese Sandman**: the 1920 recording is US public domain (17 U.S.C. § 1401, published before 1924) and over 70
   years old in the EU; the Commons transfer was taken from a YouTube upload. A straight transfer adds no new copyright
   in the US; the Commons page is tagged PD. Low risk, but provenance of the transfer is thin.
4. **Strauss medley (USAF)**: PD as a US government work (confirmed on the page), but the page does not say which
   waltzes are in it, and the file is small (1.95 MB for 4:56: low bitrate). The sleeve says only "Waltzes".
5. **Floating Clouds**: CC BY 4.0 confirmed, but it is a fresh upload by a little-known author (one can't fully rule
   out that an uploader claims music that isn't theirs), its length is not on the page, and the direct file URL
   (`opengameart.org/sites/default/files/floating_clouds_aetheria.flac`) follows OGA's usual pattern but was not
   opened. If it fails, fall back to a Kevin MacLeod ambient piece from incompetech (CC BY 4.0) and pin it.
6. **Size**: the WAVs are about 350 MB in all (48 kHz, 16-bit). The design's sound budget (under 12 MB) is about what is
   *resident*: import them into UE as **streaming** sound waves (Loading Behavior: Stream, compression quality ~40), so
   only a small buffer is in memory, and load only on Europa (`ULyudPrefetch`). Being a download, they are not in the
   packaged LFS budget either.

Alternates I looked at (in `records.json` → `alternates`): Ferera & Louise *Hawaiian Portuguese Tango* (PD, 1910s;
page not read), Tchaikovsky *June* by Bernd Krueger (CC BY-SA 2.0 DE, but a MIDI rendering), Naxos's Bach Air (CC BY-SA),
Ishizaka's *Aria da capo* (CC0, same item). Rejected: the Gutenberg *Clair de lune* by the Hotel Commodore Ensemble
(recorded 1927: still protected in the US until the end of 2027 whatever the page says), the Commons *Moonlight
Sonata* files (MIDI renders), IMSLP's piano Clair de lune recordings (all NC or ND).

## How to fetch (on the Mac)

```sh
python3 tools/spacecraft/fetch_records.py --pin     # first time: download, check, pin, render
git add tools/spacecraft/records.json audio/music/spacecraft/*.json   # the pins and the sidecars, never the audio
python3 tools/spacecraft/fetch_records.py --check   # later, offline: everything matches
python3 tools/spacecraft/fetch_records.py --list    # the table
python3 tools/spacecraft/fetch_records.py --force record_sc_air_1920   # one again
```

Needs ffmpeg/ffprobe (Homebrew). Output: `audio/music/spacecraft/_src/<id>.<ext>` (the original) and
`audio/music/spacecraft/<id>.wav` (both ignored by `audio/music/spacecraft/.gitignore`), and `<id>.json` (committed:
the same shape as `audio/music/europa_1.json` plus title, composer, performer, attribution, source hash).
Processing (the same for all, per `records.json` → `target` and each `recipe`): trim silence below -50 dBFS at both
ends; high-pass (and low-pass) only for the two acoustic transfers and the two older/live ones; mono for the cylinder and
the 78, stereo for the rest; two-pass EBU R128 loudnorm to **-20 LUFS**, true peak -1.5 dB; 50 ms fade-in and 1.5 s
fade-out; 48 kHz 16-bit WAV; **no vinyl noise baked in** (the player adds crackle and wow). An ffmpeg of another version
may give a WAV with a different hash: `--check` says so; re-run with `--force` and commit the new sidecar.
The render path (trim, two-pass loudnorm, fades, sidecar) was tested here on a synthetic file: -20.2 LUFS out, trimmed.

## The atlases

`python3 tools/spacecraft/sleeves.py` (PIL + numpy, deterministic: two runs give identical PNGs).

- `textures/spacecraft/T_SC_sleeves_{ru,en,ja}.png`, 2048x1024: eight 512x512 sleeve fronts, 4 columns x 2 rows.
- `textures/spacecraft/T_SC_record_labels.png`, 1024x512: eight 256x256 round centre labels with an alpha cut-out (the
  label disc and the spindle hole), same grid, no words (numbers and 33⅓ only), so one atlas serves all languages.
- **Cell n** = the record whose `cell` is n in `records.json`: column `n % 4`, row `n // 4`, row 0 on top;
  UV `((n % 4) / 4, (n // 4) / 2)` to `+(1/4, 1/2)`.
- Style: flat paper colour with a faint grain, one ink, a geometric motif in a framed panel (`sleeve.motif`: lattice,
  moon, columns, birch, waves, horn, lantern, clouds), a type block (composer in M PLUS 1p Bold, title in Dela Gothic
  One, performer in M PLUS 1p). The label name is the craft's own «Бортовая фонотека» / Onboard Record Library /
  機内レコード文庫, not any real label's mark. Every word comes from `records.json` → `text` (ru source, en, ja); both
  fonts cover Latin, Cyrillic and Japanese. Titles shrink and wrap to fit; checked by eye in all three languages.

## Strings

`records.json` → each record's `text` holds the sleeve name, title, composer, performer and a 1-2 sentence sleeve note
in ru/en/ja, plus `label_name`/`label_side`. These tools do not write the game's string tables; whoever wires the items
copies them as `item.<id>.name` (= `text.name`) and `item.<id>.desc` (= `text.note`, then the performer), per the
localization skill.

## Fallback when the files are not fetched

The engine should treat a missing `audio/music/spacecraft/<id>.wav` (or a missing imported `MU_SC_*` asset) as "this
record is a sleeve without a disc": keep the sleeve in the rack, and when it is put on, play one of the game's own
music cues instead, chosen by the record's `mood`: piano → `dawn_2` / `day_2` / `rain_1`; night/moon/still →
`night_1`, `night_2`; space/weightless/ambient → `europa_1` (then `ural_1`); orchestra/waltz → `dusk_1`; retro/jazz →
`dusk_2`. With nothing fetched at all, the house record (`flight.record.house`) is `europa_1`. The toast shows the
cue's own record title in that case, never the missing record's, so the credits stay true to what is heard.
