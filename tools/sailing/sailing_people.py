#!/usr/bin/env python3
"""The yacht school's people (docs/yacht_school.md "People (part F)"): real townsfolk belong to the school «Луч» and live its
day. Children 7-11 sail optimists (kids), teens 12-17 cadets and luches (juniors), grown-ups luches (adults); a head coach
(an old racer, the race officer), the children's and the juniors' coaches, an old sailor for a bosun. Their sessions are
the rules' timetable (Private/Sailing/SailSchoolModel.h); the people's life does them as a duty of their group
(PeopleSim FWorld::SetGroupActivity, fed by Private/People/SailPeople.h from the engine's conditions).

An add-on in the manner of tools/town/town_activities.py: it works on the generated town as it stands, is safe to run twice,
--check says whether it has been run; tools/town/town_life.py runs it at its end (a reroll of the town would otherwise lose
the school's people). Run it after tools/sailing/sailing.py (it reads the school's places from Content/City/sailing.json),
and it runs sailing.py again itself when the binding changed (sailing.py fills each slot's "person" from the binding):
    python3 tools/sailing/sailing_people.py            add or refresh
    python3 tools/sailing/sailing_people.py --check    exit 1 when the files are not as this script makes them
    python3 tools/sailing/sailing_people.py --table    the members as a Markdown table (for the doc)

Writes:
  Content/PeopleData/sailing_members.json   who is who: slot, person, their groups and roles, the staff's dialogue ids and
                                            staff roles; the roles' maps (what a coach does while the pupils rig ...)
  Content/PeopleData/sailing_spots.json     the school's places in town_plan.json's spots format (PeopleSim LoadSpots): the
                                            boathouse and its park and slip (S-SAIL-SCHOOL, kind sail_school), the
                                            spectators' places (sail_spectators), the parents' place by the slip (sail_parents)
  Content/PeopleData/activities.json        ACTIVITIES_ROWS at the end of the list (one a line)
  Content/PeopleData/townsfolk.json         the four staff: occupation (the old one kept in "sail_was"), their duties at
                                            S-SAIL-SCHOOL (the bosun's day, the head coach's office hours)
  Content/PeopleData/town_places.json       the beach's slots under the boathouse and the park moved out (school.displaced)
  Content/Localization/town/strings_<lang>.json   the staff's job titles
"""
import collections
import json
import math
import os
import subprocess
import sys
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONTENT = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content")
SAILING = os.path.join(CONTENT, "City", "sailing.json")
DATA = os.path.join(CONTENT, "PeopleData")
FOLK = os.path.join(DATA, "townsfolk.json")
PLACES = os.path.join(DATA, "town_places.json")
PUBLIC = os.path.join(CONTENT, "City", "public_spaces.json")
ACTIVITIES = os.path.join(DATA, "activities.json")
MEMBERS = os.path.join(DATA, "sailing_members.json")
SPOTS = os.path.join(DATA, "sailing_spots.json")
STRINGS = os.path.join(CONTENT, "Localization", "town", "strings_%s.json")
OD = collections.OrderedDict
LANGS = ("ru", "en", "ja")

SCHOOL_ID = "S-SAIL-SCHOOL"
STAFF = OD([
    # slot: occupation, dialogue id (npc_dialogue.txt sections of the words' part), staff role (StaffOnShift), titles m/f
    ("coach_head", ("sail_head_coach", "Sail_coach_head", "coach",
                    {"ru": ("старший тренер парусной школы", "старший тренер парусной школы"), "en": ("head sailing coach", "head sailing coach"),
                     "ja": ("ヨット学校の主任コーチ", "ヨット学校の主任コーチ")})),
    ("coach_kids", ("sail_coach", "Sail_coach_kids", "coach",
                    {"ru": ("тренер по парусному спорту", "тренер по парусному спорту"), "en": ("sailing coach", "sailing coach"),
                     "ja": ("セーリングコーチ", "セーリングコーチ")})),
    ("coach_juniors", ("sail_coach", "Sail_coach_juniors", "coach",
                       {"ru": ("тренер по парусному спорту", "тренер по парусному спорту"), "en": ("sailing coach", "sailing coach"),
                        "ja": ("セーリングコーチ", "セーリングコーチ")})),
    ("bosun", ("sail_bosun", "Sail_bosun", "bosun",
               {"ru": ("боцман яхт-клуба", "боцман яхт-клуба"), "en": ("bosun of the sailing school", "bosun of the sailing school"),
                "ja": ("ヨット学校のボースン", "ヨット学校のボースン")})),
])
STAFF_OCC = {v[0] for v in STAFF.values()}

