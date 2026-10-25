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
| Thunder: crack when near, rolling rumble after distance / 343 m/s | `ULyudWeatherSubsystem::OnLightning` |
| Indoors: all of the above through walls (low-pass 18 kHz -> 280 Hz, -20 dB); under water nearly gone | `ALyudSky::IsPlayerOutdoors`, eased over ~1 s; `ALyudSea::IsCameraUnderwater` |
| Cats and dogs: bark (by breed), growl, whine, meow, hiss, purr where the animal is | `UAnimalLifeSubsystem::OnAnimalEvent`; puppies and kittens pitched up; max 4 a second, none past 120 m |
| Far one-shots placed 300 m off in their direction: ship horn, foghorn (in mist), bell buoy (waves), dogs across the valley at night; the town's murmur by day | Poisson clocks per kind, by distance to the sea, fog, waves, the sun |
| The computer club: room tone where its people are; typing, clicking, old consoles at the people | humans' `ASkeletalMeshActor`s (`/Characters/Humans/`) found once |

Volumes for the settings menu: `SetVolumes(Master, Ambience, Music, Effects)`.

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
`lyud.Music.Mood <name>`, `lyud.Music.Gap <scale>`, `lyud.Sound.Debug 1`. Weather: `lyud.Weather.*`
(docs/sea_and_wind.md, "Clouds and rain").

## Making and checking

- `tools/audiosim/run.sh test` builds the DSP with clang and checks it in seconds (levels, silence, monotony,
  thunder delay, muffling); `run.sh render [scene]` writes `Saved/AudioSim/<scene>.wav`;
  `tools/audiosim/spectrogram.py <wav>` (Blender's python, numpy) draws a spectrogram PNG. Tune the voices there.
- `tools/gen_sounds.py --list | --sfx | --music [ids]` (FAL_API_KEY; Blender's python) generates what
  `audio/sounds.json` asks for and rewrites `audio/SOURCES.md`. Costs: ElevenLabs SFX $0.002/s, Stable Audio $0.20 a cue.
- `UnrealEditor-Cmd <uproject> -run=pythonscript -script=unreal/scripts/import_audio.py` imports them.
- `unreal/scripts/test_audio.sh` records the game's own mix at spots and weathers to `Saved/AudioTest/*.wav`
  offscreen and silently (`au.LinearGainScalarForFinalOutut=0` mutes the speakers after the recorded mix).
