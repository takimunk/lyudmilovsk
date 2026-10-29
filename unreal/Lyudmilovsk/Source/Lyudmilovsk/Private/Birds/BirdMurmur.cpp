#include "BirdMurmur.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace LyudBirds
{
	namespace
	{
		constexpr float Pi = 3.14159265f;
		constexpr float Mass = 0.08f;          // kg, a starling
		constexpr float Grav = 9.81f;
		constexpr float H = 1.f / 25.f;        // s, the flock's step
		constexpr float V0 = 11.f;             // m/s cruise
		// StarDisplay's social weights (Hildenbrandt et al. 2010), as in Murmuration
		constexpr int K = 7;
		constexpr int KWide = 10;
		constexpr float Separation = 1.f, Alignment = 0.5f, Cohesion = 1.f;
		constexpr float HardRadius = 0.2f, Sigma = 1.37f, BlindAngle = 90.f, MaxRange = 25.f, SpeedAlign = 1.f;
		constexpr float EscapeContagion = 40.f;
		constexpr float MaxBank = 60.f * Pi / 180.f, MaxLift = 2.f, RollRate = 6.f, Noise = 0.01f;
		constexpr float InformedFraction = 0.12f, LeaderWeight = 0.2f, FollowShare = 0.25f;
		constexpr float RoamR = 170.f;         // m round the roost the flock wanders
		constexpr float Cell = 2.f;
		constexpr int HBits = 14, HSize = 1 << HBits, HMask = HSize - 1;

		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
		float Hyp3(float A, float B, float C) { return std::sqrt(A * A + B * B + C * C); }
		float Smooth(float A, float B, float X) { const float T = Clamp((X - A) / (B - A), 0.f, 1.f); return T * T * (3.f - 2.f * T); }
		uint32_t Hash(int X, int Y, int Z) { return ((uint32_t)X * 73856093u ^ (uint32_t)Y * 19349663u ^ (uint32_t)Z * 83492791u) & HMask; }
		float HashRnd(uint32_t A, uint32_t B)
		{
			uint32_t Hh = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA77u;
			Hh ^= Hh >> 15; Hh *= 0x2C1B3C6Du; Hh ^= Hh >> 12; Hh *= 0x297A2D39u; Hh ^= Hh >> 15;
			return (Hh >> 8) * (1.f / 16777216.f);
		}
	}

	FMurmuration::FMurmuration(const FWorld& W, int Species, int RoostSite)
		: SpeciesIdx(Species)
	{
		// the reedbed: every site of the roost's kind within 60 m of it
		const FSite& R = W.Sites[RoostSite];
		V3 Sum;
		for (int I = 0; I < (int)W.Sites.size(); I++)
		{
			const FSite& S = W.Sites[I];
			if (S.Kind == R.Kind && (S.Center() - R.Center()).Len2D() < 60.f)
			{
				RoostSites.push_back(I);
				Sum += S.Center();
			}
		}
		RoostC = Sum * (1.f / std::max<size_t>(1, RoostSites.size()));
		for (int I : RoostSites)
		{
			RoostR = std::max(RoostR, (W.Sites[I].Center() - RoostC).Len2D() + W.Sites[I].Radius);
		}
		Goal = C = RoostC + V3(0.f, 0.f, 45.f);
		BucketStart.assign(HSize + 1, 0);
		Fill.assign(HSize + 1, 0);
	}

	bool FMurmuration::IsRoostSite(int Site) const
	{
		return std::find(RoostSites.begin(), RoostSites.end(), Site) != RoostSites.end();
	}

	int FMurmuration::PickRoostSite(const FWorld& W, uint32_t& R) const
	{
		return RoostSites.empty() ? -1 : RoostSites[(size_t)(W.Rand(R) * RoostSites.size()) % RoostSites.size()];
	}

	V3 FMurmuration::Centre() const { return bFormed ? C : RoostC + V3(0.f, 0.f, 45.f); }

	float FMurmuration::Rnd()
	{
		Rng ^= Rng << 13;
		Rng ^= Rng >> 17;
		Rng ^= Rng << 5;
		return (Rng & 0xffffff) / 16777216.f;
	}

	float FMurmuration::BankOf(int Id) const
	{
		return Id >= 0 && Id < (int)SlotOf.size() && SlotOf[Id] >= 0 ? M[SlotOf[Id]].Bank : 0.f;
	}

	void FMurmuration::Add(FWorld& W, int Id)
	{
		if ((int)SlotOf.size() < (int)W.Birds.size())
		{
			SlotOf.resize(W.Birds.size(), -1);
		}
		if (SlotOf[Id] >= 0)
		{
			return;
		}
		FBird& B = W.Birds[Id];
		FM F;
		F.Id = Id;
		// each bird its own speed, height and roll rate (fixed ranks), and a slowly drifting mood
		F.Rank = std::fmod(Id * 0.6180339887f, 1.f);
		F.SpeedN = (std::fmod(Id * 0.7548776662f + 0.31f, 1.f) - 0.5f) * 2.f;
		F.AltN = (W.Rand(B.Rng) - 0.5f) * 2.f;
		F.RollN = (W.Rand(B.Rng) - 0.5f) * 2.f;
		F.Mood = (W.Rand(B.Rng) - 0.5f);
		SlotOf[Id] = (int)M.size();
		M.push_back(F);
		W.SetAct(B, EAct::Murmur, 0.f);
		B.Target = -1;
		B.Site = -1;
		B.Delay = 0.f;
		B.bLook = false;
	}

	void FMurmuration::Remove(int Index)
	{
		SlotOf[M[Index].Id] = -1;
		if (Index != (int)M.size() - 1)
		{
			M[Index] = M.back();
			SlotOf[M[Index].Id] = Index;
		}
		M.pop_back();
	}

	void FMurmuration::Join(FWorld& W, int Id)
	{
		FBird& B = W.Birds[Id];
		if (B.Vel.Len() < 4.f)
		{
			B.Vel = (Centre() - B.Pos).Norm() * V0;
		}
		Add(W, Id);
	}

	void FMurmuration::Place(FWorld& W, int Id)
	{
		FBird& B = W.Birds[Id];
		// a ball round the flock, all heading the same way
		const float A = W.Rand(B.Rng) * 2.f * Pi, R = 25.f * std::cbrt(W.Rand(B.Rng));
		const float E = (W.Rand(B.Rng) - 0.5f) * 0.6f;
		const V3 Ce = Centre();
		B.Pos = Ce + V3(std::cos(A) * R, std::sin(A) * R, E * R);
		B.Vel = V3(std::cos(GoalT + 1.f), std::sin(GoalT + 1.f), 0.f) * V0;
		B.Yaw = std::atan2(B.Vel.Y, B.Vel.X);
		Add(W, Id);
		bFormed = true;
		C = Ce;
	}

	void FMurmuration::Clear()
	{
		for (const FM& F : M)
		{
			SlotOf[F.Id] = -1;
		}
		M.clear();
		bFormed = false;
		Impulses.clear();
	}

	int FMurmuration::RimBird(const FWorld& W, uint32_t& R) const
	{
		int Best = -1;
		float BestD = -1.f;
		for (int S = 0; S < 30 && !M.empty(); S++)
		{
			const FM& F = M[(size_t)(W.Rand(R) * M.size()) % M.size()];
			if (F.bDown)
			{
				continue;
			}
			const float D = (W.Birds[F.Id].Pos - C).Len();
			if (D > BestD)
			{
				BestD = D;
				Best = F.Id;
			}
		}
		return Best;
	}

	/** A new place for the flock to head for: over the roost and the land round it, out over the sea only along the coast. */
	void FMurmuration::NewGoal(const FWorld& W, float Hm)
	{
		const float MaxOff = W.Opt.MurmurOffshore;
		const bool bLate = Hm > DescendMinutes;
		for (int Try = 0; Try < 24; Try++)
		{
			const float A = Rnd() * 2.f * Pi, R = (bLate ? 60.f : RoamR) * std::sqrt(Rnd());
			V3 P = RoostC + V3(std::cos(A) * R, std::sin(A) * R, 0.f);
			if (W.Offshore(P.X, P.Y) > MaxOff * 0.3f)
			{
				continue;
			}
			const float Ground = std::max(W.TopZ(P.X, P.Y), W.SeaZ);
			P.Z = Ground + (bLate ? 25.f : 35.f + Rnd() * 40.f);
			Goal = P;
			GoalT = 9.f + Rnd() * 12.f;
			return;
		}
		Goal = RoostC + V3(0.f, 0.f, 45.f);
		GoalT = 8.f;
	}

	void FMurmuration::Step(FWorld& W, float Dt)
	{
		const auto T0 = std::chrono::steady_clock::now();
		struct FTime
		{
			const std::chrono::steady_clock::time_point& T0;
			double& Ms;
			~FTime() { Ms += std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count() * 1000.0; }
		} Timer{ T0, CpuMs };
		const FSpecies& S = SpeciesTable()[SpeciesIdx];
		const float Hm = (W.Hour - W.Sunset) * 60.f;
		const float End = S.RoostMinutes;
		const bool bOn = Hm > StartMinutes && Hm < End;
		if ((int)SlotOf.size() < (int)W.Birds.size())
		{
			SlotOf.resize(W.Birds.size(), -1);
		}
		// arrivals: groups flying in join the flock when they reach it; after the end they go straight to the reed
		for (FBird& B : W.Birds)
		{
			if (B.Species != SpeciesIdx || B.Target != -6 || B.bGone || B.Act == EAct::Murmur || !IsAirborne(B.Act))
			{
				continue;
			}
			if (!bOn)
			{
				B.Target = -1;
				const int Site = PickRoostSite(W, B.Rng);
				W.FlyTo(B, Site, W.SpotFor(B, Site));
				continue;
			}
			if ((B.Pos - Centre()).Len2D() < 160.f)
			{
				Join(W, B.Id);
			}
		}
		// birds taken out from elsewhere (the player became one)
		for (int I = (int)M.size() - 1; I >= 0; I--)
		{
			if (W.Birds[M[I].Id].Act != EAct::Murmur)
			{
				Remove(I);
			}
		}
		if (M.empty())
		{
			bFormed = false;
			Acc = 0.f;
			return;
		}
		if (!bFormed)
		{
			bFormed = true;
			C = W.Birds[M[0].Id].Pos;
			NewGoal(W, Hm);
		}
		Acc += Dt;
		int Steps = 0;
		while (Acc >= H && Steps < 8)
		{
			Sub(W, H, Hm);
			Acc -= H;
			Steps++;
		}
		Acc = std::min(Acc, H);
	}

	// --- social forces (StarDisplay, as Murmuration's StarlingFlock.cpp) -----------------------------------------

	void FMurmuration::BuildGrid(const FWorld& W)
	{
		const int N = (int)M.size();
		std::fill(BucketStart.begin(), BucketStart.end(), 0);
		CellOf.resize(N);
		Sorted.resize(N);
		SX.resize(N);
		SY.resize(N);
		SZ.resize(N);
		for (int I = 0; I < N; I++)
		{
			const V3& P = W.Birds[M[I].Id].Pos;
			const uint32_t Hh = Hash((int)std::floor(P.X / Cell), (int)std::floor(P.Y / Cell), (int)std::floor(P.Z / Cell));
			CellOf[I] = (int)Hh;
			BucketStart[Hh + 1]++;
		}
		for (int Hh = 0; Hh < HSize; Hh++)
		{
			BucketStart[Hh + 1] += BucketStart[Hh];
		}
		std::copy(BucketStart.begin(), BucketStart.end(), Fill.begin());
		for (int I = 0; I < N; I++)
		{
			const int O = Fill[CellOf[I]]++;
			const V3& P = W.Birds[M[I].Id].Pos;
			Sorted[O] = I;
			SX[O] = P.X;
			SY[O] = P.Y;
			SZ[O] = P.Z;
		}
	}

	/** The k nearest flock mates of bird i, in all directions; whether each is in front of the blind angle is recorded. */
	int FMurmuration::Knn(const FWorld& W, int I, int Kn, float CosBlind, int* NIdx, float* ND2, unsigned char* NVis) const
	{
		const FBird& B = W.Birds[M[I].Id];
		const float X = B.Pos.X, Y = B.Pos.Y, Z = B.Pos.Z;
		const float Sp = B.Vel.Len() + 1e-6f;
		const float Fx = B.Vel.X / Sp, Fy = B.Vel.Y / Sp, Fz = B.Vel.Z / Sp;
		const int Cx = (int)std::floor(X / Cell), Cy = (int)std::floor(Y / Cell), Cz = (int)std::floor(Z / Cell);
		const float Edge = std::min({ X - Cx * Cell, (Cx + 1) * Cell - X, Y - Cy * Cell, (Cy + 1) * Cell - Y, Z - Cz * Cell, (Cz + 1) * Cell - Z });
		const int MaxR = std::min(8, (int)std::ceil(MaxRange / Cell));
		const float Range2 = MaxRange * MaxRange;
		int N = 0;
		for (int R = 0; R <= MaxR; R++)
		{
			for (int DX = -R; DX <= R; DX++)
			{
				for (int DY = -R; DY <= R; DY++)
				{
					const bool bShell = DX == -R || DX == R || DY == -R || DY == R;
					const int Step = bShell ? 1 : std::max(1, 2 * R);
					for (int DZ = -R; DZ <= R; DZ += Step)
					{
						const uint32_t Hh = Hash(Cx + DX, Cy + DY, Cz + DZ);
						for (int S = BucketStart[Hh]; S < BucketStart[Hh + 1]; S++)
						{
							const float DDX = SX[S] - X, DDY = SY[S] - Y, DDZ = SZ[S] - Z;
							const float D2 = DDX * DDX + DDY * DDY + DDZ * DDZ;
							if (D2 > Range2 || D2 < 1e-8f || (N == Kn && D2 >= ND2[Kn - 1]))
							{
								continue;
							}
							const int J = Sorted[S];
							bool bDup = false;
							for (int Q = 0; Q < N; Q++)
							{
								bDup |= NIdx[Q] == J;
							}
							if (bDup)
							{
								continue;
							}
							int Pq = N < Kn ? N++ : Kn - 1;
							while (Pq > 0 && ND2[Pq - 1] > D2)
							{
								ND2[Pq] = ND2[Pq - 1];
								NIdx[Pq] = NIdx[Pq - 1];
								NVis[Pq] = NVis[Pq - 1];
								Pq--;
							}
							ND2[Pq] = D2;
							NIdx[Pq] = J;
							NVis[Pq] = (DDX * Fx + DDY * Fy + DDZ * Fz) / std::sqrt(D2) >= CosBlind ? 1 : 0;
						}
					}
				}
			}
			const float Reach = R * Cell + Edge;
			if (N == Kn && ND2[Kn - 1] <= Reach * Reach)
			{
				break;
			}
		}
		return N;
	}

	/** One bird's social steering. Out: ax ay az, copy an escape (1/0), its direction xyz, follow the descent (1/0). */
	void FMurmuration::Solve(const FWorld& W, int I, float Rdt, float Hm, float* Out) const
	{
		const float CosBlind = -std::cos(BlindAngle * 0.5f * Pi / 180.f);
		const float Ws = Separation / Mass, Wa = Alignment / Mass, Wc = Cohesion / Mass;
		const float Sig2 = Sigma * Sigma;
		int NIdx[KWide];
		float ND2[KWide];
		unsigned char NVis[KWide];
		for (int Q = 0; Q < 8; Q++)
		{
			Out[Q] = 0.f;
		}
		const FM& F = M[I];
		const FBird& B = W.Birds[F.Id];
		const float Sp = B.Vel.Len() + 1e-3f;
		const V3 Fw = B.Vel * (1.f / Sp);
		const int NCount = Knn(W, I, KWide, CosBlind, NIdx, ND2, NVis);
		if (NCount == 0)
		{
			// lost: back to the flock
			const V3 To = (C - B.Pos).Norm() * 3.f;
			Out[0] = To.X;
			Out[1] = To.Y;
			Out[2] = To.Z;
			return;
		}
		V3 Sep, Hd, U, Co, All;
		float Vs = 0.f;
		int EscJ = -1, NSep = 0, NSoc = 0, NVisAll = 0, Down = 0;
		for (int Q = 0; Q < NCount; Q++)
		{
			const int J = NIdx[Q];
			const FBird& O = W.Birds[M[J].Id];
			const float D = std::sqrt(ND2[Q]);
			const V3 Dir = (O.Pos - B.Pos) * (1.f / D);
			if (Q < K)
			{
				const float E = D <= HardRadius ? 1.f : std::exp(-(D - HardRadius) * (D - HardRadius) / Sig2);
				Sep -= Dir * E;
				All += Dir * D;
				NSep++;
			}
			if (!NVis[Q])
			{
				continue;
			}
			U += Dir;
			NVisAll++;
			if (NSoc >= K)
			{
				continue;
			}
			NSoc++;
			const float Sj = O.Vel.Len() + 1e-6f;
			Hd += O.Vel * (1.f / Sj);
			Vs += Sj;
			if (D > HardRadius)
			{
				Co += Dir * D;
			}
			if (M[J].bEscape && M[J].EscT > 0.05f)
			{
				EscJ = J;
			}
			Down += M[J].bDown;
		}
		V3 A = Sep * (Ws / std::max(NSep, 1));
		const float Inv = 1.f / std::max(NSoc, 1);
		const float HL = Hd.Len();
		if (HL > 1e-6f)
		{
			A += (Hd * (1.f / HL) - Fw) * Wa;
		}
		// speed-ups and slow-downs pass through the flock as density waves
		if (NSoc > 0)
		{
			A += Fw * ((Vs * Inv - Sp) * SpeedAlign);
		}
		// centrality: ~0 deep inside the flock, ~1 at its rim: rim birds pull in hard
		const float Xc = NVisAll ? U.Len() / NVisAll : 0.f;
		A += NSoc > 0 ? Co * (Inv * Wc * Xc) : All * (Wc / std::max(NSep, 1));
		Out[0] = A.X;
		Out[1] = A.Y;
		Out[2] = A.Z;
		// escape rolls spread from neighbour to neighbour (Hemelrijk et al. 2015)
		if (EscJ >= 0 && !F.bEscape && HashRnd((uint32_t)F.Id, Tick) < EscapeContagion * Rdt)
		{
			Out[3] = 1.f;
			Out[4] = M[EscJ].Esc.X;
			Out[5] = M[EscJ].Esc.Y;
			Out[6] = M[EscJ].Esc.Z;
		}
		// the descent comes in waves: a bird follows descending neighbours; late on, everybody goes
		if (Hm > DescendMinutes)
		{
			const float Late = Smooth(DescendMinutes, SpeciesTable()[SpeciesIdx].RoostMinutes - 6.f, Hm);
			const bool bNear = (B.Pos - RoostC).Len2D() < RoamR + 60.f;
			if (bNear && HashRnd((uint32_t)F.Id + 7919u, Tick) < Rdt * (0.004f + 0.05f * Late + 2.f * Down * Inv))
			{
				Out[7] = 1.f;
			}
		}
	}

	void FMurmuration::GoDown(FWorld& W, FM& F)
	{
		FBird& B = W.Birds[F.Id];
		F.bDown = true;
		F.Site = PickRoostSite(W, B.Rng);
		if (F.Site < 0)
		{
			return;
		}
		F.Perch = W.SpotOn(W.Sites[F.Site], B.Rng);
		F.Perch.Z += 0.6f + W.Rand(B.Rng) * 1.1f;       // on the reed stems
	}

	void FMurmuration::Sub(FWorld& W, float Hh, float Hm)
	{
		const FSpecies& Sp = SpeciesTable()[SpeciesIdx];
		Tick++;
		const int N = (int)M.size();
		// centroid and heading of the flying flock
		V3 Sum, SumV;
		int Fly = 0;
		for (const FM& F : M)
		{
			if (!F.bDown)
			{
				Sum += W.Birds[F.Id].Pos;
				SumV += W.Birds[F.Id].Vel;
				Fly++;
			}
		}
		if (Fly)
		{
			C = Sum * (1.f / Fly);
			Vm = SumV * (1.f / Fly);
		}
		GoalT -= Hh;
		if (GoalT <= 0.f || (Goal - C).Len2D() < 25.f)
		{
			NewGoal(W, Hm);
		}
		// the falcon(s) attacking now
		struct FHawk { V3 P, V; };
		FHawk Hawks[4];
		int NH = 0;
		for (const FWorld::FPredator& P : W.Predators)
		{
			const FBird& Hk = W.Birds[P.Bird];
			if (Hk.Act == EAct::Chase && NH < 4 && (Hk.Pos - C).Len() < 200.f)
			{
				Hawks[NH++] = { Hk.Pos, Hk.Vel };
			}
		}
		// spontaneous turns: a few birds on the rim swing off and their neighbours follow (every few seconds)
		ImpulseT -= Hh;
		if (ImpulseT <= 0.f && Fly > 30)
		{
			ImpulseT = 3.f + Rnd() * 5.f;
			int Seed = (int)(Rnd() * N) % N;
			for (int T = 0; T < 8 && M[Seed].bDown; T++)
			{
				Seed = (int)(Rnd() * N) % N;
			}
			const V3 V = W.Birds[M[Seed].Id].Vel;
			const float Ang = (Rnd() < 0.5f ? -1.f : 1.f) * (0.6f + Rnd() * 0.6f);
			const float Ca = std::cos(Ang), Sa = std::sin(Ang);
			V3 Dir(V.X * Ca - V.Y * Sa, V.X * Sa + V.Y * Ca, (Rnd() - 0.5f) * 3.f);
			Impulses.push_back({ M[Seed].Id, Dir.Norm(), 1.6f, 6.f + 0.02f * std::sqrt((float)Fly) * 20.f });
		}
		for (size_t Q = 0; Q < Impulses.size();)
		{
			Impulses[Q].T -= Hh;
			if (Impulses[Q].T <= 0.f || W.Birds[Impulses[Q].Seed].Act != EAct::Murmur)
			{
				Impulses[Q] = Impulses.back();
				Impulses.pop_back();
			}
			else
			{
				Q++;
			}
		}

		// social forces: each bird re-reads its neighbours every other step (reaction time 0.08 s), staggered
		BuildGrid(W);
		const float Rdt = Hh * 2.f;
		float Out[8];
		for (int I = (int)(Tick & 1); I < N; I += 2)
		{
			FM& F = M[I];
			Solve(W, I, Rdt, Hm, Out);
			F.Sf = V3(Out[0], Out[1], Out[2]);
			if (F.bDown)
			{
				continue;
			}
			if (Out[3] > 0.f && F.Refractory <= 0.f && F.EscDelay <= 0.f)
			{
				F.EscDelay = 0.01f + Rnd() * 0.05f;
				F.NextEsc = V3(Out[4], Out[5], Out[6]);
			}
			if (Out[7] > 0.f)
			{
				GoDown(W, F);
			}
		}

		const float MaxOff = W.Opt.MurmurOffshore;
		const float LeadA = LeaderWeight / Mass, FollowA = LeadA * FollowShare, ImpA = 2.f * LeaderWeight / Mass;
		const float Pull = 10.f + 0.3f * std::sqrt((float)std::max(Fly, 1));
		const float Wing = Sp.Wingbeat > 0.f ? Sp.Wingbeat : 12.f;
		for (int I = N - 1; I >= 0; I--)
		{
			FM& F = M[I];
			FBird& B = W.Birds[F.Id];
			B.Phase += Hh * Wing;
			if (F.bDown)
			{
				// glide down into the reed, slowing, and hand the bird back to the bird life on the last metres
				const V3 To = F.Perch - B.Pos;
				const float DL = To.Len();
				if (DL < 3.f || F.Site < 0)
				{
					B.Site = F.Site;
					B.Spot = F.Perch;
					B.FlyFrom = B.Pos;
					B.Target = -1;
					W.SetAct(B, EAct::Land, 0.f);
					Landed++;
					Remove(I);
					continue;
				}
				const float Want = std::min(V0, 2.f + DL * 0.35f);
				V3 Dv = To * (Want / DL) - B.Vel;
				const float Acc_ = 12.f * Hh;
				if (Dv.Len() > Acc_)
				{
					Dv = Dv * (Acc_ / Dv.Len());
				}
				B.Vel += Dv;
				B.Pos += B.Vel * Hh;
				const float Floor = std::max(W.GroundZ(B.Pos.X, B.Pos.Y), W.SeaZ) + 0.3f;
				if (DL > 5.f && B.Pos.Z < Floor)
				{
					B.Pos.Z = Floor;
				}
				F.Bank *= 0.9f;
				if (B.Vel.Len2D() > 0.3f)
				{
					B.Yaw = std::atan2(B.Vel.Y, B.Vel.X);
				}
				continue;
			}

			const float Speed = B.Vel.Len() + 1e-3f;
			const V3 Fw = B.Vel * (1.f / Speed);
			V3 A = F.Sf;
			const float Ground = std::max(W.GroundZ(B.Pos.X, B.Pos.Y), W.SeaZ);

			// the roost's circle, and the coast: a turning force (lateral only), harder the farther out and the more it
			// heads away. Over the sea past MaxOff * 0.7 from the shore it turns back to the land whatever it does.
			{
				V3 D = (RoostC - B.Pos).Flat();
				const float Dist = D.Len2D();
				float Gain = Dist > RoamR ? (Dist - RoamR) / 25.f : 0.f;
				const V3 Ahead = B.Pos + B.Vel * 4.f;
				const float Off = std::max(W.Offshore(B.Pos.X, B.Pos.Y), W.Offshore(Ahead.X, Ahead.Y));
				if (Off > MaxOff * 0.45f)
				{
					Gain += 2.f + 14.f * (Off - MaxOff * 0.45f) / (MaxOff * 0.55f);
				}
				if (Gain > 0.f && Dist > 1.f)
				{
					D = D * (1.f / Dist);
					const float Away = -(D.X * Fw.X + D.Y * Fw.Y);
					const float S = (0.02f / Mass) * 2.f * Gain * (0.5f + 0.5f * Away);
					V3 L = D - Fw * D.Dot(Fw);
					if (L.Len() < 0.2f)
					{
						L = V3(-Fw.Y, Fw.X, 0.f);       // straight away: turn to one side
					}
					A += L.Norm() * S;
				}
			}
			// where the flock is going: the informed few steer for the goal, everyone else feels it a little
			float AltWant = Goal.Z;
			if (F.Rank < InformedFraction)
			{
				const V3 G = Goal - B.Pos;
				const float GH = G.Len2D() + 1e-3f;
				const float Gy = Clamp(G.Z / std::max(GH, 20.f), -0.25f, 0.25f);
				V3 Want(G.X / GH, G.Y / GH, Gy);
				Want = Want.Norm();
				A += (Want - Fw) * LeadA;
			}
			else
			{
				const V3 G = (Goal - C).Flat().Norm();
				const float HL = Fw.Len2D() + 1e-6f;
				A += V3(G.X - Fw.X / HL, G.Y - Fw.Y / HL, 0.f) * FollowA;
			}
			// stay with the flock: pulled back toward its centre from beyond its radius (never a brake)
			{
				V3 Cd = C - B.Pos;
				Cd.Z *= 0.5f;
				const float CD = Cd.Len();
				const float Over = CD / Pull - 0.6f;
				if (Over > 0.f)
				{
					V3 Q = Cd * (1.f * std::min(Over, 4.f) / CD);
					const float Along = Q.Dot(Fw);
					if (Along < 0.f)
					{
						Q -= Fw * Along;
					}
					A += Q;
				}
			}
			for (const FImpulse& Im : Impulses)
			{
				if ((B.Pos - W.Birds[Im.Seed].Pos).Len() < Im.R)
				{
					A += (Im.Dir - Fw) * ImpA;
				}
			}
			// height: the flock's band with the bird's own offset, firmly over roofs and crowns
			const float Alt = std::max(AltWant + F.AltN * 4.f, Ground + 6.f);
			A.Z += Clamp((0.01f / Mass) * (Alt - B.Pos.Z), -2.f, 2.f) - 0.3f * B.Vel.Z;
			float Urgency = 0.f;
			const float Top = W.TopZ(B.Pos.X, B.Pos.Y) + 8.f;
			if (B.Pos.Z < Top)
			{
				A.Z += std::min(8.f, (Top - B.Pos.Z) * 0.8f) + std::max(0.f, -B.Vel.Z) * 1.5f;
				Urgency = Clamp((Top - B.Pos.Z) / 8.f, 0.f, 1.f);
			}
			A += V3(Rnd() - 0.5f, Rnd() - 0.5f, (Rnd() - 0.5f) * 0.5f) * (1.15f * Noise / Mass);

			// the falcon: roll away from its path, sideways and down
			if (!F.bEscape && F.Refractory <= 0.f)
			{
				for (int Q = 0; Q < NH; Q++)
				{
					const V3 D = B.Pos - Hawks[Q].P;
					const float Dd = D.Len();
					const bool bComing = D.Dot(Hawks[Q].V) > 0.f;
					if (Dd < 9.f || (Dd < 26.f && bComing))
					{
						const float PL = Hawks[Q].V.Len() + 1e-6f;
						const V3 Pv = Hawks[Q].V * (1.f / PL);
						V3 O = D - Pv * D.Dot(Pv);
						O = O.Norm();
						F.bNextEscape = true;
						F.NextEsc = V3(O.X, O.Y, O.Z - 0.3f);
						break;
					}
				}
			}
			if (F.bEscape)
			{
				A += F.Esc * (14.f * std::min(1.f, F.EscT * 3.f));
			}
			// the surface ahead: pull up harder the closer it is
			{
				const V3 Q = B.Pos + B.Vel;
				const float Clr = std::min(Q.Z - std::max(W.GroundZ(Q.X, Q.Y), W.SeaZ), B.Pos.Z - Ground) - 2.f;
				const float Hd = 1.5f + std::max(0.f, -B.Vel.Z) * 0.7f;
				if (Clr < Hd)
				{
					Urgency = std::max(Urgency, Clamp(1.f - Clr / Hd, 0.f, 1.5f));
					const float U1 = std::min(1.f, Urgency);
					A.Z += U1 * U1 * 25.f + U1 * std::max(0.f, -B.Vel.Z) * 5.f;
				}
			}

			// --- flight: lift from banked wings, thrust and drag toward its own speed ---
			const float Fpar = A.Dot(Fw);
			const V3 Perp = A - Fw * Fpar;
			V3 Lv(Perp.X - Grav * Fw.Z * Fw.X, Perp.Y - Grav * Fw.Z * Fw.Y, Perp.Z + Grav - Grav * Fw.Z * Fw.Z);
			float RX = Fw.Y, RY = -Fw.X;                  // side axis (horizontal)
			float RL = std::sqrt(RX * RX + RY * RY);
			if (RL < 1e-4f)
			{
				RX = 1.f;
				RY = 0.f;
				RL = 1.f;
			}
			RX /= RL;
			RY /= RL;
			// up without bank = right x forward
			const V3 U0(RY * Fw.Z, -RX * Fw.Z, RX * Fw.Y - RY * Fw.X);
			const float EscRoll = F.bEscape ? 1.6f : 1.f;
			const float Urg = std::min(1.f, Urgency);
			float Target = std::atan2(-(Lv.X * RX + Lv.Y * RY), std::max(Lv.Dot(U0), 0.25f * Grav));
			const float Lim = MaxBank * EscRoll * (1.f - 0.75f * Urg);
			Target = Clamp(Target, -Lim, Lim);
			const float RollI = RollRate * Hh * Clamp(1.f + 0.3f * F.RollN, 0.4f, 1.8f) * (1.f + Urg);
			F.Bank += (Target - F.Bank) * std::min(1.f, RollI * EscRoll * 1.5f);
			const float CB = std::cos(F.Bank), SB = std::sin(F.Bank);
			const V3 Yb(-RX * SB + U0.X * CB, -RY * SB + U0.Y * CB, U0.Z * CB);
			const float LNeed = Lv.Dot(Yb);
			const float LMax = Grav * (MaxLift * EscRoll + 2.f * Urg) * (Speed / V0) * (Speed / V0);
			const float L = Clamp(LNeed, 0.f, LMax);
			const float Own = V0 * Clamp(1.f + 0.07f * F.SpeedN + 0.05f * F.Mood, 0.6f, 1.5f);
			const float Vt = F.bEscape ? std::max(Own, V0) * 1.3f : Own;
			const float Af = (Vt - Speed) / 1.f + Clamp(Fpar, -4.f, 4.f);
			V3 Nv = B.Vel + (Fw * Af + Yb * L - V3(0.f, 0.f, Grav)) * Hh;
			const float NS = Nv.Len();
			if (NS < 7.5f || NS > 22.f)
			{
				Nv = Nv * (Clamp(NS, 7.5f, 22.f) / (NS + 1e-6f));
			}
			// the bounds are firm: a bird near the edge of the coastal strip, or far from the roost, turns back to it hard
			const float Off = W.Offshore(B.Pos.X, B.Pos.Y);
			const V3 Home = (RoostC - B.Pos).Flat();
			if ((Off > MaxOff * 0.8f || Home.Len2D() > RoamR * 2.f) && Nv.Flat().Norm().Dot(Home.Norm()) < 0.5f)
			{
				const float Turn = 1.2f * Hh * (Home.X * Nv.Y - Home.Y * Nv.X > 0.f ? -1.f : 1.f);
				const float Cs = std::cos(Turn), Sn = std::sin(Turn);
				Nv = V3(Nv.X * Cs - Nv.Y * Sn, Nv.X * Sn + Nv.Y * Cs, std::max(Nv.Z, 0.f));
			}
			B.Vel = Nv;
			B.Pos += Nv * Hh;
			const float Floor = std::max(W.GroundZ(B.Pos.X, B.Pos.Y), W.SeaZ) + 0.3f;
			if (B.Pos.Z < Floor)
			{
				B.Pos.Z = Floor;
				B.Vel.Z = std::max(B.Vel.Z, 0.f);
			}
			if (B.Vel.Len2D() > 0.3f)
			{
				B.Yaw = std::atan2(B.Vel.Y, B.Vel.X);
			}
			MaxOffshore = std::max(MaxOffshore, W.Offshore(B.Pos.X, B.Pos.Y));
			MaxFromRoost = std::max(MaxFromRoost, (B.Pos - RoostC).Len2D());
		}
		// mood drifts (Ornstein-Uhlenbeck), an eighth of the flock a step
		{
			const float Am = std::exp(-8.f * Hh / 8.f), Bm = std::sqrt(1.f - Am * Am);
			for (int I = (int)(Tick & 7); I < (int)M.size(); I += 8)
			{
				M[I].Mood = M[I].Mood * Am + (Rnd() + Rnd() + Rnd() + Rnd() - 2.f) * 1.732f * Bm;
			}
		}
		// escapes: after the pass, all at once (the roll away 0.25 s and back 0.3 s)
		for (FM& F : M)
		{
			if (F.bEscape)
			{
				F.EscT -= Hh;
				if (F.EscT <= 0.f)
				{
					F.bEscape = false;
					F.Refractory = 1.f;
				}
			}
			else if (F.Refractory > 0.f)
			{
				F.Refractory -= Hh;
			}
			if (F.EscDelay > 0.f)
			{
				F.EscDelay -= Hh;
				if (F.EscDelay <= 0.f && !F.bEscape)
				{
					F.bNextEscape = true;
				}
			}
			if (F.bNextEscape && !F.bEscape && !F.bDown)
			{
				F.bEscape = true;
				F.EscT = 0.55f;
				F.Esc = F.NextEsc.Norm();
				Escapes++;
			}
			F.bNextEscape = false;
		}
	}

	std::string FMurmuration::Describe(const FWorld& W) const
	{
		int Down = 0;
		float R = 0.f;
		for (const FM& F : M)
		{
			Down += F.bDown;
			R = std::max(R, (W.Birds[F.Id].Pos - C).Len());
		}
		char Buf[256];
		std::snprintf(Buf, sizeof(Buf), "murmuration: %d birds (%d going down), centre %.0f %.0f %.0f, %.0f m across, %.0f m from the roost, "
			"%.0f m out over the sea (max %.0f), %d escapes, %d landed", (int)M.size(), Down, C.X, C.Y, C.Z, R * 2.f, (C - RoostC).Len2D(),
			W.Offshore(C.X, C.Y), MaxOffshore, Escapes, Landed);
		return Buf;
	}
}