# who belongs where (FMembership: group, role; the first group of theirs with a programme wins)
GROUPS = {
    "kids": [["kids", ""], ["race_optimist", ""]],
    "cadet": [["juniors", ""], ["race_cadet", ""], ["theory", ""], ["crew", ""]],
    "jun_luch": [["juniors", ""], ["race_luch", ""], ["theory", ""], ["crew", ""]],
    "adults": [["adults", ""], ["race_luch", ""], ["theory", ""], ["crew", ""]],
    "coach_head": [["adults", "coach"], ["race", "officer"], ["all", "coach"], ["staff", ""]],
    "coach_kids": [["kids", "coach"], ["race", "coach2"], ["staff", ""]],
    "coach_juniors": [["juniors", "coach"], ["race", "coach2"], ["staff", ""]],
    "bosun": [["kids", "bosun"], ["juniors", "bosun"], ["adults", "bosun"], ["race", "bosun"], ["all", "bosun"], ["staff", ""]],
    "parent": [["kids_parents", ""]],
}
# a role's own part of each session part (the pupils' activity -> theirs; "" nothing: they go on as before)
_COACH = OD([("sail_change", "sail_coach_wait"), ("sail_rig", "sail_coach_rig"), ("sail_launch", "sail_coach_slip"),
             ("sail_drill", "sail_coach_launch"), ("sail_land", "sail_coach_slip"), ("sail_derig", "sail_coach_rig"),
             ("sail_tea", "sail_coach_tea"), ("sail_shore_lesson", "sail_coach_shore"), ("sail_arrive", "sail_coach_wait"),
             ("sail_theory", "sail_coach_class"), ("sail_briefing", "sail_coach_brief"), ("sail_race", "sail_coach_launch"),
             ("sail_prizes", "sail_coach_prizes")])
ROLES = OD([
    ("coach", _COACH),
    ("officer", _COACH),
    ("coach2", OD([("sail_briefing", "sail_coach_wait"), ("sail_rig", "sail_coach_rig"), ("sail_launch", "sail_coach_slip"),
                   ("sail_race", "sail_coach_mast"), ("sail_land", "sail_coach_slip"), ("sail_derig", "sail_coach_rig"),
                   ("sail_prizes", "sail_coach_prizes")])),
    ("bosun", OD([("sail_change", "sail_bosun_park"), ("sail_rig", "sail_bosun_park"), ("sail_launch", "sail_bosun_slip"),
                  ("sail_drill", "sail_bosun_watch"), ("sail_race", "sail_bosun_watch"), ("sail_land", "sail_bosun_slip"),
                  ("sail_derig", "sail_derig"), ("sail_tea", "sail_coach_tea"), ("sail_shore_lesson", "sail_bosun_mend"),
                  ("sail_arrive", "sail_bosun_mend"), ("sail_theory", "sail_bosun_mend"), ("sail_briefing", "sail_bosun_park"),
                  ("sail_prizes", "sail_coach_prizes")])),
])

