"""Small desk and floor props: soda cups/cans/bottles, fast-food litter, desk fan, lamps, plants, books, tea glass,
trinkets, and the drinks vending machine.

  blender -b -P blender/props/build_small.py -- [name ...] [--no-render]

Origin on the surface the thing stands on (centre of its base), front faces -Y, Z up.
fan_desk: the blades are a separate mesh `fan_desk_Blades` whose origin is the hub; spin it about the Blender Y axis
(glTF -Z / the fan's axis, it blows toward -Y).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

import propslib as P
from build_computers import finish, hull, RADS, black, dark, rubber


def matte(name, color, rough=0.6, **kw):
    return P.mat(name, color, rough, **kw)


def glass_mat(name="Glass_Clear", color=(0.75, 0.85, 0.85), alpha=0.3):
    return P.mat(name, color, 0.04, alpha=alpha, trans=1.0)


# ------------------------------------------------------------ cups, cans, bottles

def cup_parts(n, root, lying=False):
    red = P.mat("Cup_Red", (0.55, 0.03, 0.04), 0.55)
    white = P.mat("Cup_White", (0.9, 0.9, 0.88), 0.45)
    straw = P.mat("Straw", (0.9, 0.9, 0.92), 0.35)
    holder = P.mat("Cup_Ice", (0.7, 0.2, 0.15), 0.9)
    h = 0.16
    # outer wall: tapered paper cup with rolled rim, white band, then a plastic lid with a dome
    P.lathe(n + "_Wall", [(0.0, 0.0), (0.03, 0.0), (0.0305, 0.004), (0.042, 0.15), (0.0435, 0.154), (0.0445, 0.16)], (0, 0, 0), red, root, seg=36)
    P.lathe(n + "_Band", [(0.0345, 0.045), (0.0415, 0.105), (0.0425, 0.105), (0.0355, 0.045)], (0, 0, 0), white, root, seg=36)
    P.lathe(n + "_Lid", [(0.0455, 0.153), (0.0468, 0.158), (0.0468, 0.165), (0.04, 0.166), (0.03, 0.172), (0.0, 0.173)], (0, 0, 0), white, root, seg=36)
    P.cylinder(n + "_Hole", 0.004, 0.002, (0.016, 0.0, 0.1725), black(), root, seg=10)
    P.tube(n + "_Straw", [(0.016, 0.0, 0.05), (0.016, 0.0, 0.2), (0.021, 0.0, 0.245), (0.036, 0.0, 0.262)], 0.0035, straw, root, seg=8)
    P.lathe(n + "_Logo", [(0.0394, 0.07), (0.0402, 0.092), (0.0412, 0.092), (0.0404, 0.07)], (0, 0, 0), P.mat("Cup_Logo", (0.75, 0.55, 0.05), 0.6), root, seg=36)


def build_cup(n, lying=False):
    root = P.empty(n)
    cup_parts(n, root)
    finish(root, n)
    if lying:
        for o in root.children:
            pass
        root.rotation_euler = (0, 0, 0)
        # bake a lying pose: rotate all children about X by 90 degrees and rest on the side
        for o in bpy.data.objects:
            if o.parent == root and o.type == "MESH":
                me = o.data
                me.transform(Matrix.Translation((0, 0, -0.08)))
                me.transform(Matrix.Rotation(RADS(90), 4, "X"))
                me.transform(Matrix.Translation((0, 0, 0.04)))
    return root, (0.09, 0.3, 0.16) if lying else (0.09, 0.09, 0.27)


def can(n, root, x, y, z, main, accent, fresh=True, tab=True, rot=0.0, label_h=None, lo=False):
    """A 330 ml can. Radius 0.033, height 0.115. Returns the can's top z."""
    alu = P.mat("Can_Alu", (0.75, 0.75, 0.77), 0.25, 1.0)
    body = P.lathe(n + "_Can", [(0.0, 0.0), (0.024, 0.0), (0.028, 0.004), (0.033, 0.01), (0.033, 0.098), (0.031, 0.104), (0.0275, 0.108), (0.0, 0.108)],
                   (x, y, z), main, root, rot=(0, 0, rot), seg=12 if lo else 28)
    if lo:
        P.cylinder(n + "_CanLid", 0.0262, 0.0008, (x, y, z + 0.1085), alu, root, seg=12)
        return z + 0.115
    P.lathe(n + "_CanBottom", [(0.0, 0.0), (0.0275, 0.0), (0.0285, 0.003)], (x, y, z), alu, root, seg=28) if False else None
    P.lathe(n + "_CanRim", [(0.0275, 0.1075), (0.0285, 0.111), (0.0265, 0.114), (0.0245, 0.1115), (0.0245, 0.1085)], (x, y, z), alu, root, seg=28)
    P.cylinder(n + "_CanLid", 0.0262, 0.0008, (x, y, z + 0.1085), alu, root, seg=28)
    if tab:
        P.rbox(n + "_CanTab", (0.012, 0.018, 0.0012), (x + 0.0, y + 0.002, z + 0.1103), alu, root, r=0.0004, seg=2)
    # printed stripe band
    P.lathe(n + "_CanBand", [(0.0333, 0.045), (0.0335, 0.045), (0.0335, 0.075), (0.0333, 0.075)], (x, y, z), accent, root, rot=(0, 0, rot), seg=28)
    return z + 0.115


def build_can(n, main, accent):
    root = P.empty(n)
    can(n, root, 0, 0, 0, main, accent)
    finish(root, n)
    return root, (0.066, 0.066, 0.115)


