"""Detailed office/home desks with proper joinery, edges and textured tops.

  blender -b -P blender/props/build_desks.py -- [kind ...] [--width=1.45] [--no-render]

Kinds: soviet, white, gaming, office, school, glass, pine. Without --width every kind is exported at 1.30, 1.45, 1.60 and
1.75 m as desk_<kind>_<cm>.glb. Depth 0.80 m, height 0.75 m (scale Z a little for 0.73..0.77).

Frame: origin in the middle of the top surface (z = 0 is the top), the floor is z = -height, the chair side is -Y,
the wall side is +Y, x runs along the row.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy

import propslib as P
from build_computers import finish, hull, RADS

D = 0.8
H = 0.75


def wood(name, file, rough, tint=None, tilemult=1.0):
    return P.tex(name, file, rough, 0.0, size=1024, tint=tint, tilemult=tilemult, grain=True, fallback=(0.4, 0.25, 0.12))


def steel(name="Desk_Steel", color=(0.7, 0.7, 0.72), rough=0.3):
    return P.mat(name, color, rough, 1.0)


def cap_rubber():
    return P.mat("Desk_Rubber", (0.015, 0.015, 0.015), 0.85)


def tapered_leg(n, x, y, top_z, bottom_z, s_top, s_bot, material, root, r=0.002):
    pts = [(x + sx * s_top / 2, y + sy * s_top / 2, top_z) for sx in (-1, 1) for sy in (-1, 1)] + \
          [(x + sx * s_bot / 2, y + sy * s_bot / 2, bottom_z) for sx in (-1, 1) for sy in (-1, 1)]
    return hull(n, pts, material, root, r=r, seg=2, tile=0.6)


def handle_bar(n, x, y, z, length, material, root, vertical=False):
    if vertical:
        P.cylinder(n, 0.006, length, (x, y - 0.012, z), material, root, seg=10)
    else:
        P.cylinder(n, 0.006, length, (x, y - 0.012, z), material, root, rot=(0, RADS(90), 0), seg=10)
    for s in (-1, 1):
        dx, dz = (0, s * length * 0.4) if vertical else (s * length * 0.4, 0)
        P.cylinder(n + "_Post", 0.004, 0.012, (x + dx, y - 0.006, z + dz), material, root, rot=(RADS(90), 0, 0), seg=8)


def drawer(n, cx, cy, cz, w, h, front_mat, handle_mat, root, d_face=0.02, handle="bar", gap=0.003):
    P.rbox(n + "_Front", (w - gap, d_face, h - gap), (cx, cy, cz), front_mat, root, r=0.003)
    P.rbox(n + "_Panel", (w * 0.72, 0.003, h * 0.58), (cx, cy - d_face / 2 - 0.0008, cz), front_mat, root, r=0.002)
    if handle == "bar":
        handle_bar(n + "_Handle", cx, cy - d_face / 2 - 0.002, cz + h * 0.1, min(w * 0.4, 0.11), handle_mat, root)
    elif handle == "knob":
        P.cylinder(n + "_Knob", 0.014, 0.025, (cx, cy - d_face / 2 - 0.012, cz), front_mat, root, rot=(RADS(90), 0, 0), seg=16, r2=0.01)
    elif handle == "slot":
        P.rbox(n + "_Slot", (w * 0.4, 0.004, 0.012), (cx, cy - d_face / 2 - 0.0005, cz + h * 0.25), P.mat("Desk_SlotDark", (0.02, 0.02, 0.02), 0.6), root, r=0.003)


def build_soviet(n, w):
    root = P.empty(n)
    ww = wood("Desk_SovietWood", "wood_dark.png", 0.28, tilemult=1.0)
    brass = P.mat("Brass", (0.72, 0.55, 0.22), 0.28, 1.0)
    leather = P.mat("Desk_Leather", (0.04, 0.17, 0.1), 0.65)
    P.rbox(n + "_Top", (w, D, 0.036), (0, 0, -0.018), ww, root, r=0.01, seg=4)
    # green leather insert with a thin brass frame
    iw, idp = w * 0.64, D * 0.6
    P.rbox(n + "_Leather", (iw, idp, 0.002), (0.0, -0.02, 0.001), leather, root, r=0.0005)
    for sx in (-1, 1):
        P.rbox(n + "_Border", (0.003, idp + 0.006, 0.0022), (sx * (iw / 2 + 0.0015), -0.02, 0.0011), brass, root)
    for sy in (-1, 1):
        P.rbox(n + "_Border", (iw + 0.006, 0.003, 0.0022), (0, -0.02 + sy * (idp / 2 + 0.0015), 0.0011), brass, root)
    # left pedestal with plinth, three drawers
    pw, pd = 0.46, D - 0.06
    px = -w / 2 + 0.03 + pw / 2
    plinth = 0.07
    P.rbox(n + "_Pedestal", (pw, pd, H - 0.036 - plinth), (px, 0.0, -0.036 - (H - 0.036 - plinth) / 2), ww, root, r=0.004)
    P.rbox(n + "_Plinth", (pw - 0.03, pd - 0.03, plinth), (px, 0.0, -H + plinth / 2), P.mat("Desk_Plinth", (0.07, 0.035, 0.02), 0.5), root, r=0.003)
    avail = H - 0.036 - plinth - 0.02
    hs = [0.14, avail / 2 - 0.07 - 0.002, avail / 2 - 0.07 - 0.002]
    z = -0.036 - 0.012
    for k, hh in enumerate(hs):
        z -= hh / 2
        drawer(n + "_D%d" % k, px, -pd / 2 - 0.006, z, pw - 0.04, hh - 0.008, ww, brass, root)
        P.cylinder(n + "_Key", 0.004, 0.003, (px + 0.12, -pd / 2 - 0.0135, z + hh * 0.18), brass, root, rot=(RADS(90), 0, 0), seg=10)
        z -= hh / 2 + 0.008
    # right side: two square tapered legs, apron, back panel
    lx = w / 2 - 0.045
    for sy in (-1, 1):
        tapered_leg(n + "_Leg", lx, sy * (D / 2 - 0.05), -0.036, -H, 0.06, 0.04, ww, root)
    P.rbox(n + "_ApronFront", (w - pw - 0.12, 0.02, 0.1), ((px + pw / 2 + lx - 0.03) / 2 + 0.0, -D / 2 + 0.05, -0.036 - 0.05), ww, root, r=0.003)
    P.rbox(n + "_ApronSide", (0.02, D - 0.12, 0.1), (lx, 0, -0.036 - 0.05), ww, root, r=0.003)
    P.rbox(n + "_Back", (w - pw - 0.1, 0.02, 0.5), ((px + pw / 2 + lx) / 2, D / 2 - 0.05, -0.036 - 0.25), ww, root, r=0.003)
    # pencil drawer in the middle apron
    P.rbox(n + "_Pencil", (0.3, 0.018, 0.06), (0.1, -D / 2 + 0.04, -0.036 - 0.04), ww, root, r=0.003)
    handle_bar(n + "_PencilHandle", 0.1, -D / 2 + 0.03, -0.036 - 0.04, 0.08, brass, root)
    for sx in (-1, 1):
        P.cylinder(n + "_Cap", 0.02, 0.004, (lx, sx * (D / 2 - 0.05), -H + 0.002), cap_rubber(), root, seg=12)
    finish(root, n)
    return root


def build_white(n, w):
    root = P.empty(n)
    lam = P.tex("Desk_White", "laminate_white.png", 0.4, size=1024, tilemult=1.5, fallback=(0.85, 0.85, 0.82))
    banding = P.mat("Desk_Banding", (0.62, 0.62, 0.6), 0.5)
    st = steel("Desk_SteelChrome", (0.78, 0.78, 0.8), 0.22)
    P.rbox(n + "_Top", (w, D, 0.025), (0, 0, -0.0125), lam, root, r=0.003, seg=2)
    P.rbox(n + "_Edge", (w + 0.002, 0.002, 0.024), (0, -D / 2 - 0.0005, -0.0125), banding, root)
    # leg frame: four round legs, rail frame under the top, corner brackets, cable tray at the back
    lx, ly = w / 2 - 0.06, D / 2 - 0.06
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.cylinder(n + "_Leg", 0.0225, H - 0.025, (sx * lx, sy * ly, -0.025 - (H - 0.025) / 2), st, root, seg=20)
            P.cylinder(n + "_Foot", 0.026, 0.012, (sx * lx, sy * ly, -H + 0.006), cap_rubber(), root, seg=20)
            P.rbox(n + "_Bracket", (0.07, 0.07, 0.012), (sx * lx, sy * ly, -0.031), steel("Desk_BracketGrey", (0.3, 0.3, 0.32), 0.45), root, r=0.002)
    for sy in (-1, 1):
        P.rbox(n + "_RailX", (2 * lx, 0.025, 0.05), (0, sy * ly, -0.06), st, root, r=0.002)
    for sx in (-1, 1):
        P.rbox(n + "_RailY", (0.025, 2 * ly, 0.05), (sx * lx, 0, -0.06), st, root, r=0.002)
    # cable tray: wire basket
    P.rbox(n + "_Tray", (w * 0.5, 0.12, 0.002), (0, ly - 0.07, -0.18), st, root)
    for sx in (-1, 1):
        P.rbox(n + "_TraySide", (0.002, 0.12, 0.05), (sx * w * 0.25, ly - 0.07, -0.155), st, root)
    for i in range(4):
        P.rbox(n + "_TrayHang", (0.012, 0.012, 0.13), (-w * 0.2 + i * w * 0.133, ly - 0.07, -0.095), st, root) if False else None
    for sx in (-1, 1):
        P.rbox(n + "_TrayHang", (0.012, 0.02, 0.12), (sx * w * 0.2, ly - 0.07, -0.12), st, root)
    P.cylinder(n + "_Grommet", 0.03, 0.004, (w / 2 - 0.2, ly - 0.02, 0.0005), P.mat("Desk_Grommet", (0.08, 0.08, 0.08), 0.5), root, seg=24)
    finish(root, n)
    return root


def build_gaming(n, w):
    root = P.empty(n)
    top = P.mat("Desk_GamingTop", (0.02, 0.02, 0.023), 0.32)
    mat_b = P.mat("Desk_GamingFrame", (0.025, 0.025, 0.028), 0.45, 0.4)
    red = P.mat("Desk_GamingRed", (0.5, 0.04, 0.05), 0.4)
    # top with a carbon-style gloss and a bevelled front edge
    P.rbox(n + "_Top", (w, D, 0.03), (0, 0, -0.015), top, root, r=0.01, seg=4)
    P.rbox(n + "_Mat", (w * 0.55, D * 0.5, 0.0015), (0.0, -0.1, 0.00075), P.mat("Desk_Mousemat", (0.03, 0.03, 0.04), 0.9), root, r=0.0005) if False else None
    P.rbox(n + "_LED", (w - 0.06, 0.008, 0.006), (0, -D / 2 + 0.012, -0.0305), P.led("DeskRGB", (0.7, 0.1, 1.0), 6.0), root, r=0.002)
    # grommet with a lid and a cup holder clamp on the side
    P.cylinder(n + "_Grommet", 0.03, 0.004, (w / 2 - 0.15, D / 2 - 0.08, 0.001), P.mat("Desk_GromDark", (0.005, 0.005, 0.006), 0.5), root, seg=24)
    P.lathe(n + "_GromRing", [(0.03, 0.0), (0.034, 0.0), (0.034, 0.004), (0.03, 0.004)], (w / 2 - 0.15, D / 2 - 0.08, 0.0), mat_b, root, seg=24)
    # T legs: tall posts on feet, beam between, cable tray
    for sx in (-1, 1):
        x = sx * (w / 2 - 0.12)
        P.rbox(n + "_Post", (0.06, 0.07, H - 0.03), (x, 0.08, -0.03 - (H - 0.03) / 2), mat_b, root, r=0.004)
        P.rbox(n + "_Foot", (0.08, D - 0.1, 0.045), (x, 0.0, -H + 0.0225), mat_b, root, r=0.006)
        P.rbox(n + "_Stripe", (0.081, 0.04, 0.012), (x, -D / 2 + 0.11, -H + 0.0225), red, root) if False else None
        P.rbox(n + "_FootPad", (0.07, 0.05, 0.006), (x, -D / 2 + 0.08, -H + 0.003), cap_rubber(), root)
        P.rbox(n + "_FootPad", (0.07, 0.05, 0.006), (x, D / 2 - 0.08, -H + 0.003), cap_rubber(), root)
        P.rbox(n + "_RedStrip", (0.002, 0.04, 0.4), (x + sx * 0.031, 0.08, -0.4), red, root, r=0.0005)
    P.rbox(n + "_Beam", (w - 0.24, 0.05, 0.05), (0, 0.08, -0.07), mat_b, root, r=0.004)
    P.rbox(n + "_Brace", (w - 0.24, 0.02, 0.12), (0, D / 2 - 0.12, -0.16), mat_b, root, r=0.003)
    # headphone hook on the left side
    P.tube(n + "_Hook", [(-w / 2 + 0.02, -0.12, -0.03), (-w / 2 - 0.0, -0.12, -0.06), (-w / 2 - 0.05, -0.12, -0.08), (-w / 2 - 0.07, -0.12, -0.04)], 0.008, mat_b, root, seg=10)
    P.rbox(n + "_HookClamp", (0.012, 0.05, 0.06), (-w / 2 + 0.0, -0.12, -0.02), mat_b, root, r=0.003)
    finish(root, n)
    return root


def build_office(n, w):
    root = P.empty(n)
    grey = P.tex("Desk_OfficeGrey", "laminate_white.png", 0.5, size=1024, tilemult=1.5, tint=(0.52, 0.53, 0.52))
    frame = P.mat("Desk_OfficeFrame", (0.2, 0.2, 0.22), 0.4, 0.6)
    edge = P.mat("Desk_OfficeEdge", (0.08, 0.08, 0.09), 0.5)
    P.rbox(n + "_Top", (w, D, 0.03), (0, 0, -0.015), grey, root, r=0.003, seg=2)
    P.rbox(n + "_EdgeFront", (w, 0.003, 0.03), (0, -D / 2 - 0.0005, -0.015), edge, root)
    # left panel leg with foot, right drawer pedestal
    px = -w / 2 + 0.02
    P.rbox(n + "_PanelL", (0.03, D - 0.1, H - 0.03), (px, 0.0, -0.03 - (H - 0.03) / 2), frame, root, r=0.002)
    P.rbox(n + "_FootL", (0.05, D - 0.06, 0.03), (px, 0.0, -H + 0.015), frame, root, r=0.003)
    pw = 0.42
    cx = w / 2 - 0.02 - pw / 2
    ph = 0.55
    P.rbox(n + "_Pedestal", (pw, D - 0.1, ph), (cx, 0.0, -0.03 - ph / 2), grey, root, r=0.003)
    for k in range(3):
        hh = (ph - 0.02) / 3
        zc = -0.03 - 0.01 - hh * (k + 0.5)
        P.rbox(n + "_DFront", (pw - 0.015, 0.018, hh - 0.006), (cx, -D / 2 + 0.05 - 0.015, zc), grey, root, r=0.003)
        P.rbox(n + "_DHandle", (0.1, 0.012, 0.01), (cx, -D / 2 + 0.05 - 0.03, zc + hh * 0.25), steel("Desk_Handle", (0.65, 0.65, 0.68), 0.25), root, r=0.003)
    P.rbox(n + "_PedestalFoot", (pw - 0.05, D - 0.14, 0.02), (cx, 0.0, -0.03 - ph - 0.01), frame, root)
    for sx in (-1, 1):
        P.rbox(n + "_PedLeg", (0.04, D - 0.14, H - 0.03 - ph), (cx + sx * (pw / 2 - 0.03), 0, -0.03 - ph - (H - 0.03 - ph) / 2), frame, root, r=0.002) if False else None
    P.rbox(n + "_PedGlide", (0.04, 0.04, 0.2), (cx, 0, -H + 0.1), frame, root) if False else None
    # modesty panel and a cable port
    P.rbox(n + "_Modesty", (w - 0.5 - 0.06, 0.018, 0.4), ((px + cx - pw / 2) / 2 + 0.0, D / 2 - 0.05, -0.03 - 0.2 - 0.02), frame, root, r=0.002)
    P.cylinder(n + "_Grommet", 0.025, 0.004, (-0.2, D / 2 - 0.08, 0.0006), P.mat("Desk_GromDark", (0.02, 0.02, 0.02), 0.5), root, seg=24)
    # panel leg for the pedestal side: floor foot under the drawer unit
    P.rbox(n + "_PedLegBack", (pw, 0.03, H - 0.03 - ph), (cx, D / 2 - 0.06, -0.03 - ph - (H - 0.03 - ph) / 2), frame, root, r=0.002)
    P.rbox(n + "_PedLegFront", (pw, 0.03, H - 0.03 - ph), (cx, -D / 2 + 0.06, -0.03 - ph - (H - 0.03 - ph) / 2), frame, root, r=0.002)
    finish(root, n)
    return root


def build_school(n, w):
    root = P.empty(n)
    birch = wood("Desk_Birch", "wood_birch.png", 0.5, tint=(0.95, 0.72, 0.5))
    green = P.mat("Desk_GreenFrame", (0.03, 0.17, 0.08), 0.4, 0.55)
    P.rbox(n + "_Top", (w, D, 0.03), (0, 0, -0.015), birch, root, r=0.012, seg=3)
    P.rbox(n + "_Groove", (w - 0.1, 0.012, 0.003), (0, -D / 2 + 0.09, 0.0008), P.mat("Desk_Groove", (0.3, 0.22, 0.12), 0.7), root, r=0.0015)
    # tubular frame: legs, floor rails, upper rails, back cross tube, rubber caps
    r_t = 0.016
    lx, ly = w / 2 - 0.05, D / 2 - 0.07
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.cylinder(n + "_Leg", r_t, H - 0.03, (sx * lx, sy * ly, -0.03 - (H - 0.03) / 2), green, root, seg=14)
            P.cylinder(n + "_Cap", 0.019, 0.015, (sx * lx, sy * ly, -H + 0.0075), cap_rubber(), root, seg=14)
        P.tube(n + "_FloorRail", [(sx * lx, -ly, -H + 0.12), (sx * lx, ly, -H + 0.12)], r_t, green, root, seg=14)
        P.tube(n + "_UpperRail", [(sx * lx, -ly, -0.05), (sx * lx, ly, -0.05)], r_t, green, root, seg=14)
    P.tube(n + "_BackTube", [(-lx, ly, -0.17), (lx, ly, -0.17)], r_t, green, root, seg=14)
    P.tube(n + "_FrontTube", [(-lx, -ly, -0.05), (lx, -ly, -0.05)], r_t * 0.8, green, root, seg=12)
    # book shelf plank + clips
    P.rbox(n + "_Shelf", (w - 0.14, D - 0.26, 0.015), (0, 0.06, -0.18), birch, root, r=0.003)
    for sx in (-1, 1):
        P.rbox(n + "_Clip", (0.02, D - 0.3, 0.012), (sx * (w / 2 - 0.07), 0.06, -0.18 - 0.0135), green, root)
    finish(root, n)
    return root


def build_glass(n, w):
    root = P.empty(n)
    glass = P.mat("Desk_Glass", (0.6, 0.78, 0.78), 0.04, alpha=0.35, trans=1.0)
    glass_edge = P.mat("Desk_GlassEdge", (0.2, 0.45, 0.42), 0.1, alpha=0.8)
    chrome = P.mat("Chrome", (0.82, 0.82, 0.85), 0.1, 1.0)
    P.rbox(n + "_Top", (w, D, 0.012), (0, 0, -0.006), glass, root, r=0.003, seg=3)
    P.rbox(n + "_TopEdge", (w, D, 0.002), (0, 0, -0.011), glass_edge, root, r=0.001)
    # two chrome trestles: slanted tubes + top bar + cross bar + clips
    for sx in (-1, 1):
        x = sx * (w / 2 - 0.12)
        for sy in (-1, 1):
            P.tube(n + "_Leg", [(x, sy * 0.05, -0.02), (x, sy * (D / 2 - 0.08), -H + 0.01)], 0.014, chrome, root, seg=16, smooth_pts=False)
            P.cylinder(n + "_Foot", 0.02, 0.01, (x, sy * (D / 2 - 0.08), -H + 0.005), cap_rubber(), root, seg=16)
        P.tube(n + "_TopBar", [(x, -0.25, -0.016), (x, 0.25, -0.016)], 0.012, chrome, root, seg=16, smooth_pts=False)
        P.tube(n + "_Cross", [(x, -0.28, -H + 0.2), (x, 0.28, -H + 0.2)], 0.01, chrome, root, seg=12, smooth_pts=False)
    for i in range(4):
        sx, sy = (-1, 1)[i % 2], (-1, 1)[i // 2]
        P.cylinder(n + "_Clip", 0.015, 0.006, (sx * (w / 2 - 0.12), sy * 0.25, -0.014), chrome, root, seg=16)
    P.rbox(n + "_Shelf", (w - 0.4, D - 0.4, 0.008), (0, 0, -0.38), glass, root, r=0.002)
    for sx in (-1, 1):
        P.tube(n + "_ShelfBar", [(sx * (w / 2 - 0.12), -(D - 0.4) / 2 - 0.01, -0.376), (sx * (w / 2 - 0.12), (D - 0.4) / 2 + 0.01, -0.376)], 0.007, chrome, root, seg=10, smooth_pts=False)
    finish(root, n)
    return root


def build_pine(n, w):
    root = P.empty(n)
    pine = wood("Desk_Pine", "wood_pine.png", 0.55, tint=(0.95, 0.68, 0.42))
    dark = P.mat("Desk_PineGap", (0.14, 0.09, 0.04), 0.7)
    # top of five boards with fine gaps
    nb = 5
    bw = D / nb
    for i in range(nb):
        P.rbox(n + "_Board", (w, bw - 0.002, 0.038), (0, -D / 2 + bw * (i + 0.5), -0.019), pine, root, r=0.003, seg=2)
    P.rbox(n + "_Cleat", (w - 0.1, 0.03, 0.024), (0, 0, -0.05), pine, root, r=0.002)
    # apron around, four legs, back X brace, stretcher
    ap_h, ap_t = 0.1, 0.022
    lx, ly = w / 2 - 0.06, D / 2 - 0.06
    P.rbox(n + "_ApronF", (2 * lx, ap_t, ap_h), (0, -ly, -0.038 - ap_h / 2), pine, root, r=0.002)
    P.rbox(n + "_ApronB", (2 * lx, ap_t, ap_h), (0, ly, -0.038 - ap_h / 2), pine, root, r=0.002)
    for sx in (-1, 1):
        P.rbox(n + "_ApronS", (ap_t, 2 * ly, ap_h), (sx * lx, 0, -0.038 - ap_h / 2), pine, root, r=0.002)
        for sy in (-1, 1):
            tapered_leg(n + "_Leg", sx * lx, sy * ly, -0.038, -H, 0.065, 0.045, pine, root)
            P.rbox(n + "_Cap", (0.048, 0.048, 0.006), (sx * lx, sy * ly, -H + 0.003), cap_rubber(), root)
    # drawer in the front apron
    P.rbox(n + "_DrawerFront", (0.4, 0.02, ap_h - 0.02), (0.0, -ly - 0.006, -0.038 - ap_h / 2), pine, root, r=0.003)
    P.cylinder(n + "_Knob", 0.016, 0.03, (0.0, -ly - 0.03, -0.038 - ap_h / 2), pine, root, rot=(RADS(90), 0, 0), seg=16, r2=0.011)
    # side stretchers and an X brace on the back
    for sx in (-1, 1):
        P.rbox(n + "_Stretcher", (0.03, 2 * ly - 0.05, 0.04), (sx * lx, 0, -H + 0.22), pine, root, r=0.003)
    L = math.hypot(2 * lx - 0.08, 0.38)
    ang = math.atan2(0.38, 2 * lx - 0.08)
    for s in (-1, 1):
        P.rbox(n + "_Brace", (L, 0.02, 0.04), (0, ly - 0.004, -H + 0.32 + 0.0), pine, root, rot=(0, s * ang, 0), r=0.002)
    finish(root, n)
    return root


BUILDERS = {"soviet": build_soviet, "white": build_white, "gaming": build_gaming, "office": build_office,
            "school": build_school, "glass": build_glass, "pine": build_pine}


def main():
    kinds = [a for a in P.ARGS if not a.startswith("--")] or list(BUILDERS)
    widths = [float(a.split("=")[1]) for a in P.ARGS if a.startswith("--width=")] or [1.30, 1.45, 1.60, 1.75]
    do_render = "--no-render" not in P.ARGS
    for k in kinds:
        for w in widths:
            nm = "desk_%s_%d" % (k, round(w * 100))
            scn = P.reset()
            root = BUILDERS[k](nm, w)
            P.export(root, nm)
            n, tris = P.stats(root)
            print("BUILT %s %.2f x %.2f x %.2f objects=%d tris=%d" % (nm, w, D, H, n, tris))
            if do_render and abs(w - 1.45) < 0.01:
                P.setup_studio(scn, (900, 600), ground_z=-H)
                P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0, -0.35), (1.7, -2.0, 0.55), 45)


if __name__ == "__main__":
    main()
