"""Joins a folder of numbered PNG frames into an H.264 mp4 with Blender's own ffmpeg (no ffmpeg on the Macs).

  /Applications/Blender.app/Contents/MacOS/Blender -b --python tools/frames_to_mp4.py -- <frames dir> <out.mp4> [fps] [scale %]
"""
import os
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
fps = int(args[2]) if len(args) > 2 else 30
scale = int(args[3]) if len(args) > 3 else 100   # percent of the frames' size
frames = sorted(f for f in os.listdir(src) if f.lower().endswith(".png"))
if not frames:
    raise SystemExit("no frames in " + src)
scene = bpy.context.scene
scene.sequence_editor_create()
strips = scene.sequence_editor.strips if hasattr(scene.sequence_editor, "strips") else scene.sequence_editor.sequences
strip = strips.new_image("frames", os.path.join(src, frames[0]), 1, 1)
for f in frames[1:]:
    strip.elements.append(f)
scene.frame_start = 1
scene.frame_end = len(frames)
scene.render.fps = fps
img = bpy.data.images.load(os.path.join(src, frames[0]))
scene.render.resolution_x, scene.render.resolution_y = img.size[0], img.size[1]
scene.render.resolution_percentage = scale
if hasattr(scene.render.image_settings, "media_type"):
    scene.render.image_settings.media_type = "VIDEO"
scene.render.image_settings.file_format = "FFMPEG"
scene.render.ffmpeg.format = "MPEG4"
scene.render.ffmpeg.codec = "H264"
scene.render.ffmpeg.constant_rate_factor = "HIGH"
scene.render.filepath = out
bpy.ops.render.render(animation=True)
print("MP4 %s: %d frames" % (out, len(frames)))
