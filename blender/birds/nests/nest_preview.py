"""Headless preview renders of the nest kit (EEVEE), contact sheets via blender/birds/contact_sheet.py."""
import math
import os
import subprocess

import bpy
import numpy as np
from mathutils import Vector

import bird_common as C

BLENDER = bpy.app.binary_path
CONTACT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "contact_sheet.py")


def setup(res):
    sc = bpy.context.scene
    C.set_engine(sc)
    sc.render.resolution_x = sc.render.resolution_y = res
    sc.render.film_transparent = False
    try:
        sc.eevee.taa_render_samples = 24
    except Exception:
        pass
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("W")
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.42, 0.45, 0.5, 1)
    bg.inputs[1].default_value = 0.9
    sc.world = w
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.2
    sun.data.angle = 0.1
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    sc.collection.objects.link(sun)
    fill = bpy.data.objects.new("Fill", bpy.data.lights.new("Fill", "SUN"))
    fill.data.energy = 1.0
    fill.rotation_euler = (math.radians(60), 0, math.radians(140))
    sc.collection.objects.link(fill)
    gm = bpy.data.materials.new("Ground")
    gm.diffuse_color = (0.18, 0.2, 0.15, 1)
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.16, 0.18, 0.12, 1)
    gm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.0))
    g = bpy.context.object
    g.name = "_ground"
    g.data.materials.append(gm)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


def look(cam, pos, target, lens=50):
    cam.location = Vector(pos)
    d = Vector(target) - Vector(pos)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    cam.data.clip_start = 0.005
    cam.data.clip_end = 50


def show(objs, names):
    names = set(names)
    g = bpy.data.objects.get("_ground")
    if g is not None:
        zs = [min(v.co.z for v in objs[n].data.vertices) for n in names if n in objs and not n.startswith("_egg")]
        g.location.z = (min(zs) if zs else 0.0) - 0.0005
    for n, o in objs.items():
        o.hide_render = n not in names
        o.hide_viewport = n not in names


def extent(ob):
    co = np.array([v.co[:] for v in ob.data.vertices])
    return co.min(0), co.max(0)


