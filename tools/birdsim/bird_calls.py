"""Synthesises every species' calls (Content/BirdsData/species.json "calls") as short mono WAVs:

  /Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 tools/birdsim/bird_calls.py [out_dir]

(any Python with numpy). Writes audio/birds/<Id>/S_<Id>_<call>.wav; unreal/scripts/import_bird_calls.py imports
them as /Game/Birds/<Id>/Sounds/S_<Id>_<call>, which the bird life plays when a bird calls (UBirdLifeSubsystem).

Each call is built from a few parts, after field-guide descriptions and sonograms: whistles (a sine with its
pitch contour and a few harmonics), harsh calls (a buzzy pulse train through the formants of a crow's or a gull's
throat), trills (a note repeated, speeding up or slowing down), clicks and noise bursts. Nothing is sampled, so
there are no sources to credit.
"""
import json
import math
import os
import sys
import wave

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SR = 44100
RNG = np.random.default_rng(7)


def t_of(dur):
    return np.arange(int(dur * SR)) / SR


def env(n, attack=0.01, release=0.05, shape=1.0):
    """Attack/release envelope over n samples (seconds)."""
    e = np.ones(n)
    a, r = max(1, int(attack * SR)), max(1, int(release * SR))
    a, r = min(a, n // 2), min(r, n // 2)
    e[:a] = np.linspace(0, 1, a) ** shape
    e[n - r:] = np.linspace(1, 0, r) ** shape
    return e


def contour(points, dur):
    """Piecewise-linear frequency (or any) curve: points [(t0..1, value)]."""
    t = np.linspace(0, 1, int(dur * SR))
    xs, ys = zip(*points)
    return np.interp(t, xs, ys)


def whistle(f, dur, harm=(1.0, 0.15, 0.05), vib=0.0, vib_hz=30.0, attack=0.005, release=0.02):
    """A tonal note; f is a number or a contour array."""
    n = int(dur * SR)
    f = np.full(n, f) if np.isscalar(f) else f[:n]
    if vib:
        f = f * (1 + vib * np.sin(2 * np.pi * vib_hz * t_of(dur)[:n]))
    ph = 2 * np.pi * np.cumsum(f) / SR
    y = sum(a * np.sin((k + 1) * ph) for k, a in enumerate(harm))
    return y * env(n, attack, release)


def formants(x, peaks):
    """Shapes a buzzy signal with resonances [(Hz, width Hz, gain)] (FFT filter)."""
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1 / SR)
    g = np.zeros_like(fr)
    for c, w, a in peaks:
        g += a * np.exp(-0.5 * ((fr - c) / w) ** 2)
    return np.fft.irfft(X * g, len(x))


def buzz(f, dur, roughness=0.3):
    """A harsh pulse train (rich in harmonics) with jitter, the raw sound of a caw."""
    n = int(dur * SR)
    f = np.full(n, f) if np.isscalar(f) else f[:n]
    f = f * (1 + roughness * 0.08 * RNG.standard_normal(n).cumsum() / np.sqrt(np.arange(1, n + 1)))
    ph = np.cumsum(f) / SR
    saw = 2 * (ph - np.floor(ph)) - 1
    noise = RNG.standard_normal(n) * roughness
    return saw + noise


def noise_band(dur, centre, width):
    n = int(dur * SR)
    return formants(RNG.standard_normal(n), [(centre, width, 1.0)])


def caw(f0, dur, peaks, rough=0.35, fall=0.85):
    f = contour([(0, f0 * 0.95), (0.25, f0), (1, f0 * fall)], dur)
    y = formants(buzz(f, dur, rough), peaks)
    return y * env(len(y), 0.02, 0.08)


def silence(dur):
    return np.zeros(int(dur * SR))


def seq(*parts):
    return np.concatenate(parts)


def chirp_note(f0, f1, dur, harm=(1.0, 0.3, 0.1)):
    return whistle(contour([(0, f0), (0.3, (f0 + f1) / 2 * 1.05), (1, f1)], dur), dur, harm, attack=0.003, release=0.01)


def trill(note_fn, n, gap, accel=1.0):
    out = []
    g = gap
    for i in range(n):
        out.append(note_fn(i))
        out.append(silence(g))
        g *= accel
    return seq(*out)


