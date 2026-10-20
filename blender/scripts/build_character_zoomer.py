"""Gen Z hero v2: oversized hoodie, wide jeans, chunky sneakers, detailed hands with fingers, hair clumps.

Builds blender/character_zoomer.blend and exports blender/export/character_zoomer.glb:
one skinned mesh (single bmesh, per-vertex weights) with a 60+ bone rig and two animations
  Zoomer_SitAtComputer_FP : first-person sit-down (head hidden, camera bone "eye"), then typing, 210 frames
  Zoomer_TypeIdle         : seated typing loop with the head visible, 48 frames
plus blender/export/zoomer_eye_track.json (the "eye" bone per frame, for the first-person camera).
Textures come from tools/fal_textures.py (textures/character/*.png).

Character space: metres, origin on the floor under the chair centre, faces -Y.

Run from the repo root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/scripts/build_character_zoomer.py
"""
import json
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TEX = os.path.join(ROOT, "textures", "character")
BLEND = os.path.join(ROOT, "blender", "character_zoomer.blend")
GLB = os.path.join(ROOT, "blender", "export", "character_zoomer.glb")
EYE_JSON = os.path.join(ROOT, "blender", "export", "zoomer_eye_track.json")

FPS = 24
HIP_Z_SIT = 0.565         # hip joint height when sitting (office chair seat top is 0.47)
KEYS_Y = -0.60            # keyboard centre in front of the chair (keyboard is 60 cm ahead)
KEYS_Z = 0.79             # key tops (table top 0.75)
EYE_HEAD = Vector((0.0, -0.075, 1.628))
FIRST_PERSON_END = 210

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.fps = FPS


# ------------------------------------------------------------------ materials
def image_mat(name, png, rough=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    tex = m.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(TEX, png))
    m.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def flat_mat(name, color, rough=0.4):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    m.diffuse_color = (*color, 1.0)
    return m


MAT_ORDER = ["skin", "hoodie", "denim", "shoes", "hair", "plastic", "sole", "lace", "dark", "eye"]
MATS = {
    "skin": image_mat("Zoomer_Skin", "face.png", 0.65),
    "hoodie": image_mat("Zoomer_Hoodie", "hoodie.png", 0.95),
    "denim": image_mat("Zoomer_Denim", "denim.png", 0.85),
    "shoes": image_mat("Zoomer_Sneakers", "sneakers.png", 0.55),
    "hair": image_mat("Zoomer_Hair", "hair.png", 0.55),
    "plastic": flat_mat("Zoomer_Earbuds", (0.92, 0.92, 0.94), 0.3),
    "sole": flat_mat("Zoomer_Sole", (0.88, 0.86, 0.80), 0.7),
    "lace": flat_mat("Zoomer_Laces", (0.93, 0.93, 0.9), 0.8),
    "dark": flat_mat("Zoomer_Dark", (0.05, 0.05, 0.06), 0.6),
    "eye": flat_mat("Zoomer_Eye", (0.93, 0.93, 0.93), 0.2),
}
MI = {k: i for i, k in enumerate(MAT_ORDER)}

# ------------------------------------------------------------------ skeleton
FINGER_Y = {"index": -0.031, "middle": -0.011, "ring": 0.010, "pinky": 0.029}
FINGER_LEN = {"index": (0.040, 0.025, 0.020), "middle": (0.044, 0.028, 0.021),
              "ring": (0.040, 0.026, 0.020), "pinky": (0.032, 0.020, 0.017), "thumb": (0.034, 0.030, 0.026)}
FINGER_R = {"index": (0.0092, 0.0078), "middle": (0.0095, 0.008), "ring": (0.009, 0.0075),
            "pinky": (0.0082, 0.0068), "thumb": (0.0112, 0.0085)}
FINGER_NAMES = ["thumb", "index", "middle", "ring", "pinky"]
KNUCKLE_Z = 0.752
WRIST_Z = 0.84


def finger_chain(name, sx):
    """Joint positions (4 points) and the unit direction of the first segment, rest pose, character space."""
    x0 = 0.31 * sx
    if name == "thumb":
        base = Vector((x0 - sx * 0.016, -0.038, 0.805))
        d = Vector((-sx * 0.30, -0.50, -0.81)).normalized()
        dirs = [d, (d + Vector((0, -0.1, -0.2))).normalized(), (d + Vector((0, -0.15, -0.3))).normalized()]
    else:
        base = Vector((x0, FINGER_Y[name], KNUCKLE_Z))
        dirs = [Vector((0, 0, -1))] * 3
    pts = [base]
    for ln, d in zip(FINGER_LEN[name], dirs):
        pts.append(pts[-1] + d * ln)
    return pts


BONES = [  # name, parent, head, tail, connected
    ("root", None, (0, 0, 0), (0, 0, 0.12), False),
    ("pelvis", "root", (0, 0, 0.95), (0, 0, 1.08), False),
    ("spine", "pelvis", (0, 0, 1.08), (0, 0, 1.24), True),
    ("chest", "spine", (0, 0, 1.24), (0, 0, 1.40), True),
    ("neck", "chest", (0, 0, 1.40), (0, 0, 1.48), True),
    ("head", "neck", (0, 0, 1.48), (0, 0, 1.76), True),
    ("head_vis", "head", (0, 0, 1.52), (0, 0, 1.60), False),   # scaled to ~0 to hide the head in first person
    ("eye", "head", tuple(EYE_HEAD), tuple(EYE_HEAD + Vector((0, -0.06, 0))), False),
]
for s, sx in ((".L", 1), (".R", -1)):
    BONES += [
        ("upper_arm" + s, "chest", (0.31 * sx, 0, 1.40), (0.31 * sx, 0, 1.10), False),
        ("forearm" + s, "upper_arm" + s, (0.31 * sx, 0, 1.10), (0.31 * sx, 0, WRIST_Z), True),
        ("hand" + s, "forearm" + s, (0.31 * sx, 0, WRIST_Z), (0.31 * sx, 0, KNUCKLE_Z), True),
        ("thigh" + s, "pelvis", (0.105 * sx, 0, 0.95), (0.105 * sx, 0, 0.50), False),
        ("shin" + s, "thigh" + s, (0.105 * sx, 0, 0.50), (0.105 * sx, 0, 0.10), True),
        ("foot" + s, "shin" + s, (0.105 * sx, 0, 0.10), (0.105 * sx, -0.18, 0.045), True),
    ]
    for fn in FINGER_NAMES:
        pts = finger_chain(fn, sx)
        for k in range(3):
            BONES.append((f"{fn}_0{k + 1}{s}", "hand" + s if k == 0 else f"{fn}_0{k}{s}",
                          tuple(pts[k]), tuple(pts[k + 1]), k > 0))
