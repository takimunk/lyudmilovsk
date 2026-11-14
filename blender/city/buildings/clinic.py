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
    # ---- QA dressing: the middle of the hall and its back wall were bare
    kit.rug(B, -5.3, 8.1, 0.0, 0.0, w=4.2, d=2.2, name="rug_2")
    kit.picture(B, -5.0, 15.7, 0.0, 0.0, name="painting_sea_calm", w=1.2, zc=1.8)
    kit.wall_clock(B, -0.8, 15.7, 0.0, 0.0, zc=2.4)
    kit.plant(B, 1.4, 15.4, 0.0, 0.0, h=1.5, kind="palm")
    kit.hat_stand(B, -3.0, 5.0, 0.0, 0.0)
    _depth(B, cx)
    _depth2(B, cx)
    _depth3(B, cx)
    # two more lamps: the pharmacy end and the aquarium nook
    B.lamp((12.0, 13.2, 2.3), lumens=500, colour="#E8F4F0", kind="public", radius=4)
    B.lamp((-0.7, 14.6, 2.5), lumens=450, colour="#CFE6FF", kind="public", radius=4)


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
    for k, x in enumerate((-9.65,)):
        kit.filing_cabinet(B, x, 15.7, 0.0, 0.0, mat=("CT_PlasterGreen", "CT_PlasterBlue")[k])
    kit.wall_clock(B, -5.0, 15.7, 0.0, 0.0, zc=2.35, face="clock_2")
    kit.poster(B, -5.0, 15.7, 0.0, 0.0, name="poster_health", w=0.6, zc=1.55)
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
    kit.plant(B, 2.75, 10.7, 0.0, 0.0, h=1.6, pot="CT_TileWhite")
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
    kit.poster(B, 10.4, 11.3, 0.0, 180.0, name="chart_eye", w=0.8, zc=1.6)
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
        kit.poster(B, wx, 8.2, 0.0, 90.0, name=("chart_anatomy", "chart_calendar", "poster_health")[i], w=0.8, zc=1.7)
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
            kit.screen_divider(B, wx + 0.75, 9.9, 0.0, 90.0)             # the wardrobe went: the toy shelf (_depth2)


def _seg7(d, y, z, digit, sc=0.05, x_face=3.18, mat="CT_SteelRed"):
    """A red seven-segment digit on a board facing -x (u = y along the wall, v = z), `sc` the bar thickness."""
    S = {"0": "abcdef", "1": "bc", "2": "abdeg", "3": "abcdg", "4": "bcfg", "5": "acdfg", "6": "acdefg", "7": "abc",
         "8": "abcdefg", "9": "abcdfg"}[digit]
    w, h = 0.16, 0.12
    bars = {"a": (-w, w, h * 2, h * 2 + sc), "g": (-w, w, h, h + sc), "d": (-w, w, 0.0, sc),
            "f": (-w, -w + sc, h, h * 2), "b": (w - sc, w, h, h * 2), "e": (-w, -w + sc, 0.0, h), "c": (w - sc, w, 0.0, h)}
    for k in S:
        u0, u1, v0, v1 = bars[k]
        box(d, mat, x_face - 0.005, x_face, y + u0, y + u1, z + v0, z + v1)


