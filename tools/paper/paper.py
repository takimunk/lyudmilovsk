#!/usr/bin/env python3
"""The town paper «Людмиловская правда» and the dated notices of the boards (docs/town_paper.md).

Reads tools/paper/strings_src.py (every word, ru/en/ja), Content/PeopleData/townsfolk.json (birthdays, anniversaries),
Content/City/fair_goods.json (the fair's prices), Content/BirdsData/species.json and writes:

    unreal/Lyudmilovsk/Content/City/town_paper.json           the paper's data (ULyudTownPaper, LyudPaper::FPaper)
    unreal/Lyudmilovsk/Content/Localization/paper/strings_<lang>.json   its words (a sub-table LyudText loads by itself)
    tools/city/content/x_notices.json                         the boards' notice texts (dated pages), for interiors.py
    tools/city/content/strings/x_notices.json                 their words

    python3 tools/paper/paper.py           write everything (then run tools/city/interiors.py)
    python3 tools/paper/paper.py --check   write nothing; fail if a file would change

Stdlib only, deterministic (birthdays from a hash of the resident's key, not from random).
"""
import collections
import datetime
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
sys.path.insert(0, HERE)
import strings_src as src  # noqa: E402

OUT_DATA = os.path.join(CONTENT, "City", "town_paper.json")
OUT_STRINGS = os.path.join(CONTENT, "Localization", "paper")
OUT_NOTICES = os.path.join(ROOT, "tools", "city", "content", "x_notices.json")
OUT_NOTICE_STRINGS = os.path.join(ROOT, "tools", "city", "content", "strings", "x_notices.json")
LANGS = ("ru", "en", "ja")
START_YEAR = 2026          # UGameCalendar::StartDate (2026-10-20): ages in townsfolk.json are as of then
START = datetime.date(2026, 10, 20)   # UGameCalendar::StartDate
FIRST_ISSUE = 12001        # the number of the first game day's issue
RARE_BIRDS = ["Waxwing", "Bullfinch", "Sparrowhawk", "Peregrine", "WhiteTailedEagle", "GoldenEagle", "Raven", "GreyHeron",
              "BlackBrowedAlbatross", "Nutcracker", "MuteSwan", "Jay", "Fieldfare", "CommonBuzzard"]
# the fair's goods the paper prices (fair_goods.json gives the price), with the months they are about
MARKET = {"apple": [8, 9, 10, 11], "kefir": None, "bread_loaf": None, "pie": None, "cheese_braid": None,
          "cheese_round_small": None, "corn_cob": [7, 8, 9], "ice_cream": [5, 6, 7, 8, 9], "tea": [10, 11, 12, 1, 2, 3, 4],
          "coffee": None, "cookies": None, "seeds": None, "feed_peanuts": [10, 11, 12, 1, 2, 3], "candy": None,
          "drink_baikal": [5, 6, 7, 8], "drink_tarkhun": [5, 6, 7, 8], "chocolate": None, "water": [6, 7, 8]}
CINEMA = ["city.rassvet.poster.%s" % d for d in ("mon", "tue", "wed", "thu", "fri", "sat", "sun", "fest")]
# the newsreel of real old films before every session (tools/films/films.py writes these strings, docs/cinema.md)
NEWSREEL = ["film.programme.%s" % d for d in ("mon", "tue", "wed", "thu", "fri", "sat", "sun", "fest")]
BOARDS = {   # board -> (title ru/en/ja, the board's undated lines, its festival lines)
    "lost": (("Доска объявлений", "Notice board", "掲示板"), ["city.look.lost.%d" % i for i in range(1, 6)], ["city.fest.lost.1"]),
    "club": (("Клубная доска", "Club notice board", "クラブの掲示板"), ["city.look.club.%d" % i for i in range(1, 6)], ["city.fest.club.1"]),
    "health": (("Доска здоровья", "Health board", "健康の掲示板"), ["city.look.health.%d" % i for i in range(1, 6)], ["city.fest.health.1"]),
    "school": (("Стенгазета", "Wall newspaper", "壁新聞"), ["city.look.wallpaper.%d" % i for i in range(1, 6)], ["city.fest.wallpaper.1"]),
}


def h32(s):
    return int(hashlib.md5(s.encode("utf-8")).hexdigest()[:8], 16)


def md_of_day(doy):
    days = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    m = 0
    while doy >= days[m]:
        doy -= days[m]
        m += 1
    return "%02d-%02d" % (m + 1, doy + 1)


