"""The sea railway's sounds, synthesized (no generation service, reproducible from this file and its seed):

  /Applications/Blender.app/Contents/MacOS/Blender -b --python-expr "" -P ... (numpy) or any python3 with numpy:
  python3 tools/railsim/rail_sounds.py            -> audio/sfx/railway/<id>_<nn>.wav (+ .json sidecar, loop flag)

then unreal/scripts/import_railway.sh imports them (IMPORT_ONLY=sfx/railway/ import_audio.py) into /Game/Audio/Railway.

  xing_bell_01   the level crossing's bell, the anime "kan-kan": a struck electric bell twice a second, loop
  motor_01       the traction motors' whine (one harmonic stack; the game pitches it with the speed), loop
  roll_01        the wheels' rumble on the rails, loop
  splash_01      the wheels in the water over the bank: hiss and patter, loop
  joint_01..03   the bogies over a rail joint: "ta-tan"
  horn_01        the electric car's air horn, two tones
  door_01, door_02   the doors: open (a hiss, the leaves running, a thump), shut
"""
import json
import math
import os
import struct
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "audio", "sfx", "railway")
SR = 44100
RNG = np.random.default_rng(1958)


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
        json.dump({"id": name, "source": "tools/railsim/rail_sounds.py", "loop": loop, "seconds": round(len(x) / SR, 3)}, f)
    print("RAILSOUND %s %.2f s%s" % (name, len(x) / SR, " loop" if loop else ""))


