"""Detailed computers and monitors: classic compact Mac, iMac G3, CRT monitors/TVs, beige desktop and towers, laptops,
flat monitors, mice.

  blender -b -P blender/props/build_computers.py -- [name ...] [--no-render]

Frame: origin on the table top under the device, screen faces local -Y (the chair), +X right, +Z up.
Screens are separate meshes named <prop>_Screen with the material Screen_Off, UV 0..1 (U left to right, V top to bottom
as an image) so the scene can swap in its Screen_On_* emissive material.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
from mathutils import Matrix, Vector

import propslib as P

RADS = math.radians


# ------------------------------------------------------------ materials

def beige():
    return P.tex("PC_Beige", "plastic_beige.png", 0.5, size=512, tint=(0.66, 0.67, 0.62), fallback=(0.72, 0.66, 0.53), tilemult=4.0)


def greyplastic():
    return P.mat("PC_GreyPlastic", (0.42, 0.42, 0.40), 0.5)


def black():
    return P.mat("PC_Black", (0.02, 0.02, 0.022), 0.4)


def dark():
    return P.mat("PC_DarkGrey", (0.09, 0.09, 0.1), 0.55)


def screen_off():
    return P.mat("Screen_Off", (0.008, 0.01, 0.012), 0.06)


def metal():
    return P.mat("PC_Metal", (0.62, 0.62, 0.64), 0.28, 1.0)


def rubber():
    return P.mat("PC_Rubber", (0.015, 0.015, 0.015), 0.85)


def tile(mat_):
    return 0.3


# ------------------------------------------------------------ building blocks

def hull(name, pts, material, parent, r=0.0, seg=3, smooth=45, tile=0.4):
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    bmesh.ops.dissolve_limit(bm, angle_limit=0.002, verts=list(bm.verts), edges=list(bm.edges))
    if r > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=r, segments=seg, profile=0.5, affect="EDGES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return P._make(name, bm, material, parent, tile, smooth)


def rect_pts(w, h, y, zc=0.0, xc=0.0):
    return [(xc + sx * w / 2, y, zc + sz * h / 2) for sx in (-1, 1) for sz in (-1, 1)]


def curved_screen(name, w, h, loc, parent, bulge=0.006, nx=14, nz=10, rot=(0, 0, 0)):
    """Convex CRT glass facing -Y; UV 0..1."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.verify()
    grid = []
    for j in range(nz + 1):
        row = []
        for i in range(nx + 1):
            u, v = i / nx, j / nz
            x, z = (u - 0.5) * w, (0.5 - v) * h
            y = -bulge * (1 - (2 * u - 1) ** 2) * (1 - (2 * v - 1) ** 2) * 0.5 if bulge else 0
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for j in range(nz):
        for i in range(nx):
            f = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            for l, (a, b) in zip(f.loops, [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]):
                l[uv].uv = (a / nx, 1 - (1 - b / nz))
    # make normals point -Y
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bm.faces[0].normal.y > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    for f in bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(screen_off())
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    ob.parent = parent
    return P.place(ob, loc, rot)


def vent_slots(prefix, root, material, x0, x1, y0, y1, z, n, w=0.0025, along="y", rot=(0, 0, 0)):
    """Row of thin raised slats."""
    for i in range(n):
        t = i / max(n - 1, 1)
        if along == "y":
            P.rbox(prefix, (x1 - x0, w, 0.001), ((x0 + x1) / 2, y0 + (y1 - y0) * t, z), material, root, rot=rot)
        else:
            P.rbox(prefix, (w, y1 - y0, 0.001), (x0 + (x1 - x0) * t, (y0 + y1) / 2, z), material, root, rot=rot)


def finish(root, name):
    P.merge_by_material(root, keep=("Screen",), name=name)


# ------------------------------------------------------------ CRT monitor / TV

