// The winter play's rules outside the engine (docs/winter_play.md): the pond's ice, skating, the banya's heat and rounds,
// the cold water, the schedules, snow that packs, the children's aim, snowmen and the save, on the real
// Content/City/winter_play.json (python3 tools/winter_play/winter_play.py).
//   tools/winterplaysim/run.sh
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Winter/WinterPlayModel.h"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace LyudWinterPlay;

static int Failures = 0, Passed = 0;
#define CHECK(Cond, ...) do { if (Cond) { Passed++; } else { std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); Failures++; } } while (0)

static bool ReadFile(const std::string& Path, std::string& Out)
{
	std::ifstream F(Path, std::ios::binary);
	if (!F) return false;
	std::stringstream SS;
	SS << F.rdbuf();
	Out = SS.str();
	return true;
}

/** Skates straight with the key held for Secs, then lets go; returns the top speed and the glide after letting go. */
static void PushThenGlide(const FParams& P, double Secs, bool bShift, double& Top, double& Glide, double& GlideS)
{
	FSkater K;
	FSkateIn In;
	In.bShift = bShift;
	In.Wish = V2(1.0, 0.0);
	Top = 0.0;
	for (double T = 0.0; T < Secs; T += In.Dt)
	{
		StepSkate(K, In, P);
		Top = std::max(Top, K.Speed());
	}
	In.Wish = V2();
	const double D0 = K.DistM;
	GlideS = 0.0;
	while (K.Speed() > 0.0 && GlideS < 600.0)
	{
		StepSkate(K, In, P);
		GlideS += In.Dt;
	}
	Glide = K.DistM - D0;
}

static void TestIce(const FParams& P)
{
	std::printf("ice\n");
	FIceState S;
	FIceIn In;
	In.Month = 7; In.Day = 1;
	CHECK(!S.Step(P, In, 1.0).bFrozen, "no ice in July");
	In.Month = 1; In.Day = 10; In.FreezingDegreeDays = 0.0;
	FIce I = S.Step(P, In, 1.0);
	CHECK(I.bFrozen && std::fabs(I.Cm - P.FirstCm) < 1e-9 && !I.bOpen, "January, no frost yet: the first skin of ice, closed (%.1f cm)", I.Cm);
	double Last = 0.0;
	bool bMono = true;
	for (double F = 0.0; F <= 400.0; F += 10.0)
	{
		In.FreezingDegreeDays = F;
		const double Cm = S.Step(P, In, 1.0).Cm;
		bMono &= Cm >= Last - 1e-9;
		Last = Cm;
	}
	CHECK(bMono, "thickness grows with the frost");
	CHECK(Last <= P.MaxCm + 1e-9, "capped at %.0f cm (%.1f)", P.MaxCm, Last);
	In.FreezingDegreeDays = 40.0;
	I = S.Step(P, In, 1.0);
	CHECK(I.bOpen && I.Cm >= P.OpenCm, "40 freezing degree-days open the rink (%.1f cm)", I.Cm);
	// a thaw: after slush_hours water lies on it and the rink closes; frost again dries it in half the time
	In.TempC = 3.0; In.Melt = 0.8;
	for (int H = 0; H < (int)P.SlushHours; H++) I = S.Step(P, In, 1.0);
	CHECK(I.Slush >= 0.99 && !I.bOpen, "a long thaw puts water on the ice (slush %.2f)", I.Slush);
	In.TempC = -6.0; In.Melt = 0.0;
	for (int H = 0; H < (int)(P.SlushHours * 0.5) + 1; H++) I = S.Step(P, In, 1.0);
	CHECK(I.Slush <= 0.01 && I.bOpen, "frost again: open (slush %.2f)", I.Slush);
	In.Force = 1; In.Month = 8;
	CHECK(S.Step(P, In, 1.0).Cm >= P.ThickCm, "forced ice is thick enough");
	CHECK(PondFrozenOnCalendar(P, 12, 15) && PondFrozenOnCalendar(P, 2, 28) && !PondFrozenOnCalendar(P, 12, 14) && !PondFrozenOnCalendar(P, 3, 16),
		"the calendar: Dec 15 .. Mar 15");

	// cracks: fewer when thick, the same set each time, inside the pond
	const std::vector<FCrack> Thin = Cracks(P, 2026, P.OpenCm), Thick = Cracks(P, 2026, P.ThickCm + 5.0), Again = Cracks(P, 2026, P.OpenCm);
	CHECK((int)Thin.size() == P.CracksThin && (int)Thick.size() == P.CracksThick, "cracks: %d thin, %d thick", (int)Thin.size(), (int)Thick.size());
	bool bSame = Thin.size() == Again.size();
	for (size_t K = 0; bSame && K < Thin.size(); K++) bSame = Thin[K].A.X == Again[K].A.X && Thin[K].B.Y == Again[K].B.Y;
	CHECK(bSame, "the same winter's cracks are the same");
	bool bPrefix = true, bInside = true;
	for (size_t K = 0; K < Thick.size(); K++) bPrefix &= Thick[K].A.X == Thin[K].A.X;
	for (const FCrack& C : Thin)
	{
		const V2 Mid = (C.A + C.B) * 0.5;
		bInside &= (Mid.X * Mid.X) / (P.RinkRx * P.RinkRx) + (Mid.Y * Mid.Y) / (P.RinkRy * P.RinkRy) < 1.0;
	}
	CHECK(bPrefix, "thicker ice keeps a subset of the thin ice's cracks (no new ones appear)");
	CHECK(bInside, "every crack's middle is on the pond");
	const std::vector<FCrack> Other = Cracks(P, 2027, P.OpenCm);
	CHECK(Other[0].A.X != Thin[0].A.X, "another winter, other cracks");
	const FCrack& C = Thin[0];
	const V2 Mid = (C.A + C.B) * 0.5, N = V2(-(C.B - C.A).Y, (C.B - C.A).X).Norm();
	CHECK(CrossesCrack(Thin, Mid - N * 1.0, Mid + N * 1.0), "a step across a crack crosses it");
	CHECK(!CrossesCrack({ C }, Mid + N * 2.0, Mid + N * 3.0), "a step beside it does not");
}

