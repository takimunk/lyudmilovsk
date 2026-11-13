// The things to do inside the town's buildings (docs/city_interiors.md), in plain C++ (no Unreal types) so the same
// rules run in the game (ULyudCityThings) and in the cloud (tools/citysim/run.sh).
//
// Data: Content/City/interiors.json (tools/city/interiors.py): things[] (a chair, a shelf, a counter... of a plot, in
// Unreal cm), acts{} (what E does there), shops{} (stock tables). A thing without an act gets its kind's default act.
// What this file decides: which thing the hero looks at (a grid by position, never a pass over all things), whether its
// act may run now (open hours and days, money, an item, staff on shift, the daily limit), and what running it yields
// (minutes, price, items, a line of text, a quest event, a pose, a sound, a shop). What it remembers (the page each
// thing is on, the counts per day, the uses, the plots visited today) goes into the save as a flat string map.
#pragma once

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LyudCity
{
	struct FVec
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
	};

	/** The game's date as the rules need it. */
	struct FWhen
	{
		/** Whole days since some fixed day (the daily limits reset when it changes). */
		long long Day = 0;
		/** 0..24. */
		float Hour = 12.f;
		/** Monday = 0 .. Sunday = 6. */
		int Weekday = 0;
		/** The calendar date as month * 100 + day (1231 = 31 December); 0: unknown (date conditions then hold). */
		int MonthDay = 0;
		/** Whole days since the game began (UGameCalendar::StartDate): 0 on the first day. */
		long long Since = 0;
	};

	/** Is Hour inside [Open, Close)? Close below Open runs past midnight; Close - Open >= 24 is always. */
	inline bool InHours(float Hour, float Open, float Close)
	{
		if (Close - Open >= 24.f || (Open <= 0.f && Close >= 24.f))
		{
			return true;
		}
		const float O = std::fmod(Open + 48.f, 24.f);
		const float C = std::fmod(Close + 48.f, 24.f);
		if (O == C)
		{
			return true;
		}
		return O < C ? (Hour >= O && Hour < C) : (Hour >= O || Hour < C);
	}

	/** "MM-DD" (or "M-D") as month * 100 + day; -1 when it is not a date. */
	inline int ParseMonthDay(const std::string& Text)
	{
		const size_t Dash = Text.find('-');
		if (Dash == std::string::npos || Dash == 0 || Dash + 1 >= Text.size())
		{
			return -1;
		}
		const int M = std::atoi(Text.substr(0, Dash).c_str());
		const int D = std::atoi(Text.substr(Dash + 1).c_str());
		return M >= 1 && M <= 12 && D >= 1 && D <= 31 ? M * 100 + D : -1;
	}

	/**
	 * One line of an act and when it may be shown. interiors.json writes a plain key when the line has no conditions, else
	 * {"k": key, "h": [from, to), "d": "mon-fri", "date": ["12-20", "01-10"] (or one "MM-DD"), "after": days since the game
	 * began, "every": [n, r] (days since the game began % n == r)}. Every condition given must hold.
	 */
	struct FLine
	{
		std::string Key;
		float HourFrom = -1.f, HourTo = -1.f;
		uint8_t Days = 0x7F;
		int DateFrom = -1, DateTo = -1;
		int After = 0;
		int Every = 0, EveryAt = 0;

		bool Holds(const FWhen& W) const
		{
			if (HourFrom >= 0.f && !InHours(W.Hour, HourFrom, HourTo))
			{
				return false;
			}
			if (Days != 0x7F && !(Days & (1u << (W.Weekday % 7))))
			{
				return false;
			}
			if (DateFrom > 0 && W.MonthDay > 0)
			{
				const bool bIn = DateFrom <= DateTo ? (W.MonthDay >= DateFrom && W.MonthDay <= DateTo) : (W.MonthDay >= DateFrom || W.MonthDay <= DateTo);
				if (!bIn)
				{
					return false;
				}
			}
			if (After > 0 && W.Since < After)
			{
				return false;
			}
			if (Every > 1 && (((W.Since % Every) + Every) % Every) != EveryAt)
			{
				return false;
			}
			return true;
		}
	};

	/** What an act is (the defaults of a kind are made the same way). */
	struct FAct
	{
		std::string Id;
		std::string Prompt;
		std::string Title;
		std::vector<FLine> Lines;
		/** "rotate": "daily": one line a game day, the next one on the next day it is read (notice boards, wall newspapers); else
		 *  the next line on every use (a shelf's pages). */
		bool bDaily = false;
		int Minutes = 0;
		/** The hour the act ends at (sleep till 7): the minutes from now to the next such hour; -1: use Minutes. */
		int Until = -1;
		int Price = 0;
		std::vector<std::pair<std::string, int>> Give;
		std::string NeedsItem;
		std::string NeedsStaff;
		std::string Pose = "stand";
		std::string Sound;
		std::string Event;
		std::string Target;
		int Daily = 0;
		std::string Shop;
		std::string Screen;
		bool bFade = false;
		/** Usable whatever the plot's hours (seats, beds, doors: if the hero is in, he may sit down). */
		bool bAnyHour = false;
	};

	struct FThing
	{
		std::string Id;
		std::string Plot;
		std::string Kind;
		FVec At;
		float Yaw = 0.f;
		std::map<std::string, std::string> Props;
		/** Index into FModel::Acts (the named act, or the kind's default). */
		int Act = -1;
		/** Open hours [Open, Close) (Close < Open: past midnight) and the weekdays (bit 0 = Monday). */
		bool bHours = false;
		float Open = 0.f, Close = 24.f;
		uint8_t Days = 0x7F;

		std::string Prop(const std::string& Key, const std::string& Default = std::string()) const
		{
			const auto It = Props.find(Key);
			return It == Props.end() ? Default : It->second;
		}
		float PropF(const std::string& Key, float Default) const
		{
			const auto It = Props.find(Key);
			if (It == Props.end() || It->second.empty())
			{
				return Default;
			}
			char* End = nullptr;
			const double V = std::strtod(It->second.c_str(), &End);
			return End == It->second.c_str() ? Default : (float)V;
		}
	};

	struct FGood
	{
		std::string Item;
		int Price = 0;
		int Count = 1;
	};

	/** What the rules ask the game about the hero and the town. */
	struct FAsk
	{
		int Money = 0;
		/** Developer mode (interiors.open_all_hours): open hours, days and staff are not asked. */
		bool bOpenAll = false;
		std::function<int(const std::string& /*Item*/)> ItemCount;
		std::function<bool(const std::string& /*Plot*/, const std::string& /*Role*/)> StaffOnShift;
	};

	enum class EVerdict : uint8_t
	{
		Ok,
		Closed,
		NoMoney,
		NeedsItem,
		NoStaff,
		DailyDone,
		Unknown,
	};

	inline const char* VerdictName(EVerdict V)
	{
		switch (V)
		{
		case EVerdict::Ok: return "ok";
		case EVerdict::Closed: return "closed";
		case EVerdict::NoMoney: return "no_money";
		case EVerdict::NeedsItem: return "needs_item";
		case EVerdict::NoStaff: return "no_staff";
		case EVerdict::DailyDone: return "daily_done";
		default: return "unknown";
		}
	}

	/** The string key that tells the hero why an act was refused (city.act.<verdict>). */
	inline std::string RefusalKey(EVerdict V)
	{
		return V == EVerdict::Ok ? std::string() : std::string("city.act.") + VerdictName(V);
	}

	/** What running an act did; the game carries it out. */
	struct FOutcome
	{
		EVerdict Verdict = EVerdict::Unknown;
		int Thing = -1;
		int Act = -1;
		/** The line key shown now (empty: none), and the page title key (may be empty). */
		std::string Line;
		std::string Title;
		/** How many lines the act has (the page shows "next" while there are more to read this time). */
		int NumLines = 0;
		int Minutes = 0;
		int Price = 0;
		bool bFade = false;
		std::vector<std::pair<std::string, int>> Give;
		std::string Event;
		std::string Target;
		std::string Pose;
		std::string Sound;
		std::string Shop;
		std::string Screen;
		/** The first use of anything in this plot today: report "visit" with the plot id. */
		bool bFirstVisitToday = false;
		std::string Plot;
		/** For the refusal line: the price or the item needed. */
		int NeedMoney = 0;
		std::string NeedItem;
	};

	/** Parses a weekday list: "", "daily", "mon-sun", "tue-sun", "fri-mon", "mon,wed,fri", "sat-sun". Bit 0 = Monday. */
	inline uint8_t ParseDays(const std::string& Text)
	{
		static const char* const Names[7] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
		if (Text.empty() || Text == "daily" || Text == "all" || Text == "every")
		{
			return 0x7F;
		}
		auto DayOf = [](const std::string& W) -> int
		{
			for (int I = 0; I < 7; ++I)
			{
				if (W.compare(0, 3, Names[I]) == 0)
				{
					return I;
				}
			}
			return -1;
		};
		uint8_t Mask = 0;
		size_t Start = 0;
		while (Start <= Text.size())
		{
			size_t Comma = Text.find(',', Start);
			if (Comma == std::string::npos)
			{
				Comma = Text.size();
			}
			const std::string Part = Text.substr(Start, Comma - Start);
			const size_t Dash = Part.find('-');
			if (Dash == std::string::npos)
			{
				const int D = DayOf(Part);
				if (D >= 0)
				{
					Mask = (uint8_t)(Mask | (1u << D));
				}
			}
			else
			{
				const int A = DayOf(Part.substr(0, Dash));
				const int B = DayOf(Part.substr(Dash + 1));
				if (A >= 0 && B >= 0)
				{
					for (int D = A, Steps = 0; Steps < 7; D = (D + 1) % 7, ++Steps)
					{
						Mask = (uint8_t)(Mask | (1u << D));
						if (D == B)
						{
							break;
						}
					}
				}
			}
			Start = Comma + 1;
		}
		return Mask ? Mask : (uint8_t)0x7F;
	}

	/** A seat's or bed's surface over its own floor (m): the "height" prop when it is plausible, else the usual one. Some
	 *  exports gave the height over the building's base (6..33 m): sitting there would put the seat's frame metres under the
	 *  floor, and standing up drops the hero through it. */
	constexpr float MinSeatHeight = 0.05f;
	constexpr float MaxSeatHeight = 1.2f;

	/** The bytes of a UTF-8 character starting with Lead. */
	inline int Utf8Len(unsigned char Lead)
	{
		return Lead < 0x80 ? 1 : (Lead >> 5) == 0x6 ? 2 : (Lead >> 4) == 0xE ? 3 : (Lead >> 3) == 0x1E ? 4 : 1;
	}

	/**
	 * A line cut into subtitles for a screen (a film, a play, a TV programme): at the ends of sentences (". ", "! ", "? ",
	 * "… ", a closing quote after them, and the Japanese "。！？" with a closing bracket after them), a piece shorter than
	 * 10 bytes joined to its neighbour, then the shortest neighbours joined until there are at most MaxParts. UTF-8 in and out.
	 */
	inline std::vector<std::string> Subtitles(const std::string& Text, int MaxParts)
	{
		std::vector<std::string> Parts;
		// Japanese runs on without a space
		auto Joiner = [](const std::string& Prev) -> std::string
		{
			const size_t N = Prev.size();
			const bool bCjk = N >= 3 && ((unsigned char)Prev[N - 3] == 0xE3 || (unsigned char)Prev[N - 3] == 0xEF);
			return bCjk ? std::string() : std::string(" ");
		};
		auto IsCloser = [&Text](size_t I, size_t& Len) -> bool
		{
			const unsigned char C = (unsigned char)Text[I];
			if (C == '"' || C == ')' || C == '\'')
			{
				Len = 1;
				return true;
			}
			// » (C2 BB), ” (E2 80 9D), 」 (E3 80 8D), 』 (E3 80 8F)
			if (C == 0xC2 && I + 1 < Text.size() && (unsigned char)Text[I + 1] == 0xBB)
			{
				Len = 2;
				return true;
			}
			if (I + 2 < Text.size() && ((C == 0xE2 && (unsigned char)Text[I + 1] == 0x80 && (unsigned char)Text[I + 2] == 0x9D)
				|| (C == 0xE3 && (unsigned char)Text[I + 1] == 0x80 && ((unsigned char)Text[I + 2] == 0x8D || (unsigned char)Text[I + 2] == 0x8F))))
			{
				Len = 3;
				return true;
			}
			return false;
		};
		std::string Cur;
		size_t I = 0;
		while (I < Text.size())
		{
			const unsigned char C = (unsigned char)Text[I];
			const int Len = Utf8Len(C);
			Cur.append(Text, I, (size_t)Len);
			I += (size_t)Len;
			const bool bLatinEnd = C == '.' || C == '!' || C == '?';
			const bool bEllipsis = Len == 3 && C == 0xE2 && (unsigned char)Text[I - 2] == 0x80 && (unsigned char)Text[I - 1] == 0xA6;
			const bool bCjkEnd = Len == 3 && ((C == 0xE3 && (unsigned char)Text[I - 2] == 0x80 && (unsigned char)Text[I - 1] == 0x82)
				|| (C == 0xEF && (unsigned char)Text[I - 2] == 0xBC && ((unsigned char)Text[I - 1] == 0x81 || (unsigned char)Text[I - 1] == 0x9F)));
			if (!bLatinEnd && !bEllipsis && !bCjkEnd)
			{
				continue;
			}
			// more of the same mark ("?!", "...") and closing quotes stay with the sentence
			size_t CloserLen = 0;
			while (I < Text.size() && (Text[I] == '.' || Text[I] == '!' || Text[I] == '?' || IsCloser(I, CloserLen)))
			{
				const size_t Step = (Text[I] == '.' || Text[I] == '!' || Text[I] == '?') ? 1 : CloserLen;
				Cur.append(Text, I, Step);
				I += Step;
			}
			if (bCjkEnd || I >= Text.size() || Text[I] == ' ')
			{
				while (I < Text.size() && Text[I] == ' ')
				{
					++I;
				}
				Parts.push_back(Cur);
				Cur.clear();
			}
		}
		if (!Cur.empty())
		{
			Parts.push_back(Cur);
		}
		// a piece too short to stand alone ("No.", "Me too") goes with its neighbour
		for (size_t P = 0; P < Parts.size() && Parts.size() > 1;)
		{
			if (Parts[P].size() >= 10)
			{
				++P;
				continue;
			}
			if (P > 0)
			{
				Parts[P - 1] += Joiner(Parts[P - 1]) + Parts[P];
			}
			else
			{
				Parts[1] = Parts[0] + Joiner(Parts[0]) + Parts[1];
			}
			Parts.erase(Parts.begin() + (long)P);
		}
		const size_t Cap = (size_t)(MaxParts > 0 ? MaxParts : 1);
		while (Parts.size() > Cap)
		{
			size_t Best = 0;
			for (size_t P = 1; P + 1 < Parts.size(); ++P)
			{
				if (Parts[P].size() + Parts[P + 1].size() < Parts[Best].size() + Parts[Best + 1].size())
				{
					Best = P;
				}
			}
			Parts[Best] += Joiner(Parts[Best]) + Parts[Best + 1];
			Parts.erase(Parts.begin() + (long)Best + 1);
		}
		return Parts;
	}

	class FModel
	{
	public:
		std::vector<FThing> Things;
		std::vector<FAct> Acts;
		std::map<std::string, std::vector<FGood>> Shops;

		/** Grid cell size (cm): reach is a few metres, so a pick reads 3x3 cells. */
		static constexpr double CellCm = 400.0;

		/** Reads interiors.json; false (and Error) when it is not JSON. Replaces everything; the saved state is kept. */
		bool Load(const std::string& Text, std::string& Error)
		{
			Things.clear();
			Acts.clear();
			Shops.clear();
			ActIndex.clear();
			ThingIndex.clear();
			Grid.clear();
			LyudJson::FValue Root;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				return false;
			}
			if (Root.Type != LyudJson::FValue::Object)
			{
				Error = "interiors: the root is not an object";
				return false;
			}
			for (const auto& Field : Root["acts"].Fields)
			{
				FAct A = ReadAct(Field.first, Field.second);
				ActIndex[A.Id] = (int)Acts.size();
				Acts.push_back(std::move(A));
			}
			for (const auto& Field : Root["shops"].Fields)
			{
				std::vector<FGood>& Goods = Shops[Field.first];
				const LyudJson::FValue& List = Field.second.Type == LyudJson::FValue::Object ? Field.second["goods"] : Field.second;
				for (const LyudJson::FValue& G : List.Items)
				{
					FGood Good;
					Good.Item = G["item"].S();
					Good.Price = G["price"].I(0);
					Good.Count = G["count"].I(1);
					if (!Good.Item.empty())
					{
						Goods.push_back(Good);
					}
				}
			}
			for (const LyudJson::FValue& T : Root["things"].Items)
			{
				FThing Th;
				Th.Id = T["id"].S();
				if (Th.Id.empty() || ThingIndex.count(Th.Id))
				{
					continue;
				}
				Th.Plot = T["plot"].S();
				if (Th.Plot.empty())
				{
					const size_t Slash = Th.Id.find('/');
					Th.Plot = Slash == std::string::npos ? std::string() : Th.Id.substr(0, Slash);
				}
				Th.Kind = T["kind"].S("board");
				const LyudJson::FValue& Ue = T["ue"];
				Th.At.X = Ue[0].Num;
				Th.At.Y = Ue[1].Num;
				Th.At.Z = Ue[2].Num;
				Th.Yaw = T["yaw"].F(0.f);
				for (const auto& P : T["props"].Fields)
				{
					Th.Props[P.first] = ScalarText(P.second);
				}
				const LyudJson::FValue& Hours = T["hours"];
				if (Hours.Type == LyudJson::FValue::Array && Hours.Size() >= 2)
				{
					Th.bHours = true;
					Th.Open = Hours[0].F(0.f);
					Th.Close = Hours[1].F(24.f);
				}
				Th.Days = ParseDays(T["days"].S());
				const std::string ActId = T["act"].S();
				const auto Found = ActIndex.find(ActId);
				Th.Act = ActId.empty() || Found == ActIndex.end() ? DefaultAct(Th) : Found->second;
				ThingIndex[Th.Id] = (int)Things.size();
				Things.push_back(std::move(Th));
			}
			BuildGrid();
			return true;
		}

		int FindThing(const std::string& Id) const
		{
			const auto It = ThingIndex.find(Id);
			return It == ThingIndex.end() ? -1 : It->second;
		}
		int FindAct(const std::string& Id) const
		{
			const auto It = ActIndex.find(Id);
			return It == ActIndex.end() ? -1 : It->second;
		}
		const std::vector<FGood>* FindShop(const std::string& Id) const
		{
			const auto It = Shops.find(Id);
			return It == Shops.end() ? nullptr : &It->second;
		}
		const FAct* ActOf(int Thing) const
		{
			return Thing >= 0 && Thing < (int)Things.size() && Things[Thing].Act >= 0 ? &Acts[Things[Thing].Act] : nullptr;
		}

		/** The box round all things (cm); empty model: false. */
		bool Bounds(FVec& Min, FVec& Max) const
		{
			if (Things.empty())
			{
				return false;
			}
			Min = BoxMin;
			Max = BoxMax;
			return true;
		}
		/** Is P within Margin (cm) of the box round all things? (Outside: the game does nothing at all.) */
		bool InBounds(const FVec& P, double Margin) const
		{
			return !Things.empty() && P.X >= BoxMin.X - Margin && P.X <= BoxMax.X + Margin && P.Y >= BoxMin.Y - Margin
				&& P.Y <= BoxMax.Y + Margin && P.Z >= BoxMin.Z - Margin && P.Z <= BoxMax.Z + Margin;
		}

		/**
		 * The things in view from Eye along Dir (unit) within Reach (cm), best first, into Out (at most MaxOut); returns how
		 * many. A thing must be in front, close to the line of sight (more slack near and for big things) and on about the
		 * eye's floor. Reads only the grid cells round the eye; allocates nothing.
		 */
		int Pick(const FVec& Eye, const FVec& Dir, double Reach, int* Out, int MaxOut) const
		{
			if (Things.empty() || MaxOut <= 0)
			{
				return 0;
			}
			const int Cells = (int)std::ceil(Reach / CellCm);
			const long long CX = CellOf(Eye.X), CY = CellOf(Eye.Y);
			int Num = 0;
			double Scores[8];
			const int Cap = MaxOut < 8 ? MaxOut : 8;
			for (long long GX = CX - Cells; GX <= CX + Cells; ++GX)
			{
				for (long long GY = CY - Cells; GY <= CY + Cells; ++GY)
				{
					const auto Cell = Grid.find(Key(GX, GY));
					if (Cell == Grid.end())
					{
						continue;
					}
					for (const int I : Cell->second)
					{
						double Score = 0.0;
						if (!Score1(Things[I], Eye, Dir, Reach, Score))
						{
							continue;
						}
						// insert into the short sorted list
						int Slot = Num < Cap ? Num : Cap;
						while (Slot > 0 && Scores[Slot - 1] > Score)
						{
							if (Slot < Cap)
							{
								Scores[Slot] = Scores[Slot - 1];
								Out[Slot] = Out[Slot - 1];
							}
							--Slot;
						}
						if (Slot < Cap)
						{
							Scores[Slot] = Score;
							Out[Slot] = I;
							if (Num < Cap)
							{
								++Num;
							}
						}
					}
				}
			}
			return Num;
		}

		/** The N nearest things to P (console lists; allocates). Pairs of (thing, distance cm). */
		std::vector<std::pair<int, double>> Nearest(const FVec& P, int N) const
		{
			std::vector<std::pair<int, double>> All;
			All.reserve(Things.size());
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				All.emplace_back(I, Dist(Things[I].At, P));
			}
			const size_t Keep = std::min(All.size(), (size_t)(N > 0 ? N : 0));
			std::partial_sort(All.begin(), All.begin() + (long)Keep, All.end(),
				[](const std::pair<int, double>& A, const std::pair<int, double>& B) { return A.second < B.second; });
			All.resize(Keep);
			return All;
		}

		/** Game minutes from Hour (0..24) to the next o'clock Until (0..23); a whole day when it is that hour already. */
		static int MinutesUntil(float Hour, int Until)
		{
			int M = (int)std::lround((Until - Hour) * 60.f) % (24 * 60);
			return M <= 0 ? M + 24 * 60 : M;
		}

		/** May Thing's act run now? No side effects (the prompt may ask this). */
		EVerdict Check(int Thing, const FWhen& When, const FAsk& Ask) const
		{
			const FAct* A = ActOf(Thing);
			if (!A)
			{
				return EVerdict::Unknown;
			}
			const FThing& T = Things[Thing];
			if (!Ask.bOpenAll && !A->bAnyHour && T.bHours && (!(T.Days & (1u << (When.Weekday % 7))) || !InHours(When.Hour, T.Open, T.Close)))
			{
				return EVerdict::Closed;
			}
			if (!Ask.bOpenAll && !A->NeedsStaff.empty() && !(Ask.StaffOnShift && Ask.StaffOnShift(T.Plot, A->NeedsStaff)))
			{
				return EVerdict::NoStaff;
			}
			if (A->Daily > 0 && TodayCount(A->Id, When.Day) >= A->Daily)
			{
				return EVerdict::DailyDone;
			}
			if (!A->NeedsItem.empty() && !(Ask.ItemCount && Ask.ItemCount(A->NeedsItem) > 0))
			{
				return EVerdict::NeedsItem;
			}
			if (A->Price > 0 && Ask.Money < A->Price)
			{
				return EVerdict::NoMoney;
			}
			return EVerdict::Ok;
		}

		/** Runs Thing's act: checks it, and when it may run, counts it, turns its page and says what to do. */
		FOutcome Run(int Thing, const FWhen& When, const FAsk& Ask)
		{
			FOutcome O;
			O.Thing = Thing;
			O.Verdict = Check(Thing, When, Ask);
			const FAct* A = ActOf(Thing);
			if (!A)
			{
				return O;
			}
			const FThing& T = Things[Thing];
			O.Act = T.Act;
			O.Plot = T.Plot;
			O.NeedMoney = A->Price;
			O.NeedItem = A->NeedsItem;
			if (O.Verdict != EVerdict::Ok)
			{
				return O;
			}
			RollDay(When.Day);
			Today[A->Id] += 1;
			Uses[A->Id] += 1;
			if (!T.Plot.empty())
			{
				long long& Last = Visited[T.Plot];
				O.bFirstVisitToday = Last != When.Day + 1;     // stored +1: 0 means never
				Last = When.Day + 1;
			}
			O.Title = A->Title;
			const int Holding = CountHolding(*A, When);
			// a daily board shows today's one; a shelf the pages that may be read now, one after another
			O.NumLines = A->bDaily ? (Holding > 0 ? 1 : 0) : Holding;
			if (Holding > 0)
			{
				O.Line = TakeLine(Thing, *A, When);
			}
			O.Minutes = A->Until >= 0 ? MinutesUntil(When.Hour, A->Until) : A->Minutes;
			O.Price = A->Price;
			O.bFade = A->bFade;
			O.Give = A->Give;
			O.Event = A->Event.empty() ? std::string("do") : A->Event;
			O.Target = A->Target.empty() ? A->Id : A->Target;
			O.Pose = A->Pose;
			O.Sound = A->Sound;
			O.Shop = A->Shop;
			O.Screen = A->Screen;
			return O;
		}

		/** The next page of Thing (the page panel's "next"): its line key, and the page turned (a daily act: today's line
		 *  again). Empty: no line may be shown now. */
		std::string NextLine(int Thing, const FWhen& When)
		{
			const FAct* A = ActOf(Thing);
			if (!A || CountHolding(*A, When) == 0)
			{
				return std::string();
			}
			return TakeLine(Thing, *A, When);
		}

		/** How many of an act's lines may be shown now. */
		static int CountHolding(const FAct& A, const FWhen& When)
		{
			int N = 0;
			for (const FLine& L : A.Lines)
			{
				N += L.Holds(When) ? 1 : 0;
			}
			return N;
		}

		/** A seat's or bed's surface over its floor, metres (see MinSeatHeight). */
		static float SeatHeight(const FThing& T, bool bLie)
		{
			const float Usual = bLie ? 0.5f : 0.45f;
			const float H = T.PropF("height", Usual);
			return H >= MinSeatHeight && H <= MaxSeatHeight ? H : Usual;
		}

		/**
		 * The thing nearest P within Radius (cm) on about P's floor (|dz| <= MaxDz), from the grid only (no allocation); -1
		 * none. The game asks it a few times a second for the plot the hero is in (a "visit" when it changes).
		 */
		int NearestThing(const FVec& P, double Radius, double MaxDz) const
		{
			if (Things.empty())
			{
				return -1;
			}
			const int Cells = (int)std::ceil(Radius / CellCm);
			const long long CX = CellOf(P.X), CY = CellOf(P.Y);
			int Best = -1;
			double BestD = Radius;
			for (long long GX = CX - Cells; GX <= CX + Cells; ++GX)
			{
				for (long long GY = CY - Cells; GY <= CY + Cells; ++GY)
				{
					const auto Cell = Grid.find(Key(GX, GY));
					if (Cell == Grid.end())
					{
						continue;
					}
					for (const int I : Cell->second)
					{
						const FVec& At = Things[I].At;
						if (std::fabs(At.Z - P.Z) > MaxDz)
						{
							continue;
						}
						const double D = std::sqrt((At.X - P.X) * (At.X - P.X) + (At.Y - P.Y) * (At.Y - P.Y));
						if (D <= BestD)
						{
							BestD = D;
							Best = I;
						}
					}
				}
			}
			return Best;
		}

		/** Is this act a screen show (a film, a play, the TV: the page in the screen style, its line as subtitles)? */
		static bool IsScreen(const FAct& A) { return !A.Screen.empty(); }
		/** Does the hero sit on the thing itself (a seat, a bed, or anything with a seat height: the cinema's hall seats)? */
		static bool IsSeatThing(const FThing& T) { return T.Kind == "seat" || T.Kind == "bed" || T.Props.count("height") > 0; }

		/**
		 * The screen a hero seated on Seat watches: the nearest thing of the same plot within Radius (cm) whose act is a screen
		 * show and that is not a seat itself, in front of the seat (within 70 degrees of its yaw) and on about its floor; -1
		 * none. Asked once when he sits down (a pass over all things).
		 */
		int ScreenFrom(int Seat, double Radius) const
		{
			if (Seat < 0 || Seat >= (int)Things.size())
			{
				return -1;
			}
			const FThing& S = Things[Seat];
			const double R = S.Yaw * 3.14159265358979 / 180.0;
			const double FX = std::cos(R), FY = std::sin(R);
			int Best = -1;
			double BestD = Radius;
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FThing& T = Things[I];
				const FAct* A = ActOf(I);
				if (I == Seat || T.Plot != S.Plot || !A || !IsScreen(*A) || IsSeatThing(T) || std::fabs(T.At.Z - S.At.Z) > 450.0)
				{
					continue;
				}
				const double DX = T.At.X - S.At.X, DY = T.At.Y - S.At.Y;
				const double D = std::sqrt(DX * DX + DY * DY);
				if (D > BestD || D < 1.0 || (DX * FX + DY * FY) / D < 0.34)
				{
					continue;
				}
				BestD = D;
				Best = I;
			}
			return Best;
		}

		/** Must the hero sit down first to watch Thing's show? A sitting screen act on a thing that is not a seat, in a plot
		 *  that has a seat within Radius (cm) to watch it from. */
		bool NeedsSeatFirst(int Thing, double Radius) const
		{
			const FAct* A = ActOf(Thing);
			if (!A || !IsScreen(*A) || A->Pose != "sit" || IsSeatThing(Things[Thing]))
			{
				return false;
			}
			const FThing& T = Things[Thing];
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FThing& S = Things[I];
				if (I != Thing && S.Plot == T.Plot && S.Kind == "seat" && std::fabs(S.At.Z - T.At.Z) < 450.0
					&& std::sqrt((S.At.X - T.At.X) * (S.At.X - T.At.X) + (S.At.Y - T.At.Y) * (S.At.Y - T.At.Y)) <= Radius)
				{
					return true;
				}
			}
			return false;
		}

		int TodayCount(const std::string& ActId, long long Day) const
		{
			if (Day != CountDay)
			{
				return 0;
			}
			const auto It = Today.find(ActId);
			return It == Today.end() ? 0 : It->second;
		}
		int TotalUses(const std::string& ActId) const
		{
			const auto It = Uses.find(ActId);
			return It == Uses.end() ? 0 : It->second;
		}
		int LineIndex(const std::string& ThingId) const
		{
			const auto It = LineOf.find(ThingId);
			return It == LineOf.end() ? 0 : It->second;
		}
		/** Developer mode: today's counts start over. */
		void ResetToday() { Today.clear(); }
		/** A new game: nothing used, nothing read. */
		void ResetState()
		{
			Today.clear();
			Uses.clear();
			LineOf.clear();
			ReadDay.clear();
			Visited.clear();
			CountDay = 0;
		}

		/** The state as a flat map for the save: "day", "t:<act>", "u:<act>", "l:<thing>", "d:<thing>", "v:<plot>". */
		void SaveState(std::map<std::string, std::string>& Out) const
		{
			Out.clear();
			Out["day"] = std::to_string(CountDay);
			for (const auto& P : Today)
			{
				Out["t:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : Uses)
			{
				Out["u:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : LineOf)
			{
				if (P.second != 0)
				{
					Out["l:" + P.first] = std::to_string(P.second);
				}
			}
			for (const auto& P : ReadDay)
			{
				Out["d:" + P.first] = std::to_string(P.second);
			}
			for (const auto& P : Visited)
			{
				Out["v:" + P.first] = std::to_string(P.second);
			}
		}
		void LoadState(const std::map<std::string, std::string>& In)
		{
			ResetState();
			for (const auto& P : In)
			{
				const long long V = std::strtoll(P.second.c_str(), nullptr, 10);
				if (P.first == "day")
				{
					CountDay = V;
				}
				else if (P.first.size() > 2 && P.first[1] == ':')
				{
					const std::string Name = P.first.substr(2);
					switch (P.first[0])
					{
					case 't': Today[Name] = (int)V; break;
					case 'u': Uses[Name] = (int)V; break;
					case 'l': LineOf[Name] = (int)V; break;
					case 'd': ReadDay[Name] = V; break;
					case 'v': Visited[Name] = V; break;
					default: break;
					}
				}
			}
		}

	private:
		std::unordered_map<std::string, int> ActIndex;
		std::unordered_map<std::string, int> ThingIndex;
		std::unordered_map<long long, std::vector<int>> Grid;
		FVec BoxMin, BoxMax;
		/** The game day Today counts (a new day clears it). */
		long long CountDay = 0;
		std::map<std::string, int> Today;
		std::map<std::string, int> Uses;
		std::map<std::string, int> LineOf;
		/** A daily act's thing: the game day (+1) its line was last shown. */
		std::map<std::string, long long> ReadDay;
		std::map<std::string, long long> Visited;

		static double Dist(const FVec& A, const FVec& B)
		{
			const double DX = A.X - B.X, DY = A.Y - B.Y, DZ = A.Z - B.Z;
			return std::sqrt(DX * DX + DY * DY + DZ * DZ);
		}
		static long long CellOf(double V) { return (long long)std::floor(V / CellCm); }
		static long long Key(long long GX, long long GY) { return (GX + (1LL << 30)) * (1LL << 31) + (GY + (1LL << 30)); }

		static std::string ScalarText(const LyudJson::FValue& V)
		{
			switch (V.Type)
			{
			case LyudJson::FValue::String: return V.Str;
			case LyudJson::FValue::Bool: return V.B ? "1" : "0";
			case LyudJson::FValue::Number:
			{
				char Buf[32];
				std::snprintf(Buf, sizeof(Buf), "%g", V.Num);
				return Buf;
			}
			default: return std::string();
			}
		}

		void RollDay(long long Day)
		{
			if (Day != CountDay)
			{
				Today.clear();
				CountDay = Day;
			}
		}

		/** The first line at or after From (going round) that may be shown now; -1 none. */
		static int FirstHolding(const FAct& A, int From, const FWhen& When)
		{
			const int N = (int)A.Lines.size();
			for (int Step = 0; Step < N; ++Step)
			{
				const int I = (((From + Step) % N) + N) % N;
				if (A.Lines[(size_t)I].Holds(When))
				{
					return I;
				}
			}
			return -1;
		}

		/** Shows a line of Thing's act and turns the page: the next one that may be shown now; a daily act keeps today's line
		 *  and goes on to the next one on the next day it is read. Call only when CountHolding > 0. */
		std::string TakeLine(int Thing, const FAct& A, const FWhen& When)
		{
			const std::string& Id = Things[Thing].Id;
			const int N = (int)A.Lines.size();
			const int Cur = LineIndex(Id);
			int Pos = -1;
			if (A.bDaily)
			{
				long long& Read = ReadDay[Id];
				Pos = FirstHolding(A, Read != 0 && Read != When.Day + 1 ? Cur + 1 : Cur, When);
				Read = When.Day + 1;
				LineOf[Id] = Pos < 0 ? Cur : Pos;
			}
			else
			{
				Pos = FirstHolding(A, Cur, When);
				LineOf[Id] = Pos < 0 ? Cur : (Pos + 1) % N;
			}
			return Pos < 0 ? std::string() : A.Lines[(size_t)Pos].Key;
		}

		/** How far off the line of sight a thing of this kind may be and still be looked at (cm, at the eye). */
		static double Slack(const FThing& T)
		{
			const std::string& K = T.Kind;
			if (K == "bed" || K == "counter" || K == "shelf" || K == "screen" || K == "sport" || K == "play")
			{
				return 45.0;
			}
			if (K == "take")
			{
				return 18.0;
			}
			return 30.0;
		}

		static bool Score1(const FThing& T, const FVec& Eye, const FVec& Dir, double Reach, double& Score)
		{
			const FVec D{ T.At.X - Eye.X, T.At.Y - Eye.Y, T.At.Z - Eye.Z };
			const double Len = std::sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
			if (Len > Reach || std::fabs(D.Z) > 220.0)
			{
				return false;
			}
			const double Along = D.X * Dir.X + D.Y * Dir.Y + D.Z * Dir.Z;
			if (Along <= 1.0 && Len > 30.0)
			{
				return false;     // behind or beside the eye
			}
			const double LX = D.X - Along * Dir.X, LY = D.Y - Along * Dir.Y, LZ = D.Z - Along * Dir.Z;
			const double Lateral = std::sqrt(LX * LX + LY * LY + LZ * LZ);
			const double Allowed = Slack(T) + 0.22 * (Along > 0.0 ? Along : 0.0);
			if (Lateral > Allowed)
			{
				return false;
			}
			Score = Lateral / Allowed + 0.35 * Len / Reach;
			return true;
		}

		void BuildGrid()
		{
			Grid.clear();
			for (int I = 0; I < (int)Things.size(); ++I)
			{
				const FVec& P = Things[I].At;
				Grid[Key(CellOf(P.X), CellOf(P.Y))].push_back(I);
				if (I == 0)
				{
					BoxMin = BoxMax = P;
				}
				BoxMin.X = std::min(BoxMin.X, P.X);
				BoxMin.Y = std::min(BoxMin.Y, P.Y);
				BoxMin.Z = std::min(BoxMin.Z, P.Z);
				BoxMax.X = std::max(BoxMax.X, P.X);
				BoxMax.Y = std::max(BoxMax.Y, P.Y);
				BoxMax.Z = std::max(BoxMax.Z, P.Z);
			}
		}

		/** A line with its conditions: {"k", "h", "d", "date", "after", "every"} (see FLine). */
		static void ReadLine(const LyudJson::FValue& L, FLine& Line)
		{
			Line.Key = L["k"].S();
			const LyudJson::FValue& H = L["h"];
			if (H.Type == LyudJson::FValue::Array && H.Size() >= 2)
			{
				Line.HourFrom = H[0].F(0.f);
				Line.HourTo = H[1].F(24.f);
			}
			if (L["d"].Type == LyudJson::FValue::String)
			{
				Line.Days = ParseDays(L["d"].S());
			}
			const LyudJson::FValue& Date = L["date"];
			if (Date.Type == LyudJson::FValue::String)
			{
				Line.DateFrom = Line.DateTo = ParseMonthDay(Date.Str);
			}
			else if (Date.Type == LyudJson::FValue::Array && Date.Size() >= 2)
			{
				Line.DateFrom = ParseMonthDay(Date[0].S());
				Line.DateTo = ParseMonthDay(Date[1].S());
				if (Line.DateFrom < 0 || Line.DateTo < 0)
				{
					Line.DateFrom = Line.DateTo = -1;
				}
			}
			Line.After = L["after"].I(0);
			const LyudJson::FValue& Every = L["every"];
			if (Every.Type == LyudJson::FValue::Array && Every.Size() >= 1)
			{
				Line.Every = Every[0].I(0);
				Line.EveryAt = Every[1].I(0);
			}
			else
			{
				Line.Every = Every.I(0);
			}
			if (Line.Every > 1)
			{
				Line.EveryAt = ((Line.EveryAt % Line.Every) + Line.Every) % Line.Every;
			}
		}

		static FAct ReadAct(const std::string& Id, const LyudJson::FValue& V)
		{
			FAct A;
			A.Id = Id;
			A.Prompt = V["prompt"].S("city.act.use.prompt");
			A.Title = V["title"].S();
			for (const LyudJson::FValue& L : V["lines"].Items)
			{
				FLine Line;
				if (L.Type == LyudJson::FValue::String)
				{
					Line.Key = L.Str;
				}
				else if (L.Type == LyudJson::FValue::Object)
				{
					ReadLine(L, Line);
				}
				if (!Line.Key.empty())
				{
					A.Lines.push_back(Line);
				}
			}
			A.bDaily = V["rotate"].S() == "daily";
			A.Minutes = V["minutes"].I(0);
			A.Until = V["until"].I(-1);
			if (A.Until > 23)
			{
				A.Until = -1;
			}
			A.Price = V["price"].I(0);
			for (const LyudJson::FValue& G : V["give"].Items)
			{
				// [["item", n]], ["item"] or [{"item": .., "count": ..}]
				std::string Item;
				int Count = 1;
				if (G.Type == LyudJson::FValue::Array)
				{
					Item = G[0].S();
					Count = G[1].I(1);
				}
				else if (G.Type == LyudJson::FValue::Object)
				{
					Item = G["item"].S();
					Count = G["count"].I(1);
				}
				else
				{
					Item = G.S();
				}
				if (!Item.empty() && Count > 0)
				{
					A.Give.emplace_back(Item, Count);
				}
			}
			A.NeedsItem = V["needs_item"].S();
			A.NeedsStaff = V["needs_staff"].S();
			A.Pose = V["pose"].S("stand");
			if (A.Pose.empty())
			{
				A.Pose = "stand";
			}
			A.Sound = V["sound"].S();
			A.Event = V["event"].S();
			A.Target = V["target"].S();
			A.Daily = V["daily"].I(0);
			A.Shop = V["shop"].S();
			A.Screen = V["screen"].S();
			A.bFade = V["fade"].Type == LyudJson::FValue::Bool ? V["fade"].B : V["fade"].I(0) != 0;
			A.bAnyHour = V["any_hour"].Type == LyudJson::FValue::Bool ? V["any_hour"].B : (A.Pose == "sit" || A.Pose == "lie");
			return A;
		}

		/** The default act of a thing's kind (made once per kind, per item for "take", per role for "counter"). */
		int DefaultAct(const FThing& T)
		{
			std::string Id = "_" + T.Kind;
			const std::string Item = T.Prop("item");
			const std::string Role = T.Prop("role");
			if (T.Kind == "take" && !Item.empty())
			{
				Id += ":" + Item;
			}
			if (T.Kind == "counter" && !Role.empty())
			{
				Id += ":" + Role;
			}
			const auto Found = ActIndex.find(Id);
			if (Found != ActIndex.end())
			{
				return Found->second;
			}
			FAct A;
			A.Id = Id;
			const std::string& K = T.Kind;
			auto Lines = [&A](const std::string& Base, int N)
			{
				for (int I = 1; I <= N; ++I)
				{
					FLine Line;
					Line.Key = Base + "." + std::to_string(I);
					A.Lines.push_back(Line);
				}
			};
			if (K == "seat")
			{
				A.Prompt = "city.act.sit.prompt";
				A.Pose = "sit";
				A.Event = "sit";
				A.bAnyHour = true;
			}
			else if (K == "bed")
			{
				A.Prompt = "city.act.bed.prompt";
				A.Pose = "lie";
				A.Minutes = (int)(T.PropF("rest_hours", 2.f) * 60.f);
				A.bFade = true;
				A.Event = "rest";
				A.bAnyHour = true;
				Lines("city.act.bed", 2);
			}
			else if (K == "shelf")
			{
				A.Prompt = "city.act.shelf.prompt";
				A.Minutes = 10;
				A.Sound = "page_turn";
				A.Event = "read";
				A.Target = T.Prop("topic");
				Lines("city.act.shelf", 3);
			}
			else if (K == "board")
			{
				A.Prompt = "city.act.board.prompt";
				A.Event = "look";
				A.Target = T.Prop("topic");
				Lines("city.act.board", 3);
			}
			else if (K == "counter")
			{
				A.Prompt = "city.act.counter.prompt";
				A.NeedsStaff = Role;
				A.Event = "talk";
				A.Target = Role;
				Lines("city.act.counter", 2);
			}
			else if (K == "machine")
			{
				A.Prompt = "city.act.machine.prompt";
				A.Minutes = 5;
				A.Event = "machine";
				A.Target = T.Prop("what");
				Lines("city.act.machine", 2);
			}
			else if (K == "play")
			{
				A.Prompt = "city.act.play.prompt";
				A.Minutes = 20;
				A.Event = "play";
				A.Target = T.Prop("what");
				Lines("city.act.play", 2);
			}
			else if (K == "sport")
			{
				A.Prompt = "city.act.sport.prompt";
				A.Minutes = 15;
				A.Event = "sport";
				A.Target = T.Prop("what");
				Lines("city.act.sport", 2);
			}
			else if (K == "craft")
			{
				A.Prompt = "city.act.craft.prompt";
				A.Minutes = 30;
				A.Event = "craft";
				A.Target = T.Prop("what");
				Lines("city.act.craft", 2);
			}
			else if (K == "screen")
			{
				A.Prompt = "city.act.screen.prompt";
				A.Minutes = 30;
				A.bFade = true;
				A.Event = "watch";
				A.Target = T.Prop("what");
				Lines("city.act.screen", 2);
			}
			else if (K == "take")
			{
				A.Prompt = "city.act.take.prompt";
				A.Daily = 1;
				A.Event = "take";
				A.Target = Item;
				if (!Item.empty())
				{
					A.Give.emplace_back(Item, 1);
				}
				Lines("city.act.take", 1);
			}
			else if (K == "door")
			{
				A.Prompt = "city.act.door.prompt";
				A.Event = "look";
				A.Target = T.Prop("to");
				A.bAnyHour = true;
				Lines("city.act.door", 1);
			}
			else
			{
				A.Prompt = "city.act.use.prompt";
				Lines("city.act.board", 3);
			}
			if (A.Target.empty())
			{
				A.Target = K;
			}
			const int Index = (int)Acts.size();
			ActIndex[Id] = Index;
			Acts.push_back(std::move(A));
			return Index;
		}
	};
}
