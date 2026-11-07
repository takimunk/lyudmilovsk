"""Chinese mitten crab, Eriocheir sinensis (invasive, rare in the Baltic), carapace 7 cm wide: roughly square olive-brown
carapace (slightly wider than long) with four anterolateral teeth a side and a notched frontal margin, eyes in the front corners,
two equal claws held in front whose hands carry thick dark brown hair "mittens" (painted, with a swollen palm), whitish finger
tips, and four pairs of long slender hairy walking legs splayed onto the bottom.  r_len = 11 cm so the claws stay inside R 0..1.
The crab does not bend: flexibility ~0, claws / legs B = 1."""
import numpy as np

SPEC = dict(
    id="MittenCrab", latin="Eriocheir sinensis", length=0.07, r_len=0.11, seed=53, tris=2400,
    size=("y", 0.07), close=0.12,
    flex=[(0, 0.0), (1, 0.03)],
)

TOP = dict(
    ramp=[(0, (92, 88, 54)), (0.7, (86, 80, 48)), (1, (70, 64, 40))], grain=0.1,
    layers=[
        dict(type="mottle", col=(64, 60, 36), sc=0.10, th=0.52, a=0.6),
        dict(type="mottle", col=(120, 114, 74), sc=0.05, th=0.64, a=0.45),
        dict(type="speckle", col=(52, 48, 30), cell=0.012, prob=0.55, r=0.2, a=0.5),
        dict(type="spot", s0=0.42, q0=0.0, rs=0.12, rq=0.22, col=(70, 66, 40), a=0.45),        # gastric region
        dict(type="stripe", q0=0.9, q1=1.01, col=(60, 54, 34), a=0.6),
    ])
BOT = dict(ramp=[(0, (196, 186, 150)), (0.8, (178, 166, 126)), (1, (130, 118, 80))], grain=0.06,
           layers=[dict(type="mottle", col=(160, 150, 114), sc=0.08, th=0.55, a=0.4),
                   dict(type="spot", s0=0.72, q0=0.0, rs=0.30, rq=0.45, col=(160, 146, 108), a=0.6),      # folded abdomen
                   dict(type="stripe", q0=0.9, q1=1.01, col=(120, 108, 72), a=0.5)])
LEG = dict(col=(104, 96, 58), grain=0.12, layers=[
    dict(type="mottle", col=(76, 70, 42), sc=0.06, th=0.5, a=0.5),
    dict(type="streak", n=4, w=0.06, col=(52, 40, 26), a=0.75, s=(0.15, 0.95)),                    # hair fringes along the edges
    dict(type="speckle", col=(46, 36, 24), cell=0.004, prob=0.7, r=0.2, a=0.7, s=(0.15, 0.95)),   # bristles
    dict(type="bands", at=[0.42, 0.72], w=0.02, col=(132, 122, 80), a=0.4),
    dict(type="tip", s0=0.88, s1=1.0, col=(150, 132, 92), a=0.7)])                                  # bare amber dactyl tips
CLAW = dict(col=(104, 96, 58), grain=0.1, layers=[
    dict(type="mottle", col=(76, 70, 42), sc=0.06, th=0.5, a=0.5),
    dict(type="stripe", q0=0.62, q1=0.88, col=(150, 138, 98), a=0.35),
    # the mitten: dense dark brown hair over the hand, thickest on the outer face, ragged at its edge
    dict(type="mottle", col=(52, 36, 22), sc=0.012, th=0.42, soft=0.15, a=0.9, s=(0.52, 0.99), ssoft=0.07),
    dict(type="mottle", col=(46, 32, 20), sc=0.02, th=0.3, soft=0.1, a=0.85, s=(0.66, 0.99), ssoft=0.04),
    dict(type="speckle", col=(34, 22, 14), cell=0.0035, prob=0.85, r=0.3, a=0.85, s=(0.56, 0.98), ssoft=0.06),
    dict(type="speckle", col=(98, 74, 46), cell=0.0045, prob=0.5, r=0.22, a=0.6, s=(0.56, 0.96)),
    dict(type="streak", n=9, w=0.04, col=(28, 18, 12), a=0.5, s=(0.58, 0.95))])
FINGER = dict(col=(110, 98, 64), grain=0.08, layers=[
    dict(type="mottle", col=(52, 36, 22), sc=0.012, th=0.45, a=0.8, s=(0.0, 0.3)),               # hair reaching onto the finger bases
    dict(type="tip", s0=0.5, s1=0.85, col=(226, 216, 190), a=0.95)])
EYE = dict(col=(38, 34, 22), layers=[dict(type="eye", col=(12, 11, 8), r=0.7, a=0.9, glint=(0.2, 0.3, 0.22))])

