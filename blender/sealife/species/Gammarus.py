"""Gammarus amphipod (G. salinus / oceanicus type), 1.8 cm along the curved body: laterally flattened, arched back with the
tail curled under, deep coxal plates along the front half, kidney-shaped black eyes, two antenna pairs (upper longer),
7 pereopod pairs (gnathopods forward, the back three longer and pointing back), 3 pleopod and 3 uropod pairs, greenish-brown."""
import numpy as np

from sealife_geo import saw, seg_rings

L = 0.018
SEG = [0.11] + list(np.round(np.linspace(0.11, 0.60, 8)[1:], 4)) + [0.67, 0.74, 0.80, 0.85, 0.895, 0.94]

SPEC = dict(
    id="Gammarus", latin="Gammarus sp.", length=L, seed=29, tris=1500,
    size=("body_path", L),
    flex=[(0, 0.0), (0.4, 0.03), (0.7, 0.10), (1.0, 0.2)],
)

SHELL = dict(
    ramp=[(0, (98, 96, 62)), (0.35, (112, 106, 70)), (0.7, (140, 128, 90)), (1, (164, 150, 114))], grain=0.08,
    layers=[
        dict(type="mottle", col=(76, 72, 44), sc=0.06, th=0.55, a=0.5),
        dict(type="speckle", col=(196, 176, 96), cell=0.022, prob=0.35, r=0.22, a=0.55, q=(0.25, 0.8), seed=4),
        dict(type="bands", at=SEG[1:], w=0.02, rear=True, col=(150, 146, 98), a=0.4),
        dict(type="bands", at=SEG, w=0.005, col=(66, 66, 38), a=0.55),
    ])
LEG = dict(col=(150, 142, 98), grain=0.08, layers=[dict(type="tip", s0=0.6, s1=1.0, col=(176, 168, 128), a=0.5)])
ANT = dict(col=(132, 122, 78), grain=0.06, layers=[
    dict(type="bands", at=list(np.linspace(0.2, 0.95, 10)), w=0.015, col=(90, 84, 52), a=0.5)])
EYE = dict(col=(18, 14, 12), layers=[dict(type="eye", col=(8, 6, 6), r=0.9, a=0.8, glint=(0.15, 0.3, 0.25))])


