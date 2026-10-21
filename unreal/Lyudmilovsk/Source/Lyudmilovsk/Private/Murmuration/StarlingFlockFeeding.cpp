// The person and hand-feeding (Murmuration's lure, viewer and grounded birds).
//
// The person holds out food: flying birds within NoticeRange of the hand come down in small
// staggered groups onto free spots on the ground around them, stand, hop, peck and look at the
// hand, and leave after a while in small groups. A person who walks at them flushes them
// (flight-initiation distance, Murmuration's docs §1.20); their neighbours follow. Birds farther
// than NoticeRange don't see the food at all.

#include "Murmuration/StarlingFlock.h"
#include "Murmuration/StarlingFlockMath.h"

using namespace StarlingFlockMath;

void FStarlingFlock::SetViewer(const FVector3f& Feet, const FVector3f& Velocity)
{
	ViewerPos = Feet;
	ViewerVel = FVector3f(Velocity.X, 0.f, Velocity.Z);
	ViewerAt = SimTime;
}

void FStarlingFlock::FreeSpot(int32 I)
{
	const int32 S = SpotOf[I];
	if (S >= 0 && S < Lure.Taken.Num() && Lure.Taken[S] == I) Lure.Taken[S] = -1;
	SpotOf[I] = -1;
}

/** A free spot, near (x, z), a little scattered. */
int32 FStarlingFlock::PickSpot(float X, float Z, float Within)
{
	int32 Best = -1;
	float BD = 1e30f;
	for (int32 S = 0; S < Lure.Taken.Num(); S++)
	{
		if (Lure.Taken[S] >= 0) continue;
		const float D = Hyp2(Lure.Spots[S].X - X, Lure.Spots[S].Z - Z);
		if (D > Within) continue;
		const float Sc = D + Rnd() * 1.2f;
		if (Sc < BD) { BD = Sc; Best = S; }
	}
	return Best;
}

/** A free spot at least MinD from the person and within Within of the bird. */
int32 FStarlingFlock::FarSpot(int32 I, float MinD, float Within)
{
	int32 Best = -1;
	float BS = 1e30f;
	for (int32 S = 0; S < Lure.Taken.Num(); S++)
	{
		if (Lure.Taken[S] >= 0) continue;
		const FVector3f& Sp = Lure.Spots[S];
		if (Hyp2(Sp.X - ViewerPos.X, Sp.Z - ViewerPos.Z) < MinD) continue;
		const float D = Hyp2(Sp.X - Px[I], Sp.Z - Pz[I]);
		if (D > Within) continue;
		const float Sc = D + Rnd() * 2.f;
		if (Sc < BS) { BS = Sc; Best = S; }
	}
	return Best;
}

int32 FStarlingFlock::Visitors() const
{
	int32 V = 0;
	for (int32 T : Lure.Taken) V += T >= 0;
	return V + Lure.Queue.Num();
}

void FStarlingFlock::SetLure(bool bOn, const FVector3f& Hand, TArray<FVector3f>&& Spots)
{
	const bool bWas = Lure.bOn;
	Lure.Hand = Hand;
	if (Spots.Num())
	{
		// new spots: birds on the ground keep a spot near where they stand
		Lure.Spots = MoveTemp(Spots);
		Lure.Taken.Init(-1, Lure.Spots.Num());
		for (int32 I = 0; I < Count; I++)
		{
			if (SpotOf[I] < 0) continue;
			SpotOf[I] = -1;
			if (State[I] == Grounded || State[I] == Visiting)
			{
				const int32 S = PickSpot(Px[I], Pz[I], State[I] == Grounded ? 1.5f : 1e9f);
				if (S >= 0) { SpotOf[I] = S; Lure.Taken[S] = I; }
				else TakeOff(I, Grp[I], Lure.Hand.X, Lure.Hand.Z);
			}
		}
	}
	Lure.bOn = bOn;
	if (bOn)
	{
		if (!bWas || Lure.T > 60.f) { Lure.T = 0.f; Recruit(); }
	}
	else
	{
		Lure.Queue.Reset();
		// the birds on the ground linger a while (10-60 s each), then leave in small groups;
		// those still flying in land first
		for (int32 S = 0; S < Lure.Taken.Num(); S++)
		{
			const int32 I = Lure.Taken[S];
			if (I >= 0 && (State[I] == Grounded || State[I] == Visiting)) Dwell[I] = FMath::Min(Dwell[I], 10.f + Rnd() * 50.f);
		}
	}
}

