#!/usr/bin/env python3
"""The yacht school's sounds (docs/yacht_school.md "Sounds"). Procedural and seeded: numpy + scipy synthesis, nothing
generated or paid for, the same files on every run with the same numpy/scipy. 48 kHz 16-bit mono WAV, each with a .json
sidecar in the house format (generator, seed, parameters, measured levels, sha256, loop, trigger, radius, priority).

  python3 tools/sailing/sounds.py              write them all to audio/sfx/sailing/
  python3 tools/sailing/sounds.py halyard bell only the pools whose name has one of the words
  python3 tools/sailing/sounds.py --check      analyse every written file (levels, DC, clicks, loop seams); exit 1 on a fault
  python3 tools/sailing/sounds.py --table      print the markdown table for docs/yacht_school.md

How things are made (so they sound like the thing, not like beeps):
  - cloth (luffing, flogging, flags, the windsock) = a quasi-periodic train of cloth snaps (filtered noise bursts with a
    membrane "pop") over a fast flutter of rustle bursts, sparse dacron crinkle clicks and the low pressure thump of the
    whole sail; its rate follows a slow gust curve;
  - metal, wood, plastic (the mast, blocks, cleats, the bell, the boathouse door, the hulls) = modal resonators: sums of
    decaying partials at measured-like ratios (free-free tube bending 1 : 2.76 : 5.40 : 8.93, church/ship bell partials),
    the striker's hardness rolling the highs off, plus a contact noise burst;
  - water = Minnaert bubbles (f = 3.26 m*Hz / radius, rising chirp, damping after van den Doel) by the hundred, over
    filtered noise beds for wash, hiss and spray;
  - the outboard = a two-stroke twin's firing train (two pops a turn) through an exhaust resonance, an underwater
    exhaust burble, piston rattle and a gear whine, with the idle's misfires;
  - loops are rendered on a circular buffer: events wrap round, slow curves are periodic, filters are FFT (circular),
    every quasi-periodic train holds a whole number of cycles, so the seam is exact (--check measures it).

Import on the Mac: IMPORT_ONLY=sfx/sailing with unreal/scripts/import_audio.py (-> /Game/Audio/Sailing/S_<pool>_0N; the
soundscape's Sailing group finds them by name; loops get looping from the sidecar).
"""
import hashlib
import json
import math
import os
import sys
import wave
import zlib

import numpy as np
from scipy import signal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "audio", "sfx", "sailing")
GEN = "tools/sailing/sounds.py"
RATE = 48000
TAU = 2 * math.pi

# --------------------------------------------------------------------------------------------------------- primitives


def N(sec):
    return int(round(sec * RATE))


def T(sec):
    return np.arange(N(sec)) / RATE


def _sos(lo, hi, order):
    ny = RATE / 2
    if lo and hi:
        return signal.butter(order, [lo / ny, min(hi, ny * 0.95) / ny], "band", output="sos")
    if lo:
        return signal.butter(order, lo / ny, "high", output="sos")
    return signal.butter(order, min(hi, ny * 0.95) / ny, "low", output="sos")


def filt(x, lo=None, hi=None, order=2):
    """Causal Butterworth (one-shots: transients stay where they are)."""
    return signal.sosfilt(_sos(lo, hi, order), x)


def reson(x, f, q):
    """A causal resonant band-pass (two-pole peak) at f Hz, quality q."""
    b, a = signal.iirpeak(min(f, RATE * 0.45) / (RATE / 2), q)
    return signal.lfilter(b, a, x)


def cfilt(x, lo=None, hi=None, order=2, peaks=(), tilt=0.0):
    """Circular zero-phase filter by FFT (loops: no seam). peaks: (Hz, Q, gain) bumps; tilt: dB/octave above 1 kHz."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1 / RATE)
    fs = np.maximum(f, 1e-3)
    h = np.ones_like(f)
    if lo:
        h /= np.sqrt(1 + (lo / fs) ** (2 * order))
    if hi:
        h /= np.sqrt(1 + (fs / hi) ** (2 * order))
    for fc, q, g in peaks:
        h *= 1 + g * np.exp(-0.5 * ((f - fc) / (fc / q)) ** 2)
    if tilt:
        h *= 10 ** (tilt * np.log2(np.maximum(fs, 1000) / 1000) / 20)
    return np.fft.irfft(np.fft.rfft(x) * h, n)


def smooth(n, rng, fc):
    """A smooth random curve, periodic over n samples, std 1, with detail up to ~fc Hz."""
    f = np.fft.rfftfreq(n, 1 / RATE)
    X = (rng.standard_normal(len(f)) + 1j * rng.standard_normal(len(f))) * np.exp(-(f / max(fc, 1e-3)) ** 2)
    X[0] = 0
    y = np.fft.irfft(X, n)
    return y / (np.std(y) + 1e-12)


def env(t, attack, decay):
    t = np.maximum(t, 0)
    return (1 - np.exp(-t / max(attack, 1e-5))) * np.exp(-t / decay)


def hann_env(t, start, length):
    u = np.clip((t - start) / length, 0, 1)
    return np.sin(math.pi * u) ** 2


def place(dst, src, at, wrap=False, gain=1.0):
    """Adds src into dst at time `at` (s); wrap=True wraps round the end (loops)."""
    n, L = len(dst), len(src)
    i0 = int(round(at * RATE))
    if wrap:
        i0 %= n
        k = 0
        while k < L:
            m = min(L - k, n - i0)
            dst[i0:i0 + m] += gain * src[k:k + m]
            k += m
            i0 = 0
        return
    if i0 < 0:
        src, i0 = src[-i0:], 0
    m = min(len(src), n - i0)
    if m > 0:
        dst[i0:i0 + m] += gain * src[:m]


def train(n, rng, rate, jitter=0.15, fc=0.6, wrap=True, scatter=0.0):
    """Times (s) of a quasi-periodic train: rate*(1 + jitter*slow curve); in a loop a whole number of cycles."""
    r = rate * np.maximum(0.15, 1 + jitter * smooth(n, rng, fc))
    phi = np.cumsum(r) / RATE
    if wrap:
        phi *= max(1, round(phi[-1])) / phi[-1]
    idx = np.nonzero(np.diff(np.floor(phi)) > 0)[0] + 1
    t = idx / RATE
    if scatter:
        t = t + rng.normal(0, scatter / rate, len(t))
    return t


def poisson(n, rng, rate):
    """Times (s) of a Poisson process; rate is a number or a per-sample array (Hz)."""
    p = np.broadcast_to(np.asarray(rate, dtype=np.float64) / RATE, (n,))
    return np.nonzero(rng.random(n) < p)[0] / RATE


def modes(sec, parts, rng=None, hard=None, attack=0.0002):
    """Struck resonator: parts = [(Hz, tau s, gain)]; hard = striker corner Hz (soft strikers lose the highs)."""
    t = T(sec)
    y = np.zeros(len(t))
    for f, tau, g in parts:
        if f >= RATE * 0.45 or g == 0:
            continue
        if hard:
            g = g / (1 + (f / hard) ** 2)
        ph = 0.0 if rng is None else rng.uniform(-0.3, 0.3)
        y += g * np.exp(-t / tau) * np.sin(TAU * f * t + ph)
    y *= 1 - np.exp(-t / attack)
    m = max(1, len(t) // 8)
    y[-m:] *= 0.5 + 0.5 * np.cos(np.linspace(0, math.pi, m))
    return y


def burst(rng, sec, lo, hi, attack, decay, order=2):
    t = T(sec)
    return filt(rng.standard_normal(len(t)), lo, hi, order) * env(t, attack, decay)


def bubble(rng, r_mm, amp=1.0):
    """One Minnaert bubble: f0 = 3260/r(mm) Hz, rising slightly as it nears the surface, damped (van den Doel 2005)."""
    f0 = 3260.0 / r_mm
    d = 0.043 * f0 + 0.0014 * f0 ** 1.5
    tau = 1.0 / d
    L = max(8, N(min(5 * tau, 0.35)))
    t = np.arange(L) / RATE
    xi = rng.uniform(0.05, 0.2)
    ph = TAU * f0 * (t + 0.5 * xi * d * t * t)
    return amp * np.sin(ph) * np.exp(-t / tau)


def bubbles(dst, rng, times, rmin, rmax, amp=1.0, wrap=False, expo=1.0):
    """Many bubbles at the times; radius log-uniform in rmin..rmax mm, louder the bigger (r^expo)."""
    for t0 in times:
        r = math.exp(rng.uniform(math.log(rmin), math.log(rmax)))
        a = amp * (r / rmax) ** expo * rng.uniform(0.4, 1.0)
        place(dst, bubble(rng, r, a), t0, wrap)


def cloud(rng, sec, start, length, count, rmin, rmax, amp=1.0, shape=2.0):
    """A burst of bubbles: count of them spread over length after start, denser at first."""
    x = np.zeros(N(sec))
    times = start + length * rng.random(count) ** shape
    bubbles(x, rng, times, rmin, rmax, amp)
    return x


def space(x, rng, mix=0.15, t60=0.7, refl=(), lp=5000, wrap=False):
    """Outdoors by the water: the dry sound, a few discrete reflections (delay s, gain) and a soft diffuse tail."""
    L = N(t60)
    tail = rng.standard_normal(L) * np.exp(-6.9 * np.arange(L) / L)
    tail[:N(0.012)] = 0
    tail = filt(tail, 200, lp, 2)
    tail *= mix / (np.sqrt(np.sum(tail ** 2)) + 1e-12)
    ir = tail
    for d, g in refl:
        k = N(d)
        if k < L:
            ir[k] += g
    if wrap:
        n = len(x)
        wet = np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(np.pad(ir, (0, n - L)) if n > L else ir[:n], n), n)
        wet = cfilt(wet, None, lp * 1.5)
    else:
        wet = filt(signal.fftconvolve(x, ir)[:len(x)], None, lp * 1.5)
    return x + wet


def crinkle(n, rng, rate, lo=1800, hi=10000, wrap=False, heavy=1.5):
    """Dacron crinkle: sparse sharp clicks with heavy-tailed sizes."""
    x = np.zeros(n)
    times = poisson(n, rng, rate)
    idx = (times * RATE).astype(int) % n
    x[idx] = rng.pareto(heavy, len(idx)).clip(0, 20) * rng.choice([-1, 1], len(idx))
    return cfilt(x, lo, hi, 2) if wrap else filt(x, lo, hi, 2)


def noise(n, rng):
    return rng.standard_normal(n)


def mix(*parts):
    """Sums arrays of different lengths (zero-padded to the longest)."""
    out = np.zeros(max(len(p) for p in parts))
    for p in parts:
        out[:len(p)] += p
    return out

# ---------------------------------------------------------------------------------------------------- shared voices


def cloth_snap(rng, amp, bright, sec=0.09):
    """One cloth snap: a sharp broadband crack of the panel reversing + the membrane's low pop."""
    t = T(sec)
    x = filt(rng.standard_normal(len(t)), 450, 1800 + 6500 * bright, 2) * env(t, 0.0004, 0.004 + 0.012 * (1 - bright))
    f = rng.uniform(120, 300)
    x += (0.45 + 0.3 * (1 - bright)) * np.sin(TAU * f * t * (1 - 0.6 * t)) * env(t, 0.0015, 0.018)
    return amp * x


def ripple(rng, amp, lo, hi, decay, sec=0.12):
    """One flutter cycle along the luff: a rustle burst."""
    t = T(sec)
    return amp * filt(rng.standard_normal(len(t)), lo, hi, 2) * env(t, 0.003, decay)


