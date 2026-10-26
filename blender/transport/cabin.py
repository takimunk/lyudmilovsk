"""The cable-car cabin as a real room (docs/transport.md "The cabin"): the Hi3D egg (design/transport/gen/cabin.glb, one
closed 300k-triangle surface) cut and built into a cabin people ride inside, without any new generation:

  - the shell decimated, a door opening cut in its side with straight edges, the cut piece split into two leaves that
    slide apart (SM_TR_CabinDoorA/B);
  - its windows (dark, dull texels) taken out and glazed (TR_Glass, both faces), the rest given a wall: the inner face
    plaster (TR_Interior), the window and door reveals in the shell's own paint;
  - a flat terrazzo floor fitted to the hull, two padded benches facing each other at the ends (fitted to the hull by
    rays), a ceiling lamp, chrome grab rails (SM_TR_CabinInside);
  - the hanger arm and the grip as their own mesh (SM_TR_CabinHanger), bent so the grip is plumb over the floor's centre
    (a hanging cabin's weight is under its grip), the grip bar along the rope;
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
BUDGET = 7000            # shell triangles before the wall (which about doubles them)
HANGER_BUDGET = 1400
LEAF_BUDGET = 900
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


def outer_skin(bm):
    """Delete what is not the egg's outer skin (Hi3D leaves bits inside behind the windows): a face stays if a ray
    coming at it from outside along its normal, or from straight above or the side, meets it first."""
    bvh = BVHTree.FromBMesh(bm)
    bm.faces.ensure_lookup_table()
    bm.normal_update()
    kill = []
    for f in bm.faces:
        c = f.calc_center_median()
        seen = False
        for d in (f.normal, Vector((c.x, c.y, 0)).normalized(), Vector((0, 0, 1)), Vector((0, 0, -1))):
            if d.length < 0.5:
                continue
            hit = bvh.ray_cast(c + d * 6.0, -d, 7.0)
            if hit[2] is not None and (hit[2] == f.index or (hit[0] - c).length < 0.01):
                seen = True
                break
        if not seen:
            kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    return len(kill)


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


def leaf_lining(o):
    """The door's inside: a copy of the leaf relaxed flat (its edge kept), the wall's thickness in, faced inward, plaster
    (slot 1); a strip round the edge closes the gap between the two."""
    bm = bmesh.new()
    bm.from_mesh(o.data)
    edges = [e for e in bm.edges if e.is_boundary]
    dup = bmesh.ops.duplicate(bm, geom=list(bm.faces))
    vmap = dup["vert_map"]
    faces = [e for e in dup["geom"] if isinstance(e, bmesh.types.BMFace)]
    verts = list({v for f in faces for v in f.verts})
    inner = [v for v in verts if not v.is_boundary]
    for _ in range(12):
        bmesh.ops.smooth_vert(bm, verts=inner, factor=0.8, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    for v in verts:
        v.co.y -= WALL * 0.8
    bmesh.ops.reverse_faces(bm, faces=faces)
    for f in faces:
        f.material_index = 1
    for e in edges:
        a, b = e.verts
        try:
            f = bm.faces.new((b, a, vmap[a], vmap[b]))
            f.material_index = 0
        except (ValueError, KeyError):
            pass
    bm.normal_update()
    bm.to_mesh(o.data)
    bm.free()


def inner_lining(hull_bm, shell, m_room, centre=Vector((0, 0, 1.1)), rings=48, segs=96):
    """The room's wall: not the shell turned inward (it would repeat every trim and crease of the generated outside) but
    a smooth skin of its own: a sphere round the room's middle with every vertex pushed out along its ray to the egg's
    inside (the egg is star-shaped from there), the wall's thickness in, relaxed; faced inward. Where the shell has a
    window or the door (no shell right outside a face) the skin is cut away; under the floor too. The gap at the
    openings' edges shows the shell's back (its paint is two-sided): a dark seal round the glass."""
    hb = BVHTree.FromBMesh(hull_bm)
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0, matrix=Matrix.Translation(centre))
    for v in bm.verts:
        d = (v.co - centre).normalized()
        hit = hb.ray_cast(centre, d, 6.0)
        v.co = (hit[0] - d * WALL) if hit[0] is not None else centre + d * 1.0
    for _ in range(2):
        bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.normal_update()
    sm = bmesh.new()
    sm.from_mesh(shell.data)
    sbvh = BVHTree.FromBMesh(sm)
    kill = []
    for f in bm.faces:
        c = f.calc_center_median()
        out = (c - centre).normalized()
        if c.z < -0.03 or sbvh.ray_cast(c, out, WALL * 4)[0] is None:
            kill.append(f)
    bmesh.ops.delete(bm, geom=kill, context="FACES")
    # the cut's stair-stepped edge onto the opening's own edge in the shell (sampled every 2 cm), a little in
    from mathutils.kdtree import KDTree
    pts = []
    for e in sm.edges:
        if e.is_boundary:
            a, b = e.verts[0].co, e.verts[1].co
            k = max(1, int((b - a).length / 0.02))
            pts += [a.lerp(b, i / k) for i in range(k + 1)]
    kd = KDTree(len(pts))
    for i, q in enumerate(pts):
        kd.insert(q, i)
    kd.balance()
    for v in bm.verts:
        if v.is_boundary and v.co.z > 0.02:
            q, _i, dist = kd.find(v.co)
            if q is not None and dist < 0.15:
                v.co = q - (q - centre).normalized() * WALL
    sm.free()
    # faced inward: the sphere's normals point out
    bm.normal_update()
    bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    o = to_object("lining", bm, [m_room])
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def smooth(o, angle=40):
    for p in o.data.polygons:
        p.use_smooth = True
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


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
    # where the body ends and the arm starts: going up from the middle, the first slice much narrower than the body
    ext = {}
    for v in o.data.vertices:
        k = round(v.co.z, 1)
        a, b = ext.get(k, (1e9, -1e9))
        ext[k] = (min(a, v.co.y), max(b, v.co.y))
    wide = hi.y - lo.y
    arm_z = min(k for k, (a, b) in ext.items() if k > (hi.z + lo.z) * 0.3 and b - a < wide * 0.2) - 0.05
    body_top = arm_z
    tex_w, tex_h, px = texels(o)
    painted = painted_door(o, (tex_w, tex_h, px))
    decimate(o, BUDGET + HANGER_BUDGET)
    shell_mat = o.data.materials[0]

    bm = bmesh.new()
    bm.from_mesh(o.data)
    ul = bm.loops.layers.uv.active
    n_inner = outer_skin(bm)
    bvh = BVHTree.FromBMesh(bm)

    def ray(p, d, far=6.0):
        hit = bvh.ray_cast(Vector(p), Vector(d).normalized(), far)
        return hit[0]

    # the hull from outside in (rays from the middle would stop on anything left inside)
    def hull_y(x, z, sgn=1):
        h = ray((x, sgn * 5.0, z), (0, -sgn, 0), 10.0)
        return abs(h.y) if h else 0.0

    def hull_x(z, sgn, y=0.0):
        h = ray((sgn * 5.0, y, z), (-sgn, 0, 0), 10.0)
        return abs(h.x) if h else 0.0

    # --- the door: where the side stays tallest, so its top edge cuts the side and not the roof ----------------
    # the door: on the painted one (its frame stripes in the texture), wide enough for the standard
    door_x = painted if painted is not None else 0.0
    door_lintel = min(hull_y(door_x + sx * DOOR_W / 2, DOOR_H + 0.10) for sx in (-1, 1))
    x0, x1 = door_x - DOOR_W / 2, door_x + DOOR_W / 2
    sill_y = hull_y(door_x, 0.0)
    side_y = max(hull_y(door_x, z * 0.1) for z in range(1, 21))
    # measurements for the inside, on the closed hull
    end_x = {round(z * 0.05, 2): min(hull_x(z * 0.05, 1), hull_x(z * 0.05, -1)) for z in range(0, 45)}
    floor_ring = []
    for i in range(64):
        a = 2 * math.pi * i / 64
        h = ray((5 * math.cos(a), 5 * math.sin(a), -0.02), (-math.cos(a), -math.sin(a), 0), 10.0)
        if h:
            p = Vector((h.x, h.y, 0.0))
            floor_ring.append(p - p.normalized() * (WALL + 0.005))
    floor_ring = convex(floor_ring)
    roof_c = 2.6
    for hz in (ray((x * 0.1, y * 0.1, 1.2), (0, 0, 1)) for x in (-3, 0, 3) for y in (-2, 0, 2)):
        if hz:
            roof_c = min(roof_c, hz.z)

    # --- the hanger: everything over the lantern ---------------------------------------------------------------
    arm = [f for f in bm.faces if f.calc_center_median().z > arm_z]
    hb = copy_faces(bm, arm, ul)
    bmesh.ops.delete(bm, geom=arm, context="FACES")
    gv = [v for v in hb.verts if v.co.z > hi.z - 0.5]
    glo = Vector([min(v.co[i] for v in gv) for i in range(3)])
    ghi = Vector([max(v.co[i] for v in gv) for i in range(3)])
    grip = (glo + ghi) / 2
    foot = [v.co for v in hb.verts if v.co.z < arm_z + 0.08]
    foot_c = sum(foot, Vector()) / max(1, len(foot))
    # bend the arm so the grip comes plumb over the floor's centre: a smooth ramp from the foot up
    shift = Vector((-grip.x, -grip.y, 0.0))
    for v in hb.verts:
        t = min(1.0, max(0.0, (v.co.z - arm_z) / max(0.01, glo.z - arm_z)))
        v.co += shift * (t * t * (3 - 2 * t))
    grip += shift
    grip_along = (ghi.x - glo.x) >= (ghi.y - glo.y)

    hull = bm.copy()                             # the closed egg, for the inner lining
    # --- cut the door: planes through the side, then the piece inside them becomes the leaf ----------------
    def side_geom():
        fs = [f for f in bm.faces if f.calc_center_median().y > 0.25]
        return list({e for f in fs for e in f.edges}) + fs + list({v for f in fs for v in f.verts})

    for co, no in (((x0, 0, 0), (1, 0, 0)), ((x1, 0, 0), (1, 0, 0)),
                   ((0, 0, DOOR_H), (0, 0, 1)), ((0, 0, 0.0), (0, 0, 1))):
        bmesh.ops.bisect_plane(bm, geom=side_geom(), plane_co=co, plane_no=no)
    door = [f for f in bm.faces if f.calc_center_median().y > 0.25 and x0 < f.calc_center_median().x < x1
            and 0.0 < f.calc_center_median().z < DOOR_H]
    leaf_bm = copy_faces(bm, door, ul)
    bmesh.ops.delete(bm, geom=door, context="FACES")

    # --- windows out of the texture: dark, dull texels on the body's sides --------------------------------------
    def is_glass(f):
        u = sum(l[ul].uv[0] for l in f.loops) / len(f.loops)
        v = sum(l[ul].uv[1] for l in f.loops) / len(f.loops)
        x, y = min(tex_w - 1, max(0, int(u % 1.0 * tex_w))), min(tex_h - 1, max(0, int(v % 1.0 * tex_h)))
        k = (y * tex_w + x) * 4
        r, g, b = px[k], px[k + 1], px[k + 2]
        top, low = max(r, g, b), min(r, g, b)
        sat = (top - low) / top if top > 1e-4 else 0.0
        return top < GLASS_DARK and sat < GLASS_SAT and 0.35 < f.calc_center_median().z < body_top - 0.3

    flag = {f: is_glass(f) for f in bm.faces}
    n_glass0 = sum(flag.values())
    majority(bm, flag)
    print("CABIN glass faces", n_glass0, "->", sum(flag.values()), "of", len(flag), "door faces", len(door), "arm_z", arm_z, "inner faces cut", n_inner)
    glass = [f for f in bm.faces if flag[f]]
    gb = copy_faces(bm, glass, ul)
    bmesh.ops.delete(bm, geom=glass, context="FACES")
    # glazed both ways, a little in from the outer skin
    gb.normal_update()
    for v in gb.verts:
        v.co -= v.normal * (WALL * 0.4)
    back = bmesh.ops.duplicate(gb, geom=list(gb.faces))
    bmesh.ops.reverse_faces(gb, faces=[e for e in back["geom"] if isinstance(e, bmesh.types.BMFace)])
    for f in gb.faces:
        f.material_index = 0

    # --- objects -------------------------------------------------------------------------------------------------
    m_glass = mat("TR_Glass", (0.3, 0.4, 0.45, 0.25))
    m_room = mat("TR_Interior", (0.86, 0.82, 0.74, 1))
    m_floor = mat("TR_Terrazzo", (0.60, 0.56, 0.50, 1), 0.45)
    m_seat = mat("TR_Seat", (0.62, 0.26, 0.06, 1), 0.5)
    m_chrome = mat("TR_Chrome", (0.85, 0.85, 0.87, 1), 0.2, 1.0)
    m_glow = mat("TR_Glow", (1.0, 0.82, 0.55, 1), emit=True)

    for f in bm.faces:
        f.material_index = 0
    shell = to_object("SM_TR_Cabin", bm, [shell_mat, m_room])
    lining = inner_lining(hull, shell, m_room)
    gl = to_object("glass", gb, [m_glass])
    bpy.ops.object.select_all(action="DESELECT")
    for ob in (shell, lining, gl):
        ob.select_set(True)
    bpy.context.view_layer.objects.active = shell
    bpy.ops.object.join()                       # one mesh, slots: paint, TR_Interior, TR_Glass

    for f in leaf_bm.faces:
        f.material_index = 0
    leaf = to_object("SM_TR_CabinDoor", leaf_bm, [shell_mat, m_room])
    decimate(leaf, LEAF_BUDGET)
    leaf_lining(leaf)

    hanger = to_object("SM_TR_CabinHanger", hb, [shell_mat])
    decimate(hanger, HANGER_BUDGET)
    for ob in (shell, leaf, hanger):
        smooth(ob)

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
