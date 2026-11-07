"""Shared helpers of the restaurant «Янтарь» Blender builds (building.py, preview.py): the material slots (RS_*), the wall
builder (cells between openings, real thickness, reveals, panes), polar strips for round floors/ceilings/soffits, convex
plan regions, and the glTF export. The bmesh primitives, metre UVs and the writer come from blender/diner/diner_common.py.

Frame: metres, origin on the finished floor at the centre of the drum, +x the sea (the terrace front), -y the main door,
z up; exported with export_yup like the diner, so Unreal gets x -> X, y -> -Y.

MAT_SPEC[name] = (hex colour, roughness, metallic, emissive); MATS order = material index (the importer makes flat
material instances from it, as import_diner.py does with DN_*).
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "diner"))
import diner_common as D  # noqa: E402
from diner_common import quad, box, bx, tube, sphere, prism, lathe, metre_uv, reset, _lin  # noqa: E402,F401

ROOT = D.ROOT
OUT = os.path.join(ROOT, "design", "restaurant", "game")

# name: (hex, roughness, metallic, emissive)
MAT_SPEC = {
    "RS_Concrete": ("#CFC9BC", 0.85, 0.0, 0.0),      # light concrete: sill band, roof edge and rim, columns, curbs
    "RS_RoofTop": ("#8C8781", 0.9, 0.0, 0.0),        # the disc roof's top
    "RS_Soffit": ("#ECE7DD", 0.8, 0.0, 0.0),         # the overhang's painted underside and its ribs
    "RS_Bronze": ("#3A2E25", 0.35, 0.8, 0.0),        # dark bronze anodised aluminium: mullions, transoms, frames, rails
    "RS_Glass": ("#8FA7A8", 0.05, 0.0, 0.0),         # panes: the _Glass mesh
    "RS_Paving": ("#B9B1A3", 0.8, 0.0, 0.0),         # the podium's top
    "RS_Granite": ("#6F6963", 0.7, 0.0, 0.0),        # podium sides, steps, thresholds, the kitchen plinth
    "RS_AmberTile": ("#B9722A", 0.45, 0.0, 0.0),     # the kitchen block's ochre ceramic cladding
    "RS_Roof": ("#55524E", 0.9, 0.0, 0.0),           # the kitchen's flat roof
    "RS_Plaster": ("#EDE6D8", 0.85, 0.0, 0.0),       # plastered walls (hall chord wall, kitchen upper walls)
    "RS_Ceiling": ("#F2EEE6", 0.85, 0.0, 0.0),       # the hall's ceiling between the ribs, the oculus well
    "RS_Terrazzo": ("#CFC7BA", 0.35, 0.0, 0.0),      # the hall floor (light warm grey)
    "RS_TerrazzoAmber": ("#C47F35", 0.3, 0.0, 0.0),  # the amber ring inlay
    "RS_TerrazzoDark": ("#5D554E", 0.35, 0.0, 0.0),  # the dark ring round the planter
    "RS_KitchenTile": ("#D6D7D1", 0.45, 0.0, 0.0),   # the kitchen floor
    "RS_WallTile": ("#EFEEE7", 0.3, 0.0, 0.0),       # white wall tiles in the kitchen
    "RS_Steel": ("#B7BABD", 0.32, 1.0, 0.0),         # stainless: the pass, kitchen kit, the sign frame
    "RS_Walnut": ("#5C3B22", 0.45, 0.0, 0.0),        # walnut veneer: counters, chairs, the bench top
    "RS_DarkWood": ("#2F2119", 0.5, 0.0, 0.0),       # counter tops, chair legs
    "RS_Linen": ("#F3F0E8", 0.85, 0.0, 0.0),         # tablecloths
    "RS_Soil": ("#3A2B1F", 0.95, 0.0, 0.0),          # the planter's flat soil (plants come from the game)
    "RS_Dark": ("#1F1D1C", 0.6, 0.0, 0.0),           # the range's iron, rubber
    "RS_Enamel": ("#E9E5DA", 0.3, 0.0, 0.0),         # the fridge
    "RS_Copper": ("#A8633A", 0.35, 1.0, 0.0),        # pots
    "RS_Bottle": ("#2E4A30", 0.1, 0.0, 0.0),         # bottles behind the bar (opaque)
    "RS_BottleAmber": ("#7A4818", 0.1, 0.0, 0.0),
    "RS_Opal": ("#FFF1D6", 0.4, 0.0, 6.0),           # opal glass lamps (lit), like DN_Opal
    "RS_HeatLamp": ("#FF7A3A", 0.3, 0.0, 4.0),       # the pass's heat lamps
    "RS_SignGlow": ("#E8861C", 0.4, 0.0, 0.0),       # the sign's letters: Unreal drives the glow
}
MATS = list(MAT_SPEC)


def M(name):
    return MATS.index(name)


# ---- faces ------------------------------------------------------------------------------------------------------------

def oq(bm, pts, mi, want):
    """A face through pts with its normal along want (pts reversed if needed); duplicate corners are dropped."""
    vs = []
    for p in pts:
        v = Vector(p)
        if not vs or (v - vs[-1]).length > 1e-6:
            vs.append(v)
    if len(vs) > 2 and (vs[0] - vs[-1]).length < 1e-6:
        vs.pop()
    if len(vs) < 3:
        return None
    n = Vector((0, 0, 0))
    for i in range(len(vs)):                       # Newell's normal (works for any planar polygon)
        a, b = vs[i], vs[(i + 1) % len(vs)]
        n += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
    if n.dot(Vector(want)) < 0:
        vs.reverse()
    f = bm.faces.new([bm.verts.new(v) for v in vs])
    f.material_index = mi
    return f


def lathe_p(bm, prof, mats, seg=24, c=(0, 0, 0), cap0=True, cap1=True, phase=0.0):
    """diner_common.lathe with the rings turned by phase (radians): a lathe with seg=4 and phase pi/4 is a square."""
    if isinstance(mats, int):
        mats = [mats]
    c = Vector(c)
    rings = [[bm.verts.new(c + Vector((r * math.cos(phase + 2 * math.pi * i / seg), r * math.sin(phase + 2 * math.pi * i / seg), z)))
              for i in range(seg)] for r, z in prof]
    for k in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i])).material_index = mats[min(k, len(mats) - 1)]
    if cap0:
        bm.faces.new(rings[0][::-1]).material_index = mats[0]
    if cap1:
        bm.faces.new(rings[-1]).material_index = mats[-1]


def mark(bm):
    return len(bm.verts)


def place(bm, start, mat4):
    """Transforms every vertex added since mark() by mat4."""
    bm.verts.ensure_lookup_table()
    bmesh.ops.transform(bm, matrix=mat4, verts=bm.verts[start:])


# ---- plan geometry ----------------------------------------------------------------------------------------------------

def U(deg):
    a = math.radians(deg)
    return Vector((math.cos(a), math.sin(a)))


def exit_dist(o, d, planes):
    """From o (inside the convex region {p: p.u <= a for all planes}) along unit d: the distance to its boundary."""
    best = 1e9
    for u, a in planes:
        k = d.dot(u)
        if k > 1e-9:
            best = min(best, (a - o.dot(u)) / k)
    return best


def poly48(R, deg):
    """The point at polar angle deg on the 48-gon of circumradius R with corners at multiples of 7.5 deg (a seg=48 lathe)."""
    k = math.floor(deg / 7.5 + 1e-9)
    mid = (k + 0.5) * 7.5
    r = R * math.cos(math.radians(3.75)) / math.cos(math.radians(deg - mid))
    return U(deg) * r


def angles(lo, hi, step, extra=()):
    a = set()
    k = math.ceil(lo / step - 1e-9)
    while k * step <= hi + 1e-9:
        a.add(round(k * step, 6))
        k += 1
    for e in extra:
        if lo - 1e-9 <= e <= hi + 1e-9:
            a.add(round(e, 6))
    return sorted(a)


def strips(bm, angs, rings, mats, want_z, closed):
    """Faces between consecutive polar angles (deg) and consecutive rings; rings: f(deg) -> (x, y, z)."""
    pairs = list(zip(angs, angs[1:]))
    if closed:
        pairs.append((angs[-1], angs[0] + 360.0))
    for a0, a1 in pairs:
        if a1 - a0 < 1e-6:
            continue
        for j in range(len(rings) - 1):
            oq(bm, [rings[j](a0), rings[j](a1), rings[j + 1](a1), rings[j + 1](a0)], mats[j], (0, 0, want_z))


# ---- walls ------------------------------------------------------------------------------------------------------------

class Opening:
    def __init__(self, u0, u1, z0, z1, kind="window"):
        self.u0, self.u1, self.z0, self.z1, self.kind = u0, u1, z0, z1, kind


def wall(p0, p1, t, opens, cell, reveal, glass=None, uo=None, ui=None, zs=(), us=(), pane=0.003):
    """A straight wall: its outer face along p0 -> p1 (the outside on the right), the inner face t behind it.
    uo / ui: the u-ranges of the outer / inner face (default the whole length); zs, us: extra cell breaks.
    cell(u, z, inner) -> (bm, mi) or None picks each cell's mesh and slot (None: no face there).
    reveal(opening, side) -> (bm, mi) or None for side 'sill', 'head', 'jamb'. Windows get a pane slab (2 faces pane*2
    apart, mid-wall) in glass. Returns (d, n, L)."""
    p0, p1 = Vector(p0), Vector(p1)
    L = (p1 - p0).length
    d = (p1 - p0) / L
    n = Vector((d.y, -d.x))
    uo = uo or (0.0, L)
    ui = ui or (0.0, L)

    def P(u, z, depth):
        q = p0 + d * u - n * depth
        return (q.x, q.y, z)

    zall = sorted(set(round(z, 6) for z in list(zs) + [o.z0 for o in opens] + [o.z1 for o in opens]))
    for inner in (False, True):
        lo, hi = ui if inner else uo
        ub = {lo, hi} | {min(max(u, lo), hi) for u in us}
        for o in opens:
            ub.update((min(max(o.u0, lo), hi), min(max(o.u1, lo), hi)))
        ub = sorted(ub)
        for a, b in zip(ub, ub[1:]):
            if b - a < 1e-4:
                continue
            for c, e in zip(zall, zall[1:]):
                if e - c < 1e-4:
                    continue
                um, zm = (a + b) / 2, (c + e) / 2
                if any(o.u0 < um < o.u1 and o.z0 < zm < o.z1 for o in opens):
                    continue
                r = cell(um, zm, inner)
                if r is None:
                    continue
                depth = t if inner else 0.0
                want = (-n.x, -n.y, 0) if inner else (n.x, n.y, 0)
                oq(r[0], [P(a, c, depth), P(b, c, depth), P(b, e, depth), P(a, e, depth)], r[1], want)
    for o in opens:
        r = reveal(o, "sill")
        if r:
            oq(r[0], [P(o.u0, o.z0, 0), P(o.u1, o.z0, 0), P(o.u1, o.z0, t), P(o.u0, o.z0, t)], r[1], (0, 0, 1))
        r = reveal(o, "head")
        if r:
            oq(r[0], [P(o.u0, o.z1, 0), P(o.u1, o.z1, 0), P(o.u1, o.z1, t), P(o.u0, o.z1, t)], r[1], (0, 0, -1))
        r = reveal(o, "jamb")
        if r:
            for u, s in ((o.u0, 1), (o.u1, -1)):
                oq(r[0], [P(u, o.z0, 0), P(u, o.z0, t), P(u, o.z1, t), P(u, o.z1, 0)], r[1], (d.x * s, d.y * s, 0))
        if o.kind == "window" and glass is not None:
            for off, s in ((t / 2 - pane, 1), (t / 2 + pane, -1)):
                oq(glass, [P(o.u0, o.z0, off), P(o.u1, o.z0, off), P(o.u1, o.z1, off), P(o.u0, o.z1, off)], M("RS_Glass"),
                   (n.x * s, n.y * s, 0))
    return d, n, L


# ---- export -----------------------------------------------------------------------------------------------------------

def _object(bm, name):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    for k in MATS:
        mat = bpy.data.materials.get(k) or bpy.data.materials.new(k)
        mat.diffuse_color = _lin(MAT_SPEC[k][0]) + (1,)
        me.materials.append(mat)
    me.update()
    metre_uv(me)
    used = sorted({p.material_index for p in me.polygons})
    keep = [me.materials[i] for i in used]
    remap = {old: new for new, old in enumerate(used)}
    idx = [remap[p.material_index] for p in me.polygons]
    me.materials.clear()
    for m in keep:
        me.materials.append(m)
    for p, i in zip(me.polygons, idx):
        p.material_index = i
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def export(bm, name, out=None, smooth=None):
    """Writes <out>/<name>.glb; frees bm; returns the triangle count."""
    out = out or OUT
    os.makedirs(out, exist_ok=True)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    o = _object(bm, name)
    bm.free()
    if smooth:
        names = set(smooth)
        for p in o.data.polygons:
            m = o.data.materials[p.material_index]
            p.use_smooth = bool(m and m.name in names)
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    D._write(o, os.path.join(out, name + ".glb"))
    me = o.data
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    print("REST", name, tris, "tris", flush=True)
    return tris
