"""The Academgorodok's grocery: «Гастроном № 7» moved out of Novikova 1 (whose ground floor is the library now) into a
one-storey pavilion of its own on the yard side, west of Novikova 1, by the drive from the south gate. Pure Python (no bpy).

The hall inside is gastronom.py's 1950s Gastronom as it was (counter-vitrinas, the cases on the back wall, the cashier's
booth, the scales, the pendants), unchanged: the pavilion is built round it in the same house frame (x along the
pavilion, north +; y across it, the street front at y = -7.25 outside / -6.75 inside, the back wall inside at y = 1.5;
ground floor z = 0.7), so its shell has the openings gastronom.py expects in its front wall: the entrance (x = 0), the
hall door (x = 4.35) and three shop windows. Ochre plaster, white trim and the plinth like the houses, a hipped grey
steel roof, an enamel sign over the entrance; both doors are real doors (ALyudDoor via layout.json), shut.

The shop sells over the counter while its assistant is at work (Content/PeopleData/counters.json, people_map.py's
"gastronom" place): bread, kefir and the bird feeds (docs/bird_taming.md "Feeds and the feeder").

  python3 grocery.py        prints the pavilion's frame, size and triangles
"""
import math

from common import PLINTH_H
import geom
from geom import Facade, Frame
import gastronom as G
import plan

# the pavilion's house frame in the ensemble: parallel to Novikova 1 (93 deg), its street front looking east into the
# yard about 31 m behind Novikova 1, its south end 4 m north of the drive from the south gate (plan.square_mesh)
FRAME = (-45.2, -30.0, 93.0)
X0, X1 = -2.95, 18.5           # outer faces: the vestibule's south wall + 0.5, the hall's north wall + 0.5
Y0, Y1 = G.YO, G.HALL[3] + 0.5  # the front (-7.25) and the back (2.0)
EAVES = 4.9                    # wall top over the ground (the hall's ceiling 4.2, a slab and an attic band)
PITCH = math.radians(20.0)
OVER = 0.55
SIGN = (0.0, 3.95, 1.6, 1.125)  # the enamel sign over the entrance: centre x, centre z, width, height
STEPS = 2


def footprint():
    """(centre x, centre y) in the ensemble, yaw deg, length, depth: for tools/animal_map.py ACADEM_HOUSES."""
    f = frame()
    c = f(((X0 + X1) / 2, (Y0 + Y1) / 2, 0.0))
    return (round(c[0], 2), round(c[1], 2)), FRAME[2], round(X1 - X0, 2), round(Y1 - Y0, 2)


def frame():
    return Frame(FRAME[0], FRAME[1], math.radians(FRAME[2]))


def _sign_quad(m, name, cx, cy, cz, w, h, n, off=0.006):
    """A picture quad (material AC_Sign_<name>: tools/make_academ_signs.py, AcademToggle.cpp) facing n."""
    nx, ny = n
    tx, ty = -ny, nx                    # along the face, to the viewer's right
    c = (cx + nx * off, cy + ny * off)
    pts = [(c[0] - tx * w / 2, c[1] - ty * w / 2, cz - h / 2), (c[0] + tx * w / 2, c[1] + ty * w / 2, cz - h / 2),
           (c[0] + tx * w / 2, c[1] + ty * w / 2, cz + h / 2), (c[0] - tx * w / 2, c[1] - ty * w / 2, cz + h / 2)]
    m.face("AC_Sign_" + name, pts, [(0, 0), (1, 0), (1, 1), (0, 1)])


