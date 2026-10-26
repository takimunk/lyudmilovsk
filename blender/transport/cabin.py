"""The cable-car cabin as a room (docs/transport.md "The cabin"): the Hi3D egg (design/transport/gen/cabin.glb, one
closed 300k-triangle surface) rebuilt clean, without any new generation. Nothing is cut out of the generated triangles
(decimated they are big and crooked, and a window chosen by the texel colour under each one comes out ragged):

  - a clean shell: a sphere shrunk onto the model's outer skin along rays from outside in, the model's colour and
    relief baked onto it from the full original; solidified into a closed 4.5 cm wall, plaster inside;
  - windows where the texture is window, each a smooth outline cut through the wall by an exact boolean (chrome
    reveals), glass laid in them; the door opening (CharacterStandard) cut the same way on the painted door, the
    leaf (SM_TR_CabinDoor) the same outline intersected with the wall;
  - a terrazzo floor, two padded benches facing each other, a ceiling lamp, chrome rails (SM_TR_CabinInside);
  - the hanger, arm and lantern as the generated mesh, decimated (SM_TR_CabinHanger), bent so the grip is plumb over
    the floor's centre;
  - sockets (kit.json cabin.sockets) and simple collision boxes (cabin.collision) for the game.

Frame of every part (Blender, metres, Z up; Unreal (x, -y, z) * 100): origin on the floor's centre under the grip; the
rope along X; the door on +Y (Unreal: -Y, left of the way it goes). Called by optimize.py (`-- cabin`).
"""
import bmesh
import bpy
import math
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

CABIN_LEN = 4.6          # m along the rope: long enough for the standard door (clear 1.10 x 2.12 m) under the egg's roof
FLOOR = 0.30             # m over the egg's bottom
WALL = 0.045             # m of shell thickness
DOOR_W, DOOR_H = 1.10, 2.12   # clear opening (CharacterStandard: >= 1.08 x 2.10)
LEAF_OUT = 0.08          # m a leaf steps out before it slides
HANGER_BUDGET = 1400
TEX_SIZE = 2048
SEAT_H, SEAT_D, BACK_H = 0.45, 0.50, 1.00   # m: cushion top over the floor, depth, backrest top
GLASS_DARK, GLASS_SAT = 0.46, 0.62          # its tinted windows are a warm brown


def mat(name, rgba, rough=0.5, metal=0.0, emit=False):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = rgba
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit:
        b.inputs["Emission Color"].default_value = rgba
        b.inputs["Emission Strength"].default_value = 3.0
    m.diffuse_color = rgba
    return m


def to_object(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def copy_faces(src_bm, faces, uv_src):
    """A new bmesh with these faces of src_bm (positions, UVs, material indices)."""
    out = bmesh.new()
    uv = out.loops.layers.uv.new("UVMap")
    vm = {}
    for f in faces:
        vs = []
        for v in f.verts:
            if v not in vm:
                vm[v] = out.verts.new(v.co)
            vs.append(vm[v])
        try:
            nf = out.faces.new(vs)
        except ValueError:
            continue
        nf.material_index = f.material_index
        for l0, l1 in zip(f.loops, nf.loops):
            l1[uv].uv = l0[uv_src].uv
    return out


def box(bm, c, size, bevel=0.0, rot=None):
    m = Matrix.Translation(c) @ (rot or Matrix()) @ Matrix.Diagonal((*size, 1.0))
    r = bmesh.ops.create_cube(bm, size=1.0, matrix=m)
    if bevel > 0:
        edges = list({e for v in r["verts"] for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=edges, offset=bevel, segments=2, affect="EDGES", profile=0.5)


def rod(bm, a, b, radius, seg=8):
    a, b = Vector(a), Vector(b)
    d = b - a
    m = Matrix.Translation((a + b) / 2) @ d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=radius, radius2=radius, depth=d.length, matrix=m)


def solidify(o, thickness, inner_offset, smooth_inside=0):
    """A wall inward; the inner face (material slot + inner_offset) smoothed smooth_inside times, so the room inside
    doesn't repeat every trim and ridge of the outside."""
    mod = o.modifiers.new("wall", "SOLIDIFY")
    mod.thickness = thickness
    mod.offset = -1.0
    mod.use_rim = True
    mod.material_offset = inner_offset
    mod.material_offset_rim = 0
    mod.shell_vertex_group = "inner"
    o.vertex_groups.new(name="inner")
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier=mod.name)
    if smooth_inside:
        sm = o.modifiers.new("soft", "LAPLACIANSMOOTH")
        sm.vertex_group = "inner"
        sm.iterations = smooth_inside
        sm.lambda_factor = 0.8
        sm.use_volume_preserve = False
        bpy.ops.object.modifier_apply(modifier=sm.name)
    o.vertex_groups.clear()


