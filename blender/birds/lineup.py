"""Every species side by side: one orthographic render per species and pose, at a known scale, for lineup sheets.

    Blender -b --python blender/birds/lineup.py -- <dir> <out dir> [Id ...]

<dir> holds <Id>/<Id>.glb (blender/birds/export or a copy of it): LOD0 posed by its Idle clip (frame 0) and Glide clip.
Writes <out>/<Id>_side.png (perched, from the bird's left), <Id>_top.png (gliding, from above, head up) and
<out>/scale.json {Id: {"side": metres per pixel, "top": ...}}. blender/birds/lineup_sheet.py composes the sheets.
"""
import json
import os
import sys

import bpy
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = argv[0], argv[1]
IDS = argv[2:]
RES = 1000
os.makedirs(OUT, exist_ok=True)


def find(i):
    p = os.path.join(SRC, i, f"{i}.glb")
    return p if os.path.exists(p) else None


if not IDS:
    IDS = sorted(d for d in os.listdir(SRC) if find(d))


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = RES
    sc.render.film_transparent = True
    sc.view_settings.view_transform = "AgX"
    w = bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    b = next(n for n in w.node_tree.nodes if n.type == "BACKGROUND")
    b.inputs["Color"].default_value = (0.55, 0.6, 0.7, 1)
    b.inputs["Strength"].default_value = 0.9
    sd = bpy.data.lights.new("Sun", "SUN")
    sd.energy = 3.5
    sd.color = (1.0, 0.92, 0.82)
    sun = bpy.data.objects.new("Sun", sd)
    sc.collection.objects.link(sun)
    sun.rotation_euler = mathutils.Euler((0.9, 0.2, -0.6))
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO"
    co = bpy.data.objects.new("Cam", cd)
    sc.collection.objects.link(co)
    sc.camera = co
    return sc, co


def bounds(objs):
    lo = mathutils.Vector((1e9, 1e9, 1e9))
    hi = -lo
    for o in objs:
        if o.type != "MESH":
            continue
        dg = bpy.context.evaluated_depsgraph_get()
        me = o.evaluated_get(dg).to_mesh()
        for v in me.vertices:
            p = o.matrix_world @ v.co
            lo = mathutils.Vector(map(min, lo, p))
            hi = mathutils.Vector(map(max, hi, p))
        o.evaluated_get(dg).to_mesh_clear()
    return lo, hi


def paint(objs, i):
    """The species' atlas on every slot (the glb carries no textures; Unreal makes the materials)."""
    tp = os.path.join(SRC, i, f"T_{i}_Albedo.png")
    if not os.path.exists(tp):
        return
    im = bpy.data.images.load(tp)
    m = bpy.data.materials.new(f"L_{i}")
    m.use_nodes = True
    nt = m.node_tree
    p = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = im
    nt.links.new(t.outputs["Color"], p.inputs["Base Color"])
    nt.links.new(t.outputs["Alpha"], p.inputs["Alpha"])
    p.inputs["Roughness"].default_value = 0.6
    if hasattr(m, "surface_render_method"):
        m.surface_render_method = "DITHERED"
    for o in objs:
        for s in o.material_slots:
            s.material = m


scale = {}
for i in IDS:
    scale[i] = {}
    path = find(i)
    if not path:
        continue
    for clip, view in (("Idle", "side"), ("Glide", "top")):
        sc, co = setup()
        bpy.ops.import_scene.gltf(filepath=path)
        objs = [o for o in sc.objects if o.type == "MESH"]
        for o in objs:      # LOD0 only
            if not o.name.startswith("LOD0"):
                o.hide_render = True
        objs = [o for o in objs if not o.hide_render]
        paint(objs, i)
        arm = next(o for o in sc.objects if o.type == "ARMATURE")
        act = next((a for a in bpy.data.actions if a.name.split("_")[0].split(".")[0] == clip or a.name.startswith(clip)), None)
        arm.animation_data_create()
        arm.animation_data.action = act
        if act and hasattr(act, "slots") and len(act.slots):
            arm.animation_data.action_slot = act.slots[0]
        sc.frame_set(int(act.frame_range[0]) if act else 0)
        lo, hi = bounds(objs)
        c = (lo + hi) / 2
        ext = hi - lo
        cd = co.data
        if view == "side":
            # side view from the bird's left (-Y in Blender, the bird faces +X): feet on the frame's lower part
            cd.ortho_scale = max(ext.x, ext.z) * 1.08
            co.location = (c.x, lo.y - 5, c.z)
            co.rotation_euler = (1.5708, 0, 0)
        else:
            cd.ortho_scale = max(ext.x, ext.y) * 1.08
            co.location = (c.x, c.y, hi.z + 5)
            co.rotation_euler = (0, 0, -1.5708)    # head up in the picture
        cd.clip_end = 100
        sc.render.filepath = os.path.join(OUT, f"{i}_{view}.png")
        bpy.ops.render.render(write_still=True)
        # metres per pixel, and where the ground (lowest point) and the frame centre sit
        scale[i][view] = dict(mpp=cd.ortho_scale / RES, centre=[c.x, c.y, c.z], lo=list(lo), hi=list(hi))
        print("LINEUP", i, view, round(cd.ortho_scale, 3))
json.dump(scale, open(os.path.join(OUT, "scale.json"), "w"), indent=1)
