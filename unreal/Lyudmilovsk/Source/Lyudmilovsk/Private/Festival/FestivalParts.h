// The festivals' cloth and specks: one-section runtime meshes whose vertices carry what their materials move them by
// (M_FestivalCloth: pennants and flags in the wind; M_PoplarFluff: the poplars' fluff), so nothing is rebuilt or moved on
// the game thread (docs/festivals.md "Cloth in the wind"). Built like the fountain's parts (LyudFountain.cpp FPartMesh).
#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

#include <initializer_list>

/** One section with five UV channels. */
struct FLyudPartMesh
{
	static constexpr int32 NumUV = 5;
	TArray<FVector3f> Pos, Nrm;
	TArray<FVector2f> UV[NumUV];
	TArray<int32> Idx;

	int32 Vert(const FVector& P, const FVector& N, std::initializer_list<FVector2f> UVs)
	{
		Pos.Add(FVector3f(P));
		Nrm.Add(FVector3f(N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)));
		int32 K = 0;
		for (const FVector2f& U : UVs)
		{
			if (K < NumUV)
			{
				UV[K++].Add(U);
			}
		}
		for (; K < NumUV; K++)
		{
			UV[K].Add(FVector2f::ZeroVector);
		}
		return Pos.Num() - 1;
	}
	void Tri(int32 A, int32 B, int32 C) { Idx.Append({ A, B, C }); }
	void Quad(int32 A, int32 B, int32 C, int32 D) { Idx.Append({ A, B, C, A, C, D }); }
	bool IsEmpty() const { return Idx.Num() == 0; }
	int32 NumTriangles() const { return Idx.Num() / 3; }

	UStaticMesh* Build(UObject* Outer, FName Name) const
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(NumUV);
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name),
			RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		TArray<FVertexInstanceID> Inst;
		Inst.Reserve(Pos.Num());
		for (int32 I = 0; I < Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = Nrm[I];
			VNrm[VI] = N;
			// any tangent square to the normal (the materials use no normal map)
			const FVector3f Ref = FMath::Abs(N.Z) < 0.9f ? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
			VTan[VI] = FVector3f::CrossProduct(Ref, N).GetSafeNormal();
			VSign[VI] = -1.f;
			for (int32 K = 0; K < NumUV; K++)
			{
				VUV.Set(VI, K, UV[K][I]);
			}
			Inst.Add(VI);
		}
		for (int32 I = 0; I + 2 < Idx.Num(); I += 3)
		{
			// counterclockwise in a right-handed frame; Unreal's front faces wind the other way
			MD.CreateTriangle(PG, { Inst[Idx[I]], Inst[Idx[I + 2]], Inst[Idx[I + 1]] });
		}
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}
};

/**
 * Cloth for M_FestivalCloth (unreal/scripts/make_festival_materials.py). Per vertex: UV0 = (along: 0 held .. 1 the loose
 * end, phase 0..1), UV1 = (hang: cm this vertex hangs under what holds it, so it swings out on that arc; flutter: cm of
 * ripple here), UV2 = (arm: cm from the pole of a flag, built along world +X and turned downwind by the
 * material; 0 for what does not turn), UV3 = (red, green), UV4 = (blue, 0). The material is two-sided.
 */
struct FLyudCloth : FLyudPartMesh
{
	/** A pennant: its top edge P0..P1 held by the line, its tip hanging loose. */
	void Pennant(const FVector& P0, const FVector& P1, const FVector& Tip, const FLinearColor& C, float Phase)
	{
		const FVector N = FVector::CrossProduct(P1 - P0, Tip - P0);
		const float Hang = float(((P0 + P1) * 0.5 - Tip).Size());
		const FVector2f RG(C.R, C.G), B(C.B, 0.f);
		const int32 A = Vert(P0, N, { FVector2f(0.f, Phase), FVector2f::ZeroVector, FVector2f::ZeroVector, RG, B });
		const int32 Bv = Vert(P1, N, { FVector2f(0.f, Phase), FVector2f::ZeroVector, FVector2f::ZeroVector, RG, B });
		const int32 T = Vert(Tip, N, { FVector2f(1.f, Phase), FVector2f(Hang, 4.f), FVector2f::ZeroVector, RG, B });
		Tri(A, Bv, T);
	}

