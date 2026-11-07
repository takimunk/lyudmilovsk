"""Presentable Blender renders of the restaurant with its tables set (EEVEE, 1920x1080) -> shots/blender_*.png.

  Blender -b -P blender/restaurant/render_scene.py [-- shot ...]
  shots: promenade_day sea_day dusk hall seated_pov table_close pass stages
The building is design/restaurant/game/restaurant_kit.glb, the dishes design/restaurant/game/tableware_kit.glb, placed by
unreal/Lyudmilovsk/Content/Restaurant/layout.json and design/restaurant/tableware.json the way the game does."""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import restaurant_common as C  # noqa: E402
from tableware_mats import MAT_SPEC as TMAT  # noqa: E402

ROOT = C.ROOT
GAME = os.path.join(ROOT, "design", "restaurant", "game")
OUT = os.path.join(ROOT, "shots")
LAY = json.load(open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Restaurant", "layout.json"), encoding="utf-8"))
TW = json.load(open(os.path.join(ROOT, "design", "restaurant", "tableware.json"), encoding="utf-8"))
DI = TW["dishes"]
TABLES = {t["id"]: t for t in LAY["tables"]}
SPEC = dict(C.MAT_SPEC)
SPEC.update(TMAT)

GLASS = {"RS_Glass": 0.16, "RS_T_Glass": 0.3, "RS_T_GlassBase": 0.5}
STATE = {"dusk": False}
SRC = {}      # name -> source object


def lin(hx):
    return tuple(C._lin(hx))


def set_mats():
    dusk = STATE["dusk"]
    for m in bpy.data.materials:
        key = m.name.split(".")[0]
        if not m.node_tree or key not in SPEC:
            continue
        b = m.node_tree.nodes.get("Principled BSDF")
        if not b:
            continue
        hx, rough, metal, _e = SPEC[key]
        b.inputs["Base Color"].default_value = lin(hx) + (1,)
        b.inputs["Roughness"].default_value = rough
        b.inputs["Metallic"].default_value = metal
        glow = 0.0
        if key == "RS_Opal":
            glow = 7.0 if dusk else 1.2
        elif key == "RS_HeatLamp":
            glow = 5.0
        elif key == "RS_SignGlow":
            glow = 14.0 if dusk else 0.0
        b.inputs["Emission Color"].default_value = lin(hx) + (1,)
        b.inputs["Emission Strength"].default_value = glow
        b.inputs["Alpha"].default_value = 1.0
        if key in GLASS:
            b.inputs["Alpha"].default_value = GLASS[key]
            b.inputs["Roughness"].default_value = 0.03
            if hasattr(m, "surface_render_method"):
                m.surface_render_method = "BLENDED"
            if hasattr(m, "use_backface_culling"):
                m.use_backface_culling = False


def flat_mat(name, hx, rough=0.9, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = lin(hx) + (1,)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m


def plane(name, x0, x1, y0, y1, z, mat):
    me = bpy.data.meshes.new(name)
    me.from_pydata([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], [], [(0, 1, 2, 3)])
    o = bpy.data.objects.new(name, me)
    me.materials.append(mat)
    bpy.context.scene.collection.objects.link(o)
    return o


def env():
    conc = flat_mat("pv_concrete", "#8F8B83", 0.9)
    sea = flat_mat("pv_sea", "#2F6F8F", 0.12)
    plane("ground", -150, 22, -150, 150, -0.6, conc)
    plane("sea", 22, 600, -600, 600, -1.7, sea)
    me = bpy.data.meshes.new("quay")
    me.from_pydata([(22, -150, -0.6), (22, 150, -0.6), (22, 150, -1.7), (22, -150, -1.7)], [], [(0, 1, 2, 3)])
    o = bpy.data.objects.new("quay", me)
    me.materials.append(conc)
    bpy.context.scene.collection.objects.link(o)
    plane("far_land", -600, -150, -600, 600, -0.6, conc)


def load():
    bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, "restaurant_kit.glb"))
    n0 = {o.name for o in bpy.data.objects}
    bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, "tableware_kit.glb"))
    for o in bpy.data.objects:
        if o.name not in n0 and o.type == "MESH":
            SRC[o.name] = o
            o.hide_render = True
            o.hide_viewport = True


