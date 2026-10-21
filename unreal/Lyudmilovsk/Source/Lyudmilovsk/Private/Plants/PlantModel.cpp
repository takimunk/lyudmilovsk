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

	/** A shoot's curve: straight from Dir0, pulled up by Tropism and bent at random by Wobble. */
	TArray<FVector3f> GrowCurve(const FVector3f& Start, FVector3f Dir, float Length, float Step, float Tropism, float Wobble, uint32 Seed, uint32 Key)
	{
		TArray<FVector3f> P;
		const int32 N = FMath::CeilToInt(Length / Step) + 1;
		P.Reserve(N + 1);
		P.Add(Start);
		for (int32 I = 0; I < N; I++)
		{
			P.Add(P.Last() + Dir * Step);
			const FVector3f R(PlantModel::Hash01(Seed, Key, I, 1) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 2) * 2.f - 1.f, PlantModel::Hash01(Seed, Key, I, 3) * 2.f - 1.f);
			const FVector3f Side = R - Dir * FVector3f::DotProduct(R, Dir);
			Dir = (Dir + Up * (Tropism * Step) + Side * (Wobble * Step)).GetSafeNormal(UE_SMALL_NUMBER, Up);
		}
		return P;
	}

}

float PlantModel::Hash01(uint32 Seed, uint32 A, uint32 B, uint32 C)
{
	const uint64 H = SplitMix(SplitMix(SplitMix(((uint64)Seed << 32) ^ A) ^ B) ^ C);
	return (float)(H >> 40) * (1.f / 16777216.f);
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
	Out.LeafScale = Unfold > 0.f ? 0.25f + 0.75f * Unfold : 0.f;
	const FLinearColor Base = FMath::Lerp(S.SummerLeafColour, S.SpringLeafColour, 1.f - Smooth(S.FullLeafDay, S.FullLeafDay + 35.f, D));
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		const float Off = (K - 1) * 6.f; // bucket 0 turns and falls first
		const FLinearColor Autumn = FMath::Lerp(S.AutumnLeafColour, S.AutumnLeafColourLate, K / (FPlantSeason::NumBuckets - 1.f));
		Out.Colour[K] = FMath::Lerp(Base, Autumn, Smooth(S.ColourStartDay + Off, S.ColourFullDay + Off, D));
		Out.Fallen[K] = Unfold <= 0.f ? 1.f : Smooth(S.LeafDropStartDay + Off, S.BareDay + Off, D);
	}
	if (S.FruitSize > 0.f && D >= S.FruitSetDay && D < S.FruitFallDay)
	{
		Out.FruitScale = 0.3f + 0.7f * Smooth(S.FruitSetDay, S.FruitRipeDay, D);
		Out.FruitColour = FMath::Lerp(S.FruitUnripeColour, S.FruitColour, Smooth(S.FruitRipeDay - 40.f, S.FruitRipeDay, D));
		Out.FruitFallen = Smooth(S.FruitFallDay - 35.f, S.FruitFallDay, D);
	}
	// snow on the branches from late November to the March thaw (the calendar's climate, not the tree's)
	Out.Snow = FMath::Max(Smooth(325.f, 345.f, Day), 1.f - Smooth(65.f, 90.f, Day));
	return Out;
}

