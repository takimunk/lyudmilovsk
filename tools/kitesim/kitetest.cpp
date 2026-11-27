// Checks for the kite's lift model (Private/Kite/KiteModel.cpp) outside the engine, in the game's own wind profile and
// gust noise (Private/Weather/WindModel.cpp): the five kinds, the stunt kite's window and steering, the air (rising,
// sinking, tumbling), the launches, the tricks, a caught kite got down. See run.sh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

#include "KiteHum.h"
#include "KiteModel.h"
#include "WindModel.h"

using namespace LyudKite;

static int Failures = 0;
static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-66s %.3f\n", Ok ? "ok  " : "FAIL", What, Value);
	if (!Ok) Failures++;
}

struct FRun
{
	EState End = EState::Wound;
	double MeanElev = 0.0;      // degrees over the last 30 s (while up)
	double MeanHeight = 0.0;    // m over the hand, last 30 s
	double MaxTension = 0.0;
	double UpShare = 0.0;       // share of the last 60 s in the air
	double FirstDown = -1.0;    // s when it first lay on the ground
	double FirstSpin = -1.0;    // s when it first span
	int Spins = 0;
	double MaxHum = 0.0;
	bool bFinite = true;
	double Line = 0.0;
	double MinHeightLate = 1e9; // m: the lowest it came in the last 30 s
	double SwaySq = 0.0, SwayT = 0.0;   // its wandering across the wind, m^2 s
	double Sway() const { return SwayT > 0.0 ? std::sqrt(SwaySq / SwayT) : 0.0; }
};

/** The wind at the kite: the game's log profile over land and a gust noise (period ~6 s and ~25 s) of the given spread. */
static FVec WindAt(double U10, double Gusty, const FVec& At, double T, uint32_t Seed)
{
	const double G = 1.0 + Gusty * (0.7 * LyudWeather::SmoothNoise1(T / 6.0, Seed) + 0.5 * LyudWeather::SmoothNoise1(T / 25.0, Seed + 7));
	const double Speed = std::max(0.0, U10 * LyudWeather::HeightFactor(float(std::max(At.Z, 0.3)), false) * G);
	const double Veer = 0.15 * Gusty * LyudWeather::SmoothNoise1(T / 9.0, Seed + 3);
	return FVec(std::cos(Veer) * Speed, std::sin(Veer) * Speed, 0.0);
}

/** What the air does beside the open wind: where the flyer stands (a lee, a cliff's edge), and when. */
struct FAir
{
	double Factor = 1.0;        // on the speed (a lee: 0.45)
	double Updraft = 0.0;       // m/s, + rising
	double Turbulence = 0.0;    // rms m/s
	double LullFrom = -1.0, LullTo = -1.0;  // s: the wind all but stops between these
	double LullFactor = 0.08;   // (or blows this much harder: a squall)
	double Run = 0.0;           // m/s the flyer runs upwind at
	double RunFor = 0.0;        // s (from each launch)
	double Long = 0.0;          // > 0: launched by a helper this far downwind instead of tossed
};

/** A flyer on flat ground works the reel by AutoReel toward Want metres for Seconds. */
static FRun Fly(const FKiteDef& Def, double U10, double Gusty, double Seconds, double Want, uint32_t Seed = 5,
	double (*Reel)(const FKite&, const FVec&, double, double) = nullptr, const FAir& Air = FAir(), FTricks* Tricks = nullptr)
{
	FRun Out;
	FKite K;
	K.Def = Def;
	K.Seed = Seed;
	FVec Hand(0, 0, 1.4);
	auto Start = [&K, &Hand, &Air, U10, Gusty, Seed](double T)
	{
		if (Air.Long > 0.0)
		{
			LongLaunch(K, Hand, Hand + FVec(Air.Long, 0, 0.6), WindAt(U10, Gusty, Hand, T, Seed));
		}
		else
		{
			Launch(K, Hand, WindAt(U10, Gusty, Hand, T, Seed));
		}
	};
	Start(0.0);
	const double Dt = 1.0 / 60.0;
	double ElevSum = 0.0, HSum = 0.0, Up = 0.0, Last = 0.0, UpLast = 0.0, Wait = 0.0, Since = 0.0;
	EState Prev = K.State;
	for (double T = 0.0; T < Seconds; T += Dt)
	{
		if (K.State == EState::Wound)
		{
			// wound up: the flyer tries again after a few seconds
			Wait += Dt;
			if (Wait > 4.0)
			{
				Start(T);
				Wait = 0.0;
				Since = 0.0;
			}
		}
		Since += Dt;
		if (Air.Run > 0.0 && Since < Air.RunFor && K.State != EState::Wound)
		{
			Hand.X -= Air.Run * Dt;     // upwind
		}
		FKiteInput In;
		In.Hand = Hand;
		In.Wind = WindAt(U10, Gusty, K.Pos, T, Seed) * (Air.Factor * (T >= Air.LullFrom && T < Air.LullTo ? Air.LullFactor : 1.0));
		In.Wind.Z = Air.Updraft;
		In.Turbulence = Air.Turbulence;
		In.GroundZ = 0.0;
		In.Reel = Reel ? Reel(K, Hand, Want, T) : AutoReel(K, Hand, Want);
		Step(K, In, Dt);
		if (Tricks)
		{
			Track(*Tricks, K, In, Dt);
		}
		Out.MinHeightLate = T > Seconds - 30.0 ? std::min(Out.MinHeightLate, K.Pos.Z) : Out.MinHeightLate;
		Out.SwaySq += IsUp(K) ? K.Pos.Y * K.Pos.Y * Dt : 0.0;
		Out.SwayT += IsUp(K) ? Dt : 0.0;
		Out.bFinite = Out.bFinite && std::isfinite(K.Pos.X + K.Pos.Y + K.Pos.Z + K.Vel.X + K.Tension);
		Out.MaxTension = std::max(Out.MaxTension, K.Tension);
		Out.MaxHum = std::max(Out.MaxHum, Hum(K));
		if (K.State == EState::Grounded && Out.FirstDown < 0.0) Out.FirstDown = T;
		if (K.State == EState::Spinning && Prev != EState::Spinning)
		{
			Out.Spins++;
			if (Out.FirstSpin < 0.0) Out.FirstSpin = T;
		}
		Prev = K.State;
		if (T > Seconds - 60.0)
		{
			UpLast += Dt;
			Up += IsUp(K) ? Dt : 0.0;
		}
		if (T > Seconds - 30.0 && IsUp(K))
		{
			ElevSum += Elevation(K, Hand) * Dt;
			HSum += (K.Pos.Z - Hand.Z) * Dt;
			Last += Dt;
		}
		if (IsLost(K)) break;
	}
	Out.End = K.State;
	Out.MeanElev = Last > 0.0 ? ElevSum / Last : 0.0;
	Out.MeanHeight = Last > 0.0 ? HSum / Last : 0.0;
	Out.UpShare = UpLast > 0.0 ? Up / UpLast : 0.0;
	Out.Line = K.Line;
	return Out;
}

static double Hold(const FKite&, const FVec&, double, double) { return 0.0; }
static double LetOutFast(const FKite& K, const FVec&, double Want, double) { return K.Line < Want ? ReelOut : 0.0; }

static void Print(const char* Name, const FRun& R)
{
	std::printf("     %-34s end %-9s elev %5.1f deg  height %5.1f m  up %3.0f%%  line %4.1f m  T max %5.1f N  spins %d  hum %.2f\n",
		Name, StateName(R.End), R.MeanElev, R.MeanHeight, R.UpShare * 100.0, R.Line, R.MaxTension, R.Spins, R.MaxHum);
}

// ---------------------------------------------------------------- two lines

struct FStuntRun
{
	EState End = EState::Wound;
	double UpShare = 0.0;
	double MeanSpeed = 0.0, MaxSpeed = 0.0, MeanTension = 0.0, MaxTension = 0.0;
	int Crossings = 0;          // times it crossed the middle of the window
	int Crashes = 0;            // times it lay on its nose
	int Launches = 0;
	double MinHeight = 1e9;     // m, once it has been up 3 s
	double MaxAz = 0.0, MaxEl = 0.0;
	bool bFinite = true;
	FTricks Tricks;
	FKite Kite;
};

/** run.sh trace N: the Nth stunt flight of the checks printed four times a second (to see why a scripted trick went wrong). */
static int GTraceRun = -1, GStuntRuns = 0;

/** Steer (-1..1) as a function of the kite, this step's input (the pilot may set Pull) and the time. */
using FPilot = std::function<double(const FKite&, FKiteInput&, double)>;

/** Toward a heading (degrees: 0 up, 90 right, 180 down), the short way. */
static double SteerTo(const FKite& K, const FVec& Hand, double Deg)
{
	double E = Deg - Heading(K, Hand);
	while (E > 180.0) E -= 360.0;
	while (E < -180.0) E += 360.0;
	return std::max(-1.0, std::min(1.0, E / 25.0));
}

/**
 * A stunt kite flown from a long launch (a helper 22 m downwind lets go on the pull; the flyer takes a few steps back).
 * Pilot null = AutoSteer. A kite that has lain for 3 s is set up and launched again (bAgain).
 */
