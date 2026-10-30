#include "Winter/LyudWinter.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace
{
	const TCHAR* const SnowFlakesMaterialPath = TEXT("/Game/Winter/M_SnowFlakes.M_SnowFlakes");

	float SnowFxRand(uint32& S)
	{
		S = S * 1664525u + 1013904223u;
		return float(S >> 8) / 16777216.f;
	}
}

ALyudSnowFx::ALyudSnowFx()
{
	PrimaryActorTick.bCanEverTick = false;
	Flakes = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flakes"));
	RootComponent = Flakes;
	Flakes->SetMobility(EComponentMobility::Movable);
	Flakes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flakes->SetCastShadow(false);
	Flakes->SetReceivesDecals(false);
	Flakes->LightingChannels.bChannel0 = true;
	Flakes->LightingChannels.bChannel1 = true;
	Flakes->bUseAsOccluder = false;
}

void ALyudSnowFx::BuildMesh()
{
	// every flake a quad at the origin: UV0 its corner (-1..1), UV1 three random numbers (where in the box, speed, sway)
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
	VUV.SetNumChannels(3);
	const FPolygonGroupID PG = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Flakes");
	uint32 Seed = 1947u;
	const FVector2f Corners[4] = { FVector2f(-1, -1), FVector2f(1, -1), FVector2f(1, 1), FVector2f(-1, 1) };
	for (int32 F = 0; F < Count; F++)
	{
		const FVector2f R1(SnowFxRand(Seed), SnowFxRand(Seed));
		const FVector2f R2(SnowFxRand(Seed), SnowFxRand(Seed));
		FVertexInstanceID Q[4];
		for (int32 K = 0; K < 4; K++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = FVector3f::ZeroVector;
			Q[K] = MD.CreateVertexInstance(V);
			VNrm[Q[K]] = FVector3f::UpVector;
			VTan[Q[K]] = FVector3f::XAxisVector;
			VSign[Q[K]] = 1.f;
			VUV.Set(Q[K], 0, Corners[K]);
			VUV.Set(Q[K], 1, R1);
			VUV.Set(Q[K], 2, R2);
		}
		MD.CreateTriangle(PG, { Q[0], Q[2], Q[1] });
		MD.CreateTriangle(PG, { Q[0], Q[3], Q[2] });
	}
	UStaticMesh* Mesh = NewObject<UStaticMesh>(this, TEXT("SM_LyudSnowFlakes"), RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Flakes")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Mesh->BuildFromMeshDescriptions({ &MD }, Params);
	Mesh->SetExtendedBounds(FBoxSphereBounds(FBox(FVector(-BoxCm, -BoxCm, -BoxHeightCm), FVector(BoxCm, BoxCm, BoxHeightCm))));
	Flakes->SetStaticMesh(Mesh);
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, SnowFlakesMaterialPath))
	{
		MID = UMaterialInstanceDynamic::Create(M, this);
		Flakes->SetMaterial(0, MID);
	}
}

void ALyudSnowFx::Drive(const FVector& Camera, float Density, const FVector& Wind, const FLinearColor& Style, float DeltaSeconds)
{
	if (!Flakes->GetStaticMesh())
	{
		BuildMesh();
	}
	// ease in and out (a shower's edge, a forced change)
	const float K = 1.f - FMath::Exp(-DeltaSeconds / 3.f);
	Shown += (Density - Shown) * K;
	ShownStyle += (Style - ShownStyle) * K;
	const bool bShow = Shown > 0.005f && MID;
	Flakes->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	SetActorLocation(Camera);
	MID->SetVectorParameterValue(TEXT("Box"), FLinearColor(BoxCm, BoxCm, BoxHeightCm, Shown));
	MID->SetVectorParameterValue(TEXT("Wind"), FLinearColor(float(Wind.X), float(Wind.Y), 0.f, 0.f));
	MID->SetVectorParameterValue(TEXT("Style"), ShownStyle);
}
