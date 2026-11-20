# The film camera and the album

Backlog item 2 (docs/interactives_backlog.md). Branch `film-camera`, 2026-11-20.

The hero buys a camera and 36-frame films at a photo studio on the promenade, raises the camera to the eye, focuses by
distance and shoots. Each frame is a real picture of the view, kept as a jpg. A film gives nothing until it is developed:
at the studio (paid, ready the next morning) or by the hero in the darkroom of the club «Шестерня». Prints go into the
album; sharp frames of the right subjects earn stamps, bird sightings and the views of the quest «Портрет города».

## Keys

| Key | What |
|---|---|
| V | raise or lower the camera (the pocket must hold a `camera`) |
| mouse wheel | the focus ring: 0.8, 1, 1.2, 1.5, 2, 3, 5, 10 m, infinity |
| left button | the shutter |
| R (camera raised) | wind the film back and take it out |
| P | the album; A / D turn pages; P, Esc or Q close |
| E | at the studio's darkroom door: hand films in, take prints; in the club's darkroom: start, then take the film out of each bath |

## How it plays

- **Buying.** The counter `photo_desk` of the three studios (W1-U08, W1-U20, W1-U32) has the shop `photo`: a camera 240,
  a film 18 (six a day).
- **Shooting.** With the camera up the view narrows to a 50 mm lens and bright frame lines show what the film takes. A
  film goes in by itself from the pocket. Sharpness comes from the focus ring against the subject's distance (depth of
  field in dioptres), the hero's own speed and the subject's speed: stand still, set the ring. The line under the frame
  reads the ring and the frame counter.
- **What the frame holds.** Birds within 25 m, Maru, people within 15 m, a fish caught in the last four game minutes with
  the rod in hand, the sunset of the current weather regime (lens toward a low sun, sunset look on), the aurora (lens
  up), a festival's running part at its venue, and the map places the hero stands in.
- **People.** Within 8 m a resident notices the lens: the warm wave, the nervous fold their arms, the curious put hands
  on hips, the rest nod (`UPeopleLifeSubsystem::ReactManner`).
- **The studio.** E at the studio's darkroom (`photo_lab`, the photographer must be on shift): ready prints come back,
  exposed films go in at 45 roubles each. Ready at the first 10:00 that is at least 12 hours away.
- **The hero's darkroom.** E at `P-CLUB3/darkroom_bench`: the red lamp, three baths (developer 12 s, stop 4 s, fixer
  8 s), E takes the film out of each. Within 10 % of the time is a perfect bath; the worst bath sets the film's quality;
  under 0.25 the film is ruined. A pale film prints grey in the album. Free and immediate, one film at a time.
- **The album.** Four prints a page with a handwritten date and place, newest page first.
- **Goals.** Collections filled by code (`"by": "code"` in tools/city/content/photo.json): `photo_birds` (39 species),
  `photo_sky` (the sunset of each of the 8 weather regimes, the aurora), `photo_life` (a fish, Maru, each festival). A
  sharp bird frame is also noted in the bird book (`NoteSeen`, how "camera").
- **Quest `city_portrait`.** Asked at a studio counter (topic `photo.city`): get a camera, photograph the club, the upper
  cable station, Academgorodok, the promenade and the beach, show the prints. Reward 60 roubles and two films. The event
  is `photo <place>`, sent for every place the album has a print of, at each development and each time the album opens,
  so views taken before the quest count once the album is opened.

## The contest

Backlog item 33, branch `photo-contest`. The photo club at «Шестерня» holds a contest every calendar month.

- **Theme.** By the month, going round: the sea (January, May, September), birds (February, June, October), people at
  work (March, July, November), the festival (April, August, December). The paper and the club's boards announce it.
- **Entering.** E at the contest box in the club (`"open": "photo_contest"`) on days 1 to 25 opens the album with the
  theme in the title; keys 1 to 4 hand in that print. One print a month.
- **The jury** sits on the 26th and marks a print out of 100: the subject against the theme, up to 50 (from the tags the
  shutter kept: `sea`, `bird:<Species>`, `work`, `festival:<id>`; a second fitting thing adds up to 10; off the theme
  gets 5), the light, up to 25 (the sun's height at the shot, tag `sun:<degrees>`: best at 0 to 10 degrees, poor at night
  unless the aurora is in the frame, a sunset adds 3), and sharpness, up to 25. Five other entrants have scores of 40 to
  85, the same in every game for a given month.
