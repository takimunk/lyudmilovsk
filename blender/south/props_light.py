"""The lighthouse's keeper's things (backlog item 42): the rotating lens, the cheap beam, the yard's picket fence and the
weather station. Built with the North's kit and atlas (blender/north/northkit.py, northtex.py; no atlas change, no words);
frame as everywhere: metres, Z up, Blender -Y is the front, transforms applied. Faces are wound outward explicitly (PB).

Glass and light are material slots of their own, not atlas tiles (the game makes those materials): the lens' glass
`M_South_LensGlass` (every panel UV 0..1, the bullseyes' rings concentric round UV (0.5, 0.5)) and the beam `M_South_Beam`.
"""
import math

from mathutils import Matrix, Vector

from northkit import V
from props_marks import Z_GL0, Z_GL1, glass_slot, lathe_w, mk, ring, rx, ry, wface
from props_places import beam, box, cbox, hexa, pole, tube

LAMP_Z = round((Z_GL0 + Z_GL1) / 2, 3)          # the tower's `lamp` z: this mesh's origin stands there
ZF, ZC = Z_GL0 - LAMP_Z, Z_GL1 - LAMP_Z         # the lantern's floor and the cap's underside in the lens' frame


# =========================================================================================================
# lighthouse_lens
# =========================================================================================================

def _pt(r, a, z):
    return V(math.cos(a) * r, math.sin(a) * r, z)


def glass_band(b, gs, r0, z0, r1, z1, seg, rot0=0.0):
    """A ring of `seg` glass panels from (vertex radius r0, z0) up to (r1, z1), each panel UV 0..1, wound outward."""
    for i in range(seg):
        a0, a1 = rot0 + 2 * math.pi * i / seg, rot0 + 2 * math.pi * (i + 1) / seg
        b.face([_pt(r0, a0, z0), _pt(r0, a1, z0), _pt(r1, a1, z1), _pt(r1, a0, z1)], [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)], mat=gs)


