#include "Topi/LyudTopiProps.h"

#include "TopiModel.h"
#include "Topi/LyudTopi.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "LyudText.h"

namespace
{
	const TCHAR* const MeshFolder = TEXT("/Game/Topi/Props/");
	const TCHAR* const LanguageVariable = TEXT("lyud.Language");

	/** Map metres: where the dunes begin (the plates stand on the landscape up to Y -900, on the sand below). */
	struct FSignSpot
	{
		double X;
		double Y;
		bool bBeach;
	};
	const FSignSpot SignSpots[] = {
		{ 284.0, -778.0, true }, { 300.0, -800.0, true },
		{ 120.0, -770.0, false }, { -150.0, -765.0, false }, { -450.0, -760.0, false },
	};

	const double FirstStakeAlong = 150.0;
	const double StakeSpacing = 55.0;
	const double StakeSideM = 2.0;
	const double SignBackM = 9.0;
	const float RetrySeconds = 1.f;
	const double GiveUpSeconds = 30.0;
	const double TraceReachCm = 30000.0;

	UStaticMesh* LoadMesh(const FString& Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s%s.%s"), MeshFolder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FAutoConsoleCommandWithWorld TopiPropsCmd(TEXT("lyud.Topi.Props"), TEXT("Logs the signs, stakes and towels of Topi (TOPIPROPS ...)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W)
		{
			const ALyudTopi* Topi = ALyudTopi::Get(W);
			if (!Topi || !Topi->Props)
			{
				UE_LOG(LogTemp, Display, TEXT("TOPIPROPS no Topi in this world"));
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("TOPIPROPS %s"), *Topi->Props->Report());
		}));
}

ULyudTopiProps::ULyudTopiProps()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void ULyudTopiProps::EndPlay(const EEndPlayReason::Type Reason)
{
	if (LanguageHandle.IsValid())
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(LanguageVariable))
		{
			Variable->OnChangedDelegate().Remove(LanguageHandle);
		}
		LanguageHandle.Reset();
	}
	Super::EndPlay(Reason);
}

void ULyudTopiProps::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ALyudTopi* Topi = Cast<ALyudTopi>(GetOwner());
	if (!Topi)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (!bPlaced)
	{
		if (!Topi->HasActorBegunPlay())
		{
			return;                                   // the owner reads the map's frame and the edge table in its BeginPlay
		}
		bPlaced = true;
		if (!Topi->Model().Edge.Valid())
		{
			UE_LOG(LogTemp, Warning, TEXT("LyudTopiProps: Topi has no model (Content/Topi/topi_edge.tbl): nothing placed"));
			SetComponentTickEnabled(false);
			return;
		}
		PlaceAll(Topi);
		if (Pending.Num() > 0)
		{
			SetComponentTickInterval(RetrySeconds);
		}
		else
		{
			SetComponentTickEnabled(false);
		}
		return;
	}
	// the plates on the landscape: trace each down until the ground answers
	WaitedSeconds += DeltaTime;
	const bool bGiveUp = WaitedSeconds >= GiveUpSeconds;
	for (int32 I = Pending.Num() - 1; I >= 0; I--)
	{
		float GroundZ = 0.f;
		const FPendingSign Sign = Pending[I];
		if (TraceGround(Topi, Sign.MapX, Sign.MapY, GroundZ))
		{
			NumTraced++;
		}
		else if (bGiveUp)
		{
			GroundZ = Topi->SandZ(Topi->ToWorld(Sign.MapX, Sign.MapY, 0.0));
		}
		else
		{
			continue;
		}
		PlaceSign(Topi, Sign, GroundZ);
		Pending.RemoveAtSwap(I);
	}
	if (Pending.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

FQuat ULyudTopiProps::Stand(float YawDeg, float LeanDeg, float LeanDirDeg)
{
	// yaw first, then a lean of LeanDeg toward LeanDirDeg
	const float Dir = FMath::DegreesToRadians(LeanDirDeg);
	const FQuat Lean(FVector(-FMath::Sin(Dir), FMath::Cos(Dir), 0.f), FMath::DegreesToRadians(LeanDeg));
	return Lean * FQuat(FRotator(0.f, YawDeg, 0.f));
}

bool ULyudTopiProps::TraceGround(ALyudTopi* Topi, double MapX, double MapY, float& OutZ) const
{
	UWorld* World = GetWorld();
	if (!World || !Topi)
	{
		return false;
	}
	const FVector Base = Topi->ToWorld(MapX, MapY, 0.0);
	const float Sand = Topi->SandZ(Base);
	const FVector From(Base.X, Base.Y, Sand + TraceReachCm);
	const FVector To(Base.X, Base.Y, Sand - TraceReachCm);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TopiPropsGround), false, Topi);
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Query) ||
		World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query))
	{
		OutZ = FMath::Max(Hit.ImpactPoint.Z, Sand);           // where the sand has risen over the landscape, it is the sand
		return true;
	}
	return false;
}