def flutter_loop(sec, rng, s, small=False):
    """Luffing cloth, s = 0 (a soft flutter) .. 1 (a hard flog in a blow); small = a flag."""
    n = N(sec)
    x = np.zeros(n)
    gust = smooth(n, rng, 0.35)
    inten = np.clip(0.8 + 0.22 * gust, 0.35, 1.4)
    k = 1.6 if small else 1.0
    # the fast flutter along the luff / the flag's fly
    rate_f = (6 + 14 * s) * k
    hi = (2500 + 5000 * s) * (1.3 if small else 1.0)
    lo = 250 * k + 150 * s
    for t0 in train(n, rng, rate_f, 0.18, 0.8):
        a = (0.25 + 0.45 * s) * inten[int(t0 * RATE) % n] * rng.uniform(0.6, 1.0)
        place(x, ripple(rng, a, lo, hi, 1.0 / (rate_f * 3.0)), t0, True)
    # the whole sail (flag) flogging: cracks with the pressure thump, from a moderate wind up
    if s > 0.2:
        rate_b = (1.4 + 2.8 * s) * (2.2 if small else 1.0)
        for t0 in train(n, rng, rate_b, 0.25, 0.5, scatter=0.06):
            a = (s - 0.15) ** 1.3 * inten[int(t0 * RATE) % n] ** 1.5 * rng.uniform(0.55, 1.0) * 2.2
            place(x, cloth_snap(rng, a, min(1.0, 0.4 + 0.6 * s)), t0, True)
            if not small:
                tt = T(0.12)
                thump = np.sin(TAU * rng.uniform(55, 80) * tt) * env(tt, 0.006, 0.035)
                place(x, thump * a * 0.4, t0 - 0.004, True)
            if rng.random() < 0.5:  # the reversal's second, smaller snap
                place(x, cloth_snap(rng, a * rng.uniform(0.3, 0.6), 0.5), t0 + rng.uniform(0.03, 0.07), True)
    # crinkle of stiff dacron
    x += crinkle(n, rng, (40 + 260 * s) * inten, 2000, 10000, True) * (0.05 + 0.08 * s)
    # an airy bed of cloth against air
    x += cfilt(noise(n, rng), 300, 3000 + 3000 * s) * (0.03 + 0.05 * s) * inten
    # hard: the gear rattles (blocks and the boom shaking)
    if s > 0.7 and not small:
        for t0 in poisson(n, rng, 9.0 * (s - 0.5)):
            place(x, click(rng, rng.uniform(0.08, 0.25), 0.9), t0, True)
    return x


def click(rng, amp, f_scale=1.0, sec=0.06):
    """A ratchet pawl / small plastic-metal click."""
    j = rng.uniform(0.94, 1.06) * f_scale
    parts = [(2300 * j, 0.006, 1.0), (3650 * j, 0.004, 0.8), (5200 * j, 0.003, 0.6), (7400 * j, 0.002, 0.4),
             (850 * j, 0.012, 0.35), (1500 * j, 0.008, 0.3)]
    x = modes(sec, parts, rng)
    x += burst(rng, sec, 1500, 12000, 0.0001, 0.0005) * 0.6
    return amp * x


def mast_ring(rng, f0, hard, amp, sec=2.2, decay=1.0):
    """An aluminium mast struck: free-free tube bending partials + a shell cluster; hard = striker corner Hz."""
    ratios = [1.0, 2.756, 5.404, 8.933, 13.34]
    taus = [0.8, 0.5, 0.3, 0.18, 0.1]
    parts = [(f0 * r * rng.uniform(0.995, 1.005), tau * decay, g)
             for r, tau, g in zip(ratios, taus, [1.0, 0.8, 0.55, 0.35, 0.2])]
    # the tube's ovalling/shell modes: a bright little cluster
    for _ in range(4):
        parts.append((rng.uniform(1900, 5200), rng.uniform(0.08, 0.3) * decay, rng.uniform(0.3, 0.6)))
    # a slightly split doublet on the lowest partial (the section isn't perfectly round): a slow beat
    parts.append((f0 * 1.003 + rng.uniform(0.5, 1.5), 0.8 * decay, 0.5))
    x = modes(sec, parts, rng, hard)
    return amp * x


def halyard_slap(rng, f0, amp, shackle_p=0.5, sec=2.4):
    """A halyard swinging onto the mast: the rope's soft slap and, often, the shackle's tink, the tube ringing."""
    x = np.zeros(N(sec))
    place(x, burst(rng, 0.08, 180, 2600, 0.0008, 0.007) * 0.7 + mast_ring(rng, f0, 900, 1.0, 0.08), 0)
    place(x, mast_ring(rng, f0, 900, 0.7, sec), 0)
    if rng.random() < shackle_p:
        d = rng.uniform(0.012, 0.045)
        place(x, mix(mast_ring(rng, f0, 7000, rng.uniform(0.5, 0.9), sec - d), burst(rng, 0.02, 3000, 12000, 0.0001, 0.0008) * 0.5), d)
    return amp * x


def hull_knock(rng, amp, kind="plastic", sec=0.6):
    """A dinghy hull struck: plastic/GRP = hollow low modes; wood = plywood, higher and drier; alu = tinny boom."""
    j = rng.uniform(0.93, 1.07)
    if kind == "plastic":
        parts = [(112 * j, 0.07, 1.0), (178 * j, 0.05, 0.8), (265 * j, 0.04, 0.6), (410 * j, 0.025, 0.4), (690 * j, 0.012, 0.25)]
        hard, lo, hi = 1200, 150, 2500
    elif kind == "wood":
        parts = [(165 * j, 0.04, 0.9), (290 * j, 0.03, 1.0), (470 * j, 0.02, 0.7), (720 * j, 0.012, 0.5), (1150 * j, 0.007, 0.3)]
        hard, lo, hi = 2500, 200, 4000
    else:  # aluminium
        parts = [(96 * j, 0.35, 1.0), (158 * j, 0.3, 0.9), (241 * j, 0.25, 0.8), (333 * j, 0.22, 0.7), (468 * j, 0.18, 0.55),
                 (612 * j, 0.15, 0.45), (905 * j, 0.12, 0.35), (1310 * j, 0.09, 0.3), (2080 * j, 0.06, 0.2)]
        hard, lo, hi = 3000, 120, 5000
    x = modes(sec, parts, rng, hard)
    x += burst(rng, sec, lo, hi, 0.0005, 0.006) * 0.5
    return amp * x


def stick_slip(rng, sec, rate_lo, rate_hi, res, load_shape=None, grit=0.2):
    """Friction creak: a stick-slip pulse train whose rate wanders, through resonances res = [(Hz, Q, gain)]."""
    n = N(sec)
    t = np.arange(n) / RATE
    wander = np.clip(0.5 + 0.35 * smooth(n, rng, 3.0), 0, 1)
    rate = rate_lo + (rate_hi - rate_lo) * wander
    phi = np.cumsum(rate) / RATE
    idx = np.nonzero(np.diff(np.floor(phi)) > 0)[0] + 1
    p = np.zeros(n)
    p[idx] = rng.uniform(0.5, 1.0, len(idx))
    # creaks come in fits: the load catches and lets go
    p *= np.clip(0.55 + 0.6 * smooth(n, rng, 9.0), 0.0, 1.0) ** 1.5
    load = load_shape(t) if load_shape is not None else np.sin(math.pi * t / sec) ** 0.7
    p *= load
    y = np.zeros(n)
    for f, q, g in res:
        y += g * reson(p, f, q)
    y /= np.sqrt(np.mean(y ** 2)) + 1e-12
    y += grit * filt(noise(n, rng), 800, 6000) * load * 0.5
    return y


def water_plips(dst, rng, times, amp, wrap=False):
    """Drops falling into water: each a small splash tick and a bubble."""
    for t0 in times:
        place(dst, burst(rng, 0.01, 1500, 9000, 0.0002, 0.0012) * amp * 0.5, t0, wrap)
        r = rng.uniform(1.0, 3.5)
        place(dst, bubble(rng, r, amp * rng.uniform(0.4, 1.0)), t0 + 0.002, wrap)


def clap(rng, amp, fc, sec=0.05):
    """One pair of hands: a noise burst through the cupped hands' resonance."""
    t = T(sec)
    x = rng.standard_normal(len(t)) * env(t, 0.0003, rng.uniform(0.006, 0.012))
    y = reson(x, fc, 2.5) + 0.4 * filt(x, 400, 8000)
    return amp * y


def applause(sec, rng, people, tempo=(3.2, 5.5), rise=0.4, fall=None, dist=(4, 25), start=0.0):
    """A small crowd clapping on a pier: each person their own tempo, hands, distance; all start within `rise` s."""
    n = N(sec)
    x = np.zeros(n)
    t_end = fall if fall is not None else sec
    for _ in range(people):
        d = rng.uniform(*dist)
        g = 4.0 / d
        fc = rng.uniform(900, 2600)
        rate = rng.uniform(*tempo)
        t0 = start + rng.random() * rise
        stop = t_end - rng.uniform(0, 1.2) if fall is not None else sec
        tt = t0
        hand = np.zeros(n)
        while tt < stop:
            fade = 1.0 if fall is None else np.clip((stop - tt) / 0.8, 0, 1) ** 0.5
            place(hand, clap(rng, g * rng.uniform(0.6, 1.0) * fade, fc), tt)
            tt += (1.0 / rate) * rng.uniform(0.9, 1.1)
        x += filt(hand, None, 14000 / (1 + d / 30))
    return x

# ------------------------------------------------------------------------------------------------------- registry

REG = []  # dicts: pool, n, fn, loop, peak, sec, trigger, radius, prio, cat


def sound(pool, n=1, loop=False, peak=-3.0, trigger="", radius=30, prio="normal", cat=""):
    def deco(fn):
        REG.append(dict(pool=pool, n=n, fn=fn, loop=loop, peak=peak, trigger=trigger, radius=radius, prio=prio, cat=cat))
        return fn
    return deco

# ------------------------------------------------------------------------------------------------------------- sail


@sound("sail_luff_light", 2, loop=True, peak=-9.0, cat="Sail", radius=25, prio="normal",
       trigger="loop: sail luffing a little (luff 0.15-0.5) or light air; gain/pitch from LyudSail::Sound::MixLoops")
def sail_luff_light(k, rng):
    sec = 8.0
    return flutter_loop(sec, rng, 0.12 + 0.08 * k), {"seconds": sec, "strength": 0.12 + 0.08 * k}


@sound("sail_luff_mid", 2, loop=True, peak=-6.0, cat="Sail", radius=40,
       trigger="loop: sail luffing in a moderate wind (head to wind, eased right out)")
def sail_luff_mid(k, rng):
    sec = 8.0
    return flutter_loop(sec, rng, 0.5 + 0.08 * k), {"seconds": sec, "strength": 0.5 + 0.08 * k}


@sound("sail_luff_hard", 2, loop=True, peak=-3.0, cat="Sail", radius=70, prio="high",
       trigger="loop: hard flogging in a blow (luff > 0.6 and wind > 7 m/s, or a capsized/head-to-wind boat in a gale)")
def sail_luff_hard(k, rng):
    sec = 8.0
    return flutter_loop(sec, rng, 0.88 + 0.1 * k), {"seconds": sec, "strength": 0.88 + 0.1 * k}


@sound("sail_fill", 4, peak=-3.0, cat="Sail", radius=35,
       trigger="one-shot: the sail fills (luff drops from > 0.5 to < 0.2 within ~0.5 s): a whump")
def sail_fill(k, rng):
    sec = 1.3
    x = np.zeros(N(sec))
    t = T(sec)
    n_snaps = 2 + k % 2
    t0 = 0.04
    for i in range(n_snaps):
        place(x, cloth_snap(rng, 0.5 + 0.9 * (i + 1) / n_snaps, 0.6 + 0.1 * k), t0)
        t0 += rng.uniform(0.025, 0.06)
    # the pressure whump: a low swell as the cloth snaps taut
    f = 95 * np.exp(-np.maximum(t - t0, 0) / 0.09) + 42
    whump = np.sin(TAU * np.cumsum(f) / RATE) * env(t - t0 + 0.01, 0.01, 0.075) * (t > t0 - 0.01)
    x += 0.8 * whump
    x += burst(rng, sec, 300, 3500, 0.01, 0.22) * 0.18 * (t > t0)
    place(x, stick_slip(rng, 0.18, 140, 260, [(950, 6, 1.0), (1700, 7, 0.6)]) * 0.25, t0 + 0.04)
    place(x, mast_ring(rng, rng.uniform(170, 220), 600, 0.12, 0.8, 0.5), t0)
    return space(x, rng, 0.08, 0.5), {"seconds": sec, "snaps": n_snaps}


@sound("sail_gybe_slam", 4, peak=-1.5, cat="Sail", radius=60, prio="high",
       trigger="one-shot: a gybe, the boom slams across (boom angular speed past ~2 rad/s through the centreline)")
