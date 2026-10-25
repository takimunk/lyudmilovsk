"""Lip-sync curves for every voice line, from its waveform (docs/characters.md "Face").

    python3 tools/lipsync.py [--lang ru,en,ja]

Reads audio/voice/<lang>/<line key>.wav (tools/gen_voice.py) and writes
unreal/Lyudmilovsk/Content/Dialogue/lipsync_<lang>.json: {"fps": 30, "lines": {key: {"jaw": "...", "funnel": "...",
"smile": "..."}}}, each curve a string of digits 0-9 (0 = closed / none, 9 = full) a frame, read by
ULyudBodyAnimInstance::Speak while the line plays: the morph targets jawOpen, mouthFunnel, mouthSmile
(blender/characters/face_rig.py).

How, per 1/30 s frame (a 40 ms Hann window):
  jaw     the loudness: RMS in dB over the line's own range (its 95th percentile down 30 dB), shaped so quiet speech
          barely parts the lips, with a fast attack and a slower release (a mouth opens quicker than it closes), 0..0.5
  funnel  rounded vowels (o, u): energy low in the spectrum (the spectral centroid under ~900 Hz) while voiced
  smile   spread vowels and hissing (i, e, s, sh): a high centroid (over ~2.2 kHz)
Silence closes the mouth; the curves end at 0.
"""
import argparse
import json
import os

import warnings

import numpy as np
from scipy.io import wavfile

warnings.simplefilter("ignore", wavfile.WavFileWarning)

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FPS = 30


def curves(path):
    sr, x = wavfile.read(path)
    x = x.astype(np.float32)
    if x.ndim > 1:
        x = x.mean(1)
    x /= max(1.0, np.abs(x).max())
    hop = sr // FPS
    win = int(sr * 0.04)
    w = np.hanning(win).astype(np.float32)
    n = max(1, (len(x) - win) // hop + 1)
    freqs = np.fft.rfftfreq(win, 1.0 / sr)
    rms, cent = np.zeros(n), np.zeros(n)
    for i in range(n):
        seg = x[i * hop: i * hop + win]
        if len(seg) < win:
            seg = np.pad(seg, (0, win - len(seg)))
        seg = seg * w
        rms[i] = np.sqrt(np.mean(seg * seg) + 1e-12)
        spec = np.abs(np.fft.rfft(seg))
        band = (freqs > 80) & (freqs < 6000)
        e = spec[band] ** 2
        cent[i] = (freqs[band] * e).sum() / max(e.sum(), 1e-12)
    db = 20 * np.log10(rms + 1e-9)
    top = np.percentile(db, 95)
    loud = np.clip((db - (top - 30.0)) / 30.0, 0, 1)
    open_ = loud ** 1.6 * 0.5
    jaw = np.zeros(n)
    v = 0.0
    for i in range(n):
        k = 0.65 if open_[i] > v else 0.3                      # attack / release per frame
        v += (open_[i] - v) * k
        jaw[i] = v
    voiced = loud > 0.25
    funnel = 0.6 * np.where(voiced, np.clip((900.0 - cent) / 500.0, 0, 1), 0) * np.clip(jaw / 0.3, 0, 1)
    smile = np.where(loud > 0.15, np.clip((cent - 2200.0) / 1500.0, 0, 1), 0)
    for c in (funnel, smile):
        c[:] = np.convolve(c, np.ones(3) / 3, mode="same")
    for c in (jaw, funnel, smile):
        c[-2:] = 0.0

    def enc(c):
        return "".join(str(int(round(v * 9))) for v in np.clip(c, 0, 1))
    return {"jaw": enc(jaw), "funnel": enc(funnel), "smile": enc(smile)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="ru,en,ja")
    a = ap.parse_args()
    out_dir = os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "Dialogue")
    for lang in a.lang.split(","):
        src = os.path.join(ROOT, "audio", "voice", lang)
        if not os.path.isdir(src):
            continue
        lines = {}
        for f in sorted(os.listdir(src)):
            if f.endswith(".wav"):
                lines[f[:-4]] = curves(os.path.join(src, f))
        path = os.path.join(out_dir, "lipsync_%s.json" % lang)
        json.dump({"fps": FPS, "lines": lines}, open(path, "w"), separators=(",", ":"))
        print("lipsync %s: %d lines -> %s (%d KB)" % (lang, len(lines), os.path.relpath(path, ROOT), os.path.getsize(path) // 1024))


if __name__ == "__main__":
    main()