static void TestSkating(const FParams& P)
{
	std::printf("skating\n");
	double Top, Glide, GlideS;
	PushThenGlide(P, 12.0, false, Top, Glide, GlideS);
	CHECK(Top <= P.MaxSpeed + 0.03 && Top >= P.MaxSpeed * 0.9, "top speed %.2f m/s (max %.2f)", Top, P.MaxSpeed);
	CHECK(Glide > 25.0 && Glide < 140.0, "a long glide from full speed: %.1f m in %.1f s", Glide, GlideS);
	double TopS, GlideSh, GlideShS;
	PushThenGlide(P, 12.0, true, TopS, GlideSh, GlideShS);
	CHECK(TopS <= P.MaxSpeedShift + 0.03 && TopS < Top, "Shift skates slowly: %.2f m/s", TopS);
	// the first stroke: speed comes in pushes (faster in the push's half than the glide's)
	{
		FSkater K;
		FSkateIn In;
		In.Wish = V2(1.0, 0.0);
		int Strokes = 0;
		for (double T = 0.0; T < 3.0; T += In.Dt) Strokes += StepSkate(K, In, P) == ESkateEvent::Stroke;
		CHECK(Strokes >= 3 && Strokes <= 5, "strokes every %.2f s: %d in 3 s", P.StrokeS, Strokes);
		CHECK(K.Speed() > 2.0, "3 s of strokes: %.2f m/s", K.Speed());
	}
	// a hockey stop: S at full speed stops within a few metres, much shorter than the glide
	{
		FSkater K;
		FSkateIn In;
		In.Wish = V2(1.0, 0.0);
		for (double T = 0.0; T < 10.0; T += In.Dt) StepSkate(K, In, P);
		In.Wish = V2(-1.0, 0.0);
		const double D0 = K.DistM;
		bool bStop = false;
		for (int I = 0; I < 600 && !bStop; I++) bStop = StepSkate(K, In, P) == ESkateEvent::Stop;
		const double D = K.DistM - D0;
		CHECK(bStop && D < 7.0 && D < Glide * 0.2, "hockey stop in %.1f m", D);
	}
	// turning: the radius at full speed is no tighter than v^2 / grip
	{
		FSkater K;
		FSkateIn In;
		In.Wish = V2(1.0, 0.0);
		for (double T = 0.0; T < 10.0; T += In.Dt) StepSkate(K, In, P);
		const double V = K.Speed();
		In.Wish = V2(0.0, 1.0);
		double Turned = 0.0;
		V2 Prev = K.Vel.Norm();
		for (int I = 0; I < 6; I++)
		{
			StepSkate(K, In, P);
			const V2 D = K.Vel.Norm();
			Turned += std::fabs(std::atan2(Prev.Cross(D), Prev.Dot(D)));
			Prev = D;
		}
		const double W = Turned / (6.0 * In.Dt);
		CHECK(W * V <= P.Grip * 1.05, "turn held by the edge: v*w %.2f <= grip %.2f", W * V, P.Grip);
	}
	// a hard turn held at speed throws him; he gets up after fall + getup seconds
	{
		FSkater K;
		FSkateIn In;
		In.Wish = V2(1.0, 0.0);
		for (double T = 0.0; T < 10.0; T += In.Dt) StepSkate(K, In, P);
		In.Wish = V2(-0.3, 1.0);
		bool bFell = false;
		for (int I = 0; I < 120 && !bFell; I++) bFell = StepSkate(K, In, P) == ESkateEvent::Fall;
		CHECK(bFell, "a sharp turn at full speed throws him");
		double Down = 0.0;
		bool bUp = false;
		In.Wish = V2();
		for (int I = 0; I < 600 && !bUp; I++) { bUp = StepSkate(K, In, P) == ESkateEvent::GetUp; Down += In.Dt; }
		CHECK(bUp && std::fabs(Down - (P.FallS + P.GetupS)) < 0.05, "down %.2f s", Down);
	}
	// cracks: crossed slowly never throw him, fast often do
	{
		int SlowFalls = 0, FastFalls = 0;
		for (int Trial = 0; Trial < 200; Trial++)
		{
			FSkater K;
			K.Rng = FRng(1000u + (uint32_t)Trial);
			FSkateIn In;
			K.Vel = V2(P.CrackSpeed * 0.8, 0.0);
			In.bCrack = true;
			SlowFalls += StepSkate(K, In, P) == ESkateEvent::Fall;
			FSkater F;
			F.Rng = FRng(5000u + (uint32_t)Trial);
			F.Vel = V2(P.MaxSpeed, 0.0);
			FastFalls += StepSkate(F, In, P) == ESkateEvent::Fall;
		}
		CHECK(SlowFalls == 0, "slow over a crack: no falls (%d)", SlowFalls);
		CHECK(FastFalls > 60 && FastFalls < 195, "fast over a crack: %d falls of 200", FastFalls);
	}
	// boots on the ice: slow, and a slide after letting go
	{
		FSkater K;
		FSkateIn In;
		In.bSkates = false;
		In.Wish = V2(1.0, 0.0);
		for (double T = 0.0; T < 6.0; T += In.Dt) StepSkate(K, In, P);
		CHECK(std::fabs(K.Speed() - P.BootsMax) < 0.05, "boots: %.2f m/s", K.Speed());
		In.Wish = V2();
		const double D0 = K.DistM;
		for (double T = 0.0; T < 6.0; T += In.Dt) StepSkate(K, In, P);
		const double Slide = K.DistM - D0;
		CHECK(Slide > 1.5 && Slide < 8.0, "boots slide %.1f m after letting go", Slide);
	}
}