# --- the calls -----------------------------------------------------------------------------------

def rook_kaah():
    return seq(caw(420, 0.42, [(900, 260, 1.0), (1700, 380, 0.6), (2600, 500, 0.25)], 0.45, 0.8))


def crow_kraa():
    return seq(caw(380, 0.36, [(800, 240, 1.0), (1500, 340, 0.7), (2400, 500, 0.3)], 0.55, 0.9), silence(0.18),
               caw(370, 0.32, [(800, 240, 1.0), (1500, 340, 0.7), (2400, 500, 0.3)], 0.55, 0.9))


def jackdaw_tchak():
    return caw(900, 0.12, [(1500, 300, 1.0), (2600, 500, 0.6)], 0.25, 1.05)


def magpie_chatter():
    return trill(lambda i: caw(1100, 0.06, [(1800, 500, 1.0), (3200, 700, 0.5)], 0.8, 1.0), 9, 0.035)


def pigeon_coo():
    f = contour([(0, 300), (0.3, 360), (0.7, 330), (1, 260)], 0.9)
    y = whistle(f, 0.9, (1.0, 0.25, 0.08), vib=0.02, vib_hz=18, attack=0.08, release=0.25)
    return formants(y + 0.05 * RNG.standard_normal(len(y)), [(330, 120, 1.0), (700, 200, 0.4)])


def woodpigeon_coo_coo():
    def phrase(f):
        return whistle(contour([(0, f), (0.5, f * 1.08), (1, f * 0.95)], 0.42), 0.42, (1.0, 0.2), attack=0.06, release=0.15)
    return seq(phrase(320), silence(0.06), phrase(360), silence(0.05), phrase(330), silence(0.25), phrase(300))


def gull_note(f0, dur, nasal=1.0, fall=0.8):
    f = contour([(0, f0 * 0.85), (0.2, f0 * 1.15), (1, f0 * fall)], dur)
    y = formants(buzz(f, dur, 0.15), [(1400, 400, 1.0 * nasal), (2600, 500, 0.6), (4000, 700, 0.2)])
    return y * env(len(y), 0.02, 0.06)


def herring_laugh():
    out = [gull_note(900, 0.55, 1.0, 0.75), silence(0.12)]
    for i in range(6):
        out += [gull_note(820 - i * 25, 0.13, 1.0, 0.9), silence(0.06)]
    return seq(*out)


def herring_kyow():
    return gull_note(880, 0.4, 1.0, 0.6)


def bhg_kree():
    return gull_note(1300, 0.32, 1.2, 0.85)


def common_kee_ya():
    return seq(gull_note(1500, 0.15, 1.0, 1.1), gull_note(1350, 0.35, 1.0, 0.7))


def tit_teacher():
    def note(f0, f1, d):
        return chirp_note(f0, f1, d, (1.0, 0.1))
    return seq(*[seq(note(6200, 6000, 0.09), silence(0.03), note(4200, 4000, 0.12), silence(0.09)) for _ in range(3)])


def pink():
    return chirp_note(6000, 4800, 0.05, (1.0, 0.2))


def bluetit_tsee():
    return seq(chirp_note(8200, 8000, 0.12, (1.0,)), silence(0.05), chirp_note(8100, 7900, 0.12, (1.0,)), silence(0.04),
               trill(lambda i: chirp_note(6000, 5200, 0.03, (1.0, 0.2)), 6, 0.02))


def bluetit_churr():
    return trill(lambda i: noise_band(0.025, 5000, 1500) * env(int(0.025 * SR), 0.003, 0.01), 12, 0.018)


def goldfinch_tickelit():
    notes = [(5200, 6400, 0.04), (6500, 5600, 0.035), (4800, 6000, 0.05), (6200, 5000, 0.045)]
    return seq(*[seq(chirp_note(a, b, d, (1.0, 0.35, 0.1)), silence(0.025)) for a, b, d in notes])


def chaffinch_song():
    out = [trill(lambda i: chirp_note(6500 - i * 120, 5600 - i * 120, 0.05), 6, 0.04, 0.9),
           trill(lambda i: chirp_note(4800 - i * 150, 4000 - i * 150, 0.045), 5, 0.03, 0.95),
           chirp_note(3200, 5200, 0.12), chirp_note(5200, 2600, 0.18)]
    return seq(*out)


