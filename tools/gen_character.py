"""Runs the paid generations that make a character from a reference photo (images, then the 3D model) on fal.

  blender/characters/<id>/gen.json        the steps: model endpoint, prompt, inputs (@reference, @<step>), args (seed pinned)
  blender/characters/<id>/gen/<step>.json what was sent and got back: request id, the exact inputs, output URL and sha256 (committed)
  blender/characters/<id>/gen/<step>.png|.glb  the output itself (local, ignored; refetch from the sidecar's URL)

    python3 tools/gen_character.py nao front           run one step (skipped if its sidecar exists; --force to redo)
    python3 tools/gen_character.py nao --fetch         download the outputs again from the sidecars' URLs
    python3 tools/gen_character.py nao --list

FAL_API_KEY in the environment (the key is in ~/projects/asmrx3000/.env; never commit it). @<step> inputs use the fal URL
recorded in that step's sidecar, so a rerun of a later step sends the same picture.
"""
import argparse
import base64
import hashlib
import json
import mimetypes
import os
import sys
import time
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def call(url, body=None):
    req = urllib.request.Request(url, json.dumps(body).encode() if body is not None else None,
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(req, timeout=600))


def data_uri(path):
    mime = mimetypes.guess_type(path)[0] or "image/webp"
    with open(path, "rb") as f:
        return "data:%s;base64,%s" % (mime, base64.b64encode(f.read()).decode())


def resolve(ref, char_dir, spec):
    if ref == "@reference":
        return data_uri(os.path.join(char_dir, spec["reference"]))
    if ref.startswith("@"):
        side = os.path.join(char_dir, "gen", ref[1:] + ".json")
        if not os.path.isfile(side):
            sys.exit("step %s has not run yet" % ref[1:])
        return json.load(open(side))["output_url"]
    return ref


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def output_url(res):
    if "images" in res:
        return res["images"][0]["url"]
    m = res.get("model_mesh") or res.get("model_glb") or {}
    return m["url"]


def run_step(char_id, step, force):
    char_dir = os.path.join(ROOT, "blender", "characters", char_id)
    spec = json.load(open(os.path.join(char_dir, "gen.json")))
    s = spec["steps"][step]
    side = os.path.join(char_dir, "gen", step + ".json")
    if os.path.isfile(side) and not force:
        print("%s: done before (%s); --force to redo" % (step, os.path.relpath(side, ROOT)))
        return
    body = dict(s.get("args", {}))
    if "prompt" in s:
        body["prompt"] = s["prompt"]
    imgs = s.get("images", [])
    if isinstance(imgs, dict):
        body.update({k: resolve(v, char_dir, spec) for k, v in imgs.items()})
        shown = imgs
    else:
        body["image_urls"] = [resolve(v, char_dir, spec) for v in imgs]
        shown = imgs
    sub = call("https://queue.fal.run/" + s["model"], body)
    rid = sub["request_id"]
    print("%s: %s queued %s" % (step, s["model"], rid), flush=True)
    t0 = time.time()
    while True:
        st = call(sub["status_url"])
        if st["status"] == "COMPLETED":
            break
        if st["status"] not in ("IN_QUEUE", "IN_PROGRESS"):
            sys.exit("%s failed: %s" % (step, st))
        time.sleep(5)
    res = call(sub["response_url"])
    url = output_url(res)
    os.makedirs(os.path.dirname(side), exist_ok=True)
    ext = os.path.splitext(url.split("?")[0])[1] or ".bin"
    out = os.path.join(char_dir, "gen", step + ext)
    urllib.request.urlretrieve(url, out)
    sent = {k: v for k, v in body.items() if not (isinstance(v, str) and v.startswith("data:"))}
    if isinstance(shown, list):
        sent["image_urls"] = [u if not u.startswith("data:") else r for u, r in zip(body["image_urls"], shown)]
    json.dump({"step": step, "model": s["model"], "request_id": rid, "seconds": round(time.time() - t0),
               "inputs": sent, "output_url": url, "output_file": os.path.basename(out), "sha256": sha256(out),
               "response": {k: v for k, v in res.items() if k not in ("images",)}},
              open(side, "w"), ensure_ascii=False, indent=1)
    print("%s: %s (%.0f s)" % (step, os.path.relpath(out, ROOT), time.time() - t0))


def fetch(char_id):
    gen = os.path.join(ROOT, "blender", "characters", char_id, "gen")
    for f in sorted(os.listdir(gen)):
        if f.endswith(".json"):
            d = json.load(open(os.path.join(gen, f)))
            out = os.path.join(gen, d["output_file"])
            if not os.path.isfile(out) or sha256(out) != d["sha256"]:
                urllib.request.urlretrieve(d["output_url"], out)
                print("fetched", os.path.relpath(out, ROOT), "ok" if sha256(out) == d["sha256"] else "SHA MISMATCH")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("char")
    ap.add_argument("steps", nargs="*")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--fetch", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    if a.list:
        spec = json.load(open(os.path.join(ROOT, "blender", "characters", a.char, "gen.json")))
        for k, s in spec["steps"].items():
            done = os.path.isfile(os.path.join(ROOT, "blender", "characters", a.char, "gen", k + ".json"))
            print("%-8s %-40s %s" % (k, s["model"], "done" if done else "-"))
    elif a.fetch:
        fetch(a.char)
    for st in a.steps:
        run_step(a.char, st, a.force)


if __name__ == "__main__":
    main()
