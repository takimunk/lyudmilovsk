"""Small face helpers shared by the diner's builders (building.py, seating.py, counter.py, kitchen.py)."""
import math  # noqa: F401

from mathutils import Matrix, Vector

from diner_common import box


def oq(bm, pts, mi, want):
    """A face with its normal along want."""
    vs = [Vector(p) for p in pts]
    n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
    if n.length < 1e-12 and len(vs) > 3:
        n = (vs[2] - vs[0]).cross(vs[3] - vs[0])
    if n.dot(Vector(want)) < 0:
        vs.reverse()
    f = bm.faces.new([bm.verts.new(v) for v in vs])
    f.material_index = mi
    return f


def ybox(bm, mi, c, size, yaw, faces="all"):
    box(bm, c, size, mi, rot=Matrix.Rotation(yaw, 3, "Z"), faces=faces)
