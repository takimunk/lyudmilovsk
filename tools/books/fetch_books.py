#!/usr/bin/env python3
"""The library's classics (docs/books.md): finds open texts of the works in canon.txt and downloads them.

    python3 tools/books/fetch_books.py resolve [ids...]   # canon.txt -> catalog.json (which text of each work, per language)
    python3 tools/books/fetch_books.py fetch [ids...]     # catalog.json -> unreal/Lyudmilovsk/Content/Books/<lang>/<id>.txt + books.json
    python3 tools/books/fetch_books.py report             # what the catalog has, per language

Only the catalog (sources, creators, licences) is committed; the texts are downloaded on every machine that builds the game
(Content/Books is in .gitignore) and packaged as plain files (DirectoriesToAlwaysStageAsNonUFS, Config/DefaultGame.ini). pins.json picks editions by hand.

Rules for a text to be taken: every creator of that text (the author and the translator) died in 1955 or earlier, so it is
out of copyright in the countries the game ships to; the source says it is free (Project Gutenberg's public-domain books,
Aozora Bunko's 著作権なし works, Wikisource's PD-* licences). Each text records its source URL, its creators' death years and
its licence in catalog.json and books.json.

Sources: Project Gutenberg (en; the gutenberg.pglaf.org mirror while gutenberg.org is down), Aozora Bunko (ja; its catalogue
CSV), Russian Wikisource (ru; index.php only: its api.php refuses shared cloud addresses). Standard library only.
"""

from __future__ import annotations

import argparse
import concurrent.futures as cf
import csv
import hashlib
import html
import html.parser
import io
import json
import re
import sys
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CANON = HERE / "canon.txt"
CATALOG = HERE / "catalog.json"
CACHE = HERE / ".cache"  # .gitignore'd: raw downloads, so a second run costs nothing
OUT = ROOT / "unreal" / "Lyudmilovsk" / "Content" / "Books"
LANGS = ("ru", "en", "ja")
LAST_DEATH = 1955
UA = "BookFetch/1.0 (public-domain text collector)"
MAX_CHARS = 4_000_000  # one book; the reader pages it on a worker, but nothing in the canon comes near

# --------------------------------------------------------------------------------------------------------------------- net

_last_hit: dict[str, float] = {}


def get(url: str, *, binary: bool = False, cache: bool = True, delay: float = 1.0, tries: int = 4):
    """GET with a disk cache, one request a second per host, retries with backoff. None on 404."""
    key = hashlib.sha1(url.encode()).hexdigest()
    path = CACHE / key[:2] / key
    if cache and path.exists():
        data = path.read_bytes()
        return data if binary else data.decode("utf-8", "replace")
    host = urllib.parse.urlsplit(url).netloc
    for attempt in range(tries):
        wait = _last_hit.get(host, 0.0) + delay - time.time()
        if wait > 0:
            time.sleep(wait)
        _last_hit[host] = time.time()
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read()
            break
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if e.code in (429, 500, 502, 503, 504) and attempt + 1 < tries:
                time.sleep(5 * (attempt + 1))
                continue
            raise
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt + 1 < tries:
                time.sleep(5 * (attempt + 1))
                continue
            raise
    if cache:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return data if binary else data.decode("utf-8", "replace")


# ------------------------------------------------------------------------------------------------------------------ canon


def read_canon() -> list[dict]:
    works = []
    for line in CANON.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        f = [x.strip() for x in line.split("|")]
        if len(f) != 9:
            sys.exit(f"canon.txt: 9 fields expected: {line}")
        works.append({
            "id": f[0], "orig": f[1], "year": int(f[2]),
            "author": {"en": f[3], "ru": f[5], "ja": f[7]},
            "title": {"en": f[4], "ru": f[6], "ja": f[8]},
        })
    return works


def norm(s: str) -> str:
    s = unicodedata.normalize("NFKC", s).lower().replace("ё", "е")
    return re.sub(r"[\s\"'«»“”„’‘`.,:;!?()\[\]\-–—・＝=、。「」『』]+", "", s)


def name_like(want: str, name: str) -> bool:
    """A surname against a catalogue's name ('Dostoevsky' ~ 'Dostoyevsky, Fyodor'), spelling variants allowed."""
    import difflib
    n = norm(name)
    if want in n:
        return True
    first = norm(re.split(r"[,(]", name)[0])
    return difflib.SequenceMatcher(None, want, first).ratio() >= 0.8


def surname(name: str) -> str:
    parts = re.split(r"[\s・＝]+", name.strip())
    return parts[-1] if parts else name


# ------------------------------------------------------------------------------------------------------- Gutenberg (en)


_pg_rows: list[dict] | None = None


