"""The restaurant's sound set (docs/restaurant.md): table and kitchen foley, the room tone, a lounge-music loop. Procedural,
seeded, numpy + scipy only (no generation service); reuses tools/food_sounds.py's bandpass/env/place/noise helpers where they fit.

    nice -n 19 python3 tools/restaurant/sounds.py             writes audio/restaurant/rest_<kind>_<nn>.wav + .json
    nice -n 19 python3 tools/restaurant/sounds.py plate bell  only the sounds whose name has one of the words
    nice -n 19 python3 tools/restaurant/sounds.py --preview   also spectrogram PNGs into design/restaurant/sounds_preview/

48 kHz 16-bit mono. One-shots peak at -3 dBFS; loops are set by RMS (quiet beds, peak kept under -3 dBFS) and are seamless
by construction: event layers are rendered with a tail that is folded back onto the start, continuous layers are filtered
and convolved circularly, so the last sample runs into the first with no crossfade click. Each wav has a .json sidecar
(generator, seed, parameters, measured levels; "loop": true for loops, which unreal/scripts/import_restaurant_sounds.py reads).
"""
import json
import math
import os
import sys
import wave
import zlib

import numpy as np
from scipy import signal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from food_sounds import RATE, env, noise, place  # noqa: E402  (the repo's shared DSP helpers)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "audio", "sfx", "restaurant")   # the repo's convention: audio/sfx/<group>/<pool>_<nn>.wav -> /Game/Audio/<Group>/S_<pool>_<nn>
PREVIEW = os.path.join(ROOT, "design", "restaurant", "sounds_preview")
ONE_SHOT_PEAK_DB = -3.0
LOOP_RMS_DB = {"sizzle": -24.0, "kitchen": -26.0, "room": -26.0, "music": -25.0}
LOOP_PEAK_CEIL_DB = -3.0


# ---------------------------------------------------------------- filters (vectorised: scipy, not the per-sample loops)
def bp(x, f, q):
    f = min(f, RATE * 0.45)
    w = 2 * math.pi * f / RATE
    a = math.sin(w) / (2 * q)
    return signal.lfilter([a, 0, -a], [1 + a, -2 * math.cos(w), 1 - a], x)


def bp_var(x, fcurve, q, block=256):
    """A band-pass whose centre moves (fcurve per sample), block-wise with the filter state carried over."""
    y = np.zeros_like(x)
    zi = np.zeros(2)
    for i in range(0, len(x), block):
        f = min(float(fcurve[min(i, len(fcurve) - 1)]), RATE * 0.45)
        w = 2 * math.pi * f / RATE
        a = math.sin(w) / (2 * q)
        seg, zi = signal.lfilter([a, 0, -a], [1 + a, -2 * math.cos(w), 1 - a], x[i:i + block], zi=zi)
        y[i:i + block] = seg
    return y


def lp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f / (RATE / 2), "low", output="sos"), x)


def hp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f / (RATE / 2), "high", output="sos"), x)


def circ(fn, x):
    """Apply a causal filter circularly: run it over three copies and keep the middle one (no seam)."""
    n = len(x)
    return fn(np.tile(x, 3))[n:2 * n]


def fold(buf, n):
    """Fold the part of a rendered buffer past n back onto its start (events ringing over the loop point)."""
    out = buf[:n].copy()
    rest = buf[n:]
    for i in range(0, len(rest), n):
        c = rest[i:i + n]
        out[:len(c)] += c
    return out


def circ_reverb(x, rng, seconds, wet, damp_hz=5000.0):
    """A cheap room: decaying noise impulse response, convolved circularly (needs seconds << len(x))."""
    n = len(x)
    m = int(seconds * RATE)
    t = np.arange(m) / RATE
    ir = lp(rng.standard_normal(m), damp_hz) * np.exp(-t / (seconds / 6.9))
    ir[0] = 0
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-9
    wetsig = np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(ir, n), n)
    return x * (1 - wet) + wetsig * wet


PADN = int(2.3 * RATE)


def _pad(y):
    """Event atoms are zero-padded to one common length so that click + ring sums always line up."""
    return np.concatenate([y, np.zeros(PADN - len(y))]) if len(y) < PADN else y


def modal(parts, dur, rng=None):
    """Damped sinusoids [(freq Hz, amp, decay s)] - china, glass, steel, bells."""
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    y = np.zeros(n)
    for f, a, d in parts:
        ph = rng.uniform(0, 0.6) if rng is not None else 0.0
        y += a * np.sin(2 * math.pi * f * t + ph) * np.exp(-t / d)
    return _pad(y * np.minimum(1.0, t / 0.0004))


