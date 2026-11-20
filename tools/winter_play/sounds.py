#!/usr/bin/env python3
"""The winter play's sounds (docs/winter_play.md). Procedural and seeded (numpy + scipy; no generation service, nothing
paid), 48 kHz 16-bit mono, each with a .json sidecar (generator, seed, parameters, measured levels).

  python3 tools/winter_play/sounds.py            write them all
  python3 tools/winter_play/sounds.py skate      only the ones whose name has one of the words

  audio/sfx/winter/skate_stroke_0N.wav    a blade's push on hard ice: a scrape rising and fading
  audio/sfx/winter/skate_glide_01.wav     a seamless loop of a blade gliding (ULyudWinterPlay plays it, louder with speed)
  audio/sfx/winter/skate_stop_0N.wav      a hockey stop: a long hiss of shaved ice
  audio/sfx/winter/ice_crack_0N.wav       a blade over a crack: a click and a hollow knock of the ice
  audio/sfx/winter/ice_fall_0N.wav        a body on the ice: a thud, a slide
  audio/sfx/winter/banya_ladle_0N.wav     water on the hot stones: a burst of hiss, the sizzle dying
  audio/sfx/winter/venik_swish_0N.wav     a birch venik through hot air and on skin: a leafy whoosh and a pat
  audio/sfx/winter/banya_door_0N.wav      the banya's heavy wooden door: a creak and a soft knock
  audio/sfx/winter/snow_scoop_0N.wav      a hand scooping snow and packing it: crunches
  audio/sfx/winter/snowball_throw_0N.wav  a throw: a short whoosh
  audio/sfx/winter/snowball_hit_0N.wav    a snowball bursting: a soft puff and crumbs
  audio/sfx/winter/snow_pat_0N.wav        a palm patting a snowman's ball

Import on the Mac: IMPORT_ONLY=sfx/winter with unreal/scripts/import_audio.py (-> /Game/Audio/Winter/S_<id>_0N; the
soundscape's Winter group finds them by name).
"""
import json
import math
import os
import sys
import wave

import numpy as np
from scipy import signal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RATE = 48000


def write(name, x, seed, params, loop=False, peak_db=-3.0):
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * (10 ** (peak_db / 20.0))
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    out = os.path.join(ROOT, "audio", "sfx", "winter")
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    rms = math.sqrt(float(np.mean((pcm / 32768.0) ** 2))) or 1e-9
    side = {"name": name, "asset": "/Game/Audio/Winter/S_%s" % name, "pool": name.rsplit("_", 1)[0],
            "file": os.path.relpath(path, ROOT), "generator": "tools/winter_play/sounds.py", "seed": seed,
            "rng": "numpy.random.default_rng (PCG64), numpy %s" % np.__version__, "loop": loop, "sample_rate": RATE,
            "bit_depth": 16, "channels": 1, "seconds": round(len(pcm) / RATE, 3), "peak_dbfs": peak_db,
            "rms_dbfs": round(20 * math.log10(rms), 2), "params": params,
            "license": "Procedural, made by the project; CC BY 4.0."}
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("wrote", os.path.relpath(path, ROOT), "%.2f s" % (len(pcm) / RATE))


def t_of(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def band(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (RATE / 2), min(hi, RATE / 2 - 100) / (RATE / 2)], btype="band")
    return signal.lfilter(b, a, x)


def env(t, attack, decay):
    return np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-np.maximum(0.0, t - attack) / decay)


def place(dst, src, at):
    i = int(at * RATE)
    n = min(len(src), len(dst) - i)
    if n > 0:
        dst[i:i + n] += src[:n]


def room(x, seconds, mix, seed):
    rng = np.random.default_rng(seed)
    n = int(seconds * RATE)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (RATE * seconds / 5.0))
    ir[0] = 0
    wet = signal.fftconvolve(x, ir)[:len(x)]
    wet /= (np.max(np.abs(wet)) or 1.0)
    return (1 - mix) * x / (np.max(np.abs(x)) or 1.0) + mix * wet


def skate_stroke(k):
    seed = 2100 + k
    rng = np.random.default_rng(seed)
    d = 0.55 + 0.1 * k
    t = t_of(d)
    noise = rng.standard_normal(len(t))
    # the blade's scrape: a narrow band sweeping up as the push bites, a little grain (ice crystals)
    f0 = 2400 + 400 * k
    x = band(noise, f0 * 0.7, f0 * 2.2) * np.sin(math.pi * np.minimum(1.0, t / d)) ** 1.5
    grain = (rng.random(len(t)) < 0.004) * rng.standard_normal(len(t))
    x += 0.5 * band(grain, 3000, 9000) * np.exp(-t / d)
    write("skate_stroke_%02d" % (k + 1), x, seed, {"seconds": d, "band_hz": [f0 * 0.7, f0 * 2.2]}, peak_db=-6.0)


