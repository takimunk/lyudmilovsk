#include "Sea/Tide.h"

#include "Sky/Ephemeris.h"

LyudTide::FState LyudTide::At(const FDateTime& Utc, double LongitudeDeg)
{
	// the water answers the sky LagHours late; a little later too, to tell flood from ebb
	const FDateTime When = Utc - FTimespan::FromHours(LagHours);
	double SunHa, MoonHa;
	LyudEphemeris::HourAngles(When, LongitudeDeg, SunHa, MoonHa);
	const double Am = FMath::DegreesToRadians(2.0 * MoonHa), As = FMath::DegreesToRadians(2.0 * SunHa);
	// the two pulls as phasors: their sum's length is the tide's size (springs .. neaps), its real part the tide now
	const double Re = FMath::Cos(Am) + SunShare * FMath::Cos(As);
	const double Im = FMath::Sin(Am) + SunShare * FMath::Sin(As);
	const double Size = FMath::Max(FMath::Sqrt(Re * Re + Im * Im), 1e-6);

	FState S;
	S.Phase = FMath::Clamp(Re / Size, -1.0, 1.0);
	S.Spring = FMath::Clamp((Size - (1.0 - SunShare)) / (2.0 * SunShare), 0.0, 1.0);
	S.LowM = FMath::Lerp(NeapLowM, SpringLowM, S.Spring);
	S.RangeM = -S.LowM;
	S.LevelM = 0.5 * S.RangeM * (S.Phase - 1.0);
	// the phasor turns with the hour angles: just past high water (angle 0) it falls, the half turn before it rises
	S.bRising = FMath::Atan2(Im, Re) < 0.0;
	return S;
}

double LyudTide::RecentHighM(const FDateTime& Utc, double LongitudeDeg, double Hours)
{
	double High = At(Utc, LongitudeDeg).LevelM;
	const int32 Steps = 6;
	for (int32 I = 1; I <= Steps; ++I)
	{
		High = FMath::Max(High, At(Utc - FTimespan::FromHours(Hours * I / Steps), LongitudeDeg).LevelM);
	}
	return High;
}
