#include "Plants/ProceduralPlant.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantMeshes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Scene 01's outside sun lights channel 1 only; channel 0 keeps the default lights too. */
	void OutdoorLighting(UPrimitiveComponent* C)
	{
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCanEverAffectNavigation(false);
	}

	UInstancedStaticMeshComponent* MakeInstanced(AActor* Owner, FName Name, USceneComponent* Parent)
	{
		UInstancedStaticMeshComponent* ISM = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		ISM->SetupAttachment(Parent);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->bDisableCollision = true;
		ISM->SetMobility(EComponentMobility::Movable);
		OutdoorLighting(ISM);
		return ISM;
	}

	void SetInstances(UInstancedStaticMeshComponent* ISM, const TArray<FTransform>& T)
	{
		ISM->ClearInstances();
		if (!T.IsEmpty())
		{
			ISM->AddInstances(T, false, false, false);
		}
	}
}

AProceduralPlant::AProceduralPlant()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BaseMaterial = Basic.Object;

	Bark = MakeInstanced(this, TEXT("Bark"), RootComponent);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafComponents.Add(MakeInstanced(this, *FString::Printf(TEXT("Leaves%d"), K), RootComponent));
	}
	SummaryTrunk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SummaryTrunk"));
	SummaryCrown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SummaryCrown"));
	for (UStaticMeshComponent* C : { SummaryTrunk.Get(), SummaryCrown.Get() })
	{
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetMobility(EComponentMobility::Movable);
		C->SetVisibility(false);
		OutdoorLighting(C);
	}
	SummaryTrunk->SetStaticMesh(Cylinder.Object);
	SummaryCrown->SetStaticMesh(Sphere.Object);

	TrunkCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TrunkCollision"));
	TrunkCollision->SetupAttachment(RootComponent);
	TrunkCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TrunkCollision->SetCanEverAffectNavigation(false);
}

AProceduralPlant::~AProceduralPlant() = default;

TArray<FName> AProceduralPlant::GetSpeciesNames() const
{
	return PlantSpeciesLibrary::Names();
}

const FPlantSpecies& AProceduralPlant::GetSpecies() const
{
	const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
	return S ? *S : PlantSpeciesLibrary::All()[0];
}

float AProceduralPlant::GetAge() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return EditorPreviewAge >= 0.f ? EditorPreviewAge : AgeAtStart;
	}
	if (bDemoGrowth)
	{
		const float F = FMath::Clamp(((float)UGameCalendar::GameSeconds(World) - DemoDelay) / FMath::Max(DemoSeconds, 0.1f), 0.f, 1.f);
		return FMath::Lerp(AgeAtStart, FMath::Max(DemoTargetAge, AgeAtStart), F);
	}
	const UGameCalendar* Calendar = World->GetSubsystem<UGameCalendar>();
	const FDateTime Start = Calendar ? Calendar->StartDate : GetDefault<UGameCalendar>()->StartDate;
	return AgeAtStart + (float)UGameCalendar::YearsBetween(Start, UGameCalendar::NowIn(World));
}

float AProceduralPlant::CurrentDay() const
{
	const UWorld* World = GetWorld();
	if ((!World || !World->IsGameWorld()) && EditorPreviewDay >= 0.f)
	{
		return EditorPreviewDay;
	}
	return UGameCalendar::DayOfYear(UGameCalendar::NowIn(World));
}

FPlantSummary AProceduralPlant::GetSummary() const
{
	return PlantModel::Summary(GetSpecies(), (uint32)Seed, GetAge(), CurrentDay());
}

bool AProceduralPlant::WantsSummary() const
{
	if (bForceSummary)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	return FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > SummaryDistance;
}

void AProceduralPlant::EnsureMeshes()
{
	if (!BarkMesh)
	{
		BarkMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		BarkMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
		BarkMesh = PlantMeshes::BuildBark(this, BarkMaterial);
		Bark->SetStaticMesh(BarkMesh);
		Bark->SetMaterial(0, BarkMaterial);
		SummaryTrunk->SetMaterial(0, BarkMaterial);

		LeafMaterials.Reset();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			M->SetScalarParameterValue(TEXT("Roughness"), 0.75f);
			LeafMaterials.Add(M);
			if (K == 0)
			{
				LeafMesh = PlantMeshes::BuildLeafCluster(this, M);
			}
			LeafComponents[K]->SetStaticMesh(LeafMesh);
			LeafComponents[K]->SetMaterial(0, M);
		}
		CrownMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		CrownMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
		SummaryCrown->SetMaterial(0, CrownMaterial);
	}
	if (BuiltSpecies != Species || BuiltSeed != Seed || !Skeleton.IsBuilt())
	{
		Skeleton.Build(GetSpecies(), (uint32)Seed);
		BuiltSpecies = Species;
		BuiltSeed = Seed;
	}
}

