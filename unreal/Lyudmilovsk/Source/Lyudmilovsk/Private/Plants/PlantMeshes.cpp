#include "Plants/PlantMeshes.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Plants/PlantModel.h"
#include "StaticMeshAttributes.h"

namespace
{
	UStaticMesh* BuildMesh(UObject* Outer, FName Name, const TArray<FVector3f>& Pos, const TArray<FVector3f>& Nrm, const TArray<FVector2f>& UV,
	                       const TArray<FIntVector3>& Tris, UMaterialInterface* Material)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		VUV.SetNumChannels(1);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
		TArray<FVertexInstanceID> Inst;
		for (int32 I = 0; I < Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			const FVector3f Ref = FMath::Abs(N.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector;
			VTan[VI] = FVector3f::CrossProduct(Ref, N).GetSafeNormal();
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, UV[I]);
			Inst.Add(VI);
		}
		for (const FIntVector3& T : Tris)
		{
			MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, Name));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		}
		return Mesh;
	}
}

UStaticMesh* PlantMeshes::BuildBark(UObject* Outer, UMaterialInterface* Material)
{
	constexpr int32 Sides = 7;
	constexpr float TopRadius = 0.9f;
	TArray<FVector3f> Pos, Nrm;
	TArray<FVector2f> UV;
	TArray<FIntVector3> Tris;
	for (int32 K = 0; K <= Sides; K++)
	{
		const float A = 2.f * PI * K / Sides;
		const FVector3f R(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector3f N = (R + FVector3f(0.f, 0.f, 1.f - TopRadius)).GetSafeNormal();
		Pos.Add(R); Nrm.Add(N); UV.Add(FVector2f((float)K / Sides, 0.f));
		Pos.Add(R * TopRadius + FVector3f(0.f, 0.f, 1.f)); Nrm.Add(N); UV.Add(FVector2f((float)K / Sides, 1.f));
	}
	for (int32 K = 0; K < Sides; K++)
	{
		const int32 B0 = 2 * K, T0 = B0 + 1, B1 = B0 + 2, T1 = B0 + 3;
		// Unreal's front face: (P2 - P0) x (P1 - P0) points out
		Tris.Add(FIntVector3(B0, T0, B1));
		Tris.Add(FIntVector3(B1, T0, T1));
	}
	return BuildMesh(Outer, TEXT("SM_PlantBark"), Pos, Nrm, UV, Tris, Material);
}

UStaticMesh* PlantMeshes::BuildLeafCluster(UObject* Outer, UMaterialInterface* Material)
{
	constexpr int32 Leaves = 9;
	TArray<FVector3f> Pos, Nrm;
	TArray<FVector2f> UV;
	TArray<FIntVector3> Tris;
	for (int32 L = 0; L < Leaves; L++)
	{
		// petiole on a small sphere about the centre, the blade pointing outward and drooping a little
		const float Z = 1.f - 2.f * (L + 0.5f) / Leaves;
		const float Phi = L * 2.39996323f;
		const float Rr = FMath::Sqrt(1.f - Z * Z);
		const FVector3f Out(Rr * FMath::Cos(Phi), Rr * FMath::Sin(Phi), Z);
		const FVector3f Base = Out * 0.12f;
		const FVector3f Along = (Out + FVector3f(0.f, 0.f, -0.35f)).GetSafeNormal();
		const FVector3f Ref = FMath::Abs(Along.Z) < 0.9f ? FVector3f::UpVector : FVector3f::XAxisVector;
		FVector3f Across = FVector3f::CrossProduct(Ref, Along).GetSafeNormal();
		// twist each blade about its midrib so the cluster catches light from every side
		const float Twist = (PlantModel::Hash01(7, L, 0) - 0.5f) * 1.6f;
		const FVector3f Face = FVector3f::CrossProduct(Along, Across);
		Across = Across * FMath::Cos(Twist) + Face * FMath::Sin(Twist);
		// deltoid blade: widest near the base, a long pointed tip
		const FVector3f Q[4] = { Base, Base + Along * 0.13f + Across * 0.11f, Base + Along * 0.36f, Base + Along * 0.13f - Across * 0.11f };
		const FVector2f QUV[4] = { FVector2f(0.5f, 0.f), FVector2f(1.f, 0.35f), FVector2f(0.5f, 1.f), FVector2f(0.f, 0.35f) };
		for (int32 Side = 0; Side < 2; Side++)
		{
			const int32 I0 = Pos.Num();
			for (int32 K = 0; K < 4; K++)
			{
				Pos.Add(Q[K]);
				Nrm.Add((Q[K] + FVector3f(0.f, 0.f, 0.15f)).GetSafeNormal());
				UV.Add(QUV[K]);
			}
			if (Side == 0)
			{
				Tris.Add(FIntVector3(I0, I0 + 1, I0 + 2));
				Tris.Add(FIntVector3(I0, I0 + 2, I0 + 3));
			}
			else
			{
				Tris.Add(FIntVector3(I0, I0 + 2, I0 + 1));
				Tris.Add(FIntVector3(I0, I0 + 3, I0 + 2));
			}
		}
	}
	return BuildMesh(Outer, TEXT("SM_PlantLeafCluster"), Pos, Nrm, UV, Tris, Material);
}
