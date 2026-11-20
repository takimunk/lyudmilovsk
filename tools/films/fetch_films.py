#!/usr/bin/env python3
"""Fetch the Rassvet's newsreel films (tools/films/catalog.py) from Wikimedia Commons and encode them small for the game.

Run on a machine that can reach commons.wikimedia.org and has ffmpeg (the Mac; the cloud cannot reach Commons):

    python3 tools/films/fetch_films.py              # every film not yet in unreal/Lyudmilovsk/Content/Films
    python3 tools/films/fetch_films.py --force      # again, all of them
    python3 tools/films/fetch_films.py lumiere_train melies_moon
    python3 tools/films/fetch_films.py --pick melies_moon="File:Le Voyage dans la lune (1902).webm"

For each film it
  1. takes the Commons file pinned in films.lock.json, or else searches Commons' file namespace with the film's "commons"
     words and picks the best video: a length inside the film's "seconds", the licence "Public domain" (anything else is
     refused, so a restored copy with a new CC licence on its music or tinting never gets in), the bigger frame;
  2. downloads Commons' own 480p (or the nearest) transcode, else the original;
  3. encodes H.264 MP4, no sound, at most 640 px wide, CRF 28, fast start, into Content/Films/<id>.mp4 (git-ignored, staged
     NonUFS: DefaultGame.ini "Films"; the films are a download like the library's books, never in git or LFS);
  4. writes films.lock.json (the file page, licence, author, length, sha256 of the result) and regenerates SOURCES.md.

The rule for what may be fetched is in catalog.py's header. Network: one request at a time, a plain User-Agent naming the
project (Wikimedia asks automated clients for one), no personal details.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import catalog  # noqa: E402

OUT_DIR = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "Films")
LOCK = os.path.join(HERE, "films.lock.json")
SOURCES = os.path.join(HERE, "SOURCES.md")
API = "https://commons.wikimedia.org/w/api.php"
UA = "LyudmilovskFilmFetcher/1.0 (open-source game; https://github.com/takimunk/lyudmilovsk)"
MAX_W = 640
CRF = 28


def log(msg):
    print("films: " + msg, flush=True)


def api(**params):
    params.update(format="json", formatversion="2")
    url = API + "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                return json.load(r)
        except Exception as e:  # noqa: BLE001  (network: retry with backoff)
            if attempt == 3:
                raise
            log("  retry (%s)" % e)
            time.sleep(2 ** attempt)
    return {}


def strip_html(s):
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", s or "")).strip()


def is_public_domain(meta):
    short = strip_html(meta.get("LicenseShortName", {}).get("value", "")).lower()
    lic = strip_html(meta.get("License", {}).get("value", "")).lower()
    return short.startswith("public domain") or lic in ("pd", "public domain") or lic.startswith("pd-")


def file_info(title):
    """Commons' facts about one file page: url, size, mime, duration, licence and its transcodes."""
    d = api(action="query", titles=title, prop="videoinfo",
            viprop="url|size|mime|extmetadata|derivatives|mediatype", viurlwidth="")
    pages = d.get("query", {}).get("pages", [])
    if not pages or "videoinfo" not in pages[0]:
        return None
    vi = pages[0]["videoinfo"][0]
    meta = vi.get("extmetadata", {})
    return {
        "title": pages[0]["title"],
        "url": vi.get("url", ""),
        "page": vi.get("descriptionurl", ""),
        "mime": vi.get("mime", ""),
        "width": vi.get("width", 0),
        "height": vi.get("height", 0),
        "duration": float(vi.get("duration", 0) or 0),
        "licence": strip_html(meta.get("LicenseShortName", {}).get("value", "")),
        "licence_ok": is_public_domain(meta),
        "artist": strip_html(meta.get("Artist", {}).get("value", "")),
        "credit": strip_html(meta.get("Credit", {}).get("value", "")),
        "derivatives": vi.get("derivatives", []),
    }


def search(words):
    d = api(action="query", list="search", srnamespace="6", srlimit="20", srsearch=words + " filetype:video")
    return [h["title"] for h in d.get("query", {}).get("search", [])]