def sail_gybe_slam(k, rng):
    sec = 2.0
    x = np.zeros(N(sec))
    t = T(sec)
    ts = 0.48 + 0.04 * k
    # the sail swinging over: a rising rustle and a couple of flutters
    x += filt(noise(len(t), rng), 300, 4000) * hann_env(t, 0.0, ts * 2) * (t < ts) * 0.25
    for tt in np.sort(rng.uniform(0.15, ts - 0.05, 4)):
        place(x, ripple(rng, 0.3, 400, 4000, 0.02), tt)
    # the slam: the cloth cracks, the mainsheet thwacks taut, the boom rings, the hull takes it
    place(x, cloth_snap(rng, 1.8, 0.95, 0.12), ts)
    tt = T(0.3)
    twang = np.sin(TAU * np.cumsum(150 * (1 - 0.25 * tt / 0.3)) / RATE) * env(tt, 0.001, 0.07)
    place(x, (burst(rng, 0.3, 200, 2200, 0.0005, 0.016) + 0.5 * twang) * 1.2, ts + 0.006)
    boom = [(310, 0.28, 1.0), (852, 0.16, 0.7), (1668, 0.09, 0.5), (2760, 0.05, 0.3), (330, 0.25, 0.4)]
    place(x, modes(1.2, [(f * rng.uniform(0.97, 1.03), tau, g) for f, tau, g in boom], rng, 2500) * 0.7, ts + 0.01)
    place(x, hull_knock(rng, 1.1, "plastic"), ts + 0.015)
    for i in range(int(rng.integers(3, 6))):
        place(x, click(rng, rng.uniform(0.2, 0.45), 0.9), ts + 0.02 + rng.uniform(0, 0.09))
    # settling: the sail's rustle, the sheet's creak
    x += filt(noise(len(t), rng), 300, 3000) * env(t - ts - 0.05, 0.03, 0.25) * (t > ts + 0.05) * 0.12
    place(x, stick_slip(rng, 0.35, 90, 200, [(800, 5, 1.0), (1500, 6, 0.6)]) * 0.18, ts + 0.15)
    return space(x, rng, 0.1, 0.6, ((0.035, 0.1),)), {"seconds": sec, "slam_s": ts}


@sound("sail_leech", 1, loop=True, peak=-12.0, cat="Sail", radius=15,
       trigger="loop: leech flutter, a fast buzz of the sail's trailing edge, at speed (gain ~ speed, wind)")
def sail_leech(k, rng):
    sec = 4.0
    n = N(sec)
    x = np.zeros(n)
    sw = np.clip(0.75 + 0.25 * smooth(n, rng, 0.8), 0.2, 1.3)
    for t0 in train(n, rng, 27.0, 0.08, 1.5):
        a = sw[int(t0 * RATE) % n] * rng.uniform(0.7, 1.0)
        place(x, ripple(rng, a, 700, 5000, 0.007, 0.04), t0, True)
    x += cfilt(noise(n, rng), 500, 4000) * 0.08 * sw
    return x, {"seconds": sec, "rate_hz": 27}


@sound("sail_batten_pop", 4, peak=-4.0, cat="Sail", radius=25,
       trigger="one-shot: tacking, the battens pop through to the new side (after the luff peak of a tack)")
def sail_batten_pop(k, rng):
    sec = 0.6
    x = np.zeros(N(sec))
    j = rng.uniform(0.9, 1.1)
    pok = modes(0.4, [(620 * j, 0.04, 1.0), (1450 * j, 0.02, 0.8), (2900 * j, 0.01, 0.6), (4100 * j, 0.006, 0.4)], rng)
    place(x, mix(pok, cloth_snap(rng, 0.8, 0.7)), 0.01)
    if k % 2 == 0:  # a second batten a moment later
        place(x, cloth_snap(rng, 0.4, 0.6, 0.06), 0.09)
        place(x, modes(0.3, [(700 * j, 0.035, 1.0), (1620 * j, 0.018, 0.7), (3200 * j, 0.009, 0.5)], rng) * 0.55, 0.085)
    x += crinkle(len(x), rng, 300, wrap=False) * 0.08 * env(T(sec) - 0.02, 0.01, 0.12)
    return x, {"seconds": sec, "battens": 2 - k % 2}

# ---------------------------------------------------------------------------------------------------------- rigging

MAST_F0 = [372, 431, 488, 547, 612, 694]


def halyard_loop(sec, rng, f0, fresh):
    n = N(sec)
    x = np.zeros(n)
    # clusters: the halyard swings with the boat's roll and hits the mast once to a few times
    rate = 1.6 if fresh else 0.42
    for c0 in poisson(n, rng, rate):
        hits = int(rng.integers(2, 5) if fresh else rng.integers(1, 4))
        tt, a = c0, rng.uniform(0.6, 1.0)
        for _ in range(hits):
            place(x, halyard_slap(rng, f0, a, 0.65 if fresh else 0.45), tt, True)
            tt += rng.uniform(0.09, 0.3)
            a *= rng.uniform(0.45, 0.8)
    if fresh:  # the rope's constant soft rattle along the tube
        for t0 in poisson(n, rng, 5.0):
            place(x, burst(rng, 0.05, 250, 2500, 0.001, 0.006) * rng.uniform(0.05, 0.15), t0, True)
    return x


@sound("sail_halyard_light", 6, loop=True, peak=-6.0, cat="Rigging", radius=35,
       trigger="loop per parked boat in light wind (2-5 m/s): a halyard tinking on its mast; 6 masts = 6 pitches")
def sail_halyard_light(k, rng):
    sec = 16.0
    return halyard_loop(sec, rng, MAST_F0[k], False), {"seconds": sec, "mast_f0_hz": MAST_F0[k]}


@sound("sail_halyard_fresh", 6, loop=True, peak=-4.0, cat="Rigging", radius=45,
       trigger="loop per parked boat in a fresh wind (> 5 m/s): halyards slapping fast; same 6 mast pitches")
def sail_halyard_fresh(k, rng):
    sec = 10.0
    return halyard_loop(sec, rng, MAST_F0[k], True), {"seconds": sec, "mast_f0_hz": MAST_F0[k]}


@sound("sail_halyard_slap", 6, peak=-4.0, cat="Rigging", radius=35,
       trigger="one-shot: one slap on a mast, for rate-driven play (LyudSail::Sound::HalyardRate(wind)); variant = mast pitch")
def sail_halyard_slap(k, rng):
    sec = 2.4
    return halyard_slap(rng, MAST_F0[k], 1.0, 0.6, sec), {"seconds": sec, "mast_f0_hz": MAST_F0[k]}


def chorus(sec, rng, fresh):
    n = N(sec)
    x = np.zeros(n)
    for b in range(12):
        f0 = rng.uniform(350, 720)
        d = rng.uniform(5, 45)
        one = halyard_loop(sec, rng, f0, fresh)
        x += cfilt(one, None, 16000 / (1 + d / 25)) * (5.0 / d)
    return space(x, rng, 0.25, 0.9, ((0.06, 0.15), (0.21, 0.08)), 4000, True)


@sound("sail_halyard_chorus_light", 1, loop=True, peak=-6.0, cat="Rigging", radius=80, prio="low",
       trigger="loop at the boat park in light wind: a dozen boats' halyards (one emitter for the park)")
def sail_halyard_chorus_light(k, rng):
    sec = 20.0
    return chorus(sec, rng, False), {"seconds": sec, "boats": 12}


@sound("sail_halyard_chorus_fresh", 1, loop=True, peak=-4.0, cat="Rigging", radius=100, prio="low",
       trigger="loop at the boat park in a fresh wind: a dozen boats' halyards slapping")
def sail_halyard_chorus_fresh(k, rng):
    sec = 20.0
    return chorus(sec, rng, True), {"seconds": sec, "boats": 12}


@sound("sail_block_click", 5, peak=-6.0, cat="Rigging", radius=12,
       trigger="one-shot: a pull on the mainsheet through the ratchet block (sheet shortened by ~0.2 m)")
def sail_block_click(k, rng):
    sec = 0.45
    x = np.zeros(N(sec))
    rate = rng.uniform(18, 32)
    tt = 0.005
    for i in range(3 + k):
        place(x, click(rng, rng.uniform(0.6, 1.0)), tt)
        tt += 1 / rate * rng.uniform(0.85, 1.15)
    t = T(sec)
    x += filt(noise(len(t), rng), 1500, 7000) * hann_env(t, 0, tt + 0.02) * 0.06
    return x, {"seconds": sec, "clicks": 3 + k}


@sound("sail_block_run", 3, peak=-5.0, cat="Rigging", radius=15,
       trigger="one-shot: sheeting in hard hand over hand (a fast ratchet run)")
def sail_block_run(k, rng):
    sec = 1.2 + 0.2 * k
    x = np.zeros(N(sec))
    t = T(sec)
    pulls = 2 + k
    seg = (sec - 0.1) / pulls
    hiss = np.zeros(len(t))
    for p in range(pulls):
        p0 = 0.02 + p * seg
        L = seg * rng.uniform(0.6, 0.8)
        tt = p0
        while tt < p0 + L:
            u = (tt - p0) / L
            rate = 18 + 30 * math.sin(math.pi * u)
            place(x, click(rng, rng.uniform(0.6, 1.0) * (0.5 + 0.5 * math.sin(math.pi * u))), tt)
            tt += 1 / rate
        hiss += hann_env(t, p0, L)
    x += filt(noise(len(t), rng), 1500, 7000) * hiss * 0.07
    return x, {"seconds": sec, "pulls": pulls}


@sound("sail_sheet_run", 3, peak=-6.0, cat="Rigging", radius=15,
       trigger="one-shot: the sheet runs out through the block (eased fast or let fly)")
def sail_sheet_run(k, rng):
    sec = 1.5
    n = N(sec)
    t = np.arange(n) / RATE
    stop = 0.9 + 0.12 * k
    v = 3.2 * np.exp(-t / (0.45 + 0.1 * k)) * (t < stop)
    v = np.convolve(v, np.ones(200) / 200, "same")
    braid = TAU * np.cumsum(v / 0.006) / RATE  # a braid's texture buzzing over the sheave
    x = filt(noise(n, rng), 1200, 8000) * (v / 3.2) * (1 + 0.7 * np.sin(braid)) * 0.5
    x += filt(noise(n, rng), 200, 1200) * (v / 3.2) * 0.25
    x += np.sin(braid) * (v / 3.2) * 0.04
    x *= np.minimum(1, t / 0.02)
    # the stopper knot fetches up at the block
    place(x, mix(modes(0.25, [(720, 0.02, 1.0), (1320, 0.012, 0.6), (185, 0.04, 0.6)], rng),
                 burst(rng, 0.05, 300, 4000, 0.0003, 0.004) * 0.4) * 0.8, stop)
    return x, {"seconds": sec, "stop_s": stop}


@sound("sail_cleat_snap", 4, peak=-5.0, cat="Rigging", radius=12,
       trigger="one-shot: a sheet or the vang snapped into a cam cleat (or flicked out)")
def sail_cleat_snap(k, rng):
    sec = 0.4
    x = np.zeros(N(sec))
    zl = rng.uniform(0.05, 0.09)
    t = T(zl)
    place(x, filt(noise(len(t), rng), 1500, 8000) * (t / zl) ** 2 * 0.25, 0)
    j = rng.uniform(0.92, 1.08)
    jaw = [(1900 * j, 0.015, 1.0), (3300 * j, 0.01, 0.8), (4700 * j, 0.007, 0.6), (6100 * j, 0.004, 0.4)]
    place(x, mix(modes(0.25, jaw, rng), burst(rng, 0.02, 2000, 12000, 0.0001, 0.0006) * 0.5), zl)
    place(x, modes(0.25, jaw, rng) * rng.uniform(0.5, 0.8), zl + rng.uniform(0.006, 0.012))
    place(x, burst(rng, 0.06, 150, 1200, 0.001, 0.01) * 0.3, zl)
    return x, {"seconds": sec}


@sound("sail_rope_creak", 4, peak=-6.0, cat="Rigging", radius=10,
       trigger="one-shot: a loaded sheet creaking on the block/fairlead (sheet load rising past ~0.7, ~1 per 3 s)")
def sail_rope_creak(k, rng):
    sec = 0.7 + 0.2 * k
    x = stick_slip(rng, sec, 70 + 25 * k, 190 + 30 * k, [(900, 6, 1.0), (1650, 7, 0.7), (2600, 8, 0.4)])
    return x, {"seconds": sec}


@sound("sail_mast_creak", 4, peak=-6.0, cat="Rigging", radius=15,
       trigger="one-shot: the mast groaning in its step under load (heel or gust peak, ~1 per 5 s)")
def sail_mast_creak(k, rng):
    sec = 1.2 + 0.25 * k
    x = stick_slip(rng, sec, 18 + 6 * k, 50 + 8 * k, [(240, 5, 1.0), (520, 6, 0.7), (1100, 7, 0.35), (175, 4, 0.5)], grit=0.1)
    return x, {"seconds": sec}


@sound("sail_rudder_knock", 4, peak=-6.0, cat="Rigging", radius=12,
       trigger="one-shot: the rudder's pintles knocking in the gudgeons (a wave under the stern, a sharp helm)")