def strings_tables():
    t = {l: collections.OrderedDict() for l in LANGS}

    def put(key, ru, en, ja):
        for l, v in zip(LANGS, (ru, en, ja)):
            if key in t[l]:
                sys.exit("paper.py: key %s twice" % key)
            t[l][key] = v

    for key, ru, en, ja in src.S:
        put(key, ru, en, ja)
    for key, ru, en, ja in src.SAIL:
        put(key, ru, en, ja)
    for i, (ru, en, ja) in enumerate(src.MONTHS, 1):
        put("paper.month.gen.%d" % i, ru, en, ja)
    for i, (ru, en, ja) in enumerate(src.DOWS):
        put("paper.dow.%d" % i, ru, en, ja)
    for i, (ru, en, ja) in enumerate(src.DIRS):
        put("paper.dir.%d" % i, ru, en, ja)
    for fid in sorted(src.FEST):
        for n, (ru, en, ja) in enumerate(src.FEST[fid], 1):
            put("paper.fest.%s.%d" % (fid, n), ru, en, ja)
    for n, (ru, en, ja, _m) in enumerate(src.FRONTS, 1):
        put("paper.story.%d" % n, ru, en, ja)
    for n, (ru, en, ja, _m) in enumerate(src.TOWN, 1):
        put("paper.town.%d" % n, ru, en, ja)
    for n, (ru, en, ja, _m) in enumerate(src.ADS, 1):
        put("paper.ad.%d" % n, ru, en, ja)
    return t


def pool(prefix, rows):
    out = []
    for n, row in enumerate(rows, 1):
        months = row[3]
        out.append({"k": "%s.%d" % (prefix, n), "months": months} if months else "%s.%d" % (prefix, n))
    return out


def variants(tables):
    v = collections.OrderedDict()
    for key in tables["ru"]:
        parts = key.split(".")
        if len(parts) >= 3 and parts[0] == "paper" and parts[1] in ("front", "fest", "sail") and parts[-1].isdigit():
            kind = ".".join(parts[1:-1])
            v[kind] = max(v.get(kind, 0), int(parts[-1]))
    return v


def residents():
    folk = json.load(open(os.path.join(CONTENT, "PeopleData", "townsfolk.json"), encoding="utf-8"))["people"]
    births, anniv = [], []
    for p in sorted(folk, key=lambda p: p["key"]):
        md = md_of_day(h32("bday:" + p["key"]) % 365)
        births.append({"who": p["key"], "md": md, "born": START_YEAR - int(p["age"]) - (1 if md > "10-20" else 0)})
    households = collections.defaultdict(list)
    for p in folk:
        if p["age"] >= 22:
            households[p.get("household", "")].append(p)
    for hh in sorted(households):
        adults = sorted(households[hh], key=lambda p: -p["age"])
        if not hh or len(adults) < 2:
            continue
        a, b = adults[0], adults[1]
        if a["female"] == b["female"] or abs(a["age"] - b["age"]) > 12:
            continue
        younger = min(a["age"], b["age"])
        years = max(1, min(younger - 21, 1 + h32("wed:" + hh) % max(1, younger - 21)))
        anniv.append({"a": a["key"], "b": b["key"], "md": md_of_day(h32("wedday:" + hh) % 365), "year": START_YEAR - years})
    return births, anniv


def market():
    stalls = json.load(open(os.path.join(CONTENT, "City", "fair_goods.json"), encoding="utf-8"))["stalls"]
    price = {}
    for sid in sorted(stalls):
        for g in stalls[sid]["goods"]:
            price.setdefault(g["item"], g["price"])
    out = []
    for item in MARKET:
        if item in price:
            rec = {"item": item, "price": price[item]}
            if MARKET[item]:
                rec["months"] = MARKET[item]
            out.append(rec)
    return out