def build_bottle(n, liquid, label, cap):
    root = P.empty(n)
    pet = P.mat("Bottle_PET", (0.8, 0.88, 0.88), 0.05, alpha=0.35, trans=1.0)
    liq = P.mat("Bottle_" + n, liquid, 0.15)
    P.lathe(n + "_Body", [(0.0, 0.0), (0.02, 0.0), (0.0305, 0.006), (0.032, 0.02), (0.032, 0.135), (0.03, 0.165), (0.016, 0.19), (0.0135, 0.2), (0.0135, 0.215)],
            (0, 0, 0), pet, root, seg=28)
    P.lathe(n + "_Liquid", [(0.0, 0.002), (0.019, 0.002), (0.0295, 0.007), (0.0308, 0.02), (0.0308, 0.12), (0.0, 0.12)], (0, 0, 0), liq, root, seg=28)
    P.lathe(n + "_Label", [(0.0323, 0.05), (0.0325, 0.05), (0.0325, 0.1), (0.0323, 0.1)], (0, 0, 0), label, root, seg=28)
    P.cylinder(n + "_Cap", 0.0155, 0.02, (0, 0, 0.215), cap, root, seg=20, bevel=0.0008)
    P.lathe(n + "_Ring", [(0.0138, 0.201), (0.017, 0.2), (0.017, 0.202), (0.0138, 0.203)], (0, 0, 0), pet, root, seg=20)
    finish(root, n)
    return root, (0.066, 0.066, 0.225)


# ------------------------------------------------------------ fast food litter

def build_food_bag(n):
    root = P.empty(n)
    kraft = P.mat("Kraft_Paper", (0.55, 0.4, 0.24), 0.9)
    logo = P.mat("Bag_Logo", (0.7, 0.1, 0.05), 0.8)
    # squared-off paper bag with a gusset and a folded-over top with a crease
    P.rbox(n + "_Body", (0.15, 0.09, 0.2), (0, 0, 0.1), kraft, root, r=0.003)
    P.rbox(n + "_Fold", (0.15, 0.034, 0.04), (0, 0.0, 0.215), kraft, root, rot=(RADS(-22), 0, 0), r=0.003)
    P.rbox(n + "_Fold2", (0.15, 0.03, 0.035), (0, -0.008, 0.24), kraft, root, rot=(RADS(-45), 0, 0), r=0.003)
    for sx in (-1, 1):
        P.rbox(n + "_Gusset", (0.002, 0.07, 0.17), (sx * 0.0755, 0.0, 0.1), P.mat("Kraft_Dark", (0.47, 0.33, 0.19), 0.9), root)
    P.rbox(n + "_Logo", (0.07, 0.001, 0.07), (0, -0.0455, 0.11), logo, root)
    P.rbox(n + "_Logo2", (0.045, 0.0012, 0.02), (0, -0.0456, 0.11), P.mat("Bag_LogoYellow", (0.9, 0.7, 0.1), 0.6), root, r=0.003)
    finish(root, n)
    return root, (0.15, 0.09, 0.26)


def build_burger_box(n, open_):
    root = P.empty(n)
    card = P.mat("Burger_Box", (0.85, 0.75, 0.55), 0.8)
    print_ = P.mat("Burger_Box_Print", (0.75, 0.18, 0.05), 0.7)
    bun = P.mat("Bun", (0.8, 0.5, 0.2), 0.6)
    patty = P.mat("Patty", (0.22, 0.1, 0.06), 0.8)
    green = P.mat("Lettuce", (0.3, 0.6, 0.15), 0.6)
    P.rbox(n + "_Base", (0.13, 0.13, 0.04), (0, 0, 0.02), card, root, r=0.004)
    P.rbox(n + "_BaseInner", (0.118, 0.118, 0.002), (0, 0, 0.041), P.mat("Burger_Inside", (0.9, 0.82, 0.65), 0.8), root)
    if open_:
        # burger sitting inside, lid hinged back
        P.blob(n + "_BunBottom", (0.05, 0.05, 0.014), (0, 0, 0.052), bun, root, seg=20, rings=8)
        P.cylinder(n + "_Patty", 0.052, 0.012, (0, 0, 0.073), patty, root, seg=20)
        P.cylinder(n + "_Lettuce", 0.056, 0.006, (0, 0, 0.083), green, root, seg=20)
        P.blob(n + "_BunTop", (0.052, 0.052, 0.03), (0, 0, 0.094), bun, root, seg=20, rings=10)
        P.rbox(n + "_Lid", (0.13, 0.13, 0.03), (0, 0.075, 0.085), card, root, rot=(RADS(70), 0, 0), r=0.004)
    else:
        P.rbox(n + "_Lid", (0.132, 0.132, 0.03), (0, 0, 0.055), card, root, r=0.01, seg=3)
        P.rbox(n + "_Print", (0.133, 0.03, 0.004), (0, -0.0, 0.0705), print_, root)
        P.rbox(n + "_Print2", (0.03, 0.133, 0.004), (0, 0.0, 0.071), P.mat("Burger_Box_Yellow", (0.9, 0.7, 0.1), 0.6), root) if False else None
    finish(root, n)
    return root, (0.13, 0.2, 0.1) if open_ else (0.133, 0.133, 0.075)


def build_fries(n):
    root = P.empty(n)
    red = P.mat("Fries_Box", (0.7, 0.05, 0.04), 0.6)
    yel = P.mat("Fries", (0.9, 0.7, 0.2), 0.7)
    cream = P.mat("Fries_Print", (0.95, 0.85, 0.3), 0.6)
    # tapered carton with a scalloped front and fries in a cluster
    hull(n + "_Box", [(sx * 0.045, sy * 0.02, 0.0) for sx in (-1, 1) for sy in (-1, 1)] + [(sx * 0.06, sy * 0.032, 0.095) for sx in (-1, 1) for sy in (-1, 1)], red, root, r=0.002, seg=2)
    hull(n + "_BoxFront", [(sx * 0.047, -0.021, 0.0) for sx in (-1, 1)] + [(sx * 0.047, -0.021, 0.07) for sx in (-1, 1)] + [(sx * 0.047, -0.03, 0.02) for sx in (-1, 1)], red, root, r=0.001, seg=1) if False else None
    P.rbox(n + "_Logo", (0.04, 0.001, 0.03), (0, -0.0272, 0.05), cream, root, rot=(RADS(-5), 0, 0), r=0.003)
    rng_x = [(-0.04, 0.01), (-0.02, -0.01), (0.0, 0.015), (0.02, -0.005), (0.04, 0.008), (-0.03, -0.012), (0.01, 0.0), (0.03, 0.012), (-0.01, 0.008), (0.0, -0.014), (0.018, 0.0), (-0.045, 0.0)]
    for k, (dx, dy) in enumerate(rng_x):
        L = 0.065 + 0.012 * ((k * 7) % 3)
        P.rbox(n + "_Fry", (0.009, 0.009, L), (dx, dy, 0.092 + L / 2 - 0.02), yel, root,
               rot=(RADS(6 * ((k % 3) - 1)), RADS(8 * ((k % 5) - 2)), RADS(k * 31)), r=0.0012, seg=1)
    finish(root, n)
    return root, (0.12, 0.065, 0.18)


