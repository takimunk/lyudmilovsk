// The beach model's checks (tools/beachsim/run.sh): skipping stones, the sandcastle and the tide, the buoy swim, the finds.
#include "BeachModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

using namespace LyudBeach;

static int GPassed = 0, GFailed = 0;
#define CHECK(Cond, ...) do { if (Cond) { GPassed++; } else { GFailed++; std::printf("  FAIL "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// A tide like the town's (docs/tides.md): high water is the datum, 0; low water Low m under it, every 12 h 25 min.
static double Tide(double Hours, double Low)
{
	return Low * 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979 * Hours / 12.42));
}

static int Skips(double Q, double F, double Wave, uint32_t Seed = 7)
{
	FThrowIn In;
	In.Quality = Q;
	In.Flatness = F;
	In.WaveM = Wave;
	In.Seed = Seed;
	return Throw(In).Skips;
}

static void TestStones()
{
	std::printf("stones\n");
	int Flat = 0, Great = 0;
	const int N = 20000;
	for (int I = 0; I < N; ++I)
	{
		const double F = StoneFlatness(I % 137, I / 137, 400 + I % 9, I % 5);
		Flat += IsFlat(F) ? 1 : 0;
		Great += F > 0.93 ? 1 : 0;
		if (F < 0.0 || F > 1.0)
		{
			CHECK(false, "flatness %f out of range", F);
		}
	}
	CHECK(Flat > N * 0.25 && Flat < N * 0.35, "about three stones in ten are flat (%d of %d)", Flat, N);
	CHECK(Great > N * 0.02 && Great < N * 0.09, "a really good stone is rare (%d of %d)", Great, N);
	CHECK(StoneFlatness(3, 4, 10, 0) == StoneFlatness(3, 4, 10, 0), "the same spot, day and try: the same stone");
	CHECK(StoneFlatness(3, 4, 10, 0) != StoneFlatness(3, 4, 10, 1), "the next stone at a spot is another");
	CHECK(StoneFlatness(3, 4, 10, 0) != StoneFlatness(3, 4, 11, 0), "tomorrow the sea has turned the shingle over");

	CHECK(SwingAt(0.0) == 0.0 && std::fabs(SwingAt(SwingSeconds) - 1.0) < 1e-9 && std::fabs(SwingAt(2.0 * SwingSeconds)) < 1e-9, "the swing goes forward and back");
	CHECK(std::fabs(SwingAt(SwingSeconds * 0.5) - 0.5) < 1e-9 && std::fabs(SwingAt(SwingSeconds * 1.5) - 0.5) < 1e-9, "the swing is even both ways");
	CHECK(ReleaseQuality(BestSwing) == 1.0, "letting go at the best moment is a clean release");
	CHECK(ReleaseQuality(0.0) == 0.0 && ReleaseQuality(BestSwing - SwingWidth) == 0.0, "letting go at once is a fumble");
	CHECK(ReleaseQuality(BestSwing - 0.1) > ReleaseQuality(BestSwing - 0.2), "nearer the moment is better");
	CHECK(ReleaseQuality(1.0) > 0.3 && ReleaseQuality(1.0) < 0.6, "the very end of the swing is late but not lost (%.2f)", ReleaseQuality(1.0));

	const int Best = Skips(1.0, 1.0, 0.05);
	CHECK(Best >= 14 && Best <= 26, "a perfect disc thrown perfectly on a calm sea: %d skips", Best);
	CHECK(Skips(0.9, 0.97, 0.05) > TownRecord, "the town record (%d) can be beaten with a very good stone and release (%d)", TownRecord, Skips(0.9, 0.97, 0.05));
	CHECK(Skips(0.8, 0.8, 0.05) < TownRecord, "an ordinary good throw does not beat it (%d)", Skips(0.8, 0.8, 0.05));
	CHECK(Skips(0.7, 0.75, 0.1) >= 3 && Skips(0.7, 0.75, 0.1) <= 7, "a fair throw of a fair stone: %d", Skips(0.7, 0.75, 0.1));
	CHECK(Skips(1.0, 0.3, 0.05) <= 1, "a pebble does not skip however it is thrown (%d)", Skips(1.0, 0.3, 0.05));
	CHECK(Skips(0.0, 1.0, 0.05) <= 2, "a fumbled release wastes the best stone (%d)", Skips(0.0, 1.0, 0.05));
	bool bMono = true;
	for (double Q = 0.0; Q < 0.99; Q += 0.05)
	{
		bMono = bMono && Skips(Q + 0.05, 0.9, 0.0) >= Skips(Q, 0.9, 0.0);
	}
	CHECK(bMono, "a better release never skips less (flat water)");
	bMono = true;
	for (double F = 0.6; F < 0.99; F += 0.05)
	{
		bMono = bMono && Skips(0.9, F + 0.05, 0.0) >= Skips(0.9, F, 0.0);
	}
	CHECK(bMono, "a flatter stone never skips less (flat water)");
	int Calm = 0, Rough = 0;
	for (uint32_t Seed = 0; Seed < 200; ++Seed)
	{
		Calm += Skips(0.9, 0.9, 0.1, Seed);
		Rough += Skips(0.9, 0.9, 0.8, Seed);
	}
	CHECK(Rough * 2 < Calm, "waves spoil it: %.1f skips a throw on a calm sea, %.1f in 0.8 m waves", Calm / 200.0, Rough / 200.0);
	FThrowIn Sand;
	Sand.DepthM = 0.05;
	CHECK(Throw(Sand).Skips == 0, "a stone thrown onto wet sand does not skip");

	FThrowIn In;
	In.Quality = 0.9;
	In.Flatness = 0.9;
	const FThrowOut Out = Throw(In);
	bool bOrder = true, bShorter = true;
	for (size_t I = 1; I < Out.Hops.size(); ++I)
	{
		bOrder = bOrder && Out.Hops[I].X > Out.Hops[I - 1].X && Out.Hops[I].T > Out.Hops[I - 1].T;
		bShorter = bShorter && (I < 2 || Out.Hops[I].X - Out.Hops[I - 1].X < Out.Hops[I - 1].X - Out.Hops[I - 2].X + 1e-9);
	}
	CHECK(bOrder && bShorter, "every hop is farther out, later and shorter than the one before");
	CHECK(Out.Distance > 12.0 && Out.Distance < 45.0, "a good throw goes %.1f m in %.1f s", Out.Distance, Out.Seconds);
	CHECK(Out.Hops.back().SpeedOut == 0.0, "the last touch is where it sinks");
	double X = 0.0, MaxH = 0.0, LastX = -1.0;
	bool bForward = true, bAbove = true;
	for (double T = 0.0; T <= Out.Seconds + 0.2; T += 0.01)
	{
		const double H = StoneAt(Out, T, X);
		bForward = bForward && X >= LastX - 1e-9;
		bAbove = bAbove && H >= -1e-9;
		MaxH = H > MaxH ? H : MaxH;
		LastX = X;
	}
	CHECK(bForward && bAbove && MaxH <= 1.0 + 1e-9, "the stone's path goes forward, never under the water, never over the hand");
	CHECK(std::fabs(X - Out.Distance) < 1e-6, "the path ends where the stone sank");
	double XAt = 0.0;
	CHECK(std::fabs(StoneAt(Out, Out.Hops[1].T, XAt)) < 1e-9 && std::fabs(XAt - Out.Hops[1].X) < 1e-9, "on a touch the stone is on the water at the touch");
}