def sail_rudder_knock(k, rng):
    sec = 0.5
    x = np.zeros(N(sec))
    j = rng.uniform(0.92, 1.08)
    metal = [(1250 * j, 0.025, 1.0), (2300 * j, 0.015, 0.8), (3700 * j, 0.009, 0.5), (5200 * j, 0.005, 0.3)]
    place(x, modes(0.45, metal, rng, 5000) + hull_knock(rng, 0.12, "plastic", 0.45), 0.003)
    if k >= 2:
        place(x, modes(0.4, metal, rng, 3000) * 0.4 + hull_knock(rng, 0.06, "plastic", 0.4), rng.uniform(0.035, 0.06))
    return x, {"seconds": sec}


@sound("sail_dagger_down", 3, peak=-4.0, cat="Rigging", radius=15,
       trigger="one-shot: the daggerboard slid down in its case until it seats")
def sail_dagger_down(k, rng):
    sec = 1.0
    x = np.zeros(N(sec))
    sl = 0.35 + 0.07 * k
    t = T(sl)
    slide = filt(noise(len(t), rng), 300, 2600) * (0.4 + 0.6 * t / sl) * (1 + 0.5 * np.sin(TAU * 38 * t))
    place(x, slide * 0.3, 0.0)
    j = rng.uniform(0.92, 1.08)
    case = [(118 * j, 0.07, 1.0), (190 * j, 0.05, 0.8), (300 * j, 0.03, 0.6), (520 * j, 0.02, 0.4), (900 * j, 0.01, 0.35),
            (1700 * j, 0.006, 0.2)]
    place(x, mix(modes(0.6, case, rng, 1800), burst(rng, 0.05, 200, 3000, 0.0004, 0.004) * 0.4), sl)
    place(x, cloud(rng, 0.6, 0.0, 0.25, 12, 3, 9, 0.25), sl + 0.01)
    return x, {"seconds": sec}


@sound("sail_dagger_up", 3, peak=-5.0, cat="Rigging", radius=15,
       trigger="one-shot: the daggerboard pulled up (shallow water, coming ashore)")
def sail_dagger_up(k, rng):
    sec = 1.2
    x = np.zeros(N(sec))
    place(x, cloud(rng, 0.4, 0.0, 0.2, 18, 2.5, 9, 0.5), 0.0)   # the case gulps water as the board leaves it
    sl = 0.4
    t = T(sl)
    place(x, filt(noise(len(t), rng), 400, 3000) * np.sin(math.pi * t / sl) * 0.25, 0.05)
    j = rng.uniform(0.92, 1.08)
    place(x, modes(0.4, [(160 * j, 0.05, 0.9), (420 * j, 0.025, 1.0), (980 * j, 0.012, 0.5)], rng, 2000) * 0.7, 0.05 + sl)
    water_plips(x, rng, 0.5 + rng.random(9) * 0.65, 0.25)
    return x, {"seconds": sec}

# --------------------------------------------------------------------------------------------------- hull and water


@sound("sail_hull_ghost", 1, loop=True, peak=-12.0, cat="Hull & water", radius=12,
       trigger="loop: ghosting, speed 0.2-1.2 m/s: water trickling and lapping at the bow")
def sail_hull_ghost(k, rng):
    sec = 8.0
    n = N(sec)
    x = np.zeros(n)
    for t0 in train(n, rng, 0.75, 0.2, 0.3, scatter=0.1):
        place(x, burst(rng, 0.6, 150, 900, 0.06, 0.18) * 0.35, t0, True)
        bubbles(x, rng, t0 + 0.05 + 0.25 * rng.random(int(rng.integers(5, 15))), 1.2, 5, 0.35, True)
    bubbles(x, rng, poisson(n, rng, 18), 0.7, 2.5, 0.25, True)
    x += cfilt(noise(n, rng), 2000, 8000) * 0.008
    return x, {"seconds": sec}


@sound("sail_hull_chuckle", 1, loop=True, peak=-8.0, cat="Hull & water", radius=20,
       trigger="loop: sailing at displacement speed 1-3 m/s: the bow wave chuckling")
def sail_hull_chuckle(k, rng):
    sec = 8.0
    n = N(sec)
    x = np.zeros(n)
    enc = train(n, rng, 1.5, 0.2, 0.4, scatter=0.08)
    for t0 in enc:
        a = rng.uniform(0.5, 1.0)
        place(x, hull_knock(rng, 0.25 * a, "plastic", 0.4), t0, True)
        place(x, burst(rng, 0.6, 250, 2500, 0.03, 0.14) * 0.3 * a, t0, True)
        bubbles(x, rng, t0 + 0.18 * rng.random(int(rng.integers(25, 50))) ** 1.5, 1.0, 6, 0.4 * a, True)
    sw = np.clip(0.8 + 0.2 * smooth(n, rng, 1.2), 0.3, 1.2)
    x += cfilt(noise(n, rng), 300, 3000) * 0.06 * sw
    bubbles(x, rng, poisson(n, rng, 60), 0.6, 3, 0.2, True)
    return x, {"seconds": sec, "encounter_hz": 1.5}


@sound("sail_hull_plane", 1, loop=True, peak=-5.0, cat="Hull & water", radius=40, prio="high",
       trigger="loop: planing (speed > ~3.2 m/s, planing flag): hiss and roar under the hull")
def sail_hull_plane(k, rng):
    sec = 6.0
    n = N(sec)
    x = np.zeros(n)
    sw = np.clip(0.85 + 0.15 * smooth(n, rng, 2.0), 0.4, 1.3)
    x += cfilt(noise(n, rng), 900, 12000, 2, tilt=-1.5) * 0.25 * sw
    x += cfilt(noise(n, rng), 50, 320, 2) * 0.35 * np.clip(0.8 + 0.25 * smooth(n, rng, 4.0), 0.2, 1.4)
    bubbles(x, rng, poisson(n, rng, 350), 0.3, 1.2, 0.15, True)
    for t0 in train(n, rng, 6.5, 0.3, 1.0, scatter=0.15):  # chatter over the wavelets
        a = rng.uniform(0.3, 1.0)
        place(x, hull_knock(rng, 0.22 * a, "plastic", 0.3), t0, True)
        place(x, burst(rng, 0.1, 600, 4000, 0.0005, 0.015) * 0.25 * a, t0, True)
    for t0 in poisson(n, rng, 40):
        place(x, burst(rng, 0.01, 2000, 7000, 0.0002, 0.001) * rng.uniform(0.05, 0.2), t0, True)
    return x, {"seconds": sec}


@sound("sail_hull_hum", 1, loop=True, peak=-14.0, cat="Hull & water", radius=10,
       trigger="loop: planing fast, the foils humming (gain ~ speed past 3.5 m/s, pitch ~ speed / 4.5)")
def sail_hull_hum(k, rng):
    sec = 4.0
    n = N(sec)
    t = np.arange(n) / RATE
    f0 = 240.0  # a whole number of cycles in the loop
    dev = 0.015 * smooth(n, rng, 1.5)
    dev -= dev.mean()
    ph = TAU * (f0 * t + f0 * np.cumsum(dev) / RATE)
    a = np.clip(0.8 + 0.2 * smooth(n, rng, 2.0), 0.3, 1.2)
    x = (np.sin(ph) + 0.35 * np.sin(2 * ph + 0.4) + 0.12 * np.sin(3 * ph + 1.1)) * a
    x += cfilt(noise(n, rng), 180, 700) * 0.25 * a
    return x, {"seconds": sec, "f0_hz": f0}


@sound("sail_wave_slap_small", 5, peak=-6.0, cat="Hull & water", radius=15,
       trigger="one-shot: a small wave clops on the hull (at anchor, ashore in the shallows, ghosting)")
def sail_wave_slap_small(k, rng):
    sec = 0.8
    x = np.zeros(N(sec))
    place(x, hull_knock(rng, 0.6, "plastic", 0.5), 0.005)
    place(x, burst(rng, 0.3, 400, 5000, 0.001, 0.05) * 0.5, 0.0)
    place(x, cloud(rng, 0.7, 0.0, 0.2, int(rng.integers(20, 40)), 1, 4.5, 0.6), 0.01)
    return x, {"seconds": sec}


@sound("sail_wave_slap_large", 4, peak=-3.0, cat="Hull & water", radius=35,
       trigger="one-shot: the bow drops into a wave (pitching hard, upwind in a chop)")
def sail_wave_slap_large(k, rng):
    sec = 1.6
    x = np.zeros(N(sec))
    t = T(0.3)
    place(x, np.sin(TAU * np.cumsum(90 - 40 * t) / RATE) * env(t, 0.003, 0.06) * 0.9, 0.005)
    place(x, hull_knock(rng, 0.9, "plastic", 0.6), 0.005)
    place(x, burst(rng, 0.6, 300, 7000, 0.002, 0.11) * 0.7, 0.0)
    place(x, cloud(rng, 1.2, 0.0, 0.35, int(rng.integers(70, 110)), 1, 8, 0.7), 0.01)
    water_plips(x, rng, 0.3 + rng.random(25) * 1.0, 0.18)   # the spray falling back
    return x, {"seconds": sec}


@sound("sail_spray", 4, peak=-6.0, cat="Hull & water", radius=20,
       trigger="one-shot: a sheet of spray over the bow (wave slap large at speed > 2.5 m/s, wind > 6)")
def sail_spray(k, rng):
    sec = 1.5
    x = np.zeros(N(sec))
    t = T(sec)
    x += filt(noise(len(t), rng), 1500, 11000) * env(t, 0.04, 0.25) * 0.5
    x += filt(noise(len(t), rng), 400, 2000) * env(t, 0.01, 0.1) * 0.25
    for t0 in 0.15 + 0.6 * rng.random(70) ** 1.3:  # droplets drumming on deck and sail
        place(x, burst(rng, 0.012, 2000, 7000, 0.0002, 0.0012) * rng.uniform(0.05, 0.3), t0)
    water_plips(x, rng, 0.2 + rng.random(25) * 1.0, 0.12)
    return x, {"seconds": sec}


@sound("sail_cockpit_slosh", 1, loop=True, peak=-8.0, cat="Hull & water", radius=12,
       trigger="loop: water sloshing in a swamped cockpit (gain ~ water aboard)")
def sail_cockpit_slosh(k, rng):
    sec = 6.0
    n = N(sec)
    x = np.zeros(n)
    for t0 in train(n, rng, 0.667, 0.12, 0.3):
        slosh = burst(rng, 1.0, 200, 1800, 0.25, 0.35)
        place(x, slosh * 0.35, t0, True)
        bubbles(x, rng, t0 + 0.45 + 0.25 * rng.random(int(rng.integers(20, 40))), 2, 8, 0.5, True)
        place(x, hull_knock(rng, 0.15, "plastic", 0.3), t0 + 0.45, True)
    x = cfilt(x, peaks=((180, 3, 1.5), (420, 4, 0.8)))
    bubbles(x, rng, poisson(n, rng, 25), 1.5, 6, 0.2, True)
    return x, {"seconds": sec, "roll_hz": 0.667}


@sound("sail_bail", 4, peak=-4.0, cat="Hull & water", radius=15,
       trigger="one-shot: one scoop of the bailer: dipped, filled, thrown over the side")
def sail_bail(k, rng):
    sec = 1.7
    x = np.zeros(N(sec))
    t = T(0.12)
    place(x, (filt(noise(len(t), rng), 300, 3000) * np.sin(math.pi * t / 0.12) * 0.3 +
              hull_knock(rng, 0.2, "plastic", 0.12)), 0.0)
    place(x, cloud(rng, 0.5, 0.0, 0.25, 30, 2, 9, 0.45, 1.0), 0.08)
    water_plips(x, rng, 0.35 + rng.random(5) * 0.3, 0.2)
    th = 0.72 + 0.05 * k
    t = T(0.25)
    place(x, filt(noise(len(t), rng), 400, 4000) * (t / 0.25) ** 2 * 0.2, th - 0.25)
    place(x, burst(rng, 0.5, 300, 6000, 0.002, 0.08) * 0.8, th + 0.1)
    place(x, cloud(rng, 0.8, 0.0, 0.3, 70, 1, 7, 0.6), th + 0.1)
    water_plips(x, rng, th + 0.2 + rng.random(10) * 0.5, 0.15)
    return x, {"seconds": sec, "throw_s": th}


@sound("sail_capsize", 3, peak=-1.0, cat="Hull & water", radius=80, prio="high",
       trigger="one-shot: the boat capsizes (heel past ~70 deg): sail hits the water, crew falls in, the big splash")
