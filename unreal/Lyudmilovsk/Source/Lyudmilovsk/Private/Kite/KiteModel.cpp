// The kite on its line (see KiteModel.h). Plain C++: tools/kitesim/run.sh runs it outside the engine.
#include "KiteModel.h"

#include <algorithm>
#include <cmath>

namespace LyudKite
{
	double Dot(const FVec& A, const FVec& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	FVec Cross(const FVec& A, const FVec& B) { return FVec(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	double Length(const FVec& A) { return std::sqrt(Dot(A, A)); }
	FVec Normal(const FVec& A, const FVec& Fallback)
	{
		const double L = Length(A);
		return L > 1e-9 ? A * (1.0 / L) : Fallback;
	}

	static double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	static const FVec Up(0, 0, 1);
	static constexpr double Pi = 3.14159265358979323846;
	static constexpr double LineKgPerM = 0.003;   // a braided kite line with its share of the reel's

	FKiteDef Diamond()
	{
		return FKiteDef();
	}

	FKiteDef Box()
	{
		// a box kite: heavier, more sail, steadier (stiffer, less wobble), needs more air, takes more before it spins
		FKiteDef D;
		D.Shape = EShape::Box;
		D.Area = 0.62;
		D.Mass = 0.30;
		D.ClMax = 1.0;
		D.Cd0 = 0.2;
		D.CdPlate = 1.4;
		D.BridleDeg = 20.0;
		D.SpinSpeed = 18.0;
		D.SpinAfter = 2.5;
		D.CalmSpeed = 13.5;
		D.BreakN = 260.0;
		D.Damp = 0.9;
		return D;
	}

	const char* StateName(EState S)
	{
		switch (S)
		{
		case EState::Wound: return "wound";
		case EState::Flying: return "flying";
		case EState::Spinning: return "spinning";
		case EState::Grounded: return "grounded";
		case EState::Snagged: return "snagged";
		case EState::Broken: return "broken";
		}
		return "?";
	}

	static FVec Horizontal(const FVec& V) { return FVec(V.X, V.Y, 0.0); }

	void Launch(FKite& K, const FVec& Hand, const FVec& Wind)
	{
		// held up a little downwind of the hand and let go, the line at LaunchLine
		const FVec Down = Normal(Horizontal(Wind), FVec(1, 0, 0));
		K.Line = LaunchLine;
		K.Pos = Hand + Normal(Down * 0.75 + Up * 0.66) * (LaunchLine * 0.97);
		K.Vel = FVec();
		K.State = EState::Flying;
		K.Tension = 0.0;
		K.Over = 0.0;
		K.SpinPhase = 0.0;
		K.Roll = 0.0;
		K.Clock = 0.0;
	}

	double Elevation(const FKite& K, const FVec& Hand)
	{
		const FVec D = K.Pos - Hand;
		const double H = Length(Horizontal(D));
		return std::atan2(D.Z, std::max(H, 1e-6)) * 180.0 / Pi;
	}

	/** The sail's normal: square to the line (pointing away from the flyer), tipped up by the bridle. */
	static FVec SailNormal(const FKite& K, const FVec& Hand, const FVec& W)
	{
		const FVec E = Normal(K.Pos - Hand, Normal(Horizontal(W), FVec(1, 0, 0)));
		FVec U = Up - E * Dot(Up, E);
		if (Length(U) < 1e-4)
		{
			// straight overhead: tip it back against the wind
			const FVec Back = Normal(Horizontal(W), FVec(1, 0, 0)) * -1.0;
			U = Back - E * Dot(Back, E);
		}
		U = Normal(U);
		const double B = K.Def.BridleDeg * Pi / 180.0;
		return Normal(E * std::cos(B) + U * std::sin(B));
	}

	static void Substep(FKite& K, const FKiteInput& In, double H)
	{
		K.Clock += H;
		// the line: let out or reeled in by the flyer
		K.Line = Clamp(K.Line + In.Reel * H, 0.0, K.Def.LineMax);
		if (K.Line < MinLine)
		{
			K.State = EState::Wound;
			K.Pos = In.Hand;
			K.Vel = FVec();
			K.Tension = 0.0;
			K.Lift = 0.0;
			K.Air = 0.0;
			return;
		}
		const FVec ToKite = K.Pos - In.Hand;
		const double R = Length(ToKite);
		const FVec E = Normal(ToKite, Up);
		const bool bTaut = R >= K.Line - 0.08;
		const double Mass = K.Def.Mass + 0.5 * LineKgPerM * K.Line;

		// the air over the sail
		const FVec Wa = In.Wind - K.Vel;
		const double Va = Length(Wa);
		K.Air = Va;
		FVec Force(0, 0, -Mass * Gravity);
		K.Lift = 0.0;
		const bool bAero = K.State != EState::Grounded || bTaut;   // a kite lying on a slack line just lies
		if (Va > 1e-3 && bAero)
		{
			const FVec W = Wa * (1.0 / Va);
			const FVec N = SailNormal(K, In.Hand, W);
			const double SinA = Clamp(Dot(W, N), -1.0, 1.0);
			const double A = std::asin(SinA);
			K.Aoa = A;
			const double Q = 0.5 * AirDensity * Va * Va * K.Def.Area;
			double Cl = K.Def.ClMax * std::sin(2.0 * A);
			const double Cd = K.Def.Cd0 + K.Def.CdPlate * SinA * SinA;
			FVec L = Normal(N - W * Dot(N, W), Up);
			const FVec Side = Normal(Cross(W, L), FVec(0, 1, 0));
			if (K.State == EState::Spinning)
			{
				// the sail has lost its trim: its lift swings round the air in loops
				K.SpinPhase += H * (4.0 + 0.25 * Va);
				L = L * std::cos(K.SpinPhase) + Side * std::sin(K.SpinPhase);
				Cl *= 0.85;
			}
			else
			{
				// the tail's wag and the sail's flutter: a small sideways sway that grows with the air
				const double Wobble = 0.035 * std::sin(K.Clock * 1.31 + (K.Seed % 97) * 0.7) + 0.02 * std::sin(K.Clock * 3.7 + (K.Seed % 31));
				Force += Side * (Wobble * Q);
				// the tail and the bow of the spars turn its nose into the air: off to one side of the wind it works
				// back toward the middle of the window (downwind of the flyer)
				const FVec Across = Normal(Cross(Up, Horizontal(W)), FVec(0, 1, 0));
				Force -= Across * (0.5 * Q * Dot(E, Across));
			}
			K.Lift = Q * Cl;
			Force += L * (Q * Cl) + W * (Q * Cd);
		}
		// the tail and the sail's own drag on its motion across the line
		Force -= (K.Vel - E * Dot(K.Vel, E)) * (K.Def.Damp * Mass);

		K.Vel += Force * (H / Mass);
		K.Pos += K.Vel * H;

		// the line holds it: never further than the length out; reeling in pulls it toward the hand
		double Tension = 0.0;
		const FVec D2 = K.Pos - In.Hand;
		const double R2 = Length(D2);
		if (R2 > K.Line)
		{
			const FVec E2 = D2 * (1.0 / R2);
			K.Pos = In.Hand + E2 * K.Line;
			const double Vr = Dot(K.Vel, E2);
			const double Allowed = std::min(In.Reel, 0.0);  // the line comes in at the reel's speed
			if (Vr > Allowed)
			{
				K.Vel -= E2 * (Vr - Allowed);
				Tension = Mass * (Vr - Allowed) / H;
			}
		}
		K.Tension += (Tension - K.Tension) * std::min(1.0, H * 6.0);

		// the ground
		const double Floor = In.GroundZ + 0.05;
		if (K.Pos.Z <= Floor)
		{
			const double Impact = -K.Vel.Z;
			K.Pos.Z = Floor;
			if (K.Vel.Z < 0.0)
			{
				K.Vel.Z = 0.0;
			}
			const double Rub = std::max(0.0, 1.0 - 6.0 * H);
			K.Vel.X *= Rub;
			K.Vel.Y *= Rub;
			if (K.State == EState::Spinning && Impact > 3.0)
			{
				K.State = EState::Grounded;   // a crash: it lies where it fell
			}
			else if (K.State == EState::Flying && Length(K.Vel) < 0.6 && K.Lift < Mass * Gravity)
			{
				K.State = EState::Grounded;
			}
		}
		else if (K.State == EState::Grounded && K.Pos.Z > In.GroundZ + 0.6)
		{
			K.State = EState::Flying;     // lifted again by a tug in enough wind
			K.Over = 0.0;
		}

		// too much air: a spin; easing air: out of it
		if (K.State == EState::Flying)
		{
			K.Over = Va > K.Def.SpinSpeed ? K.Over + H : std::max(0.0, K.Over - 2.0 * H);
			if (K.Over > K.Def.SpinAfter)
			{
				K.State = EState::Spinning;
				K.Over = 0.0;
				K.SpinPhase = 0.0;
			}
		}
		else if (K.State == EState::Spinning)
		{
			K.Over = Va < K.Def.CalmSpeed ? K.Over + H : std::max(0.0, K.Over - H);
			if (K.Over > 1.0)
			{
				K.State = EState::Flying;
				K.Over = 0.0;
			}
		}
		if (K.Tension > K.Def.BreakN)
		{
			K.State = EState::Broken;
			K.Tension = 0.0;
		}

		// how it tilts, for drawing
		K.Roll = K.State == EState::Spinning ? K.SpinPhase : 0.22 * std::sin(K.Clock * 1.31 + (K.Seed % 97) * 0.7) * Clamp(Va / 8.0, 0.0, 1.0);
	}

	void Step(FKite& K, const FKiteInput& In, double Dt)
	{
		if (IsLost(K))
		{
			return;
		}
		if (In.bHitObstacle && (IsUp(K) || K.State == EState::Grounded) && K.State != EState::Wound)
		{
			K.State = EState::Snagged;
			K.Vel = FVec();
			K.Tension = 0.0;
			return;
		}
		if (K.State == EState::Wound)
		{
			K.Pos = In.Hand;
			K.Line = std::max(0.0, std::min(K.Line, MinLine * 0.5));
			return;
		}
		const int N = std::max(1, (int)std::ceil(Dt * 120.0));
		const double H = Dt / N;
		for (int I = 0; I < N && !IsLost(K) && K.State != EState::Wound; ++I)
		{
			Substep(K, In, H);
		}
	}

	void Frame(const FKite& K, const FVec& Hand, const FVec& Wind, FVec& OutForward, FVec& OutNormal)
	{
		const FVec W = Normal(Wind - K.Vel, Normal(Horizontal(Wind), FVec(1, 0, 0)));
		const FVec N = SailNormal(K, Hand, W);
		FVec F = Normal(Up - N * Dot(Up, N), FVec(1, 0, 0));
		// the roll about the sail's normal
		const FVec S = Cross(N, F);
		F = Normal(F * std::cos(K.Roll) + S * std::sin(K.Roll), F);
		OutForward = F;
		OutNormal = N;
	}

	void LinePoints(const FKite& K, const FVec& Hand, int N, FVec* Out)
	{
		const FVec D = K.Pos - Hand;
		const double R = Length(D);
		const double Slack = std::max(0.0, K.Line - R);
		double Sag;
		if (Slack > 0.05 || K.Tension < 0.2)
		{
			// a slack line hangs: the depth of a parabola whose arc is the length out
			Sag = std::sqrt(0.375 * R * std::max(Slack, 0.05));
		}
		else
		{
			// a taut line sags under its own weight across the span: w L^2 / 8 T
			const double Span = Length(Horizontal(D)) + 0.2 * std::fabs(D.Z);
			Sag = LineKgPerM * Gravity * Span * Span / (8.0 * std::max(K.Tension, 0.5));
		}
		Sag = std::min(Sag, 0.3 * std::max(R, 0.1));
		for (int I = 0; I <= N; ++I)
		{
			const double T = double(I) / double(N);
			Out[I] = Hand + D * T - Up * (Sag * 4.0 * T * (1.0 - T));
		}
	}

	double Hum(const FKite& K)
	{
		if (!IsUp(K))
		{
			return 0.0;
		}
		return Clamp(K.Tension / 25.0, 0.0, 1.0) * Clamp((K.Air - 2.0) / 9.0, 0.0, 1.0);
	}

	double AutoReel(const FKite& K, const FVec& Hand, double Want)
	{
		switch (K.State)
		{
		case EState::Spinning:
			return -ReelIn;
		case EState::Grounded:
		{
			// wind the slack in; once taut, a tug (if there is air enough it lifts)
			const double R = Length(K.Pos - Hand);
			return K.Line > R + 0.3 ? -ReelIn : -0.8;
		}
		case EState::Flying:
		{
			const double El = Elevation(K, Hand);
			if (K.Line < 8.0)
			{
				// the launch: line out slowly while it climbs, held while it doesn't
				return El > 28.0 && K.Vel.Z > -0.3 ? 0.9 : 0.0;
			}
			if (El < 22.0 && K.Vel.Z < 0.0)
			{
				return -1.5;    // sagging: a pull makes its own air
			}
			if (K.Line < Want && El > 35.0)
			{
				return std::min(ReelOut, 0.6 + 0.12 * K.Air);
			}
			if (K.Line > Want + 3.0)
			{
				return -ReelIn * 0.5;
			}
			return 0.0;
		}
		default:
			return 0.0;
		}
	}
}