static void TestCastle()
{
	std::printf("the sandcastle\n");
	CHECK(CanBuild(-1.0, -1.5, -0.9), "damp sand between the water and the wet line holds");
	CHECK(!CanBuild(-1.48, -1.5, -0.9), "sand awash does not");
	CHECK(!CanBuild(0.5, -1.5, -0.9), "dry sand high up does not");
	CHECK(CanBuild(-0.6, -1.5, -0.9), "a little over the wet line it is still damp");

	FCastle C;
	CHECK(!Build(C, 1.0), "nothing is built before it is begun");
	C.bAlive = true;
	C.Z = -1.0;
	int Stages = 0;
	double Held = 0.0;
	while (C.Work < CastleStages && Held < 100.0)
	{
		Stages += Build(C, 0.1) ? 1 : 0;
		Held += 0.1;
	}
	CHECK(Stages == CastleStages && std::fabs(Held - CastleStages * StageSeconds) < 0.5, "%d stages in %.0f s of holding E", Stages, Held);
	CHECK(C.HasMoat() && C.HasFlag() && C.Stage() == CastleStages, "the whole castle has a moat and a flag");
	CHECK(!Build(C, 1.0), "a finished castle takes no more sand");

	// the ebb: the sea far below it for hours, nothing happens
	bool bGone = false;
	for (int S = 0; S < 3600 * 3 && !bGone; ++S)
	{
		bGone = Wash(C, -3.0, 0.2, false, 1.0);
	}
	CHECK(!bGone && C.Worn == 0.0, "the sea far down the flat leaves it alone");

	// a whole tide: built an hour after high water at 1 m under it, the flood takes it hours later
	FCastle Tidal = C;
	const double Low = -3.0;
	const double Start = 3.0;     // hours after high water: the water has just gone under it
	CHECK(Tide(Start, Low) < Tidal.Z - 0.1, "it was built on sand the ebb had left (the sea %.2f m, the castle %.2f m)", Tide(Start, Low), Tidal.Z);
	double Hours = 0.0;
	bGone = false;
	while (!bGone && Hours < 14.0)
	{
		bGone = Wash(Tidal, Tide(Start + Hours, Low), 0.2, false, 10.0);
		Hours += 10.0 / 3600.0;
	}
	CHECK(bGone && Hours > 6.0 && Hours < 10.5, "the next flood washes it away %.1f hours later", Hours);
	const double Said = HoursUntilWash(C.Z, 0.2, [&](double H) { return Tide(Start + H, Low); });
	CHECK(Said > 0.0 && std::fabs(Said - Hours) < 0.6, "and the hero can tell when: %.1f hours (it took %.1f)", Said, Hours);
	CHECK(HoursUntilWash(0.5, 0.2, [&](double H) { return Tide(Start + H, Low); }) < 0.0, "a castle over high water is safe from the tide");

	// a castle higher up the beach lasts longer; one with a moat a little longer than one without
	FCastle High = C;
	High.Z = -0.3;
	double HighHours = 0.0;
	bGone = false;
	while (!bGone && HighHours < 14.0)
	{
		bGone = Wash(High, Tide(Start + HighHours, Low), 0.2, false, 10.0);
		HighHours += 10.0 / 3600.0;
	}
	CHECK(bGone && HighHours > Hours + 0.5, "higher up the beach it stands longer (%.1f h against %.1f h)", HighHours, Hours);
	FCastle Bare = C, Moated = C;
	Bare.Work = 4.0;
	double BareS = 0.0, MoatS = 0.0;
	while (!Wash(Bare, -1.0 - 0.07, 0.2, false, 1.0) && BareS < 5000.0) { BareS += 1.0; }
	while (!Wash(Moated, -1.0 - 0.07, 0.2, false, 1.0) && MoatS < 5000.0) { MoatS += 1.0; }
	CHECK(BareS < 1200.0, "the run-ups at its foot take a castle in minutes (%.0f s)", BareS);
	CHECK(MoatS >= 5000.0, "a moat takes the same run-ups and the castle stands");
	FCastle Drowned = C;
	double DrownS = 0.0;
	while (!Wash(Drowned, -0.4, 0.2, false, 1.0) && DrownS < 5000.0) { DrownS += 1.0; }
	CHECK(DrownS < 60.0, "the sea standing over it flattens it at once (%.0f s)", DrownS);

	// the wear shows, and patting the sand mends it
	FCastle Half = C;
	Half.Work = 3.5;
	for (int S = 0; S < 60; ++S) { Wash(Half, -1.0, 0.2, false, 1.0); }
	const double Worn = Half.Worn;
	CHECK(Worn > 0.2 && Worn < 1.0 && Half.Standing() < 0.8, "half washed, it stands lower (%.2f)", Half.Standing());
	Build(Half, 2.0);
	CHECK(Half.Worn < Worn, "building on mends it");
	FCastle Rained = C;
	double RainHours = 0.0;
	while (!Wash(Rained, -3.0, 0.1, true, 60.0) && RainHours < 10.0) { RainHours += 1.0 / 60.0; }
	CHECK(RainHours > 2.5 && RainHours < 3.5, "rain alone melts it in about three hours (%.1f)", RainHours);
	CHECK(!Wash(Rained, -3.0, 0.1, true, 60.0), "a washed castle is washed only once");
}

