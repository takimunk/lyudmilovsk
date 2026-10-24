"""Shared names for the Academgorodok (Laboratory No. 2 housing, Moscow 1945-48) build.

Units: metres. Local frame of the ensemble: x = east, y = north, z = up, origin at the centre of
Novikova 1's footprint at ground level (OSM 55.8005 N 37.4697 E shifted, see plan.py).
Every mesh carries exactly one of the material slot names below; the Blender preview and the
Unreal import both map these names to real materials (materials.py / unreal/scripts/import_academgorodok.py).
"""

MATERIALS = {
    # name: (base colour sRGB hex, roughness, what it is)
    "AC_Wall":      ("#E2D3A0", 0.9, "ochre lime plaster of the walls"),
    "AC_Trim":      ("#F1EEE4", 0.8, "off-white plaster: cornices, surrounds, columns, parapets, consoles, swags"),
    "AC_RusticDark":("#3A3532", 0.85, "dark rough-faced rusticated portal of Novikova 1"),
    "AC_Quoin":     ("#A39C8E", 0.85, "vermiculated quoin blocks and portal rustication of the side buildings"),
    "AC_Plinth":    ("#7A6F66", 0.9, "plinth band"),
    "AC_Roof":      ("#8E959A", 0.45, "painted standing-seam sheet steel roof"),
    "AC_RoofBrown": ("#4B3329", 0.5, "dark brown painted sheet steel roof (side buildings)"),
    "AC_Brick":     ("#9C5040", 0.85, "red brick chimneys"),
    "AC_Timber":    ("#6A4A2E", 0.7, "stained timber eaves: rafter tails, brackets, soffit boards"),
    "AC_Frame":     ("#F4F3EE", 0.5, "painted wooden window frames and sashes"),
    "AC_Glass":     ("#1E2A30", 0.05, "window glass"),
    "AC_Door":      ("#2A1E18", 0.6, "dark brown panelled entrance doors"),
    "AC_Iron":      ("#2B2B2B", 0.6, "painted iron: railings, bars, downpipe brackets"),
    "AC_IronWhite": ("#ECECEA", 0.6, "white painted iron: flowerbed railing"),
    "AC_Zinc":      ("#B9BDBF", 0.4, "galvanised downpipes, gutters, chimney hoods"),
    "AC_Pylon":     ("#A3A1A0", 0.9, "grey plastered gate scroll pylons"),
    "AC_Stone":     ("#D9D6CE", 0.8, "white cast stone: fountain, kerbs, steps"),
    "AC_Asphalt":   ("#4A4A48", 0.95, "asphalt drives and paths"),
    "AC_Concrete":  ("#9A9690", 0.9, "concrete steps, slabs, kerbs"),
    "AC_Soil":      ("#4B3B2C", 1.0, "flowerbed soil"),
}

# Storey grid (estimates, docs/academgorodok/facade_notes.md)
PLINTH_H = 0.7          # ground floor level over the ground outside
FLOOR_H = 3.8           # floor to floor (3.4 m ceiling + slab; photo ratios, plan.py notes)
SILL_H = 0.9            # window sill over its floor
WIN_W, WIN_H = 1.4, 1.9
