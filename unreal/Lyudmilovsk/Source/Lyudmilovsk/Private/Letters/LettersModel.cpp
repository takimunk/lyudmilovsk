#include "LettersModel.h"

#include "../People/MiniJson.h"

#include <algorithm>
#include <cstdlib>

namespace LyudLetters
{
	// --- the calendar

	int DaysFromCivil(int Y, int M, int D)
	{
		Y -= M <= 2 ? 1 : 0;
		const int Era = (Y >= 0 ? Y : Y - 399) / 400;
		const int Yoe = Y - Era * 400;
		const int Doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
		const int Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
		return Era * 146097 + Doe - 719468;
	}

	void CivilFromDays(int Days, int& Y, int& M, int& D)
	{
		const int Z = Days + 719468;
		const int Era = (Z >= 0 ? Z : Z - 146096) / 146097;
		const int Doe = Z - Era * 146097;
		const int Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
		const int Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
		const int Mp = (5 * Doy + 2) / 153;
		D = Doy - (153 * Mp + 2) / 5 + 1;
		M = Mp + (Mp < 10 ? 3 : -9);
		Y = Yoe + Era * 400 + (M <= 2 ? 1 : 0);
	}

	int Weekday(int Day)
	{
		// 1970-01-01 was a Thursday (3 with Monday 0)
		const int W = (Day + 3) % 7;
		return W < 0 ? W + 7 : W;
	}

	int NextPostDay(int Day)
	{
		int D = Day + 1;
		while (!IsPostDay(D))
		{
			++D;
		}
		return D;
	}

	std::string AgeGroup(int Age)
	{
		return Age < 13 ? "kid" : (Age < 18 ? "teen" : (Age >= 62 ? "elder" : "adult"));
	}

	// --- the data

	namespace
	{
		std::vector<std::string> Strings(const LyudJson::FValue& V)
		{
			std::vector<std::string> Out;
			for (const LyudJson::FValue& I : V.Items)
			{
				Out.push_back(I.S());
			}
			return Out;
		}

		void Vec3(const LyudJson::FValue& V, double Out[3])
		{
			for (int I = 0; I < 3; ++I)
			{
				Out[I] = V[(size_t)I].Type == LyudJson::FValue::Number ? V[(size_t)I].Num : 0.0;
			}
		}

		std::string Join(const std::vector<std::string>& Parts, char Sep)
		{
			std::string Out;
			for (size_t I = 0; I < Parts.size(); ++I)
			{
				if (I > 0)
				{
					Out += Sep;
				}
				Out += Parts[I];
			}
			return Out;
		}

		std::vector<std::string> Split(const std::string& Text, char Sep)
		{
			std::vector<std::string> Out;
			if (Text.empty())
			{
				return Out;
			}
			size_t From = 0;
			for (;;)
			{
				const size_t At = Text.find(Sep, From);
				Out.push_back(Text.substr(From, At == std::string::npos ? std::string::npos : At - From));
				if (At == std::string::npos)
				{
					break;
				}
				From = At + 1;
			}
			return Out;
		}

		int ToInt(const std::string& S, int Default = 0)
		{
			return S.empty() ? Default : std::atoi(S.c_str());
		}

		const std::string& Field(const std::vector<std::string>& F, size_t I)
		{
			static const std::string None;
			return I < F.size() ? F[I] : None;
		}
	}

