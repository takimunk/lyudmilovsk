"""QA r9: keeps wall-hung kit pieces (pictures, posters, boards, maps, clocks, signs, mailboxes) off window glass.

    from _wallhang import hung
    build = hung(build)        # at the bottom of a building module

While build() runs, every call of a wall-hung kit function whose spot has glazing right behind it is moved along the
wall (up to MAXU, nearest first) to a stretch of solid wall that is wide enough, free of other pieces and not behind a
corner; a `thing` declared at the old spot follows. Kitchen runs whose wall cupboards would cover glass drop them.
Pieces with no solid wall in reach stay where they are."""
import math
import inspect

import interior_kit as kit

NAMES = ("poster", "picture", "notice_board", "wall_map", "menu_board", "sign_plate", "banner", "blackboard",
         "wall_clock", "mailboxes", "mirror")
MAXU = 2.5


def _tree(me):
    from mathutils.bvhtree import BVHTree
    tris = []
    for f in me.faces:
        for j in range(1, len(f[1]) - 1):
            tris.append((f[1][0], f[1][j], f[1][j + 1]))
    return BVHTree.FromPolygons([tuple(v) for v in me.verts], tris) if tris else None


class WallHang:
    def __init__(self, B):
        self.B = B
        self.names = [n for n in NAMES + ("kitchen_unit",) if hasattr(kit, n)]
        self.sig = {n: inspect.signature(getattr(kit, n)) for n in self.names}
        self.orig = {n: getattr(kit, n) for n in self.names}
        self.moves = []
        self.placed = []
        self._gn = self._sn = -1
        self.glass = self.shell = None
        self.thing0 = B.thing
        B.thing = self._thing
        for n in self.names:
            setattr(kit, n, self._wrap(n))

    def restore(self):
        for n, f in self.orig.items():
            setattr(kit, n, f)
        self.B.thing = self.thing0

    def trees(self):
        if self._gn != len(self.B.win.faces):
            self._gn = len(self.B.win.faces)
            self.glass = _tree(self.B.win)
        if self._sn != len(self.B.m.faces):
            self._sn = len(self.B.m.faces)
            self.shell = _tree(self.B.m)

    def _thing(self, tid, kind, pos, yaw=0.0, **props):
        for (ox, oy, zm, dx, dy) in self.moves:
            if math.hypot(pos[0] - ox, pos[1] - oy) < 0.4 and abs(pos[2] - zm) < 1.0:
                pos = (pos[0] + dx, pos[1] + dy, pos[2])
                break
        return self.thing0(tid, kind, pos, yaw, **props)

    def ok(self, x, y, fx, fy, w, zlo, zhi, need_wall=True):
        from mathutils import Vector
        self.trees()
        tx, ty = -fy, fx
        d = Vector((-fx, -fy, 0.0))
        for k in range(7):
            u = -w / 2 - 0.03 + (w + 0.06) * k / 6
            for q in range(5):
                z = zlo + (zhi - zlo) * q / 4
                o = Vector((x + tx * u + fx * 0.04, y + ty * u + fy * 0.04, z))
                if self.glass is not None and self.glass.ray_cast(o, d, 0.45)[0] is not None:
                    return False
                if need_wall and (self.shell is None or self.shell.ray_cast(o, d, 0.3)[0] is None):
                    return False
        return True

    def blocked(self, x, y, fx, fy, zmid, u, w):
        """A solid thing between the old spot and the new one (a corner, a door jamb, a wardrobe)."""
        from mathutils import Vector
        self.trees()
        if self.shell is None:
            return False
        tx, ty = -fy, fx
        sg = 1 if u > 0 else -1
        o = Vector((x + fx * 0.1, y + fy * 0.1, zmid))
        h = self.shell.ray_cast(o, Vector((tx * sg, ty * sg, 0.0)), abs(u) + w / 2 + 0.03)
        return h[0] is not None

    def _wrap(self, name):
        sig = self.sig[name]
        wh = self

        def fn(B, x, y, z=0.0, yaw=0.0, *a, **kw):
            bound = sig.bind(B, x, y, z, yaw, *a, **kw)
            bound.apply_defaults()
            ar = bound.arguments
            ra = math.radians(yaw)
            fx, fy = math.sin(ra), -math.cos(ra)
            tx, ty = -fy, fx
            if name == "kitchen_unit":
                if kw.get("wall_cupboards", True) and ar.get("wall_cupboards", True):
                    n = len(ar["modules"].split(",")) if isinstance(ar["modules"], str) else len(ar["modules"])
                    wk = 0.6 * n
                    wh.trees()
                    if not wh.ok(x, y, fx, fy, wk, z + 1.45, z + 2.15, need_wall=False):
                        kw["wall_cupboards"] = False
                return wh.orig[name](B, x, y, z, yaw, *a, **kw)
            w = ar.get("w") or (2 * ar["r"] if name == "wall_clock" else 1.0)
            if name == "blackboard":
                zlo, zhi = z + ar["zb"], z + ar["zb"] + ar["h"]
            else:
                h = ar.get("h") or w * 0.7
                zc = ar.get("zc") or 1.7
                zlo, zhi = z + zc - h / 2, z + zc + h / 2
            if name == "sign_plate" and ar.get("hang"):
                return wh.orig[name](B, x, y, z, yaw, *a, **kw)
            nx, ny = x, y
            if zlo > 0.3 and not wh.ok(x, y, fx, fy, w, zlo, zhi):
                best = None
                for step in range(1, int(MAXU / 0.1) + 1):
                    for sg in (1, -1):
                        u = sg * step * 0.1
                        cx_, cy_ = x + tx * u, y + ty * u
                        if wh.ok(cx_, cy_, fx, fy, w, zlo, zhi) and not wh.blocked(x, y, fx, fy, (zlo + zhi) / 2, u, w) \
                                and not any(abs((cx_ - px) * ptx + (cy_ - py) * pty) < (w + pw) / 2 + 0.1 and
                                            abs((cx_ - px) * (-pty) + (cy_ - py) * ptx) < 0.5 and zlo < pz1 and zhi > pz0
                                            for (px, py, ptx, pty, pw, pz0, pz1) in wh.placed):
                            best = (cx_, cy_)
                            break
                    if best:
                        break
                if best:
                    nx, ny = best
                    wh.moves.append((x, y, (zlo + zhi) / 2, nx - x, ny - y))
            if zlo > 0.3:
                wh.placed.append((nx, ny, tx, ty, w, zlo, zhi))
            return wh.orig[name](B, nx, ny, z, yaw, *a, **kw)
        return fn


