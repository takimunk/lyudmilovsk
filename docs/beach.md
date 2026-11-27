# The beach at play

Small things to do where the sea meets the sand, each a minute long, all on the real sea: its level (the tide), its wet
line, its waves (`ALyudSea`). Rules: `LyudBeach` (`Private/Sea/BeachModel.h/.cpp`, plain C++), engine side `ULyudBeach`
(`Public/Sea/LyudBeach.h`), the save `ULyudBeachBook` (record "Beach"). Nothing is placed in the level: the beach is
wherever the hero stands on his feet, hands empty, on sand near the sea's level with the sea within 25 m.

## Skipping stones

* **E, a tap**, on the sand: he bends over the shingle and looks at a stone. About three in ten are flat (`StoneFlatness`:
  a spot of 3 m gives the same stones all day, seven of them, then it is picked over). A flat one goes into the pocket, up
  to five; each keeps how flat it is.
* **G held**, water in front of him: the arm swings (a bar with a marker, forward and back, 0.9 s each way); **letting go**
  throws the flattest stone. The best moment is with the arm almost forward (`BestSwing` 0.8).
* The flight is worked out at once (`Throw`): the speed from the release, how much of it each touch keeps from the
  flatness and the release, less in waves, which can also trip it. The engine plays it back along the real surface
  (`SampleSurface` under the stone every frame), a splash and the count on every touch. Waves are measured where it lands
  (the surface against the still level), not in the open sea.
* `tools/beachsim/run.sh throws` prints the table. A fair throw of a fair stone: 3 to 7; a perfect disc thrown perfectly on
  a calm sea: over twenty; in half-metre waves hardly five. The town record on the sign is 11 (`TownRecord`).

## The sandcastle

* **E held** on damp sand (`CanBuild`: from a hand over the water to 45 cm over the hour's wet line): a castle a step in
  front of him, one at a time. Six stages of 4 s: a heap, a keep, four towers, walls, a moat, a flag. Letting go and
  coming back goes on where it stopped.
* The tide takes it (`Wash`): once the water's run-up (the level plus 40% of the wave height) reaches its foot it wears,
  gone in ten minutes of licking or under a minute of standing water; a moat takes the first 6 cm; rain alone melts it in
  three hours. Building on mends the wear. The hours the hero slept or the game was closed are made up from the tide
  (`LyudTide::At`, ten minutes at a time).
* The finished castle says when the tide will be there (`HoursUntilWash`).
* Drawn from the engine's basic shapes on one root, squashed as it wears; touched only when a stage or a tenth of the wear
  changes.

## Shells and amber

* The waterline is cut into 8 m cells along the shore (Y); a cell holds at most one find a day (`FindIn`: the same all
  day, another tomorrow). Shells always (a calm kilometre has one every 60 to 100 m), several times more after a storm,
  and amber only after one, at the top of the wet sand; one piece in twelve has a fly in it.
* The storm is the strongest wind of the day and a half before the morning (`StormScore` from
  `ULyudWeatherSubsystem::GetStateAt`: nothing under 9 m/s, a whole storm from 17 m/s), worked out once a day.
* The engine lays out the cells within 50 m of the hero where the sand drops to the find's height (`ALyudSea::GroundZ`
  along the line, then a trace), again only when he walks into another cell, the sea moves 15 cm or he picks one up.
* **E** picks it up: the item `seashell` or `amber_piece`, a line, the first of a kind a diary note and a stamp.

## The buoy swim

* Below each lifeguard tower (the things with the act `lg_flagstate`) stands a post with a sign, and off it a red buoy
  where the highest water is 2.5 m deep, ten metres farther. Both are placed once, when the town's things are loaded.
* **E** at the post: the whistle. The clock starts when he swims, the buoy is touched within 3.5 m, the clock stops when
  he stands on the bottom again (`RaceStep`). Gold from 1.35 m/s over the course, silver 1.15, bronze 0.9; the
  lifeguards' time is the course at 1.5 m/s and can just be beaten (the hero swims 1.6).