def _depth(B, cx):
    """Second pass: queue ticket and board, aquarium nook, inhalation corner, the dentist's door, card file, height rod,
    radio point, pharmacy shelves, the doctors' small instruments."""
    d = B.detail
    # ---- the waiting hall: ticket stand by the desk, queue board over the corridor gap
    box(d, "CT_Steel", -7.64, -7.56, 10.56, 10.64, 0.0, 1.15)
    box(d, "CT_FrameWhite", -7.74, -7.46, 10.46, 10.74, 1.15, 1.3)
    box(d, "CT_SteelRed", -7.72, -7.48, 10.44, 10.46, 1.18, 1.27, faces="yxXzZ")
    box(d, "CT_TileWhite", -7.7, -7.5, 10.5, 10.7, 1.3, 1.34)
    B.thing("ticket_machine", "machine", (-7.6, 10.6, 1.2), 0.0, what="tickets")
    box(d, "CT_Steel", 3.2, 3.3, 12.46, 13.54, 2.43, 3.12)
    box(d, "CT_Rubber", 3.19, 3.3, 12.5, 13.5, 2.47, 3.08)
    _seg7(d, 12.8, 2.62, "1")
    _seg7(d, 13.2, 2.62, "7")
    B.thing("queue_board", "board", (3.15, 13.0, 2.5), 270.0, topic="queue")
    # ---- aquarium nook on the back wall (a watering can beside it)
    aq = kit.aquarium(B, -0.7, 15.45, 0.0, 0.0, w=1.0, seed=3)
    B.thing("hall_aquarium", "screen", (-0.7, 15.25, 1.0), 0.0, what="aquarium")
    box(d, "CT_PlasterGreen", -1.3, -1.12, 15.35, 15.55, 0.0, 0.2)
    box(d, "CT_PlasterGreen", -1.12, -1.08, 15.4, 15.5, 0.12, 0.16)
    B.thing("aquarium_can", "machine", (-1.2, 15.4, 0.2), 0.0, what="watering")
    B.spot("aquarium_watch", "stand", (-0.7, 14.6, 0.0), 180.0)
    # ---- donation jar on the desk: "for a fish"
    box(d, "CT_Glass", -2.9, -2.78, 11.85, 11.97, 0.75, 0.93)
    box(d, "CT_SteelRed", -2.91, -2.77, 11.84, 11.98, 0.93, 0.96)
    box(d, "CT_TileWhite", -2.88, -2.8, 11.835, 11.845, 0.78, 0.9, faces="y")
    B.thing("fish_jar", "machine", (-2.84, 11.9, 0.9), 0.0, what="jar")
    # ---- inhalation corner east of the middle: table with the inhaler, two stools, a screen behind
    tb = kit.table(B, 0.7, 13.7, 0.0, 0.0, w=1.0, d=0.6, h=0.75, mat="CT_TileWhite")
    t = tb["top"]
    box(d, "CT_SteelWhite", 0.45, 0.85, 13.6, 13.82, t, t + 0.2)
    box(d, "CT_Steel", 0.5, 0.8, 13.56, 13.6, t + 0.08, t + 0.16, faces="yxXzZ")
    for tx in (0.38, 1.0):
        box(d, "CT_Steel", tx - 0.015, tx + 0.015, 13.5, 13.54, t + 0.1, t + 0.12)
        box(d, "CT_Glass", tx - 0.04, tx + 0.04, 13.42, 13.5, t, t + 0.08)
    box(d, "CT_PlasterBlue", 0.6, 0.8, 13.66, 13.78, t + 0.2, t + 0.3)
    kit.stool(B, 0.45, 13.05, 0.0, 0.0, h=0.45, mat="CT_PlasterRed")
    kit.stool(B, 0.95, 13.05, 0.0, 0.0, h=0.45, mat="CT_PlasterBlue")
    kit.screen_divider(B, 0.7, 14.45, 0.0, 0.0)
    B.thing("inhaler", "machine", (0.65, 13.7, t + 0.15), 0.0, what="inhaler")
    B.spot("inhaler_stool", "seat", (0.45, 13.05, 0.0), 180.0)
    # ---- the dentist's door on the partition, hall side
    box(d, "CT_PlasterGrey", 3.24, 3.3, 14.15, 15.1, 0.0, 2.15)
    box(d, "CT_ConcreteDark", 3.2, 3.24, 14.1, 15.15, 2.15, 2.22)
    box(d, "CT_ConcreteDark", 3.2, 3.24, 14.1, 14.15, 0.0, 2.15)
    box(d, "CT_ConcreteDark", 3.2, 3.24, 15.1, 15.15, 0.0, 2.15)
    box(d, "CT_TileWhite", 3.2, 3.24, 14.45, 14.8, 1.55, 1.8)
    box(d, "CT_SteelRed", 3.19, 3.2, 14.55, 14.7, 1.62, 1.73)
    box(d, "CT_Brass", 3.19, 3.24, 14.28, 14.34, 1.0, 1.1)
    B.thing("dentist_door", "board", (3.18, 14.63, 1.4), 270.0, topic="dentist")
    B.spot("dentist_wait", "stand", (2.3, 14.2, 0.0), 90.0)
    B.spot("dentist_wait_2", "bench", (2.6, 13.9, 0.0), 90.0)
    # ---- card file at the desk's west end
    cf = kit.card_catalogue(B, -8.7, 12.6, 0.0, 0.0)
    B.thing("card_file", "shelf", (-8.7, 12.4, 1.0), 0.0, topic="cards")
    B.spot("card_file", "stand", (-8.7, 11.7, 0.0), 180.0)
    # ---- the height rod on the front wall, between the window and the door
    box(d, "CT_PlasterYellow", -6.27, -6.13, 4.3, 4.33, 0.25, 2.05)
    for k in range(18):
        zt = 0.3 + k * 0.1
        wd = 0.14 if k % 5 == 0 else 0.08
        box(d, "CT_Rubber", -6.2 - wd / 2, -6.2 + wd / 2, 4.33, 4.34, zt, zt + 0.012)
    box(d, "CT_SteelRed", -6.31, -6.09, 4.3, 4.37, 1.3, 1.34)
    B.thing("height_rod", "machine", (-6.2, 4.4, 1.2), 180.0, what="height")
    # ---- radio point (wall speaker) above the south wall bench
    box(d, "CT_Wood", 1.3, 1.7, 4.3, 4.4, 2.1, 2.4)
    box(d, "CT_Rubber", 1.35, 1.65, 4.4, 4.405, 2.15, 2.35, faces="y")
    box(d, "CT_Steel", 1.46, 1.54, 4.4, 4.41, 2.22, 2.28)
    B.thing("radio_point", "machine", (1.5, 4.4, 2.2), 180.0, what="radio_point")
    # ---- the magazine table
    B.thing("hall_table_mags", "shelf", (-8.4, 9.9, 0.45), 0.0, topic="waiting")
    # ---- pharmacy: apothecary scale on the counter, the recipe notice, jars thing
    box(d, "CT_Brass", 11.97, 12.07, 13.0, 13.2, 1.05, 1.08)
    box(d, "CT_Brass", 12.0, 12.04, 13.08, 13.12, 1.08, 1.3)
    box(d, "CT_Brass", 11.98, 12.06, 12.9, 13.3, 1.28, 1.3)
    for sy in (12.92, 13.28):
        box(d, "CT_Brass", 11.99, 12.05, sy - 0.04, sy + 0.04, 1.16, 1.17)
    B.thing("pharmacy_scale", "machine", (12.02, 13.1, 1.2), 270.0, what="apothecary")
    kit.poster(B, 13.7, 12.4, 0.0, 270.0, name="notice_1", w=0.45, zc=1.75)
    B.thing("pharmacy_recipes", "board", (13.65, 12.4, 1.75), 270.0, topic="recipes")
    B.thing("pharmacy_jars", "board", (13.3, 15.25, 1.5), 270.0, topic="jars")
    # ---- door plates by the three doors of the doctors' rooms
    for i, c in enumerate(cx):
        kit.poster(B, c + 0.85, 11.3, 0.0, 180.0, name="notice_2" if i != 1 else "notice_0", w=0.26, zc=1.6)
        B.thing("door_plate_%d" % i, "board", (c + 0.85, 11.35, 1.6), 180.0, topic="plate")
    # ---- mood: radiators under the hall's street windows, a different curtain in each doctor's room
    for rx in (-8.25, -0.35):
        kit.radiator(B, rx, 4.3, 0.0, 180.0, w=1.6, h=0.55)
    for c, cm in zip(cx, ("CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen")):
        kit.curtains(B, c, 4.3, 0.0, 180.0, w=1.5, h=2.55, mat=cm, open_=0.45)
    # ---- the doctors' desks: blood-pressure cuff, trial lenses and colour plates, reflex hammer
    for i, c in enumerate(cx):
        x = c - 0.45
        if i == 0:
            box(d, "CT_SteelWhite", x - 0.08, x + 0.08, 5.28, 5.4, 0.75, 0.85)
            box(d, "CT_Rubber", x + 0.1, x + 0.2, 5.3, 5.36, 0.75, 0.8)
            box(d, "CT_Glass", x - 0.03, x + 0.03, 5.25, 5.28, 0.8, 0.84)
            B.thing("bp_cuff", "machine", (x, 5.34, 0.85), 180.0, what="tonometer")
        elif i == 1:
            box(d, "CT_Leather", x - 0.14, x + 0.14, 5.25, 5.45, 0.75, 0.81)
            for k in range(5):
                box(d, "CT_Glass", x - 0.11 + k * 0.055, x - 0.07 + k * 0.055, 5.3, 5.34, 0.81, 0.815)
            kit.books_stack(B, x + 0.3, 5.3, 0.75, 20, n=2, seed=11)
            B.thing("eye_plates", "board", (x, 5.34, 0.85), 180.0, topic="plates")
        else:
            box(d, "CT_Rubber", x - 0.09, x + 0.09, 5.3, 5.33, 0.75, 0.77)
            box(d, "CT_Steel", x - 0.07, x + 0.07, 5.31, 5.32, 0.77, 0.79)
            box(d, "CT_Steel", x + 0.12, x + 0.3, 5.3, 5.33, 0.75, 0.765)
            box(d, "CT_Rubber", x + 0.3, x + 0.36, 5.29, 5.34, 0.75, 0.78)
            B.thing("reflex_hammer", "machine", (x + 0.1, 5.32, 0.85), 180.0, what="hammer")