FPlantSummary PlantModel::Summary(const FPlantSpecies& S, uint32 Seed, float Age, float Day)
{
	FPlantSummary Out;
	// the crown of a tree whose trunk stops short (LeaderFraction) tops out a little above the trunk
	Out.Height = Height(S, Age) * (S.LeaderFraction + 0.4f * (1.f - S.LeaderFraction));
	Out.CrownBaseHeight = S.CrownBase * Out.Height;
	Out.TrunkRadius = FMath::Max(S.MinRadius, S.TrunkRadiusPerHeight * Out.Height);
	// the widest branches sit about a third of the way up the crown; their reach follows from
	// when the trunk passed them
	const float Trunk = Height(S, Age) * S.LeaderFraction;
	const float Z = FMath::Min(Out.CrownBaseHeight + 0.35f * (Out.Height - Out.CrownBaseHeight), Trunk * 0.9f);
	const float T = FMath::Max(0.f, Age - AgeAtHeight(S, Z / S.LeaderFraction));
	const float L = S.Branch1MaxLength * Profile(S.Branch1Profile, Z / (S.MaxHeight * S.LeaderFraction)) * (1.f - FMath::Exp(-T / S.LengthTau));
	Out.CrownRadius = L * FMath::Sin(FMath::DegreesToRadians(S.Branch1Angle) * 0.8f) + FMath::Min(L, 1.f) * S.Branch2MaxLength * 0.3f + S.LeafClusterSize * 0.5f;

	const FPlantSeason Sn = Season(S, Seed, Day);
	float On = 0.f;
	FLinearColor C = FLinearColor::Black;
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		On += 1.f - Sn.Fallen[K];
		C += Sn.Colour[K] * (1.f - Sn.Fallen[K]);
	}
	Out.LeafAmount = Sn.LeafScale * On / FPlantSeason::NumBuckets;
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
	Trunk.Points = GrowCurve(FVector3f::ZeroVector, Up, Leader * 1.02f, Trunk.Step, 0.5f, S.Wobble * 0.3f, Seed, 0);

	int32 Node = 0;
	for (float Z = S.TrunkNodeSpacing * 0.5f; Z < Leader * 0.97f; Z += S.TrunkNodeSpacing * (0.7f + 0.6f * Hash01(Seed, 1, Node, 9)), Node++)
	{
		const uint32 Key1 = 1000 + Node;
		FBranch B;
		B.Parent = 0;
		B.Order = 1;
		B.BaseS = Z;
		// a node buds the season after the leader's tip passed it
		B.Birth = PlantModel::AgeAtHeight(S, Z / S.LeaderFraction) + 0.25f;
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
		const float Angle = FMath::DegreesToRadians(S.Branch1Angle * (0.85f + 0.3f * Hash01(Seed, Key1, 3)));
		const FVector3f Dir0 = TrunkDir * FMath::Cos(Angle) + Perp(TrunkDir, Node * GoldenAngle + Hash01(Seed, Key1, 4) * 0.4f) * FMath::Sin(Angle);
		B.Points = GrowCurve(Branches[0].At(Z), Dir0, B.MaxLength + B.Step, B.Step, S.Tropism, S.Wobble, Seed, Key1);
		const int32 Index1 = Branches.Add(MoveTemp(B));

		int32 Twig = 0;
		for (float S2 = 0.45f; S2 < Branches[Index1].MaxLength - 0.35f; S2 += S.Branch2Spacing * (0.7f + 0.6f * Hash01(Seed, Key1, Twig, 5)), Twig++)
		{
			const FBranch& P = Branches[Index1];
			const uint32 Key2 = 100000 + Node * 256 + Twig;
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
			C.Step = 0.25f;
			const FVector3f PDir = P.DirAt(S2);
			const float Angle2 = FMath::DegreesToRadians(S.Branch2Angle * (0.8f + 0.4f * Hash01(Seed, Key2, 2)));
			const FVector3f Dir2 = PDir * FMath::Cos(Angle2) + Perp(PDir, Twig * GoldenAngle + Node + Hash01(Seed, Key2, 3)) * FMath::Sin(Angle2);
			C.Points = GrowCurve(P.At(S2), Dir2, C.MaxLength + C.Step, C.Step, S.Tropism, S.Wobble * 1.5f, Seed, Key2);
			Branches.Add(MoveTemp(C));
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
		return FMath::Min(PlantModel::Height(S, Age) * S.LeaderFraction, (B.Points.Num() - 1) * B.Step);
	}
	return B.MaxLength * (1.f - FMath::Exp(-(Age - B.Birth) / B.Tau));
}

int32 FPlantSkeleton::FInstances::NumRings() const
{
	int32 N = 0;
	for (const FTube& T : Tubes) N += T.P.Num();
	return N;
}

