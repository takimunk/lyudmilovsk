#include "ConveyorBelt.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AConveyorBelt::AConveyorBelt()
{
	PrimaryActorTick.bCanEverTick = true;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(80.f, 3900.f, 60.f));
	Zone->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	RootComponent = Zone;

	Motor = CreateDefaultSubobject<UAudioComponent>(TEXT("Motor"));
	Motor->SetupAttachment(Zone);
	Motor->bAutoActivate = false;
}

void AConveyorBelt::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> Belts = {BeltActor.Get()};
	for (AActor* Extra : ExtraBelts)
	{
		Belts.Add(Extra);
	}
	for (AActor* Belt : Belts)
	{
		UStaticMeshComponent* Mesh = Belt ? Belt->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (Mesh)
		{
			BeltMaterials.Add(Mesh->CreateAndSetMaterialInstanceDynamic(0));
		}
	}
	if (MotorSound)
	{
		Motor->SetSound(MotorSound);
		Motor->SetVolumeMultiplier(0.f);
		Motor->Play();
	}
}

void AConveyorBelt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<AActor*> Overlapping;
	Zone->GetOverlappingActors(Overlapping, ACharacter::StaticClass());

	TArray<ACharacter*> Riders;
	for (AActor* Actor : Overlapping)
	{
		ACharacter* Character = Cast<ACharacter>(Actor);
		if (Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround())
		{
			Riders.Add(Character);
		}
	}

	const bool bRiding = Riders.Num() > 0;
	Speed = FMath::FInterpConstantTo(Speed, bRiding ? MaxSpeed : 0.f, DeltaSeconds, bRiding ? Acceleration : Deceleration);

	if (Speed > KINDA_SMALL_NUMBER)
	{
		const FVector Step = Direction.GetSafeNormal() * Speed * DeltaSeconds;
		for (ACharacter* Rider : Riders)
		{
			Rider->AddActorWorldOffset(Step, true);
		}
	}

	Offset = FMath::Fmod(Offset + Speed * DeltaSeconds / TreadSpacing, 1.f);
	for (UMaterialInstanceDynamic* Material : BeltMaterials)
	{
		Material->SetScalarParameterValue(TEXT("Offset"), Offset);
	}
	if (Motor->IsPlaying())
	{
		Motor->SetVolumeMultiplier(0.6f * Speed / MaxSpeed);
		Motor->SetPitchMultiplier(0.8f + 0.6f * Speed / MaxSpeed);
	}
}
