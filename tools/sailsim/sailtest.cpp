// Checks for the yacht school's sail physics (Private/Sailing/SailModel.cpp) outside the engine, in the game's own wind
// (Private/Weather/WindModel.cpp: the calendar's regime, the gust field, the height profile) and sea (WaveModel.cpp).
// See run.sh and docs/yacht_school.md "Sail physics".
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "SailModel.h"
#include "WaveModel.h"
#include "WindModel.h"

using namespace LyudSail;

static const double Deg = Pi / 180.0;
static int Failures = 0, Checks = 0;
static void Check(bool Ok, const char* What, double Value)
{
	std::printf("%s %-74s %.3f\n", Ok ? "ok  " : "FAIL", What, Value);
	Checks++;
	if (!Ok) Failures++;
}
static std::string Str(const char* Fmt, ...) __attribute__((format(printf, 1, 2)));
static std::string Str(const char* Fmt, ...)
{
	char Buf[256];
	va_list Args;
	va_start(Args, Fmt);
	std::vsnprintf(Buf, sizeof(Buf), Fmt, Args);
	va_end(Args);
	return Buf;
}

// ---- the game's wind over the water ----------------------------------------------------------------------------------

struct FWind
{
	LyudWeather::FWindState S;
	double Speed = 0.0, DirX = 1.0, DirY = 0.0;     // at sail height, blows toward
	double DriftX = 0.0, DriftY = 0.0, Clock = 0.0;

	/** A steady regime: Speed m/s at sail height from a compass bearing, with the game's gusts of this spread. */
	void Set(double InSpeed, double FromDeg, double Gustiness)
	{
		Speed = InSpeed;
		const double R = (FromDeg + 180.0) * Deg;
		DirX = -std::sin(R);
		DirY = std::cos(R);
		S.Speed = float(InSpeed);
		S.FromDeg = float(FromDeg);
		S.Gustiness = float(Gustiness);
	}
	/** The calendar's wind at an hour, brought down from 10 m to a dinghy's sail. */
	void SetCalendar(double Hours, const LyudWeather::FWindParams& P)
	{
		S = LyudWeather::Regime(Hours, P);
		Speed = S.Speed * LyudWeather::HeightFactor(2.0f, true);
		DirX = S.DirX;
		DirY = S.DirY;
	}
	void Advance(double Dt)
	{
		DriftX = std::fmod(DriftX + DirX * Speed * Dt + LyudWeather::GustPeriod, LyudWeather::GustPeriod);
		DriftY = std::fmod(DriftY + DirY * Speed * Dt + LyudWeather::GustPeriod, LyudWeather::GustPeriod);
		Clock += Dt;
	}
	void At(double X, double Y, double& Wx, double& Wy) const
	{
		const LyudWeather::FGust G = LyudWeather::GustAt(S, X, Y, DriftX, DriftY, Clock);
		const double C = std::cos(G.Veer), Sn = std::sin(G.Veer);
		Wx = (DirX * C - DirY * Sn) * Speed * G.Factor;
		Wy = (DirX * Sn + DirY * C) * Speed * G.Factor;
	}
	/** Unit vector toward where the wind comes from. */
	void Upwind(double& Ux, double& Uy) const { Ux = -DirX; Uy = -DirY; }
};

struct FWorld
{
	FWind Wind;
	double T = 0.0;
	double Hs = 0.0;                        // flat-water tests: only the added resistance of a sea of this height
	bool bWaves = false;                    // the real wave field
	LyudWeather::FWaveField Waves;
	FDepthFn Depth = nullptr;
	const double* Poly = nullptr;
	int PolyCount = 0;
	double CurrentX = 0.0, CurrentY = 0.0;

	FEnv EnvAt(double X, double Y) const
	{
		FEnv E;
		Wind.At(X, Y, E.WindX, E.WindY);
		E.CurrentX = CurrentX;
		E.CurrentY = CurrentY;
		E.Depth = Depth ? Depth(nullptr, X, Y) : 100.0;
		E.WaveHs = Hs;
		if (bWaves)
		{
			const LyudWeather::FSeaSurface Sf = Waves.SurfaceAt(X, Y, Waves.Wrap(T), float(std::max(E.Depth, 0.05)));
			E.NormalX = Sf.Normal[0];
			E.NormalY = Sf.Normal[1];
			E.NormalZ = Sf.Normal[2];
			E.WaveHs = std::min(double(Waves.Hs), double(Waves.BreakCap(float(std::max(E.Depth, 0.05)))));
		}
		return E;
	}
	void Advance(double Dt)
	{
		Wind.Advance(Dt);
		T += Dt;
	}
};

struct FSailor
{
	FBoat B;
	FHelm H;
	bool bFar = false;
	int Capsizes = 0, Groundings = 0, Turtles = 0, Slams = 0, Slaps = 0, Gybes = 0, TacksDone = 0;
	double MaxHeel = 0.0, Dist = 0.0, DownTime = 0.0, SailTime = 0.0;
	bool bFinite = true;
	int Leg = 0;
	double Finish = -1.0;
};

static void MakeSailor(FSailor& S, EBoat Class, double Skill, uint32_t Seed, double CrewMass = 0.0)
{
	Init(S.B, Class, CrewMass, Skill, Seed);
	HelmInit(S.H, Skill, Seed * 7919u + 17u);
}

static FNeighbour AsNeighbour(const FBoat& B)
{
	FNeighbour N;
	N.X = B.X;
	N.Y = B.Y;
	N.VX = B.VX;
	N.VY = B.VY;
	N.Length = Def(B.Class).Loa;
	const bool bSailing = B.State == EState::Sailing || B.State == EState::Swamped;
	N.Tack = !bSailing ? 0 : (B.Out.TrueWindAngle >= 0.0 ? 1 : -1);
	return N;
}

static void TickSailor(const FWorld& W, FSailor& S, const FNeighbour* Nb, int NbCount, double Dt)
{
	const FEnv E = W.EnvAt(S.B.X, S.B.Y);
	FHelmInput In;
	In.WindX = E.WindX;
	In.WindY = E.WindY;
	In.Depth = E.Depth;
	In.DepthFn = W.Depth;
	In.Poly = W.Poly;
	In.PolyCount = W.PolyCount;
	In.Neighbours = Nb;
	In.NeighbourCount = NbCount;
	const double X0 = S.B.X, Y0 = S.B.Y;
	if (S.bFar)
	{
		HelmFar(S.H, S.B, In, Dt);
		StepFar(S.B, S.H.CourseYaw, S.H.SpeedScale, E, Dt);
	}
	else
	{
		const FControls C = HelmUpdate(S.H, S.B, In, Dt);
		Step(S.B, C, E, Dt);
	}
	const uint32_t Ev = S.B.Events;
	if (Ev & EvCapsize) S.Capsizes++;
	if (Ev & EvGrounded) S.Groundings++;
	if (Ev & EvTurtle) S.Turtles++;
	if (Ev & EvBoomSlam) S.Slams++;
	if (Ev & EvHullSlap) S.Slaps++;
	if (Ev & EvGybe) S.Gybes++;
	if (Ev & EvTack) S.TacksDone++;
	const bool bSailing = S.B.State == EState::Sailing || S.B.State == EState::Swamped;
	if (bSailing)
	{
		S.MaxHeel = std::max(S.MaxHeel, std::fabs(S.B.Heel));
		S.SailTime += Dt;
	}
	else if (S.B.State == EState::Capsized || S.B.State == EState::Turtled || S.B.State == EState::Righting)
	{
		S.DownTime += Dt;
	}
	S.Dist += std::hypot(S.B.X - X0, S.B.Y - Y0);
	S.bFinite = S.bFinite && std::isfinite(S.B.X + S.B.Y + S.B.Yaw + S.B.VX + S.B.VY + S.B.Heel + S.B.Boom + S.B.YawRate + S.B.HeelRate);
}

// ---- a course --------------------------------------------------------------------------------------------------------

struct FCourseRun
{
	bool bFinished = false, bFinite = true;
	double Time = 0.0, Dist = 0.0, MaxHeel = 0.0, DownTime = 0.0;
	int Capsizes = 0, Tacks = 0, Gybes = 0, Turtles = 0;
	double EndX = 0.0, EndY = 0.0;
};

/** Marks of a windward-leeward (Shape 0) or a triangle (Shape 1) of this leg length, the start at the origin. */
static int CourseMarks(const FWind& W, int Shape, double Leg, double* Xy)
{
	double Ux, Uy;
	W.Upwind(Ux, Uy);
	const double Cx = -Uy, Cy = Ux;
	int N = 0;
	Xy[2 * N] = Ux * Leg; Xy[2 * N + 1] = Uy * Leg; N++;
	if (Shape == 1)
	{
		Xy[2 * N] = Ux * Leg * 0.5 + Cx * Leg * 0.7; Xy[2 * N + 1] = Uy * Leg * 0.5 + Cy * Leg * 0.7; N++;
	}
	Xy[2 * N] = 0.0; Xy[2 * N + 1] = 0.0; N++;
	return N;
}

static FCourseRun RunCourse(EBoat Class, double Skill, double WindSpeed, double FromDeg, double Gust, int Shape, double Leg,
	double Limit, uint32_t Seed, bool bFar = false, int Trace = 0, double Dt = 1.0 / 30.0)
{
	// WindSpeed is the mean wind at 10 m, as the school's go / no-go counts it; the sail feels the wind at 2 m.
	// The training water is close in and half sheltered: its chop grows with the wind but stays under half a metre.
	FWorld W;
	W.Wind.Set(WindSpeed * LyudWeather::HeightFactor(2.0f, true), FromDeg, Gust);
	W.Wind.DriftX = 37.0 * Seed;
	W.Hs = std::min(0.5, 0.008 * WindSpeed * WindSpeed);
	double Marks[8];
	const int NMarks = CourseMarks(W.Wind, Shape, Leg, Marks);
	FSailor S;
	MakeSailor(S, Class, Skill, Seed);
	double Ux, Uy;
	W.Wind.Upwind(Ux, Uy);
	PlaceSailing(S.B, 0.0, 0.0, std::atan2(Uy, Ux) + 60.0 * Deg, 0.8);
	S.bFar = bFar;
	FOrder O;
	O.Kind = EOrder::SailTo;
	O.X = Marks[0];
	O.Y = Marks[1];
	O.Radius = 8.0;
	HelmOrder(S.H, O);
	FCourseRun R;
	int NextTrace = 0;
	for (double T = 0.0; T < Limit; T += Dt)
	{
		TickSailor(W, S, nullptr, 0, Dt);
		W.Advance(Dt);
		if (S.H.bArrived)
		{
			S.Leg++;
			if (S.Leg >= NMarks)
			{
				R.bFinished = true;
				R.Time = T;
				break;
			}
			O.X = Marks[2 * S.Leg];
			O.Y = Marks[2 * S.Leg + 1];
			HelmOrder(S.H, O);
		}
		if (Trace && int(T / Trace) >= NextTrace)
		{
			NextTrace = int(T / Trace) + 1;
			const FOutputs& Out = S.B.Out;
			std::printf("%6.0f leg %d  x %7.1f y %7.1f  hdg %6.1f twa %6.1f tws %4.1f  v %5.2f lee %5.1f heel %5.1f  boom %5.1f sheet %.2f luff %.2f hike %5.2f rud %5.2f  %-12s %-9s %s%s w%.2f\n",
				T, S.Leg, S.B.X, S.B.Y, S.B.Yaw / Deg, Out.TrueWindAngle / Deg, Out.TrueWindSpeed, Out.Speed, Out.Leeway / Deg, S.B.Heel / Deg,
				S.B.Boom / Deg, S.B.Sheet, Out.MainLuff, S.B.Hike, S.B.Rudder, PointName(Out.Point), StateName(S.B.State),
				S.H.Maneuver == EManeuver::None ? "" : (S.H.Maneuver == EManeuver::Tack ? "TACK " : S.H.Maneuver == EManeuver::Gybe ? "GYBE " : S.H.Maneuver == EManeuver::Irons ? "IRONS " : "WEAR "),
				Out.Planing > 0.5 ? "plane" : "", S.B.Water);
		}
	}
	if (!R.bFinished) R.Time = Limit;
	R.bFinite = S.bFinite;
	R.Dist = S.Dist;
	R.MaxHeel = S.MaxHeel;
	R.DownTime = S.DownTime;
	R.Capsizes = S.Capsizes;
	R.Turtles = S.Turtles;
	R.Tacks = S.H.Tacks;
	R.Gybes = S.H.Gybes;
	R.EndX = S.B.X;
	R.EndY = S.B.Y;
	return R;
}

