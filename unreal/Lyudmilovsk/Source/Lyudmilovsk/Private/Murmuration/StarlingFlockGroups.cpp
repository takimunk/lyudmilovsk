// Flocks: where each one goes, the shape it takes, merges and splits, spontaneous turns.

#include "Murmuration/StarlingFlock.h"
#include "Murmuration/StarlingFlockMath.h"

using namespace StarlingFlockMath;

int32 FStarlingFlock::AllocGroup(EMode Mode, const FVector3f& C)
{
	int32 G = -1;
	for (int32 K = 0; K < GMAX; K++) if (Groups[K].Mode == EMode::Free) { G = K; break; }
	if (G < 0)
	{
		// full: reuse the largest group of the same mode
		int32 Best = 0;
		for (int32 K = 0; K < GMAX; K++) if (Groups[K].Mode == Mode && Groups[K].N > Best) { Best = Groups[K].N; G = K; }
		if (G >= 0) return G;
		G = 0;
	}
	FGroup& Gr = Groups[G];
	Gr.Mode = Mode;
	Gr.N = 0;
	Gr.C = C;
	Gr.V = FVector2f::ZeroVector;
	Gr.Band = C.Y;
	Gr.Lead = -1; Gr.R = 3.f; Gr.Edge = -1; Gr.Tip = -1;
	Gr.GoalT = 0.f;
	Gr.Born = SimTime;
	ChooseGoal(G);
	return G;
}

/** The nearest live flock to a point (a new one if there is none). */
int32 FStarlingFlock::NearestGroup(const FVector3f& At)
{
	int32 G = -1;
	float BD = 1e30f;
	for (int32 K = 0; K < GMAX; K++)
	{
		if (Groups[K].Mode == EMode::Free || Groups[K].Mode == EMode::Out) continue;
		const float D = FVector3f::Dist(Groups[K].C, At);
		if (D < BD) { BD = D; G = K; }
	}
	return G >= 0 ? G : AllocGroup(Act == EActivity::Foraging ? EMode::Shore : EMode::Local, At);
}

int32 FStarlingFlock::LargestGroup(EMode A, EMode B) const
{
	int32 G = -1, Best = -1;
	for (int32 K = 0; K < GMAX; K++) if ((Groups[K].Mode == A || Groups[K].Mode == B) && Groups[K].N > Best) { Best = Groups[K].N; G = K; }
	return G;
}

/** A point far along the coast on one side (Side = +-1), beyond the area. */
FVector2f FStarlingFlock::CoastPoint(float Side)
{
	const FVector2f Along(-Inland.Y, Inland.X);
	return Along * Side * (FMath::Max(Area.X, Area.Y) + 400.f + Rnd() * 100.f);
}

void FStarlingFlock::ChooseGoal(int32 G)
{
	FGroup& Gr = Groups[G];
	const float Far = FMath::Max(Area.X, Area.Y);
	if (Gr.Mode == EMode::Out)
	{
		// inland, low over the land, out of sight
		const FVector2f Side(-Inland.Y, Inland.X);
		const FVector2f T = Inland * (Far + 500.f) + Side * (Rnd() * 2.f - 1.f) * 200.f + FVector2f(Gr.C.X, Gr.C.Z) * 0.3f;
		Gr.Goal = FVector3f(T.X, GroundTop + 12.f + Rnd() * 10.f, T.Y);
		Gr.GoalT = 1e9f;
	}
	else if (Gr.Mode == EMode::In)
	{
		// to the gathering area over the sea
		Gr.Goal = FVector3f((Rnd() * 2.f - 1.f) * Area.X * 0.6f, 35.f + Rnd() * 35.f, (Rnd() * 2.f - 1.f) * Area.Y * 0.6f);
		Gr.GoalT = 1e9f;
	}
	else if (Gr.Mode == EMode::Shore)
	{
		// low along the shore; now and then in over the grass along it (where a person may be)
		if (CoastLandPts.Num() && Rnd() < 0.3f)
		{
			const FVector2f& S = CoastLandPts[FMath::Min(CoastLandPts.Num() - 1, (int32)(Rnd() * CoastLandPts.Num()))];
			Gr.Goal = FVector3f(S.X, GroundAt(S.X, S.Y) + 8.f + Rnd() * 6.f, S.Y);
		}
		else if (ShorePts.Num())
		{
			const FVector2f& S = ShorePts[FMath::Min(ShorePts.Num() - 1, (int32)(Rnd() * ShorePts.Num()))];
			Gr.Goal = FVector3f(S.X, 3.f + Rnd() * 10.f, S.Y);
		}
		else
		{
			Gr.Goal = FVector3f((Rnd() * 2.f - 1.f) * Area.X, 3.f + Rnd() * 10.f, (Rnd() * 2.f - 1.f) * Area.Y);
		}
		Gr.GoalT = P.GoalInterval * (0.4f + Rnd() * 0.8f);
	}
	else if ((Phase == EPhase::Roost || Phase == EPhase::Wake) && bHaveRoost)
	{
		// gather over the roost
		const float A = Rnd() * 2.f * PI, R = 25.f + Rnd() * 60.f;
		Gr.Goal.X = Roost.X + FMath::Cos(A) * R;
		Gr.Goal.Z = Roost.Z + FMath::Sin(A) * R * 0.7f;
		Gr.Goal.Y = FMath::Max(Roost.Y, GroundAt(Gr.Goal.X, Gr.Goal.Z)) + (Phase == EPhase::Roost ? 20.f + Rnd() * 25.f : P.Altitude * (0.7f + Rnd() * 0.6f));
		Gr.GoalT = P.GoalInterval * (0.55f + Rnd() * 1.05f);
	}
	else
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
			const float D = Hyp2(X - Gr.C.X, Z - Gr.C.Z);
			const float Score = -FMath::Abs(D - Want) + Rnd() * 60.f;
			if (Score > BScore) { BScore = Score; Best = FVector3f(X, Y, Z); }
		}
		Gr.Goal = Best;
		Gr.GoalT = P.GoalInterval * (0.55f + Rnd() * 1.05f);
	}
	// a new informed patch, on the side of the flock facing the goal (once the edge toward the
	// new goal has been found, two steps on)
	Gr.Lead = -1;
	Gr.LeadWait = 2;
	Gr.R = FMath::Max(Gr.R * 0.5f, 2.f);
	ChooseForm(G);
}

