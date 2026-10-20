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
	if (Leaf)
	{
		ClosedRotation = Leaf->GetActorRotation();
		if (UStaticMeshComponent* Mesh = Leaf->FindComponentByClass<UStaticMeshComponent>())
		{
			Mesh->SetMobility(EComponentMobility::Movable);
		}
	}
	bOpen = bStartOpen;
	Angle = bOpen ? OpenAngle : 0.f;
	if (Leaf)
	{
		Leaf->SetActorRotation(ClosedRotation + FRotator(0.f, Angle, 0.f));
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ASwingDoor::OnInteract);
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

void ASwingDoor::Toggle()
{
	bOpen = !bOpen;
}

void ASwingDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? OpenAngle : 0.f;
	if (Leaf && !FMath::IsNearlyEqual(Angle, Target))
	{
		Angle = FMath::FInterpConstantTo(Angle, Target, DeltaSeconds, SwingSpeed);
		Leaf->SetActorRotation(ClosedRotation + FRotator(0.f, Angle, 0.f));
	}
}
