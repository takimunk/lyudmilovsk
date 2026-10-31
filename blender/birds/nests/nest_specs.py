"""Per-species recipe of each open nest (kind comes from nest_table.json, radius too)."""

BARK = [("bark_brown", 3), ("bark_grey", 2), ("bark_dark", 2)]
BARK_G = BARK + [("guano", 4)]


def plat(**k):
    return dict(kind="Platform", **k)


def cup(**k):
    return dict(kind="Cup", **k)


SPECS = {
    # ------------------------------------------------------------ stick platforms
    "Rook": plat(t=.05, d=.09, rin=.10, N=175, rr=(3, 6.5), L=(.15, .45), cells=BARK, lining="earth", curls=14),
    "HoodedCrow": plat(t=.05, d=.085, rin=.09, N=150, rr=(3, 6), L=(.15, .45), cells=BARK, lining="grass_fine", curls=16, curl_cell="wool"),
    "CarrionCrow": plat(t=.05, d=.085, rin=.09, N=150, rr=(3, 6), L=(.15, .45), cells=BARK, lining="earth", curls=16, curl_cell="grass_fine"),
    "Raven": plat(t=.07, d=.11, rin=.14, N=190, rr=(4, 9), L=(.2, .6), cells=BARK, lining="wool", curls=14, curl_cell="grass_dry"),
    "Cormorant": plat(t=.06, d=.07, rin=.12, N=165, rr=(3, 6.5), L=(.15, .5), cells=BARK_G, lining="seaweed", curls=10, curl_cell="seaweed"),
    "GreyHeron": plat(t=.08, d=.09, rin=.17, N=200, rr=(3.5, 8), L=(.2, .6), cells=BARK + [("bark_pale", 1)], lining="grass_dry", curls=14, curl_cell="grass_dry"),
    "WhiteTailedEagle": plat(t=.22, d=.12, rin=.28, N=300, rr=(5, 13), L=(.3, 1.0), cells=BARK + [("bark_pale", 1)], lining="grass_dry", curls=20, curl_cell="grass_fine", f=(0.5, 0.3)),
    "GoldenEagle": plat(t=.2, d=.10, rin=.25, N=280, rr=(4.5, 11), L=(.25, .9), cells=BARK + [("bark_green", 1)], lining="bark_green", curls=18, curl_cell="grass_dry", f=(0.5, 0.3)),
    "CommonBuzzard": plat(t=.08, d=.07, rin=.15, N=160, rr=(3, 6.5), L=(.15, .5), cells=BARK, lining="bark_green", curls=12, curl_cell="grass_fine"),
    "Sparrowhawk": plat(t=.04, d=.03, rin=.13, N=150, rr=(2, 3.8), L=(.12, .35), cells=[("bark_dark", 3), ("bark_brown", 2), ("bark_grey", 1)], lining="bark_pale", curls=6, curl_cell="bark_pale"),
    "FeralPigeon": plat(t=.012, d=.012, rin=.07, N=26, rr=(1.2, 1.8), L=(.07, .18), cells=[("bark_grey", 2), ("bark_pale", 1), ("grass_dry", 2)], lining=None, sides=4),
    "WoodPigeon": plat(t=.015, d=.012, rin=.08, N=30, rr=(1.2, 2.0), L=(.08, .22), cells=[("bark_dark", 2), ("bark_brown", 2), ("bark_grey", 1)], lining=None, sides=4),
    # ------------------------------------------------------------ dome
    "Magpie": dict(kind="Dome", Nbase=55, Ndome=230, cells=BARK + [("bark_dark", 1)], rr=(3, 6), L=(.15, .5)),
    # ------------------------------------------------------------ cups
    "Chaffinch": cup(t=.012, d=.035, rin=.03, wall=.16, cell_out="moss_green", cell_in="feather", cell_in2="moss_green",
                     strands=[("moss", 16, "moss_green", (3, 5)), ("lichen", 14, "lichen", (3, 5.5)), ("grass", 18, "grass_fine", (.5, .8))],
                     rim_extra=("feather", 4, "feather")),
    "Goldfinch": cup(t=.01, d=.03, rin=.031, wall=.12, cell_out="moss_green", cell_in="down", cell_in2="moss_green",
                     strands=[("moss", 10, "moss_green", (2.5, 4)), ("lichen", 8, "lichen", (2.5, 4)), ("grass", 14, "grass_fine", (.5, .7)), ("wool", 8, "down", (2, 3.5))]),
    "Bullfinch": cup(t=.012, d=.03, rin=.03, wall=.12, shell=False, cell_out="rootlet", cell_in="rootlet", cell_in2="rootlet",
                     strands=[("twig", 40, "bark_dark", (.8, 1.2)), ("rootlet", 14, "rootlet", (.5, .7))]),
    "SongThrush": cup(t=.02, d=.05, rin=.052, wall=.22, smooth=True, cell_out="grass_dry", cell_in="mud_wet", cell_in2="mud_wet",
                      strands=[("grass", 30, "grass_dry", (.8, 1.2)), ("moss", 12, "moss_green", (4, 7)), ("twig", 8, "bark_brown", (1, 1.6))]),
    "Fieldfare": cup(t=.03, d=.06, rin=.058, wall=.22, cell_out="grass_dry", cell_in="grass_fine", cell_in2="mud_dry",
                     strands=[("grass", 36, "grass_dry", (.8, 1.3)), ("twig", 10, "bark_brown", (1, 1.8)), ("moss", 8, "moss_green", (4, 7))]),
    "Blackbird": cup(t=.025, d=.055, rin=.052, wall=.22, cell_out="grass_dry", cell_in="grass_fine", cell_in2="mud_dry",
                     strands=[("grass", 36, "grass_dry", (.8, 1.2)), ("twig", 8, "bark_brown", (1, 1.6)), ("moss", 6, "moss_green", (4, 6))]),
    "Robin": cup(t=.02, d=.04, rin=.038, wall=.2, cell_out="moss_green", cell_in="rootlet", cell_in2="moss_green",
                 strands=[("moss", 16, "moss_green", (3, 6)), ("leaf", 14, "leaf_dead", (4, 7)), ("grass", 10, "grass_dry", (.6, .9))]),
    "Jay": cup(t=.03, d=.07, rin=.075, wall=.2, cell_out="rootlet", cell_in="rootlet", cell_in2="earth",
               strands=[("twig", 52, "bark_dark", (1.2, 2.2)), ("twig", 10, "bark_brown", (1.5, 2.2))]),
    "Nutcracker": cup(t=.04, d=.07, rin=.07, wall=.22, cell_out="earth", cell_in="grass_fine", cell_in2="earth",
                      strands=[("twig", 42, "bark_brown", (1.2, 2.2)), ("moss", 10, "moss_green", (5, 9)), ("lichen", 10, "lichen", (5, 9))]),
    # ------------------------------------------------------------ mounds
    "BlackHeadedGull": dict(kind="Mound", t=.08, d=.03, rin=.075, wall=.2, n=24, cell_out="reed", cell_in="grass_fine", cell_in2="reed",
                            profile=[(0, 0), (0.55, 0), (0.82, 0.14), (1.0, 0.3), (0.93, 0.55), (0.8, 0.82)],
                            strands=[("reed", 48, "reed", (1.4, 2.6)), ("grass", 16, "grass_dry", (.8, 1.2))]),
    "MuteSwan": dict(kind="Mound", t=.40, d=.10, rin=.32, wall=.18, n=32, cell_out="reed", cell_in="grass_dry", cell_in2="reed",
                     profile=[(0, 0), (0.5, 0), (0.78, 0.1), (1.0, 0.22), (0.93, 0.5), (0.74, 0.8)],
                     strands=[("reed", 230, "reed", (4, 8)), ("twig", 50, "bark_brown", (6, 10)), ("grass", 40, "grass_dry", (2, 3))]),
    # ------------------------------------------------------------ mud cup
    "BarnSwallow": dict(kind="MudCup", t=.012, d=.045, rin=.036, wall=.16, L=.021, n=21, cell_out="mud_dry", cell_in="feather", cell_in2="mud_dry",
                        pellets=70, rim_extra=("feather", 4, "feather")),
    # ------------------------------------------------------------ scrapes
    "HerringGull": dict(kind="Scrape", d=.05, rin=.11, ground="earth", ground_full="grass_dry", lining="grass_fine",
                        ring=[("grass", 110, "grass_dry", (1.2, 1.8)), ("seaweed", 8, "seaweed", (10, 18)), ("feather", 0, "feather", (1, 1))]),
    "CommonGull": dict(kind="Scrape", d=.04, rin=.09, ground="earth", ground_full="grass_dry", lining="grass_fine",
                       ring=[("grass", 90, "grass_dry", (1.1, 1.6)), ("seaweed", 6, "seaweed", (9, 15))]),
    "Peregrine": dict(kind="Scrape", d=.04, rin=.14, ground="pebbles", lining=None,
                      ring=[("pebble", 26, "pebbles", (6, 14)), ("down", 6, "down", (10, 16))]),
}
