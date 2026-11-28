"""The review renders (build_all.py --previews DIR [--shots a,b]): each shot clears the scene, builds it from the registry
objects and renders a PNG."""
import math
import os

from mathutils import Matrix, Vector

import boatspec as B
import preview as P


def _boat_shot(reg, out, b, name, cam, target, heel=-8, sheet=22, lens=40, variant="new"):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=True, sand=False)
    P.rigged(reg, b, Matrix.Identity(4), sheet_deg=sheet, heel=heel, variant=variant)
    P.camera(cam, target, lens)
    P.render(os.path.join(out, name))


def shot_luch(reg, out):
    _boat_shot(reg, out, "luch", "luch_a.png", (6.5, 7.5, 3.0), (-0.3, 0, 2.2), heel=-6, sheet=20, lens=32)
    _boat_shot(reg, out, "luch", "luch_b.png", (-6.0, -4.0, 2.6), (0.0, 0, 1.2), heel=-6, sheet=20, lens=32)


def shot_optimist(reg, out):
    _boat_shot(reg, out, "optimist", "optimist_a.png", (4.2, 4.6, 1.8), (-0.3, 0, 1.0), heel=-5, sheet=25, lens=35, variant="old")
    _boat_shot(reg, out, "optimist", "optimist_b.png", (-3.6, -2.6, 2.4), (0.0, 0, 0.6), heel=-5, sheet=25, lens=35, variant="old")


def shot_cadet(reg, out):
    _boat_shot(reg, out, "cadet", "cadet_a.png", (5.5, 6.0, 2.4), (-0.2, 0, 1.8), heel=-6, sheet=20, lens=32)
    _boat_shot(reg, out, "cadet", "cadet_b.png", (-5.0, -3.4, 2.6), (0.0, 0, 1.0), heel=-6, sheet=20, lens=32)


def shot_lineup(reg, out):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=False, sand=True, sand_z=-0.30)
    # on the sand, side by side (keels at the sand), sails up, a 1.75 m dummy
    for b, x, var in (("optimist", -6.2, "patched"), ("cadet", -1.6, "old"), ("luch", 4.4, "new")):
        kz = min(v for _, v in B.BOATS[b]["keel_z"])
        P.rigged(reg, b, Matrix.Translation((x, 0, -0.30 - kz)), sheet_deg=0, heel=0, variant=var)
    P.dummy(Matrix.Translation((-3.9, 0.9, -0.30)))
    P.camera((-0.5, 17.0, 2.2), (-0.5, 0.0, 2.2), 38)
    P.render(os.path.join(out, "lineup.png"))


def _put(reg, name, M):
    src = reg.objects.get(name)
    if src is None:
        print("SAILING preview: no", name)
        return None
    return P.instance(src, "P_" + name, M)


def shot_launch(reg, out):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=True, sand=False)
    _put(reg, "SM_Sail_Launch", Matrix.Identity(4))
    _put(reg, "SM_Sail_Launch_Motor", Matrix.Translation((-2.32, 0, 0.43)) @ Matrix.Rotation(math.radians(8), 4, "Z"))
    _put(reg, "SM_Sail_Mark", Matrix.Translation((-3.0, 4.5, 0)))
    _put(reg, "SM_Sail_Pin", Matrix.Translation((-1.0, 6.0, 0)))
    _put(reg, "SM_Sail_Buoy", Matrix.Translation((1.5, 4.0, 0)))
    _put(reg, "SM_Sail_MOB", Matrix.Translation((3.0, 3.5, 0)))
    P.dummy(Matrix.Translation((-0.70, -0.25, 0.20)) @ Matrix.Rotation(math.radians(0), 4, "Z"), pose="sit")
    P.camera((5.2, 6.4, 2.6), (-0.6, 1.2, 0.4), 32)
    P.render(os.path.join(out, "launch.png"))


def _yawM(pos, yaw=0.0, rot=None):
    M = Matrix.Translation(pos) @ Matrix.Rotation(math.radians(yaw), 4, "Z")
    if rot:
        from mathutils import Euler
        M = M @ Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    return M


TINT_CYCLE = ["#F2C230", "#E8E4D8", "#8BB8D8", "#D8483A", "#F08A2A", "#7FC0A0"]


