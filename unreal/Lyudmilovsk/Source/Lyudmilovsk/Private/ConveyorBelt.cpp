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

	// How the riders themselves walk along the belt (their own movement, not the belt's push).
	const FVector Axis = Direction.GetSafeNormal();
	float Walk = 0.f;
	for (ACharacter* Rider : Riders)
	{
		Walk += FVector::DotProduct(Rider->GetCharacterMovement()->Velocity, Axis);
	}
	Walk = Riders.Num() > 0 ? Walk / Riders.Num() : 0.f;

	if (FMath::Abs(Walk) > WalkThreshold)
	{
		// Speed up the way they walk, harder the faster they go; walking against it brakes and reverses it.
		const float Push = FMath::Clamp(Walk / RunSpeed, -1.f, 1.f);
		Speed = FMath::Clamp(Speed + Push * Acceleration * DeltaSeconds, -MaxSpeed, MaxSpeed);
	}
	else
	{
		// Nobody on it, or they stopped: ease to a halt.
		Speed = FMath::FInterpConstantTo(Speed, 0.f, DeltaSeconds, Deceleration);
	}

	if (FMath::Abs(Speed) > KINDA_SMALL_NUMBER)
	{
		const FVector Step = Axis * Speed * DeltaSeconds;
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
		const float Load = FMath::Abs(Speed) / MaxSpeed;
		Motor->SetVolumeMultiplier(0.6f * Load);
		Motor->SetPitchMultiplier(0.8f + 0.6f * Load);
	}
}
