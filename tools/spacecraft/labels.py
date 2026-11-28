"""The printed plates of the Lastochka's panel as one atlas per language (docs/spacecraft.md, the localization skill).

  words    tools/spacecraft/strings.json          (never a literal word here or in labels.json)
  plates   tools/spacecraft/labels.json           plate id -> string keys (a key per text line), class, fallback size
  rects    unreal/Lyudmilovsk/Content/Spacecraft/kit.json when the Blender kit lists its label plates, otherwise an
           auto-packed layout; either way the rects that were drawn are written back to labels.json "layout"
  output   textures/spacecraft/T_SC_labels_<lang>.png   1024 x 1024, 8-bit grey, white text on black (use as a mask / emissive)

    python3 tools/spacecraft/labels.py                 draw ru, en, ja
    python3 tools/spacecraft/labels.py --lang ja       one language
    python3 tools/spacecraft/labels.py --check         nothing written; exit 1 if a plate has no text, a glyph is missing,
                                                       a text had to be squeezed below the floor, or a PNG is stale
    python3 tools/spacecraft/labels.py --auto          ignore kit.json and pack the plates here
    python3 tools/spacecraft/labels.py --boxes         also draw each rect's outline (a proof to look at, not to ship)

Rects are pixels of the 1024 atlas, [x, y, w, h], origin top left (UV = px / 1024, V down as in Unreal).

What it understands in kit.json (the first that is found wins): a top-level (or under "atlas" / "textures" / "labels_atlas")
"labels" | "label_plates" | "label_rects" | "plates", being either {id: entry} or [{"id" | "name": id, ...entry}], where an
entry is [x, y, w, h] or has "rect" | "px" ([x, y, w, h]) or "uv" | "uv_rect" ([u0, v0, u1, v1], V down) or x, y, w, h fields.
Numbers all <= 1 are fractions of the atlas; pixel rects are scaled if the kit names another atlas size ("atlas_size" |
"size" | "labels_size" beside the list). An entry may name its words itself: "key" (one line) or "keys" (lines).
A kit plate this file's labels.json does not know and that names no key is reported and left black; a plate of
labels.json the kit has no rect for is reported and not drawn.

Text is fitted to its rect: the largest size the class allows, then smaller, then narrower; never clipped.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
STRINGS = os.path.join(HERE, "strings.json")
LABELS = os.path.join(HERE, "labels.json")
KIT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Spacecraft", "kit.json")
OUT = os.path.join(ROOT, "textures", "spacecraft", "T_SC_labels_%s.png")
LANGS = ("ru", "en", "ja")
SS = 4               # supersampling
MIN_PX = 9           # below this cap height in px a plate is unreadable on the panel: reported
MIN_CONDENSE = 0.55  # the narrowest a line may be squeezed before it is reported
LINE_PITCH = 1.22

KIT_LISTS = ("labels", "label_plates", "label_rects", "plates")
KIT_PARENTS = (None, "atlas", "textures", "labels_atlas")
KIT_SIZES = ("atlas_size", "labels_size", "size")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


# ---------------------------------------------------------------------------------------------------------- the rects

def kit_plates(size):
    """(rects {id: [x, y, w, h] px}, keys {id: [key...]}, a note) from kit.json, or (None, {}, why not)."""
    if not os.path.isfile(KIT):
        return None, {}, "no kit.json yet"
    kit = load(KIT)
    found = None
    panel_labels = (kit.get("panel") or {}).get("labels")  # blender/spacecraft/spacecraft_kit.py: panel.labels.plates
    if isinstance(panel_labels, dict) and panel_labels.get("plates"):
        found = (panel_labels, "plates")
    for parent in () if found else KIT_PARENTS:
        box = kit if parent is None else kit.get(parent)
        if not isinstance(box, dict):
            continue
        for name in KIT_LISTS:
            if isinstance(box.get(name), (dict, list)) and box[name]:
                found = (box, name)
                break
        if found:
            break
    if not found:
        return None, {}, "kit.json has no label plate list (%s)" % " | ".join(KIT_LISTS)
    box, name = found
    entries = box[name]
    if isinstance(entries, list):
        entries = {str(e.get("id", e.get("name"))): e for e in entries if isinstance(e, dict)}
    kit_size = next((box[k] for k in KIT_SIZES if isinstance(box.get(k), (int, float))), None)
    if kit_size is None:
        kit_size = next((box[k][0] for k in KIT_SIZES if isinstance(box.get(k), list) and box[k]), size)
    rects, keys, skipped = {}, {}, []
    for pid, e in entries.items():
        r, uv = None, False
        if isinstance(e, list) and len(e) == 4:
            r = e
        elif isinstance(e, dict):
            for k in ("rect", "px"):
                if isinstance(e.get(k), list) and len(e[k]) == 4:
                    r = e[k]
            for k in ("uv", "uv_rect"):
                if r is None and isinstance(e.get(k), list) and len(e[k]) == 4:
                    r, uv = e[k], True
            if r is None and all(k in e for k in ("x", "y", "w", "h")):
                r = [e["x"], e["y"], e["w"], e["h"]]
            if isinstance(e.get("key"), str):
                keys[pid] = [e["key"]]
            elif isinstance(e.get("keys"), list):
                keys[pid] = list(e["keys"])
        if r is None:
            skipped.append(pid)
            continue
        r = [float(v) for v in r]
        if uv:
            r = [r[0], r[1], r[2] - r[0], r[3] - r[1]]
        scale = size if max(r) <= 1.0 else size / float(kit_size)
        x, y, w, h = (int(round(v * scale)) for v in r)
        if w < 4 or h < 4 or x < 0 or y < 0 or x + w > size or y + h > size:
            skipped.append(pid)
            continue
        rects[pid] = [x, y, w, h]
    note = "kit.json %s: %d rects" % (name, len(rects))
    if skipped:
        note += ", %d unreadable (%s)" % (len(skipped), ", ".join(skipped[:6]))
    return (rects or None), keys, note


def auto_pack(conf):
    """Shelf packing in the order of labels.json (grouped by class, tallest classes first): deterministic."""
    size, gut = conf["size"], conf["gutter"]
    order = sorted(conf["plates"], key=lambda p: (-conf["classes"][conf["plates"][p]["class"]]["h"],
                                                  list(conf["classes"]).index(conf["plates"][p]["class"]),
                                                  list(conf["plates"]).index(p)))
    rects, x, y, shelf = {}, gut, gut, 0
    for pid in order:
        c = conf["classes"][conf["plates"][pid]["class"]]
        w, h = c["w"], c["h"]
        if x + w + gut > size:
            x, y, shelf = gut, y + shelf + gut, 0
        if y + h + gut > size:
            raise SystemExit("labels: the plates do not fit a %d atlas: shrink the classes in labels.json" % size)
        rects[pid] = [x, y, w, h]
        x += w + gut
        shelf = max(shelf, h)
    return rects


# ----------------------------------------------------------------------------------------------------------- the text

_fonts = {}


def font(path, px):
    key = (path, px)
    if key not in _fonts:
        _fonts[key] = ImageFont.truetype(os.path.join(ROOT, path), px)
    return _fonts[key]


def missing_glyphs(path, text):
    """Characters the face has no glyph for (it would draw .notdef): compared with a code point no font has."""
    f = font(path, 48)
    blank = bytes(f.getmask("\U0010FFFE"))
    out = []
    for ch in sorted(set(text)):
        if ch.isspace():
            continue
        m = bytes(f.getmask(ch))
        if m == blank or not any(m):
            out.append(ch)
    return out


def line_image(path, text, px, tracking):
    """The line drawn at px (already supersampled) with letter spacing, cropped to its advance width and the em box."""
    f = font(path, px)
    asc, desc = f.getmetrics()
    track = int(round(tracking * px))
    adv = [f.getlength(ch) for ch in text]
    width = int(sum(adv) + track * max(0, len(text) - 1)) + 4
    img = Image.new("L", (max(4, width), asc + desc), 0)
    d = ImageDraw.Draw(img)
    x = 2.0
    for ch, a in zip(text, adv):
        d.text((x, 0), ch, font=f, fill=255)
        x += a + track
    return img


def ink(path, px):
    """Top and bottom of the ink of capitals and kanji in the em box at px: plates are centred on it, not on the line box."""
    f = font(path, px)
    box = f.getbbox("НЕ電E")
    return box[1], box[3]


def fit(path, lines, w, h, cls, condense, tracking):
    """(px, condense) for the lines in a w x h box (supersampled px): the class's fill first, then smaller, then narrower."""
    pad = cls["pad"] * SS
    aw, ah = w - 2 * pad, h - 2 * pad
    n = len(lines)
    px = int(min(cls["fill"] * h / (1 + (n - 1) * LINE_PITCH), ah / (0.74 + (n - 1) * LINE_PITCH)))
    floor = MIN_PX * SS
    while True:
        widest = max(line_image(path, t, px, tracking).width for t in lines)
        if widest * condense <= aw or px <= floor:
            break
        px = max(floor, int(px * min(0.97, aw / (widest * condense))))
    squeeze = condense
    if widest * condense > aw:
        squeeze = aw / float(widest)
    return px, squeeze


