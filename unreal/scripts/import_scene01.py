"""Builds /Game/Scene01/Lvl_Scene01 from blender/export/scene01_corridor.glb.

Imports the glTF scene through Interchange, gives every mesh exact collision and
builds the gameplay layer from blender/export/scene01_layout.json: garland, desk and
grow-lamp lights (some garland lights flicker), spinning vent and desk fans, fluttering
vent ribbons, ventilation hum and air hiss, the moving walkway, the hero's seat, the
dark-room exposure, the wake-up PlayerStart and the First Person game mode.

Endless corridor: past the hero's desk an invisible wall stops the player. The stretch of
corridor beyond it is real geometry, and Unreal repeats the whole 90 m chunk several times
further on (unlit copies; only their emissive bulbs and screens glow), while height fog
swallows the distance into darkness. It reads as a corridor going on forever.

Run headless (editor closed):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import json
import math
import os
import random
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
GLB = os.path.join(REPO, "blender", "export", "scene01_corridor.glb")
HUM_WAV = os.path.join(REPO, "audio", "vent_hum.wav")
DEST = "/Game/Scene01/Corridor"
LEVEL = "/Game/Scene01/Lvl_Scene01"
GAME_MODE = "/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"

AIR_WAV = os.path.join(REPO, "audio", "vent_air.wav")
SIT_SEQUENCE = "/Game/Characters/Zoomer/LS_SitAtComputer.LS_SitAtComputer"

# Everything positional comes from the Blender build, already in Unreal centimetres.
with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
    LAYOUT = json.load(f)
HERO = LAYOUT["hero"]


def vec(p):
    return unreal.Vector(*p)


SCREEN = vec(HERO["screen"])
WAKE = vec(HERO["wake"])
WARM = unreal.LinearColor(1.0, 0.68, 0.38, 1.0)
GARLAND_COLORS = [(1.0, 0.62, 0.28), (1.0, 0.55, 0.22), (1.0, 0.7, 0.36)]

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log_warning("SCENE01 " + msg)


def spawn(cls, loc, rot=unreal.Rotator(0, 0, 0), label=None):
    a = eas.spawn_actor_from_class(cls, loc, rot)
    if label:
        a.set_actor_label(label)
    return a


if not les.new_level(LEVEL):
    raise RuntimeError("could not create %s (delete Content/Scene01 first)" % LEVEL)

src = unreal.InterchangeManager.create_source_data(GLB)
params = unreal.ImportAssetParameters()
params.is_automated = True
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
mgr.import_scene(DEST, src, params)
mgr.wait_until_all_tasks_done(True)

# Exact collision so the player can walk on the floor, around the table and the walkway.
meshes = 0
for path in eal.list_assets(DEST, True):
    sm = eal.load_asset(path)
    if isinstance(sm, unreal.StaticMesh):
        body = sm.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            meshes += 1
log("collision set on %d meshes" % meshes)

# Nanite for the dense props (keyboards with real keycaps etc.), so ~60 stations plus the
# repeated corridor stay cheap. Meshes with see-through materials keep the classic path.
OPAQUE = (unreal.BlendMode.BLEND_OPAQUE, unreal.BlendMode.BLEND_MASKED)
nanite = 0
for path in eal.list_assets(DEST, True):
    sm = eal.load_asset(path)
    if not isinstance(sm, unreal.StaticMesh) or sm.get_num_vertices(0) < 3000:
        continue
    mats = [m.material_interface for m in sm.static_materials]
    if all(m and m.get_base_material().get_editor_property("blend_mode") in OPAQUE for m in mats):
        settings = sm.get_editor_property("nanite_settings")
        settings.enabled = True
        sm.set_editor_property("nanite_settings", settings)   # rebuilds the mesh with Nanite data
        nanite += 1
log("nanite on %d meshes" % nanite)

# Screens that are switched on: emissive materials built from the images Blender used.
def screen_material(name, tex_name, strength):
    path = "/Game/Scene01/M_" + name
    mel = unreal.MaterialEditingLibrary
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + name, "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    tex = None
    for p in eal.list_assets(DEST, True):
        if p.rsplit("/", 1)[-1].split(".")[0] == tex_name:
            tex = eal.load_asset(p)
            break
    sample = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -400, 0)
    sample.set_editor_property("texture", tex)
    boost = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 100)
    boost.set_editor_property("const_b", strength)
    mel.connect_material_expressions(sample, "RGB", boost, "A")
    mel.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -150, 250)
    rough.set_editor_property("r", 0.15)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("screen %s: %s" % (name, tex.get_path_name() if tex else None))
    return m


# Imported material name -> (texture asset name, emissive boost)
SCREENS = {
    "Hero_Screen": ("hero_os_screen", 4.0),
    "Screen_On_spreadsheet": ("screen_spreadsheet", 3.0),
    "Screen_On_16bit": ("screen_16bit", 3.0),
    "Screen_On_modern_game": ("screen_modern_game", 3.0),
    "Screen_On_exit": ("exit_sign", 6.0),
    "Screen_On_vending": ("vending_front", 2.5),
}
screen_mats = {k: screen_material(k.replace("Screen_On_", "Screen_"), *v) for k, v in SCREENS.items()}


def belt_material():
    """Dark rubber belt with treads every 40 cm; the conveyor scrolls them through 'Offset'."""
    mel = unreal.MaterialEditingLibrary
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_WalkwayBelt", "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    E = unreal
    wp = mel.create_material_expression(m, E.MaterialExpressionWorldPosition, -900, 0)
    mask = mel.create_material_expression(m, E.MaterialExpressionComponentMask, -750, 0)
    mask.set_editor_property("r", False)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    mel.connect_material_expressions(wp, "", mask, "")
    div = mel.create_material_expression(m, E.MaterialExpressionDivide, -600, 0)
    div.set_editor_property("const_b", 40.0)
    mel.connect_material_expressions(mask, "", div, "A")
    off = mel.create_material_expression(m, E.MaterialExpressionScalarParameter, -600, 150)
    off.set_editor_property("parameter_name", "Offset")
    sub = mel.create_material_expression(m, E.MaterialExpressionSubtract, -450, 0)
    mel.connect_material_expressions(div, "", sub, "A")
    mel.connect_material_expressions(off, "", sub, "B")
    frac = mel.create_material_expression(m, E.MaterialExpressionFrac, -300, 0)
    mel.connect_material_expressions(sub, "", frac, "")
    # tread = 1 for the first 15 % of each 40 cm period
    step = mel.create_material_expression(m, E.MaterialExpressionIf, -150, 0)
    mel.connect_material_expressions(frac, "", step, "A")
    for pin, val in (("B", 0.15), ("AGreaterThanB", 0.0), ("AEqualsB", 1.0), ("ALessThanB", 1.0)):
        c = mel.create_material_expression(m, E.MaterialExpressionConstant, -300, 200 + 60 * len(pin))
        c.set_editor_property("r", val)
        mel.connect_material_expressions(c, "", step, pin)
    dark = mel.create_material_expression(m, E.MaterialExpressionConstant3Vector, -150, 300)
    dark.set_editor_property("constant", unreal.LinearColor(0.018, 0.018, 0.02, 1))
    light = mel.create_material_expression(m, E.MaterialExpressionConstant3Vector, -150, 400)
    light.set_editor_property("constant", unreal.LinearColor(0.07, 0.07, 0.075, 1))
    lerp = mel.create_material_expression(m, E.MaterialExpressionLinearInterpolate, 0, 0)
    mel.connect_material_expressions(dark, "", lerp, "A")
    mel.connect_material_expressions(light, "", lerp, "B")
    mel.connect_material_expressions(step, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, E.MaterialExpressionConstant, 0, 200)
    rough.set_editor_property("r", 0.75)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


belt_mat = belt_material()
belt_actor = None
for a in eas.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    if a.get_actor_label() == "Walkway_Belt":
        belt_actor = a
        a.static_mesh_component.set_material(0, belt_mat)
        continue
    comp = a.static_mesh_component
    for i, mi in enumerate(comp.get_materials()):
        if mi and mi.get_name() in screen_mats:
            comp.set_material(i, screen_mats[mi.get_name()])

# glTF punctual lights come in with physical units that read wrong in Unreal; retune them.
for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if isinstance(a, unreal.CineCameraActor):
        eas.destroy_actor(a)

# The glow of the hero's monitor on the desk and the sleeper.
glow = spawn(unreal.RectLight, SCREEN + unreal.Vector(-4, 0, 0), unreal.Rotator(0, 0, 180), "Hero_ScreenGlow_UE")
rc = glow.get_component_by_class(unreal.RectLightComponent)
glow.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
rc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
rc.set_intensity(4.0)
rc.set_light_color(unreal.LinearColor(0.55, 0.9, 0.8, 1.0))
rc.set_attenuation_radius(900.0)
rc.set_editor_property("source_width", 42.0)
rc.set_editor_property("source_height", 32.0)

def point_light(loc, color, intensity, radius, label, folder, shadows=False):
    pl = spawn(unreal.PointLight, loc, label=label)
    pl.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    c = pl.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_intensity(intensity)
    c.set_light_color(color)
    c.set_attenuation_radius(radius)
    c.set_editor_property("cast_shadows", shadows)
    pl.set_folder_path(folder)
    return pl


# Lights only where the player can go (plus a little spill past the invisible wall); the
# corridor beyond stays dark and fades away in the fog.
WALL_Y = LAYOUT["invisible_wall_y"]


def lit(p):
    return p[1] > WALL_Y - 400


# Garland: every little bulb is its own light (tiny radius, no shadows, so dozens stay cheap).
# Some of them blink now and then (ALightFlicker).
rng = random.Random(7)
for k, p in enumerate(LAYOUT["garland_lights"]):
    if not lit(p):
        continue
    col = GARLAND_COLORS[k % len(GARLAND_COLORS)]
    gl = point_light(vec(p), unreal.LinearColor(col[0], col[1], col[2], 1.0), 0.35, 160.0,
                     "Garland_Light_%03d" % k, "Lights/Garland")
    gl.light_component.set_editor_property("source_radius", 1.0)
    if rng.random() < 0.2:
        gl.tags = [unreal.Name("Flicker")]
spawn(unreal.load_class(None, "/Script/Lyudmilovsk.LightFlicker"), unreal.Vector(0, 0, 300), label="GarlandFlicker")

# Desk lamps (warm) and the purple grow lamps over the roses.
for k, p in enumerate(LAYOUT.get("desk_lights", [])):
    if lit(p):
        point_light(vec(p), WARM, 3.0, 300.0, "DeskLamp_Light_%02d" % k, "Lights/Desk", shadows=True)
for k, hl in enumerate(LAYOUT.get("hall_lights", [])):
    c = hl["color"]
    point_light(vec(hl["pos"]), unreal.LinearColor(c[0], c[1], c[2], 1.0), hl["cd"], hl["radius"],
                "Hall_Light_%02d" % k, "Lights/Hall", shadows=hl.get("shadows", False))
for k, p in enumerate(LAYOUT.get("grow_lights", [])):
    if lit(p):
        point_light(vec(p), unreal.LinearColor(0.55, 0.1, 1.0, 1.0), 2.5, 220.0, "GrowLamp_Light_%02d" % k,
                "Lights/Desk", shadows=True)


# Ventilation: spin the fan blades and make the duct hum.
def add_rotation(actor, yaw_rate, pitch_rate=0.0):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.RotatingMovementComponent))
    if fail.is_empty():
        comp = actor.get_component_by_class(unreal.RotatingMovementComponent)
        comp.set_editor_property("rotation_rate", unreal.Rotator(0, pitch_rate, yaw_rate))
        comp.set_editor_property("rotation_in_local_space", True)
        return True
    log("could not add rotation to %s: %s" % (actor.get_actor_label(), fail))
    return False


spun = 0
for a in eas.get_all_level_actors():
    label = a.get_actor_label()
    if label.startswith("VentFan_Blades") or label.startswith("DeskFan_Blades"):
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        if label.startswith("Vent"):
            spun += add_rotation(a, 240.0)
        else:                       # desk fan blades turn about the fan's own axis (local Y)
            spun += add_rotation(a, 0.0, 900.0)
log("spinning fans: %d" % spun)


def ribbon_material(top_z):
    """Pale ribbon that flutters below the vent outlet: world-position wobble growing with depth."""
    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_VentRibbon", "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    wp = mel.create_material_expression(m, E.MaterialExpressionWorldPosition, -1200, 0)

    def mask(ch, y):
        e = mel.create_material_expression(m, E.MaterialExpressionComponentMask, -1050, y)
        for c in "rgba":
            e.set_editor_property(c, c == ch)
        mel.connect_material_expressions(wp, "", e, "")
        return e

    def op(cls, a, b, x, y, const_b=None):
        e = mel.create_material_expression(m, cls, x, y)
        mel.connect_material_expressions(a, "", e, "A")
        if b is not None:
            mel.connect_material_expressions(b, "", e, "B")
        elif const_b is not None:
            e.set_editor_property("const_b", const_b)
        return e

    def const(v, x, y):
        e = mel.create_material_expression(m, E.MaterialExpressionConstant, x, y)
        e.set_editor_property("r", v)
        return e

    # depth below the outlet, 0 at the top, 1 at the free end (32 cm)
    z = mask("b", 0)
    depth = op(E.MaterialExpressionSubtract, const(top_z, -1050, 100), z, -900, 0)
    depth = op(E.MaterialExpressionDivide, depth, None, -750, 0, const_b=32.0)
    sat = mel.create_material_expression(m, E.MaterialExpressionSaturate, -600, 0)
    mel.connect_material_expressions(depth, "", sat, "")
    # two sines at different rates, phase-shifted along the corridor
    t = mel.create_material_expression(m, E.MaterialExpressionTime, -1050, 250)
    y = mask("g", 400)
    phase = op(E.MaterialExpressionMultiply, y, None, -900, 400, const_b=0.013)
    fast = op(E.MaterialExpressionMultiply, t, None, -900, 250, const_b=2.3)
    slow = op(E.MaterialExpressionMultiply, t, None, -900, 320, const_b=0.9)
    s1 = mel.create_material_expression(m, E.MaterialExpressionSine, -600, 250)
    mel.connect_material_expressions(op(E.MaterialExpressionAdd, fast, phase, -750, 250), "", s1, "")
    s2 = mel.create_material_expression(m, E.MaterialExpressionSine, -600, 330)
    mel.connect_material_expressions(op(E.MaterialExpressionAdd, slow, phase, -750, 330), "", s2, "")
    swing = op(E.MaterialExpressionAdd, s1, op(E.MaterialExpressionMultiply, s2, None, -450, 330, const_b=0.6),
               -300, 250)
    amp = op(E.MaterialExpressionMultiply, swing, sat, -150, 250)
    # sideways flutter in X and Y plus a steady push outward and down from the airflow
    wobble = mel.create_material_expression(m, E.MaterialExpressionAppendVector, 0, 250)
    xy = mel.create_material_expression(m, E.MaterialExpressionAppendVector, -50, 250)
    mel.connect_material_expressions(op(E.MaterialExpressionMultiply, amp, None, -100, 200, const_b=7.0), "", xy, "A")
    mel.connect_material_expressions(op(E.MaterialExpressionMultiply, amp, None, -100, 300, const_b=4.0), "", xy, "B")
    mel.connect_material_expressions(xy, "", wobble, "A")
    mel.connect_material_expressions(op(E.MaterialExpressionMultiply, sat, None, -100, 380, const_b=-3.0), "",
                                     wobble, "B")
    mel.connect_material_property(wobble, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    col = mel.create_material_expression(m, E.MaterialExpressionConstant3Vector, 0, 0)
    col.set_editor_property("constant", unreal.LinearColor(0.75, 0.72, 0.62, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def steam_material(base_z, top_z):
    """Soft rising mist: 3D noise drifting upward, fading in above the nozzle and out at the top."""
    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_HumidifierSteam", "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, pin=""):
        mel.connect_material_expressions(a, "", b, pin)

    wp = node(E.MaterialExpressionWorldPosition, -1200, 0)
    t = node(E.MaterialExpressionTime, -1200, 200)
    rise = node(E.MaterialExpressionMultiply, -1050, 200, const_b=18.0)
    link(t, rise, "A")
    up = node(E.MaterialExpressionAppendVector, -900, 200)
    zero2 = node(E.MaterialExpressionConstant2Vector, -1050, 300, r=0.0, g=0.0)
    link(zero2, up, "A")
    link(rise, up, "B")
    drift = node(E.MaterialExpressionSubtract, -750, 0)
    link(wp, drift, "A")
    link(up, drift, "B")
    noise = node(E.MaterialExpressionNoise, -550, 0, scale=0.06, quality=2, levels=4, output_min=0.0, output_max=1.0)
    link(drift, noise, "Position")
    # vertical fade: 0 at the nozzle, peak a little above, 0 at the top
    z = node(E.MaterialExpressionComponentMask, -1050, 450, r=False, g=False, b=True, a=False)
    link(wp, z)
    rel = node(E.MaterialExpressionSubtract, -900, 450, const_b=base_z)
    link(z, rel, "A")
    h = node(E.MaterialExpressionDivide, -750, 450, const_b=top_z - base_z)
    link(rel, h, "A")
    hs = node(E.MaterialExpressionSaturate, -600, 450)
    link(h, hs)
    bell = node(E.MaterialExpressionSine, -450, 450, period=2.0)   # sin(pi * h): 0 -> 1 -> 0
    link(hs, bell)
    # horizontal fade toward the card edges keeps it from looking like a sheet
    uv = node(E.MaterialExpressionTextureCoordinate, -1050, 650)
    u = node(E.MaterialExpressionComponentMask, -900, 650, r=True, g=False, b=False, a=False)
    link(uv, u)
    centred = node(E.MaterialExpressionSubtract, -750, 650, const_b=0.5)
    link(u, centred, "A")
    ab = node(E.MaterialExpressionAbs, -600, 650)
    link(centred, ab)
    edge = node(E.MaterialExpressionOneMinus, -450, 650)
    two = node(E.MaterialExpressionMultiply, -500, 650, const_b=2.0)
    link(ab, two, "A")
    link(two, edge)
    a1 = node(E.MaterialExpressionMultiply, -300, 100)
    link(noise, a1, "A")
    link(bell, a1, "B")
    a2 = node(E.MaterialExpressionMultiply, -150, 100)
    link(a1, a2, "A")
    link(edge, a2, "B")
    a3 = node(E.MaterialExpressionMultiply, 0, 100, const_b=0.45)
    link(a2, a3, "A")
    mel.connect_material_property(a3, "", unreal.MaterialProperty.MP_OPACITY)
    col = node(E.MaterialExpressionConstant3Vector, 0, 0, constant=unreal.LinearColor(0.6, 0.62, 0.65, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def tinted_material(name, tex_name, tint, rough):
    """Texture times a colour: the corridor walls use the hall's textures, painted darker."""
    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    tex = next((eal.load_asset(p) for p in eal.list_assets(DEST, True)
                if p.rsplit("/", 1)[-1].split(".")[0] == tex_name), None)
    sample = mel.create_material_expression(m, E.MaterialExpressionTextureSample, -400, 0)
    sample.set_editor_property("texture", tex)
    col = mel.create_material_expression(m, E.MaterialExpressionConstant3Vector, -400, 200)
    col.set_editor_property("constant", unreal.LinearColor(*tint, 1.0))
    mul = mel.create_material_expression(m, E.MaterialExpressionMultiply, -150, 0)
    mel.connect_material_expressions(sample, "RGB", mul, "A")
    mel.connect_material_expressions(col, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = mel.create_material_expression(m, E.MaterialExpressionConstant, -150, 200)
    r.set_editor_property("r", rough)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


# Dark corridor: deep brown-olive above, darker green oil paint below.
DARK = {"Corridor_Wash": tinted_material("M_CorridorWash", "whitewash", (0.16, 0.12, 0.09), 0.9),
        "Corridor_WallPaint": tinted_material("M_CorridorPaint", "wall_paint", (0.5, 0.5, 0.5), 0.4)}
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.StaticMeshActor):
        comp = a.static_mesh_component
        for i, mi in enumerate(comp.get_materials()):
            if mi and mi.get_name() in DARK:
                comp.set_material(i, DARK[mi.get_name()])