// ---- small helpers for the physics checks ----------------------------------------------------------------------------

/** A perfect crew for the assists: only the tiller is worked, to hold a true wind angle. Wind from yaw 0 (blows toward -X). */
struct FHold
{
	FBoat B;
	FEnv E;
	FControls C;
	double Twa = 0.0;
	void Start(EBoat Class, double Tws, double InTwa, double Speed, bool bAssist = true)
	{
		Init(B, Class, 0.0, 0.85, 5);
		B.Assist.bAutoSheet = B.Assist.bAutoHike = bAssist;
		E.WindX = -Tws;
		Twa = InTwa;
		PlaceSailing(B, 0.0, 0.0, -Twa, Speed);
		Step(B, C, E, 0.0);
	}
	void Steer()
	{
		const double Err = WrapAngle(-Twa - B.Yaw);
		C.Rudder = std::max(-1.0, std::min(1.0, 2.5 * Err - 1.2 * B.YawRate)) * (B.Out.Speed < -0.05 ? -1.0 : 1.0);
	}
	void Run(double Seconds, bool bSteer = true, double Dt = 1.0 / 60.0)
	{
		for (double T = 0.0; T < Seconds; T += Dt)
		{
			if (bSteer) Steer();
			Step(B, C, E, Dt);
		}
	}
};

static void PrintPolar()
{
	const double Winds[3] = { 3.0, 6.0, 10.0 };
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		std::printf("\n%s  (speed m/s | heel deg) by true wind angle; wind at sail height\n  twa ", BoatId(Class));
		for (double W : Winds) std::printf("   %4.0f m/s     ", W);
		std::printf("\n");
		for (int A = 30; A <= 180; A += 10)
		{
			std::printf("  %3d ", A);
			for (double W : Winds) std::printf("  %5.2f | %4.1f  ", PolarSpeed(Class, A * Deg, W), PolarHeel(Class, A * Deg, W) / Deg);
			std::printf("\n");
		}
		std::printf("  beat ");
		for (double W : Winds) std::printf(" %3.0f deg vmg %4.2f ", BeatAngle(Class, W) / Deg, PolarSpeed(Class, BeatAngle(Class, W), W) * std::cos(BeatAngle(Class, W)));
		std::printf("\n  run  ");
		for (double W : Winds) std::printf(" %3.0f deg vmg %4.2f ", RunAngle(Class, W) / Deg, -PolarSpeed(Class, RunAngle(Class, W), W) * std::cos(RunAngle(Class, W)));
		std::printf("\n");
	}
}

// ---- the checks ------------------------------------------------------------------------------------------------------

static void TestPolar()
{
	std::printf("-- polar: no-go zone, shape, speeds\n");
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		const char* Id = BoatId(Class);
		const double S50 = PolarSpeed(Class, 50 * Deg, 6.0);
		Check(PolarSpeed(Class, 20 * Deg, 6.0) < 0.15 * S50 && PolarSpeed(Class, 30 * Deg, 6.0) < 0.5 * S50, Str("%s: no way made inside 30 degrees of a 6 m/s wind (share of the 50-degree speed)", Id).c_str(), PolarSpeed(Class, 30 * Deg, 6.0) / S50);
		const double Beat = BeatAngle(Class, 6.0) / Deg;
		Check(Beat >= 38.0 && Beat <= 55.0, Str("%s: best beating angle, degrees", Id).c_str(), Beat);
		double Best = 0.0;
		int BestA = 0;
		for (int A = 30; A <= 180; A += 5)
		{
			const double V = PolarSpeed(Class, A * Deg, 6.0);
			if (V > Best) { Best = V; BestA = A; }
		}
		Check(BestA >= 80 && BestA <= 140, Str("%s: fastest on a beam or broad reach (angle of the best speed at 6 m/s)", Id).c_str(), BestA);
		Check(PolarSpeed(Class, Pi, 6.0) < 0.93 * Best, Str("%s: the dead run is slower than the reach (share)", Id).c_str(), PolarSpeed(Class, Pi, 6.0) / Best);
		Check(S50 > 0.55 * Best, Str("%s: close-hauled is a fair share of the reach", Id).c_str(), S50 / Best);
		// more wind, more speed
		bool bGrows = true;
		for (double W = 1.0; W < 9.5; W += 1.0) bGrows = bGrows && PolarSpeed(Class, 100 * Deg, W + 1.0) > PolarSpeed(Class, 100 * Deg, W) - 0.02;
		Check(bGrows, Str("%s: reaching speed grows with the wind up to 10 m/s", Id).c_str(), PolarSpeed(Class, 100 * Deg, 10.0));
	}
	double OptBest = 0.0, LuchBest6 = 0.0, LuchBest10 = 0.0, CadetBest10 = 0.0, OptBest8 = 0.0, LuchBest3 = 0.0;
	for (int A = 40; A <= 180; A += 5)
	{
		OptBest = std::max(OptBest, PolarSpeed(EBoat::Optimist, A * Deg, 5.0));
		OptBest8 = std::max(OptBest8, PolarSpeed(EBoat::Optimist, A * Deg, 8.0));
		LuchBest3 = std::max(LuchBest3, PolarSpeed(EBoat::Luch, A * Deg, 3.0));
		LuchBest6 = std::max(LuchBest6, PolarSpeed(EBoat::Luch, A * Deg, 5.5));
		LuchBest10 = std::max(LuchBest10, PolarSpeed(EBoat::Luch, A * Deg, 10.0));
		CadetBest10 = std::max(CadetBest10, PolarSpeed(EBoat::Cadet, A * Deg, 10.0));
	}
	Check(OptBest > 1.6 && OptBest < 2.6, "optimist: about 2 m/s in 5 m/s of wind", OptBest);
	Check(OptBest8 < 3.6, "optimist: a pram does not fly even in 8 m/s", OptBest8);
	Check(LuchBest3 > 1.5 && LuchBest3 < 2.9, "luch: light air (3 m/s)", LuchBest3);
	Check(LuchBest6 > 2.5 && LuchBest6 < 3.7, "luch: 2.5-3.5 m/s at displacement speed (5.5 m/s of wind)", LuchBest6);
	Check(LuchBest10 > 5.0 && LuchBest10 < 7.5, "luch: planes at 5-7 m/s in 10 m/s of wind", LuchBest10);
	Check(CadetBest10 > 3.6 && CadetBest10 < 6.5, "cadet: planes in 10 m/s", CadetBest10);
	const double UpL = PolarSpeed(EBoat::Luch, BeatAngle(EBoat::Luch, 6.0), 6.0);
	Check(UpL > 1.8 && UpL < 3.2, "luch: close-hauled speed in 6 m/s", UpL);
}

static void TestTackIronsGybe()
{
	std::printf("-- tacking, irons, gybing\n");
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		const char* Id = BoatId(Class);
		FHold Hd;
		const double Up = BeatAngle(Class, 6.0) + 3.0 * Deg;
		Hd.Start(Class, 6.0, Up, 2.0);
		Hd.Run(40.0);
		const double V0 = Hd.B.Out.Speed;
		const double Lee = std::fabs(Hd.B.Out.Leeway) / Deg;
		Check(Lee > 1.0 && Lee < 9.0, Str("%s: leeway close-hauled with the board down, degrees", Id).c_str(), Lee);
		// tack: hold the other close-hauled heading
		Hd.Twa = -Up;
		double Tdone = -1.0, Vmin = V0, Tback = -1.0;
		bool bTackEvent = false;
		const double Dt = 1.0 / 60.0;
		for (double T = 0.0; T < 40.0; T += Dt)
		{
			Hd.Steer();
			Step(Hd.B, Hd.C, Hd.E, Dt);
			if (Hd.B.Events & EvTack) { bTackEvent = true; if (Tdone < 0.0) Tdone = T; }
			Vmin = std::min(Vmin, Hd.B.Out.Speed);
			if (Tdone >= 0.0 && Tback < 0.0 && Hd.B.Out.Speed > 0.9 * V0) Tback = T;
		}
		Check(bTackEvent && Tdone > 1.5 && Tdone < 9.0, Str("%s: a tack takes a few seconds (to the sail filling on the new side)", Id).c_str(), Tdone);
		Check(Vmin > 0.15 * V0 && Vmin < 0.85 * V0, Str("%s: way lost in the tack (lowest speed as a share)", Id).c_str(), Vmin / V0);
		Check(Tback > 0.0 && Tback < 30.0, Str("%s: back up to speed after the tack, s", Id).c_str(), Tback);
		Check(Hd.B.Out.TrueWindAngle < 0.0 && Hd.B.Boom > 0.0, Str("%s: on the other tack, boom the other side", Id).c_str(), Hd.B.Out.TrueWindAngle / Deg);

		// irons: head to wind, stopped, sheet pinned in, tiller amidships: stuck
		FHold Ir;
		Ir.Start(Class, 6.0, 4.0 * Deg, 0.0, false);
		Ir.C.Sheet = 0.0;
		Ir.C.Hike = 0.0;
		Ir.Run(20.0, false);
		Check(std::fabs(Ir.B.Out.TrueWindAngle) < 35.0 * Deg && Ir.B.Out.Speed < 0.3, Str("%s: in irons with the sheet in: stuck head to wind (twa)", Id).c_str(), Ir.B.Out.TrueWindAngle / Deg);
		Check(Advice(Ir.B, Ir.E) == EAdvice::InIrons, Str("%s: the tutor says: in irons", Id).c_str(), double(Advice(Ir.B, Ir.E)));
		Check(Ir.B.Out.Speed < -0.05, Str("%s: and she gathers sternway", Id).c_str(), Ir.B.Out.Speed);
		// escape: sheet out, tiller hard over; going astern the bow swings the way opposite to the tiller's ahead sense
		Ir.C.Sheet = 1.0;
		Ir.C.Rudder = 1.0;
		const double Yaw0 = Ir.B.Yaw;
		double Tout = -1.0;
		for (double T = 0.0; T < 40.0 && Tout < 0.0; T += Dt)
		{
			Step(Ir.B, Ir.C, Ir.E, Dt);
			if (std::fabs(Ir.B.Out.TrueWindAngle) > 92.0 * Deg || Ir.B.Out.Speed > 0.3) Tout = T;
		}
		Check(Tout > 0.0 && WrapAngle(Ir.B.Yaw - Yaw0) < 0.0, Str("%s: backs out of irons with the tiller reversed, s", Id).c_str(), Tout);
		Ir.B.Assist.bAutoSheet = Ir.B.Assist.bAutoHike = true;
		Ir.Twa = Ir.B.Out.TrueWindAngle > 0.0 ? 80.0 * Deg : -80.0 * Deg;
		Ir.Run(40.0);
		Check(Ir.B.Out.Speed > 1.0, Str("%s: and sails away", Id).c_str(), Ir.B.Out.Speed);

		// gybe: from a broad reach bear away through dead downwind
		FHold Gy;
		Gy.Start(Class, 6.0, 150.0 * Deg, 2.0);
		Gy.Run(20.0);
		const double BoomBefore = Gy.B.Boom;
		Gy.Twa = -150.0 * Deg;
		bool bGybe = false, bSlam = false, bLee = false;
		double Slam = 0.0;
		for (double T = 0.0; T < 25.0; T += Dt)
		{
			Gy.Steer();
			Step(Gy.B, Gy.C, Gy.E, Dt);
			bGybe = bGybe || (Gy.B.Events & EvGybe);
			bSlam = bSlam || (Gy.B.Events & EvBoomSlam);
			bLee = bLee || Gy.B.Out.bByTheLee;
			Slam = std::max(Slam, Gy.B.Out.BoomSlamSpeed);
		}
		Check(bGybe && bSlam && BoomBefore * Gy.B.Boom < 0.0, Str("%s: gybe: the boom comes across with a slam (rad/s)", Id).c_str(), Slam);
		Check(bLee, Str("%s: by the lee was flagged before it", Id).c_str(), 1.0);
		Check(Gy.B.State == EState::Sailing, Str("%s: a gybe in 6 m/s with a good crew stays upright (max heel)", Id).c_str(), Gy.B.Heel / Deg);
	}
}

