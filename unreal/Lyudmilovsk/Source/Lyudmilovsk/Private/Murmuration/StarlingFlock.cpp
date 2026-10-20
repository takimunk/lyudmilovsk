#include "Murmuration/StarlingFlock.h"

#include "Async/ParallelFor.h"

namespace
{
	constexpr float MASS = 0.08f; // kg (European starling, ~75-90 g)
	constexpr float G = 9.81f;
	constexpr float MIN_CLEAR = 0.3f;
	constexpr float BAND_TAU = 6.f;
	constexpr float BAND_RATE = 2.5f;
	constexpr float MAX_TURN = PI / 3.f; // leaders steer at most this far off the flock's heading

	FORCEINLINE float Hyp2(float A, float B) { return FMath::Sqrt(A * A + B * B); }
	FORCEINLINE float Hyp3(float A, float B, float C) { return FMath::Sqrt(A * A + B * B + C * C); }
	FORCEINLINE float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	/** A random number in [0, 1) from a bird and a step, for the parallel social solve. */
	FORCEINLINE float HashRnd(uint32 A, uint32 B)
	{
		uint32 H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u) * 0x85EBCA77u;
		H ^= H >> 15; H *= 0x2C1B3C6Du; H ^= H >> 12; H *= 0x297A2D39u; H ^= H >> 15;
		return (H >> 8) * (1.f / 16777216.f);
	}
	/** Grow or shrink a patch radius toward holding `Want` birds. */
	float AdaptRadius(float R, int32 Have, float Want)
	{
		if (Want < 1.f) return 0.f;
		return FMath::Clamp(R * (Have < Want ? 1.04f : 0.97f), 1.5f, 250.f);
	}
}

FStarlingFlock::FStarlingFlock(int32 InCount, const FMurmurationParams& InParams, FVector2f InArea, uint32 InSeed)
	: P(InParams), Area(InArea), Seed(InSeed | 1u)
{
	Count = FMath::Max(0, InCount);
	for (TArray<float>* A : { &Px, &Py, &Pz, &Vx, &Vy, &Vz, &Bank, &FlapPhase, &FlapAmp, &FlapFreq, &Timer, &Refractory,
		&Ex, &Ey, &Ez, &Sfx, &Sfy, &Sfz, &EscDelay, &NextEx, &NextEy, &NextEz, &SpeedN, &AltN, &RollN, &Mood, &FlS, &FlA, &FlT,
		&SX, &SY, &SZ })
	{
		A->SetNumZeroed(Count);
	}
	State.SetNumZeroed(Count);
	NextEscape.SetNumZeroed(Count);
	TrailSeq.SetNumZeroed(Count);
	CellOf.SetNumZeroed(Count);
	Sorted.SetNumZeroed(Count);
	BucketStart.SetNumZeroed(HSIZE + 1);
	Fill.SetNumZeroed(HSIZE);
	SocialOut.SetNumZeroed(Count * OUT_STRIDE);

	FlowFrom.SetNumZeroed(FN * FC); FlowTo.SetNumZeroed(FN * FC); FlowNow.SetNumZeroed(FN * FC);
	FlowU.SetNumZeroed(FN); FlowRate.SetNumZeroed(FN);
	for (int32 N = 0; N < FN; N++)
	{
		FlowU[N] = Rnd();
		FlowRate[N] = 0.7f + Rnd() * 0.6f;
		for (int32 C = 0; C < FC; C++) { FlowFrom[N * FC + C] = FlowVal(); FlowTo[N * FC + C] = FlowVal(); }
	}

	// a cloud arriving high up, all heading roughly one way
	GC = FVector3f(0.f, P.Altitude, 0.f);
	Band = P.Altitude;
	const float Heading = Rnd() * 2.f * PI;
	const float R0 = FMath::Min(60.f, 4.f + FMath::Sqrt((float)FMath::Max(1, Count)) * 0.55f);
	for (int32 I = 0; I < Count; I++)
	{
		SpeedN[I] = Normal(); AltN[I] = Normal(); RollN[I] = Normal(); Mood[I] = Normal();
		FlapPhase[I] = Rnd() * 2.f * PI;
		FlapFreq[I] = 11.5f + Rnd() * 2.5f; // Hz, starlings ~12-14 Hz
		FlapAmp[I] = 0.6f;
		const float A = Rnd() * 2.f * PI, R = FMath::Sqrt(Rnd()) * R0;
		Px[I] = FMath::Cos(A) * R;
		Py[I] = P.Altitude + Gauss() * 4.f;
		Pz[I] = FMath::Sin(A) * R * 0.6f;
		const float H = Heading + Gauss() * 0.1f;
		Vx[I] = FMath::Cos(H) * 10.f; Vy[I] = 0.f; Vz[I] = FMath::Sin(H) * 10.f;
		State[I] = Flying;
	}
	ChooseGoal();
}

