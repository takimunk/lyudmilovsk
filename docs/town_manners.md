# The townsfolk's characters and manners

The people's life (docs/town_life.md, docs/people.md) says where somebody is and what they do. This layer says how
they do it: who they are in manner, the small thing their hands are doing now, what they notice of the player and
of each other, and the weather's say in what they choose.

Code: `Private/People/PeopleManners.{h,cpp}` (plain C++, runs in `tools/mannersim`), `Private/People/BodyGestures.{h,cpp}`
(the gestures), `UPeopleLifeSubsystem` (owns it, feeds it), `ANpcBody::SetManner` (shows it).
Data: `Content/PeopleData/manners.json`.

## Personas

Everybody has a persona worked out from their key (the same every game, nothing in the save):

- **warmth**: greets and waves .. keeps to themselves
- **nerve**: fidgets, looks at the watch .. calm
- **curiosity**: looks at what goes on .. minds their own business
- **tidy**: prepared (an umbrella with them) .. careless
- **habits**: smoker, phone, reader, feeder (crumbs for the birds), photographer, knitter, umbrella, sweet (an ice
  cream on a warm day), dozer, hands_back; and left-handed (the gesture and the prop go to the left hand)

Age, occupation and temper shift the odds (a pensioner feeds the birds and walks with the hands behind the back, a
student lives in the phone). Habits also draw people to what suits them: `FManners::ApplyLikes` adds to their likes once
(a feeder to `feed_birds`, a reader to `read_bench`).

Town talk knows them too: `temper = warm | reserved | nervy | calm | curious | tidy | careless` (a trait over 0.66 or
under 0.34; lines in `tools/barks/lines/12_tempers.txt`), so the same bench is talked about differently by different people.

`lyud.People.Who <key>` prints somebody's persona; `tools/mannersim/run.sh who` lists the town's.

## Manners

A manner is a row of `manners.json`: when it fits (pose, activity, place kind, habit, age, hours, outdoors, rain, sun,
temperature, a need, a holiday gathering, waiting for the train), how likely (weight, a trait), how long, its cooldown,
what it puts in the hands. The file's `_doc` lists every key.

- **force** rows are what somebody does while they fit: an umbrella in the rain (those who have one; the rest cover
  the head or hug themselves and hurry), the rod while fishing, the brush at the easel, the bag carried home from the shop.
- weighted rows are picked now and then against `idle_weight` (doing nothing in particular): folded arms, hands on
  the hips, a stretch, a yawn, a look at the watch (more of it waiting for the train), a look at the sky, a hand over the
  eyes against the low sun, cold hands rubbed, a phone call, the phone scrolled, a cigarette, a photograph, a newspaper
  on the bench, knitting, an ice cream, a cup, a doze on the bench after lunch, claps and swaying at a holiday.
- **react** rows are never picked: the game starts them (`ReactManner`): a wave, a nod.

Only people who have a body now are updated (a few dozen); between picks an update is a couple of comparisons
(~0.1 µs a person in mannersim). Nothing allocates after `Bind`.

## Noticing

