#include "Plants/PlantModel.h"

namespace
{
	constexpr float Never = TNumericLimits<float>::Max();
	constexpr float GoldenAngle = 2.39996323f;
	const FVector3f Up(0.f, 0.f, 1.f);

	FORCEINLINE uint64 SplitMix(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	float Smooth(float E0, float E1, float X)
	{
		const float T = FMath::Clamp((X - E0) / FMath::Max(E1 - E0, 1e-4f), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Order-1 length cap along the mature trunk: Profile at u = 0, 0.45 and 1 of MaxHeight. */
	float Profile(const FVector& P, float U)
	{
		return U < 0.45f ? FMath::Lerp((float)P.X, (float)P.Y, Smooth(0.f, 0.45f, U))
		                 : FMath::Lerp((float)P.Y, (float)P.Z, Smooth(0.45f, 1.f, U));
	}

	/** A unit vector perpendicular to D, rotated by Angle about it. */
	FVector3f Perp(const FVector3f& D, float Angle)
	{
		const FVector3f Ref = FMath::Abs(D.Z) < 0.9f ? Up : FVector3f(1.f, 0.f, 0.f);
		const FVector3f A = FVector3f::CrossProduct(Ref, D).GetSafeNormal();
		const FVector3f B = FVector3f::CrossProduct(D, A);
		return A * FMath::Cos(Angle) + B * FMath::Sin(Angle);
	}

	struct FShoot
	{
		float Tropism = 0.f, Wobble = 0.f, Kink = 0.f, KinkSpacing = 1.f, Droop = 0.f;
	};

	/**
	 * A shoot's curve: straight from Dir0, pulled up by Tropism, bent at random by Wobble, turned at
	 * every sympodial kink, and sagging more the farther out it gets (Droop).
	 */
	TArray<FVector3f> GrowCurve(const FVector3f& Start, FVector3f Dir, float Length, float Step, const FShoot& Sh, uint32 Seed, uint32 Key)
	{
		const float Tropism = Sh.Tropism, Wobble = Sh.Wobble;
		TArray<FVector3f> P;
		const int32 N = FMath::CeilToInt(Length / Step) + 1;
		P.Reserve(N + 1);
		P.Add(Start);
		for (int32 I = 0; I < N; I++)
		{
			P.Add(P.Last() + Dir * Step);
			const FVector3f R(PlantModel::Hash01(Seed, Key, I, 1) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 2) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 3) * 2.f - 1.f);
			const FVector3f Side = R - Dir * FVector3f::DotProduct(R, Dir);
			const float S0 = I * Step;
			Dir = (Dir + Up * ((Tropism - Sh.Droop * S0) * Step) + Side * (Wobble * Step)).GetSafeNormal(UE_SMALL_NUMBER, Up);
			if (Sh.Kink > 0.f && FMath::FloorToInt((S0 + Step) / Sh.KinkSpacing) > FMath::FloorToInt(S0 / Sh.KinkSpacing))
			{
				const FVector3f K(PlantModel::Hash01(Seed, Key, I, 4) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 5) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 6) * 2.f - 1.f);
				const FVector3f KSide = (K - Dir * FVector3f::DotProduct(K, Dir)).GetSafeNormal();
				const float A = Sh.Kink * (0.4f + 0.6f * PlantModel::Hash01(Seed, Key, I, 7));
				Dir = (Dir * FMath::Cos(A) + KSide * FMath::Sin(A)).GetSafeNormal(UE_SMALL_NUMBER, Up);
			}
		}
		return P;
	}

	/** Arc length at which a curve first leaves the species' crown dome (or its full length when it never does). */
	float EnvelopeLength(const FPlantSpecies& S, const TArray<FVector3f>& P, float Step)
	{
		if (S.CrownWidth <= 0.f)
		{
			return TNumericLimits<float>::Max();
		}
		const float Base = S.CrownBase * S.MaxHeight, Mid = 0.5f * (Base + S.MaxHeight), HalfH = 0.5f * (S.MaxHeight - Base);
		const float R = 0.5f * S.CrownWidth * S.MaxHeight;
		for (int32 I = 1; I < P.Num(); I++)
		{
			// a dome over a drum: full width below the middle, an ellipse above
			const float U = FMath::Max(0.f, (P[I].Z - Mid) / HalfH);
			const float Allowed = U >= 1.f ? 0.f : R * FMath::Sqrt(1.f - U * U);
			if (FVector2f(P[I].X, P[I].Y).Size() > Allowed || P[I].Z > S.MaxHeight)
			{
				return (I - 1) * Step;
			}
		}
		return TNumericLimits<float>::Max();
	}
}