# ------------------------------------------------------------ desk fan

def blade_mesh(n, root, hub_z, mat_):
    bm = bmesh.new()
    nb = 5
    for b in range(nb):
        a = b * 2 * math.pi / nb
        tmp = bmesh.new()
        bmesh.ops.create_uvsphere(tmp, u_segments=16, v_segments=8, radius=1.0)
        for v in tmp.verts:
            v.co.x *= 0.05      # blade length along its own X
            v.co.y *= 0.022
            v.co.z *= 0.0035
        bmesh.ops.rotate(tmp, verts=tmp.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(RADS(28), 3, "X"))   # pitch
        bmesh.ops.translate(tmp, verts=tmp.verts, vec=(0.062, 0, 0))
        bmesh.ops.rotate(tmp, verts=tmp.verts, cent=(0, 0, 0), matrix=Matrix.Rotation(a, 3, "Y"))        # around the Y axis
        me = bpy.data.meshes.new("tmp")
        tmp.to_mesh(me)
        tmp.free()
        bm.from_mesh(me)
        bpy.data.meshes.remove(me)
    return P._make(n + "_Blades", bm, mat_, root, 0.2, 60)


def build_fan(n):
    root = P.empty(n)
    wh = P.mat("Fan_White", (0.8, 0.82, 0.78), 0.4)
    grille = P.mat("Fan_Grille", (0.6, 0.62, 0.62), 0.3, 1.0)
    blade = P.mat("Fan_Blade", (0.7, 0.75, 0.8), 0.3, alpha=0.65)
    hub_z = 0.26
    P.rbox(n + "_Base", (0.17, 0.13, 0.03), (0, 0.0, 0.015), wh, root, r=0.012, seg=4)
    P.cylinder(n + "_Stem", 0.011, 0.2, (0, 0.03, 0.13), wh, root, seg=14)
    P.cylinder(n + "_Motor", 0.04, 0.07, (0, 0.045, hub_z), wh, root, rot=(RADS(90), 0, 0), seg=24, bevel=0.004)
    P.cylinder(n + "_Knob", 0.012, 0.02, (0, 0.085, hub_z + 0.015), wh, root, rot=(RADS(90), 0, 0), seg=12)
    # guard: rings + spokes (front and back), centre cap
    for r_ in (0.035, 0.07, 0.105, 0.135):
        P.lathe(n + "_Ring", [(r_ - 0.0015, 0.0), (r_ + 0.0015, 0.0), (r_ + 0.0015, 0.002), (r_ - 0.0015, 0.002)], (0, -0.04, hub_z), grille, root, rot=(RADS(90), 0, 0), seg=32) if False else None
    for r_, y in ((0.138, -0.04), (0.138, 0.0), (0.095, -0.047), (0.055, -0.05)):
        pts = [(math.cos(a / 32 * 2 * math.pi) * r_, y, hub_z + math.sin(a / 32 * 2 * math.pi) * r_) for a in range(33)]
        P.tube(n + "_Hoop", pts, 0.0017, grille, root, seg=5, smooth_pts=False)
    for k in range(16):
        a = k * 2 * math.pi / 16
        P.tube(n + "_Spoke", [(math.cos(a) * 0.015, -0.052, hub_z + math.sin(a) * 0.015), (math.cos(a) * 0.09, -0.05, hub_z + math.sin(a) * 0.09),
                              (math.cos(a) * 0.138, -0.04, hub_z + math.sin(a) * 0.138)], 0.0014, grille, root, seg=4)
        P.tube(n + "_SpokeB", [(math.cos(a) * 0.138, -0.04, hub_z + math.sin(a) * 0.138), (math.cos(a) * 0.138, 0.0, hub_z + math.sin(a) * 0.138)], 0.0014, grille, root, seg=4)
    P.cylinder(n + "_Cap", 0.018, 0.01, (0, -0.054, hub_z), wh, root, rot=(RADS(90), 0, 0), seg=16)
    b = blade_mesh(n, root, hub_z, blade)
    b.location = (0, -0.01, hub_z)
    P.cylinder(n + "_Hub", 0.014, 0.02, (0, -0.012, hub_z), wh, root, rot=(RADS(90), 0, 0), seg=16)
    P.merge_by_material(root, keep=(n + "_Blades",), name=n)
    return root, (0.17, 0.13, 0.4)


# ------------------------------------------------------------ lamps, plants, books, trinkets

