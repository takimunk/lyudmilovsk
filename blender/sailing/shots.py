"""The review renders (build_all.py --previews DIR [--shots a,b]): each shot clears the scene, builds it from the registry
objects and renders a PNG."""
import math
import os

from mathutils import Matrix, Vector

import boatspec as B
import preview as P


def _boat_shot(reg, out, b, name, cam, target, heel=-8, sheet=22, lens=40):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=True, sand=False)
    P.rigged(reg, b, Matrix.Identity(4), sheet_deg=sheet, heel=heel)
    P.camera(cam, target, lens)
    P.render(os.path.join(out, name))


def shot_luch(reg, out):
    _boat_shot(reg, out, "luch", "luch_a.png", (6.5, 7.5, 3.0), (-0.3, 0, 2.2), heel=-6, sheet=20, lens=32)
    _boat_shot(reg, out, "luch", "luch_b.png", (-6.0, -4.0, 2.6), (0.0, 0, 1.2), heel=-6, sheet=20, lens=32)


def shot_optimist(reg, out):
    _boat_shot(reg, out, "optimist", "optimist_a.png", (4.2, 4.6, 1.8), (-0.3, 0, 1.0), heel=-5, sheet=25, lens=35)
    _boat_shot(reg, out, "optimist", "optimist_b.png", (-3.6, -2.6, 2.4), (0.0, 0, 0.6), heel=-5, sheet=25, lens=35)


def shot_cadet(reg, out):
    _boat_shot(reg, out, "cadet", "cadet_a.png", (5.5, 6.0, 2.4), (-0.2, 0, 1.8), heel=-6, sheet=20, lens=32)
    _boat_shot(reg, out, "cadet", "cadet_b.png", (-5.0, -3.4, 2.6), (0.0, 0, 1.0), heel=-6, sheet=20, lens=32)


def shot_lineup(reg, out):
    P.clear_scene_keep(reg)
    P.scene_setup(sea=False, sand=True, sand_z=-0.30)
    # on the sand, side by side (keels at the sand), sails up, a 1.75 m dummy
    for b, x in (("optimist", -6.2), ("cadet", -1.6), ("luch", 4.4)):
        kz = min(v for _, v in B.BOATS[b]["keel_z"])
        P.rigged(reg, b, Matrix.Translation((x, 0, -0.30 - kz)), sheet_deg=0, heel=0)
    P.dummy(Matrix.Translation((-3.9, 0.9, -0.30)))
    P.camera((-0.5, 17.0, 2.2), (-0.5, 0.0, 2.2), 38)
    P.render(os.path.join(out, "lineup.png"))


SHOTS = {"luch": shot_luch, "optimist": shot_optimist, "cadet": shot_cadet, "lineup": shot_lineup}


def run(reg, out, which=None):
    os.makedirs(out, exist_ok=True)
    try:
        import gear_shots
        SHOTS.update(gear_shots.SHOTS)
    except ImportError:
        pass
    P.prepare_materials()
    for k, fn in SHOTS.items():
        if which and k not in which:
            continue
        fn(reg, out)