static FStuntRun FlyStunt(double U10, double Gusty, double Seconds, uint32_t Seed = 5, const FPilot& Pilot = nullptr, const FAir& Air = FAir(), bool bAgain = true,
	const FVec* Walk = nullptr)
{
	FStuntRun Out;
	FKite& K = Out.Kite;
	K.Def = Stunt();
	K.Seed = Seed;
	FVec Hand(0, 0, 1.4);
	const double Dt = 1.0 / 60.0;
	LongLaunch(K, Hand, Hand + FVec(K.Def.FlyLine, 0, 0.6), WindAt(U10, Gusty, Hand, 0.0, Seed));
	Out.Launches = 1;
	double Up = 0.0, Lain = 0.0, SpeedSum = 0.0, TenSum = 0.0, Since = 0.0;
	int Side = 0;
	const bool bTrace = ++GStuntRuns == GTraceRun;
	int Traced = 0;
	for (double T = 0.0; T < Seconds; T += Dt)
	{
		FKiteInput In;
		const bool bLaunching = K.State == EState::Held || (K.State == EState::Flying && K.Clock < 1.2);
		if (bLaunching && K.State != EState::Held)
		{
			Hand.X -= 2.5 * Dt;     // the steps back that go with the pull
		}
		if (Walk)
		{
			Hand += *Walk * Dt;     // where the pilot walks
		}
		In.Hand = Hand;
		In.Wind = WindAt(U10, Gusty, K.Pos, T, Seed) * (Air.Factor * (T >= Air.LullFrom && T < Air.LullTo ? Air.LullFactor : 1.0));
		In.Wind.Z = Air.Updraft;
		In.Turbulence = Air.Turbulence;
		In.Pull = bLaunching ? 1.0 : 0.0;
		In.Steer = Pilot ? Pilot(K, In, T) : AutoSteer(K, Hand, In.Wind, 0.0);
		const EState Was = K.State;
		Step(K, In, Dt);
		const ETrick Got = Track(Out.Tricks, K, In, Dt);
		if (bTrace && (Got != ETrick::None || T >= Traced * 0.25))
		{
			Traced += Got == ETrick::None ? 1 : 0;
			double Az, El;
			Window(K, Hand, In.Wind, Az, El);
			std::printf("%7.2f %-8s az %6.1f el %5.1f h %5.2f head %6.1f v %5.1f vz %5.1f air %5.1f T %6.1f twist %5.2f steer %5.2f %s\n", T, StateName(K.State), Az, El, K.Pos.Z,
				Heading(K, Hand), Length(K.Vel), K.Vel.Z, K.Air, K.Tension, K.Twist, In.Steer, Got != ETrick::None ? TrickName(Got) : "");
		}
		Out.bFinite = Out.bFinite && std::isfinite(K.Pos.X + K.Pos.Y + K.Pos.Z + K.Vel.X + K.Tension + K.Twist);
		if (K.State == EState::Grounded && Was == EState::Flying && !K.bNoseUp) Out.Crashes++;
		if (IsUp(K))
		{
			Up += Dt;
			Since += Dt;
			const double V = Length(K.Vel);
			SpeedSum += V * Dt;
			TenSum += K.Tension * Dt;
			Out.MaxSpeed = std::max(Out.MaxSpeed, V);
			Out.MaxTension = std::max(Out.MaxTension, K.Tension);
			double Az, El;
			Window(K, Hand, In.Wind, Az, El);
			Out.MaxAz = std::max(Out.MaxAz, std::fabs(Az));
			Out.MaxEl = std::max(Out.MaxEl, El);
			if (Since > 3.0) Out.MinHeight = std::min(Out.MinHeight, K.Pos.Z);
			const int Now = Az > 10.0 ? 1 : (Az < -10.0 ? -1 : Side);
			if (Now != Side && Side != 0) Out.Crossings++;
			Side = Now;
			Lain = 0.0;
		}
		else
		{
			Since = 0.0;
			Lain += Dt;
			if (bAgain && Lain > 3.0 && K.State == EState::Grounded)
			{
				LongLaunch(K, Hand, Hand + FVec(K.Def.FlyLine, 0, 0.6), In.Wind);
				Out.Launches++;
				Lain = 0.0;
			}
		}
		if (IsLost(K)) break;
	}
	Out.End = K.State;
	Out.UpShare = Up / Seconds;
	Out.MeanSpeed = Up > 0.0 ? SpeedSum / Up : 0.0;
	Out.MeanTension = Up > 0.0 ? TenSum / Up : 0.0;
	return Out;
}

static void Print(const char* Name, const FStuntRun& R)
{
	std::printf("     %-34s end %-9s up %3.0f%%  speed %4.1f (max %4.1f) m/s  T %5.1f (max %5.1f) N  crossings %d  crashes %d  launches %d  low %4.1f m  tricks %d:",
		Name, StateName(R.End), R.UpShare * 100.0, R.MeanSpeed, R.MaxSpeed, R.MeanTension, R.MaxTension, R.Crossings, R.Crashes, R.Launches,
		R.MinHeight > 1e8 ? 0.0 : R.MinHeight, R.Tricks.Count);
	for (int I = 1; I < int(ETrick::Count); ++I)
	{
		if (R.Tricks.By[I] > 0) std::printf(" %s x%d", TrickName(ETrick(I)), R.Tricks.By[I]);
	}
	std::printf("\n");
}

/** The other three kinds beside the diamond: where each stands in the sky and what wind it wants. */
static void TestKinds()
{
	const FKiteDef Dia = Diamond(), Del = Delta(), Dra = Dragon();
	Check(ByShape(EShape::Delta).Shape == EShape::Delta && ByShape(EShape::Stunt).Lines == 2 && ByShape(EShape::Dragon).TailLength > 8.0
		&& ByShape(EShape::Diamond).Area == Dia.Area && ByShape(EShape::Box).Mass == Box().Mass, "ByShape gives each kind (the dragon's tail, m)", Dra.TailLength);

	// the delta: up in a breath of wind, and high over the flyer
	const FRun DelCalm = Fly(Del, 2.2, 0.15, 150.0, 25.0);
	const FRun DiaCalm = Fly(Dia, 2.2, 0.15, 150.0, 25.0);
	Print("delta, 2.2 m/s", DelCalm);
	Print("diamond, 2.2 m/s", DiaCalm);
	Check(DelCalm.UpShare > 0.9 && DelCalm.MeanHeight > 15.0 && DiaCalm.MeanHeight < 3.0, "2.2 m/s: the delta flies where the diamond can't (m over the hand)", DelCalm.MeanHeight);
	const FRun DelGood = Fly(Del, 5.0, 0.2, 180.0, 25.0), DiaGood = Fly(Dia, 5.0, 0.2, 180.0, 25.0), DraGood = Fly(Dra, 7.0, 0.2, 180.0, 25.0), DiaSeven = Fly(Dia, 7.0, 0.2, 180.0, 25.0);
	Print("delta, 5 m/s", DelGood);
	Print("diamond, 5 m/s", DiaGood);
	Print("dragon, 7 m/s", DraGood);
	Check(DelGood.MeanElev > DiaGood.MeanElev + 6.0 && DelGood.MeanElev < 88.0, "the delta stands higher over the flyer than the diamond (degrees)", DelGood.MeanElev);
	Check(DraGood.UpShare > 0.98 && DraGood.MeanElev < DiaSeven.MeanElev - 10.0 && DraGood.MeanElev > 20.0, "the dragon hangs low on its line (degrees)", DraGood.MeanElev);
	Check(DraGood.Sway() < 0.75 * DiaSeven.Sway(), "... and wanders less across the wind than the diamond (m rms)", DraGood.Sway());
	std::printf("     sway across the wind at 7 m/s: diamond %.2f m, dragon %.2f m\n", DiaSeven.Sway(), DraGood.Sway());
	// what each can take
	const FRun DelFresh = Fly(Del, 10.0, 0.3, 120.0, 25.0), DraFresh = Fly(Dra, 14.0, 0.3, 150.0, 25.0);
	Print("delta, 10 m/s", DelFresh);
	Print("dragon, 14 m/s", DraFresh);
	Check(DelFresh.Spins >= 1, "a fresh breeze (10 m/s) is too much for the delta: it spins", (double)DelFresh.Spins);
	Check(DraFresh.Spins == 0 && DraFresh.UpShare > 0.95, "the dragon rides 14 m/s on its tail", DraFresh.UpShare);
	Check(DraGood.MaxTension > DiaSeven.MaxTension, "the dragon pulls harder than the diamond in the same wind (N)", DraGood.MaxTension);

	// a lull: the wind all but stops for 9 s. The diamond drops; the delta glides on a slack line and loses far less
	FAir Lull;
	Lull.LullFrom = 80.0;
	Lull.LullTo = 89.0;
	auto Drop = [&Lull](const FKiteDef& D)
	{
		FKite K;
		K.Def = D;
		const FVec Hand(0, 0, 1.4);
		Launch(K, Hand, WindAt(5.0, 0.0, Hand, 0.0, 5));
		double Before = 0.0, Low = 1e9;
		for (int I = 0; I < 89 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			In.Hand = Hand;
			In.Wind = WindAt(5.0, 0.0, K.Pos, T, 5) * (T >= Lull.LullFrom ? 0.08 : 1.0);
			In.Reel = T < Lull.LullFrom ? AutoReel(K, Hand, 25.0) : 0.0;     // a flyer who just stands and watches
			Step(K, In, 1.0 / 60.0);
			if (T < Lull.LullFrom) Before = K.Pos.Z;
			else Low = std::min(Low, K.Pos.Z);
		}
		return std::make_pair(Before, Before - Low);
	};
	const auto DiaDrop = Drop(Dia), DelDrop = Drop(Del);
	std::printf("     a 9 s lull on a held line: the diamond drops %.1f m of %.1f, the delta %.1f m of %.1f\n", DiaDrop.second, DiaDrop.first, DelDrop.second, DelDrop.first);
	Check(DelDrop.second < 0.6 * DiaDrop.second && DelDrop.second < 0.5 * DelDrop.first, "in a lull the delta glides: it loses much less height than the diamond (m)", DelDrop.second);
}

