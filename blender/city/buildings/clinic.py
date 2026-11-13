"""Clinic: a small two-storey white modernist polyclinic.

Original design in the spirit of 1960s European health-centre modernism (white plaster, ribbon windows, a glazed
stair tower with diagonal concrete fins, a cantilevered canopy, a red-cross panel): 28 x 12 m main block (storey 3.6 m)
plus a 3.6 x 2.4 m glass stair tower at the front-left corner (8.4 m), roof terrace parapet, a lift/plant box (9.9 m),
vertical brise-soleil fins over the upper windows of the right half.
Ground floor ENTERABLE: waiting/reception hall (17 x 11 m, entrance door 1.8 m two leaves under the canopy, big sidelights,
reception counter with a "counter" spot, 4 "bench" spots), a corridor and three doctors' rooms (each: desk + chair
("work"), patient chair ("seat"), couch), doors 1.2 x 2.2 m, CT_Interior walls, floor and ceilings. The upper floor is solid.
Spots: counter, 4 bench, 3 x work + 3 x seat. Window kinds: "public" (all), "stair" (tower). No flats.
Entrance: "main".
Ideas for later: a first-floor with consulting rooms, a pharmacy kiosk, an ambulance bay at the back, a waiting-room TV.
Requests: none.
"""
import math
import os

from citykit import box, prism, ring, DOOR_H  # noqa: F401
import geom
import interior_kit as kit
from furn_int2 import Furn

INFO = {
    "name_ru": "Поликлиника", "name_en": "Clinic",
    "style": "modernism", "use": "public",
    "inspired_by": "1960s European health centres: white ribbon-window blocks with glazed stair towers",
}

H1, H2 = 3.4, 7.2          # ground clear height, wall top
SLAB = 0.2
Y0, Y1 = 4.0, 16.0
X0, X1 = -14.0, 14.0
T = 0.3


def _xz(m, mat, poly, y0, y1):
    geom.extrude_profile(m, mat, mat, poly, -y1, -y0, lambda u, v, t: (u, -t, v))


def _wall(B, p0, p1, mat, holes, z1=H2, kind="public", skin=True, cards=True, cols=2, skin_z1=H1):
    """A wall p0->p1 (outside on the right). holes: (a, b, h0, h1) with a, b the coordinates (x or y) along the wall.
    Adds window cards in the holes and an inner CT_Interior skin up to skin_z1 (the enterable ground floor)."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ax = 0 if abs(dx) > abs(dy) else 1
    c0 = p0[ax]

    def U(a, b, c0=c0):
        return tuple(sorted((abs(a - c0), abs(b - c0))))
    uh = []
    for a, b, h0, h1 in holes:
        u0, u1 = U(a, b)
        uh.append((u0, u1, h0, h1))
    F = B.facade(p0, p1, 0.0, z1, mat, holes=uh, depth=T, reveal_mat="CT_ConcreteWhite", back=False)
    if cards:
        for (u0, u1, h0, h1), (_, _, _, _) in zip(uh, holes):
            if h1 - h0 > 2.3 and h0 < 0.5 and kind == "public" and h1 < 2.5:
                continue                                  # a door hole: stays open
            B.facade_window(F, u0, u1, h0, h1, kind=kind, cols=cols if (u1 - u0) > 1.4 else 1, inset=0.12)
    if skin:
        L = math.hypot(dx, dy)
        n = (dy / L, -dx / L)                              # outward
        q0 = (p1[0] - n[0] * T, p1[1] - n[1] * T)
        q1 = (p0[0] - n[0] * T, p0[1] - n[1] * T)
        d0 = q0[ax]
        sh = []
        for a, b, h0, h1 in holes:
            if h0 < H1:
                sh.append(tuple(sorted((abs(a - d0), abs(b - d0)))) + (h0, min(h1, skin_z1)))
        B.facade(q0, q1, 0.0, skin_z1, "CT_Interior", holes=sh, depth=0.02, reveal_mat="CT_Interior", back=False)
    return F


def _furnish(B, cx):
    """Hall (reception, waiting rows, water cooler, posters), pharmacy corridor, three doctors' rooms (interior_kit)."""
    f = Furn(B)
    d = B.detail
    C = "#FFF0DC"
    _hall(B)
    _corridor(B)
    _rooms(B, cx)
    for seg in (((-13.7, 15.7), (-13.7, 4.3)), ((-13.7, 4.3), (-5.9, 4.3)), ((-4.1, 4.3), (3.3, 4.3)),
                ((3.3, 4.3), (3.3, 12.4)), ((3.3, 13.6), (3.3, 15.7)), ((3.3, 15.7), (-13.7, 15.7))):
        f.skirting(list(seg), h=0.12)
    f.border(-13.7, 3.3, 4.3, 15.7, w=0.35, mat="CT_ConcreteDark")
    f.border(3.6, 13.7, 11.3, 15.7, w=0.25, mat="CT_ConcreteDark")
    box(d, "CT_PlasterGrey", -6.0, -3.5, 4.3, 5.2, 0.0, 0.014, faces="Z")           # doormat
    # ---- lamps: each doctor's room, the hall, the corridor
    for c in cx:
        B.lamp((c, 7.6, 3.1), lumens=800, colour=C, kind="public", radius=6)
    for x in (-11.0, -6.5, -2.0):
        for y in (7.0, 12.8):
            B.lamp((x, y, 3.1), lumens=900, colour=C, kind="public", radius=7.5)
    for x in (6.0, 11.0):
        B.lamp((x, 13.5, 3.1), lumens=700, colour=C, kind="public", radius=6)


