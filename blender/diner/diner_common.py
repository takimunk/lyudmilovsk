"""Shared helpers of the diner's Blender builds (docs/diner.md): the material slots (DN_*), mesh primitives on bmesh, metre UVs
and the glTF export. Used by building.py (the car, the kitchen annex, the furniture) and tableware.py (plates, cups, food).

Frame of every part: metres, x along the car (+x the rounded end), y across it (-y the street front with the door), z up;
exported with export_yup (glTF), so Unreal gets x -> X, y -> -Y.

Material slots: MATS order = material index; MAT_SPEC[name] = (hex colour, roughness, metallic, emissive), read by
unreal/scripts/import_diner.py to make the flat material instances. Picture slots (signs, the menu board) are
DN_Sign_<name>: 0..1 UVs over the picture (import_diner.py / ALyudDiner swap the player's language).
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.environ.get("DINER_OUT") or os.path.join(ROOT, "design", "diner", "game")     # DINER_OUT: a scratch dir for trial builds

# name: (hex, roughness, metallic, emissive)
MAT_SPEC = {
    "DN_EnamelRed": ("#9C2228", 0.22, 0.0, 0.0),      # porcelain enamel panels (the Modern Diner's red and cream)
    "DN_EnamelCream": ("#EFE2C2", 0.22, 0.0, 0.0),
    "DN_Steel": ("#B7BABD", 0.32, 1.0, 0.0),          # brushed stainless: fluted bands, the back bar
    "DN_Chrome": ("#DCDFE3", 0.12, 1.0, 0.0),         # stool rims, trim, rails
    "DN_Glass": ("#9DB1B5", 0.05, 0.0, 0.0),          # window panes: split into the _Glass mesh
    "DN_Mahogany": ("#5B2B1B", 0.45, 0.0, 0.0),       # the interior trim, booth backs
    "DN_Formica": ("#E3D7BD", 0.35, 0.0, 0.0),        # counter and table tops
    "DN_Vinyl": ("#A01E26", 0.45, 0.0, 0.0),          # stool and booth seats
    "DN_FloorTile": ("#B9AC93", 0.5, 0.0, 0.0),       # the floor's small tiles (light)
    "DN_FloorDark": ("#3A2F2A", 0.5, 0.0, 0.0),       # the floor's border and checker (dark)
    "DN_WallTile": ("#EADFC6", 0.3, 0.0, 0.0),        # tiles under the windows inside, the counter's front
    "DN_Ceiling": ("#F1E7D2", 0.7, 0.0, 0.0),
    "DN_Brick": ("#7B3B2D", 0.9, 0.0, 0.0),           # the foundation and the kitchen annex
    "DN_Concrete": ("#8F8B83", 0.9, 0.0, 0.0),        # steps, the forecourt
    "DN_Dark": ("#1F1D1C", 0.6, 0.0, 0.0),            # rubber, the grill's iron, stool feet
    "DN_Opal": ("#FFF1D6", 0.4, 0.0, 6.0),            # lamp globes (lit)
    "DN_Roof": ("#A9ADAD", 0.45, 0.6, 0.0),           # the barrel roof's painted metal
    "DN_Plaster": ("#E6E1D6", 0.85, 0.0, 0.0),        # the kitchen's walls
    "DN_KitchenTile": ("#C9CBC6", 0.4, 0.0, 0.0),     # the kitchen's floor
    "DN_Coffee": ("#2E1A0E", 0.08, 0.0, 0.0),
    "DN_China": ("#F4F1E8", 0.18, 0.0, 0.0),          # plates, mugs, saucers
    "DN_ChinaRim": ("#7E1D22", 0.2, 0.0, 0.0),        # the red band of the diner's china
    # fittings (the quality rebuild, 2026-11)
    "DN_Aluminum": ("#C9CDD1", 0.3, 1.0, 0.0),        # ribbed table and counter edging (texture: ribs along u)
    "DN_Brass": ("#B48C55", 0.28, 1.0, 0.0),          # register trim, urn taps' collars, kick plates
    "DN_Bakelite": ("#16130F", 0.32, 0.0, 0.0),       # black knobs, handles, register keys' rims
    "DN_EnamelWhite": ("#ECEAE3", 0.22, 0.0, 0.0),    # appliances: the fridge, the range, the mixer
    "DN_CastIron": ("#2B2826", 0.65, 0.5, 0.0),       # the griddle plate, the range's grates
    "DN_Paper": ("#F4F1E8", 0.85, 0.0, 0.0),          # napkins, check pads, the jukebox's title cards
    "DN_RedPlastic": ("#B3161C", 0.3, 0.0, 0.0),      # ketchup bottles
    "DN_YellowPlastic": ("#E0B01E", 0.3, 0.0, 0.0),   # mustard bottles
    "DN_Rubber": ("#272422", 0.85, 0.0, 0.0),         # mats, glides, door stops
    "DN_Neon": ("#FF5A40", 0.4, 0.0, 6.0),            # neon tubes (lit; ALyudDiner dims them with the signs)
    # food (tableware.py)
    "DN_Pancake": ("#C98A43", 0.6, 0.0, 0.0),
    "DN_Syrup": ("#7A3510", 0.1, 0.0, 0.0),
    "DN_SourCream": ("#F3EEE1", 0.35, 0.0, 0.0),
    "DN_Jam": ("#7E1626", 0.15, 0.0, 0.0),
    "DN_EggWhite": ("#F6F1E4", 0.35, 0.0, 0.0),
    "DN_Yolk": ("#F0AA16", 0.25, 0.0, 0.0),
    "DN_Bacon": ("#9A3E28", 0.5, 0.0, 0.0),
    "DN_Toast": ("#C88A48", 0.75, 0.0, 0.0),
    "DN_Noodle": ("#E6CB82", 0.45, 0.0, 0.0),
    "DN_Sauce": ("#A2361C", 0.3, 0.0, 0.0),
    "DN_Meatball": ("#6A3824", 0.6, 0.0, 0.0),
    "DN_Crust": ("#CF9550", 0.7, 0.0, 0.0),
    "DN_PieFilling": ("#C29550", 0.4, 0.0, 0.0),
    "DN_Bun": ("#C47431", 0.6, 0.0, 0.0),
    "DN_Patty": ("#4A2818", 0.7, 0.0, 0.0),
    "DN_Cheese": ("#EFA92F", 0.4, 0.0, 0.0),
    "DN_Lettuce": ("#6C9B37", 0.5, 0.0, 0.0),
    "DN_Soup": ("#C68A45", 0.12, 0.0, 0.0),
    "DN_Fish": ("#E9DCC6", 0.5, 0.0, 0.0),
    "DN_Herb": ("#4E7B2E", 0.6, 0.0, 0.0),
    "DN_FishSkin": ("#B8782E", 0.45, 0.0, 0.0),       # the fried fish's floured, browned skin (the hero's catch, docs/fishing.md)
    "DN_Potato": ("#E7CF86", 0.5, 0.0, 0.0),
    "DN_Lemon": ("#EED54A", 0.3, 0.0, 0.0),
    "DN_Tea": ("#7F3E0E", 0.06, 0.0, 0.0),
    "DN_Kompot": ("#6E1A2A", 0.06, 0.0, 0.0),
    "DN_Berry": ("#5A1424", 0.3, 0.0, 0.0),
}
MATS = list(MAT_SPEC)

# slot: (texture set, metres per texture tile, normal strength): textures/diner/<set>_{D,N,R}.* (tools/diner_textures.py; D
# base colour sRGB, N tangent normal DirectX green, R mask: R roughness, G ambient occlusion). UV 0 is in metres (1 unit = 1 m),
# so Unreal's Tiling = 1 / metres. A slot without an entry stays a flat colour (MAT_SPEC). The D maps carry the colour, so the
# MAT_SPEC colour of a textured slot is only the preview's and the fallback's.
MAT_TEX = {
    "DN_EnamelRed": ("enamel_red", 1.0, 0.6),
    "DN_EnamelCream": ("enamel_cream", 1.0, 0.6),
    "DN_Steel": ("steel_brushed", 0.5, 0.5),
    "DN_Mahogany": ("mahogany", 1.2, 0.8),
    "DN_Formica": ("formica", 0.6, 0.4),
    "DN_Vinyl": ("vinyl_red", 0.25, 1.0),
    "DN_FloorTile": ("floor_checker", 1.0, 1.0),
    "DN_FloorDark": ("floor_border", 0.5, 0.8),
    "DN_WallTile": ("wall_tile", 0.4, 1.0),
    "DN_Ceiling": ("ceiling", 1.22, 0.8),
    "DN_Brick": ("brick", 1.0, 1.0),
    "DN_Concrete": ("concrete", 1.0, 0.8),
    "DN_Plaster": ("plaster", 1.0, 0.6),
    "DN_KitchenTile": ("kitchen_tile", 0.6, 1.0),
    "DN_Roof": ("roof", 1.0, 0.8),
    "DN_Aluminum": ("aluminum_ribbed", 0.05, 1.0),
}


def M(name):
    """Material index of a slot."""
    return MATS.index(name)


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


# ---- UVs ----------------------------------------------------------------------------------------------------------------
# UV 0 is in metres. Faces made by lathe/tube/sphere/sweep carry their own UVs (u round the axis = arc length, v along the
# profile = its length: brushed steel and grain run round a stool's column without seams); every other face is box-projected
# at export by its dominant normal (z: (x, y), y: (x, z), x: (y, z)). own_uv() marks faces that already have theirs.

def _uvl(bm):
    return bm.loops.layers.uv.verify()


def _uvflag(bm):
    return bm.faces.layers.int.get("uvset") or bm.faces.layers.int.new("uvset")


def ensure_uv(bm):
    """Makes bm's UV and flag layers. Call it before making faces you will give own_uv(): adding a layer to a bmesh
    invalidates the faces made before (ReferenceError)."""
    _uvl(bm)
    _uvflag(bm)


def own_uv(bm, face, uvs):
    """Gives face its own UVs (one (u, v) per loop, metres) and marks it so export keeps them (ensure_uv(bm) first)."""
    lay = bm.loops.layers.uv.active
    assert lay is not None and bm.faces.layers.int.get("uvset") is not None, "ensure_uv(bm) before making the faces"
    for lp, uv in zip(face.loops, uvs):
        lp[lay].uv = uv
    face[_uvflag(bm)] = 1


# ---- primitives (each adds faces to a bmesh with a material index) ------------------------------------------------------

def quad(bm, pts, mi):
    f = bm.faces.new([bm.verts.new(Vector(p)) for p in pts])
    f.material_index = mi
    return f


def box(bm, c, size, mi, rot=None, faces="all"):
    """A box centred at c (size x, y, z); faces: 'all' or a string of the sides to keep (xXyYzZ: -x +x -y +y -z +z)."""
    r = rot or Matrix.Identity(3)
    h = Vector(size) / 2
    v = [bm.verts.new(Vector(c) + r @ Vector((sx * h.x, sy * h.y, sz * h.z))) for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
    sides = {"z": (0, 2, 3, 1), "Z": (4, 5, 7, 6), "y": (0, 1, 5, 4), "Y": (2, 6, 7, 3), "x": (0, 4, 6, 2), "X": (1, 3, 7, 5)}
    for k, ids in sides.items():
        if faces == "all" or k in faces:
            bm.faces.new([v[i] for i in ids]).material_index = mi


def bx(bm, mi, x0, x1, y0, y1, z0, z1, faces="all"):
    box(bm, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), mi, faces=faces)


def rbox(bm, mi, x0, x1, y0, y1, z0, z1, r=0.01, seg=2, edges="all", rot=None, pivot=None):
    """A closed box x0..x1 / y0..y1 / z0..z1 with its edges rounded (radius r, seg segments): edges "all", "top" (the 4 top
    edges), "vert" (the 4 upright ones) or "topvert". rot: a 3x3 matrix turning it about pivot (default its centre).
    Returns the new faces."""
    before = set(bm.faces)
    box(bm, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), mi)
    new = [f for f in bm.faces if f not in before]
    vs = {v for f in new for v in f.verts}
    es = {e for f in new for e in f.edges}
    zt = max(v.co.z for v in vs)

    def keep(e):
        a, b = e.verts
        vert = abs(a.co.z - b.co.z) > 1e-6
        top = not vert and abs(a.co.z - zt) < 1e-6 and abs(b.co.z - zt) < 1e-6
        return edges == "all" or (edges == "top" and top) or (edges == "vert" and vert) or (edges == "topvert" and (top or vert))
    sel = [e for e in es if keep(e)]
    if r > 0 and sel:
        r = min(r, 0.49 * min(x1 - x0, y1 - y0, z1 - z0))
        res = bmesh.ops.bevel(bm, geom=sel + list({v for e in sel for v in e.verts}), offset=r, offset_type="OFFSET", segments=seg,
                              profile=0.5, affect="EDGES", clamp_overlap=True)
        for f in res.get("faces", []):
            f.material_index = mi
    new = [f for f in bm.faces if f not in before]
    if rot is not None:
        c = Vector(pivot) if pivot is not None else Vector(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
        for v in {v for f in new for v in f.verts}:
            v.co = c + rot @ (v.co - c)
    return new


def tube(bm, a, b, r, mi, seg=10, caps=True, r1=None):
    ensure_uv(bm)
    a, b = Vector(a), Vector(b)
    x = (b - a).normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    rb_ = r if r1 is None else r1
    ra = [bm.verts.new(a + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)]
    rb = [bm.verts.new(b + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * rb_) for i in range(seg)]
    L = (b - a).length
    for i in range(seg):
        j = (i + 1) % seg
        f = bm.faces.new((ra[i], ra[j], rb[j], rb[i]))
        f.material_index = mi
        ua = math.pi * (r + rb_) / seg
        own_uv(bm, f, ((ua * i, 0.0), (ua * (i + 1), 0.0), (ua * (i + 1), L), (ua * i, L)))
    if caps:
        bm.faces.new(ra[::-1]).material_index = mi
        bm.faces.new(rb).material_index = mi


def lathe(bm, prof, mats, seg=24, c=(0, 0, 0), cap0=True, cap1=True, sx=1.0, sy=1.0):
    """Turns prof [(r, z), ...] round the z axis at c; mats: material per band (the last repeats)."""
    ensure_uv(bm)
    if isinstance(mats, int):
        mats = [mats]
    c = Vector(c)
    rings = [[bm.verts.new(c + Vector((max(r, 1e-4) * sx * math.cos(2 * math.pi * i / seg), max(r, 1e-4) * sy * math.sin(2 * math.pi * i / seg), z)))
              for i in range(seg)] for r, z in prof]
    vs = [0.0]
    for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
        vs.append(vs[-1] + math.hypot(r1 - r0, z1 - z0))
    for k in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            f = bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
            f.material_index = mats[min(k, len(mats) - 1)]
            # a band that runs up the axis wraps round it (one scale for both rings: no shear); a band that runs out across
            # it (a disk, a rim's top) is left to the box projection, flat over it
            if abs(prof[k + 1][1] - prof[k][1]) >= abs(prof[k + 1][0] - prof[k][0]):
                ua = 2 * math.pi * (prof[k][0] + prof[k + 1][0]) / 2 * (sx + sy) / 2 / seg
                own_uv(bm, f, ((ua * i, vs[k]), (ua * (i + 1), vs[k]), (ua * (i + 1), vs[k + 1]), (ua * i, vs[k + 1])))
    if cap0 and prof[0][0] > 1e-3:
        bm.faces.new(rings[0][::-1]).material_index = mats[0]
    if cap1 and prof[-1][0] > 1e-3:
        bm.faces.new(rings[-1]).material_index = mats[-1]


def sphere(bm, c, r, mi, seg=16, rings=8, sz=1.0):
    prof = [(r * math.sin(math.pi * k / rings), -r * sz * math.cos(math.pi * k / rings)) for k in range(rings + 1)]
    lathe(bm, prof, [mi], seg, c, False, False)


def prism(bm, poly, z0, z1, mi, mi_top=None, mi_side=None, top=True, bottom=True):
    """An upright prism over a counter-clockwise polygon [(x, y), ...] from z0 to z1."""
    lo = [bm.verts.new((x, y, z0)) for x, y in poly]
    hi = [bm.verts.new((x, y, z1)) for x, y in poly]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i])).material_index = mi if mi_side is None else mi_side
    if bottom:
        bm.faces.new(lo[::-1]).material_index = mi
    if top:
        bm.faces.new(hi).material_index = mi if mi_top is None else mi_top


# ---- export -----------------------------------------------------------------------------------------------------------

def metre_uv(me, picture=None):
    """Box-projected UVs in metres for the faces without their own (own_uv); faces of the picture slots (DN_Sign_*) get 0..1
    over their own extent."""
    uv = me.uv_layers[0] if len(me.uv_layers) else me.uv_layers.new(name="UVMap")
    uv.name = "UVMap"
    flag = me.attributes.get("uvset")
    own = [0] * len(me.polygons)
    if flag is not None:
        flag.data.foreach_get("value", own)
    pic = {i for i, m in enumerate(me.materials) if m and m.name.startswith("DN_Sign_")}
    for poly in me.polygons:
        if own[poly.index] and poly.material_index not in pic:
            continue
        n = poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        co = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
        if poly.material_index in pic:
            # the picture: u along the face's horizontal direction as seen from the front, v up
            t = Vector((-n.y, n.x, 0.0))
            t = t.normalized() if t.length > 1e-6 else Vector((1, 0, 0))
            us = [p.dot(t) for p in co]
            vs = [p.z for p in co]
            u0, u1, v0, v1 = min(us), max(us), min(vs), max(vs)
            for li, u, v in zip(poly.loop_indices, us, vs):
                uv.data[li].uv = ((u - u0) / max(u1 - u0, 1e-6), (v - v0) / max(v1 - v0, 1e-6))
            continue
        for li, p in zip(poly.loop_indices, co):
            uv.data[li].uv = (p.x, p.y) if ax == 2 else ((p.x, p.z) if ax == 1 else (p.y, p.z))


def _object(bm, name, extra_mats=()):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    mats = MATS + list(extra_mats)
    for k in mats:
        mat = bpy.data.materials.get(k) or bpy.data.materials.new(k)
        if k in MAT_SPEC:
            mat.diffuse_color = _lin(MAT_SPEC[k][0]) + (1,)
        me.materials.append(mat)
    me.update()
    metre_uv(me)
    if me.attributes.get("uvset") is not None:
        me.attributes.remove(me.attributes["uvset"])
    # drop unused slots so Unreal gets only the materials the mesh has
    used = sorted({p.material_index for p in me.polygons})
    keep = [me.materials[i] for i in used]
    remap = {old: new for new, old in enumerate(used)}
    idx = [remap[p.material_index] for p in me.polygons]
    me.materials.clear()                 # (clearing resets the faces' indices: set them again after)
    for m in keep:
        me.materials.append(m)
    for p, i in zip(me.polygons, idx):
        p.material_index = i
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def _write(o, path):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_yup=True, export_apply=True)


def export(bm, name, out=None, extra_mats=(), split_glass=True, smooth=None, angle=None):
    """Writes <out>/<name>.glb (and <name>_Glass.glb with the DN_Glass faces when there are any); frees bm.
    smooth: material names to shade smooth (rounded things: china, food, vinyl); angle (degrees): instead shade every face
    smooth and keep edges sharper than angle hard (bevelled furniture: round where it is rounded, crisp where it is cut)."""
    out = out or OUT
    os.makedirs(out, exist_ok=True)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    gi = M("DN_Glass")
    glass = [f for f in bm.faces if f.material_index == gi] if split_glass else []
    tris = 0
    if glass:
        g = bm.copy()
        bmesh.ops.delete(g, geom=[f for f in g.faces if f.material_index != gi], context="FACES")
        og = _object(g, name + "_Glass", extra_mats)
        g.free()
        _write(og, os.path.join(out, name + "_Glass.glb"))
        bpy.data.objects.remove(og)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index == gi], context="FACES")
    elif os.path.isfile(os.path.join(out, name + "_Glass.glb")):
        os.remove(os.path.join(out, name + "_Glass.glb"))
    o = _object(bm, name, extra_mats)
    bm.free()
    if angle is not None:
        o.data.shade_smooth()
        o.data.set_sharp_from_angle(angle=math.radians(angle))
    elif smooth:
        names = set(smooth)
        for p in o.data.polygons:
            m = o.data.materials[p.material_index]
            p.use_smooth = bool(m and m.name in names)
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    _write(o, os.path.join(out, name + ".glb"))
    me = o.data
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    print("DINER", name, tris, "tris", "+glass" if glass else "", flush=True)
    return tris


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
