#!/usr/bin/env python3
"""Looks for the town's residents: one seeded appearance per townsperson (no bpy; build_residents.py builds them).

    python3 blender/residents/looks.py [--townsfolk PATH] [--names PATH] [--seed N] [--out PATH] [--summary]

Input: Content/PeopleData/townsfolk.json (tools/town/town_life.py: key, age, female, household, occupation, temper) and
tools/town/names.json (the origin of every given name and surname). Output: blender/residents/cache/looks.json (every
look, what build_residents.py reads) and Content/PeopleData/town_looks.json (key -> model, what the game reads).

A look is fixed by the generator seed and the person's key, so regenerating the town keeps everybody's face as long as
they keep their key, age and sex. What it decides:
- body: MakeHuman macros (sex, the real age, weight, muscle, height, proportions) and ancestry (the african/asian/
  caucasian mix of MakeHuman), loosely after the origins of their surname and given name, with plenty of chance;
- face: ~30 "genes" (nose, mouth, chin, cheeks, eyes, ears, head shape...) mapped to MakeHuman's modelling targets;
  children are their parents' mix (same household and surname) plus a little of their own, so families look alike;
- skin (MakeHuman skins by age band, ancestry and sex) with a small tone shift, iris colour, eyebrows, eyelashes;
- hair style and colour (greying with age, now and then dyed), beard or moustache, glasses;
- clothes by job, age and style (blender/residents/wardrobe.py) with colours from palettes, a coat now and then;
- accessories (blender/residents/accessories.py: caps, headscarves, bags, string bags, aprons, a cane).
"""
import argparse
import hashlib
import json
import math
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import wardrobe as W  # noqa: E402

PEOPLE = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "PeopleData")
SEED = 20261106
VERSION = 1            # bump when the meaning of a look changes (forces a rebuild of every model)

# ------------------------------------------------------------------------------------------- ancestry

# name origin -> MakeHuman ancestry mix (african, asian, caucasian) and a skin tone shift (1 = the skin as painted)
ORIGIN_MIX = {
    **{o: ((0.0, 0.02, 0.98), 1.0) for o in ("ru", "ua", "by", "pl", "cz", "lt", "lv", "fi", "ee", "se", "no", "dk",
                                             "de", "nl", "gb", "ie", "sc", "hu")},
    **{o: ((0.02, 0.0, 0.98), 0.96) for o in ("fr", "it", "es", "pt", "gr", "ar")},
    **{o: ((0.0, 0.95, 0.05), 1.0) for o in ("jp", "cn", "kr")},
    "vn": ((0.0, 0.9, 0.1), 0.95), "ph": ((0.05, 0.85, 0.1), 0.9),
    "kz": ((0.0, 0.6, 0.4), 0.97),
    **{o: ((0.08, 0.02, 0.9), 0.88) for o in ("ge", "am", "az", "tr", "ir", "il")},
    "eg": ((0.15, 0.0, 0.85), 0.82), "in": ((0.25, 0.1, 0.65), 0.75),
    "ng": ((0.95, 0.0, 0.05), 1.0), "gh": ((0.95, 0.0, 0.05), 1.0), "et": ((0.8, 0.0, 0.2), 1.0),
    "br": ((0.35, 0.1, 0.55), 0.9), "mx": ((0.1, 0.35, 0.55), 0.88), "us": None,   # us: anyone
}
WORLD = [((0.0, 0.02, 0.98), 1.0), ((0.0, 0.95, 0.05), 1.0), ((0.95, 0.0, 0.05), 1.0), ((0.35, 0.1, 0.55), 0.9),
         ((0.08, 0.02, 0.9), 0.88), ((0.25, 0.1, 0.65), 0.75), ((0.0, 0.6, 0.4), 0.97)]

# ------------------------------------------------------------------------------------------------ face

