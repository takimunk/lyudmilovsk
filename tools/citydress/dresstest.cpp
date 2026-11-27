// The game's dressing condition (Private/City/CityDressModel.h) against the Blender side's Python (citykit.dress_visible)
// on the cases tools/citydress/gen_cases.py wrote. Run through tools/citydress/run.sh.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityDressModel.h"

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: dresstest CASES\n";
		return 2;
	}
	std::ifstream In(argv[1]);
	std::string Line;
	int N = 0, Bad = 0, Shown = 0;
	while (std::getline(In, Line))
	{
		std::istringstream S(Line);
		LyudCity::FDressWhen C;
		int Wa, Wn, Mo, Wd, Per, From, To, Open;
		std::string Fest, Bar;
		float Hf, Ht, Ch, Keep;
		S >> Wa >> Wn >> Mo >> Wd >> Hf >> Ht >> Fest >> Ch >> Per >> From >> To >> Open >> Keep >> Bar;
		C.WeatherAny = (uint8_t)Wa;
		C.WeatherNone = (uint8_t)Wn;
		C.Months = (uint16_t)Mo;
		C.Weekdays = (uint8_t)Wd;
		C.HourFrom = Hf;
		C.HourTo = Ht;
		C.Festival = Fest == "-" ? std::string() : Fest;
		C.Chance = Ch;
		C.PhasePeriod = Per;
		C.PhaseFrom = From;
		C.PhaseTo = To;
		C.Open = Open;
		C.VisitKeep = Keep;
		LyudCity::FWhen W;
		int Fest01, Wx, Op, NF;
		float Ago;
		S >> W.Day >> W.Hour >> W.Weekday >> W.MonthDay >> W.Since >> Fest01 >> Wx >> Op >> Ago >> NF;
		W.bFestival = Fest01 != 0;
		W.Weather = (uint8_t)Wx;
		for (int I = 0; I < NF; ++I)
		{
			std::string F;
			S >> F;
			W.Festivals.push_back(F);
		}
		std::string Plot, Id;
		int Expected;
		S >> Bar >> Plot >> Id >> Expected;
		const bool Got = LyudCity::DressVisible(C, W, Plot, Id, Op != 0, Ago);
		++N;
		Shown += Got ? 1 : 0;
		if (Got != (Expected != 0))
		{
			if (++Bad <= 10)
			{
				std::cout << "MISMATCH python " << Expected << " c++ " << Got << ": " << Line << "\n";
			}
		}
	}
	std::cout << "CITYDRESS " << N << " cases, " << Shown << " shown, " << Bad << " mismatches\n";
	return (Bad == 0 && N > 0) ? 0 : 1;
}
