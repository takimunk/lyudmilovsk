"""Straightnose pipefish, Nerophis ophidion: extremely thin round string-like body, long straight snout, no tail fin (the tail tapers to a point),
tiny dorsal, tiny pectorals, green-olive with faint blue lines on the head, 25 cm."""
SPEC = dict(
    id="StraightPipefish", latin="Nerophis ophidion", length=0.25, seed=85, tris=1500,
    body_end=0.97, nose_len=0.010, tail_cap=0.014,
    profile=dict(
        top=[(0, 0.0014), (0.03, 0.0016), (0.07, 0.0022), (0.10, 0.0030), (0.14, 0.0034), (0.3, 0.0036), (0.6, 0.0032),
             (0.8, 0.0030), (0.9, 0.0026), (0.97, 0.0019)],
        bot=[(0, 0.0014), (0.03, 0.0016), (0.07, 0.0022), (0.10, 0.0030), (0.14, 0.0034), (0.3, 0.0036), (0.6, 0.0032),
             (0.8, 0.0030), (0.9, 0.0026), (0.97, 0.0019)],
        wid=[(0, 0.0013), (0.03, 0.0015), (0.07, 0.0020), (0.10, 0.0027), (0.14, 0.0031), (0.3, 0.0033), (0.6, 0.0029),
             (0.8, 0.0028), (0.9, 0.0024), (0.97, 0.0017)],
        n=2.4),
    head=dict(gill_t=0.11, gill_ridge=0.02, mouth_len=0.010, mouth_depth=0.0, mouth_p=0.7),
    lod0=dict(m=5, rings=44, nose_rings=3), lod1=dict(m=4, nose_rings=2),
    eyes=[dict(t=0.075, q=0.30, r=0.0030, sunk=0.25)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.62, t1=0.73, h=0.012, shape="trapezoid", rake=0, rays=30, segs=8,
             col=(150, 156, 110), ray=(100, 108, 70), alpha=0.4, edge=(70, 80, 50), edge_a=0.3),
    ],
    paint=dict(
        ramp=[(0.0, (64, 78, 36)), (0.25, (84, 100, 48)), (0.5, (112, 126, 66)), (0.75, (150, 156, 96)), (1.0, (186, 186, 130))],
        grain=0.06,
        layers=[
            dict(type="mottle", scale=0.012, thr=0.55, soft=0.1, col=(48, 60, 26), q=(0.0, 0.6), a=0.3, seed=1),
            dict(type="bars", t0=0.14, t1=0.97, period=0.045, w=0.012, col=(196, 190, 130), a=0.3, wob=0.006, soft=0.006),
            dict(type="rings", t0=0.11, t1=0.985, period=0.016, w=0.00035, col=(44, 54, 22), a=0.45),
            dict(type="stripe", at=0.40, w=0.06, col=(110, 170, 200), a=0.5, soft=0.03, t=(0.0, 0.12)),
            dict(type="stripe", at=0.62, w=0.05, col=(110, 170, 200), a=0.4, soft=0.03, t=(0.0, 0.12)),
            dict(type="flush", col=(60, 74, 34), q=(0.2, 0.8), t=(0.0, 0.05), a=0.4),
        ],
        head=dict(tint=(104, 128, 80), amount=0.18, curve=0.005, line=(50, 62, 30), line_a=0.5),
        eye=dict(iris=(190, 170, 80), iris2=(90, 76, 30), rim=(40, 44, 24), pupil=(3, 3, 3), pupil_r=0.5),
    ),
)
