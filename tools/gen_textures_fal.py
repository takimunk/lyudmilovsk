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
ICON_OUT = os.path.join(ROOT, "textures", "inventory")
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
    "ceiling": COMMON + "Ceiling of an old Soviet building: concrete slab painted with several coats of matte "
                "off-white oil paint, slightly yellowed, faint roller marks, fine hairline cracks, a few small "
                "soot smudges and chipped spots showing grey concrete, calm and nearly uniform, 2 m x 2 m.",
    "radiator_paint": COMMON + "Cast iron surface painted with many layers of cream-beige oil paint, "
                      "drips and lumps of old paint, small rust spots, 30 cm x 30 cm.",
    "grass": COMMON + "Short wild meadow grass on a windy southern Russian seaside hill, dense green and "
             "sun-dried yellow blades, a few clover leaves and tiny wildflowers, small patches of bare earth, "
             "2 m x 2 m area.",
    "sea_water": COMMON + "Calm open sea surface seen from high above, deep blue-green water with gentle small "
                 "waves and soft sparkle, uniform, 20 m x 20 m area.",
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
    "poster_aton_1": ("A vertical advertising poster of the computer club ATON in 2000s Russian style: a golden "
                      "Eye of Horus pharaoh logo glowing over a dark blue pyramid and desert night sky, big Cyrillic "
                      "title \"КОМПЬЮТЕРНЫЙ КЛУБ АТОН\", smaller lines \"Игры • Интернет • Принтер\" and "
                      "\"Ночной тариф с 23:00\", worn paper with creases, flat front view filling the frame.",
                      "3:4"),
    "poster_aton_2": ("A vertical poster of a computer club price list: Eye of Horus logo in gold at the top, "
                      "title \"АТОН\" in Egyptian style lettering, a price table in roubles for 1 hour, 3 hours, "
                      "night, rules list in Cyrillic, hieroglyph border, yellowed paper, flat front view.", "3:4"),
    "label_baikal": ("A flat rectangular soda bottle label for a drink called БАЙКАЛ: dark green, an original "
                     "illustration of pine branches and herbs, Cyrillic name in a big retro serif, small text "
                     "\"газированный напиток 0,5 л\", flat front view filling the frame, no real brand logos.",
                     "3:2"),
    "label_dyushes": ("A flat rectangular soda bottle label for a drink called ДЮШЕС: pale yellow-green, an original "
                      "illustration of a pear, Cyrillic name in retro lettering, small text "
                      "\"газированный напиток 0,5 л\", flat front view filling the frame, no real brand logos.",
                      "3:2"),
    "label_buratino": ("A flat rectangular soda bottle label for a drink called БУРАТИНО: brown and orange, an "
                       "original cartoon wooden puppet with a long nose, Cyrillic name, small text "
                       "\"газированный напиток 0,5 л\", flat front view filling the frame, no real brand logos.",
                       "3:2"),
    "label_tarkhun": ("A flat rectangular soda bottle label for a drink called ТАРХУН: bright green, an original "
                      "illustration of tarragon leaves, Cyrillic name, small text \"газированный напиток 0,5 л\", "
                      "flat front view filling the frame, no real brand logos.", "3:2"),
    "screen_feed": ("A tall full-page screenshot of a 2000s Russian social network news feed in a browser: "
                    "repeating posts with small avatars, photos of friends, cats, a beach, likes and comments, "
                    "blue header, made-up site name, readable layout, no real logos.", "9:16"),
    "screen_chat": ("A tall screenshot of a 2000s instant messenger chat window with a long conversation in "
                    "Russian, coloured nicknames, timestamps, emoticons, grey window chrome, made-up app, no logos.",
                    "9:16"),
    "screen_racing": ("Top-down view of an arcade racing video game road, seamless vertically tileable strip: "
                      "asphalt with lane markings, roadside grass, trees and a few cars, bright 2000s console "
                      "graphics, no HUD text, no logos.", "9:16"),
    "screen_platformer": ("A wide horizontally seamless tileable level strip of a colourful original 2D platformer "
                          "game: grassy hills, floating brick platforms, coins, clouds, small enemies, pixel art, "
                          "no player character, no HUD, no logos.", "21:9"),
    "screen_zx": ("A full-screen ZX Spectrum style 8-bit game screenshot with bright attribute colours on black: "
                  "a maze of platforms, ladders and collectible items, score at the top, low resolution blocky "
                  "pixels, original game, no logos.", "4:3"),
    "screen_video": ("A full-screen video player in a 2000s web browser showing a funny home video still: a cat "
                     "jumping into a cardboard box in a kitchen, progress bar and play controls at the bottom, "
                     "made-up site, no logos.", "4:3"),
    "screen_cctv": ("A security monitor split into a 3x3 grid of grainy black-and-white CCTV camera views of a "
                    "small computer club: a corridor with computer desks, an entrance with a heavy door, a lobby "
                    "with a vending machine; timestamps and camera numbers in the corners.", "4:3"),
    "landscape_1": ("A classic Russian realist oil painting of rolling green hills of Krasnodar Krai at golden "
                    "hour, poplars along a dirt road, wide sky, visible brush strokes, museum quality, no frame.",
                    "3:2"),
    "landscape_2": ("A classic oil painting of the Black Sea coast near Gelendzhik: cliffs, calm sea, a small "
                    "white sail, warm evening light, visible brush strokes, no frame.", "4:3"),
    "landscape_3": ("A classic Russian oil painting of a birch grove in early autumn with a narrow path, soft "
                    "light, in the style of nineteenth-century realists, visible brush strokes, no frame.", "3:4"),
    "sky_panorama": ("Equirectangular 360 degree panorama of a sky only, seamless left and right edges: soft late "
                     "afternoon sky over the Black Sea, pale blue at the top fading to warm hazy peach at the "
                     "horizon line exactly in the middle, scattered fluffy cumulus clouds, the lower half a smooth "
                     "hazy sea-blue gradient, no land, no sun disc, no text.", "21:9"),
    "exit_sign": ("A glowing green emergency exit sign with the Cyrillic word ВЫХОД and a running-man pictogram, "
                  "flat front view filling the frame, dark background.", "16:9"),
}