def place_house(reg, winter=False, outside=True, rigged_park=True):
    import json as _j
    import boathouse as BH
    lay = _j.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "boathouse_layout.json")))
    rows = {r["asset"]: r for r in reg.rows}
    _put(reg, "SM_Sail_Boathouse", Matrix.Identity(4))
    gl = _put(reg, "SM_Sail_Boathouse_Glass", Matrix.Identity(4))
    if gl:
        for i, mt in enumerate(gl.data.materials):
            nm = mt.copy()
            gl.data.materials[i] = nm
            b = nm.node_tree.nodes.get("Principled BSDF")
            if b:
                b.inputs["Transmission Weight"].default_value = 1.0
                b.inputs["Roughness"].default_value = 0.02
    for sd in lay["sliding_doors"]:
        for lf in sd["leaves"]:
            p = Vector(lf["closed"])
            if not winter:
                p.x += lf["open_dx"]
            _put(reg, sd["mesh"], Matrix.Translation(p))
    for e in lay["props"]:
        if winter and e["mesh"] == "SM_Sail_Prop_HullFrame":
            pass
        _put(reg, e["mesh"], _yawM(e["pos"], e.get("yaw", 0), e.get("rot_deg")))
    for k, bt in enumerate(lay["boats"]):
        pos = list(bt["pos"])
        if pos[2] is None:
            tro = bt["on_trolley"]
            pos[2] = rows[tro]["boat_origin"][2]
        src = reg.objects.get(bt["mesh"])
        if src is None:
            continue
        M = _yawM(pos, 0, bt.get("rot_deg"))
        P.instance(src, "P_boat%d" % k, M, tint=TINT_CYCLE[k % len(TINT_CYCLE)])
        if winter and bt.get("winter_cover"):
            _put(reg, bt["winter_cover"], M)
    if not winter:
        for e in lay["spars"]:
            _put(reg, e["mesh"], _yawM(e["pos"], 0, e.get("rot_deg")))
        for e in lay["hanging_sails"]:
            src = reg.objects.get(e["mesh"])
            if src:
                P.instance(src, "P_hang_" + e["mesh"], _yawM(e["pos"], 0, e["rot_deg"]), sail_tex="sail_cadet_jib_old.png" if "Jib" in e["mesh"] else "sail_cadet_old_demo.png")
    if not outside:
        return lay
    for e in lay["outside"]:
        if e["mesh"] == "SM_Sail_Prop_SlipBoard":
            continue
        M = _yawM(e["pos"], e.get("yaw", 0), e.get("rot_deg"))
        _put(reg, e["mesh"], M)
        if e.get("kind") == "trolley":
            b = e["boat"]
            z = rows[e["mesh"]]["boat_origin"][2]
            Mb = M @ Matrix.Translation((0, 0, z))
            if winter:
                cn = {"luch": "Luch", "optimist": "Optimist", "cadet": "Cadet"}[b]
                P.instance(reg.objects["SM_Sail_%s_Hull" % cn], "P_park_" + str(len(bpy_objs())), Mb, tint=TINT_CYCLE[int(e["pos"][0]) % 6])
                _put(reg, "SM_Sail_Cover_" + cn, Mb)
            else:
                P.rigged(reg, b, Mb, sheet_deg=0, tint=TINT_CYCLE[int(abs(e["pos"][0]) * 7) % 6], prefix="Park%d" % len(bpy_objs()),
                         board_down=False)
    # slip: 19 sections from the gate down the sand
    slip0 = [e for e in lay["outside"] if e["mesh"] == "SM_Sail_Prop_SlipBoard"][0]
    for k in range(19):
        x = slip0["pos"][0] + k * 1.2
        _put(reg, "SM_Sail_Prop_SlipBoard", Matrix.Translation((x, slip0["pos"][1], BH.ground(x))) @ Matrix.Rotation(math.atan(0.0727), 4, "Y"))
    fp = lay["flag_points"]
    for key, fid in (("yard_port", "sail_on" if not winter else "closed"), ("yard_stbd", "class_luch"), ("top", "shore" if winter else "class_optimist"), ("staff_top", "class_luch")):
        if not winter or key != "yard_stbd":
            _put(reg, "SM_Sail_Flag_" + fid, Matrix.Translation(fp[key]) @ Matrix.Rotation(math.radians(20), 4, "Z"))
    ws = [e for e in lay["outside"] if e["mesh"] == "SM_Sail_Prop_WindsockPole"][0]
    _put(reg, "SM_Sail_Windsock", Matrix.Translation((ws["pos"][0], ws["pos"][1], ws["pos"][2] + 2.0)) @ Matrix.Rotation(math.radians(160), 4, "Z"))
    return lay


