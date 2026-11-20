# Winter play

Interactives backlog item 18. Skating on the park pond, the banya by the pier and the walruses' ice hole, snowball fights
with the town's children, snowmen. Builds on `docs/winter.md` (snow, sea ice, the first-snow festival) and the park pond
of `docs/city/public_spaces.md`.

## Where the hero meets it

- **The rink.** The park pond freezes on the calendar (Dec 15 to Mar 15, as `ALyudPublicSpaces` draws it; `lyud.City.Ice`
  forces it). Its thickness comes from the weather's freezing degree-days (Stefan's law past an offset): from 10 cm the
  rink is open, under 20 cm its cracks are many and wide (9, then 3, the same set all winter). A day of thaw puts water on
  it and closes it until the frost dries it. With **skates** on (worn on the feet; 350 rub. at the department store's
  sports counter) the hero glides: a push every stroke while a move key is held, a long glide when it is let go (30 to 60 m
  from full speed), turns held by the edge's grip, **S** against the way is a hockey stop. A crack crossed fast or a hard
  turn at speed throws him: down for 1.5 s, up in 1.1 s (the view tips). In boots the ice is slippery; in skates on land
  he totters at 42 % speed.
- **The banya** stands on the sand under the seawall by the pier's root: a log cabin built in code, a dressing room and a
  steam room with a stove, its stones and two shelves; heated Wed, Fri, Sat, Sun 14-22, New Year's Eve 10-18 and New
  Year's Day 9-15 (the window glows after dark). At the door **E** pays 20 rub. and seats him on the upper shelf, **T**
  opens the keeper's shop (venik 10, felt hat 30, towel 35; veniks and carrots also at the market, the hat and towel at
  the haberdashery). Inside: **E** pours a ladle (the room's heat and steam rise), **G** swishes the venik (once a visit),
  **Space** goes out. His heat rises by how hot it feels (the air plus the steam's bite, the felt hat takes 9 degrees off):
  about three minutes on a plain shelf, faster with ladles; too hot warns, then the keeper walks him out. **A round:** hot
  through, then cooled by lying in the snow (crouch in snow) or by the cold sea within 150 s. Three rounds a day is the
  full banya. On the way out the keeper may give a coal (for a snowman's eyes).
- **The ice hole** off the pier's north side: one 3 m tile of the sea ice left open, a rim of broken ice and boards, a
  ladder down from the deck that stays all year. In water at 9 C or colder the cold builds (once the banya's heat is
  spent); a swim counts after 15 s; past the cold he must get out, past more the walruses haul him onto the deck. The
  towel warms three times faster. The walruses' club swims on Sunday mornings 8:30-11 in season (December to March);
  everybody who dares on New Year's morning 8-13.
- **Snowballs.** On snow that packs (3 cm or deeper, between -7 and +1.8 C; wet snow best) **G** with no feed in the
  hand scoops (0.5 s) and throws. Children aged 6 to 15 playing outdoors within 14 m join when one is hit or a ball lands
  near: they throw back every 2 to 5 s for two minutes, aiming with a lead on his run (the error grows with the distance).
  The score shows when it ends.
- **Snowmen.** Crouch and **E** on snow that packs: a ball to roll; it grows as he pushes it (about 10 m of 15 cm snow
  for a big base). **E** leaves it, or with a small enough ball at a bigger one lifts it on. Three balls are a snowman;
  then **E** gives twig arms, a carrot nose (the market), coal eyes. A thaw slumps them, they are saved (eight at most).
- **The radio** (`docs/music.md`) adds the rink to the weather (open with its thickness, thin, slush) and the walruses'
  invitation on Saturday afternoons and New Year's Day.
- **The townsfolk** (`docs/people.md`): skaters on the rink in the evenings, all day on the first snow's day, holidays and
  winter-break weekends (condition `rink`, `rink_busy`); the banya's regulars on its days (`banya`); the walruses in the
  hole on Sunday mornings and New Year's morning (`walrus`). `ULyudWinterPlay` sets the conditions every 10 s
  (`UPeopleLifeSubsystem::SetCondition`), the activities in `Content/PeopleData` read them.

Stamps: the collection "winter" (12 pieces: first glide, long glide, a fall, steam, the venik, a roll in the snow, the
ice hole, three rounds, the walruses' club, the New Year swim, a snowball fight, a snowman). Quest events `winter <what>`.

## Code and data

| What | Where |
| --- | --- |
| The rules, engine-free | `Private/Winter/WinterPlayModel.h` (ice, cracks, skating, room, body, cold water, schedules, packing, aim, fight, snowmen, save) |
| The subsystem | `Public/Winter/LyudWinterPlay.h`, `Private/Winter/LyudWinterPlay.cpp` (tick, input, HUD, console, dev), `LyudWinterPlayBuild.cpp` (the meshes) |
| The ice hole in the sea ice | `ULyudWinterSubsystem::SetIceHole` / `GetIceTop` (`Private/Winter/LyudWinter.cpp`) |
| Places and numbers | `tools/winter_play/content.json` → `tools/winter_play/winter_play.py` → `Content/City/winter_play.json`, `Content/PeopleData/winter_spots.json`, `Content/Dev/teleports/winter.json`, `tools/city/content/x_winter.json` (+ strings) |
| Words | `tools/winter_play/strings.json` → `Content/Localization/winter_play/strings_{ru,en,ja}.json` (the collection title goes to the city's table) |
| Shops | `tools/city/content/counters.json` (store_sports, store_haberdashery, market_fruit) |
| Sounds | `tools/winter_play/sounds.py` → `audio/sfx/winter/*.wav` + sidecars (the soundscape's group `Winter`) |
| Icons | `tools/winter_play/icon_grids.json` (one 3x2 Higgsfield grid, pinned) → `tools/winter_play/icons.py` |

The banya's place: s 21.5 m, y 924.5 m in the terrace frame (`tools/city_plan.py`), clear of the loungers (14 m from the
pier). The ice hole: the first point out along the pier deep enough at winter low water (1.4 m under 0.7 m below the
datum), snapped to the sea ice's 3 m grid; the cloud's macro terrain is too shallow, so there the generator puts it near
the pier head and says so (`shallow_guess`); run it on the Mac with `Saved/World/terrain_out.bin` to place it for real.
The ladder stands in the hole at its pier side (a climber's body hangs in open water) and steps off onto the deck.

After a change: `python3 tools/winter_play/winter_play.py`, then `python3 tools/city/interiors.py`. Never hand-edit the
outputs; do not regenerate `public_spaces.json` in the cloud (it needs the terrain).

Perf: the banya, the hole and the hooks are built once; the cracks again only when the winter or the band changes (the old
mesh is freed); snowballs (16), crumbs (12) and the rolled ball are pooled and out of Lumen and the distance fields;
snowmen are pooled slices of 8 parts moved only when one changes. Away from it all nothing runs but cheap checks.

## Developer mode

Group `winter`: teleports to the rink, the banya's porch and the pier by the hole; actions `winter.kit` (skates, venik,
hat, towel, carrot, coal), `winter.rink` (ice forced open / back to the weather), `winter.fight`, `winter.heat` (hot as
from the steam room), `winter.report`. Console: `lyud.WinterPlay.Report`, `lyud.WinterPlay.Ice -1|cm`,
`lyud.WinterPlay.Kit`, `lyud.WinterPlay.Fight`; `lyud.WinterPlay 0` turns it all off.

## Checks

- `tools/winterplaysim/run.sh`: 83 checks of the rules on the real data (ice growth, thaw and slush, cracks, top speed,
  glide length, hockey stop, turn radius, falls, boots, banya heat times, rounds, cold water, schedules, packing, the aim,
  the fight, snowmen, the save).
- `python3 tools/winter_play/winter_play.py --check`: outputs fresh, every engine string key in ru/en/ja, items in
  `LyudInventory.cpp`, the shops sell them.
- `python3 tools/city/interiors.py --check` (the collection "winter" is given by code: each piece must be named in the cpp).
- `tools/musicsim/run.sh` (the radio's rink and walrus lines).

## On the Mac (not done yet: the cloud cannot build or reach the image CDN)

1. Build; fix what the blind-written code needs.
2. `python3 tools/winter_play/winter_play.py` with the terrain (the ice hole's real place), then `tools/city/interiors.py`.
3. Icons: `python3 tools/winter_play/icons.py icons_winter <job_id> <output_url>` (both in `icon_grids.json`), look at
   every cell, then the game-items skill's import.
4. Sounds: `IMPORT_ONLY=sfx/winter` with `unreal/scripts/import_audio.py`.
5. Shots of the pond with skaters, the banya, the hole; a bench with skaters (`Dev.Do winter.rink`, evening) against main.
