"""The stations' towers: a kit per vestibule style (blender/transport/vestibules.py calls it), stacked by the game to
the station's height (docs/transport.md, "Station layout v2"). Clean meshes of our own in the style of each vestibule;
the working lift and stair inside are the procedural part's.

Parts, each with its origin on the tower's axis at the part's bottom, +X along the line (the station frame):
  SM_TR_Tower_<Style>_Foot    3.5 m: the street door (a Double hole, the game fits an ALyudDoor), a plinth
  SM_TR_Tower_<Style>_Shaft3  3.0 m and _Shaft1 1.0 m: the shaft, stacked (never stretched)
  SM_TR_Tower_<Style>_Neck    1.5 m: flares out under the terrace; its top at the terrace's underside (-0.6 m)
  SM_TR_Tower_Saucer_Deck2    the saucer's lower boarding deck (Zenit's second line), a shaft piece with its own
                              doors out onto a ring deck
Inside every part a clear zone of a 7.4 m circle or a 5.4 m square (the lift and the stair); walls 25-30 cm; outside
painted surfaces with metre UVs (TR_Tx_<surface>: the game's tiled textures), inside plaster (TR_Interior).
"""
import bmesh
import bpy
import math
from mathutils import Matrix, Vector

FOOT_H, NECK_H = 3.5, 1.5
SHAFT = (3.0, 1.0)
CLEAR_SQ = 2.7                 # half the clear square
CLEAR_R = 3.72                 # the clear circle

# per style: the outline (r_out(phi)), the inner shape, the surfaces, the windows, the details, and what makes it
# speak its vestibule's language (feature: fins, legs, discs; each with its own shape on the foot, the shaft, the neck)
STYLES = {
    # the arches: the shaft stands on four parabolic legs and opens into four corbel arches under the terrace
    "arches":  {"outer": ("square", 3.3), "inner": "square", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "slits", "detail": "pilasters", "feature": "legs"},
    # the dome: a ribbed drum, the ribs flare into buttresses at the foot and corbels under the terrace; portholes
    "dome":    {"outer": ("circle", 4.3), "inner": "circle", "wall": "tx_concrete", "band": "tx_amber",
                "windows": "portholes2", "detail": "none",
                "feature": ("fins", {"n": 12, "phase": math.pi / 12, "t": 0.24, "foot": (1.3, 0.12), "shaft": (0.12, 0.12),
                                     "neck": (0.12, 1.5)})},
    # the folded plates: twelve thin plates on the zigzag's ridges, fanning out at the foot and under the terrace
    "folded":  {"outer": ("zigzag", 4.15, 0.45, 12), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "none", "detail": "none",
                "feature": ("fins", {"n": 12, "phase": math.pi / 12, "t": 0.14, "foot": (1.9, 0.35), "shaft": (0.35, 0.35),
                                     "neck": (0.35, 2.0)})},
    # the lantern: amber-lit glass facets between ribs, amber bands every metre, a solid plinth; ribs curl out on top
    "lantern": {"outer": ("ngon", 4.4, 12), "inner": "circle", "wall": "glass", "band": "tx_amber",
                "windows": "none", "detail": "none", "rings_every": 1.0, "plinth": 1.2,        # its fins are the ribs
                "feature": ("fins", {"n": 12, "phase": math.pi / 12, "t": 0.2, "foot": (0.9, 0.18), "shaft": (0.18, 0.18),
                                     "neck": (0.18, 1.5)})},
    # the saucer: a ringed column with a small flying saucer round every 3 m and a big one at the foot and the neck
    "saucer":  {"outer": ("circle", 4.3), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "portholes", "detail": "none", "feature": "discs"},
    # the star: eight star points running up the shaft, growing into big fins at the foot and under the terrace
    "star":    {"outer": ("star", 4.1, 1.0, 8), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "none", "detail": "none",
                "feature": ("fins", {"n": 8, "phase": math.pi / 8, "t": 0.3, "foot": (2.8, 0.6), "shaft": (0.6, 0.6),
                                     "neck": (0.6, 2.6)})},
    # the wing: two swept wings along the sides, wide at the foot, thin up the shaft, sweeping out under the terrace
    "wing":    {"outer": ("square", 3.3), "inner": "square", "wall": "tx_enamel", "band": "tx_mosaic",
                "windows": "slits", "detail": "none",
                "feature": ("fins", {"n": 2, "phase": math.pi / 2, "t": 0.4, "foot": (3.4, 0.7), "shaft": (0.7, 0.7),
                                     "neck": (0.7, 3.2), "curve": 2.0})},
}
SEG = 48


