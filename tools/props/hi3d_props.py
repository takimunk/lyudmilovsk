"""Town props from hi3d (docs/city/hi3d_props.md): the steps around the paid hi3d calls, which go through
tools/birds/hi3d_mcp.py (upload / quote / create / download).

    python3 tools/props/hi3d_props.py fetch-sheets          # Higgsfield sheets -> blender/props_hi3d/raw/sheets/<n>.png
    python3 tools/props/hi3d_props.py cut                   # sheets -> blender/props_hi3d/raw/<id>/views/<view>.png
    python3 tools/props/hi3d_props.py args <id> <token>...  # prints the quote_generation JSON for these uploaded views
    python3 tools/props/hi3d_props.py submitted <id> <task_id> <points> <token>...   # sidecar entry for a created task
    python3 tools/props/hi3d_props.py fetched <id>          # after download to raw/<id>/model.glb: url, bytes, sha256
    python3 tools/props/hi3d_props.py status                # what is cut / submitted / downloaded / optimized

Raw files (sheets, views, hi3d GLBs) stay out of git (blender/props_hi3d/raw/); the sidecars
blender/props_hi3d/<id>/hi3d.json are committed.
"""
import datetime
import hashlib
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.join(ROOT, "blender", "props_hi3d")
RAW = os.path.join(HERE, "raw")
ROWS = ["front", "left", "back"]
HI3D_ARGS = {"version": "v3.0", "model_type": "high_quality", "generation_type": "model_with_texture", "pbr": True,
             "license": "private"}


def spec():
    return json.load(open(os.path.join(HERE, "props.json")))["props"]


def sheets():
    return json.load(open(os.path.join(HERE, "sheets.json")))["sheets"]


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def fetch_sheets():
    os.makedirs(os.path.join(RAW, "sheets"), exist_ok=True)
    for n, s in sheets().items():
        dest = os.path.join(RAW, "sheets", n + ".png")
        if os.path.exists(dest):
            print(dest, "exists")
            continue
        data = urllib.request.urlopen(urllib.request.Request(s["url"], headers={"User-Agent": "lyud-props/1"}), timeout=300).read()
        open(dest, "wb").write(data)
        print(dest, len(data), hashlib.sha256(data).hexdigest())


