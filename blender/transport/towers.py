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

# per style: the outline (r_out(phi)), the inner shape, the surfaces, the windows, the details
STYLES = {
    "arches":  {"outer": ("square", 3.3), "inner": "square", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "slits", "detail": "pilasters"},
    "dome":    {"outer": ("circle", 4.3), "inner": "circle", "wall": "tx_concrete", "band": "tx_amber",
                "windows": "portholes", "detail": "ribs"},
    "folded":  {"outer": ("zigzag", 4.15, 0.45, 12), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "none", "detail": "none"},
    "lantern": {"outer": ("ngon", 4.4, 12), "inner": "circle", "wall": "glass", "band": "tx_enamel",
                "windows": "none", "detail": "corner_ribs"},
    "saucer":  {"outer": ("circle", 4.3), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "portholes", "detail": "rings"},
    "star":    {"outer": ("star", 4.1, 1.0, 8), "inner": "circle", "wall": "tx_enamel", "band": "tx_amber",
                "windows": "none", "detail": "none"},
    "wing":    {"outer": ("square", 3.3), "inner": "square", "wall": "tx_enamel", "band": "tx_mosaic",
                "windows": "slits", "detail": "fins"},
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
        a = (phi % (2 * math.pi / n)) - math.pi / n
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
            box(bm, Vector((r * math.cos(a), r * math.sin(a), (z0 + z1) / 2)), (0.5, 0.5, z1 - z0), mi_band,
                Matrix.Rotation(a, 3, "Z"))
    elif d == "fins":
        for sx in (-1, 1):
            box(bm, Vector((sx * (spec[1] + 0.35), 0, (z0 + z1) / 2)), (0.7, 0.18, z1 - z0), mi_wall)
    elif d == "rings":
        band(bm, spec, z1 - 0.35, z1 - 0.15, 0.25, mi_band)


def window_ops(st, z0, z1):
    """Openings in a shaft piece: (kind, centre, normal, w, h)."""
    ops = []
    w = st["windows"]
    spec = st["outer"]
    zc = (z0 + z1) / 2
    if z1 - z0 < 2.0:
        return ops
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
