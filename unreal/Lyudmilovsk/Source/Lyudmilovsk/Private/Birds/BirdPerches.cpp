#include "BirdPerches.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantModel.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sea/LyudSea.h"

namespace
{
	constexpr float Cell = 2000.f;
	FIntPoint CellOf(const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
}

void FBirdPerches::BuildCrowns(UWorld* World)
{
	bBuilt = true;
	Grid.Reset();
	const float Day = UGameCalendar::NowIn(World).GetDayOfYear();
	TMap<TPair<FName, int32>, FPlantSummary> Cache;
	auto Add = [&](const FVector& Base, float Scale, const FPlantSummary& S)
	{
		if (S.Height < 2.f || S.CrownRadius < 0.5f)
		{
			return;
		}
		const float Half = FMath::Max(0.5f, (S.Height - S.CrownBaseHeight) * 0.5f) * Scale * 100.f;
		FCrown C;
		C.Centre = FVector3f(Base + FVector(0, 0, S.Height * Scale * 100.f - Half));
		C.Radii = FVector2f(S.CrownRadius * Scale * 100.f, Half);
		Grid.FindOrAdd(CellOf(Base)).Add(C);
	};
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		It->ForEachTree([&](FName Species, const FTransform& Xf, float Age)
		{
			const FPlantSpecies* Sp = PlantSpeciesLibrary::Find(Species);
			if (!Sp) return;
			const TPair<FName, int32> Key(Species, FMath::RoundToInt(Age));
			const FPlantSummary* S = Cache.Find(Key);
			if (!S) S = &Cache.Add(Key, PlantModel::Summary(*Sp, 1, Age, Day));
			Add(Xf.GetLocation(), (float)Xf.GetScale3D().Z, *S);
		});
	}
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		Add(It->GetActorLocation(), (float)It->GetActorScale3D().Z, It->GetSummary());
	}
}

bool FBirdPerches::Find(UWorld* World, const FVector& From, const FVector& Dir, float MaxDist, const AActor* Ignore, FPerch& Out)
{
	if (!World)
	{
		return false;
	}
	if (!bBuilt)
	{
		BuildCrowns(World);
	}
	const FVector Look = Dir.GetSafeNormal();
	float Best = 1e20f;
	// how good a perch is: near, ahead of the bird, and above the ground (a twig or a roof beats the grass)
	auto Score = [&](const FVector& P, bool bHigh)
	{
		const FVector To = P - From;
		const float D = To.Size();
		if (D > MaxDist || D < 30.f) return 1e20f;
		const float Ahead = FVector::DotProduct(To / D, Look);
		if (Ahead < 0.2f) return 1e20f;
		return D * (1.8f - Ahead) + (bHigh ? 0.f : 1500.f);
	};

	// the crowns: a twig on the side facing the bird, in the upper half, or the very top when the bird is above it
	const FIntPoint C0 = CellOf(From);
	const int32 R = FMath::CeilToInt(MaxDist / Cell);
	for (int32 Y = C0.Y - R; Y <= C0.Y + R; Y++)
	{
		for (int32 X = C0.X - R; X <= C0.X + R; X++)
		{
			const TArray<FCrown>* List = Grid.Find(FIntPoint(X, Y));
			if (!List) continue;
			for (const FCrown& C : *List)
			{
				const FVector Centre(C.Centre);
				FVector D = From - Centre;
				D.Z = FMath::Max(D.Z / FMath::Max(C.Radii.Y, 1.f), 0.35f);
				const FVector2D H(D.X, D.Y);
				const FVector2D Hn = H.SizeSquared() > 1.f ? H.GetSafeNormal() : FVector2D(1, 0);
				// on the outside of the crown, in its upper part (small bushes and hedges: right on top, never in the leaves)
				const bool bBush = C.Radii.Y < 200.f;
				const float Up = bBush ? 1.f : FMath::Clamp(D.Z, 0.6f, 1.f);
				const float Side = FMath::Sqrt(FMath::Max(0.f, 1.f - Up * Up));
				const FVector P = Centre + FVector(Hn.X * Side * C.Radii.X, Hn.Y * Side * C.Radii.X, Up * C.Radii.Y + 8.f);
				const float S = Score(P, true);
				if (S < Best)
				{
					Best = S;
					Out.Point = P;
					Out.bBranch = true;
				}
			}
		}
	}

	// roofs, walls, rails, the ground: traces down from a fan of points ahead
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdPerch), false, Ignore);
	if (ALyudSea* Sea = ALyudSea::Get(World))
	{
		Q.AddIgnoredActor(Sea);
		if (AActor* Vol = Sea->GetWaterVolume()) Q.AddIgnoredActor(Vol);
	}
	const FVector Fwd = FVector(Look.X, Look.Y, 0.f).GetSafeNormal();
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	for (int32 I = 1; I <= 5; I++)
	{
		for (int32 J = -3; J <= 3; J++)
		{
			const FVector Top = From + Fwd * (MaxDist * I / 6.f) + Right * (MaxDist * J / 9.f) + FVector(0, 0, 800.f);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 6000.f), ECC_Visibility, Q) && Hit.ImpactNormal.Z > 0.75f)
			{
				// "high": well above the ground just beside it (a roof, a wall top)
				FHitResult Low;
				const FVector Beside = Hit.ImpactPoint + Right * 250.f;
				const bool bHigh = World->LineTraceSingleByChannel(Low, Beside + FVector(0, 0, 50.f), Beside - FVector(0, 0, 3000.f), ECC_Visibility, Q)
					&& Hit.ImpactPoint.Z - Low.ImpactPoint.Z > 150.f;
				const float S = Score(Hit.ImpactPoint, bHigh);
				if (S < Best)
				{
					Best = S;
					Out.Point = Hit.ImpactPoint;
					Out.bBranch = false;
				}
			}
		}
	}
	return Best < 1e19f;
}
