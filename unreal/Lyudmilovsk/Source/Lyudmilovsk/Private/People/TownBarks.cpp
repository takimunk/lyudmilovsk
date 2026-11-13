#include "TownBarks.h"

#include "PeopleSim.h"

#include <algorithm>
#include <cstdlib>

namespace LyudBarks
{
	namespace BarkText
	{
		static std::string Trim(const std::string& S)
		{
			size_t A = 0;
			size_t E = S.size();
			while (A < E && (S[A] == ' ' || S[A] == '\t' || S[A] == '\r' || S[A] == '\n'))
			{
				A++;
			}
			while (E > A && (S[E - 1] == ' ' || S[E - 1] == '\t' || S[E - 1] == '\r' || S[E - 1] == '\n'))
			{
				E--;
			}
			return S.substr(A, E - A);
		}

		static void Words(const std::string& S, std::vector<std::string>& Out)
		{
			size_t I = 0;
			while (I < S.size())
			{
				while (I < S.size() && (S[I] == ' ' || S[I] == '\t'))
				{
					I++;
				}
				size_t J = I;
				while (J < S.size() && S[J] != ' ' && S[J] != '\t')
				{
					J++;
				}
				if (J > I)
				{
					Out.push_back(S.substr(I, J - I));
				}
				I = J;
			}
		}

		static bool AllDigits(const std::string& S)
		{
			if (S.empty())
			{
				return false;
			}
			for (char C : S)
			{
				if (C < '0' || C > '9')
				{
					return false;
				}
			}
			return true;
		}

		static bool Number(const std::string& S, float& Out)
		{
			if (S.empty())
			{
				return false;
			}
			char* End = nullptr;
			const double V = std::strtod(S.c_str(), &End);
			if (End == nullptr || *End != '\0')
			{
				return false;
			}
			Out = (float)V;
			return true;
		}

		static bool Contains(const std::vector<std::string>& List, const std::string& V)
		{
			return std::find(List.begin(), List.end(), V) != List.end();
		}
	}

	// --- ranges and values -------------------------------------------------------------------------------------------

	bool FRanges::Has(float V, bool bHours) const
	{
		for (const FRange& Rg : R)
		{
			if (Rg.bSingle)
			{
				if (bHours ? (V >= Rg.From && V < Rg.From + 1.f) : V == Rg.From)
				{
					return true;
				}
				continue;
			}
			if (Rg.From <= Rg.To)
			{
				if (V >= Rg.From && (bHours ? V < Rg.To : V <= Rg.To))
				{
					return true;
				}
			}
			else if (V >= Rg.From || (bHours ? V < Rg.To : V <= Rg.To))
			{
				return true;
			}
		}
		return false;
	}

	bool FRanges::Parse(const std::string& Line)
	{
		std::vector<std::string> Tokens;
		BarkText::Words(Line, Tokens);
		bool bOk = !Tokens.empty();
		for (const std::string& T : Tokens)
		{
			FRange Rg;
			const size_t Dash = T.find('-', 1);
			if (Dash == std::string::npos)
			{
				Rg.bSingle = true;
				if (!BarkText::Number(T, Rg.From))
				{
					bOk = false;
					continue;
				}
				Rg.To = Rg.From;
			}
			else if (!BarkText::Number(T.substr(0, Dash), Rg.From) || !BarkText::Number(T.substr(Dash + 1), Rg.To))
			{
				bOk = false;
				continue;
			}
			R.push_back(Rg);
		}
		return bOk;
	}

	bool FValues::Match(const std::string& Have) const
	{
		if (!Have.empty() && BarkText::Contains(No, Have))
		{
			return false;
		}
		if (BarkText::Contains(No, "any") && !Have.empty())
		{
			return false;
		}
		if (Yes.empty())
		{
			return true;
		}
		return !Have.empty() && (BarkText::Contains(Yes, Have) || BarkText::Contains(Yes, "any"));
	}

	bool FValues::Match(const std::vector<std::string>& Have) const
	{
		bool bYes = Yes.empty();
		for (const std::string& H : Have)
		{
			if (H.empty())
			{
				continue;
			}
			if (BarkText::Contains(No, H) || BarkText::Contains(No, "any"))
			{
				return false;
			}
			if (!bYes && (BarkText::Contains(Yes, H) || BarkText::Contains(Yes, "any")))
			{
				bYes = true;
			}
		}
		return bYes;
	}

