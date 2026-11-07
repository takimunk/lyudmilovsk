"""The resident generator's wardrobe: every MakeHuman asset the looks may use, with who wears it and how it is tinted.

Plain Python (no bpy), shared by looks.py (choosing), wardrobe_scan.py and build_residents.py (building) and
tools/residents/atlases.py (textures). Asset names are MPFB asset folders (tools/residents/mhdata_assets.py installs
them from the npm package where the MakeHuman packs can't be downloaded; blender/humans/fetch_assets.sh on the Mac).

Tint slots: a garment's faces are split into named slots (`slots`: rules tried in order, first match wins):
  ("legs", slot)          faces skinned mostly to thighs/calves/feet
  ("color:<name>", slot)  faces whose texel is of that colour family (blue, white, dark, green, red, grey, skin)
  ("*", slot)             everything else
Each slot is tinted with a colour from a palette (PALETTES) or kept as painted (slot name starting with "keep").
A tinted slot's texture is turned into grey detail (linear mean 0.5) in the atlas; the game shader multiplies it by
twice the tint the mesh carries in its UV channels 1-2 (docs/residents.md). `keep_texels` keeps texels of a colour
family as painted inside tinted slots (the white shirt under a suit), `clean` paints logos and prints out of a slot.
"""

# --------------------------------------------------------------------------------------------- garments

