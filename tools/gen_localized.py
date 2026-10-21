"""Generates every texture that carries words, in any language, from the text and the prompt kept apart.

  localization/assets.json                                    what each asset is: kind, aspect, its prompt (with <<key>> placeholders), and the shared styles
  unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json the words, one file per language (the game reads the same files at run time)
  textures/localized/<lang>/<id>.jpg  (+ <id>.json)           the result, with the exact prompt and strings it was made from

    python3 tools/gen_localized.py --list                      assets and which languages have them
    python3 tools/gen_localized.py --prompt poster_aton_1 --lang en
    python3 tools/gen_localized.py --lang en                   generate everything missing in English (FAL_API_KEY in the environment)
    python3 tools/gen_localized.py --lang ru --force poster_aton_1 poster_aton_2

To add a language: copy strings_ru.json to strings_<lang>.json and translate the values (keep the keys and {0} placeholders), then run
with --lang <lang>, then unreal/scripts/import_hall_assets.py (it imports every language it finds), and set the console variable
lyud.Language <lang> (or the language in the game settings) to switch on the fly. A word changed in a strings file: run with --force
for the assets that use it (--uses <key> lists them).
"""
import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MANIFEST = os.path.join(ROOT, "localization", "assets.json")
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT = os.path.join(ROOT, "textures", "localized", "%s")


def load_strings(lang):
    with open(STRINGS % lang, encoding="utf-8") as f:
        return json.load(f)


def fill(text, strings, used):
    def sub(m):
        key = m.group(1)
        if key not in strings:
            raise KeyError("no string %r" % key)
        used[key] = strings[key]
        return strings[key]
    return re.sub(r"<<([^>]+)>>", sub, text)


def build_prompt(man, asset_id, lang):
    strings = load_strings(lang)
    asset = man["assets"][asset_id]
    used = {}
    prompt = fill(man["styles"][asset["style"]], strings, used) + fill(asset["prompt"], strings, used)
    return prompt.strip(), used


def generate(man, asset_id, lang):
    prompt, used = build_prompt(man, asset_id, lang)
    asset = man["assets"][asset_id]
    out = OUT % lang
    os.makedirs(out, exist_ok=True)
    body = {"prompt": prompt, "resolution": man.get("resolution", "2K"), "aspect_ratio": asset["aspect"],
            "output_format": "png", "num_images": 1}
    req = urllib.request.Request("https://fal.run/" + man["model"], json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    res = json.load(urllib.request.urlopen(req, timeout=600))
    png = os.path.join(out, asset_id + ".png")
    urllib.request.urlretrieve(res["images"][0]["url"], png)
    jpg = os.path.join(out, asset_id + ".jpg")
    # 2K JPEG at 90 keeps LFS small; icons are shown small, so they are downscaled
    subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "90", png, "--out", jpg], check=True, capture_output=True)
    os.remove(png)
    limit = {"icon": "512", "poster": "1200", "label": "1200"}.get(asset["kind"])
    if limit:
        subprocess.run(["sips", "-Z", limit, jpg], check=True, capture_output=True)
    with open(os.path.join(out, asset_id + ".json"), "w", encoding="utf-8") as f:
        json.dump({"id": asset_id, "lang": lang, "model": man["model"], "aspect": asset["aspect"], "prompt": prompt, "strings": used},
                  f, ensure_ascii=False, indent=1)
    return jpg


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--lang", default="ru")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--prompt", metavar="ID")
    ap.add_argument("--uses", metavar="KEY")
    a = ap.parse_args()
    with open(MANIFEST, encoding="utf-8") as f:
        man = json.load(f)
    langs = sorted(os.path.basename(p)[len("strings_"):-5] for p in os.listdir(os.path.dirname(STRINGS)) if p.startswith("strings_"))
    if a.list:
        for i in man["assets"]:
            have = [l for l in langs if os.path.exists(os.path.join(OUT % l, i + ".jpg"))]
            print("%-18s %-9s %s" % (i, man["assets"][i]["kind"], ",".join(have) or "-"))
        return
    if a.prompt:
        print(build_prompt(man, a.prompt, a.lang)[0])
        return
    if a.uses:
        for i in man["assets"]:
            if a.uses in build_prompt(man, i, a.lang)[1]:
                print(i)
        return
    ids = a.ids or list(man["assets"])
    todo = [i for i in ids if a.force or not os.path.exists(os.path.join(OUT % a.lang, i + ".jpg"))]
    with concurrent.futures.ThreadPoolExecutor(max(len(todo), 1)) as ex:
        for p in ex.map(lambda i: generate(man, i, a.lang), todo):
            print(p)


if __name__ == "__main__":
    main()
