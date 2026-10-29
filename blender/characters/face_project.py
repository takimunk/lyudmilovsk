"""The character's face painted from its own front reference picture onto the generated model's texture
(Dima, 2026-10-29: keep the hi3d model, fix it). The generated texture's eyes, brows and lips are torn and smeared;
the front picture (gen/<front>.png, the one the model was generated from) has them clean.

    Blender -b -P blender/characters/face_project.py -- nao [--preview]

How: the picture's eyes and mouth are found with face_marks.py (same as the model's, face_marks.json); a similarity
(scale, shift) maps the model's front (x, z) onto picture pixels; every face corner gets that as a second UV; a bake
mixes the picture over the old texture inside a feathered face mask (front-facing, between the bangs and the chin,
inside the cheeks), written to the old UVs. Out: gen/<model>_face.glb (the model with the new texture) and
export/<id>_face_texture.png; character.json "model" can then point at <model>_face.
"""
import json
import os
import subprocess
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import rig_character as R  # noqa: E402


def log(m):
    print("FACEPROJ " + m, flush=True)


def picture_marks(png, box):
    """face_marks.py on a square crop of the picture; returns its marks in full-picture pixels (x right, y down)."""
    x0, y0, x1, y1 = box
    crop = os.path.join(ROOT, "Saved", "FaceShots", "ref_crop.png")
    os.makedirs(os.path.dirname(crop), exist_ok=True)
    size = x1 - x0
    subprocess.run(["python3", "-c", "from PIL import Image; Image.open(%r).convert('RGB').crop(%r).resize((1024, 1024)).save(%r)"
                    % (png, tuple(box), crop)], check=True)        # Blender's python has no PIL
    # face_marks thinks in metres of an orthographic picture `scale` wide centred on z=centre: use pixels
    # face_marks thinks in metres (iris-sized blobs): the crop is about 0.2 m of face, as in its own shots
    out = subprocess.run(["python3", os.path.join(HERE, "face_marks.py"), crop, "0", "0.2",
                          os.path.join(ROOT, "renders", "characters", "ref_marks.png")], capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError(out.stderr[-600:])
    m = json.loads(out.stdout.strip().splitlines()[-1])
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    k = size / 0.2
    pt = lambda x, z: (cx + x * k, cy - z * k)                  # picture x right = model +x seen from the front? see main
    return {"eyes": [pt(e["x"], e["z"]) for e in m["eyes"]], "mouth": pt(m["mouth"]["x"], m["mouth"]["z"]),
            "w": [e["w"] for e in m["eyes"]], "raw": m}


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    cid = argv[0]
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    fp = spec.get("face_project", {})
    marks = json.load(open(os.path.join(HERE, cid, "face_marks.json")))
    model_name = fp.get("source") or spec.get("model", "model").removesuffix("_face")   # the generated model itself
    front = os.path.join(HERE, cid, "gen", fp.get("front", "front_short.png" if "short" in model_name else "front.png"))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    m = R.load_model(os.path.join(HERE, cid, "gen", model_name + ".glb"), spec["height"])
    R.clean(m)
    R.decimate(m, fp.get("faces", 600000))
    ref = picture_marks(front, fp.get("box", (911, 184, 1143, 416)))
    log("picture marks %s" % {k: v for k, v in ref.items() if k != "raw"})
    # model (x, z) -> picture (px, py). The picture is the character seen from the front: her left (+x) is on the
    # picture's right, as in the model's own front shot, so px grows with x and py falls with z.
    me = marks["eyes"]
    src = np.array([[me[0]["x"], me[0]["z"]], [me[1]["x"], me[1]["z"]], [marks["mouth"]["x"], marks["mouth"]["z"]]])
    dst = np.array([ref["eyes"][0], ref["eyes"][1], ref["mouth"]])
    # least squares: px = s*x + tx, py = -s*z + ty
    A = np.zeros((6, 3))
    b = np.zeros(6)
    for i, ((x, z), (px, py)) in enumerate(zip(src, dst)):
        A[2 * i] = (x, 1, 0)
        b[2 * i] = px
        A[2 * i + 1] = (-z, 0, 1)
        b[2 * i + 1] = py
    s, tx, ty = np.linalg.lstsq(A, b, rcond=None)[0]
    res = A @ np.array([s, tx, ty]) - b
    log("model -> picture: %.1f px/m, residual %s px" % (s, np.round(res, 1).tolist()))
    W, H = bpy.data.images.load(front, check_existing=True).size

    me_ = m.data
    co = R.world_co(m)
    nrm = np.empty(len(me_.vertices) * 3)
    me_.vertices.foreach_get("normal", nrm)
    nrm = nrm.reshape(-1, 3)
    proj = me_.uv_layers.new(name="proj")
    li = np.empty(len(me_.loops), int)
    me_.loops.foreach_get("vertex_index", li)
    # then a Gaussian RBF so the eyes and mouth land exactly (the similarity leaves a few pixels)
    k = lambda p, q: np.exp(-((p[:, None, :] - q[None, :, :]) ** 2).sum(-1) / (2 * 0.025 ** 2))
    wr = np.linalg.solve(k(src, src) + 1e-6 * np.eye(3), -res.reshape(3, 2))
    off = k(co[:, [0, 2]], src) @ wr
    px_ = s * co[:, 0] + tx + off[:, 0]
    py_ = -s * co[:, 2] + ty + off[:, 1]
    u = px_[li] / W
    v = 1.0 - py_[li] / H
    proj.data.foreach_set("uv", np.stack([u, v], 1).ravel())
    # the mask: the face's front between the bangs and the chin, inside the cheeks, facing the picture
    ex = (me[0]["x"] + me[1]["x"]) / 2
    ez = (me[0]["z"] + me[1]["z"]) / 2
    top, bot = ez + fp.get("above_eyes", 0.022), marks["mouth"]["z"] - fp.get("below_mouth", 0.045)
    cz, rz = (top + bot) / 2, (top - bot) / 2
    rx = fp.get("half_width", 0.066)
    r = np.sqrt(((co[:, 0] - ex) / rx) ** 2 + ((co[:, 2] - cz) / rz) ** 2)
    feather = fp.get("feather", 0.25)
    mask = np.clip((1.0 - r) / feather, 0, 1) * np.clip((-nrm[:, 1] - 0.35) / 0.3, 0, 1)
    col = np.zeros((len(co), 4), np.float32)
    col[:, 0] = mask
    col[:, 3] = 1
    # the generated face is lumpy (bags under the eyes, dents on the cheeks: hard shadows in the game): smooth the
    # face's surface, fading out with the mask, keeping the eye openings' and the lips' own shapes
    it = fp.get("smooth", 40)
    if it:
        e = np.empty(len(me_.edges) * 2, int)
        me_.edges.foreach_get("vertices", e)
        e = e.reshape(-1, 2)
        wgt = np.clip((1.0 - r) / feather, 0, 1) * (nrm[:, 1] < 0)
        for eye in me:
            d = np.sqrt(((co[:, 0] - eye["x"]) / 0.02) ** 2 + ((co[:, 2] - eye["z"]) / 0.011) ** 2)
            wgt *= np.clip(d - 0.8, 0, 1)
        d = np.sqrt(((co[:, 0] - marks["mouth"]["x"]) / 0.03) ** 2 + ((co[:, 2] - marks["mouth"]["z"]) / 0.008) ** 2)
        wgt *= np.clip(d - 0.8, 0, 1)
        cnt = np.bincount(e.ravel(), minlength=len(co)).astype(float)
        p = co.copy()
        for _ in range(it):
            acc = np.zeros_like(p)
            np.add.at(acc, e[:, 0], p[e[:, 1]])
            np.add.at(acc, e[:, 1], p[e[:, 0]])
            avg = acc / np.maximum(cnt, 1)[:, None]
            p += (avg - p) * (0.5 * wgt)[:, None]
        R.set_world_co(m, p)
        log("face smoothed: %d passes, mean move %.4f m" % (it, np.linalg.norm(p - co, axis=1)[wgt > 0].mean()))
    attr = me_.color_attributes.new("facemask", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", col.ravel())

    # bake: emission = mix(old texture via UV0, picture via proj, mask) into a new image on UV0
    mat = me_.materials[0]
    nt = mat.node_tree
    old = next(n for n in nt.nodes if n.type == "TEX_IMAGE" and n.image and "normal" not in n.image.name.lower()
               and n.image.colorspace_settings.name != "Non-Color")
    size = old.image.size[0]
    pic = bpy.data.images.load(front)
    out_img = bpy.data.images.new(cid + "_face_texture", size, size, alpha=False)
    N = nt.nodes
    uv0, uvp = N.new("ShaderNodeUVMap"), N.new("ShaderNodeUVMap")
    uv0.uv_map, uvp.uv_map = me_.uv_layers[0].name, "proj"
    t_old = N.new("ShaderNodeTexImage")
    t_old.image = old.image
    t_pic = N.new("ShaderNodeTexImage")
    t_pic.image = pic
    t_pic.extension = "CLIP"
    nt.links.new(uv0.outputs["UV"], t_old.inputs["Vector"])
    nt.links.new(uvp.outputs["UV"], t_pic.inputs["Vector"])
    at = N.new("ShaderNodeAttribute")
    at.attribute_name = "facemask"
    mix = N.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(at.outputs["Color"], mix.inputs["Factor"])
    nt.links.new(t_old.outputs["Color"], mix.inputs[6])
    nt.links.new(t_pic.outputs["Color"], mix.inputs[7])
    em = N.new("ShaderNodeEmission")
    nt.links.new(mix.outputs[2], em.inputs["Color"])
    outn = next(n for n in N if n.type == "OUTPUT_MATERIAL")
    keep = next((l.from_socket for l in nt.links if l.to_socket == outn.inputs["Surface"]), None)
    nt.links.new(em.outputs["Emission"], outn.inputs["Surface"])
    target = N.new("ShaderNodeTexImage")
    target.image = out_img
    N.active = target
    me_.uv_layers.active = me_.uv_layers[0]
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.samples = 1
    bpy.ops.object.select_all(action="DESELECT")
    m.select_set(True)
    bpy.context.view_layer.objects.active = m
    bpy.ops.object.bake(type="EMIT", margin=8)
    out_png = os.path.join(HERE, cid, "export", cid + "_face_texture.png")
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    out_img.filepath_raw = out_png
    out_img.file_format = "PNG"
    out_img.save()
    log("baked %s (%dpx)" % (os.path.relpath(out_png, ROOT), size))
    # the model with the new texture
    for n in (uv0, uvp, t_old, t_pic, at, mix, em, target):
        N.remove(n)
    if keep is not None:
        nt.links.new(keep, outn.inputs["Surface"])
    me_.uv_layers.remove(me_.uv_layers["proj"])
    me_.color_attributes.remove(me_.color_attributes["facemask"])
    plain = bpy.data.materials.new(mat.name + "_face")             # base colour straight from the new texture
    plain.use_nodes = True
    pb = next(n for n in plain.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    ti = plain.node_tree.nodes.new("ShaderNodeTexImage")
    ti.image = out_img
    plain.node_tree.links.new(ti.outputs["Color"], pb.inputs["Base Color"])
    pb.inputs["Roughness"].default_value = 0.75
    me_.materials[0] = plain
    out_glb = os.path.join(HERE, cid, "gen", model_name + "_face.glb")
    bpy.ops.export_scene.gltf(filepath=out_glb, export_format="GLB", export_yup=True, export_image_format="AUTO")
    log("wrote %s" % os.path.relpath(out_glb, ROOT))
    if "--preview" in argv:
        sys.path.insert(0, os.path.join(ROOT, "blender", "humans"))
        import clean_human as C
        sc2, cam = C.scene_setup()
        for yaw in (0, 35):
            C.shoot(os.path.join(C.BLENDER_RENDERS, "%s_faceproj_%d.png" % (cid, yaw)), cam, (0, 0, ez - 0.02), 0.75, yaw,
                    res=(700, 700))


if __name__ == "__main__":
    main()
