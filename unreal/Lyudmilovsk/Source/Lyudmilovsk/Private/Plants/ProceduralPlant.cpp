#include "Plants/ProceduralPlant.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Plants/FallenFruit.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantMeshes.h"
#include "StaticMeshResources.h"
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

	int32 NumTriangles(const UStaticMesh* Mesh)
	{
		const FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
		return RD && RD->LODResources.Num() ? RD->LODResources[0].GetNumTriangles() : 0;
	}
}

AProceduralPlant::AProceduralPlant()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Basic(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BarkMat(TEXT("/Game/Plants/M_PlantBark.M_PlantBark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMat(TEXT("/Game/Plants/M_PlantLeaf.M_PlantLeaf"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BaseMaterial = Basic.Object;
	BarkMaterialAsset = BarkMat.Object ? BarkMat.Object : Basic.Object;
	LeafMaterialAsset = LeafMat.Object ? LeafMat.Object : Basic.Object;

	Wood = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wood"));
	Wood->SetupAttachment(RootComponent);
	Wood->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wood->SetMobility(EComponentMobility::Movable);
	OutdoorLighting(Wood);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafComponents.Add(MakeInstanced(this, *FString::Printf(TEXT("Leaves%d"), K), RootComponent));
	}
	FruitComponent = MakeInstanced(this, TEXT("Fruit"), RootComponent);
	FruitComponent->SetStaticMesh(Sphere.Object);

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

FDateTime AProceduralPlant::CurrentDate() const
{
	const UWorld* World = GetWorld();
	return UGameCalendar::NowIn(World) + FTimespan::FromDays(DemoDaysPerSecond * UGameCalendar::GameSeconds(World));
}

float AProceduralPlant::CurrentDay() const
{
	const UWorld* World = GetWorld();
	if ((!World || !World->IsGameWorld()) && EditorPreviewDay >= 0.f)
	{
		return EditorPreviewDay;
	}
	return UGameCalendar::DayOfYear(CurrentDate());
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
	if (!CrownMaterial)
	{
		WoodMaterial = UMaterialInstanceDynamic::Create(BarkMaterialAsset, this);
		Wood->SetMaterial(0, WoodMaterial);
		SummaryTrunk->SetMaterial(0, WoodMaterial);
		LeafMaterials.Reset();
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials.Add(UMaterialInstanceDynamic::Create(LeafMaterialAsset, this));
		}
		FruitMaterial = UMaterialInstanceDynamic::Create(LeafMaterialAsset, this);
		FruitComponent->SetMaterial(0, FruitMaterial);
		CrownMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		CrownMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
		SummaryCrown->SetMaterial(0, CrownMaterial);
	}
	if (BuiltSpecies != Species || BuiltSeed != Seed || !Skeleton.IsBuilt() || !LeafMesh)
	{
		const FPlantSpecies& S = GetSpecies();
		Skeleton.Build(S, (uint32)Seed);
		UTexture2D* Albedo = nullptr;
		UTexture2D* Normal = nullptr;
		PlantMeshes::BuildBarkTextures(this, S, (uint32)Seed, Albedo, Normal);
		BarkAlbedo = Albedo;
		BarkNormal = Normal;
		LeafDetail = PlantMeshes::BuildLeafDetail(this, S.Leaf);
		LeafMesh = PlantMeshes::BuildLeafCluster(this, S);

		WoodMaterial->SetTextureParameterValue(TEXT("BarkAlbedo"), BarkAlbedo);
		WoodMaterial->SetTextureParameterValue(TEXT("BarkNormal"), BarkNormal);
		WoodMaterial->SetVectorParameterValue(TEXT("Color"), S.Bark.RidgeColour); // when the plant materials are missing
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			LeafMaterials[K]->SetTextureParameterValue(TEXT("LeafDetail"), LeafDetail);
			LeafComponents[K]->SetStaticMesh(LeafMesh);
			LeafComponents[K]->SetMaterial(0, LeafMaterials[K]);
		}
		BuiltSpecies = Species;
		BuiltSeed = Seed;
		WoodAge = -1.f;
	}
}

