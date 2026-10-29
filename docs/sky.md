# The sky: a red dwarf, Europa, moonlit nights

`ALyudSky` (`Source/.../Sky/LyudSky.*`) puts the sun and the moon where they really stand over the coast at the
calendar's date and hour (`Ephemeris`), with a physical atmosphere, volumetric clouds, a real-time sky light and a
star dome. The world is a simulation, so its sky is not Earth's:

- **The sun is a huge red dwarf**, `SunAngularDiameterDeg` = 10 degrees across (the Sun from Earth: 0.53), a
  ~3200 K star. NPCs are drawn to it (docs/story/motifs.md).
- **The moon is Europa**, `MoonSizeDeg` = 7 degrees (the one value for its size; `lyud.Sky.MoonSizeDeg` overrides it in play), its phases from the real ephemeris.
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
  `textures/sky/europa_albedo.png` (sRGB) and `europa_relief.png` (relief normal east/north, height), 4096 x 2048
  equirectangular. No craters, no stamped plates: the plains are ridged multifractal noise (hummocky, ridged, from
  swells down to a pixel) with broad low regions, cut by a fracture network grown in a tidal stress field. Each of 8
  epochs turns the field 23 degrees in longitude (non-synchronous rotation), so each generation runs another way and
  crosscuts the older ones; ~38,000 cracks at five scales (global lineae, cycloidal chains of arcs with cusps, regional
  ridges, small and fine cracks that make the plains' grain), traced step by step along the field, oldest first, a
  younger ridge flattening what was under it. Each is a double ridge (two crests, a central trough), worn lower and
  softer the older it is; some global ones are triple bands with wide dark red-brown margins, a few grey striated bands.
  The colour comes from height and age: troughs and low ground red-brown, crests bright white, old terrain mottled
  brown, young ice bluish white. ~3 min to bake.
  The shader lights the sphere from the true sun
  (Lommel-Seeliger: the flat bright full disk of an icy moon), the relief normals shade ridges and bands, and
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
transmittance; the engine's simple cloud material is thin, so `CloudDensityScale` (24) thickens it (its
`Cloud_GlobalDensity`, through a dynamic instance), and the low red sun goes behind the clouds instead of glowing
through them like a lamp in fog. Thick clouds need more ray-march samples or they go grainy: `CloudViewSamples` 2 and
`CloudShadowSamples` 1 (the engine's default is 1; it was 0.5). At density 8 one view sample speckled the clouds'
fringes: the noise in their transmittance, with the opaque cut-off and without it alike (`COSMIC_EDGE=1` in
shoot_cosmic.py sweeps samples, cut-off and density over the setting sun). A soft cloud's core still let ~30-50% through, enough to show the disk's limb
and corona as a ring through it: clouds letting through less than `CloudOpaqueBelow` (0.25) are drawn opaque (the cloud
component's `StopTracingTransmittanceThreshold`, 0.005 by default; `lyud.Sky.CloudOpaque` multiplies it, 0 = the
engine's). In UE 5.8 it is a component property, not a cvar. Thin cloud still veils the disk.
v6: the cut-off is 0.03, not 0.25: the engine writes the transmittance as 0 past it and stops adding the cloud's
deeper light, so at 0.25 every cloud core was a darker, hard-edged patch, and over Europa by day that patch read as the
moon's dark side drawn on top of the cloud. The density went 8 -> 24 instead, so a cloud in front of Europa or the sun
hides the whole disk (lit and dark side) and only its fringes let some through (`COSMIC_OPAQUE=cut:density,...` in
shoot_cosmic.py compares them with the clouds frozen by slomo; `COSMIC_OPAQUE_MOON=34` draws Europa big enough to
sit behind a cloud).

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
the disk's (soft shadows). Europa's light: `FullMoonIlluminance` 1.1 scene lux at full, soft shadows
(source angle 7 degrees), shadows only when the sun is down (one shadowed directional light at a time).

**Europa's phases drive the night.** `ALyudSky::UpdateSky` takes the phase angle from the very sun and moon
directions it hands the sky material (`SunDir`, `MoonDir`), so the light's phase is the disk's. The disk is shaded
Lommel-Seeliger per pixel; on top of it a phase law, the same number for the disk (`MoonLook.z`) and the light: an
opposition surge `(1 + B0 / (1 + tan(a/2) / h)) / (1 + B0)` (`MoonOppositionSurge` 0.4, `MoonOppositionWidth` 0.06:
the porous ice's shadow hiding makes the full moon flare) and rough-surface shadowing `exp(-k a)` (`MoonRoughness`
0.55). The light = `FullMoonIlluminance` x the disk's brightness integrated (the Lommel-Seeliger sphere's phase
function x the law) x the air's transmittance toward Europa (sky_hlsl's TRANS_HLSL, per channel, so a rising or
setting moon dims and yellows) x a fade over -2..3 degrees of altitude. Relative to full at the zenith: gibbous
(45 degrees) 0.36, quarter 0.12, crescent (135) 0.02. Its colour `MoonColor` is sea-wave aquamarine (0.55, 0.85, 0.8),
the night's own colour here, and the sky light is tinted toward `NightAmbientTint` by night, so the moonlit ambient
is the same teal; the airglow `NightSkyGlow` is a dark blue-teal. By night the moon, not the sun, casts the clouds' shadows: a cloud over Europa dims the moonlight
under it, and the disk behind cloud is dimmed by the cloud's transmittance as the sun's is. The sky light captures
the sky in real time, so the night ambient (moonlit atmosphere and clouds, airglow) follows by itself. The exposure
opens a quarter of a stop more per stop of moonlight lost, up to `MoonlessExposureEV` (1) over `NightExposureEV`:
full stays a bright moonlit night, a quarter is clearly dimmer, a new moon is dark blue airglow. The LYUDSKY report
prints the phase angle, the law and the light (over full at the zenith) beside the ephemeris' own phase.
`COSMIC_PHASE=1` in shoot_cosmic.py shoots the town path, the beach and the disk at new, crescent, quarter,
gibbous and full.

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
- Europa: `python3 tools/sky/europa_bake.py --preview <png>` rebakes the maps (~3 min) and draws a lit preview at
  three phases (`--detail <png>` a lit 1:1 crop); commit both PNGs with the .json.
- Gotcha: an `ALyudSky` loaded with its level gets no `OnConstruction` in a game; its dome instance is made in
  `BeginPlay` (else the sky material shows its defaults).