def draw_plate(atlas, rect, path, lines, cls, condense, tracking):
    x, y, w, h = (v * SS for v in rect)
    px, sq = fit(path, lines, w, h, cls, condense, tracking)
    top, bottom = ink(path, px)
    cap = bottom - top
    pitch = int(px * LINE_PITCH)
    block = cap + pitch * (len(lines) - 1)
    y0 = y + (h - block) // 2 - top
    for i, t in enumerate(lines):
        img = line_image(path, t, px, tracking)
        nw = max(1, int(round(img.width * sq)))
        img = img.resize((nw, img.height), Image.LANCZOS)
        atlas.paste(img, (x + (w - nw) // 2, y0 + i * pitch), img)
    return cap / float(SS), sq


def render(conf, strings, rects, plate_keys, lang, boxes=False):
    size = conf["size"]
    atlas = Image.new("L", (size * SS, size * SS), 0)
    path = conf["font"][lang]
    bad, report = [], []
    for pid, rect in rects.items():
        keys = plate_keys.get(pid)
        if not keys:
            bad.append("%s: no string key for this plate (add it to labels.json plates)" % pid)
            continue
        missing = [k for k in keys if k not in strings or not strings[k].get(lang)]
        if missing:
            bad.append("%s: no %s text for %s" % (pid, lang, ", ".join(missing)))
            continue
        lines = [strings[k][lang] for k in keys]
        gone = missing_glyphs(path, "".join(lines))
        if gone:
            bad.append("%s: %s has no glyph for %s" % (pid, os.path.basename(path), " ".join(gone)))
        plate = conf["plates"].get(pid, {})
        cls = conf["classes"].get(plate.get("class", ""), None) or {"fill": 0.6, "pad": max(2, min(rect[2], rect[3]) // 8)}
        tracking = cls.get("tracking", conf["tracking"][lang])
        cap, sq = draw_plate(atlas, rect, path, lines, cls, conf["condense"][lang], tracking)
        report.append((pid, cap, sq))
        if sq < MIN_CONDENSE:
            bad.append("%s: %s squeezed to %.2f of its width in %dx%d: shorten the word or widen the plate" % (pid, lang, sq, rect[2], rect[3]))
        if cap < MIN_PX * 0.70:
            bad.append("%s: %s capitals are only %.1f px in %dx%d" % (pid, lang, cap, rect[2], rect[3]))
    out = atlas.resize((size, size), Image.LANCZOS)
    if boxes:
        d = ImageDraw.Draw(out)
        for x, y, w, h in rects.values():
            d.rectangle([x, y, x + w - 1, y + h - 1], outline=90)
    return out, bad, report


def main():
    args = sys.argv[1:]
    check = "--check" in args
    langs = [args[args.index("--lang") + 1]] if "--lang" in args else list(LANGS)
    conf = load(LABELS)
    strings = load(STRINGS)
    rects, kit_keys, note = (None, {}, "--auto") if "--auto" in args else kit_plates(conf["size"])
    source = "kit"
    if rects is None:
        rects, source = auto_pack(conf), "auto"
    plate_keys = {pid: p["keys"] for pid, p in conf["plates"].items()}
    plate_keys.update(kit_keys)
    bad = []
    if source == "kit":
        for pid in conf["plates"]:
            if pid not in rects:
                print("labels: note: plate %s has no rect in kit.json (not drawn)" % pid)
        for pid in rects:
            if pid not in plate_keys:
                bad.append("%s: a kit plate with no words (add it to labels.json plates, or a \"key\" in kit.json)" % pid)
    ids = list(rects)
    for i, a in enumerate(ids):
        ax, ay, aw, ah = rects[a]
        for b in ids[i + 1:]:
            bx, by, bw, bh = rects[b]
            if ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                bad.append("%s and %s overlap" % (a, b))
    layout = {"source": source, "note": note, "rects": rects}
    if conf.get("layout") != layout:
        if check:
            bad.append("labels.json layout is stale: run tools/spacecraft/labels.py")
        else:
            conf["layout"] = layout
            with open(LABELS, "w", encoding="utf-8") as f:
                json.dump(conf, f, ensure_ascii=False, indent=1)
                f.write("\n")
    for lang in langs:
        img, problems, report = render(conf, strings, rects, plate_keys, lang, "--boxes" in args)
        bad += problems
        path = OUT % lang
        if check:
            if not os.path.isfile(path):
                bad.append("%s is missing" % os.path.relpath(path, ROOT))
            elif Image.open(path).convert("L").tobytes() != img.tobytes():
                bad.append("%s is stale: run tools/spacecraft/labels.py" % os.path.relpath(path, ROOT))
        else:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            img.save(path, optimize=True)
        tight = sorted(report, key=lambda r: r[2])[:3]
        small = sorted(report, key=lambda r: r[1])[:3]
        print("labels %s: %d plates (%s), narrowest %s, smallest capitals %s" % (
            lang, len(report), source,
            ", ".join("%s x%.2f" % (p, s) for p, _, s in tight),
            ", ".join("%s %.0fpx" % (p, c) for p, c, _ in small)))
    for b in bad:
        print("labels: " + b)
    print("labels: layout %s (%s), %d rects, %d problems" % (source, note, len(rects), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
