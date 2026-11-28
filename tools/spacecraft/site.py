"""Where «Ласточка» comes down, and the other places of the flight (docs/spacecraft.md, tools/spacecraft/ENGINE_NOTES.md).

    python3 tools/spacecraft/site.py            -> unreal/Lyudmilovsk/Content/Spacecraft/spacecraft.json (+ why)
    python3 tools/spacecraft/site.py --check    the file is up to date and every rule holds (CI)

The splash point is chosen from the map's own data, not typed in: the nearest open water to the beach under the club
where the capsule can come down at any tide.

Hard rules (a candidate that breaks one is out):
  - at least MIN_DEPTH_LOW m of water at the lowest spring tide over the whole CLEAR_R disc round it (so a reef crest
    or a bar inside the disc counts): at spring low water the tidal flat is DRY for some 330 m out from the beach,
    so "250-400 m off the beach" and "8 m at spring low" cannot both hold; the depth wins, the distance is reported;
  - RAIL_CLEAR m from the sea railway's bank (the line runs along +X at Y = railway.json line.y);
  - HATCH_CLEAR m from the Europa hatch on the bed (europa.json);
  - PIER_CLEAR m from the pier and its head, FISH_CLEAR m outside every fishing spot's own reach (fishing.json) and
    outside the rowing boat's range of its mooring by BOAT_CLEAR m if that can be had (soft);
  - no tidal channel under it (they end with the flat, checked anyway through the disc's depths);
  - in front of the club's beach: Y within CLUB_Y of the hatch's beach.
Among the survivors: the one nearest the beach under the club (the shortest swim home), ties to the north (toward the
town). The cabin's heading on the water (the big porthole looks down-range) is the compass bearing from there to the
town's promenade, clamped to HEADING +- HEADING_FREE round the model's arrival heading (flight.json planet
arrival_heading_deg), so that the island and the town are what the porthole shows under the canopy.

Depths come from tools/terrain.py ground_z: the built landscape when Saved/World has it (reefs and stones included),
else the macro shape (tools/world_map.py). The game traces the real bed again at the splash and moves the point out to
sea in 20 m steps if it finds less than 3 m (ULyudSpaceflightSubsystem).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import terrain as T  # noqa: E402
import world_map as W  # noqa: E402

CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
OUT = os.path.join(CONTENT, "Spacecraft", "spacecraft.json")

MIN_DEPTH_LOW = 8.0     # m of water at the lowest spring tide
CLEAR_R = 25.0          # m: the disc whose every point must be that deep
RAIL_CLEAR = 120.0
HATCH_CLEAR = 60.0
PIER_CLEAR = 250.0
FISH_CLEAR = 60.0
BOAT_CLEAR = 0.0
CLUB_Y = (-20.0, 420.0)  # map Y searched: the beach under the club and north of it toward the town
SEARCH_X = (400.0, 900.0)
STEP = 5.0
HEADING_FREE = 35.0

# Europa and the flight (UE cm unless said)
STAGE_OFFSET_M = 60.0        # the stage stands this far from Europa's hatch, toward the planet's azimuth
REAL_ASCENT_ALT_M = 3000.0   # the cabin really climbs over the real ice up to here (the terrain's rim then lies on
                             # the model's limb: atan(3/16) = 10.6 deg, acos(200/203) = 9.9 deg), then the ice is swapped
                             # for the impostor sphere and the cabin stays here: the "stage spot" of the whole space flight
PLANET_DRAW_CM = 4.5e6       # the planet impostor's centre from the cabin (with its radius it stays inside the stars)
EUROPA_DRAW_CM = 4.0e6
STARS_RADIUS_CM = 1.0e7      # the star sphere (ALyudEuropaSite's is the same)
PLANET_RADIUS_KM = 2000.0    # flight.json planet.radius_km (checked below)


def _json(*rel):
    with open(os.path.join(CONTENT, *rel), encoding="utf-8") as f:
        return json.load(f)


def frame():
    return _json("UI", "map.json")["frame"]


def to_ue(x, y, z_m, face_y):
    return [round(x * 100.0, 1), round(face_y + y * 100.0, 1), round(z_m * 100.0, 1)]


def to_map(ue, face_y):
    return ue[0] / 100.0, (ue[1] - face_y) / 100.0


def depth_low(x, y):
    """Water over the bed at the lowest spring tide, m (negative: dry)."""
    return W.SEA_Z + W.TIDE_SPRING_LOW - T.ground_z(x, y)


def disc_depth(x, y):
    worst = depth_low(x, y)
    for r in (CLEAR_R * 0.5, CLEAR_R):
        for k in range(12):
            a = k * math.pi / 6.0
            worst = min(worst, depth_low(x + r * math.cos(a), y + r * math.sin(a)))
    return worst


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    q = dx * dx + dy * dy
    t = 0.0 if q <= 0.0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / q))
    return math.hypot(px - ax - dx * t, py - ay - dy * t)


def facts():
    fr = frame()
    face_y = fr["portal_face_y"]
    eu = _json("Europa", "europa.json")
    rail_y = float(_json("Railway", "railway.json")["line"]["y"])
    fish = _json("Fishing", "fishing.json")
    spots = []
    for s in fish.get("spots", []):
        cx, cy = to_map(s["ue"], face_y)
        ax, ay = cx + s["a"][0] / 100.0, cy + s["a"][1] / 100.0
        bx, by = cx + s["b"][0] / 100.0, cy + s["b"][1] / 100.0
        spots.append((s["id"], ax, ay, bx, by, s["radius"] / 100.0))
    boat = fish.get("boat", {})
    moor = None
    if boat:
        bx, by = to_map(boat["ue"], face_y)
        moor = (bx + boat["mooring"][0] / 100.0, by + boat["mooring"][1] / 100.0, boat.get("range", 60000) / 100.0)
    places = {p["id"]: p for p in _json("UI", "map.json")["places"]}
    flight = _json("Spacecraft", "flight.json")
    kit = _json("Spacecraft", "kit.json")
    return dict(face_y=face_y, hatch=tuple(eu["hatch_map_m"]), europa=eu, rail_y=rail_y, spots=spots, moor=moor,
                pier=(places["pier"]["x"], places["pier"]["y"]), town=(places["promenade"]["x"], places["promenade"]["y"]),
                flight=flight, kit=kit)


def island_block(x, y):
    """The baked island map on the planet (tools/spacecraft/textures.json "island", M_SC_PlanetNear) and where the
    splash point lies in it: the model's "island" is the splash point, so the planet is turned to put THAT point on
    the model's island axis."""
    with open(os.path.join(HERE, "textures.json"), encoding="utf-8") as f:
        isl = json.load(f).get("island")
    if not isl:
        return {"centre_lat_deg": 54.96, "centre_lon_deg": 20.4, "extent_km": 25.6, "splash_east_km": 0.0, "splash_north_km": 0.0}
    cx, cy = isl["centre_map_m"]
    return {
        "centre_lat_deg": isl["centre_lat_lon_true_scale"][0], "centre_lon_deg": isl["centre_lat_lon_true_scale"][1],
        "extent_km": isl["extent_km"],
        # map X runs west: east = -(X - centre X)
        "splash_east_km": round(-(x - cx) / 1000.0, 4), "splash_north_km": round((y - cy) / 1000.0, 4),
    }


