# The sanatorium's procedures as scenes

Backlog item 49 (docs/interactives_backlog.md). Before this, the course of cure at «Корона» (docs/city_interiors.md
"Stays") stamped the hero's book with a line and a fade. Now each procedure is a short hands-on scene in its own room. The act
runs as before once the scene is over, with the same price, stamps, minutes and lines, and how the scene went is written over
the act's words.

## The rooms

The hydrotherapy wing (`blender/city/buildings/round_sanatorium.py`, `_hydro_wing`) carries the ground hall on east of the
treatment room, through a door in its end wall at -45°. It has three bays:

| bay | things | act |
|---|---|---|
| 7, inhalation | `inhale_1..3` chairs with wall inhalers and masks, the nurse's table, `inhale_clock` | `san_inhale` (sit, 15 min), `san_proc_clock` |
| 8, hydro | `hydro_mud` tiled mud bath behind a curtain, `hydro_charcot` the Charcot bay and the nurse's desk with gauges and a hose, `hydro_robes` hooks with robes over a bench | `san_mud` (lie, 30 min), `san_charcot` (stand, 15 min), `san_robes` |
| 9, massage | `massage_table`, `massage_shelf` with oils and towels, `massage_radio` | `san_massage` (lie, 25 min), `san_oils`, `san_massage_radio` |

Elsewhere:

* the pump room's spring (`pump_room`, `san_pump`);
* the doctor's check at `treat_checkup` (`san_checkup`), with a cuff and a spirometer on the bench;
* the evening waltz at `lobby_dance` (`sanatorium_evening`);
* the lobby's film night at `lounge_projector` and `lounge_screen` (`san_film`, `like: cinema`, free, 20–23 on Wednesday
  and Saturday).

The wing passes lint at 0/0. It has two lit pendants, and it doesn't shift any existing `kit_*` id.

## A procedure: the act's `proc`

```
"proc": {"title": key, "keys": key, "when": {line conditions}, "steps": [step, ...],
         "good": [lines], "fair": [lines], "poor": [lines], "good_share": 0.7, "fair_share": 0.35,
         "collect": "set/piece", "vitals": true}
step: {"do": "say|pick|pour|sip|breathe|tap|cut|measure|guest|news", "lines": [...], "guests": {"<key>"|"": [...]},
       "by_pick": true, "secs": s, "count": n, "every": s, "window": s, "in": s, "out": s, "mark": [lo, hi], "minutes": n}
```

`tools/city/interiors.py` checks every field and string. `CityThingsModel.h` holds the model: `FProcDef`, `FProcState`,
`ProcNow`, `StartProc`, `TickProc`, `PressProc`, `EndProc` and `FinishProc`.

* **`say`**: a line, which E skips after 0.7 s. A `say` with no line that holds now is left out. This is how the inhaler's
  herb changes by weekday.
* **`pick`**: the first line that holds is the prompt, and the next three are the options (keys 1–3). With no choice the
  first option is taken after 10 s. A later step with `by_pick` shows the line of the option picked: the cup picked decides
  the pouring words.
* **`pour`**: hold E and the cup fills over `secs`. Let go inside `mark` for full marks, just under it for half. Holding to
  the brim spills.
* **`sip`**: a clock ticks `count` times, every `every` seconds. E within `window` of a tick is a sip, and each tick counts
  once. The pump room's sipping, the Charcot jet and the waltz's steps all work this way.
* **`breathe`**: a ring grows for `in` seconds and shrinks for `out`. Hold E while it grows and let go while it shrinks.
  Letting go only counts as breathing out after a breath taken in, so doing nothing scores nothing.
* **`tap`**: E `count` times within `secs`, like the cuff's bulb.
* **`cut`**: the screen goes dark while the robe and slippers happen. Its `minutes` pass then and come off the act's own.
* **`measure`**: the doctor's numbers (see below). Its line gets them as `{0}`..`{9}`.
* **`guest`**: a guest at the same procedure says their own line, from `guests[<key>]` or else `guests[""]`. `{0}` is
  their name. With nobody there, the step is left out.
