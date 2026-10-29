# Weather engine

`Public/Weather/LyudWeather.h` (`ULyudWeatherSubsystem`), the model in plain C++ in
`Private/Weather/WeatherModel.{h,cpp}`; checks and climate tables outside Unreal: `tools/weather/run.sh`.

The engine decides *what the weather is*; clouds, rain, fog, snow, ice, aurora and the rest are drawn by their own
systems, which read its state. It is a function of the game calendar (`UGameCalendar` + `lyud.SkyHourOffset`, the
clock of the sky and the wind), so a save, a jump of the clock or a forecast always agree. Only the ground and the
sea remember: they integrate hour by hour from the 1 September before.

## How it works

1. **The synoptic chain.** From 2020-01-01 a seeded Markov chain plays out episodes of eight weather types (the
   cyclone's life over the Baltic): `High` -> `WarmFront` -> `WarmSector` -> `ColdFront` -> `Showery` -> `High` or the
   next low's warm front; `Occluded` lows, winter `ArcticHigh`s, summer `ThunderyLow`s. The transitions and how
   often each type comes per month are tables at the top of `WeatherModel.cpp`, the lengths are per type (a cold
   front 4-8 h, a high 1-5 days, an arctic high 2-5 days). Each episode has an `Intensity` (how deep the low; the
   stormy months deepen them) and a `Bias` (its own twist: warmer air, a direction swing, a winter high's grey
   inversion deck or not). Seed 750 gives the scene's start (2026-10-20 16:00) showery polar air after a front:
   9 C, broken Cu, a WSW breeze, rain the next day.
2. **Profiles.** Each type describes the weather along its course (`Profile()`): pressure, the air mass's
   temperature anomaly and dew-point spread, the wind of the weather, the wind aloft, three cloud layers (genus,
   cover, base, top, density), steady and shower precipitation, instability, inversion, a warm layer aloft.
   A warm front goes Ci -> Cs (halo) -> As -> Ns with steady rain, the wind backing S and freshening, the pressure
   falling; the cold front is a Cb squall with a burst of rain or snow, gusts, the wind veering NW and the pressure
   jumping up. The end of a high or of showery air already shows the next front's cirrus. Neighbouring episodes
   cross-fade (an hour at a cold front, hours elsewhere); the wind turns the same way on both sides of a boundary.
3. **The air.** Temperature = the season's mean + the air mass + slow noise + the day's swing (bigger in summer,
   damped by thick cloud and wind, colder nights over snow) + the sea's pull when the wind is onshore. The dew point
   stays with the air mass, so clear nights get damp and foggy. The Cu base comes from the dew-point spread
   (125 m per degree), the freezing level from the lapse rate.
4. **Clouds, precipitation, fog** follow from the air: Cu grow with the sun on land and over a sea warmer than the
   air (autumn and winter showers come off the sea at night too), towers become Cb; snow/sleet by the wet-bulb,
   freezing rain where a warm front's warm layer runs over frozen air; hail in hot Cb, graupel in cold showers;
   fog by its cause (radiation, sea/advection, steam over open water in arctic air, frontal mist under rain);
   visibility from fog, rain and snow together. Lenticulars in strong stable flow aloft (a quarter of such episodes).
5. **The season** (`FSeason`, hourly from 1 September): ground wetness (rain and melt in, evaporation out), the snow
   pack (water, depth, density 60 -> 450 kg/m3, melt by degree-days, rain and the spring sun), frozen soil
   (insulated by snow), the sea's temperature (pulled to its climate, cooled by the air), sea ice (forms once the
   sea is at -0.3 C, thickens as the square root of the frost, melts in thaws, broken by gales), glaze.
6. **The sky beyond the weather.** Kp from solar cycle 25 (max 2024.9, declining; cycle 26 ~2036), CME storms
   (more near the maximum, rare Kp 8-9) and 27-day recurrent coronal-hole streams; `Aurora` = how bright it would be
   here (Kp 5.3 faint on the north horizon .. 8.5 overhead). `Halo` (thin Cs), `Rainbow` (showers with a low sun),
   `Noctilucent` (June-July twilight), `Waterspout` (late summer, cold air over the warm sea under Cb) are chances
   0..1; whoever draws them checks darkness, the sun's place and the cloud.

