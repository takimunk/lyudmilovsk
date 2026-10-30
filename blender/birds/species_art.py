"""Shape and plumage of each bird species, for build_bird.py. Sizes (length, wingspan) come from species.json; here
everything is relative to the template (Murmuration's hero starling) or a field-guide description of the plumage.

morph: proportions (1 = starling) and posture.
plumage:
    styles   name -> feather style (plumage.py): {"c": base, "fx": [...], "gloss", "rough", "skin", "bare", "loft"}
    regions  feather tract -> style (build_bird.FALLBACK walks up to "upper" / "under", which every species defines)
    rules    [(lambda q: ..., style)], checked first: q.region, q.z, q.s (up -1..1), q.c (|side| 0..1), q.dz, q.dy (from the eye)
    tracts   wing / tail tracts: a style, or a list per feather (primary p1 inner -> p9 outer, tail central -> outer)
    bare     beak (upper base), beak_tip, lower, lower_tip, gonys, base_skin, cere, mouth, legs, iris, eyering
    winter   {"styles": {...}, "bare": {...}}: the same styles recoloured for winter plumage (second albedo)
"""

MORPH_DEFAULTS = dict(
    body_len=1.0, body_w=1.0, body_h=1.0, belly=1.0, head=1.0, neck=1.0, neck_up=0.0, throat=1.0, crown_h=1.0, flat_crown=0.3,
    eye=1.15, eyering_w=0.0, beak_round=1.0, trousers=1.0, beak_knob=0.0, neck_thin=0.0,
    beak_len=1.0, beak_depth=1.0, beak_width=1.0, beak_taper=0.85, beak_curve=0.0, beak_hook=0.0, beak_gonys=0.0,
    arm=1.0, hand=1.0, prim=1.0, prim_profile=[1.0] * 9, prim_width=1.0, prim_angle0=70.0, prim_fan=60.0, sec=1.0, sec_width=1.0, covert=1.0,
    tail=1.0, tail_shape="square", fork=0.0, streamer=0.0, tail_width=1.0, tail_fan=1.0,
    shin=1.0, tarsus=1.0, leg_thick=1.0, toes=1.0, hallux=1.0, claw=1.0, webbed=False,
    fluff=1.0, crest=0.0, fluff_layer=0.55,
    n_prim=9, n_sec=6, n_tail=6, p_outer=1.0, n_neck=1, neck_s=0.0, foot="aniso", keel=0.0, rear_taper=0.0,
    perch_pitch=22.0, perch_low=0.008, perch_neck=-14.0, perch_head=-6.0, perch_tail=-8.0, peck_depth=42.0, head_bob=0.0, tail_flick=1.0, tail_wag=False,
    call_gape=22.0, glide_elev=6.0, glide_sweep=-4.0, glide_hand=0.0, glide_handelev=0.0, fingers=0.0, fly_neck=0.0, fly_head=0.0, fly_neck_s=None, fly_trail=0.0, bare_shin=0.0,
    feather_shapes={},
)

# profiles of the 9 primaries' lengths (inner -> outer) relative to the starling's
POINTED = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
ROUNDED = [1.02, 1.03, 1.04, 1.05, 1.05, 1.03, 0.98, 0.9, 0.78]
LONG_POINTED = [0.95, 0.97, 1.0, 1.03, 1.07, 1.11, 1.15, 1.18, 1.16]
SICKLE = [0.8, 0.86, 0.93, 1.02, 1.12, 1.24, 1.36, 1.47, 1.55]
FINGERED = [1.0, 1.02, 1.04, 1.06, 1.08, 1.08, 1.06, 1.0, 0.88]


def st(c, *fx, **kw):
    d = {"c": c, "fx": list(fx)}
    d.update(kw)
    return d


SPECIES = {}


def species(name, morph, plumage):
    SPECIES[name] = {"morph": morph, "plumage": plumage}


def get(name):
    return SPECIES[name]


# ---------------------------------------------------------------- Starling (Sturnus vulgaris), breeding plumage; winter spangled
species("Starling", dict(), dict(
    styles={
        "gloss_head": st("#130f17", ("down", "#2a2630"), gloss=0.95, rough=0.35),
        "gloss_body": st("#120e15", ("down", "#2a2630"), gloss=0.9, rough=0.38),
        "gloss_back": st("#0c140f", ("spangle", "#cfa56b", 0.22), ("down", "#2a2630"), gloss=0.9, rough=0.38),
        "under_tail": st("#141018", ("fringe", "#e9dfcc", 0.25, "all"), gloss=0.5),
        "flight": st("#15120e", ("fringe", "#8c6a42", 0.10, "outer"), ("fringe", "#8c6a42", 0.08, "tip"), gloss=0.25, rough=0.5),
        "tert": st("#15120e", ("fringe", "#b08a58", 0.16, "all"), gloss=0.3),
        "covert": st("#121410", ("fringe", "#b8925c", 0.22, "all"), gloss=0.6),
        "underwing": st("#4a443c", ("fringe", "#b5a58c", 0.2, "all")),
        "tail": st("#1a1612", ("fringe", "#9a7a50", 0.10, "all")),
    },
    regions={"upper": "gloss_back", "head": "gloss_head", "nape": "gloss_head", "under": "gloss_body", "throat": "gloss_head",
             "vent": "under_tail", "undertail": "under_tail", "lcovert": "gloss_back", "gcovert": "covert", "pcovert": "covert",
             "underwing": "underwing", "primary": "flight", "tertial": "tert", "tail": "tail", "alula": "flight"},
    bare=dict(beak="#d9b13a", beak_tip="#e8c24a", lower="#c7a043", lower_tip="#e3bf4c", mouth="#c8805a", legs="#b8786a", iris="#2a1a10",
              eyering="#2a2420", tip_from=0.2),
    winter=dict(styles={
        "gloss_head": st("#130f17", ("spangle", "#d9c49e", 0.2), gloss=0.9, rough=0.35),
        "gloss_body": st("#120e15", ("spangle", "#f1ece2", 0.55), gloss=0.85, rough=0.38),
        "gloss_back": st("#0c140f", ("spangle", "#cfa56b", 0.48), gloss=0.85, rough=0.38)},
        bare=dict(beak="#3b3631", beak_tip="#3b3631", lower="#3b3631", lower_tip="#3b3631")),
))

# ---------------------------------------------------------------- Great tit (Parus major), male
species("GreatTit", dict(body_len=0.82, body_w=1.08, body_h=1.08, head=1.32, neck=0.55, throat=1.15, perch_low=0.011, beak_len=0.42, beak_depth=0.85, beak_width=0.85,
                         beak_taper=1.1, arm=0.85, hand=0.8, prim=0.85, prim_profile=ROUNDED, tail=1.12, tail_shape="notch", tarsus=0.82,
                         leg_thick=0.9, fluff=1.25, perch_pitch=28, perch_neck=-8, peck_depth=36, tail_flick=1.4, flat_crown=0.4),
        dict(styles={
            "black": st("#0d0e12", ("down", "#2a2a30"), gloss=0.2, rough=0.4),
            "tie_edge": st("#0d0e12", ("down", "#2a2a30"), gloss=0.2, rough=0.4),
            "white": st("#f0f0ea", ("down", "#8a8a88")),
            "yellow": st("#e2cd3a", ("down", "#5a5a50"), ("grad", "#e8d850", 0.3)),
            "mantle": st("#7c9a4c", ("down", "#4a4a42")),
            "rump": st("#7e8c8a"),
            "vent": st("#f2f0e4"),
            "nape_spot": st("#e8ebe4"),
            "flight": st("#3a434c", ("fringe", "#8fa2a8", 0.14, "outer")),
            "tert": st("#3a434c", ("fringe", "#d8e0dc", 0.12, "all")),
            "gcovert": st("#3e4a56", ("tip", "#f2f2ee", 0.22)),
            "covert": st("#56708a", ("fringe", "#62809a", 0.15, "all")),
            "tail": st("#3c4652", ("fringe", "#7e94a8", 0.12, "outer")),
            "tail_outer": st("#3c4652", ("outer", "#f2f2ee")),
            "underwing": st("#c8c8c4"),
        },
            regions={"upper": "mantle", "head": "black", "crown": "black", "nape": "black", "ear": "white", "malar": "white", "lore": "black",
                     "eyestripe": "black", "chin": "black", "throat": "black", "under": "yellow", "vent": "vent", "rump": "rump",
                     "uppertail": "rump", "lcovert": "covert", "gcovert": "gcovert", "pcovert": "covert", "primary": "flight", "tertial": "tert",
                     "underwing": "underwing", "undertail": "vent", "trousers": "rump", "scapular": "mantle"},
            rules=[(lambda q: q.region in ("ear", "eyestripe", "malar") and q.dy > -0.0080 and q.dy < 0.0010 and q.dz < 0.002 and q.dz > -0.0105, "white"),
                   (lambda q: q.region == "nape" and q.s > 0.85 and q.z < 0.056 and q.z > 0.048, "nape_spot"),
                   (lambda q: q.z < 0.052 and q.z > -0.03 and q.s < -0.55 and q.c < 0.17, "black"),   # the tie
                   (lambda q: q.z < 0.052 and q.z > -0.03 and q.s < -0.55 and q.c < 0.32, "tie_edge"),   # its edge (wider in males)
                   (lambda q: q.region in ("neckside",) and q.z > 0.045, "black")],
            tracts={"tail": ["tail", "tail", "tail", "tail", "tail", "tail_outer"]},
            bare=dict(beak="#141416", beak_tip="#1c1c20", legs="#5a6a7a", iris="#120c0a", eyering="#16161a", mouth="#d07a6a")))


def fl(c, fringe=None, w=0.12, where="outer", *more, **kw):
    """Flight / covert feather: colour, optional pale edge."""
    fx = [("fringe", fringe, w, where)] if fringe else []
    return st(c, *fx, *more, **kw)


# ---------------------------------------------------------------- Corvids
CORVID = dict(body_len=1.0, body_w=1.08, body_h=1.12, head=1.1, neck_up=0.002, crown_h=1.12, flat_crown=-0.6, beak_taper=0.8, beak_round=1.3, arm=1.05, hand=1.0,
              prim_profile=FINGERED, feather_shapes={"primary": {"emarg": 0.45}}, fingers=1.0, tail_shape="round", tarsus=1.18,
              leg_thick=1.6, toes=1.15, claw=1.2, fluff=0.9, perch_pitch=26, perch_neck=-6, head_bob=0.004, glide_elev=4, call_gape=34)