float PlantModel::Hash01(uint32 Seed, uint32 A, uint32 B, uint32 C)
{
	const uint64 H = SplitMix(SplitMix(SplitMix(((uint64)Seed << 32) ^ A) ^ B) ^ C);
	return (float)(H >> 40) * (1.f / 16777216.f);
}

float PlantModel::SnowCover(float Day)
{
	return FMath::Max(Smooth(325.f, 345.f, Day), 1.f - Smooth(65.f, 90.f, Day));
}

float PlantModel::SmoothStep(float E0, float E1, float X)
{
	return Smooth(E0, E1, X);
}

float PlantModel::Height(const FPlantSpecies& S, float Age)
{
	return Age <= 0.f ? 0.f : S.MaxHeight * FMath::Pow(1.f - FMath::Exp(-S.GrowthRate * Age), S.GrowthShape);
}

float PlantModel::AgeAtHeight(const FPlantSpecies& S, float H)
{
	if (H <= 0.f) return 0.f;
	if (H >= S.MaxHeight * 0.9999f) return Never;
	const float X = FMath::Pow(H / S.MaxHeight, 1.f / S.GrowthShape);
	return -FMath::Loge(1.f - X) / S.GrowthRate;
}

FPlantSeason PlantModel::Season(const FPlantSpecies& S, uint32 Seed, float Day)
{
	FPlantSeason Out;
	// this tree's own calendar: some trees are always a few days ahead of their neighbours
	const float D = Day - (Hash01(Seed, 0x5EA5u) * 2.f - 1.f) * S.PhenologyJitter;
	const float Unfold = Smooth(S.BudBreakDay, S.FullLeafDay, D);
	Out.LeafScale = S.bEvergreen ? 1.f : (Unfold > 0.f ? 0.25f + 0.75f * Unfold : 0.f);
	// an evergreen's new shoots are light in May; the rest of the year it wears its summer or, from autumn to
	// spring, its winter colour (AutumnLeafColour) and nothing falls
	const float NewShoots = S.bEvergreen ? Unfold * (1.f - Smooth(S.FullLeafDay, S.FullLeafDay + 35.f, D)) : 1.f - Smooth(S.FullLeafDay, S.FullLeafDay + 35.f, D);
	const FLinearColor Base = FMath::Lerp(S.SummerLeafColour, S.SpringLeafColour, NewShoots);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		const float Off = (K - 1) * 6.f; // bucket 0 turns and falls first
		const FLinearColor Autumn = FMath::Lerp(S.AutumnLeafColour, S.AutumnLeafColourLate, K / (FPlantSeason::NumBuckets - 1.f));
		if (S.bEvergreen)
		{
			const float Winter = FMath::Max(Smooth(S.ColourStartDay + Off, S.ColourFullDay + Off, D), 1.f - Unfold);
			Out.Colour[K] = FMath::Lerp(Base, Autumn, Winter);
			continue;
		}
		Out.Colour[K] = FMath::Lerp(Base, Autumn, Smooth(S.ColourStartDay + Off, S.ColourFullDay + Off, D));
		// a few leaves let go first, then more and more
		auto FallenOn = [&](float Dd) { return Smooth(S.BudBreakDay, S.FullLeafDay, Dd) <= 0.f ? 1.f : FMath::Pow(Smooth(S.LeafDropStartDay + Off, S.BareDay + Off, Dd), 1.6f); };
		Out.Fallen[K] = FallenOn(D);
		Out.LitterFrom[K] = D - S.LitterDays < S.BudBreakDay ? 0.f : FallenOn(D - S.LitterDays);
	}
	Out.Day = D;
	if (S.FruitSize > 0.f)
	{
		// fruit kept into the next year (FruitFallDay past 366) is on the tree from its set day to that day of the year after
		const bool bKept = S.FruitFallDay > 366.f;
		const float Df = bKept && D < S.FruitSetDay ? D + 366.f : D;
		if (Df >= S.FruitSetDay && Df < S.FruitFallDay + (bKept ? 0.f : S.FruitRotDays))
		{
			Out.FruitScale = 0.3f + 0.7f * Smooth(S.FruitSetDay, S.FruitRipeDay, Df);
			Out.FruitRipeness = Smooth(S.FruitRipeDay - 40.f, S.FruitRipeDay, Df);
			Out.FruitColour = FMath::Lerp(S.FruitUnripeColour, S.FruitColour, Out.FruitRipeness);
		}
	}
	// snow on the branches from late November to the March thaw (the calendar's climate, not the tree's)
	Out.Snow = PlantModel::SnowCover(Day);
	return Out;
}