	bool FData::Load(const std::string& Json, std::string& Error)
	{
		LyudJson::FValue Root;
		if (!LyudJson::Parse(Json, Root, Error))
		{
			return false;
		}
		*this = FData();
		const LyudJson::FValue& P = Root["post"];
		Post.Collect = P["collect"].I(17) * 60;
		Post.DeliverTown = P["deliver_town"].I(11) * 60;
		Post.DeliverHero = P["deliver_hero"].I(10) * 60;
		Post.PenTravel = P["pen_travel"].I(3);
		Post.PenStartDay = P["pen_start_day"].I(2);
		Post.PenStartLate = P["pen_start_late"].I(5);
		Post.PenNudgeDays = P["pen_nudge_days"].I(10);
		Post.InviteAfter = P["invite_after"].I(2);
		Post.FestivalNotice = P["festival_notice"].I(2);
		Post.ThanksWindow = P["thanks_window"].I(4);
		Pictures = Strings(Root["pictures"]);
		Openings = Strings(Root["openings"]);
		Wishes = Strings(Root["wishes"]);
		for (const auto& F : Root["replies"].Fields)
		{
			Replies[F.first] = Strings(F.second);
		}
		for (const LyudJson::FValue& C : Root["correspondents"].Items)
		{
			FCorrespondent Co;
			Co.Id = C["id"].S();
			Co.Name = C["name"].S();
			Co.Home = C["home"].S();
			Co.Person = C["person"].S();
			Vec3(C["near"], Co.Near);
			Co.NearRadius = C["near_radius"].F(800.f);
			Co.bHasShelf = C["shelf"].Type == LyudJson::FValue::Array;
			if (Co.bHasShelf)
			{
				Vec3(C["shelf"], Co.Shelf);
			}
			Co.ShelfYaw = C["shelf_yaw"].F(0.f);
			Co.Replies = Strings(C["replies"]);
			Co.Invite = C["invite"].S();
			Co.Shelfnote = C["shelf_note"].S();
			if (Co.Id.empty() || Co.Name.empty())
			{
				Error = "letters: a correspondent without id or name";
				return false;
			}
			Correspondents.push_back(Co);
		}
		for (const LyudJson::FValue& T : Root["thanks"].Items)
		{
			FThanks Th;
			Th.Quest = T["quest"].S();
			Th.From = T["from"].S();
			Th.Body = T["body"].S();
			Th.Delay = T["delay"].I(1);
			Thanks.push_back(Th);
		}
		const LyudJson::FValue& Inv = Root["invites"];
		InviteGeneric = Inv["generic"].S();
		for (const auto& F : Inv["festivals"].Fields)
		{
			InviteFestival[F.first] = F.second.S();
		}
		const LyudJson::FValue& PenJ = Root["pen"];
		PenWho = PenJ["who"].S();
		PenPlace = PenJ["place"].S();
		PenNudge = PenJ["nudge"].S();
		PenAnswerNone = PenJ["answer_none"].S();
		for (const LyudJson::FValue& L : PenJ["letters"].Items)
		{
			FPenLetter Le;
			Le.Body = L["body"].S();
			Le.Diary = L["diary"].S();
			Le.Ask = L["ask"].S();
			Pen.push_back(Le);
		}
		for (const auto& F : PenJ["acks"].Fields)
		{
			PenAcks[F.first] = Strings(F.second);
		}
		for (const auto& F : PenJ["answers"].Fields)
		{
			PenAnswers[F.first] = F.second.S();
		}
		for (const auto& F : Root["homes"].Fields)
		{
			Homes[F.first] = Strings(F.second);
		}
		if (Pictures.empty() || Openings.empty() || Wishes.empty())
		{
			Error = "letters: no pictures or phrases";
			return false;
		}
		return true;
	}

	const FCorrespondent* FData::FindCorrespondent(const std::string& Id) const
	{
		const int I = CorrespondentIndex(Id);
		return I >= 0 ? &Correspondents[(size_t)I] : nullptr;
	}

	int FData::CorrespondentIndex(const std::string& Id) const
	{
		for (size_t I = 0; I < Correspondents.size(); ++I)
		{
			if (Correspondents[I].Id == Id)
			{
				return (int)I;
			}
		}
		return -1;
	}

	// --- the model

	int FModel::Roll(const std::string& Key, int N)
	{
		uint32_t H = 2166136261u;
		const std::string S = Key + ":" + std::to_string(N);
		for (const char C : S)
		{
			H ^= (uint8_t)C;
			H *= 16777619u;
		}
		H ^= H >> 15;
		return (int)(H % 100u);
	}