species("Rook", dict(CORVID, beak_len=1.45, beak_depth=1.6, beak_width=1.2, beak_curve=0.0006, tail=1.15, gloss_hue=0.62), dict(
    styles={
        "black": st("#0c0b10", ("down", "#1c1a22"), gloss=0.75, rough=0.35),
        "black_dull": st("#0e0d12", gloss=0.35, rough=0.5),
        "trousers": st("#0d0c11", ("fringe", "#18161e", 0.3, "all"), gloss=0.4, loft=2.2),
        "face": st("#b4aea8", bare=True, skin="#a8a29c"),
        "flight": st("#0b0b10", ("shaft", "#141418"), gloss=0.6, rough=0.35),
        "under": st("#121116", gloss=0.2),
    },
    regions={"upper": "black", "under": "black_dull", "head": "black", "underwing": "under", "primary": "flight", "tail": "flight",
             "trousers": "trousers", "flank": "black_dull"},
    rules=[(lambda q: q.head and q.region in ("lore", "chin", "malar") and q.dz > 0.0028 and q.dy < 0.0, "face"),
           (lambda q: q.head and q.region == "throat" and q.z > 0.068, "face")],
    bare=dict(beak="#3a3a40", beak_tip="#2a2a30", lower="#3a3a40", base_skin="#b4aea8", base_skin_len=0.22, mouth="#6a4a50",
              legs="#18181c", iris="#120e0c", eyering="#18181c", beak_rough=0.4),
))
species("HoodedCrow", dict(CORVID, body_w=1.12, beak_len=1.3, beak_depth=2.0, beak_width=1.3, beak_curve=0.0016, tail=1.05, gloss_hue=0.6,
                           tarsus=1.2), dict(
    styles={
        "black": st("#0c0c0f", ("down", "#1c1c20"), gloss=0.55, rough=0.38),
        "grey": st("#8f8c88", ("down", "#5a5856"), ("shaft", "#6a6662"), ("streak", "#7e7b78", 0.12), rough=0.6),
        "flight": st("#0b0b0e", gloss=0.5, rough=0.38),
        "under": st("#2a2a2e"),
    },
    regions={"upper": "grey", "under": "grey", "head": "black", "throat": "black", "chin": "black", "lcovert": "black", "mcovert": "black",
             "gcovert": "black", "pcovert": "black", "alula": "black", "primary": "flight", "tertial": "flight", "tail": "flight",
             "underwing": "under", "trousers": "black", "nape": "black"},
    rules=[(lambda q: q.region == "breast" and q.z > 0.022, "black"), (lambda q: q.region in ("neckside",) and q.z > 0.04, "black")],
    bare=dict(beak="#141416", legs="#141416", iris="#120e0c", eyering="#141416", mouth="#5a4048"),
))
species("Jackdaw", dict(CORVID, body_len=0.96, head=1.18, crown_h=1.08, flat_crown=0.2, beak_len=0.82, beak_depth=1.5, beak_width=1.25,
                        beak_curve=0.0008, tail=1.05, eye=1.25, gloss_hue=0.6, tarsus=1.1), dict(
    styles={
        "black": st("#121216", gloss=0.45, rough=0.4),
        "slate": st("#2c2d33", ("down", "#1c1c22"), gloss=0.15, rough=0.5),
        "silver": st("#8a8c94", ("down", "#4a4a50"), ("grad", "#a2a4ac", 0.5), rough=0.55),
        "flight": st("#101014", gloss=0.5, rough=0.38),
        "under": st("#3a3a42"),
    },
    regions={"upper": "slate", "under": "slate", "head": "black", "nape": "silver", "neckside": "silver", "ear": "silver",
             "lcovert": "black", "mcovert": "black", "gcovert": "black", "pcovert": "black", "primary": "flight", "tail": "flight",
             "underwing": "under"},
    rules=[(lambda q: q.head and q.region == "ear" and q.dz > -0.003, "black"), (lambda q: q.region == "eyestripe" and q.dz < -0.004, "silver")],
    bare=dict(beak="#141416", legs="#141416", iris="#e8e6dc", eyering="#2a2a30", mouth="#5a4048"),
))
species("Magpie", dict(CORVID, body_len=0.95, body_w=1.0, beak_len=1.1, beak_depth=1.6, beak_curve=0.0012, arm=0.9, hand=0.85, prim_profile=ROUNDED,
                       tail=2.65, tail_shape="wedge", fork=0.62, tail_width=1.05, tail_fan=0.7, gloss_hue=0.35, perch_tail=4, tail_flick=1.6,
                       fingers=0.6), dict(
    styles={
        "black": st("#0b0c10", ("down", "#1a1a1e"), gloss=0.45, rough=0.38),
        "white": st("#f2f2ee", ("down", "#c8c8c4")),
        "wing": st("#0e1622", gloss=0.95, rough=0.32),
        "prim": st("#0e1622", ("inner", "#f2f2ee"), ("tip", "#0e1622", 0.16), ("outer", "#0e1622"), gloss=0.9, rough=0.32),
        "tail": st("#101a14", ("shaft", "#0a0a0c"), gloss=1.0, rough=0.3),
        "grey_rump": st("#6a6a70"),
        "under": st("#2a2a30"),
    },
    regions={"upper": "black", "under": "white", "head": "black", "throat": "black", "chin": "black", "vent": "black", "undertail": "black",
             "trousers": "black", "scapular": "white", "lcovert": "wing", "mcovert": "wing", "gcovert": "wing", "pcovert": "wing", "alula": "black",
             "primary": "prim", "secondary": "wing", "tertial": "wing", "tail": "tail", "rump": "grey_rump", "underwing": "under"},
    rules=[(lambda q: q.region in ("breast",) and q.z > 0.020, "black"), (lambda q: q.region == "breastside" and q.z > 0.022, "black")],
    bare=dict(beak="#0e0e10", legs="#0e0e10", iris="#160e0a", eyering="#16161a", mouth="#4a3a40"),
))

# ---------------------------------------------------------------- Pigeons
PIGEON = dict(body_w=1.16, body_h=1.12, belly=1.1, head=0.82, neck=1.0, crown_h=1.0, flat_crown=0.6, beak_len=0.72, beak_depth=0.75,
              beak_width=0.8, beak_taper=0.6, arm=1.05, hand=1.05, prim_profile=LONG_POINTED, tail=1.12, tail_shape="square", tarsus=0.72,
              leg_thick=1.3, toes=1.05, fluff=1.1, perch_pitch=9, perch_neck=0, perch_head=-2, perch_tail=-4, head_bob=0.009, glide_elev=16,
              peck_depth=34, call_gape=4, gloss_hue=0.3)
species("FeralPigeon", dict(PIGEON), dict(
    styles={
        "grey": st("#8a8f9c", ("down", "#6a6e78"), rough=0.55),
        "head": st("#5e6372", rough=0.5),
        "neck": st("#4e5a5c", gloss=1.0, rough=0.3),
        "wing": st("#a2a7b2", rough=0.55),
        "bar": st("#a2a7b2", ("tip", "#202228", 0.32), rough=0.55),
        "sec": st("#8a909c", ("band", "#202228", 0.05, 0.42), ("tip", "#8a909c", 0.05)),
        "prim": st("#7e8490", ("grad", "#2a2c32", 0.9)),
        "tail": st("#7e8490", ("tip", "#26282e", 0.18), ("band", "#7e8490", 0.0, 0.03)),
        "tail_outer": st("#7e8490", ("outer", "#e6e6e6"), ("tip", "#26282e", 0.18)),
        "rump": st("#e4e4e6"),
        "under": st("#d2d6dc"),
    },
    regions={"upper": "grey", "under": "grey", "head": "head", "nape": "neck", "neckside": "neck", "throat": "neck", "mantle": "grey",
             "lcovert": "wing", "mcovert": "wing", "gcovert": "bar", "pcovert": "wing", "scapular": "wing", "primary": "prim", "secondary": "sec",
             "tertial": "sec", "tail": ["tail"] * 5 + ["tail_outer"], "rump": "rump", "underwing": "under"},
    rules=[(lambda q: q.region == "breast" and q.z > 0.026, "neck")],
    bare=dict(beak="#3a3a40", beak_tip="#2a2a2e", cere="#ece8e0", legs="#c8505a", iris="#e08a20", eyering="#6a6070", mouth="#c87a80"),
))
species("WoodPigeon", dict(PIGEON, body_len=1.06, tail=1.3, head=0.8), dict(
    styles={
        "grey": st("#7d8191", ("down", "#5a5e6a"), rough=0.55),
        "head": st("#7a8090"),
        "breast": st("#b48c8e", ("grad", "#c49a9a", 0.4)),
        "neck": st("#5a6a5a", gloss=1.0, rough=0.3),
        "neckpatch": st("#f4f2ee"),
        "wing": st("#6e7282"),
        "whitebar": st("#f4f2ee", ("fringe", "#dcdcd8", 0.2, "all")),
        "prim": st("#3a3c44", ("fringe", "#e8e8e4", 0.08, "outer")),
        "sec": st("#4a4e5a"),
        "tail": st("#6e7282", ("tip", "#26282e", 0.26)),
        "belly": st("#c4c0c8"),
        "under": st("#8e92a0"),
    },
    regions={"upper": "grey", "under": "belly", "breast": "breast", "breastside": "breast", "head": "head", "nape": "neck", "throat": "head",
             "lcovert": "wing", "mcovert": "wing", "gcovert": "wing", "pcovert": "whitebar", "alula": "whitebar", "primary": "prim",
             "secondary": "sec", "tertial": "wing", "tail": "tail", "underwing": "under"},
    rules=[(lambda q: q.region in ("neckside",) and q.z > 0.040 and q.z < 0.048, "neckpatch"),
           (lambda q: q.region in ("neckside", "nape") and q.z >= 0.048, "neck")],
    bare=dict(beak="#d86a6a", beak_tip="#f0d060", tip_from=0.45, legs="#c8505a", iris="#e8e0b0", eyering="#8a8a90", mouth="#d08080"),
))

# ---------------------------------------------------------------- Gulls
GULL = dict(body_len=1.08, body_w=1.08, body_h=1.15, crown_h=1.05, throat=1.15, flat_crown=0.4, beak_len=1.4, beak_depth=1.55, beak_width=1.0,
            beak_taper=0.7, beak_round=1.4, beak_hook=0.5, beak_gonys=0.55, arm=1.0, hand=1.3, prim=1.45, prim_profile=LONG_POINTED, prim_width=0.9, sec=0.85,
            tail=0.82, tail_shape="square", tarsus=1.0, leg_thick=1.3, toes=1.25, hallux=0.4, webbed=True, fluff=1.0, perch_pitch=8, head=1.1, neck=0.85,
            perch_neck=-4, perch_tail=-2, glide_elev=4, glide_sweep=-8, glide_hand=-14, glide_handelev=-6, call_gape=40, eyering_w=0.6)


def gull_prims(grey, black, white="#f6f6f2", mirrors=2, inner=5):
    """p1..p9: grey inner primaries with white tips; outer ones with a black tip and white mirrors on the outermost."""
    out = []
    for i in range(9):
        if i < inner:
            out.append(f"p_grey")
        elif i >= 9 - mirrors:
            out.append(f"p_mirror")
        else:
            out.append(f"p_black")
    return out


def gull_styles(grey, black, extra=None):
    d = {
        "white": st("#f6f6f2", ("down", "#d6d6d2"), rough=0.6),
        "grey": st(grey, ("down", "#8a9098"), rough=0.55),
        "p_grey": st(grey, ("tip", "#f6f6f2", 0.1)),
        "p_black": st(grey, ("tip", black, 0.42), ("tip", "#f6f6f2", 0.04)),
        "p_mirror": st(grey, ("tip", black, 0.55), ("mirror", "#f6f6f2", 0.86, 0.22), ("tip", "#f6f6f2", 0.05)),
        "sec": st(grey, ("tip", "#f6f6f2", 0.16)),
        "tert": st(grey, ("tip", "#f6f6f2", 0.28)),
        "under": st("#e8eaec"),
        "tail": st("#f6f6f2"),
    }
    d.update(extra or {})
    return d


GULL_REGIONS = {"upper": "grey", "under": "white", "head": "white", "nape": "white", "rump": "white", "uppertail": "white", "tail": "tail",
                "primary": gull_prims(0, 0), "secondary": "sec", "tertial": "tert", "underwing": "under", "trousers": "white", "alula": "grey"}
