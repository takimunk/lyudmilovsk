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
		D.FlyFrom = 5.0;
		return D;
	}

	FKiteDef Delta()
	{
		// a delta: twice the diamond's sail for less weight, a keel instead of a tail. It flies in almost nothing and
		// stands nearly overhead; a fresh breeze overpowers it early
		FKiteDef D;
		D.Shape = EShape::Delta;
		D.Area = 0.95;
		D.Mass = 0.085;
		D.ClMax = 1.25;
		D.Cd0 = 0.07;
		D.CdPlate = 1.3;
		D.BridleDeg = 14.0;
		D.SpinSpeed = 9.5;
		D.SpinAfter = 1.2;
		D.CalmSpeed = 6.5;
		D.BreakN = 150.0;
		D.Damp = 0.5;
		D.Wobble = 0.7;
		D.Glide = 0.85;
		D.FlyFrom = 2.0;
		D.FlyLine = 30.0;
		return D;
	}

	FKiteDef Stunt()
	{
		// a two-line delta wing 1.8 m across on carbon spars: no tail, the wing nearly square to its lines, so it has
		// speed instead of steadiness
		FKiteDef D;
		D.Shape = EShape::Stunt;
		D.Lines = 2;
		D.Area = 0.75;
		D.Mass = 0.26;
		D.ClMax = 1.15;
		D.Cd0 = 0.11;
		D.CdPlate = 1.5;
		D.BridleDeg = 7.0;
		D.SpinSpeed = 1e9;      // it doesn't spin of itself: it is flown into the ground instead
		D.CalmSpeed = 1e9;
		D.BreakN = 480.0;       // two braided lines
		D.LineMax = 30.0;
		D.Damp = 0.1;
		D.Wobble = 0.0;
		D.FlyFrom = 4.0;
		D.FlyLine = 22.0;
		D.SparSpeed = 24.0;
		return D;
	}

	FKiteDef Dragon()
	{
		// a dragon: a round face half a metre across and twelve metres of tail. The tail is most of its drag and all of
		// its steadiness: it hangs low on its line and nothing much upsets it
		FKiteDef D;
		D.Shape = EShape::Dragon;
		D.Area = 0.5;
		D.Mass = 0.24;
		D.ClMax = 1.0;
		D.Cd0 = 0.55;
		D.CdPlate = 1.5;
		D.BridleDeg = 30.0;
		D.SpinSpeed = 21.0;
		D.SpinAfter = 3.0;
		D.CalmSpeed = 15.0;
		D.BreakN = 300.0;
		D.Damp = 1.5;
		D.Wobble = 0.25;
		D.TailLength = 12.0;
		D.FlyFrom = 4.0;
		D.FlyLine = 22.0;
		return D;
	}

	FKiteDef ByShape(EShape Shape)
	{
		switch (Shape)
		{
		case EShape::Box: return Box();
		case EShape::Delta: return Delta();
		case EShape::Stunt: return Stunt();
		case EShape::Dragon: return Dragon();
		default: return Diamond();
		}
	}

	const char* ShapeName(EShape Shape)
	{
		switch (Shape)
		{
		case EShape::Diamond: return "diamond";
		case EShape::Box: return "box";
		case EShape::Delta: return "delta";
		case EShape::Stunt: return "stunt";
		case EShape::Dragon: return "dragon";
		}
		return "?";
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
		case EState::Held: return "held";
		case EState::Loose: return "loose";
		}
		return "?";
	}

	static FVec Horizontal(const FVec& V) { return FVec(V.X, V.Y, 0.0); }

	/** A kite's working state as it leaves the hand or the helper's. */
	static void Fresh(FKite& K, const FVec& Hand)
	{
		K.Vel = FVec();
		K.Tension = 0.0;
		K.Over = 0.0;
		K.SpinPhase = 0.0;
		K.Roll = 0.0;
		K.Clock = 0.0;
		K.Nose = Up;
		K.Twist = 0.0;
		K.Bank = 0.0;
		K.bNoseUp = true;
		K.bParked = false;
		K.bSpar = false;
		K.Flip = 0.0;
		K.Impact = 0.0;
		K.Arm = 0.0;
		K.LastHand = Hand;
		K.bHand = true;
		K.HandVel = FVec();
		K.Draw = 0.0;
	}

	void Launch(FKite& K, const FVec& Hand, const FVec& Wind)
	{
		// held up a little downwind of the hand and let go, the line at LaunchLine
		const FVec Down = Normal(Horizontal(Wind), FVec(1, 0, 0));
		Fresh(K, Hand);
		K.Line = LaunchLine;
		K.Pos = Hand + Normal(Down * 0.75 + Up * 0.66) * (LaunchLine * 0.97);
		K.State = EState::Flying;
	}

	void LongLaunch(FKite& K, const FVec& Hand, const FVec& HeldAt, const FVec& Wind)
	{
		Fresh(K, Hand);
		K.HeldAt = HeldAt;
		K.Pos = HeldAt;
		K.Line = Clamp(Length(HeldAt - Hand), MinLine + 0.5, K.Def.LineMax);
		K.CrashLine = Normal(Horizontal(HeldAt - Hand), Normal(Horizontal(Wind), FVec(1, 0, 0)));
		K.State = EState::Held;
	}

	void LayDown(FKite& K, const FVec& Hand, const FVec& At, const FVec& Wind)
	{
		Fresh(K, Hand);
		K.Pos = At;
		K.Line = Clamp(Length(At - Hand) + 0.3, MinLine + 0.5, K.Def.LineMax);
		K.CrashLine = Normal(Horizontal(At - Hand), Normal(Horizontal(Wind), FVec(1, 0, 0)));
		K.State = EState::Grounded;
		K.bParked = true;
	}

	void Free(FKite& K)
	{
		if (K.State == EState::Snagged)
		{
			K.State = EState::Loose;
			K.Vel = FVec();
			K.Tension = 0.0;
			K.Line = K.Def.LineMax;     // nobody holds its line any more
			K.Arm = 0.0;
			K.bHand = false;
		}
	}

	double Elevation(const FKite& K, const FVec& Hand)
	{
		const FVec D = K.Pos - Hand;
		const double H = Length(Horizontal(D));
		return std::atan2(D.Z, std::max(H, 1e-6)) * 180.0 / Pi;
	}

	void Window(const FKite& K, const FVec& Hand, const FVec& Wind, double& OutAzimuth, double& OutElevation)
	{
		const FVec Down = Normal(Horizontal(Wind), FVec(1, 0, 0));
		const FVec Right = Cross(Up, Down);
		const FVec D = K.Pos - Hand;
		OutAzimuth = std::atan2(Dot(D, Right), Dot(D, Down)) * 180.0 / Pi;
		OutElevation = Elevation(K, Hand);
	}

	/** The line's direction, and the two directions square to it as the flyer sees them: up the window and to the right. */
	static void LineFrame(const FKite& K, const FVec& Hand, FVec& E, FVec& U, FVec& R)
	{
		E = Normal(K.Pos - Hand, Up);
		U = Up - E * Dot(Up, E);
		if (Length(U) < 1e-4)
		{
			const FVec Back(-1, 0, 0);
			U = Back - E * Dot(Back, E);
		}
		U = Normal(U);
		R = Cross(U, E);
	}

	double Heading(const FKite& K, const FVec& Hand)
	{
		FVec E, U, R;
		LineFrame(K, Hand, E, U, R);
		return std::atan2(Dot(K.Nose, R), Dot(K.Nose, U)) * 180.0 / Pi;
	}

	double SteerGain(const FKite& K)
	{
		// the first three turns of twist slide over each other; after that every turn binds the lines a little more
		return 1.0 / (1.0 + 0.45 * std::max(0.0, std::fabs(K.Twist) - 3.0));
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

	/** Tumbling air: three unrelated swings on each axis by the kite's own clock, Rms m/s in all. */
	static FVec Shake(double T, uint32_t Seed, double Rms)
	{
		const double P = double(Seed % 1009) * 0.37;
		auto Axis = [T, P](double A, double B, double C, double O)
		{
			return 0.75 * std::sin(T * A + P + O) + 0.5 * std::sin(T * B + 1.7 * P + 2.0 * O) + 0.35 * std::sin(T * C + 2.9 * P + 3.0 * O);
		};
		// each axis' swing has an rms of 0.684; the three together make Rms
		const double S = Rms * 0.844;
		return FVec(Axis(1.7, 4.1, 9.3, 0.0) * S, Axis(2.3, 5.2, 11.1, 1.1) * S, Axis(1.3, 3.4, 7.9, 2.3) * S);
	}

	/** A two-line wing: it flies along its nose, the lines turn the nose, and the air does the rest. */
	static void StuntAir(FKite& K, const FKiteInput& In, const FVec& Wa, double Va, bool bTaut, double H, FVec& Force)
	{
		FVec E, U, R;
		LineFrame(K, In.Hand, E, U, R);
		FVec F = K.Nose - E * Dot(K.Nose, E);
		F = Normal(F, U);
		if (K.State == EState::Grounded)
		{
			F = U;      // stood on its tail edge
		}
		double Rate = 0.0;
		if (K.State == EState::Flying && bTaut)
		{
			// a pulled line turns it, the faster the faster it flies; twisted lines bind
			Rate = Clamp(In.Steer, -1.0, 1.0) * SteerGain(K) * std::min(Va / K.Def.TurnRadius, K.Def.TurnMax);
			const double Psi = Rate * H;
			F = Normal(F * std::cos(Psi) + Cross(F, E) * std::sin(Psi), F);
			K.Twist += Psi / (2.0 * Pi);
		}
		K.Bank += (Rate / K.Def.TurnMax - K.Bank) * std::min(1.0, H * 8.0);
		K.Nose = F;
		if (Va < 1e-3)
		{
			return;
		}
		const FVec W = Wa * (1.0 / Va);
		const double B = K.Def.BridleDeg * Pi / 180.0;
		const FVec N = Normal(E * std::cos(B) + F * std::sin(B));
		const double SinA = Clamp(Dot(W, N), -1.0, 1.0);
		const double A = std::asin(SinA);
		K.Aoa = A;
		const double Q = 0.5 * AirDensity * Va * Va * K.Def.Area;
		const double Cl = K.Def.ClMax * std::sin(2.0 * A);
		const double Cd = K.Def.Cd0 + K.Def.CdPlate * SinA * SinA;
		const FVec L = Normal(N - W * Dot(N, W), F);
		K.Lift = Q * Cl;
		Force += L * (Q * Cl) + W * (Q * Cd);
		// the wing grips the air sideways: it goes where its nose points
		const FVec S = Cross(E, F);
		Force += S * (0.5 * AirDensity * K.Def.Area * K.Def.Keel * Va * Dot(Wa, S));
		// air from behind (a stall, sliding back down the window's edge): the sail bags and it settles tail first
		const double Back = Dot(Wa, F);
		if (Back > 0.0)
		{
			Force += F * (0.5 * AirDensity * K.Def.Area * 1.1 * Back * Back);
		}
	}

	static void Substep(FKite& K, const FKiteInput& In, double H)
	{
		K.Clock += H;
		// the arms: a pull takes a little line in fast and gives it back slower; thrown forward they give as much slack
		const double ArmWas = K.Arm;
		K.Arm += Clamp(Clamp(In.Pull, -1.0, 1.0) * ArmReach - K.Arm, (In.Pull < 0.0 ? -3.5 : -2.0) * H, 3.5 * H);
		const double ArmIn = std::max((K.Arm - ArmWas) / H, 0.0);
		// a pull is what goes past where the hands rest (taking back what was given is no pull)
		const double ArmPull = (std::max(K.Arm, 0.0) - std::max(ArmWas, 0.0)) / H;
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
			K.Arm = 0.0;
			K.Twist = 0.0;      // wound up: the lines are laid straight again and a popped spar pushed back
			K.bSpar = false;
			return;
		}
		const bool bTwoLine = K.Def.Lines == 2;
		if (K.State == EState::Loose)
		{
			// no line on it: it rocks down like a leaf, drifting a little with the air
			const FVec Drift = Horizontal(In.Wind) * 0.25;
			K.Vel = FVec(Drift.X + 0.6 * std::sin(K.Clock * 2.3), Drift.Y + 0.6 * std::cos(K.Clock * 1.9), -1.7);
			K.Pos += K.Vel * H;
			K.Roll = 0.7 * std::sin(K.Clock * 4.1);
			K.Tension = 0.0;
			K.Lift = 0.0;
			if (K.Pos.Z <= In.GroundZ + 0.05)
			{
				K.Pos.Z = In.GroundZ + 0.05;
				K.Vel = FVec();
				K.State = EState::Grounded;
				K.bNoseUp = false;
				K.bParked = false;
				K.Roll = 0.0;
			}
			return;
		}
		const double Len = std::max(K.Line - K.Arm, 0.3);
		if (K.State == EState::Held)
		{
			// in the helper's hands: the line runs off the reel as the flyer backs away; a pull on it taut, and they let go
			K.Pos = K.HeldAt;
			K.Vel = FVec();
			K.Tension = 0.0;
			K.Lift = 0.0;
			FVec E, U, R;
			LineFrame(K, In.Hand, E, U, R);
			const double Dist = Length(K.Pos - In.Hand);
			if (Dist > K.Line)
			{
				K.Line = std::min(Dist, K.Def.LineMax);
			}
			K.Draw = -std::min(In.Reel, 0.0) + std::max(ArmPull, 0.0) - Dot(K.HandVel, E);
			const bool bPulled = Dist >= K.Line - K.Arm - 0.15 && K.Draw > 0.4;
			if (!In.bRelease && !bPulled && Dist < K.Def.LineMax + 0.5)
			{
				return;
			}
			// let go with a push up
			K.State = EState::Flying;
			K.Vel = U * 1.5;
			K.Nose = U;
			K.Clock = 0.0;
			K.Over = 0.0;
		}
		const FVec ToKite = K.Pos - In.Hand;
		const double R = Length(ToKite);
		const FVec E = Normal(ToKite, Up);
		const bool bTaut = R >= Len - 0.08;
		const double Mass = K.Def.Mass + 0.5 * LineKgPerM * K.Line;
		K.Draw = -std::min(In.Reel, 0.0) + std::max(ArmPull, 0.0) - Dot(K.HandVel, E);

		// the air over the sail
		FVec Air = In.Wind;
		if (In.Turbulence > 0.0)
		{
			Air += Shake(K.Clock, K.Seed, In.Turbulence);
		}
		const FVec Wa = Air - K.Vel;
		const double Va = Length(Wa);
		K.Air = Va;
		FVec Force(0, 0, -Mass * Gravity);
		K.Lift = 0.0;
		if (bTwoLine)
		{
			bool bAero = true;
			if (K.State == EState::Grounded)
			{
				if (!K.bNoseUp && !K.bSpar)
				{
					// flat on its nose: one line pulled for a moment rolls it over a wing tip onto its tail edge, and so
					// does walking a good way round it
					K.Flip = bTaut && std::fabs(In.Steer) > 0.5 ? K.Flip + H : std::max(0.0, K.Flip - H);
					if (K.Flip > 1.0 || Dot(Normal(Horizontal(E), K.CrashLine), K.CrashLine) < 0.8)
					{
						K.bNoseUp = true;
						K.bParked = true;
						K.Flip = 0.0;
					}
				}
				else if (K.bNoseUp && K.bParked && bTaut && K.Draw > 0.4)
				{
					K.bParked = false;      // tipped forward into the wind by the pull
				}
				bAero = K.bNoseUp && !K.bParked && !K.bSpar && bTaut;
			}
			if (bAero)
			{
				StuntAir(K, In, Wa, Va, bTaut, H, Force);
			}
		}
		else
		{
			const bool bAero = K.State != EState::Grounded || bTaut;   // a kite lying on a slack line just lies
			if (Va > 1e-3 && bAero)
			{
				const FVec W = Wa * (1.0 / Va);
				FVec N = SailNormal(K, In.Hand, W);
				if (K.Def.Glide > 0.0 && R < Len - 0.5 && K.State == EState::Flying)
				{
					// on a slack line a delta takes a glider's trim (a few degrees to the air it meets) instead of tumbling:
					// it sails on over the flyer's head and sinks slowly
					const FVec Ahead = Normal(Horizontal(In.Hand - K.Pos), Normal(Horizontal(W), FVec(1, 0, 0)) * -1.0);
					FVec Lp = Up - W * Dot(Up, W);
					if (Length(Lp) < 0.2)
					{
						Lp = Ahead - W * Dot(Ahead, W);
					}
					const double Trim = 9.0 * Pi / 180.0;
					const FVec Ng = Normal(Normal(Lp) * std::cos(Trim) + W * std::sin(Trim));
					N = Normal(N * (1.0 - K.Def.Glide) + Ng * K.Def.Glide, Ng);
				}
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
					const double Wobble = (0.035 * std::sin(K.Clock * 1.31 + (K.Seed % 97) * 0.7) + 0.02 * std::sin(K.Clock * 3.7 + (K.Seed % 31))) * K.Def.Wobble;
					Force += Side * (Wobble * Q);
					// the tail and the bow of the spars turn its nose into the air: off to one side of the wind it works
					// back toward the middle of the window (downwind of the flyer)
					const FVec Across = Normal(Cross(Up, Horizontal(W)), FVec(0, 1, 0));
					Force -= Across * (0.5 * Q * Dot(E, Across));
				}
				K.Lift = Q * Cl;
				Force += L * (Q * Cl) + W * (Q * Cd);
			}
		}
		// the tail and the sail's own drag on its motion across the line
		Force -= (K.Vel - E * Dot(K.Vel, E)) * (K.Def.Damp * Mass);

		K.Vel += Force * (H / Mass);
		K.Pos += K.Vel * H;

		// the line holds it: never further than the length out; reeling in pulls it toward the hand, and so does a hand
		// that moves away (a flyer running upwind)
		double Tension = 0.0;
		const FVec D2 = K.Pos - In.Hand;
		const double R2 = Length(D2);
		if (R2 > Len)
		{
			const FVec E2 = D2 * (1.0 / R2);
			K.Pos = In.Hand + E2 * Len;
			const double Vr = Dot(K.Vel - K.HandVel, E2);
			const double Allowed = std::min(In.Reel, 0.0) - ArmIn;  // the line comes in at the reel's speed and the arms'
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
			const double Speed = Length(K.Vel);
			K.Pos.Z = Floor;
			if (K.Vel.Z < 0.0)
			{
				K.Vel.Z = 0.0;
			}
			const double Rub = std::max(0.0, 1.0 - 6.0 * H);
			K.Vel.X *= Rub;
			K.Vel.Y *= Rub;
			if (bTwoLine)
			{
				if (K.State == EState::Flying)
				{
					// a wing that touches the sand stops there: gently and nose to the sky it stands on its tail edge (a
					// landing); anything else is a crash and it lies on its nose
					K.Impact = Speed;
					K.State = EState::Grounded;
					K.bNoseUp = K.Nose.Z > 0.4 && Speed < 4.5;
					K.bParked = K.bNoseUp;
					K.bSpar = !K.bNoseUp && K.Nose.Z < -0.5 && Speed > K.Def.SparSpeed;
					K.Flip = 0.0;
					K.CrashLine = Normal(Horizontal(D2), K.CrashLine);
					K.Vel = FVec();
				}
			}
			else if (K.State == EState::Spinning && Impact > 3.0)
			{
				K.Impact = Impact;
				K.State = EState::Grounded;   // a crash: it lies where it fell
			}
			else if (K.State == EState::Flying && Length(K.Vel) < 0.6 && K.Lift < Mass * Gravity)
			{
				K.Impact = Impact;
				K.State = EState::Grounded;
			}
		}
		else if (K.State == EState::Grounded && K.Pos.Z > In.GroundZ + 0.6 && !K.bSpar)
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
		K.Roll = K.State == EState::Spinning ? K.SpinPhase : 0.22 * K.Def.Wobble * std::sin(K.Clock * 1.31 + (K.Seed % 97) * 0.7) * Clamp(Va / 8.0, 0.0, 1.0);
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
			K.LastHand = In.Hand;
			K.bHand = true;
			K.HandVel = FVec();
			return;
		}
		// the hand's own motion (a flyer walking back, running upwind), eased: a hand shakes
		const FVec From = K.bHand ? K.LastHand : In.Hand;
		FVec Moved = In.Hand - From;
		if (Length(Moved) > 5.0)
		{
			Moved = FVec();     // a jump, not a walk
		}
		if (Dt > 1e-6)
		{
			K.HandVel += (Moved * (1.0 / Dt) - K.HandVel) * std::min(1.0, Dt * 8.0);
		}
		const int N = std::max(1, (int)std::ceil(Dt * 120.0));
		const double H = Dt / N;
		FKiteInput Sub = In;
		for (int I = 0; I < N && !IsLost(K) && K.State != EState::Wound; ++I)
		{
			Sub.Hand = In.Hand - Moved * (double(N - 1 - I) / double(N));
			Substep(K, Sub, H);
		}
		K.LastHand = In.Hand;
		K.bHand = true;
	}

	void Frame(const FKite& K, const FVec& Hand, const FVec& Wind, FVec& OutForward, FVec& OutNormal)
	{
		if (K.Def.Lines == 2 && K.State != EState::Held && K.State != EState::Loose)
		{
			FVec E, U, R;
			LineFrame(K, Hand, E, U, R);
			if (K.State == EState::Grounded && !K.bNoseUp)
			{
				// flat on the sand, its nose the way it fell
				OutNormal = Up;
				OutForward = Normal(Horizontal(K.Nose), Normal(Horizontal(E), FVec(1, 0, 0)));
				return;
			}
			if (K.State == EState::Grounded && K.bParked)
			{
				// stood on its tail edge, leaning back out of the wind
				const double Lean = 25.0 * Pi / 180.0;
				OutForward = Normal(U * std::cos(Lean) + E * std::sin(Lean));
				OutNormal = Normal(E * std::cos(Lean) - U * std::sin(Lean));
				return;
			}
			const FVec F = Normal(K.Nose - E * Dot(K.Nose, E), U);
			const double B = K.Def.BridleDeg * Pi / 180.0;
			// banked into its turn: the inside wing tip drops back
			const FVec S = Cross(F, E);
			const FVec N = Normal(E * std::cos(B) + F * std::sin(B) - S * (0.35 * Clamp(K.Bank, -1.0, 1.0)));
			OutNormal = N;
			OutForward = Normal(F - N * Dot(F, N), F);
			return;
		}
		const FVec W = Normal(Wind - K.Vel, Normal(Horizontal(Wind), FVec(1, 0, 0)));
		const FVec N = SailNormal(K, Hand, W);
		FVec F = Normal(Up - N * Dot(Up, N), FVec(1, 0, 0));
		// the roll about the sail's normal
		const FVec S = Cross(N, F);
		F = Normal(F * std::cos(K.Roll) + S * std::sin(K.Roll), F);
		OutForward = F;
		OutNormal = N;
	}

	void LineBetween(const FKite& K, const FVec& From, const FVec& To, int N, FVec* Out)
	{
		const FVec D = To - From;
		const double R = Length(D);
		const double Slack = std::max(0.0, K.Line - K.Arm - R);
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
			Out[I] = From + D * T - Up * (Sag * 4.0 * T * (1.0 - T));
		}
	}

	void LinePoints(const FKite& K, const FVec& Hand, int N, FVec* Out)
	{
		LineBetween(K, Hand, K.Pos, N, Out);
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
		// a dragon hangs low on its line: what would be a sagging diamond is a dragon flying well
		const double Low = K.Def.Shape == EShape::Dragon ? 0.6 : 1.0;
		switch (K.State)
		{
		case EState::Spinning:
			return -ReelIn;
		case EState::Held:
			return -ReelIn;     // the pull the helper waits for
		case EState::Grounded:
		{
			// wind the slack in; once taut, a tug (if there is air enough it lifts)
			const double R = Length(K.Pos - Hand);
			return K.Line > R + 0.3 ? -ReelIn : -0.8;
		}
		case EState::Flying:
		{
			const double El = Elevation(K, Hand);
			if (K.Def.Glide > 0.0 && K.Line >= 8.0 && Length(K.Pos - Hand) < K.Line - 0.5)
			{
				return -ReelIn;     // gliding in on a slack line: take the slack up until it bears again
			}
			if (K.Clock < 8.0 && K.Line > 12.0 && El < 30.0 * Low)
			{
				return -ReelIn;     // just let go by the helper on a long line: hand over hand until it is up
			}
			if (K.Line < 2.5 && El < -20.0 && K.Clock > 6.0)
			{
				return -ReelIn;     // it only dangles from the hand: wound up, to be tossed again
			}
			if (K.Line < 8.0)
			{
				// the launch: line out slowly while it climbs, held while it doesn't (a light-wind kite's line is paid out
				// no faster than its little air allows)
				const double Out = K.Def.Glide > 0.0 ? std::min(0.9, 0.3 * K.Air) : 0.9;
				return El > 28.0 * Low && K.Vel.Z > -0.3 ? Out : 0.0;
			}
			if (El < 22.0 * Low && K.Vel.Z < 0.0)
			{
				return -1.5;    // sagging: a pull makes its own air
			}
			if (K.Line < Want && El > 35.0 * Low)
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

	static double WrapPi(double A)
	{
		while (A > Pi) A -= 2.0 * Pi;
		while (A < -Pi) A += 2.0 * Pi;
		return A;
	}

	double AutoSteer(const FKite& K, const FVec& Hand, const FVec& Wind, double GroundZ, bool bLoops)
	{
		if (K.Def.Lines != 2)
		{
			return 0.0;
		}
		if (K.State == EState::Grounded)
		{
			return K.bNoseUp ? 0.0 : 1.0;   // one line held to roll it back onto its tail
		}
		if (K.State != EState::Flying)
		{
			return 0.0;
		}
		FVec E, U, R;
		LineFrame(K, Hand, E, U, R);
		const double Head = std::atan2(Dot(K.Nose, R), Dot(K.Nose, U));
		double Az, El;
		Window(K, Hand, Wind, Az, El);
		const double Height = K.Pos.Z - GroundZ;
		// low down (just launched, or out of a dive in good time): the nose to the sky the short way round
		if (Height < 4.5 + 0.55 * std::max(0.0, -K.Vel.Z))
		{
			return Clamp(-Head * 2.5, -1.0, 1.0);
		}
		// a loop now and then, where there is height for it: one way this half minute, back the other way the next,
		// so the lines never stay twisted (their twist is all the memory this needs)
		if (bLoops)
		{
			const double Every = 38.0 + double(K.Seed % 17);
			const int Want = (int((K.Clock + double(K.Seed % 29)) / Every) + int(K.Seed % 2)) & 1;
			const double Need = double(Want) - K.Twist;
			const bool bRoom = Height > 10.0 && El > 26.0 && std::fabs(Az) < 38.0;
			if (std::fabs(Need) > 0.3 && (bRoom || std::fabs(Need) < 0.6))
			{
				return Need > 0.0 ? 1.0 : -1.0;
			}
		}
		// the eight: toward the far end of the window, and at each end up and over
		const FVec Down = Normal(Horizontal(Wind), FVec(1, 0, 0));
		const FVec Right = Cross(Up, Down);
		const bool bGoingRight = Dot(K.Vel, Right) > 0.0;
		const bool bWantRight = Az > 40.0 ? false : (Az < -40.0 ? true : bGoingRight);
		const double Ta = (bWantRight ? 56.0 : -56.0) * Pi / 180.0, Te = 30.0 * Pi / 180.0;
		const FVec Aim = Hand + (Down * (std::cos(Ta) * std::cos(Te)) + Right * (std::sin(Ta) * std::cos(Te)) + Up * std::sin(Te)) * Length(K.Pos - Hand);
		FVec T = Aim - K.Pos;
		T -= E * Dot(T, E);
		const double Err = WrapPi(std::atan2(Dot(T, R), Dot(T, U)) - Head);
		if (std::fabs(Err) > 2.0)
		{
			return Head > 0.0 ? -1.0 : 1.0;     // the long way round goes over the top, never under
		}
		return Clamp(Err * 1.6, -1.0, 1.0);
	}

	// ---------------------------------------------------------------- tricks

	const char* TrickName(ETrick T)
	{
		switch (T)
		{
		case ETrick::High: return "high";
		case ETrick::Save: return "save";
		case ETrick::LoopLeft: return "loop_left";
		case ETrick::LoopRight: return "loop_right";
		case ETrick::Eight: return "eight";
		case ETrick::LowPass: return "low_pass";
		case ETrick::Landing: return "landing";
		case ETrick::DiveStop: return "dive_stop";
		default: return "none";
		}
	}

	static ETrick Named(FTricks& T, ETrick What, double Score)
	{
		T.Last = What;
		T.LastScore = Score;
		T.LastAt = T.Clock;
		T.Count++;
		T.Score += Score;
		T.By[int(What)]++;
		return What;
	}

	ETrick Track(FTricks& T, const FKite& K, const FKiteInput& In, double Dt)
	{
		T.Clock += Dt;
		ETrick Out = ETrick::None;
		const bool bStunt = K.Def.Lines == 2;
		const bool bFlying = K.State == EState::Flying;
		const double Height = K.Pos.Z - In.GroundZ;
		const double Speed = Length(K.Vel);
		const double WasUp = T.SinceUp;
		T.SinceUp = IsUp(K) ? T.SinceUp + Dt : 0.0;

		// set down on purpose: a soft touch at the window's edge, nose to the sky
		if (bStunt && T.bWasFlying && K.State == EState::Grounded && WasUp > 8.0)
		{
			double Az, El;
			Window(K, In.Hand, In.Wind, Az, El);
			if (K.bNoseUp && std::fabs(Az) > 30.0)
			{
				Out = Named(T, ETrick::Landing, 25.0);
			}
		}

		// steady and high
		if (!bStunt)
		{
			const bool bHigh = bFlying && K.Pos.Z - In.Hand.Z > 25.0 && Speed < 3.0;
			T.HighFor = bHigh ? T.HighFor + Dt : 0.0;
			if (T.HighFor > 20.0 && !T.bHighDone)
			{
				T.bHighDone = true;
				Out = Named(T, ETrick::High, 5.0);
			}
			if (K.Pos.Z - In.Hand.Z < 18.0)
			{
				T.bHighDone = false;
			}
		}

		// a save: out of a spin, or out of a stall near the ground
		if (K.State == EState::Spinning)
		{
			T.bWasSpin = true;
		}
		else if (T.bWasSpin)
		{
			T.bWasSpin = false;
			if (bFlying)
			{
				Out = Named(T, ETrick::Save, 10.0);
			}
		}
		if (bStunt)
		{
			// a stunt kite's stall: no air over the wing and falling, up where there is room to fall
			const bool bStall = bFlying && Height > 5.0 && K.Air < 3.5 && K.Vel.Z < -1.5;
			T.StallFor = bStall ? T.StallFor + Dt : (T.bDanger ? T.StallFor : 0.0);
			if (!T.bDanger && T.StallFor > 0.8)
			{
				T.bDanger = true;
				T.DangerAt = T.Clock;
			}
			if (T.bDanger)
			{
				if (!bFlying || T.Clock - T.DangerAt > 15.0)
				{
					T.bDanger = false;
					T.StallFor = 0.0;
				}
				else if (K.Air > 9.0 && K.Vel.Z > 0.5 && Height > 3.0)
				{
					T.bDanger = false;
					T.StallFor = 0.0;
					Out = Named(T, ETrick::Save, 10.0);
				}
			}
		}
		else
		{
			if (bFlying && !T.bDanger && WasUp > 6.0 && Height < 3.0 && K.Vel.Z < -0.8)
			{
				T.bDanger = true;
				T.DangerAt = T.Clock;
			}
			if (T.bDanger)
			{
				if (!bFlying || T.Clock - T.DangerAt > 25.0)
				{
					T.bDanger = false;
				}
				else if (Height > 7.0 && K.Vel.Z >= 0.0)
				{
					T.bDanger = false;
					Out = Named(T, ETrick::Save, 10.0);
				}
			}
		}

		if (bStunt && bFlying)
		{
			// loops: a whole turn of the nose one way, counted from where it last turned back (a quarter turn the
			// other way is a turning back; less is a wobble)
			const double Tw = K.Twist;
			// the lines let be for a second, whatever was begun is over
			T.StraightFor = std::fabs(In.Steer) < 0.3 ? T.StraightFor + Dt : 0.0;
			if (T.StraightFor > 1.0)
			{
				T.TwistFrom = T.TwistFar = Tw;
			}
			T.TwistFar = T.TurnDir > 0 ? std::max(T.TwistFar, Tw) : std::min(T.TwistFar, Tw);
			if ((T.TwistFar - Tw) * T.TurnDir > 0.25)
			{
				T.TwistFrom = T.TwistFar;
				T.TwistFar = Tw;
				T.TurnDir = -T.TurnDir;
			}
			if ((Tw - T.TwistFrom) * T.TurnDir >= 1.0)
			{
				const int Loop = T.TurnDir;
				T.TwistFrom = T.TwistFar = Tw;
				if (T.LastLoop == -Loop && T.Clock - T.LastLoopAt < 9.0)
				{
					// the other way round straight after: an eight
					T.LastLoop = 0;
					Out = Named(T, ETrick::Eight, 30.0);
				}
				else
				{
					T.LastLoop = Loop;
					T.LastLoopAt = T.Clock;
					Out = Named(T, Loop > 0 ? ETrick::LoopRight : ETrick::LoopLeft, 10.0);
				}
			}

			// a low pass: level and fast under two metres for six metres of sand
			if (Height < 2.0 && WasUp > 3.0 && Speed > 5.0 && std::fabs(K.Vel.Z) < 0.35 * Speed)
			{
				T.LowRun += Length(Horizontal(K.Pos - T.LastPos));
				if (!T.bLowDone && T.LowRun >= 6.0)
				{
					T.bLowDone = true;
					T.bDiving = false;      // a dive levelled into a pass is the pass
					Out = Named(T, ETrick::LowPass, 20.0);
				}
			}
			else
			{
				T.LowRun = 0.0;
				if (Height > 3.5)
				{
					T.bLowDone = false;
				}
			}

			// a dive stopped close to the sand
			if (!T.bDiving && K.Vel.Z < -7.0 && Height > 8.0)
			{
				T.bDiving = true;
				T.DiveTop = Height;
				T.DiveLow = Height;
			}
			if (T.bDiving)
			{
				T.DiveLow = std::min(T.DiveLow, Height);
				if (K.Vel.Z >= 0.0)
				{
					T.bDiving = false;
					if (T.DiveLow < 3.0 && T.DiveTop - T.DiveLow > 6.0)
					{
						Out = Named(T, ETrick::DiveStop, 15.0 + 5.0 * (3.0 - T.DiveLow));
					}
				}
			}
		}
		else
		{
			T.TwistFrom = T.TwistFar = K.Twist;
			T.StraightFor = 0.0;
			T.LowRun = 0.0;
			T.bLowDone = false;
			T.bDiving = false;
		}
		T.bWasFlying = bFlying;
		T.LastPos = K.Pos;
		return Out;
	}

	// ---------------------------------------------------------------- a caught kite got down

	FSnag MakeSnag(const FVec& At, uint32_t Seed)
	{
		FSnag S;
		S.At = At;
		uint32_t X = Seed * 2654435761u + 0x9E3779B9u;
		X ^= X >> 15;
		X *= 2246822519u;
		X ^= X >> 13;
		S.Hold = 0.7 + 0.8 * double(X % 1000) / 999.0;
		return S;
	}

	double SnagSide(const FSnag& S, const FVec& PullerAt, const FVec& Wind)
	{
		const FVec W = Horizontal(Wind);
		if (Length(W) < 0.8)
		{
			return 0.5;     // still air: nothing to work with or against
		}
		const FVec D = Horizontal(PullerAt - S.At);
		if (Length(D) < 0.5)
		{
			return 0.0;     // right under it: a pull straight down
		}
		return Dot(Normal(D), Normal(W));
	}

	bool Tug(FSnag& S, const FVec& PullerAt, const FVec& Wind, double Gust, double Strength)
	{
		if (S.bFree)
		{
			return false;
		}
		S.Tugs++;
		const double Side = SnagSide(S, PullerAt, Wind);
		const double Str = Clamp(Strength, 0.0, 1.0);
		if (Side < -0.25)
		{
			// from upwind: the pull drags it deeper into the twigs against the air that would lift it out
			S.Hold = std::min(2.5, S.Hold + 0.1 * Str * -Side);
			S.Wrong++;
			return false;
		}
		// from across the wind next to nothing; from downwind a tenth of an ordinary hold each time; in a gust, with the
		// air under the sail, several times that
		double Work = Str * (0.03 + 0.1 * std::max(0.0, Side));
		Work *= Gust >= 1.0 ? 1.0 + 6.0 * (Gust - 1.0) * std::max(0.0, Side) : std::max(0.5, Gust);
		S.Hold -= Work;
		if (S.Hold <= 0.0)
		{
			S.Hold = 0.0;
			S.bFree = true;
			return true;
		}
		return false;
	}
}
