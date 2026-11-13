"""Makes the North's loops procedurally (numpy only, seeded: the same files every run), docs/north.md "Game side":

  audio/sfx/north/north_vent_01.wav      a fumarole's steam jet: a hard broadband hiss with a low roar under it, fluttering
  audio/sfx/north/north_mudpot_01.wav    a mud pot: thick bubbles swelling and plopping now and then over a faint simmer
  audio/sfx/north/north_spring_01.wav    a hot spring: fine bubbles ticking up through still water
  audio/sfx/north/north_campfire_01.wav  the camp's fire: a soft roar and the crackle and pop of small logs

Every loop is mono, 48 kHz, 4..6 s and seamless by construction: the noise is filtered round the loop (FFT, circular),
slow wobbles have whole periods in it, and events near the end wrap round to the start.

    python3 tools/north_sounds.py
then unreal/scripts/import_audio.py (IMPORT_ONLY=sfx/north) imports them as /Game/Audio/North/S_<id>_<nn>, looping (the
sidecars say loop); ULyudSoundscapeSubsystem loads the pools north_vent, north_mudpot, north_spring, north_campfire.
Each wav gets a .json sidecar (generator, seed, sha256, loop). Released under CC BY 4.0 with the project's assets.
"""
import hashlib
import json
import os
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "audio", "sfx", "north")
RATE = 48000


def band(x, lo, hi):
    """Keeps lo..hi Hz (circular FFT mask with soft edges: a loop stays a loop)."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1.0 / RATE)
    m = np.clip((f - lo * 0.7) / (lo * 0.3 + 1e-9), 0, 1) if lo > 0 else np.ones_like(f)
    m *= np.clip((hi * 1.3 - f) / (hi * 0.3), 0, 1)
    return np.fft.irfft(np.fft.rfft(x) * m, n)


def tilt(x, db_per_octave, ref=1000.0):
    """A spectral slope round ref Hz (circular)."""
    n = len(x)
    f = np.maximum(np.fft.rfftfreq(n, 1.0 / RATE), 20.0)
    return np.fft.irfft(np.fft.rfft(x) * (f / ref) ** (db_per_octave / 6.02), n)


def slow(rng, n, hz):
    """A smooth random wobble 0-mean, ~unit size, under hz (circular)."""
    w = band(rng.standard_normal(n), 0.0, hz)
    return w / (np.std(w) + 1e-9)


def place(x, at, ev):
    """Adds ev into x at sample at, wrapping round the loop's end."""
    n = len(x)
    idx = (at + np.arange(len(ev))) % n
    np.add.at(x, idx, ev)


def loudness(x, rms_db=-24.0, drive=2.0):
    """Loops by loudness (RMS -24 dBFS like the game's other loops), peaks softly limited."""
    x = x - np.mean(x)
    x = x / (np.sqrt(np.mean(x ** 2)) + 1e-9) * 10 ** (rms_db / 20.0)
    return np.tanh(x * drive) / drive


def vent(rng):
    n = int(4.0 * RATE)
    hiss = tilt(band(rng.standard_normal(n), 250, 11000), 1.5, 2000)
    roar = band(rng.standard_normal(n), 60, 420)
    # the jet flutters: a fast turbulence and a slow surge (whole cycles in the loop)
    t = np.arange(n) / RATE
    surge = 1.0 + 0.12 * np.sin(2 * np.pi * t * 0.5) + 0.08 * np.sin(2 * np.pi * t * 1.25 + 1.0)
    turb = np.clip(1.0 + 0.22 * slow(rng, n, 9.0), 0.4, 1.8)
    x = (hiss / np.std(hiss) * 0.85 + roar / np.std(roar) * 0.45) * surge * turb
    return loudness(x, -22.0)