SYM = "SYM"            # a target with l-/r- halves: both get the same weight
GENES = [
    # name, decr target, incr target, strength, symmetric
    ("head_fat", "head/head-fat-decr", "head/head-fat-incr", 0.6, False),
    ("head_width", "head/head-scale-horiz-decr", "head/head-scale-horiz-incr", 0.45, False),
    ("head_height", "head/head-scale-vert-decr", "head/head-scale-vert-incr", 0.4, False),
    ("nose_width", "nose/nose-scale-horiz-decr", "nose/nose-scale-horiz-incr", 0.7, False),
    ("nose_length", "nose/nose-scale-vert-decr", "nose/nose-scale-vert-incr", 0.6, False),
    ("nose_hump", "nose/nose-hump-decr", "nose/nose-hump-incr", 0.7, False),
    ("nose_tip", "nose/nose-point-width-decr", "nose/nose-point-width-incr", 0.6, False),
    ("nose_flare", "nose/nose-flaring-decr", "nose/nose-flaring-incr", 0.6, False),
    ("nose_up", "nose/nose-point-down", "nose/nose-point-up", 0.5, False),
    ("mouth_width", "mouth/mouth-scale-horiz-decr", "mouth/mouth-scale-horiz-incr", 0.55, False),
    ("upper_lip", "mouth/mouth-upperlip-volume-decr", "mouth/mouth-upperlip-volume-incr", 0.6, False),
    ("lower_lip", "mouth/mouth-lowerlip-volume-decr", "mouth/mouth-lowerlip-volume-incr", 0.6, False),
    ("mouth_corners", "mouth/mouth-angles-down", "mouth/mouth-angles-up", 0.35, False),
    ("chin_width", "chin/chin-width-decr", "chin/chin-width-incr", 0.6, False),
    ("chin_out", "chin/chin-prominent-decr", "chin/chin-prominent-incr", 0.6, False),
    ("chin_height", "chin/chin-height-decr", "chin/chin-height-incr", 0.5, False),
    ("jaw", "chin/chin-jaw-drop-decr", "chin/chin-jaw-drop-incr", 0.5, False),
    ("cheekbones", "cheek/%s-cheek-bones-decr", "cheek/%s-cheek-bones-incr", 0.6, True),
    ("cheeks", "cheek/%s-cheek-volume-decr", "cheek/%s-cheek-volume-incr", 0.6, True),
    ("eye_size", "eyes/%s-eye-scale-decr", "eyes/%s-eye-scale-incr", 0.4, True),
    ("eye_height", "eyes/%s-eye-height2-decr", "eyes/%s-eye-height2-incr", 0.4, True),
    ("eye_slant", "eyes/%s-eye-corner1-down", "eyes/%s-eye-corner1-up", 0.4, True),
    ("ears", "ears/%s-ear-scale-decr", "ears/%s-ear-scale-incr", 0.5, True),
    ("ear_wing", "ears/%s-ear-wing-decr", "ears/%s-ear-wing-incr", 0.5, True),
    ("forehead", "forehead/forehead-scale-vert-decr", "forehead/forehead-scale-vert-incr", 0.5, False),
    ("brow_ridge", "forehead/forehead-nubian-decr", "forehead/forehead-nubian-incr", 0.4, False),
    ("brows", "eyebrows/eyebrows-angle-down", "eyebrows/eyebrows-angle-up", 0.5, False),
    ("neck", "neck/neck-scale-horiz-decr", "neck/neck-scale-horiz-incr", 0.4, False),
    ("shoulders", "torso/measure-shoulder-dist-decr", "torso/measure-shoulder-dist-incr", 0.4, False),
    ("hips", "hip/hip-scale-horiz-decr", "hip/hip-scale-horiz-incr", 0.4, False),
]
HEAD_SHAPES = ["head/head-oval", "head/head-round", "head/head-square", "head/head-rectangular", "head/head-triangular",
               "head/head-diamond", "head/head-invertedtriangular"]


def mh_age(years):
    """MakeHuman's age slider: 0 = 1 year, 0.1875 = 11, 0.5 = 25, 1 = 90."""
    y = max(1.0, min(90.0, float(years)))
    if y <= 11:
        return 0.1875 * (y - 1) / 10
    if y <= 25:
        return 0.1875 + 0.3125 * (y - 11) / 14
    return 0.5 + 0.5 * (y - 25) / 65