GI = {b[0]: i for i, b in enumerate(BONES)}


# ------------------------------------------------------------------ geometry builder
bm = bmesh.new()
UVL = bm.loops.layers.uv.new("UVMap")
DL = bm.verts.layers.deform.verify()


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3 - 2 * t)


def below(z, joint, h):
    """0 above the joint window, 1 below it."""
    return smooth((joint + h - z) / (2 * h))


def set_weights(v, ws):
    total = sum(ws.values()) or 1.0
    for bone, w in ws.items():
        if w > 1e-4:
            v[DL][GI[bone]] = w / total


def loft(rings, mat, weight, seg=16, tile=0.45, xf=None, caps=(True, True), uvfn=None):
    """Lofts ellipse rings along local +Z. ring = (z, rx, ry, cx, cy). xf positions the loft (rotation/translation only).
    weight(world co) -> {bone: w}. uvfn(world co) -> (u, v) overrides the tiled UV."""
    xf = xf or Matrix.Identity(4)
    grid = []
    for (z, rx, ry, cx, cy) in rings:
        row = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            co = xf @ Vector((cx + rx * math.cos(a), cy + ry * math.sin(a), z))
            v = bm.verts.new(co)
            set_weights(v, weight(co))
            row.append(v)
        grid.append(row)
    arc = [0.0]
    for k in range(1, len(rings)):
        dz = rings[k][0] - rings[k - 1][0]
        dr = (rings[k][1] + rings[k][2] - rings[k - 1][1] - rings[k - 1][2]) / 2
        arc.append(arc[-1] + math.hypot(dz, dr))
    circ = [2 * math.pi * math.sqrt((r[1] ** 2 + r[2] ** 2) / 2) for r in rings]

    def put(f, uvs):
        f.material_index = MI[mat]
        f.smooth = True
        for lp, uv in zip(f.loops, uvs):
            lp[UVL].uv = uvfn(lp.vert.co) if uvfn else (uv[0] / tile, uv[1] / tile)

    for k in range(len(rings) - 1):
        for i in range(seg):
            f = bm.faces.new([grid[k][i], grid[k][(i + 1) % seg], grid[k + 1][(i + 1) % seg], grid[k + 1][i]])
            put(f, [(i * circ[k] / seg, arc[k]), ((i + 1) * circ[k] / seg, arc[k]),
                    ((i + 1) * circ[k + 1] / seg, arc[k + 1]), (i * circ[k + 1] / seg, arc[k + 1])])
    for end, on in ((0, caps[0]), (-1, caps[1])):
        if not on:
            continue
        r = rings[end]
        co = xf @ Vector((r[3], r[4], r[0]))
        c = bm.verts.new(co)
        set_weights(c, weight(co))
        row = grid[end]
        for i in range(seg):
            tri = [c, row[i], row[(i + 1) % seg]] if end == -1 else [c, row[(i + 1) % seg], row[i]]
            f = bm.faces.new(tri)
            put(f, [(0, 0), (0, 0), (0, 0)])
    return grid


def align(p0, p1):
    d = (p1 - p0).normalized()
    return Matrix.Translation(p0) @ Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4()


def capsule(p0, p1, r0, r1, mat, weight, seg=8, tile=0.1, uvfn=None):
    L = (p1 - p0).length
    rings = [(-r0 * 0.7, r0 * 0.5, r0 * 0.5, 0, 0), (-r0 * 0.3, r0 * 0.88, r0 * 0.88, 0, 0), (0, r0, r0, 0, 0),
             (L, r1, r1, 0, 0), (L + r1 * 0.3, r1 * 0.88, r1 * 0.88, 0, 0), (L + r1 * 0.7, r1 * 0.5, r1 * 0.5, 0, 0)]
    return loft(rings, mat, weight, seg=seg, tile=tile, xf=align(p0, p1), uvfn=uvfn)


