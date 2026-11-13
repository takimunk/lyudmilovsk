"""The ranger's hut, the door leaf (hut_door.glb): a ledged-and-braced plank door 0.94 x 1.95 m, 4.5 cm thick, hinged on its
left edge as seen from outside. Origin = the hinge axis at the bottom of the leaf; the leaf stands along +X from the origin
(closed), its outside face toward -Y (the porch), and swings outward (toward -Y) about the Z axis through the origin.
In hut-local coordinates the hinge axis stands at DOOR_HINGE."""
import math

from hut_common import F, Mesh, bx, box, cyl, rot_y, rot_z, about, tube_path

LEAF_W, LEAF_H, LEAF_T = 0.94, 1.95, 0.045
DOOR_HINGE = (0.015, -1.79, F + 0.01)        # hut-local: x, y, z of the hinge axis' bottom
P = "HUT_PlankOut"


def build():
    m = Mesh("hut_door")
    t0, t1 = -LEAF_T / 2, LEAF_T / 2
    n = 5
    gap = 0.004
    bw = (LEAF_W - gap * (n - 1)) / n
    for i in range(n):                                # the boards (outside)
        x0 = i * (bw + gap)
        bx(m, P, x0, x0 + bw, t0, t0 + 0.028, 0.0, LEAF_H, grain=2)
    for z in (0.22, 0.95, 1.68):                      # three ledges on the inside, covering the gaps
        bx(m, "HUT_PlankIn", 0.0, LEAF_W, t0 + 0.028, t1, z - 0.08, z + 0.08, grain=0)
    # the brace between the lower hinge-side corner and the upper latch-side corner
    a = (0.12, 0.30)
    b = (LEAF_W - 0.12, 1.55)
    ang = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    ln = math.hypot(b[0] - a[0], b[1] - a[1])
    c = ((a[0] + b[0]) / 2, t1 - 0.003, (a[1] + b[1]) / 2)
    box(m, c, (ln, 0.012, 0.09), "HUT_PlankIn", grain=0, xf=about(rot_y(-ang), c))
    # strap hinges on the outside, barrels on the axis
    for z in (0.28, 1.62):
        bx(m, "HUT_Black", 0.0, 0.55, t0 - 0.006, t0, z - 0.025, z + 0.025, grain=0)
        cyl(m, (0.0, t0 - 0.012, z - 0.045), (0.0, t0 - 0.012, z + 0.045), 0.016, 0.016, "HUT_Black", seg=8, tile=0.3)
    # latch plate, thumb latch and a pull ring on the outside, a plain handle inside
    hx = LEAF_W - 0.10
    bx(m, "HUT_Black", hx - 0.04, hx + 0.04, t0 - 0.005, t0, 0.95, 1.07, grain=2)
    arc = [(hx + 0.035 * math.sin(t), t0 - 0.012 - 0.035 * (1 - math.cos(t)) * 0.0, 1.0 - 0.035 * math.cos(t)) for t in [2 * math.pi * i / 10 for i in range(11)]]
    tube_path(m, [(x, t0 - 0.012 - 0.0, z) for (x, _y, z) in arc], 0.005, "HUT_Black", seg=5)
    bx(m, "HUT_Black", hx - 0.015, hx + 0.015, t1, t1 + 0.012, 0.93, 1.07, grain=2)
    bx(m, "HUT_Black", hx - 0.015, hx + 0.015, t1 + 0.012, t1 + 0.05, 0.99, 1.01, grain=2)
    return m
