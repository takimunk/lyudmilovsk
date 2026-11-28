"""The cruise's seascape kit (docs/ship.md): the islands the motor ship «Полдень» passes and the home coast seen from
the sea, all procedural (numpy + bpy, no other packages).

  python3 blender/ship/seascape_kit.py                       # the kit, the json, the detail texture (with bpy as a module)
  /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/ship/seascape_kit.py [-- ...]    # the same on the Mac
  python3 blender/ship/seascape_kit.py -- --preview /mnt/project-files/ship/     # + island_<id>_<500|2000>.jpg
  python3 blender/ship/seascape_kit.py -- --only birds --preview <dir>           # one piece (no export)

Out: design/ship/game/seascape_kit.glb (gitignored: every checkout makes its own, then unreal/scripts/import_cruise.sh),
     unreal/Lyudmilovsk/Content/Cruise/seascape.json (committed: meshes, sizes, sockets, where each stands on the chart),
     design/ship/textures/T_SS_rock_{C,N}.png (a tiling rock detail, 8 m a tile).

Frame: Blender metres, Z up from the sea's datum (high water of springs; the tide only falls, so every rock goes on
down to -7 m), X and Y the chart's east... no: X = the chart's X (west, out to sea), Y = MINUS the chart's Y (glTF into
Unreal mirrors Y: Unreal gets (x, -y, z) * 100, which is then the chart's frame again). Each piece's origin is its
chart point (seascape.json "islands").

Looks: rock is shaped as a signed distance field (beds of strata as ledges, joints as gullies, fractal roughness),
meshed with surface nets, decimated; low islands and the coast are height fields. The colour is in the VERTEX COLOURS
(strata, the black wet zone, orange and grey lichen, guano, turf) and multiplies a grey tiling detail on slot `ss_rock`
(metre UVs boxed by the face's main axis, UV = metres / 8); `ss_land` is vertex colour alone. Painted parts (the
lighthouse) have palette materials; `glow_*` slots are lit by the game at night.
"""
import json
import math
import os
import sys

import bpy
import bmesh
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_GLB = os.path.join(REPO, "design", "ship", "game", "seascape_kit.glb")
OUT_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Cruise", "seascape.json")
TEX_DIR = os.path.join(REPO, "design", "ship", "textures")
UV_TILE = 8.0          # metres a tile of the rock detail
SKIRT = -7.0           # every rock goes down to here (spring low water is -4.5 m, the winter's -5.2)
sys.path.insert(0, os.path.join(REPO, "tools"))

PALETTE = {             # name: colour (sRGB), roughness, emission
    "ss_rock": ((0.50, 0.48, 0.46), 0.92, 0.0),
    "ss_land": ((0.45, 0.45, 0.40), 0.95, 0.0),
    "ss_white": ((0.90, 0.89, 0.85), 0.7, 0.0),
    "ss_red": ((0.66, 0.11, 0.09), 0.7, 0.0),
    "ss_roof": ((0.20, 0.27, 0.24), 0.6, 0.0),
    "ss_roofred": ((0.52, 0.16, 0.11), 0.8, 0.0),
    "ss_dark": ((0.07, 0.07, 0.07), 0.8, 0.0),
    "ss_glass": ((0.10, 0.14, 0.16), 0.15, 0.0),
    "ss_timber": ((0.33, 0.27, 0.21), 0.9, 0.0),
    "ss_concrete": ((0.56, 0.55, 0.52), 0.9, 0.0),
    "ss_seal": ((0.32, 0.30, 0.28), 0.6, 0.0),
    "glow_lamp": ((1.0, 0.93, 0.72), 0.5, 6.0),
    "glow_window": ((1.0, 0.78, 0.45), 0.5, 3.0),
}
VC_SLOTS = ("ss_rock", "ss_land")


# ------------------------------------------------------------------------------------------------ noise
def _hash(ix, iy, iz, seed):
    h = ix.astype(np.int64) * 73856093 ^ iy.astype(np.int64) * 19349663 ^ iz.astype(np.int64) * 83492791 ^ (seed * 2654435761)
    h &= 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF) / float(0xFFFFFF)


def vnoise(x, y, z, seed=0):
    """Value noise 0..1, period 1 in each argument's unit."""
    x = np.asarray(x, np.float64); y = np.asarray(y, np.float64); z = np.asarray(z, np.float64)
    x, y, z = np.broadcast_arrays(x, y, z)
    ix = np.floor(x); iy = np.floor(y); iz = np.floor(z)
    fx = x - ix; fy = y - iy; fz = z - iz
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz)
    out = 0.0
    for dx in (0, 1):
        wx = fx if dx else 1 - fx
        for dy in (0, 1):
            wy = fy if dy else 1 - fy
            for dz in (0, 1):
                wz = fz if dz else 1 - fz
                out = out + _hash(ix + dx, iy + dy, iz + dz, seed) * wx * wy * wz
    return out


def fbm(x, y, z, scale, octaves=4, seed=0, gain=0.5):
    """Fractal noise about -1..1; scale = the first octave's feature size, m."""
    out = 0.0
    amp = 1.0
    tot = 0.0
    f = 1.0 / scale
    for o in range(octaves):
        out = out + amp * (vnoise(x * f, y * f, z * f, seed + o * 17) * 2 - 1)
        tot += amp
        amp *= gain
        f *= 2.03
    return out / tot


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def strata(z, x, y, thick, amp, seed, dip=0.05):
    """Beds of rock: a horizontal step in or out per bed (m), the beds dipping a little and wavering."""
    zz = z + dip * x + 0.02 * y + 1.5 * fbm(x, y, 0.0, 60.0, 2, seed + 5)
    b = np.floor(zz / thick)
    f = zz / thick - b
    h0 = _hash(b, b * 0 + 3, b * 0 + 7, seed)
    h1 = _hash(b + 1, b * 0 + 3, b * 0 + 7, seed)
    # flat within a bed, a short rounded step to the next one
    return amp * ((h0 + (h1 - h0) * smooth(0.8, 1.0, f)) - 0.5) * 2.0


def joints(x, y, scale, width, seed):
    """Vertical cracks: 1 in a joint, 0 away (lines where a plan noise crosses its middle)."""
    n = vnoise(x / scale, y / scale, 0.0, seed) + 0.35 * vnoise(x / scale * 2.7, y / scale * 2.7, 0.0, seed + 1) - 0.175
    return 1.0 - smooth(0.0, width, np.abs(n - 0.5))


# ------------------------------------------------------------------------------------------------ meshing
def surface_nets(F, origin, h):
    """The zero surface of a grid of distances (negative inside): one vertex per cell the surface crosses, quads between."""
    nx, ny, nz = F.shape
    inside = F < 0
    C = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)]
    cnt = np.zeros((nx - 1, ny - 1, nz - 1), np.int8)
    for c in C:
        cnt += inside[c[0]:nx - 1 + c[0], c[1]:ny - 1 + c[1], c[2]:nz - 1 + c[2]]
    active = (cnt > 0) & (cnt < 8)
    n = int(active.sum())
    idx = -np.ones(active.shape, np.int64)
    idx[active] = np.arange(n)
    ci, cj, ck = np.nonzero(active)
    acc = np.zeros((n, 3))
    w = np.zeros(n)
    for a in range(8):
        for b in range(a + 1, 8):
            if sum(abs(C[a][i] - C[b][i]) for i in range(3)) != 1:
                continue
            fa = F[ci + C[a][0], cj + C[a][1], ck + C[a][2]]
            fb = F[ci + C[b][0], cj + C[b][1], ck + C[b][2]]
            m = (fa < 0) != (fb < 0)
            den = np.where(m, fa - fb, 1.0)
            t = np.where(m, fa / den, 0.0)
            pa = np.array(C[a], float); pb = np.array(C[b], float)
            acc += (pa[None, :] + t[:, None] * (pb - pa)[None, :]) * m[:, None]
            w += m
    verts = (np.stack([ci, cj, ck], 1) + acc / w[:, None]) * h + np.asarray(origin, float)
    quads = []
    for axis in range(3):
        a1, a2 = (axis + 1) % 3, (axis + 2) % 3
        s0 = [slice(1, -1)] * 3; s1 = [slice(1, -1)] * 3
        s0[axis] = slice(0, -1); s1[axis] = slice(1, None)
        A = inside[tuple(s0)]; B = inside[tuple(s1)]
        m = A != B
        e = np.argwhere(m)
        g = e.copy()
        g[:, a1] += 1; g[:, a2] += 1

        def cell(d1, d2):
            c = g.copy()
            c[:, a1] += d1; c[:, a2] += d2
            return idx[c[:, 0], c[:, 1], c[:, 2]]
        q = np.stack([cell(-1, -1), cell(0, -1), cell(0, 0), cell(-1, 0)], 1)
        flip = ~A[m]
        q[flip] = q[flip][:, ::-1]
        quads.append(q)
    return verts, np.concatenate(quads)


def sdf_mesh(fn, lo, hi, h):
    """fn(x, y, z) -> distance; the box lo..hi sampled every h metres."""
    xs = np.arange(lo[0], hi[0] + h, h); ys = np.arange(lo[1], hi[1] + h, h); zs = np.arange(lo[2], hi[2] + h, h)
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")
    F = fn(X, Y, Z)
    # shut at the box (the bottom is cut open again afterwards)
    F[0, :, :] = F[-1, :, :] = F[:, 0, :] = F[:, -1, :] = F[:, :, -1] = 1.0
    F[:, :, 0] = 1.0
    return surface_nets(F, lo, h)


def height_mesh(xs, ys, H, floor=SKIRT):
    """A height field as triangles; cells wholly under the floor are left out."""
    nx, ny = len(xs), len(ys)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    Hc = np.maximum(H, floor)
    verts = np.stack([X.ravel(), Y.ravel(), Hc.ravel()], 1)
    i, j = np.meshgrid(np.arange(nx - 1), np.arange(ny - 1), indexing="ij")
    keep = (H[:-1, :-1] > floor) | (H[1:, :-1] > floor) | (H[:-1, 1:] > floor) | (H[1:, 1:] > floor)
    i = i[keep]; j = j[keep]
    a = i * ny + j; b = (i + 1) * ny + j; c = (i + 1) * ny + j + 1; d = i * ny + j + 1
    return verts, np.stack([a, b, c, d], 1)