void AProceduralPlant::Apply(float Age, float Day, bool bSummary, bool bWood)
{
	EnsureMeshes();
	const FPlantSpecies& S = GetSpecies();
	const FPlantSeason Season = PlantModel::Season(S, (uint32)Seed, Day);
	const FPlantSummary Sum = PlantModel::Summary(S, (uint32)Seed, Age, Day);

	WoodMaterial->SetScalarParameterValue(TEXT("Snow"), Season.Snow);
	for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
	{
		LeafMaterials[K]->SetVectorParameterValue(TEXT("Color"), Season.Colour[K]);
		LeafComponents[K]->SetVisibility(!bSummary);
	}
	FruitMaterial->SetVectorParameterValue(TEXT("Color"), Season.FruitColour);
	Wood->SetVisibility(!bSummary);
	FruitComponent->SetVisibility(!bSummary);
	SummaryTrunk->SetVisibility(bSummary);
	SummaryCrown->SetVisibility(bSummary && Sum.LeafAmount > 0.05f);

	if (bSummary)
	{
		// level 0: no branches at all, just the summary's height, width and colour
		for (UInstancedStaticMeshComponent* L : LeafComponents) SetInstances(L, {});
		SetInstances(FruitComponent, {});
		const float TrunkH = FMath::Max(Sum.CrownBaseHeight, Sum.Height * 0.3f);
		SummaryTrunk->SetRelativeLocation(FVector(0, 0, TrunkH * 50.f));
		SummaryTrunk->SetRelativeScale3D(FVector(Sum.TrunkRadius * 2.f, Sum.TrunkRadius * 2.f, TrunkH));
		SummaryCrown->SetRelativeLocation(FVector(0, 0, (Sum.CrownBaseHeight + Sum.Height) * 50.f));
		SummaryCrown->SetRelativeScale3D(FVector(Sum.CrownRadius * 2.f, Sum.CrownRadius * 2.f, FMath::Max(Sum.Height - Sum.CrownBaseHeight, 0.05f)));
		// a solid ellipsoid has none of the crown's self-shadowing: darken it to match from afar
		CrownMaterial->SetVectorParameterValue(TEXT("Color"), Sum.LeafColour * FLinearColor(0.4f, 0.4f, 0.4f, 1.f));
	}
	else
	{
		FPlantSkeleton::FInstances Out;
		Skeleton.Evaluate(Age, Season, Out);
		for (int32 K = 0; K < FPlantSeason::NumBuckets; K++)
		{
			SetInstances(LeafComponents[K], Out.Leaves[K]);
		}
		SetInstances(FruitComponent, Out.Fruit);
		if (GetWorld() && GetWorld()->IsGameWorld())
		{
			SyncFallenFruit(Out);
		}
		if (bWood || WoodAge < 0.f)
		{
			WoodMesh = PlantMeshes::BuildWood(this, WoodMesh, Out.Tubes, S.Bark);
			if (Wood->GetStaticMesh() != WoodMesh)
			{
				Wood->SetStaticMesh(WoodMesh);
			}
			Wood->MarkRenderStateDirty();
			WoodAge = Age;
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

void AProceduralPlant::SyncFallenFruit(const FPlantSkeleton::FInstances& Out)
{
	const FPlantSpecies& S = GetSpecies();
	const int32 Year = CurrentDate().GetYear() * 100000;
	const FTransform& Xf = GetActorTransform();
	TSet<int32> Want;
	for (const FPlantSkeleton::FInstances::FFallen& F : Out.FallenFruit)
	{
		const int32 Key = Year + F.Id;
		if (PickedFruit.Contains(Key))
		{
			continue;
		}
		Want.Add(Key);
		const TWeakObjectPtr<AFallenFruit>* Have = GroundFruit.Find(Key);
		if (Have && Have->IsValid())
		{
			continue;
		}
		// one that drops while we watch falls from its twig; older ones already lie in the grass under it
		FVector At = Xf.TransformPosition(FVector(F.At) * 100.0);
		if (!HangingFruit.Contains(Key))
		{
			const FVector Scatter((PlantModel::Hash01(Seed, F.Id, 0, 31) - 0.5f) * 120.f, (PlantModel::Hash01(Seed, F.Id, 0, 32) - 0.5f) * 120.f, 0.f);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(FruitGround), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, At + Scatter, At + Scatter - FVector(0, 0, 3000), ECC_Visibility, Q))
			{
				At = Hit.ImpactPoint + FVector(0, 0, S.FruitSize * 50.f);
			}
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFallenFruit* Fruit = GetWorld()->SpawnActor<AFallenFruit>(At, FRotator(0.f, PlantModel::Hash01(Seed, F.Id, 0, 24) * 360.f, 0.f), P);
		if (!Fruit) continue;
		Fruit->Tree = this;
		Fruit->Key = Key;
		Fruit->SetActorScale3D(FVector(S.FruitSize, S.FruitSize, S.FruitSize * 0.92f));
		Fruit->Mesh->SetMaterial(0, FruitMaterial);
		Fruit->Mesh->SetSimulatePhysics(true);
		GroundFruit.Add(Key, Fruit);
	}
	// rotted away, or a new year
	for (auto It = GroundFruit.CreateIterator(); It; ++It)
	{
		if (!Want.Contains(It.Key()))
		{
			if (AFallenFruit* F = It.Value().Get()) F->Destroy();
			It.RemoveCurrent();
		}
	}
	HangingFruit.Reset();
	for (const int32 Id : Out.FruitIds)
	{
		HangingFruit.Add(Year + Id);
	}
}

void AProceduralPlant::ClearFallenFruit()
{
	for (const TPair<int32, TWeakObjectPtr<AFallenFruit>>& F : GroundFruit)
	{
		if (AFallenFruit* A = F.Value.Get()) A->Destroy();
	}
	GroundFruit.Reset();
	HangingFruit.Reset();
}

void AProceduralPlant::OnInteract()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !PC->PlayerCameraManager || GroundFruit.IsEmpty())
	{
		return;
	}
	const FVector From = PC->PlayerCameraManager->GetCameraLocation();
	const FVector To = From + PC->PlayerCameraManager->GetCameraRotation().Vector() * (PickDistance + 100.f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FruitPick), false, Pawn);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
	{
		return;
	}
	AFallenFruit* Fruit = Cast<AFallenFruit>(Hit.GetActor());
	if (!Fruit || Fruit->Tree.Get() != this || FVector::Dist(Pawn->GetActorLocation(), Fruit->GetActorLocation()) > PickDistance)
	{
		return;
	}
	PickedFruit.AddUnique(Fruit->Key);
	FruitPicked++;
	GroundFruit.Remove(Fruit->Key);
	Fruit->Destroy();
}