FString FStarlingFlock::Describe() const
{
	static const TCHAR* Forms[] = { TEXT("mass"), TEXT("stream"), TEXT("ball"), TEXT("sheet") };
	return FString::Printf(TEXT("%s, goal (%.0f, %.0f, %.0f) in %.0f s, band %.0f m, informed r %.1f m, lead %d y %.0f, trail %d y %.0f"), Forms[(int32)Form],
		Goal.X, Goal.Y, Goal.Z, GoalTime, Band, LeadR, Lead, Lead >= 0 ? Py[Lead] : -1.f, Head - TrailStart, Head > 0 ? Trail[(Head - 1) % TRAIL].Y : -1.f);
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

// --- ground ----------------------------------------------------------------------------------

void FStarlingFlock::SetGround(float InX0, float InZ0, float InCell, int32 InNx, int32 InNz, TArray<float>&& Heights)
{
	GX0 = InX0; GZ0 = InZ0; GCell = FMath::Max(0.1f, InCell); GNx = InNx; GNz = InNz;
	GroundH = MoveTemp(Heights);
	GroundTop = 0.f;
	for (float H : GroundH) GroundTop = FMath::Max(GroundTop, H);
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

// --- where the flock goes ----------------------------------------------------------------------

void FStarlingFlock::ChooseGoal()
{
	// somewhere over the water, preferably a good way from where the flock is now
	const float Want = 0.5f * (Area.X + Area.Y);
	float BScore = -1e30f;
	FVector3f Best(0.f, P.Altitude, 0.f);
	for (int32 C = 0; C < 8; C++)
	{
		const float X = (Rnd() * 2.f - 1.f) * Area.X * 0.85f;
		const float Z = (Rnd() * 2.f - 1.f) * Area.Y * 0.85f;
		if (IsLand(X, Z)) continue;
		const float H = Rnd();
		const float Y = H < 0.1f ? 8.f + Rnd() * 16.f : H < 0.75f ? P.Altitude * (0.6f + Rnd() * 0.9f) : 85.f + Rnd() * 65.f;
		const float D = Hyp2(X - GC.X, Z - GC.Z);
		const float Score = -FMath::Abs(D - Want) + Rnd() * 60.f;
		if (Score > BScore) { BScore = Score; Best = FVector3f(X, Y, Z); }
	}
	Goal = Best;
	GoalTime = P.GoalInterval * (0.55f + Rnd() * 1.05f);
	// a new informed patch, on the side of the flock facing the goal (once the edge toward the
	// new goal has been found, two steps on)
	Lead = -1;
	LeadWait = 2;
	LeadR = FMath::Max(LeadR * 0.5f, 2.f);
	ChooseForm();
}

/**
 * How the flock travels to its goal. A distant goal strings it out into a stream behind its
 * leaders; a near one rolls it up into a ball or a flat sheet circling the spot.
 */
void FStarlingFlock::ChooseForm()
{
	const float D = Hyp2(Goal.X - GC.X, Goal.Z - GC.Z);
	const float R = Rnd() / FMath::Max(1e-3f, FMath::Clamp(P.Formations, 0.f, 1.f));
	Form = EForm::Mass;
	if (D > 0.6f * FMath::Max(Area.X, Area.Y) && R < 0.6f) Form = EForm::Stream;
	else if (R < 0.75f) Form = Rnd() < 0.5f ? EForm::Ball : EForm::Sheet;
	if (Form == EForm::Stream)
	{
		TrailStart = Head;
		TrailT = 0.f;
	}
	else if (Form == EForm::Ball || Form == EForm::Sheet)
	{
		// circle the goal in its own sense of turning
		OrbitC = FVector2f(Goal.X, Goal.Z);
		const float Room = FMath::Max(25.f, FMath::Min(Area.X, Area.Y) * 0.6f);
		OrbitR = FMath::Min(Room, Form == EForm::Ball ? 30.f + Rnd() * 30.f : 50.f + Rnd() * 50.f);
		OrbitW = Rnd() < 0.5f ? -1.f : 1.f;
	}
}

void FStarlingFlock::StepGroup(float Dt)
{
	GoalTime -= Dt;
	const bool bReached = Hyp2(Goal.X - GC.X, Goal.Z - GC.Z) < 45.f;
	if (GoalTime <= 0.f || (bReached && (Form == EForm::Mass || Form == EForm::Stream))) ChooseGoal();
	if (Form == EForm::Ball || Form == EForm::Sheet)
	{
		// the goal runs round the circle ahead of the flock, so the leaders circle
		const float A = FMath::Atan2(GC.Z - OrbitC.Y, GC.X - OrbitC.X) + OrbitW * 0.9f;
		Goal.X = OrbitC.X + FMath::Cos(A) * OrbitR;
		Goal.Z = OrbitC.Y + FMath::Sin(A) * OrbitR;
	}
	else if (Form == EForm::Stream)
	{
		// lay the trail: where the leader has been
		TrailT -= Dt;
		if (TrailT <= 0.f && Lead >= 0 && Lead < Count)
		{
			TrailT = TRAIL_DT;
			Trail[Head % TRAIL] = FVector3f(Px[Lead], Py[Lead], Pz[Lead]);
			Head++;
		}
	}
	if (LeadWait > 0) LeadWait--;
	else if (Lead < 0 || Lead >= Count) Lead = Edge;
	// the flock's height follows the goal at a climb or sink rate a flock can hold
	Band += FMath::Clamp((Goal.Y - Band) / BAND_TAU, -BAND_RATE, BAND_RATE) * Dt;
}

/** A few birds at an edge turn on their own and the turn spreads (Attanasi et al. 2015). */
void FStarlingFlock::MaybeImpulse(float Dt)
{
	for (int32 K = Impulses.Num() - 1; K >= 0; K--)
	{
		Impulses[K].T -= Dt;
		if (Impulses[K].T <= 0.f || Impulses[K].SeedBird >= Count) Impulses.RemoveAt(K);
	}
	if (Impulses.Num() >= 4 || NFlock < 30 || Rnd() >= (P.SpontaneousTurns / 60.f) * Dt) return;
	const int32 J = Tip;
	if (J < 0 || J >= Count) return;
	// turn 45-120 degrees left or right of the bird's heading, often with a climb or dive
	const float Sp = Hyp3(Vx[J], Vy[J], Vz[J]) + 1e-6f;
	const float Yaw = (PI / 4.f + Rnd() * (PI * 5.f) / 12.f) * (Rnd() < 0.5f ? -1.f : 1.f);
	const float HX = Vx[J] / Sp, HZ = Vz[J] / Sp, C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
	const float DY = Rnd() < 0.6f ? (Rnd() * 2.f - 1.f) * 0.45f : 0.f;
	FVector3f D(HX * C - HZ * S, DY, HX * S + HZ * C);
	D.Normalize();
	Impulses.Add({ J, D, 1.2f + Rnd() * 1.3f, 2.f, 0, FVector3f::ZeroVector });
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
	for (int32 I = 0; I < Count; I += 11) { M += FVector3f(Px[I], Py[I], Pz[I]); N++; }
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
		if (J < 0 || J >= Count) { PredMode = 2; PredT = 0.f; return; }
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
//  - escape manoeuvres are copied from neighbours (Hemelrijk et al. 2015).

void FStarlingFlock::BuildGrid()
{
	FMemory::Memzero(BucketStart.GetData(), BucketStart.Num() * sizeof(int32));
	for (int32 I = 0; I < Count; I++)
	{
		const uint32 H = Hash(FMath::FloorToInt(Px[I] / CELL), FMath::FloorToInt(Py[I] / CELL), FMath::FloorToInt(Pz[I] / CELL));
		CellOf[I] = (int32)H;
		BucketStart[H + 1]++;
	}
	for (int32 H = 0; H < HSIZE; H++) BucketStart[H + 1] += BucketStart[H];
	FMemory::Memcpy(Fill.GetData(), BucketStart.GetData(), HSIZE * sizeof(int32));
	for (int32 I = 0; I < Count; I++)
	{
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

/** Social steering of one bird: Out = ax ay az, escape, ex ey ez. */
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
		int32 EscJ = -1, NSep = 0, NSoc = 0, NVisAll = 0;
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
		}
		const float Inv = 1.f / FMath::Max(1, NSoc);
		Ax += Sx * Ws / NSep; Ay += Sy * Ws / NSep; Az += Sz * Ws / NSep;
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
		// escape rolls spread from neighbour to neighbour (Hemelrijk et al. 2015)
		if (EscJ >= 0 && State[I] == Flying && HashRnd(I, Tick) < P.EscapeContagion * Rdt)
		{
			Out[3] = 1.f;
			Out[4] = Ex[EscJ]; Out[5] = Ey[EscJ]; Out[6] = Ez[EscJ];
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
	FVector3f Centre = FVector3f::ZeroVector;
	int32 MN = 0;
	for (int32 I = 0; I < Count; I += 13) { Centre += FVector3f(Px[I], Py[I], Pz[I]); MN++; }
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
		if (State[I] == Flying && O[3] > 0.f && Refractory[I] <= 0.f && EscDelay[I] <= 0.f)
		{
			// copy the neighbour's roll after a short delay
			EscDelay[I] = 0.01f + Rnd() * 0.05f;
			NextEx[I] = O[4]; NextEy[I] = O[5]; NextEz[I] = O[6];
		}
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
	const float Gather = FMath::Clamp(P.Gather, 0.f, 1.f);

	Tick++;
	SimTime += Dt;
	StepPredator(Dt);
	StepGroup(Dt);
	StepFlow(Dt);
	MaybeImpulse(Dt);
	if (Count == 0) return;
	Social(Dt);

	const float FlowAmp = FMath::Clamp(P.Flow, 0.f, 2.f);
	// a gathered murmuration holds together better: it shears, stretches and layers less
	const float FlowTurn = FlowAmp * (0.8f - 0.25f * Gather);
	const float FlowSpd = 0.15f * FlowAmp * (1.f - 0.3f * Gather);
	const float FlowAltK = FlowAmp * (1.f - 0.5f * Gather);
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

	// the informed patch, the way the flock is asked to turn, its cohesion radius
	FVector3f LeadPos(1e9f);
	float LeadR2 = -1.f;
	if (Lead >= 0 && Lead < Count)
	{
		LeadPos = FVector3f(Px[Lead], Py[Lead], Pz[Lead]);
		LeadR2 = P.InformedFraction > 0.f ? LeadR * LeadR : -1.f;
	}
	float GDx, GDz;
	{
		// the flock's heading, rotated toward the goal by at most 60 degrees, so a turn is led
		// progressively instead of an about-face
		const float DX = Goal.X - GC.X, DZ = Goal.Z - GC.Z, DL = Hyp2(DX, DZ) + 1e-6f;
		const float HL = GV.Length();
		if (HL < 0.5f) { GDx = DX / DL; GDz = DZ / DL; }
		else
		{
			const float HX = GV.X / HL, HZ = GV.Y / HL, UX = DX / DL, UZ = DZ / DL;
			const float A = FMath::Clamp(FMath::Atan2(HX * UZ - HZ * UX, HX * UX + HZ * UZ), -MAX_TURN, MAX_TURN);
			const float C = FMath::Cos(A), S = FMath::Sin(A);
			GDx = HX * C - HZ * S; GDz = HX * S + HZ * C;
		}
	}
	const float GPull = 10.f + 0.3f * FMath::Sqrt((float)FMath::Max(1, NFlock));
	// formation shaping: pull strength, vertical weight of the pull, altitude spread, radius
	float GPk = 1.f, GPv = 0.5f, GAltM = 1.f, GPr = 1.f;
	switch (Form)
	{
	case EForm::Stream: GPk = 0.5f; GPv = 0.5f; GAltM = 0.6f; GPr = 4.f; break;  // free to string out
	case EForm::Ball: GPk = 2.2f; GPv = 1.f; GAltM = 3.f; GPr = 0.55f; break;    // round, deep
	case EForm::Sheet: GPk = 1.2f; GPv = 0.f; GAltM = 0.25f; GPr = 1.1f; break;  // wide, thin
	default: break;
	}
	const float PullK = 0.6f + 0.6f * Gather; // m/s2 per radius outside the flock's core
	const float CoreR2 = FMath::Square(1.5f * GPull * GPr);

	FVector3f SumP = FVector3f::ZeroVector;
	FVector2f SumV = FVector2f::ZeroVector;
	float EdgeS = -1e30f, TipS = -1e30f;
	int32 NewEdge = -1, NewTip = -1, In = 0;

	for (int32 I = 0; I < Count; I++)
	{
		NextEscape[I] = 0;
		const uint8 St = State[I];
		const float Sp = Hyp3(Vx[I], Vy[I], Vz[I]) + 1e-3f;
		const float Fx = Vx[I] / Sp, Fy = Vy[I] / Sp, Fz = Vz[I] / Sp;
		float Ax = Sfx[I], Ay = Sfy[I], Az = Sfz[I];

		// flock bookkeeping (for next step)
		SumP += FVector3f(Px[I], Py[I], Pz[I]);
		SumV += FVector2f(Vx[I], Vz[I]);
		{
			// leaders and turn initiators come from the body of the flock, not from stragglers
			// (a straggler skimming the sea would lead the whole flock down to it)
			const float OX = Px[I] - GC.X, OZ = Pz[I] - GC.Z;
			const float TipD = OX * OX + OZ * OZ;
			if (TipD < CoreR2 && FMath::Abs(Py[I] - GC.Y) < 20.f)
			{
				const float E = OX * GDx + OZ * GDz;
				if (E > EdgeS) { EdgeS = E; NewEdge = I; }
				if (TipD > TipS) { TipS = TipD; NewTip = I; }
			}
		}

		// area attraction: a turning force (lateral only, StarDisplay Eq. 11), stronger when the
		// bird heads away; outside the area it pulls firmly back in
		{
			float DX = 0.f, DZ = 0.f;
			if (Px[I] > Area.X) DX = Area.X - Px[I]; else if (Px[I] < -Area.X) DX = -Area.X - Px[I];
			if (Pz[I] > Area.Y) DZ = Area.Y - Pz[I]; else if (Pz[I] < -Area.Y) DZ = -Area.Y - Pz[I];
			const float D = Hyp2(DX, DZ);
			if (D > 0.f)
			{
				const float Away = -(DX * Fx + DZ * Fz) / D; // +1 when flying straight out
				const float S = (P.AreaPull / MASS) * 2.f * (0.5f + 0.5f * Away) * (1.f + D / 40.f);
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
			FlowA = FlA[I] * FlowAltK * FMath::Clamp((Band - 4.f) * 0.6f, 2.f, 20.f); // smaller swings near the sea
			// veer left or right with the region: shear that stretches and folds a flock
			const float TL = Hyp2(Fx, Fz);
			if (TL > 0.2f) { const float T = FlT[I] * FlowTurn / TL; Ax += Fz * T; Az -= Fx * T; }
		}

		// where the flock is going: informed birds steer for the goal (3D), everyone else feels
		// it weakly and only horizontally
		float AltWant = Band;
		{
			const float Gx = Goal.X - Px[I], Gy = Goal.Y - Py[I], Gz = Goal.Z - Pz[I];
			const float GH = Hyp2(Gx, Gz) + 1e-6f;
			const float LDx = Px[I] - LeadPos.X, LDy = Py[I] - LeadPos.Y, LDz = Pz[I] - LeadPos.Z;
			if (LDx * LDx + LDy * LDy + LDz * LDz < LeadR2)
			{
				In++;
				// the informed birds lead the climb or dive, a little ahead of the flock
				AltWant = Band + FMath::Clamp(Goal.Y - Band, -8.f, 8.f);
				// climb or dive at up to ~14 degrees
				const float Gyn = FMath::Clamp(Gy / FMath::Max(GH, 20.f), -0.25f, 0.25f);
				const float GL = Hyp2(1.f, Gyn);
				Ax += (GDx / GL - Fx) * LeadA; Ay += (Gyn / GL - Fy) * LeadA; Az += (GDz / GL - Fz) * LeadA;
			}
			else if (Form == EForm::Stream && Head > TrailStart)
			{
				// pour along the leaders' trail: aim at the next trail point not yet passed
				const int32 First = FMath::Max(TrailStart, Head - TRAIL);
				int32 Q = TrailSeq[I];
				if (Q < First || Q >= Head) Q = First;
				for (int32 K = 0; K < 4 && Q < Head - 1; K++)
				{
					const FVector3f& A = Trail[Q % TRAIL];
					const FVector3f& B = Trail[(Q + 1) % TRAIL];
					const FVector3f T(A.X - Px[I], A.Y - Py[I], A.Z - Pz[I]);
					// passed it (it lies behind along the path), or close: on to the next
					if (FVector3f::DotProduct(T, B - A) < 0.f || T.SizeSquared() < 25.f) Q++;
					else break;
				}
				TrailSeq[I] = Q;
				const FVector3f& A = Trail[Q % TRAIL];
				// each bird keeps its own place across the tube, so the stream is a thick ribbon
				const float Tube = 2.f + 0.1f * FMath::Sqrt((float)NFlock);
				const float TX = A.X + Tube * FMath::Clamp(SpeedN[I] * 0.6f, -1.5f, 1.5f) - Px[I];
				const float TY = A.Y + Tube * 0.4f * FMath::Clamp(RollN[I] * 0.6f, -1.5f, 1.5f) - Py[I];
				const float TZ = A.Z + Tube * FMath::Clamp(Mood[I] * 0.6f, -1.5f, 1.5f) - Pz[I];
				const float TL = Hyp3(TX, TY, TZ) + 1e-6f;
				if (Q < Head - 1 || TL > 8.f)
				{
					Ax += (TX / TL - Fx) * StreamA; Ay += (TY / TL - Fy) * StreamA; Az += (TZ / TL - Fz) * StreamA;
					AltWant = A.Y;
				}
			}
			else
			{
				const float HL = Hyp2(Fx, Fz) + 1e-6f;
				Ax += (GDx - Fx / HL) * FollowA; Az += (GDz - Fz / HL) * FollowA;
			}
			// stay with the flock: a pull toward its centre from outside its radius
			const float CX = GC.X - Px[I], CY = (GC.Y - Py[I]) * GPv, CZ = GC.Z - Pz[I];
			const float CD = Hyp3(CX, CY, CZ);
			const float Over = CD / (GPull * GPr) - 0.6f;
			if (Over > 0.f && GPk > 0.f)
			{
				const float S = PullK * GPk * FMath::Min(Over, 4.f) / CD;
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
		const float Alt = FMath::Clamp(AltWant + AltN[I] * P.AltitudeSpread * GAltM * (AltWant < 15.f ? 0.3f : 1.f) + FlowA, 2.5f, 175.f);
		Ay += FMath::Clamp((P.AltitudePull / MASS) * (Alt - Py[I]), -2.f, 2.f) - 0.3f * Vy[I];
		// over land: well above the hill and people's heads. This floor is firmer than the
		// altitude spring, so the flock's own pull can't hold birds low there
		float FloorUrg = 0.f;
		const float Ground0 = GroundAt(Px[I], Pz[I]);
		if (Ground0 > 0.5f)
		{
			const float Floor = Ground0 + P.LandClearance;
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
				float OX = DX - Along * PredVel.X / PL, OY = DY - Along * PredVel.Y / PL, OZ = DZ - Along * PredVel.Z / PL;
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
			const float Look = 1.f;
			const float QY = Py[I] + Vy[I] * Look;
			const float G1 = GroundAt(Px[I] + Vx[I] * Look, Pz[I] + Vz[I] * Look);
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
		float LX = PerpX - G * Fy * Fx, LY = PerpY + G - G * Fy * Fy, LZ = PerpZ - G * Fy * Fz;
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
		float Target = FMath::Atan2(-(LX * RX + LZ * RZ), FMath::Max(LX * U0x + LY * U0y + LZ * U0z, 0.25f * G));
		// near the surface the wings come level so the lift points up
		const float Lim = MaxBank * EscapeRoll * (1.f - 0.75f * Urg);
		Target = FMath::Clamp(Target, -Lim, Lim);
		const float RollI = RollK * FMath::Clamp(1.f + P.RollSpread * RollN[I], 0.4f, 1.8f) * (1.f + Urg);
		Bank[I] += (Target - Bank[I]) * FMath::Min(1.f, RollI * EscapeRoll * 1.5f);
		const float CB = FMath::Cos(Bank[I]), SB = FMath::Sin(Bank[I]);
		// banked up axis: y = -right * sin + up0 * cos
		const float YX = -RX * SB + U0x * CB, YY = U0y * CB, YZ = -RZ * SB + U0z * CB;
		// lift: along the banked up axis, limited by speed (L ~ v2); a bird about to hit the
		// water pulls harder (a brief high-g pull-up)
		const float LNeed = LX * YX + LY * YY + LZ * YZ;
		const float LMax = G * (LoadMax * EscapeRoll + 2.f * Urg) * (Sp / V0) * (Sp / V0);
		const float L = FMath::Clamp(LNeed, 0.f, LMax);
		LX = L * YX; LY = L * YY; LZ = L * YZ;
		// speed: own cruise speed, mood and the flow field; faster while escaping; thrust minus
		// drag relaxes to it; gravity (added below) speeds up dives
		const float Own = V0 * FMath::Clamp(1.f + P.SpeedSpread * SpeedN[I] + P.MoodSpeed * Mood[I] + FlowS, 0.6f, 1.5f);
		const float Vt = St == Escaping ? FMath::Max(Own, V0) * 1.3f : Own;
		const float Af = (Vt - Sp) / FMath::Max(0.05f, P.SpeedTau) + FMath::Clamp(Fpar, -4.f, 4.f);
		float NVx = Vx[I] + (Af * Fx + LX) * Dt;
		float NVy = Vy[I] + (Af * Fy + LY - G) * Dt;
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
			// skimming: never sink below the skim height while heading down; never below the surface
			const float GH = GroundAt(Px[I], Pz[I]);
			if (Py[I] < GH + Skim && Vy[I] < 0.f) Vy[I] *= 0.5f;
			if (Py[I] < GH + MIN_CLEAR)
			{
				Py[I] = GH + MIN_CLEAR;
				if (Vy[I] < 0.f)
				{
					// keep the speed, but level
					const float S = Hyp3(Vx[I], Vy[I], Vz[I]), Hh = Hyp2(Vx[I], Vz[I]) + 1e-3f;
					Vy[I] = 0.f; Vx[I] *= S / Hh; Vz[I] *= S / Hh;
				}
			}
		}

		// wings: flap harder when slow or climbing, glide in bouts otherwise
		const float Effort = FMath::Max(0.f, (V0 - Sp) * 0.25f + Fy * 2.f + (St == Escaping ? 1.f : 0.f));
		const float Bout = 0.5f + 0.5f * FMath::Sin(FlapPhase[I] * 0.06f + I);
		const float WantAmp = FMath::Min(1.f, 0.15f + Effort * 0.5f + (Bout > 0.55f ? 0.45f : 0.f));
		FlapAmp[I] += (WantAmp - FlapAmp[I]) * FMath::Min(1.f, Dt * 6.f);
		FlapPhase[I] = FMath::Fmod(FlapPhase[I] + FlapFreq[I] * 2.f * PI * Dt, 2000.f * PI);
	}

	// the flock: centroid and heading for the next step; the informed patch holds its share
	NFlock = Count;
	GC = SumP / (float)Count;
	GV = SumV / (float)Count;
	if (NewEdge >= 0) Edge = NewEdge;
	if (NewTip >= 0) Tip = NewTip;
	LeadR = AdaptRadius(LeadR, In, P.InformedFraction * Count * (Form == EForm::Stream ? 0.4f : 1.f));
	// a turn starts with a few birds: ~3% of the flock
	for (FImpulse& Im : Impulses) Im.R = AdaptRadius(Im.R, Im.N, FMath::Max(8.f, 0.03f * Count));
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
}
