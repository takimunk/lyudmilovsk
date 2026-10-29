"""Headless check of the weather engine in the game (ULyudWeatherSubsystem): the state now, a few presets and
episode types, single forced values, the forecast and the wind following the fronts. Run with test_weather.sh
(-game -nullrhi on an empty map, a minute). Prints WEATHERTEST lines and PASS/FAIL."""
import unreal

st = {"t": 0.0, "i": -1, "fails": 0, "wait": 0}
STEPS = [
    ("now", []),
    ("clear", ["lyud.Weather.Preset clear"]),
    ("rain", ["lyud.Weather.Preset rain"]),
    ("storm", ["lyud.Weather.Preset storm"]),
    ("fog", ["lyud.Weather.Preset fog"]),
    ("snow", ["lyud.Weather.Preset snow"]),
    ("frost", ["lyud.Weather.Preset frost"]),
    ("aurora", ["lyud.Weather.Preset aurora"]),
    ("regime_coldfront", ["lyud.Weather.Preset off", "lyud.Weather.Regime ColdFront"]),
    ("regime_high", ["lyud.Weather.Regime High"]),
    ("off", ["lyud.Weather.Preset off"]),
    ("forced_temp", ["lyud.Weather.Temp -5", "lyud.Weather.Rain 2"]),
    ("done", ["lyud.Weather.Temp -100", "lyud.Weather.Rain -1", "lyud.Weather.Forecast 1"]),
]


def world():
    for w in unreal.ObjectIterator(unreal.World):
        if unreal.GameplayStatics.get_player_controller(w, 0):
            return w


def log(s):
    unreal.log_warning("WEATHERTEST " + s)


def check(ok, what):
    log(("ok   " if ok else "FAIL ") + what)
    if not ok:
        st["fails"] += 1


def judge(name, s, wind):
    p = s.precip
    if name == "now":
        check(-30 < s.temp_c < 35 and 950 < s.pressure_h_pa < 1060, "state is sane")
    elif name == "clear":
        check(s.cloud_cover < 0.05 and s.precip_rate == 0, "clear: no cloud, dry")
    elif name == "rain":
        check(s.precip_rate > 2 and s.cloud_cover > 0.9 and s.wet > 0.5, "rain: wet, grey")
    elif name == "storm":
        check(s.precip_rate > 6 and wind.get_mean_speed() > 12, "storm: heavy rain, gale")
    elif name == "fog":
        check(s.fog > 0.8 and s.visibility_m < 400, "fog: thick, short visibility")
    elif name == "snow":
        check(s.snow_fraction > 0.9 and s.snow_depth_cm > 20 and s.snowfall_cm_per_hour > 0.5, "snow: falling, lying")
    elif name == "frost":
        check(s.temp_c < -10 and s.sea_ice_cover > 0.9, "frost: hard frost, sea frozen")
    elif name == "aurora":
        check(s.kp > 7 and s.aurora > 0.5, "aurora: Kp high")
    elif name == "regime_coldfront":
        check(s.regime == unreal.LyudWeatherRegime.COLD_FRONT, "regime forced to ColdFront")
        st["cf_wind"] = wind.get_from_degrees()
    elif name == "regime_high":
        check(s.regime == unreal.LyudWeatherRegime.HIGH, "regime forced to High")
        check(abs(wind.get_from_degrees() - st.get("cf_wind", -999)) > 1, "the wind follows the forced episode")
    elif name == "forced_temp":
        check(p in (unreal.LyudPrecipType.SNOW, unreal.LyudPrecipType.SLEET), "forced -5 C + rain falls as snow")


def tick(dt):
    try:
        w = world()
        if not w:
            return
        st["t"] += dt
        if st["t"] < 3.0:
            return
        weather = unreal.LyudWeatherSubsystem.get_weather(w)
        wind = unreal.LyudWindSubsystem.get_wind(w)
        if st["i"] >= 0 and st["wait"] > 0:
            st["wait"] -= 1
            return
        if st["i"] >= 0:
            name = STEPS[st["i"]][0]
            s = weather.get_state()
            log(name + ": " + weather.debug_report(True).replace("\n", " / "))
            log(name + " wind: " + wind.debug_report())
            judge(name, s, wind)
        st["i"] += 1
        if st["i"] >= len(STEPS):
            log("forecast:\n" + weather.forecast(1.0))
            log("PASS" if st["fails"] == 0 else "FAIL %d" % st["fails"])
            unreal.SystemLibrary.execute_console_command(w, "quit")
            unreal.unregister_slate_post_tick_callback(handle)
            return
        for c in STEPS[st["i"]][1]:
            unreal.SystemLibrary.execute_console_command(w, c)
        st["wait"] = 5
    except Exception as e:
        log("FAIL exception %s" % e)
        unreal.SystemLibrary.execute_console_command(None, "quit")
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)
