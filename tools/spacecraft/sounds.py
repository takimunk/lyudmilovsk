#!/usr/bin/env python3
"""The sounds of the flight home in «Ласточка» (docs/spacecraft.md). Procedural and seeded (numpy + scipy; no generation
service, nothing paid, no network), 48 kHz 16-bit mono, each with a .json sidecar (generator, seed, parameters, sha256,
measured levels and checks). Use of every id and the mix: tools/spacecraft/SOUND_NOTES.md.

  python3 tools/spacecraft/sounds.py             write them all, then print the check table
  python3 tools/spacecraft/sounds.py engine hatch only the ones whose id has one of the words
  python3 tools/spacecraft/sounds.py --check      only measure what is on disk

  audio/sfx/spacecraft/sc_<id>_<nn>.wav  ->  /Game/Audio/Spacecraft/S_sc_<id>_<nn>   (loops flagged in the sidecar)

Import on the Mac: IMPORT_ONLY=sfx/spacecraft with unreal/scripts/import_audio.py.

How the loops close: every loop is built on three copies of one period (noise of exactly N samples tiled, modulations
with a whole number of cycles in N, tones on a 1/T grid, events placed in all three copies), all the processing
(filters, saturation, the cabin's reverb) runs over the three, and the middle copy is kept: its end runs into its own
start exactly as the copies did. Several modulations with co-prime cycle counts and smooth periodic random ones keep a
6 s loop from sounding like one.
"""
import hashlib
import json
import math
import os
import sys
import wave

import numpy as np
from scipy import signal

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RATE = 48000
GROUP = "spacecraft"
OUT = os.path.join(ROOT, "audio", "sfx", GROUP)
PEAK_DB = -6.0
TAU = 2 * math.pi


# ------------------------------------------------------------------------------------------------ the toolkit

def t_of(seconds):
    return np.arange(int(round(seconds * RATE))) / RATE


def db(x):
    return 10 ** (x / 20.0)


def norm(x):
    return x / (np.max(np.abs(x)) or 1.0)


def soft(x, drive=1.5):
    """Soft saturation that keeps the level: tanh with the input scaled to its peak."""
    p = np.max(np.abs(x)) or 1.0
    return np.tanh(drive * x / p) / math.tanh(drive) * p


def filt(x, kind, f, order=2):
    return signal.sosfilt(signal.butter(order, f, btype=kind, fs=RATE, output="sos"), x)


def peak(x, f, q, gain=1.0):
    b, a = signal.iirpeak(f, q, fs=RATE)
    return gain * signal.lfilter(b, a, x)


def shaped(n, rng, mag):
    """Noise of exactly n samples shaped in the frequency domain by mag(f): periodic in n, so a loop's own noise."""
    X = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / RATE)
    y = np.fft.irfft(X * mag(f), n)
    return y / (np.std(y) or 1.0)


def bump(f, fc, octaves):
    """A smooth band round fc, `octaves` wide (Gaussian on a log axis)."""
    return np.exp(-0.5 * (np.log2(np.maximum(f, 1.0) / fc) / (octaves / 2.355)) ** 2)


def lp_mag(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order))


def hp_mag(f, fc, order=2):
    return 1.0 / np.sqrt(1.0 + (np.maximum(f, 1e-3) / fc) ** (-2 * order))


def pink_mag(f):
    return 1.0 / np.sqrt(np.maximum(f, 20.0))


def brown_mag(f):
    return 1.0 / np.maximum(f, 15.0)


