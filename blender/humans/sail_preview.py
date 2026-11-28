"""Contact sheets for sail_clips.py: each clip at 5 moments from the side (port), the front (the bow) and the top, the
body in clay, a stand-in boat (or the shore's things) built from the same numbers the clip is posed with, the tiller,
its extension and the sheets drawn from the hand targets the clip returns. One PNG per clip: <dir>/<Clip>.png.
Cycles on the CPU (headless: no GPU, no EGL)."""
import math
import os

import bpy
import bmesh
from mathutils import Matrix, Vector

W, H = 300, 300
PHASES = 5
COL = {"clay": (0.78, 0.6, 0.5), "boat": (0.9, 0.9, 0.88), "deck": (0.75, 0.78, 0.8), "wood": (0.55, 0.36, 0.18),
       "rope": (0.85, 0.1, 0.08), "rope2": (0.1, 0.3, 0.85), "spar": (0.6, 0.62, 0.66), "water": (0.2, 0.4, 0.55),
       "ground": (0.55, 0.5, 0.42), "prop": (0.95, 0.45, 0.05), "dark": (0.12, 0.12, 0.13), "sail": (0.95, 0.95, 0.92)}
_mats = {}
_static = []
_dyn = []


def mat(name):
    if name not in _mats:
        m = bpy.data.materials.new("pv_" + name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes["Principled BSDF"]
        bsdf.inputs["Base Color"].default_value = COL[name] + (1.0,)
        bsdf.inputs["Roughness"].default_value = 0.8
        if name == "water":
            bsdf.inputs["Alpha"].default_value = 0.45
        _mats[name] = m
    return _mats[name]


def _obj(bm, color, bucket):
    me = bpy.data.meshes.new("pv")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("pv", me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat(color))
    bucket.append(o)
    return o


def box(lo, hi, color, bucket):
    bm = bmesh.new()
    g = bmesh.ops.create_cube(bm, size=1.0)
    c = (Vector(lo) + Vector(hi)) * 0.5
    s = Vector(hi) - Vector(lo)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(c) @ Matrix.Diagonal((abs(s.x), abs(s.y), abs(s.z), 1.0)), verts=g["verts"])
    return _obj(bm, color, bucket)


def rod(a, b, r, color, bucket, seg=10):
    a, b = Vector(a), Vector(b)
    d = b - a
    if d.length < 1e-4:
        return None
    bm = bmesh.new()
    g = bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=d.length)
    m = Matrix.Translation((a + b) * 0.5) @ d.to_track_quat("Z", "Y").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=m, verts=g["verts"])
    return _obj(bm, color, bucket)


def clear(bucket):
    for o in bucket:
        me = o.data
        bpy.data.objects.remove(o)
        bpy.data.meshes.remove(me)
    bucket.clear()


def boat_scene(B):
    k = 1.0 / B["scale"]
    y0, y1 = B["length"][0] * k, B["length"][1] * k
    hb, ch, dz = B["half_beam"] * k, B["cockpit_half"] * k, B["deck_z"] * k
    box((-ch, y0 * 0.55, -0.04), (ch, y1, 0.0), "deck", _static)                    # the cockpit sole
    for s in (-1, 1):
        box((s * ch, y0, dz - 0.04), (s * hb, y1, dz), "boat", _static)            # the side decks
    box((-hb, y0, dz - 0.04), (hb, y0 * 0.55, dz), "boat", _static)                # the foredeck
    sx, sz = B["strap"][0] * k, B["strap"][1] * k
    box((sx - 0.025, y0 * 0.3, sz - 0.005), (sx + 0.025, y1 * 0.15, sz + 0.005), "rope2", _static)  # the hiking strap
    my = B["mast_y"] * k
    rod((0, my, 0), (0, my, 3.2 * k), 0.035, "spar", _static)
    if "sheet_block" in B:
        sb = Vector(B["sheet_block"]) * k
        box(sb - Vector((0.03, 0.03, 0.04)), sb + Vector((0.03, 0.03, 0.0)), "dark", _static)


def boom(B, angle, swing_z=None):
    """The boom from the mast aft, out `angle` deg to port (+) / starboard (-)."""
    k = 1.0 / B["scale"]
    my, bz = B["mast_y"] * k, (B["boom_z"] if swing_z is None else swing_z) * k
    L = (B["length"][1] - B["mast_y"]) * k * 0.92
    a = math.radians(angle)
    end = Vector((math.sin(a) * L, my + math.cos(a) * L, bz + 0.03))
    rod((0, my, bz + 0.03), end, 0.03, "spar", _dyn)
    # the sail's foot as a thin sheet up to the mast's top (just a few bars to read where it is)
    for h in (0.5, 1.2, 2.0):
        frac = max(0.0, 1.0 - h / 3.0)
        rod((0, my, bz + h * k), Vector((0, my, bz + h * k)).lerp(Vector((end.x, end.y, bz + h * k)), frac), 0.008, "sail", _dyn)


def shore_scene(kind):
    box((-2.0, -2.0, -0.05), (2.0, 2.0, 0.0), "ground", _static)


