"""The ranger's hut, furniture and small things (hut_interior.glb): one object per piece, all in hut-local coordinates (every
object's origin is the hut origin). Floor top F = 0.40. See hut_shell.py for the room: inner x -1.88..1.88, y -1.68..3.28.

Layout (door in the front wall at x 0..1, the room's back at +Y):
  left wall:  table under the side window (x -1.82..-1.20, y 0..1.1), chair at its +X side facing -X, shelf, stove in the
              back-left corner (flue at -1.25, 2.92), wood box, stool
  back wall:  shelves with tins, jars, first-aid box, binoculars, rolled maps
  right wall: coat rack with an oilskin, the empty gun rack, the wall map, then the two-tier bunk (y 1.2..3.28)
  by the door: rubber boots; on the floor a rug. Free corridor x 0..1 from the door to the bunk, free floor round the table.
"""
import math

from mathutils import Matrix, Vector

from hut_common import (F, Mesh, bx, box, cyl, lathe, quad, softbox, tube_path, rot_x, rot_y, rot_z, about)

PI_, PO_ = "HUT_PlankIn", "HUT_PlankOut"
TABLE_TOP = F + 0.78
BULB = (0.15, -0.10, 2.26)
FLUE = (-1.25, 2.92)
MAP_QUAD = dict(x=1.859, y0=0.18, y1=1.12, z0=1.34, z1=2.04)


def stove():
    m = Mesh("Stove")
    fx, fy = FLUE
    bx(m, "HUT_Zinc", -1.80, -0.70, 2.30, 3.28, F, F + 0.006, faces="zZxXyY")              # the sheet under it
    bx(m, "HUT_Brick", -1.64, -0.86, 2.61, 3.23, F + 0.006, F + 0.74)
    bx(m, "HUT_Iron", -1.67, -0.83, 2.58, 3.26, F + 0.74, F + 0.775)                       # cast top plate
    cyl(m, (-1.05, 2.78, F + 0.775), (-1.05, 2.78, F + 0.79), 0.07, 0.07, "HUT_Iron", seg=14, tile=0.3)   # lid of a burner hole
    bx(m, "HUT_Iron", -1.40, -1.12, 2.585, 2.61, F + 0.30, F + 0.56)                       # firebox door
    bx(m, "HUT_Iron", -1.37, -1.15, 2.585, 2.60, F + 0.31, F + 0.55, faces="y")
    bx(m, "HUT_Brass", -1.20, -1.15, 2.57, 2.585, F + 0.40, F + 0.46)                      # handle
    bx(m, "HUT_Iron", -1.38, -1.14, 2.585, 2.605, F + 0.10, F + 0.22)                      # ash door
    bx(m, "HUT_Fire", -1.34, -1.18, 2.58, 2.5855, F + 0.155, F + 0.17)                     # the glow slit (emissive at night)
    for x in (-1.60, -0.90):                                                              # iron band round the body
        bx(m, "HUT_Iron", x - 0.012, x + 0.012, 2.598, 2.612, F + 0.006, F + 0.74, faces="yxX")
    cyl(m, (fx, fy, F + 0.775), (fx, fy, 2.70), 0.065, 0.065, "HUT_Iron", seg=12, caps=(False, False), tile=0.5)
    for z in (F + 1.05, 2.05):
        cyl(m, (fx, fy, z), (fx, fy, z + 0.05), 0.08, 0.08, "HUT_Iron", seg=12, tile=0.5)
    cyl(m, (fx - 0.1, fy, 1.60), (fx + 0.1, fy, 1.60), 0.008, 0.008, "HUT_Iron", seg=5, tile=0.3)     # the damper's rod
    cyl(m, (fx + 0.1, fy, 1.60), (fx + 0.14, fy, 1.60), 0.02, 0.02, "HUT_Black", seg=6, tile=0.3)
    return m