def click(rng, f, q, dur, amp):
    n = int(dur * RATE)
    return _pad(bp(noise(rng, dur), f, q) * env(n, 0.0004, dur / 4) * amp)


def thump(f, dur, amp, dec):
    t = np.arange(int(dur * RATE)) / RATE
    return _pad(np.sin(2 * math.pi * f * t * (1 + 0.5 * np.exp(-t / 0.01))) * np.exp(-t / dec) * amp)


# ---------------------------------------------------------------- one-shots
CHINA = [(1.0, 0.5, 0.090), (2.31, 0.35, 0.070), (3.86, 0.2, 0.050), (5.8, 0.1, 0.035)]
GLASS = [(1.0, 0.6, 0.55), (2.32, 0.35, 0.40), (4.25, 0.2, 0.25), (6.63, 0.1, 0.15)]


def plate(rng, k, P):
    """A china plate set down on a cloth: a soft low thud and cloth clack, a short ceramic ring over it."""
    f0 = P["ring_hz"] = float(rng.uniform(2300, 3500))
    buf = np.zeros(int(0.7 * RATE))
    place(buf, lp(noise(rng, 0.05), 380) * env(int(0.05 * RATE), 0.001, 0.014) * 1.4, 0.0)
    place(buf, thump(rng.uniform(120, 190), 0.1, 0.5, 0.025), 0.0)
    place(buf, click(rng, rng.uniform(2200, 3200), 1.5, 0.03, 0.45), 0.001)
    ring = modal([(f0 * r, a * 0.32, d * rng.uniform(0.8, 1.2)) for r, a, d in CHINA]
                 + [(f0 * 0.46, 0.22, 0.05)], 0.5, rng)
    place(buf, ring, 0.002)
    if k % 2 == 0:   # the settle: a second, softer touch of the rim
        place(buf, click(rng, 2600, 2.0, 0.02, 0.18) + modal([(f0 * 1.07, 0.1, 0.04)], 0.1, rng), 0.045)
    return buf


def clink(rng, k, P):
    """Cutlery on china: 1-2 taps of a fork, 3 a tap with ring, 4 a scrape."""
    f0 = P["ring_hz"] = float(rng.uniform(3100, 4700))
    buf = np.zeros(int(0.9 * RATE))
    parts = [(f0 * 1.0, 0.5, 0.16), (f0 * 2.7, 0.3, 0.10), (f0 * 4.6, 0.15, 0.06), (f0 * 0.37, 0.25, 0.07)]
    if k in (1, 3):
        place(buf, click(rng, 6200, 2.0, 0.006, 0.7) + modal(parts, 0.7, rng), 0.0)
        if k == 3:
            place(buf, modal([(f0 * 0.5, 0.18, 0.28)], 0.6, rng), 0.0)
    elif k == 2:
        place(buf, click(rng, 6000, 2.0, 0.006, 0.6) + modal(parts, 0.6, rng) * 0.9, 0.0)
        p2 = [(f * 1.09, a * 0.55, d) for f, a, d in parts]
        place(buf, click(rng, 6500, 2.0, 0.005, 0.4) + modal(p2, 0.5, rng), 0.16)
    else:
        n = int(0.38 * RATE)
        stick = np.abs(lp(noise(rng, 0.38), 70)) * 14                     # stick-slip of the tines
        stick = np.clip(stick, 0, 1)
        sweep = np.linspace(3200, 5200, n)
        s = bp_var(noise(rng, 0.38), sweep, 2.0) * stick * np.sin(np.linspace(0, math.pi, n)) ** 0.7
        place(buf, s * 0.8, 0.0)
        place(buf, modal([(f0 * 0.8, 0.12, 0.35), (f0 * 2.1, 0.06, 0.2)], 0.7, rng) * np.minimum(1, np.arange(PADN) / 9000), 0.0)
        place(buf, click(rng, 6000, 2.0, 0.006, 0.4) + modal(parts, 0.6, rng) * 0.5, 0.40)
    return buf


def glass(rng, k, P):
    """A glass set down (1, 2) or two glasses touching lightly (3)."""
    f0 = P["ring_hz"] = float(rng.uniform(1700, 2600))
    buf = np.zeros(int(1.4 * RATE))
    ring = lambda f, g: modal([(f * r, a * g, d * rng.uniform(0.85, 1.15)) for r, a, d in GLASS], 1.2, rng)
    if k < 3:
        place(buf, lp(noise(rng, 0.03), 700) * env(int(0.03 * RATE), 0.001, 0.008) * 0.8, 0.0)   # the base meeting the cloth
        place(buf, click(rng, 4200, 2.0, 0.008, 0.45) + ring(f0, 0.5), 0.0)
    else:
        place(buf, click(rng, 6000, 2.0, 0.004, 0.5) + ring(f0, 0.45), 0.0)
        place(buf, click(rng, 6500, 2.0, 0.004, 0.35) + ring(f0 * rng.uniform(1.04, 1.12), 0.38), 0.032)
    return buf


