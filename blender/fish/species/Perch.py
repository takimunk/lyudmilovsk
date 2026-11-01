"""European perch, Perca fluviatilis: deep humped body, two dorsals (spiny first with a black spot at its rear), 5-7 dark vertical bars on a
green-yellow flank, red-orange pelvic and anal fins, 22 cm."""
SPEC = dict(
    id="Perch", latin="Perca fluviatilis", length=0.22, seed=71, tris=2400,
    body_end=0.835, nose_len=0.04,
    profile=dict(
        top=[(0, 0), (0.03, 0.026), (0.08, 0.052), (0.16, 0.088), (0.26, 0.118), (0.34, 0.126), (0.44, 0.116), (0.56, 0.092), (0.68, 0.062), (0.78, 0.040), (0.835, 0.034)],
        bot=[(0, 0), (0.03, 0.026), (0.08, 0.046), (0.16, 0.070), (0.26, 0.090), (0.34, 0.098), (0.44, 0.092), (0.56, 0.074), (0.68, 0.052), (0.78, 0.038), (0.835, 0.034)],
        wid=[(0, 0), (0.03, 0.020), (0.08, 0.036), (0.16, 0.048), (0.26, 0.054), (0.34, 0.056), (0.44, 0.050), (0.56, 0.040), (0.68, 0.028), (0.78, 0.018), (0.835, 0.014)],
        n=2.3),
    head=dict(gill_t=0.255, gill_ridge=0.03, mouth_len=0.07, mouth_depth=0.18, mouth_p=0.60),
    eyes=[dict(t=0.085, q=0.36, r=0.020)],
    fins=[
        dict(name="D1l", kind="median", side="dorsal", t0=0.265, t1=0.500, h=0.100, shape="spiny", peak=0.25, ends=(0.3, 0.35), rake=8, rays=14, segs=7,
             col=(120, 125, 80), ray=(60, 62, 38), alpha=0.6, edge=(40, 42, 28), edge_a=0.5,
             spots=dict(col=(15, 15, 12), cell=1.5, r=0.26, prob=0.45, a=0.95, alpha=0.9)),
        dict(name="D2", kind="median", side="dorsal", t0=0.525, t1=0.665, h=0.062, shape="rounded", peak=0.35, ends=(0.3, 0.25), rake=10, rays=14, segs=7,
             col=(140, 135, 85), ray=(85, 78, 45), alpha=0.55, edge=(70, 62, 36), edge_a=0.5),
        dict(name="A", kind="median", side="anal", t0=0.585, t1=0.700, h=0.056, shape="rounded", peak=0.3, ends=(0.2, 0.25), rake=10, rays=11, segs=6,
             col=(225, 95, 40), ray=(170, 55, 25), alpha=0.65, edge=(190, 50, 25), edge_a=0.5),
        dict(name="C", kind="caudal", span_top=0.095, span_bot=0.095, shape="emarginate", fork=0.18, rays=19, segs=10,
             col=(185, 150, 70), ray=(130, 85, 35), alpha=0.6, edge=(190, 75, 35), edge_a=0.45, rag=0.03),
        dict(name="Pec", kind="paired", t=0.265, q0=0.58, q1=0.70, len=0.075, shape="rounded", ang0=-15, ang1=50, splay=20, rays=14, segs=5,
             col=(205, 175, 90), ray=(150, 110, 50), alpha=0.4),
        dict(name="Pel", kind="paired", t=0.265, q0=0.93, q1=0.96, len=0.095, shape="pointed", ang0=25, ang1=60, splay=22, rays=7, segs=4,
             col=(230, 80, 30), ray=(175, 45, 20), alpha=0.7, edge=(200, 40, 20), edge_a=0.4),
    ],
    paint=dict(
        ramp=[(0.0, (46, 64, 34)), (0.2, (78, 98, 46)), (0.38, (128, 140, 62)), (0.56, (168, 170, 80)), (0.72, (206, 200, 120)), (0.86, (232, 226, 190)), (1.0, (238, 234, 214))],
        grain=0.06,
        layers=[
            dict(type="bars", at=[0.30, 0.37, 0.44, 0.52, 0.60, 0.68], w=0.034, col=(30, 38, 20), a=0.85, soft=0.016, q=(0.0, 0.66), qsoft=0.06, wob=0.014, slant=0.03),
            dict(type="scales", size=0.0075, edge=0.06, sheen=0.04, q=(0.0, 0.9)),
            dict(type="flush", col=(40, 52, 30), q=(0.0, 0.45), t=(0.0, 0.08), a=0.5, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(150, 150, 70), amount=0.2, curve=0.014, line=(40, 46, 28), line_a=0.5),
        mouth=dict(col=(60, 45, 35), w=0.0010),
        eye=dict(iris=(220, 150, 40), iris2=(150, 80, 20), rim=(40, 40, 28), pupil=(3, 3, 3), pupil_r=0.52),
    ),
)