class Piece:
    """A piece being put together: parts of (verts, faces, slot, colours or None)."""

    def __init__(self, name):
        self.name = name
        self.parts = []
        self.sockets = {}

    def add(self, verts, faces, slot, colour=None):
        self.parts.append((np.asarray(verts, float), [tuple(int(i) for i in f) for f in faces], slot, colour))

    # --- simple solids for the built things
    def box(self, lo, hi, slot, rot=0.0, at=(0, 0, 0)):
        x0, y0, z0 = lo; x1, y1, z1 = hi
        v = np.array([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], float)
        c, s = math.cos(rot), math.sin(rot)
        v = np.stack([v[:, 0] * c - v[:, 1] * s + at[0], v[:, 0] * s + v[:, 1] * c + at[1], v[:, 2] + at[2]], 1)
        self.add(v, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], slot)

    def lathe(self, at, profile, slots, n=20, cap=True):
        """Rings (radius, z) from the bottom up round the vertical through `at`; slots per segment."""
        for k in range(len(profile) - 1):
            (r0, z0), (r1, z1) = profile[k], profile[k + 1]
            v = []
            for i in range(n):
                a = 2 * math.pi * i / n
                v.append((at[0] + r0 * math.cos(a), at[1] + r0 * math.sin(a), at[2] + z0))
            for i in range(n):
                a = 2 * math.pi * i / n
                v.append((at[0] + r1 * math.cos(a), at[1] + r1 * math.sin(a), at[2] + z1))
            f = [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
            self.add(v, f, slots[k] if isinstance(slots, (list, tuple)) else slots)
        if cap:
            r, z = profile[-1]
            if r > 1e-6:
                v = [(at[0] + r * math.cos(2 * math.pi * i / n), at[1] + r * math.sin(2 * math.pi * i / n), at[2] + z) for i in range(n)]
                self.add(v, [tuple(range(n))], slots[-1] if isinstance(slots, (list, tuple)) else slots)

    def blob(self, at, radii, slot, rot=0.0, n=8, rings=5, colour=None, squash_under=None):
        """A low-poly ellipsoid (crowns, seals)."""
        v = []; f = []
        c, s = math.cos(rot), math.sin(rot)
        for j in range(rings + 1):
            t = math.pi * j / rings
            for i in range(n):
                a = 2 * math.pi * i / n
                x = radii[0] * math.sin(t) * math.cos(a); y = radii[1] * math.sin(t) * math.sin(a); z = -radii[2] * math.cos(t)
                if squash_under is not None and z < 0:
                    z *= squash_under
                v.append((at[0] + x * c - y * s, at[1] + x * s + y * c, at[2] + z))
        for j in range(rings):
            for i in range(n):
                a = j * n + i; b = j * n + (i + 1) % n
                f.append((a, b, b + n, a + n))
        self.add(v, f, slot, None if colour is None else np.tile(np.asarray(colour, float), (len(v), 1)))


def metre_uv(co, no):
    """Boxed metre UVs: by the face's main axis."""
    ax = np.argmax(np.abs(no), 1)
    u = np.where(ax == 0, co[:, 1], co[:, 0])
    v = np.where(ax == 2, co[:, 1], co[:, 2])
    return np.stack([u, v], 1) / UV_TILE


_MATS = {}


def material(name):
    if name in _MATS:
        return _MATS[name]
    col, rough, emit = PALETTE[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    lin = [c ** 2.2 for c in col]
    bsdf.inputs["Base Color"].default_value = (*lin, 1)
    bsdf.inputs["Roughness"].default_value = rough
    if emit > 0:
        bsdf.inputs["Emission Color"].default_value = (*lin, 1)
        bsdf.inputs["Emission Strength"].default_value = emit
    m["ss_preview"] = name in VC_SLOTS
    _MATS[name] = m
    return m


def preview_materials(on):
    """The look for the previews: vertex colour x the rock detail. Off for the export (the game's material does it)."""
    for name in VC_SLOTS:
        m = material(name)
        nt = m.node_tree
        bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        for l in list(bsdf.inputs["Base Color"].links):
            nt.links.remove(l)
        for l in list(bsdf.inputs["Normal"].links):
            nt.links.remove(l)
        if not on:
            continue
        vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "Col"
        if name == "ss_rock" and os.path.exists(os.path.join(TEX_DIR, "T_SS_rock_C.png")):
            tex = nt.nodes.new("ShaderNodeTexImage")
            tex.image = bpy.data.images.load(os.path.join(TEX_DIR, "T_SS_rock_C.png"), check_existing=True)
            mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.blend_type = "MULTIPLY"
            mix.inputs[0].default_value = 1.0
            gain = nt.nodes.new("ShaderNodeVectorMath"); gain.operation = "SCALE"; gain.inputs[3].default_value = 2.0
            nt.links.new(vc.outputs["Color"], mix.inputs[6]); nt.links.new(tex.outputs["Color"], mix.inputs[7])
            nt.links.new(mix.outputs[2], gain.inputs[0])
            nt.links.new(gain.outputs[0], bsdf.inputs["Base Color"])
        else:
            nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])


