"""Ninespine stickleback, Pungitius pungitius: small, slimmer than the three-spined, a row of 9 small free dorsal spines before the soft dorsal,
very narrow caudal peduncle, olive-brown with darker bars, 6.5 cm."""
SPEC = dict(
    id="NinespineStickleback", latin="Pungitius pungitius", length=0.065, seed=91, tris=1300,
    body_end=0.85, nose_len=0.040,
    profile=dict(
        top=[(0, 0), (0.04, 0.030), (0.10, 0.056), (0.22, 0.076), (0.34, 0.082), (0.48, 0.074), (0.62, 0.048), (0.72, 0.024), (0.78, 0.015), (0.85, 0.015)],
        bot=[(0, 0), (0.04, 0.030), (0.10, 0.052), (0.22, 0.072), (0.34, 0.078), (0.48, 0.068), (0.62, 0.044), (0.72, 0.024), (0.78, 0.015), (0.85, 0.015)],
        wid=[(0, 0), (0.04, 0.018), (0.10, 0.032), (0.22, 0.042), (0.34, 0.045), (0.48, 0.040), (0.62, 0.026), (0.72, 0.013), (0.78, 0.009), (0.85, 0.009)],
        n=2.2),
    head=dict(gill_t=0.26, gill_ridge=0.02, mouth_len=0.05, mouth_depth=0.15, mouth_p=0.55),
    lod0=dict(m=6, rings=30, nose_rings=3), lod1=dict(m=4, nose_rings=2),
    eyes=[dict(t=0.100, q=0.36, r=0.026)],
    fins=[
        dict(name="S%d" % i, kind="median", side="dorsal", t0=0.24 + 0.034 * i, t1=0.24 + 0.034 * i + 0.032, h=0.030 + 0.004 * (i % 2), shape="spiny",
             peak=0.3, rake=8, rays=2, segs=2, col=(140, 140, 110), ray=(70, 70, 50), alpha=0.5, edge=(60, 60, 45), edge_a=0.4)
        for i in range(9)
    ] + [
        dict(name="D", kind="median", side="dorsal", t0=0.58, t1=0.68, h=0.044, shape="pointed", peak=0.3, rake=14, rays=10, segs=5,
             col=(140, 145, 120), ray=(90, 95, 70), alpha=0.45, edge=(60, 65, 50), edge_a=0.4),
        dict(name="A", kind="median", side="anal", t0=0.60, t1=0.70, h=0.040, shape="rounded", peak=0.4, rake=14, rays=9, segs=5,
             col=(200, 205, 190), ray=(140, 145, 125), alpha=0.45),
        dict(name="C", kind="caudal", span_top=0.052, span_bot=0.052, shape="truncate", rays=13, segs=6,
             col=(135, 140, 112), ray=(85, 90, 65), alpha=0.55, edge=(60, 65, 45), edge_a=0.5, rag=0.03),
        dict(name="Pec", kind="paired", t=0.265, q0=0.50, q1=0.60, len=0.060, shape="rounded", ang0=-10, ang1=45, splay=22, rays=11, segs=4,
             col=(205, 208, 190), ray=(150, 152, 130), alpha=0.4),
        dict(name="Pel", kind="paired", t=0.31, q0=0.93, q1=0.96, len=0.050, shape="spiny", ang0=40, ang1=70, splay=25, rays=3, segs=3,
             col=(215, 215, 195), ray=(120, 120, 100), alpha=0.5),
    ],
    paint=dict(
        ramp=[(0.0, (70, 70, 38)), (0.2, (92, 90, 50)), (0.38, (128, 124, 78)), (0.55, (168, 166, 124)), (0.75, (210, 208, 182)), (1.0, (230, 228, 210))],
        grain=0.06,
        layers=[
            dict(type="mottle", scale=0.03, thr=0.55, soft=0.1, col=(52, 50, 28), q=(0.0, 0.5), a=0.4, seed=1),
            dict(type="bars", at=[0.30, 0.40, 0.50, 0.60, 0.70, 0.78], w=0.05, col=(44, 42, 24), a=0.55, q=(0.0, 0.55), wob=0.01, soft=0.02, qsoft=0.08),
            dict(type="sheen", at=0.6, w=0.14, col=(240, 238, 225), a=0.35),
            dict(type="scales", size=0.0035, edge=0.03, sheen=0.03, q=(0.0, 0.9)),
            dict(type="flush", col=(46, 46, 26), q=(0.0, 0.4), t=(0.0, 0.05), a=0.45, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(150, 148, 100), amount=0.18, curve=0.012, line=(56, 54, 36), line_a=0.5),
        mouth=dict(col=(56, 46, 36), w=0.0010),
        eye=dict(iris=(200, 190, 120), iris2=(110, 100, 60), rim=(60, 62, 44), pupil=(4, 5, 6), pupil_r=0.5),
    ),
)