def build_lamp_soviet(n):
    root = P.empty(n)
    green = P.mat("Lamp_GreenShade", (0.05, 0.22, 0.1), 0.35, 0.5)
    brass = P.mat("Brass", (0.72, 0.55, 0.22), 0.28, 1.0)
    bulb = P.mat("Lamp_Bulb", (1.0, 0.85, 0.6), 0.2, emit=(1.0, 0.68, 0.38), strength=10.0)
    P.cylinder(n + "_Base", 0.07, 0.025, (0, 0, 0.0125), green, root, seg=32, r2=0.062, bevel=0.003)
    P.cylinder(n + "_BaseCap", 0.03, 0.02, (0, 0, 0.035), brass, root, seg=20)
    P.tube(n + "_Arm1", [(0, 0.0, 0.04), (0, 0.04, 0.17), (0, 0.04, 0.31)], 0.007, brass, root, seg=10)
    P.blob(n + "_Joint", (0.014, 0.014, 0.014), (0, 0.04, 0.31), brass, root, seg=12, rings=8)
    P.tube(n + "_Arm2", [(0, 0.04, 0.31), (0, -0.04, 0.33), (0, -0.12, 0.31)], 0.006, brass, root, seg=10)
    shade_rot = (RADS(-30), 0, 0)
    P.lathe(n + "_Shade", [(0.03, 0.0), (0.034, 0.015), (0.06, 0.1), (0.07, 0.112), (0.068, 0.114), (0.058, 0.102), (0.032, 0.02)], (0, -0.14, 0.31), green, root, rot=(RADS(150), 0, 0), seg=28) if False else None
    sh = P.lathe(n + "_Shade", [(0.02, 0.1), (0.02, 0.09), (0.06, 0.0), (0.0615, -0.002), (0.0595, -0.002), (0.019, 0.088)], (0, -0.14, 0.295), green, root, rot=(RADS(-35), 0, 0), seg=32)
    P.cylinder(n + "_Bulb", 0.024, 0.045, (0, -0.1475, 0.285), bulb, root, seg=14, r2=0.016, rot=(RADS(-35), 0, 0))
    P.cylinder(n + "_Socket", 0.013, 0.025, (0, -0.131, 0.31), brass, root, seg=12, rot=(RADS(-35), 0, 0))
    P.tube(n + "_Cable", [(0.0, 0.07, 0.01), (0.02, 0.12, 0.003), (0.1, 0.2, 0.003), (0.2, 0.3, 0.003)], 0.0022, P.mat("Cable_Cloth", (0.35, 0.3, 0.2), 0.8), root, seg=6)
    finish(root, n)
    return root, (0.14, 0.3, 0.42)


def build_rose(n):
    root = P.empty(n)
    glass = glass_mat("Vase_Glass", (0.6, 0.72, 0.72), 0.3)
    water = P.mat("Vase_Water", (0.55, 0.75, 0.75), 0.05, alpha=0.5, trans=1.0)
    stem = P.mat("Rose_Stem", (0.1, 0.3, 0.08), 0.6)
    petal = P.mat("Rose_Petal", (0.55, 0.02, 0.06), 0.5)
    petal2 = P.mat("Rose_PetalDark", (0.38, 0.01, 0.04), 0.5)
    P.lathe(n + "_Vase", [(0.0, 0.0), (0.028, 0.0), (0.034, 0.01), (0.034, 0.12), (0.03, 0.17), (0.0, 0.17)], (0, 0, 0), glass, root, seg=24) if False else None
    P.lathe(n + "_Vase", [(0.0, 0.0), (0.03, 0.0), (0.0355, 0.012), (0.0355, 0.09), (0.03, 0.16), (0.033, 0.18), (0.0315, 0.18), (0.0285, 0.16), (0.034, 0.09), (0.034, 0.014), (0.0, 0.006)], (0, 0, 0), glass, root, seg=28)
    P.lathe(n + "_Water", [(0.0, 0.008), (0.032, 0.012), (0.033, 0.09), (0.0, 0.1)], (0, 0, 0), water, root, seg=24) if False else None
    P.tube(n + "_Stem", [(0.0, 0.0, 0.01), (0.006, 0.0, 0.15), (0.0, 0.0, 0.3), (-0.004, 0.0, 0.4)], 0.0042, stem, root, seg=8)
    for k, (z, a) in enumerate(((0.2, 0.3), (0.28, 2.6), (0.34, 4.3))):
        P.blob(n + "_Leaf", (0.028, 0.012, 0.003), (math.cos(a) * 0.028, math.sin(a) * 0.028, z), stem, root, rot=(RADS(20), 0, a), seg=14, rings=6)
        P.tube(n + "_Thorn", [(0, 0, z - 0.01), (math.cos(a + 1.5) * 0.006, math.sin(a + 1.5) * 0.006, z - 0.004)], 0.0016, stem, root, seg=4) if False else None
    # bloom: layered petals as overlapping flattened ellipsoids
    cz = 0.425
    P.blob(n + "_Bud", (0.016, 0.016, 0.02), (-0.004, 0, cz), petal2, root, seg=16, rings=8)
    for layer, (rad, nn, hh, rr) in enumerate(((0.017, 5, 0.01, 0.019), (0.027, 6, 0.006, 0.024), (0.036, 7, 0.0, 0.02))):
        for k in range(nn):
            a = k * 2 * math.pi / nn + layer * 0.5
            P.blob(n + "_Petal", (0.015 + 0.004 * layer, 0.008, 0.017), (-0.004 + math.cos(a) * rad * 0.7, math.sin(a) * rad * 0.7, cz + 0.004 - layer * 0.007 + hh * 0.2),
                   petal if layer != 0 else petal2, root, rot=(RADS(-25 + 18 * layer) * math.sin(a), RADS(25 - 18 * layer) * math.cos(a), a), seg=12, rings=6)
    finish(root, n)
    return root, (0.07, 0.07, 0.45)


def build_grow_lamp(n):
    root = P.empty(n)
    b = black()
    purple = P.mat("GrowLamp_Purple", (0, 0, 0), 0.5, emit=(0.6, 0.1, 1.0), strength=20.0)
    P.rbox(n + "_Clamp", (0.05, 0.05, 0.03), (0, 0, 0.0), b, root, r=0.005) if False else None
    P.cylinder(n + "_Base", 0.07, 0.02, (0, 0, 0.01), b, root, seg=32, bevel=0.003)
    P.cylinder(n + "_Pole", 0.008, 0.6, (0, 0, 0.31), b, root, seg=12)
    P.tube(n + "_Arm", [(0, 0, 0.6), (-0.04, 0, 0.64), (-0.12, 0, 0.62), (-0.18, 0, 0.58)], 0.007, b, root, seg=10)
    P.cylinder(n + "_Head", 0.06, 0.03, (-0.18, 0, 0.56), b, root, seg=28, bevel=0.003)
    P.cylinder(n + "_LED", 0.05, 0.004, (-0.18, 0, 0.543), purple, root, seg=28)
    finish(root, n)
    return root, (0.25, 0.14, 0.66)


