"""The Aton coast (docs/south.md: the land between the club and the town) in Unreal. It borrows import_north.py's
machinery (manifests, Interchange import, collision from the manifest, spawn on the ground) with its own names:

  materials: /Game/South/Materials/M_South_Text_<id> for every textures/localized/ru/south_<id>.png (the boards', the
             finger-posts' and the stele's words; LyudText::LoadTexture("South", "south_<id>") swaps the language). The
             props' atlas is the North's M_North_Atlas (import_north.py materials makes it).
  import <file.glb>: one file of blender/export/south (blender/south/build_south.py; one process per file, as the North's)
             -> /Game/South/Meshes/SM_South_<Kind>[_V<n>] by blender/export/south/manifest_*.json.
  place(face_y): from import_world.py (and `place` here): Content/South/south.json "things" in the open level, tagged
             "South", folder Outside/South: StaticMeshActors on terrain.ground_z (or at a thing's own "z"); things of
             "set": "north" use the North's meshes (/Game/North/Meshes). A thing whose mesh is not imported yet is
             skipped with a log line (no placeholder cubes on the trail).

Headless (unreal/scripts/import_south.sh runs them all):
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_south.py materials"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_south.py import belvedere.glb"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_south.py place"
"""
import contextlib
import glob
import json
import math
import os
import sys
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools"))
import import_north as N  # noqa: E402
import world_map as W  # noqa: E402

EXPORT = os.path.join(REPO, "blender", "export", "south")
SOUTH_JSON = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content", "South", "south.json")
DEST = "/Game/South"
MATS = DEST + "/Materials"
TAG = "South"
FOLDER = "Outside/South"
FAR_THINGS = ("cliff_stair", "belvedere", "town_stele", "bird_hide", "fisher_shed", "hay_stack", "rowboat")
SOUTH = {"EXPORT": EXPORT, "DEST": DEST, "MESHES": DEST + "/Meshes", "MATS": MATS, "IMPORTED": DEST + "/Imported",
         "PREFIX": "SM_South_", "LABEL": "South_", "TAG": TAG, "FOLDER": FOLDER}
NORTH_KIT = {"LABEL": "South_", "TAG": TAG, "FOLDER": FOLDER}       # the North's meshes, standing here

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("SOUTH " + msg)


def manifest_names():
    return tuple(sorted(os.path.basename(p) for p in glob.glob(os.path.join(EXPORT, "manifest_*.json"))))


def _slot_materials():
    """The North's atlas and slot instances, and the South's text materials."""
    out = {}
    for d in ("/Game/North/Materials", MATS):
        if not eal.does_directory_exist(d):
            continue
        for p in eal.list_assets(d, False):
            nm = p.split(".")[0].rsplit("/", 1)[-1]
            if nm == "M_North_Atlas" or nm.startswith("M_North_Text_") or nm.startswith("M_South_Text_") or nm.startswith("MI_NO_"):
                out[nm[len("MI_NO_"):] if nm.startswith("MI_NO_") else nm] = unreal.load_asset(p)
    return out


@contextlib.contextmanager
def borrowed(names):
    """import_north's module names set to ours for the time of a call (its manifest cache with them)."""
    names = dict(names)
    if "EXPORT" in names:
        names["MANIFESTS"] = manifest_names()
        names["slot_materials"] = _slot_materials
    old = {k: getattr(N, k) for k in names}
    cache = dict(N._CACHE)
    try:
        for k, v in names.items():
            setattr(N, k, v)
        for k in [k for k in N._CACHE if k != "face_y"]:
            del N._CACHE[k]
        yield
    finally:
        for k, v in old.items():
            setattr(N, k, v)
        face = N._CACHE.get("face_y")
        N._CACHE.clear()
        N._CACHE.update(cache)
        if face is not None and "face_y" not in N._CACHE:
            N._CACHE["face_y"] = face


def make_materials():
    """M_South_Text_<id> from the Russian textures (the game swaps the language)."""
    import academ_materials as A
    n = 0
    for path in sorted(glob.glob(os.path.join(REPO, "textures", "localized", "ru", "south_*.png"))):
        tid = os.path.splitext(os.path.basename(path))[0][len("south_"):]
        try:
            tex = A.import_image(path, DEST, "T_South_Text_" + tid)
            eal.save_loaded_asset(tex, False)
            A.build_picture("%s/M_South_Text_%s" % (MATS, tid), tex)
            n += 1
        except Exception:   # noqa: BLE001
            log("text material %s failed:\n%s" % (tid, traceback.format_exc()))
    if eal.does_directory_exist(DEST):
        eal.save_directory(DEST)
    log("%d text materials" % n)