def crt_monitor(name, w, h, d, shell, bezel=0.03, screen_ratio=(0.8, 0.78), z0=0.0, base=True, tv=False,
                screen_name=None):
    """CRT with a tapering tube housing. w,h = front size, d = full depth. Origin at the table under the middle."""
    root = P.empty(name)
    sw, sh = w * screen_ratio[0], h * screen_ratio[1]
    front_d = d * 0.28
    # front bezel shell
    P.rbox(name + "_Front", (w, front_d, h), (0, -d / 2 + front_d / 2, z0 + h / 2 + (0.035 if base else 0)), shell, root,
           r=0.012, seg=4)
    zc = z0 + h / 2 + (0.035 if base else 0)
    # tapering tube housing
    back = [(sx * w * 0.34, d / 2, zc + sz * h * 0.33) for sx in (-1, 1) for sz in (-1, 1)]
    mid = [(sx * w * 0.47, -d / 2 + front_d, zc + sz * h * 0.47) for sx in (-1, 1) for sz in (-1, 1)]
    hull(name + "_Housing", mid + back, shell, root, r=0.012, seg=3)
    # neck at the back
    P.cylinder(name + "_Neck", h * 0.1, d * 0.12, (0, d / 2 + d * 0.04, zc), dark(), root, rot=(RADS(90), 0, 0), seg=16)
    # glass bezel (dark recessed frame) and screen
    P.rbox(name + "_ScreenFrame", (sw + 0.012, 0.004, sh + 0.012), (0, -d / 2 - 0.001, zc + h * 0.04), black(), root, r=0.003)
    curved_screen(name + "_Screen", sw, sh, (0, -d / 2 - 0.004, zc + h * 0.04), root, bulge=0.012)
    # vent slots on top of the housing
    n = 12
    for i in range(n):
        y = -d / 2 + front_d + 0.02 + i * (d * 0.45 / n)
        P.rbox(name + "_Vent", (w * 0.4, 0.004, 0.0015), (0, y, zc + h * 0.47 - 0.0005 - (y + d / 2 - front_d) * 0.19),
               black(), root)
    # controls
    ctrl_z = z0 + (0.035 if base else 0) + h * 0.06
    for i in range(3):
        P.cylinder(name + "_Knob", 0.007, 0.01, (w * 0.28 + i * 0.022, -d / 2 - 0.004, ctrl_z), dark(), root,
                   rot=(RADS(90), 0, 0), seg=14)
    P.rbox(name + "_Power", (0.014, 0.008, 0.01), (w * 0.28 - 0.04, -d / 2 - 0.004, ctrl_z), dark(), root, r=0.002)
    P.rbox(name + "_LED", (0.004, 0.002, 0.004), (w * 0.28 - 0.065, -d / 2 - 0.003, ctrl_z), P.led("RedLED", (0.9, 0.12, 0.05)), root)
    if base:                                           # swivel base
        P.cylinder(name + "_Base", min(w, d) * 0.28, 0.03, (0, 0.0, 0.015), shell, root, seg=32, r2=min(w, d) * 0.25)
        P.cylinder(name + "_Pivot", min(w, d) * 0.19, 0.012, (0, 0.0, 0.034), shell, root, seg=24)
    P.rbox(name + "_Feet", (0.03, 0.03, 0.002), (0.0, 0.0, 0.001), rubber(), root) if False else None
    finish(root, name)
    return root, (w, d, h + (0.035 if base else 0))


def build_crt_beige14(n):
    return crt_monitor(n, 0.37, 0.34, 0.38, beige())


def build_crt_grey17(n):
    return crt_monitor(n, 0.42, 0.40, 0.44, P.mat("PC_MonitorGrey", (0.5, 0.5, 0.5), 0.4))


def build_tv_silver(n):
    root, sz = crt_monitor(n, 0.62, 0.48, 0.48, P.mat("PC_TVSilver", (0.55, 0.55, 0.56), 0.3, 0.3), base=False)
    return root, sz


def build_tv_small(n):
    return crt_monitor(n, 0.36, 0.30, 0.34, P.mat("PC_TVDark", (0.12, 0.1, 0.09), 0.5), base=False)


def build_monitor_ibm(n):
    """9-ish inch greenscreen-style boxy monitor."""
    return crt_monitor(n, 0.32, 0.28, 0.34, beige(), screen_ratio=(0.78, 0.74))


# ------------------------------------------------------------ compact Mac