static void TestBanya(const FParams& P)
{
	std::printf("banya\n");
	FRoom Room;
	Room.Reset(P);
	CHECK(Room.Felt(P, true) < Room.Felt(P, false), "the felt hat takes heat off");
	Room.Ladle(P);
	const double Steam1 = Room.Steam;
	CHECK(Room.C > P.RoomC && Steam1 > 0.0, "a ladle heats the room and makes steam");
	for (int I = 0; I < 30; I++) Room.Ladle(P);
	CHECK(Room.C <= P.RoomMaxC && Room.Steam <= 1.0, "the room is capped (%.0f C)", Room.C);
	Room.Reset(P);
	// time to get hot on the shelf: a few minutes without ladles, faster with
	auto HotIn = [&P](int Ladles, bool bHat)
	{
		FRoom R;
		R.Reset(P);
		FBody B;
		double T = 0.0;
		for (int K = 0; K < Ladles; K++) R.Ladle(P);
		while (B.Heat < P.RoundHot && T < 3600.0)
		{
			StepBody(B, R, P, EWhere::Steam, bHat, ECool::Air, -5.0, 2.0, 0.1);
			R.Step(P, 0.1);
			T += 0.1;
		}
		return T;
	};
	const double Plain = HotIn(0, false), Steamy = HotIn(4, false), Hat = HotIn(4, true);
	CHECK(Plain > 120.0 && Plain < 900.0, "hot through on the shelf in %.0f s", Plain);
	CHECK(Steamy < Plain, "ladles make it faster (%.0f s)", Steamy);
	CHECK(Hat > Steamy, "the hat makes it slower (%.0f s)", Hat);
	// a round: hot, then into the snow; then into the cold sea; past the window it does not count
	{
		FBody B;
		B.Heat = P.RoundHot + 0.1;
		B.bHot = true;
		B.SinceSteam = 0.0;
		EBodyEvent Ev = EBodyEvent::None;
		double T = 0.0;
		while (Ev != EBodyEvent::Round && T < 300.0) { Ev = StepBody(B, Room, P, EWhere::Outside, false, ECool::Snow, -5.0, 2.0, 0.1); T += 0.1; }
		CHECK(Ev == EBodyEvent::Round && B.Rounds == 1 && T < P.RoundWindowS, "snow cools him through: a round in %.0f s", T);
		B.Heat = P.RoundHot + 0.1; B.bHot = true; B.SinceSteam = 0.0; T = 0.0; Ev = EBodyEvent::None;
		while (Ev != EBodyEvent::Round && T < 300.0) { Ev = StepBody(B, Room, P, EWhere::Outside, false, ECool::Water, -5.0, 2.0, 0.1); T += 0.1; }
		CHECK(Ev == EBodyEvent::Round && B.Rounds == 2 && B.LastRoundBy == ECool::Water, "the cold sea too: round 2 in %.0f s", T);
		B.Heat = P.RoundHot + 0.1; B.bHot = true; B.SinceSteam = 0.0;
		for (double S = 0.0; S < P.RoundWindowS + 5.0; S += 0.1) StepBody(B, Room, P, EWhere::Dressing, false, ECool::Air, -5.0, 2.0, 0.1);
		for (int I = 0; I < 3000; I++) StepBody(B, Room, P, EWhere::Outside, false, ECool::Snow, -5.0, 2.0, 0.1);
		CHECK(B.Rounds == 2, "cooled after the window: no round (%d)", B.Rounds);
		FBody W;
		W.Heat = P.RoundHot + 0.1; W.bHot = true; W.SinceSteam = 0.0;
		for (int I = 0; I < 3000; I++) StepBody(W, Room, P, EWhere::Outside, false, ECool::Water, 10.0, 15.0, 0.1);
		CHECK(W.Rounds == 0, "a warm sea is no cold plunge");
	}
	// too hot: a warning, then he must leave
	{
		FRoom R;
		R.Reset(P);
		for (int K = 0; K < 8; K++) R.Ladle(P);
		FBody B;
		bool bWarn = false, bLeave = false;
		for (int I = 0; I < 60000 && !bLeave; I++)
		{
			const EBodyEvent Ev = StepBody(B, R, P, EWhere::Steam, false, ECool::Air, -5.0, 2.0, 0.1);
			bWarn |= Ev == EBodyEvent::TooHot;
			bLeave = Ev == EBodyEvent::MustLeave;
		}
		CHECK(bWarn && bLeave, "too hot warns, then sends him out");
	}
	// the schedule
	CHECK(BanyaOpen(P, 1, 14, 2, 15.0) && !BanyaOpen(P, 1, 14, 2, 13.0) && !BanyaOpen(P, 1, 13, 1, 15.0), "Wednesday 15:00 open, 13:00 not, Tuesday not");
	CHECK(BanyaOpen(P, 12, 31, 3, 11.0) && !BanyaOpen(P, 12, 31, 3, 19.0), "New Year's Eve by its own hours");
	CHECK(BanyaOpen(P, 1, 1, 3, 10.0), "New Year's Day morning open");
}