def rng_for(seed, key, what=""):
    h = hashlib.sha256(("%s|%s|%s" % (seed, key, what)).encode()).digest()
    return random.Random(int.from_bytes(h[:8], "big"))


def pick(rnd, weighted):
    """[(value, weight), ...] -> value."""
    tot = sum(w for _, w in weighted)
    x = rnd.random() * tot
    for v, w in weighted:
        x -= w
        if x <= 0:
            return v
    return weighted[-1][0]


def clamp(x, lo, hi):
    return max(lo, min(hi, x))


def gauss(rnd, mu, sd, lo=None, hi=None):
    v = rnd.gauss(mu, sd)
    if lo is not None:
        v = max(lo, v)
    if hi is not None:
        v = min(hi, v)
    return v


def palette(rnd, name):
    return [round(c, 3) for c in pick(rnd, W.PALETTES[name])]


def jitter(rnd, rgb, amount=0.06):
    return [round(clamp(c * (1 + rnd.uniform(-amount, amount)), 0.0, 1.0), 3) for c in rgb]


# ----------------------------------------------------------------------------------------------- heredity

def ancestry(rnd, sur_origin, given_origin):
    """(african, asian, caucasian), tone: mostly after the surname, sometimes the given name, often anything."""
    r = rnd.random()
    src = sur_origin if r < 0.55 else (given_origin if r < 0.75 else None)
    base = ORIGIN_MIX.get(src) if src else None
    if base is None:
        base = rnd.choice(WORLD)
    mix, tone = base
    if rnd.random() < 0.25:                  # a grandparent from elsewhere
        other, otone = rnd.choice(WORLD)
        k = rnd.uniform(0.15, 0.5)
        mix = tuple(a * (1 - k) + b * k for a, b in zip(mix, other))
        tone = tone * (1 - k) + otone * k
    return [round(x, 3) for x in mix], round(tone, 3)


def fresh_genes(rnd):
    g = {name: round(clamp(rnd.gauss(0, 0.38), -1, 1), 3) for name, *_ in GENES}
    g["head_shape"] = rnd.randrange(len(HEAD_SHAPES))
    g["head_shape_w"] = round(rnd.uniform(0.1, 0.5), 3)
    return g


def child_genes(rnd, parents):
    g = {}
    for name, *_ in GENES:
        vals = [p[name] for p in parents]
        g[name] = round(clamp(sum(vals) / len(vals) * 0.85 + rnd.gauss(0, 0.2), -1, 1), 3)
    src = rnd.choice(parents)
    g["head_shape"] = src["head_shape"]
    g["head_shape_w"] = round(clamp(src["head_shape_w"] + rnd.gauss(0, 0.08), 0.05, 0.6), 3)
    return g


def natural_hair(rnd, mix):
    af, asn, ca = mix
    if af + asn > 0.6:
        return pick(rnd, [("black", 6), ("darkbrown", 3), ("brown", 0.5)])
    return pick(rnd, [("black", 1.5), ("darkbrown", 3), ("brown", 3), ("lightbrown", 2.2), ("dirtyblonde", 1.6),
                      ("blonde", 1.2), ("red", 0.35), ("auburn", 0.45), ("platinum", 0.2)])


def natural_iris(rnd, mix):
    af, asn, ca = mix
    if af + asn > 0.6:
        return pick(rnd, [("brown", 8), ("brownlight", 2)])
    return pick(rnd, [("brown", 3), ("brownlight", 2), ("blue", 2), ("lightblue", 1), ("deepblue", 0.6),
                      ("green", 1), ("bluegreen", 1), ("grey", 1.2), ("ice", 0.3)])


# ----------------------------------------------------------------------------------------------- outfits

