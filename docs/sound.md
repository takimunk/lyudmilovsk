# Sound

What the world sounds like and the music. Three kinds of sound, all CC BY 4.0 (`audio/SOURCES.md` lists every one):

1. **Procedural**: synthesised live from the world's state, in code (`Private/Audio/AmbienceDSP.cpp`, plain C++).
2. **Generated**: recorded-style sounds and music cues made once with pinned models (`audio/sounds.json`,
   `tools/gen_sounds.py`), committed as WAV with a sidecar per file, imported to `/Game/Audio`.
3. **Older self-made**: vent hum/air (`tools/gen_vent_*.py`), bird calls (`tools/birdsim/bird_calls.py`), voices
   (`tools/gen_voice.py`).

## The soundscape (`ULyudSoundscapeSubsystem`, `Public/Audio/LyudSoundscape.h`)

Starts by itself in every game world and spawns a transient holder actor with:

| Part | Driven by |
|---|---|
| Wind at the ear: rumble, whoosh, gust swells, aeolian whistle (f = 0.2 U/d) over ~8 m/s | `ULyudWindSubsystem::GetWindAt` at 2 m, sheltered 35% among trees; pan from where it blows; muted while the player is a bird in the air or on the ground (not in the camera's fly-in/out) |
| Leaves: grains of bright noise, rate ~ wind squared, crisper when dry; pine needle hiss | trees within ~35 m (`AProceduralForest::ForEachTree` + `AProceduralPlant`, 20 m grid built once), aspens/poplars louder; season: leaves from May, dry from September, mostly down by mid-November |
| Surf: each wave breaking (crash then wash), in sets, at the sea's period; calm-water slaps; the roar of the shore from afar; air absorption with distance | `ALyudSea::DepthAt` rings around the listener find the nearest water deeper than 20 cm (1 Hz); `WaveHeight()` x 0.8 breakers, period 2 + 3.6 sqrt(Hs) |
| Rain: bed + drops (ticks, puddle plips, leaf patter), roof drumming; snow nearly silent | `ULyudWeatherSubsystem::GetSky()`; an upward trace (10 m) for a roof |
| Field crickets, rate by Dolbear's law | summer (mid-June .. mid-September) evenings and nights, over 11-15 C |
| Thunder: a train of claps when near, rolling rumble after distance / 343 m/s | `ULyudWeatherSubsystem::OnLightning`; the storm comes and goes with its episode (below) |
| The weather's own voices: hail, umbrella, windows, blizzard, snow hush, drips, iced twigs, sea ice, fog signal, footsteps | `ULyudWeatherSubsystem::GetModelState()`, see "Weather" below |
| Indoors: all of the above through walls (low-pass 18 kHz -> 280 Hz, -20 dB); under water nearly gone | `ALyudSky::IsPlayerOutdoors`, eased over ~1 s; `ALyudSea::IsCameraUnderwater` |
| Cats and dogs: bark (by breed), growl, whine, meow, hiss, purr where the animal is | `UAnimalLifeSubsystem::OnAnimalEvent`; puppies and kittens pitched up; max 4 a second, none past 120 m |
| Far one-shots placed 300 m off in their direction: ship horn, foghorn (in mist), bell buoy (waves), dogs across the valley at night; the town's murmur by day | Poisson clocks per kind, by distance to the sea, fog, waves, the sun |
| The computer club: room tone where its people are; typing, clicking, old consoles at the people | humans' `ASkeletalMeshActor`s (`/Characters/Humans/`) found once |

Volumes for the settings menu: `SetVolumes(Master, Ambience, Music, Effects)`.

## Weather (`Private/Audio/WeatherVoices.h`, `AmbienceWeather.cpp`)

Everything here is synthesised in code from the weather engine's state (`docs/weather.md`), seeded, so the same weather
always sounds the same and nothing has to be generated or paid for. `ULyudSoundscapeSubsystem::UpdateWeatherVoices`
turns the state into the bed's inputs.

| Sound | What makes it | Driven by |
|---|---|---|
| Drizzle | fine drops: a soft high mist of a hiss, hardly any single ticks | `Precip` Drizzle / FreezingDrizzle |
| Rain, showers | the bed + drops as before; showers come in waves (the cloud's gusts) | `PrecipRate`, `Precip` Showers/Thunderstorm/..., `ShowerCells` |
| Hail, graupel | hard knocks, a few big stones among pellets, lower and louder on a roof | `Hail` when Hail/Graupel/Thunderstorm |
| Umbrella | every drop on taut cloth a "pock" over the head (~66 drops/s/m2 per mm/h), the trickle off the rim; snow only pats | `SetUmbrella(0..1)` (equipment), `lyud.Sound.Umbrella` |
| Window glass | indoors, rain ticking on the panes, more when the wind drives it (after the walls, not muffled) | `SetNearWindows(0..1)` (a place that has windows), `lyud.Sound.Windows` |
| Blizzard | snow driven by the wind hisses on everything (4-9 kHz, breathing with the gusts); a gale lifts fresh snow off the ground too | snowfall x wind over ~5 m/s; fresh lying snow in a gale |
| Snow hush | fresh soft snow swallows the highs of everything around (-65% over 2 kHz, -30% level at most); fog dulls the far things the same way, less | `SnowDepthCm`, `SnowDensity` (fluff hushes, old crust doesn't), `Fog` |
| Drips | the trees drip after rain and in fog (patter on leaves, plips in puddles); not when frozen | `Wet` once the rain stops, `Fog`, trees around |
| Thaw | eaves and icicles each dripping on their own beat (into a puddle, on a tin sill, on snow), quicker the faster it melts | `Melt` with snow lying |
| Iced twigs | after freezing rain, clusters of glassy clinks when the wind moves the branches | `Glaze` x trees x wind |
| Sea ice | "singing" chirps (flexural waves are dispersive: highs arrive first, so a crack is a whistle falling in pitch, longer the farther), booms of thick ice cracking, echoes round the floes; more as the air cools at night; floes grinding (stick-slip creaks) and brash clinking in a swell when the sea is half frozen | `SeaIceCover`, `SeaIceCm`, the air's cooling (C per game hour), `ALyudSea::WaveHeight`, from the shore |
| Fog signal | the lighthouse's diaphone: a deep reed tone ending in its grunt, two blasts a minute, darker with distance, echoed by the coast | `VisibilityM` under ~2 km; the Mayak station (`ULyudTransportSubsystem`, "mayak") is the lighthouse; ships' horns in fog stay the generated `foghorn_far` |
| Footsteps | the player's strides outdoors (0.7 m walking .. 1.1 m running): snow crunching (a dense crackle of breaking crystals + the weight's thud), a soft "pff" in fresh fluff, a squeak in cold snow under -8 C (two ringing friction pitches), slush squelching, puddles splashing with bubbles, ice clicking and the sole's scrape, wet and dry earth | `SnowDepthCm`, `SnowDensity`, `TempC`, `Melt`, `Wet` (puddles over ~0.5), `Glaze`, standing on sea ice |
| Thunder's storm | strikes scatter round a centre that is ~11 km off as a thundery episode starts and ends and ~1 km in its middle | `RegimeProgress` in `ULyudWeatherSubsystem::Tick` |

Not yet: snow depth is the engine's level depth (the winter thread's drifts and paths will give the local depth
underfoot); no umbrella item calls `SetUmbrella` and no place calls `SetNearWindows` yet; footsteps indoors.

## Music

Ambient cues (`/Game/Audio/Music/MU_<mood>_<n>`, Stable Audio 2.5, seeded, all in or around D minor / D major / F major
so moods meet without clashing). The mood: `flight` (a bird), `club` (indoors), `storm`, `rain`/`snow`, `night`
(`snow` in winter), `dawn`/`dusk` (sun under 10 degrees), `sea` (within 60 m of the surf), `day`. A cue plays once,
then a silence of 50-160 s (15-40 s in the air); a big change of place (into the club, into the air, a storm) fades
the cue out and the next fitting one comes after a breath. Cues aren't repeated within the last four; a mood without a
fresh cue borrows from its neighbours (`MoodFallbacks`).

## Wind for anything else: `ULyudWindAudioComponent` (`Public/Audio/LyudWindAudio.h`)

The same wind voice as a component: `SetWind(WindSpeed, Gustiness, YawToWindDeg, AirSpeed)` every frame. The bird's
flight uses it for the wind round its head (airspeed rush and buffeting).

## Console

`lyud.Sound 0` silence (A/B), `lyud.Sound.Bed 0` no procedural bed, `lyud.Music 0`, `lyud.Music.Next`,
`lyud.Music.Mood <name>`, `lyud.Music.Gap <scale>`, `lyud.Sound.Debug 1`, `lyud.Sound.Steps 0` no footsteps,
`lyud.Sound.Umbrella 0..1`, `lyud.Sound.Windows 0..1`, `lyud.Sound.Force "Hail=0.8 Melt=1 Glaze=1 Drip=1 IceRate=-1.5
Puddles=1 FogSignal=1"` (tests: forces those inputs). Weather: `lyud.Weather.*`
(docs/sea_and_wind.md, "Clouds and rain").

## Making and checking

- `tools/audiosim/run.sh test` builds the DSP with clang and checks it in seconds (levels, silence, monotony,
  thunder delay, muffling); `run.sh render [scene]` writes `Saved/AudioSim/<scene>.wav`;
  `tools/audiosim/spectrogram.py <wav>` (Blender's python, numpy) draws a spectrogram PNG. Tune the voices there.
- `tools/gen_sounds.py --list | --sfx | --music [ids]` (FAL_API_KEY; Blender's python) generates what
  `audio/sounds.json` asks for and rewrites `audio/SOURCES.md`. Costs: ElevenLabs SFX $0.002/s, Stable Audio $0.20 a cue.
- `UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_audio.py` imports them.
- `tools/audiosim/run.sh render drizzle_then_rain umbrella hail_roof blizzard snow_hush fog_lighthouse thaw
  glaze_twigs sea_ice_night ice_floes footsteps thunder_approach ...` renders the weather voices; `run.sh test` checks them.
- `unreal/scripts/test_weather_sound.sh` (WSOUNDTEST_ONLY=a,b) records the weather presets in the game to
  `Saved/WeatherSound/*.wav`, walking for the footsteps.
- `unreal/scripts/test_audio.sh` records the game's own mix at spots and weathers to `Saved/AudioTest/*.wav`
  offscreen and silently (`au.LinearGainScalarForFinalOutut=0` mutes the speakers after the recorded mix).
