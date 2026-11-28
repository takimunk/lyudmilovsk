"""Downloads a finished hi3d task's outputs for one bird species and records them in its committed sidecar.

    python3 tools/birds/hi3d_fetch.py <Id> views <task_id> '<json args>' <url> [<url> ...]
    python3 tools/birds/hi3d_fetch.py <Id> model <task_id> '<json args>' <url>

views -> blender/birds/scan_raw/<Id>/views/<i>.png, model -> blender/birds/scan_raw/<Id>/model.glb (both git-ignored).
Sidecar: blender/birds/scans/<Id>/hi3d.json, a list of runs (service, tool, arguments, task id, outputs with sha256).
The MCP calls themselves (create_image_generation / create_generation) are made by the agent; this pins what came back.
"""
import datetime
import hashlib
import json
import os
import sys
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def main():
    sid, kind, task, args, urls = sys.argv[1], sys.argv[2], sys.argv[3], json.loads(sys.argv[4]), sys.argv[5:]
    raw = os.path.join(ROOT, "blender", "birds", "scan_raw", sid)
    os.makedirs(os.path.join(raw, "views"), exist_ok=True)
    outs = []
    for i, u in enumerate(urls):
        dest = os.path.join(raw, "views", f"{i}.png") if kind == "views" else os.path.join(raw, "model.glb")
        req = urllib.request.Request(u, headers={"User-Agent": "lyud-birds/1"})
        data = urllib.request.urlopen(req, timeout=600).read()
        open(dest, "wb").write(data)
        outs.append({"url": u, "file": os.path.relpath(dest, ROOT), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        print(dest, len(data))
    side_dir = os.path.join(ROOT, "blender", "birds", "scans", sid)
    os.makedirs(side_dir, exist_ok=True)
    side = os.path.join(side_dir, "hi3d.json")
    runs = json.load(open(side)) if os.path.exists(side) else []
    runs.append({"service": "hi3d MCP (agent.hi3d.ai)", "tool": "create_image_generation" if kind == "views" else "create_generation",
                 "kind": kind, "task_id": task, "arguments": args, "outputs": outs,
                 "fetched": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")})
    json.dump(runs, open(side, "w"), indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
