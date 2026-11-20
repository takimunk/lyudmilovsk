// The allotment's rules (Private/Garden/GardenModel.*) run outside the engine over whole seasons of the game's own weather
// (Private/Weather/WeatherModel.*, the same chain and seed the game plays), docs/allotment.md. tools/gardensim/run.sh.
//   run.sh            the checks (exit 1 on a failure)
//   run.sh season Y M D DAYS [crop]   prints a gardener's diary: a well kept bed sown with crop (all six by default)
#include "GardenModel.h"
#include "WeatherModel.h"
#include "WindModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudGarden;

static int Failures = 0;
#define CHECK(Cond, ...) do { if (!(Cond)) { ++Failures; std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } else { std::printf("  ok   "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

/** The game's air at the beds for an hour (as ULyudGarden reads it from the weather subsystem). */
struct FWeatherAir
{
	LyudWeather::FChain Chain;
	LyudWeather::FSeason Season;
	FWeatherAir() : Chain(LyudWeather::FClimate()) {}
	FAir operator()(double H)
	{
		const LyudWeather::FWeatherState S = LyudWeather::Full(H, Chain, Season);
		FAir A;
		A.TempC = S.TempC;
		A.RainMm = S.PrecipRate;
		A.Snow = S.SnowFraction;
		A.Cloud = S.CloudCover;
		A.SunElev = (float)LyudWeather::SunElevationDeg(H, Chain.GetClimate());
		A.Wind = S.WindSpeed;
		return A;
	}
	/** The coldest and the warmest of the next hours (what the neighbour's forecast and the game's prompt use). */
	void Soon(double H, float Hours, float& Coldest, float& Hottest)
	{
		Coldest = 99.f;
		Hottest = -99.f;
		for (float T = 0.f; T <= Hours; T += 2.f)
		{
			const LyudWeather::FWeatherState S = LyudWeather::Instant(H + T, Chain);
			Coldest = std::min(Coldest, S.TempC);
			Hottest = std::max(Hottest, S.TempC);
		}
	}
};

struct FTally
{
	std::map<std::string, int> Picks, Things, Deaths, Spoils;
	std::map<std::string, float> FirstRipeDays;
	int Quality[4] = { 0, 0, 0, 0 };
};

/**
 * A gardener who comes twice a day (8 and 19 o'clock) and does what the bed needs; Care: 0 never comes back after sowing,
 * 1 always does everything. Resows a cell with Crops[cell % n] when it is free.
 */
static FTally Garden(FModel& M, FWeatherAir& Air, double H0, double Days, const std::vector<std::string>& Crops, float Care,
	bool bVerbose)
{
	FTally T;
	M.Init();
	M.Rent(H0);
	M.Now = H0;
	M.Fill();
	std::vector<double> SownAt(M.Bed.Cells.size(), H0);
	unsigned Rng = 12345;
	auto Roll = [&Rng]() { Rng = Rng * 1103515245u + 12345u; return float((Rng >> 8) & 0xFFFF) / 65535.f; };
	for (double H = H0; H < H0 + Days * 24.0; H += 1.0)
	{
		M.AdvanceTo(H, Air);
		const double Hod = std::fmod(H, 24.0);
		const bool bVisit = (std::fabs(Hod - 8.0) < 0.5 || std::fabs(Hod - 19.0) < 0.5) && Roll() < Care;
		// a death or spoiling is counted as it happens (the gardener may come days later)
		for (size_t I = 0; I < M.Bed.Cells.size(); ++I)
		{
			FCell& C = M.Bed.Cells[I];
			if (C.State == ECell::Dead && C.Picked >= 0)
			{
				T.Deaths[C.Crop]++;
				C.Picked = -1;     // counted
				if (bVerbose) { std::printf("    day %5.1f: %s died (water %.2f, hurt %.2f)\n", (H - H0) / 24.0, C.Crop.c_str(), C.Water, C.Hurt); }
			}
			if (C.State == ECell::Spoilt && C.Picked >= 0)
			{
				T.Spoils[C.Crop]++;
				C.Picked = -1;
			}
			if (C.State == ECell::Ripe && T.FirstRipeDays.find(C.Crop) == T.FirstRipeDays.end())
			{
				T.FirstRipeDays[C.Crop] = float((H - SownAt[I]) / 24.0);
			}
		}
		if (!bVisit)
		{
			continue;
		}
		float Coldest, Hottest;
		Air.Soon(H, 14.f, Coldest, Hottest);
		bool bCover = false, bUncover = false;
		for (int I = 0; I < (int)M.Bed.Cells.size(); ++I)
		{
			const ENeed N = M.Need(I, Coldest, Hottest);
			FHarvest Hv;
			switch (N)
			{
			case ENeed::Dig: M.Dig(I); break;
			case ENeed::Sow:
				if (M.Sow(I, M.Data.Crop(Crops[(size_t)I % Crops.size()])->Seed, H)) { SownAt[(size_t)I] = H; }
				break;
			case ENeed::Water: if (M.CanLeft == 0) { M.Fill(); } M.Water(I); break;
			case ENeed::Weed: M.Weed(I); break;
			case ENeed::Pick:
				if (M.Pick(I, Hv))
				{
					T.Picks[Hv.Item]++;
					T.Things[Hv.Item] += Hv.Count;
					T.Quality[Hv.Quality]++;
					if (bVerbose) { std::printf("    day %5.1f: picked %d %s (quality %d)\n", (H - H0) / 24.0, Hv.Count, Hv.Item.c_str(), Hv.Quality); }
				}
				break;
			case ENeed::Clear: M.Clear(I); break;
			case ENeed::Cover: bCover = true; break;
			case ENeed::Uncover: bUncover = true; break;
			default: break;
			}
			// keep the weeds down even when something else came first
			if (M.Bed.Cells[(size_t)I].Weeds > 0.35f) { M.Weed(I); }
		}
		if (bCover) { M.SetCover(true); }
		else if (bUncover) { M.SetCover(false); }
		else if (M.Bed.bCovered && Coldest > 6.f && Hottest < 24.f) { M.SetCover(false); }
	}
	return T;
}

