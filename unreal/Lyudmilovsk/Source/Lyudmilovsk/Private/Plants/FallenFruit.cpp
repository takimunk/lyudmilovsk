#include "Plants/FallenFruit.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AFallenFruit::AFallenFruit()
{
	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// the player walks through apples rather than kicking them about
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCanEverAffectNavigation(false);
	Body->SetLinearDamping(0.6f);
	Body->SetAngularDamping(1.5f);
	Body->CanCharacterStepUpOn = ECB_No;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->LightingChannels.bChannel0 = true;
	Mesh->LightingChannels.bChannel1 = true;
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AFallenFruit::Rot()
{
	if (RotTime >= 0.f)
	{
		return;
	}
	Body->SetSimulatePhysics(false);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RotTime = 0.f;
	RotFrom = Mesh->GetRelativeScale3D();
	SetActorTickEnabled(true);
}

void AFallenFruit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	constexpr float Duration = 3.f;
	RotTime += DeltaSeconds;
	const float T = FMath::Clamp(RotTime / Duration, 0.f, 1.f);
	Mesh->SetRelativeScale3D(RotFrom * (1.f - 0.7f * T));
	AddActorWorldOffset(FVector(0.f, 0.f, -2.f * DeltaSeconds));
	if (T >= 1.f)
	{
		Destroy();
	}
}

void AFallenFruit::Init(UStaticMesh* FruitMesh, UMaterialInterface* Material, float Diameter, const float* Data, int32 NumData)
{
	Body->SetSphereRadius(Diameter * 0.45f);
	Mesh->SetStaticMesh(FruitMesh);
	Mesh->SetRelativeScale3D(FVector(Diameter / 100.f));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Material, this);
	for (int32 I = 0; I < NumData; I++)
	{
		M->SetScalarParameterValue(*FString::Printf(TEXT("Fruit%d"), I), Data[I]);
	}
	Mesh->SetMaterial(0, M);
	Body->SetSimulatePhysics(true);
}
