"""Imports the game screen's pictures (tools/ui_map.py makes them):

  /Game/UI/T_WorldMap     textures/ui/map_world.jpg        the painted world map (no words: the game draws the names)
  /Game/UI/T_Binoculars   textures/ui/binoculars_mask.png  black around two lenses, for the binoculars' view

Item icons are imported by import_hall_assets.py. Run with the editor closed:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_ui_assets.py
"""
import os

import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))


def log(msg):
    unreal.log_warning("UIASSETS " + msg)


def import_texture(src, name, alpha):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = "/Game/UI"
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = eal.load_asset("/Game/UI/" + name)
    if tex is None:
        log("could not import %s" % src)
        return
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    # the map is big: block compressed; the mask needs its soft alpha
    tex.set_editor_property("compression_settings",
                            unreal.TextureCompressionSettings.TC_DEFAULT if not alpha else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    eal.save_loaded_asset(tex)
    log("%s %dx%d" % (name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))


import_texture(os.path.join(ROOT, "textures", "ui", "map_world.jpg"), "T_WorldMap", False)
import_texture(os.path.join(ROOT, "textures", "ui", "binoculars_mask.png"), "T_Binoculars", True)