def bpy_objs():
    import bpy
    return bpy.data.objects


def _sand():
    import bpy
    bpy.ops.mesh.primitive_plane_add(size=220, location=(-0.85, 0, -0.005))
    p = bpy.context.active_object
    p.name = "SandSlope"
    p.rotation_euler = (0, math.atan(0.0727), 0)
    mt = bpy.data.materials.new("PrevSand2")
    mt.use_nodes = True
    b = mt.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.30, 0.25, 0.17, 1)
    b.inputs["Roughness"].default_value = 0.95
    p.data.materials.append(mt)


def _interior_lights(lay, power=60):
    import bpy
    for k, l in enumerate(lay["lamps"]):
        ld = bpy.data.lights.new("L%d" % k, "POINT")
        ld.energy = power
        ld.color = (1.0, 0.86, 0.68)
        ld.shadow_soft_size = 0.15
        o = bpy.data.objects.new("L%d" % k, ld)
        o.location = l["pos"]
        bpy.context.scene.collection.objects.link(o)


def shot_house(reg, out):
    import bpy
    P.clear_scene_keep(reg)
    P.scene_setup(sea=False, sand=False, sun_elev=34, sun_az=200)
    _sand()
    bpy.ops.mesh.primitive_plane_add(size=300, location=(170, 0, -1.55))
    sea = bpy.context.active_object
    mt = bpy.data.materials.new("PrevSeaB")
    mt.use_nodes = True
    mt.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.02, 0.07, 0.09, 1)
    mt.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.1
    sea.data.materials.append(mt)
    place_house(reg)
    P.camera((27.0, 13.0, 2.4), (4.0, 0.0, 3.0), 30)
    P.render(os.path.join(out, "boathouse_beach.png"), samples=32)
    P.camera((14.0, -30.0, 9.0), (2.0, -9.0, 1.0), 30)
    P.render(os.path.join(out, "boat_park.png"), samples=32)


def shot_inside(reg, out, winter=False):
    import bpy
    P.clear_scene_keep(reg)
    P.scene_setup(sea=False, sand=False, sun_elev=40, sun_az=200, strength=3.0)
    _sand()
    lay = place_house(reg, winter=winter, outside=False)
    _interior_lights(lay, 90)
    bpy.context.scene.world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    P.camera((4.9, 3.4, 1.75), (-2.0, -2.2, 1.2), 22)
    P.render(os.path.join(out, "hall_winter.png" if winter else "hall.png"), samples=48)
    if not winter:
        P.camera((-3.2, 1.3, 1.75), (-8.2, -1.1, 1.3), 17)
        P.render(os.path.join(out, "classroom.png"), samples=48)


def shot_park_winter(reg, out):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=False, sand=False, sun_elev=14, sun_az=200, strength=2.2)
    _sand()
    place_house(reg, winter=True)
    P.camera((14.0, -30.0, 9.0), (2.0, -9.0, 1.0), 30)
    P.render(os.path.join(out, "winter_laid_up.png"), samples=32)


SHOTS = {"launch": shot_launch, "house": shot_house, "inside": shot_inside,
         "winter": lambda reg, out: (shot_inside(reg, out, True), shot_park_winter(reg, out)),"luch": shot_luch, "optimist": shot_optimist, "cadet": shot_cadet, "lineup": shot_lineup}


def run(reg, out, which=None):
    os.makedirs(out, exist_ok=True)
    try:
        import gear_shots
        SHOTS.update(gear_shots.SHOTS)
    except ImportError:
        pass
    P.prepare_materials()
    for k, fn in SHOTS.items():
        if which and k not in which:
            continue
        fn(reg, out)
