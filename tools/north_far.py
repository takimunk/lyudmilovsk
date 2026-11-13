"""The far volcanoes of the North (docs/north.md): three big cones beyond the map, backdrop meshes nobody walks on.
Each is one mesh (a polar grid over the cone, ~36k triangles) with one top-down colour texture (rock, scree, the green
foot, the snow cap with its streaks down the gullies), built here with numpy and committed as finished assets:

  python3 tools/north_far.py            -> blender/export/north/far_<id>.glb (texture inside) + far.json (their entries
                                           for the manifest: where each stands comes from north.far_volcanoes())
  python3 tools/north_far.py --preview out.png   a side view of each, for a look without Unreal

Mesh frame: metres, origin at the cone's centre on its base level, axes as tools/world_mesh.py writes them (map X, Y,
Z -> glTF X, Z, Y), so an actor at W.to_unreal(x, y, base) puts it in place. Needs numpy and Pillow (not run by the
Unreal build).
"""
import io
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import north as N  # noqa: E402

OUT = os.path.join(HERE, "..", "blender", "export", "north")
RINGS, SPOKES = 96, 192
TEX = 2048


def _noise2(rng, shape, octaves, base):
    """Smooth periodic-free value noise by upsampling random grids (bicubic), -1..1."""
    out = np.zeros(shape, np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = base * 2 ** o
        g = rng.random((n + 3, n + 3)).astype(np.float32) * 2 - 1
        img = Image.fromarray(g).resize((shape[1], shape[0]), Image.BICUBIC)
        out += amp * np.asarray(img)
        norm += amp
        amp *= 0.5
    return out / norm


def _angle_noise(rng, theta, octaves, base):
    """Periodic noise of the angle, -1..1."""
    out = np.zeros_like(theta)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        k = base * 2 ** o
        ph = rng.random(2) * 6.2832
        out += amp * (np.sin(k * theta + ph[0]) * 0.6 + np.sin((k + 1) * theta * 1.0 + ph[1]) * 0.4)
        norm += amp
        amp *= 0.55
    return out / norm


def shape(v, t, th, rng):
    """Height (metres over the base) at normalized radius t (0 centre .. 1 foot) and angle th (arrays)."""
    H, R = v["height"], v["radius"]
    tc = v["crater"] / R
    # a stratovolcano's profile: ~33 degrees under the summit, easing out into a long concave apron
    prof = np.exp(-3.4 * t) * (1 - t) ** 0.9 + 0.035 * (1 - t) ** 2
    prof /= np.exp(-3.4 * tc) * (1 - tc) ** 0.9 + 0.035 * (1 - tc) ** 2
    h = H * np.minimum(prof, 1.0)
    # the summit: a crater with a ragged rim, lower on one side
    rim = 1.0 + 0.035 * _angle_noise(rng, th, 3, 2) - 0.05 * (0.5 + 0.5 * np.cos(th - 2.2))
    crater = np.clip(1.0 - t / tc, 0, 1)
    h = np.where(t < tc, H * rim - H * 0.085 * crater ** 0.7, h * (1 + (rim - 1) * np.exp(-((t - tc) / 0.05) ** 2)))
    # barrancos: gullies that deepen down the middle slopes and fan out on the apron
    spokes = _angle_noise(rng, th, 4, 9)
    ridge = 1.0 - np.abs(spokes)
    mid = np.clip((t - tc) / 0.12, 0, 1) * np.clip((1.0 - t) / 0.55, 0, 1)
    h -= H * 0.05 * (ridge ** 2 - 0.4) * mid * (0.6 + 0.4 * _angle_noise(rng, th * 1.0 + t * 3.0, 2, 5))
    # shoulders: an old collapsed flank and a parasitic cone
    h += H * 0.06 * np.exp(-((t - 0.34) / 0.07) ** 2) * np.clip(np.cos(th - 4.4), 0, 1) ** 3
    h += H * 0.035 * np.exp(-((t - 0.52) / 0.035) ** 2 - ((np.angle(np.exp(1j * (th - 0.9)))) / 0.16) ** 2)
    # the foot sinks under the land's own level so the mesh's edge never shows
    h -= H * 0.02 * np.clip((t - 0.9) / 0.1, 0, 1)
    return h.astype(np.float32)


def build(v, seed):
    rng = np.random.default_rng(seed)
    R = v["radius"]
    # rings closer together near the summit where the shape turns fastest
    t = (np.linspace(0, 1, RINGS + 1) ** 1.6).astype(np.float32)[1:]
    th = np.linspace(0, 2 * np.pi, SPOKES, endpoint=False).astype(np.float32)
    T, TH = np.meshgrid(t, th, indexing="ij")
    Hh = shape(v, T, TH, np.random.default_rng(seed))
    h0 = float(shape(v, np.array([1e-4], np.float32), np.array([0.0], np.float32), np.random.default_rng(seed))[0])
    X, Y = T * R * np.cos(TH), T * R * np.sin(TH)
    pos = np.concatenate([[[0.0, 0.0, h0]], np.stack([X, Y, Hh], -1).reshape(-1, 3)]).astype(np.float32)
    tris = []
    for k in range(SPOKES):
        tris.append((0, 1 + k, 1 + (k + 1) % SPOKES))
    for r in range(RINGS - 1):
        a0 = 1 + r * SPOKES
        b0 = a0 + SPOKES
        for k in range(SPOKES):
            k1 = (k + 1) % SPOKES
            tris.append((a0 + k, b0 + k, b0 + k1))
            tris.append((a0 + k, b0 + k1, a0 + k1))
    tris = np.array(tris, np.uint32)
    # smooth normals
    e1, e2 = pos[tris[:, 1]] - pos[tris[:, 0]], pos[tris[:, 2]] - pos[tris[:, 0]]
    fn = np.cross(e1, e2)
    nrm = np.zeros_like(pos)
    for c in range(3):
        np.add.at(nrm, tris[:, c], fn)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
    uv = np.stack([pos[:, 0] / (2 * R) + 0.5, 0.5 - pos[:, 1] / (2 * R)], -1).astype(np.float32)
    return pos, nrm, uv, tris, texture(v, rng, seed)


def texture(v, rng, seed):
    """Top-down colour: u = x / 2R + 0.5, v = 0.5 - y / 2R."""
    H, R = v["height"], v["radius"]
    ax = (np.arange(TEX, dtype=np.float32) + 0.5) / TEX
    U, V = np.meshgrid(ax, ax)
    x, y = (U - 0.5) * 2 * R, (0.5 - V) * 2 * R
    t = np.clip(np.hypot(x, y) / R, 1e-4, 1.0)
    th = np.arctan2(y, x)
    z = shape(v, t, th, np.random.default_rng(seed))
    gy, gx = np.gradient(z, 2 * R / TEX)
    slope = np.hypot(gx, gy)
    # how far into a gully: the angular curvature of the height (hollows hold snow lower down, ribs lose it)
    lap = (np.roll(z, 1, 0) + np.roll(z, -1, 0) + np.roll(z, 1, 1) + np.roll(z, -1, 1) - 4 * z) / (2 * R / TEX) ** 2
    hollow = np.clip(lap * 220.0, -1, 1)
    n1 = _noise2(rng, (TEX, TEX), 5, 6)
    n2 = _noise2(rng, (TEX, TEX), 4, 40)
    zz = z + v["base"]
    rock = np.stack([0.33 + 0.05 * n1, 0.30 + 0.045 * n1, 0.30 + 0.04 * n1], -1)          # grey-brown andesite
    red = np.stack([0.36 + 0.03 * n2, 0.23 + 0.02 * n2, 0.19 + 0.02 * n2], -1)            # oxidised cinder
    scree = np.stack([0.42 + 0.04 * n2, 0.39 + 0.04 * n2, 0.36 + 0.035 * n2], -1)
    green = np.stack([0.27 + 0.04 * n1, 0.36 + 0.05 * n1, 0.19 + 0.03 * n1], -1)          # alder and meadow at the foot
    col = rock * (0.85 + 0.15 * n2[..., None])
    col = np.where((n1 > 0.25)[..., None], col * 0.6 + red * 0.4, col)
    k = np.clip((slope - 0.35) / 0.3, 0, 1)[..., None] * 0.0 + np.clip((0.45 - slope) / 0.3, 0, 1)[..., None]
    col = col * (1 - 0.45 * k) + scree * 0.45 * k
    veg_line = 420.0 + 160.0 * n1 - 120.0 * hollow
    g = np.clip((veg_line - zz) / 180.0, 0, 1)[..., None] * np.clip((0.55 - slope) / 0.25, 0, 1)[..., None]
    col = col * (1 - g) + green * g
    # the snow: solid over the snow line, reaching far down the gullies in streaks, gone from the steep ribs
    line = v["snow"] + 260.0 * n1 + 90.0 * n2 - 520.0 * np.clip(hollow, 0, 1) + 300.0 * np.clip(-hollow, 0, 1) \
        + 260.0 * np.clip((slope - 0.75) / 0.3, 0, 1)
    s = np.clip((zz - line) / 120.0, 0, 1)[..., None]
    snow = np.stack([0.93 + 0.03 * n2, 0.95 + 0.03 * n2, 0.99 + 0.0 * n2], -1)
    col = col * (1 - s) + snow * s
    # a little relief painted in (far away the mesh's own shading is too smooth to read the gullies)
    shade = np.clip(1.0 + 0.16 * hollow * -1.0 + 0.5 * (gx * 0.5 - gy * 0.3) * 0.25, 0.75, 1.15)[..., None]
    col = np.clip(col * shade, 0, 1)
    return Image.fromarray((col * 255).astype(np.uint8))


def write_glb(path, name, pos, nrm, uv, tris, tex):
    def axes(a):
        return np.ascontiguousarray(a[:, [0, 2, 1]]).astype(np.float32)
    P, Nn = axes(pos), axes(nrm)
    # front faces along the normals after the axis swap
    e1, e2 = P[tris[:, 1]] - P[tris[:, 0]], P[tris[:, 2]] - P[tris[:, 0]]
    flip = np.einsum("ij,ij->i", np.cross(e1, e2), Nn[tris[:, 0]]) < 0
    tris = tris.copy()
    tris[flip] = tris[flip][:, [0, 2, 1]]
    jpg = io.BytesIO()
    tex.save(jpg, "JPEG", quality=88)
    blob = bytearray()
    views = []

    def view(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        v = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            v["target"] = target
        blob.extend(data)
        views.append(v)
        return len(views) - 1

    acc = [
        {"bufferView": view(P.tobytes(), 34962), "componentType": 5126, "count": len(P), "type": "VEC3",
         "min": [float(c) for c in P.min(0)], "max": [float(c) for c in P.max(0)]},
        {"bufferView": view(Nn.tobytes(), 34962), "componentType": 5126, "count": len(P), "type": "VEC3"},
        {"bufferView": view(uv.astype(np.float32).tobytes(), 34962), "componentType": 5126, "count": len(P), "type": "VEC2"},
        {"bufferView": view(tris.astype(np.uint32).tobytes(), 34963), "componentType": 5125, "count": int(tris.size), "type": "SCALAR"},
    ]
    img_view = view(jpg.getvalue())
    gl = {"asset": {"version": "2.0", "generator": "lyudmilovsk north_far.py"}, "scene": 0, "scenes": [{"nodes": [0]}],
          "nodes": [{"name": name, "mesh": 0}],
          "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                                    "indices": 3, "material": 0}]}],
          "materials": [{"name": "North_Far_" + name, "pbrMetallicRoughness": {
              "baseColorTexture": {"index": 0}, "metallicFactor": 0.0, "roughnessFactor": 0.92}}],
          "textures": [{"source": 0, "sampler": 0}], "samplers": [{"wrapS": 33071, "wrapT": 33071}],
          "images": [{"bufferView": img_view, "mimeType": "image/jpeg"}],
          "accessors": acc, "bufferViews": views, "buffers": [{"byteLength": 0}]}
    while len(blob) % 4:
        blob.append(0)
    gl["buffers"][0]["byteLength"] = len(blob)
    js = json.dumps(gl, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob), 0x004E4942) + bytes(blob))