def lighthouse_lens(variant):
    b = mk("lighthouse_lens")
    gs = glass_slot(b, "M_South_LensGlass", (0.72, 0.86, 0.80, 1))
    # ---- the clockwork drum on the lantern floor (nothing hangs below it) ---------------------------------------
    lathe_w(b, [(0.52, ZF, "black_paint"), (0.52, ZF + 0.04, "black_paint"), (0.40, ZF + 0.07, "metal"), (0.40, ZF + 0.17, "metal"),
                (0.43, ZF + 0.17, "yellow_sign"), (0.43, ZF + 0.20, "yellow_sign"), (0.52, ZF + 0.20, "black_paint"),
                (0.52, ZF + 0.24, "black_paint"), (0.0, ZF + 0.24, "black_paint")], seg=24)
    # ---- the lens: an octagonal drum of eight panels, two bullseyes on +X and -X, the others plain refracting glass -------
    Rv = 0.595                                            # drum vertex radius (apothem 0.55)
    ap = Rv * math.cos(math.pi / 8)
    rot0 = -math.pi / 8                                   # vertices at -22.5 + 45k: the panels are centred on 0, 45, 90... degrees
    ZD = 0.28                                             # the drum's half height
    for k in range(8):
        a0, a1 = rot0 + 2 * math.pi * k / 8, rot0 + 2 * math.pi * (k + 1) / 8
        ac = (a0 + a1) / 2
        if k % 4 != 0:
            b.face([_pt(Rv, a0, -ZD), _pt(Rv, a1, -ZD), _pt(Rv, a1, ZD), _pt(Rv, a0, ZD)], [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)], mat=gs)
            continue
        zb = Rv * math.sin(math.pi / 8)                    # half the panel's width: the bullseye is square
        c = [_pt(Rv, a0, -zb), _pt(Rv, a1, -zb), _pt(Rv, a1, zb), _pt(Rv, a0, zb)]
        apex = _pt(ap + 0.075, ac, 0.0)                    # the bullseye: a square pyramid, rings round UV (0.5, 0.5)
        uv = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        for i in range(4):
            j = (i + 1) % 4
            b.face([c[i], c[j], apex], [uv[i], uv[j], (0.5, 0.5)], mat=gs)
        for sgn in (-1, 1):                               # brass-black plates close the bay above and below the bullseye
            z0, z1 = (zb, ZD) if sgn > 0 else (-ZD, -zb)
            P = [_pt(Rv, a0, z0), _pt(Rv, a1, z0), _pt(Rv, a1, z1), _pt(Rv, a0, z1)]
            wface(b, P, "black_paint")
    for k in range(8):                                    # posts on the vertices
        a = rot0 + 2 * math.pi * k / 8
        d = V(math.cos(a), math.sin(a), 0)
        beam(b, d * Rv + V(0, 0, -ZD - 0.02), d * Rv + V(0, 0, ZD + 0.02), 0.045, 0.045, "black_paint", up=d, end="black_paint")
    ring(b, (0, 0, 0), 0.50, 0.625, ZD, ZD + 0.04, 8, "yellow_sign", rot0=rot0)
    ring(b, (0, 0, 0), 0.50, 0.625, -ZD - 0.04, -ZD, 8, "yellow_sign", rot0=rot0)
    # ---- the refracting prism rings: three above (stepping in toward the cap), two below (down to the clockwork) -------------
    glass_band(b, gs, 0.60, ZD + 0.04, 0.58, 0.45, 16)
    ring(b, (0, 0, 0), 0.50, 0.60, 0.45, 0.465, 16, "black_paint")
    glass_band(b, gs, 0.58, 0.465, 0.54, 0.585, 16)
    ring(b, (0, 0, 0), 0.44, 0.56, 0.585, 0.60, 16, "black_paint")
    glass_band(b, gs, 0.52, 0.60, 0.42, 0.70, 16)
    lathe_w(b, [(0.44, 0.70, "yellow_sign"), (0.44, 0.72, "yellow_sign"), (0.30, 0.76, "black_paint"), (0.0, 0.775, "black_paint")], seg=16)
    glass_band(b, gs, 0.56, -0.435, 0.60, -ZD - 0.04, 16)
    ring(b, (0, 0, 0), 0.50, 0.58, -0.45, -0.435, 16, "black_paint")
    glass_band(b, gs, 0.50, -0.55, 0.56, -0.45, 16)
    # ---- the lamp inside: a mantle on a stem ---------------------------------------------------------------------
    pole(b, V(0, 0, ZF + 0.24), V(0, 0, -0.03), 0.014, seg=6, tile="metal", cap0=False)
    mantle = [(0.0, -0.06, "enamel_white")] + [(math.sin(math.pi * i / 6) * 0.055, -0.0 - math.cos(math.pi * i / 6) * 0.06, "enamel_white")
                                                for i in range(1, 6)] + [(0.0, 0.06, "enamel_white")]
    lathe_w(b, mantle, seg=10)
    notes = ("the lighthouse's rotating Fresnel lens, a separate mesh: its origin is the lens' centre on the rotation axis (Z up) "
             "and it stands in the tower's lantern with the origin at the tower manifest's `lamp` (z %.2f in the tower's frame): "
             "everything lies between z %.2f (the clockwork drum's foot on the lantern floor) and z +0.775 (the cap's underside is "
             "z +%.2f) and inside r 0.625 (the glazing's vertex radius is 1.14). Frame brass (the kit's yellow swatch) and black: "
             "a clockwork drum (r 0.52), an octagonal drum of eight panels (apothem 0.55, z +-0.28) with a bullseye on local +X and "
             "on -X (the two beams; a shallow square pyramid each, UV corners 0..1 with the centre at UV (0.5, 0.5), so a material's "
             "radial rings come out round) and six plain refracting panels, three prism rings above and two below (16 panels "
             "each), a brass cap, the mantle on its stem. Every glass panel is its own quad UV 0..1 in the slot M_South_LensGlass "
             "(a game material: glass; the frame is atlas tiles). No collision" % (LAMP_Z, ZF, ZC))
    return b, {"notes": notes, "manifest": {"axis": "z", "lamp_z": LAMP_Z, "beams": ["+x", "-x"], "floor_z": round(ZF, 3),
                                             "ceiling_z": round(ZC, 3), "radius": 0.625}}


