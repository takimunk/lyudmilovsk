"""Places the hi3d town props (docs/city/hi3d_props.md) -> unreal/Lyudmilovsk/Content/City/hi3d_props.json, which
ALyudPublicSpaces reads at play start (one instanced set per prop).

    python3 tools/city/hi3d_props.py            # write
    python3 tools/city/hi3d_props.py --check    # write nothing, fail on a clash

Outdoors a place is an anchor (a public spot, a water, a plot's front) plus an offset; the script then looks round it
(spiral, 0.5 m steps, up to `search` m) for the nearest point clear of the layout's props, trees, plants, the fountain
basin, the pond and the other hi3d props, standing on the layout's paving (z = its top) unless the place says
`ground` (a plot's forecourt: z = the plot's front). Indoors (chandeliers) a place names a building type and the lamp
it hangs round (local position, building frame of blender/export/city/<type>.json): the chandelier's top goes to
`ceiling` above the floor at that lamp, in every plot of that type. Footprint radii come from
blender/props_hi3d/game/<id>.json once a prop is optimized (else DEFAULT_R). Props not yet imported are still written
(the game skips a mesh it can't load), so placing and modelling can go in either order.
"""
import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
LAYOUT = os.path.join(CONTENT, "City", "public_spaces.json")
PLAN = os.path.join(CONTENT, "City", "city_plan.json")
EXPORTS = os.path.join(ROOT, "blender", "export", "city")
GAME = os.path.join(ROOT, "blender", "props_hi3d", "game")
SPEC = os.path.join(ROOT, "blender", "props_hi3d", "props.json")
OUT = os.path.join(CONTENT, "City", "hi3d_props.json")

DEFAULT_R = {"sundial": 0.7, "girl_dove": 1.0, "fisherman": 1.3, "drink_fountain": 0.45, "astronomer_bust": 0.6,
             "gull_stele": 1.4, "pioneer": 0.7, "chandelier": 0.8, "cosmonaut": 1.1, "anchor": 1.4, "bear_cub": 0.6,
             "deer": 1.1, "muse": 0.7, "grandfather_clock": 0.4, "ship_model": 0.7, "poet_bust": 0.5, "seal": 1.0,
             "samovar": 0.25, "park_vase": 0.5}
# clearance radius of the layout's own props (m, round their centre); others 0.8
PROP_R = {"stage": 9.0, "carousel": 7.0, "wheel": 9.0, "stall": 2.2, "bench_park": 1.3, "bench_promenade": 1.5,
          "lounger": 1.2, "umbrella": 1.4, "changing_booth": 1.3, "flowerbed": None, "pond": None, "sandpit": 4.0,
          "swings": 3.0, "slide": 3.0, "climber": 3.0, "seesaw": 2.5, "railing": 0.6, "railing_post": 0.4,
          "lamp": 0.6, "lamp_park": 0.5, "tree_grate": 1.0, "bin": 0.5}
INDOOR_PROPS = {"chandelier", "muse", "grandfather_clock", "ship_model", "samovar"}
GAP = 0.8          # free walking room kept between a prop's footprint and its neighbours