def sail_capsize(k, rng):
    sec = 4.5
    x = np.zeros(N(sec))
    t = T(sec)
    ts = 0.35 + 0.05 * k
    x += filt(noise(len(t), rng), 300, 4000) * hann_env(t, 0, ts * 2) * (t < ts) * 0.2  # the sail swinging down
    place(x, sail_hit(rng, 0.8), ts)
    place(x, hull_knock(rng, 0.6, "plastic"), ts + 0.1)
    tb = ts + 0.25 + 0.05 * k                                                            # the crew going in
    tt = T(0.4)
    place(x, np.sin(TAU * np.cumsum(70 - 30 * tt) / RATE) * env(tt, 0.004, 0.09) * 1.2, tb)
    place(x, burst(rng, 1.0, 200, 8000, 0.003, 0.18), tb)
    place(x, cloud(rng, 3.0, 0.0, 1.6, 650, 1.5, 16, 0.8, 1.6), tb)
    water_plips(x, rng, tb + 0.3 + rng.random(60) * 1.4, 0.2)
    x += filt(noise(len(t), rng), 150, 1500) * env(t - tb - 0.4, 0.3, 1.0) * (t > tb + 0.4) * 0.15  # the slosh after
    return space(x, rng, 0.08, 0.6), {"seconds": sec, "sail_s": ts, "body_s": tb}


def sail_hit(rng, amp):
    sec = 1.3
    x = np.zeros(N(sec))
    t = T(sec)
    x += filt(noise(len(t), rng), 80, 2200) * env(t, 0.0008, 0.03) * 1.2
    x += np.sin(TAU * 68 * t) * env(t, 0.003, 0.07) * 0.9
    x += filt(noise(len(t), rng), 1000, 9000) * env(t - 0.02, 0.02, 0.18) * 0.35 * (t > 0.02)
    x += cloud(rng, sec, 0.01, 0.4, 120, 1, 7, 0.5)
    x += crinkle(len(t), rng, 150, wrap=False) * 0.06 * env(t - 0.1, 0.05, 0.5) * (t > 0.1)
    return amp * x


@sound("sail_hit_water", 3, peak=-3.0, cat="Hull & water", radius=40,
       trigger="one-shot: the sail slaps flat onto the water (a capsize to 90 deg, a dunked boom)")
def sail_hit_water(k, rng):
    return sail_hit(rng, 1.0), {"seconds": 1.3}


@sound("sail_climb_in", 3, peak=-4.0, cat="Hull & water", radius=15,
       trigger="one-shot: a sailor climbing back aboard, dripping (after a righting, from a swim)")
def sail_climb_in(k, rng):
    sec = 3.0
    x = np.zeros(N(sec))
    t = T(sec)
    tt = 0.05
    for i in range(3):
        place(x, hull_knock(rng, rng.uniform(0.4, 0.9), "plastic"), tt)
        place(x, burst(rng, 0.2, 200, 2000, 0.005, 0.04) * 0.3, tt)
        tt += rng.uniform(0.25, 0.45)
    # water pouring off: a trickle stream decaying (dense plips) into the cockpit and the sea
    rate = 120 * np.exp(-np.maximum(t - 0.2, 0) / 0.9) * (t > 0.2)
    water_plips(x, rng, poisson(len(t), rng, rate), 0.25)
    x += filt(noise(len(t), rng), 600, 5000) * env(t - 0.25, 0.05, 0.7) * (t > 0.25) * 0.08
    return x, {"seconds": sec}


@sound("sail_ground_scrape", 3, peak=-4.0, cat="Hull & water", radius=20,
       trigger="one-shot: the hull grounds on sand / runs up the beach (depth < board or hull draft while moving)")
def sail_ground_scrape(k, rng):
    sec = 1.8
    n = N(sec)
    t = np.arange(n) / RATE
    v = np.exp(-t / (0.45 + 0.1 * k)) * np.minimum(1, t / 0.03)
    grains = np.zeros(n)
    times = poisson(n, rng, 2500 * v)
    grains[(times * RATE).astype(int)] = rng.standard_normal(len(times))
    hullir = modes(0.15, [(112, 0.05, 1.0), (178, 0.04, 0.8), (265, 0.03, 0.6), (410, 0.02, 0.4)])
    x = signal.fftconvolve(filt(grains, 150, 3000), hullir)[:n] * 0.08 + filt(grains, 1000, 6000) * 0.4
    x += filt(noise(n, rng), 300, 2500) * v * 0.15
    x += burst(rng, sec, 60, 300, 0.005, 0.12) * 0.6
    return x, {"seconds": sec}


@sound("sail_bump_plastic", 4, peak=-4.0, cat="Hull & water", radius=20,
       trigger="one-shot: hull bumps another boat or the trolley (GRP/plastic thud)")
def sail_bump_plastic(k, rng):
    sec = 0.7
    x = hull_knock(rng, 1.0, "plastic", sec)
    if k % 2:
        place(x, hull_knock(rng, 0.5, "plastic", sec - 0.08), 0.06)
    return x, {"seconds": sec}


@sound("sail_bump_wood", 4, peak=-4.0, cat="Hull & water", radius=20,
       trigger="one-shot: hull bumps the pontoon / the slip's boards (plywood thud)")
def sail_bump_wood(k, rng):
    sec = 0.7
    x = hull_knock(rng, 1.0, "wood", sec) + hull_knock(rng, 0.2, "plastic", sec)
    return x, {"seconds": sec}

# ------------------------------------------------------------------------------------------------------------ shore


@sound("sail_trolley_boards", 1, loop=True, peak=-6.0, cat="Shore", radius=25,
       trigger="loop: a launching trolley rolled over the slip's boards (pitch ~ walking speed / 1.2 m/s)")
def sail_trolley_boards(k, rng):
    sec = 6.0
    n = N(sec)
    x = np.zeros(n)
    x += cfilt(noise(n, rng), 60, 450) * 0.25 * np.clip(0.8 + 0.2 * smooth(n, rng, 3), 0.3, 1.3)
    for t0 in train(n, rng, 8.5, 0.06, 0.6):  # a plank's edge under the wheel
        j = rng.uniform(0.9, 1.1)
        b = modes(0.12, [(160 * j, 0.03, 1.0), (330 * j, 0.02, 0.7), (520 * j, 0.012, 0.5)], rng, 1500)
        place(x, b * rng.uniform(0.4, 0.8), t0, True)
        place(x, b * rng.uniform(0.25, 0.5), t0 + 0.035, True)  # the other wheel
    for t0 in poisson(n, rng, 6):  # the frame rattling
        place(x, click(rng, rng.uniform(0.04, 0.12), 0.6, 0.04), t0, True)
    for t0 in poisson(n, rng, 0.35):  # the hull shifting on the pads
        c = stick_slip(rng, 0.25, 40, 90, [(300, 5, 1.0), (650, 6, 0.6)])
        place(x, c * 0.4, t0, True)
    return x, {"seconds": sec, "plank_hz": 8.5}


@sound("sail_trolley_sand", 1, loop=True, peak=-7.0, cat="Shore", radius=20,
       trigger="loop: the trolley rolled over wet sand to the water's edge")
def sail_trolley_sand(k, rng):
    sec = 6.0
    n = N(sec)
    x = np.zeros(n)
    sw = np.clip(0.8 + 0.25 * smooth(n, rng, 2.0), 0.3, 1.4)
    grains = np.zeros(n)
    times = poisson(n, rng, 1600 * sw)
    grains[(times * RATE).astype(int) % n] = rng.standard_normal(len(times))
    x += cfilt(grains, 120, 1600) * 0.6 + cfilt(grains, 1500, 6000) * 0.15
    x += cfilt(noise(n, rng), 50, 300) * 0.2 * sw
    for t0 in poisson(n, rng, 2.5):  # water squelching out of the sand
        bubbles(x, rng, t0 + 0.05 * rng.random(3), 3, 8, 0.25, True)
    for t0 in poisson(n, rng, 0.3):
        place(x, stick_slip(rng, 0.25, 40, 90, [(300, 5, 1.0), (650, 6, 0.6)]) * 0.3, t0, True)
    return x, {"seconds": sec}


@sound("sail_hull_drag", 3, peak=-5.0, cat="Shore", radius=20,
       trigger="one-shot: a hull heaved a metre over the sand (no trolley; kids pulling an optimist up)")
def sail_hull_drag(k, rng):
    sec = 1.6 + 0.2 * k
    n = N(sec)
    t = np.arange(n) / RATE
    v = np.sin(math.pi * np.clip(t / (sec - 0.2), 0, 1)) ** 1.5
    v *= 1 + 0.3 * np.sin(TAU * rng.uniform(4, 7) * t)
    grains = np.zeros(n)
    times = poisson(n, rng, 3000 * v)
    grains[(times * RATE).astype(int)] = rng.standard_normal(len(times))
    hullir = modes(0.15, [(112, 0.05, 1.0), (178, 0.04, 0.8), (265, 0.03, 0.6), (410, 0.02, 0.4)])
    x = signal.fftconvolve(filt(grains, 150, 3000), hullir)[:n] * 0.06 + filt(grains, 900, 5000) * 0.35
    x += filt(noise(n, rng), 250, 2500) * v * 0.12
    return x, {"seconds": sec}


@sound("sail_hose", 1, loop=True, peak=-7.0, cat="Shore", radius=20,
       trigger="loop: a hose washing the salt off a hull at the boat park (after sailing)")
def sail_hose(k, rng):
    sec = 6.0
    n = N(sec)
    x = np.zeros(n)
    sw = np.clip(0.8 + 0.25 * smooth(n, rng, 1.0), 0.3, 1.3)
    x += cfilt(noise(n, rng), 1200, 14000, tilt=-1) * 0.25 * sw          # the jet's hiss and splatter
    drum = cfilt(noise(n, rng), 60, 900, peaks=((112, 6, 4), (178, 6, 3), (265, 6, 2), (410, 6, 1.5)))
    x += drum * 0.2 * sw                                                # on the hollow hull
    water_plips(x, rng, poisson(n, rng, 70), 0.12, True)                # drips off the hull into puddles
    return x, {"seconds": sec}


@sound("sail_sail_roll", 3, peak=-6.0, cat="Shore", radius=12,
       trigger="one-shot: a sail being rolled up / unrolled on the grass (dacron crackle)")
def sail_sail_roll(k, rng):
    sec = 2.5
    n = N(sec)
    t = np.arange(n) / RATE
    g = np.zeros(n)
    tt = 0.05
    while tt < sec - 0.4:
        L = rng.uniform(0.3, 0.6)
        g += hann_env(t, tt, L) * rng.uniform(0.6, 1.0)
        tt += L * rng.uniform(0.7, 1.1)
    x = crinkle(n, rng, 50 + 900 * g, 1500, 11000) * 0.25 * g
    x += filt(noise(n, rng), 300, 3500) * g * 0.12
    return x, {"seconds": sec}


@sound("sail_mast_step", 3, peak=-3.0, cat="Shore", radius=30,
       trigger="one-shot: rigging: the mast dropped into its step (clunk, then the shrouds and halyard rattle)")
def sail_mast_step(k, rng):
    sec = 1.6
    x = np.zeros(N(sec))
    t = T(0.3)
    place(x, np.sin(TAU * np.cumsum(140 - 60 * t) / RATE) * env(t, 0.002, 0.04) * 0.8, 0.0)
    place(x, mast_ring(rng, rng.uniform(150, 210), 1200, 0.8, 1.5, 0.35), 0.0)
    place(x, hull_knock(rng, 0.7, "plastic"), 0.0)
    for t0 in 0.12 + rng.random(5) * 0.6:
        place(x, halyard_slap(rng, rng.uniform(260, 420), rng.uniform(0.1, 0.3), 0.8, 0.8), t0)
    return space(x, rng, 0.08, 0.5), {"seconds": sec}