# =========================================================================================================
# lighthouse_beam
# =========================================================================================================

BEAM_L, BEAM_W0, BEAM_W1 = 1.0, 0.01, 0.06


def lighthouse_beam(variant):
    b = mk("lighthouse_beam")
    gs = glass_slot(b, "M_South_Beam", (1.0, 0.9, 0.6, 1))
    for sx in (1, -1):
        for i in range(3):
            th = math.radians(60.0 * i)
            d = V(0, math.cos(th), math.sin(th))
            P = [V(0, 0, 0) - d * BEAM_W0, V(sx * BEAM_L, 0, 0) - d * BEAM_W1, V(sx * BEAM_L, 0, 0) + d * BEAM_W1, V(0, 0, 0) + d * BEAM_W0]
            uv = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
            if sx < 0:                                    # wound the other way round, so both halves face the same way round
                P, uv = P[::-1], uv[::-1]
            b.face(P, uv, mat=gs)
    notes = ("the lighthouse beam, the cheap one: two cones along +X and -X from the origin (the lens' centre), each of three flat "
             "cards crossed at 60 degrees about the X axis (6 quads), %.1f m long (the engine scales X), half-width %.2f m at the "
             "origin growing to %.2f m at the end. UV: U 0 at the origin to 1 at the far end, V 0..1 across the card. One slot "
             "M_South_Beam (the game makes it unlit, additive, two-sided), no atlas, no collision" % (BEAM_L, BEAM_W0, BEAM_W1))
    return b, {"notes": notes, "manifest": {"axis": "x", "length": BEAM_L, "half_width": [BEAM_W0, BEAM_W1], "cards": 3}}


# =========================================================================================================
# picket_fence
# =========================================================================================================

FL = 2.4                                # a segment's length along X
PY = 0.035                              # the posts' and rails' middle (y); the pickets are on the front (-Y) side of the rails
ZP_TOP = 1.12


def picket(b, x, w, z0, z1, tile="white_paint"):
    """A picket: a box with a pointed top (the top's four corners meet in a ridge along its width)."""
    t = 0.03
    y0 = -0.02
    P = [V(x - w / 2, y0, z0), V(x + w / 2, y0, z0), V(x + w / 2, y0 + t, z0), V(x - w / 2, y0 + t, z0)]
    zt = z1 - 0.06
    top = [V(x - w / 2, y0, zt), V(x + w / 2, y0, zt), V(x + w / 2, y0 + t, zt), V(x - w / 2, y0 + t, zt)]
    hexa(b, P + top, tile, grain=V(0, 0, 1), skip=("top", "bottom"))
    tip = [V(x - w / 2, y0, zt), V(x + w / 2, y0, zt), V(x + w / 2, y0 + t, zt), V(x - w / 2, y0 + t, zt)]
    hexa(b, tip + [V(x, y0, z1), V(x, y0, z1), V(x, y0 + t, z1), V(x, y0 + t, z1)], tile, grain=V(0, 0, 1), skip=("bottom",))


