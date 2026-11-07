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
OUT = os.path.join(ROOT, "design", "diner", "game")

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


def M(name):
    """Material index of a slot."""
    return MATS.index(name)


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


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


def tube(bm, a, b, r, mi, seg=10, caps=True, r1=None):
    a, b = Vector(a), Vector(b)
    x = (b - a).normalized()
    up = Vector((0, 0, 1)) if abs(x.z) < 0.95 else Vector((1, 0, 0))
    y = up.cross(x).normalized()
    z = x.cross(y)
    rb_ = r if r1 is None else r1
    ra = [bm.verts.new(a + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * r) for i in range(seg)]
    rb = [bm.verts.new(b + (y * math.cos(2 * math.pi * i / seg) + z * math.sin(2 * math.pi * i / seg)) * rb_) for i in range(seg)]
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((ra[i], ra[j], rb[j], rb[i])).material_index = mi
    if caps:
        bm.faces.new(ra[::-1]).material_index = mi
        bm.faces.new(rb).material_index = mi


def lathe(bm, prof, mats, seg=24, c=(0, 0, 0), cap0=True, cap1=True, sx=1.0, sy=1.0):
    """Turns prof [(r, z), ...] round the z axis at c; mats: material per band (the last repeats)."""
    if isinstance(mats, int):
        mats = [mats]
    c = Vector(c)
    rings = [[bm.verts.new(c + Vector((max(r, 1e-4) * sx * math.cos(2 * math.pi * i / seg), max(r, 1e-4) * sy * math.sin(2 * math.pi * i / seg), z)))
              for i in range(seg)] for r, z in prof]
    for k in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i])).material_index = mats[min(k, len(mats) - 1)]
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
    """Box-projected UVs in metres; faces of the picture slots (DN_Sign_*) get 0..1 over their own extent."""
    uv = me.uv_layers.new(name="UVMap")
    pic = {i for i, m in enumerate(me.materials) if m and m.name.startswith("DN_Sign_")}
    for poly in me.polygons:
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


def export(bm, name, out=None, extra_mats=(), split_glass=True, smooth=None):
    """Writes <out>/<name>.glb (and <name>_Glass.glb with the DN_Glass faces when there are any); frees bm.
    smooth: material names to shade smooth (rounded things: china, food, vinyl)."""
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
    if smooth:
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