def kettle():
    m = Mesh("Kettle")
    c = (-1.50, 2.84, F + 0.79)
    lathe(m, [(0.0, 0.0), (0.10, 0.0), (0.115, 0.04), (0.11, 0.12), (0.08, 0.16), (0.05, 0.175), (0.0, 0.175)], "HUT_EnamelGreen", seg=16, c=c, caps=(False, False))
    lathe(m, [(0.0, 0.19), (0.016, 0.19), (0.016, 0.205), (0.0, 0.205)], "HUT_Black", seg=8, c=c)
    cyl(m, (c[0] + 0.09, c[1], c[2] + 0.07), (c[0] + 0.19, c[1], c[2] + 0.15), 0.028, 0.012, "HUT_EnamelGreen", seg=8, tile=0.3)   # spout
    arc = [(c[0] - 0.10 * math.cos(t), c[1], c[2] + 0.11 + 0.14 * math.sin(t)) for t in [math.pi * i / 8 for i in range(9)]]
    tube_path(m, arc, 0.007, "HUT_Black", seg=5)
    return m


def woodbox():
    m = Mesh("WoodBox")
    bx(m, PO_, -0.82, -0.38, 2.88, 3.22, F, F + 0.03, grain=0)
    for (x0, x1, y0, y1) in ((-0.82, -0.80, 2.88, 3.22), (-0.40, -0.38, 2.88, 3.22), (-0.80, -0.40, 2.88, 2.90), (-0.80, -0.40, 3.20, 3.22)):
        bx(m, PO_, x0, x1, y0, y1, F + 0.03, F + 0.32, grain=2)
    for i in range(4):
        for j in range(2):
            x = -0.74 + 0.11 * i
            z = F + 0.08 + 0.10 * j
            cyl(m, (x, 2.93, z), (x, 3.17, z), 0.045, 0.047, "HUT_LogOut", seg=7, endmat="HUT_LogEnd", wob=0.1, tile=1.0, rot0=m.rng.random())
    return m


def stool():
    m = Mesh("Stool")
    c = (-0.45, 2.42)
    cyl(m, (c[0], c[1], F + 0.40), (c[0], c[1], F + 0.435), 0.17, 0.17, PI_, seg=16, tile=1.2, endmat=PI_)
    for i in range(3):
        a = 2 * math.pi * i / 3 + 0.5
        top = (c[0] + 0.10 * math.cos(a), c[1] + 0.10 * math.sin(a), F + 0.40)
        bot = (c[0] + 0.17 * math.cos(a), c[1] + 0.17 * math.sin(a), F)
        cyl(m, bot, top, 0.022, 0.025, PI_, seg=6, tile=0.8)
    return m


def bunk():
    m = Mesh("Bunk")
    x0, x1, y0, y1 = 0.98, 1.88, 1.20, 3.28
    for (px, py) in ((x0, y0), (x1 - 0.07, y0), (x0, y1 - 0.07), (x1 - 0.07, y1 - 0.07)):
        bx(m, PI_, px, px + 0.07, py, py + 0.07, F, F + 1.78, grain=2)
    for zl in (0.40, 1.22):
        z = F + zl
        bx(m, PI_, x0 + 0.07, x1 - 0.07, y0 + 0.01, y1 - 0.01, z, z + 0.04, grain=1)            # the plank bed
        bx(m, PI_, x0, x0 + 0.04, y0, y1, z - 0.10, z + 0.02, grain=1)                           # side beams
        bx(m, PI_, x1 - 0.04, x1, y0, y1, z - 0.10, z + 0.02, grain=1)
        bx(m, PI_, x0 + 0.04, x1 - 0.04, y0, y0 + 0.03, z - 0.10, z + 0.02, grain=0)
        bx(m, PI_, x0 + 0.04, x1 - 0.04, y1 - 0.03, y1, z - 0.10, z + 0.02, grain=0)
    # foot board of the lower bed, head board; the upper bed's guard rails
    bx(m, PI_, x0 + 0.07, x1 - 0.07, y0 + 0.01, y0 + 0.035, F + 0.44, F + 0.60, grain=0)
    bx(m, PI_, x0 + 0.07, x1 - 0.07, y1 - 0.035, y1 - 0.01, F + 0.44, F + 0.62, grain=0)
    zt = F + 1.26
    bx(m, PI_, x0 + 0.005, x0 + 0.03, y0 + 0.30, y1 - 0.01, zt + 0.06, zt + 0.12, grain=1)
    bx(m, PI_, x0 + 0.005, x0 + 0.03, y0 + 0.30, y1 - 0.01, zt + 0.20, zt + 0.26, grain=1)
    bx(m, PI_, x0 + 0.07, x1 - 0.07, y1 - 0.035, y1 - 0.01, zt, zt + 0.24, grain=0)
    # ladder at the foot end
    for x in (1.25, 1.55):
        cyl(m, (x, 0.98, F), (x, 1.17, F + 1.42), 0.022, 0.022, PI_, seg=6, tile=0.8)
    for i in range(1, 6):
        t = i / 6.2
        y = 0.98 + 0.19 * t
        z = F + 1.42 * t
        cyl(m, (1.25, y, z), (1.55, y, z), 0.015, 0.015, PI_, seg=6, tile=0.8)
    return m