	int FModel::DeliveryDay(const FTime& T) const
	{
		const int Collected = IsPostDay(T.Day) && T.Minute < Data.Post.Collect ? T.Day : NextPostDay(T.Day);
		return NextPostDay(Collected);
	}

	int FModel::PenDeliveryDay(const FTime& T) const
	{
		int D = IsPostDay(T.Day) && T.Minute < Data.Post.Collect ? T.Day : NextPostDay(T.Day);
		for (int I = 0; I < Data.Post.PenTravel; ++I)
		{
			D = NextPostDay(D);
		}
		return D;
	}

	int FModel::TownReplyArrive(int Day) const
	{
		return NextPostDay(NextPostDay(Day));
	}

	int FModel::NextArrival(const FTime& Now) const
	{
		return IsPostDay(Now.Day) && Now.Minute < Data.Post.DeliverHero ? Now.Day : NextPostDay(Now.Day);
	}

	void FModel::Schedule(FLetter&& L)
	{
		for (const FLetter& Old : Letters)
		{
			if (Old.Id == L.Id)
			{
				return;     // once each
			}
		}
		Letters.push_back(std::move(L));
	}

	bool FModel::PostCard(const FTime& Now, const std::string& To, const std::string& Picture, const std::string& Opening, const std::string& Wish,
		const std::string& Group)
	{
		const auto Has = [](const std::vector<std::string>& List, const std::string& Id) { return std::find(List.begin(), List.end(), Id) != List.end(); };
		if (To.empty() || !Has(Data.Pictures, Picture) || !Has(Data.Openings, Opening) || !Has(Data.Wishes, Wish))
		{
			return false;
		}
		FCard C;
		C.To = To;
		C.Picture = Picture;
		C.Opening = Opening;
		C.Wish = Wish;
		C.Group = Data.FindCorrespondent(To) ? std::string() : (Group.empty() ? std::string("adult") : Group);
		C.Posted = Now;
		C.Deliver = DeliveryDay(Now);
		Cards.push_back(C);
		return true;
	}