def pg_rows() -> list[dict]:
    """Project Gutenberg's catalogue (pg_catalog.csv): id, title, language, authors with their years and roles."""
    global _pg_rows
    if _pg_rows is None:
        import gzip
        data = get("https://gutenberg.pglaf.org/cache/epub/feeds/pg_catalog.csv.gz", binary=True)
        text = gzip.decompress(data).decode("utf-8")
        _pg_rows = [r for r in csv.DictReader(io.StringIO(text)) if r["Type"] == "Text"]
    return _pg_rows


def pg_people(field: str) -> list[dict]:
    """'Tolstoy, Leo, graf, 1828-1910; Maude, Aylmer, 1858-1938 [Translator]' -> names, death years, roles."""
    out = []
    for part in field.split(";"):
        part = part.strip()
        if not part:
            continue
        role = "author"
        m = re.search(r"\[(\w[\w ]*)\]\s*$", part)
        if m:
            role = m.group(1).lower()
            part = part[: m.start()].strip()
        died = None
        y = re.search(r"(?:(\d{3,4})\??\s*(?:BCE)?)?-(\d{3,4})\??\s*(BCE)?\s*$", part)
        if y:
            died = -int(y.group(2)) if y.group(3) else int(y.group(2))
            part = part[: y.start()].rstrip(", ")
        elif re.search(r"\d{3,4}\??-\s*$", part):
            died = 9999  # still alive when catalogued, or unknown: never taken
            part = re.sub(r",?\s*\d{3,4}\??-\s*$", "", part)
        out.append({"name": part, "died": died, "role": role})
    return out


PINS_PATH = Path(__file__).resolve().parent / "pins.json"
_pins: dict | None = None


def pins() -> dict:
    global _pins
    if _pins is None:
        _pins = json.loads(PINS_PATH.read_text(encoding="utf-8")) if PINS_PATH.exists() else {}
    return _pins


def pinned_en(w: dict, pin: dict) -> dict | None:
    by_id = {int(r["Text#"]): r for r in pg_rows()}
    ids = [i for i in pin["gutenberg"] if i in by_id]
    if not ids:
        return None
    creators: list[dict] = []
    for i in ids:
        for p in pg_people(by_id[i]["Authors"]):
            if p["role"] in ("author", "creator", "translator") and p not in creators:
                creators.append(p)
    if pin.get("translator"):
        creators.append({"name": pin["translator"], "role": "translator", "died": None})
    if any(p["died"] is not None and p["died"] > LAST_DEATH for p in creators):
        print(f"pin {w['id']}: a creator died after {LAST_DEATH}, skipped", flush=True)
        return None
    return {
        "source": "gutenberg", "gutenberg": ids[0], "gutenberg_parts": ids, "title_src": by_id[ids[0]]["Title"],
        "creators": creators, "license": "Public domain (Project Gutenberg)", "pinned": True,
        "url": f"https://www.gutenberg.org/ebooks/{ids[0]}",
    }


def resolve_en(w: dict) -> dict | None:
    pin = pins().get(w["id"], {}).get("en")
    if pin:
        return pinned_en(w, pin)
    title, author = w["title"]["en"], w["author"]["en"]
    want_t = norm(title)
    anon = author in ("Anonymous",)
    want_a = [norm(x) for x in re.split(r"\s+and\s+", author)] if not anon else []
    want_a = [norm(surname(a)) for a in re.split(r"\s+and\s+", author)] if not anon else []
    best = None
    for r in pg_rows():
        if r["Language"] != "en":
            continue
        t_full = norm(r["Title"])
        t_main = norm(re.split(r"[;:\n]|, or ", r["Title"])[0])
        if not (t_main == want_t or (t_full.startswith(want_t) and len(want_t) >= 8)):
            continue
        people = pg_people(r["Authors"])
        authors = [p for p in people if p["role"] in ("author", "creator")]
        if want_a and not any(any(name_like(a, p["name"]) for a in want_a) for p in authors):
            continue
        creators = [p for p in people if p["role"] in ("author", "creator", "translator")]
        if any(p["died"] is not None and p["died"] > LAST_DEATH for p in creators):
            continue
        translators = [p for p in people if p["role"] == "translator"]
        if w["orig"] not in ("en", "enm", "ang") and w["year"] >= 1850 and not translators:
            continue  # a modern work in English with no translator named: can't tell when its translator died
        shelves = r["Bookshelves"]
        score = (t_main == want_t) * 1000 + ("Best Books" in shelves) * 300 + ("Classics" in shelves) * 200 \
            + ("Harvard Classics" in shelves) * 100 - int(r["Text#"]) / 100000.0
        if not best or score > best[0]:
            best = (score, r, creators)
    if not best:
        return None
    _, r, creators = best
    return {
        "source": "gutenberg", "gutenberg": int(r["Text#"]), "title_src": r["Title"],
        "creators": creators, "license": "Public domain (Project Gutenberg)",
        "url": f"https://www.gutenberg.org/ebooks/{r['Text#']}",
    }


