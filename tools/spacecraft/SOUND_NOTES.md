# «Ласточка»: the sound set

Made by `tools/spacecraft/sounds.py`: procedural, seeded, deterministic (two runs give the same sha256 for every
file), numpy + scipy, nothing paid, no network. 46 ids, 75 files, **11.97 MB** of wav (11.41 MiB, under the 12 MB
budget in `docs/spacecraft.md`). 16-bit mono. Everything is 48 kHz except five loops whose content sits behind the
hull (structure-borne, all under about 8 kHz), which are stored at 24 kHz: `sc_engine`, `sc_entry_roar`, `sc_wind`,
`sc_chute_creak`, `sc_afloat`. Each of those sidecars records the energy above the new Nyquist (-48 to -122 dB,
nothing you can hear). Set `LOW_RATE = RATE` in the script for 48 kHz throughout (+1.4 MB, over the budget).
Every file is normalised to a -6 dBFS peak. Levels below are relative to that.

```
python3 tools/spacecraft/sounds.py             # write all, then print the check table
python3 tools/spacecraft/sounds.py engine roar  # only ids with these words
python3 tools/spacecraft/sounds.py --check      # only measure what is on disk
```

Everything goes through one short synthetic impulse response of a "small metal cabin" (2.6 m across, quilted
walls: highs die fast, the shell rings a little), so all the sounds share one room. Loops close exactly. Each
one is built on three copies of a single period: noise exactly N samples long and tiled, modulations with whole cycle
counts, tones on the 1/T grid, events placed in every copy. All processing (filters, saturation, the reverb) runs
over the three copies, and the middle copy is kept. The difference between copies is at most 4e-10 of peak, and the
step across the wrap falls inside the signal's own step distribution (see the check table). Co-prime modulation
counts and smooth periodic random curves keep a 6 s loop from sounding like a 6 s loop.

## Ids

`seed` is the first variant's seed. Variant n uses seed + n - 1, recorded in each sidecar. RMS is in dBFS.