def pour(rng, k, P):
    """Water from a carafe into a tumbler: the pitch of the filling glass climbs, bubbles, a last dribble. ~2.5 s."""
    n = int(2.5 * RATE)
    t = np.arange(n) / RATE
    P.update(f_start=450, f_end=1700)
    fill = 450 + 1250 * (t / 2.1).clip(0, 1) ** 1.3                      # the air column above the water shortens
    body = bp_var(noise(rng, 2.5), fill, 3.5) + 0.5 * bp_var(noise(rng, 2.5), fill * 2.1, 4.0)
    stream = hp(noise(rng, 2.5), 1800) * 0.12 * (1 + 0.3 * lp(noise(rng, 2.5), 8) * 6)
    amp = np.minimum(1, t / 0.18) * np.where(t < 2.05, 1.0, np.exp(-(t - 2.05) / 0.07))
    y = (body * 0.55 + stream) * amp
    for _ in range(rng.poisson(95)):                                     # bubbles
        at = rng.uniform(0.1, 2.1)
        f = rng.uniform(500, 2800) * (0.6 + 0.5 * at / 2.1)
        d = rng.uniform(0.012, 0.035)
        m = int(d * 4 * RATE)
        tt = np.arange(m) / RATE
        b = np.sin(2 * math.pi * f * (1 + 4 * tt) * tt) * np.exp(-tt / d) * rng.uniform(0.05, 0.4)
        place(y, b, at)
    for at in (2.15, 2.27, 2.43):                                        # drips after the stream stops
        f = rng.uniform(900, 1500)
        tt = np.arange(int(0.06 * RATE)) / RATE
        place(y, np.sin(2 * math.pi * f * (1 + 6 * tt) * tt) * np.exp(-tt / 0.012) * 0.25, at)
    return y


def bell(rng, k, P):
    """The chrome service bell at the pass: a hard strike, bright inharmonic partials, ~1.5 s decay."""
    f0 = P["f0_hz"] = [2350.0, 2640.0][(k - 1) % 2] * float(rng.uniform(0.99, 1.01))
    parts = []
    for r, a, d in [(1.0, 0.55, 1.5), (2.41, 0.4, 1.0), (3.87, 0.25, 0.55), (5.93, 0.15, 0.3), (8.3, 0.06, 0.15)]:
        parts.append((f0 * r, a, d))
        parts.append((f0 * r * 1.0035, a * 0.6, d * 0.9))               # a twin mode: slow beating, like a real cast cup
    buf = np.zeros(int(2.2 * RATE))
    place(buf, click(rng, 7000, 1.5, 0.008, 0.7) + modal(parts, 2.1, rng), 0.0)
    place(buf, thump(f0 * 0.12, 0.06, 0.25, 0.02), 0.0)
    return buf


def door(rng, k, P):
    """A glass-and-bronze door: the latch, a bronze rattle, the swing; then the close, a soft thud and the latch."""
    buf = np.zeros(int(2.4 * RATE))
    glassr = lambda g: modal([(f, a * g, d) for f, a, d in [(210, 0.4, 0.2), (540, 0.3, 0.14), (1380, 0.15, 0.1), (2650, 0.08, 0.07)]], 0.5, rng)
    place(buf, click(rng, 3500, 2.0, 0.012, 0.55) + click(rng, 1400, 3.0, 0.02, 0.4), 0.0)       # latch
    place(buf, bp(noise(rng, 0.5), 2200, 6.0) * env(int(0.5 * RATE), 0.01, 0.18) * 0.12, 0.02)   # bronze handle
    place(buf, glassr(0.25), 0.03)
    sw = int(0.7 * RATE)
    place(buf, lp(noise(rng, 0.7), 500) * np.sin(np.linspace(0, math.pi, sw)) ** 2 * 0.1, 0.2)   # air and hinge
    c = 1.25
    place(buf, thump(70, 0.35, 0.95, 0.09), c)
    place(buf, lp(noise(rng, 0.12), 300) * env(int(0.12 * RATE), 0.002, 0.04) * 0.9, c)
    place(buf, glassr(0.7), c)
    place(buf, click(rng, 3000, 2.0, 0.015, 0.45), c + 0.03)
    place(buf, bp(noise(rng, 0.4), 2400, 6.0) * env(int(0.4 * RATE), 0.004, 0.1) * 0.12, c + 0.01)
    return buf


