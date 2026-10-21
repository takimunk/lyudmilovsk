"""Headless test of the hero's reach (HandReach.cpp): poses the hero with the right hand sent to a few targets and
writes every bone's world position to $HALL_REACH_OUT (default /tmp/hero_reach.json).

  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/test_hero_reach.py -unattended -nosplash
"""
import json
import os

import unreal

les = unreal.EditorLevelLibrary
OUT = os.environ.get("HALL_REACH_OUT", "/tmp/hero_reach.json")
unreal.EditorLevelLibrary  # noqa
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level("/Game/Developers/HallReachTest")

HALF = 88.0   # capsule half height: the feet stand on z = 0 at actor z = HALF (the mesh is lifted by the soles)
V = unreal.Vector


def parse(text):
    sets, cur = {}, None
    for line in text.splitlines():
        line = line.strip()
        if line in ("reach", "plain"):
            cur = sets.setdefault(line, {})
        elif line and line != "---" and cur is not None:
            n, x, y, z = line.split()
            cur[n] = [float(x), float(y), float(z)]
    return sets


def run(name, hero_xy, yaw, palm, fd, pn, curl, sc, sa, hl, r, pitch):
    hero = les.spawn_actor_from_class(unreal.HeroCharacter, V(hero_xy[0], hero_xy[1], HALF), unreal.Rotator(0, 0, yaw))
    text = unreal.HeroReachComponent.debug_reach_test(hero, V(*palm), V(*fd), V(*pn), curl, V(*sc), V(*sa), hl, r, pitch)
    res = parse(text)
    res["case"] = name
    res["target"] = {"palm": palm, "fd": fd, "pn": pn, "shape": [sc, sa, hl, r]}
    les.destroy_actor(hero)
    return res


results = []
# a bottle lying on the vending machine's tray (machine front at x = -222), the hero stands facing it (-X)
results.append(run("tray", (-150, 232), 180, (-215, 232, 20.5), (-1, 0, 0), (0, 0, -1), 1.0, (-215, 232, 16.3), (0, 1, 0), 6.0, 3.2, -50))
# an apple on the grass
results.append(run("apple_ground", (0, 0), 0, (60, 10, 8.5), (1, 0, 0), (0, 0, -1), 1.0, (60, 10, 3.8), (0, 0, 1), 0.0, 3.8, -60))
# a bottle on a desk at 75 cm, standing, from the right side
results.append(run("desk_bottle", (0, 0), 0, (45, 6.5, 80), (1, 0, 0), (0, -1, 0), 1.0, (45, 0, 80), (0, 0, 1), 6.0, 3.2, -20))
# the carry pose in front of the eyes and the mouth pose
results.append(run("carry", (0, 0), 0, (38, 14, 128), (0.3, 0, 0.9), (-1, 0, 0), 1.0, (38, 10, 128), (0, 0, 1), 6.0, 3.2, 0))
results.append(run("mouth", (0, 0), 0, (12, 6, 148), (0, 0.5, 0.8), (-0.8, 0, -0.4), 1.0, (10, 2, 152), (-0.25, -0.55, -0.8), 6.0, 3.2, 20))
with open(OUT, "w") as f:
    json.dump(results, f)
unreal.log_warning("HALLREACH written %s" % OUT)
