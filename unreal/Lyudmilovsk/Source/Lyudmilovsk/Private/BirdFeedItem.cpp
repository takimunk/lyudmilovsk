#include "BirdFeedItem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"

namespace
{
	UMaterialInstanceDynamic* Paint(UPrimitiveComponent* C, UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base || !C)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
		return MID;
	}
}

// ------------------------------------------------------------------ the bag

ABirdFeedBag::ABirdFeedBag()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Part = [this, Root, Cube](const TCHAR* Name, const FVector& Loc, const FVector& Size)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Cube);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return C;
	};
	// the bag (X depth, Y width, Z height), the top rolled over twice, a printed paper band round its waist
	Body = Part(TEXT("Body"), FVector::ZeroVector, FVector(2.f * HalfDepth, 2.f * HalfWidth, 2.f * HalfHeight));
	Fold = Part(TEXT("Fold"), FVector(0.f, 0.f, HalfHeight - 0.6f), FVector(2.f * HalfDepth + 0.7f, 2.f * HalfWidth + 0.2f, 1.8f));
	Band = Part(TEXT("Band"), FVector(0.f, 0.f, -0.8f), FVector(2.f * HalfDepth + 0.15f, 2.f * HalfWidth + 0.15f, 4.2f));
}

void ABirdFeedBag::BeginPlay()
{
	Super::BeginPlay();
	Paint(Body, this, FLinearColor(0.42f, 0.27f, 0.14f));
	Paint(Fold, this, FLinearColor(0.33f, 0.2f, 0.1f));
	Paint(Band, this, FLinearColor(0.78f, 0.7f, 0.5f));
}

FReachItem ABirdFeedBag::MakeReachItem() const
{
	FReachItem R;
	R.Actor = const_cast<ABirdFeedBag*>(this);
	R.Center = GetActorLocation();
	R.Axis = GetActorUpVector();
	R.Front = GetActorForwardVector();
	R.HalfLength = HalfHeight - HalfWidth;
	R.Radius = HalfWidth;
	R.MouthDistance = HalfHeight;
	return R;
}

void ABirdFeedBag::LieDown(const FVector& Pos, const FVector& Along)
{
	// on its back, the band up, the top toward Along
	SetActorLocationAndRotation(Pos, FRotationMatrix::MakeFromZX(Along.GetSafeNormal(), FVector::UpVector).Rotator());
}

// ------------------------------------------------------------------ the handful on the palm

ASeedHandful::ASeedHandful()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (TObjectPtr<UInstancedStaticMeshComponent>* C : { &Seeds, &Millet })
	{
		*C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(C == &Seeds ? TEXT("Seeds") : TEXT("Millet"));
		(*C)->SetupAttachment(Root);
		(*C)->SetStaticMesh(Sphere);
		(*C)->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		(*C)->SetCastShadow(true);
	}
}

void ASeedHandful::BeginPlay()
{
	Super::BeginPlay();
	Paint(Seeds, this, FLinearColor(0.03f, 0.028f, 0.026f));
	Paint(Millet, this, FLinearColor(0.72f, 0.55f, 0.25f));
	Build();
}

void ASeedHandful::Build()
{
	// a low heap, highest in the middle: each seed or grain dropped at a random spot rests on the heap's surface there.
	// Sorted from the bottom up, so the last instances are the top ones and pecking takes them first.
	struct FGrain { FTransform Xf; bool bSeed; };
	TArray<FGrain> Grains;
	FRandomStream Rand(1917);
	constexpr float Height = 1.6f;
	for (int32 I = 0; I < 110; ++I)
	{
		const bool bSeed = I % 3 != 0;
		const float R = Radius * FMath::Sqrt(Rand.FRand());
		const float A = Rand.FRand() * 2.f * PI;
		const float Top = Height * (1.f - FMath::Square(R / Radius));
		const float Z = Top * FMath::Sqrt(Rand.FRand()) + (bSeed ? 0.15f : 0.1f);
		const FVector P(R * FMath::Cos(A), R * FMath::Sin(A), Z);
		// a sunflower seed: a 1 x 0.5 x 0.3 cm flat drop lying any way round, tilted with the heap's slope; millet: a 2 mm ball
		const FRotator Rot(Rand.FRandRange(-25.f, 25.f) - 20.f * R / Radius, Rand.FRand() * 360.f, Rand.FRandRange(-20.f, 20.f));
		const FVector Scale = bSeed ? FVector(1.f, 0.5f, 0.3f) * 0.01f * Rand.FRandRange(0.85f, 1.15f) : FVector(0.0022f);
		Grains.Add({ FTransform(Rot, P, Scale), bSeed });
	}
	Grains.Sort([](const FGrain& A, const FGrain& B) { return A.Xf.GetLocation().Z < B.Xf.GetLocation().Z; });
	Seeds->ClearInstances();
	Millet->ClearInstances();
	for (const FGrain& G : Grains)
	{
		(G.bSeed ? Seeds : Millet)->AddInstance(G.Xf);
	}
	SeedCount = Seeds->GetInstanceCount();
	MilletCount = Millet->GetInstanceCount();
}

void ASeedHandful::SetLeft(float Left)
{
	Left = FMath::Clamp(Left, 0.f, 1.f);
	for (UInstancedStaticMeshComponent* C : { Seeds.Get(), Millet.Get() })
	{
		const int32 Full = C == Seeds ? SeedCount : MilletCount;
		const int32 Want = FMath::RoundToInt(Full * Left);
		// the top grains go first; they are hidden (scaled to nothing) so the order stays
		for (int32 I = 0; I < Full; ++I)
		{
			FTransform Xf;
			C->GetInstanceTransform(I, Xf, false);
			const bool bShown = I < Want;
			if (bShown != !Xf.GetScale3D().IsNearlyZero(1e-6))
			{
				if (!bShown)
				{
					Xf.SetScale3D(FVector(1e-7f));
				}
				else
				{
					// rebuilt below with the right scale
					Build();
					SetLeft(Left);
					return;
				}
				C->UpdateInstanceTransform(I, Xf, false, false, true);
			}
		}
		C->MarkRenderStateDirty();
	}
}