/** The stunt kite: the window's shape, steering, twists, the ground, AutoSteer. */
static void TestStunt()
{
	// AutoSteer: five minutes of eights in 7 m/s
	const FStuntRun Eights = FlyStunt(7.0, 0.15, 300.0);
	Print("stunt, 7 m/s, AutoSteer 5 min", Eights);
	Check(Eights.Crashes == 0 && Eights.Launches == 1 && Eights.UpShare > 0.99, "AutoSteer flies a stunt kite 5 minutes in 7 m/s without touching the ground", Eights.UpShare);
	Check(Eights.Crossings >= 60, "... in eights: back and forth across the window (crossings)", (double)Eights.Crossings);
	Check(Eights.MinHeight > 3.0, "... never lower than 3 m once up", Eights.MinHeight);
	Check(Eights.Tricks.By[int(ETrick::LoopLeft)] + Eights.Tricks.By[int(ETrick::LoopRight)] >= 3, "... with a loop now and then", (double)(Eights.Tricks.By[int(ETrick::LoopLeft)] + Eights.Tricks.By[int(ETrick::LoopRight)]));
	Check(Eights.MeanSpeed > 1.6 * 7.0 && Eights.MaxSpeed < 5.0 * 7.0, "a stunt kite flies about twice the wind's speed (m/s)", Eights.MeanSpeed);
	Check(Eights.bFinite && Eights.MaxTension < Stunt().BreakN, "... and its lines hold (N)", Eights.MaxTension);

	// the window: parked at the top it is slow and light; low in the middle fast and heavy; at the edge it stalls
	{
		double TopSpeed = 0.0, TopPull = 0.0, TopEl = 0.0, MidSpeed = 0.0, MidPull = 0.0;
		int MidN = 0, TopN = 0;
		const FStuntRun Park = FlyStunt(7.0, 0.0, 30.0, 5, [&](const FKite& K, FKiteInput& In, double T)
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			if (T > 20.0 && IsUp(K)) { TopSpeed += Length(K.Vel); TopPull += K.Tension; TopEl += El; TopN++; }
			return SteerTo(K, In.Hand, 0.0);
		});
		const FStuntRun Mid = FlyStunt(7.0, 0.0, 60.0, 5, [&](const FKite& K, FKiteInput& In, double)
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			if (IsUp(K) && std::fabs(Az) < 15.0 && El < 40.0 && K.Clock > 3.0) { MidSpeed += Length(K.Vel); MidPull += K.Tension; MidN++; }
			return AutoSteer(K, In.Hand, In.Wind, 0.0, false);
		});
		TopSpeed /= std::max(TopN, 1); TopPull /= std::max(TopN, 1); TopEl /= std::max(TopN, 1);
		MidSpeed /= std::max(MidN, 1); MidPull /= std::max(MidN, 1);
		std::printf("     the window at 7 m/s: middle %.1f m/s %.0f N; parked at the top (%.0f deg) %.1f m/s %.0f N\n", MidSpeed, MidPull, TopEl, TopSpeed, TopPull);
		Check(Park.End == EState::Flying && TopEl > 55.0 && TopEl < 85.0 && TopSpeed < 1.5, "nose up it climbs to the top of the window and parks there (degrees)", TopEl);
		Check(MidN > 100 && MidSpeed > 12.0 && MidSpeed > 6.0 * std::max(TopSpeed, 0.5), "low in the middle of the window it is fast (m/s)", MidSpeed);
		Check(MidPull > 3.0 * TopPull && TopPull > 2.0, "... and pulls several times harder than at the top (N)", MidPull);
		Check(Mid.Crashes == 0, "... (no crash on the way)", (double)Mid.Crashes);

		// flown level to the right until the window ends: it slows, stalls and comes down at the edge
		double EdgeSpeed = 1e9, EdgeAz = 0.0, EdgePull = 0.0;
		const FStuntRun Edge = FlyStunt(7.0, 0.0, 40.0, 5, [&](const FKite& K, FKiteInput& In, double)
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			if (K.State == EState::Flying && K.Clock > 3.0 && Az > EdgeAz) { EdgeAz = Az; EdgeSpeed = Length(K.Vel); EdgePull = K.Tension; }
			return K.Clock < 1.5 ? SteerTo(K, In.Hand, 0.0) : SteerTo(K, In.Hand, El > 25.0 ? 100.0 : 80.0);
		}, FAir(), false);
		std::printf("     flown level to the right: furthest %.0f deg off the wind, %.1f m/s and %.0f N there, ends %s\n", EdgeAz, EdgeSpeed, EdgePull, StateName(Edge.End));
		Check(EdgeAz > 55.0 && EdgeAz < 95.0 && EdgeSpeed < 4.0, "the window ends 60-90 degrees off the wind: flown level it stops there (degrees)", EdgeAz);
		Check(EdgePull < 0.4 * MidPull, "... hanging light on its lines (N)", EdgePull);
	}

	// loops wind the lines: each loop a turn, and more than three bind the steering until it is looped back
	{
		struct FLoops { int Want = 0; double From = 0.0; bool bOn = false; int Done = 0; double GainAfter = 1.0; double TwistAfter = 0.0; bool bBack = false; int Back = 0; } L;
		L.Want = 7;
		auto Pilot = [&L](const FKite& K, FKiteInput& In, double) -> double
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			const double Dir = L.bBack ? -1.0 : 1.0;
			if (L.bOn)
			{
				if ((K.Twist - L.From) * Dir >= 1.0)
				{
					L.bOn = false;
					(L.bBack ? L.Back : L.Done)++;
					if (!L.bBack && L.Done == L.Want) { L.GainAfter = SteerGain(K); L.TwistAfter = K.Twist; L.bBack = true; }
				}
				return Dir;
			}
			// each loop begun going up: a right-hand loop from flying left, a left-hand one from flying right
			const bool bMore = L.bBack ? L.Back < L.Want : L.Done < L.Want;
			if (bMore && K.State == EState::Flying && K.Pos.Z > 9.0 && std::fabs(Az) < 25.0 && El < 45.0 && std::fabs(Heading(K, In.Hand) + 90.0 * Dir) < 35.0)
			{
				L.bOn = true;
				L.From = K.Twist;
				return Dir;
			}
			return AutoSteer(K, In.Hand, In.Wind, 0.0, false);
		};
		const FStuntRun R = FlyStunt(7.0, 0.0, 150.0, 5, Pilot);
		Print("stunt, 7 loops right, 7 back", R);
		std::printf("     after %d loops to the right: %.2f turns of twist, steering at %.0f%%; looped back %d: %.2f turns, %.0f%%\n", L.Done, L.TwistAfter, L.GainAfter * 100.0, L.Back,
			R.Kite.Twist, SteerGain(R.Kite) * 100.0);
		Check(L.Done == 7 && L.TwistAfter > 6.5 && L.TwistAfter < 8.5, "seven loops to the right wind the lines seven turns", L.TwistAfter);
		Check(L.GainAfter < 0.45, "... and twisted lines steer sluggishly (share of full steering)", L.GainAfter);
		Check(L.Back == 7 && std::fabs(R.Kite.Twist) < 1.5 && SteerGain(R.Kite) == 1.0, "looped back the other way the lines are clean again (turns left)", R.Kite.Twist);
		Check(R.Tricks.By[int(ETrick::LoopRight)] >= 6 && R.Tricks.By[int(ETrick::LoopLeft)] >= 6, "... and every loop was seen as one (right-hand loops)", (double)R.Tricks.By[int(ETrick::LoopRight)]);
		Check(R.Crashes == 0, "... all without touching the ground", (double)R.Crashes);
	}

	// flown into the ground nose first: it lies on its nose; a line held rolls it back onto its tail; a pull with a few
	// steps back and it is up again
	{
		FKite K;
		K.Def = Stunt();
		FVec Hand(0, 0, 1.4);
		const FVec W(7.0, 0, 0);
		LongLaunch(K, Hand, Hand + FVec(22, 0, 0.6), W);
		double CrashSpeed = 0.0, FlipAt = -1.0, UpAt = -1.0, CrashAt = -1.0;
		bool bNoseDown = false, bStayed = true;
		for (int I = 0; I < 40 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			In.Wind = WindAt(7.0, 0.0, K.Pos, T, 5);
			if (CrashAt < 0.0)
			{
				In.Pull = K.State == EState::Held || K.Clock < 1.0 ? 1.0 : 0.0;
				if (K.State == EState::Flying && K.Clock < 1.0) Hand.X -= 2.5 / 60.0;
				In.Steer = K.Clock < 4.0 ? SteerTo(K, Hand, 0.0) : SteerTo(K, Hand, 180.0);     // up, then straight down
			}
			else if (T < CrashAt + 4.0)
			{
				In.Steer = 0.0;     // left alone it stays down, whatever the wind
			}
			else if (!K.bNoseUp)
			{
				In.Steer = -1.0;    // the left line held
			}
			else if (K.State == EState::Grounded)
			{
				if (FlipAt < 0.0) FlipAt = T;
				if (T > FlipAt + 1.0)
				{
					In.Pull = 1.0;
					Hand.X -= 2.5 / 60.0;
				}
			}
			else
			{
				In.Steer = SteerTo(K, Hand, 0.0);
				if (UpAt < 0.0 && K.Pos.Z > 8.0) UpAt = T;
			}
			In.Hand = Hand;
			const EState Was = K.State;
			Step(K, In, 1.0 / 60.0);
			if (CrashAt < 0.0 && Was == EState::Flying && K.State == EState::Grounded)
			{
				CrashAt = T;
				CrashSpeed = K.Impact;
				bNoseDown = !K.bNoseUp;
			}
			if (CrashAt >= 0.0 && T < CrashAt + 4.0 && K.State != EState::Grounded) bStayed = false;
		}
		std::printf("     dived into the sand at %.1f m/s (t %.1f s); rolled over by %.1f s; flying again at 8 m by %.1f s\n", CrashSpeed, CrashAt, FlipAt, UpAt);
		Check(CrashAt > 0.0 && bNoseDown && CrashSpeed > 10.0, "dived into the sand it lies on its nose (m/s at the ground)", CrashSpeed);
		Check(bStayed, "... and stays there until something is done about it", bStayed ? 1.0 : 0.0);
		Check(FlipAt > 0.0 && FlipAt < CrashAt + 6.0, "one line held for a second rolls it back onto its tail (s)", FlipAt - CrashAt);
		Check(UpAt > 0.0, "... and a pull with a few steps back flies it again (s)", UpAt);
		Check(!K.bSpar, "a crash at this speed breaks nothing", K.bSpar ? 1.0 : 0.0);
	}
	// the same dive in 14 m/s is hard enough to pop a spar: it won't fly until it is wound in
	{
		FKite K;
		K.Def = Stunt();
		FVec Hand(0, 0, 1.4);
		LongLaunch(K, Hand, Hand + FVec(22, 0, 0.6), FVec(14, 0, 0));
		double CrashAt = -1.0;
		bool bFlewAgain = false;
		for (int I = 0; I < 30 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			In.Wind = WindAt(14.0, 0.0, K.Pos, T, 5);
			In.Pull = K.State == EState::Held ? 1.0 : 0.0;
			if (CrashAt < 0.0)
			{
				In.Steer = K.Clock < 3.0 ? SteerTo(K, Hand, 0.0) : SteerTo(K, Hand, 180.0);
			}
			else
			{
				// everything that would relaunch a sound kite
				In.Steer = T < CrashAt + 3.0 ? 1.0 : 0.0;
				In.Pull = T > CrashAt + 3.0 ? 1.0 : 0.0;
				if (T > CrashAt + 3.0 && T < CrashAt + 6.0) Hand.X -= 2.5 / 60.0;
				if (T > CrashAt + 8.0) In.Reel = -ReelIn * 3.0;
			}
			In.Hand = Hand;
			const EState Was = K.State;
			Step(K, In, 1.0 / 60.0);
			if (CrashAt < 0.0 && Was == EState::Flying && K.State == EState::Grounded)
			{
				CrashAt = T;
				Check(K.bSpar && K.Impact > Stunt().SparSpeed, "a dive into the sand in 14 m/s pops a spar (m/s at the ground)", K.Impact);
			}
			bFlewAgain = bFlewAgain || (CrashAt >= 0.0 && IsUp(K) && K.Pos.Z > 3.0);
		}
		Check(CrashAt > 0.0 && !bFlewAgain, "... and it doesn't fly again by pulling", bFlewAgain ? 1.0 : 0.0);
		Check(K.State == EState::Wound && !K.bSpar && K.Twist == 0.0, "... until it is wound in: the spar is set back and the lines laid straight", (double)K.State);
	}

	// gusty, tumbling air low down: sometimes it ends in the sand, and is flown again
	{
		int Crashes = 0, Launches = 0;
		double Up = 0.0;
		bool bFinite = true;
		FAir Rough;
		Rough.Turbulence = 2.2;
		for (uint32_t Seed = 1; Seed <= 6; ++Seed)
		{
			const FStuntRun R = FlyStunt(5.5, 0.6, 180.0, Seed, nullptr, Rough);
			Crashes += R.Crashes;
			Launches += R.Launches;
			Up += R.UpShare / 6.0;
			bFinite = bFinite && R.bFinite;
		}
		std::printf("     six flyers, 3 min each, 5.5 m/s with hard gusts and rough air: %d crashes, %d launches, up %.0f%% of the time\n", Crashes, Launches, Up * 100.0);
		Check(Crashes >= 1 && Crashes <= 40, "in gusty, rough air AutoSteer crashes now and then (crashes in 18 minutes)", (double)Crashes);
		Check(Up > 0.6 && bFinite, "... but is up most of the time", Up);
	}
}