def build_mac_classic(n):
    root = P.empty(n)
    W, D, H = 0.244, 0.276, 0.344
    B = beige()
    # body: front slab with chin, tapering back
    front = [(sx * W / 2, -D / 2, sz) for sx in (-1, 1) for sz in (0.0, H)]
    mid = [(sx * W / 2, -D / 2 + 0.11, sz) for sx in (-1, 1) for sz in (0.0, H)]
    back = [(sx * 0.095, D / 2, sz) for sx in (-1, 1) for sz in (0.0, H * 0.84)]
    hull(n + "_Body", front + mid + back, B, root, r=0.008)
    # front: bezel and recessed screen, floppy slot
    P.rbox(n + "_ScreenRecess", (0.19, 0.006, 0.152), (0, -D / 2 - 0.001, H * 0.68), P.mat("Mac_Recess", (0.35, 0.33, 0.28), 0.6), root, r=0.012)
    P.rbox(n + "_ScreenFrame", (0.17, 0.004, 0.13), (0, -D / 2 - 0.004, H * 0.68), black(), root, r=0.012)
    curved_screen(n + "_Screen", 0.158, 0.118, (0, -D / 2 - 0.007, H * 0.68), root, bulge=0.004)
    P.rbox(n + "_FloppySlot", (0.056, 0.004, 0.004), (0.04, -D / 2 - 0.0025, 0.086), black(), root)
    P.rbox(n + "_FloppyFrame", (0.075, 0.003, 0.012), (0.04, -D / 2 - 0.0012, 0.086), P.mat("Mac_Slot", (0.3, 0.28, 0.24), 0.6), root, r=0.003)
    P.rbox(n + "_Eject", (0.008, 0.003, 0.008), (-0.01, -D / 2 - 0.001, 0.086), P.mat("Mac_Eject", (0.3, 0.28, 0.24), 0.6), root, r=0.002)
    P.rbox(n + "_Badge", (0.03, 0.002, 0.008), (-0.06, -D / 2 - 0.001, 0.052), dark(), root, r=0.001)
    P.rbox(n + "_LED", (0.004, 0.002, 0.004), (0.095, -D / 2 - 0.001, 0.052), P.led("Green", (0.1, 1.0, 0.2), 3.0), root)
    # handle recess on top, vent slots on the sides, back slots
    P.rbox(n + "_Handle", (0.08, 0.025, 0.004), (0, -0.02, H + 0.0005), dark(), root, r=0.002)
    for i in range(8):
        P.rbox(n + "_Vent", (0.002, 0.07, 0.001), (W / 2 - 0.0, 0.0, 0.25 - i * 0.012), black(), root) if False else None
    for sx in (-1, 1):
        for i in range(9):
            P.rbox(n + "_SideVent", (0.001, 0.07, 0.002), (sx * (0.11 + (0.0)), 0.07, 0.07 + i * 0.012), black(), root)
    # feet
    for sx in (-1, 1):
        P.rbox(n + "_Foot", (0.03, 0.03, 0.004), (sx * 0.09, -0.08, 0.0), rubber(), root, r=0.001)
        P.rbox(n + "_Foot", (0.03, 0.03, 0.004), (sx * 0.06, 0.1, 0.0), rubber(), root, r=0.001)
    finish(root, n)
    return root, (W, D, H)


# ------------------------------------------------------------ iMac G3

