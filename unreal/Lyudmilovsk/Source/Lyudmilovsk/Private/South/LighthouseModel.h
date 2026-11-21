#pragma once

// The Mayak lighthouse's rules (docs/lighthouse.md), plain C++ with no engine types so tools/lighthousesim can test them:
// when the lamp burns, how the fog dims the beam and shortens it, when the foghorn sounds, the light's character (the lens
// turns, two bull's-eye panels, a white flash every FlashPeriodS), how bright the lantern looks from where the viewer stands,
// and the keeper's weather log: the readings at 08, 14 and 20 turned into the codes the game prints (the words are keys in
// Content/Localization/lighthouse, the engine side formats them).

#include <string>
#include <vector>

namespace LyudLighthouse
{
	// ---- the light ---------------------------------------------------------------------------------------------------

	/** The lens turns once in this many seconds; with two panels the beam sweeps past a viewer twice a turn. */
	constexpr float TurnPeriodS = 20.f;
	constexpr float FlashPeriodS = TurnPeriodS / 2.f;
	/** The keeper lights the lamp this long before sunset and puts it out this long after sunrise, hours. */
	constexpr float LightBeforeSunsetH = 0.15f;
	constexpr float PutOutAfterSunriseH = 0.2f;
	/** By day the lamp burns too when the fog is thicker than this (m of visibility). */
	constexpr float DayFogM = 1000.f;
	/** The lamp comes up to full (the mantle warming, the clockwork wound) or dies down over this, seconds. */
	constexpr float WarmS = 25.f;
	/** The beam card's longest reach (a clear night) and its shortest (thick fog), m. */
	constexpr float BeamLongM = 650.f;
	constexpr float BeamShortM = 45.f;
	/** The spot light's reach, m (it lights the sea and the cliff where the beam strikes). */
	constexpr float SpotReachM = 600.f;

	float Smooth(float E0, float E1, float X);

	/** Is it the lamp's time: night (from just before sunset to just after sunrise), or a thick fog by day. Hours 0..24. */
	bool LampTime(float Hour, float Sunrise, float Sunset, float VisibilityM);
	/** When the keeper lit it today / will put it out (hours), for the log. */
	inline float LitAt(float Sunset) { return Sunset - LightBeforeSunsetH; }
	inline float OutAt(float Sunrise) { return Sunrise + PutOutAfterSunriseH; }

	/** The lamp's level one step on: 0 out .. 1 full, moving toward the target over WarmS. */
	float StepLevel(float Level, bool bOn, float Dt);

	/** The lens's turn at a time, degrees 0..360 (the world's clock: everybody sees the same flash). */
	float TurnDeg(double Seconds);

	/** How the weather dims the light: the beam card's strength 0..1 and its length in m, the spot light's share 0..1.
	 *  Thin haze lifts the card a little (there is something to scatter the light), fog swallows it: dimmer and shorter. */
	struct FBeam
	{
		float Card = 0.f;
		float LengthM = BeamLongM;
		float Spot = 0.f;
	};
	FBeam BeamFor(float VisibilityM, float Rain, float Level);

	/** The foghorn's loudness 0..1 by visibility (the same curve the soundscape used for the station's lantern). */
	float FogHorn(float VisibilityM);

	/** How bright the lantern's glass looks to a viewer at an angle (degrees) off the nearer beam: a narrow flash as the
	 *  panel sweeps past, the steady glow of the mantle between. Level scales it. */
	float GlassGlow(float OffBeamDeg, float Level);
	/** The angle (degrees, 0..90) between a direction (yaw, degrees) and the nearer of the two beams at a turn. */
	float OffBeam(float TurnDegrees, float ViewYawDeg);

	// ---- the keeper's weather log -------------------------------------------------------------------------------------

	/** One reading, as the weather model gives it (LyudWeather::FWeatherState's fields). */
	struct FReading
	{
		float Hour = 8.f;
		float WindFromDeg = 0.f;     // where the wind comes from, compass degrees (0 north, 90 east)
		float WindMs = 0.f;
		float PressureHPa = 1013.f;
		float TendencyHPa3h = 0.f;   // hPa per 3 h
		float TempC = 10.f;
		float Cloud = 0.f;           // 0..1
		float PrecipMmH = 0.f;
		float SnowFraction = 0.f;    // 0 rain .. 1 snow
		float Thunder = 0.f;         // 0..1
		float Fog = 0.f;             // 0..1
		float VisibilityM = 30000.f;
		float SeaIceCover = 0.f;     // 0..1
	};

	/** 0 N, 1 NE, 2 E, 3 SE, 4 S, 5 SW, 6 W, 7 NW (the eight points the log is written in). */
	int Compass8(float FromDeg);
	/** Beaufort force 0..12. */
	int Beaufort(float Ms);
	/** The sky's word: 0 clear, 1 fair (a few clouds), 2 cloudy, 3 overcast, 4 drizzle, 5 rain, 6 heavy rain, 7 sleet, 8 snow,
	 *  9 thunder, 10 fog, 11 mist (haze). */
	int SkyCode(const FReading& R);
	/** The pressure's way: -1 falling, 0 steady, 1 rising (a change of more than 1 hPa in 3 h). */
	int Tendency(float HPa3h);
	/** Visibility in the log's steps: 0 under 200 m, 1 under 1 km, 2 under 4 km, 3 under 10 km, 4 more. */
	int VisClass(float M);

	/** The keeper's remark at the day's end (a key suffix of lh.log.remark.*), or "" for a plain day: storm, fog hours with the
	 *  horn, a frost, a calm clear night, the ice; Seed picks among the variants of one kind (the same day, the same word). */
	std::string Remark(const std::vector<FReading>& Day, float FogHornHours, unsigned Seed);
	/** Hours the horn sounded over a day sampled hourly (visibility per hour). */
	float HornHours(const std::vector<float>& HourlyVisibilityM);
}