def bedding():
    ms = []
    mat_ = Mesh("Mattress")
    softbox(mat_, (1.43, 2.24, F + 0.485), (0.82, 1.92, 0.09), "HUT_Linen", bevel=0.02, seg=2)
    cyl(mat_, (1.02, 3.06, F + 1.36), (1.84, 3.06, F + 1.36), 0.10, 0.10, "HUT_Linen", seg=10, tile=0.6)      # rolled mattress, upper bed
    ms.append(mat_)
    bl = Mesh("Blanket")
    softbox(bl, (1.42, 1.86, F + 0.555), (0.80, 1.00, 0.05), "HUT_Wool", bevel=0.015, seg=2)
    softbox(bl, (1.0, 1.86, F + 0.50), (0.04, 1.00, 0.20), "HUT_Wool", bevel=0.012, seg=1)          # the hanging edge
    softbox(bl, (1.43, 2.20, F + 1.36 + 0.10 + 0.07), (0.72, 0.50, 0.14), "HUT_Wool", bevel=0.02, seg=2)  # folded one on the roll... on the upper bed
    ms.append(bl)
    pi = Mesh("Pillow")
    softbox(pi, (1.43, 3.00, F + 0.58), (0.52, 0.32, 0.11), "HUT_Linen", bevel=0.045, seg=3)
    ms.append(pi)
    return ms


def table():
    m = Mesh("Table")
    x0, x1, y0, y1 = -1.82, -1.20, 0.0, 1.10
    bx(m, PI_, x0, x1, y0, y1, TABLE_TOP - 0.04, TABLE_TOP, grain=1)
    bx(m, PI_, x0 + 0.04, x1 - 0.04, y0 + 0.04, y1 - 0.04, TABLE_TOP - 0.12, TABLE_TOP - 0.04, grain=1, faces="xXyYz")
    for (px, py) in ((x0 + 0.04, y0 + 0.04), (x1 - 0.09, y0 + 0.04), (x0 + 0.04, y1 - 0.09), (x1 - 0.09, y1 - 0.09)):
        bx(m, PI_, px, px + 0.05, py, py + 0.05, F, TABLE_TOP - 0.12, grain=2)
    bx(m, "HUT_Brass", x1 - 0.045, x1 - 0.035, 0.52, 0.58, TABLE_TOP - 0.09, TABLE_TOP - 0.06)     # a drawer knob
    return m


