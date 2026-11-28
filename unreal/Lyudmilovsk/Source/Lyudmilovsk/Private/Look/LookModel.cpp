#include "LookModel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace LyudLook
{
	const std::vector<FCut>& Cuts()
	{
		static const std::vector<FCut> All = {
			{ "shaggy", 0 },     // his own, grown out
			{ "crop", 30 },      // под машинку
			{ "short", 21 },     // полубокс
			{ "parted", 14 },    // с пробором: neat for a fortnight
		};
		return All;
	}

	int FindCut(const std::string& Id)
	{
		const std::vector<FCut>& All = Cuts();
		for (size_t I = 0; I < All.size(); ++I)
		{
			if (All[I].Id == Id)
			{
				return static_cast<int>(I);
			}
		}
		return -1;
	}

	std::vector<std::string> Offered()
	{
		std::vector<std::string> Out;
		for (const FCut& C : Cuts())
		{
			if (C.LastsDays > 0)
			{
				Out.push_back(C.Id);
			}
		}
		return Out;
	}

	const std::vector<FWear>& Clothes()
	{
		static const std::vector<FWear> All = {
			//  item            slot     price warm  rain   own    tint                      rough
			{ "overshirt",    "outer",   0,  3.0, false, true,  { 1.00f, 1.00f, 1.00f }, 0.85f },   // the camel corduroy he came in
			{ "wind_jacket",  "outer",  90,  6.0, false, false, { 0.16f, 0.30f, 0.24f }, 0.55f },   // a bottle-green windbreaker
			{ "raincoat",     "outer", 120,  4.0, true,  false, { 0.86f, 0.66f, 0.08f }, 0.28f },   // a yellow oilskin
			{ "winter_coat",  "outer", 260, 16.0, false, false, { 0.07f, 0.09f, 0.16f }, 0.95f },   // navy wool
			{ "cap",          "head",   45,  2.0, false, false, { 0.30f, 0.28f, 0.24f }, 0.90f },   // a grey flat cap
		};
		return All;
	}

	const FWear* FindWear(const std::string& Item)
	{
		for (const FWear& W : Clothes())
		{
			if (W.Item == Item)
			{
				return &W;
			}
		}
		return nullptr;
	}

	EFit Fit(const std::string& Outer, bool bCap, double TempC, bool bRain)
	{
		const FWear* W = FindWear(Outer);
		const double Warm = (W ? W->Warm : 0.0) + (bCap ? 2.0 : 0.0);
		if (bRain && !(W && W->bRain))
		{
			return EFit::Wet;
		}
		// the hoodie alone is fine down to twelve degrees; every degree of clothes takes one off
		if (TempC + Warm < 12.0)
		{
			return EFit::Cold;
		}
		if (Warm >= 12.0 && TempC > 14.0)
		{
			return EFit::Hot;
		}
		return EFit::Fine;
	}

	const char* FitId(EFit Fit)
	{
		switch (Fit)
		{
		case EFit::Wet: return "wet";
		case EFit::Cold: return "cold";
		case EFit::Hot: return "hot";
		default: return "";
		}
	}

	std::string CutNow(const FState& State, double Day)
	{
		const int C = FindCut(State.Cut);
		if (C < 0 || Cuts()[static_cast<size_t>(C)].LastsDays <= 0 || Day - State.CutDay >= Cuts()[static_cast<size_t>(C)].LastsDays)
		{
			return "shaggy";
		}
		return State.Cut;
	}

	bool GetCut(FState& State, const std::string& Cut, double Day)
	{
		const int C = FindCut(Cut);
		if (C < 0 || Cuts()[static_cast<size_t>(C)].LastsDays <= 0)
		{
			return false;
		}
		const bool bNew = CutNow(State, Day) != Cut;
		State.Cut = Cut;
		State.CutDay = Day;
		if (bNew)
		{
			State.Serial++;
			State.Told.clear();
			State.LastToldHour = -1.0e9;
		}
		return true;
	}

	bool WillComment(const FState& State, int Person, double Day, double Hour)
	{
		if (Person < 0 || State.Serial <= 0 || CutNow(State, Day) == "shaggy" || Day - State.CutDay > FreshDays)
		{
			return false;
		}
		if (static_cast<int>(State.Told.size()) >= MaxComments || Hour - State.LastToldHour < CommentGapHours)
		{
			return false;
		}
		return std::find(State.Told.begin(), State.Told.end(), Person) == State.Told.end();
	}

	void NoteComment(FState& State, int Person, double Hour)
	{
		State.Told.push_back(Person);
		State.LastToldHour = Hour;
	}

	int CommentLine(int Person, int Serial, int Lines)
	{
		if (Lines <= 1)
		{
			return 1;
		}
		uint32_t H = static_cast<uint32_t>(Person) * 2654435761u + static_cast<uint32_t>(Serial) * 40503u;
		H ^= H >> 15;
		return 1 + static_cast<int>(H % static_cast<uint32_t>(Lines));
	}

	bool WillRemark(const FState& State, EFit Fit, double Day, double Hour)
	{
		return Fit != EFit::Fine && static_cast<int>(std::floor(Day)) != State.RemarkDay && Hour - State.LastToldHour >= CommentGapHours;
	}

	void NoteRemark(FState& State, double Day, double Hour)
	{
		State.RemarkDay = static_cast<int>(std::floor(Day));
		State.LastToldHour = Hour;
	}

	std::string Save(const FState& State)
	{
		std::ostringstream Out;
		Out.precision(10);
		Out << State.Cut << '|' << State.CutDay << '|' << State.Serial << '|' << State.LastToldHour << '|' << State.RemarkDay << '|';
		for (size_t I = 0; I < State.Told.size(); ++I)
		{
			Out << (I ? "," : "") << State.Told[I];
		}
		return Out.str();
	}

	bool Load(FState& State, const std::string& Text)
	{
		std::vector<std::string> Parts;
		std::string Part;
		std::istringstream In(Text);
		while (std::getline(In, Part, '|'))
		{
			Parts.push_back(Part);
		}
		if (Parts.size() < 5 || FindCut(Parts[0]) < 0)
		{
			return false;
		}
		FState S;
		S.Cut = Parts[0];
		S.CutDay = std::atof(Parts[1].c_str());
		S.Serial = std::atoi(Parts[2].c_str());
		S.LastToldHour = std::atof(Parts[3].c_str());
		S.RemarkDay = std::atoi(Parts[4].c_str());
		if (Parts.size() > 5)
		{
			std::istringstream Told(Parts[5]);
			while (std::getline(Told, Part, ','))
			{
				if (!Part.empty())
				{
					S.Told.push_back(std::atoi(Part.c_str()));
				}
			}
		}
		State = S;
		return true;
	}
}
