"""Preview renders of props_walk's GLBs with the real atlas (the way blender/north/preview_north.py does it) and a contact sheet.

    python3 blender/south/preview_walk.py [name ...] [--res 640] [--samples 24] [--sheet-only]

Writes Saved/South/preview/walk_<name>_<view>.png and walk_sheet.png. The cliff stair stands on its profile ground
(stair_profile.json drawn as a mesh 16 m wide, sand below z -41.4); the bridge spans a brook trench (banks z -0.25, bed z -1.15).
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "north"))
ARGV = sys.argv[1:]
sys.argv = sys.argv[:1]                                  # preview_north parses sys.argv at import
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

import northkit as K  # noqa: E402
import preview_north as PN  # noqa: E402
import props_walk as W  # noqa: E402


def opt(n, d):
    return ARGV[ARGV.index(n) + 1] if n in ARGV else d


# the repo's atlas PNGs are git-lfs pointers in this checkout: use the regenerated copy (northtex.write is deterministic;
# Saved/South/atlas holds it, made here if missing) so the previews show the real atlas
if os.path.getsize(os.path.join(K.GEN, "north_atlas.png")) < 1000:
    alt = os.path.join(K.ROOT, "Saved", "South", "atlas")
    if not os.path.exists(os.path.join(alt, "north_atlas_n.png")):
        import northtex
        northtex.write(alt)
    K.GEN = alt

PN.RES = int(opt("--res", 640))
PN.SAMPLES = int(opt("--samples", 24))
OUT = os.path.join(K.ROOT, "Saved", "South", "preview")
EXPORT = os.path.join(K.ROOT, "blender", "export", "south")


def mat(name, rgb, noise=True):
    """A ground material: the colour broken by a noise (so contact points of posts read in the renders)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = 1.0
    if noise:
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = 1.5
        ramp = nt.nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].color = (rgb[0] * 0.6, rgb[1] * 0.6, rgb[2] * 0.6, 1)
        ramp.color_ramp.elements[1].color = (min(1, rgb[0] * 1.25), min(1, rgb[1] * 1.25), min(1, rgb[2] * 1.25), 1)
        nt.links.new(tex.outputs["Fac"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    return m


def mesh_grid(name, xs, ys, fz, m):
    verts, faces = [], []
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            verts.append((x, y, fz(x, y)))
    nx = len(xs)
    for j in range(len(ys) - 1):
        for i in range(nx - 1):
            a = j * nx + i
            faces.append((a, a + 1, a + nx + 1, a + nx))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def cliff_ground():
    ys = [4.0 - 0.25 * k for k in range(int((4.0 + 46.0) / 0.25) + 1)]
    sand = -41.45
    m = mat("cliff", (0.36, 0.30, 0.20))
    mesh_grid("cliff", [-8.0, 8.0], [y for y in ys if W.gy(y) > sand - 0.05], lambda x, y: W.gy(y), m)
    ms = mat("sand", (0.62, 0.55, 0.40))
    mesh_grid("sand", [-8.0, 8.0], [y for y in ys if W.gy(y) <= sand + 0.05], lambda x, y: W.gy(y), ms)
    mesh_grid("brow", [-8.0, 8.0], [14.0, 4.0], lambda x, y: 0.0, mat("grass", (0.20, 0.30, 0.10)))
    # the cut face at x = +8 (the profile as a filled silhouette in the side view)
    pts = [(8.0, y, W.gy(y)) for y in ys] + [(8.0, ys[-1], -46.0), (8.0, ys[0], -46.0)]
    me = bpy.data.meshes.new("cut")
    me.from_pydata(pts, [], [tuple(range(len(pts)))])
    me.materials.append(mat("cut", (0.30, 0.25, 0.18), noise=False))
    ob = bpy.data.objects.new("cut", me)
    bpy.context.scene.collection.objects.link(ob)


def brook_ground(L):
    def z(x, y):
        ax = abs(x)
        if ax > 2.0:
            return -0.25
        if ax < 1.4:
            return -1.15
        return -1.15 + 0.9 * (ax - 1.4) / 0.6
    xs = sorted(set([-L / 2 - 4, -2.0, -1.4, 1.4, 2.0, L / 2 + 4] + [x * 0.5 for x in range(int(-L - 8), int(L + 8) + 1) if abs(x * 0.5) < L / 2 + 4]))
    mesh_grid("bank", xs, [-6.0, 6.0], z, mat("grass", (0.20, 0.30, 0.10)))
    mesh_grid("water", [-1.8, 1.8], [-6.0, 6.0], lambda x, y: -0.95, mat("water", (0.10, 0.16, 0.18)))


def flat_ground():
    PN.ground(40.0)


def scene(name):
    K.reset_scene()
    PN.setup_world(sun_rot=(0.85, 0.0, 0.75))
    if name == "cliff_stair":
        cliff_ground()
    elif name.startswith("plank_bridge"):
        brook_ground(9.0 if name.endswith("1") else 6.0)
    else:
        flat_ground()
    objs = PN.import_glb(os.path.join(EXPORT, name + ".glb"))
    return objs


def shot(name, view, center, dist, az, el, lens=40, human=None):
    objs = scene(name)
    if human:
        PN.human(*human[:2])
        h = [o for o in bpy.data.objects if o.name.startswith("human")][-1]
        h.location.z = human[2]
    PN.camera(Vector(center), dist, az, el, lens=lens)
    p = os.path.join(OUT, "walk_%s_%s.png" % (name, view))
    PN.render(p, PN.RES, int(PN.RES * 0.78))
    print("rendered", p)
    return p


def views(name):
    L = W.layout()
    if name == "cliff_stair":
        zs, ds = L["zs"], L["ds"]
        i = W.BENCH_LANDINGS[0]
        s = 1 if i % 2 else -1
        return [
            ("34", (0, -17, -21), 62, -0.75, 0.30, 40, None),
            ("side", (0, -16, -20), 70, math.pi / 2, 0.02, 40, None),
            ("side_mid", (6, -16, -19), 20, math.pi / 2 + 0.15, 0.05, 40, None),
            ("front", (0, -17, -21), 66, 0.0, 0.12, 40, None),
            ("bench", (s * 3.4, -(ds[i - 1] + ds[i]) / 2, zs[i] + 0.6), 7.5, -0.5 * s - 0.2, 0.35, 40, (s * 3.0, -ds[i - 1] - 0.9, zs[i])),
            ("top", (-1.6, -3.5, 0.6), 11, -0.35, 0.42, 40, (0.0, 0.8, 0.0)),
            ("foot", (0, -33.0, -40.2), 12, -0.25, 0.12, 40, (3.6, -34.1, W.ground(34.1))),
            ("under", (2.5, -12.0, -10.5), 10, 0.9, -0.05, 35, None),
        ]
    if name.startswith("plank_bridge"):
        return [("34", (0, 0, 0), 9 if name.endswith("0") else 12, -0.6, 0.38, 40, (0.0, 0.0, 0.0)),
                ("side", (0, 0, -0.3), 9 if name.endswith("0") else 12, 0.0, 0.05, 40, None),
                ("close", (2.6 if name.endswith("0") else 4.0, -0.4, 0.0), 3.6, -0.9, 0.35, 40, None)]
    return [("34", (0, -0.6 if name.endswith("2") else 0, 0.5), 8.5, -0.6, 0.42, 40, (0.0, 0.0, 0.5) if not name.endswith("3") else (-1.0, 0.0, 0.42)),
            ("side", (0, 0, 0.3), 8.5, 0.0, 0.06, 40, None),
            ("close", (2.2 if not name.endswith("3") else 0.8, -0.3, 0.4), 3.2, -0.8, 0.32, 40, None)]


def sheet(paths, out):
    from PIL import Image, ImageDraw, ImageFont
    cw, ch = PN.RES, int(PN.RES * 0.78)
    cols = 4
    rows = math.ceil(len(paths) / cols)
    im = Image.new("RGB", (cw * cols, ch * rows), (30, 30, 30))
    d = ImageDraw.Draw(im)
    f = ImageFont.load_default(size=18)
    for k, p in enumerate(paths):
        x, y = (k % cols) * cw, (k // cols) * ch
        im.paste(Image.open(p).convert("RGB").resize((cw, ch)), (x, y))
        d.text((x + 8, y + 6), os.path.basename(p)[5:-4], fill=(255, 255, 255), font=f, stroke_width=2, stroke_fill=(0, 0, 0))
    im.save(out)
    print("sheet", out)


def main():
    os.makedirs(OUT, exist_ok=True)
    names = ["cliff_stair", "plank_bridge_v0", "plank_bridge_v1", "boardwalk_v0", "boardwalk_v1", "boardwalk_v2", "boardwalk_v3"]
    want = [a for a in ARGV if not a.startswith("--") and a in names] or names
    only = [a.split("=")[1] for a in ARGV if a.startswith("--view=")]
    paths = []
    for n in names:
        for v in views(n):
            p = os.path.join(OUT, "walk_%s_%s.png" % (n, v[0]))
            if n in want and "--sheet-only" not in ARGV and (not only or v[0] in only):
                shot(n, *v)
            if os.path.exists(p):
                paths.append(p)
    sheet(paths, os.path.join(OUT, "walk_sheet.png"))


if __name__ == "__main__":
    main()
