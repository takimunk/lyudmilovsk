#include "Friends.h"

#include "MiniJson.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace LyudFriends
{
	namespace
	{
		std::vector<std::string> Strings(const LyudJson::FValue& V)
		{
			std::vector<std::string> Out;
			for (const LyudJson::FValue& S : V.Items)
			{
				Out.push_back(S.S());
			}
			return Out;
		}

		bool Has(const std::vector<std::string>& V, const std::string& S)
		{
			return std::find(V.begin(), V.end(), S) != V.end();
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
					Cur += C;
				}
			}
			Out.push_back(Cur);
			return Out;
		}

		std::string Join(const std::set<std::string>& S, char By)
		{
			std::string Out;
			for (const std::string& X : S)
			{
				Out += (Out.empty() ? "" : std::string(1, By)) + X;
			}
			return Out;
		}

		const char* AgeGroup(int Age)
		{
			return Age < 12 ? "kid" : (Age < 18 ? "teen" : (Age < 60 ? "adult" : "elder"));
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
		*this = FData();
		const LyudJson::FValue& Ru = Root["rules"];
		if (Ru["levels"].Size() == (size_t)(MaxLevel + 1))
		{
			for (int L = 0; L <= MaxLevel; ++L)
			{
				R.Levels[L] = Ru["levels"][(size_t)L].I(R.Levels[L]);
			}
		}
		R.Greet = Ru["greet"].I(R.Greet);
		R.Heard = Ru["heard"].I(R.Heard);
		R.HeardCap = Ru["heard_cap"].I(R.HeardCap);
		R.Visit = Ru["visit"].I(R.Visit);
		R.Quest = Ru["quest"].I(R.Quest);
		R.GiftLoved = Ru["gift_loved"].I(R.GiftLoved);
		R.GiftLiked = Ru["gift_liked"].I(R.GiftLiked);
		R.GiftPlain = Ru["gift_plain"].I(R.GiftPlain);
		R.Card = Ru["card"].I(R.Card);
		R.Bench = Ru["bench"].I(R.Bench);
		R.Tea = Ru["tea"].I(R.Tea);
		R.Outing = Ru["outing"].I(R.Outing);
		R.Festival = Ru["festival"].I(R.Festival);
		R.FestivalLevel = Ru["festival_level"].I(R.FestivalLevel);
		R.TeaLevel = Ru["tea_level"].I(R.TeaLevel);
		R.TeaEvery = Ru["tea_every"].I(R.TeaEvery);
		R.TeaAhead = Ru["tea_ahead"].I(R.TeaAhead);
		R.OutingLevel = Ru["outing_level"].I(R.OutingLevel);
		R.OutingEvery = Ru["outing_every"].I(R.OutingEvery);
		R.BenchLevel = Ru["bench_level"].I(R.BenchLevel);
		R.WaveLevel = Ru["wave_level"].I(R.WaveLevel);
		R.NickLevel = Ru["nick_level"].I(R.NickLevel);
		for (const LyudJson::FValue& N : Root["nicknames"].Items)
		{
			Nicknames.emplace_back(N["id"].S(), N["hint"].S());
		}
		for (const LyudJson::FValue& G : Root["gifts"].Items)
		{
			Gifts.push_back({ G["item"].S(), Strings(G["hints"]) });
		}
		for (const LyudJson::FValue& H : Root["hosts"].Items)
		{
			FHost Ho;
			Ho.Id = H["id"].S();
			Ho.Name = H["name"].S();
			Ho.Person = H["person"].S();
			Ho.Correspondent = H["correspondent"].S();
			Ho.Quest = H["quest"].S();
			Ho.Plot = H["plot"].S();
			Ho.Chair = H["chair"].S();
			Ho.Table = H["table"].S();
			for (size_t I = 0; I < 3; ++I)
			{
				Ho.ChairAt[I] = H["chair_ue"][I].F(0.f);
				Ho.HostChairAt[I] = H["host_chair_ue"][I].F(0.f);
			}
			Ho.ChairYaw = H["chair_yaw"].F(0.f);
			Ho.HostChair = H["host_chair"].S();
			Ho.HostChairYaw = H["host_chair_yaw"].F(0.f);
			Ho.HostChairHeight = H["host_chair_height"].F(0.45f);
			Ho.Hour = H["hour"].I(17);
			Ho.Topics = H["topics"].I(3);
			Hosts.push_back(Ho);
		}
		OutingActs = Strings(Root["outing_acts"]);
		for (const auto& F : Root["jobs"].Fields)
		{
			Jobs[F.first] = F.second.S();
		}
		if (Hosts.empty() || Gifts.empty() || Nicknames.empty())
		{
			Error = "friends.json: no hosts, gifts or nicknames";
			return false;
		}
		return true;
	}

	const FHost* FData::HostByKey(const std::string& Key) const
	{
		for (const FHost& H : Hosts)
		{
			if (H.Key() == Key)
			{
				return &H;
			}
		}
		return nullptr;
	}

	const FHost* FData::HostById(const std::string& Id) const
	{
		for (const FHost& H : Hosts)
		{
			if (H.Id == Id)
			{
				return &H;
			}
		}
		return nullptr;
	}

	const FHost* FData::HostByPlot(const std::string& Plot) const
	{
		for (const FHost& H : Hosts)
		{
			if (H.Plot == Plot)
			{
				return &H;
			}
		}
		return nullptr;
	}

	const FHost* FData::HostByQuest(const std::string& Quest) const
	{
		for (const FHost& H : Hosts)
		{
			if (!H.Quest.empty() && H.Quest == Quest)
			{
				return &H;
			}
		}
		return nullptr;
	}

	const FGift* FData::Gift(const std::string& Item) const
	{
		for (const FGift& G : Gifts)
		{
			if (G.Item == Item)
			{
				return &G;
			}
		}
		return nullptr;
	}

	// ------------------------------------------------------------------ who likes what

	std::vector<std::string> Tags(const FWho& Who)
	{
		std::vector<std::string> T;
		T.push_back(AgeGroup(Who.Age));
		for (const std::string& H : Who.Habits)
		{
			T.push_back(H);
		}
		const std::string& J = Who.Job;
		if (J == "fisherman" || J == "fishmonger" || J == "lifeguard" || J == "lighthouse_keeper")
		{
			T.push_back("sea");
			if (J == "fisherman")
			{
				T.push_back("fisherman");
			}
		}
		if (J == "artist" || J == "musician" || J == "writer" || J == "photographer" || J == "dance_teacher" || J == "choir_leader")
		{
			T.push_back("artist");
		}
		if (J == "teacher" || J == "librarian" || J == "bookseller" || J == "student" || J == "writer")
		{
			T.push_back("reader");
		}
		if (Who.Warmth > 0.66f)
		{
			T.push_back("warm");
		}
		if (Who.Curiosity > 0.66f)
		{
			T.push_back("curious");
		}
		if (Who.Tidy > 0.66f)
		{
			T.push_back("tidy");
		}
		if (Who.bHost)
		{
			T.push_back("host");
		}
		return T;
	}

	const char* GiftWord(EGift G)
	{
		switch (G)
		{
		case EGift::Loved: return "loved";
		case EGift::Liked: return "liked";
		case EGift::Plain: return "plain";
		default: return "again";
		}
	}

	EGift FModel::Likes(const std::string& Item, const FWho& Who) const
	{
		const FGift* G = Data.Gift(Item);
		if (!G)
		{
			return EGift::Plain;
		}
		const std::vector<std::string> T = Tags(Who);
		const char* Age = AgeGroup(Who.Age);
		bool bAny = false, bAgeHinted = false, bAgeFits = false, bOtherHinted = false;
		int Other = 0;
		for (const std::string& H : G->Hints)
		{
			if (H == "any")
			{
				bAny = true;
			}
			else if (H == "kid" || H == "teen" || H == "adult" || H == "elder")
			{
				bAgeHinted = true;
				bAgeFits = bAgeFits || H == Age;
			}
			else
			{
				bOtherHinted = true;
				Other += Has(T, H) ? 1 : 0;
			}
		}
		// a toy is for a child: an age that doesn't fit makes it at most a nice thought, whatever else fits
		if (bAgeHinted && !bAgeFits)
		{
			return Other > 0 || bAny ? EGift::Liked : EGift::Plain;
		}
		if (Other > 0 || (bAgeHinted && !bOtherHinted))
		{
			return EGift::Loved;
		}
		return bAgeHinted || bAny ? EGift::Liked : EGift::Plain;
	}

	std::string FModel::PickNick(const FWho& Who) const
	{
		const std::vector<std::string> T = Tags(Who);
		std::vector<const std::string*> Fit;
		for (const auto& N : Data.Nicknames)
		{
			if (N.second == "any" || Has(T, N.second) || (N.second == "birds" && (Who.Curiosity > 0.55f || Has(T, "feeder"))))
			{
				Fit.push_back(&N.first);
			}
		}
		if (Fit.empty())
		{
			return Data.Nicknames.empty() ? std::string() : Data.Nicknames.front().first;
		}
		return *Fit[Hash(Who.Key + "#nick") % Fit.size()];
	}

	// ------------------------------------------------------------------ the bonds

	uint32_t FModel::Hash(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H;
	}

	int FModel::LevelOf(const FRules& R, int Points)
	{
		int L = 0;
		for (int I = 1; I <= MaxLevel; ++I)
		{
			L = Points >= R.Levels[I] ? I : L;
		}
		return L;
	}

	int FModel::Level(const std::string& Key) const
	{
		const FBond* B = Find(Key);
		return B ? LevelOf(Data.R, B->Points) : 0;
	}

	const char* FModel::BondWord(const std::string& Key) const
	{
		switch (Level(Key))
		{
		case 2: return "friendly";
		case 3: return "friend";
		case 4: return "close";
		default: return "";
		}
	}

	const FBond* FModel::Find(const std::string& Key) const
	{
		const auto It = Bonds.find(Key);
		return It != Bonds.end() ? &It->second : nullptr;
	}

	FBond& FModel::At(const std::string& Key, int Day)
	{
		FBond& B = Bonds[Key];
		B.FirstDay = B.FirstDay < 0 ? Day : B.FirstDay;
		B.LastDay = std::max(B.LastDay, Day);
		return B;
	}

	int FModel::Add(const std::string& Key, int Day, int Points)
	{
		At(Key, Day).Points += Points;
		return Points;
	}

	int FModel::Greeted(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		if (B.GreetDay == Day)
		{
			return 0;
		}
		B.GreetDay = Day;
		return Add(Key, Day, Data.R.Greet);
	}

	int FModel::Heard(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		if (B.HeardDay != Day)
		{
			B.HeardDay = Day;
			B.HeardToday = 0;
		}
		if (B.HeardToday >= Data.R.HeardCap)
		{
			return 0;
		}
		++B.HeardToday;
		return Add(Key, Day, Data.R.Heard);
	}

	int FModel::Visited(const std::string& HostKey, int Day)
	{
		FBond& B = At(HostKey, Day);
		if (B.VisitDay == Day)
		{
			return 0;
		}
		B.VisitDay = Day;
		return Add(HostKey, Day, Data.R.Visit);
	}

	int FModel::QuestDone(const std::string& Key, const std::string& Quest, int Day)
	{
		FBond& B = At(Key, Day);
		if (!B.Quests.insert(Quest).second)
		{
			return 0;
		}
		return Add(Key, Day, Data.R.Quest);
	}

	EGift FModel::Give(const std::string& Key, const std::string& Item, const FWho& Who, int Day, int* Added)
	{
		FBond& B = At(Key, Day);
		int Points = 0;
		EGift G = EGift::Again;
		if (B.GiftDay != Day)
		{
			B.GiftDay = Day;
			++B.Gifts;
			G = Likes(Item, Who);
			Points = G == EGift::Loved ? Data.R.GiftLoved : (G == EGift::Liked ? Data.R.GiftLiked : Data.R.GiftPlain);
			if (G == EGift::Loved)
			{
				B.Loves.insert(Item);
			}
			Add(Key, Day, Points);
		}
		if (Added)
		{
			*Added = Points;
		}
		return G;
	}

	int FModel::Card(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		++B.Cards;
		B.CardDay = Day;
		return Add(Key, Day, Data.R.Card);
	}

	int FModel::Bench(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		if (B.BenchDay == Day)
		{
			return 0;
		}
		B.BenchDay = Day;
		++B.Benches;
		return Add(Key, Day, Data.R.Bench);
	}

	int FModel::Tea(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		B.TeaDay = -1;
		B.TeaLast = Day;
		++B.Teas;
		return Add(Key, Day, Data.R.Tea);
	}

	int FModel::Outing(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		if (B.OutingDay != Day)
		{
			return 0;
		}
		B.OutingDay = -1;
		++B.Outings;
		return Add(Key, Day, Data.R.Outing);
	}

	int FModel::Festival(const std::string& Key, int Day)
	{
		const auto It = Bonds.find(Key);
		if (It == Bonds.end() || It->second.FestDay == Day || LevelOf(Data.R, It->second.Points) < Data.R.FestivalLevel)
		{
			return 0;
		}
		It->second.FestDay = Day;
		return Add(Key, Day, Data.R.Festival);
	}

	bool FModel::EnsureNick(const std::string& Key, const FWho& Who)
	{
		const auto It = Bonds.find(Key);
		if (It == Bonds.end() || !It->second.Nick.empty() || LevelOf(Data.R, It->second.Points) < Data.R.NickLevel)
		{
			return false;
		}
		It->second.Nick = PickNick(Who);
		return !It->second.Nick.empty();
	}

	bool FModel::TeaNow(const std::string& Key, int Day, float Hour) const
	{
		const FBond* B = Find(Key);
		return B && B->TeaDay == Day && Hour >= (float)B->TeaHour - 0.5f && Hour < (float)B->TeaHour + 2.5f;
	}

	bool FModel::OutingNow(const std::string& Key, int Day, float Hour, const FOutingInfo& O) const
	{
		const FBond* B = Find(Key);
		return B && B->OutingDay == Day && !O.Act.empty() && Hour >= O.Hour - 0.5f && Hour < O.Hour + 1.5f;
	}

	std::vector<FInvite> FModel::Step(int Day, float Hour, int Weekday, const std::map<std::string, FOutingInfo>& Outings)
	{
		std::vector<FInvite> Out;
		const FRules& R = Data.R;
		for (auto& KV : Bonds)
		{
			FBond& B = KV.second;
			// an invitation whose day has gone by is forgotten (a missed tea waits as long as a tea had)
			if (B.TeaDay >= 0 && (Day > B.TeaDay || (Day == B.TeaDay && Hour >= (float)B.TeaHour + 2.5f)))
			{
				B.TeaDay = -1;
				B.TeaLast = Day;
			}
			if (B.OutingDay >= 0 && Day > B.OutingDay)
			{
				B.OutingDay = -1;
			}
			const int L = LevelOf(R, B.Points);
			// a host's tea: written in the morning (the letter is carried the next post day), on the day after next at their hour
			if (const FHost* H = Data.HostByKey(KV.first))
			{
				if (L >= R.TeaLevel && B.TeaDay < 0 && (B.TeaLast < 0 || Day - B.TeaLast >= R.TeaEvery) && Hour >= 8.f && Hour < 12.f)
				{
					B.TeaDay = Day + R.TeaAhead;
					B.TeaHour = H->Hour;
					Out.push_back({ "tea", KV.first, "friends.tea." + H->Id + ".invite", H->Name, B.TeaDay, H->Hour, 0 });
				}
				continue;
			}
			// a close friend's family: on Wednesday or Thursday for the coming weekend's outing
			const auto O = Outings.find(KV.first);
			if (O == Outings.end() || O->second.Act.empty() || O->second.Weekday < 5 || L < R.OutingLevel || B.OutingDay >= 0
				|| (B.OutingLast >= 0 && Day - B.OutingLast < R.OutingEvery) || (Weekday != 2 && Weekday != 3) || Hour < 8.f || Hour >= 12.f
				|| !Has(Data.OutingActs, O->second.Act))
			{
				continue;
			}
			B.OutingDay = Day + (O->second.Weekday - Weekday);
			B.OutingLast = Day;
			const int Hh = (int)O->second.Hour;
			Out.push_back({ "outing", KV.first, "friends.outing.invite." + O->second.Act, std::string(), B.OutingDay, Hh,
				(int)((O->second.Hour - (float)Hh) * 60.f + 0.5f) });
		}
		return Out;
	}

	void FModel::DevBefriend(const std::string& Key, int Day)
	{
		FBond& B = At(Key, Day);
		B.Points = std::max(B.Points, Data.R.Levels[MaxLevel]);
	}

	bool FModel::DevTeaNow(const std::string& HostKey, int Day, int Hour)
	{
		if (!Data.HostByKey(HostKey))
		{
			return false;
		}
		FBond& B = At(HostKey, Day);
		B.TeaDay = Day;
		B.TeaHour = Hour;
		return true;
	}

	// ------------------------------------------------------------------ the save

	void FModel::Save(std::map<std::string, std::string>& Out) const
	{
		Out.clear();
		Out["v"] = "1";
		for (const auto& KV : Bonds)
		{
			const FBond& B = KV.second;
			char Buf[512];
			std::snprintf(Buf, sizeof Buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", B.Points, B.FirstDay, B.LastDay,
				B.GreetDay, B.HeardDay, B.HeardToday, B.VisitDay, B.GiftDay, B.BenchDay, B.Cards, B.Teas, B.Outings, B.Benches, B.Gifts,
				B.CardDay, B.TeaDay, B.TeaHour, B.TeaLast, B.OutingDay, B.OutingLast, B.LevelShown, B.FestDay);
			Out["b." + KV.first] = std::string(Buf) + "|" + B.Nick + "|" + Join(B.Quests, ',') + "|" + Join(B.Loves, ',');
		}
	}

	void FModel::LoadState(const std::map<std::string, std::string>& In)
	{
		Bonds.clear();
		for (const auto& KV : In)
		{
			if (KV.first.rfind("b.", 0) != 0)
			{
				continue;
			}
			const std::vector<std::string> Parts = Split(KV.second, '|');
			const std::vector<std::string> N = Split(Parts[0], ',');
			if (N.size() < 21)
			{
				continue;
			}
			int V[21];
			for (int I = 0; I < 21; ++I)
			{
				V[I] = std::atoi(N[(size_t)I].c_str());
			}
			FBond B;
			B.Points = V[0]; B.FirstDay = V[1]; B.LastDay = V[2]; B.GreetDay = V[3]; B.HeardDay = V[4]; B.HeardToday = V[5];
			B.VisitDay = V[6]; B.GiftDay = V[7]; B.BenchDay = V[8]; B.Cards = V[9]; B.Teas = V[10]; B.Outings = V[11];
			B.Benches = V[12]; B.Gifts = V[13]; B.CardDay = V[14]; B.TeaDay = V[15]; B.TeaHour = V[16]; B.TeaLast = V[17];
			B.OutingDay = V[18]; B.OutingLast = V[19]; B.LevelShown = V[20];
			B.FestDay = N.size() > 21 ? std::atoi(N[21].c_str()) : -1;     // (saves from before the holidays together have 21)
			if (Parts.size() > 1)
			{
				B.Nick = Parts[1];
			}
			for (size_t P = 2; P < 4 && P < Parts.size(); ++P)
			{
				for (const std::string& S : Split(Parts[P], ','))
				{
					if (!S.empty())
					{
						(P == 2 ? B.Quests : B.Loves).insert(S);
					}
				}
			}
			Bonds[KV.first.substr(2)] = B;
		}
	}
}
