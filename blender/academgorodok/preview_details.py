"""Headless preview of details.py.

Run from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P blender/academgorodok/preview_details.py
Optional: append `-- nore` to only build and validate (no rendering), or `-- <shot> ...` to
render only some shots (overview, novikova_bay, gable, corner, gate, windows, courtyard).

Builds every detail, lays them out in a row on a ground plane, adds a sun and a camera and renders
EEVEE previews (1600 x 900) to renders/academgorodok/details_*.png, including close-ups that
recreate the subjects of the reference photos in docs/academgorodok/refs/exterior/.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import details as D  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "renders", "academgorodok")
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
NO_RENDER = "nore" in ARGS
ONLY = [a for a in ARGS if a != "nore"]


# ------------------------------------------------------------------------------------------
# scene helpers

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900
    sc.render.resolution_percentage = 100
    try:
        sc.eevee.taa_render_samples = 24
    except AttributeError:
        pass
    for attr in ("use_shadows", "use_raytracing"):
        if hasattr(sc.eevee, attr):
            try:
                setattr(sc.eevee, attr, attr == "use_shadows")
            except Exception:
                pass
    sc.view_settings.view_transform = 'AgX' if 'AgX' in [i.identifier for i in
                                                          sc.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Filmic'
    w = bpy.data.worlds.new("W")
    sc.world = w
    nt = w.node_tree
    bg = next(n for n in nt.nodes if n.type == 'BACKGROUND')
    bg.inputs["Color"].default_value = (0.45, 0.62, 0.9, 1.0)
    bg.inputs["Strength"].default_value = 0.9
    return sc


def sun(rot=(50, 0, 140), energy=4.5):
    ld = bpy.data.lights.new("sun", 'SUN')
    ld.energy = energy
    ld.angle = math.radians(1.5)
    ob = bpy.data.objects.new("sun", ld)
    ob.rotation_euler = [math.radians(a) for a in rot]
    bpy.context.scene.collection.objects.link(ob)
    # weak opposite fill light standing in for bounce light under soffits and slabs
    fd = bpy.data.lights.new("fill", 'SUN')
    fd.energy = energy * 0.22
    fill = bpy.data.objects.new("fill", fd)
    fill.rotation_euler = (math.radians(180 - rot[0] + 10), 0, math.radians(rot[2] + 180))
    bpy.context.scene.collection.objects.link(fill)
    return ob


def camera(loc, target, lens=35.0, ortho=None):
    cd = bpy.data.cameras.new("cam")
    cd.lens = lens
    cd.clip_end = 500
    if ortho:
        cd.type = 'ORTHO'
        cd.ortho_scale = ortho
    ob = bpy.data.objects.new("cam", cd)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = loc
    d = Vector(target) - Vector(loc)
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = ob
    return ob


def ground(size=200, mat="AC_Asphalt", z=0.0):
    me = bpy.data.meshes.new("ground")
    s = size / 2
    me.from_pydata([(-s, -s, z), (s, -s, z), (s, s, z), (-s, s, z)], [], [(0, 1, 2, 3)])
    me.materials.append(D.get_material(mat))
    ob = bpy.data.objects.new("ground", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def place(ob, loc=(0, 0, 0), rz=0.0):
    ob.location = loc
    ob.rotation_euler = (0, 0, math.radians(rz))
    return ob


def wall(x0, x1, z0, z1, holes=(), t=0.5, mat="AC_Wall", y=0.0, rz=0.0, origin=(0, 0, 0)):
    """Wall slab (face at local y = 0, back at -t) with rectangular or arched holes cut by an
    exact boolean (preview only). holes: (xc, z0, w, h, arched)."""
    bpy.ops.mesh.primitive_cube_add(size=1)
    w = bpy.context.active_object
    w.scale = (x1 - x0, t, z1 - z0)
    w.location = ((x0 + x1) / 2, y - t / 2, (z0 + z1) / 2)
    bpy.ops.object.transform_apply(location=True, scale=True)
    w.data.materials.append(D.get_material(mat))
    for (xc, hz, hw, hh, arched) in holes:
        if arched:
            r = hw / 2
            pts = [(-r, 0.0), (r, 0.0)] + [(r * math.cos(math.pi * k / 24), hh - r + r * math.sin(math.pi * k / 24))
                                          for k in range(25)]
        else:
            pts = [(-hw / 2, 0), (hw / 2, 0), (hw / 2, hh), (-hw / 2, hh)]
        n = len(pts)
        vs = [(xc + x, y + 0.1, hz + z) for (x, z) in pts] + [(xc + x, y - t - 0.1, hz + z) for (x, z) in pts]
        fs = [tuple(range(n))[::-1], tuple(range(n, 2 * n))] + [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
        me = bpy.data.meshes.new("cut")
        me.from_pydata(vs, [], fs)
        me.update()
        cut = bpy.data.objects.new("cut", me)
        bpy.context.scene.collection.objects.link(cut)
        md = w.modifiers.new("b", 'BOOLEAN')
        md.operation = 'DIFFERENCE'
        md.solver = 'EXACT'
        md.object = cut
        bpy.context.view_layer.objects.active = w
        bpy.ops.object.modifier_apply(modifier=md.name)
        bpy.data.objects.remove(cut)
    w.data.transform(Matrix.Translation(-Vector(origin)))
    w.location = origin
    w.rotation_euler = (0, 0, math.radians(rz))
    return w


def render(name):
    path = os.path.join(OUT, "details_%s.png" % name)
    if NO_RENDER:
        print("PREVIEW skip render", path)
        return
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("PREVIEW wrote", path)


def want(name):
    return not ONLY or name in ONLY


def M(name, **kw):
    ob = D.make(name, **kw)
    assert ob is not None, name
    return ob


# ------------------------------------------------------------------------------------------
# shots

def shot_overview():
    reset()
    ground(400, "AC_Concrete")
    sun((52, 0, 150))
    items = [
        ("window", dict(w=1.4, h=1.9, unit="double")), ("window", dict(w=1.9, h=1.95, unit="triple")),
        ("window", dict(w=1.6, h=2.6, unit="french_door")), ("window", dict(w=2.6, h=2.95, unit="palladian")),
        ("window", dict(w=2.2, h=2.6, unit="shop")), ("window", dict(w=2.3, h=1.85, unit="paired")),
        ("door", dict(w=1.5, h=2.6, unit="double")), ("door", dict(w=1.3, h=2.6, unit="arched")),
        ("surround", dict(w=1.4, h=1.9, style="band")), ("surround", dict(w=1.4, h=1.9, style="ears")),
        ("surround", dict(w=1.4, h=1.9, style="hood")), ("surround", dict(w=1.4, h=1.9, style="hood_ears")),
        ("surround", dict(w=1.4, h=1.9, style="pediment")), ("surround", dict(w=1.8, h=2.6, style="arch")),
        ("portal_dark", dict(w=1.6, h=2.6)), ("portal_side", dict(w=1.3, h=2.6)),
        ("pilaster", dict(h=3.5, w=0.5)), ("column", dict(h=3.5, d=0.26)), ("parapet", dict(length=3.0, h=1.0)),
        ("railing", dict(length=2.0, h=1.0, style="bars")), ("railing", dict(length=2.4, h=1.0, style="ornamental")),
        ("bay_window", dict(w=4.2, depth=1.1, h=8.0)), ("vent", dict(s=0.3)),
        ("chimney", dict(h=2.2)), ("downpipe", dict(h=6.0)), ("cartouche", {}),
        ("scroll_pylon", dict(flip=False)), ("scroll_pylon", dict(flip=True)), ("wicket_block", {}),
        ("fountain", {}),
    ]
    small = [("console_big", dict(h=1.4)), ("modillion", dict(depth=0.95)), ("rafter", dict(length=1.1)),
             ("oculus", dict(d=0.95)), ("swag", dict(length=1.3)), ("quoin", dict(courses=11, course_h=0.5)),
             ("steps", dict(w=2.2, n=2, rise=0.15, tread=0.35)), ("slab", dict(w=3.0, depth=1.0, t=0.25)),
             ("balcony", dict(length=4.5, depth=1.2, style="ornamental", consoles=True)),
             ("railing_side", dict(length=1.2, h=1.0))]
    rows, cur, cw = [], [], 0.0
    for it in items + small:
        ob = M(it[0], **it[1])
        xs = [v.co.x for v in ob.data.vertices]
        w = max(xs) - min(xs) + 0.6
        if cw + w > 26.0 and cur:
            rows.append(cur)
            cur, cw = [], 0.0
        cur.append((ob, w, (max(xs) + min(xs)) / 2, min(v.co.z for v in ob.data.vertices)))
        cw += w
    rows.append(cur)
    for r, row in enumerate(rows):
        x = -13.0
        for ob, w, xc, zmin in row:
            place(ob, (x + w / 2 - xc, -r * 7.0, max(0.0, -zmin) + (0.1 if zmin < -0.01 else 0.0)))
            x += w
    fr = M("flowerbed_railing", radius=5.7)
    place(fr, (-21.0, -12.0, 0))
    x = 0.0
    camera((-3.0, 19.0, 11.0), (-3.0, -8.0, 1.0), lens=24)
    render("overview")
    return x


def shot_novikova_bay():
    """Ground floor + 2nd floor of the central loggia bay of Novikova 1 seen from the fountain,
    like commons_Marshal_Novikov_str._1_2_.jpg."""
    reset()
    ground(200, "AC_Asphalt")
    sun((58, 0, 115), 5.0)
    P = 0.55                         # door sill over the ground (plinth)
    F = 3.8
    holes = [(0.0, P, 1.6, 2.6, False), (-4.3, P, 1.5, 2.6, False), (4.3, P, 1.5, 2.6, False),
             (-7.6, P + 0.05, 2.2, 2.4, False), (7.6, P + 0.05, 2.2, 2.4, False),
             (0.0, P + F, 2.0, 2.6, False), (-4.3, P + F + 0.9, 1.4, 1.9, False), (4.3, P + F + 0.9, 1.4, 1.9, False)]
    wall(-10, 10, 0, 2 * F + 1.5, holes)
    # plinth band
    pl = D.cornice_run(D.plinth_profile_points(), 20.0, "AC_Plinth", returns=False)
    place(pl, (0, 0, -0.15))
    for (xc, z0, w, h, _a) in holes:
        if (xc, z0) == (0.0, P):
            place(M("door", w=w, h=h, unit="double"), (xc, 0, z0))
        elif abs(xc) == 4.3 and z0 == P:
            place(M("door", w=w, h=h, unit="double"), (xc, 0, z0))
            place(M("surround", w=w, h=h, style="band"), (xc, 0, z0))
            place(M("steps", w=2.0, n=2, rise=P / 2, tread=0.35), (xc, 0, 0))
        elif abs(xc) == 7.6:
            place(M("window", w=w, h=h, unit="shop"), (xc, 0, z0))
            place(M("surround", w=w, h=h, style="band"), (xc, 0, z0))
        elif z0 == P + F:
            place(M("window", w=w, h=h, unit="french_door"), (xc, 0, z0))
        else:
            place(M("window", w=w, h=h, unit="double"), (xc, 0, z0))
            place(M("surround", w=w, h=h, style="band"), (xc, 0, z0))
    # the loggia bay
    place(M("portal_dark", w=1.6, h=2.6), (0, 0, P))
    place(M("steps", w=2.6, n=2, rise=P / 2, tread=0.35), (0, 0.0, 0))
    for i in range(4):
        place(M("console_big", h=1.4), (-1.95 + i * 1.3, 0, P + F - 0.25))
    place(M("slab", w=5.4, depth=1.3, t=0.25), (0, 0, P + F - 0.25))
    place(M("parapet", length=4.6, h=1.0), (0, 1.13, P + F))
    for s in (-1, 1):
        place(M("pilaster", h=F - 0.25, w=0.5), (s * 2.4, 0.85, P + F))
    place(M("slab", w=5.4, depth=1.3, t=0.25), (0, 0, P + 2 * F - 0.25))
    for s in (-1, 1):
        place(M("column", h=1.2, d=0.26), (s * 0.9, 1.05, P + F + 1.0))
    # fountain + flowerbed in front (bed centre ~14 m out)
    place(M("fountain"), (0, 14.0, 0))
    place(M("flowerbed_railing", radius=5.7), (0, 14.0, 0))
    soil = bpy.data.meshes.new("soil")
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_circle(bm, cap_ends=True, segments=64, radius=5.6)
    bm.to_mesh(soil)
    soil.materials.append(D.get_material("AC_Soil"))
    so = bpy.data.objects.new("soil", soil)
    bpy.context.scene.collection.objects.link(so)
    so.location = (0, 14.0, 0.12)
    camera((0, 26.0, 1.6), (0, 0, 4.2), lens=26)
    render("novikova_bay")
    # close-up of the portal, consoles and fountain (photo crop 'portal')
    camera((0.6, 9.5, 1.7), (0, 0, 2.6), lens=40)
    render("novikova_portal")


def shot_gable():
    """Gable of the loggia bay: oculus with louvres, two swags, modillions under the eaves of
    the neighbouring wall, the Serliana window below."""
    reset()
    ground(200, "AC_Asphalt", z=-20)
    sun((50, 0, 120), 5.0)
    w = wall(-9, 9, 0, 3.4, [(0.0, -3.1, 2.6, 2.95, False)], mat="AC_Wall")
    # gable triangle
    import bmesh
    me = bpy.data.meshes.new("gable")
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in ((-3.0, 0, 3.4), (3.0, 0, 3.4), (0, 0, 6.2))]
    vb = [bm.verts.new(p) for p in ((-3.0, -0.5, 3.4), (3.0, -0.5, 3.4), (0, -0.5, 6.2))]
    bm.faces.new(vs)
    bm.faces.new(vb[::-1])
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((vs[i], vb[i], vb[j], vs[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    me.materials.append(D.get_material("AC_Wall"))
    g = bpy.data.objects.new("gable", me)
    bpy.context.scene.collection.objects.link(g)
    wall(-9, 9, -3.4, 0.0, [(0.0, -3.4 + 0.3, 2.6, 2.95, False)])
    place(M("window", w=2.6, h=2.95, unit="palladian"), (0, 0, -3.1))
    place(M("oculus", d=0.95), (0, 0, 4.25))
    for s in (-1, 1):
        place(M("swag", length=1.2), (s * 1.45, 0, 4.0))
    # eaves cornice + modillions either side of the bay
    for s in (-1, 1):
        cr = D.cornice_run(D.eaves_cornice_profile_points(), 6.0, returns=False)
        place(cr, (s * 6.0, 0, 3.4 - D.EAVES_SOFFIT_Z))
        for k in range(10):
            x = s * (3.3 + 0.6 * k)
            place(M("modillion", depth=0.5), (x, D.EAVES_MODILLION_Y, 3.4))
    place(M("downpipe", h=6.0), (3.15, 0.12, -2.6))
    camera((0, 14.0, 0.5), (0, 0, 3.0), lens=38)
    render("gable")


def shot_corner():
    """Gate-end corner of Maksimova 8: quoins, 1947 cartouche, rafter-tail eaves, chimney,
    downpipe, windows with hood cornices (commons_Maksimova_str._8_1_.jpg)."""
    reset()
    ground(200, "AC_Asphalt")
    sun((55, 0, 200), 5.0)
    P, F = 0.6, 3.8
    ez = P + 3 * F
    # building occupies x<0, y<0. Face A: y = 0 (x<0). Face B: x = 0 (y<0).
    holesA = [(-3.0, P + 0.9 + k * F, 1.3, 1.85, False) for k in range(3)]
    wall(-12, 0, 0, ez, holesA)
    holesB = [(2.6, P + 0.9 + k * F, 1.3, 1.85, False) for k in range(3)]
    # face B wall in its own frame: x along -y, then rotated -90 about z
    wb = wall(0, 12, 0, ez, holesB)
    wb.data.transform(Matrix.Rotation(-math.pi / 2, 4, 'Z'))
    for k in range(3):
        for (xc, z0, w, h, _a), rz, pos in (((-3.0, P + 0.9 + k * F, 1.3, 1.85, 0), 0, lambda x: (x, 0)),
                                           ((2.6, P + 0.9 + k * F, 1.3, 1.85, 0), -90, lambda x: (0, -x))):
            px, py = pos(xc)
            place(M("window", w=1.3, h=1.85, unit="double"), (px, py, z0), rz)
            style = "hood" if k < 2 else "band"
            place(M("surround", w=1.3, h=1.85, style=style), (px, py, z0), rz)
    q = M("quoin", courses=11, course_h=0.5)
    place(q, (0, 0, P - 0.1))
    c = M("cartouche")
    place(c, (0, 0, P - 0.1 + 11 * 0.5), -45)
    # eaves: rafter tails on both faces, boarded soffit, roof slab
    for k in range(10):
        place(M("rafter", length=1.1), (-0.6 - 1.1 * k, 0, ez), 0)
        place(M("rafter", length=1.1), (0, -0.6 - 1.1 * k, ez), -90)
    import bmesh
    me = bpy.data.meshes.new("roof")
    bm = bmesh.new()
    o = 1.15
    pts = [(-12, o, ez), (o, o, ez), (o, -12, ez), (-12, -12, ez + 0.0)]
    top = [(-12, -4.0, ez + 2.5), (-4.0, -4.0, ez + 2.5), (-4.0, -12, ez + 2.5)]
    v = [bm.verts.new(p) for p in pts]
    t = [bm.verts.new(p) for p in top]
    bm.faces.new((v[0], v[1], t[1], t[0]))
    bm.faces.new((v[1], v[2], t[2], t[1]))
    bm.to_mesh(me)
    me.materials.append(D.get_material("AC_RoofBrown"))
    ro = bpy.data.objects.new("roof", me)
    bpy.context.scene.collection.objects.link(ro)
    place(M("chimney", h=2.0), (-5.0, -6.0, ez + 1.6))
    place(M("downpipe", h=ez), (-1.3, 0.12, 0))
    camera((9.5, 9.5, 1.6), (-1.0, -1.0, 6.5), lens=24)
    render("corner")
    camera((3.0, 3.0, 6.0), (0, 0, 6.5), lens=40)
    render("cartouche")


def shot_gate():
    """N gate: wicket block on the side house, two scroll pylons (commons_Marshal_Novikov_str._1_gates.jpg)."""
    reset()
    ground(200, "AC_Asphalt")
    sun((50, 0, 200), 5.0)
    wall(-8, -1.9, 0, 12.0, [], mat="AC_Wall")
    place(M("wicket_block"), (0.0, -0.4, 0))
    place(M("scroll_pylon", flip=False), (4.3, -0.4, 0))
    place(M("scroll_pylon", flip=True), (8.6, -0.4, 0))
    w2 = wall(10.5, 25, 0, 15.5, [(12.5, 0.6, 1.2, 2.6, True)], y=-3.0)
    place(M("door", w=1.2, h=2.6, unit="arched"), (12.5, -3.0, 0.6))
    place(M("surround", w=1.2, h=2.6, style="arch"), (12.5, -3.0, 0.6))
    camera((6.5, 13.0, 1.7), (5.5, -1.0, 2.0), lens=24)
    render("gate")
    camera((7.0, 5.0, 1.5), (8.6, -0.4, 1.5), lens=40)
    render("pylon")


def shot_windows():
    """Side-house street front after the 1946 elevation: arched portal with voussoirs, balcony on
    consoles, pediment and hood_ears windows; plus a paired window and the bay window."""
    reset()
    ground(200, "AC_Asphalt")
    sun((48, 0, 150), 5.0)
    P, F = 0.6, 3.8
    holes = [(0.0, P - 0.15, 1.3, 2.6, True),
             (-4.2, P + 0.9, 1.4, 1.9, False), (4.2, P + 0.9, 1.4, 1.9, False),
             (0.0, P + F + 0.9, 1.4, 1.9, False), (-4.2, P + F + 0.9, 1.4, 1.9, False), (4.2, P + F + 0.9, 1.4, 1.9, False)]
    wall(-7, 7, 0, P + 2 * F + 0.5, holes)
    for (xc, z0, w, h, a) in holes:
        if a:
            place(M("door", w=w, h=h, unit="arched"), (xc, 0, z0))
            place(M("portal_side", w=w, h=h), (xc, 0, z0))
        else:
            place(M("window", w=w, h=h, unit="triple"), (xc, 0, z0))
            st = "pediment" if (xc == 0.0) else "hood_ears"
            place(M("surround", w=w, h=h, style=st), (xc, 0, z0))
    place(M("balcony", length=3.0, depth=0.9, style="ornamental", consoles=False), (0, 0, P + F - 0.0))
    for s in (-1, 1):
        place(M("console_big", h=1.2), (s * 1.15, 0, P + F - 0.2))
    place(M("steps", w=2.2, n=2, rise=(P - 0.15) / 2 + 0.07, tread=0.35), (0, 0, -0.14))
    camera((1.0, 15.0, 2.0), (0, 0, 3.8), lens=30)
    render("windows")
    # paired window + bay window
    reset()
    ground(200, "AC_Asphalt")
    sun((48, 0, 140), 5.0)
    wall(-8, 8, 0, 9.0, [(-4.0, P + 0.9, 2.3, 1.85, False), (-4.0, P + F + 0.9, 2.3, 1.85, False)])
    for k in range(2):
        place(M("window", w=2.3, h=1.85, unit="paired"), (-4.0, 0, P + 0.9 + k * F))
    place(M("bay_window", w=4.2, depth=1.1, h=2 * F + 0.4), (3.0, 0, P - 0.1))
    place(M("railing", length=4.0, h=1.0, style="bars"), (3.0, 1.1, P - 0.1 + 2 * F + 0.4))
    place(M("vent", s=0.3), (0.0, 0, 5.0))
    camera((0.0, 14.0, 2.5), (0, 0, 3.6), lens=28)
    render("courtyard")


SHOTS = [("overview", shot_overview), ("novikova_bay", shot_novikova_bay), ("gable", shot_gable),
         ("corner", shot_corner), ("gate", shot_gate), ("windows", shot_windows)]


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, fn in SHOTS:
        if want(name):
            fn()
    # triangle report
    reset()
    print("PREVIEW triangle counts:")
    for name, fn in D.ALL:
        ob = fn()
        print("PREVIEW  %-22s %7d tris" % (name, ob["tris"]))


main()