static void TestRace()
{
	std::printf("the buoy swim\n");
	FRace R;
	FRaceIn In;
	CHECK(RaceStep(R, In, 1.0) == ERaceEvent::None && R.State == ERace::Idle, "no whistle, no race");
	RaceReady(R, 60.0);
	CHECK(R.State == ERace::Ready, "the whistle: on your marks");
	for (int S = 0; S < 20; ++S) { RaceStep(R, In, 1.0); }
	CHECK(R.State == ERace::Ready && R.Seconds == 0.0, "the clock waits while he wades in");
	In.bSwimming = true;
	In.bAshore = false;
	In.ToBuoyM = 55.0;
	CHECK(RaceStep(R, In, 0.1) == ERaceEvent::Started && R.State == ERace::Out, "it starts when he swims");
	// 1.5 m/s out and back
	ERaceEvent Ev = ERaceEvent::None;
	double D = 55.0;
	while (R.State == ERace::Out && R.Seconds < 500.0)
	{
		D -= 1.5 * 0.1;
		In.ToBuoyM = D;
		Ev = RaceStep(R, In, 0.1);
	}
	CHECK(Ev == ERaceEvent::Turned && R.State == ERace::Back && std::fabs(R.OutSeconds - (55.0 - BuoyTouchM) / 1.5) < 0.5, "the buoy is touched after %.1f s", R.OutSeconds);
	double Back = 0.0;
	while (R.State == ERace::Back && R.Seconds < 500.0)
	{
		Back += 1.5 * 0.1;
		In.ToBuoyM = Back;
		In.bSwimming = Back < 52.0;
		In.bAshore = Back >= 56.0;
		Ev = RaceStep(R, In, 0.1);
	}
	CHECK(Ev == ERaceEvent::Finished && R.State == ERace::Done, "the clock stops on the sand");
	CHECK(R.Seconds > 65.0 && R.Seconds < 80.0, "there and back in %.1f s", R.Seconds);
	CHECK(RaceStep(R, In, 1.0) == ERaceEvent::None && R.State == ERace::Done, "a finished race stays finished");

	CHECK(Medal(2.0 * 60.0 / 1.6, 60.0) == 3, "the hero's best stroke all the way is gold");
	CHECK(Medal(2.0 * 60.0 / 1.2, 60.0) == 2 && Medal(2.0 * 60.0 / 1.0, 60.0) == 1 && Medal(2.0 * 60.0 / 0.6, 60.0) == 0, "silver, bronze, and just a swim");
	CHECK(Medal(100.0, 60.0) == Medal(200.0, 120.0), "a longer course is judged by the same speed");
	CHECK(LifeguardSeconds(60.0) > 2.0 * 60.0 / 1.6 && Medal(LifeguardSeconds(60.0), 60.0) == 3, "the lifeguards' time (%.0f s) is gold and can just be beaten", LifeguardSeconds(60.0));

	FRace Lazy;
	RaceReady(Lazy, 60.0);
	FRaceIn Ashore;
	Ev = ERaceEvent::None;
	for (int S = 0; S < 200 && Ev == ERaceEvent::None; ++S) { Ev = RaceStep(Lazy, Ashore, 1.0); }
	CHECK(Ev == ERaceEvent::GaveUp && Lazy.State == ERace::Idle, "he who stays ashore is forgotten after a minute and a half");
	FRace Quit;
	RaceReady(Quit, 60.0);
	FRaceIn Wet;
	Wet.bSwimming = true;
	Wet.bAshore = false;
	RaceStep(Quit, Wet, 0.1);
	for (int S = 0; S < 10; ++S) { RaceStep(Quit, Wet, 1.0); }
	Ev = RaceStep(Quit, Ashore, 1.0);
	CHECK(Ev == ERaceEvent::GaveUp && Quit.State == ERace::Failed, "coming back without the buoy is no race");
	FRace Slow;
	RaceReady(Slow, 60.0);
	RaceStep(Slow, Wet, 0.1);
	Ev = ERaceEvent::None;
	for (int S = 0; S < 1000 && Ev == ERaceEvent::None; ++S) { Ev = RaceStep(Slow, Wet, 1.0); }
	CHECK(Ev == ERaceEvent::GaveUp && Slow.Seconds <= SlowestPerM * 120.0 + 1.5, "a swimmer who floats about is called in after %.0f s", Slow.Seconds);
}

