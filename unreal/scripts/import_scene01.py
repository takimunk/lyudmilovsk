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
    if not isinstance(sm, unreal.StaticMesh) or sm.get_num_vertices(0) < 3000 or "SkyDome" in sm.get_name():
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


def live_screen_material(name, tex_name, scale, offset, speed, sway=(0.0, 0.0), flicker=0.0, boost=3.0):
    """A screen that keeps moving: UV = uv*scale + offset + time*speed (+ a little sway), wrapped,
    so feeds and chats scroll, racing roads rush by, platformer levels slide, shooters sway."""
    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_" + name, "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    tex = next((eal.load_asset(p) for p in eal.list_assets(DEST, True)
                if p.rsplit("/", 1)[-1].split(".")[0] == tex_name), None)

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

    uv = node(E.MaterialExpressionTextureCoordinate, -1200, 0)
    sc = node(E.MaterialExpressionConstant2Vector, -1200, 100, r=scale[0], g=scale[1])
    off = node(E.MaterialExpressionConstant2Vector, -1200, 200, r=offset[0], g=offset[1])
    spd = node(E.MaterialExpressionConstant2Vector, -1200, 300, r=speed[0], g=speed[1])
    t = node(E.MaterialExpressionTime, -1200, 400)
    a = binop(E.MaterialExpressionMultiply, uv, sc, -1000, 0)
    b = binop(E.MaterialExpressionAdd, a, off, -850, 0)
    mv = binop(E.MaterialExpressionMultiply, t, spd, -1000, 300)
    c = binop(E.MaterialExpressionAdd, b, mv, -700, 0)
    if sway != (0.0, 0.0):
        t1 = node(E.MaterialExpressionMultiply, -1000, 500, const_b=1.3)
        link(t, t1, "A")
        s1 = node(E.MaterialExpressionSine, -850, 500)
        link(t1, s1)
        t2 = node(E.MaterialExpressionMultiply, -1000, 600, const_b=2.1)
        link(t, t2, "A")
        s2 = node(E.MaterialExpressionSine, -850, 600)
        link(t2, s2)
        sx = node(E.MaterialExpressionMultiply, -700, 500, const_b=sway[0])
        link(s1, sx, "A")
        sy = node(E.MaterialExpressionMultiply, -700, 600, const_b=sway[1])
        link(s2, sy, "A")
        sv = binop(E.MaterialExpressionAppendVector, sx, sy, -550, 550)
        c = binop(E.MaterialExpressionAdd, c, sv, -550, 0)
    w = node(E.MaterialExpressionFrac, -400, 0)
    link(c, w)
    sample = node(E.MaterialExpressionTextureSample, -250, 0, texture=tex)
    link(w, sample, "UVs")
    em = node(E.MaterialExpressionMultiply, -50, 100, const_b=boost)
    mel.connect_material_expressions(sample, "RGB", em, "A")
    if flicker:
        tf = node(E.MaterialExpressionMultiply, -400, 300, const_b=23.0)
        link(t, tf, "A")
        sf = node(E.MaterialExpressionSine, -250, 300)
        link(tf, sf)
        fa = node(E.MaterialExpressionMultiply, -100, 300, const_b=flicker)
        link(sf, fa, "A")
        f1 = node(E.MaterialExpressionAdd, 0, 300, const_b=1.0)
        link(fa, f1, "A")
        em = binop(E.MaterialExpressionMultiply, em, f1, 100, 100)
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_expressions(sample, "RGB", node(E.MaterialExpressionMultiply, -50, 0, const_b=0.2), "A")
    r = node(E.MaterialExpressionConstant, 100, 250, r=0.15)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def garland_filament_material(wall_y):
    """Glowing filament that is lit like on our side at the wall and, the further down the tunnel, flickers
    more and more and glows less, every bulb on its own beat, so the far end barely glows."""
    mel = unreal.MaterialEditingLibrary
    E = unreal
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_GarlandFilament", "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, pin=""):
        mel.connect_material_expressions(a, "", b, pin)

    def un(cls, a, x, y, **props):
        e = node(cls, x, y, **props)
        link(a, e, "A" if "const_b" in props or "const_a" in props else "")
        return e

    def binop(cls, a, b, x, y):
        e = node(cls, x, y)
        link(a, e, "A")
        link(b, e, "B")
        return e

    wp = node(E.MaterialExpressionWorldPosition, -1600, 0)
    wy = node(E.MaterialExpressionComponentMask, -1450, 0, r=False, g=True, b=False, a=False)
    link(wp, wy)
    # distance past the wall, 0..1 over the whole tunnel (the real stretch plus the repeated chunks)
    past = node(E.MaterialExpressionSubtract, -1300, 0, const_a=wall_y)
    link(wy, past, "B")
    d = un(E.MaterialExpressionDivide, past, -1150, 0, const_b=FAR_LEN)
    dc0 = node(E.MaterialExpressionSaturate, -1000, 0)
    link(d, dc0)
    dc = node(E.MaterialExpressionPower, -900, 0, const_exponent=0.6)   # trouble starts early, then grows
    link(dc0, dc, "Base")
    # per-bulb id from its position along the wire (bulbs are 32 cm apart)
    idf = un(E.MaterialExpressionDivide, wy, -1300, 200, const_b=32.0)
    idr = node(E.MaterialExpressionFloor, -1150, 200)
    link(idf, idr)
    h1 = un(E.MaterialExpressionMultiply, idr, -1000, 200, const_b=12.9898)
    h2 = node(E.MaterialExpressionSine, -850, 200)
    link(h1, h2)
    h3 = un(E.MaterialExpressionMultiply, h2, -700, 200, const_b=43758.5)
    hash_ = node(E.MaterialExpressionFrac, -550, 200)
    link(h3, hash_)
    # a fresh random draw per bulb six times a second
    t = node(E.MaterialExpressionTime, -1300, 400)
    t6 = un(E.MaterialExpressionMultiply, t, -1150, 400, const_b=9.0)
    tf = node(E.MaterialExpressionFloor, -1000, 400)
    link(t6, tf)
    hb = un(E.MaterialExpressionMultiply, hash_, -1000, 500, const_b=100.0)
    seed = binop(E.MaterialExpressionAdd, tf, hb, -850, 400)
    r1 = un(E.MaterialExpressionMultiply, seed, -700, 400, const_b=78.233)
    r2 = node(E.MaterialExpressionSine, -550, 400)
    link(r1, r2)
    r3 = un(E.MaterialExpressionMultiply, r2, -400, 400, const_b=12345.6)
    rnd = node(E.MaterialExpressionFrac, -250, 400)
    link(r3, rnd)
    # a bulb is off while its draw is below d * 0.7 (sharp step): all lit at the wall, most blinking at the end
    th = un(E.MaterialExpressionMultiply, dc, -550, 0, const_b=0.7)
    diff = binop(E.MaterialExpressionSubtract, th, rnd, -100, 200)
    sharp = un(E.MaterialExpressionMultiply, diff, 50, 200, const_b=1000.0)
    off = node(E.MaterialExpressionSaturate, 200, 200)
    link(sharp, off)
    on = node(E.MaterialExpressionOneMinus, 350, 200)
    link(off, on)
    dim = un(E.MaterialExpressionMultiply, dc, -400, 0, const_b=-0.94)
    dim1 = un(E.MaterialExpressionAdd, dim, -250, 0, const_b=1.0)
    k = binop(E.MaterialExpressionMultiply, on, dim1, 500, 100)
    col = node(E.MaterialExpressionConstant3Vector, 500, 300, constant=unreal.LinearColor(1.0, 0.55, 0.2, 1))
    em = binop(E.MaterialExpressionMultiply, col, k, 650, 100)
    em2 = un(E.MaterialExpressionMultiply, em, 800, 100, const_b=40.0)
    mel.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_nanite", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


