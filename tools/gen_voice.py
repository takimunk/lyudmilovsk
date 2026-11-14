"""Generates the voice-over of every spoken line, per language, from the text and the voice kept apart.

  unreal/Lyudmilovsk/Content/Localization/strings_<lang>.json   the text of line.<speaker>.<scene>.<n>
  localization/voice.json                                       speakers (voice, direction), per-line directions, model
  audio/voice/<lang>/<line key>.wav  (+ .json)                  result, with the text hash, voice, direction and model it was made from

    python3 tools/gen_voice.py --list                  lines and which languages are voiced / stale (text changed since)
    python3 tools/gen_voice.py --lang ja               voice every line that has no audio in Japanese yet
    python3 tools/gen_voice.py --lang ru --stale       also re-voice lines whose text or direction changed
    python3 tools/gen_voice.py --lang en line.owner.first.1 --force
    python3 tools/gen_voice.py --cost                  what voicing every missing line would cost (no request is sent)
    python3 tools/gen_voice.py --design                design the voice of every MiniMax speaker that has lines but no voice_id yet

FAL_API_KEY in the environment. The direction goes in front of the text as an audio tag ("[calm, tired]"), which eleven-v3 acts out.
The request body (text with direction, voice, language_code and the pinned settings of voice.json) is hashed with the endpoint
and stored in the sidecar, so a take is stale exactly when anything that shaped it changed.
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
VOICED_SUBS = ("quests",)


def speaker_of(key):
    return key.split(".")[1]


LANG_NAMES = {"ru": "Russian", "en": "English", "ja": "Japanese"}
# fal's prices (api.fal.ai/v1/models/pricing, 2026-11-01): both TTS endpoints per 1000 characters of the request text, a designed voice each
PRICE_PER_1K = 0.1
PRICE_DESIGN = 3.0


def speaker(man, key):
    return man["speakers"].get(speaker_of(key), man["speakers"]["default"])


def skipped(man, key):
    """A speaker with "skip" (a reason) is left unvoiced: its lines are about to change or go."""
    return bool(man["speakers"].get(speaker_of(key), {}).get("skip"))


def needs_design(sp):
    return sp.get("engine") == "minimax" and not sp.get("voice_id")


def request_text(body):
    return body.get("text", body.get("prompt", ""))


def median_f0(wav):
    """Median pitch of the voiced part of a wav, Hz (men ~100-130, women ~220, children 250-300; docs/localization.md)."""
    import wave
    import numpy as np
    with wave.open(wav) as w:
        rate = w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768.0
    n = int(rate * 0.04)
    f0 = []
    for i in range(0, len(x) - n, n // 2):
        f = x[i:i + n] - x[i:i + n].mean()
        if np.sqrt((f * f).mean()) < 0.02:
            continue
        ac = np.correlate(f, f, "full")[n - 1:]
        lo, hi = int(rate / 450), int(rate / 70)
        lag = lo + int(np.argmax(ac[lo:hi]))
        if ac[lag] > 0.4 * ac[0]:
            f0.append(rate / lag)
    return float(np.median(f0)) if f0 else 0.0


def plan(man, lang, key, text):
    """Everything the take depends on: the endpoint, the request body, and a hash of both.
    A speaker is voiced by ElevenLabs v3 (the default model, a premade voice, a direction tag in front of the text) or, with
    "engine": "minimax", by MiniMax speech with a voice designed from a prompt (children and the teenage hero)."""
    sp = speaker(man, key)
    line = man.get("lines", {}).get(key, {})
    direction = line.get("direction") or sp.get("direction", "")
    if sp.get("engine") == "minimax":
        model = sp["model"]
        voice = sp["voice_id"]
        setting = dict(sp.get("voice_setting", {}), **line.get("voice_setting", {}), voice_id=voice)
        body = {"prompt": text, "voice_setting": setting, "language_boost": LANG_NAMES.get(lang, "auto"),
                "audio_setting": {"sample_rate": 24000, "format": "mp3", "channel": 1}, "output_format": "url"}
    else:
        model = man["model"]
        voice = sp.get("voice_" + lang, sp["voice"])
        settings = dict(man.get("settings", {}), **sp.get("settings", {}), **line.get("settings", {}))
        body = dict(settings, text=(direction + " " + text).strip(), voice=voice, language_code=lang)
    digest = hashlib.sha1(json.dumps([model, body], ensure_ascii=False, sort_keys=True).encode("utf-8")).hexdigest()[:12]
    return model, voice, direction, body, digest


def generate(man, lang, key, text):
    if needs_design(speaker(man, key)):
        raise SystemExit("%s: speaker %s has no designed voice yet: run --design first" % (key, speaker_of(key)))
    model, voice, direction, body, digest = plan(man, lang, key, text)
    out = OUT % lang
    os.makedirs(out, exist_ok=True)
    req = urllib.request.Request("https://fal.run/" + model, json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    res = urllib.request.urlopen(req, timeout=600)
    request_id = res.headers.get("x-fal-request-id", "")
    res = json.load(res)
    mp3 = os.path.join(out, key + ".mp3")
    urllib.request.urlretrieve(res["audio"]["url"], mp3)
    wav = os.path.join(out, key + ".wav")
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@24000", "-c", "1", mp3, wav], check=True, capture_output=True)
    os.remove(mp3)
    sp = man["speakers"].get(speaker_of(key), man["speakers"]["default"])
    with open(os.path.join(out, key + ".json"), "w", encoding="utf-8") as f:
        json.dump({"key": key, "lang": lang, "text": text, "speaker": speaker_of(key), "voice": voice, "voice_id": sp.get("voice_id", ""), "voice_design": sp.get("design"),
                   "direction": direction, "model": model, "request": body, "fal_request_id": request_id,
                   "hash": digest}, f, ensure_ascii=False, indent=1)
    return wav


def design(man, name):
    """Designs a MiniMax voice once from the speaker's design prompt (a paid call, PRICE_DESIGN), records its voice_id in
    voice.json and keeps the preview with its request in audio/voice/design/<speaker>.wav + .json."""
    sp = man["speakers"][name]
    d = sp["design"]
    body = {"prompt": d["prompt"], "preview_text": d["preview_text"]}
    req = urllib.request.Request("https://fal.run/" + d["endpoint"], json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    res = urllib.request.urlopen(req, timeout=600)
    request_id = res.headers.get("x-fal-request-id", "")
    res = json.load(res)
    out = OUT % "design"
    os.makedirs(out, exist_ok=True)
    mp3, wav = os.path.join(out, name + ".mp3"), os.path.join(out, name + ".wav")
    urllib.request.urlretrieve(res["audio"]["url"], mp3)
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@24000", "-c", "1", mp3, wav], check=True, capture_output=True)
    os.remove(mp3)
    f0 = median_f0(wav)
    with open(os.path.join(out, name + ".json"), "w", encoding="utf-8") as f:
        json.dump({"speaker": name, "endpoint": d["endpoint"], "request": body, "voice_id": res["custom_voice_id"],
                   "fal_request_id": request_id, "median_f0_hz": round(f0)}, f, ensure_ascii=False, indent=1)
    sp["voice_id"] = res["custom_voice_id"]
    with open(VOICE, "w", encoding="utf-8") as f:
        f.write(json.dumps(man, ensure_ascii=False, indent=1))
    print("%s: %s, median F0 %d Hz, preview %s" % (name, sp["voice_id"], f0, wav))


def state(man, lang, key, text):
    meta = os.path.join(OUT % lang, key + ".json")
    if not os.path.exists(os.path.join(OUT % lang, key + ".wav")) or not os.path.exists(meta):
        return "missing"
    with open(meta, encoding="utf-8") as f:
        return "ok" if json.load(f).get("hash") == plan(man, lang, key, text)[4] else "stale"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("keys", nargs="*")
    ap.add_argument("--lang", default="ru")
    ap.add_argument("--stale", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--cost", action="store_true")
    ap.add_argument("--design", action="store_true")
    a = ap.parse_args()
    with open(VOICE, encoding="utf-8") as f:
        man = json.load(f)
    langs = sorted(p[len("strings_"):-5] for p in os.listdir(os.path.dirname(STRINGS)) if p.startswith("strings_"))
    tables = {}
    for l in langs:
        with open(STRINGS % l, encoding="utf-8") as f:
            tables[l] = json.load(f)
        # hand-written tables kept apart from the big one (the quests' words: Content/Localization/quests); the generated
        # ones (town, barks) are not voiced
        for sub in VOICED_SUBS:
            p = os.path.join(os.path.dirname(STRINGS), sub, "strings_%s.json" % l)
            if os.path.isfile(p):
                with open(p, encoding="utf-8") as f:
                    for k, v in json.load(f).items():
                        tables[l].setdefault(k, v)
    keys = sorted({k for t in tables.values() for k in t if k.startswith("line.")})
    voiced = [k for k in keys if not skipped(man, k)]
    if a.cost:
        n, chars, rows = 0, 0, {}
        for l in langs:
            for k in voiced:
                if k in tables[l] and state(man, l, k, tables[l][k]) in (("missing", "stale") if a.stale else ("missing",)):
                    c = len(request_text(plan(man, l, k, tables[l][k])[3]))
                    n, chars = n + 1, chars + c
                    r = rows.setdefault(speaker_of(k), [0, 0])
                    r[0], r[1] = r[0] + 1, r[1] + c
        for sp, (cn, cc) in sorted(rows.items()):
            print("%-20s %3d takes %6d chars" % (sp, cn, cc))
        todo_design = sorted({speaker_of(k) for k in voiced if needs_design(speaker(man, k))})
        print("%d takes, %d characters: %.2f $; %d voices to design (%s): %.2f $; total %.2f $" % (
            n, chars, chars / 1000 * PRICE_PER_1K, len(todo_design), ", ".join(todo_design), len(todo_design) * PRICE_DESIGN,
            chars / 1000 * PRICE_PER_1K + len(todo_design) * PRICE_DESIGN))
        return
    if a.design:
        for name in sorted({speaker_of(k) for k in voiced if needs_design(speaker(man, k))}):
            design(man, name)
        return
    if a.list:
        for k in keys:
            print("%-28s %s%s" % (k, "skip  " if skipped(man, k) else "", "  ".join("%s:%s" % (l, state(man, l, k, tables[l][k]) if k in tables[l] else "no-text") for l in langs)))
        return
    todo = [k for k in (a.keys or voiced) if k in tables[a.lang]
            and (a.force or state(man, a.lang, k, tables[a.lang][k]) in (("missing", "stale") if a.stale else ("missing",)))]
    for k in todo:
        print(generate(man, a.lang, k, tables[a.lang][k]))


if __name__ == "__main__":
    main()
