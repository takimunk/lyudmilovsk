# The sky: a red dwarf, Europa, moonlit nights

`ALyudSky` (`Source/.../Sky/LyudSky.*`) puts the sun and the moon where they really stand over the coast at the
calendar's date and hour (`Ephemeris`), with a physical atmosphere, volumetric clouds, a real-time sky light and a
star dome. The world is a simulation, so its sky is not Earth's:

- **The sun is a huge red dwarf**, `SunAngularDiameterDeg` = 10 degrees across (the Sun from Earth: 0.53), a
  ~3200 K star. NPCs are drawn to it (docs/story/motifs.md).
- **The moon is Europa**, `MoonAngularDiameterDeg` = 7 degrees, its phases from the real ephemeris.
- **Nights are moonlit**, not black.

## Drawn by the sky material

`unreal/scripts/import_sky.py` builds `/Game/Scene01/Sky/M_LyudSky` (every scene build) from
`tools/sky/sky_hlsl.py`; the atmosphere's own light disks are off (`AtmosphereSunDiskColorScale` black).

- **Sun** (`SUN_HLSL`): limb darkening and reddening, boiling granulation (Worley cells, slowly animated), sunspot
  groups with a dark umbra in a striated penumbra (new ones every in-game day, drifting as the star turns once in
  `SunRotationDays`), bright faculae near the limb, a soft orange corona and, on some days, prominences on the limb.
  The disk is oriented by the celestial pole, like the real sky.
- **Europa** (`MOON_HLSL`): the USGS Voyager/Galileo mosaic (`textures/sky/europa_2k.jpg`, public domain, see
  `textures/sky/SOURCE.md`) tinted cream ice with red-brown lineae; lit per pixel from the true sun direction
  (Lommel-Seeliger with a little Lambert), so phases and the terminator are right; a faint ashen light on the dark
  side; 270 W (Conamara, Pwyll) faces the coast. Visible by day when it is up (`MoonDayBoost`).
- Both dim and redden toward the horizon (an analytic extinction, `TRANS_HLSL`); stars fade near both disks.

**Brightness is capped**, not physical: the disk centre is `SunIlluminance x SunDiskGain` (the spots read at the
outdoor exposure), Europa `SunIlluminance x MoonDiskGain`. To match, the sunlight's specular is cut
(`SunSpecularScale`, else the sea mirrors a white blob where the star sets), the atmosphere's forward haze is softer
(Mie 0.0045, anisotropy 0.62: the aureole round a 10-degree star would drown the disk), and outdoor bloom is low
(`OutdoorBloom`).

The sunlight's colour (`SunColor`) is the star's, paled toward white so the day stays pleasant; its source angle is
the disk's (soft shadows). Europa's light: `FullMoonIlluminance` 1.1 scene lux at full, cool blue, soft shadows
(source angle 7 degrees), shadows only when the sun is down (one shadowed directional light at a time).

**Night glow**: `NightSkyGlow` adds airglow to the dome at night (brighter toward the horizon), which the sky light
captures, so a moonless night is dark blue and readable. The outdoor exposure opens up by `NightExposureEV` after
sunset, as before; the club inside is untouched (lighting channels).

## Tuning and checks

- `lyud.SkyHourOffset <h>`, `lyud.Sky.SunDiskGain`, `lyud.Sky.MoonDiskGain`, `lyud.Sky.NightLight` (multipliers).
- `unreal/scripts/shoot_cosmic.py` (offscreen): the sun at noon, in the afternoon, at sunset and dusk (wide and
  close), Europa by day and night, full- and new-moon nights over the town and the beach, the tide states
  ([tides.md](tides.md)); `COSMIC_GAINS=0.5,2` adds brightness sweeps, `COSMIC_DIAG=only` the glow diagnostics.
- Gotcha: an `ALyudSky` loaded with its level gets no `OnConstruction` in a game; its dome instance is made in
  `BeginPlay` (else the sky material shows its defaults).