FAR_LEN = LAYOUT["chunk_length"] * 5 - abs(LAYOUT["invisible_wall_y"])   # wall to the end of the last repeated chunk
screen_mats["Garland_Filament"] = garland_filament_material(LAYOUT["invisible_wall_y"])


# Outside: sun (lux) and sky brightness, matched to the club's fixed dark-room exposure.
SUN_LUX = 25.0
SKY_BOOST = 4.0


def graph(name, shading=None):
    """New material plus small helpers for building its node graph."""
    mel = unreal.MaterialEditingLibrary
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, "/Game/Scene01", unreal.Material, unreal.MaterialFactoryNew())
    if shading is not None:
        m.set_editor_property("shading_model", shading)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def un(cls, src, x, y, pin="", **props):
        e = node(cls, x, y, **props)
        mel.connect_material_expressions(src, pin, e, "A" if ("const_b" in props or "const_a" in props) else "")
        return e

    def bi(cls, a, b, x, y):
        e = node(cls, x, y)
        mel.connect_material_expressions(a, "", e, "A")
        mel.connect_material_expressions(b, "", e, "B")
        return e

    def done(out, prop=unreal.MaterialProperty.MP_EMISSIVE_COLOR, pin=""):
        mel.connect_material_property(out, pin, prop)
    return m, node, un, bi, done


