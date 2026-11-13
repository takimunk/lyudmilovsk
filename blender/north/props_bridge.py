"""The North's log footbridges (kind log_bridge, variants 0..2 by the water they span).

Frame: metres, origin at the middle of the deck on its top walking surface (z = 0 is where feet stand); the deck runs
along the mesh's local X axis ("axis": "x"), walking width 1.1 m, a pole handrail on the +Y side.  The last 2.5 m at each
end slope down 1.0 m (ramps); X-trestles of logs reach 2.6 m below the deck.
"""
import math
import random

from mathutils import Matrix

from northkit import Builder, V

LENGTH = (16.0, 21.0, 31.0)          # deck length, m (spans up to 11 / 16 / 26 m)
RAMP_L, RAMP_DROP = 2.5, 1.0
WIDTH = 1.1
PLANK_T = 0.09
STR_R = 0.14                         # stringer radius
STR_Y = (-0.36, 0.0, 0.36)
FOOT = -2.6                          # trestle feet below the deck top


def ry(a):
    return Matrix.Rotation(a, 3, "Y")


def rz(a):
    return Matrix.Rotation(a, 3, "Z")


def supports(variant):
    a = LENGTH[variant] / 2 - RAMP_L
    if variant == 0:
        return a, [-a, a]
    if variant == 1:
        return a, [-a, 0.0, a]
    return a, [-a, -a / 2, 0.0, a / 2, a]


def deck_z(x, a):
    """Top surface height of the deck at x."""
    d = abs(x) - a
    return 0.0 if d <= 0 else -RAMP_DROP * min(1.0, d / RAMP_L)


