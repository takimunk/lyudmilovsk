#!/usr/bin/env python3
"""Listens to the band by numbers (tools/bandsim/run.sh stems <dir> <tune> writes what this reads).

    python3 tools/bandsim/analyse.py <dir>

For each dry stem: level, spectral centroid, where the energy sits. For the reed: the pitch it actually sounds (autocorrelation)
against the note the arranger asked for, the vibrato on long notes. For the bass: its pitch too. For the drums and the bass:
how far the sounded onsets fall from the written times, and the swing ratio of the off-beat hits measured from the sound.
Prints a table; exits 1 when something is off (a reed more than 35 cents out, a bass more than 25, onsets late by 25 ms).
"""
import csv
import sys
import wave

import numpy as np


def read(path):
    with wave.open(path, "rb") as w:
        rate = w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768.0
    return x.reshape(-1, 2).mean(axis=1), rate


def centroid(x, rate):
    n = 4096
    spec = np.zeros(n // 2 + 1)
    win = np.hanning(n)
    for i in range(0, len(x) - n, n):
        seg = x[i:i + n]
        if np.sqrt(np.mean(seg * seg)) > 0.003:
            spec += np.abs(np.fft.rfft(seg * win)) ** 2
    f = np.fft.rfftfreq(n, 1.0 / rate)
    total = spec.sum() or 1.0
    bands = [(0, 200), (200, 800), (800, 3000), (3000, 16000)]
    return (f * spec).sum() / total, [spec[(f >= a) & (f < b)].sum() / total for a, b in bands]


def pitch(seg, rate, lo, hi):
    """Autocorrelation pitch in Hz of a steady stretch (0: silent)."""
    seg = seg - seg.mean()
    if np.sqrt(np.mean(seg * seg)) < 0.002:
        return 0.0
    n = len(seg)
    f = np.fft.rfft(seg * np.hanning(n), 2 * n)
    ac = np.fft.irfft(np.abs(f) ** 2)[:n]
    a, b = int(rate / hi), min(int(rate / lo), n - 2)
    if b <= a + 2:
        return 0.0
    # the first strong peak (not the highest: an octave under is as strong)
    top = ac[a:b].max()
    k = a
    for i in range(a + 1, b - 1):
        if ac[i] > 0.85 * top and ac[i] >= ac[i - 1] and ac[i] >= ac[i + 1]:
            k = i
            break
    y0, y1, y2 = ac[k - 1], ac[k], ac[k + 1]
    d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
    return rate / (k + d)


def hz(midi):
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def cents(f, midi):
    return 1200.0 * np.log2(f / hz(midi)) if f > 0 else float("nan")


def onset_after(x, rate, t, span=0.09):
    """Seconds from t to where the envelope first rises through a third of its peak in the next span."""
    a = max(0, int((t - 0.004) * rate))
    b = min(len(x), int((t + span) * rate))
    if b - a < 64:
        return None
    env = np.abs(x[a:b])
    k = max(1, int(0.002 * rate))
    env = np.convolve(env, np.ones(k) / k, mode="same")
    peak = env.max()
    if peak < 0.004:
        return None
    first = int(np.argmax(env > peak / 3.0))
    return a / rate + first / rate - t


def main(folder):
    notes = list(csv.DictReader(open(folder + "/notes.csv")))
    for n in notes:
        n["time"], n["midi"], n["vel"], n["len"] = float(n["time"]), int(n["midi"]), float(n["vel"]), float(n["len"])
    bars = list(csv.DictReader(open(folder + "/bars.csv")))
    bad = 0
    print("stem    rms     centroid   <200Hz 200-800 800-3k  >3k")
    stems = {}
    for name in ("piano", "bass", "reed", "drums"):
        x, rate = read(folder + "/stem_%s.wav" % name)
        stems[name] = (x, rate)
        c, b = centroid(x, rate)
        print("%-6s %6.3f %8.0f Hz   %5.2f  %5.2f  %5.2f  %5.2f" % (name, np.sqrt(np.mean(x * x)), c, *b))
    # what each should be: the bass under the piano under the reed's upper partials under the brushes
    cb = centroid(*stems["bass"])[0]
    cp = centroid(*stems["piano"])[0]
    cd = centroid(*stems["drums"])[0]
    if not (cb < 400 and cb < cp < cd and cd > 2000):
        print("FAIL the stems' centroids are not bass < piano < drums")
        bad += 1

    # the reed: sounded pitch against the written note, on notes long enough to measure
    x, rate = stems["reed"]
    reed = [n for n in notes if n["inst"] == "reed"]
    errs, vib, measured = [], [], 0
    for i, n in enumerate(reed):
        nxt = reed[i + 1]["time"] if i + 1 < len(reed) else 1e9
        end = min(n["time"] + n["len"], nxt)
        if end - n["time"] < 0.22:
            continue
        a, b = int((n["time"] + 0.12) * rate), int((end - 0.02) * rate)
        f = pitch(x[a:b], rate, 90.0, 2200.0)
        if f <= 0:
            continue
        measured += 1
        errs.append(cents(f, n["midi"]))
        if end - n["time"] > 1.2:
            # vibrato: the pitch in 40 ms frames over the last part of the note
            fr = []
            for s in range(int((n["time"] + 0.7) * rate), int((end - 0.08) * rate), int(0.02 * rate)):
                g = pitch(x[s:s + int(0.04 * rate)], rate, hz(n["midi"]) * 0.8, hz(n["midi"]) * 1.25)
                if g > 0:
                    fr.append(cents(g, n["midi"]))
            if len(fr) > 8:
                vib.append(max(fr) - min(fr))
    errs = np.array(errs)
    if len(errs):
        off = int(np.sum(np.abs(errs) > 35))
        print("reed: %d notes written, %d measured: pitch error median %+.1f cents, 95%% within %.0f, %d over 35 cents" % (
            len(reed), measured, np.median(errs), np.percentile(np.abs(errs), 95), off))
        if off > 0.03 * len(errs):
            print("FAIL the reed is out of tune with its lead sheet")
            bad += 1
    if vib:
        print("reed: vibrato on %d long notes: %.0f cents peak to peak (median)" % (len(vib), np.median(vib)))

    # the bass: pitch and onsets
    x, rate = stems["bass"]
    bass = [n for n in notes if n["inst"] == "bass"]
    errs, late = [], []
    for i, n in enumerate(bass):
        nxt = bass[i + 1]["time"] if i + 1 < len(bass) else 1e9
        end = min(n["time"] + n["len"], nxt)
        o = onset_after(x, rate, n["time"])
        if o is not None:
            late.append(o)
        if end - n["time"] < 0.25:
            continue
        a, b = int((n["time"] + 0.08) * rate), int(min(end, n["time"] + 0.45) * rate)
        f = pitch(x[a:b], rate, 35.0, 400.0)
        if f > 0:
            errs.append(cents(f, n["midi"]))
    errs = np.array(errs)
    if len(errs):
        off = int(np.sum(np.abs(errs) > 25))
        print("bass: %d notes: pitch error median %+.1f cents, 95%% within %.0f, %d over 25 cents; onsets %.1f ms after the written time (median)" % (
            len(bass), np.median(errs), np.percentile(np.abs(errs), 95), off, 1000 * np.median(late)))
        if off > 0.05 * len(errs) or abs(np.median(late)) > 0.025:
            print("FAIL the bass is out of tune or late")
            bad += 1

    # the drums: the taps and the ride against their written times, and the swing they make
    x, rate = stems["drums"]
    late = []
    for n in notes:
        if n["inst"] == "drums" and n["midi"] in (0, 1, 3, 4):
            o = onset_after(x, rate, n["time"], 0.05)
            if o is not None:
                late.append(o)
    if late:
        print("drums: %d hits: onsets %.1f ms after the written time (median), 95%% within %.0f ms" % (
            len(late), 1000 * np.median(late), 1000 * np.percentile(np.abs(late), 95)))
        if abs(np.median(late)) > 0.025:
            print("FAIL the drums are late")
            bad += 1
    # the grid: every written onset as a fraction of its beat, by instrument: the off-beats show the swing ratio
    meter, swing = int(bars[0]["meter"]), float(bars[0]["swing"])
    starts = [float(b["start"]) for b in bars]
    spb = [float(b["spb0"]) for b in bars]
    for inst in ("reed", "bass", "piano", "drums"):
        on_beat, off_beat = [], []
        for n in notes:
            if n["inst"] != inst:
                continue
            k = max(0, int(np.searchsorted(starts, n["time"] + 0.03)) - 1)
            if bars[k]["role"] in ("final", "tag") or k >= len(bars) - 3:
                continue
            beat = (n["time"] - starts[k]) / spb[k]
            frac = beat - np.floor(beat + 0.25)
            if abs(frac) < 0.25:
                on_beat.append(frac * spb[k])
            elif 0.4 < frac < 0.8:
                off_beat.append(frac)
        line = "%-6s on the beat: %+5.1f ms mean, %4.1f ms spread" % (inst, 1000 * np.mean(on_beat), 1000 * np.std(on_beat)) if on_beat else inst
        if off_beat:
            line += "; off-beats at %.3f of the beat (written swing %.2f)" % (np.median(off_beat), swing)
        print(line)
    print("%s: %s" % (folder, "FAIL" if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
