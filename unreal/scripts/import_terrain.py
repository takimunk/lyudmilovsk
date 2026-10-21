"""The outdoor ground in an Unreal level: a Landscape over the whole map (tools/terrain.py samples the
map's macro shape, ULyudTerrainLibrary::BuildTerrain adds the natural detail, erosion, dirt tracks and
surface layers), its layered material from textures/ground/, the grass mask
(/Game/Scene01/Terrain/DA_TerrainMask, ULyudTerrainMask) and the small mesh patch that fills the
landscape's hole at the club's portal.

Run as a script it adds the ground to Lvl_Scene01 (build_scene01.sh runs it before import_world.py, in
a process of its own); make_terrain_preview_level.py imports build() for a level with only the ground.
Nothing here is committed content but the textures; the build makes the rest every time. Afterwards
tools/terrain.py ground_z() reads the built heights (Saved/World/terrain_out.bin) for placing things.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402

DEST = "/Game/Scene01/Terrain"
TEX_DIR = os.path.join(REPO, "textures", "ground")
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LIB = unreal.LyudTerrainLibrary

# Per layer: tint (multiplies the photo), roughness, and how strongly its texture's bright bits win a
# blend with its neighbours (stones and grass tips poke through, mud and sand settle into the gaps).
LOOK = {
    "Grass": ((0.66, 0.78, 0.56), 0.92, 0.6),
    "Meadow": ((0.74, 0.7, 0.6), 0.9, 0.6),
    "Dirt": ((0.6, 0.55, 0.52), 0.88, 0.5),
    "Mud": ((0.6, 0.58, 0.56), 0.62, 0.2),
    "Sand": ((0.74, 0.69, 0.6), 0.82, 0.3),
    "Rock": ((0.56, 0.56, 0.57), 0.8, 0.9),
    "Forest": ((0.72, 0.72, 0.68), 0.9, 0.5),
}
NEAR_TILE = 2.6       # metres per texture repeat up close
FAR_TILE = 11.0       # and the coarser copy that takes over in the distance and breaks the repeats


def _log(msg):
    unreal.log_warning("SCENE01 TERRAIN " + msg)


def _node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def _link(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("could not connect %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def _custom_input(name):
    c = unreal.CustomInput()
    c.set_editor_property("input_name", name)
    return c


def _custom_output(name, typ):
    c = unreal.CustomOutput()
    c.set_editor_property("output_name", name)
    c.set_editor_property("output_type", typ)
    return c


def import_textures(layers):
    """textures/ground/<Layer>_D.jpg, _N.jpg -> DEST/Textures/T_<Layer>_D, _N."""
    tasks = []
    for name in layers:
        for suffix in ("D", "N"):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(TEX_DIR, "%s_%s.jpg" % (name, suffix))
            t.destination_path = DEST + "/Textures"
            t.destination_name = "T_%s_%s" % (name, suffix)
            t.automated = True
            t.replace_existing = True
            t.save = False
            tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = {}
    for name in layers:
        d = eal.load_asset("%s/Textures/T_%s_D" % (DEST, name))
        n = eal.load_asset("%s/Textures/T_%s_N" % (DEST, name))
        n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        n.set_editor_property("srgb", False)
        for tex in (d, n):
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        out[name] = (d, n)
    return out


def _blend_hlsl(layers):
    """Height-aware blend of the layers: each layer's photo is mixed from the near and far tiling, its
    brightness lets it win a little against its neighbours, a slow macro variation breaks repeats."""
    n = len(layers)
    tints = ", ".join("float3(%g, %g, %g)" % LOOK[l][0] for l in layers)
    rough = ", ".join("%g" % LOOK[l][1] for l in layers)
    poke = ", ".join("%g" % LOOK[l][2] for l in layers)
    lines = ["float W[%d] = {%s};" % (n, ", ".join("W%d" % i for i in range(n))),
             "float3 A[%d] = {%s};" % (n, ", ".join("A%d" % i for i in range(n))),
             "float3 F[%d] = {%s};" % (n, ", ".join("F%d" % i for i in range(n))),
             "float3 N[%d] = {%s};" % (n, ", ".join("N%d" % i for i in range(n))),
             "const float3 Tint[%d] = {%s};" % (n, tints),
             "const float Rough[%d] = {%s};" % (n, rough),
             "const float Poke[%d] = {%s};" % (n, poke)]
    lines.append("""