species("HerringGull", dict(GULL), dict(
    styles=gull_styles("#a9b2bc", "#16161a", {"head": st("#f6f6f2", ("down", "#d6d6d2"))}),
    regions=dict(GULL_REGIONS, head="head", nape="head", neckside="head"),
    bare=dict(beak="#f2c23a", beak_tip="#f4cc4a", lower="#f2c23a", gonys="#d8302a", legs="#e0a8a0", iris="#f2e6a0", eyering="#e8a040",
              mouth="#e86a50", tip_from=0.8),
    winter=dict(styles={"head": st("#f6f6f2", ("streak", "#9a8a80", 0.22), ("down", "#c6c2be"))}, bare=dict(beak="#e8c040", gonys="#c8402a")),
))
species("CommonGull", dict(GULL, body_len=1.05, head=1.06, crown_h=1.12, flat_crown=-0.2, beak_len=1.05, beak_depth=0.95, beak_gonys=0.2,
                           beak_hook=0.3, eye=1.08, tarsus=1.0), dict(
    styles=gull_styles("#9ea8b4", "#16161a", {"head": st("#f6f6f2", ("down", "#d6d6d2")),
                                              "p_mirror": st("#9ea8b4", ("tip", "#16161a", 0.58), ("mirror", "#f6f6f2", 0.84, 0.3), ("tip", "#f6f6f2", 0.06))}),
    regions=dict(GULL_REGIONS, head="head", nape="head", neckside="head"),
    bare=dict(beak="#e6d55a", beak_tip="#e8d860", legs="#d8cc6a", iris="#2a1e16", eyering="#c83a2a", mouth="#e07060"),
    winter=dict(styles={"head": st("#f6f6f2", ("streak", "#9a8a80", 0.18), ("down", "#c6c2be"))}, bare=dict(beak="#d8cc6a")),
))
species("BlackHeadedGull", dict(GULL, body_len=1.05, body_w=0.92, head=0.95, beak_len=1.2, beak_depth=0.85, beak_gonys=0.05, beak_hook=0.15,
                                arm=1.3, tarsus=1.05, prim_width=0.85), dict(
    styles=gull_styles("#c3cad2", "#16161a", {
        "hood": st("#4a3226", ("down", "#3a2a22")),
        "hood_ear": st("#4a3226"),
        "eyelid": st("#f6f6f2"),
        "p_grey": st("#c3cad2", ("tip", "#f6f6f2", 0.06)),
        "p_black": st("#f2f2ee", ("tip", "#16161a", 0.22), ("inner", "#c3cad2")),
        "p_mirror": st("#f6f6f2", ("tip", "#16161a", 0.16), ("outer", "#f6f6f2")),
        "pcov": st("#f6f6f2")}),
    regions=dict(GULL_REGIONS, head="hood", crown="hood", nape="white", neckside="white", ear="hood_ear", pcovert="pcov", alula="pcov"),
    rules=[(lambda q: q.head and q.region == "supercilium" and -0.005 < q.dz < 0.001 and q.dy < 0.0035, "eyelid"),
           (lambda q: q.head and q.region == "ear" and q.dz < -0.002, "hood_ear"),
           (lambda q: q.head and q.dz > -0.0068 - 0.004 * max(0.0, -q.s), "hood"),
           (lambda q: q.region in ("throat",) and q.z > 0.056, "hood")],
    bare=dict(beak="#a8221e", beak_tip="#5a1414", tip_from=0.7, legs="#a82820", iris="#2a1e16", eyering="#8a1e1e", mouth="#c8504a"),
    winter=dict(styles={"hood": st("#f6f6f2", ("down", "#d6d6d2")), "hood_ear": st("#3a3a40"), "eyelid": st("#f6f6f2")},
                bare=dict(beak="#c8382e", beak_tip="#2a1414", legs="#c83a30")),
))

# ---------------------------------------------------------------- Cormorant
species("Cormorant", dict(body_len=1.35, body_w=0.95, body_h=0.95, head=1.0, neck=2.6, neck_up=0.008, crown_h=0.95, flat_crown=0.8,
                          beak_len=2.2, beak_depth=0.8, beak_width=0.8, beak_taper=0.35, beak_hook=1.0, arm=1.15, hand=0.95, prim_profile=FINGERED,
                          feather_shapes={"primary": {"emarg": 0.3}}, fingers=0.6, tail=1.5, tail_shape="round", tail_width=0.9, tarsus=0.6,
                          leg_thick=1.8, toes=1.8, hallux=1.4, webbed=True, fluff=0.85, perch_pitch=48, perch_neck=-22, perch_head=-8,
                          perch_tail=-30, peck_depth=10, gloss_hue=0.38, call_gape=30, eye=0.95), dict(
    styles={
        "black": st("#0c0e0e", ("down", "#1a1a1a"), gloss=0.5, rough=0.4),
        "scale": st("#4a4630", ("fringe", "#0a0a0a", 0.22, "all"), gloss=0.4, rough=0.38),
        "white": st("#e8e2d4"),
        "gular": st("#d8a830", bare=True, skin="#d8a830"),
        "flight": st("#141412", gloss=0.3),
        "under": st("#1a1a1a"),
    },
    regions={"upper": "black", "under": "black", "mantle": "scale", "back": "scale", "scapular": "scale", "lcovert": "scale", "mcovert": "scale",
             "gcovert": "scale", "tertial": "scale", "primary": "flight", "secondary": "flight", "tail": "flight", "underwing": "under"},
    rules=[(lambda q: q.head and q.region in ("lore",) and q.dz > 0.004, "gular"), (lambda q: q.head and q.region == "chin", "gular"),
           (lambda q: q.head and q.region in ("malar", "throat", "ear") and q.dy < -0.002, "white")],
    bare=dict(beak="#3a342a", beak_tip="#e8d8b0", lower="#c8a848", tip_from=0.6, legs="#141414", iris="#2a8a6a", eyering="#2a3a3a", mouth="#e0c060"),
))

# ---------------------------------------------------------------- Tits
species("BlueTit", dict(get("GreatTit")["morph"], body_len=0.78, head=1.2, beak_len=0.32, beak_depth=0.9, tail=1.0), dict(
    styles={
        "blue": st("#3d7ccc", ("down", "#2a3a5a"), rough=0.45),
        "white": st("#f4f4ee", ("down", "#9a9a98")),
        "navy": st("#1b2338", ("down", "#14182a")),
        "yellow": st("#e2cf38", ("down", "#5a5a48"), ("grad", "#e8d850", 0.3)),
        "mantle": st("#7c9a5a"),
        "flight": st("#3a5a8a", ("fringe", "#5e8ad0", 0.25, "outer")),
        "tert": st("#3a4a6a", ("fringe", "#e8eef4", 0.12, "tip")),
        "gcovert": st("#3a62a8", ("tip", "#f4f4ee", 0.3)),
        "covert": st("#3e74c0"),
        "tail": st("#3a5a90", ("fringe", "#5e8ad0", 0.15, "outer")),
        "underwing": st("#d8d8d4"),
        "vent": st("#f0f0e4"),
    },
    regions={"upper": "mantle", "head": "white", "crown": "blue", "forehead": "white", "supercilium": "white", "eyestripe": "navy", "lore": "navy",
             "ear": "white", "malar": "white", "nape": "navy", "neckside": "navy", "chin": "navy", "throat": "navy", "under": "yellow", "vent": "vent",
             "lcovert": "covert", "mcovert": "covert", "gcovert": "gcovert", "pcovert": "covert", "primary": "flight", "tertial": "tert", "tail": "tail",
             "underwing": "underwing", "rump": "mantle", "trousers": "mantle"},
    rules=[(lambda q: q.region == "nape" and q.z > 0.058 and q.s > 0.7, "blue"),
           (lambda q: q.head and q.region in ("malar",) and q.dy < -0.0072, "navy"),
           (lambda q: q.head and q.region == "throat" and q.z < 0.062 and q.c > 0.4, "white"),
           (lambda q: q.z < 0.02 and q.z > -0.02 and q.s < -0.7 and q.c < 0.15, "navy")],
    bare=dict(beak="#1a1c22", legs="#5a6a8a", iris="#120c0a", eyering="#16161a", mouth="#d07a6a"),
))

# ---------------------------------------------------------------- Finches and sparrows
FINCH = dict(body_len=0.85, body_w=1.06, body_h=1.04, head=1.12, neck=0.75, beak_len=0.62, beak_depth=1.25, beak_width=1.2, beak_taper=0.9, beak_round=1.8,
             arm=0.88, hand=0.85, prim=0.88, prim_profile=ROUNDED, tail=1.0, tail_shape="notch", tarsus=0.8, leg_thick=0.9, perch_low=0.010, fluff=1.15,
             perch_pitch=24, perch_neck=-8, peck_depth=40, tail_flick=1.2)