def build(K):
    lod = K.lod
    parts = []
    ctrl = np.array([(0.0083, 0, -0.0010), (0.0060, 0, 0.0010), (0.0020, 0, 0.0022), (-0.0025, 0, 0.0020), (-0.0060, 0, 0.0006),
                     (-0.0080, 0, -0.0018), (-0.0078, 0, -0.0040)]) * 0.857
    body = K.tube(
        "body", ctrl,
        w=[(0, 0.0003), (0.04, 0.0008), (0.11, 0.0011), (0.3, 0.00125), (0.6, 0.0012), (0.8, 0.0009), (0.94, 0.0005), (1, 0.0002)],
        top=[(0, 0.0003), (0.05, 0.0009), (0.15, 0.0012), (0.5, 0.0013), (0.75, 0.0011), (0.94, 0.0005), (1, 0.0002)],
        bot=[(0, 0.0003), (0.05, 0.0010), (0.15, 0.0019), (0.35, 0.0022), (0.55, 0.0018), (0.7, 0.0013), (0.9, 0.0006), (1, 0.0002)],
        n=2.3, m=K.pick(6, 3), nr=K.pick(20, 10), cluster=0.3, mod=saw(SEG, amp=0.06, groove=0.05, gw=0.006),
        extra_s=seg_rings(SEG, 0.005) if lod == 0 else (), paint=SHELL)
    parts.append(body)
    path = body.path

    def onbody(s, dy=0.0, dz=0.0):
        p = path.at(s)
        return (p[0], p[1] + dy, p[2] + dz)
    h = np.array(onbody(0.0))
    parts += K.pair(K.eye("eye", tuple(np.array(onbody(0.065)) + (0, 0.00085, 0.0004)), 0.00042, (0.2, 1.0, 0.15), scale=(1.6, 0.9, 0.5),
                          nlat=K.pick(2, 1), nseg=K.pick(7, 5), paint=EYE))
    # antenna 1 (upper, longer) and antenna 2 (lower)
    a1 = [h + d for d in ((-0.0007, 0.0005, 0.0012), (0.0015, 0.0009, 0.0024), (0.0039, 0.0013, 0.0038), (0.0059, 0.0016, 0.0056))]
    a2 = [h + d for d in ((-0.0003, 0.0006, 0.0002), (0.0017, 0.0010, 0.0007), (0.0037, 0.0014, 0.0009), (0.0053, 0.0017, 0.0001))]
    parts += K.pair(K.tube("ant1", a1, w=[(0, 0.00016), (0.3, 0.00011), (1, 0.00005)], n=2, m=3, nr=K.pick(6, 3), half=False, kind="extra",
                           paint=ANT, cluster=0.2))
    parts += K.pair(K.tube("ant2", a2, w=[(0, 0.00017), (0.3, 0.00012), (1, 0.00005)], n=2, m=3, nr=K.pick(5, 3), half=False, kind="extra",
                           paint=ANT, cluster=0.2))
    # pereopods 1-7 (gnathopods 1-2 forward), from the lower edge of the coxal plates
    legs = [  # s on the body, foot offset (dx, dz), lateral spread
        (0.17, (0.0020, -0.0030), 0.0012), (0.24, (0.0012, -0.0034), 0.0013), (0.31, (0.0002, -0.0040), 0.0014),
        (0.38, (-0.0008, -0.0040), 0.0015), (0.45, (-0.0022, -0.0042), 0.0017), (0.52, (-0.0034, -0.0040), 0.0018),
        (0.58, (-0.0045, -0.0036), 0.0018)]
    for i, (s, (dx, dz), sp) in enumerate(legs):
        if lod == 1 and i in (1, 3):
            continue
        b = np.array(onbody(s))
        base = b + (0, 0.0006, -0.0012)
        pts = [tuple(base), tuple(base + (dx * 0.3, sp * 0.5, dz * 0.45)), tuple(base + (dx * 0.75, sp * 0.85, dz * 0.8)), tuple(base + (dx, sp, dz))]
        r = 0.00017 if i > 1 else 0.00022
        parts += K.pair(K.tube("leg%d" % (i + 1), pts, w=[(0, r), (0.5, r * 0.8), (1, r * 0.35)], n=2, m=3, nr=K.pick(4, 2), half=False,
                               kind="extra", B=1.0, paint=LEG, cluster=0.2))
    if lod == 0:  # pleopods under the pleon
        for i, s in enumerate((0.64, 0.71, 0.77)):
            b = np.array(onbody(s))
            pts = [tuple(b + (0, 0.0004, -0.0008)), tuple(b + (0.0004, 0.0007, -0.0022)), tuple(b + (0.0010, 0.0008, -0.0030))]
            parts += K.pair(K.tube("pleo%d" % (i + 1), pts, w=[(0, 0.00016), (1, 0.00006)], n=2, m=3, nr=2, half=False, kind="extra",
                                   B=1.0, key="pleo%d" % (i + 1), paint=LEG, cluster=0.0))
    # uropods 1-3 at the curled tail end, pointing back / down
    for i, s in enumerate((0.86, 0.905, 0.95)):
        if lod == 1 and i == 1:
            continue
        b = np.array(onbody(s))
        tdir = path.tangent(s)
        pts = [tuple(b + (0, 0.0003, 0)), tuple(b + tdir * 0.0010 + (0, 0.0006, 0)), tuple(b + tdir * 0.0019 + (0, 0.0008, 0))]
        parts += K.pair(K.tube("uro%d" % (i + 1), pts, w=[(0, 0.00016), (1, 0.00006)], n=2, m=3, nr=2, half=False, kind="extra",
                               paint=LEG, cluster=0.0, gadd=[(0, 0.0), (1, 0.1)]))
    return parts
