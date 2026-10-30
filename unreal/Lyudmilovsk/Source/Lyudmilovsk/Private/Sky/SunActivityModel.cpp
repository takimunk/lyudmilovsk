#include "SunActivityModel.h"

#include <algorithm>
#include <cmath>

namespace LyudSunActivity
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double TwoPi = 2.0 * Pi;

		// slots: regions by game hours, prominences and flares by activity hours
		constexpr double RegionSlotH = 6.0;
		constexpr double RegionMaxLifeDays = 10.0;
		constexpr double LoopSlotH = 1.0;
		constexpr double LoopLookbackH = 10.0;        // the longest loop life + its eruption
		constexpr double QuietSlotH = 12.0;
		constexpr double QuietLookbackH = 14.0 * 24.0 + 4.0;
		constexpr double FlareSlotH = 0.25;
		constexpr double FlareLookbackH = 1.25;
		constexpr int MaxAlive = 64;

		uint32_t Mix(uint32_t H)
		{
			H ^= H >> 16; H *= 0x7feb352dU; H ^= H >> 15; H *= 0x846ca68bU; H ^= H >> 16;
			return H;
		}

		/** 0..1 from a seed, a stream tag, a slot and two small indices. */
		float H(uint32_t Seed, uint32_t Tag, int64_t Slot, uint32_t A = 0, uint32_t B = 0)
		{
			uint32_t X = Mix(Seed ^ Mix(Tag * 0x9e3779b9U));
			X = Mix(X ^ uint32_t(Slot) ^ Mix(uint32_t(uint64_t(Slot) >> 32) + 0x632be5abU));
			X = Mix(X ^ Mix(A * 0x85ebca6bU + 1U));
			X = Mix(X ^ Mix(B * 0xc2b2ae35U + 7U));
			return float(X >> 8) * (1.f / 16777216.f);
		}

		float Clamp01(float X) { return X < 0.f ? 0.f : (X > 1.f ? 1.f : X); }
		float Smooth(float A, float B, float X)
		{
			const float T = Clamp01((X - A) / (B - A));
			return T * T * (3.f - 2.f * T);
		}

		struct FV { double X, Y, Z; };
		FV Add(FV A, FV B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
		FV Mul(FV A, double K) { return { A.X * K, A.Y * K, A.Z * K }; }
		double Dot(FV A, FV B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
		FV Cross(FV A, FV B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
		FV Norm(FV A) { const double L = std::sqrt(std::max(Dot(A, A), 1e-30)); return Mul(A, 1.0 / L); }

		/** A point at (lat, lon) of the star frame, in the view frame at a spin. */
		FV ViewOf(double Lat, double Lon, double Spin)
		{
			const double C = std::cos(Lat);
			return { C * std::sin(Lon - Spin), std::sin(Lat), C * std::cos(Lon - Spin) };
		}

		/** The local east (growing star longitude) at (lat, lon), in the view frame. */
		FV EastOf(double Lon, double Spin)
		{
			return { std::cos(Lon - Spin), 0.0, -std::sin(Lon - Spin) };
		}

		struct FAliveRegion
		{
			double Lat, Lon;
			float Strength, Seed;
			int Id;
		};

		void Store(const FV& V, float* Out) { Out[0] = float(V.X); Out[1] = float(V.Y); Out[2] = float(V.Z); }

		/** How much of a prominence shows: off the limb (emission against the sky), or in front of the disk (a dark
		 * filament, weaker). */
		float VisibilityOf(const FV& A, float HeightNow, float HalfWidth, EPromKind Kind)
		{
			const double Rho = std::sqrt(std::max(0.0, 1.0 - A.Z * A.Z));
			// the foot spreads over HalfWidth: its ends can reach the limb before its middle does
			const double Reach = (Rho + HalfWidth * std::sqrt(std::max(0.0, A.Z * A.Z))) * (1.0 + HeightNow);
			const float Limb = Smooth(1.0f, 1.0f + 0.35f * HeightNow + 0.01f, float(Reach));
			const float Filament = A.Z > 0.0 ? (Kind == EPromKind::Hedgerow ? 0.35f : 0.12f) * Smooth(0.0f, 0.3f, float(A.Z)) : 0.f;
			return std::max(Limb, Filament);
		}

		/** A flare's light curve: a quick rise, then an exponential decay. Minutes on the activity clock. */
		float FlareCurve(float T, float Rise, float Decay)
		{
			if (T < 0.f)
			{
				return 0.f;
			}
			if (T < Rise)
			{
				const float X = T / Rise;
				return X * X * (3.f - 2.f * X);
			}
			return std::exp(-(T - Rise) / Decay);
		}
	}

	double ActivityHours(double GameHours, const FSettings& S)
	{
		return GameHours * std::max(double(S.TimeScale), 0.0);
	}

	double SpinAt(double GameHours, const FSettings& S)
	{
		const double Turns = GameHours / 24.0 / std::max(S.RotationDays, 0.01);
		return (Turns - std::floor(Turns)) * TwoPi;
	}

	void Evaluate(double GameHours, const FSettings& S, FState& Out, const FForcedFlare* Forced)
	{
		Out = FState();
		const double Spin = SpinAt(GameHours, S);
		Out.Spin = Spin;
		const float Act = std::max(S.Activity, 0.f);
		const double OmegaEq = TwoPi / std::max(S.RotationDays, 0.01);   // rad per day
		const double Ah = ActivityHours(GameHours, S);

		// --- active regions -------------------------------------------------------------------------------------
		FAliveRegion Alive[MaxAlive];
		int NumAlive = 0;
		const int64_t K1 = int64_t(std::floor(GameHours / RegionSlotH));
		const int64_t K0 = K1 - int64_t(std::ceil(RegionMaxLifeDays * 24.0 / RegionSlotH)) - 1;
		for (int64_t K = K0; K <= K1 && NumAlive < MaxAlive; ++K)
		{
			for (uint32_t B = 0; B < 2; ++B)
			{
				// ~1.6 births a day, so ~8 regions over the whole star at a time
				const float Rate = (B == 0 ? 0.32f : 0.08f) * Act;
				if (H(S.Seed, 1, K, B) >= Rate)
				{
					continue;
				}
				const double T0 = (double(K) + H(S.Seed, 2, K, B)) * RegionSlotH;
				const double Life = (2.0 + 8.0 * std::pow(H(S.Seed, 3, K, B), 1.5)) * 24.0;
				const double Age = GameHours - T0;
				if (Age < 0.0 || Age > Life)
				{
					continue;
				}
				// two belts (10..45 degrees) and, for an M dwarf, high latitudes too (50..75)
				const bool bBelt = H(S.Seed, 4, K, B) < 0.7f;
				const double LatMag = bBelt ? 10.0 + 35.0 * H(S.Seed, 5, K, B) : 50.0 + 25.0 * H(S.Seed, 5, K, B);
				const double Lat = (H(S.Seed, 6, K, B) < 0.5f ? -1.0 : 1.0) * LatMag * Pi / 180.0;
				const double Lon0 = H(S.Seed, 7, K, B) * TwoPi;
				const float Peak = 0.3f + 0.7f * H(S.Seed, 8, K, B) * H(S.Seed, 8, K, B);
				// it emerges in about a tenth of its life, then decays slowly
				const float A01 = float(Age / Life);
				const float Grow = Smooth(0.f, 0.12f, A01);
				const float Decay = A01 < 0.12f ? 1.f : std::pow(Clamp01(1.f - (A01 - 0.12f) / 0.88f), 0.7f);
				const float Strength = Peak * Grow * Decay;
				if (Strength < 0.02f)
				{
					continue;
				}
				// differential rotation: high latitudes lag the equator (and the star frame turns with the equator)
				const double SinLat = std::sin(Lat);
				const double Lon = Lon0 - OmegaEq * S.DiffRotation * SinLat * SinLat * (Age / 24.0);
				FAliveRegion& R = Alive[NumAlive++];
				R.Lat = Lat;
				R.Lon = Lon;
				R.Strength = Strength;
				R.Seed = H(S.Seed, 9, K, B) * 1000.f;
				R.Id = int(K * 2 + B);
			}
		}
		Out.AliveRegions = NumAlive;
		{
			// the sky material gets the most visible ones
			int Order[MaxAlive];
			float Score[MaxAlive];
			for (int I = 0; I < NumAlive; ++I)
			{
				Order[I] = I;
				Score[I] = Alive[I].Strength * Smooth(-0.25f, 0.15f, float(ViewOf(Alive[I].Lat, Alive[I].Lon, Spin).Z));
			}
			std::sort(Order, Order + NumAlive, [&](int A, int B) { return Score[A] > Score[B]; });
			for (int I = 0; I < NumAlive && Out.NumRegions < MaxRegions; ++I)
			{
				const FAliveRegion& A = Alive[Order[I]];
				if (Score[Order[I]] <= 0.f)
				{
					break;
				}
				FRegion& R = Out.Regions[Out.NumRegions++];
				R.Lat = float(A.Lat);
				R.Lon = float(std::fmod(A.Lon, TwoPi));
				R.Strength = A.Strength;
				R.Seed = A.Seed;
				R.ViewZ = float(ViewOf(A.Lat, A.Lon, Spin).Z);
				R.Id = A.Id;
			}
		}

		// --- prominences ----------------------------------------------------------------------------------------
		struct FCand { FProminence P; float Score; };
		FCand Cands[256];
		int NumCands = 0;
		auto Emit = [&](double Lat, double Lon, double TurnRad, float HalfWidth, float Height, float Bright, float Lift,
			float Seed, float Life01, EPromKind Kind, bool bErupting)
		{
			if (Bright <= 0.005f)
			{
				return;
			}
			++Out.AliveProms;
			Out.Eruptions += bErupting ? 1 : 0;
			const FV A = ViewOf(Lat, Lon, Spin);
			const FV E = EastOf(Lon, Spin);
			const FV N = Cross(A, E);                                    // local north
			const FV T = Norm(Add(Mul(E, std::cos(TurnRad)), Mul(N, std::sin(TurnRad))));
			const float Vis = VisibilityOf(A, Height + Lift, HalfWidth, Kind);
			if (Vis > 0.5f && A.Z < 0.4)
			{
				++Out.LimbProms;
			}
			if (Vis <= 0.01f || NumCands >= 256)
			{
				return;
			}
			FCand& C = Cands[NumCands++];
			Store(A, C.P.A);
			Store(T, C.P.T);
			C.P.HalfWidth = HalfWidth;
			C.P.Height = Height;
			C.P.Bright = Bright;
			C.P.Lift = Lift;
			C.P.Seed = Seed;
			C.P.Life01 = Life01;
			C.P.Visibility = Vis;
			C.P.Kind = Kind;
			C.P.bErupting = bErupting;
			C.Score = Bright * Vis;
		};

		// loop systems and surges on the active regions: hours
		const int64_t J1 = int64_t(std::floor(Ah / LoopSlotH));
		const int64_t J0 = J1 - int64_t(std::ceil(LoopLookbackH / LoopSlotH));
		for (int I = 0; I < NumAlive; ++I)
		{
			const FAliveRegion& R = Alive[I];
			const uint32_t Rid = uint32_t(R.Id);
			for (int64_t J = J0; J <= J1; ++J)
			{
				if (H(S.Seed, 10, J, Rid) >= 0.7f * R.Strength * Act)
				{
					continue;
				}
				const bool bSurge = H(S.Seed, 12, J, Rid) < 0.25f;
				const double Born = (double(J) + H(S.Seed, 11, J, Rid)) * LoopSlotH;
				const double Life = bSurge ? 0.25 + 0.6 * H(S.Seed, 13, J, Rid) : 1.5 + 6.5 * std::pow(H(S.Seed, 13, J, Rid), 2.0);
				const bool bEruptive = !bSurge && H(S.Seed, 14, J, Rid) < 0.2f;
				const double EruptAt = Life * (0.5 + 0.3 * H(S.Seed, 15, J, Rid));
				const double EruptDur = 0.4 + 1.0 * H(S.Seed, 16, J, Rid);
				const double Age = Ah - Born;
				const double End = bEruptive ? EruptAt + EruptDur : Life;
				if (Age < 0.0 || Age > End)
				{
					continue;
				}
				const double Lat = R.Lat + (H(S.Seed, 17, J, Rid) - 0.5) * 0.06;
				const double Lon = R.Lon + (H(S.Seed, 18, J, Rid) - 0.5) * 0.08 / std::max(std::cos(Lat), 0.2);
				const double Turn = (H(S.Seed, 19, J, Rid) - 0.5) * Pi;
				float HalfWidth = bSurge ? 0.008f : 0.05f + 0.09f * H(S.Seed, 20, J, Rid);
				float Height = bSurge ? 0.2f + 0.3f * H(S.Seed, 21, J, Rid) : 0.08f + 0.14f * H(S.Seed, 21, J, Rid);
				const float Seed = H(S.Seed, 22, J, Rid) * 1000.f;
				const float A01 = float(Age / Life);
				float Bright, Lift = 0.f;
				bool bErupting = false;
				if (bSurge)
				{
					// a jet that shoots up and falls back
					const float Up = float(std::sin(Pi * Clamp01(A01)));
					Height *= Up;
					Bright = 0.9f * std::sqrt(Up);
				}
				else
				{
					Height *= 0.5f + 0.5f * Smooth(0.f, 0.25f, A01);
					Bright = Smooth(0.f, 0.15f, A01) * (bEruptive ? 1.f : 1.f - Smooth(0.8f, 1.f, A01));
					Bright *= 0.85f + 0.15f * float(std::sin(Ah * TwoPi / 0.7 + Seed));
					Bright *= 0.6f + 0.4f * R.Strength;
					if (bEruptive && Age > EruptAt)
					{
						// it lifts off, accelerating, swells and thins out
						const float Tau = float((Age - EruptAt) / EruptDur);
						Lift = 0.3f * Tau + 3.f * Tau * Tau;
						Height *= 1.f + 2.f * Tau;
						HalfWidth *= 1.f + 1.5f * Tau;
						Bright *= (1.f + 0.4f * Smooth(0.f, 0.1f, Tau)) * std::exp(-2.5f * Tau) * (1.f - Smooth(0.85f, 1.f, Tau));
						bErupting = true;
					}
				}
				Emit(Lat, Lon, Turn, HalfWidth, Height, Bright, Lift, Seed, Clamp01(A01), bSurge ? EPromKind::Surge : EPromKind::Loop, bErupting);
			}
		}

		// quiescent hedgerows: days to weeks, middle and high latitudes (the polar crown), lying along the parallels
		const int64_t Q1 = int64_t(std::floor(Ah / QuietSlotH));
		const int64_t Q0 = Q1 - int64_t(std::ceil(QuietLookbackH / QuietSlotH));
		for (int64_t Q = Q0; Q <= Q1; ++Q)
		{
			for (uint32_t B = 0; B < 2; ++B)
			{
				if (H(S.Seed, 30, Q, B) >= (B == 0 ? 0.4f : 0.12f) * Act)
				{
					continue;
				}
				const double Born = (double(Q) + H(S.Seed, 31, Q, B)) * QuietSlotH;
				const double Life = 36.0 + 240.0 * std::pow(H(S.Seed, 32, Q, B), 1.6);
				const bool bEruptive = H(S.Seed, 33, Q, B) < 0.12f;
				const double EruptAt = Life * (0.7 + 0.25 * H(S.Seed, 34, Q, B));
				const double EruptDur = 1.0 + 2.0 * H(S.Seed, 35, Q, B);
				const double Age = Ah - Born;
				const double End = bEruptive ? EruptAt + EruptDur : Life;
				if (Age < 0.0 || Age > End)
				{
					continue;
				}
				const bool bCrown = H(S.Seed, 36, Q, B) < 0.5f;
				const double LatMag = bCrown ? 45.0 + 25.0 * H(S.Seed, 37, Q, B) : 20.0 + 25.0 * H(S.Seed, 37, Q, B);
				const double Lat = (H(S.Seed, 38, Q, B) < 0.5f ? -1.0 : 1.0) * LatMag * Pi / 180.0;
				const double Lon = H(S.Seed, 39, Q, B) * TwoPi;
				const double Turn = (H(S.Seed, 40, Q, B) - 0.5) * 0.8;
				float HalfWidth = 0.1f + 0.2f * H(S.Seed, 41, Q, B);
				float Height = 0.12f + 0.23f * H(S.Seed, 42, Q, B);
				const float Seed = H(S.Seed, 43, Q, B) * 1000.f;
				const float A01 = float(Age / Life);
				float Bright = Smooth(0.f, 0.1f, A01) * (bEruptive ? 1.f : 1.f - Smooth(0.85f, 1.f, A01));
				Bright *= 0.75f + 0.25f * float(std::sin(Ah * TwoPi / 9.0 + Seed));
				Height *= 0.6f + 0.4f * Smooth(0.f, 0.2f, A01);
				float Lift = 0.f;
				bool bErupting = false;
				if (bEruptive && Age > EruptAt)
				{
					const float Tau = float((Age - EruptAt) / EruptDur);
					Lift = 0.2f * Tau + 2.5f * Tau * Tau;
					Height *= 1.f + 1.5f * Tau;
					HalfWidth *= 1.f + 0.8f * Tau;
					Bright *= (1.f + 0.3f * Smooth(0.f, 0.1f, Tau)) * std::exp(-2.f * Tau) * (1.f - Smooth(0.85f, 1.f, Tau));
					bErupting = true;
				}
				Emit(Lat, Lon, Turn, HalfWidth, Height, 0.8f * Bright, Lift, Seed, Clamp01(A01), EPromKind::Hedgerow, bErupting);
			}
		}

		std::sort(Cands, Cands + NumCands, [](const FCand& A, const FCand& B) { return A.Score > B.Score; });
		for (int I = 0; I < NumCands && Out.NumProms < MaxProminences; ++I)
		{
			Out.Proms[Out.NumProms++] = Cands[I].P;
		}

		// --- flares ---------------------------------------------------------------------------------------------
		auto Consider = [&](const FFlare& F)
		{
			if (F.Strength * F.Face > Out.Flare.Strength * Out.Flare.Face)
			{
				Out.Flare = F;
			}
		};
		const int64_t F1 = int64_t(std::floor(Ah / FlareSlotH));
		const int64_t F0 = F1 - int64_t(std::ceil(FlareLookbackH / FlareSlotH));
		for (int I = 0; I < NumAlive; ++I)
		{
			const FAliveRegion& R = Alive[I];
			const uint32_t Rid = uint32_t(R.Id);
			for (int64_t J = F0; J <= F1; ++J)
			{
				if (H(S.Seed, 50, J, Rid) >= 0.06f * R.Strength * R.Strength * Act)
				{
					continue;
				}
				const double Start = (double(J) + H(S.Seed, 51, J, Rid)) * FlareSlotH;
				// a steep power law: most are small, a few big
				const float Peak = std::min(1.f, 0.1f * std::pow(std::max(H(S.Seed, 52, J, Rid), 1e-3f), -0.6f));
				const float Rise = 2.f + 3.f * H(S.Seed, 53, J, Rid);
				const float Decay = 6.f + 20.f * Peak + 10.f * H(S.Seed, 54, J, Rid);
				const float T = float((Ah - Start) * 60.0);
				const float Now = Peak * FlareCurve(T, Rise, Decay);
				if (Now < 0.01f)
				{
					continue;
				}
				FFlare F;
				F.Lat = float(R.Lat + (H(S.Seed, 55, J, Rid) - 0.5) * 0.04);
				F.Lon = float(std::fmod(R.Lon + (H(S.Seed, 56, J, Rid) - 0.5) * 0.05, TwoPi));
				F.Strength = Now;
				F.Peak = Peak;
				F.AgeMin = T;
				F.Face = Smooth(0.f, 0.15f, float(ViewOf(F.Lat, F.Lon, Spin).Z));
				F.Region = R.Id;
				Consider(F);
			}
		}
		if (Forced && Forced->Peak > 0.f)
		{
			const float T = float((Ah - Forced->StartActivityHours) * 60.0);
			const float Rise = 2.5f, Decay = 6.f + 20.f * Forced->Peak;
			const float Now = Forced->Peak * FlareCurve(T, Rise, Decay);
			if (Now >= 0.01f)
			{
				// on the most visible strong region, else a little north-east of the disk's centre
				FFlare F;
				F.Lat = 0.3f;
				F.Lon = float(std::fmod(Spin + 0.35, TwoPi));
				float Best = 0.f;
				for (int I = 0; I < NumAlive; ++I)
				{
					const float Sc = Alive[I].Strength * Smooth(0.25f, 0.7f, float(ViewOf(Alive[I].Lat, Alive[I].Lon, Spin).Z));
					if (Sc > Best)
					{
						Best = Sc;
						F.Lat = float(Alive[I].Lat);
						F.Lon = float(std::fmod(Alive[I].Lon, TwoPi));
						F.Region = Alive[I].Id;
					}
				}
				F.Strength = Now;
				F.Peak = Forced->Peak;
				F.AgeMin = T;
				F.Face = Smooth(0.f, 0.15f, float(ViewOf(F.Lat, F.Lon, Spin).Z));
				Out.Flare = F;
			}
		}
		Out.FlareLight = Out.Flare.Strength * Out.Flare.Face;
	}
}
