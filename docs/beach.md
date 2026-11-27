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
* Once he has a record, the post's prompt shows his own under the beach's, as if chalked below the sign: best skips, best
  swim, wins against the summer visitors.

## Summer visitors at the buoy

* From 15 June to 31 August, with the sea at 16 °C or warmer (`RivalSeason`), up to three summer visitors swim with him
  (`TodaysRivals`: six known swimmers, each comes on a given day with a chance of 55%, the same ones all day). The whistle
  line names them: Zoya Petrovna in the white cap, Gena the student, Lida, Arkady Semyonovich, Tomas from Tallinn, Nina.
* Each has a top speed and a fade (`FRival`: 1.30 to 1.86 m/s; the fast young ones tire, the old ones keep going). Their
  place along the course is worked out from the clock (`RivalAlong`: the speed fades as they go), not simulated.
* Their caps (white, yellow, blue spheres) swim lanes 3 m apart beside him, out on one side of the buoy and back on the
  other. A cap hides in the shallows and when its swimmer is in.
* At the end he gets his place (`RacePlace`) among them. First: the stamp `swim_first`, the event `beach swim_first`, a
  win on his line under the sign (`SwimWins`). The fastest visitor beats a fair swim; to win he needs a good one.
* These are visitors, not townsfolk: they are not in the people's life. The townsfolk on the sand talk about them (barks
  `beach_race_talk`).

## Children at the castle

* The town's children (ages 4 to 12) who are up and about within 14 m of his castle come and help while he builds
  (`CountHelpers`, up to two). Each adds half his speed (`Build(..., Helpers)`, `HelperBoost`). The first time a line
  says so; now and then one of them says a line over their head (barks `beach_castle_kid`, via `SayLine`).
* A castle built with help gives the stamp `castle_kids` and the event `beach castle_kids` when it is finished.
* Town life has its own castles: the activity `beach_castle` (children 4 to 11, on the beach, 10:00 to 19:00, in daylight,
  17 °C and warmer). Their parents watching them say `beach_castle_mind`. Kites on windy days were already there
  (`fly_kite`, docs/kites.md), and flat stones on the stony shore (`skip_stones`).
* The paper flag: the news kiosk sells `sand_flag` (3 roubles). If he has one when the castle is finished, it goes on top
  instead of the splinter and the sweet wrapper: a taller stick, a bigger pennant, a line, the event
  `beach castle_paper_flag`.

## Selling finds

* **Y** at a souvenir counter (the things with the act `buy_souvenirs`, within 3.2 m), from 10:00 to 20:00: the seller buys
  all his shells and amber at once (`SellPrice`: a shell 4 roubles, amber 30, amber with a fly 150). The prompt shows what
  she would pay and only appears when nothing else on the beach wants the prompt.
* Until "The amber shore" is done, three pieces of amber stay in his pocket (`KeepForQuest`), since she wants to see them.
* A line (amber or shells), the coins sound `rest_coins`, the stamp `sold`, the event `beach sold`. The book keeps the
  roubles earned (`SoldRoubles`).
* The amber with a fly (`amber_fly`) is now an item of its own, not a plain piece of amber. It still counts for the quest's
  event `beach amber`.

## Stamps, quest, words

Collection `beach` «Пляжные дела», 19 pieces, given by code: `skip3` `skip5` `skip8` `skip_record` `castle` `castle_flag`
`castle_washed` `swim` `swim_gold` `shell_cockle` `shell_mussel` `shell_tellin` `shell_whelk` `amber` `amber_fly` `amber3`
`castle_kids` `swim_first` `sold`.
Quest events `beach <what>`: `skip3`, `skip5`, `skip_record`, `castle`, `castle_washed`, `swim`, `swim_gold`, `shell`,
`amber`, `castle_kids`, `castle_paper_flag`, `swim_first`, `sold`.

Quest `amber_shore` «Янтарный берег» (`tools/city/content/x_beach.json`): the souvenir seller on the promenade tells
where amber comes from (topic `souvenir.shore`), three shells, three pieces of amber after a storm, show them
(`souvenir.shore_bring`, offered with the stamp `beach/amber3`): 60 roubles and an amber charm.

Words: `tools/city/content/strings/beach.json`; the four items (`seashell`, `amber_piece`, `amber_fly`, `sand_flag`) in the
main tables; the townsfolk's lines in `tools/barks/lines/18_beach_play.txt`.

## Console and developer mode

`lyud.Beach.Info`, `lyud.Beach.Stone [flatness]`, `lyud.Beach.Throw [quality]`, `lyud.Beach.Castle [work 0..6]`,
`lyud.Beach.Race`, `lyud.Beach.Storm <0..1|-1>`, `lyud.Beach.Reset`. With them: `lyud.TideOverride <m>` and
`lyud.TideSpeed <n>` (docs/tides.md) to watch a castle go. F7, Actions, group `beach`: stones, a castle, a storm's
waterline, the whistle, a pocket of finds (`beach.finds`: 6 shells, 5 amber, an amber with a fly, a paper flag; for the
souvenir counter and the flag on the castle). `lyud.Beach.Info` also shows the children and helpers, today's visitors,
the counters, his wins and roubles.

## Checks

`tools/beachsim/run.sh test`: 1229 checks (the stones and the count of skips, the castle through a whole tide, the swim's
clock and medals, the finds of a kilometre over ten days, the book; helpers at the castle, the summer visitors' season,
speeds, places and the same field all day; the sale prices and the items finds become).

## Performance

Four times a second: where the hero stands (ten depth lookups). Away from the sea nothing else runs. Per frame only
while it shows: the stone in the air (one mesh), the buoys within 250 m (two meshes each), the swing bar, the visitors' three caps during a swim. The children near a castle are looked at four times a second, only
while he builds (the list of children is read once). The souvenir counters are read once from the town's things. The shapes
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

* The four icons (`icon_seashell`, `icon_amber_piece`, `icon_amber_fly`, `icon_sand_flag`) are pinned in
  `localization/assets.json` and not generated.
* Not yet seen in the engine (2026-11-27): the caps on the water, the helpers' lines, the paper flag's size, the sell prompt
  at the promenade's souvenir counter. To check on the Mac: compile, `lyud.Beach.Race` in July with the sea warm, a castle
  with children around, `beach.finds` then Y at the souvenir counter.
* The visitors are caps, not swimmers with bodies.
* The town record is on the swim post's sign, not on the towers' own boards.