def texture(tex_name):
    return next((eal.load_asset(p) for p in eal.list_assets(DEST, True)
                 if p.rsplit("/", 1)[-1].split(".")[0] == tex_name), None)


def finish(m):
    unreal.MaterialEditingLibrary.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def hash_(un, bi, node, a, b, x, y):
    """frac(sin(a*12.9898 + b*78.233) * 43758.5): a fresh pseudo-random 0..1 for each (a, b)."""
    ha = un(E_.MaterialExpressionMultiply, a, x, y, const_b=12.9898)
    hb = un(E_.MaterialExpressionMultiply, b, x, y + 60, const_b=78.233)
    sn = un(E_.MaterialExpressionSine, bi(E_.MaterialExpressionAdd, ha, hb, x + 150, y), x + 300, y)
    return un(E_.MaterialExpressionFrac, un(E_.MaterialExpressionMultiply, sn, x + 450, y, const_b=43758.5), x + 600, y)


E_ = unreal


def glitch_material():
    """The owner's broken CCTV monitor: rows tear sideways, the picture jumps and rolls, the
    brightness stutters and now and then it blacks out for a moment."""
    m, node, un, bi, done = graph("M_LiveScreen_Glitch", unreal.MaterialShadingModel.MSM_UNLIT)
    uv = node(E_.MaterialExpressionTextureCoordinate, -2000, 0)
    u = un(E_.MaterialExpressionComponentMask, uv, -1850, 0, r=True, g=False, b=False, a=False)
    v = un(E_.MaterialExpressionComponentMask, uv, -1850, 100, r=False, g=True, b=False, a=False)
    t = node(E_.MaterialExpressionTime, -2000, 300)
    step = un(E_.MaterialExpressionFloor, un(E_.MaterialExpressionMultiply, t, -1850, 300, const_b=12.0), -1700, 300)
    row = un(E_.MaterialExpressionFloor, un(E_.MaterialExpressionMultiply, v, -1700, 100, const_b=24.0), -1550, 100)
    h_row = hash_(un, bi, node, row, step, -1400, 100)
    h_row2 = un(E_.MaterialExpressionFrac, un(E_.MaterialExpressionMultiply, h_row, -750, 200, const_b=91.7), -600, 200)
    # tearing rows (about one in six, each step): shift sideways
    gate = un(E_.MaterialExpressionSaturate, un(E_.MaterialExpressionMultiply,
              un(E_.MaterialExpressionSubtract, h_row, -750, 100, const_b=0.83), -600, 100, const_b=100.0), -450, 100)
    amt = un(E_.MaterialExpressionMultiply, un(E_.MaterialExpressionSubtract, h_row2, -450, 200, const_b=0.5),
             -300, 200, const_b=0.35)
    du = bi(E_.MaterialExpressionMultiply, gate, amt, -150, 100)
    # whole picture: slow roll plus sudden vertical jumps
    zero = node(E_.MaterialExpressionConstant, -1550, 500, r=3.0)
    h_step = hash_(un, bi, node, zero, step, -1400, 500)
    jump = un(E_.MaterialExpressionMultiply, un(E_.MaterialExpressionSaturate, un(E_.MaterialExpressionMultiply,
              un(E_.MaterialExpressionSubtract, h_step, -750, 500, const_b=0.85), -600, 500, const_b=100.0),
              -450, 500), -300, 500, const_b=0.27)
    roll = un(E_.MaterialExpressionMultiply, t, -450, 600, const_b=0.03)
    dv = bi(E_.MaterialExpressionAdd, jump, roll, -150, 500)
    nu = un(E_.MaterialExpressionFrac, bi(E_.MaterialExpressionAdd, u, du, 0, 0), 150, 0)
    nv = un(E_.MaterialExpressionFrac, bi(E_.MaterialExpressionAdd, v, dv, 0, 300), 150, 300)
    sample = node(E_.MaterialExpressionTextureSample, 450, 0, texture=texture("screen_cctv"))
    unreal.MaterialEditingLibrary.connect_material_expressions(bi(E_.MaterialExpressionAppendVector, nu, nv, 300, 100),
                                                               "", sample, "UVs")
    # brightness stutter with blackouts, and scanlines
    one = node(E_.MaterialExpressionConstant, -1550, 800, r=7.0)
    h_b = hash_(un, bi, node, one, step, -1400, 800)
    on = un(E_.MaterialExpressionSaturate, un(E_.MaterialExpressionMultiply,
            un(E_.MaterialExpressionSubtract, h_b, -750, 800, const_b=0.15), -600, 800, const_b=100.0), -450, 800)
    lvl = un(E_.MaterialExpressionAdd, un(E_.MaterialExpressionMultiply, h_b, -450, 900, const_b=0.8), -300, 900,
             const_b=0.4)
    scan = un(E_.MaterialExpressionAdd, un(E_.MaterialExpressionMultiply, un(E_.MaterialExpressionSine,
              un(E_.MaterialExpressionMultiply, v, -600, 1000, const_b=700.0), -450, 1000), -300, 1000, const_b=0.18),
              -150, 1000, const_b=0.82)
    k = bi(E_.MaterialExpressionMultiply, bi(E_.MaterialExpressionMultiply, on, lvl, -150, 850), scan, 0, 900)
    tint = node(E_.MaterialExpressionConstant3Vector, 450, 300, constant=unreal.LinearColor(0.75, 1.0, 0.8, 1))
    rgb = un(E_.MaterialExpressionMultiply, sample, 600, 0, pin="RGB", const_b=1.0)
    col = bi(E_.MaterialExpressionMultiply, rgb, tint, 650, 100)
    em = un(E_.MaterialExpressionMultiply, bi(E_.MaterialExpressionMultiply, col, k, 800, 300), 950, 300, const_b=3.0)
    done(em)
    m.set_editor_property("used_with_nanite", True)
    return finish(m)


