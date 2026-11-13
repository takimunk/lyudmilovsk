#include "North/LyudNorthFx.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Sky/LyudSky.h"
#include "StaticMeshAttributes.h"

namespace
{
	const TCHAR* NorthSteamPath = TEXT("/Game/North/Materials/M_North_Steam.M_North_Steam");
	const TCHAR* NorthPlumePath = TEXT("/Game/North/Materials/M_North_Plume.M_North_Plume");
	const TCHAR* NorthSmokeFallback = TEXT("/Game/Smoking/M_Smoke.M_Smoke");

	UMaterialInterface* NorthLoadMat(const TCHAR* Path, const TCHAR* Fallback)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		return M ? M : LoadObject<UMaterialInterface>(nullptr, Fallback, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** An instanced mesh of camera-facing cards in world space (the puffs, the plume's sprites). */
	UInstancedStaticMeshComponent* NorthCards(AActor* Owner, USceneComponent* Root, const TCHAR* Name, int32 CustomFloats)
	{
		UInstancedStaticMeshComponent* C = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		LyudNorthFx::Lightweight(C);
		C->SetLightingChannels(true, true, false);
		C->NumCustomDataFloats = CustomFloats;
		C->SetVisibility(false);
		return C;
	}
}

UStaticMesh* LyudNorthFx::PuffCard(UObject* Outer, UMaterialInterface* Material, FName Name)
{
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	const FPolygonGroupID PG = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Steam");
	TArray<FVertexInstanceID> Inst;
	const FVector3f P[4] = { { -0.5f, -0.5f, 0 }, { 0.5f, -0.5f, 0 }, { 0.5f, 0.5f, 0 }, { -0.5f, 0.5f, 0 } };
	const FVector2f T[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	for (int32 I = 0; I < 4; I++)
	{
		const FVertexID V = MD.CreateVertex();
		Attr.GetVertexPositions()[V] = P[I];
		const FVertexInstanceID VI = MD.CreateVertexInstance(V);
		Attr.GetVertexInstanceNormals()[VI] = FVector3f::UpVector;
		Attr.GetVertexInstanceTangents()[VI] = FVector3f::XAxisVector;
		Attr.GetVertexInstanceBinormalSigns()[VI] = 1.f;
		Attr.GetVertexInstanceUVs().Set(VI, 0, T[I]);
		Inst.Add(VI);
	}
	MD.CreateTriangle(PG, { Inst[0], Inst[2], Inst[1] });
	MD.CreateTriangle(PG, { Inst[0], Inst[3], Inst[2] });
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name),
		RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Steam")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	return Mesh;
}

void LyudNorthFx::Lightweight(UPrimitiveComponent* C)
{
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->SetVisibleInRayTracing(false);
	C->SetGenerateOverlapEvents(false);
}

// ---------------------------------------------------------------------------------------------------------------------
// the steam pool

ALyudNorthSteam::ALyudNorthSteam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// opacity, seed, grey
	Puffs = NorthCards(this, Root, TEXT("Puffs"), 3);
	All.SetNum(Pool);
	Xf.SetNum(Pool);
}

void ALyudNorthSteam::Emit(const FVector& Where, const FVector& Velocity, float Size0, float Size1, float Alpha, float Life, float Rise,
	float Drag, float Grey, float Drift)
{
	if (Puffs->GetInstanceCount() == 0)
	{
		// built once, at the first puff: the card and its pool of instances
		Card = LyudNorthFx::PuffCard(this, NorthLoadMat(NorthSteamPath, NorthSmokeFallback), TEXT("SM_NorthSteamPuff"));
		Puffs->SetStaticMesh(Card);
		TArray<FTransform> Zero;
		Zero.Init(FTransform(FQuat::Identity, Where, FVector(0.001f)), Pool);
		Puffs->AddInstances(Zero, false, true);
	}
	FPuff& P = All[Next];
	Next = (Next + 1) % Pool;
	// damp cold air keeps steam longer and thicker
	P.Pos = Where;
	P.Vel = Velocity;
	P.Age = 0.f;
	P.Life = FMath::Max(Life * FMath::Lerp(1.f, Density, 0.6f), 0.2f);
	P.Size0 = Size0;
	P.Size1 = Size1;
	P.Alpha = FMath::Min(1.f, Alpha * FMath::Lerp(1.f, Density, 0.5f));
	P.Rise = Rise;
	P.Drag = Drag;
	P.Grey = Grey;
	P.Drift = Drift;
	P.Seed = FMath::FRand();
	P.bHiddenWritten = false;
	if (!bShown)
	{
		bShown = true;
		Puffs->SetVisibility(true);
		SetActorTickEnabled(true);
	}
}

void ALyudNorthSteam::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	Live = 0;
	// only the instances that change are written: the live puffs and the ones that just went (once, hidden)
	int32 Lo = Pool, Hi = -1;
	for (int32 I = 0; I < Pool; ++I)
	{
		FPuff& P = All[I];
		if (P.Age >= P.Life && P.bHiddenWritten)
		{
			continue;
		}
		Lo = FMath::Min(Lo, I);
		Hi = I;
		float Show = 0.f;
		if (P.Age < P.Life)
		{
			P.Age += Dt;
			const float T = FMath::Clamp(P.Age / P.Life, 0.f, 1.f);
			// the jet's push dies away, warm steam climbs and wobbles, and the wind takes it more the older it is
			P.Vel *= FMath::Exp(-P.Drag * Dt);
			P.Vel.Z += P.Rise * Dt;
			const float W = P.Seed * 6.28f + P.Age * 1.7f;
			const float Carried = P.Drift * FMath::Min(1.f, 0.25f + T * 1.5f);
			P.Pos += (P.Vel + WindCm * Carried + FVector(FMath::Sin(W), FMath::Cos(W * 1.3f), 0.f) * (8.f + P.Size1 * 0.03f)) * Dt;
			Show = P.Alpha * FMath::SmoothStep(0.f, 0.1f, T) * (1.f - FMath::SmoothStep(0.4f, 1.f, T));
			++Live;
		}
		const float Scale = Show > 0.f ? FMath::Lerp(P.Size0, P.Size1, FMath::Sqrt(FMath::Clamp(P.Age / P.Life, 0.f, 1.f))) : 0.001f;
		const FVector ToCam = (Cam - P.Pos).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Xf[I] = FTransform(FRotationMatrix::MakeFromZ(ToCam).ToQuat() * FQuat(FVector::UpVector, P.Seed * 6.28f + P.Age * 0.3f), P.Pos, FVector(Scale));
		Puffs->SetCustomDataValue(I, 0, Show, false);
		Puffs->SetCustomDataValue(I, 1, P.Seed, false);
		Puffs->SetCustomDataValue(I, 2, P.Grey, false);
		P.bHiddenWritten = P.Age >= P.Life;
	}
	// incremental update (no proxy rebuild, perf checklist): the transforms and the custom data upload with it
	if (Hi >= Lo)
	{
		Puffs->BatchUpdateInstancesTransforms(Lo, TArrayView<const FTransform>(Xf.GetData() + Lo, Hi - Lo + 1), true, false, true);
	}
	if (Live == 0)
	{
		bShown = false;
		Puffs->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the far plume

ALyudNorthPlume::ALyudNorthPlume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	// opacity, seed
	Sprites = NorthCards(this, Root, TEXT("Sprites"), 2);
	// 10 km away and 600 m tall: never culled by distance
	Sprites->SetCullDistances(0, 0);
	Sprites->bNeverDistanceCull = true;
	Xf.SetNum(Count);
}

void ALyudNorthPlume::Setup(const FVector& InTop, float InCraterRadius, float InStrength)
{
	Top = InTop;
	CraterRadius = FMath::Max(InCraterRadius, 5000.f);
	Strength = FMath::Clamp(InStrength, 0.f, 1.f);
	UMaterialInterface* M = NorthLoadMat(NorthPlumePath, NorthSteamPath);
	if (!M)
	{
		M = NorthLoadMat(NorthSmokeFallback, NorthSmokeFallback);
	}
	Mat = M ? UMaterialInstanceDynamic::Create(M, this) : nullptr;
	UMaterialInterface* CardMat = Mat ? Mat.Get() : M;
	Card = LyudNorthFx::PuffCard(this, CardMat, TEXT("SM_NorthPlume"));
	Sprites->SetStaticMesh(Card);
	Sprites->SetMaterial(0, CardMat);
	TArray<FTransform> Zero;
	Zero.Init(FTransform(FQuat::Identity, Top, FVector(0.001f)), Count);
	Sprites->AddInstances(Zero, false, true);
	for (int32 I = 0; I < Count; I++)
	{
		Sprites->SetCustomDataValue(I, 1, (I * 0.618f) - FMath::FloorToFloat(I * 0.618f), false);
	}
	Sprites->SetVisibility(true);
	// a plume a long way off moves slowly: start mid-cycle so it is already up when the game begins
	Clock = 400.0;
	ColourTimer = 100.f;
}

void ALyudNorthPlume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Card)
	{
		return;
	}
	Clock += DeltaTime;
	// the wind aloft changes the bend slowly
	SmoothWind = FMath::Lerp(SmoothWind, WindCm, FMath::Min(1.f, DeltaTime * 0.05f));
	ColourTimer += DeltaTime;
	if (ColourTimer > 4.f && Mat)
	{
		ColourTimer = 0.f;
		float SunAlt = 20.f;
		TActorIterator<ALyudSky> It(GetWorld());
		if (It)
		{
			SunAlt = It->SunAltitude();
		}
		// white by day, warm at sunset, a faint grey under the stars
		const float Day = FMath::SmoothStep(-8.f, 10.f, SunAlt);
		const float Low = FMath::SmoothStep(-4.f, 2.f, SunAlt) * (1.f - FMath::SmoothStep(4.f, 16.f, SunAlt));
		const FLinearColor C = FLinearColor::LerpUsingHSV(FLinearColor(0.03f, 0.035f, 0.05f), FLinearColor(1.f, 0.98f, 0.95f), Day)
			* FLinearColor(1.f, 1.f - 0.25f * Low, 1.f - 0.45f * Low);
		Mat->SetVectorParameterValue(TEXT("Color"), C);
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	// each sprite climbs ~600 m over Period s, swelling, bending downwind as it goes, fading at the top
	const double Period = 260.0;
	const FVector Drift = SmoothWind * 0.6f;
	for (int32 I = 0; I < Count; I++)
	{
		const double Ph = FMath::Frac(Clock / Period + double(I) / Count);
		const float H = float(Ph) * 60000.f;
		const FVector At = Top + FVector(0.f, 0.f, H) + Drift * float(Ph * Period) * FMath::Pow(float(Ph), 0.6f);
		const float Scale = CraterRadius * 1.6f + float(Ph) * 26000.f;
		const float Show = Strength * 0.55f * FMath::SmoothStep(0.f, 0.08f, float(Ph)) * (1.f - FMath::SmoothStep(0.55f, 1.f, float(Ph)));
		const FVector ToCam = (Cam - At).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Xf[I] = FTransform(FRotationMatrix::MakeFromZ(ToCam).ToQuat() * FQuat(FVector::UpVector, I * 1.3f + float(Ph) * 0.8f), At, FVector(Scale));
		Sprites->SetCustomDataValue(I, 0, Show, false);
	}
	Sprites->BatchUpdateInstancesTransforms(0, TArrayView<const FTransform>(Xf.GetData(), Count), true, false, true);
}