float PlantModel::Lifespan(const FPlantSpecies& S, uint32 Seed)
{
	// log-normal: most trees die near the median, a few young, a few very old
	const float U1 = FMath::Max(Hash01(Seed, 0x11FEu, 1), 1e-6f), U2 = Hash01(Seed, 0x11FEu, 2);
	const float Z = FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
	return FMath::Clamp(S.LifeMedian * FMath::Exp(S.LifeSpread * Z), 0.3f * S.LifeMedian, S.LifeMax);
}

float PlantModel::Vigour(const FPlantSpecies& S, uint32 Seed, float Age)
{
	const float L = Lifespan(S, Seed);
	return 1.f - Smooth(L - S.DeclineYears, L, Age);
}

bool PlantModel::IsGone(const FPlantSpecies& S, uint32 Seed, float Age)
{
	return Age >= Lifespan(S, Seed) + S.SnagYears;
}

FPlantSummary PlantModel::Summary(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
{
	FPlantSummary Out;
	Out.Lifespan = Lifespan(S, Seed);
	Out.Vigour = Vigour(S, Seed, Age);
	Out.Height = Height(S, Age);
	Out.CrownBaseHeight = S.CrownBase * Out.Height;
	Out.TrunkRadius = FMath::Max(S.MinRadius, S.TrunkRadiusPerHeight * Out.Height + S.RingWidth * Age);
	// the widest branches sit about a third of the way up the crown; their reach follows from
	// when the trunk passed them
	const float Leader = S.MaxHeight * S.LeaderFraction;
	const float Z = FMath::Min(Out.CrownBaseHeight + 0.35f * (Out.Height - Out.CrownBaseHeight), Leader * 0.9f);
	const float T = FMath::Max(0.f, Age - AgeAtHeight(S, Z));
	const float L = S.Branch1MaxLength * Profile(S.Branch1Profile, Z / Leader) * (1.f - FMath::Exp(-T / S.LengthTau));
	Out.CrownRadius = L * FMath::Sin(FMath::DegreesToRadians(S.Branch1Angle) * 0.8f) + FMath::Min(L, 1.f) * S.Branch2MaxLength * 0.3f + S.LeafClusterSize * 0.5f;
	if (S.CrownWidth > 0.f)
	{
		Out.CrownRadius = FMath::Min(Out.CrownRadius, 0.5f * S.CrownWidth * S.MaxHeight * FMath::Min(1.f, Out.Height / (0.8f * S.MaxHeight)));
	}

	const FPlantSeason Sn = Season(S, Seed, Day);
	float On = 0.f;
	FLinearColor C = FLinearColor::Black;
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		On += 1.f - Sn.Fallen[K];
		C += Sn.Colour[K] * (1.f - Sn.Fallen[K]);
	}
	Out.LeafAmount = Sn.LeafScale * On / FPlantSeason::NumBuckets * Out.Vigour;
	Out.LeafColour = On > 0.f ? C / On : S.Bark.RidgeColour;
	Out.LeafColour.A = 1.f;
	return Out;
}

float FPlantSkeleton::FBranch::TimeAtLength(float L) const
{
	return L >= MaxLength ? Never : -Tau * FMath::Loge(1.f - L / MaxLength);
}

