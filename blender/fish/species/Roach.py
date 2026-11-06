"""Common roach, Rutilus rutilus: deep-ish silvery body, grey-green back, red iris, orange-red pelvic and anal fins, 22 cm."""
SPEC = dict(
    id="Roach", latin="Rutilus rutilus", length=0.22, seed=121, tris=2300,
    body_end=0.845, nose_len=0.04,
    profile=dict(
        top=[(0, 0), (0.03, 0.024), (0.08, 0.046), (0.16, 0.072), (0.26, 0.094), (0.36, 0.102), (0.46, 0.098), (0.58, 0.080), (0.70, 0.056), (0.80, 0.040), (0.845, 0.034)],
        bot=[(0, 0), (0.03, 0.026), (0.08, 0.048), (0.16, 0.072), (0.26, 0.092), (0.36, 0.098), (0.46, 0.094), (0.58, 0.078), (0.70, 0.056), (0.80, 0.040), (0.845, 0.034)],
        wid=[(0, 0), (0.03, 0.016), (0.08, 0.030), (0.16, 0.042), (0.26, 0.050), (0.36, 0.052), (0.46, 0.048), (0.58, 0.038), (0.70, 0.026), (0.80, 0.017), (0.845, 0.014)],
        n=2.2),
    head=dict(gill_t=0.22, gill_ridge=0.022, mouth_len=0.04, mouth_depth=0.12, mouth_p=0.60),
    eyes=[dict(t=0.07, q=0.38, r=0.017)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.41, t1=0.50, h=0.078, shape="rounded", peak=0.25, ends=(0.5, 0.3), rake=14, rays=11, segs=7,
             col=(130, 120, 100), ray=(90, 80, 62), alpha=0.6, edge=(70, 62, 50), edge_a=0.45),
        dict(name="A", kind="median", side="anal", t0=0.585, t1=0.700, h=0.056, shape="trapezoid", peak=0.2, rake=18, rays=12, segs=6,
             col=(225, 100, 50), ray=(175, 60, 30), alpha=0.65, edge=(200, 55, 30), edge_a=0.4),
        dict(name="C", kind="caudal", span_top=0.092, span_bot=0.092, shape="forked", fork=0.38, rays=19, segs=10,
             col=(185, 120, 90), ray=(140, 75, 50), alpha=0.6, edge=(170, 60, 40), edge_a=0.45, rag=0.03),
        dict(name="Pec", kind="paired", t=0.235, q0=0.80, q1=0.90, len=0.060, shape="rounded", ang0=5, ang1=50, splay=22, rays=14, segs=5,
             col=(215, 150, 100), ray=(170, 105, 65), alpha=0.5),
        dict(name="Pel", kind="paired", t=0.44, q0=0.93, q1=0.96, len=0.065, shape="pointed", ang0=30, ang1=60, splay=28, rays=9, segs=4,
             col=(235, 90, 40), ray=(185, 55, 25), alpha=0.7, edge=(205, 45, 25), edge_a=0.4),
    ],
    paint=dict(
        ramp=[(0.0, (52, 66, 52)), (0.16, (78, 96, 76)), (0.30, (128, 145, 120)), (0.42, (186, 196, 186)), (0.58, (214, 220, 212)),
              (0.80, (232, 232, 222)), (1.0, (240, 238, 228))],
        grain=0.05,
        layers=[
            dict(type="sheen", at=0.48, w=0.16, col=(238, 238, 228), a=0.5),
            dict(type="scales", size=0.0085, edge=0.09, sheen=0.05, q=(0.0, 0.9)),
            dict(type="flush", col=(45, 55, 45), q=(0.0, 0.34), t=(0.0, 0.05), a=0.45, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(130, 145, 120), amount=0.18, curve=0.014, line=(60, 68, 58), line_a=0.5),
        mouth=dict(col=(80, 60, 55), w=0.0010),
        eye=dict(iris=(215, 40, 25), iris2=(150, 20, 15), rim=(200, 80, 50), pupil=(3, 3, 3), pupil_r=0.45),
    ),
)
