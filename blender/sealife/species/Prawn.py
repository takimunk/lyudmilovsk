"""Baltic prawn, Palaemon adspersus (eelgrass prawn), 5.5 cm rostrum tip .. telson tip: slim laterally compressed glassy body,
long up-curved rostrum with forward-pointing teeth (6 dorsal, 3 ventral), stalked eyes, humped abdomen (bent at the 3rd segment),
very long antennae arching forward and sweeping back past the tail, antennules and antennal scales, two thin chelate leg pairs held
forward, three long thin walking legs hanging down-out (it hangs upright-ish in the weed), paddle pleopods, tail fan.
Pale translucent-looking colours (Body is opaque) with fine dark lines and dots."""
import numpy as np

from sealife_geo import saw, seg_rings, instanced, rotz, fn

L = 0.055
SEG = [0.47, 0.545, 0.615, 0.70, 0.765, 0.825, 0.875]        # carapace end, 6 abdominal segments (3rd humped), telson from 0.875
TIPS = [0.075, 0.115, 0.155, 0.195, 0.235, 0.285]            # dorsal tooth tips (s); ventral teeth on the 2nd..4th
TW = 0.0065                                                  # steep front face of each tooth (s)

SPEC = dict(
    id="Prawn", latin="Palaemon adspersus", length=L, seed=83, tris=2000,
    size=("x", L),
    flex=[(0, 0.0), (0.45, 0.03), (0.65, 0.1), (0.85, 0.18), (1.0, 0.25)],
)

GLASS = dict(
    ramp=[(0, (186, 188, 172)), (0.4, (204, 205, 192)), (0.75, (222, 222, 210)), (1, (232, 230, 220))], grain=0.05,
    layers=[
        dict(type="mottle", col=(170, 178, 150), sc=0.06, th=0.55, a=0.35, q=(0.0, 0.8)),
        dict(type="speckle", col=(84, 62, 46), cell=0.006, prob=0.35, r=0.16, star=0.25, a=0.65, q=(0.0, 0.9), s=(0.25, 1.0)),
        dict(type="bands", at=[0.31, 0.345, 0.38, 0.415, 0.445], w=0.0035, col=(96, 72, 54), a=0.6, s=(0.29, 0.47), q=(0.05, 0.85)),
        dict(type="bands", at=SEG, w=0.0035, col=(90, 66, 50), a=0.75, q=(0.0, 0.95)),
        dict(type="bands", at=[0.5 * (a + b) for a, b in zip(SEG[:-1], SEG[1:])], w=0.0025, col=(112, 86, 62), a=0.45, q=(0.25, 0.95)),
        dict(type="stripe", q0=0.0, q1=0.05, col=(128, 104, 80), a=0.35, s=(0.3, 0.9)),
        dict(type="speckle", col=(232, 214, 120), cell=0.012, prob=0.25, r=0.18, a=0.5, q=(0.3, 0.9), s=(0.3, 0.9)),   # yellow chromatophores
        dict(type="stripe", q0=0.0, q1=1.0, col=(150, 140, 118), a=0.5, s=(0.0, 0.22)),                              # rostrum a touch darker
    ])
LEG = dict(col=(214, 212, 198), grain=0.05, layers=[
    dict(type="bands", at=[0.3, 0.55, 0.78], w=0.03, col=(120, 92, 66), a=0.55),
    dict(type="speckle", col=(220, 196, 96), cell=0.02, prob=0.3, r=0.25, a=0.5)])
CHELA = dict(LEG, layers=LEG["layers"] + [dict(type="tip", s0=0.82, s1=1.0, col=(150, 120, 90), a=0.6)])
ANT = dict(col=(196, 186, 160), grain=0.05, layers=[
    dict(type="bands", at=list(np.linspace(0.05, 0.95, 18)), w=0.008, col=(120, 92, 66), a=0.5)])
STALK = dict(col=(206, 204, 188), grain=0.04)
EYE = dict(col=(56, 46, 34), layers=[dict(type="eye", col=(18, 15, 12), r=0.72, facet=0.0012, glint=(0.25, 0.3, 0.18), a=0.95),
                                     dict(type="tip", s0=0.85, s1=1.0, col=(150, 120, 70), a=0.6)])
PLATE = dict(col=(214, 212, 196), alpha=0.6, edge=(110, 86, 62), edge_w=0.25, edge_a=0.6, rag=0.04, grain=0.04,
             layers=[dict(type="speckle", col=(96, 72, 54), cell=0.012, prob=0.4, r=0.2, star=0.3, a=0.6)])
TAIL = dict(PLATE, layers=PLATE["layers"] + [dict(type="tip", s0=0.55, s1=0.8, col=(108, 84, 60), a=0.55)])
PLEO = dict(col=(212, 210, 196), grain=0.04, layers=[dict(type="tip", s0=0.6, s1=1.0, col=(150, 126, 96), a=0.5)])

