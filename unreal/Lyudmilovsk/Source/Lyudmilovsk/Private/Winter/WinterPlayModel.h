// Winter play's rules in plain C++ (docs/winter_play.md): the pond's ice and its cracks, the hero on skates (a glide with
// momentum, a stroke at a time, turns held by the edge, a hockey stop, falls on cracks), the banya (the steam room's heat
// and steam, the body's heat, rounds of steam then snow or the ice hole), the cold of the winter sea, the walruses' hours,
// snow that packs, a snowball's flight and the children's aim, a snowman's balls. No engine types: ULyudWinterPlay drives it
// and tools/winterplaysim checks it outside the engine on the real Content/City/winter_play.json.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "../People/MiniJson.h"

namespace LyudWinterPlay
{
	constexpr double Pi = 3.14159265358979323846;

	struct V2
	{
		double X = 0.0, Y = 0.0;
		V2() = default;
		V2(double InX, double InY) : X(InX), Y(InY) {}
		V2 operator+(const V2& O) const { return V2(X + O.X, Y + O.Y); }
		V2 operator-(const V2& O) const { return V2(X - O.X, Y - O.Y); }
		V2 operator*(double S) const { return V2(X * S, Y * S); }
		double Dot(const V2& O) const { return X * O.X + Y * O.Y; }
		double Cross(const V2& O) const { return X * O.Y - Y * O.X; }
		double Len() const { return std::sqrt(X * X + Y * Y); }
		V2 Norm() const { const double L = Len(); return L > 1e-9 ? V2(X / L, Y / L) : V2(); }
		V2 Rot(double A) const { const double C = std::cos(A), S = std::sin(A); return V2(X * C - Y * S, X * S + Y * C); }
	};