def bullfinch_peu():
    return whistle(contour([(0, 2300), (0.3, 2500), (1, 1900)], 0.28), 0.28, (1.0, 0.1), attack=0.04, release=0.12)


def sparrow_chirp():
    def ch(i):
        f = 3800 + 300 * RNG.standard_normal()
        return formants(chirp_note(f * 0.8, f, 0.08, (1.0, 0.6, 0.3, 0.15)), [(3500, 900, 1.0), (6500, 1200, 0.5)])
    return trill(ch, 3, 0.12)


def treesparrow_tek():
    return seq(chirp_note(4200, 3600, 0.05, (1.0, 0.5, 0.2)), silence(0.08), chirp_note(4400, 3700, 0.05, (1.0, 0.5, 0.2)))


def starling_whistle():
    return seq(whistle(contour([(0, 2400), (1, 4200)], 0.25), 0.25, (1.0, 0.15)), silence(0.05),
               whistle(contour([(0, 3800), (1, 1800)], 0.35), 0.35, (1.0, 0.2)))


def starling_click():
    return trill(lambda i: noise_band(0.008, 3500, 1800) * env(int(0.008 * SR), 0.001, 0.004), 10, 0.03)


def thrush_tsip():
    return chirp_note(6200, 5200, 0.04, (1.0, 0.1))


def thrush_song():
    def phrase(f, k):
        return trill(lambda i: chirp_note(f, f * 0.8, 0.07, (1.0, 0.3)), k, 0.05)
    return seq(phrase(3400, 3), silence(0.25), phrase(4600, 3), silence(0.25), phrase(2800, 2))


def fieldfare_chack():
    return trill(lambda i: caw(1600, 0.07, [(2300, 600, 1.0), (4000, 800, 0.4)], 0.7, 0.95), 3, 0.06)


def blackbird_chink():
    return trill(lambda i: chirp_note(5600, 5200, 0.03, (1.0, 0.5, 0.25)), 5, 0.12)


def blackbird_song():
    f1 = contour([(0, 1600), (0.3, 2100), (0.6, 1800), (1, 2300)], 0.6)
    f2 = contour([(0, 2400), (0.5, 1900), (1, 2200)], 0.45)
    return seq(whistle(f1, 0.6, (1.0, 0.2), vib=0.015, vib_hz=12, attack=0.04, release=0.1), silence(0.08),
               whistle(f2, 0.45, (1.0, 0.2), attack=0.03, release=0.1),
               trill(lambda i: chirp_note(4500, 3800, 0.03), 4, 0.02))


def robin_tic():
    return seq(chirp_note(7000, 6000, 0.015, (1.0, 0.4)), silence(0.1), chirp_note(7000, 6000, 0.015, (1.0, 0.4)))


def robin_song():
    out = []
    for i in range(6):
        f = 3500 + 2500 * RNG.random()
        out += [chirp_note(f, f * (0.75 + 0.5 * RNG.random()), 0.06 + 0.06 * RNG.random(), (1.0, 0.15)), silence(0.03)]
    out += [trill(lambda i: chirp_note(7500, 6800, 0.02, (1.0,)), 6, 0.015)]
    return seq(*out)


def wagtail():
    return seq(chirp_note(5200, 6800, 0.08, (1.0, 0.3)), silence(0.03), chirp_note(6000, 4800, 0.06, (1.0, 0.3)))


def waxwing_sirrr():
    f = contour([(0, 6800), (1, 6200)], 0.5)
    y = whistle(f, 0.5, (1.0, 0.1))
    return y * (0.55 + 0.45 * np.sin(2 * np.pi * 45 * t_of(0.5))) * env(len(y), 0.03, 0.12)


def swift_scream():
    f = contour([(0, 6500), (1, 5600)], 0.6)
    y = formants(buzz(f * 0.5, 0.6, 0.2), [(5500, 1200, 1.0), (8000, 1500, 0.5)])
    return y * env(len(y), 0.02, 0.15)


def swallow_vit():
    return seq(chirp_note(4500, 6500, 0.04, (1.0, 0.3)), silence(0.06), chirp_note(4500, 6500, 0.04, (1.0, 0.3)))


