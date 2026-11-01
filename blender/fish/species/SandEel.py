"""Lesser sand eel, Ammodytes tobianus: very slender, pointed head with protruding lower jaw, long low dorsal and anal fins, small forked tail,
no pelvics, silvery with greenish-sandy back, 16 cm."""
SPEC = dict(
    id="SandEel", latin="Ammodytes tobianus", length=0.16, seed=57, tris=1600,
    body_end=0.90, nose_len=0.040, nose_len_bot=0.024,
    profile=dict(
        top=[(0, 0), (0.03, 0.006), (0.06, 0.012), (0.09, 0.021), (0.15, 0.030), (0.3, 0.034), (0.55, 0.033), (0.75, 0.026), (0.9, 0.014)],
        bot=[(0, 0), (0.03, 0.013), (0.06, 0.017), (0.09, 0.020), (0.15, 0.027), (0.3, 0.031), (0.55, 0.030), (0.75, 0.024), (0.9, 0.013)],
        wid=[(0, 0), (0.03, 0.009), (0.08, 0.017), (0.15, 0.023), (0.3, 0.026), (0.55, 0.025), (0.75, 0.019), (0.9, 0.009)],
        n=2.2),
    head=dict(gill_t=0.16, gill_ridge=0.014, mouth_len=0.05, mouth_depth=0.12, mouth_p=0.66),
    lod0=dict(nose_rings=1),
    eyes=[dict(t=0.058, q=0.30, r=0.010)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.20, t1=0.89, h=0.026, shape="fringe", rake=0, rays=46, segs=10,
             col=(176, 180, 140), ray=(120, 128, 90), alpha=0.5, edge=(110, 120, 80), edge_a=0.35, rag=0.03),
        dict(name="A", kind="median", side="anal", t0=0.46, t1=0.89, h=0.020, shape="fringe", rake=0, rays=34, segs=10,
             col=(205, 208, 188), ray=(150, 156, 130), alpha=0.45, rag=0.03),
        dict(name="C", kind="caudal", span_top=0.040, span_bot=0.040, shape="forked", fork=0.45, base_h=0.007, rays=15, segs=8,
             col=(180, 182, 145), ray=(120, 126, 90), alpha=0.55, edge=(100, 110, 75), edge_a=0.4, rag=0.03),
        dict(name="Pec", kind="paired", t=0.17, q0=0.50, q1=0.60, len=0.032, shape="rounded", ang0=0, ang1=40, splay=20, rays=11, segs=4,
             col=(215, 215, 195), ray=(160, 160, 140), alpha=0.4),
    ],
    paint=dict(
        ramp=[(0.0, (76, 98, 62)), (0.18, (112, 134, 88)), (0.34, (160, 168, 112)), (0.5, (200, 205, 180)), (0.7, (228, 230, 220)), (1.0, (240, 240, 232))],
        grain=0.05,
        layers=[
            dict(type="stripe", at=0.40, w=0.08, col=(238, 238, 225), a=0.55, soft=0.06, t=(0.16, 0.95)),
            dict(type="sheen", at=0.55, w=0.14, col=(246, 248, 244), a=0.45),
            dict(type="scales", size=0.0030, edge=0.04, sheen=0.04, q=(0.0, 0.9)),
            dict(type="flush", col=(40, 52, 36), q=(0.0, 0.4), t=(0.0, 0.07), a=0.5, qsoft=0.05, tsoft=0.012),
        ],
        head=dict(tint=(170, 170, 120), amount=0.18, curve=0.010, line=(70, 75, 60), line_a=0.45),
        mouth=dict(col=(70, 62, 55), w=0.0008),
        eye=dict(iris=(200, 190, 130), iris2=(110, 100, 60), rim=(150, 158, 150), pupil=(4, 5, 7), pupil_r=0.42),
    ),
)