def radio():
    m = Mesh("Radio")
    x, y, z = -1.62, 0.84, TABLE_TOP
    bx(m, "HUT_Olive", x - 0.12, x + 0.12, y - 0.19, y + 0.19, z, z + 0.20)
    bx(m, "HUT_Black", x + 0.115, x + 0.125, y - 0.17, y + 0.17, z + 0.02, z + 0.18, faces="X")     # the front panel
    bx(m, "HUT_Paper", x + 0.125, x + 0.130, y - 0.15, y - 0.02, z + 0.11, z + 0.17)                 # dial scale
    for i, yy in enumerate((y + 0.03, y + 0.09, y + 0.15)):
        cyl(m, (x + 0.125, yy, z + 0.14), (x + 0.150, yy, z + 0.14), 0.017, 0.015, "HUT_Black", seg=8, tile=0.2)
    for yy in (y - 0.12, y - 0.04):
        cyl(m, (x + 0.125, yy, z + 0.05), (x + 0.145, yy, z + 0.05), 0.012, 0.012, "HUT_Brass", seg=6, tile=0.2)
    cyl(m, (x + 0.02, y + 0.12, z + 0.20), (x + 0.02, y + 0.12, z + 0.225), 0.012, 0.012, "HUT_Brass", seg=6, tile=0.2)   # terminals
    cyl(m, (x + 0.02, y + 0.06, z + 0.20), (x + 0.02, y + 0.06, z + 0.225), 0.012, 0.012, "HUT_Brass", seg=6, tile=0.2)
    return m


def handset():
    m = Mesh("Handset")
    x, y, z = -1.40, 0.62, TABLE_TOP
    box(m, (x, y, z + 0.022), (0.05, 0.19, 0.04), "HUT_Black", xf=about(rot_z(8), (x, y, 0)))
    for s in (-1, 1):
        box(m, (x, y + s * 0.095, z + 0.03), (0.07, 0.05, 0.06), "HUT_Black", xf=about(rot_z(8), (x, y, 0)))
    pts = [(x - 0.03, y + 0.1, z + 0.03)]
    for i in range(1, 9):
        t = i / 8
        pts.append((x - 0.03 - 0.17 * t, y + 0.1 + 0.15 * t, z + 0.008 + 0.02 * math.sin(math.pi * t * 4) ** 2 * 0.3))
    tube_path(m, pts, 0.0045, "HUT_Black", seg=5)
    return m


def logbook():
    m = Mesh("Logbook")
    x, y, z = -1.50, 0.34, TABLE_TOP
    bx(m, "HUT_Red", x - 0.105, x + 0.105, y - 0.15, y + 0.15, z, z + 0.028)
    bx(m, "HUT_Paper", x - 0.095, x + 0.100, y - 0.14, y + 0.14, z + 0.004, z + 0.024, faces="xXyY")
    cyl(m, (x - 0.06, y - 0.17, z + 0.032), (x + 0.07, y - 0.12, z + 0.032), 0.005, 0.005, "HUT_Black", seg=5, tile=0.2)    # a pencil
    return m


def mug():
    m = Mesh("Mug")
    c = (-1.30, 0.20, TABLE_TOP)
    lathe(m, [(0.0, 0.0), (0.04, 0.0), (0.043, 0.012), (0.043, 0.085), (0.045, 0.090), (0.0415, 0.090), (0.0405, 0.015), (0.0, 0.012)],
          ["HUT_Enamel", "HUT_Enamel", "HUT_Enamel", "HUT_EnamelBlue", "HUT_EnamelBlue", "HUT_Enamel", "HUT_Enamel"], seg=14, c=c, caps=(False, False))
    arc = [(c[0] + 0.043 + 0.035 * math.sin(t), c[1], c[2] + 0.05 + 0.035 * math.cos(t)) for t in [math.pi * (i / 6) - math.pi / 2 for i in range(7)]]
    arc = [(c[0] + 0.043 + 0.03 * math.cos(t), c[1], c[2] + 0.05 + 0.032 * math.sin(t)) for t in [-math.pi / 2 + math.pi * i / 6 for i in range(7)]]
    tube_path(m, arc, 0.006, "HUT_Enamel", seg=5)
    return m