def log_bridge(variant):
    b = Builder("log_bridge_v%d" % variant)
    rnd = random.Random(40 + variant)
    L = LENGTH[variant]
    a, sup = supports(variant)
    half = L / 2
    th = math.atan2(RAMP_DROP, RAMP_L)
    slope = math.hypot(RAMP_L, RAMP_DROP)

    # ---- deck: split half-logs / rough planks across the stringers (grain along Y) --------------------------
    def plank(c, w, t, rot, tile):
        ln = WIDTH + rnd.uniform(-0.02, 0.05)
        k = len(b.bm.faces)
        b.box(c, (w, ln, t), tile, rot=rot, tiles={"y": "endgrain", "top": tile, "bottom": tile}, grain=1)
        down = rot @ V(0, 0, -1)
        b.bm.faces.ensure_lookup_table()
        for f in [b.bm.faces[i] for i in range(k, len(b.bm.faces))]:       # the underside is never seen: drop it
            if (f.normal if f.normal.length > 0 else __import__("mathutils").geometry.normal([v.co for v in f.verts])).dot(down) > 0.99:
                b.bm.faces.remove(f)

    n = max(1, round(2 * a / 0.30))
    pw = 2 * a / n
    for i in range(n):
        t = PLANK_T + rnd.uniform(-0.012, 0.012)
        x = -a + pw * (i + 0.5)
        plank(V(x, rnd.uniform(-0.012, 0.012), -t / 2), pw - 0.02, t, rz(rnd.uniform(-0.012, 0.012)),
              "wood_dark" if rnd.random() < 0.08 else "wood")
    nr = 9
    rw = slope / nr
    for sgn in (-1, 1):
        ang = th * sgn
        nrm = V(math.sin(ang), 0, math.cos(ang))
        for i in range(nr):
            s = (i + 0.5) / nr * RAMP_L
            x = sgn * (a + s)
            top = V(x, rnd.uniform(-0.012, 0.012), -RAMP_DROP * s / RAMP_L)
            plank(top - nrm * (PLANK_T / 2), rw - 0.02, PLANK_T, ry(ang), "wood_dark" if rnd.random() < 0.08 else "wood")

    # ---- stringers: peeled logs, one length per bay (bearing on the trestle cap beams) ----------------------
    zc = -PLANK_T - 0.01 - STR_R                  # axis height under the flat deck
    for y in STR_Y:
        r = STR_R + rnd.uniform(-0.008, 0.008)
        for x0, x1 in zip(sup[:-1], sup[1:]):
            rr = r + rnd.uniform(-0.006, 0.006)
            b.cyl(V(x0, y, zc), V(x1, y, zc), rr, rr * 0.97, seg=7, tile="wood", cap_tile="endgrain", wobble=0.03)
        for sgn in (-1, 1):
            zend = zc - RAMP_DROP
            b.cyl(V(sgn * a, y, zc), V(sgn * half, y, zend), r, r * 0.95, seg=7, tile="wood", cap_tile="endgrain", wobble=0.03)
    # a sill log across under each ramp's toe, half buried in the gravel
    for sgn in (-1, 1):
        x = sgn * (half - 0.18)
        zz = deck_z(x, a) - PLANK_T - 0.01 - 2 * STR_R - 0.09
        b.cyl(V(x, -0.62, zz), V(x, 0.62, zz), 0.1, 0.095, seg=6, tile="wood_dark", cap_tile="endgrain", wobble=0.04)

    # ---- X-trestles of logs standing on the river bed -------------------------------------------------------
    zb = zc - STR_R - 0.01                        # underside of the stringers
    zbeam = zb - 0.11
    for xs in sup:
        b.cyl(V(xs, -0.67, zbeam), V(xs, 0.67, zbeam), 0.11, 0.105, seg=6, tile="wood_dark", cap_tile="endgrain", wobble=0.03)
        for y in (-0.5, 0.5):
            for sg in (-1, 1):
                b.cyl(V(xs + sg * 0.85, y, FOOT), V(xs - sg * 0.1, y, zbeam), 0.105, 0.095, seg=6, tile="wood_dark",
                      cap_tile="endgrain", wobble=0.04)
        for sg in (-1, 1):
            b.cyl(V(xs + sg * 0.85, -0.67, FOOT + 0.12), V(xs + sg * 0.85, 0.67, FOOT + 0.12), 0.08, 0.075, seg=5,
                  tile="wood_dark", cap_tile="endgrain", wobble=0.03)
            zk = -1.55
            xk = xs + sg * 0.38
            b.cyl(V(xk, -0.46, zk), V(xk, 0.46, zk), 0.06, 0.06, seg=5, tile="wood_dark", cap_tile="endgrain")

    # ---- handrail on the +Y side: round posts (box), one pole ------------------------------------------------
    yr = WIDTH / 2 + 0.07
    nflat = max(2, round(2 * a / 3.0))
    px = [-a + 2 * a * i / nflat for i in range(nflat + 1)]
    px = [-half + 0.12] + px + [half - 0.12]
    px = sorted(set(round(p, 3) for p in px))
    tops = []
    for x in px:
        dz = deck_z(x, a)
        b.box(V(x, yr, (dz - 0.38 + dz + 0.98) / 2), (0.09, 0.09, 1.36), "wood", grain=2, tiles={"top": "endgrain"})
        tops.append(V(x, yr, dz + 0.9))
    for p0, p1 in zip(tops[:-1], tops[1:]):
        b.cyl(p0, p1, 0.04, 0.04, seg=5, tile="wood", cap_tile="endgrain", wobble=0.03)

    # ---- collision (mesh space, Blender axes) ----------------------------------------------------------------
    boxes = [{"name": "deck", "center": [0.0, 0.0, -0.15], "size": [round(2 * a, 3), WIDTH, 0.3], "rot_y_deg": 0.0}]
    for sgn in (-1, 1):
        ang = th * sgn
        nrm = (math.sin(ang), math.cos(ang))
        mid = (sgn * (a + RAMP_L / 2), -RAMP_DROP / 2)
        boxes.append({"name": "ramp_" + ("east" if sgn > 0 else "west"),
                      "center": [round(mid[0] - nrm[0] * 0.15, 3), 0.0, round(mid[1] - nrm[1] * 0.15, 3)],
                      "size": [round(slope, 3), WIDTH, 0.3], "rot_y_deg": round(math.degrees(ang), 3)})
    boxes.append({"name": "rail", "center": [0.0, yr, 0.9], "size": [round(2 * a, 3), 0.1, 0.12], "rot_y_deg": 0.0})
    for sgn in (-1, 1):
        ang = th * sgn
        boxes.append({"name": "rail_ramp_" + ("east" if sgn > 0 else "west"),
                      "center": [round(sgn * (a + RAMP_L / 2), 3), yr, round(0.9 - RAMP_DROP / 2, 3)],
                      "size": [round(slope, 3), 0.1, 0.12], "rot_y_deg": round(math.degrees(ang), 3)})
    notes = ("rustic log footbridge, deck %.0f m long (spans up to %.0f m) running along local X (\"axis\": \"x\"); origin = middle of "
             "the deck at its top walking surface (z 0 = feet); walking width 1.1 m; ramps at both ends slope down 1.0 m over the last "
             "2.5 m (toes at z -1.0, stringers and sill reach z -1.5); pole handrail on the +Y side; log X-trestles at x %s reach "
             "z %.1f; no collision on trestles; collision_boxes are in mesh space, rot_y_deg is a right-handed rotation about Blender +Y "
             "(positive tips the +X end down)" % (L, L - 5, ", ".join("%g" % s for s in sup), FOOT))
    return b, {"notes": notes, "manifest": {"axis": "x", "length": L, "span_max": L - 5, "width": WIDTH,
                                            "ramp_length": RAMP_L, "ramp_drop": RAMP_DROP, "support_x": [round(s, 3) for s in sup],
                                            "support_depth": -FOOT, "collision_boxes": boxes}}