# where each thing goes. at: [dx, dy] metres from the anchor (Unreal axes: +x = the sea, +y = north); yaw: the way its
# front looks (0 = the sea, 90 = north, 180 = inland, -90 = south); plot anchors: `out` m in front, `along` m to the right
# when facing the building
PLACES = [
    {"id": "sundial_square", "prop": "sundial", "anchor": "spot:S-FOUNTAIN", "at": [28.0, 0.0], "yaw": 0.0, "search": 4},
    {"id": "girl_dove_pond", "prop": "girl_dove", "anchor": "water:pond", "at": [0.0, -25.5], "yaw": -90.0, "search": 4},
    {"id": "fisherman_pier", "prop": "fisherman", "anchor": "spot:S-PIER-HEAD", "at": [-194.0, -10.0], "yaw": 0.0, "search": 5},
    {"id": "gull_stele_prom", "prop": "gull_stele", "anchor": "spot:S-KERB-16", "at": [-6.0, 10.0], "yaw": 0.0, "search": 6},
    {"id": "drink_square_n", "prop": "drink_fountain", "anchor": "spot:S-FOUNTAIN", "at": [8.0, 26.0], "yaw": -90.0, "search": 5},
    {"id": "drink_square_s", "prop": "drink_fountain", "anchor": "spot:S-FOUNTAIN", "at": [8.0, -26.0], "yaw": 90.0, "search": 5},
    {"id": "drink_playground", "prop": "drink_fountain", "anchor": "spot:S-PLAYGROUND", "at": [7.0, -8.0], "yaw": 180.0, "search": 6},
    {"id": "drink_promenade", "prop": "drink_fountain", "anchor": "spot:S-KERB-27", "at": [-8.0, 4.0], "yaw": 180.0, "search": 5},
    {"id": "bust_library", "prop": "astronomer_bust", "anchor": "plot:P-LIBRARY", "out": 7.0, "along": 6.0, "ground": True, "search": 3},
    {"id": "pioneer_school", "prop": "pioneer", "anchor": "plot:P-SCHOOL", "out": 8.0, "along": -8.0, "ground": True, "search": 3},
    {"id": "cosmonaut_club", "prop": "cosmonaut", "anchor": "plot:P-CLUB2", "out": 9.0, "along": 0.0, "ground": True, "search": 3},
    {"id": "anchor_pier", "prop": "anchor", "anchor": "spot:S-PIER-HEAD", "at": [-194.0, 12.0], "yaw": 0.0, "search": 5},
    {"id": "bear_cub_playground", "prop": "bear_cub", "anchor": "spot:S-PLAYGROUND", "at": [-6.0, 9.0], "yaw": 0.0, "search": 6},
    {"id": "deer_park", "prop": "deer", "anchor": "spot:S-PARK", "at": [10.0, -22.0], "yaw": 90.0, "ground": True, "search": 8},
    {"id": "poet_bust_park", "prop": "poet_bust", "anchor": "spot:S-PARK", "at": [-14.0, -40.0], "yaw": 0.0, "ground": True, "search": 8},
    {"id": "seal_promenade", "prop": "seal", "anchor": "spot:S-KERB-28", "at": [-4.0, 8.0], "yaw": 180.0, "search": 6},
    {"id": "vase_square_nw", "prop": "park_vase", "anchor": "spot:S-FOUNTAIN", "at": [-22.0, 24.0], "yaw": 0.0, "search": 4},
    {"id": "vase_square_sw", "prop": "park_vase", "anchor": "spot:S-FOUNTAIN", "at": [-22.0, -24.0], "yaw": 0.0, "search": 4},
    {"id": "vase_square_ne", "prop": "park_vase", "anchor": "spot:S-FOUNTAIN", "at": [40.0, 24.0], "yaw": 0.0, "search": 4},
    {"id": "vase_square_se", "prop": "park_vase", "anchor": "spot:S-FOUNTAIN", "at": [40.0, -24.0], "yaw": 0.0, "search": 4},
]
# standing indoors on a floor: local [x, y, floor z] + kit yaw (0 = toward the street, 90 = +x) in the building frame,
# searched (0.25 m steps) for a point clear of the building's spots and things on that floor and inside its footprint;
# `on` = stands on a counter/table top at that z (no search)
INDOOR_FLOOR = [
    {"prop": "muse", "building": "workers_club", "at": [12.0, 8.0, 0.0], "yaw": 0.0, "search": 3.0},
    {"prop": "grandfather_clock", "building": "capsule_library", "at": [-12.3, 20.0, 0.0], "yaw": 90.0, "search": 2.0},
    {"prop": "ship_model", "building": "capsule_library", "at": [11.8, 21.0, 0.0], "yaw": 270.0, "search": 2.5},
    {"prop": "samovar", "building": "workers_club", "at": [1.3, 10.4, 1.05], "yaw": 0.0, "on": True},
]
INDOOR = [
    # the cinema hall of the workers' club: its three middle lamps (hall ceiling 10.5 m), the hall's lamps hang inside
    {"prop": "chandelier", "building": "workers_club", "lamps": [[0.0, 16.5], [0.0, 21.5], [0.0, 26.5]], "ceiling": 10.5},
    # the department store's upper shop hall: the four middle lamps of its 4 x 3 grid (ceiling 0.3 m over the lamps)
    {"prop": "chandelier", "building": "department_store", "lamps": [[-5.5, 8.0], [5.5, 8.0], [-5.5, 16.5], [5.5, 16.5]],
     "lamp_z": 9.1, "ceiling_over_lamp": 0.3},
]


