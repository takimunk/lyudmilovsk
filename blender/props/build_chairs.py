"""Detailed chairs: office, wooden stool, ergonomic mesh chair, racing gaming chair, bentwood, folding metal chair.

  blender -b -P blender/props/build_chairs.py -- [name ...] [--no-render]

Frame: origin on the floor under the post / seat centre, the sitter faces +Y, the backrest is at -Y, seat top at ~0.47 m.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
from mathutils import Vector

import propslib as P
from build_computers import finish, hull, RADS


def fabric_mat():
    return P.tex("Chair_Fabric", "fabric_black.png", 0.9, size=1024, tint=(0.8, 0.8, 0.82), tilemult=1.0,
                 fallback=(0.06, 0.06, 0.07))


def plastic():
    return P.mat("Chair_Plastic", (0.025, 0.025, 0.03), 0.5)


def chrome():
    return P.mat("Chair_Chrome", (0.75, 0.75, 0.78), 0.18, 1.0)


def castor(n, root, x, y, z0, metal, rub):
    """Twin-wheel castor with a stem and a fork; (x, y) is the centre, z0 the floor."""
    P.cylinder(n + "_Stem", 0.005, 0.03, (x, y, z0 + 0.05), metal, root, seg=8)
    P.rbox(n + "_Fork", (0.03, 0.02, 0.012), (x, y, z0 + 0.038), plastic(), root, r=0.003)
    for sx in (-1, 1):
        P.rbox(n + "_ForkArm", (0.004, 0.02, 0.028), (x + sx * 0.011, y, z0 + 0.026), plastic(), root, r=0.001)
    P.cylinder(n + "_Wheel", 0.0235, 0.012, (x, y, z0 + 0.0235), rub, root, rot=(0, RADS(90), 0), seg=16, bevel=0.002)


def star_base(n, root, metal, rub, post_h, legs=5, leg_len=0.30, z0=0.0, gas_r=0.027, radius_hub=0.05):
    """Five-star base with gas lift column, cover and castors."""
    for i in range(legs):
        a = i * 2 * math.pi / legs
        ca, sa = math.sin(a), math.cos(a)
        pts = [Vector((ca * 0.0, sa * 0.0, 0.07 + z0)),]
        # tapered leg: wide at hub, narrow at the castor, slight rise
        w0, w1 = 0.05, 0.026
        z_hub, z_end = 0.095, 0.065
        for sgn in (-1, 1):
            pass
        px, py = -sa, ca
        verts = []
        for sx, hh, t in ((-1, 0, 0.0), (1, 0, 0.0)):
            pass
        base = [(ca * 0.03 + px * w0 / 2, sa * 0.03 - py * w0 / 2) for _ in (0,)]
        h_pts = []
        for t, ww, zb, zt in ((0.04, w0, 0.065, 0.1), (leg_len, w1, 0.052, 0.078)):
            for s in (-1, 1):
                for zz in (zb, zt):
                    h_pts.append((ca * t - sa * s * ww / 2 * 0 + (px) * s * ww / 2, sa * t + (-py) * s * ww / 2 * 0 + (py) * s * ww / 2 * 0, zz))
        # rotate the leg block: points along direction (ca, sa) and perpendicular (-sa, ca)
        pts = []
        for t, ww, zb, zt in ((0.03, w0, 0.065, 0.105), (leg_len, w1, 0.052, 0.078)):
            for s in (-1, 1):
                for zz in (zb, zt):
                    pts.append((ca * t - sa * s * ww / 2, sa * t + ca * s * ww / 2, zz + z0))
        # direction vector uses (ca, sa) as (sin a, cos a): the leg angle
        hull(n + "_Leg", pts, metal, root, r=0.003, seg=2)
        castor(n, root, ca * (leg_len + 0.008), sa * (leg_len + 0.008), z0, metal, rub)
    P.cylinder(n + "_Hub", radius_hub, 0.04, (0, 0, 0.085 + z0), metal, root, seg=24, r2=0.04)
    P.cylinder(n + "_Gas", gas_r, post_h, (0, 0, 0.105 + z0 + post_h / 2), chrome(), root, seg=20)
    P.lathe(n + "_Cover", [(0.045, 0.0), (0.045, 0.01), (0.04, 0.2 * post_h), (0.034, 0.3 * post_h)], (0, 0, 0.105 + z0), plastic(), root, seg=24)


def seat_cushion(n, root, w, d, t, z, y, mat_, r=0.04, tilt=0.0):
    c = P.rbox(n + "_Seat", (w, d, t), (0, y, z), mat_, root, r=r, seg=5, rot=(RADS(tilt), 0, 0), tile=0.5)
    return c


def build_office(n):
    root = P.empty(n)
    fab, pl, ch = fabric_mat(), plastic(), chrome()
    rub = P.mat("Chair_Rubber", (0.015, 0.015, 0.015), 0.85)
    star_base(n, root, pl, rub, 0.28)
    # tilt mechanism under the seat
    P.rbox(n + "_Mech", (0.3, 0.3, 0.05), (0, 0, 0.405), pl, root, r=0.012)
    P.rbox(n + "_PanSeat", (0.43, 0.43, 0.012), (0, 0.0, 0.438), pl, root, r=0.01)
    seat_cushion(n, root, 0.5, 0.48, 0.07, 0.48, 0.015, fab, r=0.03, tilt=1.0)
    # waterfall front
    P.blob(n + "_Waterfall", (0.25, 0.04, 0.035), (0, 0.248, 0.465), fab, root, seg=24, rings=10)
    # back: spine bracket + upholstered back pad
    P.tube(n + "_Spine", [(0, -0.12, 0.43), (0, -0.2, 0.44), (0, -0.26, 0.58), (0, -0.27, 0.66)], 0.016, pl, root, seg=12)
    P.rbox(n + "_BackCore", (0.43, 0.04, 0.46), (0, -0.265, 0.77), pl, root, rot=(RADS(-6), 0, 0), r=0.015)
    P.rbox(n + "_Back", (0.47, 0.075, 0.5), (0, -0.255, 0.77), fab, root, rot=(RADS(-6), 0, 0), r=0.032, seg=5)
    # arm rests: C-shaped arms with pads
    for sx in (-1, 1):
        P.tube(n + "_Arm", [(sx * 0.255, 0.0, 0.46), (sx * 0.275, 0.0, 0.52), (sx * 0.275, 0.0, 0.65), (sx * 0.265, 0.02, 0.69)], 0.012, pl, root, seg=10)
        P.rbox(n + "_ArmPad", (0.055, 0.28, 0.028), (sx * 0.265, 0.03, 0.7), pl, root, r=0.01)
    # levers
    P.tube(n + "_Lever", [(0.15, 0.0, 0.405), (0.2, 0.05, 0.4)], 0.004, ch, root, seg=6)
    finish(root, n)
    return root, (0.6, 0.62, 1.05)


def build_aeron(n):
    root = P.empty(n)
    frame = P.mat("Aeron_Frame", (0.06, 0.06, 0.065), 0.35, 0.3)
    mesh = P.tex("Aeron_Mesh", "mesh_black.png", 0.8, size=1024, tint=(0.7, 0.7, 0.72), tilemult=0.3, fallback=(0.03, 0.03, 0.035))
    alu = P.mat("Aeron_Alu", (0.7, 0.7, 0.72), 0.25, 1.0)
    rub = P.mat("Chair_Rubber", (0.015, 0.015, 0.015), 0.85)
    pl = plastic()
    star_base(n, root, alu, rub, 0.3, leg_len=0.32)
    P.rbox(n + "_Mech", (0.32, 0.34, 0.07), (0, -0.02, 0.4), frame, root, r=0.02)
    # seat: frame rim with mesh and a curved front lip
    P.rbox(n + "_SeatRimL", (0.04, 0.5, 0.03), (-0.235, 0.0, 0.46), frame, root, r=0.012)
    P.rbox(n + "_SeatRimR", (0.04, 0.5, 0.03), (0.235, 0.0, 0.46), frame, root, r=0.012)
    P.rbox(n + "_SeatRimF", (0.51, 0.04, 0.03), (0, 0.245, 0.46), frame, root, r=0.012)
    P.rbox(n + "_SeatRimB", (0.51, 0.04, 0.03), (0, -0.235, 0.46), frame, root, r=0.012)
    P.rbox(n + "_SeatMesh", (0.43, 0.46, 0.006), (0, 0.0, 0.463), mesh, root, r=0.002)
    # back: two uprights with a mesh panel, lumbar pad and a top crossbar
    for sx in (-1, 1):
        P.tube(n + "_Upright", [(sx * 0.235, -0.22, 0.46), (sx * 0.24, -0.27, 0.6), (sx * 0.235, -0.3, 0.95)], 0.019, frame, root, seg=12)
    P.rbox(n + "_BackTop", (0.5, 0.035, 0.05), (0, -0.305, 0.95), frame, root, rot=(RADS(-10), 0, 0), r=0.014)
    P.rbox(n + "_BackBottom", (0.5, 0.035, 0.05), (0, -0.235, 0.46), frame, root, r=0.014) if False else None
    bm = bmesh.new()
    P.rbox(n + "_BackMesh", (0.43, 0.006, 0.45), (0, -0.28, 0.74), mesh, root, rot=(RADS(-10), 0, 0), r=0.002)
    P.rbox(n + "_Lumbar", (0.26, 0.012, 0.09), (0, -0.265, 0.62), frame, root, rot=(RADS(-10), 0, 0), r=0.004)
    # arms: loop arm with pad
    for sx in (-1, 1):
        P.tube(n + "_Arm", [(sx * 0.28, 0.0, 0.46), (sx * 0.295, 0.0, 0.56), (sx * 0.295, -0.03, 0.66), (sx * 0.29, 0.02, 0.69)], 0.013, frame, root, seg=10)
        P.rbox(n + "_ArmPad", (0.06, 0.28, 0.025), (sx * 0.29, 0.03, 0.7), pl, root, r=0.009)
    finish(root, n)
    return root, (0.65, 0.66, 1.0)


def build_gaming(n):
    root = P.empty(n)
    black = P.tex("Gaming_Black", "leatherette_black.png", 0.45, size=1024, tint=(0.55, 0.55, 0.55), tilemult=0.4, fallback=(0.02, 0.02, 0.022))
    red = P.tex("Gaming_Red", "leatherette_red.png", 0.42, size=1024, tint=(0.8, 0.8, 0.8), tilemult=0.4, fallback=(0.55, 0.03, 0.03))
    metal = P.mat("Gaming_Metal", (0.1, 0.1, 0.11), 0.35, 0.8)
    rub = P.mat("Chair_Rubber", (0.015, 0.015, 0.015), 0.85)
    pl = plastic()
    star_base(n, root, metal, rub, 0.3, leg_len=0.33)
    P.rbox(n + "_Mech", (0.34, 0.34, 0.07), (0, 0, 0.405), pl, root, r=0.02)
    # bucket seat: centre pad, raised bolsters, rolled front
    P.rbox(n + "_SeatCore", (0.5, 0.5, 0.08), (0, 0.0, 0.48), black, root, r=0.035, seg=5)
    for sx in (-1, 1):
        P.blob(n + "_SeatBolster", (0.045, 0.27, 0.05), (sx * 0.235, 0.0, 0.53), red, root, seg=24, rings=12)
    P.blob(n + "_SeatFront", (0.22, 0.04, 0.05), (0, 0.245, 0.49), black, root, seg=24, rings=10)
    # high racing back with wings, headrest, lumbar pillows
    back = P.rbox(n + "_BackCore", (0.5, 0.1, 0.8), (0, -0.27, 0.9), black, root, rot=(RADS(-9), 0, 0), r=0.04, seg=5)
    for sx in (-1, 1):
        P.blob(n + "_BackWing", (0.05, 0.075, 0.28), (sx * 0.235, -0.235, 0.82), red, root, seg=20, rings=12) if False else None
        P.rbox(n + "_BackWing", (0.07, 0.09, 0.5), (sx * 0.225, -0.235, 0.78), red, root, rot=(RADS(-9), 0, 0), r=0.025, seg=4)
        P.rbox(n + "_HeadWing", (0.05, 0.07, 0.17), (sx * 0.13, -0.285, 1.3), red, root, rot=(RADS(-9), 0, 0), r=0.02, seg=4) if False else None
        P.rbox(n + "_ShoulderWing", (0.06, 0.09, 0.3), (sx * 0.225, -0.255, 1.04), red, root, rot=(RADS(-9), 0, 0), r=0.022, seg=4)
    P.rbox(n + "_BackCenterStripe", (0.12, 0.012, 0.65), (0, -0.21, 0.9), red, root, rot=(RADS(-9), 0, 0), r=0.004)
    P.blob(n + "_HeadPillow", (0.14, 0.055, 0.08), (0, -0.245, 1.17), red, root, seg=24, rings=12)
    P.blob(n + "_LumbarPillow", (0.14, 0.05, 0.07), (0, -0.215, 0.7), red, root, seg=24, rings=12)
    # 4D arms: post, base, pad
    for sx in (-1, 1):
        P.rbox(n + "_ArmPost", (0.05, 0.07, 0.2), (sx * 0.31, -0.03, 0.58), metal, root, r=0.01)
        P.rbox(n + "_ArmPad", (0.075, 0.26, 0.04), (sx * 0.31, 0.0, 0.7), black, root, r=0.016)
    finish(root, n)
    return root, (0.7, 0.68, 1.32)


def build_stool(n):
    root = P.empty(n)
    wd = P.tex("Stool_Wood", "wood_pine.png", 0.6, size=1024, tint=(0.62, 0.4, 0.2), tilemult=1.0, grain=True, fallback=(0.45, 0.3, 0.16))
    paint = P.mat("Stool_Paint", (0.32, 0.1, 0.05), 0.55)
    P.rbox(n + "_Seat", (0.34, 0.34, 0.032), (0, 0, 0.455), wd, root, r=0.007, seg=3)
    for sx in (-1, 1):
        for sy in (-1, 1):
            hull(n + "_Leg", [(sx * 0.115 + dx, sy * 0.115 + dy, 0.439) for dx in (-0.018, 0.018) for dy in (-0.018, 0.018)] +
                 [(sx * 0.145 + dx, sy * 0.145 + dy, 0.0) for dx in (-0.013, 0.013) for dy in (-0.013, 0.013)], wd, root, r=0.003, seg=2)
    for sy in (-1, 1):
        P.rbox(n + "_Rung", (0.27, 0.02, 0.028), (0, sy * 0.13, 0.17), wd, root, r=0.004)
    for sx in (-1, 1):
        P.rbox(n + "_Rung", (0.02, 0.27, 0.028), (sx * 0.13, 0, 0.12), wd, root, r=0.004)
    P.rbox(n + "_Apron", (0.3, 0.3, 0.05), (0, 0, 0.41), wd, root, r=0.003) if False else None
    finish(root, n)
    return root, (0.34, 0.34, 0.48)


def build_bentwood(n):
    root = P.empty(n)
    wd = P.tex("Bentwood", "wood_dark.png", 0.35, size=1024, tint=(0.8, 0.7, 0.62), tilemult=0.5, grain=True, fallback=(0.25, 0.13, 0.06))
    cane = P.mat("Bentwood_Cane", (0.5, 0.35, 0.18), 0.7)
    # round seat: wooden rim plus a pressed seat plate
    P.cylinder(n + "_SeatPlate", 0.205, 0.012, (0, 0.0, 0.452), cane, root, seg=36, bevel=0.003)
    P.cylinder(n + "_SeatRim", 0.215, 0.03, (0, 0.0, 0.44), wd, root, seg=36, bevel=0.004)
    # front legs: straight, slightly splayed; back legs continue up into the back frame
    for sx in (-1, 1):
        P.tube(n + "_LegF", [(sx * 0.15, 0.15, 0.44), (sx * 0.185, 0.185, 0.0)], 0.016, wd, root, seg=12, smooth_pts=False)
        P.tube(n + "_LegB", [(sx * 0.185, -0.185, 0.0), (sx * 0.15, -0.15, 0.44), (sx * 0.145, -0.19, 0.7), (sx * 0.14, -0.215, 0.95)], 0.016, wd, root, seg=12)
    # back: top rail, loop, two spindles
    P.tube(n + "_BackLoop", [(-0.14, -0.215, 0.95), (-0.11, -0.225, 0.99), (0, -0.23, 1.0), (0.11, -0.225, 0.99), (0.14, -0.215, 0.95)], 0.014, wd, root, seg=12)
    P.tube(n + "_BackMidRail", [(-0.145, -0.2, 0.75), (-0.07, -0.215, 0.77), (0.07, -0.215, 0.77), (0.145, -0.2, 0.75)], 0.012, wd, root, seg=10)
    P.tube(n + "_BackRing", [(math.cos(a_ / 20 * 2 * math.pi) * 0.055, -0.215, 0.85 + math.sin(a_ / 20 * 2 * math.pi) * 0.055) for a_ in range(21)], 0.008, wd, root, seg=8, smooth_pts=False)
    for sx in (-1, 0, 1):
        P.tube(n + "_Spindle", [(sx * 0.085, -0.2, 0.62), (sx * 0.085, -0.215, 0.76)], 0.007, wd, root, seg=8, smooth_pts=False) if False else None
    for sx in (-1, 1):
        P.tube(n + "_Spindle", [(sx * 0.085, -0.2, 0.64), (sx * 0.085, -0.215, 0.78)], 0.007, wd, root, seg=8, smooth_pts=False)
        P.tube(n + "_SpindleUp", [(sx * 0.085, -0.215, 0.92), (sx * 0.085, -0.222, 0.968)], 0.007, wd, root, seg=8, smooth_pts=False)
    # lower ring brace
    P.tube(n + "_Ring", [(math.cos(a / 24 * 2 * math.pi) * 0.17, math.sin(a / 24 * 2 * math.pi) * 0.17, 0.2) for a in range(25)], 0.01, wd, root, seg=8, smooth_pts=False)
    finish(root, n)
    return root, (0.42, 0.45, 1.02)


def build_folding(n):
    root = P.empty(n)
    metal = P.tex("Folding_Metal", "metal_painted_grey.png", 0.45, 0.6, size=512, tilemult=1.0, fallback=(0.35, 0.36, 0.37))
    seat = P.mat("Folding_Seat", (0.16, 0.17, 0.18), 0.5, 0.3)
    rub = P.mat("Chair_Rubber", (0.015, 0.015, 0.015), 0.85)
    # pressed seat and back panels
    P.rbox(n + "_Seat", (0.42, 0.4, 0.014), (0, 0.02, 0.45), seat, root, r=0.006, seg=3)
    P.rbox(n + "_SeatLip", (0.42, 0.014, 0.03), (0, 0.222, 0.44), seat, root, r=0.005)
    P.rbox(n + "_Back", (0.38, 0.012, 0.19), (0, -0.215, 0.76), seat, root, rot=(RADS(-8), 0, 0), r=0.006, seg=3)
    for sx in (-1, 1):
        x = sx * 0.19
        # rear leg runs from the floor up through the seat frame to the back panel
        P.tube(n + "_LegBack", [(x, -0.2, 0.0), (x, -0.17, 0.44), (x, -0.215, 0.87)], 0.0115, metal, root, seg=10, smooth_pts=False)
        # front leg from the seat frame to the floor
        P.tube(n + "_LegFront", [(x, 0.17, 0.44), (x, 0.205, 0.0)], 0.0115, metal, root, seg=10, smooth_pts=False)
        P.tube(n + "_SeatRail", [(x, -0.17, 0.442), (x, 0.17, 0.442)], 0.009, metal, root, seg=8, smooth_pts=False)
        P.tube(n + "_Brace", [(x, -0.188, 0.2), (x, 0.19, 0.22)], 0.006, metal, root, seg=6, smooth_pts=False)
        for yy in (-0.2, 0.205):
            P.cylinder(n + "_Foot", 0.0145, 0.016, (x, yy, 0.008), rub, root, seg=12)
    P.tube(n + "_CrossBack", [(-0.19, -0.185, 0.2), (0.19, -0.185, 0.2)], 0.008, metal, root, seg=8, smooth_pts=False)
    P.tube(n + "_CrossFront", [(-0.19, 0.17, 0.442), (0.19, 0.17, 0.442)], 0.008, metal, root, seg=8, smooth_pts=False)
    finish(root, n)
    return root, (0.42, 0.46, 0.9)


BUILDERS = {"chair_office": build_office, "chair_aeron": build_aeron, "chair_gaming": build_gaming,
            "chair_stool": build_stool, "chair_bentwood": build_bentwood, "chair_folding": build_folding}


def main():
    names = [a for a in P.ARGS if not a.startswith("--")] or list(BUILDERS)
    do_render = "--no-render" not in P.ARGS
    for nm in names:
        scn = P.reset()
        root, size = BUILDERS[nm](nm)
        P.export(root, nm)
        n, tris = P.stats(root)
        print("BUILT %s size=%.3f x %.3f x %.3f m objects=%d tris=%d" % (nm, size[0], size[1], size[2], n, tris))
        if do_render:
            P.setup_studio(scn, (700, 800))
            P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0, size[2] * 0.45), (1.3, 1.5, size[2] * 0.85), 50)


if __name__ == "__main__":
    main()
