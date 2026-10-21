#include "SwingDoor.h"

#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ASwingDoor::ASwingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(120.f, 150.f, 110.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	RootComponent = Zone;
}

void ASwingDoor::BeginPlay()
{
	Super::BeginPlay();
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		AActor* Leaf = Leaves[i];
		const float Open = OpenAngles.IsValidIndex(i) ? OpenAngles[i] : 90.f;
		FRotator Closed = Leaf ? Leaf->GetActorRotation() : FRotator::ZeroRotator;
		if (bLeavesPlacedOpen)
		{
			Closed.Yaw -= Open;
		}
		ClosedRotations.Add(Closed);
		if (Leaf)
		{
			if (UStaticMeshComponent* Mesh = Leaf->FindComponentByClass<UStaticMeshComponent>())
			{
				Mesh->SetMobility(EComponentMobility::Movable);
			}
		}
	}
	bOpen = bStartOpen;
	Openness = bOpen ? 1.f : 0.f;
	Apply();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PC);
		if (InputComponent)
		{
			// several actors listen for E (doors, the hero's seat); none may swallow it
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ASwingDoor::OnInteract).bConsumeInput = false;
		}
	}
}

void ASwingDoor::Apply()
{
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		if (Leaves[i] && ClosedRotations.IsValidIndex(i))
		{
			const float Open = OpenAngles.IsValidIndex(i) ? OpenAngles[i] : 90.f;
			Leaves[i]->SetActorRotation(ClosedRotations[i] + FRotator(0.f, Open * Openness, 0.f));
		}
	}
}

void ASwingDoor::OnInteract()
{
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Pawn && Zone->IsOverlappingActor(Pawn))
	{
		Toggle();
	}
}

void ASwingDoor::OnGameLoaded_Implementation()
{
	Openness = bOpen ? 1.f : 0.f;
	Apply();
}

void ASwingDoor::Toggle()
{
	bOpen = !bOpen;
}

void ASwingDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Openness, Target))
	{
		Openness = FMath::FInterpConstantTo(Openness, Target, DeltaSeconds, SwingSpeed / 90.f);
		Apply();
	}
}