# activities.json rows, one a line. "clip": the yacht school clips' name (blender/humans/sail_clips*.py, part D) for the
# body to play once the body-anim knows it; the life ignores the key and the pose (stand, look, sit) is what is drawn till then.
_P = '"places": ["sail_school"], "minutes": [60, 60], "free": 0'
ACTIVITIES_ROWS = [
    # the pupils' parts of a session (the timetable's phases)
    '{"id": "sail_change", %s, "slots": ["sail_changing"], "pose": "stand", "talk": 1, "meets": {"social": 0.4}}' % _P,
    '{"id": "sail_rig", %s, "slots": ["sail_rig"], "pose": "stand", "clip": "Sail_Rig_Mast", "meets": {"fun": 0.3, "nature": 0.4, "sun": 0.3}}' % _P,
    '{"id": "sail_launch", %s, "slots": ["sail_slip"], "pose": "stand", "clip": "Sail_Trolley_Push", "meets": {"fun": 0.3, "nature": 0.5}}' % _P,
    '{"id": "sail_drill", %s, "slots": ["sail_afloat"], "pose": "hidden", "afloat": 1, "meets": {"fun": 0.8, "nature": 0.9, "sun": 0.6}, "costs": {"tired": 0.08, "hunger": 0.08}}' % _P,
    '{"id": "sail_race", %s, "slots": ["sail_afloat"], "pose": "hidden", "afloat": 1, "meets": {"fun": 0.9, "nature": 0.9, "sun": 0.6, "social": 0.3}, "costs": {"tired": 0.1, "hunger": 0.1}}' % _P,
    '{"id": "sail_land", %s, "slots": ["sail_slip"], "pose": "stand", "clip": "Sail_Trolley_Push", "meets": {"nature": 0.5}}' % _P,
    '{"id": "sail_derig", %s, "slots": ["sail_rig", "sail_wash"], "pose": "stand", "clip": "Sail_Hose", "meets": {"nature": 0.3, "social": 0.2}}' % _P,
    '{"id": "sail_tea", %s, "slots": ["sail_class"], "pose": "sit", "talk": 1, "meets": {"hunger": 0.4, "social": 0.6, "tired": 0.2}}' % _P,
    '{"id": "sail_shore_lesson", %s, "slots": ["sail_lawn"], "pose": "look", "clip": "Sail_Knots_Sit", "meets": {"mind": 0.6, "nature": 0.4}}' % _P,
    '{"id": "sail_arrive", %s, "slots": ["sail_door"], "pose": "stand", "talk": 1, "meets": {"social": 0.3}}' % _P,
    '{"id": "sail_theory", %s, "slots": ["sail_class"], "pose": "sit", "clip": "Sail_Listen_Class", "meets": {"mind": 0.7, "social": 0.2}}' % _P,
    '{"id": "sail_briefing", %s, "slots": ["sail_notice"], "pose": "stand", "clip": "Sail_Listen_Class", "meets": {"social": 0.3, "mind": 0.2}}' % _P,
    '{"id": "sail_prizes", %s, "slots": ["sail_podium_crowd"], "pose": "look", "clip": "Sail_Cheer", "meets": {"social": 0.6, "fun": 0.4}}' % _P,
    '{"id": "sail_paint", %s, "slots": ["sail_trestle"], "pose": "stand", "meets": {"fun": 0.2, "nature": 0.4, "social": 0.2}, "costs": {"tired": 0.05}}' % _P,
    '{"id": "sail_layup", %s, "slots": ["sail_boatdoor"], "pose": "stand", "clip": "Sail_Carry_Mast", "meets": {"social": 0.2}, "costs": {"tired": 0.05}}' % _P,
    # the coaches' and the bosun's parts (sailing_members.json "roles")
    '{"id": "sail_coach_wait", %s, "slots": ["sail_coach_door"], "pose": "stand", "talk": 1, "meets": {"social": 0.3}}' % _P,
    '{"id": "sail_coach_rig", %s, "slots": ["sail_coach_rig"], "pose": "stand", "clip": "Sail_Coach_Point", "meets": {"social": 0.4, "nature": 0.3}}' % _P,
    '{"id": "sail_coach_slip", %s, "slots": ["sail_coach_slip"], "pose": "stand", "clip": "Sail_Whistle", "meets": {"nature": 0.4}}' % _P,
    '{"id": "sail_coach_launch", %s, "slots": ["sail_launch_boat"], "pose": "hidden", "afloat": 1, "clip": "Sail_Coach_Drive", "meets": {"nature": 0.8, "sun": 0.5}}' % _P,
    '{"id": "sail_coach_tea", %s, "slots": ["sail_tea_table"], "pose": "stand", "talk": 1, "meets": {"hunger": 0.3, "social": 0.5}}' % _P,
    '{"id": "sail_coach_shore", %s, "slots": ["sail_coach_shore"], "pose": "stand", "clip": "Sail_Coach_Point", "meets": {"social": 0.4, "mind": 0.3}}' % _P,
    '{"id": "sail_coach_class", %s, "slots": ["sail_blackboard"], "pose": "stand", "clip": "Sail_Coach_Point", "meets": {"mind": 0.5, "social": 0.3}}' % _P,
    '{"id": "sail_coach_brief", %s, "slots": ["sail_coach_board"], "pose": "stand", "clip": "Sail_Coach_Megaphone", "meets": {"social": 0.4}}' % _P,
    '{"id": "sail_coach_prizes", %s, "slots": ["sail_podium"], "pose": "stand", "meets": {"social": 0.6}}' % _P,
    '{"id": "sail_coach_mast", %s, "slots": ["sail_mast"], "pose": "look", "clip": "Sail_Flag_Hoist", "meets": {"nature": 0.5}}' % _P,
    '{"id": "sail_bosun_park", %s, "slots": ["sail_bosun_park"], "pose": "stand", "clip": "Sail_Trolley_Push", "meets": {"nature": 0.3}}' % _P,
    '{"id": "sail_bosun_slip", %s, "slots": ["sail_bosun_slip"], "pose": "stand", "clip": "Sail_Launch_Hold", "meets": {"nature": 0.4}}' % _P,
    '{"id": "sail_bosun_watch", %s, "slots": ["sail_mast"], "pose": "look", "clip": "Sail_Watch_Sea", "meets": {"nature": 0.6, "sun": 0.3}}' % _P,
    '{"id": "sail_bosun_mend", %s, "slots": ["sail_bench"], "pose": "sit", "clip": "Sail_Knots_Sit", "meets": {"mind": 0.3, "fun": 0.2}}' % _P,
    # the staff's own duties (townsfolk.json): the bosun's day at the boathouse all year, the head coach's office hours in season
    '{"id": "sail_bosun_day", "places": ["sail_school"], "slots": ["staff"], "pose": "stand", "minutes": [60, 60], "free": 0, "staff": 1, "clip": "Sail_Hose", "meets": {"nature": 0.4, "social": 0.2, "hunger": 0.3}}',
    '{"id": "sail_office", "places": ["sail_school"], "slots": ["staff"], "pose": "stand", "minutes": [60, 60], "free": 0, "staff": 1, "when": "sail_season", "meets": {"social": 0.3, "mind": 0.2}}',
    # the town: spectators on a regatta day, the children's parents by the slip
    '{"id": "watch_regatta", "places": ["sail_spectators"], "slots": ["watch"], "pose": "look", "when": "sail_race", "minutes": [30, 90], "hours": [10.5, 20.5], "ages": [5, 200], "daylight": 1, "temp": [6, 40], "talk": 1, "clip": "Sail_Cheer", "meets": {"fun": 0.5, "nature": 0.6, "social": 0.3, "sun": 0.3}, "base": -0.1, "trait": "social", "day_more": {"5": 0.15, "6": 0.3}}',
    '{"id": "wait_child", "places": ["sail_parents"], "slots": ["sail_parent"], "pose": "look", "when": "sail_kids", "members": "kids_parents", "minutes": [40, 90], "ages": [18, 200], "talk": 1, "clip": "Sail_Watch_Sea", "meets": {"social": 0.4, "nature": 0.5, "sun": 0.3}, "base": 0.5}',
]
ACT_IDS = [json.loads(r)["id"] for r in ACTIVITIES_ROWS]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f, object_pairs_hook=OD)


def dump(d):
    return json.dumps(d, ensure_ascii=False, indent=1) + "\n"


def r3(v):
    return [round(v[0], 3), round(v[1], 3), round(v[2], 3)]


def h01(*parts):
    return (zlib.crc32(":".join(parts).encode()) & 0xFFFFFF) / float(0x1000000)


# --- the school's places ------------------------------------------------------------------------------------------------