/** The air: tumbling, a lee, rising air at a cliff's edge. */
static void TestAir()
{
	const FKiteDef Dia = Diamond();
	// rough air shakes it but doesn't bring it down in a good breeze
	FAir Rough;
	Rough.Turbulence = 2.0;
	const FRun Smooth = Fly(Dia, 6.0, 0.2, 180.0, 25.0), Shaken = Fly(Dia, 6.0, 0.2, 180.0, 25.0, 5, nullptr, Rough);
	Print("diamond, 6 m/s, smooth", Smooth);
	Print("diamond, 6 m/s, turbulence 2", Shaken);
	Check(Shaken.UpShare > 0.9 && Shaken.bFinite, "rough air (2 m/s rms) in a good breeze: shaken, still up", Shaken.UpShare);
	Check(Shaken.Sway() > 1.3 * Smooth.Sway() || Shaken.MaxTension > 1.3 * Smooth.MaxTension, "... the line snatches harder than in smooth air (N)", Shaken.MaxTension);
	const FRun Again = Fly(Dia, 6.0, 0.2, 180.0, 25.0, 5, nullptr, Rough);
	Check(Again.MeanHeight == Shaken.MeanHeight && Again.MaxTension == Shaken.MaxTension, "the shake is the same every time (deterministic)", Again.MeanHeight - Shaken.MeanHeight);

	// under the cliff in an offshore wind: less than half the wind, sinking and tumbling. No place for a kite
	FAir Lee;
	Lee.Factor = 0.45;
	Lee.Updraft = -0.7;
	Lee.Turbulence = 1.8;
	const FRun Open = Fly(Dia, 6.0, 0.2, 180.0, 25.0), InLee = Fly(Dia, 6.0, 0.2, 180.0, 25.0, 5, nullptr, Lee);
	Print("diamond, 6 m/s in a lee", InLee);
	Check(Open.MeanHeight > 15.0 && InLee.MeanHeight < 3.0, "in the cliff's lee (0.45 of a 6 m/s wind, sinking, rough) the diamond won't stay up (m over the hand)", InLee.MeanHeight);

	// at the cliff's edge with the wind onto it the air rises: the kite stands higher, and flies in less wind
	FAir Ridge;
	Ridge.Updraft = 1.2;
	const FRun Flat = Fly(Dia, 4.0, 0.15, 180.0, 25.0), Lifted = Fly(Dia, 4.0, 0.15, 180.0, 25.0, 5, nullptr, Ridge);
	Print("diamond, 4 m/s over flat ground", Flat);
	Print("diamond, 4 m/s in 1.2 m/s of lift", Lifted);
	Check(Lifted.MeanElev > Flat.MeanElev + 6.0, "ridge lift (1.2 m/s up) stands the kite higher (degrees more)", Lifted.MeanElev - Flat.MeanElev);
	const FRun Weak = Fly(Dia, 2.5, 0.15, 150.0, 25.0), WeakLifted = Fly(Dia, 2.5, 0.15, 150.0, 25.0, 5, nullptr, Ridge);
	Print("diamond, 2.5 m/s over flat ground", Weak);
	Print("diamond, 2.5 m/s in 1.2 m/s of lift", WeakLifted);
	Check(Weak.MeanHeight < 3.0 && WeakLifted.MeanHeight > 12.0, "... and keeps it up in 2.5 m/s, too little over flat ground (m over the hand)", WeakLifted.MeanHeight);
}

