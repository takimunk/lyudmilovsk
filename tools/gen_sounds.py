"""Generates the game's recorded-style sounds and music from audio/sounds.json, and writes their provenance.

    PY=/Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13   (needs numpy)
    $PY tools/gen_sounds.py --list                 what exists, what is missing or stale, what it would cost
    $PY tools/gen_sounds.py --sfx [ids...]         generate missing sound effects (all, or these ids)
    $PY tools/gen_sounds.py --music [ids...]       generate missing music cues
    $PY tools/gen_sounds.py --force ...            regenerate even if present
    $PY tools/gen_sounds.py --sources              rewrite audio/SOURCES.md only

FAL_API_KEY in the environment. Each output gets a .json sidecar with the endpoint, the exact request body, a hash of
both, the fal request id, the seed (music), the sha256 of the final wav and the license; a take is stale when the
hash of what would be sent now differs. Music (Stable Audio 2.5) is seeded, so the same request gives the same audio;
ElevenLabs sound effects take no seed, so for them the sidecar and the committed wav are the record.

Post-processing (deterministic, here): mp3/wav -> 48 kHz 16-bit WAV (afconvert); one-shots trimmed of leading and
trailing silence with 5 ms / 40 ms fades; loops cross-faded end into start (0.5 s) so they repeat without a seam;
levels set to a peak (one-shots) or RMS (loops, music) target per group.
Then unreal/scripts/import_audio.py imports them under /Game/Audio.
"""
import argparse
import hashlib
import json
import os
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.request
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MANIFEST = os.path.join(ROOT, "audio", "sounds.json")
SFX_DIR = os.path.join(ROOT, "audio", "sfx")
MUSIC_DIR = os.path.join(ROOT, "audio", "music")
RATE = 48000
# Blender's python carries no CA bundle: use the system's
SSL = ssl.create_default_context(cafile="/etc/ssl/cert.pem") if os.path.exists("/etc/ssl/cert.pem") else ssl.create_default_context()
# loudness targets: one-shots by peak (dBFS), loops and music by RMS (dBFS)
PEAK_DB = {"animals": -3.0, "far": -6.0, "club": -6.0, "diary": -9.0}
LOOP_RMS_DB = -24.0
MUSIC_RMS_DB = -21.0


def load():
    with open(MANIFEST, encoding="utf-8") as f:
        return json.load(f)


def sfx_body(man, s):
    body = dict(man["models"]["sfx"]["settings"], text=s["prompt"])
    if s.get("duration"):
        body["duration_seconds"] = s["duration"]
    if s.get("loop"):
        body["loop"] = True
    return body


def music_body(man, m):
    return dict(man["models"]["music"]["settings"], prompt=m["prompt"], seconds_total=m["seconds"], seed=m["seed"])


def digest(endpoint, body):
    return hashlib.sha1(json.dumps([endpoint, body], ensure_ascii=False, sort_keys=True).encode("utf-8")).hexdigest()[:12]


