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
- **each other** (`FManners::Passing`, twice a second): a glance at somebody passing, a wave or a nod between
  people who know each other (friends, a household).
- **greetings** (town talk): the warm greet the player nearly always, the reserved hardly ever; a greeting comes with a
  wave or a nod, and some who say nothing still nod.

## Weather in the choices

`FWorld::Rain` and `TempC` (from `ULyudWeatherSubsystem`) weigh on what people choose: outdoor seats and lookouts lose in
the rain and the frost, the water gains on a warm day, and a free outdoor activity is left when the rain starts
(activities with `"rain": "ok"` are not). New activities: `jog`, `play_run` (children), `feed_birds`, `read_bench`;
`"pace"` makes an activity's walk a run.

## Routines

- **Spots of their own** (`FWorld::PickSlot`, `FOptions::OwnSpots`, 110 m): out of the many benches, cafes and
  stretches of the promenade everybody has a few (about one in eight, by a hash of person and place: the same every
  game) they go out of their way for. So the same old man is on the same bench every morning, and a cafe has its
  regulars. Not at home, at work, at a holiday's gathering or in the club (its stations are people's own already).
- **A ritual** (`FManners::ApplyLikes` sets `FPerson::RitualAct/RitualHour/RitualMore`): one thing each grown-up does at
  their own hour every day, by who they are: a feeder's crumbs for the birds between half past eight and half past
  ten, the morning constitutional of those who walk with their hands behind the back, a reader's bench with the paper
  in the afternoon, a dozer's bench after lunch, the sweet tooth's coffee, a photographer's evening walk; the nervy jog
  at seven, the warm chat in the evening, the curious go into town before lunch, the tidy shop in the morning; everybody
  else takes an evening stroll. Within 45 minutes of the hour the activity scores up to 0.45 more; people at work then
  simply miss it. `lyud.People.Who <key>` prints it.

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
`ForceManner(Name)`; events `hero_runs`, `hero_close`, `hero_lamp` on `OnPersonEvent`.
Town talk sets: `manner = smoke phone_call ...` (what their hands are doing now).

## Testing

- `tools/mannersim/run.sh` (personas, the four weathers, noticing, passing, cost), `run.sh day`, `run.sh who`.
- `tools/peoplesim/run.sh test|town`, `tools/barksim/run.sh`.
- In the game: `lyud.People.Manner <manner|next|auto>` (everybody drawn does it), `lyud.People.Who <key>`;
  developer mode (F7), group "Townsfolk".
