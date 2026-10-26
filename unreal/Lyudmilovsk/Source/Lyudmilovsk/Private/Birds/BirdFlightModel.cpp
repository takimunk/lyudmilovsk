#include "BirdFlightModel.h"

#include "BirdBio.h"

#include <algorithm>

namespace BirdFlight
{
	namespace
	{
		double Clamp(double V, double Lo, double Hi) { return std::min(Hi, std::max(Lo, V)); }

		/** Per style: glide ratio, best glide / cruise, turn rate (rad/s), max load (g), body lag (s),
		 *  zero-speed lift of steady flapping (g), the auto pattern's clicks and the glide between them (s). */
		struct FStyleRow { double Glide, BestOverCruise, Turn, Load, Follow, HoverG; int Clicks; double Gap; bool GapTucked; };
		const FStyleRow& Row(EStyle S)
		{
			static const FStyleRow Rows[] = {
				/* Flapping */ { 7.0, 0.80, 1.7, 3.8, 0.30, 1.15, 1, 0.0, false },
				/* Bounding */ { 5.0, 0.80, 2.8, 4.0, 0.18, 1.10, 3, 0.35, true },
				/* Undulating */ { 8.0, 0.78, 2.2, 3.8, 0.25, 0.95, 4, 0.9, false },
				/* Clapping */ { 7.0, 0.80, 1.7, 3.8, 0.30, 1.20, 1, 0.0, false },
				/* Soaring  */ { 14.0, 0.70, 1.1, 3.0, 0.50, 0.95, 3, 1.6, false },
				/* Flicker  */ { 11.0, 0.72, 3.6, 5.0, 0.12, 1.05, 3, 0.5, false },
			};
			return Rows[(int)S];
		}

		double ClicksPerSecond(const FParams& P)
		{
			const FStyleRow& R = Row(P.Style);
			const double Burst = R.Clicks * P.ClickTime();
			return R.Clicks / (Burst + R.Gap);
		}
	}

	namespace
	{
		/** Scales the wingbeat thrust until holding Space flies level at the table's cruise speed. */
		void FitCruise(FParams& P)
		{
			// (fitted rested: the tuning flight must not tire)
			// the estimate above ignores the bounding finches' closed-wing dips and the rest of the pattern:
			// fly the pattern level for a while and scale the thrust until it settles at the table's cruise speed
			const double KeepStamina = P.Stamina;
			P.Stamina = 1e9;
			for (int K = 0; K < 4; K++)
			{
				FState S;
				S.Pos = V3(0, 0, 100);
				S.Vel = V3(P.Cruise, 0, 0);
				double Clock = 0, Sum = 0;
				int N = 0;
				const double Dt = 1.0 / 60.0;
				for (double T = 0; T < 16.0; T += Dt)
				{
					FInput In;
					double Tuck = 0;
					In.ClickL = In.ClickR = AutoFlap(P, Dt, Clock, Tuck);
					In.TuckL = In.TuckR = Tuck;
					In.AimPitch = Clamp((100.0 - S.Pos.Z) * 0.05, -0.3, 0.3);
					Step(P, In, FAir(), S, Dt);
					if (T > 8.0) { Sum += S.Vel.Size(); N++; }
				}
				const double Got = Sum / std::max(N, 1);
				P.ClickThrust *= Clamp(std::pow(P.Cruise / std::max(Got, 1.0), 2.0), 0.5, 2.0);
			}
			P.Stamina = KeepStamina;
		}

		/** Seconds holding Space until the bird is spent, from rested. */
		double TimeToSpent(const FParams& P)
		{
			FState S;
			S.Pos = V3(0, 0, 100);
			S.Vel = V3(P.Cruise, 0, 0);
			double Clock = 0;
			const double Dt = 1.0 / 30.0;
			for (double T = 0; T < 3600.0; T += Dt)
			{
				FInput In;
				double Tuck = 0;
				In.ClickL = In.ClickR = AutoFlap(P, Dt, Clock, Tuck);
				In.TuckL = In.TuckR = Tuck;
				In.AimPitch = Clamp((100.0 - S.Pos.Z) * 0.05, -0.3, 0.3);
				Step(P, In, FAir(), S, Dt);
				if (S.Energy <= 0.01) return T;
			}
			return 3600.0;
		}
	}

