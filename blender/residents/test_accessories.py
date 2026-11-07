"""Builds test residents with accessories, prints geometry checks and renders contact sheets.

    python3 bpyrun.py blender/residents/test_accessories.py -- [--engine eevee|wb] [--only people,lineup,heads,posed]
    (or: Blender -b -P blender/residents/test_accessories.py -- ...)

Output: renders/residents/accessories_{people_front,people_back,people_34,heads,lineup,posed_front,posed_34}.png
        and accessories_tiles.png (the tile textures).
Checks printed per accessory: triangles, UV range, material/tile props, how many vertices sit inside the body
(rest pose) and, for the posed run, the largest edge stretch (a tear detector for blended items).
"""
import math
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "humans"))
OUT = os.path.join(ROOT, "renders", "residents")

import mpfb_pipeline as P  # noqa: E402
import accessories as A  # noqa: E402

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default=None):
    return ARGS[ARGS.index(name) + 1] if name in ARGS else default


ENGINE = arg("--engine", "eevee")
ONLY = (arg("--only", "people,lineup,heads,posed")).split(",")
SCALE = float(arg("--scale", "1.0"))

# MakeHuman age: 0 = 1 yr, 0.1875 = 11, 0.5 = 25, 1.0 = 90
PEOPLE = [
    dict(name="babushka", gender=0.0, age=0.86, weight=0.6, height=0.4, skin="old_caucasian_female", hair="short01", hv=0.008,
         clothes=["female_casualsuit02", "shoes01"], acc=[("headscarf", {}), ("string_bag", {}), ("cane", {"hand": "l"})], sex="f", age_y=70, x=-1.65),
    dict(name="fisher", gender=1.0, age=0.66, weight=0.6, height=0.55, skin="middleage_caucasian_male", hair="short02", hv=0.012,
         clothes=["male_casualsuit03", "shoes03"],
         acc=[("flat_cap", {"seed": 1}), ("full_beard", {"seed": 2}), ("glasses", {"style": "rect"}), ("shoulder_bag", {})],
         sex="m", age_y=45, x=-0.55),
    dict(name="pupil", gender=1.0, age=0.17, weight=0.45, height=0.5, skin="young_caucasian_male", hair="short04", hv=0.01,
         clothes=["male_casualsuit06", "shoes01"], acc=[("baseball_cap", {"seed": 0}), ("backpack", {"seed": 1})], sex="m", age_y=10, x=0.55),
    dict(name="cook", gender=1.0, age=0.54, weight=0.68, height=0.5, skin="young_asian_male", hair="short03", hv=0.01,
         clothes=["male_worksuit01", "shoes04"], acc=[("chef_toque", {}), ("bib_apron", {}), ("mustache", {"seed": 1})], sex="m", age_y=30, x=1.65),
]
LINEUP = [
    dict(name="winter_guy", gender=1.0, age=0.58, weight=0.5, height=0.5, skin="young_caucasian_male2", hair="short01", hv=0.01,
         clothes=["male_casualsuit05", "shoes02"],
         acc=[("knit_beanie", {"pompom": True, "seed": 0}), ("scarf", {"seed": 1}), ("short_beard", {"seed": 0}), ("glasses", {"style": "round"})],
         sex="m", age_y=28, x=-2.2),
    dict(name="summer_lady", gender=0.0, age=0.7, weight=0.5, height=0.5, skin="middleage_african_female", hair="afro01", hv=0.06,
         clothes=["female_elegantsuit01", "shoes05"],
         acc=[("sun_hat", {"seed": 1}), ("shopping_bag", {"seed": 1}), ("glasses", {"style": "sun"})], sex="f", age_y=40, x=-0.75),
    dict(name="waiter", gender=1.0, age=0.52, weight=0.4, height=0.6, skin="young_caucasian_male", hair="short01", hv=0.01,
         clothes=["male_casualsuit01", "shoes06"], acc=[("waist_apron", {}), ("scarf", {"seed": 2}), ("mustache", {"seed": 0})], sex="m", age_y=26, x=0.75),
    dict(name="seller", gender=0.0, age=0.6, weight=0.7, height=0.35, skin="middleage_asian_female", hair="ponytail01", hv=0.012,
         clothes=["female_casualsuit01", "shoes01"],
         acc=[("headscarf", {"seed": 2}), ("bib_apron", {"seed": 1}), ("shopping_bag", {"seed": 2})], sex="f", age_y=38, x=2.2),
]


