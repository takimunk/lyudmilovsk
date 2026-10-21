#include "Murmuration/StarlingFlock.h"

#include "Async/ParallelFor.h"
#include "Murmuration/StarlingFlockMath.h"

using namespace StarlingFlockMath;

FStarlingFlock::FStarlingFlock(int32 InCount, const FMurmurationParams& InParams, FVector2f InArea, uint32 InSeed)
	: P(InParams), Area(InArea), Seed(InSeed | 1u)
{
	Count = FMath::Clamp(InCount, 0, 20000);
	for (TArray<float>* A : { &Px, &Py, &Pz, &Vx, &Vy, &Vz, &Bank, &FlapPhase, &FlapAmp, &Fold, &FlapFreq, &Timer, &Refractory,
		&Ex, &Ey, &Ez, &Sfx, &Sfy, &Sfz, &EscDelay, &NextEx, &NextEy, &NextEz, &SpeedN, &AltN, &RollN, &Mood, &FlS, &FlA, &FlT,
		&SX, &SY, &SZ, &Dwell, &ActT, &HopT, &HopX, &HopY, &HopZ, &PeckT, &HopCool, &Disturb, &DisturbAt, &AlarmT })
	{
		A->SetNumZeroed(Count);
	}
	LeftBeach.Init(-1e9f, Count);
	for (TArray<uint8>* A : { &State, &NextEscape, &Grp, &FinalApp, &Relocated }) A->SetNumZeroed(Count);
	TrailSeq.SetNumZeroed(Count);
	PerchOf.Init(-1, Count);
	SpotOf.Init(-1, Count);
	CellOf.SetNumZeroed(Count);
	Sorted.SetNumZeroed(Count);
	BucketStart.SetNumZeroed(HSIZE + 1);
	Fill.SetNumZeroed(HSIZE);
	SocialOut.SetNumZeroed(Count * OUT_STRIDE);
	for (int32& W : WaveGroup) W = -1;

	FlowFrom.SetNumZeroed(FN * FC); FlowTo.SetNumZeroed(FN * FC); FlowNow.SetNumZeroed(FN * FC);
	FlowU.SetNumZeroed(FN); FlowRate.SetNumZeroed(FN);
	for (int32 N = 0; N < FN; N++)
	{
		FlowU[N] = Rnd();
		FlowRate[N] = 0.7f + Rnd() * 0.6f;
		for (int32 C = 0; C < FC; C++) { FlowFrom[N * FC + C] = FlowVal(); FlowTo[N * FC + C] = FlowVal(); }
	}

	// until a clock arrives: one cloud high up, all heading roughly one way
	const int32 G = AllocGroup(EMode::Local, FVector3f(0.f, P.Altitude, 0.f));
	const float Heading = Rnd() * 2.f * PI;
	const float R0 = FMath::Min(60.f, 4.f + FMath::Sqrt((float)FMath::Max(1, Count)) * 0.55f);
	for (int32 I = 0; I < Count; I++)
	{
		SpeedN[I] = Normal(); AltN[I] = Normal(); RollN[I] = Normal(); Mood[I] = Normal();
		FlapPhase[I] = Rnd() * 2.f * PI;
		FlapFreq[I] = 11.5f + Rnd() * 2.5f; // Hz, starlings ~12-14 Hz
		const float A = Rnd() * 2.f * PI, R = FMath::Sqrt(Rnd()) * R0;
		const float H = Heading + Gauss() * 0.1f;
		FlyAt(I, FVector3f(FMath::Cos(A) * R, P.Altitude + Gauss() * 4.f, FMath::Sin(A) * R * 0.6f), FVector3f(FMath::Cos(H) * 10.f, 0.f, FMath::Sin(H) * 10.f), G);
		FlapAmp[I] = 0.6f;
	}
	Groups[G].N = Count;
}

FString FStarlingFlock::Describe() const
{
	static const TCHAR* Modes[] = { TEXT("free"), TEXT("local"), TEXT("shore"), TEXT("out"), TEXT("in") };
	static const TCHAR* Forms[] = { TEXT("mass"), TEXT("stream"), TEXT("ball"), TEXT("sheet"), TEXT("funnel") };
	int32 ByState[7] = {};
	for (uint8 S : State) ByState[FMath::Min<int32>(S, 6)]++;
	FString Out = FString::Printf(TEXT("%02d:%02d %s, gather %.2f; flying %d, descending %d, perched %d, escaping %d, visiting %d, grounded %d, away %d"),
		(int32)Hours, (int32)(FMath::Frac(Hours) * 60.f), ActivityName(Act), GatherNow,
		ByState[Flying], ByState[Descending], ByState[Perched], ByState[Escaping], ByState[Visiting], ByState[Grounded], ByState[Away]);
	if (bHaveRoost) Out += FString::Printf(TEXT("; roost (%.0f, %.0f, %.0f)"), Roost.X, Roost.Y, Roost.Z);
	for (int32 G = 0; G < GMAX; G++)
	{
		const FGroup& Gr = Groups[G];
		if (Gr.Mode == EMode::Free || !Gr.N) continue;
		Out += FString::Printf(TEXT("\n    %s %d birds at (%.0f, %.0f, %.0f) %s to (%.0f, %.0f, %.0f)"), Modes[(int32)Gr.Mode], Gr.N, Gr.C.X, Gr.C.Y, Gr.C.Z,
			Forms[(int32)Gr.Form], Gr.Goal.X, Gr.Goal.Y, Gr.Goal.Z);
	}
	return Out;
}

// --- random ----------------------------------------------------------------------------------

float FStarlingFlock::Rnd()
{
	Seed ^= Seed << 13; Seed ^= Seed >> 17; Seed ^= Seed << 5;
	return Seed * (1.f / 4294967296.f);
}
float FStarlingFlock::Gauss() { return (Rnd() + Rnd() + Rnd() - 1.5f) * 1.15f; }
/** Approximately standard normal (Irwin-Hall, 4 terms). */
float FStarlingFlock::Normal() { return (Rnd() + Rnd() + Rnd() + Rnd() - 2.f) * 1.7320508f; }

// --- the world -------------------------------------------------------------------------------

void FStarlingFlock::SetGround(float InX0, float InZ0, float InCell, int32 InNx, int32 InNz, TArray<float>&& Heights)
{
	GX0 = InX0; GZ0 = InZ0; GCell = FMath::Max(0.1f, InCell); GNx = InNx; GNz = InNz;
	GroundH = MoveTemp(Heights);
	GroundTop = 0.f;
	// inland: toward the land's centre of mass; the shore: water cells within ~25 m of land
	FVector2f LandSum = FVector2f::ZeroVector;
	int32 LandN = 0;
	ShorePts.Reset();
	CoastLandPts.Reset();
	const int32 Reach = FMath::Max(1, FMath::RoundToInt(25.f / GCell));
	for (int32 IZ = 0; IZ < GNz; IZ++)
	{
		for (int32 IX = 0; IX < GNx; IX++)
		{
			const float H = GroundH[IZ * GNx + IX];
			GroundTop = FMath::Max(GroundTop, H);
			const FVector2f C(GX0 + IX * GCell, GZ0 + IZ * GCell);
			const bool bLand = H > 0.5f;
			if (bLand) { LandSum += C; LandN++; }
			if (FMath::Abs(C.X) > Area.X + 60.f || FMath::Abs(C.Y) > Area.Y + 60.f || (IX + IZ) % 2) continue;
			// a cell with the other kind (land / water) within ~25 m: the shore
			bool bNear = false;
			for (int32 DZ = -Reach; DZ <= Reach && !bNear; DZ += Reach)
				for (int32 DX = -Reach; DX <= Reach && !bNear; DX += Reach)
				{
					const int32 X = IX + DX, Z = IZ + DZ;
					bNear = X >= 0 && Z >= 0 && X < GNx && Z < GNz && (GroundH[Z * GNx + X] > 0.5f) != bLand;
				}
			if (bNear) (bLand ? CoastLandPts : ShorePts).Add(C);
		}
	}
	if (LandN && (LandSum / (float)LandN).SizeSquared() > 1.f) Inland = (LandSum / (float)LandN).GetSafeNormal();
	bHaveRoost = false;
}