	double GlideSink(const FParams& P, double V)
	{
		// drag over weight at load 1 on the polar, times speed
		const double Vr = std::max(V / P.BestGlide, 0.35);
		const double DoverW = (Vr * Vr + 1.0 / (Vr * Vr)) / (2.0 * P.GlideRatio) + std::pow(V / P.MaxSpeed, 10.0);
		return V * DoverW;
	}

	FParams MakeParams(EStyle Style, double MassG, double SpanCm, double BeatHz, double CruiseMs)
	{
		const FStyleRow& R = Row(Style);
		FParams P;
		P.Style = Style;
		P.Mass = std::max(MassG, 5.0) / 1000.0;
		P.Span = std::max(SpanCm, 10.0) / 100.0;
		P.BeatHz = Clamp(BeatHz, 1.5, 30.0);
		P.Cruise = Clamp(CruiseMs, 4.0, 40.0);
		P.GlideRatio = R.Glide;
		P.BestGlide = P.Cruise * R.BestOverCruise;
		P.Stall = P.BestGlide * 0.45;
		P.MaxSpeed = P.Cruise * 2.3;
		// small birds turn tighter
		P.TurnRate = R.Turn * Clamp(std::pow(0.075 / P.Mass, 0.12), 0.75, 1.3);
		P.MaxLoad = R.Load;
		P.Follow = R.Follow;
		// a click is a short burst at the species' own rhythm: one beat for a gull, four for a sparrow
		P.BeatsPerClick = std::max(1, (int)std::lround(P.BeatHz / 5.0));
		// holding Space (the species' pattern) holds Cruise in level flight, with a margin to climb
		const double Vr = P.Cruise / P.BestGlide;
		const double Drag = P.Gravity * (Vr * Vr + 1.0 / (Vr * Vr)) / (2.0 * P.GlideRatio);
		const double Cps = ClicksPerSecond(P);
		P.ClickThrust = Drag / Cps * 1.05;
		// clicking without pause at zero airspeed lifts HoverG (a take-off; pigeons and finches can hover a moment)
		P.ClickLift = P.Gravity * R.HoverG * P.ClickTime();
		P.OarKick = Style == EStyle::Flicker ? 0.45 : Style == EStyle::Soaring ? 0.22 : 0.35;
		FitCruise(P);
		P.Hops = Style == EStyle::Bounding || Style == EStyle::Undulating;
		return P;
	}

	FParams MakeSpeciesParams(const char* Id, EStyle Style, double MassG, double SpanCm, double BeatHz, double CruiseMs)
	{
		const BirdBio::FBio& B = BirdBio::Get(Id);
		FParams P = MakeParams(Style, MassG, SpanCm, BeatHz, B.CruiseMs > 0 ? B.CruiseMs : CruiseMs);
		P.Stamina = B.StaminaS;
		P.MaxSpeed = std::max(B.DiveMs, P.Cruise * 1.3);
		P.TurnRate *= B.Agility;
		P.OarKick *= B.Agility;
		P.Follow /= std::max(B.Agility, 0.3);
		FitCruise(P);
		// the biology's seconds are of the species' own flight (a finch's bursts and dips): fit the drain to them
		if (P.Stamina < 1000.0)
		{
			const double Got = TimeToSpent(P);
			P.Stamina *= Clamp(B.StaminaS / std::max(Got, 1.0), 0.2, 5.0);
		}
		return P;
	}