- **the player** (`FManners::Notice`, 4 times a second for whoever is drawn): a glance when he comes near (the curious
  look longer; children turn round to watch), and events: `hero_runs` (he runs past close), `hero_close` (he stands
  within arm's reach for a few seconds), `hero_lamp` (the head lamp in their eyes at night). The events go out through
  `UPeopleLifeSubsystem::OnPersonEvent`; `ULyudTownTalkSubsystem` says a line for them now and then
  (`on = hero_runs` sets in `tools/barks/lines/10_manners.txt`).
  Also `hero_sneaks` (crouched, creeping up behind somebody's back: they start and turn round) and `hero_rides` (a
  bicycle tearing past).
  And what he is up to: `hero_fishes` (his line is out a few steps from them: the curious ask what's biting),
  `hero_swims` (he is in the water: seen from the shore and the promenade above, up to 32 m; other words in the cold),
  `hero_sits` (he sat down beside them on a bench).
- **they come to know him**: `UPeopleLifeSubsystem::NoteMet` counts the days somebody has greeted the player (kept
  with the save, `MetHero`); town talk's `met = new | seen | known` (1-4 days: seen; 5 and more: known) picks other
  greetings ("there you are, I hadn't seen you about"), and those who know him greet more readily and wave.
- **each other** (`FManners::Passing`, twice a second): a glance at somebody passing, a wave or a nod between
  people who know each other (friends, a household).
- **greetings** (town talk): the warm greet the player nearly always, the reserved hardly ever; a greeting comes with a
  wave or a nod, and some who say nothing still nod.

## Weather in the choices

`FWorld::Rain` and `TempC` (from `ULyudWeatherSubsystem`) weigh on what people choose: outdoor seats and lookouts lose in
the rain and the frost, the water gains on a warm day, and a free outdoor activity is left when the rain starts
(activities with `"rain": "ok"` are not). A fine evening (18:00-21:30, dry, over 12 C) draws walkers and those who stand and
look to the promenade, the pier, the fountain and the squares, the more the warmer (twice the mild evening's crowd at 23 C).
New activities: `jog`, `play_run` (children), `feed_birds`, `read_bench`;
`"pace"` makes an activity's walk a run.

## The sky's shows

Northern lights, a rainbow, fireworks: `FMannerNow::SkyShow` (0..1) says there is something to see up there, and the
rows with `"sky_show": 1` fit only then (the likelier the more of it): heads go back (`look_sky`, walking too), the
curious and the children point (`point_sky`), photographers lift the camera. A show also takes people away from what
their hands were at (not from a forced manner: the angler keeps the rod). `UPeopleLifeSubsystem::RefreshSkyShow`
reads `ALyudRareSky::GetNow()` once a second (the lights by night through the gaps in the clouds, a rainbow by day);
people get used to a long one (full for 2.5 minutes, down to 0.3 by the seventh). `ALyudFireworks::Fire` calls
`SetSkyShow("fireworks", ...)` with every shell. Town talk gets `weather = aurora | rainbow | fireworks` while it lasts
(sets `sky_*` in `10_manners.txt`). `lyud.People.SkyShow [seconds] [aurora|rainbow|fireworks]` stages one.

## At the counter

A customer on their feet and whoever serves them at the same place within 4 m (`LyudBarks::AtCounter`) are a pair for
town talk's exchanges, like two people chatting or sitting side by side: `tools/barks/lines/11_counters.txt` (sets with
`doing = shop shop_town` + `doing2 = work`, or the seller first). Works at shops and kiosks; the market's visitors
stand by the door, too far from the stalls, until its slots are laid out at them.

Thunder: `UPeopleLifeSubsystem::OnThunder` hears `ULyudWeatherSubsystem::OnLightning`; when the sound of a strike within
4 km reaches the camera, people out of doors start (shoulders up for a second or two; the nervy and the children
more) and one of them has a word (`on = thunder`).

## Routines

- **Spots of their own** (`FWorld::PickSlot`, `FOptions::OwnSpots`, 110 m): out of the many benches, cafes and
  stretches of the promenade everybody has a few (about one in eight, by a hash of person and place: the same every
  game) they go out of their way for. So the same old man is on the same bench every morning, and a cafe has its
  regulars. Not at home, at work, at a holiday's gathering or in the club (its stations are people's own already).
- **Company** (`PickSlot`): a bench or a spot out of doors, or a cafe's or a restaurant's table, where somebody they know
  already is draws them, to the seat next to the friend's
  (worth up to ~100 m of walking for the sociable), so friends end up on the same bench.
- **A ritual** (`FManners::ApplyLikes` sets `FPerson::RitualAct/RitualHour/RitualMore`): one thing each grown-up does at
  their own hour every day, by who they are: a feeder's crumbs for the birds between half past eight and half past
  ten, the morning constitutional of those who walk with their hands behind the back, a reader's bench with the paper
  in the afternoon, a dozer's bench after lunch, the sweet tooth's coffee, a photographer's evening walk; the nervy jog
  at seven, the warm chat in the evening, the curious go into town before lunch, the tidy shop in the morning; everybody
  else takes an evening stroll. Within 45 minutes of the hour the activity scores up to 0.45 more; people at work then
  simply miss it. `lyud.People.Who <key>` prints it.

- **The household's weekend outing** (`ApplyLikes` sets `FPerson::OutingAct/OutingDay/OutingHour/OutingPlace`): every
  household of two and more has one, the same every game: on Saturday or Sunday at their hour they all go to the same
  thing at the same place (one of the four nearest home that have it and room for them all): the sea front, a stroll, the fair, crumbs for
  the birds; a table at the cafe or the restaurant for those without small children. From half an hour before the hour
  to an hour and a quarter after (`FWorld::OutingNow`) it scores 0.7 more, people leave what they chose for it
  (`Interrupt`), take a slot at that place and stay till the others have been there a while. Those at work miss it.
  The day and the hour are the ones that keep the fewest of them at work. In mannersim 173 of the 262 households have two
  and more of theirs there together (161 whole). Those who walk (a
  stroll, the fair) keep together: `FWorld::Wander` sends everybody after the first of the household at it (the lowest
  id), to the free spot nearest theirs, a few seconds behind (83% of the time within 40 m). Town talk:
  `doing = outing` (sets `outing`, `outing_kid`). `tools/mannersim/run.sh outing` prints a weekend.
  Setting out: whoever leaves for the outing goes on foot (no bicycle), those of theirs within 25 m look up within a few
  seconds and follow, and on the way (`FWorld::WayTogether`) the one in front dawdles, the one behind steps out, and side
  by side they go at the slowest's walk. Most households are scattered about town before their hour, so this shows
  mostly with those who leave home together.
  Going home: when the first of them gets up from the outing to go home, those of theirs still at it within 60 m get up
  within a few seconds and go along to the same thing at home (`FPerson::HomeWith`, `Think`: "home with theirs"; not
  those with a duty), on foot and keeping together the same way: in mannersim about a third of the way home from an
  outing is walked with one of their own within 10 m.

- **A weekly date with a friend** (`ApplyLikes` sets `FPerson::DateAct/DateDay/DateHour/DatePlace/DateWith`): everybody
  is paired with the first friend of theirs still free (376 pairs), and one weekday a week at their hour the two meet at
  the same thing at a place between their homes that has room: a talk, a bench, the cafe or a stroll after work; the
  old in the daytime (a bench, crumbs for the birds); children at the playground after school. It works as the outing
  does (`FWorld::MeetNow`: 1 the household's outing, 2 a date), and on a stroll the two keep together. In mannersim's
  Wednesday 41 of the 73 pairs whose day it is meet (the day and hour are picked so that both are off work) (the rest: work, the culture club, supper). Whoever comes second takes the seat next to the one already there (`PickSlot`: 39 of the 43 side by side). Town talk:
  `doing = date` (sets `date`, `date_kid`, the exchanges `date_talk`). `tools/mannersim/run.sh dates`.
- **A word with their own** (`LyudBarks::WithTheirOwn`): two of a household at their outing, two friends at their date, a
  grown-up and the child they mind, within 4 m, are a pair for town talk's exchanges (sets `own_talk`, `own_talk_child`,
  `date_talk`, `mind_talk`, `mind_talk_child`).
- **Keeping an eye on the children** (activity `mind_child`, `FWorld::ChildToMind`): while a child under 12 of theirs is
  out at play (`playground`, `play_run`) within 450 m and nobody of the household is by, one grown-up leaves what they
  chose, comes to a spot by the child and stands turned to wherever the child runs; they go when the child does. Those
  at work or asleep don't. In mannersim's weekend 55% of the child-minutes at play have a grown-up of theirs within 80 m
  (most of the rest is the walk there). Town talk: `doing = mind_child`.

## The gestures

No new clips. A manner's gesture is a few goals for the hands, the head and the back worked out from the time since it
began (`LyudGesture::Eval`): a goal is a point from the pelvis, the chest or the head, the way the fingers point, what
the palm faces and how far the fingers close. `ULyudBodyAnimInstance::SetGesture` eases toward it, and the animation
worker reaches it with the arms on top of the clip (`LyudGesture::Apply`: two-bone IK, the hand turned, the fingers
curled). So a gesture goes with any pose and any body, melts into the next one, and a left-hander does it with the left
hand. Standing with folded arms uses the `FoldArms` clip (`LyudGesture::ClipFor`).

Props are static meshes `/Game/Characters/Humans/Props/SM_Prop_<name>` (phone, cigarette, camera, umbrella, rod,
brush, binoculars, newspaper, book, knitting, ice_cream, cup, bag, and easel), made by `blender/humans/props.py` and
imported by `unreal/scripts/import_props.py`. A manner whose prop's mesh is missing is never picked. How each sits in
the hands is `LyudGesture::HoldOf`:

| hold | props | the mesh |
| --- | --- | --- |
| Upright | umbrella, ice_cream, cup | origin at the grip, +Z up |
| Hanging | bag | origin at the handles, hangs along -Z |
| Palm | phone | long axis X, the screen +Z |
| Fingers | cigarette, brush | along +X from the fingers |
| TwoHands | camera, binoculars, newspaper, book, knitting | length X, the face the holder looks at +Z |
| Pointing | rod | along +X from the grip |

## API

`UPeopleLifeSubsystem`: `GetManner(Id)`, `GetTrait(Id, "warmth")`, `ReactManner(Id, "wave", Seconds)`,
`ForceManner(Name)`, `SetSkyShow(What, Strength, Seconds)`, `GetSkyShow()`; events `hero_runs`, `hero_close`, `hero_lamp`, `hero_sneaks`, `hero_rides`, `hero_fishes`, `hero_swims`,
`hero_sits` on `OnPersonEvent`.
Town talk sets: `manner = smoke phone_call ...` (what their hands are doing now).

## Testing

- `tools/mannersim/run.sh` (personas, the four weathers, noticing, passing, a weekend's outings, the sky's shows, cost),
  `run.sh day`, `run.sh outing`, `run.sh dates`, `run.sh who`.
- `tools/peoplesim/run.sh test|town`, `tools/barksim/run.sh`.
- In the game: `lyud.People.Manner <manner|next|auto>` (everybody drawn does it), `lyud.People.Who <key>`,
  `lyud.People.SkyShow`; developer mode (F7), group "Townsfolk".
