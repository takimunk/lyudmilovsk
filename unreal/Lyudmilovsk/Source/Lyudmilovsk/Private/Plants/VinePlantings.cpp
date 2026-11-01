#include "Plants/VinePlantings.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Plants/ProceduralVine.h"

DEFINE_LOG_CATEGORY_STATIC(LogVinePlantings, Log, All);

namespace
{
	struct FVinePlanting
	{
		/** The house's static mesh, and the tag its parts carry. */
		const TCHAR* Mesh;
		const TCHAR* Tag;
		/**
		 * A point outside the wall, in the house mesh's frame (m: the Academgorodok's x east, y north, z up),
		 * and the direction to look for the wall from it.
		 */
		FVector From;
		FVector Toward;
		FName Species;
		int32 Seed;
		float Age;
		/** Most steps it may grow (0: AProceduralVine's default). */
		int32 MaxNodes = 0;
	};

	// Novikova 1's courtyard face runs north-south at x ~ -7 m (the gates' doors at y -21.2, -0.4, 20.5 m are
	// left 5 m clear); the vines stand at its end bays and will go round the corners onto the ends
	const FVinePlanting Plantings[] = {
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(-20.0, -27.3, 1.5), FVector(1, 0, 0), TEXT("VirginiaCreeper"), 11, 16.f },
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(-20.0, 27.0, 1.5), FVector(1, 0, 0), TEXT("VirginiaCreeper"), 12, 12.f },
		// The south gate (between Maksimova 8's corner at (9.1, -45.3) and Novikova 1's at (8.8, -29.9), the way in from
		// the lower station): Boston ivy over both arches, on the street (east) and the yard (west) faces. Each gate block
		// is 3 x 1.1 x 5 m (SM_Gates, in the ensemble frame); one plant stands at the foot of the pier next to the house
		// and climbs it, over the arch and round onto the house wall
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(12.0, -44.96, 1.5), FVector(-1, 0, 0), TEXT("BostonIvy"), 21, 11.f, 5000 },
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(6.0, -44.96, 1.5), FVector(1, 0, 0), TEXT("BostonIvy"), 22, 13.f, 5000 },
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(12.0, -30.37, 1.5), FVector(-1, 0, 0), TEXT("BostonIvy"), 23, 12.f, 5000 },
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(5.5, -30.45, 1.5), FVector(1, 0, 0), TEXT("BostonIvy"), 24, 14.f, 5000 },
		// and one on each house wall beside the gate, 2.4 m from it (on Novikova 1's south end, west of its side door; Maksimova 8
		// is placed by SM_Gates, whose frame is the ensemble's, as its own mesh is in its own frame)
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(5.3, -44.7, 1.5), FVector(0.667, -0.745, 0), TEXT("BostonIvy"), 25, 13.f, 6000 },
		{ TEXT("SM_Gates"), TEXT("Academ"), FVector(12.93, -45.13, 1.5), FVector(-0.745, -0.667, 0), TEXT("BostonIvy"), 26, 12.f, 6000 },
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(11.7, -27.37, 1.5), FVector(-0.9986, -0.0525, 0), TEXT("BostonIvy"), 27, 14.f, 6000 },
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(3.66, -33.2, 1.5), FVector(-0.0525, 0.9986, 0), TEXT("BostonIvy"), 28, 12.f, 6000 },
	};
}

bool UVinePlantings::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UVinePlantings::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (const FVinePlanting& P : Plantings)
	{
		AStaticMeshActor* House = nullptr;
		for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
		{
			const UStaticMesh* Mesh = It->GetStaticMeshComponent() ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (Mesh && Mesh->GetName() == P.Mesh)
			{
				House = *It;
				break;
			}
		}
		if (!House)
		{
			continue;
		}
		// the house mesh's frame: x east -> +X, y north -> -Y (Blender's handedness), metres -> cm
		const FTransform Xf = House->GetActorTransform();
		auto ToWorld = [&](const FVector& M) { return Xf.TransformPosition(FVector(M.X * 100.0, -M.Y * 100.0, M.Z * 100.0)); };
		const FVector From = ToWorld(P.From);
		const FVector To = ToWorld(P.From + P.Toward * 25.0);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(VinePlant), true);
		if (!InWorld.LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			UE_LOG(LogVinePlantings, Warning, TEXT("No wall for the vine at %s of %s"), *P.From.ToString(), P.Mesh);
			continue;
		}
		UE_LOG(LogVinePlantings, Log, TEXT("%s vine at %s of %s: wall of %s at %s"), *P.Species.ToString(), *P.From.ToString(), P.Mesh,
			Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"), *Hit.ImpactPoint.ToCompactString());
		const FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.0).GetSafeNormal();
		FVector At = Hit.ImpactPoint + N * 5.0;
		FHitResult Ground;
		if (InWorld.LineTraceSingleByObjectType(Ground, At, At - FVector(0, 0, 600.0), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			At.Z = Ground.ImpactPoint.Z;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		AProceduralVine* Vine = InWorld.SpawnActor<AProceduralVine>(AProceduralVine::StaticClass(), FTransform(N.Rotation(), At), Params);
		if (!Vine)
		{
			continue;
		}
		Vine->Species = P.Species;
		Vine->Seed = P.Seed;
		Vine->AgeAtStart = P.Age;
		if (P.MaxNodes > 0)
		{
			Vine->MaxNodes = P.MaxNodes;
		}
		Vine->Tags.Add(P.Tag);
		Vine->FinishSpawning(FTransform(N.Rotation(), At));
		Vines.Add(Vine);
	}
}
