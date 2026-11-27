#!/usr/bin/env python3
"""Fetch the eight records for the craft's record player (tools/spacecraft/records.json) and make them game-ready.

Run on a machine that can reach the hosts (upload.wikimedia.org, commons.wikimedia.org, archive.org, opengameart.org)
and has ffmpeg/ffprobe (the Mac; the cloud sandbox cannot reach them):

    python3 tools/spacecraft/fetch_records.py              # every record not yet made
    python3 tools/spacecraft/fetch_records.py --pin        # first run: also write sha256/bytes/seconds into records.json
    python3 tools/spacecraft/fetch_records.py --force record_sc_clair_de_lune
    python3 tools/spacecraft/fetch_records.py --check      # no network: sources, wavs and sidecars all match
    python3 tools/spacecraft/fetch_records.py --list       # the table

For each record it
  1. (Commons) asks the Commons API for the file's licence, size and sha1 and refuses a licence that is not on the
     allow-list (public domain / CC0 / CC BY / CC BY-SA); (archive.org) reads the item's metadata for the file's md5,
     size and licence URL; (plain URL) takes records.json at its word;
  2. downloads the original into audio/music/spacecraft/_src/ (git-ignored) and checks it: against the pinned sha256 when
     records.json has one (a mismatch is an error: the host changed the file), else against the host's own hash; with
     --pin the sha256, bytes and seconds are then written back into records.json (commit that);
  3. runs ffmpeg: trim leading/trailing silence, optional high/low-pass for old transfers, mono or stereo as the recipe
     says, two-pass loudnorm to the common target, 48 kHz 16-bit WAV at audio/music/spacecraft/<id>.wav (git-ignored);
     no crackle is baked in (the game's player adds it);
  4. writes the sidecar audio/music/spacecraft/<id>.json (committed), shaped like the repo's other audio sidecars.

Stdlib only. Network: one request at a time, a generic User-Agent naming no person and no project. Wikimedia asks
automated clients for a contact: whoever runs it may add one with RECORDS_UA_CONTACT="<url or address>" (left out by
default).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
CATALOG = os.path.join(HERE, "records.json")
OUT = os.path.join(REPO, "audio", "music", "spacecraft")
SRC = os.path.join(OUT, "_src")
COMMONS_API = "https://commons.wikimedia.org/w/api.php"
ENWIKI_API = "https://en.wikipedia.org/w/api.php"
UA = "RecordFetcher/1.0 (python-urllib; openly licensed music archive script)" + (
    " contact: " + os.environ["RECORDS_UA_CONTACT"].strip() if os.environ.get("RECORDS_UA_CONTACT", "").strip() else "")

# what a Commons/archive.org licence may say for the file to be taken (lower-case prefixes / substrings)
ALLOW = ("public domain", "cc0", "cc-zero", "cc by ", "cc-by-", "cc by-sa", "cc-by-sa",
         "attribution", "publicdomain/zero", "publicdomain/mark", "licenses/by/", "licenses/by-sa/")
DENY = ("nc", "nd", "noncommercial", "noderivs", "no derivatives", "fair use", "non-free")


def log(msg):
    print("records: " + msg, flush=True)


def rel(p):
    return os.path.relpath(p, REPO).replace(os.sep, "/")


# ---------------------------------------------------------------- catalog

def load_catalog():
    with open(CATALOG, encoding="utf-8") as f:
        return json.load(f)


def save_catalog(cat):
    with open(CATALOG, "w", encoding="utf-8") as f:
        json.dump(cat, f, ensure_ascii=False, indent=1)
        f.write("\n")


def src_path(rec):
    ext = os.path.splitext(urllib.parse.unquote(urllib.parse.urlparse(rec["file_url"]).path))[1].lower() or ".bin"
    return os.path.join(SRC, rec["id"] + ext)


def wav_path(rec):
    return os.path.join(OUT, rec["id"] + ".wav")


def side_path(rec):
    return os.path.join(OUT, rec["id"] + ".json")


# ---------------------------------------------------------------- network

def get(url, timeout=120):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    for attempt in range(4):
        try:
            return urllib.request.urlopen(req, timeout=timeout)
        except Exception as e:  # noqa: BLE001  (network: retry with backoff)
            if attempt == 3 or getattr(e, "code", 0) in (403, 404, 410) or "403" in str(e):
                raise
            log("  retry (%s)" % e)
            time.sleep(2 ** attempt)
    raise RuntimeError("unreachable")


def get_json(url):
    with get(url, 60) as r:
        return json.load(r)


def strip_html(s):
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", s or "")).strip()


def licence_ok(text):
    t = " " + text.lower().strip() + " "
    words = set(re.split(r"[^a-z]+", t))
    if any(d in words for d in DENY if " " not in d) or any(d in t for d in DENY if " " in d):
        return False
    return bool(words & {"pd", "pdm"}) or any(a in t for a in ALLOW)


def commons_facts(rec):
    """Licence, size, sha1 and url of a Commons file (falls back to English Wikipedia for a local upload)."""
    title = rec["commons_file"]
    for api in (COMMONS_API, ENWIKI_API):
        q = urllib.parse.urlencode({"action": "query", "titles": title, "prop": "imageinfo",
                                    "iiprop": "url|size|sha1|mime|extmetadata", "format": "json", "formatversion": "2"})
        d = get_json(api + "?" + q)
        pages = d.get("query", {}).get("pages", [])
        if pages and "imageinfo" in pages[0]:
            ii = pages[0]["imageinfo"][0]
            meta = ii.get("extmetadata", {})
            lic = strip_html(meta.get("LicenseShortName", {}).get("value", ""))
            lic_url = strip_html(meta.get("LicenseUrl", {}).get("value", ""))
            return {"url": ii["url"], "page": ii.get("descriptionurl", rec["source_page"]), "bytes": ii.get("size"),
                    "sha1": ii.get("sha1"), "licence": lic, "licence_url": lic_url,
                    "artist": strip_html(meta.get("Artist", {}).get("value", "")), "api": api}
    raise RuntimeError("%s: not found on Commons or English Wikipedia" % title)


def archive_facts(rec):
    d = get_json("https://archive.org/metadata/" + urllib.parse.quote(rec["archive_item"]))
    lic = d.get("metadata", {}).get("licenseurl", "")
    for f in d.get("files", []):
        if f.get("name") == rec["archive_file"]:
            return {"url": rec["file_url"], "page": rec["source_page"], "bytes": int(f.get("size", 0) or 0),
                    "md5": f.get("md5"), "sha1": f.get("sha1"), "licence": lic, "licence_url": lic}
    raise RuntimeError("%s: no file %r in the item" % (rec["archive_item"], rec["archive_file"]))


def host_facts(rec):
    if rec["host"] == "commons":
        return commons_facts(rec)
    if rec["host"] == "archive":
        return archive_facts(rec)
    return {"url": rec["file_url"], "page": rec["source_page"], "licence": rec["licence"],
            "licence_url": rec["licence_url"]}


def download(url, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".part"
    with get(url, 600) as r, open(tmp, "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)
    os.replace(tmp, path)


# ---------------------------------------------------------------- files

def digest(path, algo="sha256"):
    h = hashlib.new(algo)
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def probe(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration:stream=channels,sample_rate",
                          "-of", "json", path], check=True, capture_output=True, text=True).stdout
    d = json.loads(out)
    st = (d.get("streams") or [{}])[0]
    return {"seconds": round(float(d.get("format", {}).get("duration", 0) or 0), 2),
            "channels": int(st.get("channels", 0) or 0), "rate": int(st.get("sample_rate", 0) or 0)}


def verify_source(rec, path, facts=None):
    """None when the downloaded original is the pinned one (or matches the host's hash before pinning), else why not."""
    if not os.path.exists(path):
        return "missing"
    if rec.get("sha256"):
        got = digest(path)
        return None if got == rec["sha256"] else "sha256 %s, pinned %s" % (got[:12], rec["sha256"][:12])
    if facts:
        if facts.get("sha1") and digest(path, "sha1") != facts["sha1"]:
            return "sha1 differs from the host's"
        if facts.get("md5") and digest(path, "md5") != facts["md5"]:
            return "md5 differs from the host's"
        if facts.get("bytes") and os.path.getsize(path) != facts["bytes"]:
            return "size differs from the host's"
        return None
    return "not pinned yet (run with --pin on a machine with network)"


# ---------------------------------------------------------------- processing

def filters(rec, target):
    r = rec.get("recipe", {})
    f = ["aformat=channel_layouts=%s" % ("mono" if r.get("channels", 2) == 1 else "stereo")]
    if r.get("highpass_hz"):
        f.append("highpass=f=%d" % r["highpass_hz"])
    if r.get("lowpass_hz"):
        f.append("lowpass=f=%d" % r["lowpass_hz"])
    if r.get("trim", True):
        sil = target["silence_db"]
        trim = "silenceremove=start_periods=1:start_threshold=%ddB:start_silence=0.15" % sil
        f += [trim, "areverse", trim, "areverse"]
    return f


def recipe_text(rec, target):
    r = rec.get("recipe", {})
    bits = ["trim silence below %d dBFS at both ends" % target["silence_db"]] if r.get("trim", True) else []
    if r.get("highpass_hz"):
        bits.append("high-pass %d Hz" % r["highpass_hz"])
    if r.get("lowpass_hz"):
        bits.append("low-pass %d Hz" % r["lowpass_hz"])
    bits.append("mono" if r.get("channels", 2) == 1 else "stereo")
    bits.append("two-pass loudnorm I=%g LUFS TP=%g dB LRA=%g" % (target["lufs"], target["true_peak_db"], target["lra"]))
    bits.append("fade in %gs / out %gs" % (target["fade_in_s"], target["fade_out_s"]))
    bits.append("%d Hz 16-bit WAV; no vinyl noise baked in" % target["rate"])
    return "ffmpeg: " + "; ".join(bits)


def render(rec, target, src, dst):
    pre = filters(rec, target)
    ln = "loudnorm=I=%g:TP=%g:LRA=%g" % (target["lufs"], target["true_peak_db"], target["lra"])
    # pass 1: measure
    p1 = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", src,
                         "-af", ",".join(pre + [ln + ":print_format=json"]), "-f", "null", "-"],
                        check=True, capture_output=True, text=True).stderr
    m = json.loads(p1[p1.rindex("{"):p1.rindex("}") + 1])
    ln2 = (ln + ":measured_I=%s:measured_TP=%s:measured_LRA=%s:measured_thresh=%s:offset=%s:linear=true"
           % (m["input_i"], m["input_tp"], m["input_lra"], m["input_thresh"], m["target_offset"]))
    # trimmed length, for the fade-out position
    tmp = dst + ".part.wav"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", src,
                    "-af", ",".join(pre + [ln2]), "-ar", str(target["rate"]), "-c:a", "pcm_s16le", tmp], check=True)
    secs = probe(tmp)["seconds"]
    fo = max(0.0, secs - target["fade_out_s"])
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", tmp,
                    "-af", "afade=t=in:d=%g,afade=t=out:st=%.3f:d=%g" % (target["fade_in_s"], fo, target["fade_out_s"]),
                    "-ar", str(target["rate"]), "-c:a", "pcm_s16le", "-map_metadata", "-1", "-bitexact", dst], check=True)
    os.remove(tmp)
    return m