	void FWho::Reset()
	{
		Jobs.clear();
		Age = 30;
		bFemale = false;
		Doing.clear();
		At.clear();
		Pose.clear();
		Gather.clear();
		Chrono.clear();
		Manner.clear();
		Needs.clear();
		bHeroNear = false;
	}

	// --- reading the file --------------------------------------------------------------------------------------------

	static void SplitValues(const std::string& Value, FValues& Out)
	{
		std::vector<std::string> Tokens;
		BarkText::Words(Value, Tokens);
		for (const std::string& T : Tokens)
		{
			if (T.size() > 1 && T[0] == '!')
			{
				Out.No.push_back(T.substr(1));
			}
			else
			{
				Out.Yes.push_back(T);
			}
		}
	}

	/** town.greet_early.1..5 -> town.greet_early.1 ... town.greet_early.5; anything else as it is. */
	static bool ExpandRun(const std::string& Token, std::vector<std::string>& Out)
	{
		const size_t Dots = Token.find("..");
		if (Dots == std::string::npos)
		{
			Out.push_back(Token);
			return true;
		}
		const std::string Left = Token.substr(0, Dots);
		const std::string Right = Token.substr(Dots + 2);
		size_t D = Left.size();
		while (D > 0 && Left[D - 1] >= '0' && Left[D - 1] <= '9')
		{
			D--;
		}
		if (D == Left.size() || !BarkText::AllDigits(Right))
		{
			return false;
		}
		const int From = std::atoi(Left.c_str() + D);
		const int To = std::atoi(Right.c_str());
		if (To < From || To - From > 200)
		{
			return false;
		}
		const std::string Base = Left.substr(0, D);
		for (int N = From; N <= To; N++)
		{
			Out.push_back(Base + std::to_string(N));
		}
		return true;
	}

	static bool ParseAge(const std::string& Value, FWhoCond& C)
	{
		std::vector<std::string> Tokens;
		BarkText::Words(Value, Tokens);
		bool bOk = true;
		for (const std::string& T : Tokens)
		{
			const bool bNot = T.size() > 1 && T[0] == '!';
			const std::string V = bNot ? T.substr(1) : T;
			if (V == "kid" || V == "teen" || V == "adult" || V == "elder")
			{
				(bNot ? C.AgeGroup.No : C.AgeGroup.Yes).push_back(V);
			}
			else if (bNot || !C.AgeYears.Parse(V))
			{
				bOk = false;
			}
		}
		return bOk;
	}

