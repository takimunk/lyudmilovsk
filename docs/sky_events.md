# Sky events

What happens in the sky on a given night besides the planets of `docs/telescopes.md`: meteor showers on their real
dates, the real moon's phase and libration, satellites crossing the dusk, planets standing side by side.

**State: the rules and their checks only.** `Private/Sky/SkyEventsModel.{h,cpp}` (namespace `LyudSkyEvents`, plain C++
over the telescopes' `LyudScope::Sky`) and `tools/skyeventsim`. Nothing in the game calls it yet: no meteors are drawn
from it, the eyepiece's moon is not turned by it, no diary or paper line is written from it. That wiring is the next
step (see the end).

## What the model answers

- **Meteor showers.** Eight: Quadrantids (3 Jan), Lyrids (22 Apr), Eta Aquariids (6 May), Perseids (12 Aug), Orionids
  (21 Oct), Leonids (17 Nov), Geminids (14 Dec), Ursids (22 Dec). Each is tied to the sun's ecliptic longitude at its
  maximum, taken from the planets' model, so it falls on its date every year. `Meteors(sky, moon lit, moon altitude,
  cloud cover)` gives what a watcher would count in an hour: the zenithal rate, times the sine of the radiant's height,
  times 2.5 per magnitude of sky lost to twilight and moonlight, times the clear share of the sky; and the sporadic
  background (about 4 an hour in the evening, 12 before dawn).
- **The moon.** `Moon(day)`: longitude and latitude (Schlyter's elements with the larger perturbations, about 2'), the
  lit part, the age, waxing or waning, and the optical libration in longitude and latitude (Meeus ch. 53). `NextPhase`
  finds the next new or full moon. The game's own moon in the sky is `ALyudSky`'s; this is the real one, for whoever
  wants the eyepiece's disc phased and turned as the real moon is that night.
- **Satellites.** Three on circular orbits whose planes turn with the earth's bulge: `mir` (390 km, 51.6°, bright),
  `meteor` (830 km, polar), `zarya` (560 km, 65°). `Where` gives one's place in our sky and whether the sun still reaches
  it; `Passes(from, to)` lists the passes seen (above 10°, sunlit, the sun 6° down), each with its rise, top and end, and
  whether it goes out in the earth's shadow.
- **Conjunctions.** `Conjunctions(from, to)`: pairs of Venus, Mars, Jupiter and Saturn within 3° in a dark sky, with the
  closest night, the separation, the height and evening or dawn.

## Checks

`tools/skyeventsim/run.sh test` (4,438 checks; in `tools/run_all_suites.sh`), against dates anybody can look up:

- the equinox and solstices of 2026 from the sun's longitude; every shower's maximum on its date in 2026-2029;
- new moons 2000-01-06 18:14, 2024-04-08 18:21, 2026-08-12 17:37 UT and full moons 2000-01-21 04:40, 2025-03-14 06:55 UT
  (all but the first are eclipses: the moon is also checked to be at its node) within two hours; thirteen months of
  29.2 to 29.9 days; the libration swinging 6 to 8° in longitude and 6 to 7° in latitude;
- the great conjunction of Jupiter and Saturn on 21 December 2020 (under 0.25°), Venus and Jupiter on 9 June 2026;
- passes: minutes long, sunlit at the top, in a dark sky, none of the low station around midnight in December (it is in
  the earth's shadow then), never over the pole for a 51.6° orbit.

`tools/skyeventsim/run.sh year` prints the game's first year (from 2026-10-20): the Geminids of 14 December are the
year's best (about 110 an hour before dawn, a thin moon), the Perseids of 2027 have an 84 % moon, Mars and Jupiter stand
1.2° apart before dawn on 16 November 2026.

## Next (not done)

1. `ALyudSky` or a small subsystem draws meteors at `Meteors().ShowerPerHour + SporadicPerHour`, streaking away from
   the radiant (the festival's `ALyudMeteors` can serve as the drawing).
2. The eyepiece (`LyudScope::Draw` for the moon) takes the libration and the real phase.
3. A satellite as a moving point at dusk from `Where`; a sight for the sky log.
4. The paper and the diary: tomorrow's shower, tonight's conjunction (`tools/paper/strings_src.py`, `ULyudTelescope`).