def why_not(x, y, F):
    """The first hard rule a candidate breaks, or None."""
    if abs(y - F["rail_y"]) < RAIL_CLEAR:
        return "railway"
    if math.hypot(x - F["hatch"][0], y - F["hatch"][1]) < HATCH_CLEAR:
        return "hatch"
    if math.hypot(x - F["pier"][0], y - F["pier"][1]) < PIER_CLEAR:
        return "pier"
    for sid, ax, ay, bx, by, r in F["spots"]:
        if seg_dist(x, y, ax, ay, bx, by) < r + FISH_CLEAR:
            return "fishing:" + sid
    if disc_depth(x, y) < MIN_DEPTH_LOW:
        return "depth"
    return None


def choose(F, log=None):
    hx, hy = F["hatch"]
    beach = (W.shore_x(hy), hy)                     # the high-water line in front of the club's beach
    best = None
    counts = {}
    y = CLUB_Y[0]
    while y <= CLUB_Y[1] + 1e-6:
        x = SEARCH_X[0]
        while x <= SEARCH_X[1] + 1e-6:
            why = why_not(x, y, F)
            if why:
                counts[why.split(":")[0]] = counts.get(why.split(":")[0], 0) + 1
            else:
                counts["ok"] = counts.get("ok", 0) + 1
                d = math.hypot(x - beach[0], y - beach[1])
                # soft: out of the rowing boat's reach if it can be had, and not on the hatch's own line to the beach
                soft = 0.0
                if F["moor"] and math.hypot(x - F["moor"][0], y - F["moor"][1]) < F["moor"][2] + BOAT_CLEAR:
                    soft += 40.0
                key = (round(d + soft, 3), -y)
                if best is None or key < best[0]:
                    best = (key, x, y)
            x += STEP
        y += STEP
    if log:
        log("candidates: %s" % ", ".join("%s %d" % kv for kv in sorted(counts.items())))
    if best is None:
        raise SystemExit("site: no splash point satisfies the rules")
    return best[1], best[2], beach