def smooth_rand(rng, n, hz):
    """A smooth random curve (one-shots), about -1..1, changing at up to `hz`."""
    x = filt(rng.standard_normal(n + RATE // 2), "low", hz, 2)[RATE // 2:]
    return x / (np.max(np.abs(x)) or 1.0)


def env_ad(t, attack, decay):
    return np.minimum(1.0, t / max(attack, 1e-5)) * np.exp(-np.maximum(t - attack, 0) / decay)


def burst(rng, ms, decay_ms=None, lp=None, hp=None):
    """A short noise impulse: the strike of something hard (bright) or soft (with lp)."""
    n = max(8, int(ms * RATE / 1000))
    d = (decay_ms or ms / 4) * RATE / 1000
    x = rng.standard_normal(n) * np.exp(-np.arange(n) / d)
    if lp:
        x = filt(x, "low", lp, 2)
    if hp:
        x = filt(x, "high", hp, 2)
    return x


def mix(*xs):
    """Sum of arrays of different lengths (padded at the end)."""
    n = max(len(x) for x in xs)
    return sum(np.pad(x, (0, n - len(x))) for x in xs)


def modal_ir(modes, seconds, rng=None, scale=1.0):
    """A bank of decaying partials (frequency Hz, decay s, amplitude): the ring of a metal or plastic body."""
    t = t_of(seconds)
    ir = np.zeros_like(t)
    for f, d, a in modes:
        f = f * scale
        if f >= RATE / 2.2:
            continue
        ph = rng.uniform(0, TAU) if rng is not None else 0.0
        ir += a * np.sin(TAU * f * t + ph) * np.exp(-t / d)
    return ir


def ring(exc, modes, seconds, rng=None, scale=1.0):
    return signal.fftconvolve(exc, modal_ir(modes, seconds, rng, scale))


def thump(f0, decay, seconds=None, drop=0.5, drop_s=0.015):
    """A low knock with its pitch falling in the first moment (a body hitting a body)."""
    t = t_of(seconds or decay * 6)
    f = f0 * (1 + drop * np.exp(-t / drop_s))
    return np.sin(TAU * np.cumsum(f) / RATE) * np.minimum(1.0, t / 0.0008) * np.exp(-t / decay)


def bubble(f0, tau, rise=0.3, seconds=None):
    """A water bubble's ring (Minnaert): a sine whose pitch rises as it reaches the surface."""
    t = t_of(seconds or tau * 6)
    f = f0 * (1 + rise * t / (tau * 3))
    return np.sin(TAU * np.cumsum(f) / RATE) * np.exp(-t / tau) * np.minimum(1.0, t / 0.0005)


def stickslip(rng, rate, amp=None, jitter=0.15):
    """Friction's pulse train (creaks, groans, threads): one pulse each time the phase of `rate` (Hz curve) wraps."""
    r = rate * (1 + jitter * smooth_rand(rng, len(rate), 40.0))
    ph = np.cumsum(np.maximum(r, 0)) / RATE
    idx = np.nonzero(np.diff(np.floor(ph)) > 0)[0]
    x = np.zeros(len(rate))
    a = amp if amp is not None else np.ones(len(rate))
    x[idx] = a[idx] * rng.uniform(0.55, 1.0, len(idx)) * rng.choice([-1, 1], len(idx))
    return x


def tv_band(rng, n, fc, octaves=1.0, nper=512):
    """Noise through a band that moves (fc: a centre frequency per sample): moving formants, sweeps."""
    w = rng.standard_normal(n)
    f, tt, Z = signal.stft(w, RATE, nperseg=nper)
    centres = np.interp(tt * RATE, np.arange(n), fc)
    G = bump(f[:, None], centres[None, :], octaves)
    _, y = signal.istft(Z * G, RATE, nperseg=nper)
    y = y[:n]
    if len(y) < n:
        y = np.pad(y, (0, n - len(y)))
    return y / (np.std(y) or 1.0)


def hull(x, bright=1.0, lowcut=2200):
    """Heard through the capsule's wall: the hull's low panel modes and little above a couple of kHz."""
    y = filt(x, "low", lowcut * bright, 2)
    for f, q, g in ((38, 4, 0.9), (61, 5, 0.8), (87, 6, 0.7), (124, 7, 0.55), (171, 8, 0.45), (240, 8, 0.35), (333, 9, 0.25)):
        y = y + peak(x, f, q, g)
    return y


def speaker(x, lo=150, hi=7000, drive=1.4):
    """The record player's little speaker (and the radio's): a band and a little drive."""
    return soft(filt(x, "band", [lo, hi], 2), drive)


def make_room(seed=9000, seconds=0.34):
    """The one small metal cabin everything shares: 2.6 m across, 2 m high, quilted walls (highs die fast), a shell
    that rings a little. Early reflections from those distances, then a diffuse tail decaying per band."""
    rng = np.random.default_rng(seed)
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    ir = np.zeros(n)
    for ms, g in ((3.1, 0.55), (5.9, -0.5), (7.6, 0.45), (9.8, -0.36), (12.4, 0.3), (15.1, -0.26), (18.7, 0.2),
                  (22.3, -0.16), (26.0, 0.13), (31.5, -0.1)):
        ir[int(ms * RATE / 1000)] += g
    ir = filt(ir, "low", 6500, 1)
    for lo, hi, rt in ((20, 500, 0.36), (500, 2000, 0.26), (2000, 6000, 0.15), (6000, 20000, 0.07)):
        band = filt(rng.standard_normal(n), "band", [lo, hi], 2)
        ir += 0.11 * band * np.exp(-6.91 * t / rt) * np.minimum(1.0, t / 0.004)
    shell = ((212, 0.22, 1.0), (338, 0.2, 0.8), (471, 0.17, 0.7), (689, 0.14, 0.5), (1021, 0.11, 0.4), (1433, 0.08, 0.3))
    ir += 0.004 * modal_ir(shell, seconds, rng)
    return ir / math.sqrt(np.sum(ir ** 2))


ROOM = make_room()


def room(x, wet=0.3, keep_length=False):
    """The cabin's reverb on x (equal-energy dry/wet). keep_length: for loops (cut to len(x), the tiles carry the tail)."""
    y = signal.fftconvolve(x, ROOM)
    if keep_length:
        y = y[:len(x)]
        d = x
    else:
        y = y[:len(x) + int(0.2 * RATE)]           # a one-shot keeps 0.2 s of the tail (it is faded out after)
        d = np.pad(x, (0, len(y) - len(x)))
    return math.sqrt(1 - wet) * d + math.sqrt(wet) * 1.4 * y


class Canvas:
    """A one-shot's timeline."""

    def __init__(self, seconds, seed):
        self.x = np.zeros(int(round(seconds * RATE)))
        self.rng = np.random.default_rng(seed)
        self.t = np.arange(len(self.x)) / RATE

    def add(self, s, at=0.0, gain=1.0):
        i = int(round(at * RATE))
        n = min(len(s), len(self.x) - i)
        if n > 0 and i >= 0:
            self.x[i:i + n] += gain * s[:n]


class Loop:
    """A loop's timeline: three copies of one period of N samples (see the module's notes)."""

    def __init__(self, seconds, seed):
        self.N = int(round(seconds * RATE))
        self.T = self.N / RATE
        self.n = 3 * self.N
        self.t = np.arange(self.n) / RATE
        self.rng = np.random.default_rng(seed)
        self.x = np.zeros(self.n)

    def q(self, f):
        """A frequency on the loop's grid (a whole number of cycles in T)."""
        return round(f * self.T) / self.T

    def tile(self, y):
        return np.tile(y, 3)

    def noise(self, mag=None):
        return self.tile(shaped(self.N, self.rng, mag or (lambda f: np.ones_like(f))))

    def lfo(self, cycles, phase=0.0):
        return np.sin(TAU * cycles * self.t / self.T + phase)

    def wander(self, max_cycles, min_cycles=1):
        """A smooth periodic random curve, -1..1, with whole-cycle components only (no mean)."""
        k = np.arange(min_cycles, max_cycles + 1)
        y = np.zeros(self.n)
        for c in k:
            y += (1.0 / c) * np.sin(TAU * c * self.t / self.T + self.rng.uniform(0, TAU))
        return y / (np.max(np.abs(y)) or 1.0)

    def phase(self, f, dev=0.0, wobble=None):
        """A tone's phase on the grid, its pitch wobbling by dev Hz along a zero-mean periodic curve."""
        ph = TAU * self.q(f) * self.t
        if wobble is not None and dev:
            w = wobble - np.mean(wobble[:self.N])
            ph = ph + TAU * np.cumsum(dev * w) / RATE
        return ph

    def add(self, s, at, gain=1.0):
        """An event at `at` (0..T) in every copy."""
        for k in range(3):
            i = int(round((at % self.T + k * self.T) * RATE))
            n = min(len(s), self.n - i)
            if n > 0:
                self.x[i:i + n] += gain * s[:n]

    def out(self, y):
        return y[self.N:2 * self.N]


# ------------------------------------------------------------------------------------------------ materials

def modes(spec, amp_fall=0.85):
    """(f, decay) pairs -> (f, decay, amplitude) with amplitudes falling up the series."""
    return [(f, d, amp_fall ** i) for i, (f, d) in enumerate(spec)]


HULL = modes([(47, .9), (73, .7), (112, .55), (158, .5), (231, .4), (317, .33), (446, .25), (602, .2), (811, .15),
              (1093, .12), (1477, .09), (1990, .07), (2710, .05)], 0.82)
# the hull struck hard (bangs, the splash, the chute's snatch): the same panels, but a sharp blow rings the middle modes
# most and the low ones decay sooner, so the event reads on small speakers too, not only as sub-bass
HULL_STRIKE = [(f, d * 0.6, math.exp(-0.5 * (math.log2(f / 520) / 1.3) ** 2)) for f, d, _ in HULL]
HATCH = modes([(118, .55), (196, .5), (287, .45), (403, .38), (561, .3), (744, .25), (988, .2), (1310, .16), (1720, .12),
               (2290, .09), (3010, .06)], 0.86)
PANEL = modes([(1180, .04), (2150, .025), (3420, .018), (5610, .012), (7900, .008)], 0.8)
PANEL_BODY = modes([(160, .03), (420, .025), (690, .02)], 0.7)
SMALL = modes([(1450, .05), (2380, .04), (3600, .025), (5200, .015)], 0.8)
RELAY = modes([(3300, .006), (4700, .005), (6800, .004), (9200, .003)], 0.85)
PLASTIC = modes([(1240, .012), (2630, .009), (4100, .006)], 0.75)
PLASTIC_LOW = modes([(780, .02), (1650, .014), (2900, .008)], 0.75)
BUCKLE = modes([(1720, .04), (2950, .03), (4480, .025), (6900, .015)], 0.9)
HINGE = modes([(520, .03), (890, .025), (1340, .02), (2050, .015)], 0.85)
GLASS = modes([(2700, .01), (4300, .008), (6100, .006), (8800, .004)], 0.85)
PIPE = modes([(2400, .04), (3900, .03), (6100, .02)], 0.8)
LEATHER = modes([(380, .03), (720, .02), (1150, .015)], 0.75)


# ------------------------------------------------------------------------------------------------ cabin beds (loops)

def fan_voice(L, rps, blades, noise_fc, level=1.0, detail=1.0):
    """A small cabin fan: the blade-pass tone and its harmonics, chopped broadband air, a slow drift in speed."""
    rot = L.q(rps)
    bpf = rot * blades
    drift = L.wander(3)
    ph = L.phase(bpf, dev=bpf * 0.006, wobble=drift)
    tone = 0.30 * np.sin(ph) + 0.12 * np.sin(2 * ph + 0.7) + 0.05 * np.sin(3 * ph + 1.9)
    shaft = 1 + 0.12 * np.sin(L.phase(rot, dev=rot * 0.006, wobble=drift))
    air = L.noise(lambda f: bump(f, noise_fc, 2.2) * pink_mag(f) * 30)
    chop = 1 + 0.22 * np.sin(ph) * detail
    y = level * (0.55 * tone * shaft + air * chop * shaft * 0.9)
    if detail > 1:
        # motor: electrical whine and a bearing's faint rasp
        y += 0.03 * np.sin(L.phase(rot * 52, dev=rot * 0.3, wobble=drift))
        rasp = L.noise(lambda f: bump(f, 6500, 1.0)) * (1 + 0.5 * np.sin(L.phase(rot)))
        y += 0.05 * rasp
    return y


def cabin_hum(seed):
    L = Loop(6.0, seed)
    # 400 Hz aircraft electrics from two inverters a hair apart (beating), low in the mix
    elec = np.zeros(L.n)
    for k, a in ((1, .25), (2, .12), (3, .06), (5, .02)):
        elec += a * (np.sin(L.phase(400.5 * k)) + np.sin(L.phase(399.5 * k) + k))
    elec = filt(elec, "band", [150, 3000], 1) * 0.35
    fans = fan_voice(L, 24.0, 7, 900, 1.0) + fan_voice(L, 31.0, 5, 1400, 0.8)
    rumble = L.noise(lambda f: lp_mag(f, 120, 2) * brown_mag(f) * 60) * (0.8 + 0.2 * L.wander(4))
    y = elec + fans * 0.6 + 0.25 * rumble
    y = filt(y, "low", 7000, 2)
    y = soft(y, 1.5)
    y = room(y, 0.35, keep_length=True)
    return L.out(y), {"electrics_hz": [399.5, 400.5], "fans": [{"rps": 24, "blades": 7}, {"rps": 31, "blades": 5}]}


def fan(seed):
    L = Loop(6.0, seed)
    y = fan_voice(L, 26.5, 7, 1100, 1.0, detail=2.0)
    y = filt(y, "low", 9000, 2)
    y = room(soft(y, 1.3), 0.2, keep_length=True)
    return L.out(y), {"rps": 26.5, "blades": 7, "bpf_hz": 185.5}


def gyro_whine(seed):
    L = Loop(6.0, seed)
    wob = L.wander(3)
    y = np.zeros(L.n)
    for f0, a in ((1600, 1.0), (1602, 0.8)):            # two gyros 2 Hz apart: a slow throb
        ph = L.phase(f0, dev=1.5, wobble=wob)
        y += a * (np.sin(ph) + 0.35 * np.sin(2 * ph + 1.1) + 0.12 * np.sin(0.5 * ph))
        for side in (-400, 400):                           # the rotor's own rate as sidebands
            y += a * 0.08 * np.sin(L.phase(f0 + side, dev=1.5, wobble=wob))
    bearings = L.noise(lambda f: bump(f, 5500, 1.2)) * (1 + 0.3 * L.wander(20))
    y = 0.12 * y + 0.05 * bearings
    y = room(soft(y, 1.2), 0.3, keep_length=True)
    return L.out(y), {"whine_hz": [1600, 1602], "rotor_hz": 400}


def gyro_spinup(seed):
    C = Canvas(4.0, seed)
    t, rng = C.t, C.rng
    F = 1600 / (1 - math.exp(-4.0 / 1.1))
    f = F * (1 - np.exp(-t / 1.1))
    a = (f / 1600) ** 1.5
    y = np.zeros_like(t)
    for f0, amp in ((1.0, 1.0), (1602 / 1600, 0.8)):
        ph = TAU * np.cumsum(f * f0) / RATE
        y += amp * (np.sin(ph) + 0.35 * np.sin(2 * ph + 1.1) + 0.12 * np.sin(0.5 * ph))
        ph_l = TAU * np.cumsum(np.maximum(f * f0 - 400 * (f / 1600), 1)) / RATE
        ph_h = TAU * np.cumsum(f * f0 + 400 * (f / 1600)) / RATE
        y += amp * 0.08 * (np.sin(ph_l) + np.sin(ph_h))
    y *= 0.12 * a
    # the motor's growl at the start: the rotor's own rate rising out of a hum, fading as it speeds up
    rot = f / 4
    growl = np.sin(TAU * np.cumsum(rot) / RATE) + 0.5 * np.sin(TAU * np.cumsum(2 * rot) / RATE)
    y += 0.05 * growl * np.exp(-t / 1.2) * np.minimum(1, t / 0.05)
    bearings = filt(rng.standard_normal(len(t)), "band", [3500, 8000], 2) * 0.04 * a ** 1.3
    y += bearings
    C.add(y)
    C.add(ring(burst(rng, 0.6), RELAY, 0.05, rng), 0.0, 0.25)       # the gyro relay
    C.add(thump(150, 0.02), 0.0, 0.12)
    return room(soft(C.x, 1.2), 0.3)[:len(C.x)], {"seconds": 4, "to_hz": 1600, "tau_s": 1.1}


def air_hiss(seed):
    L = Loop(6.0, seed)
    hiss = L.noise(lambda f: bump(f, 3500, 1.6) * 0.6 + bump(f, 700, 1.3) * 0.5 * lp_mag(f, 10000))
    breathe = 1 + 0.18 * L.wander(4) + 0.06 * L.lfo(5, 0.3) + 0.04 * L.lfo(7, 1.2)
    flutter_band = L.noise(lambda f: bump(f, 1800, 0.6))
    y = hiss * breathe + 0.15 * flutter_band * (0.5 + 0.5 * L.lfo(69))
    # the regenerator's valve breathing out now and then (three uneven puffs)
    for at, g, d in ((0.7, 0.5, 0.35), (2.9, 0.8, 0.5), (4.6, 0.35, 0.28)):
        n = int(d * 3 * RATE)
        tt = np.arange(n) / RATE
        puff = filt(L.rng.standard_normal(n), "band", [600, 5000], 2) * np.sin(np.pi * np.minimum(tt / (d * 3), 1)) ** 2
        L.add(puff, at, g)
    y = y + L.x
    y = room(filt(y, "high", 120, 2), 0.3, keep_length=True)
    return L.out(y), {"bands_hz": [700, 3500], "puffs_at_s": [0.7, 2.9, 4.6]}


def radio_static(seed):
    L = Loop(6.0, seed)
    hiss = L.noise(lambda f: bump(f, 1800, 3.0) * hp_mag(f, 300, 2) * lp_mag(f, 3400, 3))
    fading = 0.65 + 0.35 * L.wander(3)
    y = hiss * fading * 0.5
    for f0, dev, c, ph, g in ((1450, 260, 1, 0.3, 0.10), (2380, 410, 2, 2.0, 0.06), (870, 120, 1, 4.1, 0.05)):
        f = f0 + dev * np.sin(TAU * c * L.t / L.T + ph)
        phi = TAU * np.cumsum(f) / RATE
        # make the phase periodic: remove the per-period drift (mean frequency on the grid)
        drift = TAU * (np.mean(f[:L.N]) - L.q(np.mean(f[:L.N]))) * L.t
        env = (0.5 + 0.5 * np.sin(TAU * c * L.t / L.T + ph + 1.3)) ** 4
        y += g * np.sin(phi - drift) * env
    for _ in range(30):
        at = L.rng.uniform(0, L.T)
        m = int(L.rng.uniform(0.0005, 0.003) * RATE)
        L.add(L.rng.standard_normal(m) * np.exp(-np.arange(m) / (m / 4)), at, L.rng.uniform(0.2, 0.9))
    y = speaker(y + L.x, 300, 3400, 1.8)
    y = room(y, 0.25, keep_length=True)
    return L.out(y), {"band_hz": [300, 3400], "whistles_hz": [1450, 2380, 870], "crackles": 30}


def beacon(seed):
    L = Loop(6.0, seed)
    # a far telemetry beacon: alternating 0.3 s beeps of two pitches, like Sputnik, its carrier drifting slowly
    drift = 4.0 * L.lfo(1, 0.4)
    for k in range(5):
        for j, f0 in enumerate((1000.0, 750.0)):
            at = k * 1.2 + j * 0.6
            n = int(0.3 * RATE)
            tt = np.arange(n) / RATE
            i0 = int(at * RATE) + L.N
            f = f0 + drift[i0:i0 + n]
            ph = TAU * np.cumsum(f) / RATE
            tone = np.sin(ph) + 0.3 * np.sin(3 * ph) + 0.12 * np.sin(5 * ph)
            ramp = np.minimum(1.0, np.minimum(tt, 0.3 - tt) / 0.008)
            L.add(tone * ramp, at, 0.5 * (0.92 + 0.08 * L.rng.uniform()))
    fading = 0.75 + 0.25 * L.wander(2)
    hiss = L.noise(lambda f: bump(f, 1600, 2.5)) * 0.06
    y = speaker((L.x + hiss) * fading, 300, 3400, 1.5)
    y = room(y, 0.2, keep_length=True)
    return L.out(y), {"tones_hz": [1000, 750], "beep_s": 0.3, "pattern_s": 1.2}


# ------------------------------------------------------------------------------------------------ controls

def toggle(seed, v):
    C = Canvas(0.3, seed)
    r = C.rng
    s = 1 + 0.04 * r.normal()
    C.add(ring(burst(r, 0.6), PANEL, 0.12, r, s), 0.004, 1.0)
    C.add(ring(burst(r, 0.8, lp=3000), PANEL_BODY, 0.12, r, s), 0.004, 0.9)
    C.add(thump(160 * s, 0.02), 0.004, 0.45)
    d = 0.004 + r.uniform(0.014, 0.026)
    C.add(ring(burst(r, 0.5), PANEL, 0.1, r, s * 1.15), d, 0.45)
    C.add(modal_ir([(900 * s, 0.06, 1.0)], 0.2), 0.006, 0.03)        # the lever's spring
    return room(C.x, 0.3), {"snap_s": 0.004, "detent_s": round(d, 4), "scale": round(s, 3)}


def cover(seed, v):
    C = Canvas(0.35, seed)
    r = C.rng
    s = 1 + 0.05 * r.normal()
    C.add(ring(burst(r, 0.5, lp=8000), PLASTIC, 0.06, r, s), 0.003, 0.5)
    hit = r.uniform(0.06, 0.09)
    C.add(thump(320 * s, 0.015), hit, 0.6)
    C.add(ring(burst(r, 1.0, lp=5000), PLASTIC_LOW, 0.1, r, s), hit, 1.0)
    t = t_of(0.3)
    boing = np.sin(TAU * 640 * s * t + 3 * np.sin(TAU * 7 * t) * np.exp(-t / 0.05)) * np.exp(-t / 0.08)
    C.add(boing, hit, 0.05)
    return room(C.x, 0.3), {"stop_s": round(hit, 3), "scale": round(s, 3)}


def button(seed, v):
    C = Canvas(0.45, seed)
    r = C.rng
    s = 1 + 0.04 * r.normal()
    t = t_of(0.03)
    C.add(filt(r.standard_normal(len(t)), "band", [2000, 6000], 2) * (t / 0.03), 0.0, 0.08)
    C.add(thump(105 * s, 0.045, drop=0.6), 0.03, 1.0)
    C.add(ring(burst(r, 1.5, lp=6000), modes([(610, .06), (1130, .045), (1890, .03), (2760, .02), (4300, .012)]), 0.3, r, s),
          0.03, 0.6)
    C.add(ring(burst(r, 0.4), RELAY, 0.05, r, s), 0.036, 0.3)
    return room(C.x, 0.35), {"clunk_hz": round(105 * s, 1)}


def relay(seed, v):
    C = Canvas(0.3, seed)
    r = C.rng
    s = 1 + 0.05 * r.normal()

    def pull(at, g):
        C.add(thump(240 * s, 0.012), at, 0.35 * g)
        tt = at + 0.004
        for k, a in enumerate((1.0, 0.55, 0.3, 0.18)[:r.integers(3, 5)]):
            C.add(ring(burst(r, 0.3), RELAY, 0.04, r, s * (1 + 0.03 * k)), tt, a * g)
            tt += r.uniform(0.0015, 0.004) * (1 - 0.15 * k)
        C.add(ring(burst(r, 0.3), RELAY, 0.04, r, s * 1.2), at + 0.012, 0.35 * g)

    pull(0.0, 1.0)
    if v == 2:
        pull(0.04 + r.uniform(0, 0.02), 0.7)         # a slave relay after it
    return room(C.x, 0.3), {"stages": 2 if v == 2 else 1, "scale": round(s, 3)}


def buzzer(seed, v):
    C = Canvas(0.5, seed)
    r = C.rng
    t = C.t
    f = 180 * (1 + 0.004 * smooth_rand(r, len(t), 30))
    ph = np.cumsum(f) / RATE
    saw = 2 * (ph - np.floor(ph)) - 1
    y = filt(saw, "high", 120, 1)
    y = sum(peak(y, fc, q, g) for fc, q, g in ((900, 8, 1.0), (2100, 6, 0.6), (3400, 5, 0.35))) + 0.3 * y
    y *= 1 + 0.25 * smooth_rand(r, len(t), 35)
    gate = np.zeros_like(t)
    spans = [(0.0, 0.42)] if v == 1 else [(0.0, 0.16), (0.24, 0.40)]
    for a, b in spans:
        gate += np.clip(np.minimum((t - a) / 0.004, (b - t) / 0.02), 0, 1)
    C.add(soft(y, 2.0) * gate)
    return room(C.x, 0.25), {"hz": 180, "pulses": len(spans)}


def bell_tone(f, seconds, decay):
    t = t_of(seconds)
    x = sum(a * np.sin(TAU * f * k * t) * np.exp(-t / (decay * d)) for k, a, d in ((1, 1, 1), (2.76, .3, .4), (5.4, .1, .2), (2, .06, .7)))
    return x * np.minimum(1, t / 0.003)


def chime(seed, v):
    C = Canvas(0.8, seed)
    notes = [(0.0, 1046.5), (0.16, 784.0)] if v == 1 else [(0.0, 659.3), (0.09, 784.0), (0.18, 1046.5)]
    for at, f in notes:
        C.add(bell_tone(f, 0.7, 0.28), at, 0.6)
    return room(speaker(C.x, 400, 5000, 1.3), 0.3), {"notes_hz": [n[1] for n in notes]}


def lamp_tick(seed, v):
    C = Canvas(0.12, seed)
    r = C.rng
    s = 1 + 0.08 * r.normal()
    C.add(ring(burst(r, 0.4, hp=3000), modes([(5200, .004), (8100, .003), (11000, .002)]), 0.03, r, s), 0.002, 1.0)
    C.add(thump(600 * s, 0.003), 0.002, 0.3)
    return room(C.x, 0.15), {"scale": round(s, 3)}


def hatch_close(seed, v):
    C = Canvas(1.2, seed)
    r = C.rng
    s = 1 + 0.03 * r.normal()
    t = t_of(0.2)
    C.add(filt(r.standard_normal(len(t)), "band", [100, 500], 2) * (t / 0.2) ** 2, 0.0, 0.25)
    hit = 0.18
    C.add(ring(burst(r, 3.0, lp=4000), HATCH, 1.0, r, s), hit, 1.0)
    C.add(thump(70, 0.06), hit, 0.9)
    C.add(burst(r, 50, 15, lp=600), hit, 0.3)
    at = hit + r.uniform(0.32, 0.4)
    for k in range(3):
        C.add(thump(180 * s, 0.02), at, 0.4)
        C.add(ring(burst(r, 0.8), SMALL, 0.2, r, s * (1 + 0.05 * k)), at, 0.45)
        C.add(ring(burst(r, 1.0, lp=3000), HATCH, 0.4, r, s), at, 0.15)
        at += r.uniform(0.1, 0.15)
    return room(soft(C.x, 1.3), 0.4), {"clang_s": hit, "dogs": 3, "scale": round(s, 3)}


def hatch_open(seed, v):
    C = Canvas(1.3, seed)
    r = C.rng
    s = 1 + 0.03 * r.normal()
    for at in (0.0, r.uniform(0.07, 0.1)):
        C.add(thump(170 * s, 0.02), at, 0.4)
        C.add(ring(burst(r, 0.8), SMALL, 0.2, r, s), at, 0.4)
    rel = 0.17
    C.add(burst(r, 150, 40, hp=2000), rel, 0.2)               # the seal lets go: a breath of air
    C.add(thump(90, 0.04, drop=0.3), rel, 0.5)
    C.add(burst(r, 30, 10, lp=900), rel, 0.4)
    n = int(0.8 * RATE)
    tt = np.arange(n) / RATE
    rate = 35 + 35 * np.sin(np.pi * tt / 0.8) ** 1.5
    amp = np.sin(np.pi * tt / 0.8) ** 0.8
    p = stickslip(r, rate, amp, 0.2)
    C.add(ring(p, HINGE, 0.12, r, s), 0.25, 0.35)
    C.add(ring(p, HATCH, 0.4, r, s), 0.25, 0.035)
    C.add(thump(130, 0.04), 1.06, 0.5)
    C.add(ring(burst(r, 2.0, lp=3000), HATCH, 0.6, r, s), 1.06, 0.4)
    return room(C.x, 0.4), {"seal_s": rel, "groan_s": [0.25, 1.05], "scale": round(s, 3)}


def wheel(seed, v):
    C = Canvas(0.7, seed)
    r = C.rng
    s = 1 + 0.05 * r.normal()
    at = 0.02
    while at < 0.6:
        C.add(ring(burst(r, 0.4), modes([(2100, .02), (3300, .015), (5100, .01)]), 0.08, r, s * r.uniform(0.97, 1.03)),
              at, r.uniform(0.4, 0.7))
        at += r.uniform(0.055, 0.085)
    n = int(0.62 * RATE)
    tt = np.arange(n) / RATE
    env = np.sin(np.pi * tt / 0.62)
    p = stickslip(r, 140 + 40 * np.sin(TAU * 2.5 * tt), env, 0.3)
    C.add(ring(p, modes([(740, .02), (1280, .015), (1900, .01)]), 0.06, r, s), 0.02, 0.25)
    C.add(thump(110, 0.05), 0.02, 0.2)
    return room(C.x, 0.35), {"scale": round(s, 3)}


def belt(seed, v):
    C = Canvas(0.35, seed)
    r = C.rng
    s = 1 + 0.04 * r.normal()
    t = t_of(0.12)
    C.add(filt(r.standard_normal(len(t)), "band", [1500, 6000], 2) * np.sin(np.pi * t / 0.12), 0.0, 0.2)
    t = t_of(0.06)
    C.add(filt(r.standard_normal(len(t)), "band", [3000, 9000], 2) * (t / 0.06), 0.1, 0.12)
    for at, g in ((0.16, 1.0), (0.16 + r.uniform(0.009, 0.016), 0.6)):
        C.add(ring(burst(r, 0.5), BUCKLE, 0.15, r, s), at, g)
    return room(C.x, 0.3), {"scale": round(s, 3)}


def seat(seed, v):
    C = Canvas(0.6, seed)
    r = C.rng
    C.add(thump(75 * r.uniform(0.9, 1.1), 0.09, drop=0.2), 0.02, 1.0)
    t = t_of(0.3)
    C.add(filt(r.standard_normal(len(t)), "low", 400, 2) * env_ad(t, 0.02, 0.1), 0.01, 0.6)
    n = int(0.35 * RATE)
    tt = np.arange(n) / RATE
    p = stickslip(r, 25 + 35 * np.sin(np.pi * tt / 0.35), np.sin(np.pi * tt / 0.35), 0.25)
    C.add(ring(p, LEATHER, 0.08, r, r.uniform(0.9, 1.1)), 0.06, 0.35)
    C.add(ring(p[::2], modes([(1450, .03), (2300, .02)]), 0.06, r), 0.1, 0.05)    # the couch frame
    return room(C.x, 0.35), {}


# ------------------------------------------------------------------------------------------------ engine and flight

def burn_core(t, noise, flick, slow, jit, f_tone=72.0, tone_ph=None):
    """The main engine through the hull: a deep resonant rumble, rough combustion flicker, a tonal throb, pump grit."""
    deep = hull(filt(noise[0], "low", 300, 2), lowcut=900)
    deep = deep * (1 + 0.25 * flick)
    if tone_ph is None:
        tone_ph = TAU * np.cumsum(f_tone + 4.0 * jit) / RATE
    tone = (np.sin(tone_ph) + 0.4 * np.sin(2 * tone_ph + 0.5)) * (0.8 + 0.2 * slow)
    grit = filt(noise[1], "band", [350, 1400], 2) * (1 + 0.5 * flick)
    sub = np.sin(TAU * 25.0 * t + 0.6 * slow)
    body = filt(noise[1], "band", [120, 500], 2) * (1 + 0.4 * flick)          # the roar of the chamber in the frame
    y = (0.7 * deep / (np.std(deep) or 1) + 0.25 * tone + 0.3 * grit / (np.std(grit) or 1) + 0.06 * sub
         + 0.45 * body / (np.std(body) or 1))
    return filt(y, "low", 2500, 2)


def engine(seed):
    L = Loop(6.0, seed)
    nz = (L.noise(brown_mag), L.noise())
    flick, slow, jit = L.wander(60, 4), L.wander(4), L.wander(30, 2)
    tone_ph = L.phase(72.0, dev=4.0, wobble=jit)
    y = burn_core(L.t, nz, flick, slow, jit, tone_ph=tone_ph)
    y = soft(y, 2.0)
    y = room(y, 0.2, keep_length=True)
    return L.out(y), {"hull_modes_hz": [38, 61, 87, 124, 171, 240, 333], "throb_hz": 72, "sub_hz": 25}


def burn_oneshot(C, n, f_tone):
    r = C.rng
    nz = (filt(r.standard_normal(n), "low", 400, 1) * 8, r.standard_normal(n))
    t = np.arange(n) / RATE
    return burn_core(t, nz, smooth_rand(r, n, 10), smooth_rand(r, n, 0.7), smooth_rand(r, n, 5), f_tone)


def ignition(seed):
    C = Canvas(3.0, seed)
    r = C.rng
    C.add(thump(140, 0.03), 0.0, 0.35)
    C.add(ring(burst(r, 1.0), PANEL, 0.1, r), 0.0, 0.3)
    for at in (0.15, 0.22, 0.27):
        C.add(ring(burst(r, 0.5), HULL_STRIKE, 0.3, r), at, 0.3)
    C.add(thump(45, 0.25, drop=1.0, drop_s=0.03), 0.35, 0.5)
    C.add(ring(burst(r, 4.0, lp=2000), HULL_STRIKE, 1.0, r), 0.35, 0.22)
    n = len(C.x) - int(0.35 * RATE)
    t = np.arange(n) / RATE
    f_tone = 50 + 22 * (1 - np.exp(-t / 0.5))
    b = burn_oneshot(C, n, f_tone)
    rise = (1 - np.exp(-t / 0.45)) * (1 + 0.35 * np.sin(TAU * 8 * t) * np.exp(-t / 0.4))
    C.add(b * rise / (np.max(np.abs(b)) or 1), 0.35, 1.1)
    y = soft(C.x, 1.8)
    return room(y, 0.2)[:len(C.x)], {"igniters_s": [0.15, 0.22, 0.27], "light_s": 0.35, "ends": "at full burn (cross-fade into sc_engine)"}


def cutoff(seed):
    C = Canvas(1.6, seed)
    r = C.rng
    n = int(0.9 * RATE)
    t = np.arange(n) / RATE
    env = np.where(t < 0.12, 1.0, np.exp(-(t - 0.12) / 0.12))
    b = burn_oneshot(C, n, 72 - 30 * np.clip((t - 0.12) / 0.4, 0, 1))
    b = filt(b, "low", 900, 1) * 0.4 + b * 0.6 * np.sqrt(env)
    C.add(b * env / (np.max(np.abs(b)) or 1), 0.0, 1.0)
    C.add(thump(140, 0.03), 0.1, 0.4)
    C.add(ring(burst(r, 1.0), PANEL, 0.1, r), 0.1, 0.35)
    C.add(burst(r, 600, 220, hp=1000, lp=4000), 0.15, 0.08)
    at = 0.35
    while at < 1.5:
        C.add(ring(burst(r, 0.3), PIPE, 0.15, r, r.uniform(0.8, 1.3)), at, r.uniform(0.12, 0.3))
        at += r.uniform(0.07, 0.25) * (1 + at)
    for _ in range(4):
        C.add(bubble(r.uniform(200, 400), 0.02, 0.2), r.uniform(0.3, 0.8), 0.08)
    return room(C.x, 0.3), {"dies_s": 0.12, "ticks": "thermal, slowing"}


def thruster(seed, v):
    C = Canvas(0.35, seed)
    r = C.rng
    low = {1: 1.0, 2: 0.8, 3: 1.1}[v]
    puffs = [0.0, 0.09] if v == 3 else [0.0]
    for k, at in enumerate(puffs):
        g = 1.0 if k == 0 else 0.75
        C.add(ring(burst(r, 0.4), SMALL, 0.05, r, 1.3), at, 0.25 * g)
        d = (0.07 if v == 2 else 0.045) * r.uniform(0.9, 1.1)
        t = t_of(d * 3)
        puff = r.standard_normal(len(t)) * env_ad(t, 0.003, d)
        C.add(hull(puff, lowcut=900) * 0.8, at, g)
        C.add(thump(75 * low, 0.04, drop=0.4), at, 0.6 * g)
        t6 = t_of(d * 6)
        C.add(filt(r.standard_normal(len(t6)), "low", 1200, 2) * env_ad(t6, 0.005, 0.06), at, 0.1 * g)
    return room(C.x, 0.25), {"puffs": len(puffs)}


def sep_bang(seed):
    C = Canvas(1.3, seed)
    r = C.rng
    for at, g in ((0.01, 1.0), (0.01 + 0.075, 0.85)):
        exc = mix(burst(r, 0.4), 0.5 * burst(r, 4.0, lp=3000))
        C.add(ring(exc, HULL_STRIKE, 1.2, r), at, g)
        C.add(ring(exc, PANEL, 0.2, r, 0.7), at, 0.25 * g)
        C.add(thump(55, 0.12, drop=1.0), at, 0.35 * g)
        C.add(burst(r, 6, 1.5, hp=500, lp=6000), at, 0.8 * g)
    for at in (0.42, 0.71):
        C.add(ring(burst(r, 0.3), SMALL, 0.2, r, r.uniform(1.2, 1.6)), at, 0.05)
    return room(soft(C.x, 1.6), 0.35), {"bangs_s": [0.01, 0.085]}


def creak(seed, v):
    C = Canvas(0.6, seed)
    r = C.rng
    dur = r.uniform(0.35, 0.52)
    n = int(dur * RATE)
    tt = np.arange(n) / RATE
    r0, r1 = r.uniform(20, 40), r.uniform(40, 150)
    rate = r0 + r1 * np.sin(np.pi * tt / dur) ** r.uniform(0.7, 2.0)
    amp = np.sin(np.pi * tt / dur) ** 0.7
    p = stickslip(r, rate, amp, 0.2)
    m = [(r.uniform(240, 420) * k ** 0.9, r.uniform(0.03, 0.08), 0.8 ** k) for k in range(1, 6)]
    C.add(ring(p, m, 0.15, r), 0.02, 1.0)
    C.add(ring(p, modes([(110, .1), (170, .08)]), 0.3, r), 0.02, 0.15)
    if v >= 3:
        C.add(ring(burst(r, 0.3), PIPE, 0.1, r, r.uniform(0.9, 1.2)), 0.02 + dur, 0.25)
    return room(C.x, 0.35), {"rate_hz": [round(r0), round(r0 + r1)], "seconds": round(dur, 2)}


def rattle(seed):
    L = Loop(6.0, seed)
    r = L.rng
    objs = [modes([(1800, .012), (3100, .008), (4700, .005)]),        # a pencil in the net
            modes([(2900, .03), (4400, .025), (6600, .015)]),         # the harness carabiner
            modes([(1200, .05), (2300, .035), (3900, .02)]),          # the thermos cap
            modes([(5000, .01), (7000, .008)]),                       # loose panel screws
            modes([(1500, .02), (2600, .015)])]                       # a cable clip
    weights = np.array([0.25, 0.2, 0.15, 0.25, 0.15])
    shake = 0.6 + 0.4 * L.wander(18, 2)
    exc = [np.zeros(L.n) for _ in objs]
    tt = 0.0
    while True:
        tt += r.exponential(1 / 32.0)
        if tt >= L.T:
            break
        s = shake[int(tt * RATE) + L.N]
        if r.uniform() > s ** 2:
            continue
        k = r.choice(len(objs), p=weights)
        a = s * r.lognormal(0, 0.5)
        b = 0.0
        for j in range(r.integers(1, 4)):            # a hit and a bounce or two
            sign = r.choice([-1, 1])
            i0 = int((tt + b) * RATE)
            for c in range(3):
                exc[k][(i0 + c * L.N) % L.n] += a * (0.5 ** j) * sign
            b += r.uniform(0.005, 0.015)
    y = np.zeros(L.n)
    for e, m in zip(exc, objs):
        y += signal.fftconvolve(e, modal_ir(m, 0.15, r))[:L.n]
    rumble = L.noise(lambda f: lp_mag(f, 60, 2) * brown_mag(f) * 40) * shake * 0.15
    y = room(y + rumble, 0.35, keep_length=True)
    return L.out(y), {"rate_per_s": 32, "objects": ["pencil", "carabiner", "thermos cap", "screws", "cable clip"]}


def roar_layers(L, bands, flick_depth):
    y = np.zeros(L.n)
    for fc, g in bands:
        layer = L.noise(lambda f, fc=fc: bump(f, fc, 1.4) * pink_mag(f) * 30)
        y += g * layer * (0.5 + 0.5 * L.wander(5))        # the balance moves: a slowly moving formant
    return y * (1 + flick_depth * L.wander(50, 3))


def entry_roar(seed):
    L = Loop(6.0, seed)
    rumble = hull(L.noise(lambda f: lp_mag(f, 120, 2) * brown_mag(f) * 60), lowcut=300)
    roar = roar_layers(L, ((250, 1.0), (500, 0.9), (900, 0.7), (1600, 0.45)), 0.3)
    y = 0.9 * rumble / np.std(rumble) + 0.7 * roar / np.std(roar)
    y = filt(y, "low", 3500, 2)
    y = room(soft(y, 1.8), 0.25, keep_length=True)
    return L.out(y), {"formants_hz": [250, 500, 900, 1600], "buffet_depth": 0.3}


def entry_roar_hi(seed):
    L = Loop(6.0, seed)
    r = L.rng
    rumble = hull(L.noise(lambda f: lp_mag(f, 120, 2) * brown_mag(f) * 60), lowcut=300)
    roar = roar_layers(L, ((300, 1.0), (600, 0.9), (1100, 0.8), (2000, 0.6), (3200, 0.45), (5000, 0.3)), 0.45)
    hiss = L.noise(lambda f: bump(f, 5000, 1.8)) * (1 + 0.4 * L.wander(40, 3))
    y = 0.8 * rumble / np.std(rumble) + 0.7 * roar / np.std(roar) + 0.35 * hiss
    cr = np.zeros(L.n)
    tt = 0.0
    while True:
        tt += r.exponential(1 / 60.0)
        if tt >= L.T:
            break
        m = int(r.uniform(0.0002, 0.001) * RATE)
        a = min(r.pareto(2.0) + 0.2, 4.0)
        L.add(r.standard_normal(m) * np.exp(-np.arange(m) / max(m / 4, 1)), tt, a)
    for _ in range(5):
        L.add(ring(burst(r, 0.4), PANEL, 0.1, r, r.uniform(0.9, 1.3)), r.uniform(0, L.T), 0.8)
    cr = filt(L.x, "high", 1500, 2)
    y = y + 2.5 * cr
    y = filt(y, "low", 9000, 2)
    y = room(soft(y, 2.0), 0.25, keep_length=True)
    return L.out(y), {"crackles_per_s": 60, "hiss_hz": 5000, "buffet_depth": 0.45}


def plasma_crackle(seed, v):
    C = Canvas(0.25, seed)
    r = C.rng
    span = r.uniform(0.1, 0.19)
    for k in range(r.integers(6, 21)):
        at = 0.003 + span * r.uniform() ** 1.6
        a = min(r.pareto(1.8) + 0.2, 3.0)
        m = int(r.uniform(0.0002, 0.001) * RATE)
        c = filt(r.standard_normal(m) * np.exp(-np.arange(m) / max(m / 4, 1)), "high", 1500, 2)
        C.add(c, at, a)
        C.add(ring(c, GLASS, 0.05, r, r.uniform(0.9, 1.1)), at, 0.15 * a)
    t = t_of(span + 0.05)
    C.add(filt(r.standard_normal(len(t)), "band", [3000, 9000], 2) * np.sin(np.pi * t / t[-1]) ** 2, 0.0, 0.25)
    C.add(thump(110, 0.02), 0.003, 0.4)
    return room(C.x, 0.25), {"span_s": round(span, 3)}


def wind(seed):
    L = Loop(6.0, seed)
    r = L.rng
    gust = 0.65 + 0.35 * L.wander(4)
    body = L.noise(lambda f: bump(f, 350, 2.2) * 0.8 + lp_mag(f, 90, 2) * 0.5)
    y = body * gust * (1 + 0.12 * L.wander(25, 3))
    whistle = peak(L.noise(), L.q(520), 25)
    y = y + 0.4 * whistle / np.std(whistle) * gust ** 3
    for _ in range(6):
        d = r.uniform(0.05, 0.12)
        t = t_of(d * 4)
        L.add(filt(r.standard_normal(len(t)), "low", 70, 2) * env_ad(t, d * 0.4, d), r.uniform(0, L.T), r.uniform(4, 8))
    y = hull(y + L.x, lowcut=2500)
    y = room(soft(y, 1.4), 0.25, keep_length=True)
    return L.out(y), {"body_hz": 350, "whistle_hz": L.q(520), "buffets": 6}


def drogue(seed):
    C = Canvas(1.3, seed)
    r = C.rng
    C.add(thump(60, 0.1, drop=1.0), 0.02, 0.45)
    C.add(ring(burst(r, 2.0, lp=3000), HULL_STRIKE, 0.8, r), 0.02, 0.6)
    C.add(ring(burst(r, 0.5), PANEL, 0.1, r), 0.02, 0.35)
    n = int(0.48 * RATE)
    tt = np.arange(n) / RATE
    C.add(tv_band(r, n, 300 * 2 ** (2.5 * tt / tt[-1]), 1.5) * (tt / tt[-1]) ** 1.5, 0.06, 0.4)
    snatch = 0.56
    C.add(thump(40, 0.15, drop=0.5), snatch, 0.5)
    C.add(ring(burst(r, 3.0, lp=2500), HULL_STRIKE, 0.8, r), snatch, 0.6)
    t = t_of(0.6)
    twang = sum(a * np.sin(TAU * f * (1 + 0.02 * np.exp(-t / 0.05)) * t) * np.exp(-t / d) for f, d, a in ((330, .15, 1), (661, .1, .6), (995, .07, .4)))
    C.add(twang, snatch, 0.25)
    n = int(0.35 * RATE)
    tt = np.arange(n) / RATE
    p = stickslip(r, 40 + 30 * np.sin(np.pi * tt / 0.35), np.sin(np.pi * tt / 0.35), 0.2)
    C.add(ring(p, HINGE, 0.12, r, 0.6), snatch + 0.05, 0.3)
    return room(soft(C.x, 1.4), 0.3), {"mortar_s": 0.02, "snatch_s": snatch}


def chute_open(seed):
    C = Canvas(1.8, seed)
    r = C.rng
    n = int(0.62 * RATE)
    tt = np.arange(n) / RATE
    flap = 1 + 0.8 * np.sin(TAU * np.cumsum(18 + 14 * tt / tt[-1]) / RATE)
    C.add(filt(r.standard_normal(n), "band", [300, 3000], 2) * flap * (tt / tt[-1]) ** 2, 0.0, 0.5)
    crack = 0.62
    C.add(thump(35, 0.3, drop=0.8, drop_s=0.03), crack, 0.45)
    C.add(burst(r, 60, 18, lp=3000), crack, 1.0)
    C.add(ring(burst(r, 3.0, lp=2000), HULL_STRIKE, 0.9, r), crack, 0.5)
    n = int(1.0 * RATE)
    tt = np.arange(n) / RATE
    amp = np.sin(np.pi * tt / 1.0) ** 0.6
    p = stickslip(r, 30 + 25 * np.sin(np.pi * tt), amp, 0.25)
    C.add(ring(p, modes([(140, .05), (230, .04), (390, .03), (610, .02)]), 0.2, r), crack + 0.08, 0.6)
    C.add(ring(p[::3], HINGE, 0.1, r, 0.8), crack + 0.15, 0.2)
    t = t_of(1.1)
    C.add(filt(r.standard_normal(len(t)), "low", 600, 2) * np.exp(-t / 0.5), crack, 0.15)
    return room(soft(C.x, 1.4), 0.3), {"crack_s": crack}


def chute_creak(seed):
    L = Loop(6.0, seed)
    r = L.rng
    swing = L.lfo(2)                          # two swings (one there and back each 3 s) in the loop
    speed = np.abs(L.lfo(2, math.pi / 2))
    bed = L.noise(lambda f: lp_mag(f, 600, 2) * pink_mag(f) * 20) * (0.25 + 0.15 * speed ** 2 + 0.05 * L.wander(3))
    rustle = L.noise(lambda f: bump(f, 3000, 1.5)) * 0.03 * (1 + L.wander(12, 2))
    for k, at in enumerate((0.75, 2.25, 3.75, 5.25)):
        at += r.uniform(-0.12, 0.12)
        dur = r.uniform(0.3, 0.6)
        n = int(dur * RATE)
        tt = np.arange(n) / RATE
        rate = r.uniform(15, 30) + r.uniform(20, 50) * np.sin(np.pi * tt / dur)
        p = stickslip(r, rate, np.sin(np.pi * tt / dur) ** 0.8, 0.2)
        m = [(r.uniform(240, 300) * j ** 0.95, r.uniform(0.03, 0.07), 0.8 ** j) for j in range(1, 5)]
        L.add(ring(p, m, 0.15, r), at - dur / 2, r.uniform(0.5, 1.0))
    y = bed + rustle + 1.5 * L.x
    y = room(y, 0.3, keep_length=True)
    return L.out(y), {"swing_s": 3.0, "creaks": 4}


def jets(seed):
    C = Canvas(1.5, seed)
    r = C.rng
    t = C.t
    C.add(ring(burst(r, 0.5), HULL_STRIKE, 0.3, r), 0.0, 0.4)
    C.add(thump(50, 0.1, drop=0.8), 0.01, 0.9)
    env = np.clip(t / 0.03, 0, 1) * np.where(t < 1.15, 1.0, np.exp(-(t - 1.15) / 0.08))
    roar = hull(filt(r.standard_normal(len(t)), "band", [60, 2500], 2), lowcut=2500)
    roar = roar / np.std(roar) * (1 + 0.3 * smooth_rand(r, len(t), 15))
    hiss = filt(r.standard_normal(len(t)), "band", [1000, 5000], 2)
    C.add((roar + 0.3 * hiss / np.std(hiss)) * env, 0.0, 0.5)
    return room(soft(C.x, 1.8), 0.25), {"burn_s": 1.15}


def splash(seed):
    C = Canvas(2.4, seed)
    r = C.rng
    hit = 0.02
    C.add(thump(38, 0.25, drop=1.2, drop_s=0.02), hit, 0.45)
    C.add(thump(70, 0.15), hit, 0.3)
    C.add(ring(burst(r, 4.0, lp=2500), HULL_STRIKE, 1.0, r), hit, 0.7)
    C.add(filt(burst(r, 120, 40), "low", 3500, 2), hit, 1.0)
    t = t_of(1.2)
    C.add(filt(r.standard_normal(len(t)), "band", [400, 3000], 2) * env_ad(t, 0.05, 0.45), hit + 0.03, 0.6)
    t = t_of(2.0)
    run = filt(r.standard_normal(len(t)), "band", [600, 3000], 2) * np.exp(-t / 0.8) * np.minimum(1, t / 0.2)
    C.add(run, 0.4, 0.12)
    tt = 0.35
    while tt < 2.3:
        C.add(bubble(r.uniform(400, 1800), r.uniform(0.005, 0.015), r.uniform(0.1, 0.4)), tt, r.uniform(0.05, 0.12) * math.exp(-(tt - 0.35) / 1.0))
        tt += r.exponential(0.025) * (1 + tt)
    n = int(0.4 * RATE)
    s = np.arange(n) / RATE
    C.add(ring(stickslip(r, 30 + 20 * np.sin(np.pi * s / 0.4), np.sin(np.pi * s / 0.4)), LEATHER, 0.1, r, 0.6), 1.2, 0.12)
    y = filt(C.x, "low", 4500, 2)
    return room(soft(y, 1.5), 0.35), {"impact_s": hit}


def afloat(seed):
    L = Loop(6.0, seed)
    r = L.rng
    bed = L.noise(lambda f: lp_mag(f, 300, 2) * pink_mag(f) * 20) * (0.8 + 0.2 * L.lfo(2, 0.7) + 0.1 * L.wander(3))
    y = 0.2 * bed

    def slap(at, g):
        d = r.uniform(0.18, 0.3)
        t = t_of(d * 4)
        s = filt(r.standard_normal(len(t)), "band", [200, 900], 2) * env_ad(t, r.uniform(0.03, 0.06), d)
        L.add(s, at, g)
        L.add(thump(60, 0.2, drop=0.2), at + 0.03, 0.25 * g)
        L.add(ring(burst(r, 3.0, lp=600), HULL_STRIKE, 0.6, r), at + 0.03, 0.15 * g)

    for at, g in ((0.4, 1.0), (2.3, 0.7), (4.5, 0.85)):
        slap(at, g)
    for _ in range(8):
        slap(r.uniform(0, L.T), r.uniform(0.15, 0.35))
    for _ in range(20):
        L.add(bubble(r.uniform(300, 900), r.uniform(0.008, 0.02), 0.25), r.uniform(0, L.T), r.uniform(0.03, 0.08))
    y = filt(y + L.x, "low", 1800, 2)
    y = room(y, 0.35, keep_length=True)
    return L.out(y), {"waves_at_s": [0.4, 2.3, 4.5], "small_laps": 8}


def drip(seed, v):
    C = Canvas(0.35, seed)
    r = C.rng
    if v < 3:
        C.add(bubble(r.uniform(900, 1600), r.uniform(0.012, 0.025), 0.3), 0.004, 0.7)
        C.add(burst(r, 0.4, hp=2000), 0.002, 0.25)
    else:
        C.add(ring(burst(r, 0.3), modes([(2200, .05), (3600, .035), (5300, .02)]), 0.25, r), 0.002, 0.6)
    return room(C.x, 0.4), {"onto": "puddle" if v < 3 else "metal"}


# ------------------------------------------------------------------------------------------------ the record player

def needle_down(seed, v):
    C = Canvas(0.5, seed)
    r = C.rng
    C.add(thump(70, 0.04), 0.005, 1.0)
    C.add(thump(140, 0.02), 0.005, 0.4)
    for _ in range(4):
        C.add(burst(r, 0.4, hp=600), r.uniform(0.01, 0.06), r.uniform(0.2, 0.6))
    t = t_of(0.48)
    C.add(filt(r.standard_normal(len(t)), "band", [1000, 7000], 2) * np.minimum(1, t / 0.03) * np.exp(-t / 0.25), 0.01, 0.1)
    return room(speaker(C.x), 0.25), {}


def needle_up(seed, v):
    C = Canvas(0.4, seed)
    r = C.rng
    n = int(0.05 * RATE)
    tt = np.arange(n) / RATE
    C.add(tv_band(r, n, 4000 * 0.25 ** (tt / tt[-1]), 1.0, nper=128) * np.sin(np.pi * tt / tt[-1]), 0.0, 0.5)
    C.add(burst(r, 0.5, hp=500), 0.045, 0.6)
    C.add(thump(90, 0.03), 0.05, 0.5)
    return room(speaker(C.x), 0.25), {}


def needle_skip(seed, v):
    C = Canvas(0.3, seed)
    r = C.rng
    n = int(0.06 * RATE)
    tt = np.arange(n) / RATE
    grains = 1 + 0.8 * smooth_rand(r, n, 400)
    C.add(filt(r.standard_normal(n), "band", [1500, 6000], 2) * grains * np.sin(np.pi * tt / tt[-1]), 0.0, 1.0)
    C.add(burst(r, 0.8, hp=400), r.uniform(0.02, 0.05), 1.2)
    C.add(thump(80, 0.03), 0.01, 0.6)
    return room(speaker(C.x), 0.25), {}


def vinyl_crackle(seed):
    L = Loop(7.2, seed)                        # four turns at 33 1/3 a minute
    r = L.rng
    hiss = L.noise(lambda f: hp_mag(f, 2000, 2) * lp_mag(f, 10000, 2)) * 0.06
    rumble = L.noise(lambda f: lp_mag(f, 40, 2) * brown_mag(f) * 30) * 0.08
    turn = (0.5 + 0.5 * np.sin(TAU * L.t / 1.8)) ** 2
    swish = L.noise(lambda f: bump(f, 700, 1.8)) * turn * 0.04
    tt = 0.0
    while True:
        tt += r.exponential(1 / 25.0)
        if tt >= L.T:
            break
        m = int(r.uniform(0.0001, 0.001) * RATE)
        L.add(r.standard_normal(max(m, 4)) * np.exp(-np.arange(max(m, 4)) / max(m / 3, 1)), tt, min(0.05 + 0.15 * r.pareto(1.5), 1.2))
    for k in range(4):                         # the same scratch once a turn, never quite the same
        L.add(burst(r, 2.5, 0.6), 0.37 + 1.8 * k, r.uniform(0.35, 0.5))
    y = hiss + rumble + swish + filt(L.x, "band", [600, 9000], 2)
    y = speaker(y, 40, 10000, 1.2)
    return L.out(y), {"turn_s": 1.8, "pops_per_s": 25}


def platter_start(seed):
    C = Canvas(1.4, seed)
    r = C.rng
    t = C.t
    C.add(ring(burst(r, 0.4), SMALL, 0.08, r, 1.4), 0.0, 0.4)
    spd = 1 - np.exp(-t / 0.35)
    wow = 1 + 0.01 * np.sin(TAU * 1.5 * t)
    whir = 0
    for k, a in ((1, 1.0), (2, 0.4), (3, 0.15)):
        whir = whir + a * np.sin(TAU * np.cumsum(k * 300 * spd * wow) / RATE)
    C.add(whir * spd * 0.12, 0.02)
    rum = filt(r.standard_normal(len(t)), "low", 150, 2) * spd
    C.add(rum / np.std(rum) * 0.1, 0.02)
    C.add(np.sin(TAU * np.cumsum(900 * spd * wow) / RATE) * spd * 0.03, 0.02)
    C.x *= np.minimum(1, (t[-1] - t) / 0.3 + 0.35)
    return room(C.x, 0.25), {"motor_hz": 300, "tau_s": 0.35}


def runout(seed):
    L = Loop(7.2, seed)
    r = L.rng
    # the eccentric groove's whoosh swells into each tick and away again (a whole number of turns in the loop)
    swell = (0.5 + 0.5 * np.cos(TAU * (L.t - 0.9) / 1.8)) ** 3
    swish = L.noise(lambda f: bump(f, 600, 1.8)) * swell * 0.08
    hiss = L.noise(lambda f: hp_mag(f, 2000, 2) * lp_mag(f, 9000, 2)) * 0.03
    for k in range(4):
        at = 1.8 * k + 0.9 + r.uniform(-0.002, 0.002)
        g = r.uniform(0.9, 1.1)
        L.add(thump(60, 0.03), at, 0.8 * g)
        L.add(filt(burst(r, 0.6), "high", 800, 2), at, 0.7 * g)
    for _ in range(40):
        L.add(burst(r, 0.3), r.uniform(0, L.T), r.uniform(0.03, 0.15))
    y = speaker(L.x + swish + hiss, 40, 9000, 1.3)
    y = room(y, 0.25, keep_length=True)
    return L.out(y), {"tick_every_s": 1.8}


# ------------------------------------------------------------------------------------------------ the thermos, small things

def thermos_open(seed, v):
    C = Canvas(0.7, seed)
    r = C.rng
    n = int(0.38 * RATE)
    tt = np.arange(n) / RATE
    twists = np.sin(np.pi * np.clip(tt / 0.17, 0, 1)) ** 1.5 + np.sin(np.pi * np.clip((tt - 0.2) / 0.17, 0, 1)) ** 1.5
    p = stickslip(r, 380 + 120 * smooth_rand(r, n, 6), twists, 0.4)
    C.add(ring(p, modes([(1900, .01), (3100, .008), (4700, .006)]), 0.05, r, r.uniform(0.9, 1.1)), 0.0, 0.4)
    pop = 0.42
    C.add(thump(380 * r.uniform(0.9, 1.1), 0.02, drop=-0.3), pop, 0.8)
    C.add(burst(r, 0.5, lp=6000), pop, 0.3)
    C.add(burst(r, 250, 80, hp=3000, lp=9000), pop + 0.02, 0.08)
    return room(C.x, 0.3), {"pop_s": pop}


def sip(seed, v):
    C = Canvas(0.55, seed)
    r = C.rng
    n = int(0.22 * RATE)
    tt = np.arange(n) / RATE
    slurp = tv_band(r, n, 700 * 2 ** (1.2 * tt / tt[-1]), 0.8, nper=256) * (1 + 0.6 * smooth_rand(r, n, 40))
    C.add(slurp * np.sin(np.pi * tt / tt[-1]) ** 1.2, 0.0, 0.35)
    for _ in range(3):
        C.add(bubble(r.uniform(600, 1200), 0.008, 0.3), r.uniform(0.05, 0.18), 0.1)
    C.add(bubble(r.uniform(170, 200), 0.03, 0.35), 0.32, 0.9)
    C.add(thump(90, 0.03), 0.32, 0.4)
    t = t_of(0.13)
    C.add(filt(r.standard_normal(len(t)), "low", 1500, 2) * np.sin(np.pi * t / t[-1]), 0.42, 0.06)
    return room(C.x, 0.25), {"gulp_s": 0.32}


def page(seed, v):
    C = Canvas(0.45, seed)
    r = C.rng
    n = int(0.3 * RATE)
    tt = np.arange(n) / RATE
    rustle = filt(r.standard_normal(n), "band", [1500, 9000], 2) * (1 + 0.8 * smooth_rand(r, n, 60)) * (tt / tt[-1]) ** 0.8
    C.add(rustle, 0.0, 0.25)
    at = 0.0
    while at < 0.3:
        C.add(burst(r, 0.2, hp=3000), at, r.uniform(0.1, 0.3))
        at += r.exponential(1 / 120.0)
    flick = r.uniform(0.28, 0.33)
    C.add(filt(burst(r, 40, 10), "band", [600, 4000], 2), flick, 0.9)
    C.add(burst(r, 60, 18, lp=400), flick, 0.4)
    return room(C.x, 0.25), {"flick_s": round(flick, 3)}


# ------------------------------------------------------------------------------------------------ writing and checking

def finish_oneshot(x):
    x = filt(x, "high", 20, 2)
    p = np.max(np.abs(x)) or 1.0
    on = np.nonzero(np.abs(x) > p * db(-50))[0]
    a = max(0, on[0] - int(0.001 * RATE))
    loud = np.nonzero(np.abs(x) > p * db(-48))[0]
    b = min(len(x), loud[-1] + 1)
    x = x[a:b].copy()
    fi, fo = min(len(x) // 4, int(0.001 * RATE)), min(len(x) // 4, int(0.04 * RATE))
    x[:fi] *= np.linspace(0, 1, fi)
    x[len(x) - fo:] *= np.linspace(1, 0, fo) ** 2
    return x


def finish_loop(x):
    return x - np.mean(x)


def measure(pcm, loop, rate=RATE):
    x = pcm.astype(np.float64) / 32768.0
    n = len(x)
    pk = np.max(np.abs(x))
    rms = math.sqrt(float(np.mean(x ** 2))) or 1e-9
    X = np.abs(np.fft.rfft(x * np.hanning(n)))
    f = np.fft.rfftfreq(n, 1 / rate)
    pw = X ** 2
    centroid = float(np.sum(f * pw) / (np.sum(pw) or 1))
    roll = float(f[np.searchsorted(np.cumsum(pw), 0.85 * np.sum(pw))])
    m = {"seconds": round(n / rate, 3), "rate": rate, "peak_dbfs": round(20 * math.log10(pk or 1e-9), 2),
         "rms_dbfs": round(20 * math.log10(rms), 2), "crest_db": round(20 * math.log10((pk or 1e-9) / rms), 1),
         "centroid_hz": round(centroid), "rolloff85_hz": round(roll), "dc": round(float(np.mean(x)), 5),
         "clipped": int(np.sum(np.abs(pcm.astype(np.int32)) >= 32767))}
    if loop:
        d = np.abs(np.diff(x))
        jump = abs(x[0] - x[-1])
        m["seam_jump"] = round(float(jump / (np.sqrt(np.mean(d ** 2)) or 1e-9)), 2)
        m["seam_jump_pct"] = round(float(np.mean(d < jump) * 100), 1)      # where the wrap's step falls among all steps
        # high-passed energy in a window across the seam, against the same at 64 other places
        w = max(64, rate // 200)

        def hf_share(y):
            return np.mean(np.diff(y, 2) ** 2) / (np.mean(y ** 2) or 1e-12)

        seam = hf_share(np.concatenate([x[-w:], x[:w]]))
        rng = np.random.default_rng(0)
        others = np.array([hf_share(x[i - w:i + w]) for i in rng.integers(w, n - w, 400)])
        m["seam_energy"] = round(float(seam / (np.median(others) or 1e-12)), 2)
        m["seam_pct"] = round(float(np.mean(others < seam) * 100), 1)
        a, b = x[:int(0.25 * rate)], x[-int(0.25 * rate):]
        m["seam_level_db"] = round(20 * math.log10((np.sqrt(np.mean(b ** 2)) or 1e-9) / (np.sqrt(np.mean(a ** 2)) or 1e-9)), 1)
    return m


def write(name, x, seed, params, loop, rate=RATE):
    x = np.asarray(x, dtype=np.float64)
    x = finish_loop(x) if loop else finish_oneshot(x)
    hf = None
    if rate != RATE:
        # a low loop stored at half rate: measure what is lost above the new Nyquist, then resample it as a periodic
        # signal (FFT resampling assumes exactly that, so the seam stays closed)
        X = np.abs(np.fft.rfft(x)) ** 2
        f = np.fft.rfftfreq(len(x), 1 / RATE)
        hf = round(10 * math.log10((np.sum(X[f > rate / 2 * 0.9]) or 1e-30) / np.sum(X)), 1)
        x = signal.resample(x, len(x) * rate // RATE)
    x = x / (np.max(np.abs(x)) or 1.0) * db(PEAK_DB)
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())
    with open(path, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    m = measure(pcm, loop, rate)
    if hf is not None:
        params = dict(params, stored_rate=rate, energy_above_new_nyquist_db=hf)
    side = {"name": name, "asset": "/Game/Audio/Spacecraft/S_%s" % name, "pool": name.rsplit("_", 1)[0],
            "file": os.path.relpath(path, ROOT), "generator": "tools/spacecraft/sounds.py", "seed": seed,
            "rng": "numpy.random.default_rng (PCG64), numpy %s, scipy %s" % (np.__version__, __import__("scipy").__version__),
            "loop": loop, "sample_rate": rate, "bit_depth": 16, "channels": 1, "seconds": m["seconds"],
            "peak_dbfs": PEAK_DB, "rms_dbfs": m["rms_dbfs"], "params": params, "checks": m, "sha256": sha,
            "license": "Procedural, made by the project (own work, no recordings, no generation service); CC BY 4.0."}
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, ensure_ascii=False, indent=1)
        f.write("\n")
    return m


# id: (function, variants, loop, base seed, one line on what it is). Variants > 1 call f(seed, v).
SET = {
    "sc_cabin_hum": (cabin_hum, 1, True, 7101, "the cabin's electrics (400 Hz) and its two fans, soft and warm"),
    "sc_fan": (fan, 1, True, 7111, "one cabin fan, for a positional source"),
    "sc_gyro_whine": (gyro_whine, 1, True, 7121, "the gyros running (two rotors 2 Hz apart)"),
    "sc_gyro_spinup": (gyro_spinup, 1, False, 7131, "the gyros switched on: relay, motor growl, the whine rising over 4 s"),
    "sc_air_hiss": (air_hiss, 1, True, 7141, "life support: the air through the vents, the regenerator's valve breathing"),
    "sc_radio_static": (radio_static, 1, True, 7151, "thin shortwave hiss with far whistles from the radio speaker"),
    "sc_beacon": (beacon, 1, True, 7161, "a far two-tone telemetry beacon, like Sputnik"),
    "sc_toggle": (toggle, 3, False, 7201, "a chunky guarded toggle switch"),
    "sc_cover": (cover, 2, False, 7211, "the red cover flipped up or down"),
    "sc_button": (button, 2, False, 7221, "the big ПУСК button's clunk"),
    "sc_relay": (relay, 2, False, 7231, "relays clattering after a switch"),
    "sc_buzzer": (buzzer, 2, False, 7241, "the refusal buzzer (wrong order)"),
    "sc_chime": (chime, 2, False, 7251, "the attention tone before a callout"),
    "sc_lamp_tick": (lamp_tick, 2, False, 7261, "an annunciator lamp lighting"),
    "sc_hatch_close": (hatch_close, 2, False, 7271, "the hatch swung shut: clang, the dogs home"),
    "sc_hatch_open": (hatch_open, 2, False, 7281, "the hatch opened: dogs free, the seal lets go, the hinge groans"),
    "sc_wheel": (wheel, 2, False, 7291, "the hatch wheel turned"),
    "sc_belt": (belt, 2, False, 7301, "the harness buckle"),
    "sc_seat": (seat, 2, False, 7311, "settling into the padded couch"),
    "sc_ignition": (ignition, 1, False, 7401, "the engine lights: valves, igniters, the burn building (3 s)"),
    "sc_engine": (engine, 1, True, 7411, "the burn heard through the hull: deep, structure-borne"),
    "sc_cutoff": (cutoff, 1, False, 7421, "thrust dies; the plumbing ticks"),
    "sc_thruster": (thruster, 3, False, 7431, "cold-gas attitude puffs felt as thumps"),
    "sc_sep_bang": (sep_bang, 1, False, 7441, "the pyro bolts: a double bang and the hull ringing down"),
    "sc_creak": (creak, 4, False, 7451, "the hull creaking under g"),
    "sc_rattle": (rattle, 1, True, 7461, "loose things rattling in the shake"),
    "sc_entry_roar": (entry_roar, 1, True, 7471, "the entry: a broadband roar over a low rumble"),
    "sc_entry_roar_hi": (entry_roar_hi, 1, True, 7481, "the entry at its peak: more crackle and hiss"),
    "sc_plasma_crackle": (plasma_crackle, 4, False, 7491, "plasma crackles on the shell and the glass"),
    "sc_wind": (wind, 1, True, 7501, "thick air rushing past under the drogue"),
    "sc_drogue": (drogue, 1, False, 7511, "the drogue: mortar bang, the bag, the snatch"),
    "sc_chute_open": (chute_open, 1, False, 7521, "the main canopy cracks open; the lines groan"),
    "sc_chute_creak": (chute_creak, 1, True, 7531, "swinging under the main canopy: line creaks, soft wind"),
    "sc_jets": (jets, 1, False, 7541, "the soft-landing rockets (1.5 s)"),
    "sc_splash": (splash, 1, False, 7551, "the splash: the heavy hit, spray, water running off"),
    "sc_afloat": (afloat, 1, True, 7561, "afloat: water lapping on the hull, heard inside"),
    "sc_drip": (drip, 3, False, 7571, "a drip in the cabin after the splash"),
    "sc_needle_down": (needle_down, 2, False, 7601, "the record player's needle set down"),
    "sc_needle_up": (needle_up, 2, False, 7611, "the needle lifted"),
    "sc_needle_skip": (needle_skip, 2, False, 7621, "the needle skidding (a jolt)"),
    "sc_vinyl_crackle": (vinyl_crackle, 1, True, 7631, "surface noise to lay over the music (4 turns)"),
    "sc_platter_start": (platter_start, 1, False, 7641, "the platter motor running up"),
    "sc_runout": (runout, 1, True, 7651, "the run-out groove: a tick every 1.8 s"),
    "sc_thermos_open": (thermos_open, 2, False, 7701, "the thermos unscrewed, its cap's pop"),
    "sc_sip": (sip, 2, False, 7711, "a sip of hot tea"),
    "sc_page": (page, 2, False, 7721, "a page of the book turned"),
}


# Loops with nothing above ~8 kHz (structure-borne, behind the hull) are stored at 24 kHz to keep the set under the
# 12 MB budget; LOW_RATE = RATE stores them at 48 kHz like the rest (+1.4 MB). Each sidecar records the energy lost.
LOW_RATE = 24000
LOW = {"sc_engine", "sc_afloat", "sc_entry_roar", "sc_wind", "sc_chute_creak"}


def flags(m, loop):
    out = []
    if m["clipped"]:
        out.append("CLIP")
    if abs(m["dc"]) > 0.002:
        out.append("DC")
    if loop:
        if m["seam_jump"] > 3.0 and m["seam_jump_pct"] > 99.9:
            out.append("SEAM")
        if m["seam_energy"] > 2.5 and m["seam_pct"] > 99:
            out.append("SEAM-HF")
        if not 6.0 <= m["seconds"] <= 12.0:
            out.append("LEN")
    else:
        if m["crest_db"] < 8.0:
            out.append("FLAT")
    if abs(m["peak_dbfs"] - PEAK_DB) > 0.2:
        out.append("PEAK")
    return " ".join(out) or "ok"


def table(rows):
    print("%-26s %4s %5s %6s %7s %6s %7s %8s %7s %6s %6s %5s  %s" % ("file", "loop", "kHz", "sec", "rms", "crest", "centr", "roll85", "dc", "jump%", "seamHF", "hf%", "check"))
    total = 0
    for name, loop, m, size in rows:
        total += size
        print("%-26s %4s %5d %6.2f %7.1f %6.1f %7d %8d %7.4f %6s %6s %5s  %s" % (
            name, "L" if loop else "", m["rate"] // 1000, m["seconds"], m["rms_dbfs"], m["crest_db"], m["centroid_hz"], m["rolloff85_hz"],
            m["dc"], m.get("seam_jump_pct", ""), m.get("seam_energy", ""), m.get("seam_pct", ""), flags(m, loop)))
    print("%d files, %.2f MB (%.2f MiB) of wav" % (len(rows), total / 1e6, total / 1048576))


def check_disk():
    rows = []
    for sid, (fn, var, loop, seed, _) in SET.items():
        for v in range(1, var + 1):
            path = os.path.join(OUT, "%s_%02d.wav" % (sid, v))
            if not os.path.exists(path):
                print("MISSING", path)
                continue
            with wave.open(path, "rb") as w:
                rate = w.getframerate()
                assert rate in (RATE, LOW_RATE) and w.getnchannels() == 1 and w.getsampwidth() == 2
                pcm = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
            rows.append((os.path.basename(path), loop, measure(pcm, loop, rate), os.path.getsize(path)))
    table(rows)


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--check" not in args:
        for sid, (fn, var, loop, seed, _) in SET.items():
            if args and not any(w in sid for w in args):
                continue
            for v in range(1, var + 1):
                s = seed + (v - 1)
                x, params = fn(s, v) if var > 1 else fn(s)
                name = "%s_%02d" % (sid, v)
                write(name, x, s, params, loop, LOW_RATE if sid in LOW else RATE)
                print("wrote", name)
    check_disk()