def sky_material():
    """The sky dome: unlit panorama, rendered as sky (no fog, no shadows)."""
    m, node, un, bi, done = graph("M_SkyDome", unreal.MaterialShadingModel.MSM_UNLIT)
    sample = node(E_.MaterialExpressionTextureSample, -400, 0, texture=texture("sky_panorama"))
    done(un(E_.MaterialExpressionMultiply, sample, -150, 0, pin="RGB", const_b=SKY_BOOST))
    m.set_editor_property("is_sky", True)
    return finish(m)


def sea_material():
    """Sea far below: the water texture drifting two ways at once, glossy."""
    m, node, un, bi, done = graph("M_Sea")
    uv = node(E_.MaterialExpressionTextureCoordinate, -900, 0)
    t = node(E_.MaterialExpressionTime, -900, 200)
    tex = texture("sea_water")
    outs = []
    for i, (sp, sc) in enumerate((((0.004, 0.002), 1.0), ((-0.003, 0.0035), 0.63))):
        a = un(E_.MaterialExpressionMultiply, uv, -750, i * 300, const_b=sc)
        mv = un(E_.MaterialExpressionMultiply, t, -750, i * 300 + 100, const_b=1.0)
        spd = node(E_.MaterialExpressionConstant2Vector, -750, i * 300 + 200, r=sp[0], g=sp[1])
        b = bi(E_.MaterialExpressionAdd, a, bi(E_.MaterialExpressionMultiply, mv, spd, -600, i * 300 + 100), -450, i * 300)
        smp = node(E_.MaterialExpressionTextureSample, -300, i * 300, texture=tex)
        unreal.MaterialEditingLibrary.connect_material_expressions(b, "", smp, "UVs")
        outs.append(un(E_.MaterialExpressionMultiply, smp, -150, i * 300, pin="RGB", const_b=1.0))
    avg = un(E_.MaterialExpressionMultiply, bi(E_.MaterialExpressionAdd, outs[0], outs[1], -100, 100), 50, 100,
             const_b=0.5)
    done(avg, unreal.MaterialProperty.MP_BASE_COLOR)
    done(node(E_.MaterialExpressionConstant, 50, 300, r=0.12), unreal.MaterialProperty.MP_ROUGHNESS)
    m.set_editor_property("used_with_nanite", True)
    return finish(m)