def r_out(spec, phi):
    kind = spec[0]
    if kind == "circle":
        return spec[1]
    if kind == "square":
        return spec[1] / max(abs(math.cos(phi)), abs(math.sin(phi)))
    if kind == "ngon":
        n = spec[2]
        a = ((phi - math.pi / n) % (2 * math.pi / n)) - math.pi / n      # the corners at pi/n + k 2pi/n
        return spec[1] * math.cos(math.pi / n) / math.cos(a)
    if kind == "zigzag":
        n = spec[3]
        f = (phi * n / (2 * math.pi)) % 1.0
        return spec[1] + spec[2] * (1 - abs(2 * f - 1))
    if kind == "star":
        n = spec[3]
        f = (phi * n / (2 * math.pi)) % 1.0
        return spec[1] + spec[2] * (1 - abs(2 * f - 1)) ** 2
    raise ValueError(kind)


def r_in(kind, phi):
    if kind == "square":
        return CLEAR_SQ / max(abs(math.cos(phi)), abs(math.sin(phi)))
    return CLEAR_R


def angles(spec):
    """The ring's angles: the corners of a polygon outline kept exactly."""
    base = [2 * math.pi * i / SEG for i in range(SEG)]
    if spec[0] == "square":
        base += [math.pi / 4 + i * math.pi / 2 for i in range(4)]
    if spec[0] == "ngon":
        base = [2 * math.pi * i / spec[2] + math.pi / spec[2] for i in range(spec[2])]
        base += [2 * math.pi * (i + 0.5) / spec[2] + math.pi / spec[2] for i in range(spec[2])]
    if spec[0] in ("zigzag", "star"):
        n = spec[3]
        base = [2 * math.pi * k / (4 * n) for k in range(4 * n)]
    return sorted(set(round(a, 6) for a in base))


class Kit:
    def __init__(self, mats):
        self.mats = mats            # name -> slot index
        self.bm = bmesh.new()

    def m(self, name):
        return self.mats[name]


def ring(bm, spec, inner, z0, z1, mi_out, mi_in, mi_top, scale0=1.0, scale1=1.0):
    """The tower's wall from z0 to z1 (the outline scaled by scale0 at the bottom, scale1 at the top)."""
    ang = angles(spec)
    n = len(ang)
    ob = [bm.verts.new((r_out(spec, a) * scale0 * math.cos(a), r_out(spec, a) * scale0 * math.sin(a), z0)) for a in ang]
    ot = [bm.verts.new((r_out(spec, a) * scale1 * math.cos(a), r_out(spec, a) * scale1 * math.sin(a), z1)) for a in ang]
    ib = [bm.verts.new((r_in(inner, a) * math.cos(a), r_in(inner, a) * math.sin(a), z0)) for a in ang]
    it = [bm.verts.new((r_in(inner, a) * math.cos(a), r_in(inner, a) * math.sin(a), z1)) for a in ang]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ob[i], ob[j], ot[j], ot[i])).material_index = mi_out
        bm.faces.new((ib[j], ib[i], it[i], it[j])).material_index = mi_in
        bm.faces.new((ot[i], ot[j], it[j], it[i])).material_index = mi_top
        bm.faces.new((ob[j], ob[i], ib[i], ib[j])).material_index = mi_top


def band(bm, spec, z0, z1, out, mi):
    """A ring band standing out of the wall by `out`, from z0 to z1 (closed, collides)."""
    ang = angles(spec)
    n = len(ang)
    rr = lambda a, k: r_out(spec, a) * k
    lo_in = [bm.verts.new((rr(a, 1.0) * math.cos(a) * 0.999, rr(a, 1.0) * math.sin(a) * 0.999, z0)) for a in ang]
    hi_in = [bm.verts.new((rr(a, 1.0) * math.cos(a) * 0.999, rr(a, 1.0) * math.sin(a) * 0.999, z1)) for a in ang]
    lo_o = [bm.verts.new(((rr(a, 1.0) + out) * math.cos(a), (rr(a, 1.0) + out) * math.sin(a), z0)) for a in ang]
    hi_o = [bm.verts.new(((rr(a, 1.0) + out) * math.cos(a), (rr(a, 1.0) + out) * math.sin(a), z1)) for a in ang]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo_o[i], lo_o[j], hi_o[j], hi_o[i])).material_index = mi
        bm.faces.new((hi_o[i], hi_o[j], hi_in[j], hi_in[i])).material_index = mi
        bm.faces.new((lo_o[j], lo_o[i], lo_in[i], lo_in[j])).material_index = mi


