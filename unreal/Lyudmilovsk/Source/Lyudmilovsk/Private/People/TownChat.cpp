#include "TownChat.h"

#include "MiniJson.h"
#include "PeopleSim.h"
#include "TownBarks.h"

#include <algorithm>
#include <cmath>

namespace LyudTownChat
{
	using LyudPeople::EPose;
	using LyudPeople::EState;
	using LyudPeople::FDuty;
	using LyudPeople::FPerson;
	using LyudPeople::FWorld;

	namespace
	{
		bool Has(const std::vector<std::string>& V, const char* S)
		{
			return std::find(V.begin(), V.end(), S) != V.end();
		}

		/** A step of a small generator: 0 .. 1. */
		float Rand01(uint32_t& S)
		{
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			return (float)(S & 0xFFFFFFu) / (float)0x1000000u;
		}

		/** A duty that is a job or the school (not a dog's walk or the cats' bowl). */
		bool IsWork(const FDuty& D)
		{
			return D.Activity.compare(0, 5, "work_") == 0 || D.Activity.compare(0, 6, "school") == 0 || D.Activity == "post_round"
				|| D.Activity == "night_watch" || D.Activity.compare(0, 9, "mop_floor") == 0;
		}

		/** The topics' order on the page. */
		int Order(const std::string& Id)
		{
			static const char* const Ids[] = { "day", "work", "festival", "rumour", "weather", "hobby", "kin", "friend", "town" };
			for (int I = 0; I < (int)(sizeof(Ids) / sizeof(Ids[0])); ++I)
			{
				if (Id == Ids[I])
				{
					return I;
				}
			}
			return 99;
		}
	}

	uint32_t Hash(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H != 0u ? H : 1u;
	}

	// ------------------------------------------------------------------ data

	bool FData::Load(const std::string& Json, std::string& Error)
	{
		LyudJson::FValue Root;
		if (!LyudJson::Parse(Json, Root, Error))
		{
			return false;
		}
		*this = FData();
		const LyudJson::FValue& Ru = Root["rules"];
		R.TopicsMin = Ru["topics_min"].I(R.TopicsMin);
		R.TopicsMax = Ru["topics_max"].I(R.TopicsMax);
		R.KidAge = Ru["kid_age"].I(R.KidAge);
		R.KidShyMet = Ru["kid_shy_met"].I(R.KidShyMet);
		R.DidHours = Ru["did_hours"].F(R.DidHours);
		R.AskChance = Ru["ask_chance"].F(R.AskChance);
		for (const auto& F : Root["did"].Fields)
		{
			Did[F.first] = F.second.S();
		}
		for (const auto& F : Root["jobs"].Fields)
		{
			Jobs[F.first] = F.second.S();
		}
		for (const LyudJson::FValue& H : Root["hobbies"].Items)
		{
			Hobbies.push_back(H.S());
		}
		for (const LyudJson::FValue& A : Root["asks"].Items)
		{
			FAsk Ask;
			Ask.Id = A["id"].S();
			Ask.Answers = A["answers"].I(2);
			Ask.MinAge = A["min_age"].I(0);
			Ask.MaxAge = A["max_age"].I(200);
			if (!Ask.Id.empty() && Ask.Answers >= 2)
			{
				Asks.push_back(Ask);
			}
		}
		for (const auto& F : Root["weights"].Fields)
		{
			Weights[F.first] = F.second.F(1.f);
		}
		for (const LyudJson::FValue& N : Root["names"].Items)
		{
			Names.push_back(N.S());
		}
		std::sort(Names.begin(), Names.end());
		if (R.TopicsMin < 1 || R.TopicsMax < R.TopicsMin)
		{
			Error = "town_chat.json: rules.topics_min / topics_max";
			return false;
		}
		return true;
	}

	bool FData::HasName(const std::string& Id) const
	{
		return std::binary_search(Names.begin(), Names.end(), Id);
	}

	float FData::Weight(const std::string& Topic) const
	{
		const auto It = Weights.find(Topic);
		return It != Weights.end() ? It->second : 1.f;
	}

	// ------------------------------------------------------------------ who may be talked to

	const char* RefuseEvent(ERefuse R)
	{
		switch (R)
		{
		case ERefuse::Work: return "tc_no_work";
		case ERefuse::Hurry: return "tc_no_hurry";
		case ERefuse::Shy: return "tc_no_shy";
		default: return "";
		}
	}