static void TestHeelCapsize()
{
	std::printf("-- heel, capsize, righting, swamping\n");
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		const char* Id = BoatId(Class);
		const double Blow = Class == EBoat::Optimist ? 9.0 : 10.0;
		// overpowered on a beam reach, sheet pinned, crew sitting in the middle
		FHold Hd;
		Hd.Start(Class, Blow, 80.0 * Deg, 2.0, false);
		Hd.C.Sheet = 0.05;
		Hd.C.Hike = 0.0;
		double Tcap = -1.0;
		const double Dt = 1.0 / 60.0;
		for (double T = 0.0; T < 30.0 && Tcap < 0.0; T += Dt)
		{
			Hd.Steer();
			Step(Hd.B, Hd.C, Hd.E, Dt);
			if (Hd.B.Events & EvCapsize) Tcap = T;
		}
		Check(Tcap > 0.0, Str("%s: overpowered, sheet pinned, nobody hiking: over she goes, s", Id).c_str(), Tcap);
		Check(Advice(Hd.B, Hd.E) == EAdvice::RightBoat, Str("%s: the tutor says: right her", Id).c_str(), double(Advice(Hd.B, Hd.E)));
		// the same with the sheet eased and the crew out: stays up
		FHold Ok;
		Ok.Start(Class, Blow, 80.0 * Deg, 2.0, true);
		double MaxHeel = 0.0;
		bool bUp = true;
		for (double T = 0.0; T < 90.0; T += Dt)
		{
			Ok.E.WindX = -Blow * (1.0 + 0.25 * std::sin(T * 0.9) * std::sin(T * 0.23));
			Ok.Steer();
			Step(Ok.B, Ok.C, Ok.E, Dt);
			bUp = bUp && Ok.B.State == EState::Sailing;
			if (T > 10.0) MaxHeel = std::max(MaxHeel, std::fabs(Ok.B.Heel));
		}
		Check(bUp && MaxHeel < 40.0 * Deg, Str("%s: handled (sheet eased in the gusts, crew hiking) she stays up; max heel", Id).c_str(), MaxHeel / Deg);
		// the no-capsize assist with everything done wrong
		FHold Nc;
		Nc.Start(Class, Blow + 2.0, 80.0 * Deg, 2.0, false);
		Nc.B.Assist.bNoCapsize = true;
		Nc.C.Sheet = 0.05;
		Nc.C.Hike = 0.0;
		Nc.Run(60.0);
		Check(Nc.B.State == EState::Sailing && Nc.B.Water == 0.0, Str("%s: no-capsize assist: pinned in a gale, upright and dry (heel)", Id).c_str(), Nc.B.Heel / Deg);

		// righting: a good and a green crew
		double Times[2] = { 0.0, 0.0 };
		for (int K = 0; K < 2; K++)
		{
			FBoat B = Hd.B;
			B.CrewSkill = K == 0 ? 0.9 : 0.15;
			FControls C;
			FEnv E;
			E.WindX = -6.0;
			for (double T = 0.0; T < 3.0; T += Dt) Step(B, C, E, Dt);
			Check(K == 1 || (B.State == EState::Capsized && std::fabs(B.Heel) > 80.0 * Deg), Str("%s: lies on her side", Id).c_str(), B.Heel / Deg);
			C.bRight = true;
			double T = 0.0;
			for (; T < 120.0 && !(B.Events & EvRighted); T += Dt) Step(B, C, E, Dt);
			Times[K] = T;
			if (K == 0)
			{
				Check(B.State == EState::Sailing || B.State == EState::Swamped, Str("%s: comes up (%s), water aboard", Id, StateName(B.State)).c_str(), B.Water);
				if (B.State == EState::Swamped)
				{
					Check(Advice(B, E) == EAdvice::Bail, Str("%s: the tutor says: bail", Id).c_str(), double(Advice(B, E)));
					C.bRight = false;
					C.bBail = true;
					C.Sheet = 1.0;
					double Tb = 0.0;
					for (; Tb < 200.0 && B.State == EState::Swamped; Tb += Dt) Step(B, C, E, Dt);
					Check(B.State == EState::Sailing && Tb > 5.0 && Tb < 90.0, Str("%s: bailed dry enough to sail, s", Id).c_str(), Tb);
				}
			}
		}
		Check(Times[0] > 4.0 && Times[0] < 30.0, Str("%s: a good crew rights her, s", Id).c_str(), Times[0]);
		Check(Times[1] > Times[0] * 1.3 && Times[1] < 60.0, Str("%s: a green crew takes longer, s", Id).c_str(), Times[1]);
		// turtled takes about twice as long
		{
			FBoat B = Hd.B;
			B.CrewSkill = 0.9;
			B.State = EState::Turtled;
			FControls C;
			FEnv E;
			E.WindX = -6.0;
			for (double T = 0.0; T < 4.0; T += Dt) Step(B, C, E, Dt);
			Check(std::fabs(B.Heel) > 150.0 * Deg, Str("%s: turtled: upside down", Id).c_str(), B.Heel / Deg);
			C.bRight = true;
			double T = 0.0;
			for (; T < 120.0 && !(B.Events & EvRighted); T += Dt) Step(B, C, E, Dt);
			Check(T > 1.6 * Times[0] && T < 70.0, Str("%s: righting from turtle, s", Id).c_str(), T);
		}
	}
	// a swamped boat is slow
	{
		FHold A, B;
		A.Start(EBoat::Optimist, 5.0, 90.0 * Deg, 1.5);
		B.Start(EBoat::Optimist, 5.0, 90.0 * Deg, 1.5);
		B.B.Water = 0.9;
		A.Run(30.0);
		B.Run(30.0);
		Check(B.B.State == EState::Swamped && B.B.Out.Speed < 0.8 * A.B.Out.Speed, "optimist: full of water she is sluggish (share of dry speed)", B.B.Out.Speed / A.B.Out.Speed);
	}
	// turtles do happen to a boat left on her side in a blow, and not in the shallows
	{
		int Turtled = 0, Shallow = 0;
		for (uint32_t Seed = 1; Seed <= 40; Seed++)
		{
			for (int K = 0; K < 2; K++)
			{
				FBoat B;
				Init(B, EBoat::Cadet, 0.0, 0.3, Seed);
				Place(B, 0, 0, 0, EState::Capsized);
				B.Heel = 1.5;
				FControls C;
				FEnv E;
				E.WindX = -10.0;
				E.Depth = K == 0 ? 20.0 : 2.0;
				for (double T = 0.0; T < 60.0; T += 1.0 / 30.0) Step(B, C, E, 1.0 / 30.0);
				if (B.State == EState::Turtled) (K == 0 ? Turtled : Shallow)++;
			}
		}
		Check(Turtled >= 5 && Turtled <= 38, "cadet left on her side for a minute in 10 m/s: sometimes turns turtle (of 40)", Turtled);
		Check(Shallow == 0, "never in 2 m of water (the mast finds the bottom)", Shallow);
	}
}

static void TestWater()
{
	std::printf("-- board, grounding, rudder, calm, current, planing, waves\n");
	const double Dt = 1.0 / 60.0;
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		const char* Id = BoatId(Class);
		const FBoatDef& D = Def(Class);
		// leeway and progress upwind with the board up
		FHold Dn, Up;
		const double A = BeatAngle(Class, 6.0) + 5.0 * Deg;
		Dn.Start(Class, 6.0, A, 2.0);
		Up.Start(Class, 6.0, A, 2.0);
		Up.C.Board = 0.0;
		Dn.Run(40.0);
		Up.Run(40.0);
		const double LeeDn = std::fabs(Dn.B.Out.Leeway) / Deg, LeeUp = std::fabs(Up.B.Out.Leeway) / Deg;
		Check(LeeUp > 10.0 && LeeUp > 2.5 * LeeDn, Str("%s: leeway with the board up, degrees (down: %.1f)", Id, LeeDn).c_str(), LeeUp);
		Check(Vmg(Up.B, 1000.0, 0.0) < 0.75 * Vmg(Dn.B, 1000.0, 0.0), Str("%s: way made upwind with the board up, share", Id).c_str(), Vmg(Up.B, 1000.0, 0.0) / Vmg(Dn.B, 1000.0, 0.0));
		Check(Advice(Up.B, Up.E) == EAdvice::BoardDown, Str("%s: the tutor says: board down", Id).c_str(), double(Advice(Up.B, Up.E)));

		// grounding on the board, floating off, hard aground
		FHold G;
		G.Start(Class, 5.0, 90.0 * Deg, 2.0);
		G.Run(10.0);
		G.E.Depth = D.HullDraft + 0.5 * D.BoardLen;
		Check(Advice(G.B, G.E) == EAdvice::BoardUp, Str("%s: the tutor says: board up, shallow", Id).c_str(), double(Advice(G.B, G.E)));
		Step(G.B, G.C, G.E, Dt);
		const bool bEv = (G.B.Events & EvGrounded) != 0;
		G.Run(3.0);
		Check(bEv && G.B.State == EState::Aground && G.B.Out.Sog < 0.1, Str("%s: the board finds the bottom: aground, stopped", Id).c_str(), G.B.Out.Sog);
		Check(Advice(G.B, G.E) == EAdvice::FloatOff, Str("%s: the tutor says: float her off", Id).c_str(), double(Advice(G.B, G.E)));
		G.C.Board = 0.4;
		bool bOff = false;
		for (double T = 0.0; T < 20.0; T += Dt)
		{
			G.Steer();
			Step(G.B, G.C, G.E, Dt);
			bOff = bOff || (G.B.Events & EvFloatedOff);
		}
		Check(bOff && G.B.State == EState::Sailing && G.B.Out.Speed > 0.8, Str("%s: board up, she floats off and sails", Id).c_str(), G.B.Out.Speed);
		G.E.Depth = 0.05;
		G.Run(3.0);
		G.C.Board = 0.0;
		G.Run(3.0);
		Check(G.B.State == EState::Aground, Str("%s: on the sand she stays", Id).c_str(), G.E.Depth);

		// no steerage without way
		FHold R;
		R.Start(Class, 0.0, 0.0, 0.0, false);
		R.C.Rudder = 1.0;
		R.Run(10.0, false);
		Check(std::fabs(R.B.Yaw) < 2.0 * Deg, Str("%s: stopped in a calm the tiller does nothing (degrees turned)", Id).c_str(), R.B.Yaw / Deg);
		FHold R2;
		R2.Start(Class, 0.0, 0.0, 2.0, false);
		R2.C.Rudder = 1.0;
		double Turned = 0.0, Prev = R2.B.Yaw;
		for (double T = 0.0; T < 4.0; T += Dt)
		{
			Step(R2.B, R2.C, R2.E, Dt);
			Turned += WrapAngle(R2.B.Yaw - Prev);
			Prev = R2.B.Yaw;
		}
		Check(Turned > 45.0 * Deg, Str("%s: coasting at 2 m/s she answers it (degrees turned to starboard in 4 s)", Id).c_str(), Turned / Deg);

		// a calm with a current: drifts, sails hang
		FHold Cm;
		Cm.Start(Class, 0.0, 0.0, 0.0, false);
		Cm.E.WindX = -0.3;
		Cm.E.CurrentX = 0.2;
		Cm.E.CurrentY = 0.3;
		Cm.C.Sheet = 0.5;
		Cm.Run(120.0, false);
		const double DriftErr = std::hypot(Cm.B.VX - 0.2, Cm.B.VY - 0.3);
		Check(DriftErr < 0.12 && Cm.B.Out.MainLuff > 0.5 && std::fabs(Cm.B.Out.Speed) < 0.15, Str("%s: calm: drifts with the current, sails hang (error of drift, m/s)", Id).c_str(), DriftErr);
	}
	// weather helm grows with heel
	{
		FHold A, B2;
		A.Start(EBoat::Luch, 3.0, 60.0 * Deg, 1.5);
		A.Run(30.0);
		B2.Start(EBoat::Luch, 7.0, 60.0 * Deg, 2.5);
		B2.Run(30.0);
		Check(std::fabs(B2.B.Heel) > std::fabs(A.B.Heel) + 5.0 * Deg && B2.B.Rudder * B2.B.Heel > 0.0 && std::fabs(B2.B.Rudder) > std::fabs(A.B.Rudder) + 0.03,
			Str("luch: heeled %.0f degrees she pulls to windward (tiller held against it; share of full rudder, upright %.2f)", std::fabs(B2.B.Heel) / Deg, std::fabs(A.B.Rudder)).c_str(), std::fabs(B2.B.Rudder));
		Check(std::fabs(B2.B.Out.HelmLoad) > std::fabs(A.B.Out.HelmLoad) + 0.02, "luch: and the tiller pulls in the hand (helm load)", B2.B.Out.HelmLoad);
		// let the tiller go: she rounds up
		B2.C.Rudder = 0.0;
		const double Twa0 = std::fabs(B2.B.Out.TrueWindAngle);
		B2.Run(5.0, false);
		Check(std::fabs(B2.B.Out.TrueWindAngle) < Twa0 - 10.0 * Deg, "luch: tiller let go, she luffs up (twa after 5 s)", B2.B.Out.TrueWindAngle / Deg);
	}
	// planing events
	{
		FHold P;
		P.Start(EBoat::Luch, 4.0, 110.0 * Deg, 2.0);
		bool bStart = false, bEnd = false;
		for (double T = 0.0; T < 120.0; T += Dt)
		{
			P.E.WindX = -(T < 20.0 ? 4.0 : (T < 70.0 ? 10.0 : 4.0));
			P.Steer();
			Step(P.B, P.C, P.E, Dt);
			bStart = bStart || (P.B.Events & EvPlaningStart);
			bEnd = bEnd || (bStart && (P.B.Events & EvPlaningEnd));
		}
		Check(bStart && bEnd, "luch: a 10 m/s gust lifts her onto the plane, and she comes off it after", double(bStart) + double(bEnd));
	}
	// waves: slower into a head sea; the bow slaps
	{
		FHold Fl, Sea;
		const double A = BeatAngle(EBoat::Optimist, 6.0) + 3.0 * Deg;
		Fl.Start(EBoat::Optimist, 6.0, A, 1.5);
		Sea.Start(EBoat::Optimist, 6.0, A, 1.5);
		Sea.E.WaveHs = 0.6;
		Fl.Run(40.0);
		int Slaps = 0;
		double MaxPitch = 0.0;
		for (double T = 0.0; T < 40.0; T += Dt)
		{
			// a 9 m swell from upwind, met at the boat's own speed
			const double Phase = 2.0 * Pi / 9.0 * (Sea.B.X + 3.7 * T);
			const double Slope = 0.3 * 2.0 * Pi / 9.0 * std::cos(Phase);
			Sea.E.NormalX = -Slope;
			Sea.E.NormalZ = 1.0;
			Sea.Steer();
			Step(Sea.B, Sea.C, Sea.E, Dt);
			if (Sea.B.Events & EvHullSlap) Slaps++;
			MaxPitch = std::max(MaxPitch, std::fabs(Sea.B.Out.Pitch));
		}
		Check(Sea.B.Out.Speed < 0.93 * Fl.B.Out.Speed, "optimist: slower punching into a 0.6 m sea (share of flat-water speed)", Sea.B.Out.Speed / Fl.B.Out.Speed);
		Check(Slaps >= 5 && Slaps <= 45, "optimist: the bow slaps on the waves (events in 40 s)", Slaps);
		Check(MaxPitch > 3.0 * Deg && MaxPitch < 20.0 * Deg, "optimist: pitches with the waves, degrees", MaxPitch / Deg);
	}
	// a foul current sets her back
	{
		FHold A;
		A.Start(EBoat::Cadet, 5.0, 90.0 * Deg, 2.0);
		A.E.CurrentY = 0.5;
		A.Run(30.0);
		Check(std::fabs(A.B.Out.Sog - A.B.Out.Speed) > 0.2, "cadet: a current shows in the speed over the ground", A.B.Out.Sog - A.B.Out.Speed);
	}
}