def build_imac_g3(n):
    root = P.empty(n)
    teal = P.mat("iMac_Teal", (0.03, 0.42, 0.52), 0.12)
    ice = P.mat("iMac_Ice", (0.8, 0.9, 0.92), 0.2)
    wht = P.mat("iMac_White", (0.82, 0.84, 0.84), 0.25)
    # translucent egg shell body
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=40, v_segments=24, radius=0.5)
    for v in bm.verts:
        v.co.x *= 0.40 / 1.0 * 1.0
        v.co.y *= 0.40
        v.co.z *= 0.38
        if v.co.y < 0:       # flatten front to a nearly flat face
            v.co.y *= 0.8
        if v.co.z < 0:       # flat-ish bottom: the base is a narrower foot
            v.co.z *= 0.72
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = P._make(n + "_Shell", bm, teal, root, 0.4, 80)
    body.location = (0, 0.07, 0.255)
    # front bezel (white/ice) and glass
    P.rbox(n + "_Front", (0.375, 0.12, 0.33), (0, -0.04, 0.265), teal, root, r=0.07, seg=5)
    P.rbox(n + "_Bezel", (0.355, 0.012, 0.31), (0, -0.0985, 0.265), ice, root, r=0.03, seg=4)
    P.rbox(n + "_Frame", (0.31, 0.006, 0.245), (0, -0.108, 0.275), black(), root, r=0.012)
    curved_screen(n + "_Screen", 0.285, 0.215, (0, -0.1125, 0.275), root, bulge=0.004)
    # handle on top, foot
    P.rbox(n + "_Handle", (0.06, 0.03, 0.012), (0, 0.06, 0.442), ice, root, r=0.005)
    P.cylinder(n + "_Foot", 0.115, 0.035, (0, 0.06, 0.0175), teal, root, seg=40, r2=0.095)
    P.cylinder(n + "_FootRing", 0.1, 0.006, (0, 0.06, 0.0365), ice, root, seg=40)
    # CD tray slot and speakers, small badge
    P.rbox(n + "_CDSlot", (0.115, 0.003, 0.004), (0, -0.108, 0.098), dark(), root)
    for sx in (-1, 1):
        P.rbox(n + "_Speaker", (0.03, 0.002, 0.012), (sx * 0.12, -0.105, 0.097), dark(), root, r=0.003)
    P.rbox(n + "_LED", (0.004, 0.002, 0.004), (0.14, -0.109, 0.098), P.led("Green", (0.1, 1.0, 0.2), 3.0), root)
    finish(root, n)
    return root, (0.4, 0.4, 0.45)


# ------------------------------------------------------------ PC cases

def drive_bay(prefix, root, x, y, z, w, h, dkm, label=None):
    P.rbox(prefix + "_Bay", (w, 0.004, h), (x, y, z), dkm, root, r=0.001)


def build_pc_desktop(n):
    """Flat desktop case (the kind the CRT sits on)."""
    root = P.empty(n)
    B = beige()
    W, D, H = 0.44, 0.42, 0.14
    P.rbox(n + "_Case", (W, D, H), (0, 0, H / 2), B, root, r=0.006)
    P.rbox(n + "_Front", (W - 0.01, 0.006, H - 0.014), (0, -D / 2 - 0.001, H / 2), B, root, r=0.004)
    # 5.25" CD-ROM bay and 3.5" floppy
    P.rbox(n + "_CDTray", (0.13, 0.004, 0.036), (-0.1, -D / 2 - 0.004, H * 0.68), P.mat("PC_BayGrey", (0.5, 0.48, 0.42), 0.5), root, r=0.002)
    P.rbox(n + "_CDSlot", (0.11, 0.003, 0.004), (-0.1, -D / 2 - 0.0065, H * 0.7), dark(), root)
    P.rbox(n + "_CDButton", (0.014, 0.003, 0.005), (-0.05, -D / 2 - 0.0065, H * 0.6), dark(), root, r=0.001)
    P.rbox(n + "_FloppyBay", (0.1, 0.004, 0.036), (0.07, -D / 2 - 0.004, H * 0.68), P.mat("PC_BayGrey", (0.5, 0.48, 0.42), 0.5), root, r=0.002)
    P.rbox(n + "_FloppySlot", (0.07, 0.003, 0.004), (0.06, -D / 2 - 0.0065, H * 0.7), black(), root)
    P.rbox(n + "_FloppyBtn", (0.012, 0.003, 0.008), (0.1, -D / 2 - 0.0065, H * 0.6), dark(), root)
    P.cylinder(n + "_PowerBtn", 0.011, 0.006, (0.17, -D / 2 - 0.005, H * 0.58), dark(), root, rot=(RADS(90), 0, 0), seg=16)
    P.rbox(n + "_LED1", (0.004, 0.002, 0.004), (0.14, -D / 2 - 0.0045, H * 0.25), P.led("Green", (0.1, 1.0, 0.2), 3.0), root)
    P.rbox(n + "_LED2", (0.004, 0.002, 0.004), (0.15, -D / 2 - 0.0045, H * 0.25), P.led("Amber", (1.0, 0.5, 0.05), 3.0), root)
    # air slots and badge
    for i in range(10):
        P.rbox(n + "_Slot", (0.0015, 0.0015, 0.04), (-0.16 + i * 0.006, -D / 2 - 0.0045, H * 0.22), black(), root) if False else None
    vent_slots(n + "_Vent", root, black(), -0.18, -0.06, -D / 2 - 0.004, -D / 2 - 0.004, 0, 0) if False else None
    for i in range(12):
        P.rbox(n + "_Vent", (0.0015, 0.002, 0.03), (-0.19 + i * 0.006, -D / 2 - 0.0035, H * 0.25), black(), root)
    P.rbox(n + "_Badge", (0.04, 0.002, 0.01), (0.14, -D / 2 - 0.0035, H * 0.85 - 0.01), dark(), root)
    # feet and back ports
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.rbox(n + "_Foot", (0.025, 0.025, 0.004), (sx * (W / 2 - 0.03), sy * (D / 2 - 0.03), 0.0), rubber(), root, r=0.001)
    for i, c in enumerate([(0.1, 0.5, 0.1), (0.6, 0.1, 0.5), (0.1, 0.1, 0.4), (0.4, 0.4, 0.4)]):
        P.rbox(n + "_Port", (0.02, 0.004, 0.012), (-0.15 + i * 0.045, D / 2 + 0.001, H * 0.55), P.mat("PC_Port%d" % i, c, 0.4), root)
    finish(root, n)
    return root, (W, D, H)