static void TestCold(const FParams& P)
{
	std::printf("cold water\n");
	FRoom Room;
	Room.Reset(P);
	// cold from the start: counted, warned, hauled out; colder sea is faster
	auto Swim = [&P, &Room](double SeaC, double Heat, double& Counted, double& Warn, double& Out)
	{
		FBody B;
		B.Heat = Heat;
		Counted = Warn = Out = -1.0;
		for (double T = 0.0; T < 900.0 && Out < 0.0; T += 0.1)
		{
			const EBodyEvent Ev = StepBody(B, Room, P, EWhere::Outside, false, ECool::Water, -4.0, SeaC, 0.1);
			if (Ev == EBodyEvent::SwimCounted && Counted < 0.0) Counted = T;
			if (Ev == EBodyEvent::Cold && Warn < 0.0) Warn = T;
			if (Ev == EBodyEvent::ColdOut) Out = T;
		}
	};
	double C0, W0, O0, C1, W1, O1, C2, W2, O2;
	Swim(1.0, 0.0, C0, W0, O0);
	Swim(6.0, 0.0, C1, W1, O1);
	Swim(1.0, 0.9, C2, W2, O2);
	CHECK(C0 > 0.0 && std::fabs(C0 - P.CountS) < 0.2, "a swim counts at %.0f s", C0);
	CHECK(W0 > C0 && O0 > W0, "warned at %.0f s, hauled out at %.0f s (1 C sea)", W0, O0);
	CHECK(W0 > 20.0 && W0 < 200.0, "a minute or so in the coldest water (%.0f s)", W0);
	CHECK(O1 > O0, "a 6 C sea takes longer (%.0f s)", O1);
	CHECK(O2 > O0, "the banya's heat soaks up the cold first (%.0f s)", O2);
	// the towel warms faster
	FBody A, B;
	A.Cold = B.Cold = 0.9;
	B.TowelT = 1000.0;
	for (int I = 0; I < 100; I++)
	{
		StepBody(A, Room, P, EWhere::Outside, false, ECool::Air, -4.0, 1.0, 0.1);
		StepBody(B, Room, P, EWhere::Outside, false, ECool::Air, -4.0, 1.0, 0.1);
	}
	CHECK(B.Cold < A.Cold, "the towel warms faster (%.3f < %.3f)", B.Cold, A.Cold);
	// schedules
	CHECK(WalrusSeason(P, 1) && WalrusSeason(P, 12) && !WalrusSeason(P, 6), "walrus season Dec..Mar");
	CHECK(WalrusHour(P, 1, 18, 6, 9.0) && !WalrusHour(P, 1, 18, 6, 12.0) && !WalrusHour(P, 1, 17, 5, 9.0), "Sunday mornings");
	CHECK(IsNewYearMorning(P, 1, 1, 9.0) && !IsNewYearMorning(P, 1, 1, 14.0) && !IsNewYearMorning(P, 1, 2, 9.0), "New Year's morning");
	CHECK(WinterOf(2027, 1) == 2026 && WinterOf(2026, 12) == 2026, "a winter is named by its December");
}