	ERefuse CanChat(const FWorld& W, const FPerson& P, const FData& D, int Met, int Level)
	{
		if (!LyudBarks::CanSpeak(W, P) || P.bExternal || P.Pose == EPose::Shovel || P.Pose == EPose::Skate || P.Pose == EPose::Walrus)
		{
			return ERefuse::Gone;
		}
		if (P.BikeMode != 0 || P.BikeAct != LyudPeople::EBikeAct::None || P.bTrain || P.State == EState::Waiting)
		{
			return ERefuse::Riding;
		}
		// somebody at their counter with a customer in front of them: not now
		if (P.State == EState::Doing && P.Activity >= 0 && P.Activity < (int)W.Activities.size() && !W.Activities[(size_t)P.Activity].bFree
			&& P.Place >= 0)
		{
			for (const FPerson& Q : W.People)
			{
				if (Q.Place == P.Place && Q.Id != P.Id && LyudBarks::AtCounter(W, P, Q))
				{
					return ERefuse::Work;
				}
			}
		}
		if (P.bHurry && P.State == EState::Going)
		{
			return ERefuse::Hurry;
		}
		if (P.Age < D.R.KidAge && Met < D.R.KidShyMet && Level < 1)
		{
			return ERefuse::Shy;
		}
		return ERefuse::None;
	}

	// ------------------------------------------------------------------ whom they speak of

	std::string NameId(const std::string& PersonKey)
	{
		const size_t At = PersonKey.rfind('_');
		// (c_: a cruising townsperson's twin aboard the motor ship, ship_people.json's twin_of: the same person, the same name)
		const bool bTownsperson = PersonKey.compare(0, 2, "t_") == 0 || PersonKey.compare(0, 2, "c_") == 0;
		return bTownsperson && At != std::string::npos && At > 1 ? PersonKey.substr(At + 1) : std::string();
	}

	const char* KinEvent(const FPerson& P, const FPerson& O)
	{
		const bool bGrown = P.Age >= 18, bOtherGrown = O.Age >= 18;
		if (bGrown && !bOtherGrown)
		{
			return O.bFemale ? "tc_kin_daughter" : "tc_kin_son";
		}
		if (!bGrown && bOtherGrown)
		{
			return O.bFemale ? "tc_kin_mum" : "tc_kin_dad";
		}
		if (!bGrown)
		{
			return O.bFemale ? "tc_kin_sister" : "tc_kin_brother";
		}
		// a couple: a man and a woman of about one age, neither of them of the flatmates' years (town_life.py household_plan)
		if (P.bFemale != O.bFemale && P.Age >= 28 && O.Age >= 28 && std::abs(P.Age - O.Age) <= 7)
		{
			return O.bFemale ? "tc_kin_wife" : "tc_kin_husband";
		}
		return "tc_kin_mate";
	}

	const char* KinTitle(const char* Event)
	{
		const std::string E = Event ? Event : "";
		if (E == "tc_kin_son" || E == "tc_kin_daughter")
		{
			return "kids";
		}
		if (E == "tc_kin_mum" || E == "tc_kin_dad")
		{
			return "parents";
		}
		if (E == "tc_kin_sister" || E == "tc_kin_brother")
		{
			return "sibling";
		}
		return E == "tc_kin_wife" || E == "tc_kin_husband" ? "spouse" : "mate";
	}

	int KinOf(const FWorld& W, const FPerson& P, const FData& D, uint32_t Seed)
	{
		if (P.Household.empty())
		{
			return -1;
		}
		int N = 0, Best = -1;
		for (const FPerson& Q : W.People)
		{
			if (Q.Id != P.Id && Q.bTown && Q.Household == P.Household && D.HasName(NameId(Q.Key)))
			{
				// one of them, each as likely (a reservoir of one)
				N++;
				if (Seed % (uint32_t)N == 0u)
				{
					Best = Q.Id;
				}
				Seed = Seed * 1664525u + 1013904223u;
			}
		}
		return Best;
	}

	int FriendOf(const FWorld& W, const FPerson& P, const FData& D, uint32_t Seed)
	{
		if (P.Friends.empty())
		{
			return -1;
		}
		const size_t First = (size_t)(Seed % (uint32_t)P.Friends.size());
		for (size_t I = 0; I < P.Friends.size(); ++I)
		{
			const std::string& Key = P.Friends[(First + I) % P.Friends.size()];
			if (!D.HasName(NameId(Key)))
			{
				continue;
			}
			const int Id = W.FindPerson(Key);
			if (Id >= 0 && Id != P.Id && W.People[(size_t)Id].bTown && W.People[(size_t)Id].Household != P.Household)
			{
				return Id;
			}
		}
		return -1;
	}