def details(bm, st, z0, z1, mi_wall, mi_band, beam, box):
    spec = st["outer"]
    d = st["detail"]
    if d == "ribs":
        for i in range(16):
            a = 2 * math.pi * i / 16
            r = r_out(spec, a) + 0.1
            beam(bm, Vector((r * math.cos(a), r * math.sin(a), z0)), Vector((r * math.cos(a), r * math.sin(a), z1)), 0.22, 0.22, mi_wall)
    elif d == "corner_ribs":
        n = spec[2]
        for i in range(n):
            a = 2 * math.pi * i / n + math.pi / n
            r = spec[1] + 0.05
            beam(bm, Vector((r * math.cos(a), r * math.sin(a), z0)), Vector((r * math.cos(a), r * math.sin(a), z1)), 0.2, 0.2, mi_band)
    elif d == "pilasters":
        for i in range(4):
            a = math.pi / 4 + i * math.pi / 2
            r = r_out(spec, a) + 0.05
            # wider than the legs that run into them, so their sides don't share a plane
            box(bm, Vector((r * math.cos(a), r * math.sin(a), (z0 + z1) / 2)), (0.6, 0.6, z1 - z0), mi_band,
                Matrix.Rotation(a, 3, "Z"))
    elif d == "fins":
        for sx in (-1, 1):
            box(bm, Vector((sx * (spec[1] + 0.35), 0, (z0 + z1) / 2)), (0.7, 0.18, z1 - z0), mi_wall)
    elif d == "rings":
        band(bm, spec, z1 - 0.35, z1 - 0.15, 0.25, mi_band)


def fins(bm, st, z0, z1, depth, mi, t, n, phase, curve=1.0, K=8, mi_edge=None):
    """n plates standing out of the wall from z0 to z1, depth(f) out at f = 0..1 up the piece (f**curve between)."""
    spec = st["outer"]
    for i in range(n):
        a = phase + 2 * math.pi * i / n
        d = Vector((math.cos(a), math.sin(a), 0))
        u = Vector((-d.y, d.x, 0))
        rb = r_out(spec, a)          # from the wall's face out (sunk into it, its caps lay on the wall's top: flicker)
        rows = []
        for k in range(K + 1):
            f = k / K
            dep = depth[0] + (depth[1] - depth[0]) * (f ** curve if depth[1] > depth[0] else 1 - (1 - f) ** curve)
            z = z0 + (z1 - z0) * f
            rows.append([bm.verts.new(d * r + u * s + Vector((0, 0, z))) for r in (rb, rb + dep) for s in (-t / 2, t / 2)])
        # rows: [inner-, inner+, outer-, outer+]
        for k in range(K):
            A, B = rows[k], rows[k + 1]
            for q, m_ in (((A[0], A[2], B[2], B[0]), mi), ((A[3], A[1], B[1], B[3]), mi),
                          ((A[2], A[3], B[3], B[2]), mi if mi_edge is None else mi_edge)):
                bm.faces.new(q).material_index = m_
        bm.faces.new((rows[0][0], rows[0][1], rows[0][3], rows[0][2])).material_index = mi
        bm.faces.new((rows[-1][2], rows[-1][3], rows[-1][1], rows[-1][0])).material_index = mi


def legs(bm, st, z0, z1, up, mi, beam):
    """The arches' parabolic legs at the four corners: down to the ground from the foot (up=False), or up and out
    under the terrace from the neck (up=True)."""
    spec = st["outer"]
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        d = Vector((math.cos(a), math.sin(a), 0))
        rc = r_out(spec, a) - 0.1
        pts = []
        for k in range(11):
            s = k / 10
            if up:
                pts.append(d * (rc + 2.4 * s * s) + Vector((0, 0, z0 + (z1 - z0) * s)))
            else:
                pts.append(d * (rc + 2.6 * (1 - s) ** 2) + Vector((0, 0, z0 + (z1 - z0) * s)))
        for p, q in zip(pts, pts[1:]):
            beam(bm, p, q, 0.5, 0.42, mi)


def disc(bm, st, z, out, thick, mi, mi_edge):
    """A flying-saucer disc round the shaft at z: thick at the wall, thin at its rim, an amber rim."""
    spec = st["outer"]
    ang = angles(spec)
    n = len(ang)
    rr = lambda a: r_out(spec, a)
    rows = []
    for (rk, dz) in ((0.0, -thick / 2), (0.0, thick / 2), (out, thick * 0.15), (out, -thick * 0.15)):
        rows.append([bm.verts.new(((rr(a) - 0.02 + rk) * math.cos(a), (rr(a) - 0.02 + rk) * math.sin(a), z + dz)) for a in ang])
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((rows[1][i], rows[1][j], rows[2][j], rows[2][i])).material_index = mi
        bm.faces.new((rows[3][i], rows[3][j], rows[0][j], rows[0][i])).material_index = mi
        bm.faces.new((rows[2][i], rows[2][j], rows[3][j], rows[3][i])).material_index = mi_edge