/**
 * How a flock travels to its goal. A distant goal strings it out into a stream behind its
 * leaders; a near one rolls it up into a ball or a flat sheet circling the spot; at dusk it
 * funnels down over the roost; commuters arrive and leave in long lines.
 */
void FStarlingFlock::ChooseForm(int32 G)
{
	const FGroup& Gr = Groups[G];
	const float D = Hyp2(Gr.Goal.X - Gr.C.X, Gr.Goal.Z - Gr.C.Z);
	const float R = Rnd() / FMath::Max(1e-3f, FMath::Clamp(P.Formations, 0.f, 1.f));
	EForm F = EForm::Mass;
	if ((Gr.Mode == EMode::Out || Gr.Mode == EMode::In) && P.Formations > 0.f) F = EForm::Stream;
	else if (Gr.Mode == EMode::Shore) F = R < 0.4f ? EForm::Stream : EForm::Mass;
	else if (Phase == EPhase::Roost && bHaveRoost) F = R < 0.7f ? EForm::Funnel : EForm::Mass;
	else if (D > 0.6f * FMath::Max(Area.X, Area.Y) && R < 0.6f) F = EForm::Stream;
	else if (R < 0.75f) F = Rnd() < 0.5f ? EForm::Ball : EForm::Sheet;
	SetForm(G, F);
}

void FStarlingFlock::SetForm(int32 G, EForm F)
{
	FGroup& Gr = Groups[G];
	Gr.Form = F;
	if (F == EForm::Stream)
	{
		Gr.TrailStart = Gr.Head;
		Gr.TrailT = 0.f;
	}
	else if (F == EForm::Ball || F == EForm::Sheet || F == EForm::Funnel)
	{
		// circle the goal (or the roost) in its own sense of turning
		const bool bFun = F == EForm::Funnel;
		Gr.OrbitC = bFun ? FVector2f(Roost.X, Roost.Z) : FVector2f(Gr.Goal.X, Gr.Goal.Z);
		const float Room = FMath::Max(25.f, FMath::Min(Area.X, Area.Y) * 0.6f);
		Gr.OrbitR = bFun ? 70.f + Rnd() * 40.f : FMath::Min(Room, F == EForm::Ball ? 30.f + Rnd() * 30.f : 50.f + Rnd() * 50.f);
		Gr.OrbitW = Rnd() < 0.5f ? -1.f : 1.f;
		if (bFun) Gr.Goal.Y = Roost.Y + 18.f + Rnd() * 15.f;
	}
}