static void TestFinds()
{
	std::printf("shells and amber\n");
	const double Calm[4] = { 3.0, 6.0, 8.5, 5.0 };
	const double Fresh[4] = { 3.0, 13.0, 8.0, 5.0 };
	const double Gale[4] = { 12.0, 19.0, 22.0, 9.0 };
	CHECK(StormScore(Calm, 4) == 0.0 && StormScore(Gale, 4) == 1.0, "a calm day is no storm, a gale is a whole one");
	CHECK(std::fabs(StormScore(Fresh, 4) - 0.5) < 1e-9, "a strong wind is half a storm");
	CHECK(StormScore(nullptr, 0) == 0.0, "no wind known: no storm");

	// a kilometre of waterline over ten days
	const int Cells = static_cast<int>(1000.0 / FindCellM);
	int ShellsCalm = 0, AmberCalm = 0, ShellsStorm = 0, AmberStorm = 0, Flies = 0;
	std::set<int> Kinds;
	bool bPlaced = true;
	for (int Day = 100; Day < 110; ++Day)
	{
		std::vector<FFind> A, B;
		Finds(0, Cells - 1, Day, 0.0, A);
		Finds(0, Cells - 1, Day, 1.0, B);
		for (const FFind& F : A) { (IsAmber(F.Kind) ? AmberCalm : ShellsCalm)++; }
		for (const FFind& F : B)
		{
			(IsAmber(F.Kind) ? AmberStorm : ShellsStorm)++;
			Flies += F.Kind == EFind::AmberFly ? 1 : 0;
			Kinds.insert(static_cast<int>(F.Kind));
			bPlaced = bPlaced && F.Along >= F.Cell * FindCellM && F.Along < (F.Cell + 1) * FindCellM && F.Across >= 0.0 && F.Across <= 1.0
				&& (!IsAmber(F.Kind) || F.Across > 0.7);
		}
	}
	CHECK(ShellsCalm > 100 && ShellsCalm < 220, "a calm kilometre has a shell every 60 to 100 m (%.0f a day)", ShellsCalm / 10.0);
	CHECK(AmberCalm <= 12, "and hardly any amber (%d in ten days)", AmberCalm);
	CHECK(ShellsStorm > ShellsCalm * 3, "after a storm: several times the shells (%.0f a day)", ShellsStorm / 10.0);
	CHECK(AmberStorm > 80 && AmberStorm < 200, "and amber, a piece every 60 to 150 m (%.0f a day)", AmberStorm / 10.0);
	CHECK(Flies > 0 && Flies * 6 < AmberStorm, "a piece with a fly in it is a rarity (%d of %d)", Flies, AmberStorm);
	CHECK(Kinds.size() == static_cast<size_t>(EFind::Num), "every kind turns up (%d of %d)", (int)Kinds.size(), (int)EFind::Num);
	CHECK(bPlaced, "every find lies in its cell, amber at the top of the wet sand");
	FFind F1, F2;
	bool bSame = true, bDiffer = false;
	for (int Cell = 0; Cell < 200; ++Cell)
	{
		const bool b1 = FindIn(Cell, 105, 0.8, F1), b2 = FindIn(Cell, 105, 0.8, F2), b3 = FindIn(Cell, 106, 0.8, F2);
		bSame = bSame && b1 == b2 && (!b1 || (F1.Along == F2.Along || b3));
		bDiffer = bDiffer || b1 != b3;
	}
	CHECK(bSame, "the same waterline all day");
	CHECK(bDiffer, "another one tomorrow");
	for (int K = 0; K < static_cast<int>(EFind::Num); ++K)
	{
		CHECK(std::strlen(FindId(static_cast<EFind>(K))) > 0, "find %d has an id", K);
	}
}

