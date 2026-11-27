#include "SailModel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace LyudSail
{
	namespace
	{
		constexpr double Deg = Pi / 180.0;
		constexpr double MaxSub = 1.0 / 60.0;          // the model's longest inner step
		constexpr double RudderMax = 45.0 * Deg;       // blade angle at Rudder = 1
		constexpr double TrimAlpha = 18.0 * Deg;       // the angle of attack a well trimmed sail sits at
		constexpr double StallAlpha = 21.0 * Deg;      // where the flow starts to leave the sail's lee side
		constexpr double ClMaxMain = 1.35;
		constexpr double CapsizeHeel = 70.0 * Deg;
		constexpr double FormVanish = 80.0 * Deg;      // the hull's own righting moment is gone here
		constexpr double LeeGybe = 24.0 * Deg;         // by the lee by this much and the boom comes over
		constexpr double AutoHeelMax = 0.24;           // the auto-sheet starts to ease over this heel

		inline double Clamp(double V, double A, double B) { return V < A ? A : (V > B ? B : V); }
		inline double Smooth(double A, double B, double X)
		{
			const double T = Clamp((X - A) / (B - A), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
		inline double Toward(double Cur, double Target, double MaxStep)
		{
			const double D = Target - Cur;
			return std::fabs(D) <= MaxStep ? Target : Cur + (D > 0.0 ? MaxStep : -MaxStep);
		}
		inline double Sign(double V) { return V < 0.0 ? -1.0 : 1.0; }
		inline double Rand01(uint32_t& S)
		{
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			return double(S & 0xFFFFFFu) / double(0x1000000u);
		}

		const FBoatDef Defs[BoatCount] = {
			// «Оптимист»: a pram with a sprit sail, a daggerboard, a child
			{ "optimist", 2.30, 2.15, 1.13, 3.3, 0.0, 35.0, 1, 35.0, 2.00, 1.35, 0.62, 15.0 * Deg, 88.0 * Deg, 0.0, 0.0,
			  0.22, 0.70, 0.085, 1.05, 0.12, 0.20, 2.1, 0.40, 0.30, 0.13, 0.16, 0.45, 30.0 * Deg, 110.0, 0.75, 0.0, 14.0, 2.35 },
			// «Кадет»: two children, mainsail and jib, a pivoting centreboard, shrouds stop the boom
			{ "cadet", 3.22, 3.00, 1.27, 3.9, 1.3, 54.0, 2, 90.0, 1.95, 1.85, 0.35, 12.0 * Deg, 75.0 * Deg, 14.0 * Deg, 60.0 * Deg,
			  0.26, 0.80, 0.10, 1.45, 0.14, 0.30, 3.0, 0.60, 0.75, 0.11, 0.14, 0.62, 32.0 * Deg, 150.0, 0.5, 0.02, 16.0, 4.9 },
			// «Луч»: the single-hander, an unstayed mast, a self-draining cockpit
			{ "luch", 4.23, 3.85, 1.37, 7.06, 0.0, 59.0, 1, 75.0, 2.75, 2.30, 0.85, 15.0 * Deg, 90.0 * Deg, 0.0, 0.0,
			  0.33, 0.85, 0.13, 1.90, 0.15, 0.40, 4.0, 0.60, 1.00, 0.10, 0.13, 0.95, 28.0 * Deg, 45.0, 0.25, 0.05, 11.0, 6.1 },
		};

		struct FAir
		{
			double Ax = 0.0, Ay = 0.0;      // apparent wind in the boat's frame (forward, starboard), blows toward
			double Va = 0.0;
			double Beta = 0.0;              // where it comes from, + starboard
			double Free = 0.0;              // the boom angle a free sail would weathercock to
		};

		FAir Apparent(const FBoat& B, const FEnv& E)
		{
			FAir A;
			const double C = std::cos(B.Yaw), S = std::sin(B.Yaw);
			const double Rx = E.WindX - B.VX, Ry = E.WindY - B.VY;
			A.Ax = Rx * C + Ry * S;
			A.Ay = -Rx * S + Ry * C;
			A.Va = std::sqrt(A.Ax * A.Ax + A.Ay * A.Ay);
			if (A.Va > 0.05)
			{
				A.Beta = std::atan2(-A.Ay, -A.Ax);
				A.Free = std::atan2(A.Ay, -A.Ax);
			}
			else
			{
				A.Beta = -B.Boom;
				A.Free = B.Boom;
			}
			return A;
		}

		/** The boom angle that trims the mainsail for an apparent wind angle (unsigned). */
		inline double IdealBoom(const FBoatDef& D, double AbsBeta)
		{
			return Clamp(AbsBeta - TrimAlpha, D.BoomMin, D.BoomMax);
		}
		inline double SheetForBoom(const FBoatDef& D, double BoomAbs)
		{
			return Clamp((BoomAbs - D.BoomMin) / (D.BoomMax - D.BoomMin), 0.0, 1.0);
		}
		inline double BoomLimit(const FBoatDef& D, double Sheet) { return D.BoomMin + (D.BoomMax - D.BoomMin) * Sheet; }

		/** How full the cloth is at an angle of attack: under a few degrees it only flaps. */
		inline double SailFill(double Alpha) { return Smooth(8.0 * Deg, 16.0 * Deg, Alpha); }

		/** The share of a trimmed sail's pull that is left when it is eased to a smaller angle of attack (half luffing). */
		inline double EasedPull(double Alpha)
		{
			if (Alpha >= TrimAlpha)
			{
				return 1.0;
			}
			return SailFill(Alpha) * std::sin(0.5 * Pi * Alpha / StallAlpha) / std::sin(0.5 * Pi * TrimAlpha / StallAlpha);
		}
		/** The angle of attack that leaves this share of the pull. */
		double AlphaForPull(double Share)
		{
			if (Share >= 1.0)
			{
				return TrimAlpha;
			}
			double Lo = 8.0 * Deg, Hi = TrimAlpha;
			for (int I = 0; I < 12; I++)
			{
				const double Mid = 0.5 * (Lo + Hi);
				(EasedPull(Mid) < Share ? Lo : Hi) = Mid;
			}
			return 0.5 * (Lo + Hi);
		}
		/**
		 * The hand on the sheet. Push = how hard the sail heels her now as a share of what the crew's weight can hold,
		 * Full = (state) what it would be with the sail trimmed full. Returns the boom angle that leaves Target of it.
		 */
		double TrimBoom(const FBoatDef& D, double AbsBeta, double AbsBoom, double Push, double Target, double& Full, double Blend)
		{
			const double Now = std::max(EasedPull(Clamp(AbsBeta - AbsBoom, 0.0, Pi)), 0.12);
			Full += (std::min(Push / Now, 6.0) - Full) * Blend;
			const double Alpha = AlphaForPull(Target / std::max(Full, 1e-3));
			return Clamp(AbsBeta - Alpha, D.BoomMin, D.BoomMax);
		}

		/** Lift and drag of a soft sail at an angle of attack 0..pi. Fill = how full the cloth is, Attach = how much attached flow. */
		void SailCoef(double Alpha, double ClMax, double& Cl, double& Cd, double& Fill, double& Attach)
		{
			Fill = SailFill(Alpha);
			const double ClA = ClMax * std::sin(0.5 * Pi * std::min(1.0, Alpha / StallAlpha));
			const double CdA = 0.05 + 0.16 * ClA * ClA;
			const double Sa = std::sin(Alpha), Ca = std::cos(Alpha);
			const double Cn = 1.15 * Sa / (0.5 + 0.5 * Sa);
			const double T = Smooth(StallAlpha, StallAlpha + 16.0 * Deg, Alpha);
			Attach = Fill * (1.0 - T);
			Cl = (ClA * (1.0 - T) + Cn * Ca * T) * Fill;
			// a flapping sail still drags
			Cd = 0.06 * (1.0 - Fill) + (CdA * (1.0 - T) + (Cn * Sa + 0.03) * T) * Fill;
		}

		/**
		 * A foil in the water (centreboard, rudder). (Ul, Vl) = its velocity through the water in the boat's frame,
		 * Turn = how far its leading edge is turned to port (the rudder blade for "turn to starboard").
		 */
		void Foil(double Ul, double Vl, double Turn, double Area, double Aspect, double& Fx, double& Fy)
		{
			Fx = Fy = 0.0;
			const double S2 = Ul * Ul + Vl * Vl;
			if (S2 < 1e-8 || Area <= 0.0)
			{
				return;
			}
			const double S = std::sqrt(S2);
			const double Alpha = std::atan2(Vl, Ul) + Turn;
			const double Sa = std::sin(Alpha), Ca = std::cos(Alpha);
			const double Slope = 2.0 * Pi * Aspect / (Aspect + 2.0);
			const double Lin = Slope * Sa * Ca;
			const double ClMax = Ca >= 0.0 ? 1.05 : 0.6;        // going astern a foil works badly
			const double Cl = Lin / std::sqrt(1.0 + (Lin / ClMax) * (Lin / ClMax));
			const double Cd = 0.012 + Cl * Cl / (Pi * Aspect * 0.85) + 0.7 * Sa * Sa * Sa * Sa;
			const double Q = 0.5 * WaterDensity * Area * S2;
			// lift to port of the motion for a positive angle, drag against the motion
			Fx = Q * (Cl * Vl / S - Cd * Ul / S);
			Fy = Q * (-Cl * Ul / S - Cd * Vl / S);
		}

		inline double TotalMass(const FBoat& B, const FBoatDef& D) { return D.HullMass + B.CrewMass + B.Water * D.WaterCap; }

		/** The crew's weight where an attentive crew puts it: on the high side as soon as she heels. */
		inline double AutoHikeTarget(const FBoat& B, const FBoatDef& D)
		{
			// against the sail's push as it is felt, and a little for the heel that is left
			const double Need = B.HeelMoment / (B.CrewMass * Gravity * D.HikeArm);
			return -Clamp(Need + (B.Heel - B.WaveRoll) / 1.5, -1.0, 1.0);
		}
		/** How much more the sail heels her than the crew's whole weight on the rail can hold (1 = just held). */
		inline double PowerRatio(const FBoat& B, const FBoatDef& D)
		{
			return std::fabs(B.HeelMoment) / (B.CrewMass * Gravity * D.HikeArm);
		}

		void SetState(FBoat& B, EState S)
		{
			if (B.State != S)
			{
				B.State = S;
				B.StateTime = 0.0;
			}
		}

		/** Moves the tiller, sheet, crew and board toward what is asked, at human speeds. */
		void MoveGear(FBoat& B, const FBoatDef& D, const FControls& C, const FAir& A, double H)
		{
			const bool bAfloat = B.State == EState::Sailing || B.State == EState::Swamped;
			double SheetT = Clamp(C.Sheet, 0.0, 1.0), HikeT = Clamp(C.Hike, -1.0, 1.0);
			// the heel as a crew answers it with their weight: the lean of the last second, not every roll
			B.HeelSlow += (B.Heel - B.WaveRoll - B.HeelSlow) * std::min(1.0, H / 1.0);
			if (bAfloat && B.Assist.bAutoSheet)
			{
				const double AbsBeta = std::fabs(A.Beta);
				// eased when she lies over to leeward, or when the sail pulls harder than the crew can hold down
				const double Over = (B.Heel - B.WaveRoll) * Sign(B.HeelMoment) - AutoHeelMax;
				const double Power = PowerRatio(B, D);
				B.Depower = Clamp(B.Depower + (Over > 0.0 ? 2.0 * Over + 0.1 : (Over < -0.06 ? -0.2 : 0.0)) * H, 0.0, 1.0);
				// the tiller hard over to bear away and she will not go: ease, and she will
				const double BearAway = Smooth(0.5, 0.95, std::max(0.0, -B.Rudder * Sign(A.Beta)));
				// carry what the crew's weight, where it is now, can hold down (the hull's own stiffness holds a little)
				const double Held = B.Assist.bAutoHike ? 1.0 : Clamp(-B.Hike * Sign(B.HeelMoment), 0.0, 1.0);
				const double Target = (0.2 + 0.95 * Held) * (1.0 - 0.8 * B.Depower) * (1.0 - 0.75 * BearAway);
				SheetT = SheetForBoom(D, TrimBoom(D, AbsBeta, std::fabs(B.Boom), Power, Target, B.PowerFull, std::min(1.0, H / 0.25)));
				// no way on and the bow near the wind: a full sail would only screw her up into it. Let it fly until she has paid off
				const double Fwd = (B.VX * std::cos(B.Yaw) + B.VY * std::sin(B.Yaw));
				// (going astern she must come right round to beam-on first: sheeted in sooner she luffs before she moves)
				if ((Fwd < 0.25 && AbsBeta < 60.0 * Deg) || (Fwd < -0.2 && AbsBeta < 92.0 * Deg))
				{
					SheetT = 1.0;
				}
			}
			if (bAfloat && B.Assist.bAutoHike)
			{
				HikeT = AutoHikeTarget(B, D);
			}
			if (!bAfloat)
			{
				// nobody trims a boat on its side or on the sand
				SheetT = 1.0;
				HikeT = 0.0;
			}
			else if (C.bBail && B.Water > 0.02)
			{
				HikeT = Clamp(HikeT, -0.35, 0.35);   // bailing, the crew sits in
			}
			const double Quick = 0.6 + 0.4 * B.CrewSkill;
			B.Rudder = Toward(B.Rudder, Clamp(C.Rudder, -1.0, 1.0), 3.5 * H);
			B.Sheet = Toward(B.Sheet, SheetT, (SheetT < B.Sheet ? 0.8 * Quick : 2.5) * H);
			const double JibT = C.JibSheet >= 0.0 && bAfloat ? Clamp(C.JibSheet, 0.0, 1.0) : SheetT;
			B.JibSheet = Toward(B.JibSheet, JibT, (JibT < B.JibSheet ? 0.9 * Quick : 2.5) * H);
			B.Hike = Toward(B.Hike, HikeT, 1.6 * Quick * H);
			B.Board = Toward(B.Board, Clamp(C.Board, 0.0, 1.0), 0.8 * H);
		}

		/** The boom of a boat that is not sailing swings to the wind inside its sheet. */
		void FlagSails(FBoat& B, const FBoatDef& D, const FAir& A, double H)
		{
			const double Lim = BoomLimit(D, B.Sheet);
			const double T = Clamp(A.Free, -Lim, Lim);
			B.Boom += (T - B.Boom) * std::min(1.0, H * (0.5 + 0.4 * A.Va));
			B.BoomRate = 0.0;
			B.Jib += (Clamp(A.Free, -D.JibMax, D.JibMax) - B.Jib) * std::min(1.0, H * (0.5 + 0.4 * A.Va));
		}

		void FromFar(FBoat& B, const FBoatDef& D, const FEnv& E)
		{
			B.bFar = false;
			const double C = std::cos(B.Yaw), S = std::sin(B.Yaw);
			B.VX = C * B.FarSpeed + E.CurrentX;
			B.VY = S * B.FarSpeed + E.CurrentY;
			B.YawRate = 0.0;
			B.HeelRate = 0.0;
			B.BoomRate = 0.0;
			B.Jib = Clamp(B.Boom, -D.JibMax, D.JibMax);
			if (B.State == EState::Sailing || B.State == EState::Swamped)
			{
				B.Sheet = SheetForBoom(D, std::fabs(B.Boom));
				B.JibSheet = B.Sheet;
				B.Hike = -Clamp(B.Heel / 0.09, -1.0, 1.0);
				B.BoomSide = B.Boom >= 0.0 ? 1 : -1;
			}
		}

		// ---- the full model: one inner step afloat ---------------------------------------------------------------------
		void SailStep(FBoat& B, const FBoatDef& D, const FEnv& E, const FAir& A, double H)
		{
			const double Cy = std::cos(B.Yaw), Sy = std::sin(B.Yaw);
			const double Wx = B.VX - E.CurrentX, Wy = B.VY - E.CurrentY;
			const double U = Wx * Cy + Wy * Sy, V = -Wx * Sy + Wy * Cy;
			const double R = B.YawRate;
			const double Mass = TotalMass(B, D);
			const double Weight = Mass * Gravity;
			const double Ch = std::cos(B.Heel), Sh = std::sin(B.Heel);
			const double Spill = Ch * Ch;               // a heeled sail shows less of itself to the wind
			const double Q = 0.5 * AirDensity * A.Va * A.Va;
			const double Fxn = A.Va > 1e-6 ? A.Ax / A.Va : -1.0, Fyn = A.Va > 1e-6 ? A.Ay / A.Va : 0.0;

			double Fx = 0.0, Fy = 0.0, Nz = 0.0, Mh = 0.0;

			// ---- mainsail: the boom, then the force
			const double Lim = BoomLimit(D, B.Sheet);
			double Theta = WrapAngle(A.Free - B.Boom);          // from the boom to where the wind would carry it
			double Alpha = std::fabs(Theta);
			const bool bSameSide = A.Beta * B.Boom > 0.0 && std::fabs(B.Boom) > 0.15 && std::fabs(A.Beta) > 0.5 * Pi;
			const double Lee = bSameSide ? Pi - std::fabs(A.Beta) : 0.0;
			// rolled to windward the leech folds sooner
			const double Windward = bSameSide ? std::max(0.0, B.Heel * Sign(A.Beta)) : 0.0;
			const bool bFlip = Lee > LeeGybe - 0.5 * Windward;
			{
				double Dir = Theta >= 0.0 ? 1.0 : -1.0;
				if (bFlip)
				{
					Dir = -Sign(B.Boom);
				}
				const double Push = bFlip ? 1.0 : Clamp(2.0 * Alpha, 0.0, 1.0);
				double RateT = Dir * (1.4 / D.BoomLen) * A.Va * Push;
				RateT += 1.5 * Sh * (A.Va < 3.0 ? 1.0 : 3.0 / A.Va);    // in light air the boom falls to the low side
				B.BoomRate += (RateT - B.BoomRate) * std::min(1.0, H / 0.12);
				double NewBoom = B.Boom + B.BoomRate * H;
				if (std::fabs(NewBoom) > Lim)
				{
					const double Out = B.BoomRate * Sign(NewBoom);
					if (Out > 1.2)
					{
						B.Events |= EvBoomSlam;
						B.Out.BoomSlamSpeed = std::max(B.Out.BoomSlamSpeed, Out);
						// the slam jerks the boat over
						B.HeelRate += Sign(NewBoom) * std::min(Out, 6.0) * 0.05;
					}
					NewBoom = Sign(NewBoom) * Lim;
					B.BoomRate = 0.0;
				}
				B.Boom = NewBoom;
				Theta = WrapAngle(A.Free - B.Boom);
				Alpha = std::fabs(Theta);
			}
			double Cl, Cd, Fill, Attach;
			SailCoef(Alpha, ClMaxMain, Cl, Cd, Fill, Attach);
			// hardly any air: the cloth hangs and does nothing
			const double Breath = Smooth(0.3, 1.2, A.Va);
			Fill *= Breath;
			const double LiftSide = Theta >= 0.0 ? 1.0 : -1.0;
			const double MainL = Q * D.MainArea * Cl * Spill * Breath, MainD = Q * D.MainArea * Cd * (Fill * Spill + (1.0 - Fill)) * Breath;
			const double MainFx = MainL * LiftSide * Fyn + MainD * Fxn;
			const double MainFy = -MainL * LiftSide * Fxn + MainD * Fyn;
			const double BoardDeep = 0.5 * D.HullDraft + 0.5 * B.Board * D.BoardLen;
			{
				// (a flapping sail drags from nearer the mast than a full one pulls)
				const double Xs = D.MastX - (0.25 + 0.17 * Fill) * D.BoomLen * std::cos(B.Boom);
				const double Ys = 0.42 * D.BoomLen * std::sin(B.Boom) + D.CeHeight * Sh;
				Fx += MainFx;
				Fy += MainFy;
				Nz += Xs * MainFy - Ys * MainFx;
				Mh += MainFy * (D.CeHeight + BoardDeep);
			}
			B.Out.SheetLoad = 0.45 * std::sqrt(MainL * MainL + MainD * MainD) * Fill;
			// the sail fills on a new side: a tack or a gybe is done
			if (Fill > 0.6 && std::fabs(B.Boom) > 0.04)
			{
				const int Side = B.Boom > 0.0 ? 1 : -1;
				if (B.BoomSide != 0 && Side != B.BoomSide)
				{
					B.Events |= std::fabs(A.Beta) > 0.5 * Pi ? EvGybe : EvTack;
				}
				B.BoomSide = Side;
			}
			const bool bLuff = Fill < 0.5;
			if (bLuff && !B.bLuffing)
			{
				B.Events |= EvLuff;
			}
			B.bLuffing = bLuff;
			B.Out.MainLuff = 1.0 - Fill;

			// ---- jib
			double JibAttach = 0.0;
			if (D.JibArea > 0.0)
			{
				const double JLim = D.JibMin + (D.JibMax - D.JibMin) * B.JibSheet;
				const double JT = Clamp(A.Free, -JLim, JLim);
				B.Jib += (JT - B.Jib) * std::min(1.0, H * (1.0 + 0.8 * A.Va));
				const double JTheta = WrapAngle(A.Free - B.Jib);
				double JCl, JCd, JFill;
				SailCoef(std::fabs(JTheta), 1.25, JCl, JCd, JFill, JibAttach);
				JFill *= Breath;
				// behind the mainsail on a run it hangs in its lee
				const double Blanket = 1.0 - 0.65 * Smooth(135.0 * Deg, 160.0 * Deg, std::fabs(A.Beta));
				const double JS = JTheta >= 0.0 ? 1.0 : -1.0;
				const double JL = Q * D.JibArea * JCl * Spill * Blanket * Breath, JD = Q * D.JibArea * JCd * Blanket * Breath;
				const double JFx = JL * JS * Fyn + JD * Fxn, JFy = -JL * JS * Fxn + JD * Fyn;
				Fx += JFx;
				Fy += JFy;
				Nz += (D.MastX + 0.40) * JFy - 0.8 * D.CeHeight * Sh * JFx;
				Mh += JFy * (0.75 * D.CeHeight + BoardDeep);
				B.Out.JibLuff = 1.0 - JFill;
			}
			else
			{
				B.Jib = B.Boom;
				B.Out.JibLuff = B.Out.MainLuff;
			}

			// ---- hull, spars and crew in the wind: a drag, centred ahead of the board so a stopped boat's bow blows off
			{
				const double Wd = 0.5 * AirDensity * D.Windage * A.Va;
				const double WFx = Wd * A.Ax, WFy = Wd * A.Ay;
				Fx += WFx;
				Fy += WFy;
				Nz += 0.18 * D.Loa * WFy;
				Mh += WFy * 0.5;
			}

			// ---- hull resistance
			const double Fn = std::fabs(U) / std::sqrt(Gravity * D.Lwl);
			const double Plan = U > 0.0 ? D.Planing * Smooth(0.50, 0.85, Fn) : 0.0;
			{
				const double Rf = 0.5 * WaterDensity * 0.0045 * D.Wetted * (1.0 - 0.3 * Plan) * U * U;
				const double X4 = (Fn / 0.4) * (Fn / 0.4) * (Fn / 0.4) * (Fn / 0.4);
				const double Rd = Weight * std::min(0.021 * X4, 0.8);
				const double Rp = Weight * (D.PlaneDrag + 0.045 * std::max(0.0, Fn - 0.6));
				// (the last term: the skin's drag at a crawl, so that a breath of air moves nothing)
				double Rt = Rf + (1.0 - Plan) * Rd + Plan * Rp + 2.0 * D.Wetted * std::fabs(U) / (1.0 + 4.0 * U * U);
				// waves: worst punching into them
				const double Tx = E.WindX * Cy + E.WindY * Sy;
				const double Tw = std::sqrt(E.WindX * E.WindX + E.WindY * E.WindY);
				const double Into = Tw > 0.1 ? Clamp(-Tx / Tw, 0.0, 1.0) : 0.5;
				Rt *= 1.0 + 2.2 * Clamp(E.WaveHs / D.Lwl, 0.0, 0.5) * (0.25 + 0.75 * Into);
				Rt *= 1.0 + 0.6 * B.Water;                      // a cockpit full of water drags
				Fx -= (U >= 0.0 ? 1.0 : -1.6) * Rt;             // a transom going backwards is a brake
				// the hull itself pushed sideways
				Fy -= 0.5 * WaterDensity * D.HullLatArea * (1.1 * V * std::fabs(V) + 0.25 * std::fabs(U) * V);
			}

			// ---- centreboard and rudder
			double RudderFy = 0.0;
			{
				double Bx, By;
				const double BoardX = 0.02 * D.Loa;
				Foil(U, V + R * BoardX, 0.0, D.BoardArea * B.Board, 2.2 + 1.2 * B.Board, Bx, By);
				Fx += Bx;
				Fy += By;
				Nz += BoardX * By;
				double Rx, Ry;
				// a rudder lifted half out of the water by the heel bites less
				const double Bite = Clamp(Ch * Ch * 1.15, 0.0, 1.0);
				// (it works in the board's downwash: it feels only a part of the leeway)
				Foil(U, 0.35 * V - R * D.RudderX, B.Rudder * RudderMax, D.RudderArea * Bite, 2.8, Rx, Ry);
				Fx += Rx;
				Fy += Ry;
				Nz -= D.RudderX * Ry;
				RudderFy = Ry;
			}
			// heeled, the hull's shape turns her up into the wind
			Nz -= 0.6 * Mass * Sh * U * std::fabs(U);
			// the hull does not like to spin
			// (least when she has way on and carves round; stopped, the whole hull has to be pushed sideways through the water)
			const double YawQuad = 0.003 * (1.0 + 5.0 / (1.0 + U * U)) * WaterDensity * D.HullDraft * D.Loa * D.Loa * D.Loa * D.Loa;
			Nz -= YawQuad * R * std::fabs(R);
			const double YawLin = 0.001 * WaterDensity * D.HullDraft * D.Loa * D.Loa * D.Loa * (1.0 + std::fabs(U));

			// ---- heel
			B.HeelMoment = Mh;
			const double Run = Smooth(60.0 * Deg, 85.0 * Deg, Alpha) * Fill;    // a stalled sail square to the wind
			const double FormMax = Weight * D.Beam * D.FormK * (1.0 - 0.45 * B.Water);
			const double Ixx = 1.3 * (Mass * (0.32 * D.Beam) * (0.32 * D.Beam) + 0.12 * D.HullMass * D.MastLen * D.MastLen * 0.25);
			{
				const double Rel = Clamp(Pi * (B.Heel - B.WaveRoll) / FormVanish, -1.2 * Pi, 1.2 * Pi);
				double M = Mh - FormMax * std::sin(Rel);
				M += B.CrewMass * Gravity * (B.Hike * D.HikeArm * Ch + 0.25 * Sh);     // the crew sits above the water: over she leans, over they pull
				// dead before the wind the eddies off the sail roll her from side to side
				const double Stiff = FormMax * Pi / FormVanish;
				const double Wn = std::sqrt(Stiff / Ixx);
				B.RollPhase = std::fmod(B.RollPhase + H * Wn, 2.0 * Pi);
				M += 0.13 * Run * Q * D.MainArea * D.CeHeight * std::sin(B.RollPhase);
				// what steadies her: the hull and board in the water, and a sail with flow on it
				const double DampHull = 2.0 * (0.28 + 0.14 * B.Board + 0.03 * std::min(std::fabs(U), 4.0)) * std::sqrt(Stiff * Ixx);
				const double DampSail = 0.5 * AirDensity * (D.MainArea + D.JibArea) * A.Va * (0.4 + 2.6 * std::max(Attach, JibAttach)) * D.CeHeight * D.CeHeight;
				double Damp = DampHull + DampSail;
				if (B.Assist.bNoCapsize && std::fabs(B.Heel) > 0.55)
				{
					M -= Sign(B.Heel) * (std::fabs(B.Heel) - 0.55) * 12.0 * Stiff;
					Damp *= 3.0;
				}
				B.HeelRate = (B.HeelRate + H * M / Ixx) / (1.0 + H * Damp / Ixx);
				B.Heel += B.HeelRate * H;
				if (B.Assist.bNoCapsize)
				{
					B.Heel = Clamp(B.Heel, -0.9, 0.9);
				}
			}

			// ---- move
			const double Ax = Fx / (Mass * 1.05), Ay = Fy / (Mass * 1.5);
			B.VX += (Ax * Cy - Ay * Sy) * H;
			B.VY += (Ax * Sy + Ay * Cy) * H;
			const double Izz = 1.25 * Mass * (0.27 * D.Loa) * (0.27 * D.Loa);
			B.YawRate = (B.YawRate + H * Nz / Izz) / (1.0 + H * YawLin / Izz);
			B.Yaw = WrapAngle(B.Yaw + B.YawRate * H);
			B.X += B.VX * H;
			B.Y += B.VY * H;

			// ---- water aboard
			if (!B.Assist.bNoCapsize)
			{
				const double Over = std::fabs(B.Heel - B.WaveRoll) - D.SwampHeel;
				if (Over > 0.0)
				{
					B.Water = std::min(1.0, B.Water + H * 0.8 * Over * (1.0 + 0.3 * std::fabs(U)));
				}
			}
			if (B.Water > 0.0 && std::fabs(U) > 1.2)
			{
				B.Water = std::max(0.0, B.Water - H * D.SelfBail);
			}
			if (B.State == EState::Sailing && B.Water > 0.4)
			{
				SetState(B, EState::Swamped);
				B.Events |= EvSwamped;
			}
			else if (B.State == EState::Swamped && B.Water < 0.12)
			{
				SetState(B, EState::Sailing);
				B.Events |= EvBailed;
			}

			// ---- over she goes
			if (std::fabs(B.Heel) > CapsizeHeel)
			{
				SetState(B, EState::Capsized);
				B.Events |= EvCapsize;
				B.bWasTurtled = false;
				B.Depower = 0.0;
			}

			// ---- on the plane or off it
			if (!B.bPlaning && Plan > 0.6)
			{
				B.bPlaning = true;
				B.Events |= EvPlaningStart;
			}
			else if (B.bPlaning && Plan < 0.3)
			{
				B.bPlaning = false;
				B.Events |= EvPlaningEnd;
			}
			B.Out.Planing = Plan;
			B.Out.HelmLoad = Clamp(RudderFy / (0.5 * WaterDensity * D.RudderArea * 9.0), -1.0, 1.0);
		}

		// ---- not sailing: on her side, upside down, being righted, on the bottom, on a buoy, on the sand -----------------
		void PassiveStep(FBoat& B, const FBoatDef& D, const FControls& C, const FEnv& E, const FAir& A, double H)
		{
			const double Tws = std::sqrt(E.WindX * E.WindX + E.WindY * E.WindY);
			B.Out.SheetLoad = 0.0;
			B.HeelMoment = 0.0;
			B.Out.Planing = 0.0;
			B.Out.HelmLoad = 0.0;
			B.Out.MainLuff = B.Out.JibLuff = B.State == EState::Capsized || B.State == EState::Turtled ? 0.0 : 1.0;
			B.bPlaning = false;
			switch (B.State)
			{
			case EState::Capsized:
			case EState::Turtled:
			case EState::Righting:
			{
				// drifts with the water and a little with the wind
				const double K = std::min(1.0, H / 1.5);
				B.VX += (E.CurrentX + 0.03 * E.WindX - B.VX) * K;
				B.VY += (E.CurrentY + 0.03 * E.WindY - B.VY) * K;
				B.YawRate *= std::max(0.0, 1.0 - 2.0 * H);
				B.X += B.VX * H;
				B.Y += B.VY * H;
				B.Yaw = WrapAngle(B.Yaw + B.YawRate * H);
				const double Side = Sign(B.Heel);
				if (B.State == EState::Capsized)
				{
					B.Heel = Toward(B.Heel, Side * 0.5 * Pi, 1.8 * H);
					B.HeelRate = 0.0;
					B.Water = std::max(B.Water, D.RightWater * std::min(1.0, B.StateTime / 3.0));
					// left alone in a blow, or for long, the mast goes under (unless it finds the bottom first)
					if (B.StateTime > 6.0 && E.Depth > 0.7 * D.MastLen && !C.bRight)
					{
						const double PerSec = (Tws > 9.0 ? 0.02 : 0.004) * (1.3 - B.CrewSkill);
						if (Rand01(B.Seed) < PerSec * H)
						{
							SetState(B, EState::Turtled);
							B.Events |= EvTurtle;
						}
					}
				}
				else if (B.State == EState::Turtled)
				{
					B.Heel = Toward(B.Heel, Side * Pi, 0.8 * H);
				}
				if ((B.State == EState::Capsized || B.State == EState::Turtled) && C.bRight)
				{
					B.bWasTurtled = B.State == EState::Turtled;
					B.RightNeed = D.RightTime * (1.7 - B.CrewSkill) * Clamp(std::sqrt(D.CrewMass / std::max(B.CrewMass, 10.0)), 0.75, 1.4)
						* (B.bWasTurtled ? 2.0 : 1.0) * (1.0 + Tws / 30.0);
					B.RightDone = 0.0;
					SetState(B, EState::Righting);
				}
				else if (B.State == EState::Righting)
				{
					B.RightDone += H;
					const double P = Clamp(B.RightDone / B.RightNeed, 0.0, 1.0);
					// turtled: first back onto her side; then she hangs, then comes up with a rush
					const double Lying = B.bWasTurtled ? (P < 0.5 ? Pi - 0.5 * Pi * Smooth(0.0, 0.5, P) : 0.5 * Pi) : 0.5 * Pi;
					B.Heel = Side * (P < 0.8 ? Lying : 0.5 * Pi * (1.0 - Smooth(0.8, 1.0, P)) + 0.08);
					if (P >= 1.0)
					{
						B.Heel = Side * 0.08;
						B.HeelRate = 0.0;
						B.VX = E.CurrentX;
						B.VY = E.CurrentY;
						B.YawRate = 0.0;
						B.Sheet = B.JibSheet = 1.0;
						B.Hike = 0.0;
						B.Water = std::max(B.Water, D.RightWater);
						B.Boom = Clamp(A.Free, -D.BoomMax, D.BoomMax);
						B.BoomSide = 0;
						B.Events |= EvRighted;
						SetState(B, B.Water > 0.4 ? EState::Swamped : EState::Sailing);
						if (B.State == EState::Swamped)
						{
							B.Events |= EvSwamped;
						}
					}
				}
				break;
			}
			case EState::Aground:
			{
				const double K = std::max(0.0, 1.0 - 6.0 * H);
				B.VX *= K;
				B.VY *= K;
				B.YawRate *= K;
				B.X += B.VX * H;
				B.Y += B.VY * H;
				B.Heel = Toward(B.Heel, 0.0, 0.5 * H);
				B.HeelRate = 0.0;
				FlagSails(B, D, A, H);
				break;
			}
			case EState::Moored:
			{
				// swings head to wind on her painter
				B.VX = B.VY = 0.0;
				if (Tws > 0.5)
				{
					const double Err = WrapAngle(WindFromYaw(E.WindX, E.WindY) - B.Yaw);
					B.YawRate = Clamp(Err * 0.4, -0.3, 0.3) * std::min(1.0, Tws / 4.0);
					B.Yaw = WrapAngle(B.Yaw + B.YawRate * H);
				}
				B.Heel = Toward(B.Heel, B.WaveRoll, 1.0 * H);
				B.HeelRate = 0.0;
				FlagSails(B, D, A, H);
				break;
			}
			default:    // Ashore, Towed (StepTowed moves it)
				B.Heel = Toward(B.Heel, 0.0, 1.0 * H);
				B.HeelRate = 0.0;
				if (B.State == EState::Towed)
				{
					FlagSails(B, D, A, H);
				}
				break;
			}
			if (C.bBail && B.State != EState::Capsized && B.State != EState::Turtled && B.State != EState::Righting && B.Water > 0.0)
			{
				B.Water = std::max(0.0, B.Water - H * (0.03 + 0.03 * B.CrewSkill));
			}
		}

		void FillOut(FBoat& B, const FBoatDef& D, const FEnv& E)
		{
			FOutputs& O = B.Out;
			const FAir A = Apparent(B, E);
			const double Cy = std::cos(B.Yaw), Sy = std::sin(B.Yaw);
			const double Wx = B.VX - E.CurrentX, Wy = B.VY - E.CurrentY;
			const double U = Wx * Cy + Wy * Sy, V = -Wx * Sy + Wy * Cy;
			O.X = B.X;
			O.Y = B.Y;
			O.Yaw = B.Yaw;
			O.VX = B.VX;
			O.VY = B.VY;
			O.YawRate = B.YawRate;
			O.Speed = U;
			O.Sog = std::sqrt(B.VX * B.VX + B.VY * B.VY);
			O.Leeway = std::fabs(U) > 0.2 ? std::atan2(V, std::fabs(U)) : 0.0;
			O.Heel = B.Heel;
			const bool bAfloat = B.State == EState::Sailing || B.State == EState::Swamped;
			O.Pitch = (B.State == EState::Ashore ? 0.0 : 0.8 * B.WavePitch) + 0.07 * O.Planing + (bAfloat ? 0.012 * Clamp(U, 0.0, 3.0) : 0.0);
			B.FlapPhase = std::fmod(B.FlapPhase + A.Va * 0.9, 2.0 * Pi);
			O.Boom = B.Boom + 0.05 * O.MainLuff * std::min(1.0, A.Va / 4.0) * std::sin(B.FlapPhase);
			O.Jib = B.Jib;
			const double Alpha = std::fabs(WrapAngle(A.Free - B.Boom));
			O.CamberSide = O.MainLuff > 0.5 ? 0 : (B.Boom > 0.0 ? 1 : -1);
			O.RudderAngle = B.Rudder * RudderMax;
			O.Board = B.Board;
			O.Hike = B.Hike;
			O.Sheet = B.Sheet;
			O.AppWindSpeed = A.Va;
			O.AppWindAngle = A.Beta;
			O.TrueWindSpeed = std::sqrt(E.WindX * E.WindX + E.WindY * E.WindY);
			O.TrueWindAngle = O.TrueWindSpeed > 0.05 ? TrueWindAngle(B.Yaw, E.WindX, E.WindY) : 0.0;
			const double Twa = std::fabs(O.TrueWindAngle);
			O.Point = Twa < 32.0 * Deg ? EPoint::InIrons : Twa < 56.0 * Deg ? EPoint::CloseHauled : Twa < 80.0 * Deg ? EPoint::CloseReach
				: Twa < 105.0 * Deg ? EPoint::BeamReach : Twa < 150.0 * Deg ? EPoint::BroadReach : EPoint::Run;
			O.bByTheLee = bAfloat && A.Beta * B.Boom > 0.0 && std::fabs(A.Beta) > 0.5 * Pi && Pi - std::fabs(A.Beta) > 4.0 * Deg && std::fabs(B.Boom) > 0.3;
			const double Ideal = IdealBoom(D, std::fabs(A.Beta));
			O.TrimError = bAfloat && A.Va > 0.7 ? Clamp((Ideal - std::fabs(B.Boom)) / 0.30, -1.0, 1.0) : 0.0;
			O.TellWindward = O.MainLuff > 0.35 ? ETell::Lifting : (Alpha > 60.0 * Deg ? ETell::Stalled : ETell::Flowing);
			O.TellLeeward = Alpha > StallAlpha + 5.0 * Deg ? ETell::Stalled : (O.MainLuff > 0.8 ? ETell::Lifting : ETell::Flowing);
			O.SheetLoad01 = Clamp(O.SheetLoad / (22.0 * (D.MainArea + 2.0)), 0.0, 1.0);
			const double Hull = std::sqrt(Gravity * D.Lwl);
			O.Wake = bAfloat ? Clamp(std::fabs(U) / (0.75 * Hull), 0.0, 1.0) : 0.0;
			O.Spray = bAfloat ? Clamp(0.7 * O.Planing + 0.25 * std::max(0.0, U - 1.0) * Clamp(E.WaveHs, 0.0, 1.0) + O.SlapStrength, 0.0, 1.0) : 0.0;
			O.Water = B.Water;
			O.Righting = B.State == EState::Righting && B.RightNeed > 0.0 ? Clamp(B.RightDone / B.RightNeed, 0.0, 1.0) : 0.0;
			O.State = B.State;
			O.Events = B.Events;
		}

		/** The sea's slope under the boat, eased by her own inertia; and the bow's slap on a wave. */
		void FeelWaves(FBoat& B, const FBoatDef& D, const FEnv& E, double Dt)
		{
			const double Cy = std::cos(B.Yaw), Sy = std::sin(B.Yaw);
			const double Nz = std::max(E.NormalZ, 0.2);
			const double Nf = E.NormalX * Cy + E.NormalY * Sy, Nr = -E.NormalX * Sy + E.NormalY * Cy;
			const double Roll = 0.7 * std::atan2(Nr, Nz), Pitch = -0.7 * std::atan2(Nf, Nz);
			const double K = std::min(1.0, Dt / 0.2);
			const double Old = B.WavePitch;
			B.WaveRoll += (Roll - B.WaveRoll) * K;
			B.WavePitch += (Pitch - B.WavePitch) * K;
			const double Rate = (B.WavePitch - Old) / Dt;
			B.SlapWait = std::max(0.0, B.SlapWait - Dt);
			const bool bAfloat = B.State == EState::Sailing || B.State == EState::Swamped;
			const double U = (B.VX - E.CurrentX) * Cy + (B.VY - E.CurrentY) * Sy;
			// the bow comes down: the pitch rate turns sharply negative while she is driving
			if (bAfloat && U > 1.0 && E.WaveHs > 0.1 && B.SlapWait <= 0.0 && B.WavePitchRate > -0.12 && Rate <= -0.12)
			{
				B.Events |= EvHullSlap;
				B.Out.SlapStrength = Clamp(0.25 * U * (-Rate) / 0.3 * std::sqrt(2.3 / D.Loa), 0.1, 1.0);
				B.SlapWait = 0.7;
			}
			B.WavePitchRate = Rate;
		}
	}

	// ---- public: the basics -------------------------------------------------------------------------------------------

	const FBoatDef& Def(EBoat Class) { return Defs[std::min(int(Class), BoatCount - 1)]; }
	const char* BoatId(EBoat Class) { return Def(Class).Id; }

	bool BoatById(const char* Id, EBoat& Out)
	{
		for (int I = 0; I < BoatCount; I++)
		{
			if (Id && std::strcmp(Id, Defs[I].Id) == 0)
			{
				Out = EBoat(I);
				return true;
			}
		}
		return false;
	}

	double CrewMassFor(EBoat Class, double AgeYears)
	{
		const double One = AgeYears >= 18.0 ? 76.0 : Clamp(22.0 + (AgeYears - 7.0) * 4.1, 20.0, 68.0);
		return Class == EBoat::Cadet ? 1.85 * One : One;
	}

	const char* StateName(EState S)
	{
		switch (S)
		{
		case EState::Ashore: return "ashore";
		case EState::Moored: return "moored";
		case EState::Sailing: return "sailing";
		case EState::Swamped: return "swamped";
		case EState::Capsized: return "capsized";
		case EState::Turtled: return "turtled";
		case EState::Righting: return "righting";
		case EState::Aground: return "aground";
		case EState::Towed: return "towed";
		}
		return "?";
	}

	const char* PointName(EPoint P)
	{
		switch (P)
		{
		case EPoint::InIrons: return "in_irons";
		case EPoint::CloseHauled: return "close_hauled";
		case EPoint::CloseReach: return "close_reach";
		case EPoint::BeamReach: return "beam_reach";
		case EPoint::BroadReach: return "broad_reach";
		case EPoint::Run: return "run";
		}
		return "?";
	}

	double WrapAngle(double A)
	{
		A = std::fmod(A + Pi, 2.0 * Pi);
		if (A < 0.0)
		{
			A += 2.0 * Pi;
		}
		return A - Pi;
	}

	double WindFromYaw(double WindX, double WindY) { return std::atan2(-WindY, -WindX); }

	double TrueWindAngle(double Yaw, double WindX, double WindY) { return WrapAngle(WindFromYaw(WindX, WindY) - Yaw); }

	double IdealSheet(const FBoat& B)
	{
		const FBoatDef& D = Def(B.Class);
		return SheetForBoom(D, IdealBoom(D, std::fabs(B.Out.AppWindAngle)));
	}

	double Vmg(const FBoat& B, double TX, double TY)
	{
		const double Dx = TX - B.X, Dy = TY - B.Y;
		const double L = std::sqrt(Dx * Dx + Dy * Dy);
		return L > 1e-6 ? (B.VX * Dx + B.VY * Dy) / L : 0.0;
	}

	double Draught(const FBoat& B)
	{
		const FBoatDef& D = Def(B.Class);
		return D.HullDraft + B.Board * D.BoardLen;
	}

	void Init(FBoat& B, EBoat Class, double CrewMassKg, double CrewSkill, uint32_t Seed)
	{
		B = FBoat();
		B.Class = Class;
		B.CrewMass = CrewMassKg > 0.0 ? CrewMassKg : Def(Class).CrewMass;
		B.CrewSkill = Clamp(CrewSkill, 0.0, 1.0);
		B.Seed = Seed ? Seed : 1u;
		B.RollPhase = 2.0 * Pi * Rand01(B.Seed);
		B.Out.State = B.State;
	}

	void Place(FBoat& B, double X, double Y, double Yaw, EState State)
	{
		B.X = X;
		B.Y = Y;
		B.Yaw = WrapAngle(Yaw);
		B.VX = B.VY = B.YawRate = 0.0;
		B.Heel = B.HeelRate = 0.0;
		B.BoomRate = 0.0;
		B.State = State;
		B.StateTime = 0.0;
		B.bFar = false;
		B.FarSpeed = 0.0;
		B.bPlaning = false;
		B.Depower = 0.0;
		B.BoomSide = 0;
		B.Out.X = X;
		B.Out.Y = Y;
		B.Out.Yaw = B.Yaw;
		B.Out.State = State;
	}

	void PlaceSailing(FBoat& B, double X, double Y, double Yaw, double Speed)
	{
		Place(B, X, Y, Yaw, EState::Sailing);
		B.VX = std::cos(B.Yaw) * Speed;
		B.VY = std::sin(B.Yaw) * Speed;
		B.FarSpeed = Speed;
	}

	void Step(FBoat& B, const FControls& C, const FEnv& E, double Dt)
	{
		B.Events = 0;
		B.Out.SlapStrength = 0.0;
		B.Out.BoomSlamSpeed = 0.0;
		const FBoatDef& D = Def(B.Class);
		if (!(Dt > 0.0))
		{
			FillOut(B, D, E);
			return;
		}
		if (B.bFar)
		{
			FromFar(B, D, E);
		}
		FeelWaves(B, D, E, Dt);

		// the bottom
		const bool bAfloat = B.State == EState::Sailing || B.State == EState::Swamped;
		if (bAfloat && E.Depth < Draught(B))
		{
			SetState(B, EState::Aground);
			B.Events |= EvGrounded;
			B.VX *= 0.3;
			B.VY *= 0.3;
		}
		else if (B.State == EState::Aground && E.Depth > Draught(B) + 0.02)
		{
			SetState(B, B.Water > 0.4 ? EState::Swamped : EState::Sailing);
			B.Events |= EvFloatedOff;
		}

		const int N = int(Clamp(std::ceil(Dt / MaxSub - 1e-9), 1.0, 8.0));
		const double H = Dt / N;
		for (int I = 0; I < N; I++)
		{
			const FAir A = Apparent(B, E);
			MoveGear(B, D, C, A, H);
			if (B.State == EState::Sailing || B.State == EState::Swamped)
			{
				if (C.bBail && B.Water > 0.0)
				{
					B.Water = std::max(0.0, B.Water - H * (0.03 + 0.03 * B.CrewSkill));
				}
				SailStep(B, D, E, A, H);
			}
			else
			{
				PassiveStep(B, D, C, E, A, H);
			}
			B.StateTime += H;
		}
		FillOut(B, D, E);
	}

	void StepFar(FBoat& B, double CourseYaw, double SpeedScale, const FEnv& E, double Dt)
	{
		B.Events = 0;
		B.Out.SlapStrength = 0.0;
		B.Out.BoomSlamSpeed = 0.0;
		const FBoatDef& D = Def(B.Class);
		const double C0 = std::cos(B.Yaw), S0 = std::sin(B.Yaw);
		if (!B.bFar)
		{
			B.bFar = true;
			B.FarSpeed = (B.VX - E.CurrentX) * C0 + (B.VY - E.CurrentY) * S0;
		}
		if (!(Dt > 0.0))
		{
			FillOut(B, D, E);
			return;
		}
		const double Tws = std::sqrt(E.WindX * E.WindX + E.WindY * E.WindY);
		B.StateTime += Dt;
		FeelWaves(B, D, E, Dt);
		switch (B.State)
		{
		case EState::Sailing:
		case EState::Swamped:
		{
			if (E.Depth < D.HullDraft)
			{
				SetState(B, EState::Aground);
				B.Events |= EvGrounded;
				B.FarSpeed = 0.0;
				break;
			}
			// a distant crew is always sensible with the board
			B.Board = Clamp((E.Depth - D.HullDraft - 0.15) / D.BoardLen, 0.0, 1.0);
			const double Twa = Tws > 0.05 ? TrueWindAngle(B.Yaw, E.WindX, E.WindY) : 0.0;
			const double Into = Clamp(std::cos(Twa), 0.0, 1.0);
			double Target = PolarSpeed(B.Class, Twa, Tws) * (0.72 + 0.28 * B.CrewSkill) * Clamp(SpeedScale, 0.0, 1.0);
			Target *= (1.0 - 0.4 * B.Water) / (1.0 + 0.9 * Clamp(E.WaveHs / D.Lwl, 0.0, 0.5) * (0.3 + 0.7 * Into));
			// in a blow the gusts knock a real crew about more than the steady polar knows (measured against the full model)
			static const double BlowLoss[BoatCount] = { 0.15, 0.03, 0.10 };
			Target *= 1.0 - BlowLoss[int(B.Class)] * Smooth(5.5, 7.5, Tws);
			B.FarSpeed += (Target - B.FarSpeed) * std::min(1.0, Dt / (Target < B.FarSpeed ? 2.0 : 4.0));
			const double Turn = (0.2 + 0.35 * std::min(1.0, std::fabs(B.FarSpeed))) * Dt;
			const double OldTwa = Twa;
			B.YawRate = Clamp(WrapAngle(CourseYaw - B.Yaw), -Turn, Turn) / Dt;
			B.Yaw = WrapAngle(B.Yaw + B.YawRate * Dt);
			B.VX = std::cos(B.Yaw) * B.FarSpeed + E.CurrentX;
			B.VY = std::sin(B.Yaw) * B.FarSpeed + E.CurrentY;
			B.X += B.VX * Dt;
			B.Y += B.VY * Dt;
			const double NewTwa = Tws > 0.05 ? TrueWindAngle(B.Yaw, E.WindX, E.WindY) : 0.0;
			if (OldTwa * NewTwa < 0.0)
			{
				B.Events |= std::fabs(NewTwa) > 0.5 * Pi ? EvGybe : EvTack;
			}
			const double HeelT = -Sign(NewTwa) * PolarHeel(B.Class, NewTwa, Tws) + B.WaveRoll;
			B.Heel += (HeelT - B.Heel) * std::min(1.0, Dt / 0.8);
			B.HeelRate = 0.0;
			const FAir A = Apparent(B, E);
			const double BoomT = Sign(A.Free) * std::min(std::fabs(A.Free), IdealBoom(D, std::fabs(A.Beta)) + (1.0 - Clamp(SpeedScale, 0.0, 1.0)) * 0.5);
			B.Boom = Toward(B.Boom, BoomT, 2.5 * Dt);
			B.Jib = Clamp(B.Boom, -D.JibMax, D.JibMax);
			B.Sheet = B.JibSheet = SheetForBoom(D, std::fabs(B.Boom));
			B.BoomSide = B.Boom >= 0.0 ? 1 : -1;
			B.Hike = Toward(B.Hike, -Clamp(B.Heel / 0.09, -1.0, 1.0), 1.5 * Dt);
			B.HeelMoment = -B.Hike * B.CrewMass * Gravity * D.HikeArm;    // what the crew is holding down, for the hand-over
			B.Rudder = Clamp(B.YawRate * 2.0, -1.0, 1.0);
			const double Fill = std::fabs(WrapAngle(A.Free - B.Boom)) > 8.0 * Deg && A.Va > 0.7 ? 1.0 : 0.0;
			B.Out.MainLuff += ((1.0 - Fill) - B.Out.MainLuff) * std::min(1.0, Dt / 0.4);
			B.Out.JibLuff = B.Out.MainLuff;
			const double Fn = std::fabs(B.FarSpeed) / std::sqrt(Gravity * D.Lwl);
			B.Out.Planing = D.Planing * Smooth(0.50, 0.85, Fn);
			B.Out.SheetLoad = 0.2 * AirDensity * A.Va * A.Va * D.MainArea * Fill;
			B.Out.HelmLoad = 0.0;
			if (B.Water > 0.0)
			{
				B.Water = std::max(0.0, B.Water - 0.03 * Dt);
				if (B.State == EState::Swamped && B.Water < 0.12)
				{
					SetState(B, EState::Sailing);
					B.Events |= EvBailed;
				}
			}
			// now and then a beginner goes in when it blows
			if (!B.Assist.bNoCapsize && Tws > 5.5 && SpeedScale > 0.05)
			{
				const double PerSec = (1.0 - B.CrewSkill) * (1.0 - B.CrewSkill) * (Tws - 5.5) * (Tws - 5.5) * 3e-4 * (B.Class == EBoat::Optimist ? 1.5 : 1.0);
				if (Rand01(B.Seed) < PerSec * Dt)
				{
					SetState(B, EState::Capsized);
					B.Events |= EvCapsize;
					B.Heel = (B.Heel >= 0.0 ? 1.0 : -1.0) * 0.5 * Pi;
					B.FarSpeed = 0.0;
				}
			}
			break;
		}
		case EState::Capsized:
		case EState::Turtled:
		case EState::Righting:
		{
			B.VX = E.CurrentX + 0.03 * E.WindX;
			B.VY = E.CurrentY + 0.03 * E.WindY;
			B.X += B.VX * Dt;
			B.Y += B.VY * Dt;
			B.YawRate = 0.0;
			B.FarSpeed = 0.0;
			const double Need = 3.0 + 5.0 * (1.0 - B.CrewSkill) + D.RightTime * (1.7 - B.CrewSkill) * (B.State == EState::Turtled ? 2.0 : 1.0);
			B.Out.MainLuff = B.Out.JibLuff = 0.0;
			if (B.StateTime > Need)
			{
				B.Heel = 0.0;
				B.Water = std::max(B.Water, D.RightWater);
				B.Events |= EvRighted;
				SetState(B, B.Water > 0.4 ? EState::Swamped : EState::Sailing);
			}
			break;
		}
		case EState::Aground:
			B.VX = B.VY = B.YawRate = 0.0;
			B.FarSpeed = 0.0;
			if (E.Depth > D.HullDraft + 0.05)
			{
				SetState(B, EState::Sailing);
				B.Events |= EvFloatedOff;
			}
			break;
		default:
			B.VX = B.VY = B.YawRate = 0.0;
			B.FarSpeed = 0.0;
			break;
		}
		FillOut(B, D, E);
	}

	void StepTowed(FBoat& B, double HitchX, double HitchY, double RopeLen, const FEnv& E, double Dt)
	{
		B.Events = 0;
		B.Out.SlapStrength = 0.0;
		B.Out.BoomSlamSpeed = 0.0;
		const FBoatDef& D = Def(B.Class);
		SetState(B, EState::Towed);
		B.bFar = false;
		if (Dt > 0.0)
		{
			FeelWaves(B, D, E, Dt);
			// the bow is where the rope ends; the boat trails behind it
			const double BowX = B.X + std::cos(B.Yaw) * 0.5 * D.Loa, BowY = B.Y + std::sin(B.Yaw) * 0.5 * D.Loa;
			const double Dx = HitchX - BowX, Dy = HitchY - BowY;
			const double Dist = std::sqrt(Dx * Dx + Dy * Dy);
			double Mx = E.CurrentX * Dt, My = E.CurrentY * Dt;
			if (Dist > RopeLen && Dist > 1e-6)
			{
				const double Pull = Dist - RopeLen;
				Mx = Dx / Dist * Pull;
				My = Dy / Dist * Pull;
				const double Err = WrapAngle(std::atan2(Dy, Dx) - B.Yaw);
				B.YawRate = Clamp(Err * 2.0, -1.2, 1.2);
			}
			else
			{
				B.YawRate *= std::max(0.0, 1.0 - 2.0 * Dt);
			}
			B.X += Mx;
			B.Y += My;
			B.VX = Mx / Dt;
			B.VY = My / Dt;
			B.Yaw = WrapAngle(B.Yaw + B.YawRate * Dt);
			B.Sheet = Toward(B.Sheet, 1.0, 2.5 * Dt);
			B.JibSheet = B.Sheet;
			B.Rudder = Toward(B.Rudder, 0.0, 3.5 * Dt);
			B.Hike = Toward(B.Hike, 0.0, Dt);
			const FControls None;
			PassiveStep(B, D, None, E, Apparent(B, E), Dt);
			B.StateTime += Dt;
		}
		FillOut(B, D, E);
	}

	// ---- the polar table ----------------------------------------------------------------------------------------------

	namespace
	{
		const float PolarWind[PolarWinds] = { 0.f, 1.f, 2.f, 3.f, 4.5f, 6.f, 8.f, 10.f, 12.f, 14.f };

		double Bilinear(const FPolar& P, const float (*T)[PolarAngles], double Twa, double Tws)
		{
			const double A = Clamp(std::fabs(WrapAngle(Twa)) / (10.0 * Deg), 0.0, double(PolarAngles - 1));
			const int A0 = std::min(int(A), PolarAngles - 2);
			const double Af = A - A0;
			int W0 = 0;
			const double W = Clamp(Tws, 0.0, double(P.Wind[PolarWinds - 1]));
			while (W0 < PolarWinds - 2 && W > P.Wind[W0 + 1])
			{
				W0++;
			}
			const double Wf = (W - P.Wind[W0]) / (P.Wind[W0 + 1] - P.Wind[W0]);
			const double Lo = T[W0][A0] * (1.0 - Af) + T[W0][A0 + 1] * Af;
			const double Hi = T[W0 + 1][A0] * (1.0 - Af) + T[W0 + 1][A0 + 1] * Af;
			return Lo * (1.0 - Wf) + Hi * Wf;
		}

		FPolar BuildPolar(EBoat Class)
		{
			FPolar P;
			std::memset(&P, 0, sizeof(P));
			const FBoatDef& D = Def(Class);
			for (int W = 0; W < PolarWinds; W++)
			{
				P.Wind[W] = PolarWind[W];
				P.BeatAngle[W] = float(45.0 * Deg);
				P.RunAngle[W] = float(Pi);
				if (W == 0)
				{
					continue;
				}
				const double Tws = PolarWind[W];
				FEnv E;
				E.WindX = -Tws;     // from yaw 0
				// (nothing sails closer than 30 degrees: those columns stay zero)
				for (int Ai = 3; Ai < PolarAngles; Ai++)
				{
					const double Twa = Ai * 10.0 * Deg;
					FBoat B;
					Init(B, Class, 0.0, 0.85, 77u + uint32_t(Ai) * 13u + uint32_t(W) * 101u);
					B.Assist.bAutoSheet = B.Assist.bAutoHike = B.Assist.bNoCapsize = true;
					PlaceSailing(B, 0.0, 0.0, -Twa, std::min(0.3 * Tws, 2.0));
					B.Sheet = SheetForBoom(D, IdealBoom(D, Twa * 0.7));
					B.Boom = -BoomLimit(D, B.Sheet);
					FControls C;
					double Sum = 0.0, HeelSum = 0.0, DepSum = 0.0;
					int N = 0;
					const double Dt = 1.0 / 30.0;
					for (int I = 0; I < 1500; I++)
					{
						const double Err = WrapAngle(-Twa - B.Yaw);
						C.Rudder = Clamp(2.5 * Err - 1.2 * B.YawRate, -1.0, 1.0) * (B.Out.Speed < -0.05 ? -1.0 : 1.0);
						C.Board = Twa < 75.0 * Deg ? 1.0 : (Twa < 130.0 * Deg ? 0.7 : 0.4);
						Step(B, C, E, Dt);
						if (I >= 1050)
						{
							Sum += B.Out.Speed * std::cos(B.Out.Leeway);
							HeelSum += std::fabs(B.Heel);
							DepSum += B.Depower;
							N++;
						}
					}
					P.Speed[W][Ai] = float(std::max(0.0, Sum / N));
					P.Heel[W][Ai] = float(HeelSum / N);
					P.Depower[W][Ai] = float(DepSum / N);
				}
			}
			for (int W = 1; W < PolarWinds; W++)
			{
				double BestUp = 0.0, BestDn = 0.0, Up = 45.0 * Deg, Dn = Pi;
				for (int A = 30; A <= 70; A++)
				{
					const double V = Bilinear(P, P.Speed, A * Deg, P.Wind[W]) * std::cos(A * Deg);
					if (V > BestUp)
					{
						BestUp = V;
						Up = A * Deg;
					}
				}
				for (int A = 120; A <= 180; A++)
				{
					const double V = -Bilinear(P, P.Speed, A * Deg, P.Wind[W]) * std::cos(A * Deg);
					if (V > BestDn)
					{
						BestDn = V;
						Dn = A * Deg;
					}
				}
				// not worth gybing down the angles for a few per cent
				if (Bilinear(P, P.Speed, Pi, P.Wind[W]) >= 0.97 * BestDn)
				{
					Dn = Pi;
				}
				P.BeatAngle[W] = float(Up);
				P.RunAngle[W] = float(Dn);
			}
			P.BeatAngle[0] = P.BeatAngle[1];
			return P;
		}

		double Linear(const FPolar& P, const float* T, double Tws)
		{
			int W0 = 0;
			const double W = Clamp(Tws, 0.0, double(P.Wind[PolarWinds - 1]));
			while (W0 < PolarWinds - 2 && W > P.Wind[W0 + 1])
			{
				W0++;
			}
			const double Wf = (W - P.Wind[W0]) / (P.Wind[W0 + 1] - P.Wind[W0]);
			return T[W0] * (1.0 - Wf) + T[W0 + 1] * Wf;
		}
	}

	const FPolar& Polar(EBoat Class)
	{
		switch (Class)
		{
		case EBoat::Optimist:
		{
			static const FPolar P = BuildPolar(EBoat::Optimist);
			return P;
		}
		case EBoat::Cadet:
		{
			static const FPolar P = BuildPolar(EBoat::Cadet);
			return P;
		}
		default:
		{
			static const FPolar P = BuildPolar(EBoat::Luch);
			return P;
		}
		}
	}

	void WarmPolars()
	{
		for (int I = 0; I < BoatCount; I++)
		{
			(void)Polar(EBoat(I));
		}
	}

	double PolarSpeed(EBoat Class, double Twa, double Tws)
	{
		const FPolar& P = Polar(Class);
		return Bilinear(P, P.Speed, Twa, Tws);
	}

	double PolarHeel(EBoat Class, double Twa, double Tws)
	{
		const FPolar& P = Polar(Class);
		return Bilinear(P, P.Heel, Twa, Tws);
	}

	double BeatAngle(EBoat Class, double Tws)
	{
		const FPolar& P = Polar(Class);
		return Linear(P, P.BeatAngle, Tws);
	}

	double RunAngle(EBoat Class, double Tws)
	{
		const FPolar& P = Polar(Class);
		// either straight down or at an angle: never a blend of the two
		int W0 = 0;
		const double W = Clamp(Tws, 0.0, double(P.Wind[PolarWinds - 1]));
		while (W0 < PolarWinds - 2 && W > 0.5 * (P.Wind[W0] + P.Wind[W0 + 1]))
		{
			W0++;
		}
		return P.RunAngle[W0];
	}

	// ---- the tutor ----------------------------------------------------------------------------------------------------

	const char* AdviceName(EAdvice A)
	{
		switch (A)
		{
		case EAdvice::None: return "none";
		case EAdvice::SheetIn: return "sheet_in";
		case EAdvice::Ease: return "ease";
		case EAdvice::BearAway: return "bear_away";
		case EAdvice::InIrons: return "in_irons";
		case EAdvice::Hike: return "hike";
		case EAdvice::BoardUp: return "board_up";
		case EAdvice::BoardDown: return "board_down";
		case EAdvice::ByTheLee: return "by_the_lee";
		case EAdvice::RightBoat: return "right_boat";
		case EAdvice::Bail: return "bail";
		case EAdvice::FloatOff: return "float_off";
		}
		return "?";
	}

	EAdvice Advice(const FBoat& B, const FEnv& E)
	{
		const FBoatDef& D = Def(B.Class);
		const FOutputs& O = B.Out;
		switch (B.State)
		{
		case EState::Capsized:
		case EState::Turtled:
			return EAdvice::RightBoat;
		case EState::Aground:
			return EAdvice::FloatOff;
		case EState::Sailing:
		case EState::Swamped:
			break;
		default:
			return EAdvice::None;
		}
		if (E.Depth < Draught(B) + 0.25 && B.Board > 0.05)
		{
			return EAdvice::BoardUp;
		}
		const double Twa = std::fabs(O.TrueWindAngle);
		const double Heel = std::fabs(B.Heel - B.WaveRoll);
		if (B.State == EState::Swamped && B.Water > 0.3)
		{
			return EAdvice::Bail;
		}
		if (O.TrueWindSpeed > 1.0 && Twa < 30.0 * Deg && O.Speed < 0.4)
		{
			return EAdvice::InIrons;
		}
		if (O.bByTheLee && Pi - std::fabs(O.AppWindAngle) > 10.0 * Deg)
		{
			return EAdvice::ByTheLee;
		}
		if (Heel > 0.30)
		{
			// lean out first; when that is not enough, let the sheet go
			const bool bHiked = B.Hike * B.Heel < 0.0 && std::fabs(B.Hike) > 0.8;
			return bHiked || B.Assist.bAutoHike ? EAdvice::Ease : EAdvice::Hike;
		}
		if (B.State == EState::Swamped)
		{
			return EAdvice::Bail;
		}
		if (O.AppWindSpeed > 1.0)
		{
			if (O.MainLuff > 0.5 && !B.Assist.bAutoSheet)
			{
				// flapping with the sheet already in: she is pointing too high
				return B.Sheet < 0.08 || O.TrimError > -0.15 ? EAdvice::BearAway : EAdvice::SheetIn;
			}
			if (O.TrimError > 0.45 && !B.Assist.bAutoSheet)
			{
				return EAdvice::Ease;
			}
			if (O.TrimError < -0.45 && !B.Assist.bAutoSheet)
			{
				return EAdvice::SheetIn;
			}
		}
		if (Twa < 70.0 * Deg && B.Board < 0.5 && E.Depth > D.HullDraft + D.BoardLen + 0.4 && std::fabs(O.Leeway) > 8.0 * Deg)
		{
			return EAdvice::BoardDown;
		}
		if (Heel > 0.16 && B.Hike * B.Heel > -0.3 * std::fabs(B.Heel) && !B.Assist.bAutoHike)
		{
			return EAdvice::Hike;
		}
		return EAdvice::None;
	}

	// ---- the helmsman -------------------------------------------------------------------------------------------------

	namespace
	{
		bool InPoly(const double* Poly, int N, double X, double Y)
		{
			bool bIn = false;
			for (int I = 0, J = N - 1; I < N; J = I++)
			{
				const double Xi = Poly[2 * I], Yi = Poly[2 * I + 1], Xj = Poly[2 * J], Yj = Poly[2 * J + 1];
				if ((Yi > Y) != (Yj > Y) && X < (Xj - Xi) * (Y - Yi) / (Yj - Yi) + Xi)
				{
					bIn = !bIn;
				}
			}
			return bIn;
		}

		bool Blocked(const FHelmInput& In, double X, double Y)
		{
			if (In.Poly && In.PolyCount >= 3 && !InPoly(In.Poly, In.PolyCount, X, Y))
			{
				return true;
			}
			return In.DepthFn && In.DepthFn(In.DepthUser, X, Y) < In.MinDepth;
		}

		void Perceive(FHelm& H, const FBoat& B, const FHelmInput& In, double Dt)
		{
			if (!H.bInit)
			{
				H.bInit = true;
				H.FastX = H.SlowX = In.WindX;
				H.FastY = H.SlowY = In.WindY;
				const double Twa = TrueWindAngle(B.Yaw, In.WindX, In.WindY);
				H.Tack = Twa >= 0.0 ? 1 : -1;
				H.HeelSeen = B.Heel;
			}
			const double Kf = std::min(1.0, Dt / (2.0 + 3.0 * (1.0 - H.Skill))), Ks = std::min(1.0, Dt / 45.0);
			H.FastX += (In.WindX - H.FastX) * Kf;
			H.FastY += (In.WindY - H.FastY) * Kf;
			H.SlowX += (In.WindX - H.SlowX) * Ks;
			H.SlowY += (In.WindY - H.SlowY) * Ks;
			H.Clock += Dt;
			H.SinceManeuver += Dt;
		}

		/**
		 * Where to point to get to (TX, TY): straight at it on a reach, close-hauled tack by tack when it lies upwind,
		 * down the gybing angles when it lies dead downwind and that pays. bTurn = it is time to tack or gybe.
		 */
		double Heading(FHelm& H, const FBoat& B, double TX, double TY, bool bFar, bool& bTurn)
		{
			bTurn = false;
			const double Dx = TX - B.X, Dy = TY - B.Y;
			const double Dist = std::sqrt(Dx * Dx + Dy * Dy);
			const double Bearing = Dist > 1e-3 ? std::atan2(Dy, Dx) : B.Yaw;
			const double Tws = std::sqrt(H.FastX * H.FastX + H.FastY * H.FastY);
			if (Tws < 0.3)
			{
				H.Mode = 0;
				return Bearing;
			}
			const FBoatDef& D = Def(B.Class);
			const double From = WindFromYaw(H.FastX, H.FastY);
			const double SlowFrom = std::sqrt(H.SlowX * H.SlowX + H.SlowY * H.SlowY) > 0.3 ? WindFromYaw(H.SlowX, H.SlowY) : From;
			const double Up = BeatAngle(B.Class, Tws) + 2.0 * Deg;
			const double Lee = bFar ? 0.0 : Clamp(std::fabs(B.Out.Leeway), 0.0, 12.0 * Deg);
			const double UpCog = Up + 0.7 * Lee;
			const double Dn = H.Skill < 0.35 ? Pi : RunAngle(B.Class, Tws);
			const double G = Pi - Dn;
			const double Rel = WrapAngle(Bearing - SlowFrom);
			const double AbsRel = std::fabs(Rel);
			const double Down = Pi - AbsRel;

			if (H.Mode == 1)
			{
				if (AbsRel > UpCog + 7.0 * Deg)
				{
					H.Mode = 0;
				}
			}
			else if (AbsRel < UpCog)
			{
				H.Mode = 1;
			}
			if (H.Mode == 2)
			{
				if (G < 4.0 * Deg || Down > G + 8.0 * Deg)
				{
					H.Mode = 0;
				}
			}
			else if (H.Mode == 0 && G > 4.0 * Deg && Down < G - 2.0 * Deg)
			{
				H.Mode = 2;
			}
			if (Dist < 2.0 * D.Loa)
			{
				H.Mode = 0;
			}

			if (H.Mode == 1)
			{
				// Side > 0: the mark lies over on the other tack's side of the wind
				const double Side = -Rel * H.Tack;
				const bool bLay = Side > UpCog + H.Overstand;
				const bool bLane = Side > 0.0 && Dist * std::sin(std::min(AbsRel, 0.5 * Pi)) > H.Order.Lane;
				const double Shift = WrapAngle(From - SlowFrom);
				const bool bHeaded = -H.Tack * Shift > H.HeaderDeg * Deg && Side > -10.0 * Deg;
				// HelmUpdate counts how long the header has lasted
				const bool bHeader = !bFar && H.Skill > 0.3 && bHeaded && H.HeaderTime > 3.0 + 10.0 * (1.0 - H.Skill) && H.SinceManeuver > 20.0;
				if ((bLay || bLane || bHeader) && H.SinceManeuver > 5.0)
				{
					bTurn = true;
				}
				return WrapAngle(From - H.Tack * Up);
			}
			if (H.Mode == 2)
			{
				const double C = WrapAngle(Rel - Pi);    // the mark's angle off dead downwind
				const double Side = -C * H.Tack;
				const bool bLay = Side > G + H.Overstand;
				const bool bLane = Side > 0.0 && Dist * std::sin(std::min(std::fabs(C), 0.5 * Pi)) > H.Order.Lane;
				if ((bLay || bLane) && H.SinceManeuver > 6.0)
				{
					bTurn = true;
				}
				return WrapAngle(From - H.Tack * Dn);
			}
			double Want = Bearing;
			if (!bFar && B.Out.Sog > 0.6 && Dist > 3.0 * D.Loa)
			{
				// hold the crab angle the leeway and the current need
				Want = WrapAngle(Bearing + Clamp(WrapAngle(B.Yaw - std::atan2(B.VY, B.VX)), -15.0 * Deg, 15.0 * Deg));
			}
			const double Off = WrapAngle(Want - From);
			if (std::fabs(Off) < Up)
			{
				const double Side = std::fabs(Off) < 4.0 * Deg ? -double(H.Tack) : Sign(Off);
				Want = WrapAngle(From + Side * Up);
			}
			return Want;
		}

		/** Station keeping: slow reaches to and fro across the wind through (X, Y). Returns false when too far off to start. */
		bool StationPoint(FHelm& H, const FBoat& B, const FBoatDef& D, double From, double X, double Y, double Radius, double& Tx, double& Ty)
		{
			const double Px = X - B.X, Py = Y - B.Y;
			const double PDist = std::sqrt(Px * Px + Py * Py);
			H.bOnStation = PDist < Radius;
			if (PDist > 1.2 * Radius)
			{
				return false;       // hurry back to it
			}
			const double Cx = -std::sin(From), Cy = std::cos(From);
			const double Half = std::max(0.4 * Radius, 1.5 * D.Loa);
			const double UpX = std::cos(From), UpY = std::sin(From);
			Tx = X + H.StationSide * Half * Cx + 0.15 * Radius * UpX;
			Ty = Y + H.StationSide * Half * Cy + 0.15 * Radius * UpY;
			const double Ex = Tx - B.X, Ey = Ty - B.Y;
			if (std::sqrt(Ex * Ex + Ey * Ey) < std::max(0.3 * Radius, 1.5 * D.Loa) || (Ex * Cx + Ey * Cy) * H.StationSide < 0.0)
			{
				H.StationSide = -H.StationSide;
				Tx = X + H.StationSide * Half * Cx + 0.15 * Radius * UpX;
				Ty = Y + H.StationSide * Half * Cy + 0.15 * Radius * UpY;
			}
			H.SpeedScale = 0.55;
			return true;
		}

		/** The order's point and pace; then the heading. */
		void Navigate(FHelm& H, const FBoat& B, bool bFar, bool& bTurn)
		{
			bTurn = false;
			const FBoatDef& D = Def(B.Class);
			const double Tws = std::sqrt(H.FastX * H.FastX + H.FastY * H.FastY);
			const double From = WindFromYaw(H.FastX, H.FastY);
			const FOrder& O = H.Order;
			H.SpeedScale = 1.0;
			H.bOnStation = false;
			const double Px = O.X - B.X, Py = O.Y - B.Y;
			const double PDist = std::sqrt(Px * Px + Py * Py);
			double Tx = O.X, Ty = O.Y;
			switch (O.Kind)
			{
			case EOrder::Idle:
				// lie with the wind on the beam and the sheet flying
				H.Mode = 0;
				H.CourseYaw = WrapAngle(From - H.Tack * 75.0 * Deg);
				H.SpeedScale = 0.0;
				return;
			case EOrder::SailTo:
				if (PDist < O.Radius)
				{
					H.bArrived = true;
				}
				if (H.bArrived && PDist < 1.5 * O.Radius)
				{
					H.SpeedScale = 0.4;
				}
				break;
			case EOrder::Station:
				StationPoint(H, B, D, From, O.X, O.Y, O.Radius, Tx, Ty);
				break;
			case EOrder::Follow:
			{
				const double Lx = std::cos(O.LeaderYaw), Ly = std::sin(O.LeaderYaw);
				const double Sx = O.LeaderX - Lx * O.Gap * O.Slot, Sy = O.LeaderY - Ly * O.Gap * O.Slot;
				const double Ex = Sx - B.X, Ey = Sy - B.Y;
				const double Ed = std::sqrt(Ex * Ex + Ey * Ey);
				const double Mine = std::max(0.3, PolarSpeed(B.Class, TrueWindAngle(O.LeaderYaw, H.FastX, H.FastY), Tws));
				if (O.LeaderSpeed < 0.5)
				{
					// the launch has stopped: nobody can park a dinghy, so each jogs to and fro about its own place
					// (the places fan out across the launch's stern so that the jogging boats have room)
					const double Row = double((O.Slot + 1) / 2), Wing = (O.Slot % 2 ? 1.0 : -1.0) * 1.2 * O.Gap * Row;
					const double Hx = O.LeaderX - Lx * O.Gap * (1.0 + Row) - Ly * Wing, Hy = O.LeaderY - Ly * O.Gap * (1.0 + Row) + Lx * Wing;
					Tx = Hx;
					Ty = Hy;
					StationPoint(H, B, D, From, Hx, Hy, 1.3 * O.Gap, Tx, Ty);
					break;
				}
				H.bOnStation = Ed < 0.8 * O.Gap;
				// aim a little ahead of the slot; in it, keep the leader's pace
				const double Ahead = std::min(Ed, 2.0 * O.Gap) + 4.0 * D.Loa;
				Tx = Sx + Lx * Ahead;
				Ty = Sy + Ly * Ahead;
				const double Along = Ex * Lx + Ey * Ly;     // > 0: the slot is ahead of me
				// (under some 0.8 m/s a dinghy stops answering her helm: no slower than that)
				H.SpeedScale = Clamp((O.LeaderSpeed + 0.25 * Along) / Mine, std::min(1.0, 0.8 / Mine), 1.0);
				break;
			}
			case EOrder::Ashore:
				if (PDist < O.Radius || B.State == EState::Aground)
				{
					H.bArrived = true;
				}
				if (PDist < 25.0)
				{
					H.SpeedScale = 0.5;
				}
				break;
			}
			H.CourseYaw = Heading(H, B, Tx, Ty, bFar, bTurn);
		}

		/** Must I keep clear of this one? (port gives way to starboard, windward to leeward, the overtaking boat to the one ahead) */
		bool GiveWay(const FBoat& B, int MyTack, const FNeighbour& N, double FromYaw)
		{
			// anything not under command: a launch, a mark, a capsized boat, one stopped in irons
			if (N.Tack == 0 || N.VX * N.VX + N.VY * N.VY < 0.6 * 0.6)
			{
				return true;
			}
			if (N.Tack != MyTack)
			{
				return MyTack < 0;
			}
			const double Rx = N.X - B.X, Ry = N.Y - B.Y;
			// overtaking: he is ahead of my beam and I am the faster
			const double Ahead = Rx * std::cos(B.Yaw) + Ry * std::sin(B.Yaw);
			const double Dist = std::sqrt(Rx * Rx + Ry * Ry);
			const double Mine = B.VX * std::cos(B.Yaw) + B.VY * std::sin(B.Yaw);
			const double His = N.VX * std::cos(B.Yaw) + N.VY * std::sin(B.Yaw);
			if (Ahead > 0.8 * Dist && Mine > His)
			{
				return true;
			}
			// I am to windward when he lies downwind of me
			const double Upwind = Rx * std::cos(FromYaw) + Ry * std::sin(FromYaw);
			return Upwind < 0.0;
		}

		/** Looks at the water and the boats ahead and picks how far to turn off the wanted heading. 4 times a second. */
		void LookOut(FHelm& H, const FBoat& B, const FHelmInput& In, double Want, bool bFar)
		{
			const FBoatDef& D = Def(B.Class);
			const double Tws = std::sqrt(H.FastX * H.FastX + H.FastY * H.FastY);
			const double From = WindFromYaw(H.FastX, H.FastY);
			const double Up = BeatAngle(B.Class, std::max(Tws, 1.0));
			const double Sp = std::max(bFar ? B.FarSpeed : B.Out.Sog, 0.8);
			const bool bWater = (In.Poly && In.PolyCount >= 3) || In.DepthFn;
			// landing: the shallows are where we are going
			const double Px = H.Order.X - B.X, Py = H.Order.Y - B.Y;
			const bool bLanding = H.Order.Kind == EOrder::Ashore && Px * Px + Py * Py < 80.0 * 80.0;
			static const double Offs[] = { 0.0, 15.0, -15.0, 30.0, -30.0, 50.0, -50.0, 80.0, -80.0, 120.0, -120.0, 170.0 };
			const int NOffs = int(sizeof(Offs) / sizeof(Offs[0]));
			double Best = 1e9, BestOff = 0.0;
			bool bBestTack = false;
			double ZeroPen = 0.0;
			H.TackPen = 0.0;
			for (int I = 0; I <= NOffs; I++)
			{
				// the last candidate on a beat is the other tack
				const bool bTack = I == NOffs;
				if (bTack && (H.Mode != 1 || H.Maneuver != EManeuver::None))
				{
					break;
				}
				const double Off = bTack ? 0.0 : Offs[I] * Deg;
				const double Yaw = bTack ? WrapAngle(From + H.Tack * Up) : WrapAngle(Want + Off);
				if (!bTack && I > 0 && Tws > 0.5 && std::fabs(WrapAngle(From - Yaw)) < Up - 4.0 * Deg)
				{
					continue;       // nobody sails there
				}
				// a big turn is a manoeuvre, not a dodge
				double Pen = bTack ? 1.2 : std::fabs(Off) * 1.2 + (std::fabs(Off) > 1.0 ? 2.0 : 0.0) + (Off * H.AvoidYaw < 0.0 ? 0.3 : 0.0);
				const double Cx = std::cos(Yaw), Cy = std::sin(Yaw);
				if (bWater && !bLanding)
				{
					const double Look[3] = { 1.5 * D.Loa + 1.0 * Sp, 3.0 * D.Loa + 3.0 * Sp, 5.0 * D.Loa + 6.0 * Sp };
					for (int K = 0; K < 3; K++)
					{
						if (Blocked(In, B.X + Cx * Look[K], B.Y + Cy * Look[K]))
						{
							Pen += 100.0 / (1.0 + K);
							break;
						}
					}
				}
				for (int J = 0; J < In.NeighbourCount; J++)
				{
					const FNeighbour& N = In.Neighbours[J];
					const double Rx = N.X - B.X, Ry = N.Y - B.Y;
					const double R2 = Rx * Rx + Ry * Ry;
					if (R2 > 60.0 * 60.0)
					{
						continue;
					}
					const bool bMust = GiveWay(B, H.Tack, N, From);
					const double Len = 0.5 * (D.Loa + N.Length);
					// the give-way boat keeps a length of clear water; the right-of-way boat only acts at the last moment
					const double Safe = bMust ? 1.7 * Len + 0.5 : 1.1 * Len;
					const double Horizon = bMust ? 8.0 : 2.5;
					const double Vx = N.VX - Sp * Cx, Vy = N.VY - Sp * Cy;
					const double V2 = Vx * Vx + Vy * Vy;
					const double Closing = -(Rx * Vx + Ry * Vy);
					if (Closing <= 0.0)
					{
						// drawing apart: only mind him if he is alongside
						if (R2 < 0.8 * Len * 0.8 * Len)
						{
							Pen += 4.0;
						}
						continue;
					}
					const double T = V2 > 1e-4 ? std::min(Closing / V2, Horizon) : 0.0;
					const double Mx = Rx + Vx * T, My = Ry + Vy * T;
					const double Miss = std::sqrt(Mx * Mx + My * My);
					if (Miss < Safe)
					{
						Pen += 40.0 * (1.0 - Miss / Safe) / (1.0 + 0.4 * T) + 1.5;
					}
				}
				if (I == 0)
				{
					ZeroPen = Pen;
				}
				if (bTack)
				{
					H.TackPen = Pen;
				}
				if (Pen < Best)
				{
					Best = Pen;
					BestOff = Off;
					bBestTack = bTack;
				}
			}
			// once dodging, keep dodging the same way for a moment
			if (ZeroPen < 1.0 && H.AvoidHold > 0.0)
			{
				H.AvoidHold -= 0.25;
			}
			else if (ZeroPen < 1.0)
			{
				H.AvoidYaw = 0.0;
			}
			else
			{
				H.AvoidYaw = bBestTack ? 0.0 : BestOff;
				H.AvoidHold = 1.0;
			}
			H.bAvoidTack = bBestTack && ZeroPen >= 1.0;
			// nowhere good to go: spill the wind and slow down
			H.AvoidSlow = Best > 20.0 ? 0.4 : (ZeroPen >= 1.0 && Best > 6.0 ? 0.7 : 1.0);
		}
	}

	void HelmInit(FHelm& H, double Skill, uint32_t Seed)
	{
		H = FHelm();
		H.Skill = Clamp(Skill, 0.0, 1.0);
		H.Seed = Seed ? Seed : 1u;
		uint32_t S = H.Seed * 2654435761u + 12345u;
		S = S ? S : 1u;
		Rand01(S);
		H.Overstand = (2.0 + 5.0 * Rand01(S) + 5.0 * (1.0 - H.Skill)) * Deg;
		H.HeelPref = 0.19 + 0.08 * Rand01(S);
		H.React = 0.12 + 1.3 * (1.0 - H.Skill) * (0.6 + 0.8 * Rand01(S));
		H.Timid = (1.0 - H.Skill) * (0.3 + 0.7 * Rand01(S));
		H.HeaderDeg = 7.0 + 6.0 * Rand01(S);
		H.Wobble = (1.0 - H.Skill) * (0.5 + Rand01(S));
		H.StationSide = Rand01(S) < 0.5 ? -1 : 1;
		H.Seed = S;
	}

	void HelmOrder(FHelm& H, const FOrder& Order)
	{
		H.Order = Order;
		H.bArrived = false;
		H.bOnStation = false;
	}

	void HelmFar(FHelm& H, const FBoat& B, const FHelmInput& In, double Dt)
	{
		Perceive(H, B, In, Dt);
		if (B.State != EState::Sailing && B.State != EState::Swamped)
		{
			H.SpeedScale = 0.0;
			return;
		}
		const double Twa = TrueWindAngle(B.Yaw, H.FastX, H.FastY);
		if (H.SinceManeuver > 8.0 && std::fabs(Twa) > 15.0 * Deg)
		{
			H.Tack = Twa >= 0.0 ? 1 : -1;
		}
		H.Maneuver = EManeuver::None;
		// the feel of the boat, kept fresh for the moment the full model takes her back
		H.PushSeen = B.HeelMoment / (B.CrewMass * Gravity * Def(B.Class).HikeArm);
		H.PowerFull = std::fabs(H.PushSeen);
		H.HeelSeen = B.Heel;
		H.Ease = std::max(0.0, H.Ease - 0.2 * Dt);
		H.PaceShare = 1.0;
		bool bTurn = false;
		Navigate(H, B, true, bTurn);
		H.AvoidClock += Dt;
		if (H.AvoidClock >= 1.0)
		{
			H.AvoidClock = 0.0;
			LookOut(H, B, In, H.CourseYaw, true);
			if (H.bAvoidTack && H.SinceManeuver > 4.0)
			{
				bTurn = true;
			}
		}
		if (bTurn)
		{
			H.Tack = -H.Tack;
			H.SinceManeuver = 0.0;
			(H.Mode == 2 ? H.Gybes : H.Tacks)++;
			bool bAgain = false;
			Navigate(H, B, true, bAgain);
		}
		H.CourseYaw = WrapAngle(H.CourseYaw + H.AvoidYaw);
		H.SpeedScale *= H.AvoidSlow;
		if (B.State == EState::Swamped)
		{
			H.SpeedScale *= 0.6;
		}
	}

	FControls HelmUpdate(FHelm& H, const FBoat& B, const FHelmInput& In, double Dt)
	{
		FControls C;
		const FBoatDef& D = Def(B.Class);
		const FOutputs& O = B.Out;
		Perceive(H, B, In, Dt);
		const double Tws = std::sqrt(H.FastX * H.FastX + H.FastY * H.FastY);
		const double From = WindFromYaw(H.FastX, H.FastY);
		const double BoardMax = Clamp((In.Depth - D.HullDraft - 0.15) / D.BoardLen, 0.0, 1.0);

		switch (B.State)
		{
		case EState::Capsized:
		case EState::Turtled:
			// a moment in the water, then onto the board
			H.DownTime += Dt;
			H.Maneuver = EManeuver::None;
			H.Ease = 0.6;
			C.Sheet = 1.0;
			C.bRight = H.DownTime > 2.0 + 5.0 * (1.0 - H.Skill);
			return C;
		case EState::Righting:
			C.Sheet = 1.0;
			C.bRight = true;
			return C;
		case EState::Aground:
			// board up and she floats off; then away from the shallows by the look-out
			H.DownTime = 0.0;
			C.Sheet = 1.0;
			C.Board = std::min(BoardMax, std::max(0.0, B.Board - 0.3));
			if (H.Order.Kind == EOrder::Ashore)
			{
				H.bArrived = true;
			}
			return C;
		case EState::Ashore:
		case EState::Moored:
		case EState::Towed:
			H.DownTime = 0.0;
			C.Sheet = 1.0;
			C.Board = B.State == EState::Towed ? std::min(0.3, BoardMax) : 0.0;
			return C;
		default:
			break;
		}
		H.DownTime = 0.0;

		const double Twa = TrueWindAngle(B.Yaw, H.FastX, H.FastY);
		const double AbsTwa = std::fabs(Twa);
		if (H.Maneuver == EManeuver::None && H.SinceManeuver > 3.0 && AbsTwa > 15.0 * Deg)
		{
			H.Tack = Twa >= 0.0 ? 1 : -1;
		}
		const int StationWas = H.StationSide;
		bool bTurn = false;
		Navigate(H, B, false, bTurn);
		if (H.Mode == 1 && H.Maneuver == EManeuver::None)
		{
			const double Shift = WrapAngle(From - WindFromYaw(H.SlowX, H.SlowY));
			H.HeaderTime = -H.Tack * Shift > H.HeaderDeg * Deg ? H.HeaderTime + Dt : 0.0;
		}
		else
		{
			H.HeaderTime = 0.0;
		}

		// the look-out
		H.AvoidClock += Dt;
		if (H.AvoidClock >= 0.25)
		{
			H.AvoidClock = 0.0;
			LookOut(H, B, In, H.CourseYaw, false);
		}
		if (H.bAvoidTack && H.Maneuver == EManeuver::None && H.SinceManeuver > 4.0 && H.Mode == 1)
		{
			bTurn = true;
			H.bAvoidTack = false;
		}

		const double Up = BeatAngle(B.Class, std::max(Tws, 1.0)) + 2.0 * Deg;
		// turning round on station: through the wind
		if (H.Order.Kind == EOrder::Station && H.StationSide != StationWas && H.Maneuver == EManeuver::None && Tws > 0.5)
		{
			const double Other = WrapAngle(From + H.Tack * Up);
			// only when the new reach is on the other tack
			if (std::fabs(WrapAngle(H.CourseYaw - Other)) < 0.5 * Pi)
			{
				H.Tack = -H.Tack;
				H.Maneuver = EManeuver::Tack;
				H.ManeuverTime = 0.0;
			}
		}
		// do not tack into somebody: hold on a few seconds
		if (bTurn && H.Mode == 1 && H.TackPen > 6.0 && H.TackHold < 6.0)
		{
			H.TackHold += Dt;
			bTurn = false;
		}
		if (bTurn && H.Maneuver == EManeuver::None)
		{
			H.TackHold = 0.0;
			if (H.Mode == 2)
			{
				// a beginner in a blow goes round the long way, through the wind
				if (Tws > 7.5 && H.Skill < 0.45)
				{
					H.Maneuver = EManeuver::WearRound;
					H.WearPhase = 0;
					H.WearYaw = WrapAngle(From - H.Tack * (Up + 4.0 * Deg));
				}
				else
				{
					H.Maneuver = EManeuver::Gybe;
				}
				H.Tack = -H.Tack;
			}
			else
			{
				H.Maneuver = EManeuver::Tack;
				H.Tack = -H.Tack;
			}
			H.ManeuverTime = 0.0;
			bool bAgain = false;
			Navigate(H, B, false, bAgain);
		}

		// in irons? (head to wind and stopped)
		if (AbsTwa < 50.0 * Deg && O.Speed < 0.25 && Tws > 0.8 && H.Maneuver != EManeuver::Irons)
		{
			H.IronsTime += Dt;
			if (H.IronsTime > 1.2)
			{
				H.Maneuver = EManeuver::Irons;
				H.ManeuverTime = 0.0;
				H.WearPhase = 0;
				if (AbsTwa > 3.0 * Deg)
				{
					H.Tack = Twa >= 0.0 ? 1 : -1;
				}
			}
		}
		else
		{
			H.IronsTime = 0.0;
		}

		double Want = H.CourseYaw;
		bool bSheetOut = false, bSheetHalf = false;
		switch (H.Maneuver)
		{
		case EManeuver::None:
			break;
		case EManeuver::Tack:
			H.ManeuverTime += Dt;
			Want = WrapAngle(From - H.Tack * (Up + 6.0 * Deg));     // a little free to gather way
			if ((Twa * H.Tack > 0.0 && AbsTwa > Up - 8.0 * Deg) || H.ManeuverTime > 14.0)
			{
				if (H.ManeuverTime <= 14.0)
				{
					H.Tacks++;
				}
				H.Maneuver = EManeuver::None;
				H.SinceManeuver = 0.0;
			}
			break;
		case EManeuver::Gybe:
			H.ManeuverTime += Dt;
			bSheetHalf = H.Skill > 0.5 && AbsTwa > 150.0 * Deg;
			if ((B.BoomSide == -H.Tack && Twa * H.Tack > 0.0) || H.ManeuverTime > 14.0)
			{
				if (H.ManeuverTime <= 14.0)
				{
					H.Gybes++;
				}
				H.Maneuver = EManeuver::None;
				H.SinceManeuver = 0.0;
			}
			break;
		case EManeuver::WearRound:
			H.ManeuverTime += Dt;
			if (H.WearPhase == 0)
			{
				// up to close-hauled on the old tack first
				Want = WrapAngle(From + H.Tack * (Up + 4.0 * Deg));
				if (std::fabs(WrapAngle(Want - B.Yaw)) < 20.0 * Deg || H.ManeuverTime > 12.0)
				{
					H.Maneuver = EManeuver::Tack;
					H.ManeuverTime = 0.0;
				}
			}
			break;
		case EManeuver::Irons:
			// let the sheet fly and steer backwards until she has paid off; then sheet in on a reach and gather way
			H.ManeuverTime += Dt;
			// (with sternway on, right round to beam-on before the sheet comes in, or she luffs again before she moves)
			H.WearPhase = AbsTwa > (H.WearPhase ? 50.0 : (O.Speed < -0.2 ? 90.0 : 62.0)) * Deg && Twa * H.Tack > 0.0 ? 1 : 0;
			bSheetOut = H.WearPhase == 0;
			Want = WrapAngle(From - H.Tack * (H.WearPhase ? 80.0 : 105.0) * Deg);
			if ((H.WearPhase == 1 && O.Speed > std::max(0.5, 0.4 * PolarSpeed(B.Class, 75.0 * Deg, Tws))) || H.ManeuverTime > 90.0)
			{
				H.Maneuver = EManeuver::None;
				H.SinceManeuver = 0.0;
			}
			break;
		}
		// no way on: off the wind a little until she moves
		if (H.Mode == 1 && H.Maneuver == EManeuver::None && Tws > 1.0 && O.Speed < 0.45 * PolarSpeed(B.Class, Up, Tws))
		{
			Want = WrapAngle(From - H.Tack * (Up + 14.0 * Deg));
		}

		// what the helmsman feels of the heel, late if he is green
		H.HeelSeen += (B.HeelSlow - H.HeelSeen) * std::min(1.0, Dt / H.React);
		H.PushSeen += (B.HeelMoment / (B.CrewMass * Gravity * D.HikeArm) - H.PushSeen) * std::min(1.0, Dt / (0.1 + 0.6 * H.React));
		const double AbsHeel = std::fabs(H.HeelSeen);
		const double LeeHeel = H.HeelSeen * Sign(H.PushSeen);

		// ---- tiller
		{
			double W = WrapAngle(Want + (H.Maneuver == EManeuver::None ? H.AvoidYaw : 0.0));
			if (H.Mode == 1 && H.Maneuver == EManeuver::None && H.Skill > 0.5)
			{
				// a good helmsman feathers up in the puffs
				W = WrapAngle(W + H.Tack * Clamp((AbsHeel - H.HeelPref) * 0.6, 0.0, 6.0 * Deg) * H.Skill);
			}
			if (H.Mode != 1 && AbsTwa > 140.0 * Deg && H.Maneuver == EManeuver::None && Tws > 6.0)
			{
				// rolling to windward on a run: head up before she gybes herself
				const double ToWindward = std::max(0.0, H.HeelSeen * H.Tack);
				W = WrapAngle(W + H.Tack * H.Skill * Clamp(ToWindward * 1.2, 0.0, 15.0 * Deg));
			}
			const double Err = WrapAngle(W - B.Yaw);
			double Rud = 1.8 * Err - 0.9 * B.YawRate;
			if (O.Speed < -0.05)
			{
				Rud = -Rud;     // going astern the tiller works the other way
				if (H.Skill < 0.3 && H.Maneuver == EManeuver::Irons && H.ManeuverTime < 5.0 * (1.0 - H.Skill))
				{
					Rud = 0.0;  // and a beginner needs a while to remember that
				}
			}
			Rud += 0.10 * H.Wobble * std::sin(H.Clock * 0.9 + double(H.Seed & 255u));
			const double MaxRud = H.Maneuver != EManeuver::None ? 1.0 : Clamp(2.4 / (0.6 + std::fabs(O.Speed)), 0.35, 1.0);
			C.Rudder = Clamp(Rud, -MaxRud, MaxRud);
		}

		// ---- sheet
		{
			const double AbsBeta = std::fabs(O.AppWindAngle);
			const double Over = LeeHeel - (H.HeelPref - 0.07 * H.Timid);
			// lying over too far for too long: carry less sail (a slow hand on top of the quick one below)
			H.Ease = Clamp(H.Ease + Dt * (Over > 0.0 ? (1.0 + 2.0 * H.Skill) * Over + 0.1 : (Over < -0.05 ? -0.2 : 0.0)), 0.0, 1.0);
			// carry what the crew can hold down (a green crew does not lean out all the way; a timid one carries less in a blow)
			double Target = (0.2 + 0.95 * (0.6 + 0.4 * H.Skill) - 0.3 * H.Timid * Clamp((Tws - 4.0) / 5.0, 0.0, 1.0)) * (1.0 - 0.8 * H.Ease);
			// the tiller hard over to bear away and she will not go: ease, and she will
			Target *= 1.0 - 0.75 * Smooth(0.5, 0.95, std::max(0.0, -C.Rudder * Sign(Twa)));
			// holding back: spill wind until the pace is right
			const double Pace = H.SpeedScale * H.AvoidSlow * (B.State == EState::Swamped ? 0.7 : 1.0);
			if (Pace < 0.99)
			{
				const double Full = std::max(0.3, PolarSpeed(B.Class, Twa, Tws));
				H.PaceShare = Clamp(H.PaceShare + Dt * 0.8 * (Pace * Full - O.Speed) / std::max(0.5, Full), 0.03, 1.0);
			}
			else
			{
				H.PaceShare = std::min(1.0, H.PaceShare + Dt * 0.5);
			}
			// the quick hand: feel the load, ease to what the crew can hold (a green hand feels it late)
			const double Push = std::fabs(H.PushSeen);
			double Boom = TrimBoom(D, AbsBeta, std::fabs(B.Boom), Push, Target, H.PowerFull, std::min(1.0, Dt / (0.15 + 0.5 * H.React)));
			if (H.PaceShare < 0.99)
			{
				Boom = std::max(Boom, Clamp(AbsBeta - AlphaForPull(H.PaceShare * H.PaceShare), D.BoomMin, D.BoomMax));
			}
			Boom += 0.2 * (1.0 - H.Skill) * std::sin(H.Clock / 7.0 + double((H.Seed >> 8) & 255u));
			C.Sheet = SheetForBoom(D, Boom);
			if (bSheetHalf)
			{
				C.Sheet = std::min(C.Sheet, 0.45);
			}
			if (bSheetOut || H.Order.Kind == EOrder::Idle)
			{
				C.Sheet = 1.0;
			}
			if (H.Order.Kind == EOrder::Ashore && H.bArrived)
			{
				C.Sheet = 1.0;
			}
		}

		// ---- crew weight, board, bailer
		C.Hike = -Clamp(H.PushSeen + (B.Heel - B.WaveRoll) / 1.5, -1.0, 1.0) * (0.6 + 0.4 * H.Skill);
		C.Board = H.Mode == 1 || AbsTwa < 65.0 * Deg ? 1.0 : (AbsTwa < 130.0 * Deg ? 0.7 : (H.Skill > 0.5 ? 0.4 : 0.8));
		C.Board = std::min(C.Board, BoardMax);
		C.bBail = B.Water > 0.25 || (B.State == EState::Swamped && B.Water > 0.05);
		return C;
	}

	// ---- the coach's launch -------------------------------------------------------------------------------------------

	void StepLaunch(FLaunch& L, double Throttle, double Turn, const FEnv& E, double Dt)
	{
		if (!(Dt > 0.0))
		{
			return;
		}
		Throttle = Clamp(Throttle, -1.0, 1.0);
		Turn = Clamp(Turn, -1.0, 1.0);
		L.Throttle = Toward(L.Throttle, Throttle, 1.5 * Dt);
		// displacement up to ~3 m/s, a hump, then the plane
		const double T = std::fabs(L.Throttle);
		double Target = T < 0.4 ? 3.2 * T / 0.4 : 3.2 + (LaunchTopSpeed - 3.2) * Smooth(0.4, 1.0, T) * (0.35 + 0.65 * (T - 0.4) / 0.6);
		if (L.Throttle < 0.0)
		{
			Target = -1.6 * T;
		}
		const double Tau = Target > L.Speed ? (L.Speed > 3.0 && L.Speed < 5.5 ? 3.5 : 2.2) : 1.6;
		L.Speed += (Target - L.Speed) * std::min(1.0, Dt / Tau);
		const double Grip = Clamp(std::fabs(L.Speed) / 1.0, 0.0, 1.0);
		const double RateT = Turn * Grip * std::min(0.9, 2.6 / (1.0 + 0.3 * std::fabs(L.Speed))) * (L.Speed < 0.0 ? -1.0 : 1.0);
		L.YawRate += (RateT - L.YawRate) * std::min(1.0, Dt / 0.4);
		L.Yaw = WrapAngle(L.Yaw + L.YawRate * Dt);
		// stopped, it drifts a little with the wind
		const double Drift = 0.02 * (1.0 - Grip);
		L.VX = std::cos(L.Yaw) * L.Speed + E.CurrentX + Drift * E.WindX;
		L.VY = std::sin(L.Yaw) * L.Speed + E.CurrentY + Drift * E.WindY;
		L.X += L.VX * Dt;
		L.Y += L.VY * Dt;
		L.Planing = Smooth(4.0, 6.5, L.Speed);
		const double HeelT = Clamp(L.YawRate * L.Speed * 0.05, -0.35, 0.35);       // banks into the turn
		L.Heel += (HeelT - L.Heel) * std::min(1.0, Dt / 0.3);
		const double Cy = std::cos(L.Yaw), Sy = std::sin(L.Yaw);
		const double Wave = -0.7 * std::atan2(E.NormalX * Cy + E.NormalY * Sy, std::max(E.NormalZ, 0.2));
		const double Hump = Smooth(1.5, 4.5, L.Speed) * (1.0 - 0.6 * L.Planing);
		const double PitchT = 0.11 * Hump + 0.04 * L.Planing + Wave;
		L.Pitch += (PitchT - L.Pitch) * std::min(1.0, Dt / 0.4);
		L.Wake = Clamp(std::fabs(L.Speed) / 5.0, 0.0, 1.0);
		L.EngineLoad = Clamp(0.15 + 0.85 * T + 0.3 * Clamp((Target - L.Speed) / 4.0, 0.0, 1.0), 0.0, 1.0);
	}

	void LaunchDriveTo(const FLaunch& L, double TX, double TY, double Cruise, double Radius, double& OutThrottle, double& OutTurn)
	{
		const double Dx = TX - L.X, Dy = TY - L.Y;
		const double Dist = std::sqrt(Dx * Dx + Dy * Dy);
		if (Dist < Radius)
		{
			OutThrottle = 0.0;
			OutTurn = 0.0;
			return;
		}
		const double Err = WrapAngle(std::atan2(Dy, Dx) - L.Yaw);
		OutTurn = Clamp(1.6 * Err - 0.5 * L.YawRate, -1.0, 1.0);
		// slow for the turn and for the arrival
		double Want = std::min(Cruise, 0.6 + 0.35 * (Dist - Radius));
		Want *= 1.0 - 0.6 * Smooth(0.5, 1.6, std::fabs(Err));
		// the lever for a speed: the inverse of StepLaunch's curve, near enough
		double Lever = Want <= 3.2 ? 0.4 * Want / 3.2 : 0.4 + 0.6 * std::sqrt(Clamp((Want - 3.2) / (LaunchTopSpeed - 3.2), 0.0, 1.0));
		Lever += 0.15 * Clamp(Want - L.Speed, -2.0, 2.0);
		OutThrottle = Clamp(Lever, 0.0, 1.0);
	}
}
