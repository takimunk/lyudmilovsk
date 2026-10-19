"""Synthesises audio/vent_air.wav: a seamless 8 s loop of air hissing out of a vent outlet.

Band-limited noise with slow gusts. Pure standard library:  python3 tools/gen_vent_air.py
"""
import math
import os
import random
import struct
import wave

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "audio", "vent_air.wav")
RATE, SECONDS, FADE = 48000, 8, 1.0

n = RATE * SECONDS
fade = int(RATE * FADE)
random.seed(11)

# Noise -> high-pass (remove rumble) -> gentle low-pass (soften the top): an airy hiss.
sig = []
lp = hp_in = hp_out = 0.0
for _ in range(n + fade):
    x = random.uniform(-1, 1)
    hp_out = 0.995 * (hp_out + x - hp_in)
    hp_in = x
    lp += 0.25 * (hp_out - lp)
    sig.append(lp)
for i in range(fade):
    t = i / fade
    sig[i] = sig[i] * t + sig[n + i] * (1 - t)
sig = sig[:n]
peak = max(abs(v) for v in sig) or 1.0

frames = bytearray()
for i in range(n):
    t = i / RATE
    gust = 0.75 + 0.15 * math.sin(2 * math.pi * t / 8) + 0.1 * math.sin(2 * math.pi * 3 * t / 8 + 1.3)
    v = 0.5 * gust * sig[i] / peak
    frames += struct.pack("<h", int(max(-1, min(1, v)) * 32767))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with wave.open(OUT, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(bytes(frames))
print(OUT)
