"""Grey seal, Halichoerus grypus, adult male-ish, 2.0 m nose to hind-flipper tips: heavy torpedo body with a thick neck, long
straight-to-convex "Roman nose" muzzle, big dark eyes, small fore-flippers with five dark claws, hind flippers held together
pointing backwards like a fish tail (spread at the tips, the middle digits shorter).  Dark grey back with paler irregular
blotches, lighter grey belly with dark blotches.  The game's lateral bend sweeps the hind body and flippers: G is 0 on the
head, rises through the body to 1 at the flipper tips; fore-flippers B = 1 (paddling)."""
import numpy as np

from sealife_geo import instanced, nrm

L = 2.0

SPEC = dict(
    id="GreySeal", latin="Halichoerus grypus", length=L, seed=71, tris=6000,
    size=("x", L), close=0.08,
    flex=[(0, 0.0), (0.12, 0.0), (0.3, 0.04), (0.5, 0.18), (0.65, 0.38), (0.78, 0.6), (0.88, 0.8), (1.0, 1.0)],
)

FUR = dict(
    ramp=[(0, (46, 48, 51)), (0.3, (54, 56, 58)), (0.5, (78, 78, 78)), (0.7, (118, 115, 108)), (1, (138, 134, 125))],
    grain=0.07, grain_sc=0.006,
    layers=[
        dict(type="mottle", col=(112, 110, 104), sc=0.035, th=0.64, soft=0.05, a=0.55, q=(0.0, 0.62)),     # pale blotches on the back
        dict(type="mottle", col=(40, 40, 42), sc=0.04, th=0.6, soft=0.05, a=0.8, q=(0.45, 1.05)),          # dark blotches on the belly
        dict(type="speckle", col=(36, 36, 38), cell=0.012, prob=0.2, r=0.28, star=0.3, a=0.5, q=(0.35, 1.0), s=(0.12, 0.85)),
        dict(type="mottle", col=(30, 31, 33), sc=0.02, th=0.5, soft=0.2, a=0.35, s=(0.0, 0.08)),          # darker muzzle
        dict(type="stripe", q0=0.60, q1=0.66, col=(22, 20, 20), a=0.85, soft=0.012, s=(0.003, 0.05), ssoft=0.006),   # mouth line
        dict(type="spot", s0=0.006, q0=0.14, rs=0.004, rq=0.07, col=(16, 15, 15), a=0.9),                 # nostril
        dict(type="spot", s0=0.088, q0=0.36, rs=0.016, rq=0.10, col=(30, 30, 31), a=0.5),                 # dark skin round the eye
        dict(type="speckle", col=(26, 24, 24), cell=0.0035, prob=0.6, r=0.18, a=0.7, s=(0.012, 0.045), q=(0.38, 0.58)),  # whisker pores
    ])
FLIP = dict(ramp=[(0, (52, 53, 55)), (0.5, (60, 60, 61)), (1, (52, 53, 55))], grain=0.06, grain_sc=0.006,
            layers=[dict(type="streak", n=10, w=0.02, col=(34, 34, 36), a=0.6, s=(0.45, 1.0)),
                    dict(type="tip", s0=0.6, s1=1.0, col=(34, 34, 36), a=0.6),
                    dict(type="mottle", col=(92, 90, 86), sc=0.03, th=0.66, a=0.4, s=(0.0, 0.4))])
HIND = dict(FLIP, layers=[dict(type="streak", n=10, w=0.025, col=(30, 30, 32), a=0.55, s=(0.35, 1.0)),
                          dict(type="tip", s0=0.75, s1=1.0, col=(30, 30, 32), a=0.6),
                          dict(type="mottle", col=(96, 94, 88), sc=0.03, th=0.64, a=0.45, s=(0.0, 0.45))])
CLAW = dict(col=(30, 27, 24), grain=0.05, layers=[dict(type="tip", s0=0.5, s1=1.0, col=(70, 64, 56), a=0.6)])
EYE = dict(col=(18, 15, 14), layers=[dict(type="eye", col=(8, 7, 7), r=0.55, a=0.9, glint=(0.25, 0.25, 0.14)),
                                     dict(type="tip", s0=0.88, s1=1.0, col=(40, 36, 34), a=0.8)])


