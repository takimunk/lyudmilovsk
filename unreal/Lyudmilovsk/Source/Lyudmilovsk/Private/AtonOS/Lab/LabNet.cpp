#include "LabNet.h"

#include "../../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace AtonLab
{
	namespace NetDetail
	{
		/** The hour the hero woke at the club's computer: what the lab's clocks show when they are switched on. */
		const char* const DutyPassword = "0314";

		unsigned Mix(unsigned A, unsigned B)
		{
			unsigned H = A * 2654435761u ^ (B + 0x9E3779B9u + (A << 6) + (A >> 2));
			H ^= H >> 15;
			H *= 2246822519u;
			H ^= H >> 13;
			return H;
		}

		std::vector<std::string> Split(const std::string& Text, char Sep)
		{
			std::vector<std::string> Out;
			size_t From = 0;
			while (From <= Text.size())
			{
				const size_t To = Text.find(Sep, From);
				if (To == std::string::npos)
				{
					Out.push_back(Text.substr(From));
					break;
				}
				Out.push_back(Text.substr(From, To - From));
				From = To + 1;
			}
			return Out;
		}

		std::string Join(const std::set<std::string>& Items)
		{
			std::string Out;
			for (const std::string& Item : Items)
			{
				if (!Out.empty())
				{
					Out += ',';
				}
				Out += Item;
			}
			return Out;
		}

		/** Ids only: letters, digits, underscore, dot, dash (so a hand-edited save cannot break the line). */
		bool IsId(const std::string& Text)
		{
			if (Text.empty() || Text.size() > 64)
			{
				return false;
			}
			for (const char C : Text)
			{
				const bool bOk = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '.' || C == '-';
				if (!bOk)
				{
					return false;
				}
			}
			return true;
		}

		bool ToInt(const std::string& Text, int& Out)
		{
			if (Text.empty() || Text.size() > 9)
			{
				return false;
			}
			size_t I = Text[0] == '-' ? 1 : 0;
			if (I == Text.size())
			{
				return false;
			}
			for (; I < Text.size(); ++I)
			{
				if (Text[I] < '0' || Text[I] > '9')
				{
					return false;
				}
			}
			Out = std::atoi(Text.c_str());
			return true;
		}
	}

	std::string PasswordDigits(const std::string& Utf8Typed)
	{
		std::string Out;
		const size_t N = Utf8Typed.size();
		for (size_t I = 0; I < N; ++I)
		{
			const unsigned char C = (unsigned char)Utf8Typed[I];
			if (C >= '0' && C <= '9')
			{
				Out += (char)C;
			}
			// U+FF10..U+FF19, the full-width digits: EF BC 90 .. EF BC 99
			else if (C == 0xEF && I + 2 < N && (unsigned char)Utf8Typed[I + 1] == 0xBC)
			{
				const unsigned char D = (unsigned char)Utf8Typed[I + 2];
				if (D >= 0x90 && D <= 0x99)
				{
					Out += (char)('0' + (D - 0x90));
					I += 2;
				}
			}
		}
		return Out;
	}

	bool PasswordOk(const std::string& Utf8Typed)
	{
		return PasswordDigits(Utf8Typed) == NetDetail::DutyPassword;
	}

	bool LoadLetters(const std::string& JsonText, std::vector<FLabLetter>& Out, std::string* Error)
	{
		LyudJson::FValue Root;
		std::string ParseError;
		LyudJson::FParser Parser(JsonText);
		if (!Parser.Parse(Root, ParseError))
		{
			if (Error)
			{
				*Error = ParseError;
			}
			return false;
		}
		const LyudJson::FValue& List = Root["letters"];
		if (List.Type != LyudJson::FValue::Array)
		{
			if (Error)
			{
				*Error = "lab mail: no \"letters\" list";
			}
			return false;
		}
		std::vector<FLabLetter> Letters;
		std::set<std::string> Seen;
		for (const LyudJson::FValue& Item : List.Items)
		{
			FLabLetter Letter;
			Letter.Id = Item["id"].S();
			Letter.Box = Item["box"].S();
			Letter.After = Item["after"].S();
			const bool bBox = Letter.Box == "guest" || Letter.Box == "staff";
			if (!NetDetail::IsId(Letter.Id) || !bBox || Seen.count(Letter.Id) > 0 || (!Letter.After.empty() && Seen.count(Letter.After) == 0))
			{
				if (Error)
				{
					*Error = "lab mail: bad letter \"" + Letter.Id + "\" (id, box, a repeated id, or \"after\" names no earlier letter)";
				}
				return false;
			}
			Seen.insert(Letter.Id);
			Letters.push_back(Letter);
		}
		Out = Letters;
		return true;
	}

	int ScopeSlotHour(int Slot)
	{
		static const int Hours[ScopeSlots] = { 22, 0, 2, 4 };
		return Hours[((Slot % ScopeSlots) + ScopeSlots) % ScopeSlots];
	}

	double ScopeSlotAt(int Night, int Slot)
	{
		// night N runs from noon of day N to noon of day N + 1; its slots are 10, 12, 14 and 16 hours after that noon
		return Night * 24.0 + 12.0 + 10.0 + 2.0 * Slot;
	}

	int ScopeNightOf(double HoursSinceFirstMidnight)
	{
		return (int)std::floor((HoursSinceFirstMidnight - 12.0) / 24.0);
	}

	const char* ScopeTargetId(int Target)
	{
		static const char* const Ids[ScopeTargets] = { "moon", "evening_star", "giant", "rings", "double", "field14" };
		return Ids[((Target % ScopeTargets) + ScopeTargets) % ScopeTargets];
	}

	std::vector<FScopeSlot> ScopeNight(int Night)
	{
		std::vector<FScopeSlot> Out((size_t)ScopeSlots);
		const unsigned Seed = NetDetail::Mix(0x1AB2u, (unsigned)(Night + 100000));
		// four different targets out of six
		int Order[ScopeTargets];
		for (int I = 0; I < ScopeTargets; ++I)
		{
			Order[I] = I;
		}
		for (int I = ScopeTargets - 1; I > 0; --I)
		{
			const int J = (int)(NetDetail::Mix(Seed, (unsigned)I) % (unsigned)(I + 1));
			std::swap(Order[I], Order[J]);
		}
		// one slot is always free, a second one on about every third night
		const int Free1 = (int)(NetDetail::Mix(Seed, 77u) % (unsigned)ScopeSlots);
		const int Free2 = NetDetail::Mix(Seed, 78u) % 3u == 0u ? (int)(NetDetail::Mix(Seed, 79u) % (unsigned)ScopeSlots) : -1;
		for (int S = 0; S < ScopeSlots; ++S)
		{
			Out[(size_t)S].Target = Order[S];
			Out[(size_t)S].Owner = (S == Free1 || S == Free2) ? -1 : (int)(NetDetail::Mix(Seed, 200u + (unsigned)S) % (unsigned)ScopeStaff);
		}
		return Out;
	}

	int Compass8(float FromDeg)
	{
		const float D = FromDeg - 360.f * std::floor(FromDeg / 360.f);
		return ((int)std::floor((D + 22.5f) / 45.f)) % 8;
	}

	int Oktas(float Cover)
	{
		const float C = Cover < 0.f ? 0.f : (Cover > 1.f ? 1.f : Cover);
		int N = (int)std::floor(C * 8.f + 0.5f);
		// 0 and 8 only for a sky that really is clear or shut
		if (N == 0 && C > 0.02f)
		{
			N = 1;
		}
		if (N == 8 && C < 0.98f)
		{
			N = 7;
		}
		return N;
	}

	int Beaufort(float SpeedMs)
	{
		static const float Tops[12] = { 0.3f, 1.6f, 3.4f, 5.5f, 8.0f, 10.8f, 13.9f, 17.2f, 20.8f, 24.5f, 28.5f, 32.7f };
		int B = 0;
		while (B < 12 && SpeedMs >= Tops[B])
		{
			++B;
		}
		return B;
	}

	float SolarFlux(float SunAltitudeDeg, float CloudCover)
	{
		if (SunAltitudeDeg <= 0.f)
		{
			return 0.f;
		}
		const float C = CloudCover < 0.f ? 0.f : (CloudCover > 1.f ? 1.f : CloudCover);
		const float SinAlt = std::sin(SunAltitudeDeg * 3.14159265f / 180.f);
		// clear-sky global radiation (Haurwitz), cut by the cloud (Kasten and Czeplak)
		const float Clear = 1098.f * SinAlt * std::exp(-0.057f / (SinAlt > 0.01f ? SinAlt : 0.01f));
		return Clear * (1.f - 0.75f * std::pow(C, 3.4f));
	}

	std::vector<const FLabLetter*> FLabState::Visible(const std::vector<FLabLetter>& Letters, const std::string& Box) const
	{
		std::vector<const FLabLetter*> Out;
		if (Box == "staff" && !bStaff)
		{
			return Out;
		}
		for (const FLabLetter& Letter : Letters)
		{
			if (Letter.Box == Box && (Letter.After.empty() || Read.count(Letter.After) > 0))
			{
				Out.push_back(&Letter);
			}
		}
		return Out;
	}

	int FLabState::Unread(const std::vector<FLabLetter>& Letters, const std::string& Box) const
	{
		int N = 0;
		for (const FLabLetter* Letter : Visible(Letters, Box))
		{
			N += Read.count(Letter->Id) == 0 ? 1 : 0;
		}
		return N;
	}

	bool FLabState::HasResult(int Night) const
	{
		for (const FScopeResult& R : Results)
		{
			if (R.Night == Night)
			{
				return true;
			}
		}
		return false;
	}

	bool FLabState::Book(int Night, int Slot, double NowHours)
	{
		if (!bStaff || Slot < 0 || Slot >= ScopeSlots || Bookings.count(Night) > 0 || ScopeSlotAt(Night, Slot) <= NowHours)
		{
			return false;
		}
		if (ScopeNight(Night)[(size_t)Slot].Owner != -1)
		{
			return false;
		}
		Bookings[Night] = Slot;
		return true;
	}

	bool FLabState::Unbook(int Night, double NowHours)
	{
		const std::map<int, int>::iterator It = Bookings.find(Night);
		if (It == Bookings.end() || ScopeSlotAt(Night, It->second) <= NowHours)
		{
			return false;
		}
		Bookings.erase(It);
		return true;
	}

	std::string FLabState::Save() const
	{
		std::string Out = "v1";
		Out += "|staff=" + std::to_string(bStaff ? 1 : 0);
		Out += "|fail=" + std::to_string(FailedLogins);
		Out += "|read=" + NetDetail::Join(Read);
		Out += "|end=" + NetDetail::Join(Endings);
		Out += "|chess=";
		for (int L = 0; L < 3; ++L)
		{
			Out += (L ? "," : "") + std::to_string(ChessWon[L]) + "," + std::to_string(ChessDrawn[L]) + "," + std::to_string(ChessLost[L]);
		}
		Out += "|lvl=" + std::to_string(ChessLevel);
		Out += "|book=";
		bool bFirst = true;
		for (const std::pair<const int, int>& B : Bookings)
		{
			Out += (bFirst ? "" : ",") + std::to_string(B.first) + ":" + std::to_string(B.second);
			bFirst = false;
		}
		Out += "|res=";
		bFirst = true;
		for (const FScopeResult& R : Results)
		{
			Out += (bFirst ? "" : ",") + std::to_string(R.Night) + ":" + std::to_string(R.Slot) + ":" + std::to_string(R.Target) + ":" + (R.bClear ? "1" : "0");
			bFirst = false;
		}
		// the two games' own lines last: they never hold a '|'
		Out += "|cs=" + ChessSave;
		Out += "|as=" + AdvSave;
		return Out;
	}

	bool FLabState::Restore(const std::string& Line)
	{
		const std::vector<std::string> Parts = NetDetail::Split(Line, '|');
		if (Parts.empty() || Parts[0] != "v1")
		{
			return false;
		}
		FLabState Next;
		for (size_t P = 1; P < Parts.size(); ++P)
		{
			const size_t Eq = Parts[P].find('=');
			if (Eq == std::string::npos)
			{
				return false;
			}
			const std::string Key = Parts[P].substr(0, Eq), Value = Parts[P].substr(Eq + 1);
			int N = 0;
			if (Key == "staff")
			{
				Next.bStaff = Value == "1";
			}
			else if (Key == "fail")
			{
				if (!NetDetail::ToInt(Value, N))
				{
					return false;
				}
				Next.FailedLogins = N < 0 ? 0 : N;
			}
			else if (Key == "read" || Key == "end")
			{
				for (const std::string& Id : NetDetail::Split(Value, ','))
				{
					if (Id.empty())
					{
						continue;
					}
					if (!NetDetail::IsId(Id))
					{
						return false;
					}
					(Key == "read" ? Next.Read : Next.Endings).insert(Id);
				}
			}
			else if (Key == "chess")
			{
				const std::vector<std::string> Nums = NetDetail::Split(Value, ',');
				if (Nums.size() != 9)
				{
					return false;
				}
				for (size_t I = 0; I < 9; ++I)
				{
					if (!NetDetail::ToInt(Nums[I], N) || N < 0)
					{
						return false;
					}
					(I % 3 == 0 ? Next.ChessWon : (I % 3 == 1 ? Next.ChessDrawn : Next.ChessLost))[I / 3] = N;
				}
			}
			else if (Key == "lvl")
			{
				if (!NetDetail::ToInt(Value, N))
				{
					return false;
				}
				Next.ChessLevel = N < 1 ? 1 : (N > 3 ? 3 : N);
			}
			else if (Key == "book")
			{
				for (const std::string& Item : NetDetail::Split(Value, ','))
				{
					if (Item.empty())
					{
						continue;
					}
					const std::vector<std::string> F = NetDetail::Split(Item, ':');
					int Night = 0, Slot = 0;
					if (F.size() != 2 || !NetDetail::ToInt(F[0], Night) || !NetDetail::ToInt(F[1], Slot) || Slot < 0 || Slot >= ScopeSlots)
					{
						return false;
					}
					Next.Bookings[Night] = Slot;
				}
			}
			else if (Key == "res")
			{
				for (const std::string& Item : NetDetail::Split(Value, ','))
				{
					if (Item.empty())
					{
						continue;
					}
					const std::vector<std::string> F = NetDetail::Split(Item, ':');
					FScopeResult R;
					int Clear = 0;
					if (F.size() != 4 || !NetDetail::ToInt(F[0], R.Night) || !NetDetail::ToInt(F[1], R.Slot) || !NetDetail::ToInt(F[2], R.Target)
						|| !NetDetail::ToInt(F[3], Clear) || R.Slot < 0 || R.Slot >= ScopeSlots || R.Target < 0 || R.Target >= ScopeTargets)
					{
						return false;
					}
					R.bClear = Clear != 0;
					Next.Results.push_back(R);
				}
			}
			else if (Key == "cs")
			{
				Next.ChessSave = Value;
			}
			else if (Key == "as")
			{
				Next.AdvSave = Value;
			}
			// an unknown key is a newer save's: skipped
		}
		*this = Next;
		return true;
	}
}
