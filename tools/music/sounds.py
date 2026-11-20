#!/usr/bin/env python3
"""The radio's and the record players' own sounds (docs/music.md). Procedural and seeded (numpy + scipy; no generation
service, nothing paid), 48 kHz 16-bit mono, each with a .json sidecar (generator, seed, parameters, measured levels).

  python3 tools/music/sounds.py            write them all
  python3 tools/music/sounds.py chime      only the ones whose name has one of the words

  audio/sfx/radio/radio_chime_01.wav    the call sign of «Волна Людмиловска»: five notes on a vibraphone-like bell
  audio/sfx/radio/radio_pips_01.wav     the time signal: five short pips and a long sixth
  audio/sfx/radio/radio_static_01.wav   a seamless loop of the air between stations (off air at night)
  audio/sfx/city/vinyl_crackle_0N.wav   the needle set down on a record: a soft thump, then crackle   (the acts' vinyl_crackle)
  audio/sfx/city/radio_tune_0N.wav      a radio dial turned: hiss, whistles, a station sliding past     (the acts' radio_tune)
  audio/sfx/city/jukebox_click_0N.wav   a coin in the jukebox, the arm's clunk, the motor              (the acts' jukebox_click)

Import on the Mac: IMPORT_ONLY=sfx/radio and IMPORT_ONLY=sfx/city with unreal/scripts/import_audio.py
(-> /Game/Audio/Radio/S_radio_*_01, /Game/Audio/City/S_<id>_0N; the soundscape's pools find them by name).
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


def write(group, name, x, seed, params, loop=False, peak_db=-3.0):
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * (10 ** (peak_db / 20.0))
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    out = os.path.join(ROOT, "audio", "sfx", group)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    rms = math.sqrt(float(np.mean((pcm / 32768.0) ** 2))) or 1e-9
    pool = name.rsplit("_", 1)[0]
    side = {"name": name, "asset": "/Game/Audio/%s/S_%s" % (group.capitalize(), name), "pool": pool,
            "file": os.path.relpath(path, ROOT), "generator": "tools/music/sounds.py", "seed": seed,
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


def bell(f, seconds, decay=1.6, bright=1.0):
    """A vibraphone-like bar: a few inharmonic partials, each decaying at its own pace, a soft mallet attack."""
    t = t_of(seconds)
    partials = [(1.0, 1.0, decay), (4.0, 0.35 * bright, decay * 0.35), (10.0, 0.12 * bright, decay * 0.12), (2.0, 0.08, decay * 0.7)]
    x = np.zeros_like(t)
    for ratio, amp, d in partials:
        x += amp * np.sin(2 * math.pi * f * ratio * t) * np.exp(-t / d)
    attack = np.minimum(1.0, t / 0.004)
    trem = 1.0 + 0.08 * np.sin(2 * math.pi * 5.5 * t)     # the vibraphone's slow vibrato
    return x * attack * trem


def place(dst, src, at):
    i = int(at * RATE)
    n = min(len(src), len(dst) - i)
    if n > 0:
        dst[i:i + n] += src[:n]


def reverb(x, seconds=1.2, mix=0.25, seed=1):
    rng = np.random.default_rng(seed)
    n = int(seconds * RATE)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (RATE * seconds / 5.0))
    ir[0] = 0
    wet = signal.fftconvolve(x, ir)[:len(x)]
    wet /= (np.max(np.abs(wet)) or 1.0)
    return (1 - mix) * x / (np.max(np.abs(x)) or 1.0) + mix * wet


def radio_band(x):
    """Through a small loudspeaker: 300 Hz .. 3.4 kHz and a little drive."""
    b, a = signal.butter(2, [300 / (RATE / 2), 3400 / (RATE / 2)], btype="band")
    y = signal.lfilter(b, a, x)
    return np.tanh(2.0 * y / (np.max(np.abs(y)) or 1.0))


def chime():
    seed = 1061
    # an original five-note call sign in D major: A4 F#5 D5 E5 A5, the last one held
    notes = [(0.00, 440.00), (0.32, 739.99), (0.64, 587.33), (0.96, 659.26), (1.40, 880.00)]
    x = np.zeros(int(4.2 * RATE))
    for i, (at, f) in enumerate(notes):
        place(x, bell(f, 2.8 if i == len(notes) - 1 else 1.6, decay=1.8 if i == len(notes) - 1 else 0.9), at)
        place(x, 0.4 * bell(f / 2, 1.2, decay=0.6, bright=0.4), at)
    x = reverb(x, 1.4, 0.3, seed)
    x = 0.7 * radio_band(x) + 0.3 * x
    write("radio", "radio_chime_01", x, seed, {"notes_hz": [n[1] for n in notes], "key": "D major", "through": "300-3400 Hz speaker"})


def pips():
    seed = 1062
    # the time signal: five 0.1 s pips of 1 kHz a second apart, the sixth 0.5 s, the hour at its start
    x = np.zeros(int(6.0 * RATE))
    for k in range(6):
        d = 0.5 if k == 5 else 0.1
        t = t_of(d)
        p = np.sin(2 * math.pi * 1000 * t) * np.minimum(1.0, np.minimum(t, d - t) / 0.004)
        place(x, p, k * 1.0)
    x = radio_band(x)
    write("radio", "radio_pips_01", x, seed, {"pips": 6, "hz": 1000, "spacing_s": 1.0, "last_s": 0.5}, peak_db=-6.0)


def static_loop():
    seed = 1063
    rng = np.random.default_rng(seed)
    n = int(8.0 * RATE)
    # circular filtering keeps the loop seamless: noise shaped in the frequency domain, crackles folded round the end
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    shape = np.exp(-((np.log(np.maximum(f, 1.0)) - math.log(1800)) ** 2) / 1.2)
    hiss = np.fft.irfft(spec * shape, n)
    hiss /= np.max(np.abs(hiss))
    # a slow fading of the air (whole cycles in the loop)
    t = np.arange(n) / RATE
    fade = 0.75 + 0.25 * np.sin(2 * math.pi * t / 8.0) * np.sin(2 * math.pi * 3 * t / 8.0)
    x = hiss * fade
    for _ in range(40):
        at = rng.integers(0, n)
        m = int(rng.uniform(0.0005, 0.004) * RATE)
        burst = rng.standard_normal(m) * np.exp(-np.arange(m) / (m / 4)) * rng.uniform(0.4, 1.4)
        idx = (at + np.arange(m)) % n
        x[idx] += burst
    write("radio", "radio_static_01", x, seed, {"seconds": 8, "band_hz": 1800, "crackles": 40}, loop=True, peak_db=-9.0)


def vinyl(take):
    seed = 2070 + take
    rng = np.random.default_rng(seed)
    secs = 3.0
    n = int(secs * RATE)
    x = np.zeros(n)
    # the needle down: a dull thump
    t = t_of(0.12)
    thump = np.sin(2 * math.pi * 70 * t) * np.exp(-t / 0.03)
    place(x, thump * 0.9, 0.05)
    # the lead-in groove: a soft rumble that turns 33 1/3 a minute, and the crackle
    tt = np.arange(n) / RATE
    rumble = np.sin(2 * math.pi * 0.555 * tt) * 0.0 + rng.standard_normal(n)
    b, a = signal.butter(2, 180 / (RATE / 2))
    rumble = signal.lfilter(b, a, rumble)
    rumble = rumble / (np.max(np.abs(rumble)) or 1) * 0.08
    x[int(0.1 * RATE):] += rumble[int(0.1 * RATE):]
    for _ in range(int(rng.integers(55, 80))):
        at = rng.uniform(0.15, secs - 0.05)
        m = int(rng.uniform(0.0003, 0.002) * RATE)
        pop = rng.standard_normal(m) * np.exp(-np.arange(m) / (m / 3)) * rng.uniform(0.15, 0.7)
        place(x, pop, at)
    # the once-a-turn tick of a scratch (1.8 s per turn)
    for k in range(2):
        m = int(0.003 * RATE)
        place(x, rng.standard_normal(m) * np.exp(-np.arange(m) / 40) * 0.6, 0.6 + 1.8 * k)
    b, a = signal.butter(2, [120 / (RATE / 2), 9000 / (RATE / 2)], btype="band")
    x = signal.lfilter(b, a, x) * np.minimum(1.0, (secs - tt) / 0.4)
    write("city", "vinyl_crackle_%02d" % take, x, seed, {"seconds": secs, "turn_s": 1.8}, peak_db=-6.0)


def tune(take):
    seed = 2080 + take
    rng = np.random.default_rng(seed)
    secs = 2.6
    n = int(secs * RATE)
    tt = np.arange(n) / RATE
    hiss = rng.standard_normal(n)
    b, a = signal.butter(2, [400 / (RATE / 2), 4000 / (RATE / 2)], btype="band")
    hiss = signal.lfilter(b, a, hiss)
    hiss /= np.max(np.abs(hiss))
    # heterodyne whistles sweeping past as the dial moves, then a station fades up and settles
    x = 0.5 * hiss
    for k in range(3):
        c = rng.uniform(0.3, 1.8)
        w = 0.25
        f = 1800 + 1600 * np.tanh((tt - c) / w) * (1 if k % 2 == 0 else -1)
        ph = 2 * math.pi * np.cumsum(np.abs(f)) / RATE
        x += 0.35 * np.sin(ph) * np.exp(-((tt - c) / 0.18) ** 2)
    settle = np.clip((tt - 1.7) / 0.5, 0, 1)
    tone = sum(np.sin(2 * math.pi * fr * tt) * am for fr, am in ((293.66, 0.5), (369.99, 0.35), (440.0, 0.3)))
    tone *= 0.5 + 0.5 * np.sin(2 * math.pi * 1.3 * tt) ** 2
    x = x * (1 - 0.8 * settle) + 0.6 * tone * settle
    x = radio_band(x) * np.minimum(1.0, (secs - tt) / 0.3)
    write("city", "radio_tune_%02d" % take, x, seed, {"seconds": secs, "whistles": 3, "settles_at_s": 1.7}, peak_db=-6.0)


def jukebox(take):
    seed = 2090 + take
    rng = np.random.default_rng(seed)
    secs = 2.4
    n = int(secs * RATE)
    x = np.zeros(n)
    # the coin: two bright clinks down the chute
    for at, f in ((0.02, 4200), (0.16, 3600)):
        place(x, 0.5 * bell(f, 0.15, decay=0.03, bright=1.5), at)
    # the selector arm: a heavy clunk
    t = t_of(0.2)
    clunk = (np.sin(2 * math.pi * 110 * t) + 0.5 * rng.standard_normal(len(t)) * np.exp(-t / 0.01)) * np.exp(-t / 0.05)
    place(x, clunk, 0.55 + 0.05 * take)
    # the turntable motor running up
    tt = t_of(1.4)
    f = 50 + 70 * np.minimum(1.0, tt / 0.8)
    motor = np.sin(2 * math.pi * np.cumsum(f) / RATE) * 0.25 * np.minimum(1.0, tt / 0.2) * np.minimum(1.0, (1.4 - tt) / 0.3)
    place(x, motor, 0.85)
    b, a = signal.butter(2, [60 / (RATE / 2), 10000 / (RATE / 2)], btype="band")
    x = signal.lfilter(b, a, x)
    write("city", "jukebox_click_%02d" % take, x, seed, {"seconds": secs, "coin_clinks": 2}, peak_db=-4.0)


JOBS = {"chime": chime, "pips": pips, "static": static_loop,
        "vinyl_1": lambda: vinyl(1), "vinyl_2": lambda: vinyl(2),
        "tune_1": lambda: tune(1), "tune_2": lambda: tune(2),
        "jukebox_1": lambda: jukebox(1), "jukebox_2": lambda: jukebox(2)}

if __name__ == "__main__":
    words = sys.argv[1:]
    for name, job in JOBS.items():
        if not words or any(w in name for w in words):
            job()
