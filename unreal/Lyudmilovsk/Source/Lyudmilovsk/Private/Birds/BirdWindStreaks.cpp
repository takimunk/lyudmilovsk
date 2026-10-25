#include "BirdWindStreaks.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

const FName UBirdWindStreaks::MaterialPath(TEXT("/Game/Birds/Flight/M_WindStreak.M_WindStreak"));

namespace
{
	constexpr int32 NumStreaks = 90;
	constexpr float Radius = 2200.f;   // cm round the camera

	/** A card 1 cm long along +X, 1 cm wide, both faces; U along it. */
	UStaticMesh* CardMesh(UObject* Outer, UMaterialInterface* Material)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Streak");
		TArray<FVertexInstanceID> Inst;
		const FVector3f P[4] = { { 0, -0.5f, 0 }, { 1, -0.5f, 0 }, { 1, 0.5f, 0 }, { 0, 0.5f, 0 } };
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
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, TEXT("SM_WindStreak"), RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Streak")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}
}

UBirdWindStreaks::UBirdWindStreaks()
{
	PrimaryComponentTick.bCanEverTick = false;
	Rng.Initialize(7177);
}

void UBirdWindStreaks::OnRegister()
{
	Super::OnRegister();
	if (Cards || !GetOwner())
	{
		return;
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath.ToString());
	Cards = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("WindStreakCards"));
	Cards->SetStaticMesh(CardMesh(this, Material));
	Cards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cards->SetCastShadow(false);
	Cards->NumCustomDataFloats = 1;
	Cards->SetUsingAbsoluteLocation(true);
	Cards->SetUsingAbsoluteRotation(true);
	Cards->SetupAttachment(this);
	Cards->RegisterComponent();
	Streaks.SetNum(NumStreaks);
	for (int32 I = 0; I < NumStreaks; I++)
	{
		Cards->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.01f)), true);
	}
}

void UBirdWindStreaks::Respawn(FStreak& S, const FVector& CamLoc, const FVector& AirMs, bool bAnyAge)
{
	// anywhere round the camera, more of them upwind (they drift toward it)
	const FVector Up = AirMs.SizeSquared() > 0.01f ? -AirMs.GetSafeNormal() : FVector::ZeroVector;
	FVector R = Rng.GetUnitVector() * Radius * FMath::Pow(Rng.FRand(), 0.5f);
	R.Z *= 0.5f;
	S.Pos = CamLoc + R + Up * Radius * 0.4f * Rng.FRand();
	S.Life = Rng.FRandRange(2.f, 3.8f);
	S.Age = bAnyAge ? Rng.FRand() * S.Life : 0.f;
	S.Len = Rng.FRandRange(0.6f, 1.4f);
}

void UBirdWindStreaks::UpdateStreaks(const FVector& CamLoc, const FVector& AirMs, float Visible, float Dt)
{
	if (!Cards)
	{
		return;
	}
	const float AirSpeed = AirMs.Size();
	const float Show = Visible * FMath::Clamp((AirSpeed - 1.f) / 5.f, 0.f, 1.f);
	Cards->SetVisibility(Show > 0.01f);
	if (Show <= 0.01f)
	{
		return;
	}
	const FQuat Q = FRotationMatrix::MakeFromX(AirMs.GetSafeNormal()).ToQuat();
	const float LenCm = FMath::Clamp(AirSpeed * 35.f, 60.f, 420.f);
	TArray<FTransform> Xf;
	Xf.SetNum(Streaks.Num());
	for (int32 I = 0; I < Streaks.Num(); I++)
	{
		FStreak& S = Streaks[I];
		S.Age += Dt;
		S.Pos += AirMs * 100.f * Dt;
		if (S.Age > S.Life || FVector::DistSquared(S.Pos, CamLoc) > FMath::Square(Radius * 1.4f) || S.Life <= 0.f)
		{
			Respawn(S, CamLoc, AirMs, S.Life <= 1.f && S.Age == 0.f);
		}
		const float Fade = FMath::Sin(PI * FMath::Clamp(S.Age / S.Life, 0.f, 1.f));
		Xf[I] = FTransform(Q, S.Pos, FVector(LenCm * S.Len, 7.f, 1.f));
		Cards->SetCustomDataValue(I, 0, Fade * Show, false);
	}
	Cards->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
}