static void TestHelpers()
{
	std::printf("children at the castle\n");
	auto Time = [](int Helpers)
	{
		FCastle C;
		C.bAlive = true;
		double T = 0.0;
		while (C.Work < CastleStages && T < 600.0)
		{
			Build(C, 0.1, Helpers);
			T += 0.1;
		}
		return T;
	};
	const double Alone = Time(0), One = Time(1), Two = Time(2), Five = Time(5);
	CHECK(std::fabs(Alone - CastleStages * StageSeconds) < 0.2, "alone a castle takes %.1f s (%.1f)", Alone, CastleStages * StageSeconds);
	CHECK(One < Alone && Two < One, "each child helping speeds it: %.1f, %.1f, %.1f s", Alone, One, Two);
	CHECK(std::fabs(Two - Alone / (1.0 + HelperBoost * MaxHelpers)) < 0.3, "two children: %.1f s", Two);
	CHECK(std::fabs(Five - Two) < 0.11, "more than %d help no more: %.1f vs %.1f", MaxHelpers, Five, Two);
	FCastle Dead;
	CHECK(!Build(Dead, 1.0, 2), "no castle, nothing to help with");
}

static void TestRivals()
{
	std::printf("summer visitors at the buoy\n");
	int Out[MaxRivals];
	CHECK(TodaysRivals(9000, 10, 5, 18.0, Out) == 0, "no visitors in October");
	CHECK(TodaysRivals(9000, 7, 20, 13.0, Out) == 0, "none in cold water");
	CHECK(TodaysRivals(9000, 6, 10, 19.0, Out) == 0, "none before mid-June");
	int Days = 0, Total = 0;
	for (int D = 0; D < 300; ++D)
	{
		const int N = TodaysRivals(9000 + D, 7, 20, 19.0, Out);
		CHECK(N >= 0 && N <= MaxRivals, "never more than %d visitors (%d)", MaxRivals, N);
		Days += N > 0 ? 1 : 0;
		Total += N;
		for (int I = 0; I < N; ++I)
		{
			CHECK(Out[I] >= 0 && Out[I] < RivalCount, "a real visitor (%d)", Out[I]);
		}
	}
	CHECK(Days > 270 && Total > 600, "most summer days somebody swims with him: %d days, %.2f a day", Days, Total / 300.0);
	int A[MaxRivals], B[MaxRivals];
	const int Na = TodaysRivals(9123, 8, 1, 20.0, A), Nb = TodaysRivals(9123, 8, 1, 20.0, B);
	CHECK(Na == Nb && std::equal(A, A + Na, B), "the same visitors all day");
	// a course like the first tower's: the buoy 170 m out
	const double Course = 170.0;
	double Fastest = 1e9, Slowest = 0.0;
	for (int I = 0; I < RivalCount; ++I)
	{
		const double T = RivalSeconds(I, Course, 9050);
		Fastest = std::min(Fastest, T);
		Slowest = std::max(Slowest, T);
		const double End = RivalAlong(I, Course, T, 9050);
		CHECK(std::fabs(End - 2.0 * Course) < 0.05, "%s is in at its time (%.2f of %.0f m)", Rival(I).Id, End, 2.0 * Course);
		CHECK(RivalAlong(I, Course, T * 0.5, 9050) > Course * 0.95, "%s: half the time, past half the way (it fades)", Rival(I).Id);
		CHECK(RivalAlong(I, Course, 0.0, 9050) == 0.0 && RivalAlong(I, Course, T * 3.0, 9050) <= 2.0 * Course, "%s starts at 0, never past the end", Rival(I).Id);
		const double Mean = 2.0 * Course / T;
		CHECK(Mean > 1.1 && Mean < 1.8, "%s swims a believable %.2f m/s", Rival(I).Id, Mean);
	}
	// the hero swims 1.6 m/s: he beats some, not all
	const double Hero = 2.0 * Course / 1.6;
	double All[RivalCount];
	for (int I = 0; I < RivalCount; ++I) { All[I] = RivalSeconds(I, Course, 9050); }
	const int Place = RacePlace(Hero, All, RivalCount);
	CHECK(Place > 1 && Place < RivalCount + 1, "at 1.6 m/s against all six: mid-field (place %d)", Place);
	CHECK(Fastest < LifeguardSeconds(Course), "the fastest visitor beats the lifeguards' time (%.0f < %.0f)", Fastest, LifeguardSeconds(Course));
	CHECK(RacePlace(1.0, All, RivalCount) == 1 && RacePlace(1e9, All, RivalCount) == RivalCount + 1, "first and last places");
}

