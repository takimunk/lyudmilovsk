# Station halls: baseline (main 81e70e32c, 2026-11-20)

Offscreen -game run of `unreal/scripts/test_station_halls.sh` (branch stations-v3), 1600x900 shots scaled to 1280.
I took them in Lvl_Scene01 copied from the north worktree, with the eye 1.65 m over the hall floor, at 12:30 and at 22:00.
Each file is named `<station>_<hhmm>_<view>.webp`:
- core: from the lift's top landing (`Transport.Goto <s> top`). In every hall except Mayak this landing looks straight at the
  hall's street door from 1-2 m, so this view shows little.
- corner: from a corner across the hall.
- vending: from 2 m off the vending machine's spot, looking at it.
- piece / centre: at the signature piece.

`sheets/<station>.webp` puts the 8 views of one station on one sheet: noon on the top row, 22:00 on the bottom.

## Defect found that applies to every hall

**The lobby kit is gone from main.** Commit 638b04c3f (2026-10-29, "floors, glass and the lift's cab") re-imported
transport_kit and deleted every `SM_TR_Lobby_*` static mesh. That covers the vending cabinet, Aton's coffee bar, Academ's
pendulum, Zenit's chandelier, Polar's stove + samovar, Mayak's lens, Amber's amber case and Horizon's aquarium.
`LyudLobby.cpp` loads them with `LOAD_NoWarn`, so nothing warns. In game, only the parts made in code are left: the goods
float in mid-air as rows of icons and bottles, with the black delivery tray on the floor under them. My first run showed this well,
because its camera stood on the machine's own spot. They were added in 288c98975
and ae3708f90.

## What every hall has in common

- It is the same room seven times: a beige plaster ring round a glazed lift core with red-brick bands, a black glass
  railing round the stairwell, a pebble/terrazzo floor strip, the odd poster, a floor lamp or sconce, a potted plant, a
  bench, the route diagram. There is nothing else in it: no people, no staff, no counter, no seating groups, no signage
  except Polar's «Касса».
- Lighting: flat and dim. The lift core's ceiling spots throw bright cones on the core's own roof and on the ceiling
  (Zenit, Amber, Horizon). The walls and floor stay grey-beige. Most halls have no windows (or only small ones), so noon
  and 22:00 look **almost the same**: the light inside does not change with the time of day, and there is no evening
  warmth.
- The glass railing and the core's black mullions are the strongest shapes in every view. They are chunky and glossy-black,
  and the noise texture on them looks wet.
- Materials: untextured plaster everywhere, a big blank surface. The plaster walls and ceiling show no detail, trim,
  skirting or cove.

## Per station

- **aton (Wing)**: a narrow ring and the smallest hall. It feels cramped round the core and is dark at both times. The
  clerestory band of windows shows the sky at noon but adds little light. The coffee bar (the signature piece) is missing
  (see above). There is a floor lamp, a poster and the diagram. "piece" shows only the core and the stairwell.
- **academ (Dome)**: a big round dome with portholes, and the most empty hall. Large bare curved walls, and the dark top of
  the dome. At 22:00 the portholes turn black and the room goes grey-brown. The pendulum (centre) is missing: the view looks
  up at an empty dome with one bright spot. White egg-chairs stand by the wall. A small black box floats high on the dome
  wall in the centre view (top middle; possibly a sconce or speaker at the wrong height, to check).
- **zenit (Saucer)**: the street door stands open at noon (core view). Warm cove lighting on the ceiling makes it the best
  lit hall. The chandelier is missing. There is a bench and a planter behind the railing. Otherwise empty.
- **polar (Star)**: a bare hall with plain walls. The «Касса» sign hangs over an empty low counter, with no stove and no
  samovar (missing). There is an easel/diagram and a plant. The core view looks at a closed door 2 m away.
- **mayak (Lantern)**: fully glazed walls with a sea and forest view, and the only hall where noon and night differ a lot. A
  cabin hangs right outside the glass. At night the room is almost black: there is no interior light to speak of, and the
  glass shows only reflections. **White specks** cover the glass at both times (looks like rain/drop decals or particles
  stuck on or behind the glass). The black frames of the faceted dome are heavy. The lens (centre) is missing.
- **amber (Arches)**: dim, with a warm wall-washer glow on the left wall. The amber case is missing. There is a floor lamp,
  a poster, a planter and the diagram. The core view looks at a closed door.
- **horizon (Folded)**: like Amber, with a bench and a floor lamp. The aquarium is missing. A large translucent **glass sphere
  floats** in the upper left of the "piece" view (a bubble from the aquarium's code-made parts, left floating without its
  tank).

## Timings on this Mac (2026-11-20 evening, other threads' builds and games running)

- Editor build of origin/main, with Intermediate copied from a built worktree: 405 s (225 actions, 4 parallel).
- One test_station_halls run (7 halls x 4 views x 2 times = 56 shots): 886-913 s with nice 19. With `taskpolicy -b`
  (background QoS) under load it crawled to about 1 frame in 30 s and hit the 20-minute watchdog after 15 shots.