def sidecar(rec, target, facts, measured):
    info = probe(wav_path(rec))
    return {
        "id": rec["id"],
        "variant": 1,
        "kind": "music",
        "file": rel(wav_path(rec)),
        "endpoint": "fetched: " + rec["host"],
        "title": rec["work"],
        "composer": rec["composer"],
        "performer": rec["performer"],
        "recorded": rec.get("recorded", ""),
        "source_page": facts.get("page") or rec["source_page"],
        "source_url": facts.get("url") or rec["file_url"],
        "source_sha256": rec.get("sha256") or digest(src_path(rec)),
        "source_bytes": os.path.getsize(src_path(rec)),
        "created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "postprocess": recipe_text(rec, target),
        "measured_input": {k: measured.get(k) for k in ("input_i", "input_tp", "input_lra")} if measured else None,
        "sha256": digest(wav_path(rec)),
        "seconds": info["seconds"],
        "channels": info["channels"],
        "license": rec["licence"],
        "license_url": rec["licence_url"],
        "attribution": rec["attribution"],
        "host_licence": facts.get("licence", ""),
        "mood": rec.get("mood", []),
    }


def write_json(path, d):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(d, f, ensure_ascii=False, indent=1)
        f.write("\n")


# ---------------------------------------------------------------- commands