def menu(rng, k, P):
    """A leather folder opening: a soft creak-flap, then paper shifting inside."""
    buf = np.zeros(int(1.1 * RATE))
    n = int(0.28 * RATE)
    flap = lp(noise(rng, 0.28), 1300) * np.sin(np.linspace(0, math.pi, n)) ** 1.6
    place(buf, flap * 0.8, 0.0)
    place(buf, thump(110, 0.1, 0.35, 0.03), 0.17)                       # the cover meeting the table
    creak = bp_var(noise(rng, 0.25), np.linspace(900, 520, int(0.25 * RATE)), 9.0) * np.sin(np.linspace(0, math.pi, int(0.25 * RATE)))
    place(buf, creak * 0.25, 0.02)
    t = 0.2
    for _ in range(int(rng.integers(5, 9))):                            # paper
        d = rng.uniform(0.03, 0.09)
        place(buf, bp(noise(rng, d), rng.uniform(3500, 6500), 1.2) * env(int(d * RATE), 0.006, d / 2.5) * rng.uniform(0.1, 0.3), t)
        t += rng.uniform(0.03, 0.08)
    return buf


def tray(rng, k, P):
    """Plates and cups shifting on a steel tray as the waiter walks: soft irregular rattle."""
    dur = 2.2
    buf = np.zeros(int(dur * RATE))
    steps = np.cumsum(rng.uniform(0.42, 0.55, 6))                       # walking pace: a jolt per step
    f0 = P["ring_hz"] = float(rng.uniform(2600, 3600))
    for s in steps:
        if s > dur - 0.4:
            break
        place(buf, modal([(rng.uniform(280, 520), 0.25, 0.1), (rng.uniform(800, 1300), 0.1, 0.07)], 0.3, rng), s)   # the steel
        for _ in range(int(rng.integers(2, 5))):
            at = s + rng.exponential(0.035)
            g = rng.uniform(0.1, 0.5)
            f = f0 * rng.uniform(0.7, 1.5)
            place(buf, click(rng, f * 1.5, 2.0, 0.005, 0.45 * g) + modal([(f * r, a * g, d * 0.6) for r, a, d in CHINA[:3]], 0.25, rng) * 0.4, at)
    return buf


def coins(rng, k, P):
    """A few coins set down on a saucer."""
    buf = np.zeros(int(1.8 * RATE))
    t = 0.0
    for i in range(int(rng.integers(4, 7))):
        f = rng.uniform(3800, 6800)
        g = rng.uniform(0.5, 1.0) * (0.85 ** i)
        parts = [(f, 0.5, rng.uniform(0.12, 0.28)), (f * 2.76, 0.25, 0.1), (f * 5.4, 0.1, 0.05), (rng.uniform(2500, 3300), 0.2, 0.2)]
        place(buf, (click(rng, 7000, 2.0, 0.004, 0.5) + modal(parts, 0.5, rng)) * g, t)
        t += rng.uniform(0.09, 0.26)
    return buf


# ---------------------------------------------------------------- loops
def sizzle_loop(rng, k, P):
    """Meat in a hot pan, 8 s: a hiss that breathes, dense small crackles, now and then a spit of fat."""
    L = 8 * RATE
    P.update(seconds=8, crackles_per_s=110)
    hiss = circ(lambda x: hp(x, 3200, 2), rng.standard_normal(L))
    hiss = circ(lambda x: lp(x, 11000, 2), hiss)
    wob = 1 + 0.35 * circ(lambda x: lp(x, 2.5, 2), rng.standard_normal(L)) / 0.01
    wob = np.clip(wob, 0.4, 1.6) if np.std(wob) > 0 else wob
    body = circ(lambda x: bp(x, 1200, 0.7), rng.standard_normal(L)) * 0.25
    ev = np.zeros(L + 6000)
    for _ in range(int(rng.poisson(110 * 8))):
        at = rng.uniform(0, 8)
        d = rng.uniform(0.003, 0.016)
        place(ev, bp(noise(rng, d), rng.uniform(2200, 9500), rng.uniform(1.5, 4)) * env(int(d * RATE), 0.0003, d / 4) * rng.lognormal(-0.5, 0.7), at)
    for _ in range(int(rng.poisson(14))):                               # fat spits: lower, longer
        at = rng.uniform(0, 8)
        d = rng.uniform(0.02, 0.05)
        place(ev, bp(noise(rng, d), rng.uniform(900, 2200), 2.0) * env(int(d * RATE), 0.001, d / 3) * rng.uniform(1.5, 3.5), at)
    ev = fold(ev, L)
    return (hiss * 0.20 * wob + body * 0.3 + ev * 0.55) * 1.0


