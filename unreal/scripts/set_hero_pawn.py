"""Makes the hero (AHeroCharacter, first person in the hero's own body) the player pawn of
BP_FirstPersonGameMode, or switches back to the template's BP_FirstPersonCharacter with "template".
Headless: UnrealEditor <uproject> -run=pythonscript -script="<this file> [hero|template]"
"""
import sys

import unreal

GM = "/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode"
which = (sys.argv[1:] or ["hero"])[0]
pawn = unreal.load_class(None, "/Script/Lyudmilovsk.HeroCharacter") if which == "hero" else \
    unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
bp = unreal.load_asset(GM)
cdo = unreal.get_default_object(unreal.load_class(None, GM + ".BP_FirstPersonGameMode_C"))
cdo.set_editor_property("default_pawn_class", pawn)
unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log_warning("HERO default pawn -> %s" % cdo.get_editor_property("default_pawn_class").get_name())
