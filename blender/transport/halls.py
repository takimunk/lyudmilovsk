"""The cable-car stations' halls, v3 (Dima 2026-11-20: "inside it is crude, dark, empty, a cafe with nobody to buy from").

Each vestibule's hall (blender/transport/vestibules.py) is a ring 1.5..2.8 m wide round the tower's core: the lift and
the stair come up through a railed opening in the middle. This script dresses that ring as a real station hall with
the town's interior kit (blender/city/interior_kit.py, the CT_* materials and the picture atlas):

  * the exit side (where the lift and the stair arrive and the door to the platforms is) stays a clear concourse; the
    station attendant's desk stands at the old kiosk spot, the live route diagram (code) at its spot;
  * a buffet: a counter with a brass top, a samovar, cups and a till, shelves and the menu on the wall behind it,
    a sign hung over it, two lamps; the barista stands behind it (the game sells over it: T);
  * a waiting corner: rows of moulded seats along the wall, planters at their ends, a clock;
  * a cafe corner: small round tables with chairs, newspapers, a magazine rack;
  * a wooden wainscot and a skirting along every solid stretch of the wall, posters and mosaic panels between the
    windows, lamps over each corner and round the stairwell.
The hall's vending machine and its signature piece (vestibules.json "furniture": Vending, CoffeeBar, Stove, AmberCase,
Aquarium) and the route diagram keep their places and are kept clear.

Writes (station frame of vestibules.json; Blender metres x, y, z -> UE cm x*100, -y*100, z*100):
  blender/export/city/tr_hall__<style>.glb    SM_tr_hall__<style> (the hidden collision cores) and
                                              SM_tr_hall__<style>_Detail (the furniture), imported like a town
                                              building (unreal/scripts/import_city.sh, TYPES=tr_hall__<style>)
  blender/export/city/tr_hall__<style>.json   the lint numbers
  unreal/Lyudmilovsk/Content/Transport/halls.json   per style: the meshes, lamps, the planters (live plants),
                                              staff (barista, attendant), seats and standing spots for the waiting
                                              passengers, the counter (ALyudStationHall, LyudStationHall.cpp)
  renders/transport/halls/<style>_<n>.jpg     with the vestibule model (HALL_VEST=<dir with <Style>.vglb|glb>)

  python3 blender/transport/halls.py -- [styles,comma] [--no-render] [--samples N]     (pip's bpy, or blender -b -P)
"""
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CITY = os.path.join(ROOT, "blender", "city")
sys.path.insert(0, CITY)
sys.path.insert(0, os.path.join(CITY, "buildings"))
_argv = sys.argv[:]
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
sys.argv = [sys.argv[0], "--", "--no-render"]           # build_city reads its own flags at import
import citykit  # noqa: E402
import interior_kit as kit  # noqa: E402
try:
    import bpy  # noqa: E402
    from mathutils import Vector  # noqa: E402
    import build_city as BC  # noqa: E402
except ImportError:                                     # plain python3: the layout and halls.json only
    bpy = None
sys.argv = _argv

VEST_JSON = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "vestibules.json")
OUT_JSON = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "halls.json")
EXPORT = os.path.join(ROOT, "blender", "export", "city")
RENDERS = os.path.join(ROOT, "renders", "transport", "halls")
STYLES = ["arches", "dome", "folded", "lantern", "saucer", "star", "wing"]
KEEP = ("Vending", "CoffeeBar", "Stove", "AmberCase", "Aquarium")
# the signature pieces' footprint radius (m) to keep clear round them (lobby_props.py sizes)
KEEP_R = {"Vending": 0.75, "CoffeeBar": 1.6, "Stove": 1.0, "AmberCase": 0.9, "Aquarium": 1.0}
PLANTS = ["Monstera", "KentiaPalm", "Aspidistra", "Ficus", "Dracaena", "Sansevieria"]
RAIL = 0.12          # the railing round the stairwell's opening, its thickness and a hand's clearance
WALK = 0.95          # the narrowest walk left between anything and the railing

# per style: the hall's colours (the buffet's front, the seats, the wainscot) so the seven halls differ
LOOK = {
    "wing":    {"front": "CT_Leather", "seat": "CT_PlasterRed", "panel": "CT_Wood", "tile": "tile_1", "pend": "cone"},
    "dome":    {"front": "CT_PlasterBlue", "seat": "CT_PlasterBlue", "panel": "CT_WoodLight", "tile": "tile_2", "pend": "opal"},
    "saucer":  {"front": "CT_PlasterYellow", "seat": "CT_PlasterYellow", "panel": "CT_Wood", "tile": "tile_3", "pend": "opal"},
    "star":    {"front": "CT_PlasterGreen", "seat": "CT_PlasterGreen", "panel": "CT_Wood", "tile": "tile_4", "pend": "cone"},
    "lantern": {"front": "CT_PlasterRed", "seat": "CT_PlasterRed", "panel": "CT_WoodLight", "tile": "tile_5", "pend": "opal"},
    "arches":  {"front": "CT_Leather", "seat": "CT_PlasterYellow", "panel": "CT_Wood", "tile": "tile_6", "pend": "cone"},
    "folded":  {"front": "CT_PlasterPink", "seat": "CT_PlasterBlue", "panel": "CT_WoodLight", "tile": "tile_7", "pend": "opal"},
}


def log(*a):
    print("HALLS", *a, flush=True)


def ue_xy(v):
    return (v[0] / 100.0, -v[1] / 100.0)


def to_ue(p, z=None):
    return [round(p[0] * 100.0, 1), round(-p[1] * 100.0, 1), round((p[2] if z is None else z) * 100.0, 1)]


def ue_yaw(kit_yaw):
    """The kit's yaw (0 faces -y, 90 faces +x) -> UE yaw of the facing (degrees)."""
    return round(((90.0 - kit_yaw) + 180.0) % 360.0 - 180.0, 1)


def facing_yaw(dx, dy):
    """The kit's yaw that faces the direction (dx, dy) (Blender)."""
    return math.degrees(math.atan2(dx, -dy)) % 360.0


def adiff(a, b):
    return abs(((a - b + math.pi) % (2 * math.pi)) - math.pi)