static void TestPlayer()
{
	std::printf("-- the tutor and the assists\n");
	const double Dt = 1.0 / 60.0;
	// sheet far too loose on a reach: sheet in
	{
		FHold P;
		P.Start(EBoat::Luch, 5.0, 70.0 * Deg, 2.0, false);
		P.B.Assist.bAutoHike = true;
		P.C.Sheet = 1.0;
		P.Run(15.0);
		Check(Advice(P.B, P.E) == EAdvice::SheetIn && P.B.Out.MainLuff > 0.5 && P.B.Out.TellWindward == ETell::Lifting, "luffing on a reach: sheet in (luff amount)", P.B.Out.MainLuff);
		const double Slow = P.B.Out.Speed;
		P.C.Sheet = IdealSheet(P.B);
		for (double T = 0.0; T < 25.0; T += Dt)
		{
			P.C.Sheet = IdealSheet(P.B);
			P.Steer();
			Step(P.B, P.C, P.E, Dt);
		}
		Check(Advice(P.B, P.E) == EAdvice::None && P.B.Out.Speed > Slow + 1.0 && std::fabs(P.B.Out.TrimError) < 0.2 && P.B.Out.TellLeeward == ETell::Flowing, "trimmed to IdealSheet: no advice, telltales flow, fast", P.B.Out.Speed);
		const double Fast = P.B.Out.Speed;
		// sheeted hard in on a reach: stalled, ease
		P.C.Sheet = 0.0;
		P.Run(15.0);
		Check(Advice(P.B, P.E) == EAdvice::Ease && P.B.Out.TellLeeward == ETell::Stalled && P.B.Out.Speed < Fast, "over-sheeted on a reach: stalled, ease (trim error)", P.B.Out.TrimError);
	}
	// pinching: sheet in, pointing too high
	{
		FHold P;
		P.Start(EBoat::Luch, 5.0, 24.0 * Deg, 2.0, false);
		P.B.Assist.bAutoHike = true;
		P.C.Sheet = 0.0;
		P.Run(6.0);
		const EAdvice A = Advice(P.B, P.E);
		Check(A == EAdvice::BearAway || A == EAdvice::InIrons, "pinching with the sheet in: bear away", double(A));
	}
	// heeling with the crew inboard: hike
	{
		FHold P;
		P.Start(EBoat::Luch, 7.0, 60.0 * Deg, 2.5, false);
		P.B.Assist.bAutoSheet = true;
		P.C.Hike = 0.0;
		P.Run(15.0);
		Check(Advice(P.B, P.E) == EAdvice::Hike, "heeling with the crew inboard: hike (heel)", P.B.Heel / Deg);
	}
	// by the lee
	{
		FHold P;
		P.Start(EBoat::Luch, 5.0, 170.0 * Deg, 2.0);
		P.Run(15.0);
		P.Twa = -168.0 * Deg;
		bool bSaid = false;
		for (double T = 0.0; T < 6.0 && !(P.B.Events & EvGybe); T += Dt)
		{
			P.Steer();
			Step(P.B, P.C, P.E, Dt);
			bSaid = bSaid || Advice(P.B, P.E) == EAdvice::ByTheLee;
		}
		Check(bSaid, "bearing away past dead downwind: by the lee", 1.0);
	}
	// easy mode: only the tiller, a gusty 8 m/s, half an hour: never over
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const EBoat Class = EBoat(Ci);
		FWorld W;
		W.Wind.Set(Class == EBoat::Optimist ? 7.0 : 8.5, 250.0, 0.3);
		FBoat B;
		Init(B, Class, 0.0, 0.5, 11);
		B.Assist.bAutoSheet = B.Assist.bAutoHike = true;
		double Ux, Uy;
		W.Wind.Upwind(Ux, Uy);
		const double From = std::atan2(Uy, Ux);
		PlaceSailing(B, 0, 0, From + 90 * Deg, 1.0);
		FControls C;
		int Caps = 0;
		double MaxHeel = 0.0, Dist = 0.0;
		const double Angles[6] = { 90.0, 50.0, 130.0, -130.0, -50.0, 175.0 };
		for (double T = 0.0; T < 1800.0; T += Dt)
		{
			const FEnv E = W.EnvAt(B.X, B.Y);
			const double Want = From - Angles[int(T / 45.0) % 6] * Deg;
			C.Rudder = std::max(-1.0, std::min(1.0, 2.0 * WrapAngle(Want - B.Yaw) - 1.0 * B.YawRate)) * (B.Out.Speed < -0.05 ? -1.0 : 1.0);
			C.bRight = true;
			Step(B, C, E, Dt);
			W.Advance(Dt);
			if (B.Events & EvCapsize) Caps++;
			MaxHeel = std::max(MaxHeel, std::fabs(B.Heel));
			Dist += B.Out.Sog * Dt;
		}
		Check(Caps == 0 && Dist > 1800.0, Str("%s: easy mode (auto sheet and hike), gusty blow, 30 min of every point of sail: capsizes (max heel %.0f)", BoatId(Class), MaxHeel / Deg).c_str(), Caps);
	}
}

static void TestCourses(bool bQuick)
{
	std::printf("-- the helmsman round a course: 8 wind directions x 3, 6, 9 m/s x 3 classes x windward-leeward and triangle\n");
	const double Winds[3] = { 3.0, 6.0, 9.0 };
	double GoodTime[BoatCount][3] = {}, GreenTime[BoatCount][3] = {};
	int GoodCaps[BoatCount][3] = {}, GreenCaps[BoatCount][3] = {}, GoodRuns[BoatCount][3] = {}, GreenRuns[BoatCount][3] = {};
	int NotFinished = 0, GreenNotFinished = 0, NotFinite = 0, Runs = 0;
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		for (int Wi = 0; Wi < 3; Wi++)
		{
			for (int Di = 0; Di < 8; Di++)
			{
				for (int Shape = 0; Shape < 2; Shape++)
				{
					if (bQuick && (Di % 4 != 1)) continue;
					const uint32_t Seed = 100u + uint32_t(Ci * 1000 + Wi * 100 + Di * 10 + Shape);
					const double Leg = 180.0;
					const FCourseRun G = RunCourse(EBoat(Ci), 0.9, Winds[Wi], 45.0 * Di + 10.0, 0.2, Shape, Leg, 2400.0, Seed);
					Runs++;
					GoodRuns[Ci][Wi]++;
					GoodTime[Ci][Wi] += G.Time;
					GoodCaps[Ci][Wi] += G.Capsizes;
					if (!G.bFinished)
					{
						NotFinished++;
						std::printf("     not finished: %s skill 0.9 wind %.0f from %.0f shape %d (seed %u) at %.0f,%.0f\n", BoatId(EBoat(Ci)), Winds[Wi], 45.0 * Di + 10.0, Shape, Seed, G.EndX, G.EndY);
					}
					if (!G.bFinite) NotFinite++;
					const FCourseRun B = RunCourse(EBoat(Ci), 0.2, Winds[Wi], 45.0 * Di + 10.0, 0.2, Shape, Leg, 3600.0, Seed + 5u);
					Runs++;
					GreenRuns[Ci][Wi]++;
					GreenTime[Ci][Wi] += B.Time;
					GreenCaps[Ci][Wi] += B.Capsizes;
					if (!B.bFinished)
					{
						GreenNotFinished++;
						std::printf("     not finished: %s skill 0.2 wind %.0f from %.0f shape %d (seed %u) at %.0f,%.0f caps %d\n", BoatId(EBoat(Ci)), Winds[Wi], 45.0 * Di + 10.0, Shape, Seed + 5u, B.EndX, B.EndY, B.Capsizes);
					}
					if (!B.bFinite) NotFinite++;
				}
			}
		}
	}
	std::printf("     mean time round the course, s (capsizes per run):   good crew          green crew\n");
	int GreenCapsBlow = 0, GoodCapsAll = 0, GreenCapsLight = 0;
	bool bSlower = true;
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		for (int Wi = 0; Wi < 3; Wi++)
		{
			const double Tg = GoodTime[Ci][Wi] / GoodRuns[Ci][Wi], Tb = GreenTime[Ci][Wi] / GreenRuns[Ci][Wi];
			std::printf("     %-9s %2.0f m/s                                     %6.0f (%.2f)      %6.0f (%.2f)\n", BoatId(EBoat(Ci)), Winds[Wi], Tg, double(GoodCaps[Ci][Wi]) / GoodRuns[Ci][Wi], Tb, double(GreenCaps[Ci][Wi]) / GreenRuns[Ci][Wi]);
			bSlower = bSlower && Tb > Tg * 1.05;
			GoodCapsAll += GoodCaps[Ci][Wi];
			if (Wi == 2) GreenCapsBlow += GreenCaps[Ci][Wi];
			if (Wi == 0) GreenCapsLight += GreenCaps[Ci][Wi];
		}
	}
	Check(NotFinite == 0, "every run stays finite", NotFinite);
	Check(NotFinished == 0, Str("good crews (skill 0.9) all finish (of %d runs)", Runs / 2).c_str(), NotFinished);
	Check(GreenNotFinished == 0, "green crews (skill 0.2) all finish too", GreenNotFinished);
	Check(bSlower, "green crews are slower in every class and wind", bSlower);
	Check(GoodCapsAll <= Runs / 2 / 12, "good crews hardly ever capsize (capsizes in all runs)", GoodCapsAll);
	Check(GreenCapsBlow >= 3, "green crews do capsize sometimes in the 9 m/s gusts (and right the boat and finish)", GreenCapsBlow);
	Check(GreenCapsLight == 0, "and never in 3 m/s", GreenCapsLight);
}