# kind: suit (top + bottom in one), dress, top, bottom, outer (worn over a suit), shoes, hat
# sex: m / f / u; ages: [min, max]; styles: casual, smart, work, sport, summer, elder, kid
GARMENTS = {
    "male_casualsuit01": dict(kind="suit", sex="m", ages=[8, 90], styles=["casual", "elder", "kid"],
                              slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "shirt", "bottom": "jeans"}),
    "male_casualsuit02": dict(kind="suit", sex="m", ages=[5, 60], styles=["casual", "kid", "sport"],
                              slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "jeans"},
                              clean={"top": True}),
    "male_casualsuit03": dict(kind="suit", sex="m", ages=[14, 90], styles=["casual", "smart", "elder"],
                              slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "shirt", "bottom": "trousers"}),
    "male_casualsuit04": dict(kind="suit", sex="m", ages=[5, 70], styles=["casual", "kid", "summer", "sport"],
                              slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "jeans"},
                              clean={"top": True}),
    "male_casualsuit05": dict(kind="suit", sex="m", ages=[16, 90], styles=["casual", "elder", "work"],
                              slots=[("legs", "bottom"), ("*", "top")], keep_texels="maroon",
                              palettes={"top": "jacket", "bottom": "jeans"}),
    "male_casualsuit06": dict(kind="suit", sex="m", ages=[5, 70], styles=["casual", "summer", "kid", "sport"],
                              slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "jeans"},
                              clean={"top": True}),
    "male_elegantsuit01": dict(kind="suit", sex="m", ages=[18, 90], styles=["smart"],
                               slots=[("*", "top")], keep_texels="white", palettes={"top": "suit"}),
    "male_worksuit01": dict(kind="suit", sex="u", ages=[16, 80], styles=["work"],
                            slots=[("color:white", "top"), ("*", "bottom")], palettes={"top": "tee", "bottom": "overalls"}),
    "female_casualsuit01": dict(kind="suit", sex="f", ages=[5, 70], styles=["casual", "kid"],
                                slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "jeans"},
                                clean={"top": True}),
    "female_casualsuit02": dict(kind="suit", sex="f", ages=[5, 45], styles=["casual", "summer", "kid"],
                                slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "jeans"},
                                clean={"top": True}),
    "female_elegantsuit01": dict(kind="suit", sex="f", ages=[16, 90], styles=["smart", "elder", "casual"],
                                 slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "blouse", "bottom": "skirt"}),
    "female_sportsuit01": dict(kind="suit", sex="f", ages=[12, 40], styles=["sport"],
                               slots=[("legs", "bottom"), ("*", "top")], palettes={"top": "tee", "bottom": "leggings"},
                               clean={"top": True}),
    "mindfront_f_dress_01": dict(kind="dress", sex="f", ages=[18, 90], styles=["smart", "elder"],
                                 slots=[("*", "top")], palettes={"top": "dress_dark"}),
    "mindfront_f_dress_02": dict(kind="dress", sex="f", ages=[5, 90], styles=["summer", "casual", "elder", "kid"],
                                 slots=[("*", "top")], palettes={"top": "dress"}, patterns=True),
    "mindfront_f_beach_dress_01": dict(kind="dress", sex="f", ages=[12, 70], styles=["summer"],
                                       slots=[("*", "top")], palettes={"top": "dress"}),
    "punkduck_coat": dict(kind="outer", sex="u", ages=[5, 90], styles=["casual", "smart", "elder", "work"],
                          slots=[("*", "outer")], palettes={"outer": "coat"}),
    "punkduck_jeans_skirt": dict(kind="bottom", sex="f", ages=[12, 45], styles=["casual", "summer"],
                                 slots=[("*", "bottom")], palettes={"bottom": "jeans"}),
    "punkduck_short_jeans": dict(kind="bottom", sex="f", ages=[10, 40], styles=["summer"],
                                 slots=[("*", "bottom")], palettes={"bottom": "jeans"}),
    "punkduck_tight_jeans": dict(kind="bottom", sex="f", ages=[12, 60], styles=["casual"],
                                 slots=[("*", "bottom")], palettes={"bottom": "jeans"}),
    "punkduck_miniskirt": dict(kind="bottom", sex="f", ages=[15, 40], styles=["casual", "smart"],
                               slots=[("*", "bottom")], palettes={"bottom": "skirt"}),
    "punkduck_sleeveless": dict(kind="top", sex="f", ages=[12, 60], styles=["summer", "casual"],
                                slots=[("*", "top")], palettes={"top": "blouse"}),
    "punkduck_vneck_top": dict(kind="top", sex="f", ages=[12, 70], styles=["casual", "smart"],
                               slots=[("*", "top")], palettes={"top": "blouse"}),
    "wolgade_female_top_01": dict(kind="top", sex="f", ages=[12, 60], styles=["casual", "summer"],
                                  slots=[("*", "top")], palettes={"top": "tee"}),
    "mindfront_tank_top_01": dict(kind="top", sex="u", ages=[12, 50], styles=["summer", "sport"],
                                  slots=[("*", "top")], palettes={"top": "tee"}),
    "drednicolson_short_tail_tee": dict(kind="top", sex="f", ages=[12, 50], styles=["casual", "sport"],
                                        slots=[("*", "top")], palettes={"top": "tee"}),
    # shoes
    "shoes01": dict(kind="shoes", sex="u", ages=[12, 90], styles=["smart", "elder", "casual"],
                    slots=[("*", "shoes")], palettes={"shoes": "leather"}),
    "shoes02": dict(kind="shoes", sex="u", ages=[5, 70], styles=["casual", "work", "kid"],
                    slots=[("*", "keep_shoes")]),
    "shoes03": dict(kind="shoes", sex="u", ages=[14, 90], styles=["smart", "elder", "work"],
                    slots=[("*", "keep_shoes")]),
    "shoes04": dict(kind="shoes", sex="u", ages=[14, 90], styles=["smart", "elder"],
                    slots=[("*", "keep_shoes")]),
    "shoes05": dict(kind="shoes", sex="u", ages=[5, 60], styles=["casual", "sport", "kid", "summer"],
                    slots=[("*", "sneakers")], palettes={"sneakers": "sneakers"}),
    "shoes06": dict(kind="shoes", sex="u", ages=[5, 50], styles=["sport", "casual", "kid"],
                    slots=[("*", "keep_shoes")]),
    "punkduck_winter_boots": dict(kind="shoes", sex="u", ages=[10, 90], styles=["work", "elder", "casual"],
                                  slots=[("*", "shoes")], palettes={"shoes": "boots"}),
    # hats and glasses that come as MakeHuman assets
    "mindfront_fishing_hat_01": dict(kind="hat", sex="u", ages=[8, 90], styles=["work", "summer", "elder"],
                                     slots=[("*", "hat")], palettes={"hat": "hat"}),
    "punkduck_glasses": dict(kind="glasses", sex="u", ages=[10, 90], styles=["casual", "smart", "elder"],
                             slots=[("*", "keep_glasses")]),
}

# ------------------------------------------------------------------------------------------- hair etc.