def kitchen_loop(rng, k, P):
    """A kitchen heard through a wall, 12 s: pans, a ladle tapping a pot, an extractor, faint sizzle; all muffled."""
    L = 12 * RATE
    P.update(seconds=12, lowpass_hz=1900)
    ev = np.zeros(L + 4 * RATE)
    t = rng.uniform(0.2, 0.6)
    while t < 12:
        f = rng.uniform(450, 1100)
        parts = [(f, 0.5, rng.uniform(0.18, 0.4)), (f * 2.76, 0.3, 0.14), (f * 5.4, 0.15, 0.07), (f * 8.9, 0.06, 0.04)]
        place(ev, (click(rng, 3000, 1.5, 0.01, 0.5) + modal(parts, 0.9, rng)) * rng.uniform(0.3, 1.0), t)
        if rng.random() < 0.3:                                          # a scrape of a spatula
            d = rng.uniform(0.15, 0.35)
            place(ev, bp(noise(rng, d), rng.uniform(1500, 3000), 1.5) * np.sin(np.linspace(0, math.pi, int(d * RATE))) * 0.25, t + 0.1)
        t += rng.uniform(0.6, 2.2)
    for _ in range(2):                                                  # ladle on a pot: tink-tink-tonk
        at = rng.uniform(0, 12)
        f = rng.uniform(620, 840)
        for j in range(int(rng.integers(2, 4))):
            parts = [(f * (1 + 0.05 * j), 0.6, 0.45), (f * 2.4, 0.25, 0.2), (f * 4.1, 0.1, 0.1)]
            place(ev, (click(rng, 2500, 2.0, 0.006, 0.5) + modal(parts, 1.0, rng)) * (0.8 - 0.15 * j), at + j * rng.uniform(0.13, 0.2))
    for _ in range(int(rng.integers(8, 16))):                           # chopping, far
        place(ev, thump(rng.uniform(140, 240), 0.06, 0.4, 0.012) + click(rng, 1500, 2.0, 0.01, 0.2), rng.uniform(0, 12))
    ev = fold(ev, L)
    rumble = circ(lambda x: lp(x, 260, 2), rng.standard_normal(L)) * 1.8 + 0.25 * np.sin(2 * math.pi * 100 * np.arange(L) / RATE)
    sz = circ(lambda x: hp(lp(x, 5200), 2500), rng.standard_normal(L)) * 0.07
    y = ev * 0.55 + rumble * 0.12 + sz
    y = circ(lambda x: lp(x, 1900, 3), y)
    return circ_reverb(y, rng, 0.35, 0.3, 2500)


VOWELS = [(730, 1090, 2440), (530, 1840, 2480), (270, 2290, 3010), (570, 840, 2410), (300, 870, 2240), (660, 1720, 2410)]


def voice(rng, L, f0):
    """One speaker out of earshot: syllable-length buzz+breath bursts through vowel formants, in phrases with pauses.
    The syllables are never a word: formants and pitch wander at random."""
    buf = np.zeros(L + 4 * RATE)
    t = rng.uniform(0, 20)
    end = t + 20
    while t < end:
        for _ in range(int(rng.integers(3, 9))):                       # a phrase
            d = rng.uniform(0.09, 0.22)
            n = int(d * RATE)
            f = f0 * rng.uniform(0.9, 1.15)
            phase = np.cumsum(np.full(n, f) * np.linspace(1.0, rng.uniform(0.9, 1.1), n)) / RATE
            src = (2 * (phase % 1) - 1) + 0.6 * rng.standard_normal(n)
            fm = VOWELS[rng.integers(len(VOWELS))]
            seg = sum(bp(src, ff, qq) * g for ff, qq, g in zip(fm, (6, 9, 11), (1.0, 0.6, 0.3)))
            seg = seg * np.sin(np.linspace(0, math.pi, n)) ** 1.5 * rng.uniform(0.4, 1.0)
            place(buf, seg, t % 20)
            t += d + rng.uniform(0.01, 0.06)
        t += rng.uniform(0.6, 3.5)                                      # pause / the other person's turn
    return fold(buf, L)


