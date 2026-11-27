// The yacht school's rules in plain C++ (docs/yacht_school.md, part B "School rules and data"): the season, the day's
// go / no-go and the signal mast, the weekly timetable and a session's phases, the sailing water (areas, the launch point
// at any tide), courses set from the wind, mark rounding, the start, scoring and series, a cheap resolver of races nobody
// watches, the hero's lessons and ranks, the logbook that is saved, and the facts for the paper and the radio.
// No engine types: ULyudSailing drives it and tools/sailschoolsim checks it outside the engine on the real
// Content/City/sailing.json (tools/sailing/sailing.py writes that from tools/sailing/content.json).
//
// The boat's physics and the helmsman are SailModel.{h,cpp} (part A), in this namespace too: this file never includes it
// and takes positions, headings and wind angles from the caller. Every name here starts with School, Season, Timetable,
// Race or Lesson so the two cannot clash. Boat classes are ints in the contract's order (EBoat): 0 optimist, 1 cadet, 2 luch.
//
// Frame: Unreal world metres, +X to the sea (compass west), +Y north. Seen from above this frame is mirrored, so "left" of a
// direction V is (V.Y, -V.X). Winds are given as the unit vector the air moves along (WindTo); SchoolWindTo() makes one from a
// compass bearing the wind comes from. Hours are local, weekdays 0 = Monday, tide metres over the datum (0 high water).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../People/MiniJson.h"

namespace LyudSail
{
	constexpr double SchoolPi = 3.14159265358979323846;
	constexpr int SchoolBoats = 3;
	constexpr int SchoolOptimist = 0, SchoolCadet = 1, SchoolLuch = 2;

	inline const char* SchoolBoatId(int Class) { return Class == 0 ? "optimist" : (Class == 1 ? "cadet" : (Class == 2 ? "luch" : "")); }
	inline int SchoolBoatClass(const std::string& Id) { return Id == "optimist" ? 0 : (Id == "cadet" ? 1 : (Id == "luch" ? 2 : -1)); }

	struct FSchoolV2
	{
		double X = 0.0, Y = 0.0;
		FSchoolV2() = default;
		FSchoolV2(double InX, double InY) : X(InX), Y(InY) {}
		FSchoolV2 operator+(const FSchoolV2& O) const { return FSchoolV2(X + O.X, Y + O.Y); }
		FSchoolV2 operator-(const FSchoolV2& O) const { return FSchoolV2(X - O.X, Y - O.Y); }
		FSchoolV2 operator*(double S) const { return FSchoolV2(X * S, Y * S); }
		double Dot(const FSchoolV2& O) const { return X * O.X + Y * O.Y; }
		double Cross(const FSchoolV2& O) const { return X * O.Y - Y * O.X; }
		double Len() const { return std::sqrt(X * X + Y * Y); }
		FSchoolV2 Norm() const { const double L = Len(); return L > 1e-9 ? FSchoolV2(X / L, Y / L) : FSchoolV2(); }
		/** The direction on the left hand of this one, seen from above on the map (the frame is mirrored: see the header). */
		FSchoolV2 Left() const { return FSchoolV2(Y, -X); }
	};

	inline double SchoolClamp(double V, double A, double B) { return V < A ? A : (V > B ? B : V); }

	/** The unit vector the air moves along, from the compass bearing the wind comes FROM (0 north, 90 east; east is -X). */
	inline FSchoolV2 SchoolWindTo(double FromCompassDeg)
	{
		const double A = FromCompassDeg * SchoolPi / 180.0;
		return FSchoolV2(std::sin(A), -std::cos(A));
	}

	/** A small deterministic random stream (xorshift32). */
	struct FSchoolRng
	{
		uint32_t S = 2463534242u;
		explicit FSchoolRng(uint32_t Seed = 2463534242u) : S(Seed ? Seed : 2463534242u) {}
		uint32_t Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		double U() { return (Next() & 0xFFFFFF) / double(0x1000000); }
		/** About normal(0, 1): the sum of four uniforms. */
		double N() { return (U() + U() + U() + U() - 2.0) * 1.7320508; }
	};

	inline uint32_t SchoolHash(uint32_t A, uint32_t B = 0x9E3779B9u)
	{
		uint32_t H = A * 0x85EBCA6Bu ^ (B + 0x9E3779B9u + (A << 6) + (A >> 2));
		H ^= H >> 16; H *= 0x7FEB352Du; H ^= H >> 15; H *= 0x846CA68Bu; H ^= H >> 16;
		return H ? H : 1u;
	}