def tube(pts, radii, mat, weight, seg=6, uvfn=None, tile=0.12, flat=1.0):
    """Tapered tube through a polyline (parallel-transport frames); open ends capped by a fan."""
    n = len(pts)
    tans = []
    for i in range(n):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, n - 1)]
        tans.append((b - a).normalized())
    up = Vector((0, 0, 1)) if abs(tans[0].z) < 0.9 else Vector((1, 0, 0))
    nrm = tans[0].cross(up).normalized()
    grid, arc = [], [0.0]
    for i in range(n):
        if i:
            arc.append(arc[-1] + (pts[i] - pts[i - 1]).length)
            nrm = (nrm - tans[i] * nrm.dot(tans[i])).normalized()
        bin_ = tans[i].cross(nrm).normalized()
        row = []
        for k in range(seg):
            a = 2 * math.pi * k / seg
            co = pts[i] + (nrm * math.cos(a) + bin_ * math.sin(a) * flat) * radii[i]
            v = bm.verts.new(co)
            set_weights(v, weight(co))
            row.append(v)
        grid.append(row)
    for i in range(n - 1):
        for k in range(seg):
            f = bm.faces.new([grid[i][k], grid[i][(k + 1) % seg], grid[i + 1][(k + 1) % seg], grid[i + 1][k]])
            f.material_index = MI[mat]
            f.smooth = True
            uvs = [(k / seg, arc[i]), ((k + 1) / seg, arc[i]), ((k + 1) / seg, arc[i + 1]), (k / seg, arc[i + 1])]
            for lp, uv in zip(f.loops, uvs):
                lp[UVL].uv = uvfn(lp.vert.co) if uvfn else (uv[0] * 0.5, uv[1] / tile)
    # close the tip so the strand tapers to a point
    tip = bm.verts.new(pts[-1] + tans[-1] * radii[-1] * 0.5)
    set_weights(tip, weight(pts[-1]))
    for k in range(seg):
        f = bm.faces.new([tip, grid[-1][(k + 1) % seg], grid[-1][k]])
        f.material_index = MI[mat]
        f.smooth = True
        for lp in f.loops:
            lp[UVL].uv = (0.25, arc[-1] / tile)
    return grid


def slab(center, size, mat, weight, bevel=0.006, tile=0.3, uvfn=None):
    """Rounded box (pockets, tongue, straps), beveled in a scratch bmesh and copied over."""
    tb = bmesh.new()
    res = bmesh.ops.create_cube(tb, size=1.0)
    for v in res["verts"]:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(center)
    bmesh.ops.bevel(tb, geom=list(tb.edges), offset=bevel, segments=2, affect="EDGES")
    vmap = {}
    for v in tb.verts:
        nv = bm.verts.new(v.co)
        set_weights(nv, weight(v.co))
        vmap[v] = nv
    for f in tb.faces:
        nf = bm.faces.new([vmap[v] for v in f.verts])
        nf.material_index = MI[mat]
        nf.smooth = True
        for lp in nf.loops:
            c = lp.vert.co
            lp[UVL].uv = uvfn(c) if uvfn else ((c.x + c.y) / tile, c.z / tile)
    tb.free()


PLAIN = (0.04, 0.5)


# ------------------------------------------------------------------ body parts
def build_torso():
    def wt(c):
        b = below(c.z, 1.08, 0.06)
        top = smooth((c.z - 1.40) / 0.06)
        return {"pelvis": b, "spine": (1 - b) * below(c.z, 1.24, 0.06),
                "chest": (1 - below(c.z, 1.24, 0.06)) * (1 - top), "neck": (1 - below(c.z, 1.24, 0.06)) * top}
    loft([(0.82, 0.282, 0.192, 0, 0.0), (0.845, 0.29, 0.20, 0, 0.0),            # rib band
          (0.88, 0.272, 0.182, 0, 0.0), (0.95, 0.262, 0.172, 0, 0.0), (1.05, 0.262, 0.168, 0, 0.0),
          (1.14, 0.268, 0.168, 0, 0.0), (1.24, 0.272, 0.168, 0, 0.0), (1.33, 0.262, 0.16, 0, 0.0),
          (1.39, 0.235, 0.145, 0, 0.0), (1.425, 0.18, 0.12, 0, 0.0), (1.45, 0.12, 0.098, 0, 0.0)],
         "hoodie", wt, seg=28, tile=0.45)
    # kangaroo pocket with the two openings
    slab((0.0, -0.172, 1.0), (0.30, 0.03, 0.15), "hoodie", lambda c: {"spine": 0.6, "pelvis": 0.4}, bevel=0.012)
    for sx in (1, -1):
        slab((0.115 * sx, -0.186, 0.99), (0.012, 0.012, 0.11), "dark", lambda c: {"spine": 0.6, "pelvis": 0.4}, bevel=0.004)
    # drawstrings
    for sx in (1, -1):
        pts = [Vector((0.035 * sx, -0.150, 1.385)), Vector((0.04 * sx, -0.163, 1.30)), Vector((0.043 * sx, -0.168, 1.20)),
               Vector((0.045 * sx, -0.169, 1.14))]
        tube(pts, [0.0045, 0.0045, 0.0045, 0.0042], "lace", lambda c: {"chest": 1.0}, seg=6, tile=0.2)
        capsule(pts[-1], pts[-1] + Vector((0, 0, -0.012)), 0.0065, 0.0065, "lace", lambda c: {"chest": 1.0})
    # hood bunched behind the neck, plus the collar rim
    def hw(c):
        k = smooth((c.z - 1.40) / 0.1)
        return {"chest": 1 - k, "neck": k}
    loft([(1.26, 0.17, 0.06, 0, 0.15), (1.34, 0.20, 0.08, 0, 0.145), (1.42, 0.185, 0.095, 0, 0.12),
          (1.50, 0.15, 0.10, 0, 0.08), (1.55, 0.10, 0.08, 0, 0.05)], "hoodie", hw, seg=22, tile=0.45)
    loft([(1.415, 0.12, 0.10, 0, 0.0), (1.455, 0.105, 0.088, 0, 0.0), (1.50, 0.092, 0.078, 0, -0.002),
          (1.515, 0.098, 0.082, 0, -0.004)], "hoodie", lambda c: {"neck": 1.0}, seg=22, caps=(False, False))