UStaticMeshComponent* ULyudTopiProps::Spawn(UStaticMesh* Mesh, const FVector& At, const FQuat& Rotation, bool bCollide)
{
	AActor* Host = GetOwner();
	USceneComponent* Anchor = Host ? Host->GetRootComponent() : nullptr;
	if (!Mesh || !Anchor)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Host);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Static);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCastShadow(true);
	C->LightingChannels.bChannel1 = true;          // the outdoor sun
	C->SetupAttachment(Anchor);
	C->SetRelativeTransform(FTransform(Rotation, At).GetRelativeTransform(Anchor->GetComponentTransform()));
	C->RegisterComponent();
	Host->AddInstanceComponent(C);
	PlacedParts.Add(C);
	return C;
}

void ULyudTopiProps::PlaceAll(ALyudTopi* Topi)
{
	WarnMesh = LoadMesh(TEXT("SM_Topi_WarnSign"));
	UralMesh = LoadMesh(TEXT("SM_Topi_UralSign"));
	StakeMesh = LoadMesh(TEXT("SM_Topi_Stake"));
	TowelMeshes.Reset();
	for (const TCHAR* Letter : { TEXT("A"), TEXT("B"), TEXT("C") })
	{
		TowelMeshes.Add(LoadMesh(FString(TEXT("SM_Topi_Towel_")) + Letter));
	}
	if (!WarnMesh || !UralMesh || !StakeMesh || TowelMeshes.Contains(nullptr))
	{
		if (!bMissingLogged)
		{
			bMissingLogged = true;
			UE_LOG(LogTemp, Display, TEXT("LyudTopiProps: some of /Game/Topi/Props/SM_Topi_* are not imported (unreal/scripts/import_topi.sh): those props are left out"));
		}
	}

	// the warning plates: those on the beach face north, toward someone arriving along it; the rest face north too (the dunes lie south)
	FRandomStream Rand(7419);
	int32 Index = 0;
	for (const FSignSpot& Spot : SignSpots)
	{
		FPendingSign Sign;
		Sign.MapX = Spot.X;
		Sign.MapY = Spot.Y;
		Sign.YawDeg = Rand.FRandRange(-12.f, 12.f);
		Sign.LeanDeg = Rand.FRandRange(3.f, 9.f);
		Sign.LeanDirDeg = Rand.FRandRange(0.f, 360.f);
		Pending.Add(Sign);
		Index++;
	}
	if (!WarnMesh)
	{
		Pending.Reset();
	}
	// first try at once (the ground may be there already)
	for (int32 I = Pending.Num() - 1; I >= 0; I--)
	{
		float GroundZ = 0.f;
		if (TraceGround(Topi, Pending[I].MapX, Pending[I].MapY, GroundZ))
		{
			NumTraced++;
			PlaceSign(Topi, Pending[I], GroundZ);
			Pending.RemoveAtSwap(I);
		}
	}
	PlaceStakes(Topi);
	PlaceRiverBank(Topi);
	ApplySignLanguage();
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(LanguageVariable))
	{
		LanguageHandle = Variable->OnChangedDelegate().AddUObject(this, &ULyudTopiProps::OnLanguageChanged);
	}
	UE_LOG(LogTemp, Display, TEXT("LyudTopiProps: %s"), *Report());
}

void ULyudTopiProps::PlaceSign(ALyudTopi* Topi, const FPendingSign& Sign, float GroundZ)
{
	FVector At = Topi->ToWorld(Sign.MapX, Sign.MapY, 0.0);
	At.Z = GroundZ;
	UStaticMeshComponent* C = Spawn(WarnMesh, At, Stand(Sign.YawDeg, Sign.LeanDeg, Sign.LeanDirDeg), true);
	if (!C)
	{
		return;
	}
	NumSigns++;
	const TArray<FName> Slots = C->GetMaterialSlotNames();
	for (const FName& Slot : Slots)
	{
		const FString SlotName = Slot.ToString();
		if (!SlotName.StartsWith(TEXT("TP_Sign")))
		{
			continue;
		}
		UMaterialInterface* Base = C->GetMaterial(C->GetMaterialIndex(Slot));
		if (Base)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
			C->SetMaterial(C->GetMaterialIndex(Slot), MID);
			SignMIDs.Add(MID);
			SignNames.Add(SlotName.Mid(7).ToLower());
		}
	}
	ApplySignLanguage();
}