WORK_OUTFITS = {
    # occupation: (style, extras)   extras: accessories / forced garments / palette overrides
    "waiter": ("waiter", {"acc": ["waist_apron"]}),
    "cook": ("cook", {"acc": ["chef_toque", "bib_apron"]}),
    "barista": ("casual", {"acc": ["waist_apron"]}),
    "seller": ("casual", {"acc_p": [("bib_apron", 0.5)]}),
    "cashier": ("casual", {"acc_p": [("bib_apron", 0.4)]}),
    "vendor": ("casual", {"acc_p": [("bib_apron", 0.5)]}),
    "butcher": ("work", {"acc": ["bib_apron"]}),
    "fishmonger": ("work", {"acc": ["bib_apron"], "acc_p": [("knit_beanie", 0.5)]}),
    "fisherman": ("work", {"acc_p": [("knit_beanie", 0.4), ("flat_cap", 0.25), ("mindfront_fishing_hat_01", 0.2)], "boots": 0.7}),
    "lighthouse_keeper": ("work", {"acc_p": [("knit_beanie", 0.5)], "boots": 0.6}),
    "doctor": ("smart", {"coat": "white_coat"}),
    "nurse": ("casual", {"coat": "white_coat"}),
    "clerk": ("smart", {}), "manager": ("smart", {}), "director": ("smart", {}), "receptionist": ("smart", {}),
    "teacher": ("smart", {}), "dance_teacher": ("casual", {}), "projectionist": ("casual", {}),
    "repairman": ("work", {"acc_p": [("baseball_cap", 0.4)]}), "janitor": ("work", {}), "cleaner": ("work", {}),
    "guard": ("smart", {"acc_p": [("baseball_cap", 0.3)]}), "lifeguard": ("summer", {"acc_p": [("baseball_cap", 0.5), ("sun_hat", 0.2)]}),
    "coach": ("sport", {}), "carousel_operator": ("casual", {"acc_p": [("baseball_cap", 0.4)]}),
    "programmer": ("casual", {}), "musician": ("casual", {}), "artist": ("casual", {}), "writer": ("casual", {}),
    "student": ("casual", {"acc_p": [("backpack", 0.5), ("shoulder_bag", 0.3)]}),
    "pupil": ("kid", {"acc": ["backpack"]}),
}


def wearable(name, sex, age, styles):
    g = W.GARMENTS[name]
    return (g["sex"] in ("u", sex)) and g["ages"][0] <= age <= g["ages"][1] and any(s in g["styles"] for s in styles)


def garments_of(kind, sex, age, styles):
    return [n for n, g in W.GARMENTS.items() if g["kind"] == kind and wearable(n, sex, age, styles)]


def choose_outfit(rnd, sex, age, occupation, temper):
    """-> (garment list, style name, extras dict)."""
    style, extras = WORK_OUTFITS.get(occupation, (None, {}))
    if age < 12:
        style = "kid"
    elif style is None:
        if occupation == "pensioner" or age >= 66:
            style = "elder"
        elif temper.get("active", 0.5) > 0.72 and age < 45 and rnd.random() < 0.5:
            style = "sport"
        elif temper.get("studious", 0.5) > 0.65 and rnd.random() < 0.5:
            style = "smart"
        else:
            style = "casual"
    styles = {"waiter": ["smart"], "cook": ["work"]}.get(style, [style])
    if style == "elder":
        styles = ["elder", "casual"] if rnd.random() < 0.4 else ["elder"]
    out = []
    # body: a dress (women, not at work in overalls) or a suit, or a top + bottom
    dresses = garments_of("dress", sex, age, styles) if sex == "f" and style not in ("work", "cook", "waiter") else []
    suits = garments_of("suit", sex, age, styles) or garments_of("suit", sex, age, ["casual"])
    tops = garments_of("top", sex, age, styles)
    bottoms = garments_of("bottom", sex, age, styles)
    r = rnd.random()
    if style == "cook":
        out.append("male_worksuit01")
    elif style == "waiter":
        out.append("male_casualsuit03" if sex == "m" else "female_elegantsuit01")
    elif dresses and r < (0.45 if style in ("elder", "summer") else 0.25):
        out.append(rnd.choice(dresses))
    elif tops and bottoms and sex == "f" and r < 0.55:
        out += [rnd.choice(tops), rnd.choice(bottoms)]
    else:
        out.append(rnd.choice(suits))
    # coat over it: the town is on the Baltic and the game begins in late October
    p_coat = {"elder": 0.45, "smart": 0.3, "casual": 0.22, "work": 0.12, "kid": 0.08, "sport": 0.03}.get(style, 0.0)
    if extras.get("coat"):
        out.append("punkduck_coat")
    elif age >= 14 and rnd.random() < p_coat:
        out.append("punkduck_coat")
    # shoes
    boots = extras.get("boots", 0.15 if style in ("elder", "work") else 0.05)
    if rnd.random() < boots:
        out.append("punkduck_winter_boots")
    else:
        shoes = garments_of("shoes", sex, age, styles) or garments_of("shoes", sex, age, ["casual"])
        out.append(rnd.choice(shoes))
    return out, style, extras