static void TestFleet()
{
	std::printf("-- twelve boats: a start, keeping clear; station keeping; the file behind the launch\n");
	const double Dt = 1.0 / 30.0;
	// twelve luches and cadets wait near the line for a minute and a half, then beat to a mark 250 m upwind
	for (int Wi = 0; Wi < 2; Wi++)
	{
		FWorld W;
		W.Wind.Set(Wi == 0 ? 4.0 : 7.0, 230.0, 0.2);
		double Ux, Uy;
		W.Wind.Upwind(Ux, Uy);
		const double Cx = -Uy, Cy = Ux;
		const int N = 12;
		std::vector<FSailor> F(N);
		for (int I = 0; I < N; I++)
		{
			MakeSailor(F[I], I % 3 == 0 ? EBoat::Cadet : (I % 3 == 1 ? EBoat::Luch : EBoat::Optimist), 0.45 + 0.05 * (I % 10), 300u + I);
			const double Along = (I - 5.5) * 14.0;
			// two rows below the line, every boat with its own patch of water to wait in
			const double Back = 25.0 + 22.0 * (I % 2);
			PlaceSailing(F[I].B, Cx * Along - Ux * Back, Cy * Along - Uy * Back, std::atan2(Uy, Ux) + 80.0 * Deg, 1.0);
			FOrder O;
			O.Kind = EOrder::Station;
			O.X = Cx * Along - Ux * Back;
			O.Y = Cy * Along - Uy * Back;
			O.Radius = 20.0;
			HelmOrder(F[I].H, O);
		}
		double MinGap = 1e9;
		int Contacts = 0, Finished = 0;
		std::vector<int> Touch(N * N, 0);
		FNeighbour Nb[N];
		for (double T = 0.0; T < 1500.0 && Finished < N; T += Dt)
		{
			if (T >= 90.0 && T < 90.0 + Dt)
			{
				for (int I = 0; I < N; I++)
				{
					FOrder O;
					O.Kind = EOrder::SailTo;
					O.X = Ux * 250.0;
					O.Y = Uy * 250.0;
					O.Radius = 12.0;
					O.Lane = 90.0;
					HelmOrder(F[I].H, O);
				}
			}
			for (int I = 0; I < N; I++)
			{
				int C = 0;
				for (int J = 0; J < N; J++)
				{
					if (J != I && F[J].Finish < 0.0) Nb[C++] = AsNeighbour(F[J].B);
				}
				if (F[I].Finish >= 0.0) continue;
				TickSailor(W, F[I], Nb, C, Dt);
				if (T >= 90.0 && F[I].H.bArrived)
				{
					F[I].Finish = T;
					Finished++;
				}
			}
			W.Advance(Dt);
			for (int I = 0; I < N; I++)
			{
				for (int J = I + 1; J < N; J++)
				{
					if (F[I].Finish >= 0.0 || F[J].Finish >= 0.0) continue;
					const double Gap = std::hypot(F[I].B.X - F[J].B.X, F[I].B.Y - F[J].B.Y);
					// hulls touch when the centres are nearer than about 0.4 of the two lengths together (beam-on they would pass closer)
					const double Hit = 0.35 * (Def(F[I].B.Class).Loa + Def(F[J].B.Class).Loa);
					MinGap = std::min(MinGap, Gap / Hit);
					if (Gap < Hit)
					{
						if (!Touch[I * N + J] && std::getenv("SAIL_DEBUG"))
						{
							for (int K : { I, J })
							{
								const FSailor& S = F[K];
								std::printf("     contact t %.1f boat %2d %-8s skill %.2f at %.1f,%.1f v %.2f twa %.0f %s man %d mode %d avoid %.0f slow %.2f order %d\n", T, K, BoatId(S.B.Class), S.H.Skill, S.B.X, S.B.Y, S.B.Out.Speed,
									S.B.Out.TrueWindAngle / Deg, StateName(S.B.State), int(S.H.Maneuver), S.H.Mode, S.H.AvoidYaw / Deg, S.H.AvoidSlow, int(S.H.Order.Kind));
							}
						}
						if (!Touch[I * N + J]) Contacts++;
						Touch[I * N + J] = 1;
					}
					else if (Gap > 1.5 * Hit)
					{
						Touch[I * N + J] = 0;
					}
				}
			}
		}
		int Caps = 0;
		for (int I = 0; I < N; I++) Caps += F[I].Capsizes;
		Check(Finished == N, Str("%.0f m/s: all twelve reach the windward mark (capsizes %d)", W.Wind.Speed, Caps).c_str(), Finished);
		Check(Contacts <= 1, Str("%.0f m/s: boats keep clear of each other (contacts; nearest approach %.2f of touching)", W.Wind.Speed, MinGap).c_str(), Contacts);
	}

	// station keeping: three minutes near a point
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		const double Winds[3] = { 3.0, 6.0, 9.0 };
		for (int Wi = 0; Wi < 3; Wi++)
		{
			double Worst = 0.0, MeanSpeed = 0.0;
			int Caps = 0;
			for (int K = 0; K < 2; K++)
			{
				FWorld W;
				W.Wind.Set(Winds[Wi], 300.0, 0.2);
				W.Hs = 0.011 * Winds[Wi] * Winds[Wi];
				FSailor S;
				MakeSailor(S, EBoat(Ci), K == 0 ? 0.8 : 0.3, 40u + Ci * 10 + Wi + K * 100);
				PlaceSailing(S.B, 30.0, 10.0, 1.0, 1.0);
				FOrder O;
				O.Kind = EOrder::Station;
				O.Radius = 25.0;
				HelmOrder(S.H, O);
				int Nn = 0;
				for (double T = 0.0; T < 240.0; T += Dt)
				{
					TickSailor(W, S, nullptr, 0, Dt);
					W.Advance(Dt);
					if (T > 60.0)
					{
						Worst = std::max(Worst, std::hypot(S.B.X, S.B.Y));
						MeanSpeed += S.B.Out.Sog;
						Nn++;
					}
				}
				MeanSpeed /= Nn;
				Caps += S.Capsizes;
			}
			Check(Worst < 40.0 && (Caps == 0 || Wi == 2), Str("%s holds station for 3 min in %.0f m/s: farthest from the point, m (pace %.1f m/s)", BoatId(EBoat(Ci)), Winds[Wi], MeanSpeed).c_str(), Worst);
		}
	}

	// ducklings: six optimists follow the coach's launch out along a beam reach and round a corner
	{
		FWorld W;
		W.Wind.Set(4.5, 180.0, 0.15);       // from the south: blows toward +Y
		FLaunch L;
		L.X = 40.0;
		const int N = 6;
		std::vector<FSailor> F(N);
		for (int I = 0; I < N; I++)
		{
			MakeSailor(F[I], EBoat::Optimist, 0.25 + 0.05 * I, 500u + I);
			PlaceSailing(F[I].B, 30.0 - 9.0 * I, (I % 2) * 6.0, 0.0, 1.0);
		}
		const double Way[3][2] = { { 320.0, 0.0 }, { 420.0, 140.0 }, { 150.0, 200.0 } };
		int Leg = 0;
		double InSlot = 0.0, Samples = 0.0, MaxBack = 0.0;
		int Contacts = 0;
		FNeighbour Nb[N + 1];
		double T = 0.0;
		for (; T < 1500.0 && Leg < 3; T += Dt)
		{
			double Th, Tu;
			// the coach goes at the fleet's pace and waits when the last duckling drops behind
			const double Last = std::hypot(F[N - 1].B.X - L.X, F[N - 1].B.Y - L.Y);
			LaunchDriveTo(L, Way[Leg][0], Way[Leg][1], Last > 95.0 ? 0.4 : 1.3, 10.0, Th, Tu);
			const FEnv El = W.EnvAt(L.X, L.Y);
			StepLaunch(L, Th, Tu, El, Dt);
			if (std::hypot(Way[Leg][0] - L.X, Way[Leg][1] - L.Y) < 12.0) Leg++;
			for (int I = 0; I < N; I++)
			{
				FOrder& O = F[I].H.Order;
				O.Kind = EOrder::Follow;
				O.LeaderX = L.X;
				O.LeaderY = L.Y;
				O.LeaderYaw = L.Yaw;
				O.LeaderSpeed = L.Speed;
				O.Slot = I + 1;
				O.Gap = 10.0;
				int C = 0;
				for (int J = 0; J < N; J++)
				{
					if (J != I) Nb[C++] = AsNeighbour(F[J].B);
				}
				FNeighbour Ln;
				Ln.X = L.X; Ln.Y = L.Y; Ln.VX = L.VX; Ln.VY = L.VY; Ln.Length = LaunchLength;
				Nb[C++] = Ln;
				TickSailor(W, F[I], Nb, C, Dt);
				if (T > 120.0)
				{
					const double D = std::hypot(F[I].B.X - L.X, F[I].B.Y - L.Y);
					InSlot += std::fabs(D - 10.0 * (I + 1)) < 20.0 ? 1.0 : 0.0;
					Samples += 1.0;
					MaxBack = std::max(MaxBack, D);
				}
				for (int J = 0; J < I; J++)
				{
					if (std::hypot(F[I].B.X - F[J].B.X, F[I].B.Y - F[J].B.Y) < 1.5) Contacts++;
				}
			}
			W.Advance(Dt);
		}
		Check(Leg == 3, "the coach's launch leads six optimists round three legs (s)", T);
		Check(InSlot / std::max(Samples, 1.0) > 0.6 && MaxBack < 160.0, Str("the ducklings keep their file (share of time within 20 m of the slot; farthest %.0f m)", MaxBack).c_str(), InSlot / std::max(Samples, 1.0));
		Check(Contacts == 0, "and do not run into each other", Contacts);
	}
}

