"""The restaurant «Янтарь» in Unreal (docs/restaurant.md).

  import: design/restaurant/game/restaurant_kit.glb (blender/restaurant/building.py, pack.py) and tableware_kit.glb
          (blender/restaurant/tableware.py, pack_tableware.py) -> /Game/Restaurant/Meshes/SM_Rest_*, SM_RT_* (tableware), SM_RF_*
          (food: bite stages, loads, fills); flat materials MI_RS_<slot> from every MAT_SPEC in blender/restaurant/*.py, clear glass;
          copies design/restaurant/tableware.json to Content/Restaurant/tableware.json (read by ALyudRestaurant).
  place:  place(face_y) puts the pavilion on the city plan's plot (Content/City/city_plan.json plots[id=restaurant.json plot]:
          its map centre, facing and ground; the floor `podium` m over the ground): the meshes tagged "Restaurant" (shell and
          insides also "Roof"), ALyudRestaurant (lamps, hours, tables, the waiter) and the ALyudDoors of layout.json (the outer ones
          also "RestaurantOuter": locked when closed).

Headless:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_restaurant.py import"
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script="unreal/scripts/import_restaurant.py place"
import_world.py calls place() too, so a full build_scene01.sh keeps the restaurant.
"""
import ast
import glob
import json
import os
import shutil
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools"))

KITS = [os.path.join(REPO, "design", "restaurant", "game", n) for n in ("restaurant_kit.glb", "tableware_kit.glb")]
CONTENT = os.path.join(REPO, "unreal", "Lyudmilovsk", "Content")
LAYOUT = os.path.join(CONTENT, "Restaurant", "layout.json")
CONFIG = os.path.join(CONTENT, "Restaurant", "restaurant.json")
CITY = os.path.join(CONTENT, "City", "city_plan.json")
DEST = "/Game/Restaurant"
MESHES = DEST + "/Meshes"
MATS = DEST + "/Materials"
TAG = "Restaurant"
NEAR_CM = 6000.0
BUILDING = ("SM_Rest_Shell", "SM_Rest_Inside", "SM_Rest_Glass", "SM_Rest_Furniture", "SM_Rest_Lamps", "SM_Rest_Sign")

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning("RESTAURANT " + msg)


def _specs():
    """Every MAT_SPEC dict literal in blender/restaurant/*.py (the building's and the tableware's), without bpy."""
    out = {}
    for path in sorted(glob.glob(os.path.join(REPO, "blender", "restaurant", "*.py"))):
        tree = ast.parse(open(path).read())
        for n in tree.body:
            if isinstance(n, ast.Assign) and getattr(n.targets[0], "id", "").endswith("MAT_SPEC"):
                try:
                    out.update(ast.literal_eval(n.value))
                except ValueError:
                    log("MAT_SPEC in %s is not a literal" % os.path.basename(path))
    return out


