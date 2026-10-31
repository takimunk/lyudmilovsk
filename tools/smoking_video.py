"""The drag-and-cough clip from an offscreen run of unreal/scripts/test_smoking.sh with FRAMES=1 (docs/smoking.md "Checks"):
its frames (Saved/SmokeTest/frames, one per game frame at a fixed 24 fps) and, since offscreen runs are silent, the game's own sounds
laid on where the log says they played (a drag's hiss at each big use, a breath out at each exhale, the hero's cough).

  /Applications/Blender.app/Contents/MacOS/Blender -b --python tools/smoking_video.py -- <test log> <frames dir> <out.mp4> [lang]
"""
import os
import re
import sys

import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
args = sys.argv[sys.argv.index("--") + 1:]
LOG, FRAMES, OUT = args[0], args[1], args[2]
LANG = args[3] if len(args) > 3 else "ru"
FPS = 24

# the frames and the events in the order the log has them: an event goes on the frame captured last before it (the log's frame
# counter wraps at 1000, so the order is what counts)
events = []
pending = []
cough_n = 0
current = None
for line in open(LOG, encoding="utf-8", errors="replace"):
    s = re.search(r"SMOKETEST frame (\d+)", line)
    if s:
        current = int(s.group(1))
        # the events since the last frame belong to the recording
        events += [(current - 1, w, v) for (_, w, v) in pending]
        pending = []
        continue
    if current is None:
        continue
    if "HANDUSE use cigarettes big" in line:
        pending.append((current, os.path.join(ROOT, "audio", "sfx", "smoking", "cig_drag_01.wav"), 0.8))
    elif "SMOKE exhale" in line:
        pending.append((current, os.path.join(ROOT, "audio", "sfx", "smoking", "smoke_exhale_%02d.wav" % (len(events) % 3 + 1)), 0.8))
    elif "SMOKE cough" in line:
        cough_n = cough_n % 3 + 1
        pending.append((current, os.path.join(ROOT, "audio", "voice", LANG, "line.hero.cough.%d.wav" % cough_n), 1.0))

# every frame of the fixed step, a dropped screenshot filled by the one before it
have = {int(os.path.splitext(n)[0]): n for n in os.listdir(FRAMES) if n.endswith(".png") and n[0].isdigit()}
if not have:
    raise SystemExit("no frames in " + FRAMES)
seqdir = os.path.join(FRAMES, "seq")
os.makedirs(seqdir, exist_ok=True)
last = None
pngs = []
for i in range(min(have), max(have) + 1):
    last = have.get(i, last)
    dst = os.path.join(seqdir, "%05d.png" % i)
    if not os.path.exists(dst):
        os.link(os.path.join(FRAMES, last), dst)
    pngs.append((i, "%05d.png" % i))
FRAMES = seqdir
first = pngs[0][0]

scene = bpy.context.scene
scene.sequence_editor_create()
seq = scene.sequence_editor
strips = seq.strips if hasattr(seq, "strips") else seq.sequences
img = strips.new_image("frames", os.path.join(FRAMES, pngs[0][1]), 1, 1)
for _, n in pngs[1:]:
    img.elements.append(n)
scene.frame_start, scene.frame_end = 1, len(pngs)
scene.render.fps = FPS
im = bpy.data.images.load(os.path.join(FRAMES, pngs[0][1]))
scene.render.resolution_x, scene.render.resolution_y = im.size
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = "Standard"
placed = 0
for k, (f, wav, vol) in enumerate(events):
    if f < first or f > pngs[-1][0] or not os.path.exists(wav):
        continue
    at = f - first + 1
    s = strips.new_sound("snd%d" % k, wav, 2 + k % 6, at)
    s.volume = vol
    placed += 1
if hasattr(scene.render.image_settings, "media_type"):
    scene.render.image_settings.media_type = "VIDEO"
scene.render.image_settings.file_format = "FFMPEG"
scene.render.ffmpeg.format = "MPEG4"
scene.render.ffmpeg.codec = "H264"
scene.render.ffmpeg.constant_rate_factor = "MEDIUM"
scene.render.ffmpeg.audio_codec = "AAC"
scene.render.ffmpeg.audio_bitrate = 160
scene.render.filepath = OUT
bpy.ops.render.render(animation=True)
print("SMOKEVIDEO %d frames, %d sounds -> %s" % (len(pngs), placed, OUT))
