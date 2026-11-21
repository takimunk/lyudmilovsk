"""Talks to the hi3d MCP (https://agent.hi3d.ai/mcp) directly over HTTP JSON-RPC, so reference pictures on disk can be
uploaded without passing base64 through a model's tool call.

    python3 tools/birds/hi3d_mcp.py balance
    python3 tools/birds/hi3d_mcp.py upload <file.png|jpg>            -> prints the file_token
    python3 tools/birds/hi3d_mcp.py quote '<json arguments>'         -> quote_generation
    python3 tools/birds/hi3d_mcp.py create <quote_id>                -> create_generation (PAID, confirmed=true)
    python3 tools/birds/hi3d_mcp.py task <task_id> | outputs <task_id>
    python3 tools/birds/hi3d_mcp.py download <task_id> <dest.glb>
    python3 tools/birds/hi3d_mcp.py call <tool> '<json arguments>'

Credential: HI3D_TOKEN_CMD (a shell command that prints the bearer token) if set, else the OAuth access token Claude
Code keeps for this MCP server in the macOS keychain entry "Claude Code-credentials" (mcpOAuth). Read at run time,
kept in memory only, never printed or written anywhere.
"""
import base64
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

URL = "https://agent.hi3d.ai/mcp"


def token():
    cmd = os.environ.get("HI3D_TOKEN_CMD")
    if cmd:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True, check=True).stdout.strip()
    raw = subprocess.run(["security", "find-generic-password", "-s", "Claude Code-credentials", "-w"],
                         capture_output=True, text=True, check=True).stdout
    oauth = json.loads(raw).get("mcpOAuth", {})
    for k, v in oauth.items():
        if "hi3d" in k.lower() and v.get("accessToken"):
            return v["accessToken"]
    raise SystemExit("no hi3d OAuth token in the keychain (reconnect hi3d in Claude Code)")


class Client:
    def __init__(self):
        self.tok = token()
        self.sid = None
        self.n = 0
        self.rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                "clientInfo": {"name": "lyud-birds", "version": "1"}})
        self.rpc("notifications/initialized", None, notify=True)

    def rpc(self, method, params, notify=False):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        if not notify:
            self.n += 1
            msg["id"] = self.n
        h = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream",
             "Authorization": "Bearer " + self.tok}
        if self.sid:
            h["Mcp-Session-Id"] = self.sid
        req = urllib.request.Request(URL, json.dumps(msg).encode(), h, method="POST")
        try:
            r = urllib.request.urlopen(req, timeout=600)
        except urllib.error.HTTPError as e:
            raise SystemExit(f"HTTP {e.code}: {e.read()[:300].decode('utf-8', 'replace')}")
        self.sid = r.headers.get("Mcp-Session-Id") or self.sid
        body = r.read().decode()
        if notify or not body.strip():
            return None
        if "text/event-stream" in (r.headers.get("Content-Type") or ""):
            datas = [l[5:].strip() for l in body.splitlines() if l.startswith("data:")]
            body = next((d for d in reversed(datas) if '"id"' in d), datas[-1])
        out = json.loads(body)
        if "error" in out:
            raise SystemExit("MCP error: " + json.dumps(out["error"])[:400])
        return out["result"]

    def tool(self, name, args):
        res = self.rpc("tools/call", {"name": name, "arguments": args})
        if res.get("structuredContent"):
            data = res["structuredContent"]
        else:
            txt = "".join(c.get("text", "") for c in res.get("content", []) if c.get("type") == "text")
            try:
                data = json.loads(txt)
            except ValueError:
                data = {"text": txt}
        if res.get("isError"):
            raise SystemExit(f"{name} failed: " + json.dumps(data, ensure_ascii=False)[:500])
        return data


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VIEWS = ["front", "back", "left", "right"]


def sidecar(sid):
    p = os.path.join(ROOT, "blender", "birds", "scans", sid, "hi3d.json")
    return p, (json.load(open(p)) if os.path.exists(p) else [])