def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def lowpass(x, fc):
    a = math.exp(-2 * math.pi * fc / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i in range(len(x)):
        acc = (1 - a) * x[i] + a * acc
        y[i] = acc
    return y


def onepole_fft(x, lo=None, hi=None):
    """Band-limit by FFT (cheap, zero phase; fine for noise beds and loops: circular)."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    g = np.ones_like(f)
    if lo:
        g *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** 4)
    if hi:
        g *= 1 / np.sqrt(1 + (f / hi) ** 4)
    return np.fft.irfft(X * g, len(x))


def bell():
    # an electric crossing bell: a hammer strikes a gong ~2.3 times a second; partials of a thick bell, a fast attack,
    # ~0.35 s ring; alternate strikes a hair apart in tone (two gongs), a little room
    period = 0.43
    n = 8
    out = np.zeros(int(period * n * SR))
    partials = [(1.0, 1.0, 1.3), (2.76, 0.55, 2.2), (5.40, 0.25, 3.5), (8.93, 0.10, 5.0), (1.5, 0.15, 1.6)]
    for k in range(n):
        f0 = 742.0 if k % 2 == 0 else 735.0
        s = int(k * period * SR)
        tt = t_of(1.2)
        sig = np.zeros_like(tt)
        for ratio, amp, dec in partials:
            sig += amp * np.sin(2 * math.pi * f0 * ratio * tt + RNG.uniform(0, 6.28)) * np.exp(-tt * dec * 3.2)
        sig += 0.35 * RNG.standard_normal(len(tt)) * np.exp(-tt * 120)      # the hammer's click
        e = min(len(out), s + len(tt))
        out[s:e] += sig[:e - s]
        # wrap the tail round (a seamless loop)
        if s + len(tt) > len(out):
            rest = s + len(tt) - len(out)
            out[:rest] += sig[len(tt) - rest:]
    # a short slap echo off the cliff
    d = int(0.09 * SR)
    out = out + 0.25 * np.roll(out, d)
    write("xing_bell_01", out, loop=True)


def motor():
    # a DC traction motor's gear whine: a fundamental with its harmonics, a slow beat, a little brush noise
    sec = 2.0
    tt = t_of(sec)
    f = 180.0            # a whole number of cycles in the loop: 360
    x = np.zeros_like(tt)
    for h, a in ((1, 1.0), (2, 0.5), (3, 0.35), (5, 0.2), (7, 0.12), (11, 0.05)):
        x += a * np.sin(2 * math.pi * f * h * tt + h)
    x *= 1 + 0.15 * np.sin(2 * math.pi * 3.0 * tt)
    noise = onepole_fft(RNG.standard_normal(len(tt)), lo=800, hi=4000)
    x += 0.25 * noise / np.max(np.abs(noise))
    write("motor_01", x, loop=True, gain=0.7)


def roll():
    sec = 4.0
    tt = t_of(sec)
    x = onepole_fft(RNG.standard_normal(len(tt)), lo=40, hi=700)
    x /= np.max(np.abs(x))
    # a rhythmic flutter of the wheels' flats
    x *= 1 + 0.2 * np.sin(2 * math.pi * 6.0 * tt)
    hiss = onepole_fft(RNG.standard_normal(len(tt)), lo=2500, hi=9000)
    x += 0.12 * hiss / np.max(np.abs(hiss))
    write("roll_01", x, loop=True, gain=0.75)


def splash():
    sec = 4.0
    tt = t_of(sec)
    hiss = onepole_fft(RNG.standard_normal(len(tt)), lo=500, hi=7000)
    hiss /= np.max(np.abs(hiss))
    env = 0.6 + 0.4 * onepole_fft(RNG.standard_normal(len(tt)), hi=3)
    x = hiss * env / np.max(np.abs(env))
    # droplets: short blips falling back on the water
    for _ in range(160):
        s = RNG.integers(0, len(tt))
        f = RNG.uniform(900, 2600)
        d = t_of(0.04)
        blip = np.sin(2 * math.pi * (f + 6000 * d) * d) * np.exp(-d * 120) * RNG.uniform(0.3, 1.0)
        idx = (s + np.arange(len(d))) % len(tt)
        x[idx] += blip
    write("splash_01", x, loop=True, gain=0.7)


def joint(k):
    # two axles of a bogie over a rail joint, then the next bogie: ta-tan ... ta-tan
    sec = 1.3
    x = np.zeros(int(sec * SR))
    gap = 0.11 + 0.01 * k
    for i, at in enumerate((0.02, 0.02 + gap)):
        tt = t_of(0.25)
        click = RNG.standard_normal(len(tt)) * np.exp(-tt * 45)
        body = np.sin(2 * math.pi * (95 + 10 * k) * tt) * np.exp(-tt * 18)
        sig = onepole_fft(click, lo=150, hi=3500) * 0.8 + body
        s = int(at * SR)
        x[s:s + len(sig)] += sig * (1.0 if i == 0 else 0.85)
    write("joint_%02d" % (k + 1), x)


def horn():
    # two reeds a fourth apart (Japanese EMU air horn, "fuaaan"), a breathy onset, a soft cut
    sec = 1.6
    tt = t_of(sec)
    x = np.zeros_like(tt)
    for f in (349.0, 466.0):
        ph = 2 * math.pi * f * tt + 0.8 * np.sin(2 * math.pi * 5.5 * tt) * 0.02
        saw = sum(np.sin(h * ph) / h for h in range(1, 14))
        x += saw
    env = np.minimum(1, tt / 0.08) * np.minimum(1, (sec - tt) / 0.25)
    breath = onepole_fft(RNG.standard_normal(len(tt)), lo=600, hi=5000) * np.exp(-tt * 6) * 2
    x = onepole_fft(x * env + breath * env, lo=120, hi=3500)
    write("horn_01", x)


def door(opening):
    sec = 2.2
    tt = t_of(sec)
    hiss = onepole_fft(RNG.standard_normal(len(tt)), lo=1500, hi=9000)
    hiss = hiss / np.max(np.abs(hiss)) * np.exp(-tt * 4.0) * 0.6
    run = onepole_fft(RNG.standard_normal(len(tt)), lo=80, hi=600)
    run = run / np.max(np.abs(run))
    a, b = (0.15, 1.15) if opening else (0.05, 1.05)
    run *= ((tt > a) & (tt < b)) * 0.5
    thump = np.zeros_like(tt)
    s = int(b * SR)
    d = t_of(0.3)
    th = (np.sin(2 * math.pi * 70 * d) + 0.4 * RNG.standard_normal(len(d)) * np.exp(-d * 60)) * np.exp(-d * 14)
    thump[s:s + len(d)] = th[: len(tt) - s]
    write("door_%02d" % (1 if opening else 2), hiss + run + thump)


def main():
    bell()
    motor()
    roll()
    splash()
    for k in range(3):
        joint(k)
    horn()
    door(True)
    door(False)


if __name__ == "__main__":
    main()