LAND_X = (-15000.0, 2600.0)       # the far land's box, map metres; the sea cuts it in the west
LAND_Y = (2300.0, 17000.0)
LAND_CELL = 120.0
LAND_TEX = 2048


def far_land():
    """The country beyond the map's north and east edges: rolling tundra hills that climb away from the landscape, so
    nobody on the Cone's rim or the northern foot looks over the map's edge into nothing. One grid mesh in map metres
    (its origin is the map's origin), a hole where the landscape lies; the sea cuts it in the west."""
    import world_map as W
    rng = np.random.default_rng(77)
    xs = np.arange(LAND_X[0], LAND_X[1] + 1, LAND_CELL, dtype=np.float32)
    ys = np.arange(LAND_Y[0], LAND_Y[1] + 1, LAND_CELL, dtype=np.float32)
    # the landscape's own edges fall on grid lines so the hole's rim is straight
    xs = np.unique(np.concatenate([xs, [W.X_MIN - 6.0]]))
    ys = np.unique(np.concatenate([ys, [W.Y_MAX + 60.0]]))
    X, Y = np.meshgrid(xs, ys)
    shore = np.array([W.shore_x(float(y)) for y in ys], np.float32)[:, None]
    # metres outside the landscape's rectangle (0 inside it)
    dx = np.maximum(W.X_MIN - 6.0 - X, 0.0)
    dy = np.maximum(Y - (W.Y_MAX + 60.0), 0.0)
    d = np.hypot(dx, dy)
    n1 = np.asarray(Image.fromarray((rng.random((14, 14)) * 2 - 1).astype(np.float32)).resize(X.shape[::-1], Image.BICUBIC))
    n2 = np.asarray(Image.fromarray((rng.random((45, 45)) * 2 - 1).astype(np.float32)).resize(X.shape[::-1], Image.BICUBIC))
    far = np.clip(d / 2500.0, 0, 1)
    # it starts at the landscape's own height along its north edge (the real land's), easing to the far country's
    edge = np.array([W.height(float(min(max(x, W.X_MIN + 2.0), W.X_MAX - 2.0)), W.Y_MAX + 55.0) for x in xs], np.float32)[None, :]
    edge = np.maximum(edge, -20.0)
    z = 20.0 + (edge - 20.0) * np.exp(-d / 350.0) + 0.07 * np.minimum(d, 1500.0) + 0.04 * np.maximum(d - 1500.0, 0.0) + (170.0 * n1 + 60.0 * n2) * far + 240.0 * np.clip((d - 3000.0) / 9000.0, 0, 1) * (0.6 + 0.4 * n1)
    # the coast: an 80 m cliff like the map's own, then the sea bed
    s = shore - X
    z = np.where(s < 90.0, -55.0 + (z + 55.0) * np.clip((s - 25.0) / 65.0, 0, 1) ** 0.6, z)
    z = np.where(s < 0.0, -55.0, z)
    # nothing south of the landscape's north edge: east of the map lie the Topi (tools/topi.py), their own ground
    inside = Y < W.Y_MAX + 60.0 - 1.0
    pos = np.stack([X, Y, z], -1).astype(np.float32)
    ny, nx = X.shape
    idx = np.arange(ny * nx).reshape(ny, nx)
    quads = []
    for j in range(ny - 1):
        for i in range(nx - 1):
            # no quads over the landscape (its corner cell included) or far out at sea
            if inside[j, i] and inside[j + 1, i + 1] and inside[j, i + 1] and inside[j + 1, i]:
                continue
            if xs[i] > shore[j, 0] + 400.0:
                continue
            a, b, c, e = idx[j, i], idx[j, i + 1], idx[j + 1, i + 1], idx[j + 1, i]
            quads.append((a, b, c))
            quads.append((a, c, e))
    tris = np.array(quads, np.uint32)
    P = pos.reshape(-1, 3)
    e1, e2 = P[tris[:, 1]] - P[tris[:, 0]], P[tris[:, 2]] - P[tris[:, 0]]
    fn = np.cross(e1, e2)
    fn[fn[:, 2] < 0] *= -1
    nrm = np.zeros_like(P)
    for k in range(3):
        np.add.at(nrm, tris[:, k], fn)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
    uv = np.stack([(P[:, 0] - LAND_X[0]) / (LAND_X[1] - LAND_X[0]), 1.0 - (P[:, 1] - LAND_Y[0]) / (LAND_Y[1] - LAND_Y[0])], -1)
    # colour: meadow green near, tundra brown-green higher, scree and old snow on the tops, sand at the shore
    t1 = _noise2(rng, (LAND_TEX, LAND_TEX), 5, 8)
    t2 = _noise2(rng, (LAND_TEX, LAND_TEX), 4, 60)
    zi = np.asarray(Image.fromarray(z.astype(np.float32)).resize((LAND_TEX, LAND_TEX), Image.BILINEAR))[::-1]
    green = np.stack([0.26 + 0.04 * t1, 0.36 + 0.05 * t1, 0.17 + 0.03 * t1], -1)
    tundra = np.stack([0.36 + 0.05 * t2, 0.35 + 0.04 * t2, 0.2 + 0.03 * t2], -1)
    scree = np.stack([0.4 + 0.04 * t2, 0.38 + 0.04 * t2, 0.35 + 0.04 * t2], -1)
    k = np.clip((zi - 150.0 + 60.0 * t1) / 250.0, 0, 1)[..., None]
    col = green * (1 - k) + tundra * k
    k = np.clip((zi - 520.0 + 90.0 * t1) / 200.0, 0, 1)[..., None]
    col = col * (1 - k) + scree * k
    k = np.clip((zi - 640.0 + 200.0 * t2) / 60.0, 0, 1)[..., None] * 0.85
    col = col * (1 - k) + np.array([0.93, 0.95, 0.99]) * k
    k = np.clip((-20.0 - zi) / 20.0, 0, 1)[..., None]
    col = col * (1 - k) + np.array([0.3, 0.28, 0.25]) * k          # the cliff's dark lava and the bed
    tex = Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8))
    return P, nrm.astype(np.float32), uv.astype(np.float32), tris, tex


