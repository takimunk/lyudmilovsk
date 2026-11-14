"""Topi's props (docs/topi.md), game-ready with baked textures: the enamel warning plate on its rusted pipe post, the Ural's
hand-painted board on driftwood posts, three beach towels (cloth simulated on uneven sand), four split stakes with rags
(cloth in the wind).

  Blender -b --factory-startup --python blender/topi/props.py [-- quick]       (Blender 5.x; bpy as a module works too)
  needs tools/make_topi_signs.py first (the signs' wear layers textures/topi/{warn,ural}_wear*.png)
  -> blender/topi/out/SM_Topi_{WarnSign,UralSign,Towel_A,Towel_B,Towel_C,Stake,Stake_A..D}.glb
     textures/topi/baked/<mesh>_<slot>_{A,N,ORM}.png       (albedo sRGB with alpha, normal DirectX green, ORM = AO rough metal)
     blender/topi/out/preview.jpg                            (every prop from three sides with its baked textures)
  `quick` bakes at a quarter of the size (for trying things).

Frame: metres, origin at the ground point, a sign's front faces Blender -y (exported Y-up: Unreal +Y, map north at yaw 0;
ULyudTopiProps turns them to face the town). Material slots keep their names (unreal/scripts/import_topi.py reads them):
TP_Metal (the warning sign's body: pipe, footing, plate back, bolts), TP_SignWarn (the plate's front, 0..1 UVs: the localized
picture from tools/make_topi_signs.py lies under the baked wear layer), TP_Wood (the Ural board's posts, back, edges, nails;
the stakes), TP_SignUral (the boards' fronts, 0..1 over both boards), TP_Towel, TP_Rag. Each slot of each mesh gets its own
baked texture set (an atlas from smart UV projection; the picture slots keep their 0..1 layout). Wear on the sign fronts is a
separate layer (tools/make_topi_signs.py: *_wear.png RGBA colour + coverage, *_wear_n.png, *_wear_rm.png) so that it lies
over the text of every language; here only the fronts' ambient occlusion is baked for them.

Every mesh is checked for UV overlap per slot (UVCHECK lines) and rendered from three sides into the preview.
"""
import math
import os
import random
import sys

import bpy  # noqa: F401  (first: it registers bmesh)
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(HERE, "out")
TEX_OUT = os.path.join(ROOT, "textures", "topi", "baked")
TEX_IN = os.path.join(ROOT, "textures", "topi")
LOC = os.path.join(ROOT, "textures", "localized", "ru")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
QUICK = "quick" in ARGS
# `nobake`: rebuild and export the meshes only (the baked textures stay as they are; everything here is seeded)
NOBAKE = "nobake" in ARGS
# names after -- (WarnSign, UralSign, Stake_A.., Towel_A..) build only those
ONLY = {a for a in ARGS if a not in ("quick", "nobake")}


def wanted(name):
    return not ONLY or name.replace("SM_Topi_", "") in ONLY
SIGN_PX = 512 if QUICK else 2048
SMALL_PX = 256 if QUICK else 1024


# ================================================================================================ geometry helpers

