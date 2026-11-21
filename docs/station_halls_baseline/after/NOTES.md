# Station halls v3: after (stations-v3 f9b9b9977, 2026-11-21 night)

Same views as the baseline (`../NOTES.md`), plus, at 12:30 only, `<station>_1230_barista` and `<station>_1230_attendant`
from 2 m in front of the staff places in halls.json. Before this run I rebuilt the hall dressing on the Mac (halls.py with
HALL_VEST, import_city tr_hall__*) and restored the lobby props (SM_TR_Lobby_*, committed in f9b9b9977).
`sheets/<station>.webp`: row 1 is noon, row 2 is 22:00, row 3 is the barista and the attendant.

## Transport.Hall (noon, all seven)

    hall 0 wing:    dressing yes, cores yes, 7 lamps (on), 14 places, staff 2/2, passengers 4 (want 4)
    hall 1 dome:    dressing yes, cores yes, 9 lamps (on), 18 places, staff 2/2, passengers 3 (want 3)
    hall 2 saucer:  dressing yes, cores yes, 10 lamps (on), 18 places, staff 2/2, passengers 3 (want 3)
    hall 3 star:    dressing yes, cores yes, 9 lamps (on), 14 places, staff 2/2, passengers 4 (want 4)
    hall 4 lantern: dressing yes, cores yes, 10 lamps (on), 22 places, staff 2/2, passengers 4 (want 4)
    hall 5 arches:  dressing yes, cores yes, 10 lamps (on), 17 places, staff 2/2, passengers 3 (want 3)
    hall 6 folded:  dressing yes, cores yes, 9 lamps (on), 16 places, staff 2/2, passengers 5 (want 5)

The log has no errors from LyudStationHall. The C++ compiled on the Mac as pushed, with no fixes needed.

## What got better

Every hall is now warm and evenly lit at both times, with no dark corners. Each has a buffet with a menu board (prices in
roubles), a samovar, cups, a till and cakes. There is a waiting corner with rows of seats, a cafe corner with tables, the
wainscot with its rail, and pendants. People sit, read and stand at the railing. The barista stands behind the counter
(seen front-on in Academ, Zenit and Amber). The vending cabinet is back (Mayak shows it, white, by the lift).

## Defects seen (to fix)

1. **The attendant is missing from her desk in 6 of 7 halls.** Each attendant view shows an empty desk with a small gold
   object (the bell?) and a blank wall behind. Only Mayak has her, standing behind the desk. Transport.Hall still says
   staff 2/2, so the body exists somewhere else: wrong spot, under the floor, or hidden.
2. **The barista view lands on or behind the barista in Aton, Polar, Horizon and Mayak.** From the staff spot plus 2 m
   along its yaw, the camera sees the back of the barista's head close up (in Mayak, the inside of a body or bag). Either
   halls.json's staff yaw points away from the counter in those halls (the barista faces the wall), or the person stands
   on the customer side.
3. **Full-size trees grow inside the halls.** A tall birch with yellow leaves stands from the planters up to the ceiling or
   dome in Academ, Polar, Horizon and Mayak. halls.json planters get a procedural plant whose species or age is a tree.
4. **Horizon at 22:00 (piece view): a white fish-like shape lies on the buffet counter**, in front of the samovar. It looks
   like the aquarium's fish (Lobby Fish) swimming outside the tank, or a dish of the buffet.
5. **Polar, piece view: thin black bars cross the whole picture**, a lattice hanging between the camera and the room, at
   both times. Possibly the stove's or a mosaic panel's frame, floating.
6. **Amber, piece view: a large flat amber-gold polygon fills the left side**, and the till's keys glow bright. That is
   the amber case's glow material, very strong at this distance.
7. **Academ 22:00, centre view: a big white wedge-shaped plane cuts across the picture.** It is not there at noon.
8. The black glossy glass railing and core mullions are still the heaviest shapes in every view (unchanged from the
   baseline). The hall street door stays the only thing the "core" view sees in 6 halls (see `../NOTES.md`).
9. Lint (blender/transport/lint_mesh.py) on the rebuilt lobby props: 6 of 11 have coplanar faces. The vending machine's
   steel shelves lie on its glowing back (0.26 m2 each, 4 of them) and its skin on its glass; the stove's door lies on the
   fire. Look for z-fighting there.
