# Public spaces

The places where the town comes out: the promenade along the sea, the beach, the pier, **Sun Square** with its
fountain and stage, the **seaside park** with its pond and playground, the **fair**, and the old stair up to the
club. They furnish the town plan ([../city_plan.md](../city_plan.md), `Content/City/city_plan.json`): the plan's
lead builds the ground, the terraces' walls, their stairs, the seawall and the pier deck (`tools/city_mesh.py`);
this part lays the paving, the benches, the lamps, the railings, the fountain, the planting and the furniture.

## Where it comes from

    python3 tools/public_spaces.py              # -> Content/City/public_spaces.json + PeopleData/town_plan.json "spots"
    python3 tools/public_spaces.py --png p.png  # a plan drawing (not committed)

`tools/public_spaces.py` reads the plan (plots, spots, wall bays, links, terraces; `city_plan.py`'s
`to_map`/`wall_s`/`seaward_yaw`) and writes one layout. Rerun it after the plan changes: nothing is hand-placed.
Frames: the plan's terrace frame `(s, y)` (metres inland from the high-water line, map Y) for laying out; the JSON is
in Unreal metres, yaw in Unreal degrees (0 = +x = the sea, 90 = north).

The game builds it at play start (`ULyudPublicSpotsSubsystem` spawns `ALyudPublicSpaces` in Lvl_Scene01); nothing is
saved in the level, so a scene rebuild doesn't need to know it.

## What is where

| Place | Where (plan) | What |
|---|---|---|
| Promenade | T0 (-39.5), s 25 (seawall) .. W1 (s ~70), the whole town's length | pale concrete slabs with a granite band at s 44..47; maples in iron grates down the band every 20 m; tall globe lamps by the sea every 30 m and a second row by the town; the plan's 26 benches (S-BENCH-P*) backs to W1, more by the sea (S-BENCH-R*); bins; no railing on the seawall: its 0.45 m kerb (the lead's) is the parapet and a seat |
| Beach | B, Y 600..1400 | striped umbrellas with two loungers each, two rows (June-August only); changing booths by every other flight of sea steps (all year) |
| Pier | the lead's deck on the axis Y 905 | railings on its edge kerbs and round the head, lamps and benches down the deck, benches round the head (its middle is the lighthouse's) |
| Sun Square | P-SQUARE (T1, s 95..205, Y 840..970) | slab paving, a granite ring round the **Sun fountain** (an 18 m basin whose rim is a bench for 24, a two-tier centrepiece with a gilded sun); 12 benches facing it, 8 lamps round it, 4 round flowerbeds; maples and benches along both sides; the **stage** with a concrete shell on the east side, facing the sea |
| Seaside park | P-PARK (T1, s 195..315, Y 1070..1370) | a gravel loop, a maple avenue down the middle, cross paths; the **pond** (an oval in a granite kerb, frozen mid-December to mid-March); the **playground** (sand pit, swings, slide, climbing dome, seesaw on a rubber floor); birch, rowan and apple groups; a privet hedge on the fair's side, spiraea, two flowerbeds; park lamps and benches along the paths |
| Fair | P-FAIR (T1, s 105..175, Y 1135..1285), under the buildings thread's wave canopy | slab paving, two rows of striped stalls across a lane and a third row facing the promenade, a carousel, lamps, benches |
| Club stair | L-CLUB-STAIR (20, 300) -> (10, 175) | granite steps following the spur's slope (landings where it is gentle), posts and handrails, low lamps |

Plants are the plant system's (no static plants): the trees go into one `AProceduralForest` (shared bakes, live plants
only up close: Maple, Rowan, SilverBirch, WeepingBirch, BlackAlder, Apple), the flowers and shrubs are single
`AProceduralPlant`s (Marigold, Aster, Cosmos, DayLily, Privet, Spiraea). The plan's lead plants the street-tree strips
12 m in front of W2..W5 and skips plots; nothing here overlaps them.

## How it is built

- **Kit** (`Private/City/CityKit.*`): every prop is a small mesh built in code once (Soviet park bench, promenade
  bench, globe lamp, park lantern, cast-iron railing, tree grate, concrete urn, umbrella, lounger, changing booth,
  flowerbed kerb, pond kerb, stage, stall, carousel, swings, slide, climbing dome, seesaw, sand pit, the fountain's
  stone). Colours are vertex colours, roughness in their alpha; one material for all: `/Game/City/M_CityProp`
  (`unreal/scripts/make_city_materials.py`: slab, sett, board, masonry, gravel and rubber patterns drawn from UV0
  metres, lamp glass glowing from UV2 x `Glow`).
- **Props** are instanced (one set per kind and variant); the beach's summer things are their own sets, hidden out
  of season. Small props cast no distance-field or GI; nothing is updated per frame.
- **Paving and steps** (the layout's `boxes`) are merged into one mesh per skin and 100 m cell at their true sizes (so
  the joints meet the edges); their collision is a hidden instanced unit cube per block.
