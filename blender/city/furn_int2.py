"""Small low-poly furniture helpers for the interiors of the enterable building halls (round 2, int-2).

Pure Python on top of citykit. Every helper places a piece at (x, y) on the floor z0 with a yaw in degrees using the
same convention as B.spot / B.entrance: yaw 0 = the piece FACES -y (a chair's sitter / a counter's customer looks toward
-y), 90 = faces +x, 180 = faces +y, 270 = faces -x. Solid pieces (tables, counters, shelves) go to B.m (collide),
small ones to B.detail. Colours are material slots only (CT_*).
"""
import math

from citykit import Mesh, box, prism, geom, ring

GOODS = ("CT_PlasterRed", "CT_PlasterYellow", "CT_PlasterBlue", "CT_PlasterGreen", "CT_TileWhite", "CT_Wood", "CT_PlasterPink")


class Furn:
    def __init__(self, B, z0=0.0):
        self.B, self.z0 = B, z0
        self.n = 0

    # -------------------------------------------------------------- placement
    def put(self, build_fn, x, y, yaw=0.0, solid=False):
        """build_fn(mesh) draws a piece in a local frame (origin on the floor, faces -y); it is rotated + moved."""
        m = Mesh()
        build_fn(m)
        fr = geom.Frame(x, y, math.radians(yaw), oz=self.z0)
        (self.B.m if solid else self.B.detail).extend(fr.apply(m))

    # -------------------------------------------------------------- seating
    def chair(self, x, y, yaw=0.0, seat="CT_Wood", frame="CT_Steel", h=0.45):
        def f(m):
            box(m, seat, -0.22, 0.22, -0.22, 0.22, h - 0.04, h)
            box(m, seat, -0.22, 0.22, 0.17, 0.2, h, h + 0.42)           # back at +y
            for dx in (-0.19, 0.19):
                for dy in (-0.19, 0.19):
                    box(m, frame, dx - 0.015, dx + 0.015, dy - 0.015, dy + 0.015, 0.0, h - 0.04)
        self.put(f, x, y, yaw)

    def stool(self, x, y, seat="CT_PlasterRed", h=0.62):
        def f(m):
            box(m, seat, -0.18, 0.18, -0.18, 0.18, h - 0.05, h)
            box(m, "CT_Steel", -0.02, 0.02, -0.02, 0.02, 0.0, h - 0.05)
            box(m, "CT_Steel", -0.16, 0.16, -0.16, 0.16, 0.0, 0.03)
        self.put(f, x, y, 0.0)

    def bench(self, x, y, yaw=0.0, w=1.8, seat="CT_Wood", back=False):
        def f(m):
            box(m, seat, -w / 2, w / 2, -0.24, 0.24, 0.42, 0.47)
            if back:
                box(m, seat, -w / 2, w / 2, 0.2, 0.25, 0.47, 0.85)
            for sx in (-1, 1):
                box(m, "CT_Steel", sx * (w / 2 - 0.12) - 0.04, sx * (w / 2 - 0.12) + 0.04, -0.2, 0.2, 0.0, 0.42)
        self.put(f, x, y, yaw, solid=True)

    def armchair(self, x, y, yaw=0.0, mat="CT_PlasterRed", w=0.8):
        def f(m):
            box(m, mat, -w / 2, w / 2, -0.4, 0.4, 0.0, 0.4)
            box(m, mat, -w / 2, w / 2, 0.28, 0.4, 0.4, 0.85)
            for sx in (-1, 1):
                box(m, mat, sx * (w / 2 - 0.08) - 0.08, sx * (w / 2 - 0.08) + 0.08, -0.4, 0.28, 0.4, 0.62)
        self.put(f, x, y, yaw, solid=True)

    def sofa(self, x, y, yaw=0.0, w=1.9, mat="CT_PlasterBlue"):
        def f(m):
            box(m, mat, -w / 2, w / 2, -0.4, 0.4, 0.0, 0.42)
            box(m, mat, -w / 2, w / 2, 0.26, 0.4, 0.42, 0.85)
            for sx in (-1, 1):
                box(m, mat, sx * (w / 2 - 0.1) - 0.1, sx * (w / 2 - 0.1) + 0.1, -0.4, 0.26, 0.42, 0.62)
        self.put(f, x, y, yaw, solid=True)

    # -------------------------------------------------------------- tables, desks, counters
    def table(self, x, y, w=1.2, d=1.2, h=0.75, top="CT_Wood", leg="CT_Steel", yaw=0.0):
        def f(m):
            box(m, top, -w / 2, w / 2, -d / 2, d / 2, h - 0.04, h)
            for sx in (-1, 1):
                for sy in (-1, 1):
                    box(m, leg, sx * (w / 2 - 0.07) - 0.03, sx * (w / 2 - 0.07) + 0.03,
                        sy * (d / 2 - 0.07) - 0.03, sy * (d / 2 - 0.07) + 0.03, 0.0, h - 0.04)
        self.put(f, x, y, yaw, solid=True)

    def round_table(self, x, y, r=0.55, h=0.75, top="CT_Wood"):
        def f(m):
            poly = ring(0, 0, r, 10)
            prism(m, top, poly, h - 0.04, h, top=True, bottom=True)
            prism(m, "CT_Steel", ring(0, 0, 0.05, 6), 0.0, h - 0.04, top=False, bottom=False)
            prism(m, "CT_Steel", ring(0, 0, 0.25, 8), 0.0, 0.03, top=True, bottom=False)
        self.put(f, x, y, 0.0, solid=True)

    def desk(self, x, y, yaw=0.0, w=1.4, d=0.65, h=0.74, top="CT_Wood"):
        """A desk whose user faces yaw (the user stands at local -y... the desk is on the user's +y side?). The user sits at
        local y = -d/2 - 0.3, facing +y... here, kept simple: the desk is centred at (x, y); drawers on the +x side."""
        def f(m):
            box(m, top, -w / 2, w / 2, -d / 2, d / 2, h - 0.04, h)
            box(m, "CT_FrameWhite", w / 2 - 0.45, w / 2 - 0.03, -d / 2 + 0.03, d / 2 - 0.03, 0.05, h - 0.04)
            box(m, "CT_Steel", -w / 2 + 0.03, -w / 2 + 0.07, -d / 2 + 0.03, d / 2 - 0.03, 0.0, h - 0.04)
        self.put(f, x, y, yaw, solid=True)

    def counter(self, x, y, w, d=0.7, h=1.05, yaw=0.0, mat="CT_Wood", top="CT_Steel", front=None):
        """A service counter: customers at local -y. Front face may use another material (a coloured fascia)."""
        def f(m):
            box(m, mat, -w / 2, w / 2, -d / 2, d / 2, 0.0, h - 0.04)
            if front:
                box(m, front, -w / 2 + 0.05, w / 2 - 0.05, -d / 2 - 0.02, -d / 2, 0.1, h - 0.15, faces="y")
            box(m, top, -w / 2 - 0.03, w / 2 + 0.03, -d / 2 - 0.05, d / 2 + 0.03, h - 0.04, h)
        self.put(f, x, y, yaw, solid=True)

    def shelf(self, x, y, w=1.6, d=0.4, h=1.9, yaw=0.0, rows=4, goods=True, mat="CT_Wood", seed=0):
        """A shelving unit facing -y with goods boxes (inert detail)."""
        def f(m):
            box(m, mat, -w / 2, w / 2, d / 2 - 0.03, d / 2, 0.0, h)
            for sx in (-1, 1):
                box(m, mat, sx * w / 2 - (0.0 if sx > 0 else -0.03), sx * w / 2 + (0.03 if sx > 0 else 0.0), -d / 2, d / 2, 0.0, h)
            for r in range(rows + 1):
                z = 0.1 + r * (h - 0.14) / rows
                box(m, mat, -w / 2, w / 2, -d / 2, d / 2, z, z + 0.03)
        self.put(f, x, y, yaw, solid=True)
        if goods:
            def g(m):
                k = seed
                for r in range(rows):
                    z = 0.13 + r * (h - 0.14) / rows
                    u = -w / 2 + 0.06
                    while u < w / 2 - 0.15:
                        k += 1
                        bw = 0.1 + 0.05 * ((k * 7) % 4)
                        bh = 0.12 + 0.04 * ((k * 5) % 4)
                        if u + bw > w / 2 - 0.04:
                            break
                        box(m, GOODS[(k * 3 + r) % len(GOODS)], u, u + bw, -d / 2 + 0.05, d / 2 - 0.06, z, z + min(bh, (h - 0.14) / rows - 0.05))
                        u += bw + 0.03
            self.put(g, x, y, yaw)

    def cabinet(self, x, y, w=0.9, d=0.45, h=1.8, yaw=0.0, mat="CT_FrameWhite"):
        def f(m):
            box(m, mat, -w / 2, w / 2, -d / 2, d / 2, 0.0, h)
            box(m, "CT_Steel", -0.02, 0.02, -d / 2 - 0.02, -d / 2, 0.3, h - 0.3, faces="y")
            box(m, "CT_Glass" if False else "CT_Aluminium", -w / 2 + 0.05, -0.04, -d / 2 - 0.01, -d / 2, 0.25, h - 0.25, faces="y")
        self.put(f, x, y, yaw, solid=True)

    # -------------------------------------------------------------- props
    def plant(self, x, y, h=1.2, pot="CT_PlasterRed", leaf="CT_Grass", z=None):
        def f(m):
            r0 = 0.2
            prism(m, pot, ring(0, 0, r0, 8), 0.0, 0.38, top=True, bottom=False)
            # a blobby crown: two stacked low octagonal prisms, tapering
            prism(m, leaf, ring(0, 0, 0.28, 8), 0.38, 0.38 + h * 0.35, top=False, bottom=False)
            prism(m, leaf, ring(0, 0, 0.3, 8), 0.38 + h * 0.35, 0.38 + h * 0.65, top=False, bottom=False)
            prism(m, leaf, ring(0, 0, 0.14, 8), 0.38 + h * 0.65, 0.38 + h * 0.9, top=True, bottom=False)
            prism(m, leaf, ring(0, 0, 0.28, 8), 0.38 + h * 0.35, 0.38 + h * 0.35 + 0.001, top=False, bottom=True)
        self.put(f, x, y, 0.0)

    def panel(self, x, y, zc, w, h, yaw=0.0, mat="CT_PlasterRed", frame="CT_Frame", t=0.03, off=0.0):
        """A flat wall panel / poster / board centred at height zc, standing on the wall at (x, y), its face towards `yaw`."""
        # local y: the panel sticks out toward -y (faces -y) from the wall at local y=0
        fr = geom.Frame(x, y, math.radians(yaw), oz=0.0)
        mm = Mesh()
        t = t + off
        def g(m):
            if frame:
                box(m, frame, -w / 2 - 0.03, w / 2 + 0.03, -t, 0.0, zc - h / 2 - 0.03, zc + h / 2 + 0.03)
            box(m, mat, -w / 2, w / 2, -t - 0.012, -t, zc - h / 2, zc + h / 2, faces="y")
        g(mm)
        self.B.detail.extend(fr.apply(mm))

    def tray(self, x, y, z, mat="CT_Aluminium"):
        box(self.B.detail, mat, x - 0.2, x + 0.2, y - 0.15, y + 0.15, z, z + 0.02)

    def cup(self, x, y, z, mat="CT_TileWhite", h=0.1):
        box(self.B.detail, mat, x - 0.04, x + 0.04, y - 0.04, y + 0.04, z, z + h)

    def rug(self, x0, x1, y0, y1, mat="CT_PlasterRed"):
        box(self.B.detail, mat, x0, x1, y0, y1, self.z0 + 0.0, self.z0 + 0.012, faces="Z")

    def pendant(self, x, y, zc, lumens=700.0, colour="#FFD2A0", kind="public", radius=6.0, drop=0.45):
        """A real lamp plus a hanging cord (zc = ceiling height)."""
        self.B.lamp((x, y, zc - drop), lumens=lumens, colour=colour, kind=kind, radius=radius)
        box(self.B.detail, "CT_Steel", x - 0.008, x + 0.008, y - 0.008, y + 0.008, zc - drop + 0.08, zc)

    def skirting(self, pts, h=0.1, mat="CT_Frame", t=0.02):
        """Skirting along a polyline of (x, y) with the room on its LEFT (CCW room outline)."""
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            L = math.hypot(bx - ax, by - ay)
            if L < 1e-6:
                continue
            ux, uy = (bx - ax) / L, (by - ay) / L
            nx, ny = -uy * t, ux * t                                   # into the room
            prism(self.B.detail, mat, [(ax, ay), (bx, by), (bx + nx, by + ny), (ax + nx, ay + ny)], self.z0, self.z0 + h,
                  top=True, bottom=False)

    def border(self, x0, x1, y0, y1, w=0.25, mat="CT_ConcreteDark", z=0.0):
        """A darker floor border strip (frame) just inside the rectangle, 1 cm proud of the floor."""
        d = self.B.detail
        zz = self.z0 + z + 0.008
        for a, b, c, e in ((x0, x1, y0, y0 + w), (x0, x1, y1 - w, y1), (x0, x0 + w, y0 + w, y1 - w), (x1 - w, x1, y0 + w, y1 - w)):
            box(d, mat, a, b, c, e, zz - 0.006, zz, faces="Z")

    def ceiling_panels(self, x0, x1, y0, y1, z, step=1.2, mat="CT_FrameWhite"):
        """Thin light-coloured beams under a ceiling to give it relief (a grid of battens)."""
        d = self.B.detail
        x = x0 + step
        while x < x1 - 0.1:
            box(d, mat, x - 0.04, x + 0.04, y0, y1, z - 0.09, z - 0.0, faces="xXyYz")
            x += step