The wind subsystem takes its weather (direction and strength before the day's rhythm, sea breeze and gusts) from the
same chain (`FWindParams::bSynoptic`, `LyudWeather::SynopticWind`), so fronts veer and freshen the wind, the sea's
waves grow with it and the birds feel it.

## Climate (seed 750, 10 years, `tools/weather/run.sh climate`)

January -3.5 C (hard frosts to -18), July 19.7 C (hot days 29), ~930 mm a year, wet ~19 % of hours, May the driest
and clearest, November the greyest, thunder in summer (~17 h a year), fog under 1 km on ~1 % of hours (most in
spring), level snow 10-35 cm at the winter's peak on 35-80 days, sea ice 6-14 cm on 7-50 days in every winter
(thicker in the cold ones), the sea 18 C in August and -0.3 C under ice in February, aurora-worthy Kp on a few
nights a year while cycle 25 declines, halos often ahead of warm fronts, noctilucent clouds on June-July nights,
freezing rain a few times a winter. `WinterSeverity` and `Wetness` scale the winters and the rain.

## API

- `GetState()` -> `FLyudWeatherState` (Blueprint, Python `get_state()`), `GetModelState()` -> `const
  LyudWeather::FWeatherState&` for C++ (include `Weather/WeatherModel.h`). Fields: the episode (`Regime`,
  `NextRegime`, `RegimeProgress`, `HoursToNext`, `Intensity`, pressure and its 3-h tendency), the air (temperature,
  dew point, humidity, freezing level, inversion, convection), the weather's wind and the wind aloft, the three
  cloud layers + total cover + lenticular + shower cells, precipitation (type, mm/h, snow share, cm/h of fresh snow,
  thunder, hail), fog (type, 0..1, visibility, depth), the ground and the sea (wet, snow depth/water/density/age,
  melt, frost, glaze, frozen soil, sea temperature, freezing degree-days, sea ice cm and cover), the sky (Kp,
  aurora, halo, rainbow, noctilucent, waterspout).
- `GetStateAt(FDateTime)`: any moment (forecasts, people's talk, the diary). `Forecast(days)`: text table.
- `GetSky()`: the old short summary (cloud, rain, snow, thunder, fog, temperature, wet), still used by the sound
  and people.
- Events: `OnLightning(Location, DistanceM)` (Poisson, ~2 x Thunder a minute), `OnRegimeChanged(From, To)`,
  `OnPrecipChanged(From, To)`. Sets `lyud.Overcast` (cover > 0.75).
- `Climate` (`FLyudClimateSettings`): seed, wetness, winter severity, mean temperature and swing, solar activity.

## Debug

- `lyud.Weather.Debug 1|2`: on-screen summary / every field.
- `lyud.Weather.Preset <name>`: `clear fair overcast drizzle rain downpour storm thunder fog seafog snow blizzard
  freezingrain frost lenticular aurora halo rainbow`, `off`. A preset plays an episode of the right type (found in
  the chain within weeks of this date in any year, at this hour of the day; the wind follows it) and forces a few
  values.
- `lyud.Weather.Regime <type>`: play one episode type now.
- Single values: `lyud.Weather.Rain / Cloud / Fog / Temp / Thunder / Snow / Kp / SnowDepth / SeaIce / Lenticular`
  (negative = the calendar's). `lyud.Weather.Forecast [days]`, `lyud.Weather.Strike [m]`.
- Outside Unreal: `tools/weather/run.sh test` (climate and laws: continuity, fronts veer the wind and raise the
  pressure, determinism, the scene's start), `climate [seed]`, `days y m d n [seed]` (+ `Saved/Weather/days.csv`),
  `winters [seed]`, `seeds` (seeds that give the scene's start its weather).