void ULyudTopiProps::PlaceStakes(ALyudTopi* Topi)
{
	if (!StakeMesh)
	{
		return;
	}
	FRandomStream Rand(3302);
	const double Length = LyudTopi::FModel::PathLength();
	for (double Along = FirstStakeAlong; Along <= Length; Along += StakeSpacing)
	{
		const LyudTopi::FVec2 P = LyudTopi::FModel::PathPoint(Along);
		const LyudTopi::FVec2 Ahead = LyudTopi::FModel::PathPoint(Along + 1.0);
		const LyudTopi::FVec2 Behind = LyudTopi::FModel::PathPoint(Along - 1.0);
		double Dx = Ahead.X - Behind.X;
		double Dy = Ahead.Y - Behind.Y;
		const double Len = FMath::Sqrt(Dx * Dx + Dy * Dy);
		if (Len < 1e-6)
		{
			continue;
		}
		Dx /= Len;
		Dy /= Len;
		FVector At = Topi->ToWorld(P.X + Dy * StakeSideM, P.Y - Dx * StakeSideM, 0.0);   // to the right of the way down
		At.Z = Topi->SandZ(At);
		const FQuat Rotation = Stand(Rand.FRandRange(-20.f, 20.f), Rand.FRandRange(2.f, 7.f), Rand.FRandRange(0.f, 360.f));
		if (Spawn(StakeMesh, At, Rotation, false))
		{
			NumStakes++;
		}
	}
}

void ULyudTopiProps::PlaceRiverBank(ALyudTopi* Topi)
{
	const double Length = LyudTopi::FModel::PathLength();
	const LyudTopi::FVec2 End = LyudTopi::FModel::PathPoint(Length);
	// the water's edge on the path's side (north): the river's middle plus its half width
	const double EdgeY = LyudTopi::FModel::RiverY(End.X) + LyudTopi::RiverHalfWidth;
	FRandomStream Rand(5521);
	if (UralMesh)
	{
		FVector At = Topi->ToWorld(End.X + 2.0, EdgeY + SignBackM, 0.0);          // beside the way, so that it is not in it
		At.Z = Topi->SandZ(At);
		if (Spawn(UralMesh, At, Stand(Rand.FRandRange(-6.f, 6.f), Rand.FRandRange(1.f, 3.f), Rand.FRandRange(0.f, 360.f)), true))
		{
			bUral = true;
			// the board's picture
			UStaticMeshComponent* C = PlacedParts.Last();
			const TArray<FName> Slots = C->GetMaterialSlotNames();
			for (const FName& Slot : Slots)
			{
				const FString SlotName = Slot.ToString();
				UMaterialInterface* Base = SlotName.StartsWith(TEXT("TP_Sign")) ? C->GetMaterial(C->GetMaterialIndex(Slot)) : nullptr;
				if (Base)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
					C->SetMaterial(C->GetMaterialIndex(Slot), MID);
					SignMIDs.Add(MID);
					SignNames.Add(SlotName.Mid(7).ToLower());
				}
			}
		}
	}
	double TowelX = End.X - 3.0;
	for (int32 K = 0; K < TowelMeshes.Num(); K++)
	{
		if (!TowelMeshes[K])
		{
			continue;
		}
		FVector At = Topi->ToWorld(TowelX, EdgeY + Rand.FRandRange(3.0f, 6.0f), 0.0);
		At.Z = Topi->SandZ(At);
		if (Spawn(TowelMeshes[K], At, FQuat(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f)), false))
		{
			NumTowels++;
		}
		TowelX += Rand.FRandRange(2.0f, 4.0f);
	}
}

void ULyudTopiProps::ApplySignLanguage()
{
	for (int32 I = 0; I < SignMIDs.Num(); I++)
	{
		UTexture2D* Tex = LyudText::LoadTexture(TEXT("Topi"), TEXT("T_Sign_") + SignNames[I]);
		if (Tex && SignMIDs[I])
		{
			SignMIDs[I]->SetTextureParameterValue(TEXT("Image"), Tex);
		}
	}
}

void ULyudTopiProps::OnLanguageChanged(IConsoleVariable* Variable)
{
	ApplySignLanguage();
}

FString ULyudTopiProps::Report() const
{
	const ALyudTopi* Topi = Cast<ALyudTopi>(GetOwner());
	FString Out = FString::Printf(TEXT("%d warning plates (%d waiting for the ground, %d traced), %d stakes, ural sign %s, %d towels, %d parts"),
		NumSigns, Pending.Num(), NumTraced, NumStakes, bUral ? TEXT("yes") : TEXT("no"), NumTowels, PlacedParts.Num());
	if (Topi && bPlaced)
	{
		for (const TObjectPtr<UStaticMeshComponent>& C : PlacedParts)
		{
			if (C && C->GetStaticMesh() && C->GetStaticMesh() == UralMesh)
			{
				const FVector2D M = Topi->ToMap(C->GetComponentLocation());
				Out += FString::Printf(TEXT("; ural sign at map (%.1f, %.1f)"), M.X, M.Y);
			}
		}
	}
	return Out;
}