	FEvents FModel::Step(const FTime& Now, const FFacts& Facts)
	{
		FEvents Out;
		if (FirstDay < 0)
		{
			FirstDay = Now.Day;
		}
		// cards carried round the town at DeliverTown; each may bring a reply
		for (FCard& C : Cards)
		{
			if (C.bDelivered || Now < FTime{ C.Deliver, Data.Post.DeliverTown })
			{
				continue;
			}
			C.bDelivered = true;
			const int N = ++Got[C.To];
			GotDay[C.To] = C.Deliver;
			Out.Delivered.push_back(C.To);
			const std::vector<std::string> PicArg = { "@letters.pic." + C.Picture };
			if (const FCorrespondent* Co = Data.FindCorrespondent(C.To))
			{
				if (N - 1 < (int)Co->Replies.size())
				{
					FLetter L;
					L.Id = "home:" + Co->Id + ":" + std::to_string(N);
					L.From = Co->Name;
					L.Kind = "home";
					L.Body = Co->Replies[(size_t)(N - 1)];
					L.Args = PicArg;
					L.Arrive = TownReplyArrive(C.Deliver);
					Schedule(std::move(L));
				}
				if (N >= Data.Post.InviteAfter && !Co->Invite.empty() && Done.insert("homeinvite:" + Co->Id).second)
				{
					FLetter L;
					L.Id = "homeinvite:" + Co->Id;
					L.From = Co->Name;
					L.Kind = "invite";
					L.Body = Co->Invite;
					L.Arrive = NextPostDay(TownReplyArrive(C.Deliver));
					Schedule(std::move(L));
				}
				continue;
			}
			// a townsperson: more likely to answer the better they know him by cards
			const auto It = Data.Replies.find(C.Group);
			if (It == Data.Replies.end() || It->second.empty())
			{
				continue;
			}
			const int Chance = N == 1 ? 55 : (N == 2 ? 70 : 85);
			if (Roll(C.To, N) >= Chance)
			{
				continue;
			}
			const std::vector<std::string>& Pool = It->second;
			// a different reply each time to the same person (the pool walked from a start of their own)
			const size_t Pick = ((size_t)Roll(C.To + "#", 0) + (size_t)(N - 1)) % Pool.size();
			FLetter L;
			L.Id = "reply:" + C.To + ":" + std::to_string(N);
			L.From = C.To;
			L.Kind = "reply";
			L.Body = Pool[Pick];
			L.Args = PicArg;
			L.Arrive = TownReplyArrive(C.Deliver);
			Schedule(std::move(L));
		}
		// thanks after quests
		for (const FThanks& T : Data.Thanks)
		{
			if (Facts.QuestsDone.count(T.Quest) && Done.insert("thanks:" + T.Quest).second)
			{
				int D = Now.Day + std::max(0, T.Delay);
				if (!IsPostDay(D))
				{
					D = NextPostDay(D);
				}
				FLetter L;
				L.Id = "thanks:" + T.Quest;
				L.From = T.From;
				L.Kind = "thanks";
				L.Body = T.Body;
				L.Arrive = std::max(D, NextArrival(Now));
				Schedule(std::move(L));
			}
		}
		// invitations to the holidays, a couple of days before
		for (const auto& F : Facts.Festivals)
		{
			const int Lead = F.first - Now.Day;
			if (Lead < 0 || Lead > Data.Post.FestivalNotice)
			{
				continue;
			}
			const int Arrive = NextArrival(Now);
			if (Arrive > F.first)
			{
				continue;     // too late for the post: the holiday is on before a letter could come
			}
			const std::string Key = "invite:" + F.second + "@" + std::to_string(F.first);
			if (!Done.insert(Key).second)
			{
				continue;
			}
			const auto Own = Data.InviteFestival.find(F.second);
			const std::string Body = Own != Data.InviteFestival.end() ? Own->second : Data.InviteGeneric;
			if (Body.empty())
			{
				continue;
			}
			FLetter L;
			L.Id = Key;
			L.From = "letters.from.council";
			L.Kind = "invite";
			L.Body = Body;
			L.Args = { "@festival." + F.second + ".name" };
			L.Arrive = Arrive;
			Schedule(std::move(L));
		}
		StepPen(Now, Facts, Out);
		// the morning round brings the letters to the club
		for (size_t I = 0; I < Letters.size(); ++I)
		{
			FLetter& L = Letters[I];
			if (!L.bArrived && Now >= FTime{ L.Arrive, Data.Post.DeliverHero })
			{
				L.bArrived = true;
				Out.Arrived.push_back((int)I);
			}
		}
		return Out;
	}