FVector3f FPlantSkeleton::FBranch::At(float L) const
{
	const float F = FMath::Max(L, 0.f) / Step;
	const int32 I = FMath::Min(FMath::FloorToInt(F), Points.Num() - 2);
	return FMath::Lerp(Points[I], Points[I + 1], F - I);
}

FVector3f FPlantSkeleton::FBranch::DirAt(float L) const
{
	const int32 I = FMath::Clamp(FMath::FloorToInt(FMath::Max(L, 0.f) / Step), 0, Points.Num() - 2);
	return (Points[I + 1] - Points[I]).GetSafeNormal(UE_SMALL_NUMBER, Up);
}

void FPlantSkeleton::Build(const FPlantSpecies& Species, uint32 InSeed)
{
	S = Species;
	Seed = InSeed;
	Branches.Reset();
	using PlantModel::Hash01;

	FBranch& Trunk = Branches.AddDefaulted_GetRef();
	Trunk.Order = 0;
	Trunk.Step = 0.25f;
	const float Leader = S.MaxHeight * S.LeaderFraction;
	Trunk.MaxLength = Leader;
	FShoot TrunkShoot;
	TrunkShoot.Tropism = 0.5f;
	TrunkShoot.Wobble = S.Wobble * 0.3f;
	TrunkShoot.Kink = S.Kink * 0.3f;
	TrunkShoot.KinkSpacing = S.KinkSpacing * 2.f;
	Trunk.Points = GrowCurve(FVector3f::ZeroVector, Up, Leader * 1.02f, Trunk.Step, TrunkShoot, Seed, 0);
	FShoot Shoot1;
	Shoot1.Tropism = S.Tropism;
	Shoot1.Wobble = S.Wobble;
	Shoot1.Kink = S.Kink;
	Shoot1.KinkSpacing = S.KinkSpacing;
	Shoot1.Droop = S.Droop;
	FShoot Shoot2 = Shoot1;
	Shoot2.Wobble = S.Wobble * 1.5f;
	Shoot2.Tropism = FMath::Lerp(S.Tropism, -2.5f, S.TwigHang);

	int32 Node = 0;
	for (float Z = S.TrunkNodeSpacing * 0.5f; Z < Leader * 0.97f; Z += S.TrunkNodeSpacing * (0.7f + 0.6f * Hash01(Seed, 1, Node, 9)), Node++)
	{
		for (int32 Whorl = 0; Whorl < FMath::Max(S.WhorlSize, 1); Whorl++)
		{
			const int32 Branch = Node * FMath::Max(S.WhorlSize, 1) + Whorl;
			const uint32 Key1 = 1000 + Branch;
			FBranch B;
			B.Parent = 0;
			B.Order = 1;
			B.BaseS = Z;
			// a node buds the season after the leader's tip passed it
			B.Birth = PlantModel::AgeAtHeight(S, Z) + 0.25f;
			// and is shed once the crown base climbs past it
			B.Death = Z / S.CrownBase < S.MaxHeight * 0.999f ? PlantModel::AgeAtHeight(S, Z / S.CrownBase) : Never;
			if (B.Birth == Never || B.Death < B.Birth + 0.5f)
			{
				continue;
			}
			B.MaxLength = S.Branch1MaxLength * Profile(S.Branch1Profile, Z / Leader) * (0.8f + 0.4f * Hash01(Seed, Key1, 1));
			B.Tau = S.LengthTau * (0.85f + 0.3f * Hash01(Seed, Key1, 2));
			B.Step = 0.35f;
			const FVector3f TrunkDir = Branches[0].DirAt(Z);
			const float AngleHere = S.Branch1AngleTop < 0.f ? S.Branch1Angle : FMath::Lerp(S.Branch1Angle, S.Branch1AngleTop, Z / Leader);
			const float Angle = FMath::DegreesToRadians(AngleHere * (0.85f + 0.3f * Hash01(Seed, Key1, 3)));
			const FVector3f Dir0 = TrunkDir * FMath::Cos(Angle) + Perp(TrunkDir, Node * GoldenAngle + Whorl * (2.f * PI / S.WhorlSize) + Hash01(Seed, Key1, 4) * 0.4f) * FMath::Sin(Angle);
			B.Points = GrowCurve(Branches[0].At(Z), Dir0, B.MaxLength + B.Step, B.Step, Shoot1, Seed, Key1);
			B.MaxLength = FMath::Min(B.MaxLength, EnvelopeLength(S, B.Points, B.Step));
			if (B.MaxLength < 0.3f)
			{
				continue;
			}
			const int32 Index1 = Branches.Add(MoveTemp(B));

			int32 Twig = 0;
			// small plants (dwarf birch) start their twigs close to the limb's base
			const float TwigStart = FMath::Min(0.45f, S.Branch2Spacing * 1.5f), TwigEnd = FMath::Min(0.35f, 0.25f * Branches[Index1].MaxLength);
			for (float S2 = TwigStart; S2 < Branches[Index1].MaxLength - TwigEnd; S2 += S.Branch2Spacing * (0.7f + 0.6f * Hash01(Seed, Key1, Twig, 5)), Twig++)
			{
				const FBranch& P = Branches[Index1];
				const uint32 Key2 = 100000 + Branch * 256 + Twig;
				FBranch C;
				C.Parent = Index1;
				C.Order = 2;
				C.BaseS = S2;
				C.Birth = P.Birth + P.TimeAtLength(S2) + 0.3f;
				// shaded out once the parent's tip has grown ShadeDepth past it
				const float Shade = P.TimeAtLength(S2 + S.Branch2ShadeDepth);
				C.Death = FMath::Min(P.Death, Shade == Never ? Never : P.Birth + Shade);
				C.MaxLength = FMath::Min(S.Branch2MaxLength, S.Branch2LengthRatio * (P.MaxLength - S2)) * (0.7f + 0.6f * Hash01(Seed, Key2, 1));
				if (P.Birth + P.TimeAtLength(S2) >= Never * 0.5f || C.Death < C.Birth + 0.3f || C.MaxLength < 0.12f)
				{
					continue;
				}
				C.Tau = P.Tau * 0.45f;
				C.Step = S.TwigHang > 0.5f ? 0.4f : 0.25f;
				const FVector3f PDir = P.DirAt(S2);
				const float Angle2 = FMath::DegreesToRadians(S.Branch2Angle * (0.8f + 0.4f * Hash01(Seed, Key2, 2)));
				const FVector3f Dir2 = PDir * FMath::Cos(Angle2) + Perp(PDir, Twig * GoldenAngle + Branch + Hash01(Seed, Key2, 3)) * FMath::Sin(Angle2);
				C.Points = GrowCurve(P.At(S2), Dir2, C.MaxLength + C.Step, C.Step, Shoot2, Seed, Key2);
				C.MaxLength = FMath::Min(C.MaxLength, EnvelopeLength(S, C.Points, C.Step));
				if (C.MaxLength < 0.12f)
				{
					continue;
				}
				Branches.Add(MoveTemp(C));
			}
		}
	}

	// spurs: short shoots along the older wood of the limbs and their laterals
	if (S.SpurSpacing > 0.f)
	{
		FShoot SpurShoot;
		SpurShoot.Tropism = 0.6f;
		SpurShoot.Wobble = S.Wobble;
		const int32 NumBase = Branches.Num();
		for (int32 Pi = 0; Pi < NumBase; Pi++)
		{
			if (Branches[Pi].Order != 1 && Branches[Pi].Order != 2) continue;
			int32 Sp = 0;
			for (float S3 = S.SpurSpacing * 0.5f; S3 < Branches[Pi].MaxLength - 0.1f; S3 += S.SpurSpacing * (0.6f + 0.8f * Hash01(Seed, 7000 + Pi, Sp, 1)), Sp++)
			{
				const FBranch& P = Branches[Pi];
				const float T = P.TimeAtLength(S3);
				if (T >= Never * 0.5f) break;
				FBranch C;
				C.Parent = Pi;
				C.Order = 3;
				C.BaseS = S3;
				C.Birth = P.Birth + T + S.SpurAge;
				C.Death = P.Death;
				if (C.Death < C.Birth + 0.5f) continue;
				C.MaxLength = S.SpurLength * (0.6f + 0.8f * Hash01(Seed, 7000 + Pi, Sp, 2));
				C.Tau = 1.5f;
				C.Step = FMath::Max(C.MaxLength * 0.5f, 0.03f);
				const FVector3f PDir = P.DirAt(S3);
				const float A = FMath::DegreesToRadians(60.f + 25.f * Hash01(Seed, 7000 + Pi, Sp, 3));
				const FVector3f Dir = PDir * FMath::Cos(A) + Perp(PDir, Sp * GoldenAngle + Hash01(Seed, 7000 + Pi, Sp, 4)) * FMath::Sin(A);
				C.Points = GrowCurve(P.At(S3), Dir, C.MaxLength + C.Step, C.Step, SpurShoot, Seed, 900000 + Pi * 512 + Sp);
				Branches.Add(MoveTemp(C));
			}
		}
	}
}

