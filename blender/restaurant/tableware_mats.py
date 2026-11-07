"""Flat material slots of the restaurant's tableware and food (blender/restaurant/tableware.py), in the diner's MAT_SPEC
style: name -> (hex colour, roughness, metallic, emissive). RS_T_* tableware, RS_F_* food. RS_T_MenuCover is a picture slot
(0..1 UVs over the menu folder's front cover; flat amber leather without a picture). Kept apart from restaurant_common.py
(the building's slots) so the two builds never collide."""

MAT_SPEC = {
    # ---- tableware -------------------------------------------------------------------------------------------------------
    "RS_T_China": ("#F6F3EB", 0.18, 0.0, 0.0),        # the restaurant's white china
    "RS_T_Amber": ("#C27A1C", 0.22, 0.0, 0.0),        # its thin amber band
    "RS_T_ChinaBlack": ("#24211F", 0.2, 0.0, 0.0),     # the pepper shaker
    "RS_T_Glaze": ("#2B2523", 0.16, 0.0, 0.0),         # the ramen bowl's dark glaze
    "RS_T_GlazeRim": ("#8C4A20", 0.2, 0.0, 0.0),       # its rust rim
    "RS_T_Stoneware": ("#B5A288", 0.8, 0.0, 0.0),      # the bowl's unglazed foot
    "RS_T_Steel": ("#B9BCBF", 0.3, 1.0, 0.0),          # cutlery, the coupe, the tray, shaker caps
    "RS_T_Glass": ("#A8BFC2", 0.05, 0.0, 0.0),         # split into the <Name>_Glass meshes
    "RS_T_GlassBase": ("#8FA9A6", 0.08, 0.0, 0.0),     # the thick glass bottoms (main meshes of the tumbler, the carafe)
    "RS_T_Wood": ("#A8743F", 0.55, 0.0, 0.0),          # the khachapuri board
    "RS_T_WoodEdge": ("#7E5230", 0.6, 0.0, 0.0),
    "RS_T_Wicker": ("#B88E52", 0.8, 0.0, 0.0),         # the bread basket
    "RS_T_WickerDark": ("#87612F", 0.8, 0.0, 0.0),
    "RS_T_Linen": ("#F1EDE3", 0.85, 0.0, 0.0),         # napkins
    "RS_T_LinenStripe": ("#B7462C", 0.85, 0.0, 0.0),
    "RS_T_Lacquer": ("#3B1712", 0.25, 0.0, 0.0),       # chopsticks
    "RS_T_Bamboo": ("#D4B47A", 0.6, 0.0, 0.0),
    "RS_T_Leather": ("#A65F1A", 0.5, 0.0, 0.0),        # the menu folder
    "RS_T_MenuCover": ("#A65F1A", 0.5, 0.0, 0.0),      # picture slot: the front cover
    "RS_T_Paper": ("#EFE6D2", 0.9, 0.0, 0.0),          # the menu's page edges
    "RS_T_Gold": ("#D6A23A", 0.3, 1.0, 0.0),           # the emblem
    "RS_T_Nickel": ("#C3C6C4", 0.28, 1.0, 0.0),        # the tea glass holder
    "RS_T_Clay": ("#A5603A", 0.75, 0.0, 0.0),          # the ukha bowl's outside
    "RS_T_ClayGlaze": ("#6E3B1F", 0.2, 0.0, 0.0),      # its glaze inside and on the rim
    "RS_T_Cast": ("#2B2B2D", 0.6, 0.6, 0.0),           # the cast-iron pan
    "RS_T_Aluminium": ("#C9CCCF", 0.38, 1.0, 0.0),     # the stock pot
    "RS_T_Chrome": ("#E6E9EB", 0.06, 1.0, 0.0),        # the service bell
    "RS_T_Black": ("#1B1B1D", 0.35, 0.0, 0.0),         # bakelite handles and knobs
    "RS_T_TicketRed": ("#C23B2A", 0.9, 0.0, 0.0),      # the order tickets' header
    "RS_T_TicketInk": ("#2B2B2B", 0.9, 0.0, 0.0),      # their printed lines
    # ---- food ------------------------------------------------------------------------------------------------------------
    "RS_F_Borscht": ("#8E1A2B", 0.1, 0.0, 0.0),
    "RS_F_Beet": ("#5E0F2A", 0.4, 0.0, 0.0),
    "RS_F_Cabbage": ("#E9D9B4", 0.5, 0.0, 0.0),
    "RS_F_SourCream": ("#F5F0E4", 0.35, 0.0, 0.0),
    "RS_F_Dill": ("#4D7F2A", 0.6, 0.0, 0.0),
    "RS_F_Dough": ("#EEE0C3", 0.45, 0.0, 0.0),
    "RS_F_Filling": ("#7E4E3A", 0.6, 0.0, 0.0),
    "RS_F_Sprat": ("#B07A32", 0.3, 0.0, 0.0),
    "RS_F_SpratDark": ("#5A3B1E", 0.3, 0.0, 0.0),
    "RS_F_Rye": ("#5E3E29", 0.85, 0.0, 0.0),
    "RS_F_RyeCrust": ("#33200F", 0.8, 0.0, 0.0),
    "RS_F_WhiteCrumb": ("#F0DDB2", 0.85, 0.0, 0.0),
    "RS_F_WhiteCrust": ("#B8783A", 0.75, 0.0, 0.0),
    "RS_F_Salmon": ("#E46A3A", 0.35, 0.0, 0.0),
    "RS_F_SalmonFat": ("#F4B48A", 0.35, 0.0, 0.0),
    "RS_F_Lemon": ("#F4DE6A", 0.4, 0.0, 0.0),
    "RS_F_LemonRind": ("#E8B517", 0.35, 0.0, 0.0),
    "RS_F_Onion": ("#B36A98", 0.35, 0.0, 0.0),
    "RS_F_Broth": ("#CF9A55", 0.08, 0.0, 0.0),
    "RS_F_Noodle": ("#EBD38E", 0.45, 0.0, 0.0),
    "RS_F_Chashu": ("#94532F", 0.5, 0.0, 0.0),
    "RS_F_ChashuFat": ("#E9CBA2", 0.45, 0.0, 0.0),
    "RS_F_EggWhite": ("#F7F2E6", 0.35, 0.0, 0.0),
    "RS_F_EggShellBrown": ("#C9975A", 0.4, 0.0, 0.0),   # the marinated egg's outside
    "RS_F_Yolk": ("#EE8E12", 0.25, 0.0, 0.0),
    "RS_F_Nori": ("#1D2A1E", 0.6, 0.0, 0.0),
    "RS_F_SpringOnion": ("#71A83E", 0.5, 0.0, 0.0),
    "RS_F_Naruto": ("#F7F1EC", 0.4, 0.0, 0.0),
    "RS_F_NarutoPink": ("#E0607C", 0.4, 0.0, 0.0),
    "RS_F_Pasta": ("#EFD28E", 0.45, 0.0, 0.0),
    "RS_F_Cream": ("#F2E6C4", 0.3, 0.0, 0.0),
    "RS_F_Parmesan": ("#F5E6A6", 0.6, 0.0, 0.0),
    "RS_F_Pepper": ("#2A2420", 0.7, 0.0, 0.0),
    "RS_F_Steak": ("#5E2C1C", 0.55, 0.0, 0.0),
    "RS_F_SteakIn": ("#B4524A", 0.45, 0.0, 0.0),
    "RS_F_GrillMark": ("#22130C", 0.6, 0.0, 0.0),
    "RS_F_Potato": ("#E0A846", 0.5, 0.0, 0.0),
    "RS_F_PotatoIn": ("#F2DFA4", 0.5, 0.0, 0.0),
    "RS_F_Bean": ("#4C8A2B", 0.45, 0.0, 0.0),
    "RS_F_Gravy": ("#6E3A1D", 0.2, 0.0, 0.0),
    "RS_F_Bread": ("#C98444", 0.6, 0.0, 0.0),           # the khachapuri's crust
    "RS_F_Crumb": ("#F1D9A2", 0.8, 0.0, 0.0),
    "RS_F_Cheese": ("#F6DD86", 0.35, 0.0, 0.0),
    "RS_F_CheeseBrown": ("#D9A04A", 0.4, 0.0, 0.0),
    "RS_F_Butter": ("#F8E58A", 0.25, 0.0, 0.0),
    "RS_F_YolkMix": ("#F2BE3A", 0.3, 0.0, 0.0),
    "RS_F_Olivier": ("#E9DDAA", 0.5, 0.0, 0.0),
    "RS_F_Pea": ("#6FA83A", 0.4, 0.0, 0.0),
    "RS_F_Carrot": ("#E27A26", 0.45, 0.0, 0.0),
    "RS_F_Pickle": ("#59702A", 0.45, 0.0, 0.0),
    "RS_F_Sausage": ("#D99788", 0.45, 0.0, 0.0),
    "RS_F_Parsley": ("#3E7A28", 0.55, 0.0, 0.0),
    "RS_F_HoneyCake": ("#C58640", 0.6, 0.0, 0.0),
    "RS_F_CakeCream": ("#EED7A8", 0.45, 0.0, 0.0),
    "RS_F_CakeCrumbs": ("#A86A2C", 0.85, 0.0, 0.0),
    "RS_F_Plombir": ("#FBF5E4", 0.4, 0.0, 0.0),
    "RS_F_Wafer": ("#DCA85C", 0.6, 0.0, 0.0),
    "RS_F_WaferDark": ("#B8823C", 0.6, 0.0, 0.0),
    "RS_F_BerrySauce": ("#88132B", 0.12, 0.0, 0.0),
    "RS_F_Berry": ("#B0142E", 0.2, 0.0, 0.0),
    "RS_F_Mint": ("#3F8F3A", 0.5, 0.0, 0.0),
    "RS_F_Syrnik": ("#E2B064", 0.55, 0.0, 0.0),
    "RS_F_SyrnikCrust": ("#B9732E", 0.5, 0.0, 0.0),
    "RS_F_SyrnikIn": ("#F4E3B9", 0.6, 0.0, 0.0),
    "RS_F_Jam": ("#8A1830", 0.15, 0.0, 0.0),
    "RS_F_Coffee": ("#2B170C", 0.08, 0.0, 0.0),
    "RS_F_Crema": ("#8C5B32", 0.2, 0.0, 0.0),
    "RS_F_Mors": ("#7C0F26", 0.06, 0.0, 0.0),
    "RS_F_Water": ("#B4D2D8", 0.04, 0.0, 0.0),
    # ---- new dishes (daily special, festival menu) -------------------------------------------------------------------
    "RS_F_Solyanka": ("#B54A1C", 0.08, 0.0, 0.0),
    "RS_F_Olive": ("#3F4B24", 0.3, 0.0, 0.0),
    "RS_F_Ham": ("#A95742", 0.45, 0.0, 0.0),
    "RS_F_Tomato": ("#B3301B", 0.25, 0.0, 0.0),
    "RS_F_CabbageRoll": ("#D2BE82", 0.5, 0.0, 0.0),
    "RS_F_Draniki": ("#C98C3E", 0.55, 0.0, 0.0),
    "RS_F_DranikiDark": ("#8A5522", 0.6, 0.0, 0.0),
    "RS_F_Fish": ("#D8A04A", 0.4, 0.0, 0.0),
    "RS_F_FishIn": ("#F2E9DA", 0.45, 0.0, 0.0),
    "RS_F_FishFin": ("#9A6428", 0.5, 0.0, 0.0),
    "RS_F_FishSkinDark": ("#8A5A28", 0.5, 0.0, 0.0),
    "RS_F_Bone": ("#EEE7D4", 0.6, 0.0, 0.0),
    "RS_F_Pork": ("#B3693A", 0.45, 0.0, 0.0),
    "RS_F_PorkChar": ("#47260F", 0.6, 0.0, 0.0),
    "RS_F_PorkIn": ("#E4C7A0", 0.45, 0.0, 0.0),
    "RS_F_Lavash": ("#E6CC9C", 0.8, 0.0, 0.0),
    "RS_F_LavashToast": ("#C9944E", 0.8, 0.0, 0.0),
    "RS_F_Rice": ("#F7F4EA", 0.5, 0.0, 0.0),
    "RS_F_Tuna": ("#B0314A", 0.35, 0.0, 0.0),
    "RS_F_Tamago": ("#EFBF3A", 0.4, 0.0, 0.0),
    "RS_F_Cucumber": ("#7FA24C", 0.4, 0.0, 0.0),
    "RS_F_Ginger": ("#EAAFA6", 0.4, 0.0, 0.0),
    "RS_F_Wasabi": ("#7BAA38", 0.4, 0.0, 0.0),
    "RS_F_Soy": ("#24120A", 0.05, 0.0, 0.0),
    "RS_F_Blin": ("#E3B565", 0.55, 0.0, 0.0),
    "RS_F_BlinSpot": ("#B97B34", 0.6, 0.0, 0.0),
    "RS_F_BlinIn": ("#F1DCA8", 0.6, 0.0, 0.0),
    "RS_F_Caviar": ("#BF2F1A", 0.15, 0.0, 0.0),
    "RS_F_CaviarBead": ("#E65A24", 0.1, 0.0, 0.0),
    "RS_F_Ukha": ("#D7A63E", 0.06, 0.0, 0.0),
    "RS_F_Pastry": ("#CC8D3F", 0.6, 0.0, 0.0),
    "RS_F_PastryGlaze": ("#A8622A", 0.45, 0.0, 0.0),
    "RS_F_KbRice": ("#F3EBCB", 0.6, 0.0, 0.0),
    "RS_F_Sbiten": ("#B15A19", 0.05, 0.0, 0.0),
    "RS_F_Tea": ("#5E230C", 0.05, 0.0, 0.0),
    "RS_F_Cutlet": ("#B9772F", 0.55, 0.0, 0.0),
    "RS_F_CutletDark": ("#6D3D17", 0.6, 0.0, 0.0),
}