from citykit import prism  # noqa: E402


def backboards(B):
    """QA r8: the wings are mostly ribbon glazing, so wall pieces hung by their axis position often land on a window
    and float in front of the glass. Every wall-hung piece with glass right behind it gets a thin wooden backing board
    (the piece's width + 4 cm, 3 cm thick) so it hangs on something."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    if not B.win.faces:
        return
    tris = []
    for f in B.win.faces:
        for j in range(1, len(f[1]) - 1):
            tris.append((f[1][0], f[1][j], f[1][j + 1]))
    tree = BVHTree.FromPolygons([tuple(v) for v in B.win.verts], tris)
    for it in getattr(B, 'kit_items', []):
        if it["name"] not in NAMES or it["z0"] < 0.3:
            continue
        f = it["foot"]
        ed = [math.dist(f[i], f[(i + 1) % 4]) for i in range(4)]
        if min(ed[0], ed[1]) > 0.2:
            continue
        i0 = 0 if ed[0] >= ed[1] else 1
        a, b = f[i0], f[i0 + 1]
        L, t = max(ed[0], ed[1]), min(ed[0], ed[1])
        e = ((b[0] - a[0]) / L, (b[1] - a[1]) / L)
        cx, cy = sum(q[0] for q in f) / 4, sum(q[1] for q in f) / 4
        n = (-e[1], e[0])
        side = 0
        for sg in (1, -1):
            for z in (it["z0"] + 0.02, (it["z0"] + it["z1"]) / 2, it["z1"] - 0.02):
                if tree.ray_cast(Vector((cx, cy, z)), Vector((n[0] * sg, n[1] * sg, 0.0)), 0.35)[0] is not None:
                    side = sg
        if not side:
            continue
        h = L / 2 + 0.02
        d0, d1 = t / 2 - 0.004, t / 2 + 0.03
        poly = [(cx - e[0] * h + n[0] * side * d0, cy - e[1] * h + n[1] * side * d0),
                (cx + e[0] * h + n[0] * side * d0, cy + e[1] * h + n[1] * side * d0),
                (cx + e[0] * h + n[0] * side * d1, cy + e[1] * h + n[1] * side * d1),
                (cx - e[0] * h + n[0] * side * d1, cy - e[1] * h + n[1] * side * d1)]
        if sum(poly[i][0] * poly[(i + 1) % 4][1] - poly[(i + 1) % 4][0] * poly[i][1] for i in range(4)) < 0:
            poly.reverse()
        prism(B.detail, "CT_WoodLight", poly, it["z0"] - 0.02, it["z1"] + 0.02, top=True, bottom=True)


def hung(build):
    def wrapped(B, *a, **k):
        wh = WallHang(B)
        try:
            r = build(B, *a, **k)
        finally:
            wh.restore()
        backboards(B)
        print("WALLHANG moved %d pieces off the glass" % len(wh.moves))
        return r
    wrapped.__name__ = getattr(build, "__name__", "build")
    wrapped.__doc__ = build.__doc__
    return wrapped