def notices():
    """x_notices.json: a text per board, a holiday page, the dated notices packed into pages, then the board's own lines
    (one a day): a page's alternatives never overlap in date, so a page shows one notice at a time or none."""
    texts = collections.OrderedDict()
    strings = collections.OrderedDict()
    strings["_doc"] = "Words of the boards' dated notices; written by tools/paper/paper.py from tools/paper/strings_src.py NOTICES. Never edit by hand."
    acts = collections.OrderedDict()
    for board, (title, base, fest) in BOARDS.items():
        tkey = "city.notices.%s.title" % board
        strings[tkey] = dict(zip(LANGS, title))
        slots = []   # each: list of (entry, ranges)
        for nb, nid, window, first, ru, en, ja in src.NOTICES:
            if nb != board:
                continue
            key = "city.note.%s.%s" % (board, nid)
            strings[key] = {"ru": ru, "en": en, "ja": ja}
            if window:
                # one window, or several (a notice that comes back: the photo contest's by the month): an entry for each
                windows = [tuple(w) for w in window] if isinstance(window[0], (list, tuple)) else [tuple(window)]
                entries = [{"k": key, "date": list(w)} for w in windows]
                span = ("d", windows)
            else:
                entries = [{"k": key, "after": first[0], "before": first[1]}]
                # as dates of the first year, so it never shares a page with a dated notice on those days
                span = ("d", [tuple((START + datetime.timedelta(days=n)).strftime("%m-%d") for n in (first[0], first[1] - 1))])
            placed = False
            for slot in slots:
                if not any(overlap(span, s2) for _e, s2 in slot):
                    slot.append((entries, span))
                    placed = True
                    break
            if not placed:
                slots.append([(entries, span)])
        pages = [[{"k": f, "festival": "any"} for f in fest]]
        pages += [[e for es, _s in slot for e in es] for slot in slots]
        pages.append(list(base))
        if len(pages) > 12:
            sys.exit("paper.py: board %s needs %d pages (12 at most)" % (board, len(pages)))
        texts["notices_" + board] = {"title": tkey, "kind": "notice", "minutes": 1, "pages": pages}
    # the photo circle's own sheet at «Шестерня» (the lab's board, P-CLUB3/photo_board): the contest's notice of today (the
    # same notices as on the town's club boards: the month's theme, the reminder, the jury's day, the results) and its rules
    ptitle, prules = src.CONTEST_SHEET
    strings["city.notices.photo.title"] = dict(zip(LANGS, ptitle))
    strings["city.note.photo.rules"] = dict(zip(LANGS, prules))
    today = []
    for nb, nid, window, _first, _ru, _en, _ja in src.NOTICES:
        if nb == "club" and nid.startswith("contest_"):
            today += [{"k": "city.note.club.%s" % nid, "date": list(w)} for w in window]
    for i, a in enumerate(today):
        for b in today[i + 1:]:
            if overlap(("d", [tuple(a["date"])]), ("d", [tuple(b["date"])])):
                sys.exit("paper.py: the contest's notices %s and %s share a day" % (a["k"], b["k"]))
    texts["notices_photo"] = {"title": "city.notices.photo.title", "kind": "notice", "minutes": 1, "pages": [today, "city.note.photo.rules"]}
    return {"_doc": "The notice boards' texts (docs/town_paper.md \"Notices\"): written by tools/paper/paper.py, never edit by hand. "
                    "The board acts read them (acts.json: read notices_<board>): a holiday page; dated notices, each page holding "
                    "notices whose dates never overlap; last the board's own lines, one a day.",
            "texts": texts}, strings


def overlap(a, b):
    """Two spans ("d", [windows]) share a day of the year."""
    days_b = set().union(*(days_of(w) for w in b[1]))
    return any(d in days_b for w in a[1] for d in days_of(w))


def days_of(window):
    lo, hi = window
    out = set()
    m, d = int(lo[:2]), int(lo[3:])
    days = [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    for _ in range(370):
        cur = "%02d-%02d" % (m, d)
        out.add(cur)
        if cur == hi:
            break
        d += 1
        if d > days[m - 1]:
            d, m = 1, m % 12 + 1
    return out


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def main():
    check = "--check" in sys.argv
    tables = strings_tables()
    births, anniv = residents()
    species = {s["id"] for s in json.load(open(os.path.join(CONTENT, "BirdsData", "species.json"), encoding="utf-8"))["species"]}
    for b in RARE_BIRDS:
        if b not in species:
            sys.exit("paper.py: rare bird %s is not in species.json" % b)
    data = collections.OrderedDict()
    data["_doc"] = ("The town paper (docs/town_paper.md): written by tools/paper/paper.py, never edit by hand. Read by "
                    "ULyudTownPaper and tools/papersim (LyudPaper::FPaper, Private/City/TownPaperModel.h).")
    data["first_issue"] = FIRST_ISSUE
    data["rare_birds"] = RARE_BIRDS
    data["fest_blurbs"] = sorted(src.FEST)
    data["cinema"] = CINEMA
    data["newsreel"] = NEWSREEL
    # the holidays with a newsreel of their own (cinema.json "festivals", tools/films/films.py: film.programme.fest.<id>)
    with open(os.path.join(CONTENT, "City", "cinema.json"), encoding="utf-8") as f:
        data["newsreel_fest"] = sorted(json.load(f).get("festivals", {}))
    data["variants"] = variants(tables)
    data["fronts"] = pool("paper.story", src.FRONTS)
    data["town"] = pool("paper.town", src.TOWN)
    data["ads"] = pool("paper.ad", src.ADS)
    data["market"] = market()
    data["birthdays"] = births
    data["anniversaries"] = anniv
    notice_texts, notice_strings = notices()
    outs = [(OUT_DATA, dump(data)), (OUT_NOTICES, dump(notice_texts)), (OUT_NOTICE_STRINGS, dump(notice_strings))]
    for l in LANGS:
        outs.append((os.path.join(OUT_STRINGS, "strings_%s.json" % l), dump(tables[l])))
    changed = 0
    for path, text in outs:
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            changed += 1
            if not check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
        print("%s  %s" % ("same" if old == text else ("DIFF" if check else "wrote"), os.path.relpath(path, ROOT)))
    print("paper: %d keys a language, %d fronts, %d town stories, %d ads, %d birthdays, %d anniversaries, %d market goods, %d notices"
          % (len(tables["ru"]), len(src.FRONTS), len(src.TOWN), len(src.ADS), len(births), len(anniv), len(data["market"]), len(src.NOTICES)))
    if check and changed:
        sys.exit(1)


if __name__ == "__main__":
    main()