	void FModel::StepPen(const FTime& Now, const FFacts& Facts, FEvents& Out)
	{
		if (Data.Pen.empty())
		{
			return;
		}
		if (PenSent == 0)
		{
			const int Start = FirstDay + (Facts.Birds.empty() ? Data.Post.PenStartLate : Data.Post.PenStartDay);
			if (Now.Day < Start)
			{
				return;
			}
			FLetter L;
			L.Id = "pen:1";
			L.From = Data.PenWho;
			L.Kind = "pen";
			L.Body = Data.Pen[0].Body;
			L.Args = { "@" + Data.PenPlace };
			L.Arrive = NextArrival(Now);
			Schedule(std::move(L));
			PenSent = 1;
			return;
		}
		// the hero's answer reaches her: she writes back the same day
		if (PenAnswerArrive >= 0 && Now >= FTime{ PenAnswerArrive, Data.Post.DeliverTown })
		{
			const int Got1 = PenAnswerArrive;
			PenAnswerArrive = -1;
			Out.bPenReplyDelivered = true;
			if (PenSent < (int)Data.Pen.size())
			{
				const int N = PenSent + 1;
				const FAnswer A = PenAnswers.empty() ? FAnswer{ "none", "" } : PenAnswers.back();
				FLetter L;
				L.Id = "pen:" + std::to_string(N);
				L.From = Data.PenWho;
				L.Kind = "pen";
				L.Body = Data.Pen[(size_t)(N - 1)].Body;
				L.Args = { "@" + Data.PenPlace };
				const auto Acks = Data.PenAcks.find(A.Fact.empty() ? std::string("none") : A.Kind);
				if (Acks != Data.PenAcks.end() && !Acks->second.empty())
				{
					L.Ack = Acks->second[(size_t)(N - 2) % Acks->second.size()];
					if (!A.Fact.empty())
					{
						L.AckArgs = { "@" + A.Fact };
					}
				}
				L.Arrive = PenDeliveryDay(FTime{ Got1, Data.Post.DeliverTown });
				Schedule(std::move(L));
				PenSent = N;
				bPenNudged = false;
			}
		}
		// no answer for long after her letter was read: she asks whether it came
		if (PenWaiting() && !bPenNudged && !Data.PenNudge.empty())
		{
			const std::string Id = "pen:" + std::to_string(PenSent);
			for (const FLetter& L : Letters)
			{
				if (L.Id == Id && L.bRead && Now.Day - L.ReadDay >= Data.Post.PenNudgeDays)
				{
					bPenNudged = true;
					FLetter Nudge;
					Nudge.Id = Id + ":nudge";
					Nudge.From = Data.PenWho;
					Nudge.Kind = "pen";
					Nudge.Body = Data.PenNudge;
					Nudge.Args = { "@" + Data.PenPlace };
					Nudge.Arrive = NextArrival(Now);
					Schedule(std::move(Nudge));
					break;
				}
			}
		}
	}

	bool FModel::PenWaiting() const
	{
		if (PenSent <= 0 || PenAnswered >= PenSent || PenSent > (int)Data.Pen.size() || Data.Pen[(size_t)(PenSent - 1)].Ask.empty())
		{
			return false;
		}
		const std::string Id = "pen:" + std::to_string(PenSent);
		for (const FLetter& L : Letters)
		{
			if (L.Id == Id)
			{
				return L.bRead;
			}
		}
		return false;
	}

	std::string FModel::PenAsk() const
	{
		return PenWaiting() ? Data.Pen[(size_t)(PenSent - 1)].Ask : std::string();
	}

	std::vector<FAnswer> FModel::PenOptions(const FFacts& Facts, int Max) const
	{
		std::vector<FAnswer> Out;
		const std::string Ask = PenAsk();
		if (Ask.empty())
		{
			return Out;
		}
		const std::vector<std::string>* List = Ask == "bird" ? &Facts.Birds
			: (Ask == "fish" ? &Facts.Fish : (Ask == "place" ? &Facts.Places : (Ask == "friend" ? &Facts.Friends : nullptr)));
		std::set<std::string> Told;
		for (const FAnswer& A : PenAnswers)
		{
			Told.insert(A.Fact);
		}
		if (List)
		{
			// the newest first, nothing told her already; then the told ones if nothing new is left
			for (int Pass = 0; Pass < 2 && (int)Out.size() < Max; ++Pass)
			{
				for (auto It = List->rbegin(); It != List->rend() && (int)Out.size() < Max; ++It)
				{
					const bool bTold = Told.count(*It) > 0;
					if (It->empty() || bTold != (Pass == 1))
					{
						continue;
					}
					bool bDup = false;
					for (const FAnswer& A : Out)
					{
						bDup = bDup || A.Fact == *It;
					}
					if (!bDup)
					{
						Out.push_back({ Ask, *It });
					}
				}
			}
		}
		Out.push_back({ "none", "" });
		return Out;
	}

	bool FModel::PostPenAnswer(const FTime& Now, const FAnswer& Answer)
	{
		if (!PenWaiting())
		{
			return false;
		}
		PenAnswers.push_back(Answer);
		PenAnswered = PenSent;
		PenAnswerArrive = PenDeliveryDay(Now);
		return true;
	}