def lamp():
    m = Mesh("KeroseneLamp")
    c = (-1.64, 0.14, TABLE_TOP)
    lathe(m, [(0.0, 0.0), (0.06, 0.0), (0.065, 0.02), (0.06, 0.07), (0.035, 0.10), (0.03, 0.12), (0.0, 0.12)], "HUT_Brass", seg=14, c=c, caps=(False, False))
    lathe(m, [(0.03, 0.12), (0.034, 0.16), (0.03, 0.17), (0.0, 0.17)], "HUT_Brass", seg=14, c=c, caps=(False, False))
    lathe(m, [(0.027, 0.17), (0.036, 0.21), (0.048, 0.25), (0.04, 0.30), (0.03, 0.36)], "HUT_LampGlass", seg=14, c=c, caps=(False, False))
    lathe(m, [(0.0, 0.17), (0.006, 0.17), (0.006, 0.22), (0.0, 0.235)], "HUT_Fire", seg=6, c=c, caps=(False, False))
    arc = [(c[0] + 0.065 + 0.03 * math.cos(t), c[1], c[2] + 0.06 + 0.04 * math.sin(t)) for t in [-math.pi / 2 + math.pi * i / 6 for i in range(7)]]
    tube_path(m, arc, 0.005, "HUT_Brass", seg=5)
    return m


def chair():
    m = Mesh("Chair")
    cx, cy = -0.95, 0.55
    sx0, sx1, sy0, sy1 = cx - 0.20, cx + 0.20, cy - 0.20, cy + 0.20
    zs_ = F + 0.44
    bx(m, PI_, sx0, sx1, sy0, sy1, zs_, zs_ + 0.035, grain=1)
    for (px, py) in ((sx0 + 0.01, sy0 + 0.01), (sx0 + 0.01, sy1 - 0.05), (sx1 - 0.05, sy0 + 0.01), (sx1 - 0.05, sy1 - 0.05)):
        bx(m, PI_, px, px + 0.04, py, py + 0.04, F, zs_, grain=2)
    for py in (sy0 + 0.01, sy1 - 0.05):                           # back posts (the back is on the +X side)
        bx(m, PI_, sx1 - 0.05, sx1 - 0.01, py, py + 0.04, zs_ + 0.035, F + 0.92, grain=2)
    for z in (F + 0.62, F + 0.72, F + 0.82):
        bx(m, PI_, sx1 - 0.045, sx1 - 0.015, sy0 + 0.04, sy1 - 0.04, z, z + 0.06, grain=1)
    for (a, b_) in ((sy0 + 0.03, sy1 - 0.03),):
        bx(m, PI_, sx0 + 0.03, sx1 - 0.03, a - 0.01, a + 0.01, F + 0.18, F + 0.21, grain=0)
        bx(m, PI_, sx0 + 0.03, sx1 - 0.03, b_ - 0.01, b_ + 0.01, F + 0.18, F + 0.21, grain=0)
    return m


def shelf_back():
    m = Mesh("ShelfBack")
    for zt in (1.40, 1.82):
        bx(m, PI_, -0.15, 0.85, 3.04, 3.28, zt - 0.03, zt, grain=0)
        for x in (-0.12, 0.82):
            bx(m, PI_, x - 0.015, x + 0.015, 3.06, 3.28, zt - 0.17, zt - 0.03, grain=2)
    return m


def tins():
    m = Mesh("Tins")
    for i in range(5):
        x = -0.05 + 0.10 * i
        h = 0.10 + 0.012 * (i % 3)
        v0 = (i % 4) / 4 + 0.01
        cyl(m, (x, 3.14, 1.40), (x, 3.14, 1.40 + h), 0.04, 0.04, "HUT_Tin", seg=12, band=(v0, v0 + 0.23), tile=0.25, rot0=0.3 * i)
    for i in range(3):
        x = 0.58 + 0.085 * i
        cyl(m, (x, 3.14, 1.82), (x, 3.14, 1.82 + 0.08), 0.034, 0.034, "HUT_Tin", seg=12, band=((i + 1) % 4 / 4 + 0.01, (i + 1) % 4 / 4 + 0.24), tile=0.25)
    return m


