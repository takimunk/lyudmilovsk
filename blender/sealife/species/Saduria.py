"""Baltic giant isopod, Saduria entomon, 7 cm: flat segmented oval body (head, 7 pereonites with scalloped lateral margins,
3 short pleon rings) running into a long pointed pleotelson; 7 pairs of legs splayed beneath, two antenna pairs (2nd longer),
small dorsolateral eyes, grey-brown mottled with paler segment margins."""
import numpy as np

from sealife_geo import saw, seg_rings

L = 0.07
SEG = [0.10] + list(np.round(np.linspace(0.10, 0.60, 8)[1:], 4)) + [0.635, 0.665, 0.695]   # head | 7 pereonites | 3 pleon rings | pleotelson

SPEC = dict(
    id="Saduria", latin="Saduria entomon", length=L, seed=47, tris=2500,
    size=("body_path", L),
    flex=[(0, 0.0), (0.3, 0.02), (0.6, 0.06), (0.85, 0.12), (1.0, 0.16)],
)

SHELL = dict(
    ramp=[(0, (118, 108, 90)), (0.4, (126, 116, 97)), (0.62, (150, 141, 122)), (0.8, (182, 174, 156)), (1, (196, 189, 172))], grain=0.08,
    layers=[
        dict(type="mottle", col=(84, 76, 62), sc=0.05, th=0.56, a=0.55, q=(0, 0.7)),
        dict(type="mottle", col=(168, 158, 136), sc=0.035, th=0.62, a=0.45, q=(0, 0.7), seed=2),
        dict(type="speckle", col=(70, 62, 50), cell=0.008, prob=0.5, r=0.22, a=0.6, q=(0, 0.75)),
        dict(type="bands", at=SEG[1:], w=0.022, rear=True, col=(176, 166, 144), a=0.45, q=(0, 0.7)),
        dict(type="bands", at=SEG, w=0.0035, col=(70, 62, 50), a=0.6, q=(0, 0.66)),
        dict(type="stripe", q0=0.0, q1=0.06, col=(104, 95, 78), a=0.4, s=(0.1, 0.9)),
    ])
LEG = dict(col=(160, 150, 128), grain=0.08, layers=[
    dict(type="tip", s0=0.0, s1=0.35, col=(186, 178, 160), a=0.5),
    dict(type="bands", at=[0.35, 0.62, 0.82], w=0.04, col=(118, 108, 90), a=0.5),
    dict(type="tip", s0=0.88, s1=1.0, col=(92, 80, 62), a=0.7)])
ANT = dict(col=(140, 128, 106), grain=0.06, layers=[
    dict(type="bands", at=list(np.linspace(0.3, 0.95, 9)), w=0.015, col=(96, 86, 70), a=0.5)])
EYE = dict(col=(40, 36, 32), layers=[dict(type="eye", col=(12, 11, 10), r=0.85, facet=0.0008, glint=(0.2, 0.25, 0.25), a=0.95)])
URO = dict(col=(128, 118, 98), alpha=1.0, edge=(96, 86, 70), edge_w=0.3, edge_a=0.6, rag=0.03, grain=0.06)


def build(K):
    lod = K.lod
    parts = []
    body = K.tube(
        "body", [(0.035, 0, 0.0002), (0.020, 0, 0.0008), (0.0, 0, 0.0010), (-0.018, 0, 0.0006), (-0.035, 0, -0.0004)],
        w=[(0, 0.0005), (0.02, 0.0040), (0.06, 0.0062), (0.10, 0.0074), (0.2, 0.0104), (0.35, 0.0121), (0.5, 0.0117), (0.6, 0.0098),
           (0.695, 0.0084), (0.8, 0.0058), (0.9, 0.0031), (0.97, 0.0010), (1, 0.0002)],
        top=[(0, 0.0003), (0.04, 0.0018), (0.1, 0.0030), (0.3, 0.0046), (0.5, 0.0046), (0.65, 0.0037), (0.8, 0.0025), (0.95, 0.0008), (1, 0.0002)],
        bot=[(0, 0.0002), (0.05, 0.0010), (0.2, 0.0017), (0.6, 0.0016), (0.8, 0.0010), (1, 0.0002)],
        n=2.2, m=K.pick(8, 4), nr=K.pick(26, 12), cluster=0.35, mod=saw(SEG, amp=0.08, groove=0.05, gw=0.006, power=0.7),
        extra_s=seg_rings(SEG, 0.006) if lod == 0 else (), paint=SHELL)
    parts.append(body)
    parts += K.pair(K.eye("eye", (0.0300, 0.0046, 0.0016), 0.0009, (0.2, 0.7, 0.75), scale=(1.0, 0.75, 0.6), nlat=K.pick(2, 1),
                          nseg=K.pick(7, 5), paint=EYE))
    # antennae: 1st pair short, 2nd pair longer, both forward-outward
    parts += K.pair(K.tube("ant1", [(0.033, 0.0016, 0.0012), (0.037, 0.0030, 0.0018), (0.0405, 0.0048, 0.0020)],
                           w=[(0, 0.00032), (1, 0.00012)], n=2, m=3, nr=K.pick(4, 2), half=False, kind="extra", paint=ANT, cluster=0.0))
    parts += K.pair(K.tube("ant2", [(0.032, 0.0040, 0.0004), (0.036, 0.0072, 0.0006), (0.0395, 0.0115, 0.0002), (0.0410, 0.0165, -0.0008),
                                    (0.0405, 0.0210, -0.0016)],
                           w=[(0, 0.00048), (0.3, 0.00034), (1, 0.00012)], n=2, m=K.pick(4, 3), nr=K.pick(8, 4), half=False, kind="extra",
                           paint=ANT, cluster=0.3))
    # 7 pairs of pereopods, one per pereonite: front three reach forward, back four backward; tips just past the margin
    for i in range(7):
        s = 0.5 * (SEG[i] + SEG[i + 1])
        x = 0.035 - s * L
        fwd = [0.006, 0.004, 0.002, -0.002, -0.004, -0.0055, -0.007][i]
        reach = [0.0128, 0.0145, 0.0155, 0.0160, 0.0158, 0.0150, 0.0135][i]
        pts = [(x, 0.0025, -0.0008), (x + fwd * 0.2, 0.0085, -0.0019), (x + fwd * 0.6, reach - 0.002, -0.0006), (x + fwd, reach, -0.0034)]
        r = 0.00062
        parts += K.pair(K.tube("leg%d" % (i + 1), pts, w=[(0, r), (0.4, r * 0.85), (0.8, r * 0.6), (1, r * 0.3)], n=2, m=K.pick(4, 3),
                               nr=K.pick(6, 3), half=False, kind="extra", B=1.0, paint=LEG, cluster=0.2))
    # uropods: small ventrolateral flaps at the pleotelson base
    parts += K.pair(K.plate("uro", [(-0.0140, 0.0058, -0.0013), (-0.0180, 0.0064, -0.0015), (-0.0222, 0.0062, -0.0017)],
                            [(0, 0.0010), (0.5, 0.0015), (1, 0.0006)], nu=K.pick(2, 1), nv=1, normal=(0, -0.15, 1), paint=URO,
                            gadd=[(0, 0.0), (1, 0.1)]))
    return parts