	void FModel::MarkRead(int Letter, int Day)
	{
		if (Letter >= 0 && Letter < (int)Letters.size() && !Letters[(size_t)Letter].bRead)
		{
			Letters[(size_t)Letter].bRead = true;
			Letters[(size_t)Letter].ReadDay = Day;
		}
	}

	int FModel::NumUnread() const
	{
		int N = 0;
		for (const FLetter& L : Letters)
		{
			N += L.bArrived && !L.bRead ? 1 : 0;
		}
		return N;
	}

	int FModel::NumOnTheWay() const
	{
		int N = 0;
		for (const FLetter& L : Letters)
		{
			N += L.bArrived ? 0 : 1;
		}
		return N;
	}

	std::vector<std::string> FModel::WantsToThank(int Today) const
	{
		std::vector<std::string> Out;
		for (const auto& G : Got)
		{
			const auto D = GotDay.find(G.first);
			if (D == GotDay.end() || Today - D->second > Data.Post.ThanksWindow || Today < D->second)
			{
				continue;
			}
			if (!Thanked.count(G.first + ":" + std::to_string(G.second)))
			{
				Out.push_back(G.first);
			}
		}
		return Out;
	}

	void FModel::NoteThanked(const std::string& To)
	{
		const auto G = Got.find(To);
		if (G != Got.end())
		{
			Thanked.insert(To + ":" + std::to_string(G->second));
		}
	}

	bool FModel::Meet(const std::string& Correspondent)
	{
		return Data.FindCorrespondent(Correspondent) && Known.insert(Correspondent).second;
	}

	std::vector<std::string> FModel::AddressOf(const std::string& Home) const
	{
		const auto It = Data.Homes.find(Home);
		if (It != Data.Homes.end())
		{
			return It->second;
		}
		// a flat of a building: the building's entry with the flat after the slash
		const size_t Slash = Home.find('/');
		if (Slash != std::string::npos)
		{
			const auto B = Data.Homes.find(Home.substr(0, Slash));
			if (B != Data.Homes.end() && B->second.size() >= 2)
			{
				return { B->second[0], B->second[1], Home.substr(Slash + 1) };
			}
		}
		return {};
	}

	void FModel::Reset()
	{
		FirstDay = -1;
		Cards.clear();
		Letters.clear();
		Got.clear();
		GotDay.clear();
		Thanked.clear();
		Known.clear();
		Done.clear();
		PenSent = 0;
		PenAnswered = 0;
		PenAnswerArrive = -1;
		PenAnswers.clear();
		bPenNudged = false;
	}

	// --- the save: a flat map of strings, fields split by tabs, lists by the unit separator

	void FModel::Save(std::map<std::string, std::string>& Out) const
	{
		Out.clear();
		Out["first"] = std::to_string(FirstDay);
		for (size_t I = 0; I < Cards.size(); ++I)
		{
			const FCard& C = Cards[I];
			Out["card." + std::to_string(I)] = Join({ C.To, C.Picture, C.Opening, C.Wish, C.Group, std::to_string(C.Posted.Day), std::to_string(C.Posted.Minute),
				std::to_string(C.Deliver), C.bDelivered ? "1" : "0" }, '\t');
		}
		for (size_t I = 0; I < Letters.size(); ++I)
		{
			const FLetter& L = Letters[I];
			Out["letter." + std::to_string(I)] = Join({ L.Id, L.From, L.Kind, L.Body, Join(L.Args, '\x1f'), L.Ack, Join(L.AckArgs, '\x1f'), std::to_string(L.Arrive),
				L.bArrived ? "1" : "0", L.bRead ? "1" : "0", std::to_string(L.ReadDay) }, '\t');
		}
		for (const auto& G : Got)
		{
			Out["got." + G.first] = std::to_string(G.second);
		}
		for (const auto& G : GotDay)
		{
			Out["gotday." + G.first] = std::to_string(G.second);
		}
		Out["thanked"] = Join(std::vector<std::string>(Thanked.begin(), Thanked.end()), '\x1f');
		Out["known"] = Join(std::vector<std::string>(Known.begin(), Known.end()), '\x1f');
		Out["done"] = Join(std::vector<std::string>(Done.begin(), Done.end()), '\x1f');
		Out["pen"] = Join({ std::to_string(PenSent), std::to_string(PenAnswered), std::to_string(PenAnswerArrive), bPenNudged ? "1" : "0" }, '\t');
		for (size_t I = 0; I < PenAnswers.size(); ++I)
		{
			Out["pen.answer." + std::to_string(I)] = PenAnswers[I].Kind + "\t" + PenAnswers[I].Fact;
		}
	}