def majority(bm, flag, rounds=2):
    """Smooth a per-face flag: a face takes what most of its edge neighbours are (no speckles or pinholes)."""
    for _ in range(rounds):
        nxt = dict(flag)
        for f in bm.faces:
            nb = [g for e in f.edges for g in e.link_faces if g is not f]
            k = sum(1 for g in nb if flag[g])
            if k * 2 > len(nb):
                nxt[f] = True
            elif k * 2 < len(nb):
                nxt[f] = False
        flag.clear()
        flag.update(nxt)


def painted_door(o, texture):
    """Where the Hi3D texture has the door on the +Y side: the two grey frame stripes, 0.6..1.2 m apart, crossing a
    line 0.5 m over the floor, nearest the middle. Returns the door's middle x (or None)."""
    w, h, px = texture
    bm = bmesh.new()
    bm.from_mesh(o.data)
    ul = bm.loops.layers.uv.active
    bm.faces.ensure_lookup_table()
    bvh = BVHTree.FromBMesh(bm)
    grey = []
    for i in range(-200, 201):
        x = i * 0.01
        loc, _n, fi, _d = bvh.ray_cast(Vector((x, 5.0, 0.5)), Vector((0, -1, 0)), 10.0)
        if fi is None:
            continue
        f = bm.faces[fi]
        u = sum(l[ul].uv[0] for l in f.loops) / len(f.loops)
        v = sum(l[ul].uv[1] for l in f.loops) / len(f.loops)
        k = (min(h - 1, int(v % 1.0 * h)) * w + min(w - 1, int(u % 1.0 * w))) * 4
        if max(px[k:k + 3]) < 0.62:
            grey.append(x)
    bm.free()
    # stripes: runs of grey samples -> their middles
    runs, cur = [], []
    for x in grey:
        if cur and x - cur[-1] > 0.015:
            runs.append(sum(cur) / len(cur))
            cur = []
        cur.append(x)
    if cur:
        runs.append(sum(cur) / len(cur))
    pairs = [(a, b) for a in runs for b in runs if 0.6 < b - a < 1.2]
    if not pairs:
        return None
    a, b = min(pairs, key=lambda p: abs(p[0] + p[1]))
    return (a + b) / 2


def convex(pts):
    """2D convex hull (z kept at 0), counter-clockwise."""
    p = sorted({(round(v.x, 4), round(v.y, 4)) for v in pts})
    if len(p) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for q in p:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], q) <= 0:
            lower.pop()
        lower.append(q)
    for q in reversed(p):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], q) <= 0:
            upper.pop()
        upper.append(q)
    return [Vector((x, y, 0.0)) for x, y in lower[:-1] + upper[:-1]]


