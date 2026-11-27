"""Preview renders of the sailing school's meshes (Cycles CPU, modest size): materials approximating the game's (flat
slots by colour, the sail pictures, the hull tint through COLOR_0.R), parts assembled at their pivots (sockets.json),
a sky, a sun, sand and sea. Used by build_all.py --previews (writes PNGs to the given folder)."""
import math
import os

import bpy
from mathutils import Euler, Matrix, Vector

import boatspec as B
from sailkit import TEX, MAT_SPEC, _lin, slot_colour, is_picture

TINTS = {"luch": "#E8E4D8", "optimist": "#F2C230", "cadet": "#8BB8D8"}


def _img(name):
    p = os.path.join(TEX, name)
    if not os.path.exists(p):
        return None
    im = bpy.data.images.get(name) or bpy.data.images.load(p)
    return im


def setup_material(mat, tint=None, sail_tex=None):
    slot = mat.name.split(".")[0]
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    hx, rough, metal = slot_colour(slot)
    col = _lin(hx) + (1,)
    bsdf.inputs["Base Color"].default_value = col
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if slot in MAT_SPEC and MAT_SPEC[slot][3] > 0:
        bsdf.inputs["Emission Color"].default_value = col
        bsdf.inputs["Emission Strength"].default_value = 2.0
    pic = None
    if slot == "SL_Sail":
        pic = sail_tex or "sail_luch_demo.png"
    elif slot == "SLP_Atlas":
        pic = "sail_atlas.png"
    elif slot == "SLP_Flags":
        pic = "sail_flags.png"
    elif slot.startswith("SLP_Sign"):
        pic = "sign_%s_ru.png" % slot[len("SLP_Sign_"):].lower()
    if pic and _img(pic):
        t = nt.nodes.new("ShaderNodeTexImage")
        t.image = _img(pic)
        nt.links.new(t.outputs[0], bsdf.inputs["Base Color"])
    if slot in ("SL_Hull", "SL_HullDeck") and tint:
        a = nt.nodes.new("ShaderNodeVertexColor")
        a.layer_name = "Col"
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(a.outputs[0], sep.inputs[0])
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs[6].default_value = col
        mix.inputs[7].default_value = _lin(tint) + (1,)
        nt.links.new(sep.outputs[0], mix.inputs[0])
        nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    if slot == "SL_Sail":
        bsdf.inputs["Subsurface Weight"].default_value = 0.0
        tr = bsdf.inputs.get("Transmission Weight")
        if tr is not None:
            tr.default_value = 0.0


def instance(src, name, M, tint=None, sail_tex=None):
    """A linked copy of a registry object with its own materials for the tint / sail picture."""
    o = bpy.data.objects.new(name, src.data.copy())
    bpy.context.scene.collection.objects.link(o)
    o.matrix_world = M
    if tint or sail_tex:
        for i, m in enumerate(o.data.materials):
            if m is None:
                continue
            base = m.name.split(".")[0]
            if base in ("SL_Hull", "SL_HullDeck", "SL_Sail"):
                nm = m.copy()
                nm.name = base + "." + name
                setup_material(nm, tint=tint, sail_tex=sail_tex)
                o.data.materials[i] = nm
    return o


def rigged(reg, b, M=Matrix.Identity(4), sheet_deg=18.0, tint=None, prefix=None, lod="", heel=0.0, sails=True,
           board_down=True):
    """Places a whole boat (parts at their pivots) at M; sheet_deg = boom / sail yaw to port (+)."""
    sk = B.sockets()[b]["parts"]
    cn = {"luch": "Luch", "optimist": "Optimist", "cadet": "Cadet"}[b]
    prefix = prefix or ("P_" + cn)
    objs = []
    H = M @ Matrix.Rotation(math.radians(heel), 4, "X")

    def place(part, extra=Matrix.Identity(4), tint_=None, sail_tex=None):
        nm = "SM_Sail_%s_%s%s" % (cn, part, lod)
        src = reg.objects.get(nm) or reg.objects.get("SM_Sail_%s_%s" % (cn, part))
        if src is None:
            return None
        piv = Vector(sk[part]["pivot"])
        o = instance(src, "%s_%s" % (prefix, part), H @ Matrix.Translation(piv) @ extra, tint=tint_, sail_tex=sail_tex)
        objs.append(o)
        return o
    place("Hull", tint_=tint or TINTS[b])
    place("Mast")
    yaw = Matrix.Rotation(math.radians(sheet_deg), 4, "Z")
    place("Boom", yaw)
    if sails:
        place("Main", yaw, sail_tex="sail_%s_demo.png" % b)
        if b == "cadet":
            ax = Vector(sk["Jib"]["axis"])
            place("Jib", Matrix.Rotation(math.radians(sheet_deg * 0.9), 4, ax), sail_tex="sail_cadet_jib.png")
    if b == "optimist":
        place("Sprit", yaw)
    rud = Matrix.Rotation(math.radians(-6), 4, "Z")
    place("Rudder", rud)
    tip = sk["Tiller"]["pivot"]
    o = place("Tiller", Matrix.Translation(Vector((0, 0, 0))) @ rud)
    # the extension at the tiller's tip, swung up toward the helm's side
    L = B.BOATS[b]["rudder"]["tiller_len"]
    ext_M = Matrix.Translation(Vector(sk["Tiller"]["pivot"]) - Vector(sk["Ext"]["pivot"])) @ rud @ Matrix.Translation((L, 0, 0.02)) @ \
        Matrix.Rotation(math.radians(150), 4, "Z") @ Matrix.Rotation(math.radians(-25), 4, "Y")
    place("Ext", ext_M)
    place("Board", Matrix.Identity(4) if board_down else Matrix.Translation((0, 0, 0.5)))
    return objs