def _depth3(B, cx):
    """Third pass: lost-property cubbies and a column scale in the hall, a bean-bag toss and a child's drawing by the
    children's corner, the corridor's duty rota and wall newspaper above the queue benches, the pharmacy bell and
    the steam steriliser on the filing cabinets of the first two rooms."""
    d = B.detail
    # ---- lost property: a pigeonhole cabinet on the west wall (front +x), things left behind in it
    x0, x1, ya, yb = -13.7, -13.35, 4.85, 5.85
    box(d, "CT_Wood", x0, x1, ya, ya + 0.03, 0.0, 1.3)
    box(d, "CT_Wood", x0, x1, yb - 0.03, yb, 0.0, 1.3)
    box(d, "CT_Wood", x0, x1, 5.335, 5.365, 0.0, 1.3)
    box(d, "CT_Wood", x0, x0 + 0.02, ya, yb, 0.0, 1.3)
    for zb in (0.0, 0.43, 0.86, 1.27):
        box(d, "CT_Wood", x0, x1, ya, yb, zb, zb + 0.03)
    xc = -13.52
    box(d, "CT_PlasterRed", xc - 0.1, xc + 0.1, 5.0, 5.28, 0.03, 0.2)                    # a knitted hat
    box(d, "CT_PlasterRed", xc - 0.06, xc + 0.06, 5.07, 5.21, 0.2, 0.25)
    box(d, "CT_PlasterBlue", xc - 0.08, xc + 0.08, 5.42, 5.6, 0.03, 0.05)               # a mitten pair
    box(d, "CT_PlasterBlue", xc - 0.07, xc + 0.07, 5.62, 5.8, 0.03, 0.05)
    box(d, "CT_Steel", xc - 0.01, xc + 0.01, 5.08, 5.1, 0.46, 0.84)                      # an umbrella
    box(d, "CT_PlasterGreen", xc - 0.03, xc + 0.03, 5.0, 5.2, 0.46, 0.5)
    box(d, "CT_PlasterYellow", xc - 0.12, xc + 0.12, 5.42, 5.78, 0.46, 0.6)              # a folded scarf
    box(d, "CT_PlasterBlue", xc - 0.12, xc + 0.12, 5.42, 5.78, 0.52, 0.54)
    box(d, "CT_Leather", xc - 0.09, xc + 0.09, 5.0, 5.26, 0.89, 0.97)                    # a book left on a bench
    box(d, "CT_Paper", xc - 0.08, xc + 0.08, 5.0, 5.26, 0.97, 0.985)
    box(d, "CT_Glass", xc - 0.05, xc + 0.05, 5.45, 5.75, 0.89, 0.91)                     # spectacles without an arm
    box(d, "CT_Steel", xc - 0.05, xc + 0.05, 5.45, 5.75, 0.91, 0.915, faces="Z")
    kit.poster(B, -13.7, 5.35, 0.0, 90.0, name="notice_2", w=0.4, zc=1.65)
    B.thing("lost_found", "shelf", (-13.5, 5.35, 1.0), 90.0, topic="lost")
    B.spot("lost_look", "stand", (-12.7, 5.4, 0.0), 270.0)
    # ---- a column scale between the radiator and the palm
    kit.scales(B, -9.6, 5.1, 0.0, 0.0)
    B.thing("hall_scales", "machine", (-9.6, 5.15, 1.0), 0.0, what="scales")
    B.spot("scales_wait", "stand", (-9.6, 5.95, 0.0), 180.0)
    # ---- a bean-bag toss board east of the children's rug (its face -x), a basket of bags, a throwing line
    bx = -9.2
    box(d, "CT_PlasterBlue", bx, bx + 0.05, 13.55, 14.45, 0.4, 1.2)
    box(d, "CT_Wood", bx, bx + 0.05, 13.5, 13.55, 0.0, 1.25)
    box(d, "CT_Wood", bx, bx + 0.05, 14.45, 14.5, 0.0, 1.25)
    box(d, "CT_Wood", bx, bx + 0.05, 13.5, 14.5, 1.2, 1.25)
    box(d, "CT_Wood", bx - 0.02, bx + 0.12, 13.5, 14.5, 0.0, 0.04)
    for k, (yy, mt) in enumerate(((13.8, "CT_SteelRed"), (14.0, "CT_PlasterYellow"), (14.2, "CT_PlasterGreen"))):
        zz = 0.55 + 0.25 * (k % 2)
        box(d, mt, bx - 0.006, bx, yy - 0.1, yy + 0.1, zz - 0.1, zz + 0.1)
        box(d, "CT_Rubber", bx - 0.012, bx - 0.006, yy - 0.065, yy + 0.065, zz - 0.065, zz + 0.065)
    box(d, "CT_Wood", -9.85, -9.55, 14.55, 14.85, 0.0, 0.22)
    for k, mt in enumerate(("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterGreen")):
        box(d, mt, -9.8 + 0.09 * k, -9.72 + 0.09 * k, 14.6 + 0.07 * k, 14.7 + 0.07 * k, 0.22, 0.27)
    box(d, "CT_Rubber", -10.41, -10.39, 13.4, 14.6, 0.0, 0.006, faces="Z")
    B.thing("hall_bagtoss", "machine", (bx - 0.02, 14.0, 0.9), 270.0, what="toss")
    B.spot("bagtoss_stand", "stand", (-10.4, 14.0, 0.0), 270.0)
    # ---- a child's drawing on the west wall between the rack and the bookshelf
    kit.poster(B, -13.7, 13.65, 0.0, 90.0, name="paper_drawing", w=0.45, zc=1.65)
    B.thing("kids_drawings", "board", (-13.65, 13.65, 1.65), 90.0, topic="drawings")
    # ---- corridor: duty rota and wall newspaper on the wall above the two queue benches
    kit.notice_board(B, 6.0, 15.7, 0.0, 0.0, w=1.0, zc=1.75, seed=4)
    B.thing("corridor_rota", "board", (6.0, 15.65, 1.75), 0.0, topic="rota")
    B.spot("rota_read", "stand", (6.0, 14.6, 0.0), 180.0)
    kit.poster(B, 9.75, 15.7, 0.0, 0.0, name="news_1", w=0.9, zc=1.75)
    B.thing("corridor_paper", "board", (9.75, 15.65, 1.75), 0.0, topic="wallpaper")
    B.spot("paper_read", "stand", (9.75, 14.6, 0.0), 180.0)
    # ---- the pharmacy bell on the counter
    box(d, "CT_Brass", 11.95, 12.05, 13.63, 13.73, 1.05, 1.065)
    box(d, "CT_Brass", 11.98, 12.02, 13.66, 13.70, 1.065, 1.1)
    box(d, "CT_SteelRed", 11.99, 12.01, 13.67, 13.69, 1.1, 1.115)
    B.thing("pharmacy_bell", "machine", (12.0, 13.68, 1.1), 270.0, what="bell")
    B.spot("bell_wait", "stand", (11.2, 13.4, 0.0), 270.0)
    # ---- steam steriliser on the filing cabinets of the first two rooms
    for i in (0, 1):
        ex = (6.8, 10.3)[i]
        box(d, "CT_SteelWhite", ex - 0.45, ex - 0.05, 8.5, 8.9, 1.32, 1.5)
        box(d, "CT_Steel", ex - 0.46, ex - 0.04, 8.49, 8.91, 1.5, 1.52)
        box(d, "CT_SteelRed", ex - 0.46, ex - 0.45, 8.62, 8.78, 1.38, 1.44)
        B.thing("sterilizer_%d" % i, "machine", (ex - 0.25, 8.7, 1.52), 270.0, what="sterilizer")