screen_mats["Screen_On_live_glitch"] = glitch_material()
screen_mats["Screen_On_sky"] = sky_material()
screen_mats["Outside_Sea"] = sea_material()

# Visitors' screens: content matched to who sits there, always in motion.
LIVE = {
    "Screen_On_live_feed": ("screen_feed", (1, 0.32), (0, 0), (0, 0.018)),
    "Screen_On_live_video": ("screen_video", (1, 1), (0, 0), (0, 0)),
    "Screen_On_live_cctv": ("screen_cctv", (1, 1), (0, 0), (0, 0)),
    "Screen_On_live_chat": ("screen_chat", (1, 0.35), (0, 0), (0, 0.012)),
    "Screen_On_live_racing": ("screen_racing", (1, 0.45), (0, 0), (0, -0.45)),
    "Screen_On_live_platformer": ("screen_platformer", (0.4, 1), (0, 0), (0.035, 0)),
    "Screen_On_live_zx": ("screen_zx", (1, 1), (0, 0), (0, 0)),
    "Screen_On_live_modern_game": ("screen_modern_game", (0.9, 0.9), (0.05, 0.05), (0, 0)),
}
for k, (tex_name, sc_, off_, spd_) in LIVE.items():
    extra = {}
    if k.endswith("modern_game"):
        extra = {"sway": (0.02, 0.012), "flicker": 0.08}
    if k.endswith("zx"):
        extra = {"flicker": 0.12}
    if k.endswith("video"):
        extra = {"sway": (0.004, 0.003), "flicker": 0.05}
    if k.endswith("cctv"):
        extra = {"flicker": 0.15}
    screen_mats[k] = live_screen_material(k.replace("Screen_On_live_", "LiveScreen_"), tex_name, sc_, off_, spd_, **extra)


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


# Garland: every little bulb is its own light (tiny radius, no shadows, so dozens stay cheap), on both sides
# of the invisible wall. Beyond it the bulbs carry FarFlicker, which makes them glow less and blink more and
# more toward the end of the tunnel; on our side some of them blink now and then (ALightFlicker).
rng = random.Random(7)
for k, p in enumerate(LAYOUT["garland_lights"]):
    col = GARLAND_COLORS[k % len(GARLAND_COLORS)]
    gl = point_light(vec(p), unreal.LinearColor(col[0], col[1], col[2], 1.0), 0.35, 160.0,
                     "Garland_Light_%03d" % k, "Lights/Garland")
    gl.light_component.set_editor_property("source_radius", 1.0)
    if p[1] < WALL_Y:
        gl.tags = [unreal.Name("FarFlicker")]
    elif rng.random() < 0.2:
        gl.tags = [unreal.Name("Flicker")]