def build_arm(sx, s):
    x0 = 0.31 * sx
    rings = [(1.45, 0.08, 0.085, 0.24 * sx, 0.0), (1.405, 0.095, 0.098, 0.285 * sx, 0.0),
             (1.30, 0.096, 0.096, x0, 0.0), (1.16, 0.085, 0.085, x0, 0.0), (1.10, 0.084, 0.084, x0, 0.0),
             (1.04, 0.080, 0.080, x0, 0.0), (0.93, 0.078, 0.078, x0, 0.0), (0.86, 0.078, 0.078, x0, 0.0),
             (0.815, 0.064, 0.064, x0, 0.0), (0.79, 0.060, 0.060, x0, 0.0)]       # last two: ribbed cuff
    loft(rings, "hoodie", lambda c: {"upper_arm" + s: 1 - below(c.z, 1.10, 0.07), "forearm" + s: below(c.z, 1.10, 0.07)},
         seg=18, tile=0.45, caps=(True, False))
    # hand: palm + fingers + thumb
    palm = [(0.862, 0.020, 0.036, x0, 0.0), (0.83, 0.022, 0.044, x0, 0.0), (0.79, 0.022, 0.048, x0, 0.0),
            (0.757, 0.018, 0.046, x0, 0.0)]
    loft(palm, "skin", lambda c: {"hand" + s: 1.0}, seg=14, uvfn=lambda c: PLAIN)
    for fn in FINGER_NAMES:
        pts = finger_chain(fn, sx)
        r0, r1 = FINGER_R[fn]
        for k in range(3):
            ra = r0 + (r1 - r0) * k / 3
            rb = r0 + (r1 - r0) * (k + 1) / 3
            bone = f"{fn}_0{k + 1}{s}"
            capsule(pts[k], pts[k + 1], ra, rb, "skin", lambda c, b=bone: {b: 1.0}, seg=8, uvfn=lambda c: PLAIN)
        # fingernail: a flat bump on the back of the fingertip is skipped on purpose; the tip stays round


def build_leg(sx, s):
    cx = 0.105 * sx
    rings = [(1.02, 0.14, 0.13, cx, 0.0), (0.985, 0.138, 0.128, cx, 0.0),             # waistband
             (0.93, 0.138, 0.128, cx, 0.0), (0.72, 0.136, 0.126, cx, 0.0), (0.56, 0.131, 0.121, cx, 0.0),
             (0.50, 0.133, 0.123, cx, 0.0), (0.44, 0.131, 0.121, cx, 0.0), (0.28, 0.129, 0.123, cx, 0.0),
             (0.17, 0.13, 0.128, cx + 0.002 * sx, 0.0), (0.115, 0.14, 0.14, cx + 0.004 * sx, -0.006),   # cuff folds
             (0.09, 0.128, 0.13, cx + 0.004 * sx, -0.006), (0.065, 0.142, 0.142, cx + 0.005 * sx, -0.01),
             (0.045, 0.14, 0.14, cx + 0.005 * sx, -0.01)]
    def wt(c):
        hip = smooth((c.z - 0.95) / 0.08)
        knee = below(c.z, 0.50, 0.08)
        return {"pelvis": hip, "thigh" + s: (1 - hip) * (1 - knee), "shin" + s: (1 - hip) * knee}
    loft(rings, "denim", wt, seg=20, tile=0.5, caps=(True, False))
    # back pocket and a visible thread seam strip down the outer side
    slab((cx, 0.135, 0.86), (0.11, 0.012, 0.12), "denim", lambda c: {"pelvis": 0.6, "thigh" + s: 0.4}, bevel=0.004, tile=0.5)


def build_shoe(sx, s):
    cx = 0.105 * sx
    xf = Matrix.Translation((cx, 0.095, 0.0)) @ Matrix.Rotation(math.radians(90), 4, "X")
    w = lambda c: {"foot" + s: 1.0}
    # upper (local z runs heel -> toe, local y is up)
    upper = [(0.0, 0.040, 0.044, 0, 0.082), (0.03, 0.054, 0.058, 0, 0.088), (0.10, 0.060, 0.060, 0, 0.088),
             (0.19, 0.060, 0.054, 0, 0.078), (0.26, 0.052, 0.044, 0, 0.066), (0.31, 0.036, 0.034, 0, 0.056),
             (0.325, 0.016, 0.02, 0, 0.05)]
    loft(upper, "shoes", w, seg=18, tile=0.14, xf=xf)
    # chunky sole
    sole = [(-0.004, 0.044, 0.02, 0, 0.022), (0.03, 0.058, 0.024, 0, 0.026), (0.10, 0.064, 0.026, 0, 0.026),
            (0.19, 0.064, 0.024, 0, 0.025), (0.26, 0.056, 0.022, 0, 0.024), (0.315, 0.04, 0.02, 0, 0.022),
            (0.33, 0.02, 0.014, 0, 0.022)]
    loft(sole, "sole", w, seg=18, tile=0.14, xf=xf)
    # tongue and laces across the instep
    slab((cx, -0.03, 0.15), (0.05, 0.03, 0.06), "shoes", w, bevel=0.01, tile=0.14)
    for k in range(4):
        y = -0.02 - 0.03 * k
        pts = [Vector((cx - 0.034, y + 0.0, 0.158 - k * 0.012)), Vector((cx, y - 0.008, 0.172 - k * 0.012)),
               Vector((cx + 0.034, y + 0.0, 0.158 - k * 0.012))]
        tube(pts, [0.0035, 0.0035, 0.0035], "lace", w, seg=6)
    # heel tab
    slab((cx, 0.112, 0.145), (0.03, 0.012, 0.03), "shoes", w, bevel=0.004, tile=0.14)


def head_ring_xy(z):
    """Head egg outline: (rx, ry, cy) per height."""
    pts = [(1.500, 0.040, 0.050, -0.012), (1.525, 0.072, 0.074, -0.008), (1.570, 0.094, 0.098, 0.0),
           (1.625, 0.104, 0.108, 0.003), (1.680, 0.100, 0.106, 0.005), (1.725, 0.080, 0.088, 0.005),
           (1.752, 0.045, 0.055, 0.005), (1.762, 0.01, 0.015, 0.005)]
    for a, b in zip(pts, pts[1:]):
        if a[0] <= z <= b[0]:
            t = (z - a[0]) / (b[0] - a[0])
            return tuple(a[i] + (b[i] - a[i]) * t for i in (1, 2, 3))
    return pts[-1][1:]


