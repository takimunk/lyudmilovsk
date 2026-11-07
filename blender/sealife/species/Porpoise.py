"""Harbour porpoise, Phocoena phocoena (rare in the Baltic proper), 1.5 m snout .. fluke tips: small rounded head without a beak,
stout torpedo body tapering into a laterally compressed tail stock, low triangular dorsal fin just behind the middle, small dark
rounded flippers, horizontal flukes.  Dark grey back, paler grey flanks fading into a white belly, dark chin and a dark line from the
mouth corner to the flipper.
The real animal swims with an up-down stroke but the game's shader bends sideways, so G stays low on the body and only rises
moderately on the tail stock / flukes: the sideways bend stays subtle."""
import numpy as np

from sealife_geo import fn

L = 1.5

SPEC = dict(
    id="Porpoise", latin="Phocoena phocoena", length=L, seed=103, tris=4800,
    size=("x", L), close=0.10,
    flex=[(0, 0.0), (0.4, 0.02), (0.6, 0.05), (0.75, 0.12), (0.88, 0.24), (1.0, 0.35)],
)

DARK = (50, 54, 60)
# dark line from the mouth corner (s 0.03, q 0.6) to the flipper (s 0.19, q 0.76)
LINE = [dict(type="spot", s0=s, q0=0.60 + (s - 0.03) * 1.0, rs=0.02, rq=0.03, col=(70, 74, 80), a=0.35)
        for s in np.linspace(0.035, 0.185, 30)]
SKIN = dict(
    ramp=[(0, DARK), (0.28, (56, 60, 66)), (0.42, (96, 100, 106)), (0.55, (150, 152, 156)), (0.68, (220, 220, 216)), (1, (234, 234, 230))],
    grain=0.04, grain_sc=0.01,
    layers=[
        dict(type="mottle", col=(76, 80, 86), sc=0.05, th=0.5, soft=0.15, a=0.4, q=(0.38, 0.62)),      # irregular back / flank boundary
        dict(type="mottle", col=(226, 226, 222), sc=0.05, th=0.55, soft=0.15, a=0.45, q=(0.55, 0.72), s=(0.15, 0.6)),   # pale flank swoosh
        dict(type="stripe", q0=0.64, q1=1.1, col=(78, 82, 88), a=0.9, s=(-0.1, 0.035), ssoft=0.012),   # dark chin / lower lip
        dict(type="stripe", q0=0.56, q1=0.62, col=(40, 42, 46), a=0.55, s=(0.002, 0.034), ssoft=0.008, soft=0.035),   # mouth line
        dict(type="spot", s0=0.07, q0=0.0, rs=0.006, rq=0.05, col=(26, 28, 30), a=0.9),               # blowhole
        dict(type="spot", s0=0.062, q0=0.47, rs=0.016, rq=0.06, col=(60, 64, 70), a=0.8),             # dark eye ring
        dict(type="stripe", q0=0.0, q1=0.18, col=(40, 43, 48), a=0.5, s=(0.6, 1.0)),                   # darker tail-stock ridge
        dict(type="stripe", q0=0.4, q1=1.1, col=(70, 74, 80), a=0.7, s=(0.88, 1.1), ssoft=0.04),        # dark tail stock underside
    ] + LINE)
FIN = dict(col=(46, 50, 56), grain=0.05, grain_sc=0.01, layers=[
    dict(type="mottle", col=(62, 66, 72), sc=0.05, th=0.55, a=0.4)])
FLIP = dict(col=(48, 52, 58), grain=0.05, grain_sc=0.01, layers=[
    dict(type="tip", s0=0.0, s1=0.2, col=(70, 74, 80), a=0.5)])
EYE = dict(col=(20, 18, 18), layers=[dict(type="eye", col=(8, 8, 8), r=0.6, a=0.9, glint=(0.25, 0.25, 0.14)),
                                     dict(type="tip", s0=0.85, s1=1.0, col=(46, 48, 52), a=0.8)])

W_ = [(0, 0.0), (0.005, 0.030), (0.02, 0.060), (0.05, 0.100), (0.10, 0.140), (0.20, 0.175), (0.35, 0.190), (0.50, 0.180),
      (0.65, 0.130), (0.80, 0.065), (0.90, 0.035), (0.96, 0.024), (1, 0.0)]
