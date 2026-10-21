// The day: the schedule (Murmuration's Act), the regime, departures and arrivals, the roost.
//
//  night        roosting on the land nearest the flock
//  dawn         exodus: waves of birds leave inland, ~3 min apart; a few stay along the shore
//  day          foraging inland (not simulated), small groups loafing along the shore
//  afternoon    groups return from inland and along the coast, gather, merge
//  sunset       the murmuration: one big flock over the sea
//  dusk         settling: descents into the roost in waves, funnels over it
//  night        nobody stays up

#include "Murmuration/StarlingFlock.h"
#include "Murmuration/StarlingFlockMath.h"

using namespace StarlingFlockMath;

const TCHAR* FStarlingFlock::ActivityName(EActivity A)
{
	switch (A)
	{
	case EActivity::Roosting: return TEXT("roosting");
	case EActivity::Exodus: return TEXT("dawn exodus");
	case EActivity::Foraging: return TEXT("foraging inland");
	case EActivity::Returning: return TEXT("returning from inland");
	case EActivity::Gathering: return TEXT("pre-roost gathering");
	case EActivity::Murmuration: return TEXT("murmuration");
	case EActivity::Settling: return TEXT("settling to roost");
	}
	return TEXT("?");
}

FStarlingFlock::EActivity FStarlingFlock::ActivityAt(float H) const
{
	auto After = [H](float T) { const float D = Dh24(H, T); return D >= 0.f && D < 12.f; };
	if (After(NightStart()) || !After(ExStart())) return EActivity::Roosting;
	if (!After(ExEnd())) return EActivity::Exodus;
	if (!After(DayEnd())) return EActivity::Foraging;
	if (!After(GatherStart())) return EActivity::Returning;
	if (!After(MurmStart())) return EActivity::Gathering;
	if (!After(SettleStart())) return EActivity::Murmuration;
	return EActivity::Settling;
}

/** Share of the flock that stays at the coast by day (small groups along the shore). */
float FStarlingFlock::ShoreShare() const
{
	return Count ? FMath::Min(1.f, FMath::Max((float)FMath::Min(Count, 40), FMath::Min(400.f, Count * 0.05f)) / Count) : 0.f;
}

/** Which dawn wave a bird leaves with (0..WAVES-1); -1 = stays at the coast. */
int32 FStarlingFlock::WaveOf(int32 I) const
{
	const float R = RankA(I), S = ShoreShare();
	if (R < S) return -1;
	return FMath::Min(WAVES - 1, (int32)((1.f - R) / (1.f - S) * WAVES));
}

/** Share of the flock back at the coast during the afternoon return. */
float FStarlingFlock::PresentShare(float H) const
{
	const float S = ShoreShare();
	const EActivity A = ActivityAt(H);
	if (A == EActivity::Returning || A == EActivity::Gathering)
	{
		const float U = FMath::Clamp(Dh24(H, DayEnd()) / (MurmStart() - 0.1f - DayEnd()), 0.f, 1.f);
		return S + (1.f - S) * U;
	}
	if (A == EActivity::Foraging) return S;
	return 1.f;
}

/** Automatic gather value for the activity, before the natural swing. */
void FStarlingFlock::AutoGather(float& Base, float& Swing) const
{
	if (!bClockOn) { Base = Phase == EPhase::Roost ? 0.9f : 0.58f; Swing = Phase == EPhase::Roost ? 0.1f : 0.45f; return; }
	switch (Act)
	{
	case EActivity::Foraging: Base = 0.05f; Swing = 0.1f; return;
	case EActivity::Returning: Base = 0.15f; Swing = 0.15f; return;
	case EActivity::Gathering:
	{
		const float U = FMath::Clamp(Dh24(Hours, GatherStart()) / (MurmStart() - GatherStart()), 0.f, 1.f);
		Base = 0.3f + 0.5f * U; Swing = 0.2f; return;
	}
	case EActivity::Murmuration: Base = 0.88f; Swing = 0.15f; return;
	case EActivity::Settling: Base = 0.9f; Swing = 0.1f; return;
	default: Base = 0.4f; Swing = 0.2f; return;
	}
}

