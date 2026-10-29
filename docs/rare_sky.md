# The rare sky

Aurora, halos and sun dogs, rainbows and moonbows, noctilucent clouds, lightning and the flares of our red dwarf.

## Who decides what

- **The weather engine** (`ULyudWeatherSubsystem`, `WeatherModel`) says how likely each one is: the geomagnetic Kp
  and the aurora's strength at our latitude, the chances of a halo (thin cirrostratus), a rainbow (showers with sun),
  noctilucent clouds (June and July), thunder (strikes come through its `OnLightning`).
- **RareSkyModel** (`Private/Sky/RareSkyModel.{h,cpp}`, plain C++) turns those chances into what is in the sky this
  minute. Like the weather it is a pure function of the local hour, so saves, clock jumps and replays agree.
- **ALyudRareSky** (`Public/Sky/LyudRareSky.h`) draws it. `ULyudRareSkySubsystem` spawns one in every game world that
  has an `ALyudSky`.

## The aurora

Our site sits at ~52 degrees corrected geomagnetic latitude, and the geomagnetic pole lies at bearing 344 (NNW). The
oval's equatorward edge is at `66 - 2 Kp` degrees at magnetic midnight (local ~23:40), farther from us toward dusk and
dawn. So at Kp 5 the main arc stands ~500 km off: a green arc 10 degrees up in the NNW with red above it. At Kp 7 it
reaches the zenith, and at Kp 9 it is south of us, with a corona overhead.

A night is a string of substorms (up to six candidates, more of them the higher the Kp):

| phase    | minutes  | what you see |
|----------|----------|--------------|
| growth   | -70..0   | a quiet arc brightens a little and creeps toward the equator |
| breakup  | 0..12    | the arc flares within a minute or two, splits into rays and folds, races about and spreads toward the pole; a pink lower border |
| recovery | 12..~120 | it fades to diffuse glow and patches that pulse every 5 to 15 s |

Drawing (`tools/sky/rare_hlsl.py` AURORA_HLSL, on the star dome, so every cloud hides it): the Earth is curved and
the field lines dip 69 degrees. Each arc is a thin glowing sheet whose foot runs magnetic east-west. Slow big waves
and quick curls fold it, and the folds are what make the bright edges where we see a sheet edge-on. The rays are
field-aligned columns that drift along the arc and flicker. The emission follows height: green at 100 to 150 km, the
red 630 nm line above 150 km, and pink at the very bottom in a vigorous display. Behind the sheets lies a diffuse band.

## Optics under a red dwarf

The light is a ~3200 K star 10 degrees across (Europa by night is 7). Every bow and halo is the point-source pattern
(Descartes' rainbow angles, minimum deviation through ice prisms, per wavelength) convolved over the limb-darkened
disk and summed over the star's spectrum. The colours spread over 2 degrees, but the disk blurs them over 10, and the
star has little blue to split off. So the bows are broad, pale and warm: a gold-white band with a reddish rim and a
bright inside. Halos are wide soft rings with a red inner edge. Sun dogs are big glowing patches, and the pillar is
as wide as the disk. `ALyudRareSky.OpticsDiskScale` below 1 sharpens them toward Earth's look (1 = physical).

They are drawn on a sphere ~3 km round the camera (`M_LyudSkyOptics`, additive, fogged by the clouds in front of it).

## Noctilucent clouds

They form 83 km up and shine while the sun is 6 to 16 degrees down. The Earth's shadow, plus ~15 km of ozone
screening, cuts them off. On Earth they look electric blue, but here the dwarf's light makes them pale gold-pearl.

## Lightning

`OnLightning` carries a strike. 40% reach the ground (more of the near ones), and the rest flash inside the cloud.
A ground strike has a faint stepped leader, then 1 to 4 return strokes 40 to 120 ms apart, each decaying in ~45 ms.
The bolt is a Brownian bridge from the cloud base to the ground point, with branches. It is worked out per pixel
(`M_LyudLightning`) on a sphere just short of the strike, so nearer land hides it. A point light (outdoor channel)
flashes the land and the sea, and the dome glows toward the strike between the clouds.

## Flares

Red dwarfs are flare stars. A small flare comes every ~4 days and a big one ~6 times a year (`Flares()`, slots of 6 h).
A flare draws two white-hot ribbons and a kernel on the star's face. The ribbons spread apart as it fades. While it
lasts, the daylight is up to 25% brighter and whiter (`ALyudSky.FlareBoost`).

## Console

- `lyud.Rare.Kp`, `lyud.Rare.Aurora`, `lyud.Rare.Halo`, `lyud.Rare.Rainbow` and `lyud.Rare.Noctilucent` force a
  value. A negative value means the weather's.
- `lyud.Rare.AuroraOnset <min>` stages a substorm whose breakup was that many minutes ago.
- `lyud.Rare.Flare <0..1>` shows a flare at that strength.
- `lyud.Rare.Strike [m] [1 ground / 0 cloud] [yaw from view]` makes a strike now.
- `lyud.Rare.StrikeHold <s>` freezes strikes at an age, for screenshots.
- `lyud.Rare.Debug 1` prints the state on screen.

## Tools and tests

- `tools/sky/rare/run.sh` prints an aurora night at Kp 3 to 9, a year of flares, and halo and rainbow shares, and
  runs the model's checks (`run.sh test`).
- `tools/sky/rare/rebuild_sky.sh` rebuilds the sky materials headless (`import_sky.py`).
- `tools/sky/rare/shoot.sh <tag>` takes offscreen screenshots (`unreal/scripts/shoot_rare.py`) into
  `Saved/RareSky/<tag>/`. It waits while two heavy UE runs are already going (`ue_slot.sh`).