static void TestSnow(const FParams& P)
{
	std::printf("snow\n");
	CHECK(Packs(P, 1.0, -2.0, 0.0) == 0.0, "too thin: no");
	CHECK(Packs(P, 10.0, -12.0, 0.0) == 0.0, "frost-dry: no");
	CHECK(Packs(P, 10.0, 6.0, 1.0) == 0.0, "slush: no");
	CHECK(Packs(P, 10.0, -1.0, 0.5) > Packs(P, 10.0, -6.0, 0.0), "wet near freezing packs best");
	CHECK(Packs(P, 10.0, -1.0, 0.0) > 0.3, "near freezing packs");
	// a ball thrown flat hits the ground ~ at the expected range; StepBall ends it on its life
	{
		FBall B;
		B.Pos = V3(0.0, 0.0, 1.5);
		const double A = P.UpDeg * Pi / 180.0;
		B.Vel = V3(std::cos(A) * P.BallSpeed, 0.0, std::sin(A) * P.BallSpeed);
		double T = 0.0;
		while (B.Pos.Z > 0.0 && !B.bDone && T < 10.0) { StepBall(B, P, 1.0 / 120.0); T += 1.0 / 120.0; }
		CHECK(B.Pos.X > 12.0 && B.Pos.X < 30.0, "the hero's throw lands %.1f m out", B.Pos.X);
	}
	// the children's aim: without error it passes within a hit of a still target and of a running one
	{
		FRng R(9u);
		int Hits = 0, Moving = 0, Far = 0, Tried = 0, TriedMoving = 0;
		for (int I = 0; I < 50; I++)
		{
			const double D = 5.0 + I * 0.2;     // within a child's reach (13 m/s: about 17 m)
			const V3 From(0.0, 0.0, 1.2), Tgt(D, 2.0, 1.0), Vel(0.0, I % 2 ? 2.0 : 0.0, 0.0);
			V3 Launch;
			if (!Aim(From, Tgt, Vel, P.KidSpeed, P.Gravity, 0.0, nullptr, Launch)) continue;
			Tried++;
			TriedMoving += I % 2;
			FBall B;
			B.Pos = From;
			B.Vel = Launch;
			double Best = 1e9;
			V3 T = Tgt;
			for (double S = 0.0; S < 4.0 && !B.bDone; S += 1.0 / 240.0)
			{
				const V3 Prev = B.Pos;
				StepBall(B, P, 1.0 / 240.0);
				T = T + Vel * (1.0 / 240.0);
				Best = std::min(Best, PassDistance(Prev, B.Pos, T));
			}
			if (Best < P.HitM) { Hits++; if (I % 2) Moving++; }
			// with the error, far throws miss more
			V3 L2;
			Aim(From, Tgt, Vel, P.KidSpeed, P.Gravity, FFight::ErrorFor(P, D), &R, L2);
			Far += (L2 - Launch).Len() > 0.01;
		}
		CHECK(Tried >= 45, "in reach: %d of 50", Tried);
		CHECK(Hits >= Tried - 1, "the aim without error hits %d of %d", Hits, Tried);
		CHECK(Moving >= TriedMoving - 1, "leading a running target: %d of %d", Moving, TriedMoving);
		CHECK(Far >= Tried - 2, "the error moves the throw (%d)", Far);
	}
	// the fight
	{
		FFight F;
		CHECK(F.Join(P, 7, 0.0) && F.bOn && !F.Join(P, 7, 1.0), "a child joins once");
		for (int Id = 8; Id < 20; Id++) F.Join(P, Id, 0.0);
		CHECK((int)F.Kids.size() == P.MaxKids, "at most %d children", P.MaxKids);
		int Throws = 0;
		for (double T = 0.0; T < 30.0; T += 0.1) Throws += (int)F.Due(P, T).size();
		CHECK(Throws > (int)(30.0 / P.ThrowEvery[1]) * P.MaxKids / 2 && Throws < (int)(30.0 / P.ThrowEvery[0] + 2) * P.MaxKids, "%d throws in 30 s", Throws);
		CHECK(!F.Over(P.FightS - 1.0) && F.Over(P.FightS + 1.0), "the fight lasts %.0f s", P.FightS);
		for (int Id = 7; Id < 20; Id++) F.Drop(Id);
		CHECK(F.Over(5.0), "nobody left: over");
	}
}