def fetch_one(cat, rec, force, pin):
    target = cat["target"]
    src, dst = src_path(rec), wav_path(rec)
    if not force and os.path.exists(dst) and os.path.exists(side_path(rec)) and not verify_source(rec, src):
        log("%s: up to date" % rec["id"])
        return True
    facts = host_facts(rec)
    if not licence_ok(facts.get("licence", "") + " " + facts.get("licence_url", "")):
        log("%s: REFUSED, the host says the licence is %r" % (rec["id"], facts.get("licence")))
        return False
    log("%s: licence %r" % (rec["id"], facts.get("licence") or rec["licence"]))
    if force or verify_source(rec, src, facts):
        log("  download %s" % facts["url"])
        download(facts["url"], src)
    why = verify_source(rec, src, facts)
    if why:
        log("%s: SOURCE CHECK FAILED: %s" % (rec["id"], why))
        return False
    info = probe(src)
    lo, hi = rec.get("seconds_expected", [0, 1e9])
    if not lo <= info["seconds"] <= hi:
        log("%s: WARNING length %.1f s outside the expected %s..%s s" % (rec["id"], info["seconds"], lo, hi))
    if pin and not rec.get("sha256"):
        rec["sha256"], rec["bytes"], rec["seconds"] = digest(src), os.path.getsize(src), info["seconds"]
        save_catalog(cat)
        log("  pinned sha256 %s" % rec["sha256"][:16])
    measured = render(rec, target, src, dst)
    side = sidecar(rec, target, facts, measured)
    write_json(side_path(rec), side)
    log("  -> %s (%.1f s, %d ch)" % (rel(dst), side["seconds"], side["channels"]))
    return True