float FPlantSkeleton::LengthAt(const FBranch& B, float Age) const
{
	if (Age < B.Birth || Age >= B.Death)
	{
		return -1.f;
	}
	if (B.Order == 0)
	{
		return FMath::Min(PlantModel::Height(S, Age), (B.Points.Num() - 1) * B.Step);
	}
	return B.MaxLength * (1.f - FMath::Exp(-(Age - B.Birth) / B.Tau));
}

int32 FPlantSkeleton::FInstances::NumRings() const
{
	int32 N = 0;
	for (const FTube& T : Tubes) N += T.P.Num();
	return N;
}

void FPlantSkeleton::Evaluate(float Age, const FPlantSeason& Season, FInstances& Out, const FPlantSeason* Prev) const
{
	Out.Tubes.Reset();
	for (TArray<FTransform>& L : Out.Leaves) L.Reset();
	for (TArray<float>& L : Out.LeafSway) L.Reset();
	Out.Fruit.Reset();
	Out.FruitIds.Reset();
	Out.FallenFruit.Reset();
	Out.FruitData.Reset();
	Out.Litter.Reset();
	Out.Dropped.Reset();
	Out.Height = Out.Reach = 0.f;
	Out.LiveBranches = 0;
	using PlantModel::Hash01;

	const int32 N = Branches.Num();
	TArray<float> Len, Rad;
	Len.Init(-1.f, N);
	Rad.Init(0.f, N);
	const float Tip = S.MinRadius * 0.3f;
	// a trunk that has stopped at its fork (LeaderFraction) stays thick to the top: its limbs carry on from there
	const float ForkedTrunk = S.LeaderFraction < 1.f && Branches[0].Points.Num() > 1 && PlantModel::Height(S, Age) >= 0.95f * S.MaxHeight * S.LeaderFraction ? 0.6f : 1.f;
	auto RadiusAt = [&](int32 I, float L) { return Tip + (Rad[I] - Tip) * FMath::Pow(FMath::Max(0.f, 1.f - (I == 0 ? ForkedTrunk : 1.f) * L / Len[I]), 0.8f); };
	const float H = PlantModel::Height(S, Age);
	if (PlantModel::IsGone(S, Seed, Age))
	{
		return;
	}
	// an old tree's crown thins as it declines: twigs die back, then the leaves go; dead, it stands bare
	const float Vigour = PlantModel::Vigour(S, Seed, Age);
	const float LeafScale = Vigour > 0.f ? Season.LeafScale * FMath::Clamp(H / 4.f, 0.5f, 1.f) : 0.f;
	const bool bFruit = Season.FruitScale > 0.f && Age >= S.FruitingAge && Vigour > 0.5f;

	TArray<int32> TubeOf;
	TubeOf.Init(INDEX_NONE, N);
	for (int32 I = 0; I < N; I++)
	{
		const FBranch& B = Branches[I];
		const float L = LengthAt(B, Age);
		// a limb the trunk has shed leaves its scar (birch: a black "eye" on the white bark)
		if (S.Bark.Scars > 0.f && B.Order == 1 && Age >= B.Death && TubeOf[B.Parent] != INDEX_NONE)
		{
			const float Was = LengthAt(B, B.Death - 0.01f);
			Out.Tubes[TubeOf[B.Parent]].Junctions.Add({ B.BaseS, B.DirAt(0.f), 0.8f * (S.BranchRadiusBase + S.BranchRadiusPerLength * FMath::Max(Was, 0.f)), true });
		}
		if (L < 0.005f || (B.Parent != INDEX_NONE && Len[B.Parent] < B.BaseS) || (B.Order >= 2 && Hash01(Seed, I, 0, 51) > 0.3f + 0.7f * Vigour))
		{
			continue;
		}
		Len[I] = L;
		// thicker with height and, every year, by one more ring
		Rad[I] = B.Order == 0 ? FMath::Max(S.MinRadius, S.TrunkRadiusPerHeight * H + S.RingWidth * Age)
		                      : FMath::Min(((S.BranchRadiusBase + S.BranchRadiusPerLength * L) + (B.Order <= 1 ? S.RingWidth * 0.6f * (Age - B.Birth) : 0.f)) * (B.Order == 2 ? S.TwigRadiusScale : 1.f), 0.75f * RadiusAt(B.Parent, B.BaseS));
		Out.LiveBranches++;

		TubeOf[I] = Out.Tubes.Num();
		FTube& T = Out.Tubes.AddDefaulted_GetRef();
		T.Order = B.Order;
		T.Key = I;
		if (B.Parent != INDEX_NONE && TubeOf[B.Parent] != INDEX_NONE)
		{
			Out.Tubes[TubeOf[B.Parent]].Junctions.Add({ B.BaseS, B.DirAt(0.f), FMath::Min(Rad[I], 0.75f * RadiusAt(B.Parent, B.BaseS)), false });
		}
		for (float S0 = 0.f;; S0 += B.Step)
		{
			const float At = FMath::Min(S0, L);
			const FVector3f P = B.At(At);
			float R = RadiusAt(I, At);
			if (B.Order == 0)
			{
				R *= 1.f + 0.5f * FMath::Exp(-P.Z / 0.35f); // root flare
			}
			T.P.Add(P);
			T.R.Add(R);
			Out.Height = FMath::Max(Out.Height, P.Z);
			Out.Reach = FMath::Max(Out.Reach, FVector2f(P.X, P.Y).Size());
			if (At >= L) break;
		}

		// fruit grows on the spurs where a species has them (apple), else on the twigs
		const int32 FruitOrder = S.SpurSpacing > 0.f ? 3 : 2;
		if (bFruit && B.Order == FruitOrder && L > (FruitOrder == 3 ? 0.02f : 0.2f) && Hash01(Seed, I, 0, 21) < S.FruitChance)
		{
			// berries and cones hang for good and are simply gone when their time is up; apples and acorns fall
			const bool bFalls = S.FruitCluster <= 1 && S.FruitFallDay <= 366.f;
			for (int32 Fr = 0; Fr < FMath::Max(S.FruitCluster, 1); Fr++)
			{
				const int32 Id = Fr == 0 ? I : I + 20000 * Fr;
				// hanging under the twig, two thirds of the way out (a spur's tip), until its own day to drop; a cluster is spread along the twig
				const float Along = FruitOrder == 3 ? L : (S.FruitCluster > 1 ? L * (0.25f + 0.75f * (Fr + Hash01(Seed, Id, 0, 25)) / S.FruitCluster) : L * 0.66f);
				const FVector3f P = B.At(Along) - FVector3f(0.f, 0.f, S.FruitSize * 0.6f);
				// its skin: how much it blushes and on which side, how striped, its tint, how early it ripens
				const float Data[FInstances::FruitDataFloats] = {
					S.FruitBlush * (0.25f + 0.75f * Hash01(Seed, Id, 0, 41)), Hash01(Seed, Id, 0, 42),
					S.FruitStripes * Hash01(Seed, Id, 0, 43), Hash01(Seed, Id, 0, 44), (Hash01(Seed, Id, 0, 45) - 0.5f) * 0.3f };
				const float FallDay = S.FruitFallDay - 35.f * Hash01(Seed, Id, 0, 22);
				const float Size = S.FruitSize * 100.f * Season.FruitScale * (0.85f + 0.3f * Hash01(Seed, Id, 0, 23));
				const FTransform Hung(FRotator(0.f, Hash01(Seed, Id, 0, 24) * 360.f, 0.f).Quaternion(), FVector(P) * 100.0, FVector(Size, Size, Size * S.FruitAspect) / 100.f);
				if (Season.Day < FallDay || !bFalls)
				{
					Out.Fruit.Add(Hung);
					Out.FruitIds.Add(Id);
					Out.FruitData.Append(Data, FInstances::FruitDataFloats);
				}
				else if (Season.Day < FallDay + S.FruitRotDays)
				{
					FInstances::FFallen& F = Out.FallenFruit.AddDefaulted_GetRef();
					F.Id = Id;
					F.At = P;
					F.Hung = Hung;
					FMemory::Memcpy(F.Data, Data, sizeof(Data));
				}
			}
		}

		if (Season.LeafScale <= 0.f && Season.LitterFrom[0] >= 1.f) continue;
		// leaves sit at fixed places along the shoot, so they never slide as it grows; a spur has one rosette at its tip
		const float From = B.Order >= 2 ? 0.08f : FMath::Max(B.Order == 0 ? S.CrownBase * L + 0.1f : 0.3f, L - S.LeafyTipLength);
		const int32 K0 = B.Order == 3 ? 0 : FMath::CeilToInt(From / S.LeafSpacing);
		const int32 K1 = B.Order == 3 ? 0 : FMath::FloorToInt(L / S.LeafSpacing);
		for (int32 K = K0; K <= K1; K++)
		{
			const float At = B.Order == 3 ? L : FMath::Min(L, (K + 0.5f * Hash01(Seed, I, K, 3)) * S.LeafSpacing);
			const int32 Bucket = FMath::Min((int32)(Hash01(Seed, I, K, 2) * FPlantSeason::NumBuckets), FPlantSeason::NumBuckets - 1);
			const float H1 = Hash01(Seed, I, K, 1);
			if (H1 < Season.Fallen[Bucket])
			{
				// on the ground under the tree, carried a little downwind, until it rots
				const bool bLitter = H1 >= Season.LitterFrom[Bucket];
				const bool bDropped = Prev && H1 >= Prev->Fallen[Bucket];
				if (bLitter || bDropped)
				{
					const FVector3f Hang = B.At(At);
					FInstances::FLitter Lt;
					Lt.At = FVector2f(Hang.X, Hang.Y) + FVector2f(Hash01(Seed, I, K, 14) - 0.5f + 0.6f, Hash01(Seed, I, K, 15) - 0.5f + 0.2f) * 2.5f;
					Lt.Yaw = Hash01(Seed, I, K, 12) * 360.f;
					Lt.Size = 0.85f + 0.3f * Hash01(Seed, I, K, 7);
					Lt.Bucket = Bucket;
					if (bDropped)
					{
						Out.Dropped.Add({ FTransform(FRotator(0.f, Lt.Yaw, 0.f), FVector(Hang) * 100.0, FVector(Lt.Size)), Lt });
					}
					else
					{
						Out.Litter.Add(Lt);
					}
				}
				continue;
			}
			if (LeafScale <= 0.f || Hash01(Seed, I, K, 52) > Vigour) continue;
			const FVector3f Off(Hash01(Seed, I, K, 4) - 0.5f, Hash01(Seed, I, K, 5) - 0.5f, Hash01(Seed, I, K, 6) - 0.5f);
			// the newest leaves at a shoot's tip are still small
			const float Size = LeafScale * FMath::Clamp(0.45f + (L - At) / 0.4f, 0.45f, 1.f) * (0.85f + 0.3f * Hash01(Seed, I, K, 7));
			// blades face the sky, give or take
			const FRotator Rot((Hash01(Seed, I, K, 11) - 0.5f) * 50.f, Hash01(Seed, I, K, 12) * 360.f, (Hash01(Seed, I, K, 13) - 0.5f) * 50.f);
			Out.Leaves[Bucket].Emplace(Rot.Quaternion(), FVector(B.At(At) + Off * (S.LeafClusterSize * 0.4f)) * 100.0, FVector(Size));
			Out.LeafSway[Bucket].Add(B.Order >= 2 ? FMath::Pow(At / L, 1.3f) : 0.f);
		}
	}
}
