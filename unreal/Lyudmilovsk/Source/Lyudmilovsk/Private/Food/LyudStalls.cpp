#include "Food/LyudStalls.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"

namespace
{
	TAutoConsoleVariable<int32> CVarStalls(TEXT("lyud.Food.Stalls"), 1, TEXT("Street food stalls (the cheese tent, the corn stall) at play start; 0 leaves them out."));
}

void ULyudStallSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarStalls.GetValueOnGameThread() == 0 || !TActorIterator<ALyudSky>(&InWorld))
	{
		return;
	}
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Food/stalls_built.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("stalls"), List))
	{
		UE_LOG(LogTemp, Warning, TEXT("Stalls: Content/Food/stalls_built.json unreadable"));
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *List)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(TEXT("pos_cm"), Pos) || Pos->Num() != 3)
		{
			continue;
		}
		const FString Name = O->GetStringField(TEXT("mesh"));
		const FString Path = FString::Printf(TEXT("/Game/Food/Stalls/%s.%s"), *Name, *Name);
		if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
		{
			UE_LOG(LogTemp, Warning, TEXT("Stalls: no %s (unreal/scripts/import_food.sh)"), *Path);
			continue;
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		FVector At((*Pos)[0]->AsNumber(), (*Pos)[1]->AsNumber(), (*Pos)[2]->AsNumber());
		// onto the ground as it is in the game (the walk grid's height is coarse)
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("StallGround"), false);
		if (InWorld.LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 400.f), At - FVector(0.f, 0.f, 600.f), ECC_WorldStatic, Q))
		{
			At.Z = Hit.ImpactPoint.Z;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Name = MakeUniqueObjectName(&InWorld, AStaticMeshActor::StaticClass(), FName(*(TEXT("Stall_") + O->GetStringField(TEXT("id")))));
		AStaticMeshActor* A = InWorld.SpawnActor<AStaticMeshActor>(At, FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f), Params);
		if (!A)
		{
			continue;
		}
		A->SetMobility(EComponentMobility::Static);
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetStaticMesh(Mesh);
		// the outdoor sun lights only channel 1 (import_world.py)
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		A->Tags.Add(TEXT("NoGrass"));
		A->Tags.Add(TEXT("NoTree"));
		Stalls.Add(A);
		UE_LOG(LogTemp, Display, TEXT("Stalls: %s at %s"), *Name, *At.ToString());
	}
}
