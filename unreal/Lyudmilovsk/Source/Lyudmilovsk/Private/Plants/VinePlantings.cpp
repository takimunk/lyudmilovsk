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
	};

	// Novikova 1's courtyard face runs north-south at x ~ -7 m (the gates' doors at y -21.2, -0.4, 20.5 m are
	// left 5 m clear); the vines stand at its end bays and will go round the corners onto the ends
	const FVinePlanting Plantings[] = {
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(-20.0, -27.3, 1.5), FVector(1, 0, 0), TEXT("VirginiaCreeper"), 11, 16.f },
		{ TEXT("SM_Novikova1"), TEXT("Academ"), FVector(-20.0, 27.0, 1.5), FVector(1, 0, 0), TEXT("VirginiaCreeper"), 12, 12.f },
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
		Vine->Tags.Add(P.Tag);
		Vine->FinishSpawning(FTransform(N.Rotation(), At));
		Vines.Add(Vine);
	}
}