/** Launches: running upwind, and the long launch from a helper's hands. */
static void TestLaunch()
{
	const FKiteDef Dia = Diamond(), Dra = Dragon();
	// 2.5 m/s: tossed up by a flyer standing still it comes down; run upwind at 3 m/s and it climbs
	{
		auto Go = [&Dia](double Run, double& OutTop, double& OutEnd)
		{
			FKite K;
			K.Def = Dia;
			FVec Hand(0, 0, 1.4);
			Launch(K, Hand, WindAt(2.5, 0.0, Hand, 0.0, 5));
			OutTop = 0.0;
			for (int I = 0; I < 12 * 60; ++I)
			{
				Hand.X -= Run / 60.0;
				FKiteInput In;
				In.Hand = Hand;
				In.Wind = WindAt(2.5, 0.0, K.Pos, I / 60.0, 5);
				In.Reel = IsUp(K) && Elevation(K, Hand) > 25.0 ? 1.2 : 0.0;    // line out while it stands up well
				Step(K, In, 1.0 / 60.0);
				OutTop = std::max(OutTop, K.Pos.Z);
			}
			OutEnd = K.Pos.Z;
			return K.State;
		};
		double StandTop = 0.0, StandEnd = 0.0, RunTop = 0.0, RunEnd = 0.0;
		const EState Stand = Go(0.0, StandTop, StandEnd), Ran = Go(3.0, RunTop, RunEnd);
		std::printf("     2.5 m/s, 12 s after the toss: standing still %s at %.1f m; running upwind at 3 m/s %s at %.1f m\n", StateName(Stand), StandEnd, StateName(Ran), RunEnd);
		Check(Stand == EState::Grounded || StandEnd < 2.0, "2.5 m/s: tossed up by a flyer who stands still, the diamond comes down (m)", StandEnd);
		Check(Ran == EState::Flying && RunEnd > 8.0 && RunEnd > StandTop + 4.0, "... run upwind at 3 m/s and it climbs (m after 12 s)", RunEnd);
	}
	// the hand's motion is what does it: the same run downwind kills the kite's air
	{
		FKite K;
		K.Def = Dia;
		FVec Hand(0, 0, 1.4);
		Launch(K, Hand, FVec(5.0, 0, 0));
		for (int I = 0; I < 6 * 60; ++I)
		{
			Hand.X += 4.0 / 60.0;
			FKiteInput In;
			In.Hand = Hand;
			In.Wind = WindAt(5.0, 0.0, K.Pos, I / 60.0, 5);
			Step(K, In, 1.0 / 60.0);
		}
		Check(K.State == EState::Grounded || K.Pos.Z < 1.5, "run downwind at 4 m/s in a 5 m/s wind and it falls behind you (m)", K.Pos.Z);
	}

	// the long launch: held 20 m downwind and let go on the pull, it is up in a wind where a toss fails
	{
		FAir Long;
		Long.Long = 20.0;
		double TossMin = -1.0, LongMin = -1.0;
		for (double U = 1.5; U <= 5.0 && (TossMin < 0.0 || LongMin < 0.0); U += 0.25)
		{
			if (TossMin < 0.0 && Fly(Dia, U, 0.1, 120.0, 25.0).UpShare > 0.9) TossMin = U;
			if (LongMin < 0.0 && Fly(Dia, U, 0.1, 120.0, 25.0, 5, nullptr, Long).UpShare > 0.9) LongMin = U;
		}
		std::printf("     the diamond's least wind: tossed from the hand %.2f m/s, from a helper 20 m downwind %.2f m/s\n", TossMin, LongMin);
		Check(LongMin > 0.0 && LongMin <= TossMin - 0.25, "a long launch gets the diamond up in less wind than a toss (m/s less)", TossMin - LongMin);
		// the dragon: a toss in its least wind leaves it trailing its tail on the sand; from a helper it goes up
		double DraToss = -1.0, DraLong = -1.0;
		for (double U = 2.0; U <= 8.0 && (DraToss < 0.0 || DraLong < 0.0); U += 0.5)
		{
			if (DraToss < 0.0 && Fly(Dra, U, 0.1, 120.0, 22.0).UpShare > 0.9) DraToss = U;
			if (DraLong < 0.0 && Fly(Dra, U, 0.1, 120.0, 22.0, 5, nullptr, Long).UpShare > 0.9) DraLong = U;
		}
		std::printf("     the dragon's least wind: tossed %.1f m/s, from a helper %.1f m/s\n", DraToss, DraLong);
		Check(DraLong > 0.0 && DraLong <= DraToss - 0.5, "... and the dragon too (m/s less)", DraToss - DraLong);
	}
	// held, it waits: no pull, no flight; a slack line pulled comes taut first; a pull and it goes
	{
		FKite K;
		K.Def = Dia;
		const FVec Hand(0, 0, 1.4), W(5.0, 0, 0);
		LongLaunch(K, Hand, FVec(18.0, 0, 2.0), W);
		K.Line += 1.5;   // a little slack lying on the sand
		FKiteInput In;
		In.Hand = Hand;
		In.Wind = W;
		for (int I = 0; I < 120; ++I) Step(K, In, 1.0 / 60.0);
		Check(K.State == EState::Held && Length(K.Pos - FVec(18.0, 0, 2.0)) < 1e-9, "a held kite waits in the helper's hands", (double)K.State);
		In.Reel = -ReelIn;
		int Steps = 0;
		while (K.State == EState::Held && Steps < 600) { Step(K, In, 1.0 / 60.0); Steps++; }
		Check(K.State == EState::Flying && Steps > 20 && Steps < 120, "the slack is wound in first, then the pull lets it go (s)", Steps / 60.0);
		for (int I = 0; I < 20 * 60; ++I) { In.Wind = WindAt(5.0, 0.0, K.Pos, I / 60.0, 5); In.Reel = AutoReel(K, Hand, 25.0); Step(K, In, 1.0 / 60.0); }
		Check(K.State == EState::Flying && K.Pos.Z > 10.0, "... and it is up (m after 20 s)", K.Pos.Z);
	}
	// the stunt kite alone: stood on the sand nose up, it waits leaning back; the flyer pulls and steps back, and it flies
	{
		FKite K;
		K.Def = Stunt();
		FVec Hand(0, 0, 1.4);
		LayDown(K, Hand, FVec(20.0, 0, 0.05), FVec(6.0, 0, 0));
		bool bWaited = true;
		for (int I = 0; I < 20 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			In.Wind = WindAt(6.0, 0.0, K.Pos, T, 5);
			if (T > 5.0 && T < 6.5) { Hand.X -= 2.5 / 60.0; In.Pull = 1.0; }
			In.Hand = Hand;
			In.Steer = K.State == EState::Flying ? AutoSteer(K, Hand, In.Wind, 0.0, false) : 0.0;
			Step(K, In, 1.0 / 60.0);
			if (T < 5.0 && K.State != EState::Grounded) bWaited = false;
		}
		Check(bWaited, "a stunt kite stood on the sand waits for the pull", bWaited ? 1.0 : 0.0);
		Check(K.State == EState::Flying && K.Pos.Z > 5.0, "... a pull and three steps back and it flies (m)", K.Pos.Z);
	}
}