def curtain_setup(top_z):
    """Fabric strips bend away from up to two nearby characters (positions from ACurtainPusher)."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mpc = tools.create_asset("MPC_Curtain", "/Game/Scene01", unreal.MaterialParameterCollection,
                             unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for n in ("Pusher0", "Pusher1"):
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", unreal.LinearColor(0, 0, -100000, 0))
        params.append(p)
    mpc.set_editor_property("vector_parameters", params)
    eal.save_loaded_asset(mpc)

    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = tools.create_asset("M_PartitionCurtain", "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, pin=""):
        mel.connect_material_expressions(a, "", b, pin)

    def binop(cls, a, b, x, y):
        e = node(cls, x, y)
        link(a, e, "A")
        link(b, e, "B")
        return e

    wp = node(E.MaterialExpressionWorldPosition, -1600, 0)
    wxy = node(E.MaterialExpressionComponentMask, -1450, 0, r=True, g=True, b=False, a=False)
    link(wp, wxy)
    wz = node(E.MaterialExpressionComponentMask, -1450, 300, r=False, g=False, b=True, a=False)
    link(wp, wz)
    # sag: 0 at the rail, 1 at the bottom, so the strips bend more the lower they go
    depth = node(E.MaterialExpressionSubtract, -1300, 300, const_a=top_z)
    link(wz, depth, "B")
    sag = node(E.MaterialExpressionDivide, -1150, 300, const_b=top_z)
    link(depth, sag, "A")
    sag_c = node(E.MaterialExpressionSaturate, -1000, 300)
    link(sag, sag_c)
    pushes = []
    for k, name in enumerate(("Pusher0", "Pusher1")):
        y0 = 600 + k * 400
        cp = node(E.MaterialExpressionCollectionParameter, -1600, y0, collection=mpc, parameter_name=name)
        cxy = node(E.MaterialExpressionComponentMask, -1450, y0, r=True, g=True, b=False, a=False)
        link(cp, cxy)
        d = binop(E.MaterialExpressionSubtract, wxy, cxy, -1300, y0)          # away from the character
        dist = node(E.MaterialExpressionLength, -1150, y0 + 50)
        link(d, dist)
        fall = node(E.MaterialExpressionDivide, -1000, y0 + 50, const_b=70.0)
        link(dist, fall, "A")
        fs = node(E.MaterialExpressionSaturate, -900, y0 + 50)
        link(fall, fs)
        near = node(E.MaterialExpressionOneMinus, -800, y0 + 50)
        link(fs, near)
        safe = node(E.MaterialExpressionAdd, -1000, y0 + 150, const_b=1.0)
        link(dist, safe, "A")
        dirv = binop(E.MaterialExpressionDivide, d, safe, -850, y0)
        amt = node(E.MaterialExpressionMultiply, -700, y0 + 50, const_b=55.0)
        link(near, amt, "A")
        pushes.append(binop(E.MaterialExpressionMultiply, dirv, amt, -550, y0))
    both = binop(E.MaterialExpressionAdd, pushes[0], pushes[1], -400, 700)
    bent = binop(E.MaterialExpressionMultiply, both, sag_c, -250, 700)
    # a gentle idle sway so the strips are never perfectly still
    t = node(E.MaterialExpressionTime, -700, 1300)
    tt = node(E.MaterialExpressionMultiply, -550, 1300, const_b=0.7)
    link(t, tt, "A")
    sw = node(E.MaterialExpressionSine, -400, 1300)
    link(tt, sw)
    swa = binop(E.MaterialExpressionMultiply, sw, sag_c, -250, 1300)
    swc = node(E.MaterialExpressionMultiply, -100, 1300, const_b=1.5)
    link(swa, swc, "A")
    swv = node(E.MaterialExpressionAppendVector, 50, 1300)
    link(swc, swv, "A")
    link(node(E.MaterialExpressionConstant, -100, 1400, r=0.0), swv, "B")
    xy = binop(E.MaterialExpressionAdd, bent, swv, 150, 700)
    wpo = node(E.MaterialExpressionAppendVector, 300, 700)
    link(xy, wpo, "A")
    link(node(E.MaterialExpressionConstant, 150, 850, r=0.0), wpo, "B")
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    col = node(E.MaterialExpressionConstant3Vector, 150, 0, constant=unreal.LinearColor(0.32, 0.05, 0.06, 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(E.MaterialExpressionConstant, 150, 150, r=0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return mpc, m


if "steam" in LAYOUT:
    st = steam_material(LAYOUT["steam"]["base_z"], LAYOUT["steam"]["top_z"])
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Humidifier_Steam":
            a.static_mesh_component.set_material(0, st)
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            a.static_mesh_component.set_editor_property("cast_shadow", False)

rib = ribbon_material(LAYOUT["ribbon_top_z"])
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "Vent_Ribbons":
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        a.static_mesh_component.set_material(0, rib)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def import_sound(path, looping=True):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = "/Game/Scene01/Audio"
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    snd = eal.load_asset("/Game/Scene01/Audio/" + os.path.splitext(os.path.basename(path))[0])
    snd.set_editor_property("looping", looping)
    eal.save_loaded_asset(snd)
    return snd


def ambient(sound, loc, label, volume, inner, falloff):
    a = spawn(unreal.AmbientSound, loc, label=label)
    ac = a.get_component_by_class(unreal.AudioComponent)
    ac.set_editor_property("sound", sound)
    ac.set_editor_property("volume_multiplier", volume)
    ac.set_editor_property("override_attenuation", True)
    att = ac.get_editor_property("attenuation_overrides")
    att.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0, 0))
    att.set_editor_property("falloff_distance", falloff)
    ac.set_editor_property("attenuation_overrides", att)
    a.set_folder_path("Audio")
    return a

hum = import_sound(HUM_WAV)
air = import_sound(AIR_WAV)
for i, p in enumerate(LAYOUT["vent_fans"]):
    ambient(hum, vec(p), "VentHum_%d" % i, 0.8, 300.0, 1800.0)
for i, p in enumerate(LAYOUT["vent_outlets"]):
    ambient(air, vec(p), "VentAir_%02d" % i, 0.55, 60.0, 650.0)
if "vending" in LAYOUT:   # the drinks machine's compressor
    ambient(hum, vec(LAYOUT["vending"]), "VendingHum", 0.25, 50.0, 400.0)

# Invisible wall a few places past the hero's desk.
wall = spawn(unreal.StaticMeshActor, unreal.Vector(0, WALL_Y, 250), label="InvisibleWall")
wall.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
wall.set_actor_scale3d(unreal.Vector(6.0, 0.2, 5.0))
wall.set_actor_hidden_in_game(True)
wall.static_mesh_component.set_editor_property("cast_shadow", False)
wall.static_mesh_component.set_collision_profile_name("BlockAll")

# Repeat the corridor past the wall: every static mesh except the near-end props and the
# hero's place, shifted by whole chunks (Unreal Y decreases toward the distance).
CHUNK = LAYOUT["chunk_length"]
COPIES = 4
SKIP = ("Mailboxes", "Radiator", "HeroStation", "HeroChair", "Walkway_Comb", "InvisibleWall", "ElectricCabinet",
        "Hall_", "AdminBooth", "Admin_", "VendingMachine", "ExitDoor", "ExitSign", "Booth_", "Humidifier",
        "Painting_", "FoodVending", "Clerk", "Partition", "Chandelier", "Owner", "Visitor_")
originals = [a for a in eas.get_all_level_actors()
             if isinstance(a, unreal.StaticMeshActor) and not a.get_actor_label().startswith(SKIP)]
extra_belts = []


def copy_actor(src, offset, label, folder):
    """Plain re-spawn (EditorActorSubsystem.duplicate_actors crashes in a commandlet)."""
    comp = src.static_mesh_component
    dst = spawn(unreal.StaticMeshActor, src.get_actor_location() + offset, src.get_actor_rotation(), label)
    dst.set_actor_scale3d(src.get_actor_scale3d())
    dc = dst.static_mesh_component
    dc.set_static_mesh(comp.static_mesh)
    for i, m in enumerate(comp.get_materials()):
        dc.set_material(i, m)
    dc.set_editor_property("cast_shadow", False)
    dc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    dst.set_folder_path(folder)
    if src.get_component_by_class(unreal.RotatingMovementComponent):
        dc.set_mobility(unreal.ComponentMobility.MOVABLE)
        r = src.get_component_by_class(unreal.RotatingMovementComponent).rotation_rate
        add_rotation(dst, r.yaw, r.pitch)
    return dst


for n in range(1, COPIES + 1):
    for a in originals:
        c = copy_actor(a, unreal.Vector(0, -CHUNK * n, 0), "%s_far%d" % (a.get_actor_label(), n), "Distance/Chunk_%d" % n)
        if a.get_actor_label().startswith("Walkway_Belt"):
            extra_belts.append(c)
log("distance chunks: %d x %d actors" % (COPIES, len(originals)))

# Partition between hall and corridor: a door that starts open (E toggles it) and soft curtain
# strips that bend away from whoever walks through.
if "door" in LAYOUT:
    leaf = next((a for a in eas.get_all_level_actors() if a.get_actor_label() == LAYOUT["door"]["leaf"]), None)
    if leaf:
        leaf.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        door = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.SwingDoor"), vec(LAYOUT["door"]["center"]),
                     label="PartitionDoor")
        door.set_editor_property("leaf", leaf)
        door.set_editor_property("open_angle", 90.0)     # swings into the hall
if "curtain" in LAYOUT:
    mpc, cmat = curtain_setup(LAYOUT["curtain"]["top_z"])
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "Partition_Curtain":
            a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            a.static_mesh_component.set_material(0, cmat)
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    pusher = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.CurtainPusher"), vec(LAYOUT["curtain"]["center"]),
                   label="CurtainPusher")
    pusher.set_editor_property("collection", mpc)

# The moving walkway (C++ AConveyorBelt): it runs whichever way its riders walk, speeding up
# as they keep going and easing off when they stop. Works for NPCs too.
walk = LAYOUT["walkway"]
y_near, y_far = walk["y0"], WALL_Y
zone_center = unreal.Vector((walk["x"][0] + walk["x"][1]) / 2, (y_near + y_far) / 2, 75.0)
zone_extent = unreal.Vector((walk["x"][1] - walk["x"][0]) / 2 - 5, abs(y_near - y_far) / 2, 60.0)
conveyor = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.ConveyorBelt"), zone_center, label="Walkway_Conveyor")
conveyor.get_component_by_class(unreal.BoxComponent).set_box_extent(zone_extent)
conveyor.set_editor_property("belt_actor", belt_actor)
conveyor.set_editor_property("extra_belts", extra_belts)
conveyor.set_editor_property("direction", unreal.Vector(0, 1, 0))   # axis; riders choose the way
conveyor.set_editor_property("max_speed", 900.0)
conveyor.set_editor_property("acceleration", 260.0)
conveyor.set_editor_property("deceleration", 220.0)
conveyor.set_editor_property("motor_sound", hum)
log("conveyor belt: %s" % (belt_actor.get_actor_label() if belt_actor else "MISSING"))

# Darkness: fixed exposure so the eye does not "adapt" the dark corridor into daylight.
ppv = spawn(unreal.PostProcessVolume, unreal.Vector(0, -4500, 300), label="PP_Darkness")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 7.3)
s.set_editor_property("override_vignette_intensity", True)
s.set_editor_property("vignette_intensity", 0.6)
s.set_editor_property("override_film_grain_intensity", True)
s.set_editor_property("film_grain_intensity", 0.15)
ppv.set_editor_property("settings", s)

fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, -4500, 0), label="Fog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
# Clear around the player, then the corridor slowly sinks into darkness with distance.
fc.set_editor_property("fog_density", 0.08)
fc.set_editor_property("fog_height_falloff", 0.001)
fc.set_editor_property("start_distance", 3500.0)
# Black fog: it only takes light away, so the distance goes dark instead of glowing.
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
fc.set_editor_property("enable_volumetric_fog", False)

# The hero's seat with the sit-down cutscene (AHeroSeat): origin on the floor at the chair,
# +X toward the screen.
seat_cls = unreal.load_class(None, "/Script/Lyudmilovsk.HeroSeat")
if seat_cls:
    seat = spawn(seat_cls, vec(HERO["chair"]), unreal.Rotator(0, 0, HERO["chair_yaw"]), "HeroSeat")
    seq = unreal.load_asset(SIT_SEQUENCE)
    if seq:
        seat.set_editor_property("sit_sequence", seq)
    else:
        log("sit sequence missing: %s" % SIT_SEQUENCE)
else:
    log("HeroSeat class missing: build the C++ module")

# The clerk NPC at Station 09, painting in a spreadsheet (mesh + looping animation from the clerk
# thread; the animated Excel picture is part of his mesh). The mesh faces +Y at yaw 0.
CLERK = LAYOUT.get("clerk")
CLERK_MESH = "/Game/Characters/Clerk/character_clerk/SkeletalMeshes/character_clerk"
if CLERK and eal.does_asset_exist(CLERK_MESH):
    clerk = spawn(unreal.SkeletalMeshActor, vec(CLERK["chair"]), unreal.Rotator(0, 0, CLERK["chair_yaw"] - 90), "Clerk")
    sk = clerk.skeletal_mesh_component
    sk.set_skeletal_mesh_asset(eal.load_asset(CLERK_MESH))
    sk.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    sk.override_animation_data(eal.load_asset(CLERK_MESH + "_Anim"), True, True)
    sk.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    cglow = spawn(unreal.RectLight, vec(CLERK["screen"]) + unreal.Vector(-4, 0, 0), unreal.Rotator(0, 0, 180),
                  "Clerk_ScreenGlow")
    cglow.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    crc = cglow.get_component_by_class(unreal.RectLightComponent)
    crc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    crc.set_intensity(2.5)
    crc.set_light_color(unreal.LinearColor(0.75, 0.85, 1.0, 1.0))
    crc.set_attenuation_radius(500.0)
    crc.set_editor_property("source_width", CLERK["screen_size"][0])
    crc.set_editor_property("source_height", CLERK["screen_size"][1])
else:
    log("clerk mesh missing: %s" % CLERK_MESH)

# People: the owner in his booth and a crowd at the reachable stations (meshes and loops from
# the humans thread; each mesh faces +Y at yaw 0 and stands on the floor under the seat centre).
HUMANS = "/Game/Characters/Humans/"
CROWD = [("Streamer", "Sit_Gamepad", ("sega", "ps2", "modern")),
         ("Guy_a", "Sit_KeyboardMouse", ("gaming", "beige", "spreadsheet", "mac")),
         ("Guy_b", "Sit_Browse", ("laptop", "imac", "vr", "beige")),
         ("Girl_a", "Sit_KeyboardMouse", ("gaming", "spreadsheet", "mac", "beige")),
         ("Girl_b", "Sit_Browse", ("imac", "laptop", "zx")),
         ("Kid_boy", "Sit_KeyboardMouse", ("gaming", "beige", "mac", "zx")),
         ("Kid_girl", "Sit_Browse", ("laptop", "imac", "sega", "zx"))]


def seat_person(name, anim, loc, facing_yaw, label):
    mesh = HUMANS + "%s/SK_%s" % (name, name)
    seq = HUMANS + "%s/Anims/A_%s_%s" % (name, name, anim)
    if not (eal.does_asset_exist(mesh) and eal.does_asset_exist(seq)):
        log("missing person %s / %s" % (mesh, seq))
        return None
    p = spawn(unreal.SkeletalMeshActor, loc, unreal.Rotator(0, 0, facing_yaw - 90), label)
    sk = p.skeletal_mesh_component
    sk.set_skeletal_mesh_asset(eal.load_asset(mesh))
    sk.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    sk.override_animation_data(eal.load_asset(seq), True, True)
    sk.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    p.set_folder_path("People")
    return p


if "owner" in LAYOUT:
    seat_person("Owner", "Sit_Idle", vec(LAYOUT["owner"]["chair"]), LAYOUT["owner"]["sitter_yaw"], "Owner")
taken = {CLERK["chair"][1] if CLERK else None}
free = [st for st in LAYOUT.get("stations", []) if st["reachable"] and st["chair"][1] not in taken]
for name, anim, kinds in CROWD:
    pick = next((st for st in free if st["kind"] in kinds), None) or (free[0] if free else None)
    if not pick:
        break
    free.remove(pick)
    seat_person(name, anim, vec(pick["chair"]), pick["sitter_yaw"], "Visitor_" + name)
    log("%s at station %02d (%s)" % (name, pick["index"], pick["kind"]))

# Screens light their users: a soft glow in front of each reachable station's display.
GLOW = {"gaming": (0.55, 0.6, 1.0), "sega": (0.9, 0.7, 0.6), "spreadsheet": (0.8, 0.9, 1.0), "vr": (0.5, 0.8, 1.0),
        "modern": (0.6, 0.7, 1.0), "ps2": (0.6, 0.7, 0.9), "beige": (0.75, 0.85, 1.0), "zx": (0.7, 0.8, 0.9),
        "laptop": (0.85, 0.9, 1.0), "imac": (0.7, 0.9, 1.0), "mac": (0.9, 0.9, 0.85)}
for st in LAYOUT.get("stations", []):
    if not st["reachable"]:
        continue
    c = GLOW.get(st["kind"], (0.8, 0.85, 1.0))
    g = spawn(unreal.RectLight, vec(st["desk"]) + unreal.Vector(-28, 0, 32), unreal.Rotator(0, 0, 180),
              "ScreenGlow_%02d" % st["index"])
    g.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    rc_ = g.get_component_by_class(unreal.RectLightComponent)
    rc_.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    rc_.set_intensity(1.6)
    rc_.set_light_color(unreal.LinearColor(c[0], c[1], c[2], 1.0))
    rc_.set_attenuation_radius(320.0)
    rc_.set_editor_property("source_width", 40.0)
    rc_.set_editor_property("source_height", 30.0)
    rc_.set_editor_property("cast_shadows", False)
    g.set_folder_path("Lights/Screens")

# Wake up in the aisle behind the hero's chair, facing the glowing screen.
yaw = math.degrees(math.atan2(SCREEN.y - WAKE.y, SCREEN.x - WAKE.x))
spawn(unreal.PlayerStart, WAKE, unreal.Rotator(0, 0, yaw), "PlayerStart_WakeUp")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, GAME_MODE))

les.save_current_level()
eal.save_directory("/Game/Scene01")
log("done: %d actors" % len(eas.get_all_level_actors()))