species("Goldfinch", dict(FINCH, beak_len=0.66, beak_depth=1.12, beak_width=1.0, beak_taper=0.8, beak_round=1.5, tail=0.95, tail_shape="fork", fork=0.12), dict(
    styles={
        "red": st("#c8202a", ("down", "#6a2a2a"), rough=0.5),
        "black": st("#0c0c0e", ("down", "#2a2a2c")),
        "white": st("#f2f0ea", ("down", "#a8a6a0")),
        "buff": st("#b89470", ("down", "#6a5a4a")),
        "flank": st("#c8a888"),
        "prim": st("#0e0e10", ("base", "#f0c81e", 0.4), ("tip", "#f4f2ec", 0.1)),
        "sec": st("#0e0e10", ("base", "#f0c81e", 0.55), ("tip", "#f4f2ec", 0.16)),
        "tert": st("#0e0e10", ("tip", "#f4f2ec", 0.22)),
        "gcovert": st("#e8c020", ("tip", "#e8c020", 0.1)),
        "covert": st("#0e0e10"),
        "tail": st("#0e0e10", ("tip", "#f4f2ec", 0.12)),
        "tail_spot": st("#0e0e10", ("spot", "#f4f2ec", 0.32, 0.72)),
        "rump": st("#f0ece0"),
        "under": st("#d8d4cc"),
    },
    regions={"upper": "buff", "under": "white", "breastside": "flank", "flank": "flank", "head": "white", "crown": "black", "nape": "black",
             "forehead": "red", "lore": "red", "chin": "red", "ear": "white", "malar": "white", "throat": "white", "supercilium": "red",
             "eyestripe": "white", "neckside": "black", "rump": "rump", "lcovert": "covert", "mcovert": "covert", "gcovert": "gcovert",
             "pcovert": "covert", "alula": "covert", "primary": "prim", "secondary": "sec", "tertial": "tert",
             "tail": ["tail", "tail", "tail", "tail_spot", "tail_spot", "tail_spot"], "underwing": "under", "trousers": "flank"},
    rules=[(lambda q: q.head and q.region in ("eyestripe", "supercilium", "ear") and q.dz > -0.0015, "red"),
           (lambda q: q.head and q.region in ("malar", "throat") and q.z > 0.070, "red"),
           (lambda q: q.head and q.region in ("ear", "eyestripe") and q.dz < -0.0075, "black"),
           (lambda q: q.region == "neckside" and q.s < -0.1, "white")],
    bare=dict(beak="#e8cfc0", beak_tip="#4a3a34", tip_from=0.75, lower="#e8cfc0", legs="#c8a090", iris="#120c0a", eyering="#1a1a1c", mouth="#d08080"),
))
species("Chaffinch", dict(FINCH, beak_len=0.72, beak_depth=1.05, beak_width=1.0, beak_taper=0.85, tail=1.12, body_len=0.92), dict(
    styles={
        "bluegrey": st("#4f6e8c", ("down", "#3a4a5a")),
        "pink": st("#c87a62", ("down", "#7a5a50"), ("grad", "#d48c74", 0.3)),
        "belly": st("#efe6e0"),
        "mantle": st("#7a5a44"),
        "rump": st("#6a8c5a"),
        "white": st("#f2f0ea"),
        "prim": st("#1a1a1e", ("fringe", "#e0d8a8", 0.12, "outer"), ("base", "#f2f0ea", 0.16)),
        "sec": st("#1a1a1e", ("fringe", "#d8d0a0", 0.14, "outer"), ("base", "#f2f0ea", 0.22)),
        "tert": st("#1a1a1e", ("fringe", "#c8b088", 0.18, "all")),
        "gcovert": st("#1a1a1e", ("tip", "#f0ecd8", 0.4)),
        "mcovert": st("#f2f0ea"),
        "tail": st("#2a2a2e", ("fringe", "#5a6a6a", 0.1, "outer")),
        "tail_outer": st("#2a2a2e", ("outer", "#f2f0ea"), ("base", "#f2f0ea", 0.6)),
        "under": st("#e4e0dc"),
    },
    regions={"upper": "mantle", "under": "belly", "head": "bluegrey", "nape": "bluegrey", "ear": "pink", "malar": "pink", "lore": "pink",
             "eyestripe": "pink", "chin": "pink", "throat": "pink", "breast": "pink", "breastside": "pink", "rump": "rump", "uppertail": "rump",
             "lcovert": "mcovert", "mcovert": "mcovert", "gcovert": "gcovert", "pcovert": "tert", "alula": "tert", "primary": "prim",
             "secondary": "sec", "tertial": "tert", "tail": ["tail", "tail", "tail", "tail", "tail_outer", "tail_outer"], "underwing": "under",
             "forehead": "bluegrey", "supercilium": "bluegrey", "neckside": "bluegrey"},
    rules=[(lambda q: q.head and q.region == "forehead" and q.z > 0.0775, "pink")],
    bare=dict(beak="#6a7a8c", beak_tip="#4a5a6c", legs="#b89080", iris="#120c0a", eyering="#2a2420", mouth="#d08080"),
))
species("Bullfinch", dict(FINCH, body_len=0.92, body_w=1.14, body_h=1.12, belly=1.08, head=1.22, neck=0.62, beak_len=0.5, beak_depth=1.55,
                          beak_width=1.35, beak_taper=1.25, beak_curve=0.0012, tail=1.05, tail_shape="square", fluff=1.25), dict(
    styles={
        "black": st("#0d0d10", ("down", "#2a2a2c"), gloss=0.2),
        "red": st("#d8484a", ("down", "#7a4a4a"), ("grad", "#e05a54", 0.25)),
        "grey": st("#7e8794", ("down", "#5a5e66")),
        "white": st("#f4f4f0"),
        "prim": st("#16161a", gloss=0.3),
        "sec": st("#16161a", ("fringe", "#3a3a40", 0.1, "outer"), gloss=0.4),
        "gcovert": st("#16161a", ("tip", "#c8c8c4", 0.55)),
        "tail": st("#0e0e12", gloss=0.6),
        "under": st("#c8c8c4"),
    },
    regions={"upper": "grey", "under": "red", "head": "red", "crown": "black", "forehead": "black", "nape": "black", "lore": "black", "chin": "black",
             "vent": "white", "undertail": "white", "rump": "white", "uppertail": "white", "lcovert": "grey", "mcovert": "grey", "gcovert": "gcovert",
             "pcovert": "prim", "alula": "prim", "primary": "prim", "secondary": "sec", "tertial": "sec", "tail": "tail", "underwing": "under",
             "neckside": "grey"},
    rules=[(lambda q: q.head and q.region in ("supercilium", "eyestripe") and q.dy > 0.0005, "black"),
           (lambda q: q.head and q.region == "malar" and q.z > 0.074, "black"), (lambda q: q.head and q.region == "throat" and q.z > 0.068, "black")],
    bare=dict(beak="#141416", legs="#4a3a34", iris="#120c0a", eyering="#16161a", mouth="#a05a5a"),
))
SPARROW = dict(FINCH, body_len=0.9, body_w=1.08, head=1.1, beak_len=0.7, beak_depth=1.3, beak_width=1.2, beak_taper=0.95, tail=1.05,
               tail_shape="notch", tarsus=0.95, fluff=1.15, perch_pitch=20)
species("HouseSparrow", dict(SPARROW), dict(
    styles={
        "crown": st("#7c7e80", ("down", "#5a5a5a")),
        "chestnut": st("#8a5232", ("down", "#5a3a2a")),
        "black": st("#0e0e10", ("fringe", "#2a2a2a", 0.1, "tip")),
        "cheek": st("#c9c6bf"),
        "under": st("#b8b6b0", ("down", "#8a8884")),
        "mantle": st("#8a6448", ("streak", "#1a1410", 0.45), ("fringe", "#c8a070", 0.18, "all")),
        "rump": st("#8a8680"),
        "prim": st("#3a2e26", ("fringe", "#b08860", 0.14, "outer")),
        "sec": st("#3a2e26", ("fringe", "#b08458", 0.18, "outer")),
        "tert": st("#2a221c", ("fringe", "#c8a070", 0.22, "all")),
        "gcovert": st("#3a2a20", ("fringe", "#b8865a", 0.26, "all")),
        "mcovert": st("#f2efe6", ("base", "#5a3a2a", 0.3)),
        "lcovert": st("#8a5232"),
        "tail": st("#5a4a3e", ("fringe", "#9a8064", 0.12, "outer")),
        "underwing": st("#c8c0b0"),
    },
    regions={"upper": "mantle", "under": "under", "head": "crown", "crown": "crown", "forehead": "crown", "nape": "chestnut", "neckside": "chestnut",
             "supercilium": "chestnut", "eyestripe": "chestnut", "ear": "cheek", "malar": "cheek", "lore": "black", "chin": "black",
             "throat": "black", "rump": "rump", "uppertail": "rump", "lcovert": "lcovert", "mcovert": "mcovert", "gcovert": "gcovert",
             "pcovert": "prim", "alula": "prim", "primary": "prim", "secondary": "sec", "tertial": "tert", "tail": "tail", "underwing": "underwing",
             "scapular": "mantle"},
    rules=[(lambda q: q.head and q.region == "supercilium" and q.dz > -0.002, "crown"),
           (lambda q: q.region == "breast" and q.z > 0.024 and q.c < 0.5, "black")],
    bare=dict(beak="#1a1a1c", legs="#b8907a", iris="#1a120c", eyering="#2a2420", mouth="#d08a7a"),
))
species("TreeSparrow", dict(SPARROW, body_len=0.86, head=1.12), dict(
    styles={
        "cap": st("#7a3e22", ("down", "#4a2a1a")),
        "white": st("#f2efe6"),
        "black": st("#0e0e10"),
        "under": st("#c9bfae", ("down", "#8a8478")),
        "mantle": st("#a07a56", ("streak", "#1a1410", 0.45), ("fringe", "#c8a878", 0.18, "all")),
        "rump": st("#a88a68"),
        "prim": st("#3a2e26", ("fringe", "#b08860", 0.14, "outer")),
        "tert": st("#2a221c", ("fringe", "#c8a070", 0.22, "all")),
        "gcovert": st("#3a2a20", ("tip", "#f2efe6", 0.28), ("fringe", "#b8865a", 0.2, "outer")),
        "mcovert": st("#f2efe6", ("base", "#5a3a2a", 0.3)),
        "lcovert": st("#8a4e2a"),
        "tail": st("#5a4a3e", ("fringe", "#9a8064", 0.12, "outer")),
        "underwing": st("#c8c0b0"),
    },
    regions={"upper": "mantle", "under": "under", "head": "cap", "ear": "white", "malar": "white", "eyestripe": "white", "neckside": "white",
             "lore": "black", "chin": "black", "throat": "under", "rump": "rump", "uppertail": "rump", "lcovert": "lcovert", "mcovert": "mcovert",
             "gcovert": "gcovert", "pcovert": "prim", "primary": "prim", "secondary": "prim", "tertial": "tert", "tail": "tail",
             "underwing": "underwing", "scapular": "mantle"},
    rules=[(lambda q: q.head and q.region == "ear" and -0.0075 < q.dz < -0.003 and q.dy < -0.002, "black"),
           (lambda q: q.head and q.region == "throat" and q.z > 0.060, "black"),
           (lambda q: q.region == "nape" and q.z < 0.052 and q.s < 0.8, "white")],
    bare=dict(beak="#1a1a1c", legs="#b8907a", iris="#1a120c", eyering="#2a2420", mouth="#d08a7a"),
))

# ---------------------------------------------------------------- Thrushes and chats
THRUSH = dict(body_len=1.0, body_w=1.0, head=1.02, beak_len=0.95, beak_depth=0.9, beak_width=0.85, beak_curve=0.0004, arm=0.95, hand=0.92,
              prim_profile=ROUNDED, tail=1.12, tail_shape="square", tarsus=1.25, leg_thick=1.0, toes=1.05, fluff=1.05, perch_pitch=30,
              perch_neck=-4, perch_tail=-14, peck_depth=46, eye=1.3, call_gape=26)