def door(rng, close, sec=3.6):
    n = N(sec)
    t = np.arange(n) / RATE
    run = sec - 0.9
    v = np.clip(np.minimum(t / 0.5, (run - t) / 0.4), 0, 1)
    roll = filt(noise(n, rng), 40, 320) * v
    # the big sheet panel booming with the rollers
    panel = [(38, 0.5, 1.0), (61, 0.45, 0.9), (87, 0.4, 0.8), (124, 0.3, 0.6), (190, 0.25, 0.5), (305, 0.2, 0.3)]
    pir = modes(0.8, panel)
    x = signal.fftconvolve(roll, pir)[:n] * 0.03 + roll * 0.4
    x += filt(noise(n, rng), 350, 3000) * v * (1 + 0.5 * np.sin(TAU * 17 * t)) * 0.06
    # rail joints, every ~0.5 s of travel
    for t0 in np.arange(0.3, run - 0.1, 0.5) + rng.uniform(-0.05, 0.05):
        place(x, mix(modes(0.3, [(220, 0.04, 1.0), (640, 0.02, 0.6), (1450, 0.01, 0.3)], rng) * 0.5,
                     signal.fftconvolve(burst(rng, 0.05, 40, 400, 0.002, 0.01), pir)[:N(0.6)] * 0.02) * v[min(n - 1, N(t0))], t0)
    # a roller squeal
    sq0, sql = rng.uniform(0.6, 1.2), rng.uniform(0.3, 0.6)
    ts = T(sql)
    fq = rng.uniform(1300, 1900) * (1 + 0.02 * np.sin(TAU * 7 * ts))
    place(x, np.sin(TAU * np.cumsum(fq) / RATE) * np.sin(math.pi * ts / sql) * 0.08, sq0)
    # the end stop
    stop = run
    big = panel + [(450, 0.15, 0.4), (720, 0.1, 0.3), (1150, 0.07, 0.2), (2200, 0.04, 0.15)]
    place(x, mix(modes(0.9, big, rng, 2500 if close else 500) * (1.0 if close else 0.4),
                 burst(rng, 0.1, 100, 3000, 0.0005, 0.01) * (0.6 if close else 0.2)), stop)
    if close:  # the latch
        place(x, click(rng, 0.3, 0.6, 0.1), stop + 0.12)
    return space(x, rng, 0.2, 0.8, ((0.045, 0.2),), 4000)


@sound("sail_boathouse_open", 2, peak=-4.0, cat="Shore", radius=50,
       trigger="one-shot: the boathouse's big sliding door rolled open (morning, before the first lesson)")
def sail_boathouse_open(k, rng):
    return door(rng, False), {"seconds": 3.6}


@sound("sail_boathouse_close", 2, peak=-3.0, cat="Shore", radius=60,
       trigger="one-shot: the boathouse door rolled shut with a bang (evening)")
def sail_boathouse_close(k, rng):
    return door(rng, True), {"seconds": 3.6}


@sound("sail_buckle", 4, peak=-8.0, cat="Shore", radius=6, prio="low",
       trigger="one-shot: a life jacket's buckle clicked shut (putting on / taking off)")
def sail_buckle(k, rng):
    sec = 0.3
    x = np.zeros(N(sec))
    place(x, burst(rng, 0.04, 1500, 8000, 0.01, 0.01) * 0.15, 0.0)
    j = rng.uniform(0.9, 1.1)
    snap = [(2500 * j, 0.005, 1.0), (4200 * j, 0.004, 0.8), (6800 * j, 0.003, 0.5), (1300 * j, 0.008, 0.3)]
    for i, d in enumerate((0.04, 0.04 + rng.uniform(0.006, 0.012))):
        place(x, mix(modes(0.1, snap, rng) * (1.0 if i == 0 else 0.7), burst(rng, 0.01, 2000, 12000, 0.0001, 0.0004) * 0.4), d)
    return x, {"seconds": sec}


@sound("sail_flag_squeak", 3, peak=-7.0, cat="Shore", radius=25,
       trigger="one-shot: a signal flag hoisted on the signal mast (the pulley squeaking hand over hand)")
def sail_flag_squeak(k, rng):
    sec = 2.2
    x = np.zeros(N(sec))
    tt = 0.05
    f = rng.uniform(1500, 2300)
    for i in range(4 + k % 2):
        L = rng.uniform(0.09, 0.16)
        ts = T(L)
        fq = f * rng.uniform(0.94, 1.06) * (1 + 0.04 * np.sin(TAU * rng.uniform(20, 35) * ts)) * (1 + 0.08 * ts / L)
        sq = np.sin(TAU * np.cumsum(fq) / RATE) + 0.3 * np.sin(2 * TAU * np.cumsum(fq) / RATE)
        place(x, sq * np.sin(math.pi * ts / L) ** 0.6 * 0.4, tt)
        place(x, burst(rng, 0.2, 800, 5000, 0.02, 0.06) * 0.15, tt)
        tt += rng.uniform(0.32, 0.42)
    for t0 in rng.random(3) * (sec - 0.4):
        place(x, halyard_slap(rng, rng.uniform(500, 700), 0.12, 0.3, 0.5), t0)
    return x, {"seconds": sec}


@sound("sail_flag_light", 1, loop=True, peak=-12.0, cat="Shore", radius=15,
       trigger="loop: a small flag (signal mast, the launch's staff) in light wind 2-5 m/s")
def sail_flag_light(k, rng):
    return flutter_loop(6.0, rng, 0.15, True), {"seconds": 6.0, "strength": 0.15}


@sound("sail_flag_mid", 1, loop=True, peak=-9.0, cat="Shore", radius=20,
       trigger="loop: a small flag in a fresh wind 5-9 m/s")
def sail_flag_mid(k, rng):
    return flutter_loop(6.0, rng, 0.5, True), {"seconds": 6.0, "strength": 0.5}


@sound("sail_flag_strong", 1, loop=True, peak=-6.0, cat="Shore", radius=30,
       trigger="loop: a small flag snapping in a strong wind > 9 m/s")
def sail_flag_strong(k, rng):
    return flutter_loop(6.0, rng, 0.9, True), {"seconds": 6.0, "strength": 0.9}


@sound("sail_windsock", 1, loop=True, peak=-14.0, cat="Shore", radius=12, prio="low",
       trigger="loop: the windsock rustling on its pole (gain ~ wind from 2 m/s)")
def sail_windsock(k, rng):
    sec = 8.0
    n = N(sec)
    x = np.zeros(n)
    sw = np.clip(0.7 + 0.3 * smooth(n, rng, 0.4), 0.2, 1.3)
    for t0 in train(n, rng, 3.5, 0.3, 0.5):
        place(x, ripple(rng, 0.4 * sw[int(t0 * RATE) % n], 500, 5000, 0.08, 0.3), t0, True)
    x += cfilt(noise(n, rng), 600, 5000) * 0.06 * sw
    for t0 in poisson(n, rng, 0.4):
        place(x, cloth_snap(rng, 0.25, 0.4, 0.06), t0, True)
    x += crinkle(n, rng, 30 * sw, 2500, 9000, True) * 0.03
    return x, {"seconds": sec}

# ---------------------------------------------------------------------------------------------------------- signals


def pea_whistle(rng, blasts, sec):
    """A coach's pea whistle (~3 kHz): the pea's rattle modulating pitch and level ~35 Hz, breath noise, a tongued onset."""
    n = N(sec)
    t = np.arange(n) / RATE
    f0 = rng.uniform(2850, 3300)
    a = np.zeros(n)
    for b0, bl in blasts:
        a += np.clip(np.minimum((t - b0) / 0.012, (b0 + bl - t) / 0.04), 0, 1)
    a = np.minimum(a, 1)
    a = np.convolve(a, np.ones(96) / 96, "same")
    pea_rate = rng.uniform(30, 40) * (1 + 0.1 * smooth(n, rng, 3.0))
    pea = np.sin(TAU * np.cumsum(pea_rate) / RATE)
    pea_shape = np.sign(pea) * np.abs(pea) ** 0.5
    scoop = 1 - 0.03 * np.exp(-np.maximum(t - 0, 0) / 0.02)
    f = f0 * (1 + 0.045 * pea_shape) * scoop
    ph = TAU * np.cumsum(f) / RATE
    tone = np.sin(ph) + 0.12 * np.sin(2 * ph) + 0.04 * np.sin(3 * ph)
    am = 1 - 0.35 * (0.5 + 0.5 * pea_shape)
    breath = reson(noise(n, rng), f0, 4) * 0.25 + filt(noise(n, rng), 1500, 9000) * 0.08
    x = (tone * am + breath) * a
    return space(x, rng, 0.12, 1.0, ((0.38, 0.12), (0.71, 0.06)), 5000)


@sound("sail_whistle_short", 3, peak=-3.0, cat="Signals", radius=150, prio="high",
       trigger="one-shot: the coach's whistle, one short blast (attention, 'go')")
def sail_whistle_short(k, rng):
    bl = 0.3 + 0.06 * k
    return pea_whistle(rng, [(0.02, bl)], bl + 1.1), {"blast_s": bl}


@sound("sail_whistle_long", 2, peak=-3.0, cat="Signals", radius=150, prio="high",
       trigger="one-shot: one long blast (everybody, come in to the slip)")
def sail_whistle_long(k, rng):
    bl = 1.3 + 0.3 * k
    return pea_whistle(rng, [(0.02, bl)], bl + 1.1), {"blast_s": bl}


@sound("sail_whistle_recall", 2, peak=-3.0, cat="Signals", radius=150, prio="high",
       trigger="one-shot: three blasts (recall / a boat in trouble)")
def sail_whistle_recall(k, rng):
    bl = 0.32
    gap = 0.22 + 0.04 * k
    blasts = [(0.02 + i * (bl + gap), bl) for i in range(3)]
    return pea_whistle(rng, blasts, 3 * (bl + gap) + 1.1), {"blast_s": bl, "gap_s": gap}


def air_horn(rng, length, sec):
    """A canister air horn: a diaphragm reed's buzzy pulse wave (~430 Hz) through the trumpet's formant."""
    n = N(sec)
    t = np.arange(n) / RATE
    f0 = rng.uniform(410, 450)
    a = np.clip(np.minimum((t - 0.02) / 0.03, (0.02 + length - t) / 0.08), 0, 1)
    bend = 1 - 0.06 * np.exp(-np.maximum(t - 0.02, 0) / 0.05) - 0.04 * np.clip((t - 0.02 - length + 0.1) / 0.1, 0, 1)
    f = f0 * bend * (1 + 0.004 * smooth(n, rng, 8))
    ph = TAU * np.cumsum(f) / RATE
    x = np.zeros(n)
    for h in range(1, 40):
        if h * f0 > 16000:
            break
        x += np.sin(h * ph) / h ** 0.6
    x = filt(x, 300, 9000)
    x = reson(x, 1800, 1.5) * 0.6 + reson(x, 3200, 2.0) * 0.35 + x * 0.25
    x = np.tanh(2.0 * x / (np.max(np.abs(x)) + 1e-9)) * a * (1 + 0.04 * smooth(n, rng, 20))
    return space(x, rng, 0.15, 1.6, ((0.55, 0.22), (1.15, 0.1), (1.8, 0.05)), 3500)


@sound("sail_horn_short", 2, peak=-1.0, cat="Signals", radius=400, prio="high",
       trigger="one-shot: start signal horn, short blast (the start, a minute signal)")
def sail_horn_short(k, rng):
    L = 0.8 + 0.15 * k
    return air_horn(rng, L, L + 2.2), {"blast_s": L}


@sound("sail_horn_long", 2, peak=-1.0, cat="Signals", radius=400, prio="high",
       trigger="one-shot: start signal horn, long blast (the warning signal, finish of the first boat)")
def sail_horn_long(k, rng):
    L = 2.2 + 0.4 * k
    return air_horn(rng, L, L + 2.2), {"blast_s": L}


def ship_bell(rng, strikes, sec):
    """A small brass ship's bell: bell partials (hum, prime, tierce, quint, nominal ...) as split doublets, the clapper."""
    n = N(sec)
    prime = rng.uniform(820, 900)
    partials = [(0.5, 7.0, 0.5), (1.0, 4.5, 0.8), (1.19, 3.5, 0.6), (1.5, 2.5, 0.4), (2.0, 2.8, 1.0), (2.5, 1.6, 0.5),
                (2.66, 1.4, 0.35), (3.0, 1.1, 0.4), (4.0, 0.7, 0.25), (5.33, 0.4, 0.15), (6.4, 0.25, 0.1)]
    x = np.zeros(n)
    for s0, sa in strikes:
        parts = []
        for r, tau, g in partials:
            f = prime * r
            tau *= 0.55
            beat = rng.uniform(0.6, 2.2)
            parts += [(f, tau, g * 0.6), (f + beat, tau * 0.9, g * 0.4)]
        one = modes(sec - s0, parts, rng, 6000) + burst(rng, sec - s0, 2500, 10000, 0.0001, 0.003) * 0.4
        place(x, one * sa, s0)
    return space(x, rng, 0.1, 1.2, ((0.42, 0.1),), 6000)


@sound("sail_bell_single", 2, peak=-2.0, cat="Signals", radius=200, prio="high",
       trigger="one-shot: the school's bell by the boathouse, one stroke (half-hours of the timetable)")