def build_head():
    wneck = lambda c: {"neck": 1 - smooth((c.z - 1.46) / 0.05), "head_vis": smooth((c.z - 1.46) / 0.05)}
    loft([(1.40, 0.052, 0.052, 0, 0.0), (1.47, 0.046, 0.048, 0, 0.0), (1.53, 0.046, 0.05, 0, -0.005)],
         "skin", wneck, seg=14, uvfn=lambda c: PLAIN)
    start = len(bm.verts)
    hw = lambda c: {"head_vis": 1.0}
    rings = [(1.500, 0.040, 0.050, 0, -0.012), (1.515, 0.062, 0.064, 0, -0.01), (1.535, 0.080, 0.082, 0, -0.006),
             (1.560, 0.091, 0.094, 0, -0.001), (1.590, 0.099, 0.102, 0, 0.002), (1.625, 0.104, 0.108, 0, 0.003),
             (1.655, 0.103, 0.108, 0, 0.004), (1.685, 0.099, 0.105, 0, 0.005), (1.715, 0.086, 0.094, 0, 0.005),
             (1.740, 0.062, 0.072, 0, 0.005), (1.756, 0.032, 0.040, 0, 0.005), (1.762, 0.008, 0.012, 0, 0.005)]

    def face_uv(c):
        if c.y > 0.0:
            return PLAIN
        return (0.5 + c.x / 0.263, 0.55 + (c.z - 1.625) / 0.263)
    loft(rings, "skin", hw, seg=36, uvfn=face_uv)
    # sculpt: brows, cheeks, lips, chin, eye sockets; done after the UVs so the texture stays put
    for v in list(bm.verts)[start:]:
        c = v.co
        if c.y > -0.02:
            continue
        front = smooth((-c.y - 0.02) / 0.07)
        def bump(cx, cz, rx, rz, amp):
            d = ((c.x - cx) / rx) ** 2 + ((c.z - cz) / rz) ** 2
            return amp * math.exp(-d)
        dy = 0.0
        dy -= bump(-0.032, 1.658, 0.032, 0.008, 0.006) + bump(0.032, 1.658, 0.032, 0.008, 0.006)      # brow ridge
        dy += bump(-0.034, 1.636, 0.022, 0.012, 0.004) + bump(0.034, 1.636, 0.022, 0.012, 0.004)      # sockets
        dy -= bump(-0.052, 1.58, 0.03, 0.02, 0.005) + bump(0.052, 1.58, 0.03, 0.02, 0.005)            # cheeks
        dy -= bump(0.0, 1.545, 0.024, 0.007, 0.006)                                                    # lips
        dy -= bump(0.0, 1.512, 0.022, 0.012, 0.005)                                                    # chin
        v.co.y += dy * front
    # nose: bridge, tip, nostrils
    nw = lambda c: {"head_vis": 1.0}
    loft([(1.600, 0.003, 0.003, 0, -0.1), (1.612, 0.008, 0.012, 0, -0.104), (1.600, 0.003, 0.003, 0, -0.1)][:2] +
         [(1.585, 0.011, 0.016, 0, -0.109), (1.578, 0.014, 0.012, 0, -0.110)], "skin", nw, seg=10, uvfn=lambda c: PLAIN) \
        if False else None
    loft([(1.574, 0.010, 0.008, 0, -0.1075), (1.585, 0.0085, 0.011, 0, -0.1085), (1.600, 0.006, 0.009, 0, -0.1055),
          (1.616, 0.004, 0.005, 0, -0.1025)], "skin", nw, seg=10, uvfn=lambda c: PLAIN)
    for sx in (1, -1):
        capsule(Vector((0.010 * sx, -0.112, 1.574)), Vector((0.011 * sx, -0.114, 1.571)), 0.0022, 0.002, "dark", nw, seg=6)
    # ears with a rim and the earbuds
    for sx in (1, -1):
        cx = 0.1045 * sx
        loft([(1.592, 0.004, 0.011, cx, 0.005), (1.612, 0.010, 0.020, cx + 0.002 * sx, 0.004),
              (1.632, 0.010, 0.019, cx + 0.002 * sx, 0.003), (1.648, 0.005, 0.012, cx, 0.003)], "skin", nw, seg=10,
             uvfn=lambda c: PLAIN)
        capsule(Vector((0.1135 * sx, -0.002, 1.618)), Vector((0.1145 * sx, -0.004, 1.612)), 0.0085, 0.0075, "plastic", nw, seg=10)
        capsule(Vector((0.1135 * sx, -0.003, 1.612)), Vector((0.1135 * sx, -0.005, 1.568)), 0.0032, 0.0030, "plastic", nw, seg=6)