def preview(out):
    """Each cone seen from the side under a low sun: orthographic, painter's algorithm over the triangles."""
    vs = N.far_volcanoes()
    W_, H_ = 1500, 620
    sheet = Image.new("RGB", (W_, H_ * len(vs)), (150, 185, 225))
    for row, v in enumerate(vs):
        pos, nrm, uv, tris, tex = build(v, 100 + row)
        t = np.asarray(tex.resize((512, 512))).astype(np.float32) / 255
        sun = np.array([0.5, -0.6, 0.62])
        sun /= np.linalg.norm(sun)
        lit = np.clip(nrm @ sun, 0.0, 1) * 0.85 + 0.25
        c = t[np.clip((uv[:, 1] * 511).astype(int), 0, 511), np.clip((uv[:, 0] * 511).astype(int), 0, 511)] * lit[:, None]
        s = (W_ * 0.47) / v["radius"]
        px = W_ / 2 + pos[:, 0] * s
        py = H_ * (row + 1) - 30 - pos[:, 2] * s
        order = np.argsort(-pos[tris].mean(1)[:, 1])           # far (north, +y) first, seen from the south
        from PIL import ImageDraw
        d = ImageDraw.Draw(sheet)
        for i in order[::-1][::1]:
            pass
        for i in order:
            a, b, c3 = tris[i]
            col = tuple(int(q * 255) for q in np.clip((c[a] + c[b] + c[c3]) / 3, 0, 1))
            d.polygon([(px[a], py[a]), (px[b], py[b]), (px[c3], py[c3])], fill=col)
    sheet.save(out)