def sail_bell_single(k, rng):
    return ship_bell(rng, [(0.01, 1.0)], 5.5), {"strikes": 1}


@sound("sail_bell_double", 2, peak=-2.0, cat="Signals", radius=200, prio="high",
       trigger="one-shot: the bell, a double stroke ding-ding (lesson start/end, the hours in pairs)")
def sail_bell_double(k, rng):
    gap = 0.42 + 0.06 * k
    return ship_bell(rng, [(0.01, 1.0), (0.01 + gap, 0.85)], 6.0), {"strikes": 2, "gap_s": gap}


@sound("sail_megaphone_click", 3, peak=-8.0, cat="Signals", radius=40,
       trigger="one-shot: the coach's megaphone keyed (before and after the words; the words are voice lines)")
def sail_megaphone_click(k, rng):
    sec = 0.5
    x = np.zeros(N(sec))
    place(x, click(rng, 0.8, 0.8, 0.05), 0.01)
    t = T(sec - 0.02)
    pop = np.sin(TAU * 60 * t) * env(t, 0.001, 0.02) * 0.6
    hiss = filt(noise(len(t), rng), 400, 4000) * 0.05 * (np.exp(-t / 0.25) if k == 2 else np.minimum(1, t / 0.02))
    hum = np.sin(TAU * 100 * t) * 0.02 + np.sin(TAU * 300 * t) * 0.01
    place(x, np.tanh(3 * reson(pop + hiss + hum, 1200, 1.0)) * 0.5, 0.02)
    return x, {"seconds": sec}


@sound("sail_megaphone_squeal", 2, peak=-4.0, cat="Signals", radius=80,
       trigger="one-shot: megaphone feedback squeal (pointed the wrong way; comic beat)")
def sail_megaphone_squeal(k, rng):
    sec = 1.3
    n = N(sec)
    t = np.arange(n) / RATE
    f = rng.uniform(2100, 2600) * (1 + 0.06 * t / sec) * (1 + 0.003 * np.sin(TAU * 6 * t))
    a = np.minimum(1, (t / 0.7) ** 2) * np.clip((sec - 0.1 - t) / 0.03, 0, 1)
    x = np.sin(TAU * np.cumsum(f) / RATE) * a + filt(noise(n, rng), 400, 4000) * 0.03
    x = np.tanh(4 * x)
    x = reson(x, 1500, 0.8) + 0.3 * x
    return space(x, rng, 0.12, 0.9, ((0.3, 0.1),)), {"seconds": sec}


@sound("sail_start_clap", 3, peak=-1.0, cat="Signals", radius=300, prio="high",
       trigger="one-shot: the starter's clap (a starting-pistol-like report) instead of the horn")
def sail_start_clap(k, rng):
    sec = 2.6
    x = np.zeros(N(sec))
    t = T(0.02)
    nwave = np.where(t < 0.0012, 1 - 2 * t / 0.0012, 0) * (t < 0.0012)
    place(x, nwave * 1.0, 0.002)
    place(x, burst(rng, 0.2, 300, 12000, 0.0001, 0.006) * 0.8, 0.002)
    place(x, burst(rng, 0.4, 60, 600, 0.001, 0.05) * 0.4, 0.002)
    return space(x, rng, 0.2, 2.0, ((0.028, 0.35), (0.41, 0.18), (0.83, 0.09), (1.3, 0.05)), 3000), {"seconds": sec}


@sound("sail_stopwatch", 1, loop=True, peak=-12.0, cat="Signals", radius=3, prio="low",
       trigger="loop: the coach's stopwatch ticking (5 beats/s) for the last 10 s of a start sequence (HUD)")
def sail_stopwatch(k, rng):
    sec = 2.0
    n = N(sec)
    x = np.zeros(n)
    for i in range(10):
        j = 1.0 if i % 2 == 0 else 1.08
        tick = modes(0.03, [(3200 * j, 0.002, 1.0), (5500 * j, 0.0015, 0.7), (8100 * j, 0.001, 0.4),
                            (1400 * j, 0.004, 0.25)], rng)
        place(x, tick * (1.0 if i % 2 == 0 else 0.8), i * 0.2 + 0.01, True)
    x += cfilt(noise(n, rng), 2000, 8000) * 0.003
    return x, {"seconds": sec, "beats_per_s": 5}

# ----------------------------------------------------------------------------------------------------------- launch

EXH = [(190, 0.009, 1.0), (430, 0.006, 0.6), (880, 0.004, 0.35), (1700, 0.002, 0.15)]


def engine_frame(n, rng, times, rpm, roughness, wrap):
    """The firing train: each pop through the exhaust's resonances; under-water burble; piston rattle; gear whine."""
    x = np.zeros(n)
    pop_ir = modes(0.05, EXH)
    pulses = np.zeros(n)
    amps = np.clip(1 + roughness * rng.standard_normal(len(times)), 0.1, 2)
    miss = rng.random(len(times)) < roughness * 0.25
    amps[miss] *= 0.1
    idx = (np.asarray(times) * RATE).astype(int) % n
    np.add.at(pulses, idx, amps)
    if wrap:
        pir = np.pad(pop_ir, (0, n - len(pop_ir)))
        pops = np.fft.irfft(np.fft.rfft(pulses) * np.fft.rfft(pir), n)
        F = lambda y, lo, hi: cfilt(y, lo, hi)
    else:
        pops = signal.fftconvolve(pulses, pop_ir)[:n]
        F = lambda y, lo, hi: filt(y, lo, hi)
    x += pops * 0.9
    # each pop's rasp: a burst of exhaust noise (per-pop envelope, ~3 ms)
    kern = np.exp(-np.arange(N(0.012)) / (0.003 * RATE))
    if wrap:
        pe = np.fft.irfft(np.fft.rfft(pulses) * np.fft.rfft(np.pad(kern, (0, n - len(kern)))), n)
    else:
        pe = signal.fftconvolve(pulses, kern)[:n]
    pe /= np.max(pe) + 1e-9
    x += F(noise(n, rng), 300, 3500) * pe * 0.25
    # exhaust out through the prop hub: a gurgling burble keyed to the pops
    env_p = F(np.abs(pops), None, 60)
    x += F(noise(n, rng), 80, 900) * env_p * 1.0
    # piston/crank rattle and intake hiss, louder with rpm
    mech = F(noise(n, rng), 1500, 6000) * (0.3 + 0.7 * pe)
    x += mech * (0.03 + 0.07 * rpm / 5000)
    # the lower unit's gear whine
    t = np.arange(n) / RATE
    fw = rpm / 60 * 13 / 2
    if wrap:  # whole cycles in the loop
        fw = round(fw * n / RATE) / (n / RATE)
    x += np.sin(TAU * fw * t) * 0.008 * (rpm / 5000) ** 1.5
    return x


def engine_loop(sec, rng, rpm, roughness):
    fire = 2 * rpm / 60.0  # a two-stroke twin fires twice a turn
    sec = round(fire * sec) / fire
    n = N(sec)
    # a whole number of firings in the loop (train rescales); jitter is per firing and wraps
    times = train(n, rng, fire, 0.01 + 0.03 * roughness, 1.5)
    times = times + rng.normal(0, roughness * 0.12 / fire, len(times))
    return engine_frame(n, rng, times, rpm, roughness, True), sec


def launch_loop(name, rpm, roughness, k, rng):
    x, sec = engine_loop(4.0, rng, rpm, roughness)
    return x, {"seconds": round(sec, 4), "rpm": rpm, "firing_hz": round(2 * rpm / 60, 2), "roughness": roughness}


@sound("sail_outboard_idle", 1, loop=True, peak=-6.0, cat="Launch", radius=60,
       trigger="loop: the coach's launch outboard («Вихрь»-like two-stroke twin) idling, ~1000 rpm")
def sail_outboard_idle(k, rng):
    return launch_loop("idle", 1000, 0.35, k, rng)


@sound("sail_outboard_low", 1, loop=True, peak=-5.0, cat="Launch", radius=100,
       trigger="loop: 1800 rpm (slow along the course); crossfade by pitch, see LyudSail::Sound::MixOutboard")
def sail_outboard_low(k, rng):
    return launch_loop("low", 1800, 0.2, k, rng)


@sound("sail_outboard_mid", 1, loop=True, peak=-4.0, cat="Launch", radius=150,
       trigger="loop: 3000 rpm (cruising to a mark)")
def sail_outboard_mid(k, rng):
    return launch_loop("mid", 3000, 0.1, k, rng)


@sound("sail_outboard_high", 1, loop=True, peak=-3.0, cat="Launch", radius=200, prio="high",
       trigger="loop: 4800 rpm (flat out, planing to a capsized boat)")
def sail_outboard_high(k, rng):
    return launch_loop("high", 4800, 0.06, k, rng)


def recoil(rng, x, t0, catch):
    """A pull on the recoil starter: the pawls ratcheting, the rope rasping, the engine turning over."""
    pl = rng.uniform(0.45, 0.6)
    tt = t0
    while tt < t0 + pl:
        u = (tt - t0) / pl
        place(x, click(rng, 0.25 * (0.5 + 0.5 * math.sin(math.pi * u)), 0.55, 0.03), tt)
        tt += 1 / (25 + 70 * math.sin(math.pi * u))
    t = T(pl)
    place(x, filt(noise(len(t), rng), 1200, 6000) * np.sin(math.pi * t / pl) * 0.12, t0)
    # compressions: chuffs accelerating then slowing
    tt, gap = t0 + 0.08, 0.16
    for i in range(int(rng.integers(3, 6))):
        c = burst(rng, 0.15, 50, 500, 0.004, 0.035) * 0.8 + modes(0.15, EXH[:2], rng) * 0.4
        place(x, c * rng.uniform(0.6, 1.0), tt)
        gap *= 0.75 if tt < t0 + pl * 0.6 else 1.35
        tt += gap
    end = t0 + pl
    if not catch:  # the rope rewinding, the handle knocking the cowling
        tt = end + 0.05
        rate = 70
        while rate > 12:
            place(x, click(rng, 0.12, 0.5, 0.03), tt)
            tt += 1 / rate
            rate *= 0.93
        place(x, modes(0.2, [(620, 0.02, 1.0), (1500, 0.01, 0.6), (240, 0.03, 0.5)], rng) * 0.5, tt + 0.02)
    return end


@sound("sail_outboard_pull_fail", 3, peak=-4.0, cat="Launch", radius=40,
       trigger="one-shot: a pull of the starter cord that doesn't catch (cold engine: 1-3 of these first)")
def sail_outboard_pull_fail(k, rng):
    sec = 1.8
    x = np.zeros(N(sec))
    recoil(rng, x, 0.02, False)
    return x, {"seconds": sec}


@sound("sail_outboard_pull_catch", 2, peak=-3.0, cat="Launch", radius=60,
       trigger="one-shot: a pull that catches: it sputters up to idle; crossfade into sail_outboard_idle at the end")
def sail_outboard_pull_catch(k, rng):
    sec = 3.5
    n = N(sec)
    x = np.zeros(n)
    t1 = recoil(rng, x, 0.02, True)
    # firings: ragged pops accelerating, overshooting, settling to idle
    times = []
    tt = t1 - 0.05
    while tt < sec:
        u = tt - t1
        rpm = 1000 + 900 * np.exp(-((u - 0.5) / 0.35) ** 2) - 700 * np.exp(-u / 0.15)
        times.append(tt)
        tt += 60.0 / (2 * max(rpm, 200)) * rng.uniform(0.85, 1.15)
    times = np.array(times)
    e = engine_frame(n, rng, times, 1200, 0.45, False)
    e[:N(max(t1 - 0.05, 0))] = 0
    x += e * np.clip((T(sec) - t1 + 0.05) / 0.05, 0, 1)
    return x, {"seconds": sec, "fires_s": round(t1, 3)}


@sound("sail_outboard_off", 2, peak=-4.0, cat="Launch", radius=60,
       trigger="one-shot: the kill cord pulled at idle: the last coughs, the engine coasting down")
def sail_outboard_off(k, rng):
    sec = 2.4
    n = N(sec)
    t = np.arange(n) / RATE
    times = []
    tt = 0.0
    gap = 1 / 33.0
    while tt < 0.55:
        times.append(tt)
        gap *= 1.05 if tt > 0.25 else 1.0
        tt += gap * rng.uniform(0.85, 1.2)
    times += [0.75 + 0.05 * k]  # the last cough
    x = engine_frame(n, rng, np.array(times), 900, 0.4, False)
    x *= np.where(t < 0.85, 1.0, np.exp(-(t - 0.85) / 0.08))
    x += filt(noise(n, rng), 1500, 5000) * 0.01 * np.exp(-t / 0.5)        # the crank coasting
    x += cloud(rng, sec, 0.6, 1.4, 40, 2, 10, 0.12, 1.0)                  # water draining out of the leg
    return x, {"seconds": sec}