void FStarlingFlock::SetClock(float InHours, float InSunrise, float InSunset, float InDusk)
{
	Sunrise = InSunrise; Sunset = InSunset; Dusk = InDusk;
	const float H = Wrap24(InHours);
	if (!bClockOn)
	{
		bClockOn = true;
		Hours = H; HRate = 0.f;
		Resync();
	}
	else
	{
		const float Since = SimTime - LastClockSim;
		const float Moved = Dh24(H, LastClockH);
		// a jump (a loaded game, a changed calendar): backwards, or much further than the clock
		// runs. A faster clock is not a jump: the rate follows it.
		const bool bJump = Moved < -0.05f || (FMath::Abs(Moved) > 0.25f && FMath::Abs(Moved) > 3.f * FMath::Abs(HRate) * FMath::Max(Since, 1e-3f) + 0.25f);
		if (!bJump && Since > 0.2f)
		{
			// the rate of the clock (hours per simulated second), from successive calls
			const float R = Moved / Since;
			HRate = FMath::Abs(R - HRate) > FMath::Max(1e-4f, FMath::Abs(HRate) * 0.3f) ? R : HRate + (R - HRate) * 0.5f;
		}
		Hours = H;
		if (bJump) Resync();
		else if (ActivityAt(H) != Act) EnterActivity(ActivityAt(H));
	}
	HRate = FMath::Clamp(HRate, 0.f, 2.f);
	LastClockSim = SimTime; LastClockH = H;
}

void FStarlingFlock::EnterActivity(EActivity A)
{
	const EActivity Prev = Act;
	Act = A;
	const EPhase NewPhase = A == EActivity::Settling || A == EActivity::Roosting ? EPhase::Roost : EPhase::Murmur;
	if (NewPhase == EPhase::Roost && Phase != EPhase::Roost)
	{
		ChooseRoost();
		Phase = NewPhase;
		// the first birds to drop seed the descent waves
		for (int32 I = 0; I < Count; I++) if (State[I] == Flying && Rnd() < 0.01f) StartDescent(I);
	}
	Phase = NewPhase;
	if (A == EActivity::Exodus && Prev == EActivity::Roosting)
	{
		for (int32& W : WaveGroup) W = -1;
		bShoreGroupsMade = false;
	}
	if (A == EActivity::Returning || A == EActivity::Gathering || A == EActivity::Murmuration || A == EActivity::Settling)
	{
		// the shore groups and arrivals join the display
		for (int32 G = 0; G < GMAX; G++) if (Groups[G].Mode == EMode::Shore) { Groups[G].Mode = EMode::Local; ChooseGoal(G); }
	}
	if (A == EActivity::Settling || A == EActivity::Roosting)
	{
		for (int32 G = 0; G < GMAX; G++)
		{
			if (Groups[G].Mode == EMode::Local || Groups[G].Mode == EMode::In) { Groups[G].Mode = EMode::Local; ChooseGoal(G); }
		}
	}
}

/** What a bird should be doing at this hour (after a jump of the clock). */
FStarlingFlock::EWant FStarlingFlock::WantAt(int32 I, float H) const
{
	switch (ActivityAt(H))
	{
	case EActivity::Roosting: return EWant::Perched;
	case EActivity::Exodus:
	{
		const int32 W = WaveOf(I);
		if (W < 0) return Dh24(H, Sunrise + 0.05f) >= 0.f ? EWant::Present : EWant::Perched;
		// a wave that left more than ~2 min ago is gone; the current one is still taking off
		return Dh24(H, WaveStart(W)) > 0.035f ? EWant::Away : EWant::Perched;
	}
	case EActivity::Foraging: case EActivity::Returning: case EActivity::Gathering:
		return RankA(I) < PresentShare(H) ? EWant::Present : EWant::Away;
	case EActivity::Murmuration: return EWant::Present;
	case EActivity::Settling:
	{
		const float U = FMath::Clamp(Dh24(H, SettleStart()) / (NightStart() - SettleStart()), 0.f, 1.f);
		return RankB(I) < FMath::Pow(U, 0.8f) ? EWant::Perched : EWant::Present;
	}
	}
	return EWant::Present;
}