	// ------------------------------------------------------------------ the talk

	void FTalk::Reset()
	{
		Person = -1;
		Hello.clear();
		Bye.clear();
		Topics.clear();
		Ask = -1;
		AskAfter = 1;
	}

	bool HasLine(const LyudBarks::FBook& Book, const LyudBarks::FWho& Who, const LyudBarks::FNow& Now, const std::string& Event)
	{
		for (const LyudBarks::FSet& S : Book.Sets)
		{
			if (S.Kind == LyudBarks::EKind::Event && S.On == Event && !S.Lines.empty() && Book.MatchWho(S.A, Who) && Book.MatchNow(S, Now))
			{
				return true;
			}
		}
		return false;
	}

	std::string DayEvent(const FWorld& W, const FPerson& P, const FData& D, const LyudBarks::FBook& Book, const LyudBarks::FWho& Who,
		const LyudBarks::FNow& Now, uint32_t Seed)
	{
		// what they did before this, while it is still news
		std::string Did;
		if (P.PrevActivity >= 0 && P.PrevActivity < (int)W.Activities.size() && W.Time - P.PrevActivityEnd < (double)D.R.DidHours * 3600.0)
		{
			const auto It = D.Did.find(W.Activities[(size_t)P.PrevActivity].Id);
			if (It != D.Did.end() && HasLine(Book, Who, Now, "tc_did_" + It->second))
			{
				Did = "tc_did_" + It->second;
			}
		}
		// their job or school today: still ahead, behind them, or a day off
		std::string Duty;
		bool bAnyWork = false, bToday = false;
		for (const FDuty& Du : P.Duties)
		{
			if (!IsWork(Du))
			{
				continue;
			}
			bAnyWork = true;
			if ((Du.Days & (1 << W.Weekday)) == 0 || Du.To <= Du.From)
			{
				continue;
			}
			bToday = true;
			if (W.Hour < Du.From && W.Hour > Du.From - 6.f)
			{
				Duty = "tc_day_before_work";
			}
			else if (W.Hour >= Du.To)
			{
				Duty = "tc_day_after_work";
			}
		}
		if (bAnyWork && !bToday)
		{
			Duty = "tc_day_off";
		}
		if (!Duty.empty() && !HasLine(Book, Who, Now, Duty))
		{
			Duty.clear();
		}
		// one of the three, so that the same person has other news another day
		if (!Did.empty() && Rand01(Seed) < 0.5f)
		{
			return Did;
		}
		return !Duty.empty() && Rand01(Seed) < 0.6f ? Duty : std::string("tc_day");
	}

