#include "Plants/FallenFruit.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AFallenFruit::AFallenFruit()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetStaticMesh(Sphere.Object);
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// the player walks through apples rather than kicking them about
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetLinearDamping(0.6f);
	Mesh->SetAngularDamping(1.5f);
	Mesh->LightingChannels.bChannel0 = true;
	Mesh->LightingChannels.bChannel1 = true;
	Mesh->CanCharacterStepUpOn = ECB_No;
	SetCanBeDamaged(false);
}
