"""The bicycles' sounds, synthesized (no generation service, reproducible from this file and its seed; docs/bikes.md):

  python3 tools/bikesim/bike_sounds.py      (numpy)  -> audio/sfx/bikes/<id>.wav (+ .json sidecar, loop flag)

then unreal/scripts/import_bikes.sh imports them (IMPORT_ONLY=sfx/bikes/ import_audio.py) into /Game/Audio/Bikes.

  tyre_asphalt_01   700x23 tyres on asphalt: a smooth hum with a faint hiss, loop (the game pitches it with the speed)
  tyre_gravel_01    on earth and gravel: crunch and patter over a rumble, loop
  freewheel_01      the freewheel's pawls ticking as the wheel runs on while the cranks stand: a wheel turning 1.5 times a
                    second, 24 clicks a turn, loop (the game pitches it with the wheel's speed)
  chain_01          the chain running over the rings and the jockey wheels: a soft whirr, loop
  brake_01          rim brakes: the pads on the rims, a rub with a little squeal
  stand_01          the kickstand: a clack
"""
import json
import math
import os
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "audio", "sfx", "bikes")
SR = 44100
RNG = np.random.default_rng(1983)


def write(name, x, loop=False, gain=0.85):
    os.makedirs(OUT, exist_ok=True)
    x = np.asarray(x, dtype=np.float64)
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak * gain
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump({"id": name, "source": "tools/bikesim/bike_sounds.py", "loop": loop, "seconds": round(len(x) / SR, 3)}, f)
    print("BIKESOUND %s %.2f s%s" % (name, len(x) / SR, " loop" if loop else ""))


def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def band(x, lo=None, hi=None):
    """Band-limit by FFT (zero phase, circular: loops stay seamless)."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    g = np.ones_like(f)
    if lo:
        g *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** 4)
    if hi:
        g *= 1 / np.sqrt(1 + (f / hi) ** 4)
    return np.fft.irfft(X * g, len(x))


def norm(x):
    return x / (np.max(np.abs(x)) or 1.0)


def place(out, sig, at):
    """Adds sig at sample at, wrapping round the end (seamless loops)."""
    n = len(out)
    for i in range(0, len(sig), n):
        part = sig[i:i + n]
        s = (at + i) % n
        e = min(n, s + len(part))
        out[s:e] += part[:e - s]
        if e - s < len(part):
            out[:len(part) - (e - s)] += part[e - s:]


def tyre_asphalt():
    tt = t_of(4.0)
    hum = norm(band(RNG.standard_normal(len(tt)), lo=60, hi=420))
    hiss = norm(band(RNG.standard_normal(len(tt)), lo=2500, hi=7000))
    # the tread's seam and a slight wobble of the rim: once a wheel turn (~2.5 turns a second at 19 km/h)
    flutter = 1 + 0.12 * np.sin(2 * math.pi * 2.5 * tt) + 0.05 * np.sin(2 * math.pi * 5.0 * tt + 1.0)
    write("tyre_asphalt_01", hum * flutter + 0.18 * hiss, loop=True, gain=0.7)


def tyre_gravel():
    tt = t_of(4.0)
    out = 0.6 * norm(band(RNG.standard_normal(len(tt)), lo=50, hi=350))
    # stones popping under the tyres: short bursts of bright noise, dense and uneven
    n = int(4.0 * 140)
    for _ in range(n):
        at = int(RNG.uniform(0, len(tt)))
        dur = RNG.uniform(0.004, 0.02)
        k = t_of(dur)
        burst = RNG.standard_normal(len(k)) * np.exp(-k / (dur * 0.3)) * RNG.uniform(0.2, 1.0)
        place(out, band(np.pad(burst, (0, 64)), lo=900, hi=6500)[:len(k)] * 1.8, at)
    write("tyre_gravel_01", out, loop=True, gain=0.75)


def freewheel():
    # 1.5 wheel turns a second x 24 pawl clicks: 36 clicks a second; 2 s = 72 clicks, a whole number (seamless)
    sec = 2.0
    out = np.zeros(int(sec * SR))
    clicks = 72
    k = t_of(0.012)
    for i in range(clicks):
        at = int((i + RNG.uniform(-0.04, 0.04)) * len(out) / clicks)
        a = 0.75 + 0.25 * RNG.uniform()
        sig = (np.sin(2 * math.pi * 3400 * k) + 0.6 * np.sin(2 * math.pi * 6150 * k + 0.7) + 0.4 * RNG.standard_normal(len(k)) * np.exp(-k * 900))
        place(out, a * sig * np.exp(-k * 520), at)
    # the hub's hollow body rings a little
    out += 0.15 * norm(band(out, lo=700, hi=1600))
    write("freewheel_01", out, loop=True, gain=0.8)


def chain():
    tt = t_of(3.0)
    # links over the teeth: ~40 a second at a calm cadence, a soft whirr round them
    rate = 40.0
    base = norm(band(RNG.standard_normal(len(tt)), lo=300, hi=2400))
    teeth = 0.5 + 0.5 * np.sin(2 * math.pi * rate * tt) ** 8
    rattle = norm(band(RNG.standard_normal(len(tt)), lo=3000, hi=8000)) * (0.5 + 0.5 * np.sin(2 * math.pi * 1.0 * tt))
    write("chain_01", base * teeth + 0.12 * rattle, loop=True, gain=0.6)


def brake():
    tt = t_of(0.9)
    env = np.minimum(1.0, tt / 0.04) * np.exp(-np.maximum(0, tt - 0.25) * 5.0)
    rub = norm(band(RNG.standard_normal(len(tt)), lo=900, hi=3800))
    squeal = np.sin(2 * math.pi * (2150 + 40 * np.sin(2 * math.pi * 7 * tt)) * tt) * np.minimum(1, tt / 0.2)
    write("brake_01", env * (rub + 0.22 * squeal), gain=0.7)


def stand():
    tt = t_of(0.35)
    x = np.zeros_like(tt)
    for at, a in ((0.0, 1.0), (0.06, 0.45)):
        k = tt[tt >= at] - at
        s = (np.sin(2 * math.pi * 1850 * k) + 0.7 * np.sin(2 * math.pi * 4320 * k + 0.4) + 0.6 * RNG.standard_normal(len(k)) * np.exp(-k * 400)) * np.exp(-k * 38)
        x[tt >= at] += a * s
    write("stand_01", x, gain=0.7)


if __name__ == "__main__":
    tyre_asphalt()
    tyre_gravel()
    freewheel()
    chain()
    brake()
    stand()