float FStarlingFlock::GroundAt(float X, float Z) const
{
	if (GNx < 2 || GNz < 2) return 0.f;
	const float FXi = (X - GX0) / GCell, FZi = (Z - GZ0) / GCell;
	if (FXi < 0.f || FZi < 0.f || FXi >= GNx - 1 || FZi >= GNz - 1) return 0.f;
	const int32 IX = (int32)FXi, IZ = (int32)FZi;
	const float TX = FXi - IX, TZ = FZi - IZ;
	const float* H = GroundH.GetData();
	const float A = FMath::Lerp(H[IZ * GNx + IX], H[IZ * GNx + IX + 1], TX);
	const float B = FMath::Lerp(H[(IZ + 1) * GNx + IX], H[(IZ + 1) * GNx + IX + 1], TX);
	return FMath::Max(0.f, FMath::Lerp(A, B, TZ)); // water is always >= 0
}

// --- flow field (artistic) ---------------------------------------------------------------------
// Value noise on a coarse periodic lattice that drifts across the sea; each node eases between
// random values on its own clock. Channels: preferred speed, preferred altitude, turning.
// Neighbouring birds read nearly the same values, so whole regions of a flock speed up, slow
// down, rise, sink and veer together.

float FStarlingFlock::FlowVal()
{
	const float R = Rnd() * 2.f - 1.f;
	return FMath::Sign(R) * FMath::Sqrt(FMath::Abs(R));
}

void FStarlingFlock::StepFlow(float Dt)
{
	const float K = Dt / FMath::Max(1.f, P.FlowTime);
	for (int32 N = 0; N < FN; N++)
	{
		float U = FlowU[N] + K * FlowRate[N];
		if (U >= 1.f)
		{
			U -= 1.f;
			for (int32 C = 0; C < FC; C++) { FlowFrom[N * FC + C] = FlowTo[N * FC + C]; FlowTo[N * FC + C] = FlowVal(); }
		}
		FlowU[N] = U;
		const float E = U * U * (3.f - 2.f * U);
		for (int32 C = 0; C < FC; C++) FlowNow[N * FC + C] = FMath::Lerp(FlowFrom[N * FC + C], FlowTo[N * FC + C], E);
	}
	// the pattern drifts slowly, its direction wandering
	const float A = SimTime * 0.013f;
	FlowOx += FMath::Cos(A) * 2.5f * Dt;
	FlowOz += FMath::Sin(A * 1.3f) * 2.5f * Dt;
}

void FStarlingFlock::SampleFlow(float X, float Y, float Z, float& S, float& A, float& T) const
{
	const float Sc = FMath::Max(1.f, P.FlowScale);
	const float Gx = (X - FlowOx) / Sc, Gy = Y / (Sc * 0.5f), Gz = (Z - FlowOz) / Sc;
	const int32 IX = FMath::FloorToInt(Gx), IY = FMath::FloorToInt(Gy), IZ = FMath::FloorToInt(Gz);
	float TX = Gx - IX, TY = Gy - IY, TZ = Gz - IZ;
	TX = TX * TX * (3 - 2 * TX); TY = TY * TY * (3 - 2 * TY); TZ = TZ * TZ * (3 - 2 * TZ);
	const int32 X0 = ((IX % FX) + FX) % FX, Y0 = ((IY % FY) + FY) % FY, Z0 = ((IZ % FZ) + FZ) % FZ;
	const int32 X1 = (X0 + 1) % FX, Y1 = (Y0 + 1) % FY, Z1 = (Z0 + 1) % FZ;
	S = A = T = 0.f;
	for (int32 C = 0; C < 8; C++)
	{
		const int32 Xi = C & 1 ? X1 : X0, Yi = C & 2 ? Y1 : Y0, Zi = C & 4 ? Z1 : Z0;
		const float W = (C & 1 ? TX : 1 - TX) * (C & 2 ? TY : 1 - TY) * (C & 4 ? TZ : 1 - TZ);
		const int32 O = ((Zi * FY + Yi) * FX + Xi) * FC;
		S += FlowNow[O] * W; A += FlowNow[O + 1] * W; T += FlowNow[O + 2] * W;
	}
}

// --- predator (peregrine falcon) ---------------------------------------------------------------

void FStarlingFlock::SetPredator(bool bOn)
{
	if (bOn == bPredatorOn) return;
	bPredatorOn = bOn;
	if (bOn)
	{
		PredMode = 0; PredT = 0.f;
		// in from off to the side, high
		const int32 G = LargestGroup(EMode::Local, EMode::Shore);
		const FVector3f GC = G >= 0 ? Groups[G].C : FVector3f::ZeroVector;
		const float A = Rnd() * 2.f * PI;
		PredPos = FVector3f(GC.X + FMath::Cos(A) * 200.f, 140.f, GC.Z + FMath::Sin(A) * 200.f);
		PredVel = FVector3f(-FMath::Cos(A) * 12.f, 0.f, -FMath::Sin(A) * 12.f);
	}
	else
	{
		for (int32 I = 0; I < Count; I++) if (State[I] == Escaping) State[I] = Flying;
	}
}

void FStarlingFlock::StepPredator(float Dt)
{
	if (!bPredatorOn || Count == 0) return;
	PredT += Dt;
	FVector3f T(0.f, 120.f, 0.f);
	float Want = 15.f;
	FVector3f M = FVector3f::ZeroVector;
	int32 N = 0;
	for (int32 I = 0; I < Count; I += 11)
	{
		if (!IsFlyingBird(I)) continue;
		M += FVector3f(Px[I], Py[I], Pz[I]); N++;
	}
	if (!N) return;
	M /= (float)N;
	if (PredMode == 0)
	{
		// circle high above the flock, then pick a bird at the edge
		const float A = PredT * 0.25f;
		T = FVector3f(M.X + FMath::Cos(A) * 90.f, M.Y + 45.f, M.Z + FMath::Sin(A) * 90.f);
		if (PredT > 6.f)
		{
			int32 Best = -1;
			float BD = -1.f;
			for (int32 S = 0; S < 40; S++)
			{
				const int32 J = FMath::Min(Count - 1, (int32)(Rnd() * Count));
				if (!IsFlyingBird(J)) continue;
				const float D = Hyp3(Px[J] - M.X, Py[J] - M.Y, Pz[J] - M.Z);
				if (D > BD) { BD = D; Best = J; }
			}
			PredTarget = Best; PredMode = 1; PredT = 0.f;
		}
	}
	else if (PredMode == 1)
	{
		// stoop at the target with a pursuit lead
		const int32 J = PredTarget;
		if (J < 0 || J >= Count || !IsFlyingBird(J)) { PredMode = 2; PredT = 0.f; return; }
		const float D = Hyp3(Px[J] - PredPos.X, Py[J] - PredPos.Y, Pz[J] - PredPos.Z);
		const float LeadT = FMath::Min(1.2f, D / 30.f);
		T = FVector3f(Px[J] + Vx[J] * LeadT, Py[J] + Vy[J] * LeadT, Pz[J] + Vz[J] * LeadT);
		Want = 26.f;
		if (PredT > 7.f || D < 1.5f) { PredMode = 2; PredT = 0.f; }
	}
	else
	{
		// pull out and climb away
		T = FVector3f(PredPos.X + PredVel.X * 3.f, M.Y + 60.f, PredPos.Z + PredVel.Z * 3.f);
		Want = 18.f;
		if (PredT > 5.f) { PredMode = 0; PredT = 0.f; }
	}
	const FVector3f D = T - PredPos;
	const float DL = D.Length() + 1e-6f;
	const float K = PredMode == 1 ? 4.f : 1.2f;
	PredVel += ((D / DL) * Want - PredVel) * (K * Dt);
	PredPos += PredVel * Dt;
	// the falcon too pulls out above the water
	const float GP = GroundAt(PredPos.X, PredPos.Z) + 2.f;
	if (PredPos.Y < GP)
	{
		PredPos.Y = GP;
		PredVel.Y = FMath::Max(0.f, PredVel.Y);
		if (PredMode == 1) { PredMode = 2; PredT = 0.f; }
	}
}