def draw_roles(r, roles, B):
    """The tiller and its extension, the sheets, and whatever the clip asks drawn, from its hand targets."""
    mv = r.v
    for name, h in roles.items():
        if name == "draw":
            for item in h:
                kind = item[0]
                if kind == "rod":
                    rod(mv(item[1]), mv(item[2]), item[3], item[4], _dyn)
                elif kind == "box":
                    lo, hi = mv(item[1]), mv(item[2])
                    box(Vector((min(lo.x, hi.x), min(lo.y, hi.y), min(lo.z, hi.z))),
                        Vector((max(lo.x, hi.x), max(lo.y, hi.y), max(lo.z, hi.z))), item[3], _dyn)
            continue
        if name == "boom" and B:
            boom(B, h * r.m)
            continue
        if not isinstance(h, dict) or "grip" not in h:
            continue
        g = Vector(h["grip"])
        if "pivot" in h:                                   # a tiller: rudder head -> its end -> the extension to the hand
            rod(mv(h["rudder"]), mv(h["pivot"]), 0.022, "wood", _dyn)
            e = (g - Vector(h["pivot"])).normalized()
            rod(mv(h["pivot"]), mv(g + e * h.get("beyond", 0.06)), 0.012, "dark", _dyn)
        elif "from" in h:
            rod(mv(h["from"]), mv(g), max(h.get("radius", 0.005), 0.005), h.get("color", "rope"), _dyn, seg=6)
            if "to" in h:
                rod(mv(g), mv(h["to"]), max(h.get("radius", 0.005), 0.005), h.get("color", "rope"), _dyn, seg=6)


def setup(spec, boats):
    sc = bpy.context.scene
    if sc.get("pv_ready"):
        return
    sc["pv_ready"] = True
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 10
    sc.cycles.use_denoising = False
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("pv_w")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.66, 0.7, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    sc.world = w
    sun = bpy.data.objects.new("pv_sun", bpy.data.lights.new("pv_sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(40), math.radians(15), math.radians(35))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("pv_cam", bpy.data.cameras.new("pv_cam"))
    cam.data.type = "ORTHO"
    sc.collection.objects.link(cam)
    sc.camera = cam
    clay = mat("clay")
    for o in bpy.data.objects:
        if o.type == "MESH" and o.parent and o.parent.type == "ARMATURE":
            o.data.materials.clear()
            o.data.materials.append(clay)


def sheet(r, spec, full, out_dir, boats):
    from PIL import Image, ImageDraw
    setup(spec, boats)
    sc = bpy.context.scene
    cam = sc.camera
    clear(_static)
    B = boats.get(spec["boat"]) if spec.get("boat") else None
    if spec["scene"] == "boat" and B:
        boat_scene(B)
    elif spec["scene"] in ("shore", "seat"):
        shore_scene(spec["scene"])
    T = spec["T"]
    times = [T * j / PHASES if spec["loop"] else T * j / (PHASES - 1) for j in range(PHASES)]
    # framing: round the body over all the moments
    pts = []
    for t in times:
        spec["fn"](r, t, T)
        pts += [r.p.head(b) for b in ("pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r")]
    lo = Vector((min(q.x for q in pts), min(q.y for q in pts), min(q.z for q in pts)))
    hi = Vector((max(q.x for q in pts), max(q.y for q in pts), max(q.z for q in pts)))
    c = (lo + hi) * 0.5
    size = max(max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) * 1.15, 1.3) + 0.3
    sx = -1.0 if (spec["frame"] in ("boat",) and r.m > 0) else 1.0      # the side the body faces
    views = {"side": (Vector((sx, 0, 0)), Vector((0, 0, 1))), "front": (Vector((0, -1, 0)), Vector((0, 0, 1))),
             "top": (Vector((0, 0, 1)), Vector((0, -1, 0)))}
    tiles = {}
    os.makedirs(out_dir, exist_ok=True)
    tmp = os.path.join(out_dir, "_tmp_%s.png" % full)
    for j, t in enumerate(times):
        clear(_dyn)
        roles = spec["fn"](r, t, T) or {}
        draw_roles(r, roles, B)
        bpy.context.view_layer.update()
        for vn, (d, up) in views.items():
            cam.data.ortho_scale = size
            cam.location = c + d * 8.0
            cam.rotation_euler = (-d).to_track_quat("-Z", "Y" if vn != "top" else "Y").to_euler()
            if vn == "top":
                cam.rotation_euler = Matrix(((-1, 0, 0), (0, -1, 0), (0, 0, 1))).to_euler()
                cam.rotation_euler = (0.0, 0.0, math.pi)
            sc.render.filepath = tmp
            bpy.ops.render.render(write_still=True)
            tiles[(vn, j)] = Image.open(tmp).convert("RGB").copy()
    clear(_dyn)
    if os.path.exists(tmp):
        os.remove(tmp)
    lab = 18
    img = Image.new("RGB", (W * PHASES, (H + lab) * 3 + lab), (40, 40, 44))
    dr = ImageDraw.Draw(img)
    dr.text((6, 3), "%s  %.2f s %s   (side: the side it faces | front: from the bow (-Y) | top, bow up)" % (
        full, T, "loop" if spec["loop"] else "one-shot"), fill=(240, 240, 240))
    for vi, vn in enumerate(views):
        for j in range(PHASES):
            x, y = j * W, lab + vi * (H + lab)
            img.paste(tiles[(vn, j)], (x, y + lab))
            dr.text((x + 4, y + 3), "%s t=%.2f" % (vn, times[j]), fill=(230, 230, 230))
    path = os.path.join(out_dir, full + ".png")
    img.save(path)
    print("SAILCLIPS preview %s" % path, flush=True)