# carapace outline (+Y half), front (y = 0) round to the back: notched front, orbit, 4 anterolateral teeth, straight sides
OUTLINE = [(0.0290, 0.0), (0.0294, 0.0050), (0.0282, 0.0095), (0.0262, 0.0130), (0.0270, 0.0185), (0.0238, 0.0205), (0.0228, 0.0255),
           (0.0192, 0.0264), (0.0172, 0.0305), (0.0132, 0.0312), (0.0106, 0.0350), (0.0062, 0.0346), (-0.0040, 0.0336), (-0.0150, 0.0306),
           (-0.0240, 0.0250), (-0.0295, 0.0170), (-0.0310, 0.0080), (-0.0312, 0.0)]


def ztop(rho, u):
    h = 0.0110 + 0.0025 * np.sin(np.pi * np.clip(u, 0, 1) * 0.9)
    return -0.0010 + h * np.maximum(1 - rho ** 2.6, 0) ** 0.5


def zbot(rho, u):
    return -0.0010 - 0.0085 * np.maximum(1 - rho ** 2.0, 0) ** 0.6


def build(K):
    lod = K.lod
    parts = K.dome("cara", OUTLINE, 0.0, ztop, zbot, nring=K.pick(5, 3), m=K.pick(24, 12), paint=TOP, paint_bot=BOT)
    parts += K.pair(K.eye("eye", (0.0272, 0.0150, 0.0035), 0.0021, (1.0, 0.4, 0.45), nlat=K.pick(2, 1), nseg=K.pick(7, 5), paint=EYE))
    # claws: arm + swollen hairy palm in one tube, then the fixed finger and the dactyl
    arm = [(0.0160, 0.0200, -0.0060), (0.0270, 0.0330, -0.0050), (0.0365, 0.0290, -0.0022), (0.0425, 0.0190, -0.0016), (0.0470, 0.0100, -0.0016)]
    parts += K.pair(K.tube("claw", arm, w=[(0, 0.0028), (0.35, 0.0033), (0.48, 0.0037), (0.62, 0.0050), (0.75, 0.0060), (0.9, 0.0058), (1, 0.0036)],
                           top=[(0, 0.0028), (0.35, 0.0036), (0.48, 0.0044), (0.62, 0.0064), (0.75, 0.0078), (0.9, 0.0076), (1, 0.0044)],
                           n=2.0, m=K.pick(8, 5), nr=K.pick(11, 5), half=False, kind="extra", B=1.0, paint=CLAW, cluster=0.3))
    for nm, z0, zc in (("pollex", -0.0034, -0.0011), ("dactyl", 0.0011, 0.0006)):
        parts += K.pair(K.tube(nm, [(0.0455, 0.0118, z0), (0.0512, 0.0070, z0 + zc), (0.0548, 0.0018, z0 + zc * 1.6)],
                               w=[(0, 0.0015), (0.5, 0.0012), (1, 0.00035)], top=[(0, 0.0019), (0.5, 0.0014), (1, 0.0004)],
                               n=2, m=K.pick(5, 3), nr=K.pick(5, 3), half=False, kind="extra", B=1.0, key="finger" if nm == "pollex" else "dactyl",
                               paint=FINGER, cluster=0.2))
    # walking legs (pereopods 2-5): long and slender, out under the rim, up over the knee, down onto the bottom
    legs = [(0.0100, 24, 12), (0.0025, 4, 22), (-0.0060, -18, 30), (-0.0140, -40, 34)]     # attach x, merus direction, dactyl sweep back
    for i, (x, ang, sweep) in enumerate(legs):
        a, a2 = np.radians(ang), np.radians(ang - sweep)
        d = np.array([np.sin(a), np.cos(a), 0.0])
        d2 = np.array([np.sin(a2), np.cos(a2), 0.0])
        sc = [0.95, 1.08, 1.05, 0.88][i]
        b0 = np.array([x, 0.0275, -0.0058])
        knee = b0 + d * 0.046 * sc + (0, 0, 0.0110)
        pts = [b0, b0 + d * 0.020 * sc + (0, 0, 0.0055), knee, knee + d2 * 0.020 * sc + (0, 0, 0.0010),
               knee + d2 * 0.034 * sc + (0, 0, -0.0110), knee + d2 * 0.040 * sc + (0, 0, -0.0230)]
        parts += K.pair(K.tube("leg%d" % (i + 2), [tuple(p) for p in pts],
                               w=[(0, 0.0018), (0.45, 0.0016), (0.8, 0.0011), (1, 0.0003)],
                               top=[(0, 0.0026), (0.45, 0.0025), (0.8, 0.0014), (1, 0.0004)], n=2.2, m=K.pick(5, 3), nr=K.pick(11, 6),
                               half=False, kind="extra", B=1.0, paint=LEG, cluster=0.2, up=(0, 0, 1)))
    return parts