def _lin(hx):
    hx = hx.lstrip("#")
    c = [int(hx[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def make_materials():
    import academ_materials as A
    flat = A.build_flat(MATS + "/M_RS_Flat")
    clear = A.build_glass_clear(MATS + "/M_RS_GlassClear")
    out = {}
    for slot, spec in _specs().items():
        hx, rough, metal, emissive = (list(spec) + [0.0, 0.0, 0.0])[:4]
        if "Glass" in slot and not slot.endswith("Glow"):
            mi = A.instance("%s/MI_%s" % (MATS, slot), clear)
            c = _lin(hx)
            A._set(mi, "v", "Color", unreal.LinearColor(c[0] * 0.3, c[1] * 0.3, c[2] * 0.3, 1))
            A._set(mi, "s", "Opacity", 0.14)
            A._set(mi, "s", "OpacityGrazing", 0.6)
        else:
            mi = A.instance("%s/MI_%s" % (MATS, slot), flat)
            c = _lin(hx)
            A._set(mi, "v", "Color", unreal.LinearColor(c[0], c[1], c[2], 1))
            A._set(mi, "s", "Roughness", rough)
            A._set(mi, "s", "Metallic", metal)
            A._set(mi, "s", "Emissive", emissive)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        out[slot] = mi
    return out


def import_meshes(only=None):
    """Each kit in a process of its own (Interchange may skip a second glTF import in one process): `import kit=restaurant_kit`."""
    mats = make_materials()
    count = 0
    for kit in KITS:
        if only and only not in os.path.basename(kit):
            continue
        if not os.path.exists(kit):
            log("missing kit %s" % kit)
            continue
        tmp = DEST + "/_import"
        src = unreal.InterchangeManager.create_source_data(kit)
        params = unreal.ImportAssetParameters()
        params.is_automated = True
        mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
        mgr.import_asset(tmp, src, params)
        mgr.wait_until_all_tasks_done(True)
        for path in eal.list_assets(tmp, True):
            sm = eal.load_asset(path)
            if not isinstance(sm, unreal.StaticMesh):
                continue
            name = sm.get_name()
            if not name.startswith("SM_"):
                name = "SM_" + name
            glass = name.endswith("_Glass")
            building = name in BUILDING
            for i, sl in enumerate(sm.get_editor_property("static_materials")):
                key = str(sl.material_slot_name)
                # glTF import may add .001 to duplicated material names
                base = key.split(".")[0]
                mi = mats.get(base) or next((v for k, v in sorted(mats.items(), key=lambda kv: -len(kv[0])) if base.startswith(k)), None)
                if mi is not None:
                    sm.set_material(i, mi)
                else:
                    log("no material for slot %s on %s" % (key, name))
            body = sm.get_editor_property("body_setup")
            if body:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            # Interchange turns Nanite on by itself, and its glTF parent lacks the Nanite usage flag: set it explicitly (off for the
            # small and the glass meshes, on for the building's opaque shells with a full fallback for the complex collision)
            settings = sm.get_editor_property("nanite_settings")
            settings.enabled = building and not glass and name != "SM_Rest_Lamps"
            settings.fallback_target = unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
            settings.fallback_percent_triangles = 1.0
            sm.set_editor_property("nanite_settings", settings)
            target = MESHES + "/" + name
            if eal.does_asset_exist(target):
                eal.delete_asset(target)
            eal.save_loaded_asset(sm)
            eal.rename_asset(sm.get_path_name(), target)
            count += 1
        eal.delete_directory(tmp)
    eal.save_directory(DEST)
    tw = os.path.join(REPO, "design", "restaurant", "tableware.json")
    if os.path.exists(tw):
        shutil.copyfile(tw, os.path.join(CONTENT, "Restaurant", "tableware.json"))
    log("imported %d meshes" % count)


def plot():
    """The city plan's plot for the restaurant: (ue x, ue y, ground z cm, yaw deg)."""
    with open(CONFIG) as f:
        cfg = json.load(f)
    with open(CITY) as f:
        city = json.load(f)
    p = next(p for p in city["plots"] if p["id"] == cfg.get("plot", "P-RESTAURANT"))
    return p["ue"][0], p["ue"][1], p["ue"][2] + 100.0 * cfg.get("podium", 0.6), p["yaw"]


def place(face_y=None, report=False):
    """Spawns the restaurant in the open level (face_y unused: the city plan's plot carries Unreal coordinates)."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            eas.destroy_actor(a)
    with open(LAYOUT) as f:
        layout = json.load(f)
    ux, uy, uz, yaw = plot()
    origin = unreal.Vector(ux, uy, uz)
    import math
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def to_ue(lx, ly, lz):
        # the building's frame: Blender (x, y, z) m -> the mesh's (x, -y, z) cm, turned by the plot's yaw
        x, y = lx * 100.0, -ly * 100.0
        return (ux + x * c - y * s, uy + x * s + y * c, uz + lz * 100.0)
    n = 0
    for name in layout.get("parts", BUILDING):
        sm = unreal.load_asset(MESHES + "/" + name)
        if sm is None:
            log("missing mesh %s" % name)
            continue
        a = eas.spawn_actor_from_class(unreal.StaticMeshActor, origin, unreal.Rotator(0.0, 0.0, yaw))
        a.static_mesh_component.set_static_mesh(sm)
        a.set_actor_label("Restaurant_" + name[8:])
        glass = name.endswith("_Glass") or name == "SM_Rest_Glass"
        a.tags = [TAG, "NoGrass", "NoTree"] + ([] if glass or name == "SM_Rest_Furniture" else ["Roof"])
        a.set_folder_path("Outside/Restaurant")
        comp = a.static_mesh_component
        comp.set_editor_property("lighting_channels", unreal.LightingChannels(True, True, False))
        if glass:
            comp.set_editor_property("cast_shadow", False)
            comp.set_collision_profile_name("BlockAll")
        if name in ("SM_Rest_Inside", "SM_Rest_Furniture", "SM_Rest_Lamps"):
            comp.set_editor_property("ld_max_draw_distance", NEAR_CM)
        if name in ("SM_Rest_Lamps", "SM_Rest_Sign"):
            comp.set_collision_profile_name("NoCollision")
        n += 1
    cls = unreal.load_class(None, "/Script/Lyudmilovsk.LyudRestaurant")
    d = eas.spawn_actor_from_class(cls, origin, unreal.Rotator(0.0, 0.0, yaw))
    d.set_actor_label("Restaurant")
    d.tags = [TAG]
    d.set_folder_path("Outside/Restaurant")
    import place_doors
    leaf = unreal.load_asset(MATS + "/MI_RS_Bronze") or unreal.load_asset(MATS + "/MI_RS_Frame")
    for outside in (True, False):
        place_doors.spawn_doors([e for e in layout["doors"] if bool(e.get("outside")) == outside], to_ue,
                                tags=[TAG] + (["RestaurantOuter"] if outside else []), folder="Outside/Restaurant/Doors",
                                label_prefix="Restaurant_Door_", leaf_material=leaf, frame_material=leaf, log=log)
    log("placed %d parts at ue (%.0f, %.0f, %.0f), yaw %.1f" % (n, ux, uy, uz, yaw))


if __name__ == "__main__":
    if "import" in sys.argv[1:]:
        kit = next((a[4:] for a in sys.argv[1:] if a.startswith("kit=")), None)
        import_meshes(kit)
    if "place" in sys.argv[1:]:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not les.load_level("/Game/Scene01/Lvl_Scene01"):
            raise RuntimeError("no /Game/Scene01/Lvl_Scene01")
        place()
        les.save_current_level()
        log("done")