def jars():
    m = Mesh("Jars")
    for x, content in ((0.46, "HUT_Jam"), (0.58, "HUT_Honey"), (0.70, "HUT_Jam")):
        cyl(m, (x, 3.14, 1.40), (x, 3.14, 1.52), 0.045, 0.045, "HUT_JarGlass", seg=12, caps=(False, False), tile=0.3)
        cyl(m, (x, 3.14, 1.405), (x, 3.14, 1.49), 0.040, 0.040, content, seg=12, tile=0.3)
        cyl(m, (x, 3.14, 1.52), (x, 3.14, 1.535), 0.047, 0.047, "HUT_Zinc", seg=12, tile=0.3)
    return m


def firstaid():
    m = Mesh("FirstAidBox")
    x, y, z = 0.62, 3.13, 1.82
    bx(m, "HUT_Enamel", x - 0.11, x + 0.11, y - 0.06, y + 0.06, z, z + 0.15)
    bx(m, "HUT_Red", x - 0.03, x + 0.03, y - 0.0605, y - 0.0595 - 0.001, z + 0.04, z + 0.115, faces="y")
    bx(m, "HUT_Red", x - 0.05, x + 0.05, y - 0.0615, y - 0.0595, z + 0.062, z + 0.092, faces="y")
    bx(m, "HUT_Red", x - 0.03, x + 0.03, y - 0.0615, y - 0.0595, z + 0.04, z + 0.115, faces="y")
    bx(m, "HUT_Zinc", x - 0.012, x + 0.012, y - 0.064, y - 0.06, z + 0.125, z + 0.140)
    return m


def binoculars():
    m = Mesh("Binoculars")
    x, y, z = 0.05, 3.10, 1.82
    for s in (-1, 1):
        cyl(m, (x + s * 0.035, y - 0.07, z + 0.04), (x + s * 0.035, y + 0.06, z + 0.04), 0.032, 0.032, "HUT_Black", seg=10, tile=0.3, endmat="HUT_Black")
        cyl(m, (x + s * 0.035, y - 0.095, z + 0.04), (x + s * 0.035, y - 0.07, z + 0.04), 0.038, 0.034, "HUT_Black", seg=10, tile=0.3)
    bx(m, "HUT_Black", x - 0.04, x + 0.04, y - 0.03, y + 0.01, z + 0.03, z + 0.05)
    pts = [(x - 0.07, y - 0.0, z + 0.05), (x - 0.10, y + 0.01, z + 0.10), (x, y + 0.03, z + 0.14), (x + 0.10, y + 0.01, z + 0.10), (x + 0.07, y - 0.0, z + 0.05)]
    tube_path(m, pts, 0.006, "HUT_Leather", seg=4)
    return m


def maps():
    m = Mesh("RolledMaps")
    z0 = 1.82
    for (y, z, x1) in ((3.12, z0 + 0.03, 0.52), (3.19, z0 + 0.03, 0.46), (3.155, z0 + 0.085, 0.50)):
        cyl(m, (0.15, y, z), (x1, y, z), 0.032, 0.032, "HUT_Paper", seg=10, tile=0.25, caps=(True, True))
    for xx in (0.22, 0.42):
        cyl(m, (xx, 3.10, z0 + 0.035), (xx, 3.215, z0 + 0.035), 0.036, 0.036, "HUT_Hemp", seg=8, tile=0.2, caps=(False, False))
    return m


def shelf_left():
    m = Mesh("ShelfLeft")
    bx(m, PI_, -1.88, -1.64, 1.38, 2.30, 1.52, 1.55, grain=1)
    for y in (1.42, 2.26):
        bx(m, PI_, -1.88, -1.66, y - 0.015, y + 0.015, 1.36, 1.52, grain=2)
    for y in (1.62, 1.95):                                                    # pegs under it for cups
        cyl(m, (-1.88, y, 1.40), (-1.80, y, 1.40), 0.011, 0.011, PI_, seg=6, tile=0.5)
    return m