	bool FBook::ParseLine(FSet& S, const std::string& Key, const std::string& Value, int LineNo)
	{
		auto Warn = [this, &S, LineNo](const std::string& What)
		{
			Warnings.push_back("line " + std::to_string(LineNo) + " [" + S.Name + "]: " + What);
		};
		const bool bSecond = Key.size() > 1 && Key.back() == '2';
		FWhoCond& Who = bSecond ? S.B : S.A;
		const std::string K = bSecond ? Key.substr(0, Key.size() - 1) : Key;
		bool bCondition = true;
		if (Key == "say")
		{
			bCondition = false;
			std::vector<std::string> Tokens;
			BarkText::Words(Value, Tokens);
			for (const std::string& T : Tokens)
			{
				if (!ExpandRun(T, S.Lines))
				{
					Warn("bad run of lines: " + T);
				}
			}
		}
		else if (Key == "talk")
		{
			bCondition = false;
			std::vector<std::string> Turns;
			size_t I = 0;
			while (I <= Value.size())
			{
				const size_t Bar = std::min(Value.find('|', I), Value.size());
				const std::string T = BarkText::Trim(Value.substr(I, Bar - I));
				if (!T.empty())
				{
					Turns.push_back(T);
				}
				I = Bar + 1;
			}
			if (Turns.size() < 2)
			{
				Warn("a talk needs two lines at least");
			}
			else
			{
				S.Talks.push_back(Turns);
			}
		}
		else if (Key == "to")
		{
			bCondition = false;
			if (Value != "hero")
			{
				Warn("to = " + Value + ": only hero");
			}
			S.Kind = EKind::Greet;
		}
		else if (Key == "on")
		{
			bCondition = false;
			S.On = Value;
			S.Kind = EKind::Event;
		}
		else if (Key == "weight")
		{
			bCondition = false;
			if (!BarkText::Number(Value, S.Weight) || S.Weight < 0.f)
			{
				Warn("bad weight " + Value);
				S.Weight = 1.f;
			}
		}
		else if (Key == "near")
		{
			if (Value != "hero")
			{
				Warn("near = " + Value + ": only hero");
			}
			S.A.bNearHero = true;
		}
		else if (K == "job") { SplitValues(Value, Who.Job); }
		else if (K == "sex") { SplitValues(Value, Who.Sex); }
		else if (K == "doing") { SplitValues(Value, Who.Doing); }
		else if (K == "age")
		{
			if (!ParseAge(Value, Who))
			{
				Warn("bad age " + Value);
			}
		}
		else if (!bSecond && Key == "at")
		{
			SplitValues(Value, S.A.At);
			// outdoors / indoors alone only says where a line can be said: it doesn't make the set more particular
			std::vector<std::string> Tokens;
			BarkText::Words(Value, Tokens);
			bool bWhere = !Tokens.empty();
			for (const std::string& T : Tokens)
			{
				const std::string V = T.size() > 1 && T[0] == '!' ? T.substr(1) : T;
				bWhere = bWhere && (V == "outdoors" || V == "indoors");
			}
			bCondition = !bWhere;
		}
		else if (!bSecond && Key == "pose") { SplitValues(Value, S.A.Pose); }
		else if (!bSecond && Key == "gather") { SplitValues(Value, S.A.Gather); }
		else if (!bSecond && Key == "chrono") { SplitValues(Value, S.A.Chrono); }
		else if (!bSecond && Key == "manner") { SplitValues(Value, S.A.Manner); }
		else if (!bSecond && Key == "need") { SplitValues(Value, S.A.Need); }
		else if (Key == "hours" || Key == "months")
		{
			if (!(Key == "hours" ? S.Hours : S.Months).Parse(Value))
			{
				Warn("bad range in " + Key + " = " + Value);
			}
		}
		else if (Key == "weekday") { SplitValues(Value, S.Weekday); }
		else if (Key == "sun") { SplitValues(Value, S.Sun); }
		else if (Key == "weather") { SplitValues(Value, S.Weather); }
		else if (Key == "festival") { SplitValues(Value, S.Festival); }
		else if (Key == "phase") { SplitValues(Value, S.Phase); }
		else
		{
			Warn("unknown key " + Key);
			return false;
		}
		if (bCondition)
		{
			S.Conditions++;
		}
		return true;
	}

	bool FBook::Parse(const std::string& Source)
	{
		Sets.clear();
		Warnings.clear();
		LastError.clear();
		size_t I = 0;
		int LineNo = 0;
		FSet* Cur = nullptr;
		while (I < Source.size())
		{
			size_t E = Source.find('\n', I);
			if (E == std::string::npos)
			{
				E = Source.size();
			}
			const std::string Line = BarkText::Trim(Source.substr(I, E - I));
			I = E + 1;
			LineNo++;
			if (Line.empty() || Line[0] == '#')
			{
				continue;
			}
			if (Line[0] == '[')
			{
				const size_t Close = Line.find(']');
				FSet S;
				S.Name = BarkText::Trim(Line.substr(1, Close == std::string::npos ? std::string::npos : Close - 1));
				S.FileLine = LineNo;
				Sets.push_back(S);
				Cur = &Sets.back();
				continue;
			}
			const size_t Eq = Line.find('=');
			if (Eq == std::string::npos || Cur == nullptr)
			{
				Warnings.push_back("line " + std::to_string(LineNo) + ": not 'key = value' in a [set]: " + Line);
				continue;
			}
			ParseLine(*Cur, BarkText::Trim(Line.substr(0, Eq)), BarkText::Trim(Line.substr(Eq + 1)), LineNo);
		}
		// what each set is: a greeting or an event's line keeps its kind; lines in turn make an exchange
		for (FSet& S : Sets)
		{
			if (!S.Talks.empty() && (S.Kind == EKind::Greet || S.Kind == EKind::Event))
			{
				Warnings.push_back("[" + S.Name + "]: a greeting or an event says single lines, its talks are left out");
				S.Talks.clear();
			}
			if (!S.Talks.empty())
			{
				S.Kind = EKind::Talk;
				if (!S.Lines.empty())
				{
					Warnings.push_back("[" + S.Name + "]: both say and talk; the says are left out");
					S.Lines.clear();
				}
			}
			if (S.Lines.empty() && S.Talks.empty())
			{
				Warnings.push_back("[" + S.Name + "]: no lines");
			}
		}
		if (Sets.empty())
		{
			LastError = "no [sets]";
			return false;
		}
		return true;
	}

