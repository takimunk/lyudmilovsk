# The sun: the light source, and a living star

Our sun is a red dwarf ~10 degrees across (docs/sky.md). This page is about two things: that it reads as **the**
light of the world at every altitude, the setting sun most of all, and that it is **alive**: active regions come and
go, prominences stand on the limb, sway, erupt and fade, and the star flares.

## The light source

A real sun is ~10^5 times brighter than the sky round it and blinding even on the horizon; we draw it capped (a
physical one would be a white hole) for the adapted eye; the fresh eye sees it far brighter (see *Glare* below). The cap was too low: at noon the disk (`SunDiskGain` 0.14) was dimmer than the
clouds it lit, and at sunset the atmosphere left a dim red coin in a pale sky over a flat grey land. Now:

- **The disk is brighter than anything it lights**: `SunDiskGain` 0.45 (a white wall in full sun is about
  `SunIlluminance / pi`). It sits on the tonemapper's shoulder, which flattens detail, so the disk's own detail
  (granulation, spots, filaments) is steepened by `SunDetailContrast` (3.2) before it is drawn: the spots and the
  granulation still read at noon. The bloom (`OutdoorBloom`) gives it a soft glare.
- **The sun's reflections match it**: `SunSpecularScale` 0.03 (was 0.012 for the dimmer disk).
- **It has depth, not a flat plate.** Limb darkening of a ~3000 K star (a quadratic law, ~0.1 of the centre at the
  edge) raised to `SunLimbDarkening` (2) for the screen, and redder toward the rim; granulation at its true scale
  (sub-pixel, faded to its mean), the mesogranulation (cells ~110 across the star) that shows up close, a faint
  supergranular network (darker lanes at the centre, bright faculae along it toward the limb), faculae round the spot
  groups (`SunFaculae` 0.6), spots with a dark umbra (`SunSpotUmbra` 0.5) in a lighter filamentary penumbra. Only 15%
  of the sky's own glow stays over the disk (more paled the orange to peach).

And as the star sinks (`ALyudSky::LowSun()`, 0 above 25 degrees to 1 at 2 degrees):

