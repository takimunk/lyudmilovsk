"""Synthesises audio/vent_hum.wav: a seamless 12 s loop of ventilation drone.

Mains hum (50 Hz and harmonics), a slow fan beat and filtered air noise.
Pure standard library:  python3 tools/gen_vent_hum.py
"""
import math
import os
import random
import struct
import wave

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "audio", "vent_hum.wav")
RATE, SECONDS, FADE = 48000, 12, 1.0

n = RATE * SECONDS
fade = int(RATE * FADE)
random.seed(7)

# Air: brown-ish noise through two one-pole low-passes, generated with an extra tail
# that is cross-faded into the head so the loop has no seam.
air = []
a = b = 0.0
for _ in range(n + fade):
    a += 0.02 * (random.uniform(-1, 1) - a)
    b += 0.05 * (a - b)
    air.append(b)
for i in range(fade):
    t = i / fade
    air[i] = air[i] * t + air[n + i] * (1 - t)
air = air[:n]
peak = max(abs(x) for x in air) or 1.0

frames = bytearray()
for i in range(n):
    t = i / RATE
    beat = 1.0 + 0.25 * math.sin(2 * math.pi * 0.5 * t)          # fan wobble, 2 s period
    hum = (0.30 * math.sin(2 * math.pi * 50 * t) + 0.22 * math.sin(2 * math.pi * 100 * t)
           + 0.08 * math.sin(2 * math.pi * 150 * t) + 0.05 * math.sin(2 * math.pi * 200 * t))
    blade = 0.10 * math.sin(2 * math.pi * 24 * t) * beat           # blade pass rumble
    v = 0.45 * hum * beat + blade + 0.55 * air[i] / peak
    frames += struct.pack("<h", int(max(-1, min(1, v * 0.6)) * 32767))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with wave.open(OUT, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(bytes(frames))
print(OUT)
