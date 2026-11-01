"""Baltic herring, Clupea harengus membras: slim silver schooling fish, blue-green back, deeply forked tail, 18 cm."""
SPEC = dict(
    id="Herring", latin="Clupea harengus membras", length=0.18, seed=11, tris=1900,
    body_end=0.865, nose_len=0.05,
    # profile: heights/widths are fractions of the total length L; top/bot measured from the body axis
    profile=dict(
        top=[(0, 0), (0.04, 0.034), (0.10, 0.060), (0.22, 0.086), (0.36, 0.094), (0.5, 0.091), (0.65, 0.071), (0.78, 0.042), (0.865, 0.026)],
        bot=[(0, 0), (0.04, 0.034), (0.10, 0.060), (0.22, 0.092), (0.36, 0.100), (0.5, 0.097), (0.65, 0.072), (0.78, 0.042), (0.865, 0.026)],
        wid=[(0, 0), (0.04, 0.020), (0.10, 0.038), (0.22, 0.054), (0.36, 0.060), (0.5, 0.054), (0.65, 0.040), (0.78, 0.022), (0.865, 0.013)],
        n=2.2),
    head=dict(gill_t=0.215, gill_ridge=0.03, mouth_len=0.05, mouth_depth=0.12, mouth_p=0.60),
    eyes=[dict(t=0.078, q=0.40, r=0.021)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.425, t1=0.525, h=0.070, shape="pointed", peak=0.2, rake=18, rays=17, segs=8,
             col=(120, 135, 135), ray=(70, 90, 95), alpha=0.5, edge=(40, 60, 70), edge_a=0.55),
        dict(name="A", kind="median", side="anal", t0=0.675, t1=0.785, h=0.034, shape="pointed", peak=0.15, rake=20, rays=17, segs=8,
             col=(205, 215, 212), ray=(150, 165, 165), alpha=0.45),
        dict(name="C", kind="caudal", span_top=0.088, span_bot=0.088, shape="forked", fork=0.55, rays=19, segs=12,
             col=(105, 120, 125), ray=(55, 75, 85), alpha=0.6, edge=(35, 55, 65), edge_a=0.6, rag=0.03),
        dict(name="Pec", kind="paired", t=0.222, q0=0.78, q1=0.88, len=0.058, shape="pointed", ang0=10, ang1=45, splay=28, rays=15, segs=6,
             col=(215, 222, 218), ray=(160, 175, 175), alpha=0.42),
        dict(name="Pel", kind="paired", t=0.505, q0=0.93, q1=0.95, len=0.040, shape="pointed", ang0=35, ang1=60, splay=35, rays=9, segs=4,
             col=(225, 230, 226), ray=(170, 180, 180), alpha=0.42),
    ],
    paint=dict(
        ramp=[(0.0, (24, 58, 74)), (0.16, (40, 88, 100)), (0.30, (84, 130, 140)), (0.44, (165, 190, 195)), (0.58, (208, 220, 220)),
              (0.78, (232, 236, 233)), (1.0, (242, 242, 238))],
        grain=0.05,
        layers=[
            dict(type="stripe", at=0.27, w=0.07, col=(60, 125, 135), a=0.35, soft=0.05),
            dict(type="sheen", at=0.5, w=0.14, col=(246, 248, 246), a=0.55),
            dict(type="scales", size=0.0042, edge=0.07, sheen=0.05, q=(0.0, 0.9)),
            dict(type="flush", col=(30, 45, 50), q=(0.0, 0.36), t=(0.0, 0.05), a=0.55, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(205, 190, 140), amount=0.22, curve=0.012, line=(70, 75, 70), line_a=0.55),
        mouth=dict(col=(70, 62, 55), w=0.0008),
        eye=dict(iris=(205, 190, 140), iris2=(120, 105, 70), rim=(170, 178, 175), pupil=(4, 5, 7), pupil_r=0.4),
    ),
)