def build(log=None):
    F = facts()
    x, y, beach = choose(F, log)
    face_y = F["face_y"]
    # the heading: toward the town, kept near the model's arrival heading
    arrive = float(F["flight"]["planet"]["arrival_heading_deg"])
    tx, ty = F["town"]
    # compass bearing (0 north, 90 east); the map's X runs WEST, so east is -X
    town_bearing = math.degrees(math.atan2(-(tx - x), ty - y)) % 360.0
    diff = (town_bearing - arrive + 180.0) % 360.0 - 180.0
    heading = (arrive + max(-HEADING_FREE, min(HEADING_FREE, diff))) % 360.0
    # Unreal yaw: 0 = +X (west), 90 = +Y (north)  ->  yaw = 90 + bearing
    yaw = (90.0 + heading + 180.0) % 360.0 - 180.0
    low_line = beach[0]
    while depth_low(low_line, y) < 0.0 and low_line < 2000.0:
        low_line += 1.0
    d_high = x - W.shore_x(y)
    d_low = x - low_line
    hw = W.SEA_Z - T.ground_z(x, y)
    stage_alt = REAL_ASCENT_ALT_M
    origin = F["europa"]["europa_origin_cm"]
    assert abs(float(F["flight"]["planet"]["radius_km"]) - PLANET_RADIUS_KM) < 1e-6, "flight.json planet.radius_km changed"
    assert stage_alt * 100.0 < F["europa"]["space_radius_cm"] * 0.9, "the stage spot must stay inside Europa's space radius"
    data = {
        "_doc": "Written by tools/spacecraft/site.py: do not edit by hand. The places of the flight home (docs/spacecraft.md); UE cm.",
        "splash_map_m": [x, y],
        "splash_cm": to_ue(x, y, W.SEA_Z, face_y),
        "splash_heading_deg": round(heading, 2),
        "splash_yaw_deg": round(yaw, 2),
        "town_bearing_deg": round(town_bearing, 2),
        "depth_high_water_m": round(hw, 2),
        "depth_spring_low_m": round(hw + W.TIDE_SPRING_LOW, 2),
        "disc_depth_spring_low_m": round(disc_depth(x, y), 2),
        "off_high_water_line_m": round(d_high, 1),
        "off_spring_low_line_m": round(d_low, 1),
        "from_hatch_m": round(math.hypot(x - F["hatch"][0], y - F["hatch"][1]), 1),
        "from_railway_m": round(abs(y - F["rail_y"]), 1),
        "from_pier_m": round(math.hypot(x - F["pier"][0], y - F["pier"][1]), 1),
        "terrain": "built" if T._OUT is not None else "macro",
        "min_depth_play_m": 3.0,
        "nudge_step_m": 20.0,
        "stage": {"offset_m": STAGE_OFFSET_M, "pad_trace_cm": 3000.0},
        "flight_spot": {"real_ascent_alt_cm": stage_alt * 100.0, "blend_seconds": 12.0,
                        "europa_origin_cm": origin},
        "draw": {"planet_cm": PLANET_DRAW_CM, "europa_cm": EUROPA_DRAW_CM, "stars_radius_cm": STARS_RADIUS_CM,
                 "planet_radius_km": PLANET_RADIUS_KM, "air_radius": 1.045},
        "island": island_block(x, y),
        "afloat": {"bob_cm": 6.0, "roll_deg": 3.5, "roll_period_s": 5.5, "drift_cm_s": 4.0, "hero_far_cm": 4000.0},
    }
    return data, F, beach