| id | kind | var | seconds | kHz | RMS | what, and what drives it | 2D / positional |
|---|---|---|---|---|---|---|---|
| `sc_cabin_hum` | loop | 1 | 6.00 | 48 | -19 | the cabin's 400 Hz electrics and two fans. On from `power` (the first switch) till the hatch opens afloat | 2D bed |
| `sc_fan` | loop | 1 | 6.00 | 48 | -19 | one fan, for the two fan meshes (optional, layered quietly over the hum) | positional |
| `sc_gyro_whine` | loop | 1 | 6.00 | 48 | -16 | the gyros running. From the end of `sc_gyro_spinup` till splash | 2D bed (or at the panel) |
| `sc_gyro_spinup` | one-shot | 1 | 4.00 | 48 | -19 | the `gyros` switch: relay, motor growl, the whine rising to 1600 Hz. Ends at full whine | at the panel |
| `sc_air_hiss` | loop | 1 | 6.00 | 48 | -21 | life support, from the `air` switch. Gain follows the model's fan or O2 flow if it has one | 2D bed |
| `sc_radio_static` | loop | 1 | 6.00 | 48 | -21 | the radio speaker when the radio is on and nothing is heard. Up during `entry` blackout | positional (speaker grille) |
| `sc_beacon` | loop | 1 | 6.00 | 48 | -18 | the radio on in `coast` and `approach`: a far two-tone telemetry beacon | positional (speaker grille) |
| `sc_toggle` | one-shot | 3 | 0.37 | 48 | -24 | any of the five guarded toggles, cabin light, CRT mode, radio | at the switch |
| `sc_cover` | one-shot | 2 | 0.41–0.43 | 48 | -26 | the red cover over ПУСК (and over the parachute handle) | at the cover |
| `sc_button` | one-shot | 2 | 0.38–0.39 | 48 | -23 | ПУСК pressed | at the button |
| `sc_relay` | one-shot | 2 | 0.34–0.39 | 48 | -26 | 40–80 ms after any accepted switch (02 is a double stage, use it for `arm` and ПУСК) | behind the panel |
| `sc_buzzer` | one-shot | 2 | 0.70 | 48 | -18 | wrong switch order (01 one long buzz, 02 two short) | panel |
| `sc_chime` | one-shot | 2 | 0.91 | 48 | -18 | the attention tone before a callout or a lamp (01 two notes falling, 02 three rising for good news) | speaker grille |
| `sc_lamp_tick` | one-shot | 2 | 0.10–0.12 | 48 | -28 | an annunciator lamp lighting | panel |
| `sc_hatch_close` | one-shot | 2 | 1.21–1.26 | 48 | -22 | the hatch swung shut in `prelaunch` (the clang at 0.18 s, then three dogs) | at the hatch |
| `sc_hatch_open` | one-shot | 2 | 1.50 | 48 | -23 | the hatch opened (`parked`, `afloat`) | at the hatch |
| `sc_wheel` | one-shot | 2 | 0.83–0.86 | 48 | -22 | the hatch wheel turned (after close, before open) | at the hatch |
| `sc_belt` | one-shot | 2 | 0.39–0.45 | 48 | -25 | harness buckle, on sitting (and when unbuckling afloat) | at the couch |
| `sc_seat` | one-shot | 2 | 0.67–0.71 | 48 | -19 | settling into the couch, also waking from the nap | at the couch |
| `sc_ignition` | one-shot | 1 | 3.00 | 48 | -23 | `liftoff`: valves, three igniter cracks, light at 0.35 s, the burn building. Ends at full burn | 2D |
| `sc_engine` | loop | 1 | 6.00 | 24 | -18 | the burn through the hull, during `ascent` and the correction burn | 2D bed |
| `sc_cutoff` | one-shot | 1 | 1.51 | 48 | -25 | `engine_cutoff` (and the end of the correction burn): thrust dies in 0.12 s, then plumbing ticks | 2D |
| `sc_thruster` | one-shot | 3 | 0.39–0.49 | 48 | -22 | attitude puffs in `coast` and `approach` (03 is a double puff) | 2D (or by thruster side) |
| `sc_sep_bang` | one-shot | 1 | 1.24 | 48 | -23 | `module_sep` (700 s): pyro double bang, the hull ringing down | 2D |
| `sc_creak` | one-shot | 4 | 0.70–0.76 | 48 | -21 | hull creaks under g: in `ascent`, `entry`, the chute jolts | positional (random hull points) |
| `sc_rattle` | loop | 1 | 6.00 | 48 | -26 | loose things rattling, gain from shake | 2D bed |
| `sc_entry_roar` | loop | 1 | 6.00 | 24 | -18 | `entry_interface` onward: broad roar over low rumble | 2D bed |
| `sc_entry_roar_hi` | loop | 1 | 6.00 | 48 | -22 | the entry's peak: more crackle and hiss (cross-faded with the roar) | 2D bed |
| `sc_plasma_crackle` | one-shot | 4 | 0.27–0.34 | 48 | -29 | random crackles on the shell and the glass in `entry` | positional (portholes) |
| `sc_wind` | loop | 1 | 6.00 | 24 | -21 | thick air past the hull: from `drogue` till `main_chute` | 2D bed |
| `sc_drogue` | one-shot | 1 | 1.38 | 48 | -24 | `drogue`: mortar bang, the bag, the snatch at 0.56 s | 2D |
| `sc_chute_open` | one-shot | 1 | 1.75 | 48 | -24 | `main_chute`: canopy flutter, the crack at 0.62 s, lines groan | 2D |
| `sc_chute_creak` | loop | 1 | 6.00 | 24 | -23 | swinging under the main canopy (two 3 s swings in the loop) | 2D bed |
| `sc_jets` | one-shot | 1 | 1.55 | 48 | -23 | `jets`: the soft-landing rockets, 1.15 s burn | 2D |
| `sc_splash` | one-shot | 1 | 1.79 | 48 | -24 | `splash`: the hit at 0.02 s, spray, water running off | 2D |
| `sc_afloat` | loop | 1 | 6.00 | 24 | -23 | `afloat`: water lapping on the hull, heard inside | 2D bed |
| `sc_drip` | one-shot | 3 | 0.25–0.26 | 48 | -21 | drips afloat, every 2–9 s at random places (03 onto metal) | positional |
| `sc_needle_down` | one-shot | 2 | 0.56–0.57 | 48 | -22 | the arm set down. Start the record about 0.15 s later | record player |
| `sc_needle_up` | one-shot | 2 | 0.39 | 48 | -26 | the arm lifted (stop the record at its start) | record player |
| `sc_needle_skip` | one-shot | 2 | 0.35 | 48 | -25 | a jolt (thruster, `module_sep`, entry shake) while a record plays. Jump the music 1–3 s | record player |
| `sc_vinyl_crackle` | loop | 1 | 7.20 | 48 | -30 | surface noise under the music (4 turns of 1.8 s) | record player |
| `sc_platter_start` | one-shot | 1 | 1.60 | 48 | -18 | the motor running up when the player is switched on | record player |
| `sc_runout` | loop | 1 | 7.20 | 48 | -26 | the run-out groove when a side ends: a tick every 1.8 s | record player |
| `sc_thermos_open` | one-shot | 2 | 0.78–0.79 | 48 | -22 | the thermos unscrewed (cap pops at 0.42 s) | in hand |
| `sc_sip` | one-shot | 2 | 0.72–0.74 | 48 | -20 | a sip of tea | in hand / 2D |
| `sc_page` | one-shot | 2 | 0.56–0.64 | 48 | -27 | a page of the book turned | in hand / 2D |