def room_loop(rng, k, P):
    """A restaurant room, 20 s: several people murmuring (no words), a faint cutlery tick now and then, air."""
    L = 20 * RATE
    n_voices = P["voices"] = 6
    mur = np.zeros(L)
    for v in range(n_voices):
        f0 = rng.uniform(95, 135) if v % 2 == 0 else rng.uniform(185, 235)
        vv = voice(rng, L, f0)
        vv /= np.std(vv) + 1e-9
        mur += vv * rng.uniform(0.5, 1.0)
    mur = circ(lambda x: lp(x, 3800, 2), mur)
    mur = circ_reverb(mur, rng, 0.5, 0.45, 3500)
    air = circ(lambda x: lp(x, 420, 2), rng.standard_normal(L)) * 1.2
    air += circ(lambda x: hp(lp(x, 2200), 600), rng.standard_normal(L)) * 0.25
    ev = np.zeros(L + 2 * RATE)
    for _ in range(int(rng.integers(8, 13))):
        f = rng.uniform(3000, 5200)
        g = rng.uniform(0.15, 0.5)
        place(ev, (click(rng, 6000, 2.0, 0.006, 0.5) + modal([(f, 0.5, 0.1), (f * 2.7, 0.25, 0.06)], 0.4, rng)) * g, rng.uniform(0, 20))
    for _ in range(int(rng.integers(2, 4))):                            # a far plate set down
        place(ev, thump(150, 0.08, 0.4, 0.02) + modal([(rng.uniform(2400, 3200), 0.2, 0.08)], 0.3, rng) * 0.5, rng.uniform(0, 20))
    ev = circ(lambda x: lp(x, 7000, 2), fold(ev, L))
    ev = circ_reverb(ev, rng, 0.5, 0.4, 5000)
    P.update(layers="murmur, air, faint cutlery, far plates")
    return mur * 0.42 / (np.std(mur) / 1.0) * 1.0 + air * 0.06 + ev * 0.05


# ---- the lounge music: bossa nova in C, 120 BPM, 32 bars (64 s), Rhodes-ish piano, vibes, bass, brushes and rim clicks
BPM = 120
BAR = 4 * 60.0 / BPM
PROG = [("Cmaj9", [64, 67, 71, 74], 36), ("Am9", [60, 64, 67, 71], 45), ("Dm9", [65, 69, 72, 76], 38), ("G13", [59, 65, 69, 76], 43),
        ("Em7", [62, 67, 71, 74], 40), ("A7b9", [61, 67, 70, 76], 45), ("Dm9", [65, 69, 72, 76], 38), ("G13", [59, 65, 69, 76], 43)]
CLAVE = [[0, 3, 6, 10, 13], [3, 6, 10, 13]]


def mtof(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def epiano(note, dur, vel):
    """A tine electric piano: soft FM, mellow bell on top, natural decay."""
    f = mtof(note)
    n = int((min(dur, 1.2) + 1.6) * RATE)
    t = np.arange(n) / RATE
    idx = (0.9 + 1.6 * vel) * np.exp(-t / 0.35)
    car = np.sin(2 * math.pi * f * t + idx * np.sin(2 * math.pi * f * t))
    tine = 0.12 * vel * np.sin(2 * math.pi * f * 7 * t + 0.4 * np.sin(2 * math.pi * f * 14 * t)) * np.exp(-t / 0.05)
    y = (car * np.exp(-t / 0.75) + tine) * vel
    rel = np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.25))
    return y * rel * np.minimum(1, t / 0.003)


def vibes(note, dur, vel):
    f = mtof(note)
    n = int(2.2 * RATE)
    t = np.arange(n) / RATE
    y = np.sin(2 * math.pi * f * t) * np.exp(-t / 1.1) + 0.3 * np.sin(2 * math.pi * f * 4 * t) * np.exp(-t / 0.35) \
        + 0.06 * np.sin(2 * math.pi * f * 10 * t) * np.exp(-t / 0.1)
    y *= 1 - 0.25 * (0.5 + 0.5 * np.sin(2 * math.pi * 4.6 * t - 1.0))        # motor tremolo
    return y * vel * np.minimum(1, t / 0.002)


def bass_note(note, dur, vel):
    f = mtof(note)
    n = int((dur + 0.3) * RATE)
    t = np.arange(n) / RATE
    y = (np.sin(2 * math.pi * f * t) + 0.35 * np.sin(2 * math.pi * 2 * f * t) + 0.1 * np.sin(2 * math.pi * 3 * f * t)) * np.exp(-t / 0.5)
    return y * vel * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.06)) * np.minimum(1, t / 0.006)