def shoot(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def orbit(cam, target, dist, elev_deg, az_deg, lens=50):
    e, a = math.radians(elev_deg), math.radians(az_deg)
    pos = (target[0] + dist * math.cos(e) * math.cos(a), target[1] + dist * math.cos(e) * math.sin(a), target[2] + dist * math.sin(e))
    look(cam, pos, target, lens)


def sheet(out, cols, cw, files):
    subprocess.run([BLENDER, "-b", "--factory-startup", "--python", CONTACT, "--", out, str(cols), str(cw)] + files,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)


def pack_eggs(rng, n, L, B, rf):
    """positions (x, y, yaw) of n egg ellipses (a = L/2, b = B/2, metres) inside radius rf, no overlaps"""
    a, b = L / 2000.0, B / 2000.0
    got = []

    def inside(p, q):   # does ellipse q (x,y,yaw) overlap ellipse p?  sample q's rim and centre against p
        for t in np.linspace(0, 2 * math.pi, 16, endpoint=False):
            ex, ey = a * 0.98 * math.cos(t), b * 0.98 * math.sin(t)
            wx = q[0] + ex * math.cos(q[2]) - ey * math.sin(q[2])
            wy = q[1] + ex * math.sin(q[2]) + ey * math.cos(q[2])
            dx, dy = wx - p[0], wy - p[1]
            lx = dx * math.cos(p[2]) + dy * math.sin(p[2])
            ly = -dx * math.sin(p[2]) + dy * math.cos(p[2])
            if (lx / a) ** 2 + (ly / b) ** 2 < 1.0:
                return True
        return False
    rf_try = rf
    for attempt in range(3):
        got = []
        tries = 0
        while len(got) < n and tries < 6000:
            tries += 1
            rho = (rf_try - b * 1.0) * math.sqrt(rng.random()) if n > 1 else 0
            ph = rng.uniform(0, 6.283)
            yaw = rng.uniform(0, 6.283) if n > 1 else 0.0
            q = (rho * math.cos(ph), rho * math.sin(ph), yaw)
            # whole egg inside the flat floor disc
            ext = math.hypot(a * math.cos(yaw), b * math.sin(yaw))
            if math.hypot(q[0], q[1]) + ext > rf_try:
                continue
            if any(inside(p, q) or inside(q, p) for p in got):
                continue
            got.append(q)
        if len(got) >= n:
            break
        rf_try *= 1.12
    return got, rf_try


def run(objs, table, out, only=None, mat=None):
    import json
    pdir = os.path.join(out, "previews")
    os.makedirs(pdir, exist_ok=True)
    cam = setup(480)
    stages = ("B1", "B2", "Full", "Old")
    rng = np.random.default_rng(7)
    species = [s for s in table if f"SM_Nest_{s}_Full" in objs]
    fulls, graze, eggshots = [], [], []
    for sid in species:
        R = table[sid]["radius"]
        names = [f"SM_Nest_{sid}_{s}" for s in stages]
        files = []
        for st, nm in zip(stages, names):
            show(objs, [nm])
            lo, hi = extent(objs[nm])
            tz = max(0.0, (hi[2] + lo[2]) / 2 * 0.5)
            orbit(cam, (0, 0, tz), max(R * 4.4, 0.25), 35, -40)
            p = os.path.join(pdir, f"_{sid}_{st}.png")
            shoot(p)
            files.append(p)
        sp = os.path.join(pdir, f"{sid}_stages.png")
        sheet(sp, 2, 480, files)
        # grazing close-up of the Full rim
        show(objs, [names[2]])
        rin = {"Cup": 0, "Platform": 0}.get(table[sid]["nest"], 0)
        lo, hi = extent(objs[names[2]])
        rim = R * 0.8
        dist = 0.15 if R >= 0.2 else 0.15 * (0.5 + R)
        pos = (-(rim + dist) * 0.0 + (-1) * 0, 0, 0)
        e = math.radians(8)
        tgt = (0.0, 0.0, max(hi[2] * 0.6, 0.01))
        cam_pos = (-rim - dist * math.cos(e) * 0.9, -rim * 0.3, max(hi[2], 0.01) + dist * math.sin(e) + 0.004)
        look(cam, cam_pos, (-rim * 0.25, 0, max(hi[2] * 0.7, 0.01)), 40 if R > 0.15 else 50)
        gp = os.path.join(pdir, f"{sid}_graze.png")
        shoot(gp)
        graze.append(gp)
        # eggs in the Full nest
        eg = objs.get(f"SM_Egg_{sid}")
        if eg is not None:
            L, B = AT_EGGS(sid)
            rin_ = rin_of(sid)
            slots, rf = pack_eggs(rng, table[sid]["clutchMax"], L, B, rin_ * 0.72)
            made = []
            for k, (x, y, yaw) in enumerate(slots):
                c = eg.copy()
                bpy.context.scene.collection.objects.link(c)
                c.location = (x, y, floor_z(sid))
                c.rotation_euler = (0, 0, yaw)
                made.append(c)
                objs[f"_egg_{sid}_{k}"] = c
            show(objs, [names[2]] + [f"_egg_{sid}_{k}" for k in range(len(made))])
            orbit(cam, (0, 0, 0.0), max(R * 2.6, 0.2), 48, -40)
            ep = os.path.join(pdir, f"{sid}_eggs.png")
            shoot(ep)
            eggshots.append(ep)
            for c in made:
                bpy.data.objects.remove(c)
            for k in range(len(made)):
                objs.pop(f"_egg_{sid}_{k}", None)
        fulls.append(os.path.join(pdir, f"_{sid}_Full.png"))
    # contact sheets
    sheet(os.path.join(pdir, "sheet_full_nests.png"), 6, 320, fulls)
    sheet(os.path.join(pdir, "sheet_grazing.png"), 5, 380, graze)
    sheet(os.path.join(pdir, "sheet_eggs_in_nests.png"), 5, 380, eggshots)
    other = NM_previews(cam, objs, pdir, table)
    for f in os.listdir(pdir):
        if f.startswith("_"):
            os.remove(os.path.join(pdir, f))


def AT_EGGS(sid):
    import nest_atlas as AT
    return AT.EGGS[sid][0], AT.EGGS[sid][1]


def rin_of(sid):
    import nest_specs as SP
    sp = SP.SPECS[sid]
    r = sp.get("rin")
    if sp["kind"] == "Scrape":
        r = sp["rin"] * 0.78
    if sp["kind"] == "Dome":
        r = 0.11
    return r


def floor_z(sid):
    import nest_specs as SP
    sp = SP.SPECS[sid]
    return 0.0012 if sp["kind"] == "Scrape" and sp.get("lining") else 0.0


def NM_previews(cam, objs, pdir, table):
    import nest_misc as NM
    return NM.previews(cam, objs, pdir, table, show, orbit, look, shoot, sheet, extent)
