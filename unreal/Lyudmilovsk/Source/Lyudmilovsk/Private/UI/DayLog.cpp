#include "DayLog.h"

#include "../People/MiniJson.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace LyudDayLog
{
	namespace
	{
		uint32_t Mix(uint32_t H)
		{
			H ^= H >> 16;
			H *= 0x7feb352dU;
			H ^= H >> 15;
			H *= 0x846ca68bU;
			H ^= H >> 16;
			return H;
		}

		uint32_t HashOf(const std::string& S, uint32_t Seed)
		{
			uint32_t H = 2166136261U ^ Seed;
			for (const char C : S)
			{
				H = (H ^ (uint8_t)C) * 16777619U;
			}
			return Mix(H);
		}

		bool StartsWith(const std::string& S, const std::string& P)
		{
			return S.size() >= P.size() && S.compare(0, P.size(), P) == 0;
		}

		bool Matches(const std::string& Pattern, const std::string& Target)
		{
			if (Pattern.empty() || Pattern == "*")
			{
				return true;
			}
			if (Pattern[0] == '~')
			{
				return Target.find(Pattern.substr(1)) != std::string::npos;
			}
			if (Pattern.back() == '*')
			{
				return StartsWith(Target, Pattern.substr(0, Pattern.size() - 1));
			}
			return Pattern == Target;
		}

		std::string Num(float V)
		{
			char B[32];
			std::snprintf(B, sizeof(B), "%.2f", V);
			return B;
		}

		std::vector<std::string> Split(const std::string& S, char By)
		{
			std::vector<std::string> Out;
			std::string Cur;
			for (const char C : S)
			{
				if (C == By)
				{
					Out.push_back(Cur);
					Cur.clear();
				}
				else
				{
					Cur.push_back(C);
				}
			}
			Out.push_back(Cur);
			return Out;
		}

		/** One of the variants Prefix.0..N-1 not used the day before (any of them if all were); "" when N is 0. */
		std::string Pick(const std::string& Prefix, int N, const FEntry* Before, uint32_t Seed)
		{
			if (N <= 0)
			{
				return std::string();
			}
			std::vector<std::string> Free;
			for (int I = 0; I < N; ++I)
			{
				const std::string K = Prefix + "." + std::to_string(I);
				bool bUsed = false;
				if (Before)
				{
					for (const FLine& L : Before->Lines)
					{
						bUsed = bUsed || L.Key == K || L.Fallback == K;
					}
				}
				if (!bUsed)
				{
					Free.push_back(K);
				}
			}
			if (Free.empty())
			{
				Free.push_back(Prefix + ".0");
			}
			return Free[Mix(Seed) % Free.size()];
		}
	}

	// ------------------------------------------------------------------ data

	bool FData::Load(const std::string& Json, std::string& Error)
	{
		LyudJson::FValue Root;
		if (!LyudJson::Parse(Json, Root, Error))
		{
			return false;
		}
		Rules.clear();
		Topics.clear();
		Weather.clear();
		for (const LyudJson::FValue& R : Root["rules"].Items)
		{
			FRule Rule;
			Rule.Event = R["event"].S();
			Rule.Target = R["target"].S();
			Rule.Topic = R["topic"].S();
			for (const LyudJson::FValue& E : R["except"].Items)
			{
				Rule.Except.push_back(E.S());
			}
			if (Rule.Event.empty() || Rule.Topic.empty())
			{
				Error = "daylog.json: a rule without event or topic";
				return false;
			}
			Rules.push_back(Rule);
		}
		for (const LyudJson::FValue& T : Root["topics"].Items)
		{
			FTopic Topic;
			Topic.Id = T["id"].S();
			Topic.Rank = T["rank"].I(10);
			Topic.Slot = T["slot"].S();
			Topic.Doodle = T["doodle"].S();
			Topic.bOnce = T["once"].B;
			Topic.bFiller = T["filler"].B;
			Topic.One = T["one"].I();
			Topic.Many = T["many"].I();
			Topic.Plain = T["plain"].I();
			if (Topic.Id.empty() || Topic.Plain == 0)
			{
				Error = "daylog.json: topic " + Topic.Id + " has no lines";
				return false;
			}
			Topics.push_back(Topic);
		}
		for (const LyudJson::FValue& W : Root["weather"].Items)
		{
			Weather.emplace_back(W["code"].S(), W["n"].I());
		}
		Quiet = Root["quiet"].I();
		Busy = Root["busy"].I();
		Calm = Root["calm"].I();
		MaxTopics = Root["max_topics"].I(4);
		BusyHits = Root["busy_hits"].I(7);
		EveningHour = Root["evening_hour"].I(18);
		for (const FRule& R : Rules)
		{
			if (!FindTopic(R.Topic))
			{
				Error = "daylog.json: rule for an unknown topic " + R.Topic;
				return false;
			}
		}
		if (Weather.empty() || Quiet <= 0)
		{
			Error = "daylog.json: no weather or quiet lines";
			return false;
		}
		return true;
	}

	const FTopic* FData::FindTopic(const std::string& Id) const
	{
		for (const FTopic& T : Topics)
		{
			if (T.Id == Id)
			{
				return &T;
			}
		}
		return nullptr;
	}

	const FTopic* FData::Match(const std::string& Event, const std::string& Target) const
	{
		for (const FRule& R : Rules)
		{
			if (R.Event != Event || !Matches(R.Target, Target))
			{
				continue;
			}
			if (std::find(R.Except.begin(), R.Except.end(), Target) != R.Except.end())
			{
				continue;
			}
			return FindTopic(R.Topic);
		}
		return nullptr;
	}

	// ------------------------------------------------------------------ facts

	FHit* FDay::Find(const std::string& Topic)
	{
		for (FHit& H : Hits)
		{
			if (H.Topic == Topic)
			{
				return &H;
			}
		}
		return nullptr;
	}

	const FHit* FDay::Find(const std::string& Topic) const
	{
		return const_cast<FDay*>(this)->Find(Topic);
	}

	std::string Snake(const std::string& Camel)
	{
		std::string Out;
		for (size_t I = 0; I < Camel.size(); ++I)
		{
			const char C = Camel[I];
			if (C >= 'A' && C <= 'Z')
			{
				if (I > 0)
				{
					Out.push_back('_');
				}
				Out.push_back((char)(C - 'A' + 'a'));
			}
			else
			{
				Out.push_back(C);
			}
		}
		return Out;
	}

	std::string NameArg(const std::string& Slot, const std::string& Target)
	{
		if (Target.empty())
		{
			return std::string();
		}
		if (Slot == "item") return "@item." + Target + ".name";
		if (Slot == "npc") return "@npc." + Target + ".name";
		if (Slot == "bird") return "@bird." + Snake(Target) + ".name";
		if (Slot == "place") return "@place." + Target + ".name";
		if (Slot == "film") return "@film." + Target + ".title";
		if (Slot == "festival")
		{
			// festival_visit sun_day.zenith: the festival is what comes before the dot
			const size_t Dot = Target.find('.');
			return "@festival." + (Dot == std::string::npos ? Target : Target.substr(0, Dot)) + ".name";
		}
		return std::string();
	}

	std::string WeatherOf(const FDay& D)
	{
		const float Cloud = D.Hours ? D.CloudSum / (float)D.Hours : 0.5f;
		for (const char* Code : { "thunder", "aurora", "rainbow", "snow", "rain", "fog", "drizzle", "frost", "hot", "clear", "cloudy", "mixed" })
		{
			const std::string C(Code);
			const bool bTrue =
				C == "thunder" ? D.ThunderH > 0 :
				C == "aurora" ? D.bAurora :
				C == "rainbow" ? D.bRainbow :
				C == "snow" ? D.SnowH >= 1 :
				C == "rain" ? D.RainH >= 2 :
				C == "fog" ? D.FogH >= 2 :
				C == "drizzle" ? D.DrizzleH + D.RainH >= 1 :
				C == "frost" ? D.Hours > 0 && D.TMin <= -5.f :
				C == "hot" ? D.TMax >= 25.f :
				C == "clear" ? Cloud < 0.3f :
				C == "cloudy" ? Cloud > 0.7f :
				true;
			if (bTrue)
			{
				return C;
			}
		}
		return "mixed";
	}

	FDay& FModel::DayOf(int Day)
	{
		for (auto It = Days.rbegin(); It != Days.rend(); ++It)
		{
			if (It->Day == Day)
			{
				return *It;
			}
		}
		FDay D;
		D.Day = Day;
		// kept in order (a load or a dev jump can go back)
		auto At = std::upper_bound(Days.begin(), Days.end(), Day, [](int V, const FDay& E) { return V < E.Day; });
		return *Days.insert(At, D);
	}

	bool FModel::Note(int Day, const std::string& Event, const std::string& Target, int N)
	{
		const FTopic* T = Data.Match(Event, Target);
		if (!T)
		{
			return false;
		}
		FDay& D = DayOf(Day);
		FHit* H = D.Find(T->Id);
		if (!H)
		{
			D.Hits.push_back(FHit());
			H = &D.Hits.back();
			H->Topic = T->Id;
		}
		H->Count += std::max(1, N);
		if (!Target.empty() && std::find(H->Targets.begin(), H->Targets.end(), Target) == H->Targets.end() && H->Targets.size() < 12)
		{
			H->Targets.push_back(Target);
		}
		return true;
	}

	void FModel::Sample(int Day, int Hour, const FSky& Sky)
	{
		FDay& D = DayOf(Day);
		if (Hour == D.LastHour)
		{
			return;
		}
		D.LastHour = Hour;
		++D.Hours;
		D.RainH += Sky.Precip == "rain" ? 1 : 0;
		D.DrizzleH += Sky.Precip == "drizzle" ? 1 : 0;
		D.SnowH += Sky.Precip == "snow" ? 1 : 0;
		D.ThunderH += Sky.Precip == "thunder" ? 1 : 0;
		D.FogH += Sky.Fog > 0.5f ? 1 : 0;
		D.CloudSum += Sky.Cloud;
		D.TMin = std::min(D.TMin, Sky.TempC);
		D.TMax = std::max(D.TMax, Sky.TempC);
		D.bRainbow = D.bRainbow || Sky.bRainbow;
		D.bAurora = D.bAurora || Sky.bAurora;
	}

	// ------------------------------------------------------------------ writing

	FEntry FModel::Compose(const FDay& D, const FEntry* Before, std::set<std::string>& Ever) const
	{
		FEntry E;
		E.Day = D.Day;
		const uint32_t Seed = Mix((uint32_t)D.Day * 2654435761U + 17U);

		// the topics of the day, worth a line each: their rank, a first time ever counts more, a little chance
		struct FCand
		{
			const FTopic* T;
			const FHit* H;
			std::string Name;   // the target named (one met for the first time if any)
			bool bFirst;
			int Score;
		};
		std::vector<FCand> Cands;
		int Events = 0;
		for (const FHit& H : D.Hits)
		{
			const FTopic* T = Data.FindTopic(H.Topic);
			if (!T)
			{
				continue;
			}
			std::string Name;
			bool bFirst = false;
			for (const std::string& Tg : H.Targets)
			{
				if (!Ever.count(T->Id + ":" + Tg))
				{
					Name = Tg;
					bFirst = true;
					break;
				}
			}
			if (T->bOnce && !bFirst)
			{
				continue;
			}
			if (Name.empty() && !H.Targets.empty())
			{
				Name = H.Targets[Mix(Seed ^ HashOf(T->Id, 3)) % H.Targets.size()];
			}
			if (!Ever.count(T->Id))
			{
				bFirst = true;
			}
			Events += H.Count;
			const int Score = T->Rank + (bFirst ? 25 : 0) + (int)(Mix(Seed ^ HashOf(T->Id, 7)) % 12);
			Cands.push_back({ T, &H, Name, bFirst, Score });
		}
		for (const FHit& H : D.Hits)
		{
			Ever.insert(H.Topic);
			for (const std::string& Tg : H.Targets)
			{
				Ever.insert(H.Topic + ":" + Tg);
			}
		}
		std::stable_sort(Cands.begin(), Cands.end(), [](const FCand& A, const FCand& B)
		{
			if (A.T->bFiller != B.T->bFiller)
			{
				return !A.T->bFiller;
			}
			return A.Score > B.Score;
		});
		// a filler (what he ate) only on a day with little else
		size_t Take = std::min<size_t>(Cands.size(), (size_t)std::max(0, Data.MaxTopics));
		while (Take > 0 && Cands[Take - 1].T->bFiller && Take > 2)
		{
			--Take;
		}

		// the weather first
		E.Weather = WeatherOf(D);
		int WN = 0;
		for (const auto& W : Data.Weather)
		{
			WN = W.first == E.Weather ? W.second : WN;
		}
		if (D.Hours > 0 && WN > 0)
		{
			E.Lines.push_back({ Pick("diary.day.weather." + E.Weather, WN, Before, Seed ^ 0x11U), {}, "" });
		}
		E.Doodle = "weather:" + E.Weather;

		bool bDoodle = false;
		for (size_t I = 0; I < Take; ++I)
		{
			const FCand& C = Cands[I];
			const std::string Base = "diary.day." + C.T->Id;
			const std::string Arg = NameArg(C.T->Slot, C.Name);
			const uint32_t S = Seed ^ HashOf(C.T->Id, 29);
			const std::string Plain = Pick(Base + ".plain", C.T->Plain, Before, S);
			FLine L;
			if (!Arg.empty() && C.H->Count > 1 && C.T->Many > 0 && C.H->Targets.size() <= 1)
			{
				L.Key = Pick(Base + ".many", C.T->Many, Before, S);
				L.Args = { Arg, std::to_string(C.H->Count) };
			}
			else if (!Arg.empty() && C.T->One > 0)
			{
				L.Key = Pick(Base + ".one", C.T->One, Before, S);
				L.Args = { Arg };
			}
			else
			{
				L.Key = Plain;
			}
			L.Fallback = L.Key == Plain ? std::string() : Plain;
			E.Lines.push_back(L);
			if (!bDoodle && !C.T->Doodle.empty() && !C.Name.empty())
			{
				E.Doodle = C.T->Doodle + ":" + C.Name;
				bDoodle = true;
			}
		}

		// the last line: a quiet day says so, a full one sighs, an ordinary one now and then
		if (Take == 0)
		{
			E.Lines.push_back({ Pick("diary.day.quiet", Data.Quiet, Before, Seed ^ 0x23U), {}, "" });
		}
		else if (Events >= Data.BusyHits && Data.Busy > 0)
		{
			E.Lines.push_back({ Pick("diary.day.close.busy", Data.Busy, Before, Seed ^ 0x37U), {}, "" });
		}
		else if (Data.Calm > 0 && Mix(Seed ^ 0x41U) % 3 == 0)
		{
			E.Lines.push_back({ Pick("diary.day.close.calm", Data.Calm, Before, Seed ^ 0x43U), {}, "" });
		}
		return E;
	}

	std::vector<FEntry> FModel::Write(int Today, int Hour) const
	{
		std::vector<FEntry> Out;
		std::set<std::string> Ever;
		for (const FDay& D : Days)
		{
			if (D.Day > Today || (D.Day == Today && Hour < Data.EveningHour) || D.Empty())
			{
				continue;
			}
			const FEntry* Before = (!Out.empty() && Out.back().Day == D.Day - 1) ? &Out.back() : nullptr;
			Out.push_back(Compose(D, Before, Ever));
		}
		return Out;
	}

	// ------------------------------------------------------------------ the save: a line of numbers and a line a topic

	void FModel::Save(std::map<std::string, std::string>& Out) const
	{
		for (const FDay& D : Days)
		{
			std::string S = "w " + std::to_string(D.Hours) + " " + std::to_string(D.RainH) + " " + std::to_string(D.DrizzleH) + " " +
				std::to_string(D.SnowH) + " " + std::to_string(D.ThunderH) + " " + std::to_string(D.FogH) + " " + Num(D.CloudSum) + " " +
				Num(D.TMin) + " " + Num(D.TMax) + " " + (D.bRainbow ? "1" : "0") + " " + (D.bAurora ? "1" : "0") + " " + std::to_string(D.LastHour);
			for (const FHit& H : D.Hits)
			{
				S += "\nt\t" + H.Topic + "\t" + std::to_string(H.Count);
				for (const std::string& T : H.Targets)
				{
					S += "\t" + T;
				}
			}
			Out["d" + std::to_string(D.Day)] = S;
		}
	}

	void FModel::LoadState(const std::map<std::string, std::string>& In)
	{
		Days.clear();
		for (const auto& KV : In)
		{
			if (KV.first.size() < 2 || KV.first[0] != 'd')
			{
				continue;
			}
			FDay D;
			D.Day = std::atoi(KV.first.c_str() + 1);
			for (const std::string& Line : Split(KV.second, '\n'))
			{
				if (StartsWith(Line, "w "))
				{
					std::istringstream Is(Line.substr(2));
					int Rb = 0, Au = 0;
					Is >> D.Hours >> D.RainH >> D.DrizzleH >> D.SnowH >> D.ThunderH >> D.FogH >> D.CloudSum >> D.TMin >> D.TMax >> Rb >> Au >> D.LastHour;
					D.bRainbow = Rb != 0;
					D.bAurora = Au != 0;
				}
				else if (StartsWith(Line, "t\t"))
				{
					const std::vector<std::string> F = Split(Line, '\t');
					if (F.size() < 3)
					{
						continue;
					}
					FHit H;
					H.Topic = F[1];
					H.Count = std::atoi(F[2].c_str());
					H.Targets.assign(F.begin() + 3, F.end());
					D.Hits.push_back(H);
				}
			}
			Days.push_back(D);
		}
		std::sort(Days.begin(), Days.end(), [](const FDay& A, const FDay& B) { return A.Day < B.Day; });
	}
}
