"""European eel (yellow eel), Anguilla anguilla: long snake-like round body, one continuous dorsal-tail-anal fringe (dorsal starts well behind
the pectorals), small round pectorals, no pelvics, lower jaw a little longer, olive-brown back, yellowish belly, 65 cm."""
SPEC = dict(
    id="Eel", latin="Anguilla anguilla", length=0.65, seed=81, tris=3200,
    body_end=0.96, nose_len=0.012, nose_len_bot=0.016,
    profile=dict(
        top=[(0, 0), (0.012, 0.0085), (0.03, 0.0135), (0.06, 0.0165), (0.09, 0.0178), (0.2, 0.0200), (0.4, 0.0200), (0.6, 0.0180),
             (0.8, 0.0140), (0.9, 0.0105), (0.96, 0.0060)],
        bot=[(0, 0), (0.012, 0.0085), (0.03, 0.0135), (0.06, 0.0165), (0.09, 0.0178), (0.2, 0.0195), (0.4, 0.0195), (0.6, 0.0175),
             (0.8, 0.0135), (0.9, 0.0100), (0.96, 0.0055)],
        wid=[(0, 0), (0.012, 0.0070), (0.03, 0.0110), (0.06, 0.0140), (0.09, 0.0158), (0.2, 0.0185), (0.4, 0.0188), (0.6, 0.0165),
             (0.8, 0.0122), (0.9, 0.0085), (0.96, 0.0045)],
        n=2.1),
    head=dict(gill_t=0.085, gill_ridge=0.008, mouth_len=0.040, mouth_depth=0.12, mouth_p=0.6),
    eyes=[dict(t=0.030, q=0.30, r=0.0042, sunk=0.2)],
    fins=[
        dict(name="D", kind="median", side="dorsal", t0=0.255, t1=0.945, h=0.0180, shape="spiny", peak=0.3, rake=0, rays=110, segs=30, ends=(0.0, 0.55),
             col=(104, 92, 54), ray=(66, 58, 30), alpha=0.6, edge=(36, 30, 18), edge_a=0.5, rag=0.03),
        dict(name="A", kind="median", side="anal", t0=0.34, t1=0.945, h=0.0180, shape="spiny", peak=0.3, rake=0, rays=90, segs=26, ends=(0.0, 0.55),
             col=(150, 134, 80), ray=(100, 88, 48), alpha=0.55, edge=(60, 52, 30), edge_a=0.4, rag=0.03),
        dict(name="C", kind="caudal", span_top=0.0150, span_bot=0.0150, base_h=0.0180, shape="oval", rays=14, segs=10,
             col=(104, 92, 54), ray=(66, 58, 30), alpha=0.6, edge=(36, 30, 18), edge_a=0.5, rag=0.03),
        dict(name="Pec", kind="paired", t=0.092, q0=0.40, q1=0.56, len=0.030, shape="rounded", ang0=-20, ang1=45, splay=26, rays=16, segs=5,
             col=(128, 112, 66), ray=(84, 72, 40), alpha=0.5, edge=(60, 52, 30), edge_a=0.35),
    ],
    paint=dict(
        ramp=[(0.0, (48, 42, 24)), (0.2, (74, 66, 36)), (0.42, (102, 92, 52)), (0.6, (150, 134, 74)), (0.78, (200, 178, 98)),
              (1.0, (222, 204, 130))],
        grain=0.07,
        layers=[
            dict(type="mottle", scale=0.03, thr=0.55, soft=0.1, col=(40, 36, 20), q=(0.0, 0.5), a=0.35, seed=1),
            dict(type="sheen", at=0.55, w=0.16, col=(236, 214, 130), a=0.22),
            dict(type="scales", size=0.0030, edge=0.02, sheen=0.012, q=(0.0, 0.85), t=(0.1, 1.0)),
            dict(type="flush", col=(60, 54, 30), q=(0.0, 0.7), t=(0.0, 0.05), a=0.4, qsoft=0.08, tsoft=0.01),
        ],
        head=dict(tint=(86, 76, 44), amount=0.18, curve=0.01, line=(40, 34, 20), line_a=0.45),
        mouth=dict(col=(36, 28, 18), w=0.0010),
        eye=dict(iris=(200, 190, 130), iris2=(110, 100, 60), rim=(40, 36, 24), pupil=(3, 3, 3), pupil_r=0.5),
    ),
)