// --- social forces -----------------------------------------------------------------------------
// What each bird makes of its neighbours (StarDisplay):
//  - topological neighbours: the k nearest (Ballerini et al. 2008), found with a spatial hash;
//  - separation from all k nearest, in every direction; alignment and cohesion only toward
//    visible ones (rear blind angle);
//  - cohesion scaled by centrality, measured over a slightly wider set, so birds at the rim pull
//    in hard and birds deep inside barely at all;
//  - speed alignment (Cavagna et al. 2010; Bialek et al. 2014);
//  - escape manoeuvres and roost descents are copied from neighbours (Hemelrijk et al. 2015).

void FStarlingFlock::BuildGrid()
{
	FMemory::Memzero(BucketStart.GetData(), BucketStart.Num() * sizeof(int32));
	for (int32 I = 0; I < Count; I++)
	{
		if (!InFlock(State[I])) { CellOf[I] = -1; continue; }
		const uint32 H = Hash(FMath::FloorToInt(Px[I] / CELL), FMath::FloorToInt(Py[I] / CELL), FMath::FloorToInt(Pz[I] / CELL));
		CellOf[I] = (int32)H;
		BucketStart[H + 1]++;
	}
	for (int32 H = 0; H < HSIZE; H++) BucketStart[H + 1] += BucketStart[H];
	FMemory::Memcpy(Fill.GetData(), BucketStart.GetData(), HSIZE * sizeof(int32));
	for (int32 I = 0; I < Count; I++)
	{
		if (CellOf[I] < 0) continue;
		const int32 O = Fill[CellOf[I]]++;
		Sorted[O] = I;
		SX[O] = Px[I]; SY[O] = Py[I]; SZ[O] = Pz[I];
	}
}

/** The k nearest birds of bird i, all directions; visibility is recorded, not filtered. */
int32 FStarlingFlock::Knn(int32 I, int32 K, float CosBlind, float MaxRange, int32* NIdx, float* ND2, uint8* NVis) const
{
	int32 N = 0;
	const float X = Px[I], Y = Py[I], Z = Pz[I];
	const float Sp = Hyp3(Vx[I], Vy[I], Vz[I]) + 1e-6f;
	const float Fx = Vx[I] / Sp, Fy = Vy[I] / Sp, Fz = Vz[I] / Sp;
	const int32 Cx = FMath::FloorToInt(X / CELL), Cy = FMath::FloorToInt(Y / CELL), Cz = FMath::FloorToInt(Z / CELL);
	// distance from the bird to the walls of its own cell: the searched block reaches at least
	// r * CELL + edge in every direction
	const float CellEdge = FMath::Min3(FMath::Min(X - Cx * CELL, (Cx + 1) * CELL - X), FMath::Min(Y - Cy * CELL, (Cy + 1) * CELL - Y),
		FMath::Min(Z - Cz * CELL, (Cz + 1) * CELL - Z));
	const int32 MaxR = FMath::Min(9, FMath::CeilToInt(MaxRange / CELL));
	const float Range2 = MaxRange * MaxRange;
	for (int32 R = 0; R <= MaxR; R++)
	{
		for (int32 DX = -R; DX <= R; DX++)
		{
			for (int32 DY = -R; DY <= R; DY++)
			{
				const bool bShellXY = DX == -R || DX == R || DY == -R || DY == R;
				const int32 Step = bShellXY ? 1 : FMath::Max(1, 2 * R);
				for (int32 DZ = -R; DZ <= R; DZ += Step)
				{
					const uint32 H = Hash(Cx + DX, Cy + DY, Cz + DZ);
					const int32 End = BucketStart[H + 1];
					for (int32 S = BucketStart[H]; S < End; S++)
					{
						const float DDX = SX[S] - X, DDY = SY[S] - Y, DDZ = SZ[S] - Z;
						const float D2 = DDX * DDX + DDY * DDY + DDZ * DDZ;
						if (D2 > Range2 || D2 < 1e-8f) continue; // also skips the bird itself
						if (N == K && D2 >= ND2[K - 1]) continue;
						const int32 J = Sorted[S];
						// hash collisions can revisit a bucket
						bool bDup = false;
						for (int32 Q = 0; Q < N; Q++) if (NIdx[Q] == J) { bDup = true; break; }
						if (bDup) continue;
						int32 Pq = N < K ? N++ : K - 1;
						while (Pq > 0 && ND2[Pq - 1] > D2)
						{
							ND2[Pq] = ND2[Pq - 1]; NIdx[Pq] = NIdx[Pq - 1]; NVis[Pq] = NVis[Pq - 1];
							Pq--;
						}
						ND2[Pq] = D2;
						NIdx[Pq] = J;
						NVis[Pq] = (DDX * Fx + DDY * Fy + DDZ * Fz) / FMath::Sqrt(D2) >= CosBlind ? 1 : 0;
					}
				}
			}
		}
		const float Reach = R * CELL + CellEdge;
		if (N == K && ND2[K - 1] <= Reach * Reach) break;
	}
	return N;
}