	std::vector<std::string> FBook::Keys() const
	{
		std::vector<std::string> Out;
		auto Add = [&Out](const std::string& K)
		{
			const std::string Full = "line." + K;
			if (std::find(Out.begin(), Out.end(), Full) == Out.end())
			{
				Out.push_back(Full);
			}
		};
		for (const FSet& S : Sets)
		{
			for (const std::string& K : S.Lines)
			{
				Add(K);
			}
			for (const std::vector<std::string>& T : S.Talks)
			{
				for (const std::string& K : T)
				{
					Add(K);
				}
			}
		}
		return Out;
	}

	// --- matching ----------------------------------------------------------------------------------------------------

	const char* FBook::AgeGroup(int Age)
	{
		return Age < 12 ? "kid" : Age < 18 ? "teen" : Age < 60 ? "adult" : "elder";
	}

	const char* FBook::WeekdayName(int Weekday)
	{
		static const char* const Names[7] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
		return Names[((Weekday % 7) + 7) % 7];
	}

	bool FBook::MatchWho(const FWhoCond& C, const FWho& W) const
	{
		if (C.bNearHero && !W.bHeroNear)
		{
			return false;
		}
		if (!C.AgeGroup.Empty() || !C.AgeYears.Empty())
		{
			const std::string Group = AgeGroup(W.Age);
			if (BarkText::Contains(C.AgeGroup.No, Group))
			{
				return false;
			}
			if ((!C.AgeGroup.Yes.empty() || !C.AgeYears.Empty())
				&& !BarkText::Contains(C.AgeGroup.Yes, Group) && !C.AgeYears.Has((float)W.Age, false))
			{
				return false;
			}
		}
		static const std::string Female = "female";
		static const std::string Male = "male";
		return C.Sex.Match(W.bFemale ? Female : Male)
			&& C.Job.Match(W.Jobs)
			&& C.Doing.Match(W.Doing)
			&& C.At.Match(W.At)
			&& C.Pose.Match(W.Pose)
			&& C.Gather.Match(W.Gather)
			&& C.Chrono.Match(W.Chrono)
			&& C.Manner.Match(W.Manner)
			&& C.Need.Match(W.Needs);
	}

	bool FBook::MatchNow(const FSet& S, const FNow& N) const
	{
		if (!S.Hours.Empty() && !S.Hours.Has(N.Hour, true))
		{
			return false;
		}
		if (!S.Months.Empty() && !S.Months.Has((float)N.Month, false))
		{
			return false;
		}
		if (!S.Weekday.Empty() && !S.Weekday.Match(std::string(WeekdayName(N.Weekday))))
		{
			return false;
		}
		return S.Sun.Match(N.Sun) && S.Weather.Match(N.Weather) && S.Festival.Match(N.Festival) && S.Phase.Match(N.Phase);
	}

	// --- picking -----------------------------------------------------------------------------------------------------

	float FBook::Rand01()
	{
		// xorshift32
		uint32_t X = Rng;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		Rng = X;
		return (float)(X >> 8) / 16777216.f;
	}

	uint32_t FBook::Hash(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H;
	}

	bool FBook::IsRecent(uint32_t H) const
	{
		for (int I = 0; I < RecentN; I++)
		{
			if (Recent[I] == H)
			{
				return true;
			}
		}
		return false;
	}

	void FBook::Remember(uint32_t H)
	{
		Recent[RecentAt] = H;
		RecentAt = (RecentAt + 1) % RecentMax;
		RecentN = std::min(RecentN + 1, RecentMax);
	}

