#!/usr/bin/env python3
"""Generates the zoomer character textures through fal.ai (Nano Banana 2.1).

The key is read from the FAL_API_KEY environment variable at runtime and is
never stored in the repo:
  FAL_API_KEY=... python3 tools/fal_textures.py [name ...]
"""
import json
import os
import sys
import time
import urllib.request
from pathlib import Path

ENDPOINT = "google/nano-banana-2.1"
OUT = Path(__file__).resolve().parent.parent / "textures" / "character"
STYLE = ("flat even lighting, no shadows, no text, no watermark, "
         "low-poly PS2-era game texture look, slightly painterly, ")

PROMPTS = {
    "face": ("Front view of a young Gen Z person's face for a game character texture: neutral sleepy "
             "half-lidded eyes, thin eyebrows, small nose, slightly pale skin, faint dark circles under the eyes, "
             "relaxed closed mouth. Only the face from forehead to chin, skin color fills the whole square "
             "edge to edge, no hair, no ears, no neck, no glasses. Symmetric, centered, eyes at 45% height. " + STYLE),
    "hoodie": ("Seamless tileable fabric texture of a heavy washed-out grey-lilac cotton fleece hoodie, "
               "soft knit grain, subtle wear, top-down macro, " + STYLE),
    "denim": ("Seamless tileable fabric texture of faded light-blue wide-leg denim jeans, visible twill diagonal "
              "weave, light whiskering and wear, top-down macro, " + STYLE),
    "sneakers": ("Seamless tileable texture of worn off-white chunky sneaker leather with subtle grey and cream "
                 "paneling, light scuffs, top-down macro, " + STYLE),
    "hair": ("Seamless tileable texture of messy dark brown hair strands, fine strand lines running vertically, "
             "slight highlights, top-down macro, " + STYLE),
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
        "prompt": PROMPTS[name], "aspect_ratio": "1:1", "resolution": "1K",
        "num_images": 1, "output_format": "png"})
    while True:
        st = post(sub["status_url"], k)
        if st.get("status") == "COMPLETED":
            break
        if st.get("status") in ("FAILED", "ERROR"):
            raise RuntimeError(str(st)[:300])
        time.sleep(2)
    res = post(sub["response_url"], k)
    url = res["images"][0]["url"]
    OUT.mkdir(parents=True, exist_ok=True)
    dest = OUT / (name + ".png")
    with urllib.request.urlopen(url, timeout=120) as f:
        dest.write_bytes(f.read())
    print("saved", dest)


if __name__ == "__main__":
    k = key()
    for n in (sys.argv[1:] or PROMPTS):
        generate(n, k)