def poly_in(p, poly, margin=0.0):
    """Inside a convex polygon grown by margin (m)."""
    n = len(poly)
    sign = 0.0
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        sign += ax * by - bx * ay
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        ex, ey = bx - ax, by - ay
        L = math.hypot(ex, ey)
        # outward distance of p from edge i
        d = ((p[0] - ax) * ey - (p[1] - ay) * ex) / L
        if sign < 0:
            d = -d
        if d > margin:
            return False
    return True


def footprints(sc):
    return {k: sc[k]["corners"] for k in ("boathouse", "park", "slip")}


def build_spots(s):
    sc = s["school"]
    pt = sc["points"]

    def at(k):
        return pt[k]["pos"]

    def yaw(k):
        return round(pt[k]["yaw"] % 360.0, 1)

    def slot(kind, pos, yw, height=0.0, cap=None, reach=None, tag=None):
        o = OD([("kind", kind), ("pos", r3([pos[0], pos[1], pos[2] + height])), ("yaw", round(yw % 360.0, 1)), ("height", height)])
        if cap:
            o["capacity"] = cap
        if tag:
            o["tag"] = tag
        if reach:
            o["reach"] = [r3(q) for q in reach]
        return o

    entrance, gate = at("entrance"), at("gate")
    bh = sc["boathouse"]
    door, bdoor = bh["door"]["pos"], bh["boat_door"]["pos"]
    # the ways in (Reach, walked as lines from the walk grid at the gate): along the park's aisle by the boathouse wall to a
    # door, through it, into the room
    aisle_y = gate[1] - 0.1

    def via_gate(*more):
        return [entrance, gate] + list(more)

    def via_door(p):
        return via_gate([door[0], aisle_y, gate[2]], at("boathouse_door"), [door[0], door[1] - 0.9, door[2]], p)

    def via_boat_door(p):
        return via_gate([bdoor[0], aisle_y, gate[2]], at("boat_door"), [bdoor[0], bdoor[1] - 1.2, bdoor[2]], p)

    def ahead(p, yw, d, side=0.0):
        a = math.radians(yw)
        return [p[0] + d * math.cos(a) - side * math.sin(a), p[1] + d * math.sin(a) + side * math.cos(a), p[2]]

    slots = []
    # pupils
    for q in pt["changing"]["slots"]:
        slots.append(slot("sail_changing", q["pos"], q["yaw"], cap=2, reach=via_boat_door(q["pos"])))
    for b in sc["park_slots"]:
        if b["boat"] == "launch":
            continue
        slots.append(slot("sail_rig", b["rig"], b["yaw"] + 90.0, cap=2, reach=via_gate([b["rig"][0], aisle_y, gate[2]], b["rig"])))
    head, foot = sc["slip"]["head"], sc["slip"]["foot"]
    sx, sy = foot[0] - head[0], foot[1] - head[1]
    L = math.hypot(sx, sy)
    ux, uy = sx / L, sy / L
    slip_yaw = math.degrees(math.atan2(uy, ux))
    for i in range(5):
        d = 1.2 + i * 1.3
        for side in (-1.0, 1.0):
            p = [head[0] + ux * d - uy * side, head[1] + uy * d + ux * side, head[2] + (foot[2] - head[2]) * d / L]
            slots.append(slot("sail_slip", p, slip_yaw + 90.0 * side, cap=2))
    for q in pt["wash"]["slots"]:
        slots.append(slot("sail_wash", q["pos"], q["yaw"], cap=2, reach=via_gate(q["pos"])))
    for q in pt["classroom_seat"]["slots"]:
        slots.append(slot("sail_class", q["pos"], q["yaw"], height=0.45, cap=1, reach=via_door(q["pos"])))
    for q in pt["shore_pupil"]["slots"]:
        slots.append(slot("sail_lawn", q["pos"], q["yaw"], cap=2))
    bd = at("boathouse_door")
    for k in range(3):
        slots.append(slot("sail_door", ahead(bd, yaw("boathouse_door") + 180.0, 1.0 + 0.8 * k, 0.0), yaw("boathouse_door"), cap=3, reach=via_gate(ahead(bd, yaw("boathouse_door") + 180.0, 1.0 + 0.8 * k))))
    nb = at("notice_board")
    for k in range(8):
        p = ahead(nb, yaw("notice_board") + 180.0, 1.6 + 1.1 * (k // 4), (k % 4 - 1.5) * 1.1)
        slots.append(slot("sail_notice", p, yaw("notice_board"), cap=4, reach=via_gate(p)))
    pod = at("podium")
    for k in range(8):
        p = ahead(pod, yaw("podium"), 3.0 + 1.3 * (k // 4), (k % 4 - 1.5) * 1.5)
        slots.append(slot("sail_podium_crowd", p, yaw("podium") + 180.0, cap=4))
    for q in pt["paint_trestle"]["slots"]:
        slots.append(slot("sail_trestle", q["pos"], q["yaw"], cap=2))
    bdo = at("boat_door")
    for k in range(3):
        p = [bdoor[0] + 1.2 * (k - 1), bdoor[1] - 2.0, bdoor[2]]
        slots.append(slot("sail_boatdoor", p, yaw("boat_door") + 180.0, cap=2, reach=via_boat_door(p)))
    # afloat: at the slip's foot, hidden (the engine shows them in the boats); down the slip from its head
    slots.append(slot("sail_afloat", foot, slip_yaw, cap=40, reach=[head, foot]))
    slots.append(slot("sail_launch_boat", foot, slip_yaw, cap=3, reach=[head, foot]))
    # staff
    slots.append(slot("sail_coach_door", ahead(bd, yaw("boathouse_door") + 180.0, 0.6, 1.2), yaw("boathouse_door") + 180.0, cap=2,
                      reach=via_gate(ahead(bd, yaw("boathouse_door") + 180.0, 0.6, 1.2))))
    slots.append(slot("sail_coach_rig", at("rigging"), yaw("rigging"), cap=2, reach=via_gate([at("rigging")[0], aisle_y, gate[2]], at("rigging"))))
    slots.append(slot("sail_coach_slip", at("coach_slip"), yaw("coach_slip"), cap=2))
    slots.append(slot("sail_bosun_slip", at("slip_head"), yaw("slip_head"), cap=1))
    tt = at("tea_table")
    for k in range(3):
        p = [tt[0] + 0.7 * (k - 1), tt[1] - 0.8, tt[2]]
        slots.append(slot("sail_tea_table", p, yaw("tea_table") + 180.0, cap=1, reach=via_door(p)))
    slots.append(slot("sail_coach_shore", at("coach_shore"), yaw("coach_shore"), cap=1))
    slots.append(slot("sail_blackboard", at("blackboard"), yaw("blackboard"), cap=1, reach=via_door(at("blackboard"))))
    slots.append(slot("sail_coach_board", ahead(nb, yaw("notice_board") + 180.0, 0.7), yaw("notice_board") + 180.0, cap=1,
                      reach=via_gate(ahead(nb, yaw("notice_board") + 180.0, 0.7))))
    slots.append(slot("sail_podium", pod, yaw("podium"), cap=2))
    slots.append(slot("sail_mast", ahead(at("signal_mast"), 180.0, 0.8), 0.0, cap=1))
    slots.append(slot("sail_mast", at("coach_mast"), 0.0, cap=1))
    slots.append(slot("sail_bosun_park", gate, yaw("gate"), cap=1, reach=[entrance, gate]))
    slots.append(slot("sail_bosun_park", at("bosun"), yaw("bosun"), cap=1, reach=via_gate(at("bosun"))))
    for q in pt["knot_bench"]["slots"]:
        slots.append(slot("sail_bench", q["pos"], q["yaw"], height=0.45, cap=1, reach=via_gate(q["pos"])))
    # the staff's own duties (bStaff: a slot tagged with their role at S-SAIL-SCHOOL): the bosun by his place in the aisle,
    # the head coach at the notice board (where the hero finds him in his office hours)
    slots.append(slot("staff", at("bosun"), yaw("bosun"), cap=1, tag="bosun", reach=via_gate(at("bosun"))))
    slots.append(slot("staff", ahead(nb, yaw("notice_board") + 180.0, 0.7, -0.8), yaw("notice_board") + 180.0, cap=1, tag="coach",
                      reach=via_gate(ahead(nb, yaw("notice_board") + 180.0, 0.7, -0.8))))
    spots = [OD([("id", SCHOOL_ID), ("kind", "sail_school"), ("pos", r3(entrance)), ("yaw", yaw("entrance")), ("seats", len(slots)),
                 ("name_ru", "ДЮСШ парусного спорта «Луч»"), ("name_en", "The sailing school «Luch»"), ("slots", slots)])]

    def watch_place(pid, kind, qs, cap=2):
        return OD([("id", pid), ("kind", kind), ("pos", r3(qs[len(qs) // 2]["pos"])), ("yaw", round(qs[0]["yaw"] % 360.0, 1)), ("seats", len(qs)),
                   ("slots", [slot("watch", q["pos"], q["yaw"], cap=cap) for q in qs])])
    spots.append(watch_place("S-SAIL-WATCH-BEACH", "sail_spectators", pt["spectators_beach"]["slots"]))
    spots.append(watch_place("S-SAIL-WATCH-PIER", "sail_spectators", pt["spectators_pier"]["slots"]))
    pr = pt["spectators_promenade"]
    prom = [OD([("pos", [pr["pos"][0], pr["pos"][1] + 2.5 * (k - 2.5), pr["pos"][2]]), ("yaw", pr["yaw"])]) for k in range(6)]
    spots.append(watch_place("S-SAIL-WATCH-PROM", "sail_spectators", prom))
    # the children's parents: on the sand south of the slip, seaward of the boathouse, facing the water
    feet = footprints(sc)
    par = []
    for k in range(6):
        p = [158.9 + 1.6 * (k % 3), 885.6 + 1.6 * (k // 3), head[2]]
        if any(poly_in(p, f, 0.6) for f in feet.values()):
            sys.exit("sailing_people: a parents' place at %s falls in the school's footprint" % p[:2])
        par.append(slot("sail_parent", p, 0.0, cap=2))
    spots.append(OD([("id", "S-SAIL-PARENTS"), ("kind", "sail_parents"), ("pos", r3(par[0]["pos"])), ("yaw", 0.0), ("seats", len(par)), ("slots", par)]))
    return spots


# --- who: the members ---------------------------------------------------------------------------------------------------

def windows(s):
    """Per group: (weekday, from, to, summer_only) the school wants them, the half hour before included."""
    tt = s["timetable"]
    out = collections.defaultdict(list)
    for season, rows in (("term", tt["term"]), ("summer", tt["summer"])):
        for e in rows:
            for d in e["days"]:
                out[e["group"]].append((d, e["from"] - 0.5, e["from"] + e["len"], season == "summer"))
    for e in s["races"]["events"]:
        if e.get("rule") != "weekly":
            continue
        for cls in e["classes"]:
            out["race_" + cls].append((e["weekday"], e["from"] - 0.5, e["from"] + e["len"], False))
    se = s["season"]
    for d in se["theory_days"]:
        out["theory"].append((d, se["theory_hours"][0] - 0.5, se["theory_hours"][1], False))
    for d in range(7):
        out["fitout"].append((d, se["fitout_hours"][0] - 0.5, se["fitout_hours"][1], False))
    return out


def busy_for(win, groups):
    return [w for g in groups for w in win.get(g, [])]


def clashes(p, busy):
    """Their fixed hours against the school's: a duty that overlaps (school in the summer holidays aside), a bedtime before
    the end, a wake after the start."""
    for d0, a, b, summer in busy:
        for du in p.get("duties", []):
            if du["activity"].startswith("sail_"):
                continue
            if summer and du["activity"].startswith("school"):
                continue        # the summer timetable is the school holidays (the life has none: the programme wins)
            days = du.get("days", list(range(7))) or []
            h0, h1 = du["hours"]
            if du.get("sun"):
                h0, h1 = 16.0 + h0, 21.75 + h1
            for dd in days:
                if dd == d0 and h0 < b and a < h1:
                    return True
        if p["wake"] > a - 0.25:
            return True
        bed = p["bed"] if p["bed"] > 12 else p["bed"] + 24
        if bed < b + 0.4:
            return True
    return False


def score(p, salt):
    t = p["temper"]
    return 0.55 * t.get("active", 0.5) + 0.35 * t.get("nature", 0.5) + 0.1 * t.get("social", 0.5) + 0.15 * h01(salt, p["key"])


def pick_members(s, people):
    win = windows(s)
    free = [p for p in people if not p.get("stay") and p.get("occupation") != "guest" and p.get("home")]
    taken = set()
    chosen = OD()

    def take(slot_id, p, kind):
        chosen[slot_id] = (p, kind)
        taken.add(p["key"])

    def best(cands, salt, n, bonus=None):
        out = []
        pool = [p for p in cands if p["key"] not in taken]
        while len(out) < n and pool:
            pool.sort(key=lambda p: (-(score(p, salt) + (bonus(p, out) if bonus else 0.0)), p["key"]))
            q = pool.pop(0)
            out.append(q)
            taken.add(q["key"])
        return out

    # staff first: an old racer for the head coach, an old sailor for the bosun, two grown-ups without work for the coaches
    staff_groups = {"coach_head": ["adults", "race_luch", "race_optimist", "theory"], "coach_kids": ["kids", "race_optimist"],
                    "coach_juniors": ["juniors", "race_cadet"], "bosun": ["kids", "juniors", "adults", "race_luch", "theory", "fitout"]}
    rules = {
        "coach_head": lambda p: p["occupation"] in ("pensioner", "sail_head_coach") and 60 <= p["age"] <= 72 and p["temper"].get("active", 0) >= 0.45,
        "bosun": lambda p: p["occupation"] in ("pensioner", "sail_bosun") and 63 <= p["age"] <= 78 and p["temper"].get("nature", 0) >= 0.5,
        "coach_kids": lambda p: p["occupation"] in ("unemployed", "homemaker", "sail_coach") and 22 <= p["age"] <= 45 and p["temper"].get("active", 0) >= 0.5,
        "coach_juniors": lambda p: p["occupation"] in ("unemployed", "homemaker", "sail_coach") and 24 <= p["age"] <= 50 and p["temper"].get("active", 0) >= 0.5,
    }
    for sid in STAFF:
        cands = [p for p in free if rules[sid](p) and not p.get("duties") and not clashes(p, busy_for(win, staff_groups[sid]))]
        # (the bosun is an old sailor: «старый моряк»)
        got = best(cands, "staff:" + sid, 1, (lambda p, out: 0.0 if p.get("female") else 0.4) if sid == "bosun" else None)
        if not got:
            sys.exit("sailing_people: nobody fits the school's %s" % sid)
        take(sid, got[0], sid)

    # children 7-11: brothers and sisters come along now and then
    def sib(p, out):
        return 0.3 if any(q["household"] == p["household"] for q in out) else 0.0
    kids = [p for p in free if 7 <= p["age"] <= 11 and p["occupation"] == "pupil" and not clashes(p, busy_for(win, ["kids", "race_optimist"]))]
    kids = best(kids, "kids", 8, sib)
    kids.sort(key=lambda p: (-p["age"], -score(p, "skill"), p["key"]))
    for i, p in enumerate(kids):
        take("kids_%d" % (i + 1), p, "kids")
    # teens 12-17: the three oldest single-handers (luch), then four cadet helms and their four crews (the youngest)
    jun = [p for p in free if 12 <= p["age"] <= 17 and p["occupation"] == "pupil"
           and not clashes(p, busy_for(win, ["juniors", "race_cadet"]))]
    jun = best(jun, "juniors", 11, sib)
    jun.sort(key=lambda p: (-p["age"], -score(p, "skill"), p["key"]))
    luch = [p for p in jun if not clashes(p, busy_for(win, ["race_luch"]))][:3]
    rest = [p for p in jun if p not in luch]
    for i, p in enumerate(luch):
        take("jun_l%d" % (i + 1), p, "jun_luch")
    for i, p in enumerate(rest[:4]):
        take("jun_c%d" % (i + 1), p, "cadet")
    for i, p in enumerate(rest[4:8]):
        take("jun_x%d" % (i + 1), p, "cadet")
    # grown-ups: free on the evenings and Saturday afternoon, fit, not the club's regulars of the night
    ad = [p for p in free if 18 <= p["age"] <= 58 and p["occupation"] not in {"pupil"} | STAFF_OCC
          and not clashes(p, busy_for(win, ["adults", "race_luch"]))]
    ad = best(ad, "adults", 5)
    ad.sort(key=lambda p: (-score(p, "skill"), p["key"]))
    for i, p in enumerate(ad):
        take("adult_%d" % (i + 1), p, "adults")
    return chosen


def build():
    s = load(SAILING)
    fd = load(FOLK)
    people = fd["people"]
    by_key = {p["key"]: p for p in people}
    # what this script wrote last time comes off first (the staff's old jobs back, our duties gone): then the same pick again
    strings = {lang: load(STRINGS % lang) for lang in LANGS}
    for p in people:
        if "sail_was" in p:
            p["occupation"] = p.pop("sail_was")
            for lang, role in zip(LANGS, p.pop("sail_was_role", [])):
                strings[lang]["npc.%s.role" % p["key"]] = role
        p["duties"] = [d for d in p.get("duties", []) if not d["activity"].startswith("sail_")]
    chosen = pick_members(s, people)
    sailors = {x["slot"]: x for x in s["fleet"]["sailors"]}
    for x in s["fleet"]["sailors"]:
        if x.get("crew"):
            sailors[x["crew"]] = OD([("slot", x["crew"]), ("group", x["group"]), ("helm", x["slot"])])
    for slot_id in sailors:
        if slot_id not in chosen:
            sys.exit("sailing_people: the slot %s has nobody" % slot_id)

    members = []
    all_week = list(range(7))
    for slot_id, (p, kind) in chosen.items():
        m = OD([("slot", slot_id), ("person", p["key"]), ("age", p["age"])])
        if slot_id in STAFF:
            occ, dialogue, role, _ = STAFF[slot_id]
            m["group"] = "staff"
            m["groups"] = GROUPS[slot_id]
            m["dialogue"] = dialogue
            m["staff_role"] = role
            p["sail_was"] = p["occupation"]
            p["sail_was_role"] = [strings[lang].get("npc.%s.role" % p["key"], "") for lang in LANGS]
            p["occupation"] = occ
            if slot_id == "bosun":
                p["duties"].append(OD([("activity", "sail_bosun_day"), ("place", SCHOOL_ID), ("role", "bosun"), ("hours", [9.0, 19.0]), ("days", all_week)]))
            if slot_id == "coach_head":
                # findable: the notice board Tuesday to Saturday 10-13 in season (and at his sessions; out of season the
                # theory evenings Tuesday, Thursday, Saturday 15-18)
                p["duties"].append(OD([("activity", "sail_office"), ("place", SCHOOL_ID), ("role", "coach"), ("hours", [10.0, 13.0]), ("days", [1, 2, 3, 4, 5])]))
        else:
            m["group"] = sailors[slot_id]["group"]
            m["groups"] = GROUPS[kind]
            if sailors[slot_id].get("crew"):
                m["crew"] = sailors[slot_id]["crew"]
            if sailors[slot_id].get("helm"):
                m["helm"] = sailors[slot_id]["helm"]
        members.append(m)
    # the children's parents: who waits for them by the slip
    kid_house = {p["household"] for slot_id, (p, kind) in chosen.items() if kind == "kids"}
    parents = sorted((p for p in people if p["household"] in kid_house and p["age"] >= 20 and p["key"] not in {q["key"] for q, _ in chosen.values()}),
                     key=lambda p: p["key"])
    for p in parents:
        members.append(OD([("slot", ""), ("person", p["key"]), ("age", p["age"]), ("group", "kids_parents"), ("groups", GROUPS["parent"])]))

    out = {}
    out[MEMBERS] = dump(OD([("_doc", "Written by tools/sailing/sailing_people.py (docs/yacht_school.md \"People (part F)\"); do not edit. Who belongs to "
                                     "the sailing school: slot (Content/City/sailing.json's fleet and coaches), person (townsfolk.json key), "
                                     "groups [[group, role]] (PeopleSim FWorld::LoadMembers: while a group has a programme its members do it, "
                                     "the first group of theirs with one wins; a role turns the pupils' part into its own by \"roles\"), "
                                     "dialogue (the staff's talk id, npc_dialogue.txt [Sail_*]), staff_role (StaffOnShift(\"S-SAIL-SCHOOL\", role))."),
                            ("roles", ROLES), ("members", members)]))
    out[SPOTS] = dump(OD([("_doc", "Written by tools/sailing/sailing_people.py from Content/City/sailing.json's school.points; do not edit. "
                                   "town_plan.json's spots format (PeopleSim LoadSpots): pos on the floor + height for seats; a slot's reach "
                                   "is its way on from the walk grid (the gate, the park's aisle, a door), walked as a line. "
                                   "moved: the town places' slots (town_places.json) moved out of the boathouse's and the park's "
                                   "footprints, from their public spot's position (City/public_spaces.json, slot index) to where they are now."),
                          ("spots", build_spots(s))]))
    out[FOLK] = dump(fd)
    for lang in LANGS:
        sd = strings[lang]
        for slot_id, (p, kind) in chosen.items():
            if slot_id in STAFF:
                sd["npc.%s.role" % p["key"]] = STAFF[slot_id][3][lang][1 if p.get("female") else 0]
        out[STRINGS % lang] = dump(sd)

    # the beach's soft slots under the boathouse and the park (sailing.json school.displaced): along the shore out of it
    pd = load(PLACES)
    feet = footprints(s["school"])
    school_pts = [q["pos"] for v in s["school"]["points"].values() for q in ([v] + v.get("slots", []))]
    moved = []
    for pl in pd["places"]:
        for sl in pl.get("slots", []):
            p = sl.get("pos")
            if not p or not any(poly_in(p, f, 0.3) for f in feet.values()):
                continue
            others = [q["pos"] for q in pl["slots"] if q is not sl]
            best_p = None
            for k in range(1, 80):
                for sgn in (-1.0, 1.0):
                    c = [p[0], p[1] + sgn * 0.5 * k, p[2]]
                    if c[1] > 904.0 or any(poly_in(c, f, 1.0) for f in feet.values()):
                        continue
                    if any(math.hypot(c[0] - q[0], c[1] - q[1]) < 2.0 for q in school_pts):
                        continue
                    if any(math.hypot(c[0] - q[0], c[1] - q[1]) < 1.2 for q in others):
                        continue
                    best_p = c
                    break
                if best_p:
                    break
            if not best_p:
                sys.exit("sailing_people: nowhere to move %s's slot at %s" % (pl["id"], p[:2]))
            moved.append((pl["id"], p[:2], best_p[:2]))
            sl["pos"] = [round(best_p[0], 3), round(best_p[1], 3), p[2]]
    out[PLACES] = dump(pd)
    # the record of it, for check_town_data.py (a town place's slots are its public spot's, City/public_spaces.json, but
    # for these): public_spaces.json is tools/public_spaces.py's and keeps them where they were (docs/yacht_school.md)
    public = load(PUBLIC)
    tp_by_id = {p["id"]: p for p in pd["places"]}
    record = []
    for sp in public["spots"]:
        tp = tp_by_id.get(sp["id"])
        if not tp:
            continue
        for i, sl in enumerate(sp.get("slots", [])):
            p = sl["pos"]
            if i < len(tp.get("slots", [])) and any(poly_in(p, f, 0.3) for f in feet.values()):
                q = tp["slots"][i]["pos"]
                record.append(OD([("place", sp["id"]), ("slot", i), ("from", [round(p[0], 3), round(p[1], 3)]),
                                  ("to", [round(q[0], 3), round(q[1], 3)])]))
    spots_doc = json.loads(out[SPOTS], object_pairs_hook=OD)
    spots_doc["moved"] = record
    out[SPOTS] = dump(spots_doc)

    # activities.json is written by hand, one activity a line: ours at the end
    text = open(ACTIVITIES, encoding="utf-8").read()
    lines = [l for l in text.split("\n") if not any('"id": "%s"' % a in l for a in ACT_IDS)]
    end = max(i for i, l in enumerate(lines) if l.strip() == "]")
    last = max(i for i in range(end) if lines[i].strip())
    if not lines[last].rstrip().endswith(","):
        lines[last] = lines[last].rstrip() + ","
    rows = ["    " + r + "," for r in ACTIVITIES_ROWS]
    rows[-1] = rows[-1][:-1]
    lines[last + 1:end] = rows
    out[ACTIVITIES] = "\n".join(lines)
    json.loads(out[ACTIVITIES])
    return out, chosen, members, moved


def table(chosen):
    names = {lang: load(STRINGS % lang) for lang in ("ru", "en")}
    print("| slot | who | age | occupation | household |")
    print("|---|---|---|---|---|")
    for slot_id, (p, kind) in chosen.items():
        k = p["key"]
        print("| `%s` | %s (%s) | %d | %s | %s |" % (slot_id, names["en"].get("npc.%s.name" % k, k), names["ru"].get("npc.%s.name" % k, k), p["age"],
                                                p.get("sail_was", p["occupation"]) if slot_id not in STAFF else "%s (was %s)" % (p["occupation"], p.get("sail_was")),
                                                p["household"]))


def main():
    out, chosen, members, moved = build()
    if "--table" in sys.argv:
        table(chosen)
        return
    stale = [p for p, t in out.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
    if "--check" in sys.argv:
        for p in stale:
            print("sailing_people: %s is not up to date (python3 tools/sailing/sailing_people.py)" % os.path.relpath(p, ROOT))
        sys.exit(1 if stale else 0)
    for p in stale:
        with open(p, "w", encoding="utf-8") as f:
            f.write(out[p])
        print("wrote", os.path.relpath(p, ROOT))
    if MEMBERS in stale:
        # sailing.py fills each slot's "person" from the binding
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "sailing", "sailing.py")], check=True, stdout=subprocess.DEVNULL)
    staff = ", ".join("%s %s" % (sid, chosen[sid][0]["key"]) for sid in STAFF)
    print("sailing people: %d sailors, %s; %d parents; %d beach slots moved%s" % (
        sum(1 for sid in chosen if sid not in STAFF), staff, sum(1 for m in members if m["group"] == "kids_parents"), len(moved),
        "".join("\n  moved %s %s -> %s" % m for m in moved)))


if __name__ == "__main__":
    main()