static void TestSnowman(const FParams& P)
{
	std::printf("snowmen\n");
	double R = P.StartR, Dist = 0.0;
	while (R < 0.4 && Dist < 500.0) { R = GrowBall(P, R, 0.1, 15.0); Dist += 0.1; }
	CHECK(Dist > 5.0 && Dist < 80.0, "a 40 cm-radius ball after %.0f m of 15 cm snow", Dist);
	double R2 = P.StartR;
	for (int I = 0; I < 10000; I++) R2 = GrowBall(P, R2, 0.1, 30.0);
	CHECK(std::fabs(R2 - P.MaxR) < 1e-9, "capped at %.2f m", P.MaxR);
	CHECK(!CanLift(P, 0.5, 0.55) && CanLift(P, 0.3, 0.45) && !CanLift(P, 0.3, 0.3), "lift: light enough and smaller");
	FSnowman S;
	S.Balls = 3; S.R[0] = 0.45; S.R[1] = 0.32; S.R[2] = 0.22;
	CHECK(S.Done() && !S.Dressed(), "three balls: done, not dressed");
	S.bNose = true; S.Eyes = 2; S.bArms = true;
	CHECK(S.Dressed(), "dressed");
	CHECK(S.Height() > 1.2 && S.Height() < 2.0, "height %.2f m", S.Height());
	FSnowman Cold = S;
	CHECK(MeltSnowman(Cold, P, 48.0, -5.0, 0.0) && Cold.R[0] == S.R[0], "frost keeps him");
	FSnowman Thaw = S;
	bool bAlive = true;
	int Hours = 0;
	while (bAlive && Hours < 2000) { bAlive = MeltSnowman(Thaw, P, 1.0, 4.0, 0.8); Hours++; }
	CHECK(!bAlive && Hours > 10 && Hours < 400, "a thaw takes him in %d h", Hours);
	FSnowman Half = S;
	for (int H = 0; H < 12; H++) MeltSnowman(Half, P, 1.0, 4.0, 0.8);
	CHECK(Half.Slump > 0.0 && Half.R[0] < S.R[0], "the thaw slumps him");
}

