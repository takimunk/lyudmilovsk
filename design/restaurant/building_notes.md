# Ресторан «Янтарь» (Yantar): the building

Built by `blender/restaurant/building.py` (helpers `restaurant_common.py`, which reuses `blender/diner/diner_common.py`);
the layout the game reads is `unreal/Lyudmilovsk/Content/Restaurant/layout.json`.

## Design

A Soviet-modernist seaside pavilion on the Baltic promenade, 1960s-70s: a round, fully glazed drum under a thin, wide,
slightly upturned concrete disc on slender tapering columns (Niemeyer's floating discs), a solid ochre-tiled kitchen block
behind a straight chord wall, all on a low granite podium with steps to the promenade (-y), the sea (+x, 8 m wide) and
the kitchen yard (-x). The name «ЯНТАРЬ» stands in constructivist block letters on a steel frame on the roof rim over
the main door.

## Sizes (metres, frame: origin on the floor at the drum's centre, +x sea, -y main door)

- Podium r 13.6, top z 0, skirt to -1.2 (ground about -0.6); steps 4 x 0.15 rise, 0.35 tread: door flight 2.4 wide,
  sea flight 8 wide, kitchen yard landing 1.6 x 1.2 then 4 steps.
- Drum: 24-gon, wall mid plane at apothem 10.0 (segment centres at 15 deg * k), 0.16 thick, ceiling 4.0. Each segment is
  two panes wide with a transom (2.5..2.58); a 0.35 concrete sill band, a 0.45 bronze head band (the ribs land on it).
- Chord wall x = -8.4 (0.25), kitchen block x -14.4..-8.4, y +-5.394 (outer; the drum's outer line meets the chord's kitchen
  face there), walls 0.25, roof 3.6, ceiling 3.3, ten + ten small windows high up, amber tile cladding.
- Pass y +-1.7, z 0.95..2.05, stainless shelf top 1.05 (0.45 deep), heat-lamp bar, low counter (0.92) under it in the hall.
- Doors (ALyudDoor holes 1.22 x 2.22): front (0, -10), kitchen (-8.4, 3.6), back (-14.275, -2.5).
- Disc roof r 14: soffit 4.0 inside the glass line, flat to r 10.4 then rising to 4.2 at the edge; top 4.6, edge 4.55
  with a 0.25 raised rim (4.8); oculus ring r 1.0..2.0 glazed at 4.7; 12 ribs inside, 10 under the overhang on 10
  tapering columns at r 12.4 (two of the twelve positions fall inside the kitchen block, which carries the disc there),
  a central column r 0.35 in a planter-bench ring r 1.3 (flat soil at 0.40, seat 0.45).
- Hall: terrazzo floor with a dark ring round the planter and an amber ring at r 5.6..5.75; bar y -5.2..-2.0 (1.05)
  with shelves behind; cloakroom counter by the main door; a menu lectern outside right of the door.
- Furniture: 6 round 4-seat tables (d 1.0, r 7.2 at 0, 45, -45, 90, 135, -120 deg) and 4 square 2-seat tables (0.7, r 4.0
  at +-30, +-150 deg), cloths 0.15 deep, 32 moulded-plywood chairs (seat 0.42 at 0.46). Hero tables: t1, t2, t7, t8.
- Walk graph: 26 nodes, 29 edges, 0.35 m clearance check: all clear.

## Triangles

| mesh | tris |
|---|---|
| SM_Rest_Shell | 6144 |
| SM_Rest_Inside | 4520 |
| SM_Rest_Glass | 592 |
| SM_Rest_Furniture | 5612 |
| SM_Rest_Lamps | 4034 |
| SM_Rest_Sign | 264 |
| building | 21166 |