def tower_case(n, kind):
    root = P.empty(n)
    W, D, H = 0.2, 0.45, 0.44
    if kind == "beige":
        shell, trim = beige(), P.mat("PC_BayGrey", (0.5, 0.48, 0.42), 0.5)
    elif kind == "black":
        shell, trim = black(), dark()
    else:
        shell, trim = black(), dark()
    P.rbox(n + "_Case", (W, D, H), (0, 0, H / 2), shell, root, r=0.005)
    P.rbox(n + "_FrontPanel", (W - 0.016, 0.01, H - 0.02), (0, -D / 2 - 0.002, H / 2), shell, root, r=0.004)
    # bays
    if kind != "rgb":
        for i, (z, h) in enumerate([(H - 0.045, 0.04), (H - 0.095, 0.04), (H - 0.145, 0.04)]):
            P.rbox(n + "_Bay%d" % i, (0.15, 0.004, h), (0, -D / 2 - 0.0085, z), trim, root, r=0.002)
            if i == 0:
                P.rbox(n + "_CDSlot", (0.12, 0.003, 0.004), (0, -D / 2 - 0.011, z + 0.005), black(), root)
                P.rbox(n + "_CDBtn", (0.014, 0.003, 0.005), (0.055, -D / 2 - 0.011, z - 0.012), dark(), root)
            if i == 1:
                P.rbox(n + "_Slot", (0.09, 0.003, 0.004), (0, -D / 2 - 0.011, z + 0.003), black(), root)
        P.cylinder(n + "_PowerBtn", 0.012, 0.008, (0, -D / 2 - 0.008, H - 0.22), dark(), root, rot=(RADS(90), 0, 0), seg=16)
        P.rbox(n + "_Reset", (0.012, 0.004, 0.008), (0.06, -D / 2 - 0.0075, H - 0.24), dark(), root, r=0.001)
        P.rbox(n + "_LED", (0.004, 0.002, 0.004), (-0.06, -D / 2 - 0.007, H - 0.24), P.led("Green", (0.1, 1.0, 0.2), 3.0), root)
        P.rbox(n + "_LED2", (0.004, 0.002, 0.004), (-0.05, -D / 2 - 0.007, H - 0.24), P.led("Amber", (1.0, 0.5, 0.05), 3.0), root)
        for i in range(14):
            P.rbox(n + "_Vent", (0.14, 0.002, 0.0015), (0, -D / 2 - 0.007, 0.03 + i * 0.006), black(), root)
    else:
        # mesh front, RGB fans, glass side
        for i in range(3):
            fz = H * 0.2 + i * 0.13
            col = [(0.7, 0.1, 1.0), (0.1, 0.5, 1.0), (1.0, 0.1, 0.5)][i]
            P.cylinder(n + "_FanFrame", 0.06, 0.012, (0, -D / 2 - 0.004, fz), black(), root, rot=(RADS(90), 0, 0), seg=32)
            P.lathe(n + "_FanRing", [(0.047, 0.0), (0.055, 0.0)], (0, -D / 2 - 0.0105, fz), P.led("RGB%d" % i, col, 2.0), root, rot=(RADS(90), 0, 0), seg=40)
            for k in range(7):
                a_ = k * 2 * math.pi / 7
                P.rbox(n + "_Blade", (0.034, 0.002, 0.012), (math.cos(a_) * 0.026, -D / 2 - 0.0105, fz + math.sin(a_) * 0.026), P.mat("PC_FanBlade", (0.04, 0.04, 0.05), 0.4), root, rot=(0, -a_ + RADS(25), 0))
            P.cylinder(n + "_FanHub", 0.015, 0.014, (0, -D / 2 - 0.0045, fz), dark(), root, rot=(RADS(90), 0, 0), seg=16)
        P.rbox(n + "_Glass", (0.004, D - 0.06, H - 0.06), (-W / 2 - 0.001, 0.0, H / 2), P.mat("PC_Glass", (0.05, 0.05, 0.07), 0.05, alpha=0.5), root, r=0.002)
        P.rbox(n + "_LEDStrip", (0.004, D - 0.1, 0.006), (-W / 2 + 0.002, 0.0, H - 0.06), P.led("RGBStrip", (0.6, 0.1, 1.0), 5.0), root)
        P.rbox(n + "_PowerBtn", (0.02, 0.006, 0.02), (0, -D / 2 - 0.005, H - 0.03), dark(), root, r=0.004)
    for sx in (-1, 1):
        P.rbox(n + "_Foot", (0.03, 0.04, 0.006), (sx * (W / 2 - 0.02), 0, 0.0), rubber(), root, r=0.001)
    for i in range(3):
        P.rbox(n + "_BackPort", (0.012, 0.004, 0.008), (0.02 * i - 0.04, D / 2 + 0.001, H - 0.04), dark(), root)
    for i in range(7):
        P.rbox(n + "_BackSlot", (0.012, 0.004, 0.0035), (0.0, D / 2 + 0.001, 0.02 + i * 0.012), black(), root) if False else None
    P.rbox(n + "_PSU", (0.1, 0.004, 0.08), (0.04, D / 2 + 0.001, 0.08), P.mat("PC_PSU", (0.15, 0.15, 0.16), 0.5), root)
    finish(root, n)
    return root, (W, D, H)