def gutenberg_text(gid: int) -> str | None:
    digits = str(gid)
    sub = "/".join(digits[:-1]) if len(digits) > 1 else "0"
    urls = [
        f"https://gutenberg.pglaf.org/{sub}/{gid}/{gid}-0.txt",
        f"https://gutenberg.pglaf.org/{sub}/{gid}/{gid}-8.txt",
        f"https://gutenberg.pglaf.org/{sub}/{gid}/{gid}.txt",
        f"https://www.gutenberg.org/cache/epub/{gid}/pg{gid}.txt",
    ]
    for u in urls:
        try:
            data = get(u, binary=True, delay=0.5)
        except Exception:
            data = None
        if not data or data[:15].lower().startswith(b"<!doctype html") or data[:6].lower() == b"<html>":
            continue
        if u.endswith("-8.txt"):
            return data.decode("latin-1")
        try:
            return data.decode("utf-8")
        except UnicodeDecodeError:
            return data.decode("latin-1")
    return None


HEAD_EN = re.compile(r"^(CHAPTER|Chapter|BOOK|Book|PART|Part|ACT|Act|SCENE|Scene|STAVE|Stave|CANTO|Canto|LETTER|Letter|VOLUME|Volume)\b"
                     r"|^[IVXLC]+\.?$|^[IVXLC]+\.\s+\S")


def clean_gutenberg(raw: str) -> str:
    raw = raw.replace("\r\n", "\n").replace("\r", "\n")
    s = re.search(r"^\*\*\* ?START OF (THE|THIS) PROJECT GUTENBERG[^*]*\*\*\*[^\n]*\n", raw, re.M | re.I)
    e = re.search(r"^\*\*\* ?END OF (THE|THIS) PROJECT GUTENBERG", raw, re.M | re.I)
    body = raw[s.end() if s else 0: e.start() if e else len(raw)]
    # the pictures of the printed book: "[Illustration: ...]" blocks, alone or inside a paragraph
    # (an illustrated edition may print the chapter's heading inside its picture: that line is kept)
    def _illus(m: re.Match) -> str:
        heads = re.findall(r"^\s*((?:CHAPTER|Chapter|BOOK|Book|PART|Part)\s+[IVXLC\d]+\.?[^\n\]]*)", m.group(0), re.M)
        return "\n\n" + "\n\n".join(h.strip() for h in heads) + "\n\n" if heads else ""
    body = re.sub(r"\[Illustration[^\]]*\]", _illus, body, flags=re.I | re.S)
    body = re.sub(r"^\s*(Produced by|This etext was|E-text prepared by|Transcriber's Note)[^\n]*(\n[^\n]+)*\n", "", body, flags=re.I)
    out = []
    for para in re.split(r"\n\s*\n", body):
        lines = [l.rstrip() for l in para.split("\n") if l.strip()]
        if not lines:
            continue
        stripped = [l.strip() for l in lines]
        text = " ".join(stripped)
        text = re.sub(r"_([^_]+)_", r"\1", text)
        if len(lines) == 1 and len(text) < 70 and HEAD_EN.match(text):
            out.append("# " + text)
            continue
        if len(lines) == 2 and len(stripped[0]) < 30 and len(stripped[1]) < 70 and HEAD_EN.match(stripped[0]):
            out.append("# " + stripped[0] + " " + stripped[1])  # "CHAPTER I." / "The Period"
            continue
        short = sum(1 for l in stripped if len(l) < 52)
        if len(lines) >= 2 and short == len(lines) and max(len(l) for l in stripped) < 60:
            # verse (or a letter's address): the lines stay as they are
            out.append("\n".join(re.sub(r"_([^_]+)_", r"\1", l) for l in stripped))
        else:
            out.append(text)
    text = "\n\n".join(out).strip() + "\n"
    # title pages, publishers' lines and the contents list before chapter one: the reader shows its own title page.
    # The first "Chapter I" heading in the first 15% that a paragraph of prose follows is the real one (a contents list
    # may name it first, with only more headings after it).
    for m in re.finditer(r"^# (CHAPTER|Chapter) (I|1|ONE|One)\b[^\n]*\n\n([^\n]*)", text, re.M):
        if m.start() > len(text) * 0.15:
            break
        if len(m.group(3)) > 60 and not m.group(3).startswith("#"):
            # ...unless real text comes before it (Frankenstein's letters): a heading with prose under it
            pre = text[:m.start()]
            h = re.search(r"^# ", pre, re.M)
            if not (h and re.search(r"^[^#\n][^\n]{300}", pre[h.start():], re.M)):
                text = text[m.start():]
            break
    return text


# --------------------------------------------------------------------------------------------------------- Aozora (ja)

_aozora_rows: list[dict] | None = None


def aozora_rows() -> list[dict]:
    global _aozora_rows
    if _aozora_rows is None:
        data = get("https://www.aozora.gr.jp/index_pages/list_person_all_extended_utf8.zip", binary=True)
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            name = [n for n in z.namelist() if n.endswith(".csv")][0]
            text = z.read(name).decode("utf-8-sig")
        _aozora_rows = list(csv.DictReader(io.StringIO(text)))
    return _aozora_rows