static void Print(const char* Title, const FTally& T)
{
	std::printf("  %s:", Title);
	for (const auto& P : T.Things) { std::printf(" %s %d (%d pickings)", P.first.c_str(), P.second, T.Picks.at(P.first)); }
	for (const auto& P : T.Deaths) { std::printf("; %s died %d", P.first.c_str(), P.second); }
	for (const auto& P : T.Spoils) { std::printf("; %s spoilt %d", P.first.c_str(), P.second); }
	std::printf("; quality 1/2/3: %d/%d/%d\n", T.Quality[1], T.Quality[2], T.Quality[3]);
	for (const auto& P : T.FirstRipeDays) { std::printf("    %s first ripe after %.1f days\n", P.first.c_str(), P.second); }
}

static int Run(const FData& Data)
{
	FModel M;
	M.Data = Data;
	const std::vector<std::string> All = { "radish", "dill", "strawberry", "potato", "tomato", "sunflower" };
	FWeatherAir Air;

	std::printf("data\n");
	CHECK(Data.Crops.size() == 6 && Data.Cells == 6, "six crops, six cells (%d, %d)", (int)Data.Crops.size(), Data.Cells);
	for (const FCrop& C : Data.Crops)
	{
		CHECK(Data.CropBySeed(C.Seed) == &C && Data.CropByYield(C.Yield) == &C, "%s: seed %s, yield %s", C.Id.c_str(), C.Seed.c_str(), C.Yield.c_str());
	}

	std::printf("a summer, well kept (1 May - 30 Sep 2027, the game's weather)\n");
	{
		const double H0 = LyudWeather::HoursSince2000(2027, 5, 1, 7.0);
		const FTally T = Garden(M, Air, H0, 152.0, All, 1.f, false);
		Print("harvest", T);
		for (const FCrop& C : Data.Crops)
		{
			CHECK(T.Picks.count(C.Yield) && T.Picks.at(C.Yield) >= 1, "%s is picked in a summer", C.Id.c_str());
		}
		CHECK(T.FirstRipeDays.count("radish") && T.FirstRipeDays.at("radish") <= 6.f, "radish ripe in under a week from 1 May (%.1f days)",
			T.FirstRipeDays.count("radish") ? T.FirstRipeDays.at("radish") : -1.f);
		CHECK(T.Deaths.empty(), "nothing dies when it is kept well (%d)", (int)T.Deaths.size());
		CHECK(T.Quality[3] > T.Quality[1] + T.Quality[2], "a well kept bed gives mostly excellent harvests (%d of %d)", T.Quality[3],
			T.Quality[1] + T.Quality[2] + T.Quality[3]);
		CHECK(T.Picks.count("radish") && T.Picks.at("radish") >= 10, "radish again and again through the summer (%d)",
			T.Picks.count("radish") ? T.Picks.at("radish") : 0);
	}

	std::printf("midsummer: radish in about three days (1 Jul 2027)\n");
	{
		const double H0 = LyudWeather::HoursSince2000(2027, 7, 1, 8.0);
		const FTally T = Garden(M, Air, H0, 14.0, { "radish" }, 1.f, false);
		const float D = T.FirstRipeDays.count("radish") ? T.FirstRipeDays.at("radish") : 99.f;
		CHECK(D >= 2.5f && D <= 4.5f, "radish ripe in %.1f days in July", D);
	}

	std::printf("the game's autumn: from its first day (20 Oct 2026), well kept, the film on cold nights\n");
	{
		const double H0 = LyudWeather::HoursSince2000(2026, 10, 20, 16.0);
		const FTally T = Garden(M, Air, H0, 30.0, All, 1.f, true);
		Print("harvest", T);
		CHECK(T.Picks.count("radish") >= 1, "radishes still come in late October under film (first after %.1f days)",
			T.FirstRipeDays.count("radish") ? T.FirstRipeDays.at("radish") : -1.f);
		CHECK(T.FirstRipeDays.count("radish") && T.FirstRipeDays.at("radish") <= 10.f, "...within ten days");
		CHECK(!T.Picks.count("tomatoes"), "no tomatoes in November");
	}

	std::printf("autumn neglected: sown on 20 Oct, never looked at again\n");
	{
		const double H0 = LyudWeather::HoursSince2000(2026, 10, 20, 16.0);
		FModel N;
		N.Data = Data;
		N.Init();
		N.Rent(H0);
		N.Now = H0;
		for (int I = 0; I < 6; ++I)
		{
			N.Dig(I);
			N.Sow(I, Data.Crop(All[(size_t)I])->Seed, H0);
		}
		N.AdvanceTo(H0 + 40.0 * 24.0, Air);
		for (const FCell& C : N.Bed.Cells)
		{
			std::printf("    %-10s %-8s growth %.2f water %.2f weeds %.2f hurt %.2f\n", C.Crop.c_str(), CellWord(C.State), C.Growth, C.Water, C.Weeds, C.Hurt);
		}
		const FCell& Tomato = N.Bed.Cells[4];
		CHECK(Tomato.State == ECell::Dead, "the frost kills tomatoes left open in November");
		CHECK(N.Bed.Cells[2].State != ECell::Dead, "strawberries (perennial) live through the November frosts");
	}

	std::printf("summer drought: sown 1 Jun 2027, never watered (rain only)\n");
	{
		const double H0 = LyudWeather::HoursSince2000(2027, 6, 1, 8.0);
		FModel N;
		N.Data = Data;
		N.Init();
		N.Rent(H0);
		N.Now = H0;
		N.Dig(0);
		N.Sow(0, "seedling_tomato", H0);
		N.Bed.Cells[0].Water = 0.4f;
		N.AdvanceTo(H0 + 12.0 * 24.0, Air);
		const FCell& C = N.Bed.Cells[0];
		const int Q = N.Quality(C);
		std::printf("    tomato after 12 days unwatered: %s growth %.2f water %.2f weeds %.2f care %.2f quality %d\n", CellWord(C.State), C.Growth, C.Water,
			C.Weeds, C.Care, Q);
		CHECK(C.State != ECell::Ripe || Q < 3, "an unwatered, unweeded tomato is never excellent in 12 days");
		CHECK(C.Weeds > 0.5f, "weeds take over an unweeded bed in June (%.2f)", C.Weeds);
	}

	std::printf("the film in summer\n");
	{
		FAir Noon;
		Noon.TempC = 26.f;
		Noon.Cloud = 0.f;
		Noon.SunElev = 55.f;
		FAir Night;
		Night.TempC = 1.f;
		CHECK(M.BedTemp(Noon, true) > 33.f, "a shut film bed in the July sun is too hot (%.1f C)", M.BedTemp(Noon, true));
		CHECK(M.BedTemp(Night, true) > M.BedTemp(Night, false) + 1.5f, "the film keeps a frosty night off (%.1f vs %.1f)", M.BedTemp(Night, true),
			M.BedTemp(Night, false));
		M.Init();
		M.Rent(0.0);
		M.Dig(0);
		M.Sow(0, "seedling_tomato", 0.0);
		M.SetCover(true);
		CHECK(M.Need(0, 18.f, 28.f) == ENeed::Uncover, "the neighbour says to roll the film up on a hot day");
		M.SetCover(false);
		CHECK(M.Need(0, -1.f, 10.f) == ENeed::Cover, "...and to pull it down before a frost");
	}

	std::printf("actions\n");
	{
		M.Init();
		CHECK(!M.Dig(0), "nothing can be dug before the bed is rented");
		M.Rent(0.0);
		CHECK(M.Dig(0) && M.Bed.Cells[0].State == ECell::Dug, "dig");
		CHECK(!M.Sow(0, "apple", 0.0), "an apple is not a seed");
		CHECK(M.Sow(0, "seeds_radish", 0.0) && M.Bed.Cells[0].Crop == "radish", "sow radish");
		CHECK(!M.Water(0), "no water without filling the can");
		M.Fill();
		M.Bed.Cells[0].Water = 0.2f;
		CHECK(M.Water(0) && M.CanLeft == Data.R.CanFills - 1, "water: one from the can");
		M.Bed.Cells[0].State = ECell::Ripe;
		M.Bed.Cells[0].Care = 0.95f;
		FHarvest Hv;
		CHECK(M.Pick(0, Hv) && Hv.Item == "radish" && Hv.Count == 5 && Hv.Quality == 3, "pick: 5 excellent radishes (%d, q%d)", Hv.Count, Hv.Quality);
		CHECK(M.Bed.Cells[0].State == ECell::Dug, "the cell is free again");
		M.Sow(0, "seeds_dill", 0.0);
		M.Bed.Cells[0].State = ECell::Ripe;
		M.Pick(0, Hv);
		CHECK(M.Bed.Cells[0].State == ECell::Growing && M.Bed.Cells[0].Growth > 0.4f, "dill comes again after cutting");
		M.Bed.Cells[1].State = ECell::Grass;
		CHECK(M.Need(1) == ENeed::Dig, "grass needs digging");
	}

	std::printf("save and load\n");
	{
		M.Init();
		M.Rent(100.0);
		M.Dig(2);
		M.Sow(2, "seedling_strawberry", 100.0);
		M.Bed.Cells[2].Growth = 0.4321f;
		M.SetCover(true);
		M.CanLeft = 3;
		M.JamQuality = 2;
		M.Now = 1234.5;
		std::map<std::string, std::string> S;
		M.Save(S);
		FModel L;
		L.Data = Data;
		L.LoadState(S);
		CHECK(L.Bed.bRented && L.Bed.bCovered && L.CanLeft == 3 && L.JamQuality == 2 && std::fabs(L.Now - 1234.5) < 1e-3, "the bed's own state");
		CHECK(L.Bed.Cells[2].State == ECell::Growing && L.Bed.Cells[2].Crop == "strawberry" && std::fabs(L.Bed.Cells[2].Growth - 0.4321f) < 1e-3, "a cell");
		CHECK(L.Bed.Cells[0].State == ECell::Grass, "untouched cells stay grass");
	}

	std::printf("a save left for a year is stepped at most %.0f days\n", Data.R.CatchUpDays);
	{
		M.Init();
		M.Rent(0.0);
		M.Now = 0.0;
		int Steps = M.AdvanceTo(24.0 * 365.0, [](double) { return FAir(); });
		CHECK(Steps <= (int)(Data.R.CatchUpDays * 24.f) + 1, "%d hours stepped", Steps);
	}

	std::printf("the neighbour's bed\n");
	{
		CHECK(NeighbourGrowth("potato", 200.f) > 0.5f && NeighbourGrowth("potato", 300.f) < 0.f, "her potatoes: up in July, dug by October");
		CHECK(NeighbourGrowth("strawberry", 10.f) > 0.f, "strawberries keep their leaves over winter");
		CHECK(NeighbourGrowth("tomato", 296.f) < 0.f, "no tomatoes on the game's first day");
		CHECK(Season(296.f) == 2 && Season(10.f) == 3 && Season(100.f) == 0 && Season(200.f) == 1, "seasons");
	}
	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const char* Root = std::getenv("LYUD_ROOT");
	const std::string Path = std::string(Root ? Root : ".") + "/unreal/Lyudmilovsk/Content/Garden/garden.json";
	FData Data;
	std::string Error;
	if (!Data.Load(ReadFile(Path), Error))
	{
		std::printf("FAIL garden.json: %s\n", Error.c_str());
		return 1;
	}
	if (Argc >= 6 && std::strcmp(Argv[1], "season") == 0)
	{
		FModel M;
		M.Data = Data;
		FWeatherAir Air;
		std::vector<std::string> Crops = { "radish", "dill", "strawberry", "potato", "tomato", "sunflower" };
		if (Argc >= 7)
		{
			Crops = { Argv[6] };
		}
		const double H0 = LyudWeather::HoursSince2000(std::atoi(Argv[2]), std::atoi(Argv[3]), std::atoi(Argv[4]), 8.0);
		Print("harvest", Garden(M, Air, H0, std::atof(Argv[5]), Crops, 1.f, true));
		return 0;
	}
	return Run(Data);
}