def pick(film):
    """The best Commons copy of a film: public domain, the right length, the biggest frame; None if nothing fits."""
    lo, hi = film["seconds"]
    seen, best = set(), None
    for words in film["commons"]:
        for title in search(words):
            if title in seen:
                continue
            seen.add(title)
            info = file_info(title)
            if not info:
                continue
            why = []
            if not info["licence_ok"]:
                why.append("licence %r" % info["licence"])
            if not lo <= info["duration"] <= hi:
                why.append("%.0f s (wants %d..%d)" % (info["duration"], lo, hi))
            if why:
                log("  skip %s: %s" % (title, ", ".join(why)))
                continue
            key = (info["height"], info["duration"])
            if best is None or key > (best["height"], best["duration"]):
                best = info
    return best


def source_url(info):
    """Commons' transcode nearest 480 lines (smaller to download), else the original."""
    vids = [d for d in info["derivatives"] if d.get("type", "").startswith("video/") and d.get("height")]
    if vids:
        vids.sort(key=lambda d: (abs(int(d["height"]) - 480), -int(d["height"])))
        return vids[0]["src"]
    return info["url"]


def download(url, path):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=300) as r, open(path, "wb") as f:
        shutil.copyfileobj(r, f, 1 << 20)


def encode(src, dst):
    """H.264 main, yuv420p, no audio, at most MAX_W wide (even sizes), CRF 28, moov first: small and plays everywhere UE does."""
    tmp = dst + ".part.mp4"
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", src, "-an",
           "-vf", "scale='min(%d,iw)':-2,setsar=1" % MAX_W,
           "-c:v", "libx264", "-profile:v", "main", "-pix_fmt", "yuv420p", "-preset", "slow", "-crf", str(CRF),
           "-movflags", "+faststart", tmp]
    subprocess.run(cmd, check=True)
    os.replace(tmp, dst)


def probe_seconds(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path],
                         check=True, capture_output=True, text=True).stdout.strip()
    return float(out or 0)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_lock():
    if os.path.exists(LOCK):
        with open(LOCK, encoding="utf-8") as f:
            return json.load(f)
    return {"about": "Written by tools/films/fetch_films.py: the Commons copy of each film and the encoded file's facts.",
            "films": {}}


def write_lock(lock):
    lock["films"] = dict(sorted(lock["films"].items()))
    with open(LOCK, "w", encoding="utf-8") as f:
        json.dump(lock, f, ensure_ascii=False, indent=1)
        f.write("\n")


def write_sources(lock):
    with open(SOURCES, "w", encoding="utf-8") as f:
        f.write(sources_md(lock))