def fin_angles(st):
    f = st.get("feature")
    if isinstance(f, tuple) and f[0] == "fins":
        return [f[1]["phase"] + 2 * math.pi * i / f[1]["n"] for i in range(f[1]["n"])]
    if f == "legs":
        return [math.pi / 4 + i * math.pi / 2 for i in range(4)]
    return []


def feature(bm, st, piece, z0, z1, mi_w, mi_b, beam):
    """The style's own expression on a piece: piece is "foot", "shaft" or "neck"."""
    f = st.get("feature")
    if piece == "neck":
        z1 += 0.03          # up into the terrace's slab: their tops don't lie on the neck's flared top (the lint)
    if isinstance(f, tuple) and f[0] == "fins":
        c = f[1]
        fins(bm, st, z0, z1, c[piece], mi_w, c["t"], c["n"], c["phase"], c.get("curve", 1.6), mi_edge=mi_b)
    elif f == "legs":
        if piece == "foot":
            legs(bm, st, z0, z1, False, mi_w, beam)
        elif piece == "neck":
            legs(bm, st, z0, z1, True, mi_w, beam)
    elif f == "discs":
        if piece == "foot":
            disc(bm, st, z1 - 0.35, 2.4, 0.5, mi_w, mi_b)
        elif piece == "neck":
            disc(bm, st, z0 + 0.4, 2.0, 0.45, mi_w, mi_b)
        elif z1 - z0 >= 2.5:
            disc(bm, st, (z0 + z1) / 2 + 0.6, 1.2, 0.3, mi_w, mi_b)
    if st.get("rings_every") and piece == "shaft":
        z = z0 + st["rings_every"]
        while z < z1 - 0.01:
            band(bm, st["outer"], z - 0.06, z + 0.06, 0.1, mi_b)
            z += st["rings_every"]


def podium(bm, st, steps, mi_top, mi_side, mi_band, skirt=2.0):
    """The stepped platform the tower stands on: `steps` steps of 15 cm up to its top, a skirt into the ground."""
    spec = st["outer"]
    ang = angles(spec)
    n = len(ang)
    ext = 0.0
    for k in range(steps):
        r_add = 3.0 - 0.45 * k
        z0 = -skirt if k == 0 else 0.15 * k
        z1 = 0.15 * (k + 1)
        b = [bm.verts.new(((r_out(spec, a) + r_add) * math.cos(a), (r_out(spec, a) + r_add) * math.sin(a), z0)) for a in ang]
        t = [bm.verts.new(((r_out(spec, a) + r_add) * math.cos(a), (r_out(spec, a) + r_add) * math.sin(a), z1)) for a in ang]
        bm.faces.new(t).material_index = mi_top
        bm.faces.new(b[::-1]).material_index = mi_side
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((b[i], b[j], t[j], t[i])).material_index = mi_band if k == steps - 1 else mi_side
    return 0.15 * steps


def window_ops(st, z0, z1):
    """Openings in a shaft piece: (kind, centre, normal, w, h)."""
    ops = []
    w = st["windows"]
    spec = st["outer"]
    zc = (z0 + z1) / 2
    if z1 - z0 < 2.0:
        return ops
    if w == "portholes2":
        for row, zz in ((0, z0 + (z1 - z0) * 0.3), (1, z0 + (z1 - z0) * 0.72)):
            for i in range(8):
                a = (i + 0.5 * row) * math.pi / 4 + math.pi / 16
                n = Vector((math.cos(a), math.sin(a), 0))
                ops.append(("round", n * (r_out(spec, a) + r_in(st["inner"], a)) / 2 + Vector((0, 0, zz)), n, 0.55, 0.55))
    if w == "portholes":
        for i in range(4):
            a = math.pi / 4 + i * math.pi / 2
            n = Vector((math.cos(a), math.sin(a), 0))
            ops.append(("round", n * (r_out(spec, a) + r_in(st["inner"], a)) / 2 + Vector((0, 0, zc)), n, 0.8, 0.8))
    elif w == "slits":
        for i in range(4):
            a = i * math.pi / 2
            n = Vector((math.cos(a), math.sin(a), 0))
            for s in (-1.2, 1.2):
                u = Vector((-n.y, n.x, 0))
                ops.append(("box", n * (r_out(spec, a) + r_in(st["inner"], a)) / 2 + u * s + Vector((0, 0, zc)), n, 0.35, 1.8))
    return ops