	void Build(const FWorld& W, const FPerson& P, const FData& D, const LyudBarks::FBook& Book, const LyudBarks::FWho& Who,
		const LyudBarks::FNow& Now, int Day, FTalk& Out)
	{
		Out.Reset();
		Out.Person = P.Id;
		Out.Hello = "tc_hello";
		Out.Bye = "tc_bye";
		// the same talk through a part of the day (a third of it), another one after, another tomorrow
		uint32_t Seed = Hash(P.Key) ^ ((uint32_t)Day * 2654435761u) ^ ((uint32_t)(Now.Hour / 8.f) * 40503u);
		Seed = Seed != 0u ? Seed : 1u;
		Rand01(Seed);
		const bool bYoung = P.Age < 18;
		const char* Young = bYoung ? ".young" : "";

		struct FCand { FTopic T; float Weight; };
		std::vector<FCand> Cands;
		Cands.reserve(9);
		auto Add = [&](const char* Id, const std::string& Title, const std::string& Event, int Other, float More)
		{
			if (!Event.empty() && HasLine(Book, Who, Now, Event))
			{
				FCand C;
				C.T.Id = Id;
				C.T.Title = "townchat.topic." + Title + Young;
				C.T.Event = Event;
				C.T.Other = Other;
				C.Weight = D.Weight(Id) * More;
				Cands.push_back(C);
			}
		};
		Add("day", Now.Hour < 11.f ? "day.morning" : (Now.Hour >= 17.f ? "day.evening" : "day"), DayEvent(W, P, D, Book, Who, Now, Seed), -1, 1.f);
		{
			const auto It = D.Jobs.find(P.Occupation);
			Add("work", "work." + (It != D.Jobs.end() ? It->second : std::string("work")), "tc_work", -1, 1.f);
		}
		if (!Now.Festival.empty())
		{
			Add("festival", "festival", "tc_festival", -1, 1.f);
		}
		if (!Who.Heard.empty())
		{
			Add("rumour", "rumour", "tc_rumour", -1, 1.f);
		}
		{
			const bool bSky = Has(Now.Weather, "rain") || Has(Now.Weather, "snow") || Has(Now.Weather, "fog") || Has(Now.Weather, "warm") || Has(Now.Weather, "cold");
			Add("weather", "weather", "tc_weather", -1, bSky ? 1.8f : 1.f);
		}
		if (!D.Hobbies.empty())
		{
			// one of the things they like that there are words for
			const size_t First = (size_t)(Seed % (uint32_t)D.Hobbies.size());
			for (size_t I = 0; I < D.Hobbies.size(); ++I)
			{
				const std::string& Hobby = D.Hobbies[(First + I) % D.Hobbies.size()];
				bool bLikes = false;
				for (const auto& L : P.Likes)
				{
					bLikes |= L.first == Hobby && L.second > 0.f;
				}
				if (bLikes && HasLine(Book, Who, Now, "tc_hobby_" + Hobby))
				{
					Add("hobby", "hobby", "tc_hobby_" + Hobby, -1, 1.f);
					break;
				}
			}
		}
		{
			const int Kin = KinOf(W, P, D, Seed >> 3);
			if (Kin >= 0)
			{
				const char* Event = KinEvent(P, W.People[(size_t)Kin]);
				Add("kin", std::string("kin.") + KinTitle(Event), Event, Kin, 1.f);
			}
			const int Friend = FriendOf(W, P, D, Seed >> 5);
			if (Friend >= 0)
			{
				Add("friend", "friend", "tc_friend", Friend, 1.f);
			}
		}
		Add("town", "town", "tc_town", -1, Who.Met == "new" || Who.Met.empty() ? 1.6f : 1.f);

		// how many: the warm have more to say, the reserved less
		int Want = D.R.TopicsMin + (int)(Rand01(Seed) * (float)(D.R.TopicsMax - D.R.TopicsMin + 1));
		Want += Has(Who.Temper, "warm") ? 1 : 0;
		Want -= Has(Who.Temper, "reserved") ? 1 : 0;
		Want = std::max(D.R.TopicsMin, std::min(D.R.TopicsMax, Want));
		while ((int)Out.Topics.size() < Want && !Cands.empty())
		{
			float Sum = 0.f;
			for (const FCand& C : Cands)
			{
				Sum += C.Weight;
			}
			float Roll = Rand01(Seed) * Sum;
			size_t Pick = Cands.size() - 1;
			for (size_t I = 0; I < Cands.size(); ++I)
			{
				Roll -= Cands[I].Weight;
				if (Roll <= 0.f)
				{
					Pick = I;
					break;
				}
			}
			Out.Topics.push_back(Cands[Pick].T);
			Cands.erase(Cands.begin() + (std::ptrdiff_t)Pick);
		}
		std::stable_sort(Out.Topics.begin(), Out.Topics.end(), [](const FTopic& A, const FTopic& B) { return Order(A.Id) < Order(B.Id); });

		// a question back, now and then
		float Chance = D.R.AskChance + (Has(Who.Temper, "curious") ? 0.25f : 0.f) + (Has(Who.Temper, "warm") ? 0.1f : 0.f)
			- (Has(Who.Temper, "reserved") ? 0.2f : 0.f);
		if (!Out.Topics.empty() && !D.Asks.empty() && Rand01(Seed) < Chance)
		{
			const size_t First = (size_t)(Seed % (uint32_t)D.Asks.size());
			for (size_t I = 0; I < D.Asks.size(); ++I)
			{
				const size_t Ix = (First + I) % D.Asks.size();
				const FAsk& A = D.Asks[Ix];
				if (P.Age >= A.MinAge && P.Age <= A.MaxAge && HasLine(Book, Who, Now, "tc_ask_" + A.Id))
				{
					Out.Ask = (int)Ix;
					Out.AskAfter = std::min((int)Out.Topics.size(), 1 + (int)((Seed >> 9) % 2u));
					break;
				}
			}
		}
	}
}