def build_person(spec):
    s = P.services()
    HS = s["HumanService"]
    macro = dict(s["TargetService"].get_default_macro_info_dict())
    macro.update(dict(gender=spec["gender"], age=spec["age"], muscle=0.5, weight=spec["weight"], height=spec["height"], proportions=0.5))
    macro["race"] = dict(caucasian=0.6, asian=0.2, african=0.2)
    bm = HS.create_human(macro_detail_dict=macro)
    bm.name = spec["name"]
    arm = P.add_ue_rig(bm)
    HS.set_character_skin(P.find_skin(spec["skin"]), bm, skin_type="GAMEENGINE")
    for folder, sub, at in [("high-poly", "eyes", "Eyes"), ("eyebrow004", "eyebrows", "Eyebrows"), ("eyelashes01", "eyelashes", "Eyelashes"),
                            ("teeth_base", "teeth", "Teeth"), ("tongue01", "tongue", "Tongue"), (spec["hair"], "hair", "Hair")] + \
                           [(c, "clothes", "Clothes") for c in spec["clothes"]]:
        try:
            P.add_asset(bm, folder, sub, at)
        except Exception as e:  # noqa: BLE001
            print("asset failed", folder, e)
    ctx = dict(basemesh=bm, armature=arm, hair_volume=spec["hv"], sex=spec["sex"], age=spec["age_y"])
    objs = []
    for kind, par in spec["acc"]:
        t = time.time()
        o = A.add(kind, ctx, dict(par))
        objs.append(o)
        print("  built %-13s %4d tris %.2fs" % (kind, o["tris"], time.time() - t))
    spec["bm"], spec["arm"], spec["ctx"], spec["objs"] = bm, arm, ctx, objs
    return spec


def check(spec):
    """Prints geometry checks for the person's accessories."""
    fit = A.Fit(spec["ctx"])
    for o in spec["objs"]:
        me = o.data
        n = len(me.vertices)
        co = np.empty(n * 3)
        me.vertices.foreach_get("co", co)
        co = co.reshape(n, 3)
        uv = np.empty(len(me.uv_layers[0].data) * 2)
        me.uv_layers[0].data.foreach_get("uv", uv)
        uv = uv.reshape(-1, 2)
        inside = 0
        worst = 0.0
        for p in co:
            r = fit.nearest(p, 0.2)
            if r is None:
                continue
            loc, nrm, d = r
            sd = d if np.dot(p - loc, nrm) >= 0 else -d
            if sd < -0.0015:
                inside += 1
            worst = min(worst, sd)
        # texel density spread (uv area / world area per triangle): large spread = stretched UVs
        ratios = []
        loops = me.uv_layers[0].data
        for poly in me.polygons:
            vs = list(poly.vertices)
            ls = list(poly.loop_indices)
            for k in range(1, len(vs) - 1):
                a3 = 0.5 * np.linalg.norm(np.cross(co[vs[k]] - co[vs[0]], co[vs[k + 1]] - co[vs[0]]))
                u0, u1, u2 = (np.array(loops[ls[i]].uv) for i in (0, k, k + 1))
                a2 = 0.5 * abs((u1[0] - u0[0]) * (u2[1] - u0[1]) - (u2[0] - u0[0]) * (u1[1] - u0[1]))
                if a3 > 2e-6 and a2 > 0:
                    ratios.append(a2 / a3)
        ratios = np.array(ratios) if ratios else np.ones(1)
        spread = np.percentile(ratios, 95) / max(np.percentile(ratios, 5), 1e-9)
        mats = [m.material.name for m in o.material_slots]
        ok = (len(mats) == 1 and mats[0].split(".")[0] in ("Cloth", "Hair") and uv.min() >= 0 and uv.max() <= 1
              and o["tris"] <= (800 if o["kind"] == "glasses" else 1500))
        print("%-9s %-13s tris=%4d uv=[%.2f..%.2f] mat=%s tile=%s tint_slot=%s tintable=%s inside_body=%d/%d worst=%.1fmm uv_density_p95/p5=%.1f %s" % (
            spec["name"], o["kind"], o["tris"], uv.min(), uv.max(), mats, o["tile"], o["tint_slot"], o["tintable"], inside, n,
            worst * 1000, spread, "OK" if ok else "CHECK"))