* At low water the buoy lies on the sand: no race, come back with the tide.

## Stamps, quest, words

Collection `beach` «Пляжные дела», 16 pieces, given by code: `skip3` `skip5` `skip8` `skip_record` `castle` `castle_flag`
`castle_washed` `swim` `swim_gold` `shell_cockle` `shell_mussel` `shell_tellin` `shell_whelk` `amber` `amber_fly` `amber3`.
Quest events `beach <what>`: `skip3`, `skip5`, `skip_record`, `castle`, `castle_washed`, `swim`, `swim_gold`, `shell`,
`amber`.

Quest `amber_shore` «Янтарный берег» (`tools/city/content/x_beach.json`): the souvenir seller on the promenade tells
where amber comes from (topic `souvenir.shore`), three shells, three pieces of amber after a storm, show them
(`souvenir.shore_bring`, offered with the stamp `beach/amber3`): 60 roubles and an amber charm.

Words: `tools/city/content/strings/beach.json`; the two items in the main tables.

## Console and developer mode

`lyud.Beach.Info`, `lyud.Beach.Stone [flatness]`, `lyud.Beach.Throw [quality]`, `lyud.Beach.Castle [work 0..6]`,
`lyud.Beach.Race`, `lyud.Beach.Storm <0..1|-1>`, `lyud.Beach.Reset`. With them: `lyud.TideOverride <m>` and
`lyud.TideSpeed <n>` (docs/tides.md) to watch a castle go. F7, Actions, group `beach`: stones, a castle, a storm's
waterline, the whistle.

## Checks

`tools/beachsim/run.sh test`: 90 checks (the stones and the count of skips, the castle through a whole tide, the swim's
clock and medals, the finds of a kilometre over ten days, the book).

## Performance

Four times a second: where the hero stands (ten depth lookups). Away from the sea nothing else runs. Per frame only
while it shows: the stone in the air (one mesh), the buoys within 250 m (two meshes each), the swing bar. The shapes
are kept out of Lumen's scene and the distance fields, made once, hidden when not needed.

## Checking it on the Mac

`unreal/scripts/shoot_beach.sh` (UE queue, offscreen, `shoot_beach.py`): the hero on the waterline by the first lifeguard
tower (`BEACH_X`/`BEACH_Y`, the ground 3 to 12 cm over the water: `BEACH_LO`/`BEACH_HI`), hands emptied with
`Inventory.Equip none` (a new game starts with the bird feed in hand, and the beach wants them empty), `lyud.Beach.Info`,
a stone thrown, a castle built and the tide pinned over it, a storm's waterline walked cell by cell, the post and the buoy.

First Mac run (2026-11-21): sand 1 damp 1 water ahead 1 on the waterline; the throw 13 skips, 24 m; "castle washed
away"; finds 3, then 8 to 9 with `lyud.Beach.Storm 1`, 1.5 cm over the sand; two posts with buoys 170 m out (this beach
is very flat: about 1.3 cm a metre). Fixed: the moat was a near-black disc 1.7 m across, wider than the castle; now a ring
of wet sand 1.15 m across, and the castle's colours are lifted to meet the beach's sand.

Noted, not changed: 25 m off the waterline the sea is too shallow for "the sea within 25 m" (over 10 cm deep), so on this
flat beach the damp sand higher up (where castles are allowed) reads as not the beach. A piece of amber (3.5 cm) is
hard to see from standing height.

## Not done yet

* The two icons (`icon_seashell`, `icon_amber_piece`) are pinned in `localization/assets.json` and not generated.
* Selling shells and amber at the souvenir counter (the shops only sell): the quest pays for the first three.
* Children joining at a castle, residents racing to the buoy in summer, a flag bought at the kiosk (the flag is a
  splinter and a sweet wrapper).
* The town record is on the swim post's sign, not on the towers' own boards.