W_ = [(0, 0.00012), (0.03, 0.00024), (0.15, 0.00030), (0.235, 0.00045), (0.28, 0.0013), (0.32, 0.0021), (0.40, 0.0025), (0.47, 0.0024),
      (0.6, 0.0022), (0.7, 0.0019), (0.8, 0.0014), (0.875, 0.0009), (0.95, 0.0004), (1, 0.00012)]
TOP = [(0, 0.00012), (0.05, 0.00028), (0.2, 0.00042), (0.26, 0.0009), (0.30, 0.0021), (0.38, 0.0030), (0.47, 0.0031), (0.58, 0.0032),
       (0.66, 0.0033), (0.75, 0.0026), (0.875, 0.0012), (0.94, 0.0006), (1, 0.00012)]
BOT = [(0, 0.00012), (0.05, 0.00026), (0.2, 0.00034), (0.26, 0.0007), (0.30, 0.0017), (0.38, 0.0027), (0.47, 0.0026), (0.6, 0.0024),
       (0.75, 0.0018), (0.875, 0.0010), (0.95, 0.0004), (1, 0.00012)]
CTRL = [(0.0275, 0, 0.0017), (0.020, 0, 0.0006), (0.0125, 0, 0.0), (0.002, 0, 0.0001), (-0.0065, 0, 0.0009), (-0.0140, 0, -0.0003),
        (-0.0210, 0, -0.0021), (-0.0275, 0, -0.0033)]


def teeth(ring, S, q):
    """Forward-pointing rostral teeth: a steep front face (TW) and a long sloping back to the next tooth."""
    hd = np.zeros_like(S)
    for k, t in enumerate(TIPS):
        nxt = TIPS[k + 1] - TW if k + 1 < len(TIPS) else t + 0.035
        up = np.clip((S - (t - TW)) / TW, 0, 1) * (S <= t)
        dn = np.clip((nxt - S) / (nxt - t), 0, 1) * (S > t)
        hd = np.maximum(hd, (up + dn) * (0.00055 if k < 5 else 0.0004))
    hv = np.zeros_like(S)
    for t in TIPS[1:4]:
        nxt = t + 0.04 - TW
        up = np.clip((S - (t - TW)) / TW, 0, 1) * (S <= t)
        dn = np.clip((nxt - S) / (nxt - t), 0, 1) * (S > t)
        hv = np.maximum(hv, (up + dn) * 0.00035)
    wt = np.clip(1 - q / 0.22, 0, 1)[None, :]
    wb = np.clip(1 - (1 - q) / 0.22, 0, 1)[None, :]
    ring = ring.copy()
    ring[..., 2] += hd[:, None] * wt - hv[:, None] * wb
    return ring


