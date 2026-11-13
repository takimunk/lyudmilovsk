"""Showroom of the interior kit (blender/city/interior_kit.py): every piece in bays along a long hall, a lint of the
whole lot (triangles, stretched UVs, coplanar faces, kit.check) and contact-sheet shots of each bay.

    python3 blender/city/kit_showroom.py -- [--no-render] [--bays 1,3] [--close] [--samples 16] [--res 1600x1000]
    (or: blender -b -P blender/city/kit_showroom.py -- ...)

Writes renders/city/interior/kit_<n>_<bay>.jpg. Not a building type (nothing is exported to the game). A person
1.75 m tall stands in every bay (a grey figure at the left) for scale; the bays' back wall is 4 m high.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_argv = list(sys.argv)
ARGS = _argv[_argv.index("--") + 1:] if "--" in _argv else []

import citykit  # noqa: E402
import interior_kit as kit  # noqa: E402
from citykit import geom  # noqa: E402

BAY_W, WALL_Y = 15.0, 7.0

# rows of each bay: wall pieces (backs on the wall at WALL_Y, facing -y), a back row and a front row of floor pieces,
# "props" = small things on a long table in the front row. Entries: name or (name, {options}).
BAYS = [
    ("seating", {
        "wall": ["picture", ("picture", {"name": "painting_sea_storm", "w": 0.9}), ("poster", {"name": "cinema"}),
                 ("mosaic_panel", {"w": 1.0, "h": 1.0}), ("sign_plate", {"name": "sign_canteen", "w": 1.4, "zc": 2.6}),
                 ("menu_board", {}), "wall_clock", ("notice_board", {"w": 1.1}), "radiator"],
        "back": ["sofa", "armchair", ("chair", {"kind": "wood"}), ("chair", {"kind": "tube"}), ("chair", {"kind": "school"}),
                 "stool", "bar_stool", ("bench", {"w": 1.4}), "bean_bag", "zabuton"],
        "front": [("cinema_row", {"n": 4}), ("waiting_row", {"n": 3}), ("bench", {"kind": "gym", "w": 1.2}),
                  ("bench", {"kind": "sauna", "w": 1.0})],
    }),
    ("tables", {
        "wall": [("blackboard", {"w": 2.2}), ("wall_map", {"w": 1.4, "h": 1.0}), ("eye_chart", {}),
                 ("picture", {"name": "chart_periodic", "w": 1.0}), ("poster", {"name": "kids", "w": 0.5})],
        "back": ["table", "table_round", ("reading_table", {"w": 2.0, "lamps": 2}), "coffee_table", "school_desk",
                 "office_desk", "drafting_table"],
        "front": ["chabudai", "restaurant_table", "chess_table", "teacher_desk", "lab_bench"],
    }),
    ("storage", {
        "wall": [("bookshelf", {"topic": "library", "seed": 1}), ("bookshelf", {"topic": "kids", "seed": 2, "w": 0.8}),
                 "magazine_rack", "wardrobe", "filing_cabinet", ("lockers", {"n": 3}), "safe",
                 ("shoe_rack", {"w": 0.9})],
        "back": ["card_catalogue", "display_case", "clothes_rail", ("coat_rack", {"w": 2.0}), "aquarium"],
        "front": [("rug", {"w": 1.6, "d": 2.4}), ("rug", {"w": 1.6, "round": True}), ("mats", {"n": 2}), "hat_stand",
                  ("plant", {"kind": "ficus"}), ("plant", {"kind": "monstera"}), ("plant", {"kind": "palm"}),
                  ("plant", {"kind": "cactus"}), ("plant", {"kind": "fern"}), "floor_lamp"],
    }),
    ("shops", {
        "wall": [("shop_shelf", {"goods": "groceries", "sign": "sign_bread"}), ("shop_shelf", {"goods": "boxes", "w": 1.0}),
                 ("shop_shelf", {"goods": "jars", "w": 1.0}), ("shop_shelf", {"goods": "fabric", "w": 1.0}),
                 ("shop_shelf", {"goods": "shoes", "w": 1.0}), ("shop_shelf", {"goods": "toys", "w": 1.0}),
                 ("shop_shelf", {"goods": "records", "w": 1.0}), ("shop_shelf", {"goods": "hardware", "w": 1.0}),
                 ("shop_shelf", {"goods": "bottles", "w": 1.0})],
        "back": [("shop_counter", {"w": 1.8}), ("bar_counter", {"w": 2.4, "back": False}), ("reception_desk", {"w": 1.8}),
                 "ticket_booth"],
        "front": [("lending_desk", {"w": 2.0}), ("pharmacy_counter", {"w": 1.8, "back": False}), "market_stall"],
    }),
    ("home_culture", {
        "wall": [("kitchen_unit", {}), ("curtains", {"w": 1.4}), "mirror", ("wall_shelf", {}), ("piano", {}),
                 "dart_board", "fire_extinguisher", "wall_phone"],
        "back": [("bed", {"size": "double"}), "bunk_bed", "futon", "bedside_table", "tv_set", "grand_piano"],
        "front": ["jukebox", "arcade", "photo_booth", "table_football", ("easel", {"canvas": "painting_still_life"}),
                  ("stage", {"w": 3.0, "d": 2.0, "h": 0.45, "curtain_h": 2.6})],
    }),
    ("leisure_sport", {
        "wall": [("cinema_screen", {"w": 2.6, "h": 1.3, "zb": 1.3}), ("wall_bars", {}), ("basketball_hoop", {}),
                 ("banner", {"w": 1.6, "zc": 2.9}), "skeleton"],
        "back": ["potters_wheel", "sewing_table", "loom", "telescope", ("globe", {"floor": True}), "barbell_rack"],
        "front": ["bench_press", "punching_bag", "vaulting_horse", "exercise_bike", "medical_couch", "scales",
                  "screen_divider", "dentist_chair"],
    }),
]
PROPS = ["books_stack", "newspaper", "cup", "glass", "teapot", "plates", "bottles", "vase", "typewriter", "telephone",
         "cash_register", "kitchen_scales", "samovar", "radio", "record_player", "projector", "slide_projector",
         "table_lamp", "lamp_green", "globe", "chess_set", "paint_jars", "tools", "crate", "sack", "cardboard_box"]


def entry(e):
    return (e, {}) if isinstance(e, str) else e


def width_of(name, opts):
    """Width along x of a piece at yaw 0 (built into a throw-away building)."""
    S = kit.Building_stub()
    r = kit.PIECES[name](S, 0.0, 0.0, 0.0, 0.0, **opts) if name not in ("pendant", "ceiling_fan") else \
        kit.PIECES[name](S, 0.0, 0.0, 3.0, **opts)
    xs = [p[0] for p in r["foot"]]
    ys = [p[1] for p in r["foot"]]
    return max(xs) - min(xs), max(ys) - min(ys)


def person(B, x, y):
    """A 1.75 m reference figure (B.detail, grey)."""
    m = B.detail
    geom.box(m, "CT_PlasterGrey", x - 0.17, x - 0.03, y - 0.1, y + 0.1, 0.0, 0.86)
    geom.box(m, "CT_PlasterGrey", x + 0.03, x + 0.17, y - 0.1, y + 0.1, 0.0, 0.86)
    geom.box(m, "CT_PlasterGrey", x - 0.21, x + 0.21, y - 0.12, y + 0.12, 0.86, 1.5)
    geom.box(m, "CT_PlasterGrey", x - 0.1, x + 0.1, y - 0.11, y + 0.11, 1.53, 1.75)


def build(B, which=None):
    stats = []
    for bi, (bay, rows) in enumerate(BAYS):
        if which and bi + 1 not in which:
            continue
        x0 = bi * (BAY_W + 1.0)
        # hall: floor, back wall, side stubs
        B.slab([(x0, -3.0), (x0 + BAY_W, -3.0), (x0 + BAY_W, WALL_Y + 0.3), (x0, WALL_Y + 0.3)], -0.12, 0.0, mat="CT_Floor")
        geom.box(B.m, "CT_Interior", x0, x0 + BAY_W, WALL_Y, WALL_Y + 0.3, 0.0, 4.0)
        geom.box(B.m, "CT_Interior", x0 - 0.3, x0, -3.0, WALL_Y + 0.3, 0.0, 4.0)
        person(B, x0 + 0.5, 1.0)
        kit.pendant(B, x0 + BAY_W / 2, 2.5, 4.0, kind="cone")
        kit.ceiling_fan(B, x0 + BAY_W / 2 + 3, 2.5, 4.0)
        for row, y_line in (("wall", WALL_Y), ("back", WALL_Y - 2.4), ("front", WALL_Y - 5.6)):
            items = [entry(e) for e in rows.get(row, [])]
            ws = [width_of(n, o) for n, o in items]
            gap = 0.45
            total = sum(w for w, d in ws) + gap * (len(ws) - 1)
            x = x0 + max(1.0, (BAY_W - total) / 2)
            for (name, opts), (w, d) in zip(items, ws):
                cx = x + w / 2
                fn = kit.PIECES[name]
                if row == "wall":
                    r = fn(B, cx, y_line, 0.0, 0.0, **opts)
                else:
                    r = fn(B, cx, y_line, 0.0, 0.0, **opts)
                stats.append((bay, name, r["tris"]))
                x += w + gap
        if bay == "seating":
            # a dressed table: the props, on a long table at the front right
            tx, ty = x0 + BAY_W - 3.2, WALL_Y - 7.9
            t = kit.table(B, tx, ty, 0.0, 0.0, w=5.4, d=0.9)
            for k, name in enumerate(PROPS):
                px = tx - 2.5 + (k % 13) * 0.4
                py = ty - 0.2 + (k // 13) * 0.4
                r = kit.PIECES[name](B, px, py, t["top"], 20.0)
                stats.append(("props", name, r["tris"]))
            kit.around_table(B, tx, ty, 5.4, 0.9, 0.0, per_side=(0, 4, 0, 0), kind="tube")
    return stats


def main():
    which = None
    if "--bays" in ARGS:
        which = {int(v) for v in ARGS[ARGS.index("--bays") + 1].split(",")}
    B = citykit.Building("kit_showroom")
    stats = build(B, which)
    over = [(n, t, kit.BUDGET[n]) for _, n, t in stats if n in kit.BUDGET and t > kit.BUDGET[n]]
    print("SHOWROOM %d pieces placed, detail %d tris, shell %d tris" % (len(stats), B.detail.tri_count(), B.m.tri_count()))
    for b in sorted({s[0] for s in stats}):
        print("SHOWROOM   %-14s %s" % (b, ", ".join("%s %d" % (n, t) for bb, n, t in stats if bb == b)))
    print("SHOWROOM over budget: %s" % (over or "none"))
    for w in kit.check(B):
        print("SHOWROOM check: " + w)
    if "--no-render" in ARGS and "bpy" not in sys.modules:
        try:
            import bpy  # noqa: F401
        except ImportError:
            return
    sys.argv = [_argv[0], "--", "--no-render"]
    import build_city as bc
    import shoot_interior as si
    sys.argv = _argv
    for line in bc.lint(B):
        print("SHOWROOM lint " + line)
    if "--no-render" in ARGS:
        return
    cams = []
    for bi, (bay, _) in enumerate(BAYS):
        if which and bi + 1 not in which:
            continue
        x0 = bi * (BAY_W + 1.0)
        if "--close" in ARGS:
            cams.append((x0 + 4.0, -1.8, 3.0, 180.0, -22.0, bay + "_left"))
            cams.append((x0 + 11.0, -1.8, 3.0, 180.0, -22.0, bay + "_right"))
        else:
            cams.append((x0 + BAY_W / 2, -6.5, 5.0, 180.0, -24.0, bay))
    res = ARGS[ARGS.index("--res") + 1] if "--res" in ARGS else "1600x1000"
    samples = int(ARGS[ARGS.index("--samples") + 1]) if "--samples" in ARGS else 16
    # camera yaw: kit convention, 180 = looking toward +y (into the bays)
    si.shoot(B, "kit", cams, samples=samples, res=res, lens=24.0, exposure=0.3)
    # closer views: python3 blender/city/kit_showroom.py -- --close --bays 2 (two shots a bay, renders kit_<n>_<bay>_left|right)


if __name__ == "__main__":
    main()
