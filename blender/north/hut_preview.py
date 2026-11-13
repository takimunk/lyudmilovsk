"""Cycles previews of the ranger's hut (a CPU render in bpy): neutral sky light plus a low sun, a grey 1.75 m human-sized box
for scale. Helpers only; hut_build.py calls them on the freshly built scene (`--previews`)."""
import math
import os

import bpy
from mathutils import Vector


def setup(res=(1280, 800), samples=48):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:
        pass
    sc.cycles.max_bounces = 6
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"
    sc.view_settings.view_transform = "Filmic" if "Filmic" in [v.identifier for v in sc.view_settings.bl_rna.properties["view_transform"].enum_items] else "AgX"
    sc.render.film_transparent = False
    return sc


def world(strength=1.0, color=(0.62, 0.72, 0.88)):
    w = bpy.data.worlds.new("W") if not bpy.context.scene.world else bpy.context.scene.world
    bpy.context.scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    bg = nt.nodes.new("ShaderNodeBackground")
    out = nt.nodes.new("ShaderNodeOutputWorld")
    # a simple vertical gradient: a bright horizon, a bluer zenith
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    cr = nt.nodes.new("ShaderNodeValToRGB")
    nt.links.new(tc.outputs["Generated"], sep.inputs["Vector"])
    nt.links.new(sep.outputs["Z"], cr.inputs["Fac"])
    cr.color_ramp.elements[0].position = 0.35
    cr.color_ramp.elements[0].color = (0.9, 0.9, 0.88, 1)
    cr.color_ramp.elements[1].position = 0.8
    cr.color_ramp.elements[1].color = tuple(color) + (1,)
    nt.links.new(cr.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = strength
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])


def sun(elev=22.0, azim=215.0, energy=3.2, color=(1.0, 0.93, 0.8)):
    d = bpy.data.lights.new("Sun", "SUN")
    d.energy = energy
    d.color = color
    d.angle = math.radians(1.2)
    o = bpy.data.objects.new("Sun", d)
    bpy.context.scene.collection.objects.link(o)
    o.rotation_euler = (math.radians(90 - elev), 0, math.radians(azim + 180))
    return o


def point(loc, watts, color=(1, 0.8, 0.55), radius=0.04, name="P"):
    d = bpy.data.lights.new(name, "POINT")
    d.energy = watts
    d.color = color
    d.shadow_soft_size = radius
    o = bpy.data.objects.new(name, d)
    o.location = loc
    bpy.context.scene.collection.objects.link(o)
    return o


def ground(size=60.0, color=(0.30, 0.34, 0.17)):
    me = bpy.data.meshes.new("Ground")
    me.from_pydata([(-size, -size, 0), (size, -size, 0), (size, size, 0), (-size, size, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("Ground", me)
    bpy.context.scene.collection.objects.link(ob)
    m = bpy.data.materials.new("GroundM")
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = color + (1,)
    b.inputs["Roughness"].default_value = 1.0
    me.materials.append(m)
    return ob


def human(x, y, z0, yaw=0.0, name="Human175"):
    """A grey 1.75 m box (0.5 x 0.3 x 1.75) standing at (x, y) on z0."""
    me = bpy.data.meshes.new(name)
    hx, hy = 0.25, 0.15
    v = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0), (-hx, -hy, 1.75), (hx, -hy, 1.75), (hx, hy, 1.75), (-hx, hy, 1.75)]
    me.from_pydata(v, [], [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)])
    ob = bpy.data.objects.new(name, me)
    ob.location = (x, y, z0)
    ob.rotation_euler = (0, 0, math.radians(yaw))
    bpy.context.scene.collection.objects.link(ob)
    m = bpy.data.materials.new("HumanM")
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.35, 0.35, 0.37, 1)
    me.materials.append(m)
    return ob


def camera(loc, target, lens=28.0):
    cam = bpy.data.cameras.new("Cam")
    cam.lens = lens
    cam.clip_start = 0.05
    cam.clip_end = 400
    ob = bpy.data.objects.new("Cam", cam)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = loc
    d = Vector(target) - Vector(loc)
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = ob
    return ob


def render(path, cam_loc, target, lens=28.0):
    """Renders one view to path. Returns the path."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    c = camera(cam_loc, target, lens)
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(c)
    return path


def sheet(paths, out, cols=2):
    from PIL import Image
    ims = [Image.open(p).convert("RGB") for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    img = Image.new("RGB", (w * cols, h * rows))
    for i, im in enumerate(ims):
        img.paste(im, ((i % cols) * w, (i // cols) * h))
    img.save(out)