def pot():
    m = Mesh("Pot")
    c = (-1.76, 1.78, 1.55)
    lathe(m, [(0.0, 0.0), (0.11, 0.0), (0.12, 0.02), (0.125, 0.15), (0.12, 0.16), (0.11, 0.16), (0.11, 0.02), (0.0, 0.02)],
          "HUT_EnamelBlue", seg=16, c=c, caps=(False, False))
    lathe(m, [(0.125, 0.14), (0.125, 0.15), (0.12, 0.16)], "HUT_Enamel", seg=16, c=c, caps=(False, False))
    for s in (-1, 1):
        cyl(m, (c[0], c[1] + s * 0.125, c[2] + 0.12), (c[0], c[1] + s * 0.145, c[2] + 0.12), 0.01, 0.01, "HUT_Black", seg=6, tile=0.2)
    return m


def frying_pan():
    m = Mesh("FryingPan")
    c = (-1.86, 2.28, 1.40)
    lathe(m, [(0.0, 0.0), (0.10, 0.0), (0.12, 0.03), (0.125, 0.032), (0.12, 0.034), (0.095, 0.006), (0.0, 0.006)], "HUT_Iron", seg=16, c=(0, 0, 0),
          xf=Matrix.Translation(c) @ rot_y(90), caps=(False, False))
    cyl(m, (c[0] + 0.0, c[1] - 0.12, c[2] - 0.0), (c[0] + 0.0, c[1] - 0.12 - 0.2, c[2] - 0.0), 0.009, 0.009, "HUT_Iron", seg=6, tile=0.3)
    return m


def coat_rack():
    m = Mesh("CoatRack")
    bx(m, PI_, 1.84, 1.88, -1.58, -0.78, 1.70, 1.80, grain=1)
    for y in (-1.50, -1.20, -0.98, -0.82):
        cyl(m, (1.84, y, 1.75), (1.755, y, 1.75), 0.012, 0.012, PI_, seg=6, tile=0.5)
        cyl(m, (1.755, y, 1.75), (1.745, y, 1.75), 0.02, 0.02, PI_, seg=8, tile=0.5)
    return m


def oilskin():
    m = Mesh("Oilskin")
    c = (1.72, -1.20, 1.12)
    softbox(m, c, (0.13, 0.50, 1.10), "HUT_Oilskin", bevel=0.03, seg=2, xf=about(rot_y(2.5), (1.72, -1.2, 1.7)))
    for s in (-1, 1):
        sl = (1.71, -1.20 + s * 0.30, 1.28)
        softbox(m, sl, (0.10, 0.12, 0.62), "HUT_Oilskin", bevel=0.04, seg=2, xf=about(rot_x(s * -4.0), (1.71, -1.2 + s * 0.27, 1.58)))
    softbox(m, (1.70, -1.20, 1.62), (0.16, 0.34, 0.12), "HUT_Oilskin", bevel=0.05, seg=2)            # the hood
    return m


def cap():
    m = Mesh("Cap")
    c = (1.745, -0.98, 1.62)
    lathe(m, [(0.0, 0.0), (0.10, 0.0), (0.105, 0.02), (0.09, 0.06), (0.0, 0.08)], "HUT_Olive", seg=12, c=c, caps=(False, False),
          xf=None, sx=0.8, sy=1.0)
    box(m, (c[0] - 0.08, c[1], c[2] + 0.005), (0.10, 0.17, 0.012), "HUT_Olive")
    return m


def gun_rack():
    m = Mesh("GunRack")
    bx(m, PI_, 1.85, 1.88, -0.62, 0.12, 1.30, 2.12, grain=2, faces="xXyYzZ")
    for z in (1.50, 2.00):
        bx(m, PI_, 1.80, 1.85, -0.62, 0.12, z, z + 0.05, grain=1)
        for y in (-0.60, -0.38, -0.12, 0.10):
            bx(m, PI_, 1.77, 1.85, y - 0.015, y + 0.015, z, z + 0.09, grain=0)
        bx(m, "HUT_Leather", 1.775, 1.80, -0.58, 0.08, z + 0.05, z + 0.058)
    return m