HAIR = {
    # name: (sex, styles, min_age, max_age, volume m: how far it stands off the scalp, for hats)
    "short01": dict(sex="m", ages=[3, 90], volume=0.012),
    "short02": dict(sex="u", ages=[3, 90], volume=0.015),
    "short03": dict(sex="m", ages=[3, 70], volume=0.010),
    "short04": dict(sex="m", ages=[3, 90], volume=0.008),
    "bob01": dict(sex="f", ages=[10, 45], volume=0.02),
    "bob02": dict(sex="f", ages=[5, 90], volume=0.02),
    "long01": dict(sex="f", ages=[5, 70], volume=0.015),
    "ponytail01": dict(sex="f", ages=[4, 60], volume=0.015),
    "braid01": dict(sex="f", ages=[4, 50], volume=0.012),
    "afro01": dict(sex="u", ages=[3, 70], volume=0.055),
}
EYEBROWS = ["eyebrow%03d" % i for i in range(1, 13)]
EYEBROWS_M = ["eyebrow001", "eyebrow002", "eyebrow004", "eyebrow005", "eyebrow006", "eyebrow007", "eyebrow010", "eyebrow011"]
EYEBROWS_F = ["eyebrow003", "eyebrow008", "eyebrow009", "eyebrow012", "eyebrow002", "eyebrow007"]
EYELASHES_M = ["eyelashes01", "eyelashes04"]
EYELASHES_F = ["eyelashes02", "eyelashes03"]
IRISES = ["brown", "brownlight", "blue", "lightblue", "deepblue", "green", "bluegreen", "grey", "ice"]

SKINS = {
    # (age band, ancestry, sex) -> skin assets (MakeHuman team, CC0)
    ("young", "caucasian", "f"): ["young_caucasian_female", "young_caucasian_female2"],
    ("young", "caucasian", "m"): ["young_caucasian_male", "young_caucasian_male2"],
    ("young", "african", "f"): ["young_african_female"],
    ("young", "african", "m"): ["young_african_male"],
    ("young", "asian", "f"): ["young_asian_female"],
    ("young", "asian", "m"): ["young_asian_male"],
    ("middleage", "caucasian", "f"): ["middleage_caucasian_female"],
    ("middleage", "caucasian", "m"): ["middleage_caucasian_male"],
    ("middleage", "african", "f"): ["middleage_african_female"],
    ("middleage", "african", "m"): ["middleage_african_male"],
    ("middleage", "asian", "f"): ["middleage_asian_female"],
    ("middleage", "asian", "m"): ["middleage_asian_male"],
    ("old", "caucasian", "f"): ["old_caucasian_female"],
    ("old", "caucasian", "m"): ["old_caucasian_male"],
    ("old", "african", "f"): ["old_african_female"],
    ("old", "african", "m"): ["old_african_male"],
    ("old", "asian", "f"): ["old_asian_female"],
    ("old", "asian", "m"): ["old_asian_male"],
}

# --------------------------------------------------------------------------------------------- palettes

