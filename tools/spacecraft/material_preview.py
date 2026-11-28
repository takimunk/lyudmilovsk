"""The craft's materials run in numpy: the very HLSL of unreal/scripts/import_spacecraft.py, not a repaint of it.

    python3 tools/spacecraft/material_preview.py [planet] [plasma] [crt] [panel] [glass]
        -> design/spacecraft/preview/planet_*.png, plasma.png, crt.png, panel.png, glass.png

import_spacecraft.py's custom nodes are written in a small branch-free subset of HLSL (declarations with a value,
assignments, [unroll] loops with constant bounds, constant arrays, one return). run() turns such a node into Python and
executes it over a whole picture with vector types that behave like HLSL's (float2/3/4 with swizzles; mixing sizes is
an error here, where HLSL would silently truncate), textures with mips and SampleLevel, ddx / ddy / fwidth as the
picture's own differences. smoke() executes every node of every material once (the importer's lint calls it): a
misspelt name, a float2 added to a float3 or a missing function fails here instead of on the Mac.

What this cannot show: Unreal's tone curve and exposure (here: 1 - exp(-x * exposure), then sRGB), the lit materials'
lighting, compression, and anything the engine feeds in wrongly.
"""
import math
import os
import re
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "design", "spacecraft", "preview")
TEXDIRS = {"/Game/Europa/Textures/": os.path.join(REPO, "textures", "europa"), "": os.path.join(REPO, "textures", "spacecraft")}


class HlslError(Exception):
    pass


class V:
    """An HLSL float2/3/4 over a picture: d_[..., n]."""
    __array_ufunc__ = None             # an array times a V is the V's business

    def __init__(self, a):
        self.d_ = np.asarray(a, np.float32)
        self.n = self.d_.shape[-1]

    def __getattr__(self, name):
        idx = []
        for ch in name:
            k = "xyzw".find(ch)
            if k < 0:
                k = "rgba".find(ch)
            if k < 0 or k >= self.n:
                raise HlslError("no .%s on a float%d" % (name, self.n))
            idx.append(k)
        return self.d_[..., idx[0]] if len(idx) == 1 else V(self.d_[..., idx])

    def _bin(self, o, f, swap=False):
        if isinstance(o, V):
            if o.n != self.n:
                raise HlslError("float%d with float%d" % (self.n, o.n))
            b = o.d_
        else:
            b = np.asarray(o, np.float32)[..., None]
        return V(f(b, self.d_) if swap else f(self.d_, b))

    def __add__(self, o): return self._bin(o, np.add)
    def __radd__(self, o): return self._bin(o, np.add, True)
    def __sub__(self, o): return self._bin(o, np.subtract)
    def __rsub__(self, o): return self._bin(o, np.subtract, True)
    def __mul__(self, o): return self._bin(o, np.multiply)
    def __rmul__(self, o): return self._bin(o, np.multiply, True)
    def __truediv__(self, o): return self._bin(o, np.divide)
    def __rtruediv__(self, o): return self._bin(o, np.divide, True)
    def __neg__(self): return V(-self.d_)

    def __getitem__(self, k):                      # a float4 indexed by a number per pixel
        k = np.asarray(k)
        if k.ndim == 0:
            return self.d_[..., int(k)]
        a = np.broadcast_to(self.d_, k.shape + (self.n,))
        return np.take_along_axis(a, k[..., None].astype(np.int64), -1)[..., 0]


class Arr:
    """A constant array."""

    def __init__(self, items):
        self.items = items
        self.vec = isinstance(items[0], V)
        self.table = np.stack([i.d_ if self.vec else np.float32(i) for i in items], 0)

    def __getitem__(self, k):
        if isinstance(k, (int, np.integer)):
            return self.items[k]
        t = self.table[np.clip(np.asarray(k).astype(np.int64), 0, len(self.items) - 1)]
        return V(t) if self.vec else t


def _f(x):
    return x.d_ if isinstance(x, V) else np.asarray(x, np.float32)


def _map(fn, *args):
    ns = {a.n for a in args if isinstance(a, V)}
    if len(ns) > 1:
        raise HlslError("float sizes %s in one call" % sorted(ns))
    if not ns:
        return fn(*[np.asarray(a, np.float32) for a in args])
    return V(fn(*[a.d_ if isinstance(a, V) else np.asarray(a, np.float32)[..., None] for a in args]))