def load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def radius(pid):
    p = os.path.join(GAME, pid + ".json")
    return load(p)["radius"] if os.path.exists(p) else DEFAULT_R[pid]


def in_box(b, x, y):
    a = math.radians(b["yaw"])
    dx, dy = x - b["c"][0], y - b["c"][1]
    u = dx * math.cos(a) + dy * math.sin(a)
    v = -dx * math.sin(a) + dy * math.cos(a)
    return abs(u) <= b["size"][0] / 2 and abs(v) <= b["size"][1] / 2


def box_corners_clear(boxes, x, y, r, z_ref):
    """Paving under the whole footprint: the centre and 8 points on the rim each stand on a box near z_ref."""
    tops = []
    for k in range(9):
        px, py = (x, y) if k == 0 else (x + r * math.cos(k * math.pi / 4), y + r * math.sin(k * math.pi / 4))
        hit = [b["c"][2] + b["size"][2] / 2 for b in boxes if b["skin"] not in ("basin",) and in_box(b, px, py)
               and abs(b["c"][2] + b["size"][2] / 2 - z_ref) < 1.6]
        if not hit:
            return None
        tops.append(max(hit))
    return tops[0] if max(tops) - min(tops) < 0.05 else None


def obstacles(L):
    obs = []
    for p in L["props"]:
        if p["kind"] in ("flowerbed", "pond"):
            r = p.get("r", 2.0)
            obs.append((p["at"][0], p["at"][1], (max(r) if isinstance(r, list) else r) + 0.3))
        else:
            obs.append((p["at"][0], p["at"][1], PROP_R.get(p["kind"]) or 0.8))
    obs += [(t["at"][0], t["at"][1], 1.6) for t in L["trees"]]
    obs += [(t["at"][0], t["at"][1], 0.9) for t in L["plants"]]
    for f in L["fountains"]:
        obs.append((f["at"][0], f["at"][1], f["stone"]["rim_r"] + f["stone"]["rim_w"] + 0.3))
    return obs


def pond_clear(L, x, y, r):
    for w in L["waters"]:
        if "r" not in w or isinstance(w.get("r"), (int, float)):
            continue
        rx, ry = w["r"]
        k = w.get("kerb", 0.3)
        dx, dy = x - w["at"][0], y - w["at"][1]
        if (dx / (rx + k + r)) ** 2 + (dy / (ry + k + r)) ** 2 < 1.0:
            return False
    return True


def anchor(L, plan, a):
    kind, name = a.split(":", 1)
    if kind == "spot":
        s = next(s for s in L["spots"] if s["id"] == name)
        return s["pos"]
    if kind == "water":
        return next(w for w in L["waters"] if w["id"] == name)["at"]
    raise SystemExit("anchor " + a)


def plot(plan, pid):
    p = next(p for p in plan["plots"] if p["id"] == pid)
    f = p["front"]
    return [f[0], f[1] + plan["portal_face_y"] / 100.0, f[2]], p["yaw"], p.get("building")


def local_to_world(front, yaw, local):
    a = math.radians(yaw)
    x, y, z = local
    return [front[0] + x * math.sin(a) - y * math.cos(a), front[1] - x * math.cos(a) - y * math.sin(a), front[2] + z]