def build_plant(n, cactus):
    root = P.empty(n)
    pot = P.mat("Pot_Terracotta", (0.5, 0.22, 0.1), 0.8)
    soil = P.mat("Soil", (0.08, 0.05, 0.03), 0.95)
    leaf = P.mat("Plant_Leaf", (0.06, 0.3, 0.09), 0.6)
    P.lathe(n + "_Pot", [(0.0, 0.0), (0.042, 0.0), (0.058, 0.09), (0.064, 0.095), (0.064, 0.108), (0.058, 0.108), (0.054, 0.095), (0.0, 0.095)], (0, 0, 0), pot, root, seg=32)
    P.cylinder(n + "_Soil", 0.055, 0.004, (0, 0, 0.094), soil, root, seg=24)
    P.cylinder(n + "_Saucer", 0.07, 0.008, (0, 0, 0.004), pot, root, seg=32, r2=0.075) if False else None
    if cactus:
        P.lathe(n + "_Body", [(0.0, 0.092), (0.03, 0.095), (0.034, 0.12), (0.03, 0.2), (0.015, 0.235), (0.0, 0.24)], (0, 0, 0), leaf, root, seg=16)
        P.tube(n + "_ArmA", [(0.025, 0, 0.14), (0.065, 0, 0.15), (0.075, 0, 0.19)], 0.016, leaf, root, seg=10)
        P.blob(n + "_ArmTip", (0.016, 0.016, 0.016), (0.075, 0, 0.195), leaf, root, seg=12, rings=8)
        for k in range(22):
            a = k * 0.95
            z = 0.11 + (k % 8) * 0.016
            P.rbox(n + "_Spine", (0.0016, 0.0016, 0.006), (math.cos(a) * 0.0325, math.sin(a) * 0.0325, z), P.mat("Cactus_Spine", (0.9, 0.88, 0.7), 0.6), root, rot=(0, RADS(80), a), r=0.0003, seg=1)
    else:
        for k in range(9):
            a = k * 2 * math.pi / 9 + 0.2
            tilt = 0.3 + 0.12 * (k % 3)
            L = 0.17 + 0.03 * (k % 4)
            tip = Vector((math.cos(a) * L * math.sin(tilt) * 1.3, math.sin(a) * L * math.sin(tilt) * 1.3, 0.098 + L * math.cos(tilt)))
            mid = Vector((math.cos(a) * L * 0.4 * math.sin(tilt), math.sin(a) * L * 0.4 * math.sin(tilt), 0.098 + L * 0.5))
            P.tube(n + "_Stalk", [(math.cos(a) * 0.01, math.sin(a) * 0.01, 0.098), mid, tip], 0.0025, leaf, root, seg=5)
            P.blob(n + "_Leaf", (0.038, 0.002, 0.026), tip + Vector((math.cos(a) * 0.02, math.sin(a) * 0.02, 0.0)), leaf, root, rot=(RADS(-35), 0, a - math.pi / 2), seg=14, rings=6)
    finish(root, n)
    return root, (0.13, 0.13, 0.25 if cactus else 0.32)


def build_books(n, count):
    root = P.empty(n)
    cols = [(0.4, 0.05, 0.05), (0.05, 0.15, 0.35), (0.1, 0.3, 0.12), (0.6, 0.5, 0.2), (0.15, 0.12, 0.1)]
    paper = P.mat("Book_Pages", (0.85, 0.8, 0.65), 0.9)
    gold = P.mat("Book_Gold", (0.75, 0.6, 0.2), 0.4, 0.6)
    z = 0.0
    for k in range(count):
        th = 0.026 + 0.012 * (k % 3)
        w, d = 0.165 + 0.02 * (k % 2), 0.235 - 0.02 * (k % 2)
        c = P.mat("Book_%d" % (k % len(cols)), cols[k % len(cols)], 0.7)
        a = RADS(7 * (k - 1))
        P.rbox(n + "_Cover", (w, d, th), (0, 0, z + th / 2), c, root, rot=(0, 0, a), r=0.002, seg=2)
        # page block visible on three sides
        P.rbox(n + "_Pages", (w - 0.006, d + 0.0015, th - 0.006), (0.0015, 0.0, z + th / 2), paper, root, rot=(0, 0, a), r=0.0005, seg=1)
        P.rbox(n + "_Title", (0.006, 0.12, 0.008), (-w / 2 - 0.0005, 0.0, z + th / 2), gold, root, rot=(0, 0, a)) if False else None
        z += th
    finish(root, n)
    return root, (0.2, 0.25, z)