def swallow_twitter():
    out = []
    for i in range(10):
        f = 3000 + 4000 * RNG.random()
        out += [chirp_note(f, f * (0.7 + 0.6 * RNG.random()), 0.04, (1.0, 0.3)), silence(0.02)]
    return seq(*out)


def martin_tschr():
    y = noise_band(0.15, 4200, 1200)
    return y * env(len(y), 0.01, 0.05) * (0.5 + 0.5 * np.sin(2 * np.pi * 60 * t_of(0.15)))


def cormorant_grunt():
    return caw(140, 0.5, [(400, 150, 1.0), (900, 250, 0.4)], 0.9, 0.9)


def hawk_kek(n=5, f=2200):
    return trill(lambda i: caw(f, 0.07, [(f * 1.2, 500, 1.0), (f * 2.2, 700, 0.4)], 0.2, 1.0), n, 0.06)


CALLS = {
    ("Rook", "kaah"): rook_kaah, ("HoodedCrow", "kraa"): crow_kraa, ("Jackdaw", "tchak"): jackdaw_tchak,
    ("Magpie", "chatter"): magpie_chatter, ("FeralPigeon", "coo"): pigeon_coo, ("WoodPigeon", "coo_coo"): woodpigeon_coo_coo,
    ("HerringGull", "laugh"): herring_laugh, ("HerringGull", "kyow"): herring_kyow, ("BlackHeadedGull", "kree"): bhg_kree,
    ("CommonGull", "kee_ya"): common_kee_ya, ("GreatTit", "teacher"): tit_teacher, ("GreatTit", "pink"): pink,
    ("BlueTit", "tsee_tsee"): bluetit_tsee, ("BlueTit", "churr"): bluetit_churr, ("Goldfinch", "tickelit"): goldfinch_tickelit,
    ("Chaffinch", "pink"): pink, ("Chaffinch", "song_rattle"): chaffinch_song, ("Bullfinch", "peu"): bullfinch_peu,
    ("HouseSparrow", "chirp"): sparrow_chirp, ("TreeSparrow", "tek"): treesparrow_tek, ("Starling", "whistle"): starling_whistle,
    ("Starling", "click"): starling_click, ("SongThrush", "tsip"): thrush_tsip, ("SongThrush", "song_repeat"): thrush_song,
    ("Fieldfare", "chack_chack"): fieldfare_chack, ("Blackbird", "chink_alarm"): blackbird_chink,
    ("Blackbird", "song_fluty"): blackbird_song, ("Robin", "tic"): robin_tic, ("Robin", "song_wistful"): robin_song,
    ("WhiteWagtail", "tslee_vit"): wagtail, ("Waxwing", "sirrr"): waxwing_sirrr, ("CommonSwift", "scream"): swift_scream,
    ("BarnSwallow", "vit"): swallow_vit, ("BarnSwallow", "twitter"): swallow_twitter, ("SandMartin", "tschr"): martin_tschr,
    ("Cormorant", "grunt"): cormorant_grunt, ("Sparrowhawk", "kek"): lambda: hawk_kek(5, 2400),
    ("Peregrine", "kek_kek"): lambda: hawk_kek(7, 1900),
}


def write(path, y, peak=0.7):
    y = y / max(1e-6, np.max(np.abs(y))) * peak
    y = np.concatenate([silence(0.01), y, silence(0.03)])
    pcm = (np.clip(y, -1, 1) * 32767).astype("<i2")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "audio", "birds")
    with open(os.path.join(ROOT, "unreal", "Lyudmilovsk", "Content", "BirdsData", "species.json")) as f:
        species = json.load(f)["species"]
    made, missing = 0, []
    for s in species:
        for call in s.get("calls", []):
            fn = CALLS.get((s["id"], call))
            if not fn:
                missing.append("%s/%s" % (s["id"], call))
                continue
            write(os.path.join(out, s["id"], "S_%s_%s.wav" % (s["id"], call)), fn())
            made += 1
    print("bird_calls: %d calls -> %s%s" % (made, out, ("; no synth for " + ", ".join(missing)) if missing else ""))


if __name__ == "__main__":
    main()