def build(K):
    lod = K.lod
    parts = []
    ex = seg_rings(SEG, 0.005)
    if lod == 0:
        ex = ex + [t - TW for t in TIPS] + TIPS
    else:
        ex = [SEG[0] - 0.005, SEG[3]]
    body = K.tube("body", CTRL, w=W_, top=TOP, bot=BOT, n=2.3, m=K.pick(5, 4), nr=K.pick(12, 11), cluster=0.3,
                  mod=saw(SEG, amp=0.06, groove=0.05, gw=0.006), extra_s=ex, deform=teeth if lod == 0 else None, paint=GLASS)
    parts.append(body)
    # stalked eyes at the rostrum base, looking out-forward
    parts += K.pair(K.tube("stalk", [(0.0128, 0.0005, 0.0006), (0.0130, 0.0011, 0.0007), (0.0132, 0.0016, 0.0007)],
                           w=[(0, 0.00055), (1, 0.00060)], n=2, m=4, nr=2, ends=("apex", "apex"), half=False,
                           kind="extra", paint=STALK, cluster=0.0))
    parts += K.pair(K.eye("eye", (0.0132, 0.00185, 0.0007), 0.00085, (0.45, 1.0, 0.15), nlat=K.pick(3, 2), nseg=K.pick(8, 5), paint=EYE))
    # 2nd antennae: forward, arching out and sweeping back far past the tail
    parts += K.pair(K.tube("ant2", [(0.0135, 0.0011, -0.0006), (0.0185, 0.0026, -0.0002), (0.0222, 0.0058, 0.0005), (0.0195, 0.0115, 0.0008),
                                    (0.0070, 0.0170, 0.0), (-0.0120, 0.0205, -0.0012), (-0.0320, 0.0200, -0.0024), (-0.0520, 0.0165, -0.0036)],
                           w=[(0, 0.00024), (0.08, 0.00018), (1, 0.00009)], n=2, m=3, nr=K.pick(28, 20), half=False, kind="extra", dens=0.5,
                           paint=ANT, cluster=0.45))
    # antennules: two flagella per side forward-up (the second an instance)
    a1 = K.tube("ant1", [(0.0138, 0.0006, 0.0004), (0.0205, 0.0018, 0.0016), (0.0275, 0.0032, 0.0026), (0.0335, 0.0046, 0.0030)],
                w=[(0, 0.00020), (1, 0.00006)], n=2, m=3, nr=K.pick(5, 3), half=False, kind="extra", paint=ANT, cluster=0.0)
    parts += K.pair(a1)
    if lod == 0:
        a1b = instanced(a1, "ant1b", rotz(-7), (0.0003, -0.0010, -0.0002))
        parts += K.pair(a1b)
    # antennal scales: narrow plates out-forward beside the rostrum
    parts += K.pair(K.plate("scale", [(0.0128, 0.0023, -0.0002), (0.0165, 0.0030, 0.0), (0.0205, 0.0036, 0.0002)],
                            [(0, 0.00045), (0.4, 0.0008), (0.85, 0.0007), (1, 0.0002)], nu=K.pick(3, 2), nv=1, normal=(0, -0.25, 1), paint=PLATE))
    # pereopods: 1st and 2nd thin chelate pairs held forward-down, 3rd-5th long thin legs hanging down-out
    legs = [  # base x, knee (x, y, z), foot (x, y, z), radius, paint
        (0.0102, (0.0125, 0.0034, -0.0046), (0.0172, 0.0032, -0.0050), 0.00019, CHELA),
        (0.0086, (0.0108, 0.0044, -0.0058), (0.0182, 0.0040, -0.0064), 0.00024, CHELA),
        (0.0068, (0.0072, 0.0055, -0.0040), (0.0072, 0.0072, -0.0092), 0.00021, LEG),
        (0.0048, (0.0042, 0.0060, -0.0040), (0.0025, 0.0078, -0.0094), 0.00022, LEG),
        (0.0028, (0.0012, 0.0058, -0.0038), (-0.0028, 0.0074, -0.0090), 0.00022, LEG),
    ]
    for i, (x, kn, ft, r, pa) in enumerate(legs):
        if lod == 1 and i in (0, 3):
            continue
        b0 = (x, 0.0009, -0.0022)
        b1 = (x - 0.0001, 0.0020, -0.0032)
        mid = tuple(0.5 * (np.array(kn) + np.array(ft)) + np.array([0, 0.0004, 0.0004]))
        pts = [b0, b1, kn, mid, ft]
        if pa is CHELA:   # palm swelling then the fingers
            w = [(0, r), (0.4, r * 0.85), (0.7, r * 0.9), (0.8, r * 1.35), (0.88, r * 1.1), (1, r * 0.3)]
        else:
            w = [(0, r), (0.35, r * 0.9), (0.8, r * 0.6), (1, r * 0.3)]
        parts += K.pair(K.tube("leg%d" % (i + 1), pts, w=w, n=2, m=K.pick(4, 3), nr=K.pick(7, 4), half=False, kind="extra", B=1.0,
                               paint=pa, cluster=0.2))
    # pleopods: paddle swimmerets under abdominal segments 1-5, slanting down-back
    tb = fn(BOT)
    pleo0 = None
    for i in range(5):
        if lod == 1 and i % 2:
            continue
        s = 0.5 * (SEG[i] + SEG[i + 1])
        c = body.path.at(s)
        base = c + np.array([0, 0.0008, -0.75 * tb(s)])
        pts = [tuple(base), tuple(base + (-0.0010, 0.0006, -0.0018)), tuple(base + (-0.0024, 0.0008, -0.0030))]
        pl = K.tube("pleo%d" % (i + 1), pts, w=[(0, 0.00026), (0.6, 0.00028), (1, 0.0001)], n=2, m=3,
                    nr=3, half=False, kind="extra", B=1.0, key="pleo", paint=PLEO, up=(1, 0, 0))
        if pleo0 is not None:
            pl.mirror[:] = True
        pleo0 = pleo0 or pl
        parts += K.pair(pl)
    # tail fan: exopod and endopod uropods spread beside the telson (tilted with the down-bent tail)
    nz = (-0.25, 0, 1)
    parts += K.pair(K.plate("uro_ex", [(-0.0212, 0.0012, -0.0024), (-0.0250, 0.0030, -0.0032), (-0.0292, 0.0046, -0.0041)],
                            [(0, 0.0005), (0.4, 0.0012), (0.85, 0.0011), (1, 0.0004)], nu=K.pick(3, 2), nv=1, normal=nz, paint=TAIL,
                            gadd=[(0, 0.0), (1, 0.25)]))
    parts += K.pair(K.plate("uro_en", [(-0.0216, 0.0006, -0.0022), (-0.0254, 0.0016, -0.0031), (-0.0288, 0.0022, -0.0038)],
                            [(0, 0.0004), (0.4, 0.0010), (0.85, 0.0009), (1, 0.0003)], nu=K.pick(3, 2), nv=1, normal=nz, paint=TAIL,
                            gadd=[(0, 0.0), (1, 0.25)]))
    return parts
