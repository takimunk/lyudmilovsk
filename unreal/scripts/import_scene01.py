"""Builds /Game/Scene01/Lvl_Scene01 from blender/export/scene01_corridor.glb.

Imports the glTF scene through Interchange, gives every mesh exact collision,
adds garland lights, spinning vent fans and the ventilation hum,
replaces the glTF lights with tuned Unreal ones, adds the dark-room exposure,
the wake-up PlayerStart and the First Person game mode.

Run headless (editor closed):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import math
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
GLB = os.path.join(REPO, "blender", "export", "scene01_corridor.glb")
HUM_WAV = os.path.join(REPO, "audio", "vent_hum.wav")
DEST = "/Game/Scene01/Corridor"
LEVEL = "/Game/Scene01/Lvl_Scene01"
GAME_MODE = "/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C"

# Blender metres (x, y, z) land in Unreal as centimetres (x, -y, z).
# The hero's screen faces -X toward the chair at (55, -8700); see build_hero() in Blender.
SCREEN = unreal.Vector(169.4, -8700.0, 114.0)
WAKE = unreal.Vector(10.0, -8810.0, 100.0)
WARM = unreal.LinearColor(1.0, 0.68, 0.38, 1.0)
# Must match build_scene01_corridor.py (GARLAND_*, FAN_YS, DUCT_Z), converted to cm.
GARLAND_X, GARLAND_Z = -340.0, 205.0
GARLAND_COLORS = [(1.0, 0.62, 0.28), (1.0, 0.55, 0.22), (1.0, 0.7, 0.36)]
FAN_YS = [9.0, 27.0, 45.0, 63.0, 81.0]
DUCT_Z = 745.0
# Walkway belt: Blender x -3.05..-1.35, y 4..82 m. It carries riders toward the near end (+Y here).
WALKWAY_CENTER = unreal.Vector(-220.0, -4300.0, 75.0)
WALKWAY_EXTENT = unreal.Vector(75.0, 3900.0, 60.0)

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
    if isinstance(a, unreal.PointLight) and label.startswith("Corridor_Fill"):
        c = a.light_component
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        c.set_intensity(25.0)
        c.set_light_color(WARM)
        c.set_attenuation_radius(1800.0)
        c.set_editor_property("source_radius", 13.0)
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

# Garland: small warm lights along the wire so it actually tints the wall and table.
y = 60.0
k = 0
while y < 8940.0:
    col = GARLAND_COLORS[k % len(GARLAND_COLORS)]
    gl = spawn(unreal.PointLight, unreal.Vector(GARLAND_X + 15, -y, GARLAND_Z - 10), label="Garland_Light_%02d" % k)
    glc = gl.light_component
    gl.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    glc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    glc.set_intensity(1.2)
    glc.set_light_color(unreal.LinearColor(col[0], col[1], col[2], 1.0))
    glc.set_attenuation_radius(320.0)
    glc.set_editor_property("cast_shadows", False)
    gl.set_folder_path("Lights/Garland")
    y += 250.0
    k += 1


# Ventilation: spin the fan blades and make the duct hum.
def add_rotation(actor, yaw_rate):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.RotatingMovementComponent))
    if fail.is_empty():
        comp = actor.get_component_by_class(unreal.RotatingMovementComponent)
        comp.set_editor_property("rotation_rate", unreal.Rotator(0, 0, yaw_rate))
        comp.set_editor_property("rotation_in_local_space", True)
        return True
    log("could not add rotation to %s: %s" % (actor.get_actor_label(), fail))
    return False


spun = 0
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith("VentFan_Blades"):
        a.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        spun += add_rotation(a, 240.0)
log("spinning fans: %d" % spun)

task = unreal.AssetImportTask()
task.filename = HUM_WAV
task.destination_path = "/Game/Scene01/Audio"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
hum = eal.load_asset("/Game/Scene01/Audio/vent_hum")
hum.set_editor_property("looping", True)
eal.save_loaded_asset(hum)
for i, fy in enumerate(FAN_YS):
    snd = spawn(unreal.AmbientSound, unreal.Vector(0, -fy * 100.0, DUCT_Z - 40), label="VentHum_%d" % i)
    ac = snd.get_component_by_class(unreal.AudioComponent)
    ac.set_editor_property("sound", hum)
    ac.set_editor_property("volume_multiplier", 0.8)
    ac.set_editor_property("override_attenuation", True)
    att = ac.get_editor_property("attenuation_overrides")
    att.set_editor_property("attenuation_shape_extents", unreal.Vector(400, 0, 0))
    att.set_editor_property("falloff_distance", 2200.0)
    ac.set_editor_property("attenuation_overrides", att)

# The moving walkway (C++ AConveyorBelt from Source/Lyudmilovsk).
conveyor = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.ConveyorBelt"), WALKWAY_CENTER, label="Walkway_Conveyor")
conveyor.get_component_by_class(unreal.BoxComponent).set_box_extent(WALKWAY_EXTENT)
conveyor.set_editor_property("belt_actor", belt_actor)
conveyor.set_editor_property("direction", unreal.Vector(0, 1, 0))
conveyor.set_editor_property("motor_sound", hum)
log("conveyor belt: %s" % (belt_actor.get_actor_label() if belt_actor else "MISSING"))

# Darkness: fixed exposure so the eye does not "adapt" the dark corridor into daylight.
ppv = spawn(unreal.PostProcessVolume, unreal.Vector(0, -4500, 300), label="PP_Darkness")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_method", True)
s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 7.5)
s.set_editor_property("override_vignette_intensity", True)
s.set_editor_property("vignette_intensity", 0.6)
s.set_editor_property("override_film_grain_intensity", True)
s.set_editor_property("film_grain_intensity", 0.15)
ppv.set_editor_property("settings", s)

fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, -4500, 0), label="Fog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", 0.02)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.012, 0.008, 0.004, 1.0))
fc.set_editor_property("enable_volumetric_fog", True)

# Wake up next to the chair, facing the glowing screen and the corridor behind it.
yaw = math.degrees(math.atan2(SCREEN.y - WAKE.y, SCREEN.x - WAKE.x))
spawn(unreal.PlayerStart, WAKE, unreal.Rotator(0, 0, yaw), "PlayerStart_WakeUp")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, GAME_MODE))

les.save_current_level()
eal.save_directory("/Game/Scene01")
log("done: %d actors" % len(eas.get_all_level_actors()))