| what | where | how |
|---|---|---|
| the disk keeps its brightness | `SUN_HLSL` (`SunLook.y`), `SunDisk.y` | the air's colour pixel by pixel, but only `SunHorizonDimming` (0.4) of its dimming, and `1 + LowSunDiskGain` (1.3x) brighter at the horizon: still the brightest thing in the sky, deep orange (1 and 0 = physical). Brighter than that it blew out to pale yellow on the tonemapper's shoulder |
| behind the haze | `FOG_DISK_HLSL` (`SkyFogA..E`), `SunHazeDimming`, `SunVeilClear` | the weather's haze and fog lie over the dome like over the land (see *Behind the air* below); the star keeps 0.45 of the haze's real dimming toward it and only a quarter of its flat glow where it outshines it |
| the haze round the disk | `DISK_MASK_HLSL` (`SunLook2.y`) | `SunRimClear` (0.5) of the sky's own glow is taken off just outside a low disk, fading over a radius: the forward glow round a capped disk this big blurred its limb |
| the aureole | `SUN_HLSL` (`SunLook.x`) | a warm forward-scattering glow round the disk, three lobes (a few, ~10 and ~30 degrees wide), stronger the more air the light crosses; `SunAureole`, at full strength only round sunset (`SunsetGlow()`), a third of it higher up |
| the haze glows | `USkyAtmosphereComponent` | Mie up by `LowSunMieGain` (0.25) and its anisotropy from 0.62 to `LowSunMieAnisotropy` (0.72): a brighter, narrower forward lobe in the sky and over the far sea and land; only round sunset (`SunsetGlow()`: 1 from about -3 to 1 degrees, nothing above 10 or below -7), since the late-autumn sun never climbs over 25 degrees and `LowSun` alone kept a warm glow over the horizon all day |
| the light | sun `UDirectionalLightComponent` | `1 + LowSunLightGain` (2.5x) the atmosphere's light at the horizon; the atmosphere still reddens it. The atmosphere's sky and haze (aerial perspective) follow the light, so they are scaled back by the same factor (`SkyAndAerialPerspectiveLuminanceFactor`; the material's `SkyLumScale` stays 1): a brighter light alone paled the whole sky and veiled the disk. The clouds get `LowSunCloudLight` (0.3) of the extra light (`CloudScatteredLuminanceScale`): with all of it they shone as bright as the disk where they cross it |
| the shadows | the sky light | `LowSunAmbient` (0.7) of its intensity: deeper shadows against warm lit faces; they are long by geometry |
| the sun path | the light's specular | `SunSpecularScale x (1 + LowSunSpecularGain)` (5x at the horizon): the sea's capillary ripples break the mirror image into a glittering path |
| god rays | the light's screen-space light shafts | the bright disk blurred radially over trees, buildings and clouds in front of it, `SunShaftBloom` (0.15) when low (a fifth high up), from scene colour over `SunShaftThreshold` (1); stronger shafts veil the big disk itself |

### Behind the air (round 3)

Dima's playtest of round 2 found a warm band over the horizon at every hour, cutting through fog, and a seam across
the sky with a semi-transparent layer starting at it; and the star looked pasted on the sky, with no air in front.

- **The band and the seam were one thing: the fog stopped short of the dome.** Round 2 cut the weather's height fog
  off at 150 km (`FogCutoffDistance`) so it would not veil the setting disk; the dome is 200 km out. In haze, fog or
  under an overcast the land, the sea (60 km) and the cloud deck sank into the fog, but the dome over the horizon did
  not: a hard line on the sea horizon, the bare atmosphere's bright yellow-orange horizon above it (its Mie glow, the
  red dwarf's light), and the fogged cloud deck ending as a semi-transparent sheet at the clouds' tracing distance
  (60 km) with the bare sky under it. The fog now lies over the dome too (`ALyudSky` keeps the cutoff at 0;
  `lyud.Sky.DomeFog 0` brings the old cutoff back for comparison): the horizon sinks into the haze like the land.
- **The star is drawn behind that fog** (`tools/sky/sky_hlsl.py FOG_DISK_HLSL`). Over the star the engine's fog is a
  flat veil (opaque near the horizon, the dome being 200 km out), which is what paled the setting disk before. The sky
  material works out the engine's own fog over each pixel (the formula of `HeightFogCommon.ush`, from the live
  component: `ALyudSky::UpdateSkyFog` sets `SkyFogA..E` every frame: both layers' density at the eye and falloff, the
  inscatter, the glow toward the sun with the atmosphere's sunlight, the sky's ambient share) and draws the star for
  it: what that fog takes off is given back, and the haze's real dimming is put on instead, along the pixel's own
  direction and round the earth's curve (an exponential layer of scale height H at elevation e has an air mass of
  ~1 / (sin e + sqrt(2H / pi R)); flat, it went on for ever). The disk keeps `SunHazeDimming` (0.45) of it, as a log
  share like `SunHorizonDimming`. Over the disk, where the star's light outshines the haze in front of it, up to
  `SunVeilClear` (0.75) of the haze's flat glow is taken off (a real sun is ~10^5 times brighter than its haze; the
  capped one is not): a = K (1 - exp(-x / K)) of it, x the star's light over the haze's there, K = `SunVeilClear`. It
  grows smoothly with x and never takes off more than the light itself, so the darkened limb keeps a little more veil
  with no step (round 3's on/off smoothstep(0.8, 2) of x left a pale ring inside the limb in mist) and nothing ends
  darker than the haze round it (no brown ring). Off the disk nothing is taken off.
  Off the disk (prominences, corona, a flare's glow off the limb, the aureole: a real star's are hundreds of times
  fainter than its disk) the light fades with how far the disk's centre outshines the haze's glow, Vc (the centre as
  `SUN_HLSL` draws it, through the same haze): `smoothstep(2, 9, Vc)` (`sky_hlsl.GLOW_HAZE`). Measured next to the limb
  Vc is ~10 on a clear noon (untouched), ~7 in a light haze (a little softer), ~3 in a mist (nearly gone), < 2 low in
  a hazy sunset. Dimmed only like the disk, they stood out crisp and pink over a misty disk ("pasted on top").
  Where the haze outshines the star (fog) the disk shows as a pale coin or not at all. The emission there may go below
  zero (`allow_negative_emissive_color`): the fog pass adds the glow back. The sky light's reflection capture takes
  the plain star (`ReflectionCapturePassSwitch`).
  So the star reddens and dims toward the horizon (the air, `TRANS_HLSL`) and with the weather's haze, sinks into it
  at the horizon, fades to a pale disk in mist and is gone in fog, and keeps some of the haze's glow on top: less
  contrast in haze. On a clear day high up the haze is thin and it is drawn as before.
  The haze's dimming is a little chromatic (Angstrom exponent ~1.3: 0.8, 1, 1.3 of it in r, g, b), so the setting
  disk's lower edge reddens as it sinks into the haze.
- **Thin cloud in front softens it.** A cirrostratus or cirrus veil (the clouds' `HighA`/`HighB`) scatters part of the
  disk's light into a soft glow just round it and flattens its detail (`SUN_HLSL`, `veil`); the veil itself dims it
  (`HIGH_HLSL`). The volumetric decks dim it by their own transmittance and light their edges by their phase.
- **The haze under a cloud deck is lit by the deck, not the blue sky**: the atmosphere's share of the fog's light
  (`HeightFogContribution`) falls to `HazeSkyUnderDeck` (0.2) under a closed deck (the deck's opacity, as for the
  exposure). With all of it the horizon below an overcast shone as a white band under the deck's far edge.
- **The warm low-sun sky only round sunset.** The haze's stronger, narrower forward glow (Mie gain and anisotropy) and
  the aureole's wide lobes follow `SunsetGlow()` (-7..10 degrees), not `LowSun()` (25 degrees down): the late-October
  sun stays under 25 degrees all day. The light itself, its shadows and the sea's glitter path still follow `LowSun`.

Still open (not in LyudSky): under a full overcast a thin bright strip stays just over the sea horizon, below the
deck's far edge (the volumetric deck fades out at its 60 km tracing distance and the haze beyond is lit brighter than
the deck's underside: `ALyudRainFx::SkyLuminance`); with the `as` cloud preset (altostratus) the sun does not show at
all (the deck's transmittance along the sun is ~0).

The exposure already opens a little for a low sun (`lyud.Sky.LowSunEV`). Volumetric fog stays off (its cost); the
shafts are the screen-space kind, so they show while the sun is on the screen or near it.

## Glare: blinding until the eye adapts

Dima (2026-11-01): wherever we are, on the coast or on Europa, the sun must be big and bright, blinding, and leave
afterimages; staring at it the hero blinks and says "ай", and if he holds on the eye adapts and the star shows as it is
drawn; a couple of seconds away and it blinds again. `ULyudSunGlare` (`Public/Sky/LyudSunGlare.h`, a world subsystem in
game worlds) does this for whatever the camera is (the hero, a swimmer, the bird), with the same `ALyudSky` star on Earth
and on Europa (`SetSpaceView`).

- **How much of the star reaches the eye**, every frame: where the disk is on the screen and how far from the middle of
  the view; how much of it the land, the trees, the buildings and the club hide (5 line traces over the disk, 10 times a
  second, only while the star is near the view; `lyud.Sun.Glare.TraceKm` 30); the air (the same transmittance as the sky
  material), the haze and fog along the sun (`ALyudSky::GetSunHazeThrough`, the height fog as `FOG_DISK_HLSL` sees it)
  and the overcast (`GetOvercastSunScale`). Below the horizon or behind a hill it is nothing.
- **The unadapted star** (`ALyudSky::SetGlare`): the photosphere is drawn `lyud.Sun.Glare.Gain` (14) times brighter, far
  over white (the sky material's `SunGlare.x`, `SUN_HLSL`; the off-limb light, prominences and corona keep their own
  brightness, and the sky light's capture takes the adapted star: the glare is in the eye, it lights nothing), the
  outdoor bloom up to `lyud.Sun.Glare.Bloom` (2.5) times while it is in sight, and a glow in the eye round it over
  everything in front plus a faint veil over the view (the overlay; `lyud.Sun.Glare.Halo`). Low down the glare gain fades
  out (`lyud.Sun.Glare.HorizonGain` 0 of it on the horizon, all of it from 20 degrees up) and the photosphere itself is
  drawn at `lyud.Sun.Glare.HorizonDisk` (0.3) of the sky's brightness on the horizon (all of it from 15 degrees up),
  fresh or adapted: brighter, the setting disk blew out to pale yellow on the tonemapper even at physical dimming
  (tuning sweep 2026-11-01); so the setting star stays orange, pink, red and can be looked at. A material built before the glare gets the gain on the whole star
  with its off-limb light scaled back (rebuild with `import_sky.py`).
- **Staring** (the star within ~14 degrees of the middle of the view): the hero squints (the eyelids come in), and after
  `lyud.Sun.Glare.StareSeconds` (3) of it blinks, his head flinches and he says "ай" (`line.hero.sun_ow.1..3`, the hero's
  designed voice, not subtitled, at most once in `lyud.Sun.Glare.OwEvery` 20 s). He keeps blinking while it still hurts.
  Holding on, the eye adapts over `lyud.Sun.Glare.AdaptSeconds` (6): the gain, the bloom, the glow and the squint fade
  and the star shows as drawn, granulation, spots, prominences. Looking elsewhere the adaptation goes with a time
  constant so that after `lyud.Sun.Glare.RecoverSeconds` (2) a tenth is left: it blinds again. Shut eyes facing the sun
  see red through the lids.
- **Afterimages** are fixed to the retina, i.e. to the screen (the eye turns with the view): a 192x108 map of the retina
  takes the disk's light where it falls (192x108). A fast positive afterimage follows the light (up in a quarter second, down in
  under one): turning away, a bright blot in the star's colour stays a moment where it was, smeared along a pan. The
  slow one is the bleach: it grows over several seconds of the star on the same spot and comes back over 2-6 s (longer
  after a long look); it shows as a dark blot in the complementary colours, violet in the middle, green-teal round it,
  with a slight drift. Nothing is drawn where the star shines now.
- **Drawn** as a Slate overlay under the HUD (z 50: under the swimmer's meter, prompts, game screen and menu; nothing over
  the menu or the game screen; the eye waits while they are open): the retina map as a 192x108 texture uploaded at most 30
  times a second while an afterimage lasts (four buffers in turn, no allocation per frame), the eyelids and the glow as
  textures made once. No material, no render target.
- **Off**: the Settings page ("Sun glare (flashes, blinking)", `[Lyudmilovsk.Settings] SunGlare`) or `lyud.Sun.Glare 0`:
  the star as drawn (the adapted look), no flashes, blinks or afterimages.

- **Blinded by a sudden light** (API for other systems, e.g. climbing out of the hatch onto Europa the first time):
  `ULyudSunGlare::BeginFlashBlind(float Strength = 1)` (`Public/Sky/LyudSunGlare.h`) washes the view toward white,
  makes the eye fresh, the hero squints, says "ай" and blinks about once a second; each blink keeps
  `lyud.Sun.Glare.FlashStep` (0.5) of the whiteout, so 3-5 blinks bring the view back, and the sun's own glare goes on
  as usual. `ULyudSunGlare::FlashBlindOnce(FName Key, float Strength = 1)` does it only the first time for `Key`
  (remembered in the save: `ULyudSunGlareMemory`, record "SunGlare"); true if it fired. Get the subsystem with
  `ULyudSunGlare::Get(WorldContext)`. `lyud.Sun.Glare.FlashBlind [strength]` tries it from the console.

| variable | default | what |
|---|---|---|
| `lyud.Sun.Glare` | 1 | the whole glare (0 = off; the Settings page stores it) |
| `lyud.Sun.Glare.Gain` / `.HorizonGain` / `.HorizonDisk` | 14 / 0 / 0.3 | the photosphere's gain for the fresh eye / share of it on the horizon / the disk's brightness on the horizon |
| `lyud.Sun.Glare.Bloom` / `.Halo` / `.Afterimage` | 2.5 / 1 / 1 | the bloom while it is in sight, the glow and veil in the eye, the afterimages |
| `lyud.Sun.Glare.Blink` / `.Voice` | 1 / 1 | squint and blinks, the "ай" |
| `lyud.Sun.Glare.StareSeconds` / `.AdaptSeconds` / `.RecoverSeconds` | 3 / 6 / 2 | to the "ай", to adapted, to blinding again |
| `lyud.Sun.Glare.OwEvery` | 20 | at most one "ай" in this many seconds |
| `lyud.Sun.Glare.Adapt` | -1 | holds the adaptation (tests) |
| `lyud.Sun.Glare.Freeze` / `.FreezeAtShut` | 0 / 0 | 1: the eye and the retina stop (screenshots) / that is set when a blink shuts the eyes |
| `lyud.Sun.Glare.Reset` | | a fresh eye |
| `lyud.Sun.Glare.FlashBlind [s]` / `.FlashStep` | 1 / 0.5 | blinded by a sudden light / the whiteout kept per blink |
| `lyud.Sun.Glare.Debug` | 0 | the `GLARE` line on screen |

Test: `unreal/scripts/shoot_glare.py` (offscreen, "shot showui": the overlay is UI): the noon star wide and stared at,
squint, blink, adapting, adapted, the afterimages 0.6, 2.4, 5.4 and 8 s after looking away, back at once and after 3 s
away, the glare off, the sunsets fresh and adapted, the terrace, Europa. `GLARE_FILM=1` takes a frame a tick through
one stare at slomo 0.2 for a video.

## A living star

`ULyudSunActivity` (`Public/Sky/LyudSunActivity.h`, a world subsystem) wraps a plain C++ model
(`Private/Sky/SunActivityModel.{h,cpp}`) that is a **pure function of the shown time**: a save, a clock jump or a
second machine see the same star. `ALyudSky` asks it every tick and it sets the sky material's parameters.

### What real red dwarfs do (and what we took)

- **Starspots and active regions.** Like the Sun, M dwarfs have magnetic active regions with spots and faculae, but
  fully convective ones keep strong fields and spots at high latitudes and near the poles too (Doppler imaging; Morin
  et al. 2008, *MNRAS* 390, 567, magnetic topologies of fully convective stars). Spot groups live from days to
  months. *We:* regions born ~1.6 a game day, two belts (10-45 degrees) and 30% at 50-75 degrees, living 2-10 game
  days: they emerge in a tenth of their life and decay slowly. Each has a leader spot, followers and pores trailing it
  in longitude (more and bigger the stronger it is), bright plage and faculae (strongest toward the limb), and lifts
  the corona above it into a streamer when near the limb. Spots stay small and subtle (Dima's choice, umbra 0.7).
- **Rotation.** The regions ride the star's spin (`SunRotationDays`, 6 game days at the equator). Fully convective
  M dwarfs turn almost as solid bodies: their differential rotation is a few percent of the Sun's at most (Barnes et
  al. 2005, *MNRAS* 357, L1; Reiners 2006). *We:* `Omega(lat) = Omega_eq (1 - 0.06 sin^2 lat)`: high-latitude regions
  slowly fall behind.
- **Prominences.** Cool dense plasma held in the corona by the field, glowing red in H-alpha against the sky at the
  limb and dark (filaments) in front of the disk. Quiescent ones lie along the neutral lines between old field,
  often in a crown at high latitudes, as tall thin hedgerows of vertical threads, and live days to months; active
  region prominences and loop systems are low, bright and change in minutes to hours; surges shoot up and fall back in
  tens of minutes (Mackay et al. 2010, *Space Sci. Rev.* 151, 333; Parenti 2014, *Living Rev. Sol. Phys.* 11, 1). A
  prominence may erupt: it rises slowly, then accelerates to hundreds of km/s and is gone within an hour or so,
  often the core of a coronal mass ejection (Gopalswamy et al. 2003). Rapidly rotating M dwarfs also hold
  co-rotating "slingshot" prominences several radii out (Collier Cameron & Robinson 1989; Jardine & Collier Cameron
  2019) and CMEs are seen on them (Argiroffi et al. 2019, *Nature Astr.* 3, 742). *We:* three kinds, each anchored at
  a point on the star (so it turns with it, and stands on the limb only while its foot is near the limb):
  - loop systems on active regions: a few hours of star time, low (0.05-0.17 radii) and bright, 20% erupt;
  - surges on active regions: up and back down in 15-50 star minutes;
  - quiescent hedgerows: 1.5-11 star days, middle (20-45 degrees) and high (45-70) latitudes, along the parallels,
    0.08-0.25 radii tall, made of vertical threads under the arch; 12% erupt near the end of their life.
  An erupting one lifts off accelerating (`0.3 t + 3 t^2` radii over its eruption), swells, thins out and fades, with
  a faint front ahead of it.
- **Flares.** M dwarfs are flare stars: small flares many times a day, the energies falling off as a power law
  (Hawley et al. 2014, *ApJ* 797, 121, GJ 1243 with Kepler; Davenport 2014; Howard et al. 2018 on Proxima Centauri's
  superflare). A white-light flare rises within minutes (the impulsive phase) and decays over tens of minutes, two
  ribbons parting on either side of a neutral line in the active region. *We:* each region flares at a rate growing
  with its strength squared (~14 a star day over the whole star), a steep power law of peaks (most ~0.1-0.3, now
  and then a big one), rise 2-5 star minutes, decay 6-36. The strongest flare facing us is drawn (rare_hlsl
  `FLARE_HLSL`, the rare sky's own, `SunFlare`: see Drawing) and brightens the daylight a little
  (`ActivityFlareLight`, +12% at strength 1). The rare sky's big flares (docs/rare_sky.md) stay on top of these.

### Time

The regions live by the **game's** days (they cross the disk as the star turns). Everything that changes within
hours on a real star runs on an **activity clock** `lyud.Sun.TimeScale` (20) times faster than the game clock: a
real hour of a prominence's life passes in three game minutes, so a few real minutes of looking at the limb show
loops grow and fade, a surge rise and fall back, sometimes an eruption; a flare is over in about a minute. On top
of that the shader keeps everything moving by the second: the prominences sway, plasma knots drain down their legs
and the threads flicker.

`tools/sky/sun/run.sh` prints ten real minutes of the limb and a game day of regions and checks the model: mean
~8 regions on the whole star, ~5-6 prominences standing on the limb (never none), an eruption going on ~20% of the
time, a visible flare ~4% of the time, and in ~90% of 5-minute windows a prominence comes or goes at the limb.

### Drawing

`tools/sky/sky_hlsl.py SUN_HLSL` (built into `M_LyudSky` by `import_sky.py`). The model sends the 8 most visible
regions (`SunRegion0..7`: latitude, star longitude, strength, seed) and the 16 most visible prominences
(`SunPromA/B/C0..15`, in the view frame: anchor and height, foot axis and half length, brightness, kind, lift, seed).
Each prominence is an arch over its foot, worked out in 3D on the star and projected: a polyline of 10 segments
(hedgerows also stand a thread under each vertex), its width never under a pixel (energy kept), with strands, flowing
knots and a soft glow. Off the limb it glows red; where it lies in front of the disk it darkens the photosphere as a
filament; behind the disk it is hidden. They glow in H-alpha, a hot pinkish red, brightest low down where the plasma
is densest, as fine threads (strand noise across the arch) with a soft glow round them. The corona gets streamers
over active regions near the limb.

A flare (`rare_hlsl.FLARE_HLSL`) is drawn as on real stars: two bright ribbons on either side of the region's neutral
line (a gentle S through it, the filament channel), warm white-yellow at their cores and yellow-orange at their soft
edges, with a halo hugging each and a glow over the region; soft knots on them in the first minutes; they part as the
flare goes on. Never narrower than ~1.5 pixels (energy kept): no hard lines or aliasing. The model's light curve rises
in 2-5 star minutes and decays over 6-36. When the region is near the limb a post-flare loop arcade (six loops across
the neutral line, projected from the star) stands on the limb and brightens as the flare fades.

## Sunsets

No two evenings are alike: one is a pale golden glow and gone, the next an orange sky, pink clouds and a violet blue
hour, a deep red sun in haze, a dramatic crimson, or only a grey strip under the overcast. Sunrises have their own.
`ULyudSunset` (`Public/Sky/LyudSunset.h`, a world subsystem) wraps a plain C++ model (`Private/Sky/SunsetModel.{h,cpp}`)
that, like the star's activity, is a **pure function of the shown date**: a save, a clock jump or a second machine
see the same sunset, and nothing has to be saved.

### The day's character

`Profile(seed, day, sunrise, month)` draws a kind and its own numbers for every evening and every morning:

| kind | hue | aerosol | what it looks like |
|---|---|---|---|
| golden | 0.04 | low | clean dry air: a short warm glow, pale gold clouds |
| orange | 0.27 | moderate | the everyday one |
| pink | 0.5 | low | clean upper air: pink cloud undersides, a violet blue hour (more ozone), a long afterglow |
| red | 0.74 | high | hazier: a deeper red sun and sky |
| crimson | 0.96 | high | dramatic: the strongest cloud glow, a long afterglow |
| grey | 0.3 | very high | milky, washed out whatever the weather |

The odds lean with the month (hazy summers bring more golden, orange and red evenings, clean winter air more pink and
crimson ones) and mornings are cleaner (more pink and golden dawns, fewer red and grey ones). Every kind varies by day
(hue +-0.08, intensity +-15%, aerosol, ozone, cloud glow, afterglow). About one evening in twenty is **spectacular**:
pink or crimson, the colours and the cloud glow at their strongest, the longest afterglow. The sunrise and the sunset
of a day are drawn separately (the morning's look holds before noon, the evening's after; at noon the look is off, so
the switch never shows).

### The weather makes or spoils it

`AirOf(weather)` reads five continuous shares from the weather state, with the cloud layers as the clouds draw them
(a `lyud.Clouds.Preset` counts):

- **overcast** (the same deck opacity the exposure adapts to): the colours fade to a dull grey (intensity down to a
  fifth, tints toward grey), the clouds' glow nearly gone, a dimmer disk;
- **broken** (high cirrus, altocumulus with gaps, heaped low cloud; none under a closed deck): the clouds catch the low
  light, up to twice the glow: the most vivid pink and red undersides;
- **clean** (wet ground and nothing falling, or a fresh wind): less Mie (to ~0.55x), saturated colours;
- **haze** (humid air, poor visibility, fog, an inversion): more and deeper Mie, the hue pushed toward red, a dimmer
  disk in a bigger, softer aureole, less contrast;
- **rain** falling now: dull.

### What it changes

All of it is weighted by `Band(alt)`: 0 above 11 degrees and below -9, 1 from -5 to 4. Outside the band every value is
exactly neutral and the subsystem does nothing (it puts the atmosphere back once on leaving it). `lyud.Sunset.Intensity`
scales every change (0 = off).

| lever | owner | what |
|---|---|---|
| Mie scattering, anisotropy | `ALyudSky` (x `MieScale`, + `MieGAdd`) | the haze's density: clean 0.55x .. thick 2.25x |
| Mie absorption, the haze's scale height | `ULyudSunset` | a duller, smoky haze on grey evenings; deeper haze reddens the horizon more |
| Rayleigh scale | `ULyudSunset` | a little more for the red kinds: deeper colour near the horizon |
| ozone (other absorption) | `ULyudSunset` | the violet blue hour after a pink sunset (up to 2.6x) |
| sky luminance tint (`SkyLuminanceFactor`) | `ULyudSunset` | the sky's hue (golden .. pink .. crimson), not the haze over the land |
| the sunlight's colour | `ALyudSky` (x `LightTint`) | the land, the sea's glitter and the clouds; luminance-normalised, so only the hue changes (grey: a little dimmer) |
| the clouds' light (`CloudScatteredLuminanceScale`) | `ALyudSky` (x `CloudTint`) | the cloud glow's colour and strength: by how much the sky catches the light (broken cloud), up to 1.6x; after the sun has set the afterglow holds it up for a while (to -9 degrees) |
| the aureole, the disk | `ALyudSky` (`SunLook.x`, `SunDisk.y`) | hazy evenings a bigger aureole and a dimmer disk |
| the exposure | `ALyudSky::UpdateExposure` (+ `ExposureEV`) | held back by 0.4 stops round sunset and 1.5 just after (`Dusk`, 1 below -3 degrees), gone by -9: the night's exposure opens from 8 degrees up (~3 stops at -4) and blew the glowing twilight and the afterglow out to cream whatever their colour |

The tints are deep multipliers (a crimson light is ~(1, 0.42, 0.40) of the atmosphere's own colour), with only part of
their luminance loss given back, so a red evening is a little darker than a golden one: tints normalised to the same
luminance (the first try) changed the bright sunset sky by a few percent on the tonemapper's shoulder. A dull evening
mixes toward a cool grey, which takes the atmosphere's own orange out instead of only fading the tint. After sunset
the clouds keep more of their tint but get dimmer, not brighter. No new shader, no new material parameter, no per-frame allocation: each value is set only when it
changes by more than 0.2%. The disk's own colour follows the day too (`FLook::DiskTint` -> `FLyudSunsetLook::DiskTint` ->
the sky material's `SunDiskTint`, multiplied into the photosphere in `SUN_HLSL`, on top of `TRANS_HLSL`'s fixed air and
the weather's fog): the evening's aerosol and hue redden it (`Redden = Wb (0.9 Aerosol + 0.35 Hue - 0.2)`: green
x(1 - 0.85 Redden), blue x(1 - 0.95 Redden), all x(1 - 0.3 Redden): the red channel is on the tonemapper's shoulder, so
a milder tint did not show), so a hazy evening has a deep red, dim disk; clean washed air makes it a little
more golden (green up to x1.3). The glare's afterimages take the same colour. `lyud.Sunset.DiskTint 0` = the air's
colour alone (before).

### Console

| variable | default | what |
|---|---|---|
| `lyud.Sunset.Profile` | -1 | force the kind: 0 golden, 1 orange, 2 pink, 3 red, 4 crimson, 5 grey, 6 spectacular (-1 = the day's own) |
| `lyud.Sunset.Intensity` | 1 | scales every change (0 = off, for comparison) |
| `lyud.Sunset.Seed` | 0 | another run of evenings |
| `lyud.Sunset.Clean`, `lyud.Sunset.Haze` | -1 | override the air's clean and haze shares 0..1 (tests) |
| `lyud.Sunset.DiskTint` | 1 | how much of the day's colour the disk takes (0 = the air's colour alone) |
| `lyud.Sunset.Debug` | 0 | the profile on screen (`SUNSET` line; also in `ALyudSky`'s debug report while it is on) |

### Tests

- `tools/sky/sunset/run.sh [test]`: a month of evenings, each kind in each weather, and the checks (deterministic,
  the sunrise its own, ~5% spectacular, every kind 4-40% of evenings, neighbouring evenings mostly differ, exactly
  neutral outside the band and at intensity 0, continuous in the sun's altitude and in the weather, overcast dull,
  broken cloud glowing most, haze redder with a dimmer disk, after rain cleaner, tints bounded).
- `unreal/scripts/shoot_sunset.py` offscreen: the same view from the club's cliff toward the sun for every kind, two
  days left to their own character and the look without it, in clean clear air, broken altocumulus, overcast, after
  rain and haze at +2 degrees, in broken cloud at -1 and in clear air and broken cloud at -4; the town across the light and three sunrises.
  `tools/sky/sunset/sheet.py <log> <screenshots> <out.png>` lays them out in one sheet.

Still open: the clouds' own ambient (the sky light and multiple scattering) is not tinted, so cloud edges lit from
behind after sunset stay cream whatever the evening (the sunlight's share of their light is tinted); a tint in
M_LyudClouds would be the clouds thread's.

## Console

| variable | default | what |
|---|---|---|
| `lyud.Sun.Activity` | 1 | births of regions, prominences and flares x this (0 = a quiet star) |
| `lyud.Sun.TimeScale` | 20 | the activity clock over the game clock |
| `lyud.Sun.ForceFlare <peak>` | 0 | a flare starts now at this peak 0..1 on the most visible strong region (set 0 and again for another) |
| `lyud.Sun.ForceFlareLimb 1` | 0 | the forced flare goes on a region near the limb (the loop arcade) |
| `lyud.Sun.Debug 1` | 0 | the activity on screen (`SUNACT` line; also in `ALyudSky`'s debug report) |
| `lyud.Sky.SunAureole` | 1 | multiplies the aureole |
| `lyud.Sky.SunHorizonDimming` | -1 | overrides `SunHorizonDimming` when >= 0 (1 = physical) |
| `lyud.Sky.LowSunLight` | 1 | multiplies the low sun's extra light, specular and haze (0 = the atmosphere alone) |
| `lyud.Sky.SunShafts` | 1 | multiplies the light shafts (0 = off) |
| `lyud.Sky.DomeFog` | 1 | 0: the fog cut off short of the dome and the star drawn without it (round 2's look, for comparison) |
| `lyud.Sky.SunHaze`, `lyud.Sky.SunVeil` | -1 | override `SunHazeDimming`, `SunVeilClear` when set (tuning) |
| `lyud.Sky.LowSunDiskGain`, `lyud.Sky.LowSunCloud`, `lyud.Sky.SunRimClear`, `lyud.Sky.SunLimb`, `lyud.Sky.LowSunMieG` | -1 | override `LowSunDiskGain`, `LowSunCloudLight`, `SunRimClear`, `SunLimbDarkening`, `LowSunMieAnisotropy` when set (tuning) |

## Tests

- `tools/sky/sun/run.sh [test]`: the model outside the engine.
- `unreal/scripts/shoot_sun.py` offscreen (see its docstring): sunsets from the terrace and the beach (and the same
  without the low-sun changes: `_old`), the lit land with the sun behind, the noon disk wide, close and in detail,
  the limb, a forced flare, a busy star, Europa's night for comparison. `SUN_SHOTS=noon_limb SUN_TIMELAPSE=10,60`
  stays on the limb and takes a frame a minute for ten minutes. `sunset_close_low` is the setting disk close from the
  cliff; `flare_late` is a forced flare 30 s on (the ribbons parted), `flare_arcade` one near the limb 40 s on (the
  loop arcade). `SUN_VARY="tag=cmd;cmd|tag2=..."` takes each chosen shot again per variant (tuning sweeps in one
  process).
- `SUN_SKY3=1` (round 3): the horizon at noon toward and away from the sun, with and without fog and clouds, the
  sunsets, fog and overcast at noon toward and away from the sun, the sun through cirrostratus, cirrus, altostratus,
  haze and mist, and the old look (`lyud.Sky.DomeFog 0`) of the fog, the overcast, the sunset and the noon disk.