	int FBook::TakeWeighted()
	{
		float Total = 0.f;
		for (int Ix : Cand)
		{
			Total += Chance(Sets[(size_t)Ix]);
		}
		if (Cand.empty() || Total <= 0.f)
		{
			return -1;
		}
		float R = Rand01() * Total;
		size_t At = Cand.size() - 1;
		for (size_t I = 0; I < Cand.size(); I++)
		{
			R -= Chance(Sets[(size_t)Cand[I]]);
			if (R <= 0.f)
			{
				At = I;
				break;
			}
		}
		const int Picked = Cand[At];
		Cand.erase(Cand.begin() + (std::ptrdiff_t)At);
		if (!CandSwap.empty())
		{
			// CandSwap runs alongside Cand (PickTalk): the picked one's flag goes to its end, the caller reads it there
			const uint8_t Flag = CandSwap[At];
			CandSwap.erase(CandSwap.begin() + (std::ptrdiff_t)At);
			CandSwap.push_back(Flag);
		}
		return Picked;
	}

	const std::string* FBook::PickLine(EKind Kind, const FWho& Who, const FNow& Now, const char* On, int* OutSet)
	{
		Cand.clear();
		CandSwap.clear();
		for (size_t I = 0; I < Sets.size(); I++)
		{
			const FSet& S = Sets[I];
			if (S.Kind != Kind || S.Lines.empty() || (Kind == EKind::Event && (On == nullptr || S.On != On)))
			{
				continue;
			}
			if (MatchWho(S.A, Who) && MatchNow(S, Now))
			{
				Cand.push_back((int)I);
			}
		}
		const bool bRepeat = Kind == EKind::Greet || Kind == EKind::Event;
		int Fallback = -1;
		while (!Cand.empty())
		{
			const int Ix = TakeWeighted();
			if (Ix < 0)
			{
				break;
			}
			const FSet& S = Sets[(size_t)Ix];
			Fresh.clear();
			for (size_t L = 0; L < S.Lines.size(); L++)
			{
				if (!IsRecent(Hash(S.Lines[L])))
				{
					Fresh.push_back((int)L);
				}
			}
			if (Fresh.empty())
			{
				Fallback = Fallback < 0 ? Ix : Fallback;
				continue;
			}
			const std::string& Key = S.Lines[(size_t)Fresh[std::min((size_t)(Rand01() * (float)Fresh.size()), Fresh.size() - 1)]];
			Remember(Hash(Key));
			if (OutSet)
			{
				*OutSet = Ix;
			}
			return &Key;
		}
		if (bRepeat && Fallback >= 0)
		{
			const FSet& S = Sets[(size_t)Fallback];
			const std::string& Key = S.Lines[std::min((size_t)(Rand01() * (float)S.Lines.size()), S.Lines.size() - 1)];
			Remember(Hash(Key));
			if (OutSet)
			{
				*OutSet = Fallback;
			}
			return &Key;
		}
		return nullptr;
	}

	const std::vector<std::string>* FBook::PickTalk(const FWho& A, const FWho& B, const FNow& Now, bool& bSwapped, int* OutSet)
	{
		Cand.clear();
		CandSwap.clear();
		bSwapped = false;
		for (size_t I = 0; I < Sets.size(); I++)
		{
			const FSet& S = Sets[I];
			if (S.Kind != EKind::Talk || S.Talks.empty() || !MatchNow(S, Now))
			{
				continue;
			}
			const bool bAB = MatchWho(S.A, A) && MatchWho(S.B, B);
			const bool bBA = MatchWho(S.A, B) && MatchWho(S.B, A);
			if (bAB || bBA)
			{
				Cand.push_back((int)I);
				// both ways round: whoever starts it, by chance
				CandSwap.push_back(bAB && bBA ? (Rand01() < 0.5f ? 1 : 0) : (bBA ? 1 : 0));
			}
		}
		while (!Cand.empty())
		{
			const int Ix = TakeWeighted();
			if (Ix < 0)
			{
				break;
			}
			const bool bSwap = CandSwap.back() != 0;
			CandSwap.pop_back();
			const FSet& S = Sets[(size_t)Ix];
			Fresh.clear();
			for (size_t T = 0; T < S.Talks.size(); T++)
			{
				if (!IsRecent(Hash(S.Talks[T][0])))
				{
					Fresh.push_back((int)T);
				}
			}
			if (Fresh.empty())
			{
				continue;
			}
			const std::vector<std::string>& Talk = S.Talks[(size_t)Fresh[std::min((size_t)(Rand01() * (float)Fresh.size()), Fresh.size() - 1)]];
			Remember(Hash(Talk[0]));
			bSwapped = bSwap;
			if (OutSet)
			{
				*OutSet = Ix;
			}
			return &Talk;
		}
		return nullptr;
	}

