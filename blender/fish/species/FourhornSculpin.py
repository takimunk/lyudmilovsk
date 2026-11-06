"""Fourhorn sculpin, Myoxocephalus quadricornis: big broad flat head with four bony knobs on top, wide mouth, wide fan pectorals, scaleless dark
grey-brown mottled skin, pale belly, two dorsal fins, 25 cm."""
SPEC = dict(
    id="FourhornSculpin", latin="Myoxocephalus quadricornis", length=0.25, seed=87, tris=3000,
    body_end=0.84, nose_len=0.03, nose_len_bot=0.025,
    profile=dict(
        top=[(0, 0), (0.03, 0.026), (0.08, 0.040), (0.16, 0.050), (0.26, 0.058), (0.34, 0.060), (0.42, 0.056), (0.52, 0.048),
             (0.62, 0.038), (0.72, 0.028), (0.84, 0.018)],
        bot=[(0, 0), (0.03, 0.022), (0.08, 0.032), (0.16, 0.040), (0.26, 0.044), (0.34, 0.046), (0.42, 0.043), (0.52, 0.036),
             (0.62, 0.029), (0.72, 0.022), (0.84, 0.015)],
        wid=[(0, 0), (0.03, 0.034), (0.08, 0.060), (0.16, 0.078), (0.26, 0.086), (0.34, 0.080), (0.42, 0.066), (0.52, 0.050),
             (0.62, 0.036), (0.72, 0.025), (0.84, 0.012)],
        n=[(0, 2.4), (0.2, 2.6), (0.4, 2.3), (0.84, 2.1)]),
    head=dict(gill_t=0.30, gill_ridge=0.02, mouth_len=0.13, mouth_depth=0.12, mouth_p=0.5, mouth_w=0.25),
    eyes=[dict(t=0.125, q=0.10, r=0.0140, sunk=0.2, height=0.9)],
    fins=[
        dict(name="D1", kind="median", side="dorsal", t0=0.36, t1=0.52, h=0.050, shape="spiny", peak=0.35, rake=12, rays=8, segs=8,
             col=(84, 76, 66), ray=(44, 40, 34), alpha=0.55, edge=(30, 28, 24), edge_a=0.5, scallop=0.12,
             bands=[(0.3, 0.55, (40, 36, 30), 0.5)]),
        dict(name="D2", kind="median", side="dorsal", t0=0.55, t1=0.79, h=0.046, shape="rounded", peak=0.35, ends=(0.2, 0.25), rake=10, rays=14, segs=10,
             col=(90, 82, 70), ray=(50, 44, 38), alpha=0.58, edge=(40, 36, 30), edge_a=0.4,
             bands=[(0.25, 0.45, (40, 36, 30), 0.45)]),
        dict(name="A", kind="median", side="anal", t0=0.58, t1=0.79, h=0.040, shape="rounded", peak=0.4, ends=(0.2, 0.25), rake=10, rays=13, segs=9,
             col=(170, 160, 138), ray=(110, 100, 82), alpha=0.55, edge=(90, 82, 66), edge_a=0.3),
        dict(name="C", kind="caudal", span_top=0.058, span_bot=0.058, shape="rounded", rays=16, segs=12,
             col=(90, 82, 70), ray=(50, 44, 38), alpha=0.65, edge=(30, 28, 24), edge_a=0.55, rag=0.03,
             bands=[(0.35, 0.5, (40, 36, 30), 0.5), (0.65, 0.78, (40, 36, 30), 0.4)]),
        dict(name="Pec", kind="paired", t=0.315, q0=0.42, q1=0.74, len=0.17, shape="fan", ang0=-35, ang1=55, splay=14, rays=18, segs=8,
             col=(120, 108, 88), ray=(64, 56, 44), alpha=0.55, edge=(50, 44, 36), edge_a=0.35,
             bands=[(0.3, 0.45, (50, 44, 36), 0.5), (0.6, 0.72, (50, 44, 36), 0.4)]),
        dict(name="Pel", kind="paired", t=0.30, dt=0.02, q0=0.93, q1=0.96, len=0.050, shape="pointed", ang0=25, ang1=65, splay=25, rays=4, segs=3,
             col=(200, 190, 165), ray=(150, 140, 110), alpha=0.5),
    ],
    extras=[
        dict(kind="cones", name="horns", seed=5, seg=6, sink=0.4, col=(96, 84, 66), tip=(176, 160, 124), items=[
            dict(t=0.100, q=0.04, r=0.0105, h=0.012, lean=0.1),
            dict(t=0.185, q=0.04, r=0.0115, h=0.014, lean=0.15),
        ]),
    ],
    paint=dict(
        ramp=[(0.0, (46, 42, 36)), (0.2, (66, 60, 50)), (0.42, (94, 86, 72)), (0.62, (140, 130, 108)), (0.78, (190, 182, 160)),
              (1.0, (218, 212, 192))],
        grain=0.07,
        layers=[
            dict(type="mottle", scale=0.04, thr=0.5, soft=0.1, col=(30, 28, 24), q=(0.0, 0.62), a=0.6, seed=1),
            dict(type="mottle", scale=0.02, thr=0.6, soft=0.07, col=(120, 110, 86), q=(0.0, 0.6), a=0.35, seed=2),
            dict(type="bars", at=[0.38, 0.52, 0.65, 0.76], w=0.06, col=(28, 26, 22), a=0.6, q=(0.0, 0.5), wob=0.02, soft=0.02, qsoft=0.08, t=(0.33, 0.86)),
            dict(type="spots", cell=0.03, r=(0.002, 0.004), prob=0.5, col=(30, 28, 24), q=(0.0, 0.4), t=(0.0, 0.33), a=0.5, seed=6),
        ],
        head=dict(tint=(80, 72, 60), amount=0.12, curve=0.02, line=(34, 30, 26), line_a=0.45),
        mouth=dict(col=(30, 22, 18), w=0.0010),
        eye=dict(iris=(190, 160, 80), iris2=(100, 76, 36), rim=(34, 30, 24), pupil=(3, 3, 3), pupil_r=0.45),
    ),
)
