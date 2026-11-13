"""The kit every town building of Lyudmilovsk is built with (pure Python, no bpy: test with python3).

A building type is a module in blender/city/buildings/<type>.py with
    INFO = {"name_ru": ..., "name_en": ..., "inspired_by": ..., "style": ..., "use": "residential"|"public"|"mixed"}
    def build(B): ...            # fills the Building B
build_city.py (Blender) turns a Building into the game meshes, blender/export/city/<type>.glb and <type>.json.

Frame of a building: metres, x along the street front (to the right seen from the street), y INTO the plot
(away from the street), z up, origin on the street front line at the middle of the frontage, z 0 = the ground
at the main entrance. Plots in docs/city_plan.md say where that origin goes and which way the street is.

Meshes (geom.Mesh, metre UVs, one material slot per face, citymats.MATERIALS names):
    B.m          the opaque shell (walls, slabs, roofs, columns, stairs) - Nanite, exact collision
    B.detail     small things with no collision (mullions, railings, louvres, signs)
Windows are NOT modelled into B.m as glass: B.window() puts a recessed card (material CT_Window, one shader that
is dark reflective glass by day and a lit room with a curtain at night) into its own mesh, with its light slot.

Lights (the town-life thread's contract): each window has a slot (0..4095) and an id "<flat>/w<n>"; a flat id is
"<storey>-<n>" (local; the plot id is prefixed in the game: "T2-05/3-02/w1"). ULyudWindowLights lights a slot by
writing one texel of a 64x64 texture; UV channel 1 of the window card points at that texel. Stair and public
windows have flat None and ids "stair/w<n>", "pub/w<n>": they are lit by the building's own hours.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "academgorodok"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import geom  # noqa: E402,F401  - re-exported for the building modules
from geom import Mesh, Facade, Frame, box, prism, wall_with_holes, reveal, triangulate, extrude_profile  # noqa: E402,F401

try:
    from character_standard import MIN_DOOR_CLEAR_HEIGHT_M, MIN_DOOR_CLEAR_WIDTH_M  # noqa: E402
except Exception:  # noqa: BLE001
    MIN_DOOR_CLEAR_WIDTH_M, MIN_DOOR_CLEAR_HEIGHT_M = 1.08, 2.10

LIGHT_TEX = 64                 # the light texture is LIGHT_TEX x LIGHT_TEX texels: 4096 slots a building
WINDOW_INSET = 0.10            # the card sits this far behind the wall face at least (in its reveal)
DOOR_W, DOOR_H = max(1.10, MIN_DOOR_CLEAR_WIDTH_M), max(2.20, MIN_DOOR_CLEAR_HEIGHT_M)   # hole of a street door
STEP_RISE, STEP_GOING = 0.16, 0.30        # comfortable stairs: the hero climbs 0.45 m steps, NPCs need gentle ones


def slot_uv(slot):
    return ((slot % LIGHT_TEX + 0.5) / LIGHT_TEX, (slot // LIGHT_TEX + 0.5) / LIGHT_TEX)


class Building:
    def __init__(self, type_id):
        self.type = type_id
        self.m = Mesh()
        self.detail = Mesh()
        self.win = Mesh()                  # window cards
        self.win_slot = []                 # per face of self.win: its light slot
        self.windows = []                  # {slot, id, flat, kind, centre, normal, size}
        self.flats = {}                    # flat id -> {flat, storey, beds, door, entrance, windows[]}
        self.entrances = []                # {id, pos, yaw, width, height, kind, leaves, to}
        self.spots = []                    # {id, kind, pos, yaw}: benches, counters, stalls... for the life sim
        self.lamps = []                    # {pos, lumens, colour, kind, radius}: real lights inside enterable rooms
        self.things = []                   # {id, kind, pos, yaw, props}: what the hero can use inside (docs/city_interiors.md)
        self.footprint = []                # outline [(x, y)] on the ground, CCW
        self.height = 0.0
        self.notes = []
        self._wn = {}

    # ---------------------------------------------------------------- windows and flats
    def window(self, corners, flat=None, kind=None, inset=WINDOW_INSET, cols=1, rows=1, frame=0.05, shape="rect"):
        """A window card on the quad `corners` (4 points CCW seen from OUTSIDE, in the wall's face plane:
        bottom-left, bottom-right, top-right, top-left). The card is pushed `inset` along the inward normal.
        cols x rows: mullion grid drawn as thin frame bars into B.detail (0 = none). shape "round": the card is the
        circle's bounding square and the shader cuts the circle (UV0 runs 2..3 instead of 0..1). Returns its id."""
        kind = kind or ("flat" if flat else "stair")
        p0, p1, p2, p3 = corners
        n = geom.norm(geom.cross(geom.sub(p1, p0), geom.sub(p3, p0)))         # outward
        ins = [(p[0] - n[0] * inset, p[1] - n[1] * inset, p[2] - n[2] * inset) for p in corners]
        slot = len(self.windows)
        key = flat if flat else {"stair": "stair", "public": "pub", "shop": "pub"}.get(kind, kind)
        k = self._wn.get(key, 0) + 1
        self._wn[key] = k
        wid = "%s/w%d" % (key, k)
        o = 2.0 if shape == "round" else 0.0
        self.win.face("CT_Window", ins, [(o, 0), (o + 1, 0), (o + 1, 1), (o, 1)])
        self.win_slot.append(slot)
        c = tuple(sum(p[i] for p in corners) / 4 for i in range(3))
        w = math.dist(p0, p1)
        h = math.dist(p0, p3)
        self.windows.append({"slot": slot, "id": wid, "flat": flat, "kind": kind,
                             "centre": [round(v, 3) for v in c], "normal": [round(v, 3) for v in n],
                             "size": [round(w, 3), round(h, 3)], "shape": shape})
        if flat:
            self.flat(flat)["windows"].append(wid)
        if frame and (cols > 1 or rows > 1 or frame):
            self._mullions(ins, n, cols, rows, frame)
        return wid

    def _mullions(self, q, n, cols, rows, t):
        """Thin frame bars (into B.detail) around and across the card, standing 3 cm proud of it."""
        p0, p1, p2, p3 = q
        off = 0.03

        def P(a, b):           # a along bottom edge 0..1, b up 0..1
            x = [p0[i] + (p1[i] - p0[i]) * a + (p3[i] - p0[i]) * b + n[i] * off for i in range(3)]
            return tuple(x)
        w = math.dist(p0, p1) or 1.0
        h = math.dist(p0, p3) or 1.0
        ta, tb = t / w, t / h
        bars = [(0, 0, 1, tb), (0, 1 - tb, 1, 1), (0, 0, ta, 1), (1 - ta, 0, 1, 1)]
        for i in range(1, cols):
            a = i / cols
            bars.append((a - ta / 2, 0, a + ta / 2, 1))
        for j in range(1, rows):
            b = j / rows
            bars.append((0, b - tb / 2, 1, b + tb / 2))
        for a0, b0, a1, b1 in bars:
            pts = [P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)]
            self.detail.face("CT_Frame", pts)

    def flat(self, fid, storey=None, beds=None, door=None, entrance=None):
        """Declare / update a flat (storey: 0 = ground; beds: how many sleep there; door: the flat's door
        (local xyz); entrance: the id of the street entrance its people use)."""
        f = self.flats.setdefault(fid, {"flat": fid, "storey": None, "beds": 2, "door": None,
                                        "entrance": None, "windows": []})
        if storey is None and f["storey"] is None:
            try:
                storey = int(fid.split("-")[0])
            except ValueError:
                storey = 0
        for k, v in (("storey", storey), ("beds", beds), ("door", door), ("entrance", entrance)):
            if v is not None:
                f[k] = [round(c, 3) for c in v] if k == "door" else v
        return f

    # ---------------------------------------------------------------- facade helpers
    def facade(self, p0, p1, z0, z1, mat, holes=(), depth=0.30, reveal_mat=None, back=True):
        """A straight wall from p0 to p1 (2D, outside on the right walking p0->p1, i.e. a CCW footprint),
        between z0 and z1, `depth` thick, with rectangular holes [(u0, u1, h0, h1)] in facade coordinates
        (u along the wall from p0, h = z). Reveals line the holes. back: also the inner face.
        Returns the Facade (F.to_building((u, y, z)) maps facade coords; y > 0 is outside)."""
        F = Facade(p0, p1)
        m = Mesh()
        wall_with_holes(m, mat, F.length, z0, z1, list(holes))
        for (u0, u1, h0, h1) in holes:
            reveal(m, reveal_mat or mat, u0, u1, h0, h1, depth)
        if back:
            inner = Mesh()
            wall_with_holes(inner, mat, F.length, z0, z1, list(holes))
            # the inner face: mirrored onto y = -depth and facing in
            inner.verts = [(u, -depth, z) for (u, _, z) in inner.verts]
            inner.faces = [(mt, list(reversed(i)), list(reversed(uv))) for mt, i, uv in inner.faces]
            m.extend(inner)
        self.m.extend(F.apply(m))
        return F

    def facade_window(self, F, u0, u1, z0, z1, flat=None, kind=None, cols=1, rows=1, inset=0.18):
        """A window card in the hole (u0, u1, z0, z1) of facade F (made by B.facade)."""
        pts = [F.to_building((u, 0.0, z)) for u, z in ((u0, z0), (u1, z0), (u1, z1), (u0, z1))]
        # seen from outside u runs to the viewer's right, so (u0, z0) is the bottom-left
        return self.window(pts, flat=flat, kind=kind, cols=cols, rows=rows, inset=inset)

    def entrance(self, eid, pos, yaw, width=DOOR_W, height=DOOR_H, kind="street", leaves=1, to=None):
        """A doorway for ALyudDoor (placed by the import): pos = middle of the threshold on the outer wall
        face, yaw = degrees of the outward direction (0 = -y, toward the street; 90 = +x)."""
        self.entrances.append({"id": eid, "pos": [round(v, 3) for v in pos], "yaw": yaw, "width": width,
                               "height": height, "kind": kind, "leaves": leaves, "to": to})

    def spot(self, sid, kind, pos, yaw=0.0):
        """A place for the life simulation: pos on the floor; yaw = degrees of the way the person using it FACES
        (0 = -y, toward the street; 90 = +x), the same convention as entrances."""
        self.spots.append({"id": sid, "kind": kind, "pos": [round(v, 3) for v in pos], "yaw": yaw})

    THING_KINDS = ("seat", "bed", "shelf", "board", "counter", "machine", "play", "sport", "craft", "screen", "take", "door")

    def thing(self, tid, kind, pos, yaw=0.0, **props):
        """Something the hero can use with E (docs/city_interiors.md): pos = the middle of the object (the point the
        hero looks at), yaw = degrees of the way the object FACES (0 = -y, toward the street; 90 = +x). props: free
        fields of the kind (height, topic, role, service, what, item, to). Ids are unique in the building and stable."""
        if kind not in self.THING_KINDS:
            raise ValueError("thing kind %r (one of %s)" % (kind, ", ".join(self.THING_KINDS)))
        if any(t["id"] == tid for t in self.things):
            raise ValueError("thing id %r used twice in %s" % (tid, self.type))
        self.things.append({"id": tid, "kind": kind, "pos": [round(v, 3) for v in pos], "yaw": round(yaw, 1),
                            "props": props})

    def lamp(self, pos, lumens=900.0, colour="#FFD9A8", kind="public", radius=7.0, fixture=True):
        """A real light inside an enterable room (Unreal: a point light, no shadow, drawn within ~40 m). pos = the
        light's centre (put it ~0.3 m under the ceiling). kind "public": on in the building's open hours (all day
        long: interiors need it), "stair"/"lobby": dusk to dawn. fixture: a small opal globe/pendant into B.detail
        (material CT_Opal, which glows)."""
        self.lamps.append({"pos": [round(v, 3) for v in pos], "lumens": lumens, "colour": colour, "kind": kind,
                           "radius": radius})
        if fixture:
            x, y, z = pos
            box(self.detail, "CT_Opal", x - 0.18, x + 0.18, y - 0.18, y + 0.18, z - 0.08, z + 0.08)

    # ---------------------------------------------------------------- solids
    def slab(self, poly, z0, z1, mat="CT_Concrete", m=None):
        prism(m or self.m, mat, poly, z0, z1, top=True, bottom=True)

    def column_round(self, x, y, z0, z1, r, mat="CT_Concrete", segs=12, m=None):
        poly = [(x + r * math.cos(2 * math.pi * k / segs), y + r * math.sin(2 * math.pi * k / segs)) for k in range(segs)]
        prism(m or self.m, mat, poly, z0, z1, top=False, bottom=False)

    def stairs(self, x0, x1, y0, z0, z1, direction=1, mat="CT_Concrete", going=STEP_GOING, m=None):
        """A straight flight across x0..x1 climbing from (y0, z0) along +y (direction 1) or -y (-1) to z1,
        solid to the ground (z0). Returns the y where it ends."""
        m = m or self.m
        rise = z1 - z0
        n = max(1, int(math.ceil(rise / STEP_RISE)))
        r = rise / n
        y = y0
        for i in range(n):
            ya, yb = sorted((y, y + direction * going))
            box(m, mat, x0, x1, ya, yb, z0, z0 + r * (i + 1))
            y += direction * going
        return y

    def railing(self, pts, z, h=1.05, mat="CT_Steel", post=1.2, bar=0.04):
        """A simple rail along the polyline pts [(x, y)] standing on z: top rail, mid rail, posts (B.detail)."""
        d = self.detail
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            L = math.hypot(bx - ax, by - ay)
            if L < 1e-6:
                continue
            ux, uy = (bx - ax) / L, (by - ay) / L
            nx, ny = -uy * bar / 2, ux * bar / 2
            for zz in (z + h - bar, z + h * 0.5):
                quad_bar(d, mat, (ax + nx, ay + ny), (bx + nx, by + ny), (ax - nx, ay - ny), (bx - nx, by - ny), zz, zz + bar)
            k = max(1, int(L / post))
            for i in range(k + 1):
                px, py = ax + ux * L * i / k, ay + uy * L * i / k
                box(d, mat, px - bar / 2, px + bar / 2, py - bar / 2, py + bar / 2, z, z + h)

    def note(self, s):
        self.notes.append(s)

    # ---------------------------------------------------------------- export data
    def data(self, info):
        return {
            "type": self.type, "info": info, "frame": "x along the street front, y into the plot, z up; origin "
            "on the front line, mid-frontage, z 0 = ground at the main entrance",
            "footprint": [[round(x, 3), round(y, 3)] for x, y in self.footprint], "height": round(self.height, 2),
            "entrances": self.entrances, "spots": self.spots, "lamps": self.lamps, "things": self.things,
            "flats": sorted(self.flats.values(), key=lambda f: f["flat"]),
            "windows": self.windows, "light_tex": LIGHT_TEX, "notes": self.notes,
            "tris": {"shell": self.m.tri_count(), "detail": self.detail.tri_count(), "windows": self.win.tri_count()},
        }


