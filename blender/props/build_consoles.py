"""Detailed game consoles and gamepads: 16-bit console, PS2-style, PS5-style, Dendy/Famicom clone and pads.

  blender -b -P blender/props/build_consoles.py -- [name ...] [--no-render]

Frame as in build_computers.py: origin on the table under the device, front (ports, buttons, discs) toward -Y.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
from mathutils import Matrix, Vector

import propslib as P
from build_computers import black, dark, finish, hull, rubber, RADS

LED = P.led


def prism_x(name, outline, x0, x1, material, parent, r=0.0, tile=0.3):
    """Extrude a side outline [(y, z), ...] along X from x0 to x1."""
    bm = bmesh.new()
    vs0 = [bm.verts.new((x0, y, z)) for y, z in outline]
    f = bm.faces.new(vs0)
    res = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [v for v in res["geom"] if isinstance(v, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=top, vec=(x1 - x0, 0, 0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if r > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=r, segments=3, profile=0.5, affect="EDGES")
    return P._make(name, bm, material, parent, tile, 40)


def pad_buttons(n, root, cx, cy, z, size, mat_, spacing, labels_mat=None):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        P.cylinder(n + "_Btn", size, 0.004, (cx + dx * spacing, cy + dy * spacing, z), mat_, root, seg=14, bevel=0.0006)


def dpad(n, root, cx, cy, z, s, mat_):
    P.rbox(n + "_DPadH", (s, s * 0.34, 0.004), (cx, cy, z), mat_, root, r=0.001)
    P.rbox(n + "_DPadV", (s * 0.34, s, 0.004), (cx, cy, z), mat_, root, r=0.001)


# ------------------------------------------------------------ 16-bit console

def build_megadrive(n):
    root = P.empty(n)
    shell = P.mat("MD_Black", (0.018, 0.018, 0.02), 0.4)
    W, D, H = 0.285, 0.215, 0.05
    P.rbox(n + "_Base", (W, D, H), (0, 0, H / 2), shell, root, r=0.008, seg=4)
    P.rbox(n + "_Top", (W - 0.03, D - 0.03, 0.01), (0, 0.0, H + 0.003), shell, root, r=0.006, seg=3)
    # big round ring on the left, glossy inner disc with a small raised label
    P.lathe(n + "_Ring", [(0.065, 0.0), (0.07, 0.003), (0.07, 0.008), (0.062, 0.009)], (-0.06, -0.015, H + 0.008), shell, root, seg=48)
    P.cylinder(n + "_RingDisc", 0.062, 0.002, (-0.06, -0.015, H + 0.011), P.mat("MD_Disc", (0.03, 0.03, 0.035), 0.15), root, seg=48)
    P.text_mesh(n + "_RingText", "16-BIT", 0.011, (-0.06, -0.015, H + 0.0125), P.mat("MD_Text", (0.8, 0.8, 0.82), 0.4), root, extrude=0.0004)
    # cartridge slot with a cartridge in it
    P.rbox(n + "_Slot", (0.1, 0.03, 0.002), (0.065, 0.05, H + 0.0085), P.mat("MD_SlotDark", (0.005, 0.005, 0.006), 0.6), root)
    cart = P.mat("MD_Cart", (0.07, 0.07, 0.075), 0.5)
    P.rbox(n + "_Cart", (0.095, 0.014, 0.065), (0.065, 0.05, H + 0.037), cart, root, r=0.002)
    P.rbox(n + "_CartLabel", (0.07, 0.001, 0.04), (0.065, 0.0425, H + 0.043), P.mat("MD_Label", (0.45, 0.06, 0.06), 0.7), root)
    # switches and buttons
    P.rbox(n + "_Power", (0.03, 0.012, 0.007), (-0.1, 0.075, H + 0.008), dark(), root, r=0.002)
    P.rbox(n + "_Volume", (0.02, 0.008, 0.005), (0.12, -0.08, H + 0.008), dark(), root, r=0.002)
    P.rbox(n + "_Reset", (0.014, 0.01, 0.005), (0.09, 0.0, H + 0.008), P.mat("MD_Btn", (0.12, 0.12, 0.13), 0.4), root, r=0.002)
    P.rbox(n + "_LED", (0.005, 0.002, 0.004), (-0.12, -D / 2 - 0.0005, 0.035), LED("Red", (1.0, 0.1, 0.05), 4.0), root)
    # two DE-9 style controller ports on the front
    for sx in (-1, 1):
        hull(n + "_Port", [(sx * 0.03 + dx, -D / 2 - 0.004, 0.012 + dz) for dx, dz in ((-0.0125, 0), (0.0125, 0), (-0.01, 0.01), (0.01, 0.01))] + [(sx * 0.03 + dx, -D / 2 + 0.004, 0.012 + dz) for dx, dz in ((-0.0125, 0), (0.0125, 0), (-0.01, 0.01), (0.01, 0.01))], dark(), root, r=0.001, seg=2)
    # vents on the back, expansion edge
    for i in range(10):
        P.rbox(n + "_Vent", (0.002, 0.04, 0.001), (-0.0 + 0.1 - i * 0.008, 0.0, 0.0) if False else (0.03 + i * 0.008 - 0.0, 0.0, 0.0), dark(), root) if False else None
    for i in range(9):
        P.rbox(n + "_Rib", (0.002, 0.07, 0.0015), (0.1 + (i - 4) * 0.007 + 0.0, 0.025, H + 0.0085), dark(), root) if False else None
    for sx in (-1, 1):
        P.rbox(n + "_Foot", (0.025, 0.02, 0.004), (sx * 0.11, 0.07, 0.0), rubber(), root, r=0.001)
        P.rbox(n + "_Foot", (0.025, 0.02, 0.004), (sx * 0.11, -0.07, 0.0), rubber(), root, r=0.001)
    finish(root, n)
    return root, (W, D, H + 0.07)


def build_pad_mega(n):
    root = P.empty(n)
    shell = P.mat("MD_Black", (0.018, 0.018, 0.02), 0.4)
    hull(n + "_Body", [(sx * 0.08 * (1 - 0.1 * (abs(sy))), sy * 0.04, z) for sx in (-1, 1) for sy in (-1, 1) for z in (0.0, 0.02)] + [(sx * 0.07, sy * 0.05, 0.007) for sx in (-1, 1) for sy in (-1, 1)], shell, root, r=0.012, seg=4)
    dpad(n, root, -0.045, 0.0, 0.0215, 0.032, dark())
    for i in range(3):
        P.cylinder(n + "_Btn", 0.0085, 0.004, (0.026 + i * 0.019, -0.008 + i * 0.006 - 0.0, 0.0215), P.mat("MD_ButtonGrey", (0.14, 0.14, 0.15), 0.35), root, seg=14, bevel=0.0007)
    P.rbox(n + "_Start", (0.016, 0.005, 0.003), (0.005, 0.016, 0.0215), dark(), root, rot=(0, 0, RADS(-20)), r=0.001)
    P.tube(n + "_Cable", [(0, 0.04, 0.008), (0, 0.08, 0.003), (-0.04, 0.14, 0.003), (0.0, 0.25, 0.003)], 0.0018, P.mat("Cable", (0.02, 0.02, 0.02), 0.6), root, seg=6)
    finish(root, n)
    return root, (0.17, 0.09, 0.022)


# ------------------------------------------------------------ PS2

def build_ps2(n):
    root = P.empty(n)
    sh = P.mat("PS2_Black", (0.018, 0.018, 0.02), 0.5)
    W, D, H = 0.301, 0.182, 0.078
    P.rbox(n + "_Body", (W, D, H), (0, 0, H / 2), sh, root, r=0.006, seg=4)
    # ribbed top vent field and slightly raised rear
    for i in range(18):
        P.rbox(n + "_Rib", (0.2, 0.003, 0.0015), (-0.02, 0.0 - 0.0 + 0.012 + i * 0.0075 - 0.065, H + 0.0004), P.mat("PS2_Rib", (0.008, 0.008, 0.01), 0.7), root) if False else None
    for i in range(16):
        P.rbox(n + "_Rib", (0.18, 0.0025, 0.0012), (-0.03, -0.01 + i * 0.0068 - 0.0 - 0.0, H + 0.0004), P.mat("PS2_Rib", (0.008, 0.008, 0.01), 0.7), root) if False else None
    for i in range(11):
        P.rbox(n + "_Rib", (0.19, 0.0028, 0.0013), (0.0, -0.02 + i * 0.0085, H + 0.0004), P.mat("PS2_Rib", (0.008, 0.008, 0.01), 0.7), root)
    # front: disc tray with slot, eject, LED, memory-card and controller ports
    P.rbox(n + "_Tray", (0.2, 0.004, 0.012), (-0.045, -D / 2 - 0.0005, H * 0.67), P.mat("PS2_TrayDark", (0.005, 0.005, 0.007), 0.5), root)
    P.rbox(n + "_TrayLip", (0.2, 0.003, 0.002), (-0.045, -D / 2 - 0.002, H * 0.67 - 0.008), P.mat("PS2_Lip", (0.12, 0.12, 0.13), 0.4), root)
    P.rbox(n + "_Eject", (0.012, 0.004, 0.012), (0.075, -D / 2 - 0.002, H * 0.62), P.mat("PS2_Btn", (0.1, 0.1, 0.11), 0.4), root, r=0.002)
    P.rbox(n + "_LED", (0.005, 0.002, 0.004), (0.05, -D / 2 - 0.001, H * 0.6), LED("PS2Blue", (0.1, 0.35, 1.0), 6.0), root)
    P.rbox(n + "_PowerBtn", (0.02, 0.005, 0.01), (0.115, -D / 2 - 0.002, H * 0.65), P.mat("PS2_Btn", (0.1, 0.1, 0.11), 0.4), root, r=0.002)
    P.rbox(n + "_ResetBtn", (0.01, 0.005, 0.008), (0.13, -D / 2 - 0.002, H * 0.65), P.mat("PS2_Btn", (0.1, 0.1, 0.11), 0.4), root, r=0.002)
    for i in range(2):                                       # memory card slots
        P.rbox(n + "_MemSlot", (0.026, 0.004, 0.012), (-0.123 + i * 0.03, -D / 2 - 0.0005, H * 0.25), P.mat("PS2_MemSlot", (0.2, 0.2, 0.25), 0.5), root, r=0.002)
    for i in range(2):                                       # controller ports
        P.rbox(n + "_Port", (0.02, 0.004, 0.012), (0.095 + i * 0.03, -D / 2 - 0.0005, H * 0.25), dark(), root, r=0.002)
    for i in range(2):                                       # USB
        P.rbox(n + "_USB", (0.012, 0.004, 0.006), (0.032 + i * 0.016, -D / 2 - 0.0005, H * 0.25), dark(), root, r=0.001)
    P.rbox(n + "_Stripe", (W - 0.01, 0.002, 0.002), (0, -D / 2 - 0.0005, H * 0.45), P.mat("PS2_Stripe", (0.07, 0.07, 0.08), 0.4), root)
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.rbox(n + "_Foot", (0.025, 0.02, 0.005), (sx * 0.12, sy * 0.065, 0.0), rubber(), root, r=0.001)
    finish(root, n)
    return root, (W, D, H)


# ------------------------------------------------------------ PS5

def build_ps5(n):
    root = P.empty(n)
    wh = P.mat("PS5_White", (0.82, 0.83, 0.84), 0.3)
    bk = P.mat("PS5_Black", (0.015, 0.015, 0.02), 0.3)
    H, D = 0.39, 0.26
    # side silhouette (y, z): a tall shape, slanted bottom/top ends, gently pinched
    def outline(inset=0.0, wave=0.0):
        pts = []
        N = 24
        for i in range(N + 1):             # front edge going up
            t = i / N
            z = 0.04 + t * (H - 0.04)
            y = -D / 2 + inset + 0.03 * math.sin(math.pi * t) * 0.0 + wave * math.sin(math.pi * t * 2) * 0.0
            pts.append((y + 0.045 * (1 - math.sin(math.pi * t)) * (1 - t) * 0, z))
        for i in range(N + 1):             # back edge going down, with the characteristic swoop
            t = 1 - i / N
            z = 0.04 + t * (H - 0.04)
            y = D / 2 - inset - 0.10 * (1 - t) ** 2 - 0.035 * (1 - math.sin(math.pi * t)) * 0
            pts.append((y, z))
        return pts
    for sx in (-1, 1):
        prism_x(n + "_Shell", outline(), sx * 0.0225, sx * 0.0585, wh, root, r=0.006)
    prism_x(n + "_Core", outline(inset=0.012), -0.0225, 0.0225, bk, root, r=0.003)
    # glowing seam
    prism_x(n + "_Glow", outline(inset=0.002)[:25], -0.0235, 0.0235, LED("PS5Blue", (0.25, 0.5, 1.0), 6.0), root, r=0.0) if False else None
    P.rbox(n + "_Seam", (0.052, 0.002, H * 0.8), (0, -D / 2 + 0.0035, 0.04 + H * 0.5), LED("PS5Blue", (0.2, 0.45, 1.0), 3.0), root) if False else None
    # base stand
    P.cylinder(n + "_Stand", 0.055, 0.02, (0, -0.02, 0.012), wh, root, seg=32, r2=0.07)
    P.rbox(n + "_StandBar", (0.058, 0.2, 0.03), (0, 0.0, 0.03), wh, root, r=0.006)
    P.rbox(n + "_DiscSlot", (0.002, 0.07, 0.004), (0.0, -D / 2 + 0.05 + 0.04, H * 0.88), dark(), root) if False else None
    # front: USB-C, power buttons, disc slot on the side
    P.rbox(n + "_Power", (0.012, 0.004, 0.006), (0.0, -D / 2 + 0.011, H * 0.18), LED("PS5Pwr", (0.9, 0.9, 1.0), 2.0), root, r=0.001)
    P.rbox(n + "_Slot", (0.0015, 0.1, 0.004), (0.0585, -0.0, H * 0.7), dark(), root)
    for i in range(30):          # vent dots on the back
        P.rbox(n + "_Dot", (0.045, 0.0015, 0.0015), (0, D / 2 - 0.0 - 0.098 + 0.0, 0.1 + (i % 15) * 0.015), dark(), root) if False else None
    finish(root, n)
    return root, (0.117, D, H + 0.04)


# ------------------------------------------------------------ Dendy / Famicom clone

def build_dendy(n):
    root = P.empty(n)
    lg = P.mat("Dendy_Light", (0.68, 0.68, 0.65), 0.45)
    dg = P.mat("Dendy_Dark", (0.16, 0.16, 0.17), 0.5)
    rd = P.mat("Dendy_Red", (0.5, 0.05, 0.04), 0.4)
    W, D, H = 0.27, 0.19, 0.065
    P.rbox(n + "_Base", (W, D, H), (0, 0, H / 2), lg, root, r=0.006, seg=3)
    P.rbox(n + "_Deck", (W - 0.02, D - 0.07, 0.006), (0, 0.025, H + 0.002), dg, root, r=0.003)
    # cartridge slot at the front-top (slanted opening) with a cartridge
    hull(n + "_SlotHood", [(sx * 0.07, -D / 2 + 0.02 + y, H - 0.006 + z) for sx in (-1, 1) for y, z in ((0, 0), (0.05, 0), (0.05, 0.012), (0.0, 0.004))], dg, root, r=0.001)
    P.rbox(n + "_SlotDark", (0.13, 0.002, 0.008), (0, -D / 2 + 0.0105, H - 0.012), P.mat("Dendy_Slot", (0.005, 0.005, 0.006), 0.6), root)
    cart = P.mat("Dendy_Cart", (0.45, 0.45, 0.43), 0.5)
    P.rbox(n + "_Cart", (0.12, 0.012, 0.058), (0.0, 0.0, H + 0.032), cart, root, rot=(RADS(-8), 0, 0), r=0.002)
    P.rbox(n + "_CartLabel", (0.09, 0.001, 0.04), (0.0, -0.0065, H + 0.036), P.mat("Dendy_Label", (0.9, 0.8, 0.2), 0.7), root, rot=(RADS(-8), 0, 0))
    # buttons and LED
    P.rbox(n + "_Power", (0.025, 0.014, 0.008), (-0.095, 0.045, H + 0.007), rd, root, r=0.002)
    P.rbox(n + "_Reset", (0.025, 0.014, 0.008), (-0.095, 0.015, H + 0.007), dg, root, r=0.002)
    P.rbox(n + "_LED", (0.006, 0.002, 0.004), (-0.065, -D / 2 - 0.0005, 0.045), LED("Red", (1.0, 0.1, 0.05), 4.0), root)
    for i in range(9):
        P.rbox(n + "_Vent", (0.0018, 0.05, 0.001), (0.06 + i * 0.0055, 0.04, H + 0.0055), dg, root) if False else None
    for sx in (-1, 1):                                  # two pad ports on the front
        P.rbox(n + "_Port", (0.022, 0.004, 0.012), (sx * 0.075, -D / 2 - 0.0005, 0.03), dark(), root, r=0.002)
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.rbox(n + "_Foot", (0.025, 0.02, 0.005), (sx * 0.11, sy * 0.07, 0.0), rubber(), root, r=0.001)
    finish(root, n)
    return root, (W, D, H + 0.08)


def build_pad_dendy(n):
    root = P.empty(n)
    lg = P.mat("Dendy_Light", (0.68, 0.68, 0.65), 0.45)
    P.rbox(n + "_Body", (0.15, 0.058, 0.019), (0, 0, 0.0095), lg, root, r=0.004)
    P.rbox(n + "_Face", (0.138, 0.046, 0.002), (0, 0, 0.0195), P.mat("Dendy_PadFace", (0.05, 0.05, 0.055), 0.45), root, r=0.002)
    dpad(n, root, -0.048, 0.0, 0.0215, 0.027, P.mat("Dendy_PadBtn", (0.12, 0.12, 0.13), 0.4))
    for i in range(2):
        P.cylinder(n + "_AB", 0.0085, 0.004, (0.036 + i * 0.026, -0.008 + i * 0.0 + (0.004 if i else 0), 0.0215), P.mat("Dendy_Red", (0.5, 0.05, 0.04), 0.4), root, seg=14, bevel=0.0007)
    for i in range(2):
        P.rbox(n + "_SS", (0.016, 0.006, 0.003), (-0.008 + i * 0.022, -0.012, 0.0215), P.mat("Dendy_PadBtn", (0.12, 0.12, 0.13), 0.4), root, rot=(0, 0, RADS(-15)), r=0.001)
    P.tube(n + "_Cable", [(0, 0.03, 0.01), (0, 0.08, 0.003), (-0.04, 0.14, 0.003), (0.0, 0.25, 0.003)], 0.0018, P.mat("Cable", (0.02, 0.02, 0.02), 0.6), root, seg=6)
    finish(root, n)
    return root, (0.15, 0.058, 0.021)


# ------------------------------------------------------------ DualShock / DualSense

def pad_modern(n, white):
    root = P.empty(n)
    body = P.mat("DS_White", (0.85, 0.86, 0.87), 0.35) if white else P.mat("DS_Black", (0.025, 0.025, 0.03), 0.45)
    acc = P.mat("DS_AccentBlack", (0.015, 0.015, 0.02), 0.4) if white else P.mat("DS_AccentGrey", (0.07, 0.07, 0.08), 0.4)
    W = 0.16
    # central body + two angled grips
    hull(n + "_Body", [(sx * 0.05, sy * 0.028, z) for sx in (-1, 1) for sy in (-1, 1) for z in (0.0, 0.026)], body, root, r=0.012, seg=4)
    for sx in (-1, 1):
        P.tube(n + "_Grip", [(sx * 0.045, 0.0, 0.0135), (sx * 0.068, -0.022, 0.0135), (sx * 0.078, -0.05, 0.0135)], 0.0132, body, root, seg=14, cap=False)
        P.blob(n + "_GripEnd", (0.0132, 0.0132, 0.0132), (sx * 0.078, -0.05, 0.0135), body, root, seg=14, rings=8)
    # shoulder bumpers
    for sx in (-1, 1):
        P.rbox(n + "_Bumper", (0.04, 0.012, 0.012), (sx * 0.058, 0.03, 0.022), body, root, r=0.004)
    # touchpad / lightbar
    P.rbox(n + "_Touchpad", (0.06, 0.03, 0.002), (0.0, 0.0, 0.0265), acc if not white else P.mat("DS_Pad", (0.8, 0.8, 0.82), 0.4), root, r=0.002)
    P.rbox(n + "_Light", (0.05, 0.0025, 0.0015), (0, 0.0305, 0.0262), LED("DSBlue", (0.15, 0.3, 1.0), 6.0), root)
    # d-pad, face buttons, sticks
    dpad(n, root, -0.045, 0.012, 0.027, 0.028, acc)
    cols = [(0.15, 0.55, 0.8), (0.8, 0.2, 0.25), (0.8, 0.45, 0.65), (0.3, 0.7, 0.35)]
    for i, (dx, dy) in enumerate(((0, 1), (1, 0), (-1, 0), (0, -1))):
        P.cylinder(n + "_Face", 0.0075, 0.004, (0.045 + dx * 0.012, 0.012 + dy * 0.012, 0.0275), acc if white else P.mat("DS_B%d" % i, cols[i], 0.35), root, seg=14, bevel=0.0006)
    for sx in (-1, 1):
        P.cylinder(n + "_Stick", 0.0095, 0.012, (sx * 0.028, -0.014, 0.0285), acc, root, seg=18, bevel=0.0008)
        P.cylinder(n + "_StickTop", 0.0105, 0.004, (sx * 0.028, -0.014, 0.0355), P.mat("DS_Rubber", (0.01, 0.01, 0.012), 0.8), root, seg=18)
    P.rbox(n + "_Logo", (0.012, 0.002, 0.002), (0, -0.015, 0.0268), acc, root)
    finish(root, n)
    return root, (0.16, 0.1, 0.04)


BUILDERS = {
    "console_megadrive": build_megadrive,
    "pad_mega": build_pad_mega,
    "console_ps2": build_ps2,
    "console_ps5": build_ps5,
    "console_dendy": build_dendy,
    "pad_dendy": build_pad_dendy,
    "pad_dualshock": lambda n: pad_modern(n, False),
    "pad_dualsense": lambda n: pad_modern(n, True),
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
            P.setup_studio(scn, (800, 700))
            hh = size[2]
            dist = max(size[0], size[1]) * 2.4 + 0.15
            P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0, hh * 0.4), (dist * 0.5, -dist, hh * 1.4 + 0.1), 50)


if __name__ == "__main__":
    main()