void AProceduralPlant::OnGameLoaded_Implementation()
{
	// the picked list came back with the save: lay the ground out again from it
	ClearFallenFruit();
	Apply(GetAge(), CurrentDay(), WantsSummary());
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
	SinceUpdate = SinceWood = 0.f;
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
	if (GetSpecies().FruitSize > 0.f)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			EnableInput(PC);
			if (InputComponent)
			{
				// several actors listen for E (doors, seats, people); none may swallow it
				InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AProceduralPlant::OnInteract).bConsumeInput = false;
			}
		}
	}
	Apply(GetAge(), CurrentDay(), WantsSummary());
}

void AProceduralPlant::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearFallenFruit();
	Super::EndPlay(Reason);
}

void AProceduralPlant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SinceUpdate += DeltaSeconds;
	SinceWood += DeltaSeconds;
	if (SinceUpdate < UpdateInterval)
	{
		return;
	}
	SinceUpdate = 0.f;
	const float Age = GetAge(), Day = CurrentDay();
	const bool bSummary = WantsSummary();
	// a tree grows a few mm a day: re-shape only when the change is visible; the wood less often
	const bool bChanged = bSummary != bShownSummary || FMath::Abs(Age - ShownAge) > 0.01f || FMath::Abs(Day - ShownDay) > 0.25f;
	const bool bWoodDue = !bSummary && SinceWood >= WoodUpdateInterval && FMath::Abs(Age - WoodAge) > 0.001f;
	if (bChanged || bWoodDue)
	{
		Apply(Age, Day, bSummary, bWoodDue);
		if (bWoodDue)
		{
			SinceWood = 0.f;
		}
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
	const double T1 = FPlatformTime::Seconds();
	WoodMesh = PlantMeshes::BuildWood(this, WoodMesh, Out.Tubes, S.Bark);
	Wood->SetStaticMesh(WoodMesh);
	WoodAge = Age;
	const double T2 = FPlatformTime::Seconds();
	const int32 Leaves = Out.Leaves[0].Num() + Out.Leaves[1].Num() + Out.Leaves[2].Num();
	return FString::Printf(TEXT("%s seed %d age %.1f day %.0f: %d/%d branches, %d rings, wood %d tris, %d leaf clusters x %d tris, %d fruit, drawn h %.2f reach %.2f | summary h %.2f crown r %.2f leaves %.2f | evaluate %.2f ms, wood mesh %.1f ms"),
		*S.Name.ToString(), Seed, Age, DayOfYear, Out.LiveBranches, Skeleton.GetBranches().Num(), Out.NumRings(), NumTriangles(WoodMesh),
		Leaves, NumTriangles(LeafMesh), Out.Fruit.Num(), Out.Height, Out.Reach, Sum.Height, Sum.CrownRadius, Sum.LeafAmount,
		(T1 - T0) * 1000.0, (T2 - T1) * 1000.0);
}
