"""The three training dinghies of the sailing school as numbers (docs/yacht_school.md, part C). Plain Python (no bpy): the
Blender builders (boats.py), the sockets writer (this file, run directly) and anyone else read it.

BOAT FRAME (every boat part, every socket): metres; origin on the static WATERLINE, AMIDSHIPS (x = 0 at LOA / 2) on the
centreline; +X FORWARD (bow), +Y PORT, +Z up. That is the repo's convention for script-built boats and vehicles (the
north's rowboat: bow at +X; the diner: x -> Unreal X, y -> Unreal -Y). Exported with export_yup, Unreal gets
X = x (forward), Y = -y (Unreal +Y = STARBOARD), Z = z, x100 cm. sockets.json gives both ("pos" Blender metres, "ue_cm").

Proportions after the real boats (class rules / known measurements, rounded):
  luch      Laser-like single-hander: 4.23 x 1.37 m, unstayed two-part mast (63.5 / 50 mm), sleeve-luff sail 7.06 m^2,
            daggerboard, kick-up-less rudder, long shallow self-draining cockpit, mainsheet to a transom traveller.
  optimist  the 2.30 x 1.13 m children's pram: flat-bottomed box with a bow transom, sprit rig 3.3 m^2 laced to the mast,
            daggerboard, buoyancy bags, mast thwart and centre thwart.
  cadet     the 3.22 x 1.27 m two-hander: pram bow, hard chine, stayed mast with main + jib (5.2 m^2), pivoting
            centreboard, rudder and tiller; helm and crew.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))

BOATS = {
    "luch": {
        "title": "Luch", "loa": 4.23, "beam": 1.37, "crew": 1,
        # hull: (x fraction from transom 0 .. bow 1) -> values, piecewise-linear through these keys
        "sheer_z":   [(0.0, 0.20), (0.5, 0.27), (0.85, 0.33), (1.0, 0.37)],      # deck edge above the waterline
        "keel_z":    [(0.0, -0.06), (0.35, -0.13), (0.6, -0.13), (0.88, -0.05), (1.0, 0.18)],
        "half_beam": [(0.0, 0.57), (0.40, 0.685), (0.62, 0.66), (0.80, 0.52), (0.92, 0.30), (1.0, 0.012)],
        "chine_frac": 0.86,          # bilge (chine) half-width as a fraction of the deck half-beam
        "chine_up": 0.40,            # chine height between keel and sheer (fraction)
        "deadrise": 0.05,            # bottom rise from keel to chine, m
        "round": 0.6,                # bilge rounding 0 hard chine .. 1 soft
        "bow": "point", "deck": "decked", "crown": 0.05,
        "cockpit": {"x0": -1.98, "x1": -0.62, "half_w": 0.33, "floor_z": 0.07},
        "mast_x": 1.13, "mast_d": 0.0635, "mast_d_top": 0.050, "mast_joint_z": 3.10, "mast_top_z": 6.33,
        "mast_foot_z": -0.05,        # the step tube in the hull: the mast stands in it to this depth
        "gooseneck_z": 1.15, "boom_len": 2.74, "boom_d": 0.055,
        # mainsail outline in the MAIN frame: x aft = negative from the mast axis, z absolute (boat frame)
        "main": {"tack": (0.0, 1.20), "head": (0.0, 5.95), "clew": (-2.52, 1.22), "roach": 0.24, "battens": 3,
                 "head_w": 0.12, "window": (0.30, 0.55, 0.40, 0.62)},
        "rudder": {"x": -2.115 - 0.04, "z_top": 0.30, "z_bot": -0.92, "chord": 0.26, "stock_h": 0.42,
                   "tiller_len": 1.02, "tiller_z": 0.38, "ext_len": 1.00},
        "board": {"kind": "dagger", "x": 0.30, "chord": 0.30, "len": 1.30, "top_z": 0.42, "thick": 0.024},
        "strap": [(-1.80, 0.0, 0.10), (-0.80, 0.0, 0.10)],
        "traveller": {"x": -1.98, "half_w": 0.42},
        "ratchet": (-0.95, 0.0, 0.10),
        "boom_block": -1.10,          # mainsheet block on the boom, m aft of the mast axis
        "sail_area": 7.06,
    },
    "optimist": {
        "title": "Optimist", "loa": 2.30, "beam": 1.13, "crew": 1,
        "sheer_z":   [(0.0, 0.30), (0.5, 0.33), (1.0, 0.42)],
        "keel_z":    [(0.0, -0.03), (0.30, -0.09), (0.62, -0.09), (0.88, 0.02), (1.0, 0.16)],
        "half_beam": [(0.0, 0.47), (0.45, 0.565), (0.62, 0.56), (0.85, 0.47), (1.0, 0.34)],
        "chine_frac": 0.76, "chine_up": 0.0, "deadrise": 0.0, "round": 0.0,
        "bow": "pram", "deck": "open", "crown": 0.0,
        "mast_x": 0.70, "mast_d": 0.045, "mast_d_top": 0.040, "mast_joint_z": None, "mast_top_z": 2.30,
        "mast_foot_z": -0.07,
        "gooseneck_z": 0.66, "boom_len": 2.02, "boom_d": 0.042,
        # sprit sail: tack, clew, peak, throat (the head between peak and throat)
        "main": {"tack": (0.0, 0.70), "throat": (0.0, 2.22), "peak": (-1.62, 2.62), "clew": (-1.98, 0.70), "roach": 0.10,
                 "battens": 0, "window": None},
        "sprit": {"snotter_z": 1.02, "d": 0.035},
        "rudder": {"x": -1.15 - 0.03, "z_top": 0.30, "z_bot": -0.62, "chord": 0.24, "stock_h": 0.40,
                   "tiller_len": 0.78, "tiller_z": 0.38, "ext_len": 0.75},
        "board": {"kind": "dagger", "x": 0.06, "chord": 0.26, "len": 0.98, "top_z": 0.40, "thick": 0.020},
        "strap": [(-0.70, 0.0, -0.05), (0.00, 0.0, -0.05)],
        "thwart_x": 0.12, "mast_thwart_x": 0.70,
        "ratchet": (-0.25, 0.0, -0.04),
        "boom_block": -1.05,
        "sail_area": 3.3,
    },
    "cadet": {
        "title": "Cadet", "loa": 3.22, "beam": 1.27, "crew": 2,
        "sheer_z":   [(0.0, 0.31), (0.5, 0.36), (1.0, 0.47)],
        "keel_z":    [(0.0, -0.05), (0.35, -0.12), (0.6, -0.12), (0.88, -0.02), (1.0, 0.20)],
        "half_beam": [(0.0, 0.50), (0.42, 0.635), (0.62, 0.62), (0.85, 0.47), (1.0, 0.24)],
        "chine_frac": 0.80, "chine_up": 0.30, "deadrise": 0.07, "round": 0.0,
        "bow": "pram", "deck": "foredeck", "crown": 0.03,
        "foredeck_x": 0.75, "side_deck": 0.13,
        "mast_x": 0.62, "mast_d": 0.058, "mast_d_top": 0.045, "mast_joint_z": None, "mast_top_z": 5.25,
        "mast_foot_z": -0.10,
        "gooseneck_z": 1.12, "boom_len": 2.15, "boom_d": 0.05,
        "main": {"tack": (0.0, 1.16), "head": (0.0, 4.95), "clew": (-1.90, 1.18), "roach": 0.16, "battens": 3,
                 "head_w": 0.08, "window": None},
        "jib": {"tack": (1.555, 0.52), "head": (0.672, 3.70), "clew": (0.55, 0.80)},
        "forestay": {"bow": (1.58, 0.0, 0.47), "hound_z": 3.85},
        "shrouds": {"x": 0.42, "y": 0.60, "z": 0.36},
        "rudder": {"x": -1.61 - 0.03, "z_top": 0.30, "z_bot": -0.80, "chord": 0.26, "stock_h": 0.42,
                   "tiller_len": 0.95, "tiller_z": 0.40, "ext_len": 0.85},
        "board": {"kind": "centre", "x": 0.12, "chord": 0.36, "len": 0.95, "top_z": 0.30, "thick": 0.020,
                  "pivot": (0.30, -0.10)},
        "strap": [(-1.20, 0.30, -0.06), (-0.10, 0.30, -0.06)],
        "thwart_x": -0.05,
        "ratchet": (-0.55, 0.0, -0.02),
        "boom_block": -1.10,
        "sail_area": 5.2,
    },
}


def lerp_keys(keys, t):
    """Piecewise-linear value of [(t, v), ...] at t (clamped)."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t <= t1:
            a = (t - t0) / max(t1 - t0, 1e-9)
            a = a * a * (3 - 2 * a) * 0.5 + a * 0.5      # a little smoothing between keys
            return v0 + (v1 - v0) * a
    return keys[-1][1]


