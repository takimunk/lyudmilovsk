"""Common eider, Somateria mollissima, adult drake diving, 0.6 m bill tip .. tail tip, horizontal diving pose: stocky heavy duck body,
long wedge-shaped bill running straight into the sloping forehead, black crown cap, pale green nape, white cheeks / neck / back and
pinkish breast, black belly, flanks and rump (a small white patch on the rear flank), short black tail, wings half-open close to the
body for underwater flapping (white forewing, black flight feathers), olive feet trailing beside the tail.
Wings and feet are the paddles: B = 1, the feet sit in the flexible rear; the body stays stiff."""
import numpy as np

from sealife_geo import fn

L = 0.6

SPEC = dict(
    id="Eider", latin="Somateria mollissima", length=L, seed=97, tris=4400,
    size=("x", L), close=0.10,
    flex=[(0, 0.02), (0.2, 0.04), (0.4, 0.05), (0.6, 0.07), (0.75, 0.14), (0.88, 0.3), (1.0, 0.45)],
)

SB = 0.10
WHITE = (234, 232, 224)
BLACK = (24, 24, 24)
PLUM = dict(
    col=WHITE, grain=0.05, grain_sc=0.004,
    layers=[
        dict(type="stripe", q0=0.3, q1=1.2, col=(236, 216, 194), a=0.7, s=(0.30, 0.50), ssoft=0.04),          # pinkish breast
        dict(type="stripe", q0=0.56, q1=1.2, col=BLACK, a=1.0, s=(0.49, 1.1), ssoft=0.025, soft=0.03),           # black flanks + belly
        dict(type="stripe", q0=-0.2, q1=1.2, col=BLACK, a=1.0, s=(0.885, 1.1), ssoft=0.02),                      # black rump
        dict(type="spot", s0=0.86, q0=0.47, rs=0.022, rq=0.07, col=WHITE, a=0.95),                              # white rear-flank patch
        dict(type="stripe", q0=-0.2, q1=0.30, col=BLACK, a=1.0, s=(SB - 0.008, 0.215), ssoft=0.012, soft=0.03),  # black crown cap
        dict(type="spot", s0=SB + 0.012, q0=0.36, rs=0.03, rq=0.07, col=BLACK, a=0.9),                          # cap running down beside the bill
        dict(type="spot", s0=0.25, q0=0.30, rs=0.045, rq=0.12, col=(156, 196, 146), a=0.9),                     # pale green nape
        dict(type="speckle", col=(196, 194, 186), cell=0.006, prob=0.5, r=0.25, a=0.35, s=(0.2, 0.88), q=(0.0, 0.55)),   # feather texture
        dict(type="speckle", col=(44, 44, 44), cell=0.006, prob=0.5, r=0.25, a=0.4, s=(0.49, 1.0), q=(0.6, 1.0)),
        # wedge bill: olive-grey, paler nail at the tip, dark gape line
        dict(type="stripe", q0=-0.2, q1=1.2, col=(150, 158, 108), a=1.0, s=(-0.1, SB - 0.004), ssoft=0.006),
        dict(type="tip", s0=0.025, s1=0.0, col=(206, 200, 160), a=0.9),
        dict(type="stripe", q0=0.60, q1=0.68, col=(48, 50, 36), a=0.85, s=(0.0, SB), ssoft=0.004, soft=0.01),
    ])
WING = dict(col=WHITE, grain=0.05, grain_sc=0.004, layers=[
    dict(type="stripe", q0=0.28, q1=0.72, col=(30, 30, 30), a=1.0, soft=0.03),        # black flight feathers along the trailing half
    dict(type="tip", s0=0.58, s1=0.72, col=(28, 28, 28), a=1.0),                    # black primaries
    dict(type="streak", n=10, w=0.02, col=(60, 60, 60), a=0.5, s=(0.6, 1.0)),
    dict(type="bands", at=list(np.linspace(0.05, 0.55, 8)), w=0.03, rear=True, col=(206, 204, 196), a=0.5, q=(0.0, 0.3))])
TAIL = dict(col=(26, 26, 26), grain=0.06, grain_sc=0.004, layers=[dict(type="streak", n=12, w=0.012, col=(52, 52, 50), a=0.6, s=(0.2, 1.0))])
LEGC = dict(col=(134, 136, 80), grain=0.08, layers=[dict(type="speckle", col=(96, 98, 60), cell=0.02, prob=0.5, r=0.25, a=0.5)])
FOOT = dict(col=(84, 86, 56), alpha=1.0, edge=(54, 54, 40), edge_w=0.2, edge_a=0.8, rag=0.06, grain=0.08,
            layers=[dict(type="streak", n=3, w=0.06, col=(148, 150, 88), a=0.85, s=(0.0, 1.0))])                  # olive toes, darker webs
EYE = dict(col=(52, 36, 24), layers=[dict(type="eye", col=(12, 10, 8), r=0.6, a=0.95, glint=(0.25, 0.3, 0.16))])