def dummy(M, h=1.75, name="Dummy", pose="stand"):
    """A 1.75 m grey human-scale dummy (capsule body, head) at M (feet at the origin)."""
    import bmesh
    from sailkit import Mesh
    m = Mesh(name)
    if pose == "stand":
        m.tube((0, 0, 0.0), (0, 0, h * 0.52), 0.13, "SL_Dark", seg=12)
        m.tube((0, 0, h * 0.52), (0, 0, h * 0.84), 0.17, "SL_Dark", seg=12)
        m.sphere((0, 0, h * 0.92), 0.11, "SL_Dark", seg=12, rings=8)
    else:   # sitting on a deck: feet forward
        m.tube((0, 0, 0), (0.42, 0, 0), 0.07, "SL_Dark", seg=8)
        m.tube((0.42, 0, 0), (0.48, 0, -0.42), 0.06, "SL_Dark", seg=8)
        m.tube((0, 0, 0), (-0.05, 0, 0.55), 0.16, "SL_Dark", seg=12)
        m.sphere((-0.06, 0, 0.68), 0.11, "SL_Dark", seg=12, rings=8)
    o = m.to_object()
    o.matrix_world = M
    for mt in o.data.materials:
        setup_material(mt)
    return o


def scene_setup(sea=True, sand=True, sun_elev=38, sun_az=140, strength=3.2, sea_z=0.0, sand_z=0.0, sand_x=None):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 24
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:      # noqa: BLE001
        pass
    sc.cycles.max_bounces = 4
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Medium High Contrast" if "AgX - Medium High Contrast" in [i.identifier for i in sc.view_settings.bl_rna.properties["look"].enum_items] else "None"
    w = bpy.data.worlds.new("Sky")
    sc.world = w
    w.use_nodes = True
    nt = w.node_tree
    bg = nt.nodes.get("Background")
    sky = nt.nodes.new("ShaderNodeTexSky")
    try:
        sky.sky_type = "MULTIPLE_SCATTERING"
    except Exception:       # noqa: BLE001
        try:
            sky.sky_type = "NISHITA"
        except Exception:   # noqa: BLE001
            pass
    try:
        sky.sun_elevation = math.radians(sun_elev)
        sky.sun_rotation = math.radians(sun_az)
    except Exception:       # noqa: BLE001
        pass
    nt.links.new(sky.outputs[0], bg.inputs[0])
    bg.inputs[1].default_value = 0.35
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = strength
    sun.angle = math.radians(1.0)
    so = bpy.data.objects.new("Sun", sun)
    so.rotation_euler = Euler((math.radians(90 - sun_elev), 0, math.radians(sun_az)))
    sc.collection.objects.link(so)
    if sea:
        bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, sea_z))
        p = bpy.context.active_object
        p.name = "Sea"
        mt = bpy.data.materials.new("PrevSea")
        mt.use_nodes = True
        b = mt.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = (0.02, 0.07, 0.09, 1)
        b.inputs["Roughness"].default_value = 0.12
        p.data.materials.append(mt)
    if sand:
        bpy.ops.mesh.primitive_plane_add(size=400, location=((sand_x or 0) - (200 if sand_x is not None else 0), 0, sand_z))
        p = bpy.context.active_object
        p.name = "Sand"
        mt = bpy.data.materials.new("PrevSand")
        mt.use_nodes = True
        b = mt.node_tree.nodes["Principled BSDF"]
        b.inputs["Base Color"].default_value = (0.28, 0.22, 0.15, 1)
        b.inputs["Roughness"].default_value = 0.95
        p.data.materials.append(mt)


def camera(loc, target, lens=40, name="Cam"):
    cd = bpy.data.cameras.new(name)
    cd.lens = lens
    cd.clip_start = 0.05
    cd.clip_end = 600
    o = bpy.data.objects.new(name, cd)
    bpy.context.scene.collection.objects.link(o)
    o.location = loc
    d = Vector(target) - Vector(loc)
    o.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = o
    return o


def render(path, w=1600, h=1000, samples=24):
    sc = bpy.context.scene
    sc.render.resolution_x = w
    sc.render.resolution_y = h
    sc.render.resolution_percentage = 100
    sc.cycles.samples = samples
    sc.render.filepath = path
    sc.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    print("SAILING preview", path, flush=True)


def prepare_materials():
    for m in bpy.data.materials:
        if not m.name.startswith("Prev"):
            setup_material(m)


def clear_scene_keep(reg):
    keep = set(reg.objects.values())
    for o in list(bpy.data.objects):
        if o not in keep:
            bpy.data.objects.remove(o)
    for o in reg.objects.values():
        o.hide_render = True
        o.hide_set(True)
    for w in list(bpy.data.worlds):
        bpy.data.worlds.remove(w)
