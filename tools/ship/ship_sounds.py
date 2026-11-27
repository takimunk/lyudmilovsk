"""The motor ship's sounds, synthesized (no generation service, reproducible from this file and its seed), after
tools/railsim/rail_sounds.py:

  python3 tools/ship/ship_sounds.py            -> audio/sfx/ship/<id>_01.wav (+ .json sidecar, loop flag)
  python3 tools/ship/ship_sounds.py --check    also checks every file: level, no clipping, the loops' seams

then unreal/scripts/import_cruise.sh imports them (IMPORT_ONLY=sfx/ship/ import_audio.py) into /Game/Audio/Ship, the
pools of ULyudSoundscapeSubsystem's group "Ship" (ULyudCruiseScenery and the ship's acts play them).

  ship_hum      the diesel through the hull: two engines' firing harmonics beating slowly, loop
  ship_wash     the bow wave: surging hiss, loop
  ship_horn     one long blast of the typhon (4.5 s);  ship_horn3: three long blasts (the departure)
  ship_bell     one stroke of the ship's bell
  ship_chain    the anchor's chain running out through the hawse and the windlass
  ship_rigging  wind in the wires and halyards, a halyard tapping the mast, loop
  ship_creak    the cabins' joinery working as she rolls, loop
  ship_engine   the engine gallery's roar when the door is opened (4 s)
  ship_telegraph  the engine telegraph: the ratchet and its two bells
  gulls_feed    gulls squabbling over thrown food (6 s)
"""
import json
import math
import os
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "audio", "sfx", "ship")
SR = 44100
RNG = np.random.default_rng(1962)
MADE = []


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
        json.dump({"id": name, "source": "tools/ship/ship_sounds.py", "loop": loop, "seconds": round(len(x) / SR, 3)}, f)
    MADE.append((name, x, loop))
    print("SHIPSOUND %s %.2f s%s" % (name, len(x) / SR, " loop" if loop else ""))


def t_of(sec):
    return np.arange(int(sec * SR)) / SR


def band(n, lo, hi, tilt=0.0, soft=0.25):
    """Periodic noise of n samples between lo and hi Hz (made in the spectrum, so it loops without a seam);
    tilt: dB per octave over lo."""
    f = np.fft.rfftfreq(n, 1.0 / SR)
    spec = (RNG.standard_normal(len(f)) + 1j * RNG.standard_normal(len(f)))
    w = np.zeros(len(f))
    ok = f > 0
    lf = np.log2(np.maximum(f, 1e-6))
    a, b = math.log2(lo), math.log2(hi)
    w[ok] = np.clip((lf[ok] - (a - soft)) / soft, 0, 1) * np.clip(((b + soft) - lf[ok]) / soft, 0, 1)
    w[ok] *= 10 ** (tilt * (lf[ok] - a) / 20.0)
    x = np.fft.irfft(spec * w, n)
    return x / (np.max(np.abs(x)) or 1.0)