/** The nearest flying birds that can see the hand come down, in small staggered groups. */
void FStarlingFlock::Recruit()
{
	const int32 Room = FMath::Min(FMath::Max(0, P.LureMax), Lure.Taken.Num()) - Visitors();
	if (Room <= 0) return;
	const float Range = FMath::Max(1.f, P.NoticeRange);
	struct FNear { int32 I; float D; };
	TArray<FNear> Near;
	for (int32 I = 0; I < Count; I++)
	{
		if (State[I] != Flying || SimTime - LeftBeach[I] <= 90.f) continue;
		const EMode M = Groups[Grp[I]].Mode;
		if (M == EMode::Out || M == EMode::In) continue;
		const float D = FVector3f::Dist(FVector3f(Px[I], Py[I], Pz[I]), Lure.Hand);
		if (D < Range) Near.Add({ I, D });
	}
	if (Near.Num() > Room) Near.Sort([](const FNear& A, const FNear& B) { return A.D < B.D; });
	for (int32 K = 0; K < FMath::Min(Room, Near.Num()); K++) Lure.Queue.Add(Near[K].I);
	Lure.QT = 0.f;
}

void FStarlingFlock::StepLure(float Dt)
{
	// spots of birds that have gone are free again
	for (int32 S = 0; S < Lure.Taken.Num(); S++)
	{
		const int32 J = Lure.Taken[S];
		if (J >= 0 && (J >= Count || SpotOf[J] != S)) Lure.Taken[S] = -1;
	}
	if (Tick % 10 == 0)
	{
		GroundList.Reset();
		for (int32 I = 0; I < Count; I++) if (State[I] == Grounded || State[I] == Visiting || State[I] == Perched) GroundList.Add(I);
	}
	StepViewer(Dt);
	if (!Lure.bOn) return;
	Lure.T += Dt;
	// look again for birds now and then while there is room: a flock passing close sees the hand
	if (Lure.Queue.Num() == 0 && Tick % 120 == 0) Recruit();
	Lure.QT -= Dt;
	if (Lure.QT > 0.f || Lure.Queue.Num() == 0) return;
	// the next small group sets off
	const int32 N = FMath::Min(Lure.Queue.Num(), 3 + (int32)(Rnd() * 6.f));
	const float Stay = 20.f + Rnd() * 30.f;
	for (int32 K = 0; K < N; K++)
	{
		const int32 I = Lure.Queue[0];
		Lure.Queue.RemoveAt(0);
		if (I >= Count || State[I] != Flying) continue;
		const int32 S = PickSpot(Lure.Hand.X, Lure.Hand.Z);
		if (S < 0) { Lure.Queue.Reset(); break; }
		SpotOf[I] = S; Lure.Taken[S] = I;
		State[I] = Visiting;
		FinalApp[I] = 0;
		Dwell[I] = Stay + Rnd() * 6.f; // planned stay on the ground, from landing
	}
	Lure.QT = 0.4f + Rnd() * 0.9f;
}

/**
 * Flight-initiation distance of a bird on the ground: ~3 m from a person standing still, up to
 * ~8 m from one walking straight at it; much closer to a still person offering food. Movement
 * counts more for a bird facing the person and near the food, and a bird disturbed before is
 * warier.
 */
float FStarlingFlock::FidOf(int32 I, float D, float DXV, float DZV) const
{
	const float VMag = Hyp2(ViewerVel.X, ViewerVel.Z);
	const float Approach = FMath::Max(0.f, (ViewerVel.X * DXV + ViewerVel.Z * DZV) / (D + 1e-6f)); // m/s toward the bird
	float Fid = Lure.bOn && VMag < 0.4f ? 0.7f : 3.f;
	Fid += 4.f * FMath::Clamp(Approach / 1.4f, 0.f, 1.f);
	// facing the person, near the food: less tolerant of movement
	const float FL = Hyp2(Vx[I], Vz[I]) + 1e-6f;
	const float Facing = FMath::Max(0.f, -(Vx[I] * DXV + Vz[I] * DZV) / (FL * (D + 1e-6f)));
	const float NearFood = Lure.bOn ? FMath::Clamp(1.f - Hyp2(Px[I] - Lure.Hand.X, Pz[I] - Lure.Hand.Z) / 3.f, 0.f, 1.f) : 0.f;
	Fid += 2.f * FMath::Clamp(VMag, 0.f, 1.5f) * (1.f + 0.6f * Facing) * (1.f + 0.5f * NearFood) * (Lure.bOn ? 0.6f : 1.f);
	const float Dist = Disturb[I] * FMath::Exp(-(SimTime - DisturbAt[I]) / 60.f);
	return Fid * (1.f + 0.25f * FMath::Min(Dist, 4.f));
}

