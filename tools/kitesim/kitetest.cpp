// Checks for the kite's lift model (Private/Kite/KiteModel.cpp) outside the engine, in the game's own wind profile and
// gust noise (Private/Weather/WindModel.cpp). See run.sh.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
};

/** The wind at the kite: the game's log profile over land and a gust noise (period ~6 s and ~25 s) of the given spread. */
static FVec WindAt(double U10, double Gusty, const FVec& At, double T, uint32_t Seed)
{
	const double G = 1.0 + Gusty * (0.7 * LyudWeather::SmoothNoise1(T / 6.0, Seed) + 0.5 * LyudWeather::SmoothNoise1(T / 25.0, Seed + 7));
	const double Speed = std::max(0.0, U10 * LyudWeather::HeightFactor(float(std::max(At.Z, 0.3)), false) * G);
	const double Veer = 0.15 * Gusty * LyudWeather::SmoothNoise1(T / 9.0, Seed + 3);
	return FVec(std::cos(Veer) * Speed, std::sin(Veer) * Speed, 0.0);
}

/** A flyer on flat ground works the reel by AutoReel toward Want metres for Seconds. */
static FRun Fly(const FKiteDef& Def, double U10, double Gusty, double Seconds, double Want, uint32_t Seed = 5,
	double (*Reel)(const FKite&, const FVec&, double, double) = nullptr)
{
	FRun Out;
	FKite K;
	K.Def = Def;
	K.Seed = Seed;
	const FVec Hand(0, 0, 1.4);
	Launch(K, Hand, WindAt(U10, Gusty, Hand, 0.0, Seed));
	const double Dt = 1.0 / 60.0;
	double ElevSum = 0.0, HSum = 0.0, Up = 0.0, Last = 0.0, UpLast = 0.0, Wait = 0.0;
	EState Prev = K.State;
	for (double T = 0.0; T < Seconds; T += Dt)
	{
		if (K.State == EState::Wound)
		{
			// wound up: the flyer tries again after a few seconds
			Wait += Dt;
			if (Wait > 4.0)
			{
				Launch(K, Hand, WindAt(U10, Gusty, Hand, T, Seed));
				Wait = 0.0;
			}
		}
		FKiteInput In;
		In.Hand = Hand;
		In.Wind = WindAt(U10, Gusty, K.Pos, T, Seed);
		In.GroundZ = 0.0;
		In.Reel = Reel ? Reel(K, Hand, Want, T) : AutoReel(K, Hand, Want);
		Step(K, In, Dt);
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
	if (Argc > 2 && std::strcmp(Argv[1], "fly") == 0)
	{
		// run.sh fly <speed> [box] -> a trace every second
		const double U = std::atof(Argv[2]);
		FKiteDef D = (Argc > 3 && std::strcmp(Argv[3], "box") == 0) ? Box() : Diamond();
		FKite K;
		K.Def = D;
		const FVec Hand(0, 0, 1.4);
		Launch(K, Hand, FVec(U, 0, 0));
		for (int I = 0; I < 120 * 60; ++I)
		{
			const double T = I / 60.0;
			FKiteInput In;
			In.Hand = Hand;
			In.Wind = WindAt(U, 0.3, K.Pos, T, 5);
			In.Reel = AutoReel(K, Hand, 30.0);
			Step(K, In, 1.0 / 60.0);
			if (I % 60 == 0)
				std::printf("%5.0f s %-9s line %5.1f  pos %6.1f %6.1f %6.1f  elev %5.1f  air %5.1f  aoa %5.1f  lift %6.2f  T %6.2f\n", T, StateName(K.State),
					K.Line, K.Pos.X, K.Pos.Y, K.Pos.Z, Elevation(K, Hand), K.Air, K.Aoa * 57.3, K.Lift, K.Tension);
		}
		return 0;
	}
	return Test();
}