def report(data, F, beach, log):
    x, y = data["splash_map_m"]
    log("splash point: map (%.0f, %.0f) m, UE %s" % (x, y, data["splash_cm"]))
    log("  water: %.1f m at high tide, %.1f m at the lowest spring tide (%.1f m at the shallowest point of the %.0f m disc); bed from the %s terrain"
        % (data["depth_high_water_m"], data["depth_spring_low_m"], data["disc_depth_spring_low_m"], CLEAR_R, data["terrain"]))
    log("  %.0f m off the high-water line of the club's beach (x %.0f), %.0f m off the spring-low waterline: the flat is dry that far at spring low,"
        % (data["off_high_water_line_m"], beach[0], data["off_spring_low_line_m"]))
    log("  so the asked 250-400 m off the beach gives way to the asked 8 m of water (the nearest water that deep)")
    log("  %.0f m from the Europa hatch (>= %.0f), %.0f m north of the sea railway's bank (>= %.0f), %.0f m from the pier (>= %.0f)"
        % (data["from_hatch_m"], HATCH_CLEAR, data["from_railway_m"], RAIL_CLEAR, data["from_pier_m"], PIER_CLEAR))
    log("  heading %.0f deg (the town's promenade bears %.0f deg; the model arrives on %.0f): Unreal yaw %.1f"
        % (data["splash_heading_deg"], data["town_bearing_deg"], float(F["flight"]["planet"]["arrival_heading_deg"]), data["splash_yaw_deg"]))
    log("stage: %.0f m from Europa's hatch toward the planet's azimuth; the real ascent to %.0f m, the flight's spot there"
        % (data["stage"]["offset_m"], data["flight_spot"]["real_ascent_alt_cm"] / 100.0))


def main():
    check = "--check" in sys.argv
    data, F, beach = build(print)
    text = json.dumps(data, indent=1, ensure_ascii=False) + "\n"
    report(data, F, beach, print)
    if check:
        try:
            with open(OUT, encoding="utf-8") as f:
                old = f.read()
        except OSError:
            old = ""
        bad = []
        if old != text:
            bad.append("spacecraft.json is not up to date (run tools/spacecraft/site.py)")
        x, y = data["splash_map_m"]
        if why_not(x, y, F):
            bad.append("the splash point breaks the rule: %s" % why_not(x, y, F))
        for b in bad:
            print("FAIL " + b)
        print("site: %s" % ("%d problems" % len(bad) if bad else "ok"))
        sys.exit(1 if bad else 0)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote " + os.path.relpath(OUT, REPO))


if __name__ == "__main__":
    main()