Positional sources in a 2.6 m cabin: use a short attenuation (full volume within about 1 m, -6 dB at 3 m) or the
soundscape's `NearAttenuation`. For 2D beds, `AttachLoop` them to the cabin root and set
`AC->bAllowSpatialization = false` on the returned component, or keep them spatialised at the cabin centre. The
listener is always inside, so either works.

## Mix (volume multipliers on files normalised to -6 dBFS peak)

Effects bus at 1.0. Beds, with all of them on together in a quiet coast:

| bed | volume | note |
|---|---|---|
| `sc_cabin_hum` | 0.35 | the floor of the mix. Never off while powered |
| `sc_air_hiss` | 0.25 | |
| `sc_gyro_whine` | 0.12 | high and thin. It should be noticed only when it stops |
| `sc_fan` (each of 2) | 0.15 | positional, optional |
| `sc_beacon` / `sc_radio_static` | 0.3 x radio volume | |
| `sc_vinyl_crackle` | 0.18 x music volume | under the record. Duck it 30% when music is loud |
| `sc_runout` | 0.5 x music volume | |
| `sc_engine` | 0.9 (ascent), 0.6 (correction burn) | duck the cabin hum to 0.2 under it |
| `sc_afloat` | 0.5 | |

**Hand-overs that should not jump.** The files are peak-normalised, so the loop after a one-shot needs a matched gain.

- `sc_ignition` -> `sc_engine`: the last 0.6 s of ignition is 7.7 dB under the engine loop's RMS. Start
  `sc_engine` at **0.41 x the ignition's volume** at t = 2.7 s, cross-fade 0.3 s equal-power, then ease it to
  the ascent level over 2 s.
- `sc_gyro_spinup` -> `sc_gyro_whine`: start the loop at 3.8 s at **0.74 x the spin-up's volume**, cross-fade 0.2 s.
- `sc_engine` -> `sc_cutoff`: fire `sc_cutoff` and fade `sc_engine` out over 0.08 s. The cut-off file carries
  0.12 s of burn itself.

**Engine.** Volume = 0.9 x (0.6 + 0.4 x throttle). Pitch = 1 + 0.03 x (g - 1.6) (a stiffer burn sounds a hair
higher), clamped to 0.95..1.08. Add `sc_rattle` at 0.25 x g / 1.6 and a `sc_creak` every 3–8 s while g > 1.2.

**Entry** (plasma p = 0..1 from the model, g = felt load):
- `sc_entry_roar`: volume = 0.9 x smoothstep(0, 0.35, p) x (1 - 0.5 x smoothstep(0.55, 0.9, p))
- `sc_entry_roar_hi`: volume = 1.0 x smoothstep(0.45, 0.9, p). It is 3.6 dB quieter in RMS than the roar, so at
  the crossover (p of about 0.7) the two sum to an even loudness.
- both: pitch = 0.9 + 0.15 x p (the roar rises as it thickens). Low-pass the roar at 900 Hz + 3000 x p if there is a
  filter on the component (`SetLowPassFilterFrequency`).
- `sc_rattle`: 0.2 + 0.6 x clamp(g / 4.2, 0, 1). `sc_creak`: one every 0.6–2.5 s, faster with g, random variant
  and pitch 0.9..1.1.
- `sc_plasma_crackle`: Poisson rate = 6 x p^2 per second, volume 0.4 + 0.6 x p, pitch 0.85..1.2 at random, from a
  porthole.
- radio blackout: `sc_radio_static` to 0.4 if the radio is on, the beacon off.
- after the peak, as p falls: the same curves in reverse. Let `sc_wind` come in from `drogue`.

