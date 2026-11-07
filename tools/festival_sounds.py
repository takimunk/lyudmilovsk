"""Makes the festivals' sounds procedurally (numpy only, seeded: the same files every run), docs/festivals.md:

  audio/sfx/festival/firework_launch_01..02.wav   a mortar's thump and the shell's rising hiss
  audio/sfx/festival/firework_boom_01..03.wav     a shell bursting high over the sea: the crack, the boom, its echo off
                                                  the shore (03 with a crackling tail)
  audio/sfx/festival/bonfire_loop_01.wav          a beach bonfire: a low roar and popping logs, a seamless 10 s loop
  audio/sfx/festival/firecrackers_01..02.wav      a string of firecrackers going off: a fast ragged rattle of cracks (4-6 s)
  audio/sfx/festival/drums_loop_01.wav            festival drums (Lunar New Year, Holi): big drum, small drums, a cymbal, a
                                                  seamless 8-bar loop at 120 BPM
  audio/sfx/festival/bells_01.wav                 handbells ringing changes for the Christmas carols (12 s)
  audio/sfx/festival/crowd_loop_01.wav            a crowd talking on a square, nobody's words, a seamless 12 s loop
  audio/sfx/festival/cheer_01.wav                 a crowd cheering and clapping after the fireworks (5 s)

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


def firecrackers(rng):
    secs = 4.0 + rng.random() * 2.0
    n = int(secs * RATE)
    x = np.zeros(n)
    t = 0.05
    while t < secs - 0.3:
        at = int(t * RATE)
        ln = int(0.03 * RATE)
        # a crack: a hard noise burst with a little body, each a bit different
        k = rng.standard_normal(ln) * np.exp(-np.arange(ln) / (0.0025 * RATE)) * (0.5 + rng.random() * 0.7)
        body = np.sin(2 * np.pi * (180 + rng.random() * 220) * np.arange(ln) / RATE) * np.exp(-np.arange(ln) / (0.006 * RATE)) * 0.4
        x[at:at + ln] += k + body
        # ragged: bursts of fast ones, now and then a gap
        t += rng.exponential(0.035) + (0.18 if rng.random() < 0.04 else 0.0)
    x = band(x, 300, 12000) + band(x, 60, 300) * 0.6
    # the smoky echo off the buildings round the square
    dry = x.copy()
    for d, g in ((0.09, 0.35), (0.21, 0.2), (0.37, 0.1)):
        k = int(d * RATE)
        x[k:] += band(dry, 200, 4000)[: n - k] * g
    return fade(norm(x, -3.0), 0.001, 0.4)


def drums(rng):
    bpm, bars = 120.0, 8
    beat = 60.0 / bpm
    n = int(bars * 4 * beat * RATE)
    x = np.zeros(n)

    def hit(at, f0, decay, amp, noise=0.0, hi=4000):
        a = int(at * RATE)
        ln = min(int(decay * 6 * RATE), n - a)
        if ln <= 0:
            return
        tt = np.arange(ln) / RATE
        tone = np.sin(2 * np.pi * (f0 + f0 * 0.6 * np.exp(-tt / 0.02)) * tt) * np.exp(-tt / decay)
        if noise:
            tone += band(rng.standard_normal(ln), 200, hi) * np.exp(-tt / (decay * 0.3)) * noise
        x[a:a + ln] += tone * amp

    for b in range(bars * 4):
        t0 = b * beat
        # the big drum on one and three, the small ones in eighths with accents, a cymbal crash every two bars
        if b % 2 == 0:
            hit(t0, 55, 0.35, 1.0, 0.2, 800)
        for e in range(2):
            acc = 1.0 if e == 0 else 0.55
            hit(t0 + e * beat / 2 + rng.normal(0, 0.004), 190 + rng.random() * 15, 0.08, 0.45 * acc, 0.5, 3000)
        if b % 8 == 0:
            a = int(t0 * RATE)
            ln = min(int(2.0 * RATE), n - a)
            x[a:a + ln] += band(rng.standard_normal(ln), 3000, 12000) * np.exp(-np.arange(ln) / (0.6 * RATE)) * 0.35
    # seamless: the tail of the last bar folded back onto the start
    return fade(norm(x, -4.0), 0.0005, 0.005)


def bells(rng):
    n = int(12.0 * RATE)
    x = np.zeros(n)
    # six handbells (D major), rung in rounds and a few changes
    notes = [587.33, 659.25, 739.99, 783.99, 880.0, 987.77]
    order = list(range(6))
    t = 0.1
    for row in range(14):
        if row >= 2:
            i = rng.integers(0, 5)
            order[i], order[i + 1] = order[i + 1], order[i]
        for k in order:
            a = int(t * RATE)
            ln = min(int(3.0 * RATE), n - a)
            if ln <= 0:
                break
            tt = np.arange(ln) / RATE
            f = notes[5 - k]
            # a bell's partials: hum, prime, tierce, quint, nominal
            tone = sum(g * np.sin(2 * np.pi * f * r * tt) * np.exp(-tt / d) for r, g, d in
                       ((0.5, 0.35, 1.6), (1.0, 1.0, 1.1), (1.2, 0.45, 0.7), (1.5, 0.3, 0.5), (2.0, 0.5, 0.4)))
            x[a:a + ln] += tone * (0.7 + rng.random() * 0.3) * np.minimum(1.0, tt / 0.002)
            t += 0.135
        t += 0.135
    return fade(norm(x, -6.0), 0.005, 1.0)


def crowd(rng):
    secs = 12.5
    n = int(secs * RATE)
    x = np.zeros(n)
    # dozens of voices: each a babble of formant-shaped noise syllables at its own pitch and pace
    for v in range(40):
        f0 = rng.uniform(95, 260)
        t = rng.random()
        while t < secs:
            ln = int(rng.uniform(0.08, 0.25) * RATE)
            a = int(t * RATE)
            if a + ln >= n:
                break
            tt = np.arange(ln) / RATE
            syl = np.sin(2 * np.pi * f0 * (1 + 0.05 * rng.standard_normal()) * tt) + 0.6 * rng.standard_normal(ln)
            x[a:a + ln] += syl * np.sin(np.pi * tt / tt[-1]) * rng.uniform(0.2, 1.0) / (1 + v * 0.05)
            t += ln / RATE + rng.exponential(0.12) + (rng.uniform(0.5, 2.0) if rng.random() < 0.08 else 0)
    x = band(x, 250, 900) * 1.0 + band(x, 900, 3200) * 0.45
    # a few laughs and calls standing out
    for _ in range(4):
        a = int(rng.uniform(0.5, secs - 1.5) * RATE)
        ln = int(0.6 * RATE)
        tt = np.arange(ln) / RATE
        f = rng.uniform(220, 380)
        x[a:a + ln] += band(np.sin(2 * np.pi * f * tt * (1 + 0.08 * np.sin(2 * np.pi * 7 * tt))) + 0.3 * rng.standard_normal(ln), 300, 3000) * \
            np.abs(np.sin(2 * np.pi * 4.5 * tt)) * np.exp(-tt / 0.4) * 0.5 * np.std(x) * 6
    k = int(0.5 * RATE)
    head, tail = x[:k].copy(), x[-k:].copy()
    w = np.linspace(0, 1, k)
    x = x[:-k]
    x[:k] = head * w + tail * (1 - w)
    x = x - np.mean(x)
    return x / (np.sqrt(np.mean(x ** 2)) + 1e-9) * 10 ** (-26 / 20.0)


def cheer(rng):
    n = int(5.0 * RATE)
    t = np.arange(n) / RATE
    swell = np.minimum(1.0, t / 0.4) * np.exp(-np.maximum(t - 1.2, 0) / 1.4)
    roar = band(rng.standard_normal(n), 300, 3500) * swell
    # "woo" calls: gliding voices
    for _ in range(14):
        a = int(rng.uniform(0.0, 1.5) * RATE)
        ln = min(int(rng.uniform(0.5, 1.2) * RATE), n - a)
        tt = np.arange(ln) / RATE
        f = rng.uniform(300, 700) * (1 + 0.4 * tt / tt[-1])
        roar[a:a + ln] += np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.sin(np.pi * tt / tt[-1]) * 0.25
    # clapping: many hands, each a short burst
    claps = np.zeros(n)
    for _ in range(900):
        a = int(rng.uniform(0.3, 4.6) ** 1.0 * RATE)
        ln = int(0.012 * RATE)
        if a + ln < n:
            claps[a:a + ln] += rng.standard_normal(ln) * np.exp(-np.arange(ln) / (0.002 * RATE)) * (1.4 - a / n)
    x = roar + band(claps, 800, 9000) * 0.5
    return fade(norm(x, -5.0), 0.01, 0.8)


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
    for i in (1, 2):
        write("firecrackers_%02d" % i, firecrackers(np.random.default_rng(5400 + i)), 5400 + i)
    write("drums_loop_01", drums(np.random.default_rng(5501)), 5501, loop=True)
    write("bells_01", bells(np.random.default_rng(5601)), 5601)
    write("crowd_loop_01", crowd(np.random.default_rng(5701)), 5701, loop=True)
    write("cheer_01", cheer(np.random.default_rng(5801)), 5801)


if __name__ == "__main__":
    main()
