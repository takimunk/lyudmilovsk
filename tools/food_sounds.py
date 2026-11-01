"""Procedural eating sounds for the food in the hand (ULyudFoodSubsystem, docs/food.md): bites by what is bitten, chewing, a
swallow, a napkin's rustle. Noise and tones shaped with envelopes and resonant band-pass filters, seeded, numpy only:

    python3 tools/food_sounds.py          writes audio/sfx/food/<pool>_<nn>.wav (48 kHz 16-bit mono)

then unreal/scripts/import_audio.py puts them under /Game/Audio/Food/S_<pool>_<nn> (the soundscape's "Food" pools).
Kept soft and close: eating is heard in the head, not across the street.
"""
import math
import os
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "audio", "sfx", "food")
RATE = 48000


def bandpass(x, f, q):
    """A resonant 2-pole band-pass (RBJ), f Hz (scalar or per sample), q."""
    y = np.zeros_like(x)
    f = np.broadcast_to(np.asarray(f, dtype=float), x.shape)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        w = 2 * math.pi * min(f[i], RATE * 0.45) / RATE
        alpha = math.sin(w) / (2 * q)
        b0, b2 = alpha, -alpha
        a0, a1, a2 = 1 + alpha, -2 * math.cos(w), 1 - alpha
        yi = (b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1 = x1, x[i]
        y2, y1 = y1, yi
        y[i] = yi
    return y


def lowpass(x, f):
    a = math.exp(-2 * math.pi * f / RATE)
    y = np.zeros_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc = (1 - a) * x[i] + a * acc
        y[i] = acc
    return y


def env(n, attack, decay):
    t = np.arange(n) / RATE
    return np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-np.maximum(t - attack, 0) / max(decay, 1e-4))


def place(buf, sig, at):
    i = int(at * RATE)
    j = min(len(buf), i + len(sig))
    if i < len(buf):
        buf[i:j] += sig[: j - i]


def noise(rng, seconds):
    return rng.standard_normal(int(seconds * RATE))


def pop(rng, f, q, dur, amp):
    """One kernel bursting / a crumb cracking: a short resonant click."""
    return bandpass(noise(rng, dur), f, q) * env(int(dur * RATE), 0.0008, dur / 5) * amp


def squelch(rng, dur, f0, f1, amp):
    """A wet soft mouth sound: low noise through a sweeping resonance."""
    n = int(dur * RATE)
    sweep = np.linspace(f0, f1, n)
    s = bandpass(noise(rng, dur), sweep, 2.5)
    return s * np.sin(np.linspace(0, math.pi, n)) ** 1.5 * amp


def bite_crisp(rng):
    buf = np.zeros(int(0.7 * RATE))
    place(buf, squelch(rng, 0.09, 1400, 700, 0.5), 0.0)              # the teeth through the kernels' skins
    t = 0.01
    for _ in range(rng.integers(5, 9)):
        place(buf, pop(rng, rng.uniform(2200, 5200), rng.uniform(4, 9), rng.uniform(0.012, 0.03), rng.uniform(0.5, 1.0)), t)
        t += rng.uniform(0.008, 0.03)
    place(buf, squelch(rng, 0.22, 900, 300, 0.35), t)                # juice
    return buf


def bite_soft(rng):
    buf = np.zeros(int(0.5 * RATE))
    place(buf, squelch(rng, 0.18, 1600, 500, 0.6), 0.0)
    place(buf, lowpass(noise(rng, 0.15), 1200) * env(int(0.15 * RATE), 0.01, 0.05) * 0.8, 0.05)
    return buf


def bite_squeak(rng):
    """Fresh cheese squeaks on the teeth: a rubbery chirp over a soft tear."""
    buf = np.zeros(int(0.6 * RATE))
    n = int(0.12 * RATE)
    t = np.arange(n) / RATE
    f = rng.uniform(850, 1100) + 700 * t / t[-1] + 60 * np.sin(2 * math.pi * 31 * t)
    ph = 2 * math.pi * np.cumsum(f) / RATE
    tone = (np.sin(ph) + 0.4 * np.sin(2 * ph) + 0.15 * np.sin(3 * ph)) * np.sin(np.linspace(0, math.pi, n)) ** 2
    rough = 1 + 0.6 * bandpass(noise(rng, 0.12), 180, 1.0)
    place(buf, tone * rough * 0.25, 0.03)
    place(buf, squelch(rng, 0.2, 1300, 450, 0.45), 0.0)
    return buf