def place_outdoor(L, plan, P, obs, placed, errors):
    pid = P["prop"]
    r = radius(pid)
    if P["anchor"].startswith("plot:"):
        front, pyaw, _ = plot(plan, P["anchor"][5:])
        a = math.radians(pyaw)
        # out along the plot's yaw (its front looks that way); right of a viewer facing the building = -90 from yaw... the
        # viewer faces pyaw + 180, so their right is pyaw + 90
        x0 = front[0] + math.cos(a) * P["out"] + math.cos(a + math.pi / 2) * P["along"]
        y0 = front[1] + math.sin(a) * P["out"] + math.sin(a + math.pi / 2) * P["along"]
        z_ref, yaw = front[2], P.get("yaw", pyaw)
    else:
        at = anchor(L, plan, P["anchor"])
        x0, y0, z_ref, yaw = at[0] + P["at"][0], at[1] + P["at"][1], at[2], P["yaw"]   # ground: lawn level with the spot
    boxes = L["boxes"]
    step, best = 0.5, None
    for ring in range(int(P.get("search", 4) / step) + 1):
        pts = [(x0, y0)] if ring == 0 else [(x0 + ring * step * math.cos(t * 2 * math.pi / (8 * ring)),
                                             y0 + ring * step * math.sin(t * 2 * math.pi / (8 * ring))) for t in range(8 * ring)]
        for x, y in pts:
            if any(math.hypot(x - ox, y - oy) < r + orad + GAP for ox, oy, orad in obs):
                continue
            if any(math.hypot(x - q["at"][0], y - q["at"][1]) < r + q["_r"] + GAP for q in placed):
                continue
            if not pond_clear(L, x, y, r + GAP):
                continue
            if P.get("ground"):
                hits = [b for b in boxes if in_box(b, x, y) and abs(b["c"][2] + b["size"][2] / 2 - z_ref) < 1.6]
                z = max(b["c"][2] + b["size"][2] / 2 for b in hits) if hits else z_ref
            else:
                z = box_corners_clear(boxes, x, y, r, z_ref)
                if z is None:
                    continue
            best = (x, y, z)
            break
        if best:
            break
    if not best:
        errors.append("%s: no clear paved spot within %s m of (%.1f, %.1f)" % (P["id"], P.get("search", 4), x0, y0))
        return None
    moved = math.hypot(best[0] - x0, best[1] - y0)
    return {"id": P["id"], "prop": pid, "at": [round(best[0], 3), round(best[1], 3), round(best[2], 3)], "yaw": round(yaw, 1),
            "_r": r, "_moved": round(moved, 2)}


def place_indoor(plan, I, errors):
    out = []
    for p in plan["plots"]:
        if p.get("building") != I["building"]:
            continue
        name = "%s__%s.json" % (I["building"], p["id"].lower().replace("-", "_"))
        path = os.path.join(EXPORTS, name) if os.path.exists(os.path.join(EXPORTS, name)) else os.path.join(EXPORTS, I["building"] + ".json")
        exp = load(path)
        front, pyaw, _ = plot(plan, p["id"])
        for k, (lx, ly) in enumerate(I["lamps"]):
            lamp = [l for l in exp["lamps"] if abs(l["pos"][0] - lx) < 0.05 and abs(l["pos"][1] - ly) < 0.05
                    and ("lamp_z" not in I or abs(l["pos"][2] - I["lamp_z"]) < 0.05)]
            if not lamp:
                errors.append("%s %s: no lamp at (%.2f, %.2f) in %s (building changed?)" % (p["id"], I["prop"], lx, ly, os.path.basename(path)))
                continue
            top = I["ceiling"] if "ceiling" in I else lamp[0]["pos"][2] + I["ceiling_over_lamp"]
            w = local_to_world(front, pyaw, [lx, ly, top])
            out.append({"id": "%s_%s_%d" % (I["prop"], p["id"], k + 1), "prop": I["prop"], "at": [round(v, 3) for v in w],
                        "yaw": round(pyaw, 1), "plot": p["id"]})
    return out