void FStarlingFlock::StepViewer(float Dt)
{
	RecentFlush *= FMath::Exp(-Dt / 3.f);
	if (SimTime - ViewerAt > 1.5f || !GroundList.Num()) return;
	TArray<int32> Flushes;
	int32 OnGround = 0;
	for (int32 I : GroundList)
	{
		const uint8 St = State[I];
		if (St != Grounded && St != Visiting && St != Perched) continue;
		const float DXV = Px[I] - ViewerPos.X, DZV = Pz[I] - ViewerPos.Z;
		const float D = Hyp3(DXV, (Py[I] - ViewerPos.Y) * 0.5f, DZV);
		if (D > 30.f) continue;
		if (St == Visiting)
		{
			// coming in: a spot too close to a moving person is given up
			const int32 S = SpotOf[I];
			if (S >= 0 && S < Lure.Spots.Num() && D < 25.f &&
				Hyp2(Lure.Spots[S].X - ViewerPos.X, Lure.Spots[S].Z - ViewerPos.Z) < 0.6f * FidOf(I, D, DXV, DZV))
			{
				const int32 NS = FarSpot(I, FidOf(I, D, DXV, DZV) + 1.5f, 12.f);
				if (NS >= 0) { FreeSpot(I); SpotOf[I] = NS; Lure.Taken[NS] = I; }
				else TakeOff(I, Grp[I], ViewerPos.X, ViewerPos.Z);
			}
			continue;
		}
		OnGround++;
		if (AlarmT[I] > 0.f)
		{
			AlarmT[I] -= Dt;
			if (AlarmT[I] <= 0.f) { Flushes.Add(I); continue; }
		}
		const float Fid = FidOf(I, D, DXV, DZV);
		if (D < Fid && Rnd() < (3.f + 10.f * (1.f - D / Fid)) * Dt) Flushes.Add(I);
	}
	if (!Flushes.Num()) return;
	const float Alarm = FMath::Clamp(RecentFlush / FMath::Max(5, OnGround), 0.f, 1.f);
	for (int32 I : Flushes)
	{
		if (State[I] != Grounded && State[I] != Perched) continue;
		RecentFlush += 1.f;
		Disturb[I] = Disturb[I] * FMath::Exp(-(SimTime - DisturbAt[I]) / 60.f) + 1.f;
		DisturbAt[I] = SimTime;
		AlarmT[I] = 0.f;
		// the neighbours are alarmed: they follow within a fraction of a second
		const float PC = 0.55f + 0.4f * Alarm;
		for (int32 J : GroundList)
		{
			if (J == I || (State[J] != Grounded && State[J] != Perched) || AlarmT[J] > 0.f) continue;
			if (Hyp2(Px[J] - Px[I], Pz[J] - Pz[I]) < 3.f && Rnd() < PC) AlarmT[J] = 0.08f + Rnd() * 0.3f;
		}
		const float DXV = Px[I] - ViewerPos.X, DZV = Pz[I] - ViewerPos.Z, D = Hyp2(DXV, DZV);
		if (State[I] == Perched)
		{
			// up from the roost; it circles with the nearest flock and comes down again later
			const FVector3f At(Px[I], Py[I], Pz[I]);
			Launch(I, NearestGroup(At));
			LeftBeach[I] = SimTime;
			const float L = D + 1e-6f;
			Vx[I] = DXV / L * 6.f; Vz[I] = DZV / L * 6.f;
			continue;
		}
		// relocate a few metres further away, or leave for the flock: more likely to leave when
		// disturbed again and again, or when the whole group goes up
		const float PLeave = FMath::Clamp(0.2f + 0.25f * (Disturb[I] - 1.f) + 0.5f * Alarm, 0.f, 1.f);
		const int32 NS = Rnd() < PLeave ? -1 : FarSpot(I, FidOf(I, D, DXV, DZV) + 2.f, 10.f);
		if (NS < 0) { TakeOff(I, Grp[I], ViewerPos.X, ViewerPos.Z); continue; }
		FreeSpot(I);
		SpotOf[I] = NS; Lure.Taken[NS] = I;
		State[I] = Visiting;
		FinalApp[I] = 1;
		Relocated[I] = 1;
		Dwell[I] = FMath::Max(Dwell[I], 8.f + Rnd() * 20.f);
		// a short low flight, away first
		const float L = D + 1e-6f;
		Vx[I] = DXV / L * 3.f; Vy[I] = 2.5f; Vz[I] = DZV / L * 3.f;
		Py[I] += 0.1f;
	}
}

