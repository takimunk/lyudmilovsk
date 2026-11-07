"""Vimba bream, Vimba vimba: bream-like laterally flattened body, grey-blue back, silver sides, fleshy snout overhanging an underslung mouth,
long anal fin, 30 cm."""
SPEC = dict(
    id="Vimba", latin="Vimba vimba", length=0.30, seed=131, tris=2400,
    body_end=0.85, nose_len=0.06, nose_len_bot=0.025,
    profile=dict(
        top=[(0, 0), (0.04, 0.026), (0.10, 0.050), (0.18, 0.074), (0.28, 0.092), (0.38, 0.098), (0.48, 0.092), (0.60, 0.074), (0.72, 0.052), (0.80, 0.038), (0.85, 0.032)],
        bot=[(0, 0), (0.04, 0.020), (0.10, 0.040), (0.18, 0.062), (0.28, 0.080), (0.38, 0.088), (0.48, 0.086), (0.60, 0.072), (0.72, 0.052), (0.80, 0.038), (0.85, 0.032)],
        wid=[(0, 0), (0.04, 0.016), (0.10, 0.030), (0.18, 0.040), (0.28, 0.046), (0.38, 0.048), (0.48, 0.044), (0.60, 0.034), (0.72, 0.024), (0.80, 0.016), (0.85, 0.013)],
        n=2.2),
    head=dict(gill_t=0.215, gill_ridge=0.02, mouth_len=0.05, mouth_depth=0.14, mouth_p=0.78),
    eyes=[dict(t=0.075, q=0.38, r=0.0145)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.40, t1=0.485, h=0.080, shape="rounded", peak=0.25, ends=(0.5, 0.3), rake=14, rays=10, segs=7,
             col=(120, 128, 130), ray=(75, 85, 90), alpha=0.55, edge=(45, 55, 62), edge_a=0.5),
        dict(name="A", kind="median", side="anal", t0=0.585, t1=0.770, h=0.050, shape="trapezoid", peak=0.15, rake=22, rays=22, segs=6,
             col=(190, 195, 190), ray=(140, 148, 145), alpha=0.5, edge=(80, 90, 95), edge_a=0.3),
        dict(name="C", kind="caudal", span_top=0.100, span_bot=0.100, shape="forked", fork=0.45, rays=19, segs=10,
             col=(105, 115, 120), ray=(60, 72, 80), alpha=0.6, edge=(35, 45, 55), edge_a=0.55, rag=0.03),
        dict(name="Pec", kind="paired", t=0.23, q0=0.80, q1=0.90, len=0.060, shape="pointed", ang0=5, ang1=50, splay=22, rays=14, segs=5,
             col=(190, 185, 170), ray=(140, 135, 120), alpha=0.5),
        dict(name="Pel", kind="paired", t=0.44, q0=0.93, q1=0.96, len=0.060, shape="pointed", ang0=30, ang1=60, splay=28, rays=9, segs=4,
             col=(205, 170, 130), ray=(160, 120, 85), alpha=0.55),
    ],
    paint=dict(
        ramp=[(0.0, (50, 68, 82)), (0.16, (72, 96, 112)), (0.30, (120, 144, 156)), (0.42, (184, 198, 204)), (0.58, (214, 222, 222)),
              (0.80, (234, 234, 228)), (1.0, (240, 238, 230))],
        grain=0.05,
        layers=[
            dict(type="sheen", at=0.48, w=0.16, col=(238, 240, 238), a=0.5),
            dict(type="scales", size=0.0070, edge=0.08, sheen=0.05, q=(0.0, 0.9)),
            dict(type="flush", col=(45, 58, 68), q=(0.0, 0.34), t=(0.0, 0.06), a=0.5, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(110, 125, 130), amount=0.2, curve=0.014, line=(55, 62, 66), line_a=0.5),
        mouth=dict(col=(80, 60, 55), w=0.0010),
        eye=dict(iris=(200, 190, 150), iris2=(120, 105, 70), rim=(170, 178, 175), pupil=(3, 3, 3), pupil_r=0.5),
    ),
)
