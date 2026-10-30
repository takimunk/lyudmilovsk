#!/usr/bin/env python3
"""Generates the outdoor ground's surface textures through fal.ai (Nano Banana 2.1): one photo-like,
top-down patch of ground per landscape layer (tools/terrain.py, TerrainGen.cpp). Raw images land in
textures/ground/raw/ (not committed); tools/make_ground_textures.py turns them into the seamless
albedo + normal pairs the build imports.

The key is read from the FAL_API_KEY environment variable at runtime and is never stored in the repo:
  FAL_API_KEY=... python3 tools/fal_ground_textures.py [layer ...]
"""
import json
import os
import sys
import time
import urllib.request
from pathlib import Path

ENDPOINT = "google/nano-banana-2.1"
OUT = Path(__file__).resolve().parent.parent / "textures" / "ground" / "raw"
STYLE = ("photographed straight on, a 2 x 2 metre patch filling the whole square edge to edge, "
         "soft overcast daylight, no cast shadows, no horizon, no objects, no people, no text, no watermark, "
         "evenly spread detail with no single feature in the middle, photorealistic ground texture for a game, "
         "seamless tileable")

PROMPTS = {
    "Grass": "short dense wild meadow grass on the Baltic coast in early autumn: thin green blades of mixed "
             "grasses, a little clover and plantain, a few dry blades, earth barely showing between them, ",
    "Meadow": "dry autumn meadow: yellowed and straw-coloured grass blades lying in all directions, some still "
              "green underneath, seed heads, ",
    "Dirt": "bare compacted brown sandy earth of a worn patch in a meadow, small pebbles, a few dead grass "
            "bits and roots, slightly cracked, ",
    "Mud": "wet dark brown trampled mud of a dirt track after rain, shallow puddle sheen, boot and tyre "
           "imprints going different ways, small stones pressed in, ",
    "Sand": "fine pale Baltic beach sand, slightly rippled by wind, tiny shell fragments and a few small dark "
            "pebbles, ",
    "Rock": "a vertical eroded Baltic coastal cliff face on the Sambian peninsula seen straight on from 2 m: horizontal "
            "layers of grey-brown glacial till clay with embedded pebbles and small boulders, alternating with thin "
            "bands of pale yellow sand and darker silt, fine vertical rain rills running down, a few dry roots, ",
    "Forest": "Scots pine forest floor: brown fallen pine needles, a few small pine cones and twigs, patches "
              "of green moss and lingonberry leaves, ",
    # the forest floor's litter (docs/forest_floor.md): tinted in M_Ground by what trees stand around
    "Leaves": "a thick carpet of freshly fallen birch and aspen leaves on a forest floor in autumn, overlapping "
              "small oval toothed leaves in soft yellow and pale ochre, a few curled and brown, no grass, ",
    "Moss": "a dense carpet of feather moss (Pleurozium) on a Baltic pine forest floor, fine yellow-green and "
            "green moss fronds, a few brown pine needles lying on it, ",
}


def key():
    k = os.environ.get("FAL_API_KEY") or os.environ.get("FAL_KEY")
    if not k:
        sys.exit("set FAL_API_KEY in the environment")
    return k.strip()


def post(url, k, body=None):
    req = urllib.request.Request(url, json.dumps(body).encode() if body is not None else None,
                                 {"Authorization": "Key " + k, "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as f:
        return json.load(f)


def generate(name, k):
    sub = post("https://queue.fal.run/" + ENDPOINT, k, {
        "prompt": PROMPTS[name] + STYLE, "aspect_ratio": "1:1", "resolution": "1K",
        "num_images": 1, "output_format": "png"})
    while True:
        st = post(sub["status_url"], k)
        if st.get("status") == "COMPLETED":
            break
        if st.get("status") in ("FAILED", "ERROR"):
            raise RuntimeError(str(st)[:300])
        time.sleep(2)
    res = post(sub["response_url"], k)
    OUT.mkdir(parents=True, exist_ok=True)
    dest = OUT / (name + ".png")
    with urllib.request.urlopen(res["images"][0]["url"], timeout=120) as f:
        dest.write_bytes(f.read())
    print("saved", dest)


if __name__ == "__main__":
    k = key()
    for n in (sys.argv[1:] or PROMPTS):
        generate(n, k)
