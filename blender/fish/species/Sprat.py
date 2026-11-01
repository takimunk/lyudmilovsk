"""European sprat, Sprattus sprattus: small herring-like schooling fish, deeper belly with a keel of scutes, dorsal fin just behind the pelvics,
blue-green back, silver sides with golden sheen, 11 cm."""
SPEC = dict(
    id="Sprat", latin="Sprattus sprattus", length=0.11, seed=51, tris=1500,
    body_end=0.865, nose_len=0.045,
    profile=dict(
        top=[(0, 0), (0.04, 0.032), (0.10, 0.056), (0.22, 0.078), (0.36, 0.086), (0.5, 0.082), (0.65, 0.064), (0.78, 0.040), (0.865, 0.024)],
        bot=[(0, 0), (0.04, 0.034), (0.10, 0.064), (0.22, 0.100), (0.36, 0.114), (0.5, 0.108), (0.65, 0.082), (0.78, 0.048), (0.865, 0.026)],
        wid=[(0, 0), (0.04, 0.018), (0.10, 0.034), (0.22, 0.048), (0.36, 0.054), (0.5, 0.048), (0.65, 0.036), (0.78, 0.020), (0.865, 0.012)],
        n=2.2),
    head=dict(gill_t=0.23, gill_ridge=0.028, mouth_len=0.045, mouth_depth=0.12, mouth_p=0.62),
    lod0=dict(nose_rings=2),
    eyes=[dict(t=0.075, q=0.40, r=0.022)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.475, t1=0.575, h=0.075, shape="pointed", peak=0.2, rake=18, rays=16, segs=8,
             col=(110, 128, 130), ray=(70, 92, 98), alpha=0.5, edge=(40, 62, 72), edge_a=0.5),
        dict(name="A", kind="median", side="anal", t0=0.70, t1=0.80, h=0.030, shape="pointed", peak=0.15, rake=20, rays=16, segs=8,
             col=(205, 212, 205), ray=(150, 165, 160), alpha=0.45),
        dict(name="C", kind="caudal", span_top=0.082, span_bot=0.082, shape="forked", fork=0.55, rays=19, segs=12,
             col=(100, 118, 122), ray=(55, 76, 86), alpha=0.6, edge=(35, 55, 65), edge_a=0.6, rag=0.03),
        dict(name="Pec", kind="paired", t=0.235, q0=0.80, q1=0.90, len=0.055, shape="pointed", ang0=10, ang1=45, splay=28, rays=15, segs=6,
             col=(218, 224, 216), ray=(160, 175, 172), alpha=0.42),
        dict(name="Pel", kind="paired", t=0.455, q0=0.94, q1=0.96, len=0.036, shape="pointed", ang0=35, ang1=60, splay=35, rays=8, segs=4,
             col=(228, 230, 222), ray=(172, 182, 178), alpha=0.42),
    ],
    paint=dict(
        ramp=[(0.0, (22, 62, 74)), (0.15, (38, 92, 98)), (0.28, (90, 138, 138)), (0.42, (170, 192, 190)), (0.58, (214, 222, 214)),
              (0.76, (234, 236, 228)), (1.0, (240, 240, 232))],
        grain=0.05,
        layers=[
            dict(type="stripe", at=0.27, w=0.07, col=(70, 130, 130), a=0.3, soft=0.05),
            dict(type="sheen", at=0.46, w=0.14, col=(240, 232, 190), a=0.45),
            dict(type="scales", size=0.0060, edge=0.07, sheen=0.05, q=(0.0, 0.9)),
            # belly keel of scutes: tiny sawtooth ticks along the ventral edge
            dict(type="bars", t0=0.30, t1=0.80, period=0.011, w=0.0030, col=(165, 170, 160), a=0.5, soft=0.002, q=(0.93, 1.0), qsoft=0.02),
            dict(type="flush", col=(30, 50, 55), q=(0.0, 0.36), t=(0.0, 0.05), a=0.5, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(215, 195, 130), amount=0.22, curve=0.012, line=(70, 75, 70), line_a=0.5),
        mouth=dict(col=(70, 62, 55), w=0.0008),
        eye=dict(iris=(215, 195, 130), iris2=(120, 105, 70), rim=(170, 178, 175), pupil=(4, 5, 7), pupil_r=0.4),
    ),
)