ICON_STYLE = ("Inventory icon for a moody adventure game set in a Russian computer club in 2004: the object centred "
              "and filling most of a square frame, softly lit from the upper left, painterly realistic style, "
              "plain dark warm-grey background with a faint vignette, no text except what is on the object, "
              "no frame, no UI elements. ")
ICONS = {
    "icon_drink_baikal": ICON_STYLE + "A half-litre glass bottle of dark green fizzy herbal drink with a green pine-and-herbs label reading БАЙКАЛ.",
    "icon_drink_dyushes": ICON_STYLE + "A half-litre glass bottle of pale yellow pear lemonade with a light yellow-green label reading ДЮШЕС.",
    "icon_drink_buratino": ICON_STYLE + "A half-litre glass bottle of dark brown citrus fizzy drink with a brown and orange label reading БУРАТИНО.",
    "icon_drink_tarkhun": ICON_STYLE + "A half-litre glass bottle of bright green tarragon fizzy drink with a green label reading ТАРХУН.",
    "icon_empty_bottle": ICON_STYLE + "An empty half-litre glass soda bottle, greenish clear glass, a faded peeling label, a crown cap.",
    "icon_apple": ICON_STYLE + "A single ripe red-green apple with a short stem and one leaf, small blemishes, like a garden apple.",
}


def gen(name):
    if name in ICONS:
        prompt, aspect = ICONS[name], "1:1"
    elif name in SCREENS:
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
    out = ICON_OUT if name in ICONS else OUT
    os.makedirs(out, exist_ok=True)
    png = os.path.join(out, name + ".png")
    urllib.request.urlretrieve(url, png)
    # 2K JPEG keeps LFS small; Unreal and Blender read it directly.
    jpg = os.path.join(out, name + ".jpg")
    subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "90", png, "--out", jpg],
                   check=True, capture_output=True)
    os.remove(png)
    return jpg


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    names = sys.argv[1:] or list(TEXTURES) + list(SCREENS) + list(ICONS)
    with ThreadPoolExecutor(len(names)) as ex:
        for p in ex.map(gen, names):
            print(p)