	inline uint32_t SchoolHashStr(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (const char C : S) { H ^= (uint8_t)C; H *= 16777619u; }
		return H ? H : 1u;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The calendar

	struct FSchoolDate
	{
		int Y = 2026, M = 1, D = 1;
		FSchoolDate() = default;
		FSchoolDate(int InY, int InM, int InD) : Y(InY), M(InM), D(InD) {}
		bool operator==(const FSchoolDate& O) const { return Y == O.Y && M == O.M && D == O.D; }
	};

	/** Days since 1970-01-01 (proleptic Gregorian) and back. */
	inline long long SchoolDayNumber(const FSchoolDate& Date)
	{
		const int Y = Date.M <= 2 ? Date.Y - 1 : Date.Y;
		const long long Era = (Y >= 0 ? Y : Y - 399) / 400;
		const long long Yoe = Y - Era * 400;
		const long long Doy = (153 * (Date.M + (Date.M > 2 ? -3 : 9)) + 2) / 5 + Date.D - 1;
		const long long Doe = Yoe * 365 + Yoe / 4 - Yoe / 100 + Doy;
		return Era * 146097 + Doe - 719468;
	}

	inline FSchoolDate SchoolDateOf(long long Days)
	{
		const long long Z = Days + 719468;
		const long long Era = (Z >= 0 ? Z : Z - 146096) / 146097;
		const long long Doe = Z - Era * 146097;
		const long long Yoe = (Doe - Doe / 1460 + Doe / 36524 - Doe / 146096) / 365;
		const long long Doy = Doe - (365 * Yoe + Yoe / 4 - Yoe / 100);
		const long long Mp = (5 * Doy + 2) / 153;
		FSchoolDate Out;
		Out.D = (int)(Doy - (153 * Mp + 2) / 5 + 1);
		Out.M = (int)(Mp < 10 ? Mp + 3 : Mp - 9);
		Out.Y = (int)(Yoe + Era * 400 + (Out.M <= 2 ? 1 : 0));
		return Out;
	}

	/** 0 Monday .. 6 Sunday. */
	inline int SchoolWeekday(const FSchoolDate& Date)
	{
		const long long N = SchoolDayNumber(Date);
		return (int)(((N + 3) % 7 + 7) % 7);
	}

	inline FSchoolDate SchoolAddDays(const FSchoolDate& Date, int N) { return SchoolDateOf(SchoolDayNumber(Date) + N); }

	/** The first day on or after From that is weekday Wd. */
	inline FSchoolDate SchoolWeekdayOnOrAfter(const FSchoolDate& From, int Wd)
	{
		return SchoolAddDays(From, ((Wd - SchoolWeekday(From)) % 7 + 7) % 7);
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Content/City/sailing.json

	struct FSchoolPoint
	{
		double X = 0.0, Y = 0.0, Z = 0.0, Yaw = 0.0;
		FSchoolV2 At() const { return FSchoolV2(X, Y); }
	};

	/** A named place of the school: its middle and, when it is a row, its slots. */
	struct FSchoolPlace
	{
		std::string Id;
		FSchoolPoint At;
		std::vector<FSchoolPoint> Slots;
	};

	struct FSchoolRect
	{
		std::string PlanId;
		FSchoolPoint At;
		double SizeSea = 0.0, SizeCoast = 0.0;
		std::vector<FSchoolV2> Corners;
	};

	struct FSchoolBoatSlot
	{
		std::string Id;
		int Class = -1;             // -1: the launch
		FSchoolPoint At, Rig, Hall;
		int HallTier = 0;
	};

	struct FSchoolArea
	{
		std::string Id;
		double MinDepth = 0.5;
		std::vector<FSchoolV2> Poly;
	};

	struct FSchoolSailor
	{
		std::string Slot, Group, Crew, Person;
		int Class = 0;
		double Skill = 0.5, Heavy = 0.0;
	};

	struct FSchoolGroup
	{
		std::string Id, Condition, Coach;
		std::vector<int> Boats;
		std::vector<std::string> Drills;
	};

	struct FSchoolDrill
	{
		std::string Id, Area, Course, Launch;
		double Wind[2] = { 1.5, 12.0 };
		double Station[2] = { 0.0, 0.0 };       // metres upwind / to the right of the drill's middle
		bool bWarmOnly = false;
	};

	struct FTimetableEntry
	{
		int Days = 0;               // a bit per weekday
		double From = 10.0, Len = 2.5;
		std::string Group;
	};

	struct FTimetablePhaseDef
	{
		std::string Id, Place, Activity;
		double Minutes = 0.0;       // 0: what is left
	};

	struct FSchoolSignalDef
	{
		std::vector<std::string> Flags;
		std::string ClassFlag[SchoolBoats];
		int Horn = 0;
	};

	struct FRaceEventDef
	{
		std::string Id, Rule, Course, Stamp, Festival;
		int Month = 0, Day = 0, Weekday = -1, Days = 1, Races = 1, EntryFee = 0;
		double From = 11.0, Len = 4.0, Pull = 0.5;
		bool Classes[SchoolBoats] = { false, false, false };
		bool bParade = false, bLayupAfter = false, bCup = false, bChampionship = false;
	};

	struct FLessonObjective
	{
		std::string Kind, Event, Course;
		int Count = 1, Laps = 1;
		double Seconds = 0.0, Twa[2] = { 0.0, 180.0 }, WithinS = 0.0, RadiusM = 0.0, MaxSpeed = 0.0;
		bool bNoIrons = false, bNoCapsize = false;
	};

	struct FLessonDef
	{
		std::string Id, Area, NeedsRank;
		bool bAfloat = true, bWarmOnly = false;
		double Wind[2] = { 0.0, 99.0 };
		std::vector<FLessonObjective> Objectives;
	};

	struct FSchoolRankDef
	{
		std::string Id, Name;
		std::vector<std::string> Lessons;
		int Races = 0, Podiums = 0;
		double Miles = 0.0;
		bool bCertificate = false;
	};

	/** Every number of sailing.json the rules use. */
	struct FSchoolParams
	{
		std::string BedSource = "macro";
		double SeaZ = -43.0, TideNeapLow = -2.0, TideSpringLow = -4.5;
		// the school's ground
		double Yaw = 0.0;
		FSchoolRect Boathouse, Park, Slip;
		double FloorZ = 0.0;
		std::vector<FSchoolPlace> Places;
		std::vector<FSchoolBoatSlot> BoatSlots;
		// the water
		std::vector<std::pair<double, double>> Shore;      // (y, x of the high-water line)
		std::vector<FSchoolArea> Areas;
		std::map<std::string, std::vector<std::string>> GroupArea;
		std::vector<std::string> ClassArea[SchoolBoats];
		FSchoolV2 LaunchOrigin, LaunchDir = FSchoolV2(1.0, 0.0);
		double BedStep = 5.0, SlipFootD = 14.0, SwimLineM = 60.0;
		std::vector<double> Bed;                            // the bed under the datum along the launch line
		FSchoolV2 PierA, PierB; double PierHalfW = 6.0, PierClear = 8.0, PierHeadClear = 28.0;
		FSchoolV2 MooringA, MooringB; double MooringClear = 6.0;
		FSchoolV2 IceHole; double IceHoleClear = 20.0;
		FSchoolV2 BankA, BankB; double BankClear = 300.0;
		FSchoolV2 Island; double IslandR = 105.0, IslandClear = 500.0;
		// the fleet and the people
		int BoatCount[SchoolBoats] = { 8, 4, 9 };
		double Loa[SchoolBoats] = { 2.3, 3.22, 4.23 }, SpeedMs[SchoolBoats] = { 1.7, 2.4, 3.0 };
		std::string HeroBoat = "luch_9";
		double HeroSkill = 0.5;
		std::vector<FSchoolSailor> Sailors;
		std::vector<FSchoolGroup> Groups;
		std::vector<std::string> Coaches;
		// the season
		int Opening[2] = { 4, 25 }, OpeningWeekday = 5, ClosingMonth = 11, ClosingNth = 1, ClosingWeekday = 6;
		int FitoutDays = 14, LayupDays = 7, Summer[2][2] = { { 6, 1 }, { 8, 31 } }, TheoryDays = 0;
		double TheoryHours[2] = { 15.0, 18.0 }, FitoutHours[2] = { 10.0, 17.0 };
		double Latitude = 54.95, Longitude = 20.3, TzHours = 2.0, DaylightMargin = 0.5;
		// limits
		double Wind[SchoolBoats][2] = { { 1.5, 8.0 }, { 1.5, 10.0 }, { 1.5, 12.0 } };
		double GustOver = 3.0, RecallMargin = 1.5, StormWind = 14.0, StormGust = 19.0, FogVisibility = 400.0;
		double LaunchDepth = 0.5, BarDepth = 0.3, BoardFullDepth = 1.0, MaxHaul = 260.0, KidsDeepWind = 6.0, ReefWind = 9.0;
		double CapsizeWaterC = 15.0, CapsizeWind = 6.0, MinWaterH = 0.75;
		double RescueKidsS = 20.0, RescueS = 60.0, TowSpeed = 2.5, LaunchSpeed = 6.0, StrayM = 40.0;
		std::map<std::string, FSchoolSignalDef> Signals;
		// the timetable
		std::vector<FTimetableEntry> Term, SummerWeek;
		std::map<std::string, std::vector<FTimetablePhaseDef>> Phases;
		std::vector<std::string> ShoreTopics;
		int DrillBlocks = 2;
		double HireFrom = 10.0, HireBeforeSunset = 1.0; int HireFee = 50;
		std::vector<FSchoolDrill> Drills;
		// courses
		double WindRef = 6.0, CourseScale[2] = { 0.6, 0.4 }, CourseMargin = 15.0, CourseShrink = 0.88, MinLeg = 40.0;
		double LegM[SchoolBoats] = { 130.0, 220.0, 300.0 }, LegClamp[2] = { 0.55, 1.25 }, KidsLeg = 70.0, WingDeg = 45.0;
		double LeewardGap = 20.0, LinePerBoat = 1.5, LineMin = 24.0, Figure8Gap = 50.0, PracticeLine = 30.0, MarkR = 0.45;
		int LapsWL = 2, LapsTriangle = 1, LapsKids = 2;
		// races
		double WarningS = 180.0, PrepS = 60.0, XFlagS = 120.0, StartWindowS = 240.0, GeneralRecallFrac = 0.5;
		std::map<std::string, std::string> StartFlags;
		double ZoneM = 60.0, TouchM = 1.1, PenaltyTurnDeg = 360.0, PenaltyWindowS = 90.0;
		double TimeLimitS[SchoolBoats] = { 1500.0, 1800.0, 2100.0 }, FinishWindowS = 600.0, BetweenRacesS = 600.0;
		double LuckSigma = 0.2, HeavyGain = 0.12, TimeBase = 1.3, TimeGain = 0.4, DnfBase = 0.01, DnfWind = 0.22, OcsChance = 0.03;
		double DnsChance = 0.03, BeatFactor = 1.45;
		std::vector<std::pair<int, int>> Discards;
		std::vector<FRaceEventDef> Events;
		// the hero
		int JoinFee = 150, LessonFee = 0, HeroClass = SchoolLuch, MendWeekday = 0;
		std::string CardItem = "sail_card";
		double LessonFrom = 10.0, LessonBeforeSunset = 1.5, WearPerCapsize = 0.03, WearPerHour = 0.004;
		double IronsSpeed = 0.35, IronsTwa = 30.0, IronsS = 4.0, AsternSpeed = 0.2, FollowM = 25.0;
		std::vector<FSchoolRankDef> Ranks;
		std::vector<FLessonDef> Lessons;
		std::string StampSet = "sailing";
		std::vector<std::string> StampPieces;
		double MileMetres = 1852.0;
		std::vector<std::pair<int, std::string>> MileStamps;
		std::map<std::string, std::string> RadioForecast, RadioNews;

		bool Load(const std::string& Text, std::string& Error)
		{
			LyudJson::FValue J;
			if (!LyudJson::Parse(Text, J, Error))
			{
				return false;
			}
			typedef LyudJson::FValue FV;
			auto Num = [](const FV& O, const char* K, double& Out) { if (O[K].Type == FV::Number) Out = O[K].Num; };
			auto Int = [](const FV& O, const char* K, int& Out) { if (O[K].Type == FV::Number) Out = (int)O[K].Num; };
			auto Arr = [](const FV& A, double* Out, int N) { for (int I = 0; I < N && I < (int)A.Size(); I++) Out[I] = A[(size_t)I].Num; };
			auto P2 = [](const FV& A) { return FSchoolV2(A[(size_t)0].Num, A[(size_t)1].Num); };
			auto Pt = [](const FV& O) { FSchoolPoint Q; const FV& A = O["pos"]; Q.X = A[(size_t)0].Num; Q.Y = A[(size_t)1].Num; Q.Z = A[(size_t)2].Num; Q.Yaw = O["yaw"].Num; return Q; };
			auto At3 = [](const FV& A, double Yaw) { FSchoolPoint Q; Q.X = A[(size_t)0].Num; Q.Y = A[(size_t)1].Num; Q.Z = A[(size_t)2].Num; Q.Yaw = Yaw; return Q; };
			auto Strs = [](const FV& A) { std::vector<std::string> Out; for (const FV& V : A.Items) Out.push_back(V.S()); return Out; };
			auto Rect = [&](const FV& O)
			{
				FSchoolRect R;
				R.PlanId = O["plan_id"].S();
				R.At = At3(O["at"], O["yaw"].Num);
				R.SizeSea = O["size"][(size_t)0].Num; R.SizeCoast = O["size"][(size_t)1].Num;
				for (const FV& C : O["corners"].Items) R.Corners.push_back(P2(C));
				return R;
			};

			const FV& S = J["school"];
			const FV& W = J["water"];
			if (S.IsNull() || W.IsNull() || J["timetable"].IsNull() || J["lessons"].IsNull())
			{
				Error = "sailing.json: no school, water, timetable or lessons";
				return false;
			}
			BedSource = J["bed_source"].S("macro");
			Num(J, "sea_z", SeaZ); Num(J["tide"], "neap_low", TideNeapLow); Num(J["tide"], "spring_low", TideSpringLow);
			Num(S, "yaw", Yaw);
			Boathouse = Rect(S["boathouse"]); Park = Rect(S["park"]); Slip = Rect(S["slip"]);
			Num(S["boathouse"], "floor_z", FloorZ);
			Places.clear();
			for (const auto& F : S["points"].Fields)
			{
				FSchoolPlace Pl;
				Pl.Id = F.first;
				Pl.At = Pt(F.second);
				for (const FV& Sl : F.second["slots"].Items) Pl.Slots.push_back(Pt(Sl));
				Places.push_back(Pl);
			}
			BoatSlots.clear();
			for (const FV& B : S["park_slots"].Items)
			{
				FSchoolBoatSlot Sl;
				Sl.Id = B["id"].S();
				Sl.Class = SchoolBoatClass(B["boat"].S());
				Sl.At = Pt(B);
				Sl.Rig = At3(B["rig"], Sl.At.Yaw);
				for (const FV& H : S["hall_slots"].Items)
				{
					if (H["id"].S() == Sl.Id) { Sl.Hall = Pt(H); Sl.HallTier = H["tier"].I(); }
				}
				BoatSlots.push_back(Sl);
			}

			Shore.clear();
			for (const FV& P : W["shore"].Items) Shore.push_back({ P[(size_t)0].Num, P[(size_t)1].Num });
			Areas.clear();
			for (const auto& F : W["areas"].Fields)
			{
				FSchoolArea A;
				A.Id = F.first;
				Num(F.second, "min_depth", A.MinDepth);
				for (const FV& P : F.second["poly"].Items) A.Poly.push_back(P2(P));
				Areas.push_back(A);
			}
			GroupArea.clear();
			for (const auto& F : W["group_area"].Fields) GroupArea[F.first] = Strs(F.second);
			for (int C = 0; C < SchoolBoats; C++) ClassArea[C] = Strs(W["class_area"][SchoolBoatId(C)]);
			Num(W, "swim_line_m", SwimLineM);
			const FV& L = W["launch"];
			LaunchOrigin = P2(L["origin"]); LaunchDir = P2(L["dir"]).Norm();
			Num(L, "step_m", BedStep); Num(L, "slip_foot_d", SlipFootD);
			Bed.clear();
			for (const FV& B : L["bed"].Items) Bed.push_back(B.Num);
			const FV& K = W["keep_clear"];
			PierA = P2(K["pier"]["a"]); PierB = P2(K["pier"]["b"]);
			Num(K["pier"], "half_w", PierHalfW); Num(K["pier"], "clear_m", PierClear); Num(K["pier"], "head_clear_m", PierHeadClear);
			MooringA = P2(K["mooring"]["a"]); MooringB = P2(K["mooring"]["b"]); Num(K["mooring"], "clear_m", MooringClear);
			IceHole = P2(K["ice_hole"]["at"]); Num(K["ice_hole"], "clear_m", IceHoleClear);
			BankA = P2(K["bank"]["a"]); BankB = P2(K["bank"]["b"]); Num(K["bank"], "clear_m", BankClear);
			Island = P2(K["island"]["at"]); Num(K["island"], "r", IslandR); Num(K["island"], "clear_m", IslandClear);

			const FV& Fl = J["fleet"];
			for (int C = 0; C < SchoolBoats; C++)
			{
				Int(Fl["boats"], SchoolBoatId(C), BoatCount[C]);
				Num(Fl["loa_m"], SchoolBoatId(C), Loa[C]);
				Num(Fl["speed_ms"], SchoolBoatId(C), SpeedMs[C]);
			}
			HeroBoat = Fl["hero_boat"].S(HeroBoat);
			Num(Fl, "hero_skill", HeroSkill);
			Sailors.clear();
			for (const FV& V : Fl["sailors"].Items)
			{
				FSchoolSailor Sa;
				Sa.Slot = V["slot"].S(); Sa.Group = V["group"].S(); Sa.Crew = V["crew"].S(); Sa.Person = V["person"].S();
				Sa.Class = SchoolBoatClass(V["boat"].S());
				Num(V, "skill", Sa.Skill); Num(V, "heavy", Sa.Heavy);
				Sailors.push_back(Sa);
			}
			Groups.clear();
			for (const auto& F : J["groups"].Fields)
			{
				FSchoolGroup G;
				G.Id = F.first; G.Condition = F.second["condition"].S(); G.Coach = F.second["coach"].S();
				for (const FV& B : F.second["boats"].Items) G.Boats.push_back(SchoolBoatClass(B.S()));
				G.Drills = Strs(F.second["drills"]);
				Groups.push_back(G);
			}
			Coaches.clear();
			for (const auto& F : J["coaches"].Fields) Coaches.push_back(F.first);

			const FV& Se = J["season"];
			Opening[0] = Se["opening"][(size_t)0].I(4); Opening[1] = Se["opening"][(size_t)1].I(25);
			Int(Se, "opening_weekday", OpeningWeekday); Int(Se, "closing_month", ClosingMonth); Int(Se, "closing_nth", ClosingNth);
			Int(Se, "closing_weekday", ClosingWeekday); Int(Se, "fitout_days", FitoutDays); Int(Se, "layup_days", LayupDays);
			for (int A = 0; A < 2; A++) for (int B = 0; B < 2; B++) Summer[A][B] = Se["summer"][(size_t)A][(size_t)B].I(Summer[A][B]);
			TheoryDays = 0;
			for (const FV& D : Se["theory_days"].Items) TheoryDays |= 1 << D.I();
			Arr(Se["theory_hours"], TheoryHours, 2); Arr(Se["fitout_hours"], FitoutHours, 2);
			Num(Se, "latitude", Latitude); Num(Se, "longitude", Longitude); Num(Se, "tz_hours", TzHours); Num(Se, "daylight_margin_h", DaylightMargin);

			const FV& Li = J["limits"];
			for (int C = 0; C < SchoolBoats; C++) Arr(Li["wind"][SchoolBoatId(C)], Wind[C], 2);
			Num(Li, "gust_over", GustOver); Num(Li, "recall_margin", RecallMargin); Num(Li, "storm_wind", StormWind); Num(Li, "storm_gust", StormGust);
			Num(Li, "fog_visibility_m", FogVisibility); Num(Li, "launch_depth_m", LaunchDepth); Num(Li, "bar_depth_m", BarDepth);
			Num(Li, "board_full_depth_m", BoardFullDepth); Num(Li, "max_haul_m", MaxHaul); Num(Li, "kids_deep_wind", KidsDeepWind);
			Num(Li, "reef_wind", ReefWind); Num(Li, "capsize_drill_water_c", CapsizeWaterC); Num(Li, "capsize_drill_wind", CapsizeWind);
			Num(Li, "min_water_h", MinWaterH);
			const FV& Re = Li["rescue"];
			Num(Re, "kids_capsized_s", RescueKidsS); Num(Re, "capsized_s", RescueS); Num(Re, "tow_speed", TowSpeed);
			Num(Re, "launch_speed", LaunchSpeed); Num(Re, "stray_m", StrayM);
			Signals.clear();
			for (const auto& F : J["signals"].Fields)
			{
				FSchoolSignalDef D;
				D.Flags = Strs(F.second["flags"]);
				for (int C = 0; C < SchoolBoats; C++) D.ClassFlag[C] = F.second["class_flags"][SchoolBoatId(C)].S();
				Int(F.second, "horn", D.Horn);
				Signals[F.first] = D;
			}

			const FV& Tt = J["timetable"];
			auto Entries = [&](const FV& A, std::vector<FTimetableEntry>& Out)
			{
				Out.clear();
				for (const FV& V : A.Items)
				{
					FTimetableEntry E;
					for (const FV& D : V["days"].Items) E.Days |= 1 << D.I();
					Num(V, "from", E.From); Num(V, "len", E.Len);
					E.Group = V["group"].S();
					Out.push_back(E);
				}
			};
			Entries(Tt["term"], Term); Entries(Tt["summer"], SummerWeek);
			Phases.clear();
			for (const auto& F : Tt["phases"].Fields)
			{
				std::vector<FTimetablePhaseDef> Out;
				for (const FV& V : F.second.Items)
				{
					FTimetablePhaseDef D;
					D.Id = V[(size_t)0].S(); D.Minutes = V[(size_t)1].Num; D.Place = V[(size_t)2].S(); D.Activity = V[(size_t)3].S();
					Out.push_back(D);
				}
				Phases[F.first] = Out;
			}
			ShoreTopics = Strs(Tt["shore_topics"]);
			Int(Tt, "drill_blocks", DrillBlocks);
			Num(Tt["hire"], "from", HireFrom); Num(Tt["hire"], "before_sunset_h", HireBeforeSunset); Int(Tt["hire"], "fee", HireFee);
			Drills.clear();
			for (const auto& F : J["drills"].Fields)
			{
				FSchoolDrill D;
				D.Id = F.first; D.Area = F.second["area"].S(); D.Course = F.second["course"].S();
				Arr(F.second["wind"], D.Wind, 2);
				if (F.second["launch"].Type == FV::String) D.Launch = F.second["launch"].S(); else Arr(F.second["launch"], D.Station, 2);
				D.bWarmOnly = F.second["warm_only"].B;
				Drills.push_back(D);
			}

			const FV& Co = J["course"];
			Num(Co, "wind_ref", WindRef); Arr(Co["scale"], CourseScale, 2); Num(Co, "margin_m", CourseMargin); Num(Co, "shrink", CourseShrink);
			Num(Co, "min_leg_m", MinLeg); Arr(Co["leg_clamp"], LegClamp, 2); Num(Co, "kids_triangle_leg_m", KidsLeg); Num(Co, "wing_deg", WingDeg);
			Num(Co, "leeward_gap_m", LeewardGap); Num(Co, "line_per_boat", LinePerBoat); Num(Co, "line_min_m", LineMin);
			Num(Co, "figure8_gap_m", Figure8Gap); Num(Co, "start_line_practice_m", PracticeLine); Num(Co, "mark_r_m", MarkR);
			for (int C = 0; C < SchoolBoats; C++) Num(Co["leg_m"], SchoolBoatId(C), LegM[C]);
			Int(Co["laps"], "windward_leeward", LapsWL); Int(Co["laps"], "triangle", LapsTriangle); Int(Co["laps"], "kids_triangle", LapsKids);

			const FV& Ra = J["races"];
			const FV& St = Ra["start"];
			Num(St, "warning_s", WarningS); Num(St, "prep_s", PrepS); Num(St, "x_flag_s", XFlagS); Num(St, "start_window_s", StartWindowS);
			Num(St, "general_recall_frac", GeneralRecallFrac);
			StartFlags.clear();
			for (const auto& F : St["flags"].Fields) StartFlags[F.first] = F.second.S();
			Num(Ra, "zone_m", ZoneM); Num(Ra, "touch_m", TouchM); Num(Ra, "penalty_turn_deg", PenaltyTurnDeg); Num(Ra, "penalty_window_s", PenaltyWindowS);
			for (int C = 0; C < SchoolBoats; C++) Num(Ra["time_limit_s"], SchoolBoatId(C), TimeLimitS[C]);
			Num(Ra, "finish_window_s", FinishWindowS); Num(Ra, "between_races_s", BetweenRacesS);
			const FV& Rs = Ra["resolve"];
			Num(Rs, "luck_sigma", LuckSigma); Num(Rs, "heavy_gain", HeavyGain); Num(Rs, "time_base", TimeBase); Num(Rs, "time_gain", TimeGain);
			Num(Rs, "dnf_base", DnfBase); Num(Rs, "dnf_wind", DnfWind); Num(Rs, "ocs", OcsChance); Num(Rs, "dns", DnsChance); Num(Rs, "beat_factor", BeatFactor);
			Discards.clear();
			for (const FV& D : Ra["scoring"]["discards"].Items) Discards.push_back({ D[(size_t)0].I(), D[(size_t)1].I() });
			Events.clear();
			for (const FV& V : Ra["events"].Items)
			{
				FRaceEventDef E;
				E.Id = V["id"].S(); E.Rule = V["rule"].S(); E.Course = V["course"].S(); E.Stamp = V["stamp"].S(); E.Festival = V["festival"].S();
				E.Month = V["date"][(size_t)0].I(); E.Day = V["date"][(size_t)1].I();
				Int(V, "weekday", E.Weekday); Int(V, "days", E.Days); Int(V, "races", E.Races); Int(V, "entry_fee", E.EntryFee);
				Num(V, "from", E.From); Num(V, "len", E.Len); Num(V, "pull", E.Pull);
				for (const FV& C : V["classes"].Items) { const int Cls = SchoolBoatClass(C.S()); if (Cls >= 0) E.Classes[Cls] = true; }
				E.bParade = V["parade"].B; E.bLayupAfter = V["layup_after"].B; E.bCup = V["cup"].B; E.bChampionship = V["championship"].B;
				Events.push_back(E);
			}

			const FV& He = J["hero"];
			Int(He, "join_fee", JoinFee); Int(He, "lesson_fee", LessonFee);
			CardItem = He["card_item"].S(CardItem);
			HeroClass = std::max(0, SchoolBoatClass(He["boat"].S("luch")));
			Num(He, "lesson_hours_from", LessonFrom); Num(He, "lesson_before_sunset_h", LessonBeforeSunset);
			Num(He["wear"], "per_capsize", WearPerCapsize); Num(He["wear"], "per_hour", WearPerHour); Int(He["wear"], "mend_weekday", MendWeekday);
			const FV& Te = He["telemetry"];
			Num(Te, "irons_speed", IronsSpeed); Num(Te, "irons_twa", IronsTwa); Num(Te, "irons_s", IronsS); Num(Te, "astern_speed", AsternSpeed);
			Num(Te, "follow_m", FollowM);
			Ranks.clear();
			for (const FV& V : He["ranks"].Items)
			{
				FSchoolRankDef R;
				R.Id = V["id"].S(); R.Name = V["name"].S(); R.Lessons = Strs(V["lessons"]);
				Int(V, "races", R.Races); Int(V, "podiums", R.Podiums); Num(V, "miles", R.Miles);
				R.bCertificate = V["certificate"].B;
				Ranks.push_back(R);
			}
			Lessons.clear();
			for (const FV& V : J["lessons"].Items)
			{
				FLessonDef Le;
				Le.Id = V["id"].S(); Le.Area = V["area"].S(); Le.NeedsRank = V["needs_rank"].S();
				Le.bAfloat = V["afloat"].Type == FV::Bool ? V["afloat"].B : true;
				Le.bWarmOnly = V["warm_only"].B;
				Arr(V["wind"], Le.Wind, 2);
				for (const FV& O : V["objectives"].Items)
				{
					FLessonObjective Ob;
					Ob.Kind = O["kind"].S(); Ob.Event = O["event"].S(); Ob.Course = O["course"].S();
					Int(O, "count", Ob.Count); Int(O, "laps", Ob.Laps);
					Num(O, "seconds", Ob.Seconds); Arr(O["twa"], Ob.Twa, 2); Num(O, "within_s", Ob.WithinS); Num(O, "radius_m", Ob.RadiusM);
					Num(O, "max_speed", Ob.MaxSpeed);
					Ob.bNoIrons = O["no_irons"].B; Ob.bNoCapsize = O["no_capsize"].B;
					Le.Objectives.push_back(Ob);
				}
				Lessons.push_back(Le);
			}
			StampSet = J["stamps"]["set"].S(StampSet);
			StampPieces = Strs(J["stamps"]["pieces"]);
			Num(J["miles"], "metres", MileMetres);
			MileStamps.clear();
			for (const FV& V : J["miles"]["stamps"].Items) MileStamps.push_back({ V[(size_t)0].I(), V[(size_t)1].S() });
			RadioForecast.clear(); RadioNews.clear();
			for (const auto& F : J["radio"]["forecast"].Fields) RadioForecast[F.first] = F.second.S();
			for (const auto& F : J["radio"]["news"].Fields) RadioNews[F.first] = F.second.S();
			if (Areas.empty() || Bed.size() < 2 || Lessons.empty() || Groups.empty() || Shore.size() < 2)
			{
				Error = "sailing.json: no areas, bed, lessons, groups or shore";
				return false;
			}
			return true;
		}

		const FSchoolPlace* Place(const std::string& Id) const
		{
			for (const FSchoolPlace& Pl : Places) if (Pl.Id == Id) return &Pl;
			return nullptr;
		}
		const FSchoolArea* Area(const std::string& Id) const
		{
			for (const FSchoolArea& A : Areas) if (A.Id == Id) return &A;
			return nullptr;
		}
		const FSchoolGroup* Group(const std::string& Id) const
		{
			for (const FSchoolGroup& G : Groups) if (G.Id == Id) return &G;
			return nullptr;
		}
		const FSchoolDrill* Drill(const std::string& Id) const
		{
			for (const FSchoolDrill& D : Drills) if (D.Id == Id) return &D;
			return nullptr;
		}
		int Event(const std::string& Id) const
		{
			for (size_t I = 0; I < Events.size(); I++) if (Events[I].Id == Id) return (int)I;
			return -1;
		}
		int Lesson(const std::string& Id) const
		{
			for (size_t I = 0; I < Lessons.size(); I++) if (Lessons[I].Id == Id) return (int)I;
			return -1;
		}
		int Rank(const std::string& Id) const
		{
			for (size_t I = 0; I < Ranks.size(); I++) if (Ranks[I].Id == Id) return (int)I;
			return -1;
		}
		bool HasStamp(const std::string& Piece) const
		{
			for (const std::string& S : StampPieces) if (S == Piece) return true;
			return false;
		}
	};

	// ------------------------------------------------------------------------------------------------------------------
	// The season and the light

	enum class ESeasonState : uint8_t { LaidUp, FitOut, Open, LayUp };

	/** The opening day of a year: the first Saturday on or after April 25. */
	inline FSchoolDate SeasonOpening(const FSchoolParams& P, int Year)
	{
		return SchoolWeekdayOnOrAfter(FSchoolDate(Year, P.Opening[0], P.Opening[1]), P.OpeningWeekday);
	}

	/** The closing day: the first Sunday of November. */
	inline FSchoolDate SeasonClosing(const FSchoolParams& P, int Year)
	{
		return SchoolAddDays(SchoolWeekdayOnOrAfter(FSchoolDate(Year, P.ClosingMonth, 1), P.ClosingWeekday), 7 * (P.ClosingNth - 1));
	}

	inline ESeasonState SeasonStateOn(const FSchoolParams& P, const FSchoolDate& Date)
	{
		const long long N = SchoolDayNumber(Date), O = SchoolDayNumber(SeasonOpening(P, Date.Y)), C = SchoolDayNumber(SeasonClosing(P, Date.Y));
		if (N >= O && N <= C) return ESeasonState::Open;
		if (N < O && N >= O - P.FitoutDays) return ESeasonState::FitOut;
		if (N > C && N <= C + P.LayupDays) return ESeasonState::LayUp;
		return ESeasonState::LaidUp;
	}

	/** Boats may be on the water by the calendar and the ice (the contract's sail_season). */
	inline bool SeasonOpen(const FSchoolParams& P, const FSchoolDate& Date, bool bSeaIce = false)
	{
		return !bSeaIce && SeasonStateOn(P, Date) == ESeasonState::Open;
	}

	/** The summer holidays' timetable (June 1 - August 31). */
	inline bool SeasonIsSummer(const FSchoolParams& P, const FSchoolDate& Date)
	{
		const int K = Date.M * 100 + Date.D;
		return K >= P.Summer[0][0] * 100 + P.Summer[0][1] && K <= P.Summer[1][0] * 100 + P.Summer[1][1];
	}

	struct FSchoolDaylight
	{
		double Sunrise = 6.0, Sunset = 18.0;
	};

	/** Sunrise and sunset, local hours, to a few minutes (the sun's declination and the equation of time by the day of the
	 *  year). The timetable is laid out by it; the day's go / no-go takes the sky's real light from the caller. */
	inline FSchoolDaylight SchoolDaylight(const FSchoolParams& P, const FSchoolDate& Date)
	{
		const double N = (double)(SchoolDayNumber(Date) - SchoolDayNumber(FSchoolDate(Date.Y, 1, 1)) + 1);
		const double Decl = 23.44 * SchoolPi / 180.0 * std::sin(2.0 * SchoolPi * (284.0 + N) / 365.0);
		const double B = 2.0 * SchoolPi * (N - 81.0) / 364.0;
		const double Eot = 9.87 * std::sin(2.0 * B) - 7.53 * std::cos(B) - 1.5 * std::sin(B);
		const double Lat = P.Latitude * SchoolPi / 180.0;
		const double CosH = (std::sin(-0.833 * SchoolPi / 180.0) - std::sin(Lat) * std::sin(Decl)) / (std::cos(Lat) * std::cos(Decl));
		const double H = std::acos(SchoolClamp(CosH, -1.0, 1.0)) * 12.0 / SchoolPi;
		const double Noon = 12.0 + P.TzHours - P.Longitude / 15.0 - Eot / 60.0;
		FSchoolDaylight Out;
		Out.Sunrise = Noon - H;
		Out.Sunset = Noon + H;
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The water: the areas, the shore, the bed along the launch line

	inline double SchoolSegDist(const FSchoolV2& Pt, const FSchoolV2& A, const FSchoolV2& B, FSchoolV2* Nearest = nullptr)
	{
		const FSchoolV2 Ab = B - A;
		const double T = SchoolClamp((Pt - A).Dot(Ab) / std::max(1e-9, Ab.Dot(Ab)), 0.0, 1.0);
		const FSchoolV2 Q = A + Ab * T;
		if (Nearest) *Nearest = Q;
		return (Pt - Q).Len();
	}

	inline bool SchoolInside(const FSchoolArea& A, const FSchoolV2& Pt)
	{
		bool bIn = false;
		const size_t N = A.Poly.size();
		for (size_t I = 0, J = N - 1; I < N; J = I++)
		{
			const FSchoolV2& U = A.Poly[I];
			const FSchoolV2& V = A.Poly[J];
			if ((U.Y > Pt.Y) != (V.Y > Pt.Y) && Pt.X < (V.X - U.X) * (Pt.Y - U.Y) / (V.Y - U.Y) + U.X)
			{
				bIn = !bIn;
			}
		}
		return bIn;
	}

	/** How far inside the area's outline a point is (negative: outside), m. */
	inline double SchoolEdgeDistance(const FSchoolArea& A, const FSchoolV2& Pt, FSchoolV2* Nearest = nullptr)
	{
		double Best = 1e18;
		const size_t N = A.Poly.size();
		for (size_t I = 0; I < N; I++)
		{
			FSchoolV2 Q;
			const double D = SchoolSegDist(Pt, A.Poly[I], A.Poly[(I + 1) % N], &Q);
			if (D < Best)
			{
				Best = D;
				if (Nearest) *Nearest = Q;
			}
		}
		return SchoolInside(A, Pt) ? Best : -Best;
	}

	inline FSchoolV2 SchoolCentroid(const FSchoolArea& A)
	{
		FSchoolV2 C;
		for (const FSchoolV2& V : A.Poly) C = C + V;
		return A.Poly.empty() ? C : C * (1.0 / (double)A.Poly.size());
	}

	/** The nearest point that is at least Margin inside the area (the point itself if it already is). */
	inline FSchoolV2 SchoolNearestInside(const FSchoolArea& A, const FSchoolV2& Pt, double Margin = 0.0)
	{
		FSchoolV2 Q = Pt, Edge;
		const FSchoolV2 C = SchoolCentroid(A);
		for (int Iter = 0; Iter < 12; Iter++)
		{
			const double D = SchoolEdgeDistance(A, Q, &Edge);
			if (D >= Margin)
			{
				return Q;
			}
			// onto the outline, then inward toward the middle by what is missing
			Q = Edge + (C - Edge).Norm() * (Margin + 0.05 + Iter * 0.5 * std::max(1.0, Margin));
		}
		return C;
	}

	/** In the school's water at all (the corridor, the children's area or the race area). */
	inline bool SchoolInsideWater(const FSchoolParams& P, const FSchoolV2& Pt, std::string* OutArea = nullptr)
	{
		for (const FSchoolArea& A : P.Areas)
		{
			if (SchoolInside(A, Pt))
			{
				if (OutArea) *OutArea = A.Id;
				return true;
			}
		}
		return false;
	}

	/** The nearest point of the school's water (a boat that strayed is sent back to it). */
	inline FSchoolV2 SchoolNearestWater(const FSchoolParams& P, const FSchoolV2& Pt, double Margin = 0.0)
	{
		FSchoolV2 Best = Pt;
		double BestD = 1e18;
		for (const FSchoolArea& A : P.Areas)
		{
			const FSchoolV2 Q = SchoolNearestInside(A, Pt, Margin);
			const double D = (Q - Pt).Len();
			if (D < BestD) { BestD = D; Best = Q; }
		}
		return Best;
	}

	/** X of the high-water line at Y (the data's polyline). */
	inline double SchoolShoreX(const FSchoolParams& P, double Y)
	{
		const auto& S = P.Shore;
		if (Y <= S.front().first) return S.front().second;
		if (Y >= S.back().first) return S.back().second;
		for (size_t I = 1; I < S.size(); I++)
		{
			if (Y <= S[I].first)
			{
				const double T = (Y - S[I - 1].first) / std::max(1e-9, S[I].first - S[I - 1].first);
				return S[I - 1].second + (S[I].second - S[I - 1].second) * T;
			}
		}
		return S.back().second;
	}

	/** Metres out to sea from the high-water line. */
	inline double SchoolOffshore(const FSchoolParams& P, const FSchoolV2& Pt) { return Pt.X - SchoolShoreX(P, Pt.Y); }

	/** The bed under the datum at D metres out (the launch line's profile: a guess anywhere else; negative down). */
	inline double SchoolBedAtD(const FSchoolParams& P, double D)
	{
		const double F = SchoolClamp(D / P.BedStep, 0.0, (double)P.Bed.size() - 1.0);
		const size_t I = std::min((size_t)F, P.Bed.size() - 2);
		return P.Bed[I] + (P.Bed[I + 1] - P.Bed[I]) * (F - (double)I);
	}

	/** The water's depth at a point at a tide, by the launch line's profile (the engine asks ALyudSea instead where it can). */
	inline double SchoolDepthAt(const FSchoolParams& P, const FSchoolV2& Pt, double TideM)
	{
		return TideM - SchoolBedAtD(P, SchoolOffshore(P, Pt));
	}

	/** How far down the centreboard may go: all the way in a metre of water, half under that, none aground. */
	inline double SchoolBoardDown(const FSchoolParams& P, double DepthM)
	{
		return DepthM >= P.BoardFullDepth ? 1.0 : (DepthM >= P.LaunchDepth ? 0.5 : 0.0);
	}

	struct FSchoolLaunch
	{
		bool bOk = false;
		double D = 0.0;             // metres out along the launch line from the high-water line
		FSchoolV2 At;
		double HaulM = 0.0;         // from the slip's foot over the flat
		bool bGuess = true;         // from the data's profile, not the sea's own depth
	};

	inline FSchoolV2 SchoolLaunchLineAt(const FSchoolParams& P, double D) { return P.LaunchOrigin + P.LaunchDir * D; }

	/** Where the trolleys stop: the first point out along the launch line with the launch depth of water and no bar
	 *  shallower than the bar depth beyond it. DepthAtD gives the water's depth D metres out along the line. */
	inline FSchoolLaunch SchoolLaunchPointBy(const FSchoolParams& P, const std::function<double(double)>& DepthAtD, bool bGuess)
	{
		FSchoolLaunch Out;
		Out.bGuess = bGuess;
		const double End = P.BedStep * ((double)P.Bed.size() - 1.0), Step = 1.0;
		double LastBar = -1.0;
		for (double D = End; D >= 0.0; D -= Step)
		{
			if (DepthAtD(D) < P.BarDepth)
			{
				LastBar = D;
				break;
			}
		}
		for (double D = std::max(0.0, LastBar + Step); D <= End; D += Step)
		{
			if (DepthAtD(D) >= P.LaunchDepth)
			{
				Out.bOk = true;
				Out.D = D;
				Out.At = SchoolLaunchLineAt(P, D);
				Out.HaulM = std::max(0.0, D - P.SlipFootD);
				return Out;
			}
		}
		return Out;
	}

	/** The launch point at a tide by the data's bed profile. */
	inline FSchoolLaunch SchoolLaunchPoint(const FSchoolParams& P, double TideM)
	{
		return SchoolLaunchPointBy(P, [&P, TideM](double D) { return TideM - SchoolBedAtD(P, D); }, true);
	}

	/** The launch point by the sea's own depth (the engine: DepthAt = ALyudSea::DepthAt at a world point, metres). */
	inline FSchoolLaunch SchoolLaunchPointSampled(const FSchoolParams& P, const std::function<double(const FSchoolV2&)>& DepthAt)
	{
		return SchoolLaunchPointBy(P, [&P, &DepthAt](double D) { return DepthAt(SchoolLaunchLineAt(P, D)); }, false);
	}

	/** The share of an area with its minimum depth of water at a tide (a 25 m grid, the profile's guess). */
	inline double SchoolAreaAfloat(const FSchoolParams& P, const FSchoolArea& A, double TideM)
	{
		double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18;
		for (const FSchoolV2& V : A.Poly) { X0 = std::min(X0, V.X); X1 = std::max(X1, V.X); Y0 = std::min(Y0, V.Y); Y1 = std::max(Y1, V.Y); }
		int In = 0, Wet = 0;
		const double Step = std::max(8.0, std::min(25.0, std::min(X1 - X0, Y1 - Y0) / 4.0));
		for (double Y = Y0 + Step * 0.5; Y < Y1; Y += Step)
		{
			for (double X = X0 + Step * 0.5; X < X1; X += Step)
			{
				const FSchoolV2 Q(X, Y);
				if (SchoolInside(A, Q))
				{
					In++;
					if (SchoolDepthAt(P, Q, TideM) >= A.MinDepth) Wet++;
				}
			}
		}
		return In ? (double)Wet / (double)In : 0.0;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The day's go / no-go and the signal mast

	/** What the rules need of the weather at a moment (the engine fills it from the weather, the wind, the sea). */
	struct FSchoolWeather
	{
		double Wind = 4.0;          // mean, m/s, over the water
		double Gust = 5.0;          // the gusts' peak, m/s
		double VisibilityM = 20000.0;
		double TideM = 0.0;         // the water's level over the datum (0 high water, negative down)
		double WaterC = 16.0;
		bool bThunder = false;
		bool bDaylight = true;
		bool bSeaIce = false;
	};

	enum class ESchoolGo : uint8_t { Closed, Storm, Shore, Sail };
	enum class ESchoolReason : uint8_t { None, Season, Ice, Dark, Calm, Wind, Gusts, Thunder, Fog, LowWater, DayOff };

	inline const char* SchoolReasonId(ESchoolReason R)
	{
		static const char* const Ids[] = { "none", "season", "ice", "dark", "calm", "wind", "gusts", "thunder", "fog", "low_water", "day_off" };
		return Ids[(int)R];
	}

	struct FSchoolGo
	{
		ESchoolGo Go = ESchoolGo::Closed;
		ESchoolReason Reason = ESchoolReason::None;
		bool ClassOk[SchoolBoats] = { false, false, false };
		ESchoolReason ClassReason[SchoolBoats] = { ESchoolReason::None, ESchoolReason::None, ESchoolReason::None };
		std::string ClassArea[SchoolBoats];     // where each class sails today
		bool bReef = false;                     // luches reef
		bool bTowOut = false;                   // the children are towed out past the dry flat by the launch
		FSchoolLaunch Launch;
	};

	/** One class's wind verdict. */
	inline ESchoolReason SchoolWindVerdict(const FSchoolParams& P, int Class, const FSchoolWeather& W)
	{
		if (W.Wind < P.Wind[Class][0]) return ESchoolReason::Calm;
		if (W.Wind > P.Wind[Class][1]) return ESchoolReason::Wind;
		if (W.Gust > P.Wind[Class][1] + P.GustOver) return ESchoolReason::Gusts;
		return ESchoolReason::None;
	}

	/** The go / no-go for a set of classes at a moment (decided once, at a session's start). Classes: a bit per class. */
	inline FSchoolGo SchoolDecide(const FSchoolParams& P, const FSchoolDate& Date, const FSchoolWeather& W, int Classes = 7)
	{
		FSchoolGo G;
		auto All = [&G, Classes](ESchoolReason R) { for (int C = 0; C < SchoolBoats; C++) if (Classes >> C & 1) G.ClassReason[C] = R; };
		if (SeasonStateOn(P, Date) != ESeasonState::Open) { G.Reason = ESchoolReason::Season; All(G.Reason); return G; }
		if (W.bSeaIce) { G.Reason = ESchoolReason::Ice; All(G.Reason); return G; }
		if (!W.bDaylight) { G.Reason = ESchoolReason::Dark; All(G.Reason); return G; }
		if (W.bThunder) { G.Go = ESchoolGo::Storm; G.Reason = ESchoolReason::Thunder; All(G.Reason); return G; }
		if (W.Wind >= P.StormWind || W.Gust >= P.StormGust) { G.Go = ESchoolGo::Storm; G.Reason = ESchoolReason::Wind; All(G.Reason); return G; }
		G.Go = ESchoolGo::Shore;
		if (W.VisibilityM < P.FogVisibility) { G.Reason = ESchoolReason::Fog; All(G.Reason); return G; }
		G.Launch = SchoolLaunchPoint(P, W.TideM);
		if (!G.Launch.bOk || G.Launch.HaulM > P.MaxHaul) { G.Reason = ESchoolReason::LowWater; All(G.Reason); return G; }
		for (int C = 0; C < SchoolBoats; C++)
		{
			if (!(Classes >> C & 1)) continue;
			ESchoolReason R = SchoolWindVerdict(P, C, W);
			if (R == ESchoolReason::None)
			{
				// the first of the class's areas that is mostly afloat; the children only go out to the deep one in a light wind
				R = ESchoolReason::LowWater;
				for (size_t K = 0; K < P.ClassArea[C].size(); K++)
				{
					const FSchoolArea* A = P.Area(P.ClassArea[C][K]);
					if (!A || SchoolAreaAfloat(P, *A, W.TideM) < 0.5) continue;
					if (C == SchoolOptimist && K > 0 && W.Wind > P.KidsDeepWind) continue;
					G.ClassArea[C] = A->Id;
					if (C == SchoolOptimist && K > 0) G.bTowOut = true;
					R = ESchoolReason::None;
					break;
				}
			}
			G.ClassReason[C] = R;
			G.ClassOk[C] = R == ESchoolReason::None;
			if (G.ClassOk[C]) G.Go = ESchoolGo::Sail;
			else if (G.Reason == ESchoolReason::None) G.Reason = R;
		}
		if (G.Go == ESchoolGo::Sail) G.Reason = ESchoolReason::None;
		G.bReef = G.ClassOk[SchoolLuch] && W.Wind >= P.ReefWind;
		return G;
	}

	/** Afloat: must the boats be recalled now? The wind past the limit and the margin for every class that is out, thunder,
	 *  fog, dark, ice. OutClassRecall: the classes over their own limit (those go in even when the others stay). */
	inline ESchoolReason SchoolMustRecall(const FSchoolParams& P, const FSchoolWeather& W, const FSchoolGo& AtStart, bool* OutClassRecall = nullptr)
	{
		bool bAny = false, bAll = true;
		ESchoolReason Wind = ESchoolReason::Wind;
		for (int C = 0; C < SchoolBoats; C++)
		{
			const bool bOver = W.Wind > P.Wind[C][1] + P.RecallMargin || W.Gust > P.Wind[C][1] + P.GustOver + P.RecallMargin;
			if (OutClassRecall) OutClassRecall[C] = AtStart.ClassOk[C] && bOver;
			if (!AtStart.ClassOk[C]) continue;
			bAny = true;
			if (!bOver) bAll = false;
			else if (W.Wind <= P.Wind[C][1] + P.RecallMargin) Wind = ESchoolReason::Gusts;
		}
		if (W.bThunder) return ESchoolReason::Thunder;
		if (W.bSeaIce) return ESchoolReason::Ice;
		if (!W.bDaylight) return ESchoolReason::Dark;
		if (W.VisibilityM < P.FogVisibility) return ESchoolReason::Fog;
		if (W.Wind >= P.StormWind || W.Gust >= P.StormGust) return ESchoolReason::Wind;
		return bAny && bAll ? Wind : ESchoolReason::None;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Race days of the calendar

	struct FRaceDay
	{
		int Event = -1;             // index into FSchoolParams::Events
		int Day = 0;                // which day of the event (0 first)
		bool bLast = true;          // the event's last day
	};

	/** The regatta that governs a date: the opening, the closing, a dated one or a weekend cup (these take the whole day),
	 *  else the weekday's weekly race (lessons go on beside it). Event -1: none. */
	inline FRaceDay RaceDayOn(const FSchoolParams& P, const FSchoolDate& Date)
	{
		FRaceDay Out;
		if (SeasonStateOn(P, Date) != ESeasonState::Open)
		{
			return Out;
		}
		const long long N = SchoolDayNumber(Date);
		for (int Pass = 0; Pass < 2; Pass++)
		{
			for (size_t I = 0; I < P.Events.size(); I++)
			{
				const FRaceEventDef& E = P.Events[I];
				long long First = -1;
				if (Pass == 0)
				{
					if (E.Rule == "opening") First = SchoolDayNumber(SeasonOpening(P, Date.Y));
					else if (E.Rule == "closing") First = SchoolDayNumber(SeasonClosing(P, Date.Y));
					else if (E.Rule == "date") First = SchoolDayNumber(FSchoolDate(Date.Y, E.Month, E.Day));
					else if (E.Rule == "weekend_on_or_after") First = SchoolDayNumber(SchoolWeekdayOnOrAfter(FSchoolDate(Date.Y, E.Month, E.Day), 5));
					else continue;
				}
				else if (E.Rule == "weekly" && SchoolWeekday(Date) == E.Weekday)
				{
					First = N;
				}
				if (First >= 0 && N >= First && N < First + std::max(1, E.Days))
				{
					Out.Event = (int)I;
					Out.Day = (int)(N - First);
					Out.bLast = Out.Day == std::max(1, E.Days) - 1;
					return Out;
				}
			}
		}
		return Out;
	}

	/** True when the day's regatta takes the whole day (no lessons beside it). */
	inline bool RaceDayIsWholeDay(const FSchoolParams& P, const FRaceDay& D) { return D.Event >= 0 && P.Events[(size_t)D.Event].Rule != "weekly"; }

	// ------------------------------------------------------------------------------------------------------------------
	// The timetable

	struct FTimetablePhase
	{
		std::string Id, Place, Activity;
		double From = 0.0, To = 0.0;
	};

	/** One session of a day as planned: who, when, its parts. */
	struct FTimetableSession
	{
		long long Key = 0;          // the day's number x 16 + the session's place in the day
		std::string Group, Coach, Kind;     // Kind: lesson, theory, race, fitout, layup (shore and recall come from the state)
		std::string Event;                  // the regatta's id on a race session
		int EventIndex = -1, EventDay = 0, Races = 0;
		double From = 0.0, To = 0.0;
		bool bAfloat = false;               // planned on the water
		bool bDark = false;                 // a lesson or race held ashore because the daylight does not hold it
		double WaterFrom = 0.0, WaterTo = 0.0;
		int Classes = 0;                    // a bit per class
		std::vector<FTimetablePhase> Phases;
	};

	/** What the engine remembers of a running session: the decision at its start, the recall. */
	struct FSchoolSessionState
	{
		long long Key = 0;
		ESchoolGo Go = ESchoolGo::Sail;
		ESchoolReason Reason = ESchoolReason::None;
		double RecallHour = -1.0;
	};

	inline std::vector<FTimetablePhase> TimetablePhases(const FSchoolParams& P, const std::string& Kind, double From, double To)
	{
		std::vector<FTimetablePhase> Out;
		const auto It = P.Phases.find(Kind);
		if (It == P.Phases.end())
		{
			return Out;
		}
		double Fixed = 0.0;
		for (const FTimetablePhaseDef& D : It->second) Fixed += D.Minutes / 60.0;
		const double Rest = std::max(0.0, To - From - Fixed);
		double T = From;
		for (const FTimetablePhaseDef& D : It->second)
		{
			FTimetablePhase Ph;
			Ph.Id = D.Id; Ph.Place = D.Place; Ph.Activity = D.Activity;
			Ph.From = T;
			T += D.Minutes > 0.0 ? D.Minutes / 60.0 : Rest;
			Ph.To = std::min(T, To);
			Ph.From = std::min(Ph.From, To);
			Out.push_back(Ph);
		}
		return Out;
	}

	inline int TimetableClassBits(const std::vector<int>& Boats)
	{
		int Bits = 0;
		for (const int C : Boats) if (C >= 0) Bits |= 1 << C;
		return Bits;
	}

	/** Lays an afloat session into the daylight: the boats are ashore the margin before sunset. Too little water time left:
	 *  the session is held ashore (a theory evening) and bDark says why. MinWaterH: what must be left (a race: half an hour). */
	inline void TimetableFitDaylight(const FSchoolParams& P, FTimetableSession& S, const FSchoolDaylight& Light, double MinWaterH)
	{
		S.Phases = TimetablePhases(P, S.Kind, S.From, S.To);
		size_t Wi = S.Phases.size();
		for (size_t I = 0; I < S.Phases.size(); I++) if (S.Phases[I].Id == "water") Wi = I;
		if (Wi >= S.Phases.size())
		{
			return;
		}
		double After = 0.0;         // the return: until the boats are ashore
		if (Wi + 1 < S.Phases.size()) After = S.Phases[Wi + 1].To - S.Phases[Wi + 1].From;
		const double Latest = Light.Sunset - P.DaylightMargin - After;
		const double Earliest = Light.Sunrise + P.DaylightMargin;
		double Wf = S.Phases[Wi].From, Wt = std::min(S.Phases[Wi].To, Latest);
		if (Wf < Earliest || Wt - Wf < MinWaterH)
		{
			S.bAfloat = false;
			S.bDark = true;
			S.Kind = "theory";
			S.Races = 0;
			S.Phases = TimetablePhases(P, "theory", S.From, S.To);
			return;
		}
		const double Cut = S.Phases[Wi].To - Wt;
		S.Phases[Wi].To = Wt;
		for (size_t I = Wi + 1; I < S.Phases.size(); I++) { S.Phases[I].From -= Cut; S.Phases[I].To -= Cut; }
		S.To -= Cut;
		S.bAfloat = true;
		S.WaterFrom = Wf;
		S.WaterTo = Wt;
	}

	/** The day's sessions in time order. bDayOff: a public holiday from the game's calendar (no lessons; regattas go on). */
	inline std::vector<FTimetableSession> SchoolSessionsOn(const FSchoolParams& P, const FSchoolDate& Date, bool bDayOff = false)
	{
		std::vector<FTimetableSession> Out;
		const ESeasonState State = SeasonStateOn(P, Date);
		const int Wd = SchoolWeekday(Date);
		const long long N = SchoolDayNumber(Date);
		const FSchoolDaylight Light = SchoolDaylight(P, Date);
		auto Plain = [&](const std::string& Kind, double From, double To)
		{
			FTimetableSession S;
			S.Group = "all"; S.Coach = Kind == "theory" ? "coach_head" : "bosun"; S.Kind = Kind;
			S.From = From; S.To = To;
			S.Phases = TimetablePhases(P, Kind, From, To);
			Out.push_back(S);
		};
		if (State == ESeasonState::FitOut)
		{
			Plain("fitout", P.FitoutHours[0], P.FitoutHours[1]);
		}
		else if (State == ESeasonState::LayUp)
		{
			Plain("layup", P.FitoutHours[0], P.FitoutHours[1]);
		}
		else if (State == ESeasonState::LaidUp)
		{
			if ((P.TheoryDays >> Wd & 1) && !bDayOff) Plain("theory", P.TheoryHours[0], P.TheoryHours[1]);
		}
		else
		{
			const FRaceDay Rd = RaceDayOn(P, Date);
			if (!RaceDayIsWholeDay(P, Rd) && !bDayOff)
			{
				for (const FTimetableEntry& E : SeasonIsSummer(P, Date) ? P.SummerWeek : P.Term)
				{
					const FSchoolGroup* G = P.Group(E.Group);
					if (!(E.Days >> Wd & 1) || !G) continue;
					FTimetableSession S;
					S.Group = G->Id; S.Coach = G->Coach; S.Kind = "lesson";
					S.From = E.From; S.To = E.From + E.Len;
					S.Classes = TimetableClassBits(G->Boats);
					TimetableFitDaylight(P, S, Light, P.MinWaterH);
					Out.push_back(S);
				}
			}
			if (Rd.Event >= 0)
			{
				const FRaceEventDef& E = P.Events[(size_t)Rd.Event];
				const FSchoolGroup* G = P.Group("race");
				FTimetableSession S;
				S.Group = "race"; S.Coach = G ? G->Coach : "coach_head"; S.Kind = "race";
				S.Event = E.Id; S.EventIndex = Rd.Event; S.EventDay = Rd.Day; S.Races = E.Races;
				S.From = E.From; S.To = E.From + E.Len;
				for (int C = 0; C < SchoolBoats; C++) if (E.Classes[C]) S.Classes |= 1 << C;
				TimetableFitDaylight(P, S, Light, 0.5);
				if (S.bAfloat) S.Races = std::max(1, std::min(E.Races, (int)((S.WaterTo - S.WaterFrom) / 0.5)));
				Out.push_back(S);
				if (E.bLayupAfter && Rd.bLast) Plain("layup", S.To, S.To + 2.0);
			}
		}
		std::stable_sort(Out.begin(), Out.end(), [](const FTimetableSession& A, const FTimetableSession& B) { return A.From < B.From; });
		for (size_t I = 0; I < Out.size(); I++) Out[I].Key = N * 16 + (long long)I;
		return Out;
	}

	/** A session as it really goes: held ashore when the start's decision said so, cut short by a recall. */
	inline FTimetableSession SchoolApplyState(const FSchoolParams& P, const FTimetableSession& Plan, const FSchoolSessionState* State)
	{
		FTimetableSession S = Plan;
		if (!State || !Plan.bAfloat)
		{
			return S;
		}
		if (State->Go != ESchoolGo::Sail)
		{
			S.bAfloat = false;
			S.Kind = "shore";
			S.Phases = TimetablePhases(P, "shore", S.From, S.To);
			return S;
		}
		if (State->RecallHour >= 0.0 && State->RecallHour < Plan.WaterTo)
		{
			const double R = std::max(State->RecallHour, Plan.WaterFrom);
			std::vector<FTimetablePhase> Ph;
			for (const FTimetablePhase& X : Plan.Phases)
			{
				if (X.To <= R) Ph.push_back(X);
				else if (X.From < R) { FTimetablePhase Y = X; Y.To = R; Ph.push_back(Y); }
			}
			for (const FTimetablePhase& X : TimetablePhases(P, "recall", R, std::max(Plan.To, R + 0.75))) Ph.push_back(X);
			S.Phases = Ph;
			S.WaterTo = R;
			S.To = std::max(Plan.To, R + 0.75);
		}
		return S;
	}

	/** The drill of a block of a session's time on the water: one of the group's that suits the wind and the water's warmth,
	 *  the same whenever it is asked for the same day. W null: by the calendar alone (the capsize drill only in summer). */
	inline std::string SchoolDrillFor(const FSchoolParams& P, const FTimetableSession& S, int Block, const FSchoolWeather* W, const FSchoolDate& Date)
	{
		const FSchoolGroup* G = P.Group(S.Group);
		if (!G || G->Drills.empty())
		{
			return std::string();
		}
		std::vector<std::string> Ok;
		for (const std::string& Id : G->Drills)
		{
			const FSchoolDrill* D = P.Drill(Id);
			if (!D) continue;
			if (W && (W->Wind < D->Wind[0] || W->Wind > D->Wind[1])) continue;
			if (D->bWarmOnly && (W ? (W->WaterC < P.CapsizeWaterC || W->Wind > P.CapsizeWind) : !SeasonIsSummer(P, Date))) continue;
			Ok.push_back(Id);
		}
		if (Ok.empty())
		{
			return "free";
		}
		const uint32_t H = SchoolHash((uint32_t)SchoolDayNumber(Date), SchoolHashStr(S.Group));
		// consecutive blocks never repeat: a walk through the list from the day's start
		return Ok[(size_t)((H + (uint32_t)Block) % (uint32_t)Ok.size())];
	}

	/** The shore lesson's topic of a day. */
	inline std::string SchoolShoreTopic(const FSchoolParams& P, const FTimetableSession& S, const FSchoolDate& Date)
	{
		if (P.ShoreTopics.empty()) return std::string();
		return P.ShoreTopics[(size_t)(SchoolHash((uint32_t)SchoolDayNumber(Date), SchoolHashStr(S.Group) ^ 0x51u) % (uint32_t)P.ShoreTopics.size())];
	}

	/** What is going on at the school at a moment. */
	struct FSchoolNow
	{
		bool bOn = false;
		long long Key = 0;
		std::string Group, Coach, Kind, Phase, Place, Activity, Drill, Event, Topic, Area;
		int Block = -1;             // the drill block of the time on the water
		int Race = -1;              // on a race day: which race is on (by the clock)
		double PhaseT = 0.0;        // how far through the phase, 0..1
		bool bAfloat = false;       // the phase is on the water
		FTimetableSession Session;
	};

	/** The session and its phase at Date, Hour. States: the engine's memory of today's sessions (the decision at the start,
	 *  a recall); none: as planned. W: the weather now, for the drill's choice (null: by the calendar). */
	inline FSchoolNow SchoolSessionAt(const FSchoolParams& P, const FSchoolDate& Date, double Hour, bool bDayOff = false,
		const std::vector<FSchoolSessionState>* States = nullptr, const FSchoolWeather* W = nullptr)
	{
		FSchoolNow Now;
		for (const FTimetableSession& Plan : SchoolSessionsOn(P, Date, bDayOff))
		{
			const FSchoolSessionState* St = nullptr;
			if (States) for (const FSchoolSessionState& X : *States) if (X.Key == Plan.Key) St = &X;
			const FTimetableSession S = SchoolApplyState(P, Plan, St);
			if (Hour < S.From || Hour >= S.To) continue;
			Now.bOn = true;
			Now.Key = S.Key; Now.Group = S.Group; Now.Coach = S.Coach; Now.Kind = S.Kind; Now.Event = S.Event;
			Now.Session = S;
			for (const FTimetablePhase& Ph : S.Phases)
			{
				if (Hour < Ph.From || Hour >= Ph.To) continue;
				Now.Phase = Ph.Id; Now.Place = Ph.Place; Now.Activity = Ph.Activity;
				Now.PhaseT = (Hour - Ph.From) / std::max(1e-6, Ph.To - Ph.From);
				if (Ph.Id == "water")
				{
					Now.bAfloat = true;
					const auto It = P.GroupArea.find(S.Group);
					Now.Area = It != P.GroupArea.end() && !It->second.empty() ? It->second[0] : std::string("race");
					Now.Place = "water:" + Now.Area;
					if (S.Kind == "race")
					{
						Now.Race = std::min(std::max(1, S.Races) - 1, (int)(Now.PhaseT * std::max(1, S.Races)));
					}
					else
					{
						Now.Block = std::min(P.DrillBlocks - 1, (int)(Now.PhaseT * P.DrillBlocks));
						Now.Drill = SchoolDrillFor(P, S, Now.Block, W, Date);
						const FSchoolDrill* D = P.Drill(Now.Drill);
						if (D && D->Area != "group" && P.Area(D->Area)) Now.Area = D->Area;
						Now.Place = "water:" + Now.Area;
					}
				}
				else if (Ph.Id == "shore" || Ph.Id == "theory")
				{
					Now.Topic = SchoolShoreTopic(P, S, Date);
				}
				break;
			}
			return Now;
		}
		return Now;
	}

	/** The people's conditions the engine sets (the contract's names in the comments). */
	struct FSchoolConditions
	{
		bool bSeason = false;       // sail_season: the boats are out of the boathouse, the water is open
		bool bKids = false;         // sail_kids: the children's session is on
		bool bJuniors = false;      // sail_juniors
		bool bAdults = false;       // sail_adults
		bool bRace = false;         // sail_race: a regatta is on (everyone)
		bool bShore = false;        // sail_shore: the session now is held ashore
		bool bFitOut = false;       // not in the contract: the fitting-out fortnight or the laying up (the school's own crowd)
	};

	inline FSchoolConditions SchoolConditionsAt(const FSchoolParams& P, const FSchoolDate& Date, double Hour, bool bDayOff = false,
		bool bSeaIce = false, const std::vector<FSchoolSessionState>* States = nullptr)
	{
		FSchoolConditions C;
		C.bSeason = SeasonOpen(P, Date, bSeaIce);
		const FSchoolNow Now = SchoolSessionAt(P, Date, Hour, bDayOff, States);
		if (!Now.bOn)
		{
			return C;
		}
		C.bKids = Now.Group == "kids";
		C.bJuniors = Now.Group == "juniors";
		C.bAdults = Now.Group == "adults";
		C.bRace = Now.Kind == "race";
		C.bFitOut = Now.Kind == "fitout" || Now.Kind == "layup";
		C.bShore = Now.Kind == "shore" || Now.Kind == "theory" || C.bFitOut || (bSeaIce && !C.bFitOut);
		return C;
	}

	/** The hero may hire a boat now: in season, in the hire hours, luches may sail, no regatta on, the luches not out with a group. */
	inline bool SchoolHireOpen(const FSchoolParams& P, const FSchoolDate& Date, double Hour, const FSchoolWeather& W, bool bDayOff = false,
		ESchoolReason* OutWhy = nullptr)
	{
		const FSchoolGo G = SchoolDecide(P, Date, W, 1 << P.HeroClass);
		if (OutWhy) *OutWhy = G.Reason;
		if (G.Go != ESchoolGo::Sail)
		{
			return false;
		}
		if (Hour < P.HireFrom || Hour > SchoolDaylight(P, Date).Sunset - P.HireBeforeSunset)
		{
			if (OutWhy) *OutWhy = ESchoolReason::Dark;
			return false;
		}
		const FSchoolNow Now = SchoolSessionAt(P, Date, Hour, bDayOff);
		if (Now.bOn && (Now.Kind == "race" || (Now.Session.Classes >> P.HeroClass & 1)))
		{
			if (OutWhy) *OutWhy = ESchoolReason::DayOff;
			return false;
		}
		return true;
	}

	struct FSchoolSignal
	{
		std::string State;          // closed, fitout, shore, wait, sail, race, recall, storm
		std::vector<std::string> Flags;     // top first
		int Horn = 0;               // blasts when the state is entered
		ESchoolReason Reason = ESchoolReason::None;
	};

	/** What the signal mast shows at a moment. */
	inline FSchoolSignal SchoolSignalAt(const FSchoolParams& P, const FSchoolDate& Date, double Hour, const FSchoolWeather& W,
		bool bDayOff = false, const std::vector<FSchoolSessionState>* States = nullptr)
	{
		FSchoolSignal Out;
		int ClassBits = 0;
		const ESeasonState Season = SeasonStateOn(P, Date);
		const FSchoolNow Now = SchoolSessionAt(P, Date, Hour, bDayOff, States);
		const FSchoolSessionState* St = nullptr;
		if (States && Now.bOn) for (const FSchoolSessionState& X : *States) if (X.Key == Now.Key) St = &X;
		Out.State = "closed";
		if (Season != ESeasonState::Open)
		{
			Out.Reason = ESchoolReason::Season;
			if (Now.bOn && Now.Kind != "theory") Out.State = "fitout";
		}
		else if (W.bSeaIce) Out.Reason = ESchoolReason::Ice;
		else if (!W.bDaylight) Out.Reason = ESchoolReason::Dark;
		else
		{
			const int Classes = Now.bOn && Now.Session.Classes ? Now.Session.Classes : 1 << P.HeroClass;
			const FSchoolGo G = SchoolDecide(P, Date, W, Classes);
			Out.Reason = G.Reason;
			if (G.Go == ESchoolGo::Storm) Out.State = "storm";
			else if (St && St->RecallHour >= 0.0 && Hour >= St->RecallHour) { Out.State = "recall"; Out.Reason = St->Reason; }
			else if (St && St->Go != ESchoolGo::Sail) { Out.State = St->Reason == ESchoolReason::Calm || St->Reason == ESchoolReason::LowWater ? "wait" : "shore"; Out.Reason = St->Reason; }
			else if (Now.bOn && !Now.Session.bAfloat && !St) Out.State = "shore";
			else if (!Now.bOn && (Hour < P.HireFrom || Hour > SchoolDaylight(P, Date).Sunset - P.HireBeforeSunset)) Out.Reason = ESchoolReason::None;
			else if (G.Go == ESchoolGo::Sail)
			{
				Out.State = Now.bOn && Now.Kind == "race" ? "race" : "sail";
				for (int C = 0; C < SchoolBoats; C++) if (G.ClassOk[C]) ClassBits |= 1 << C;
			}
			else Out.State = G.Reason == ESchoolReason::Calm || G.Reason == ESchoolReason::LowWater ? "wait" : "shore";
		}
		const auto It = P.Signals.find(Out.State);
		if (It != P.Signals.end())
		{
			Out.Flags = It->second.Flags;
			Out.Horn = It->second.Horn;
			for (int C = 0; C < SchoolBoats; C++) if ((ClassBits >> C & 1) && !It->second.ClassFlag[C].empty()) Out.Flags.push_back(It->second.ClassFlag[C]);
		}
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The coach's launch: whom it goes to

	struct FSchoolBoatState
	{
		FSchoolV2 Pos;
		bool bCapsized = false;
		double CapsizedS = 0.0;     // how long it has lain over
		bool bKid = false;          // an optimist
		bool bAshore = false;
	};

	struct FSchoolLaunchTask
	{
		enum EKind : uint8_t { Station, Right, Tow, Shepherd } Kind = Station;
		int Boat = -1;
		FSchoolV2 Target;
	};

	/** The launch's errand now: a boat capsized too long is righted (a child's sooner; on a recall a capsized child is towed
	 *  in), a boat strayed out of the water's bounds is shepherded back, on a recall the farthest child is towed home; else
	 *  it keeps its station. */
	inline FSchoolLaunchTask SchoolLaunchTask(const FSchoolParams& P, const std::vector<FSchoolBoatState>& Boats, const FSchoolV2& LaunchPos,
		const FSchoolV2& Station, bool bRecall, const FSchoolV2& Home)
	{
		FSchoolLaunchTask T;
		T.Target = Station;
		double Best = 1e18;
		for (size_t I = 0; I < Boats.size(); I++)
		{
			const FSchoolBoatState& B = Boats[I];
			if (B.bAshore || !B.bCapsized || B.CapsizedS < (B.bKid ? P.RescueKidsS : P.RescueS)) continue;
			const double D = (B.Pos - LaunchPos).Len() - (B.bKid ? 1000.0 : 0.0);      // children first
			if (D < Best) { Best = D; T.Kind = bRecall && B.bKid ? FSchoolLaunchTask::Tow : FSchoolLaunchTask::Right; T.Boat = (int)I; T.Target = B.Pos; }
		}
		if (T.Boat >= 0)
		{
			return T;
		}
		Best = P.StrayM;
		for (size_t I = 0; I < Boats.size(); I++)
		{
			const FSchoolBoatState& B = Boats[I];
			if (B.bAshore || SchoolInsideWater(P, B.Pos)) continue;
			const double D = (SchoolNearestWater(P, B.Pos) - B.Pos).Len();
			if (D > Best) { Best = D; T.Kind = FSchoolLaunchTask::Shepherd; T.Boat = (int)I; T.Target = B.Pos; }
		}
		if (T.Boat >= 0 || !bRecall)
		{
			return T;
		}
		Best = P.StrayM;
		for (size_t I = 0; I < Boats.size(); I++)
		{
			const FSchoolBoatState& B = Boats[I];
			if (B.bAshore || !B.bKid) continue;
			const double D = (B.Pos - Home).Len();
			if (D > Best) { Best = D; T.Kind = FSchoolLaunchTask::Tow; T.Boat = (int)I; T.Target = B.Pos; }
		}
		if (T.Boat < 0) T.Target = Home;
		return T;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Courses

	enum class ERaceCourse : uint8_t { WindwardLeeward, Triangle, KidsTriangle, Figure8, StartLine };

	inline ERaceCourse RaceCourseKind(const std::string& Id)
	{
		return Id == "triangle" ? ERaceCourse::Triangle : Id == "kids_triangle" ? ERaceCourse::KidsTriangle : Id == "figure8" ? ERaceCourse::Figure8
			: Id == "start_line" ? ERaceCourse::StartLine : ERaceCourse::WindwardLeeward;
	}

	struct FRaceMark
	{
		std::string Id;             // 1 windward, 2, 3; a, b of a figure-eight
		FSchoolV2 At;
	};

	struct FRaceCourse
	{
		bool bOk = false;
		ERaceCourse Kind = ERaceCourse::WindwardLeeward;
		int Class = SchoolLuch;
		std::string Area;
		FSchoolV2 WindTo, Up;       // Up = -WindTo
		bool bLine = false;         // has a start / finish line (the launch at the starboard end, the pin at the port end)
		FSchoolV2 LineBoat, LinePin;
		std::vector<FRaceMark> Marks;
		std::vector<int> Order;     // the marks to round, in order (indices into Marks)
		std::vector<int> Side;      // for each of Order: +1 leave it to port, -1 to starboard
		double LegM = 0.0, LengthM = 0.0, Scale = 1.0;
		FSchoolV2 Middle;

		FSchoolV2 LineMid() const { return (LineBoat + LinePin) * 0.5; }
		/** The point before / after the K-th rounding (the line's middle at the ends). */
		FSchoolV2 Waypoint(int K) const
		{
			if (K < 0 || K >= (int)Order.size()) return bLine ? LineMid() : Marks[(size_t)Order[(size_t)(K < 0 ? Order.size() - 1 : 0)]].At;
			return Marks[(size_t)Order[(size_t)K]].At;
		}
	};

	/** The course's shape round the origin: X = metres to port (left looking upwind), Y = metres upwind. */
	inline void RaceCourseShape(const FSchoolParams& P, ERaceCourse Kind, double Leg, double LineLen, int LapsWanted, std::vector<FSchoolV2>& Marks,
		std::vector<int>& Order, std::vector<int>& Side, bool& bLine, FSchoolV2& Boat, FSchoolV2& Pin)
	{
		Marks.clear(); Order.clear(); Side.clear();
		bLine = Kind != ERaceCourse::Figure8;
		Boat = FSchoolV2(-LineLen * 0.5, 0.0);
		Pin = FSchoolV2(LineLen * 0.5, 0.0);
		const double Gap = std::min(P.LeewardGap, Leg * 0.25);
		if (Kind == ERaceCourse::WindwardLeeward)
		{
			Marks = { FSchoolV2(0.0, Leg), FSchoolV2(0.0, Gap) };
			Order.push_back(0);
			for (int L = 1; L < (LapsWanted > 0 ? LapsWanted : P.LapsWL); L++) { Order.push_back(1); Order.push_back(0); }
		}
		else if (Kind == ERaceCourse::Triangle || Kind == ERaceCourse::KidsTriangle)
		{
			const double Half = (Leg - Gap) * 0.5;
			Marks = { FSchoolV2(0.0, Leg), FSchoolV2(Half * std::tan(P.WingDeg * SchoolPi / 180.0), Gap + Half), FSchoolV2(0.0, Gap) };
			Order.push_back(0);
			const int Laps = LapsWanted > 0 ? LapsWanted : (Kind == ERaceCourse::Triangle ? P.LapsTriangle : P.LapsKids);
			for (int L = 0; L < Laps; L++) { Order.push_back(1); Order.push_back(2); Order.push_back(0); }
		}
		else if (Kind == ERaceCourse::Figure8)
		{
			Marks = { FSchoolV2(-P.Figure8Gap * 0.5, 0.0), FSchoolV2(P.Figure8Gap * 0.5, 0.0) };
		}
		for (size_t I = 0; I < Order.size(); I++) Side.push_back(1);
	}

	/** The laps of a figure-eight (the lesson and the drill say how many): mark a to port, mark b to starboard, in turn. */
	inline void RaceSetFigure8Laps(FRaceCourse& C, int Laps)
	{
		C.Order.clear(); C.Side.clear();
		for (int L = 0; L < Laps && C.Marks.size() >= 2; L++) { C.Order.push_back(0); C.Side.push_back(1); C.Order.push_back(1); C.Side.push_back(-1); }
	}

	/** Sets a course for a class from the wind at the time: square to it, sized by the class and the wind, shrunk and moved
	 *  until every mark and both ends of the line lie the margin inside the class's area with the area's depth of water under
	 *  them at this tide. Seed: the day and the race (RaceSeed), so a race's course is the same whenever it is asked for.
	 *  AreaId: a given area instead of the class's own list. Laps: 0 the data's (a windward-leeward's laps are its beats).
	 *  bOk false: no room or no water (no race). */
	inline FRaceCourse RaceSetCourse(const FSchoolParams& P, ERaceCourse Kind, int Class, const FSchoolV2& WindTo, double WindSpeed, int Boats,
		uint32_t Seed, double TideM, const std::string& AreaId = std::string(), int Laps = 0)
	{
		FRaceCourse C;
		C.Kind = Kind; C.Class = Class;
		C.WindTo = WindTo.Norm();
		if (C.WindTo.Len() < 0.5) C.WindTo = FSchoolV2(-1.0, 0.0);
		C.Up = C.WindTo * -1.0;
		const FSchoolV2 Port = C.Up.Left();
		const double Factor = SchoolClamp(P.CourseScale[0] + P.CourseScale[1] * WindSpeed / std::max(0.1, P.WindRef), P.LegClamp[0], P.LegClamp[1]);
		const double Leg0 = Kind == ERaceCourse::KidsTriangle ? P.KidsLeg * Factor : P.LegM[Class] * Factor;
		const double Line0 = Kind == ERaceCourse::StartLine ? P.PracticeLine : std::max(P.LineMin, P.LinePerBoat * P.Loa[Class] * std::max(1, Boats));
		std::vector<std::string> Try = AreaId.empty() ? P.ClassArea[Class] : std::vector<std::string>{ AreaId };
		for (const std::string& Aid : Try)
		{
			const FSchoolArea* A = P.Area(Aid);
			if (!A) continue;
			double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18;
			for (const FSchoolV2& V : A->Poly) { X0 = std::min(X0, V.X); X1 = std::max(X1, V.X); Y0 = std::min(Y0, V.Y); Y1 = std::max(Y1, V.Y); }
			const FSchoolV2 Mid = SchoolCentroid(*A);
			for (double Scale = 1.0; Leg0 * Scale >= P.MinLeg * 0.999 || Scale == 1.0; Scale *= P.CourseShrink)
			{
				std::vector<FSchoolV2> Shape;
				bool bLine = false;
				FSchoolV2 Boat, Pin;
				RaceCourseShape(P, Kind, Leg0 * Scale, std::min(Line0, Leg0 * Scale * 0.8 + P.LineMin * 0.5), Laps, Shape, C.Order, C.Side, bLine, Boat, Pin);
				std::vector<FSchoolV2> Key = Shape;
				if (bLine) { Key.push_back(Boat); Key.push_back(Pin); }
				FSchoolV2 Mean;
				for (const FSchoolV2& K : Key) Mean = Mean + K;
				Mean = Mean * (1.0 / (double)std::max<size_t>(1, Key.size()));
				FSchoolRng Rng(SchoolHash(Seed, SchoolHashStr(Aid)));
				for (int Cand = 0; Cand < 40; Cand++)
				{
					// the area's middle first, then the day's own spots, then a grid of others
					FSchoolV2 At = Mid;
					if (Cand > 0)
					{
						const double R = Cand < 8 ? 0.25 : 0.48;
						At = FSchoolV2(Mid.X + (Rng.U() * 2.0 - 1.0) * (X1 - X0) * R, Mid.Y + (Rng.U() * 2.0 - 1.0) * (Y1 - Y0) * R);
					}
					bool bFit = true;
					for (const FSchoolV2& K : Key)
					{
						const FSchoolV2 Wp = At + Port * (K.X - Mean.X) + C.Up * (K.Y - Mean.Y);
						if (SchoolEdgeDistance(*A, Wp) < P.CourseMargin || SchoolDepthAt(P, Wp, TideM) < A->MinDepth) { bFit = false; break; }
					}
					if (!bFit) continue;
					auto World = [&](const FSchoolV2& K) { return At + Port * (K.X - Mean.X) + C.Up * (K.Y - Mean.Y); };
					C.Marks.clear();
					static const char* const Names3[] = { "1", "2", "3" };
					for (size_t I = 0; I < Shape.size(); I++)
					{
						FRaceMark M;
						M.Id = Kind == ERaceCourse::Figure8 ? (I == 0 ? "a" : "b") : Names3[std::min<size_t>(I, 2)];
						M.At = World(Shape[I]);
						C.Marks.push_back(M);
					}
					C.bLine = bLine;
					C.LineBoat = World(Boat); C.LinePin = World(Pin);
					C.Area = Aid; C.Scale = Scale; C.LegM = Leg0 * Scale; C.Middle = At;
					if (Kind == ERaceCourse::Figure8) RaceSetFigure8Laps(C, Laps > 0 ? Laps : 2);
					C.LengthM = 0.0;
					for (int K = 0; K <= (int)C.Order.size() && !C.Order.empty(); K++) C.LengthM += (C.Waypoint(K) - C.Waypoint(K - 1)).Len();
					C.bOk = true;
					return C;
				}
				if (Leg0 * Scale < P.MinLeg) break;
			}
		}
		C.Order.clear(); C.Side.clear();
		return C;
	}

	inline uint32_t RaceSeed(const FSchoolDate& Date, int Event, int Race, int Class)
	{
		return SchoolHash((uint32_t)SchoolDayNumber(Date), (uint32_t)(Event * 97 + Race * 13 + Class + 1));
	}

	/** Where the coach's launch lies for a drill set round Middle (the drill's station: metres upwind and to the right). */
	inline FSchoolV2 SchoolDrillStation(const FSchoolDrill& D, const FSchoolV2& Middle, const FSchoolV2& WindTo)
	{
		const FSchoolV2 Up = WindTo.Norm() * -1.0;
		return Middle + Up * D.Station[0] + Up.Left() * -D.Station[1];
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The start's signals

	struct FRaceSignal
	{
		double T = 0.0;             // seconds from the gun (negative: before it)
		std::string Id;             // warning, prep, go
		std::string Flag;           // the flag that moves on the launch's staff
		bool bUp = true;
		int Horns = 1;
		bool bLong = false;
	};

	/** The start sequence of a class: 3 minutes (the class flag up, a horn), 1 minute (flag P up, a long horn), the gun
	 *  (both down, a horn). Every flag move is an entry; the horn sounds with the first entry of each time. */
	inline std::vector<FRaceSignal> RaceStartSequence(const FSchoolParams& P, int Class)
	{
		auto Flag = [&P](const char* K) { const auto It = P.StartFlags.find(K); return It != P.StartFlags.end() ? It->second : std::string(); };
		const auto Sg = P.Signals.find("race");
		std::string ClassFlag = Sg != P.Signals.end() ? Sg->second.ClassFlag[Class] : std::string();
		if (ClassFlag.empty()) ClassFlag = Flag("warning");
		std::vector<FRaceSignal> Out;
		FRaceSignal S;
		S.T = -P.WarningS; S.Id = "warning"; S.Flag = ClassFlag; S.bUp = true; S.Horns = 1; S.bLong = false; Out.push_back(S);
		S.T = -P.PrepS; S.Id = "prep"; S.Flag = Flag("prep"); S.bUp = true; S.Horns = 1; S.bLong = true; Out.push_back(S);
		S.T = 0.0; S.Id = "go"; S.Flag = ClassFlag; S.bUp = false; S.Horns = 1; S.bLong = false; Out.push_back(S);
		S.T = 0.0; S.Id = "go"; S.Flag = Flag("prep"); S.bUp = false; S.Horns = 0; Out.push_back(S);
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// A boat's race, read from its track

	enum class ERaceEvent : uint8_t { None, Started, Ocs, OcsCleared, Rounded, WrongSide, Missed, Touch, PenaltyDone, Finished, FinishRefused, Dns };
	enum class ERaceStatus : uint8_t { Waiting, Racing, Finished, Dnf, Dns, Dsq, Ocs };

	inline const char* RaceStatusId(ERaceStatus S)
	{
		static const char* const Ids[] = { "waiting", "racing", "finished", "dnf", "dns", "dsq", "ocs" };
		return Ids[(int)S];
	}

	/** Does the way from A to B cross the segment from C to D? T: where along C-D (0..1). */
	inline bool RaceCrosses(const FSchoolV2& A, const FSchoolV2& B, const FSchoolV2& C, const FSchoolV2& D, double* OutT = nullptr)
	{
		// by the side of the line each end is on: a position exactly on the line belongs to one side, so the step that ends
		// there and the step that leaves from there count once between them
		const FSchoolV2 S = D - C;
		const double Sa = S.Cross(A - C), Sb = S.Cross(B - C);
		if ((Sa > 0.0) == (Sb > 0.0) || S.Dot(S) < 1e-12)
		{
			return false;
		}
		const FSchoolV2 X = A + (B - A) * (Sa / (Sa - Sb));
		const double U = (X - C).Dot(S) / S.Dot(S);
		if (OutT) *OutT = U;
		return U >= 0.0 && U <= 1.0;
	}

	/** Follows one boat round a course from its positions: the start (over early, back, started), each mark in order on its
	 *  side, a touch and its penalty turn, the finish. Feed Step every tick with the time from the gun (negative before it). */
	struct FRaceTracker
	{
		ERaceStatus Status = ERaceStatus::Waiting;
		bool bHasPrev = false, bGun = false, bOcs = false, bStarted = false, bFreeStart = false;
		FSchoolV2 Prev;
		double PrevHeading = 0.0;
		int Next = 0;               // the next rounding (index into the course's Order)
		int Wind = 0;               // the windings round the next mark (a wrong-way pass must be unwound)
		double StartT = -1.0, FinishT = -1.0;
		int Touches = 0, PenaltiesOwed = 0, WrongSides = 0;
		bool bTouching = false, bMissedSaid = false, bRefusedSaid = false, bDnsSaid = false, bBeenNear = false;
		double PenAccum = 0.0, PenMin = 0.0, PenMax = 0.0, PenT0 = 0.0;    // the heading's run since the touch, its lowest and highest

		void Reset() { *this = FRaceTracker(); }
		/** A lesson's lap without a start: the boat is racing from the first step. */
		void ResetFree() { Reset(); bFreeStart = true; }

		static FSchoolV2 OutRay(const FRaceCourse& C, int K)
		{
			const FSchoolV2 M = C.Waypoint(K);
			const FSchoolV2 A = (C.Waypoint(K - 1) - M).Norm(), B = (C.Waypoint(K + 1) - M).Norm();
			const FSchoolV2 Sum = A + B;
			if (Sum.Len() > 0.2)
			{
				return Sum.Norm() * -1.0;
			}
			// straight through (or a first / last mark with nothing either side): the side it is left on says where the boat passes
			const FSchoolV2 Way = (B - A).Norm();
			return C.Side[(size_t)K] > 0 ? Way.Left() * -1.0 : Way.Left();
		}

		/** One step. Events: at most a few; read them all. */
		std::vector<ERaceEvent> Step(const FSchoolParams& P, const FRaceCourse& C, double T, const FSchoolV2& Pos, double HeadingDeg)
		{
			std::vector<ERaceEvent> Ev;
			if (Status == ERaceStatus::Finished || Status == ERaceStatus::Dns || Status == ERaceStatus::Dsq || Status == ERaceStatus::Dnf)
			{
				return Ev;
			}
			if (!bHasPrev)
			{
				bHasPrev = true;
				Prev = Pos;
				PrevHeading = HeadingDeg;
			}
			const FSchoolV2 Mid = C.bLine ? C.LineMid() : FSchoolV2();
			const double SideNow = C.bLine ? (Pos - Mid).Dot(C.Up) : 0.0, SideWas = C.bLine ? (Prev - Mid).Dot(C.Up) : 0.0;
			if ((bFreeStart || !C.bLine) && !bStarted)
			{
				bStarted = true;
				bGun = true;
				StartT = T;
				Status = ERaceStatus::Racing;
			}
			if (T >= 0.0 && !bGun)
			{
				// the gun: anyone on the course side between the ends (and a little past them) is over
				bGun = true;
				const FSchoolV2 Along = (C.LinePin - C.LineBoat).Norm();
				const double U = (Pos - C.LineBoat).Dot(Along), Len = (C.LinePin - C.LineBoat).Len();
				if (SideNow > 0.0 && U > -5.0 && U < Len + 5.0)
				{
					bOcs = true;
					Status = ERaceStatus::Ocs;
					Ev.push_back(ERaceEvent::Ocs);
				}
			}
			if (bGun && !bStarted)
			{
				if (bOcs && SideNow < -0.5)
				{
					bOcs = false;
					Status = ERaceStatus::Waiting;
					Ev.push_back(ERaceEvent::OcsCleared);
				}
				else if (!bOcs && SideWas <= 0.0 && SideNow > 0.0 && RaceCrosses(Prev, Pos, C.LineBoat, C.LinePin))
				{
					bStarted = true;
					StartT = T;
					Status = ERaceStatus::Racing;
					Ev.push_back(ERaceEvent::Started);
				}
				else if (!bOcs && T > P.StartWindowS && !bDnsSaid)
				{
					bDnsSaid = true;
					Status = ERaceStatus::Dns;
					Ev.push_back(ERaceEvent::Dns);
				}
			}
			if (bStarted)
			{
				// a touch of any mark: one turn owed
				bool bNear = false;
				for (const FRaceMark& M : C.Marks)
				{
					const double D = (Pos - M.At).Len();
					if (D < P.TouchM && !bTouching)
					{
						bTouching = true;
						Touches++;
						PenaltiesOwed++;
						PenAccum = PenMin = PenMax = 0.0;
						PenT0 = T;
						Ev.push_back(ERaceEvent::Touch);
					}
					if (D < P.TouchM * 2.5) bNear = true;
				}
				if (!bNear) bTouching = false;
				if (PenaltiesOwed > 0)
				{
					double Dh = std::fmod(HeadingDeg - PrevHeading + 540.0, 360.0) - 180.0;
					if (Dh < -180.0) Dh += 360.0;
					if (T - PenT0 > P.PenaltyWindowS) { PenAccum = PenMin = PenMax = 0.0; PenT0 = T; }
					PenAccum += Dh;
					PenMin = std::min(PenMin, PenAccum);
					PenMax = std::max(PenMax, PenAccum);
					// a full turn one way: the heading's run spans it (the rounding's own turn the other way does not undo it)
					if (PenMax - PenMin >= P.PenaltyTurnDeg)
					{
						PenaltiesOwed--;
						PenAccum = PenMin = PenMax = 0.0;
						PenT0 = T;
						Ev.push_back(ERaceEvent::PenaltyDone);
					}
				}
				if (Next < (int)C.Order.size())
				{
					const FSchoolV2 M = C.Waypoint(Next);
					const FSchoolV2 Out = OutRay(C, Next);
					const int Want = C.Side[(size_t)Next];
					if ((Pos - M).Len() < P.ZoneM) bBeenNear = true;
					if (RaceCrosses(Prev, Pos, M + Out * (P.TouchM * 0.5), M + Out * P.ZoneM))
					{
						// anticlockwise on the map (the mark to port) is a negative turn in this mirrored frame
						const int Dir = (Prev - M).Cross(Pos - M) < 0.0 ? 1 : -1;
						Wind += Dir == Want ? 1 : -1;
						if (Wind >= 1)
						{
							Next++;
							Wind = 0;
							bMissedSaid = false;
							bBeenNear = false;
							Ev.push_back(ERaceEvent::Rounded);
						}
						else if (Dir != Want)
						{
							WrongSides++;
							Ev.push_back(ERaceEvent::WrongSide);
						}
					}
					else if (Next + 1 < (int)C.Order.size() && !bMissedSaid && bBeenNear)
					{
						// came by this one, did not round it, and is rounding the one after
						const FSchoolV2 M2 = C.Waypoint(Next + 1);
						const FSchoolV2 Out2 = OutRay(C, Next + 1);
						if (C.Order[(size_t)(Next + 1)] != C.Order[(size_t)Next] && RaceCrosses(Prev, Pos, M2 + Out2 * (P.TouchM * 0.5), M2 + Out2 * P.ZoneM))
						{
							bMissedSaid = true;
							Ev.push_back(ERaceEvent::Missed);
						}
					}
				}
				const bool bCrossedLine = C.bLine && SideWas > 0.0 && SideNow <= 0.0 && RaceCrosses(Prev, Pos, C.LineBoat, C.LinePin);
				if (Next >= (int)C.Order.size() && (bCrossedLine || !C.bLine))
				{
					FinishT = T;
					Status = PenaltiesOwed > 0 ? ERaceStatus::Dsq : ERaceStatus::Finished;
					Ev.push_back(ERaceEvent::Finished);
				}
				else if (bCrossedLine && (Next > 0 || WrongSides > 0 || T - StartT > P.StartWindowS) && !bRefusedSaid)     // not a dip back at the start
				{
					bRefusedSaid = true;
					Ev.push_back(ERaceEvent::FinishRefused);
				}
				else if (!bCrossedLine)
				{
					bRefusedSaid = false;
				}
			}
			Prev = Pos;
			PrevHeading = HeadingDeg;
			return Ev;
		}

		/** The race is over (the time limit or the finish window ran out): what the boat is scored. */
		ERaceStatus Close()
		{
			if (Status == ERaceStatus::Waiting) Status = ERaceStatus::Dns;
			else if (Status == ERaceStatus::Racing) Status = ERaceStatus::Dnf;
			return Status;
		}

		double StartDelay() const { return bStarted ? StartT : -1.0; }
		/** The next mark to round (-1: the finish). */
		int NextMark(const FRaceCourse& C) const { return Next < (int)C.Order.size() ? C.Order[(size_t)Next] : -1; }
	};

	/** A general recall: more than the given share of the fleet was over at the gun. */
	inline bool RaceGeneralRecall(const FSchoolParams& P, int Over, int Fleet) { return Fleet > 0 && (double)Over > P.GeneralRecallFrac * (double)Fleet; }

	// ------------------------------------------------------------------------------------------------------------------
	// Scoring

	struct FRaceFinish
	{
		std::string Slot;           // a sailor's slot; "hero" the hero
		ERaceStatus Status = ERaceStatus::Finished;
		double TimeS = 0.0;
		double Points = 0.0;
		int Place = 0;              // 1.. for finishers (ties share the better number); 0 not placed
	};

	/** Low point: finishers in time order score their place, boats level to a tenth of a second share the places' mean;
	 *  DNF, DNS, DSQ and OCS score the entries + 1. Sorts R by points. Entries 0: the number of boats in R. */
	inline void RaceScore(std::vector<FRaceFinish>& R, int Entries = 0)
	{
		const int N = Entries > 0 ? Entries : (int)R.size();
		std::stable_sort(R.begin(), R.end(), [](const FRaceFinish& A, const FRaceFinish& B)
		{
			const bool Fa = A.Status == ERaceStatus::Finished, Fb = B.Status == ERaceStatus::Finished;
			return Fa != Fb ? Fa : (Fa && A.TimeS < B.TimeS);
		});
		size_t I = 0;
		while (I < R.size())
		{
			if (R[I].Status != ERaceStatus::Finished)
			{
				R[I].Points = N + 1.0;
				R[I].Place = 0;
				I++;
				continue;
			}
			size_t J = I + 1;
			while (J < R.size() && R[J].Status == ERaceStatus::Finished && std::fabs(R[J].TimeS - R[I].TimeS) < 0.1) J++;
			const double Mean = ((double)(I + 1) + (double)J) * 0.5;
			for (size_t K = I; K < J; K++) { R[K].Points = Mean; R[K].Place = (int)I + 1; }
			I = J;
		}
	}

	/** How many of a boat's worst scores a series of Races drops. */
	inline int RaceDiscards(const FSchoolParams& P, int Races)
	{
		int D = 0;
		for (const auto& X : P.Discards) if (Races >= X.first) D = std::max(D, X.second);
		return D;
	}

	struct FRaceSeriesRow
	{
		std::string Slot;
		std::vector<double> Scores;         // race by race
		std::vector<bool> Dropped;
		double Total = 0.0, Net = 0.0;
		int Place = 0;
	};

	/** A series' table from every boat's scores race by race (a boat with fewer scores than Races is given Missing for the
	 *  rest: it did not come). Net = the total less the discards. Ties: the better scores first (A8.1, kept scores only), then
	 *  the last race back (A8.2); still level, they share the place. Sorted by place. */
	inline std::vector<FRaceSeriesRow> RaceSeriesTable(const FSchoolParams& P, const std::map<std::string, std::vector<double>>& Scores, int Races, double Missing)
	{
		std::vector<FRaceSeriesRow> Rows;
		const int Drop = RaceDiscards(P, Races);
		for (const auto& It : Scores)
		{
			FRaceSeriesRow R;
			R.Slot = It.first;
			R.Scores = It.second;
			R.Scores.resize((size_t)std::max(0, Races), Missing);
			R.Dropped.assign(R.Scores.size(), false);
			for (int K = 0; K < Drop; K++)
			{
				int Worst = -1;
				for (size_t I = 0; I < R.Scores.size(); I++) if (!R.Dropped[I] && (Worst < 0 || R.Scores[I] > R.Scores[(size_t)Worst])) Worst = (int)I;
				if (Worst >= 0) R.Dropped[(size_t)Worst] = true;
			}
			for (size_t I = 0; I < R.Scores.size(); I++) { R.Total += R.Scores[I]; if (!R.Dropped[I]) R.Net += R.Scores[I]; }
			Rows.push_back(R);
		}
		auto Kept = [](const FRaceSeriesRow& R)
		{
			std::vector<double> K;
			for (size_t I = 0; I < R.Scores.size(); I++) if (!R.Dropped[I]) K.push_back(R.Scores[I]);
			std::sort(K.begin(), K.end());
			return K;
		};
		auto Cmp = [&Kept](const FRaceSeriesRow& A, const FRaceSeriesRow& B)
		{
			if (std::fabs(A.Net - B.Net) > 1e-9) return A.Net < B.Net ? -1 : 1;
			const std::vector<double> Ka = Kept(A), Kb = Kept(B);
			for (size_t I = 0; I < Ka.size() && I < Kb.size(); I++) if (std::fabs(Ka[I] - Kb[I]) > 1e-9) return Ka[I] < Kb[I] ? -1 : 1;
			for (size_t I = A.Scores.size(); I-- > 0;) if (std::fabs(A.Scores[I] - B.Scores[I]) > 1e-9) return A.Scores[I] < B.Scores[I] ? -1 : 1;
			return 0;
		};
		std::stable_sort(Rows.begin(), Rows.end(), [&Cmp](const FRaceSeriesRow& A, const FRaceSeriesRow& B) { return Cmp(A, B) < 0; });
		for (size_t I = 0; I < Rows.size(); I++) Rows[I].Place = I > 0 && Cmp(Rows[I - 1], Rows[I]) == 0 ? Rows[I - 1].Place : (int)I + 1;
		return Rows;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// A race nobody watches

	struct FRaceEntry
	{
		std::string Slot;
		double Skill = 0.5, Heavy = 0.0;
	};

	/** The school's entries of a class (the sailors' slots; the hero is added by the caller when he sails). */
	inline std::vector<FRaceEntry> RaceEntries(const FSchoolParams& P, int Class)
	{
		std::vector<FRaceEntry> Out;
		for (const FSchoolSailor& S : P.Sailors)
		{
			if (S.Class == Class) { FRaceEntry E; E.Slot = S.Slot; E.Skill = S.Skill; E.Heavy = S.Heavy; Out.push_back(E); }
		}
		return Out;
	}

	/** A class's speed made good round a course in a wind, m/s (the resolver's clock). */
	inline double RaceClassSpeed(const FSchoolParams& P, int Class, double Wind)
	{
		return P.SpeedMs[Class] * (1.0 - std::exp(-std::max(0.0, Wind) / 3.5)) / std::max(1.0, P.BeatFactor * 0.5 + 0.5);
	}

	/** Resolves a race from skill, the day's luck and the wind: the better sailor wins more often, not always. Scored
	 *  (RaceScore) and in finishing order. The same seed gives the same race. */
	inline std::vector<FRaceFinish> RaceResolve(const FSchoolParams& P, uint32_t Seed, int Class, const std::vector<FRaceEntry>& Entries, double Wind,
		double Gust, double CourseLengthM)
	{
		std::vector<FRaceFinish> Out;
		const double Base = std::max(60.0, CourseLengthM) / std::max(0.2, RaceClassSpeed(P, Class, Wind));
		const double Stress = SchoolClamp((std::max(Wind, Gust - P.GustOver) - 0.6 * P.Wind[Class][1]) / (0.4 * P.Wind[Class][1]), 0.0, 1.5);
		for (const FRaceEntry& E : Entries)
		{
			FSchoolRng Rng(SchoolHash(Seed, SchoolHashStr(E.Slot)));
			Rng.Next(); Rng.Next();
			FRaceFinish F;
			F.Slot = E.Slot;
			const double Perf = E.Skill + Rng.N() * P.LuckSigma + E.Heavy * (Wind - P.WindRef) / std::max(0.1, P.WindRef) * P.HeavyGain;
			F.TimeS = Base * std::max(0.6, P.TimeBase - P.TimeGain * Perf);
			const double Roll = Rng.U();
			const double Dnf = P.DnfBase + P.DnfWind * Stress * (1.2 - E.Skill);
			if (Roll < P.DnsChance * (1.2 - E.Skill)) F.Status = ERaceStatus::Dns;
			else if (Roll < P.DnsChance * (1.2 - E.Skill) + Dnf) F.Status = ERaceStatus::Dnf;
			else if (Roll > 1.0 - P.OcsChance * (0.4 + E.Skill)) F.Status = ERaceStatus::Ocs;      // the keen ones push the line
			else F.Status = F.TimeS > P.TimeLimitS[Class] + P.FinishWindowS ? ERaceStatus::Dnf : ERaceStatus::Finished;
			Out.push_back(F);
		}
		RaceScore(Out);
		return Out;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The logbook (what is saved)

	/** A series' scores of one class this season. */
	struct FSchoolSeries
	{
		std::string Event;
		int Class = 0;
		int Races = 0;
		int Entries = 0;                    // the biggest fleet so far (a boat that did not come scores this + 1)
		std::map<std::string, std::vector<double>> Scores;     // slot -> its score race by race ("hero" the hero)
	};

	/** A race day's own table of one class (for the paper the next morning). */
	struct FSchoolDayTable
	{
		long long Day = -1;
		std::string Event;
		int Class = 0;
		int Races = 0;
		int Abandoned = 0;                  // an ESchoolReason when nothing was sailed
		double Wind = 0.0;
		bool bLast = false;                 // the event's last day: the series' table is final
		std::vector<std::pair<std::string, double>> Points;    // slot -> the day's points, best first
	};

	struct FSchoolLog
	{
		bool bMember = false;
		long long JoinedDay = -1;
		uint32_t LessonsDone = 0;           // a bit per lesson (the data's order)
		int Rank = 0;
		int Races = 0, Wins = 0, Podiums = 0, BestPlace = 0;
		double MetresSailed = 0.0, HoursAfloat = 0.0;
		int Capsizes = 0;
		double Wear = 0.0;                  // the hire boat's, 0..1
		long long HiredDay = -1, MendedDay = -1;
		uint32_t Stamps = 0;                // a bit per piece of the collection (given already)
		int Season = 0;                     // the year the tables are of
		std::vector<FSchoolSeries> Series;
		std::vector<FSchoolDayTable> Days;  // the last race day's tables

		double Miles(const FSchoolParams& P) const { return MetresSailed / std::max(1.0, P.MileMetres); }
		bool HasLesson(int I) const { return I >= 0 && I < 32 && (LessonsDone >> I & 1u); }

		FSchoolSeries& SeriesOf(const std::string& Event, int Class)
		{
			for (FSchoolSeries& S : Series) if (S.Event == Event && S.Class == Class) return S;
			FSchoolSeries S;
			S.Event = Event; S.Class = Class;
			Series.push_back(S);
			return Series.back();
		}
		const FSchoolSeries* FindSeries(const std::string& Event, int Class) const
		{
			for (const FSchoolSeries& S : Series) if (S.Event == Event && S.Class == Class) return &S;
			return nullptr;
		}

		std::string Save() const
		{
			std::ostringstream O;
			O.precision(9);
			O << "v1 " << (bMember ? 1 : 0) << ' ' << JoinedDay << ' ' << LessonsDone << ' ' << Rank << ' ' << Races << ' ' << Wins << ' ' << Podiums
				<< ' ' << BestPlace << ' ' << MetresSailed << ' ' << HoursAfloat << ' ' << Capsizes << ' ' << Wear << ' ' << HiredDay << ' ' << MendedDay
				<< ' ' << Stamps << ' ' << Season << ' ' << Series.size();
			for (const FSchoolSeries& S : Series)
			{
				O << ' ' << S.Event << ' ' << S.Class << ' ' << S.Races << ' ' << S.Entries << ' ' << S.Scores.size();
				for (const auto& It : S.Scores)
				{
					O << ' ' << It.first << ' ' << It.second.size();
					for (const double V : It.second) O << ' ' << (long long)std::llround(V * 2.0);     // points are whole or halves
				}
			}
			O << ' ' << Days.size();
			for (const FSchoolDayTable& D : Days)
			{
				O << ' ' << D.Day << ' ' << D.Event << ' ' << D.Class << ' ' << D.Races << ' ' << D.Abandoned << ' ' << D.Wind << ' ' << (D.bLast ? 1 : 0)
					<< ' ' << D.Points.size();
				for (const auto& X : D.Points) O << ' ' << X.first << ' ' << (long long)std::llround(X.second * 2.0);
			}
			return O.str();
		}

		bool Load(const std::string& Text)
		{
			std::istringstream I(Text);
			std::string V;
			FSchoolLog L;
			int Member = 0;
			size_t Ns = 0, Nd = 0;
			if (!(I >> V) || V != "v1" || !(I >> Member >> L.JoinedDay >> L.LessonsDone >> L.Rank >> L.Races >> L.Wins >> L.Podiums >> L.BestPlace
				>> L.MetresSailed >> L.HoursAfloat >> L.Capsizes >> L.Wear >> L.HiredDay >> L.MendedDay >> L.Stamps >> L.Season >> Ns) || Ns > 64)
			{
				return false;
			}
			L.bMember = Member != 0;
			for (size_t K = 0; K < Ns; K++)
			{
				FSchoolSeries S;
				size_t N = 0;
				if (!(I >> S.Event >> S.Class >> S.Races >> S.Entries >> N) || N > 64) return false;
				for (size_t J = 0; J < N; J++)
				{
					std::string Slot;
					size_t M = 0;
					if (!(I >> Slot >> M) || M > 512) return false;
					std::vector<double>& Sc = S.Scores[Slot];
					for (size_t Q = 0; Q < M; Q++)
					{
						long long H = 0;
						if (!(I >> H)) return false;
						Sc.push_back((double)H * 0.5);
					}
				}
				L.Series.push_back(S);
			}
			if (!(I >> Nd) || Nd > 16) return false;
			for (size_t K = 0; K < Nd; K++)
			{
				FSchoolDayTable D;
				int Last = 0;
				size_t N = 0;
				if (!(I >> D.Day >> D.Event >> D.Class >> D.Races >> D.Abandoned >> D.Wind >> Last >> N) || N > 64) return false;
				D.bLast = Last != 0;
				for (size_t J = 0; J < N; J++)
				{
					std::string Slot;
					long long H = 0;
					if (!(I >> Slot >> H)) return false;
					D.Points.push_back({ Slot, (double)H * 0.5 });
				}
				L.Days.push_back(D);
			}
			*this = L;
			return true;
		}
	};

	/** What a step of the hero's path asks the engine to do: stamps to give, quest events to fire, a new rank. */
	struct FSchoolEvents
	{
		std::vector<std::string> Stamps;    // pieces of the collection "sailing"
		std::vector<std::string> Quest;     // "sail <what>"
		int NewRank = -1;
		bool bCertificate = false;

		void Add(const FSchoolEvents& O)
		{
			Stamps.insert(Stamps.end(), O.Stamps.begin(), O.Stamps.end());
			Quest.insert(Quest.end(), O.Quest.begin(), O.Quest.end());
			if (O.NewRank >= 0) NewRank = O.NewRank;
			bCertificate = bCertificate || O.bCertificate;
		}
	};

	inline void SchoolGiveStamp(const FSchoolParams& P, FSchoolLog& Log, const std::string& Piece, FSchoolEvents& Ev)
	{
		for (size_t I = 0; I < P.StampPieces.size() && I < 32; I++)
		{
			if (P.StampPieces[I] == Piece && !(Log.Stamps >> I & 1u))
			{
				Log.Stamps |= 1u << I;
				Ev.Stamps.push_back(Piece);
			}
		}
	}

	/** A new season: last year's tables are cleared when the first thing of a new year is written. */
	inline void SchoolRollSeason(FSchoolLog& Log, int Year)
	{
		if (Log.Season != Year)
		{
			Log.Season = Year;
			Log.Series.clear();
			Log.Days.clear();
		}
	}

	/** The rank the log has earned (the highest whose lessons, races, miles and podiums are all there; ranks are in order). */
	inline int SchoolRankFor(const FSchoolParams& P, const FSchoolLog& Log)
	{
		int Rank = 0;
		if (!Log.bMember)
		{
			return 0;
		}
		for (size_t I = 1; I < P.Ranks.size(); I++)
		{
			const FSchoolRankDef& R = P.Ranks[I];
			bool bOk = Log.Races >= R.Races && Log.Podiums >= R.Podiums && Log.Miles(P) + 1e-9 >= R.Miles;
			for (const std::string& L : R.Lessons) bOk = bOk && Log.HasLesson(P.Lesson(L));
			if (!bOk) break;
			Rank = (int)I;
		}
		return Rank;
	}

	inline bool SchoolHasCertificate(const FSchoolParams& P, const FSchoolLog& Log)
	{
		for (int I = 0; I <= Log.Rank && I < (int)P.Ranks.size(); I++) if (P.Ranks[(size_t)I].bCertificate) return true;
		return false;
	}

	/** Raises the rank to what the log has earned; says what is new. */
	inline void SchoolPromote(const FSchoolParams& P, FSchoolLog& Log, FSchoolEvents& Ev)
	{
		const int Rank = SchoolRankFor(P, Log);
		for (int R = Log.Rank + 1; R <= Rank; R++)
		{
			const FSchoolRankDef& D = P.Ranks[(size_t)R];
			Ev.NewRank = R;
			Ev.Quest.push_back("sail rank " + D.Id);
			if (P.HasStamp(D.Id)) SchoolGiveStamp(P, Log, D.Id, Ev);
			if (D.bCertificate)
			{
				Ev.bCertificate = true;
				Ev.Quest.push_back("sail certificate");
				SchoolGiveStamp(P, Log, "certificate", Ev);
			}
		}
		Log.Rank = std::max(Log.Rank, Rank);
	}

	/** Joining: a talk with the head coach and the fee (the engine takes the money and gives the item FSchoolParams::CardItem). */
	inline bool SchoolCanJoin(const FSchoolParams& P, const FSchoolLog& Log, int Money) { return !Log.bMember && Money >= P.JoinFee; }

	inline FSchoolEvents SchoolJoin(const FSchoolParams& P, FSchoolLog& Log, const FSchoolDate& Date)
	{
		FSchoolEvents Ev;
		if (Log.bMember)
		{
			return Ev;
		}
		Log.bMember = true;
		Log.JoinedDay = SchoolDayNumber(Date);
		Ev.Quest.push_back("sail join");
		SchoolGiveStamp(P, Log, "joined", Ev);
		SchoolPromote(P, Log, Ev);
		return Ev;
	}

	/** The first lesson not done yet (-1: all done). */
	inline int SchoolNextLesson(const FSchoolParams& P, const FSchoolLog& Log)
	{
		for (size_t I = 0; I < P.Lessons.size(); I++) if (!Log.HasLesson((int)I)) return (int)I;
		return -1;
	}

	struct FLessonGate
	{
		bool bOk = false;
		ESchoolReason Reason = ESchoolReason::None;
		std::string Why;            // not_member, done_before, order, rank, hours, race, weather, cold_water
		std::string Coach;
	};

	/** May the hero take a lesson now? In order, a member, the coach in (for an afloat lesson: in season, in the lesson
	 *  hours, luches may sail, the lesson's own wind and warmth, no regatta on); the shore lesson any day the school has
	 *  somebody in (in season in the lesson hours, out of it on the theory days). */
	inline FLessonGate LessonAvailable(const FSchoolParams& P, const FSchoolLog& Log, int Lesson, const FSchoolDate& Date, double Hour,
		const FSchoolWeather& W, bool bDayOff = false)
	{
		FLessonGate G;
		if (Lesson < 0 || Lesson >= (int)P.Lessons.size()) { G.Why = "order"; return G; }
		const FLessonDef& L = P.Lessons[(size_t)Lesson];
		if (!Log.bMember) { G.Why = "not_member"; return G; }
		for (int I = 0; I < Lesson; I++) if (!Log.HasLesson(I)) { G.Why = "order"; return G; }
		if (!L.NeedsRank.empty() && Log.Rank < P.Rank(L.NeedsRank)) { G.Why = "rank"; return G; }
		const FSchoolNow Now = SchoolSessionAt(P, Date, Hour, bDayOff);
		const FSchoolDaylight Light = SchoolDaylight(P, Date);
		const ESeasonState Season = SeasonStateOn(P, Date);
		// the tutor: the head coach, or whoever is not out with a group
		G.Coach = Now.bOn && Now.Coach == "coach_head" ? (Now.Group == "kids" ? "coach_juniors" : "coach_kids") : "coach_head";
		if (!L.bAfloat)
		{
			const bool bIn = Season == ESeasonState::Open ? (Hour >= P.LessonFrom && Hour <= Light.Sunset)
				: (Now.bOn && (Now.Kind == "theory" || Now.Kind == "fitout" || Now.Kind == "layup"));
			if (!bIn) { G.Why = "hours"; G.Reason = Season == ESeasonState::Open ? ESchoolReason::Dark : ESchoolReason::Season; return G; }
			if (Now.bOn && Now.Kind == "race") { G.Why = "race"; return G; }
			G.bOk = true;
			return G;
		}
		if (Now.bOn && Now.Kind == "race") { G.Why = "race"; return G; }
		const FSchoolGo Go = SchoolDecide(P, Date, W, 1 << P.HeroClass);
		if (Go.Go != ESchoolGo::Sail) { G.Why = "weather"; G.Reason = Go.Reason; return G; }
		if (Hour < P.LessonFrom || Hour > Light.Sunset - P.LessonBeforeSunset) { G.Why = "hours"; G.Reason = ESchoolReason::Dark; return G; }
		if (W.Wind < L.Wind[0]) { G.Why = "weather"; G.Reason = ESchoolReason::Calm; return G; }
		if (W.Wind > L.Wind[1]) { G.Why = "weather"; G.Reason = ESchoolReason::Wind; return G; }
		if (L.bWarmOnly && W.WaterC < P.CapsizeWaterC) { G.Why = "cold_water"; return G; }
		G.bOk = true;
		return G;
	}

	/** A lesson is done: the stamps, the quest events, the rank. */
	inline FSchoolEvents SchoolCompleteLesson(const FSchoolParams& P, FSchoolLog& Log, int Lesson)
	{
		FSchoolEvents Ev;
		if (Lesson < 0 || Lesson >= (int)P.Lessons.size() || Lesson >= 32 || Log.HasLesson(Lesson))
		{
			return Ev;
		}
		Log.LessonsDone |= 1u << Lesson;
		const std::string& Id = P.Lessons[(size_t)Lesson].Id;
		Ev.Quest.push_back("sail lesson " + Id);
		if (P.HasStamp(Id)) SchoolGiveStamp(P, Log, Id, Ev);
		SchoolPromote(P, Log, Ev);
		return Ev;
	}

	/** Time on the water written into the log: metres, hours, capsizes (the mile stamps, the hire boat's wear). */
	inline FSchoolEvents SchoolAddSailing(const FSchoolParams& P, FSchoolLog& Log, double Metres, double Hours, int Capsizes)
	{
		FSchoolEvents Ev;
		const double Before = Log.Miles(P);
		Log.MetresSailed += std::max(0.0, Metres);
		Log.HoursAfloat += std::max(0.0, Hours);
		Log.Capsizes += std::max(0, Capsizes);
		Log.Wear = SchoolClamp(Log.Wear + Capsizes * P.WearPerCapsize + Hours * P.WearPerHour, 0.0, 1.0);
		if (Capsizes > 0) Ev.Quest.push_back("sail capsize");
		for (const auto& M : P.MileStamps)
		{
			if (Before < M.first && Log.Miles(P) >= M.first)
			{
				Ev.Quest.push_back("sail miles " + std::to_string(M.first));
				SchoolGiveStamp(P, Log, M.second, Ev);
			}
		}
		SchoolPromote(P, Log, Ev);
		return Ev;
	}

	/** The bosun mends the hire boat on his day (call once a day). */
	inline void SchoolMend(const FSchoolParams& P, FSchoolLog& Log, const FSchoolDate& Date)
	{
		const long long N = SchoolDayNumber(Date);
		if (SchoolWeekday(Date) == P.MendWeekday && Log.MendedDay != N)
		{
			Log.MendedDay = N;
			Log.Wear = 0.0;
		}
	}

	/** The hero was out when the boats were recalled and came in: the storm stamp. */
	inline FSchoolEvents SchoolNoteRecall(const FSchoolParams& P, FSchoolLog& Log)
	{
		FSchoolEvents Ev;
		Ev.Quest.push_back("sail recall");
		SchoolGiveStamp(P, Log, "recall", Ev);
		return Ev;
	}

	/** May the hero hire a boat alone / enter a regatta? (The certificate; the hours and the weather are SchoolHireOpen's.) */
	inline bool SchoolCanHire(const FSchoolParams& P, const FSchoolLog& Log) { return Log.bMember && SchoolHasCertificate(P, Log); }
	inline bool SchoolCanEnter(const FSchoolParams& P, const FSchoolLog& Log, int Event, int Money = 1000000)
	{
		return SchoolCanHire(P, Log) && Event >= 0 && Event < (int)P.Events.size() && P.Events[(size_t)Event].Classes[P.HeroClass]
			&& Money >= P.Events[(size_t)Event].EntryFee;
	}

	inline FSchoolEvents SchoolHire(const FSchoolParams& P, FSchoolLog& Log, const FSchoolDate& Date)
	{
		FSchoolEvents Ev;
		(void)P;
		Log.HiredDay = SchoolDayNumber(Date);
		Ev.Quest.push_back("sail hire");
		return Ev;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Race days in the log: scores, the day's tables, the news

	inline FSchoolDayTable& SchoolDayTableOf(FSchoolLog& Log, long long Day, const std::string& Event, int Class)
	{
		if (!Log.Days.empty() && Log.Days[0].Day != Day) Log.Days.clear();
		for (FSchoolDayTable& D : Log.Days) if (D.Event == Event && D.Class == Class) return D;
		FSchoolDayTable D;
		D.Day = Day; D.Event = Event; D.Class = Class;
		Log.Days.push_back(D);
		return Log.Days.back();
	}

	/** Writes one sailed race into the series and the day's table. Results: scored (RaceScore). The hero's slot is "hero":
	 *  his races, wins and podiums are counted and the stamps given. */
	inline FSchoolEvents SchoolRecordRace(const FSchoolParams& P, FSchoolLog& Log, const FSchoolDate& Date, int Event, int Class,
		const std::vector<FRaceFinish>& Results, double Wind)
	{
		FSchoolEvents Ev;
		if (Event < 0 || Event >= (int)P.Events.size())
		{
			return Ev;
		}
		SchoolRollSeason(Log, Date.Y);
		const FRaceEventDef& E = P.Events[(size_t)Event];
		const long long Day = SchoolDayNumber(Date);
		FSchoolSeries& S = Log.SeriesOf(E.Id, Class);
		const int Was = S.Entries;
		S.Entries = std::max(S.Entries, (int)Results.size());
		// a boat new to the series is given DNS for the races it missed; one absent today scores DNS today
		for (const FRaceFinish& F : Results)
		{
			std::vector<double>& Sc = S.Scores[F.Slot];
			Sc.resize((size_t)S.Races, (double)std::max(Was, 1) + 1.0);
			Sc.push_back(F.Points);
		}
		S.Races++;
		for (auto& It : S.Scores) It.second.resize((size_t)S.Races, (double)S.Entries + 1.0);
		FSchoolDayTable& D = SchoolDayTableOf(Log, Day, E.Id, Class);
		D.Races++;
		D.Wind = Wind;
		D.bLast = RaceDayOn(P, Date).bLast && E.Rule != "weekly";
		for (const FRaceFinish& F : Results)
		{
			bool bFound = false;
			for (auto& X : D.Points) if (X.first == F.Slot) { X.second += F.Points; bFound = true; }
			if (!bFound) D.Points.push_back({ F.Slot, F.Points + (D.Races - 1) * ((double)Results.size() + 1.0) });
		}
		for (auto& X : D.Points)
		{
			bool bHere = false;
			for (const FRaceFinish& F : Results) bHere = bHere || F.Slot == X.first;
			if (!bHere) X.second += (double)Results.size() + 1.0;
		}
		// the day's order: points, then the last race's finishing order
		std::stable_sort(D.Points.begin(), D.Points.end(), [&Results](const std::pair<std::string, double>& A, const std::pair<std::string, double>& B)
		{
			if (std::fabs(A.second - B.second) > 1e-9) return A.second < B.second;
			size_t Ia = Results.size(), Ib = Results.size();
			for (size_t I = 0; I < Results.size(); I++) { if (Results[I].Slot == A.first) Ia = I; if (Results[I].Slot == B.first) Ib = I; }
			return Ia < Ib;
		});
		for (const FRaceFinish& F : Results)
		{
			if (F.Slot != "hero") continue;
			Log.Races++;
			Ev.Quest.push_back("sail race");
			SchoolGiveStamp(P, Log, "first_race", Ev);
			if (!E.Stamp.empty() && !E.bCup) { SchoolGiveStamp(P, Log, E.Stamp, Ev); Ev.Quest.push_back("sail " + E.Stamp); }
			if (F.Place > 0 && (Log.BestPlace == 0 || F.Place < Log.BestPlace)) Log.BestPlace = F.Place;
			if (F.Place == 1) { Log.Wins++; Ev.Quest.push_back("sail race_win"); SchoolGiveStamp(P, Log, "race_win", Ev); }
			if (F.Place >= 1 && F.Place <= 3) { Log.Podiums++; Ev.Quest.push_back("sail podium"); }
		}
		SchoolPromote(P, Log, Ev);
		return Ev;
	}

	/** A line of news: who won what. Slots are sailors' slots ("hero" the hero); the engine turns them into names. */
	struct FSchoolNewsItem
	{
		std::string Key;            // the string key of the line (FSchoolParams::RadioNews)
		std::string Event;
		int Class = 0;
		std::string First, Second, Third;
		int HeroPlace = 0;          // 0: he did not sail
		int Races = 0;
		double Wind = 0.0;
		bool bAbandoned = false;
		ESchoolReason Reason = ESchoolReason::None;
		bool bSeries = false;       // the series' final table (a cup's last day, the season's on the closing day), not a day's
	};

	inline FSchoolNewsItem SchoolNewsFromTable(const FSchoolParams& P, const std::string& Event, int Class, const std::vector<std::string>& Order)
	{
		FSchoolNewsItem N;
		N.Event = Event; N.Class = Class;
		if (Order.size() > 0) N.First = Order[0];
		if (Order.size() > 1) N.Second = Order[1];
		if (Order.size() > 2) N.Third = Order[2];
		for (size_t I = 0; I < Order.size(); I++) if (Order[I] == "hero") N.HeroPlace = (int)I + 1;
		auto Key = [&P](const char* K) { const auto It = P.RadioNews.find(K); return It != P.RadioNews.end() ? It->second : std::string(K); };
		N.Key = N.HeroPlace == 1 ? Key("hero_won") : (N.HeroPlace > 0 ? Key("hero_place") : Key("result"));
		return N;
	}

	/** The news of a race day from the log (ask the next morning for yesterday): each class's day, and on an event's last
	 *  day (and for the weekly series on the closing day) the series' final tables. Empty: no racing that day. */
	inline std::vector<FSchoolNewsItem> SchoolNewsOfDay(const FSchoolParams& P, const FSchoolLog& Log, const FSchoolDate& Date)
	{
		std::vector<FSchoolNewsItem> Out;
		const long long Day = SchoolDayNumber(Date);
		auto Key = [&P](const char* K) { const auto It = P.RadioNews.find(K); return It != P.RadioNews.end() ? It->second : std::string(K); };
		// in the calendar's order of events, optimists first (the log holds them in the order they were sailed)
		std::vector<const FSchoolDayTable*> Tables;
		for (const FSchoolDayTable& D : Log.Days) if (D.Day == Day) Tables.push_back(&D);
		std::stable_sort(Tables.begin(), Tables.end(), [&P](const FSchoolDayTable* A, const FSchoolDayTable* B)
		{
			const int Ea = P.Event(A->Event), Eb = P.Event(B->Event);
			return Ea != Eb ? Ea < Eb : A->Class < B->Class;
		});
		for (const FSchoolDayTable* Dp : Tables)
		{
			const FSchoolDayTable& D = *Dp;
			std::vector<std::string> Order;
			for (const auto& X : D.Points) Order.push_back(X.first);
			FSchoolNewsItem N = SchoolNewsFromTable(P, D.Event, D.Class, Order);
			N.Races = D.Races; N.Wind = D.Wind;
			if (D.Races == 0)
			{
				N.bAbandoned = true;
				N.Reason = (ESchoolReason)D.Abandoned;
				N.Key = Key("abandoned");
			}
			else if (D.Event == "sail_opening" && N.HeroPlace == 0) N.Key = Key("opening");
			else if (D.Event == "sail_closing" && N.HeroPlace == 0) N.Key = Key("closing");
			Out.push_back(N);
		}
		if (Out.empty())
		{
			return Out;
		}
		const bool bClosing = Date == SeasonClosing(P, Date.Y);
		for (const FSchoolSeries& S : Log.Series)
		{
			const int Ei = P.Event(S.Event);
			if (Ei < 0 || S.Races == 0) continue;
			const FRaceEventDef& E = P.Events[(size_t)Ei];
			bool bFinal = false;
			if (E.Rule == "weekly") bFinal = bClosing;
			else if (E.Days > 1) for (const FSchoolDayTable& D : Log.Days) bFinal = bFinal || (D.Day == Day && D.Event == S.Event && D.bLast);
			if (!bFinal) continue;
			std::vector<std::string> Order;
			for (const FRaceSeriesRow& R : RaceSeriesTable(P, S.Scores, S.Races, S.Entries + 1.0)) Order.push_back(R.Slot);
			FSchoolNewsItem N = SchoolNewsFromTable(P, S.Event, S.Class, Order);
			N.Races = S.Races;
			N.bSeries = true;
			if (N.HeroPlace == 0) N.Key = Key("cup");
			Out.push_back(N);
		}
		return Out;
	}

	/** The end of a race day (call once, after the last race's time, or the next morning when the hero was away): every race
	 *  of the day not sailed in front of the player is resolved from the fleet's skill and the wind and written into the log,
	 *  a class that could not sail is written as abandoned, a cup's podium stamp is given on its last day. W: the weather at
	 *  the races' time; WindTo: where the wind blew. Returns the day's news (SchoolNewsOfDay). */
	inline std::vector<FSchoolNewsItem> SchoolResolveDay(const FSchoolParams& P, FSchoolLog& Log, const FSchoolDate& Date, const FSchoolWeather& W,
		const FSchoolV2& WindTo, bool bDayOff = false, FSchoolEvents* OutEvents = nullptr)
	{
		const long long Day = SchoolDayNumber(Date);
		FSchoolEvents Ev;
		for (const FTimetableSession& S : SchoolSessionsOn(P, Date, bDayOff))
		{
			if (S.Kind != "race") continue;     // an evening race the daylight does not hold is simply not sailed
			SchoolRollSeason(Log, Date.Y);
			const FRaceEventDef& E = P.Events[(size_t)S.EventIndex];
			FSchoolWeather Wr = W;
			Wr.bDaylight = Wr.bDaylight && S.bAfloat;
			const FSchoolGo Go = SchoolDecide(P, Date, Wr, S.Classes);
			for (int C = 0; C < SchoolBoats; C++)
			{
				if (!(S.Classes >> C & 1)) continue;
				FSchoolDayTable& D = SchoolDayTableOf(Log, Day, E.Id, C);
				if (!Go.ClassOk[C])
				{
					if (D.Races == 0) D.Abandoned = (int)(Go.ClassReason[C] == ESchoolReason::None ? ESchoolReason::Dark : Go.ClassReason[C]);
					D.Wind = W.Wind;
					continue;
				}
				for (int R = D.Races; R < S.Races; R++)
				{
					const uint32_t Seed = RaceSeed(Date, S.EventIndex, R + S.EventDay * 8, C);
					const std::vector<FRaceEntry> En = RaceEntries(P, C);
					const FRaceCourse Co = RaceSetCourse(P, RaceCourseKind(E.Course), C, WindTo, W.Wind, (int)En.size(), Seed, W.TideM, Go.ClassArea[C]);
					Ev.Add(SchoolRecordRace(P, Log, Date, S.EventIndex, C, RaceResolve(P, Seed, C, En, W.Wind, W.Gust, Co.bOk ? Co.LengthM : P.LegM[C] * 3.0), W.Wind));
				}
			}
			// a cup's podium on its last day
			if (E.bCup && RaceDayOn(P, Date).bLast && !E.Stamp.empty())
			{
				const FSchoolSeries* Se = Log.FindSeries(E.Id, P.HeroClass);
				if (Se && Se->Scores.count("hero"))
				{
					for (const FRaceSeriesRow& R : RaceSeriesTable(P, Se->Scores, Se->Races, Se->Entries + 1.0))
					{
						if (R.Slot == "hero" && R.Place <= 3) { SchoolGiveStamp(P, Log, E.Stamp, Ev); Ev.Quest.push_back("sail cup " + E.Id); }
					}
				}
			}
		}
		if (OutEvents) *OutEvents = Ev;
		return SchoolNewsOfDay(P, Log, Date);
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The radio's forecast for sailors, the spectators

	struct FSchoolForecast
	{
		std::string Key;            // the line's key (FSchoolParams::RadioForecast)
		ESchoolGo Go = ESchoolGo::Closed;
		ESchoolReason Reason = ESchoolReason::None;
		double Wind = 0.0, Gust = 0.0;
		double BestHour = -1.0;     // the day's best hour for a luch (-1: none)
		bool bReef = false, bRace = false;
		std::string Event;          // today's regatta
		std::vector<std::string> Groups;    // the groups whose session can go afloat by the forecast
	};

	/** The facts of the morning's line for sailors, from the day's forecast hour by hour (hour, weather). */
	inline FSchoolForecast SchoolForecastForSailors(const FSchoolParams& P, const FSchoolDate& Date,
		const std::vector<std::pair<double, FSchoolWeather>>& Hours, bool bDayOff = false)
	{
		FSchoolForecast F;
		auto Key = [&P](const char* K) { const auto It = P.RadioForecast.find(K); return It != P.RadioForecast.end() ? It->second : std::string(K); };
		F.Key = Key("closed");
		if (SeasonStateOn(P, Date) != ESeasonState::Open)
		{
			F.Reason = ESchoolReason::Season;
			return F;
		}
		const std::vector<FTimetableSession> Sessions = SchoolSessionsOn(P, Date, bDayOff);
		for (const FTimetableSession& S : Sessions) if (S.Kind == "race") { F.bRace = true; F.Event = S.Event; }
		bool bStorm = false, bAnyDay = false;
		double BestScore = 1e18;
		ESchoolReason Why = ESchoolReason::None;
		for (const auto& H : Hours)
		{
			const FSchoolGo G = SchoolDecide(P, Date, H.second, 7);
			if (G.Reason == ESchoolReason::Dark || G.Reason == ESchoolReason::Ice)
			{
				if (G.Reason == ESchoolReason::Ice) Why = G.Reason;
				continue;
			}
			bAnyDay = true;
			F.Wind = std::max(F.Wind, H.second.Wind);
			F.Gust = std::max(F.Gust, H.second.Gust);
			bStorm = bStorm || G.Go == ESchoolGo::Storm;
			if (G.ClassOk[SchoolLuch])
			{
				const double Score = std::fabs(H.second.Wind - 5.5);
				if (Score < BestScore) { BestScore = Score; F.BestHour = H.first; }
				F.bReef = F.bReef || G.bReef;
			}
			else if (Why == ESchoolReason::None || G.Go == ESchoolGo::Storm) Why = G.ClassReason[SchoolLuch];
		}
		// each session by the forecast's hour nearest to the middle of its time on the water
		for (const FTimetableSession& S : Sessions)
		{
			if (!S.bAfloat || S.Group.empty() || Hours.empty()) continue;
			const double Mid = (S.WaterFrom + S.WaterTo) * 0.5;
			size_t Best = 0;
			for (size_t I = 1; I < Hours.size(); I++) if (std::fabs(Hours[I].first - Mid) < std::fabs(Hours[Best].first - Mid)) Best = I;
			FSchoolWeather Ws = Hours[Best].second;
			Ws.bDaylight = true;        // the timetable already keeps the session inside the daylight
			const FSchoolGo G = SchoolDecide(P, Date, Ws, S.Classes);
			bool bGo = false;
			for (int C = 0; C < SchoolBoats; C++) bGo = bGo || ((S.Classes >> C & 1) && G.ClassOk[C]);
			if (bGo && std::find(F.Groups.begin(), F.Groups.end(), S.Group) == F.Groups.end()) F.Groups.push_back(S.Group);
		}
		if (bStorm) { F.Go = ESchoolGo::Storm; F.Key = Key("storm"); F.Reason = Why == ESchoolReason::None ? ESchoolReason::Wind : Why; F.BestHour = -1.0; F.Groups.clear(); }
		else if (F.BestHour >= 0.0) { F.Go = ESchoolGo::Sail; F.Key = F.bRace ? Key("race") : (F.bReef ? Key("reef") : Key("sail")); }
		else if (bAnyDay) { F.Go = ESchoolGo::Shore; F.Key = Key("shore"); F.Reason = Why; }
		else F.Reason = Why == ESchoolReason::None ? ESchoolReason::Dark : Why;
		return F;
	}

	/** How strongly a regatta draws the town to watch (0 none .. ~1.2 the opening day) and where they stand: only while the
	 *  boats are on the water, a little before the first start. OutPlace: a key of the school's places. */
	inline double SchoolSpectatorPull(const FSchoolParams& P, const FSchoolDate& Date, double Hour, bool bDayOff = false, std::string* OutPlace = nullptr)
	{
		for (const FTimetableSession& S : SchoolSessionsOn(P, Date, bDayOff))
		{
			if (S.Kind != "race" || !S.bAfloat || Hour < S.WaterFrom - 0.25 || Hour >= S.WaterTo) continue;
			const double Pull = P.Events[(size_t)S.EventIndex].Pull;
			if (OutPlace) *OutPlace = Pull >= 0.9 ? "spectators_beach" : "spectators_pier";
			return Pull;
		}
		return 0.0;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// A lesson's objectives, read from the boat

	/** What the engine tells a lesson of the hero's boat each tick. */
	struct FLessonTelemetry
	{
		double Dt = 1.0 / 30.0;
		FSchoolV2 Pos;
		double HeadingDeg = 0.0;    // any steady convention: only its changes are read
		double Speed = 0.0;         // along the bow, m/s (negative: making sternway)
		double TwaDeg = 0.0;        // the true wind's angle off the bow, signed by the tack (+ wind over the starboard side), -180..180
		bool bCapsized = false;
		bool bHasLeader = false;
		FSchoolV2 Leader;           // the coach's launch (the follow objective)
		bool bHasTarget = false;
		FSchoolV2 Target;           // the active objective's point: the buoy to stop at or pick up, the landing place
		double RaceT = 0.0;         // seconds from the gun (the start objective)
	};

	enum class ELessonEvent : uint8_t { None, Progress, ObjectiveDone, LessonDone, Irons, Tack, Gybe, Capsized, Righted, NextTarget, BadStart, Touch, WrongSide };

	/** A lesson in progress: the objectives are worked through in order (Active); Step reads the boat. A course objective
	 *  needs SetCourse first (RaceSetCourse with the lesson's area: the engine lays the buoys there). */
	struct FLessonRun
	{
		int Lesson = -1;
		int Active = 0;
		std::vector<double> Progress;       // per objective: seconds held, things counted
		bool bComplete = false;
		FRaceCourse Course;
		FRaceTracker Tracker;
		bool bCourseSet = false;
		// the boat's reading
		int TackSide = 0;                   // +1 starboard, -1 port, 0 not known yet
		double IronsT = 0.0, HoldT = 0.0, CapsizedT = 0.0, SinceTurn = 1e9;
		bool bIrons = false, bWasCapsized = false, bTurnIsGybe = false, bTurnPending = false, bTurnSpoilt = false, bInTarget = false;

		void Begin(const FSchoolParams& P, int InLesson)
		{
			*this = FLessonRun();
			Lesson = InLesson;
			if (Lesson >= 0 && Lesson < (int)P.Lessons.size()) Progress.assign(P.Lessons[(size_t)Lesson].Objectives.size(), 0.0);
			bComplete = Progress.empty();
		}

		const FLessonObjective* Objective(const FSchoolParams& P) const
		{
			return Lesson >= 0 && Lesson < (int)P.Lessons.size() && Active < (int)P.Lessons[(size_t)Lesson].Objectives.size()
				? &P.Lessons[(size_t)Lesson].Objectives[(size_t)Active] : nullptr;
		}

		/** What the active objective wants (its count, its seconds; 1 for a single deed). */
		static double Wanted(const FLessonObjective& O)
		{
			if (O.Kind == "hold" || O.Kind == "follow" || O.Kind == "astern" || O.Kind == "stop") return O.Seconds;
			if (O.Kind == "event" || O.Kind == "tacks" || O.Kind == "gybes" || O.Kind == "right" || O.Kind == "pickup") return O.Count;
			return 1.0;
		}

		void SetCourse(const FRaceCourse& C, bool bWithStart)
		{
			Course = C;
			bCourseSet = true;
			if (bWithStart) Tracker.Reset(); else Tracker.ResetFree();
		}

		/** The start objective again (he was over, or late): the engine runs the sequence once more. */
		void RestartStart() { Tracker.Reset(); }

		void Advance(const FSchoolParams& P, std::vector<ELessonEvent>& Ev)
		{
			Ev.push_back(ELessonEvent::ObjectiveDone);
			Active++;
			HoldT = 0.0;
			bInTarget = false;
			bCourseSet = false;
			if (Active >= (int)P.Lessons[(size_t)Lesson].Objectives.size())
			{
				bComplete = true;
				Ev.push_back(ELessonEvent::LessonDone);
			}
		}

		/** Something done ashore (the engine's own little games): "knot", "parts", "rig". */
		std::vector<ELessonEvent> Notify(const FSchoolParams& P, const std::string& What)
		{
			std::vector<ELessonEvent> Ev;
			const FLessonObjective* O = Objective(P);
			if (bComplete || !O || O->Kind != "event" || O->Event != What)
			{
				return Ev;
			}
			Progress[(size_t)Active] += 1.0;
			Ev.push_back(ELessonEvent::Progress);
			if (Progress[(size_t)Active] >= O->Count) Advance(P, Ev);
			return Ev;
		}

		std::vector<ELessonEvent> Step(const FSchoolParams& P, const FLessonTelemetry& T)
		{
			std::vector<ELessonEvent> Ev;
			if (bComplete || Lesson < 0)
			{
				return Ev;
			}
			const double Dt = SchoolClamp(T.Dt, 0.0, 0.5);
			const double Twa = std::fabs(T.TwaDeg);
			// the boat's own events: a capsize and its righting, lying in irons, a tack or a gybe
			bool bRighted = false, bTack = false, bGybe = false;
			double CapsizedFor = 0.0;
			if (T.bCapsized)
			{
				if (!bWasCapsized) { Ev.push_back(ELessonEvent::Capsized); CapsizedT = 0.0; bTurnSpoilt = true; }
				CapsizedT += Dt;
			}
			else if (bWasCapsized)
			{
				bRighted = true;
				CapsizedFor = CapsizedT;
				Ev.push_back(ELessonEvent::Righted);
			}
			bWasCapsized = T.bCapsized;
			if (!T.bCapsized && std::fabs(T.Speed) < P.IronsSpeed && Twa < P.IronsTwa)
			{
				IronsT += Dt;
				if (IronsT >= P.IronsS && !bIrons) { bIrons = true; bTurnSpoilt = true; Ev.push_back(ELessonEvent::Irons); }
			}
			else
			{
				IronsT = 0.0;
				if (std::fabs(T.Speed) > P.IronsSpeed * 2.0 && Twa > P.IronsTwa) bIrons = false;
			}
			SinceTurn += Dt;
			if (!T.bCapsized && Twa > 8.0 && Twa < 172.0)
			{
				const int Side = T.TwaDeg > 0.0 ? 1 : -1;
				if (TackSide != 0 && Side != TackSide)
				{
					bTurnPending = true;
					bTurnIsGybe = Twa > 90.0;
					bTurnSpoilt = bIrons;       // a tack that hung in irons on the way does not count
					SinceTurn = 0.0;
				}
				TackSide = Side;
			}
			if (bTurnPending)
			{
				// a turn counts once the boat is sailing again on the new tack; one that ends in irons or upside down does not
				if (bTurnSpoilt || SinceTurn > 15.0) bTurnPending = false;
				else if (SinceTurn >= (bTurnIsGybe ? 3.0 : 0.0) && std::fabs(T.Speed) > P.IronsSpeed * 2.0 && Twa > P.IronsTwa)
				{
					bTurnPending = false;
					bTack = !bTurnIsGybe;
					bGybe = bTurnIsGybe;
					Ev.push_back(bTack ? ELessonEvent::Tack : ELessonEvent::Gybe);
				}
			}
			const FLessonObjective* O = Objective(P);
			if (!O)
			{
				return Ev;
			}
			double& Pr = Progress[(size_t)Active];
			const double Before = Pr;
			bool bDone = false;
			if (O->Kind == "hold")
			{
				if (!T.bCapsized && Twa >= O->Twa[0] && Twa <= O->Twa[1] && T.Speed > P.IronsSpeed) HoldT += Dt; else HoldT = 0.0;
				Pr = HoldT;
				bDone = HoldT >= O->Seconds;
			}
			else if (O->Kind == "follow")
			{
				if (T.bHasLeader && !T.bCapsized && (T.Pos - T.Leader).Len() <= P.FollowM && T.Speed > P.IronsSpeed) Pr += Dt;
				bDone = Pr >= O->Seconds;
			}
			else if (O->Kind == "tacks")
			{
				if (bTack) Pr += 1.0;
				bDone = Pr >= O->Count;
			}
			else if (O->Kind == "gybes")
			{
				if (bGybe) Pr += 1.0;
				bDone = Pr >= O->Count;
			}
			else if (O->Kind == "right")
			{
				if (bRighted && (O->WithinS <= 0.0 || CapsizedFor <= O->WithinS)) Pr += 1.0;
				bDone = Pr >= O->Count;
			}
			else if (O->Kind == "astern")
			{
				if (!T.bCapsized && T.Speed < -P.AsternSpeed) HoldT += Dt; else HoldT = 0.0;
				Pr = HoldT;
				bDone = HoldT >= O->Seconds;
			}
			else if (O->Kind == "stop")
			{
				if (T.bHasTarget && !T.bCapsized && (T.Pos - T.Target).Len() <= O->RadiusM && std::fabs(T.Speed) <= O->MaxSpeed) HoldT += Dt; else HoldT = 0.0;
				Pr = HoldT;
				bDone = HoldT >= O->Seconds;
			}
			else if (O->Kind == "pickup")
			{
				const double D = T.bHasTarget ? (T.Pos - T.Target).Len() : 1e9;
				if (!bInTarget && !T.bCapsized && D <= O->RadiusM && std::fabs(T.Speed) <= O->MaxSpeed)
				{
					bInTarget = true;
					Pr += 1.0;
					if (Pr < O->Count) Ev.push_back(ELessonEvent::NextTarget);     // the coach throws the buoy somewhere else
				}
				else if (D > O->RadiusM * 4.0) bInTarget = false;
				bDone = Pr >= O->Count;
			}
			else if (O->Kind == "land")
			{
				bDone = T.bHasTarget && !T.bCapsized && (T.Pos - T.Target).Len() <= O->RadiusM && std::fabs(T.Speed) <= O->MaxSpeed;
				Pr = bDone ? 1.0 : 0.0;
			}
			else if ((O->Kind == "course" || O->Kind == "start") && bCourseSet)
			{
				for (const ERaceEvent E : Tracker.Step(P, Course, T.RaceT, T.Pos, T.HeadingDeg))
				{
					if (E == ERaceEvent::Touch) Ev.push_back(ELessonEvent::Touch);
					else if (E == ERaceEvent::WrongSide || E == ERaceEvent::Missed) Ev.push_back(ELessonEvent::WrongSide);
					else if (E == ERaceEvent::Ocs || E == ERaceEvent::Dns) Ev.push_back(ELessonEvent::BadStart);
					else if (E == ERaceEvent::Started && O->Kind == "start")
					{
						if (Tracker.StartT <= O->WithinS) bDone = true; else Ev.push_back(ELessonEvent::BadStart);
					}
					else if (E == ERaceEvent::Finished && O->Kind == "course") bDone = Tracker.Status == ERaceStatus::Finished;
				}
				if (O->Kind == "course") Pr = Course.Order.empty() ? 0.0 : (double)Tracker.Next / (double)Course.Order.size();
				if (bDone) Pr = 1.0;
			}
			if (Pr != Before && !bDone && (O->Kind == "tacks" || O->Kind == "gybes" || O->Kind == "right" || O->Kind == "pickup")) Ev.push_back(ELessonEvent::Progress);
			if (bDone) Advance(P, Ev);
			return Ev;
		}
	};
}