def smooth(o, angle=40):
    for p in o.data.polygons:
        p.use_smooth = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def clean_hull(body_bvh, centre, segs=72, rings=40, smooth_steps=2):
    """The egg as a clean, even mesh: a sphere round the room's middle whose every vertex is moved along its ray onto
    the generated model's outer skin (rays from outside in, so nothing left inside the model can catch them). Vertices
    whose ray finds nothing (the cut-off roof cap under the lantern and the arm) are filled in from their neighbours."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0, matrix=Matrix.Translation(centre))
    known = {}
    for v in bm.verts:
        d = (v.co - centre).normalized()
        hit = body_bvh.ray_cast(centre + d * 8.0, -d, 8.0)
        if hit[0] is not None:
            v.co = hit[0]
            known[v] = True
    free = [v for v in bm.verts if v not in known]
    for _ in range(200):                         # harmonic fill of the holes
        for v in free:
            nb = [e.other_vert(v).co for e in v.link_edges]
            v.co = sum(nb, Vector()) / len(nb)
    for _ in range(smooth_steps):
        bmesh.ops.smooth_vert(bm, verts=list(bm.verts), factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.normal_update()
    return bm, len(free)


def bake(src, dst, size):
    """The generated model's colour and relief (trims, seams, handles) baked onto the clean shell's own UVs."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 1
    sc.render.bake.margin = 8
    bpy.context.view_layer.objects.active = dst
    bpy.ops.object.select_all(action="DESELECT")
    dst.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004)
    bpy.ops.object.mode_set(mode="OBJECT")
    m = bpy.data.materials.new("MI_TR_Cabin")
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.45
    imgs = {}
    for kind in ("color", "normal"):
        img = bpy.data.images.new("cabin_" + kind, size, size)
        if kind == "normal":
            img.colorspace_settings.name = "Non-Color"
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = img
        imgs[kind] = (img, node)
    dst.data.materials.clear()
    dst.data.materials.append(m)
    src.hide_render = False
    src.select_set(True)
    dst.select_set(True)
    bpy.context.view_layer.objects.active = dst
    for kind, btype in (("color", "DIFFUSE"), ("normal", "NORMAL")):
        nt.nodes.active = imgs[kind][1]
        kw = {"pass_filter": {"COLOR"}} if btype == "DIFFUSE" else {}
        bpy.ops.object.bake(type=btype, use_selected_to_active=True, cage_extrusion=0.06, max_ray_distance=0.25, **kw)
        imgs[kind][0].pack()
    nt.links.new(imgs["color"][1].outputs["Color"], bsdf.inputs["Base Color"])
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(imgs["normal"][1].outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    src.select_set(False)
    return m


def chaikin(poly, rounds=3):
    for _ in range(rounds):
        out = []
        for i, a in enumerate(poly):
            b = poly[(i + 1) % len(poly)]
            out += [a.lerp(b, 0.25), a.lerp(b, 0.75)]
        poly = out
    return poly


def inset(poly, d):
    c = sum(poly, Vector((0, 0, 0))) / len(poly)
    return [c + (p - c) * max(0.0, 1.0 - d / max(1e-6, (p - c).length)) for p in poly]


def prism(bm, poly2d, frame, z0, z1):
    """A closed prism into bm: poly2d (Vectors, x/y in the frame's plane) from z0 to z1 along the frame's normal."""
    o, u, v, n = frame
    bot = [bm.verts.new(o + u * p.x + v * p.y + n * z0) for p in poly2d]
    top = [bm.verts.new(o + u * p.x + v * p.y + n * z1) for p in poly2d]
    bm.faces.new(list(reversed(bot)))
    bm.faces.new(top)
    k = len(poly2d)
    for i in range(k):
        bm.faces.new((bot[i], bot[(i + 1) % k], top[(i + 1) % k], top[i]))


def boolean(target, cutter, op):
    mod = target.modifiers.new("cut", "BOOLEAN")
    mod.operation = op
    mod.solver = "EXACT"
    mod.object = cutter
    if hasattr(mod, "material_mode"):
        mod.material_mode = "TRANSFER"
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=mod.name)