float nearK = 1.0 - saturate((Dist - 600.0) / 5000.0);
float farMix = lerp(0.65, 0.25, nearK);
float b[%(n)d];
float top = -10.0;
for (int i = 0; i < %(n)d; i++)
{
    float h = dot(A[i], float3(0.33, 0.34, 0.33));
    b[i] = W[i] > 0.002 ? W[i] + h * Poke[i] * 0.6 : -10.0;
    top = max(top, b[i]);
}
float total = 0.0;
for (int i = 0; i < %(n)d; i++)
{
    b[i] = max(b[i] - top + 0.22, 0.0);
    total += b[i];
}
float3 col = 0;
float3 nrm = 0;
float r = 0;
for (int i = 0; i < %(n)d; i++)
{
    float k = b[i] / max(total, 1e-4);
    float3 c = lerp(A[i], F[i], farMix);
    col += k * c * Tint[i];
    nrm += k * N[i];
    r += k * Rough[i];
}
col *= 0.7 + 0.75 * Macro;                      // Macro averages ~0.4
Normal = normalize(lerp(float3(0, 0, 1), nrm, nearK * 0.4 + 0.1));
Roughness = r;
return col;
""" % {"n": n})
    return "\n".join(lines)


# Layers that cover steep faces (the bluff) are projected from the sides as well as from above, so
# their photo is not stretched down a cliff: metres per repeat.
TRIPLANAR = {"Rock": 6.0}

TRIPLANAR_UV_HLSL = """
float s = 1.0 / (Tile * 100.0);
UVX = float2(P.y, -P.z) * s;
UVY = float2(P.x, -P.z) * s;
return float2(P.x, P.y) * s;
"""

TRIPLANAR_MIX_HLSL = """
// the facing from the pixel's own slope (a landscape's vertex normal is not reliable in its pixel shader)
float3 Nw = normalize(cross(ddx(P), ddy(P)));
float3 w = pow(abs(Nw), 4.0);
w /= (w.x + w.y + w.z);
// the side views' tangent space is only roughly the landscape's: their bumps at half strength
Nrm = normalize(NZ * w.z + float3(NX.xy * 0.5, 1) * w.x + float3(NY.xy * 0.5, 1) * w.y);
float3 c = CX * w.x + CY * w.y + CZ * w.z;
// strata on the side views: wavy bands of pale sand and dark silt through the clay, a metre or two apart
float side = saturate(1.0 - w.z * 2.0);
float wav = sin(P.x * 0.0007 + P.y * 0.0005) * 140.0 + sin(P.x * 0.0031 - P.y * 0.0023) * 35.0;
float sand = smoothstep(0.55, 0.95, sin((P.z + wav) * 0.0042));
float silt = smoothstep(0.75, 0.98, sin((P.z + wav * 0.7) * 0.0097 + 1.3));
c = lerp(c, c * float3(1.35, 1.22, 0.92), sand * 0.7 * side);
c = lerp(c, c * 0.55, silt * 0.6 * side);
return c;
"""


def _triplanar(m, d, nm, tile, x, y, shared):
    """Colour and normal of a photo projected along world X, Y and Z, mixed by the surface's facing."""
    pos = _node(m, unreal.MaterialExpressionWorldPosition, x - 700, y)
    uv = _node(m, unreal.MaterialExpressionCustom, x - 450, y, code=TRIPLANAR_UV_HLSL,
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2, description="TriplanarUV",
               inputs=[_custom_input("P"), _custom_input("Tile")],
               additional_outputs=[_custom_output("UVX", unreal.CustomMaterialOutputType.CMOT_FLOAT2),
                                   _custom_output("UVY", unreal.CustomMaterialOutputType.CMOT_FLOAT2)])
    tile_c = _node(m, unreal.MaterialExpressionConstant, x - 700, y + 300, r=tile)
    _link(pos, "", uv, "P")
    _link(tile_c, "", uv, "Tile")
    mix = _node(m, unreal.MaterialExpressionCustom, x + 300, y, code=TRIPLANAR_MIX_HLSL,
                output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="TriplanarMix",
                inputs=[_custom_input(n) for n in ("CX", "CY", "CZ", "NX", "NY", "NZ", "P")],
                additional_outputs=[_custom_output("Nrm", unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    for k, out in enumerate(("UVX", "UVY", "")):
        axis = "XYZ"[k]
        c = _node(m, unreal.MaterialExpressionTextureSample, x, y + k * 90, texture=d, sampler_source=shared)
        t = _node(m, unreal.MaterialExpressionTextureSample, x, y + 270 + k * 90, texture=nm, sampler_source=shared,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        _link(uv, out, c, "UVs")
        _link(uv, out, t, "UVs")
        _link(c, "RGB", mix, "C" + axis)
        _link(t, "RGB", mix, "N" + axis)
    _link(pos, "", mix, "P")
    return mix                                        # outputs: "" colour, "Nrm" normal


def make_material(layers, textures):
    path = DEST + "/M_Ground"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Ground", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    shared = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
    near = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 0, mapping_scale=NEAR_TILE)
    far = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 200, mapping_scale=FAR_TILE)
    macro1 = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 400, mapping_scale=47.0)
    macro2 = _node(m, unreal.MaterialExpressionLandscapeLayerCoords, -2400, 600, mapping_scale=131.0)
    n = len(layers)
    blend = _node(m, unreal.MaterialExpressionCustom, -300, 0, code=_blend_hlsl(layers),
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="GroundBlend",
                  inputs=[_custom_input(p + str(i)) for p in "WAFN" for i in range(n)] +
                  [_custom_input("Macro"), _custom_input("Dist")],
                  additional_outputs=[_custom_output("Normal", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                                      _custom_output("Roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    for i, name in enumerate(layers):
        d, nm = textures[name]
        y = i * 520
        w = _node(m, unreal.MaterialExpressionLandscapeLayerSample, -1200, y, parameter_name=name,
                  preview_weight=1.0 if i == 0 else 0.0)
        if name in TRIPLANAR:
            mix = _triplanar(m, d, nm, TRIPLANAR[name], -1800, y, shared)
            _link(w, "", blend, "W%d" % i)
            _link(mix, "", blend, "A%d" % i)
            _link(mix, "", blend, "F%d" % i)
            _link(mix, "Nrm", blend, "N%d" % i)
            continue
        a = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 100, texture=d, sampler_source=shared)
        f = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 250, texture=d, sampler_source=shared)
        t = _node(m, unreal.MaterialExpressionTextureSample, -1800, y + 400, texture=nm, sampler_source=shared,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        _link(near, "", a, "UVs")
        _link(far, "", f, "UVs")
        _link(near, "", t, "UVs")
        _link(w, "", blend, "W%d" % i)
        _link(a, "RGB", blend, "A%d" % i)
        _link(f, "RGB", blend, "F%d" % i)
        _link(t, "RGB", blend, "N%d" % i)
    # slow brightness drift from the grass photo seen from very far, two scales
    grass_d = textures[layers[0]][0]
    m1 = _node(m, unreal.MaterialExpressionTextureSample, -1800, -500, texture=grass_d, sampler_source=shared)
    m2 = _node(m, unreal.MaterialExpressionTextureSample, -1800, -300, texture=grass_d, sampler_source=shared)
    _link(macro1, "", m1, "UVs")
    _link(macro2, "", m2, "UVs")
    mix = _node(m, unreal.MaterialExpressionLinearInterpolate, -1400, -400, const_alpha=0.5)
    _link(m1, "G", mix, "A")
    _link(m2, "R", mix, "B")
    _link(mix, "", blend, "Macro")
    depth = _node(m, unreal.MaterialExpressionPixelDepth, -1400, -200)
    _link(depth, "", blend, "Dist")
    mel.connect_material_property(blend, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(blend, "Normal", MP.MP_NORMAL)
    mel.connect_material_property(blend, "Roughness", MP.MP_ROUGHNESS)
    hole = _node(m, unreal.MaterialExpressionLandscapeVisibilityMask, -300, 400)
    mel.connect_material_property(hole, "", MP.MP_OPACITY_MASK)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def make_patch_material(textures):
    """The portal patch's ground: the grass photo, world-aligned at the landscape's near tiling."""
    path = DEST + "/M_GroundPatch"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_GroundPatch", DEST, unreal.Material, unreal.MaterialFactoryNew())
    d, nm = textures["Grass"]
    pos = _node(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    xy = _node(m, unreal.MaterialExpressionComponentMask, -700, 0, r=True, g=True, b=False, a=False)
    scale = _node(m, unreal.MaterialExpressionDivide, -500, 0, const_b=NEAR_TILE * 100.0)
    _link(pos, "", xy, "")
    _link(xy, "", scale, "A")
    a = _node(m, unreal.MaterialExpressionTextureSample, -300, 0, texture=d)
    t = _node(m, unreal.MaterialExpressionTextureSample, -300, 250, texture=nm,
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    _link(scale, "", a, "UVs")
    _link(scale, "", t, "UVs")
    tint = _node(m, unreal.MaterialExpressionMultiply, -100, 0, const_b=0.95)
    _link(a, "RGB", tint, "A")
    mel.connect_material_property(tint, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(t, "RGB", MP.MP_NORMAL)
    rough = _node(m, unreal.MaterialExpressionConstant, -100, 300, r=LOOK["Grass"][1])
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def make_patch(face_y, material):
    """terrain.portal_patch() as a static mesh with exact (complex-as-simple) collision, in the
    landscape's hole at the portal."""
    verts, tris = T.portal_patch()
    path = DEST + "/SM_PortalPatch"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    sm = tools.create_asset("SM_PortalPatch", DEST, unreal.StaticMesh, None)
    desc = sm.create_static_mesh_description()
    group = desc.create_polygon_group()
    vids = []
    for x, y, z in verts:
        v = desc.create_vertex()
        desc.set_vertex_position(v, unreal.Vector(x * 100.0, y * 100.0, z * 100.0))
        vids.append(v)
    for a, b, c in tris:
        inst = []
        # Unreal's front face winds clockwise seen from above (its Y runs the other way to glTF's)
        for k in (a, c, b):
            vi = desc.create_vertex_instance(vids[k])
            desc.set_vertex_instance_uv(vi, unreal.Vector2D(verts[k][0] / NEAR_TILE, verts[k][1] / NEAR_TILE), 0)
            inst.append(vi)
        desc.create_triangle(group, inst)
    desc.compute_polygon_triangle_normals_and_tangents() if hasattr(desc, "compute_polygon_triangle_normals_and_tangents") else None
    sm.set_editor_property("allow_cpu_access", True)     # the collision is cooked from this mesh's own triangles
    sm.build_from_static_mesh_descriptions([desc])
    sm.set_material(0, material)
    body = sm.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(sm)
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, face_y, 0))
    a.static_mesh_component.set_static_mesh(sm)
    a.static_mesh_component.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
    a.set_actor_label("Ground_PortalPatch")
    a.set_folder_path("Outside/Ground")
    return a


def build(face_y):
    """Adds the ground to the open level; face_y = the portal face, Unreal Y cm."""
    out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "World")
    meta = T.write_inputs(out_dir, log=_log)
    _log("inputs written to %s (%d x %d)" % (out_dir, meta["nx"], meta["ny"]))
    layers = [str(n) for n in LIB.get_layer_names()]
    textures = import_textures(layers)
    material = make_material(layers, textures)
    infos = []
    for name in layers:
        path = "%s/Layers/LI_%s" % (DEST, name)
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        info = LIB.create_layer_info(path, name)
        eal.save_loaded_asset(info)
        infos.append(info)
    mask_path = DEST + "/DA_TerrainMask"
    if eal.does_asset_exist(mask_path):
        eal.delete_asset(mask_path)
    mask = LIB.create_mask(mask_path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    origin = unreal.Vector(T.GRID_X0 * 100.0, face_y + T.GRID_Y0 * 100.0, 0.0)
    landscape, report = LIB.build_terrain(world, out_dir, origin, material, infos, mask)
    for line in str(report).splitlines():
        _log(line)
    if not landscape:
        raise RuntimeError("terrain build failed: %s" % report)
    landscape.set_folder_path("Outside/Ground")
    eal.save_loaded_asset(mask)
    T.load(out_dir)                                   # ground_z now answers with the built heights
    make_patch(face_y, make_patch_material(textures))
    eal.save_directory(DEST)
    return landscape


if __name__ == "__main__":
    import json
    with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
        layout = json.load(f)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level("/Game/Scene01/Lvl_Scene01"):
        raise RuntimeError("no /Game/Scene01/Lvl_Scene01: run import_scene01.py first")
    build(layout["outside"]["portal_face_y"])
    les.save_current_level()
    _log("done")