def music_loop(rng, k, P):
    bars = 32
    L = int(bars * BAR * RATE)
    P.update(key="C major / A minor", tempo_bpm=BPM, time_signature="4/4", bars=bars, seconds=bars * BAR,
             progression=[c[0] for c in PROG], form="8-bar cycle x4", feel="bossa nova, straight eighths",
             instruments="FM electric piano comp, vibraphone melody, plucked bass, brushes, rim clicks, foot chick")
    e8 = BAR / 8
    piano, vib, bassb, drums = [np.zeros(L + 5 * RATE) for _ in range(4)]
    mel = 76
    for bar in range(bars):
        name, voicing, root = PROG[bar % 8]
        b0 = bar * BAR
        for i, pos in enumerate([0, 3, 4, 7]):                          # the comping figure
            vel = [0.8, 0.55, 0.7, 0.5][i] * rng.uniform(0.9, 1.05)
            for j, nt in enumerate(voicing):
                place(piano, epiano(nt, 0.55 if pos in (0, 4) else 0.3, vel * (0.55 if j == 3 else 1.0)), b0 + pos * e8 + j * 0.006)
        for pos, nt, d in [(0, root, 0.45), (3, root + 7, 0.3), (4, root + (12 if root < 40 else 0), 0.45), (7, root + 7, 0.3)]:
            place(bassb, bass_note(nt, d, 0.9 if pos in (0, 4) else 0.65), b0 + pos * e8)
        if bar % 2 == 1 and (bar // 2) % 4 != 3 or bar % 8 == 6:         # a sparse vibraphone line, answered every other bar
            pool = sorted({n + o for n in voicing for o in (12, 24) if 72 <= n + o <= 88})
            for pos, d in rng.choice([[0, 3], [1, 4, 6], [2, 5], [0, 4]]) if False else [(0, 0.9), (3, 0.5), (5, 0.4)][: int(rng.integers(2, 4))]:
                mel = min(pool, key=lambda p: abs(p - mel) + rng.uniform(0, 3))
                place(vib, vibes(mel, d, rng.uniform(0.45, 0.7)), b0 + pos * e8 * (1 if pos < 4 else 1))
        for q in range(4):                                              # brush swish: a breath of noise per beat
            nq = int(0.5 * RATE)
            swell = (0.35 + 0.65 * np.sin(np.linspace(0, math.pi, nq)) ** 1.5) * (1.0 if q in (1, 3) else 0.75)
            place(drums, bp(noise(rng, 0.5), 6500, 0.6) * swell * 0.16, b0 + q * 0.5)
        for q in (1, 3):
            place(drums, bp(noise(rng, 0.04), 5200, 1.5) * env(int(0.04 * RATE), 0.0005, 0.01) * 0.28, b0 + q * 0.5)
        for s in CLAVE[bar % 2]:
            place(drums, click(rng, 1900, 4.0, 0.05, 0.5) + thump(650, 0.04, 0.2, 0.012), b0 + s * BAR / 16)
    wet = lambda x, w: circ_reverb(fold(x, L), rng, 1.3, w, 4500)
    mix = wet(piano, 0.22) * 0.30 + wet(vib, 0.3) * 0.22 + circ(lambda x: lp(x, 500, 2), fold(bassb, L)) * 0.55 + wet(drums, 0.12) * 0.55
    return circ(lambda x: lp(x, 9000, 2), mix)


SOUNDS = []
for _k, _n in (("plate", 4), ("clink", 4), ("glass", 3), ("pour", 1), ("bell", 2), ("door", 1), ("menu", 1), ("tray", 2), ("coins", 1)):
    SOUNDS += [(_k, i, globals()[_k], False) for i in range(1, _n + 1)]
SOUNDS += [("sizzle", 1, sizzle_loop, True), ("kitchen", 1, kitchen_loop, True),
           ("room", 1, room_loop, True), ("music", 1, music_loop, True)]


# ---------------------------------------------------------------- finishing, writing
def db(x):
    return 20 * math.log10(max(float(x), 1e-9))


def finish(x, kind, loop):
    x = np.asarray(x, dtype=float)
    if loop:
        n = len(x)
        x = circ(lambda y: hp(y, 25, 2), x)
        x -= np.mean(x)
        rms = math.sqrt(float(np.mean(x ** 2)))
        x *= 10 ** (LOOP_RMS_DB[kind] / 20) / rms
        pk = float(np.max(np.abs(x)))
        if db(pk) > LOOP_PEAK_CEIL_DB:
            x *= 10 ** (LOOP_PEAK_CEIL_DB / 20) / pk
        assert len(x) == n
        return x
    x = hp(x, 25, 2)
    x -= np.mean(x)
    x *= 10 ** (ONE_SHOT_PEAK_DB / 20) / max(float(np.max(np.abs(x))), 1e-9)
    nz = np.nonzero(np.abs(x) > 10 ** (-60 / 20))[0]                    # trim the silent tail, leave a short fade
    if len(nz):
        x = x[: min(len(x), nz[-1] + int(0.03 * RATE))]
    fo = min(len(x), int(0.02 * RATE))
    x[-fo:] *= np.linspace(1, 0, fo) ** 2
    fi = int(0.0005 * RATE)
    x[:fi] *= np.linspace(0, 1, fi)
    return x


def write_wav(path, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).round().astype("<i2").tobytes())