flicker = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.LightFlicker"), unreal.Vector(0, 0, 300), label="GarlandFlicker")
flicker.set_editor_property("far_start_y", WALL_Y)
flicker.set_editor_property("far_end_y", WALL_Y - FAR_LEN)

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
    # the machine itself: its tray and the buy / take / drink behaviour (the model's front faces +X)
    vend_cls = unreal.load_class(None, "/Script/Lyudmilovsk.VendingMachine")
    if vend_cls:
        v = LAYOUT["vending"]
        vm = spawn(vend_cls, unreal.Vector(v[0], v[1], 0.0), label="VendingMachine_Logic")
        vm.set_folder_path("Props")

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
        "Painting_", "FoodVending", "Clerk", "Partition", "Chandelier", "Owner", "Visitor_", "Outside_")
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

# Partition between hall and corridor: a wide double door standing open (E swings it shut/open).
if "door" in LAYOUT:
    labels = LAYOUT["door"]["leaves"]
    leaves = [next((x for x in eas.get_all_level_actors() if x.get_actor_label() == l), None) for l in labels]
    leaves = [x for x in leaves if x]
    if leaves:
        for lf in leaves:
            lf.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        door = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.SwingDoor"), vec(LAYOUT["door"]["center"]),
                     label="PartitionDoor_Actor")
        door.set_editor_property("leaves", leaves)
        door.set_editor_property("open_angles", LAYOUT["door"]["open_angles"][:len(leaves)])
        door.set_editor_property("leaves_placed_open", True)

if "exit_door" in LAYOUT:
    ed = LAYOUT["exit_door"]
    by_label = {x.get_actor_label(): x for x in eas.get_all_level_actors()}
    leaves = [by_label[l] for l in ed["leaves"] if l in by_label]
    for lf in leaves:
        lf.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    for r in ed.get("riders", []):
        if r in by_label and leaves:
            by_label[r].root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            by_label[r].attach_to_actor(leaves[0], "", unreal.AttachmentRule.KEEP_WORLD,
                                        unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    if leaves:
        xd = spawn(unreal.load_class(None, "/Script/Lyudmilovsk.SwingDoor"), vec(ed["center"]), label="ExitDoor_Actor")
        xd.set_editor_property("leaves", leaves)
        xd.set_editor_property("open_angles", ed["open_angles"][:len(leaves)])
        xd.set_editor_property("leaves_placed_open", False)
        xd.set_editor_property("start_open", False)
        xd.set_editor_property("swing_speed", 55.0)
    log("exit door: %d leaves" % len(leaves))

# Outside: a grassy hilltop over the sea under a late afternoon sun. The sun and its fill only
# light lighting channel 1 (the outside), so nothing of it reaches the dark club inside.
if "outside" in LAYOUT:
    for a in eas.get_all_level_actors():
        lab = a.get_actor_label()
        if isinstance(a, unreal.StaticMeshActor) and lab.startswith(("Outside_", "ExitDoor")):
            c = a.static_mesh_component
            c.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
            if lab == "Outside_SkyDome":
                c.set_editor_property("cast_shadow", False)
                c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for label, rot, lux, shadows in (("Sun", unreal.Rotator(0, -40, 60), SUN_LUX, True),
                                     ("Sun_Fill", unreal.Rotator(0, -40, -160), SUN_LUX * 0.3, False)):
        sun = spawn(unreal.DirectionalLight, vec(LAYOUT["outside"]["center"]) + unreal.Vector(0, 0, 800), rot, label)
        dl = sun.get_component_by_class(unreal.DirectionalLightComponent)
        dl.set_mobility(unreal.ComponentMobility.MOVABLE)
        dl.set_intensity(lux)
        dl.set_light_color(unreal.LinearColor(1.0, 0.86, 0.7, 1) if shadows else unreal.LinearColor(0.6, 0.75, 1.0, 1))
        dl.set_editor_property("lighting_channels", unreal.LightingChannels(False, True, False))
        dl.set_editor_property("cast_shadows", shadows)
        dl.set_editor_property("forward_shading_priority", 1 if shadows else 0)
        sun.set_folder_path("Outside")

# Starlings (the murmuration thread's AStarlingMurmuration): a flock over the sea 300 m out from
# the portal; it builds its meshes at runtime and traces the hill and sea under it at BeginPlay.
flock_cls = unreal.load_class(None, "/Script/Lyudmilovsk.StarlingMurmuration")
if "outside" in LAYOUT and flock_cls:
    spawn(flock_cls, unreal.Vector(0, LAYOUT["outside"]["portal_face_y"] + 30000, LAYOUT["outside"]["sea_z"]),
          label="Starlings").set_folder_path("Outside")
    log("starlings placed")
# A poplar (the plants thread's AProceduralPlant) on the grass beside the path from the door; it grows
# from a sprout with the game clock and snaps to the hill at BeginPlay.
plant_cls = unreal.load_class(None, "/Script/Lyudmilovsk.ProceduralPlant")
if "outside" in LAYOUT and plant_cls:
    spawn(plant_cls, unreal.Vector(-450, LAYOUT["outside"]["portal_face_y"] + 650, 0),
          label="Poplar").set_folder_path("Outside")
    log("poplar placed")
    # An old apple tree on the other side of the path; its calendar runs fast (a month in ~100 s)
    # so the leaves turn and the ripe apples drop (pick them up with E).
    apple = spawn(plant_cls, unreal.Vector(450, LAYOUT["outside"]["portal_face_y"] + 900, 0), label="AppleTree")
    for k, v in (("species", "Apple"), ("seed", 6), ("age_at_start", 25.0), ("demo_growth", False),
                 ("demo_days_per_second", 0.3)):
        apple.set_editor_property(k, v)
    apple.set_folder_path("Outside")
    log("apple tree placed")

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

FOG_EXTINCTION = 10.0
LFV = getattr(unreal, "LocalFogVolume", None)
if LFV:
    # A long black fog volume over the corridor beyond the wall: the further, the darker.
    span = CHUNK * (COPIES + 1)
    lfv = spawn(LFV, unreal.Vector(0, WALL_Y - span * 0.42, 150), label="Fog_Corridor")
    lfv.set_actor_scale3d(unreal.Vector(4.0, span * 0.5 / 500.0, 4.0))
    lc = lfv.get_component_by_class(unreal.LocalFogVolumeComponent)
    for k, v in (("radial_fog_extinction", FOG_EXTINCTION), ("height_fog_extinction", 0.0),
                 ("fog_albedo", unreal.LinearColor(0, 0, 0, 1)), ("fog_emissive", unreal.LinearColor(0, 0, 0, 1))):
        try:
            lc.set_editor_property(k, v)
        except Exception as e:
            log("fog volume: %s: %s" % (k, e))
fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, -4500, 0), label="Fog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
# Clear around the player, then the corridor slowly sinks into darkness with distance.
fc.set_editor_property("fog_density", 0.0 if LFV else 0.08)
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

