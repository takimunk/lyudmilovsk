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

- **Sun** (`SUN_HLSL`): a deep red-orange ~3000 K disk with strong limb darkening (about 0.15 of the centre at the
  edge) and reddening; boiling granulation (Worley cells, each a little brighter or dimmer, soft dark lanes; contrast
  `SunGranulation`); 3-4 sunspot groups a day, each a big leader with 0-2 smaller followers trailing it, a dark umbra
  (`SunSpotUmbra`) in a finely striated penumbra. The groups are put on the face the star turns to us at the day's noon
  (`SunSpots.x`), so 4-8 spots cross the disk while it is up, drifting as it turns once in `SunRotationDays`. Faculae
  (`SunFaculae`) round the groups and a faint network near the limb; a soft orange corona and, on some days,
  prominences on the limb. The disk is oriented by the celestial pole, like the real sky. Over the disk most of the
  atmosphere's own glow is taken off (`DISK_MASK_HLSL`): added to the capped disk it paled it to pink.
- **Europa** (`MOON_HLSL`): the USGS Voyager/Galileo mosaic (`textures/sky/europa_2k.jpg`, public domain, see
  `textures/sky/SOURCE.md`) tinted cream ice with red-brown lineae, its midtones stretched by `MoonContrast` so the
  lineae and chaos terrain read; lit per pixel from the true sun direction (Lommel-Seeliger with a little Lambert), so
  phases and the terminator are right; a faint ashen light on the dark side; 270 W (Conamara, Pwyll) faces the coast.
  Visible by day when it is up (`MoonDayBoost`).
- Both dim and redden toward the horizon (an analytic extinction, `TRANS_HLSL`); stars fade near both disks.

**Brightness is capped**, not physical: the disk centre is `SunIlluminance x SunDiskGain` (the spots read at the
outdoor exposure), Europa `SunIlluminance x MoonDiskGain`. To match, the sunlight's specular is cut
(`SunSpecularScale`, else the sea mirrors a white blob where the star sets), the atmosphere's forward haze is softer
(Mie 0.0045, anisotropy 0.62: the aureole round a 10-degree star would drown the disk), and outdoor bloom is low
(`OutdoorBloom`).

**The clouds hide the sun.** The volumetric clouds are composited over the dome, so they occlude the disk by their
transmittance; the engine's simple cloud material is thin, so `CloudDensityScale` (3) thickens it (its
`Cloud_GlobalDensity`, through a dynamic instance), and the low red sun goes behind the clouds instead of glowing
through them like a lamp in fog.

**The sun on the sea.** The sun's image on calm water was a hard-edged semicircle: mostly the sunlight's own specular
(a 10-degree area light mirrored by flat water), which no cloud in the view hides, plus the sky light's cubemap. Now
the specular is cut to the disk's real brightness (`SunSpecularScale` 0.012) and the sea is never a mirror: capillary
ripples (`tools/sea/sea_hlsl.py`, `Glitter.x` slope per train, 6 trains of 0.12-2 m, deep-water dispersion) break the
image into glitter near the camera and, where they are finer than a pixel, go into the roughness, so far off the
image stretches into a path. `lyud.Sea.Capillary` overrides the slope.

**Nights round Europa**: the haze is thinner by night (`NightMieShare` of the day's Mie), bloom drops to `NightBloom`,
and the moonlight scatters less in the clouds (`MoonCloudScatter`): moonlit haze and clouds round a 7-degree moon at
the night exposure made a big halo.

The sunlight's colour (`SunColor`) is the star's, paled toward white so the day stays pleasant; its source angle is
the disk's (soft shadows). Europa's light: `FullMoonIlluminance` 1.1 scene lux at full, cool blue, soft shadows
(source angle 7 degrees), shadows only when the sun is down (one shadowed directional light at a time).

**Night glow**: `NightSkyGlow` adds airglow to the dome at night (brighter toward the horizon), which the sky light
captures, so a moonless night is dark blue and readable. The outdoor exposure opens up by `NightExposureEV` after
sunset, as before; the club inside is untouched (lighting channels).

## Tuning and checks

- `lyud.SkyHourOffset <h>`, `lyud.Sky.SunDiskGain`, `lyud.Sky.MoonDiskGain`, `lyud.Sky.NightLight`,
  `lyud.Sky.SunSpecular`, `lyud.Sky.CloudDensity` (multipliers), `lyud.Sky.NightMie` (the night's Mie share),
  `lyud.Sea.Capillary` (the ripples' slope).
- `unreal/scripts/shoot_cosmic.py` (offscreen): the sun at noon, in the afternoon, at sunset and dusk (wide and
  close), Europa by day and night, full- and new-moon nights over the town and the beach, the tide states
  ([tides.md](tides.md)); `COSMIC_GAINS=0.5,2` adds brightness sweeps, `COSMIC_DIAG=only` the glow diagnostics,
  `COSMIC_TUNE=only` the sun-path ripple sweep.
- Gotcha: an `ALyudSky` loaded with its level gets no `OnConstruction` in a game; its dome instance is made in
  `BeginPlay` (else the sky material shows its defaults).