/** Leave the ground: up and away (from the hand, or from what scared it), back to a flock. */
void FStarlingFlock::TakeOff(int32 I, int32 G, float FromX, float FromZ)
{
	if (SpotOf[I] >= 0) FreeSpot(I);
	LeftBeach[I] = SimTime;
	FVector2f A(Px[I] - FromX, Pz[I] - FromZ);
	A = A.GetSafeNormal();
	// mostly out to sea
	const FVector2f O = (A * 0.5f - Inland * 0.85f).GetSafeNormal();
	State[I] = Flying;
	Vx[I] = O.X * 5.f; Vy[I] = 3.5f; Vz[I] = O.Y * 5.f;
	Py[I] += 0.1f;
	Fold[I] = 0.3f; FlapAmp[I] = 1.f; Bank[I] = 0.f;
	Refractory[I] = 0.f; Timer[I] = 0.f;
	if (G < 0 || G >= GMAX || Groups[G].Mode == EMode::Free) G = LargestGroup(EMode::Local, EMode::Shore);
	if (G < 0) G = AllocGroup(Act == EActivity::Foraging ? EMode::Shore : EMode::Local, FVector3f(Px[I], Py[I] + 10.f, Pz[I]) - FVector3f(Inland.X, 0.f, Inland.Y) * 30.f);
	Grp[I] = (uint8)G;
}

/** On the ground: hop, peck, look at the hand; leave when the time is up. */
void FStarlingFlock::StepGrounded(int32 I, float Dt)
{
	// on its feet: wings folded, still (a brief flick only during a hop)
	Fold[I] = 1.f;
	FlapAmp[I] = 0.f;
	Bank[I] = 0.f;
	Dwell[I] -= Dt;
	if (Dwell[I] <= 0.f)
	{
		// neighbours near the end of their stay go with it: small groups leave together
		const float X = Px[I], Z = Pz[I];
		TakeOff(I, Grp[I], Lure.Hand.X, Lure.Hand.Z);
		for (int32 J : GroundList)
		{
			if (State[J] == Grounded && Dwell[J] < 8.f && Hyp2(Px[J] - X, Pz[J] - Z) < 2.5f) Dwell[J] = FMath::Min(Dwell[J], 0.05f + Rnd() * 0.4f);
		}
		return;
	}
	// facing, as a tiny velocity; pecking pitches it nose-down
	float FX0 = Vx[I], FZ0 = Vz[I];
	const float FL = Hyp2(FX0, FZ0) + 1e-9f;
	FX0 /= FL; FZ0 /= FL;
	HopCool[I] -= Dt;
	if (HopT[I] > 0.f)
	{
		// a hop: 0.2 s, a few cm high, a brief small flick of the wings
		HopT[I] = FMath::Max(0.f, HopT[I] - Dt);
		const float U = 1.f - HopT[I] / HOP_T;
		const float K = FMath::Min(1.f, Dt / FMath::Max(Dt, HopT[I] + Dt));
		Px[I] += (HopX[I] - Px[I]) * K; Pz[I] += (HopZ[I] - Pz[I]) * K;
		Py[I] = HopY[I] + 0.06f * 4.f * U * (1.f - U); // 6 cm at the top
		const float Flick = FMath::Sin(U * PI);
		FlapAmp[I] = 0.2f * Flick; Fold[I] = 1.f - 0.35f * Flick;
		FlapPhase[I] += FlapFreq[I] * 2.f * PI * Dt;
		Vx[I] = FX0 * 0.01f; Vy[I] = 0.f; Vz[I] = FZ0 * 0.01f;
		return;
	}
	const int32 S = SpotOf[I];
	const bool bHas = S >= 0 && S < Lure.Spots.Num();
	ActT[I] -= Dt;
	if (ActT[I] <= 0.f)
	{
		ActT[I] = 0.8f + Rnd() * 2.2f;
		const float R = Rnd();
		if (R < 0.4f) PeckT[I] = 0.25f + (int32)(Rnd() * 3.f) * 0.25f; // a bout of 1-3 pecks
		else if (R < 0.65f || HopCool[I] > 0.f)
		{
			// turn: look at the hand (head cocked a little), or look about
			const float A = Lure.bOn && Rnd() < 0.7f ? FMath::Atan2(Lure.Hand.Z - Pz[I], Lure.Hand.X - Px[I]) + (Rnd() - 0.5f) * 1.2f
				: FMath::Atan2(FZ0, FX0) + (Rnd() - 0.5f) * 2.f;
			FX0 = FMath::Cos(A); FZ0 = FMath::Sin(A);
		}
		else
		{
			// a hop of 5-40 cm, back toward its spot if it has wandered off
			const float SpotX = bHas ? Lure.Spots[S].X : Px[I], SpotZ = bHas ? Lure.Spots[S].Z : Pz[I];
			const float Off = Hyp2(SpotX - Px[I], SpotZ - Pz[I]);
			const float A = Off > 0.5f ? FMath::Atan2(SpotZ - Pz[I], SpotX - Px[I]) + (Rnd() - 0.5f) * 0.6f : Rnd() * 2.f * PI;
			const float D = 0.05f + Rnd() * 0.35f;
			FX0 = FMath::Cos(A); FZ0 = FMath::Sin(A);
			HopX[I] = Px[I] + FX0 * D; HopZ[I] = Pz[I] + FZ0 * D;
			HopY[I] = bHas ? Lure.Spots[S].Y : Py[I];
			HopT[I] = HOP_T;
			HopCool[I] = 2.f + Rnd() * 4.f;
		}
	}
	float Pitch = 0.f;
	if (PeckT[I] > 0.f)
	{
		PeckT[I] = FMath::Max(0.f, PeckT[I] - Dt);
		Pitch = 0.75f * FMath::Max(0.f, FMath::Sin(PeckT[I] / 0.25f * PI)); // up to ~43 degrees nose-down
	}
	const float C = FMath::Cos(Pitch);
	Vx[I] = FX0 * C * 0.01f; Vy[I] = -FMath::Sin(Pitch) * 0.01f; Vz[I] = FZ0 * C * 0.01f;
}