def _plate(d, x, y, z, side="S"):
    """A light-switch plate on the south wall (faces +y, side S) with a rocker."""
    box(d, "CT_FrameWhite", x - 0.04, x + 0.04, 4.3, 4.312, z - 0.06, z + 0.06)
    box(d, "CT_Rubber", x - 0.014, x + 0.014, 4.312, 4.322, z - 0.03, z + 0.03)


def _ball(d, x, y, z, r, mat):
    """A low-poly ball: stacked round prisms (x, y = the middle, z = the bottom)."""
    for k, (dz, rr) in enumerate(((0.0, 0.55), (0.18, 0.92), (0.5, 1.0), (0.82, 0.92), (1.0, 0.55))):
        pass
    s = 2 * r
    for dz0, dz1, rr in ((0.0, 0.2, 0.62), (0.2, 0.8, 1.0), (0.8, 1.0, 0.62)):
        B_col(d, x, y, z + dz0 * s, z + dz1 * s, r * rr, mat)


def B_col(d, x, y, z0, z1, r, mat, segs=8):
    poly = [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
    prism(d, mat, poly, z0, z1, top=True, bottom=False)


def _depth2(B, cx):
    """Round 2: every doctor's room a specialist's (therapist, eye doctor, children's doctor), the physio corner and the
    nurse's vaccination corner in the hall, switches, the umbrella stand, the porter's table, the queue's standing places."""
    d = B.detail
    wxs, exs = (3.6, 7.1, 10.6), (6.8, 10.3, 13.7)
    # ---- a painted dado in every room, in its own colour (the rooms differ at a glance)
    for i, (c, wx, ex) in enumerate(zip(cx, wxs, exs)):
        mat = ("CT_PlasterGreen", "CT_PlasterBlue", "CT_PlasterPink")[i]
        box(d, mat, wx, wx + 0.025, 4.3, 10.99, 0.12, 0.85, faces="xXyYZ")
        box(d, mat, ex - 0.025, ex, 4.3, 10.99, 0.12, 0.85, faces="xXyYZ")
        box(d, mat, wx, c - 0.6, 10.975, 11.0, 0.12, 0.85, faces="xXyYZ")
        box(d, mat, c + 0.6, ex, 10.975, 11.0, 0.12, 0.85, faces="xXyYZ")
        box(d, mat, wx, ex, 4.3, 4.325, 0.12, 0.85, faces="xXyYZ")
        for a, b, y0, y1 in ((wx, wx + 0.035, 4.3, 10.99), (ex - 0.035, ex, 4.3, 10.99)):
            box(d, "CT_Wood", a, b, y0, y1, 0.85, 0.88)
        # switch beside the window, a plate on the wall (a dev/hero switch for this room's lamp only)
        _plate(d, c - 0.88, 4.3, 1.3)
        B.thing("room_switch_%d" % i, "machine", (c - 0.88, 4.34, 1.3), 180.0, what="switch", switch=4.2)

    # ============================================================== room 0: the therapist (green)
    c, wx, ex = cx[0], wxs[0], exs[0]
    kit.bookshelf(B, wx, 6.2, 0.0, 90.0, w=0.9, h=1.9, levels=5, topic="medicine", seed=3)
    B.thing("therapist_books", "shelf", (wx + 0.2, 6.2, 1.0), 90.0, topic="medicine")
    kit.display_case(B, ex - 0.25, 5.65, 0.0, 270.0, w=0.9, d=0.45, goods="pharmacy", mat="CT_SteelWhite", seed=2)
    kit.wall_shelf(B, ex, 5.65, 0.0, 270.0, w=0.8, zt=1.75, seed=4)
    B.thing("therapist_remedies", "board", (ex - 0.1, 5.65, 1.3), 270.0, topic="remedies")
    # stethoscope on the desk edge: the chest piece, a loop of tube, two ear tips
    sx, sy = c - 0.2, 5.58
    B_col(d, sx, sy, 0.75, 0.765, 0.035, "CT_Steel")
    B_col(d, sx, sy, 0.765, 0.775, 0.022, "CT_Brass")
    for k in range(4):
        a = k * math.pi / 3.0
        box(d, "CT_Rubber", sx - 0.012 + 0.07 * math.cos(a) * 0 + (k - 1.5) * 0.045, sx + 0.012 + (k - 1.5) * 0.045,
            sy - 0.03 + 0.05 * (1 if k in (0, 3) else 2) * 0.5, sy + 0.01 + 0.05 * (1 if k in (0, 3) else 2) * 0.5, 0.75, 0.765)
    box(d, "CT_Steel", sx - 0.08, sx - 0.06, sy + 0.07, sy + 0.09, 0.75, 0.77)
    B.thing("therapist_stethoscope", "machine", (sx, sy, 0.8), 180.0, what="stethoscope")
    # prescription pad and a blotter
    box(d, "CT_Paper", c + 0.22, c + 0.4, 5.5, 5.7, 0.75, 0.762, faces="xXyYZ")
    box(d, "CT_SteelRed", c + 0.22, c + 0.24, 5.5, 5.7, 0.75, 0.764)
    B.thing("therapist_pad", "board", (c + 0.31, 5.6, 0.85), 180.0, topic="prescription")
    B.thing("doctor_talk_0", "counter", (c + 0.5, 5.85, 0.95), 180.0, role="doctor", service="doctor")
    kit.rug(B, 5.2, 8.6, 0.0, 0.0, w=1.5, d=2.0, name="rug_1", seed=21)
    B.spot("therapist_listen", "stand", (4.9, 8.4, 0.0), 0.0)

    # ============================================================== room 1: the eye doctor (blue)
    c, wx, ex = cx[1], wxs[1], exs[1]
    # the lit chart gets its own lamp; a floor tape marks the five-metre line under the eye chair's feet
    B.lamp((c + 1.1, 10.3, 2.75), lumens=380, colour="#DDEBFF", kind="public", radius=3.2, fixture=False)
    box(d, "CT_SteelWhite", c + 0.75, c + 1.45, 10.2, 10.4, 2.62, 2.72)
    box(d, "CT_Opal", c + 0.8, c + 1.4, 10.22, 10.38, 2.6, 2.62)
    box(d, "CT_SteelRed", c + 0.55, c + 1.45, 6.02, 6.06, 0.0, 0.004, faces="Z")
    # slit lamp on the desk: base, column, chin rest, the microscope arm
    lx, ly = c + 0.3, 5.3
    box(d, "CT_Steel", lx - 0.16, lx + 0.16, ly - 0.14, ly + 0.14, 0.75, 0.78)
    box(d, "CT_SteelWhite", lx - 0.03, lx + 0.03, ly - 0.02, ly + 0.04, 0.78, 1.0)
    box(d, "CT_SteelWhite", lx - 0.04, lx + 0.04, ly - 0.2, ly + 0.02, 1.0, 1.06)
    box(d, "CT_Steel", lx - 0.12, lx - 0.06, ly - 0.16, ly - 0.1, 0.8, 0.95)
    box(d, "CT_Steel", lx + 0.06, lx + 0.12, ly - 0.16, ly - 0.1, 0.8, 0.95)
    box(d, "CT_Rubber", lx - 0.12, lx + 0.12, ly - 0.17, ly - 0.09, 0.95, 0.975)
    box(d, "CT_Glass", lx - 0.03, lx + 0.03, ly - 0.24, ly - 0.2, 1.0, 1.06)
    B.thing("eye_slit_lamp", "machine", (lx, ly, 0.95), 180.0, what="slit_lamp")
    # the lens box open on the desk's other end: trays of lenses
    box(d, "CT_Wood", c - 0.28, c + 0.02, 5.2, 5.55, 0.75, 0.82, faces="xXyYZ")
    for k in range(6):
        box(d, "CT_Glass", c - 0.26 + k * 0.045, c - 0.235 + k * 0.045, 5.25, 5.5, 0.82, 0.832)
        box(d, "CT_Brass", c - 0.26 + k * 0.045, c - 0.235 + k * 0.045, 5.25, 5.28, 0.832, 0.85)
    # eye-glasses frames on a wall shelf over the sink side, a pointer by the chart
    kit.wall_shelf(B, ex, 8.1, 0.0, 270.0, w=0.7, zt=1.6, seed=5)
    for k, yy in enumerate((7.9, 8.1, 8.3)):
        box(d, "CT_Rubber", ex - 0.2, ex - 0.17, yy - 0.05, yy + 0.05, 1.62, 1.68, faces="xXyYZ")
        box(d, "CT_Glass", ex - 0.185, ex - 0.18, yy - 0.04, yy + 0.04, 1.63, 1.67)
    B.thing("eye_frames", "board", (ex - 0.15, 8.1, 1.7), 270.0, topic="frames")
    box(d, "CT_Wood", c + 0.78, c + 0.82, 10.84, 10.88, 0.1, 1.5)
    B.thing("eye_pointer", "machine", (c + 0.8, 10.82, 1.2), 0.0, what="eye_test")
    B.thing("doctor_talk_1", "counter", (c + 0.5, 5.85, 0.95), 180.0, role="doctor", service="doctor")
    B.spot("eye_pointer", "stand", (c + 0.6, 10.1, 0.0), 180.0)
    kit.rug(B, c - 0.4, 8.7, 0.0, 0.0, w=1.2, d=1.6, name="rug_2", seed=24)

    # ============================================================== room 2: the children's doctor (pink, toys)
    c, wx, ex = cx[2], wxs[2], exs[2]
    kit.rug(B, c + 0.4, 7.0, 0.0, 0.0, w=2.0, d=2.0, round=True, seed=31)
    kit.toy_house(B, ex - 0.7, 8.1, 0.0, 0.0)
    kit.toy_bin(B, ex - 0.55, 5.85, 0.0, 0.0, seed=2)
    B.thing("toy_chest", "machine", (ex - 0.55, 5.85, 0.45), 0.0, what="toy_chest")
    # baby scales on a low table at the east wall: a dish with a sliding weight beam
    tb = kit.table(B, ex - 0.3, 9.95, 0.0, 270.0, w=0.8, d=0.5, h=0.7, mat="CT_TileWhite", solid=False)
    box(d, "CT_SteelWhite", ex - 0.5, ex - 0.12, 9.7, 10.2, 0.7, 0.76, faces="xXyYZ")
    box(d, "CT_Steel", ex - 0.46, ex - 0.2, 9.74, 10.16, 0.76, 0.78)
    box(d, "CT_Brass", ex - 0.14, ex - 0.1, 9.74, 10.16, 0.76, 0.9)
    box(d, "CT_Brass", ex - 0.16, ex - 0.08, 9.74, 10.16, 0.88, 0.9)
    B.thing("baby_scales", "machine", (ex - 0.3, 9.95, 0.85), 270.0, what="baby_scales")
    # height and growth chart on the west wall: a giraffe board, ticks every 10 cm, a stamped line for each visit
    box(d, "CT_PlasterYellow", wx, wx + 0.03, 5.45, 5.95, 0.3, 1.5, faces="xXyYZ")
    for k in range(13):
        zt = 0.35 + k * 0.095
        box(d, "CT_Rubber", wx + 0.03, wx + 0.036, 5.5 + (0.08 if k % 5 else 0.0), 5.9, zt, zt + 0.012)
    for yy, zz in ((5.62, 0.52), (5.74, 0.92), (5.84, 1.2)):
        box(d, "CT_SteelRed", wx + 0.03, wx + 0.04, yy - 0.025, yy + 0.025, zz, zz + 0.03)
    B.thing("growth_chart", "machine", (wx + 0.05, 5.7, 1.0), 90.0, what="growth")
    # the reaction ruler on the desk: a ruler hanging over a cup of pencils
    rx = c + 0.1
    box(d, "CT_TileWhite", rx - 0.035, rx + 0.035, 5.4, 5.46, 0.75, 0.84)
    box(d, "CT_Wood", rx - 0.012, rx + 0.012, 5.4, 5.402, 0.84, 1.14, faces="yYxX")
    B.thing("reflex_ruler", "machine", (rx, 5.42, 1.0), 180.0, what="ruler")
    B.thing("doctor_talk_2", "counter", (c + 0.5, 5.85, 0.95), 180.0, role="doctor", service="doctor")
    # the mobile over the couch: a cross bar on a cord, four shapes on threads
    mx, my = c - 0.35, 8.2
    box(d, "CT_Paper", mx - 0.004, mx + 0.004, my - 0.004, my + 0.004, 2.75, 3.4)
    box(d, "CT_Wood", mx - 0.3, mx + 0.3, my - 0.01, my + 0.01, 2.75, 2.77)
    box(d, "CT_Wood", mx - 0.01, mx + 0.01, my - 0.3, my + 0.3, 2.77, 2.79)
    for (ox, oy, m_) in ((-0.3, 0.0, "CT_PlasterRed"), (0.3, 0.0, "CT_PlasterBlue"), (0.0, -0.3, "CT_PlasterYellow"),
                         (0.0, 0.3, "CT_PlasterGreen")):
        box(d, "CT_Paper", mx + ox - 0.002, mx + ox + 0.002, my + oy - 0.002, my + oy + 0.002, 2.4, 2.75)
        box(d, m_, mx + ox - 0.05, mx + ox + 0.05, my + oy - 0.05, my + oy + 0.05, 2.3, 2.4)
    B.thing("kids_mobile", "board", (mx, my, 2.5), 0.0, topic="mobile")
    # a frieze of coloured squares over the dado of the children's room (west wall only: the east wall has a window)
    for k in range(10):
        m_ = ("CT_PlasterRed", "CT_PlasterBlue", "CT_PlasterYellow", "CT_PlasterGreen")[k % 4]
        yy = 4.55 + k * 0.62
        if 5.35 <= yy <= 6.0 or 7.1 <= yy <= 9.3:
            continue                                        # the growth chart and the couch's picture
        z0 = 1.95 if k % 2 else 2.2
        box(d, m_, wx, wx + 0.03, yy, yy + 0.2, z0, z0 + 0.2)
    B.spot("kids_room_play", "stand", (c + 0.4, 7.0, 0.0), 0.0)
    B.spot("growth_chart", "stand", (wx + 0.75, 5.7, 0.0), 270.0)

    # ============================================================== the hall
    # the umbrella stand by the door (rain: wet umbrellas) and its drip tray
    ux, uy = -3.65, 4.78
    box(d, "CT_Steel", ux - 0.17, ux + 0.17, uy - 0.17, uy + 0.17, 0.0, 0.03, faces="xXyYZ")
    B_col(d, ux, uy, 0.03, 0.55, 0.15, "CT_SteelRed")
    B_col(d, ux, uy, 0.55, 0.57, 0.16, "CT_Steel")
    for k, (ox, oy, tilt, m_) in enumerate(((-0.06, 0.0, -0.1, "CT_PlasterBlue"), (0.05, 0.04, 0.12, "CT_Leather"),
                                           (0.0, -0.06, 0.04, "CT_PlasterGreen"))):
        box(d, m_, ux + ox - 0.025 + tilt * 0.2, ux + ox + 0.025 + tilt * 0.2, uy + oy - 0.025, uy + oy + 0.025, 0.2, 0.95)
        box(d, "CT_Wood", ux + ox - 0.012 + tilt * 0.2, ux + ox + 0.012 + tilt * 0.2, uy + oy - 0.012, uy + oy + 0.012, 0.95, 1.05)
    box(d, "CT_Glass", ux - 0.3, ux + 0.3, uy - 0.3, uy + 0.3, 0.0, 0.006, faces="Z")
    B.thing("umbrella_stand", "machine", (ux, uy, 0.7), 0.0, what="umbrellas")
    B.spot("umbrella_stand", "stand", (ux, 5.45, 0.0), 0.0)
    # the hall switch on the front wall beside the door (all the building's lamps)
    _plate(d, -3.3, 4.3, 1.35)
    B.thing("hall_switch", "machine", (-3.3, 4.34, 1.35), 180.0, what="switch", switch="hall")
    # the night porter's table by the radio: a thermos, a glass in a holder, a paper, his lamp, a folding cot behind
    tp = kit.table(B, 1.0, 4.85, 0.0, 0.0, w=0.8, d=0.5, h=0.75, solid=False)
    kit.teapot(B, 0.8, 4.85, 0.75, 20.0)
    kit.cup(B, 1.15, 4.9, 0.75, 0.0)
    kit.newspaper(B, 0.95, 5.0, 0.75, 8, seed=6)
    kit.table_lamp(B, 1.28, 4.7, 0.75, 0.0, shade="CT_PlasterGreen")
    kit.chair(B, 1.0, 5.5, 0.0, 0.0, kind="wood")
    B.thing("porter_table", "machine", (1.0, 4.9, 0.85), 0.0, what="porter")
    B.spot("porter_chair", "seat", (1.0, 5.5, 0.0), 0.0)
    # ---- the physio corner on the west wall: Swedish ladder, mat, a medicine ball, a low bench
    kit.wall_bars(B, -8.7, 15.7, 0.0, 0.0, w=0.8, h=2.4)
    kit.mats(B, -8.7, 14.6, 0.0, 0.0, w=1.2, d=1.1, n=1, mat="CT_PlasterBlue")
    _ball(d, -9.7, 14.3, 0.08, 0.13, "CT_PlasterRed")
    kit.sign_plate(B, -7.9, 15.7, 0.0, 0.0, name="sign_quiet", w=0.5, zc=2.45)
    B.thing("physio_bars", "sport", (-8.7, 15.55, 1.2), 0.0, what="physio_bars")
    B.thing("physio_ball", "machine", (-9.7, 14.3, 0.25), 90.0, what="physio_ball")
    B.spot("physio_bars", "stand", (-8.7, 14.3, 0.0), 180.0)
    B.spot("physio_ball", "stand", (-9.7, 13.6, 0.0), 180.0)
    # ---- the nurse's vaccination corner by the corridor gap: a stool, the tray table, the vaccine fridge, the schedule
    kit.table(B, 1.4, 11.55, 0.0, 0.0, w=0.9, d=0.5, h=0.8, kind="steel", mat="CT_SteelWhite", solid=False)
    box(d, "CT_Steel", 1.0, 1.3, 11.4, 11.7, 0.8, 0.815, faces="xXyYZ")
    box(d, "CT_TileWhite", 1.05, 1.25, 11.45, 11.65, 0.815, 0.835)
    box(d, "CT_Glass", 1.45, 1.55, 11.55, 11.65, 0.8, 0.9)
    box(d, "CT_SteelRed", 1.44, 1.56, 11.54, 11.66, 0.9, 0.92)
    box(d, "CT_Paper", 1.65, 1.8, 11.4, 11.62, 0.8, 0.808)
    B.thing("vaccine_tray", "machine", (1.15, 11.55, 0.9), 0.0, what="vaccination")
    B.thing("nurse_station", "counter", (1.62, 11.62, 0.9), 0.0, role="nurse", service="nurse")
    kit.stool(B, 1.4, 10.65, 0.0, 180.0, h=0.5, mat="CT_PlasterBlue")
    B.spot("vaccine_stool", "seat", (1.4, 10.65, 0.0), 180.0)
    B.spot("vaccine_nurse", "work", (1.4, 12.3, 0.0), 0.0)
    B.spot("vaccine_wait", "stand", (0.35, 10.7, 0.0), 0.0)
    kit.kitchen_unit(B, 2.3, 12.1, 0.0, 270.0, modules="fridge", wall_cupboards=False)
    B.thing("vaccine_fridge", "machine", (2.0, 12.1, 0.9), 270.0, what="vaccine_fridge")
    kit.poster(B, 3.3, 11.25, 0.0, 270.0, name="notice_1", w=0.45, zc=1.75)
    B.thing("vaccine_schedule", "board", (3.25, 11.25, 1.75), 270.0, topic="vaccines")
    kit.first_aid_box(B, 3.3, 10.9, 0.0, 270.0, zc=1.5)
    B.lamp((1.4, 11.3, 2.7), lumens=420, colour="#EAF4F2", kind="public", radius=3.4, fixture=False)
    box(d, "CT_Opal", 1.0, 1.8, 11.1, 11.5, 2.6, 2.65)
    B.thing("pharmacist_talk", "counter", (11.95, 14.78, 1.05), 270.0, role="nurse", service="pharmacy")
    # the card book on the registry desk (stamps of the rooms)
    box(d, "CT_PlasterBlue", -4.0, -3.76, 11.6, 11.85, 0.75, 0.77, faces="xXyYZ")
    box(d, "CT_Paper", -3.99, -3.77, 11.61, 11.84, 0.77, 0.78)
    B.thing("medical_card", "board", (-3.88, 11.72, 0.85), 0.0, topic="card")
    # ---- standing places of the queue: by the ticket stand, in line to the desk, by every doctor's door, the children
    B.spot("ticket_stand", "stand", (-7.4, 10.0, 0.0), 180.0)
    for k, (sx_, sy_) in enumerate(((-5.0, 9.7), (-4.3, 9.2), (-5.7, 9.2))):
        B.spot("queue_line_%d" % k, "stand", (sx_, sy_, 0.0), 180.0)
    for i, c_ in enumerate(cx):
        B.spot("door_wait_%d" % i, "stand", (c_, 11.9, 0.0), 0.0)
    B.spot("kids_play_0", "stand", (-12.5, 12.55, 0.0), 0.0)
    B.spot("kids_play_1", "stand", (-10.7, 14.3, 0.0), 270.0)
    B.spot("inhaler_wait", "stand", (1.6, 13.1, 0.0), 180.0)


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