static void TestLaunch()
{
	std::printf("-- the coach's launch, towing\n");
	const double Dt = 1.0 / 30.0;
	FLaunch L;
	FEnv E;
	double Tplane = -1.0, MaxPitch = 0.0;
	for (double T = 0.0; T < 30.0; T += Dt)
	{
		StepLaunch(L, 1.0, 0.0, E, Dt);
		if (Tplane < 0.0 && L.Planing > 0.9) Tplane = T;
		MaxPitch = std::max(MaxPitch, L.Pitch);
	}
	Check(L.Speed > 9.0 && L.Speed < 12.0, "full throttle: top speed, m/s", L.Speed);
	Check(Tplane > 2.0 && Tplane < 12.0 && MaxPitch > 4.0 * Deg, "climbs over the hump onto the plane, s", Tplane);
	double Yaw0 = L.Yaw, Turned = 0.0, MaxBank = 0.0;
	for (double T = 0.0; T < 6.0; T += Dt)
	{
		StepLaunch(L, 0.6, 1.0, E, Dt);
		Turned += WrapAngle(L.Yaw - Yaw0);
		Yaw0 = L.Yaw;
		MaxBank = std::max(MaxBank, L.Heel);
	}
	Check(Turned > 90.0 * Deg && MaxBank > 5.0 * Deg, "turns hard to starboard and banks into it (degrees in 6 s)", Turned / Deg);
	for (double T = 0.0; T < 10.0; T += Dt) StepLaunch(L, 0.0, 0.0, E, Dt);
	Check(std::fabs(L.Speed) < 0.3, "throttle shut: stops", L.Speed);
	for (double T = 0.0; T < 6.0; T += Dt) StepLaunch(L, -1.0, 0.0, E, Dt);
	Check(L.Speed < -1.0 && L.Speed > -2.5, "astern, slowly", L.Speed);
	// drive to a point and stop there
	FLaunch D;
	double Th, Tu, T = 0.0;
	for (; T < 200.0; T += Dt)
	{
		LaunchDriveTo(D, -150.0, 220.0, 6.0, 6.0, Th, Tu);
		StepLaunch(D, Th, Tu, E, Dt);
		if (std::hypot(D.X + 150.0, D.Y - 220.0) < 6.0 && std::fabs(D.Speed) < 0.5) break;
	}
	Check(T < 120.0, "the coxswain drives to a point 270 m off and stops there, s", T);
	// tow a capsized-and-righted optimist home
	FBoat B;
	Init(B, EBoat::Optimist, 0.0, 0.3, 3);
	Place(B, -160.0, 215.0, 2.0, EState::Sailing);
	double MaxStretch = 0.0;
	for (T = 0.0; T < 200.0; T += Dt)
	{
		LaunchDriveTo(D, 0.0, 0.0, 2.5, 6.0, Th, Tu);
		StepLaunch(D, Th, Tu, E, Dt);
		const double Hx = D.X - std::cos(D.Yaw) * 0.5 * LaunchLength, Hy = D.Y - std::sin(D.Yaw) * 0.5 * LaunchLength;
		StepTowed(B, Hx, Hy, 6.0, E, Dt);
		const double Bx = B.X + std::cos(B.Yaw) * 0.5 * Def(B.Class).Loa, By = B.Y + std::sin(B.Yaw) * 0.5 * Def(B.Class).Loa;
		MaxStretch = std::max(MaxStretch, std::hypot(Hx - Bx, Hy - By));
		if (std::hypot(D.X, D.Y) < 6.0) break;
	}
	Check(B.State == EState::Towed && std::hypot(B.X, B.Y) < 20.0 && MaxStretch < 6.3, "tows a dinghy home on a 6 m line (longest stretch of the line, m)", MaxStretch);
	Place(B, B.X, B.Y, B.Yaw, EState::Sailing);
	FControls C;
	E.WindX = -5.0;
	Step(B, C, E, Dt);
	Check(B.State == EState::Sailing, "cast off, she is a sailing boat again", 1.0);
}

static void TestLod(bool bQuick)
{
	std::printf("-- far boats: the polar LOD against the full model\n");
	double WorstRatio = 0.0;
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		for (int Wi = 0; Wi < 3; Wi++)
		{
			const double Wind = Wi == 0 ? 3.0 : (Wi == 1 ? 6.0 : 9.0);
			double Near = 0.0, Far = 0.0;
			int NearDone = 0, FarDone = 0;
			const int Seeds = bQuick ? 2 : 4;
			for (int K = 0; K < Seeds; K++)
			{
				const FCourseRun A = RunCourse(EBoat(Ci), 0.8, Wind, 20.0 + 90.0 * K, 0.15, 1, 220.0, 3000.0, 700u + K + Ci * 10 + Wi * 100, false);
				const FCourseRun B = RunCourse(EBoat(Ci), 0.8, Wind, 20.0 + 90.0 * K, 0.15, 1, 220.0, 3000.0, 700u + K + Ci * 10 + Wi * 100, true);
				Near += A.Time;
				Far += B.Time;
				NearDone += A.bFinished;
				FarDone += B.bFinished;
			}
			const double Ratio = Near / Far;    // > 1: the far boat is faster round the same course
			WorstRatio = std::max(WorstRatio, std::fabs(Ratio - 1.0));
			Check(NearDone == Seeds && FarDone == Seeds && std::fabs(Ratio - 1.0) < 0.15, Str("%s %.0f m/s: mean speed round a triangle, far / full (full %.0f s)", BoatId(EBoat(Ci)), Wind, Near / Seeds).c_str(), Ratio);
		}
	}
	// switching back and forth every 20 s: no jump in place, heading or speed
	for (int Ci = 0; Ci < BoatCount; Ci++)
	{
		FWorld W;
		W.Wind.Set(6.0, 200.0, 0.15);
		FSailor S;
		MakeSailor(S, EBoat(Ci), 0.8, 91);
		double Ux, Uy;
		W.Wind.Upwind(Ux, Uy);
		PlaceSailing(S.B, 0, 0, std::atan2(Uy, Ux) + 1.0, 1.0);
		double Marks[8];
		const int NM = CourseMarks(W.Wind, 1, 250.0, Marks);
		FOrder O;
		O.Kind = EOrder::SailTo;
		O.X = Marks[0];
		O.Y = Marks[1];
		HelmOrder(S.H, O);
		const double Dt = 1.0 / 30.0;
		double MaxJump = 0.0, MaxTurn = 0.0, MaxDv = 0.0;
		int Switches = 0, Caps = 0;
		double SpeedAtSwitch = 0.0, SinceSwitch = 100.0;
		int TurnsAtSwitch = 0;
		bool bTurning = false;
		for (double T = 0.0; T < 900.0; T += Dt)
		{
			const bool bFar = int(T / 20.0) % 2 == 1;
			const double X0 = S.B.X, Y0 = S.B.Y, Yaw0 = S.B.Yaw, V0 = S.B.Out.Sog;
			if (bFar != S.bFar)
			{
				Switches++;
				SpeedAtSwitch = V0;
				SinceSwitch = 0.0;
				TurnsAtSwitch = S.H.Tacks + S.H.Gybes;
				bTurning = false;
			}
			S.bFar = bFar;
			TickSailor(W, S, nullptr, 0, Dt);
			W.Advance(Dt);
			SinceSwitch += Dt;
			MaxJump = std::max(MaxJump, std::hypot(S.B.X - X0, S.B.Y - Y0) - std::max(V0, S.B.Out.Sog) * Dt);
			MaxTurn = std::max(MaxTurn, std::fabs(WrapAngle(S.B.Yaw - Yaw0)));
			// the speed in the two seconds after a switch against the speed at it
			// (a tack or a gybe in those two seconds, or one she was caught in the middle of, loses way in either model
			// and is not the switch's doing)
			bTurning = bTurning || std::fabs(S.B.Out.TrueWindAngle) < 35.0 * Deg || S.H.Tacks + S.H.Gybes != TurnsAtSwitch || S.H.Maneuver != EManeuver::None;
			if (SinceSwitch < 2.0 && SpeedAtSwitch > 1.0 && !bTurning)
			{
				const double Dv = std::fabs(S.B.Out.Sog - SpeedAtSwitch) / SpeedAtSwitch;
				if (std::getenv("SAIL_T") ? (std::fabs(T - std::atof(std::getenv("SAIL_T"))) < 2.5 && int(T * 30 + 0.5) % 5 == 0) : (Dv > 0.3 && int(T * 30 + 0.5) % 6 == 0 && std::getenv("SAIL_DEBUG") != nullptr))
				{
					std::printf("   t %.1f far %d  at switch %.2f now %.2f  twa %.0f tws %.1f heel %.0f sheet %.2f boom %.0f luff %.2f hike %.2f scale %.2f water %.2f\n", T, int(bFar), SpeedAtSwitch, S.B.Out.Sog,
						S.B.Out.TrueWindAngle / Deg, S.B.Out.TrueWindSpeed, S.B.Heel / Deg, S.B.Sheet, S.B.Boom / Deg, S.B.Out.MainLuff, S.B.Hike, S.H.SpeedScale, S.B.Water);
				}
				MaxDv = std::max(MaxDv, Dv);
			}
			if (S.H.bArrived)
			{
				S.Leg = (S.Leg + 1) % NM;
				O.X = Marks[2 * S.Leg];
				O.Y = Marks[2 * S.Leg + 1];
				HelmOrder(S.H, O);
			}
			Caps += (S.B.Events & EvCapsize) ? 1 : 0;
		}
		Check(MaxJump < 0.02 && MaxTurn < 0.06, Str("%s: %d switches far <-> full: no jump (m beyond a step's own travel; max turn in a step %.3f rad)", BoatId(EBoat(Ci)), Switches, MaxTurn).c_str(), MaxJump);
		Check(MaxDv < 0.35 && Caps == 0, Str("%s: speed in the 2 s after a switch stays near the speed at it (largest change, share)", BoatId(EBoat(Ci))).c_str(), MaxDv);
	}
}

static uint64_t HashRun(double Dt, uint32_t Seed, double* EndX = nullptr, double* Time = nullptr)
{
	FWorld W;
	W.Wind.Set(6.5, 140.0, 0.25);
	FSailor S;
	MakeSailor(S, EBoat::Cadet, 0.5, Seed);
	double Ux, Uy;
	W.Wind.Upwind(Ux, Uy);
	PlaceSailing(S.B, 0, 0, std::atan2(Uy, Ux) + 1.0, 1.0);
	double Marks[8];
	const int NM = CourseMarks(W.Wind, 1, 150.0, Marks);
	FOrder O;
	O.Kind = EOrder::SailTo;
	O.X = Marks[0];
	O.Y = Marks[1];
	HelmOrder(S.H, O);
	uint64_t Hash = 1469598103934665603ull;
	double T = 0.0;
	for (; T < 1500.0; T += Dt)
	{
		TickSailor(W, S, nullptr, 0, Dt);
		W.Advance(Dt);
		if (S.H.bArrived)
		{
			if (++S.Leg >= NM) break;
			O.X = Marks[2 * S.Leg];
			O.Y = Marks[2 * S.Leg + 1];
			HelmOrder(S.H, O);
		}
		uint64_t Bits;
		const double V = S.B.X * 3.0 + S.B.Y * 7.0 + S.B.Heel;
		std::memcpy(&Bits, &V, sizeof(Bits));
		Hash = (Hash ^ Bits) * 1099511628211ull;
	}
	if (EndX) *EndX = S.B.X;
	if (Time) *Time = T;
	return Hash;
}

static void TestDeterminism()
{
	std::printf("-- determinism, step size\n");
	const uint64_t A = HashRun(1.0 / 30.0, 9), B = HashRun(1.0 / 30.0, 9), C = HashRun(1.0 / 30.0, 10);
	Check(A == B, "the same seed gives the same run, bit for bit", double(A == B));
	Check(A != C, "another seed gives another run", double(A != C));
	double T30 = 0.0, T60 = 0.0, T120 = 0.0;
	HashRun(1.0 / 30.0, 9, nullptr, &T30);
	HashRun(1.0 / 60.0, 9, nullptr, &T60);
	HashRun(1.0 / 120.0, 9, nullptr, &T120);
	Check(T30 < 1400.0 && std::fabs(T120 / T30 - 1.0) < 0.2 && std::fabs(T60 / T30 - 1.0) < 0.2, Str("round the course at dt 1/30, 1/60, 1/120: %.0f, %.0f, %.0f s (1/120 against 1/30)", T30, T60, T120).c_str(), T120 / T30);
	// fixed controls, no helmsman: the physics itself at three step sizes
	double X[4], Y[4], V[4];
	const double Dts[4] = { 1.0 / 20.0, 1.0 / 30.0, 1.0 / 60.0, 1.0 / 120.0 };
	for (int K = 0; K < 4; K++)
	{
		FHold H;
		H.Start(EBoat::Luch, 7.0, 95.0 * Deg, 2.0);
		H.Run(60.0, true, Dts[K]);
		X[K] = H.B.X;
		Y[K] = H.B.Y;
		V[K] = H.B.Out.Speed;
	}
	const double Far = std::max(std::max(std::hypot(X[0] - X[3], Y[0] - Y[3]), std::hypot(X[1] - X[3], Y[1] - Y[3])), std::hypot(X[2] - X[3], Y[2] - Y[3]));
	Check(Far < 0.04 * std::hypot(X[3], Y[3]) && std::fabs(V[1] - V[3]) < 0.05 * V[3], Str("a minute's reach at dt 1/20, 1/30, 1/60 against 1/120: farthest apart, m (of %.0f m sailed)", std::hypot(X[3], Y[3])).c_str(), Far);
	// a slow frame (0.25 s) does not blow up
	{
		FHold H;
		H.Start(EBoat::Optimist, 9.0, 120.0 * Deg, 2.0);
		H.Run(60.0, true, 0.25);
		Check(std::isfinite(H.B.X + H.B.Heel + H.B.Yaw) && std::fabs(H.B.Out.Speed) < 6.0, "a 4 fps frame time does not blow the model up (speed)", H.B.Out.Speed);
	}
}