/** Tricks: each seen when it is flown, none seen in ordinary flight. */
static void TestTricks()
{
	auto Count = [](const FTricks& T, ETrick W) { return T.By[int(W)]; };
	// ordinary flight: a diamond on 20 m, a dragon, a stunt kite in plain eights
	{
		FTricks A, B;
		Fly(Diamond(), 7.0, 0.35, 240.0, 20.0, 5, nullptr, FAir(), &A);
		Fly(Dragon(), 8.0, 0.3, 240.0, 22.0, 5, nullptr, FAir(), &B);
		const FStuntRun C = FlyStunt(7.0, 0.15, 240.0, 5, [](const FKite& K, FKiteInput& In, double) { return AutoSteer(K, In.Hand, In.Wind, 0.0, false); });
		Check(A.Count == 0 && B.Count == 0 && C.Tricks.Count == 0, "ordinary flight is no trick (diamond, dragon, a stunt kite's plain eights)", double(A.Count + B.Count + C.Tricks.Count));
	}
	// high and steady: once for one stay up there
	{
		FTricks T;
		const FRun R = Fly(Diamond(), 5.0, 0.2, 200.0, 45.0, 5, nullptr, FAir(), &T);
		Check(Count(T, ETrick::High) == 1 && T.Count == 1 && T.Last == ETrick::High && T.LastScore > 0.0, "a diamond held over 25 m for 20 s: 'high', once", R.MeanHeight);
	}
	// a save: a diamond spinning in a gale reeled down until it flies again
	{
		FTricks T;
		FAir Squall;
		Squall.LullFrom = 50.0;
		Squall.LullTo = 54.0;
		Squall.LullFactor = 2.3;
		const FRun R = Fly(Diamond(), 6.5, 0.0, 100.0, 30.0, 5, nullptr, Squall, &T);
		Print("diamond, 6.5 m/s, a 4 s squall", R);
		Check(R.Spins == 1 && Count(T, ETrick::Save) == 1 && R.End == EState::Flying, "set spinning by a squall and reeled out of it: 'save' (spins)", (double)R.Spins);
	}
	// a save near the ground: the wind stops, the kite sinks to head height, the flyer hauls it back up
	{
		FTricks T;
		FAir Lull;
		Lull.LullFrom = 60.0;
		Lull.LullTo = 68.0;
		const FRun R = Fly(Diamond(), 6.0, 0.0, 90.0, 14.0, 5, [](const FKite& K, const FVec& Hand, double Want, double) { return K.Pos.Z < 3.2 && K.Clock > 20.0 ? -ReelIn : AutoReel(K, Hand, Want); },
			Lull, &T);
		Print("diamond, a lull at 60 s, hauled back", R);
		std::printf("     lowest in the last 30 s %.2f m\n", R.MinHeightLate);
		Check(Count(T, ETrick::Save) == 1 && R.End == EState::Flying, "sunk to the ground in a lull and hauled back up: 'save'", (double)Count(T, ETrick::Save));
	}
	// loops and an eight
	{
		struct FS { int Stage = 0; double From = 0.0; double Wait = 0.0; } S;
		const FStuntRun R = FlyStunt(7.0, 0.0, 60.0, 5, [&S](const FKite& K, FKiteInput& In, double T) -> double
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			// 0 wait for room, 1 loop right, 2 fly on 12 s, 3 wait for room, 4 loop left, 5 loop right at once (an eight), 6 fly on
			switch (S.Stage)
			{
			case 0: case 3:
				if (T > S.Wait && K.State == EState::Flying && K.Pos.Z > 12.0 && std::fabs(Az) < 20.0) { S.From = K.Twist; S.Stage++; }
				break;
			case 1:
				if (K.Twist - S.From >= 1.0) { S.Stage = 2; S.Wait = T + 12.0; }
				return 1.0;
			case 2:
				if (T > S.Wait) S.Stage = 3;
				break;
			case 4:
				if (S.From - K.Twist >= 1.0) { S.Stage = 5; S.From = K.Twist; }
				return -1.0;
			case 5:
				if (K.Twist - S.From >= 1.0) S.Stage = 6;
				return 1.0;
			default:
				break;
			}
			return AutoSteer(K, In.Hand, In.Wind, 0.0, false);
		});
		Print("stunt: a loop, then an eight", R);
		Check(S.Stage == 6 && R.Crashes == 0, "a loop to the right, later a loop left and right at once, flown clean", (double)S.Stage);
		Check(Count(R.Tricks, ETrick::LoopRight) == 1 && Count(R.Tricks, ETrick::LoopLeft) == 1 && Count(R.Tricks, ETrick::Eight) == 1 && R.Tricks.Count == 3,
			"... seen as 'loop_right', 'loop_left' and 'eight' (tricks)", (double)R.Tricks.Count);
		Check(R.Tricks.Last == ETrick::Eight && R.Tricks.LastScore > 10.0 && R.Tricks.Score == 50.0, "... the eight scoring most (points in all)", R.Tricks.Score);
	}
	// a low pass: across the middle of the window a metre and a half over the sand
	{
		int Stage = 0;      // 0 fly; 1 down to the sand on the left; 2 level across; 3 done
		double Low = 1e9;
		const FStuntRun R = FlyStunt(7.0, 0.0, 40.0, 5, [&](const FKite& K, FKiteInput& In, double T) -> double
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			const double Head = Heading(K, In.Hand);
			if (K.State != EState::Flying) return 0.0;
			if (Stage == 0 && T > 8.0 && Az < -30.0 && Head > 20.0 && Head < 110.0) Stage = 1;
			if (Stage == 1)
			{
				if (K.Pos.Z < 5.5) Stage = 2;
				return SteerTo(K, In.Hand, 160.0);     // down toward the sand, a little to the right
			}
			if (Stage == 2)
			{
				// level at 1.3 m: the nose a little up or down by the height
				Low = std::min(Low, K.Pos.Z);
				if (Az > 28.0) Stage = 3;
				return SteerTo(K, In.Hand, 90.0 + std::max(-40.0, std::min(40.0, (K.Pos.Z - 1.3) * 22.0 + K.Vel.Z * 4.0)));
			}
			return Stage == 3 && K.Pos.Z < 6.0 ? SteerTo(K, In.Hand, 0.0) : AutoSteer(K, In.Hand, In.Wind, 0.0, false);
		});
		Print("stunt: a low pass", R);
		std::printf("     the pass's lowest point %.2f m\n", Low);
		Check(Count(R.Tricks, ETrick::LowPass) == 1 && R.Crashes == 0 && Low < 2.0 && Low > 0.2, "across the window under 2 m without touching: 'low_pass' (lowest, m)", Low);
		Check(R.Tricks.Count == 1, "... and nothing else", (double)R.Tricks.Count);
	}
	// a dive stopped close to the sand
	{
		int Stage = 0;
		double Low = 1e9;
		const FStuntRun R = FlyStunt(7.0, 0.0, 30.0, 5, [&](const FKite& K, FKiteInput& In, double T) -> double
		{
			if (K.State != EState::Flying) return 0.0;
			if (Stage == 0)
			{
				if (T > 6.0 && K.Pos.Z > 19.0) Stage = 1;
				return SteerTo(K, In.Hand, 0.0);
			}
			if (Stage == 1)
			{
				Low = std::min(Low, K.Pos.Z);
				if (K.Pos.Z < 5.2 + 0.0 * K.Vel.Z) Stage = 2;
				return SteerTo(K, In.Hand, 180.0);
			}
			Low = std::min(Low, K.Pos.Z);
			// out of it the way it went in (turned right into the dive, so left out of it: no loop)
			return Stage == 2 ? (std::fabs(Heading(K, In.Hand)) < 20.0 ? (Stage = 3, 0.0) : -1.0) : AutoSteer(K, In.Hand, In.Wind, 0.0, false);
		});
		Print("stunt: a dive and a pull-out", R);
		std::printf("     the dive's lowest point %.2f m\n", Low);
		Check(Count(R.Tricks, ETrick::DiveStop) == 1 && R.Crashes == 0 && Low < 3.0, "a dive from the top pulled out under 3 m: 'dive_stop' (lowest, m)", Low);
	}
	// a landing: flown to the edge, the nose turned up, let settle tail first
	{
		bool bStopped = false;
		FVec Walk;
		const FStuntRun R = FlyStunt(7.0, 0.0, 40.0, 5, [&bStopped, &Walk](const FKite& K, FKiteInput& In, double T) -> double
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			if (T < 10.0) return AutoSteer(K, In.Hand, In.Wind, 0.0, false);
			// out to the right and low until the window ends and it stops; then the nose to the sky, the arms thrown forward
			// and a few steps toward it: the lines go slack and it settles on its tail
			bStopped = bStopped || (Az > 50.0 && Length(K.Vel) < 1.5);
			Walk = FVec();
			if (!bStopped || K.State != EState::Flying)
			{
				return SteerTo(K, In.Hand, El > 6.0 ? 105.0 : 85.0);
			}
			if (std::fabs(Heading(K, In.Hand)) < 25.0)
			{
				In.Pull = -1.0;
				Walk = Normal(FVec(K.Pos.X - In.Hand.X, K.Pos.Y - In.Hand.Y, 0.0)) * 3.0;
			}
			return SteerTo(K, In.Hand, 0.0);
		}, FAir(), false, &Walk);
		Print("stunt: a landing at the edge", R);
		Check(R.End == EState::Grounded && R.Kite.bNoseUp && R.Kite.bParked && R.Crashes == 0, "flown to the edge and stalled nose up it settles on its tail (m/s at the sand)", R.Kite.Impact);
		Check(Count(R.Tricks, ETrick::Landing) == 1, "... seen as 'landing'", (double)Count(R.Tricks, ETrick::Landing));
	}
	Check(std::string(TrickName(ETrick::LowPass)) == "low_pass" && std::string(TrickName(ETrick::None)) == "none", "the tricks have names", 1.0);
}