def seam_click_ratio(x):
    """Largest sample step across the loop point against the loudest step anywhere in the loop (<= ~1 means no click)."""
    d = np.abs(np.diff(np.concatenate([x[-3:], x[:3]])))
    return float(np.max(d) / (np.max(np.abs(np.diff(x))) + 1e-12))


def render(kind, idx, fn, loop):
    name = "rest_%s_%02d" % (kind, idx)          # the pool is rest_<kind>; the game asset is S_<name>
    seed = zlib.crc32(name.encode()) & 0x7FFFFFFF
    rng = np.random.default_rng(seed)
    P = {}
    x = finish(fn(rng, idx or 1, P), kind, loop)
    path = os.path.join(OUT, name + ".wav")
    write_wav(path, x)
    side = {"name": name, "asset": "/Game/Audio/Restaurant/S_" + name, "pool": "rest_" + kind, "file": "audio/sfx/restaurant/%s.wav" % name, "generator": "tools/restaurant/sounds.py:%s" % fn.__name__,
            "seed": seed, "rng": "numpy.random.default_rng (PCG64), numpy %s" % np.__version__, "take": idx,
            "loop": loop, "sample_rate": RATE, "bit_depth": 16, "channels": 1,
            "seconds": round(len(x) / RATE, 3), "peak_dbfs": round(db(np.max(np.abs(x))), 2),
            "rms_dbfs": round(db(math.sqrt(float(np.mean(x ** 2)))), 2), "dc": round(float(np.mean(x)), 7),
            "params": {k: (round(v, 3) if isinstance(v, float) else v) for k, v in P.items()}}
    if loop:
        side["seam_click_ratio"] = round(seam_click_ratio(x), 3)
    with open(os.path.join(OUT, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(side, f, indent=1, ensure_ascii=False)
        f.write("\n")
    return name, x, side


# ---------------------------------------------------------------- spectrogram previews (PNG, to look at)
def _cmap(v):
    stops = np.array([[0, 0, 4], [40, 11, 84], [101, 21, 110], [159, 42, 99], [212, 72, 66], [245, 125, 21], [250, 193, 39], [252, 255, 164]], float)
    v = np.clip(v, 0, 1) * (len(stops) - 1)
    i = np.minimum(v.astype(int), len(stops) - 2)
    fr = (v - i)[..., None]
    return (stops[i] * (1 - fr) + stops[i + 1] * fr).astype(np.uint8)


def preview(name, x, loop):
    from PIL import Image, ImageDraw
    sig = np.concatenate([x, x]) if loop else x            # a loop is shown twice: the seam is the vertical line in the middle
    f, t, S = signal.spectrogram(sig, RATE, nperseg=1024, noverlap=768, scaling="spectrum")
    S = 10 * np.log10(S + 1e-14)
    keep = f <= 16000
    S, f = S[keep], f[keep]
    img = _cmap((S + 100) / 70.0)[::-1]
    W, H = 1200, 300
    spec = Image.fromarray(img).resize((W, H), Image.BILINEAR)
    wave_h = 80
    canvas = Image.new("RGB", (W, H + wave_h), (12, 12, 16))
    canvas.paste(spec, (0, 0))
    d = ImageDraw.Draw(canvas)
    cols = np.array_split(sig, W)
    for cx, c in enumerate(cols):
        if len(c) == 0:
            continue
        hi, lo = float(c.max()), float(c.min())
        clip = max(abs(hi), abs(lo)) >= 0.985
        d.line([(cx, H + wave_h / 2 - hi * wave_h / 2), (cx, H + wave_h / 2 - lo * wave_h / 2)], fill=(255, 40, 40) if clip else (120, 200, 255))
    if loop:
        d.line([(W // 2, 0), (W // 2, H + wave_h)], fill=(80, 255, 80), width=1)
    d.text((6, 4), "%s  %.2fs  peak %.1f dBFS  (0-16 kHz)" % (name, len(x) / RATE, db(np.max(np.abs(x)))), fill=(255, 255, 255))
    os.makedirs(PREVIEW, exist_ok=True)
    canvas.save(os.path.join(PREVIEW, name + ".png"))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    want_preview = "--preview" in sys.argv
    os.makedirs(OUT, exist_ok=True)
    for kind, idx, fn, loop in SOUNDS:
        full = "%s_%02d" % (kind, idx)
        if args and not any(a in full for a in args):
            continue
        name, x, side = render(kind, idx, fn, loop)
        print("%-24s %6.2f s  peak %6.2f dBFS  rms %6.2f%s" % (name, side["seconds"], side["peak_dbfs"], side["rms_dbfs"],
                                                         ("  seam %.2f" % side["seam_click_ratio"]) if loop else ""))
        if want_preview:
            preview(name, x, loop)
    print("restaurant sounds ->", OUT)


if __name__ == "__main__":
    main()
