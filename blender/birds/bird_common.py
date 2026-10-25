"""Helpers shared by the bird builds (from Murmuration's tools/blender/common.py, Dima's own code).

Bird geometry is authored like Murmuration's: three.js-style coordinates (Y up, +Z = the bird's forward, +X its left),
metres. to_blender() turns that into Blender Z-up with the bird facing +X, so glTF -> Unreal gives forward +X, up +Z.
"""
import math
import os

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "blender", "birds", "export")


def to_blender(p):
    x, y, z = p
    return (z, x, y)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def principled(mat):
    return next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


def make_material(name, color, roughness=0.5, metallic=0.0, vcol=False, emission=None,
                  emission_strength=0.0, image=None):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = principled(m)
    p.inputs["Base Color"].default_value = (*color, 1.0)
    p.inputs["Roughness"].default_value = roughness
    p.inputs["Metallic"].default_value = metallic
    if emission is not None:
        p.inputs["Emission Color"].default_value = (*emission, 1.0)
        p.inputs["Emission Strength"].default_value = emission_strength
    if image is not None:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = image
        nt.links.new(tex.outputs["Color"], p.inputs["Base Color"])
    elif vcol:
        # glTF: COLOR_0 multiplies baseColorFactor; the exporter maps Color Attribute x RGB.
        ca = nt.nodes.new("ShaderNodeVertexColor")
        ca.layer_name = "Col"
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs[6].default_value = (1, 1, 1, 1)
        mix.inputs[7].default_value = (*color, 1.0)
        nt.links.new(ca.outputs["Color"], mix.inputs[6])
        nt.links.new(mix.outputs[2], p.inputs["Base Color"])
    m.diffuse_color = (*color, 1.0)
    return m


def srgb(hexstr):
    """'#rrggbb' -> linear RGB tuple."""
    h = hexstr.lstrip("#")
    out = []
    for i in range(3):
        c = int(h[2 * i:2 * i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return tuple(out)


def set_engine(scene, prefer=("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "BLENDER_WORKBENCH")):
    for e in prefer:
        try:
            scene.render.engine = e
            return e
        except TypeError:
            continue
    return scene.render.engine


def patch_glb(path, fn):
    """Edit the JSON chunk of a .glb in place: fn(gltf_dict) mutates it."""
    import json
    import struct
    with open(path, "rb") as fh:
        data = fh.read()
    jlen = struct.unpack("<I", data[12:16])[0]
    gl = json.loads(data[20:20 + jlen])
    rest = data[20 + jlen:]
    fn(gl)
    js = json.dumps(gl, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + len(rest))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js + rest
    with open(path, "wb") as fh:
        fh.write(out)


def tile_noise(n, seed, scale_px, aniso=(1.0, 1.0)):
    """Tileable gaussian-filtered noise, zero mean / unit std. aniso stretches (x, y) features."""
    import numpy as np
    r = np.random.default_rng(seed)
    w = r.standard_normal((n, n))
    ky = np.fft.fftfreq(n)[:, None] * aniso[1]
    kx = np.fft.fftfreq(n)[None, :] * aniso[0]
    filt = np.exp(-((kx ** 2 + ky ** 2) * (scale_px * math.pi) ** 2))
    out = np.real(np.fft.ifft2(np.fft.fft2(w) * filt))
    return (out - out.mean()) / (out.std() + 1e-9)


def lin_to_srgb(a):
    import numpy as np
    a = np.clip(a, 0, 1)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055)


def normal_from_height(h, strength=1.0):
    """Tangent-space (OpenGL, +Y up) normal map RGB in 0..1 from a tileable height (pixel units)."""
    import numpy as np
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5 * strength
    n = np.stack([-dx, -dy, np.ones_like(h)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5