void FStarlingFlock::StepGroups(float Dt)
{
	for (int32 G = 0; G < GMAX; G++)
	{
		FGroup& Gr = Groups[G];
		if (Gr.Mode == EMode::Free) continue;
		// empty: gone (merged, landed, away)
		if (Gr.N == 0 && SimTime - Gr.Born > 1.f) { Gr.Mode = EMode::Free; continue; }
		Gr.GoalT -= Dt;
		const bool bReached = Hyp2(Gr.Goal.X - Gr.C.X, Gr.Goal.Z - Gr.C.Z) < (Gr.Mode == EMode::Shore ? 25.f : 45.f);
		if (Gr.Mode == EMode::In && FMath::Abs(Gr.C.X) < Area.X && FMath::Abs(Gr.C.Z) < Area.Y && !IsLand(Gr.C.X, Gr.C.Z))
		{
			// arrived over the sea: a flock of the display (or the shore, by day)
			Gr.Mode = Act == EActivity::Foraging ? EMode::Shore : EMode::Local;
			ChooseGoal(G);
		}
		else if (Gr.GoalT <= 0.f || (bReached && Gr.Mode != EMode::Out && Gr.Mode != EMode::In && Gr.Form <= EForm::Stream))
		{
			ChooseGoal(G);
		}
		const EForm F = Gr.Form;
		if (F == EForm::Ball || F == EForm::Sheet || F == EForm::Funnel)
		{
			// the goal runs round the circle ahead of the flock, so the leaders circle
			if (F == EForm::Funnel) Gr.OrbitR = FMath::Max(22.f, Gr.OrbitR - Dt * 1.2f); // a tightening funnel
			const float A = FMath::Atan2(Gr.C.Z - Gr.OrbitC.Y, Gr.C.X - Gr.OrbitC.X) + Gr.OrbitW * 0.9f;
			Gr.Goal.X = Gr.OrbitC.X + FMath::Cos(A) * Gr.OrbitR;
			Gr.Goal.Z = Gr.OrbitC.Y + FMath::Sin(A) * Gr.OrbitR;
		}
		else if (F == EForm::Stream)
		{
			// lay the trail: where the leader has been
			Gr.TrailT -= Dt;
			const int32 L = Gr.Lead;
			if (Gr.TrailT <= 0.f && L >= 0 && L < Count && IsFlyingBird(L))
			{
				Gr.TrailT = TRAIL_DT;
				Gr.Trail[Gr.Head % TRAIL] = FVector3f(Px[L], Py[L], Pz[L]);
				Gr.Head++;
			}
		}
		const int32 L = Gr.Lead;
		if (Gr.LeadWait > 0) Gr.LeadWait--;
		else if (L < 0 || L >= Count || Grp[L] != G || !IsFlyingBird(L)) Gr.Lead = Gr.Edge;
		// the flock's height follows the goal at a climb or sink rate a flock can hold
		Gr.Band += FMath::Clamp((Gr.Goal.Y - Gr.Band) / BAND_TAU, -BAND_RATE, BAND_RATE) * Dt;
	}
}

/** A few birds at an edge turn on their own and the turn spreads (Attanasi et al. 2015). */
void FStarlingFlock::MaybeImpulse(float Dt, int32 FlyingN)
{
	for (int32 K = Impulses.Num() - 1; K >= 0; K--)
	{
		FImpulse& Im = Impulses[K];
		Im.T -= Dt;
		if (Im.T <= 0.f || Im.SeedBird >= Count || !IsFlyingBird(Im.SeedBird)) Impulses.RemoveAt(K);
	}
	if (Impulses.Num() >= 4 || FlyingN < 50 || Rnd() >= (P.SpontaneousTurns / 60.f) * Dt) return;
	// the tip of a flock, the flock picked by size
	float Pick = Rnd() * FlyingN;
	int32 G = -1;
	for (int32 K = 0; K < GMAX; K++)
	{
		const EMode M = Groups[K].Mode;
		if (M == EMode::Free || M == EMode::Out || M == EMode::In) continue;
		Pick -= Groups[K].N;
		if (Pick <= 0.f) { G = K; break; }
	}
	if (G < 0 || Groups[G].N < 30) return;
	const int32 J = Groups[G].Tip;
	if (J < 0 || J >= Count || !IsFlyingBird(J)) return;
	// turn 45-120 degrees left or right of the bird's heading, often with a climb or dive
	const float Sp = Hyp3(Vx[J], Vy[J], Vz[J]) + 1e-6f;
	const float Yaw = (PI / 4.f + Rnd() * (PI * 5.f) / 12.f) * (Rnd() < 0.5f ? -1.f : 1.f);
	const float HX = Vx[J] / Sp, HZ = Vz[J] / Sp, C = FMath::Cos(Yaw), S = FMath::Sin(Yaw);
	const float DY = Rnd() < 0.6f ? (Rnd() * 2.f - 1.f) * 0.45f : 0.f;
	FVector3f D(HX * C - HZ * S, DY, HX * S + HZ * C);
	D.Normalize();
	Impulses.Add({ J, G, D, 1.2f + Rnd() * 1.3f, 2.f, 0, FVector3f::ZeroVector });
}

/** How many local flocks a regime asks for. */
int32 FStarlingFlock::TargetGroups(int32 Local, float Gather) const
{
	const int32 Cap = FMath::Clamp(Local / 400, 1, 16);
	return FMath::Max(1, FMath::FloorToInt(1.3f + (Cap - 1) * FMath::Square(1.f - FMath::Clamp(Gather, 0.f, 1.f))));
}

