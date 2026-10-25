"""Puts the hi3d pigeon (gen/hi3d_dec.blend) into the bind pose of SKEL_FeralPigeon: the bird's frame (X forward, Z up,
Y its left, metres, origin between the feet), its size, its neck lifted (the model was made from a view from below and
came out with the body tilted up and the head bent down). FIT holds the numbers; fit_mesh(ob) applies them in place.
"""
import math

import numpy as np

FIT = {
    "pitch_deg": -21.0,         # nose up, after the frame swap
    "scale": 0.667,             # span 1.0 -> the species' 66.7 cm
    "offset": (0.0, 0.0, 0.07),
    "neck_pivot": (0.045, 0.085),   # x, z of the neck's base (after the steps above)
    "neck_deg": -55.0,          # head and neck lifted about the pivot (negative = up)
    "neck_blend": 0.035,        # metres over which the lift fades in
    "head_shift": (0.0, 0.0),   # then the head moved (x, z), with the same fade
}


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def fit_points(co, F=FIT):
    """co: (n, 3) in the glTF-import frame (span X, head +Z, belly -Y) -> the bird's frame."""
    p = co[:, [2, 0, 1]].copy()                       # x <- z, y <- x, z <- y
    p = p @ rot_y(math.radians(F["pitch_deg"])).T
    p = p * F["scale"] + np.array(F["offset"])
    # the neck: points ahead of the pivot turn about it, fading in over neck_blend (only near the middle, not the wings)
    px, pz = F["neck_pivot"]
    u = np.clip((p[:, 0] - px) / F["neck_blend"], 0, 1)
    # only the neck and head: not the shoulders and wing roots beside it, nor the breast below it
    side = np.clip(1 - (np.abs(p[:, 1]) - F.get("neck_halfwidth", 0.024)) / 0.012, 0, 1)
    # (the bill hangs low before the lift: whatever is far enough forward is head)
    low = np.maximum(np.clip((p[:, 2] - F.get("breast_z", 0.035)) / 0.02, 0, 1), np.clip((p[:, 0] - 0.095) / 0.015, 0, 1))
    u = u * u * (3 - 2 * u) * side * side * (3 - 2 * side) * low * low * (3 - 2 * low)
    a = math.radians(F["neck_deg"]) * u
    dx, dz = p[:, 0] - px, p[:, 2] - pz
    c, s = np.cos(a), np.sin(a)
    p[:, 0] = px + c * dx + s * dz
    p[:, 2] = pz - s * dx + c * dz
    p[:, 0] += F["head_shift"][0] * u
    p[:, 2] += F["head_shift"][1] * u
    return p


def fit_mesh(ob, F=FIT):
    n = len(ob.data.vertices)
    co = np.empty(n * 3)
    ob.data.vertices.foreach_get("co", co)
    ob.data.vertices.foreach_set("co", fit_points(co.reshape(-1, 3), F).ravel())
    ob.data.update()


def vertex_albedo(ob):
    """(n, 3) base colour at every vertex (from its first loop's UV), linear 0..1, and the image size."""
    import bpy  # noqa: F401
    me = ob.data
    img = next(n.image for n in ob.data.materials[0].node_tree.nodes if n.type == "TEX_IMAGE" and n.outputs[0].links
               and n.outputs[0].links[0].to_socket.name == "Base Color")
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vi = np.empty(len(me.loops), np.int64)
    me.loops.foreach_get("vertex_index", vi)
    vuv = np.zeros((len(me.vertices), 2))
    vuv[vi] = uv
    x = np.clip((vuv[:, 0] * w).astype(int), 0, w - 1)
    y = np.clip((vuv[:, 1] * h).astype(int), 0, h - 1)
    return px[y, x, :3]


def find_eyes(ob):
    """Centres of the two irises (orange-brown, in the head), bird frame."""
    n = len(ob.data.vertices)
    co = np.empty(n * 3)
    ob.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    c = vertex_albedo(ob)
    head = (co[:, 0] > 0.06) & (np.abs(co[:, 1]) < 0.03) & (co[:, 2] > 0.07)
    iris = head & (c[:, 0] > 0.12) & (c[:, 0] > c[:, 1] * 1.6) & (c[:, 1] > c[:, 2] * 1.1)
    out = []
    for side in (1, -1):
        s = co[iris & (co[:, 1] * side > 0.004)]
        out.append(s.mean(0) if len(s) else None)
        print("EYE", "L" if side > 0 else "R", len(s), None if not len(s) else s.mean(0).round(4))
    return out