def _vec(n):
    def make(*args):
        parts = []
        for a in args:
            parts += [a.d_[..., k] for k in range(a.n)] if isinstance(a, V) else [np.asarray(a, np.float32)]
        if len(parts) != n:
            raise HlslError("float%d(...) from %d numbers" % (n, len(parts)))
        return V(np.stack(np.broadcast_arrays(*parts), -1))
    return make


def _smoothstep(a, b, x):
    def f(a, b, x):
        t = np.clip((x - a) / np.where(b == a, 1.0e-20, b - a), 0.0, 1.0)
        return t * t * (3.0 - 2.0 * t)
    return _map(f, a, b, x)


def _pow(a, b):
    def f(a, b):
        if np.any((a < 0) & np.isfinite(a)):
            raise HlslError("pow of a negative number")
        return np.power(np.maximum(a, 0.0), b)
    return _map(f, a, b)


def _dot(a, b):
    if not (isinstance(a, V) and isinstance(b, V) and a.n == b.n):
        raise HlslError("dot of unlike things")
    return (a.d_ * b.d_).sum(-1)


def _length(a):
    if not isinstance(a, V):
        raise HlslError("length of a float")
    return np.sqrt((a.d_ * a.d_).sum(-1))


def _grad(x, axis):
    def g(a):
        if a.ndim < 2 or a.shape[0] < 2 or a.shape[1] < 2:
            return np.zeros_like(a)
        return np.gradient(a, axis=axis)
    return V(np.stack([g(x.d_[..., k]) for k in range(x.n)], -1)) if isinstance(x, V) else g(np.asarray(x, np.float32))


def _fwidth(x):
    a, b = _grad(x, 1), _grad(x, 0)
    return V(np.abs(a.d_) + np.abs(b.d_)) if isinstance(x, V) else np.abs(a) + np.abs(b)


def _cross(a, b):
    if not (isinstance(a, V) and isinstance(b, V) and a.n == 3 and b.n == 3):
        raise HlslError("cross wants two float3")
    return V(np.cross(a.d_, b.d_))


FUNCS = {
    "float2": _vec(2), "float3": _vec(3), "float4": _vec(4),
    "_int": lambda x: x if isinstance(x, int) else np.floor(np.asarray(x, np.float32)).astype(np.int64),
    "_float": lambda x: float(x) if isinstance(x, int) else np.asarray(x, np.float32),
    "abs": lambda x: _map(np.abs, x), "floor": lambda x: _map(np.floor, x), "frac": lambda x: _map(lambda a: a - np.floor(a), x),
    "round": lambda x: _map(np.round, x), "sin": lambda x: _map(np.sin, x), "cos": lambda x: _map(np.cos, x),
    "exp": lambda x: _map(np.exp, x), "exp2": lambda x: _map(np.exp2, x),
    "log2": lambda x: _map(lambda a: np.log2(np.maximum(a, 1.0e-30)), x),
    "sqrt": lambda x: _map(lambda a: np.sqrt(np.maximum(a, 0.0)), x),
    "asin": lambda x: _map(np.arcsin, x), "acos": lambda x: _map(np.arccos, x), "atan2": lambda y, x: _map(np.arctan2, y, x),
    "radians": lambda x: _map(np.radians, x), "degrees": lambda x: _map(np.degrees, x),
    "saturate": lambda x: _map(lambda a: np.clip(a, 0.0, 1.0), x),
    "min": lambda a, b: _map(np.minimum, a, b), "max": lambda a, b: _map(np.maximum, a, b),
    "clamp": lambda x, a, b: _map(lambda x, a, b: np.minimum(np.maximum(x, a), b), x, a, b),
    "lerp": lambda a, b, t: _map(lambda a, b, t: a + (b - a) * t, a, b, t),
    "step": lambda e, x: _map(lambda e, x: (x >= e).astype(np.float32), e, x),
    "fmod": lambda a, b: _map(np.fmod, a, b),
    "smoothstep": _smoothstep, "pow": _pow, "dot": _dot, "length": _length, "cross": _cross,
    "normalize": lambda a: V(a.d_ / np.maximum(_length(a), 1.0e-30)[..., None]),
    "ddx": lambda x: _grad(x, 1), "ddy": lambda x: _grad(x, 0), "fwidth": _fwidth,
    "Arr": Arr,
}