def inst(name, loc, rz=0.0, scale=(1, 1, 1)):
    s = SRC.get(name)
    if s is None:
        print("MISSING", name, flush=True)
        return None
    o = bpy.data.objects.new(name + "_i", s.data)
    o.location = tuple(loc)
    o.rotation_euler = (0, 0, rz)
    o.scale = scale
    bpy.context.scene.collection.objects.link(o)
    return o


def interp(tab, t):
    for (a, va), (b, vb) in zip(tab, tab[1:]):
        if a <= t <= b:
            return va + (vb - va) * (t - a) / (b - a) if b > a else vb
    return tab[-1][1]


def dish(d, loc, rz, stage=0, level=None):
    """The vessel at loc (bottom at loc.z), the food stage / fill on it."""
    J = DI[d]
    z = loc[2]
    if J.get("saucer"):
        inst(J["saucer"], loc, rz)
        z += J.get("saucer_z", 0.0)
    v = J["vessel"]
    inst(v, (loc[0], loc[1], z), rz)
    if (v + "_Glass") in SRC:
        inst(v + "_Glass", (loc[0], loc[1], z), rz)
    fz = z + J["fill_z"]
    if J.get("fill"):
        t = 1.0 if level is None else level
        k = interp(J["fill_xy"], t) if J.get("fill_xy") else 1.0
        inst(J["fill"], (loc[0], loc[1], fz), rz, (k, k, max(t, 0.05)))
    if J.get("stages", 0):
        inst("SM_RF_%s_%d" % (d, stage), (loc[0], loc[1], fz), rz)


def vec(yaw):
    a = math.radians(yaw)
    return Vector((math.cos(a), math.sin(a), 0)), Vector((math.sin(a), -math.cos(a), 0))   # forward, right


def at(p, f, r, df=0.0, dr=0.0):
    q = Vector(p) + f * df + r * dr
    return (q.x, q.y, p[2])


def set_table1():
    t = TABLES["t1"]
    c = Vector(t["at"])
    top = c.z
    plan = [("borscht", None), ("steak", "coffee"), ("ramen", "mors"), ("khachapuri", None)]
    for s, (main, drink) in zip(t["seats"], plan):
        f, r = vec(s["yaw"])
        p = s["place"]
        rz = math.radians(s["yaw"] + 90)
        yw = math.radians(s["yaw"])
        dish(main, p, rz)
        if drink:
            dish(drink, at(p, f, r, 0.2, 0.2), rz)
        if main in ("borscht", "steak"):
            inst("SM_RT_Fork", at(p, f, r, -0.06, -0.2), yw)
            inst("SM_RT_Knife", at(p, f, r, -0.06, 0.2), yw)
            inst("SM_RT_Spoon", at(p, f, r, -0.06, 0.26), yw)
        if main == "ramen":
            inst("SM_RT_Chopsticks", at(p, f, r, 0.2, -0.04), yw + math.pi / 2)
        if main == "khachapuri":
            inst("SM_RT_Napkin", at(p, f, r, 0.0, -0.3), yw + math.pi / 2)
    dish("carafe_water", (c.x - 0.07, c.y + 0.07, top), 0.0)
    inst("SM_RT_BreadBasket", (c.x + 0.07, c.y - 0.1, top), 0.5)
    inst("SM_RF_bread_0", (c.x + 0.07, c.y - 0.1, top + DI["bread"]["fill_z"]), 0.5)
    inst("SM_RT_SaltPepper", (c.x + 0.12, c.y + 0.14, top), 0.3)
    inst("SM_RT_MenuCard", (c.x + 0.04, c.y + 0.05, top), 0.9)


def set_table2():
    S = TABLES["t2"]["seats"]
    s = S[0]
    f, r = vec(s["yaw"])
    p = s["place"]
    dish("pelmeni", p, math.radians(s["yaw"] + 90))
    dish("tea", at(p, f, r, 0.2, 0.2), 0.0)
    inst("SM_RT_Fork", at(p, f, r, -0.06, -0.2), math.radians(s["yaw"]))
    s = S[2]
    f, r = vec(s["yaw"])
    dish("sushi_set", s["place"], math.radians(s["yaw"] + 90))
    inst("SM_RT_Chopsticks", at(s["place"], f, r, -0.15, 0.0), math.radians(s["yaw"] + 90))
    s = S[1]
    f, r = vec(s["yaw"])
    dish("plombir", s["place"], math.radians(s["yaw"] + 90))
    inst("SM_RT_TeaSpoon", at(s["place"], f, r, 0.0, 0.17), math.radians(s["yaw"]))