def _year(date: str) -> int | None:
    m = re.match(r"(\d{4})", date or "")
    return int(m.group(1)) if m else None


def resolve_ja(w: dict) -> dict | None:
    rows = aozora_rows()
    works: dict[str, list[dict]] = {}
    for r in rows:
        works.setdefault(r["作品ID"], []).append(r)
    want_t = norm(w["title"]["ja"])
    want_a = w["author"]["ja"]
    a_tokens = [norm(t) for t in re.split(r"[・＝、\s]", want_a) if len(t) >= 2] or [norm(want_a)]
    found = []
    for wid, rs in works.items():
        t = norm(rs[0]["作品名"])
        if not t.startswith(want_t):
            continue
        if rs[0]["作品著作権フラグ"] != "なし":
            continue
        authors = [r for r in rs if r["役割フラグ"] == "著者"]
        if want_a not in ("作者不詳",):
            names = [norm(r["姓"] + r["名"]) for r in authors]
            if not any(any(tok in n or n in tok for tok in a_tokens) for n in names):
                continue
        people = [r for r in rs if r["役割フラグ"] in ("著者", "翻訳者")]
        deaths = [_year(r["没年月日"]) for r in people]
        if any(d is None and r["役割フラグ"] == "翻訳者" for d, r in zip(deaths, people)):
            continue
        if any(d is not None and d > LAST_DEATH for d in deaths):
            continue
        if not rs[0]["テキストファイルURL"].endswith(".zip"):
            continue
        found.append((wid, rs))
    if not found:
        return None
    exact = [f for f in found if norm(f[1][0]["作品名"]) == want_t and not f[1][0]["副題"]]
    if exact:
        # one text: the modern spelling first, then the newest
        exact.sort(key=lambda f: (f[1][0]["文字遣い種別"] != "新字新仮名", -int(f[0])))
        parts = [exact[0]]
    else:
        # a work in several parts (源氏物語 桐壺, 帚木...): every part by the same translator, in the catalogue's order
        by_tr: dict[str, list] = {}
        for f in found:
            tr = "".join(r["姓"] + r["名"] for r in f[1] if r["役割フラグ"] == "翻訳者")
            by_tr.setdefault(tr, []).append(f)
        parts = max(by_tr.values(), key=len)
        parts.sort(key=lambda f: int(f[0]))
        if len(parts) > 80:
            return None
    head = parts[0][1]
    creators = []
    for r in head:
        if r["役割フラグ"] in ("著者", "翻訳者"):
            creators.append({"name": r["姓"] + r["名"], "died": _year(r["没年月日"]),
                             "role": "author" if r["役割フラグ"] == "著者" else "translator"})
    return {
        "source": "aozora",
        "aozora": [{"work": wid, "title": rs[0]["作品名"] + (" " + rs[0]["副題"] if rs[0]["副題"] else ""),
                    "zip": rs[0]["テキストファイルURL"]} for wid, rs in parts],
        "creators": creators, "license": "Public domain (Aozora Bunko, 著作権なし)",
        "url": head[0]["図書カードURL"],
    }


