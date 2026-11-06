"""Harris mud crab, Rhithropanopeus harrisii (invasive in the Baltic), carapace 2.5 cm wide: squarish brown carapace with a
straight frontal margin and the widest point just in front of the middle, eyes in the front corners, two stout claws (fingers
dark with whitish tips) folded in front, 4 pairs of flattened walking legs splayed onto the sand.  r_len = 4 cm so the
claws stay inside R = 0..1.  The crab does not bend: flexibility ~0, legs B = 1."""
import numpy as np

SPEC = dict(
    id="MudCrab", latin="Rhithropanopeus harrisii", length=0.025, r_len=0.04, seed=37, tris=2200,
    size=("y", 0.025),
    flex=[(0, 0.0), (1, 0.03)],
)

TOP = dict(
    ramp=[(0, (112, 84, 56)), (0.7, (104, 77, 50)), (1, (84, 62, 40))], grain=0.1,
    layers=[
        dict(type="mottle", col=(72, 52, 34), sc=0.12, th=0.52, a=0.6),
        dict(type="mottle", col=(146, 118, 84), sc=0.06, th=0.66, a=0.45, seed=3),
        dict(type="speckle", col=(60, 44, 30), cell=0.02, prob=0.5, r=0.22, a=0.5),
        dict(type="stripe", q0=0.93, q1=1.01, col=(70, 50, 32), a=0.6),
    ])
BOT = dict(ramp=[(0, (206, 192, 166)), (0.8, (190, 172, 142)), (1, (150, 122, 92))], grain=0.06,
           layers=[dict(type="mottle", col=(176, 160, 132), sc=0.08, th=0.55, a=0.4),
                   dict(type="spot", s0=0.72, q0=0.0, rs=0.28, rq=0.42, col=(172, 150, 118), a=0.6),        # folded abdomen
                   dict(type="stripe", q0=0.9, q1=1.01, col=(150, 120, 88), a=0.5)])
LEG = dict(col=(118, 90, 60), grain=0.1, layers=[
    dict(type="mottle", col=(80, 60, 40), sc=0.08, th=0.5, a=0.5),
    dict(type="bands", at=[0.35, 0.68], w=0.035, col=(154, 128, 94), a=0.45),
    dict(type="tip", s0=0.86, s1=1.0, col=(70, 54, 38), a=0.6)])
CLAW = dict(col=(116, 88, 58), grain=0.1, layers=[
    dict(type="mottle", col=(78, 58, 38), sc=0.08, th=0.5, a=0.5),
    dict(type="stripe", q0=0.62, q1=0.88, col=(170, 146, 112), a=0.4)])            # paler underside of the palm
FINGER = dict(col=(64, 46, 32), grain=0.08, layers=[dict(type="tip", s0=0.55, s1=0.85, col=(236, 228, 210), a=0.95)])
EYE = dict(col=(40, 32, 26), layers=[dict(type="eye", col=(14, 12, 10), r=0.7, a=0.9, glint=(0.2, 0.3, 0.25))])

# carapace outline (+Y half), front (y = 0) round to the back: straight frontal margin, orbits, anterolateral arc, posterior
OUTLINE = [(0.0091, 0.0), (0.0091, 0.0030), (0.0090, 0.0048), (0.0083, 0.0067), (0.0068, 0.0094), (0.0050, 0.0116), (0.0024, 0.0125),
           (-0.0004, 0.0122), (-0.0040, 0.0104), (-0.0072, 0.0080), (-0.0088, 0.0058), (-0.0093, 0.0032), (-0.0094, 0.0)]


def ztop(rho, u):
    h = 0.0040 + 0.0010 * np.sin(np.pi * np.clip(u, 0, 1) * 0.9)
    return -0.0004 + h * np.maximum(1 - rho ** 2.6, 0) ** 0.55


def zbot(rho, u):
    return -0.0004 - 0.0030 * np.maximum(1 - rho ** 2.0, 0) ** 0.6


def build(K):
    lod = K.lod
    parts = K.dome("cara", OUTLINE, 0.0, ztop, zbot, nring=K.pick(7, 3), m=K.pick(16, 7), paint=TOP, paint_bot=BOT)
    parts += K.pair(K.eye("eye", (0.0091, 0.0043, 0.0010), 0.0008, (1.0, 0.35, 0.45), nlat=K.pick(2, 1), nseg=K.pick(7, 5), paint=EYE))
    # claws: arm + palm in one tube, then the fixed finger and the dactyl
    arm = [(0.0055, 0.0075, -0.0020), (0.0095, 0.0118, -0.0018), (0.0128, 0.0104, -0.0008), (0.0150, 0.0068, -0.0006), (0.0168, 0.0036, -0.0006)]
    parts += K.pair(K.tube("claw", arm, w=[(0, 0.0010), (0.35, 0.0012), (0.5, 0.0014), (0.65, 0.0019), (0.9, 0.0019), (1, 0.0011)],
                           top=[(0, 0.0010), (0.35, 0.0013), (0.5, 0.0017), (0.65, 0.0029), (0.9, 0.0028), (1, 0.0014)],
                           n=2.2, m=K.pick(7, 4), nr=K.pick(10, 5), half=False, kind="extra", B=1.0, paint=CLAW, cluster=0.3))
    for nm, z0, zc in (("pollex", -0.0012, -0.0004), ("dactyl", 0.0004, 0.0002)):
        parts += K.pair(K.tube(nm, [(0.0163, 0.0042, z0), (0.0183, 0.0025, z0 + zc), (0.0196, 0.0006, z0 + zc * 1.6)],
                               w=[(0, 0.00055), (0.5, 0.00042), (1, 0.00012)], top=[(0, 0.0007), (0.5, 0.0005), (1, 0.00014)],
                               n=2, m=K.pick(5, 3), nr=K.pick(5, 3), half=False, kind="extra", B=1.0, key="finger" if nm == "pollex" else "dactyl",
                               paint=FINGER, cluster=0.2))
    # walking legs (pereopods 2-5): out of the side under the rim, up over the knee, down onto the sand
    legs = [(0.0034, 22, 10), (0.0008, 4, 22), (-0.0022, -16, 30), (-0.0050, -38, 34)]     # attach x, merus direction, dactyl sweep back
    for i, (x, ang, sweep) in enumerate(legs):
        a, a2 = np.radians(ang), np.radians(ang - sweep)
        d = np.array([np.sin(a), np.cos(a), 0.0])
        d2 = np.array([np.sin(a2), np.cos(a2), 0.0])
        sc = [1.0, 1.08, 1.05, 0.92][i]
        b0 = np.array([x, 0.0092, -0.0020])
        knee = b0 + d * 0.0088 * sc + (0, 0, 0.0026)
        pts = [b0, b0 + d * 0.0042 * sc + (0, 0, 0.0010), knee, knee + d2 * 0.0038 * sc + (0, 0, 0.0006),
               knee + d2 * 0.0068 * sc + (0, 0, -0.0050)]
        parts += K.pair(K.tube("leg%d" % (i + 2), [tuple(p) for p in pts], w=[(0, 0.0007), (0.45, 0.00062), (0.75, 0.00045), (1, 0.00012)],
                               top=[(0, 0.0010), (0.45, 0.0010), (0.75, 0.0006), (1, 0.00014)], n=2.2, m=K.pick(5, 3), nr=K.pick(9, 4),
                               half=False, kind="extra", B=1.0, paint=LEG, cluster=0.2, up=(0, 0, 1)))
    return parts
