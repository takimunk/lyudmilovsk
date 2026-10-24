"""Materials of the Academgorodok build: which texture set (textures/academ/, made by tools/fal_academ_textures.py)
each common.MATERIALS slot uses, with what tint, roughness and grime, and build_materials() which (re)creates the
Blender preview materials from them.

The mapping (SLOTS, tint(), roughness_scale(), load_texture_info()) is plain Python with no bpy, so the Unreal
side (unreal/scripts/academ_materials.py) imports this file too and both engines agree.

Texture coordinates: UV 0 in metres (1 UV unit = 1 m, as the build's meshes are unwrapped); each texture covers
tile_m metres (textures/academ/academ_textures.json), so the shader scales UV by 1 / tile_m. For meshes with no
UVs, projection="BOX" maps object coordinates (metres) instead.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import common  # noqa: E402

TEX_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "textures", "academ"))
GRIME_TEX = "academ_grime_mask"

# slot -> texture set (None = flat colour), grime amount (0..1, world-aligned streak overlay), metallic, AO strength
SLOTS = {
    "AC_Wall":       ("plaster_ochre", 0.4, 0.0, 0.5),
    "AC_Trim":       ("plaster_white", 0.45, 0.0, 0.5),
    "AC_RusticDark": ("rustic_dark", 0.2, 0.0, 0.8),
    "AC_Quoin":      ("quoin_vermiculated", 0.4, 0.0, 0.8),
    "AC_Plinth":     ("concrete", 0.5, 0.0, 0.5),
    "AC_Roof":       ("roof_steel", 0.3, 0.0, 0.4),
    "AC_RoofBrown":  ("roof_steel", 0.3, 0.0, 0.4),
    "AC_Brick":      ("brick_red", 0.35, 0.0, 0.8),
    "AC_Timber":     ("timber_stained", 0.0, 0.0, 0.5),
    "AC_Frame":      (None, 0.0, 0.0, 0.0),
    "AC_Glass":      (None, 0.0, 0.0, 0.0),
    "AC_Door":       ("timber_stained", 0.0, 0.0, 0.5),
    "AC_Iron":       (None, 0.0, 0.0, 0.0),
    "AC_IronWhite":  (None, 0.0, 0.0, 0.0),
    "AC_Zinc":       (None, 0.0, 1.0, 0.0),
    "AC_Pylon":      ("plaster_white", 0.45, 0.0, 0.5),
    "AC_Stone":      ("stone_cast", 0.25, 0.0, 0.6),
    "AC_Asphalt":    ("asphalt", 0.0, 0.0, 0.5),
    "AC_Concrete":   ("concrete", 0.2, 0.0, 0.5),
    "AC_Soil":       ("soil_bed", 0.0, 0.0, 0.6),
}
GRIME_COLOUR = (0.055, 0.05, 0.042)    # linear: sooty grey-brown
GRIME_MULT = tuple(c / 0.25 for c in GRIME_COLOUR)   # multiplier on the base colour where the mask is 1
GRIME_TILE_M = 4.0
GLASS_SPECULAR = 0.5                   # Principled specular (F0 4 %)

DEFAULT_TILE = {"plaster_ochre": 2.0, "roof_steel": 1.2, "asphalt": 2.0, "concrete": 2.0}


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_lin(h):
    h = h.lstrip("#")
    return tuple(srgb_to_lin(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4))


def load_texture_info(tex_dir=None):
    path = os.path.join(tex_dir or TEX_DIR, "academ_textures.json")
    if os.path.exists(path):
        with open(path) as f:
            return json.load(f)
    return {}


def tile_m(tex, info):
    return info.get(tex, {}).get("tile_m", DEFAULT_TILE.get(tex, 1.0))


def tint(slot, info):
    """Linear multiplier taking the texture's mean colour to the slot's target colour (1,1,1 for the slot the
    texture was made for; e.g. the brown roof reuses the grey steel)."""
    tex = SLOTS[slot][0]
    target = hex_lin(common.MATERIALS[slot][0])
    if tex is None:
        return target
    made_for = info.get(tex, {}).get("target_hex", common.MATERIALS[slot][0])
    if made_for.upper() == common.MATERIALS[slot][0].upper():
        return (1.0, 1.0, 1.0)       # the texture was colour-matched to this slot already
    base = hex_lin(made_for)
    return tuple(t / max(m, 1e-4) for t, m in zip(target, base))


def roughness_scale(slot, info):
    tex = SLOTS[slot][0]
    rough = common.MATERIALS[slot][1]
    if tex is None:
        return rough
    return rough / max(info.get(tex, {}).get("roughness", rough), 1e-3)


# ------------------------------------------------------------------------------------------- Blender

def _image(bpy, path, colorspace):
    name = os.path.basename(path)
    im = bpy.data.images.get(name)
    if im is None or os.path.abspath(bpy.path.abspath(im.filepath)) != os.path.abspath(path):
        im = bpy.data.images.load(path, check_existing=True)
    im.colorspace_settings.name = colorspace
    return im


def _fresh(bpy, name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes.clear()
    return m


def _grime_factor(bpy, nt, tex_dir, amount, x, y):
    """World-aligned streak mask on walls: U along the wall (x + y), V down the height; fades out on faces that
    look up or down. Returns the socket of (mask * amount * wallness)."""
    n = nt.nodes
    L = nt.links
    geo = n.new("ShaderNodeNewGeometry"); geo.location = (x - 900, y)
    sep = n.new("ShaderNodeSeparateXYZ"); sep.location = (x - 720, y)
    L.new(geo.outputs["Position"], sep.inputs[0])
    add = n.new("ShaderNodeMath"); add.operation = "ADD"; add.location = (x - 560, y + 40)
    L.new(sep.outputs["X"], add.inputs[0]); L.new(sep.outputs["Y"], add.inputs[1])
    comb = n.new("ShaderNodeCombineXYZ"); comb.location = (x - 400, y)
    L.new(add.outputs[0], comb.inputs["X"]); L.new(sep.outputs["Z"], comb.inputs["Y"])
    sc = n.new("ShaderNodeVectorMath"); sc.operation = "SCALE"; sc.location = (x - 250, y)
    sc.inputs["Scale"].default_value = 1.0 / GRIME_TILE_M
    L.new(comb.outputs[0], sc.inputs[0])
    img = n.new("ShaderNodeTexImage"); img.location = (x - 80, y)
    img.image = _image(bpy, os.path.join(tex_dir, GRIME_TEX + ".jpg"), "Non-Color")
    L.new(sc.outputs[0], img.inputs["Vector"])
    nsep = n.new("ShaderNodeSeparateXYZ"); nsep.location = (x - 720, y - 200)
    L.new(geo.outputs["Normal"], nsep.inputs[0])
    absz = n.new("ShaderNodeMath"); absz.operation = "ABSOLUTE"; absz.location = (x - 560, y - 200)
    L.new(nsep.outputs["Z"], absz.inputs[0])
    wall = n.new("ShaderNodeMath"); wall.operation = "SUBTRACT"; wall.location = (x - 400, y - 200)
    wall.inputs[0].default_value = 1.0; wall.use_clamp = True
    L.new(absz.outputs[0], wall.inputs[1])
    m1 = n.new("ShaderNodeMath"); m1.operation = "MULTIPLY"; m1.location = (x + 200, y - 100)
    L.new(img.outputs["Color"], m1.inputs[0]); L.new(wall.outputs[0], m1.inputs[1])
    m2 = n.new("ShaderNodeMath"); m2.operation = "MULTIPLY"; m2.location = (x + 360, y - 100)
    m2.inputs[1].default_value = amount; m2.use_clamp = True
    L.new(m1.outputs[0], m2.inputs[0])
    return m2.outputs[0]


def build_materials(tex_dir=None, projection="UV"):
    """(Re)creates one Blender material per common.MATERIALS key; returns {name: bpy.types.Material}."""
    import bpy

    tex_dir = os.path.abspath(tex_dir or TEX_DIR)
    info = load_texture_info(tex_dir)
    out = {}
    for slot in common.MATERIALS:
        tex, grime, metallic, ao_strength = SLOTS[slot]
        m = _fresh(bpy, slot)
        nt = m.node_tree
        n, L = nt.nodes, nt.links
        bsdf = n.new("ShaderNodeBsdfPrincipled"); bsdf.location = (400, 0)
        outn = n.new("ShaderNodeOutputMaterial"); outn.location = (750, 0)
        L.new(bsdf.outputs[0], outn.inputs["Surface"])
        bsdf.inputs["Metallic"].default_value = metallic
        col = tint(slot, info)
        m.diffuse_color = (*col, 1.0)             # solid-view colour
        m.roughness = common.MATERIALS[slot][1]
        if slot == "AC_Glass":
            bsdf.inputs["Base Color"].default_value = (*col, 1.0)
            bsdf.inputs["Roughness"].default_value = common.MATERIALS[slot][1]
            bsdf.inputs["IOR"].default_value = 1.5
            out[slot] = m
            continue
        if tex is None or not os.path.exists(os.path.join(tex_dir, tex + "_D.jpg")):
            bsdf.inputs["Base Color"].default_value = (*hex_lin(common.MATERIALS[slot][0]), 1.0)
            bsdf.inputs["Roughness"].default_value = common.MATERIALS[slot][1]
            out[slot] = m
            continue
        # coordinates: UV (metres) or object-space box projection, scaled to the texture's tile
        coord = n.new("ShaderNodeTexCoord"); coord.location = (-1500, 0)
        mapping = n.new("ShaderNodeMapping"); mapping.location = (-1300, 0)
        s = 1.0 / tile_m(tex, info)
        mapping.inputs["Scale"].default_value = (s, s, s)
        L.new(coord.outputs["UV" if projection == "UV" else "Object"], mapping.inputs["Vector"])

        def img(suffix, colorspace, y):
            t = n.new("ShaderNodeTexImage"); t.location = (-1050, y)
            t.image = _image(bpy, os.path.join(tex_dir, "%s_%s.jpg" % (tex, suffix)), colorspace)
            if projection != "UV":
                t.projection = "BOX"; t.projection_blend = 0.2
            L.new(mapping.outputs[0], t.inputs["Vector"])
            return t
        d = img("D", "sRGB", 300)
        r = img("R", "Non-Color", -50)
        nm = img("N", "Non-Color", -400)
        # base colour = D * tint * mix(1, AO, strength) * mix(1, grime colour, grime mask)
        tn = n.new("ShaderNodeMix"); tn.data_type = "RGBA"; tn.blend_type = "MULTIPLY"; tn.location = (-700, 300)
        tn.inputs["Factor"].default_value = 1.0
        L.new(d.outputs["Color"], tn.inputs[6]); tn.inputs[7].default_value = (*col, 1.0)
        rsep = n.new("ShaderNodeSeparateColor"); rsep.location = (-750, -50)
        L.new(r.outputs["Color"], rsep.inputs[0])
        ao = n.new("ShaderNodeMix"); ao.data_type = "RGBA"; ao.blend_type = "MULTIPLY"; ao.location = (-450, 300)
        ao.inputs["Factor"].default_value = ao_strength
        L.new(tn.outputs[2], ao.inputs[6]); L.new(rsep.outputs["Green"], ao.inputs[7])
        base = ao.outputs[2]
        if grime > 0 and os.path.exists(os.path.join(tex_dir, GRIME_TEX + ".jpg")):
            g = _grime_factor(bpy, nt, tex_dir, grime, -300, 900)
            gm = n.new("ShaderNodeMix"); gm.data_type = "RGBA"; gm.blend_type = "MULTIPLY"; gm.location = (150, 350)
            L.new(g, gm.inputs["Factor"]); L.new(base, gm.inputs[6])
            gm.inputs[7].default_value = (*GRIME_MULT, 1)
            base = gm.outputs[2]
        L.new(base, bsdf.inputs["Base Color"])
        rs = n.new("ShaderNodeMath"); rs.operation = "MULTIPLY"; rs.use_clamp = True; rs.location = (-450, -50)
        rs.inputs[1].default_value = roughness_scale(slot, info)
        L.new(rsep.outputs["Red"], rs.inputs[0])
        L.new(rs.outputs[0], bsdf.inputs["Roughness"])
        # the normal maps are DirectX green (Unreal); Blender wants OpenGL, so flip G
        nsep = n.new("ShaderNodeSeparateColor"); nsep.location = (-750, -400)
        L.new(nm.outputs["Color"], nsep.inputs[0])
        inv = n.new("ShaderNodeMath"); inv.operation = "SUBTRACT"; inv.location = (-580, -450)
        inv.inputs[0].default_value = 1.0
        L.new(nsep.outputs["Green"], inv.inputs[1])
        ncomb = n.new("ShaderNodeCombineColor"); ncomb.location = (-420, -400)
        L.new(nsep.outputs["Red"], ncomb.inputs["Red"]); L.new(inv.outputs[0], ncomb.inputs["Green"])
        L.new(nsep.outputs["Blue"], ncomb.inputs["Blue"])
        nmap = n.new("ShaderNodeNormalMap"); nmap.location = (-200, -400)
        L.new(ncomb.outputs[0], nmap.inputs["Color"])
        L.new(nmap.outputs[0], bsdf.inputs["Normal"])
        out[slot] = m
    return out


def preview(out_path, tex_dir=None):
    """Renders every material on a bevelled block and a sphere (EEVEE, small) to out_path."""
    import math
    import bpy

    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = build_materials(tex_dir)
    scene = bpy.context.scene
    names = list(common.MATERIALS)
    cols = 5
    for i, name in enumerate(names):
        r, c = divmod(i, cols)
        x, z = c * 3.0, -r * 2.6
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x - 0.55, 0, z))
        cube = bpy.context.object
        cube.scale = (1.0, 1.0, 1.6)
        bpy.ops.object.transform_apply(scale=True)
        bev = cube.modifiers.new("bevel", "BEVEL"); bev.width = 0.04; bev.segments = 3
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.6, location=(x + 0.75, 0, z - 0.2), segments=48, ring_count=24)
        sph = bpy.context.object
        bpy.ops.object.shade_smooth()
        for ob in (cube, sph):
            bpy.context.view_layer.objects.active = ob
            ob.select_set(True)
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_all(action="SELECT")
            bpy.ops.uv.cube_project(cube_size=1.0, correct_aspect=False, scale_to_bounds=False)   # 1 UV = 1 m
            bpy.ops.object.mode_set(mode="OBJECT")
            ob.select_set(False)
            ob.data.materials.append(mats[name])
        bpy.ops.object.text_add(location=(x - 1.1, -0.7, z - 1.2), rotation=(math.radians(90), 0, 0))
        t = bpy.context.object
        t.data.body = name
        t.data.size = 0.28
        lab = bpy.data.materials.get("label") or bpy.data.materials.new("label")
        lab.use_nodes = True
        lab.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.02, 0.02, 0.02, 1)
        t.data.materials.append(lab)
    rows = (len(names) + cols - 1) // cols
    cx, cz = (cols - 1) * 3.0 / 2, -(rows - 1) * 2.6 / 2
    bpy.ops.mesh.primitive_plane_add(size=60, location=(cx, 2.0, cz), rotation=(math.radians(90), 0, 0))
    bg = bpy.data.materials.new("bg"); bg.use_nodes = True
    bg.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.35, 0.36, 0.38, 1)
    bpy.context.object.data.materials.append(bg)
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(55), math.radians(-10), math.radians(-35)))
    bpy.context.object.data.energy = 2.6
    bpy.context.object.data.angle = math.radians(2)
    world = bpy.data.worlds.new("w"); scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.65, 0.8, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.9
    bpy.ops.object.camera_add(location=(cx, -30, cz + 1.5), rotation=(math.radians(88), 0, 0))
    cam = bpy.context.object
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = cols * 3.0 + 0.6
    scene.camera = cam
    scene.render.engine = "BLENDER_EEVEE"
    try:
        scene.eevee.taa_render_samples = 32
    except AttributeError:
        pass
    scene.render.resolution_x, scene.render.resolution_y = 1280, 1000
    scene.view_settings.view_transform = "Standard"   # colours as authored
    scene.render.filepath = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(scene.render.filepath), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print("preview", scene.render.filepath)


if __name__ == "__main__":
    # blender -b --factory-startup --python blender/academgorodok/materials.py -- preview renders/academgorodok/materials_preview.png
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if argv and argv[0] == "preview":
        preview(argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "renders", "academgorodok",
                                                           "materials_preview.png"))
    else:
        print("built", len(build_materials()), "materials")
