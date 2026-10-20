"""Creates /Game/Characters/Zoomer/LS_SitAtComputer: the first-person "sit at the computer" cutscene.

The camera is the hero's eyes (the "eye" bone track exported by blender/scripts/build_character_zoomer.py):
he steps to the chair, sits down, his hands come out onto the keyboard and start typing, then the camera
flies to the monitor until the screen fills the frame. The head is hidden by the animation itself, so
only the sleeves and hands are seen. One skeletal-animation section plays once (no second section).

Authored in the space of the AHeroSeat actor (on the floor at the chair, +X towards the screen; the keyboard
is ~60 cm and the screen ~91 cm ahead); AHeroSeat sets it as the sequence Transform Origin.

Run headless after import_zoomer.py:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import json
import math
import os
import unreal

DEST = "/Game/Characters/Zoomer"
SK = DEST + "/character_zoomer/SkeletalMeshes/"
MESH = SK + "character_zoomer.character_zoomer"
ANIM = SK + "character_zoomerZoomer_SitAtComputer_FP.character_zoomerZoomer_SitAtComputer_FP"
HERE = os.path.dirname(os.path.abspath(__file__))
EYE_JSON = os.path.normpath(os.path.join(HERE, "..", "..", "blender", "export", "zoomer_eye_track.json"))

HERO_YAW = -90.0                    # the imported mesh faces +Y at yaw 0; the sitter looks along +X
FPS = 24
TICKS = 1000                        # tick resolution 24000 / display 24
FOCUS_START, END = 150, 210         # camera leaves the eyes and flies to the monitor between these frames
SCREEN_CAM = unreal.Vector(32.0, 0.0, 114.0)     # screen centre is 91 cm ahead at z 114
SCREEN_TARGET = unreal.Vector(150.0, 0.0, 114.0)

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("SITSEQ " + msg)


def to_local(v):
    """Blender character space (m; the hero faces -Y, +X is his left) -> sequence space (cm; forward +X, right +Y)."""
    return unreal.Vector(-v[1] * 100.0, -v[0] * 100.0, v[2] * 100.0)


def dir_local(v):
    return unreal.Vector(-v[1], -v[0], v[2])


def look(fwd):
    return (math.degrees(math.atan2(fwd.z, math.hypot(fwd.x, fwd.y))), math.degrees(math.atan2(fwd.y, fwd.x)))


if eal.does_asset_exist(DEST + "/LS_SitAtComputer"):
    eal.delete_asset(DEST + "/LS_SitAtComputer")
seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "LS_SitAtComputer", DEST, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_tick_resolution(unreal.FrameRate(FPS * TICKS, 1))
seq.set_playback_start(0)
seq.set_playback_end(END)


def frame(f):
    return unreal.FrameNumber(int(f * TICKS))


def tchannels(binding):
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    sec = track.add_section()
    sec.set_range(0, END * TICKS)
    return sec.get_all_channels()


def key(ch, f, v):
    ch.add_key(frame(f), v, 0.0, unreal.MovieSceneTimeUnit.TICK_RESOLUTION)


# ---- hero: one animation section that covers the whole cutscene
hero = seq.add_spawnable_from_class(unreal.SkeletalMeshActor)
hero.set_name("Hero")
tmpl = hero.get_object_template()
tmpl.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(MESH))
tmpl.skeletal_mesh_component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_CUSTOM_MODE)
for c, v in zip(tchannels(hero), [0, 0, 0, 0.0, 0.0, HERO_YAW, 1, 1, 1]):
    key(c, 0, v)
anim_track = hero.add_track(unreal.MovieSceneSkeletalAnimationTrack)
sec = anim_track.add_section()
sec.params.animation = unreal.load_asset(ANIM)
sec.set_range(0, END * TICKS)

# ---- warm fill light near the hands (the corridor is dark)
lamp = seq.add_spawnable_from_class(unreal.PointLight)
lamp.set_name("HeroFill")
lc = lamp.get_object_template().light_component
lc.set_mobility(unreal.ComponentMobility.MOVABLE)
lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
lc.set_intensity(5.0)
lc.set_light_color(unreal.LinearColor(1.0, 0.75, 0.5, 1.0))
lc.set_attenuation_radius(450.0)
lc.set_editor_property("source_radius", 30.0)
for c, v in zip(tchannels(lamp), [-10, 10, 190, 0, 0, 0, 1, 1, 1]):
    key(c, 0, v)

# ---- camera: the eye bone, then the flight to the monitor
cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
cam.set_name("Camera")
cc = cam.get_object_template().get_editor_property("camera_component")
fs = cc.get_editor_property("focus_settings")
fs.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)   # no depth-of-field blur
cc.set_editor_property("focus_settings", fs)
cc.set_editor_property("current_aperture", 16.0)
cc.set_editor_property("current_focal_length", 20.0)                        # wide, like the player camera
ch = tchannels(cam)
with open(EYE_JSON) as fh:
    eye = json.load(fh)["frames"]
for f in range(0, FOCUS_START + 1, 2):
    e = eye[str(f)]
    pos = to_local(e["pos"])
    pitch, yaw = look(dir_local(e["fwd"]))
    for c, v in zip(ch, [pos.x, pos.y, pos.z, 0.0, pitch, yaw, 1, 1, 1]):
        key(c, f, v)
pitch, yaw = look(SCREEN_TARGET - SCREEN_CAM)
for c, v in zip(ch, [SCREEN_CAM.x, SCREEN_CAM.y, SCREEN_CAM.z, 0.0, pitch, yaw, 1, 1, 1]):
    key(c, END, v)

cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
cut = cut_track.add_section()
cut.set_range(0, END * TICKS)
cut.set_camera_binding_id(unreal.MovieSceneSequenceExtensions.get_binding_id(seq, cam))

# ---- the cut from the player's own view to the hero's eyes is hidden by a short fade from black
fade_track = seq.add_track(unreal.MovieSceneFadeTrack)
fade = fade_track.add_section()
fade.set_range(0, END * TICKS)
fch = fade.get_all_channels()[0]
key(fch, 0, 1.0)
key(fch, 12, 0.0)

eal.save_asset(DEST + "/LS_SitAtComputer")
log("done")
