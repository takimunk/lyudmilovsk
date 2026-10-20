#!/usr/bin/env python3
"""Seamless material textures for the detailed props (desks, chairs, computers) via fal.ai (Nano Banana 2.1).

The key is read from FAL_API_KEY at runtime and never stored in the repo:
  FAL_API_KEY=... python3 tools/fal_prop_textures.py [name ...]
"""
import json, os, sys, time, urllib.error, urllib.request
from pathlib import Path

ENDPOINT = "google/nano-banana-2.1"
OUT = Path(__file__).resolve().parent.parent / "textures" / "props"
STYLE = ("Seamless tileable texture, top-down orthographic macro photo, perfectly flat even lighting, "
         "no shadows, no perspective, no text, no watermark, fills the whole square edge to edge. ")

PROMPTS = {
    "wood_dark": "Dark polished mahogany veneer with fine straight grain, furniture veneer, slightly worn varnish. ",
    "wood_pine": "Plain light pine boards with knots and straight grain, matte. ",
    "wood_birch": "Pale birch plywood, tight fine grain, slight yellowing. ",
    "laminate_white": "Off-white melamine laminate, very subtle fine speckle, slight yellow aging. ",
    "laminate_grey": "Mid-grey matte desk surface with fine pebbled grain. ",
    "fabric_black": "Black woven office chair upholstery fabric, tight small herringbone weave, slightly faded. ",
    "mesh_black": "Black elastic breathable mesh fabric of an ergonomic chair, fine diamond lattice weave. ",
    "leatherette_black": "Black synthetic leatherette with fine pebbled grain, slightly worn, subtle sheen. ",
    "leatherette_red": "Deep red synthetic leatherette with fine pebbled grain, slightly worn, subtle sheen. ",
    "plastic_beige": "Yellowed beige ABS computer plastic from the nineties, very fine matte grain with faint mottled aging. ",
    "plastic_grey": "Light grey matte plastic surface with very fine grain. ",
    "metal_brushed": "Brushed aluminium with fine horizontal scratches, neutral grey. ",
    "metal_painted_grey": "Grey hammered-finish painted steel, small chips of paint worn off at random. ",
}


def key():
    k = os.environ.get("FAL_API_KEY") or os.environ.get("FAL_KEY")
    if not k:
        sys.exit("set FAL_API_KEY in the environment")
    return k.strip()


def post(url, k, body=None):
    req = urllib.request.Request(url, json.dumps(body).encode() if body is not None else None,
                                 {"Authorization": "Key " + k, "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as f:
            return json.load(f)
    except urllib.error.HTTPError as e:
        raise RuntimeError("%s: %s" % (e.code, e.read()[:400]))


def generate(name, k):
    dest = OUT / (name + ".png")
    if dest.exists():
        return
    sub = post("https://queue.fal.run/" + ENDPOINT, k, {
        "prompt": STYLE + PROMPTS[name], "aspect_ratio": "1:1", "resolution": "1K",
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
    with urllib.request.urlopen(res["images"][0]["url"], timeout=120) as f:
        dest.write_bytes(f.read())
    print("saved", dest)


if __name__ == "__main__":
    k = key()
    for n in (sys.argv[1:] or PROMPTS):
        for attempt in range(4):      # the content checker is flaky on harmless texture prompts
            try:
                generate(n, k)
                break
            except RuntimeError as e:
                print("retry", n, str(e)[:60])