def tint_for(rnd, garment, slot, style, extras, age):
    pal = W.GARMENTS[garment].get("palettes", {}).get(slot)
    if pal is None:
        return None
    if garment == "punkduck_coat" and extras.get("coat"):
        pal = extras["coat"]
    elif style == "elder" and slot in ("top", "outer") and pal in ("tee", "shirt", "blouse", "jacket", "dress", "coat") and rnd.random() < 0.7:
        pal = "elder_top"
    elif style == "kid" and slot == "top" and pal in ("tee", "shirt", "dress"):
        pal = "kid_top"
    elif style == "waiter":
        pal = {"top": "white_coat", "bottom": "suit"}.get(slot, pal)
    elif style == "cook":
        pal = "white_coat"
    return jitter(rnd, palette(rnd, pal))


# ------------------------------------------------------------------------------------------------ a look

def make_look(person, seed, names, parents_looks=None):
    key = person["key"]
    rnd = rng_for(seed, key, "look%d" % VERSION)
    sex = "f" if person.get("female") else "m"
    age = int(person.get("age", 30))
    sur_id, given_id = split_key(key)
    sur_o = names["sur"].get(sur_id)
    given_o = names["given"].get(given_id)
    if parents_looks:
        mixes = [p["ancestry"] for p in parents_looks]
        mix = [round(sum(m[i] for m in mixes) / len(mixes), 3) for i in range(3)]
        tone = round(sum(p["tone"] for p in parents_looks) / len(parents_looks) * rnd.uniform(0.98, 1.02), 3)
        genes = child_genes(rnd, [p["genes"] for p in parents_looks])
        hair_nat = rnd.choice(parents_looks)["hair_natural"] if rnd.random() < 0.8 else natural_hair(rnd, mix)
        iris = rnd.choice(parents_looks)["iris"] if rnd.random() < 0.75 else natural_iris(rnd, mix)
    else:
        mix, tone = ancestry(rnd, sur_o, given_o)
        genes = fresh_genes(rnd)
        hair_nat = natural_hair(rnd, mix)
        iris = natural_iris(rnd, mix)
    temper = person.get("temper", {})

    # body
    if age < 13:
        weight, muscle = gauss(rnd, 0.5, 0.07, 0.38, 0.65), gauss(rnd, 0.45, 0.05, 0.35, 0.55)
    elif age < 30:
        weight, muscle = gauss(rnd, 0.48, 0.1, 0.3, 0.75), gauss(rnd, 0.5 + 0.08 * temper.get("active", 0.5), 0.09, 0.3, 0.75)
    elif age < 60:
        weight, muscle = gauss(rnd, 0.58, 0.12, 0.35, 0.9), gauss(rnd, 0.48, 0.09, 0.28, 0.72)
    else:
        weight, muscle = gauss(rnd, 0.56, 0.11, 0.35, 0.85), gauss(rnd, 0.38, 0.07, 0.22, 0.55)
    height = gauss(rnd, 0.5, 0.13, 0.15, 0.85)
    if age >= 70:
        height -= 0.06
    macro = dict(gender=round(rnd.uniform(0.0, 0.08) if sex == "f" else rnd.uniform(0.92, 1.0), 3),
                 age=round(mh_age(age), 4), muscle=round(muscle, 3), weight=round(weight, 3),
                 height=round(clamp(height, 0.1, 0.9), 3), proportions=round(gauss(rnd, 0.5, 0.06, 0.38, 0.62), 3),
                 cupsize=round(gauss(rnd, 0.5, 0.12, 0.25, 0.8) if sex == "f" and age >= 13 else 0.5, 3),
                 firmness=round(gauss(rnd, 0.5 if age < 45 else 0.35, 0.08, 0.2, 0.7), 3),
                 race=dict(african=mix[0], asian=mix[1], caucasian=mix[2]))
    targets = {}
    for name, lo, hi, strength, sym in GENES:
        v = genes[name] * strength
        if age < 10:
            v *= 0.5                           # children's faces differ less
        t = hi if v > 0 else lo
        w = round(min(0.6, abs(v)), 3)
        if w < 0.03:
            continue
        for side in (("l", "r") if sym else (None,)):
            targets[t % side if side else t] = w
    targets[HEAD_SHAPES[genes["head_shape"]]] = genes["head_shape_w"]
    if age >= 45:
        a = clamp((age - 45) / 40, 0, 1)
        targets["head/head-age-incr"] = round(0.3 + 0.6 * a, 3)
        for s in ("l", "r"):
            targets["eyes/%s-eye-bag-incr" % s] = round(0.2 + 0.5 * a, 3)
        if weight > 0.6:
            targets["neck/neck-double-incr"] = round(min(0.6, (weight - 0.6) * 2 + 0.2 * a), 3)
    if sex == "m" and age >= 35 and weight > 0.55 and rnd.random() < 0.6:
        targets["stomach/stomach-pregnant-incr"] = round(min(0.5, (weight - 0.5) * 1.5), 3)

    # skin
    band = "young" if age < 33 else ("middleage" if age < 58 else "old")
    anc = pick(rnd, [("african", mix[0] ** 2 + 1e-6), ("asian", mix[1] ** 2 + 1e-6), ("caucasian", mix[2] ** 2 + 1e-6)])
    skin = rnd.choice(W.SKINS[(band, anc, sex)])
    # darker for a caucasian skin on a mixed body, a little personal variation for everyone
    shade = tone * (1.0 - 0.45 * mix[0] if anc != "african" else 1.0) * rnd.uniform(0.94, 1.06)
    skin_tint = round(clamp(shade, 0.5, 1.12), 3)

    # hair
    grey_p = 0.0 if age < 35 else clamp((age - 35) / 40, 0, 0.92)
    hair_color = hair_nat
    if rnd.random() < grey_p:
        hair_color = pick(rnd, [("grey", 3), ("silver", 2), ("white", 1 + (age > 75) * 2)])
        if sex == "f" and age < 80 and rnd.random() < 0.4:            # dyed
            hair_color = pick(rnd, [("auburn", 2), ("brown", 2), ("lightbrown", 1), ("blonde", 1), ("dyed_purple", 0.3)])
    elif sex == "f" and 15 <= age < 35 and rnd.random() < 0.06:
        hair_color = rnd.choice(["dyed_purple", "dyed_blue", "dyed_pink", "platinum"])
    styles_ok = [h for h, d in W.HAIR.items() if d["sex"] in ("u", sex) and d["ages"][0] <= age <= d["ages"][1]]
    weights = []
    for h in styles_ok:
        w = 1.0
        if h == "afro01":
            w = 3.0 if mix[0] > 0.6 else 0.05
        if sex == "f" and age >= 60 and h in ("bob02", "short02"):
            w = 4.0
        if sex == "f" and age >= 60 and h in ("braid01", "ponytail01", "bob01"):
            w = 0.3
        if sex == "m" and h == "short02":
            w = 1.5
        weights.append((h, w))
    hair = pick(rnd, weights)
    bald_p = 0.0 if sex == "f" or age < 30 else clamp((age - 30) / 50, 0, 0.5)
    if rnd.random() < bald_p:
        hair = None
    # brows, lashes, eyes
    brows = rnd.choice(W.EYEBROWS_F if sex == "f" else W.EYEBROWS_M)
    lashes = rnd.choice(W.EYELASHES_F if sex == "f" else W.EYELASHES_M)
    brow_color = hair_nat if hair_color in ("dyed_purple", "dyed_blue", "dyed_pink", "platinum") else hair_color
    if brow_color in ("white", "silver") and rnd.random() < 0.5:
        brow_color = "grey"

    # clothes and accessories
    outfit, style, extras = choose_outfit(rnd, sex, age, person.get("occupation", ""), temper)
    tints = {}
    for g in outfit:
        for slot in sorted(set(s for _, s in W.GARMENTS[g]["slots"])):
            if slot.startswith("keep"):
                continue
            t = tint_for(rnd, g, slot, style, extras, age)
            if t:
                tints["%s:%s" % (g, slot)] = t
    acc = list(extras.get("acc", []))
    for a, p in extras.get("acc_p", []):
        if rnd.random() < p:
            acc.append(a)
    if style == "elder":
        if sex == "f" and rnd.random() < 0.3 and "headscarf" not in acc:
            acc.append("headscarf")
        if sex == "m" and rnd.random() < 0.4:
            acc.append(rnd.choice(["flat_cap", "flat_cap", "sun_hat", "knit_beanie"]))
        if rnd.random() < 0.15:
            acc.append(rnd.choice(["string_bag", "shopping_bag"]))
        if age >= 78 and rnd.random() < 0.3:
            acc.append("cane")
    elif age >= 13:
        if rnd.random() < 0.08:
            acc.append(rnd.choice(["baseball_cap", "knit_beanie"]))
        if rnd.random() < 0.12:
            acc.append(rnd.choice(["shoulder_bag", "backpack", "shopping_bag"]))
        if rnd.random() < 0.08:
            acc.append("scarf")
    gl_p = 0.05 if age < 13 else (0.15 if age < 45 else 0.38)
    if rnd.random() < gl_p:
        acc.append("glasses:" + pick(rnd, [("round", 1.2), ("rect", 1.5)]))
    elif 16 <= age < 60 and rnd.random() < 0.04:
        acc.append("glasses:sun")
    if sex == "m" and age >= 17:
        fh = pick(rnd, [(None, 6.5), ("mustache", 1.0 + (age > 50)), ("short_beard", 1.3), ("full_beard", 0.8)])
        if fh:
            acc.append(fh)
    # hats and headscarves cover hair: only one thing on the head
    heads = [a for a in acc if a in ("flat_cap", "knit_beanie", "baseball_cap", "sun_hat", "headscarf", "chef_toque",
                                     "mindfront_fishing_hat_01")]
    for extra in heads[1:]:
        acc.remove(extra)
    acc_tints = {}
    for a in acc:
        base = a.split(":")[0]
        if base in ("glasses", "mustache", "short_beard", "full_beard", "cane"):
            continue
        acc_tints[a] = jitter(rnd, palette(rnd, {"headscarf": "dress", "scarf": "kid_top" if age < 13 else "coat",
                                                 "chef_toque": "white_coat", "bib_apron": "white_coat" if style == "cook" else "jacket",
                                                 "waist_apron": "suit" if style == "waiter" else "jacket",
                                                 "backpack": "kid_top" if age < 16 else "jacket"}.get(base, "coat")))

    look = dict(key=key, model=model_name(key), sex=sex, age=age, occupation=person.get("occupation", ""),
                style=style, ancestry=mix, tone=tone, genes=genes, macro=macro, targets=targets,
                skin=skin, skin_tint=skin_tint, iris=iris, eyebrows=brows, eyelashes=lashes,
                hair=hair, hair_natural=hair_nat, hair_color=hair_color, brow_color=brow_color,
                hair_rgb=list(W.HAIR_COLORS[hair_color]), brow_rgb=list(W.HAIR_COLORS[brow_color]),
                clothes=outfit, tints=tints, accessories=acc, accessory_tints=acc_tints)
    look["hash"] = hashlib.sha1(json.dumps({k: v for k, v in look.items() if k not in ("genes", "hash")},
                                           sort_keys=True).encode()).hexdigest()[:12]
    return look


