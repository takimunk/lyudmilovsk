"""Generates the voice-over of every spoken line, per language, from the text and the voice kept apart.

  unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json   the text of line.<speaker>.<scene>.<n>
  localization/voice.json                                       speakers (voice, direction), per-line directions, model
  audio/voice/<lang>/<line key>.wav  (+ .json)                  result, with the text hash, voice, direction and model it was made from

    python3 tools/gen_voice.py --list                  lines and which languages are voiced / stale (text changed since)
    python3 tools/gen_voice.py --lang ja               voice every line that has no audio in Japanese yet
    python3 tools/gen_voice.py --lang ru --stale       also re-voice lines whose text or direction changed
    python3 tools/gen_voice.py --lang en line.owner.first.1 --force

FAL_API_KEY in the environment. The direction goes in front of the text as an audio tag ("[calm, tired]"), which eleven-v3 acts out.
Then unreal/scripts/import_voice.py imports audio/voice/<lang>/*.wav as /Game/Voice/<lang>/V_<key with dots as underscores>.
"""
import argparse
import hashlib
import json
import os
import subprocess
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
VOICE = os.path.join(ROOT, "localization", "voice.json")
STRINGS = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Localization", "strings_%s.json")
OUT = os.path.join(ROOT, "audio", "voice", "%s")


def speaker_of(key):
    return key.split(".")[1]


def plan(man, lang, key, text):
    sp = man["speakers"].get(speaker_of(key), man["speakers"]["default"])
    voice = sp.get("voice_" + lang, sp["voice"])
    direction = man.get("lines", {}).get(key, {}).get("direction") or sp.get("direction", "")
    spoken = (direction + " " + text).strip()
    digest = hashlib.sha1(("%s|%s|%s" % (man["model"], voice, spoken)).encode("utf-8")).hexdigest()[:12]
    return voice, direction, spoken, digest


def generate(man, lang, key, text):
    voice, direction, spoken, digest = plan(man, lang, key, text)
    out = OUT % lang
    os.makedirs(out, exist_ok=True)
    body = {"text": spoken, "voice": voice, "language_code": lang}
    req = urllib.request.Request("https://fal.run/" + man["model"], json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    res = json.load(urllib.request.urlopen(req, timeout=600))
    mp3 = os.path.join(out, key + ".mp3")
    urllib.request.urlretrieve(res["audio"]["url"], mp3)
    wav = os.path.join(out, key + ".wav")
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@24000", "-c", "1", mp3, wav], check=True, capture_output=True)
    os.remove(mp3)
    with open(os.path.join(out, key + ".json"), "w", encoding="utf-8") as f:
        json.dump({"key": key, "lang": lang, "text": text, "speaker": speaker_of(key), "voice": voice, "direction": direction,
                   "model": man["model"], "hash": digest}, f, ensure_ascii=False, indent=1)
    return wav


def state(man, lang, key, text):
    meta = os.path.join(OUT % lang, key + ".json")
    if not os.path.exists(os.path.join(OUT % lang, key + ".wav")) or not os.path.exists(meta):
        return "missing"
    with open(meta, encoding="utf-8") as f:
        return "ok" if json.load(f).get("hash") == plan(man, lang, key, text)[3] else "stale"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("keys", nargs="*")
    ap.add_argument("--lang", default="ru")
    ap.add_argument("--stale", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    with open(VOICE, encoding="utf-8") as f:
        man = json.load(f)
    langs = sorted(p[len("strings_"):-5] for p in os.listdir(os.path.dirname(STRINGS)) if p.startswith("strings_"))
    tables = {}
    for l in langs:
        with open(STRINGS % l, encoding="utf-8") as f:
            tables[l] = json.load(f)
    keys = sorted({k for t in tables.values() for k in t if k.startswith("line.")})
    if a.list:
        for k in keys:
            print("%-28s %s" % (k, "  ".join("%s:%s" % (l, state(man, l, k, tables[l][k]) if k in tables[l] else "no-text") for l in langs)))
        return
    todo = [k for k in (a.keys or keys) if k in tables[a.lang]
            and (a.force or state(man, a.lang, k, tables[a.lang][k]) in (("missing", "stale") if a.stale else ("missing",)))]
    for k in todo:
        print(generate(man, a.lang, k, tables[a.lang][k]))


if __name__ == "__main__":
    main()
