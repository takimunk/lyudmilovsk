// The town paper «Людмиловская правда» in plain C++ (docs/town_paper.md): every morning an issue is set from the game's own
// news (the next day's weather, the tides, the festivals today and soon, the market, the fish that bite, the residents'
// birthdays and anniversaries, the cinema, what the hero did yesterday) by templates with slots in Content/City/town_paper.json
// (written by tools/paper/paper.py) and the words of Content/Localization/paper/strings_<lang>.json.
//
// Engine-free, so the same code sets the paper in the game (ULyudTownPaper gathers the facts) and in tools/papersim (a year of
// made-up days checked offline). An issue holds keys and arguments only, never text: it is kept in the save as it was set and
// is shown in whatever language the game speaks when it is read. An argument starting with '@' is a key itself.
#pragma once

#include "../People/MiniJson.h"
#include "../Festival/FestivalCalendar.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace LyudPaper
{
	/** One paragraph of a page: a key and its arguments ("@key" is a key, anything else is shown as it is). */
	struct FLine
	{
		std::string Key;
		std::vector<std::string> Args;
	};
	using FPage = std::vector<FLine>;

	/** One day's paper. Front is the front story's signature (key and arguments): two days running never share one. */
	struct FIssue
	{
		int Since = 0;
		int Number = 0;
		LyudFest::FDate Date;
		std::string Front;
		std::vector<FPage> Pages;
	};

	/** The next day's weather as the paper prints it. Sky: clear, fair, cloudy, overcast, drizzle, rain, showers, thunder,
	 *  sleet, snow, fog. Extreme (the front page): storm (wind >= 15 m/s), thunder, heat (>= 27 C), frost (<= -12 C), snow
	 *  (a snowy day after a snowless one), fog. */
	struct FWeather
	{
		bool bKnown = false;
		std::string Sky = "cloudy";
		int TMin = 0, TMax = 0;
		int WindMs = 0;
		int WindFromDeg = 0;
		/** Sea surface, C (< -50: not printed). */
		int SeaC = -100;
		/** Northern lights likely tonight. */
		bool bAurora = false;
		/** The snow lies for the first time this winter (the town notices). */
		bool bFirstSnow = false;
	};

	/** A turn of the tide tomorrow: minutes from midnight, high or low water, level over the datum, cm. */
	struct FTide
	{
		int Minute = 0;
		bool bHigh = false;
		int LevelCm = 0;
	};

	/** Something the hero did on the day before the issue. Kind: tamed (What: species id), rare (a rare species seen for the
	 *  first time), seen (any other first sighting), record (What: fish species, N: grams, the biggest of its kind he ever took),
	 *  catch (the biggest fish of the day, N grams), quest (What: quest id finished), win (What: a game won), contest (the
	 *  photo club's monthly contest was judged that day, the 26th, and the hero had a print in it: N his place 1..6, What
	 *  the map place the print was taken at or empty; ULyudPhoto::ContestNews. Place 1 is a front story, 2 and 3 a brief,
	 *  4..6 nothing; the theme is the month's, ContestTheme). */
	struct FDeed
	{
		std::string Kind;
		std::string What;
		int N = 0;
	};

	/** A club tournament (ULyudCityThings::GetCupNews): its name and venue keys, the first round (minutes from midnight) and,
	 *  once over, the champion and the runner-up (name keys; empty: the hero) and how far the hero got (-1 not in it, 0..2 out
	 *  in that round, 3 champion). */
	struct FCup
	{
		std::string Title, Venue, Winner, RunnerUp;
		int StartMinute = 0;
		int HeroReached = -1;
		bool bOver = false;
	};

	/** What the game knows on the morning of a day; the rest (residents, market, pools) is the paper's data. */
	struct FFacts
	{
		int Since = 0;
		LyudFest::FDate Date;
		FWeather Tomorrow;
		std::vector<FTide> Tides;
		std::vector<FDeed> Deeds;
		/** The fish species that bite best now against the usual (empty: none stands out). */
		std::string BiteFish;
		/** The front signature of the day before's issue (no front repeats). */
		std::string PrevFront;
		/** Festivals from the calendar: filled by FPaper::Festivals unless the caller set them. */
		std::vector<std::pair<std::string, int>> Festivals; // (id, days ahead: 0 today, negative: started that many days ago)
		/** The tournaments of the day before (over) and today's (to come). */
		std::vector<FCup> CupsYesterday;
		std::vector<FCup> CupsToday;
	};

	struct FPoolEntry
	{
		std::string Key;
		std::vector<int> Months;   // empty: any month
		bool Fits(int M) const { return Months.empty() || std::find(Months.begin(), Months.end(), M) != Months.end(); }
	};

	struct FBirthday
	{
		std::string Who;   // townsfolk key (npc.<key>.name)
		int Month = 1, Day = 1, Born = 1990;
	};

	struct FAnniversary
	{
		std::string A, B;
		int Month = 1, Day = 1, Year = 2000;
	};

	struct FGood
	{
		std::string Item;
		int Price = 0;
		std::vector<int> Months;
	};

	/** 32-bit mix (the same in the game and the tests). */
	inline uint32_t Hash(uint32_t X)
	{
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15; X *= 0x846ca68bU; X ^= X >> 16;
		return X;
	}

	class FPaper
	{
	public:
		std::string LastError;
		int FirstIssue = 1;
		std::vector<std::string> RareBirds;
		std::vector<std::string> FestBlurbs;      // festivals with paper.fest.<id>.<n> stories of their own
		std::vector<FBirthday> Birthdays;
		std::vector<FAnniversary> Anniversaries;
		std::vector<FGood> Market;
		std::vector<FPoolEntry> Fronts, Town, Ads;
		/** How many variants (1..n) each template kind has: "front.fest_today" -> 3 means paper.front.fest_today.1..3. */
		std::vector<std::pair<std::string, int>> Variants;
		std::vector<std::string> CinemaKeys;      // by weekday Mon..Sun, then the festival one
		std::vector<std::string> NewsreelKeys;    // the old films before the sessions, the same way (docs/cinema.md); may be absent
		LyudFest::FCalendar Calendar;
		bool bCalendar = false;

		bool Load(const std::string& JsonText)
		{
			LyudJson::FValue Root;
			std::string Error;
			if (!LyudJson::Parse(JsonText, Root, Error))
			{
				LastError = "town_paper.json: " + Error;
				return false;
			}
			FirstIssue = Root["first_issue"].I(1);
			for (const LyudJson::FValue& V : Root["rare_birds"].Items) RareBirds.push_back(V.S());
			for (const LyudJson::FValue& V : Root["fest_blurbs"].Items) FestBlurbs.push_back(V.S());
			for (const LyudJson::FValue& V : Root["cinema"].Items) CinemaKeys.push_back(V.S());
			for (const LyudJson::FValue& V : Root["newsreel"].Items) NewsreelKeys.push_back(V.S());
			for (const LyudJson::FValue& V : Root["birthdays"].Items)
			{
				FBirthday B;
				B.Who = V["who"].S();
				ReadMd(V["md"].S(), B.Month, B.Day);
				B.Born = V["born"].I(1990);
				Birthdays.push_back(B);
			}
			for (const LyudJson::FValue& V : Root["anniversaries"].Items)
			{
				FAnniversary A;
				A.A = V["a"].S();
				A.B = V["b"].S();
				ReadMd(V["md"].S(), A.Month, A.Day);
				A.Year = V["year"].I(2000);
				Anniversaries.push_back(A);
			}
			for (const LyudJson::FValue& V : Root["market"].Items)
			{
				FGood G;
				G.Item = V["item"].S();
				G.Price = V["price"].I();
				for (const LyudJson::FValue& M : V["months"].Items) G.Months.push_back(M.I());
				Market.push_back(G);
			}
			ReadPool(Root["fronts"], Fronts);
			ReadPool(Root["town"], Town);
			ReadPool(Root["ads"], Ads);
			for (const auto& F : Root["variants"].Fields) Variants.push_back({ F.first, std::max(1, F.second.I(1)) });
			if (Fronts.size() < 2 || Town.empty() || Ads.size() < 3 || CinemaKeys.size() < 7)
			{
				LastError = "town_paper.json: needs 2+ fronts, town stories, 3+ ads and 7 cinema keys";
				return false;
			}
			return true;
		}

		/** The festival calendar (Content/PeopleData/festivals.json) for the days' festivals. */
		bool LoadCalendar(const std::string& JsonText)
		{
			bCalendar = Calendar.Load(JsonText);
			if (!bCalendar)
			{
				LastError = "festivals.json: " + Calendar.LastError;
			}
			return bCalendar;
		}

		/** The festivals on Date (0, or minus the days since a long one began) and starting in the next 14 days. Weekly
		 *  ones (the market) are left out. */
		std::vector<std::pair<std::string, int>> Festivals(const LyudFest::FDate& Date) const
		{
			std::vector<std::pair<std::string, int>> Out;
			if (!bCalendar)
			{
				return Out;
			}
			const int D0 = LyudFest::DaysFromCivil(Date);
			for (int Fi = 0; Fi < (int)Calendar.Festivals.size(); Fi++)
			{
				const LyudFest::FFestival& F = Calendar.Festivals[(size_t)Fi];
				if (F.Rule == "weekly" || F.Rule == "trigger")
				{
					continue;
				}
				for (int Back = std::max(1, F.Days) - 1; Back >= 0; --Back)
				{
					if (Calendar.StartsOn(Fi, LyudFest::CivilFromDays(D0 - Back)))
					{
						Out.push_back({ F.Id, -Back });
						break;
					}
				}
			}
			for (const auto& U : Calendar.Upcoming(LyudFest::CivilFromDays(D0 + 1), 14))
			{
				const LyudFest::FFestival& F = Calendar.Festivals[(size_t)U.second];
				if (F.Rule != "weekly" && F.Rule != "trigger")
				{
					Out.push_back({ F.Id, LyudFest::DaysFromCivil(U.first) - D0 });
				}
			}
			std::stable_sort(Out.begin(), Out.end(), [](const auto& L, const auto& R) { return L.second < R.second; });
			return Out;
		}

		/** Sets the issue of a day. */
		FIssue Build(FFacts Facts) const
		{
			if (Facts.Festivals.empty())
			{
				Facts.Festivals = Festivals(Facts.Date);
			}
			FIssue Out;
			Out.Since = Facts.Since;
			Out.Number = FirstIssue + Facts.Since;
			Out.Date = Facts.Date;
			std::vector<FDeed> Briefs;
			std::string CupOnFront;
			FPage Front = FrontPage(Facts, Out, Briefs, CupOnFront);
			Out.Pages.push_back(std::move(Front));
			Out.Pages.push_back(TownPage(Facts, Briefs, CupOnFront));
			Out.Pages.push_back(WeatherPage(Facts));
			Out.Pages.push_back(BillsPage(Facts));
			return Out;
		}

		/** The photo contest's theme of a calendar month (docs/photo.md: the same order as the contest's own code): its
		 *  name is the key paper.contest.theme.<id>. */
		static const char* ContestTheme(int Month)
		{
			static const char* const Themes[4] = { "sea", "birds", "work", "festival" };
			return Themes[(((Month - 1) % 4) + 4) % 4];
		}

		/** The photo club's contest by the day of the month: prints are taken on days 1..ContestLastDay, the jury sits on
		 *  ContestJuryDay (so the issue of the day after tells the hero's place). */
		static constexpr int ContestLastDay = 25;
		static constexpr int ContestJuryDay = 26;

		/** The masthead line of an issue (also the archive's line for it). */
		static FLine Masthead(const FIssue& I)
		{
			return { "paper.masthead", { std::to_string(I.Number), std::to_string(I.Date.D), "@paper.month.gen." + std::to_string(I.Date.M),
				std::to_string(I.Date.Y), "@paper.dow." + std::to_string(LyudFest::Weekday(I.Date)) } };
		}

		/** The front story of an issue (its second line). */
		static const FLine* FrontLine(const FIssue& I)
		{
			return !I.Pages.empty() && I.Pages[0].size() > 1 ? &I.Pages[0][1] : nullptr;
		}

		static std::string Signature(const FLine& L)
		{
			std::string S = L.Key;
			for (const std::string& A : L.Args)
			{
				S += '|';
				S += A;
			}
			return S;
		}

		/** Every key an issue uses (its lines' and its '@' arguments'), for the checks. */
		static void Keys(const FIssue& I, std::set<std::string>& Out)
		{
			for (const FPage& P : I.Pages)
			{
				for (const FLine& L : P)
				{
					Out.insert(L.Key);
					for (const std::string& A : L.Args)
					{
						if (!A.empty() && A[0] == '@') Out.insert(A.substr(1));
					}
				}
			}
		}

		// --- the archive kept in the save: one string per issue, fields split by control characters
		static std::string Pack(const FIssue& I)
		{
			std::string S = std::to_string(I.Since) + '\x1f' + std::to_string(I.Number) + '\x1f' + std::to_string(I.Date.Y) + '\x1f' +
				std::to_string(I.Date.M) + '\x1f' + std::to_string(I.Date.D) + '\x1f' + I.Front;
			for (const FPage& P : I.Pages)
			{
				S += '\x1d';
				bool bFirst = true;
				for (const FLine& L : P)
				{
					if (!bFirst) S += '\x1e';
					bFirst = false;
					S += L.Key;
					for (const std::string& A : L.Args)
					{
						S += '\x1f';
						S += A;
					}
				}
			}
			return S;
		}

		static bool Unpack(const std::string& S, FIssue& Out)
		{
			const std::vector<std::string> Parts = Split(S, '\x1d');
			if (Parts.empty())
			{
				return false;
			}
			const std::vector<std::string> Head = Split(Parts[0], '\x1f');
			if (Head.size() < 5)
			{
				return false;
			}
			Out = FIssue();
			Out.Since = std::atoi(Head[0].c_str());
			Out.Number = std::atoi(Head[1].c_str());
			Out.Date.Y = std::atoi(Head[2].c_str());
			Out.Date.M = std::atoi(Head[3].c_str());
			Out.Date.D = std::atoi(Head[4].c_str());
			for (size_t I = 5; I < Head.size(); ++I)
			{
				Out.Front += (I > 5 ? "\x1f" : "") + Head[I];
			}
			for (size_t P = 1; P < Parts.size(); ++P)
			{
				FPage Page;
				for (const std::string& LineS : Split(Parts[P], '\x1e'))
				{
					const std::vector<std::string> F = Split(LineS, '\x1f');
					if (F.empty() || F[0].empty()) continue;
					FLine L;
					L.Key = F[0];
					L.Args.assign(F.begin() + 1, F.end());
					Page.push_back(std::move(L));
				}
				Out.Pages.push_back(std::move(Page));
			}
			return !Out.Pages.empty();
		}

		int VariantsOf(const std::string& Kind) const
		{
			for (const auto& V : Variants)
			{
				if (V.first == Kind) return V.second;
			}
			return 1;
		}

	private:
		static void ReadMd(const std::string& S, int& M, int& D)
		{
			if (S.size() >= 5)
			{
				M = std::atoi(S.substr(0, 2).c_str());
				D = std::atoi(S.substr(3, 2).c_str());
			}
		}

		static void ReadPool(const LyudJson::FValue& V, std::vector<FPoolEntry>& Out)
		{
			for (const LyudJson::FValue& E : V.Items)
			{
				FPoolEntry P;
				if (E.Type == LyudJson::FValue::String)
				{
					P.Key = E.Str;
				}
				else
				{
					P.Key = E["k"].S();
					for (const LyudJson::FValue& M : E["months"].Items) P.Months.push_back(M.I());
				}
				if (!P.Key.empty()) Out.push_back(P);
			}
		}

		static std::vector<std::string> Split(const std::string& S, char C)
		{
			std::vector<std::string> Out;
			size_t From = 0;
			for (;;)
			{
				const size_t At = S.find(C, From);
				Out.push_back(S.substr(From, At == std::string::npos ? std::string::npos : At - From));
				if (At == std::string::npos) break;
				From = At + 1;
			}
			return Out;
		}

		static bool Has(const std::vector<std::string>& V, const std::string& S)
		{
			return std::find(V.begin(), V.end(), S) != V.end();
		}

		static std::string Hm(int Minute)
		{
			char B[8];
			std::snprintf(B, sizeof(B), "%02d:%02d", (Minute / 60) % 24, Minute % 60);
			return B;
		}

		/** A template kind's variants as candidate lines, starting at the day's pick (so a repeat falls to the next one). */
		void AddVariants(std::vector<FLine>& Into, const std::string& Kind, const std::vector<std::string>& Args, int Since) const
		{
			const int N = VariantsOf(Kind);
			for (int I = 0; I < N; ++I)
			{
				const int V = (Since + I) % N + 1;
				Into.push_back({ "paper." + Kind + "." + std::to_string(V), Args });
			}
		}

		/** The weather worth a front page, or "". */
		static std::string Extreme(const FWeather& W)
		{
			if (!W.bKnown) return "";
			if (W.bFirstSnow) return "first_snow";
			if (W.Sky == "thunder") return "thunder";
			if (W.WindMs >= 15) return "storm";
			if (W.TMax >= 27) return "heat";
			if (W.TMin <= -12) return "frost";
			if (W.Sky == "fog") return "fog";
			return "";
		}

		FPage FrontPage(const FFacts& F, FIssue& Out, std::vector<FDeed>& Briefs, std::string& CupOnFront) const
		{
			FPage Page;
			Page.push_back(Masthead(Out));
			std::vector<FLine> Cand;
			std::vector<const FDeed*> Used;
			auto Deed = [&](const char* Kind, const char* What = nullptr) -> const FDeed*
			{
				for (const FDeed& D : F.Deeds)
				{
					if (D.Kind == Kind && (!What || D.What == What)) return &D;
				}
				return nullptr;
			};
			auto DeedLines = [&](const FDeed* D, const std::string& Kind, const std::vector<std::string>& Args)
			{
				if (D)
				{
					AddVariants(Cand, Kind, Args, F.Since);
					Used.push_back(D);
				}
			};
			// 1. the hero's big deeds of yesterday
			if (const FDeed* D = Deed("tamed", "MuteSwan"))
			{
				DeedLines(D, "front.tamed_swan", {});
			}
			if (const FDeed* D = Deed("record"))
			{
				DeedLines(D, "front.record", { "@fish." + Lower(D->What) + ".name", std::to_string(D->N) });
			}
			for (const FCup& C : F.CupsYesterday)
			{
				if (C.bOver && C.HeroReached == 3)
				{
					AddVariants(Cand, "front.cup", { "@" + C.Title, C.RunnerUp.empty() ? std::string("@paper.cup.nobody") : "@" + C.RunnerUp }, F.Since);
				}
			}
			// the photo contest judged yesterday: the hero's print took the first place, the paper prints it
			if (const FDeed* D = Deed("contest"))
			{
				if (D->N == 1)
				{
					const std::string Theme = std::string("@paper.contest.theme.") + ContestTheme(LyudFest::AddDays(F.Date, -1).M);
					if (D->What.empty())
					{
						DeedLines(D, "front.contest_noplace", { Theme });
					}
					else
					{
						DeedLines(D, "front.contest", { Theme, "@place." + D->What + ".name" });
					}
				}
			}
			// 2. a festival today
			std::string Today;
			int TodayDay = 0;
			int TodayAt = -1000;
			for (const auto& Fe : F.Festivals)
			{
				// the one that began most recently: a festival starting today beats a long one in its tenth day
				const bool bBlurb = Has(FestBlurbs, Fe.first);
				const int At = Fe.second * 2 + (bBlurb ? 1 : 0);
				if (Fe.second <= 0 && At > TodayAt)
				{
					Today = Fe.first;
					TodayDay = -Fe.second;
					TodayAt = At;
				}
			}
			if (!Today.empty())
			{
				const std::string Name = "@festival." + Today + ".name";
				if (Has(FestBlurbs, Today))
				{
					AddVariants(Cand, "fest." + Today, {}, F.Since + TodayDay);
				}
				AddVariants(Cand, TodayDay == 0 ? "front.fest_today" : "front.fest_more", { Name }, F.Since);
			}
			// 3. the hero's other deeds
			for (const FDeed& D : F.Deeds)
			{
				if (std::find(Used.begin(), Used.end(), &D) != Used.end()) continue;
				if (D.Kind == "tamed") DeedLines(&D, "front.tamed", { "@bird." + Snake(D.What) + ".name" });
				else if (D.Kind == "rare") DeedLines(&D, "front.rare", { "@bird." + Snake(D.What) + ".name" });
				else if (D.Kind == "quest") DeedLines(&D, "front.quest", { "@quest." + D.What + ".title" });
				else if (D.Kind == "catch" && D.N >= 1000) DeedLines(&D, "front.catch", { "@fish." + Lower(D.What) + ".name", std::to_string(D.N) });
			}
			// 4. a festival soon, the weather, then the stories of the season
			for (const auto& Fe : F.Festivals)
			{
				if (Fe.second >= 1 && Fe.second <= 3)
				{
					AddVariants(Cand, Fe.second == 1 ? "front.fest_tomorrow" : "front.fest_soon", { "@festival." + Fe.first + ".name" }, F.Since);
					break;
				}
			}
			const std::string Ex = Extreme(F.Tomorrow);
			if (!Ex.empty())
			{
				AddVariants(Cand, "front.weather_" + Ex, { std::to_string(F.Tomorrow.TMin), std::to_string(F.Tomorrow.TMax), std::to_string(F.Tomorrow.WindMs) }, F.Since);
			}
			const int M = F.Date.M;
			std::vector<const FPoolEntry*> Season;
			for (const FPoolEntry& P : Fronts)
			{
				if (P.Fits(M)) Season.push_back(&P);
			}
			if (Season.size() < 2)
			{
				Season.clear();
				for (const FPoolEntry& P : Fronts) Season.push_back(&P);
			}
			const size_t Start = Hash((uint32_t)F.Since * 2654435761u + 17u) % Season.size();
			for (size_t I = 0; I < Season.size(); ++I)
			{
				Cand.push_back({ Season[(Start + I) % Season.size()]->Key, {} });
			}
			// the first candidate that is not yesterday's front
			size_t Pick = 0;
			while (Pick + 1 < Cand.size() && Signature(Cand[Pick]) == F.PrevFront)
			{
				++Pick;
			}
			Page.push_back(Cand[Pick]);
			Out.Front = Signature(Cand[Pick]);
			if (Cand[Pick].Key.rfind("paper.front.cup.", 0) == 0 && !Cand[Pick].Args.empty())
			{
				CupOnFront = Cand[Pick].Args[0];
			}
			// the deeds that did not make the front go to the town page as briefs
			const std::string PickedKey = Cand[Pick].Key;
			for (const FDeed& D : F.Deeds)
			{
				const bool bOnFront = PickedKey.find(".tamed_swan.") != std::string::npos ? (D.Kind == "tamed" && D.What == "MuteSwan")
					: PickedKey.find(".record.") != std::string::npos ? D.Kind == "record"
					: PickedKey.rfind("paper.front.contest", 0) == 0 ? D.Kind == "contest"
					: (PickedKey.find(".tamed.") != std::string::npos || PickedKey.find(".rare.") != std::string::npos ||
					   PickedKey.find(".quest.") != std::string::npos || PickedKey.find(".catch.") != std::string::npos)
						&& Cand[Pick].Args.size() >= 1 && DeedArg(D) == Cand[Pick].Args[0] && PickedKey.find("." + D.Kind + ".") != std::string::npos;
				if (!bOnFront) Briefs.push_back(D);
			}
			// the front's second paragraph: what is inside
			Page.push_back({ "paper.inside", {} });
			return Page;
		}

		static std::string DeedArg(const FDeed& D)
		{
			if (D.Kind == "tamed" || D.Kind == "rare" || D.Kind == "seen") return "@bird." + Snake(D.What) + ".name";
			if (D.Kind == "quest") return "@quest." + D.What + ".title";
			if (D.Kind == "catch" || D.Kind == "record") return "@fish." + Lower(D.What) + ".name";
			return D.What;
		}

		FPage TownPage(const FFacts& F, const std::vector<FDeed>& Briefs, const std::string& CupOnFront) const
		{
			FPage Page;
			Page.push_back({ "paper.head.town", {} });
			// yesterday's tournaments: the champion (the hero's own win is on the front), how far the hero got
			for (const FCup& C : F.CupsYesterday)
			{
				if (!C.bOver || "@" + C.Title == CupOnFront)
				{
					continue;
				}
				if (C.HeroReached == 3)
				{
					Page.push_back({ "paper.cup.hero_won", { "@" + C.Title } });
				}
				else
				{
					Page.push_back({ "paper.cup.won", { "@" + C.Title, "@" + C.Winner, C.RunnerUp.empty() ? std::string("@paper.cup.newcomer") : "@" + C.RunnerUp } });
					if (C.HeroReached >= 0)
					{
						Page.push_back({ "paper.cup.hero_out." + std::to_string(std::min(2, C.HeroReached)), {} });
					}
				}
			}
			int NBrief = 0;
			for (const FDeed& D : Briefs)
			{
				if (NBrief >= 3) break;
				if (D.Kind == "tamed" || D.Kind == "rare" || D.Kind == "seen" || D.Kind == "quest")
				{
					Page.push_back({ "paper.brief." + D.Kind, { DeedArg(D) } });
				}
				else if (D.Kind == "catch" || D.Kind == "record")
				{
					Page.push_back({ "paper.brief.catch", { DeedArg(D), std::to_string(D.N) } });
				}
				else if (D.Kind == "win")
				{
					Page.push_back({ "paper.brief.win", {} });
				}
				else if (D.Kind == "contest" && D.N >= 1 && D.N <= 3)
				{
					// the prize places (the first one only when a bigger story took the front); places 4..6 are no news
					Page.push_back({ "paper.brief.contest_" + std::to_string(D.N),
						{ std::string("@paper.contest.theme.") + ContestTheme(LyudFest::AddDays(F.Date, -1).M) } });
				}
				else
				{
					continue;
				}
				++NBrief;
			}
			// birthdays: the round ones first, then the children's, at most two; and one anniversary
			std::vector<std::pair<int, const FBirthday*>> Bd;
			for (const FBirthday& B : Birthdays)
			{
				if (B.Month == F.Date.M && B.Day == F.Date.D)
				{
					const int Age = F.Date.Y - B.Born;
					if (Age <= 0) continue;
					const int Score = (Age % 10 == 0 && Age >= 20) ? 3 : (Age < 12 ? 2 : (Age % 5 == 0 ? 1 : 0));
					Bd.push_back({ Score * 1000 + (int)(Hash((uint32_t)Age * 31u + (uint32_t)B.Who.size() + (uint32_t)F.Since) % 1000), &B });
				}
			}
			std::sort(Bd.begin(), Bd.end(), [](const auto& L, const auto& R) { return L.first > R.first; });
			for (size_t I = 0; I < Bd.size() && I < 2; ++I)
			{
				const FBirthday& B = *Bd[I].second;
				const int Age = F.Date.Y - B.Born;
				const char* Kind = Age < 12 ? "paper.bday.child" : (Age % 10 == 0 && Age >= 20 ? "paper.bday.jubilee" : "paper.bday.plain");
				Page.push_back({ Kind, { "@npc." + B.Who + ".name", std::to_string(Age) } });
			}
			for (const FAnniversary& A : Anniversaries)
			{
				const int Years = F.Date.Y - A.Year;
				if (A.Month == F.Date.M && A.Day == F.Date.D && Years > 0)
				{
					Page.push_back({ Years == 25 ? "paper.wed.silver" : (Years == 50 ? "paper.wed.golden" : "paper.wed.plain"),
						{ "@npc." + A.A + ".name", "@npc." + A.B + ".name", std::to_string(Years) } });
					break;
				}
			}
			// the fair's prices: three goods of the season, turning by day
			std::vector<const FGood*> Goods;
			for (const FGood& G : Market)
			{
				if (G.Months.empty() || std::find(G.Months.begin(), G.Months.end(), F.Date.M) != G.Months.end()) Goods.push_back(&G);
			}
			if (Goods.size() >= 3)
			{
				FLine L{ "paper.market", {} };
				const size_t S0 = (size_t)(F.Since * 3) % Goods.size();
				for (size_t I = 0; I < 3; ++I)
				{
					const FGood& G = *Goods[(S0 + I) % Goods.size()];
					L.Args.push_back("@item." + G.Item + ".name");
					L.Args.push_back(std::to_string(G.Price));
				}
				Page.push_back(L);
			}
			// a town story of the season, in turn (never yesterday's: on the day the season's list changes, two steps on)
			Page.push_back({ TownStory(F.Date, F.Since), {} });
			return Page;
		}

		/** The season's town stories (all of them if none fits the month). */
		std::vector<const FPoolEntry*> TownSeason(int Month) const
		{
			std::vector<const FPoolEntry*> Season;
			for (const FPoolEntry& P : Town)
			{
				if (P.Fits(Month)) Season.push_back(&P);
			}
			if (Season.empty())
			{
				for (const FPoolEntry& P : Town) Season.push_back(&P);
			}
			return Season;
		}

		std::string TownStory(const LyudFest::FDate& Date, int Since) const
		{
			const std::vector<const FPoolEntry*> Season = TownSeason(Date.M);
			const size_t N = Season.size();
			const size_t Raw = (size_t)(((Since % (int)N) + (int)N) % (int)N);
			const std::vector<const FPoolEntry*> Before = TownSeason(LyudFest::AddDays(Date, -1).M);
			const std::string Yesterday = Before[(size_t)((((Since - 1) % (int)Before.size()) + (int)Before.size()) % (int)Before.size())]->Key;
			return Season[Raw]->Key == Yesterday && N > 2 ? Season[(Raw + 2) % N]->Key : Season[Raw]->Key;
		}

		FPage WeatherPage(const FFacts& F) const
		{
			FPage Page;
			Page.push_back({ "paper.head.weather", {} });
			const FWeather& W = F.Tomorrow;
			if (W.bKnown)
			{
				Page.push_back({ "paper.weather." + W.Sky, { std::to_string(W.TMin), std::to_string(W.TMax) } });
				const int Dir = ((W.WindFromDeg % 360 + 360 + 22) % 360) / 45;
				Page.push_back({ W.WindMs <= 1 ? "paper.wind.calm" : "paper.wind", { "@paper.dir." + std::to_string(Dir), std::to_string(W.WindMs) } });
				if (W.SeaC > -50)
				{
					Page.push_back({ W.SeaC >= 18 ? "paper.sea.swim" : "paper.sea", { std::to_string(W.SeaC) } });
				}
				if (W.bAurora)
				{
					Page.push_back({ "paper.aurora", {} });
				}
			}
			else
			{
				Page.push_back({ "paper.weather.unknown", {} });
			}
			std::string Highs, Lows;
			for (const FTide& T : F.Tides)
			{
				std::string& S = T.bHigh ? Highs : Lows;
				if (!S.empty()) S += ", ";
				S += Hm(T.Minute);
			}
			if (!Highs.empty() && !Lows.empty())
			{
				Page.push_back({ "paper.tides", { Highs, Lows } });
			}
			if (!F.BiteFish.empty())
			{
				Page.push_back({ "paper.bite", { "@fish." + Lower(F.BiteFish) + ".name" } });
			}
			else
			{
				Page.push_back({ "paper.bite.none", {} });
			}
			return Page;
		}

		FPage BillsPage(const FFacts& F) const
		{
			FPage Page;
			Page.push_back({ "paper.head.bills", {} });
			bool bFest = false;
			int NSoon = 0;
			for (const auto& Fe : F.Festivals)
			{
				if (Fe.second <= 0)
				{
					bFest = true;
					continue;
				}
				if (NSoon >= 3) break;
				const LyudFest::FDate D = LyudFest::AddDays(F.Date, Fe.second);
				if (Fe.second == 1)
				{
					Page.push_back({ "paper.soon.tomorrow", { "@festival." + Fe.first + ".name" } });
				}
				else
				{
					Page.push_back({ "paper.soon", { "@festival." + Fe.first + ".name", std::to_string(D.D), "@paper.month.gen." + std::to_string(D.M) } });
				}
				++NSoon;
			}
			if (NSoon == 0)
			{
				Page.push_back({ "paper.soon.none", {} });
			}
			for (const FCup& C : F.CupsToday)
			{
				Page.push_back({ "paper.cup.today", { "@" + C.Title, Hm(C.StartMinute), "@" + C.Venue } });
			}
			// the first of the month: the photo club's contest opens with the month's theme
			if (F.Date.D == 1)
			{
				Page.push_back({ "paper.contest.open", { std::string("@paper.contest.theme.") + ContestTheme(F.Date.M),
					std::to_string(ContestLastDay), std::to_string(ContestJuryDay), "@paper.month.gen." + std::to_string(F.Date.M) } });
			}
			const int Wd = LyudFest::Weekday(F.Date);
			Page.push_back({ "paper.cinema", { "@" + (bFest && CinemaKeys.size() > 7 ? CinemaKeys[7] : CinemaKeys[(size_t)Wd]) } });
			if (NewsreelKeys.size() >= 7)
			{
				Page.push_back({ "paper.newsreel", { "@" + (bFest && NewsreelKeys.size() > 7 ? NewsreelKeys[7] : NewsreelKeys[(size_t)Wd]) } });
			}
			// three small ads of the season, turning by day
			std::vector<const FPoolEntry*> Season;
			for (const FPoolEntry& P : Ads)
			{
				if (P.Fits(F.Date.M)) Season.push_back(&P);
			}
			if (Season.size() < 3)
			{
				Season.clear();
				for (const FPoolEntry& P : Ads) Season.push_back(&P);
			}
			Page.push_back({ "paper.head.ads", {} });
			const size_t S0 = (size_t)(F.Since * 3) % Season.size();
			for (size_t I = 0; I < 3; ++I)
			{
				Page.push_back({ Season[(S0 + I) % Season.size()]->Key, {} });
			}
			return Page;
		}

	public:
		static std::string Lower(std::string S)
		{
			for (char& C : S) C = (char)((C >= 'A' && C <= 'Z') ? C - 'A' + 'a' : C);
			return S;
		}
		/** HouseSparrow -> house_sparrow (bird.<snake>.name, as ULyudBirdBook::Snake). */
		static std::string Snake(const std::string& Camel)
		{
			std::string S;
			for (size_t I = 0; I < Camel.size(); ++I)
			{
				const char C = Camel[I];
				if (C >= 'A' && C <= 'Z')
				{
					if (I > 0) S += '_';
					S += (char)(C - 'A' + 'a');
				}
				else
				{
					S += C;
				}
			}
			return S;
		}
	};
}