* **`news`**: the town's talk, the first "everywhere" topic line that holds (item 22). Otherwise the step's own lines are
  used.

The result is the share of hands-on steps done well:

* `good_share` or more gives the `good` lines and the `collect` piece (once);
* `fair_share` or more gives `fair`;
* anything less gives `poor`.

Each result has its own page of lines (`<thing>#proc<n>`).

The `when` gives the scene. Without it, or when the act would be refused, the act runs plainly. The pump room's spring is a
scene only on the voucher, and the waltz only from 19 to 23. Without the voucher the treatment rooms say their `no_stay`
words.

## The doctor's numbers

`FModel::VitalsOn(day)` gives weight, pressure, pulse and lung capacity. They improve with the procedures gone through, up
to `VitalsCourse` = 30 (about a week of five a day), with a little of the day's own variation.

The card keeps the first numbers to compare with (`{5}`..`{9}`). The save holds `p:<proc>` (procedures gone through) and
`m:first` / `m:last` (the numbers).

Collection `san_proc` has five pieces: `inhale`, `mud`, `charcot`, `massage` and `checkup`. Each comes from the first good
result.

## The guests at the procedures

`tools/town/sanatorium_guests.py` gives each guest a half-hour turn in the inhalation room (activity
`cure_sanatorium_inhale`, place `P-SANATORIUM-INHALE`). The turns run Monday to Saturday on chairs `inhale_2` and
`inhale_3`; `inhale_1` by the door stays free for the hero. The n-th guest sits at `INHALE_HOURS[n // 2]` (10, 10, 11, 11
with the four guests). The way in follows the ring of the hall and goes through the wing's door.

When a scene begins, the game takes as companions the residents whose home is `P-SANATORIUM/...` and who are within 13 m of
the thing. The guest's key is the second part of their person key (`t_gromov_pyotr` gives `gromov`). They speak in the
`guest` step.

## The game

`ULyudCityThings` handles the scene:

* `Run` asks `ProcNow` first and plays the scene (`BeginProcedure`).
* The hero sits on the chair, or lies on the bed or the bath, when he is at it.
* The panel `SLyudCityProc` shows the title, the step's line, a gauge (a cup with its mark, a clock with the sips, a
  breathing ring, the cuff's bar), the options, the count and the keys.
* The keys are polled and never consumed. E held pours and breathes. The scene's tick comes before the bed's, so E doesn't
  get him up.
* Walking off, getting up, Esc or a screen coming up ends the scene with nothing given ("I walked out before the end").
* At the end, `FinishProcedure` announces the stamps and runs the act again with `bRunAfterProc`. That run asks no scene and
  offers no talk chooser, it puts the result over the act's line, and it takes off the minutes the cuts let pass.

Console: `lyud.City.Proc <thing id> | press [1-3] | quit`. Developer mode (F7, Actions): "Interiors: sanatorium procedure
(nearest)".

## Checks

`tools/citysim/run.sh` (`ProcChecks`) plays every procedure with scripted keys. It checks:

* a careful hand gets the good words and the piece, and a careless one the poor words;
* the second cup's own pouring words;
* Tuesday's herb;
* Gromov's line with him in the next chair, and no guest line alone;
* walking off counts nothing;
* the mud's cuts take off part of its half hour;
* the doctor's numbers get better over five days, and the first ones are kept and saved;
* the collection is complete;
* the waltz with Sonya;
* the lobby's film night on Wednesday and not on Monday.

## Still owed (Mac)

* The compile (the new `SLyudCityProc` widget and the procedure plumbing in `LyudCityThings.cpp`).
* Shots of three procedures (the pump room's cup, the inhaler's ring, the doctor's card).
* A bench in the wing.
* `build_city` + `import_city` for `round_sanatorium` (the wing's mesh).
* The sounds the acts name that have no soundscape pool yet are silent until they exist.