	struct V3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		V3() = default;
		V3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
		V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
		V3 operator*(double S) const { return V3(X * S, Y * S, Z * S); }
		double Len() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		double Len2D() const { return std::sqrt(X * X + Y * Y); }
	};

	inline double Clamp(double V, double A, double B) { return V < A ? A : (V > B ? B : V); }
	inline double Lerp(double A, double B, double T) { return A + (B - A) * T; }

	/** A small deterministic random stream (xorshift32). */
	struct FRng
	{
		uint32_t S = 2463534242u;
		explicit FRng(uint32_t Seed = 2463534242u) : S(Seed ? Seed : 2463534242u) {}
		uint32_t Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
		double U() { return (Next() & 0xFFFFFF) / double(0x1000000); }
		double Range(double A, double B) { return A + (B - A) * U(); }
	};

	inline uint32_t Hash(uint32_t A, uint32_t B = 0x9E3779B9u)
	{
		uint32_t H = A * 0x85EBCA6Bu ^ (B + 0x9E3779B9u + (A << 6) + (A >> 2));
		H ^= H >> 16; H *= 0x7FEB352Du; H ^= H >> 15; H *= 0x846CA68Bu; H ^= H >> 16;
		return H ? H : 1u;
	}

	/** Days in a month (no leap years matter here but February's 29th is accepted). */
	inline int DateKey(int Month, int Day) { return Month * 100 + Day; }

	// ------------------------------------------------------------------------------------------------------------------
	/** Every number of Content/City/winter_play.json the rules use. */
	struct FParams
	{
		// the rink
		V3 RinkAt; double RinkRx = 14.0, RinkRy = 22.0, RinkYaw = 0.0;
		std::vector<V3> RinkLoop;
		// the banya and the hole (positions the engine places them by; metres)
		V3 BanyaAt; double BanyaYaw = 0.0; double BanyaSize[3] = { 4.6, 3.6, 2.3 };
		double Door[3] = { 2.3, -0.7, 0.0 };
		double DressC[3] = { 1.25, 0.0, 1.1 }, DressHalf[3] = { 1.0, 1.7, 1.1 };
		double SteamC[3] = { -1.1, 0.0, 1.1 }, SteamHalf[3] = { 1.1, 1.7, 1.1 }, Shelf[3] = { -1.75, 0.0, 0.9 }, Stove[3] = { -0.5, 1.2, 0.0 };
		V3 HoleAt; double HoleR = 1.3, HoleBedZ = -45.0; V3 LadderAt; double LadderYaw = 90.0, DeckZ = -41.5, LadderTopBack = 1.2, DeckEdgeY = 0.0; V3 DeckStand;
		// ice
		int IceFrom[2] = { 12, 15 }, IceTo[2] = { 3, 15 };
		double FddOffset = 12.0, StefanCm = 2.4, FirstCm = 4.0, MaxCm = 42.0, OpenCm = 10.0, ThickCm = 20.0;
		double ThawMelt = 0.35, ThawTempC = 1.5, SlushHours = 30.0;
		int CracksThin = 9, CracksThick = 3; double CrackLen[2] = { 5.0, 15.0 }, CrackWidth = 0.5;
		// skating
		double StrokeS = 0.85, Push = 2.6, PushShift = 1.3, MaxSpeed = 5.6, MaxSpeedShift = 2.6;
		double GlideDecel = 0.07, Drag = 0.014, Grip = 3.8, TurnRateDeg = 140.0, Brake = 3.4, StopDot = -0.45;
		double CrackSpeed = 2.3, CrackChance = 0.55, TurnFall = 1.35, FallS = 1.5, GetupS = 1.1, FallSlideDecel = 2.6;
		double BootsAccel = 1.5, BootsDecel = 1.1, BootsMax = 3.0, BootsCrackSpeed = 3.4, BootsCrackChance = 0.25;
		double LandScale = 0.42, KerbPutOnM = 4.5, LaceS = 4.0;
		// the banya
		int BanyaDays = (1 << 2) | (1 << 4) | (1 << 5) | (1 << 6); double BanyaHours[2] = { 14.0, 22.0 };
		std::vector<std::pair<int, std::pair<double, double>>> BanyaDates;     // MMDD -> hours
		int BanyaPrice = 20;
		double RoomC = 78.0, RoomMaxC = 96.0, RoomCoolPerS = 0.03, LadleC = 4.0, LadleSteam = 0.2, SteamDecayPerS = 0.012;
		double SteamBiteC = 26.0, HatC = 9.0, HeatFromC = 42.0, HeatPerCS = 0.00012, VenikHeat = 0.025, VenikS = 1.1;
		double TooHot = 1.0, OutHot = 1.12, CoolAir = 0.006, CoolAirFrost = 0.00025, CoolSnow = 0.05, CoolWater = 0.085;
		double RoundHot = 0.75, RoundCool = 0.3, RoundWindowS = 150.0; int RoundsFull = 3;
		// the cold sea and the walruses
		int WalrusMonths = (1 << 12) | (1 << 1) | (1 << 2) | (1 << 3);
		double ColdSeaC = 9.0, ColdPerS = 0.02, WarmAirPerS = 0.004, TowelMult = 3.0, CountS = 15.0, OutCold = 1.3;
		double SundayHours[2] = { 8.5, 11.0 }; int NewYear = 101; double NewYearHours[2] = { 8.0, 13.0 };
		int ClubMin = 2; double ClubRadiusM = 12.0;
		// snowballs and the children's fight
		double PackCm = 3.0, PackTemp[2] = { -7.0, 1.8 }, ScoopS = 0.5, CooldownS = 0.8, BallSpeed = 17.0, UpDeg = 7.0;
		double BallRadius = 0.045, HitM = 0.45, Gravity = 9.8, BallLife = 4.0;
		int KidAges[2] = { 6, 15 }; double JoinM = 14.0, FightRadius = 26.0; int MaxKids = 4;
		double ThrowEvery[2] = { 2.2, 4.6 }, AimErrDeg = 5.0, AimErrPerM = 0.35, KidSpeed = 13.0, FightS = 120.0, LeaveM = 40.0;
		// snowmen
		double ManMinCm = 5.0, StartR = 0.12, Pack = 0.55, MaxR = 0.55, LiftR = 0.34, Ratio = 0.9, PushM = 0.75;
		double MeltPerH = 0.012; int MaxSaved = 8; double NearM = 1.6;
		// the shops and stamps
		std::vector<std::string> StampPieces;
		std::string StampSet = "winter";
		struct FGood { std::string Item; int Price = 0, Count = 1; };
		std::vector<FGood> BanyaShop;
		std::string RadioOpen, RadioThin, RadioSlush, RadioWalrus, RadioNewYear;

		bool Load(const std::string& Text, std::string& Error)
		{
			LyudJson::FValue J;
			if (!LyudJson::Parse(Text, J, Error))
			{
				return false;
			}
			auto Vec = [](const LyudJson::FValue& A, V3 Def) { return A.Size() >= 3 ? V3(A[0].F(), A[1].F(), A[2].F()) : Def; };
			auto Arr = [](const LyudJson::FValue& A, double* Out, int N) { for (int I = 0; I < N && I < (int)A.Size(); I++) Out[I] = A[I].F((float)Out[I]); };
			auto Num = [](const LyudJson::FValue& O, const char* K, double& Out) { if (O[K].Type == LyudJson::FValue::Number) Out = O[K].Num; };
			auto Int = [](const LyudJson::FValue& O, const char* K, int& Out) { if (O[K].Type == LyudJson::FValue::Number) Out = (int)O[K].Num; };
			auto Mask = [](const LyudJson::FValue& A, int& Out) { if (A.Size()) { Out = 0; for (const auto& V : A.Items) Out |= 1 << V.I(); } };
			auto MonthDay = [](const std::string& S) { return S.size() == 5 ? std::atoi(S.substr(0, 2).c_str()) * 100 + std::atoi(S.substr(3, 2).c_str()) : -1; };

			const LyudJson::FValue& R = J["rink"];
			if (R.IsNull() || J["banya"].IsNull() || J["ice_hole"].IsNull())
			{
				Error = "winter_play.json: no rink, banya or ice_hole";
				return false;
			}
			RinkAt = Vec(R["at"], RinkAt);
			RinkRx = R["r"][0].F((float)RinkRx);
			RinkRy = R["r"][1].F((float)RinkRy);
			RinkYaw = R["yaw"].F(0.f);
			RinkLoop.clear();
			for (const auto& P : R["loop"].Items) RinkLoop.push_back(Vec(P, V3()));
			const LyudJson::FValue& B = J["banya"];
			BanyaAt = Vec(B["at"], BanyaAt);
			BanyaYaw = B["yaw"].F(0.f);
			Arr(B["size"], BanyaSize, 3);
			Arr(B["door"], Door, 3);
			Arr(B["dressing_room"]["c"], DressC, 3);
			Arr(B["dressing_room"]["half"], DressHalf, 3);
			Arr(B["steam_room"]["c"], SteamC, 3);
			Arr(B["steam_room"]["half"], SteamHalf, 3);
			Arr(B["steam_room"]["shelf"], Shelf, 3);
			Arr(B["steam_room"]["stove"], Stove, 3);
			const LyudJson::FValue& H = J["ice_hole"];
			HoleAt = Vec(H["at"], HoleAt);
			HoleR = H["r"].F((float)HoleR);
			HoleBedZ = H["bed_z"].F((float)HoleBedZ);
			LadderAt = Vec(H["ladder"]["at"], LadderAt);
			LadderYaw = H["ladder"]["yaw"].F((float)LadderYaw);
			DeckZ = H["ladder"]["deck_z"].F((float)DeckZ);
			LadderTopBack = H["ladder"]["top_back_m"].F((float)LadderTopBack);
			DeckEdgeY = H["ladder"]["deck_edge_y"].F((float)DeckEdgeY);
			DeckStand = Vec(H["deck_stand"], DeckStand);

			const LyudJson::FValue& I = J["ice"];
			IceFrom[0] = I["from"][0].I(IceFrom[0]); IceFrom[1] = I["from"][1].I(IceFrom[1]);
			IceTo[0] = I["to"][0].I(IceTo[0]); IceTo[1] = I["to"][1].I(IceTo[1]);
			Num(I, "fdd_offset", FddOffset); Num(I, "stefan_cm", StefanCm); Num(I, "first_cm", FirstCm); Num(I, "max_cm", MaxCm);
			Num(I, "open_cm", OpenCm); Num(I, "thick_cm", ThickCm); Num(I, "thaw_melt", ThawMelt); Num(I, "thaw_temp_c", ThawTempC);
			Num(I, "slush_hours", SlushHours); Int(I, "cracks_thin", CracksThin); Int(I, "cracks_thick", CracksThick);
			Arr(I["crack_len_m"], CrackLen, 2); Num(I, "crack_width_m", CrackWidth);

			const LyudJson::FValue& S = J["skating"];
			Num(S, "stroke_s", StrokeS); Num(S, "push", Push); Num(S, "push_shift", PushShift); Num(S, "max_speed", MaxSpeed);
			Num(S, "max_speed_shift", MaxSpeedShift); Num(S, "glide_decel", GlideDecel); Num(S, "drag", Drag); Num(S, "grip", Grip);
			Num(S, "turn_rate_deg", TurnRateDeg); Num(S, "brake", Brake); Num(S, "stop_dot", StopDot); Num(S, "crack_speed", CrackSpeed);
			Num(S, "crack_chance", CrackChance); Num(S, "turn_fall", TurnFall); Num(S, "fall_s", FallS); Num(S, "getup_s", GetupS);
			Num(S, "fall_slide_decel", FallSlideDecel); Num(S, "boots_accel", BootsAccel); Num(S, "boots_decel", BootsDecel);
			Num(S, "boots_max", BootsMax); Num(S, "boots_crack_speed", BootsCrackSpeed); Num(S, "boots_crack_chance", BootsCrackChance);
			Num(S, "land_scale", LandScale); Num(S, "kerb_put_on_m", KerbPutOnM); Num(S, "lace_s", LaceS);

			const LyudJson::FValue& BR = J["banya_rules"];
			Mask(BR["days"], BanyaDays);
			Arr(BR["hours"], BanyaHours, 2);
			BanyaDates.clear();
			for (const auto& D : BR["dates"].Fields)
			{
				BanyaDates.push_back({ MonthDay(D.first), { D.second[0].F(0.f), D.second[1].F(0.f) } });
			}
			Int(BR, "price", BanyaPrice); Num(BR, "room_c", RoomC); Num(BR, "room_max_c", RoomMaxC); Num(BR, "room_cool_per_s", RoomCoolPerS);
			Num(BR, "ladle_c", LadleC); Num(BR, "ladle_steam", LadleSteam); Num(BR, "steam_decay_per_s", SteamDecayPerS);
			Num(BR, "steam_bite_c", SteamBiteC); Num(BR, "hat_c", HatC); Num(BR, "heat_from_c", HeatFromC); Num(BR, "heat_per_c_s", HeatPerCS);
			Num(BR, "venik_heat", VenikHeat); Num(BR, "venik_s", VenikS); Num(BR, "too_hot", TooHot); Num(BR, "out_hot", OutHot);
			Num(BR, "cool_air", CoolAir); Num(BR, "cool_air_frost", CoolAirFrost); Num(BR, "cool_snow", CoolSnow); Num(BR, "cool_water", CoolWater);
			Num(BR, "round_hot", RoundHot); Num(BR, "round_cool", RoundCool); Num(BR, "round_window_s", RoundWindowS); Int(BR, "rounds_full", RoundsFull);

			const LyudJson::FValue& W = J["walrus"];
			Mask(W["months"], WalrusMonths);
			Num(W, "cold_sea_c", ColdSeaC); Num(W, "cold_per_s", ColdPerS); Num(W, "warm_air_per_s", WarmAirPerS); Num(W, "towel_mult", TowelMult);
			Num(W, "count_s", CountS); Num(W, "out_cold", OutCold); Arr(W["sunday_hours"], SundayHours, 2);
			if (W["new_year"].Type == LyudJson::FValue::String) NewYear = MonthDay(W["new_year"].Str);
			Arr(W["new_year_hours"], NewYearHours, 2); Int(W, "club_min", ClubMin); Num(W, "club_radius_m", ClubRadiusM);

			const LyudJson::FValue& SB = J["snowball"];
			Num(SB, "pack_cm", PackCm); Arr(SB["temp_c"], PackTemp, 2); Num(SB, "scoop_s", ScoopS); Num(SB, "cooldown_s", CooldownS);
			Num(SB, "speed", BallSpeed); Num(SB, "up_deg", UpDeg); Num(SB, "radius_m", BallRadius); Num(SB, "hit_m", HitM);
			Num(SB, "gravity", Gravity); Num(SB, "life_s", BallLife);
			const LyudJson::FValue& FI = J["fight"];
			KidAges[0] = FI["ages"][0].I(KidAges[0]); KidAges[1] = FI["ages"][1].I(KidAges[1]);
			Num(FI, "join_m", JoinM); Num(FI, "radius_m", FightRadius); Int(FI, "max_kids", MaxKids); Arr(FI["throw_every_s"], ThrowEvery, 2);
			Num(FI, "aim_error_deg", AimErrDeg); Num(FI, "aim_error_per_m", AimErrPerM); Num(FI, "speed", KidSpeed);
			Num(FI, "duration_s", FightS); Num(FI, "leave_m", LeaveM);
			const LyudJson::FValue& SM = J["snowman"];
			Num(SM, "min_cm", ManMinCm); Num(SM, "start_r", StartR); Num(SM, "pack", Pack); Num(SM, "max_r", MaxR); Num(SM, "lift_r", LiftR);
			Num(SM, "ratio", Ratio); Num(SM, "push_m", PushM); Num(SM, "melt_per_h", MeltPerH); Int(SM, "max_saved", MaxSaved); Num(SM, "near_m", NearM);

			StampSet = J["stamps"]["set"].S("winter");
			StampPieces.clear();
			for (const auto& P : J["stamps"]["pieces"].Items) StampPieces.push_back(P.S());
			BanyaShop.clear();
			for (const auto& G : J["banya_shop"].Items) BanyaShop.push_back({ G[0].S(), G[1].I(), G[2].I(1) });
			const LyudJson::FValue& RA = J["radio"];
			RadioOpen = RA["open"].S(); RadioThin = RA["thin"].S(); RadioSlush = RA["slush"].S();
			RadioWalrus = RA["walrus"].S(); RadioNewYear = RA["new_year"].S();
			return true;
		}

		bool HasPiece(const std::string& P) const
		{
			for (const std::string& S : StampPieces) if (S == P) return true;
			return false;
		}
	};

	// ------------------------------------------------------------------------------------------------------------------
	// The calendar: the banya's hours, the walruses', the pond's freeze window

	/** Weekday 0 = Monday. Hour local, fractional. */
	inline bool BanyaOpen(const FParams& P, int Month, int Day, int Weekday, double Hour)
	{
		const int Key = DateKey(Month, Day);
		for (const auto& D : P.BanyaDates)
		{
			if (D.first == Key)
			{
				return Hour >= D.second.first && Hour < D.second.second;
			}
		}
		return (P.BanyaDays >> Weekday & 1) && Hour >= P.BanyaHours[0] && Hour < P.BanyaHours[1];
	}

	inline bool WalrusSeason(const FParams& P, int Month) { return (P.WalrusMonths >> Month & 1) != 0; }

	inline bool IsNewYearMorning(const FParams& P, int Month, int Day, double Hour)
	{
		return DateKey(Month, Day) == P.NewYear && Hour >= P.NewYearHours[0] && Hour < P.NewYearHours[1];
	}

	/** The walruses swim now (the club on Sunday mornings in season; everybody on New Year's morning). */
	inline bool WalrusHour(const FParams& P, int Month, int Day, int Weekday, double Hour)
	{
		if (IsNewYearMorning(P, Month, Day, Hour))
		{
			return true;
		}
		return WalrusSeason(P, Month) && Weekday == 6 && Hour >= P.SundayHours[0] && Hour < P.SundayHours[1];
	}

	/** The pond's calendar ice (mid December to mid March: ALyudPublicSpaces draws it so). */
	inline bool PondFrozenOnCalendar(const FParams& P, int Month, int Day)
	{
		const int K = DateKey(Month, Day), From = DateKey(P.IceFrom[0], P.IceFrom[1]), To = DateKey(P.IceTo[0], P.IceTo[1]);
		return From > To ? (K >= From || K < To) : (K >= From && K < To);
	}

	/** The winter a date belongs to (its autumn's year: 15 Dec 2026 and 3 Feb 2027 are both 2026's). */
	inline int WinterOf(int Year, int Month) { return Month >= 7 ? Year : Year - 1; }

	// ------------------------------------------------------------------------------------------------------------------
	// The pond's ice

	struct FCrack
	{
		V2 A, B;            // in the pond's frame (metres from its centre, x along its yaw)
		double Width = 0.5;
	};

	struct FIceIn
	{
		int Month = 1, Day = 1;
		double FreezingDegreeDays = 0.0;    // the weather's net of this winter
		double TempC = -5.0;
		double Melt = 0.0;                  // 0..1 the weather's melting now
		int Force = -1;                     // lyud.City.Ice: -1 the calendar, 0 water, 1 ice
	};

	struct FIce
	{
		bool bFrozen = false;
		double Cm = 0.0;
		double Slush = 0.0;                 // 0..1 water over the ice
		bool bOpen = false;                 // skating allowed (thick enough, no slush)
	};

	/** The pond's ice, remembering how long a thaw has gone on (water lies on the ice after a day of it). */
	class FIceState
	{
	public:
		double ThawHours = 0.0;

		FIce Step(const FParams& P, const FIceIn& In, double Hours)
		{
			FIce I;
			I.bFrozen = In.Force == 1 || (In.Force < 0 && PondFrozenOnCalendar(P, In.Month, In.Day));
			if (!I.bFrozen)
			{
				ThawHours = 0.0;
				return I;
			}
			const double F = std::max(0.0, In.FreezingDegreeDays - P.FddOffset);
			I.Cm = std::min(P.MaxCm, P.FirstCm + P.StefanCm * std::sqrt(F));
			if (In.Force == 1)
			{
				I.Cm = std::max(I.Cm, P.ThickCm);
			}
			const bool bThaw = In.TempC > P.ThawTempC && In.Melt > P.ThawMelt;
			ThawHours = bThaw ? ThawHours + Hours : std::max(0.0, ThawHours - Hours * 2.0);
			I.Slush = Clamp(ThawHours / std::max(1.0, P.SlushHours), 0.0, 1.0);
			I.bOpen = I.Cm >= P.OpenCm && I.Slush < 0.5;
			return I;
		}
	};

	/** The cracks of a winter's ice: fewer and narrower as it thickens; the same every time for the same winter and thickness band. */
	inline std::vector<FCrack> Cracks(const FParams& P, int Winter, double Cm)
	{
		std::vector<FCrack> Out;
		if (Cm <= 0.0)
		{
			return Out;
		}
		const double T = Clamp((Cm - P.OpenCm) / std::max(1.0, P.ThickCm - P.OpenCm), 0.0, 1.0);
		const int N = (int)std::lround(Lerp(P.CracksThin, P.CracksThick, T));
		FRng R(Hash((uint32_t)Winter, 0xC4AC5u));
		for (int I = 0; I < std::max(P.CracksThin, N); I++)
		{
			// a point inside the oval, a direction, a length; the first N kept (so a thinner ice has these and more)
			const double A = R.Range(0.0, 2.0 * Pi), Rr = std::sqrt(R.U()) * 0.8;
			const V2 C(std::cos(A) * P.RinkRx * Rr, std::sin(A) * P.RinkRy * Rr);
			const double D = R.Range(0.0, Pi), L = R.Range(P.CrackLen[0], P.CrackLen[1]);
			const V2 U(std::cos(D), std::sin(D));
			const double W = P.CrackWidth * Lerp(1.0, 0.5, T) * R.Range(0.6, 1.2);
			if (I < N)
			{
				Out.push_back({ C - U * (L * 0.5), C + U * (L * 0.5), W });
			}
		}
		return Out;
	}

	inline double DistToSegment(const V2& P, const V2& A, const V2& B)
	{
		const V2 Ab = B - A;
		const double T = Clamp((P - A).Dot(Ab) / std::max(1e-9, Ab.Dot(Ab)), 0.0, 1.0);
		return (P - (A + Ab * T)).Len();
	}

	/** Whether the way from From to To (pond frame) crosses a crack. */
	inline bool CrossesCrack(const std::vector<FCrack>& Cr, const V2& From, const V2& To)
	{
		for (const FCrack& C : Cr)
		{
			// the segments intersect, or the end point lies on the crack's width
			const V2 R = To - From, S = C.B - C.A;
			const double Den = R.Cross(S);
			if (std::fabs(Den) > 1e-9)
			{
				const double T = (C.A - From).Cross(S) / Den, U = (C.A - From).Cross(R) / Den;
				if (T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0)
				{
					return true;
				}
			}
			if (DistToSegment(To, C.A, C.B) < C.Width * 0.5 && DistToSegment(From, C.A, C.B) >= C.Width * 0.5)
			{
				return true;
			}
		}
		return false;
	}

	/** World metres to the pond's frame and back (the rink's yaw). */
	inline V2 ToPond(const FParams& P, double X, double Y)
	{
		return V2(X - P.RinkAt.X, Y - P.RinkAt.Y).Rot(-P.RinkYaw * Pi / 180.0);
	}
	inline bool OnPond(const FParams& P, double X, double Y, double Margin = 0.0)
	{
		const V2 L = ToPond(P, X, Y);
		const double Ax = P.RinkRx - Margin, Ay = P.RinkRy - Margin;
		return Ax > 0.0 && Ay > 0.0 && (L.X * L.X) / (Ax * Ax) + (L.Y * L.Y) / (Ay * Ay) <= 1.0;
	}
	/** How far outside the kerb a point is (negative inside), roughly, m. */
	inline double FromKerb(const FParams& P, double X, double Y)
	{
		const V2 L = ToPond(P, X, Y);
		const double K = std::sqrt((L.X * L.X) / (P.RinkRx * P.RinkRx) + (L.Y * L.Y) / (P.RinkRy * P.RinkRy));
		return (K - 1.0) * std::min(P.RinkRx, P.RinkRy);
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Skating

	enum class ESkateEvent : uint8_t { None, Stroke, Crack, Fall, GetUp, Stop };

	struct FSkateIn
	{
		V2 Wish;                // the move keys in the world (unit, or zero)
		bool bShift = false;    // easy: short strokes, slow
		bool bSkates = true;    // skates on (else boots on ice)
		bool bCrack = false;    // crossed a crack this step
		double Dt = 1.0 / 60.0;
	};

	struct FSkater
	{
		V2 Vel;
		double StrokeT = 0.0;           // time into the current stroke (a stroke runs while a key is held)
		bool bStroking = false;
		bool bDown = false;
		double DownT = 0.0;
		double GlideM = 0.0;            // metres glided since the last push (the long glide)
		double BestGlideM = 0.0;
		double DistM = 0.0;
		double TurnHardT = 0.0;         // how long a turn past the grip has been asked for
		FRng Rng = FRng(77u);

		double Speed() const { return Vel.Len(); }
		void Reset() { *this = FSkater(); }
	};

	/** One step of the hero on the ice. Returns what happened (a stroke began, a crack jolted him, he fell, got up, stopped). */
	inline ESkateEvent StepSkate(FSkater& K, const FSkateIn& In, const FParams& P)
	{
		const double Dt = Clamp(In.Dt, 0.0, 0.1);
		ESkateEvent Ev = ESkateEvent::None;
		double V = K.Vel.Len();
		if (K.bDown)
		{
			K.DownT += Dt;
			V = std::max(0.0, V - P.FallSlideDecel * Dt);
			K.Vel = K.Vel.Norm() * V;
			K.DistM += V * Dt;
			if (K.DownT >= P.FallS + P.GetupS)
			{
				K.bDown = false;
				K.DownT = 0.0;
				K.Vel = V2();
				return ESkateEvent::GetUp;
			}
			return ESkateEvent::None;
		}
		const bool bWish = In.Wish.Len() > 0.2;
		const V2 Wish = In.Wish.Norm();
		if (!In.bSkates)
		{
			// boots on ice: a slow slippery walk that keeps sliding a little after the keys are let go
			const V2 Want = bWish ? Wish * P.BootsMax * (In.bShift ? 0.55 : 1.0) : V2();
			const V2 D = Want - K.Vel;
			const double Rate = bWish ? P.BootsAccel : P.BootsDecel;
			const double Step = std::min(D.Len(), Rate * Dt);
			K.Vel = K.Vel + D.Norm() * Step;
			V = K.Vel.Len();
			if (In.bCrack && V > P.BootsCrackSpeed && K.Rng.U() < P.BootsCrackChance)
			{
				K.bDown = true;
				K.DownT = 0.0;
				return ESkateEvent::Fall;
			}
			K.DistM += V * Dt;
			return In.bCrack ? ESkateEvent::Crack : ESkateEvent::None;
		}

		const double MaxV = In.bShift ? P.MaxSpeedShift : P.MaxSpeed;
		const V2 Dir = V > 1e-3 ? K.Vel * (1.0 / V) : (bWish ? Wish : V2(1.0, 0.0));
		bool bPushed = false;
		if (bWish && V > 0.05 && Dir.Dot(Wish) < P.StopDot)
		{
			// a hockey stop: the blades across the way, a spray of ice (the last half metre a second is the stop itself)
			double Nv = V - P.Brake * Dt;
			if (Nv < 0.5)
			{
				Nv = 0.0;
			}
			K.Vel = Dir * Nv;
			K.bStroking = false;
			K.StrokeT = 0.0;
			K.GlideM = 0.0;
			if (Nv <= 0.0 && V > 0.0)
			{
				Ev = ESkateEvent::Stop;
			}
			V = Nv;
		}
		else if (bWish)
		{
			// steer: the way turns toward the wish, no faster than the edge holds (v * w <= grip) nor the turn rate
			if (V > 0.3)
			{
				const double Want = std::atan2(Dir.Cross(Wish), Dir.Dot(Wish));
				const double MaxW = std::min(P.TurnRateDeg * Pi / 180.0, P.Grip / std::max(V, 0.5));
				const double Turn = Clamp(Want, -MaxW * Dt, MaxW * Dt);
				K.Vel = K.Vel.Rot(Turn);
				// a turn sharper than the grip at speed: held long enough it throws him
				if (std::fabs(Want) > 1.2 && V > P.MaxSpeed * 0.75)
				{
					K.TurnHardT += Dt;
				}
				else
				{
					K.TurnHardT = std::max(0.0, K.TurnHardT - Dt);
				}
				if (K.TurnHardT > 0.35 * P.TurnFall)
				{
					K.TurnHardT = 0.0;
					K.bDown = true;
					K.DownT = 0.0;
					return ESkateEvent::Fall;
				}
			}
			// strokes: a push in the first half of each, along the wish
			if (!K.bStroking)
			{
				K.bStroking = true;
				K.StrokeT = 0.0;
				Ev = ESkateEvent::Stroke;
			}
			const double Phase = K.StrokeT / P.StrokeS;
			if (Phase < 0.5)
			{
				const double A = (In.bShift ? P.PushShift : P.Push) * (1.0 - Clamp(V / MaxV, 0.0, 1.0) * 0.7);
				if (V < MaxV)
				{
					K.Vel = K.Vel + Wish * (A * Dt);
					bPushed = true;
				}
			}
			K.StrokeT += Dt;
			if (K.StrokeT >= P.StrokeS)
			{
				K.StrokeT -= P.StrokeS;
				Ev = Ev == ESkateEvent::None ? ESkateEvent::Stroke : Ev;
			}
		}
		else
		{
			K.bStroking = false;
			K.StrokeT = 0.0;
		}
		// the glide: the blade's little friction and the air
		V = K.Vel.Len();
		const double Dec = (P.GlideDecel + P.Drag * V * V) * Dt;
		const double Nv = std::max(0.0, V - Dec);
		K.Vel = V > 1e-6 ? K.Vel * (Nv / V) : V2();
		if (Nv > MaxV + 0.02 && bPushed)
		{
			K.Vel = K.Vel * (MaxV / Nv);
		}
		V = K.Vel.Len();
		K.DistM += V * Dt;
		if (bPushed)
		{
			K.GlideM = 0.0;
		}
		else if (V > 1.0)
		{
			K.GlideM += V * Dt;
			K.BestGlideM = std::max(K.BestGlideM, K.GlideM);
		}
		if (In.bCrack && V > 0.8)
		{
			// a crack under the blade: a jolt; fast, it may throw him
			const double Chance = V > P.CrackSpeed ? Clamp(P.CrackChance * (0.5 + (V - P.CrackSpeed) / 2.0), 0.0, 0.95) : 0.0;
			if (K.Rng.U() < Chance)
			{
				K.bDown = true;
				K.DownT = 0.0;
				K.bStroking = false;
				return ESkateEvent::Fall;
			}
			return ESkateEvent::Crack;
		}
		return Ev;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// The banya and the cold water

	enum class EWhere : uint8_t { Outside, Dressing, Steam };
	enum class ECool : uint8_t { Air, Snow, Water };

	struct FRoom
	{
		double C = 78.0;        // the steam room's air
		double Steam = 0.0;     // 0..1 the last ladles' steam
		void Reset(const FParams& P) { C = P.RoomC; Steam = 0.0; }
		/** A ladle on the stones. */
		void Ladle(const FParams& P) { C = std::min(P.RoomMaxC, C + P.LadleC); Steam = std::min(1.0, Steam + P.LadleSteam); }
		/** How hot it feels on the upper shelf (the steam bites; the hat takes some off). */
		double Felt(const FParams& P, bool bHat) const { return C + Steam * P.SteamBiteC - (bHat ? P.HatC : 0.0); }
		void Step(const FParams& P, double Dt)
		{
			C += (P.RoomC - C) * std::min(1.0, P.RoomCoolPerS * Dt);
			Steam = std::max(0.0, Steam - P.SteamDecayPerS * Dt);
		}
	};

	enum class EBodyEvent : uint8_t { None, TooHot, MustLeave, Round, Cold, ColdOut, SwimCounted };

	struct FBody
	{
		double Heat = 0.0;          // 0 normal .. 1 hot through (and past)
		double Cold = 0.0;          // 0 .. 1 chilled through (and past)
		bool bHot = false;          // got hot enough for a round since the last
		double SinceSteam = 1e9;    // s since he left the steam room hot
		int Rounds = 0;
		ECool LastRoundBy = ECool::Air;
		bool bWarnedHot = false;
		bool bWarnedCold = false;
		double SwimS = 0.0;         // this time in the cold water
		bool bSwimCounted = false;
		double TowelT = 0.0;        // s of the towel's rub left
		void Reset() { *this = FBody(); }
	};

	/** One step of the body: where he is, what cools him (snow if he lies in it, the water if he swims in a cold sea),
	 *  the air's and the sea's temperatures. Returns at most one event (the most important). */
	inline EBodyEvent StepBody(FBody& B, const FRoom& Room, const FParams& P, EWhere Where, bool bHat, ECool Cool, double AirC,
		double SeaC, double Dt)
	{
		EBodyEvent Ev = EBodyEvent::None;
		const bool bColdWater = Cool == ECool::Water && SeaC <= P.ColdSeaC;
		if (Where == EWhere::Steam)
		{
			B.Heat += std::max(0.0, Room.Felt(P, bHat) - P.HeatFromC) * P.HeatPerCS * Dt;
			B.Cold = std::max(0.0, B.Cold - 0.03 * Dt);
			if (B.Heat >= P.RoundHot)
			{
				B.bHot = true;
			}
			B.SinceSteam = 0.0;
			if (B.Heat >= P.OutHot)
			{
				Ev = EBodyEvent::MustLeave;
			}
			else if (B.Heat >= P.TooHot && !B.bWarnedHot)
			{
				B.bWarnedHot = true;
				Ev = EBodyEvent::TooHot;
			}
		}
		else
		{
			B.bWarnedHot = B.bWarnedHot && B.Heat >= P.TooHot * 0.9;
			B.SinceSteam += Dt;
			double Rate = Where == EWhere::Dressing ? P.CoolAir * 0.5 : P.CoolAir + std::max(0.0, -AirC) * P.CoolAirFrost;
			if (Cool == ECool::Snow)
			{
				Rate = P.CoolSnow;
			}
			else if (bColdWater)
			{
				Rate = P.CoolWater;
			}
			else if (Cool == ECool::Water)
			{
				Rate = P.CoolAir * 2.0;
			}
			const double Was = B.Heat;
			B.Heat = std::max(0.0, B.Heat - Rate * Dt);
			// the cold of the water builds once the banya's heat is spent
			if (bColdWater)
			{
				B.SwimS += Dt;
				if (Was <= 0.05)
				{
					B.Cold += P.ColdPerS * (P.ColdSeaC + 1.0 - SeaC) / P.ColdSeaC * Dt;
				}
				if (!B.bSwimCounted && B.SwimS >= P.CountS)
				{
					B.bSwimCounted = true;
					Ev = EBodyEvent::SwimCounted;
				}
				if (B.Cold >= P.OutCold)
				{
					Ev = EBodyEvent::ColdOut;
				}
				else if (B.Cold >= 1.0 && !B.bWarnedCold)
				{
					B.bWarnedCold = true;
					Ev = EBodyEvent::Cold;
				}
			}
			else
			{
				B.SwimS = 0.0;
				B.bSwimCounted = false;
				const double Warm = P.WarmAirPerS * (B.TowelT > 0.0 ? P.TowelMult : 1.0) * (Where == EWhere::Dressing ? 2.0 : 1.0);
				B.Cold = std::max(0.0, B.Cold - Warm * Dt);
				B.TowelT = std::max(0.0, B.TowelT - Dt);
				if (B.Cold < 0.8)
				{
					B.bWarnedCold = false;
				}
			}
			// a round: hot from the steam, then cooled through by snow or the cold water soon after
			if (B.bHot && (Cool == ECool::Snow || bColdWater) && B.Heat <= P.RoundCool && B.SinceSteam <= P.RoundWindowS)
			{
				B.bHot = false;
				B.Rounds++;
				B.LastRoundBy = Cool;
				if (Ev == EBodyEvent::None || Ev == EBodyEvent::SwimCounted)
				{
					Ev = EBodyEvent::Round;
				}
			}
			if (B.SinceSteam > P.RoundWindowS)
			{
				B.bHot = false;
			}
		}
		return Ev;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// Snow that packs, snowballs, the children's aim

	/** How well the snow packs, 0 (won't hold: too thin, too cold and dry, slush) .. 1. */
	inline double Packs(const FParams& P, double DepthCm, double TempC, double Melt)
	{
		if (DepthCm < P.PackCm || TempC < P.PackTemp[0] || TempC > P.PackTemp[1] + 2.0)
		{
			return 0.0;
		}
		// best just under freezing; wet snow (some melt) holds even better; colder, drier, worse
		const double T = TempC <= 0.0 ? 1.0 - Clamp(-TempC / std::max(0.1, -P.PackTemp[0]), 0.0, 1.0) * 0.8 : 1.0;
		const double Over = TempC > P.PackTemp[1] ? 1.0 - (TempC - P.PackTemp[1]) / 2.0 : 1.0;
		return Clamp((0.55 + 0.45 * Clamp(Melt * 2.0, 0.0, 1.0)) * T * Over * Clamp(DepthCm / (P.PackCm * 2.0), 0.4, 1.0), 0.0, 1.0);
	}

	struct FBall
	{
		V3 Pos, Vel;
		double Age = 0.0;
		int Owner = -1;         // -1 the hero, else a person id
		bool bDone = false;
	};

	inline void StepBall(FBall& B, const FParams& P, double Dt)
	{
		B.Vel.Z -= P.Gravity * Dt;
		B.Pos = B.Pos + B.Vel * Dt;
		B.Age += Dt;
		if (B.Age > P.BallLife)
		{
			B.bDone = true;
		}
	}

	/** The launch velocity from From to hit Target (moving at TargetVel) at Speed, the low arc, the lead worked out over the
	 *  flight; Error radians turned off at random by Rng (none when Error is 0). False when out of reach (then the best try). */
	inline bool Aim(const V3& From, const V3& Target, const V3& TargetVel, double Speed, double G, double Error, FRng* Rng, V3& Out)
	{
		V3 Tgt = Target;
		bool bOk = true;
		V3 Launch;
		for (int Iter = 0; Iter < 4; Iter++)
		{
			const V3 D = Tgt - From;
			const double X = D.Len2D(), Y = D.Z;
			const double V2s = Speed * Speed;
			const double Disc = V2s * V2s - G * (G * X * X + 2.0 * Y * V2s);
			double Ang;
			if (Disc < 0.0)
			{
				bOk = false;
				Ang = Pi / 4.0;
			}
			else
			{
				bOk = true;
				Ang = std::atan2(V2s - std::sqrt(Disc), G * std::max(X, 1e-3));
			}
			const double H = std::atan2(D.Y, D.X);
			Launch = V3(std::cos(H) * std::cos(Ang) * Speed, std::sin(H) * std::cos(Ang) * Speed, std::sin(Ang) * Speed);
			const double Flight = X / std::max(0.1, std::cos(Ang) * Speed);
			Tgt = Target + TargetVel * Flight;
		}
		if (Rng && Error > 0.0)
		{
			const double H = std::atan2(Launch.Y, Launch.X) + Rng->Range(-Error, Error);
			const double Hz = std::sqrt(Launch.X * Launch.X + Launch.Y * Launch.Y);
			const double A = std::atan2(Launch.Z, Hz) + Rng->Range(-Error, Error) * 0.6;
			Launch = V3(std::cos(H) * std::cos(A) * Speed, std::sin(H) * std::cos(A) * Speed, std::sin(A) * Speed);
		}
		Out = Launch;
		return bOk;
	}

	/** The closest a ball came to a point during a step (from A to B). */
	inline double PassDistance(const V3& A, const V3& B, const V3& P)
	{
		const V3 Ab = B - A;
		const double L2 = Ab.X * Ab.X + Ab.Y * Ab.Y + Ab.Z * Ab.Z;
		const double T = L2 > 1e-12 ? Clamp(((P.X - A.X) * Ab.X + (P.Y - A.Y) * Ab.Y + (P.Z - A.Z) * Ab.Z) / L2, 0.0, 1.0) : 0.0;
		return (P - (A + Ab * T)).Len();
	}

	/** A snowball fight with the children near: who is in, when each throws next, the score. */
	struct FFight
	{
		struct FKid { int Id = -1; double NextThrow = 0.0; int Thrown = 0, Hits = 0; };
		bool bOn = false;
		double Ends = 0.0;
		std::vector<FKid> Kids;
		int HeroThrown = 0, HeroHits = 0, KidHits = 0;
		FRng Rng = FRng(31u);

		bool Has(int Id) const { for (const FKid& K : Kids) if (K.Id == Id) return true; return false; }
		/** A child joins (the fight starts with the first), up to the limit; a new join tops the time up. */
		bool Join(const FParams& P, int Id, double Now)
		{
			if (Has(Id) || (int)Kids.size() >= P.MaxKids)
			{
				if (bOn) Ends = std::max(Ends, Now + P.FightS * 0.5);
				return false;
			}
			if (!bOn)
			{
				bOn = true;
				HeroThrown = HeroHits = KidHits = 0;
				Kids.clear();
			}
			FKid K;
			K.Id = Id;
			K.NextThrow = Now + Rng.Range(0.8, 2.0);
			Kids.push_back(K);
			Ends = std::max(Ends, Now + P.FightS);
			return true;
		}
		/** The kids whose turn it is now (each then waits a new while). */
		std::vector<int> Due(const FParams& P, double Now)
		{
			std::vector<int> Out;
			for (FKid& K : Kids)
			{
				if (Now >= K.NextThrow)
				{
					Out.push_back(K.Id);
					K.Thrown++;
					K.NextThrow = Now + Rng.Range(P.ThrowEvery[0], P.ThrowEvery[1]);
				}
			}
			return Out;
		}
		void Drop(int Id)
		{
			for (size_t I = 0; I < Kids.size(); I++) if (Kids[I].Id == Id) { Kids.erase(Kids.begin() + (long)I); return; }
		}
		/** Over: time is up or nobody is left. */
		bool Over(double Now) const { return bOn && (Now >= Ends || Kids.empty()); }
		void End() { bOn = false; Kids.clear(); }
		/** The aim's error for a throw over Distance m, radians. */
		static double ErrorFor(const FParams& P, double Distance) { return (P.AimErrDeg + P.AimErrPerM * Distance) * Pi / 180.0; }
	};

	// ------------------------------------------------------------------------------------------------------------------
	// Snowmen

	/** A rolled ball's radius after rolling Dist m more over snow DepthCm deep (its swept snow joins it), capped. */
	inline double GrowBall(const FParams& P, double R, double DistM, double DepthCm)
	{
		const double D = std::max(0.0, DepthCm) / 100.0;
		// the snow it sweeps (a strip about its contact's width, the depth deep, packed) joins its volume: R^2 grows with the
		// way it rolls; about 10 m of 15 cm snow from a fist to a snowman's base
		return std::min(P.MaxR, std::sqrt(R * R + D * P.Pack * std::max(0.0, DistM) * 0.18));
	}

	/** A ball can be lifted onto another: light enough to carry and smaller than the one under it. */
	inline bool CanLift(const FParams& P, double Top, double Base) { return Top <= P.LiftR && Top <= Base * P.Ratio; }

	struct FSnowman
	{
		V3 At;                  // the bottom ball's foot (world metres)
		double Yaw = 0.0;       // the face's way (deg)
		double R[3] = { 0.0, 0.0, 0.0 };    // the balls, bottom up (0: none yet)
		int Balls = 0;
		bool bNose = false;
		int Eyes = 0;
		bool bArms = false;
		double Slump = 0.0;     // 0 .. 1 the thaw's work
		bool Done() const { return Balls >= 3; }
		bool Dressed() const { return bNose && Eyes >= 2 && bArms; }
		double Height() const { double H = 0.0; for (int I = 0; I < Balls; I++) H += R[I] * 2.0 * (I ? 0.85 : 1.0); return H; }
	};

	/** The thaw shrinks them; a ball that melts away takes the ones over it down. Returns false when it is gone. */
	inline bool MeltSnowman(FSnowman& S, const FParams& P, double Hours, double TempC, double Melt)
	{
		if (TempC <= 0.5 && Melt < 0.2)
		{
			return true;
		}
		const double Rate = P.MeltPerH * (0.5 + std::max(0.0, TempC) * 0.25 + Melt) * Hours;
		S.Slump = std::min(1.0, S.Slump + Rate * 2.0);
		for (int I = 0; I < S.Balls; I++)
		{
			S.R[I] = std::max(0.0, S.R[I] - Rate);
		}
		while (S.Balls > 0 && S.R[S.Balls - 1] < 0.05)
		{
			S.R[--S.Balls] = 0.0;
		}
		if (S.Balls > 0 && S.R[0] < 0.08)
		{
			S.Balls = 0;
		}
		if (S.Balls < 3)
		{
			S.bNose = false;
			S.Eyes = 0;
			S.bArms = false;
		}
		return S.Balls > 0;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// What is saved

	struct FSaved
	{
		std::vector<FSnowman> Snowmen;
		int Rounds = 0;             // today's banya rounds
		long long RoundsDay = -1;
		long long SwamNewYear = -1; // the winter of the last New Year swim
		double ThawHours = 0.0;
		double BestGlideM = 0.0;
		double SkatedM = 0.0;

		std::string Save() const
		{
			std::ostringstream O;
			O.precision(6);
			O << "v1 " << Rounds << ' ' << RoundsDay << ' ' << SwamNewYear << ' ' << ThawHours << ' ' << BestGlideM << ' ' << SkatedM << ' '
				<< Snowmen.size();
			for (const FSnowman& S : Snowmen)
			{
				O << ' ' << S.At.X << ' ' << S.At.Y << ' ' << S.At.Z << ' ' << S.Yaw << ' ' << S.Balls << ' ' << S.R[0] << ' ' << S.R[1] << ' '
					<< S.R[2] << ' ' << (S.bNose ? 1 : 0) << ' ' << S.Eyes << ' ' << (S.bArms ? 1 : 0) << ' ' << S.Slump;
			}
			return O.str();
		}

		bool Load(const std::string& Text)
		{
			std::istringstream I(Text);
			std::string V;
			size_t N = 0;
			FSaved Out;
			if (!(I >> V) || V != "v1" || !(I >> Out.Rounds >> Out.RoundsDay >> Out.SwamNewYear >> Out.ThawHours >> Out.BestGlideM >> Out.SkatedM >> N)
				|| N > 64)
			{
				return false;
			}
			for (size_t K = 0; K < N; K++)
			{
				FSnowman S;
				int Nose = 0, Arms = 0;
				if (!(I >> S.At.X >> S.At.Y >> S.At.Z >> S.Yaw >> S.Balls >> S.R[0] >> S.R[1] >> S.R[2] >> Nose >> S.Eyes >> Arms >> S.Slump))
				{
					return false;
				}
				S.Balls = std::max(0, std::min(3, S.Balls));
				S.bNose = Nose != 0;
				S.bArms = Arms != 0;
				Out.Snowmen.push_back(S);
			}
			*this = Out;
			return true;
		}
	};
}
