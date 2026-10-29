"""Paid generations for the cable-car metro's 3D assets (docs/transport.md), pinned in sidecars.

  design/transport/gen.json            the steps: fal model endpoint, inputs (image URLs of the Higgsfield
                                       Nano Banana 2.1 renders, see design/transport/concepts/sources.json), args
  design/transport/gen/<step>.json     what was sent and got back: request id, inputs, output URL, sha256 (committed)
  design/transport/gen/<step>.glb      the raw model (not committed: blender/transport/optimize.py makes the game mesh)

  FAL_API_KEY=... python3 tools/gen_transport_assets.py <step> [<step> ...] [--force]
  python3 tools/gen_transport_assets.py --list

These ten models went through Hi3D v3.0 on fal (its API takes image URLs; the hi3d MCP only takes inline base64).
Dima's rule since (2026-10-25): 3D goes ONLY through the hi3d MCP (shrink the image for upload_input, then
quote_generation and create_generation), so this script is the record of how they were made, not the way to make
new ones. The key is in ~/projects/asmrx3000/.env; never commit it.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import sys
import time
import urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SPEC = os.path.join(ROOT, "design", "transport", "gen.json")
GEN = os.path.join(ROOT, "design", "transport", "gen")


def call(url, body=None):
    req = urllib.request.Request(url, json.dumps(body).encode() if body is not None else None,
                                 {"Authorization": "Key " + os.environ["FAL_API_KEY"], "Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(req, timeout=600))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def run_step(step, force):
    spec = json.load(open(SPEC))
    s = spec["steps"][step]
    side = os.path.join(GEN, step + ".json")
    if os.path.isfile(side) and not force:
        return "%s: done before; --force to redo" % step
    body = dict(s.get("args", {}))
    body.update(s.get("inputs", {}))
    if "prompt" in s:
        body["prompt"] = s["prompt"]
    sub = call("https://queue.fal.run/" + s["model"], body)
    rid = sub["request_id"]
    print("%s: %s queued %s" % (step, s["model"], rid), flush=True)
    t0 = time.time()
    while True:
        st = call(sub["status_url"])
        if st["status"] == "COMPLETED":
            break
        if st["status"] not in ("IN_QUEUE", "IN_PROGRESS"):
            return "%s failed: %s" % (step, st)
        time.sleep(10)
    res = call(sub["response_url"])
    m = res.get("model_mesh") or res.get("model_glb") or {}
    url = m.get("url") or res["images"][0]["url"]
    os.makedirs(GEN, exist_ok=True)
    ext = os.path.splitext(url.split("?")[0])[1] or ".bin"
    out = os.path.join(GEN, step + ext)
    urllib.request.urlretrieve(url, out)
    json.dump({"step": step, "model": s["model"], "request_id": rid, "seconds": round(time.time() - t0),
               "inputs": body, "output_url": url, "output_file": os.path.basename(out), "sha256": sha256(out),
               "response": {k: v for k, v in res.items() if k != "images"}},
              open(side, "w"), ensure_ascii=False, indent=1)
    return "%s: %s (%.0f s)" % (step, os.path.relpath(out, ROOT), time.time() - t0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("steps", nargs="*")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    spec = json.load(open(SPEC))
    if a.list or not a.steps:
        for k, s in spec["steps"].items():
            print("%-16s %-40s %s" % (k, s["model"], "done" if os.path.isfile(os.path.join(GEN, k + ".json")) else "-"))
        return
    with concurrent.futures.ThreadPoolExecutor(len(a.steps)) as ex:
        for r in ex.map(lambda st: run_step(st, a.force), a.steps):
            print(r, flush=True)


if __name__ == "__main__":
    main()