def build_tea(n):
    root = P.empty(n)
    metal = P.mat("Podstakannik", (0.62, 0.57, 0.45), 0.28, 1.0)
    glass = P.mat("Tea_Glass", (0.65, 0.4, 0.12), 0.04, alpha=0.7, trans=0.8)
    tea = P.mat("Tea_Liquid", (0.45, 0.17, 0.03), 0.1)
    # filigree holder: lathe holder with a decorated waist, ring handle
    P.lathe(n + "_Holder", [(0.0, 0.0), (0.034, 0.0), (0.04, 0.006), (0.037, 0.02), (0.036, 0.04), (0.04, 0.062), (0.0415, 0.066), (0.0395, 0.066), (0.036, 0.06), (0.0345, 0.04), (0.035, 0.02), (0.0, 0.008)], (0, 0, 0), metal, root, seg=36)
    for k in range(10):
        a = k * 2 * math.pi / 10
        P.blob(n + "_Ornament", (0.007, 0.0022, 0.011), (math.cos(a) * 0.0385, math.sin(a) * 0.0385, 0.035), metal, root, rot=(0, 0, a - math.pi / 2), seg=8, rings=5)
    P.tube(n + "_Handle", [(0.0385, 0, 0.058), (0.062, 0.0, 0.055), (0.066, 0.0, 0.03), (0.04, 0.0, 0.012)], 0.0035, metal, root, seg=8)
    P.lathe(n + "_Glass", [(0.0, 0.004), (0.028, 0.004), (0.033, 0.01), (0.035, 0.11), (0.0, 0.11)], (0, 0, 0.006), tea, root, seg=28) if False else None
    P.lathe(n + "_Glass", [(0.0, 0.0), (0.028, 0.0), (0.0335, 0.006), (0.0355, 0.1), (0.0345, 0.1), (0.0325, 0.01), (0.0, 0.006)], (0, 0, 0.006), glass, root, seg=32)
    P.lathe(n + "_Tea", [(0.0, 0.008), (0.027, 0.008), (0.0325, 0.012), (0.0345, 0.085), (0.0, 0.085)], (0, 0, 0.006), tea, root, seg=28)
    P.tube(n + "_Spoon", [(-0.012, 0.0, 0.04), (-0.016, 0.0, 0.1), (-0.02, 0.0, 0.135), (-0.026, 0.0, 0.14)], 0.0025, metal, root, seg=6)
    P.blob(n + "_SpoonBowl", (0.011, 0.007, 0.0025), (-0.014, 0.0, 0.03), metal, root, rot=(0, RADS(10), 0), seg=10, rings=6)
    P.cylinder(n + "_Saucer", 0.05, 0.003, (0, 0, 0.0015), metal, root, seg=32, r2=0.052) if False else None
    finish(root, n)
    return root, (0.14, 0.09, 0.14)


def build_rubik(n):
    root = P.empty(n)
    cols = [(0.8, 0.05, 0.05), (0.05, 0.5, 0.1), (0.05, 0.15, 0.7), (0.9, 0.8, 0.1), (0.9, 0.9, 0.9), (0.9, 0.4, 0.05)]
    blk = P.mat("Rubik_Black", (0.01, 0.01, 0.012), 0.45)
    s = 0.057
    P.rbox(n + "_Core", (s, s, s), (0, 0, s / 2 + 0.0), blk, root, r=0.003, seg=2, rot=(0, 0, RADS(20)))
    c = s / 3
    rot = RADS(20)
    ca, sa = math.cos(rot), math.sin(rot)
    # six faces of 3 x 3 stickers (placed in the cube's rotated frame)
    def pt(x, y, z):
        return (x * ca - y * sa, x * sa + y * ca, z + s / 2)
    for face, col in enumerate(cols):
        for i in range(3):
            for j in range(3):
                u, v = (i - 1) * c, (j - 1) * c
                if face == 0:   loc, sz = pt(s / 2 + 0.0004, u, v), (0.0008, c - 0.0025, c - 0.0025)
                elif face == 1: loc, sz = pt(-s / 2 - 0.0004, u, v), (0.0008, c - 0.0025, c - 0.0025)
                elif face == 2: loc, sz = pt(u, s / 2 + 0.0004, v), (c - 0.0025, 0.0008, c - 0.0025)
                elif face == 3: loc, sz = pt(u, -s / 2 - 0.0004, v), (c - 0.0025, 0.0008, c - 0.0025)
                elif face == 4: loc, sz = pt(u, v, s / 2 + 0.0004), (c - 0.0025, c - 0.0025, 0.0008)
                else:           loc, sz = pt(u, v, -s / 2 - 0.0004), (c - 0.0025, c - 0.0025, 0.0008)
                if face >= 4:
                    loc = (loc[0], loc[1], (s if face == 4 else 0) + (0.0004 if face == 4 else -0.0004))
                    loc = ((u) * ca - v * sa, (u) * sa + v * ca, loc[2])
                P.rbox(n + "_Sticker", sz, loc, P.mat("Rubik_%d" % ((face + i + 2 * j) % 6), cols[(face * 2 + i + j) % 6], 0.4), root, rot=(0, 0, rot), r=0.0003, seg=1)
    finish(root, n)
    return root, (0.06, 0.06, 0.057)


def build_alarm_clock(n):
    root = P.empty(n)
    red = P.mat("Clock_Red", (0.6, 0.05, 0.04), 0.35, 0.5)
    face = P.mat("Clock_Face", (0.9, 0.88, 0.8), 0.5)
    brass = P.mat("Brass", (0.72, 0.55, 0.22), 0.28, 1.0)
    hand = P.mat("Clock_Hand", (0.02, 0.02, 0.02), 0.5)
    glass = P.mat("Clock_Glass", (0.8, 0.9, 0.9), 0.03, alpha=0.2)
    P.cylinder(n + "_Body", 0.05, 0.035, (0, 0, 0.065), red, root, rot=(RADS(90), 0, 0), seg=36, bevel=0.004)
    P.cylinder(n + "_Face", 0.042, 0.002, (0, -0.0185, 0.065), face, root, rot=(RADS(90), 0, 0), seg=36)
    for k in range(12):
        a = k * math.pi / 6
        P.rbox(n + "_Tick", (0.0025, 0.0008, 0.007 if k % 3 == 0 else 0.004), (math.sin(a) * 0.035, -0.0197, 0.065 + math.cos(a) * 0.035), hand, root, rot=(0, a, 0)) if False else None
        P.rbox(n + "_Tick", (0.0025, 0.0008, 0.007 if k % 3 == 0 else 0.004), (math.sin(a) * 0.034, -0.0197, 0.065 + math.cos(a) * 0.034), hand, root, rot=(0, -a, 0))
    P.rbox(n + "_HandH", (0.003, 0.001, 0.022), (0.006, -0.0205, 0.065 + 0.008), hand, root, rot=(0, RADS(-35), 0))
    P.rbox(n + "_HandM", (0.002, 0.001, 0.032), (-0.012, -0.0212, 0.065 + 0.013), hand, root, rot=(0, RADS(25), 0))
    P.cylinder(n + "_Glass", 0.043, 0.001, (0, -0.0205, 0.065), glass, root, rot=(RADS(90), 0, 0), seg=36)
    for sx in (-1, 1):
        P.blob(n + "_Bell", (0.021, 0.009, 0.021), (sx * 0.028, 0.0, 0.113), brass, root, seg=16, rings=8)
        P.cylinder(n + "_Leg", 0.004, 0.026, (sx * 0.03, 0.0, 0.0125), brass, root, rot=(0, RADS(sx * 25), 0), seg=8)
    P.cylinder(n + "_Hammer", 0.003, 0.03, (0, 0, 0.113), brass, root, seg=8)
    P.cylinder(n + "_Knob", 0.005, 0.01, (0.0, 0.025, 0.04), brass, root, rot=(RADS(90), 0, 0), seg=10)
    finish(root, n)
    return root, (0.11, 0.05, 0.125)


