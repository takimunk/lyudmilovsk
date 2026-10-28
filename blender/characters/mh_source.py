"""What a MetaHuman conform needs from a generated character (unreal/scripts/metahuman_character.py):

    Blender -b -P blender/characters/mh_source.py -- nao

Writes blender/characters/<id>/export/mh/:
  model.glb        the hi3d model at its height, feet at 0, facing -Y (UE +Y), decimated to 300k, with its texture
  head.glb         the same, only the head and neck below the hair (the conform's face target)
  portrait.png     an unlit front picture of the face for MetaHuman's landmark tracker
  portrait.json    that picture's camera in UE terms (location cm, yaw, FOV) so the landmarks back-project onto the mesh
"""
import json
import math
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rig_character as R  # noqa: E402

FOV = 30.0          # degrees, horizontal
DIST = 0.75         # metres from the eyes


def export(objs, path):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB", export_yup=True,
                              export_apply=True, export_image_format="JPEG", export_jpeg_quality=92)


def main():
    cid = sys.argv[sys.argv.index("--") + 1]
    spec = json.load(open(os.path.join(HERE, cid, "character.json")))
    marks = json.load(open(os.path.join(HERE, cid, "face_marks.json")))
    out = os.path.join(HERE, cid, "export", "mh")
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    m = R.load_model(os.path.join(HERE, cid, "gen", spec.get("model", "model") + ".glb"), spec["height"])
    R.clean(m)
    R.decimate(m, 300000)
    img = next(n.image for n in m.data.materials[0].node_tree.nodes if n.type == "TEX_IMAGE" and n.image)
    if img.size[0] > 4096:
        img.scale(4096, 4096)
    export([m], os.path.join(out, "model.glb"))

    # the head below the hair: faces above the collar whose texel is not hair-dark
    eye_z = marks["eyes"][0]["z"]
    head = m.copy()
    head.data = m.data.copy()
    bpy.context.scene.collection.objects.link(head)
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(head.data)
    uv = bm.loops.layers.uv.active
    px = np.array(img.pixels[:], np.float32).reshape(img.size[1], img.size[0], 4)
    lo_z = eye_z - spec["height"] * 0.12
    drop = []
    for f in bm.faces:
        c = f.calc_center_median()
        if c.z < lo_z:
            drop.append(f)
            continue
        u, v = sum((l[uv].uv for l in f.loops), start=type(f.loops[0][uv].uv)((0, 0))) / len(f.loops)
        p = px[min(int(v * img.size[1]), img.size[1] - 1), min(int(u * img.size[0]), img.size[0] - 1)]
        lum = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]
        if lum < 0.12 and (c.z > eye_z + 0.02 or abs(c.x) > 0.06):      # hair, not the eyes' or brows' paint
            drop.append(f)
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    bm.to_mesh(head.data)
    bm.free()
    R.clean(head, min_island=2000)
    export([head], os.path.join(out, "head.glb"))

    # portrait: unlit, perspective, from the front at the eyes' height
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = 1024
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("w")
    sc.world = w
    w.color = (1, 1, 1)
    nt = m.data.materials[0].node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    base = next(l.from_node for l in nt.links if l.to_socket == bsdf.inputs["Base Color"])
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(base.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL").inputs["Surface"])
    head.hide_render = True
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.angle = math.radians(FOV)
    sc.collection.objects.link(cam)
    sc.camera = cam
    ez = eye_z + 0.01
    cam.location = (0, -DIST, ez)
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.render.filepath = os.path.join(out, "portrait.png")
    bpy.ops.render.render(write_still=True)
    # Blender (x, y, z) m -> UE (x*100, -y*100, z*100); the camera looks along Blender +Y = UE -Y: yaw -90
    json.dump({"location_cm": [0.0, DIST * 100, ez * 100], "yaw": -90.0, "pitch": 0.0, "fov": FOV, "size": 1024},
              open(os.path.join(out, "portrait.json"), "w"), indent=1)
    print("MHSRC wrote %s" % out, flush=True)


if __name__ == "__main__":
    main()
