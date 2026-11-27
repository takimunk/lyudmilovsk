# Yacht school (яхтенная школа)

Dima's ask (2026-11-27): a sailing school where schoolchildren and adults train on small training dinghies (the Soviet
«Луч» and the like): sail physics on the game's real wind, a helm the player works, lessons, races, animations, a daily
timetable. It opens in spring when it is warm (late April / May) and closes in November.

This file is the **contract** between the parts while they are written in parallel; each part's owner replaces its
section with the real description when done. Branch `yacht-school`.

## The school

- **ДЮСШ парусного спорта «Луч»** (children's and youth sailing school "Luch"; ja ヨット学校「ルーチ」). A boathouse
  (эллинг) on the beach by the SOUTH side of the pier's root (the banya is on the north side), a boat park with dinghies
  on launching trolleys, a slip of boards over the sand, a signal mast with flags, a windsock, a coach's launch.
- **Boat classes** (ids are fixed, used in code, data, mesh names):

  | id | Boat | Crew | LOA | Beam | Sail | Hull |
  |---|---|---|---|---|---|---|
  | `optimist` | «Оптимист», the children's pram, sprit sail | 1 child 7-12 | 2.30 m | 1.13 m | 3.3 m² | 35 kg |
  | `cadet` | «Кадет», two children, mainsail + jib | 2, ages 10-16 | 3.22 m | 1.27 m | 5.2 m² | 54 kg |
  | `luch` | «Луч», the single-hander (teens, adults, the hero) | 1 | 4.23 m | 1.37 m | 7.06 m² | 59 kg |

  The coach's launch `launch` is an aluminium motor boat («Казанка»-like, 4.6 m); race marks are orange inflatable
  cylinders `mark`; the start line is the launch (with a flag staff) and a pin buoy `pin`.
- **Season**: opening day = the first Saturday on or after April 25 (opening regatta and parade of sail, festival id
  `sail_opening`); closing = the first Sunday of November (closing regatta `sail_closing`, then the boats are laid up).
  Out of season the boats stand in the boathouse under covers, the coach is in on some days (theory, knots, repairs).
- **A day's go / no-go** (the signal mast shows it): daylight; mean wind 1.5-8 m/s for `optimist`, to 10 for `cadet`,
  to 12 for `luch`; no thunderstorm, no dense fog; sea ice gone. Too much wind = shore lesson (knots, theory at the
  blackboard, rigging on the grass). Tide: boats are rolled on their trolleys to the water's edge wherever the tide has
  it; sailing needs 0.5 m of water (centreboard half up under 1.0 m).
- **Groups** (real townsfolk; ids fixed): `kids` (optimists, ages 7-11), `juniors` (cadets and luches, 12-17),
  `adults` (luches, 18+), each with a coach; `race` = everyone on race days.

## Parts and owners

| Part | Files (only the owner edits them) |
|---|---|
| A. Sail physics, helmsman autopilot | `Source/Lyudmilovsk/Private/Sailing/SailModel.{h,cpp}`, `tools/sailsim/` |
| B. School rules: season, timetable, lessons, races, scoring, save; generator and data | `Private/Sailing/SailSchoolModel.{h,cpp}`, `tools/sailschoolsim/`, `tools/sailing/` (content.json, sailing.py), `Content/City/sailing.json` |
| C. Meshes | `blender/sailing/`, `unreal/scripts/import_sailing.py`, previews |
| D. Body clips and props | `blender/humans/` additions for sailing, `docs/yacht_school.md` "Clips" |
| E. Engine | `Public/Sailing/*.h`, `Private/Sailing/Lyud*.cpp` |
| F. People | activities, spots, members (idempotent add-on, never reroll the town) |
| G. Words, quests, stamps, items, shop, paper/radio, festival, dev mode, sounds | data + `tools/sailing/strings*.json` |

Rules for every part: plain-C++ models have NO Unreal types (namespace `LyudSail`), build with
`-Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Werror` under clang, are deterministic, and have a host test
`tools/<sim>/run.sh` (pattern: tools/kitesim, tools/winterplaysim). Frame: Unreal world, metres in the models, north =
+Y, the sea lies toward +X from the town's coast (compass: east = -X). The wind comes ONLY from
`LyudWeather::Regime/GustAt/HeightFactor` (model) / `ULyudWindSubsystem::GetWindAt(Location, HeightM, bOverWater=true)`
(engine): another thread is upgrading the wind tonight behind those calls, never edit Weather/ files. Waves and depth
from `ALyudSea::SampleSurface / DepthAt / SeaLevel`. Perf rules: docs/benchmark.md and the checklist (no per-frame mesh
rebuilds, moving meshes out of Lumen, pooled, nothing ticking far from the hero or out of season). Text: the
localization skill (ru/en/ja). Items: game-items skill. Dev mode: dev-mode skill.

## Shared names

- Namespace `LyudSail`; boat class enum `EBoat { Optimist, Cadet, Luch }`, ids as above.
- Engine: `ULyudSailing` (world subsystem, `Public/Sailing/LyudSailing.h`), `ALyudDinghy` (`Public/Sailing/LyudDinghy.h`),
  save object `ULyudSailingBook` ("Sailing"). Console `lyud.Sail.*`, dev group `sailing`.
- People conditions set by the engine (`UPeopleLifeSubsystem::SetCondition`): `sail_kids`, `sail_juniors`,
  `sail_adults`, `sail_race`, `sail_shore` (a shore lesson instead), `sail_season`.
- Place kind `sail_school`, plan-like ids `S-SAIL-SCHOOL` (boathouse), `S-SAIL-PARK` (boat park), `S-SAIL-SLIP`.
- Strings table `Content/Localization/sailing/strings_{ru,en,ja}.json`, keys `sail.*`; HUD/prompt keys `ui.sail.*`.
- Stamps collection `sailing`, quest ids `sail_*`, quest event `sail <what>`.
- Items: `sail_gloves`, `life_jacket` (worn), `sail_card` (the school's membership book), `knot_cord`.
- Meshes `/Game/Sailing/SM_Sail_<Name>`; sounds group `Sailing`, `audio/sfx/sailing/`.

## Clips (part D)

People's body clips for the school and the props in their hands, all procedural (poser.py IK on the clips' body Guy_a,
like the house's own clips), baked to `/Game/Characters/Humans/Anims/A_Human_Sail_*` by retarget_clips.py.

**Files.** `blender/humans/sail_clips.py` (entry, bake, `sail_grips.json`), `sail_core.py` (boat numbers, mirrored
posing, grips, checks), `sail_clips_boat.py` (afloat), `sail_clips_shore.py` (shore, class, coach), `sail_preview.py`
(contact sheets), `standin_rig.py` (a 56-bone SK_Human_Skeleton stand-in for checking in the cloud, never baked for the
game), `sail_grips.json` (generated: every clip's hand targets in UE cm and the boat numbers it was posed with). Props in
`blender/humans/props.py` (`SAIL_MAKERS`). Contact sheets (side, front, top at 5 moments, clay body, stand-in boat drawn
from the same numbers, the tiller/extension/sheets drawn from the clip's own hand targets):
`/mnt/project-files/yacht-school/clips/Sail_<Clip>.png`; the props and the jacket worn: `Sail_Props.png`.

**Boat numbers.** Part C's `blender/sailing/sockets.json` is read when present (`sail_core.boat_from_sockets`): seat,
strap, mainsheet block, tiller pivot and length, extension, boom, mast, crew station, beam, deck height. The clips were
checked against the sockets.json of `yacht-school-meshes` 7ee3d9bc. When the boats change, re-run the bake (Mac steps).

### Frames and origins

Clip space is the usual body space (UE: the mesh faces +Y, +X its left, +Z up; Blender: faces -Y). In place, root at 0.

| frame | origin of the clip | how the engine places the body |
|---|---|---|
| boat (`Sail_Helm` ... `Sail_Climb_In`) | on the boat's centreline abreast the helm's seat (`helm_port.pos.x`), on the floor (`helm_port.feet.z` less the ankle's 8 cm x the class's body scale). `sail_grips.json` `boats.<class>.origin_in_boat` = (x, 0, z) m in the boat frame | attach the body at `origin_in_boat` (boat frame; UE cm = (x, -y, z) x 100), facing the bow (mesh +Y = boat +X: actor yaw = boat yaw - 90, as for any human mesh) |
| capsized (`Sail_Swim_ToBoard`, `Sail_Right`) | where the centreboard leaves the hull, on the water's surface; the hull lies on its side toward the clip's -X, the board sticks out along +X | attach to the capsized hull at the board's root, clip +X along the board |
| shore | on the ground between the feet, like every life clip | the actor on the ground as usual |
| seat (`Sail_Knots_Sit`, `Sail_Listen_Class`) | under the house chair's seat centre (seat top 47 cm, pelvis UE (0, 1, 56.7)), as the Sit clips | as a seated life pose |
| seat (`Sail_Coach_Drive`) | under the launch's aft thwart's centre (thwart top 40 cm), the outboard's tiller behind on the right | attach to the launch's helm seat |

**Port / Stbd.** Every boat clip comes as `_Port` (the body on the port side deck, i.e. starboard tack: tiller in the
right hand, sheet in the left) and `_Stbd` (its mirror image, posed mirrored, not bone-flipped). `Sail_Tack_Port` and
`Sail_Gybe_Port` start as `Sail_Helm_Port` and end as `Sail_Helm_Stbd` (and the other way round).

**Children.** A child is the same skeleton at a smaller BodyScale S. The optimist (posed for S 0.74) and the cadet (0.86,
helm and crew) clips are posed for a grown-up in the boat scaled by 1/S, so a child of that S fits the real boat
exactly. For a child of another S' raise the attach point by `deck_z * (1 - S'/S)` (deck_z in `sail_grips.json`) so the
seat stays on the deck; the hands' targets come from the boat anyway (below). The luch clips are for a grown-up (S 1); a
teenager in a luch takes the same offset with S = 1.

### The clips

45 clips: 15 boat clips as Port/Stbd pairs (30), and 15 shore ones. 30 fps.

| clip | s | kind | when |
|---|---|---|---|
| `Sail_Helm_Port/Stbd` | 8.0 | loop | luch helm on the windward deck: tiller and sheet corrections, looks at the luff, ahead, over the shoulder. Hike 0 |
| `Sail_Hike_Half_Port/Stbd` | 8.0 | loop | hiking with bent knees, the seat on the gunwale's edge. Hike 0.5 |
| `Sail_Hike_Full_Port/Stbd` | 8.0 | loop | full hike, legs nearly straight under the strap, hips over the water. Hike 1 |
| `Sail_Cadet_Helm_Port/Stbd` | 8.0 | loop | cadet helm (a teenager), centre mainsheet |
| `Sail_Tack_Port/Stbd` | 2.2 | one-shot | tack facing forward: tiller away, duck under the boom, cross, sit, hands swapped behind the back |
| `Sail_Gybe_Port/Stbd` | 1.6 | one-shot | gybe: quicker, lower duck, the sheet hand grabs the falls and throws the boom over |
| `Sail_Lull_Port/Stbd` | 8.0 | loop | very light air: kneeling in the cockpit's middle, leaning to leeward |
| `Sail_Opti_Helm_Port/Stbd` | 7.0 | loop | optimist child on the side deck, the sheet from the boom's block |
| `Sail_Opti_Bail_Port/Stbd` | 4.0 | loop | optimist child kneeling, bailing (prop `bailer` in the right hand), the extension held low |
| `Sail_Crew_Jib_Port/Stbd` | 6.0 | loop | cadet crew on the windward side trimming the jib sheet |
| `Sail_Crew_Tack_Port/Stbd` | 2.4 | one-shot | cadet crew tacking: old sheet off, under the boom, the new side, the new sheet |
| `Sail_Capsize_Fall_Port/Stbd` | 1.5 | one-shot | the boat goes over to leeward, the helm falls in; ends treading water at clip (1.25, -0.1, -0.95) m (water -0.05). Play it while the hull rolls (the legs pass where the deck was) |
| `Sail_Swim_ToBoard_Port/Stbd` | 3.0 | one-shot | capsized frame: from treading water to standing on the centreboard, hands to the up gunwale; ends as `Sail_Right` |
| `Sail_Right_Port/Stbd` | 3.0 | loop | on the board, leaning back in pulses until the boat comes up |
| `Sail_Climb_In_Port/Stbd` | 3.2 | one-shot | from the water by the side: haul, chest over, a leg in, roll into the cockpit, kneel up; then cross-fade to `Sail_Helm` |
| `Sail_Rig_Mast` | 5.0 | loop | rigging: the sail's sleeve threaded up the stepped mast hand over hand (mast 0.36 m ahead) |
| `Sail_Rig_Knot` | 4.0 | loop | crouched at the boat on its trolley, tying off ~0.5 m up |
| `Sail_Trolley_Push` | 1.25 | loop | walk in place at 0.65 m/s pushing the trolley's bow bar (0.78 m up, 0.5 m ahead). Rate = speed / 0.65 |
| `Sail_Launch_Hold` | 6.0 | loop | thigh-deep (water 0.6 m) holding the boat head to wind by the bow |
| `Sail_Hose` | 6.0 | loop | rinsing the hull: the nozzle (prop `hose_nozzle`) in the right hand, the hose in the left |
| `Sail_Carry_Mast` | 1.1 | loop | walk in place at 1.0 m/s, the mast on the right shoulder. Rate = speed / 1.0 |
| `Sail_Knots_Sit` | 6.0 | loop | the class at desks tying a knot (prop `knot_cord`, both hands) |
| `Sail_Listen_Class` | 10.0 | loop | the class at desks listening, following the board (3.5 m ahead), a nod |
| `Sail_Coach_Point` | 6.0 | loop | the coach pointing with the right arm out to sea / at the board |
| `Sail_Coach_Megaphone` | 5.0 | loop | the coach calling through the megaphone (prop `megaphone`, right hand), the left hand on the hip |
| `Sail_Coach_Drive` | 8.0 | loop | the coach on the launch's thwart steering the outboard behind on the right, looking round the fleet |
| `Sail_Whistle` | 3.0 | loop | the coach's whistle (prop `whistle`, right hand), the left arm up |
| `Sail_Flag_Hoist` | 2.4 | loop | a flag hoisted on the signal mast's halyard (0.33 m ahead), hand over hand |
| `Sail_Watch_Sea` | 8.0 | loop | a hand shading the eyes, the sea scanned slowly |
| `Sail_Cheer` | 3.0 | loop | the finish: fists up, a bounce, clapping |

Not made (TODO): a full-hike clip for the cadet's crew (Crew_Jib hikes moderately), the hero's first-person hands, a
wave from the boat at the parade of sail (the `wave` manner's gesture over `Sail_Helm` will do).

### Props

`/Game/Characters/Humans/Props/SM_Prop_<name>` (props.py; `EProp` names in `PeopleManners.h`, `HoldOf` in BodyGestures):

| prop | hold | notes |
|---|---|---|
| `megaphone` | Palm | the grip across the fist, the cone along the fingers |
| `whistle` | Fingers | chromed, the lanyard hanging from its ring |
| `bailer` | Palm | the optimist's scoop cut from a bottle |
| `knot_cord` | TwoHands | ~0.4 m of braided cord between the hands |
| `hose_nozzle` | Pointing | a brass pistol nozzle, the hose going back and down |
| `stopwatch` | Palm | the face out of the palm, the crown along the fingers |
| `start_flag` | Upright | a blue flag with a white square on a 0.74 m stick |
| `clipboard` | Palm | the race sheet |
| `life_jacket` | worn | below |

**The life jacket** (orange foam blocks on a cloth vest, a collar roll, white ties; Soviet style). Its origin is the
`spine_03` joint of a grown-up (Guy_a's size) and it is modelled in the body's rest frame (mesh space). Attach a static
mesh component to the body's skeletal mesh at bone `spine_03` with relative location 0, relative rotation
`RefPoseComponentSpace(spine_03).GetRotation().Inverse()` (so the jacket keeps the mesh's rest orientation over the
bone) and relative scale `BodyScale` (ANpcBody's; the hero's 1): a child's mesh is the same skeleton made smaller, not a
scaled component. One mesh fits every size that way (`Sail_Props.png` shows it on a grown-up and on a 0.74 child).

### How the engine plays them

- **Load and play.** `LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Humans/Anims/A_Human_Sail_Helm_Port.A_Human_Sail_Helm_Port"))`,
  then on the body's `ULyudBodyAnimInstance` (People/LyudBodyAnim.h): `PlayClip(Seq, bLoop, Rate, BlendTime, StartTime)`.
  One clip plays at a time with a cross-fade (0.3 s by default). While a resident is in a boat the life must not drive
  their body (ANpcBody picks a pose clip on its updates, and its `Play`/`Clip` are private): hand the body over to the
  sailing code, or play after ANpcBody's update. Call `SetOnFeet(false)` in the boat (no ground planting).
- **The hike** has no blendspace: `Sail_Helm` (0), `Sail_Hike_Half` (0.5) and `Sail_Hike_Full` (1) share one 8 s loop
  phase, so cross-fade between them keeping the phase: `PlayClip(Next, true, 1.f, 0.6f, CurrentClipPhase() * 8.f)`.
  Pick the nearest of the three to the autopilot's hike value; the hand IK below follows the real tiller and sheet.
- **The hands** (tiller extension, mainsheet, jib sheet): every frame `ULyudBodyAnimInstance::SetBikeRider(FBikeRiderPose)`
  (Public/Bikes/LyudBikeRider.h) with `Weight = 1`, `PelvisWeight = 0`, `TorsoPitch = 0`, `bAutoLean = false`,
  `HeadUp = 0`, both `Feet[i].Weight = 0`, and for each hand `Hands[i] = { Grip, Axis, Radius, PalmNormal, FingerDir,
  Weight = 1, Close = 1 }` (0 left, 1 right) in the mesh's component space (cm). LyudBikeRider::ApplyArms bends the arm
  by two-bone IK, turns the hand onto the bar and closes the fingers round it. The tiller hand: Grip on the extension at
  the dagger grip (part C's `hand_tiller_ext`; the cadet's `hand_tiller`), Axis along the extension, Radius 1.2 cm. The
  sheet hand: on the sheet (Radius 0.5) between its block and the clip's hand. `sail_grips.json`
  `clips.<Clip>.hands.<role>` holds each role's `hand` (l/r), `grip_cm`, `axis`, `palm`, `fingers` and `radius_cm` at the
  clip's first frame (UE, clip space): the rest values, and a check of the IK. Ease Weight to 0 for the hand swaps in
  `Sail_Tack`/`Sail_Gybe`/`Sail_Crew_Tack` (the clip carries the hands across) and for the capsize set.
- **The hero** runs HeroAnimInstance, which cannot play arbitrary clips: in the boat give the hero's mesh a
  ULyudBodyAnimInstance (same skeleton) while sailing, or add a clip slot to HeroAnimInstance (part E).
- **Manners** (residents at the school; PeopleManners): `sail_rig`, `sail_knot`, `sail_hose`, `sail_knots` (seated),
  `sail_listen` (seated), `sail_point`, `sail_megaphone`, `sail_whistle`, `sail_flag`, `sail_watch`, `sail_cheer`: rows in
  `Content/PeopleData/manners.json` at place kind `sail_school`. Each plays its own clip through `LyudGesture::ClipFor`
  (standing, or seated where `LyudGesture::ClipSeated`), its prop in the right hand. Part F may narrow the rows to its
  activities (`doing`).

### Mac steps

```sh
# 1. pose and bake the clips on Guy_a (reads blender/sailing/sockets.json; writes Saved/ClipSources/sail_clips.glb,
#    blender/humans/sail_grips.json, Saved/ClipSources/sail_clips_report.json); add -- --preview DIR for sheets
Blender -b -P blender/humans/sail_clips.py
# 2. carry them onto the clips' body (clips.json source "sail"); no --only = every clip
Blender -b -P blender/humans/retarget_clips.py -- --only <the list below>
# 3. import Saved/LifeAnims/life_anims.glb (now only these) as /Game/Characters/Humans/Anims/A_Human_Sail_*
#    (LIFE_ONLY=<Name> only when step 2 made a single clip)
UnrealEditor-Cmd unreal/Lyudmilovsk/Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_life_anims.py -unattended -nullrhi
# 4. the props
Blender -b -P blender/humans/props.py -- --only megaphone,whistle,bailer,knot_cord,hose_nozzle,stopwatch,start_flag,clipboard,life_jacket
unreal/scripts/import_props.sh megaphone whistle bailer knot_cord hose_nozzle stopwatch start_flag clipboard life_jacket
```

The list: `Sail_Helm_Port,Sail_Helm_Stbd,Sail_Hike_Half_Port,Sail_Hike_Half_Stbd,Sail_Hike_Full_Port,Sail_Hike_Full_Stbd,Sail_Cadet_Helm_Port,Sail_Cadet_Helm_Stbd,Sail_Tack_Port,Sail_Tack_Stbd,Sail_Gybe_Port,Sail_Gybe_Stbd,Sail_Lull_Port,Sail_Lull_Stbd,Sail_Opti_Helm_Port,Sail_Opti_Helm_Stbd,Sail_Opti_Bail_Port,Sail_Opti_Bail_Stbd,Sail_Crew_Jib_Port,Sail_Crew_Jib_Stbd,Sail_Crew_Tack_Port,Sail_Crew_Tack_Stbd,Sail_Capsize_Fall_Port,Sail_Capsize_Fall_Stbd,Sail_Swim_ToBoard_Port,Sail_Swim_ToBoard_Stbd,Sail_Right_Port,Sail_Right_Stbd,Sail_Climb_In_Port,Sail_Climb_In_Stbd,Sail_Rig_Mast,Sail_Rig_Knot,Sail_Trolley_Push,Sail_Launch_Hold,Sail_Hose,Sail_Carry_Mast,Sail_Knots_Sit,Sail_Listen_Class,Sail_Coach_Point,Sail_Coach_Megaphone,Sail_Coach_Drive,Sail_Whistle,Sail_Flag_Hoist,Sail_Watch_Sea,Sail_Cheer`.

Checked in the cloud on the stand-in only: before importing, look over the Mac bake's own sheets (step 1 with
`--preview`), since Guy_a's proportions differ a little from the stand-in's. Known contacts the checker reports (cm of a
limb's skin into the boat): thighs on the deck's edge 3-9 (hike, tack, the cadet's seats, the kneeling lull and
bailing), the capsize fall's legs where the deck was (the hull rolls away in the game), the climb-in's knees on the side
deck.
