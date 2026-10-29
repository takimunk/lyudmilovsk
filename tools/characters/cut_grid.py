"""Cuts a turnaround grid (4 people in rows x back / left / right in columns, one Higgsfield picture) into the views
the 3D step needs, and puts each on fal's storage so hi3d on fal can read it.

    python3 tools/characters/cut_grid.py <grid.png> <grid job id> <grid url> owner clerk streamer kid_boy

Writes blender/characters/<id>/gen/{back,left,right}.png and a sidecar per view (the grid it came from, the cell,
the uploaded URL, sha256). FAL_API_KEY in the environment.
"""
import hashlib
import json
import os
import subprocess
import sys
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VIEWS = ["back", "left", "right"]


def size(path):
    out = subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", path], capture_output=True, text=True).stdout
    vals = [int(l.split(":")[1]) for l in out.splitlines() if "pixel" in l]
    return vals[0], vals[1]


def upload(path):
    key = os.environ["FAL_API_KEY"]
    req = urllib.request.Request("https://rest.alpha.fal.ai/storage/upload/initiate?storage_type=fal-cdn-v3",
                                 json.dumps({"content_type": "image/png", "file_name": os.path.basename(path)}).encode(),
                                 {"Authorization": "Key " + key, "Content-Type": "application/json"})
    d = json.load(urllib.request.urlopen(req, timeout=60))
    put = urllib.request.Request(d["upload_url"], open(path, "rb").read(), {"Content-Type": "image/png"}, method="PUT")
    urllib.request.urlopen(put, timeout=300)
    return d["file_url"]


def main():
    grid, job, url, ids = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    w, h = size(grid)
    cw, ch = w // 3, h // len(ids)
    for r, cid in enumerate(ids):
        gen = os.path.join(ROOT, "blender", "characters", cid, "gen")
        os.makedirs(gen, exist_ok=True)
        for c, view in enumerate(VIEWS):
            out = os.path.join(gen, view + ".png")
            subprocess.run(["sips", "-c", str(ch), str(cw), "--cropOffset", str(r * ch), str(c * cw), grid, "--out", out],
                           check=True, capture_output=True)
            furl = upload(out)
            json.dump({"step": view, "service": "higgsfield grid, cut by tools/characters/cut_grid.py, uploaded to fal storage",
                       "grid_job_id": job, "grid_url": url, "cell": {"row": r, "col": c, "x": c * cw, "y": r * ch, "w": cw, "h": ch},
                       "output_url": furl, "output_file": view + ".png",
                       "sha256": hashlib.sha256(open(out, "rb").read()).hexdigest()},
                      open(os.path.join(gen, view + ".json"), "w"), indent=1)
            print(cid, view, furl)


if __name__ == "__main__":
    main()