**Parachutes and splash.** On `drogue`: `sc_drogue`, then `sc_wind` at volume 0.8 x clamp(speed / 200 m/s, 0.3, 1),
pitch 0.85 + 0.25 x clamp(speed / 200, 0, 1). On `main_chute`: `sc_chute_open`, `sc_wind` fades to 0.2 over 4 s,
`sc_chute_creak` fades in at 0.6 over 2 s (pitch 1.0. If the model swings the cabin, volume 0.4 + 0.4 x |swing rate|).
On `jets`: `sc_jets`. On `splash`: `sc_splash`, stop the chute loop over 1 s, `sc_afloat` fades in over 1.5 s,
then `sc_drip` at random every 2–9 s for the first minute.

**Mortar, pyro, splash** are the loudest moments: play them at 1.0 and duck the beds (-6 dB, release over 1.5 s).
Do not let the beds and these sum past the limiter.

**Record player.** Platter on: `sc_platter_start`. Arm down: `sc_needle_down`, the music 0.15 s later with
`sc_vinyl_crackle`. End of side: stop the music and play `sc_runout` till the arm is lifted (`sc_needle_up`).
On a jolt (a thruster puff at 30% chance, `module_sep`, entry g over 2), play `sc_needle_skip` and jump the track.
Weightless (`coast`): the spring-loaded arm keeps playing.

## Events -> sounds (the flight model's names)