def quad_bar(m, mat, a0, a1, b0, b1, z0, z1):
    """A bar of quadrilateral section between the edge a0-a1 and b0-b1 (2D), z0..z1."""
    poly = [a0, a1, b1, b0]
    area = sum(poly[i][0] * poly[(i + 1) % 4][1] - poly[(i + 1) % 4][0] * poly[i][1] for i in range(4))
    if area < 0:
        poly = poly[::-1]
    prism(m, mat, poly, z0, z1, top=True, bottom=True)


def ring(cx, cy, r, segs=32, a0=0.0, a1=2 * math.pi, closed=True):
    """Points on a circle (CCW)."""
    n = segs if closed else segs + 1
    return [(cx + r * math.cos(a0 + (a1 - a0) * k / segs), cy + r * math.sin(a0 + (a1 - a0) * k / segs)) for k in range(n)]


def offset_polyline(pts, d):
    """The polyline pts [(x, y)] moved d to its left (mitred)."""
    out = []
    n = len(pts)
    for i in range(n):
        if i == 0:
            dx, dy = pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]
        elif i == n - 1:
            dx, dy = pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1]
        else:
            ax, ay = pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]
            bx, by = pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]
            la, lb = math.hypot(ax, ay) or 1, math.hypot(bx, by) or 1
            dx, dy = ax / la + bx / lb, ay / la + by / lb
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L, dx / L
        if 0 < i < n - 1:            # mitre
            ax, ay = pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]
            la = math.hypot(ax, ay) or 1
            c = (nx * -ay / la + ny * ax / la)
            nx, ny = nx / max(c, 0.3), ny / max(c, 0.3)
        out.append((pts[i][0] + nx * d, pts[i][1] + ny * d))
    return out


