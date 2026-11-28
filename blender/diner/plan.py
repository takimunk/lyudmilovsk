"""The diner's plan: every shared measure of the car (metres, the diner frame of diner_common.py). building.py (the shell),
seating.py, counter.py and kitchen.py build from these; people_map.py and ALyudDiner read the result from layout.json, so
moving anything here moves the customers' and the staff's spots too (docs/diner.md).
"""

# ---- the car --------------------------------------------------------------------------------------------------------------
WD = 5.4                    # outer width
TH = 0.12                   # wall thickness
YF, YB = -WD / 2, WD / 2    # street front, back (outer faces)
XW = -7.3                   # west end (outer face)
XE = 7.4                    # the nose's centre: straight sides end here, a half circle of radius WD/2 beyond
R = WD / 2
NOSE_SEG = 16
EAVE, CROWN = 2.45, 3.2     # outer wall top and roof crown over the floor
EAVE_IN, CROWN_IN = 2.36, 3.02
WALL_Z0 = -0.15             # the car's skirt under the floor line (the brick foundation below it)
SILL, HEAD = 0.95, 2.05     # the window band
FLOOR_OVER_GROUND = 0.7
DOOR_HOLE = (1.22, 2.22)    # CharacterStandard: clear 110 x 215 -> hole 120 x 218 (+2 cm each way)
DOOR_X = 0.0
KITCHEN_DOOR_X = -6.0
# the kitchen annex behind the west half
AX0, AX1, AY1 = -7.3, -2.0, 6.3
ATH = 0.2
A_TOP = 2.38
BACK_DOOR_X = -3.4

# ---- inside ----------------------------------------------------------------------------------------------------------------
YI_F, YI_B = YF + TH, YB - TH          # inner faces of the long walls
XI_W = XW + TH
COUNTER_X = (-4.6, 3.8)
COUNTER_Y = (0.38, 0.95)               # its body; the top overhangs 0.28..1.02
COUNTER_TOP = 1.02
STOOL_Y, STOOL_SEAT = 0.06, 0.76
STOOLS = [-4.0 + 0.72 * i for i in range(11)]
BOOTHS = [(-2.7, -0.9), (-4.5, -2.7), (-6.3, -4.5), (0.9, 2.7), (2.7, 4.5), (4.5, 6.3)]
BOOTH_Y = -1.76                         # the seats' aisle edge
BACKBAR_Y = 1.95                        # its front; the work aisle behind the counter is 0.95..1.95
BACKBAR_X = (-5.0, XE - 0.1)
BACKBAR_TOP = 0.92
GRILL_X = (4.4, 5.8)
URNS_X = (-4.25, -3.65)
MENU = (-1.6, 2.4, 1.42, 2.08)          # the menu board on the back wall: x0, x1, z0, z1
STATION = (-7.18, -6.62, -1.62, -0.42)  # the self-service coffee table against the west wall: x0, x1, y0, y1
STATION_TOP = 0.92
URN_SELF = (-6.94, -0.82)
NOSE_TABLES = [(XE + 1.35, -0.85), (XE + 1.35, 0.85)]