def build_hair():
    rnd = random.Random(11)
    hw = lambda c: {"head_vis": 1.0}
    shell_rings = [(1.545, 0.095, 0.100, 0, 0.012), (1.57, 0.105, 0.110, 0, 0.012), (1.625, 0.116, 0.121, 0, 0.012),
                   (1.68, 0.113, 0.119, 0, 0.014), (1.725, 0.094, 0.103, 0, 0.014), (1.752, 0.064, 0.072, 0, 0.014),
                   (1.768, 0.03, 0.038, 0, 0.014), (1.774, 0.008, 0.012, 0, 0.014)]
    start = len(bm.verts)
    loft(shell_rings, "hair", hw, seg=36, uvfn=lambda c: (math.atan2(c.y, c.x) / math.pi * 1.2, c.z * 3.0), caps=(False, True))
    # cut the front: forehead and face stay open; the fringe is built from clumps
    kill = []
    for f in bm.faces:
        cm = f.calc_center_median()
        if f.material_index == MI["hair"] and ((cm.y < -0.03 and cm.z < 1.705) or (cm.y < 0.02 and cm.z < 1.60 and abs(cm.x) < 0.1)):
            kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    # strand clumps: roots on the scalp, falling outward and down with a bit of noise
    def root(az, z):
        rx, ry, cy = head_ring_xy(z)
        return Vector((math.cos(az) * (rx + 0.012), cy + math.sin(az) * (ry + 0.014), z))
    def clump(az, z0, length, curl, rad, drop=1.0, sway=0.0):
        p = root(az, z0)
        out = Vector((math.cos(az), math.sin(az), 0)).normalized()
        pts = [p]
        for i in range(1, 6):
            t = i / 5
            q = pts[-1] + out * (0.012 * (1 - t) * curl) + Vector((0, 0, -length / 5 * drop))
            q += Vector((sway * 0.004, rnd.uniform(-0.002, 0.002), 0))
            pts.append(q)
        tube(pts, [rad, rad * 1.1, rad * 1.0, rad * 0.8, rad * 0.55, rad * 0.35], "hair", hw, seg=6, tile=0.08, flat=0.4)
    # back and sides: layered, finer strands
    for k in range(95):
        az = math.radians(rnd.uniform(-40, 220))     # angle from +x through +y (back of the head)
        z = rnd.uniform(1.60, 1.73)
        clump(az, z, rnd.uniform(0.05, 0.12), rnd.uniform(0.8, 1.6), rnd.uniform(0.0055, 0.0085))
    for k in range(24):                               # nape
        az = math.radians(60 + k * 8)
        clump(az, 1.57, rnd.uniform(0.06, 0.10), 1.0, 0.0065)
    # curtain fringe over the forehead: many thin tapered strands
    for k in range(40):
        x = -0.088 + 0.176 * k / 39
        rx, ry, cy = head_ring_xy(1.72)
        p0 = Vector((x, cy - ry * math.sqrt(max(0.0, 1 - (x / rx) ** 2)) - 0.008, 1.725))
        sway = 0.02 * math.sin(k * 0.55) + x * 0.35 + rnd.uniform(-0.004, 0.004)
        ln = 0.07 + 0.02 * math.sin(k * 0.9) + rnd.uniform(0, 0.01)
        pts = [p0, p0 + Vector((sway * 0.3, -0.014, -ln * 0.25)), p0 + Vector((sway * 0.7, -0.022, -ln * 0.55)),
               p0 + Vector((sway * 1.0, -0.027, -ln * 0.8)), p0 + Vector((sway * 1.2, -0.029, -ln))]
        tube(pts, [0.0075, 0.0085, 0.008, 0.0062, 0.0030], "hair", hw, seg=6, tile=0.08, flat=0.35)
    for sx in (1, -1):                                # side locks in front of the ears
        for j in range(5):
            p0 = Vector((0.104 * sx, -0.05 + j * 0.012, 1.68))
            pts = [p0, p0 + Vector((0.008 * sx, -0.012, -0.03)), p0 + Vector((0.009 * sx, -0.015, -0.06)),
                   p0 + Vector((0.005 * sx, -0.013, -0.09))]
            tube(pts, [0.0085, 0.009, 0.007, 0.003], "hair", hw, seg=6, tile=0.08, flat=0.35)


def build_character():
    build_torso()
    for s, sx in ((".L", 1), (".R", -1)):
        build_arm(sx, s)
        build_leg(sx, s)
        build_shoe(sx, s)
    build_head()
    build_hair()
    me = bpy.data.meshes.new("Zoomer_Mesh")
    bm.to_mesh(me)
    for m in MAT_ORDER:
        me.materials.append(MATS[m])
    ob = bpy.data.objects.new("Zoomer_Mesh", me)
    scene.collection.objects.link(ob)
    for b in BONES:
        ob.vertex_groups.new(name=b[0])
    return ob


# ------------------------------------------------------------------ armature
def build_armature():
    data = bpy.data.armatures.new("Zoomer")
    arm = bpy.data.objects.new("Zoomer_Armature", data)
    scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail, conn in BONES:
        b = data.edit_bones.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = data.edit_bones[parent]
            b.use_connect = conn
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


# ------------------------------------------------------------------ posing
def pitch(deg):
    a = math.radians(deg)
    return Vector((0, -math.sin(a), math.cos(a)))