def set_table7():
    t = TABLES["t7"]
    s = t["seats"][0]
    f, r = vec(s["yaw"])
    p = s["place"]
    dish("baltic_fish", p, math.radians(s["yaw"] + 90))
    dish("coffee", at(p, f, r, 0.2, 0.2), 0.0)
    inst("SM_RT_Fork", at(p, f, r, -0.06, -0.2), math.radians(s["yaw"]))
    inst("SM_RT_Knife", at(p, f, r, -0.06, 0.2), math.radians(s["yaw"]))
    s = t["seats"][1]
    f, r = vec(s["yaw"])
    dish("medovik", s["place"], math.radians(s["yaw"] + 90))
    dish("tea", at(s["place"], f, r, 0.2, 0.2), 0.0)


def set_pass():
    sh = LAY["pass"]["shelf"]
    dish("pasta", sh[0], math.radians(0))
    dish("olivier", sh[1], math.radians(0))
    bz = sh[-1][2]
    inst("SM_RT_ServiceBell", (sh[-1][0], sh[-1][1] + 0.45, bz))
    inst("SM_RT_ServiceBell_Plunger", (sh[-1][0], sh[-1][1] + 0.45, bz + 0.0305))
    inst("SM_RT_TicketRail", (-8.75, 0.0, 1.92), math.radians(90))
    ck = LAY["cook"]["at"]
    zr = 0.92
    inst("SM_RT_FryPan", (ck[0] + 0.6, ck[1] - 0.3, zr), math.radians(-20))
    inst("SM_RT_PanFood", (ck[0] + 0.6, ck[1] - 0.3, zr + 0.005), math.radians(-20))
    inst("SM_RT_StockPot", (ck[0] + 0.6, ck[1] + 0.35, zr))


def stage_sheet(base):
    cloth = flat_mat("cloth", "#EDEDE8", 0.9)
    plane("sheet_cloth", base[0] - 3, base[0] + 3, base[1] - 3, base[1] + 3, base[2] - 0.0005, cloth)
    for i, d in enumerate(["steak", "ramen", "khachapuri"]):
        for j, st in enumerate((0, 2, 4)):
            p = (base[0] + (j - 1) * 0.5, base[1] + (1 - i) * 0.5, base[2])
            level = DI[d]["stage_fill"][st] if DI[d].get("stage_fill") else None
            dish(d, p, 0.0, stage=st, level=level)
            if d == "steak":
                inst("SM_RT_Fork", (p[0] - 0.19, p[1] - 0.02, p[2]), math.radians(90))
                inst("SM_RT_Knife", (p[0] + 0.19, p[1] - 0.02, p[2]), math.radians(90))


def world(dusk, el):
    w = bpy.data.worlds.new("w")
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputWorld")
    bg = nt.nodes.new("ShaderNodeBackground")
    sky = nt.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "MULTIPLE_SCATTERING"
    sky.sun_elevation = math.radians(el)
    sky.sun_rotation = math.radians(210)
    sky.sun_size = math.radians(1.0)
    sky.sun_disc = False
    nt.links.new(sky.outputs[0], bg.inputs[0])
    bg.inputs[1].default_value = float(os.environ.get("WS", "0.5")) if dusk else float(os.environ.get("WS", "0.12"))
    nt.links.new(bg.outputs[0], out.inputs[0])
    return w


