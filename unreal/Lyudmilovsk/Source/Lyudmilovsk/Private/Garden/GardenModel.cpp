#include "GardenModel.h"

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace LyudGarden
{
	namespace
	{
		float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }
		float Lerp(float A, float B, float K) { return A + (B - A) * K; }

		/** How fast a crop grows at warmth T: 0 at Base, 1 from Best to Hot, back to 0 at Hot + 8. */
		float Warmth(const FCrop& C, float T)
		{
			if (T <= C.Base)
			{
				return 0.f;
			}
			if (T < C.Best)
			{
				return (T - C.Base) / (C.Best - C.Base);
			}
			if (T <= C.Hot)
			{
				return 1.f;
			}
			return Clamp01(1.f - (T - C.Hot) / 8.f);
		}

		std::string Num(double V)
		{
			char B[48];
			std::snprintf(B, sizeof(B), "%.4f", V);
			return B;
		}
	}

	const char* CellWord(ECell C)
	{
		switch (C)
		{
		case ECell::Grass: return "grass";
		case ECell::Dug: return "dug";
		case ECell::Growing: return "growing";
		case ECell::Ripe: return "ripe";
		case ECell::Spoilt: return "spoilt";
		case ECell::Dead: return "dead";
		}
		return "grass";
	}

	const char* NeedWord(ENeed N)
	{
		switch (N)
		{
		case ENeed::None: return "none";
		case ENeed::Dig: return "dig";
		case ENeed::Sow: return "sow";
		case ENeed::Water: return "water";
		case ENeed::Weed: return "weed";
		case ENeed::Cover: return "cover";
		case ENeed::Uncover: return "uncover";
		case ENeed::Pick: return "pick";
		case ENeed::Clear: return "clear";
		case ENeed::Wait: return "wait";
		}
		return "none";
	}

	int Stage(const FCell& C)
	{
		if (C.State == ECell::Ripe || C.State == ECell::Spoilt)
		{
			return 4;
		}
		if (C.State != ECell::Growing && C.State != ECell::Dead)
		{
			return -1;
		}
		return C.Growth < 0.08f ? 0 : (C.Growth < 0.35f ? 1 : (C.Growth < 0.75f ? 2 : 3));
	}

	bool FData::Load(const std::string& Json, std::string& Error)
	{
		LyudJson::FValue Root;
		if (!LyudJson::Parse(Json, Root, Error))
		{
			return false;
		}
		*this = FData();
		const LyudJson::FValue& Ru = Root["rules"];
		R.CoverNight = Ru["cover_night"].F(R.CoverNight);
		R.CoverSun = Ru["cover_sun"].F(R.CoverSun);
		R.OpenSun = Ru["open_sun"].F(R.OpenSun);
		R.RainWet = Ru["rain_wet"].F(R.RainWet);
		R.CanWater = Ru["can_water"].F(R.CanWater);
		R.CanFills = Ru["can_fills"].I(R.CanFills);
		R.WeedDays = Ru["weed_days"].F(R.WeedDays);
		R.WeedBase = Ru["weed_base"].F(R.WeedBase);
		R.DryBelow = Ru["dry_below"].F(R.DryBelow);
		R.FrostHurt = Ru["frost_hurt"].F(R.FrostHurt);
		R.SpoilDays = Ru["spoil_days"].F(R.SpoilDays);
		R.CatchUpDays = Ru["catch_up_days"].F(R.CatchUpDays);
		R.Rent = Ru["rent"].I(R.Rent);
		Cells = Root["cells"].I(Cells);
		for (const LyudJson::FValue& V : Root["crops"].Items)
		{
			FCrop C;
			C.Id = V["id"].S();
			C.Species = V["species"].S();
			C.Seed = V["seed"].S();
			C.Yield = V["yield"].S();
			C.YieldCount = V["yield_count"].I(C.YieldCount);
			C.Days = V["days"].F(C.Days);
			C.Base = V["base"].F(C.Base);
			C.Best = V["best"].F(C.Best);
			C.Hot = V["hot"].F(C.Hot);
			C.Thirst = V["thirst"].F(C.Thirst);
			C.FrostKill = V["frost_kill"].F(C.FrostKill);
			C.RipeDays = V["ripe_days"].F(C.RipeDays);
			C.Again = V["again"].F(C.Again);
			C.Pickings = V["pickings"].I(C.Pickings);
			C.bPerennial = V["perennial"].B;
			C.bSeedling = V["seedling"].B;
			if (C.Id.empty() || C.Seed.empty() || C.Yield.empty() || C.Days <= 0.f || C.Best <= C.Base)
			{
				Error = "bad crop " + C.Id;
				return false;
			}
			Crops.push_back(C);
		}
		if (Crops.empty() || Cells <= 0)
		{
			Error = "no crops or cells";
			return false;
		}
		return true;
	}

	const FCrop* FData::Crop(const std::string& Id) const
	{
		for (const FCrop& C : Crops)
		{
			if (C.Id == Id)
			{
				return &C;
			}
		}
		return nullptr;
	}

	const FCrop* FData::CropBySeed(const std::string& Item) const
	{
		for (const FCrop& C : Crops)
		{
			if (C.Seed == Item)
			{
				return &C;
			}
		}
		return nullptr;
	}

	const FCrop* FData::CropByYield(const std::string& Item) const
	{
		for (const FCrop& C : Crops)
		{
			if (C.Yield == Item)
			{
				return &C;
			}
		}
		return nullptr;
	}

	void FModel::Init()
	{
		Bed = FBed();
		Bed.Id = "hero";
		Bed.Cells.assign((size_t)Data.Cells, FCell());
		Now = -1.0;
		CanLeft = 0;
		JamQuality = 0;
		JamAt = 0.0;
		JamJarQuality = 0;
		LastQualities.clear();
	}

	bool FModel::Rent(double Hours)
	{
		if (Bed.bRented)
		{
			return false;
		}
		Bed.bRented = true;
		if (Now < 0.0)
		{
			Now = Hours;
		}
		return true;
	}

	float FModel::BedTemp(const FAir& A, bool bCovered) const
	{
		const float Sun = A.SunElev > 0.f ? std::sin(A.SunElev * 0.0174533f) * Clamp01(1.f - A.Cloud * 0.85f) : 0.f;
		if (bCovered)
		{
			// the film keeps the day's warmth in at night and traps the sun: in a summer noon a shut bed is too hot
			return A.TempC + Data.R.CoverNight + Data.R.CoverSun * Sun;
		}
		return A.TempC + Data.R.OpenSun * Sun;
	}

	void FModel::Step(const FAir& A, float Dt)
	{
		if (!Bed.bRented)
		{
			return;
		}
		const float T = BedTemp(A, Bed.bCovered);
		for (FCell& C : Bed.Cells)
		{
			StepCell(C, A, T, Dt);
		}
	}

	void FModel::StepCell(FCell& C, const FAir& A, float T, float Dt)
	{
		const FRules& R = Data.R;
		const float Day = Dt / 24.f;
		// the soil's water: rain wets an open bed (not snow on frozen ground), the warmth and the wind dry it; the film
		// keeps it in. Grass holds its own.
		if (!Bed.bCovered && A.RainMm > 0.f)
		{
			C.Water += A.RainMm * (1.f - A.Snow * 0.8f) * R.RainWet * Dt;
		}
		const FCrop* Crop = (C.State == ECell::Growing || C.State == ECell::Ripe) ? Data.Crop(C.Crop) : nullptr;
		const float Drink = Crop ? Crop->Thirst : 0.12f;
		const float Dry = Drink * Day * (0.15f + std::max(0.f, T) / 20.f) * (1.f - 0.4f * A.Cloud) * (1.f + A.Wind * 0.03f) * (Bed.bCovered ? 0.6f : 1.f);
		C.Water = Clamp01(C.Water - Dry);
		if (C.State == ECell::Grass)
		{
			C.Water = std::max(C.Water, 0.35f);
			return;
		}
		// weeds come up in any warm dug soil
		if (T > R.WeedBase)
		{
			C.Weeds = Clamp01(C.Weeds + Day / R.WeedDays * Clamp01((T - R.WeedBase) / 12.f));
		}
		if (C.Rich > 0.f)
		{
			C.Rich = std::max(0.f, C.Rich - Day / 14.f);
		}
		if (!Crop)
		{
			return;
		}
		// frost: below the crop's limit it hurts, more the colder (a perennial's roots sleep through all but hard frost)
		const float Limit = Crop->bPerennial ? Crop->FrostKill - 8.f : Crop->FrostKill;
		if (T < Limit)
		{
			C.Hurt += (Limit - T) * R.FrostHurt * Dt;
		}
		// drought: a day and more with dry soil wilts it
		if (C.Water < 0.06f)
		{
			C.DryHours += Dt;
			if (C.DryHours > 36.f)
			{
				C.Hurt += 0.012f * Dt;
			}
		}
		else
		{
			C.DryHours = 0.f;
		}
		if (C.Hurt >= 1.f)
		{
			C.Hurt = 1.f;
			C.State = ECell::Dead;
			return;
		}
		const float WaterK = C.Water >= R.DryBelow ? 1.f : Clamp01((C.Water - 0.05f) / (R.DryBelow - 0.05f));
		const float WeedK = 1.f - 0.6f * C.Weeds;
		const float WarmK = Warmth(*Crop, T);
		// how it is kept, a running mean over the hours it grows: the harvest's quality
		const float WaterCare = Clamp01((C.Water - 0.05f) / 0.15f);
		const float CareNow = std::min(WaterCare, std::min(1.f - 0.7f * std::max(0.f, C.Weeds - 0.25f), 1.f - C.Hurt));
		C.Care = (C.Care * C.CareHours + CareNow * Dt) / (C.CareHours + Dt);
		C.CareHours = std::min(C.CareHours + Dt, 240.f);
		if (C.State == ECell::Growing)
		{
			// the night grows too: Days are of round-the-clock growing at the best warmth
			C.Growth += Day / Crop->Days * WarmK * WaterK * WeedK * (1.f + 0.3f * C.Rich);
			if (C.Growth >= 1.f)
			{
				C.Growth = 1.f;
				C.State = ECell::Ripe;
				C.RipeHours = 0.f;
			}
		}
		else if (C.State == ECell::Ripe)
		{
			// ripe things keep slower in the cold
			C.RipeHours += Dt * (T < 8.f ? 0.5f : 1.f);
			if (C.RipeHours > (Crop->RipeDays + R.SpoilDays) * 24.f)
			{
				C.State = ECell::Spoilt;
			}
		}
	}

	bool FModel::Dig(int Cell)
	{
		if (!Bed.bRented || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if (C.State != ECell::Grass && !(C.State == ECell::Dug && C.Weeds > 0.3f))
		{
			return false;
		}
		const float Water = C.Water;
		C = FCell();
		C.State = ECell::Dug;
		C.Water = Water;
		return true;
	}

	bool FModel::Sow(int Cell, const std::string& SeedItem, double Hours)
	{
		const FCrop* Crop = Data.CropBySeed(SeedItem);
		if (!Bed.bRented || !Crop || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if (C.State != ECell::Dug)
		{
			return false;
		}
		C.State = ECell::Growing;
		C.Crop = Crop->Id;
		C.Growth = Crop->bSeedling ? 0.15f : 0.f;
		C.Weeds = std::min(C.Weeds, 0.1f);
		C.Hurt = 0.f;
		C.Care = 1.f;
		C.CareHours = 0.f;
		C.Picked = 0;
		C.RipeHours = 0.f;
		C.DryHours = 0.f;
		C.SownAt = Hours;
		return true;
	}

	bool FModel::Water(int Cell)
	{
		if (!Bed.bRented || CanLeft <= 0 || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if (C.State == ECell::Grass || C.Water > 0.9f)
		{
			return false;
		}
		C.Water = Clamp01(C.Water + Data.R.CanWater);
		C.DryHours = 0.f;
		--CanLeft;
		return true;
	}

	bool FModel::WaterAll()
	{
		bool bAny = false;
		for (int I = 0; I < (int)Bed.Cells.size() && CanLeft > 0; ++I)
		{
			const FCell& C = Bed.Cells[(size_t)I];
			if ((C.State == ECell::Growing || C.State == ECell::Ripe) && C.Water < 0.6f)
			{
				bAny |= Water(I);
			}
		}
		return bAny;
	}

	bool FModel::Weed(int Cell)
	{
		if (!Bed.bRented || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if (C.State == ECell::Grass || C.Weeds < 0.15f)
		{
			return false;
		}
		C.Weeds = 0.f;
		return true;
	}

	bool FModel::Clear(int Cell)
	{
		if (!Bed.bRented || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if (C.State != ECell::Dead && C.State != ECell::Spoilt)
		{
			return false;
		}
		const float Water = C.Water;
		const float Weeds = C.Weeds;
		C = FCell();
		C.State = ECell::Dug;
		C.Water = Water;
		C.Weeds = Weeds;
		return true;
	}

	int FModel::Quality(const FCell& C) const
	{
		float Q = C.Care;
		const FCrop* Crop = Data.Crop(C.Crop);
		if (Crop && C.State == ECell::Ripe && C.RipeHours > Crop->RipeDays * 24.f)
		{
			// past its best: going over
			Q -= 0.5f * Clamp01((C.RipeHours - Crop->RipeDays * 24.f) / (Data.R.SpoilDays * 24.f));
		}
		return Q > 0.82f ? 3 : (Q > 0.55f ? 2 : 1);
	}

	bool FModel::Pick(int Cell, FHarvest& Out)
	{
		Out = FHarvest();
		if (!Bed.bRented || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		const FCrop* Crop = Data.Crop(C.Crop);
		if (C.State != ECell::Ripe || !Crop)
		{
			return false;
		}
		Out.Item = Crop->Yield;
		Out.Quality = Quality(C);
		Out.Count = std::max(1, (int)std::lround(Crop->YieldCount * (0.45f + 0.55f * Clamp01(C.Care))));
		LastQualities[Out.Item] = Out.Quality;
		++C.Picked;
		if (Crop->Again > 0.f && C.Picked < Crop->Pickings)
		{
			// it comes again: back to most of the way grown
			C.State = ECell::Growing;
			C.Growth = Clamp01(1.f - Crop->Again / Crop->Days);
			C.RipeHours = 0.f;
		}
		else if (Crop->bPerennial)
		{
			// a strawberry bed rests till next year's flowers: back to young plants
			C.State = ECell::Growing;
			C.Growth = 0.4f;
			C.Picked = 0;
			C.RipeHours = 0.f;
		}
		else
		{
			const float Water = C.Water;
			const float Weeds = C.Weeds;
			C = FCell();
			C.State = ECell::Dug;
			C.Water = Water;
			C.Weeds = Weeds;
		}
		return true;
	}

	bool FModel::Compost(int Cell)
	{
		if (!Bed.bRented || Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return false;
		}
		FCell& C = Bed.Cells[(size_t)Cell];
		if ((C.State != ECell::Dug && C.State != ECell::Growing) || C.Rich >= 0.3f)
		{
			return false;
		}
		C.Rich = 1.f;
		return true;
	}

	ENeed FModel::Need(int Cell, float ColdestSoon, float HottestSoon) const
	{
		if (Cell < 0 || Cell >= (int)Bed.Cells.size())
		{
			return ENeed::None;
		}
		const FCell& C = Bed.Cells[(size_t)Cell];
		switch (C.State)
		{
		case ECell::Grass: return ENeed::Dig;
		case ECell::Dug: return C.Weeds > 0.3f ? ENeed::Dig : ENeed::Sow;
		case ECell::Dead:
		case ECell::Spoilt: return ENeed::Clear;
		case ECell::Ripe: return ENeed::Pick;
		case ECell::Growing: break;
		}
		const FCrop* Crop = Data.Crop(C.Crop);
		if (Crop)
		{
			const float Limit = Crop->bPerennial ? Crop->FrostKill - 8.f : Crop->FrostKill;
			if (!Bed.bCovered && ColdestSoon < Limit + 2.f)
			{
				return ENeed::Cover;
			}
			if (Bed.bCovered && HottestSoon + Data.R.CoverNight + Data.R.CoverSun * 0.7f > Crop->Hot + 4.f)
			{
				return ENeed::Uncover;
			}
		}
		if (C.Water < Data.R.DryBelow + 0.05f)
		{
			return ENeed::Water;
		}
		if (C.Weeds > 0.35f)
		{
			return ENeed::Weed;
		}
		return ENeed::Wait;
	}

	void FModel::Save(std::map<std::string, std::string>& Out) const
	{
		Out["now"] = Num(Now);
		Out["rented"] = Bed.bRented ? "1" : "0";
		Out["covered"] = Bed.bCovered ? "1" : "0";
		Out["can"] = std::to_string(CanLeft);
		Out["jam"] = std::to_string(JamQuality);
		Out["jam_at"] = Num(JamAt);
		Out["jar"] = std::to_string(JamJarQuality);
		for (const auto& KV : LastQualities)
		{
			Out["q." + KV.first] = std::to_string(KV.second);
		}
		for (size_t I = 0; I < Bed.Cells.size(); ++I)
		{
			const FCell& C = Bed.Cells[I];
			char B[400];
			std::snprintf(B, sizeof(B), "%s|%s|%.4f|%.4f|%.4f|%.4f|%.4f|%.2f|%.2f|%d|%.2f|%.2f|%.3f", CellWord(C.State), C.Crop.c_str(),
				C.Growth, C.Water, C.Weeds, C.Hurt, C.Care, C.CareHours, C.RipeHours, C.Picked, C.SownAt, C.DryHours, C.Rich);
			Out["c." + std::to_string(I)] = B;
		}
	}

	void FModel::LoadState(const std::map<std::string, std::string>& In)
	{
		Init();
		auto Get = [&In](const char* K) -> std::string
		{
			const auto It = In.find(K);
			return It == In.end() ? std::string() : It->second;
		};
		if (!Get("now").empty())
		{
			Now = std::atof(Get("now").c_str());
		}
		Bed.bRented = Get("rented") == "1";
		Bed.bCovered = Get("covered") == "1";
		CanLeft = std::atoi(Get("can").c_str());
		JamQuality = std::atoi(Get("jam").c_str());
		JamAt = std::atof(Get("jam_at").c_str());
		JamJarQuality = std::atoi(Get("jar").c_str());
		for (const auto& KV : In)
		{
			if (KV.first.compare(0, 2, "q.") == 0)
			{
				LastQualities[KV.first.substr(2)] = std::atoi(KV.second.c_str());
			}
		}
		for (size_t I = 0; I < Bed.Cells.size(); ++I)
		{
			const std::string V = Get(("c." + std::to_string(I)).c_str());
			if (V.empty())
			{
				continue;
			}
			std::vector<std::string> P;
			size_t S = 0;
			for (size_t E = V.find('|'); ; E = V.find('|', S))
			{
				P.push_back(V.substr(S, E == std::string::npos ? std::string::npos : E - S));
				if (E == std::string::npos)
				{
					break;
				}
				S = E + 1;
			}
			if (P.size() < 12)
			{
				continue;
			}
			FCell& C = Bed.Cells[I];
			static const ECell States[] = { ECell::Grass, ECell::Dug, ECell::Growing, ECell::Ripe, ECell::Spoilt, ECell::Dead };
			for (ECell St : States)
			{
				if (P[0] == CellWord(St))
				{
					C.State = St;
				}
			}
			C.Crop = P[1];
			C.Growth = (float)std::atof(P[2].c_str());
			C.Water = (float)std::atof(P[3].c_str());
			C.Weeds = (float)std::atof(P[4].c_str());
			C.Hurt = (float)std::atof(P[5].c_str());
			C.Care = (float)std::atof(P[6].c_str());
			C.CareHours = (float)std::atof(P[7].c_str());
			C.RipeHours = (float)std::atof(P[8].c_str());
			C.Picked = std::atoi(P[9].c_str());
			C.SownAt = std::atof(P[10].c_str());
			C.DryHours = (float)std::atof(P[11].c_str());
			C.Rich = P.size() > 12 ? (float)std::atof(P[12].c_str()) : 0.f;
			if ((C.State == ECell::Growing || C.State == ECell::Ripe || C.State == ECell::Spoilt || C.State == ECell::Dead) && !Data.Crop(C.Crop))
			{
				C = FCell();
				C.State = ECell::Dug;   // a crop the data no longer has
			}
		}
	}

	float NeighbourGrowth(const std::string& Crop, float D)
	{
		// sown (or planted) day, ripe day, the day the cell is bare again; a second sowing for radish
		struct FPlan { const char* Crop; float Sow, Ripe, Bare; };
		static const FPlan Plans[] = {
			{ "radish", 110.f, 135.f, 150.f }, { "radish", 228.f, 252.f, 275.f },
			{ "dill", 118.f, 165.f, 285.f },
			{ "potato", 125.f, 215.f, 245.f },
			{ "tomato", 150.f, 212.f, 272.f },
			{ "sunflower", 130.f, 238.f, 285.f },
		};
		if (Crop == "strawberry")
		{
			// a perennial: young leaves from April, flowers in May, ripe in late June to July, leaves till the frosts
			if (D < 95.f || D > 315.f)
			{
				return 0.2f;
			}
			if (D < 172.f)
			{
				return Lerp(0.3f, 1.f, (D - 95.f) / 77.f);
			}
			return D < 205.f ? 1.f : 0.7f;
		}
		for (const FPlan& P : Plans)
		{
			if (Crop == P.Crop && D >= P.Sow && D < P.Bare)
			{
				return D >= P.Ripe ? 1.f : Clamp01((D - P.Sow) / (P.Ripe - P.Sow));
			}
		}
		return -1.f;
	}

	int Season(float D)
	{
		return D >= 60.f && D < 152.f ? 0 : (D >= 152.f && D < 244.f ? 1 : (D >= 244.f && D < 335.f ? 2 : 3));
	}
}