@sound("sail_alu_slap", 4, peak=-3.0, cat="Launch", radius=80,
       trigger="one-shot: the launch's aluminium hull banging on a wave at speed (rate ~ speed x chop)")
def sail_alu_slap(k, rng):
    sec = 1.2
    x = np.zeros(N(sec))
    place(x, hull_knock(rng, 1.0, "alu", 1.1), 0.005)
    place(x, burst(rng, 0.4, 300, 6000, 0.001, 0.06) * 0.6, 0.0)
    place(x, cloud(rng, 1.0, 0.0, 0.3, 60, 1, 7, 0.5), 0.01)
    t = T(0.8)
    place(x, filt(noise(len(t), rng), 1500, 10000) * env(t, 0.05, 0.2) * 0.25, 0.05)
    return x, {"seconds": sec}

# ------------------------------------------------------------------------------------------------------------ crowd


def finger_whistle(rng, sec):
    t = T(sec)
    f = 2300 + 700 * np.sin(math.pi * np.clip(t / sec, 0, 1)) - 500 * (t / sec) ** 2
    a = np.sin(math.pi * np.clip(t / sec, 0, 1)) ** 0.5
    return (np.sin(TAU * np.cumsum(f) / RATE) + 0.1 * filt(noise(len(t), rng), 2000, 5000)) * a


@sound("sail_crowd_cheer", 2, peak=-3.0, cat="Crowd", radius=120, prio="high",
       trigger="one-shot: the pier cheering a finish (claps building, a couple of whistles)")
def sail_crowd_cheer(k, rng):
    sec = 5.0
    x = applause(sec, rng, 28, (4.5, 7.0), 0.5, 4.2)
    for _ in range(2 + k):
        place(x, finger_whistle(rng, rng.uniform(0.5, 0.8)) * rng.uniform(0.15, 0.3), rng.uniform(0.2, 2.0))
    return space(x, rng, 0.15, 1.0, ((0.05, 0.15),)), {"seconds": sec, "people": 28}


@sound("sail_prize_claps", 3, peak=-4.0, cat="Crowd", radius=60,
       trigger="one-shot: polite applause at the prize-giving (a name read out, a cup handed over)")
def sail_prize_claps(k, rng):
    sec = 4.5 + 0.5 * k
    x = applause(sec, rng, 16 + 4 * k, (3.0, 4.5), 0.6, sec - 0.6)
    return space(x, rng, 0.12, 0.8, ((0.04, 0.12),)), {"seconds": sec, "people": 16 + 4 * k}


def brass_note(rng, f, start, length, sec, amp=1.0):
    """A brass note: harmonics brightening with loudness, a lip scoop into pitch, vibrato after the attack."""
    n = N(sec)
    t = np.arange(n) / RATE - start
    a = np.clip(np.minimum(t / 0.03, (length - t) / 0.08), 0, 1) * (t >= 0)
    a = a * (1 - 0.15 * np.exp(-np.maximum(t, 0) / 0.08))
    scoop = 1 - 0.018 * np.exp(-np.maximum(t, 0) / 0.035)
    vib = 1 + 0.003 * np.sin(TAU * 5.5 * t) * np.clip((t - 0.25) / 0.2, 0, 1)
    ph = TAU * np.cumsum(f * scoop * vib * (1 + rng.uniform(-0.002, 0.002))) / RATE
    x = np.zeros(n)
    bright = 1.2 + 5 * a
    for h in range(1, 24):
        if h * f > 15000:
            break
        x += np.sin(h * ph) * np.exp(-(h - 1) / bright) * (1 if h > 1 else 0.7)
    x += filt(noise(n, rng), f, 6 * f) * 0.03 * a
    return amp * x * a


@sound("sail_fanfare", 2, peak=-2.0, cat="Crowd", radius=150, prio="high",
       trigger="one-shot: a brass fanfare sting (regatta opening/closing, a cup awarded) — three trumpets")
def sail_fanfare(k, rng):
    sec = 3.6
    n = N(sec)
    bb4 = 466.16
    semis = lambda s: bb4 * 2 ** (s / 12)
    x = np.zeros(n)
    if k == 0:  # ta-ta-ta TAAA, then the chord
        seq = [(0.0, 0.11, 0), (0.15, 0.11, 0), (0.3, 0.11, 0), (0.45, 0.55, 7)]
    else:       # rising arpeggio
        seq = [(0.0, 0.18, 0), (0.22, 0.18, 4), (0.44, 0.18, 7), (0.66, 0.55, 12)]
    chord = [0, 4, 7]
    for p in range(3):
        dt = rng.uniform(0, 0.015)
        for s0, L, st in seq:
            x += brass_note(rng, semis(st - (12 if p == 2 else 0)), s0 + dt, L, sec, 0.8 if p == 2 else 1.0)
        x += brass_note(rng, semis(chord[p] + (12 if p == 0 else 0)), 1.15 + dt, 1.6, sec, 0.9)
    x = filt(filt(x, 120, None), None, 5500, 4)
    return space(x, rng, 0.2, 1.4, ((0.09, 0.12), (0.5, 0.08)), 5000), {"seconds": sec, "key": "Bb major"}

# --------------------------------------------------------------------------------------------------------- writing


def seed_of(pool, k):
    return (zlib.crc32(pool.encode()) % 90000) * 10 + 1000 + k


def level_steps(x, w=N(0.05)):
    """For every sample k of a loop: |dB| of the level of the w samples after k against the w before (circular);
    levels floored 40 dB under the file's own, so silence against silence is no step."""
    n = len(x)
    sq = np.concatenate([x[-w:], x, x[:w]]) ** 2
    c = np.concatenate([[0.0], np.cumsum(sq)])
    k = np.arange(w, w + n)
    before = (c[k] - c[k - w]) / w
    after = (c[k + w] - c[k]) / w
    floor = np.mean(x ** 2) * 1e-4 + 1e-12
    return np.abs(10 * np.log10((after + floor) / (before + floor)))


def finish(x, loop, peak_db):
    x = np.asarray(x, dtype=np.float64)
    if loop:
        x = cfilt(x, 25, None, 2)
    else:
        x = filt(x, 25, None, 2)
        x -= np.mean(x)
        a, b = N(0.001), max(N(0.02), min(N(0.15), len(x) // 20))
        x[:a] *= 0.5 - 0.5 * np.cos(np.linspace(0, math.pi, a))
        x[-b:] *= 0.5 + 0.5 * np.cos(np.linspace(0, math.pi, b))
    if loop:
        x -= np.mean(x)
        # the loop is circular, so it may start anywhere: put the seam where the level changes least (no onset on it)
        x = np.roll(x, -int(np.argmin(level_steps(x))))
    pk = np.max(np.abs(x)) or 1.0
    return x / pk * 10 ** (peak_db / 20.0)


def write(entry, k, x, params):
    pool = entry["pool"]
    name = "%s_%02d" % (pool, k + 1)
    seed = seed_of(pool, k)
    x = finish(x, entry["loop"], entry["peak"])
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    rms = math.sqrt(float(np.mean((pcm / 32768.0) ** 2))) or 1e-9
    with open(path, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    side = {"name": name, "asset": "/Game/Audio/Sailing/S_%s" % name, "pool": pool,
            "file": os.path.relpath(path, ROOT), "generator": GEN, "seed": seed,
            "rng": "numpy.random.default_rng (PCG64), numpy %s, scipy %s" % (np.__version__, __import__("scipy").__version__),
            "loop": entry["loop"], "sample_rate": RATE, "bit_depth": 16, "channels": 1,
            "seconds": round(len(pcm) / RATE, 3), "peak_dbfs": entry["peak"], "rms_dbfs": round(20 * math.log10(rms), 2),
            "sha256": sha, "trigger": entry["trigger"], "attenuation_radius_m": entry["radius"],
            "priority": entry["prio"], "params": params,
            "license": "Procedural, made by the project; CC BY 4.0."}
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print("wrote %-44s %6.2f s%s" % (os.path.relpath(path, ROOT), len(pcm) / RATE, "  loop" if entry["loop"] else ""))

# ---------------------------------------------------------------------------------------------------------- checking


def read(path):
    with wave.open(path) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
    return a


def check():
    """Levels, DC, clipping, start/end clicks of one-shots, loop seams (sample jump + level match), a crude spectrum."""
    bad = 0
    for e in REG:
        for k in range(e["n"]):
            name = "%s_%02d" % (e["pool"], k + 1)
            path = os.path.join(OUT, name + ".wav")
            if not os.path.exists(path):
                print("MISSING", name)
                bad += 1
                continue
            x = read(path)
            n = len(x)
            peak = np.max(np.abs(x))
            rms = math.sqrt(np.mean(x ** 2))
            dc = abs(np.mean(x))
            d = np.abs(np.diff(x))
            typ = np.percentile(d, 99.9) + 1e-9
            faults = []
            if not np.all(np.isfinite(x)):
                faults.append("nan")
            if peak > 0.99:
                faults.append("clip")
            if rms < 10 ** (-48 / 20):
                faults.append("too quiet %.1f dB" % (20 * math.log10(rms + 1e-12)))
            if dc > 2e-3:
                faults.append("DC %.4f" % dc)
            if e["loop"]:
                seam = abs(x[0] - x[-1])
                if seam > typ * 1.5:
                    faults.append("seam jump %.4f > %.4f" % (seam, typ))
                # a level step across the seam: the level of the 50 ms before against the 50 ms after, at the seam and
                # at every other point of the loop (circularly); the seam must not stand out from the file's own onsets
                w = N(0.05)
                step = level_steps(x)
                others = np.max(step[w:n - w])
                if step[0] > others + 0.5:
                    faults.append("seam level step %.1f dB > %.1f" % (step[0], others))
            else:
                if abs(x[0]) > 1e-3 or abs(x[-1]) > 1e-3:
                    faults.append("edge click")
            # crude spectrum: centroid and the share below 200 Hz / above 4 kHz
            S = np.abs(np.fft.rfft(x * np.hanning(n))) ** 2
            f = np.fft.rfftfreq(n, 1 / RATE)
            cen = float(np.sum(S * f) / (np.sum(S) + 1e-12))
            lo = float(np.sum(S[f < 200]) / (np.sum(S) + 1e-12))
            hi = float(np.sum(S[f > 4000]) / (np.sum(S) + 1e-12))
            bad += bool(faults)
            print("%-34s %5.2fs %s peak %5.1f rms %6.1f  centroid %5.0f Hz  <200 %3.0f%%  >4k %3.0f%%  %s" % (
                name, n / RATE, "L" if e["loop"] else " ", 20 * math.log10(peak + 1e-12), 20 * math.log10(rms + 1e-12),
                cen, 100 * lo, 100 * hi, "FAULT " + ", ".join(faults) if faults else "ok"))
    print("%d fault(s)" % bad)
    return bad


def table():
    print("| Pool (files) | s | Kind | Trigger | Radius m | Priority |")
    print("|---|---|---|---|---|---|")
    cat = None
    for e in REG:
        if e["cat"] != cat:
            cat = e["cat"]
            print("| **%s** | | | | | |" % cat)
        secs = []
        for k in range(e["n"]):
            p = os.path.join(OUT, "%s_%02d.json" % (e["pool"], k + 1))
            if os.path.exists(p):
                secs.append(json.load(open(p, encoding="utf-8"))["seconds"])
        s = ("%.1f" % secs[0]) if secs and max(secs) - min(secs) < 0.05 else ("%.1f-%.1f" % (min(secs), max(secs)) if secs else "?")
        trig = e["trigger"]
        for pre in ("loop: ", "one-shot: "):
            trig = trig[len(pre):] if trig.startswith(pre) else trig
        print("| `%s` (%d) | %s | %s | %s | %d | %s |" % (e["pool"], e["n"], s, "loop" if e["loop"] else "one-shot", trig,
                                                        e["radius"], e["prio"]))


def main():
    args = sys.argv[1:]
    if "--check" in args:
        sys.exit(1 if check() else 0)
    if "--table" in args:
        table()
        return
    for e in REG:
        if args and not any(w in e["pool"] for w in args):
            continue
        for k in range(e["n"]):
            rng = np.random.default_rng(seed_of(e["pool"], k))
            x, params = e["fn"](k, rng)
            write(e, k, x, params)


if __name__ == "__main__":
    main()