static void TestSelling()
{
	std::printf("selling finds\n");
	CHECK(!std::strcmp(FindItem(EFind::Cockle), "seashell") && !std::strcmp(FindItem(EFind::Whelk), "seashell"), "shells are shells");
	CHECK(!std::strcmp(FindItem(EFind::Amber), "amber_piece") && !std::strcmp(FindItem(EFind::AmberFly), "amber_fly"), "amber, and amber with a fly");
	CHECK(SellPrice("seashell") > 0 && SellPrice("amber_piece") > SellPrice("seashell") && SellPrice("amber_fly") > SellPrice("amber_piece"),
		"a fly in amber dearest, then amber, then shells (%d, %d, %d)", SellPrice("seashell"), SellPrice("amber_piece"), SellPrice("amber_fly"));
	CHECK(SellPrice("amber_charm") == 0 && SellPrice(nullptr) == 0, "the seller buys only what the sea gave");
}

static void TestBook()
{
	std::printf("the book\n");
	FBook B;
	CHECK(!NoteThrow(B, 2).bBest && B.BestSkips == 2 && B.Throws == 1, "two skips are nothing to write down");
	CHECK(NoteThrow(B, 6).bBest && B.BestSkips == 6, "six are a best");
	CHECK(!NoteThrow(B, 5).bBest && B.BestSkips == 6 && B.Throws == 3, "a worse throw changes nothing");
	FNews N = NoteThrow(B, TownRecord);
	CHECK(N.bBest && !N.bTownRecord, "equalling the town record is not beating it");
	N = NoteThrow(B, TownRecord + 1);
	CHECK(N.bBest && N.bTownRecord, "one more is the new record");
	N = NoteThrow(B, TownRecord + 3);
	CHECK(N.bBest && !N.bTownRecord, "the record is beaten once");
	CHECK(NoteSwim(B, 90.0).bBest && !NoteSwim(B, 95.0).bBest && NoteSwim(B, 80.0).bBest && B.BestSwim == 80.0 && B.Swims == 3, "the best swim is the shortest");
	CHECK(NoteFind(B, EFind::Cockle).bFirstOfKind && !NoteFind(B, EFind::Cockle).bFirstOfKind && B.Shells == 2, "a first cockle is news once");
	CHECK(NoteFind(B, EFind::AmberFly).bFirstOfKind && B.Ambers == 1 && B.Shells == 2, "amber is counted apart");
}

