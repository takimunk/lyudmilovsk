// The talk of the town about the hero (docs/town_rumours.md, backlog item 22), in plain C++ (no Unreal types) so
// tools/barksim can test it on the town's people outside the engine.
//
// Something notable the hero does (a cup won, a regular beaten, a bird tamed, a first shift at a job: game events that
// ULyudQuests::Notify already carries) becomes a rumour by the rules of Content/PeopleData/rumours.json (tools/rumours).
// Whoever was close by knows it at once; from them it goes along the town's ties: that evening to their household, their
// workmates and the friends who live near (a friend across town hears it a day later); each further hop takes a day more,
// at most a few hops and not along every tie. A rumour lives a few game days and is gone. Each resident remarks on any one rumour at most once,
// and on at most one rumour a day; the older the rumour, the less likely they bring it up at all.
//
// Only keys live here: a rumour's detail is a string key (the bird's name, the beaten regular's name, the cup's title) the
// game shows in the speaker's language. ULyudTownRumours (Public/People/LyudTownRumours.h) keeps the store in the game.
#pragma once

#include "MiniJson.h"
#include "PeopleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace LyudRumour
{
	/** What a kind of rumour is like (rumours.json "kinds"). */
	struct FKind
	{
		std::string Id;
		int Hops = 3;           // how far it goes from those who saw it
		int Life = 4;           // game days it lives (the day it happened is day 0)
		float Pass = 0.6f;      // the chance a tie passes it on
		int Seed = 0;           // made-up witnesses (somebody from the train): so many residents know it at once
		float News = 0.f;       // the share of the town that hears it anyway the next day (the paper prints it, the radio)
		bool bNewsToday = false;    // and on the same day (the radio itself)
		bool bOnce = false;     // the same detail is a rumour only once ever (a bird tamed is news the first time)
		int Rank = 1;           // which rumour the paper and the letters pick first
	};

	/** How a game event becomes a rumour (rumours.json "rules", first match wins). Target: "*" any, "pre*" a prefix, or the
	 *  target itself. Detail: a key with {t} (the target) and {p} (the target's part after its '/') put in. */
	struct FRule
	{
		std::string Event;
		std::string Target;
		std::string Kind;
		std::string Detail;
	};

	/** One rumour. Witnesses: person ids of those who knew it at once. Told: who has remarked on it already. */
	struct FRumour
	{
		int Kind = -1;
		std::string Detail;
		int Day = 0;            // the day since the game began it happened on
		int Minute = 0;         // and the minute of that day
		uint32_t Seed = 1;      // which ties pass it on (fixed per rumour, so a load gives the same spread)
		std::vector<int> Witnesses;
		std::vector<int> Told;
		std::vector<int16_t> HeardOn;   // per person: the day offset they heard it (-1 never); built by Spread
	};

	/** The town's ties for the spreading: per person, the people they pass things to and after how many days. */
	struct FTies
	{
		struct FTie { int To = -1; uint8_t Days = 0; };
		std::vector<std::vector<FTie>> Of;
		std::vector<uint8_t> bResident;     // a townsperson who can hear things (not away for good)
		int Num() const { return (int)Of.size(); }
	};

	inline uint32_t Mix(uint32_t X)
	{
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15; X *= 0x846ca68bU; X ^= X >> 16;
		return X;
	}
	inline uint32_t Mix3(uint32_t A, uint32_t B, uint32_t C) { return Mix(A * 0x9E3779B1u ^ Mix(B + 0x85EBCA77u) ^ Mix(C + 0xC2B2AE3Du)); }
	inline float Unit(uint32_t H) { return (float)(H >> 8) / 16777216.f; }
	inline uint32_t HashStr(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H;
	}

	/** The ties of the town's people: household (same day), workmates at one workplace (same day; a big one, a school, ties
	 *  each to a few of the others only), friends (same day when their homes are within FarM, else a day later). */
	inline void BuildTies(const LyudPeople::FWorld& W, FTies& Out, float FarM = 200.f, int BigGroup = 12, int GroupLinks = 4)
	{
		const int N = (int)W.People.size();
		Out.Of.assign((size_t)N, {});
		Out.bResident.assign((size_t)N, 0);
		auto Add = [&Out](int A, int B, uint8_t Days)
		{
			if (A == B || A < 0 || B < 0)
			{
				return;
			}
			for (FTies::FTie& T : Out.Of[(size_t)A])
			{
				if (T.To == B)
				{
					T.Days = std::min(T.Days, Days);
					return;
				}
			}
			Out.Of[(size_t)A].push_back({ B, Days });
		};
		std::map<std::string, std::vector<int>> Households, Works;
		std::map<std::string, int> ByKey;
		for (const LyudPeople::FPerson& P : W.People)
		{
			ByKey[P.Key] = P.Id;
			Out.bResident[(size_t)P.Id] = 1;
			if (!P.Household.empty())
			{
				Households[P.Household].push_back(P.Id);
			}
			for (const LyudPeople::FDuty& D : P.Duties)
			{
				if (!D.PlaceId.empty() && D.Activity != "sleep")
				{
					std::vector<int>& G = Works[D.PlaceId];
					if (G.empty() || G.back() != P.Id)
					{
						G.push_back(P.Id);
					}
				}
			}
		}
		for (const auto& H : Households)
		{
			for (const int A : H.second)
			{
				for (const int B : H.second)
				{
					Add(A, B, 0);
				}
			}
		}
		for (const auto& G : Works)
		{
			const std::vector<int>& M = G.second;
			if ((int)M.size() <= BigGroup)
			{
				for (const int A : M)
				{
					for (const int B : M)
					{
						Add(A, B, 0);
					}
				}
				continue;
			}
			const uint32_t GH = HashStr(G.first);
			for (size_t I = 0; I < M.size(); ++I)
			{
				for (int L = 0; L < GroupLinks; ++L)
				{
					const int J = (int)(Mix3(GH, (uint32_t)I, (uint32_t)L) % (uint32_t)M.size());
					Add(M[I], M[(size_t)J], 0);
					Add(M[(size_t)J], M[I], 0);
				}
			}
		}
		auto HomeAt = [&W](const LyudPeople::FPerson& P, LyudPeople::V3& At) -> bool
		{
			int Pl = P.Home;
			if (Pl < 0 && !P.HomeId.empty())
			{
				Pl = W.FindPlace(P.HomeId);
			}
			if (Pl < 0 || Pl >= (int)W.Places.size())
			{
				return false;
			}
			At = W.Places[(size_t)Pl].Portal;
			return true;
		};
		for (const LyudPeople::FPerson& P : W.People)
		{
			LyudPeople::V3 A;
			const bool bA = HomeAt(P, A);
			for (const std::string& F : P.Friends)
			{
				const auto It = ByKey.find(F);
				if (It == ByKey.end())
				{
					continue;
				}
				const LyudPeople::FPerson& Q = W.People[(size_t)It->second];
				LyudPeople::V3 B;
				const bool bNear = bA && HomeAt(Q, B) && std::hypot(A.X - B.X, A.Y - B.Y) <= FarM;
				Add(P.Id, Q.Id, bNear ? 0 : 1);
				Add(Q.Id, P.Id, bNear ? 0 : 1);
			}
		}
	}

	class FTown
	{
	public:
		std::vector<FKind> Kinds;
		std::vector<FRule> Rules;
		float FarM = 200.f;
		int BigGroup = 12, GroupLinks = 4;
		int MaxRumours = 16;
		float WitnessM = 30.f;          // whoever is this close to the hero when it happens knows it at once (the game's to ask)
		float RemarkEveryS = 150.f;     // the game's pause between two remarks about the hero anywhere
		std::map<std::string, int> PaperLines;  // kind -> how many column lines the paper has for it (rumour.paper.<kind>.<n>)
		std::vector<std::string> PsKinds;       // the kinds a townsperson's letter adds a P.S. about (rumour.ps.<kind>)
		/** How likely somebody who heard it brings it up, by its age in days (the last one for anything older). */
		std::vector<float> Fresh = { 1.f, 0.8f, 0.5f, 0.3f };
		std::string LastError;

		std::vector<FRumour> Rumours;
		/** "kind|detail" of the bOnce kinds already told: never again. */
		std::vector<std::string> Spent;

		/** Reads rumours.json. */
		bool Load(const std::string& Text)
		{
			LyudJson::FValue Root;
			if (!LyudJson::Parse(Text, Root, LastError))
			{
				return false;
			}
			Kinds.clear();
			Rules.clear();
			for (const auto& F : Root["kinds"].Fields)
			{
				FKind K;
				K.Id = F.first;
				K.Hops = F.second["hops"].I(3);
				K.Life = std::max(1, F.second["life"].I(4));
				K.Pass = F.second["pass"].F(0.6f);
				K.Seed = F.second["seed"].I(0);
				K.News = F.second["news"].F(0.f);
				K.bNewsToday = F.second["news_today"].F(0.f) > 0.f;
				K.bOnce = F.second["once"].F(0.f) > 0.f;
				K.Rank = F.second["rank"].I(1);
				Kinds.push_back(K);
			}
			for (const LyudJson::FValue& R : Root["rules"].Items)
			{
				FRule Rule{ R["event"].S(), R["target"].S("*"), R["kind"].S(), R["detail"].S() };
				if (FindKind(Rule.Kind) < 0)
				{
					LastError = "rule for " + Rule.Event + ": unknown kind " + Rule.Kind;
					return false;
				}
				Rules.push_back(Rule);
			}
			FarM = Root["far_m"].F(FarM);
			BigGroup = Root["big_group"].I(BigGroup);
			GroupLinks = Root["group_links"].I(GroupLinks);
			MaxRumours = Root["max_rumours"].I(MaxRumours);
			WitnessM = Root["witness_m"].F(WitnessM);
			RemarkEveryS = Root["remark_every_s"].F(RemarkEveryS);
			PaperLines.clear();
			for (const auto& F : Root["paper"].Fields)
			{
				PaperLines[F.first] = F.second.I(0);
			}
			PsKinds.clear();
			for (const LyudJson::FValue& V : Root["ps"].Items)
			{
				PsKinds.push_back(V.S());
			}
			if (Root["fresh"].Size() > 0)
			{
				Fresh.clear();
				for (const LyudJson::FValue& V : Root["fresh"].Items)
				{
					Fresh.push_back(V.F(0.f));
				}
			}
			if (Kinds.empty())
			{
				LastError = "no kinds";
				return false;
			}
			return true;
		}

		int FindKind(const std::string& Id) const
		{
			for (int I = 0; I < (int)Kinds.size(); ++I)
			{
				if (Kinds[(size_t)I].Id == Id)
				{
					return I;
				}
			}
			return -1;
		}
		const FKind* KindOf(const FRumour& R) const { return R.Kind >= 0 && R.Kind < (int)Kinds.size() ? &Kinds[(size_t)R.Kind] : nullptr; }

		/** The rule a game event matches: its kind and the detail key; false: not news. */
		bool Match(const std::string& Event, const std::string& Target, int& OutKind, std::string& OutDetail) const
		{
			for (const FRule& R : Rules)
			{
				if (R.Event != Event)
				{
					continue;
				}
				const bool bAny = R.Target == "*";
				const bool bPrefix = !bAny && !R.Target.empty() && R.Target.back() == '*';
				if (!(bAny || (bPrefix ? Target.compare(0, R.Target.size() - 1, R.Target, 0, R.Target.size() - 1) == 0 : Target == R.Target)))
				{
					continue;
				}
				OutKind = FindKind(R.Kind);
				const size_t Slash = Target.find('/');
				const std::string Piece = Slash == std::string::npos ? Target : Target.substr(Slash + 1);
				OutDetail = R.Detail;
				for (const auto& Sub : { std::make_pair(std::string("{t}"), Target), std::make_pair(std::string("{p}"), Piece) })
				{
					for (size_t At = OutDetail.find(Sub.first); At != std::string::npos; At = OutDetail.find(Sub.first, At + Sub.second.size()))
					{
						OutDetail.replace(At, Sub.first.size(), Sub.second);
					}
				}
				return OutKind >= 0;
			}
			return false;
		}

		/**
		 * Something happened: a rumour of Kind with Detail on Day at Minute, known at once by Witnesses. -1: not added (the same
		 * one is still going round, or a bOnce detail was news already). Otherwise its index. Ties: the town's (BuildTies).
		 */
		int Add(int Kind, const std::string& Detail, int Day, int Minute, const std::vector<int>& Witnesses, const FTies& Ties)
		{
			if (Kind < 0 || Kind >= (int)Kinds.size())
			{
				return -1;
			}
			const FKind& K = Kinds[(size_t)Kind];
			const std::string Sig = K.Id + "|" + Detail;
			if (K.bOnce && std::find(Spent.begin(), Spent.end(), Sig) != Spent.end())
			{
				return -1;
			}
			Expire(Day);
			for (const FRumour& R : Rumours)
			{
				if (R.Kind == Kind && R.Detail == Detail)
				{
					return -1;      // still going round: no fresh start for the same news
				}
			}
			if (K.bOnce)
			{
				Spent.push_back(Sig);
			}
			FRumour R;
			R.Kind = Kind;
			R.Detail = Detail;
			R.Day = Day;
			R.Minute = Minute;
			R.Seed = Mix3(HashStr(Sig), (uint32_t)Day, (uint32_t)Minute) | 1u;
			for (const int W : Witnesses)
			{
				if (W >= 0 && W < Ties.Num() && std::find(R.Witnesses.begin(), R.Witnesses.end(), W) == R.Witnesses.end())
				{
					R.Witnesses.push_back(W);
				}
			}
			// made-up witnesses (the train's passengers, the sanatorium's guests): a few residents picked by the rumour's seed
			for (int S = 0, Tries = 0; S < K.Seed && Tries < 64 && Ties.Num() > 0; ++Tries)
			{
				const int P = (int)(Mix3(R.Seed, 77u, (uint32_t)Tries) % (uint32_t)Ties.Num());
				if (Ties.bResident[(size_t)P] && std::find(R.Witnesses.begin(), R.Witnesses.end(), P) == R.Witnesses.end())
				{
					R.Witnesses.push_back(P);
					++S;
				}
			}
			Spread(R, Ties);
			// the oldest goes when the store is full
			if ((int)Rumours.size() >= MaxRumours)
			{
				Rumours.erase(Rumours.begin());
			}
			Rumours.push_back(std::move(R));
			return (int)Rumours.size() - 1;
		}

		/** Works out when each person hears R (HeardOn): its witnesses on day 0, then along the ties that pass it, within Hops
		 *  (the first hop takes the tie's days, every later one at least a day); the town at large by News. Fixed by R.Seed: the
		 *  same every time it is built. */
		void Spread(FRumour& R, const FTies& Ties) const
		{
			const FKind* K = KindOf(R);
			const int N = Ties.Num();
			R.HeardOn.assign((size_t)N, (int16_t)-1);
			if (!K)
			{
				return;
			}
			for (const int W : R.Witnesses)
			{
				if (W >= 0 && W < N)
				{
					R.HeardOn[(size_t)W] = 0;
				}
			}
			// relax round by round: a round is one hop, the days add up along the way (the earliest day within Hops wins)
			std::vector<int> Front = R.Witnesses;
			for (int H = 1; H <= K->Hops && !Front.empty(); ++H)
			{
				std::vector<int> Next;
				for (const int A : Front)
				{
					if (A < 0 || A >= N)
					{
						continue;
					}
					for (const FTies::FTie& T : Ties.Of[(size_t)A])
					{
						if (!Ties.bResident[(size_t)T.To] || Unit(Mix3(R.Seed, (uint32_t)A, (uint32_t)T.To)) >= K->Pass)
						{
							continue;
						}
						const int16_t Day = (int16_t)(R.HeardOn[(size_t)A] + std::max<int>(T.Days, H >= 2 ? 1 : 0));
						int16_t& Cur = R.HeardOn[(size_t)T.To];
						if (Cur < 0 || Day < Cur)
						{
							Cur = Day;
							Next.push_back(T.To);
						}
					}
				}
				Front.swap(Next);
			}
			if (K->News > 0.f)
			{
				const int16_t Day = K->bNewsToday ? 0 : 1;
				for (int P = 0; P < N; ++P)
				{
					if (Ties.bResident[(size_t)P] && Unit(Mix3(R.Seed, 991u, (uint32_t)P)) < K->News)
					{
						int16_t& Cur = R.HeardOn[(size_t)P];
						Cur = Cur < 0 ? Day : std::min(Cur, Day);
					}
				}
			}
		}

		/** Drops the rumours past their life on Today. */
		void Expire(int Today)
		{
			Rumours.erase(std::remove_if(Rumours.begin(), Rumours.end(), [this, Today](const FRumour& R)
			{
				const FKind* K = KindOf(R);
				return !K || Today - R.Day >= K->Life || Today < R.Day;
			}), Rumours.end());
		}

		/** Does Person know R on Today (heard it by then, and it is still going round)? */
		bool Knows(const FRumour& R, int Person, int Today) const
		{
			const FKind* K = KindOf(R);
			if (!K || Person < 0 || Person >= (int)R.HeardOn.size() || Today - R.Day >= K->Life || Today < R.Day)
			{
				return false;
			}
			const int16_t H = R.HeardOn[(size_t)Person];
			return H >= 0 && R.Day + H <= Today;
		}

		/** Would Person bring R up on Today: knows it, hasn't remarked on it, and is in the mood (less so as it gets old). */
		bool WouldMention(const FRumour& R, int Person, int Today) const
		{
			if (!Knows(R, Person, Today) || std::find(R.Told.begin(), R.Told.end(), Person) != R.Told.end())
			{
				return false;
			}
			const int Age = Today - R.Day;
			const float F = Fresh.empty() ? 1.f : Fresh[(size_t)std::min<int>(Age, (int)Fresh.size() - 1)];
			return Unit(Mix3(R.Seed, (uint32_t)Person, 31337u + (uint32_t)Today)) < F;
		}

		/** The rumours Person would bring up today (indices into Rumours), none if they remarked on one today already. */
		void Mentionable(int Person, int Today, std::vector<int>& Out) const
		{
			Out.clear();
			if (RemarkedToday(Person, Today))
			{
				return;
			}
			for (int I = 0; I < (int)Rumours.size(); ++I)
			{
				if (WouldMention(Rumours[(size_t)I], Person, Today))
				{
					Out.push_back(I);
				}
			}
		}

		/** Person remarked on rumour I today: never again on it, and on nothing else today. */
		void MarkTold(int I, int Person, int Today)
		{
			if (I < 0 || I >= (int)Rumours.size() || Person < 0)
			{
				return;
			}
			std::vector<int>& T = Rumours[(size_t)I].Told;
			if (std::find(T.begin(), T.end(), Person) == T.end())
			{
				T.push_back(Person);
			}
			if ((int)RemarkDay.size() <= Person)
			{
				RemarkDay.resize((size_t)Person + 1, -1);
			}
			RemarkDay[(size_t)Person] = Today;
		}
		bool RemarkedToday(int Person, int Today) const
		{
			return Person >= 0 && Person < (int)RemarkDay.size() && RemarkDay[(size_t)Person] == Today;
		}

		/** How many people know R on Today. */
		int CountKnowing(const FRumour& R, int Today) const
		{
			int N = 0;
			for (int P = 0; P < (int)R.HeardOn.size(); ++P)
			{
				N += Knows(R, P, Today) ? 1 : 0;
			}
			return N;
		}

		/** The talk of the town on Today, best first (by the kind's rank, then the newest): the paper's column and the letters. */
		void Talk(int Today, std::vector<int>& Out, int MaxAge = 2) const
		{
			Out.clear();
			for (int I = 0; I < (int)Rumours.size(); ++I)
			{
				const FRumour& R = Rumours[(size_t)I];
				const FKind* K = KindOf(R);
				if (K && Today >= R.Day && Today - R.Day <= MaxAge && Today - R.Day < K->Life)
				{
					Out.push_back(I);
				}
			}
			std::stable_sort(Out.begin(), Out.end(), [this](int A, int B)
			{
				const FRumour& RA = Rumours[(size_t)A];
				const FRumour& RB = Rumours[(size_t)B];
				const int KA = Kinds[(size_t)RA.Kind].Rank, KB = Kinds[(size_t)RB.Kind].Rank;
				return KA != KB ? KA > KB : (RA.Day != RB.Day ? RA.Day > RB.Day : RA.Minute > RB.Minute);
			});
		}

		/** The paper's column on the morning of Day: up to Max lines about the day before's talk, the kinds that have column
		 *  lines (key rumour.paper.<kind>.<n>, the variant by the rumour and the day; its argument "@<detail>" if it has one). */
		void PaperColumn(int Day, std::vector<std::pair<std::string, std::vector<std::string>>>& Out, int Max = 2) const
		{
			Out.clear();
			std::vector<int> Order;
			Talk(Day - 1, Order, 0);
			for (const int I : Order)
			{
				const FRumour& R = Rumours[(size_t)I];
				const auto It = PaperLines.find(Kinds[(size_t)R.Kind].Id);
				if (It == PaperLines.end() || It->second <= 0 || (int)Out.size() >= Max)
				{
					continue;
				}
				const int V = 1 + (int)(Mix3(R.Seed, (uint32_t)Day, 7u) % (uint32_t)It->second);
				Out.push_back({ "rumour.paper." + It->first + "." + std::to_string(V), R.Detail.empty() ? std::vector<std::string>() : std::vector<std::string>{ "@" + R.Detail } });
			}
		}

		/** A P.S. for a townsperson's letter written on Day: the best of the last days' talk that has one (key rumour.ps.<kind>,
		 *  its argument "@<detail>" if it has one). False: nothing worth a P.S. */
		bool PostScript(int Day, std::string& Key, std::vector<std::string>& Args) const
		{
			std::vector<int> Order;
			Talk(Day, Order, 2);
			for (const int I : Order)
			{
				const FRumour& R = Rumours[(size_t)I];
				const std::string& K = Kinds[(size_t)R.Kind].Id;
				if (std::find(PsKinds.begin(), PsKinds.end(), K) != PsKinds.end())
				{
					Key = "rumour.ps." + K;
					Args = R.Detail.empty() ? std::vector<std::string>() : std::vector<std::string>{ "@" + R.Detail };
					return true;
				}
			}
			return false;
		}

		// --- the save: one line per rumour, the people by their keys (ids may change with the town's data) -----------------

		/** "kind\tdetail\tday\tminute\tseed\twitness,witness\ttold,told" per rumour; "spent\tkind|detail" per spent detail. */
		std::vector<std::string> Save(const LyudPeople::FWorld& W) const
		{
			std::vector<std::string> Out;
			auto Keys = [&W](const std::vector<int>& Ids)
			{
				std::string S;
				for (const int Id : Ids)
				{
					if (Id >= 0 && Id < (int)W.People.size())
					{
						S += (S.empty() ? "" : ",") + W.People[(size_t)Id].Key;
					}
				}
				return S;
			};
			for (const FRumour& R : Rumours)
			{
				const FKind* K = KindOf(R);
				if (!K)
				{
					continue;
				}
				char Nums[64];
				std::snprintf(Nums, sizeof(Nums), "%d\t%d\t%u", R.Day, R.Minute, R.Seed);
				Out.push_back(K->Id + "\t" + R.Detail + "\t" + Nums + "\t" + Keys(R.Witnesses) + "\t" + Keys(R.Told));
			}
			for (const std::string& S : Spent)
			{
				Out.push_back("spent\t" + S);
			}
			return Out;
		}

		/** Reads Save's lines back (unknown kinds and people are skipped) and spreads each rumour again. */
		void Load(const std::vector<std::string>& Lines, const LyudPeople::FWorld& W, const FTies& Ties)
		{
			Rumours.clear();
			Spent.clear();
			RemarkDay.clear();
			std::map<std::string, int> ByKey;
			for (const LyudPeople::FPerson& P : W.People)
			{
				ByKey[P.Key] = P.Id;
			}
			auto Split = [](const std::string& S, char C)
			{
				std::vector<std::string> Parts;
				size_t I = 0;
				while (true)
				{
					const size_t E = S.find(C, I);
					Parts.push_back(S.substr(I, E == std::string::npos ? std::string::npos : E - I));
					if (E == std::string::npos)
					{
						break;
					}
					I = E + 1;
				}
				return Parts;
			};
			auto Ids = [&ByKey, &Split](const std::string& S)
			{
				std::vector<int> Out;
				if (S.empty())
				{
					return Out;
				}
				for (const std::string& K : Split(S, ','))
				{
					const auto It = ByKey.find(K);
					if (It != ByKey.end())
					{
						Out.push_back(It->second);
					}
				}
				return Out;
			};
			for (const std::string& L : Lines)
			{
				const std::vector<std::string> P = Split(L, '\t');
				if (P.size() == 2 && P[0] == "spent")
				{
					Spent.push_back(P[1]);
					continue;
				}
				if (P.size() < 7)
				{
					continue;
				}
				FRumour R;
				R.Kind = FindKind(P[0]);
				if (R.Kind < 0)
				{
					continue;
				}
				R.Detail = P[1];
				R.Day = std::atoi(P[2].c_str());
				R.Minute = std::atoi(P[3].c_str());
				R.Seed = (uint32_t)std::strtoul(P[4].c_str(), nullptr, 10) | 1u;
				R.Witnesses = Ids(P[5]);
				R.Told = Ids(P[6]);
				Spread(R, Ties);
				Rumours.push_back(std::move(R));
			}
		}

	private:
		std::vector<int> RemarkDay;     // per person: the day they last remarked on a rumour (not saved: a day at most)
	};
}
