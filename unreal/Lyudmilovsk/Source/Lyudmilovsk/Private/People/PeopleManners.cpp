#include "PeopleManners.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace LyudPeople
{
	namespace
	{
		const char* MannerNames[NumManners] = { "none", "fold_arms", "hands_hips", "hands_back", "stretch", "yawn", "scratch_head",
			"check_watch", "look_sky", "shade_eyes", "rub_hands", "hug_self", "wave", "nod", "point", "clap", "sway", "doze",
			"cover_head", "phone_call", "phone_scroll", "smoke", "photo", "feed_birds", "umbrella", "fish", "paint", "binoculars",
			"newspaper", "book", "knit", "ice_cream", "drink", "carry_bag", "point_sky" };
		const char* PropNames[(int)EProp::Num] = { "none", "phone", "cigarette", "camera", "umbrella", "rod", "brush", "binoculars",
			"newspaper", "book", "knitting", "ice_cream", "cup", "bag" };
		const char* HabitNames[] = { "smoker", "phone", "reader", "feeder", "photographer", "knitter", "umbrella", "sweet", "dozer",
			"hands_back" };
		constexpr int NumHabits = (int)(sizeof(HabitNames) / sizeof(HabitNames[0]));
		const char* NoticeNames[(int)ENoticeEvent::Num] = { "", "hero_runs", "hero_close", "hero_lamp", "hero_sneaks", "hero_rides", "hero_fishes", "hero_swims", "hero_sits" };

		float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

		bool Among(const std::vector<std::string>& V, const std::string& S)
		{
			return std::find(V.begin(), V.end(), S) != V.end();
		}

		float WrapDeg(float D)
		{
			while (D > 180.f)
			{
				D -= 360.f;
			}
			while (D < -180.f)
			{
				D += 360.f;
			}
			return D;
		}

		/** How far off somebody's facing a point is, degrees 0 .. 180. */
		float OffFacing(const FPerson& P, const V3& To)
		{
			const float Yaw = std::atan2(To.Y - P.Pos.Y, To.X - P.Pos.X) * 57.29578f;
			return std::fabs(WrapDeg(Yaw - P.Yaw));
		}

		bool InHours(float From, float To, float H)
		{
			return From <= To ? (H >= From && H < To) : (H >= From || H < To);
		}

		void Strings(const LyudJson::FValue& V, std::vector<std::string>& Yes, std::vector<std::string>* No)
		{
			for (const LyudJson::FValue& I : V.Items)
			{
				const std::string S = I.S();
				if (S.empty())
				{
					continue;
				}
				if (S[0] == '!' && No)
				{
					No->push_back(S.substr(1));
				}
				else
				{
					Yes.push_back(S);
				}
			}
		}

		/** On their feet or seated where a manner can show: not lying, hidden, at a computer or working a scraper. */
		bool Mannered(const FPerson& P)
		{
			if (P.Bike >= 0 && (P.BikeMode > 0 || P.BikeAct != EBikeAct::None))
			{
				return false;
			}
			return P.State == EState::Doing || P.State == EState::Going || P.State == EState::Waiting;
		}
	}

	const char* MannerName(EManner M) { return M < EManner::Num ? MannerNames[(int)M] : "?"; }
	const char* PropName(EProp P) { return P < EProp::Num ? PropNames[(int)P] : "?"; }
	const char* NoticeEventName(ENoticeEvent E) { return E < ENoticeEvent::Num ? NoticeNames[(int)E] : ""; }

	EManner MannerFromName(const std::string& S)
	{
		for (int I = 0; I < NumManners; I++)
		{
			if (S == MannerNames[I])
			{
				return (EManner)I;
			}
		}
		return EManner::Num;
	}

	EProp PropFromName(const std::string& S)
	{
		for (int I = 0; I < (int)EProp::Num; I++)
		{
			if (S == PropNames[I])
			{
				return (EProp)I;
			}
		}
		return EProp::Num;
	}

	const char* HabitName(uint32_t Bit)
	{
		for (int I = 0; I < NumHabits; I++)
		{
			if (Bit == (1u << I))
			{
				return HabitNames[I];
			}
		}
		return "?";
	}

	uint32_t HabitFromName(const std::string& S)
	{
		for (int I = 0; I < NumHabits; I++)
		{
			if (S == HabitNames[I])
			{
				return 1u << I;
			}
		}
		return 0;
	}

	float PersonaTrait(const FPersona& P, const std::string& Name)
	{
		if (Name == "warmth")
		{
			return P.Warmth;
		}
		if (Name == "nerve")
		{
			return P.Nerve;
		}
		if (Name == "curiosity")
		{
			return P.Curiosity;
		}
		if (Name == "tidy")
		{
			return P.Tidy;
		}
		return 0.5f;
	}

	FPersona MakePersona(const FPerson& P)
	{
		// the same person, the same character: everything comes from their key (FNV-1a), shaded by what the life knows
		uint32_t H = 2166136261u;
		for (const char C : P.Key)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		H |= 1u;
		auto R = [&H]()
		{
			H = H * 1664525u + 1013904223u;
			return (float)((H >> 8) & 0xFFFF) / 65535.f;
		};
		const bool bKid = P.Age < 12, bTeen = P.Age >= 12 && P.Age < 18, bAdult = P.Age >= 18;
		const std::string& Job = P.Occupation;
		FPersona O;
		O.Warmth = Clamp01(0.22f + 0.5f * P.Social + (R() - 0.5f) * 0.5f + (P.Age >= 60 ? 0.08f : 0.f) + (bTeen ? -0.08f : 0.f));
		O.Nerve = Clamp01(0.5f + (R() - 0.5f) * 0.8f + (P.Active - 0.5f) * 0.3f - (P.Age > 65 ? 0.12f : 0.f) + (bKid ? 0.15f : 0.f));
		O.Curiosity = Clamp01(0.28f + (R() - 0.5f) * 0.6f + (bKid ? 0.35f : (bTeen ? 0.1f : 0.f)) + P.Studious * 0.2f);
		O.Tidy = Clamp01(0.5f + (R() - 0.5f) * 0.7f + (P.Studious - 0.5f) * 0.3f + (P.Age > 50 ? 0.1f : 0.f) - (bTeen ? 0.15f : 0.f));
		O.bLeftHanded = R() < 0.1f;
		auto Roll = [&R, &O](uint32_t Habit, float Chance)
		{
			// always one roll per habit, so adding a habit doesn't reshuffle the others
			if (R() < Chance)
			{
				O.Habits |= Habit;
			}
		};
		float Smoke = 0.f;
		if (bAdult)
		{
			Smoke = 0.12f;
			if (Job == "fisherman" || Job == "musician" || Job == "writer" || Job == "artist" || Job == "repairman" || Job == "guard"
				|| Job == "unemployed" || Job == "bartender" || Job == "projectionist")
			{
				Smoke += 0.12f;
			}
			if (P.Chronotype == "night" || P.Chronotype == "owl")
			{
				Smoke += 0.06f;
			}
			if (P.Active > 0.8f || Job == "coach" || Job == "doctor" || Job == "lifeguard")
			{
				Smoke -= 0.09f;
			}
			if (P.Age > 72)
			{
				Smoke -= 0.05f;
			}
		}
		Roll(HabitSmoker, Smoke);
		Roll(HabitPhone, P.Age < 10 ? 0.f : (P.Age < 18 ? 0.7f : (P.Age < 35 ? 0.55f : (P.Age < 55 ? 0.32f : (P.Age < 70 ? 0.14f : 0.05f)))) + P.Gamer * 0.2f);
		Roll(HabitReader, P.Age < 25 ? 0.f : 0.08f + 0.5f * P.Studious + (P.Age > 55 ? 0.15f : 0.f));
		Roll(HabitFeeder, (P.Age >= 60 ? 0.28f : (bKid ? 0.2f : 0.04f)) + P.NatureLove * 0.2f);
		Roll(HabitPhotographer, Job == "photographer" ? 1.f : (P.Age < 14 ? 0.f : (Job == "artist" ? 0.45f : 0.04f + P.SunLove * 0.12f)));
		Roll(HabitKnitter, P.bFemale ? (P.Age >= 50 ? 0.3f : (bAdult ? 0.05f : 0.f)) : (P.Age >= 65 ? 0.02f : 0.f));
		Roll(HabitUmbrella, P.Age < 10 ? 0.3f : 0.25f + 0.6f * O.Tidy);
		Roll(HabitSweet, bKid ? 0.7f : (bTeen ? 0.5f : 0.2f));
		Roll(HabitDozer, P.Age >= 65 ? 0.5f : (P.Age >= 50 ? 0.15f : 0.f));
		Roll(HabitHandsBack, P.Age >= 58 ? (P.bFemale ? 0.35f : 0.55f) : (P.Age >= 40 ? 0.06f : 0.f));
		return O;
	}

	float FManners::Rand(uint32_t& S)
	{
		S = S * 1664525u + 1013904223u;
		return (float)((S >> 8) & 0xFFFF) / 65536.f;
	}

	bool FManners::Load(const std::string& JsonText)
	{
		Defs.clear();
		Warnings.clear();
		LastError.clear();
		LyudJson::FValue Root;
		if (!LyudJson::Parse(JsonText, Root, LastError))
		{
			LastError = "manners.json: " + LastError;
			return false;
		}
		IdleWeight = Root["idle_weight"].F(3.f);
		for (const LyudJson::FValue& J : Root["manners"].Items)
		{
			FMannerDef D;
			const std::string Id = J["id"].S();
			D.Manner = MannerFromName(Id);
			if (D.Manner == EManner::Num || D.Manner == EManner::None)
			{
				Warnings.push_back("unknown manner: " + Id);
				continue;
			}
			if (!J["prop"].IsNull())
			{
				D.Prop = PropFromName(J["prop"].S());
				if (D.Prop == EProp::Num)
				{
					Warnings.push_back(Id + ": unknown prop " + J["prop"].S());
					D.Prop = EProp::None;
				}
			}
			for (const LyudJson::FValue& Po : J["poses"].Items)
			{
				bool bFound = false;
				for (int I = 0; I < (int)EPose::Num; I++)
				{
					if (Po.S() == PoseName((EPose)I))
					{
						D.Poses |= 1u << I;
						bFound = true;
					}
				}
				if (!bFound)
				{
					Warnings.push_back(Id + ": unknown pose " + Po.S());
				}
			}
			Strings(J["doing"], D.Doing, &D.NotDoing);
			Strings(J["at"], D.At, nullptr);
			Strings(J["after"], D.After, nullptr);
			D.AfterSeconds = J["after_s"].F(900.f);
			if (!J["habit"].IsNull())
			{
				D.Habit = HabitFromName(J["habit"].S());
				if (D.Habit == 0)
				{
					Warnings.push_back(Id + ": unknown habit " + J["habit"].S());
					continue;
				}
			}
			if (J["ages"].Size() == 2)
			{
				D.MinAge = J["ages"][0].I();
				D.MaxAge = J["ages"][1].I();
			}
			if (J["hours"].Size() == 2)
			{
				D.From = J["hours"][0].F();
				D.To = J["hours"][1].F();
			}
			D.Outdoors = (int8_t)J["outdoors"].I(-1);
			D.Rain = (int8_t)J["rain"].I(-1);
			D.SunUp = (int8_t)J["sun_up"].I(-1);
			D.bSunLow = J["sun_low"].I() != 0;
			D.bClear = J["clear"].I() != 0;
			D.bSkyShow = J["sky_show"].I() != 0;
			D.TempBelow = J["temp_below"].F(1e9f);
			D.TempAbove = J["temp_above"].F(-1e9f);
			if (!J["need"].IsNull())
			{
				const ENeed N = NeedFromName(J["need"].S());
				if (N == ENeed::Num)
				{
					Warnings.push_back(Id + ": unknown need " + J["need"].S());
				}
				else
				{
					D.Need = (int)N;
					D.NeedOver = J["need_over"].F(0.6f);
				}
			}
			if (!J["gather"].IsNull())
			{
				static const char* Modes[] = { "mingle", "watch", "sit", "fair" };
				for (int I = 0; I < 4; I++)
				{
					if (J["gather"].S() == Modes[I])
					{
						D.Gather = I;
					}
				}
				if (D.Gather < 0)
				{
					Warnings.push_back(Id + ": unknown gather " + J["gather"].S());
					continue;
				}
			}
			D.bWaiting = J["waiting"].I() != 0;
			D.Trait = J["trait"].S();
			D.Weight = J["weight"].F(1.f);
			if (J["seconds"].Size() == 2)
			{
				D.SecMin = J["seconds"][0].F();
				D.SecMax = std::max(D.SecMin, J["seconds"][1].F());
			}
			D.Cooldown = J["cooldown"].F(60.f);
			D.bForce = J["force"].I() != 0;
			D.bReactOnly = J["react"].I() != 0;
			if (D.Poses == 0 && !D.bReactOnly)
			{
				Warnings.push_back(Id + ": no poses");
				continue;
			}
			Defs.push_back(std::move(D));
		}
		if (Defs.empty())
		{
			LastError = "manners.json: no manners";
			return false;
		}
		return true;
	}

	void FManners::Bind(const FWorld& W)
	{
		const size_t N = W.People.size();
		Personas.resize(N);
		States.assign(N, FMannerState());
		for (size_t I = 0; I < N; I++)
		{
			Personas[I] = MakePersona(W.People[I]);
			FMannerState& S = States[I];
			S.Rng = (uint32_t)(I * 2654435761u) | 1u;
			for (float& L : S.Last)
			{
				L = -1e9f;
			}
			// nobody starts in step: the first pick comes at their own moment
			S.Until = 2.f + 10.f * Rand(S.Rng);
		}
		for (int& Row : RowOf)
		{
			Row = -1;
		}
		for (int I = 0; I < (int)Defs.size(); I++)
		{
			FMannerDef& D = Defs[I];
			if (RowOf[(int)D.Manner] < 0)
			{
				RowOf[(int)D.Manner] = I;
			}
			auto Mask = [&W, this, &D](const std::vector<std::string>& Yes, const std::vector<std::string>& No, std::vector<uint8_t>& Out)
			{
				Out.clear();
				if (Yes.empty() && No.empty())
				{
					return;
				}
				Out.assign(W.Activities.size(), Yes.empty() ? 1 : 0);
				for (size_t A = 0; A < W.Activities.size(); A++)
				{
					if (Among(Yes, W.Activities[A].Id))
					{
						Out[A] = 1;
					}
					if (Among(No, W.Activities[A].Id))
					{
						Out[A] = 0;
					}
				}
				for (const std::vector<std::string>* List : { &Yes, &No })
				{
					for (const std::string& Name : *List)
					{
						if (W.FindActivity(Name) < 0)
						{
							Warnings.push_back(std::string(MannerName(D.Manner)) + ": no activity " + Name);
						}
					}
				}
			};
			Mask(D.Doing, D.NotDoing, D.DoingOk);
			Mask(D.After, std::vector<std::string>(), D.AfterOk);
		}
		Cand.reserve(Defs.size());
		CandW.reserve(Defs.size());
	}

	void FManners::ApplyLikes(FWorld& W) const
	{
		struct FLike { uint32_t Habit; const char* Activity; float More; };
		static const FLike Table[] = { { HabitFeeder, "feed_birds", 0.55f }, { HabitReader, "read_bench", 0.35f },
			{ HabitDozer, "sit_bench", 0.1f }, { HabitPhotographer, "sun_watch", 0.2f } };
		for (size_t I = 0; I < W.People.size() && I < Personas.size(); I++)
		{
			for (const FLike& L : Table)
			{
				if (!Personas[I].Has(L.Habit) || W.FindActivity(L.Activity) < 0)
				{
					continue;
				}
				bool bHas = false;
				for (auto& Like : W.People[I].Likes)
				{
					if (Like.first == L.Activity)
					{
						Like.second = std::max(Like.second, L.More);
						bHas = true;
					}
				}
				if (!bHas)
				{
					W.People[I].Likes.emplace_back(L.Activity, L.More);
				}
			}
		}
		// a ritual each: one thing they do at their own hour every day, by who they are (the first that fits; the hour
		// is theirs, the same every game). People at work at that hour simply miss it; days off they keep it.
		struct FRitual { uint32_t Habit; const char* Trait; float Over; const char* Activity; float From, To; int MinAge, MaxAge; };
		static const FRitual Rituals[] = {
			{ HabitFeeder, "", 0.f, "feed_birds", 8.5f, 10.5f, 0, 200 },
			{ HabitHandsBack, "", 0.f, "stroll", 7.f, 9.f, 50, 200 },
			{ HabitReader, "", 0.f, "read_bench", 15.f, 17.5f, 12, 200 },
			{ HabitDozer, "", 0.f, "sit_bench", 13.5f, 15.f, 40, 200 },
			{ HabitSweet, "", 0.f, "coffee", 15.5f, 17.5f, 10, 200 },
			{ HabitPhotographer, "", 0.f, "walk", 17.f, 19.f, 14, 200 },
			{ 0, "nerve", 0.68f, "jog", 6.5f, 8.f, 16, 58 },
			{ 0, "warmth", 0.7f, "chat", 18.f, 20.f, 12, 200 },
			{ 0, "curiosity", 0.7f, "town", 11.f, 13.f, 14, 200 },
			{ 0, "tidy", 0.72f, "shop_town", 9.5f, 11.f, 18, 200 },
			{ 0, "", 0.f, "stroll", 19.f, 21.f, 14, 200 },
		};
		for (size_t I = 0; I < W.People.size() && I < Personas.size(); I++)
		{
			FPerson& P = W.People[I];
			const FPersona& Who = Personas[I];
			P.RitualAct = -1;
			if (P.bChild)
			{
				continue;
			}
			for (const FRitual& R : Rituals)
			{
				const int Act = W.FindActivity(R.Activity);
				if (Act < 0 || P.Age < R.MinAge || P.Age > R.MaxAge || (R.Habit && !Who.Has(R.Habit))
					|| (R.Trait[0] && PersonaTrait(Who, R.Trait) < R.Over))
				{
					continue;
				}
				uint32_t H = 2166136261u;
				for (const char C : P.Key)
				{
					H = (H ^ (uint8_t)C) * 16777619u;
				}
				H ^= 0x51ed270bu;
				H |= 1u;
				P.RitualAct = Act;
				P.RitualHour = R.From + (R.To - R.From) * Rand(H);
				P.RitualMore = 0.45f;
				break;
			}
		}
		// a weekend outing for every household of two and more: on Saturday or Sunday, at the household's hour, they all go
		// to the same thing at the same place (one of the four nearest home that have it): the sea front, a stroll, the fair, crumbs
		// for the birds, a table at the cafe for those without small children. The same every game.
		// how many can be at an activity at a place: its slots of the activity's kinds
		auto Room = [&W](const FActivity& A, const FPlace& Pl)
		{
			int N = 0;
			for (const int SI : Pl.Slots)
			{
				const FSlot& Sl = W.Slots[(size_t)SI];
				N += A.Slots.empty() || std::find(A.Slots.begin(), A.Slots.end(), Sl.Kind) != A.Slots.end() ? Sl.Capacity : 0;
			}
			return N;
		};
		// at work (school, any duty) on that weekday round that hour: they'd miss a meeting then
		auto Busy = [](const FPerson& P, int Day, float Hour)
		{
			for (const FDuty& Duty : P.Duties)
			{
				const bool bOver = Duty.From <= Duty.To ? (Hour + 1.f > Duty.From && Hour - 0.75f < Duty.To) : (Hour + 1.f > Duty.From || Hour - 0.75f < Duty.To);
				if (((Duty.Days >> Day) & 1) && bOver)
				{
					return true;
				}
			}
			return false;
		};
		// (what needs daylight ends by half past three: the sun is down by five in late autumn)
		struct FOuting { const char* Activity; float From, To; bool bKids; };
		static const FOuting Outings[] = {
			{ "sea_air", 11.f, 15.f, true }, { "stroll", 11.f, 17.f, true }, { "fair_visit", 12.f, 17.f, true }, { "feed_birds", 10.5f, 15.f, true },
			{ "sea_air", 13.f, 15.5f, true }, { "eat_out", 13.f, 19.5f, false }, { "coffee", 11.f, 16.f, false }, { "stroll", 17.f, 19.5f, false },
		};
		std::map<std::string, std::vector<int>> Houses;
		for (FPerson& P : W.People)
		{
			P.OutingAct = P.OutingDay = P.OutingPlace = -1;
			if (!P.Household.empty() && P.Home >= 0)
			{
				Houses[P.Household].push_back(P.Id);
			}
		}
		for (const auto& House : Houses)
		{
			if (House.second.size() < 2)
			{
				continue;
			}
			uint32_t H = 2166136261u;
			for (const char C : House.first)
			{
				H = (H ^ (uint8_t)C) * 16777619u;
			}
			H ^= 0x7a3c91e5u;
			H |= 1u;
			bool bKids = false;
			for (const int Id : House.second)
			{
				bKids |= W.People[(size_t)Id].Age < 14;
			}
			const int NumOutings = (int)(sizeof(Outings) / sizeof(Outings[0]));
			const FOuting* O = nullptr;
			int Act = -1;
			for (int Try = 0; Try < 8 && Act < 0; Try++)
			{
				const FOuting& Choice = Outings[(int)(Rand(H) * NumOutings) % NumOutings];
				if (!Choice.bKids && bKids)
				{
					continue;
				}
				Act = W.FindActivity(Choice.Activity);
				O = &Choice;
			}
			if (Act < 0)
			{
				continue;
			}
			const FActivity& A = W.Activities[(size_t)Act];
			const FPerson& First = W.People[(size_t)House.second[0]];
			const V3 HomeAt = W.Places[(size_t)First.Home].Portal;
			// the four nearest places of its kinds on their side of the water
			int Near[4] = { -1, -1, -1, -1 };
			float NearD[4] = { 1e30f, 1e30f, 1e30f, 1e30f };
			for (int Pi = 0; Pi < (int)W.Places.size(); Pi++)
			{
				const FPlace& Pl = W.Places[(size_t)Pi];
				if (Pl.Island != W.Places[(size_t)First.Home].Island || std::find(A.Places.begin(), A.Places.end(), Pl.Kind) == A.Places.end()
					|| Room(A, Pl) < (int)House.second.size())
				{
					continue;
				}
				float D = (Pl.Portal - HomeAt).Len2D();
				int Put = Pi;
				for (int K = 0; K < 4; K++)
				{
					if (D < NearD[K])
					{
						std::swap(D, NearD[K]);
						std::swap(Put, Near[K]);
					}
				}
			}
			int Count = 0;
			while (Count < 4 && Near[Count] >= 0)
			{
				Count++;
			}
			if (Count == 0)
			{
				continue;
			}
			const int Place = Near[(int)(Rand(H) * Count) % Count];
			// the day and the hour that keep the fewest of them at work (of a few tries)
			int Day = 6, FewestBusy = 99;
			float Hour = O->From;
			for (int Try = 0; Try < 6 && FewestBusy > 0; Try++)
			{
				const int TryDay = Rand(H) < 0.45f ? 5 : 6;
				const float TryHour = O->From + (O->To - O->From) * Rand(H);
				int NumBusy = 0;
				for (const int Id : House.second)
				{
					NumBusy += Busy(W.People[(size_t)Id], TryDay, TryHour) ? 1 : 0;
				}
				if (NumBusy < FewestBusy)
				{
					FewestBusy = NumBusy;
					Day = TryDay;
					Hour = TryHour;
				}
			}
			for (const int Id : House.second)
			{
				FPerson& P = W.People[(size_t)Id];
				// (whoever the activity isn't for stays behind: a small child and the cafe's evening)
				P.OutingAct = Act;
				P.OutingDay = Day;
				P.OutingPlace = Place;
				P.OutingHour = Hour;
			}
		}
		// a standing date with a friend: everybody is paired with the first friend of theirs still free (in the order of
		// the town's list), and the two meet one weekday a week at their hour, at the same thing at the same place between
		// their homes: a talk, a bench, the cafe after work; the old in the daytime; children at the playground after school.
		struct FDate { const char* Activity; float From, To; int MinAge, MaxAge; };
		static const FDate Dates[] = {
			{ "chat", 17.5f, 20.f, 13, 200 }, { "coffee", 17.f, 19.5f, 16, 200 }, { "sit_bench", 17.f, 19.f, 13, 200 },
			{ "stroll", 17.5f, 20.f, 13, 200 }, { "eat_out", 18.f, 20.5f, 18, 200 }, { "chat", 16.f, 18.5f, 13, 200 },
		};
		static const FDate OldDates[] = {
			{ "sit_bench", 10.f, 13.f, 60, 200 }, { "feed_birds", 9.f, 11.5f, 60, 200 }, { "coffee", 10.5f, 13.f, 60, 200 },
			{ "stroll", 9.5f, 12.f, 60, 200 },
		};
		static const FDate KidDates[] = { { "playground", 15.f, 17.5f, 5, 12 }, { "play_run", 15.f, 17.5f, 5, 12 } };
		std::map<std::string, int> ByKey;
		for (FPerson& P : W.People)
		{
			P.DateAct = P.DateDay = P.DatePlace = P.DateWith = -1;
			ByKey[P.Key] = P.Id;
		}
		for (FPerson& P : W.People)
		{
			if (P.DateWith >= 0 || P.Home < 0)
			{
				continue;
			}
			for (const std::string& FriendKey : P.Friends)
			{
				const auto It = ByKey.find(FriendKey);
				if (It == ByKey.end() || It->second == P.Id)
				{
					continue;
				}
				FPerson& F = W.People[(size_t)It->second];
				const int Young = std::min(P.Age, F.Age), Old = std::max(P.Age, F.Age);
				if (F.DateWith >= 0 || F.Home < 0 || F.Household == P.Household || W.Places[(size_t)F.Home].Island != W.Places[(size_t)P.Home].Island)
				{
					continue;
				}
				const FDate* Pool = Young >= 60 ? OldDates : (Old <= 12 ? KidDates : Dates);
				const int Num = Young >= 60 ? (int)(sizeof(OldDates) / sizeof(FDate)) : (Old <= 12 ? (int)(sizeof(KidDates) / sizeof(FDate)) : (int)(sizeof(Dates) / sizeof(FDate)));
				uint32_t H = 2166136261u;
				for (const char C : P.Key + "+" + F.Key)
				{
					H = (H ^ (uint8_t)C) * 16777619u;
				}
				H ^= 0x3b9f5d17u;
				H |= 1u;
				const FDate* D = nullptr;
				int Act = -1;
				for (int Try = 0; Try < 8 && Act < 0; Try++)
				{
					const FDate& Choice = Pool[(int)(Rand(H) * Num) % Num];
					if (Young < Choice.MinAge || Old > Choice.MaxAge)
					{
						continue;
					}
					Act = W.FindActivity(Choice.Activity);
					D = &Choice;
				}
				if (Act < 0)
				{
					continue;
				}
				const FActivity& A = W.Activities[(size_t)Act];
				// the three nearest places of its kinds to the point between their doors
				const V3 Mid = (W.Places[(size_t)P.Home].Portal + W.Places[(size_t)F.Home].Portal) * 0.5f;
				int Near[3] = { -1, -1, -1 };
				float NearD[3] = { 1e30f, 1e30f, 1e30f };
				for (int Pi = 0; Pi < (int)W.Places.size(); Pi++)
				{
					const FPlace& Pl = W.Places[(size_t)Pi];
					if (Pl.Island != W.Places[(size_t)P.Home].Island || std::find(A.Places.begin(), A.Places.end(), Pl.Kind) == A.Places.end()
						|| Room(A, Pl) < 2)
					{
						continue;
					}
					float Dist = (Pl.Portal - Mid).Len2D();
					int Put = Pi;
					for (int K = 0; K < 3; K++)
					{
						if (Dist < NearD[K])
						{
							std::swap(Dist, NearD[K]);
							std::swap(Put, Near[K]);
						}
					}
				}
				int Count = 0;
				while (Count < 3 && Near[Count] >= 0)
				{
					Count++;
				}
				if (Count == 0)
				{
					continue;
				}
				P.DateAct = F.DateAct = Act;
				P.DatePlace = F.DatePlace = Near[(int)(Rand(H) * Count) % Count];
				// a weekday and an hour both are free at (of a few tries; else the last tried)
				for (int Try = 0; Try < 8; Try++)
				{
					P.DateDay = F.DateDay = (int)(Rand(H) * 5.f) % 5;
					P.DateHour = F.DateHour = D->From + (D->To - D->From) * Rand(H);
					if (!Busy(P, P.DateDay, P.DateHour) && !Busy(F, F.DateDay, F.DateHour))
					{
						break;
					}
				}
				P.DateWith = F.Id;
				F.DateWith = P.Id;
				break;
			}
		}
	}

	float FManners::Fits(const FWorld& W, const FPerson& P, const FPersona& Who, const FMannerState& S, const FMannerDef& D, const FMannerNow& Now) const
	{
		(void)S;
		if (D.bReactOnly || !((D.Poses >> (int)P.Pose) & 1u) || !Mannered(P))
		{
			return 0.f;
		}
		if ((D.Habit && !Who.Has(D.Habit)) || P.Age < D.MinAge || P.Age > D.MaxAge)
		{
			return 0.f;
		}
		if (D.Prop != EProp::None && !((PropsReady >> (int)D.Prop) & 1u))
		{
			return 0.f;
		}
		const bool bOut = !P.bIndoors;
		if ((D.Outdoors == 1 && !bOut) || (D.Outdoors == 0 && bOut))
		{
			return 0.f;
		}
		const bool bWet = bOut && Now.Rain > 0.2f;
		if ((D.Rain == 1 && !bWet) || (D.Rain == 0 && bWet))
		{
			return 0.f;
		}
		const bool bDay = Now.SunAlt > 0.f;
		if ((D.SunUp == 1 && !bDay) || (D.SunUp == 0 && bDay))
		{
			return 0.f;
		}
		if (D.bSunLow && !(Now.SunAlt > 0.f && Now.SunAlt < 14.f && Now.Cloud < 0.6f))
		{
			return 0.f;
		}
		if (D.bSkyShow && !(bOut && Now.SkyShow > 0.25f))
		{
			return 0.f;
		}
		if (D.bClear && Now.Cloud > 0.75f)
		{
			return 0.f;
		}
		// the cold and the heat are felt outdoors
		if ((D.TempBelow < 1e8f && !(bOut && Now.TempC < D.TempBelow)) || (D.TempAbove > -1e8f && !(bOut && Now.TempC > D.TempAbove)))
		{
			return 0.f;
		}
		if (!InHours(D.From, D.To, W.Hour))
		{
			return 0.f;
		}
		if (D.Need >= 0 && P.N[D.Need] < D.NeedOver)
		{
			return 0.f;
		}
		if (D.bWaiting && P.State != EState::Waiting)
		{
			return 0.f;
		}
		const int Act = P.State == EState::Doing ? P.Activity : -1;
		if (!D.DoingOk.empty() && (Act < 0 || Act >= (int)D.DoingOk.size() || !D.DoingOk[Act]))
		{
			return 0.f;
		}
		if (!D.AfterOk.empty())
		{
			// on the way from it (or doing the next thing) for a while: the life remembers what they did last
			if (P.PrevActivity < 0 || P.PrevActivity >= (int)D.AfterOk.size() || !D.AfterOk[P.PrevActivity]
				|| W.Time - P.PrevActivityEnd > (double)D.AfterSeconds)
			{
				return 0.f;
			}
		}
		if (D.Gather >= 0)
		{
			const FWorld::FGathering* G = P.State == EState::Doing && Act >= 0 && W.Activities[Act].bGather ? W.GatheringAt(P.Place) : nullptr;
			if (!G || (int)G->Mode != D.Gather)
			{
				return 0.f;
			}
		}
		if (!D.At.empty())
		{
			const int Place = P.State == EState::Doing ? P.Place : -1;
			if (Place < 0 || Place >= (int)W.Places.size() || !Among(D.At, W.Places[Place].Kind))
			{
				return 0.f;
			}
		}
		float Wt = D.Weight;
		if (!D.Trait.empty())
		{
			Wt *= 0.4f + 1.2f * PersonaTrait(Who, D.Trait);
		}
		if (D.bSkyShow)
		{
			Wt *= Now.SkyShow;
		}
		return Wt;
	}

	void FManners::End(FMannerState& S, float Clock)
	{
		if (S.Cur != EManner::None)
		{
			S.Last[(int)S.Cur] = Clock;
		}
		S.Cur = EManner::None;
		S.Def = -1;
		S.bReact = false;
	}

	void FManners::Pick(const FWorld& W, const FPerson& P, FMannerState& S, const FMannerNow& Now)
	{
		const FPersona& Who = Personas[P.Id];
		Cand.clear();
		CandW.clear();
		// the calm do nothing in particular more of the time
		float Sum = IdleWeight * (1.3f - 0.6f * Who.Nerve);
		for (int I = 0; I < (int)Defs.size(); I++)
		{
			const FMannerDef& D = Defs[I];
			if (D.bForce || Now.Clock - S.Last[(int)D.Manner] < D.Cooldown)
			{
				continue;
			}
			const float Wt = Fits(W, P, Who, S, D, Now);
			if (Wt > 0.f)
			{
				Cand.push_back(I);
				CandW.push_back(Wt);
				Sum += Wt;
			}
		}
		float R = Rand(S.Rng) * Sum;
		for (size_t I = 0; I < Cand.size(); I++)
		{
			R -= CandW[I];
			if (R <= 0.f)
			{
				const FMannerDef& D = Defs[Cand[I]];
				S.Cur = D.Manner;
				S.Def = Cand[I];
				S.Start = Now.Clock;
				S.Until = Now.Clock + D.SecMin + (D.SecMax - D.SecMin) * Rand(S.Rng);
				return;
			}
		}
		S.Cur = EManner::None;
		S.Def = -1;
		S.Start = Now.Clock;
		S.Until = Now.Clock + 5.f + 9.f * Rand(S.Rng);
	}

	void FManners::Update(const FWorld& W, int Person, const FMannerNow& Now)
	{
		if (Person < 0 || Person >= (int)States.size() || Person >= (int)W.People.size())
		{
			return;
		}
		FMannerState& S = States[Person];
		const FPerson& P = W.People[Person];
		if (Forced != EManner::Num)
		{
			if (S.Cur != Forced)
			{
				End(S, Now.Clock);
				S.Cur = Forced;
				S.Def = RowOf[(int)Forced];
				S.Start = Now.Clock;
			}
			S.Until = Now.Clock + 1.f;
			return;
		}
		if (S.bReact)
		{
			// a wave runs out by itself; sitting down or lying down ends it at once
			if (Now.Clock >= S.Until || !Mannered(P) || P.Pose == EPose::Sleep)
			{
				End(S, Now.Clock);
				S.Until = Now.Clock + 1.f + 3.f * Rand(S.Rng);
			}
			return;
		}
		// the pose changed under the manner (stood up, walked off, went in at a door): it ends now, without waiting for the
		// whole look
		const FMannerDef* Cur = DefOf(S);
		if (S.Cur != EManner::None && (!Cur || !((Cur->Poses >> (int)P.Pose) & 1u) || !Mannered(P) || (Cur->Outdoors == 1 && P.bIndoors)))
		{
			End(S, Now.Clock);
			S.Until = Now.Clock + 1.f + 4.f * Rand(S.Rng);
			Cur = nullptr;
		}
		if (Now.Clock >= S.CheckAt)
		{
			// once a second, whole: what must be done now wins (an umbrella in the rain, the rod at the water), and a manner
			// whose moment has passed ends (the rain stopped)
			S.CheckAt = Now.Clock + 0.8f + 0.4f * Rand(S.Rng);
			const FPersona& Who = Personas[Person];
			int Must = -1;
			for (int I = 0; I < (int)Defs.size() && Must < 0; I++)
			{
				if (Defs[I].bForce && Fits(W, P, Who, S, Defs[I], Now) > 0.f)
				{
					Must = I;
				}
			}
			if (Must >= 0)
			{
				if (S.Def != Must)
				{
					End(S, Now.Clock);
					S.Cur = Defs[Must].Manner;
					S.Def = Must;
					S.Start = Now.Clock;
				}
				S.Until = Now.Clock + 2.f;
				return;
			}
			if (Cur && (Cur->bForce || Fits(W, P, Who, S, *Cur, Now) <= 0.f))
			{
				End(S, Now.Clock);
				S.Until = Now.Clock + 1.f + 4.f * Rand(S.Rng);
			}
			else if (Now.SkyShow > 0.25f && !P.bIndoors && !S.bReact && !(Cur && Cur->bSkyShow) && S.Until - Now.Clock > 2.f
				&& (S.Cur == EManner::None || Now.Clock - S.Start > 8.f)
				&& Rand(S.Rng) < 0.2f * Now.SkyShow)
			{
				// something in the sky: whatever their hands were at can wait (the pick below sees the sky's rows)
				End(S, Now.Clock);
				S.Until = Now.Clock;
			}
		}
		if (Now.Clock >= S.Until)
		{
			End(S, Now.Clock);
			if (Mannered(P))
			{
				Pick(W, P, S, Now);
			}
			else
			{
				S.Until = Now.Clock + 2.f;
			}
		}
	}

	void FManners::Release(int Person, float Clock)
	{
		if (Person < 0 || Person >= (int)States.size())
		{
			return;
		}
		FMannerState& S = States[Person];
		End(S, Clock);
		S.Until = Clock + 1.f + 5.f * Rand(S.Rng);
		S.LookAt = -2;
		S.bTurn = false;
		S.NearSince = -1.f;
	}

	bool FManners::React(int Person, EManner M, float Clock, float Seconds, bool bEvenBusy)
	{
		if (Person < 0 || Person >= (int)States.size() || M >= EManner::Num || M == EManner::None)
		{
			return false;
		}
		FMannerState& S = States[Person];
		if (!bEvenBusy && (PropOf(S) != EProp::None || S.bReact))
		{
			return false;
		}
		End(S, Clock);
		S.Cur = M;
		S.Def = RowOf[(int)M];
		S.bReact = true;
		S.Start = Clock;
		S.Until = Clock + Seconds;
		return true;
	}

	ENoticeEvent FManners::TakeEvent(int Person)
	{
		if (Person < 0 || Person >= (int)States.size())
		{
			return ENoticeEvent::None;
		}
		const ENoticeEvent E = States[Person].Event;
		States[Person].Event = ENoticeEvent::None;
		return E;
	}

	void FManners::Notice(const FWorld& W, int Person, const FMannerNow& Now, const FHeroNow& Hero)
	{
		if (Person < 0 || Person >= (int)States.size() || Person >= (int)W.People.size())
		{
			return;
		}
		FMannerState& S = States[Person];
		const FPerson& P = W.People[Person];
		const FPersona& Who = Personas[Person];
		if (S.LookAt != -2 && Now.Clock >= S.LookUntil)
		{
			S.LookAt = -2;
			S.bTurn = false;
		}
		const bool bAwake = Mannered(P) && P.Pose != EPose::Sleep && P.Pose != EPose::Hidden && P.Pose != EPose::Computer
			&& P.Pose != EPose::Work && P.Pose != EPose::Shovel;
		const V3 To = Hero.Pos - P.Pos;
		const float D = To.Len2D();
		// in the water: seen from the promenade above too, and from further off
		if (Hero.bThere && Hero.bSwim && bAwake && !P.bIndoors && D < 32.f && std::fabs(To.Z) < 9.f && Now.Clock >= S.EventCool
			&& S.Event == ENoticeEvent::None && (P.Pose == EPose::Stand || P.Pose == EPose::Look || P.Pose == EPose::Sit)
			&& OffFacing(P, Hero.Pos) < 75.f && Rand(S.Rng) < 0.04f + 0.08f * Who.Curiosity)
		{
			S.Event = ENoticeEvent::HeroSwims;
			S.EventCool = Now.Clock + 240.f;
			S.LookAt = -1;
			S.LookUntil = std::max(S.LookUntil, Now.Clock + 5.f);
		}
		if (!Hero.bThere || !bAwake || std::fabs(To.Z) > 2.5f || D > 14.f)
		{
			S.NearSince = -1.f;
			if (S.LookAt == -1 && (!Hero.bThere || !bAwake))
			{
				S.LookAt = -2;
				S.bTurn = false;
			}
			return;
		}
		const bool bKid = P.Age < 12;
		const float Speed = Hero.Vel.Len2D();
		const bool bRun = Speed > 3.2f;
		const bool bNight = Now.SunAlt < -4.f;
		const float Off = OffFacing(P, Hero.Pos);
		const bool bFront = Off < 100.f;
		// reading and eating people look up less
		const float Busy = P.Pose == EPose::Read || P.Pose == EPose::Eat || S.Cur == EManner::Newspaper || S.Cur == EManner::PhoneScroll
			|| S.Cur == EManner::Knit || S.Cur == EManner::Doze ? 0.35f : 1.f;
		float Range = 4.5f + 3.5f * Who.Curiosity + (bKid ? 2.f : 0.f);
		if (bRun)
		{
			Range = std::max(Range, 11.f);
		}
		if (Hero.bLamp && bNight)
		{
			Range = std::max(Range, 12.f);
		}
		const bool bSees = D < Range && (bFront || D < 2.2f || (bRun && D < 7.f));
		if (bSees && S.LookAt != -1 && Now.Clock >= S.LookCool && S.Cur != EManner::Doze)
		{
			// called a few times a second: the curious look up soon, the others may let him pass
			const float Chance = (0.12f + 0.3f * Who.Curiosity + (bRun || bKid || (Hero.bLamp && bNight) ? 0.35f : 0.f)) * Busy;
			if (Rand(S.Rng) < Chance)
			{
				S.LookAt = -1;
				S.LookUntil = Now.Clock + (1.5f + 3.5f * Who.Curiosity) * (bKid ? 1.8f : 1.f) * (bRun ? 1.4f : 1.f);
				S.LookCool = S.LookUntil + 5.f + 14.f * (1.f - Who.Curiosity);
				// children turn round to watch; anybody turns to somebody running up to them
				S.bTurn = (bKid || (bRun && D < 6.f)) && (P.Pose == EPose::Stand || P.Pose == EPose::Look);
			}
		}
		// things worth a word
		const bool bComing = Hero.Vel.X * (P.Pos.X - Hero.Pos.X) + Hero.Vel.Y * (P.Pos.Y - Hero.Pos.Y) > 0.f;
		if (D < 1.1f && Speed < 0.6f && !IsSeated(P.Pose))
		{
			if (S.NearSince < 0.f)
			{
				S.NearSince = Now.Clock;
			}
		}
		else
		{
			S.NearSince = -1.f;
		}
		if (Now.Clock < S.EventCool || S.Event != ENoticeEvent::None)
		{
			return;
		}
		if (bRun && bComing && D < (Hero.bBike ? 6.f : 4.5f) && bFront)
		{
			// tearing past on foot, or on a bicycle
			S.Event = Hero.bBike ? ENoticeEvent::HeroRides : ENoticeEvent::HeroRuns;
			S.EventCool = Now.Clock + 45.f;
		}
		else if (Hero.bCrouch && !Hero.bBike && bComing && Speed > 0.2f && Speed < 2.4f && D < 2.4f && Off > 100.f && !IsSeated(P.Pose))
		{
			// creeping up behind their back: they feel it, start and turn round
			S.Event = ENoticeEvent::HeroSneaks;
			S.EventCool = Now.Clock + 90.f;
			S.bTurn = P.Pose == EPose::Stand || P.Pose == EPose::Look;
		}
		else if (S.NearSince >= 0.f && Now.Clock - S.NearSince > 3.5f + 3.f * Who.Warmth)
		{
			S.Event = ENoticeEvent::HeroTooClose;
			S.EventCool = Now.Clock + 60.f;
			S.NearSince = -1.f;
		}
		else if (Hero.bLamp && bNight && D < 4.f && Off < 50.f && !bRun)
		{
			S.Event = ENoticeEvent::HeroLamp;
			S.EventCool = Now.Clock + 90.f;
		}
		else if (Hero.bSeated && !P.bIndoors && IsSeated(P.Pose) && D < 2.4f && Rand(S.Rng) < 0.06f + 0.1f * Who.Warmth)
		{
			// he sat down beside them on the bench
			S.Event = ENoticeEvent::HeroSits;
			S.EventCool = Now.Clock + 600.f;
		}
		else if (Hero.bFishing && !Hero.bSeated && D < 7.f && bFront && Speed < 0.6f && P.Pose != EPose::Walk && S.Cur != EManner::Fish
			&& Rand(S.Rng) < 0.02f + 0.06f * Who.Curiosity)
		{
			// his line is out near them: the curious have a question
			S.Event = ENoticeEvent::HeroFishes;
			S.EventCool = Now.Clock + 300.f;
		}
		if (S.Event != ENoticeEvent::None)
		{
			S.LookAt = -1;
			S.LookUntil = std::max(S.LookUntil, Now.Clock + 3.f);
		}
	}

	void FManners::Passing(const FWorld& W, const int* Shown, int NumShown, const FMannerNow& Now)
	{
		for (int I = 0; I < NumShown; I++)
		{
			const int A = Shown[I];
			if (A < 0 || A >= (int)States.size())
			{
				continue;
			}
			const FPerson& PA = W.People[A];
			if (PA.Pose != EPose::Walk || !Mannered(PA))
			{
				continue;
			}
			FMannerState& SA = States[A];
			for (int J = 0; J < NumShown; J++)
			{
				const int B = Shown[J];
				if (B == A || B < 0 || B >= (int)States.size() || PA.Partner == B)
				{
					continue;
				}
				const FPerson& PB = W.People[B];
				const V3 To = PB.Pos - PA.Pos;
				if (std::fabs(To.X) > 5.f || std::fabs(To.Y) > 5.f || std::fabs(To.Z) > 1.5f)
				{
					continue;
				}
				const float D = To.Len2D();
				if (D > 5.f || D < 0.3f || !Mannered(PB) || PB.Pose == EPose::Sleep || OffFacing(PA, PB.Pos) > 65.f)
				{
					continue;
				}
				FMannerState& SB = States[B];
				// a glance at whoever comes the other way
				if (SA.LookAt == -2 && Now.Clock >= SA.LookCool)
				{
					SA.LookAt = B;
					SA.LookUntil = Now.Clock + 1.f + 1.5f * Personas[A].Curiosity;
					SA.LookCool = SA.LookUntil + 4.f;
				}
				// people who know each other: a wave or a nod, answered
				const bool bKnow = PA.Household == PB.Household || Among(PA.Friends, PB.Key) || Among(PB.Friends, PA.Key);
				if (!bKnow || Now.Clock - SA.Last[(int)EManner::Wave] < 90.f || Now.Clock - SA.Last[(int)EManner::Nod] < 90.f
					|| SA.Cur == EManner::Wave || SA.Cur == EManner::Nod)
				{
					continue;
				}
				const EManner Mine = Personas[A].Warmth > 0.5f ? EManner::Wave : EManner::Nod;
				if (React(A, Mine, Now.Clock, Mine == EManner::Wave ? 2.4f : 1.6f))
				{
					if (SB.LookAt == -2)
					{
						SB.LookAt = A;
						SB.LookUntil = Now.Clock + 2.5f;
						SB.LookCool = SB.LookUntil + 4.f;
					}
					if (PB.Pose != EPose::Read && PB.Pose != EPose::Eat && PB.Pose != EPose::Computer && PB.Pose != EPose::Work)
					{
						const EManner Theirs = Personas[B].Warmth > 0.5f ? EManner::Wave : EManner::Nod;
						React(B, Theirs, Now.Clock, Theirs == EManner::Wave ? 2.6f : 1.8f);
					}
				}
				else
				{
					// hands full: at least it isn't tried again every scan
					SA.Last[(int)EManner::Nod] = Now.Clock;
				}
			}
		}
	}

	std::string FManners::Describe(const FWorld& W, int Person) const
	{
		if (Person < 0 || Person >= (int)Personas.size() || Person >= (int)W.People.size())
		{
			return std::string();
		}
		const FPersona& Who = Personas[Person];
		const FMannerState& S = States[Person];
		char Buf[256];
		std::snprintf(Buf, sizeof Buf, "%s: warmth %.2f nerve %.2f curiosity %.2f tidy %.2f%s; habits", W.People[Person].Key.c_str(),
			Who.Warmth, Who.Nerve, Who.Curiosity, Who.Tidy, Who.bLeftHanded ? " left-handed" : "");
		std::string Out = Buf;
		for (int I = 0; I < NumHabits; I++)
		{
			if (Who.Habits & (1u << I))
			{
				Out += " ";
				Out += HabitNames[I];
			}
		}
		const FPerson& P = W.People[Person];
		if (P.RitualAct >= 0 && P.RitualAct < (int)W.Activities.size())
		{
			std::snprintf(Buf, sizeof Buf, "; ritual %s at %d:%02d", W.Activities[(size_t)P.RitualAct].Id.c_str(), (int)P.RitualHour,
				(int)((P.RitualHour - (int)P.RitualHour) * 60.f));
			Out += Buf;
		}
		if (P.OutingAct >= 0 && P.OutingAct < (int)W.Activities.size() && P.OutingPlace >= 0 && P.OutingPlace < (int)W.Places.size())
		{
			std::snprintf(Buf, sizeof Buf, "; the household's outing %s at %s on %s at %d:%02d", W.Activities[(size_t)P.OutingAct].Id.c_str(),
				W.Places[(size_t)P.OutingPlace].Id.c_str(), P.OutingDay == 5 ? "Saturday" : "Sunday", (int)P.OutingHour,
				(int)((P.OutingHour - (int)P.OutingHour) * 60.f));
			Out += Buf;
		}
		if (P.DateAct >= 0 && P.DateAct < (int)W.Activities.size() && P.DatePlace >= 0 && P.DatePlace < (int)W.Places.size() && P.DateWith >= 0
			&& P.DateWith < (int)W.People.size())
		{
			static const char* const Days[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
			std::snprintf(Buf, sizeof Buf, "; a date with %s: %s at %s on %ss at %d:%02d", W.People[(size_t)P.DateWith].Key.c_str(),
				W.Activities[(size_t)P.DateAct].Id.c_str(), W.Places[(size_t)P.DatePlace].Id.c_str(), Days[P.DateDay % 7], (int)P.DateHour,
				(int)((P.DateHour - (int)P.DateHour) * 60.f));
			Out += Buf;
		}
		Out += "; now ";
		Out += MannerName(S.Cur);
		return Out;
	}
}