# ------------------------------------------------------------ laptops

def laptop(n, shell, w=0.34, d=0.24, thick=0.022, lid_deg=105, screen_w=0.30, kb_keys=True):
    root = P.empty(n)
    P.rbox(n + "_Base", (w, d, thick), (0, -d / 2 - 0.0, thick / 2), shell, root, r=0.004)
    # keyboard well: rows of small keys
    keys = P.mat("LT_Keys", (0.025, 0.025, 0.028), 0.5)
    kx0 = -w / 2 + 0.03
    pitch = 0.0165
    rows = [14, 14, 13, 12, 8]
    for r, cnt in enumerate(rows):
        y = -0.04 - r * 0.0165 - 0.04
        xs = -(cnt - 1) * pitch / 2
        for c in range(cnt):
            wd = pitch - 0.0012
            x = xs + c * pitch
            if r == 4 and c == 2:
                continue
            ww = wd
            if r == 4 and c == 1:
                x = xs + (1 + 4.5 * 0.5) * pitch * 0.9
                ww = wd + pitch * 5
            P.rbox(n + "_Key", (ww, 0.0148, 0.0035), (x, y, thick + 0.0015), keys, root, r=0.0008, seg=2)
    P.rbox(n + "_Touchpad", (0.07, 0.05, 0.0006), (0, -d + 0.04, thick + 0.0003), P.mat("LT_Pad", (0.1, 0.1, 0.11), 0.3), root, r=0.002)
    # lid on a hinge at the back edge: swung open
    hinge = Vector((0, -0.0, thick))
    ang = RADS(lid_deg - 90 + 90) - RADS(90)        # lid opened lid_deg degrees from closed
    lid_h = d - 0.01
    lid = P.rbox(n + "_Lid", (w, 0.008, lid_h), (0, 0, lid_h / 2), shell, None, r=0.003)
    bez = P.rbox(n + "_LidBezel", (screen_w + 0.016, 0.003, lid_h - 0.025), (0, -0.0046, lid_h / 2 - 0.004), black(), None, r=0.002)
    scr = curved_screen(n + "_Screen", screen_w, lid_h - 0.045, (0, -0.0065, lid_h / 2 - 0.004), None, bulge=0.0)
    tilt = RADS(lid_deg - 90)       # 90 = upright
    for o in (lid, bez, scr):
        o.parent = root
        o.matrix_parent_inverse = Matrix.Identity(4)
        mw = Matrix.Translation((0, 0.0, thick)) @ Matrix.Rotation(-tilt, 4, "X")
        o.matrix_basis = mw @ o.matrix_basis
    # hinge cylinder and LED
    P.cylinder(n + "_Hinge", 0.006, w * 0.6, (0, 0.0, thick), dark(), root, rot=(0, RADS(90), 0), seg=10)
    P.rbox(n + "_LED", (0.004, 0.002, 0.003), (w / 2 - 0.02, -d + 0.003, thick / 2), P.led("Amber", (1.0, 0.6, 0.1), 3.0), root)
    for sx in (-1, 1):
        P.rbox(n + "_Foot", (0.03, 0.02, 0.003), (sx * (w / 2 - 0.03), -0.02, 0.0), rubber(), root)
        P.rbox(n + "_Foot", (0.03, 0.02, 0.003), (sx * (w / 2 - 0.03), -d + 0.02, 0.0), rubber(), root)
    # move so that the origin is under the middle of the whole device footprint
    for o in list(bpy.data.objects):
        if o.parent == root and o.type == "MESH":
            o.location.y += d / 2 if False else 0
    root.location = (0, d / 2 - 0.0, 0)
    finish(root, n)
    return root, (w, d, lid_h + thick)