# linear-ish sRGB colours (0..1) with weights; the shader multiplies the grey detail (mean 0.5) by 2 * colour
PALETTES = {
    "tee": [((0.85, 0.85, 0.83), 3), ((0.12, 0.12, 0.13), 3), ((0.55, 0.12, 0.10), 1.5), ((0.15, 0.25, 0.45), 2),
            ((0.35, 0.45, 0.30), 1.2), ((0.80, 0.65, 0.20), 1), ((0.45, 0.45, 0.47), 2), ((0.55, 0.30, 0.45), 1),
            ((0.20, 0.45, 0.55), 1), ((0.85, 0.45, 0.25), 0.8), ((0.60, 0.70, 0.85), 1.2), ((0.90, 0.75, 0.70), 1)],
    "shirt": [((0.80, 0.82, 0.85), 3), ((0.40, 0.50, 0.65), 2), ((0.30, 0.25, 0.20), 1), ((0.55, 0.20, 0.18), 1),
              ((0.25, 0.35, 0.28), 1), ((0.70, 0.65, 0.55), 1.5), ((0.15, 0.18, 0.28), 1.5), ((0.60, 0.55, 0.65), 0.8)],
    "blouse": [((0.88, 0.86, 0.82), 3), ((0.75, 0.55, 0.60), 1.5), ((0.30, 0.35, 0.55), 1.5), ((0.55, 0.20, 0.25), 1),
               ((0.40, 0.55, 0.45), 1), ((0.85, 0.75, 0.45), 1), ((0.20, 0.20, 0.22), 1.5), ((0.60, 0.65, 0.80), 1)],
    "jacket": [((0.30, 0.33, 0.25), 2), ((0.18, 0.20, 0.25), 2), ((0.35, 0.25, 0.18), 1.5), ((0.12, 0.12, 0.13), 1.5),
               ((0.45, 0.40, 0.32), 1), ((0.40, 0.12, 0.10), 0.6), ((0.20, 0.28, 0.40), 1)],
    "coat": [((0.22, 0.20, 0.18), 2), ((0.40, 0.33, 0.25), 2), ((0.15, 0.17, 0.22), 2), ((0.45, 0.42, 0.38), 1.5),
             ((0.35, 0.10, 0.10), 0.7), ((0.25, 0.30, 0.22), 1), ((0.55, 0.48, 0.38), 1)],
    "white_coat": [((0.93, 0.93, 0.92), 1)],
    "suit": [((0.10, 0.10, 0.11), 3), ((0.15, 0.17, 0.25), 2), ((0.30, 0.30, 0.32), 1.5), ((0.30, 0.25, 0.20), 1)],
    "jeans": [((0.20, 0.30, 0.48), 4), ((0.12, 0.14, 0.20), 2), ((0.12, 0.12, 0.13), 2), ((0.35, 0.45, 0.60), 1.5),
              ((0.40, 0.38, 0.33), 0.8)],
    "trousers": [((0.15, 0.15, 0.16), 3), ((0.25, 0.25, 0.27), 2), ((0.35, 0.30, 0.22), 1.5), ((0.20, 0.22, 0.30), 1.5),
                 ((0.45, 0.42, 0.35), 1)],
    "skirt": [((0.15, 0.15, 0.17), 2), ((0.30, 0.30, 0.33), 1.5), ((0.35, 0.18, 0.15), 1), ((0.18, 0.22, 0.35), 1.5),
              ((0.45, 0.38, 0.30), 1), ((0.30, 0.35, 0.25), 0.8)],
    "leggings": [((0.08, 0.08, 0.09), 3), ((0.20, 0.22, 0.30), 1), ((0.30, 0.18, 0.30), 0.6)],
    "overalls": [((0.25, 0.35, 0.55), 2), ((0.20, 0.30, 0.20), 1.5), ((0.50, 0.25, 0.10), 1), ((0.22, 0.22, 0.24), 1)],
    "dress": [((0.20, 0.45, 0.40), 1), ((0.55, 0.15, 0.18), 1.2), ((0.15, 0.20, 0.40), 1.5), ((0.75, 0.65, 0.45), 1),
              ((0.45, 0.30, 0.50), 1), ((0.85, 0.80, 0.75), 1), ((0.25, 0.25, 0.27), 1), ((0.80, 0.50, 0.40), 0.8)],
    "dress_dark": [((0.15, 0.10, 0.10), 2), ((0.12, 0.12, 0.18), 2), ((0.25, 0.20, 0.15), 1.5), ((0.10, 0.18, 0.12), 1)],
    "leather": [((0.30, 0.18, 0.10), 2), ((0.10, 0.08, 0.07), 2), ((0.45, 0.30, 0.18), 1)],
    "boots": [((0.35, 0.25, 0.15), 2), ((0.12, 0.10, 0.09), 2), ((0.25, 0.30, 0.18), 1)],
    "sneakers": [((0.85, 0.85, 0.85), 3), ((0.15, 0.15, 0.17), 2), ((0.25, 0.35, 0.60), 1), ((0.60, 0.15, 0.12), 0.7)],
    "hat": [((0.30, 0.32, 0.22), 2), ((0.75, 0.70, 0.55), 2), ((0.15, 0.18, 0.25), 1)],
    # the elderly dress quieter, kids brighter (looks.py picks from these instead where it fits)
    "elder_top": [((0.40, 0.35, 0.30), 2), ((0.25, 0.28, 0.35), 2), ((0.55, 0.52, 0.48), 1.5), ((0.35, 0.20, 0.18), 1),
                  ((0.30, 0.35, 0.28), 1), ((0.70, 0.68, 0.62), 1)],
    "kid_top": [((0.85, 0.20, 0.15), 2), ((0.15, 0.45, 0.80), 2), ((0.95, 0.80, 0.15), 1.5), ((0.25, 0.65, 0.30), 1.5),
                ((0.90, 0.50, 0.70), 1.5), ((0.95, 0.55, 0.15), 1), ((0.85, 0.85, 0.85), 1)],
}

HAIR_COLORS = {
    "black": (0.025, 0.022, 0.02), "darkbrown": (0.07, 0.045, 0.03), "brown": (0.14, 0.09, 0.05),
    "lightbrown": (0.28, 0.19, 0.11), "dirtyblonde": (0.45, 0.35, 0.22), "blonde": (0.55, 0.43, 0.25),
    "platinum": (0.62, 0.56, 0.44), "red": (0.38, 0.12, 0.05), "auburn": (0.25, 0.09, 0.05),
    "grey": (0.45, 0.44, 0.42), "silver": (0.68, 0.67, 0.65), "white": (0.85, 0.84, 0.82),
    "dyed_purple": (0.35, 0.18, 0.45), "dyed_blue": (0.15, 0.25, 0.55), "dyed_pink": (0.75, 0.35, 0.50),
}