def ribbon_wall(m, mat, pts, z0, z1, uv_off=0.0):
    """Vertical faces along the polyline pts, facing to its RIGHT (walk the outline CCW for an outside face)."""
    u = uv_off
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax, by - ay)
        m.face(mat, [(ax, ay, z0), (bx, by, z0), (bx, by, z1), (ax, ay, z1)],
               [(-u, z0), (-u - L, z0), (-u - L, z1), (-u, z1)])
        u += L
    return u


def band(m, mat, outer, inner, z):
    """A horizontal strip at height z between two polylines of equal length (outer, inner), facing up."""
    for i in range(len(outer) - 1):
        q = [outer[i], outer[i + 1], inner[i + 1], inner[i]]
        area = sum(q[k][0] * q[(k + 1) % 4][1] - q[(k + 1) % 4][0] * q[k][1] for k in range(4))
        if area < 0:
            q = q[::-1]
        m.face(mat, [(x, y, z) for x, y in q])


def band_down(m, mat, outer, inner, z):
    for i in range(len(outer) - 1):
        q = [outer[i], outer[i + 1], inner[i + 1], inner[i]]
        area = sum(q[k][0] * q[(k + 1) % 4][1] - q[(k + 1) % 4][0] * q[k][1] for k in range(4))
        if area > 0:
            q = q[::-1]
        m.face(mat, [(x, y, z) for x, y in q])