# ------------------------------------------------------------------------------------------------ the hall's shape
class Hall:
    """The ring between the stairwell's railing and the room's wall, in Blender metres (station frame)."""

    def __init__(self, style, V, walls=None):
        self.style = style
        self.V = V
        room = V["room"]
        self.rc = ue_xy(room["centre"])
        self.H = room["height"] / 100.0
        self.R = room["radius"] / 100.0
        b0, b1 = room["box"]
        xs = sorted((b0[0] / 100.0, b1[0] / 100.0))
        ys = sorted((-b0[1] / 100.0, -b1[1] / 100.0))
        self.box = (xs[0], xs[1], ys[0], ys[1])
        self.kind = {"wing": "box"}.get(style, "cyl")
        fh = V["floor_hole"]
        self.hole = (fh[0] / 100.0, fh[2] / 100.0, -fh[3] / 100.0, -fh[1] / 100.0)
        ce = V.get("core_exit", "-x")
        self.exit = {"-x": (-1, 0), "+x": (1, 0), "-y": (0, 1), "+y": (0, -1)}[ce]
        self.doors = [ue_xy(d["at"]) for d in V.get("doors", [])]
        self.walls = walls          # [(angle rad, distance m, solid bool)] from the vestibule model, or None
        # the hall's own things kept where they are (UE furniture records) and the code's spots
        self.keep = [f for f in V.get("furniture", []) if f["item"] in KEEP]
        self.spots = {k: (ue_xy(s["at"]), s["yaw"]) for k, s in V.get("spots", {}).items()}

    # distance from the room's centre to the wall along angle a (rad)
    def wall(self, a):
        if self.walls:
            best = min(self.walls, key=lambda w: adiff(w[0], a))
            return best[1]
        if self.kind == "box":
            x0, x1, y0, y1 = self.box
            c, s = math.cos(a), math.sin(a)
            tx = ((x1 if c > 0 else x0) - self.rc[0]) / c if abs(c) > 1e-6 else 1e9
            ty = ((y1 if s > 0 else y0) - self.rc[1]) / s if abs(s) > 1e-6 else 1e9
            return min(tx, ty)
        return self.R

    def solid(self, a, half=0.0):
        """Is the wall solid (no window, no door) over the angles a +- half?"""
        if not self.walls:
            return False
        n = max(1, int(half / math.radians(2.5)))
        for k in range(-n, n + 1):
            aa = a + (half * k / n if n else 0.0)
            w = min(self.walls, key=lambda w: adiff(w[0], aa))
            if not w[2]:
                return False
        for d in self.doors:
            if adiff(self.ang(d), a) < half + math.atan2(1.2, self.wall(a)):
                return False
        return True

    # distance from the room's centre to the railing round the stairwell along angle a
    def core(self, a):
        x0, x1, y0, y1 = self.hole
        x0, x1, y0, y1 = x0 - RAIL, x1 + RAIL, y0 - RAIL, y1 + RAIL
        c, s = math.cos(a), math.sin(a)
        ox, oy = self.rc
        ts = []
        for t in ((x1 - ox) / c if c > 1e-6 else None, (x0 - ox) / c if c < -1e-6 else None,
                  (y1 - oy) / s if s > 1e-6 else None, (y0 - oy) / s if s < -1e-6 else None):
            if t is not None and t > 0:
                ts.append(t)
        return min(ts) if ts else 0.0

    def ang(self, p):
        return math.atan2(p[1] - self.rc[1], p[0] - self.rc[0])

    def at(self, a, r):
        return (self.rc[0] + math.cos(a) * r, self.rc[1] + math.sin(a) * r)

    def inward(self, a):
        """The kit yaw facing the room's centre from angle a."""
        return facing_yaw(-math.cos(a), -math.sin(a))


