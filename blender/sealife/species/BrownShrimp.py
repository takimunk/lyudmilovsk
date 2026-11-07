"""Brown shrimp, Crangon crangon, 6 cm (rostrum tip .. telson tip): dorsoventrally flattened, sandy translucent grey-brown with
dark star chromatophores, short rostrum, eyes on top, long antennae swept back along the flanks, antennal scales, 5 pairs of
walking legs (1st subchelate, held forward), pleopods, fan tail.  Lies low on the sand, legs splayed, tail fan flat."""
import numpy as np

from sealife_geo import saw, seg_rings, instanced, rotz

L = 0.06
SEG = [0.40, 0.475, 0.55, 0.625, 0.695, 0.765, 0.835]       # carapace end, 6 abdominal segments, telson from 0.835

SPEC = dict(
    id="BrownShrimp", latin="Crangon crangon", length=L, seed=61, tris=2100,
    size=("body_path", L),
    flex=[(0, 0.0), (0.4, 0.02), (0.6, 0.07), (0.8, 0.14), (1.0, 0.2)],
)

SAND = dict(
    ramp=[(0, (146, 130, 106)), (0.45, (160, 145, 120)), (0.75, (196, 186, 166)), (1, (214, 206, 190))], grain=0.08,
    layers=[
        dict(type="mottle", col=(112, 96, 74), sc=0.07, th=0.55, a=0.45, q=(0.0, 0.7)),
        dict(type="speckle", col=(62, 46, 34), cell=0.0075, prob=0.45, r=0.2, star=0.3, a=0.55, q=(0.0, 0.85)),
        dict(type="speckle", col=(48, 34, 26), cell=0.014, prob=0.55, r=0.24, star=0.35, a=0.85, q=(0.0, 0.75), seed=5),
        dict(type="speckle", col=(222, 212, 188), cell=0.011, prob=0.3, r=0.16, a=0.4, q=(0.0, 0.9), seed=3),
        dict(type="bands", at=SEG[1:], w=0.03, rear=True, col=(196, 186, 164), a=0.35),
        dict(type="bands", at=SEG, w=0.005, col=(92, 80, 64), a=0.55, q=(0.0, 0.8)),
    ])
LEG = dict(col=(190, 178, 156), grain=0.08, layers=[
    dict(type="speckle", col=(70, 55, 42), cell=0.012, prob=0.4, r=0.25, a=0.7),
    dict(type="tip", s0=0.75, s1=1.0, col=(150, 135, 112), a=0.6)])
ANT = dict(col=(176, 158, 130), grain=0.06, layers=[
    dict(type="bands", at=list(np.linspace(0.08, 0.95, 14)), w=0.012, col=(98, 82, 62), a=0.55)])
EYE = dict(col=(48, 40, 32), layers=[dict(type="eye", col=(16, 13, 12), r=0.75, facet=0.0015, glint=(0.25, 0.3, 0.18), a=0.95)])
PLATE = dict(col=(186, 172, 146), alpha=0.82, edge=(72, 58, 44), edge_w=0.3, edge_a=0.75, rag=0.05, grain=0.06,
             layers=[dict(type="speckle", col=(60, 45, 34), cell=0.012, prob=0.5, r=0.25, star=0.4, a=0.8)])
TAIL = dict(PLATE, layers=PLATE["layers"] + [dict(type="tip", s0=0.6, s1=0.8, col=(58, 44, 34), a=0.7)])