def call(endpoint, body):
    req = urllib.request.Request("https://fal.run/" + endpoint, json.dumps(body).encode(),
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    for attempt in range(3):
        try:
            res = urllib.request.urlopen(req, timeout=900, context=SSL)
            rid = res.headers.get("x-fal-request-id", "")
            return json.load(res), rid
        except Exception as e:  # network hiccups: retry a couple of times
            if attempt == 2:
                raise
            print("  retry after", e)
            time.sleep(4 * (attempt + 1))


def read_wav(path):
    with wave.open(path) as w:
        n, ch = w.getnframes(), w.getnchannels()
        a = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768.0
    return a.reshape(-1, ch)


def write_wav(path, a):
    a = np.clip(a, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(a.shape[1])
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((a * 32767).astype("<i2").tobytes())


def to_wav48(src, dst, channels):
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@%d" % RATE, "-c", str(channels), src, dst], check=True, capture_output=True)


def db(x):
    return 10 ** (x / 20)


def finish_oneshot(a, peak_db):
    env = np.abs(a).max(axis=1)
    thr = env.max() * db(-45)
    idx = np.where(env > thr)[0]
    if len(idx):
        a = a[max(idx[0] - int(0.01 * RATE), 0): min(idx[-1] + int(0.15 * RATE), len(a))]
    fi, fo = int(0.005 * RATE), int(0.04 * RATE)
    a[:fi] *= np.linspace(0, 1, fi)[:, None]
    a[-fo:] *= np.linspace(1, 0, fo)[:, None]
    return a * (db(peak_db) / max(np.abs(a).max(), 1e-6))


def finish_loop(a, rms_db):
    # cross-fade the last half second into the start: the loop then runs end -> start without a seam
    x = int(0.5 * RATE)
    if len(a) > 4 * x:
        t = np.linspace(0, 1, x)[:, None]
        head = a[:x] * t + a[-x:] * (1 - t)
        a = np.concatenate([head, a[x:-x]])
    rms = np.sqrt(np.mean(a ** 2))
    return a * (db(rms_db) / max(rms, 1e-6))


def finish_music(a, rms_db):
    fo = int(3.0 * RATE)
    a[-fo:] *= np.linspace(1, 0, fo)[:, None] ** 2
    a[:int(0.02 * RATE)] *= np.linspace(0, 1, int(0.02 * RATE))[:, None]
    rms = np.sqrt(np.mean(a ** 2))
    a = a * (db(rms_db) / max(rms, 1e-6))
    peak = np.abs(a).max()
    return a * (db(-1) / peak) if peak > db(-1) else a


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def outputs(man):
    """(kind, id, n, wav path, endpoint, body, entry) of every take the manifest asks for."""
    out = []
    for s in man["sfx"]:
        body = sfx_body(man, s)
        for n in range(1, s["variants"] + 1):
            out.append(("sfx", s["id"], n, os.path.join(SFX_DIR, s["group"], "%s_%02d.wav" % (s["id"], n)), man["models"]["sfx"]["endpoint"], body, s))
    for m in man["music"]:
        out.append(("music", m["id"], 1, os.path.join(MUSIC_DIR, m["id"] + ".wav"), man["models"]["music"]["endpoint"], music_body(man, m), m))
    return out


def state(path, endpoint, body):
    side = path[:-4] + ".json"
    if not os.path.exists(path) or not os.path.exists(side):
        return "missing"
    with open(side, encoding="utf-8") as f:
        return "ok" if json.load(f).get("hash") == digest(endpoint, body) else "stale"


def generate(man, kind, sid, n, path, endpoint, body, entry):
    res, rid = call(endpoint, body)
    url = res["audio"]["url"]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        raw = os.path.join(tmp, "raw" + os.path.splitext(url.split("?")[0])[1])
        with urllib.request.urlopen(url, timeout=300, context=SSL) as r, open(raw, "wb") as f:
            f.write(r.read())
        mid = os.path.join(tmp, "mid.wav")
        to_wav48(raw, mid, 2 if kind == "music" else 1)
        a = read_wav(mid)
    if kind == "music":
        a = finish_music(a, MUSIC_RMS_DB)
        post = "afconvert -> 48 kHz stereo; 3 s fade-out; RMS %.0f dBFS, peak <= -1 dBFS" % MUSIC_RMS_DB
    elif entry.get("loop"):
        a = finish_loop(a, LOOP_RMS_DB)
        post = "afconvert -> 48 kHz mono; 0.5 s end-to-start cross-fade (seamless loop); RMS %.0f dBFS" % LOOP_RMS_DB
    else:
        pk = PEAK_DB.get(entry.get("group"), -6.0)
        a = finish_oneshot(a, pk)
        post = "afconvert -> 48 kHz mono; silence trimmed (-45 dB), 5/40 ms fades; peak %.0f dBFS" % pk
    write_wav(path, a)
    side = {"id": sid, "variant": n, "kind": kind, "file": os.path.relpath(path, ROOT), "endpoint": endpoint, "request": body,
            "hash": digest(endpoint, body), "fal_request_id": rid, "seed": res.get("seed"), "source_url": url,
            "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "postprocess": post, "sha256": sha256(path),
            "seconds": round(len(a) / RATE, 2), "license": man["models"][kind]["license"]}
    for k in ("mood", "group", "loop"):
        if k in entry:
            side[k] = entry[k]
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, ensure_ascii=False, indent=1)
    print("  %s  %.1f s  %s" % (os.path.relpath(path, ROOT), len(a) / RATE, rid))


def write_sources(man):
    lines = ["# Sound sources", "",
             "Every sound in the game and where it comes from. All of it is the project's own work or generated for it, released",
             "under CC BY 4.0 (LICENSE-ASSETS). Generated sounds are pinned in audio/sounds.json; each file's .json sidecar holds",
             "the exact request, the fal request id, the seed (music) and the sha256 of the committed file. Regenerate with",
             "tools/gen_sounds.py.", "",
             "## Procedural (code, no recordings)", "",
             "| Sound | Made by |", "|---|---|",
             "| Wind (rumble, whoosh, aeolian whistle, flight rush) | Private/Audio/AmbienceDSP.cpp FWindVoice |",
             "| Leaves rustling, pine needles | AmbienceDSP.cpp FAmbience::RenderLeaves (noise grains) |",
             "| Surf: breaking waves, wash, roar of the shore, calm-water slaps | AmbienceDSP.cpp FAmbience::RenderSurf |",
             "| Rain: bed, ticks, puddle plips, roof drumming; snow hush | AmbienceDSP.cpp FAmbience::RenderRain |",
             "| Field crickets (Dolbear's law) | AmbienceDSP.cpp FAmbience::RenderCrickets |",
             "| Thunder: crack and rolling rumble | AmbienceDSP.cpp FAmbience::RenderThunder |",
             "| Ventilation hum and air | tools/gen_vent_hum.py, tools/gen_vent_air.py |",
             "| Bird calls (29 species) | tools/birdsim/bird_calls.py |", "",
             "## Generated", "",
             "| File | Model | Seed | fal request | Prompt |", "|---|---|---|---|---|"]
    for kind, sid, n, path, endpoint, body, entry in outputs(man):
        side = path[:-4] + ".json"
        if not os.path.exists(side):
            continue
        with open(side, encoding="utf-8") as f:
            s = json.load(f)
        prompt = body.get("text") or body.get("prompt")
        lines.append("| %s | %s | %s | %s | %s |" % (s["file"], endpoint, s.get("seed") or "", s.get("fal_request_id", ""), prompt.replace("|", "/")))
    lines += ["", "## Licenses of the generators", ""]
    for kind, m in man["models"].items():
        lines.append("- %s (%s): %s" % (kind, m["endpoint"], m["license"]))
    with open(os.path.join(ROOT, "audio", "SOURCES.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--sfx", action="store_true")
    ap.add_argument("--music", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--sources", action="store_true")
    ap.add_argument("ids", nargs="*")
    a = ap.parse_args()
    man = load()
    todo = []
    cost = 0.0
    for o in outputs(man):
        kind, sid, n, path, endpoint, body, entry = o
        st = state(path, endpoint, body)
        if a.list:
            print("%-8s %-6s %s" % (st, kind, os.path.relpath(path, ROOT)))
        want = (a.sfx and kind == "sfx") or (a.music and kind == "music")
        if want and (not a.ids or sid in a.ids) and (a.force or st != "ok"):
            todo.append(o)
            cost += 0.2 if kind == "music" else 0.002 * body.get("duration_seconds", 5)
    if todo:
        print("generating %d takes, about $%.2f" % (len(todo), cost))
        for o in todo:
            generate(man, *o)
    elif a.list:
        print("pending if generated now: see above")
    write_sources(man)


if __name__ == "__main__":
    sys.exit(main())