def filt(x, lo, hi, soft=0.25):
    """Band-limits a one-shot (FFT, with soft edges)."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    lf = np.log2(np.maximum(f, 1e-6))
    a, b = math.log2(lo), math.log2(hi)
    w = np.clip((lf - (a - soft)) / soft, 0, 1) * np.clip(((b + soft) - lf) / soft, 0, 1)
    return np.fft.irfft(np.fft.rfft(x) * w, n)


def fade(x, a=0.01, b=0.05):
    n = len(x)
    na, nb = int(a * SR), int(b * SR)
    e = np.ones(n)
    e[:na] = np.linspace(0, 1, na)
    e[n - nb:] *= np.linspace(1, 0, nb)
    return x * e


def hum(sec=8.0, name="ship_hum", clatter=0.0, loop=True):
    # two six-cylinder diesels at ~300 rpm: firing at 15.0 and 15.25 Hz (whole cycles in 8 s), their harmonics beating
    tt = t_of(sec)
    x = np.zeros_like(tt)
    for f0, ph in ((15.0, 0.0), (15.25, 1.1)):
        for h in range(1, 28):
            amp = (1.0 / h ** 0.9) * (1.6 if h in (2, 4, 6) else 1.0) * (0.55 if h == 1 else 1.0)
            x += amp * np.sin(2 * math.pi * f0 * h * tt + ph * h + RNG.uniform(0, 6.28))
    x /= np.max(np.abs(x))
    n = len(tt)
    rumble = band(n, 25, 180, tilt=-4)
    air = band(n, 300, 2500, tilt=-5) * 0.12
    # the hull breathes with the beat
    beat = 0.8 + 0.2 * np.sin(2 * math.pi * 0.25 * tt)
    y = x * beat + 0.45 * rumble + air
    if clatter > 0:
        # valve gear and injectors heard in the engine room: ticks at the firing rate
        tick = band(n, 1500, 7000) * (0.5 + 0.5 * np.sign(np.sin(2 * math.pi * 30.0 * tt))) * np.abs(np.sin(2 * math.pi * 15.0 * tt)) ** 6
        y = y + clatter * (tick + 0.5 * band(n, 400, 3000, tilt=-3))
    if not loop:
        y = fade(y, 0.25, 0.9)
    write(name + "_01", y, loop)


def wash():
    sec = 10.0
    tt = t_of(sec)
    n = len(tt)
    hiss = band(n, 500, 7000, tilt=-3)
    body = band(n, 120, 700, tilt=-2)
    # surges as the stem meets each sea (periods that divide the loop)
    s1 = 0.5 + 0.5 * np.sin(2 * math.pi * tt / 2.5)
    s2 = 0.5 + 0.5 * np.sin(2 * math.pi * tt / (10.0 / 3.0) + 1.0)
    env = 0.45 + 0.35 * s1 ** 2 + 0.2 * s2
    write("ship_wash_01", hiss * env + 0.7 * body * (0.6 + 0.4 * s2), True)


def blast(sec):
    # a typhon: two low reeds a major third apart, rich in harmonics, a slow start, the note sagging as the air goes
    tt = t_of(sec)
    x = np.zeros_like(tt)
    sag = 1.0 - 0.012 * (tt / sec)
    for f0, lvl in ((131.0, 1.0), (164.5, 0.8)):
        ph = 2 * math.pi * np.cumsum(f0 * sag) / SR
        for h in range(1, 40):
            if f0 * h > 9000:
                break
            form = 1.0 / (1 + ((f0 * h - 420.0) / 500.0) ** 2) + 0.25 / (1 + ((f0 * h - 1500.0) / 900.0) ** 2)
            x += lvl * form / h ** 0.35 * np.sin(h * ph + RNG.uniform(0, 6.28))
    env = np.minimum(tt / 0.18, 1.0) ** 1.5 * np.minimum((sec - tt) / 0.5, 1.0) ** 1.2
    x = x * env * (1 + 0.03 * np.sin(2 * math.pi * 5.3 * tt))
    x += 0.05 * RNG.standard_normal(len(tt)) * env        # the air
    return x / np.max(np.abs(x))


def echo(x, tail=2.0):
    # the blast coming back off the water and the land
    y = np.concatenate([x, np.zeros(int(tail * SR))])
    for d, g in ((0.31, 0.22), (0.74, 0.13), (1.25, 0.07)):
        k = int(d * SR)
        y[k:] += g * filt(np.concatenate([x, np.zeros(int(tail * SR))]), 80, 1800)[: len(y) - k]
    return y


def horn():
    one = blast(4.5)
    write("ship_horn_01", echo(one))
    three = np.zeros(int((3 * 4.2 + 2 * 1.6) * SR))
    for k in range(3):
        b = blast(4.2)
        s = int(k * (4.2 + 1.6) * SR)
        three[s:s + len(b)] += b
    write("ship_horn3_01", echo(three))


def stroke(f0, sec, bright=1.0):
    # a bronze bell: hum, prime, tierce, quint, nominal and the high ring, each dying at its own rate
    tt = t_of(sec)
    x = np.zeros_like(tt)
    for ratio, amp, dec in ((0.5, 0.35, 1.2), (1.0, 1.0, 2.2), (1.19, 0.5, 2.8), (1.5, 0.3, 3.5), (2.0, 0.7, 3.6), (2.51, 0.3 * bright, 5.0),
                            (3.01, 0.22 * bright, 6.5), (4.17, 0.15 * bright, 9.0), (5.43, 0.1 * bright, 12.0)):
        x += amp * np.sin(2 * math.pi * f0 * ratio * tt + RNG.uniform(0, 6.28)) * np.exp(-tt * dec) * (1 + 0.1 * np.sin(2 * math.pi * 3.1 * ratio * tt))
    clap = filt(RNG.standard_normal(len(tt)) * np.exp(-tt * 180), 1500, 9000) * 0.5
    return x + clap


def bell():
    write("ship_bell_01", fade(stroke(985.0, 3.2), 0.001, 0.3))


def telegraph():
    sec = 2.0
    tt = t_of(sec)
    x = np.zeros_like(tt)
    # the handle's ratchet, then the two answering bells
    for k in range(7):
        s = int((0.05 + k * 0.055) * SR)
        d = t_of(0.03)
        c = filt(RNG.standard_normal(len(d)), 1800, 7000) * np.exp(-d * 240)
        x[s:s + len(d)] += 0.35 * c
    for k, at in enumerate((0.55, 0.85)):
        b = stroke(2350.0 * (1.0 if k == 0 else 1.005), 1.1, bright=1.5)
        s = int(at * SR)
        x[s:s + len(b)] += b[: len(x) - s]
    write("ship_telegraph_01", fade(x, 0.001, 0.2))


def chain():
    sec = 7.0
    tt = t_of(sec)
    x = np.zeros_like(tt)
    # the anchor goes: a splash, then the links run over the gypsy fast and slower, and the brake comes on
    sp = t_of(1.2)
    x[: len(sp)] += 0.5 * filt(RNG.standard_normal(len(sp)), 300, 5000) * np.exp(-sp * 3.0) * np.minimum(sp / 0.05, 1)
    at = 0.25
    rate = 13.0
    while at < sec - 0.6:
        d = t_of(0.09)
        f = RNG.uniform(900, 1700)
        link = np.zeros_like(d)
        for r, a in ((1.0, 1.0), (1.47, 0.7), (2.09, 0.5), (2.56, 0.45), (3.9, 0.3)):
            link += a * np.sin(2 * math.pi * f * r * d + RNG.uniform(0, 6.28)) * np.exp(-d * RNG.uniform(45, 80))
        link += 0.8 * filt(RNG.standard_normal(len(d)), 600, 6000) * np.exp(-d * 120)
        s = int(at * SR)
        x[s:s + len(d)] += link[: len(x) - s] * RNG.uniform(0.5, 1.0)
        rate = max(3.0, rate * (0.985 if at < 4.5 else 0.93))
        at += (1.0 / rate) * RNG.uniform(0.8, 1.2)
    n = len(tt)
    rumble = band(n, 40, 260, tilt=-3) * np.minimum(tt / 0.3, 1) * np.clip((sec - 0.8 - tt) / 1.5, 0, 1)
    write("ship_chain_01", fade(x + 0.55 * rumble, 0.002, 0.4))


def rigging():
    sec = 8.0
    tt = t_of(sec)
    n = len(tt)
    low = band(n, 60, 500, tilt=-4) * (0.7 + 0.3 * np.sin(2 * math.pi * tt / 4.0))
    x = 0.6 * low
    # the wires sing: narrow bands that swell in the gusts (their periods divide the loop)
    for f0, per, ph in ((640.0, 4.0, 0.3), (910.0, 8.0, 2.0), (1320.0, 8.0 / 3.0, 4.0)):
        tone = band(n, f0 * 0.97, f0 * 1.03, soft=0.03)
        x += 0.5 * tone * (0.5 + 0.5 * np.sin(2 * math.pi * tt / per + ph)) ** 2
    # a halyard taps the mast
    for at in (0.5, 0.92, 1.3, 3.1, 3.5, 5.2, 5.6, 6.05, 7.1):
        d = t_of(0.12)
        tap = (np.sin(2 * math.pi * 520 * d) + 0.5 * np.sin(2 * math.pi * 1370 * d)) * np.exp(-d * 55) * RNG.uniform(0.15, 0.3)
        s = int(at * SR)
        x[s:s + len(d)] += tap
    write("ship_rigging_01", x, True)


def creak():
    sec = 12.0
    tt = t_of(sec)
    n = len(tt)
    x = 0.04 * band(n, 80, 600, tilt=-5)
    # wood on wood, stick and slip: a slow sawtooth through the panel's resonances, a few times in the loop
    for at, dur, f0, f1 in ((0.8, 0.7, 55, 95), (3.4, 0.45, 80, 60), (5.1, 0.9, 40, 78), (7.7, 0.35, 110, 140), (9.6, 0.8, 62, 48), (10.9, 0.3, 90, 120)):
        d = t_of(dur)
        f = f0 + (f1 - f0) * (d / dur) ** 1.5 + 4 * np.sin(2 * math.pi * 7 * d)
        ph = np.cumsum(f) / SR
        saw = 2 * (ph - np.floor(ph)) - 1
        tone = filt(saw, 500, 2600) + 0.5 * filt(saw, 1500, 5200)
        env = np.sin(math.pi * d / dur) ** 0.7
        s = int(at * SR)
        x[s:s + len(d)] += 0.6 * tone / (np.max(np.abs(tone)) or 1) * env * RNG.uniform(0.5, 1.0)
    write("ship_creak_01", x, True, gain=0.6)


def gulls():
    sec = 6.0
    tt = t_of(sec)
    x = np.zeros_like(tt)
    # a dozen calls from several birds: each a nasal cry whose pitch climbs and falls, some in laughing runs
    at = 0.05
    while at < sec - 0.7:
        base = RNG.uniform(780, 1250)
        run = int(RNG.integers(1, 5))
        for k in range(run):
            dur = RNG.uniform(0.16, 0.42) if run > 1 else RNG.uniform(0.4, 0.75)
            d = t_of(dur)
            u = d / dur
            f = base * (0.8 + 0.55 * np.sin(math.pi * u ** 0.7) - 0.12 * u) * (1 + 0.015 * np.sin(2 * math.pi * 38 * d))
            ph = 2 * math.pi * np.cumsum(f) / SR
            cry = np.zeros_like(d)
            for h in range(1, 9):
                form = 1.0 / (1 + ((f.mean() * h - 2600.0) / 1100.0) ** 2) + 0.3 / h
                cry += form * np.sin(h * ph + 0.6 * h * np.sin(ph))
            cry *= np.sin(math.pi * u) ** 0.6 * RNG.uniform(0.35, 1.0)
            s = int(at * SR)
            if s + len(d) < len(x):
                x[s:s + len(d)] += cry / (np.max(np.abs(cry)) or 1)
            at += dur * RNG.uniform(0.75, 1.0)
            base *= RNG.uniform(0.94, 1.03)
        at += RNG.uniform(-0.25, 0.3)
        at = max(at, 0.05)
    wings = band(len(tt), 200, 1500, tilt=-4) * 0.06
    write("gulls_feed_01", fade(filt(x, 500, 9000) + wings, 0.01, 0.3))


def check():
    bad = 0
    for name, x, loop in MADE:
        peak = float(np.max(np.abs(x)))
        rms = float(np.sqrt(np.mean(x ** 2)))
        seam = abs(float(x[0] - x[-1])) if loop else 0.0
        step = float(np.max(np.abs(np.diff(x))))
        ok = peak <= 0.86 and rms > 0.02 and (not loop or seam <= max(step, 0.02))
        print("SHIPSOUNDCHECK %-18s peak %.2f rms %.3f%s %s" % (name, peak, rms, " seam %.3f (largest step inside %.3f)" % (seam, step) if loop else "", "ok" if ok else "FAIL"))
        bad += 0 if ok else 1
    if bad:
        raise SystemExit("SHIPSOUNDCHECK: %d failed" % bad)


def main():
    hum()
    hum(4.0, "ship_engine", clatter=0.6, loop=False)
    wash()
    horn()
    bell()
    telegraph()
    chain()
    rigging()
    creak()
    gulls()
    if "--check" in sys.argv:
        check()


if __name__ == "__main__":
    main()
