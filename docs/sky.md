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
  edge) and reddening. Granulation at its true scale: a granule is ~1/1000 of the diameter (1000-1500 km on the
  Sun), so ~1000 across the disk, a couple of pixels each even full-screen: a fine, barely resolved, slowly boiling
  mottling (soft cells and thin lanes on a warped grid, so no polygon outlines), faded to its mean where a pixel covers
  several; faint meso- and supergranulation patches (contrast `SunGranulation`). Spots are small and subtle: one to
  three groups a day, each a small spot with a slight soft penumbra and a few pores trailing it, only a little darker
  than the photosphere (`SunSpotUmbra` 0.7), ragged and a little elongated. The groups are put on the face the star
  turns to us at the day's noon (`SunSpots.x`), drifting as it turns once in `SunRotationDays`. Faint faculae
  (`SunFaculae`) round the groups and toward the limb; a soft orange corona and, on some days, prominences on the
  limb. The disk is oriented by the celestial pole. Over the disk most of the atmosphere's own glow is taken off
  (`DISK_MASK_HLSL`): added to the capped disk it paled it to pink.
- **Europa** (`MOON_HLSL`): our own procedural Europa, baked once into two committed maps by
  `tools/sky/europa_bake.py` (seed and every parameter in `tools/sky/europa_bake.json`; same file, same maps):
  `textures/sky/europa_albedo.png` (sRGB) and `europa_relief.png` (relief normal east/north, height), 2048 x 1024
  equirectangular. On it, oldest first: ridged plains (cream ice, darker brownish mottling, blue-white young ice, a fine
  grain of tiny ridges); lineae along arcs of great and small circles, most double ridges (two crests, a trough between),
  some triple bands (a bright central ridge in red-brown margins), some smooth grey striated bands, their margins
  red-brown to ochre; chaos terrain (tilted ice plates keeping the old surface, in a lowered hummocky red-brown matrix);
  a few craters (bowl, rim, central peak, bright ejecta; one rayed). The shader lights the sphere from the true sun
  (Lommel-Seeliger: the flat bright full disk of an icy moon), the relief normals shade ridges, bands and plates, and
  a short march toward the sun over the height map casts their shadows, long near the terminator; a faint ashen light
  on the dark side. 285 E (the bright side) faces the coast. Brighter by night (`MoonNightBoost`) and by day
  (`MoonDayBoost`), so it reads as a cream-white icy disk. The NASA mosaic (`europa_2k.jpg`) is kept only as a colour
  reference.
- Both dim and redden toward the horizon (an analytic extinction, `TRANS_HLSL`); stars fade near both disks.

**Brightness is capped**, not physical: the disk centre is `SunIlluminance x SunDiskGain` (the spots read at the
outdoor exposure), Europa `SunIlluminance x MoonDiskGain`. To match, the sunlight's specular is cut
(`SunSpecularScale`, else the sea mirrors a white blob where the star sets), the atmosphere's forward haze is softer
(Mie 0.0045, anisotropy 0.62: the aureole round a 10-degree star would drown the disk), and outdoor bloom is low
(`OutdoorBloom`).

**The clouds hide the sun.** The volumetric clouds are composited over the dome, so they occlude the disk by their
transmittance; the engine's simple cloud material is thin, so `CloudDensityScale` (4) thickens it (its
`Cloud_GlobalDensity`, through a dynamic instance), and the low red sun goes behind the clouds instead of glowing
through them like a lamp in fog. Thick clouds need more ray-march samples or they go grainy: `CloudViewSamples` and
`CloudShadowSamples` (1, the engine's default; it was 0.5).

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
  `lyud.Sky.SunSpecular`, `lyud.Sky.CloudDensity`, `lyud.Sky.CloudSamples` (multipliers), `lyud.Sky.NightMie` (the night's Mie share),
  `lyud.Sea.Capillary` (the ripples' slope).
- `unreal/scripts/shoot_cosmic.py` (offscreen): the sun at noon, in the afternoon, at sunset and dusk (wide and
  close), Europa by day and night, full- and new-moon nights over the town and the beach, the tide states
  ([tides.md](tides.md)); `COSMIC_GAINS=0.5,2` adds brightness sweeps, `COSMIC_DIAG=only` the glow diagnostics,
  `COSMIC_TUNE=only` the cloud-sample and Europa-brightness sweeps, `COSMIC_BANK=1` the high water by the railway
  bank with and without seabed life, dry and without the sea.
- Europa: `python3 tools/sky/europa_bake.py --preview <png>` rebakes the maps (~20 s) and draws a lit preview at
  three phases; commit both PNGs with the .json.
- Gotcha: an `ALyudSky` loaded with its level gets no `OnConstruction` in a game; its dome instance is made in
  `BeginPlay` (else the sky material shows its defaults).