	bool AutoFlap(const FParams& P, double Dt, double& Clock, double& Tuck)
	{
		const FStyleRow& R = Row(P.Style);
		const double Burst = R.Clicks * P.ClickTime();
		const double Cycle = Burst + R.Gap;
		const double Before = Clock;
		Clock += Dt;
		const double T0 = std::fmod(Before, Cycle), T1 = std::fmod(Clock, Cycle);
		Tuck = (R.GapTucked && T1 > Burst) ? 1.0 : 0.0;
		// a click at the start of each beat of the burst
		for (int K = 0; K < R.Clicks; K++)
		{
			const double At = K * P.ClickTime();
			const bool Crossed = (T0 <= T1) ? (Before == 0.0 ? At <= T1 : (T0 < At && At <= T1)) : (T0 < At || At <= T1);
			if (Crossed)
			{
				return true;
			}
		}
		return false;
	}

	void Step(const FParams& P, const FInput& In, const FAir& Air, FState& S, double Dt)
	{
		if (Dt <= 0)
		{
			return;
		}
		const double G = P.Gravity;
		const double Click = P.ClickTime();

		// wings: a click starts a burst, or waits for the one under way to pass its middle
		auto Wing = [&](bool Clicked, double& Burst, bool& Queued, double& Phase, double& Spread, double Tuck)
		{
			if (Clicked)
			{
				if (Burst <= 0) Burst = Click;
				else if (Burst < Click * 0.5) Queued = true;
			}
			const bool Beating = Burst > 0;
			if (Beating || Phase > 0)
			{
				Phase += P.BeatHz * Dt;
				if (!Beating && Phase >= 1) Phase = 0;
				else Phase = std::fmod(Phase, 1.0);
			}
			if (Beating)
			{
				Burst -= Dt;
				if (Burst <= 0)
				{
					Burst = Queued ? Click + Burst : 0;
					Queued = false;
				}
			}
			const double Target = Beating ? 1.0 : 1.0 - Clamp(Tuck, 0, 1) * 0.9;
			const double Rate = Target < Spread ? 12.0 : 7.0;
			Spread += (Target - Spread) * std::min(1.0, Rate * Dt);
			return Beating;
		};
		const bool BeatL = Wing(In.ClickL, S.BurstL, S.QueuedL, S.PhaseL, S.SpreadL, In.TuckL);
		const bool BeatR = Wing(In.ClickR, S.BurstR, S.QueuedR, S.PhaseR, S.SpreadR, In.TuckR);
		// rowing with one oar: the beating wing pushes its side forward, the course turns away from it
		if (BeatL != BeatR && (In.ClickL || In.ClickR))
		{
			S.YawKick += BeatL ? P.OarKick : -P.OarKick;
		}
		const double KickRate = Clamp(S.YawKick / 0.15, -P.TurnRate * 2.5, P.TurnRate * 2.5);
		S.YawKick -= KickRate * Dt;

		const double V = S.Vel.Size();
		const double Spread = 0.5 * (S.SpreadL + S.SpreadR);
		const double Flare = Clamp(In.Flare, 0, 1);
		V3 D = V > 0.5 ? S.Vel * (1.0 / V) : V3(std::cos(In.AimYaw), std::sin(In.AimYaw), 0);
		const double Gamma = std::asin(Clamp(D.Z, -1, 1));
		const double Psi = std::atan2(D.Y, D.X);
		const V3 Right(-std::sin(Psi), std::cos(Psi), 0);
		const V3 UpN(-std::sin(Gamma) * std::cos(Psi), -std::sin(Gamma) * std::sin(Psi), std::cos(Gamma));

		// the body chases the gaze; a pulled-in wing, the tail and an oar beat add their turn
		const double K = 1.0 / P.Follow;
		double Omega = Clamp(Wrap(In.AimYaw - Psi) * K, -P.TurnRate, P.TurnRate);
		Omega += (S.SpreadR - S.SpreadL) * -P.TurnRate * 0.9 + Clamp(In.Roll, -1, 1) * P.TurnRate * 0.45 + KickRate;
		const double GammaDot = Clamp((In.AimPitch - Gamma) * K, -2.5, 2.5);

		// wingbeats: thrust along the path, and lift that matters most when slow (take-off, hover)
		const V3 ThrustDir = V > 0.5 ? D : V3(std::cos(In.AimYaw) * 0.87, std::sin(In.AimYaw) * 0.87, 0.5);
		const double SlowLift = 1.0 - 0.7 * std::min(V / P.Stall, 1.0);
		const int Beats = (BeatL ? 1 : 0) + (BeatR ? 1 : 0);
		// the flight muscles give power, not force: slow, the same beats drive the bird forward far harder
		// (a take-off reaches flying speed in a second or two)
		const double Launch = Clamp(std::pow(P.Cruise / std::max(V, 0.5 * P.Stall), 1.25), 1.0, 5.0);
		// the flight muscles tire: every beat spends, gliding rests them (a spent bird's beats are weak)
		const double Effort = 0.5 * Beats * (1.0 + 0.6 * SlowLift);
		if (Beats > 0) S.Energy -= Dt * Effort / std::max(P.Stamina, 1.0);
		else S.Energy += Dt * (0.25 + 0.5 * (1.0 - Spread)) / std::max(P.Stamina, 1.0) * 1.5;
		S.Energy = Clamp(S.Energy, 0.0, 1.0);
		// tiring shows long before the end: the beats weaken from a third of the strength left, and a spent bird
		// can no longer hold its height
		const double E = Clamp(S.Energy / 0.35, 0.0, 1.0);
		const double Strength = 0.1 + 0.9 * E * E * (3.0 - 2.0 * E);
		double ThrustAcc = 0.5 * Beats * P.ClickThrust / Click * Launch * Strength;
		double BeatLift = 0.5 * Beats * P.ClickLift / Click * SlowLift * Strength;
		// the beats lift no more than the course needs (and a little to climb); the rest drives the bird on
		const double Need = std::max(0.0, V * GammaDot + G * std::cos(Gamma));
		if (BeatLift > Need + 0.3 * G)
		{
			ThrustAcc += (BeatLift - Need - 0.3 * G) * 0.6;
			BeatLift = Need + 0.3 * G;
		}
		const V3 Thrust = ThrustDir * ThrustAcc;

		// lift: what the course needs, less what the beats already give, up to what the wings can make
		double NVert = V * GammaDot + G * std::cos(Gamma) - BeatLift * std::cos(Gamma);
		double NLat = V * std::cos(Gamma) * Omega;
		const double LiftK = std::pow(std::max(Spread, 0.0), 1.5) * (1.0 + 0.6 * Flare);
		const double LMax = std::min(P.MaxLoad * G, G * (V / P.Stall) * (V / P.Stall) * LiftK);
		NVert = std::max(NVert, -0.5 * LMax);
		const double N = std::sqrt(NVert * NVert + NLat * NLat);
		if (N > LMax && N > 0)
		{
			NVert *= LMax / N;
			NLat *= LMax / N;
		}
		const double BankTarget = std::atan2(NLat, std::max(NVert, 0.2 * G)) + (S.SpreadR - S.SpreadL) * -0.5 + In.Roll * 0.25;
		S.Bank += (Clamp(BankTarget, -1.4, 1.4) - S.Bank) * std::min(1.0, 8.0 * Dt);
		S.Load = std::sqrt(NVert * NVert + NLat * NLat) / G;
		V3 Acc = UpN * NVert + Right * NLat + Thrust + V3(0, 0, BeatLift);

		// drag on the glide polar (parasite ~V^2, induced ~load^2/V^2), the air brake, the dive limit
		const double Q = G / (2.0 * P.GlideRatio);
		const double Vr = V / P.BestGlide;
		const double Dp = Q * Vr * Vr * (0.3 + 0.7 * Spread) * (1.0 + 14.0 * Flare);
		const double Di = std::min(1.5 * G, Q * S.Load * S.Load / std::max(Vr * Vr, 0.25));
		const double Dhi = G * std::pow(V / P.MaxSpeed, 10.0);
		Acc += D * -(Dp + Di + Dhi);

		Acc.Z -= G;
		S.Vel += Acc * Dt;
		S.Ground = S.Vel + V3(Air.Wind.X, Air.Wind.Y, 0) + V3(0, 0, Air.Updraft);
		S.Pos += S.Ground * Dt;
	}
}