def look(cam, eye, tgt, lens, hfov=None):
    cam.location = eye
    cam.rotation_euler = (Vector(tgt) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    cam.data.sensor_fit = "HORIZONTAL"
    cam.data.sensor_width = 36
    cam.data.lens = lens if hfov is None else 18.0 / math.tan(math.radians(hfov / 2))
    cam.data.clip_start = 0.03
    cam.data.clip_end = 2000


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    bpy.ops.wm.read_factory_settings(use_empty=True)
    load()
    env()
    set_table1()
    set_table2()
    set_table7()
    set_pass()
    base = (300.0, 300.0, 80.0)
    stage_sheet(base)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
    sc.render.resolution_percentage = int(os.environ.get("PCT", "100"))
    try:
        sc.eevee.taa_render_samples = 48
        sc.eevee.shadow_ray_count = 1
        sc.eevee.shadow_pool_size = "1024"
        sc.eevee.shadow_step_count = 6
    except Exception:  # noqa: BLE001
        pass
    sc.view_settings.exposure = float(os.environ.get("EXPO", "-0.3"))
    vts = [e.identifier for e in bpy.types.ColorManagedViewSettings.bl_rna.properties["view_transform"].enum_items]
    sc.view_settings.view_transform = "AgX" if "AgX" in vts else "Standard"
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sc.collection.objects.link(sun)
    sun.data.angle = math.radians(1.0)
    pls = []
    for L in LAY["lights"]:
        o = bpy.data.objects.new("pl", bpy.data.lights.new("pl", "POINT"))
        o.data.color = lin(L["colour"])
        o.location = L["at"]
        o.data.shadow_soft_size = 0.25
        sc.collection.objects.link(o)
        pls.append((o, L))
    fill = bpy.data.objects.new("fill", bpy.data.lights.new("fill", "AREA"))
    fill.data.shape = "DISK"
    fill.data.size = 18
    fill.data.color = (1.0, 0.92, 0.8)
    fill.location = (0, 0, 3.8)
    fill.rotation_euler = (math.pi, 0, 0)
    sc.collection.objects.link(fill)

    SHOTS = {
        "promenade_day": ((12.0, -25.5, 1.7), (-0.5, -6.0, 3.2), 22, False, None),
        "sea_day": ((33.0, 8.0, 0.8), (0.0, 0.0, 3.0), 22, False, None),
        "dusk": ((12.0, -25.5, 1.7), (-0.5, -6.0, 3.2), 22, True, None),
        "hall": ((-1.4, -8.9, 1.6), (6.5, 2.5, 1.5), 17, False, None),
        "seated_pov": (None, None, None, False, 90),
        "table_close": ((6.35, -1.05, 1.3), (7.2, 0.0, 0.8), 40, False, None),
        "pass": ((-5.9, -1.9, 1.55), (-8.4, 0.5, 1.35), 20, False, None),
        "stages": ((base[0], base[1] - 0.001, base[2] + 2.3), (base[0], base[1], base[2]), 36, False, None),
    }
    s0 = TABLES["t1"]["seats"][0]
    f, _r = vec(s0["yaw"])
    eye = Vector(s0["at"]) - f * 0.2
    eye.z = 1.18
    p = math.radians(28)
    d = Vector((f.x * math.cos(p), f.y * math.cos(p), -math.sin(p)))
    SHOTS["seated_pov"] = (tuple(eye), tuple(eye + d), None, False, 90)
    os.makedirs(OUT, exist_ok=True)
    for name, (eye, tgt, lens, dusk, hfov) in SHOTS.items():
        if args and name not in args:
            continue
        STATE["dusk"] = dusk
        set_mats()
        sc.world = world(dusk, -2.5 if dusk else 38)
        el = math.radians(-2.5 if dusk else 38)
        sun.rotation_euler = (math.radians(90) - el, 0, math.radians(210) + math.pi)
        sun.data.color = (1.0, 0.93, 0.82)
        sun.data.energy = 0.0 if dusk else float(os.environ.get("SUN", "5"))
        for o, L in pls:
            if L["kind"] == "inside":
                o.data.energy = L["lumens"] / (1.5 if dusk else 8.0)
            else:
                o.data.energy = (L["lumens"] / 0.8) if dusk else 0.0
        fill.data.energy = 0.0 if dusk else 150
        if name in ("seated_pov", "table_close", "pass"):
            fill.data.energy = 260
        if name == "stages":
            fill.data.energy = 0.0
            sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
            sun.data.color = (1.0, 0.93, 0.82)
        sun.data.energy = 3.5
        look(cam, eye, tgt, lens, hfov)
        sc.render.filepath = os.path.join(OUT, "blender_%s.png" % name)
        bpy.ops.render.render(write_still=True)
        print("SHOT", sc.render.filepath, flush=True)


main()
