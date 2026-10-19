"""Creates /Game/Characters/Zoomer/LS_SitAtComputer: the "sit at the computer" cutscene.

The hero (spawnable) steps to the chair, sits down, puts his hands on the keyboard and starts typing;
the camera follows him and then flies to the monitor until the screen fills the frame.
Authored in the space of the AHeroSeat actor (placed on the floor at the chair, facing +X towards the screen; the keyboard is ~97 cm and the screen ~114 cm ahead); AHeroSeat sets it as the sequence Transform Origin.

Run headless after import_zoomer.py:
  UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=<this file>
"""
import math
import unreal

DEST = "/Game/Characters/Zoomer"
SK = DEST + "/character_zoomer/SkeletalMeshes/"
MESH = SK + "character_zoomer.character_zoomer"
SIT = SK + "character_zoomerZoomer_SitDown.character_zoomerZoomer_SitDown"
IDLE = SK + "character_zoomerZoomer_TypeIdle.character_zoomerZoomer_TypeIdle"

CHAIR = unreal.Vector(0.0, 0.0, 0.0)             # sequence is authored in the AHeroSeat actor space (Transform Origin)
HERO_YAW = -90.0                    # the imported mesh faces +Y at yaw 0; the sitter looks along +X
HERO_OFFSET = unreal.Vector(22.0, 0.0, -4.0)   # sits on the front of the seat, keyboard is 97 cm ahead
FPS = 24
TICKS = 1000                        # tick resolution 24000 / display 24

eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("SITSEQ " + msg)


def world(f, r, z):
    """chair-local (forward, right, up) in cm -> world vector. Forward is +X, right is +Y."""
    return unreal.Vector(CHAIR.x + f, CHAIR.y + r, CHAIR.z + z)


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    return pitch, yaw


if eal.does_asset_exist(DEST + "/LS_SitAtComputer"):
    eal.delete_asset(DEST + "/LS_SitAtComputer")
seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "LS_SitAtComputer", DEST, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_tick_resolution(unreal.FrameRate(FPS * TICKS, 1))

SIT_FRAMES = 110
FOCUS_START, END = 150, 210         # camera flies to the monitor between these frames
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


# ---- hero
hero = seq.add_spawnable_from_class(unreal.SkeletalMeshActor)
hero.set_name("Hero")
tmpl = hero.get_object_template()
tmpl.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(MESH))
tmpl.skeletal_mesh_component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_CUSTOM_MODE)
ch = tchannels(hero)
vals = [CHAIR.x + HERO_OFFSET.x, CHAIR.y + HERO_OFFSET.y, CHAIR.z + HERO_OFFSET.z, 0.0, 0.0, HERO_YAW, 1, 1, 1]
for c, v in zip(ch, vals):
    key(c, 0, v)

anim_track = hero.add_track(unreal.MovieSceneSkeletalAnimationTrack)
s1 = anim_track.add_section()
s1.params.animation = unreal.load_asset(SIT)
s1.set_range(0, SIT_FRAMES * TICKS)
s2 = anim_track.add_section()
s2.params.animation = unreal.load_asset(IDLE)
s2.set_range(SIT_FRAMES * TICKS, END * TICKS)
s2.params.set_editor_property("loop", True) if hasattr(s2.params, "loop") else None

# ---- warm fill light on the hero (the corridor is dark)
lamp = seq.add_spawnable_from_class(unreal.PointLight)
lamp.set_name("HeroFill")
lt = lamp.get_object_template()
lt.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
lc = lt.light_component
lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
lc.set_intensity(25.0)
lc.set_light_color(unreal.LinearColor(1.0, 0.75, 0.5, 1.0))
lc.set_attenuation_radius(500.0)
lc.set_editor_property("source_radius", 30.0)
ch = tchannels(lamp)
for c, v in zip(ch, [world(0, -70, 180).x, world(0, -70, 180).y, world(0, -70, 180).z, 0, 0, 0, 1, 1, 1]):
    key(c, 0, v)

# ---- camera
cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
cam.set_name("Camera")
ch = tchannels(cam)
looks = [  # frame, camera pos (forward, right, up), target (forward, right, up)
    (0, (-150, -120, 145), (22, -30, 105)),
    (40, (-90, -150, 135), (22, -10, 105)),
    (110, (20, -190, 125), (60, 0, 95)),
    (FOCUS_START, (20, -190, 125), (60, 0, 95)),
    (END, (55, 0, 114), (150, 0, 114)),
]
for f, p, t in looks:
    src, dst = world(*p), world(*t)
    pitch, yaw = look_at(src, dst)
    for c, v in zip(ch, [src.x, src.y, src.z, 0.0, pitch, yaw, 1, 1, 1]):
        key(c, f, v)

cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
cut = cut_track.add_section()
cut.set_range(0, END * TICKS)
cut.set_camera_binding_id(unreal.MovieSceneSequenceExtensions.get_binding_id(seq, cam))

eal.save_asset(DEST + "/LS_SitAtComputer")
log("done")