/** A caught kite got down by the line. */
static void TestSnag()
{
	const FVec Tree(10, 0, 6), Wind(6, 0, 0);
	const FVec Downwind(16, 0, 0), Upwind(4, 0, 0), Across(10, 6, 0);
	auto Tugs = [&](const FVec& From, double Gust, int Max, FSnag& S)
	{
		int N = 0;
		while (N < Max && !Tug(S, From, Wind, Gust)) N++;
		return S.bFree ? N + 1 : -1;
	};
	FSnag A = MakeSnag(Tree, 3), B = A, C = A, D = A;
	const double Hold = A.Hold;
	Check(Hold >= 0.7 && Hold <= 1.5 && MakeSnag(Tree, 3).Hold == Hold && MakeSnag(Tree, 4).Hold != Hold, "a snag holds 0.7-1.5 by its seed", Hold);
	Check(SnagSide(A, Downwind, Wind) > 0.95 && SnagSide(A, Upwind, Wind) < -0.95 && std::fabs(SnagSide(A, Across, Wind)) < 0.05 && SnagSide(A, Upwind, FVec(0.3, 0, 0)) == 0.5,
		"the right side to pull from is downwind of the branch", SnagSide(A, Downwind, Wind));
	const int Plain = Tugs(Downwind, 1.0, 40, A), InGust = Tugs(Downwind, 1.45, 40, B);
	std::printf("     a hold of %.2f: %d tugs from downwind in a steady wind, %d in a gust of 1.45\n", Hold, Plain, InGust);
	Check(Plain >= 5 && Plain <= 14, "from downwind it comes free in a handful of tugs", (double)Plain);
	Check(InGust >= 1 && InGust <= 3 && InGust < Plain, "... in a gust in two or three", (double)InGust);
	const int Wrong = Tugs(Upwind, 1.45, 40, C);
	Check(Wrong == -1 && C.Hold > Hold && C.Wrong == 40, "from upwind it never comes free: every tug drags it deeper", C.Hold);
	const int After = Tugs(Downwind, 1.0, 60, C);
	Check(After > Plain, "... and then takes longer from the right side", (double)After);
	const int Side = Tugs(Across, 1.0, 100, D);
	Check(Side > 2 * Plain, "from across the wind it takes several times as long", (double)Side);
	Check(!Tug(A, Downwind, Wind, 1.0) && A.Tugs == Plain, "a free kite takes no more tugs", (double)A.Tugs);

	// freed, it flutters down where it hung and lies there
	FKite K;
	const FVec Hand(0, 0, 1.4);
	Launch(K, Hand, Wind);
	FKiteInput In;
	In.Hand = Hand;
	In.Wind = Wind;
	for (int I = 0; I < 300; ++I) { In.Reel = 1.0; Step(K, In, 1.0 / 60.0); }
	In.Reel = 0.0;
	In.bHitObstacle = true;
	Step(K, In, 1.0 / 60.0);
	In.bHitObstacle = false;
	const FVec Hung = K.Pos;
	Free(K);
	Check(K.State == EState::Loose && !IsLost(K) && !IsUp(K), "a freed kite is loose", (double)K.State);
	double Fall = 0.0;
	while (K.State == EState::Loose && Fall < 60.0) { Step(K, In, 1.0 / 60.0); Fall += 1.0 / 60.0; }
	Check(K.State == EState::Grounded && Fall > 1.0 && Fall < 15.0 && Length(K.Pos - FVec(Hung.X, Hung.Y, 0.05)) < 12.0 && K.Pos.X > Hung.X, "... flutters down in a few seconds a little downwind, and lies there (s)", Fall);
	for (int I = 0; I < 300; ++I) Step(K, In, 1.0 / 60.0);
	Check(K.State == EState::Grounded && K.Pos.Z < 0.1, "... with nobody on its line it stays lying", K.Pos.Z);
}