def skate_glide():
    seed = 2110
    rng = np.random.default_rng(seed)
    n = int(6.0 * RATE)
    # circular shaping keeps the loop seamless: a soft high hiss with a faint ring of the ice
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    shape = np.exp(-((np.log(np.maximum(f, 1.0)) - math.log(4200)) ** 2) / 0.5) + 0.25 * np.exp(-((f - 1650) / 40.0) ** 2)
    x = np.fft.irfft(spec * shape, n)
    t = np.arange(n) / RATE
    x *= 0.8 + 0.2 * np.sin(2 * math.pi * t / 3.0)        # whole cycles in the loop
    write("skate_glide_01", x, seed, {"seconds": 6.0, "centre_hz": 4200, "ring_hz": 1650}, loop=True, peak_db=-9.0)


def skate_stop(k):
    seed = 2120 + k
    rng = np.random.default_rng(seed)
    d = 0.9 + 0.15 * k
    t = t_of(d)
    x = band(rng.standard_normal(len(t)), 1800, 11000) * env(t, 0.03, d * 0.45)
    x += 0.3 * band(rng.standard_normal(len(t)), 400, 1500) * env(t, 0.01, 0.15)
    write("skate_stop_%02d" % (k + 1), x, seed, {"seconds": d}, peak_db=-4.0)


def ice_crack(k):
    seed = 2130 + k
    rng = np.random.default_rng(seed)
    d = 0.7
    t = t_of(d)
    click = band(rng.standard_normal(len(t)), 2000, 12000) * env(t, 0.001, 0.008)
    # the ice's hollow knock: a falling "pew" of a sheet under tension
    f = 900 * np.exp(-t / 0.12) + 160
    knock = np.sin(2 * math.pi * np.cumsum(f) / RATE) * env(t, 0.002, 0.12 + 0.04 * k)
    write("ice_crack_%02d" % (k + 1), click + 0.6 * knock, seed, {"seconds": d}, peak_db=-5.0)


def ice_fall(k):
    seed = 2140 + k
    rng = np.random.default_rng(seed)
    d = 1.3
    t = t_of(d)
    thud = band(rng.standard_normal(len(t)), 40, 320) * env(t, 0.004, 0.09)
    thud += 0.6 * np.sin(2 * math.pi * 70 * t) * env(t, 0.002, 0.08)
    slide = band(rng.standard_normal(len(t)), 600, 4000) * env(t, 0.12, 0.35) * (t > 0.06)
    write("ice_fall_%02d" % (k + 1), thud + 0.35 * slide, seed, {"seconds": d}, peak_db=-3.0)


def banya_ladle(k):
    seed = 2150 + k
    rng = np.random.default_rng(seed)
    d = 2.6
    t = t_of(d)
    burst = band(rng.standard_normal(len(t)), 1500, 12000) * env(t, 0.02, 0.35)
    # the sizzle: crackles thinning out as the stones dry
    rate = 0.02 * np.exp(-t / 0.8)
    crack = (rng.random(len(t)) < rate) * rng.standard_normal(len(t))
    sizzle = band(crack, 2500, 10000)
    splash = band(rng.standard_normal(len(t)), 300, 2500) * env(t, 0.003, 0.05)
    x = room(burst + 0.8 * sizzle + 0.4 * splash, 0.4, 0.2, seed)
    write("banya_ladle_%02d" % (k + 1), x, seed, {"seconds": d}, peak_db=-4.0)


def venik_swish(k):
    seed = 2160 + k
    rng = np.random.default_rng(seed)
    d = 0.75
    t = t_of(d)
    # leaves through the air: a whoosh with many tiny rustles, then the pat on skin
    sweep = band(rng.standard_normal(len(t)), 700, 6000) * np.sin(math.pi * np.clip(t / 0.4, 0, 1)) ** 2
    rustle = band((rng.random(len(t)) < 0.03) * rng.standard_normal(len(t)), 2000, 9000) * np.sin(math.pi * np.clip(t / 0.5, 0, 1))
    pat_t = 0.38 + 0.03 * k
    pat = band(rng.standard_normal(len(t)), 200, 3000) * env(np.maximum(0, t - pat_t), 0.002, 0.04) * (t >= pat_t)
    write("venik_swish_%02d" % (k + 1), sweep + 0.7 * rustle + 0.9 * pat, seed, {"seconds": d, "pat_s": pat_t}, peak_db=-5.0)


