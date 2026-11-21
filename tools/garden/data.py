"""The allotment's structure (docs/allotment.md): crops, rules, the layout behind the commune house, the new pocket items.
The words are tools/garden/words.py; tools/garden/build_garden.py writes Content/Garden/garden.json and the string tables.

Days are sown-to-ripe at the best warmth with water and no weeds, round the clock (the game's clock runs at real speed:
the brief asks for radishes in three days). The warmth is the bed's: the air plus the film's (docs/allotment.md "Growing").
"""

RULES = {
    "cover_night": 2.0, "cover_sun": 10.0, "open_sun": 1.5, "rain_wet": 0.08, "can_water": 0.55, "can_fills": 6,
    "weed_days": 4.0, "weed_base": 5.0, "dry_below": 0.3, "frost_hurt": 0.25, "spoil_days": 4.0, "catch_up_days": 60.0,
    "rent": 30,
}

# id, species (PlantSpeciesLibrary), seed item, yield item, yield count, days, base, best, hot, thirst, frost_kill, ripe_days,
# again (days to ripen again after a picking, 0 none), pickings, perennial, seedling
CROPS = [
    dict(id="radish", species="GardenRadish", seed="seeds_radish", yield_="radish", yield_count=5, days=3.0, base=3.0, best=16.0,
         hot=25.0, thirst=0.35, frost_kill=-4.0, ripe_days=2.0),
    dict(id="dill", species="GardenDill", seed="seeds_dill", yield_="dill", yield_count=2, days=5.0, base=4.0, best=17.0, hot=27.0,
         thirst=0.30, frost_kill=-3.0, ripe_days=4.0, again=2.5, pickings=3),
    dict(id="strawberry", species="GardenStrawberry", seed="seedling_strawberry", yield_="strawberries", yield_count=4, days=8.0,
         base=6.0, best=20.0, hot=30.0, thirst=0.40, frost_kill=-2.0, ripe_days=2.0, again=3.0, pickings=4, perennial=True,
         seedling=True),
    dict(id="potato", species="GardenPotato", seed="seed_potatoes", yield_="potatoes", yield_count=6, days=10.0, base=7.0, best=18.0,
         hot=28.0, thirst=0.40, frost_kill=-1.0, ripe_days=8.0),
    dict(id="tomato", species="GardenTomato", seed="seedling_tomato", yield_="tomatoes", yield_count=4, days=12.0, base=10.0,
         best=24.0, hot=32.0, thirst=0.60, frost_kill=0.5, ripe_days=3.0, again=2.0, pickings=3, seedling=True),
    dict(id="sunflower", species="GardenSunflower", seed="seeds_sunflower", yield_="seeds", yield_count=3, days=12.0, base=8.0,
         best=24.0, hot=33.0, thirst=0.45, frost_kill=-1.0, ripe_days=6.0),
]

# the new pocket items (game-items skill): id, look, size cm, tint (r, g, b), price, where it is sold, edible
# (the icon prompts are words.ICONS; the sunflower's yield is the town's own «seeds», семечки)
ITEMS = [
    dict(id="spade", look="flat", size=40, tint=(0.45, 0.42, 0.38), price=60, sold="hardware"),
    dict(id="watering_can", look="bottle", size=30, tint=(0.35, 0.55, 0.45), price=45, sold="hardware"),
    dict(id="garden_compost", look="pack", size=26, tint=(0.35, 0.25, 0.16), price=15, sold="hardware"),
    dict(id="seeds_radish", look="flat", size=10, tint=(0.85, 0.35, 0.45), price=5, sold="hardware market"),
    dict(id="seeds_dill", look="flat", size=10, tint=(0.45, 0.65, 0.30), price=4, sold="hardware market"),
    dict(id="seeds_sunflower", look="flat", size=10, tint=(0.95, 0.75, 0.15), price=4, sold="hardware market"),
    dict(id="seed_potatoes", look="pack", size=22, tint=(0.70, 0.58, 0.40), price=12, sold="market"),
    dict(id="seedling_tomato", look="cup", size=12, tint=(0.30, 0.55, 0.25), price=8, sold="market"),
    dict(id="seedling_strawberry", look="cup", size=12, tint=(0.30, 0.55, 0.25), price=10, sold="market"),
    dict(id="radish", look="pack", size=12, tint=(0.85, 0.20, 0.30), price=3, edible=True),
    dict(id="dill", look="flat", size=20, tint=(0.40, 0.60, 0.25), price=4, edible=True),
    dict(id="potatoes", look="pack", size=18, tint=(0.75, 0.62, 0.42), price=3),
    dict(id="tomatoes", look="pack", size=14, tint=(0.85, 0.20, 0.12), price=5, edible=True),
    dict(id="strawberries", look="cup", size=12, tint=(0.85, 0.12, 0.18), price=6, edible=True),
    dict(id="jam_strawberry", look="glass", size=12, tint=(0.60, 0.05, 0.10), price=40, edible=True),
]