def wall_map():
    m = Mesh("WallMap")
    bx(m, PI_, 1.862, 1.88, 0.12, 1.18, 1.28, 2.10, grain=1)                                  # the backing board
    q = MAP_QUAD
    x = q["x"]
    quad(m, [(x, q["y1"], q["z0"]), (x, q["y0"], q["z0"]), (x, q["y0"], q["z1"]), (x, q["y1"], q["z1"])], "HUT_Map",
         uvs=[(0, 0), (1, 0), (1, 1), (0, 1)], ref=(x + 1, 0.6, 1.7))
    for yy, zz in ((q["y0"] + 0.02, q["z1"] - 0.02), (q["y1"] - 0.02, q["z1"] - 0.02), (q["y0"] + 0.02, q["z0"] + 0.02), (q["y1"] - 0.02, q["z0"] + 0.02)):
        cyl(m, (x, yy, zz), (x - 0.012, yy, zz), 0.008, 0.008, "HUT_Brass", seg=6, tile=0.2)
    return m


def boots():
    m = Mesh("RubberBoots")
    for (x, y, yaw) in ((1.45, -1.44, 18), (1.47, -1.22, -8)):
        xf = about(rot_z(yaw), (x, y, F))
        softbox(m, (x, y - 0.05, F + 0.05), (0.11, 0.28, 0.10), "HUT_Rubber", bevel=0.035, seg=2, xf=xf)
        cyl(m, (x, y + 0.045, F + 0.05), (x, y + 0.055, F + 0.40), 0.062, 0.068, "HUT_Rubber", seg=10, tile=0.5,
            band=None, caps=(False, True))
        # the heel and the shaft's top rim
        bx(m, "HUT_Rubber", x - 0.045, x + 0.045, y + 0.05, y + 0.14, F, F + 0.03, faces="all")
    return m


def rug():
    m = Mesh("Rug")
    x0, x1, y0, y1, z = -0.65, 0.45, -0.20, 1.30, F + 0.012
    quad(m, [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], "HUT_Rug", uvs=[(0, 0), (1, 0), (1, 1), (0, 1)], ref=(0, 0.5, z - 1))
    for (a, b_, ca, cb) in (((x0, y0), (x1, y0), 0, -1), ((x1, y0), (x1, y1), 1, 0), ((x1, y1), (x0, y1), 0, 1), ((x0, y1), (x0, y0), -1, 0)):
        quad(m, [(a[0], a[1], F), (b_[0], b_[1], F), (b_[0], b_[1], z), (a[0], a[1], z)], "HUT_Rug", ref=((x0 + x1) / 2, (y0 + y1) / 2, F + 0.006), tile=1.0,
             uvs=None)
    return m


def ceiling_lamp():
    m = Mesh("CeilingLamp")
    x, y, z = BULB
    cyl(m, (x, y, 2.70), (x, y, z + 0.13), 0.005, 0.005, "HUT_Black", seg=5, caps=(False, False), tile=0.3)
    lathe(m, [(0.0, z + 0.08), (0.02, z + 0.08), (0.02, z + 0.14), (0.0, z + 0.14)], "HUT_Brass", seg=10, c=(x, y, 0), tile=0.2)
    lathe(m, [(0.12, z + 0.0), (0.025, z + 0.08), (0.02, z + 0.08), (0.116, z - 0.004)], "HUT_Enamel", seg=16, c=(x, y, 0), caps=(False, False), tile=0.4)
    lathe(m, [(0.0, z - 0.045), (0.025, z - 0.035), (0.04, z - 0.005), (0.045, z + 0.02), (0.025, z + 0.06), (0.012, z + 0.08)], "HUT_BulbGlass", seg=12,
          c=(x, y, 0), caps=(False, False), tile=0.3)
    return m


def build():
    """-> list of (object name, kind, Mesh)"""
    parts = [stove(), kettle(), woodbox(), stool(), bunk(), table(), radio(), handset(), logbook(), mug(), lamp(), chair(),
             shelf_back(), tins(), jars(), firstaid(), binoculars(), maps(), shelf_left(), pot(), frying_pan(), coat_rack(), oilskin(),
             cap(), gun_rack(), wall_map(), boots(), rug(), ceiling_lamp()] + bedding()
    return parts