def solve2(root, target, l1, l2, pole):
    """Two-bone IK: returns the middle joint position."""
    d = target - root
    dist = min(max(d.length, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
    dirn = d.normalized()
    a = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = math.sqrt(max(l1 * l1 - a * a, 0))
    side = (pole - dirn * pole.dot(dirn))
    side = side.normalized() if side.length > 1e-6 else Vector((0, 0, 1))
    return root + dirn * a + side * h


class Rig:
    def __init__(self, arm):
        self.arm = arm
        self.pb = arm.pose.bones
        self.prev_q = {}
        self.curl_axis = {}
        for s, sx in ((".L", 1), (".R", -1)):
            for fn in FINGER_NAMES:
                pts = finger_chain(fn, sx)
                d = (pts[1] - pts[0]).normalized()
                n = Vector((-sx, 0, 0))
                self.curl_axis[fn + s] = d.cross(n).normalized()       # world axis; + angle curls towards the palm

    def update(self):
        bpy.context.view_layer.update()

    def head_of(self, pb):
        if pb.parent is None:
            return pb.bone.head_local.copy()
        rel = pb.parent.bone.matrix_local.inverted() @ pb.bone.head_local
        return pb.parent.matrix @ rel

    def aim(self, name, d, twist=0.0, head=None, normal=None, target_normal=None):
        pb = self.pb[name]
        rest = pb.bone.matrix_local.to_3x3()
        d = d.normalized()
        y0 = rest @ Vector((0, 1, 0))
        R = y0.rotation_difference(d).to_matrix() @ rest
        if normal is not None:
            n = R @ normal
            tn = target_normal - d * target_normal.dot(d)
            n = n - d * n.dot(d)
            ang = math.atan2(d.dot(n.cross(tn)), n.dot(tn))
            R = Matrix.Rotation(ang, 3, d) @ R
        if twist:
            R = Matrix.Rotation(twist, 3, d) @ R
        pb.matrix = Matrix.Translation(head if head is not None else self.head_of(pb)) @ R.to_4x4()
        self.update()

    def curl(self, fn, s, angles):
        """angles: radians for the three phalanges, + curls towards the palm (axes in the rest frame of each bone)."""
        for k, ang in enumerate(angles):
            pb = self.pb[f"{fn}_0{k + 1}{s}"]
            axis = pb.bone.matrix_local.to_3x3().inverted() @ self.curl_axis[fn + s]
            pb.rotation_quaternion = Matrix.Rotation(ang, 3, axis).to_quaternion()
        self.update()

    def key(self, frame, hide_head=False):
        for pb in self.pb:
            q = pb.rotation_quaternion
            p = self.prev_q.get(pb.name)
            if p is not None and q.dot(p) < 0:
                q.negate()
            self.prev_q[pb.name] = q.copy()
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            if pb.name in ("pelvis", "root"):
                pb.keyframe_insert("location", frame=frame)
        hv = self.pb["head_vis"]
        k = 0.001 if hide_head else 1.0
        hv.scale = (k, k, k)
        hv.keyframe_insert("scale", frame=frame)

    def eye(self):
        pb = self.pb["eye"]
        m = pb.matrix
        fwd = m.to_3x3() @ Vector((0, 1, 0))
        up = m.to_3x3() @ (pb.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, 1)))
        return m.translation.copy(), fwd, up


def lerp(a, b, t):
    return a + (b - a) * t


def pose_character(rig, p):
    """p: dict of pose parameters, all in character space."""
    rig.aim("root", Vector((0, 0, 1)))
    rig.aim("pelvis", pitch(p["pelvis_pitch"]), head=p["pelvis"])
    rig.aim("spine", pitch(p["spine_pitch"]))
    rig.aim("chest", pitch(p["chest_pitch"]))
    rig.aim("neck", pitch(p["neck_pitch"]))
    rig.aim("head", pitch(p["head_pitch"]), twist=p.get("head_yaw", 0.0))
    for s, sx in ((".L", 1), (".R", -1)):
        sh = rig.head_of(rig.pb["upper_arm" + s])
        wrist = Vector(p["wrist" + s])
        elbow = solve2(sh, wrist, 0.30, 0.26, Vector((0.5 * sx, 0.9, -0.3)))
        rig.aim("upper_arm" + s, elbow - sh)
        rig.aim("forearm" + s, wrist - elbow)
        rig.aim("hand" + s, Vector(p["hand_dir" + s]),
                normal=rig.pb["hand" + s].bone.matrix_local.to_3x3().inverted() @ Vector((-sx, 0, 0)),
                target_normal=Vector(p["palm" + s]))
        for fn in FINGER_NAMES:
            base, tap = p["fingers" + s][fn]
            rig.curl(fn, s, [math.radians(base * 1.0 + tap), math.radians(base * 1.1 + tap * 0.6), math.radians(base * 0.7 + tap * 0.4)])
        hip = rig.head_of(rig.pb["thigh" + s])
        ankle = Vector(p["ankle" + s])
        knee = solve2(hip, ankle, 0.45, 0.40, Vector((0, -1, 0.15)))
        rig.aim("thigh" + s, knee - hip)
        rig.aim("shin" + s, ankle - knee)
        rig.aim("foot" + s, Vector(p["toe_dir" + s]))


def relaxed_fingers(extra=0.0):
    return {fn: (14.0 + extra + (6 if fn == "pinky" else 0), 0.0) for fn in FINGER_NAMES}


def typing_fingers(t, side_phase=0.0):
    out = {}
    for i, fn in enumerate(FINGER_NAMES):
        ph = 2 * math.pi * (2 * t) + i * 1.25 + side_phase
        tap = 9.0 * max(0.0, math.sin(ph))
        base = 8.0 if fn == "thumb" else 26.0 + 3 * i
        out[fn] = (base, tap)
    return out


def stand_pose(px=0.0, py=0.0, head_yaw=0.0):
    return dict(
        pelvis=Vector((px, py, 0.95)), pelvis_pitch=0, spine_pitch=0, chest_pitch=0, neck_pitch=0, head_pitch=0,
        head_yaw=head_yaw,
        **{"wrist.L": (px + 0.315, py - 0.01, 0.86), "wrist.R": (px - 0.315, py - 0.01, 0.86),
           "hand_dir.L": Vector((0, -0.05, -1)), "hand_dir.R": Vector((0, -0.05, -1)),
           "palm.L": (-1, 0, 0), "palm.R": (1, 0, 0),
           "fingers.L": relaxed_fingers(), "fingers.R": relaxed_fingers(),
           "ankle.L": (px + 0.105, py, 0.10), "ankle.R": (px - 0.105, py, 0.10),
           "toe_dir.L": Vector((0, -0.97, -0.25)), "toe_dir.R": Vector((0, -0.97, -0.25))})