// ---- a training session in the calendar's wind ----------------------------------------------------------------------

static double BeachDepth(void*, double X, double)
{
	// the sea lies toward +X: a sandy shelf of 4 cm a metre, then 6 m of water
	return std::max(-0.5, std::min(6.0, 0.04 * X));
}

struct FSession
{
	double Wind10 = 0.0, FromDeg = 0.0, Gustiness = 0.0, Hs = 0.0;
	int Boats = 0, Home = 0, Capsizes = 0, Groundings = 0, Out = 0, Laps = 0, Turtles = 0, Contacts = 0;
	double SailShare = 0.0, MaxOff = 0.0, MeanSpeed = 0.0;
	bool bFinite = true;
};

static FSession RunSession(int Year, int Month, int Day, double Hour, bool bTrace)
{
	FSession R;
	LyudWeather::FWindParams P;
	P.Seed = 921;
	const double Hours0 = LyudWeather::HoursSince2000(Year, Month, Day, Hour);
	FWorld W;
	W.Wind.SetCalendar(Hours0, P);
	R.Wind10 = W.Wind.S.Speed;
	R.FromDeg = W.Wind.S.FromDeg;
	R.Gustiness = W.Wind.S.Gustiness;
	W.bWaves = true;
	LyudWeather::FWaveParams Wp;
	W.Waves.Init(Wp);
	// the school sails inside the pier's shelter: the sea there is the wind's own short chop, not the open Baltic's
	const LyudWeather::FWindState Sea = LyudWeather::SeaStateWind(Hours0, P);
	W.Waves.SetWind(Sea.Speed * 0.6f, Sea.DirX, Sea.DirY);
	R.Hs = W.Waves.Hs;
	W.Depth = BeachDepth;
	static const double Poly[8] = { 12.0, -420.0, 620.0, -420.0, 620.0, 420.0, 12.0, 420.0 };
	W.Poly = Poly;
	W.PolyCount = 4;

	// who goes out: the contract's wind limits by class (mean wind at 10 m)
	const double Limit[BoatCount] = { 8.0, 10.0, 12.0 };
	std::vector<FSailor> F;
	for (int I = 0; I < 9; I++)
	{
		const EBoat Class = EBoat(I % 3);
		if (R.Wind10 < 1.5 || R.Wind10 > Limit[int(Class)]) continue;
		F.emplace_back();
		const double Skill = 0.25 + 0.2 * (I / 3) + 0.05 * (I % 3);
		MakeSailor(F.back(), Class, Skill, 2000u + I, CrewMassFor(Class, Class == EBoat::Optimist ? 8.0 + I : (Class == EBoat::Cadet ? 12.0 + I % 4 : 16.0 + 4 * (I / 3))));
		PlaceSailing(F.back().B, 26.0, -30.0 + 8.0 * I, 0.0, 0.5);
	}
	const int N = int(F.size());
	R.Boats = N;
	if (N == 0) return R;
	FLaunch L;
	L.X = 60.0;
	const double Dt = 1.0 / 30.0;
	double Ux, Uy;
	W.Wind.Upwind(Ux, Uy);
	const double Gx = 260.0, Gy = 0.0;      // the middle of the training water
	double Marks[6] = { Gx + Ux * 110.0, Gy + Uy * 110.0, Gx - Uy * 110.0, Gy + Ux * 110.0, Gx - Ux * 110.0, Gy - Uy * 110.0 };
	std::vector<FNeighbour> Nb(N + 4);
	std::vector<int> Phase(N, 0), Touch(N * N, 0);
	double SailSum = 0.0, SpeedSum = 0.0;
	long Samples = 0;
	int NextTrace = 0;
	for (double T = 0.0; T < 7200.0; T += Dt)
	{
		// the wind of the hour (it changes over two hours)
		if (int(T / 60.0) != int((T - Dt) / 60.0))
		{
			const double Dx = W.Wind.DriftX, Dy = W.Wind.DriftY, Ck = W.Wind.Clock;
			W.Wind.SetCalendar(Hours0 + T / 3600.0, P);
			W.Wind.DriftX = Dx;
			W.Wind.DriftY = Dy;
			W.Wind.Clock = Ck;
		}
		// the launch: out to the middle with the file behind it, waits there, then shepherds the last boat home
		double Th = 0.0, Tu = 0.0;
		if (T < 600.0)
		{
			const double Last = std::hypot(F[N - 1].B.X - L.X, F[N - 1].B.Y - L.Y);
			LaunchDriveTo(L, Gx, Gy, Last > 12.0 * N + 40.0 ? 1.0 : 1.6, 8.0, Th, Tu);
		}
		else if (T < 6000.0)
		{
			LaunchDriveTo(L, Gx - 40.0, Gy, 2.0, 10.0, Th, Tu);
		}
		else
		{
			LaunchDriveTo(L, 45.0, 0.0, 2.0, 10.0, Th, Tu);
		}
		StepLaunch(L, Th, Tu, W.EnvAt(L.X, L.Y), Dt);
		for (int I = 0; I < N; I++)
		{
			FSailor& S = F[I];
			FOrder O = S.H.Order;
			int Want = T < 600.0 ? 0 : (T < 900.0 ? 1 : (T < 6000.0 ? 2 : 3));
			if (Want == 0)
			{
				O.Kind = EOrder::Follow;
				O.LeaderX = L.X; O.LeaderY = L.Y; O.LeaderYaw = L.Yaw; O.LeaderSpeed = L.Speed;
				O.Slot = I + 1;
				O.Gap = 11.0;
				S.H.Order = O;
			}
			else if (Want != Phase[I] || (Want == 2 && S.H.bArrived))
			{
				if (Want == 1)
				{
					O = FOrder();
					O.Kind = EOrder::Station;
					O.X = Gx + 30.0 * (I % 3 - 1);
					O.Y = Gy + 30.0 * (I / 3 - 1);
					O.Radius = 25.0;
				}
				else if (Want == 2)
				{
					if (Phase[I] == 2)
					{
						S.Leg++;
						if (S.Leg % 3 == 0) R.Laps++;
					}
					O = FOrder();
					O.Kind = EOrder::SailTo;
					O.X = Marks[2 * (S.Leg % 3)];
					O.Y = Marks[2 * (S.Leg % 3) + 1];
					O.Radius = 12.0;
					O.Lane = 80.0;
				}
				else
				{
					O = FOrder();
					O.Kind = EOrder::Ashore;
					O.X = 2.0;
					O.Y = -30.0 + 8.0 * I;
					O.Radius = 4.0;
				}
				HelmOrder(S.H, O);
			}
			Phase[I] = Want;
			if (Want == 3 && S.H.bArrived && S.B.State != EState::Ashore)
			{
				// the crew jumps out and holds her on the sand
				Place(S.B, S.B.X, S.B.Y, S.B.Yaw, EState::Ashore);
				R.Home++;
			}
			if (S.B.State == EState::Ashore) continue;
			int C = 0;
			for (int J = 0; J < N; J++)
			{
				if (J != I && F[J].B.State != EState::Ashore) Nb[C++] = AsNeighbour(F[J].B);
			}
			FNeighbour Ln;
			Ln.X = L.X; Ln.Y = L.Y; Ln.VX = L.VX; Ln.VY = L.VY; Ln.Length = LaunchLength;
			Nb[C++] = Ln;
			// far from the beach (where the player stands) the boats are moved by the cheap model
			S.bFar = S.B.X > 330.0 && Want == 2;
			const int Grounded = S.Groundings;
			TickSailor(W, S, Nb.data(), C, Dt);
			if (S.Groundings > Grounded && Want != 3) R.Groundings++;
			if (S.B.X < 5.0 && Want != 3) R.Out++;
			if (S.B.X > 640.0 || std::fabs(S.B.Y) > 440.0)
			{
				R.Out++;
				R.MaxOff = std::max(R.MaxOff, std::max(S.B.X - 620.0, std::fabs(S.B.Y) - 420.0));
			}
			const bool bSailing = S.B.State == EState::Sailing || S.B.State == EState::Swamped;
			SailSum += bSailing ? 1.0 : 0.0;
			SpeedSum += S.B.Out.Sog;
			Samples++;
			for (int J = 0; J < I; J++)
			{
				if (F[J].B.State == EState::Ashore || Want == 3) continue;
				const double Gap = std::hypot(S.B.X - F[J].B.X, S.B.Y - F[J].B.Y);
				const double Hit = 0.35 * (Def(S.B.Class).Loa + Def(F[J].B.Class).Loa);
				if (Gap < Hit && !Touch[I * N + J])
				{
					R.Contacts++;
					Touch[I * N + J] = 1;
					if (std::getenv("SAIL_DEBUG"))
					{
						std::printf("   contact t %.0f phase %d: %d %s%s %.0f,%.0f v %.1f twa %.0f %s man %d  /  %d %s%s %.0f,%.0f v %.1f twa %.0f %s man %d\n", T, Want,
							I, BoatId(S.B.Class), S.bFar ? "~" : "", S.B.X, S.B.Y, S.B.Out.Sog, S.B.Out.TrueWindAngle / Deg, StateName(S.B.State), int(S.H.Maneuver),
							J, BoatId(F[J].B.Class), F[J].bFar ? "~" : "", F[J].B.X, F[J].B.Y, F[J].B.Out.Sog, F[J].B.Out.TrueWindAngle / Deg, StateName(F[J].B.State), int(F[J].H.Maneuver));
					}
				}
				else if (Gap > 1.5 * Hit)
				{
					Touch[I * N + J] = 0;
				}
			}
		}
		W.Advance(Dt);
		if (bTrace && int(T / 300.0) >= NextTrace)
		{
			NextTrace = int(T / 300.0) + 1;
			std::printf("  %4.0f min  wind %.1f m/s at the sail  launch %.0f,%.0f |", T / 60.0, W.Wind.Speed, L.X, L.Y);
			for (int I = 0; I < N; I++)
			{
				std::printf(" %c%c %4.0f,%4.0f %.1f%s", BoatId(F[I].B.Class)[0], F[I].bFar ? '~' : ' ', F[I].B.X, F[I].B.Y, F[I].B.Out.Sog,
					F[I].B.State == EState::Sailing ? "" : Str("(%s)", StateName(F[I].B.State)).c_str());
			}
			std::printf("\n");
		}
	}
	for (int I = 0; I < N; I++)
	{
		R.Capsizes += F[I].Capsizes;
		R.Turtles += F[I].Turtles;
		R.bFinite = R.bFinite && F[I].bFinite;
		if (bTrace) std::printf("  %-9s skill %.2f crew %3.0f kg: %2d legs, %d capsizes, %5.0f m sailed, max heel %2.0f, ends %s at %.0f,%.0f\n", BoatId(F[I].B.Class), F[I].H.Skill, F[I].B.CrewMass, F[I].Leg, F[I].Capsizes, F[I].Dist, F[I].MaxHeel / Deg, StateName(F[I].B.State), F[I].B.X, F[I].B.Y);
	}
	R.SailShare = SailSum / std::max(1L, Samples);
	R.MeanSpeed = SpeedSum / std::max(1L, Samples);
	return R;
}