/** Social steering of one bird: Out = ax ay az, escape, ex ey ez, descend. */
void FStarlingFlock::SolveBird(int32 I, float Rdt, const FVector3f& Centre, bool bHaveCentre, float* Out) const
{
	const int32 K = FMath::Clamp(P.Neighbours, 1, MAXK);
	const int32 KWide = FMath::Min(MAXK, K + 3);
	const float CosBlind = -FMath::Cos(FMath::DegreesToRadians(P.BlindAngle * 0.5f));
	const float Ws = P.Separation / MASS, Wa = P.Alignment / MASS, Wc = P.Cohesion / MASS;
	const float Sig2 = P.SeparationSigma * P.SeparationSigma;
	int32 NIdx[MAXK];
	float ND2[MAXK];
	uint8 NVis[MAXK];

	Out[3] = 0.f;
	Out[7] = 0.f;
	const uint8 St = State[I];
	if (!InFlock(St)) { Out[0] = Out[1] = Out[2] = 0.f; return; }
	const float Sp = Hyp3(Vx[I], Vy[I], Vz[I]) + 1e-3f;
	const float Fx = Vx[I] / Sp, Fy = Vy[I] / Sp, Fz = Vz[I] / Sp;
	const int32 NCount = Knn(I, KWide, CosBlind, P.MaxRange, NIdx, ND2, NVis);

	float Ax = 0.f, Ay = 0.f, Az = 0.f;
	if (NCount > 0)
	{
		float Sx = 0, Sy = 0, Sz = 0;   // separation
		float Hx = 0, Hy = 0, Hz = 0;   // sum of neighbour headings
		float Vs = 0;                   // sum of neighbour speeds
		float Ux = 0, Uy = 0, Uz = 0;   // sum of unit vectors to visible neighbours
		float Cx = 0, Cy = 0, Cz = 0;   // sum of offsets to neighbours, m
		float Bx = 0, By = 0, Bz = 0;   // offsets to all k (for a bird that sees no one)
		int32 EscJ = -1, NSep = 0, NSoc = 0, NVisAll = 0, Settled = 0;
		for (int32 Q = 0; Q < NCount; Q++)
		{
			const int32 J = NIdx[Q];
			const float D = FMath::Sqrt(ND2[Q]);
			const float DX = (Px[J] - Px[I]) / D, DY = (Py[J] - Py[I]) / D, DZ = (Pz[J] - Pz[I]) / D;
			if (Q < K)
			{
				const float E = D <= P.HardRadius ? 1.f : FMath::Exp(-FMath::Square(D - P.HardRadius) / Sig2);
				Sx -= DX * E; Sy -= DY * E; Sz -= DZ * E;
				Bx += DX * D; By += DY * D; Bz += DZ * D;
				NSep++;
			}
			if (!NVis[Q]) continue;
			Ux += DX; Uy += DY; Uz += DZ;
			NVisAll++;
			if (NSoc >= K) continue;
			NSoc++;
			const float Sj = Hyp3(Vx[J], Vy[J], Vz[J]) + 1e-6f;
			Hx += Vx[J] / Sj; Hy += Vy[J] / Sj; Hz += Vz[J] / Sj;
			Vs += Sj;
			if (D > P.HardRadius) { Cx += DX * D; Cy += DY * D; Cz += DZ * D; }
			if (State[J] == Escaping && Timer[J] > 0.05f) EscJ = J;
			if (State[J] == Descending) Settled++;
		}
		const float Inv = 1.f / FMath::Max(1, NSoc);
		Ax += Sx * Ws / NSep; Ay += Sy * Ws / NSep; Az += Sz * Ws / NSep;
		if (St != Descending)
		{
			const float HL = Hyp3(Hx, Hy, Hz);
			if (HL > 1e-6f)
			{
				Ax += (Hx / HL - Fx) * Wa; Ay += (Hy / HL - Fy) * Wa; Az += (Hz / HL - Fz) * Wa;
			}
			// speed alignment, along the bird's own heading: speed-ups and slow-downs travel
			// through the flock as density waves
			if (NSoc > 0 && P.SpeedAlign > 0.f)
			{
				const float DV = (Vs * Inv - Sp) * P.SpeedAlign;
				Ax += Fx * DV; Ay += Fy * DV; Az += Fz * DV;
			}
			// centrality: ~0 inside the flock, ~1 at its rim
			const float Xc = NVisAll ? Hyp3(Ux, Uy, Uz) / NVisAll : 0.f;
			if (NSoc > 0)
			{
				Ax += Cx * Inv * Wc * Xc; Ay += Cy * Inv * Wc * Xc; Az += Cz * Inv * Wc * Xc;
			}
			else
			{
				// everyone is behind: ease back toward them
				Ax += Bx / NSep * Wc; Ay += By / NSep * Wc; Az += Bz / NSep * Wc;
			}
		}
		// escape rolls spread from neighbour to neighbour (Hemelrijk et al. 2015)
		if (EscJ >= 0 && St == Flying && HashRnd(I, Tick) < P.EscapeContagion * Rdt)
		{
			Out[3] = 1.f;
			Out[4] = Ex[EscJ]; Out[5] = Ey[EscJ]; Out[6] = Ez[EscJ];
		}
		// descent to the roost comes in waves: a bird follows descending neighbours
		if (Phase == EPhase::Roost && St == Flying && bHaveRoost)
		{
			const bool bNear = Hyp2(Px[I] - Roost.X, Pz[I] - Roost.Z) < 200.f;
			if (bNear && HashRnd(I + 7919u, Tick) < Rdt * (0.008f + 2.f * Settled * Inv)) Out[7] = 1.f;
		}
	}
	else if (bHaveCentre)
	{
		// lost: head back to the flock
		const float DX = Centre.X - Px[I], DY = Centre.Y - Py[I], DZ = Centre.Z - Pz[I];
		const float DL = Hyp3(DX, DY, DZ) + 1e-6f;
		Ax += DX / DL * 3.f; Ay += DY / DL * 3.f; Az += DZ / DL * 3.f;
	}
	Out[0] = Ax; Out[1] = Ay; Out[2] = Az;
}

void FStarlingFlock::Social(float Dt)
{
	// birds re-read their neighbours once per reaction time, staggered so a share of the flock
	// does it each step
	const int32 ReactSteps = FMath::Max(1, FMath::RoundToInt(P.ReactionTime / Dt));
	const int32 Off = (ReactSteps - (int32)(Tick % ReactSteps)) % ReactSteps;
	const float Rdt = Dt * ReactSteps;
	BuildGrid();
	// centroid of the flying flock, to bring lost birds back
	FVector3f Centre = FVector3f::ZeroVector;
	int32 MN = 0;
	for (int32 I = 0; I < Count; I += 13)
	{
		if (!InFlock(State[I])) continue;
		Centre += FVector3f(Px[I], Py[I], Pz[I]); MN++;
	}
	if (MN) Centre /= (float)MN;
	const int32 NSolve = Off < Count ? (Count - Off + ReactSteps - 1) / ReactSteps : 0;
	ParallelFor(NSolve, [&](int32 N)
	{
		const int32 I = Off + N * ReactSteps;
		SolveBird(I, Rdt, Centre, MN > 0, &SocialOut[I * OUT_STRIDE]);
	}, NSolve < 256);
	for (int32 I = Off; I < Count; I += ReactSteps)
	{
		const float* O = &SocialOut[I * OUT_STRIDE];
		Sfx[I] = O[0]; Sfy[I] = O[1]; Sfz[I] = O[2];
		if (State[I] != Flying) continue;
		if (O[3] > 0.f && Refractory[I] <= 0.f && EscDelay[I] <= 0.f)
		{
			// copy the neighbour's roll after a short delay
			EscDelay[I] = 0.01f + Rnd() * 0.05f;
			NextEx[I] = O[4]; NextEy[I] = O[5]; NextEz[I] = O[6];
		}
		if (O[7] > 0.f && Phase == EPhase::Roost && Groups[Grp[I]].Mode == EMode::Local) StartDescent(I);
	}
}

// --- step ----------------------------------------------------------------------------------------

