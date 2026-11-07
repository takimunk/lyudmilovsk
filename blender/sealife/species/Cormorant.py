"""Great cormorant, Phalacrocorax carbo sinensis, swimming underwater, 0.85 m beak tip .. tail tip, horizontal diving glide:
streamlined black body with a bronze-green sheen and scaly bronze wing coverts, long neck stretched forward, long hooked bill
with yellow bare skin at the gape, white cheek / throat patch, emerald eye, wings folded tight on the back (tips crossed over the
rump), stiff wedge tail fan behind, big black totipalmate feet trailing back on both sides of the tail.
The feet propel it: they sit in the flexible rear (G high) and carry B = 1.  The body stays fairly stiff (G low ahead of the tail)."""
import numpy as np

from sealife_geo import nrm

L = 0.85

SPEC = dict(
    id="Cormorant", latin="Phalacrocorax carbo sinensis", length=L, seed=91, tris=4400,
    size=("x", L), close=0.10,
    flex=[(0, 0.02), (0.15, 0.04), (0.35, 0.05), (0.55, 0.06), (0.7, 0.12), (0.85, 0.3), (1.0, 0.45)],
)

SB = 0.096          # bill base (gape) along the body path
PLUM = dict(
    ramp=[(0, (22, 24, 22)), (0.45, (28, 30, 27)), (0.8, (34, 34, 31)), (1, (38, 37, 34))], grain=0.08, grain_sc=0.004,
    layers=[
        dict(type="mottle", col=(46, 54, 38), sc=0.015, th=0.5, soft=0.2, a=0.45, s=(0.18, 1.0), q=(0.0, 0.7)),     # bronze-green sheen
        dict(type="bands", at=list(np.linspace(0.47, 0.98, 22)), w=0.006, rear=True, col=(58, 56, 40), a=0.45, q=(0.0, 0.5)),  # feather rows
        dict(type="speckle", col=(14, 14, 14), cell=0.004, prob=0.6, r=0.25, a=0.4, s=(0.15, 1.0)),
        # bill: dark horn upper mandible, paler lower mandible, gape line, hooked tip darker
        dict(type="stripe", q0=-0.1, q1=0.62, col=(70, 66, 58), a=1.0, s=(-0.1, SB - 0.004), ssoft=0.004, soft=0.02),
        dict(type="stripe", q0=0.62, q1=1.1, col=(150, 138, 110), a=1.0, s=(-0.1, SB - 0.004), ssoft=0.004, soft=0.02),
        dict(type="stripe", q0=0.58, q1=0.66, col=(20, 18, 16), a=0.9, s=(0.0, SB + 0.004), ssoft=0.004, soft=0.01),
        dict(type="tip", s0=0.03, s1=0.0, col=(40, 36, 30), a=0.8),
        # yellow bare skin round the gape and on the chin
        dict(type="spot", s0=SB + 0.004, q0=0.72, rs=0.016, rq=0.3, col=(214, 176, 60), a=0.95),
        # white cheek and throat patch
        dict(type="spot", s0=SB + 0.04, q0=0.78, rs=0.038, rq=0.34, col=(226, 224, 214), a=0.95),
                # white thigh patch (breeding) near the legs
        dict(type="spot", s0=0.86, q0=0.62, rs=0.035, rq=0.12, col=(150, 148, 140), a=0.4),
    ])
WING = dict(col=(62, 56, 38), grain=0.08, grain_sc=0.004, layers=[
    dict(type="bands", at=list(np.linspace(0.04, 0.6, 14)), w=0.035, rear=True, col=(18, 18, 16), a=0.85),   # black-edged bronze coverts
    dict(type="tip", s0=0.55, s1=0.7, col=(26, 26, 24), a=0.95),                                         # black primaries
    dict(type="streak", n=6, w=0.03, col=(44, 44, 40), a=0.6, s=(0.6, 1.0)),
    dict(type="mottle", col=(84, 90, 56), sc=0.02, th=0.6, a=0.4, s=(0.0, 0.55))])
TAIL = dict(col=(24, 24, 22), grain=0.06, grain_sc=0.004, layers=[
    dict(type="streak", n=14, w=0.012, col=(48, 48, 44), a=0.7, s=(0.15, 1.0)),
    dict(type="mottle", col=(40, 44, 34), sc=0.02, th=0.55, a=0.4)])
LEGC = dict(col=(26, 25, 24), grain=0.08, layers=[dict(type="speckle", col=(44, 42, 40), cell=0.02, prob=0.6, r=0.25, a=0.6)])
FOOT = dict(col=(30, 29, 28), alpha=1.0, edge=(18, 17, 16), edge_w=0.2, edge_a=0.8, rag=0.06, grain=0.08,
            layers=[dict(type="streak", n=4, w=0.05, col=(56, 54, 50), a=0.75, s=(0.08, 1.0))])                 # the toes in the web
EYE = dict(col=(40, 120, 84), layers=[dict(type="eye", col=(10, 12, 10), r=0.45, a=0.95, glint=(0.25, 0.3, 0.16)),
                                      dict(type="tip", s0=0.8, s1=1.0, col=(30, 40, 34), a=0.8)])

W_ = [(0, 0.0), (0.01, 0.0030), (0.05, 0.0050), (SB, 0.0090), (0.125, 0.0190), (0.165, 0.0215), (0.21, 0.0165), (0.30, 0.0155),
      (0.42, 0.0190), (0.50, 0.0400), (0.60, 0.0640), (0.72, 0.0720), (0.85, 0.0580), (0.95, 0.0340), (1, 0.0)]