class Builder:
    """One mesh object: a bmesh with a UV layer, a face attribute `part` (which kind of surface, for the shaders) and an
    ordered list of material slots."""

    def __init__(self, name, slots):
        self.name = name
        self.slots = list(slots)
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.part = self.bm.faces.layers.float.new("part")

    def face(self, pts, slot, uvs=None, part=0, smooth=True):
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.material_index = self.slots.index(slot)
        f.smooth = smooth
        f[self.part] = float(part)
        for k, loop in enumerate(f.loops):
            loop[self.uv].uv = uvs[k] if uvs else (0.0, 0.0)
        return f

    def tube(self, rings, slot, part, cap_top=True, cap_bottom=True, smooth=True):
        """Rings of points (each counter-clockwise seen from above), joined into a closed tube."""
        n = len(rings[0])
        for r in range(len(rings) - 1):
            for k in range(n):
                k2 = (k + 1) % n
                self.face([rings[r][k], rings[r][k2], rings[r + 1][k2], rings[r + 1][k]], slot, part=part, smooth=smooth)
        if cap_top:
            self.face(list(rings[-1]), slot, part=part, smooth=False)
        if cap_bottom:
            self.face(list(reversed(rings[0])), slot, part=part, smooth=False)

    def box(self, centre, size, slot, part, rot=None):
        c = Vector(centre)
        h = Vector(size) * 0.5
        m = rot or Matrix.Identity(3)
        corners = [c + m @ Vector(((h.x if i & 1 else -h.x), (h.y if i & 2 else -h.y), (h.z if i & 4 else -h.z))) for i in range(8)]
        for fi in ((1, 3, 7, 5), (0, 4, 6, 2), (2, 6, 7, 3), (0, 1, 5, 4), (4, 5, 7, 6), (0, 2, 3, 1)):
            self.face([corners[i] for i in fi], slot, part=part, smooth=False)

    def slab(self, nx, ny, front, back, slot_front, slot_back, uv_front, part_back=0):
        """A thin solid over a grid: front(i, j)/back(i, j) Vectors; the front's normal is d/di x d/dj."""
        F = [[front(i, j) for j in range(ny + 1)] for i in range(nx + 1)]
        B = [[back(i, j) for j in range(ny + 1)] for i in range(nx + 1)]
        for i in range(nx):
            for j in range(ny):
                q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                self.face([F[a][b] for a, b in q], slot_front, [uv_front(a, b) for a, b in q])
                self.face([B[a][b] for a, b in reversed(q)], slot_back, part=part_back)
        rim = [(i, 0) for i in range(nx)] + [(nx, j) for j in range(ny)] + [(i, ny) for i in range(nx, 0, -1)] + [(0, j) for j in range(ny, 0, -1)]
        for k in range(len(rim)):
            a, b = rim[k], rim[(k + 1) % len(rim)]
            pts = [F[a[0]][a[1]], F[b[0]][b[1]], B[b[0]][b[1]], B[a[0]][a[1]]]
            self.face(list(reversed(pts)), slot_back, part=part_back, smooth=False)

    def build(self):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-6)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def ring(z, radius, sides, rng=None, jitter=0.0, centre=(0.0, 0.0), phase=0.0, shape=None):
    pts = []
    for k in range(sides):
        a = 2 * math.pi * k / sides + phase
        r = radius * (shape(a) if shape else 1.0) * (1 + (rng.uniform(-jitter, jitter) if rng else 0.0))
        pts.append(Vector((centre[0] + r * math.cos(a), centre[1] + r * math.sin(a), z)))
    return pts


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def recalc(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()


def select_only(*objs):
    bpy.context.view_layer.update()
    for o in list(bpy.context.view_layer.objects):
        if o is not None:
            o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[-1]


def evaluated_copy(obj, name):
    """A new mesh object with all of obj's modifiers (and its cloth frame) applied."""
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(obj.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
    me.name = name
    new = bpy.data.objects.new(name, me)
    new.matrix_world = obj.matrix_world
    bpy.context.scene.collection.objects.link(new)
    return new


def smart_uv(obj, slots, margin=0.012):
    """Smart UV projection of the faces in the given slots (the others keep their UVs)."""
    select_only(obj)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bm = bmesh.from_edit_mesh(obj.data)
    idx = [obj.data.materials.find(s) for s in slots]
    for f in bm.faces:
        f.select = f.material_index in idx
    bmesh.update_edit_mesh(obj.data)
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    try:
        bpy.ops.uv.pack_islands(margin=0.004, rotate=True, shape_method="CONCAVE")
    except Exception as e:  # noqa: BLE001
        print("TOPI pack_islands: %s" % e)
    bpy.ops.object.mode_set(mode="OBJECT")
    # long thin parts (pipes, stakes) pack into a strip: stretch the slot's islands to fill the square (the textures are
    # baked for these UVs, so an uneven texel density along and across is fine)
    uvd = obj.data.uv_layers["UVMap"].data
    loops = [li for p in obj.data.polygons if p.material_index in idx for li in p.loop_indices]
    us = [uvd[li].uv[0] for li in loops]
    vs = [uvd[li].uv[1] for li in loops]
    u0, u1, v0, v1 = min(us), max(us), min(vs), max(vs)
    m = 0.01
    for li in loops:
        u, v = uvd[li].uv
        uvd[li].uv = (m + (u - u0) / max(u1 - u0, 1e-6) * (1 - 2 * m), m + (v - v0) / max(v1 - v0, 1e-6) * (1 - 2 * m))


# ================================================================================================ shader helpers

class Nodes:
    """Small wiring helpers over a material's node tree."""

    def __init__(self, mat):
        self.t = mat.node_tree
        self.n = self.t.nodes
        self.l = self.t.links

    def new(self, kind, **props):
        nd = self.n.new(kind)
        for k, v in props.items():
            setattr(nd, k, v)
        return nd

    def link(self, a, b):
        self.l.new(a, b)
        return b

    def val(self, v):
        nd = self.new("ShaderNodeValue")
        nd.outputs[0].default_value = v
        return nd.outputs[0]

    def rgb(self, c):
        nd = self.new("ShaderNodeRGB")
        nd.outputs[0].default_value = (c[0], c[1], c[2], 1.0)
        return nd.outputs[0]

    def _in(self, sock, v):
        if isinstance(v, (int, float)):
            sock.default_value = v
        elif isinstance(v, tuple):
            sock.default_value = v if len(v) == len(sock.default_value) else tuple(v) + (1.0,)
        else:
            self.link(v, sock)

    def math(self, op, a, b=0.0, clamp=False):
        nd = self.new("ShaderNodeMath", operation=op, use_clamp=clamp)
        self._in(nd.inputs[0], a)
        self._in(nd.inputs[1], b)
        return nd.outputs[0]

    def vmath(self, op, a, b=(0.0, 0.0, 0.0), scale=None):
        nd = self.new("ShaderNodeVectorMath", operation=op)
        self._in(nd.inputs[0], a)
        self._in(nd.inputs[1], b)
        if scale is not None:
            self._in(nd.inputs[3], scale)
        return nd.outputs["Vector"] if op not in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else nd.outputs["Value"]

    def smooth(self, x, a, b):
        """smoothstep(a, b, x) for floats (a > b gives the falling edge)."""
        nd = self.new("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP", clamp=True)
        self._in(nd.inputs["Value"], x)
        lo, hi = (a, b) if a < b else (b, a)
        nd.inputs["From Min"].default_value = lo
        nd.inputs["From Max"].default_value = hi
        nd.inputs["To Min"].default_value = 0.0 if a < b else 1.0
        nd.inputs["To Max"].default_value = 1.0 if a < b else 0.0
        return nd.outputs["Result"]

    def mix(self, fac, a, b):
        """Colour lerp."""
        nd = self.new("ShaderNodeMix", data_type="RGBA", blend_type="MIX", clamp_factor=True)
        self._in(nd.inputs[0], fac)
        self._in(nd.inputs[6], a)
        self._in(nd.inputs[7], b)
        return nd.outputs[2]

    def mixf(self, fac, a, b):
        nd = self.new("ShaderNodeMix", data_type="FLOAT", clamp_factor=True)
        self._in(nd.inputs[0], fac)
        self._in(nd.inputs[2], a)
        self._in(nd.inputs[3], b)
        return nd.outputs[0]

    def noise(self, vec, scale, detail=4.0, rough=0.55, dims="3D", out="Fac"):
        nd = self.new("ShaderNodeTexNoise", noise_dimensions=dims)
        if vec is not None:
            self._in(nd.inputs["Vector"], vec)
        nd.inputs["Scale"].default_value = scale
        nd.inputs["Detail"].default_value = detail
        nd.inputs["Roughness"].default_value = rough
        return nd.outputs[out]

    def voronoi(self, vec, scale, feature="F1", out="Distance", rand=1.0):
        nd = self.new("ShaderNodeTexVoronoi", feature=feature)
        if vec is not None:
            self._in(nd.inputs["Vector"], vec)
        nd.inputs["Scale"].default_value = scale
        nd.inputs["Randomness"].default_value = rand
        return nd.outputs[out]

    def pos(self):
        return self.new("ShaderNodeNewGeometry").outputs["Position"]

    def uv(self):
        return self.new("ShaderNodeUVMap", uv_map="UVMap").outputs["UV"]

    def sep(self, v):
        nd = self.new("ShaderNodeSeparateXYZ")
        self.link(v, nd.inputs[0])
        return nd.outputs

    def attr(self, name):
        return self.new("ShaderNodeAttribute", attribute_name=name).outputs["Fac"]

    def scale(self, v, s):
        return self.vmath("MULTIPLY", v, s)

    def image(self, path, non_color=False):
        img = bpy.data.images.load(path, check_existing=True)
        if non_color:
            img.colorspace_settings.name = "Non-Color"
        nd = self.new("ShaderNodeTexImage", image=img)
        return nd

    def is_part(self, part_sock, k):
        """1 where the face attribute `part` is k."""
        return self.math("LESS_THAN", self.math("ABSOLUTE", self.math("SUBTRACT", part_sock, float(k))), 0.5)


def new_material(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    for nd in list(m.node_tree.nodes):
        if nd.type != "OUTPUT_MATERIAL":
            m.node_tree.nodes.remove(nd)
    return m


def finish_material(m, N, albedo, rough, metal, height=None, bump=0.002, alpha=None):
    """Wires a Principled BSDF and remembers the sources the bakes read (m['bake'] keeps socket paths)."""
    b = N.new("ShaderNodeBsdfPrincipled")
    N._in(b.inputs["Base Color"], albedo)
    N._in(b.inputs["Roughness"], rough)
    N._in(b.inputs["Metallic"], metal)
    if alpha is not None:
        N._in(b.inputs["Alpha"], alpha)
    if height is not None:
        bp = N.new("ShaderNodeBump")
        bp.inputs["Distance"].default_value = bump
        bp.inputs["Strength"].default_value = 1.0
        N._in(bp.inputs["Height"], height)
        N.link(bp.outputs["Normal"], b.inputs["Normal"])
    out = next(nd for nd in m.node_tree.nodes if nd.type == "OUTPUT_MATERIAL")
    N.link(b.outputs[0], out.inputs["Surface"])
    BAKE_SRC[m.name] = {"albedo": _src(albedo, N), "rough": _src(rough, N), "metal": _src(metal, N)}
    return m


BAKE_SRC = {}


def _src(v, N):
    """A socket for a bake source (constants become Value nodes)."""
    if isinstance(v, (int, float)):
        return N.val(float(v))
    if isinstance(v, tuple):
        return N.rgb(v)
    return v


# ================================================================================================ materials

def mat_warn_body():
    """Rusted galvanized pipe, a concrete footing half out of the sand, the plate's painted back and edges, rusty bolts."""
    m = new_material("TP_Metal")
    N = Nodes(m)
    p = N.pos()
    part = N.attr("part")
    z = N.sep(p)[2]
    # rust: patches of orange-brown over darker scale, running down
    streak = N.noise(N.vmath("MULTIPLY", p, (9.0, 9.0, 1.2)), 3.0, 6.0, 0.6)
    patch = N.noise(p, 7.0, 8.0, 0.62)
    rust_k = N.smooth(N.math("ADD", N.math("MULTIPLY", patch, 0.7), N.math("MULTIPLY", streak, 0.45)), 0.48, 0.78)
    rust = N.mix(N.noise(p, 60.0, 3.0, 0.5), (0.20, 0.07, 0.025), (0.42, 0.16, 0.05))
    zinc = N.mix(N.noise(p, 25.0, 4.0, 0.5), (0.30, 0.30, 0.29), (0.45, 0.45, 0.43))
    pipe_col = N.mix(rust_k, zinc, rust)
    # concrete: grey with pits and aggregate, sand dust where it meets the ground, rust run off the pipe
    pits = N.voronoi(p, 90.0)
    agg = N.noise(p, 30.0, 6.0, 0.7)
    conc = N.mix(agg, (0.33, 0.32, 0.30), (0.52, 0.50, 0.46))
    conc = N.mix(N.math("MULTIPLY", N.smooth(pits, 0.08, 0.0), 0.6), conc, (0.18, 0.17, 0.16))
    dust = N.math("MULTIPLY", N.smooth(z, 0.10, -0.02), N.smooth(N.noise(p, 12.0), 0.3, 0.7))
    conc = N.mix(dust, conc, (0.62, 0.55, 0.42))
    near_pipe = N.smooth(N.vmath("LENGTH", N.vmath("MULTIPLY", p, (1.0, 1.0, 0.0))), 0.09, 0.035)
    conc = N.mix(N.math("MULTIPLY", near_pipe, 0.6), conc, (0.30, 0.13, 0.05))
    # the plate's back: grey-green paint flaking to rust
    back_k = N.smooth(N.noise(p, 14.0, 6.0, 0.6), 0.55, 0.68)
    back = N.mix(back_k, N.mix(N.noise(p, 40.0), (0.22, 0.25, 0.22), (0.27, 0.30, 0.27)), rust)
    # bolts: rust over steel
    bolt = N.mix(N.noise(p, 300.0), (0.18, 0.08, 0.03), (0.33, 0.15, 0.06))
    is_c, is_b, is_bolt = N.is_part(part, 1), N.is_part(part, 2), N.is_part(part, 3)
    col = N.mix(is_c, pipe_col, conc)
    col = N.mix(is_b, col, back)
    col = N.mix(is_bolt, col, bolt)
    rough = N.mixf(is_c, N.mixf(rust_k, 0.55, 0.85), 0.93)
    rough = N.mixf(is_b, rough, N.mixf(back_k, 0.6, 0.85))
    metal = N.mixf(is_c, N.mixf(rust_k, 0.75, 0.15), 0.0)
    metal = N.mixf(is_b, metal, N.mixf(back_k, 0.0, 0.2))
    metal = N.mixf(is_bolt, metal, 0.35)
    h = N.math("ADD", N.math("MULTIPLY", rust_k, N.noise(p, 120.0, 4.0, 0.6)), N.math("MULTIPLY", is_c, N.math("MULTIPLY", N.smooth(pits, 0.1, 0.0), -0.8)))
    h = N.math("ADD", h, N.math("MULTIPLY", is_b, back_k))
    return finish_material(m, N, col, rough, metal, h, bump=0.0015)


def mat_picture(slot, picture, wear):
    """A sign's front for the bakes (only its ambient occlusion is baked) and the preview: the picture under the wear layer."""
    m = new_material(slot)
    N = Nodes(m)
    pic = N.image(picture)
    w = N.image(wear)
    col = N.mix(w.outputs["Alpha"], pic.outputs["Color"], w.outputs["Color"])
    return finish_material(m, N, col, 0.5, 0.0)


def mat_wood():
    """The Ural sign's driftwood posts, the boards' backs and edges, the nails; and the split stakes (bark, raw split wood)."""
    m = new_material("TP_Wood")
    N = Nodes(m)
    p = N.pos()
    part = N.attr("part")
    # grain runs along the local length: z for posts and stakes, x for boards
    along_z = N.vmath("MULTIPLY", p, (14.0, 14.0, 0.7))
    along_x = N.vmath("MULTIPLY", p, (0.7, 14.0, 14.0))
    is_board = N.is_part(part, 5)
    is_nail = N.is_part(part, 6)
    is_bark = N.is_part(part, 7)
    is_split = N.is_part(part, 8)
    gvec = N.new("ShaderNodeMix", data_type="VECTOR", clamp_factor=True)
    N.link(is_board, gvec.inputs[0])
    N.link(along_z, gvec.inputs[4])
    N.link(along_x, gvec.inputs[5])
    g = gvec.outputs[1]
    grain = N.noise(g, 6.0, 10.0, 0.7)
    rings = N.math("SINE", N.math("MULTIPLY", N.noise(g, 2.0, 3.0, 0.5), 60.0))
    # g already squeezes the grain direction ~20x: cracks run along it
    cracks = N.smooth(N.voronoi(g, 4.0, "DISTANCE_TO_EDGE"), 0.018, 0.0)
    # weathered grey driftwood (posts, board backs), fresher brown split wood for the stakes
    grey = N.mix(grain, (0.30, 0.29, 0.26), (0.52, 0.50, 0.45))
    grey = N.mix(N.math("MULTIPLY", N.math("ADD", rings, 1.0), 0.15), grey, (0.40, 0.38, 0.34))
    split = N.mix(grain, (0.38, 0.27, 0.16), (0.58, 0.45, 0.30))
    split = N.mix(N.math("MULTIPLY", N.smooth(N.sep(p)[2], 0.05, -0.05), 0.7), split, (0.22, 0.18, 0.13))   # dirt below the sand line
    bark = N.mix(N.noise(N.vmath("MULTIPLY", p, (30.0, 30.0, 4.0)), 3.0, 8.0, 0.7), (0.10, 0.08, 0.06), (0.27, 0.22, 0.17))
    col = N.mix(is_split, grey, split)
    col = N.mix(is_bark, col, bark)
    col = N.mix(N.math("MULTIPLY", cracks, 0.85), col, (0.05, 0.04, 0.035))
    nail = N.mix(N.noise(p, 200.0), (0.15, 0.06, 0.02), (0.30, 0.14, 0.05))
    col = N.mix(is_nail, col, nail)
    rough = N.mixf(is_nail, N.mixf(is_bark, 0.88, 0.95), 0.7)
    metal = N.mixf(is_nail, 0.0, 0.4)
    bark_h = N.noise(N.vmath("MULTIPLY", p, (60.0, 60.0, 6.0)), 2.0, 8.0, 0.75)
    h = N.math("SUBTRACT", N.math("MULTIPLY", grain, 0.35), N.math("MULTIPLY", cracks, 1.0))
    h = N.mixf(is_bark, h, N.math("MULTIPLY", bark_h, 1.4))
    return finish_material(m, N, col, rough, metal, h, bump=0.0025)


TOWEL_LOOK = {   # ground colour, stripe colour, second stripe colour, print
    "A": ((0.80, 0.82, 0.84), (0.10, 0.25, 0.55), (0.85, 0.75, 0.35), False),
    "B": ((0.66, 0.24, 0.16), (0.93, 0.88, 0.76), (0.20, 0.22, 0.38), False),
    "C": ((0.40, 0.56, 0.38), (0.88, 0.72, 0.18), (0.20, 0.34, 0.22), True),
}


def mat_towel(letter):
    """Terry cloth: stripes across both ends (or a faded print), loops, faded in the sun, sand in the low parts."""
    m = new_material("TP_Towel_" + letter)
    N = Nodes(m)
    ground, stripe, stripe2, printed = TOWEL_LOOK[letter]
    uvs = N.sep(N.uv())
    u, v = uvs[0], uvs[1]
    ends = N.math("MINIMUM", v, N.math("SUBTRACT", 1.0, v))           # 0 at the short ends
    band = lambda a, b: N.math("MULTIPLY", N.smooth(ends, a - 0.004, a), N.smooth(ends, b + 0.004, b))   # 1 for a < ends < b  # noqa: E731
    s1 = N.math("ADD", band(0.06, 0.10), band(0.135, 0.15))
    s2 = band(0.115, 0.125)
    col = N.mix(s1, ground, stripe)
    col = N.mix(s2, col, stripe2)
    if printed:
        # a faded sun in the middle: rays round a disc
        du = N.math("MULTIPLY", N.math("SUBTRACT", u, 0.5), 2.0)
        dv = N.math("MULTIPLY", N.math("SUBTRACT", v, 0.5), 4.0)
        r = N.math("SQRT", N.math("ADD", N.math("MULTIPLY", du, du), N.math("MULTIPLY", dv, dv)))
        ang = N.math("ARCTAN2", dv, du)
        rays = N.math("MULTIPLY", N.smooth(N.math("SINE", N.math("MULTIPLY", ang, 12.0)), 0.3, 0.6), N.smooth(r, 0.85, 0.75))
        disc = N.smooth(r, 0.42, 0.38)
        sun = N.math("MAXIMUM", disc, N.math("MULTIPLY", rays, N.smooth(r, 0.45, 0.5)))
        col = N.mix(N.math("MULTIPLY", sun, 0.65), col, (0.95, 0.62, 0.20))
    # sun fade: blotchy toward a washed-out pale
    fade = N.smooth(N.noise(N.uv(), 3.0, 3.0, 0.5), 0.35, 0.75)
    col = N.mix(N.math("MULTIPLY", fade, 0.35), col, (0.88, 0.86, 0.80))
    # terry loops: fine speckle
    loops = N.noise(N.vmath("MULTIPLY", N.uv(), (1.0, 2.0, 1.0)), 420.0, 2.0, 0.5)
    col = N.mix(N.math("MULTIPLY", N.math("SUBTRACT", loops, 0.5), 0.5), col, (0.05, 0.05, 0.05))
    # sand: in the low parts (near the ground) and in clumps
    z = N.sep(N.pos())[2]
    sand_k = N.math("MULTIPLY", N.smooth(z, 0.035, 0.0), N.smooth(N.noise(N.pos(), 9.0, 5.0, 0.6), 0.35, 0.65))
    sand_k = N.math("ADD", sand_k, N.math("MULTIPLY", N.smooth(N.noise(N.pos(), 40.0, 4.0, 0.6), 0.68, 0.8), 0.6))
    sand_k = N.math("MINIMUM", sand_k, 1.0)
    col = N.mix(N.math("MULTIPLY", sand_k, 0.85), col, (0.66, 0.58, 0.44))
    h = N.math("ADD", loops, N.math("MULTIPLY", sand_k, 0.5))
    return finish_material(m, N, col, N.mixf(sand_k, 0.93, 0.98), 0.0, h, bump=0.0012)


def mat_rag():
    """A torn, sun-bleached red rag."""
    m = new_material("TP_Rag")
    N = Nodes(m)
    uv = N.uv()
    weave = N.noise(N.vmath("MULTIPLY", uv, (1.0, 4.0, 1.0)), 180.0, 2.0, 0.5)
    bleach = N.smooth(N.noise(uv, 4.0, 4.0, 0.6), 0.3, 0.8)
    col = N.mix(bleach, (0.45, 0.07, 0.05), (0.78, 0.52, 0.45))
    col = N.mix(N.math("MULTIPLY", N.math("SUBTRACT", weave, 0.5), 0.6), col, (0.08, 0.04, 0.03))
    dirt = N.smooth(N.sep(uv)[1], 0.75, 1.0)
    col = N.mix(N.math("MULTIPLY", dirt, 0.4), col, (0.35, 0.30, 0.24))
    return finish_material(m, N, col, 0.95, 0.0, weave, bump=0.0008)


# ================================================================================================ the warning sign

def warn_sign():
    rng = random.Random(11)
    b = Builder("SM_Topi_WarnSign", ["TP_Metal", "TP_SignWarn"])
    lean = Matrix.Rotation(math.radians(1.6), 3, "X") @ Matrix.Rotation(math.radians(-0.8), 3, "Y")
    # the footing: a rough concrete lump, 0.25 m in the sand, ~0.12 m out of it, its top crumbled
    rings = []
    for zz, rr in ((-0.25, 0.15), (-0.05, 0.17), (0.06, 0.165), (0.12, 0.14)):
        rings.append(ring(zz, rr, 14, rng, 0.08, phase=0.2))
    for p in rings[-1]:
        p.z += rng.uniform(-0.025, 0.01)
    b.tube(rings, "TP_Metal", 1)
    # the pipe: 60 mm, from inside the footing to 2.25 m, leaning a little; a welded cap
    R = 0.030
    pipe = []
    for zz in (0.05, 0.6, 1.2, 1.8, 2.25):
        pipe.append([lean @ p for p in ring(zz, R, 12, rng, 0.02)])
    b.tube(pipe, "TP_Metal", 0, cap_bottom=False)
    top = lean @ Vector((0, 0, 2.25))
    cap = [lean @ p for p in ring(2.25, R * 1.12, 12)]
    b.tube([cap, [Vector((q.x, q.y, q.z + 0.012)) for q in cap]], "TP_Metal", 0, cap_bottom=True)
    # the plate: 70 x 50 cm steel, 2.5 mm; top right corner bent toward the viewer, a dent, bowed; on two flat bars
    W, H, T = 0.70, 0.50, 0.0025
    NX, NY = 28, 20
    zc = 1.78
    pipe_at = lean @ Vector((0, 0, zc))
    yf = pipe_at.y - R - 0.008 - T
    wob = [[rng.uniform(-0.0006, 0.0006) for _ in range(NY + 1)] for _ in range(NX + 1)]

    def plate_front(i, j):
        u, v = i / NX, j / NY
        x, z = pipe_at.x + (u - 0.5) * W, zc + (v - 0.5) * H
        c = smoothstep(0.80, 1.0, 0.55 * u + 0.45 * v + 0.1 * (u - v) * 0) if (u > 0.6 and v > 0.6) else 0.0
        bend = 0.05 * c ** 1.6                                        # the corner folded toward the viewer
        bow = 0.005 * (1 - ((u - 0.5) * 2) ** 2)
        dent = 0.012 * math.exp(-(((u - 0.30) / 0.10) ** 2 + ((v - 0.36) / 0.12) ** 2))
        dent2 = 0.006 * math.exp(-(((u - 0.74) / 0.06) ** 2 + ((v - 0.16) / 0.09) ** 2))
        edge = 0.004 * (1 - smoothstep(0.0, 0.06, v))                 # the lower edge kicked back by a boot
        return Vector((x, yf - bow - bend + dent + dent2 + edge + wob[i][j], z - 0.010 * c))

    def plate_back(i, j):
        p = plate_front(i, j)
        return Vector((p.x, p.y + T, p.z))
    b.slab(NX, NY, plate_front, plate_back, "TP_SignWarn", "TP_Metal", lambda i, j: (i / NX, j / NY), part_back=2)
    # two flat bars behind it, clamped to the pipe with U-bolts
    for dz in (-0.17, 0.17):
        b.box((pipe_at.x, yf + T + 0.004, zc + dz), (0.62, 0.008, 0.035), "TP_Metal", 2)
        band = [Vector((pipe_at.x + (R + 0.004) * math.cos(a), pipe_at.y + (R + 0.004) * math.sin(a), zc + dz)) for a in [2 * math.pi * k / 10 for k in range(10)]]
        b.tube([[q + Vector((0, 0, -0.008)) for q in band], [q + Vector((0, 0, 0.008)) for q in band]], "TP_Metal", 3, cap_top=False, cap_bottom=False)
    # four bolt heads on the front, hex, a little proud
    for su in (0.07, 0.93):
        for sv in (0.10, 0.90):
            i, j = su * NX, sv * NY
            p = plate_front(int(round(i)), int(round(j)))
            hexa = [Vector((p.x + 0.009 * math.cos(a), p.y, p.z + 0.009 * math.sin(a))) for a in [math.pi / 6 + k * math.pi / 3 for k in range(6)]]
            outer = [Vector((q.x, q.y - 0.006, q.z)) for q in hexa]
            # rings in the x-z plane: the tube helper wants rings round z, so build faces here
            for k in range(6):
                k2 = (k + 1) % 6
                b.face([hexa[k], hexa[k2], outer[k2], outer[k]], "TP_Metal", part=3, smooth=False)
            b.face(list(reversed(outer)), "TP_Metal", part=3, smooth=False)
    return b.build()


# ================================================================================================ the Ural sign

def ural_sign():
    rng = random.Random(23)
    b = Builder("SM_Topi_UralSign", ["TP_Wood", "TP_SignUral"])
    # two grey driftwood posts: crooked, knobbly, 0.4 m in the sand
    for sx, top, seed in ((-1, 1.42, 1), (1, 1.33, 2)):
        r2 = random.Random(seed)
        rings = []
        bend = Vector((r2.uniform(-0.03, 0.03), r2.uniform(-0.02, 0.02), 0))
        for k, zz in enumerate([-0.40, -0.1, 0.2, 0.5, 0.8, 1.05, 1.25, top]):
            t = (zz + 0.4) / (top + 0.4)
            c = Vector((sx * 0.48, 0.0, 0)) + bend * math.sin(t * math.pi) + Vector((0.01 * math.sin(k * 1.7 + seed), 0.008 * math.cos(k * 2.3), 0))
            rad = 0.045 * (1.0 - 0.25 * t) * (1 + 0.12 * math.sin(k * 2.1 + seed))
            rings.append(ring(zz, rad, 9, r2, 0.12, centre=(c.x, c.y), phase=seed))
        for p in rings[-1]:
            p.z += r2.uniform(-0.03, 0.02)
        b.tube(rings, "TP_Wood", 4)
    # two boards nailed across the posts' front; the lower one has lost a nail and hangs askew; the text spans both
    W, Hb, gap, T = 1.12, 0.17, 0.008, 0.025
    H = 2 * Hb + gap
    zc = 1.06
    yf = -0.05 - T
    NX = 16
    for which, (v0, v1) in enumerate(((0.0, Hb / H), ((Hb + gap) / H, 1.0))):
        lower = which == 0
        tilt = math.radians(-3.0) if lower else math.radians(0.6)
        pivot = Vector((0.5 * W - 0.04, 0, 0)) if lower else Vector((0, 0, 0))     # the lower one hangs from its right nail
        warp = [[rng.uniform(-1, 1) for _ in range(3)] for _ in range(NX + 1)]

        def front(i, j, v0=v0, v1=v1, tilt=tilt, pivot=pivot, warp=warp):
            u = i / NX
            v = v0 + (v1 - v0) * j / 2
            x, z = (u - 0.5) * W, (v - 0.5) * H
            # broken, uneven ends and a slight cup across the board
            if i in (0, NX):
                z += 0.004 * warp[i][j]
                x += 0.006 * warp[i][(j + 1) % 3]
            y = yf + 0.003 * math.sin(u * 3.0 + which) + (0.002 if j == 1 else 0.0) + 0.0015 * warp[i][j]
            q = Vector((x, 0, z)) - pivot
            q = Vector((q.x * math.cos(tilt) - q.z * math.sin(tilt), 0, q.x * math.sin(tilt) + q.z * math.cos(tilt))) + pivot
            return Vector((q.x, y, zc + q.z))

        def back(i, j, front=front):
            p = front(i, j)
            return Vector((p.x, p.y + T, p.z))
        b.slab(NX, 2, front, back, "TP_SignUral", "TP_Wood",
               lambda i, j, v0=v0, v1=v1: (i / NX, v0 + (v1 - v0) * j / 2), part_back=5)
        # nails: two at each end, one missing on the lower board's left
        for su in (0.035, 0.965):
            for sv in (0.3, 0.7):
                if lower and su < 0.5:
                    continue
                p = front(int(round(su * NX)), 1)
                p = Vector((p.x, p.y - 0.001, p.z + (sv - 0.5) * 0.08))
                b.box(p, (0.010, 0.006, 0.010), "TP_Wood", 6)
    return b.build()


# ================================================================================================ stakes

STAKES = {   # section, radius m, tilt deg (x, y), bark?, crack, rag height, seed
    "A": ("quarter", 0.050, (4.0, -2.0), True, 1.0, 1.08, 1),
    "B": ("half", 0.040, (-2.5, 3.0), False, 0.6, 1.15, 2),
    "C": ("plank", 0.032, (6.0, 1.0), False, 1.2, 1.00, 3),
    "D": ("quarter", 0.060, (-1.0, -5.0), True, 0.8, 1.12, 4),
}


def stake_section(kind, r):
    """The cross-section of a split stake (counter-clockwise from above); and which edges are bark (outer arc)."""
    if kind == "quarter":
        pts = [(0.0, 0.0)] + [(r * math.cos(a), r * math.sin(a)) for a in np.linspace(0.0, math.pi / 2, 6)]
        bark = [False] + [True] * 5 + [False]
    elif kind == "half":
        pts = [(r * math.cos(a), r * math.sin(a)) for a in np.linspace(0.0, math.pi, 7)]
        bark = [True] * 6 + [False]
    else:
        pts = [(-r, -r * 0.45), (r, -r * 0.40), (r * 1.05, r * 0.42), (-r * 0.95, r * 0.47)]
        bark = [False] * 4
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    return [(x - cx, y - cy) for x, y in pts], bark


def stake(letter):
    kind, r, tilt, has_bark, crack, rag_z, seed = STAKES[letter]
    rng = random.Random(seed)
    b = Builder("SM_Topi_Stake_" + letter, ["TP_Wood", "TP_Rag"])
    sec, bark = stake_section(kind, r)
    rot = Matrix.Rotation(math.radians(tilt[0]), 3, "X") @ Matrix.Rotation(math.radians(tilt[1]), 3, "Y")
    rings = []
    zs = [-0.30, -0.20, -0.08, 0.0, 0.35, 0.7, 1.0, 1.25]
    for k, zz in enumerate(zs):
        taper = 0.15 if zz < -0.25 else (0.55 if zz < -0.15 else 1.0)
        twist = 0.15 * (zz + 0.3)
        rr = []
        for (x, y) in sec:
            xr = x * math.cos(twist) - y * math.sin(twist)
            yr = x * math.sin(twist) + y * math.cos(twist)
            j = 1 + rng.uniform(-0.06, 0.06)
            top_cut = 0.04 * xr / r if k == len(zs) - 1 else 0.0       # chopped aslant
            rr.append(rot @ Vector((xr * taper * j, yr * taper * j, zz + top_cut + (rng.uniform(-0.01, 0.01) if k == len(zs) - 1 else 0.0))))
        rings.append(rr)
    n = len(sec)
    for ri in range(len(rings) - 1):
        for k in range(n):
            k2 = (k + 1) % n
            part = 7 if (has_bark and bark[k] and bark[k2] and 0.0 <= zs[ri] < 1.0 and rng.random() < 0.8) else 8
            b.face([rings[ri][k], rings[ri][k2], rings[ri + 1][k2], rings[ri + 1][k]], "TP_Wood", part=part)
    b.face(list(rings[-1]), "TP_Wood", part=8, smooth=False)
    b.face(list(reversed(rings[0])), "TP_Wood", part=8, smooth=False)
    obj = b.build()
    obj["rag_z"] = rag_z
    obj["rag_seed"] = seed
    obj["tilt"] = tilt
    obj["radius"] = r
    return obj


def rag_cloth(stake_obj):
    """A rag tied round the stake: a strip whose first part is pinned round the stake, the rest blown by the wind (cloth sim)."""
    seed = stake_obj["rag_seed"]
    rng = random.Random(seed + 100)
    zr = stake_obj["rag_z"]
    tilt = stake_obj["tilt"]
    rot = Matrix.Rotation(math.radians(tilt[0]), 3, "X") @ Matrix.Rotation(math.radians(tilt[1]), 3, "Y")
    rad = stake_obj["radius"] * 0.95 + 0.004
    width = rng.uniform(0.035, 0.05)
    tail = rng.uniform(0.28, 0.42)
    wrap = 2 * math.pi * rad * 1.05
    L = wrap + tail
    NU, NV = 4, 30
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    dl = bm.verts.layers.deform.verify()
    verts = []
    start = rng.uniform(0, 2 * math.pi)
    for j in range(NV + 1):
        s = L * j / NV
        row = []
        for i in range(NU + 1):
            h = (i / NU - 0.5) * width
            if s <= wrap:
                a = start + s / rad
                p = Vector((rad * math.cos(a), rad * math.sin(a), h))
            else:
                # the tail: hangs from where the wrap ends, its width across (along the stake's tangent there), a little
                # away from the stake and twisted so the wind can take it
                a = start + wrap / rad
                t = s - wrap
                tangent = Vector((-math.sin(a), math.cos(a), 0))
                outward = Vector((math.cos(a), math.sin(a), 0))
                tw = 0.6 * t / tail
                across = tangent * math.cos(tw) + outward * math.sin(tw)
                p = outward * (rad + 0.012 + 0.25 * t) + across * h * 1.0 + Vector((0, 0, -0.5 * width - 0.85 * t))
            v = bm.verts.new(rot @ (p + Vector((0, 0, zr))))
            v[dl][0] = 1.0 if s <= wrap + 0.004 else 0.0
            row.append(v)
        verts.append(row)
    for j in range(NV):
        for i in range(NU):
            f = bm.faces.new([verts[j][i], verts[j][i + 1], verts[j + 1][i + 1], verts[j + 1][i]])
            f.smooth = True
            for k, (a, c) in enumerate(((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                f.loops[k][uvl].uv = (a / NU, c / NV)
    me = bpy.data.meshes.new("rag")
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new("rag_" + stake_obj.name, me)
    bpy.context.scene.collection.objects.link(obj)
    obj.vertex_groups.new(name="pin")             # index 0: the weights came through the deform layer
    col = stake_obj.modifiers.new("collide", "COLLISION")
    col.settings.thickness_outer = 0.004
    cl = obj.modifiers.new("cloth", "CLOTH")
    s = cl.settings
    s.quality = 8
    s.mass = 0.05
    s.tension_stiffness = 8
    s.compression_stiffness = 8
    s.bending_stiffness = 0.05
    s.air_damping = 2.0
    s.vertex_group_mass = "pin"
    cl.collision_settings.distance_min = 0.003
    cl.point_cache.frame_start = 1
    cl.point_cache.frame_end = 60
    bpy.ops.object.effector_add(type="WIND", location=(0.0, 0.0, 0.0))
    wind = bpy.context.active_object
    wind.field.strength = rng.uniform(3.0, 4.5)
    wind.field.noise = 0.6
    wind.field.seed = seed
    wind.rotation_euler = (0.0, math.radians(90), math.radians(rng.uniform(-30, 30)))     # blowing toward +x (east, down the wind)
    frame = 40 + seed * 3
    for f in range(1, frame + 1):
        bpy.context.scene.frame_set(f)
    out = evaluated_copy(obj, "rag_done_" + stake_obj.name)
    bpy.context.scene.frame_set(1)
    for o in (obj, wind):
        bpy.data.objects.remove(o)
    stake_obj.modifiers.remove(col)
    out.data.materials.clear()
    return out


# ================================================================================================ towels

def sand_collider(seed):
    """Uneven sand to drop cloth on: wind ripples and lumps, a few cm."""
    rng = random.Random(seed)
    bm = bmesh.new()
    n = 70
    size = 3.0
    grid = []
    ph = [rng.uniform(0, 6.28) for _ in range(6)]
    for j in range(n + 1):
        row = []
        for i in range(n + 1):
            x, y = (i / n - 0.5) * size, (j / n - 0.5) * size
            z = 0.012 * math.sin(x * 22 + 3 * math.sin(y * 4 + ph[0])) + 0.025 * math.sin(x * 3.1 + ph[1]) * math.sin(y * 2.3 + ph[2]) \
                + 0.015 * math.sin(x * 7.3 + y * 5.1 + ph[3])
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for j in range(n):
        for i in range(n):
            bm.faces.new([grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]])
    me = bpy.data.meshes.new("sand")
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new("sand_%d" % seed, me)
    bpy.context.scene.collection.objects.link(obj)
    c = obj.modifiers.new("collide", "COLLISION")
    c.settings.thickness_outer = 0.003
    c.settings.cloth_friction = 15.0
    return obj


def towel(letter):
    """A towel dropped on the sand. A: flat with a folded corner; B: crumpled in a heap; C: twisted, half buried."""
    LEN, WID = 1.6, 0.8
    NU, NV = 40, 80
    rng = random.Random(ord(letter))
    sand = sand_collider(ord(letter))
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rows = []
    for j in range(NV + 1):
        row = []
        for i in range(NU + 1):
            row.append(bm.verts.new(((i / NU - 0.5) * WID, (j / NV - 0.5) * LEN, 0.0)))
        rows.append(row)
    for j in range(NV):
        for i in range(NU):
            f = bm.faces.new([rows[j][i], rows[j][i + 1], rows[j + 1][i + 1], rows[j + 1][i]])
            f.smooth = True
            for k, (a, c) in enumerate(((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                f.loops[k][uvl].uv = (a / NU, c / NV)
    me = bpy.data.meshes.new("towel_" + letter)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new("towel_sim_" + letter, me)
    bpy.context.scene.collection.objects.link(obj)
    # the start shape is a shape key over the flat rest shape (the cloth keeps the flat towel's lengths)
    obj.shape_key_add(name="Basis")
    start = obj.shape_key_add(name="start")
    start.value = 1.0
    helper = None
    for k, v in enumerate(me.vertices):
        x, y = v.co.x, v.co.y
        if letter == "A":
            n = Vector((0.6, 1.0, 0)).normalized()
            s = (x - 0.10) * n.x + (y - 0.48) * n.y
            p = Vector((x, y, 0.06))
            if s > 0:
                p = p - n * (2 * s)
                p.z = 0.06 + 0.012 + 0.008 * math.exp(-s * 40)
            start.data[k].co = p
        elif letter == "B":
            start.data[k].co = Vector((x, y, 0.75 + 0.02 * math.sin(x * 9 + y * 7)))
        else:
            a = math.pi * 1.1 * max(0.0, y / LEN + 0.5) ** 1.3
            start.data[k].co = Vector((x * math.cos(a), y, 0.12 + x * math.sin(a) + 0.04 * math.sin(y * 5)))
    if letter == "B":
        # a ball under it that sinks away once the towel has draped over it: the towel collapses into a heap
        sm = bpy.data.meshes.new("ball")
        bm2 = bmesh.new()
        bmesh.ops.create_uvsphere(bm2, u_segments=24, v_segments=12, radius=0.28)
        bm2.to_mesh(sm)
        bm2.free()
        helper = bpy.data.objects.new("ball", sm)
        bpy.context.scene.collection.objects.link(helper)
        hc = helper.modifiers.new("collide", "COLLISION")
        hc.settings.thickness_outer = 0.004
        hc.settings.cloth_friction = 30.0
        helper.location = (0.05, 0.1, 0.22)
        helper.keyframe_insert("location", frame=40)
        helper.location = (0.05, 0.1, -0.40)
        helper.keyframe_insert("location", frame=70)
        helper.scale = (1, 1.4, 0.9)
    cl = obj.modifiers.new("cloth", "CLOTH")
    s = cl.settings
    s.quality = 6
    s.mass = 0.35
    s.tension_stiffness = 12
    s.compression_stiffness = 12
    s.shear_stiffness = 6
    s.bending_stiffness = 0.25
    s.air_damping = 1.5
    s.rest_shape_key = obj.data.shape_keys.key_blocks["Basis"]
    cc = cl.collision_settings
    cc.use_self_collision = True
    cc.self_distance_min = 0.004
    cc.distance_min = 0.004
    cc.self_friction = 8.0
    end = {"A": 45, "B": 110, "C": 60}[letter]
    cl.point_cache.frame_start = 1
    cl.point_cache.frame_end = end
    for f in range(1, end + 1):
        bpy.context.scene.frame_set(f)
    high = evaluated_copy(obj, "SM_Topi_Towel_%s_high" % letter)
    bpy.context.scene.frame_set(1)
    bpy.data.objects.remove(obj)
    if helper:
        bpy.data.objects.remove(helper)
    if high.data.shape_keys:
        high.shape_key_clear()
    # the lowest point just under the sand line (the ground's z = 0)
    zmin = min(v.co.z for v in high.data.vertices)
    for v in high.data.vertices:
        v.co.z -= zmin + 0.008
        if letter == "C":                     # half buried: its far half goes under the sand
            v.co.z -= 0.07 * smoothstep(-0.1, 0.5, v.co.y)
    high.data.update()
    sand.location.z = -zmin - 0.008
    sand_z = sand
    # the low poly: decimated
    low = evaluated_copy(high, "SM_Topi_Towel_" + letter)
    target = {"A": 2400, "B": 3000, "C": 2600}[letter]
    dec = low.modifiers.new("dec", "DECIMATE")
    dec.ratio = target / max(1, len(low.data.polygons) * 2)
    dec.use_collapse_triangulate = True
    low2 = evaluated_copy(low, "SM_Topi_Towel_" + letter + "_lo")
    bpy.data.objects.remove(low)
    low2.name = "SM_Topi_Towel_" + letter
    for d in low2.data.uv_layers["UVMap"].data:          # the decimation can nudge border UVs a hair outside 0..1
        d.uv = (min(max(d.uv[0], 0.0), 1.0), min(max(d.uv[1], 0.0), 1.0))
    low2.data.name = low2.name
    # the high poly for the normal bake: subdivided once
    sub = high.modifiers.new("sub", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1
    high2 = evaluated_copy(high, high.name + "2")
    bpy.data.objects.remove(high)
    high2.name = "SM_Topi_Towel_%s_high" % letter
    for o in (low2, high2):
        o.data.materials.clear()
        o.data.materials.append(MATS["TP_Towel_" + letter])
        for poly in o.data.polygons:
            poly.use_smooth = True
    return low2, high2, sand_z


# ================================================================================================ baking

def set_render_for_bake():
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 16
    sc.cycles.use_denoising = False
    sc.render.bake.margin = 8
    sc.render.bake.margin_type = "EXTEND"


def make_image(name, px, non_color):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, px, px, alpha=True, float_buffer=False)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    img.generated_color = (0.5, 0.5, 1.0, 1.0) if non_color else (0, 0, 0, 1)
    return img


def set_target(mat, img):
    N = Nodes(mat)
    nd = next((x for x in mat.node_tree.nodes if x.type == "TEX_IMAGE" and x.label == "BAKE"), None)
    if nd is None:
        nd = N.new("ShaderNodeTexImage")
        nd.label = "BAKE"
    nd.image = img
    for x in mat.node_tree.nodes:
        x.select = False
    nd.select = True
    mat.node_tree.nodes.active = nd


def emit_from(mat, sock):
    """Points the material's output at an emission of sock; returns how to restore it."""
    t = mat.node_tree
    out = next(nd for nd in t.nodes if nd.type == "OUTPUT_MATERIAL")
    old = out.inputs["Surface"].links[0].from_socket if out.inputs["Surface"].links else None
    em = t.nodes.new("ShaderNodeEmission")
    if sock.type in ("RGBA", "VECTOR"):
        t.links.new(sock, em.inputs["Color"])
    else:
        cv = t.nodes.new("ShaderNodeCombineColor")
        for i in range(3):
            t.links.new(sock, cv.inputs[i])
        t.links.new(cv.outputs[0], em.inputs["Color"])
    t.links.new(em.outputs[0], out.inputs["Surface"])

    def restore():
        if old is not None:
            t.links.new(old, out.inputs["Surface"])
    return restore


def pixels(img):
    a = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)


def save_png(arr, path, non_color):
    """arr: rows bottom-up (Blender's order), RGBA 0..1."""
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=True, float_buffer=False)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    img.pixels.foreach_set(np.clip(arr, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def load_png(path):
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = "Non-Color"
    a = pixels(img)
    bpy.data.images.remove(img)
    return a


def bake(obj, px, high=None, extras=(), skip_slots=()):
    """Bakes albedo, roughness, metallic, AO and normal for every slot of obj into textures/topi/baked/<obj>_<slot>_{A,N,ORM}.png.
    high: an object to bake the normal from (selected to active). extras: objects present for the AO (the ground).
    skip_slots: slots baked for AO only (the sign fronts: their colour, normal and roughness are the wear layer's)."""
    os.makedirs(TEX_OUT, exist_ok=True)
    sc = bpy.context.scene
    keep = {obj.name} | {e.name for e in extras} | ({high.name} if high else set())
    for o in sc.objects:
        o.hide_render = o.name not in keep
    mats = [m for m in obj.data.materials]
    res = {}
    for pas in ("albedo", "rough", "metal", "ao", "normal"):
        imgs = {}
        for m in mats:
            imgs[m.name] = make_image("%s_%s_%s" % (obj.name, m.name, pas), px, pas != "albedo")
            set_target(m, imgs[m.name])
        restores = []
        if pas in ("albedo", "rough", "metal"):
            for m in mats:
                restores.append(emit_from(m, BAKE_SRC[m.name][pas]))
            select_only(obj)
            sc.cycles.samples = 1
            bpy.ops.object.bake(type="EMIT", margin=8, use_clear=True)
        elif pas == "ao":
            if high is not None:
                high.hide_render = True
            select_only(obj)
            sc.cycles.samples = 64
            bpy.ops.object.bake(type="AO", margin=8, use_clear=True)
        else:
            sc.cycles.samples = 4
            if high is not None:
                select_only(high, obj)
                bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT", use_selected_to_active=True, cage_extrusion=0.003,
                                    max_ray_distance=0.006, margin=8, use_clear=True)
            else:
                select_only(obj)
                bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT", margin=8, use_clear=True)
        if high is not None:
            high.hide_render = False
        for r in restores:
            r()
        for name, img in imgs.items():
            res.setdefault(name, {})[pas] = pixels(img)
            bpy.data.images.remove(img)
    out = {}
    for m in mats:
        r = res[m.name]
        base = os.path.join(TEX_OUT, "%s_%s" % (obj.name, m.name))
        orm = np.ones_like(r["ao"])
        orm[..., 0] = 1.0 if (m.name.startswith("TP_Towel") or m.name == "TP_Rag") else r["ao"][..., 0]
        orm[..., 1] = r["rough"][..., 0]
        orm[..., 2] = r["metal"][..., 0]
        if m.name in skip_slots:
            np.save(base + "_ao.npy", r["ao"][..., 0])        # combined with the wear layer below
            out[m.name] = base
            continue
        alb = r["albedo"].copy()
        alb[..., 3] = 1.0
        if m.name == "TP_Rag":
            alb[..., 3] = rag_alpha(px)
        nrm = r["normal"].copy()
        nrm[..., 1] = 1.0 - nrm[..., 1]                          # Unreal's normal maps: DirectX green
        nrm[..., 3] = 1.0
        save_png(alb, base + "_A.png", False)
        save_png(nrm, base + "_N.png", True)
        save_png(orm, base + "_ORM.png", True)
        out[m.name] = base
    for o in sc.objects:
        o.hide_render = False
    return out


def rag_alpha(px):
    """The rag's torn, frayed outline in its 0..1 UVs (u across, v along; the far end v = 1 is torn into threads)."""
    rng = np.random.default_rng(7)
    v, u = np.meshgrid(np.linspace(0, 1, px), np.linspace(0, 1, px), indexing="ij")    # rows bottom-up = v
    # a ragged tail end: threads hang past a jagged tear
    tear = 0.86 + 0.06 * np.sin(u * 23.0 + 1.3) + 0.04 * np.sin(u * 61.0)
    threads = (np.sin(u * px * 0.45) > 0.55) & (v < tear + 0.08 + 0.05 * np.sin(u * 17))
    a = (v < tear) | threads
    # frayed long edges: small notches
    notch = 0.03 + 0.025 * np.sin(v * 140 + rng.uniform(0, 6)) * (np.sin(v * 37) > 0.2)
    a &= (u > notch) & (u < 1 - notch * 0.8)
    return a.astype(np.float32)


def sign_layers(obj, base_slot, wear_name, px):
    """Writes <mesh>_<slot>_{Wear,N,ORM}.png for a sign front: the wear layer from tools/make_topi_signs.py with the baked AO."""
    base = os.path.join(TEX_OUT, "%s_%s" % (obj.name, base_slot))
    ao = np.load(base + "_ao.npy")
    os.remove(base + "_ao.npy")
    wear = load_png(os.path.join(TEX_IN, wear_name + "_wear.png"))
    nrm = load_png(os.path.join(TEX_IN, wear_name + "_wear_n.png"))
    rm = load_png(os.path.join(TEX_IN, wear_name + "_wear_rm.png"))
    if wear.shape[0] != ao.shape[0] or wear.shape[1] != ao.shape[1]:
        ys = (np.arange(wear.shape[0]) * ao.shape[0] / wear.shape[0]).astype(int)
        xs = (np.arange(wear.shape[1]) * ao.shape[1] / wear.shape[1]).astype(int)
        ao = ao[ys][:, xs]
    orm = np.ones(wear.shape, dtype=np.float32)
    orm[..., 0] = ao
    orm[..., 1] = rm[..., 0]
    orm[..., 2] = rm[..., 1]
    # the wear picture is sRGB: copy it as it is (load_png read the raw values)
    save_png(wear, base + "_Wear.png", True)
    save_png(nrm, base + "_N.png", True)
    save_png(orm, base + "_ORM.png", True)


# ================================================================================================ checks

def uv_overlap(obj, res=512):
    """Per slot: the share of covered texels that more than one triangle covers (shrunk a little so neighbours don't count)."""
    me = obj.data
    me.calc_loop_triangles()
    uv = me.uv_layers["UVMap"].data
    out = {}
    for si, m in enumerate(me.materials):
        count = np.zeros((res, res), dtype=np.int16)
        oob = 0
        for tri in me.loop_triangles:
            if tri.material_index != si:
                continue
            p = np.array([uv[li].uv[:] for li in tri.loops]) * res
            if (p < -0.5).any() or (p > res + 0.5).any():
                oob += 1
            c = p.mean(axis=0)
            p = c + (p - c) * 0.9
            x0, y0 = np.floor(p.min(axis=0)).astype(int)
            x1, y1 = np.ceil(p.max(axis=0)).astype(int)
            x0, y0 = max(x0, 0), max(y0, 0)
            x1, y1 = min(x1, res - 1), min(y1, res - 1)
            if x1 < x0 or y1 < y0:
                continue
            ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1] + 0.5
            (ax, ay), (bx, by), (cx, cy) = p
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            l1 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / d
            l2 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / d
            inside = (l1 >= 0) & (l2 >= 0) & (l1 + l2 <= 1)
            count[y0:y1 + 1, x0:x1 + 1] += inside
        cov = (count > 0).sum()
        over = (count > 1).sum()
        out[m.name] = (over / max(cov, 1), cov / (res * res), oob)
        print("UVCHECK %s %s: overlap %.2f%% of covered, coverage %.0f%%, triangles out of 0..1: %d" % (
            obj.name, m.name, 100 * over / max(cov, 1), 100 * cov / (res * res), oob))
    return out


def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


# ================================================================================================ preview

def preview_material(obj, slot, base, picture=None):
    m = bpy.data.materials.new("prev_%s_%s" % (obj.name, slot))
    m.use_nodes = True
    N = Nodes(m)
    for nd in list(m.node_tree.nodes):
        if nd.type != "OUTPUT_MATERIAL":
            m.node_tree.nodes.remove(nd)
    bsdf = N.new("ShaderNodeBsdfPrincipled")
    orm = N.image(base + "_ORM.png", True)
    sepc = N.new("ShaderNodeSeparateColor")
    N.link(orm.outputs["Color"], sepc.inputs[0])
    if picture:
        pic = N.image(picture)
        wear = N.image(base + "_Wear.png")
        col = N.mix(wear.outputs["Alpha"], pic.outputs["Color"], wear.outputs["Color"])
    else:
        alb = N.image(base + "_A.png")
        col = alb.outputs["Color"]
        N.link(alb.outputs["Alpha"], bsdf.inputs["Alpha"])
    ao_mul = N.new("ShaderNodeMix", data_type="RGBA", blend_type="MULTIPLY")
    ao_mul.inputs[0].default_value = 1.0
    N.link(col, ao_mul.inputs[6])
    N.link(sepc.outputs[0], ao_mul.inputs[7])
    N.link(ao_mul.outputs[2], bsdf.inputs["Base Color"])
    N.link(sepc.outputs[1], bsdf.inputs["Roughness"])
    N.link(sepc.outputs[2], bsdf.inputs["Metallic"])
    nimg = N.image(base + "_N.png", True)
    # back to OpenGL green for Blender
    flip = N.new("ShaderNodeSeparateColor")
    N.link(nimg.outputs["Color"], flip.inputs[0])
    inv = N.math("SUBTRACT", 1.0, flip.outputs[1])
    comb = N.new("ShaderNodeCombineColor")
    N.link(flip.outputs[0], comb.inputs[0])
    N.link(inv, comb.inputs[1])
    N.link(flip.outputs[2], comb.inputs[2])
    nm = N.new("ShaderNodeNormalMap", uv_map="UVMap")
    N.link(comb.outputs[0], nm.inputs["Color"])
    N.link(nm.outputs[0], bsdf.inputs["Normal"])
    out = next(nd for nd in m.node_tree.nodes if nd.type == "OUTPUT_MATERIAL")
    N.link(bsdf.outputs[0], out.inputs["Surface"])
    return m


def render_preview(objs, path):
    """Each prop from the front, the side and the back (plus a close look at the signs) on sand, into one sheet."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 24 if QUICK else 64
    sc.cycles.use_denoising = True
    sc.render.resolution_x = 520
    sc.render.resolution_y = 520
    sc.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.62, 0.75, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    sc.world = world
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    sc.collection.objects.link(sun)
    gm = bpy.data.meshes.new("ground")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=200.0)
    bm.to_mesh(gm)
    bm.free()
    ground = bpy.data.objects.new("ground", gm)
    gmat = bpy.data.materials.new("sandprev")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.62, 0.55, 0.42, 1)
    gm.materials.append(gmat)
    sc.collection.objects.link(ground)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    tiles = []
    for o in objs:
        bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
        lo = Vector((min(p.x for p in bb), min(p.y for p in bb), max(min(p.z for p in bb), 0.0)))
        hi = Vector((max(p.x for p in bb), max(p.y for p in bb), max(p.z for p in bb)))
        centre = (lo + hi) / 2
        size = max((hi - lo).length, 0.3)
        for other in objs:
            other.hide_render = other is not o
        views = [(-90, 12), (-20, 25), (120, 18)]
        if "Sign" in o.name or "Stake" in o.name:
            views.append(("close", 0))
        for az, el in views:
            if az == "close" and "Stake" in o.name:
                target = Vector((centre.x, centre.y, 1.0))
                cam.location = target + Vector((0.25, -0.75, 0.15))
            elif az == "close":
                # close on the sign's face
                target = Vector((centre.x, lo.y, hi.z - (0.45 if "Warn" in o.name else 0.55)))
                d = 0.95 if "Warn" in o.name else 1.25
                cam.location = target + Vector((0.12, -d, 0.05))
            else:
                a, e = math.radians(az), math.radians(el)
                d = size * 1.25 + 0.4
                target = centre
                cam.location = centre + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * d
            cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
            cam.data.lens = 40
            f = os.path.join(OUT, "_prev_%s_%s.png" % (o.name, az))
            sc.render.filepath = f
            bpy.ops.render.render(write_still=True)
            tiles.append(f)
    for o in objs:
        o.hide_render = False
    # the sheet: 3 tiles a row, 1600 px wide
    cols = 3
    tw = 1600 // cols
    rows = (len(tiles) + cols - 1) // cols
    sheet = np.zeros((rows * tw, cols * tw, 4), dtype=np.float32)
    sheet[..., 3] = 1
    for k, f in enumerate(tiles):
        img = bpy.data.images.load(f)
        img.scale(tw, tw)
        a = pixels(img)
        bpy.data.images.remove(img)
        r, c = k // cols, k % cols
        y0 = (rows - 1 - r) * tw
        sheet[y0:y0 + tw, c * tw:c * tw + tw] = a
        os.remove(f)
    img = bpy.data.images.new("preview", cols * tw, rows * tw, alpha=False)
    img.pixels.foreach_set(sheet.ravel())
    img.filepath_raw = path
    img.file_format = "JPEG"
    sc.render.image_settings.quality = 88
    img.save()
    print("TOPI preview -> %s (%d tiles)" % (path, len(tiles)))


# ================================================================================================ export

def export(obj, path):
    select_only(obj)
    kw = dict(filepath=path, use_selection=True, export_format="GLB", export_yup=True, export_apply=True, export_materials="EXPORT",
              export_attributes=False)
    props = bpy.ops.export_scene.gltf.get_rna_type().properties
    bpy.ops.export_scene.gltf(**{k: v for k, v in kw.items() if k in props or k == "filepath"})


MATS = {}


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    set_render_for_bake()
    MATS["TP_Metal"] = mat_warn_body()
    MATS["TP_SignWarn"] = mat_picture("TP_SignWarn", os.path.join(LOC, "topi_sign_warn.png"), os.path.join(TEX_IN, "warn_wear.png"))
    MATS["TP_Wood"] = mat_wood()
    MATS["TP_SignUral"] = mat_picture("TP_SignUral", os.path.join(LOC, "topi_sign_ural.png"), os.path.join(TEX_IN, "ural_wear.png"))
    MATS["TP_Rag"] = mat_rag()
    for L in "ABC":
        MATS["TP_Towel_" + L] = mat_towel(L)

    made = []
    # the signs
    for fn, wear in ((warn_sign, "warn"), (ural_sign, "ural")):
        if not wanted("WarnSign" if wear == "warn" else "UralSign"):
            continue
        obj = fn()
        recalc(obj)
        for slot in (["TP_Metal", "TP_SignWarn"] if wear == "warn" else ["TP_Wood", "TP_SignUral"]):
            obj.data.materials.append(MATS[slot])
        body = "TP_Metal" if wear == "warn" else "TP_Wood"
        front = "TP_SignWarn" if wear == "warn" else "TP_SignUral"
        smart_uv(obj, [body])
        uv_overlap(obj)
        if not NOBAKE:
            bake(obj, SIGN_PX, skip_slots=(front,))
            sign_layers(obj, front, wear, SIGN_PX)
        made.append((obj, {body: None, front: os.path.join(LOC, "topi_sign_%s.png" % wear)}))
    # stakes with rags
    for L in "ABCD":
        if not wanted("Stake_" + L):
            continue
        st = stake(L)
        st.data.materials.append(MATS["TP_Wood"])
        st.data.materials.append(MATS["TP_Rag"])
        smart_uv(st, ["TP_Wood"])
        rag = rag_cloth(st)
        # join the rag into the stake (its faces in the TP_Rag slot)
        rag.data.materials.append(MATS["TP_Rag"])
        for poly in rag.data.polygons:
            poly.material_index = 0
        select_only(rag, st)
        bpy.ops.object.join()
        uv_overlap(st)
        if not NOBAKE:
            bake(st, SMALL_PX)
        made.append((st, {"TP_Wood": None, "TP_Rag": None}))
    # towels
    for L in "ABC":
        if not wanted("Towel_" + L):
            continue
        low, high, sand = towel(L)
        uv_overlap(low)
        if not NOBAKE:
            bake(low, SMALL_PX, high=high, extras=(sand,))
        for o in (high, sand):
            bpy.data.objects.remove(o)
        made.append((low, {"TP_Towel_" + L: None}))

    # export: the slot names are the materials' names (TP_Towel_<L> reads as TP_Towel in import_topi.py)
    for obj, _ in made:
        export(obj, os.path.join(OUT, obj.name + ".glb"))
        print("TOPI %s: %d triangles, slots %s -> %s" % (obj.name, tris(obj), [m.name for m in obj.data.materials], os.path.join(OUT, obj.name + ".glb")))
    # the old single stake name stays (LyudTopiProps falls back to it): a copy of stake A
    import shutil
    if wanted("Stake_A"):
        shutil.copyfile(os.path.join(OUT, "SM_Topi_Stake_A.glb"), os.path.join(OUT, "SM_Topi_Stake.glb"))
    preview = "preview.jpg" if not ONLY else "preview_%s.jpg" % "_".join(sorted(ONLY))

    if NOBAKE:
        return
    # preview with the baked textures
    for obj, slots in made:
        for i, m in enumerate(list(obj.data.materials)):
            base = os.path.join(TEX_OUT, "%s_%s" % (obj.name, m.name))
            obj.data.materials[i] = preview_material(obj, m.name, base, slots.get(m.name))
    # spread them out for the renders (each is shown alone)
    render_preview([o for o, _ in made], os.path.join(OUT, preview))


if __name__ == "__main__":
    main()