# ------------------------------------------------------------------------------------------------ dressing
class Dresser:
    def __init__(self, style, V, walls=None):
        self.h = Hall(style, V, walls)
        self.B = citykit.Building("tr_hall__" + style)
        self.look = LOOK[style]
        self.rnd = random.Random(sum(map(ord, style)) * 7)
        self.taken = []            # (angle, half-angle) spans of the wall used
        self.rail_taken = []       # (angle, half-width m) along the railing
        self.rail_keep = []        # (angle, why): the railing kept clear (the core's exit)
        self.out = {"lamps": [], "planters": [], "staff": [], "seats": [], "stands": [], "counter": None,
                    "keep": [k["item"] for k in self.h.keep], "notes": []}
        h = self.h
        # the clear ways: the core's exit concourse, the doors, the diagram, the kept things
        ea = math.atan2(h.exit[1], h.exit[0])
        self.exit_a = ea
        self.block(ea, math.radians(38), "exit")
        self.rail_keep.append((ea, "exit"))
        for d in h.doors:
            self.block(h.ang(d), math.atan2(1.0, h.wall(h.ang(d))), "door")
        for k in ("diagram", "kiosk"):
            if k in h.spots:
                p = h.spots[k][0]
                self.block(h.ang(p), math.atan2(1.0 if k == "diagram" else 0.9, max(1.0, math.dist(p, h.rc))), k)
        for f in h.keep:
            p = ue_xy(f["at"])
            self.block(h.ang(p), math.atan2(KEEP_R.get(f["item"], 0.8), max(1.0, math.dist(p, h.rc))), f["item"])

    def free_rail(self, a, half_m):
        r = self.h.core(a)
        return not any(adiff(a, b) < math.atan2(half_m + hb, r) for b, hb in self.rail_taken) and \
            not any(adiff(a, b) < math.radians(30) for b, why in self.rail_keep)

    def block(self, a, half, why=""):
        self.taken.append((a, half, why))

    def free(self, a, half):
        return not any(adiff(a, b) < half + hb for b, hb, _ in self.taken)

    def depth(self, a):
        """The free depth between the railing and the wall at angle a."""
        return self.h.wall(a) - self.h.core(a)

    # -------------------------------------------------------------- choosing where each corner goes
    def sides(self):
        """The three sides of the core away from its exit, best (deepest, least blocked) first."""
        ea = self.exit_a
        out = []
        for k in (1, 2, 3):
            a = ea + k * math.pi / 2
            # the free angular room round a, and the depth there
            span = 0.0
            for step in range(1, 30):
                d = math.radians(step * 2)
                if self.free(a + d, 0.0) and self.free(a - d, 0.0):
                    span = d
                else:
                    break
            out.append((a, span, self.depth(a)))
        return out

    # -------------------------------------------------------------- pieces along the wall
    def wall_point(self, a, off):
        r = self.h.wall(a) - off
        return self.h.at(a, r)

    def lamp(self, x, y, lumens, colour="#FFD9A8", radius=6.0, kind=None, drop=0.55):
        """A pendant under the ceiling (a real light in the game), its drop longer in a tall hall."""
        ceil = self.h.H
        drop = drop if ceil < 4.5 else ceil - 2.9
        r = kit.pendant(self.B, x, y, ceil, drop=drop, kind=kind or self.look["pend"], light=False)
        self.out["lamps"].append({"at": to_ue((x, y, 0), ceil - drop - 0.12), "lumens": lumens, "colour": colour,
                                  "radius": round(radius * 100)})
        return r

    def planter(self, a, off=0.3):
        p = self.wall_point(a, off)
        self.out["planters"].append({"at": to_ue((p[0], p[1], 0)), "yaw": ue_yaw(self.h.inward(a)) + 180.0,
                                     "plant": PLANTS[self.rnd.randrange(len(PLANTS))]})
        self.block(a, math.atan2(0.4, self.h.wall(a)), "planter")

    # -------------------------------------------------------------- the buffet
    def coffee_bar(self):
        """The hall already has the coffee bar (Aton, LyudLobby.cpp CoffeeBar: the counter faces its +X, 2.3 m long): the
        barista stands behind it; its menu and sign go on the wall behind."""
        h = self.h
        rec = next((f for f in h.keep if f["item"] == "CoffeeBar"), None)
        if rec is None:
            return False
        p = ue_xy(rec["at"])
        yu = rec["yaw"]
        f = (math.cos(math.radians(yu)), -math.sin(math.radians(yu)))         # Blender facing of its +X
        yaw = facing_yaw(*f)
        st = (p[0] - f[0] * 0.62, p[1] - f[1] * 0.62)
        cu = (p[0] + f[0] * 0.75, p[1] + f[1] * 0.75)
        self.out["staff"].append({"role": "barista", "at": to_ue((st[0], st[1], 0)), "yaw": ue_yaw(yaw)})
        self.out["counter"] = {"at": to_ue((p[0], p[1], 0), 1.05), "customer": to_ue((cu[0], cu[1], 0)), "yaw": ue_yaw(yaw),
                               "shop": "buffet", "lobby": "CoffeeBar"}
        self.out["stands"].append({"at": to_ue((cu[0], cu[1], 0)), "yaw": ue_yaw((yaw + 180) % 360), "kind": "counter"})
        a = h.ang(p)
        R = h.wall(a)
        if h.solid(a, math.atan2(0.5, R)) or not h.walls:
            bx, by = self.wall_point(a, 0.03)
            kit.menu_board(self.B, bx, by, 0.0, h.inward(a), name="menu", w=0.9, zc=2.1, seed=4)
        return True

    def buffet(self, a):
        h, B, look = self.h, self.B, self.look
        dep = self.depth(a)
        R = h.wall(a)
        d = 0.6 if dep >= 2.45 else 0.52
        staff = 0.8 if dep >= 2.45 else 0.7
        w = 2.0 if dep >= 2.45 else 1.6
        if dep < d + staff + WALK:
            self.out["notes"].append("buffet: no room (%.2f m)" % dep)
            return False
        hw = w / 2
        # the counter's back line: the staff's gap to the wall at least `staff` at its ends too
        if h.kind == "cyl" and not h.walls:
            r_back = min(math.sqrt(max(0.0, (R - staff) ** 2 - hw ** 2)), R - staff)
        else:
            r_back = min(h.wall(a + math.atan2(hw, R)), h.wall(a - math.atan2(hw, R)), R) - staff
        r_back = min(r_back, h.core(a) + WALK + d)      # never closer to the railing than a walk
        r_c = r_back - d / 2
        if r_c - d / 2 - h.core(a) < WALK - 0.05:
            self.out["notes"].append("buffet: too tight")
            return False
        x, y = h.at(a, r_c)
        yaw = h.inward(a)
        c = kit.bar_counter(B, x, y, 0.0, yaw, w=w, d=d, front=look["front"], taps=0, back=False, seed=3)
        top = c["top"]
        fx, fy = math.cos(a), math.sin(a)                  # outward
        tx, ty = -fy, fx                                   # along the counter
        def on(u, v):                                      # a point on the top: u along, v toward the customer
            return (x + tx * u - fx * v, y + ty * u - fy * v)
        sx, sy = on(-0.55, 0.0)
        kit.samovar(B, sx, sy, top, yaw)
        for k, u in enumerate((-0.15, 0.05, 0.25)):
            px, py = on(u, -0.12)
            kit.cup(B, px, py, top, yaw + 20 * k)
        px, py = on(0.6, 0.05)
        kit.cash_register(B, px, py, top, yaw)
        px, py = on(0.35, 0.1)
        kit.teapot(B, px, py, top, yaw)
        # behind: two shelves of goods and the menu on the wall (on a window only the low shelf)
        span = math.atan2(hw + 0.2, R)
        solid = h.solid(a, span) or not h.walls
        bx, by = self.wall_point(a, 0.03)
        if solid:
            kit.wall_shelf(B, *self.wall_point(a - math.atan2(0.5, R), 0.03), 0.0, yaw, w=0.9, zt=1.45, seed=1)
            kit.wall_shelf(B, *self.wall_point(a + math.atan2(0.5, R), 0.03), 0.0, yaw, w=0.9, zt=1.45, seed=2)
            kit.menu_board(B, bx, by, 0.0, yaw, name="menu", w=0.9, zc=2.05, seed=4)
        else:
            kit.shop_counter(B, *self.wall_point(a, 0.25), 0.0, yaw, w=1.4, d=0.4, h=0.9) if hasattr(kit, "shop_counter") else None
        # the sign hung over the counter, two lamps
        if h.H > 2.9:
            sx, sy = on(0.0, -0.05)
            kit.sign_plate(B, sx, sy, 0.0, yaw, name="sign_buffet", w=1.0, zc=min(2.55, h.H - 0.45), hang=True)
        for u in (-0.6, 0.6):
            lx, ly = on(u, 0.15)
            self.lamp(lx, ly, 900, "#FFD49A", 4.5, kind="cone", drop=0.75)
        st = c["staff"]
        cu = c["customer"]
        self.out["staff"].append({"role": "barista", "at": to_ue((st[0], st[1], 0)), "yaw": ue_yaw(yaw)})
        self.out["counter"] = {"at": to_ue((x, y, 0), top), "customer": to_ue((cu[0], cu[1], 0)), "yaw": ue_yaw(yaw),
                               "shop": "buffet"}
        self.out["stands"].append({"at": to_ue((cu[0], cu[1], 0)), "yaw": ue_yaw((yaw + 180) % 360), "kind": "counter"})
        self.block(a, math.atan2(hw + 0.35, R), "buffet")
        for e in (a - math.atan2(hw + 0.7, R), a + math.atan2(hw + 0.7, R)):
            if self.free(e, math.atan2(0.3, R)) and self.depth(e) > 0.6 + WALK:
                self.planter(e)
        return True

    # -------------------------------------------------------------- the waiting corner
    def waiting(self, a):
        h, B = self.h, self.B
        R = h.wall(a)
        made = 0
        for k, n in ((0, 4), (1, 3), (-1, 3), (2, 2), (-2, 2)):
            seg = n * 0.52
            # rows on a polygon round the wall, each tangent at its own angle
            da = 2 * math.atan2(seg / 2 + 0.12, R - 0.4) * k
            aa = a + da
            if not self.free(aa, math.atan2(seg / 2 + 0.05, R)):
                continue
            off = 0.36 + (R - math.sqrt(max(0.0, R * R - (seg / 2) ** 2)) if h.kind == "cyl" else 0.0)
            dep = self.depth(aa)
            if dep < off + 0.3 + WALK:
                continue
            x, y = self.wall_point(aa, off)
            yaw = h.inward(aa)
            r = kit.waiting_row(B, x, y, 0.0, yaw, n=n, mat=self.look["seat"])
            for s in r["seats"]:
                self.out["seats"].append({"at": to_ue((s[0], s[1], 0)), "yaw": ue_yaw(s[3]), "height": round(s[2] * 100)})
            self.block(aa, math.atan2(seg / 2 + 0.05, R), "seats")
            made += 1
        # a row with its back to the stairwell's railing, looking at the windows (where the ring is deep enough)
        for k in (0, 1, -1):
            aa = a + k * math.radians(24)
            dep = self.depth(aa)
            if dep < 0.6 + WALK + 0.65 or not self.free_rail(aa, 0.9):
                continue
            r0 = h.core(aa) + 0.34
            x, y = h.at(aa, r0)
            yaw = facing_yaw(math.cos(aa), math.sin(aa))
            r = kit.waiting_row(B, x, y, 0.0, yaw, n=3, mat=self.look["seat"])
            for s_ in r["seats"]:
                self.out["seats"].append({"at": to_ue((s_[0], s_[1], 0)), "yaw": ue_yaw(s_[3]), "height": round(s_[2] * 100)})
            self.rail_taken.append((aa, 0.9))
            made += 1
            break
        if made:
            if h.solid(a, math.atan2(0.3, R)) or not h.walls:
                kit.wall_clock(B, *self.wall_point(a, 0.02), 0.0, h.inward(a), zc=min(2.45, h.H - 0.5))
            lx, ly = h.at(a, (h.core(a) + R) / 2)
            self.lamp(lx, ly, 1000)
            kit.rug(B, lx, ly, 0.0, h.inward(a), w=2.6, d=max(1.2, min(1.9, self.depth(a) - 0.4)), seed=3)
            for e in (a - 0.62, a + 0.62):
                if self.free(e, math.atan2(0.3, R)) and self.depth(e) > 0.6 + WALK:
                    self.planter(e)
        return made > 0

    # -------------------------------------------------------------- the cafe corner
    def cafe(self, a):
        h, B = self.h, self.B
        R = h.wall(a)
        made = 0
        for k in (0, -2, 2, -1, 1):
            aa = a + k * math.atan2(0.9, R)
            if not self.free(aa, math.atan2(0.75, R)):
                continue
            off = 0.62
            if self.depth(aa) < off + 0.35 + WALK:
                continue
            x, y = self.wall_point(aa, off)
            kit.table_round(B, x, y, 0.0, 0.0, r=0.32)
            # two chairs either side along the wall, facing the table
            tx, ty = -math.sin(aa), math.cos(aa)
            for sgn in (-1, 1):
                cx, cy = x + tx * 0.55 * sgn, y + ty * 0.55 * sgn
                yaw = facing_yaw(-tx * sgn, -ty * sgn)
                r = kit.chair(B, cx, cy, 0.0, yaw, kind="tube", mat=self.look["seat"])
                for s in r["seats"]:
                    self.out["seats"].append({"at": to_ue((s[0], s[1], 0)), "yaw": ue_yaw(s[3]), "height": round(s[2] * 100),
                                              "table": True})
            if made == 0:
                kit.newspaper(B, x + 0.08, y - 0.05, 0.75, 20)
            else:
                kit.cup(B, x - 0.1, y + 0.05, 0.75, 0)
            if h.H > 2.9 and made < 2:
                self.lamp(x, y, 650, "#FFDCA8", 3.5, kind="opal", drop=0.9)
            self.block(aa, math.atan2(0.85, R), "cafe")
            made += 1
            if made == 3:
                break
        if made:
            rx, ry = self.wall_point(a, 0.75)
            kit.rug(B, rx, ry, 0.0, h.inward(a), w=3.2, d=1.4, seed=9)
        if made and self.depth(a) > 0.7 + WALK + 0.7 and self.free_rail(a, 0.5):
            x, y = h.at(a, h.core(a) + 0.45)
            r = kit.high_table(B, x, y, 0.0, 0.0)
            for sp in r.get("spots", [])[:2]:
                self.out["stands"].append({"at": to_ue((sp[0], sp[1], 0)), "yaw": ue_yaw(facing_yaw(x - sp[0], y - sp[1])), "kind": "table"})
            self.rail_taken.append((a, 0.5))
        if made:
            for e in (a - 0.7, a + 0.7):
                if self.free(e, math.atan2(0.5, R)) and self.depth(e) > 0.4 + WALK and (h.solid(e, math.atan2(0.5, R)) or not h.walls):
                    kit.magazine_rack(B, *self.wall_point(e, 0.02), 0.0, h.inward(e), w=0.8, seed=5)
                    self.block(e, math.atan2(0.5, R), "rack")
                    self.out["stands"].append({"at": to_ue((*self.wall_point(e, 0.7), 0)), "yaw": ue_yaw((h.inward(e) + 180) % 360),
                                               "kind": "read"})
                    break
        return made > 0

    # -------------------------------------------------------------- the attendant at the old kiosk spot
    def attendant(self):
        h, B = self.h, self.B
        if "kiosk" not in h.spots:
            return
        p, yaw_ue = h.spots["kiosk"]
        # the spot's +X faces the visitor (code's kiosk): kit yaw facing that way
        f = (math.cos(math.radians(yaw_ue)), -math.sin(math.radians(yaw_ue)))
        yaw = facing_yaw(*f)
        x, y = p[0] - f[0] * 0.35, p[1] - f[1] * 0.35
        r = kit.reception_desk(B, x, y, 0.0, yaw, w=1.3, d=0.62, front=self.look["front"] if self.look["front"] != "CT_Leather" else "CT_PlasterBlue")
        kit.telephone(B, *r["on"][:2], r["top"] - 0.35, yaw) if hasattr(kit, "telephone") else None
        st = r["staff"]
        self.out["staff"].append({"role": "attendant", "at": to_ue((st[0], st[1], 0)), "yaw": ue_yaw(yaw)})
        if h.H > 2.9:
            self.lamp(x - f[0] * 0.2, y - f[1] * 0.2, 700, "#FFE2B8", 3.5, kind="cone", drop=0.8)

    # -------------------------------------------------------------- walls: wainscot, skirting, posters
    def walls(self):
        h, B, look = self.h, self.B, self.look
        if not h.walls:
            return
        m = B.detail
        n = len(h.walls)
        ws = sorted(h.walls)
        posters = 0
        run = []
        def flush(run):
            if len(run) < 2:
                return
            for (a0, r0, _), (a1, r1, _) in zip(run, run[1:]):
                p0 = h.at(a0, r0 - 0.012)
                p1 = h.at(a1, r1 - 0.012)
                q0 = h.at(a0, r0 - 0.035)
                q1 = h.at(a1, r1 - 0.035)
                # panel 0..0.95 (front face toward the room), a rail on top, a dark skirting at the foot
                m.face(look["panel"], [(q1[0], q1[1], 0.1), (q0[0], q0[1], 0.1), (q0[0], q0[1], 0.95), (q1[0], q1[1], 0.95)])
                m.face(look["panel"], [(q0[0], q0[1], 0.95), (p0[0], p0[1], 0.95), (p1[0], p1[1], 0.95), (q1[0], q1[1], 0.95)])
                s0, s1 = h.at(a0, r0 - 0.05), h.at(a1, r1 - 0.05)
                m.face("CT_ConcreteDark", [(s1[0], s1[1], 0.0), (s0[0], s0[1], 0.0), (s0[0], s0[1], 0.1), (s1[0], s1[1], 0.1)])
                m.face("CT_ConcreteDark", [(s0[0], s0[1], 0.1), (q0[0], q0[1], 0.1), (q1[0], q1[1], 0.1), (s1[0], s1[1], 0.1)])
                t0, t1 = h.at(a0, r0 - 0.06), h.at(a1, r1 - 0.06)
                m.face(kit.BRASS, [(t1[0], t1[1], 0.95), (t0[0], t0[1], 0.95), (t0[0], t0[1], 0.99), (t1[0], t1[1], 0.99)])
                m.face(kit.BRASS, [(t0[0], t0[1], 0.99), (p0[0], p0[1], 0.99), (p1[0], p1[1], 0.99), (t1[0], t1[1], 0.99)])
        for i in range(n):
            a, r, s = ws[i]
            door = any(adiff(h.ang(d), a) < math.atan2(1.0, r) for d in h.doors)
            if s and not door:
                run.append(ws[i])
            else:
                flush(run)
                run = []
        if run and ws[0][2] and len(run) < n:
            flush(run + [ws[0]])
        else:
            flush(run)
        # posters and mosaic panels on solid wall over the wainscot, where nothing stands in front
        cells = ["travel", "map", look["tile"], "poster_birds", "travel", "poster_sun"]
        for k in range(36):
            a = self.exit_a + math.pi + k * math.radians(10) * (1 if k % 2 else -1) * 0.5
            R = h.wall(a)
            if not h.solid(a, math.atan2(0.45, R)):
                continue
            if any(adiff(a, b) < math.atan2(0.4, R) + hb for b, hb, why in self.taken if why in ("buffet", "rack", "door", "exit")):
                continue
            if any(adiff(a, b) < math.atan2(0.9, R) for b in getattr(self, "_hung", [])):
                continue
            x, y = self.wall_point(a, 0.04)
            name = cells[posters % len(cells)]
            if name.startswith("tile"):
                kit.mosaic_panel(B, x, y, 0.0, h.inward(a), name=name, w=0.8, h=0.8, zc=1.75, seed=k)
            else:
                kit.poster(B, x, y, 0.0, h.inward(a), name=name, w=0.55, zc=1.7, seed=k + 11)
            self._hung = getattr(self, "_hung", []) + [a]
            posters += 1
            if posters >= 6:
                break

    # -------------------------------------------------------------- floor and ceiling: a stone border round the stairwell, a
    # dark band along the wall, a wooden cove with a glowing strip round the opening under a low ceiling
    def finish(self):
        h, m = self.h, self.B.detail
        x0, x1, y0, y1 = h.hole
        def ring_band(o0, o1, z, mat, up=True):
            pts0 = [(x0 - o0, y0 - o0), (x1 + o0, y0 - o0), (x1 + o0, y1 + o0), (x0 - o0, y1 + o0)]
            pts1 = [(x0 - o1, y0 - o1), (x1 + o1, y0 - o1), (x1 + o1, y1 + o1), (x0 - o1, y1 + o1)]
            for i in range(4):
                j = (i + 1) % 4
                q = [(pts0[i][0], pts0[i][1], z), (pts1[i][0], pts1[i][1], z), (pts1[j][0], pts1[j][1], z), (pts0[j][0], pts0[j][1], z)]
                m.face(mat, q if not up else q[::-1] if False else q)
        # the floor's stone border round the railing (faces up: CCW seen from above)
        for i, (o0, o1, mat) in enumerate(((RAIL + 0.02, RAIL + 0.30, "CT_Stone"), (RAIL + 0.30, RAIL + 0.34, kit.BRASS))):
            pts0 = [(x0 - o0, y0 - o0), (x1 + o0, y0 - o0), (x1 + o0, y1 + o0), (x0 - o0, y1 + o0)]
            pts1 = [(x0 - o1, y0 - o1), (x1 + o1, y0 - o1), (x1 + o1, y1 + o1), (x0 - o1, y1 + o1)]
            for k in range(4):
                j = (k + 1) % 4
                m.face(mat, [(pts1[k][0], pts1[k][1], 0.004), (pts1[j][0], pts1[j][1], 0.004),
                             (pts0[j][0], pts0[j][1], 0.004), (pts0[k][0], pts0[k][1], 0.004)][::-1])
        # a dark band along the wall
        n = 96
        for k in range(n):
            a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
            if any(adiff(h.ang(d), (a0 + a1) / 2) < math.atan2(0.9, h.wall(a0)) for d in h.doors):
                continue
            p0, p1 = h.at(a0, h.wall(a0) - 0.06), h.at(a1, h.wall(a1) - 0.06)
            q0, q1 = h.at(a0, h.wall(a0) - 0.36), h.at(a1, h.wall(a1) - 0.36)
            m.face("CT_ConcreteDark", [(q0[0], q0[1], 0.004), (q1[0], q1[1], 0.004), (p1[0], p1[1], 0.004), (p0[0], p0[1], 0.004)])
        # the cove round the opening under a low ceiling: a wooden fascia, its underside glowing
        if h.H < 4.5:
            zt, zb = h.H - 0.02, h.H - 0.32
            o0, o1 = RAIL + 0.05, RAIL + 0.45
            c0 = [(x0 - o0, y0 - o0), (x1 + o0, y0 - o0), (x1 + o0, y1 + o0), (x0 - o0, y1 + o0)]
            c1 = [(x0 - o1, y0 - o1), (x1 + o1, y0 - o1), (x1 + o1, y1 + o1), (x0 - o1, y1 + o1)]
            for k in range(4):
                j = (k + 1) % 4
                # outer face (toward the room), inner face (toward the stairwell), the glowing underside
                m.face(self.look["panel"], [(c1[k][0], c1[k][1], zb), (c1[j][0], c1[j][1], zb), (c1[j][0], c1[j][1], zt), (c1[k][0], c1[k][1], zt)][::-1])
                m.face(self.look["panel"], [(c0[k][0], c0[k][1], zb), (c0[j][0], c0[j][1], zb), (c0[j][0], c0[j][1], zt), (c0[k][0], c0[k][1], zt)])
                m.face("CT_Opal", [(c0[k][0], c0[k][1], zb), (c1[k][0], c1[k][1], zb), (c1[j][0], c1[j][1], zb), (c0[j][0], c0[j][1], zb)][::-1])

    # -------------------------------------------------------------- lamps round the stairwell and spots by the railing
    def ring(self):
        h = self.h
        x0, x1, y0, y1 = h.hole
        for k, (cx, cy) in enumerate(((x0, y0), (x1, y0), (x1, y1), (x0, y1))):
            a = h.ang((cx, cy))
            r = (h.core(a) + h.wall(a)) / 2
            x, y = h.at(a, r)
            self.lamp(x, y, 800, "#FFE0B0", 5.0)
        # people stand at the railing looking down the stairwell, and at the windows
        for k in range(4):
            a = self.exit_a + math.pi / 2 * k + math.pi / 4
            r = h.core(a) + 0.35
            x, y = h.at(a, r)
            self.out["stands"].append({"at": to_ue((x, y, 0)), "yaw": ue_yaw(h.inward(a)), "kind": "rail"})
        if h.walls:
            for k in range(24):
                a = k * math.pi / 12
                if not h.solid(a, 0.0) and self.free(a, math.atan2(0.4, h.wall(a))) and self.depth(a) > 1.4:
                    x, y = self.wall_point(a, 0.45)
                    self.out["stands"].append({"at": to_ue((x, y, 0)), "yaw": ue_yaw((h.inward(a) + 180) % 360), "kind": "window"})
        if "diagram" in h.spots:
            p, yaw_ue = h.spots["diagram"]
            f = (math.cos(math.radians(yaw_ue)), -math.sin(math.radians(yaw_ue)))
            x, y = p[0] + f[0] * 1.1, p[1] + f[1] * 1.1
            self.out["stands"].append({"at": to_ue((x, y, 0)), "yaw": round((yaw_ue + 180) % 360 - 180, 1), "kind": "diagram"})

    def run(self):
        sides = self.sides()
        log(self.h.style, "sides", [(round(math.degrees(a)), round(math.degrees(s)), round(d, 2)) for a, s, d in sides])
        order = sorted(sides, key=lambda t: (-(t[1] > math.radians(14)), -t[2]))
        done = set()
        if self.coffee_bar():
            done.add("buffet")
        for a, s, d in order:
            if "buffet" not in done and self.buffet(a):
                done.add("buffet")
                continue
            if "waiting" not in done and self.waiting(a):
                done.add("waiting")
                continue
            if "cafe" not in done and self.cafe(a):
                done.add("cafe")
                continue
        # what found no side of its own: anywhere free
        for name, fn in (("waiting", self.waiting), ("cafe", self.cafe)):
            if name not in done:
                for k in range(16):
                    a = self.exit_a + math.pi + (k // 2 + 1) * math.radians(22) * (1 if k % 2 else -1)
                    if fn(a):
                        done.add(name)
                        break
        self.attendant()
        # live plants in the corners of the ring (the core's diagonals), against the wall
        for k in range(4):
            a = self.exit_a + math.pi / 4 + k * math.pi / 2
            for da in (0.0, 0.12, -0.12):
                if self.free(a + da, math.atan2(0.35, self.h.wall(a + da))) and self.depth(a + da) > 0.5 + WALK:
                    self.planter(a + da, 0.28)
                    break
        self.walls()
        self.finish()
        self.ring()
        self.out["zones"] = sorted(done)
        notes = kit.check(self.B) if hasattr(kit, "check") else []
        self.out["notes"] += [str(n) for n in notes][:20]
        log(self.h.style, "zones", sorted(done), "lamps", len(self.out["lamps"]), "seats", len(self.out["seats"]),
            "stands", len(self.out["stands"]), "detail tris", self.B.detail.tri_count(), "notes", self.out["notes"][:6])
        return self


# ------------------------------------------------------------------------------------------------ Blender side
def find_vest(style):
    d = os.environ.get("HALL_VEST", os.path.join(ROOT, "docs", "station_halls_baseline", "vest"))
    for name in (style.capitalize() + ".vglb", style.capitalize() + ".glb", "SM_TR_Vestibule_%s.glb" % style.capitalize()):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    return None


def load_vest(style):
    """The vestibule model (and its glass) into the scene, in the station frame (Blender metres)."""
    p = find_vest(style)
    if not p or bpy is None:
        return []
    obs = []
    for path in (p, p.replace(style.capitalize() + ".", style.capitalize() + "_Glass.")):
        if not os.path.isfile(path) or (path == p and "_Glass" in path):
            continue
        tmp = path
        if path.endswith(".vglb"):
            tmp = os.path.join("/tmp", os.path.basename(path)[:-5] + ".glb")
            if not os.path.isfile(tmp):
                import shutil
                shutil.copy(path, tmp)
        before = set(bpy.data.objects)
        bpy.ops.import_scene.gltf(filepath=tmp)
        obs += [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    return obs


def wall_samples(style, obs, H):
    """Rays from the room's middle at 1.2 m: the distance to the room's wall every 2.5 degrees and whether it is solid
    (TR_Interior) or glass. None without the model."""
    if not obs:
        return None
    from mathutils.bvhtree import BVHTree
    import bmesh
    V = json.load(open(VEST_JSON))[style]
    rc = ue_xy(V["room"]["centre"])
    bm = bmesh.new()
    mats = []
    for o in obs:
        me = o.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
        me.transform(o.matrix_world)
        base = len(mats)
        mats += [m.name if m else "" for m in o.data.materials] or [""]
        tmp = bmesh.new()
        tmp.from_mesh(me)
        for f in tmp.faces:
            f.material_index += base
        me2 = bpy.data.meshes.new("t")
        tmp.to_mesh(me2)
        bm.from_mesh(me2)
        tmp.free()
    bm.faces.ensure_lookup_table()
    fmat = [f.material_index for f in bm.faces]
    tree = BVHTree.FromBMesh(bm)
    out = []
    for k in range(144):
        a = 2 * math.pi * k / 144
        d = Vector((math.cos(a), math.sin(a), 0.0))
        hits = []
        for z in (0.5, 1.2, 2.0):
            loc, nor, idx, dist = tree.ray_cast(Vector((rc[0], rc[1], z)) + d * 2.4, d, 12.0)
            hits.append((loc, idx, dist))
        loc, idx, dist = hits[1]
        if loc is None:
            out.append((a, 6.0, False))
            continue
        mname = mats[fmat[idx]] if idx is not None and fmat[idx] < len(mats) else ""
        solid = all(h[0] is not None and "Glass" not in (mats[fmat[h[1]]] if fmat[h[1]] < len(mats) else "Glass") and
                    abs(h[2] - dist) < 0.25 for h in hits) and "Glass" not in mname
        out.append((a, dist + 2.4, solid))
    return out


def render(style, B, obs, D, samples):
    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    BC.cycles_device(scn)
    scn.cycles.samples = samples
    scn.cycles.use_denoising = True
    scn.render.resolution_x, scn.render.resolution_y = 1280, 720
    scn.render.image_settings.file_format = "JPEG"
    scn.render.image_settings.quality = 86
    scn.view_settings.view_transform = "AgX"
    scn.view_settings.exposure = float(os.environ.get("HALL_EXPOSURE", "0.4"))
    world = bpy.data.worlds.new("w")
    scn.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.68, 0.85, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 2.5
    sun.rotation_euler = (math.radians(55), 0, math.radians(30))
    scn.collection.objects.link(sun)
    for i, l in enumerate(D.out["lamps"]):
        ld = bpy.data.lights.new("lamp%d" % i, "POINT")
        ld.energy = l["lumens"] / 15.0
        ld.shadow_soft_size = 0.15
        hx = l["colour"]
        ld.color = tuple(int(hx[k:k + 2], 16) / 255.0 for k in (1, 3, 5))
        lo = bpy.data.objects.new("lamp%d" % i, ld)
        lo.location = (l["at"][0] / 100.0, -l["at"][1] / 100.0, l["at"][2] / 100.0)
        scn.collection.objects.link(lo)
    h = D.h
    # stand-ins for what the game places itself (preview only): the kept lobby things, the diagram, the planters
    import bmesh
    bm = bmesh.new()
    def pbox(cx, cy, z0, sx, sy, sz, yaw_bl):
        r = bmesh.ops.create_cube(bm, size=1.0)
        vs = r["verts"]
        c, s_ = math.cos(yaw_bl), math.sin(yaw_bl)
        for v in vs:
            x, y, z = v.co.x * sx, v.co.y * sy, v.co.z * sz + sz / 2 + z0
            v.co = Vector((cx + x * c - y * s_, cy + x * s_ + y * c, z))
    SIZES = {"Vending": (0.8, 1.0, 1.9), "CoffeeBar": (0.7, 2.3, 1.1), "Stove": (1.0, 1.2, 2.2), "AmberCase": (0.6, 1.4, 1.6),
             "Aquarium": (0.5, 1.6, 1.3)}
    for f in h.keep:
        p = ue_xy(f["at"])
        sx, sy, sz = SIZES.get(f["item"], (0.6, 0.6, 1.5))
        pbox(p[0], p[1], 0.0, sx, sy, sz, -math.radians(f["yaw"]))
    if "diagram" in h.spots:
        p, yu = h.spots["diagram"]
        pbox(p[0], p[1], 0.0, 0.16, 2.7, 2.4, -math.radians(yu))
    for pl in D.out["planters"]:
        pbox(pl["at"][0] / 100.0, -pl["at"][1] / 100.0, 0.0, 0.5, 0.5, 0.5, 0.0)
        pbox(pl["at"][0] / 100.0, -pl["at"][1] / 100.0, 0.5, 0.6, 0.6, 0.9, 0.7)
    me = bpy.data.meshes.new("proxies")
    bm.to_mesh(me)
    po = bpy.data.objects.new("proxies", me)
    me.materials.append(BC.material("CT_PlasterGreen"))
    scn.collection.objects.link(po)
    shots = []
    ea = D.exit_a
    for k, (a_from, a_look, dz) in enumerate(((ea, ea + math.pi * 0.75, 0.0), (ea + math.pi * 0.5, ea + math.pi * 1.2, 0.0),
                                              (ea + math.pi, ea + math.pi * 0.35, 0.0), (ea + math.pi * 1.5, ea + math.pi * 0.9, 0.0))):
        r0 = (h.core(a_from) + h.wall(a_from)) / 2
        p0 = h.at(a_from, r0)
        tgt = h.at(a_look, h.wall(a_look) - 0.6)
        cam = bpy.data.objects.new("cam%d" % k, bpy.data.cameras.new("cam%d" % k))
        cam.data.lens = 14
        cam.location = Vector((p0[0], p0[1], 1.6 + dz))
        look = (Vector((tgt[0], tgt[1], 1.2)) - cam.location).normalized()
        cam.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
        scn.collection.objects.link(cam)
        scn.camera = cam
        os.makedirs(RENDERS, exist_ok=True)
        scn.render.filepath = os.path.join(RENDERS, "%s_%d.jpg" % (style, k))
        bpy.ops.render.render(write_still=True)
        shots.append(scn.render.filepath)
    # from above, the roof off (a plan view of the ring)
    cam = bpy.data.objects.new("top", bpy.data.cameras.new("top"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2 * max(h.wall(a) for a in [i * math.pi / 8 for i in range(16)]) + 1.0
    cam.data.clip_end = 100
    cam.location = Vector((h.rc[0], h.rc[1], min(h.H - 0.15, 3.0)))
    cam.rotation_euler = (0, 0, 0)
    scn.collection.objects.link(cam)
    scn.camera = cam
    scn.render.resolution_x = scn.render.resolution_y = 900
    scn.render.filepath = os.path.join(RENDERS, "%s_plan.jpg" % style)
    bpy.ops.render.render(write_still=True)
    shots.append(scn.render.filepath)
    return shots


def models(style_index):
    """Bodies for the hall's people: townsfolk who are away from the town (townsfolk.json occupation "away": they are never
    shown in the town, so nobody meets them twice); each with its fallback pool model. Staff: two adults; passengers: 14."""
    p = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData", "townsfolk.json")
    try:
        people = json.load(open(p))["people"]
    except (OSError, ValueError, KeyError):
        return {}
    away = [q for q in people if q.get("occupation") == "away" and q.get("body")]
    away.sort(key=lambda q: q["key"])
    adults = [q for q in away if 22 <= q.get("age", 30) <= 62]
    rest = [q for q in away if q not in adults[:len(STYLES) * 2]]
    def rec(q):
        return {"body": q["body"], "mesh": q.get("mesh", "Crowd_11"), "female": q.get("female", 0)}
    k = style_index
    staff = adults[2 * k: 2 * k + 2]
    pas = rest[14 * k: 14 * k + 14]
    return {"barista": [rec(q) for q in staff[:1]], "attendant": [rec(q) for q in staff[1:2]],
            "passengers": [rec(q) for q in pas]}


def main():
    names = [a for a in ARGS if not a.startswith("--") and not (ARGS.index(a) > 0 and ARGS[ARGS.index(a) - 1] == "--samples")]
    styles = names[0].split(",") if names else STYLES
    samples = int(ARGS[ARGS.index("--samples") + 1]) if "--samples" in ARGS else 24
    do_render = "--no-render" not in ARGS and bpy is not None
    VJ = json.load(open(VEST_JSON))
    data = json.load(open(OUT_JSON)) if os.path.isfile(OUT_JSON) else {}
    for style in styles:
        V = VJ[style]
        obs = []
        walls = None
        if bpy is not None:
            BC.clear_scene()
            BC._MATS.clear()
            obs = load_vest(style)
            walls = wall_samples(style, obs, V["room"]["height"] / 100.0)
            if walls:
                log(style, "walls from the model:", sum(1 for w in walls if w[2]), "solid of", len(walls))
        D = Dresser(style, V, walls).run()
        B = D.B
        rec = dict(D.out)
        rec["mesh"] = "/Game/City/Buildings/tr_hall__%s/SM_tr_hall__%s" % (style, style)
        rec["detail"] = rec["mesh"] + "_Detail"
        rec["walls_from_model"] = bool(walls)
        rec["models"] = models(STYLES.index(style))
        data[style] = rec
        if bpy is not None:
            os.makedirs(EXPORT, exist_ok=True)
            report = BC.lint(B)
            with open(os.path.join(EXPORT, "tr_hall__%s.json" % style), "w") as f:
                json.dump({"type": "tr_hall__" + style, "lint": report, "tris": B.detail.tri_count()}, f, indent=1)
            mine = [BC.to_object("SM_tr_hall__" + style, B.m)]
            if B.detail.faces:
                mine.append(BC.to_object("SM_tr_hall__%s_Detail" % style, B.detail))
            BC.export("tr_hall__" + style, mine)
            log(style, "export", report)
            if do_render:
                for s in render(style, B, obs, D, samples):
                    log(style, "shot", os.path.relpath(s, ROOT))
    data["_doc"] = ("Made by blender/transport/halls.py (docs/transport.md 'Halls v3'). Station frame of vestibules.json, UE cm "
                    "and yaw. lamps: real lights (lumens); planters: live plants; staff: barista / attendant spots; seats and "
                    "stands: where waiting passengers sit or stand; counter: the buffet the barista sells over.")
    with open(OUT_JSON, "w") as f:
        json.dump(data, f, indent=1, sort_keys=True, ensure_ascii=False)
    log("wrote", os.path.relpath(OUT_JSON, ROOT))


main()