species("SongThrush", dict(THRUSH), dict(
    styles={
        "brown": st("#8a6c4e", ("down", "#5a4a3a")),
        "breast": st("#ead3a0", ("spot", "#3a2a1c", 0.17, 0.8), ("down", "#8a8070")),
        "belly": st("#f4ecdc", ("spot", "#3a2a1c", 0.14, 0.82)),
        "vent": st("#f4ecdc"),
        "face": st("#c8a878", ("streak", "#6a5038", 0.3)),
        "malar": st("#3a2a1c"),
        "prim": st("#6a5038", ("fringe", "#9a7a58", 0.14, "outer")),
        "gcovert": st("#7a5c40", ("tip", "#d8b888", 0.14)),
        "tail": st("#7a5e44"),
        "underwing": st("#e4b070"),
    },
    regions={"upper": "brown", "under": "belly", "breast": "breast", "breastside": "breast", "throat": "vent", "chin": "vent", "vent": "vent",
             "undertail": "vent", "ear": "face", "lore": "face", "malar": "face", "eyestripe": "face", "head": "brown", "lcovert": "brown",
             "mcovert": "brown", "gcovert": "gcovert", "primary": "prim", "tertial": "prim", "tail": "tail", "underwing": "underwing",
             "trousers": "belly", "flank": "belly"},
    rules=[(lambda q: q.head and q.region in ("malar", "throat") and -0.0098 < q.dy < -0.0078 and q.dz > -0.004, "malar")],
    bare=dict(beak="#4a3a2a", lower="#c8a868", lower_tip="#4a3a2a", tip_from=0.5, legs="#d8b0a0", iris="#1a120c", eyering="#3a2a20", mouth="#e0b050"),
))
species("Fieldfare", dict(THRUSH, body_len=1.04, tail=1.2), dict(
    styles={
        "grey": st("#8c919b", ("down", "#5a5e66")),
        "chestnut": st("#7a4c32", ("down", "#4a3a2a")),
        "breast": st("#d8a650", ("arrow", "#2a2220", 0.28, 0.66), ("down", "#8a7a60")),
        "flank": st("#f2eee6", ("arrow", "#2a2220", 0.32, 0.64)),
        "belly": st("#f2eee6"),
        "supercil": st("#e8e4dc"),
        "throat": st("#e8c888", ("streak", "#2a2220", 0.3)),
        "prim": st("#3a2e2a", ("fringe", "#6a5a50", 0.12, "outer")),
        "tert": st("#4a362a", ("fringe", "#8a6a50", 0.14, "all")),
        "tail": st("#221c1a"),
        "underwing": st("#f2eee6"),
    },
    regions={"upper": "chestnut", "under": "belly", "head": "grey", "nape": "grey", "neckside": "grey", "rump": "grey", "uppertail": "grey",
             "breast": "breast", "breastside": "breast", "flank": "flank", "throat": "throat", "chin": "throat", "supercilium": "supercil",
             "lcovert": "chestnut", "mcovert": "chestnut", "gcovert": "tert", "pcovert": "prim", "primary": "prim", "tertial": "tert", "tail": "tail",
             "underwing": "underwing", "lore": "chestnut", "trousers": "belly"},
    bare=dict(beak="#e8b830", beak_tip="#2a2220", tip_from=0.65, legs="#3a2e2a", iris="#1a120c", eyering="#3a2a20", mouth="#e0a040"),
))
species("Blackbird", dict(THRUSH, body_len=1.02, tail=1.3, tail_shape="round", perch_tail=-6, tail_flick=1.6, eyering_w=0.9, beak_depth=1.0), dict(
    styles={
        "black": st("#0e0d0d", ("down", "#1a1a1a"), gloss=0.15, rough=0.5),
        "throat": st("#0e0d0d", ("down", "#1a1a1a"), gloss=0.15, rough=0.5),
        "flight": st("#0e0d0d", ("fringe", "#1e1c1a", 0.12, "outer"), gloss=0.1),
        "under": st("#1a1818"),
    },
    regions={"upper": "black", "under": "black", "throat": "throat", "chin": "throat", "primary": "flight", "tail": "flight", "underwing": "under"},
    bare=dict(beak="#f0a020", beak_tip="#f4b030", legs="#3a2e26", iris="#1a120c", eyering="#f0a020", mouth="#e8a030"),
))
species("Robin", dict(THRUSH, body_len=0.82, body_w=1.08, body_h=1.08, head=1.15, neck=0.7, beak_len=0.68, beak_depth=0.85, beak_width=0.8,
                      tail=1.05, tarsus=1.2, eye=1.42, fluff=1.25, perch_pitch=28), dict(
    styles={
        "orange": st("#e2702e", ("down", "#8a5a3a"), ("grad", "#e8803a", 0.3)),
        "olive": st("#7e6a4c", ("down", "#5a4a3a")),
        "grey": st("#a7aeb4"),
        "belly": st("#f2eee6"),
        "flank": st("#c8b490"),
        "prim": st("#5a4a36", ("fringe", "#8a7454", 0.16, "outer")),
        "gcovert": st("#6a5840", ("tip", "#c8a870", 0.12)),
        "tail": st("#6a5038"),
        "underwing": st("#d8c4a4"),
    },
    regions={"upper": "olive", "under": "belly", "head": "olive", "forehead": "orange", "lore": "orange", "ear": "orange", "malar": "orange",
             "eyestripe": "orange", "supercilium": "orange", "chin": "orange", "throat": "orange", "breast": "orange", "breastside": "grey",
             "flank": "flank", "neckside": "grey", "lcovert": "olive", "mcovert": "olive", "gcovert": "gcovert", "primary": "prim", "tertial": "prim",
             "tail": "tail", "underwing": "underwing", "trousers": "flank"},
    rules=[(lambda q: q.head and q.region == "supercilium" and q.dz < -0.004, "grey"),
           (lambda q: q.region == "breast" and q.z < 0.010, "belly"),
           (lambda q: q.region in ("breastside", "neckside") and q.s < 0.05 and q.z > 0.008, "orange"),
           (lambda q: q.region in ("breastside", "neckside") and q.s >= 0.05 and q.s < 0.3, "grey"),
           (lambda q: q.region in ("breastside", "neckside") and q.s >= 0.3, "olive")],
    bare=dict(beak="#2a2420", legs="#b89078", iris="#0c0806", eyering="#2a2420", mouth="#e8b040"),
))

# ---------------------------------------------------------------- Wagtail, waxwing
species("WhiteWagtail", dict(body_len=0.95, body_w=0.85, body_h=0.92, head=0.95, beak_len=0.85, beak_depth=0.7, beak_width=0.7, arm=0.95,
                             hand=0.92, prim_profile=ROUNDED, tail=1.95, tail_shape="square", tail_width=0.85, tarsus=1.3, leg_thick=0.85, toes=1.1,
                             fluff=0.95, perch_pitch=14, perch_tail=4, tail_wag=True, head_bob=0.004), dict(
    styles={
        "grey": st("#8e939a", ("down", "#5a5e66")),
        "black": st("#0e0e10", ("down", "#2a2a2c")),
        "cap": st("#0e0e10", ("down", "#2a2a2c")),
        "white": st("#f6f6f2", ("down", "#b8b8b4")),
        "prim": st("#1e1e22", ("fringe", "#d8d8d4", 0.08, "outer")),
        "sec": st("#1e1e22", ("fringe", "#e8e8e4", 0.2, "all")),
        "gcovert": st("#1e1e22", ("fringe", "#f2f2ee", 0.42, "all")),
        "tail": st("#141416"),
        "tail_outer": st("#f6f6f2"),
        "under": st("#e8e8e4"),
    },
    regions={"upper": "grey", "under": "white", "head": "white", "crown": "cap", "nape": "cap", "neckside": "black", "chin": "black",
             "throat": "black", "lcovert": "grey", "mcovert": "gcovert", "gcovert": "gcovert", "pcovert": "prim", "primary": "prim",
             "secondary": "sec", "tertial": "sec", "tail": ["tail", "tail", "tail", "tail", "tail_outer", "tail_outer"], "underwing": "under",
             "rump": "black", "uppertail": "black"},
    rules=[(lambda q: q.head and q.region in ("crown",) and q.z > 0.068, "white"), (lambda q: q.region == "breast" and q.z > 0.020, "black"),
           (lambda q: q.head and q.region == "nape" and q.s < 0.6 and q.z > 0.056, "white")],
    bare=dict(beak="#141416", legs="#141416", iris="#120c0a", eyering="#16161a", mouth="#c87a6a"),
))
species("Waxwing", dict(body_len=0.95, body_w=1.08, body_h=1.06, head=1.05, beak_len=0.6, beak_depth=1.0, beak_width=1.15, beak_taper=0.9,
                        arm=1.0, hand=1.0, prim_profile=POINTED, tail=0.85, tail_shape="square", tarsus=0.8, fluff=1.2, crest=1.8, perch_pitch=26,
                        perch_neck=-6), dict(
    styles={
        "pinkbrown": st("#b89082", ("down", "#7a6058")),
        "crest": st("#c49a88"),
        "black": st("#0e0e10"),
        "chestnut": st("#8c4a3a"),
        "grey": st("#8a8a90"),
        "prim": st("#16161a", ("fringe", "#f0d030", 0.22, "outer"), ("tip", "#f4f0e0", 0.06)),
        "sec": st("#2a2a2e", ("tip", "#c8202a", 0.07), ("fringe", "#f2f0ea", 0.08, "tip")),
        "pcov": st("#16161a", ("tip", "#f2f0ea", 0.25)),
        "tail": st("#5a5a60", ("tip", "#f0d030", 0.16), ("band", "#16161a", 0.16, 0.34)),
        "under": st("#c8c0bc"),
    },
    regions={"upper": "pinkbrown", "under": "pinkbrown", "crest": "crest", "head": "pinkbrown", "lore": "black", "eyestripe": "black", "chin": "black",
             "vent": "chestnut", "undertail": "chestnut", "rump": "grey", "uppertail": "grey", "lcovert": "pinkbrown", "mcovert": "pinkbrown",
             "gcovert": "pinkbrown", "pcovert": "pcov", "alula": "pcov", "primary": "prim", "secondary": "sec", "tertial": "pinkbrown",
             "tail": "tail", "underwing": "under"},
    rules=[(lambda q: q.head and q.region == "forehead" and q.z > 0.077, "chestnut"),
           (lambda q: q.head and q.region in ("throat",) and q.z > 0.060, "black"),
           (lambda q: q.head and q.region == "ear" and q.dz > -0.006 and q.dy > -0.002, "black")],
    bare=dict(beak="#141416", legs="#141416", iris="#2a120c", eyering="#16161a", mouth="#c86a6a"),
))

# ---------------------------------------------------------------- Swift, swallows
AERIAL = dict(body_len=0.95, body_w=0.95, body_h=0.9, head=1.05, neck=0.65, flat_crown=0.7, beak_len=0.32, beak_depth=0.7, beak_width=1.6,
              beak_taper=1.4, prim_width=0.8, tarsus=0.45, shin=0.6, leg_thick=0.8, toes=0.7, fluff=0.9, perch_pitch=12, perch_neck=-2,
              call_gape=40, eye=1.15)
species("CommonSwift", dict(AERIAL, arm=0.45, hand=1.8, prim=1.3, prim_profile=SICKLE, prim_angle0=55, prim_fan=40, sec=0.45, sec_width=0.8,
                            tail=0.8, tail_shape="fork", fork=0.5, tail_width=0.8, tarsus=0.32, toes=0.6, glide_sweep=-22, glide_hand=-30,
                            glide_elev=2, feather_shapes={"primary": {"narrow": 0.7, "pointed": 0.8}}, perch_pitch=60, perch_tail=-20), dict(
    styles={
        "sooty": st("#2a2624", ("fringe", "#3a3634", 0.12, "tip"), rough=0.5),
        "pale": st("#d2ccc4"),
        "flight": st("#221e1c", ("fringe", "#3a3632", 0.1, "outer"), gloss=0.15),
        "under": st("#2e2a28"),
    },
    regions={"upper": "sooty", "under": "sooty", "chin": "pale", "primary": "flight", "tail": "flight", "underwing": "under"},
    rules=[(lambda q: q.head and q.region in ("throat",) and q.z > 0.058, "pale")],
    bare=dict(beak="#141414", legs="#2a2422", iris="#0c0808", eyering="#1a1614", mouth="#c88a7a"),
))
species("BarnSwallow", dict(AERIAL, arm=0.75, hand=1.25, prim=1.08, prim_profile=LONG_POINTED, sec=0.75, tail=1.35, tail_shape="fork", fork=0.75,
                            streamer=1.0, tail_width=0.85, glide_sweep=-14, glide_hand=-16, gloss_hue=0.62,
                            feather_shapes={"primary": {"narrow": 0.85, "pointed": 0.5}}), dict(
    styles={
        "blue": st("#14244a", ("down", "#1a1a24"), gloss=0.9, rough=0.32),
        "red": st("#a0281e", ("down", "#5a2a24")),
        "cream": st("#f2e2d0"),
        "flight": st("#141a28", gloss=0.5),
        "tail": st("#141a28", ("spot", "#f2efe8", 0.22, 0.66), gloss=0.6),
        "tail_c": st("#141a28", gloss=0.6),
        "under": st("#d8ccbc"),
    },
    regions={"upper": "blue", "under": "cream", "head": "blue", "forehead": "red", "chin": "red", "throat": "red", "primary": "flight",
             "tertial": "flight", "tail": ["tail_c", "tail", "tail", "tail", "tail", "tail_c"], "underwing": "under"},
    rules=[(lambda q: q.head and q.region == "lore" and q.dz > 0.004, "red"), (lambda q: q.region == "breast" and q.z > 0.024, "blue"),
           (lambda q: q.region == "breastside" and q.z > 0.018, "blue")],
    bare=dict(beak="#141414", legs="#2a2422", iris="#0c0808", eyering="#1a1614", mouth="#e8c070"),
))
species("SandMartin", dict(AERIAL, body_len=0.88, arm=0.75, hand=1.15, prim=1.0, prim_profile=LONG_POINTED, sec=0.75, tail=0.95, tail_shape="fork",
                           fork=0.2, glide_sweep=-12, glide_hand=-14), dict(
    styles={
        "brown": st("#7a6a5c", ("down", "#4a4038")),
        "white": st("#f4f2ee"),
        "band": st("#8a7a6a"),
        "flight": st("#5a4e44"),
        "under": st("#8a7a6c"),
    },
    regions={"upper": "brown", "under": "white", "head": "brown", "throat": "white", "chin": "white", "primary": "flight", "tail": "flight",
             "underwing": "under", "ear": "brown", "malar": "brown"},
    rules=[(lambda q: q.region in ("breast", "breastside") and 0.014 < q.z < 0.032, "band")],
    bare=dict(beak="#141414", legs="#3a3430", iris="#0c0808", eyering="#1a1614", mouth="#e0b070"),
))