	/** A flag on a pole: its hoist from Top down by Height at the pole, Length long; it flies downwind. */
	void Flag(const FVector& Top, float Length, float Height, const FLinearColor& C, float Phase, int32 Cols = 6)
	{
		const FVector2f RG(C.R, C.G), B(C.B, 0.f);
		const int32 Base = Pos.Num();
		for (int32 I = 0; I <= Cols; I++)
		{
			const float U = float(I) / Cols;
			for (int32 J = 0; J < 2; J++)
			{
				Vert(Top + FVector(U * Length, 0.f, -J * Height), FVector(0.f, -0.5f, 1.f),
					{ FVector2f(U, Phase), FVector2f(0.f, 14.f * U), FVector2f(U * Length, 0.f), RG, B });
			}
		}
		for (int32 I = 0; I < Cols; I++)
		{
			const int32 K = Base + I * 2;
			Quad(K + 1, K + 3, K + 2, K);
		}
	}

	/** A streamer (a kite's tail, a ribbon): Segs quads from From, Length long hanging down, Width wide; loose all along. */
	void Ribbon(const FVector& From, float Length, float Width, const FLinearColor& C, float Phase, int32 Segs = 5)
	{
		const FVector2f RG(C.R, C.G), B(C.B, 0.f);
		const int32 Base = Pos.Num();
		for (int32 I = 0; I <= Segs; I++)
		{
			const float U = float(I) / Segs;
			for (int32 J = 0; J < 2; J++)
			{
				Vert(From + FVector((J - 0.5f) * Width, 0.f, -U * Length), FVector(0.f, -1.f, 0.2f),
					{ FVector2f(U, Phase), FVector2f(U * Length, 6.f * U), FVector2f::ZeroVector, RG, B });
			}
		}
		for (int32 I = 0; I < Segs; I++)
		{
			const int32 K = Base + I * 2;
			Quad(K, K + 1, K + 3, K + 2);
		}
	}
};

/** What the cloth's users share: its material, how its components are set up, the festive palette. */
namespace LyudFestParts
{
	inline const TCHAR* ClothPath = TEXT("/Game/Festival/M_FestivalCloth.M_FestivalCloth");
	inline const TCHAR* BasicPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// the pennants' colours: the festive palette with the Big Sun's gold and orange
	inline const FLinearColor ClothPalette[] = {
		FLinearColor(0.80f, 0.06f, 0.05f), FLinearColor(0.95f, 0.70f, 0.05f), FLinearColor(0.05f, 0.25f, 0.75f),
		FLinearColor(0.10f, 0.55f, 0.15f), FLinearColor(0.90f, 0.88f, 0.82f), FLinearColor(0.95f, 0.38f, 0.04f),
	};

	inline UMaterialInterface* LoadCloth()
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, ClothPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M)
		{
			// not built on this machine yet (unreal/scripts/make_festival_materials.py): the cloth hangs still and grey
			UE_LOG(LogTemp, Warning, TEXT("Festivals: %s is missing, the pennants hang still (run make_festival_materials.py)"), ClothPath);
			M = LoadObject<UMaterialInterface>(nullptr, BasicPath);
		}
		return M;
	}

	/** Cloth and specks: seen, never touched, never in the way of the light. */
	inline void ClothComponent(UStaticMeshComponent* C, float DrawDistance)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetVisibleInRayTracing(false);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCachedMaxDrawDistance(DrawDistance);
		// far off the cloth stands still: the vertex shader's work is for what can be seen moving
		C->WorldPositionOffsetDisableDistance = 15000;
	}

}