| event / phase | sounds |
|---|---|
| `parked` | hatch: `sc_wheel` + `sc_hatch_open` / `sc_hatch_close` + `sc_wheel` |
| `prelaunch` | `sc_seat`, `sc_belt`. Each switch: `sc_toggle` + `sc_relay` (+ its system: `power` -> `sc_cabin_hum` fades in over 1.5 s, `air` -> `sc_air_hiss`, `gyros` -> `sc_gyro_spinup`), lamps `sc_lamp_tick`, wrong order `sc_buzzer`, cover `sc_cover`, ПУСК `sc_button` + `sc_relay`_02 |
| `liftoff` | `sc_ignition` -> `sc_engine`, `sc_rattle`, `sc_creak` |
| `engine_cutoff` | `sc_cutoff`, rattle out over 0.5 s, `sc_chime` 01 |
| `correction_burn_on` / off | `sc_ignition` at 0.5 (or `sc_engine` faded in over 0.4 s) / `sc_cutoff` |
| attitude puffs | `sc_thruster` |
| `module_sep` | `sc_sep_bang`, a `sc_creak`, maybe `sc_needle_skip` |
| `entry_interface` | `sc_chime` 01, then the entry curves above |
| `drogue` | `sc_drogue`, `sc_wind` |
| `main_chute` | `sc_chute_open`, `sc_chute_creak` |
| `jets` | `sc_jets` |
| `splash` | `sc_splash`, `sc_afloat`, `sc_drip` |
| `afloat` | `sc_wheel` + `sc_hatch_open` (bring the real sea's surf up as the hatch opens) |
| thermos, book | `sc_thermos_open`, `sc_sip`, `sc_page` |

## Importing (on the Mac)

```
IMPORT_ONLY=sfx/spacecraft "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/unreal/Lyudmilovsk/Lyudmilovsk.uproject" -run=pythonscript -script=unreal/scripts/import_audio.py -unattended -nosplash
```

(Adjust the editor path to the local install.) This gives
`/Game/Audio/Spacecraft/S_sc_<id>_<nn>`, and the 15 loops get `looping = true` from their sidecars (`"loop": true`).
The 24 kHz files import as they are. Unreal resamples on playback.

## The C++ edit this needs (for the engine agent; not made here)

The soundscape finds pools only in the `Groups` table of `ULyudSoundscapeSubsystem::LoadSounds`
(`Private/Audio/LyudSoundscape.cpp`, the list that ends with the `Winter` group). Add, after the `Winter` lines:

```cpp
		// the flight home in «Ласточка» (ULyudSpaceflightSubsystem, tools/spacecraft/sounds.py, tools/spacecraft/SOUND_NOTES.md)
		{ TEXT("Spacecraft"), TEXT("sc_cabin_hum") }, { TEXT("Spacecraft"), TEXT("sc_fan") }, { TEXT("Spacecraft"), TEXT("sc_gyro_whine") },
		{ TEXT("Spacecraft"), TEXT("sc_gyro_spinup") }, { TEXT("Spacecraft"), TEXT("sc_air_hiss") }, { TEXT("Spacecraft"), TEXT("sc_radio_static") },
		{ TEXT("Spacecraft"), TEXT("sc_beacon") }, { TEXT("Spacecraft"), TEXT("sc_toggle") }, { TEXT("Spacecraft"), TEXT("sc_cover") },
		{ TEXT("Spacecraft"), TEXT("sc_button") }, { TEXT("Spacecraft"), TEXT("sc_relay") }, { TEXT("Spacecraft"), TEXT("sc_buzzer") },
		{ TEXT("Spacecraft"), TEXT("sc_chime") }, { TEXT("Spacecraft"), TEXT("sc_lamp_tick") }, { TEXT("Spacecraft"), TEXT("sc_hatch_close") },
		{ TEXT("Spacecraft"), TEXT("sc_hatch_open") }, { TEXT("Spacecraft"), TEXT("sc_wheel") }, { TEXT("Spacecraft"), TEXT("sc_belt") },
		{ TEXT("Spacecraft"), TEXT("sc_seat") }, { TEXT("Spacecraft"), TEXT("sc_ignition") }, { TEXT("Spacecraft"), TEXT("sc_engine") },
		{ TEXT("Spacecraft"), TEXT("sc_cutoff") }, { TEXT("Spacecraft"), TEXT("sc_thruster") }, { TEXT("Spacecraft"), TEXT("sc_sep_bang") },
		{ TEXT("Spacecraft"), TEXT("sc_creak") }, { TEXT("Spacecraft"), TEXT("sc_rattle") }, { TEXT("Spacecraft"), TEXT("sc_entry_roar") },
		{ TEXT("Spacecraft"), TEXT("sc_entry_roar_hi") }, { TEXT("Spacecraft"), TEXT("sc_plasma_crackle") }, { TEXT("Spacecraft"), TEXT("sc_wind") },
		{ TEXT("Spacecraft"), TEXT("sc_drogue") }, { TEXT("Spacecraft"), TEXT("sc_chute_open") }, { TEXT("Spacecraft"), TEXT("sc_chute_creak") },
		{ TEXT("Spacecraft"), TEXT("sc_jets") }, { TEXT("Spacecraft"), TEXT("sc_splash") }, { TEXT("Spacecraft"), TEXT("sc_afloat") },
		{ TEXT("Spacecraft"), TEXT("sc_drip") }, { TEXT("Spacecraft"), TEXT("sc_needle_down") }, { TEXT("Spacecraft"), TEXT("sc_needle_up") },
		{ TEXT("Spacecraft"), TEXT("sc_needle_skip") }, { TEXT("Spacecraft"), TEXT("sc_vinyl_crackle") }, { TEXT("Spacecraft"), TEXT("sc_platter_start") },
		{ TEXT("Spacecraft"), TEXT("sc_runout") }, { TEXT("Spacecraft"), TEXT("sc_thermos_open") }, { TEXT("Spacecraft"), TEXT("sc_sip") },
		{ TEXT("Spacecraft"), TEXT("sc_page") },
```

After that, `PlayEffect(TEXT("sc_toggle"), Where)` and `AttachLoop(TEXT("sc_engine"), Cabin)` work the way
`LyudEuropa.cpp` plays `hatch_open`. The pool loader already tries `_01`..`_12` and stops at the first missing one.

**Memory caveat.** `LoadSounds` loads every pool when the soundscape starts and keeps it (`CueRefs`). That makes the
whole set resident in every world, against the doc's rule that it loads only on Europa and is freed after the flight.
Compressed in the cook it is a few MB, so the simple edit above is acceptable at first. The clean version is a
small public pair on the soundscape that `ULyudSpaceflightSubsystem` calls from its Europa prefetch and after the
flight:

```cpp
// LyudSoundscape.h (public)
/** Loads /Game/Audio/<Group>/S_<id>_<nn> into the pool `Id` (lazy groups such as Spacecraft); idempotent. */
void LoadPool(const TCHAR* Group, FName Id);
/** Empties a pool and lets its sounds go (stop the loops that use it first). */
void UnloadPool(FName Id);
```

These are the same loop body as in `LoadSounds`, without adding the sounds to `CueRefs` (keep them in the
`FPool::Sounds` `TObjectPtr`s, which `Pools` already holds). The engine agent decides which one.

## Not verified here

- Nothing was auditioned by ear in this environment. Quality was checked by measurement (the table the script
  prints: centroid, 85% roll-off, crest, DC, clipping, peak, seam step percentile, seam high-frequency share) and by
  looking at spectrograms. Listen on the Mac before tuning the levels above.
- The import was not run (no Unreal here). The file naming, sidecars and loop flags follow `import_audio.py`.
- The mix numbers and crossfade laws are a starting point, worked out from the files' RMS. Not tried in the game.
