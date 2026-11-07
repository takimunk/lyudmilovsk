"""Baltic whitefish, Coregonus maraena: silvery, small head with a slightly overhanging snout, adipose fin, deeply forked tail, dark-edged fins, 40 cm."""
SPEC = dict(
    id="Whitefish", latin="Coregonus maraena", length=0.40, seed=101, tris=2300,
    body_end=0.865, nose_len=0.04, nose_len_bot=0.03,
    profile=dict(
        top=[(0, 0), (0.03, 0.026), (0.08, 0.046), (0.16, 0.066), (0.26, 0.084), (0.38, 0.092), (0.50, 0.090), (0.62, 0.074), (0.74, 0.052), (0.82, 0.038), (0.865, 0.030)],
        bot=[(0, 0), (0.03, 0.024), (0.08, 0.042), (0.16, 0.062), (0.26, 0.078), (0.38, 0.086), (0.50, 0.084), (0.62, 0.068), (0.74, 0.048), (0.82, 0.036), (0.865, 0.030)],
        wid=[(0, 0), (0.03, 0.016), (0.08, 0.028), (0.16, 0.040), (0.26, 0.050), (0.38, 0.054), (0.50, 0.050), (0.62, 0.040), (0.74, 0.028), (0.82, 0.020), (0.865, 0.015)],
        n=2.2),
    head=dict(gill_t=0.20, gill_ridge=0.022, mouth_len=0.045, mouth_depth=0.12, mouth_p=0.66),
    eyes=[dict(t=0.058, q=0.36, r=0.0125)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.38, t1=0.47, h=0.072, shape="pointed", peak=0.25, rake=14, rays=13, segs=7,
             col=(130, 138, 135), ray=(80, 90, 90), alpha=0.5, edge=(40, 48, 55), edge_a=0.6),
        dict(name="Ad", kind="median", side="dorsal", t0=0.755, t1=0.800, h=0.024, shape="rounded", peak=0.5, rake=0, rays=3, segs=4,
             col=(125, 130, 125), ray=(80, 85, 80), alpha=0.7, edge=(40, 48, 55), edge_a=0.4),
        dict(name="A", kind="median", side="anal", t0=0.665, t1=0.725, h=0.050, shape="pointed", peak=0.25, rake=14, rays=12, segs=6,
             col=(200, 205, 200), ray=(145, 152, 148), alpha=0.5, edge=(60, 68, 72), edge_a=0.45),
        dict(name="C", kind="caudal", span_top=0.100, span_bot=0.100, shape="forked", fork=0.5, rays=20, segs=12,
             col=(110, 118, 118), ray=(60, 70, 74), alpha=0.6, edge=(30, 36, 42), edge_a=0.75, rag=0.03),
        dict(name="Pec", kind="paired", t=0.215, q0=0.78, q1=0.88, len=0.075, shape="pointed", ang0=10, ang1=45, splay=25, rays=14, segs=6,
             col=(190, 195, 190), ray=(130, 138, 135), alpha=0.5, edge=(50, 56, 60), edge_a=0.5),
        dict(name="Pel", kind="paired", t=0.45, q0=0.93, q1=0.96, len=0.065, shape="pointed", ang0=30, ang1=60, splay=30, rays=10, segs=4,
             col=(200, 205, 200), ray=(150, 155, 150), alpha=0.5, edge=(55, 62, 66), edge_a=0.45),
    ],
    paint=dict(
        ramp=[(0.0, (58, 74, 70)), (0.14, (78, 98, 94)), (0.30, (140, 156, 152)), (0.42, (194, 202, 200)), (0.58, (222, 226, 224)),
              (0.80, (238, 238, 234)), (1.0, (244, 243, 238))],
        grain=0.04,
        layers=[
            dict(type="sheen", at=0.46, w=0.16, col=(240, 244, 242), a=0.5),
            dict(type="scales", size=0.0075, edge=0.07, sheen=0.05, q=(0.0, 0.9)),
            dict(type="flush", col=(50, 62, 60), q=(0.0, 0.34), t=(0.0, 0.05), a=0.45, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(150, 160, 150), amount=0.2, curve=0.014, line=(60, 68, 66), line_a=0.5),
        mouth=dict(col=(70, 62, 58), w=0.0010),
        eye=dict(iris=(215, 210, 190), iris2=(120, 115, 95), rim=(160, 168, 165), pupil=(3, 4, 5), pupil_r=0.5),
    ),
)