def banya_door(k):
    seed = 2170 + k
    rng = np.random.default_rng(seed)
    d = 1.6
    t = t_of(d)
    # a creak: a slow stick-slip, pulses whose rate wanders, through a woody resonance
    rate = 38 + 14 * np.sin(2 * math.pi * 0.7 * t + k)
    pulses = np.sin(2 * math.pi * np.cumsum(rate) / RATE) > 0.97
    creak = band(pulses.astype(float) * (0.5 + rng.random(len(t))), 300, 2400) * np.sin(math.pi * np.clip(t / 1.0, 0, 1))
    knock = band(rng.standard_normal(len(t)), 60, 500) * env(np.maximum(0, t - 1.15), 0.003, 0.08) * (t > 1.15)
    write("banya_door_%02d" % (k + 1), room(creak + 1.2 * knock, 0.3, 0.15, seed), seed, {"seconds": d}, peak_db=-5.0)


def crunches(rng, t, at, n, spread, lo=500, hi=6000):
    x = np.zeros(len(t))
    for i in range(n):
        c0 = at + i * spread + rng.uniform(-0.01, 0.01)
        e = env(np.maximum(0, t - c0), 0.004, 0.035) * (t >= c0)
        x += band(rng.standard_normal(len(t)), lo, hi) * e * rng.uniform(0.6, 1.0)
    return x


def snow_scoop(k):
    seed = 2180 + k
    rng = np.random.default_rng(seed)
    t = t_of(0.6)
    write("snow_scoop_%02d" % (k + 1), crunches(rng, t, 0.02, 5 + k, 0.08), seed, {"seconds": 0.6}, peak_db=-6.0)


def snowball_throw(k):
    seed = 2190 + k
    rng = np.random.default_rng(seed)
    d = 0.35
    t = t_of(d)
    x = band(rng.standard_normal(len(t)), 400 + 100 * k, 3500) * np.sin(math.pi * t / d) ** 2
    write("snowball_throw_%02d" % (k + 1), x, seed, {"seconds": d}, peak_db=-8.0)


def snowball_hit(k):
    seed = 2200 + k
    rng = np.random.default_rng(seed)
    d = 0.4
    t = t_of(d)
    puff = band(rng.standard_normal(len(t)), 150, 2500) * env(t, 0.002, 0.05)
    write("snowball_hit_%02d" % (k + 1), puff + 0.5 * crunches(rng, t, 0.01, 3, 0.03, 1500, 8000), seed, {"seconds": d}, peak_db=-4.0)


def snow_pat(k):
    seed = 2210 + k
    rng = np.random.default_rng(seed)
    d = 0.5
    t = t_of(d)
    x = np.zeros(len(t))
    for i in range(2):
        c0 = 0.02 + i * 0.16
        x += band(rng.standard_normal(len(t)), 120, 1800) * env(np.maximum(0, t - c0), 0.003, 0.05) * (t >= c0)
    write("snow_pat_%02d" % (k + 1), x + 0.3 * crunches(rng, t, 0.03, 2, 0.16), seed, {"seconds": d}, peak_db=-6.0)


ALL = [("skate_stroke", lambda: [skate_stroke(k) for k in range(3)]), ("skate_glide", skate_glide),
       ("skate_stop", lambda: [skate_stop(k) for k in range(2)]), ("ice_crack", lambda: [ice_crack(k) for k in range(3)]),
       ("ice_fall", lambda: [ice_fall(k) for k in range(2)]), ("banya_ladle", lambda: [banya_ladle(k) for k in range(3)]),
       ("venik_swish", lambda: [venik_swish(k) for k in range(3)]), ("banya_door", lambda: [banya_door(k) for k in range(2)]),
       ("snow_scoop", lambda: [snow_scoop(k) for k in range(3)]), ("snowball_throw", lambda: [snowball_throw(k) for k in range(3)]),
       ("snowball_hit", lambda: [snowball_hit(k) for k in range(3)]), ("snow_pat", lambda: [snow_pat(k) for k in range(3)])]


def main():
    words = sys.argv[1:]
    for name, fn in ALL:
        if not words or any(w in name for w in words):
            fn()


if __name__ == "__main__":
    main()