def build():
    h = plan.House("Gastronom", frame())
    h.interiors = False
    # --- the street front (east, into the yard): walked from south to north at y = Y0 --------------------
    fac = Facade((X0, Y0), (X1, Y0))
    cu = lambda x: x - X0
    ops = []
    for cx, sill, w, ht in (G.ENTRANCE, G.HALL_DOOR):
        ops.append(plan.Opening(cu(cx), sill, w, ht, kind="door", unit="double", room=False, surround=None))
    for cx, sill, w, ht in G.SHOP_WINDOWS:
        ops.append(plan.Opening(cu(cx), sill, w, ht, unit="shop", surround=None, room=False))

    def front_details(m):
        # the white trim round the openings and a cornice band under the eaves, as on Novikova 1's ground floor
        for o in ops:
            u0, u1, z0, z1 = o.rect
            geom.box(m, "AC_Trim", u0 - 0.18, u1 + 0.18, 0.0, 0.05, z1, z1 + 0.18)
            geom.box(m, "AC_Trim", u0 - 0.18, u0, 0.0, 0.05, max(z0, PLINTH_H), z1)
            geom.box(m, "AC_Trim", u1, u1 + 0.18, 0.0, 0.05, max(z0, PLINTH_H), z1)
        geom.box(m, "AC_Trim", -0.1, X1 - X0 + 0.1, 0.0, 0.12, EAVES - 0.35, EAVES - 0.1)
        # the sign's white enamel plate on its iron frame (the picture is a quad in front of it)
        sx, sz, sw, sh = SIGN
        geom.box(m, "AC_IronWhite", cu(sx) - sw / 2 - 0.03, cu(sx) + sw / 2 + 0.03, 0.0, 0.03, sz - sh / 2 - 0.03, sz + sh / 2 + 0.03)
    plan.build_facade(h, fac, EAVES, ops, extra=front_details)
    for o in ops:
        if o.kind == "door":
            h.detail(fac, "steps", o.uc, 0.0, 0.0, w=o.w + 0.6, n=STEPS, rise=o.z0 / STEPS, tread=0.35)
    # --- the plain ends and the back ---------------------------------------------------------------------
    def band(length):
        return lambda m: geom.box(m, "AC_Trim", -0.1, length + 0.1, 0.0, 0.12, EAVES - 0.35, EAVES - 0.1)
    for p0, p1 in (((X1, Y0), (X1, Y1)), ((X1, Y1), (X0, Y1)), ((X0, Y1), (X0, Y0))):
        f = Facade(p0, p1)
        plan.build_facade(h, f, EAVES, [], extra=band(f.length))
    # --- the roof, and a lid over the unbuilt space behind the vestibule (seen from nowhere, closes the box) --
    geom.hip_roof(h.mesh, "AC_Roof", "AC_Timber", "AC_Trim", X0, X1, Y0, Y1, EAVES, PITCH, OVER)
    h.mesh.face("AC_Trim", [(X0, Y1, EAVES), (X1, Y1, EAVES), (X1, Y0, EAVES), (X0, Y0, EAVES)])
    # --- the forecourt: asphalt in front of both doors and a strip south to the drive -------------------------
    ap = [(X0 - 7.0, Y0 - 3.4), (G.HALL_DOOR[0] + 2.0, Y0 - 3.4), (G.HALL_DOOR[0] + 2.0, Y0 - 0.02), (X0 - 7.0, Y0 - 0.02)]
    h.mesh.face("AC_Asphalt", [(x, y, 0.025) for x, y in ap])
    # --- the outside sign: «ГАСТРОНОМ» on white enamel over the entrance -----------------------------------
    sx, sz, sw, sh = SIGN
    _sign_quad(h.mesh, "grocery", sx, Y0 - 0.03, sz, sw, sh, (0.0, -1.0))
    # --- inside: the Gastronom's hall as gastronom.py builds it, its boards printed per language ------------
    h.inside.extend(G.build())
    for (key, lx, ly, lz, lw, lh), nrm in zip(G.LABELS, G.LABEL_NORMALS):
        if key.startswith("sign.gastronom."):
            _sign_quad(h.inside, "gastronom_" + key.split(".")[-1], lx, ly, lz, lw, lh, (nrm[0], nrm[1]))
    h.lights = list(G.LIGHTS)
    h.plants = []
    return h


if __name__ == "__main__":
    hh = build()
    print("frame", FRAME, "footprint", footprint())
    print("shell tris", hh.mesh.tri_count(), "inside tris", hh.inside.tri_count(), "details", len(hh.details),
          "doors", len(getattr(hh, "doors", [])), "lights", len(hh.lights))
    for d in hh.doors:
        print("door", [round(v, 2) for v in hh.frame(d["at"])], d["hole"], "open" if d["open"] else "shut")