# ---------------------------------------------------------------- Raptors
RAPTOR = dict(body_w=1.05, head=1.05, flat_crown=0.8, beak_len=0.78, beak_depth=1.35, beak_width=1.05, beak_taper=0.6, beak_curve=0.0026,
              beak_hook=1.2, prim_profile=ROUNDED, tail=1.3, tail_shape="square", tarsus=1.3, leg_thick=1.15, toes=1.35, claw=1.8,
              perch_pitch=40, perch_neck=-6, perch_tail=-22, eye=1.2, call_gape=30)
species("Sparrowhawk", dict(RAPTOR, arm=0.95, hand=0.85, feather_shapes={"primary": {"emarg": 0.35}}, fingers=0.8, tail=1.5, tarsus=1.6,
                            leg_thick=0.85), dict(
    styles={
        "slate": st("#5c6a7a", ("down", "#4a5260")),
        "barred": st("#f2eee6", ("bars", "#c86a3a", 3, 0.38), ("down", "#b8b0a8")),
        "cheek": st("#c86a3a"),
        "flight": st("#5c6a7a", ("bars", "#3a4452", 4, 0.2)),
        "tail": st("#5c6a7a", ("bars", "#2a3240", 4, 0.22), ("tip", "#e8e4dc", 0.04)),
        "under": st("#e8e2d8", ("bars", "#8a6a5a", 4, 0.3)),
    },
    regions={"upper": "slate", "under": "barred", "head": "slate", "ear": "cheek", "malar": "cheek", "throat": "barred", "primary": "flight",
             "secondary": "flight", "tail": "tail", "underwing": "under"},
    bare=dict(beak="#2a3040", cere="#e8b828", legs="#e8b828", iris="#e8a020", eyering="#e8c040", mouth="#e8a0a0"),
))
species("Peregrine", dict(RAPTOR, body_w=1.12, arm=1.1, hand=1.15, prim_profile=LONG_POINTED, tail=1.05, glide_sweep=-10, glide_hand=-12), dict(
    styles={
        "slate": st("#3c4652", ("bars", "#2a3038", 2, 0.25), ("down", "#3a404a")),
        "hood": st("#16161a"),
        "white": st("#f2eee6"),
        "barred": st("#ece6dc", ("bars", "#16161a", 3, 0.22)),
        "spotted": st("#ece6dc", ("spot", "#2a2a2e", 0.18, 0.7)),
        "flight": st("#2e343e", ("bars", "#1e2228", 4, 0.18)),
        "tail": st("#3c4652", ("bars", "#1e2228", 5, 0.25), ("tip", "#e8e4dc", 0.05)),
        "under": st("#d8d2c8", ("bars", "#3a3a40", 5, 0.3)),
    },
    regions={"upper": "slate", "under": "barred", "breast": "spotted", "head": "hood", "nape": "hood", "ear": "white", "throat": "white", "chin": "white",
             "malar": "hood", "primary": "flight", "secondary": "flight", "tail": "tail", "underwing": "under"},
    rules=[(lambda q: q.head and q.region == "ear" and q.dz > -0.004 and q.dy > -0.004, "hood")],
    bare=dict(beak="#4a5a6a", beak_tip="#1a1a1e", cere="#e8b828", legs="#e8b828", iris="#1a120c", eyering="#e8c040", mouth="#e8a0a0"),
))


# ---------------------------------------------------------------- bills and heads, calibrated against photos (bill length to head length)
# (beak_len, beak_depth, beak_width, beak_round, extra)
BILLS = {
    "Starling": (1.0, 0.8, 0.8, 1.0, {}),
    "Rook": (1.12, 1.7, 1.3, 1.25, dict(beak_curve=0.0008, beak_taper=0.9)),
    "HoodedCrow": (1.05, 2.0, 1.4, 1.3, dict(beak_curve=0.0018)),
    "Jackdaw": (0.62, 1.6, 1.3, 1.4, dict(beak_curve=0.0008)),
    "Magpie": (0.82, 1.7, 1.3, 1.3, dict(beak_curve=0.0014)),
    "FeralPigeon": (0.5, 0.95, 0.9, 1.2, {}),
    "WoodPigeon": (0.5, 0.95, 0.9, 1.2, {}),
    "HerringGull": (1.12, 2.0, 1.15, 1.3, dict(beak_hook=0.9, beak_gonys=0.8)),
    "CommonGull": (0.82, 1.3, 1.0, 1.3, dict(beak_hook=0.5, beak_gonys=0.3)),
    "BlackHeadedGull": (0.92, 1.1, 0.9, 1.2, dict(beak_hook=0.3, beak_gonys=0.1)),
    "GreatTit": (0.3, 1.0, 0.95, 1.6, {}),
    "BlueTit": (0.24, 1.0, 0.95, 1.6, {}),
    "Goldfinch": (0.5, 1.5, 1.3, 1.6, dict(beak_taper=0.8)),
    "Chaffinch": (0.5, 1.5, 1.35, 1.6, {}),
    "Bullfinch": (0.32, 1.75, 1.55, 2.0, {}),
    "HouseSparrow": (0.42, 1.6, 1.4, 1.9, {}),
    "TreeSparrow": (0.38, 1.5, 1.35, 1.9, {}),
    "SongThrush": (0.6, 1.1, 1.0, 1.2, {}),
    "Fieldfare": (0.64, 1.2, 1.05, 1.2, {}),
    "Blackbird": (0.64, 1.25, 1.1, 1.2, {}),
    "Robin": (0.44, 1.0, 1.0, 1.2, {}),
    "WhiteWagtail": (0.55, 0.85, 0.85, 1.1, {}),
    "Waxwing": (0.42, 1.4, 1.5, 1.6, {}),
    "CommonSwift": (0.2, 1.0, 2.0, 1.4, {}),
    "BarnSwallow": (0.22, 1.0, 1.9, 1.4, {}),
    "SandMartin": (0.22, 0.9, 1.6, 1.4, {}),
    "Cormorant": (1.7, 1.0, 0.9, 1.1, dict(beak_hook=1.0)),
    "Sparrowhawk": (0.55, 1.9, 1.4, 1.3, dict(beak_curve=0.003, beak_hook=1.4)),
    "Peregrine": (0.55, 2.0, 1.5, 1.3, dict(beak_curve=0.003, beak_hook=1.4)),
}
# (head, neck, crown_h, throat): small birds round, big-headed and neckless; pigeons a small head on a plump breast
HEADS = {
    "GreatTit": (1.42, 0.45, 1.15, 1.2), "BlueTit": (1.45, 0.42, 1.15, 1.2),
    "Goldfinch": (1.3, 0.5, 1.12, 1.15), "Chaffinch": (1.25, 0.55, 1.1, 1.12), "Bullfinch": (1.35, 0.45, 1.12, 1.2),
    "HouseSparrow": (1.32, 0.5, 1.12, 1.15), "TreeSparrow": (1.32, 0.5, 1.12, 1.15), "Robin": (1.32, 0.5, 1.1, 1.18),
    "SongThrush": (1.15, 0.72, 1.08, 1.1), "Fieldfare": (1.15, 0.72, 1.08, 1.1), "Blackbird": (1.15, 0.72, 1.08, 1.1),
    "Rook": (1.18, 0.75, 1.12, 1.15), "HoodedCrow": (1.2, 0.75, 1.12, 1.15), "Jackdaw": (1.22, 0.7, 1.12, 1.12), "Magpie": (1.15, 0.72, 1.1, 1.1),
    "FeralPigeon": (1.05, 0.95, 1.08, 1.3), "WoodPigeon": (1.02, 0.95, 1.08, 1.3),
    "HerringGull": (1.18, 0.8, 1.08, 1.2), "CommonGull": (1.2, 0.8, 1.12, 1.15), "BlackHeadedGull": (1.12, 0.8, 1.08, 1.12),
    "WhiteWagtail": (1.12, 0.68, 1.05, 1.08), "Waxwing": (1.15, 0.6, 1.08, 1.12),
    "CommonSwift": (1.2, 0.45, 1.05, 1.12), "BarnSwallow": (1.2, 0.45, 1.05, 1.12), "SandMartin": (1.2, 0.45, 1.05, 1.12),
    "Starling": (1.05, 0.85, 1.02, 1.05), "Sparrowhawk": (1.15, 0.6, 1.05, 1.12), "Peregrine": (1.18, 0.6, 1.05, 1.15),
}
for _n, (_l, _d, _w, _r, _x) in BILLS.items():
    SPECIES[_n]["morph"].update(dict(beak_len=_l, beak_depth=_d, beak_width=_w, beak_round=_r, **_x))
for _n, (_h, _k, _c, _t) in HEADS.items():
    _k = max(_k, 0.55)   # shorter folds the neck (a seam)
    SPECIES[_n]["morph"].update(dict(head=_h, neck=_k, crown_h=_c, throat=_t))
SPECIES["Rook"]["morph"]["trousers"] = 2.2
SPECIES["Magpie"]["morph"].update(tail=3.0)
SPECIES["CommonSwift"]["morph"].update(perch_pitch=78, perch_neck=8, perch_tail=-28)
SPECIES["FeralPigeon"]["morph"].update(body_h=1.22, belly=1.05)
SPECIES["WoodPigeon"]["morph"].update(body_h=1.22, belly=1.05)

# perched poses (QA): the cormorant upright with the bill level, aerial birds on their short legs, not on their bellies
SPECIES["Cormorant"]["morph"].update(neck_thin=0.3, body_w=1.15, body_h=1.1, perch_pitch=40, perch_neck=-14, perch_head=-28, neck=2.2)
for _n in ("CommonSwift", "BarnSwallow", "SandMartin"):
    SPECIES[_n]["morph"]["perch_low"] = 0.0015