def sit_pose(t=0.0, head_pitch=9.0):
    """Seated, hunched towards the keyboard, hands on the keys. t is the typing phase (0..1 loops)."""
    bob = math.sin(t * 2 * math.pi * 2)
    bob2 = math.sin(t * 2 * math.pi * 2 + 2.2)
    return dict(
        pelvis=Vector((0, 0.09, HIP_Z_SIT + 0.003 * math.sin(t * 2 * math.pi))), pelvis_pitch=-4, spine_pitch=12, chest_pitch=20,
        neck_pitch=28, head_pitch=head_pitch + 0.8 * bob2, head_yaw=0.0,
        **{"wrist.L": (0.15, KEYS_Y + 0.09, KEYS_Z + 0.045 + 0.003 * max(bob, 0)),
           "wrist.R": (-0.15, KEYS_Y + 0.09, KEYS_Z + 0.045 + 0.003 * max(bob2, 0)),
           "hand_dir.L": Vector((-0.30, -1, -0.12)), "hand_dir.R": Vector((0.30, -1, -0.12)),
           "palm.L": (0, 0, -1), "palm.R": (0, 0, -1),
           "fingers.L": typing_fingers(t), "fingers.R": typing_fingers(t, 2.4),
           "ankle.L": (0.115, -0.46, 0.10), "ankle.R": (-0.115, -0.46, 0.10),
           "toe_dir.L": Vector((0, -0.97, -0.25)), "toe_dir.R": Vector((0, -0.97, -0.25))})


def blend(a, b, t):
    out = {}
    for k in a:
        va, vb = a[k], b[k]
        if isinstance(va, Vector):
            out[k] = va.lerp(vb, t)
        elif isinstance(va, tuple):
            out[k] = tuple(lerp(x, y, t) for x, y in zip(va, vb))
        elif isinstance(va, dict):
            out[k] = {fk: (lerp(va[fk][0], vb[fk][0], t), lerp(va[fk][1], vb[fk][1], t)) for fk in va}
        else:
            out[k] = lerp(va, vb, t)
    return out


def new_action(rig, name):
    ad = rig.arm.animation_data_create()
    act = bpy.data.actions.new(name)
    ad.action = act
    rig.prev_q.clear()
    for pb in rig.pb:
        pb.rotation_mode = "QUATERNION"
    return act


def lift(p, side, amount):
    a = list(p["ankle" + side])
    a[2] += amount
    p["ankle" + side] = tuple(a)


def anim_sit_first_person(rig):
    """Standing on the sitter's right (-X) looking at the desk, a side step, sit down, hands out to the keyboard, typing."""
    act = new_action(rig, "Zoomer_SitAtComputer_FP")
    stand = stand_pose(-0.50, -0.02)
    stand["head_pitch"] = 4
    over = stand_pose(0.0, 0.09)
    over["head_pitch"] = 6
    drop = sit_pose(0.0, head_pitch=32)
    drop["wrist.L"] = (0.20, -0.20, 0.78)
    drop["wrist.R"] = (-0.20, -0.20, 0.78)
    drop["hand_dir.L"] = Vector((0, -0.3, -1))
    drop["hand_dir.R"] = Vector((0, -0.3, -1))
    drop["palm.L"] = (-1, 0, 0)
    drop["palm.R"] = (1, 0, 0)
    drop["fingers.L"] = relaxed_fingers(6)
    drop["fingers.R"] = relaxed_fingers(6)
    reach = sit_pose(0.0, head_pitch=44)
    reach["wrist.L"] = (0.12, KEYS_Y + 0.20, KEYS_Z + 0.14)
    reach["wrist.R"] = (-0.12, KEYS_Y + 0.20, KEYS_Z + 0.14)
    reach["palm.L"] = (-0.5, 0, -0.85)
    reach["palm.R"] = (0.5, 0, -0.85)
    reach["fingers.L"] = relaxed_fingers(2)
    reach["fingers.R"] = relaxed_fingers(2)
    T = [(0, stand), (8, stand), (30, over), (54, drop), (74, reach)]
    track = {}
    for f in range(0, FIRST_PERSON_END + 1, 2):
        if f <= 74:
            for (f0, p0), (f1, p1) in zip(T, T[1:]):
                if f0 <= f <= f1:
                    p = blend(p0, p1, smooth((f - f0) / max(f1 - f0, 1)))
                    break
            if 8 <= f <= 30:
                lift(p, ".R", 0.10 * math.sin((f - 8) / 22 * math.pi))
        elif f <= 100:       # settle onto the keys, looking up at the screen
            k = smooth((f - 74) / 26)
            p = blend(reach, sit_pose(0.0, head_pitch=34), k)
        else:                # typing: watches his hands first, then looks up at the screen
            hp = 34 - 24 * smooth((f - 120) / 30)
            p = sit_pose(((f - 100) % 48) / 48.0, head_pitch=hp)
        pose_character(rig, p)
        rig.key(f, hide_head=True)
        pos, fwd, up = rig.eye()
        track[f] = {"pos": list(pos), "fwd": list(fwd), "up": list(up)}
    act.use_fake_user = True
    with open(EYE_JSON, "w") as fh:
        json.dump({"fps": FPS, "frames": track}, fh)
    return act


def anim_type_idle(rig):
    act = new_action(rig, "Zoomer_TypeIdle")
    for f in range(0, 49, 2):
        pose_character(rig, sit_pose((f % 48) / 48.0))
        rig.key(f)
    act.use_fake_user = True
    return act


# ------------------------------------------------------------------ main
def main():
    body = build_character()
    arm = build_armature()
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    rig = Rig(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    anim_sit_first_person(rig)
    idle = anim_type_idle(rig)
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.animation_data.action = idle
    os.makedirs(os.path.dirname(GLB), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=GLB, export_format="GLB", use_selection=True, export_animations=True,
                              export_animation_mode="ACTIONS", export_skins=True, export_apply=False,
                              export_yup=True, export_image_format="AUTO")
    print("done:", GLB, "verts:", len(body.data.vertices), "bones:", len(BONES))


main()