	// --- from the people's life ----------------------------------------------------------------------------------------

	bool CanSpeak(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P)
	{
		using namespace LyudPeople;
		if (P.Activity < 0 || P.Activity == W.SleepAct || P.Pose == EPose::Hidden || P.Pose == EPose::Sleep)
		{
			return false;
		}
		if (P.State == EState::Idle || P.State == EState::Riding || P.State == EState::Away)
		{
			return false;
		}
		return !W.InsideShell(P);
	}

	void FillWhoRole(FWho& Who, const char* Role)
	{
		if (Role != nullptr && *Role != '\0' && std::find(Who.Jobs.begin(), Who.Jobs.end(), Role) == Who.Jobs.end())
		{
			Who.Jobs.push_back(Role);
		}
	}

	void FillWho(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, FWho& Who)
	{
		using namespace LyudPeople;
		Who.Reset();
		FillWhoRole(Who, P.Occupation.c_str());
		for (const FDuty& D : P.Duties)
		{
			FillWhoRole(Who, D.Role.c_str());
		}
		Who.Age = P.Age;
		Who.bFemale = P.bFemale;
		// at the place of what they do (walking there doesn't count; a walk is at the spot it goes to next)
		const bool bThere = P.State == EState::Doing || P.State == EState::Settling || P.State == EState::Leaving;
		bool bWork = false;
		if (P.Activity >= 0 && P.Activity < (int)W.Activities.size())
		{
			const FActivity& A = W.Activities[(size_t)P.Activity];
			Who.Doing.push_back(A.Id);
			for (const FDuty& D : P.Duties)
			{
				if (bThere && W.ActOf(D) == P.Activity && P.Activity != W.SleepAct && (D.Place < 0 || D.Place == P.Place))
				{
					bWork = true;
				}
			}
			if (A.GatherMode >= 0 && A.GatherMode <= 3)
			{
				static const char* const Modes[4] = { "mingle", "watch", "sit", "fair" };
				Who.Gather = Modes[A.GatherMode];
			}
		}
		Who.Doing.push_back(bWork ? "work" : "free");
		// under the sky or under a roof (a flat, the club, the restaurant's hall, the diner)
		const bool bRoof = P.bIndoors || (bThere && P.Place >= 0 && P.Place < (int)W.Places.size() && W.Places[(size_t)P.Place].bIndoor);
		Who.At.push_back(bRoof ? "indoors" : "outdoors");
		if (P.Place >= 0 && P.Place < (int)W.Places.size() && (bThere || P.State == EState::Waiting))
		{
			const FPlace& Pl = W.Places[(size_t)P.Place];
			Who.At.push_back(Pl.Kind);
			Who.At.push_back(Pl.Id);
			if (!Pl.Building.empty() && Pl.Building != Pl.Id)
			{
				Who.At.push_back(Pl.Building);
			}
		}
		Who.Pose = PoseName(P.Pose);
		Who.Chrono = P.Chronotype;
		for (int N = 0; N < NumNeeds; N++)
		{
			if (P.N[N] > 0.75f)
			{
				Who.Needs.push_back(NeedName((ENeed)N));
			}
		}
	}

	const char* SunState(float AltitudeDeg, float Cloud)
	{
		if (AltitudeDeg < -4.f)
		{
			return "night";
		}
		if (Cloud > 0.75f)
		{
			return "cloudy";
		}
		return AltitudeDeg < 10.f ? "low" : "day";
	}

	void WeatherWords(float Cloud, float RainMmH, float Snow, float Fog, float TempC, float Wet, std::vector<std::string>& Out)
	{
		Out.clear();
		if (RainMmH > 0.2f && Snow < 0.5f)
		{
			Out.push_back("rain");
		}
		if (RainMmH > 0.1f && Snow >= 0.5f)
		{
			Out.push_back("snow");
		}
		if (Fog > 0.45f)
		{
			Out.push_back("fog");
		}
		if (TempC < 2.f)
		{
			Out.push_back("cold");
		}
		if (TempC > 22.f)
		{
			Out.push_back("warm");
		}
		if (Cloud < 0.35f && RainMmH < 0.05f && Fog <= 0.45f)
		{
			Out.push_back("clear");
		}
		if (Wet > 0.4f)
		{
			Out.push_back("wet");
		}
	}
}