def build_cd_stack(n):
    root = P.empty(n)
    jewel = P.mat("CD_Case", (0.75, 0.82, 0.85), 0.08, alpha=0.5, trans=1.0)
    black_ = P.mat("CD_Tray", (0.03, 0.03, 0.035), 0.5)
    disc = P.mat("CD_Disc", (0.7, 0.72, 0.78), 0.12, 1.0)
    for k in range(4):
        z = 0.005 + k * 0.0104
        P.rbox(n + "_Case", (0.142, 0.125, 0.0102), (0, 0, z), jewel, root, rot=(0, 0, RADS(5 * (k - 1))), r=0.001, seg=1)
        P.rbox(n + "_Tray", (0.12, 0.112, 0.003), (0, 0, z - 0.001), black_, root, rot=(0, 0, RADS(5 * (k - 1))))
        P.lathe(n + "_Disc", [(0.008, 0.0), (0.06, 0.0)], (0.0, 0.0, z + 0.001), disc, root, rot=(0, 0, 0), seg=40)
    finish(root, n)
    return root, (0.15, 0.13, 0.045)


# ------------------------------------------------------------ vending machine

def build_vending(n):
    root = P.empty(n)
    W, D, H = 0.95, 0.75, 1.85
    red = P.mat("Vending_Red", (0.5, 0.035, 0.03), 0.28, 0.3)
    dkred = P.mat("Vending_DarkRed", (0.3, 0.02, 0.02), 0.4, 0.3)
    glassm = P.mat("Vending_Glass", (0.1, 0.12, 0.13), 0.04, alpha=0.18, trans=1.0)
    inner = P.mat("Vending_Inner", (0.9, 0.9, 0.85), 0.6)
    shelf = P.mat("Vending_Shelf", (0.65, 0.65, 0.68), 0.35, 0.8)
    lit = P.mat("Vending_Light", (0.9, 0.9, 0.85), 0.5, emit=(1.0, 0.95, 0.8), strength=3.0)
    dark_ = dark()
    fy = -D / 2
    zb, zt = 0.05, H
    # shell: back, sides, solid chest below the window, header block above it, selection column on the right
    P.rbox(n + "_Back", (W, 0.03, H - 0.05), (0, D / 2 - 0.015, zb + (H - zb) / 2), red, root, r=0.006)
    for sx in (-1, 1):
        P.rbox(n + "_Side", (0.03, D, H - 0.05), (sx * (W / 2 - 0.015), 0, zb + (H - zb) / 2), red, root, r=0.01, seg=3)
    P.rbox(n + "_Chest", (W - 0.06, D - 0.03, 0.3), (0, -0.015, zb + 0.15), red, root, r=0.01, seg=3)
    P.rbox(n + "_Header", (W - 0.06, D - 0.03, 0.23), (0, -0.015, H - 0.115), red, root, r=0.01, seg=3)
    P.rbox(n + "_ColumnBlock", (0.28, D - 0.03, 1.27), (0.35 + 0.0, -0.015, 0.355 + 0.635), red, root, r=0.008, seg=2)
    P.rbox(n + "_Plinth", (W - 0.06, D - 0.06, 0.05), (0, 0, 0.025), dark_, root, r=0.004)
    # window cavity (x -0.445..0.21): interior walls, glass, shelves with cans
    cx0, cx1 = -0.445, 0.21
    cxm, cw = (cx0 + cx1) / 2, cx1 - cx0
    P.rbox(n + "_Backwall", (cw, 0.01, 1.27), (cxm, D / 2 - 0.035, 0.99), inner, root)
    P.rbox(n + "_Ceiling", (cw, D - 0.06, 0.01), (cxm, 0.0, 1.63), inner, root)
    P.rbox(n + "_Floor", (cw, D - 0.06, 0.01), (cxm, 0.0, 0.355), shelf, root)
    P.rbox(n + "_Light", (cw - 0.1, 0.02, 0.012), (cxm, fy + 0.07, 1.62), lit, root)
    P.rbox(n + "_Glass", (cw, 0.006, 1.27), (cxm, fy + 0.008, 0.99), glassm, root, r=0.001)
    for sd, x in ((0, cx0), (1, cx1)):
        P.rbox(n + "_GlassFrame", (0.012, 0.02, 1.27), (x, fy + 0.008, 0.99), black(), root)
    colors = [((0.55, 0.04, 0.04), (0.95, 0.95, 0.95)), ((0.02, 0.18, 0.5), (0.9, 0.9, 0.95)), ((0.08, 0.4, 0.1), (0.9, 0.95, 0.4)), ((0.9, 0.55, 0.05), (0.15, 0.1, 0.05)), ((0.03, 0.03, 0.035), (0.2, 0.9, 0.4))]
    for s_ in range(5):
        z = 0.5 + s_ * 0.22
        P.rbox(n + "_Shelf", (cw - 0.02, D - 0.2, 0.008), (cxm, 0.02, z), shelf, root, r=0.002, seg=1)
        P.rbox(n + "_ShelfLip", (cw - 0.02, 0.008, 0.02), (cxm, fy + 0.1, z + 0.01), shelf, root, r=0.002, seg=1)
        # the shelves are stocked in Unreal (AVendingMachine::StockShelves) with the very bottles the player buys
    # header sign (lit) and side panels
    P.rbox(n + "_HeaderSign", (0.84, 0.03, 0.2), (0.0, fy - 0.002, 1.72), lit, root, r=0.004)
    P.text_mesh(n + "_HeaderText", "НАПИТКИ", 0.075, (0.0, fy - 0.019, 1.72), P.mat("Vending_Text", (0.55, 0.03, 0.03), 0.5), root, rot=(RADS(90), 0, 0), extrude=0.002)
    # selection column: coin slot, display, buttons, change tray
    cx = 0.35
    P.rbox(n + "_Column", (0.2, 0.012, 1.2), (cx, fy - 0.002, 1.02), dkred, root, r=0.003)
    P.rbox(n + "_Display", (0.12, 0.006, 0.04), (cx, fy - 0.011, 1.5), P.mat("Vending_LED", (0.02, 0.02, 0.02), 0.3, emit=(1.0, 0.2, 0.1), strength=4.0), root, r=0.002)
    P.rbox(n + "_CoinSlot", (0.012, 0.006, 0.06), (cx, fy - 0.011, 1.32), black(), root, r=0.002)
    P.rbox(n + "_CoinPlate", (0.08, 0.004, 0.11), (cx, fy - 0.009, 1.32), shelf, root, r=0.003)
    for r_ in range(4):
        for c_ in range(3):
            P.rbox(n + "_Button", (0.034, 0.008, 0.026), (cx - 0.045 + c_ * 0.045, fy - 0.012, 1.13 - r_ * 0.042), P.mat("Vending_Btn", (0.85, 0.85, 0.82), 0.4), root, r=0.004)
    P.rbox(n + "_ChangeSlot", (0.05, 0.006, 0.03), (cx, fy - 0.011, 0.85), black(), root, r=0.003)
    # delivery flap
    P.rbox(n + "_Flap", (0.62, 0.03, 0.2), (-0.1, fy + 0.0, 0.22), black(), root, r=0.004)
    P.rbox(n + "_FlapDoor", (0.5, 0.012, 0.12), (-0.1, fy - 0.014, 0.22), P.mat("Vending_Flap", (0.1, 0.1, 0.11), 0.4), root, rot=(RADS(-6), 0, 0), r=0.004)
    # white swoosh stripe + base decal
    P.rbox(n + "_Stripe", (W - 0.02, 0.003, 0.06), (0.0, fy - 0.002, 0.28), P.mat("Vending_Stripe", (0.9, 0.9, 0.9), 0.4), root, r=0.001) if False else None
    P.rbox(n + "_Stripe", (0.2, 0.003, 0.2), (cx, fy - 0.002, 0.28), P.mat("Vending_Stripe", (0.9, 0.9, 0.9), 0.4), root, r=0.001) if False else None
    for sx in (-1, 1):
        P.rbox(n + "_Foot", (0.05, 0.05, 0.012), (sx * 0.4, -0.3, 0.006), rubber(), root)
        P.rbox(n + "_Foot", (0.05, 0.05, 0.012), (sx * 0.4, 0.3, 0.006), rubber(), root)
    finish(root, n)
    return root, (W, D, H)