def picket_fence(variant):
    b = mk("picket_fence_v%d" % variant)
    px = FL / 2 - 0.1
    for sx in (-1, 1):                                    # the posts, bedded 0.25 into the ground, with a pitched cap
        box(b, V(sx * px, PY, (ZP_TOP - 0.25) / 2), (0.1, 0.1, ZP_TOP + 0.25), "white_paint", grain=2)
        hexa(b, [V(sx * px - 0.07, PY - 0.07, ZP_TOP), V(sx * px + 0.07, PY - 0.07, ZP_TOP), V(sx * px + 0.07, PY + 0.07, ZP_TOP),
                 V(sx * px - 0.07, PY + 0.07, ZP_TOP), V(sx * px - 0.02, PY - 0.02, ZP_TOP + 0.05), V(sx * px + 0.02, PY - 0.02, ZP_TOP + 0.05),
                 V(sx * px + 0.02, PY + 0.02, ZP_TOP + 0.05), V(sx * px - 0.02, PY + 0.02, ZP_TOP + 0.05)], "white_paint", skip=("bottom",))
    for zr in (0.3, 0.78):                                # two rails between the posts, behind the pickets
        box(b, V(0, PY, zr), (2 * px, 0.05, 0.07), "white_paint", grain=0)
    inner = px - 0.05                                     # clear width between the posts
    if variant == 0:
        n = 17
        w = 0.07
        pitch = (inner * 2 - w) / (n - 1)
        for i in range(n):
            x = -inner + w / 2 + i * pitch
            picket(b, x, w, 0.04, 0.98, "wood" if i == 6 else "white_paint")      # one board is new, unpainted
        notes_v = "plain: 17 pickets 0.07 wide, 0.98 m high, two rails (z 0.30 and 0.78), two posts (x +-1.1, to z 1.12 with a cap)"
        coll = [cbox("fence", (0, PY, 0.5 + 0.0), (FL, 0.14, 1.0))]
        gate = None
    else:
        # the gate: a leaf hung on the -X post, closed, latched on the +X post; its top rises in a gentle arch, a diagonal brace
        n = 15
        w = 0.07
        pitch = (inner * 2 - w) / (n - 1)
        for i in range(n):
            x = -inner + w / 2 + i * pitch
            u = x / inner
            top = 0.93 + 0.14 * (1 - u * u)
            picket(b, x, w, 0.04, top)
        beam(b, V(-inner + 0.04, PY, 0.24), V(inner - 0.04, PY, 0.84), 0.05, 0.035, "white_paint", up=V(0, 1, 0))     # the brace, up to the latch side
        for zh in (0.3, 0.78):                            # strap hinges on the -X post (proud of the leaf's front, y -0.02)
            box(b, V(-px + 0.055, -0.031, zh), (0.2, 0.014, 0.04), "black_paint", grain=0)
            box(b, V(-px + 0.03, -0.029, zh), (0.05, 0.034, 0.05), "black_paint")
        # the latch: a bar on the leaf and a keeper on the post, a ring pull
        box(b, V(inner - 0.2, -0.031, 0.62), (0.3, 0.014, 0.035), "black_paint", grain=0)
        box(b, V(inner - 0.055, -0.029, 0.62), (0.05, 0.034, 0.06), "black_paint")
        box(b, V(px, -0.033, 0.62), (0.04, 0.036, 0.06), "black_paint")
        pole(b, V(inner - 0.3, -0.055, 0.58), V(inner - 0.3, -0.055, 0.66), 0.012, seg=6, tile="black_paint")
        notes_v = ("with a closed gate in place of the pickets between the posts: a leaf of 15 pickets with an arched top (z 1.07 in the "
                   "middle, 0.93 at the sides), two rails and a diagonal brace, strap hinges on the -X post, a latch bar and keeper "
                   "on the +X post. Collision: the two posts only, the leaf is not solid (the gate is the way in; the hero walks "
                   "through)")
        coll = [cbox("post_-x", (-px, PY, 0.5), (0.14, 0.14, 1.0)), cbox("post_+x", (px, PY, 0.5), (0.14, 0.14, 1.0))]
        gate = {"x": [-inner, inner], "passable": True}
    notes = ("a whitewashed picket fence segment %.1f m long along local X, origin at its middle on the ground, the pickets on the "
             "front (-Y) side (the rails and the posts behind); posts bedded 0.25 m (nothing else under ground). Atlas tiles: white "
             "paint, a little wood and black iron. %s. Collision: box (collision_boxes: one slab %.1f x 0.14 x 1.0 m for the "
             "plain one)" % (FL, notes_v, FL))
    man = {"collision_boxes": coll, "bedded": 0.25, "length": FL}
    if gate:
        man["gate"] = gate
    return b, {"notes": notes, "manifest": man}