def inside_poly(pt, poly):
    x, y = pt
    c = False
    for i in range(len(poly)):
        (x1, y1), (x2, y2) = poly[i], poly[i - 1]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def world_yaw(plot_yaw, local_yaw):
    w = (plot_yaw - local_yaw) % 360.0
    return round(w - 360.0 if w > 180.0 else w, 1)


def place_floor(plan, I, errors):
    out = []
    r = radius(I["prop"])
    for p in plan["plots"]:
        if p.get("building") != I["building"]:
            continue
        name = "%s__%s.json" % (I["building"], p["id"].lower().replace("-", "_"))
        path = os.path.join(EXPORTS, name) if os.path.exists(os.path.join(EXPORTS, name)) else os.path.join(EXPORTS, I["building"] + ".json")
        exp = load(path)
        x0, y0, z = I["at"]
        best = (x0, y0) if I.get("on") else None
        if not best:
            pts = [q["pos"] for q in exp["spots"] + exp["things"] if abs(q["pos"][2] - z) < 2.0]
            for ring in range(int(I.get("search", 2.0) / 0.25) + 1):
                cand = [(x0, y0)] if ring == 0 else [(x0 + ring * 0.25 * math.cos(t * 2 * math.pi / (8 * ring)),
                                                      y0 + ring * 0.25 * math.sin(t * 2 * math.pi / (8 * ring))) for t in range(8 * ring)]
                for x, y in cand:
                    if not inside_poly((x, y), exp["footprint"]):
                        continue
                    if any(math.hypot(x - q[0], y - q[1]) < r + 0.6 for q in pts):
                        continue
                    best = (x, y)
                    break
                if best:
                    break
        if not best:
            errors.append("%s %s: no free floor within %s m of (%.1f, %.1f)" % (p["id"], I["prop"], I.get("search"), x0, y0))
            continue
        front, pyaw, _ = plot(plan, p["id"])
        w = local_to_world(front, pyaw, [best[0], best[1], z])
        out.append({"id": "%s_%s" % (I["prop"], p["id"]), "prop": I["prop"], "at": [round(v, 3) for v in w],
                    "yaw": world_yaw(pyaw, I["yaw"]), "plot": p["id"], "_moved": round(math.hypot(best[0] - x0, best[1] - y0), 2)})
    return out


def main():
    check = "--check" in sys.argv
    L, plan, spec = load(LAYOUT), load(PLAN), load(SPEC)["props"]
    obs = obstacles(L)
    errors, placed = [], []
    for P in PLACES:
        q = place_outdoor(L, plan, P, obs, placed, errors)
        if q:
            placed.append(q)
    for I in INDOOR:
        placed += place_indoor(plan, I, errors)
    for I in INDOOR_FLOOR:
        placed += place_floor(plan, I, errors)
    for q in placed:
        print("%-28s %-16s at %8.2f %8.2f %7.2f yaw %6.1f%s" % (q["id"], q["prop"], *q["at"], q["yaw"],
              "  moved %.1f m" % q["_moved"] if q.get("_moved") else ""))
    for e in errors:
        print("ERROR", e)
    used = sorted({q["prop"] for q in placed})
    doc = {"_doc": "Built by tools/city/hi3d_props.py (docs/city/hi3d_props.md); do not edit by hand. Unreal metres, yaw "
                   "Unreal degrees (the way the prop's front looks). meshes: prop -> static mesh, cast_shadow, collide, "
                   "cull (m); items: one instance each.",
           "meshes": {p: {"mesh": "/Game/City/Hi3D/SM_Hi3d_%s.SM_Hi3d_%s" % (p, p), "collide": bool(spec[p]["collide"]),
                          "cast_shadow": True, "cull": 90.0 if p in INDOOR_PROPS else 250.0,
                          "indoor": p in INDOOR_PROPS} for p in used},
           "items": [{k: v for k, v in q.items() if not k.startswith("_")} for q in placed]}
    if errors:
        sys.exit(1)
    if not check:
        with open(OUT, "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=1, ensure_ascii=False)
            f.write("\n")
        print("wrote", os.path.relpath(OUT, ROOT), len(placed), "items")


if __name__ == "__main__":
    main()