def main():
    if "--preview" in sys.argv:
        preview(sys.argv[sys.argv.index("--preview") + 1])
        return
    os.makedirs(OUT, exist_ok=True)
    entries = []
    for i, v in enumerate(N.far_volcanoes()):
        pos, nrm, uv, tris, tex = build(v, 100 + i)
        name = "far_" + v["id"]
        write_glb(os.path.join(OUT, name + ".glb"), name, pos, nrm, uv, tris, tex)
        entries.append({"kind": "far_volcano", "id": v["id"], "file": name + ".glb", "tris": int(len(tris)),
                        "size": [2 * v["radius"], 2 * v["radius"], round(float(pos[:, 2].max()), 1)], "collision": "none",
                        "notes": "backdrop: no collision, no shadow casting, no Nanite, no distance field, no Lumen "
                                 "card capture; placed at north.json far_volcanoes[id] (x, y, base)"})
        print(name, len(tris), "tris, top", round(float(pos[:, 2].max())), "m")
    pos, nrm, uv, tris, tex = far_land()
    write_glb(os.path.join(OUT, "far_land.glb"), "far_land", pos, nrm, uv, tris, tex)
    entries.append({"kind": "far_land", "id": "land", "file": "far_land.glb", "tris": int(len(tris)),
                    "size": [LAND_X[1] - LAND_X[0], LAND_Y[1] - LAND_Y[0], round(float(pos[:, 2].max()), 1)], "collision": "none",
                    "notes": "backdrop: the country beyond the map's north and east edges; map metres from the map's "
                             "origin: place at W.to_unreal(0, 0, 0); no collision, no shadow casting, no Nanite"})
    print("far_land", len(tris), "tris, top", round(float(pos[:, 2].max())), "m")
    with open(os.path.join(OUT, "far.json"), "w") as f:
        json.dump({"meshes": entries}, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
