"""Contact sheets of the restaurant's tableware kit (design/restaurant/game/tableware_kit.glb), placed by
design/restaurant/tableware.json exactly as the game would (vessel at its origin, food at fill_z, fills scaled by
stage_fill / fill_xy, loads on their utensils), on a white tablecloth. Workbench, flat slot colours.

  Blender -b -P blender/restaurant/preview_tableware.py [-- sheet ...]
  sheets: dishes (dishes_sheet.png), stages (stages_all.png), close (close_<dish>.png)
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tableware_mats import MAT_SPEC  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(ROOT, "design", "restaurant", "game", "tableware_kit.glb")
PREV = os.path.join(ROOT, "design", "restaurant", "previews")
CELLS = os.path.join(PREV, "cells")
J = json.load(open(os.path.join(ROOT, "design", "restaurant", "tableware.json")))
DI = J["dishes"]


def lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=KIT)
    for m in bpy.data.materials:
        k = m.name.split(".")[0]
        if k in MAT_SPEC:
            m.diffuse_color = lin(MAT_SPEC[k][0]) + ((0.28,) if k == "RS_T_Glass" else (1.0,))
            m.metallic = MAT_SPEC[k][2] * 0.5
            m.roughness = MAT_SPEC[k][1]
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.display.shading.light = "STUDIO"
    sc.display.shading.color_type = "MATERIAL"
    sc.display.shading.show_object_outline = False
    sc.display.shading.show_cavity = True
    sc.display.shading.cavity_type = "WORLD"
    sc.display.shading.show_shadows = True
    sc.display.shading.shadow_intensity = 0.35
    sc.display.light_direction = (0.45, -0.35, 0.82)
    sc.view_settings.view_transform = "Standard"
    sc.render.film_transparent = False
    if not sc.world:
        sc.world = bpy.data.worlds.new("W")
    sc.world.color = (0.80, 0.80, 0.80)
    cloth = bpy.data.meshes.new("cloth")
    cloth.from_pydata([(-2, -2, -0.0002), (2, -2, -0.0002), (2, 2, -0.0002), (-2, 2, -0.0002)], [], [(0, 1, 2, 3)])
    cm = bpy.data.materials.new("Cloth")
    cm.diffuse_color = lin("#EDEDE8") + (1,)
    cloth.materials.append(cm)
    co = bpy.data.objects.new("Cloth", cloth)
    sc.collection.objects.link(co)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    sc.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.clip_start, cam.data.clip_end = 0.01, 20
    sc.camera = cam
    txt = bpy.data.curves.new("label", "FONT")
    to = bpy.data.objects.new("Label", txt)
    tm = bpy.data.materials.new("Ink")
    tm.diffuse_color = (0.05, 0.05, 0.05, 1)
    txt.materials.append(tm)
    sc.collection.objects.link(to)
    return sc, cam, to


def place(items):
    """items: [(mesh name, (x, y, z), scale (sx, sy, sz) or None, rot z)] -> shows only those."""
    used = set()
    for o in bpy.data.objects:
        if o.type == "MESH" and o.name != "Cloth":
            o.hide_render = True
    objs = []
    for name, loc, scl, rz in items:
        o = bpy.data.objects.get(name)
        if o is None:
            print("MISSING", name, flush=True)
            continue
        if name in used:                         # the same mesh twice in one picture: a linked copy
            o2 = o.copy()
            bpy.context.scene.collection.objects.link(o2)
            o2["tmp"] = 1
            o = o2
        used.add(name)
        o.hide_render = False
        o.location = loc
        o.scale = scl or (1, 1, 1)
        o.rotation_mode = "XYZ"
        o.rotation_euler = (0, 0, rz)
        objs.append(o)
    return objs


def clear_tmp():
    for o in list(bpy.data.objects):
        if o.get("tmp"):
            bpy.data.objects.remove(o)


def render(path, objs, label, view=(0.75, -1.0, 0.95), res=360, zoom=0.80, focus=None):
    sc = bpy.context.scene
    cam, to = sc.camera, bpy.data.objects["Label"]
    sc.render.resolution_x = sc.render.resolution_y = res
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in (focus or objs) for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    ctr = (lo + hi) / 2
    size = max((hi - lo).length, 0.05)
    d = Vector(view).normalized()
    cam.location = ctr + d * 3.0
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.ortho_scale = size * zoom / 0.80 * 0.95
    s = cam.data.ortho_scale
    to.data.body = label
    to.data.size = s * 0.055
    to.rotation_euler = cam.rotation_euler
    to.location = cam.matrix_world @ Vector((-0.47 * s, -0.47 * s, -1.0))
    to.hide_render = False
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    clear_tmp()


def fxy(e, t):
    tab = e["fill_xy"]
    for (a, b), (c, d) in zip(tab, tab[1:]):
        if a <= t <= c:
            return b + (d - b) * (t - a) / (c - a)
    return tab[-1][1]


def dish_items(dish, k=0):
    e = DI[dish]
    it = []
    z0 = 0.0
    if dish == "coffee":
        it.append(("SM_RT_Saucer", (0, 0, 0), None, 0))
        z0 = e["saucer_z"]
    if dish == "water":
        it += [("SM_RT_Carafe", (0.07, 0.05, 0), None, 0), ("SM_RT_Carafe_Glass", (0.07, 0.05, 0), None, 0)]
        ec = DI["carafe_water"]
        it.append((ec["fill"], (0.07, 0.05, ec["fill_z"]), None, 0))
    it.append((e["vessel"], (0, 0, z0), None, 0))
    if e["vessel"] in ("SM_RT_Tumbler", "SM_RT_Carafe", "SM_RT_GlassMug", "SM_RT_TeaHolder"):
        it.append((e["vessel"] + "_Glass", (0, 0, z0), None, 0))
    if e["fill"]:
        t = e["stage_fill"][k] if "stage_fill" in e else 1.0
        s = fxy(e, t)
        it.append((e["fill"], (0, 0, z0 + e["fill_z"]), (s, s, t), 0))
    if e["stages"]:
        it.append(("SM_RF_%s_%d" % (dish, k), (0, 0, z0 + e["fill_z"]), None, 0))
    return it


def load_items(dish, big):
    e = DI[dish]
    ld = e["load_b" if big else "load_s"]
    it = [(ld, (0, 0, 0), None, 0)]
    u = e["utensil"]
    if u in ("fork", "knife_fork"):
        it.append(("SM_RT_Fork", (-J["TINE_X"], 0, -J["LOAD_Z"]), None, 0))
    elif u == "spoon":
        it.append(("SM_RT_Spoon", (-J["SPOON_X"], 0, -J["SPOON_LOAD_Z"]), None, 0))
    elif u == "teaspoon":
        it.append(("SM_RT_TeaSpoon", (-J["TEASPOON_X"], 0, -J["TEASPOON_LOAD_Z"]), None, 0))
    elif u == "chopsticks":
        it.append(("SM_RT_Chopsticks", (-J["CHOP_TIP_X"], 0, -J["CHOP_TIP_Z"]), None, 0))
    it = [(n, (l[0], l[1], l[2] + 0.12), sc_, r) for n, l, sc_, r in it]      # in the air: chopstick loads hang down
    return it, ld


def compose(names, out, cols, S=360):
    import numpy as np
    rows = (len(names) + cols - 1) // cols
    W, H = cols * S, rows * S
    canvas = np.ones((H, W, 4), dtype=np.float32)
    for k, n in enumerate(names):
        if n is None:
            continue
        img = bpy.data.images.load(n)
        w, h = img.size
        a = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(a)
        a = a.reshape(h, w, 4)
        a = a[(np.arange(S) * h // S)][:, (np.arange(S) * w // S)]
        r, c = k // cols, k % cols
        y0 = H - (r + 1) * S
        canvas[y0:y0 + S, c * S:(c + 1) * S] = a
        canvas[y0:y0 + S, c * S, :3] = 0.6
        canvas[y0, c * S:(c + 1) * S, :3] = 0.6
        bpy.data.images.remove(img)
    im = bpy.data.images.new("sheet", W, H, alpha=False)
    im.pixels.foreach_set(canvas.ravel())
    im.filepath_raw = out
    im.file_format = "PNG"
    im.save()
    print("SHEET", out, flush=True)


ORDER = ["borscht", "pelmeni", "baltic_fish", "ramen", "pasta", "steak", "khachapuri", "olivier", "medovik", "plombir", "syrniki",
         "coffee", "mors", "water", "bread"]
EATEN = [d for d in ORDER if DI[d]["stages"]]
ORDER2 = ["solyanka", "golubtsy", "draniki", "fried_flounder", "shashlik", "sushi_set", "blini_caviar", "fest_ukha", "fest_kulebyaka",
          "fest_sbiten", "tea"]
STAGES2 = ["sushi_set", "shashlik", "fried_flounder"]
PROPS = ["SM_RT_Geta", "SM_RT_ClayBowl", "SM_RT_GlassMug", "SM_RT_TeaHolder", "SM_RT_FryPan", "SM_RT_StockPot", "SM_RT_Ladle",
         "SM_RT_ServiceBell", "SM_RT_PlateStack", "SM_RT_TicketRail", "SM_RT_Cloche"]
TABLE = ["SM_RT_Fork", "SM_RT_Knife", "SM_RT_Spoon", "SM_RT_TeaSpoon", "SM_RT_Chopsticks", "SM_RT_Napkin", "SM_RT_MenuCard",
         "SM_RT_SaltPepper", "SM_RT_Tray", "SM_RT_DessertPlate", "SM_RT_Board", "SM_RT_BreadBasket"]


def cell(name):
    return os.path.join(CELLS, name + ".png")


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["dishes", "stages", "close"]
    os.makedirs(CELLS, exist_ok=True)
    setup()
    if "dishes" in args:
        names = []
        for d in ORDER:
            render(cell("full_" + d), place(dish_items(d)), d)
            names.append(cell("full_" + d))
        names.append(None)
        for d in ("steak", "ramen", "khachapuri"):
            for k in range(5):
                render(cell("st_%s_%d" % (d, k)), place(dish_items(d, k)), "%s %d" % (d, k))
                names.append(cell("st_%s_%d" % (d, k)))
        names.append(None)
        for t in TABLE:
            items = [(t, (0, 0, 0), None, 0)]
            render(cell(t), place(items), t[6:], view=(0.55, -1.0, 1.1) if t in ("SM_RT_Fork", "SM_RT_Knife", "SM_RT_Spoon", "SM_RT_TeaSpoon", "SM_RT_Chopsticks") else (0.75, -1.0, 0.95))
            names.append(cell(t))
        for d in EATEN:
            for big in (False, True):
                items, ld = load_items(d, big)
                objs = place(items)
                render(cell(ld), objs, ld[11:], view=(0.55, -1.0, 0.55), zoom=1.15, focus=objs[:1])
                names.append(cell(ld))
        compose(names, os.path.join(PREV, "dishes_sheet.png"), 8)
    if "dishes2" in args:
        names = []
        for d in ORDER2:
            render(cell("full_" + d), place(dish_items(d)), d)
            names.append(cell("full_" + d))
        names.append(None)
        for d in STAGES2:
            for k in range(5):
                render(cell("st_%s_%d" % (d, k)), place(dish_items(d, k)), "%s %d" % (d, k))
                names.append(cell("st_%s_%d" % (d, k)))
        while len(names) % 8:
            names.append(None)
        # kitchen props
        pan = J["PAN_FOOD_Z"]
        items = {"SM_RT_FryPan": [("SM_RT_FryPan", (0, 0, 0), None, 0), ("SM_RT_PanFood", (0, 0, pan), None, 0)],
                 "SM_RT_StockPot": [("SM_RT_StockPot", (0, 0, 0), None, 0)],
                 "SM_RT_ServiceBell": [("SM_RT_ServiceBell", (0, 0, 0), None, 0), ("SM_RT_ServiceBell_Plunger", (0, 0, J["SERVICE_BELL_PLUNGER_Z"]), None, 0)],
                 "SM_RT_GlassMug": [("SM_RT_GlassMug", (0, 0, 0), None, 0), ("SM_RT_GlassMug_Glass", (0, 0, 0), None, 0)],
                 "SM_RT_TeaHolder": [("SM_RT_TeaHolder", (0, 0, 0), None, 0), ("SM_RT_TeaHolder_Glass", (0, 0, 0), None, 0)],
                 "SM_RT_Cloche": [("SM_RT_DinnerPlate", (0, 0, 0), None, 0), ("SM_RT_Cloche", (0, 0, 0.0145 + 0.02), None, 0)]}
        for t in PROPS:
            render(cell(t), place(items.get(t, [(t, (0, 0, 0), None, 0)])), t[6:], view=(0.7, -1.0, 0.8))
            names.append(cell(t))
        for d in ("solyanka", "golubtsy", "draniki", "fried_flounder", "shashlik", "sushi_set", "blini_caviar", "fest_ukha", "fest_kulebyaka"):
            for big in (False, True):
                its, ld = load_items(d, big)
                objs = place(its)
                render(cell(ld), objs, ld[11:], view=(0.55, -1.0, 0.55), zoom=1.15, focus=objs[:1])
                names.append(cell(ld))
        compose(names, os.path.join(PREV, "dishes_sheet2.png"), 8)
    if "close2" in args:
        for d in [a for a in args if a in DI]:
            objs = place(dish_items(d))
            food = [o for o in objs if o.name.startswith("SM_RF_") and "_Fill" not in o.name] or objs
            render(os.path.join(CELLS, "close2_%s.png" % d), objs, d, view=(0.5, -1.0, 0.75), res=800, zoom=0.95, focus=food)
        for t in [a for a in args if a.startswith("SM_RT_")]:
            render(os.path.join(CELLS, "close2_%s.png" % t), place([(t, (0, 0, 0), None, 0)]), t, view=(0.6, 1.0, 0.5) if "Rail" in t else (0.6, -1.0, 0.5), res=800)
    if "stages2" in args:
        names = []
        for d in [x for x in ORDER2 if DI[x]["stages"]]:
            for k in range(5):
                render(cell("st_%s_%d" % (d, k)), place(dish_items(d, k)), "%s %d" % (d, k), res=300)
                names.append(cell("st_%s_%d" % (d, k)))
        compose(names, os.path.join(PREV, "stages_all2.png"), 5, S=300)
    if "stages" in args:
        names = []
        for d in EATEN:
            for k in range(5):
                render(cell("st_%s_%d" % (d, k)), place(dish_items(d, k)), "%s %d" % (d, k))
                names.append(cell("st_%s_%d" % (d, k)))
        compose(names, os.path.join(PREV, "stages_all.png"), 5)
    if "close" in args:
        for d in EATEN + ["coffee", "mors", "water"]:
            objs = place(dish_items(d))
            food = [o for o in objs if o.name.startswith("SM_RF_") and "_Fill" not in o.name] or objs
            render(os.path.join(PREV, "close_%s.png" % d), objs, d, view=(0.5, -1.0, 0.75), res=900, zoom=0.95, focus=food)


main()