- **The result.** E at the box from the 26th: the place, and for places 1 to 3 a prize of 150, 80 or 40 roubles. Those
  prints hang on the club's wall (`"open": "photo_wall"`). The paper of the 27th reports places 1 to 3
  (`ULyudPhoto::ContestNews`).
- **New tags** written at the shot: `sun:<degrees>`, `work` (a resident in the frame busy with an activity), `sea` (the
  sight line reaches the sea's level with nothing in the way).
- **Quest events.** `do photo_contest_enter`, `do photo_contest_prize`, `do photo_contest_result`. The event `photo` now
  also carries `maru` and a resident's key for a sharp print of them, besides the map places.
- Rules: `Private/Photo/PhotoContestModel.h/.cpp`, checks in `tools/photosim/contest_test.cpp` (2190 checks, synthetic
  shots). Saved with the camera (key "Photo", field SavedContest).
- Console: `lyud.Photo.Contest`, `lyud.Photo.Wall`; the same two as developer actions.

## Files

- `Source/Lyudmilovsk/Private/Photo/PhotoModel.h/.cpp`: the rules in plain C++ (film, sharpness, studio clock, darkroom,
  album, stamps, the save line). Host suite `tools/photosim/run.sh` (514 checks; in tools/run_all_suites.sh).
- `Source/Lyudmilovsk/Public/Photo/LyudPhoto.h`, `Private/Photo/LyudPhoto.cpp`: `ULyudPhoto`, the world subsystem: keys,
  the viewfinder, what is in the frame, the capture, the studio and darkroom hooks (`ULyudCityThings::OnOpen`, screens
  `photo_lab` and `photo_darkroom`), the album, the save (key "Photo").
- `tools/photo/strings_src.py` -> `tools/photo/strings/ui.json` -> `python3 tools/photo/strings.py` ->
  `Content/Localization/photo/` (generated; `--check` in the suite).
- `tools/city/content/photo.json`, `strings/photo.json`: the act, collections and quest; regenerate with
  `python3 tools/city/interiors.py`. Items `camera`, `photo_film` in LyudInventory.cpp.
- Pictures: `Saved/Photos/<game id>/filmNNN_FF.jpg`, 720x480, black and white, blurred by the frame's unsharpness, with
  grain. Never in git.

## Cost

Camera down: one key poll per tick, no capture, no widget work (the overlay is collapsed). Camera up: an FOV lerp and a
leaf widget of nine boxes. A shot: one scene capture into a 720x480 target made at the first shot (never per frame), a
readback, a blur on the game thread and a jpg write; a hitch of one frame is expected and hidden by the shutter's dark.
The album loads four textures per page and frees them on closing.

## Developer mode

Actions (F7, group "photo"): give a camera, three films and 200 roubles; develop every film now; open the album.
Console: `lyud.Photo.Raise 0|1`, `lyud.Photo.Shoot`, `lyud.Photo.Develop`, `lyud.Photo.Album`, `lyud.Photo.State`.

## Where it departs from the brief

- The album is its own screen on P, not a diary tab: the diary's sections and the 3D book's five tabs are shared by
  several branches tonight.
- No camera model in the hands: the view is the finder itself.
- "Turn away" has no manner in PeopleManners; the nervous fold their arms.
- The studio counter is a quick exchange with toasts, not a dialogue page. The club darkroom's old lines
  (`city.cog.dark.*`) are not shown any more.
- Exposed films are not pocket items; the frame line and the studio toasts count them.
- The shutter uses the pool `camera_flash`, silent until the city sounds are imported.
- The bird book writes a camera sighting with the "hand" wording.

## Icons

Both items' icons are cells of one 2x2 Higgsfield grid (design/photo/icons_photo_1.png with its sidecar): the camera in
its case, the film box with a cassette. Diary sketches made by tools/ui_diary_icons.py.

## State

Builds on the Mac (2026-11-20, UE 5.8). Never run in the level yet.

## First run on the Mac (owed)

1. F7, photo: give. V, wheel, left button: `PHOTO frame` in the log, a jpg under Saved/Photos.
2. `lyud.Photo.Develop`, P: the album shows the prints, captions fit.
3. A studio on the promenade by day: buy, hand in, come back after 10:00 next day.
4. The club's darkroom: three baths, E not also firing the bench again (the 0.4 s guard).
5. Check: right mouse (binoculars) with the camera up; the left button with a rod or feed in hand.
6. Bench with the camera down and up against main, alone.