def build(K):
    lod = K.lod
    parts = []
    body = K.tube(
        "body", [(0.030, 0, 0.0004), (0.018, 0, 0.0012), (0.004, 0, 0.0013), (-0.008, 0, 0.0012), (-0.019, 0, 0.0002), (-0.030, 0, -0.0016)],
        w=[(0, 0.0002), (0.02, 0.0012), (0.06, 0.0045), (0.12, 0.0062), (0.25, 0.0068), (0.38, 0.0062), (0.45, 0.0056), (0.6, 0.0048),
           (0.75, 0.0034), (0.835, 0.0022), (0.87, 0.0016), (0.97, 0.0006), (1, 0.0002)],
        top=[(0, 0.0001), (0.03, 0.0006), (0.08, 0.0020), (0.2, 0.0034), (0.38, 0.0037), (0.5, 0.0036), (0.7, 0.0029), (0.835, 0.0016),
             (0.9, 0.0008), (1, 0.0002)],
        bot=[(0, 0.0001), (0.05, 0.0008), (0.15, 0.0024), (0.3, 0.0029), (0.4, 0.0027), (0.6, 0.0023), (0.8, 0.0013), (0.9, 0.0005), (1, 0.0002)],
        n=2.5, m=K.pick(6, 4), nr=K.pick(24, 13), cluster=0.35, mod=saw(SEG, amp=0.07, groove=0.05, gw=0.007),
        extra_s=seg_rings(SEG, 0.006) if lod == 0 else (), paint=SAND)
    parts.append(body)
    # eyes (on top, close together)
    parts += K.pair(K.eye("eye", (0.0243, 0.0019, 0.0026), 0.00105, (0.45, 0.55, 0.7), nlat=K.pick(3, 2), nseg=K.pick(8, 5), paint=EYE))
    # antennae: forward, then swept back along the flanks onto the sand
    parts += K.pair(K.tube("ant2", [(0.024, 0.0033, 0.0004), (0.029, 0.0058, 0.0009), (0.027, 0.011, 0.0), (0.016, 0.0165, -0.0018),
                                    (0.002, 0.0195, -0.0030), (-0.014, 0.0205, -0.0036), (-0.026, 0.0195, -0.0037)],
                           w=[(0, 0.00030), (0.15, 0.00022), (1, 0.0001)], n=2, m=3, nr=K.pick(14, 11), half=False, kind="extra",
                           paint=ANT, cluster=0.5))
    if lod == 0:  # antennules: two short flagella per side (the second is an instance of the first)
        a1 = K.tube("ant1", [(0.0245, 0.0012, 0.0010), (0.0300, 0.0020, 0.0016), (0.0345, 0.0034, 0.0018)],
                    w=[(0, 0.00022), (1, 0.00007)], n=2, m=3, nr=4, half=False, kind="extra", paint=ANT, cluster=0.0)
        a1b = instanced(a1, "ant1b", rotz(8), (-0.0003, 0.0004, -0.0005))
        parts += K.pair(a1) + K.pair(a1b)
    # antennal scales
    parts += K.pair(K.plate("scale", [(0.0215, 0.0040, 0.0006), (0.0262, 0.0052, 0.0008), (0.0310, 0.0062, 0.0009)],
                            [(0, 0.0006), (0.35, 0.0011), (0.8, 0.0009), (1, 0.0003)], nu=K.pick(3, 2), nv=1, normal=(0, -0.1, 1), paint=PLATE))
    # walking legs: 1st pair subchelate held forward, 2nd-5th splayed onto the sand
    legs = [  # attach x, foot (x, y), knee lift, radius
        (0.017, (0.0265, 0.0075), 0.0008, 0.00055),
        (0.0135, (0.019, 0.0105), 0.0012, 0.00030),
        (0.0100, (0.010, 0.0120), 0.0016, 0.00034),
        (0.0062, (0.000, 0.0128), 0.0018, 0.00036),
        (0.0025, (-0.008, 0.0118), 0.0016, 0.00036),
    ]
    for i, (x, (fx, fy), lift, r) in enumerate(legs):
        if lod == 1 and i == 1:
            continue
        kx = 0.5 * (x + fx)
        pts = [(x, 0.0012, -0.0012), (x - 0.0002, 0.0045, -0.0026), (kx, 0.5 * (0.0045 + fy) + 0.001, -0.0026 + lift), (fx, fy, -0.0038)]
        parts += K.pair(K.tube("leg%d" % (i + 1), pts, w=[(0, r), (0.35, r * 0.9), (0.8, r * 0.6), (1, r * 0.3)], n=2, m=K.pick(4, 3),
                               nr=K.pick(7, 3), half=False, kind="extra", B=1.0, paint=LEG, cluster=0.2))
    if lod == 0:  # pleopods under abdominal segments 1-5
        for i in range(5):
            s = 0.5 * (SEG[1 + i] + SEG[2 + i]) if i < 5 else 0.8
            x = 0.030 - s * 0.06
            parts += K.pair(K.tube("pleo%d" % (i + 1), [(x, 0.0012, -0.0006), (x - 0.0012, 0.0026, -0.0026), (x - 0.0026, 0.0030, -0.0032)],
                                   w=[(0, 0.00035), (1, 0.0001)], n=2, m=3, nr=3, half=False, kind="extra", B=1.0, key="pleo", paint=LEG))
        parts = _dedupe_key(parts, "pleo")
    # tail fan: exopod (outer) and endopod (inner) uropods lying flat beside the telson
    parts += K.pair(K.plate("uro_ex", [(-0.0218, 0.0016, -0.0010), (-0.0265, 0.0034, -0.0016), (-0.0315, 0.0049, -0.0021)],
                            [(0, 0.0006), (0.4, 0.0014), (0.85, 0.0012), (1, 0.0004)], nu=K.pick(3, 2), nv=1, normal=(0, 0, 1), paint=TAIL,
                            gadd=[(0, 0.0), (1, 0.25)]))
    parts += K.pair(K.plate("uro_en", [(-0.0222, 0.0009, -0.0006), (-0.0268, 0.0018, -0.0011), (-0.0308, 0.0024, -0.0015)],
                            [(0, 0.0005), (0.4, 0.0011), (0.85, 0.0009), (1, 0.0003)], nu=K.pick(3, 2), nv=1, normal=(0, 0, 1), paint=TAIL,
                            gadd=[(0, 0.0), (1, 0.25)]))
    return parts


def _dedupe_key(parts, prefix):
    """Every pleopod shares the first one's texels: later copies become instances."""
    seen = False
    for p in parts:
        if p.name.startswith(prefix):
            if seen:
                p.mirror[:] = True
            seen = seen or not p.mirror.all()
    return parts