# The clerk NPC at Station 09, painting in a spreadsheet: his body and an animated desk rig (mouse
# with its cable and the Excel picture on the CRT) run the same 8 s loop from 0, so they stay in step.
# The meshes face +Y at yaw 0.
CLERK = LAYOUT.get("clerk")
CLERK_PARTS = [("Clerk", "/Game/Characters/Humans/Clerk/SK_Clerk", "/Game/Characters/Clerk/A_Clerk_Sit_Excel"),
               ("ClerkDesk", "/Game/Characters/Clerk/Desk/clerk_desk/SkeletalMeshes/clerk_desk",
                "/Game/Characters/Clerk/Desk/clerk_desk/SkeletalMeshes/clerk_desk_Anim")]
if CLERK and all(eal.does_asset_exist(m) and eal.does_asset_exist(a_) for _, m, a_ in CLERK_PARTS):
    for label, mesh, anim in CLERK_PARTS:
        act = spawn(unreal.SkeletalMeshActor, vec(CLERK["chair"]), unreal.Rotator(0, 0, CLERK["chair_yaw"] - 90), label)
        sk = act.skeletal_mesh_component
        sk.set_skeletal_mesh_asset(eal.load_asset(mesh))
        sk.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        sk.override_animation_data(eal.load_asset(anim), True, True, 0.0, 1.0)
        sk.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        act.set_folder_path("People")
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
    log("clerk assets missing")