# =========================================================================================================
# meteo_screen
# =========================================================================================================

SZ0, SZ1 = 1.25, 1.85                   # the screen's underside and the roof slab's top
LEG_X, LEG_Y = 0.27, 0.2
RG_X = 0.8                              # the rain gauge
MAST_X, MAST_Y, MAST_H = 1.3, 0.3, 2.5


def louvres(b, c, size, axis, tilt, n, tile="white_paint"):
    """n louvre boards for a wall centred at c: boards run along `axis` ('x' or 'y') over `size[0]`, tilted by `tilt` about
    their length so the top edge leans in (the bottom edge out), stacked over `size[1]` in z."""
    L, H = size
    t, wid = 0.012, 0.078
    pitch = H / n
    for i in range(n):
        z = c[2] - H / 2 + pitch * (i + 0.5)
        if axis == "y":
            box(b, V(c[0], c[1], z), (t, L, wid), tile, rot=ry(tilt), grain=1)
        else:
            box(b, V(c[0], c[1], z), (L, t, wid), tile, rot=rx(tilt), grain=0)


def meteo_screen(variant):
    b = mk("meteo_screen")
    za, zb = SZ0 + 0.03, SZ1 - 0.03                       # the louvred walls' z range
    zm = (za + zb) / 2
    # ---- the box: floor board, a dark core behind the louvres, corner posts, louvred sides and back, roof -------------
    box(b, V(0, 0, SZ0 + 0.015), (0.62, 0.52, 0.03), "white_paint", grain=0)
    box(b, V(0, 0, zm), (0.54, 0.44, zb - za), "dark", grain=2, skip=("top", "bottom"))
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(b, V(sx * 0.278, sy * 0.228, zm), (0.045, 0.045, zb - za), "white_paint", grain=2, skip=("top", "bottom"))
    louvres(b, V(0.285, 0, zm), (0.42, zb - za), "y", -math.radians(40), 9)
    louvres(b, V(-0.285, 0, zm), (0.42, zb - za), "y", math.radians(40), 9)
    louvres(b, V(0, 0.235, zm), (0.50, zb - za), "x", math.radians(40), 9)
    # ---- the louvred door on the front (-Y): a frame and louvres, a knob and two hinge straps -------------------------
    yd = -0.267
    for sx in (-1, 1):
        box(b, V(sx * 0.245, yd, zm), (0.04, 0.025, zb - za), "white_paint", grain=2)
    for zz in (za + 0.02, zb - 0.02):
        box(b, V(0, yd, zz), (0.45, 0.025, 0.04), "white_paint", grain=0)
    louvres(b, V(0, yd, zm), (0.45, zb - za - 0.08), "x", -math.radians(40), 8)
    box(b, V(0.205, yd - 0.022, zm), (0.03, 0.03, 0.03), "black_paint")
    for zz in (za + 0.08, zb - 0.08):
        box(b, V(-0.2, yd - 0.015, zz), (0.12, 0.008, 0.025), "black_paint", grain=0)
    # ---- roof: an eave slab and a low gable, ridge along X -----------------------------------------------------------------
    box(b, V(0, 0, SZ1 - 0.015), (0.74, 0.64, 0.03), "white_paint", grain=0)
    hexa(b, [V(-0.35, -0.29, SZ1), V(0.35, -0.29, SZ1), V(0.35, 0.29, SZ1), V(-0.35, 0.29, SZ1),
             V(-0.35, 0.0, SZ1 + 0.09), V(0.35, 0.0, SZ1 + 0.09), V(0.35, 0.0, SZ1 + 0.09), V(-0.35, 0.0, SZ1 + 0.09)],
         "white_paint", grain=V(1, 0, 0), skip=("bottom",))
    # ---- legs (bedded 0.25), a low frame brace ----------------------------------------------------------------------
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(b, V(sx * LEG_X, sy * LEG_Y, (SZ0 + 0.015 - 0.25) / 2), (0.07, 0.07, SZ0 + 0.015 + 0.25), "white_paint", grain=2)
    for sy in (-1, 1):
        box(b, V(0, sy * LEG_Y, 0.55), (2 * LEG_X - 0.07, 0.035, 0.05), "white_paint", grain=0)
    for sx in (-1, 1):
        box(b, V(sx * LEG_X, 0, 0.55), (0.035, 2 * LEG_Y - 0.07, 0.05), "white_paint", grain=1)
    # ---- the tin case on the front +X leg: lid ajar, the log book hanging on a cord by it -------------------------------
    cx, cy, cz = LEG_X, -LEG_Y - 0.035 - 0.04, 0.95
    box(b, V(cx, cy, cz), (0.2, 0.08, 0.26), "tin", grain=2)
    box(b, V(cx, cy - 0.043, cz + 0.0), (0.17, 0.008, 0.22), "steel", grain=2)                            # the door's panel
    box(b, V(cx, cy - 0.055, cz + 0.02), (0.04, 0.02, 0.03), "black_paint")                               # its catch
    box(b, V(cx, cy, cz + 0.14), (0.22, 0.1, 0.02), "tin", grain=0, rot=rx(math.radians(-6)))          # a lid over the top
    for sz in (-1, 1):
        box(b, V(cx - 0.11, cy + 0.0, cz + sz * 0.1), (0.015, 0.09, 0.04), "steel")                       # straps round the leg
    bk = V(cx + 0.26, cy - 0.01, cz - 0.03)                                                              # the book on its cord
    box(b, bk, (0.14, 0.02, 0.19), "red_paint", grain=2)
    box(b, bk + V(0.0, -0.011, 0.0), (0.12, 0.003, 0.17), "enamel_white", grain=2)                         # its label
    tube(b, [V(cx + 0.1, cy - 0.01, cz + 0.12), V(cx + 0.17, cy - 0.04, cz + 0.2), V(bk.x, bk.y - 0.012, bk.z + 0.095)], 0.004, seg=4, tile="rope")
    # ---- the rain gauge: a short post and a metal cylinder with a funnel ----------------------------------------------------
    box(b, V(RG_X, 0, (0.62 - 0.25) / 2), (0.08, 0.08, 0.87), "white_paint", grain=2)
    box(b, V(RG_X, 0, 0.622), (0.14, 0.14, 0.025), "white_paint", grain=0)
    lathe_w(b, [(0.0, 0.635, "tin"), (0.058, 0.635, "tin"), (0.058, 0.93, "tin"), (0.066, 0.95, "steel"), (0.05, 0.95, "steel"),
                (0.012, 0.88, "tin"), (0.0, 0.88, "tin")], c=(RG_X, 0, 0), seg=16)
    for z in (0.7, 0.86):
        ring(b, (RG_X, 0, 0), 0.056, 0.064, z, z + 0.015, 16, "steel")
    # ---- the mast with the wind vane ---------------------------------------------------------------------------------------
    pole(b, V(MAST_X, MAST_Y, -0.25), V(MAST_X, MAST_Y, MAST_H), 0.03, 0.016, seg=8, tile="white_paint")
    zv = MAST_H + 0.02
    pole(b, V(MAST_X, MAST_Y, MAST_H - 0.06), V(MAST_X, MAST_Y, zv + 0.12), 0.01, seg=6, tile="black_paint")
    for a in (0.0, math.pi / 2, math.pi, 3 * math.pi / 2):                                               # the compass cross
        d = V(math.cos(a), math.sin(a), 0)
        pole(b, V(MAST_X, MAST_Y, MAST_H - 0.28) + d * 0.015, V(MAST_X, MAST_Y, MAST_H - 0.28) + d * 0.22, 0.007, seg=4, tile="black_paint")
    vz = zv + 0.0
    box(b, V(MAST_X - 0.2, MAST_Y, vz), (0.28, 0.012, 0.12), "black_paint", grain=0)                       # the tail
    pole(b, V(MAST_X, MAST_Y, vz), V(MAST_X + 0.27, MAST_Y, vz), 0.01, 0.0, seg=5, tile="black_paint")      # the arrow
    hexa(b, [V(MAST_X + 0.04, MAST_Y - 0.006, vz - 0.05), V(MAST_X + 0.22, MAST_Y - 0.006, vz), V(MAST_X + 0.22, MAST_Y + 0.006, vz),
             V(MAST_X + 0.04, MAST_Y + 0.006, vz - 0.05), V(MAST_X + 0.04, MAST_Y - 0.006, vz + 0.05), V(MAST_X + 0.22, MAST_Y - 0.006, vz),
             V(MAST_X + 0.22, MAST_Y + 0.006, vz), V(MAST_X + 0.04, MAST_Y + 0.006, vz + 0.05)], "black_paint", skip=("bottom",))
    # ---- collision --------------------------------------------------------------------------------------------------------------
    boxes = [cbox("screen", (0, 0, (SZ0 + SZ1 + 0.09) / 2), (0.74, 0.64, SZ1 + 0.09 - SZ0))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            boxes.append(cbox("leg_%s%s" % ("p" if sx > 0 else "n", "p" if sy > 0 else "n"), (sx * LEG_X, sy * LEG_Y, SZ0 / 2), (0.1, 0.1, SZ0)))
    boxes.append(cbox("rain_gauge", (RG_X, 0, 0.5), (0.16, 0.16, 1.0)))
    boxes.append(cbox("mast", (MAST_X, MAST_Y, MAST_H / 2), (0.08, 0.08, MAST_H)))
    read_at = {"x": LEG_X, "y": -0.85, "z": 0.0, "yaw": 180.0}
    notes = ("a Stevenson screen with the keeper's weather things, front -Y. The screen: a white louvred wooden box 0.6 x 0.5 m, "
             "underside z %.2f, walls to z %.2f (louvred sides and back, a louvred door with a knob and hinge straps on the front, a "
             "dark core behind the louvres), an eave slab and a low gable on top (top z %.2f), on four legs bedded 0.25 m with "
             "a low brace. On the front +X leg (x %.2f) a small tin case (z 0.82..1.08) with a catch and its lid, the log book (red "
             "cover, white label) hanging on a cord at its side. A rain gauge %.1f m to +X (a post, a tin cylinder 0.12 m wide with a "
             "funnel, top z 0.95). A thin white mast at x %.1f, y %.1f, %.1f m high, with a compass cross and a wind vane. Origin on "
             "the ground under the screen's middle. read_at = where the hero stands to read the log (in front of the case, facing "
             "it). Collision: box (collision_boxes): the screen, four legs, the gauge, the mast" % (SZ0, SZ1 - 0.03, SZ1 + 0.09, LEG_X, RG_X, MAST_X, MAST_Y, MAST_H))
    return b, {"notes": notes, "manifest": {"read_at": read_at, "collision_boxes": boxes, "bedded": 0.25,
                                             "case": {"x": cx, "y": cy, "z": cz}, "rain_gauge": {"x": RG_X, "y": 0.0, "z": 0.95},
                                             "mast": {"x": MAST_X, "y": MAST_Y, "z": MAST_H}}}


KINDS = {
    "lighthouse_lens": (lighthouse_lens, {"variants": [0], "collision": "none"}),
    "lighthouse_beam": (lighthouse_beam, {"variants": [0], "collision": "none"}),
    "picket_fence": (picket_fence, {"variants": [0, 1], "collision": "box"}),
    "meteo_screen": (meteo_screen, {"variants": [0], "collision": "box"}),
}