- **Fountain**: the stone is the kit's (`BuildFountainStone`, from the layout's `stone` levels); the water, splashes,
  sound and season are `ALyudFountain`'s (docs/fountain.md), built from the same levels with `flow_scale` 1.8 (speeds
  x1.8, the jet and bell grow with it, more and bigger drops). `ULyudFountainSubsystem::GetFountains()` lists both.
- **Lamps at night**: the glass glows (one MID parameter, fading over ~4 s when the sun drops under ~0.5 degree, off
  after it rises over ~2.5 degrees); a pool of `lyud.City.StreetLights` (14) shadowless point lights moves to the lamps
  nearest the camera (re-picked when the camera has moved 1.5 m, at most 4 times a second), so the street is lit
  where you are without a light per lamp.
- Tagged `NoTree`/`NoGrass`: the forests and the grass leave the paving bare.

Console: `lyud.City.PublicSpaces 0` (not built at the next start), `lyud.City.Show 0|1` (hide everything, for A/B),
`lyud.City.StreetLights N`, `lyud.City.Night -1|0|1`, `lyud.City.Summer -1|0|1`, `lyud.City.Ice -1|0|1`.

## Spots API

For the town life and the holidays: every place people use, by a stable id. The same list is written to
`Content/PeopleData/town_plan.json` key `spots` (offline schedules) and loaded at runtime by
`ULyudPublicSpotsSubsystem`.

Entry: `id`, `kind` (bench, fountain, promenade, beach, fair, square, park, pond, playground, pier, stage,
sunrise_point, sunset_point, bonfire), `sub` (rail, kerb, stroll, crowd, audience, towel, loungers, customers,
parents, bandstand...), `pos` [x, y, z] Unreal metres, `yaw`, `seats`; `slots` [{pos, yaw, height}] seat points for
benches, the fountain's rim (24), the seawall kerb, the pond's kerb, loungers; `path` stroll polylines (the promenade
by the sea and by the town, the park's loop, round the pond, the fair's lane, the pier); `season: summer` for the beach
ones; fair stalls carry `seller` {pos, yaw} and `goods`; `furnished: false` marks the plan's spots passed on without
furniture here (the lookouts, the cascade, the bonfire, the upper terraces' benches). Ids: the plan's own (`S-FOUNTAIN`,
`S-SQUARE`, `S-STAGE`, `S-PIER-HEAD`, `S-PROMENADE`, `S-BENCH-P01..26`, `S-BENCH-01..10`, `S-BEACH`, `S-FAIR`, `S-PARK`,
`S-POND`, `S-PLAYGROUND`, ...) and this layout's (`S-BENCH-SQ*` square, `S-BENCH-PK*` park, `S-BENCH-R*` promenade by
the sea, `S-BENCH-PR*` pier, `S-BENCH-FR*` fair, `S-KERB-*` seawall kerb, `S-RAIL-*`, `S-BEACH-*`, `S-TOWEL-*`,
`S-FAIR-STALL01..`, `S-SQUARE-AUDIENCE`, `S-PIER-SUNRISE`, `S-PROMENADE-SEA`, `S-PROMENADE-TOWN`).

Runtime (`Public/City/LyudPublicSpaces.h`):

    ULyudPublicSpotsSubsystem* S = ULyudPublicSpotsSubsystem::Get(this);
    FName Bench = S->FindNearestFree(TEXT("bench"), Where, 5000.f);      // optional Sub: TEXT("rail")...
    int32 Seat = S->Reserve(Bench, Person);                              // -1: full; again for the same person: its seat
    FTransform At = S->GetSeat(Bench, Seat);                             // walk there, sit facing its rotation
    S->Release(Bench, Person);                                           // or ReleaseAll(Person) going home

`GetSpots(Kind)`, `GetSpot(Id, Out)`, `FreePlaces(Id)`. Seatless spots (a crowd, a stroll) hand out places on a
spiral round the spot or spread along its path. Summer-only spots are skipped by `FindNearestFree` out of season.
Reservations live only in memory (people re-pick their seats after a load).

## Checking

`unreal/scripts/test_public_spaces.sh` (offscreen, silent, waits for a UE slot): shots of the square, the fountain,
the stage, the promenade, the beach in July and October, the pier, the park, the pond (and frozen in January), the
playground, the fair and the club stair, by day and at night; checks the props, the lamps lit at night, the season
switches, both fountains running, and the spots API (find a bench by the fountain, three seats then full, release).
`CITY_AB=1 CITY_EXTRA=-ueexclusive` adds frame times with everything shown and hidden in one process (alone on the
machine). Shots go to `unreal/Lyudmilovsk/Saved/CityShots/`.

## Open

- The cascade's water steps (S-CASCADE) wait for the walls' final shape at the bend.
- Ducks on the pond and skaters on the ice are the bird and town-life threads' to add (the pond is a spot).
- Ice follows the calendar, not the weather engine's frost.