def aozora_text(zip_url: str) -> str | None:
    data = get(zip_url, binary=True, delay=0.5)
    if not data:
        return None
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        names = [n for n in z.namelist() if n.lower().endswith(".txt")]
        if not names:
            return None
        raw = z.read(names[0])
    for enc in ("cp932", "shift_jis", "utf-8"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("cp932", "replace")


def clean_aozora(raw: str, part_title: str | None = None) -> str:
    raw = raw.replace("\r\n", "\n")
    lines = raw.split("\n")
    # the header: title, author, then a block between two rows of dashes (the notation's legend)
    dash = [i for i, l in enumerate(lines[:80]) if re.match(r"^-{20,}", l)]
    start = dash[1] + 1 if len(dash) >= 2 else 0
    if not dash:
        # no legend: the title and author lines go
        start = 0
        while start < len(lines) and start < 4 and lines[start].strip():
            start += 1
    end = len(lines)
    for i in range(len(lines) - 1, max(start, len(lines) - 120), -1):
        if lines[i].startswith("底本："):
            end = i
            break
    out = []
    if part_title:
        out.append("# " + part_title)
    para_break = False
    for l in lines[start:end]:
        l = l.rstrip()
        head = re.search(r"［＃「(.+?)」は(大|中|小)見出し］", l)
        l = re.sub(r"｜", "", l)
        l = re.sub(r"《[^》]*》", "", l)
        l = re.sub(r"※［＃[^］]*］", "〓", l)
        l = re.sub(r"［＃[^］]*］", "", l)
        l = l.replace("〓", "")
        if head:
            out.append("# " + re.sub(r"《[^》]*》|｜", "", head.group(1)))
            continue
        if not l.strip():
            para_break = True
            continue
        out.append(l.strip("　 ") if l.startswith("　") else l)
        para_break = False
    # every line of an Aozora text is a paragraph (verse lines too): one per paragraph, a heading on its own
    text = "\n\n".join(x for x in out if x)
    return text.strip() + "\n"


# --------------------------------------------------------------------------------------------------- Wikisource (ru)

WS = "https://ru.wikisource.org"


def ws_raw(title: str) -> str | None:
    url = f"{WS}/w/index.php?" + urllib.parse.urlencode({"title": title, "action": "raw"})
    return get(url, delay=1.0)


def ws_render(title: str) -> str | None:
    url = f"{WS}/w/index.php?" + urllib.parse.urlencode({"title": title, "action": "render"})
    return get(url, delay=1.0)


def ws_search(query: str) -> list[str]:
    url = f"{WS}/w/index.php?" + urllib.parse.urlencode(
        {"search": query, "title": "Служебная:Поиск", "profile": "default", "fulltext": "1", "ns0": "1"})
    h = get(url, delay=1.5) or ""
    return [html.unescape(t) for t in re.findall(r'mw-search-result-heading[^>]*><a href="[^"]+" title="([^"]+)"', h)]


def ws_field(wikitext: str, name: str) -> str:
    m = re.search(r"\|\s*" + name + r"\s*=\s*([^\n|]*)", wikitext)
    return m.group(1).strip() if m else ""


def ws_years(field: str) -> int | None:
    m = re.search(r"\(\s*\d{3,4}\s*[—–-]\s*(\d{4})\s*\)", field)
    return int(m.group(1)) if m else None


_deaths: dict[str, int | None] = {}


def ws_death(field: str) -> int | None:
    """The death year of a creator named in a header field: "(1836—1902)" in the field itself, else the "ум. ..." line of
    his Автор: page (dates there come from Wikidata, so only the rendered page has them)."""
    y = ws_years(field)
    if y:
        return y
    m = re.search(r"\[\[(?:Автор:)?([^\]|]+)", field)
    name = (m.group(1) if m else re.sub(r"\(.*?\)", "", field)).strip().lstrip("~").strip()
    if not name or len(name) < 4 or re.fullmatch(r"[\d\s.]+", name):
        return None
    if name not in _deaths:
        h = ws_render("Автор:" + name) or ""
        t = re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", " ", h)))
        d = re.search(r"\bум\.(.{0,60})", t)
        y4 = re.search(r"\b(1[0-9]{3})\b", d.group(1)) if d else None
        _deaths[name] = int(y4.group(1)) if y4 else None
    return _deaths[name]


def ws_follow(title: str) -> tuple[str, str] | None:
    for _ in range(3):
        raw = ws_raw(title)
        if raw is None:
            return None
        m = re.match(r"\s*#(перенаправление|redirect)\s*\[\[([^\]|#]+)", raw, re.I)
        if not m:
            return title, raw
        title = m.group(2).strip()
    return None


def resolve_ru(w: dict) -> dict | None:
    title = w["title"]["ru"]
    sur = surname(w["author"]["ru"])
    folk = w["author"]["ru"] in ("Народный эпос", "Народные сказки", "Неизвестный автор")
    if folk:
        sur = ""
    translated = w["orig"] not in ("ru", "orv")
    cands = [f"{title} ({sur})"] if sur else []
    cands.append(title)
    for t in ws_search(f"{title} {sur}".strip())[:15]:
        root = t.split("/")[0]
        if root not in cands:
            cands.append(root)
    want = norm(title)
    best = None
    tried = 0
    for cand in cands:
        head = cand.split(" (")[0]
        inside = cand.split("(", 1)[1] if "(" in cand else ""
        if not norm(head).startswith(want[: max(4, len(want) - 2)]):
            continue
        if sur and inside and not name_like(norm(sur), inside.split(";")[0].split(",")[0]):
            continue
        if "," in inside.split(";")[0]:
            continue  # "(Шекспир, Лэм)": somebody's retelling
        if tried >= 8:
            break
        tried += 1
        got = ws_follow(cand)
        if not got:
            continue
        page, raw = got
        low = raw.lower()
        if "{{неоднозначность" in low or "{{disambig" in low or "{{другие значения" in low and len(raw) < 1500:
            continue
        lic = ws_field(raw, "ЛИЦЕНЗИЯ")
        author_f = ws_field(raw, "АВТОР")
        tr_f = ws_field(raw, "ПЕРЕВОДЧИК")
        if not tr_f and ";" in page:
            tr_f = page.split(";", 1)[1].rstrip(")").strip()
        if sur and author_f and norm(sur)[:5] not in norm(author_f):
            continue  # another author's book of the same title (Petrarch's and Heine's "Книга песен")
        a_died = ws_years(author_f)
        t_died = ws_death(tr_f) if tr_f else None
        if a_died and a_died > LAST_DEATH or t_died and t_died > LAST_DEATH:
            continue
        if lic and not lic.upper().startswith("PD"):
            continue
        if translated and not tr_f:
            continue  # a translation with no translator named: could be anybody's, any year
        if translated and (t_died is None or norm(surname(tr_f)) == norm(sur) or tr_f.strip().lower() in ("нет", "")):
            continue  # every translator's death year must be known (1955 rule); "нет" or the author himself: an original
        if not lic and not (a_died or w["year"] < 1880) and not (translated and t_died):
            continue
        # the text itself must be on Wikisource: a page of just links to other translations is no text
        if "{{отексте" not in low and "пск" not in low and not re.search(r"\{\{[^}|]*(ПСС|Отексте)", raw) and len(raw) < 2500:
            continue
        score = (norm(head) == want) * 10 + (t_died is not None) * 3 + (lic != "") * 2 + ("ДО" not in page) * 1
        if not best or score > best[0]:
            best = (score, page, author_f, tr_f, a_died, t_died, lic)
    if not best:
        return None
    _, page, author_f, tr_f, a_died, t_died, lic = best

    def clean(f, fallback):
        n = re.sub(r"\(.*?\)", "", f)
        m = re.search(r"\[\[([^\]|]+)(?:\|([^\]]+))?\]\]", n)
        return re.sub(r"^(\[\[)?(Автор:)?|\]\]$", "", (m.group(1) if m else n).strip()).strip(" ~") or fallback

    creators = [{"name": clean(author_f, w["author"]["ru"]), "died": a_died, "role": "author"}]
    if tr_f:
        creators.append({"name": clean(tr_f, tr_f), "died": t_died, "role": "translator"})
    return {"source": "wikisource", "wikisource": page, "creators": creators,
            "license": lic or "PD-old (Wikisource)",
            "url": f"{WS}/wiki/" + urllib.parse.quote(page.replace(" ", "_"))}


class _WsText(html.parser.HTMLParser):
    """The reading text of a rendered Wikisource page: paragraphs, verse, headings; links to its own subpages."""

    SKIP_CLASSES = ("ws-noexport", "noprint", "navbox", "reference", "references", "mw-editsection", "ws-header",
                    "header_", "headertemplate", "mw-references-wrap", "metadata", "noexport", "toc", "sister")

    def __init__(self, root: str):
        super().__init__(convert_charrefs=True)
        self.root = root
        self.out: list[str] = []
        self.links: list[str] = []
        self.buf: list[str] = []
        self.skip = 0
        self.stack: list[tuple[str, bool]] = []
        self.mode = None  # "p", "h", "poem", "pre"
        self.htag = ""

    def _skipping(self, tag, attrs) -> bool:
        a = dict(attrs)
        cls = a.get("class", "") or ""
        if tag in ("table", "style", "script", "sup") or a.get("id") in ("toc", "headertemplate"):
            return True
        return any(c in cls for c in self.SKIP_CLASSES)

    def handle_starttag(self, tag, attrs):
        if tag in self.VOID:
            if tag == "br" and not self.skip and self.mode:
                self.buf.append("\n")
            return
        sk = self._skipping(tag, attrs)
        self.stack.append((tag, sk))
        if sk:
            self.skip += 1
            return
        if self.skip:
            return
        a = dict(attrs)
        if tag == "a":
            href = a.get("href", "")
            m = re.match(r"(?:https://ru\.wikisource\.org)?/wiki/([^#?]+)", href)
            if m:
                t = urllib.parse.unquote(m.group(1)).replace("_", " ")
                if t.startswith(self.root + "/") and t not in self.links:
                    self.links.append(t)
        if tag in ("p", "dd") and self.mode is None:
            self.mode = "p"
            self.buf = []
        elif (tag in ("h2", "h3", "h4", "h5", "center") or (tag == "div" and a.get("role") == "heading")) and self.mode is None:
            # chapter numbers come as <h3>, <div role="heading"> or a centred "I."
            self.mode = "h"
            self.htag = tag
            self.buf = []
        elif tag == "pre" and self.mode is None:
            self.mode = "pre"
            self.buf = []
        elif tag == "div" and "poem" in (a.get("class") or "") and self.mode is None:
            self.mode = "poem"
            self.buf = []

    VOID = ("br", "img", "hr", "wbr", "meta", "link", "input", "source", "col", "area", "base", "embed", "param", "track")

    def handle_endtag(self, tag):
        if tag in self.VOID or not any(t == tag for t, _ in self.stack):
            return
        while self.stack:
            t, sk = self.stack.pop()
            if sk:
                self.skip -= 1
            if t == tag:
                break
        if self.skip:
            return
        if (self.mode == "p" and tag in ("p", "dd")) or (self.mode == "h" and tag == self.htag) \
                or (self.mode == "poem" and tag == "div") or (self.mode == "pre" and tag == "pre"):
            text = "".join(self.buf)
            if self.mode in ("poem", "pre"):
                lines = [re.sub(r"\s+", " ", l).strip() for l in text.split("\n")]
                stanzas, cur = [], []
                for l in lines:
                    if l:
                        cur.append(l)
                    elif cur:
                        stanzas.append("\n".join(cur))
                        cur = []
                if cur:
                    stanzas.append("\n".join(cur))
                self.out.extend(stanzas)
            else:
                text = re.sub(r"[ \t ]+", " ", text)
                text = "\n".join(l.strip() for l in text.split("\n")).strip()
                if text:
                    heading = self.mode == "h" and len(text) <= 80  # a long centred block is a paragraph (an epigraph)
                    self.out.append(("# " + text.replace("\n", " ")) if heading else text)
            self.mode = None
            self.buf = []

    def handle_data(self, data):
        if self.skip or not self.mode:
            return
        self.buf.append(data if self.mode in ("poem", "pre") else data.replace("\n", " "))


def ws_text(page: str, depth: int = 0, seen: set | None = None, budget: list | None = None) -> list[str]:
    seen = seen if seen is not None else set()
    budget = budget if budget is not None else [500]
    if page in seen or budget[0] <= 0:
        return []
    seen.add(page)
    budget[0] -= 1
    h = ws_render(page)
    if not h:
        return []
    p = _WsText(page)
    p.feed(h)
    links = p.links
    # editions: a text in modern spelling (ВТ, ВТ:Ё) over the pre-reform one (ДО)
    eds = [l for l in links if l.count("/") == page.count("/") + 1
           and re.search(r"/(ВТ|ДО|ВТ:Ё|СО)$|\((ВТ|ДО|ВТ:Ё|СО|ВТ:Ъ)\)$", l)]
    if eds:
        def rank(l):
            code = re.search(r"(ВТ:Ё|ВТ:Ъ|ВТ|ДО|СО)\)?$", l).group(1)
            return {"ВТ:Ё": 0, "ВТ": 1, "СО": 2, "ВТ:Ъ": 4, "ДО": 5}.get(code, 3)
        for pick in sorted(eds, key=rank):
            got = ws_text(pick, depth, seen, budget)
            if sum(len(x) for x in got) > 2000:
                return got
        return []
    out = list(p.out)
    kids = [l for l in links if l.count("/") == page.count("/") + 1]
    if kids and depth < 3:
        # a contents page: its own words are the contents list's (dropped: lists are not read) and a preface at most
        if sum(len(x) for x in out) < 3000:
            out = [x for x in out if not x.startswith("# ")]
        for k in kids:
            name = k.rsplit("/", 1)[1]
            sub = ws_text(k, depth + 1, seen, budget)
            if sub:
                if not (sub[0].startswith("# ")):
                    out.append("# " + name)
                out.extend(sub)
    return out


# ------------------------------------------------------------------------------------------------------------- resolve


def cmd_resolve(ids: list[str], langs: list[str]):
    works = read_canon()
    cat = json.loads(CATALOG.read_text(encoding="utf-8")) if CATALOG.exists() else {"works": {}}
    todo = [w for w in works if not ids or w["id"] in ids]
    fns = {"en": resolve_en, "ja": resolve_ja, "ru": resolve_ru}

    def one(w, lang):
        try:
            return w, lang, fns[lang](w), None
        except Exception as e:  # noqa: BLE001 - one work's trouble never stops the run
            return w, lang, None, repr(e)

    jobs = [(w, l) for w in todo for l in langs]
    # one worker per source host: each keeps its own one-request-a-second pace
    with cf.ThreadPoolExecutor(max_workers=3) as ex:
        futs = []
        for lang in langs:
            futs.append(ex.submit(lambda L=lang: [one(w, L) for w, l in jobs if l == L]))
        for f in cf.as_completed(futs):
            for w, lang, res, err in f.result():
                entry = cat["works"].setdefault(w["id"], {})
                entry["canon"] = {k: w[k] for k in ("orig", "year", "author", "title")}
                texts = entry.setdefault("texts", {})
                if res:
                    texts[lang] = res
                else:
                    texts.pop(lang, None)
                print(f"{w['id']:22s} {lang} {'+' if res else '-'} {res.get('gutenberg') or res.get('wikisource') or (res.get('aozora') or [{}])[0].get('title') if res else (err or '')}", flush=True)
                CATALOG.write_text(json.dumps(cat, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    cmd_report()


def cmd_report():
    cat = json.loads(CATALOG.read_text(encoding="utf-8"))
    n = {l: 0 for l in LANGS}
    for e in cat["works"].values():
        for l in e.get("texts", {}):
            n[l] += 1
    any_ = sum(1 for e in cat["works"].values() if e.get("texts"))
    print(f"works {len(cat['works'])}, with any text {any_}: " + ", ".join(f"{l} {n[l]}" for l in LANGS))


# --------------------------------------------------------------------------------------------------------------- fetch


def tidy(text: str) -> str:
    text = unicodedata.normalize("NFC", text)
    text = text.replace("­", "").replace("﻿", "")
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip() + "\n"


HEAD_RU = re.compile(r"^(ГЛАВА|Глава|ЧАСТЬ|Часть|КНИГА|Книга|ТОМ|Том|ДЕЙСТВИЕ|Действие|ДЕЙСТВИЕ|ЯВЛЕНИЕ|Явление|СЦЕНА|Сцена|"
                     r"ПЕСНЬ|Песнь|ЭПИЛОГ|Эпилог|ПРОЛОГ|Пролог|ПИСЬМО|Письмо)\b[^\n]{0,60}$|^[IVXLC]+\.?$|^\d{1,3}\.?$")


def ru_heading(p: str) -> str:
    """A chapter line Wikisource printed as an ordinary paragraph ("ГЛАВА ПЕРВАЯ", "XII.") becomes a heading."""
    return "# " + p if not p.startswith("# ") and "\n" not in p and HEAD_RU.match(p.strip()) else p


def fetch_one(wid: str, lang: str, t: dict) -> tuple[str | None, str]:
    if t["source"] == "gutenberg":
        parts = []
        for gid in t.get("gutenberg_parts") or [t["gutenberg"]]:
            raw = gutenberg_text(gid)
            if raw:
                parts.append(clean_gutenberg(raw))
        return ("\n\n".join(parts) if parts else None), "gutenberg"
    if t["source"] == "aozora":
        parts = []
        multi = len(t["aozora"]) > 1
        for p in t["aozora"]:
            raw = aozora_text(p["zip"])
            if raw:
                parts.append(clean_aozora(raw, p["title"] if multi else None))
        return ("\n\n".join(parts) if parts else None), "aozora"
    if t["source"] == "wikisource":
        paras = [ru_heading(p) for p in ws_text(t["wikisource"])]
        paras = [p for p in paras if p and not (p.startswith("# ") and not re.search(r"\w", p[2:]))]  # "# ————"
        return ("\n\n".join(paras) if paras else None), "wikisource"
    return None, "?"


def cmd_fetch(ids: list[str], langs: list[str], out: Path):
    cat = json.loads(CATALOG.read_text(encoding="utf-8"))
    index_path = out / "books.json"
    index = json.loads(index_path.read_text(encoding="utf-8")) if index_path.exists() else {"books": {}}
    jobs = []
    for wid, e in sorted(cat["works"].items()):
        if ids and wid not in ids:
            continue
        for lang, t in e.get("texts", {}).items():
            if lang in langs:
                jobs.append((wid, lang, t, e["canon"]))

    def run(job):
        wid, lang, t, canon = job
        try:
            text, src = fetch_one(wid, lang, t)
        except Exception as ex:  # noqa: BLE001
            return job, None, repr(ex)
        return job, text, None

    by_src: dict[str, list] = {}
    for j in jobs:
        by_src.setdefault(j[2]["source"], []).append(j)
    with cf.ThreadPoolExecutor(max_workers=3) as ex:
        futs = [ex.submit(lambda js=js: [run(j) for j in js]) for js in by_src.values()]
        for f in cf.as_completed(futs):
            for (wid, lang, t, canon), text, err in f.result():
                key = f"{lang}/{wid}"
                if not text or len(text) < 400:
                    print(f"{key:28s} FAILED {err or 'too short'}", flush=True)
                    index["books"].pop(key, None)
                    continue
                text = tidy(text)[:MAX_CHARS]
                path = out / lang / f"{wid}.txt"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
                index["books"][key] = {
                    "id": wid, "lang": lang, "file": f"{lang}/{wid}.txt",
                    "title": canon["title"], "author": canon["author"], "year": canon["year"], "orig": canon["orig"],
                    "creators": t["creators"], "license": t["license"], "source": t["url"],
                    "chars": len(text), "sha256": hashlib.sha256(text.encode("utf-8")).hexdigest(),
                }
                print(f"{key:28s} {len(text):>9,d} chars", flush=True)
                index_path.parent.mkdir(parents=True, exist_ok=True)
                index_path.write_text(json.dumps(index, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    total = sum(b["chars"] for b in index["books"].values())
    print(f"books.json: {len(index['books'])} texts, {total / 1e6:.1f} M chars")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=("resolve", "fetch", "report"))
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--lang", default=",".join(LANGS), help="ru,en,ja")
    ap.add_argument("--out", default=str(OUT))
    a = ap.parse_args()
    langs = [l for l in a.lang.split(",") if l in LANGS]
    if a.cmd == "resolve":
        cmd_resolve(a.ids, langs)
    elif a.cmd == "fetch":
        cmd_fetch(a.ids, langs, Path(a.out))
    else:
        cmd_report()


if __name__ == "__main__":
    main()