# what the market's scales pay for a harvest by quality 1..3, roubles each (the sunflower's семечки: the shops' own price)
SELL = {"radish": [2, 3, 4], "dill": [2, 4, 5], "potatoes": [2, 3, 4], "tomatoes": [3, 5, 7], "strawberries": [4, 6, 8]}

# the jam: strawberries the neighbour needs for a jar, hours till it is ready; the market contest: jars taken at the
# control scales Monday to Friday, judged on Sunday at 16 (the market's own notices city.reg.market.1, city.mk.day.jam.1)
JAM = {"strawberries": 6, "hours": 20, "contest_weekday": 6, "contest_hour": 16, "prize": [50, 30, 15]}

# the harvest fair (festivals.json harvest_fair, docs/festivals.md "The new toys"): the first harvest he brings to the scales on
# its day is judged too (LyudGarden::FairPlace: how well it was kept, how many crops), roubles for the first three places
FAIR = {"festival": "harvest_fair", "prize": [80, 40, 20]}

# where it is: the commune house's yard (P-T2H2, Narkomfin's slab on the second terrace), behind the house on the inland
# side, between its back and the third terrace's wall. Local metres from the plot's middle: +x toward the sea (the front),
# +y north; the build turns them by the plot's yaw. Ground: the terrace's (the build takes the plot's z).
SITE = {
    "plot": "P-T2H2",
    "centre": (-34.0, 0.0),          # 34 m behind the house's middle (its back is at -7 m, the wall W3 at about -70 m)
    "size": (14.0, 26.0),            # fenced (x across, y along)
    "gate": (7.0, -13.0),            # in the fence's seaward side, at its south end
}
# beds: id, local centre (from the site's centre), size (x, y) m, who keeps it, its crops (one per cell, for the
# neighbours; the hero's are his own)
BED_SIZE = (1.2, 3.6)
CELLS = (2, 3)                       # cells across, along
BEDS = [
    dict(id="hero", at=(2.5, -4.5), keeper="hero"),
    dict(id="maarja", at=(2.5, 0.0), keeper="t_berntsen_maarja", crops=["strawberry", "strawberry", "dill", "radish", "tomato", "tomato"]),
    dict(id="sargsyan", at=(2.5, 4.5), keeper="t_sargsyan_eleni", crops=["potato", "potato", "potato", "potato", "dill", "sunflower"]),
    dict(id="patel", at=(-2.5, -4.5), keeper="t_patel_sachiko", crops=["tomato", "tomato", "radish", "dill", "strawberry", "strawberry"]),
    dict(id="brooks", at=(-2.5, 0.0), keeper="t_brooks_chiara", crops=["sunflower", "sunflower", "potato", "potato", "radish", "dill"]),
    dict(id="mendoza", at=(-2.5, 4.5), keeper="t_mendoza_ingrid", crops=["strawberry", "strawberry", "strawberry", "strawberry", "tomato", "dill"]),
]
# the neighbour who gives advice and cooks jam: on the bed next to his, in the mornings (a lark: up at five)
NEIGHBOUR = {"person": "t_berntsen_maarja", "bed": "maarja", "hours": [7.0, 10.5], "days": "mon tue wed thu fri sat",
             "stand": (1.6, -0.6)}
# props, local to the site's centre: kind, at, yaw (deg, relative to the plot's), size
PROPS = [
    dict(id="board", kind="board", at=(6.4, -11.5), yaw=0.0),      # the house committee's notice board: rent a bed
    dict(id="barrel", kind="barrel", at=(5.6, -8.0), yaw=0.0),     # rain-water barrel with a dipper: fill the can
    dict(id="shed", kind="shed", at=(-5.0, -10.5), yaw=0.0),
    dict(id="bench", kind="bench", at=(-0.2, -9.5), yaw=90.0),    # where the jar waits for him
]
# the market's control scales (P-MARKET/control_scales, interiors.json): the harvest is taken there and jam for the contest
MARKET = {"thing": "P-MARKET/control_scales", "hours": [7, 18]}