def to_object(piece, target_tris=None, smooth_angle=38.0, colour_fn=None, decimate_slots=VC_SLOTS):
    """Parts -> one object: decimated to the budget (the natural slots only), coloured, boxed UVs, sharp edges."""
    slots = []
    for _v, _f, s, _c in piece.parts:
        if s not in slots:
            slots.append(s)
    me = bpy.data.meshes.new(piece.name)
    bm = bmesh.new()
    col_layer = bm.loops.layers.color.new("Col")
    natural = []
    for v, f, s, c in piece.parts:
        bv = [bm.verts.new(p) for p in v]
        si = slots.index(s)
        for face in f:
            try:
                bf = bm.faces.new([bv[i] for i in face])
            except ValueError:
                continue
            bf.material_index = si
            if c is not None:
                for l in bf.loops:
                    l[col_layer] = (*c[face[list(bf.verts).index(l.vert)]][:3], 1.0) if False else (1, 1, 1, 1)
        if s in decimate_slots and c is None:
            natural.extend(bv)
    bm.verts.ensure_lookup_table()
    bmesh.ops.remove_doubles(bm, verts=[v for v in bm.verts], dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    for s in slots:
        me.materials.append(material(s))
    ob = bpy.data.objects.new(piece.name, me)
    bpy.context.scene.collection.objects.link(ob)
    n = len(me.polygons)
    if target_tris and n > target_tris:
        mod = ob.modifiers.new("dec", "DECIMATE")
        mod.ratio = target_tris / n
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        bpy.ops.object.modifier_apply(modifier=mod.name)
        ob.select_set(False)
        me = ob.data
    finish(ob, piece, smooth_angle, colour_fn)
    return ob


def finish(ob, piece, smooth_angle, colour_fn):
    """Colours (per vertex from colour_fn(P, N, slot) for the natural slots, the parts' own for blobs), UVs, shading."""
    me = ob.data
    nv = len(me.vertices)
    co = np.zeros(nv * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    vn = np.zeros(nv * 3); me.vertices.foreach_get("normal", vn); vn = vn.reshape(-1, 3)
    nl = len(me.loops)
    lv = np.zeros(nl, np.int64); me.loops.foreach_get("vertex_index", lv)
    npoly = len(me.polygons)
    pn = np.zeros(npoly * 3); me.polygons.foreach_get("normal", pn); pn = pn.reshape(-1, 3)
    pm = np.zeros(npoly, np.int64); me.polygons.foreach_get("material_index", pm)
    ls = np.zeros(npoly, np.int64); me.polygons.foreach_get("loop_start", ls)
    lt = np.zeros(npoly, np.int64); me.polygons.foreach_get("loop_total", lt)
    lp = np.repeat(np.arange(npoly), lt)           # loop -> polygon (loops are stored polygon by polygon)
    names = [m.name for m in me.materials]
    col = np.ones((nl, 4))
    for si, s in enumerate(names):
        if s not in VC_SLOTS:
            continue
        sel = pm[lp] == si
        if not sel.any():
            continue
        if colour_fn is not None:
            c = colour_fn(co[lv[sel]], 0.5 * vn[lv[sel]] + 0.5 * pn[lp[sel]], s)
            col[sel, :3] = np.clip(c, 0.0, 1.0)
    # a part's own colours (crowns, seals): nearest by position is not needed, they are told by a marker in piece.tints
    for (lo, hi, c) in getattr(piece, "tints", []):
        P = co[lv]
        sel = np.all((P >= np.asarray(lo)) & (P <= np.asarray(hi)), 1)
        col[sel, :3] = c
    if "Col" in me.color_attributes:
        me.color_attributes.remove(me.color_attributes["Col"])
    ca = me.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ca.data.foreach_set("color_srgb", col.ravel())
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = 0
    uv = me.uv_layers.new(name="UVMap") if not me.uv_layers else me.uv_layers[0]
    uvs = metre_uv(co[lv], pn[lp])
    uv.data.foreach_set("uv", uvs.ravel())
    me.polygons.foreach_set("use_smooth", np.ones(npoly, bool))
    try:
        me.set_sharp_from_angle(angle=math.radians(smooth_angle))
    except Exception:
        pass
    me.update()


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def far_copy(ob, ratio, min_tris=300):
    """The far level: the same piece decimated (seen beyond ~4 km: a silhouette and its big colours)."""
    o2 = ob.copy(); o2.data = ob.data.copy(); o2.name = ob.name + "_far"; o2.data.name = o2.name
    bpy.context.scene.collection.objects.link(o2)
    n = tris(o2)
    r = max(ratio, min(1.0, min_tris / max(n, 1)))
    mod = o2.modifiers.new("dec", "DECIMATE"); mod.ratio = r
    bpy.context.view_layer.objects.active = o2
    o2.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    o2.select_set(False)
    reuv(o2)
    return o2


def reuv(ob):
    """Boxed metre UVs again (a decimation blends them across the box's seams)."""
    me = ob.data
    nl = len(me.loops)
    lv = np.zeros(nl, np.int64); me.loops.foreach_get("vertex_index", lv)
    co = np.zeros(len(me.vertices) * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    npoly = len(me.polygons)
    pn = np.zeros(npoly * 3); me.polygons.foreach_get("normal", pn); pn = pn.reshape(-1, 3)
    lt = np.zeros(npoly, np.int64); me.polygons.foreach_get("loop_total", lt)
    me.uv_layers[0].data.foreach_set("uv", metre_uv(co[lv], pn[np.repeat(np.arange(npoly), lt)]).ravel())
    me.update()


# ------------------------------------------------------------------------------------------------ colours
ROCK = np.array([0.47, 0.44, 0.42])       # grey gneiss with a warm cast
ROCK_RED = np.array([0.52, 0.40, 0.35])   # rapakivi-like red granite beds
WET = np.array([0.075, 0.075, 0.07])      # the black zone (Verrucaria) the swell keeps wet
WEED = np.array([0.13, 0.14, 0.07])       # wrack under the low water mark
ORANGE = np.array([0.66, 0.47, 0.13])     # Xanthoria on the splash zone and the bird perches
GREY_LICHEN = np.array([0.60, 0.61, 0.56])
GUANO = np.array([0.88, 0.87, 0.82])
TURF = np.array([0.30, 0.37, 0.17])
TURF_DRY = np.array([0.46, 0.43, 0.24])
SAND = np.array([0.70, 0.63, 0.47])


def rock_colour(P, N, seed=0, beds=4.5, red=0.35, guano=0.0, guano_from=10.0, turf_from=1e9, turf_slope=0.72, wet_top=1.6):
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    nz = N[:, 2]
    # the beds: every one its own tone, some of them red
    zz = z + 0.05 * x + 0.02 * y + 1.5 * fbm(x, y, 0.0, 60.0, 2, seed + 5)
    b = np.floor(zz / beds)
    tone = _hash(b, b * 0 + 1, b * 0 + 2, seed + 9)
    redk = (_hash(b, b * 0 + 4, b * 0 + 2, seed + 11) < red).astype(float)
    c = ROCK[None, :] * (1 - redk[:, None]) + ROCK_RED[None, :] * redk[:, None]
    c = c * (0.80 + 0.36 * tone)[:, None]
    # weathering: big soft blotches and a fine grain, darker in the hollows that face down
    c = c * (0.86 + 0.22 * fbm(x, y, z, 14.0, 3, seed + 20))[:, None]
    c = c * (0.94 + 0.10 * fbm(x, y, z, 2.2, 2, seed + 21))[:, None]
    c = c * (0.78 + 0.22 * smooth(-0.6, 0.3, nz))[:, None]
    # rain streaks down the faces: dark stripes that hold their place in plan
    steep = 1 - smooth(0.25, 0.6, np.abs(nz))
    streak = smooth(0.55, 0.8, vnoise(x * 0.55, y * 0.55, z * 0.03, seed + 30))
    c = c * (1 - 0.30 * streak * steep)[:, None]
    # grey lichen over the dry upper rock, orange on the splash zone and where birds sit
    gl = smooth(0.50, 0.75, vnoise(x * 0.12, y * 0.12, z * 0.12, seed + 40)) * smooth(4.0, 9.0, z)
    c = c * (1 - 0.45 * gl)[:, None] + GREY_LICHEN[None, :] * (0.45 * gl)[:, None]
    orange = smooth(0.55, 0.72, vnoise(x * 0.22, y * 0.22, z * 0.30, seed + 41)) * smooth(1.8, 3.2, z) * (1 - smooth(7.0, 12.0, z)) * smooth(-0.2, 0.5, nz)
    c = c * (1 - 0.8 * orange)[:, None] + ORANGE[None, :] * (0.8 * orange)[:, None]
    if guano > 0:
        # whitewash: on the ledges' tops and in long streaks under them
        ledge = smooth(0.45, 0.8, nz) * smooth(guano_from - 4, guano_from + 4, z)
        drip = smooth(0.50, 0.72, vnoise(x * 0.8, y * 0.8, z * 0.045, seed + 50)) * steep * smooth(guano_from, guano_from + 12, z)
        patch = smooth(0.35, 0.65, vnoise(x * 0.03, y * 0.03, z * 0.05, seed + 51))
        g = np.clip(guano * (0.85 * ledge + 0.75 * drip) * (0.35 + 0.65 * patch), 0, 0.92)
        c = c * (1 - g)[:, None] + GUANO[None, :] * g[:, None]
        orange2 = guano * 0.5 * smooth(0.6, 0.8, vnoise(x * 0.3, y * 0.3, z * 0.3, seed + 52)) * smooth(guano_from, guano_from + 6, z) * smooth(0.2, 0.7, nz)
        c = c * (1 - orange2)[:, None] + ORANGE[None, :] * orange2[:, None]
    # turf on the level tops
    t = smooth(turf_slope, turf_slope + 0.14, nz) * smooth(turf_from, turf_from + 2.5, z + 2.0 * fbm(x, y, 0.0, 9.0, 2, seed + 60))
    tc = TURF[None, :] * (1 - 0.0) + (TURF_DRY - TURF)[None, :] * smooth(0.4, 0.8, vnoise(x * 0.07, y * 0.07, 0.0, seed + 61))[:, None]
    tc = tc * (0.85 + 0.3 * vnoise(x * 0.4, y * 0.4, 0.0, seed + 62))[:, None]
    c = c * (1 - t)[:, None] + tc * t[:, None]
    # the wet black zone and the weed under it
    wz = z + 0.5 * fbm(x, y, 0.0, 6.0, 2, seed + 70)
    wet = 1 - smooth(wet_top - 1.0, wet_top + 0.5, wz)
    c = c * (1 - wet)[:, None] + WET[None, :] * wet[:, None]
    weed = 1 - smooth(-2.6, -1.2, wz)
    c = c * (1 - weed)[:, None] + WEED[None, :] * weed[:, None]
    # a pale line of barnacles and salt just over the black
    salt = np.exp(-((wz - (wet_top + 0.9)) / 0.5) ** 2) * 0.25
    c = c + salt[:, None] * (1 - c)
    return c


# ------------------------------------------------------------------------------------------------ the islands
def stack_sdf(x, y, z, cx, cy, top, r0, r1, lean, seed, bed=3.6):
    """One sea stack: a column of jointed rock, many-sided (it breaks off along its joints), tapering and leaning,
    bedded, with a hard cap bed that overhangs and a notch the sea cuts at its foot."""
    t = np.clip(z / top, -0.3, 1.2)
    px = x - cx - lean[0] * t * top; py = y - cy - lean[1] * t * top
    rng = np.random.default_rng(seed)
    nface = int(rng.integers(5, 8))
    base = rng.uniform(0, 2 * math.pi)
    taper = (r0 + (r1 - r0) * smooth(0.0, 1.0, t) ** 0.7)
    d = np.full(np.shape(px), -1e9)
    for k in range(nface):
        a = base + 2 * math.pi * (k + rng.uniform(-0.3, 0.3)) / nface
        rk = rng.uniform(0.78, 1.08)
        # every face has its own batter, and steps back where a slab has fallen away
        fall = rng.uniform(0.0, 0.22) * smooth(rng.uniform(0.25, 0.7), 1.0, t) * (1 if rng.random() < 0.6 else 0)
        dk = px * math.cos(a) + py * math.sin(a) - taper * (rk - fall)
        d = np.maximum(d, dk)
    d = d - 3.2 * smooth(4.0, -5.0, z)                               # the foot spreads under water
    d = d + 1.5 * np.exp(-((z - 1.4) / 1.2) ** 2)                     # the notch at the waterline
    d = d - 0.9 * smooth(top - 6.0, top - 3.5, z)                     # the cap bed stands proud
    d = d - strata(z, x, y, bed, 0.55, seed, dip=0.07) - 0.25 * strata(z, x, y, bed * 0.31, 0.5, seed + 7, dip=0.07)
    d = d - 1.1 * fbm(x, y, z, 7.0, 4, seed + 1) - 1.6 * fbm(x, y, z, 21.0, 2, seed + 4)
    d = d + 1.8 * joints(x, y, 9.0, 0.03, seed + 2) * smooth(-2.0, 4.0, z)
    capz = z - top - 1.0 * fbm(x, y, 0.0, 4.0, 3, seed + 3) - 0.06 * px
    return np.maximum(d, capz)


def skerry_height(X, Y, blobs, seed, rough=0.5):
    """A low wave-washed platform: smooth whalebacks (cx, cy, rx, ry, h) summed softly, striated."""
    H = np.full(X.shape, -9.0)
    for (cx, cy, rx, ry, h, rot) in blobs:
        c, s = math.cos(rot), math.sin(rot)
        u = ((X - cx) * c + (Y - cy) * s) / rx; v = (-(X - cx) * s + (Y - cy) * c) / ry
        q = u * u + v * v
        # steeper on the lee end, like ice-smoothed rock
        hh = (h + 9.0) * np.clip(1 - q, 0, None) ** 0.75 * (1 + 0.25 * np.clip(u, -1, 1)) - 9.0
        H = np.maximum(H, hh)
    live = smooth(-5, 0.5, H)
    H = H + rough * (1.6 * fbm(X, Y, 0.0, 13.0, 4, seed) + 0.9 * np.abs(fbm(X, Y, 0.0, 5.0, 3, seed + 3))) * live + 0.2 * fbm(X, Y, 0.0, 1.8, 2, seed + 1)
    # joints: clefts across, two sets, and blocks plucked out on the lee side
    H = H - (1.5 * joints(X, Y, 17.0, 0.035, seed + 2) + 0.8 * joints(X + 0.7 * Y, Y - 0.7 * X, 9.0, 0.04, seed + 6)) * live
    H = H - 1.1 * smooth(0.62, 0.75, vnoise(X / 11.0, Y / 7.0, 0.0, seed + 8)) * live
    return H


def brothers():
    """The Stone Brothers: three stacks, 38, 31 and 25 m, on one wave-cut reef."""
    p = Piece("SS_brothers")
    S = [(-34.0, 6.0, 38.0, 12.5, 7.5, (0.05, -0.03), 3), (8.0, -10.0, 31.0, 11.0, 6.5, (-0.04, 0.06), 11), (44.0, 9.0, 25.0, 9.5, 6.0, (0.08, 0.02), 23)]

    def fn(x, y, z):
        d = None
        for (cx, cy, top, r0, r1, lean, seed) in S:
            s = stack_sdf(x, y, z, cx, cy, top, r0, r1, lean, seed)
            d = s if d is None else np.minimum(d, s)
        return d
    v, q = sdf_mesh(fn, (-62, -36, SKIRT - 1.5), (66, 36, 44), 1.0)
    p.add(v, q, "ss_rock")
    xs = np.arange(-80, 84, 2.0); ys = np.arange(-46, 48, 2.0)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    H = skerry_height(X, Y, [(-30, 4, 34, 22, 1.6, 0.1), (10, -8, 30, 20, 1.1, -0.2), (44, 8, 26, 18, 1.3, 0.3), (-62, -14, 12, 8, 0.6, 0.6), (70, -8, 9, 6, 2.2, 0.0)], 5)
    v, q = height_mesh(xs, ys, H)
    p.add(v, q, "ss_rock")
    col = lambda P, N, s: rock_colour(P, N, seed=3, beds=3.6, red=0.5, guano=0.55, guano_from=16.0, turf_from=29.0)
    ob = to_object(p, 26000, colour_fn=col)
    return ob, p, col


def arch():
    """The Needle's Eye: a blade of rock 34 m high with a tall pointed hole through it, a small stack beside it."""
    p = Piece("SS_arch")

    def fn(x, y, z):
        # the blade: 78 m long (X), 15 m thick, highest over the eye
        top = 34.0 - 11.0 * smooth(8.0, 38.0, np.abs(x - 4.0)) + 2.0 * fbm(x, y, 0.0, 14.0, 2, 31)
        half = 7.5 + 3.0 * smooth(10.0, -6.0, z) - 2.2 * smooth(12.0, 32.0, z) + 1.5 * np.sin(x * 0.11)
        yc = 3.0 * np.sin(x * 0.045)
        ends = np.abs(x + 2.0) - (39.0 - 4.0 * smooth(0.0, 30.0, z))
        d = np.maximum(np.abs(y - yc) - half, ends)
        d = d - strata(z, x, y, 3.2, 1.1, 37, dip=0.12) - 1.3 * fbm(x, y, z, 8.0, 4, 38)
        d = d + 1.4 * joints(x, y, 12.0, 0.035, 39)
        d = np.maximum(d, z - top)
        # the eye: 13 m wide at the water, pointed at 23 m, leaning a little
        ex = x - 4.0 - 0.10 * z
        w = 6.5 * np.sqrt(np.clip(1 - (np.clip(z, 0, None) / 23.0) ** 1.7, 0, None)) + 1.2 * fbm(x, y, z, 6.0, 2, 40)
        eye = np.maximum(np.abs(ex) - w, z - 23.0)
        d = np.maximum(d, -eye)
        # the fallen stack to the east of the blade
        s = stack_sdf(x, y, z, -58.0, -7.0, 15.0, 7.5, 4.5, (0.1, 0.05), 41, bed=3.2)
        return np.minimum(d, s)
    v, q = sdf_mesh(fn, (-72, -26, SKIRT - 1.5), (46, 26, 40), 1.0)
    p.add(v, q, "ss_rock")
    xs = np.arange(-84, 60, 2.0); ys = np.arange(-34, 36, 2.0)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    H = skerry_height(X, Y, [(-22, 0, 22, 17, 1.2, 0.0), (28, 2, 20, 16, 1.4, 0.2), (-58, -6, 16, 12, 0.9, 0.3), (50, -12, 8, 6, 1.8, 0.0)], 43)
    # the sill under the eye stays under water: the swell runs through
    H = H - 4.0 * np.exp(-((X - 4.0) / 9.0) ** 2)
    v, q = height_mesh(xs, ys, H)
    p.add(v, q, "ss_rock")
    p.sockets["eye"] = [4.0 + 1.0, 0.0, 10.0]
    p.sockets["eye_axis_deg"] = 90.0          # one looks through it along the piece's Y
    col = lambda P, N, s: rock_colour(P, N, seed=37, beds=3.2, red=0.3, guano=0.35, guano_from=18.0, turf_from=30.0)
    ob = to_object(p, 26000, colour_fn=col)
    return ob, p, col


def birds():
    """Bird Island: 600 m long, a 70 m bedded cliff to the west (-X is the chart's east: see the frame) with ledges and
    whitewash, the top sloping east under turf, a boulder beach at the east foot."""
    p = Piece("SS_birds")

    def plan(x, y):
        # the outline: a long bean, a bight in the east side, two buttresses to the west
        u = x / 96.0; v = y / 302.0
        r = np.sqrt(u * u + v * v)
        ang = np.arctan2(v, u)
        edge = 1 + 0.10 * np.sin(ang * 3 + 0.6) + 0.07 * np.sin(ang * 5 + 2.0) + 0.05 * np.sin(ang * 9 + 1.0) + 0.10 * fbm(x, y, 0.0, 70.0, 3, 61)
        return (r - edge) * 96.0 * (0.9 + 0.3 * np.abs(np.sin(ang)))        # about metres outside the outline

    def top_of(x, y):
        return 70.0 - 26.0 * smooth(-60.0, 95.0, -x) - 9.0 * smooth(150.0, 300.0, np.abs(y)) + 4.0 * fbm(x, y, 0.0, 90.0, 3, 62)

    def fn(x, y, z):
        d = plan(x, y)
        west = smooth(-20.0, 40.0, x)                # 1 on the cliff side, 0 on the landward (east) side
        # the east side lies back at ~40 degrees with terraces; the west stands
        d = d + (1 - west) * np.clip(z, 0, None) * 1.15 + west * np.clip(z, 0, None) * 0.10
        d = d - 3.0 * smooth(3.0, -6.0, z) + 1.6 * np.exp(-((z - 1.5) / 1.5) ** 2) * west
        d = d - strata(z, x, y, 5.2, 3.4, 63, dip=0.04) * (0.45 + 0.55 * west) - strata(z, x, y, 1.7, 0.7, 64, dip=0.04)
        d = d - 2.6 * fbm(x, y, z, 22.0, 4, 65)
        d = d + 5.0 * joints(x, y, 46.0, 0.03, 66) * smooth(-2, 6, z) + 1.6 * joints(x, y, 15.0, 0.03, 67)
        return np.maximum(d, (z - top_of(x, y)) * 0.8)
    v, q = sdf_mesh(fn, (-130, -330, SKIRT - 2.0), (124, 330, 78), 2.0)
    p.add(v, q, "ss_rock")

    def col(P, N, s):
        c = rock_colour(P, N, seed=63, beds=5.2, red=0.25, guano=1.6, guano_from=7.0, turf_from=1e9) * 1.12
        # turf over the whole top and the east slope's terraces
        x, y, z = P[:, 0], P[:, 1], P[:, 2]
        t = smooth(0.62, 0.8, N[:, 2]) * smooth(-6.0, -1.0, z - top_of(x, y) + 5.0 * (1 - smooth(-20, 40, x)) * 8.0)
        t = t * smooth(8.0, 14.0, z)
        tc = TURF[None, :] + (TURF_DRY - TURF)[None, :] * smooth(0.35, 0.75, vnoise(x * 0.05, y * 0.05, 0.0, 71))[:, None]
        tc = tc * (0.8 + 0.4 * vnoise(x * 0.3, y * 0.3, 0.0, 72))[:, None]
        return c * (1 - t)[:, None] + tc * t[:, None]
    p.sockets["colony"] = [60.0, 0.0, 45.0]        # the middle of the west cliff's face (birds wheel in front of it)
    p.sockets["colony_half_length"] = 240.0
    ob = to_object(p, 64000, colour_fn=col)
    return ob, p, col


def lighthouse():
    """The Far Light: a bare skerry 150 m long, a banded tower 28 m to the lamp, the keeper's hut, a jetty."""
    p = Piece("SS_lighthouse")
    xs = np.arange(-110, 112, 2.0); ys = np.arange(-70, 72, 2.0)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    blobs = [(0, 0, 70, 38, 7.5, 0.15), (-48, 10, 38, 26, 4.5, -0.3), (46, -8, 36, 22, 5.0, 0.4), (84, 18, 14, 10, 1.6, 0.0), (-88, -22, 12, 9, 1.2, 0.5),
             (20, 44, 16, 9, 1.0, 0.2)]
    H = skerry_height(X, Y, blobs, 81, rough=0.7)
    # a level seat for the tower and the hut
    seat = np.exp(-(((X - 6) / 9.0) ** 2 + ((Y - 2) / 9.0) ** 2))
    H = H * (1 - seat) + 7.4 * seat
    seat2 = np.exp(-(((X + 22) / 7.0) ** 2 + ((Y - 6) / 6.0) ** 2))
    H = H * (1 - seat2) + 5.6 * seat2
    v, q = height_mesh(xs, ys, H)
    p.add(v, q, "ss_rock")
    T = (6.0, 2.0, 7.2)
    # the tower: five bands, a gallery, the lantern, a dome
    bands = [(3.7, 0.0), (3.5, 4.4), (3.3, 8.8), (3.1, 13.2), (2.9, 17.6), (2.7, 22.0)]
    p.lathe(T, [(4.3, -1.0), (4.3, 0.0)], "ss_concrete", 20, cap=True)
    p.lathe(T, bands, ["ss_red", "ss_white", "ss_red", "ss_white", "ss_red"], 20, cap=False)
    p.lathe(T, [(2.7, 22.0), (3.7, 22.5), (3.7, 22.75)], ["ss_dark", "ss_dark"], 20, cap=True)       # the gallery
    p.lathe(T, [(3.62, 22.75), (3.62, 23.85), (3.56, 23.85)], ["ss_dark", "ss_dark"], 20, cap=False)   # its rail, as a band
    p.lathe(T, [(2.0, 22.75), (2.0, 23.6)], "ss_white", 16, cap=False)                               # the lantern's wall
    p.lathe(T, [(1.95, 23.6), (1.95, 26.3)], "ss_glass", 16, cap=False)                              # the glazing
    p.lathe(T, [(2.25, 26.3), (1.6, 27.3), (0.5, 28.0), (0.12, 28.2), (0.12, 29.4), (0.0, 29.5)], ["ss_roof"] * 5, 16, cap=False)
    p.lathe(T, [(0.55, 24.3), (0.75, 25.0), (0.55, 25.7)], ["glow_lamp", "glow_lamp"], 10, cap=True)   # the lens
    p.lathe(T, [(0.55, 24.29), (0.0, 24.29)], "glow_lamp", 10, cap=False)
    p.box((-0.6, -0.08, 0.0), (0.6, 0.08, 2.1), "ss_dark", rot=math.radians(200), at=(T[0] + 3.72 * math.cos(math.radians(200 + 90)) * 0 - 3.66, T[1] - 0.6, T[2]))
    for k, zz in enumerate((6.0, 11.0, 16.0, 20.0)):
        a = math.radians(40 + k * 95)
        r = 3.72 - 0.045 * zz
        p.box((-0.25, -0.05, 0.0), (0.25, 0.05, 0.8), "ss_dark", rot=a + math.pi / 2, at=(T[0] + r * math.cos(a), T[1] + r * math.sin(a), T[2] + zz))
    p.sockets["lamp"] = [T[0], T[1], T[2] + 25.0]
    p.sockets["tower_top"] = [T[0], T[1], T[2] + 29.5]
    # the keeper's hut: white walls, a red roof, a chimney, lit windows
    Hh = (-22.0, 6.0, 5.4)
    rot = math.radians(12)
    p.box((-4.0, -2.6, -0.6), (4.0, 2.6, 2.7), "ss_white", rot, Hh)
    c, s = math.cos(rot), math.sin(rot)

    def hut(px, py, pz):
        return (Hh[0] + px * c - py * s, Hh[1] + px * s + py * c, Hh[2] + pz)
    roof = [hut(-4.3, -2.9, 2.7), hut(4.3, -2.9, 2.7), hut(4.3, 2.9, 2.7), hut(-4.3, 2.9, 2.7), hut(-4.3, 0, 4.5), hut(4.3, 0, 4.5)]
    p.add(roof, [(0, 1, 5, 4), (2, 3, 4, 5), (0, 4, 3), (1, 2, 5), (0, 3, 2, 1)], "ss_roofred")
    p.box((1.6, -0.4, 3.4), (2.3, 0.3, 5.1), "ss_concrete", rot, Hh)
    for wx in (-2.4, 0.2, 2.6):
        p.box((wx - 0.45, -2.66, 0.9), (wx + 0.45, -2.6, 1.9), "glow_window", rot, Hh)
    p.box((-0.5, 2.6, -0.2), (0.5, 2.66, 1.9), "ss_timber", rot, Hh)
    p.sockets["hut"] = [Hh[0], Hh[1], Hh[2] + 1.4]
    # an oil store and a mast with a storm cone
    p.box((-2.0, -1.5, -0.5), (2.0, 1.5, 2.2), "ss_concrete", math.radians(-20), (30.0, -6.0, 4.6))
    p.lathe((-34.0, 14.0, 4.6), [(0.09, -0.5), (0.06, 9.0)], "ss_white", 6)
    # the jetty: a concrete deck on piles out to the deep water on the lee side, steps up the rock
    J0 = np.array([14.0, 30.0]); J1 = np.array([24.0, 66.0])
    dj = (J1 - J0) / np.linalg.norm(J1 - J0); jr = math.atan2(dj[1], dj[0]); jl = float(np.linalg.norm(J1 - J0))
    p.box((0.0, -1.6, 1.9), (jl, 1.6, 2.3), "ss_concrete", jr, (J0[0], J0[1], 0.0))
    for k in range(7):
        for side in (-1.2, 1.2):
            p.box((k * jl / 6.3 - 0.2, side - 0.2, SKIRT), (k * jl / 6.3 + 0.2, side + 0.2, 1.9), "ss_timber", jr, (J0[0], J0[1], 0.0))
    for k in range(9):
        p.box((jl * 0.1 + k * jl * 0.1, 1.5, 2.3), (jl * 0.1 + k * jl * 0.1 + 0.08, 1.58, 3.3), "ss_dark", jr, (J0[0], J0[1], 0.0))
    p.box((jl * 0.1, 1.5, 3.25), (jl * 0.9 + 0.08, 1.58, 3.33), "ss_dark", jr, (J0[0], J0[1], 0.0))
    for k in range(12):                       # the steps from the jetty's root up to the tower
        f = k / 11.0
        px = J0[0] + (T[0] + 3.0 - J0[0]) * f; py = J0[1] + (T[1] + 6.0 - J0[1]) * f
        pz = 2.0 + (6.9 - 2.0) * f
        p.box((-0.9, -0.5, -0.9), (0.9, 0.5, 0.0), "ss_concrete", jr, (px, py, pz))
    p.sockets["jetty_head"] = [float(J1[0]), float(J1[1]), 2.3]
    col = lambda P, N, s: rock_colour(P, N, seed=81, beds=6.0, red=0.6, guano=0.25, guano_from=3.5, turf_from=1e9, wet_top=1.4)
    ob = to_object(p, 15000, colour_fn=col)
    return ob, p, col


def long_island():
    """Long Island: 2.5 km of low rock and sand under wind-bent pines; a sandy bay open to the south-east where the ship
    anchors for the night (socket "anchorage": the chart point; the island lies round it)."""
    p = Piece("SS_long")
    xs = np.arange(-1300, 1301, 20.0); ys = np.arange(-330, 331, 10.0)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    # the spine wanders; the bay is bitten out of the +Y side (Blender +Y = the chart's south)
    spine = 40.0 * np.sin(X / 420.0) + 25.0 * np.sin(X / 170.0 + 1.0)
    halfw = 165.0 * np.sqrt(np.clip(1 - (X / 1260.0) ** 2, 0, None)) * (0.75 + 0.25 * np.sin(X / 260.0 + 0.5)) + 1e-3
    d = (np.abs(Y - spine) / halfw)
    H = 15.0 * np.clip(1 - d ** 2.2, -1.0, None)
    H = np.where(d > 1, -3.0 - (d - 1) * halfw * 0.08, H)
    H = H + 2.2 * fbm(X, Y, 0.0, 120.0, 4, 91) * smooth(-2, 3, H) + 0.5 * fbm(X, Y, 0.0, 25.0, 3, 92)
    bay = np.exp(-(((X - 180.0) / 330.0) ** 2 + ((Y - 215.0) / 150.0) ** 2))
    H = H - 13.0 * bay
    # the beach round the bay: a smooth sand ramp
    sandk = smooth(0.12, 0.45, bay) * (1 - smooth(0.78, 0.95, bay))
    H = np.where(sandk > 0.3, np.minimum(H, 0.4 + 2.4 * (1 - smooth(0.3, 0.8, bay))), H)
    # rocky points at both ends and the bay's horns
    for (cx, cy, r, h) in [(-1210, 0, 60, 6.0), (1190, 30, 70, 7.0), (-150, 150, 45, 5.0), (520, 150, 50, 5.5), (880, -120, 40, 4.0)]:
        H = np.maximum(H, h * (1 - ((X - cx) ** 2 + (Y - cy) ** 2) / r ** 2) + 0.8 * fbm(X, Y, 0.0, 12.0, 3, 93))
    v, q = height_mesh(xs, ys, H)
    p.add(v, q, "ss_land")

    def hgt(x, y):
        i = np.clip(((np.asarray(x) - xs[0]) / 20.0).round().astype(int), 0, len(xs) - 1)
        j = np.clip(((np.asarray(y) - ys[0]) / 10.0).round().astype(int), 0, len(ys) - 1)
        return H[i, j]

    def bayk(x, y):
        return np.exp(-(((x - 180.0) / 330.0) ** 2 + ((y - 215.0) / 150.0) ** 2))

    def col(P, N, s):
        x, y, z = P[:, 0], P[:, 1], P[:, 2]
        rock = rock_colour(P, N, seed=91, beds=5.0, red=0.5, wet_top=1.2)
        heath = np.array([0.24, 0.29, 0.15])[None, :] * (0.8 + 0.4 * vnoise(x * 0.02, y * 0.02, 0.0, 95))[:, None]
        k = smooth(2.5, 4.5, z) * smooth(0.6, 0.85, N[:, 2])
        c = rock * (1 - k)[:, None] + heath * k[:, None]
        sk = smooth(0.10, 0.35, bayk(x, y)) * smooth(-2.5, -0.5, z) * (1 - smooth(3.2, 4.5, z))
        sand = SAND[None, :] * (0.9 + 0.15 * vnoise(x * 0.05, y * 0.05, 0.0, 96))[:, None] * (1 - 0.35 * (1 - smooth(-0.3, 0.6, z)))[:, None]
        return c * (1 - sk)[:, None] + sand * sk[:, None]
    # the pines: crowns pushed over by the west wind (the chart's west is +X here), a lean trunk, in belts on the spine
    rng = np.random.default_rng(97)
    p.tints = []
    crown_cols = [np.array([0.11, 0.19, 0.10]), np.array([0.14, 0.22, 0.11]), np.array([0.09, 0.16, 0.10])]
    trunk_col = np.array([0.36, 0.24, 0.17])
    crowns = Piece("crowns"); trunks = Piece("trunks")
    made = 0
    for _ in range(9000):
        if made >= 620:
            break
        x = rng.uniform(-1230, 1230); y = rng.uniform(-300, 300)
        h0 = float(hgt(x, y))
        dens = float(vnoise(x / 140.0, y / 140.0, 0.0, 98))
        if h0 < 4.5 or dens < 0.33 or float(bayk(x, y)) > 0.3:
            continue
        made += 1
        ht = rng.uniform(10.0, 17.0) * (0.75 + 0.5 * dens)
        lean = rng.uniform(0.12, 0.3) * ht          # the top stands this far downwind (-X: towards the chart's east)
        w = rng.uniform(3.6, 5.6)
        a = rng.uniform(0, math.pi)
        tv = [(x + 0.42 * math.cos(a + k * 2.1), y + 0.42 * math.sin(a + k * 2.1), h0 - 0.4) for k in range(3)]
        tv += [(x - lean * 0.8 + 0.2 * math.cos(a + k * 2.1), y + 0.2 * math.sin(a + k * 2.1), h0 + ht * 0.8) for k in range(3)]
        trunks.add(tv, [(0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)], "ss_land")
        cc = crown_cols[rng.integers(0, 3)] * rng.uniform(0.85, 1.15)
        crowns.blob((x - lean * 0.75, y, h0 + ht * 0.82), (w * 1.25, w, ht * 0.20), "ss_land", n=5, rings=2, colour=cc, squash_under=0.45)
        crowns.blob((x - lean * 1.15 - w * 0.5, y + rng.uniform(-0.8, 0.8), h0 + ht * 0.66), (w * 0.9, w * 0.7, ht * 0.14), "ss_land", n=5, rings=2, colour=cc * 0.9, squash_under=0.45)
    ob = to_object(p, 30000, colour_fn=col)
    # the trees are their own objects first (their colours are their own), then joined
    oc = to_object_tinted(crowns, lambda P: None)
    ot = to_object_flat(trunks, trunk_col)
    ob = join([ob, oc, ot], "SS_long")
    p.sockets["anchorage"] = [180.0, 300.0, 0.0]
    p.sockets["beach"] = [180.0, 120.0, 1.0]
    return ob, p, col


def to_object_flat(piece, colour):
    ob = to_object(piece, None, smooth_angle=60.0, colour_fn=lambda P, N, s: np.tile(colour, (len(P), 1)))
    return ob


def to_object_tinted(piece, _unused):
    """Blobs with a colour each: built part by part so every blob keeps its own tint."""
    obs = []
    slots = ["ss_land"]
    me = bpy.data.meshes.new(piece.name)
    bm = bmesh.new()
    cl = bm.loops.layers.color.new("ColTmp")
    for v, f, s, c in piece.parts:
        bv = [bm.verts.new(q) for q in v]
        for face in f:
            try:
                bf = bm.faces.new([bv[i] for i in face])
            except ValueError:
                continue
            for l in bf.loops:
                l[cl] = (*[float(t) for t in c[0][:3]], 1.0)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-4)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    me.materials.append(material(slots[0]))
    ob = bpy.data.objects.new(piece.name, me)
    bpy.context.scene.collection.objects.link(ob)
    # the temporary float colours (linear) -> the kit's byte layer "Col" (given as sRGB values)
    nl = len(me.loops)
    tmp = np.zeros(nl * 4); me.color_attributes["ColTmp"].data.foreach_get("color", tmp)
    tmp = tmp.reshape(-1, 4)
    # shade the undersides: darker below, lighter on top
    lv = np.zeros(nl, np.int64); me.loops.foreach_get("vertex_index", lv)
    vn = np.zeros(len(me.vertices) * 3); me.vertices.foreach_get("normal", vn); vn = vn.reshape(-1, 3)
    tmp[:, :3] *= (0.62 + 0.5 * np.clip(vn[lv, 2], -0.5, 1.0))[:, None]
    me.color_attributes.remove(me.color_attributes["ColTmp"])
    ca = me.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ca.data.foreach_set("color_srgb", np.clip(tmp, 0, 1).ravel())
    co = np.zeros(len(me.vertices) * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    npoly = len(me.polygons)
    pn = np.zeros(npoly * 3); me.polygons.foreach_get("normal", pn); pn = pn.reshape(-1, 3)
    lt = np.zeros(npoly, np.int64); me.polygons.foreach_get("loop_total", lt)
    lp = np.repeat(np.arange(npoly), lt)
    uv = me.uv_layers.new(name="UVMap")
    uv.data.foreach_set("uv", metre_uv(co[lv], pn[lp]).ravel())
    me.polygons.foreach_set("use_smooth", np.ones(npoly, bool))
    me.update()
    return ob


def join(obs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name; ob.data.name = name
    ob.select_set(False)
    return ob


def seals():
    """The Seal Rocks: low rounded rocks awash over 200 m, grey seals hauled out on the two biggest."""
    p = Piece("SS_seals")
    xs = np.arange(-130, 132, 1.5); ys = np.arange(-80, 82, 1.5)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    blobs = [(-70, -20, 24, 15, 2.3, 0.2), (-28, 14, 18, 11, 1.4, -0.4), (12, -8, 28, 16, 2.6, 0.1), (58, 22, 15, 10, 1.0, 0.5), (86, -26, 20, 12, 1.8, -0.2),
             (-104, 30, 10, 7, 0.5, 0.0), (36, 48, 9, 6, 0.4, 0.3), (-48, -52, 11, 7, 0.7, 0.9), (112, 8, 8, 5, 0.3, 0.0)]
    H = skerry_height(X, Y, blobs, 101, rough=0.35)
    v, q = height_mesh(xs, ys, H)
    p.add(v, q, "ss_rock")

    def hgt(x, y):
        i = int(np.clip(round((x - xs[0]) / 1.5), 0, len(xs) - 1)); j = int(np.clip(round((y - ys[0]) / 1.5), 0, len(ys) - 1))
        return float(H[i, j])
    rng = np.random.default_rng(103)
    seal = Piece("sealbodies")
    spots = []
    for (cx, cy, n) in [(12, -8, 5), (-70, -20, 4), (86, -26, 2)]:
        for _ in range(n):
            for _try in range(30):
                x = cx + rng.uniform(-13, 13); y = cy + rng.uniform(-8, 8)
                z = hgt(x, y)
                if z > 1.0 and all((x - sx) ** 2 + (y - sy) ** 2 > 9.0 for sx, sy in spots):
                    break
            else:
                continue
            spots.append((x, y))
            a = rng.uniform(0, 2 * math.pi)
            L = rng.uniform(1.7, 2.3)
            tone = np.array([0.34, 0.32, 0.30]) * rng.uniform(0.55, 1.25)
            c, s = math.cos(a), math.sin(a)
            # a banana: the body, the raised head and neck, the lifted hind flippers
            seal.blob((x, y, z + 0.30), (L * 0.5, 0.38, 0.36), "ss_land", rot=a, n=8, rings=5, colour=tone, squash_under=0.6)
            seal.blob((x + c * L * 0.48, y + s * L * 0.48, z + 0.52), (0.36, 0.24, 0.26), "ss_land", rot=a, n=6, rings=4, colour=tone * 1.05)
            seal.blob((x + c * L * 0.70, y + s * L * 0.70, z + 0.66), (0.22, 0.16, 0.15), "ss_land", rot=a, n=6, rings=3, colour=tone * 0.9)
            seal.blob((x - c * L * 0.55, y - s * L * 0.55, z + 0.40), (0.30, 0.20, 0.10), "ss_land", rot=a, n=6, rings=3, colour=tone * 0.8)
            p.sockets.setdefault("seals", []).append([round(x, 2), round(y, 2), round(z + 0.3, 2), round(math.degrees(a), 1)])
    col = lambda P, N, s: rock_colour(P, N, seed=101, beds=8.0, red=0.2, guano=0.3, guano_from=1.6, wet_top=1.0)
    ob = to_object(p, 9000, colour_fn=col)
    osl = to_object_tinted(seal, None)
    ob = join([ob, osl], "SS_seals")
    return ob, p, col


# ------------------------------------------------------------------------------------------------ the home coast
W_SEA = -43.0
COAST_ORIGIN = (2000.0, 1500.0)       # the chart point of `home` (the roads): the piece's origin


def coast():
    """The home coast as seen from the sea, 2..20 km off: the game's own map heights (tools/world_map.py) as a strip
    13 km long: the clay cliffs with the forest on top, the valley with the town as blocks, the pier and its tower,
    the north hills, the cinder cones of the North, and the three far volcanoes with their snow."""
    import world_map as W
    import north
    p = Piece("SS_coast")
    SEA = W.SEA_Z
    ys = np.concatenate([np.arange(-3200.0, 4400.0, 34.0), np.arange(4400.0, 9300.0, 120.0)])
    xs = np.concatenate([np.arange(-2200.0, -900.0, 100.0), np.arange(-900.0, 100.0, 40.0), np.arange(100.0, 620.0, 20.0)])
    H = np.zeros((len(xs), len(ys)))
    for j, y in enumerate(ys):
        yy = min(max(y, -1380.0), 4195.0)            # south of the map's edge the upland's cliff simply runs on
        for i, x in enumerate(xs):
            try:
                h = W.height(float(x), float(yy))
            except Exception:
                h = SEA - 10.0
            H[i, j] = h - SEA
        if y < -1380.0:
            H[:, j] = H[:, j] * (0.9 + 0.1 * math.sin(y / 500.0)) - 40.0 * smooth(-2800.0, -3200.0, y) * (H[:, j] > 0)
        if y > 4195.0:
            # the far land north of the map: the last real profile sinking into low wooded hills
            f = (y - 4195.0) / (9300.0 - 4195.0)
            H[:, j] = H[:, j] * (1 - 0.55 * f) + 14.0 * np.sin(y / 380.0) * (xs < 150.0) * f - 30.0 * smooth(0.8, 1.0, f)
    H = np.maximum(H, -8.0)
    # frame: x = chart X - origin, y = -(chart Y - origin)
    V = []
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    verts, quads = height_mesh(xs, ys, H, floor=-1.5)
    verts[:, 0] -= COAST_ORIGIN[0]
    verts[:, 1] = -(verts[:, 1] - COAST_ORIGIN[1])
    p.add(verts, quads, "ss_land")
    # slopes for the colours (in map space)
    gx = np.gradient(H, xs, axis=0); gy = np.gradient(H, ys, axis=1)
    slope = np.hypot(gx, gy)

    def col(P, N, s):
        mx = P[:, 0] + COAST_ORIGIN[0]; my = -P[:, 1] + COAST_ORIGIN[1]; z = P[:, 2]
        i = np.clip(np.searchsorted(xs, mx), 0, len(xs) - 1); j = np.clip(np.searchsorted(ys, my), 0, len(ys) - 1)
        sl = slope[i, j]
        forest = np.array([0.12, 0.19, 0.11])[None, :] * (0.8 + 0.4 * vnoise(mx / 90.0, my / 90.0, 0.0, 111))[:, None]
        meadow = np.array([0.36, 0.40, 0.20])[None, :] * (0.85 + 0.3 * vnoise(mx / 60.0, my / 60.0, 0.0, 112))[:, None]
        clay = np.array([0.62, 0.52, 0.36])[None, :] * (0.8 + 0.4 * vnoise(mx / 25.0, my / 25.0, z / 6.0, 113))[:, None]
        sand = np.tile(SAND, (len(z), 1))
        cinder = np.array([0.23, 0.20, 0.19])[None, :] * (0.8 + 0.4 * vnoise(mx / 70.0, my / 70.0, 0.0, 114))[:, None]
        valley = smooth(30.0, 10.0, z) * smooth(200.0, 320.0, my) * (1 - smooth(1400.0, 1520.0, my))
        c = forest * (1 - valley)[:, None] + meadow * valley[:, None]
        northk = smooth(2330.0, 2700.0, my) * smooth(70.0, 110.0, z)
        c = c * (1 - northk)[:, None] + cinder * northk[:, None]
        mead_n = smooth(2330.0, 2600.0, my) * (1 - smooth(55.0, 80.0, z)) * (1 - northk)
        c = c * (1 - 0.6 * mead_n)[:, None] + meadow * (0.6 * mead_n)[:, None]
        k = smooth(0.45, 0.9, sl) * smooth(1.0, 4.0, z)
        c = c * (1 - k)[:, None] + clay * k[:, None]
        k = 1 - smooth(1.8, 3.2, z)
        c = c * (1 - k)[:, None] + sand * k[:, None]
        wet = 1 - smooth(-0.6, 0.3, z)
        c = c * (1 - 0.45 * wet)[:, None]
        return c
    # the town: its plots as blocks (the look of roofs and walls from miles off), the pier and its tower
    lights = Piece("SS_coast_lights")
    nl = 0
    try:
        import city_plan
        plots = json.load(open(os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "City", "city_plan.json")))["plots"]
    except Exception as e:      # the plan is generated: without it the coast simply has no town
        print("SEASCAPE: no city plan (%s): the coast gets no town blocks" % e)
        plots = []
    rng = np.random.default_rng(117)

    def cv(mx, my, mz):
        return (mx - COAST_ORIGIN[0], -(my - COAST_ORIGIN[1]), mz - SEA)
    nb = 0
    for pl in plots:
        m = pl.get("map") or pl.get("pos")
        if not m or pl.get("id") in ("P-SHIP",) or abs(m[0]) > 3000 or abs(m[1]) > 5000:
            continue
        w = float(pl.get("w", 14.0)); d = float(pl.get("d", 14.0))
        hmax = float(pl.get("height_max") or pl.get("height") or rng.uniform(6.0, 14.0))
        hmax = min(hmax, 45.0)
        yaw = math.radians(float(pl.get("yaw", 0.0)))
        cx, cy, cz = cv(m[0], m[1], m[2] if len(m) > 2 else W.height(m[0], m[1]))
        if pl.get("id") == "P-PIER":
            continue
        slot = "ss_white" if rng.random() < 0.6 else "ss_concrete"
        p.box((-d / 2, -w / 2, -1.0), (d / 2, w / 2, hmax), slot, rot=-yaw, at=(cx, cy, cz))
        nb += 1
        # lit windows towards the sea: a few warm points per block
        for k in range(int(2 + hmax / 5)):
            lz = cz + rng.uniform(2.0, max(2.5, hmax - 1.0)); ly = cy + rng.uniform(-w / 2, w / 2) * 0.8
            lights.box((d / 2 + 0.3, -1.4, -1.0), (d / 2 + 0.6, 1.4, 1.0), "glow_window", at=(cx, ly, lz))
            nl += 1
    # the pier (docs/world_map.md: from the promenade out to its head at map x 317, y 905, deck 3.5 m over the datum)
    ax, ay, az = cv(141.0, 905.0, SEA + 3.5)
    bx, by, _ = cv(324.0, 905.0, SEA + 3.5)
    p.box((ax, ay - 6.0, 2.9), (bx, ay + 6.0, 3.5), "ss_concrete")
    for k in range(12):
        px = ax + (bx - ax) * (k + 0.5) / 12.0
        p.box((px - 0.6, ay - 5.0, -6.0), (px + 0.6, ay + 5.0, 2.9), "ss_dark")
    p.lathe((bx - 8.0, by, 3.5), [(5.5, 0.0), (2.2, 22.0), (2.6, 33.0), (2.6, 36.0)], ["ss_white", "ss_white", "ss_dark"], 12)
    p.lathe((bx - 8.0, by, 3.5), [(0.9, 34.0), (0.9, 35.6)], "glow_lamp", 8)
    p.sockets["pier_light"] = [bx - 8.0, by, 3.5 + 34.8]
    # the sea railway (docs/railway.md): its bank and masts along map Y -60 out to x 20 km, a line on the water
    r0 = cv(620.0, -60.0, SEA); r1 = cv(20000.0, -60.0, SEA)
    p.box((r0[0], r0[1] - 3.0, -6.0), (r1[0], r0[1] + 3.0, 0.9), "ss_dark")
    for mx in np.arange(700.0, 20000.0, 150.0):
        q = cv(float(mx), -60.0, SEA)
        p.box((q[0] - 0.15, q[1] - 3.2, 0.9), (q[0] + 0.15, q[1] - 2.9, 7.5), "ss_dark")
    # promenade lamps: a string of points along the valley's beach
    for my in np.arange(460.0, 1250.0, 28.0):
        lx, ly, lz = cv(W.shore_x(float(my)) - 26.0, float(my), SEA + 8.0)
        lights.box((lx - 0.5, ly - 0.5, lz - 0.5), (lx + 0.5, ly + 0.5, lz + 0.5), "glow_lamp")
        nl += 1
    # the far volcanoes (tools/north.py far_volcanoes): cones with a crater and a snow cap
    cone_cols = []
    for vv in north.far_volcanoes():
        cx, cy, cz = cv(vv["x"], vv["y"], vv["base"])
        n = 28
        rings = [(1.0, 0.0), (0.72, 0.12), (0.5, 0.27), (0.33, 0.45), (0.2, 0.64), (0.11, 0.82), (vv["crater"] / vv["radius"], 1.0), (vv["crater"] / vv["radius"] * 0.6, 0.97)]
        verts = []
        for (rr, hh) in rings:
            for i in range(n):
                a = 2 * math.pi * i / n
                wob = 1 + 0.07 * math.sin(a * 3 + vv["x"] * 0.001) + 0.04 * math.sin(a * 7 + hh * 5)
                vx = cx + vv["radius"] * rr * wob * math.cos(a)
                if vv["base"] > 0.0:
                    # a cone standing on the land: its seaward skirt stops at the coast (the game's backdrop cone is only seen from the town)
                    lim = 150.0 - COAST_ORIGIN[0]
                    vx = lim + (vx - lim) * 0.06 if vx > lim else vx
                verts.append((vx, cy + vv["radius"] * rr * wob * math.sin(a), max(cz, -2.0) + vv["height"] * hh))
        faces = [(k * n + i, k * n + (i + 1) % n, (k + 1) * n + (i + 1) % n, (k + 1) * n + i) for k in range(len(rings) - 1) for i in range(n)]
        faces.append(tuple(range((len(rings) - 1) * n, len(rings) * n)))
        cone = Piece("cone_" + vv["id"])
        cone.add(verts, faces, "ss_land")
        cone_cols.append((cone, vv, cz))
    ob = to_object(p, None, smooth_angle=50.0, colour_fn=col, decimate_slots=())
    parts = [ob]
    for cone, vv, cz in cone_cols:
        def ccol(P, N, s, vv=vv, cz=cz):
            z = P[:, 2] - max(cz, -2.0)
            rock = np.array([0.30, 0.27, 0.27])[None, :] * (0.8 + 0.4 * vnoise(P[:, 0] / 400.0, P[:, 1] / 400.0, P[:, 2] / 300.0, 119))[:, None]
            low = np.array([0.20, 0.26, 0.17])[None, :]
            k = smooth(vv["height"] * 0.12, vv["height"] * 0.3, z)
            c = low * (1 - k)[:, None] + rock * k[:, None]
            sn = smooth(vv["snow"] - 180.0, vv["snow"] + 120.0, z + 220.0 * fbm(P[:, 0], P[:, 1], 0.0, 900.0, 3, 120))
            return c * (1 - sn)[:, None] + np.array([0.93, 0.94, 0.96])[None, :] * sn[:, None]
        parts.append(to_object(cone, None, smooth_angle=60.0, colour_fn=ccol, decimate_slots=()))
    cones = parts[1:]
    for o, (cone, vv, cz) in zip(cones, cone_cols):
        # each cone is a piece of its own about its summit's foot (the seascape places and sinks them one by one)
        o.name = "SS_volcano_" + vv["id"]; o.data.name = o.name
        cx, cy, _ = cv(vv["x"], vv["y"], 0.0)
        for v in o.data.vertices:
            v.co.x -= cx; v.co.y -= cy
    ol = to_object(lights, None, colour_fn=None) if nl else None
    coast.cones = [(o, vv) for o, (cone, vv, cz) in zip(cones, cone_cols)]
    print("SEASCAPE: coast with %d town blocks, %d lights" % (nb, nl))
    return ob, p, col, ol


# ------------------------------------------------------------------------------------------------ texture
def write_png(path, rgb):
    """An 8-bit RGB png with the standard library alone (Blender's Python has no PIL)."""
    import struct
    import zlib
    h, w, _ = rgb.shape
    raw = np.concatenate([np.zeros((h, 1), np.uint8), np.ascontiguousarray(rgb).reshape(h, w * 3)], 1).tobytes()

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def write_textures():
    """A tiling grey rock detail (mid grey 0.5 so the vertex colour x 2 x it keeps its tone) and its normal map."""
    os.makedirs(TEX_DIR, exist_ok=True)
    n = 1024
    u = np.arange(n) / n
    U, V = np.meshgrid(u, u, indexing="ij")

    def tile_noise(freq, seed):
        # periodic value noise: lattice hashed modulo freq
        x = U * freq; y = V * freq
        ix = np.floor(x); iy = np.floor(y); fx = x - ix; fy = y - iy
        fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy)
        out = 0
        for dx in (0, 1):
            for dy in (0, 1):
                out = out + _hash(np.mod(ix + dx, freq), np.mod(iy + dy, freq), ix * 0, seed) * (fx if dx else 1 - fx) * (fy if dy else 1 - fy)
        return out
    h = 0
    amp = 1.0; tot = 0
    for o, f in enumerate((4, 8, 16, 32, 64, 128, 256)):
        h = h + amp * tile_noise(f, 200 + o); tot += amp; amp *= 0.62
    h = h / tot
    # cracks: thin dark lines where two noises cross their middles
    cr = np.minimum(np.abs(tile_noise(6, 300) + 0.3 * tile_noise(24, 301) - 0.65), np.abs(tile_noise(9, 302) + 0.3 * tile_noise(31, 303) - 0.65))
    crack = 1 - smooth(0.0, 0.012, cr)
    speck = smooth(0.80, 0.95, tile_noise(256, 304)) * 0.12 - smooth(0.82, 0.95, tile_noise(200, 305)) * 0.10
    c = 0.5 + (h - 0.5) * 0.55 + speck - 0.22 * crack
    c = np.clip(c + (0.5 - c.mean()), 0, 1)
    rgb = np.stack([c * 1.01, c, c * 0.985], 2)
    write_png(os.path.join(TEX_DIR, "T_SS_rock_C.png"), (np.clip(rgb, 0, 1) * 255).astype(np.uint8))
    hh = h - 0.35 * crack
    dx = (np.roll(hh, -1, 0) - np.roll(hh, 1, 0)) * 22.0; dy = (np.roll(hh, -1, 1) - np.roll(hh, 1, 1)) * 22.0
    nrm = np.stack([-dx, -dy, np.ones_like(dx)], 2)
    nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
    write_png(os.path.join(TEX_DIR, "T_SS_rock_N.png"), ((nrm * 0.5 + 0.5) * 255).astype(np.uint8))


# ------------------------------------------------------------------------------------------------ previews
def setup_preview_scene():
    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    scn.cycles.samples = 20
    scn.cycles.use_denoising = False
    scn.cycles.device = "CPU"
    scn.render.threads_mode = "FIXED"; scn.render.threads = 3
    scn.render.resolution_x = 800; scn.render.resolution_y = 450
    scn.render.image_settings.file_format = "JPEG"; scn.render.image_settings.quality = 88
    scn.view_settings.view_transform = "Filmic" if "Filmic" in [i.identifier for i in type(scn.view_settings).bl_rna.properties["view_transform"].enum_items] else "AgX"
    w = bpy.data.worlds.new("sky"); scn.world = w; w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    sky = w.node_tree.nodes.new("ShaderNodeTexSky")
    try:
        sky.sun_elevation = math.radians(28); sky.sun_rotation = math.radians(210)
    except Exception:
        pass
    w.node_tree.links.new(sky.outputs[0], bg.inputs[0]); bg.inputs[1].default_value = 0.35
    scn.view_settings.exposure = -1.6
    try:
        scn.view_settings.look = "AgX - Medium High Contrast"
    except Exception:
        pass
    # the sea: a big dark glossy plane with a little chop
    bpy.ops.mesh.primitive_plane_add(size=60000, location=(0, 0, -0.6))
    sea = bpy.context.active_object; sea.name = "PreviewSea"
    m = bpy.data.materials.new("sea"); m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (0.012, 0.045, 0.06, 1); b.inputs["Roughness"].default_value = 0.12
    nz = m.node_tree.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 9000.0; nz.inputs["Detail"].default_value = 4.0
    bump = m.node_tree.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.25; bump.inputs["Distance"].default_value = 1.0
    m.node_tree.links.new(nz.outputs[0], bump.inputs["Height"]); m.node_tree.links.new(bump.outputs[0], b.inputs["Normal"])
    sea.data.materials.append(m)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); scn.collection.objects.link(cam); scn.camera = cam
    cam.data.clip_end = 100000
    return sea, cam


def render_views(ob, name, out, cam, views):
    """views: (tag, distance m, azimuth deg in the piece's frame, horizontal fov deg, aim height m)."""
    scn = bpy.context.scene
    for o in scn.objects:
        if o.type == "MESH" and o.name != "PreviewSea":
            o.hide_render = o is not ob and not o.name.startswith(ob.name + "_lights")
    for (tag, dist, az, fov, aim) in views:
        a = math.radians(az)
        cam.location = (dist * math.cos(a), dist * math.sin(a), 7.0)
        d = np.array([0, 0, aim]) - np.array(cam.location)
        cam.rotation_euler = (math.atan2(math.hypot(d[0], d[1]), -d[2]), 0, math.atan2(d[1], d[0]) - math.pi / 2)
        cam.data.angle = math.radians(fov)
        sky = scn.world.node_tree.nodes.get("Sky Texture")
        if sky is not None:
            try:
                sky.sun_rotation = math.radians(90.0 - (az + 50.0))      # the sun over the viewer's shoulder
            except Exception:
                pass
        scn.render.filepath = os.path.join(out, "island_%s_%s.jpg" % (name, tag))
        bpy.ops.render.render(write_still=True)
        print("SEASCAPE preview", scn.render.filepath)


# ------------------------------------------------------------------------------------------------ main
CHART = {       # where each piece's origin stands on the chart (DESIGN's table), its yaw about Z there (degrees, chart frame)
    "brothers": (12000.0, 7000.0, 25.0), "arch": (28000.0, 16000.0, 70.0), "birds": (45000.0, 12000.0, 0.0),
    "lighthouse": (52000.0, 30000.0, 0.0), "long": (30000.0, 38000.0, 0.0), "seals": (11000.0, 24000.0, 40.0),
    "home": (COAST_ORIGIN[0], COAST_ORIGIN[1], 0.0),
}
VIEWS = {       # previews: from sea level at 500 m (a 60 degree field) and at 2 km (20 degrees: through binoculars)
    "brothers": (-70, 14), "arch": (-90, 14), "birds": (5, 30), "lighthouse": (80, 14), "long": (75, 6), "seals": (-80, 2),
}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    prev = argv[argv.index("--preview") + 1] if "--preview" in argv else None
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if not os.path.exists(os.path.join(TEX_DIR, "T_SS_rock_C.png")) or not only:
        write_textures()
    makers = [("brothers", brothers), ("arch", arch), ("birds", birds), ("lighthouse", lighthouse), ("long", long_island), ("seals", seals)]
    built = []
    info = {}
    for key, fn in makers:
        if only and key not in only:
            continue
        ob, piece, _col = fn()
        far = far_copy(ob, {"long": 0.25, "lighthouse": 0.45, "seals": 0.3}.get(key, 0.10))
        built += [ob, far]
        bb = np.array([v.co[:] for v in ob.data.vertices])
        info[key] = {"mesh": ob.name, "far": far.name, "tris": tris(ob), "far_tris": tris(far),
                     "size": [round(float(x), 1) for x in (bb.max(0) - bb.min(0))], "top": round(float(bb[:, 2].max()), 1),
                     "radius": round(float(np.hypot(bb[:, 0], bb[:, 1]).max()), 1),
                     "sockets": {k: v for k, v in piece.sockets.items()}}
        print("SEASCAPE %-12s %6d tris, far %5d, size %s" % (ob.name, tris(ob), tris(far), info[key]["size"]))
    if not only or "home" in only:
        ob, piece, _col, ol = coast()
        built.append(ob)
        bb = np.array([v.co[:] for v in ob.data.vertices])
        info["home"] = {"mesh": ob.name, "far": "", "lights": ol.name if ol else "", "tris": tris(ob), "far_tris": 0,
                        "size": [round(float(x), 1) for x in (bb.max(0) - bb.min(0))], "top": round(float(bb[:, 2].max()), 1),
                        "radius": round(float(np.hypot(bb[:, 0], bb[:, 1]).max()), 1), "sockets": dict(piece.sockets)}
        if ol:
            built.append(ol)
            info["home"]["lights_tris"] = tris(ol)
        for o, vv in coast.cones:
            built.append(o)
            key = "volcano_" + vv["id"]
            CHART[key] = (vv["x"], vv["y"], 0.0)
            info[key] = {"mesh": o.name, "far": "", "tris": tris(o), "far_tris": 0, "size": [2 * vv["radius"], 2 * vv["radius"], vv["height"]],
                         "top": vv["height"] + max(vv["base"] - W_SEA, 0.0), "radius": vv["radius"], "sockets": {"plume": [0.0, 0.0, vv["height"]]} if vv.get("plume") else {}}
        print("SEASCAPE %-12s %6d tris, size %s" % (ob.name, tris(ob), info["home"]["size"]))
    total = sum(tris(o) for o in built)
    print("SEASCAPE total %d triangles in %d meshes" % (total, len(built)))
    # glitch checks: boxed metre UVs must not stretch on the rock (the house rule: look for UV trouble on every asset)
    problems = uv_check(built)
    if prev:
        os.makedirs(prev, exist_ok=True)
        preview_materials(True)
        _sea, cam = setup_preview_scene()
        for key, (az, aim) in VIEWS.items():
            if key in info:
                ob = bpy.data.objects[info[key]["mesh"]]
                wide = key in ("birds", "long")
                views = [("500", 500.0 + (info[key]["radius"] * 0.6 if key == "birds" else 0.0), az, 60.0 if wide else 24.0, aim),
                         ("2000", 2000.0, az, 40.0 if key == "long" else (20.0 if wide else 8.0), aim)]
                if "--close" in argv:
                    views = [("close", 160.0 + info[key]["radius"], az + 25.0, 40.0, aim)]
                render_views(ob, key, prev, cam, views)
        if "home" in info:
            ob = bpy.data.objects[info["home"]["mesh"]]
            for o, vv in coast.cones:           # in their places for the picture
                o.location = (vv["x"] - COAST_ORIGIN[0], -(vv["y"] - COAST_ORIGIN[1]), 0.0)
                o.name = ob.name + "_lights_" + vv["id"]
            if info["home"].get("lights"):
                bpy.data.objects[info["home"]["lights"]].name = ob.name + "_lights"
            scn = bpy.context.scene
            render_views(ob, "home", prev, cam, [("3000", 3000.0, 0.0, 60.0, 30.0), ("12000", 12000.0, 0.0, 40.0, 200.0)])
        preview_materials(False)
    if only:
        return
    if total > 250000:
        raise SystemExit("SEASCAPE: %d triangles, over the 250k budget" % total)
    os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in built:
        o.location = (0, 0, 0)
        o.hide_render = False
        o.select_set(True)
    kw = dict(filepath=OUT_GLB, use_selection=True, export_format="GLB", export_yup=True, export_apply=True, export_image_format="NONE")
    try:
        bpy.ops.export_scene.gltf(export_vertex_color="ACTIVE", export_all_vertex_colors=False, export_active_vertex_color_when_no_material=True, **kw)
    except TypeError:
        bpy.ops.export_scene.gltf(**kw)
    out = {
        "_doc": "Made by blender/ship/seascape_kit.py; metres. A piece's frame in Unreal after the glTF import: X = the chart's X "
                "(west, out to sea), Y = the chart's Y (north), Z up from the sea's DATUM (not the tide: the rocks stand, the water falls). "
                "Sockets are in that frame (the script's Y negated). Assets: /Game/Cruise/Meshes/seascape_kit/SM_<mesh>.",
        "uv_tile_m": UV_TILE, "skirt_m": SKIRT, "total_tris": total,
        "islands": {},
    }
    for key, d in info.items():
        cx, cy, yaw = CHART[key]
        socks = {}
        for k, v in d["sockets"].items():
            if isinstance(v, list) and v and isinstance(v[0], list):
                socks[k] = [[q[0], -q[1], q[2]] + ([-q[3]] if len(q) > 3 else []) for q in v]
            elif isinstance(v, list):
                socks[k] = [round(v[0], 2), round(-v[1], 2), round(v[2], 2)]
            else:
                socks[k] = v
        out["islands"][key] = {"mesh": d["mesh"], "far": d["far"], "lights": d.get("lights", ""), "chart": [cx, cy], "yaw": yaw,
                               "tris": d["tris"], "far_tris": d["far_tris"], "size": d["size"], "top": d["top"], "radius": d["radius"],
                               "sockets": socks}
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w") as f:
        json.dump(out, f, indent=1)
    print("SEASCAPE: %d meshes, %d triangles, %d UV findings -> %s" % (len(built), total, problems, OUT_GLB))


def uv_check(obs):
    """Metre UVs: a triangle's UV area against its real area (1.0 = true); zero-area UVs. Prints SEASCAPELINT lines."""
    bad = 0
    for ob in obs:
        me = ob.data
        if not me.uv_layers:
            print("SEASCAPELINT %s: no UVs" % ob.name); bad += 1
            continue
        nl = len(me.loops)
        uv = np.zeros(nl * 2); me.uv_layers[0].data.foreach_get("uv", uv); uv = uv.reshape(-1, 2) * UV_TILE
        lv = np.zeros(nl, np.int64); me.loops.foreach_get("vertex_index", lv)
        co = np.zeros(len(me.vertices) * 3); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
        ls = np.zeros(len(me.polygons), np.int64); me.polygons.foreach_get("loop_start", ls)
        mi = np.zeros(len(me.polygons), np.int64); me.polygons.foreach_get("material_index", mi)
        a, b, c = co[lv[ls]], co[lv[ls + 1]], co[lv[ls + 2]]
        area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
        ua, ub, uc = uv[ls], uv[ls + 1], uv[ls + 2]
        uarea = 0.5 * np.abs((ub[:, 0] - ua[:, 0]) * (uc[:, 1] - ua[:, 1]) - (ub[:, 1] - ua[:, 1]) * (uc[:, 0] - ua[:, 0]))
        rock = np.array([me.materials[i].name == "ss_rock" for i in mi]) & (area > 1e-4)
        if rock.any():
            ratio = uarea[rock] / area[rock]
            # a boxed projection shows a face at cos(angle to its axis): never under 1/sqrt(3) = 0.577
            n_bad = int((ratio < 0.5).sum()); n_zero = int((uarea[rock] < 1e-9).sum())
            print("SEASCAPELINT %-22s rock tris %6d, UV/real area min %.2f median %.2f, under 0.5: %d, zero: %d" %
                  (ob.name, int(rock.sum()), float(ratio.min()), float(np.median(ratio)), n_bad, n_zero))
            bad += n_bad + n_zero
    return bad


main()
