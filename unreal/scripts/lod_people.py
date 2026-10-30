"""LODs for the generated people (55-70k triangles each, a town of them): /Game/Characters/Humans/LOD_HumanPeople
(SkeletalMeshLODSettings) gives four LODs, 100 % / 40 % / 15 % / at most 5000 triangles, switching at screen sizes
1.0 / 0.45 / 0.2 / 0.08 (a person ~1.7 m: LOD1 from ~5 m, LOD2 ~12 m, LOD3 ~30 m), and each mesh is regenerated with it.

  UnrealEditor-Cmd <uproject> -run=pythonscript -script="unreal/scripts/lod_people.py Zotov_lev Sato_kei ..." -unattended -nullrhi
(import_character.py calls apply() itself.)
"""
import sys

import unreal

BASE = "/Game/Characters/Humans"
PATH = BASE + "/LOD_HumanPeople"
GROUPS = [(1.0, 100.0, 0), (0.45, 40.0, 0), (0.2, 15.0, 0), (0.08, 100.0, 5000)]   # screen size, % triangles, max triangles
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log_warning("LODPEOPLE " + str(m))


def settings():
    if eal.does_asset_exist(PATH):
        s = unreal.load_asset(PATH)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        s = tools.create_asset(PATH.split("/")[-1], BASE, unreal.SkeletalMeshLODSettings, None)
    groups = []
    for screen, pct, cap in GROUPS:
        g = unreal.SkeletalMeshLODGroupSettings()
        ps = unreal.PerPlatformFloat()
        ps.set_editor_property("default", screen)
        g.set_editor_property("screen_size", ps)
        r = unreal.SkeletalMeshOptimizationSettings()
        r.set_editor_property("num_of_triangles_percentage", pct / 100.0)
        if cap:
            r.set_editor_property("termination_criterion", unreal.SkeletalMeshTerminationCriterion.SMTC_ABS_NUM_OF_TRIANGLES)
            r.set_editor_property("max_num_of_triangles", cap)
        g.set_editor_property("reduction_settings", r)
        groups.append(g)
    s.set_editor_property("lod_groups", groups)
    eal.save_loaded_asset(s)
    return s


def apply(mesh, s=None):
    s = s or settings()
    mesh.set_editor_property("lod_settings", s)
    sme = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sme.regenerate_lod(mesh, len(GROUPS), True, False)
    eal.save_loaded_asset(mesh)
    out = []
    for i in range(sme.get_lod_count(mesh)):
        out.append(sme.get_num_verts(mesh, i))
    log("%s: %d LODs, render verts %s" % (mesh.get_name(), len(out), out))


if __name__ == "__main__":
    s = settings()
    for who in [a for a in sys.argv[1:] if not a.startswith("-")]:
        apply(unreal.load_asset("%s/%s/SK_%s" % (BASE, who, who)), s)
