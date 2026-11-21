// Weather and night indoors (docs/indoor_weather.md, backlog item 28): the town on the same Saturday afternoon dry, in the
// rain and in the heat; the cafes, the library and the clubs fill in the rain, the ice-cream kiosk in the heat, and the
// flats' lamps go on sooner on a dark day. Included by peoplesim.cpp (run.sh weather).
#pragma once

struct FIndoorCount
{
	double Indoor = 0.0;        // townsfolk-samples at a public place under a roof (cafes, the library, clubs, shops; not at work)
	double Outdoor = 0.0;       // townsfolk-samples out of doors doing a free activity (not away, not going)
	double IceCream = 0.0;      // at the ice-cream kiosk
	int Samples = 0;
	std::map<std::string, double> ByKind;
	int CafeSeats = 0;          // the cafes' seats (every one of them taken: full)
};

/** Saturday 11:00 + Hours in one weather; samples every ten minutes after the first hour (people settle). */
static bool RunWeather(const char* Name, bool bRainOn, float RainMmH, float TempC, float Wind, float Hours, FIndoorCount& Out, bool bPull = true)
{
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return false;
	}
	W.AssignHomes();
	AttachRail(W);
	if (!bPull)
	{
		W.Pulls.clear();
		W.IndexPulls();
	}
	for (const FPlace& Pl : W.Places)
	{
		for (const int S : Pl.Slots)
		{
			Out.CafeSeats += Pl.Kind == "cafe" && W.Slots[(size_t)S].Kind == "seat" ? W.Slots[(size_t)S].Capacity : 0;
		}
	}
	FClock C = FClock::At(5, 11.f);
	W.bRain = bRainOn;
	W.Rain = RainMmH;
	W.TempC = TempC;
	W.Wind = Wind;
	C.Set(W);
	W.Resettle();
	const int Kiosk = W.FindPlace("P-KIOSK2");
	const float Dt = 1.f;
	for (double T = 0.0; T < Hours * 3600.0; T += Dt)
	{
		C.Seconds += Dt;
		C.Set(W);
		W.Step(Dt);
		W.Events.clear();
		if (T < 3600.0 || std::fmod(T, 600.0) >= Dt)
		{
			continue;
		}
		Out.Samples++;
		for (const FPerson& P : W.People)
		{
			if (!P.bTown || P.Place < 0 || P.Activity < 0 || P.State == EState::Away || P.State == EState::Going)
			{
				continue;
			}
			const FActivity& A = W.Activities[(size_t)P.Activity];
			const FPlace& Pl = W.Places[(size_t)P.Place];
			if (A.bStaff || P.Place == P.Home)
			{
				continue;
			}
			static const std::set<std::string> Public = { "cafe", "restaurant", "library", "club", "culture_club", "hall", "culture", "hotel", "shop", "market" };
			if (Pl.bIndoor && Public.count(Pl.Kind))
			{
				Out.Indoor += 1.0;
				Out.ByKind[Pl.Kind] += 1.0;
			}
			else if (!Pl.bIndoor && !A.bAway)
			{
				Out.Outdoor += 1.0;
			}
			Out.IceCream += P.Place == Kiosk ? 1.0 : 0.0;
		}
	}
	const double N = std::max(1, Out.Samples);
	std::printf("  %-6s indoors %5.1f  out of doors %5.1f  at the ice-cream kiosk %4.1f  (", Name, Out.Indoor / N, Out.Outdoor / N, Out.IceCream / N);
	for (const auto& K : Out.ByKind)
	{
		std::printf(" %s %.1f", K.first.c_str(), K.second / N);
	}
	std::printf(" )\n");
	return true;
}

/** How many flats with windows are lit at 14:00 on a Saturday under this gloom (after a settled morning). */
static int LitAt14(float Gloom)
{
	FWorld W;
	if (!W.Load(DataDir, AnimalsDir))
	{
		return -1;
	}
	W.AssignHomes();
	AttachRail(W);
	FClock C = FClock::At(5, 13.f);
	W.Gloom = Gloom;
	C.Set(W);
	W.Resettle();
	for (double T = 0.0; T < 3600.0; T += 1.0)
	{
		C.Seconds += 1.0;
		C.Set(W);
		W.Step(1.f);
		W.Events.clear();
	}
	std::vector<int> Changed;
	W.UpdateLights(Changed);
	int Lit = 0;
	for (size_t I = 0; I < W.FlatLit.size(); I++)
	{
		Lit += W.FlatLit[I] == 1 ? 1 : 0;
	}
	return Lit;
}

static int IndoorWeather()
{
	std::printf("weather and night indoors (Saturday 11:00-16:00, the town)\n");
	FIndoorCount Dry, Wet, Hot, WetBare;
	if (!RunWeather("dry", false, 0.f, 13.f, 5.f, 5.f, Dry) || !RunWeather("rain", true, 4.f, 11.f, 7.f, 5.f, Wet)
		|| !RunWeather("hot", false, 0.f, 28.f, 3.f, 5.f, Hot) || !RunWeather("rain0", true, 4.f, 11.f, 7.f, 5.f, WetBare, false))
	{
		return 1;
	}
	const double N = std::max(1, Dry.Samples);
	std::printf("  (rain0: the rain without weather_pull.json; the cafes have %d seats)\n", Dry.CafeSeats);
	CHECK(Wet.Indoor > Dry.Indoor * 1.15, "the rain fills the cafes, the library and the clubs (%.1f vs %.1f dry)", Wet.Indoor / N, Dry.Indoor / N);
	CHECK(Wet.Indoor > WetBare.Indoor * 1.1, "the weather's pull is what sends them under a roof (%.1f vs %.1f without it)", Wet.Indoor / N, WetBare.Indoor / N);
	CHECK(Wet.Outdoor < Dry.Outdoor * 0.2, "and the rain empties the outdoors (%.1f vs %.1f dry)", Wet.Outdoor / N, Dry.Outdoor / N);
	CHECK(Wet.ByKind["cafe"] >= 0.9 * Wet.CafeSeats * N, "the cafes are full in the rain (%.1f of %d seats)", Wet.ByKind["cafe"] / N, Wet.CafeSeats);
	CHECK(Hot.IceCream > Dry.IceCream + 0.25 * N, "busier at the ice-cream kiosk on a hot afternoon (one visit spot: a buyer at a time) (%.1f vs %.1f on a mild one)", Hot.IceCream / N, Dry.IceCream / N);
	CHECK(Hot.Indoor < Wet.Indoor, "fewer indoors on a hot day than in the rain (%.1f vs %.1f)", Hot.Indoor / N, Wet.Indoor / N);
	const int LitBright = LitAt14(0.f), LitDark = LitAt14(0.97f);
	CHECK(LitBright == 0, "no flat lit at 14:00 under a bright sky (%d)", LitBright);
	CHECK(LitDark > 10, "lamps on at 14:00 on a dark rainy day (%d flats)", LitDark);
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}
