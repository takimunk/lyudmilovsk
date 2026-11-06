"""Material slots of the town buildings (blender/city). Plain Python: build_city.py (Blender preview) and
unreal/scripts/import_city.py (game materials) both read this table, so both engines agree.

slot: (base colour sRGB hex, roughness, texture set in textures/academ or textures/city or None, tile metres, what it is)
Texture sets are <set>_D/_N/_R.jpg. UV 0 of every mesh is in metres.
"""
MATERIALS = {
    # raw concrete
    "CT_Concrete":      ("#A39E95", 0.9, "concrete", 2.0, "raw board-marked concrete (brutalist walls, slabs, shells)"),
    "CT_ConcreteDark":  ("#6F6B65", 0.9, "concrete", 2.0, "weathered dark concrete: plinths, undersides, retaining faces"),
    "CT_ConcreteWhite": ("#E3DFD6", 0.8, "concrete", 2.0, "white-painted concrete (Brazilian modernism, shells)"),
    "CT_Plaster":       ("#EDE8DC", 0.85, "plaster_white", 2.0, "smooth white lime plaster (constructivist walls)"),
    "CT_PlasterGrey":   ("#B8B4AC", 0.85, "plaster_white", 2.0, "grey plaster"),
    "CT_PlasterPink":   ("#D9A79A", 0.85, "plaster_white", 2.0, "faded terracotta-pink plaster"),
    "CT_PlasterBlue":   ("#8FA9B8", 0.85, "plaster_white", 2.0, "faded sky-blue plaster"),
    "CT_PlasterYellow": ("#E3C374", 0.85, "plaster_ochre", 2.0, "warm yellow plaster"),
    "CT_PlasterGreen":  ("#6E9473", 0.85, "plaster_white", 2.0, "faded green plaster (loggia panels)"),
    "CT_PlasterRed":    ("#A8483A", 0.8, "plaster_white", 2.0, "constructivist red (accents, signs, stair towers)"),
    "CT_Brick":         ("#9C5040", 0.85, "brick_red", 1.0, "red brick"),
    "CT_Tile":          ("#3E6D8E", 0.3, None, 1.0, "blue glazed tile mosaic (azulejo murals, pilotis cladding)"),
    "CT_TileWhite":     ("#E9EDEE", 0.3, None, 1.0, "white glazed tile"),
    "CT_Cobogo":        ("#D8D2C4", 0.85, None, 1.0, "cement breeze-block screen (cobogó): perforated, opaque core"),
    "CT_Louvre":        ("#CFC9BD", 0.8, None, 1.0, "concrete brise-soleil fins"),
    "CT_Steel":         ("#2E3133", 0.55, None, 1.0, "dark painted steel: railings, frames, canopies"),
    "CT_SteelRed":      ("#A3322A", 0.5, None, 1.0, "red painted steel"),
    "CT_SteelWhite":    ("#E6E6E2", 0.5, None, 1.0, "white painted steel (Shukhov lattice, capsule frames)"),
    "CT_Aluminium":     ("#B4B8BA", 0.35, None, 1.0, "anodised aluminium cladding (capsules)"),
    "CT_Frame":         ("#3A3C3D", 0.5, None, 1.0, "window frames and mullions"),
    "CT_FrameWhite":    ("#ECECE8", 0.5, None, 1.0, "white window frames"),
    "CT_Door":          ("#4A3428", 0.6, "timber_stained", 1.0, "timber doors"),
    "CT_Wood":          ("#8A6544", 0.7, "timber_stained", 1.0, "timber: benches, soffits, decks"),
    "CT_Roof":          ("#3B3B3A", 0.9, "asphalt", 2.0, "flat bitumen roof"),
    "CT_Gravel":        ("#8E887E", 1.0, "concrete", 1.0, "gravel roof / terrace"),
    "CT_Paving":        ("#9B958C", 0.9, "concrete", 1.0, "concrete paving slabs"),
    "CT_Copper":        ("#5E8C7A", 0.6, None, 1.0, "green copper"),
    "CT_Stone":         ("#D9D6CE", 0.8, "stone_cast", 1.0, "white cast stone"),
    "CT_Grass":         ("#4F6B33", 1.0, None, 1.0, "roof garden turf"),
    "CT_Interior":      ("#C9C0B0", 0.9, "plaster_white", 2.0, "inside walls of enterable halls"),
    "CT_Floor":         ("#8B8279", 0.7, "concrete", 1.0, "terrazzo-ish inside floors"),
    "CT_Neon":          ("#FF6A3D", 0.4, None, 1.0, "neon letters / lamp glow (emissive at night)"),
    # windows: one opaque card per window (dark glass by day, a lit room at night); not in the shell
    "CT_Window":        ("#1F2A30", 0.05, None, 1.0, "window card: UV0 0..1 over the pane, UV1 = its light texel"),
    "CT_Glass":         ("#2A3A40", 0.05, None, 1.0, "big shop glazing seen from far (opaque, reflective)"),
}

# Materials that emit at night (Unreal: driven by the light texture or the building's hours).
EMISSIVE = {"CT_Window", "CT_Neon"}
