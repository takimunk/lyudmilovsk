"""Interior preview shots of a town building (no Unreal needed): what the hero sees inside.

    python3 blender/city/shoot_interior.py -- <type>[:<plot>] [--cam x,y,z,yaw[,pitch]]... [--auto N] [--samples N]
                                              [--res WxH] [--lens MM] [--out DIR] [--top Z0,Z1]

Building frame (docs/city_buildings.md): x along the street front, y into the plot, z up; yaw in the kit's convention
(0 = looking toward -y, the street; 90 = +x; 180 = +y, into the plot). z of --cam is the eye (floor + 1.6).
--auto N (default 4 when no --cam): N cameras picked from the building's own spots (kind counter, work, table, seat,
stand), standing 2.5 m in front of the spot and looking at it; falls back to the entrances looking inward.
--top Z0,Z1: one more orthographic plan view from above with everything over Z1 clipped away (furniture layout
check); Z0 is the floor of the storey.
Lights: the building's own lamps (B.lamp), as in the game; day sky outside. Writes renders/city/interior/<type>_<n>.jpg.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv = sys.argv
ARGS = _argv[_argv.index("--") + 1:] if "--" in _argv else []
sys.argv = [_argv[0], "--", "--no-render"]          # build_city parses its own options at import
import build_city as bc  # noqa: E402
import citykit  # noqa: E402

sys.argv = _argv


def opts(name):
    return [ARGS[i + 1] for i, a in enumerate(ARGS) if a == name and i + 1 < len(ARGS)]


def opt(name, default):
    v = opts(name)
    return v[-1] if v else default


def look_dir(yaw, pitch=0.0):
    """Kit yaw (0 = -y, 90 = +x) and pitch (degrees up) -> a unit vector."""
    a, p = math.radians(yaw), math.radians(pitch)
    return Vector((math.sin(a) * math.cos(p), -math.cos(a) * math.cos(p), math.sin(p)))


def auto_cams(B, n):
    cams = []
    order = {"counter": 0, "work": 1, "table": 2, "stand": 3, "seat": 4, "stall": 5, "view": 6, "bench": 7}
    spots = sorted(B.spots, key=lambda s: order.get(s["kind"], 9))
    picked = []
    for s in spots:
        p = s["pos"]
        if any(math.dist(p, q) < 6.0 for q in picked):
            continue
        picked.append(p)
        d = look_dir(s.get("yaw", 0.0))
        eye = Vector((p[0], p[1], p[2] + 1.6)) + d * 2.5        # stand where the spot's user looks, look back at it
        yaw = (s.get("yaw", 0.0) + 180.0) % 360.0
        cams.append((eye.x, eye.y, eye.z, yaw, -8.0, s["id"]))
        if len(cams) >= n:
            break
    for e in B.entrances:
        if len(cams) >= n:
            break
        d = look_dir(e["yaw"])
        p = Vector(e["pos"]) - d * 1.5
        cams.append((p.x, p.y, p.z + 1.6, (e["yaw"] + 180.0) % 360.0, -5.0, "door_" + e["id"]))
    return cams


def main():
    names = [a for i, a in enumerate(ARGS) if not a.startswith("--") and (i == 0 or not ARGS[i - 1].startswith("--"))]
    if not names:
        print(__doc__)
        return
    type_id, _, plot = names[0].partition(":")
    mod = bc.importlib.import_module(type_id)
    variants = getattr(mod, "VARIANTS", {})
    out_id = type_id
    if plot or variants:
        plot = plot or next(iter(variants))
        for k, v in variants[plot].items():
            setattr(mod, k, v)
        out_id = bc.variant_name(type_id, plot)
    B = citykit.Building(type_id)
    mod.build(B)
    bc.lint(B)
    bc.clear_scene()
    bc._MATS.clear()
    bc.to_object("SM_" + out_id, B.m)
    if B.detail.faces:
        bc.to_object("SM_%s_Detail" % out_id, B.detail)
    if B.win.faces:
        bc.to_object("SM_%s_Windows" % out_id, B.win, slots=B.win_slot, lit=set())

    scn = bpy.context.scene
    scn.render.engine = "CYCLES"
    scn.cycles.device = "CPU"
    scn.cycles.samples = int(opt("--samples", 32))
    scn.cycles.use_denoising = True
    scn.cycles.max_bounces = 6
    res = opt("--res", "1120x700").split("x")
    scn.render.resolution_x, scn.render.resolution_y = int(res[0]), int(res[1])
    scn.render.image_settings.file_format = "JPEG"
    scn.render.image_settings.quality = 86
    scn.view_settings.view_transform = "AgX"
    scn.view_settings.exposure = float(opt("--exposure", 0.6))
    world = bpy.data.worlds.new("w")
    scn.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.68, 0.9, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(55), 0, math.radians(35))
    scn.collection.objects.link(sun)
    for i, l in enumerate(B.lamps):
        ld = bpy.data.lights.new("lamp%d" % i, "POINT")
        ld.energy = l["lumens"] / 15.0 * 4.0
        ld.shadow_soft_size = 0.25
        hx = l["colour"]
        ld.color = tuple(int(hx[k:k + 2], 16) / 255.0 for k in (1, 3, 5))
        lo = bpy.data.objects.new("lamp%d" % i, ld)
        lo.location = (l["pos"][0], l["pos"][1], l["pos"][2] - 0.2)
        scn.collection.objects.link(lo)
    me = bpy.data.meshes.new("ground")
    s = 300
    me.from_pydata([(-s, -s, -0.02), (s, -s, -0.02), (s, s, -0.02), (-s, s, -0.02)], [], [[0, 1, 2, 3]])
    g = bpy.data.objects.new("ground", me)
    me.materials.append(bc.material("CT_Paving"))
    scn.collection.objects.link(g)

    cams = []
    for c in opts("--cam"):
        v = [float(t) for t in c.split(",")]
        cams.append((v[0], v[1], v[2], v[3], v[4] if len(v) > 4 else -5.0, "cam%d" % len(cams)))
    if not cams:
        cams = auto_cams(B, int(opt("--auto", 4)))
    out = opt("--out", os.path.join(bc.ROOT, "renders", "city", "interior"))
    os.makedirs(out, exist_ok=True)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = float(opt("--lens", 20))
    cam.data.clip_start = 0.05
    scn.collection.objects.link(cam)
    scn.camera = cam
    for i, (x, y, z, yaw, pitch, name) in enumerate(cams):
        cam.location = (x, y, z)
        cam.rotation_euler = look_dir(yaw, pitch).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(out, "%s_%d_%s.jpg" % (out_id, i, name.replace("/", "_")))
        bpy.ops.render.render(write_still=True)
        print("SHOT %s  eye (%.1f, %.1f, %.1f) yaw %.0f" % (os.path.relpath(scn.render.filepath, bc.ROOT), x, y, z, yaw))
    top = opt("--top", "")
    if top:
        z0, z1 = [float(t) for t in top.split(",")]
        xs = [v[0] for v in B.m.verts]
        ys = [v[1] for v in B.m.verts]
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = max(max(xs) - min(xs), (max(ys) - min(ys)) * 1.6) + 2
        cam.location = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, z1)
        cam.rotation_euler = (0, 0, 0)
        cam.data.clip_start = 0.01
        cam.data.clip_end = z1 - z0 + 0.5
        # the ceiling lamps sit above the cut: light the plan evenly instead
        world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.2
        fill = bpy.data.objects.new("fill", bpy.data.lights.new("fill", "SUN"))
        fill.data.energy = 4.0
        fill.data.use_shadow = False
        scn.collection.objects.link(fill)
        scn.view_settings.exposure = -1.2
        scn.render.filepath = os.path.join(out, "%s_top_%g.jpg" % (out_id, z0))
        bpy.ops.render.render(write_still=True)
        print("SHOT %s  plan of z %.1f..%.1f (x right, y up)" % (os.path.relpath(scn.render.filepath, bc.ROOT), z0, z1))


if __name__ == "__main__":
    main()
