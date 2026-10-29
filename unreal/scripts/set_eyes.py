"""Writes each generated character's eyes (blender/characters/<id>/eyes.json, from blender/characters/eyes.py) into its
MI_<Id>_body: the openings M_HumanFace cuts and the eyeballs ULyudBodyAnimInstance sets behind them (EyeL/R_Hole,
EyeL/R_Lid, EyeL/R_Ball, EyeL/R_Size, IrisColor). In mesh cm
(Blender metres (x, y, z) -> (100 x, -100 y, 100 z)). docs/characters.md "Face".

  EYES_CAST=Nao,Owner (default: every character with an eyes.json) UnrealEditor-Cmd <uproject>
      -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import json
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CHARS = os.path.join(ROOT, "blender", "characters")
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
cast = [c for c in os.environ.get("EYES_CAST", "").split(",") if c] or \
    sorted(d for d in os.listdir(CHARS) if os.path.isfile(os.path.join(CHARS, d, "eyes.json")))


def lin(c):
    """The iris colour measured on the unlit texture (sRGB), linear, lifted a little (the paint is darker than the
    iris it shows: the lids' shade is painted in)."""
    v = [max(x, 0.0) ** 2.2 for x in c]
    k = max(1.0, 0.06 / max(max(v), 1e-4))
    return unreal.LinearColor(v[0] * k, v[1] * k, v[2] * k, 1.0)


def apply(mi, cid):
    """The eye parameters of one character's MI_<Id>_body; False if it has no eyes.json (its painted eyes stay)."""
    path = os.path.join(CHARS, cid, "eyes.json")
    if not os.path.isfile(path):
        return False
    eyes = json.load(open(path))["eyes"]
    for s, e in zip("LR", eyes):
        o, b = e["open"], e["ball"]
        mel.set_material_instance_vector_parameter_value(mi, "Eye%s_Hole" % s, unreal.LinearColor(
            o["cx"] * 100, -o["y"] * 100, o["cz"] * 100, o["hw"] * 100))
        mel.set_material_instance_vector_parameter_value(mi, "Eye%s_Lid" % s, unreal.LinearColor(
            o["up"] * 100, o["lo"] * 100, o["tilt"] * 100, 0.0))
        # the ball: its middle and turn (Blender's yaw about z, the outer end back), its size and how far further back
        # the dark socket sits
        mel.set_material_instance_vector_parameter_value(mi, "Eye%s_Ball" % s, unreal.LinearColor(
            b["x"] * 100, -b["y"] * 100, b["z"] * 100, b["yaw"]))
        mel.set_material_instance_vector_parameter_value(mi, "Eye%s_Size" % s, unreal.LinearColor(
            b["depth"] * 100, b["width"] * 100, b["height"] * 100, (b["socket_y"] - b["y"]) * 100))
    mel.set_material_instance_vector_parameter_value(mi, "IrisColor", lin(eyes[0]["iris"]))
    return True


if __name__ != "set_eyes":                        # run as a script (import_character.py imports apply())
    for cid in cast:
        name = cid[0].upper() + cid[1:]
        mi = unreal.load_asset("/Game/Characters/Humans/%s/Materials/MI_%s_body" % (name, name))
        if not mi:
            unreal.log_warning("EYES %s: no MI_%s_body" % (cid, name))
            continue
        apply(mi, cid)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        unreal.log_warning("EYES %s: set" % cid)