SPECIES["CommonSwift"]["morph"].update(prim=1.5, perch_neck=-30, perch_head=-34)
# gulls in winter: soft grey-brown smudging on the head, not regular streaks
for _n in ("HerringGull", "CommonGull"):
    SPECIES[_n]["plumage"]["winter"]["styles"]["head"] = st("#f2f0ec", ("mottle", "#a89a90", 0.45), ("grad", "#e0d8d0", 0.4))
SPECIES["BarnSwallow"]["morph"].update(tarsus=0.75, shin=0.85, perch_low=0.0, perch_pitch=18)
SPECIES["Waxwing"]["morph"].update(crest=2.6)
SPECIES["Waxwing"]["plumage"]["styles"]["pinkbrown"] = st("#b89082", ("mottle", "#a47e72", 0.35), ("down", "#9a786c"))

# how much loose plume shows over the contour feathers: sleek crows, gulls and swifts little, fluffy small birds a lot
for _n, _f in {"Rook": 0.15, "HoodedCrow": 0.2, "Jackdaw": 0.25, "Magpie": 0.15, "HerringGull": 0.35, "CommonGull": 0.35,
               "BlackHeadedGull": 0.35, "Cormorant": 0.15, "CommonSwift": 0.15, "BarnSwallow": 0.2, "SandMartin": 0.35,
               "Starling": 0.3, "Peregrine": 0.3, "Sparrowhawk": 0.4, "Waxwing": 0.9, "GreatTit": 0.8, "BlueTit": 0.85,
               "Robin": 0.85, "Bullfinch": 0.85, "HouseSparrow": 0.7, "TreeSparrow": 0.7, "Goldfinch": 0.7, "Chaffinch": 0.6}.items():
    SPECIES[_n]["morph"]["fluff_layer"] = _f


# ---------------------------------------------------------------- anatomy by family (feather counts, neck, foot, body)
ANATOMY = {
    # crows: 10 primaries, the outermost short; 9 secondaries
    "Rook": dict(n_prim=10, p_outer=0.62, n_sec=9), "HoodedCrow": dict(n_prim=10, p_outer=0.62, n_sec=9),
    "Jackdaw": dict(n_prim=10, p_outer=0.6, n_sec=9), "Magpie": dict(n_prim=10, p_outer=0.55, n_sec=9),
    # pigeons: 10 primaries, 11 secondaries, a deep keel, a 2-joint neck (the walking nod)
    "FeralPigeon": dict(n_prim=10, p_outer=0.92, n_sec=11, n_neck=2, keel=0.25),
    "WoodPigeon": dict(n_prim=10, p_outer=0.92, n_sec=11, n_neck=2, keel=0.25),
    # gulls: 10 primaries, a long arm with many secondaries, a 2-joint neck
    "HerringGull": dict(n_prim=10, p_outer=0.97, n_sec=16, n_neck=2, keel=0.1),
    "CommonGull": dict(n_prim=10, p_outer=0.97, n_sec=15, n_neck=2, keel=0.08),
    "BlackHeadedGull": dict(n_prim=10, p_outer=0.97, n_sec=14, n_neck=2, keel=0.06),
    # the cormorant: 10 primaries, ~17 secondaries, 14 tail feathers, a long S-neck, all four toes webbed, a slim rear
    "Cormorant": dict(n_prim=10, p_outer=0.9, n_sec=17, n_tail=7, n_neck=3, neck_s=12.0, foot="toti", rear_taper=0.15),
    # the swift: 10 primaries (the outer longest), few short secondaries, 10 tail feathers, four forward toes, a cigar body
    "CommonSwift": dict(n_prim=10, n_sec=8, n_tail=5, foot="pampro", rear_taper=0.25),
    # raptors: 10 primaries, ~13 secondaries
    "Sparrowhawk": dict(n_prim=10, p_outer=0.78, n_sec=12, keel=0.08), "Peregrine": dict(n_prim=10, p_outer=0.95, n_sec=13, keel=0.12),
}
for _n, _a in ANATOMY.items():
    SPECIES[_n]["morph"].update(_a)

# ---------------------------------------------------------------- females (species_art "female": the same styles recoloured)
FEMALE = {
    "GreatTit": dict(styles={"tie_edge": st("#e2cd3a", ("down", "#5a5a50"), ("grad", "#e8d850", 0.3))}),
    "BlueTit": dict(styles={"blue": st("#5a86bc", ("down", "#2a3a5a"), rough=0.5), "yellow": st("#d8c84a", ("down", "#5a5a48"))}),
    "Goldfinch": dict(styles={"red": st("#b83038", ("down", "#6a2a2a"), rough=0.5)}),
    "Chaffinch": dict(styles={"bluegrey": st("#8a8a7c", ("down", "#5a5a50")), "pink": st("#c4b49c", ("down", "#7a6e60")),
                              "mantle": st("#7a6c56"), "rump": st("#7a9060"), "belly": st("#e8e2d6")},
                      bare=dict(beak="#9a8a78", beak_tip="#7a6a58")),
    "Bullfinch": dict(styles={"red": st("#a8907e", ("down", "#6a5a50")), "grey": st("#8a7e72", ("down", "#5a544c"))}),
    "HouseSparrow": dict(styles={"crown": st("#8a7a64", ("down", "#5a4e40")), "chestnut": st("#b8a688", ("down", "#6a5a48")),
                                 "black": st("#b0a48e"), "cheek": st("#bcb09c"), "lcovert": st("#8a6a4a")},
                         bare=dict(beak="#a89070", beak_tip="#7a6a50")),
    "Blackbird": dict(styles={"black": st("#43352a", ("down", "#2a221c"), rough=0.6), "flight": st("#3a2e26", ("fringe", "#4a3c30", 0.12, "outer")),
                              "under": st("#5a4a3e"), "throat": st("#6e5a48", ("streak", "#3a2e24", 0.35))},
                      bare=dict(beak="#6a5a40", beak_tip="#5a4a34", eyering="#4a3c30")),
    "Starling": dict(bare=dict(iris="#6a4a2a")),
    "WhiteWagtail": dict(styles={"cap": st("#55575c", ("down", "#3a3a3e"))}),
    "Waxwing": dict(styles={"sec": st("#2a2a2e", ("tip", "#c8202a", 0.035), ("fringe", "#f2f0ea", 0.08, "tip")),
                            "tail": st("#5a5a60", ("tip", "#f0d030", 0.1), ("band", "#16161a", 0.1, 0.28))}),
    "BarnSwallow": dict(styles={"red": st("#a83a2a", ("down", "#5a2a24")), "cream": st("#f4ece2")}),
    "Sparrowhawk": dict(styles={"slate": st("#6a6658", ("down", "#4a4840")), "barred": st("#f2eee6", ("bars", "#6e5c4e", 3, 0.38), ("down", "#b8b0a8")),
                                "cheek": st("#c8a080"), "flight": st("#6a6658", ("bars", "#4a4438", 4, 0.2)),
                                "tail": st("#6a6658", ("bars", "#3a342c", 4, 0.22), ("tip", "#e8e4dc", 0.04))}),
    "Peregrine": dict(styles={"slate": st("#4a4c50", ("bars", "#2e3034", 2, 0.25), ("down", "#3a3c40")),
                              "barred": st("#e8e0d2", ("bars", "#2a2622", 3, 0.28))}),
}
for _n, _f in FEMALE.items():
    SPECIES[_n]["plumage"]["female"] = _f
SPECIES["Cormorant"]["morph"]["perch_low"] = 0.012     # short legs set far back: the body sits low on them
SPECIES["CommonSwift"]["morph"]["perch_low"] = 0.007


# ---------------------------------------------------------------- Mute swan (Cygnus olor)
species("MuteSwan", dict(body_len=1.3, body_w=1.45, body_h=1.2, belly=1.05, head=0.9, neck=4.2, n_neck=3, neck_s=-12.0, neck_up=0.004,
                         crown_h=1.0, flat_crown=0.6, throat=1.0, beak_len=1.2, beak_depth=0.95, beak_width=1.5, beak_round=1.2, beak_taper=0.5,
                         beak_hook=0.3, beak_knob=2.2, beak_curve=-0.0006, eye=0.9, neck_thin=0.5,
                         arm=1.2, hand=1.0, prim=1.0, prim_profile=FINGERED, n_prim=10, p_outer=0.9, n_sec=20, sec=1.05, fingers=0.5,
                         tail=0.7, tail_shape="wedge", fork=0.25, n_tail=7, tarsus=0.75, leg_thick=2.0, toes=1.6, webbed=True, foot="aniso",
                         hallux=0.4, fluff=1.1, fluff_layer=0.3, keel=0.1, perch_pitch=4, perch_neck=66, perch_head=-34, perch_tail=6,
                         perch_low=0.012, glide_elev=4, glide_sweep=-4, call_gape=24, gloss_hue=0.0), dict(
    styles={
        "white": st("#f6f6f2", ("down", "#e4e2dc"), rough=0.7),
        "head": st("#f4f2ea", ("down", "#e2dccc")),
        "flight": st("#f2f2ee", ("shaft", "#e0e0dc")),
        "lore": st("#141414", bare=True, skin="#141414"),
        "under": st("#ecece8"),
    },
    regions={"upper": "white", "under": "white", "head": "head", "primary": "flight", "secondary": "flight", "tertial": "white",
             "tail": "white", "underwing": "under"},
    rules=[(lambda q: q.head and q.region == "lore" and q.dz > 0.0015, "lore")],
    bare=dict(beak="#141414", beak_tip="#e86a2a", lower="#e06a2a", lower_tip="#e06a2a", tip_from=0.18, base_skin="#141414", base_skin_len=0.2,
              legs="#2a2a2a", iris="#1a120c", eyering="#2a2a2a", mouth="#c86a5a", beak_rough=0.55),
))


# ---------------------------------------------------------------- the big birds (round 4): eagles, buzzard, raven, heron, albatross
# Broad "fingered" soaring wings (deeply emarginated outer primaries spread like fingers), many secondaries, short or wedge tails.
EAGLE = dict(RAPTOR, body_len=1.12, body_w=1.18, body_h=1.12, head=0.98, neck=0.9, flat_crown=0.9, beak_taper=0.55, beak_curve=0.0032,
             arm=1.25, hand=1.05, prim=1.1, prim_profile=FINGERED, prim_width=1.25, sec=1.15, sec_width=1.2,
             feather_shapes={"primary": {"emarg": 0.55}}, fingers=1.4, n_prim=10, p_outer=0.78, n_sec=18, n_tail=6,
             tarsus=1.05, leg_thick=1.9, toes=1.5, claw=2.2, trousers=1.8, fluff=0.9, fluff_layer=0.3, keel=0.12,
             perch_pitch=34, perch_neck=-4, perch_tail=-24, glide_elev=7, glide_sweep=-2, glide_hand=-4, glide_handelev=4, eye=1.05, call_gape=34)