def bird_submit(c, sid, master=False):
    import datetime
    import hashlib
    vd = os.path.join(ROOT, "blender", "birds", "scan_raw", sid, "views")
    toks, ins = [], []
    for v in VIEWS:
        f = os.path.join(vd, v + ".png")
        data = open(f, "rb").read()
        t = c.tool("upload_input", {"image_base64": base64.b64encode(data).decode(), "mime_type": "image/png"})["file_token"]
        toks.append(t)
        ins.append({"view": v, "file": os.path.relpath(f, ROOT), "sha256": hashlib.sha256(data).hexdigest()})
    args = {"file_tokens": toks, "version": "v3.0", "model_type": "master" if master else "high_quality",
            "generation_type": "model_with_texture", "pbr": True, "license": "private"}
    q = c.tool("quote_generation", args)
    task = c.tool("create_generation", {"quote_id": q["quote_id"], "confirmed": True})
    p, runs = sidecar(sid)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    runs.append({"service": "hi3d MCP (agent.hi3d.ai) via tools/birds/hi3d_mcp.py", "tool": "create_generation",
                 "kind": "model", "task_id": task["task_id"], "points": task.get("actual_points", q.get("estimated_points")),
                 "arguments": dict(args, file_tokens="(uploaded views, see inputs)"), "inputs": ins,
                 "submitted": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"), "outputs": []})
    json.dump(runs, open(p, "w"), indent=1, ensure_ascii=False)
    print(sid, task["task_id"], task.get("actual_points"), "pts")


def bird_poll(c):
    import datetime
    import hashlib
    sd = os.path.join(ROOT, "blender", "birds", "scans")
    for sid in sorted(os.listdir(sd)):
        p, runs = sidecar(sid)
        changed = False
        for r in runs:
            if r.get("kind") != "model" or r.get("outputs") or r.get("failed"):
                continue
            t = c.tool("get_task", {"task_id": r["task_id"]})
            if t["status"] in ("failed", "canceled", "cancelled"):
                r["failed"] = t.get("error") or t["status"]
                changed = True
                print(sid, "FAILED", r["failed"])
                continue
            if t["status"] != "succeeded":
                print(sid, t["status"], t.get("stage"), t.get("progress"))
                continue
            out = c.tool("get_task_outputs", {"task_id": r["task_id"]})
            glb = [a for a in out.get("assets", []) if a.get("name", "").lower().endswith(".glb") or a.get("format", "").upper() == "GLB"]
            if not glb:
                print(sid, "succeeded but no GLB asset", [a.get("name") for a in out.get("assets", [])])
                continue
            tag = "model_master" if r["arguments"].get("model_type") == "master" else "model"
            dest = os.path.join(ROOT, "blender", "birds", "scan_raw", sid, tag + ".glb")
            data = urllib.request.urlopen(urllib.request.Request(glb[0]["url"], headers={"User-Agent": "lyud-birds/1"}), timeout=900).read()
            open(dest, "wb").write(data)
            r["outputs"] = [{"url": glb[0]["url"], "file": os.path.relpath(dest, ROOT), "bytes": len(data),
                             "sha256": hashlib.sha256(data).hexdigest()}]
            r["model_page_url"] = out.get("model_page_url")
            r["fetched"] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
            changed = True
            print(sid, "DOWNLOADED", dest, len(data))
        if changed:
            json.dump(runs, open(p, "w"), indent=1, ensure_ascii=False)


def main():
    cmd, rest = sys.argv[1], sys.argv[2:]
    c = Client()
    if cmd == "balance":
        out = c.tool("get_balance", {})
        print(json.dumps({"available_points": out.get("available_points")}))
    elif cmd == "upload":
        p = rest[0]
        low = p.lower()
        mime = "image/png" if low.endswith(".png") else "image/webp" if low.endswith(".webp") else "image/jpeg"
        out = c.tool("upload_input", {"image_base64": base64.b64encode(open(p, "rb").read()).decode(), "mime_type": mime})
        print(out["file_token"])
    elif cmd == "quote":
        print(json.dumps(c.tool("quote_generation", json.loads(rest[0])), ensure_ascii=False))
    elif cmd == "create":
        print(json.dumps(c.tool("create_generation", {"quote_id": rest[0], "confirmed": True}), ensure_ascii=False))
    elif cmd in ("task", "outputs"):
        print(json.dumps(c.tool("get_task" if cmd == "task" else "get_task_outputs", {"task_id": rest[0]}), ensure_ascii=False))
    elif cmd == "download":
        out = c.tool("get_task_outputs", {"task_id": rest[0]})
        glb = [a for a in out.get("assets", []) if a.get("format", "").upper() == "GLB" or a.get("name", "").lower().endswith(".glb")]
        if not glb:
            raise SystemExit("no GLB yet: " + json.dumps(out, ensure_ascii=False)[:400])
        data = urllib.request.urlopen(urllib.request.Request(glb[0]["url"], headers={"User-Agent": "lyud-birds/1"}), timeout=900).read()
        os.makedirs(os.path.dirname(os.path.abspath(rest[1])), exist_ok=True)
        open(rest[1], "wb").write(data)
        print(json.dumps({"url": glb[0]["url"], "bytes": len(data)}))
    elif cmd == "bird-submit":   # bird-submit [--master] Id ...: upload the cut views, quote, create (PAID)
        master = "--master" in rest
        for sid in [r for r in rest if not r.startswith("--")]:
            bird_submit(c, sid, master)
    elif cmd == "bird-poll":     # bird-poll: download every finished, not yet fetched bird model
        bird_poll(c)
    elif cmd == "call":
        print(json.dumps(c.tool(rest[0], json.loads(rest[1]) if len(rest) > 1 else {}), ensure_ascii=False))
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