def sheer(b, x):
    s = BOATS[b]
    return lerp_keys(s["sheer_z"], (x + s["loa"] / 2) / s["loa"])


def half_beam(b, x):
    s = BOATS[b]
    return lerp_keys(s["half_beam"], (x + s["loa"] / 2) / s["loa"])


def deck_z(b, x, y=0.0):
    """Deck surface height (decked hulls crown toward the centreline)."""
    s = BOATS[b]
    hb = max(half_beam(b, x), 1e-3)
    return sheer(b, x) + s.get("crown", 0.0) * max(0.0, 1.0 - (y / hb) ** 2)


# ------------------------------------------------------------------------------------------------ sail geometry
def _l(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def sail_edges(b, which="main"):
    """(luff0, luff1, leech0, leech1, roach, foot_round) of a sail in boat x/z (x aft = smaller)."""
    m = BOATS[b][which]
    if which == "jib":
        t, h, c = m["tack"], m["head"], m["clew"]
        return t, h, c, (h[0] - 0.04, h[1] - 0.03), -0.04, 0.03
    if "peak" in m:
        return m["tack"], m["throat"], m["clew"], m["peak"], m["roach"], 0.04
    hd = m["head"]
    return m["tack"], hd, m["clew"], (hd[0] - m.get("head_w", 0.1), hd[1] - 0.02), m["roach"], 0.05


def sail_point(b, sv, hv, which="main"):
    """Point (x, z) on the flat sail at (s = 0 luff .. 1 leech, h = 0 foot .. 1 head)."""
    L0, L1, E0, E1, roach, foot = sail_edges(b, which)
    L = _l(L0, L1, hv)
    E = _l(E0, E1, hv)
    # roach: the leech bows out, perpendicular to the straight leech and away from the luff, by sin(pi h)
    dx, dz = E1[0] - E0[0], E1[1] - E0[1]
    n = math.hypot(dx, dz)
    nx, nz = dz / n, -dx / n
    if nx > 0:
        nx, nz = -nx, -nz
    bulge = roach * math.sin(math.pi * hv) ** 0.9 * sv ** 2
    p = _l(L, E, sv)
    p = (p[0] + nx * bulge, p[1] + nz * bulge)
    # foot round: the foot sags a little between tack and clew
    return (p[0], p[1] - foot * math.sin(math.pi * sv) * max(0.0, 1.0 - hv * 8.0))


def sail_box(b, which="main"):
    """UV box of a sail picture (xmin, xmax, zmin, zmax); u = (xmax - x) / W (0 at the luff), v = (z - zmin) / H."""
    pts = [sail_point(b, i / 20.0, j / 20.0, which) for i in range(21) for j in range(21)]
    xs, zs = [p[0] for p in pts], [p[1] for p in pts]
    return min(xs) - 0.01, max(xs) + 0.01, min(zs) - 0.01, max(zs) + 0.01


def sail_uv(b, p, which="main"):
    x0, x1, z0, z1 = sail_box(b, which)
    return ((x1 - p[0]) / (x1 - x0), (p[1] - z0) / (z1 - z0))


# where things sit on each sail, in (s, h): emblem centre + size (fraction of the sail's height), number boxes per side
# (starboard higher, as the rules have it), number height (fraction of the sail's height), battens (h), panels
SAIL_MARKS = {
    "luch": {"emblem": (0.56, 0.79, 0.07), "num_stbd": (0.40, 0.655), "num_port": (0.40, 0.555), "num_h": 0.055,
             "battens": [0.30, 0.52, 0.74], "batten_len": 0.20, "panels": 11},
    "optimist": {"emblem": (0.62, 0.80, 0.15), "num_stbd": (0.45, 0.50), "num_port": (0.45, 0.33), "num_h": 0.10,
                 "battens": [], "batten_len": 0.0, "panels": 6},
    "cadet": {"emblem": (0.40, 0.82, 0.08), "num_stbd": (0.40, 0.62), "num_port": (0.40, 0.50), "num_h": 0.06,
              "battens": [0.30, 0.55, 0.78], "batten_len": 0.22, "panels": 9},
    "cadet_jib": {"emblem": None, "battens": [], "batten_len": 0.0, "panels": 5},
}


def sail_area(b, which="main", n=40):
    """Area of the flat sail, m^2."""
    a = 0.0
    for i in range(n):
        for j in range(n):
            p = [sail_point(b, (i + di) / n, (j + dj) / n, which) for di, dj in ((0, 0), (1, 0), (1, 1), (0, 1))]
            a += 0.5 * abs(sum(p[k][0] * p[(k + 1) % 4][1] - p[(k + 1) % 4][0] * p[k][1] for k in range(4)))
    return a


def ue(p):
    return [round(p[0] * 100, 1), round(-p[1] * 100, 1), round(p[2] * 100, 1)]


def sockets():
    """Crew and rigging points per class, boat frame. Port (+Y) variants; starboard = the same with y negated (also
    listed). Engine / animation (parts D, E) attach the body by these; hand grips on moving parts are given in the PART's
    own frame (origin = its pivot) and marked "frame": "<part>"."""
    out = {"_doc": ("Boat frame: metres, origin at the static waterline amidships on the centreline, +X forward, +Y PORT, "
                    "+Z up (Blender). ue_cm = Unreal cm in the boat actor (X fwd, Y = -y: Unreal +Y is STARBOARD, Z up). "
                    "Seat/hike positions are the PELVIS (hip joint midpoint) of a 1.75 m adult (luch) or the class's crew; "
                    "feet = between the two ankles; facing = the yaw the torso faces (deg, 0 = forward, +90 = to port). "
                    "Entries with 'frame' are in that part's own frame (its origin = its pivot, see parts). Side variants: "
                    "_port / _stbd (sitting on the port / starboard side deck, i.e. starboard / port tack when hiking)."),
           "frame": {"forward": "+X", "port": "+Y", "up": "+Z", "units": "m", "origin": "waterline amidships"}}
    for b, s in BOATS.items():
        d = {}
        parts = {}
        # ---- part pivots (the origin of each part mesh, in the boat frame) ----
        mx = s["mast_x"]
        parts["Hull"] = {"pivot": [0, 0, 0], "note": "boat origin"}
        parts["Mast"] = {"pivot": [mx, 0, round(s["mast_foot_z"], 3)], "axis": [0, 0, 1],
                         "note": "foot of the mast in its step; the mast rotates about its own axis (luch) / is fixed"}
        parts["Boom"] = {"pivot": [mx, 0, s["gooseneck_z"]], "axis": [0, 0, 1],
                         "note": "gooseneck ON THE MAST AXIS; the boom lies along -X; yaw = sheeting angle (+ to port)"}
        parts["Main"] = {"pivot": [mx, 0, s["gooseneck_z"]], "axis": [0, 0, 1],
                         "note": "same pivot as the boom: rotate the sail and the boom together"}
        rud = s["rudder"]
        parts["Rudder"] = {"pivot": [rud["x"], 0, 0.0], "axis": [0, 0, 1],
                           "note": "on the pintle axis at the waterline; yaw = rudder angle. Blender +yaw (right-handed about +Z) swings "
                                   "the blade's trailing edge to starboard and the tiller to port: the bow turns to starboard. "
                                   "Unreal yaw = -(Blender yaw)."}
        parts["Tiller"] = {"pivot": [rud["x"], 0, rud["tiller_z"]], "axis": [0, 0, 1],
                           "note": "child of the rudder head; tiller along +X; carries the extension joint at its tip"}
        parts["Ext"] = {"pivot": [round(rud["x"] + rud["tiller_len"], 3), 0, rud["tiller_z"] + 0.02],
                        "note": "tiller extension universal joint at the tiller's tip; along +X at rest, swings freely"}
        bd = s["board"]
        if bd["kind"] == "dagger":
            parts["Board"] = {"pivot": [bd["x"], 0, 0.0], "slide_axis": [0, 0, 1],
                              "note": "daggerboard at the waterline on the slot's centre; slides along +Z: 0 = fully down, "
                                      "+%.2f = raised half, +%.2f = out" % (bd["len"] * 0.45, bd["len"])}
        else:
            parts["Board"] = {"pivot": [bd["pivot"][0], 0, bd["pivot"][1]], "axis": [0, 1, 0],
                              "note": "centreboard pivot bolt; rotates about +Y (pitch): 0 = down, ~ -80 deg = up into the case"}
        if b == "optimist":
            parts["Sprit"] = {"pivot": [mx, 0, s["sprit"]["snotter_z"]],
                              "note": "the snotter on the mast; the sprit runs up and aft to the peak, rotate with the main"}
        if b == "cadet":
            fs = s["forestay"]
            dx, dz = mx - fs["bow"][0], fs["hound_z"] - fs["bow"][2]
            L = math.hypot(dx, dz)
            parts["Jib"] = {"pivot": [s["jib"]["tack"][0], 0, s["jib"]["tack"][1]],
                            "axis": [round(dx / L, 4), 0, round(dz / L, 4)],
                            "note": "jib tack; the jib swings about the FORESTAY axis (given), sheeted to either side"}
        # ---- crew ----
        if b == "luch":
            seats = {"helm": (-1.00, 0.60, 0.33 + 0.10), "hike": (-0.92, 0.70, 0.30 + 0.12)}
            feet = {"helm": (-1.05, 0.12, 0.10), "hike": (-1.25, 0.02, 0.12)}
        elif b == "optimist":
            seats = {"helm": (-0.40, 0.47, 0.33 + 0.10), "hike": (-0.36, 0.53, 0.33 + 0.10)}
            feet = {"helm": (-0.30, 0.05, -0.05), "hike": (-0.45, 0.02, -0.03)}
        else:
            seats = {"helm": (-0.90, 0.56, 0.36 + 0.10), "hike": (-0.85, 0.62, 0.36 + 0.10),
                     "crew": (-0.15, 0.55, 0.38 + 0.10), "crew_hike": (-0.12, 0.62, 0.38 + 0.10),
                     "crew_low": (-0.20, 0.05, 0.00 + 0.12)}
            feet = {"helm": (-0.85, 0.15, -0.06), "hike": (-0.95, 0.30, -0.04),
                    "crew": (-0.05, 0.12, -0.06), "crew_hike": (-0.10, 0.30, -0.04), "crew_low": (0.20, 0.0, -0.06)}
        for k, p in seats.items():
            for side, sg in (("port", 1), ("stbd", -1)):
                pos = (p[0], p[1] * sg, p[2])
                fe = feet[k]
                fpos = (fe[0], fe[1] * sg, fe[2])
                facing = 0 if k == "crew_low" else (-75 if sg > 0 else 75)    # hikers face inboard-forward
                if k.endswith("hike"):
                    note = "sitting ON the gunwale, thighs on the side deck, torso leaning out to %s, feet hooked under the hiking strap" % side
                elif k == "crew_low":
                    note = "light airs: crouched on the floor on the centreline"
                else:
                    note = "sitting on the %s side deck, facing inboard and forward" % side
                d["%s_%s" % (k, side)] = {"pos": [round(v, 3) for v in pos], "ue_cm": ue(pos), "facing": facing,
                                         "feet": [round(v, 3) for v in fpos], "feet_ue_cm": ue(fpos), "note": note}
                if k == "crew_low":
                    break
        # hiking strap (feet under it), mainsheet, tiller grips
        d["strap"] = {"from": list(s["strap"][0]), "to": list(s["strap"][1]),
                      "note": "the strap line (feet go under it, toes up, on the hiking side of it)"}
        if b == "cadet":
            d["strap_stbd"] = {"from": [s["strap"][0][0], -s["strap"][0][1], s["strap"][0][2]],
                               "to": [s["strap"][1][0], -s["strap"][1][1], s["strap"][1][2]]}
        rx, ry, rz = s["ratchet"]
        d["mainsheet_block"] = {"pos": [rx, ry, rz], "ue_cm": ue((rx, ry, rz)),
                                "note": "the ratchet block the sheet comes up from (fixed on the hull)"}
        d["hand_mainsheet"] = {"pos": [rx, 0.0, rz + 0.45], "ue_cm": ue((rx, 0.0, rz + 0.45)),
                               "note": "front hand holding the sheet 45 cm above the ratchet block (neutral); slide along "
                                       "the line block -> hand toward the helm's shoulder when trimming"}
        d["hand_tiller_ext"] = {"frame": "Ext", "pos": [round(rud["ext_len"] - 0.06, 3), 0, 0],
                                "note": "aft hand: dagger grip on the extension 6 cm from its end (Ext frame)"}
        d["hand_tiller"] = {"frame": "Tiller", "pos": [round(rud["tiller_len"] - 0.12, 3), 0, 0.02],
                            "note": "a hand on the tiller itself (light airs, the cadet's helm)"}
        d["boom_end"] = {"frame": "Boom", "pos": [-s["boom_len"], 0, 0]}
        d["boom_block"] = {"frame": "Boom", "pos": [s["boom_block"], 0, -0.06],
                           "note": "where the mainsheet leaves the boom (Boom frame)"}
        if b == "cadet":
            d["hand_jibsheet"] = {"pos": [-0.25, 0.30, 0.40], "ue_cm": ue((-0.25, 0.30, 0.40)),
                                  "note": "crew's hand on the leeward jib sheet (port value; mirror y)"}
        d["painter"] = {"pos": [round(s["loa"] / 2 - 0.05, 3), 0, round(sheer(b, s["loa"] / 2 - 0.05), 3)],
                        "note": "bow eye where the painter is made fast / the trolley winch strap hooks"}
        d["trolley"] = {"pos": [0, 0, round(min(k[1] for k in s["keel_z"]) - 0.0, 3)],
                        "note": "lowest point of the keel: the hull rests on its trolley cradle this far below the waterline"}
        out[b] = {"parts": parts, "sockets": d,
                  "dims": {"loa": s["loa"], "beam": s["beam"], "mast_top_z": s["mast_top_z"],
                           "deck_z_mid": round(sheer(b, 0.0), 3), "sail_area": s["sail_area"]}}
    return out


def compact_json(obj):
    """indent=1 JSON with short number lists kept on one line."""
    import re
    txt = json.dumps(obj, indent=1, ensure_ascii=False)
    return re.sub(r"\[\s*(-?[\d.e+-]+(?:,\s*-?[\d.e+-]+)*)\s*\]",
                  lambda m: "[" + ", ".join(v.strip() for v in m.group(1).split(",")) + "]", txt)


if __name__ == "__main__":
    path = os.path.join(HERE, "sockets.json")
    with open(path, "w") as f:
        f.write(compact_json(sockets()) + "\n")
    print("wrote", path)