BUILDERS = {
    "cup_cola": lambda n: build_cup(n),
    "cup_cola_lying": lambda n: build_cup(n, True),
    "can_cola": lambda n: build_can(n, P.mat("Can_Red", (0.55, 0.03, 0.04), 0.3, 0.6), P.mat("Can_White", (0.95, 0.95, 0.95), 0.4)),
    "can_energy": lambda n: build_can(n, P.mat("Can_Black", (0.03, 0.03, 0.035), 0.3, 0.6), P.mat("Can_Neon", (0.2, 0.9, 0.35), 0.4)),
    "can_lemon": lambda n: build_can(n, P.mat("Can_Green", (0.08, 0.4, 0.1), 0.3, 0.6), P.mat("Can_Yellow", (0.9, 0.9, 0.3), 0.4)),
    "bottle_cola": lambda n: build_bottle(n, (0.06, 0.02, 0.01), P.mat("Label_Red", (0.6, 0.04, 0.04), 0.5), P.mat("Cap_Red", (0.6, 0.04, 0.04), 0.4)),
    "bottle_water": lambda n: build_bottle(n, (0.7, 0.85, 0.9), P.mat("Label_Blue", (0.1, 0.35, 0.7), 0.5), P.mat("Cap_Blue", (0.1, 0.35, 0.7), 0.4)),
    "food_bag": build_food_bag,
    "burger_box_closed": lambda n: build_burger_box(n, False),
    "burger_box_open": lambda n: build_burger_box(n, True),
    "fries": build_fries,
    "fan_desk": build_fan,
    "lamp_soviet": build_lamp_soviet,
    "rose_vase": build_rose,
    "lamp_grow": build_grow_lamp,
    "plant_cactus": lambda n: build_plant(n, True),
    "plant_leafy": lambda n: build_plant(n, False),
    "books_2": lambda n: build_books(n, 2),
    "books_3": lambda n: build_books(n, 3),
    "books_4": lambda n: build_books(n, 4),
    "tea_glass": build_tea,
    "rubik_cube": build_rubik,
    "alarm_clock": build_alarm_clock,
    "cd_stack": build_cd_stack,
    "vending_machine": build_vending,
}


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
            P.setup_studio(scn, (700, 700))
            m = max(size)
            P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0, size[2] * 0.45), (m * 1.6, -m * 2.2, size[2] * 0.9 + m * 0.3), 50)


if __name__ == "__main__":
    main()
