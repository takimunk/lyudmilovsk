#include "PhotoContestModel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace PhotoContest
{
	namespace ContestDetail
	{
		float Clamp01(float V)
		{
			return V != V ? 0.f : std::min(1.f, std::max(0.f, V));
		}

		bool StartsWith(const std::string& Text, const char* Prefix)
		{
			return Text.rfind(Prefix, 0) == 0;
		}

		uint32_t Mix(uint32_t X)
		{
			X ^= X >> 16;
			X *= 0x7feb352dU;
			X ^= X >> 15;
			X *= 0x846ca68bU;
			X ^= X >> 16;
			return X;
		}

		/** A whole number of at most 9 digits with an optional minus; false on anything else. */
		bool ReadInt(const std::string& Text, int& Out)
		{
			size_t I = 0;
			bool bMinus = false;
			if (!Text.empty() && Text[0] == '-')
			{
				bMinus = true;
				I = 1;
			}
			if (I >= Text.size() || Text.size() - I > 9)
			{
				return false;
			}
			int V = 0;
			for (; I < Text.size(); ++I)
			{
				if (Text[I] < '0' || Text[I] > '9')
				{
					return false;
				}
				V = V * 10 + (Text[I] - '0');
			}
			Out = bMinus ? -V : V;
			return true;
		}

		bool PlainId(const std::string& Text)
		{
			for (const char C : Text)
			{
				const bool bOk = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-' || C == '.';
				if (!bOk)
				{
					return false;
				}
			}
			return true;
		}

		std::vector<std::string> Split(const std::string& Text, char Sep)
		{
			std::vector<std::string> Parts;
			size_t From = 0;
			for (;;)
			{
				const size_t At = Text.find(Sep, From);
				if (At == std::string::npos)
				{
					Parts.push_back(Text.substr(From));
					return Parts;
				}
				Parts.push_back(Text.substr(From, At - From));
				From = At + 1;
			}
		}
	}

	const char* ThemeId(int Theme)
	{
		static const char* const Ids[Themes] = { "sea", "birds", "work", "festival" };
		return Ids[std::min(Themes - 1, std::max(0, Theme))];
	}

	int ThemeOf(int Month)
	{
		return ((std::min(12, std::max(1, Month)) - 1) % Themes);
	}

	int ContestOf(int Year, int Month)
	{
		return Year * 12 + std::min(12, std::max(1, Month)) - 1;
	}

	int YearOf(int Contest)
	{
		return Contest / 12;
	}

	int MonthOf(int Contest)
	{
		return Contest % 12 + 1;
	}

	FMarks Judge(const PhotoModel::FShot& Shot, int Theme)
	{
		using namespace ContestDetail;
		// what the frame holds
		float Sea = -1.f, Work = -1.f, Festival = -1.f, Bird = -1.f, Fish = -1.f;
		bool bSunset = false, bAurora = false, bSun = false;
		int SunDeg = 0, People = 0;
		std::vector<std::string> Species;
		for (const PhotoModel::FSubject& S : Shot.Subjects)
		{
			const float Sharp = Clamp01(S.Sharp);
			if (S.Tag == "sea")
			{
				Sea = std::max(Sea, Sharp);
			}
			else if (S.Tag == "work")
			{
				Work = std::max(Work, Sharp);
			}
			else if (S.Tag == "fish")
			{
				Fish = std::max(Fish, Sharp);
			}
			else if (S.Tag == "aurora")
			{
				bAurora = true;
			}
			else if (StartsWith(S.Tag, "festival:") && S.Tag.size() > 9)
			{
				Festival = std::max(Festival, Sharp);
			}
			else if (StartsWith(S.Tag, "sunset:"))
			{
				bSunset = true;
			}
			else if (StartsWith(S.Tag, "person:") && S.Tag.size() > 7)
			{
				People += Sharp >= PhotoModel::SharpEnough ? 1 : 0;
			}
			else if (StartsWith(S.Tag, "bird:") && S.Tag.size() > 5)
			{
				Bird = std::max(Bird, Sharp);
				if (Sharp >= PhotoModel::SharpEnough && std::find(Species.begin(), Species.end(), S.Tag) == Species.end())
				{
					Species.push_back(S.Tag);
				}
			}
			else if (StartsWith(S.Tag, "sun:"))
			{
				bSun = ReadInt(S.Tag.substr(4), SunDeg) || bSun;
			}
		}

		FMarks Marks;
		float Own = -1.f, More = 0.f;
		switch (std::min(Themes - 1, std::max(0, Theme)))
		{
		case 0:
			Own = Sea;
			More = (bSunset ? 5.f : 0.f) + (Bird >= PhotoModel::SharpEnough ? 5.f : 0.f) + (Fish >= PhotoModel::SharpEnough ? 5.f : 0.f);
			break;
		case 1:
			Own = Bird;
			More = Species.size() >= 2 ? 10.f : 0.f;
			break;
		case 2:
			Own = Work;
			More = 5.f * float(std::max(0, People - 1));
			break;
		default:
			Own = Festival;
			More = 5.f * float(People);
			break;
		}
		const float FrameSharp = Clamp01(Shot.Sharp);
		if (Own >= 0.f)
		{
			Marks.Subject = 30.f + 10.f * Own + std::min(10.f, More);
			Marks.Sharp = 25.f * Own;
		}
		else
		{
			Marks.Subject = 5.f;
			Marks.Sharp = 25.f * FrameSharp;
		}

		if (!bSun)
		{
			Marks.Light = 12.f;
		}
		else if (SunDeg < -6)
		{
			Marks.Light = bAurora ? 22.f : 6.f;
		}
		else if (SunDeg < 0)
		{
			Marks.Light = 18.f;
		}
		else if (SunDeg <= 10)
		{
			Marks.Light = 25.f;
		}
		else if (SunDeg <= 25)
		{
			Marks.Light = 25.f - 10.f * float(SunDeg - 10) / 15.f;
		}
		else
		{
			Marks.Light = 15.f - 5.f * float(std::min(SunDeg, 60) - 25) / 35.f;
		}
		if (bSunset)
		{
			Marks.Light = std::min(25.f, Marks.Light + 3.f);
		}
		return Marks;
	}

	float RivalScore(int Contest, int Index)
	{
		const uint32_t H = ContestDetail::Mix(uint32_t(Contest) * 2654435761U + uint32_t(Index) * 40503U + 17U);
		return 40.f + float(H % 4501U) / 100.f;
	}

	int PlaceOf(int Contest, float Score)
	{
		int Place = 1;
		for (int I = 0; I < Rivals; ++I)
		{
			Place += RivalScore(Contest, I) > Score ? 1 : 0;
		}
		return Place;
	}

	int PrizeOf(int Place)
	{
		return Place == 1 ? 150 : (Place == 2 ? 80 : (Place == 3 ? 40 : 0));
	}

	bool FState::CanEnter(int Year, int Month, int Day) const
	{
		// (the entries stay in the order of their months, as Restore asks: a clock set back enters nothing)
		return Day >= 1 && Day <= LastEntryDay && (All.empty() || All.back().Contest < ContestOf(Year, Month));
	}

	bool FState::Enter(int Year, int Month, int Day, const PhotoModel::FShot& Shot)
	{
		if (!CanEnter(Year, Month, Day))
		{
			return false;
		}
		FEntry Entry;
		Entry.Contest = ContestOf(Year, Month);
		Entry.Roll = Shot.Roll;
		Entry.Frame = Shot.Frame;
		// kept to one decimal, as the save keeps it: the place never changes over a save
		Entry.Score = std::round(Judge(Shot, ThemeOf(Month)).Sum() * 10.f) / 10.f;
		Entry.Place = ContestDetail::PlainId(Shot.Place) ? Shot.Place : std::string();
		All.push_back(Entry);
		return true;
	}

	const FEntry* FState::Find(int Contest) const
	{
		for (const FEntry& Entry : All)
		{
			if (Entry.Contest == Contest)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	bool FState::IsJudged(const FEntry& Entry, int Year, int Month, int Day)
	{
		const int Now = ContestOf(Year, Month);
		return Now > Entry.Contest || (Now == Entry.Contest && Day >= JudgeDay);
	}

	const FEntry* FState::Untold(int Year, int Month, int Day) const
	{
		for (const FEntry& Entry : All)
		{
			if (!Entry.bTold && IsJudged(Entry, Year, Month, Day))
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	void FState::Told(int Contest)
	{
		for (FEntry& Entry : All)
		{
			if (Entry.Contest == Contest)
			{
				Entry.bTold = true;
			}
		}
	}

	std::vector<const FEntry*> FState::Wall(int Year, int Month, int Day) const
	{
		std::vector<const FEntry*> Out;
		for (const FEntry& Entry : All)
		{
			if (IsJudged(Entry, Year, Month, Day) && PlaceOf(Entry.Contest, Entry.Score) <= PrizePlaces)
			{
				Out.push_back(&Entry);
			}
		}
		return Out;
	}

	std::string FState::Save() const
	{
		std::string Line = "c1";
		for (const FEntry& Entry : All)
		{
			const int Tenths = int(std::lround(double(ContestDetail::Clamp01(Entry.Score / 100.f)) * 1000.0));
			Line += "|e=" + std::to_string(Entry.Contest) + ":" + std::to_string(Entry.Roll) + ":" + std::to_string(Entry.Frame) + ":"
				+ std::to_string(Tenths) + ":" + (Entry.bTold ? "1" : "0") + ":" + Entry.Place;
		}
		return Line;
	}

	bool FState::Restore(const std::string& Line)
	{
		All.clear();
		const std::vector<std::string> Parts = ContestDetail::Split(Line, '|');
		if (Parts.empty() || Parts[0] != "c1")
		{
			return false;
		}
		std::vector<FEntry> Read;
		for (size_t I = 1; I < Parts.size(); ++I)
		{
			const size_t Eq = Parts[I].find('=');
			if (Eq == std::string::npos)
			{
				return false;
			}
			if (Parts[I].compare(0, Eq, "e") != 0)
			{
				// a key of a later version
				continue;
			}
			const std::vector<std::string> F = ContestDetail::Split(Parts[I].substr(Eq + 1), ':');
			FEntry Entry;
			int Tenths = 0, ToldFlag = 0;
			if (F.size() != 6 || !ContestDetail::ReadInt(F[0], Entry.Contest) || !ContestDetail::ReadInt(F[1], Entry.Roll)
				|| !ContestDetail::ReadInt(F[2], Entry.Frame) || !ContestDetail::ReadInt(F[3], Tenths) || !ContestDetail::ReadInt(F[4], ToldFlag)
				|| Entry.Contest < 0 || Entry.Roll < 1 || Entry.Frame < 1 || Entry.Frame > PhotoModel::FramesPerFilm
				|| Tenths < 0 || Tenths > 1000 || ToldFlag < 0 || ToldFlag > 1 || !ContestDetail::PlainId(F[5]))
			{
				return false;
			}
			for (const FEntry& Old : Read)
			{
				if (Old.Contest >= Entry.Contest)
				{
					return false;
				}
			}
			Entry.Score = float(Tenths) / 10.f;
			Entry.bTold = ToldFlag == 1;
			Entry.Place = F[5];
			Read.push_back(Entry);
		}
		All = Read;
		return true;
	}
}