class Texture:
    """A picture with its mips: SampleLevel(sampler, uv, lod) as the GPU does it (bilinear, between two mips)."""

    def __init__(self, path, srgb, clamp=(False, False)):
        im = np.asarray(Image.open(path), np.float32) / 255.0
        if im.ndim == 2:
            im = np.stack([im, im, im, np.ones_like(im)], -1)
        elif im.shape[-1] == 3:
            im = np.concatenate([im, np.ones_like(im[..., :1])], -1)
        if srgb:
            im = im.copy()
            im[..., :3] = im[..., :3] ** 2.2
        self.mips = [im]
        while min(self.mips[-1].shape[:2]) > 1:
            a = self.mips[-1]
            h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
            self.mips.append(a[:h, :w].reshape(h // 2, 2, w // 2, 2, 4).mean((1, 3)))
        self.clamp = clamp

    def _bilinear(self, k, u, v):
        a = self.mips[k]
        h, w = a.shape[:2]
        x, y = u * w - 0.5, v * h - 0.5
        x0, y0 = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
        fx, fy = (x - x0)[..., None], (y - y0)[..., None]

        def at(xx, yy):
            xx = np.clip(xx, 0, w - 1) if self.clamp[0] else xx % w
            yy = np.clip(yy, 0, h - 1) if self.clamp[1] else yy % h
            return a[yy, xx]
        return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy

    def SampleLevel(self, sampler, uv, lod):
        if not (isinstance(uv, V) and uv.n == 2):
            raise HlslError("SampleLevel wants a float2")
        u, v, lod = np.broadcast_arrays(uv.d_[..., 0], uv.d_[..., 1], np.asarray(lod, np.float32))
        lod = np.clip(np.nan_to_num(lod), 0.0, len(self.mips) - 1.0)
        out = np.zeros(u.shape + (4,), np.float32)
        l0 = np.floor(lod).astype(np.int64)
        for k in np.unique(l0):
            sel = l0 == k
            a = self._bilinear(int(k), u[sel], v[sel])
            b = self._bilinear(min(int(k) + 1, len(self.mips) - 1), u[sel], v[sel])
            f = (lod[sel] - k)[..., None]
            out[sel] = a * (1 - f) + b * f
        return V(out)


_TEX = {}


def texture(name, sampler):
    key = (name, sampler)
    if key not in _TEX:
        for prefix, folder in TEXDIRS.items():
            if name.startswith(prefix) and (prefix or not name.startswith("/")):
                base = name[len(prefix):]
                for cand in (base + ".png", base + "_ru.png"):
                    path = os.path.join(folder, cand)
                    if os.path.isfile(path):
                        clamp = (base in ("T_SC_island", "T_SC_island_M", "T_SC_dials", "T_SC_digits", "T_SC_record_labels")
                                 or base.startswith(("T_SC_labels", "T_SC_sleeves")),) * 2
                        if "planet" in base or "clouds" in base or base == "T_SC_ball":
                            clamp = (False, True)
                        _TEX[key] = Texture(_europa_png(path) if prefix else path, sampler == "color", clamp)
                        break
        if key not in _TEX:
            raise HlslError("no texture %s on disk" % name)
    return _TEX[key]


def _europa_png(path):
    """textures/europa/*.png are LFS files: where only the pointer is checked out, the same pictures are made again
    (tools/europa/europa_textures.py, seeded) into a temporary folder."""
    with open(path, "rb") as f:
        if f.read(8) == b"\x89PNG\r\n\x1a\n":
            return path
    import tempfile
    cache = os.path.join(tempfile.gettempdir(), "lyud_sc_preview_europa")
    out = os.path.join(cache, os.path.basename(path))
    if not os.path.isfile(out):
        os.makedirs(cache, exist_ok=True)
        sys.path.insert(0, os.path.join(REPO, "tools", "europa"))
        import europa_textures as ET
        keep, ET.OUT = ET.OUT, cache
        ET.planet()
        ET.OUT = keep
    return out


def _unused():
    return _TEX[key]


def to_python(code):
    """One custom node's HLSL (the subset) as Python source that leaves its value in _ret."""
    src = re.sub(r"//[^\n]*", "", code)
    src = re.sub(r"static const float4? (\w+)\[\d+\] = \{(.*?)\};", lambda m: "%s = Arr([%s]);" % (m.group(1), " ".join(m.group(2).split())), src, flags=re.S)
    src = re.sub(r"\bint\(", "_int(", src)
    src = re.sub(r"\bfloat\(", "_float(", src)
    out, depth = [], 0
    for raw in re.split(r"(;|\{|\})", src.replace("\n", " ")):
        st = raw.strip()
        if not st or st == ";":
            continue
        if st == "{":
            depth += 1
            continue
        if st == "}":
            depth -= 1
            continue
        pad = "    " * depth
        m = re.match(r"\[unroll\] for \(int (\w+) = (\d+)$", st)
        if m:                                              # the header was split at its semicolons
            out.append(("for", m.group(1), m.group(2), pad))
            continue
        if out and isinstance(out[-1], tuple):
            kind, var, lo, pad0 = out[-1][:4]
            if len(out[-1]) == 4:
                hi = re.match(r"%s < (\d+)$" % var, st)
                if not hi:
                    raise HlslError("a loop this reader does not know: %s" % st)
                out[-1] = (kind, var, lo, pad0, hi.group(1))
                continue
            if st == "%s++)" % var:
                out[-1] = "%sfor %s in range(%s, %s):" % (pad0, var, lo, out[-1][4])
                continue
        m = re.match(r"(float[234]?|int) (\w+) = (.*)$", st, re.S)
        if m:
            size = {"float": 1, "int": 1, "float2": 2, "float3": 3, "float4": 4}[m.group(1)]
            out.append("%s%s = _chk(%s, %d, '%s')" % (pad, m.group(2), m.group(3), size, m.group(2)))
            continue
        m = re.match(r"return (.*)$", st, re.S)
        if m:
            out.append("%s_ret = %s" % (pad, m.group(1)))
            continue
        if re.match(r"\w+ (=|\+=|\*=|-=) ", st):
            out.append(pad + st)
            continue
        raise HlslError("a statement this reader does not know: %s" % st)
    return "\n".join(out)


def _chk(x, size, name):
    n = x.n if isinstance(x, V) else 1
    if isinstance(x, Arr):
        return x
    if n != size:
        raise HlslError("%s is declared float%d and given a float%d" % (name, size, n))
    return x


def run(code, inputs):
    """Execute one node. inputs: {pin: number, array, V or Texture}."""
    env = dict(FUNCS)
    env["_chk"] = _chk
    for k, v in inputs.items():
        env[k] = v
        if isinstance(v, Texture):
            env[k + "Sampler"] = None
    with np.errstate(all="ignore"):
        exec(compile(to_python(code), "<hlsl>", "exec"), env)
    return env["_ret"]


def run_material(spec, scene, params=None):
    """All of a material's nodes over a scene: {'uv': V2, 'uv1': V2, 'time': t, 'wp', 'objpos', 'objr', 'campos',
    'camvec', 'vnormal', 'local': 3x3 world -> mesh}. Returns {node key: value}."""
    params = params or {}
    built = {}

    def value(src):
        if isinstance(src, str):
            return scene[src]
        kind = src[0]
        if kind == "scalar":
            return np.float32(params.get(src[1], src[2]))
        if kind == "vector":
            return V(np.array(params.get(src[1], src[2]), np.float32))
        if kind == "tex":
            return params.get(src[1]) or texture(src[2], src[3])
        if kind == "sample":
            uv = built[src[4]] if len(src) > 4 else scene["uv"]
            return (params.get(src[1]) or texture(src[2], src[3])).SampleLevel(None, uv, 0.0)
        if kind == "node":
            return built[src[1]]
        if kind == "local":
            return V(built[src[1]].d_ @ np.asarray(scene["local"], np.float32).T)
        raise HlslError("source %r" % (src,))

    for key, code, typ, inputs in spec["nodes"]:
        r = run(code, {pin: value(s) for pin, s in inputs})
        n = r.n if isinstance(r, V) else 1
        if n != int(typ[-1]):
            raise HlslError("node %s returns a float%d, declared %s" % (key, n, typ))
        built[key] = r
    return built


def grid(w, h):
    u, v = np.meshgrid((np.arange(w) + 0.5) / w, (np.arange(h) + 0.5) / h)
    return V(np.stack([u, v], -1))


def flat_scene(w, h, t=3.7):
    uv = grid(w, h)
    one = np.ones((h, w), np.float32)
    return {"uv": uv, "uv1": uv, "time": np.float32(t), "vnormal": V(np.stack([0 * one, 0 * one, one], -1)),
            "camvec": V(np.stack([0.3 * one, 0.1 * one, 0.95 * one], -1)), "wp": V(np.stack([uv.d_[..., 0] * 200 - 100, uv.d_[..., 1] * 200 - 100, 0 * one + 100], -1)),
            "objpos": V(np.zeros(3, np.float32)), "objr": np.float32(100.0), "campos": V(np.array([0, 0, 400], np.float32)),
            "local": np.eye(3)}


def smoke(imp):
    """Every node of every material once; the problems as strings."""
    bad = []
    sc = flat_scene(24, 16)
    for name, spec in imp.MATERIALS.items():
        try:
            out = run_material(spec, sc)
            for k, v in out.items():
                if not np.all(np.isfinite(_f(v))):
                    bad.append("%s.%s: not finite" % (name, k))
        except Exception as e:  # noqa: BLE001 (a report, whatever went wrong)
            bad.append("%s: %s: %s" % (name, type(e).__name__, e))
    try:
        run(imp.CHUTE_SSS_HLSL, {"A": V(np.ones(4, np.float32)), "Tint": V(np.ones(4, np.float32)), "Through": 0.5})
    except Exception as e:  # noqa: BLE001
        bad.append("M_SC_Chute.sss: %s" % e)
    for b in bad:
        print("RUN ", b)
    return bad


# ---------------------------------------------------------------------------------------------------------------------
# pictures


def show(col, exposure=1.0):
    a = np.clip(1.0 - np.exp(-np.maximum(_f(col), 0.0) * exposure), 0, 1) ** (1 / 2.2)
    return (a * 255 + 0.5).astype(np.uint8)


def save(name, img):
    os.makedirs(OUT, exist_ok=True)
    Image.fromarray(img).save(os.path.join(OUT, name))
    print("wrote", os.path.join("design", "spacecraft", "preview", name), img.shape)


def frame_dir(lat, lon):
    """A unit vector of the planet textures' frame (right-handed: x to longitude 0, z north). The preview's world is
    this frame; the mesh's local frame in Unreal is its mirror in Y (the glTF import), which is what `local` says."""
    la, lo = math.radians(lat), math.radians(lon)
    return np.array([math.cos(la) * math.cos(lo), math.cos(la) * math.sin(lo), math.sin(la)])


def planet_scene(w, h, fov_deg, alt_km, over, look_off=(0.0, 0.0), radius_km=6371.0, roll=0.0):
    """A camera alt_km over the point `over` (lat, lon), looking at the planet's centre turned by look_off (degrees
    right, up). World = the mesh's frame, in km."""
    up = frame_dir(*over)
    cam = up * (radius_km + alt_km)
    north = np.array([0.0, 0.0, 1.0]) - up * up[2]
    north /= np.linalg.norm(north)
    east = np.cross(north, up)
    fwd = -up
    right, upv = east, north
    a, b = math.radians(look_off[0]), math.radians(look_off[1])
    fwd = fwd * math.cos(a) + right * math.sin(a)
    right = np.cross(upv, fwd) * -1.0
    right = east * math.cos(a) - (-up) * math.sin(a)
    f2 = fwd * math.cos(b) + upv * math.sin(b)
    upv = upv * math.cos(b) - fwd * math.sin(b)
    fwd = f2
    t = math.tan(math.radians(fov_deg) / 2)
    x = (np.arange(w) + 0.5) / w * 2 - 1
    y = 1 - (np.arange(h) + 0.5) / h * 2
    xx, yy = np.meshgrid(x * t, y * t * h / w)
    d = fwd[None, None, :] + right[None, None, :] * xx[..., None] + upv[None, None, :] * yy[..., None]
    d /= np.linalg.norm(d, axis=-1, keepdims=True)
    P = cam[None, None, :] + d * 1000.0
    return {"wp": V(P), "campos": V(cam.astype(np.float32)), "objpos": V(np.zeros(3, np.float32)),
            "objr": np.float32(radius_km / 0.9985), "camvec": V(-d), "time": np.float32(40.0), "local": np.diag([1.0, -1.0, 1.0]),
            "uv": grid(w, h)}, d


def planet(imp, name, w, h, fov, alt, over, sun, look=(0.0, 0.0), params=None, exposure=0.085, air=True):
    sc, _ = planet_scene(w, h, fov, alt, over, look)
    s = frame_dir(*sun)
    p = {"SunDir": (s[0], s[1], s[2], 0.0)}
    p.update(params or {})
    out = run_material(imp.MATERIALS["M_SC_PlanetNear"], sc, p)
    col = _f(out["col"]) * _f(out["mask"])[..., None]
    if air:
        # the shell, 1.045 of the planet's mesh, additive; drawn where the planet's mask left a hole or beside it
        sc2 = dict(sc)
        sc2["objr"] = np.float32(float(sc["objr"]) * 1.045)
        col = col + _f(run_material(imp.MATERIALS["M_SC_PlanetAir"], sc2, {"SunDir": p["SunDir"]})["col"]) * (1 - _f(out["mask"])[..., None])
    save(name, show(col, exposure))


def planets(imp):
    isl = (imp.EU_ISLAND[0], imp.EU_ISLAND[1])
    # far: as it leaves Europa, the planet 26 degrees across in a 50 degree porthole view; the island a speck
    planet(imp, "planet_1_far.png", 720, 720, 50.0, 6371.0 / math.sin(math.radians(13.0)) - 6371.0, (30.0, 5.0), (15.0, -40.0),
           params={"IslandSizeKm": 700.0})
    # half-filling
    planet(imp, "planet_2_half.png", 720, 720, 50.0, 6371.0 * 1.2, (38.0, 12.0), (20.0, -35.0), look=(14.0, 0.0), params={"IslandSizeKm": 400.0})
    # the low arc: 300 km up, the island coming over the horizon ahead, the limb and its air across the top
    planet(imp, "planet_3_arc.png", 720, 720, 60.0, 300.0, (isl[0] - 24.0, isl[1] - 14.0), (30.0, -30.0), look=(20.0, 48.0),
           params={"IslandSizeKm": 120.0})
    # close: 60 km up, the island below at its true size
    planet(imp, "planet_4_island.png", 720, 720, 50.0, 60.0, (isl[0] + 0.06, isl[1] - 0.02), (30.0, -30.0), look=(0.0, 0.0),
           params={"IslandSizeKm": 25.6})
    # the last look before the cloud: 9 km over the town
    planet(imp, "planet_5_town.png", 720, 720, 50.0, 9.0, (isl[0] - 0.054, isl[1] + 0.012), (30.0, -30.0), params={"IslandSizeKm": 25.6})
    # a night arrival: the town's lights
    planet(imp, "planet_6_night.png", 720, 720, 50.0, 40.0, (isl[0] - 0.03, isl[1] - 0.02), (-20.0, 170.0), params={"IslandSizeKm": 25.6},
           exposure=0.6)


def plasma(imp):
    w = h = 360
    tiles = []
    for heat, pl in ((0.25, 0.7), (0.6, 1.0), (1.0, 1.0)):
        row = []
        for t in (11.3, 11.42):
            uv = grid(w, h)
            # the porthole sees a sixth of the shell round, half of it along the flow
            sc = {"uv": V(np.stack([0.2 + uv.d_[..., 0] / 6.0, 0.25 + uv.d_[..., 1] * 0.5], -1)), "time": np.float32(t)}
            col = _f(run_material(imp.MATERIALS["M_SC_Plasma"], sc, {"Heat": heat, "Plasma": pl, "Gain": 1.0})["col"])
            x, y = np.meshgrid(np.linspace(-1, 1, w), np.linspace(-1, 1, h))
            img = show(col, 1.3)
            img[np.hypot(x, y) > 0.97] = (26, 28, 27)
            row.append(img)
        tiles.append(np.concatenate(row, 1))
    save("plasma.png", np.concatenate(tiles, 0))


def crt(imp):
    w, h = 474, 360
    tiles = []
    for mode, p in ((0, {"Craft01": 0.42, "Phase": 3}), (0, {"Craft01": 0.86, "Phase": 5}), (1, {"Att": (0.35, 0.08, 0, 0), "Phase": 2}),
                    (2, {"GLoad": (3.4, 4.2, 0, 0), "Phase": 5})):
        sc = flat_scene(w, h, 12.3)
        p = dict(p, Mode=mode, Gain=1.0)
        tiles.append(show(run_material(imp.MATERIALS["M_SC_CRT"], sc, p)["col"], 2.0))
    save("crt.png", np.concatenate(tiles, 1))


def panel(imp):
    """The unlit instruments: the digit cells as the kit lays them (the clock, the range, the speed; then a small
    number and a blanked window), the bar, the lamp board."""
    rows = []
    cw, ch, gapw = 58, 96, 5
    sets = ({"Value0": 123456.0, "Value1": 384400.0, "Value2": 7420.0},
            {"Value0": 1507.0, "Value1": 12.0, "Value2": 7.0, "Opt1": (0, 1, 0, 0), "Win2": (12, 5, 0, 2)})
    for p in sets:
        for wid, first, n, colon, dot in imp.DIGIT_WINDOWS:
            row = np.zeros((ch + 8, 480, 3), np.uint8) + 6
            x = 8
            for k in range(n):
                if colon and k and (n - k) % colon == 0:
                    x += 14                                      # the kit's wider gap where the colon stands
                uv = grid(cw, ch)
                sc = {"uv": uv, "uv1": V(np.stack([0 * uv.d_[..., 0] + (first + k + 0.5) / imp.DIGIT_COUNT, 0 * uv.d_[..., 1] + 0.5], -1)),
                      "time": np.float32(0.2)}
                row[4:4 + ch, x:x + cw] = show(run_material(imp.MATERIALS["M_SC_Digits"], sc, dict(p, Gain=1.0))["col"], 3.0)
                x += cw + gapw
            rows.append(row)
    left = np.concatenate(rows, 0)
    bars = []
    for lv in (0.0, 0.3, 0.7, 0.97):
        sc = flat_scene(40, left.shape[0], 0.1)
        bars.append(show(run_material(imp.MATERIALS["M_SC_Bar"], sc, {"Level": lv, "Gain": 1.0})["col"], 3.0))
    # the lamp board laid out 6 x 4: UV0 into the label atlas by labels.json's rects when it is there
    import json
    lw, lh = 100, 40
    board = np.zeros((4 * lh, 6 * lw, 3), np.uint8)
    rects = {}
    lj = os.path.join(HERE, "labels.json")
    if os.path.isfile(lj):
        with open(lj) as f:
            rects = json.load(f).get("layout", {}).get("rects", {})
    keys = [k for k in rects if k.startswith("lamp_")][:24]
    vals = [1, 5, 0, 9, 2, 13, 1, 0, 6, 1, 11, 0, 7, 1, 0, 3, 13, 1, 9, 0, 5, 1, 2, 10]
    for n in range(24):
        uv = grid(lw, lh)
        r = rects.get(keys[n]) if n < len(keys) else None
        uv0 = V(np.stack([(r[0] + uv.d_[..., 0] * r[2]) / 1024.0, (r[1] + uv.d_[..., 1] * r[3]) / 1024.0], -1)) if r else uv
        one = 0 * uv.d_[..., 0]
        sc = {"uv": uv0, "uv1": V(np.stack([one + (n % 6 + 0.5) / 6.0, one + (n // 6 + 0.5) / 4.0], -1)), "time": np.float32(0.2)}
        p = {"Lamps" + "ABCDEF"[k]: tuple(vals[k * 4:k * 4 + 4]) for k in range(6)}
        img = show(run_material(imp.MATERIALS["M_SC_Lamps"], sc, dict(p, Gain=1.0))["col"], 3.0)
        board[(n // 6) * lh:(n // 6 + 1) * lh, (n % 6) * lw:(n % 6 + 1) * lw] = img
        board[(n // 6) * lh, :, :] = 20
        board[:, (n % 6) * lw, :] = 20
    hgt = max(left.shape[0], board.shape[0])

    def padto(a):
        return np.concatenate([a, np.zeros((hgt - a.shape[0], a.shape[1], 3), np.uint8)], 0)
    save("panel.png", np.concatenate([padto(left)] + [padto(b) for b in bars] + [padto(board)], 1))


def glass(imp):
    w = h = 300
    tiles = []
    back = np.zeros((h, w, 3), np.float32)
    yy, xx = np.mgrid[0:h, 0:w]
    back[...] = np.array([0.02, 0.08, 0.25]) * (0.5 + yy[..., None] / h) + 0.5 * (np.hypot(xx - 200, yy - 90) < 40)[..., None]
    for p in ({}, {"Frost": 0.35}, {"Frost": 0.8}, {"Scorch": 0.4}, {"Scorch": 1.0}, {"Wet": 0.5}, {"Wet": 1.0}, {"Fog": 0.6, "FogColor": (0.8, 0.82, 0.85, 0)}):
        sc = flat_scene(w, h, 5.0)
        out = run_material(imp.MATERIALS["M_SC_Glass"], sc, dict(p, Light=(0.5, 0.42, 0.33, 0)))
        col = _f(out["col"]) + back * (1 - _f(out["op"])[..., None])
        img = show(col, 2.5)
        x, y = np.meshgrid(np.linspace(-1, 1, w), np.linspace(-1, 1, h))
        img[np.hypot(x, y) > 1.0] = (26, 28, 27)
        tiles.append(img)
    save("glass.png", np.concatenate([np.concatenate(tiles[:4], 1), np.concatenate(tiles[4:], 1)], 0))


def record(imp):
    """The record from above and a little to one side: the vinyl (no UVs: its place in the world) and its label."""
    w = h = 300
    tiles = []
    for cell, turn, cam in ((0, 0.0, (0.3, 0.1, 0.95)), (3, 1.0, (-0.5, 0.3, 0.8)), (6, 2.5, (0.1, -0.7, 0.7))):
        sc = flat_scene(w, h)
        one = np.ones((h, w), np.float32)
        sc["camvec"] = V(np.stack([cam[0] * one, cam[1] * one, cam[2] * one], -1))
        c, s_ = math.cos(turn), math.sin(turn)
        sc["local"] = np.array([[c, s_, 0], [-s_, c, 0], [0, 0, 1.0]])
        out = run_material(imp.MATERIALS["M_SC_Record"], sc, {"RecordR": 100.0, "LabelR": 36.0})
        # a plain look: base colour under a soft light, the sheen added
        col = _f(out["base"]) * 0.9 + _f(out["e"]) + 0.03 * (1 - _f(out["rough"]))[..., None]
        q = sc["uv"].d_ * 2 - 1
        r = np.hypot(q[..., 0], q[..., 1])
        # the label's own disc, turned with the record
        lq = np.stack([q[..., 0] * c + q[..., 1] * s_, -q[..., 0] * s_ + q[..., 1] * c], -1) / 0.36 * 0.5 + 0.5
        sl = dict(sc)
        sl["uv"] = V(np.clip(lq, 0, 1).astype(np.float32))
        lab = _f(run_material(imp.MATERIALS["M_SC_RecordLabel"], sl, {"Cell": cell})["base"]) * 0.9
        col = np.where((r < 0.36)[..., None], lab, col)
        img = show(col, 2.5)
        img[r > 1.0] = (26, 28, 27)
        tiles.append(img)
    save("record.png", np.concatenate(tiles, 1))


def main():
    sys.path.insert(0, os.path.join(REPO, "unreal", "scripts"))
    import import_spacecraft as imp
    what = sys.argv[1:] or ["planet", "plasma", "crt", "panel", "glass", "record"]
    todo = {"planet": planets, "plasma": plasma, "crt": crt, "panel": panel, "glass": glass, "record": record}
    for k in what:
        todo[k](imp)


if __name__ == "__main__":
    main()