void FStarlingFlock::Step(float Dt)
{
	const float V0 = P.CruiseSpeed;
	const float MaxBank = FMath::DegreesToRadians(P.MaxBank);
	const float LoadMax = P.MaxLift;
	const float RollK = P.RollRate * Dt;
	const float NoiseA = P.Noise / MASS;

	Tick++;
	SimTime += Dt;
	if (bClockOn)
	{
		Hours = Wrap24(Hours + HRate * Dt);
		if (ActivityAt(Hours) != Act) EnterActivity(ActivityAt(Hours));
	}
	StepPredator(Dt);
	StepRegime(Dt);
	StepDay(Dt);
	StepFlow(Dt);
	StepLure(Dt);
	StepGroups(Dt);
	int32 FlyingN = 0;
	for (int32 G = 0; G < GMAX; G++) if (Groups[G].Mode != EMode::Free) FlyingN += Groups[G].N;
	MaybeImpulse(Dt, FlyingN);
	if (Count == 0) return;
	Social(Dt);

	const float FlowAmp = FMath::Clamp(P.Flow, 0.f, 2.f);
	// a gathered murmuration holds together better: it shears, stretches and layers less
	const float FlowTurn = FlowAmp * (0.8f - 0.25f * GatherNow);
	const float FlowSpd = 0.15f * FlowAmp * (1.f - 0.3f * GatherNow);
	const float FlowAltK = FlowAmp * (1.f - 0.5f * GatherNow);
	const float LeadA = P.LeaderWeight / MASS, FollowA = LeadA * P.FollowShare;
	const float ImpA = 2.f * P.LeaderWeight / MASS;
	const float StreamA = 0.18f / MASS;
	const float Skim = FMath::Max(0.3f, P.SkimHeight);
	const float GTopTest = GroundTop + FMath::Max(30.f, P.LandClearance + 15.f);
	for (FImpulse& Im : Impulses)
	{
		Im.At = FVector3f(Px[Im.SeedBird], Py[Im.SeedBird], Pz[Im.SeedBird]);
		Im.N = 0;
	}
	// per flock: the informed patch, the way it is asked to turn, its cohesion radius, its shape
	for (int32 G = 0; G < GMAX; G++)
	{
		FGroup& Gr = Groups[G];
		const int32 L = Gr.Lead;
		if (Gr.Mode == EMode::Free || L < 0 || L >= Count) { Gr.LeadR2 = -1.f; Gr.LeadPos = FVector3f(1e9f); }
		else { Gr.LeadPos = FVector3f(Px[L], Py[L], Pz[L]); Gr.LeadR2 = P.InformedFraction > 0.f ? Gr.R * Gr.R : -1.f; }
		// the flock's heading, rotated toward the goal by at most 60 degrees, so a turn is led
		// progressively instead of an about-face
		{
			const float DX = Gr.Goal.X - Gr.C.X, DZ = Gr.Goal.Z - Gr.C.Z, DL = Hyp2(DX, DZ) + 1e-6f;
			const float HL = Gr.V.Length();
			if (HL < 0.5f) { Gr.Dx = DX / DL; Gr.Dz = DZ / DL; }
			else
			{
				const float HX = Gr.V.X / HL, HZ = Gr.V.Y / HL, UX = DX / DL, UZ = DZ / DL;
				const float A = FMath::Clamp(FMath::Atan2(HX * UZ - HZ * UX, HX * UX + HZ * UZ), -MAX_TURN, MAX_TURN);
				const float C = FMath::Cos(A), S = FMath::Sin(A);
				Gr.Dx = HX * C - HZ * S; Gr.Dz = HX * S + HZ * C;
			}
		}
		Gr.Pull = 10.f + 0.3f * FMath::Sqrt((float)FMath::Max(1, Gr.N));
		// formation shaping: pull strength, vertical weight of the pull, altitude spread, radius
		switch (Gr.Form)
		{
		case EForm::Stream: Gr.Pk = 0.5f; Gr.Pv = 0.5f; Gr.AltM = 0.6f; Gr.Pr = 4.f; break;  // free to string out
		case EForm::Ball: Gr.Pk = 2.2f; Gr.Pv = 1.f; Gr.AltM = 3.f; Gr.Pr = 0.55f; break;    // round, deep
		case EForm::Sheet: Gr.Pk = 1.2f; Gr.Pv = 0.f; Gr.AltM = 0.25f; Gr.Pr = 1.1f; break;  // wide, thin
		case EForm::Funnel: Gr.Pk = 1.5f; Gr.Pv = 0.3f; Gr.AltM = 2.f; Gr.Pr = 0.8f; break;
		default: Gr.Pk = 1.f; Gr.Pv = 0.5f; Gr.AltM = 1.f; Gr.Pr = 1.f;
		}
		Gr.SumP = FVector3f::ZeroVector; Gr.SumV = FVector2f::ZeroVector; Gr.Cnt = 0; Gr.In = 0;
		Gr.EdgeS = Gr.TipS = -1e30f; Gr.NewEdge = Gr.NewTip = -1;
	}
	const float PullK = 0.6f + 0.6f * GatherNow; // m/s2 per radius outside a flock's core

	int32 Perched_ = 0, Away_ = 0, Air = 0;
	for (int32 I = 0; I < Count; I++)
	{
		const uint8 St = State[I];
		NextEscape[I] = 0;
		if (St == Away) { Away_++; continue; }
		if (St == Perched) { Perched_++; Fold[I] = 1.f; FlapAmp[I] = 0.f; continue; }
		if (St == Grounded) { StepGrounded(I, Dt); continue; }
		if (St == Visiting)
		{
			const int32 S = SpotOf[I];
			if (S < 0 || S >= Lure.Spots.Num()) { TakeOff(I, Grp[I], Lure.Hand.X, Lure.Hand.Z); continue; }
			const FVector3f Sp = Lure.Spots[S];
			const float HD = Hyp2(Sp.X - Px[I], Sp.Z - Pz[I]);
			// come in from the open side: a bird beside or behind its spot first flies round to a
			// point 20 m out (away from the land), 5 m up
			const FVector2f Out = -Inland;
			const float Side = (Px[I] - Sp.X) * Out.X + (Pz[I] - Sp.Z) * Out.Y;
			if (Side > 6.f || HD < 4.f) FinalApp[I] = 1;
			const bool bFinal = FinalApp[I] == 1;
			const FVector3f Aim = bFinal ? Sp : Sp + FVector3f(Out.X, 0.f, Out.Y) * 20.f;
			// glide in: aim above the spot, lower as it gets closer
			const float TY = bFinal ? Sp.Y + FMath::Min(25.f, HD * 0.22f) * Smooth(1.5f, 8.f, HD) : Sp.Y + 5.f;
			const FVector3f T(Aim.X, TY, Aim.Z);
			const float DL = FVector3f::Dist(T, FVector3f(Px[I], Py[I], Pz[I])) + 1e-3f;
			// on the ground only when at its spot, feet on it
			if (HD < 0.25f && FMath::Abs(Py[I] - Sp.Y) < 0.05f)
			{
				State[I] = Grounded;
				Px[I] = Sp.X; Py[I] = Sp.Y; Pz[I] = Sp.Z;
				Fold[I] = 1.f; FlapAmp[I] = 0.f;
				const float A = Lure.bOn ? FMath::Atan2(Lure.Hand.Z - Sp.Z, Lure.Hand.X - Sp.X) + (Rnd() - 0.5f) : FMath::Atan2(Vz[I], Vx[I]);
				Vx[I] = FMath::Cos(A) * 0.01f; Vy[I] = 0.f; Vz[I] = FMath::Sin(A) * 0.01f;
				ActT[I] = 0.5f + Rnd() * 2.f; HopT[I] = 0.f; PeckT[I] = 0.f; Bank[I] = 0.f; HopCool[I] = 2.f + Rnd() * 3.f;
				// everyone is gone ~60 s after the food came out (or soon after it went away);
				// relocated birds keep their own time
				if (!Relocated[I]) Dwell[I] = Lure.bOn ? FMath::Max(5.f, FMath::Min(Dwell[I], 62.f - Lure.T)) : 10.f + Rnd() * 50.f;
				Relocated[I] = 0;
				continue;
			}
			SeekStep(I, T, DL, Dt, V0 * 1.05f, 0.f, false);
			if (HD < 2.f) { if (Py[I] < Sp.Y) { Py[I] = Sp.Y; if (Vy[I] < 0.f) Vy[I] = 0.f; } }
			else FloorClamp(I, MIN_CLEAR);
			const float Close = Smooth(6.f, 0.f, HD);
			Fold[I] = 0.f; Bank[I] *= 1.f - FMath::Min(1.f, RollK);
			FlapAmp[I] = 0.55f + Close * 0.45f;
			FlapPhase[I] += FlapFreq[I] * 2.f * PI * Dt;
			continue;
		}

		const float Sp = Hyp3(Vx[I], Vy[I], Vz[I]) + 1e-3f;
		const float Fx = Vx[I] / Sp, Fy = Vy[I] / Sp, Fz = Vz[I] / Sp;
		float Ax = Sfx[I], Ay = Sfy[I], Az = Sfz[I];

		if (St == Descending)
		{
			// seek the perch, slowing down on arrival
			FVector3f T;
			PerchPos(I, T);
			const float DL = FVector3f::Dist(T, FVector3f(Px[I], Py[I], Pz[I])) + 1e-3f;
			if (DL < 0.5f) { Settle(I, T); continue; }
			SeekStep(I, T, DL, Dt, V0 * 1.1f, 0.3f);
			if (DL >= 2.f) FloorClamp(I, MIN_CLEAR);
			else if (Py[I] < T.Y) Py[I] = T.Y;
			const float Close = FMath::Clamp((8.f - DL) / 8.f, 0.f, 1.f);
			Fold[I] = Close * 0.3f;
			FlapAmp[I] = 0.5f + Close * 0.5f;
			FlapPhase[I] += FlapFreq[I] * 2.f * PI * Dt;
			Bank[I] *= 1.f - FMath::Min(1.f, RollK);
			continue;
		}

		Air++;
		int32 G = Grp[I];
		if (G >= GMAX || Groups[G].Mode == EMode::Free)
		{
			// its flock is gone: join the nearest one
			G = NearestGroup(FVector3f(Px[I], Py[I], Pz[I]));
			Grp[I] = (uint8)G;
		}
		FGroup& Gr = Groups[G];
		const EMode Mode = Gr.Mode;
		// flock bookkeeping (for next step); leaders and turn initiators come from the body of the
		// flock, not from stragglers (a straggler skimming the sea would lead it down to the water)
		Gr.SumP += FVector3f(Px[I], Py[I], Pz[I]); Gr.SumV += FVector2f(Vx[I], Vz[I]); Gr.Cnt++;
		{
			const float OX = Px[I] - Gr.C.X, OZ = Pz[I] - Gr.C.Z;
			const float TipD = OX * OX + OZ * OZ;
			if (TipD < FMath::Square(1.5f * Gr.Pull * Gr.Pr) && FMath::Abs(Py[I] - Gr.C.Y) < 20.f)
			{
				const float E = OX * Gr.Dx + OZ * Gr.Dz;
				if (E > Gr.EdgeS) { Gr.EdgeS = E; Gr.NewEdge = I; }
				if (TipD > Gr.TipS) { Gr.TipS = TipD; Gr.NewTip = I; }
			}
		}

		const float Ground0 = GroundAt(Px[I], Pz[I]);
		// area attraction: a turning force (lateral only, StarDisplay Eq. 11), stronger when the bird
		// heads away. At dusk it is the roost; otherwise the area over the water. Commuters are free.
		if (Mode != EMode::Out && Mode != EMode::In)
		{
			float DX = 0.f, DZ = 0.f, Gain = 1.f, Soft = P.RoostRadius;
			if (Phase == EPhase::Roost && bHaveRoost)
			{
				DX = Roost.X - Px[I]; DZ = Roost.Z - Pz[I];
				const float D = Hyp2(DX, DZ);
				if (D > P.RoostRadius) { const float K = (D - P.RoostRadius) / (D + 1e-6f); DX *= K; DZ *= K; }
				else DX = DZ = 0.f;
			}
			else
			{
				// the area; outside it, pull back in firmly
				Gain = 2.f; Soft = 40.f;
				const float AX = Area.X + (Mode == EMode::Shore ? 40.f : 0.f), AZ = Area.Y + (Mode == EMode::Shore ? 40.f : 0.f);
				if (Px[I] > AX) DX = AX - Px[I]; else if (Px[I] < -AX) DX = -AX - Px[I];
				if (Pz[I] > AZ) DZ = AZ - Pz[I]; else if (Pz[I] < -AZ) DZ = -AZ - Pz[I];
				// the land ahead: turn back out over the water well before it (shore flocks may
				// skirt it); not at dusk, when the roost is on land
				const float LookX = Px[I] + Fx * 40.f, LookZ = Pz[I] + Fz * 40.f;
				if (Mode == EMode::Local && (Ground0 > 0.5f || IsLand(LookX, LookZ)))
				{
					DX += -Inland.X * 30.f; DZ += -Inland.Y * 30.f; Gain = 4.f; Soft = 15.f;
				}
			}
			const float D = Hyp2(DX, DZ);
			if (D > 0.f)
			{
				const float AwayK = -(DX * Fx + DZ * Fz) / D; // +1 when flying straight out
				const float S = (P.AreaPull / MASS) * Gain * (0.5f + 0.5f * AwayK) * (1.f + D / Soft);
				// lateral part only: the bird turns rather than brakes
				float LX = DX / D, LZ = DZ / D;
				const float Along = LX * Fx + LZ * Fz;
				LX -= Along * Fx; LZ -= Along * Fz;
				Ax += LX * S; Az += LZ * S;
			}
		}

		// the flow field this bird is in (artistic)
		float FlowS = 0.f, FlowA = 0.f;
		if (FlowAmp > 0.f)
		{
			if ((I & 3) == (int32)(Tick & 3)) SampleFlow(Px[I], Py[I], Pz[I], FlS[I], FlA[I], FlT[I]);
			FlowS = FlS[I] * FlowSpd;
			FlowA = FlA[I] * FlowAltK * FMath::Clamp((Gr.Band - 4.f) * 0.6f, 2.f, 20.f); // smaller swings near the sea
			// veer left or right with the region: shear that stretches and folds a flock
			const float TL = Hyp2(Fx, Fz);
			if (TL > 0.2f) { const float T = FlT[I] * FlowTurn / TL; Ax += Fz * T; Az -= Fx * T; }
		}

		// where the flock is going: informed birds steer for the goal (3D), everyone else feels it
		// weakly and only horizontally
		float AltWant = Gr.Band;
		{
			const float Gx = Gr.Goal.X - Px[I], Gy = Gr.Goal.Y - Py[I], Gz = Gr.Goal.Z - Pz[I];
			const float GH = Hyp2(Gx, Gz) + 1e-6f;
			const FVector3f LD = FVector3f(Px[I], Py[I], Pz[I]) - Gr.LeadPos;
			const bool bCommute = Mode == EMode::Out || Mode == EMode::In;
			if (LD.SizeSquared() < Gr.LeadR2)
			{
				Gr.In++;
				// the informed birds lead the climb or dive, a little ahead of the flock
				AltWant = Gr.Band + FMath::Clamp(Gr.Goal.Y - Gr.Band, -8.f, 8.f);
				// climb or dive at up to ~14 degrees (commuters climb out at up to ~24)
				const float GMax = bCommute ? 0.45f : 0.25f;
				const float Gyn = FMath::Clamp((Mode == EMode::Out ? FMath::Max(Gy, 40.f) : Gy) / FMath::Max(GH, 20.f), -GMax, GMax);
				const float GL = Hyp2(1.f, Gyn);
				Ax += (Gr.Dx / GL - Fx) * LeadA; Ay += (Gyn / GL - Fy) * LeadA; Az += (Gr.Dz / GL - Fz) * LeadA;
			}
			else if (Gr.Form == EForm::Stream && Gr.Head > Gr.TrailStart)
			{
				// pour along the leaders' trail: aim at the next trail point not yet passed
				const int32 First = FMath::Max(Gr.TrailStart, Gr.Head - TRAIL);
				int32 Q = TrailSeq[I];
				if (Q < First || Q >= Gr.Head) Q = First;
				for (int32 K = 0; K < 4 && Q < Gr.Head - 1; K++)
				{
					const FVector3f& A = Gr.Trail[Q % TRAIL];
					const FVector3f& B = Gr.Trail[(Q + 1) % TRAIL];
					const FVector3f T(A.X - Px[I], A.Y - Py[I], A.Z - Pz[I]);
					// passed it (it lies behind along the path), or close: on to the next
					if (FVector3f::DotProduct(T, B - A) < 0.f || T.SizeSquared() < 25.f) Q++;
					else break;
				}
				TrailSeq[I] = Q;
				const FVector3f& A = Gr.Trail[Q % TRAIL];
				// each bird keeps its own place across the tube, so the stream is a thick ribbon
				const float Tube = 2.f + 0.1f * FMath::Sqrt((float)Gr.N);
				const float TX = A.X + Tube * FMath::Clamp(SpeedN[I] * 0.6f, -1.5f, 1.5f) - Px[I];
				const float TY = A.Y + Tube * 0.4f * FMath::Clamp(RollN[I] * 0.6f, -1.5f, 1.5f) - Py[I];
				const float TZ = A.Z + Tube * FMath::Clamp(Mood[I] * 0.6f, -1.5f, 1.5f) - Pz[I];
				const float TL = Hyp3(TX, TY, TZ) + 1e-6f;
				if (Q < Gr.Head - 1 || TL > 8.f)
				{
					Ax += (TX / TL - Fx) * StreamA; Ay += (TY / TL - Fy) * StreamA; Az += (TZ / TL - Fz) * StreamA;
					AltWant = A.Y;
				}
			}
			else
			{
				const float HL = Hyp2(Fx, Fz) + 1e-6f;
				const float FA = bCommute ? FollowA * 3.f : FollowA;
				Ax += (Gr.Dx - Fx / HL) * FA; Az += (Gr.Dz - Fz / HL) * FA;
			}
			// stay with the flock: a pull toward its centre from outside its radius (this is what
			// makes merging flocks fly to each other)
			const float CX = Gr.C.X - Px[I], CY = (Gr.C.Y - Py[I]) * Gr.Pv, CZ = Gr.C.Z - Pz[I];
			const float CD = Hyp3(CX, CY, CZ);
			const float Over = CD / (Gr.Pull * Gr.Pr) - 0.6f;
			if (Over > 0.f && Gr.Pk > 0.f)
			{
				const float S = PullK * Gr.Pk * FMath::Min(Over, 4.f) / CD;
				float QX = CX * S, QY = CY * S, QZ = CZ * S;
				// never a brake: a bird ahead of its flock is not turned back, only steered in
				const float Along = QX * Fx + QY * Fy + QZ * Fz;
				if (Along < 0.f) { QX -= Along * Fx; QY -= Along * Fy; QZ -= Along * Fz; }
				Ax += QX; Ay += QY; Az += QZ;
			}
		}
		// spontaneous turns: the birds near the initiator turn to its new heading
		for (FImpulse& Im : Impulses)
		{
			const float DX = Px[I] - Im.At.X, DY = Py[I] - Im.At.Y, DZ = Pz[I] - Im.At.Z;
			if (DX * DX + DY * DY + DZ * DZ < Im.R * Im.R)
			{
				Im.N++;
				Ax += (Im.Dir.X - Fx) * ImpA; Ay += (Im.Dir.Y - Fy) * ImpA; Az += (Im.Dir.Z - Fz) * ImpA;
			}
		}

		// preferred altitude: the flock's band, the bird's own offset and the flow field
		const float Alt = FMath::Clamp(AltWant + AltN[I] * P.AltitudeSpread * Gr.AltM * (AltWant < 15.f ? 0.3f : 1.f) + FlowA, 2.5f, 175.f);
		Ay += FMath::Clamp((P.AltitudePull / MASS) * (Alt - Py[I]), -2.f, 2.f) - 0.3f * Vy[I];
		// over land: well above the hill and people's heads (shore flocks lower). This floor is
		// firmer than the altitude spring, so the flock's own pull can't hold birds low there
		float FloorUrg = 0.f;
		if (Ground0 > 0.5f)
		{
			const float Lift = Mode == EMode::Shore ? 6.f : (Mode == EMode::Out || Mode == EMode::In) ? P.LandClearance * 0.7f : P.LandClearance;
			const float Floor = Ground0 + Lift;
			if (Py[I] < Floor)
			{
				Ay += FMath::Min(8.f, (Floor - Py[I]) * 0.8f) + FMath::Max(0.f, -Vy[I]) * 1.5f;
				FloorUrg = FMath::Clamp((Floor - Py[I]) / 8.f, 0.f, 1.f);
			}
		}
		// noise (uniform, same variance as gauss(): one draw per axis)
		Ax += (Rnd() - 0.5f) * 1.15f * NoiseA; Ay += (Rnd() - 0.5f) * 0.575f * NoiseA; Az += (Rnd() - 0.5f) * 1.15f * NoiseA;

		// predator: roll away from it
		if (bPredatorOn && St == Flying && Refractory[I] <= 0.f)
		{
			const float DX = Px[I] - PredPos.X, DY = Py[I] - PredPos.Y, DZ = Pz[I] - PredPos.Z;
			const float D = Hyp3(DX, DY, DZ);
			const bool bApproaching = DX * PredVel.X + DY * PredVel.Y + DZ * PredVel.Z > 0.f;
			if (D < 9.f || (D < 26.f && bApproaching))
			{
				NextEscape[I] = 1;
				// away from the predator's path, sideways and down
				const float PL = PredVel.Length() + 1e-6f;
				const float Along = (DX * PredVel.X + DY * PredVel.Y + DZ * PredVel.Z) / PL;
				const float OX = DX - Along * PredVel.X / PL, OY = DY - Along * PredVel.Y / PL, OZ = DZ - Along * PredVel.Z / PL;
				const float OL = Hyp3(OX, OY, OZ) + 1e-6f;
				NextEx[I] = OX / OL; NextEy[I] = OY / OL - 0.3f; NextEz[I] = OZ / OL;
			}
		}
		if (St == Escaping)
		{
			const float S = 14.f * FMath::Min(1.f, Timer[I] * 3.f);
			Ax += Ex[I] * S; Ay += Ey[I] * S; Az += Ez[I] * S;
		}

		// the surface: look ~1 s ahead along the flight path and pull up harder the closer that
		// point is to the sea or the ground
		float Urgency = FloorUrg;
		if (Py[I] < GTopTest)
		{
			const float QY = Py[I] + Vy[I];
			const float G1 = GroundAt(Px[I] + Vx[I], Pz[I] + Vz[I]);
			const float Clr = FMath::Min(QY - G1, Py[I] - Ground0) - Skim;
			// the faster it sinks, the earlier it reacts; level skimming is left alone
			const float H = 1.5f + FMath::Max(0.f, -Vy[I]) * 0.7f;
			if (Clr < H)
			{
				Urgency = FMath::Max(Urgency, FMath::Clamp(1.f - Clr / H, 0.f, 1.5f));
				const float U1 = FMath::Min(1.f, Urgency);
				Ay += U1 * U1 * 25.f + U1 * FMath::Max(0.f, -Vy[I]) * 5.f;
				// escape dives and climbs toward the goal give way near the surface
				if (Ay < 0.f) Ay *= 1.f - U1;
			}
		}

		// --- flight mechanics ---
		const float Fpar = Ax * Fx + Ay * Fy + Az * Fz;
		const float PerpX = Ax - Fpar * Fx, PerpY = Ay - Fpar * Fy, PerpZ = Az - Fpar * Fz;
		// lift needed = desired lateral acceleration + support against gravity
		float LX = PerpX - GRAV * Fy * Fx, LY = PerpY + GRAV - GRAV * Fy * Fy, LZ = PerpZ - GRAV * Fy * Fz;
		// body axes without bank: side axis = up x forward (horizontal)
		float RX = Fz, RZ = -Fx;
		float RL = Hyp2(RX, RZ);
		if (RL < 1e-4f) { RX = 1.f; RZ = 0.f; RL = 1.f; }
		RX /= RL; RZ /= RL;
		// up0 = forward x right
		const float U0x = Fy * RZ, U0y = Fz * RX - Fx * RZ, U0z = -Fy * RX;
		const float EscapeRoll = St == Escaping ? 1.6f : 1.f;
		const float Urg = FMath::Min(1.f, Urgency);
		// (a bird asked to dive keeps its wings near level rather than flipping over)
		float Target = FMath::Atan2(-(LX * RX + LZ * RZ), FMath::Max(LX * U0x + LY * U0y + LZ * U0z, 0.25f * GRAV));
		// near the surface the wings come level so the lift points up
		const float Lim = MaxBank * EscapeRoll * (1.f - 0.75f * Urg);
		Target = FMath::Clamp(Target, -Lim, Lim);
		const float RollI = RollK * FMath::Clamp(1.f + P.RollSpread * RollN[I], 0.4f, 1.8f) * (1.f + Urg);
		Bank[I] += (Target - Bank[I]) * FMath::Min(1.f, RollI * EscapeRoll * 1.5f);
		const float CB = FMath::Cos(Bank[I]), SB = FMath::Sin(Bank[I]);
		// banked up axis: y = -right * sin + up0 * cos
		const float YX = -RX * SB + U0x * CB, YY = U0y * CB, YZ = -RZ * SB + U0z * CB;
		// lift: along the banked up axis, limited by speed (L ~ v2); a bird about to hit the water
		// pulls harder (a brief high-g pull-up)
		const float LNeed = LX * YX + LY * YY + LZ * YZ;
		const float LMax = GRAV * (LoadMax * EscapeRoll + 2.f * Urg) * (Sp / V0) * (Sp / V0);
		const float L = FMath::Clamp(LNeed, 0.f, LMax);
		LX = L * YX; LY = L * YY; LZ = L * YZ;
		// speed: own cruise speed, mood and the flow field; faster while escaping; thrust minus
		// drag relaxes to it; gravity (added below) speeds up dives
		const float Own = V0 * FMath::Clamp(1.f + P.SpeedSpread * SpeedN[I] + P.MoodSpeed * Mood[I] + FlowS, 0.6f, 1.5f);
		const float Vt = St == Escaping ? FMath::Max(Own, V0) * 1.3f : Own;
		const float Af = (Vt - Sp) / FMath::Max(0.05f, P.SpeedTau) + FMath::Clamp(Fpar, -4.f, 4.f);
		float NVx = Vx[I] + (Af * Fx + LX) * Dt;
		float NVy = Vy[I] + (Af * Fy + LY - GRAV) * Dt;
		float NVz = Vz[I] + (Af * Fz + LZ) * Dt;
		const float NS = Hyp3(NVx, NVy, NVz);
		constexpr float SMin = 5.f, SMax = 22.f;
		if (NS < SMin || NS > SMax)
		{
			const float C = FMath::Clamp(NS, SMin, SMax) / (NS + 1e-6f);
			NVx *= C; NVy *= C; NVz *= C;
		}
		Vx[I] = NVx; Vy[I] = NVy; Vz[I] = NVz;
		Px[I] += NVx * Dt; Py[I] += NVy * Dt; Pz[I] += NVz * Dt;
		if (Urgency > 0.f)
		{
			// skimming: never sink below the skim height while heading down
			if (Py[I] < GroundAt(Px[I], Pz[I]) + Skim && Vy[I] < 0.f) Vy[I] *= 0.5f;
			FloorClamp(I, MIN_CLEAR);
		}
		// commuters far inland are gone for the day
		if (Mode == EMode::Out && Hyp2(Px[I], Pz[I]) > FMath::Max(Area.X, Area.Y) + 300.f) { MakeAway(I); continue; }

		// wings: flap harder when slow or climbing, glide in bouts otherwise
		const float Effort = FMath::Max(0.f, (V0 - Sp) * 0.25f + Fy * 2.f + (St == Escaping ? 1.f : 0.f));
		const float Bout = 0.5f + 0.5f * FMath::Sin(FlapPhase[I] * 0.06f + I);
		const float WantAmp = FMath::Min(1.f, 0.15f + Effort * 0.5f + (Bout > 0.55f ? 0.45f : 0.f));
		FlapAmp[I] += (WantAmp - FlapAmp[I]) * FMath::Min(1.f, Dt * 6.f);
		FlapPhase[I] = FMath::Fmod(FlapPhase[I] + FlapFreq[I] * 2.f * PI * Dt, 2000.f * PI);
		Fold[I] *= 0.95f;
	}

	// flocks: centroids and heading for the next step; informed patches and turn patches hold
	// their share of the flock
	for (int32 G = 0; G < GMAX; G++)
	{
		FGroup& Gr = Groups[G];
		Gr.N = Gr.Cnt;
		if (Gr.Mode == EMode::Free || !Gr.Cnt) continue;
		Gr.C = Gr.SumP / (float)Gr.Cnt;
		Gr.V = Gr.SumV / (float)Gr.Cnt;
		if (Gr.NewEdge >= 0) Gr.Edge = Gr.NewEdge;
		if (Gr.NewTip >= 0) Gr.Tip = Gr.NewTip;
		// in a stream only a narrow head knows the way
		Gr.R = AdaptRadius(Gr.R, Gr.In, P.InformedFraction * Gr.Cnt * (Gr.Form == EForm::Stream ? 0.4f : 1.f));
	}
	// a turn starts with a few birds: ~3% of their flock
	for (FImpulse& Im : Impulses) Im.R = AdaptRadius(Im.R, Im.N, FMath::Max(8.f, 0.03f * Groups[Im.G].N));
	// mood: Ornstein-Uhlenbeck, updated for an eighth of the flock each step
	{
		const float H = 8.f * Dt, Tau = FMath::Max(0.5f, P.MoodTime);
		const float A = FMath::Exp(-H / Tau), B = FMath::Sqrt(1.f - A * A);
		for (int32 I = Tick & 7; I < Count; I += 8) Mood[I] = Mood[I] * A + Normal() * B;
	}

	// apply escape state changes after the pass (synchronous update)
	for (int32 I = 0; I < Count; I++)
	{
		if (State[I] == Escaping)
		{
			Timer[I] -= Dt;
			if (Timer[I] <= 0.f) { State[I] = Flying; Refractory[I] = 1.f; }
		}
		else if (Refractory[I] > 0.f)
		{
			Refractory[I] -= Dt;
		}
		if (EscDelay[I] > 0.f)
		{
			EscDelay[I] -= Dt;
			if (EscDelay[I] <= 0.f && State[I] == Flying) NextEscape[I] = 1;
		}
		if (NextEscape[I] && State[I] == Flying)
		{
			State[I] = Escaping;
			Timer[I] = 0.55f; // roll away 0.25 s, back 0.3 s
			const float L = Hyp3(NextEx[I], NextEy[I], NextEz[I]) + 1e-6f;
			Ex[I] = NextEx[I] / L; Ey[I] = NextEy[I] / L; Ez[I] = NextEz[I] / L;
		}
	}

	PerchedLast = Perched_; AwayLast = Away_; AirLast = Air;
	if (Phase == EPhase::Murmur || Phase == EPhase::Wake)
	{
		// a wake-up or a change of mind: descending birds rejoin the flock
		for (int32 I = 0; I < Count; I++) if (State[I] == Descending) State[I] = Flying;
	}
}