static void TestSessions(bool bQuick)
{
	std::printf("-- two-hour training sessions in the calendar's wind and sea (nine boats, a launch; out in file, station, laps, home)\n");
	// days of the 2027 season at 15:00 picked for a light, a middling, a fresh and a strong (optimists stay ashore) wind
	LyudWeather::FWindParams P;
	P.Seed = 921;
	const double Want[4] = { 3.0, 5.0, 7.0, 9.0 };
	int Found = 0;
	for (int K = 0; K < 4; K++)
	{
		if (bQuick && K != 1) continue;
		int BestM = 5, BestD = 1;
		double BestErr = 1e9;
		for (int M = 5; M <= 10; M++)
		{
			for (int D = 1; D <= 28; D++)
			{
				const double H0 = LyudWeather::HoursSince2000(2027, M, D, 15.0);
				// steady enough over the two hours to be a fair session
				const double S0 = LyudWeather::Regime(H0, P).Speed, S1 = LyudWeather::Regime(H0 + 1.0, P).Speed, S2 = LyudWeather::Regime(H0 + 2.0, P).Speed;
				const double Err = std::fabs(S0 - Want[K]) + std::fabs(S1 - Want[K]) + std::fabs(S2 - Want[K]);
				if (Err < BestErr) { BestErr = Err; BestM = M; BestD = D; }
			}
		}
		const FSession R = RunSession(2027, BestM, BestD, 15.0, false);
		Found++;
		std::printf("     2027-%02d-%02d 15:00  wind %.1f m/s from %.0f, gustiness %.2f, sea %.2f m: %d boats, %d laps, %d capsizes (%d turtles), %d groundings, mean %.2f m/s, sailing %.0f %% of the time\n",
			BestM, BestD, R.Wind10, R.FromDeg, R.Gustiness, R.Hs, R.Boats, R.Laps, R.Capsizes, R.Turtles, R.Groundings, R.MeanSpeed, 100.0 * R.SailShare);
		const std::string Tag = Str("%.0f m/s day: ", Want[K]);
		Check(R.Boats >= (K == 3 ? 6 : 9) && R.bFinite, (Tag + "boats out (optimists stay ashore over 8 m/s), all finite").c_str(), R.Boats);
		Check(R.Home == R.Boats, (Tag + "everyone is back on the beach at the end").c_str(), R.Home);
		Check(R.Laps >= R.Boats, (Tag + "laps of the triangle sailed by the fleet").c_str(), R.Laps);
		Check(R.Out == 0, (Tag + "nobody leaves the training water (steps outside)").c_str(), R.Out);
		Check(R.Groundings <= 2, (Tag + "groundings away from the landing").c_str(), R.Groundings);
		Check(R.SailShare > 0.9, (Tag + "share of the time spent sailing (not capsized)").c_str(), R.SailShare);
		// (some three hundred mark roundings in the two hours: a few touches at the crowded windward mark are the school's everyday)
		Check(R.Contacts <= 6, (Tag + "contacts between boats in two hours (at most 6)").c_str(), R.Contacts);
		Check(K >= 2 || R.Capsizes <= 2, (Tag + "capsizes").c_str(), R.Capsizes);
	}
	Check(Found > 0, "sessions run", Found);
}

static void TestCost()
{
	std::printf("-- cost\n");
	FWorld W;
	W.Wind.Set(6.0, 200.0, 0.2);
	FSailor S;
	MakeSailor(S, EBoat::Cadet, 0.6, 5);
	PlaceSailing(S.B, 0, 0, 0.5, 1.0);
	FOrder O;
	O.Kind = EOrder::Station;
	O.Radius = 60.0;
	HelmOrder(S.H, O);
	FEnv E = W.EnvAt(0, 0);
	FHelmInput In;
	In.WindX = E.WindX;
	In.WindY = E.WindY;
	const int N = 200000;
	const double Dt = 1.0 / 60.0;
	auto T0 = std::chrono::steady_clock::now();
	for (int I = 0; I < N; I++)
	{
		const FControls C = HelmUpdate(S.H, S.B, In, Dt);
		Step(S.B, C, E, Dt);
	}
	auto T1 = std::chrono::steady_clock::now();
	for (int I = 0; I < N; I++)
	{
		if (I % 15 == 0) HelmFar(S.H, S.B, In, Dt * 15.0);
		StepFar(S.B, S.H.CourseYaw, S.H.SpeedScale, E, Dt);
	}
	auto T2 = std::chrono::steady_clock::now();
	const double Full = std::chrono::duration<double, std::micro>(T1 - T0).count() / N, Far = std::chrono::duration<double, std::micro>(T2 - T1).count() / N;
	std::printf("     full step + helmsman %.2f us, far step (helmsman 4 times a second) %.2f us\n", Full, Far);
	Check(Far < 0.6 * Full, "a far boat costs a fraction of a near one (share)", Far / Full);
	auto T3 = std::chrono::steady_clock::now();
	const int Builds = 1;
	(void)Builds;
	WarmPolars();
	auto T4 = std::chrono::steady_clock::now();
	std::printf("     polar tables (already built; first build is about 0.1 s a class): %.3f ms\n", std::chrono::duration<double, std::milli>(T4 - T3).count());
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "polar")
	{
		auto T0 = std::chrono::steady_clock::now();
		WarmPolars();
		auto T1 = std::chrono::steady_clock::now();
		PrintPolar();
		std::printf("\n(built in %.0f ms)\n", std::chrono::duration<double, std::milli>(T1 - T0).count());
		return 0;
	}
	if (Mode == "sail")
	{
		EBoat Class = EBoat::Luch;
		if (Argc > 2 && !BoatById(Argv[2], Class))
		{
			std::printf("unknown class %s (optimist, cadet, luch)\n", Argv[2]);
			return 2;
		}
		const double Wind = Argc > 3 ? std::atof(Argv[3]) : 6.0;
		const double Skill = Argc > 4 ? std::atof(Argv[4]) : 0.8;
		const int Shape = Argc > 5 && std::strcmp(Argv[5], "tri") == 0 ? 1 : 0;
		const double From = Argc > 6 ? std::atof(Argv[6]) : 250.0;
		const double Gust = Argc > 7 ? std::atof(Argv[7]) : 0.2;
		const uint32_t Seed = Argc > 8 ? uint32_t(std::atoi(Argv[8])) : 1u;
		const bool bFar = Argc > 9 && std::strcmp(Argv[9], "far") == 0;
		std::printf("%s, skill %.2f, %.1f m/s from %.0f, gustiness %.2f, %s, legs of 180 m%s\n", BoatId(Class), Skill, Wind, From, Gust, Shape ? "triangle" : "windward-leeward", bFar ? " (far model)" : "");
		const FCourseRun R = RunCourse(Class, Skill, Wind, From, Gust, Shape, 180.0, 3600.0, Seed, bFar, 5);
		std::printf("%s in %.0f s: %.0f m sailed (%.2f m/s), %d tacks, %d gybes, %d capsizes (%.0f s in the water), max heel %.0f\n", R.bFinished ? "finished" : "NOT FINISHED",
			R.Time, R.Dist, R.Dist / std::max(R.Time, 1.0), R.Tacks, R.Gybes, R.Capsizes, R.DownTime, R.MaxHeel / Deg);
		return 0;
	}
	if (Mode == "irons")
	{
		// run.sh irons <class> [wind]: stuck head to wind, then the escape (sheet out, tiller reversed), then the assists
		EBoat Class = EBoat::Optimist;
		if (Argc > 2) BoatById(Argv[2], Class);
		FHold H;
		H.Start(Class, Argc > 3 ? std::atof(Argv[3]) : 6.0, 4.0 * Deg, 0.0, false);
		H.C.Sheet = 0.0;
		int Phase = 0;
		for (double T = 0.0; T < 90.0; T += 1.0)
		{
			if (Phase == 0 && T >= 20.0) { Phase = 1; H.C.Sheet = 1.0; H.C.Rudder = 1.0; }
			if (Phase == 1 && std::fabs(H.B.Out.TrueWindAngle) > 92.0 * Deg)
			{
				Phase = 2;
				H.B.Assist.bAutoSheet = H.B.Assist.bAutoHike = true;
				H.Twa = H.B.Out.TrueWindAngle > 0.0 ? 80.0 * Deg : -80.0 * Deg;
			}
			H.Run(1.0, Phase == 2);
			const FOutputs& O = H.B.Out;
			std::printf("%5.1f ph %d twa %6.1f awa %6.1f v %5.2f lee %5.1f heel %5.1f boom %6.1f sheet %.2f luff %.2f hike %5.2f rud %5.2f yawrate %5.2f push %6.0f %s\n", T + 1.0, Phase, O.TrueWindAngle / Deg, O.AppWindAngle / Deg,
				O.Speed, O.Leeway / Deg, H.B.Heel / Deg, H.B.Boom / Deg, H.B.Sheet, O.MainLuff, H.B.Hike, H.B.Rudder, H.B.YawRate, H.B.HeelMoment, AdviceName(Advice(H.B, H.E)));
		}
		return 0;
	}
	if (Mode == "hold")
	{
		// run.sh hold <class> <wind> <twa> [assist 0/1] [sheet] [hike] [seconds]: a boat held on a true wind angle by its tiller
		EBoat Class = EBoat::Luch;
		if (Argc > 2) BoatById(Argv[2], Class);
		const double Wind = Argc > 3 ? std::atof(Argv[3]) : 6.0, Twa = (Argc > 4 ? std::atof(Argv[4]) : 60.0) * Deg;
		const bool bAssist = Argc > 5 ? std::atoi(Argv[5]) != 0 : true;
		FHold H;
		H.Start(Class, Wind, Twa, Argc > 11 ? std::atof(Argv[11]) : 2.0, bAssist);
		if (Argc > 6) H.C.Sheet = std::atof(Argv[6]);
		if (Argc > 7) H.C.Hike = std::atof(Argv[7]);
		const double Secs = Argc > 8 ? std::atof(Argv[8]) : 30.0;
		const double Twa2 = Argc > 9 ? std::atof(Argv[9]) * Deg : Twa;
		const double Step2 = Argc > 10 ? std::atof(Argv[10]) : 0.5;
		for (double T = 0.0; T < Secs; T += Step2)
		{
			if (T >= Secs * 0.5) H.Twa = Twa2;
			H.Run(Step2);
			const FOutputs& O = H.B.Out;
			std::printf("%5.1f twa %6.1f awa %6.1f aws %4.1f v %5.2f lee %5.1f heel %5.1f rate %5.2f boom %6.1f sheet %.2f luff %.2f dep %.2f hike %5.2f rud %5.2f yawrate %5.2f push %6.0f water %.2f %s %s %s\n", T + Step2, O.TrueWindAngle / Deg, O.AppWindAngle / Deg, O.AppWindSpeed,
				O.Speed, O.Leeway / Deg, H.B.Heel / Deg, H.B.HeelRate, H.B.Boom / Deg, H.B.Sheet, O.MainLuff, H.B.Depower, H.B.Hike, H.B.Rudder, H.B.YawRate, H.B.HeelMoment, H.B.Water, StateName(H.B.State), AdviceName(Advice(H.B, H.E)),
				(H.B.Events & EvTack) ? "TACK" : (H.B.Events & EvGybe) ? "GYBE" : "");
		}
		return 0;
	}
	if (Mode == "session")
	{
		const int Y = Argc > 2 ? std::atoi(Argv[2]) : 2027, M = Argc > 3 ? std::atoi(Argv[3]) : 6, D = Argc > 4 ? std::atoi(Argv[4]) : 12;
		const double H = Argc > 5 ? std::atof(Argv[5]) : 15.0;
		const FSession R = RunSession(Y, M, D, H, true);
		std::printf("%04d-%02d-%02d %02.0f:00  wind %.1f m/s at 10 m from %.0f, gustiness %.2f, sea %.2f m: %d boats out, %d home, %d laps, %d capsizes, %d groundings, %d contacts, %d steps outside, sailing %.0f %%\n",
			Y, M, D, H, R.Wind10, R.FromDeg, R.Gustiness, R.Hs, R.Boats, R.Home, R.Laps, R.Capsizes, R.Groundings, R.Contacts, R.Out, 100.0 * R.SailShare);
		return 0;
	}
	if (Mode != "test" && Mode != "quick")
	{
		std::printf("usage: run.sh [test | quick | polar | sail <class> <wind> [skill] [wl|tri] [fromdeg] [gustiness] [seed] [far] | session <y> <m> <d> <hour>]\n");
		return 2;
	}
	const bool bQuick = Mode == "quick";
	TestPolar();
	TestTackIronsGybe();
	TestHeelCapsize();
	TestWater();
	TestPlayer();
	TestLaunch();
	TestDeterminism();
	TestCourses(bQuick);
	TestFleet();
	TestLod(bQuick);
	TestSessions(bQuick);
	TestCost();
	std::printf("\n%d checks, %d failed: %s\n", Checks, Failures, Failures ? "FAIL" : "all ok");
	return Failures ? 1 : 0;
}