def mudpot(rng):
    n = int(5.0 * RATE)
    x = band(rng.standard_normal(n), 120, 1800) * 0.04          # the mud's faint simmer
    # thick bubbles: a swelling murmur, then the plop (a low resonance rising as the bubble opens) and a wet splat
    k = 0
    at = int(rng.uniform(0.1, 0.6) * RATE)
    while at < n and k < 9:
        size = rng.uniform(0.5, 1.0)
        ln = int(0.32 * RATE)
        tt = np.arange(ln) / RATE
        f0 = 70 + 90 * (1 - size) + rng.uniform(-10, 10)
        chirp = np.sin(2 * np.pi * np.cumsum(f0 * (1 + 2.2 * tt / 0.32)) / RATE)
        plop = chirp * np.minimum(1.0, tt / 0.004) * np.exp(-tt / (0.05 + 0.05 * size)) * size
        splat = band(rng.standard_normal(ln), 300, 3500) * np.exp(-tt / 0.012) * 0.35 * size
        swell = np.zeros(ln)
        sl = int(0.22 * RATE)
        swell[:sl] = band(rng.standard_normal(sl), 60, 400)[:sl] * np.linspace(0, 0.12, sl) * size
        ev = np.concatenate([swell[:sl], plop + splat])
        place(x, at, ev)
        k += 1
        at += int(rng.uniform(0.35, 0.9) * RATE)
    return loudness(x, -24.0, 1.6)


def spring(rng):
    n = int(4.0 * RATE)
    x = band(rng.standard_normal(n), 180, 2400) * 0.05 * np.clip(1 + 0.3 * slow(rng, n, 1.0), 0.3, 2.0)
    # fine bubbles rising: tiny tonal ticks, sometimes in little runs
    t = 0.0
    while t < 4.0:
        ln = int(0.03 * RATE)
        tt = np.arange(ln) / RATE
        f0 = rng.uniform(700, 2600)
        ev = np.sin(2 * np.pi * np.cumsum(f0 * (1 + 0.8 * tt / 0.03)) / RATE) * np.exp(-tt / rng.uniform(0.004, 0.01))
        place(x, int(t * RATE), ev * rng.uniform(0.15, 0.6))
        t += rng.exponential(0.05) + (rng.uniform(0.1, 0.4) if rng.random() < 0.15 else 0.0)
    return loudness(x, -27.0, 1.5)


def campfire(rng):
    n = int(6.0 * RATE)
    roar = band(tilt(rng.standard_normal(n), -4.0, 100.0), 40, 600)          # brownish, filtered round the loop
    roar -= np.mean(roar)
    roar *= np.clip(1.0 + 0.3 * slow(rng, n, 2.5), 0.4, 1.7)
    pops = np.zeros(n)
    for _ in range(int(6.0 * 14)):
        at = int(rng.integers(0, n))
        ln = int((0.002 + rng.random() * 0.012) * RATE)
        ev = rng.standard_normal(ln) * np.exp(-np.arange(ln) / (ln * 0.25)) * (0.15 + rng.random() ** 3 * 1.6)
        place(pops, at, ev)
    # a sizzle now and then (sap boiling out of a log)
    sizzle = band(rng.standard_normal(n), 2500, 9000) * np.clip(slow(rng, n, 0.8), 0, None) * 0.06
    x = roar / (np.max(np.abs(roar)) + 1e-9) * 0.45 + band(pops, 800, 9000) * 0.9 + sizzle
    return loudness(x, -24.0)


def seam_error(x):
    """How much the loop jumps where it wraps, relative to its usual sample-to-sample step."""
    d = np.abs(np.diff(x))
    return float(abs(x[0] - x[-1]) / (np.percentile(d, 99) + 1e-9))


def write(name, x, seed):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    sha = hashlib.sha256(open(path, "rb").read()).hexdigest()
    side = {"id": name.rsplit("_", 1)[0], "kind": "sfx", "file": os.path.relpath(path, ROOT), "generator": "tools/north_sounds.py",
            "seed": seed, "seconds": round(len(x) / RATE, 3), "sha256": sha, "loop": True,
            "license": "Procedural (tools/north_sounds.py); released by the project under CC BY 4.0."}
    with open(path[:-4] + ".json", "w", encoding="utf-8") as f:
        json.dump(side, f, indent=1)
        f.write("\n")
    print("%s  %.2f s  rms %.1f dBFS  peak %.1f dBFS  seam %.2f" % (
        os.path.relpath(path, ROOT), len(x) / RATE, 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-9),
        20 * np.log10(np.max(np.abs(x)) + 1e-9), seam_error(x)))


def main():
    write("north_vent_01", vent(np.random.default_rng(7101)), 7101)
    write("north_mudpot_01", mudpot(np.random.default_rng(7201)), 7201)
    write("north_spring_01", spring(np.random.default_rng(7301)), 7301)
    write("north_campfire_01", campfire(np.random.default_rng(7401)), 7401)


if __name__ == "__main__":
    main()