def _seat(B, tid, r, h=0.45, **props):
    x, y, z, yw = r["seats"][0]
    B.thing(tid, "seat", (x, y, z), yw, height=h, **props)


def _hall(B):
    d = B.detail
    # ---- reception: a long desk facing the door, staff behind it
    rd = kit.reception_desk(B, -5.0, 11.65, 0.0, 0.0, w=5.2, front="CT_PlasterGreen")
    kit.telephone(B, -3.6, 11.9, 0.75, 200)
    kit.cup(B, -6.8, 12.0, 0.75, 0.0)
    kit.books_stack(B, -4.4, 11.95, 0.75, 10, n=3)
    kit.typewriter(B, -6.0, 11.95, 0.75, 180, mat="CT_SteelWhite")
    B.spot("reception", "counter", (-5.0, 10.8, 0.0), 180.0)
    B.spot("nurse_desk", "work", (-5.0, 12.65, 0.0), 0.0)
    B.thing("reception", "counter", (-5.0, 11.5, 1.1), 0.0, role="nurse", service="reception", shop="pharmacy")
    kit.chair(B, -5.8, 12.75, 0.0, 0.0, kind="school", mat="CT_PlasterGreen")
    # behind the desk: filing cabinets between the back windows, a notice board and a clock
    for k, x in enumerate((-9.35, -8.8)):
        kit.filing_cabinet(B, x, 15.7, 0.0, 0.0, mat=("CT_PlasterGreen", "CT_PlasterBlue")[k])
    kit.wall_clock(B, -5.0, 15.7, 0.0, 0.0, zc=2.35, face="clock_2")
    kit.poster(B, -5.0, 15.7, 0.0, 0.0, name="poster_safety", w=0.6, zc=1.55)
    kit.filing_cabinet(B, -2.0, 15.7, 0.0, 0.0, drawers=3)
    kit.fire_extinguisher(B, -1.4, 15.7, 0.0, 0.0)
    # ---- waiting: rows along the walls and two in front of the desk
    rows = [("wait_0", -12.85, 7.2, 90.0), ("wait_1", -12.85, 9.4, 90.0), ("wait_2", -8.4, 8.0, 180.0),
            ("wait_3", -1.6, 8.0, 180.0), ("wait_4", 2.8, 6.4, 270.0), ("wait_5", 2.8, 8.4, 270.0)]
    for i, (sid, x, y, yaw) in enumerate(rows):
        r = kit.waiting_row(B, x, y, 0.0, yaw, n=4 if i in (2, 3) else 3, mat=("CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen")[i % 3])
        B.spot(sid, "bench", (x, y, 0.0), yaw)
        mid = r["seats"][len(r["seats"]) // 2]
        B.thing("hall_" + sid, "seat", (mid[0], mid[1], mid[2]), mid[3], height=0.45)
    # magazine corner: a low table with magazines and a sofa under the west windows
    ct = kit.coffee_table(B, -8.4, 9.9, 0.0, 0.0, w=1.1)
    kit.newspaper(B, -8.6, 9.9, ct["top"], 15, seed=2)
    kit.newspaper(B, -8.1, 9.95, ct["top"], -20, seed=3)
    kit.rug(B, -8.4, 9.9, 0.0, 0.0, w=2.2, d=1.5, name="rug_4", seed=3)
    rk = kit.magazine_rack(B, -13.7, 12.9, 0.0, 90.0, w=0.9)
    B.thing("hall_magazines", "shelf", (rk["pos"][0], rk["pos"][1], 1.1), 90.0, topic="medicine")
    # ---- plants
    kit.plant(B, -12.3, 15.35, 0.0, 0.0, h=1.7)
    kit.plant(B, 2.7, 14.9, 0.0, 0.0, h=1.6, pot="CT_TileWhite")
    kit.plant(B, -2.4, 5.0, 0.0, 0.0, h=1.3, kind="monstera")
    kit.plant(B, -10.6, 5.0, 0.0, 0.0, h=1.3, pot="CT_TileWhite", kind="palm")
    # ---- water cooler by the corridor door
    box(d, "CT_FrameWhite", 2.85, 3.25, 11.4, 11.8, 0.0, 0.95)
    box(d, "CT_PlasterBlue", 2.92, 3.18, 11.47, 11.73, 0.95, 1.4)
    box(d, "CT_SteelRed", 3.2, 3.3, 11.5, 11.6, 0.6, 0.75)
    B.thing("water_cooler", "machine", (3.05, 11.6, 1.0), 270.0, what="fountain")
    # ---- walls (south wall = the tower's back, west wall between the windows, east partition)
    nb = kit.notice_board(B, -12.2, 4.3, 0.0, 180.0, w=1.5, zc=1.6)
    B.thing("hall_notices", "board", (-12.2, 4.35, 1.6), 180.0, topic="health")
    kit.poster(B, -10.8, 4.3, 0.0, 180.0, name="poster_health", w=0.5, zc=1.7)
    kit.poster(B, -13.7, 9.1, 0.0, 90.0, name="chart_calendar", w=0.7, zc=1.6)
    B.thing("hall_calendar", "board", (-13.65, 9.1, 1.6), 90.0, topic="health")
    kit.poster(B, -13.7, 10.0, 0.0, 90.0, name="chart_birds", w=0.7, zc=1.6)
    kit.wall_clock(B, -12.8, 15.7, 0.0, 0.0, zc=2.4, face="clock_0")
    kit.picture(B, 3.3, 5.6, 0.0, 270.0, name="painting_dunes", w=0.9, zc=1.65)
    kit.poster(B, 3.3, 10.2, 0.0, 270.0, name="chart_anatomy", w=0.8, zc=1.6)
    B.thing("hall_anatomy", "board", (3.25, 10.2, 1.6), 270.0, topic="health")
    kit.sign_plate(B, -5.0, 11.65, 0.0, 0.0, name="sign_quiet", w=1.0, zc=2.75, hang=True)
    # children's corner in the north-west
    kit.rug(B, -11.7, 13.4, 0.0, 0.0, w=2.0, d=2.0, round=True, seed=7)
    kt = kit.table(B, -11.7, 13.4, 0.0, 0.0, w=0.9, d=0.6, h=0.5, mat="CT_PlasterYellow")
    kit.books_stack(B, -11.9, 13.4, kt["top"], 30, n=2, seed=5)
    for k, (dx, dy, yw) in enumerate(((-0.0, -0.55, 0.0), (0.0, 0.55, 180.0), (-0.75, 0.0, 270.0))):
        st = kit.stool(B, -11.7 + dx, 13.4 + dy, 0.0, yw, h=0.3, mat=("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterGreen")[k])
    B.thing("kids_table", "seat", (-11.7, 12.85, 0.3), 0.0, height=0.3)
    kb = kit.bookshelf(B, -13.7, 14.4, 0.0, 90.0, w=0.9, h=1.1, topic="kids", levels=3)
    B.thing("kids_books", "shelf", (-13.4, 14.4, 0.9), 90.0, topic="kids")
    kit.poster(B, -13.7, 15.1, 0.0, 90.0, name="kids_1", w=0.5, zc=1.6)
    kit.rug(B, -3.0, 8.0, 0.0, 0.0, w=3.2, d=1.6, name="rug_2", seed=4)


def _corridor(B):
    # ---- pharmacy counter on the east end, facing the hall gap
    pc = kit.pharmacy_counter(B, 12.05, 14.0, 0.0, 270.0, w=2.4)
    kit.cup(B, 12.0, 13.5, 1.05, 0.0, mat="CT_PlasterGreen")
    kit.bottles(B, 12.05, 14.6, 1.05, 270.0, n=2, seed=4)
    kit.cash_register(B, 12.0, 14.4, 1.05, 270.0)
    B.thing("pharmacy", "counter", (11.95, 14.0, 1.05), 270.0, role="nurse", service="pharmacy", shop="pharmacy")
    B.spot("pharmacy", "counter", (11.2, 14.0, 0.0), 270.0)
    B.spot("pharmacist", "work", (12.9, 14.0, 0.0), 90.0)
    kit.sign_plate(B, 13.7, 14.0, 0.0, 270.0, name="sign_pharmacy", w=1.5, zc=2.6)
    # ---- queue seats along the north wall (under the windows) and a bench by the door of room 1
    for k, x in enumerate((6.0, 9.2)):
        r = kit.waiting_row(B, x, 15.4, 0.0, 0.0, n=3, mat=("CT_PlasterGreen", "CT_PlasterYellow")[k])
        B.spot("queue_%d" % k, "bench", (x, 15.4, 0.0), 0.0)
        mid = r["seats"][1]
        B.thing("corridor_row_%d" % (k + 1), "seat", (mid[0], mid[1], mid[2]), mid[3], height=0.45)
    # ---- the corridor's south wall between the doors (faces +y)
    kit.notice_board(B, 6.95, 11.3, 0.0, 180.0, w=1.3, zc=1.6)
    B.thing("corridor_notices", "board", (6.95, 11.35, 1.6), 180.0, topic="health")
    kit.poster(B, 10.4, 11.3, 0.0, 180.0, name="chart_periodic", w=0.8, zc=1.6)
    B.thing("corridor_chart", "board", (10.4, 11.35, 1.6), 180.0, topic="health")
    kit.fire_extinguisher(B, 4.1, 11.3, 0.0, 180.0)
    kit.wall_phone(B, 13.4, 11.3, 0.0, 180.0)
    kit.plant(B, 4.0, 15.0, 0.0, 0.0, h=1.5, kind="fern")
    kit.plant(B, 13.2, 11.9, 0.0, 0.0, h=1.4, pot="CT_TileWhite")
    kit.floor_lamp(B, 4.2, 14.4, 0.0, 0.0)
    kit.rug(B, 8.4, 13.5, 0.0, 90.0, w=1.3, d=4.8, name="rug_1", seed=9)


def _rooms(B, cx):
    wxs = (3.6, 7.1, 10.6)
    exs = (6.8, 10.3, 13.7)
    names = ("therapist", "eyes", "checkup")
    cabs = ("CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterYellow")
    for i, c in enumerate(cx):
        wx, ex = wxs[i], exs[i]
        nm = names[i]
        # doctor's desk under the window (the doctor's back to the glass), a visitor's chair on the other side
        dk = kit.office_desk(B, c, 5.35, 0.0, 180.0, w=1.4, typewriter_on=(i == 0), seed=i)
        ch = kit.chair(B, c, 4.72, 0.0, 180.0, kind="school", mat="CT_PlasterGreen")
        kit.table_lamp(B, c + 0.55, 5.45, 0.75, 0.0)
        _seat(B, "%s_doctor_chair" % nm, ch)
        B.spot("doctor_%d" % i, "work", (c, 4.72, 0.0), 180.0)
        vc = kit.chair(B, c - 0.2, 6.35, 0.0, 0.0, kind="school", mat="CT_PlasterYellow")
        B.spot("patient_%d" % i, "seat", (c - 0.2, 6.35, 0.0), 0.0)
        if i == 2:
            x_, y_, z_, yw_ = vc["seats"][0]
            B.thing("patient_2", "seat", (x_, y_, z_), yw_, height=0.45, what="checkup")
        else:
            _seat(B, "%s_patient_chair" % nm, vc)
        # couch along the west wall, anatomy / health picture above it
        cu = kit.medical_couch(B, wx + 0.38, 8.2, 0.0, 90.0)
        B.thing("%s_couch" % nm, "bed", (wx + 0.38, 8.2, 0.7), 90.0, height=0.7, rest="nap")
        kit.poster(B, wx, 8.2, 0.0, 90.0, name=("chart_anatomy", "chart_calendar", "poster_safety")[i], w=0.8, zc=1.7)
        B.thing("%s_chart" % nm, "board", (wx + 0.05, 8.2, 1.7), 90.0, topic="health")
        # sink and mirror on the east wall, a cabinet
        sy = 9.1 if i == 2 else 7.0                      # room 3 has a window in the east wall at y 6..8
        kit.kitchen_unit(B, ex, sy, 0.0, 270.0, modules="sink", wall_cupboards=False)
        kit.mirror(B, ex, sy, 0.0, 270.0, zc=1.6, w=0.55, h=0.8)
        if i < 2:
            kit.filing_cabinet(B, ex, 8.7, 0.0, 270.0, mat=cabs[i])
        kit.plant(B, wx + 0.3, 5.0, 0.0, 0.0, h=1.0, kind="cactus" if i != 1 else "fern", pot="CT_PlasterRed")
        if i == 2:
            kit.fire_extinguisher(B, wx, 10.3, 0.0, 90.0)
        # the room's own equipment
        if i == 0:
            sc = kit.scales(B, ex - 0.35, 10.3, 0.0, 0.0)
            B.thing("patient_0", "machine", (ex - 0.35, 10.3, 1.0), 0.0, what="scales")
            sk = kit.skeleton(B, wx + 0.35, 10.3, 0.0, 90.0)
            B.thing("skeleton", "board", (wx + 0.35, 10.3, 1.2), 90.0, topic="medicine")
        elif i == 1:
            kit.eye_chart(B, c + 1.1, 11.0, 0.0, 0.0, zc=1.55)
            B.thing("patient_1", "board", (c + 1.1, 10.9, 1.55), 0.0, what="eye_chart")
            ec = kit.chair(B, c + 1.0, 6.5, 0.0, 180.0, kind="school", mat="CT_PlasterBlue")
            B.spot("eye_seat", "seat", (c + 1.0, 6.5, 0.0), 180.0)
        else:
            kit.screen_divider(B, wx + 0.75, 9.9, 0.0, 90.0)
            kit.wardrobe(B, ex, 10.35, 0.0, 270.0, w=0.8, h=1.9)


def build(B):
    m, d = B.m, B.detail
    IX0, IX1, IY0, IY1 = X0 + T, X1 - T, Y0 + T, Y1 - T
    # ================================================================ facades (holes in world x / y)
    bay = (X1 - (-10.4)) / 7
    upfront = []
    for i in range(7):
        a, b = -10.4 + i * bay + 0.3, -10.4 + (i + 1) * bay - 0.3
        if i == 1:
            continue                                       # the red-cross panel hangs here
        upfront.append((a, b, 4.5, 6.6))
    front = [(-5.9, -4.1, 0.0, 2.4), (-10.0, -6.5, 0.3, 3.0), (-3.5, 2.8, 0.3, 3.0),
             (4.3, 6.1, 0.9, 2.6), (7.8, 9.6, 0.9, 2.6), (11.3, 13.1, 0.9, 2.6)] + upfront
    _wall(B, (-10.4, Y0), (X1, Y0), "CT_Plaster", front)
    B.entrance("main", (-5.0, Y0, 0.0), 0.0, width=1.8, height=2.4, kind="street", leaves=2)
    # back (faces +y), from x = 14 to -14
    back = [(x - 0.9, x + 0.9, 0.9, 2.6) for x in (-11.0, -7.0, -3.0, 4.5, 8.0, 11.5)]
    back += [(-13.0 + i * 3.857 + 0.3, -13.0 + (i + 1) * 3.857 + 0.3 - 0.6 + 0.0, 4.5, 6.6) for i in range(7)]
    _wall(B, (X1, Y1), (X0, Y1), "CT_Plaster", [(a, b, h0, h1) for a, b, h0, h1 in back])
    # right end (faces +x)
    _wall(B, (X1, Y0), (X1, Y1), "CT_Plaster", [(6.0, 8.0, 0.9, 2.6), (12.0, 14.0, 0.9, 2.6), (6.0, 8.0, 4.5, 6.6), (12.0, 14.0, 4.5, 6.6)])
    # left end (faces -x), from y = 16 to 4
    _wall(B, (X0, Y1), (X0, Y0), "CT_Plaster", [(5.5, 8.5, 0.9, 2.6), (10.5, 13.5, 0.9, 2.6), (5.5, 8.5, 4.5, 6.6), (10.5, 13.5, 4.5, 6.6)])
    # ================================================================ glazed stair tower (front-left)
    TH = 8.1
    box(m, "CT_ConcreteWhite", -13.7, -10.4, 1.9, Y0 + T, 0.0, TH)
    box(m, "CT_ConcreteWhite", -14.3, -10.1, 1.3, 4.3, TH, TH + 0.3)
    F = B.facade((-14.0, 1.6), (-10.4, 1.6), 0.0, TH, "CT_ConcreteWhite", holes=[(0.45, 3.15, 0.15, 7.9)], depth=T,
                 reveal_mat="CT_ConcreteWhite", back=False)
    B.facade_window(F, 0.45, 3.15, 0.15, 7.9, kind="stair", cols=3, rows=4, inset=0.12)
    F = B.facade((-14.0, Y0), (-14.0, 1.6), 0.0, TH, "CT_ConcreteWhite", holes=[(0.4, 2.0, 0.15, 7.9)], depth=T,
                 reveal_mat="CT_ConcreteWhite", back=False)
    B.facade_window(F, 0.4, 2.0, 0.15, 7.9, kind="stair", cols=2, rows=4, inset=0.12)
    # diagonal fins across the front glazing (stair flight silhouette) and landing bands
    for k, xs in enumerate((-13.6, -12.4, -11.2)):
        _xz(m, "CT_ConcreteWhite", [(xs, 0.15), (xs + 0.4, 0.15), (xs + 1.2, 7.9), (xs + 0.8, 7.9)], 1.35, 1.5)
    for z in (3.6, 7.2):
        box(m, "CT_ConcreteWhite", -14.0, -10.4, 1.35, 1.6, z - 0.1, z + 0.1)
    # ================================================================ interior of the ground floor
    # floors / ceiling / upper storey
    box(m, "CT_Floor", IX0, IX1, IY0, IY1, 0.0, 0.02, faces="Z")
    box(m, "CT_Interior", IX0, IX1, IY0, IY1, H1, H1 + SLAB)
    box(m, "CT_Plaster", IX0, IX1, IY0, IY1, H1 + SLAB, H2)
    # partition hall | rooms (x 3.3..3.6), gap to the corridor at y 12.4..13.6
    box(m, "CT_Interior", 3.3, 3.6, IY0, 12.4, 0.0, H1)
    box(m, "CT_Interior", 3.3, 3.6, 13.6, IY1, 0.0, H1)
    box(m, "CT_Interior", 3.3, 3.6, 12.4, 13.6, 2.3, H1)
    # corridor wall behind the rooms (y 11..11.3) with three door gaps, dividers between rooms
    cx = (5.2, 8.7, 12.15)
    xs = [3.6] + [v for c in cx for v in (c - 0.6, c + 0.6)] + [IX1]
    box(m, "CT_Interior", xs[0], xs[1], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[2], xs[3], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[4], xs[5], 11.0, 11.3, 0.0, H1)
    box(m, "CT_Interior", xs[6], xs[7], 11.0, 11.3, 0.0, H1)
    for c in cx:
        box(m, "CT_Interior", c - 0.6, c + 0.6, 11.0, 11.3, 2.3, H1)
    for x in (6.8, 10.3):
        box(m, "CT_Interior", x, x + 0.3, IY0, 11.3, 0.0, H1)
    _furnish(B, cx)
    # ================================================================ canopy, cross panel, roof, fins
    box(m, "CT_ConcreteWhite", -8.5, -1.5, 0.8, Y0, 3.0, 3.25)
    box(m, "CT_PlasterRed", -8.5, -1.5, 0.75, 0.85, 2.9, 3.5)
    for px in (-8.2, -1.8):
        B.column_round(px, 1.3, 0.0, 3.0, 0.1, mat="CT_Steel", segs=10)
    cxp, czp = -5.17, 5.4
    box(m, "CT_TileWhite", cxp - 1.1, cxp + 1.1, Y0 - 0.06, Y0, czp - 1.1, czp + 1.1, faces="yxXzZ")
    box(m, "CT_SteelRed", cxp - 0.75, cxp + 0.75, Y0 - 0.1, Y0 - 0.06, czp - 0.25, czp + 0.25, faces="yxXzZ")
    box(m, "CT_SteelRed", cxp - 0.25, cxp + 0.25, Y0 - 0.1, Y0 - 0.06, czp - 0.75, czp + 0.75, faces="yxXzZ")
    # roof slab (proud), parapet, plant box
    box(m, "CT_ConcreteWhite", X0 - 0.4, X1 + 0.4, Y0 - 0.4, Y1 + 0.4, H2, H2 + 0.3)
    t = 0.2
    for (a, b, c, e) in ((X0 - 0.4, X1 + 0.4, Y0 - 0.4, Y0 - 0.4 + t), (X0 - 0.4, X1 + 0.4, Y1 + 0.4 - t, Y1 + 0.4),
                         (X0 - 0.4, X0 - 0.4 + t, Y0 - 0.4 + t, Y1 + 0.4 - t), (X1 + 0.4 - t, X1 + 0.4, Y0 - 0.4 + t, Y1 + 0.4 - t)):
        box(m, "CT_ConcreteWhite", a, b, c, e, H2 + 0.3, H2 + 0.8)
    box(m, "CT_Roof", X0 - 0.2, X1 + 0.2, Y0 - 0.2, Y1 + 0.2, H2 + 0.29, H2 + 0.31, faces="Z")
    box(m, "CT_PlasterGrey", 4.0, 8.0, 9.0, 13.0, H2 + 0.3, 9.9)
    # brise-soleil fins before the upper windows of the right half
    x = 0.6
    while x < 13.8:
        box(d, "CT_Louvre", x, x + 0.1, Y0 - 0.5, Y0 - 0.02, 4.1, 6.9)
        x += 0.9
    B.footprint = [(X0 - 0.4, 0.75), (X1 + 0.4, 0.75), (X1 + 0.4, Y1 + 0.4), (X0 - 0.4, Y1 + 0.4)]
    B.height = 9.9
    if os.environ.get("KIT_CHECK"):
        print("KITCHECK", kit.check(B))