void AProceduralPlant::Apply(float Age, float Day, bool bSummary)
{
	EnsureMeshes();
	const FPlantSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S, (uint32)Seed, Day);
	const FPlantSummary Sum = PlantModel::Summary(S, (uint32)Seed, Age, Day);
	BarkMaterial->SetVectorParameterValue(TEXT("Color"), S.BarkColour);

	Bark->SetVisibility(!bSummary);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafMaterials[K]->SetVectorParameterValue(TEXT("Color"), Season.Colour[K]);
		LeafComponents[K]->SetVisibility(!bSummary);
	}
	SummaryTrunk->SetVisibility(bSummary);
	SummaryCrown->SetVisibility(bSummary && Sum.LeafAmount > 0.05f);

	if (bSummary)
	{
		// level 0: no branches at all, just the summary's height, width and colour
		SetInstances(Bark, {});
		for (UInstancedStaticMeshComponent* L : LeafComponents) SetInstances(L, {});
		const float TrunkH = FMath::Max(Sum.CrownBaseHeight, Sum.Height * 0.3f);
		SummaryTrunk->SetRelativeLocation(FVector(0, 0, TrunkH * 50.f));
		SummaryTrunk->SetRelativeScale3D(FVector(Sum.TrunkRadius * 2.f, Sum.TrunkRadius * 2.f, TrunkH));
		SummaryCrown->SetRelativeLocation(FVector(0, 0, (Sum.CrownBaseHeight + Sum.Height) * 50.f));
		SummaryCrown->SetRelativeScale3D(FVector(Sum.CrownRadius * 2.f, Sum.CrownRadius * 2.f, FMath::Max(Sum.Height - Sum.CrownBaseHeight, 0.05f)));
		// a solid ellipsoid has none of the crown's self-shadowing: darken it to match from afar
		CrownMaterial->SetVectorParameterValue(TEXT("Color"), Sum.LeafColour * FLinearColor(0.6f, 0.6f, 0.6f, 1.f));
	}
	else
	{
		FPlantSkeleton::FInstances Out;
		Skeleton.Evaluate(Age, Season, Out);
		SetInstances(Bark, Out.Bark);
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			SetInstances(LeafComponents[K], Out.Leaves[K]);
		}
	}
	const float R = FMath::Max(Sum.TrunkRadius * 100.f, 2.f);
	const float HalfH = FMath::Clamp(Sum.Height * 50.f, R, 150.f);
	TrunkCollision->SetCapsuleSize(R, HalfH);
	TrunkCollision->SetRelativeLocation(FVector(0, 0, HalfH));

	ShownAge = Age;
	ShownDay = Day;
	bShownSummary = bSummary;
}

void AProceduralPlant::ShowAt(float Age, float DayOfYear, bool bSummary)
{
	Apply(Age, DayOfYear, bSummary);
}

void AProceduralPlant::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Apply(GetAge(), CurrentDay(), bForceSummary);
}

void AProceduralPlant::BeginPlay()
{
	Super::BeginPlay();
	SinceUpdate = 0.f;
	if (bSnapToGround)
	{
		FHitResult Hit;
		const FVector At = GetActorLocation();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(PlantGround), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 500), At - FVector(0, 0, 3000), ECC_Visibility, Q))
		{
			SetActorLocation(Hit.ImpactPoint - FVector(0, 0, 3)); // the trunk goes into the soil a little
		}
	}
	Apply(GetAge(), CurrentDay(), WantsSummary());
}

void AProceduralPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SinceUpdate += DeltaSeconds;
	if (SinceUpdate < UpdateInterval)
	{
		return;
	}
	SinceUpdate = 0.f;
	const float Age = GetAge(), Day = CurrentDay();
	const bool bSummary = WantsSummary();
	// a tree grows a few mm a day: re-shape only when the change is visible
	if (bSummary != bShownSummary || FMath::Abs(Age - ShownAge) > 0.01f || FMath::Abs(Day - ShownDay) > 0.25f)
	{
		Apply(Age, Day, bSummary);
	}
}

FString AProceduralPlant::DebugReport(float Age, float DayOfYear)
{
	EnsureMeshes();
	const FPlantSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S, (uint32)Seed, DayOfYear);
	const FPlantSummary Sum = PlantModel::Summary(S, (uint32)Seed, Age, DayOfYear);
	FPlantSkeleton::FInstances Out;
	const double T0 = FPlatformTime::Seconds();
	Skeleton.Evaluate(Age, Season, Out);
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	const int32 Leaves = Out.Leaves[0].Num() + Out.Leaves[1].Num() + Out.Leaves[2].Num();
	return FString::Printf(TEXT("%s seed %d age %.1f day %.0f: %d/%d branches live, %d bark, %d leaves (%d/%d/%d), drawn h %.2f reach %.2f | summary h %.2f crown r %.2f base %.2f trunk r %.3f leaves %.2f colour %s | %.2f ms"),
		*S.Name.ToString(), Seed, Age, DayOfYear, Out.LiveBranches, Skeleton.GetBranches().Num(), Out.Bark.Num(), Leaves,
		Out.Leaves[0].Num(), Out.Leaves[1].Num(), Out.Leaves[2].Num(), Out.Height, Out.Reach,
		Sum.Height, Sum.CrownRadius, Sum.CrownBaseHeight, Sum.TrunkRadius, Sum.LeafAmount, *Sum.LeafColour.ToString(), Ms);
}
