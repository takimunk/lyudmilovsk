"""Makes the festivals' sounds procedurally (numpy only, seeded: the same files every run), docs/festivals.md:

  audio/sfx/festival/firework_launch_01..02.wav   a mortar's thump and the shell's rising hiss
  audio/sfx/festival/firework_boom_01..03.wav     a shell bursting high over the sea: the crack, the boom, its echo off
                                                  the shore (03 with a crackling tail)
  audio/sfx/festival/bonfire_loop_01.wav          a beach bonfire: a low roar and popping logs, a seamless 10 s loop

    python3 tools/festival_sounds.py
then unreal/scripts/import_audio.py (IMPORT_ONLY=festival) imports them as /Game/Audio/Festival/S_<id>_<nn>.
Each wav gets a .json sidecar (generator, seed, sha256, loop). Released under CC BY 4.0 with the project's assets.
"""
import hashlib
import json
import os
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "audio", "sfx", "festival")
RATE = 48000


def band(x, lo, hi):
    """Keeps lo..hi Hz (FFT mask with soft edges)."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1.0 / RATE)
    m = np.clip((f - lo * 0.7) / (lo * 0.3 + 1e-9), 0, 1) if lo > 0 else np.ones_like(f)
    m *= np.clip((hi * 1.3 - f) / (hi * 0.3), 0, 1)
    return np.fft.irfft(np.fft.rfft(x) * m, n)


def env(n, attack, decay):
    t = np.arange(n) / RATE
    return np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-t / decay)


def norm(x, peak_db):
    x = x - np.mean(x)
    return x / (np.max(np.abs(x)) + 1e-9) * 10 ** (peak_db / 20.0)


def fade(x, a=0.005, b=0.04):
    na, nb = int(a * RATE), int(b * RATE)
    x[:na] *= np.linspace(0, 1, na)
    x[-nb:] *= np.linspace(1, 0, nb)
    return x


def launch(rng):
    n = int(2.0 * RATE)
    t = np.arange(n) / RATE
    thump = np.sin(2 * np.pi * (55 + 40 * np.exp(-t / 0.03)) * t) * env(n, 0.002, 0.09)
    thump += band(rng.standard_normal(n), 80, 900) * env(n, 0.001, 0.04) * 0.6
    # the hiss rising as the shell climbs and goes away: noise in a narrow band sliding up, swelling then fading
    hiss = np.zeros(n)
    noise = rng.standard_normal(n)
    for k in range(8):
        a, b = int(n * k / 8), int(n * (k + 1) / 8)
        seg = np.zeros(n)
        seg[a:b] = noise[a:b]
        c = 1400 + 2200 * (k / 7.0)
        hiss += band(seg, c * 0.8, c * 1.25)
    shape = np.clip((t - 0.05) / 0.25, 0, 1) * np.exp(-np.maximum(t - 0.4, 0) / 0.55)
    x = thump * 1.0 + hiss * shape * 0.35
    return fade(norm(x, -4.0))


def boom(rng, crackle):
    n = int(3.6 * RATE)
    t = np.arange(n) / RATE
    crack = rng.standard_normal(n) * env(n, 0.0005, 0.025)
    low = band(rng.standard_normal(n), 25, 260) * env(n, 0.004, 0.55)
    thud = np.sin(2 * np.pi * (38 + 30 * np.exp(-t / 0.08)) * t) * env(n, 0.003, 0.7)
    x = crack * 0.7 + low * 1.4 + thud * 0.9
    # the echo off the shore and the cliff: late, duller copies
    dry = x.copy()
    for delay, gain, top in ((0.33, 0.45, 1800), (0.82, 0.3, 900), (1.45, 0.18, 500)):
        d = int(delay * RATE)
        x[d:] += band(dry, 20, top)[: n - d] * gain
    if crackle:
        # a glittering tail: hundreds of tiny pops scattered over a second and a half
        pops = np.zeros(n)
        for _ in range(420):
            at = int((0.35 + rng.random() ** 0.7 * 1.6) * RATE)
            ln = int(0.004 * RATE)
            if at + ln < n:
                pops[at:at + ln] += rng.standard_normal(ln) * np.exp(-np.arange(ln) / (0.0012 * RATE)) * (0.3 + rng.random())
        x += band(pops, 1500, 9000) * 0.5
    return fade(norm(x, -2.0), 0.0005, 0.3)


def bonfire(rng):
    secs = 10.5
    n = int(secs * RATE)
    roar = band(np.cumsum(rng.standard_normal(n)) * 0.02 + rng.standard_normal(n) * 0.3, 40, 700)
    flutter = 1.0 + 0.25 * band(rng.standard_normal(n), 0.2, 3.0) / 0.01
    roar *= np.clip(flutter, 0.4, 1.6)
    pops = np.zeros(n)
    for _ in range(int(secs * 9)):
        at = rng.integers(0, n - 4000)
        ln = int((0.003 + rng.random() * 0.02) * RATE)
        pops[at:at + ln] += rng.standard_normal(ln) * np.exp(-np.arange(ln) / (ln * 0.25)) * (0.2 + rng.random() ** 2 * 1.5)
    x = roar / (np.max(np.abs(roar)) + 1e-9) * 0.5 + band(pops, 700, 7000) * 0.8
    # seamless: the last half second cross-faded into the start
    k = int(0.5 * RATE)
    head, tail = x[:k].copy(), x[-k:].copy()
    w = np.linspace(0, 1, k)
    x = x[:-k]
    x[:k] = head * w + tail * (1 - w)
    # loops by loudness (RMS -24 dBFS like the game's other loops), the pops softly limited
    x = x - np.mean(x)
    x = x / (np.sqrt(np.mean(x ** 2)) + 1e-9) * 10 ** (-24 / 20.0)
    return np.tanh(x * 2.0) / 2.0


def write(name, x, seed, loop=False):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
    side = {"id": name.rsplit("_", 1)[0], "kind": "sfx", "file": os.path.relpath(path, ROOT), "generator": "tools/festival_sounds.py",
            "seed": seed, "seconds": round(len(x) / RATE, 3), "sha256": sha, "loop": loop,
            "license": "Procedural (tools/festival_sounds.py); released by the project under CC BY 4.0."}
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, indent=1)
        f.write("\n")
    print("%s  %.2f s" % (os.path.relpath(path, ROOT), len(x) / RATE))


def main():
    for i in (1, 2):
        write("firework_launch_%02d" % i, launch(np.random.default_rng(5100 + i)), 5100 + i)
    for i in (1, 2, 3):
        write("firework_boom_%02d" % i, boom(np.random.default_rng(5200 + i), crackle=(i == 3)), 5200 + i)
    write("bonfire_loop_01", bonfire(np.random.default_rng(5301)), 5301, loop=True)


if __name__ == "__main__":
    main()