def build(o, ctx):
    """o: the raw egg joined into one mesh. ctx: optimize.py's helpers. Returns (objects by export name, kit entry)."""
    transform, bounds, decimate, texels = ctx["transform"], ctx["bounds"], ctx["decimate"], ctx["texels"]
    # long side along Y in the raw model, the door on +X: turn so the long side is X and the door +Y; scale; floor at 0
    transform(o, Matrix.Rotation(math.radians(90), 4, "Z"))
    lo, hi = bounds(o)
    s = CABIN_LEN / (hi.x - lo.x)
    c = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    transform(o, Matrix.Translation((0, 0, -FLOOR * 1.0)) @ Matrix.Scale(s, 4) @ Matrix.Translation(-c))
    lo, hi = bounds(o)
    # going up from the middle: the roof lantern starts where the slices get much narrower than the body, the arm
    # where they are thinner still
    ext = {}
    for v in o.data.vertices:
        k = round(v.co.z, 1)
        a, b = ext.get(k, (1e9, -1e9))
        ext[k] = (min(a, v.co.y), max(b, v.co.y))
    wide = hi.y - lo.y
    mid = (hi.z + lo.z) * 0.3
    arm_z = min(k for k, (a, b) in ext.items() if k > mid and b - a < wide * 0.2) - 0.05
    top_z = min(k for k, (a, b) in ext.items() if k > mid and b - a < wide * 0.4) - 0.05
    tex_w, tex_h, px = texels(o)
    painted = painted_door(o, (tex_w, tex_h, px))
    shell_src_mat = o.data.materials[0]

    # the full-detail model stays as the bake source and the texture lookup
    src = o
    sb = bmesh.new()
    sb.from_mesh(src.data)
    sul = sb.loops.layers.uv.active
    sb.faces.ensure_lookup_table()
    body = [f.index for f in sb.faces if f.calc_center_median().z <= top_z]
    body_bvh = BVHTree.FromPolygons([v.co for v in sb.verts], [[v.index for v in sb.faces[i].verts] for i in body])
    full_bvh = BVHTree.FromBMesh(sb)

    def texel_at(p, n):
        """The generated texture's colour where a ray along -n through p meets the model."""
        loc, _n, fi, _d = full_bvh.ray_cast(p + n * 0.15, -n, 0.4)
        if fi is None:
            return None
        f = sb.faces[fi]
        u = sum(l[sul].uv[0] for l in f.loops) / len(f.loops)
        v = sum(l[sul].uv[1] for l in f.loops) / len(f.loops)
        k = (min(tex_h - 1, int(v % 1.0 * tex_h)) * tex_w + min(tex_w - 1, int(u % 1.0 * tex_w))) * 4
        return px[k:k + 3]

    # --- the hanger, the arm and the lantern: the generated mesh, decimated, the grip brought plumb ---------------
    hsrc = bmesh.new()
    hsrc.from_mesh(src.data)
    hul = hsrc.loops.layers.uv.active
    hb = copy_faces(hsrc, [f for f in hsrc.faces if f.calc_center_median().z > top_z], hul)
    hsrc.free()
    gv = [v for v in hb.verts if v.co.z > hi.z - 0.5]
    glo = Vector([min(v.co[i] for v in gv) for i in range(3)])
    ghi = Vector([max(v.co[i] for v in gv) for i in range(3)])
    grip = (glo + ghi) / 2
    foot = [v.co for v in hb.verts if arm_z < v.co.z < arm_z + 0.08]
    foot_c = sum(foot, Vector()) / max(1, len(foot))
    shift = Vector((-grip.x, -grip.y, 0.0))
    for v in hb.verts:
        t = min(1.0, max(0.0, (v.co.z - arm_z) / max(0.01, glo.z - arm_z)))
        v.co += shift * (t * t * (3 - 2 * t))
    grip += shift
    grip_along = (ghi.x - glo.x) >= (ghi.y - glo.y)

    # --- the clean shell ---------------------------------------------------------------------------------------
    centre = Vector((0.0, 0.0, 1.0))
    hull_bm, n_filled = clean_hull(body_bvh, centre)
    hull_bvh = BVHTree.FromBMesh(hull_bm)

    def ray(p, d, far=6.0):
        hit = hull_bvh.ray_cast(Vector(p), Vector(d).normalized(), far)
        return hit[0]

    def hull_y(x, z, sgn=1):
        h = ray((x, sgn * 5.0, z), (0, -sgn, 0), 10.0)
        return abs(h.y) if h else 0.0

    def hull_x(z, sgn, y=0.0):
        h = ray((sgn * 5.0, y, z), (-sgn, 0, 0), 10.0)
        return abs(h.x) if h else 0.0

    door_x = painted if painted is not None else 0.0
    door_lintel = min(hull_y(door_x + sx * DOOR_W / 2, DOOR_H + 0.10) for sx in (-1, 1))
    x0, x1 = door_x - DOOR_W / 2, door_x + DOOR_W / 2
    sill_y = hull_y(door_x, 0.0)
    side_y = max(hull_y(door_x, z * 0.1) for z in range(1, 21))
    end_x = {round(z * 0.05, 2): min(hull_x(z * 0.05, 1), hull_x(z * 0.05, -1)) for z in range(0, 45)}
    floor_ring = []
    for i in range(64):
        a = 2 * math.pi * i / 64
        h = ray((5 * math.cos(a), 5 * math.sin(a), 0.0), (-math.cos(a), -math.sin(a), 0), 10.0)
        if h:
            p = Vector((h.x, h.y, 0.0))
            floor_ring.append(p - p.normalized() * (WALL + 0.005))
    floor_ring = convex(floor_ring)
    roof_c = 2.6
    for hz in (ray((x * 0.1, y * 0.1, 1.2), (0, 0, 1)) for x in (-3, 0, 3) for y in (-2, 0, 2)):
        if hz:
            roof_c = min(roof_c, hz.z)

    # --- windows: where the generated texture is window, smooth outlines ---------------------------------------
    def is_glass(rgb, z):
        if rgb is None:
            return False
        top, low = max(rgb), min(rgb)
        sat = (top - low) / top if top > 1e-4 else 0.0
        return top < GLASS_DARK and sat < GLASS_SAT and 0.4 < z < roof_c - 0.25

    hull_bm.faces.ensure_lookup_table()
    flag = {}
    for f in hull_bm.faces:
        cc = f.calc_center_median()
        in_door = cc.y > 0.3 and x0 - 0.05 < cc.x < x1 + 0.05 and cc.z < DOOR_H + 0.08
        flag[f] = (not in_door) and is_glass(texel_at(cc, f.normal), cc.z)
    majority(hull_bm, flag)
    windows = []
    seen = set()
    for f in hull_bm.faces:
        if not flag[f] or f in seen:
            continue
        comp, stack = [], [f]
        seen.add(f)
        while stack:
            g = stack.pop()
            comp.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if flag[h] and h not in seen:
                        seen.add(h)
                        stack.append(h)
        if sum(g.calc_area() for g in comp) > 0.05:
            windows.append(comp)

    cut_bm = bmesh.new()
    panes = []
    for comp in windows:
        n = sum((g.normal * g.calc_area() for g in comp), Vector()).normalized()
        o3 = sum((g.calc_center_median() for g in comp), Vector()) / len(comp)
        u = n.cross(Vector((0, 0, 1)))
        u = u.normalized() if u.length > 1e-3 else Vector((1, 0, 0))
        v = n.cross(u).normalized()
        pts = [Vector(((p.co - o3).dot(u), (p.co - o3).dot(v), 0.0)) for g in comp for p in g.verts]
        poly = inset(chaikin(convex(pts), 3), 0.02)
        frame = (o3, u, v, n)
        prism(cut_bm, poly, frame, -0.35, 0.30)
        panes.append((poly, frame))
    # the door: a straight opening, the corners at the top a little round
    r = 0.10
    door_poly = [Vector((x0, -0.02, 0)), Vector((x1, -0.02, 0))]
    for i in range(7):
        a = math.pi / 2 * i / 6
        door_poly.append(Vector((x1 - r + r * math.cos(a), DOOR_H - r + r * math.sin(a), 0)))
    for i in range(7):
        a = math.pi / 2 + math.pi / 2 * i / 6
        door_poly.append(Vector((x0 + r + r * math.cos(a), DOOR_H - r + r * math.sin(a), 0)))
    door_frame = (Vector((0, 0, 0)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0)))
    # the outline's winding must agree with the prism's normal (u x v = n): here u x v = -n, so turn it round
    door_poly2 = [Vector((p.x, p.y, 0)) for p in reversed(door_poly)]
    door_bm = bmesh.new()
    prism(door_bm, door_poly2, door_frame, 0.3, 2.0)
    prism(cut_bm, door_poly2, door_frame, 0.3, 2.0)

    # --- objects -------------------------------------------------------------------------------------------------
    m_glass = mat("TR_Glass", (0.3, 0.4, 0.45, 0.25))
    m_room = mat("TR_Interior", (0.86, 0.82, 0.74, 1))
    m_floor = mat("TR_Terrazzo", (0.60, 0.56, 0.50, 1), 0.45)
    m_seat = mat("TR_Seat", (0.62, 0.26, 0.06, 1), 0.5)
    m_chrome = mat("TR_Chrome", (0.85, 0.85, 0.87, 1), 0.2, 1.0)
    m_glow = mat("TR_Glow", (1.0, 0.82, 0.55, 1), emit=True)

    shell = to_object("SM_TR_Cabin", hull_bm, [])
    m_paint = bake(src, shell, TEX_SIZE)
    shell.data.materials.append(m_room)
    for p in shell.data.polygons:
        p.use_smooth = True
    solidify(shell, WALL, 1)
    whole = shell.copy()
    whole.data = shell.data.copy()
    bpy.context.scene.collection.objects.link(whole)
    cutters = to_object("cutters", cut_bm, [m_chrome])
    boolean(shell, cutters, "DIFFERENCE")
    # the leaf: the shell inside the door outline, a hair smaller than the opening
    door_cut = to_object("door_cut", door_bm, [m_chrome])
    for v in door_cut.data.vertices:
        v.co.x = door_x + (v.co.x - door_x) * (1 - 0.008 / DOOR_W)
        v.co.z = 0.004 + v.co.z * (1 - 0.008 / DOOR_H)
    boolean(whole, door_cut, "INTERSECT")
    whole.name = whole.data.name = "SM_TR_CabinDoor"
    leaf = whole

    # glass: each window's outline filled and laid on the hull, half a wall in, glazed both ways
    gb = bmesh.new()
    for poly, (o3, u, v, n) in panes:
        vs = [gb.verts.new(o3 + u * p.x + v * p.y) for p in poly]
        f = gb.faces.new(vs)
        tri = bmesh.ops.triangulate(gb, faces=[f], quad_method="BEAUTY", ngon_method="BEAUTY")
        edges = list({e for t in tri["faces"] for e in t.edges})
        bmesh.ops.subdivide_edges(gb, edges=edges, cuts=2, use_grid_fill=True)
    gb.verts.ensure_lookup_table()
    for vert in gb.verts:
        # onto the hull along the window's normal (the nearest pane's)
        best = min(panes, key=lambda pf: (vert.co - pf[1][0]).length)
        n = best[1][3]
        hit = hull_bvh.ray_cast(vert.co + n * 0.6, -n, 1.2)
        if hit[0] is not None:
            vert.co = hit[0] - n * (WALL * 0.5)
    back = bmesh.ops.duplicate(gb, geom=list(gb.faces))
    bmesh.ops.reverse_faces(gb, faces=[e for e in back["geom"] if isinstance(e, bmesh.types.BMFace)])
    gl = to_object("glass", gb, [m_glass])
    bpy.ops.object.select_all(action="DESELECT")
    for ob in (shell, gl):
        ob.select_set(True)
    bpy.context.view_layer.objects.active = shell
    bpy.ops.object.join()
    for ob in (cutters, door_cut):
        bpy.data.objects.remove(ob)

    hanger = to_object("SM_TR_CabinHanger", hb, [shell_src_mat])
    decimate(hanger, HANGER_BUDGET)
    for ob in (shell, leaf, hanger):
        smooth(ob, 35)
    print("CABIN windows", len(windows), "filled hull verts", n_filled)

    # the inside: floor, benches, lamp, rails
    ib = bmesh.new()

    def add(fn, mi):
        n0 = len(ib.faces)
        fn()
        ib.faces.ensure_lookup_table()
        for i in range(n0, len(ib.faces)):
            ib.faces[i].material_index = mi

    def floor():
        vs = [ib.verts.new(p) for p in floor_ring]
        f = ib.faces.new(vs)
        f.normal_update()
        if f.normal.z < 0:
            f.normal_flip()
        bmesh.ops.triangulate(ib, faces=[f], quad_method="BEAUTY", ngon_method="BEAUTY")

    add(floor, 0)

    def inner_end(z0, z1):
        return min(v for z, v in end_x.items() if z0 <= z <= z1) - WALL - 0.02

    seats = []
    bench = {}
    for sgn in (1, -1):
        back_x = inner_end(0.12, SEAT_H)             # the hull's end, inside, at the cushion
        while True:                                 # forward until two people fit side by side
            front_x = back_x - SEAT_D
            half_w = min(min(hull_y(sgn * x, z, 1), hull_y(sgn * x, z, -1)) for x in (front_x, back_x)
                         for z in (0.12, SEAT_H, BACK_H)) - WALL - 0.04
            if half_w >= 0.55 or back_x < 1.0:
                break
            back_x -= 0.05
        base_back = min(back_x, inner_end(0.0, 0.12)) - 0.02
        cx = sgn * (front_x + back_x) / 2
        bcx = sgn * (front_x + 0.02 + base_back) / 2
        add(lambda: box(ib, Vector((bcx, 0, (SEAT_H - 0.12) / 2)), (base_back - front_x - 0.02, half_w * 2 - 0.08, SEAT_H - 0.12)), 2)
        add(lambda: box(ib, Vector((cx, 0, SEAT_H - 0.06)), (SEAT_D, half_w * 2, 0.12), 0.03), 1)
        # the backrest: from the cushion's back, leaning back as far as the hull lets it
        bz0, bz1 = SEAT_H, BACK_H
        bx0 = back_x - 0.06
        bx1 = min(bx0 + 0.12, inner_end(bz1 - 0.05, bz1 + 0.05) - 0.06)
        tilt = math.atan2(bx1 - bx0, bz1 - bz0)
        rot = Matrix.Rotation(sgn * tilt, 4, "Y")
        add(lambda: box(ib, Vector((sgn * (bx0 + bx1) / 2, 0, (bz0 + bz1) / 2)), (0.10, half_w * 2, (bz1 - bz0) / math.cos(tilt)),
                        0.03, rot), 1)
        bench[sgn] = {"x": round(cx, 3), "front": round(sgn * front_x, 3), "half_w": round(half_w, 3)}
        for t in (-1, 1):
            # a seat: the floor under the cushion's middle, facing the other bench
            seats.append({"at": [round(cx, 3), round(t * half_w * 0.5, 3), 0.0], "yaw": 180.0 if sgn > 0 else 0.0})
    roof_in = roof_c - WALL
    add(lambda: bmesh.ops.create_cone(ib, cap_ends=True, segments=16, radius1=0.22, radius2=0.18, depth=0.04,
                                      matrix=Matrix.Translation((0, 0, roof_in - 0.03))), 3)
    rail_z = min(DOOR_H + 0.02, roof_in - 0.12)
    rail_x = bench[1]["front"] - 0.05
    for t in (-1, 1):
        add(lambda: rod(ib, (-rail_x, t * 0.40, rail_z), (rail_x, t * 0.40, rail_z), 0.016), 4)
        for sx in (-1, 1):
            post = ray((sx * rail_x, t * 0.40, 1.2), (0, 0, 1))
            post_z = (post.z - WALL if post else rail_z + 0.1)
            add(lambda: rod(ib, (sx * rail_x, t * 0.40, rail_z), (sx * rail_x, t * 0.40, post_z), 0.012, 6), 4)
    inside = to_object("SM_TR_CabinInside", ib, [m_floor, m_seat, m_seat, m_glow, m_chrome])

    # --- sockets and collision (Blender frame, metres from the origin) -------------------------------------------
    wall_y = min(hull_y(x * 0.1, z, sg) for x in range(-12, 13) for z in (0.15, 1.0, 1.9) for sg in (1, -1)) - WALL
    end_in = inner_end(0.1, 1.9)
    top_in = min(roof_in, DOOR_H + 0.25)
    col = [
        {"name": "floor", "c": [0, 0, -0.05], "s": [end_in * 2, wall_y * 2 + 0.6, 0.1]},
        {"name": "roof", "c": [0, 0, top_in + 0.05], "s": [end_in * 2, wall_y * 2, 0.1]},
        {"name": "end+", "c": [end_in + 0.05, 0, top_in / 2], "s": [0.1, wall_y * 2, top_in]},
        {"name": "end-", "c": [-end_in - 0.05, 0, top_in / 2], "s": [0.1, wall_y * 2, top_in]},
        {"name": "side-", "c": [0, -wall_y - 0.05, top_in / 2], "s": [end_in * 2, 0.1, top_in]},
        {"name": "side+a", "c": [(-end_in + x0) / 2, wall_y + 0.05, top_in / 2], "s": [x0 + end_in, 0.1, top_in]},
        {"name": "side+b", "c": [(end_in + x1) / 2, wall_y + 0.05, top_in / 2], "s": [end_in - x1, 0.1, top_in]},
        {"name": "lintel", "c": [door_x, wall_y + 0.05, (DOOR_H + top_in) / 2], "s": [DOOR_W, 0.1, top_in - DOOR_H]},
    ]
    for sgn, b in bench.items():
        col.append({"name": "bench%+d" % sgn, "c": [b["x"], 0, (SEAT_H - 0.05) / 2], "s": [SEAT_D, b["half_w"] * 2, SEAT_H - 0.05]})
    for cbox in col:
        cbox["c"] = [round(v, 3) for v in cbox["c"]]
        cbox["s"] = [round(v, 3) for v in cbox["s"]]
    parts = {"SM_TR_Cabin": shell, "SM_TR_CabinInside": inside, "SM_TR_CabinHanger": hanger,
             "SM_TR_CabinDoor": leaf}
    bpy.data.objects.remove(src)
    lo, hi = bounds(shell)
    kit = {
        "origin": "floor centre under the grip",
        "length": round(hi.x - lo.x, 3), "width": round(hi.y - lo.y, 3), "height": round(hi.z - lo.z, 3),
        "belly": round(-lo.z, 3), "floor": 0.0, "body_top": round(hi.z, 3),
        "grip": round(grip.z, 3), "grip_along_rope": grip_along,
        "door": {"x": round(door_x, 3), "clear_w": DOOR_W, "clear_h": DOOR_H, "lintel_y": round(door_lintel, 3),
                 "sill_y": round(sill_y, 3), "side_y": round(side_y, 3), "slide": round(-math.copysign(DOOR_W + 0.04, door_x), 3),
                 "out": LEAF_OUT},
        "inside": {"wall_y": round(wall_y, 3), "end_x": round(end_in, 3), "headroom": round(roof_in, 3)},
        "sockets": {
            "grip": [round(v, 3) for v in grip],
            "hanger_foot": [round(foot_c.x, 3), round(foot_c.y, 3), round(arm_z, 3)],
            "lamp": [0.0, 0.0, round(roof_in - 0.1, 3)],
            "board": [round(door_x, 3), round(wall_y - 0.05, 3), round(DOOR_H + 0.12, 3)],
            "plate": [round(door_x + DOOR_W / 2 + 0.75, 3), round(side_y + 0.02, 3), 0.55],
            "seats": seats,
        },
        "collision": col,
        "tris": {k: tris(v) for k, v in parts.items()},
        "scale_from_raw": round(s, 4),
    }
    return parts, kit