/** Put every bird into the state it should be in now (at the start, after a jump of the clock). */
void FStarlingFlock::Resync()
{
	Act = ActivityAt(Hours);
	Phase = Act == EActivity::Settling || Act == EActivity::Roosting ? EPhase::Roost : EPhase::Murmur;
	Impulses.Reset();
	for (int32& W : WaveGroup) W = -1;
	bShoreGroupsMade = Act != EActivity::Exodus || Dh24(Hours, Sunrise + 0.05f) >= 0.f;
	if (Phase == EPhase::Roost || Act == EActivity::Exodus) ChooseRoost();
	TArray<int32> Spawn;
	for (int32 I = 0; I < Count; I++)
	{
		const uint8 St = State[I];
		if (St == Visiting || St == Grounded) continue;
		const EWant W = WantAt(I, Hours);
		if (W == EWant::Perched)
		{
			if (St != Perched)
			{
				StartDescent(I);
				FVector3f At;
				PerchPos(I, At);
				Settle(I, At);
			}
		}
		else if (W == EWant::Away) { if (St != Away) MakeAway(I); }
		else if (St == Perched || St == Away || St == Descending || Grp[I] >= GMAX || Groups[Grp[I]].Mode == EMode::Free) Spawn.Add(I);
	}
	// commuting flocks make no sense after a jump: they join the coast
	for (int32 G = 0; G < GMAX; G++)
	{
		FGroup& Gr = Groups[G];
		if (Gr.Mode == EMode::Out || Gr.Mode == EMode::In) Gr.Mode = Act == EActivity::Foraging || Act == EActivity::Exodus ? EMode::Shore : EMode::Local;
		if (Gr.Mode != EMode::Free) ChooseGoal(G);
	}
	if (Spawn.Num())
	{
		const bool bShore = Act == EActivity::Foraging || Act == EActivity::Exodus;
		const int32 Target = bShore ? FMath::Max(1, FMath::RoundToInt(Spawn.Num() / 70.f)) : TargetGroups(Spawn.Num(), GatherNow);
		PlaceGroups(Spawn, FMath::Min(Target, 20), bShore ? EMode::Shore : EMode::Local);
	}
}

/** Spread birds over k new flocks over the water (or along the shore), flying. */
void FStarlingFlock::PlaceGroups(const TArray<int32>& Birds, int32 K, EMode Mode)
{
	K = FMath::Clamp(K, 1, Birds.Num());
	for (int32 Q = 0; Q < K; Q++)
	{
		const int32 From = Q * Birds.Num() / K, To = (Q + 1) * Birds.Num() / K;
		const int32 N = To - From;
		FVector3f C;
		if (Mode == EMode::Shore && ShorePts.Num())
		{
			const FVector2f& S = ShorePts[FMath::Min(ShorePts.Num() - 1, (int32)(Rnd() * ShorePts.Num()))];
			C = FVector3f(S.X, 4.f + Rnd() * 8.f, S.Y);
		}
		else if (Phase == EPhase::Roost && bHaveRoost)
		{
			C = FVector3f(Roost.X + (Rnd() * 2.f - 1.f) * 80.f, Roost.Y + 25.f + Rnd() * 20.f, Roost.Z + (Rnd() * 2.f - 1.f) * 60.f);
		}
		else
		{
			C = FVector3f((Rnd() * 2.f - 1.f) * Area.X * 0.8f, P.Altitude * (0.6f + Rnd() * 0.8f), (Rnd() * 2.f - 1.f) * Area.Y * 0.8f);
		}
		const int32 G = AllocGroup(Mode, C);
		const float A = Rnd() * 2.f * PI;
		const float HX = FMath::Cos(A), HZ = FMath::Sin(A);
		// a flat, dense flock (~1 m apart), all heading one way
		const float R = FMath::Sqrt((float)N) * 0.5f + 2.f;
		for (int32 S = From; S < To; S++)
		{
			const float RR = FMath::Sqrt(Rnd()) * R, AA = Rnd() * 2.f * PI;
			const float X = C.X + FMath::Cos(AA) * RR, Z = C.Z + FMath::Sin(AA) * RR * 0.7f;
			FlyAt(Birds[S], FVector3f(X, FMath::Max(C.Y + Gauss() * 2.f, GroundAt(X, Z) + 3.f), Z), FVector3f(HX * 10.f, 0.f, HZ * 10.f), G);
		}
	}
}