/** Steering to a point with a simple arrival law (descents to the roost and visitors). */
void FStarlingFlock::SeekStep(int32 I, const FVector3f& T, float DL, float Dt, float VMax, float Social, bool bHoldHeight)
{
	const float DX = T.X - Px[I], DY = T.Y - Py[I], DZ = T.Z - Pz[I];
	const float HD = Hyp2(DX, DZ);
	const float Want = FMath::Min(VMax, 1.2f + DL * 0.6f);
	const float UX = DX / DL, UZ = DZ / DL;
	float UY = DY / DL;
	// keep some height until close horizontally
	if (bHoldHeight && HD > 25.f && DY < 0.f) UY *= 0.4f;
	constexpr float K2 = 3.f;
	float Ax = Sfx[I] * Social + (UX * Want - Vx[I]) * K2;
	float Ay = Sfy[I] * Social + (UY * Want - Vy[I]) * K2;
	float Az = Sfz[I] * Social + (UZ * Want - Vz[I]) * K2;
	const float AL = Hyp3(Ax, Ay, Az);
	if (AL > 25.f) { Ax *= 25.f / AL; Ay *= 25.f / AL; Az *= 25.f / AL; }
	Vx[I] += Ax * Dt; Vy[I] += Ay * Dt; Vz[I] += Az * Dt;
	Px[I] += Vx[I] * Dt; Py[I] += Vy[I] * Dt; Pz[I] += Vz[I] * Dt;
}

/** Last resort: never below the surface (unless landing on it). */
void FStarlingFlock::FloorClamp(int32 I, float Min)
{
	const float GH = GroundAt(Px[I], Pz[I]) + Min;
	if (Py[I] < GH)
	{
		Py[I] = GH;
		if (Vy[I] < 0.f)
		{
			// keep the speed, but level
			const float S = Hyp3(Vx[I], Vy[I], Vz[I]), H = Hyp2(Vx[I], Vz[I]) + 1e-3f;
			Vy[I] = 0.f; Vx[I] *= S / H; Vz[I] *= S / H;
		}
	}
}