# ------------------------------------------------------------------------------ rendering
def setup_render():
    sc = bpy.context.scene
    if ENGINE == "wb":
        sc.render.engine = "BLENDER_WORKBENCH"
        sh = sc.display.shading
        sh.light, sh.color_type = "STUDIO", "TEXTURE"
        sc.display.render_aa = "8"
        sc.view_settings.view_transform = "Standard"
        w = bpy.data.worlds.new("w")
        w.color = (0.35, 0.36, 0.38)
        sc.world = w
        if "cam" not in bpy.data.objects:
            cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
            sc.collection.objects.link(cam)
            sc.camera = cam
    else:
        import preview
        preview.setup_stage()
        sc.eevee.taa_render_samples = 24
    return sc


def shot(path, eye, target, lens, res, hide=()):
    import preview
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = int(res[0] * SCALE), int(res[1] * SCALE)
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = False
    for spec in hide:
        for o in [spec["bm"]] + list(spec["bm"].children_recursive) + spec["objs"] + list(spec["arm"].children_recursive):
            o.hide_render = True
    cam = bpy.data.objects["cam"]
    cam.data.lens = lens
    cam.data.clip_end = 100
    cam.location = eye
    preview._look_at(cam, target)
    sc.render.filepath = path
    t = time.time()
    bpy.ops.render.render(write_still=True)
    print("render %s %.1fs" % (os.path.basename(path), time.time() - t))