void FStarlingFlock::FlyAt(int32 I, const FVector3f& At, const FVector3f& Vel, int32 G)
{
	Px[I] = At.X; Py[I] = At.Y; Pz[I] = At.Z;
	Vx[I] = Vel.X + Gauss() * 0.5f; Vy[I] = Vel.Y; Vz[I] = Vel.Z + Gauss() * 0.5f;
	State[I] = Flying;
	Timer[I] = 0.f; Refractory[I] = 0.f; EscDelay[I] = 0.f;
	Bank[I] = 0.f; Fold[I] = 0.f; FlapAmp[I] = 0.7f;
	Grp[I] = (uint8)G;
	Sfx[I] = Sfy[I] = Sfz[I] = 0.f;
}

void FStarlingFlock::MakeAway(int32 I)
{
	State[I] = Away;
	if (SpotOf[I] >= 0) FreeSpot(I);
	Vx[I] = Vz[I] = 0.01f; Vy[I] = 0.f;
	Sfx[I] = Sfy[I] = Sfz[I] = 0.f;
}

/** Departures at dawn, arrivals in the afternoon, the end of the day. */
void FStarlingFlock::StepDay(float Dt)
{
	if (!bClockOn) return;
	const float H = Hours;
	if ((Act == EActivity::Exodus || Act == EActivity::Foraging) && PerchedLast > 0)
	{
		// dawn waves: each wave takes off within ~20 s and heads inland as a group
		int32 Started = 0;
		for (int32 W = 0; W < WAVES; W++) if (Dh24(H, WaveStart(W)) >= 0.f) Started = W + 1;
		if (Started)
		{
			const float Pr = Dt / 6.f;
			for (int32 I = 0; I < Count; I++)
			{
				if (State[I] != Perched || Rnd() >= Pr) continue;
				const int32 W = WaveOf(I);
				if (W < 0 || W >= Started) continue;
				int32 G = WaveGroup[W];
				if (G < 0 || Groups[G].Mode != EMode::Out) G = WaveGroup[W] = AllocGroup(EMode::Out, FVector3f(Roost.X, Roost.Y + 30.f, Roost.Z));
				Launch(I, G);
			}
		}
		// the birds that stay wake a little later and loaf along the shore
		if (Dh24(H, Sunrise + 0.05f) >= 0.f && !bShoreGroupsMade)
		{
			bShoreGroupsMade = true;
			TArray<int32> Stay;
			for (int32 I = 0; I < Count; I++) if (State[I] == Perched && WaveOf(I) < 0) Stay.Add(I);
			const int32 K = FMath::Max(1, FMath::RoundToInt(Stay.Num() / 70.f));
			TArray<int32> Gs;
			for (int32 Q = 0; Q < K; Q++) Gs.Add(AllocGroup(EMode::Shore, FVector3f(Roost.X, Roost.Y + 6.f, Roost.Z)));
			for (int32 S = 0; S < Stay.Num(); S++) Launch(Stay[S], Gs[S % K]);
		}
	}
	if (Act == EActivity::Returning || Act == EActivity::Gathering || Act == EActivity::Murmuration || Act == EActivity::Settling)
	{
		// groups come back from inland, from different directions
		ArriveT -= Dt;
		if (ArriveT <= 0.f)
		{
			const float Share = PresentShare(H);
			if (AwayLast > 0 && Count - AwayLast < Share * Count - FMath::Max(5.f, Count * 0.01f))
			{
				const int32 Size = FMath::Max(30, FMath::RoundToInt(Count / (Act == EActivity::Returning ? 30.f : 12.f)));
				TArray<int32> Arrivals;
				for (int32 I = 0; I < Count && Arrivals.Num() < Size; I++) if (State[I] == Away && RankA(I) < Share + 0.02f) Arrivals.Add(I);
				if (Arrivals.Num()) Arrive(Arrivals);
				ArriveT = Act == EActivity::Returning ? 2.f + Rnd() * 3.f : 0.8f;
			}
			else ArriveT = 1.f;
		}
	}
	if (Act == EActivity::Roosting && (AirLast > 0 || AwayLast > 0))
	{
		// night: nobody stays up (a bird the person just flushed circles a little first)
		for (int32 I = 0; I < Count; I++)
		{
			if ((State[I] == Flying || State[I] == Escaping) && SimTime - LeftBeach[I] > 8.f) StartDescent(I);
			else if (State[I] == Away)
			{
				StartDescent(I);
				FVector3f At;
				PerchPos(I, At);
				Settle(I, At);
			}
		}
	}
}

