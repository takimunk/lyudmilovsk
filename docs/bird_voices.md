# Bird voices that mean something

Interactives backlog item 45. The birds of the bird life (docs/bird_life.md) call by species; now what they say follows
what they are doing. The model is plain C++ in `Private/Birds/BirdVoice.cpp` (part of the bird life model, declared in
`BirdSim.h`), so it runs in `tools/birdsim/run.sh test` without the engine. The game plays the right call for the
voice and shows what it means in the bird guide.

## The voices

Every `Call` and `Song` event of the model carries a voice (`LyudBirds::EVoice`, `FEvent::Voice`):

| voice | who, when | what it means |
|---|---|---|
| `contact` | any bird awake; a little more often in company, so a flock is a murmur | here I am, where are you |
| `song` | a singer sitting on a perch off the ground (never on the ground, never in flight), in its breeding months | this patch is mine |
| `alarm` | a bird that scolds (tits, blackbird, robin, sparrows, swallow, wagtail, starling, crows, jay, magpie) sees a cat within 18 m from its twig (a creeping cat only within 5 m) or a hawk within 30 m | a hunter is here |
| `beg` | fledglings out of the nest (docs/bird_nests.md) | feed me |
| `chatter` | starlings, sparrows, jackdaws, rooks, sand martins gathering before the roost and before leaving it | the roost's talk |
| `long_call` | gulls on the beach and the roofs from sunrise for three hours | this shore is mine |

**The dawn chorus.** Each singer starts at its own minutes from that day's sunrise, in the order field guides give for
northern Europe: robin 60 min before, blackbird 50, song thrush 40, woodpigeon 30, great tit 25, blue tit 20, swallow
15, chaffinch 10, goldfinch and bullfinch 5, wagtail and starling at sunrise, the sparrows 5 min after, town pigeons 10
after. Full song lasts 1 h 45 min from the start, a quarter of it until four hours after sunrise, a trickle through the
day, and the robin, the blackbird and the song thrush sing again round sunset. Song seasons are per species (great tit
from 15 January, chaffinch from 1 March, most stop in July); the robin alone sings in autumn and winter, more quietly.
The table is `VoiceDetail::Rows()` in BirdVoice.cpp. Males on a nest's song post keep their own song (BirdNest.cpp).

**Weather.** Rain quiets everything (a drizzle a little, 2.5 mm/h and more nearly all, and nobody sings); so does the
midday heat (from 24 C between 11:30 and 16:00, most at 30 C). The game fills `FWorld::Rain` and `TempC` from the
weather every 5 s.

**The alarm spreads.** A scolding bird calls every second or two for as long as it sees the danger. Each call is taken
up by scolders within 24 m after 0.4 to 1.6 s, who scold for 4 to 7 s and look toward the danger; it passes through at
most three birds and never further than 60 m from the danger itself, and a bird that has said its piece does not take
the same alarm in again for 20 s. So a garden knows about the cat and the next street does not, and the hero can walk
toward where the scolding is thickest to find it. An alarm call alone does not flush anybody.

## The voice budget

Only birds near the camera have a voice at all (LOD tiers 0 and 1, and nothing beyond 1.3 x `FWorld::HearDist`, 70 m:
their timers are only asked again every few seconds). Of those, at most `FWorld::MaxVoices` (6) sound at once. A voice
takes a place for as long as it lasts (a contact call 0.6 s, a song 2.4 s, a long call 3 s); the more places are taken,
the nearer or louder the next voice must be to get one (reach = earshot x free share ^ 0.8), so the last place goes to a
bird a few steps away. Voices that get no place are not events at all: nothing is played and nothing is queued.
`FWorld::Say` is the one door; `VoicesSaid`, `VoicesDropped` and `VoicesPeak` count what went through it.

## In the game

- `UBirdLifeSubsystem::PlayCall` plays the voice's own call when the species has one for it (species.json `"voices"`:
  `{"song": "song_fluty", "alarm": "chink_alarm"}`), else any of its calls; an alarm is a little louder and higher,
  begging thin and high, chatter quieter with a wide pitch spread.
- `OnBirdVoice(BirdId, Voice)` for whoever listens (quests, the book); `LastVoiceOf(Species, Voice)` is what a species
  near the hero last said.
- The bird guide (B) shows a line under each species near the hero: what it last said in the past 12 s and what that
  means (`ui.guide.voice.<voice>` in ru/en/ja).
- Developer mode: Actions tab, "Bird voices" group, "Alarm from the nearest bird" (`lyud.Birds.Alarm`): the nearest
  perched scolder gives a 12 s alarm and its neighbours take it up.

## Checks

`BIRD_ONLY=voices sh tools/birdsim/run.sh test` (22 checks, a few seconds; also part of the whole test): the dawn
order by sunrise on eleven days from March to June, who sings in late October and on 20 January, perch only, rain,
heat, dusk; no alarm without a cat, the first tit within 3 s, everybody within 36 m within 8 s, nobody from 84 m on,
alarm calls on average under 35 m from the cat, quiet again 40 s after it leaves; a flock's murmur, the starlings'
roost, the gulls' morning; the budget held in a minute of a hundred noisy sparrows with the near ones heard first,
nothing at all from a flock 140 m away, a sleeping flock silent at night.

## Not done here

New synthesized calls (a begging note, starling chatter, a gull's proper long call, a tit's scold) need
`tools/birdsim/bird_calls.py` renders and an import on the Mac; until then those voices use the species' existing calls
with the level and pitch changes above. Nothing was changed in the soundscape's mix.