int main(int Argc, char** Argv)
{
	const char* Mode = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Mode, "throws"))
	{
		for (double Wave : { 0.05, 0.3, 0.8 })
		{
			std::printf("waves %.2f m: skips by release quality (columns 0 .. 1) and flatness (rows)\n", Wave);
			for (double F : { 0.3, 0.6, 0.7, 0.8, 0.9, 1.0 })
			{
				std::printf("  flat %.1f:", F);
				for (double Q = 0.0; Q <= 1.001; Q += 0.1)
				{
					std::printf(" %2d", Skips(Q, F, Wave));
				}
				std::printf("\n");
			}
		}
		return 0;
	}
	if (!std::strcmp(Mode, "shore"))
	{
		for (double Storm : { Argc > 2 ? std::atof(Argv[2]) : 0.0, Argc > 2 ? std::atof(Argv[2]) : 1.0 })
		{
			std::printf("storm %.2f: a kilometre of waterline, ten days\n", Storm);
			for (int Day = 0; Day < 10; ++Day)
			{
				std::vector<FFind> Found;
				Finds(0, static_cast<int>(1000.0 / FindCellM) - 1, 300 + Day, Storm, Found);
				int Count[static_cast<int>(EFind::Num)] = {};
				for (const FFind& F : Found) { Count[static_cast<int>(F.Kind)]++; }
				std::printf("  day %d:", Day);
				for (int K = 0; K < static_cast<int>(EFind::Num); ++K) { std::printf(" %s %d", FindId(static_cast<EFind>(K)), Count[K]); }
				std::printf("\n");
			}
			if (Argc > 2) { break; }
		}
		return 0;
	}
	TestStones();
	TestCastle();
	TestRace();
	TestFinds();
	TestHelpers();
	TestRivals();
	TestSelling();
	TestBook();
	std::printf("beachsim: %d checks, %d failed\n", GPassed + GFailed, GFailed);
	return GFailed ? 1 : 0;
}