# People: the owner in his booth and a crowd at the reachable stations (meshes and loops from
# the humans thread; each mesh faces +Y at yaw 0 and stands on the floor under the seat centre).
HUMANS = "/Game/Characters/Humans/"


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


OWNER_ANIMS = {"sit_anim": "Sit_Idle", "stand_up_anim": "StandUp", "walk_anim": "Walk", "stand_anim": "StandLook",
               "sit_down_anim": "SitDown"}
owner_cls = unreal.load_class(None, "/Script/Lyudmilovsk.BoothOwner")
if "owner" in LAYOUT and owner_cls and eal.does_asset_exist(HUMANS + "Owner/SK_Owner"):
    o = LAYOUT["owner"]
    owner = spawn(owner_cls, vec(o["zone_center"]), label="Owner")
    owner.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(*o["zone_extent"]))
    body = owner.get_component_by_class(unreal.SkeletalMeshComponent)
    body.set_skeletal_mesh_asset(eal.load_asset(HUMANS + "Owner/SK_Owner"))
    owner.set_editor_property("seat_location", vec(o["chair"]))
    owner.set_editor_property("seat_yaw", o["sitter_yaw"])
    owner.set_editor_property("window_location", vec(o["window"]))
    owner.set_editor_property("window_yaw", o["window_yaw"])
    found = []
    for prop_name, anim in OWNER_ANIMS.items():
        path = HUMANS + "Owner/Anims/A_Owner_" + anim
        if eal.does_asset_exist(path):
            owner.set_editor_property(prop_name, eal.load_asset(path))
            found.append(anim)
    body.set_world_location_and_rotation(vec(o["chair"]), unreal.Rotator(0, 0, o["sitter_yaw"] - 90.0), False, True)
    body.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    body.override_animation_data(eal.load_asset(HUMANS + "Owner/Anims/A_Owner_Sit_Idle"), True, True)
    owner.set_folder_path("People")
    log("owner: anims %s" % found)
# Visitors sit where the layout put them (their chairs are square and their screens are live);
# the loop follows the device: gamepad at consoles, keyboard+mouse at PCs, browsing on laptops.
# Each person's own loop from the humans thread (poses checked there).
ANIM = {"Streamer": "Sit_Stream", "Kid_boy": "Sit_Gamepad", "Kid_girl": "Sit_ChinRest", "Guy_a": "Sit_KeyboardMouse",
        "Guy_b": "Sit_LeanBack", "Girl_a": "Sit_KeyboardMouse", "Girl_b": "Sit_Browse"}
CROWD_ANIM = {11: "Sit_Browse", 12: "Sit_KeyboardMouse", 13: "Sit_Browse"}
seats = {st["index"]: st for st in LAYOUT.get("stations", [])}
for v in LAYOUT.get("visitors", []):
    st = seats.get(v["index"])
    if st:
        crowd = v["index"] > 10     # extra people past the hero (HERO_INDEX) hide the invisible wall
        p = seat_person(v["person"], CROWD_ANIM.get(v["index"], ANIM.get(v["person"], "Sit_Idle")), vec(st["chair"]),
                        st["sitter_yaw"], "Visitor_" + v["person"] + ("_%02d" % v["index"] if crowd else ""))
        if p and crowd:
            p.tags = [unreal.Name("NoTalk")]
        log("%s at station %02d (%s)" % (v["person"], v["index"], v["kind"]))


# Screens light their users: a soft glow in front of each reachable station's display.
GLOW = {"gaming": (0.55, 0.6, 1.0), "sega": (0.9, 0.7, 0.6), "spreadsheet": (0.8, 0.9, 1.0), "vr": (0.5, 0.8, 1.0),
        "modern": (0.6, 0.7, 1.0), "ps2": (0.6, 0.7, 0.9), "beige": (0.75, 0.85, 1.0), "zx": (0.7, 0.8, 0.9),
        "laptop": (0.85, 0.9, 1.0), "imac": (0.7, 0.9, 1.0), "mac": (0.9, 0.9, 0.85)}
for st in LAYOUT.get("stations", []):
    if not st["reachable"] or (CLERK and st["chair"][1] == CLERK["chair"][1]):   # the clerk has his own glow
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