/** A group of birds appears inland or along the coast and flies to the sea. */
void FStarlingFlock::Arrive(const TArray<int32>& Birds)
{
	FVector2f At;
	float Y;
	if (Rnd() < 0.6f)
	{
		// from inland, low over the land
		const FVector2f Side(-Inland.Y, Inland.X);
		At = Inland * (FMath::Max(Area.X, Area.Y) + 250.f) + Side * (Rnd() * 2.f - 1.f) * 300.f;
		Y = GroundTop + 14.f + Rnd() * 8.f;
	}
	else
	{
		At = CoastPoint(Rnd() < 0.5f ? -1.f : 1.f);
		Y = 50.f + Rnd() * 50.f;
	}
	const int32 G = AllocGroup(EMode::In, FVector3f(At.X, Y, At.Y));
	const FVector2f T = FVector2f(Groups[G].Goal.X, Groups[G].Goal.Z) - At;
	const FVector2f TD = T.GetSafeNormal();
	const float R = FMath::Sqrt((float)Birds.Num()) * 0.5f + 3.f;
	for (int32 I : Birds)
	{
		const float RR = FMath::Sqrt(Rnd()) * R, AA = Rnd() * 2.f * PI;
		FlyAt(I, FVector3f(At.X + FMath::Cos(AA) * RR, Y + Gauss() * 3.f, At.Y + FMath::Sin(AA) * RR), FVector3f(TD.X * 10.f, 0.f, TD.Y * 10.f), G);
	}
}

// --- the roost -------------------------------------------------------------------------------

bool FStarlingFlock::NearestLand(float X, float Z, FVector2f& Out) const
{
	float BD = 1e30f;
	bool bFound = false;
	for (int32 IZ = 0; IZ < GNz; IZ++)
	{
		for (int32 IX = 0; IX < GNx; IX++)
		{
			if (GroundH[IZ * GNx + IX] <= 0.5f) continue;
			const float CX = GX0 + IX * GCell, CZ = GZ0 + IZ * GCell;
			const float D = FMath::Square(CX - X) + FMath::Square(CZ - Z);
			if (D < BD) { BD = D; Out = FVector2f(CX, CZ); bFound = true; }
		}
	}
	return bFound;
}

/**
 * The roost: the land nearest the flock (its biggest group, or the middle of the area), a little
 * way in from the edge, and a patch of sleeping spots on the ground there.
 */
