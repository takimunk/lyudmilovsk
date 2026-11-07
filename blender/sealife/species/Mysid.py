"""Opossum shrimp, Neomysis integer, 1.5 cm: slender glassy body, big dark eyes on short stalks, antennae, feathery thoracic
legs (alpha strips), tail fan.  Drawn in clouds of hundreds: LOD0 <= 150 triangles."""
SPEC = dict(
    id="Mysid", latin="Neomysis integer", length=0.015, seed=13, tris=150, tris_max=150,
    size=("body_path", 0.015),
    flex=[(0, 0.0), (0.35, 0.02), (0.7, 0.10), (1.0, 0.2)],
)

GLASS = dict(
    ramp=[(0, (128, 130, 118)), (0.5, (152, 154, 142)), (0.8, (170, 168, 154)), (1, (160, 154, 136))], grain=0.08,
    layers=[
        dict(type="stripe", q0=0.62, q1=0.78, col=(132, 118, 86), a=0.55, s=(0.2, 0.8)),          # gut line seen through the body
        dict(type="speckle", col=(70, 52, 40), cell=0.05, prob=0.6, r=0.2, star=0.5, a=0.8, q=(0.75, 1.0)),
        dict(type="bands", at=[0.42, 0.5, 0.58, 0.66, 0.74, 0.82], w=0.008, col=(118, 118, 104), a=0.5),
    ])
EYE = dict(col=(30, 24, 20), layers=[dict(type="eye", col=(10, 8, 8), r=0.85, a=0.95, glint=(0.25, 0.3, 0.25))])
ANT = dict(col=(160, 154, 136), grain=0.05)
FEATHER = dict(col=(170, 168, 152), alpha=0.5, edge=(150, 140, 118), edge_w=0.4, edge_a=0.4, rag=0.25, grain=0.1)
TAIL = dict(col=(160, 158, 142), alpha=0.6, edge=(110, 96, 74), edge_w=0.35, edge_a=0.5, rag=0.08, grain=0.08)


def build(K):
    lod = K.lod
    parts = []
    body = K.tube(
        "body", [(0.0075, 0, 0.0002), (0.004, 0, 0.0004), (-0.001, 0, 0.0003), (-0.0055, 0, 0.0), (-0.0075, 0, -0.0003)],
        w=[(0, 0.0003), (0.08, 0.0007), (0.3, 0.0009), (0.55, 0.0007), (0.85, 0.0004), (1, 0.0002)],
        top=[(0, 0.0003), (0.08, 0.0007), (0.3, 0.0009), (0.55, 0.0007), (0.85, 0.0004), (1, 0.0002)],
        n=2.0, m=K.pick(3, 2), nr=K.pick(6, 4), cluster=0.3, paint=GLASS, minpx=10)
    parts.append(body)
    parts += K.pair(K.eye("eye", (0.0060, 0.00085, 0.0003), 0.00055, (0.4, 1.0, 0.25), nlat=K.pick(2, 1), nseg=K.pick(5, 4), phimax=2.2, paint=EYE))
    if lod == 0:
        parts += K.pair(K.tube("ant", [(0.0068, 0.0004, 0.0002), (0.0095, 0.0010, 0.0006), (0.0120, 0.0022, 0.0004)],
                               w=[(0, 0.00012), (1, 0.00005)], n=2, m=3, nr=2, half=False, kind="extra", paint=ANT, cluster=0.0))
    parts += K.pair(K.plate("legs", [(0.0035, 0.0004, -0.0006), (0.0010, 0.0016, -0.0012)], [(0, 0.0008), (1, 0.0006)],
                            nu=1, nv=1, normal=(0, 0.3, 1), paint=FEATHER, B=1.0, floor=0.6))
    parts.append(K.plate("tail", [(-0.0062, 0, -0.0002), (-0.0090, 0, -0.0004)], [(0, 0.0004), (0.6, 0.0011), (1, 0.0010)],
                         nu=K.pick(2, 1), nv=1, normal=(0, 0, 1), paint=TAIL, gadd=[(0, 0.0), (1, 0.2)], floor=0.4))
    return parts