def crop(cell):
    """The object in a sheet cell on white: biggest ink blob (a neighbour's edge dropped), cropped with a margin and
    padded to a white square, at most 2048 px. Returns (image, touches_edge)."""
    a = np.asarray(cell.convert("RGB")).astype(int)
    ink = (255 * 3 - a.sum(2)) > 45
    lab, n = ndimage.label(ndimage.binary_dilation(ink, iterations=8))
    if n > 1:
        sizes = ndimage.sum(ink, lab, range(1, n + 1))
        big = 1 + int(np.argmax(sizes))
        keep = lab == big
        # keep other big parts too (a chandelier's rod can be thin; a dove can float off the hands)
        for i, s in enumerate(sizes):
            if s > 0.04 * sizes.max():
                keep |= lab == i + 1
        ink &= keep
        cell = Image.fromarray(np.where(ink[..., None], a, 255).astype(np.uint8))
    ys, xs = np.nonzero(ink)
    if len(xs) == 0:
        raise SystemExit("empty cell")
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    edge = bool(ink[0].any() or ink[-1].any() or ink[:, 0].any() or ink[:, -1].any())
    side = int(max(x1 - x0, y1 - y0) * 1.12)
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    out = Image.new("RGB", (side, side), (255, 255, 255))
    out.paste(cell.convert("RGB").crop((x0, y0, x1 + 1, y1 + 1)), (side // 2 - (cx - x0), side // 2 - (cy - y0)))
    if side > 2048:
        out = out.resize((2048, 2048), Image.LANCZOS)
    return out, edge


def cut():
    sp = spec()
    for n, s in sheets().items():
        src = os.path.join(RAW, "sheets", n + ".png")
        if not os.path.exists(src):
            print("no", src, "(fetch-sheets)")
            continue
        sheet = Image.open(src)
        W, H = sheet.size
        cols = s["columns"]
        cw, ch = W / len(cols), H / len(ROWS)
        for c, pid in enumerate(cols):
            d = os.path.join(RAW, pid, "views")
            os.makedirs(d, exist_ok=True)
            for r, view in enumerate(ROWS):
                cell = sheet.crop((int(c * cw), int(r * ch), int((c + 1) * cw), int((r + 1) * ch)))
                im, edge = crop(cell)
                im.save(os.path.join(d, view + ".png"))
                if view == "left" and "right" in sp[pid]["views"]:
                    im.transpose(Image.FLIP_LEFT_RIGHT).save(os.path.join(d, "right.png"))
                print(pid, view, im.size, "TOUCHES CELL EDGE" if edge else "")


def sidecar(pid):
    p = os.path.join(HERE, pid, "hi3d.json")
    return p, (json.load(open(p)) if os.path.exists(p) else [])


def args(pid, tokens):
    print(json.dumps(dict(HI3D_ARGS, file_tokens=tokens)))


def submitted(pid, task, points, tokens):
    views = spec()[pid]["views"]
    if len(tokens) != len(views):
        raise SystemExit("%s: %d tokens for views %s" % (pid, len(tokens), views))
    p, runs = sidecar(pid)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    ins = []
    for v in views:
        f = os.path.join(RAW, pid, "views", v + ".png")
        ins.append({"view": v, "file": os.path.relpath(f, ROOT), "sha256": sha(f)})
    runs.append({"service": "hi3d MCP (agent.hi3d.ai) via tools/birds/hi3d_mcp.py", "tool": "create_generation",
                 "kind": "model", "task_id": task, "points": float(points),
                 "arguments": dict(HI3D_ARGS, file_tokens="(uploaded views, see inputs)"), "inputs": ins,
                 "reference": "blender/props_hi3d/sheets.json", "submitted": now(), "outputs": []})
    json.dump(runs, open(p, "w"), indent=1, ensure_ascii=False)
    print("sidecar", os.path.relpath(p, ROOT))


def fetched(pid, url=None):
    p, runs = sidecar(pid)
    glb = os.path.join(RAW, pid, "model.glb")
    if not runs or not os.path.exists(glb):
        raise SystemExit("no run or no " + glb)
    r = runs[-1]
    r["outputs"] = [{"url": url or "(see hi3d task)", "file": os.path.relpath(glb, ROOT), "bytes": os.path.getsize(glb),
                     "sha256": sha(glb)}]
    r["model_page_url"] = "https://www.hi3d.ai/3d-model/" + r["task_id"]
    r["fetched"] = now()
    json.dump(runs, open(p, "w"), indent=1, ensure_ascii=False)
    print("fetched", pid, os.path.getsize(glb))


def status():
    for pid, s in sorted(spec().items(), key=lambda kv: kv[1]["priority"]):
        _, runs = sidecar(pid)
        views = all(os.path.exists(os.path.join(RAW, pid, "views", v + ".png")) for v in s["views"])
        game = os.path.exists(os.path.join(HERE, "game", pid + ".glb"))
        print("%-16s views %-3s runs %d glb %-3s game %s" % (pid, "yes" if views else "no", len(runs),
              "yes" if os.path.exists(os.path.join(RAW, pid, "model.glb")) else "no", "yes" if game else "no"))


def main():
    cmd, rest = (sys.argv[1], sys.argv[2:]) if len(sys.argv) > 1 else ("", [])
    if cmd == "fetch-sheets":
        fetch_sheets()
    elif cmd == "cut":
        cut()
    elif cmd == "args":
        args(rest[0], rest[1:])
    elif cmd == "submitted":
        submitted(rest[0], rest[1], rest[2], rest[3:])
    elif cmd == "fetched":
        fetched(rest[0], rest[1] if len(rest) > 1 else None)
    elif cmd == "status":
        status()
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
