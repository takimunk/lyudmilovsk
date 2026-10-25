"""Draws a WAV's spectrogram (log frequency 30 Hz .. 20 kHz, dB colour) with its loudness curve under it, as PNG.

    /Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 tools/audiosim/spectrogram.py a.wav [b.wav ...]
Writes a.png next to each file. Needs numpy (Blender's python has it); no PIL (a tiny PNG writer is below).
"""
import struct
import sys
import wave
import zlib

import numpy as np


def read(path):
    with wave.open(path) as w:
        n, ch, rate = w.getnframes(), w.getnchannels(), w.getframerate()
        a = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768.0
    return a.reshape(-1, ch).mean(axis=1), rate


def png(path, rgb):
    h, w, _ = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def colour(v):
    # dark blue -> purple -> orange -> pale yellow
    stops = np.array([[0, 0, 8], [40, 10, 90], [160, 30, 110], [240, 110, 30], [255, 240, 170]], np.float32)
    x = np.clip(v, 0, 1) * (len(stops) - 1)
    i = np.minimum(x.astype(int), len(stops) - 2)
    t = (x - i)[..., None]
    return (stops[i] * (1 - t) + stops[i + 1] * t).astype(np.uint8)


def draw(path, width=1200, height=360, meter=90):
    x, rate = read(path)
    win = 2048
    hop = max(len(x) // width, 64)
    frames = [x[i:i + win] * np.hanning(win) for i in range(0, max(len(x) - win, 1), hop)][:width]
    spec = np.abs(np.fft.rfft(np.array(frames), axis=1)) / (win / 4) + 1e-9
    db = 20 * np.log10(spec)
    freqs = np.fft.rfftfreq(win, 1 / rate)
    rows = np.geomspace(30, 20000, height)[::-1]
    idx = np.clip(np.searchsorted(freqs, rows), 1, len(freqs) - 1)
    img = db[:, idx].T
    img = colour((img + 110) / 80)
    # loudness per column, dBFS -60..0
    rms = 20 * np.log10(np.sqrt(np.mean(np.array(frames) ** 2, axis=1)) + 1e-9)
    m = np.zeros((meter, img.shape[1], 3), np.uint8) + 16
    for c, r in enumerate(rms):
        hgt = int(np.clip((r + 60) / 60, 0, 1) * (meter - 1))
        m[meter - 1 - hgt:, c] = (90, 200, 120)
    for d in (-12, -24, -36, -48):
        m[int((-d / 60) * (meter - 1)), :] = (70, 70, 70)
    # frequency grid lines at 100 Hz, 1 kHz, 10 kHz
    for f in (100, 1000, 10000):
        img[int(np.argmin(np.abs(rows - f))), ::4] = (255, 255, 255)
    png(path[:-4] + ".png", np.concatenate([img, m], axis=0))


for p in sys.argv[1:]:
    draw(p)
    print(p[:-4] + ".png")