def sources_md(lock):
    """SOURCES.md: what every film is, why it is free, and (once fetched) the exact Commons copy."""
    L = ["# The Rassvet's films: sources and licences", "",
         "Written by `tools/films/fetch_films.py` from `catalog.py` and `films.lock.json`; do not edit by hand.", "",
         "The films are not in git: `python3 tools/films/fetch_films.py` downloads them from Wikimedia Commons into",
         "`unreal/Lyudmilovsk/Content/Films/` (ignored by git, staged as plain files). Each copy is used only when its Commons",
         "page says Public domain; the script refuses any other licence.", "",
         "## Why they are free", "",
         "Every film was published before 1929, so it is in the public domain in the United States; every maker died in 1955",
         "or earlier, so the term (life + 70) is over in the European Union, Russia and the United Kingdom, and in Japan both",
         "the author term and the old 70-years-from-release term for films are over. **Exception: Mexico** (life + 100): the",
         "Lumières (d. 1948, 1954), Méliès (d. 1938), Cohl (d. 1938), Chomón (d. 1929) and Porter (d. 1941) are not yet free",
         "there, so a release that ships in Mexico must leave `Content/Films` out (the cinema then goes straight to the feature).", "",
         "The lecturer's words (the subtitles over the films) are ours, CC BY 4.0, like the rest of the game's text.", "",
         "## The films", "",
         "| id | film | year | made by (died) | Commons copy | licence on the page | length |",
         "|---|---|---|---|---|---|---|"]
    for film in catalog.FILMS:
        got = lock.get("films", {}).get(film["id"])
        died = ", ".join(str(y) for y in film["died"])
        if got:
            copy = "[%s](%s)" % (got["title"].replace("|", "/"), got["page"])
            lic = got["licence"]
            length = "%.0f s" % got["seconds"]
        else:
            copy, lic, length = "not fetched yet", "", ""
        L.append("| `%s` | %s | %d | %s (%s) | %s | %s | %s |" % (film["id"], film["title"][1], film["year"], film["maker"][1],
                                                                died, copy, lic, length))
    L += ["", "Authors and credit lines as Commons gives them are kept in `films.lock.json` (`artist`, `credit`).", ""]
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("ids", nargs="*", help="only these films")
    ap.add_argument("--force", action="store_true", help="fetch again even if the file is there")
    ap.add_argument("--pick", action="append", default=[], metavar="ID=File:...", help="pin a Commons file for a film")
    ap.add_argument("--sources", action="store_true", help="only rewrite SOURCES.md from the lock")
    args = ap.parse_args()
    lock = load_lock()
    if args.sources:
        write_sources(lock)
        return 0
    if not shutil.which("ffmpeg") or not shutil.which("ffprobe"):
        log("ffmpeg/ffprobe not found (brew install ffmpeg)")
        return 2
    by_id = {f["id"]: f for f in catalog.FILMS}
    picks = {}
    for p in args.pick:
        k, _, v = p.partition("=")
        if k not in by_id or not v.startswith("File:"):
            log("bad --pick %r" % p)
            return 2
        picks[k] = v
    ids = args.ids or [f["id"] for f in catalog.FILMS]
    for i in ids:
        if i not in by_id:
            log("unknown film %s" % i)
            return 2
    os.makedirs(OUT_DIR, exist_ok=True)
    failed = []
    for fid in ids:
        film = by_id[fid]
        dst = os.path.join(OUT_DIR, fid + ".mp4")
        if os.path.exists(dst) and not args.force and fid not in picks:
            log("%s: there already" % fid)
            continue
        log("%s: %s (%d)" % (fid, film["title"][1], film["year"]))
        pinned = picks.get(fid) or lock["films"].get(fid, {}).get("title")
        info = file_info(pinned) if pinned else pick(film)
        if info and pinned and not info["licence_ok"]:
            log("  %s: licence %r is not public domain: refused" % (pinned, info["licence"]))
            info = None
        if not info:
            log("  no usable copy found; pin one with --pick %s=\"File:...\"" % fid)
            failed.append(fid)
            continue
        url = source_url(info)
        log("  %s (%s, %.0f s, %dx%d) <- %s" % (info["title"], info["licence"], info["duration"], info["width"], info["height"], url))
        with tempfile.TemporaryDirectory() as tmp:
            raw = os.path.join(tmp, "src" + os.path.splitext(urllib.parse.urlparse(url).path)[1])
            try:
                download(url, raw)
                encode(raw, dst)
            except Exception as e:  # noqa: BLE001
                log("  failed: %s" % e)
                failed.append(fid)
                continue
        secs = probe_seconds(dst)
        lock["films"][fid] = {"title": info["title"], "page": info["page"], "licence": info["licence"], "artist": info["artist"],
                              "credit": info["credit"], "source": url, "seconds": round(secs, 2), "bytes": os.path.getsize(dst),
                              "sha256": sha256(dst), "encode": "h264 main crf %d max %d px, no audio" % (CRF, MAX_W)}
        log("  -> %s (%.1f s, %.1f MB)" % (os.path.relpath(dst, REPO), secs, os.path.getsize(dst) / 1e6))
        write_lock(lock)
    write_sources(lock)
    if failed:
        log("not fetched: " + ", ".join(failed))
        return 1
    log("done: %d films in %s" % (len([f for f in catalog.FILMS if os.path.exists(os.path.join(OUT_DIR, f["id"] + ".mp4"))]),
                                  os.path.relpath(OUT_DIR, REPO)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