W_ = [(0, 0.0), (0.01, 0.0035), (0.05, 0.0085), (SB, 0.0145), (0.14, 0.0240), (0.20, 0.0300), (0.26, 0.0285), (0.32, 0.0350),
      (0.42, 0.0700), (0.55, 0.0950), (0.70, 0.1000), (0.85, 0.0800), (0.95, 0.0450), (1, 0.0)]
TOP = [(0, 0.0), (0.01, 0.0030), (0.05, 0.0070), (SB, 0.0160), (0.14, 0.0300), (0.20, 0.0340), (0.26, 0.0300), (0.32, 0.0320),
       (0.42, 0.0550), (0.55, 0.0650), (0.70, 0.0650), (0.85, 0.0500), (0.95, 0.0300), (1, 0.0)]
BOT = [(0, 0.0), (0.01, 0.0020), (0.05, 0.0050), (SB, 0.0090), (0.14, 0.0180), (0.20, 0.0240), (0.26, 0.0280), (0.32, 0.0360),
       (0.42, 0.0650), (0.55, 0.0800), (0.70, 0.0800), (0.85, 0.0600), (0.95, 0.0350), (1, 0.0)]
CTRL = [(0.30, 0, 0.010), (0.275, 0, 0.016), (0.245, 0, 0.024), (0.21, 0, 0.030), (0.17, 0, 0.026), (0.12, 0, 0.012), (0.03, 0, 0.0),
        (-0.10, 0, 0.0), (-0.24, 0, 0.006)]


def build(K):
    lod = K.lod
    parts = []
    body = K.tube("body", CTRL, w=W_, top=TOP, bot=BOT, n=[(0, 2.2), (SB, 2.2), (0.3, 2.0), (1, 2.1)], m=K.pick(13, 7), nr=K.pick(54, 26),
                  cluster=0.25, extra_s=(SB,) if lod == 0 else (), paint=PLUM)
    parts.append(body)
    se = 0.17
    c = body.path.at(se)
    wy, tz = fn(W_)(se), fn(TOP)(se)
    parts += K.pair(K.eye("eye", (c[0], 0.90 * wy * np.sin(np.radians(60)), c[2] + 0.90 * tz * np.cos(np.radians(60))), 0.0042,
                          (0.3, 1.0, 0.3), phimax=1.5, nlat=K.pick(3, 2), nseg=K.pick(10, 6), paint=EYE, dens=1.5))
    # wings half-open, close to the body and slightly out: the underwater flapping paddles
    wing = K.tube("wing", [(0.07, 0.050, 0.030), (0.0, 0.105, 0.036), (-0.09, 0.140, 0.032), (-0.19, 0.150, 0.026)],
                  top=[(0, 0.018), (0.15, 0.042), (0.4, 0.050), (0.75, 0.036), (0.95, 0.018), (1, 0.012)],
                  w=[(0, 0.012), (0.3, 0.008), (0.8, 0.004), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(16, 7),
                  ends=("apex", "edge"), half=False, kind="extra", up=(0, -0.25, 1), roll=90, B=1.0, paint=WING,
                  gadd=[(0, 0.0), (1, 0.15)], cluster=0.2)
    parts += K.pair(wing)
    # short black tail, horizontal, from inside the rump to a knife edge
    parts.append(K.tube("tail", [(-0.20, 0, 0.008), (-0.25, 0, 0.012), (-0.30, 0, 0.016)],
                        top=[(0, 0.016), (0.4, 0.028), (1, 0.034)], w=[(0, 0.010), (0.4, 0.007), (1, 0.0)], n=2.0, m=K.pick(10, 6),
                        nr=K.pick(7, 4), ends=("apex", "edge"), half=False, kind="body", up=(0, 0, 1), roll=90, paint=TAIL, cluster=0.2))
    # legs: tarsus out of the rear belly, webbed foot trailing beside the tail
    parts += K.pair(K.tube("tarsus", [(-0.10, 0.040, -0.050), (-0.15, 0.056, -0.054), (-0.195, 0.064, -0.046)],
                           w=[(0, 0.0075), (0.5, 0.0060), (1, 0.0050)], top=[(0, 0.0095), (1, 0.0065)], n=2, m=K.pick(6, 4),
                           nr=K.pick(5, 3), half=False, kind="extra", B=1.0, paint=LEGC, gadd=[(0, 0.1), (1, 0.3)], up=(0, 0, 1)))
    parts += K.pair(K.plate("foot", [(-0.190, 0.064, -0.046), (-0.235, 0.070, -0.041), (-0.282, 0.074, -0.034)],
                            [(0, 0.006), (0.25, 0.013), (0.6, 0.024), (0.9, 0.030), (1, 0.026)], nu=K.pick(4, 2), nv=K.pick(4, 2),
                            normal=(0, 1, 0.2), paint=FOOT, B=1.0, gadd=[(0, 0.3), (1, 0.55)], minpx=16))
    return parts