/** Merges and splits of the local flocks toward the regime's number of flocks. */
void FStarlingFlock::StepRegime(float Dt)
{
	// natural swing: a slow random walk in [-1, 1] (~45 s)
	GWanderV += (Normal() * 0.35f - GWanderV * 0.15f) * Dt;
	GWander = FMath::Clamp(GWander + GWanderV * Dt * 0.12f, -1.f, 1.f);
	if (P.Gather >= 0.f) GatherNow = FMath::Clamp(P.Gather, 0.f, 1.f);
	else
	{
		float Base, Swing;
		AutoGather(Base, Swing);
		GatherNow = FMath::Clamp(Base + Swing * GWander, 0.f, 1.f);
	}

	RegimeT -= Dt;
	if (RegimeT > 0.f) return;
	RegimeT = 3.f + Rnd() * 5.f;
	int32 Local = 0, NLocal = 0;
	for (int32 G = 0; G < GMAX; G++) if (Groups[G].Mode == EMode::Local && Groups[G].N > 0) { Local += Groups[G].N; NLocal++; }
	if (!NLocal) return;
	const int32 Want = TargetGroups(Local, GatherNow);
	if (NLocal > Want)
	{
		// merge the two closest flocks: they fly to each other
		int32 A = -1, B = -1;
		float BD = 1e30f;
		for (int32 G = 0; G < GMAX; G++)
		{
			if (Groups[G].Mode != EMode::Local || !Groups[G].N) continue;
			for (int32 H = G + 1; H < GMAX; H++)
			{
				if (Groups[H].Mode != EMode::Local || !Groups[H].N) continue;
				const FVector3f D = Groups[G].C - Groups[H].C;
				const float DD = Hyp3(D.X, D.Y * 2.f, D.Z);
				if (DD < BD) { BD = DD; A = G; B = H; }
			}
		}
		if (A < 0) return;
		if (Groups[B].N > Groups[A].N) Swap(A, B);
		for (int32 I = 0; I < Count; I++) if (Grp[I] == B) Grp[I] = A;
		Groups[A].N += Groups[B].N; Groups[B].N = 0; Groups[B].Mode = EMode::Free;
		// meet halfway, then carry on
		Groups[A].Goal = (Groups[A].C + Groups[B].C) * 0.5f;
		Groups[A].GoalT = 6.f + Rnd() * 6.f;
		Groups[A].Lead = -1;
	}
	else if (NLocal < Want)
	{
		// split the largest flock into a left and a right half that peel apart
		const int32 A = LargestGroup(EMode::Local);
		if (A < 0 || Groups[A].N < 120) return;
		const int32 B = AllocGroup(EMode::Local, Groups[A].C);
		if (B == A) return;
		FGroup& GA = Groups[A];
		FGroup& GB = Groups[B];
		const float HL = GA.V.Length() + 1e-6f;
		const float LeftX = -GA.V.Y / HL, LeftZ = GA.V.X / HL; // to the left of the heading
		const float Bias = (Rnd() - 0.5f) * 0.3f * FMath::Sqrt((float)GA.N); // uneven halves
		int32 Moved = 0;
		for (int32 I = 0; I < Count; I++)
		{
			if (Grp[I] != A || !IsFlyingBird(I)) continue;
			if ((Px[I] - GA.C.X) * LeftX + (Pz[I] - GA.C.Z) * LeftZ > Bias) { Grp[I] = B; Moved++; }
		}
		GB.N = Moved; GA.N -= Moved;
		GB.Band = GA.Band;
		const float Off = 120.f + Rnd() * 120.f, Fwd = 60.f + Rnd() * 80.f;
		const float FX0 = GA.V.X / HL, FZ0 = GA.V.Y / HL;
		GB.Goal = FVector3f(GA.C.X + LeftX * Off + FX0 * Fwd, FMath::Clamp(GA.C.Y + (Rnd() * 2.f - 1.f) * 30.f, 8.f, 140.f), GA.C.Z + LeftZ * Off + FZ0 * Fwd);
		GB.GoalT = 8.f + Rnd() * 8.f;
		GA.Goal.X = GA.C.X - LeftX * Off + FX0 * Fwd; GA.Goal.Z = GA.C.Z - LeftZ * Off + FZ0 * Fwd;
		GA.GoalT = 8.f + Rnd() * 8.f; GA.Lead = -1; GB.Lead = -1;
		// splitting flocks stay over the water
		for (FGroup* Gp : { &GA, &GB })
		{
			Gp->Goal.X = FMath::Clamp(Gp->Goal.X, -Area.X, Area.X);
			Gp->Goal.Z = FMath::Clamp(Gp->Goal.Z, -Area.Y, Area.Y);
		}
	}
}