def sheet(paths, out, cols, bg=(46, 46, 48)):
    from PIL import Image
    ims = [Image.open(p).convert("RGB") for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    S = Image.new("RGB", (w * cols, h * rows), bg)
    for i, im in enumerate(ims):
        S.paste(im, ((i % cols) * w, (i // cols) * h))
    S.save(out)
    print("wrote", out)


def place(specs):
    for sp in specs:
        sp["arm"].location.x = sp["x"]


def head_pos(spec):
    return spec["bm"].matrix_world.translation, spec["arm"].location.x, A_FIT[spec["name"]].c


A_FIT = {}


# ------------------------------------------------------------------------------ pose
def rot_world(arm, bone, axis, deg):
    pb = arm.pose.bones[bone]
    M = pb.bone.matrix_local.to_3x3()
    q = (M.inverted() @ Matrix.Rotation(math.radians(deg), 3, axis) @ M).to_quaternion()
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = q


def pose_walk(arm):
    rot_world(arm, "thigh_l", "X", -26)
    rot_world(arm, "calf_l", "X", 12)
    rot_world(arm, "thigh_r", "X", 22)
    rot_world(arm, "upperarm_l", "Y", 34)
    rot_world(arm, "upperarm_r", "Y", -34)
    rot_world(arm, "upperarm_l", "X", 18)
    rot_world(arm, "upperarm_r", "X", -22)
    rot_world(arm, "spine_02", "Z", 9)
    rot_world(arm, "spine_01", "X", -6)
    rot_world(arm, "head", "Z", -16)
    rot_world(arm, "neck_01", "X", 8)
    rot_world(arm, "jaw", "X", -10)
    bpy.context.view_layer.update()


def clear_pose(arm):
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
    bpy.context.view_layer.update()


def stretch_report(specs):
    """Largest edge-length ratio (posed / rest) per accessory: ~1.0 = rigid, big = tearing."""
    dg = bpy.context.evaluated_depsgraph_get()
    for sp in specs:
        for o in sp["objs"]:
            me = o.data
            e = np.array([v.vertices[:] for v in me.edges])
            n = len(me.vertices)
            rest = np.empty(n * 3)
            me.vertices.foreach_get("co", rest)
            rest = rest.reshape(-1, 3)
            ev = o.evaluated_get(dg)
            m2 = ev.to_mesh()
            pos = np.empty(len(m2.vertices) * 3)
            m2.vertices.foreach_get("co", pos)
            pos = pos.reshape(-1, 3)
            ev.to_mesh_clear()
            l0 = np.linalg.norm(rest[e[:, 0]] - rest[e[:, 1]], axis=1)
            l1 = np.linalg.norm(pos[e[:, 0]] - pos[e[:, 1]], axis=1)
            ok = l0 > 1e-4
            r = (l1[ok] / l0[ok])
            print("stretch %-9s %-13s max=%.2f min=%.2f  moved=%.1fcm" % (sp["name"], o["kind"], r.max(), r.min(),
                                                                         np.abs(pos - rest).max() * 100))


# ------------------------------------------------------------------------------ main
def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    t0 = time.time()
    specs = []
    for sp in PEOPLE:
        print("person", sp["name"])
        specs.append(build_person(sp))
    line = []
    if "lineup" in ONLY or "posed" in ONLY:
        for sp in LINEUP:
            print("person", sp["name"])
            line.append(build_person(sp))
    print("built in %.1fs" % (time.time() - t0))
    for sp in specs + line:
        check(sp)
        A_FIT[sp["name"]] = A.Fit(sp["ctx"])
    # tiles contact sheet
    tiles = A.make_tiles(os.path.join(OUT, "tiles"))
    from PIL import Image
    tl = []
    for name, path in tiles.items():
        im = Image.open(path).convert("RGBA")
        bg = Image.new("RGBA", im.size, (200, 60, 200, 255))
        bg.alpha_composite(im)
        tl.append(bg.convert("RGB").resize((256, 256)))
    S = Image.new("RGB", (256 * 6, 256 * 2), (40, 40, 40))
    for i, im in enumerate(tl):
        S.paste(im, ((i % 6) * 256, (i // 6) * 256))
    S.save(os.path.join(OUT, "accessories_tiles.png"))
    place(specs + line)
    setup_render()
    tmp = os.path.join(OUT, "_tmp")
    os.makedirs(tmp, exist_ok=True)
    sizes = (1700, 800)
    if "people" in ONLY:
        shot(os.path.join(OUT, "accessories_people_front.png"), (0.0, -9.0, 1.0), (0, 0, 0.88), 60, sizes, hide=line)
        shot(os.path.join(OUT, "accessories_people_back.png"), (0.0, 9.0, 1.0), (0, 0, 0.88), 60, sizes, hide=line)
        shot(os.path.join(OUT, "accessories_people_34.png"), (6.4, -6.9, 1.15), (0, 0, 0.88), 60, sizes, hide=line)
    if "heads" in ONLY:
        paths = []
        for sp in specs:
            f = A_FIT[sp["name"]]
            hx = sp["x"]
            tz = float(f.c[2]) + 0.01
            for vi, (dx, dy) in enumerate([(0.55, -0.95), (-0.9, -0.5)]):
                p = os.path.join(tmp, "head_%s_%d.png" % (sp["name"], vi))
                shot(p, (hx + dx, f.c[1] + dy, tz + 0.06), (hx, f.c[1], tz), 85, (420, 420), hide=[s for s in specs if s is not sp] + line)
                paths.append(p)
        sheet(paths, os.path.join(OUT, "accessories_heads.png"), 4)
    if "lineup" in ONLY:
        shot(os.path.join(OUT, "accessories_lineup.png"), (0.0, -10.5, 1.0), (0, 0, 0.88), 60, (1700, 800), hide=specs)
        shot(os.path.join(OUT, "accessories_lineup_34.png"), (7.4, -7.4, 1.15), (0, 0, 0.88), 60, (1700, 800), hide=specs)
        paths = []
        for sp in line:
            f = A_FIT[sp["name"]]
            tz = float(f.c[2]) + 0.01
            p = os.path.join(tmp, "lhead_%s.png" % sp["name"])
            shot(p, (sp["x"] + 0.55, f.c[1] - 0.95, tz + 0.06), (sp["x"], f.c[1], tz), 85, (420, 420), hide=specs + [s for s in line if s is not sp])
            paths.append(p)
        sheet(paths, os.path.join(OUT, "accessories_lineup_heads.png"), 4)
    if "posed" in ONLY:
        for sp in specs + line:
            pose_walk(sp["arm"])
        stretch_report(specs + line)
        shot(os.path.join(OUT, "accessories_posed_front.png"), (0.0, -9.0, 1.0), (0, 0, 0.88), 60, sizes, hide=line)
        shot(os.path.join(OUT, "accessories_posed_34.png"), (6.4, -6.9, 1.15), (0, 0, 0.88), 60, sizes, hide=line)
        shot(os.path.join(OUT, "accessories_posed_lineup.png"), (7.4, -7.4, 1.15), (0, 0, 0.88), 60, (1700, 800), hide=specs)
    print("done in %.1fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
