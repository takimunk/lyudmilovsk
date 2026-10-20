"""Detailed keyboards with individual keycaps and real legend geometry.

  blender -b -P blender/props/build_keyboards.py -- [name ...] [--no-render]

Outputs (blender/props/export/*.glb, blender/props/*.blend, renders/props/*.png):
  keyboard_modern     black 104-key office keyboard, Latin + Cyrillic legends
  keyboard_gaming     black 104-key, RGB under-glow, glowing legends
  keyboard_modelm     beige IBM Model M style 101 key, tall caps, big case
  keyboard_macext     Apple Extended Keyboard II style, platinum, dark modifiers
  keyboard_imac       compact bondi-teal USB keyboard (iMac G3 era), no numpad
  keyboard_tkl        mid-grey tenkeyless (no numpad), laptop-style low keys
  keyboard_zx         ZX Spectrum style: rubber key matrix with coloured legends

Origin: centre of the footprint on the table, z = 0 on the table; the user sits at -Y, the cable leaves at +Y.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmesh
import bpy
from mathutils import Matrix, Vector

import propslib as P

U = 0.01905           # key pitch


# ------------------------------------------------------------ layouts (key units)
# each key: (label, width, [secondary legend], [kind]); None = gap; ("gap", u)

def _row(items):
    return items


F_ROW = ["Esc", ("gap", 1), "F1", "F2", "F3", "F4", ("gap", .5), "F5", "F6", "F7", "F8", ("gap", .5), "F9", "F10",
         "F11", "F12", ("gap", .25), "PrtSc", "ScrLk", "Pause"]

RU = dict(zip("QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,.", "ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ"))


def full_layout(win=True, numpad=True, modern_bottom=True):
    rows = [[], [], [], [], [], []]
    rows[0] = F_ROW[:]
    r1 = ["`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="]
    rows[1] = [(k, 1) for k in r1] + [("Bksp", 2), ("gap", .25), ("Ins", 1), ("Home", 1), ("PgUp", 1)]
    rows[2] = [("Tab", 1.5)] + [(k, 1) for k in "QWERTYUIOP[]"] + [("\\", 1.5), ("gap", .25), ("Del", 1), ("End", 1),
                                                                      ("PgDn", 1)]
    rows[3] = [("Caps", 1.75)] + [(k, 1) for k in "ASDFGHJKL;'"] + [("Enter", 2.25)]
    rows[4] = [("Shift", 2.25)] + [(k, 1) for k in "ZXCVBNM,./"] + [("Shift", 2.75)]
    if win:
        rows[5] = [("Ctrl", 1.25), ("Win", 1.25), ("Alt", 1.25), ("", 6.25), ("Alt", 1.25), ("Win", 1.25),
                   ("Menu", 1.25), ("Ctrl", 1.25)]
    else:
        rows[5] = [("Ctrl", 1.5), ("", 1), ("Alt", 1.5), ("", 6), ("Alt", 1.5), ("", 1), ("Ctrl", 1.5)]
    return rows


# ------------------------------------------------------------ geometry

def keycap_bm(bm, w, d, h, taper, dish, bev, tilt):
    """One keycap in `bm`, bottom at z=0 centred on the origin, then tilted about X."""
    wt, dt = w - 2 * taper, d - 2 * taper * 1.15
    pts = [(-w / 2, -d / 2, 0), (w / 2, -d / 2, 0), (w / 2, d / 2, 0), (-w / 2, d / 2, 0),
           (-wt / 2, -dt / 2, h), (wt / 2, -dt / 2, h), (wt / 2, dt / 2, h), (-wt / 2, dt / 2, h)]
    old = set(bm.verts)
    vs = [bm.verts.new(p) for p in pts]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    fs = [bm.faces.new([vs[i] for i in f]) for f in faces]
    top = fs[1]
    if dish:
        bmesh.ops.inset_region(bm, faces=[top], thickness=min(wt, dt) * 0.16, depth=-dish)
    if bev:
        edges = list({e for v in vs for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=edges, offset=bev * 1.8, segments=3, affect="EDGES")
    new = [v for v in bm.verts if v not in old]
    if tilt:
        bmesh.ops.rotate(bm, verts=new, cent=(0, 0, 0), matrix=Matrix.Rotation(math.radians(tilt), 3, "X"))
    return new


def legend_cache():
    return {}


class Legends:
    def __init__(self):
        self.cache = {}

    def mesh(self, label, size):
        key = (label, round(size, 5))
        if key not in self.cache:
            ob = P.text_mesh("lg", label, size, (0, 0, 0), P.mat("_tmp", (1, 1, 1)), extrude=0.00012)
            me = ob.data
            self.cache[key] = ([v.co.copy() for v in me.vertices], [tuple(p.vertices) for p in me.polygons])
            bpy.data.objects.remove(ob)
        return self.cache[key]

    def add(self, bm, label, size, loc, tilt, align=(0, 0)):
        vs, fs = self.mesh(label, size)
        rot = Matrix.Rotation(math.radians(tilt), 3, "X")
        new = [bm.verts.new(rot @ (v + Vector((align[0], align[1], 0))) + Vector(loc)) for v in vs]
        for f in fs:
            try:
                bm.faces.new([new[i] for i in f])
            except ValueError:
                pass


def wedge(name, w, d, hf, hb, material, parent, r=0.003, tile=0.3):
    """Keyboard case: footprint w x d, height hf at the front (-Y), hb at the back."""
    bm = bmesh.new()
    pts = [(-w / 2, -d / 2, 0), (w / 2, -d / 2, 0), (w / 2, d / 2, 0), (-w / 2, d / 2, 0),
           (-w / 2, -d / 2, hf), (w / 2, -d / 2, hf), (w / 2, d / 2, hb), (-w / 2, d / 2, hb)]
    vs = [bm.verts.new(p) for p in pts]
    for f in [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]:
        bm.faces.new([vs[i] for i in f])
    bmesh.ops.bevel(bm, geom=list(bm.edges), offset=r, segments=3, profile=0.5, affect="EDGES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return P._make(name, bm, material, parent, tile, 40)


STYLES = {
    "modern": dict(win=True, numpad=True, cap="black", legend="grey", H=0.0095, taper=0.0022, dish=0.0006, bev=0.0007,
                   case="black", margin=0.009, angle=6, cyr=True, tilts=(0, 7, 3, 0, -3, -7), hf=0.016, cable="black"),
    "gaming": dict(win=True, numpad=True, cap="black", legend="glow", H=0.0105, taper=0.002, dish=0.0007, bev=0.0007,
                   case="black", margin=0.008, angle=7, cyr=False, tilts=(0, 8, 4, 0, -4, -8), hf=0.019, glow=True,
                   cable="braid"),
    "modelm": dict(win=False, numpad=True, cap="beige", legend="dark", H=0.0155, taper=0.0016, dish=0.0008, bev=0.0006,
                   case="beige", margin=0.017, angle=7, cyr=True, tilts=(0, 6, 3, 0, -3, -6), hf=0.022,
                   cable="beigecoil", tallcase=True),
    "macext": dict(win=True, numpad=True, cap="platinum", legend="dark", H=0.0125, taper=0.0018, dish=0.0006, bev=0.0008,
                   case="platinum", margin=0.012, angle=5, cyr=False, tilts=(0, 6, 3, 0, -3, -6), hf=0.017,
                   cable="grey", mac=True),
    "tkl": dict(win=True, numpad=False, cap="grey", legend="dark", H=0.007, taper=0.0018, dish=0.0004, bev=0.0006,
                case="grey", margin=0.008, angle=4, cyr=True, tilts=(0, 3, 2, 0, -2, -3), hf=0.012, cable="grey"),
    "imac": dict(win=False, numpad=False, cap="tealglass", legend="dark", H=0.010, taper=0.0016, dish=0.0006, bev=0.0008,
                 case="teal", margin=0.01, angle=4, cyr=False, tilts=(0, 4, 2, 0, -2, -4), hf=0.015, cable="grey",
                 compact=True),
}


def materials(style):
    s = style
    M = {}
    M["black"] = P.mat("KB_PlasticBlack", (0.025, 0.025, 0.028), 0.42)
    M["grey"] = P.tex("KB_PlasticGrey", "laminate_white.png", 0.5, size=256, tint=(0.45, 0.45, 0.46), tilemult=2.0)
    M["beige"] = P.tex("KB_PlasticBeige", "plastic_beige.png", 0.5, size=512, tint=(0.66, 0.67, 0.62),
                       fallback=(0.72, 0.66, 0.53), tilemult=4.0)
    M["platinum"] = P.tex("KB_Platinum", "laminate_white.png", 0.45, size=256, tint=(0.9, 0.88, 0.82), tilemult=2.0)
    M["teal"] = P.mat("KB_Teal", (0.02, 0.3, 0.38), 0.12)
    M["tealglass"] = P.mat("KB_TealGlass", (0.55, 0.72, 0.75), 0.18)
    M["dkcap"] = P.mat("KB_DarkKey", (0.12, 0.12, 0.13), 0.45)
    M["dkmod"] = P.mat("KB_PlatinumDark", (0.28, 0.28, 0.3), 0.45)
    M["legend_grey"] = P.mat("KB_LegendGrey", (0.75, 0.75, 0.75), 0.5)
    M["legend_dark"] = P.mat("KB_LegendDark", (0.07, 0.07, 0.08), 0.5)
    M["legend_glow"] = P.mat("KB_LegendGlow", (0, 0, 0), 0.5, emit=(0.9, 0.3, 1.0), strength=5.0)
    M["rubber"] = P.mat("KB_Rubber", (0.015, 0.015, 0.015), 0.85)
    M["cable"] = P.mat("KB_Cable", (0.02, 0.02, 0.02), 0.6)
    M["metal"] = P.mat("KB_Metal", (0.6, 0.6, 0.62), 0.3, 1.0)
    return M


def build_full(name, sty):
    s = STYLES[sty]
    M = materials(s)
    root = P.empty(name)
    rows = full_layout(win=s["win"])
    if s.get("mac"):
        rows[5] = [("Ctrl", 1.25), ("Opt", 1.25), ("Cmd", 1.25), ("", 6.25), ("Cmd", 1.25), ("Opt", 1.25),
                   ("Ctrl", 1.25)]
        rows[3][0] = ("Caps", 1.75)
        rows[3][-1] = ("Return", 2.25)
        rows[1][-4:-3] = [("Delete", 2)]
        rows[4][0] = ("Shift", 2.25)
    # arrows & numpad are attached to rows 4/5 and rows 1..5
    arrows = {4: [("gap", .25), ("gap", 1), ("Up", 1), ("gap", 1)], 5: [("gap", .25), ("Left", 1), ("Down", 1), ("Right", 1)]}
    numpad = {1: ["Num", "/", "*", "-"], 2: ["7", "8", "9", "+"], 3: ["4", "5", "6"], 4: ["1", "2", "3", "Ent"],
              5: [("0", 2), ".", None]}
    ypos = {0: 0.0, 1: -1.5, 2: -2.5, 3: -3.5, 4: -4.5, 5: -5.5}
    keys = []        # (label, x_u(centre), y_u(centre), w, h, row)
    for r, row in enumerate(rows):
        x = 0.0
        seq = list(row)
        if r in arrows:
            seq += arrows[r]
        if s["numpad"] and r >= 1:
            seq.append(("gap", .25))
            for it in numpad.get(r, []):
                seq.append(it)
        for it in seq:
            if isinstance(it, str):
                it = (it, 1)
            if it is None:
                x += 1
                continue
            lab, w = it
            if lab == "gap":
                x += w
                continue
            hh = 1
            keys.append([lab, x + w / 2, ypos[r], w, hh, r])
            x += w
    # tall numpad keys + and Enter
    if s["numpad"]:
        for k in keys:
            if k[0] == "+":
                k[2] = ypos[2] - 0.5
                k[4] = 2
            if k[0] == "Ent":
                k[2] = ypos[4] - 0.5 + 0.0
                k[4] = 2
                k[0] = "Enter"
        # fix: row 5 numpad "." must exist, Ent covers rows 4+5 right column; drop row-5 right placeholder
    # keys list may contain row-3 numpad without 4th key (+ spans rows 2-3) - fine
    if not s["numpad"]:
        pass
    if s.get("compact"):
        # iMac G3 keyboard: no F-row spacing quirks - keep Esc row but drop PrtSc/Scr/Pause/Ins block
        keys = [k for k in keys if k[0] not in ("PrtSc", "ScrLk", "Pause", "Ins", "Home", "PgUp", "Del", "End", "PgDn")]
    if s.get("compact"):
        # recompute x so that no gaps remain after dropping the nav block: shift arrow cluster next to main block
        mx = max(k[1] + k[3] / 2 for k in keys if k[5] in (1, 2, 3) and k[0] in ("Bksp", "\\", "Enter"))
        for k in keys:
            if k[0] in ("Up", "Left", "Down", "Right"):
                k[1] = mx + .25 + {"Left": .5, "Down": 1.5, "Right": 2.5, "Up": 1.5}[k[0]]
        # place Bksp row to hold "Del" is skipped
    minx = min(k[1] - k[3] / 2 for k in keys)
    maxx = max(k[1] + k[3] / 2 for k in keys)
    miny = min(k[2] - k[4] / 2 for k in keys)
    maxy = max(k[2] + k[4] / 2 for k in keys)
    cx, cy = (minx + maxx) / 2, (miny + maxy) / 2
    mg = s["margin"]
    W = (maxx - minx) * U + 2 * mg
    D = (maxy - miny) * U + 2 * mg + (0.018 if s.get("mac") or s["case"] == "beige" else 0.0)
    angle = s["angle"]
    hf, hb = s["hf"], s["hf"] + D * math.tan(math.radians(angle))
    case_m = M[s["case"]]
    case = wedge(name + "_Case", W, D, hf, hb, case_m, root, r=0.004 if sty != "modelm" else 0.003)
    case.location = (0, 0, 0)

    # caps in a frame tilted like the case top
    board = P.empty("Board", (0, -D / 2, hf), (math.radians(angle), 0, 0), root)
    legends = Legends()
    cap_bm = {"cap": bmesh.new(), "mod": bmesh.new()}
    leg_bm = bmesh.new()
    lamp_bm = bmesh.new()
    cap_bm_names = {"cap": "cap", "mod": "mod"}
    fkeys_h = s["H"]
    for lab, kx, ky, w, h, r in keys:
        x = (kx - cx) * U
        y = (ky - cy) * U + D / 2 + (0.008 if (s.get("mac") or s["case"] == "beige") else 0.0)
        w_m = w * U - 0.0012
        d_m = h * U - 0.0012
        mod = lab in ("Esc", "Tab", "Caps", "Shift", "Ctrl", "Alt", "Win", "Menu", "Bksp", "Enter", "Ent", "Opt", "Cmd",
                      "Delete", "Return", "Num")
        use_mod = ("mod" if (mod and (s["cap"] in ("platinum",) or s["cap"] == "beige" and False)) else "cap")
        H = s["H"] * (0.9 if r == 0 else 1.0)
        tilt = s["tilts"][r] - angle * 0     # tilt is relative to the board (already tilted)
        bm_t = cap_bm[use_mod]
        for v in keycap_bm(bm_t, w_m, d_m, H, s["taper"], s["dish"], s["bev"], tilt):
            v.co += Vector((x, y, 0.0004))
        # legends
        top = H
        cz = 0.0004 + top * math.cos(math.radians(tilt)) + (y - 0) * 0  # approximate: legend sits just above the dish
        zc = 0.0004 + H * math.cos(math.radians(tilt)) + 0.00015
        yc = y + H * math.sin(math.radians(tilt)) * -1.0
        if lab:
            txt = {"Bksp": "Bksp", "Ent": "Enter", "PrtSc": "PrtSc", "ScrLk": "ScrLk", "Num": "Num",
                   "Up": "▲", "Down": "▼", "Left": "◀", "Right": "▶"}.get(lab, lab)
            if txt in ("▲", "▼", "◀", "▶"):
                txt = {"▲": "↑", "▼": "↓", "◀": "←", "▶": "→"}[txt]
            if s.get("mac"):
                txt = {"Opt": "opt", "Cmd": "cmd", "Ctrl": "ctrl", "Return": "return", "Delete": "delete", "Caps": "caps",
                       "Shift": "shift", "Tab": "tab", "Esc": "esc", "Bksp": "delete", "Enter": "enter"}.get(lab, txt)
            long_ = len(txt) > 1
            size = (0.0048 if not long_ else 0.0034) * (1.0 if sty != "tkl" else 0.9)
            if len(txt) > 4:
                size *= 0.8
            single = len(txt) == 1
            if single and txt.isalpha():
                ypos2 = (-0.0012 if s["cyr"] and txt in RU else 0.0)
                legends.add(leg_bm, txt, size * 1.1, (x - (0.0022 if (s["cyr"] and txt in RU) else 0), y + ypos2 + 0.0008, zc), tilt)
                if s["cyr"] and txt in RU:
                    legends.add(leg_bm, RU[txt], size * 0.75, (x + 0.004, y - 0.0035, zc - 0.0001), tilt)
            elif single and txt in "1234567890-=`[]\\;',./" and r in (1, 2, 3, 4):
                top_sym = dict(zip("1234567890-=`[]\\;',./", "!@#$%^&*()_+~{}|:\"<>?"))[txt]
                legends.add(leg_bm, top_sym, size * 0.95, (x, y + 0.0042, zc), tilt)
                legends.add(leg_bm, txt, size * 0.95, (x, y - 0.0028, zc), tilt)
            else:
                if lab == "Enter" and h == 2:
                    y_off = 0.0
                else:
                    y_off = 0.0
                legends.add(leg_bm, txt, size, (x - (w_m * 0.18 if long_ and w > 1.2 and r not in (0,) and lab in ("Tab", "Caps", "Shift", "Bksp", "Enter", "Return", "Delete") else 0),
                                                y + y_off + 0.001, zc), tilt)
    # materials
    capmat = {"black": M["black"], "grey": M["grey"], "beige": M["beige"], "platinum": M["platinum"],
              "tealglass": M["tealglass"]}[s["cap"]]
    modmat = M["dkmod"] if s["cap"] == "platinum" else capmat
    legmat = M["legend_" + s["legend"]]
    for key, bm in cap_bm.items():
        if not bm.verts:
            bm.free()
            continue
        mm = capmat if key == "cap" else modmat
        ob = P._make(name + "_Keycaps_" + key, bm, mm, board, 0.05, 40)
    P._make(name + "_Legends", leg_bm, legmat, board, 0.05, None)
    lamp_bm.free()

    # glow under keys
    if s.get("glow"):
        n = 4
        cols = [(1.0, 0.1, 0.5), (0.6, 0.1, 1.0), (0.1, 0.5, 1.0), (0.1, 1.0, 0.6)]
        for i in range(n):
            w_ = (W - 2 * mg * 0.8) / n
            lm = P.led("KB_Glow%d" % i, cols[i], 4.0)
            P.rbox(name + "_Glow%d" % i, (w_, D - 2 * mg * 0.8, 0.0006), ((i - (n - 1) / 2) * w_, D / 2, 0.0002), lm, board)
        # legends glow with the same hue as the nearest segment: keep one emissive material
    # cable
    cab = M["cable"]
    cabr = 0.0022
    start = Vector((0, D / 2 - 0.002, hf + 0.006))
    if s["cable"] == "beigecoil":
        pts = [(0, D / 2, hf * 0.5), (0, D / 2 + 0.04, 0.003)]
        n = 60
        for i in range(n):
            t = i / (n - 1)
            pts.append((math.sin(t * 40) * 0.008, D / 2 + 0.05 + t * 0.35, 0.008 + math.cos(t * 40) * 0.008))
        mt = M["beige"]
        P.tube(name + "_Cable", pts, 0.0022, mt, root, seg=6, smooth_pts=False)
    else:
        P.tube(name + "_Cable", [(0, D / 2 - 0.002, hf * 0.6), (0, D / 2 + 0.03, 0.004), (0.03, D / 2 + 0.12, 0.003),
                                 (-0.05, D / 2 + 0.25, 0.003), (0.0, D / 2 + 0.38, 0.003)], cabr, M["black"] if s["cable"] != "grey" else M["grey"], root, seg=8)
    P.rbox(name + "_Plug", (0.012, 0.018, 0.005), (0.0, D / 2 + 0.39, 0.003), M["metal"], root, r=0.0008)
    # feet
    for sx in (-1, 1):
        P.rbox(name + "_Foot", (0.02, 0.012, 0.003), (sx * (W / 2 - 0.02), D / 2 - 0.012, -0.0005), M["rubber"], root, r=0.001)
        P.rbox(name + "_Foot", (0.02, 0.012, 0.003), (sx * (W / 2 - 0.02), -D / 2 + 0.012, -0.0005), M["rubber"], root, r=0.001)
    # badge / LED row
    for i, c in enumerate([(0.1, 1.0, 0.2), (0.1, 1.0, 0.2), (0.1, 1.0, 0.2)]):
        P.rbox(name + "_LED%d" % i, (0.004, 0.001, 0.002), (W / 2 - 0.12 + i * 0.014 - 0.0, -D / 2 + 0.003 + 0.0, hf - 0.0), P.led("KB_Green", c, 3.0), root) if False else None
    return root, (W, D, hb + s["H"] + 0.002)


def build_zx(name):
    root = P.empty(name)
    W, D = 0.233, 0.145
    case = P.mat("ZX_Case", (0.02, 0.02, 0.022), 0.5)
    rubber = P.mat("ZX_Rubber", (0.55, 0.55, 0.56), 0.9)
    leg = P.mat("ZX_Legend", (0.9, 0.9, 0.9), 0.5)
    c = wedge(name + "_Case", W, D, 0.014, 0.03, case, root, r=0.003)
    ang = math.atan2(0.016, D)
    board = P.empty("Board", (0, -D / 2, 0.014), (ang, 0, 0), root)
    cb = bmesh.new()
    lg = bmesh.new()
    L = Legends()
    rows = ["1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"]
    pitch = 0.0215
    for ri, row in enumerate(rows):
        for ci, ch in enumerate(row):
            x = (ci - 4.5) * pitch + (ri * 0.0 if ri != 2 else 0.5 * pitch) + ({0: 0, 1: 0, 2: 0, 3: 0}[ri]) * pitch
            if ri == 3:
                x = (ci - 4.0 + 0.5) * pitch - 0.0
            y = D / 2 + (1.5 - ri) * pitch - 0.012 - 0.003
            for v in keycap_bm(cb, 0.0188, 0.0188, 0.0045, 0.0008, 0.0003, 0.001, 0):
                v.co += Vector((x, y, 0.0004))
            L.add(lg, ch, 0.006, (x, y, 0.0004 + 0.0045 - 0.0003 + 0.0002), 0)
    P._make(name + "_Keys", cb, rubber, board, 0.05, 40)
    P._make(name + "_Legends", lg, P.mat("ZX_LegendDark", (0.05, 0.05, 0.05), 0.5), board, 0.05, None)
    # rainbow stripe
    for i, col in enumerate([(0.85, 0.05, 0.05), (0.95, 0.65, 0.05), (0.1, 0.65, 0.15), (0.1, 0.35, 0.9)]):
        P.rbox(name + "_Stripe", (0.004, 0.012, 0.0006), (W / 2 - 0.02 + i * 0.0045 - 0.0, -D / 2 + 0.012, 0.0165),
               P.mat("ZX_Stripe%d" % i, col, 0.4), root, rot=(0, 0, math.radians(-25)))
    P.tube(name + "_Cable", [(0, D / 2, 0.01), (0, D / 2 + 0.05, 0.004), (0.04, D / 2 + 0.15, 0.003), (0, D / 2 + 0.28, 0.003)],
           0.0025, P.mat("ZX_Cable", (0.02, 0.02, 0.02), 0.6), root)
    return root, (W, D, 0.04)


BUILDERS = {
    "keyboard_modern": lambda n: build_full(n, "modern"),
    "keyboard_gaming": lambda n: build_full(n, "gaming"),
    "keyboard_modelm": lambda n: build_full(n, "modelm"),
    "keyboard_macext": lambda n: build_full(n, "macext"),
    "keyboard_imac": lambda n: build_full(n, "imac"),
    "keyboard_tkl": lambda n: build_full(n, "tkl"),
    "keyboard_zx": build_zx,
}


def main():
    names = [a for a in ARGS if not a.startswith("--")] if (ARGS := P.ARGS) else []
    names = names or list(BUILDERS)
    render = "--no-render" not in P.ARGS
    for nm in names:
        scn = P.reset()
        root, size = BUILDERS[nm](nm)
        # bake: one mesh per material is already the case (keycaps/legends/case separate), keep hierarchy
        P.export(root, nm)
        n, tris = P.stats(root)
        print("BUILT %s size=%.3f x %.3f x %.3f m  objects=%d tris=%d" % (nm, size[0], size[1], size[2], n, tris))
        if render:
            P.setup_studio(scn)
            P.render(scn, os.path.join(P.RENDER_DIR, nm + ".png"), (0, 0.0, 0.02), (0.0, -0.36, 0.52), 50)


if __name__ == "__main__":
    main()