def build_laptop_silver(n):
    return laptop(n, P.tex("LT_Alu", "metal_brushed.png", 0.3, 1.0, size=512, fallback=(0.65, 0.65, 0.67)))


def build_laptop_black(n):
    return laptop(n, P.mat("LT_Black", (0.025, 0.025, 0.028), 0.45), w=0.31, d=0.23, screen_w=0.27)


# ------------------------------------------------------------ flat monitor

def build_monitor_flat(n):
    root = P.empty(n)
    bl = black()
    W, H = 0.62, 0.36
    P.rbox(n + "_Panel", (W, 0.022, H), (0, 0.04, 0.42), bl, root, r=0.004)
    P.rbox(n + "_Back", (W * 0.6, 0.03, H * 0.7), (0, 0.065, 0.42), bl, root, r=0.01)
    P.rbox(n + "_Bezel", (W - 0.01, 0.002, H - 0.01), (0, 0.0275, 0.42), P.mat("FM_Bezel", (0.01, 0.01, 0.012), 0.35), root, r=0.003)
    curved_screen(n + "_Screen", W - 0.026, H - 0.028, (0, 0.0255, 0.42), root, bulge=0.0)
    P.rbox(n + "_Neck", (0.05, 0.02, 0.26), (0, 0.075, 0.2), P.mat("FM_Neck", (0.04, 0.04, 0.045), 0.35, 0.6), root, r=0.003)
    P.rbox(n + "_Foot", (0.24, 0.2, 0.012), (0, 0.06, 0.006), P.mat("FM_Neck", (0.04, 0.04, 0.045), 0.35, 0.6), root, r=0.004)
    P.rbox(n + "_LED", (0.004, 0.002, 0.003), (W / 2 - 0.04, 0.0275, 0.42 - H / 2 + 0.006), P.led("Blue", (0.2, 0.5, 1.0), 4.0), root)
    # move forward so the neck is under the middle: shift all by -0.05 in y to centre the footprint
    finish(root, n)
    return root, (W, 0.2, 0.6)