TOP = [(0, 0.0), (0.005, 0.025), (0.02, 0.050), (0.05, 0.085), (0.10, 0.120), (0.20, 0.150), (0.35, 0.165), (0.50, 0.165),
       (0.65, 0.140), (0.80, 0.100), (0.90, 0.065), (0.96, 0.040), (1, 0.0)]
BOT = [(0, 0.0), (0.005, 0.025), (0.02, 0.055), (0.05, 0.090), (0.10, 0.130), (0.20, 0.165), (0.35, 0.180), (0.50, 0.170),
       (0.65, 0.120), (0.80, 0.070), (0.90, 0.040), (0.96, 0.026), (1, 0.0)]
CTRL = [(0.75, 0, -0.02), (0.60, 0, 0.0), (0.20, 0, 0.01), (-0.30, 0, 0.0), (-0.70, 0, -0.01)]


def build(K):
    lod = K.lod
    parts = []
    body = K.tube("body", CTRL, w=W_, top=TOP, bot=BOT, n=[(0, 2.0), (0.6, 2.0), (0.85, 1.9), (1, 1.9)], m=K.pick(15, 7),
                  nr=K.pick(60, 26), cluster=0.35, paint=SKIN,
                  extra_s=(0.008, 0.015, 0.022, 0.029, 0.036, 0.045) if lod == 0 else ())
    parts.append(body)
    se = 0.062
    c = body.path.at(se)
    wy, tz = fn(W_)(se), fn(TOP)(se)
    parts += K.pair(K.eye("eye", (c[0], 0.92 * wy * np.sin(np.radians(84)), c[2] + 0.92 * tz * np.cos(np.radians(84))), 0.011,
                          (0.25, 1.0, 0.0), phimax=1.4, scale=(1, 1, 0.8), nlat=K.pick(3, 2), nseg=K.pick(10, 6), paint=EYE, dens=1.5))
    # low triangular dorsal fin: leading edge sloping, trailing edge almost upright (chord forward 'top', back 'bot')
    parts.append(K.tube("dorsal", [(-0.05, 0, 0.125), (-0.11, 0, 0.215), (-0.165, 0, 0.30)],
                        w=[(0, 0.020), (0.5, 0.013), (0.9, 0.005), (1, 0.003)],
                        top=[(0, 0.125), (0.4, 0.075), (0.85, 0.020), (1, 0.004)], bot=[(0, 0.060), (0.4, 0.038), (0.85, 0.012), (1, 0.003)],
                        n=2.0, m=K.pick(10, 6), nr=K.pick(9, 4), half=False, kind="extra", up=(1, 0, 0), paint=FIN, cluster=0.2))
    # small dark flippers from the lower flank behind the head, back-down-out
    parts += K.pair(K.tube("flipper", [(0.47, 0.115, -0.085), (0.40, 0.185, -0.130), (0.30, 0.225, -0.170)],
                           top=[(0, 0.034), (0.3, 0.044), (0.7, 0.040), (0.95, 0.020), (1, 0.012)],
                           w=[(0, 0.020), (0.3, 0.014), (0.8, 0.006), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(10, 5),
                           ends=("apex", "edge"), half=False, kind="extra", up=(0, -0.45, 1), roll=90, B=1.0, paint=FLIP, cluster=0.2))
    # horizontal flukes: one swept lobe per side, chord in the horizontal plane, closing to a knife edge at the tip
    parts += K.pair(K.tube("fluke", [(-0.625, 0.004, -0.010), (-0.675, 0.075, -0.010), (-0.720, 0.130, -0.010), (-0.742, 0.165, -0.010)],
                           top=[(0, 0.058), (0.3, 0.052), (0.65, 0.038), (0.9, 0.018), (1, 0.008)],
                           w=[(0, 0.018), (0.4, 0.012), (0.85, 0.005), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(12, 5),
                           ends=("apex", "edge"), half=False, kind="body", up=(0, 0, 1), roll=90, paint=FIN, gadd=[(0, 0.0), (1, 0.1)],
                           cluster=0.2))
    return parts
