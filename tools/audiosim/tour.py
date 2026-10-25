"""Joins the offscreen recordings (unreal/scripts/test_audio.sh) into one listening tour, 1 s cross-fades, as AAC.

    /Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 tools/audiosim/tour.py <out.m4a> <a.wav> <b.wav> ...
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

out, files = sys.argv[1], sys.argv[2:]
rate, parts = None, []
for f in files:
    with wave.open(f) as w:
        rate = w.getframerate()
        a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32).reshape(-1, w.getnchannels()) / 32768.0
    parts.append(a[int(0.3 * rate):])
x = int(1.0 * rate)
tour = parts[0]
for p in parts[1:]:
    t = np.linspace(0, 1, x)[:, None]
    tour = np.concatenate([tour[:-x], tour[-x:] * (1 - t) + p[:x] * t, p[x:]])
with tempfile.TemporaryDirectory() as tmp:
    wav = os.path.join(tmp, "tour.wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(tour.shape[1])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(tour, -1, 1) * 32767).astype("<i2").tobytes())
    subprocess.run(["afconvert", "-f", "m4af", "-d", "aac", "-b", "192000", wav, out], check=True)
print(out, "%.0f s" % (len(tour) / rate))