TOP = [(0, 0.0), (0.01, 0.0042), (0.05, 0.0060), (SB, 0.0100), (0.125, 0.0215), (0.165, 0.0235), (0.21, 0.0175), (0.30, 0.0165),
       (0.42, 0.0200), (0.50, 0.0400), (0.60, 0.0550), (0.72, 0.0580), (0.85, 0.0450), (0.95, 0.0250), (1, 0.0)]
BOT = [(0, 0.0), (0.01, 0.0020), (0.05, 0.0048), (SB, 0.0080), (0.125, 0.0160), (0.165, 0.0180), (0.21, 0.0160), (0.30, 0.0170),
       (0.42, 0.0220), (0.50, 0.0450), (0.60, 0.0620), (0.72, 0.0650), (0.85, 0.0480), (0.95, 0.0250), (1, 0.0)]
CTRL = [(0.425, 0, 0.010), (0.412, 0, 0.0170), (0.37, 0, 0.0200), (0.33, 0, 0.0215), (0.28, 0, 0.0180), (0.20, 0, 0.0110), (0.12, 0, 0.0010),
        (0.0, 0, -0.0050), (-0.13, 0, -0.0020), (-0.25, 0, 0.0050)]


def wrap(R=0.075):
    """Bend a flat wing section round the body (centre lower than the edges)."""
    def f(ring, S, q):
        ring = ring.copy()
        C = ring[:, :-1].mean(1)
        T = np.gradient(C, axis=0)
        T = nrm(T)
        wd = nrm(ring[:, 0] - C)
        nd = nrm(np.cross(T, wd))
        out = np.stack([np.zeros(len(C)), C[:, 1], C[:, 2]], 1)
        nd = nd * np.sign(np.einsum("ij,ij->i", nd, out))[:, None]
        lat = np.einsum("nkj,nj->nk", ring - C[:, None], wd)
        ring -= nd[:, None, :] * (lat ** 2 / (2 * R))[..., None]
        return ring
    return f


def build(K):
    lod = K.lod
    parts = []
    body = K.tube("body", CTRL, w=W_, top=TOP, bot=BOT, n=[(0, 2.0), (SB, 2.1), (0.5, 2.0), (1, 2.0)], m=K.pick(13, 7), nr=K.pick(56, 26),
                  cluster=0.25, extra_s=(SB,) if lod == 0 else (), paint=PLUM, dens=1.0)
    parts.append(body)
    # eye: small emerald cap on the side of the head, just behind the gape
    se = 0.125
    c = body.path.at(se)
    from sealife_geo import fn
    wy, tz = fn(W_)(se), fn(TOP)(se)
    parts += K.pair(K.eye("eye", (c[0], 0.90 * wy * np.sin(np.radians(62)), c[2] + 0.90 * tz * np.cos(np.radians(62))), 0.0045,
                          (0.3, 1.0, 0.35), phimax=1.5, nlat=K.pick(3, 2), nseg=K.pick(10, 6), paint=EYE, dens=1.5))
    # folded wings on the upper flanks, tips crossed over the rump
    wing = K.tube("wing", [(0.10, 0.034, 0.026), (0.02, 0.050, 0.042), (-0.10, 0.050, 0.040), (-0.20, 0.029, 0.027), (-0.29, 0.008, 0.024)],
                  top=[(0, 0.006), (0.12, 0.024), (0.28, 0.036), (0.5, 0.036), (0.75, 0.026), (0.92, 0.014), (1, 0.008)],
                  w=[(0, 0.002), (0.12, 0.0045), (0.3, 0.0045), (0.7, 0.003), (0.95, 0.002), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(16, 7),
                  ends=("apex", "edge"), half=False, kind="extra", up=nrm((0, 0.7, 0.7)), roll=90, paint=WING, deform=wrap(0.06), cluster=0.2)
    parts += K.pair(wing)
    # stiff wedge tail, horizontal, from inside the rump to a knife edge
    tail = K.tube("tail", [(-0.20, 0, 0.000), (-0.30, 0, 0.003), (-0.425, 0, 0.006)],
                  top=[(0, 0.018), (0.3, 0.032), (0.7, 0.044), (1, 0.047)],
                  w=[(0, 0.012), (0.3, 0.008), (0.8, 0.0045), (1, 0.0)], n=2.0, m=K.pick(10, 6), nr=K.pick(10, 5),
                  ends=("apex", "edge"), half=False, kind="body", up=(0, 0, 1), roll=90, paint=TAIL, cluster=0.2)
    parts.append(tail)
    # legs: short tarsus out of the rear belly, then the big webbed foot trailing back beside the tail
    tars = K.tube("tarsus", [(-0.12, 0.030, -0.036), (-0.17, 0.046, -0.040), (-0.215, 0.056, -0.034)],
                  w=[(0, 0.0075), (0.5, 0.0060), (1, 0.0050)], top=[(0, 0.0095), (1, 0.0065)], n=2, m=K.pick(6, 4), nr=K.pick(5, 3),
                  half=False, kind="extra", B=1.0, paint=LEGC, gadd=[(0, 0.1), (1, 0.3)], up=(0, 0, 1))
    parts += K.pair(tars)
    foot = K.plate("foot", [(-0.210, 0.056, -0.034), (-0.265, 0.063, -0.030), (-0.322, 0.068, -0.024)],
                   [(0, 0.006), (0.25, 0.014), (0.6, 0.027), (0.9, 0.034), (1, 0.030)], nu=K.pick(4, 2), nv=K.pick(4, 2),
                   normal=(0, 1, 0.15), paint=FOOT, B=1.0, gadd=[(0, 0.3), (1, 0.55)], minpx=16)
    parts += K.pair(foot)
    return parts