# White-tailed eagle (Haliaeetus albicilla), adult: brown, a pale sandy head and neck, a short white wedge tail, a huge yellow bill
species("WhiteTailedEagle", dict(EAGLE, beak_len=0.95, beak_depth=2.1, beak_width=1.45, beak_round=1.3, beak_hook=1.5, tail=0.8,
                                 tail_shape="wedge", fork=0.2), dict(
    styles={
        "brown": st("#4e3c2a", ("fringe", "#8a7254", 0.18, "all"), ("down", "#3a2c20")),
        "cov": st("#6a5238", ("fringe", "#c8b088", 0.28, "all"), ("down", "#4a3a28")),
        "head": st("#cdbb98", ("streak", "#8a7458", 0.18), ("down", "#a89878")),
        "flight": st("#241a12", ("shaft", "#2e2218")),
        "sec": st("#2e2218", ("grad", "#1e1610", 0.4)),
        "tail": st("#f2efe6", ("down", "#d8d0c0"), ("shaft", "#e0dace")),
        "under": st("#3a2c20", ("mottle", "#5a4632", 0.3)),
        "trousers": st("#3e2e22", loft=2.0),
    },
    regions={"upper": "brown", "under": "brown", "head": "head", "nape": "head", "neckside": "head", "throat": "head", "chin": "head",
             "lcovert": "cov", "mcovert": "cov", "gcovert": "cov", "scapular": "cov", "primary": "flight", "secondary": "sec", "tertial": "brown",
             "tail": "tail", "uppertail": "brown", "underwing": "under", "trousers": "trousers"},
    bare=dict(beak="#e8c040", beak_tip="#e2b838", lower="#e8c040", cere="#e8c040", legs="#e8c040", iris="#e8d070", eyering="#d8b040",
              mouth="#e8a0a0", beak_rough=0.4),
))
# Golden eagle (Aquila chrysaetos), adult: dark brown, the golden crown and nape, greyish bars on the flight feathers and tail
species("GoldenEagle", dict(EAGLE, body_len=1.08, arm=1.18, hand=1.1, beak_len=0.82, beak_depth=1.9, beak_width=1.3, beak_round=1.3, beak_hook=1.6,
                            tail=1.05, tail_shape="round"), dict(
    styles={
        "brown": st("#3e2c1e", ("fringe", "#5a4430", 0.15, "all"), ("down", "#2e2016")),
        "gold": st("#c89a50", ("streak", "#8a6a3a", 0.2), ("down", "#8a6a3a")),
        "cov": st("#5a4430", ("fringe", "#8a6a44", 0.25, "all")),
        "flight": st("#2a1e14", ("bars", "#5a4e44", 5, 0.25)),
        "tail": st("#3a2c20", ("bars", "#6a5a48", 4, 0.3), ("tip", "#1e160e", 0.2)),
        "under": st("#2e2218"),
        "trousers": st("#5a4230", loft=2.2),
    },
    regions={"upper": "brown", "under": "brown", "head": "brown", "crown": "gold", "nape": "gold", "neckside": "gold", "lcovert": "cov",
             "mcovert": "cov", "primary": "flight", "secondary": "flight", "tail": "tail", "underwing": "under", "trousers": "trousers"},
    bare=dict(beak="#2a2420", beak_tip="#141210", lower="#3a3430", cere="#e0b040", legs="#e0b040", iris="#7a4a1a", eyering="#3a2c20",
              mouth="#e8a0a0", beak_rough=0.4),
))
# Common buzzard (Buteo buteo), a typical brown bird: dark brown above, a pale breast band (the "U"), barred tail and flight feathers
species("CommonBuzzard", dict(EAGLE, body_len=1.0, body_w=1.15, head=1.08, arm=1.1, hand=0.95, prim=1.0, n_sec=14, fingers=1.1,
                              beak_len=0.6, beak_depth=1.7, beak_width=1.2, beak_round=1.3, beak_hook=1.4, tail=1.05, tail_shape="round",
                              trousers=1.4, leg_thick=1.4, toes=1.3, claw=1.8), dict(
    styles={
        "brown": st("#5a4030", ("fringe", "#8a6a4a", 0.15, "all"), ("down", "#3e2c22")),
        "breast": st("#5e4432", ("mottle", "#e2d2b0", 0.25)),
        "band": st("#e2d2b0", ("streak", "#7a5a40", 0.15)),
        "flight": st("#3a2c22", ("bars", "#8a7460", 6, 0.3)),
        "tail": st("#8a6a4a", ("bars", "#3a2a1e", 9, 0.28), ("tip", "#e6dcc8", 0.03)),
        "under": st("#ece2cc", ("bars", "#6a5240", 5, 0.25)),
        "trousers": st("#5a4232", ("bars", "#8a6a4a", 3, 0.3), loft=1.8),
    },
    regions={"upper": "brown", "under": "breast", "head": "brown", "throat": "breast", "breast": "breast", "primary": "flight",
             "secondary": "flight", "tail": "tail", "underwing": "under", "trousers": "trousers"},
    rules=[(lambda q: q.region in ("breast", "breastside") and 0.016 < q.z < 0.03, "band")],
    bare=dict(beak="#2a2420", beak_tip="#141210", lower="#3a3430", cere="#e8c050", legs="#e8c050", iris="#6a3a1a", eyering="#3a2c22",
              mouth="#e8a0a0"),
))
# Common raven (Corvus corax): the biggest crow, all black with a blue-purple sheen, a massive bill, shaggy throat, a wedge tail
species("Raven", dict(CORVID, body_len=1.08, body_w=1.12, beak_len=1.6, beak_depth=2.3, beak_width=1.45, beak_curve=0.0022, beak_round=1.3,
                      arm=1.15, hand=1.05, tail=1.2, tail_shape="wedge", fork=0.25, throat=1.35, gloss_hue=0.7, n_prim=10, p_outer=0.62, n_sec=10), dict(
    styles={
        "black": st("#0a0a0e", ("down", "#1a1a22"), gloss=0.85, rough=0.3),
        "black_dull": st("#0e0e12", gloss=0.45, rough=0.45),
        "hackles": st("#0c0c10", ("streak", "#1a1a22", 0.3), gloss=0.6, loft=1.8),
        "flight": st("#0a0a0e", ("shaft", "#141418"), gloss=0.7, rough=0.32),
        "under": st("#121216", gloss=0.25),
    },
    regions={"upper": "black", "under": "black_dull", "head": "black", "throat": "hackles", "chin": "hackles", "primary": "flight",
             "tail": "flight", "underwing": "under", "flank": "black_dull"},
    bare=dict(beak="#141414", beak_tip="#0e0e0e", lower="#1a1a1a", legs="#141414", iris="#120e0c", eyering="#141414", mouth="#6a4a50", beak_rough=0.35),
))
# Grey heron (Ardea cinerea): grey back and wings, black flight feathers, white neck with a black stripe down the front, a black
# crest stripe and plumes, a yellow dagger bill, long legs; it flies with the neck drawn back into an S
species("GreyHeron", dict(body_len=1.25, body_w=0.85, body_h=0.95, belly=0.9, head=0.82, neck=3.6, n_neck=3, neck_s=18.0, neck_up=0.006,
                          neck_thin=0.6, crown_h=0.85, flat_crown=0.9, throat=0.9, eye=1.0, crest=1.6,
                          beak_len=2.4, beak_depth=0.85, beak_width=0.7, beak_taper=0.45, beak_round=1.0, beak_curve=0.0,
                          arm=1.2, hand=1.05, prim=1.1, prim_profile=ROUNDED, prim_width=1.2, n_prim=10, p_outer=0.85, n_sec=17, sec=1.15,
                          fingers=0.5, tail=0.55, tail_shape="square", n_tail=6, shin=1.9, tarsus=2.6, leg_thick=1.1, toes=1.8, hallux=1.2,
                          fluff=0.85, fluff_layer=0.2, perch_pitch=22, perch_neck=40, perch_head=-40, perch_tail=-6, perch_low=0.0,
                          peck_depth=60, glide_elev=2, glide_sweep=-2, fly_neck=20, fly_neck_s=70, fly_head=-10, fly_trail=1.0, bare_shin=0.6, call_gape=26), dict(
    styles={
        "grey": st("#8a909a", ("down", "#6a707a"), rough=0.6),
        "white": st("#ececec", ("down", "#d0d0d0")),
        "neck": st("#e2e2e4", ("down", "#c8c8cc")),
        "stripe": st("#ececec", ("streak", "#1c1c20", 0.45)),
        "black": st("#1c1c20"),
        "flight": st("#26282c", ("shaft", "#30323a")),
        "sec": st("#4a4e56"),
        "plume": st("#d8dadc", ("streak", "#8a8e94", 0.1), loft=2.0),
        "under": st("#7a808a"),
    },
    regions={"upper": "grey", "under": "white", "head": "white", "crown": "white", "crest": "black", "neckside": "neck", "nape": "neck",
             "throat": "stripe", "breast": "plume", "flank": "black", "primary": "flight", "secondary": "sec", "tertial": "grey", "tail": "grey",
             "underwing": "under", "pcovert": "flight", "alula": "flight"},
    rules=[(lambda q: q.head and q.region in ("supercilium", "eyestripe") and q.dz < 0.0, "black")],
    bare=dict(beak="#d8b040", beak_tip="#c8a038", lower="#e0c050", legs="#9a8a6a", iris="#e8d040", eyering="#c8c060", base_skin="#c8c060",
              base_skin_len=0.1, mouth="#e0b080", beak_rough=0.4),
))
# Black-browed albatross (Thalassarche melanophris): white head and body, blackish-grey upperwing and back, a dark eyebrow, a pink-orange
# bill hooked at the tip, very long narrow stiff wings (a long arm with ~25 secondaries), a short grey tail, webbed feet
species("BlackBrowedAlbatross", dict(GULL, body_len=1.18, body_w=1.25, body_h=1.15, head=1.05, beak_len=1.75, beak_depth=1.25, beak_width=1.05,
                                     beak_hook=1.6, beak_gonys=0.2, beak_taper=0.6, arm=1.75, hand=1.2, prim=1.1, prim_profile=LONG_POINTED,
                                     prim_width=0.85, n_prim=10, p_outer=0.95, n_sec=25, sec=0.75, sec_width=0.8, tail=0.6, tail_shape="round",
                                     tarsus=0.85, leg_thick=1.6, toes=1.6, keel=0.12, fluff_layer=0.25, glide_elev=1, glide_sweep=-6, glide_hand=-8,
                                     glide_handelev=-3, perch_pitch=6, perch_neck=-8), dict(
    styles={
        "white": st("#f4f4f0", ("down", "#d8d8d4"), rough=0.6),
        "dark": st("#2a2c30", ("down", "#1c1e22")),
        "flight": st("#1e2024", ("shaft", "#3a3c40")),
        "brow": st("#202024"),
        "tail": st("#4a4c50", ("grad", "#2a2c30", 0.5)),
        "under": st("#f0f0ec", ("fringe", "#2a2c30", 0.4, "outer")),
    },
    regions={"upper": "dark", "under": "white", "head": "white", "nape": "white", "neckside": "white", "rump": "white", "uppertail": "white",
             "tail": "tail", "primary": "flight", "secondary": "flight", "tertial": "dark", "underwing": "under", "trousers": "white"},
    rules=[(lambda q: q.head and q.region in ("supercilium", "eyestripe", "lore") and abs(q.dz) < 0.004 and q.dy > -0.002, "brow")],
    bare=dict(beak="#f0a030", beak_tip="#e8782a", lower="#f0a030", lower_tip="#e8782a", tip_from=0.85, legs="#c8b8b0", iris="#2a1e16",
              eyering="#1e1e22", mouth="#e8a080", beak_rough=0.35),
))
# the new birds' plume layers: sleek raptors and the albatross, the shaggy raven
for _n, _f in {"WhiteTailedEagle": 0.3, "GoldenEagle": 0.3, "CommonBuzzard": 0.4, "Raven": 0.2, "GreyHeron": 0.2, "BlackBrowedAlbatross": 0.25}.items():
    SPECIES[_n]["morph"]["fluff_layer"] = _f