	void FModel::LoadState(const std::map<std::string, std::string>& In)
	{
		Reset();
		const auto Get = [&In](const std::string& Key) -> std::string
		{
			const auto It = In.find(Key);
			return It == In.end() ? std::string() : It->second;
		};
		FirstDay = ToInt(Get("first"), -1);
		for (int I = 0;; ++I)
		{
			const auto It = In.find("card." + std::to_string(I));
			if (It == In.end())
			{
				break;
			}
			const std::vector<std::string> F = Split(It->second, '\t');
			FCard C;
			C.To = Field(F, 0);
			C.Picture = Field(F, 1);
			C.Opening = Field(F, 2);
			C.Wish = Field(F, 3);
			C.Group = Field(F, 4);
			C.Posted.Day = ToInt(Field(F, 5));
			C.Posted.Minute = ToInt(Field(F, 6));
			C.Deliver = ToInt(Field(F, 7));
			C.bDelivered = Field(F, 8) == "1";
			Cards.push_back(C);
		}
		for (int I = 0;; ++I)
		{
			const auto It = In.find("letter." + std::to_string(I));
			if (It == In.end())
			{
				break;
			}
			const std::vector<std::string> F = Split(It->second, '\t');
			FLetter L;
			L.Id = Field(F, 0);
			L.From = Field(F, 1);
			L.Kind = Field(F, 2);
			L.Body = Field(F, 3);
			L.Args = Split(Field(F, 4), '\x1f');
			L.Ack = Field(F, 5);
			L.AckArgs = Split(Field(F, 6), '\x1f');
			L.Arrive = ToInt(Field(F, 7));
			L.bArrived = Field(F, 8) == "1";
			L.bRead = Field(F, 9) == "1";
			L.ReadDay = ToInt(Field(F, 10));
			Letters.push_back(L);
		}
		for (const auto& KV : In)
		{
			if (KV.first.rfind("got.", 0) == 0)
			{
				Got[KV.first.substr(4)] = ToInt(KV.second);
			}
			else if (KV.first.rfind("gotday.", 0) == 0)
			{
				GotDay[KV.first.substr(7)] = ToInt(KV.second);
			}
		}
		for (const std::string& S : Split(Get("thanked"), '\x1f'))
		{
			Thanked.insert(S);
		}
		for (const std::string& S : Split(Get("known"), '\x1f'))
		{
			Known.insert(S);
		}
		for (const std::string& S : Split(Get("done"), '\x1f'))
		{
			Done.insert(S);
		}
		const std::vector<std::string> P = Split(Get("pen"), '\t');
		PenSent = ToInt(Field(P, 0));
		PenAnswered = ToInt(Field(P, 1));
		PenAnswerArrive = ToInt(Field(P, 2), -1);
		bPenNudged = Field(P, 3) == "1";
		for (int I = 0;; ++I)
		{
			const auto It = In.find("pen.answer." + std::to_string(I));
			if (It == In.end())
			{
				break;
			}
			const std::vector<std::string> F = Split(It->second, '\t');
			PenAnswers.push_back({ Field(F, 0), Field(F, 1) });
		}
	}
}