# ------------------------------------------------------------ mice

def mouse(n, kind):
    root = P.empty(n)
    if kind == "ball":
        shell = beige()
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=14, radius=0.5)
        for v in bm.verts:
            v.co.x *= 0.065
            v.co.y *= 0.11
            v.co.z *= 0.06
            if v.co.z < 0:
                v.co.z = 0.0
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        P._make(n + "_Body", bm, shell, root, 0.3, 60)
        P.rbox(n + "_Btn", (0.058, 0.001, 0.002), (0, -0.01, 0.0) , dark(), root) if False else None
        P.rbox(n + "_Split", (0.0015, 0.045, 0.002), (0.0, -0.0275, 0.0285), dark(), root)
        P.rbox(n + "_Split2", (0.062, 0.0015, 0.002), (0.0, -0.005, 0.0285), dark(), root)
        P.tube(n + "_Cable", [(0, 0.055, 0.01), (0, 0.09, 0.003), (0.05, 0.14, 0.003), (0.1, 0.25, 0.003)], 0.0017, P.mat("M_Cable", (0.5, 0.48, 0.42), 0.6), root, seg=6)
    elif kind == "mac":
        shell = beige()
        P.rbox(n + "_Body", (0.052, 0.098, 0.03), (0, 0, 0.015), shell, root, r=0.01, seg=4)
        P.rbox(n + "_Btn", (0.026, 0.026, 0.004), (0, -0.015, 0.0295), P.mat("Mac_Btn", (0.4, 0.38, 0.34), 0.5), root, r=0.004)
        P.tube(n + "_Cable", [(0, 0.05, 0.012), (0, 0.09, 0.003), (-0.03, 0.14, 0.003)], 0.0017, P.mat("M_Cable", (0.5, 0.48, 0.42), 0.6), root, seg=6)
    else:   # optical, black, two buttons + wheel
        shell = black()
        P.rbox(n + "_Body", (0.062, 0.115, 0.036), (0, 0, 0.018), shell, root, r=0.014, seg=4)
        P.rbox(n + "_Split", (0.0012, 0.05, 0.002), (0, -0.025, 0.0365), dark(), root)
        P.cylinder(n + "_Wheel", 0.007, 0.005, (0, -0.03, 0.037), P.mat("M_Wheel", (0.3, 0.3, 0.32), 0.5), root, rot=(0, RADS(90), 0), seg=12)
        if kind == "rgb":
            P.rbox(n + "_Glow", (0.065, 0.004, 0.003), (0, 0.045, 0.01), P.led("MouseRGB", (0.6, 0.1, 1.0), 6.0), root, r=0.001)
        P.tube(n + "_Cable", [(0, 0.056, 0.012), (0, 0.09, 0.003), (0.03, 0.14, 0.003), (0, 0.25, 0.003)], 0.0017, P.mat("M_CableB", (0.02, 0.02, 0.02), 0.6), root, seg=6)
    finish(root, n)
    return root, (0.065, 0.115, 0.036)


BUILDERS = {
    "mac_classic": build_mac_classic,
    "imac_g3": build_imac_g3,
    "crt_beige14": build_crt_beige14,
    "crt_grey17": build_crt_grey17,
    "tv_silver": build_tv_silver,
    "tv_small": build_tv_small,
    "pc_desktop_beige": build_pc_desktop,
    "pc_tower_beige": lambda n: tower_case(n, "beige"),
    "pc_tower_black": lambda n: tower_case(n, "black"),
    "pc_tower_rgb": lambda n: tower_case(n, "rgb"),
    "laptop_silver": build_laptop_silver,
    "laptop_black": build_laptop_black,
    "monitor_flat": build_monitor_flat,
    "mouse_ball": lambda n: mouse(n, "ball"),
    "mouse_mac": lambda n: mouse(n, "mac"),
    "mouse_optical": lambda n: mouse(n, "optical"),
    "mouse_rgb": lambda n: mouse(n, "rgb"),
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
            dist = max(size) * 2.2 + 0.2
            P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0, hh * 0.45), (dist * 0.55, -dist, hh * 0.9), 50)


if __name__ == "__main__":
    main()
