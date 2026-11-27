"""Soda-water automat: the late-Soviet street machine «Газированная вода» on a small concrete pad.

Original model in the spirit of the AT-101 / AT-110 street automats of the 1960s-80s USSR (our own proportions): an
enamelled cabinet with a red lit head, a coin slot for 1 kopeck (plain sparkling water) and one for 3 kopecks (with the
syrup of the day), a stainless niche with the nozzle, the drip grille and the star-shaped washer on which the one shared
faceted glass stands upside down (rinse it, fill it, drink, put it back). The piece itself is interior_kit.soda_machine,
so a building can stand one indoors too.

Not a plot: the machines stand where people pass, listed in tools/city_plan.py STREET_MACHINES (the plan's
"street_machines": the promenade, the bus-stop kiosks, the beach steps, the park, the cable-car station's forecourt).
Frame: origin = the machine's "front" point of the plan (the paving in front of it), x along its front, y behind it.
Footprint 0.9 x 0.75 m (the pad), height 1.91 m. No windows, no flats, no entrances, no lamps (the head's card glows
through CT_Neon's frame only).
Things: plain (left button, 1 kop.), syrup (right button, 3 kop.): acts in tools/city/content/x_soda.json.
Spots: drink (where a resident stands to drink, facing the machine).
Dressing (docs/city_dress.md): "broken" (the cardboard «АВТОМАТ НЕ РАБОТАЕТ» across the niche, May-October on
BROKEN_SHARE of the days: the same die the game's things use to shut the machine, LyudCity::ThingBroken) and "winter"
(November-April: a tarpaulin hood tied over the machine; the things are out of season then).
Requests: none.
"""
import geom
import interior_kit as kit

INFO = {
    "name_ru": "Автомат газированной воды", "name_en": "Soda-water automat",
    "style": "late-soviet street furniture", "use": "public",
    "inspired_by": "the AT-101 / AT-110 street soda-water automats of the 1960s-80s USSR (red lit head, 1 and 3 kopeck "
                   "slots, a shared faceted glass on a washer); proportions and details are our own",
}

PAD = (0.45, 0.375, 0.03)          # half width, half depth, height of the concrete pad
MACHINE_Y = 0.38                   # the cabinet's middle behind the origin (its front face ~0.12 m behind the pad's edge)
SEASON = (5, 6, 7, 8, 9, 10)       # months it works (tools/city/content/x_soda.json "months" must agree)
BROKEN_SHARE = 0.12                # share of the season's days it stands broken (x_soda.json "broken" must agree)


def build(B):
    px, py, ph = PAD
    cy = MACHINE_Y
    # the pad (collision: one steps up 3 cm onto it) and the cabinet
    geom.box(B.m, "CT_Concrete", -px, px, cy - py, cy + py, 0.0, ph)
    r = kit.soda_machine(B, 0.0, cy, ph, 0.0)
    B.footprint = [(-px, cy - py), (px, cy - py), (px, cy + py), (-px, cy + py)]
    B.height = round(r["height"] if "height" in r else ph + r["size"][2], 2)
    B.thing("plain", "machine", r["plain"], 0.0, what="soda", syrup=0)
    B.thing("syrup", "machine", r["syrup"], 0.0, what="soda", syrup=1)
    B.spot("drink", "stand", (0.0, cy - 0.85, 0.0), 180.0)
    # dressing: the bad days' cardboard sign across the niche, the winter hood
    with B.dress("broken", months=SEASON, chance=BROKEN_SHARE):
        _broken_sign(B, r, cy, ph)
    with B.dress("winter", months=tuple(m for m in range(1, 13) if m not in SEASON)):
        _winter_hood(B, cy, ph)


def _broken_sign(B, r, cy, ph):
    """A sheet of cardboard wedged into the niche, a little askew, «АВТОМАТ НЕ РАБОТАЕТ»."""
    p = kit._P(B, 0.0, cy, ph, 0.0, "soda_broken_sign")
    p.kind = "prop"
    y = -0.26 - 0.012
    p.slab("CT_Paper", (-0.22, y, 0.97), (0.22, y, 1.0), (0.22, y, 1.11), (-0.22, y, 1.08), 0.004)
    p.card("soda_broken", (-0.215, y - 0.0005, 0.975), (0.215, y - 0.0005, 1.004), (0.215, y - 0.0005, 1.106),
           (-0.215, y - 0.0005, 1.077))
    p.done(solid=False)


def _winter_hood(B, cy, ph):
    """A green tarpaulin pulled over the cabinet for the winter, tied round with two cords."""
    p = kit._P(B, 0.0, cy, ph, 0.0, "soda_winter_hood")
    hw, hd = 0.375, 0.315
    rings = [kit._ring_rect(-hw - 0.03, hw + 0.03, -hd - 0.03, hd + 0.03, 0.3, 0.04),
             kit._ring_rect(-hw - 0.01, hw + 0.01, -hd - 0.035, hd + 0.01, 1.2, 0.04),
             kit._ring_rect(-hw - 0.012, hw + 0.012, -hd - 0.012, hd + 0.012, 1.9, 0.05),
             kit._ring_rect(-hw + 0.06, hw - 0.06, -hd + 0.05, hd - 0.05, 1.935, 0.05)]
    p.loft("CT_PlasterGreen", rings, top=True, bottom=False)
    for z in (0.62, 1.42):
        p.box("CT_Rubber", -hw - 0.035, hw + 0.035, -hd - 0.045, hd + 0.035, z, z + 0.018, f="xXyY")
    p.done(solid=False)