static void TestSave()
{
	std::printf("save\n");
	FSaved A;
	A.Rounds = 2; A.RoundsDay = 739999; A.SwamNewYear = 2026; A.ThawHours = 3.5; A.BestGlideM = 41.25; A.SkatedM = 1234.5;
	FSnowman S;
	S.At = V3(-70.5, 1260.25, -35.9); S.Yaw = 135.0; S.Balls = 3; S.R[0] = 0.45; S.R[1] = 0.3; S.R[2] = 0.2; S.bNose = true; S.Eyes = 2; S.bArms = false; S.Slump = 0.25;
	A.Snowmen.push_back(S);
	A.Snowmen.push_back(FSnowman());
	FSaved B;
	CHECK(B.Load(A.Save()), "loads its own save");
	CHECK(B.Rounds == 2 && B.RoundsDay == 739999 && B.SwamNewYear == 2026 && B.BestGlideM == 41.25 && B.Snowmen.size() == 2, "the numbers survive");
	CHECK(B.Snowmen[0].bNose && B.Snowmen[0].Eyes == 2 && !B.Snowmen[0].bArms && std::fabs(B.Snowmen[0].At.Y - 1260.25) < 1e-6 && B.Snowmen[0].Slump == 0.25,
		"the snowman survives");
	FSaved C = A;
	CHECK(!C.Load("v0 1 2 3") && !C.Load("") && !C.Load("v1 1 2") && C.Rounds == 2, "a bad save is refused and leaves the state");
}

int main()
{
	std::string Text, Err;
	FParams P;
	if (!ReadFile("unreal/Lyudmilovsk/Content/City/winter_play.json", Text) || !P.Load(Text, Err))
	{
		std::printf("cannot load Content/City/winter_play.json: %s\n", Err.c_str());
		return 1;
	}
	CHECK(P.RinkLoop.size() >= 12 && P.BanyaShop.size() == 3, "the data: rink loop %d points, banya shop %d goods", (int)P.RinkLoop.size(), (int)P.BanyaShop.size());
	CHECK(P.LadderTopBack > 0.5 && P.LadderAt.Y > P.DeckEdgeY && P.LadderAt.Y < P.HoleAt.Y - 1.3, "the ladder hangs in the hole at its pier side");
	TestIce(P);
	TestSkating(P);
	TestBanya(P);
	TestCold(P);
	TestSnow(P);
	TestSnowman(P);
	TestSave();
	std::printf("%d passed, %d failed\n", Passed, Failures);
	return Failures ? 1 : 0;
}