void FPlantSkeleton::Evaluate(float Age, const FPlantSeason& Season, FInstances& Out) const
{
	Out.Tubes.Reset();
	for (TArray<FTransform>& L : Out.Leaves) L.Reset();
	Out.Fruit.Reset();
	Out.Height = Out.Reach = 0.f;
	Out.LiveBranches = 0;
	using PlantModel::Hash01;

	const int32 N = Branches.Num();
	TArray<float> Len, Rad;
	Len.Init(-1.f, N);
	Rad.Init(0.f, N);
	const float Tip = S.MinRadius * 0.3f;
	auto RadiusAt = [&](int32 I, float L) { return Tip + (Rad[I] - Tip) * FMath::Pow(FMath::Max(0.f, 1.f - L / Len[I]), 0.8f); };
	const float H = PlantModel::Height(S, Age);
	const float LeafScale = Season.LeafScale * FMath::Clamp(H / 4.f, 0.5f, 1.f);
	const bool bFruit = Season.FruitScale > 0.f && Age >= S.FruitingAge;

	for (int32 I = 0; I < N; I++)
	{
		const FBranch& B = Branches[I];
		const float L = LengthAt(B, Age);
		if (L < 0.005f || (B.Parent != INDEX_NONE && Len[B.Parent] < B.BaseS))
		{
			continue;
		}
		Len[I] = L;
		Rad[I] = B.Order == 0 ? FMath::Max(S.MinRadius, S.TrunkRadiusPerHeight * H)
		                      : FMath::Min(S.BranchRadiusBase + S.BranchRadiusPerLength * L, 0.65f * RadiusAt(B.Parent, B.BaseS));
		Out.LiveBranches++;

		FTube& T = Out.Tubes.AddDefaulted_GetRef();
		T.Order = B.Order;
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

		if (bFruit && B.Order == 2 && L > 0.2f && Hash01(Seed, I, 0, 21) < S.FruitChance && Hash01(Seed, I, 0, 22) >= Season.FruitFallen)
		{
			// hanging under the twig, two thirds of the way out
			const FVector3f P = B.At(L * 0.66f) - FVector3f(0.f, 0.f, S.FruitSize * 0.6f);
			const float Size = S.FruitSize * 100.f * Season.FruitScale * (0.85f + 0.3f * Hash01(Seed, I, 0, 23));
			Out.Fruit.Emplace(FRotator(0.f, Hash01(Seed, I, 0, 24) * 360.f, 0.f).Quaternion(), FVector(P) * 100.0, FVector(Size, Size, Size * 0.92f) / 100.f);
		}

		if (LeafScale <= 0.f) continue;
		// leaves sit at fixed places along the shoot, so they never slide as it grows
		const float From = B.Order == 2 ? 0.08f : FMath::Max(B.Order == 0 ? S.CrownBase * L + 0.1f : 0.3f, L - S.LeafyTipLength);
		for (int32 K = FMath::CeilToInt(From / S.LeafSpacing); K * S.LeafSpacing <= L; K++)
		{
			const float At = FMath::Min(L, (K + 0.5f * Hash01(Seed, I, K, 3)) * S.LeafSpacing);
			const int32 Bucket = FMath::Min((int32)(Hash01(Seed, I, K, 2) * FPlantSeason::NumBuckets), FPlantSeason::NumBuckets - 1);
			if (Hash01(Seed, I, K, 1) < Season.Fallen[Bucket])
			{
				continue;
			}
			const FVector3f Off(Hash01(Seed, I, K, 4) - 0.5f, Hash01(Seed, I, K, 5) - 0.5f, Hash01(Seed, I, K, 6) - 0.5f);
			// the newest leaves at a shoot's tip are still small
			const float Size = LeafScale * FMath::Clamp(0.45f + (L - At) / 0.4f, 0.45f, 1.f) * (0.85f + 0.3f * Hash01(Seed, I, K, 7));
			// blades face the sky, give or take
			const FRotator Rot((Hash01(Seed, I, K, 11) - 0.5f) * 50.f, Hash01(Seed, I, K, 12) * 360.f, (Hash01(Seed, I, K, 13) - 0.5f) * 50.f);
			Out.Leaves[Bucket].Emplace(Rot.Quaternion(), FVector(B.At(At) + Off * (S.LeafClusterSize * 0.4f)) * 100.0, FVector(Size));
		}
	}
}