def bite_chewy(rng):
    """Smoked string cheese: a pull and a scatter of fibres tearing."""
    buf = np.zeros(int(0.7 * RATE))
    place(buf, squelch(rng, 0.25, 700, 350, 0.5), 0.0)
    t = 0.06
    for _ in range(rng.integers(10, 18)):
        place(buf, pop(rng, rng.uniform(1200, 3000), 3.0, rng.uniform(0.006, 0.014), rng.uniform(0.15, 0.4)), t)
        t += rng.uniform(0.008, 0.025)
    return buf


def bite_crunch(rng):
    buf = np.zeros(int(0.5 * RATE))
    t = 0.0
    for _ in range(rng.integers(14, 24)):
        place(buf, pop(rng, rng.uniform(2500, 7000), rng.uniform(2, 5), rng.uniform(0.006, 0.02), rng.uniform(0.4, 1.0)), t)
        t += rng.uniform(0.004, 0.018)
    place(buf, squelch(rng, 0.12, 900, 400, 0.2), 0.0)
    return buf


def chew(rng):
    """Three or four calm chews, mouth closed: soft low squelches with a little crunch left in the first ones."""
    buf = np.zeros(int(1.6 * RATE))
    t = 0.0
    k = int(rng.integers(3, 5))
    for i in range(k):
        place(buf, squelch(rng, rng.uniform(0.16, 0.24), rng.uniform(500, 750), rng.uniform(220, 320), 0.55 - 0.08 * i), t)
        if i < 2:
            for _ in range(int(rng.integers(1, 4))):
                place(buf, pop(rng, rng.uniform(1500, 3000), 3.0, 0.008, 0.12), t + rng.uniform(0.02, 0.1))
        t += rng.uniform(0.34, 0.42)
    return buf


def swallow(rng):
    buf = np.zeros(int(0.6 * RATE))
    n = int(0.12 * RATE)
    tt = np.arange(n) / RATE
    thump = np.sin(2 * math.pi * (110 + 40 * tt / tt[-1]) * tt) * np.sin(np.linspace(0, math.pi, n)) ** 2 * 0.5
    place(buf, thump, 0.08)
    place(buf, squelch(rng, 0.18, 450, 750, 0.35), 0.0)
    place(buf, squelch(rng, 0.12, 650, 350, 0.25), 0.2)
    return buf


def wrap(rng):
    buf = np.zeros(int(0.9 * RATE))
    t = 0.0
    while t < 0.7:
        place(buf, pop(rng, rng.uniform(1500, 6500), rng.uniform(1.0, 3.0), rng.uniform(0.004, 0.03), rng.uniform(0.15, 0.6)), t)
        t += rng.exponential(0.012)
    return buf


POOLS = {
    "food_bite_crisp": (bite_crisp, 4), "food_bite_soft": (bite_soft, 4), "food_bite_squeak": (bite_squeak, 4),
    "food_bite_chewy": (bite_chewy, 4), "food_bite_crunch": (bite_crunch, 4), "food_chew": (chew, 4),
    "food_swallow": (swallow, 3), "food_wrap": (wrap, 3),
}
PEAK_DB = -10.0


def write(path, x):
    x = x - np.mean(x)
    pk = np.max(np.abs(x)) or 1.0
    x = x / pk * 10 ** (PEAK_DB / 20)
    nz = np.nonzero(np.abs(x) > 1e-4)[0]
    if len(nz):
        x = x[: nz[-1] + int(0.02 * RATE)]
    fade = int(0.01 * RATE)
    x[-fade:] *= np.linspace(1, 0, fade)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())


def main():
    os.makedirs(OUT, exist_ok=True)
    for pool, (fn, n) in POOLS.items():
        for k in range(1, n + 1):
            rng = np.random.default_rng(sum(map(ord, pool)) * 31 + k)
            write(os.path.join(OUT, "%s_%02d.wav" % (pool, k)), fn(rng))
    print("food sounds ->", OUT)


if __name__ == "__main__":
    main()