static int Test()
{
	const FKiteDef Dia = Diamond(), Bx = Box();

	// calm: it won't stay up
	const FRun Calm = Fly(Dia, 1.5, 0.2, 90.0, 25.0);
	Print("diamond, calm 1.5 m/s", Calm);
	Check(Calm.FirstDown >= 0.0 && Calm.FirstDown < 30.0, "a calm (1.5 m/s): the diamond falls within 30 s", Calm.FirstDown);
	Check(Calm.UpShare < 0.5, "... and spends most of the last minute on the ground", Calm.UpShare);

	// a light breeze: the diamond flies, the heavy box kite doesn't
	const FRun Light = Fly(Dia, 3.5, 0.2, 180.0, 25.0);
	Print("diamond, light 3.5 m/s", Light);
	Check(Light.UpShare > 0.9 && Light.MeanElev > 25.0, "a light breeze (3.5 m/s): the diamond flies (up share)", Light.UpShare);
	const FRun BoxLight = Fly(Bx, 2.5, 0.2, 120.0, 25.0);
	Print("box, light 2.5 m/s", BoxLight);
	Check(BoxLight.UpShare < 0.5, "2.5 m/s is too little for the box kite", BoxLight.UpShare);

	// a good breeze, gusty: both fly high and steady, no spin
	const FRun Good = Fly(Dia, 7.0, 0.35, 240.0, 35.0);
	Print("diamond, breezy 7 m/s gusty", Good);
	Check(Good.UpShare > 0.98 && Good.Spins == 0, "7 m/s with gusts: the diamond flies all the time, no spin", Good.UpShare);
	Check(Good.MeanElev > 40.0 && Good.MeanElev < 80.0, "... at a kite's elevation (40-80 degrees)", Good.MeanElev);
	Check(Good.MeanHeight > 18.0, "... high up on 35 m of line", Good.MeanHeight);
	const FRun BoxGood = Fly(Bx, 7.0, 0.35, 240.0, 35.0);
	Print("box, breezy 7 m/s gusty", BoxGood);
	Check(BoxGood.UpShare > 0.98 && BoxGood.Spins == 0, "7 m/s with gusts: the box kite flies too", BoxGood.UpShare);
	Check(Good.MaxHum > 0.1 && Good.MaxHum <= 1.0, "a taut line hums", Good.MaxHum);

	// higher on more line: the wind is stronger up there
	const FRun Short = Fly(Dia, 5.0, 0.2, 150.0, 12.0);
	const FRun Long = Fly(Dia, 5.0, 0.2, 200.0, 45.0);
	Print("diamond, 5 m/s on 12 m", Short);
	Print("diamond, 5 m/s on 45 m", Long);
	Check(Long.MeanHeight > Short.MeanHeight + 15.0, "letting the line out takes it higher (45 m vs 12 m of line)", Long.MeanHeight - Short.MeanHeight);

	// a gale: the diamond spins; reeling in brings it out of the spin lower down (AutoReel), but holding on breaks or crashes it
	const FRun Gale = Fly(Dia, 15.0, 0.3, 120.0, 30.0, 5, LetOutFast);
	Print("diamond, gale 15 m/s held at 30 m", Gale);
	Check(Gale.FirstSpin >= 0.0 && Gale.FirstSpin < 40.0, "a gale (15 m/s): the diamond starts to spin", Gale.FirstSpin);
	Check(Gale.End != EState::Flying || Gale.Spins >= 2, "... and held out it keeps spinning, crashes or snaps", (double)Gale.Spins);
	const FRun Saved = Fly(Dia, 15.0, 0.3, 120.0, 30.0, 5);
	Print("diamond, gale 15 m/s reeled in", Saved);
	Check(Saved.End != EState::Broken, "... reeled in when it spins, the line holds", Saved.MaxTension);
	Check(Saved.Line < 15.0, "... and the flyer has reeled it in short", Saved.Line);
	const FRun BoxGale = Fly(Bx, 12.0, 0.3, 150.0, 30.0);
	Print("box, 12 m/s", BoxGale);
	Check(BoxGale.Spins == 0 && BoxGale.UpShare > 0.95, "the box kite rides 12 m/s that would spin the diamond", BoxGale.UpShare);
	const FRun Dia12 = Fly(Dia, 12.0, 0.3, 150.0, 30.0);
	Print("diamond, 12 m/s", Dia12);
	Check(Dia12.Spins >= 1, "... the diamond spins at 12 m/s on 30 m", (double)Dia12.Spins);

	// a storm on a held line snaps it
	const FRun Storm = Fly(Dia, 22.0, 0.4, 60.0, 30.0, 5, LetOutFast);
	Print("diamond, storm 22 m/s held", Storm);
	Check(Storm.End == EState::Broken || Storm.End == EState::Grounded || Storm.End == EState::Spinning, "a storm (22 m/s) on a held line: it doesn't fly calmly", (double)Storm.End);

	// a held line: no reel at all in a good breeze still flies at the launch length (3 m) without blowing up
	const FRun Held = Fly(Dia, 6.0, 0.2, 60.0, 0.0, 5, Hold);
	Print("diamond, 6 m/s, 3 m held", Held);
	Check(Held.bFinite && Held.MaxTension < Dia.BreakN, "held short in a breeze it stays finite and the line holds", Held.MaxTension);

	// every run stays finite
	bool bAll = Calm.bFinite && Light.bFinite && Good.bFinite && BoxGood.bFinite && Gale.bFinite && Saved.bFinite && Storm.bFinite;
	Check(bAll, "every run stays finite", bAll ? 1.0 : 0.0);

	// reeling all the way in winds it up
	{
		FKite K;
		const FVec Hand(0, 0, 1.4);
		Launch(K, Hand, FVec(6, 0, 0));
		for (int I = 0; I < 600; ++I)
		{
			FKiteInput In;
			In.Hand = Hand;
			In.Wind = FVec(6, 0, 0);
			In.Reel = I < 300 ? 2.0 : -ReelIn;
			Step(K, In, 1.0 / 30.0);
		}
		Check(K.State == EState::Wound, "reeled all the way in, the kite is wound on the reel", K.Line);
	}

	// a snag: a tree where it would be flying in 40 s loses it
	{
		const FVec Hand(0, 0, 1.4);
		auto Run = [&Hand](const FVec* Tree, bool& bCaught)
		{
			FKite K;
			Launch(K, Hand, FVec(6, 0, 0));
			bCaught = false;
			for (int I = 0; I < 40 * 60 && !IsLost(K); ++I)
			{
				FKiteInput In;
				In.Hand = Hand;
				In.Wind = FVec(6, 0, 0);
				In.Reel = AutoReel(K, Hand, 30.0);
				In.bHitObstacle = Tree && Length(K.Pos - *Tree) < 2.0;
				Step(K, In, 1.0 / 60.0);
				bCaught = bCaught || In.bHitObstacle;
			}
			return K;
		};
		bool bCaught = false;
		const FVec Tree = Run(nullptr, bCaught).Pos;
		const FKite K = Run(&Tree, bCaught);
		Check(bCaught && K.State == EState::Snagged, "a kite flown into a tree is caught and lost", (double)K.State);
	}

	// the line's shape: from the hand to the kite, sagging below the chord; deeper when slack
	{
		FKite K;
		const FVec Hand(0, 0, 1.4);
		K.Pos = FVec(20, 0, 21.4);
		K.Line = Length(K.Pos - Hand);
		K.Tension = 8.0;
		K.State = EState::Flying;
		FVec P[9];
		LinePoints(K, Hand, 8, P);
		const double TautSag = (Hand.Z + K.Pos.Z) * 0.5 - P[4].Z;
		Check(Length(P[0] - Hand) < 1e-9 && Length(P[8] - K.Pos) < 1e-9, "the line runs from the hand to the kite", Length(P[8] - K.Pos));
		Check(TautSag > 0.0 && TautSag < 1.0, "a taut line sags a little under its weight (m at the middle)", TautSag);
		K.Line += 6.0;
		K.Tension = 0.0;
		LinePoints(K, Hand, 8, P);
		const double SlackSag = (Hand.Z + K.Pos.Z) * 0.5 - P[4].Z;
		Check(SlackSag > TautSag + 2.0, "a slack line hangs in a deep curve", SlackSag);
	}

	// determinism
	{
		const FRun A = Fly(Dia, 7.0, 0.35, 60.0, 30.0, 9), B = Fly(Dia, 7.0, 0.35, 60.0, 30.0, 9);
		Check(A.MeanHeight == B.MeanHeight && A.MaxTension == B.MaxTension, "the same wind flies the same kite the same way", A.MeanHeight - B.MeanHeight);
	}

	// a minimum wind table: the lowest steady 10 m wind each kind flies in (for the children's choice in town)
	for (const FKiteDef* D : { &Dia, &Bx })
	{
		double Min = -1.0;
		for (double U = 1.0; U <= 8.0; U += 0.5)
		{
			const FRun R = Fly(*D, U, 0.15, 120.0, 25.0);
			if (R.UpShare > 0.9)
			{
				Min = U;
				break;
			}
		}
		std::printf("     the %s flies from %.1f m/s (10 m wind)\n", D->Shape == EShape::Box ? "box kite" : "diamond", Min);
		Check(Min >= 2.0 && Min <= (D->Shape == EShape::Box ? 5.0 : 3.5), D->Shape == EShape::Box ? "the box kite's least wind is 2-5 m/s" : "the diamond's least wind is 2-3.5 m/s", Min);
		Check(std::fabs(Min - D->FlyFrom) <= 0.5, "... as its FlyFrom says", D->FlyFrom);
	}
	// the other kinds: the delta from a toss, the dragon and the stunt kite from a helper's hands
	{
		const FKiteDef Del = Delta(), Dra = Dragon(), Stu = Stunt();
		FAir Helper;
		Helper.Long = 20.0;
		double DelMin = -1.0, DraMin = -1.0, StuMin = -1.0;
		for (double U = 1.0; U <= 8.0 && DelMin < 0.0; U += 0.5) if (Fly(Del, U, 0.15, 120.0, 25.0).UpShare > 0.9) DelMin = U;
		for (double U = 1.0; U <= 8.0 && DraMin < 0.0; U += 0.5) if (Fly(Dra, U, 0.15, 120.0, 22.0, 5, nullptr, Helper).UpShare > 0.9) DraMin = U;
		for (double U = 1.0; U <= 8.0 && StuMin < 0.0; U += 0.5)
		{
			const FStuntRun R = FlyStunt(U, 0.15, 120.0);
			if (R.UpShare > 0.9 && R.Launches <= 2) StuMin = U;
		}
		std::printf("     the delta flies from %.1f m/s, the dragon from %.1f m/s, the stunt kite from %.1f m/s (10 m wind)\n", DelMin, DraMin, StuMin);
		Check(DelMin >= 1.0 && DelMin <= 2.5 && std::fabs(DelMin - Del.FlyFrom) <= 0.5, "the delta's least wind is 1-2.5 m/s", DelMin);
		Check(DraMin >= 3.0 && DraMin <= 5.0 && std::fabs(DraMin - Dra.FlyFrom) <= 0.5, "the dragon's least wind is 3-5 m/s", DraMin);
		Check(StuMin >= 3.5 && StuMin <= 5.0 && std::fabs(StuMin - Stu.FlyFrom) <= 0.5, "the stunt kite's least wind is 3.5-5 m/s", StuMin);
	}

	TestKinds();
	TestStunt();
	TestAir();
	TestLaunch();
	TestTricks();
	TestSnag();
	// determinism again, with everything on: two lines, gusts, rough air, tricks
	{
		FAir Rough;
		Rough.Turbulence = 1.5;
		const FStuntRun A = FlyStunt(7.0, 0.35, 90.0, 9, nullptr, Rough), B = FlyStunt(7.0, 0.35, 90.0, 9, nullptr, Rough);
		Check(A.Kite.Pos.X == B.Kite.Pos.X && A.Kite.Twist == B.Kite.Twist && A.MaxTension == B.MaxTension && A.Tricks.Count == B.Tricks.Count && A.Crossings == B.Crossings,
			"the same air flies the same stunt kite the same way", A.Kite.Twist - B.Kite.Twist);
	}

	// the line's hum: silent when slack, louder and higher with a taut line in more air, never clipping
	{
		auto Render = [](float H, float Air, float& OutFreq)
		{
			LyudKiteHum::FVoice V;
			V.Init(48000.f, 3);
			static float Buf[4800];
			double Sum = 0.0;
			float Peak = 0.f;
			for (int B = 0; B < 20; ++B)
			{
				std::memset(Buf, 0, sizeof(Buf));
				V.Render(Buf, 4800, H, Air, 1.f);
				if (B >= 10)
				{
					for (float X : Buf) { Sum += double(X) * X; Peak = std::max(Peak, std::fabs(X)); }
				}
			}
			OutFreq = V.Freq;
			return std::make_pair(std::sqrt(Sum / (10.0 * 4800.0)), Peak);
		};
		float F0 = 0.f, F1 = 0.f, F2 = 0.f;
		const auto Silent = Render(0.f, 8.f, F0), Soft = Render(0.3f, 5.f, F1), Loud = Render(1.f, 12.f, F2);
		Check(Silent.first < 1e-6, "a slack line is silent", Silent.first);
		Check(Loud.first > Soft.first * 2.0 && Soft.first > 1e-3, "a taut line in more air hums louder", Loud.first / std::max(Soft.first, 1e-9));
		Check(F2 > F1 * 1.8 && F1 > 300.f, "... and higher (the aeolian tone, Hz)", F2);
		Check(Loud.second < 1.0f, "... never clipping", Loud.second);
	}

	std::printf(Failures ? "%d FAILED\n" : "all ok\n", Failures);
	return Failures ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	if (Argc > 2 && std::strcmp(Argv[1], "trace") == 0)
	{
		GTraceRun = std::atoi(Argv[2]);
	}
	if (Argc > 2 && std::strcmp(Argv[1], "fly") == 0)
	{
		// run.sh fly <speed> [box|delta|dragon|stunt] -> a trace every second (the stunt kite: from a helper's hands, by AutoSteer)
		const double U = std::atof(Argv[2]);
		FKiteDef D = Diamond();
		for (int I = 0; I < NumShapes; ++I)
		{
			if (Argc > 3 && std::strcmp(Argv[3], ShapeName(EShape(I))) == 0) D = ByShape(EShape(I));
		}
		FKite K;
		K.Def = D;
		FVec Hand(0, 0, 1.4);
		FTricks Tricks;
		if (D.Lines == 2) LongLaunch(K, Hand, Hand + FVec(D.FlyLine, 0, 0.6), FVec(U, 0, 0));
		else Launch(K, Hand, FVec(U, 0, 0));
		for (int I = 0; I < 120 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			const bool bLaunching = D.Lines == 2 && (K.State == EState::Held || (K.State == EState::Flying && K.Clock < 1.2));
			if (bLaunching && K.State != EState::Held) Hand.X -= 2.5 / 60.0;
			In.Hand = Hand;
			In.Wind = WindAt(U, 0.3, K.Pos, T, 5);
			In.Pull = bLaunching ? 1.0 : 0.0;
			In.Reel = D.Lines == 2 ? 0.0 : AutoReel(K, Hand, 30.0);
			In.Steer = AutoSteer(K, Hand, In.Wind, 0.0);
			Step(K, In, 1.0 / 60.0);
			const ETrick Got = Track(Tricks, K, In, 1.0 / 60.0);
			if (Got != ETrick::None) std::printf("        %s (%.0f points)\n", TrickName(Got), Tricks.LastScore);
			if (I % 60 == 0)
			{
				double Az, El;
				Window(K, Hand, In.Wind, Az, El);
				std::printf("%5.0f s %-9s line %5.1f  pos %6.1f %6.1f %6.1f  az %6.1f elev %5.1f  speed %5.1f  air %5.1f  aoa %5.1f  lift %6.2f  T %6.2f  twist %5.2f\n", T, StateName(K.State),
					K.Line, K.Pos.X, K.Pos.Y, K.Pos.Z, Az, El, Length(K.Vel), K.Air, K.Aoa * 57.3, K.Lift, K.Tension, K.Twist);
			}
		}
		return 0;
	}
	return Test();
}
