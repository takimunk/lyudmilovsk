"""Where a player capsule (r 34 cm, 192 cm tall, steps up to 6 cm) can walk in each vestibule
(design/transport/game/SM_TR_Vestibule_<Style>.glb, Content/Transport/vestibules.json): a 15 cm grid of the cells where
it stands clear of the mesh on the floor, flooded from the gallery (X +2.5 m) through the connector's gap; the room, the
diagram spot, the inside of the door and the street past the entrance must be reached. TR_DEBUG=<dir>: walk_<style>.png.
  Blender -b -P blender/transport/check_vestibules.py [-- style ...]      (exit code 1 when one is blocked)
"""
import bpy
import bmesh
import math
import json
import os
import sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
GAME = os.path.join(ROOT, "design", "transport", "game")
JSON = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Transport", "vestibules.json")
R, H, STEP = 0.34, 1.92, 0.06


def b(p):
    """UE cm (x, y) -> Blender m."""
    return Vector((p[0] / 100.0, -p[1] / 100.0, 0.0))


def check(style, d):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(GAME, d["mesh"] + ".glb"))
    o = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.transform(o.matrix_world)
    t = BVHTree.FromBMesh(bm)
    door = b(d["doors"][0]["at"])
    yaw = math.radians(-d["doors"][0]["yaw"])
    out = Vector((math.cos(yaw), math.sin(yaw), 0))
    c = b(d["room"]["centre"])
    dg = b(d["spots"]["diagram"]["at"])
    E = d["track_gap"] / 200.0 - 1.05
    tx = d["tower_x"] / 100.0
    h = d["floor_hole"]
    hx0, hx1 = min(h[0], h[2]) / 100.0, max(h[0], h[2]) / 100.0
    hy0, hy1 = min(-h[1], -h[3]) / 100.0, max(-h[1], -h[3]) / 100.0
    ex = {"+x": Vector((1, 0, 0)), "-x": Vector((-1, 0, 0)), "+y": Vector((0, -1, 0)), "-y": Vector((0, 1, 0))}[d["core_exit"]]
    start = Vector((tx, 0, 0)) + ex * ((hx1 - hx0) / 2 + 0.6)
    vs = [v.co for v in bm.verts]
    x0, x1 = min(v.x for v in vs) - 0.5, max(v.x for v in vs) + 0.5
    y0, y1 = -E - 0.5, E + 0.5
    G = 0.15
    nx, ny = int((x1 - x0) / G) + 1, int((y1 - y0) / G) + 1
    cell = lambda i, j: Vector((x0 + i * G, y0 + j * G, 0))

    def free(i, j):
        p = cell(i, j)
        if hx0 - 0.1 <= p.x <= hx1 + 0.1 and hy0 - 0.1 <= p.y <= hy1 + 0.1:
            return False                      # the core's hole (its own walls stand there)
        loc, nor, idx, dist = t.ray_cast(Vector((p.x, p.y, 0.5)), Vector((0, 0, -1)), 0.6)
        if loc is None or abs(loc.z) > 0.03:
            return False
        z = STEP + R
        while z <= H - R + 1e-6:
            if t.find_nearest(Vector((p.x, p.y, z)), R - 0.005)[0] is not None:
                return False
            z += 0.25
        return True

    ok_cells = {(i, j) for i in range(nx) for j in range(ny) if free(i, j)}
    near = lambda v: min(ok_cells, key=lambda k: (cell(*k) - v).length)
    s0 = near(start)
    seen = {s0}
    todo = [s0]
    while todo:
        i, j = todo.pop()
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                k = (i + di, j + dj)
                if k in ok_cells and k not in seen:
                    seen.add(k)
                    todo.append(k)
    ok = True
    if (cell(*s0) - start).length > 0.4:
        print("VESTCHECK %s: no floor at the core's exit (%.2f, %.2f)" % (style, start.x, start.y))
        ok = False
    stop = d.get("stop_x", 0.0) / 100.0
    in_room = d.get("core_in_room")
    targets = [("diagram", dg + (c - dg) * 0.15), ("inside the door", door - out * 0.8)]
    if not in_room:
        targets.append(("room", c))
    for sy in (1, -1):
        for dx in (-2.0, 0.0, 2.0):
            targets.append(("edge %+d stop %+.0f" % (sy, dx), Vector((stop + dx, sy * (E - 0.7), 0))))
    for name, v in targets:
        k = min(seen, key=lambda q: (cell(*q) - v).length)
        dist = (cell(*k) - v).length
        if dist > 0.3:
            print("VESTCHECK %s: can't reach the %s (%.2f, %.2f): the nearest reachable is %.2f m off" % (style, name, v.x, v.y, dist))
            ok = False
    # the spec: nothing above the floor within 1.35 m of a track's centre; 22 m along the line; 16 m tall
    for f in bm.faces:
        p = f.calc_center_median()
        if abs(p.y) > E - 0.3 + 1e-3 and p.z > 0.02:
            print("VESTCHECK %s: something in a track's keep-clear at (%.2f, %.2f, %.2f)" % (style, p.x, p.y, p.z))
            ok = False
            break
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    if hi.x - lo.x > 22.0 or hi.z > 16.0 or lo.z < -0.601:
        print("VESTCHECK %s: out of its box: %s .. %s" % (style, tuple(round(x, 2) for x in lo), tuple(round(x, 2) for x in hi)))
        ok = False
    # a picture of the walkable cells: white reachable, grey free but cut off
    try:
        img = bpy.data.images.new("walk", nx, ny)
        px = [0.0] * (nx * ny * 4)
        for (i, j) in ok_cells:
            g = 1.0 if (i, j) in seen else 0.35
            px[(j * nx + i) * 4: (j * nx + i) * 4 + 4] = [g, g, g, 1.0]
        img.pixels = px
        img.filepath_raw = os.path.join(os.environ.get("TR_DEBUG", "/tmp"), "walk_%s.png" % style)
        img.file_format = "PNG"
        if os.environ.get("TR_DEBUG"):
            img.save()
    except Exception as ex:
        print("VESTCHECK picture failed", ex)
    print("VESTCHECK %s: %s" % (style, "OK" if ok else "BLOCKED"))
    return ok


data = json.load(open(JSON))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
styles = argv or sorted(s for s in data if not s.startswith("_"))
bad = [s for s in styles if not check(s, data[s])]
print("VESTCHECK done, %d of %d blocked %s" % (len(bad), len(styles), " ".join(bad)))
sys.exit(1 if bad else 0)