void FStarlingFlock::ChooseRoost()
{
	FVector3f From(0.f, 0.f, 0.f);
	const int32 G = LargestGroup(EMode::Local, EMode::Shore);
	if (G >= 0 && Groups[G].N > 0) From = Groups[G].C;
	FVector2f Land;
	Perches.Reset();
	PerchCursor = 0;
	if (!NearestLand(From.X, From.Z, Land))
	{
		// no land anywhere: they sleep on the water in the middle of the area (a raft)
		bHaveRoost = true;
		Roost = FVector3f(0.f, 0.f, 0.f);
	}
	else
	{
		// step in from the edge so the patch lies on the land
		const FVector2f In = (Land - FVector2f(From.X, From.Z)).GetSafeNormal();
		for (int32 K = 0; K < 4 && IsLand(Land.X + In.X * GCell, Land.Y + In.Y * GCell); K++) Land += In * GCell;
		bHaveRoost = true;
		Roost = FVector3f(Land.X, Probe(Land.X, Land.Y), Land.Y);
	}
	// sleeping spots ~0.35 m apart in a patch round the roost, on land
	const int32 Want = FMath::Min(Count, 6000);
	const float R0 = 2.f + FMath::Sqrt((float)Want) * 0.35f;
	for (int32 Try = 0; Try < Want * 6 && Perches.Num() < Want; Try++)
	{
		const float R = R0 * (1.f + Try / (float)(Want * 2)) * FMath::Sqrt(Rnd());
		const float A = Rnd() * 2.f * PI;
		const float X = Roost.X + FMath::Cos(A) * R, Z = Roost.Z + FMath::Sin(A) * R;
		if (Roost.Y > 0.5f && !IsLand(X, Z)) continue;
		Perches.Add(FVector3f(X, Roost.Y > 0.5f ? Probe(X, Z) : 0.f, Z));
	}
	if (!Perches.Num()) Perches.Add(Roost);
}

void FStarlingFlock::PerchPos(int32 I, FVector3f& Out) const
{
	const int32 N = Perches.Num();
	if (N == 0) { Out = Roost; return; }
	const int32 Pi = FMath::Max(0, PerchOf[I]) % N;
	// more birds than spots: shuffle them a little
	const int32 Layer = FMath::Max(0, PerchOf[I]) / N;
	Out = Perches[Pi] + FVector3f(FMath::Frac(Layer * 0.37f) - 0.5f, 0.f, FMath::Frac(Layer * 0.61f) - 0.5f) * 0.6f;
}

void FStarlingFlock::StartDescent(int32 I)
{
	if (SpotOf[I] >= 0) FreeSpot(I);
	if (!bHaveRoost) ChooseRoost();
	State[I] = Descending;
	const int32 N = FMath::Max(1, Perches.Num());
	PerchOf[I] = PerchCursor + FMath::Min(N - 1, (int32)(Rnd() * N));
	PerchCursor += N;
}

void FStarlingFlock::Settle(int32 I, const FVector3f& At)
{
	State[I] = Perched;
	Px[I] = At.X; Py[I] = At.Y; Pz[I] = At.Z;
	// face along the heading, kept horizontal; a tiny velocity keeps the orientation
	const float H = Hyp2(Vx[I], Vz[I]) + 1e-6f;
	Vx[I] = Vx[I] / H * 0.01f; Vy[I] = 0.f; Vz[I] = Vz[I] / H * 0.01f;
	Bank[I] = 0.f;
	Fold[I] = 1.f;
	FlapAmp[I] = 0.f;
}

void FStarlingFlock::Launch(int32 I, int32 G)
{
	State[I] = Flying;
	const float A = Rnd() * 2.f * PI;
	Vx[I] = FMath::Cos(A) * 6.f; Vy[I] = 4.f; Vz[I] = FMath::Sin(A) * 6.f;
	Py[I] += 0.3f;
	Fold[I] = 0.5f;
	FlapAmp[I] = 1.f;
	Grp[I] = (uint8)G;
	Refractory[I] = 0.f; Timer[I] = 0.f;
}