def import_all(files):
    n = 0
    with borrowed(SOUTH):
        if not files:
            files = sorted({os.path.basename(r.get("file", "")) for r in N.manifests_cached() if r.get("file")})
        for f in files:
            n += N.import_file(f)
    log("imported %d of %d files" % (n, len(files)))


def place(face_y=None):
    """Spawns the Aton coast's things into the open level (removing what an earlier run placed)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if face_y is None:
        with open(os.path.join(REPO, "blender", "export", "scene01_layout.json")) as f:
            face_y = json.load(f).get("outside", {}).get("portal_face_y", 790.0)
    removed = 0
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
            removed += 1
    with open(SOUTH_JSON) as f:
        data = json.load(f)
    placed, missing, spots = 0, {}, 0
    saved_face = N._CACHE.get("face_y")
    N._CACHE["face_y"] = face_y
    try:
        for kit, names in (("south", SOUTH), ("north", NORTH_KIT)):
            with borrowed(names):
                N._CACHE["face_y"] = face_y
                for it in data.get("things", []):
                    if it.get("set", "south") != kit:
                        continue
                    kind, variant = it["kind"], int(it.get("variant", 0))
                    if N.mesh_for(kind, variant) is None:
                        missing[kind] = missing.get(kind, 0) + 1
                        continue
                    z = it.get("z")
                    a = N.spawn_mesh(kind, variant, it["x"], it["y"], float(it.get("yaw", 0.0)), it["id"], _cap(kind),
                                     z=None if z is None else float(z), near=kind not in FAR_THINGS)
                    if kind in ("trail_sign", "info_board", "town_stele"):
                        a.tags = list(a.tags) + [unreal.Name("Text:south")]
                    placed += 1
                    spots += _spots(a, it, N.row_for(kind, variant))
    finally:
        if saved_face is None:
            N._CACHE.pop("face_y", None)
        else:
            N._CACHE["face_y"] = saved_face
    if missing:
        log("no mesh yet, skipped: %s" % ", ".join("%s x%d" % kv for kv in sorted(missing.items())))
    log("placed %d things, %d spots to sit or look (removed %d old actors)" % (placed, spots, removed))


SEAT_CM = 45.0           # the bench pose's cushion over its floor (AHeroCharacter::SitOn)
SEAT_FRONT_CM = 10.0     # the seat's frame is under the cushion's front edge


def _spots(actor, it, row):
    """ALyudSouthSpot actors for a placed thing: one per seat of its manifest row ("seats": [x, y, z, yaw] in the mesh's
    Blender frame, z the cushion's top, yaw counter-clockwise from the front) and one where a viewer's looker stands
    ("stand_at"). The mesh's Blender (x, y, z) m is the actor's local (x, -y, z) * 100; a sitter whose Blender yaw is s
    faces the thing's front yaw minus s."""
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudSouthSpot")
    if cls is None:
        return 0
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    tf = actor.get_actor_transform()
    front = float(it.get("yaw", 0.0))
    rows = [("seat", s[0], s[1], s[2] - SEAT_CM / 100.0, front - s[3], SEAT_FRONT_CM) for s in row.get("seats") or []]
    st = row.get("stand_at")
    if st:
        rows.append(("view", st[0], st[1], st[2], front, 0.0))
    for n, (kind, bx, by, bz, yaw, ahead) in enumerate(rows):
        at = unreal.MathLibrary.transform_location(tf, unreal.Vector(bx * 100.0, -by * 100.0, bz * 100.0))
        r = math.radians(yaw)
        at = unreal.Vector(at.x + math.cos(r) * ahead, at.y + math.sin(r) * ahead, at.z)
        a = eas.spawn_actor_from_class(cls, at, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.set_editor_property("kind", unreal.Name(kind))
        a.set_editor_property("seat_height", SEAT_CM)
        a.set_editor_property("reach", 110.0 if kind == "seat" else 90.0)
        a.set_actor_label("South_%s_%s_%d" % (_cap(kind), it["id"], n + 1))
        a.set_folder_path(FOLDER + "/Spots")
        a.tags = list(a.tags) + [unreal.Name(TAG)]
    return len(rows)


def _cap(s):
    return s[:1].upper() + s[1:]


def place_safe(face_y):
    """import_world.py's hook: a broken or half-built Aton coast never stops the world's build."""
    try:
        place(face_y)
    except Exception:       # noqa: BLE001
        log("place failed:\n" + traceback.format_exc())


if __name__ == "__main__":
    args = sys.argv[1:]
    if "materials" in args:
        make_materials()
    if "import" in args:
        import_all([a for a in args if a.endswith(".glb")])
    if "place" in args:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.load_level("/Game/Scene01/Lvl_Scene01"):
            raise RuntimeError("no /Game/Scene01/Lvl_Scene01")
        place()
        les.save_current_level()
        log("done")