def check(cat):
    """No network: every source matches its pin, every wav matches its sidecar."""
    bad = 0
    for rec in cat["records"]:
        problems = []
        why = verify_source(rec, src_path(rec))
        if why:
            problems.append("source " + why)
        if not os.path.exists(side_path(rec)):
            problems.append("no sidecar")
        elif not os.path.exists(wav_path(rec)):
            problems.append("no wav (fetch it)")
        else:
            with open(side_path(rec), encoding="utf-8") as f:
                side = json.load(f)
            if side.get("sha256") != digest(wav_path(rec)):
                problems.append("wav differs from its sidecar (another ffmpeg? re-run with --force)")
            if rec.get("sha256") and side.get("source_sha256") != rec["sha256"]:
                problems.append("sidecar made from another source file")
        print("%-32s %s" % (rec["id"], "ok" if not problems else "; ".join(problems)))
        bad += bool(problems)
    print("%d of %d records ok" % (len(cat["records"]) - bad, len(cat["records"])))
    return bad == 0


def list_table(cat):
    print("%-4s %-32s %-28s %-24s %-32s %-8s %s" % ("cell", "id", "composer", "performer", "licence", "seconds", "pinned"))
    for rec in cat["records"]:
        print("%-4s %-32s %-28s %-24s %-32s %-8s %s" % (
            rec.get("cell", ""), rec["id"], rec["composer"][:28], rec["performer"][:24], rec["licence"][:32],
            rec.get("seconds") or "~%d" % (sum(rec.get("seconds_expected", [0, 0])) / 2),
            (rec.get("sha256") or "-")[:12]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("ids", nargs="*", help="record ids (default: all)")
    ap.add_argument("--force", action="store_true", help="download and render again")
    ap.add_argument("--pin", action="store_true", help="write sha256/bytes/seconds of unpinned sources into records.json")
    ap.add_argument("--check", action="store_true", help="verify without network")
    ap.add_argument("--list", action="store_true", help="print the table")
    a = ap.parse_args()
    cat = load_catalog()
    if a.list:
        list_table(cat)
        return 0
    if a.check:
        return 0 if check(cat) else 1
    for tool in ("ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            log("needs %s on PATH" % tool)
            return 2
    recs = [r for r in cat["records"] if not a.ids or r["id"] in a.ids]
    unknown = set(a.ids) - {r["id"] for r in recs}
    if unknown:
        log("unknown ids: %s" % ", ".join(sorted(unknown)))
        return 2
    ok = 0
    for rec in recs:
        try:
            ok += fetch_one(cat, rec, a.force, a.pin)
        except Exception as e:  # noqa: BLE001  (one bad host must not stop the rest)
            log("%s: FAILED: %s" % (rec["id"], e))
    log("%d of %d records made" % (ok, len(recs)))
    return 0 if ok == len(recs) else 1


if __name__ == "__main__":
    sys.exit(main())