def split_key(key):
    k = key[2:] if key.startswith("t_") else key
    parts = k.split("_")
    if len(parts) < 2:
        return k, ""
    given = parts[-1].rstrip("0123456789")
    return "_".join(parts[:-1]), given


def model_name(key):
    k = key[2:] if key.startswith("t_") else key
    return "R_" + "".join(ch if ch.isalnum() or ch == "_" else "_" for ch in k)


def load_json(path_or_ref):
    """A path in the working tree, or <git ref>:<path> (a branch not merged yet)."""
    if os.path.isfile(path_or_ref):
        with open(path_or_ref, encoding="utf-8") as f:
            return json.load(f)
    ref, _, path = path_or_ref.partition(":")
    out = subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (ref, path)], capture_output=True, check=True)
    return json.loads(out.stdout.decode("utf-8"))


def load_names(src):
    d = load_json(src)
    return {"given": {g["id"]: g.get("origin") for g in d["given"]},
            "sur": {s["id"]: s.get("origin") for s in d["surnames"]}}


def make_all(people, seed, names):
    by_key = {p["key"]: p for p in people}
    households = {}
    for p in people:
        households.setdefault(p.get("household", p["key"]), []).append(p)
    looks = {}
    # adults first, then children from their parents (same household, same surname, 16+ years older)
    order = sorted(people, key=lambda p: -int(p.get("age", 30)))
    for p in order:
        parents = []
        if int(p.get("age", 30)) < 18:
            sur, _ = split_key(p["key"])
            for q in households.get(p.get("household", p["key"]), []):
                if q["key"] != p["key"] and int(q.get("age", 30)) >= int(p.get("age", 30)) + 16 \
                        and split_key(q["key"])[0] == sur and q["key"] in looks:
                    parents.append(looks[q["key"]])
            parents = parents[:2]
        looks[p["key"]] = make_look(p, seed, names, parents or None)
    return looks