def build(K):
    lod = K.lod
    parts = []
    body = K.tube(
        "body", [(1.0, 0, -0.03), (0.86, 0, -0.005), (0.6, 0, 0.02), (0.2, 0, 0.02), (-0.2, 0, 0.0), (-0.5, 0, -0.01), (-0.72, 0, -0.005)],
        w=[(0, 0.0), (0.004, 0.030), (0.012, 0.046), (0.03, 0.060), (0.07, 0.080), (0.10, 0.096), (0.145, 0.118), (0.2, 0.156),
           (0.32, 0.235), (0.44, 0.268), (0.58, 0.255), (0.73, 0.198), (0.85, 0.125), (0.93, 0.072), (0.98, 0.035), (1, 0.0)],
        top=[(0, 0.0), (0.004, 0.026), (0.012, 0.040), (0.03, 0.054), (0.07, 0.076), (0.10, 0.094), (0.145, 0.112), (0.2, 0.142),
             (0.32, 0.215), (0.44, 0.238), (0.58, 0.222), (0.73, 0.170), (0.85, 0.110), (0.93, 0.064), (0.98, 0.030), (1, 0.0)],
        bot=[(0, 0.0), (0.004, 0.024), (0.012, 0.036), (0.03, 0.046), (0.07, 0.062), (0.10, 0.080), (0.145, 0.104), (0.2, 0.148),
             (0.32, 0.245), (0.44, 0.278), (0.58, 0.258), (0.73, 0.190), (0.85, 0.112), (0.93, 0.062), (0.98, 0.030), (1, 0.0)],
        n=[(0, 2.0), (0.2, 2.1), (0.6, 2.15), (1, 2.0)], m=K.pick(16, 8), nr=K.pick(62, 28), cluster=0.55, paint=FUR, dens=1.0)
    parts.append(body)
    # eyes: big dark caps on the side of the head, looking forward-outward
    parts += K.pair(K.eye("eye", (0.834, 0.070, 0.047), 0.028, (0.35, 1.0, 0.3), phimax=1.55, scale=(1, 1, 0.75), nlat=K.pick(4, 2), nseg=K.pick(12, 7),
                          paint=EYE, dens=1.5))
    # fore-flippers: flattened paddles from the lower flank, pointing back-down-out
    up = nrm(np.array([0.0, -0.35, 1.0]))
    fctrl = [(0.44, 0.15, -0.10), (0.36, 0.23, -0.18), (0.25, 0.27, -0.24), (0.13, 0.29, -0.28)]
    # roll 90: 'w' is the thickness (closing to the sharp tip edge), 'top' / 'bot' the in-plane half-width
    fore = K.tube("fore", fctrl, top=[(0, 0.042), (0.3, 0.050), (0.7, 0.058), (0.95, 0.054), (1, 0.045)],
                  w=[(0, 0.040), (0.3, 0.030), (0.7, 0.020), (0.95, 0.010), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(12, 6),
                  ends=("apex", "edge"), half=False, kind="extra", up=up, roll=90, B=1.0, paint=FLIP, gadd=[(0, 0.0), (1, 0.3)], cluster=0.3)
    parts += K.pair(fore)
    if lod == 0:
        # five claws along the flipper's tip, pointing on along the flipper
        T = fore.path.tangent(1.0)
        side = nrm(np.cross(T, up))
        tip = fore.path.at(1.0)
        claw = None
        for k in range(5):
            off = (k - 2) * 0.019
            base = tip - T * 0.03 + side * off + up * 0.004
            pts = [tuple(base), tuple(base + T * 0.03 + up * 0.002), tuple(base + T * 0.055 - up * 0.006)]
            if claw is None:
                claw = K.tube("claw", pts, w=[(0, 0.007), (0.5, 0.005), (1, 0.0015)], top=[(0, 0.004), (1, 0.001)], n=2, m=4, nr=3,
                              half=False, kind="extra", up=up, B=1.0, paint=CLAW, gadd=[(0, 0.3), (1, 0.3)], cluster=0.0, minpx=8)
                parts += K.pair(claw)
            else:
                d = side * off - side * (-2 * 0.019)
                c = instanced(claw, "claw%d" % k, np.eye(3), d)
                parts += [c, instanced(c, "claw%d_L" % k, np.diag([1, -1, 1]), (0, 0, 0))]
    # hind flippers: vertical paddles held side by side behind the body, spread at the tips, middle digits shorter
    def fan(ring, S, q):
        f = np.clip((S - 0.5) / 0.5, 0, 1)[:, None] ** 2          # slope stays below the path's, so no fold
        c = np.cos(2 * np.pi * q)[None, :]
        ring = ring.copy()
        ring[..., 0] += 0.05 * f * (1 - c * c)
        return ring
    hind = K.tube("hind", [(-0.56, 0.034, -0.005), (-0.72, 0.038, -0.005), (-0.86, 0.050, -0.004), (-1.0, 0.068, -0.003)],
                  w=[(0, 0.032), (0.3, 0.030), (0.6, 0.022), (0.9, 0.011), (1, 0.0)],
                  top=[(0, 0.065), (0.3, 0.062), (0.55, 0.080), (0.8, 0.125), (1, 0.150)],
                  n=2.0, m=K.pick(12, 6), nr=K.pick(14, 7), ends=("apex", "edge"), half=False, kind="body", up=(0, 0, 1),
                  paint=HIND, deform=fan, cluster=0.3)
    parts += K.pair(hind)
    return parts
