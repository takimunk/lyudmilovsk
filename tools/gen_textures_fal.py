"""Generates the seamless podyezd textures with fal (Nano Banana 2.1).

Needs FAL_API_KEY in the environment. Writes textures/podyezd/<name>.jpg (macOS: converts with sips).
    python3 tools/gen_textures_fal.py [name ...]
"""
import json
import os
import subprocess
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "textures", "podyezd")
MODEL = "google/nano-banana-2.1"

COMMON = ("Seamless tileable PBR albedo texture, perfectly flat orthographic top-down scan, "
          "even diffuse lighting, no shadows, no perspective, no vignette, no text, edges wrap seamlessly. ")
TEXTURES = {
    "wall_paint": COMMON + "Old Soviet apartment stairwell (podyezd) wall painted with glossy dark "
                  "green-teal oil paint, several layers, small chips and scratches showing lighter paint "
                  "underneath, faint drips, slight grime, 1 m x 1 m area.",
    "whitewash": COMMON + "Soviet whitewashed plaster wall (pobelka), warm off-white slightly yellowed lime "
                 "wash, subtle brush strokes, faint water stains and soot smudges, hairline cracks, 1 m x 1 m.",
    "floor_tiles": COMMON + "Soviet metlakh ceramic floor tiles in a stairwell, small 10 cm square tiles, "
                   "checker of terracotta brown and dusty beige, worn matte surface, grey grout, a few "
                   "chipped tiles, 1 m x 1 m area.",
    "ceiling": COMMON + "Whitewashed concrete ceiling slab of an old Soviet building, chalky off-white, "
                "patchy flaking whitewash, faint grey soot and rust-brown leak stains, 2 m x 2 m.",
    "radiator_paint": COMMON + "Cast iron surface painted with many layers of cream-beige oil paint, "
                      "drips and lumps of old paint, small rust spots, 30 cm x 30 cm.",
}


def gen(name):
    body = {"prompt": TEXTURES[name], "resolution": "2K", "aspect_ratio": "1:1",
            "output_format": "png", "num_images": 1}
    req = urllib.request.Request("https://fal.run/" + MODEL, json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"],
                                  "Content-Type": "application/json"})
    res = json.load(urllib.request.urlopen(req, timeout=600))
    url = res["images"][0]["url"]
    png = os.path.join(OUT, name + ".png")
    urllib.request.urlretrieve(url, png)
    # 2K JPEG keeps LFS small; Unreal and Blender read it directly.
    jpg = os.path.join(OUT, name + ".jpg")
    subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "90", png, "--out", jpg],
                   check=True, capture_output=True)
    os.remove(png)
    return jpg


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    names = sys.argv[1:] or list(TEXTURES)
    with ThreadPoolExecutor(len(names)) as ex:
        for p in ex.map(gen, names):
            print(p)