def summary(looks):
    from collections import Counter
    c = Counter()
    for lk in looks.values():
        for g in lk["clothes"]:
            c["clothes " + g] += 1
        for a in lk["accessories"]:
            c["acc " + a.split(":")[0]] += 1
        c["hair " + str(lk["hair"])] += 1
        c["skin " + lk["skin"]] += 1
        c["style " + lk["style"]] += 1
    for k, v in sorted(c.items()):
        print("%5d  %s" % (v, k))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--townsfolk", default=os.path.join(PEOPLE, "townsfolk.json"))
    ap.add_argument("--names", default=os.path.join(ROOT, "tools", "town", "names.json"))
    ap.add_argument("--ref", help="read townsfolk.json and names.json from this git ref (e.g. origin/town-life)")
    ap.add_argument("--seed", type=int, default=SEED)
    ap.add_argument("--out", default=os.path.join(HERE, "cache", "looks.json"))
    ap.add_argument("--game-out", default=os.path.join(PEOPLE, "town_looks.json"))
    ap.add_argument("--summary", action="store_true")
    a = ap.parse_args()
    tf = a.townsfolk
    nm = a.names
    if a.ref:
        tf = "%s:unreal/Lyudmilovsk/Content/PeopleData/townsfolk.json" % a.ref
        nm = "%s:tools/town/names.json" % a.ref
    people = load_json(tf)["people"]
    names = load_names(nm)
    looks = make_all(people, a.seed, names)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump({"seed": a.seed, "version": VERSION, "looks": looks}, f, indent=1, sort_keys=True)
    game = {"_doc": "Each townsperson's own model, made by blender/residents (docs/residents.md). tools/town/town_life.py "
                    "copies it into townsfolk.json 'body'; the pool model stays as 'mesh' until the model is imported.",
            "seed": a.seed, "version": VERSION,
            "people": {k: {"model": "Town/" + lk["model"], "hash": lk["hash"]} for k, lk in sorted(looks.items())}}
    with open(a.game_out, "w", encoding="utf-8") as f:
        json.dump(game, f, indent=1, sort_keys=True)
    print("looks: %d people -> %s, %s" % (len(looks), os.path.relpath(a.out, ROOT), os.path.relpath(a.game_out, ROOT)))
    if a.summary:
        summary(looks)


if __name__ == "__main__":
    main()
