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

# Screens for the stations: (prompt, aspect ratio). Plain images, not tileable.
SCREENS = {
    "screen_spreadsheet": ("Full-screen capture of a late-1990s office spreadsheet program on a 4:3 CRT: grey "
                           "Windows-98-style toolbars, a grid of cells with column letters and row numbers filled "
                           "with numbers and totals, a small 3D bar chart, low resolution, no brand names or logos.",
                           "4:3"),
    "screen_16bit": ("Full-screen capture of an original 16-bit console platformer, colourful pixel art, an original "
                     "hero, a small orange fox kid in a red cap, jumping between floating brick platforms over a "
                     "lake at sunset, collectible stars, score counter in the corner. Not based on any existing "
                     "game character, no logos or trademarks.", "4:3"),
    "screen_modern_game": ("Full-screen capture of a modern first-person sci-fi game at night: neon city street in "
                           "rain, holographic signs in Cyrillic, minimal HUD with ammo counter, cinematic lighting, "
                           "no real game logos.", "16:9"),
    "newspaper": ("Front page of a Russian regional newspaper from 2004, black and white print on slightly "
                  "yellowed paper, Cyrillic masthead, columns of small text, two grainy photos, flat scan, "
                  "no real newspaper names or logos.", "3:4"),
    "vending_front": ("Front panel of a worn early-2000s drinks vending machine, seen straight on, flat: a lit "
                      "glass window with rows of colourful soda cans and bottles with made-up Russian names "
                      "(Буратино, Тархун, Байкал, Дюшес, Колокольчик), price tags in roubles, coin slot and "
                      "buttons on the right, scratched red body. Absolutely no real-world brands: no Coca-Cola, "
                      "Fanta, Sprite, Pepsi or any existing logo.", "9:16"),
    "exit_sign": ("A glowing green emergency exit sign with the Cyrillic word ВЫХОД and a running-man pictogram, "
                  "flat front view filling the frame, dark background.", "16:9"),
}


def gen(name):
    if name in SCREENS:
        prompt, aspect = SCREENS[name]
    else:
        prompt, aspect = TEXTURES[name], "1:1"
    body = {"prompt": prompt, "resolution": "2K", "aspect_ratio": aspect,
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
    names = sys.argv[1:] or list(TEXTURES) + list(SCREENS)
    with ThreadPoolExecutor(len(names)) as ex:
        for p in ex.map(gen, names):
            print(p)
