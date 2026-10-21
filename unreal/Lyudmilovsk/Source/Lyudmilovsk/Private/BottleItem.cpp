#include "BottleItem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

namespace
{
	struct FLathe
	{
		TArray<FVector3f> Pos, Nrm;
		TArray<FVector2f> UV;
		TArray<FIntVector3> Tris;
	};

	/**
	 * A surface of revolution about Z from a profile of (radius, z) points, cm. The arc starts at StartAngle (radians about Z,
	 * 0 = +X) and covers Span; U runs along the arc, V along the profile (0..1 by height).
	 */
	FLathe Lathe(const TArray<FVector2f>& Profile, float StartAngle, float Span, int32 Segments, float ZMin, float ZMax)
	{
		FLathe L;
		const int32 N = Profile.Num();
		const bool bFull = Span >= 2.f * PI - 1e-3f;
		const int32 Cols = bFull ? Segments : Segments + 1;
		for (int32 I = 0; I < N; ++I)
		{
			const FVector2f Prev = Profile[FMath::Max(I - 1, 0)], Next = Profile[FMath::Min(I + 1, N - 1)];
			const FVector2f T = Next - Prev;
			FVector2f Out(T.Y, -T.X);
			Out.Normalize();
			for (int32 C = 0; C < Cols; ++C)
			{
				const float A = StartAngle + Span * C / Segments;
				const float Ca = FMath::Cos(A), Sa = FMath::Sin(A);
				L.Pos.Add(FVector3f(Profile[I].X * Ca, Profile[I].X * Sa, Profile[I].Y));
				L.Nrm.Add(FVector3f(Out.X * Ca, Out.X * Sa, Out.Y));
				L.UV.Add(FVector2f(float(C) / Segments, 1.f - (Profile[I].Y - ZMin) / FMath::Max(ZMax - ZMin, 0.01f)));
			}
		}
		for (int32 I = 0; I + 1 < N; ++I)
		{
			for (int32 C = 0; C < Segments; ++C)
			{
				const int32 C1 = bFull ? (C + 1) % Cols : C + 1;
				const int32 A = I * Cols + C, B = I * Cols + C1, Up = (I + 1) * Cols + C, UpB = (I + 1) * Cols + C1;
				// front face where (third - first) x (second - first) points out: (a, up, b) and (b, up, upb) for a profile going up
				L.Tris.Add(FIntVector3(A, Up, B));
				L.Tris.Add(FIntVector3(B, Up, UpB));
			}
		}
		return L;
	}

	UStaticMesh* BuildMesh(UObject* Outer, FName Name, const FLathe& D)
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
		for (int32 I = 0; I < D.Pos.Num(); ++I)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			VTan[VI] = FVector3f::CrossProduct(FVector3f::UpVector, N).GetSafeNormal(UE_SMALL_NUMBER, FVector3f::XAxisVector);
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tris)
		{
			MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}

	struct FBottleMeshes
	{
		UStaticMesh* Glass = nullptr;
		UStaticMesh* Liquid = nullptr;
		UStaticMesh* Label = nullptr;
		UStaticMesh* Cap = nullptr;
	};

	/** Built once and kept (rooted): every bottle shares them. z = 0 at the middle of the bottle. */
	const FBottleMeshes& Meshes()
	{
		static FBottleMeshes M;
		if (M.Glass)
		{
			return M;
		}
		constexpr float H = ABottleItem::BottleHalfLength;
		auto Z = [](float Z0) { return Z0 - ABottleItem::BottleHalfLength; };
		// outside: a 0.5 l soda bottle, 22.4 cm tall, 6.4 wide, a slim neck with a lip
		const TArray<FVector2f> Outer = {
			{ 0.f, Z(0.f) }, { 2.3f, Z(0.f) }, { 2.9f, Z(0.3f) }, { 3.2f, Z(1.1f) }, { 3.2f, Z(13.f) }, { 3.1f, Z(14.f) }, { 2.7f, Z(15.2f) },
			{ 2.0f, Z(16.4f) }, { 1.5f, Z(17.6f) }, { 1.3f, Z(18.6f) }, { 1.25f, Z(21.2f) }, { 1.5f, Z(21.5f) }, { 1.5f, Z(21.9f) }, { 1.1f, Z(22.0f) } };
		// the liquid: a column inside, from the bottom to the neck, built for a full bottle (the actor scales it in Z)
		const TArray<FVector2f> Inner = {
			{ 0.f, 0.f }, { 2.0f, 0.f }, { 2.7f, 0.6f }, { 2.95f, 1.4f }, { 2.95f, 12.8f }, { 2.8f, 13.8f }, { 2.4f, 14.8f }, { 1.6f, 16.2f }, { 1.1f, 17.4f }, { 1.0f, 18.4f }, { 0.f, 18.4f } };
		// the label band: 230 degrees around the front, a hair outside the glass
		const TArray<FVector2f> Band = { { 3.24f, Z(3.2f) }, { 3.24f, Z(11.2f) } };
		const TArray<FVector2f> Crown = { { 0.f, Z(22.0f) }, { 1.6f, Z(22.0f) }, { 1.65f, Z(21.5f) }, { 1.55f, Z(21.2f) }, { 1.55f, Z(21.0f) } };
		UObject* Pkg = GetTransientPackage();
		M.Glass = BuildMesh(Pkg, TEXT("Lyud_BottleGlass"), Lathe(Outer, 0.f, 2.f * PI, 36, Z(0.f), Z(22.f)));
		M.Liquid = BuildMesh(Pkg, TEXT("Lyud_BottleLiquid"), Lathe(Inner, 0.f, 2.f * PI, 28, 0.f, 18.4f));
		const float Span = FMath::DegreesToRadians(230.f);
		M.Label = BuildMesh(Pkg, TEXT("Lyud_BottleLabel"), Lathe(Band, -0.5f * Span, Span, 28, Z(3.2f), Z(11.2f)));
		M.Cap = BuildMesh(Pkg, TEXT("Lyud_BottleCap"), Lathe(Crown, 0.f, 2.f * PI, 20, Z(21.0f), Z(22.0f)));
		for (UStaticMesh* S : { M.Glass, M.Liquid, M.Label, M.Cap })
		{
			S->AddToRoot();
		}
		(void)H;
		return M;
	}

	struct FFlavorLook { FLinearColor Glass, Liquid, Cap; };
	FFlavorLook LookOf(FName Flavor)
	{
		if (Flavor == TEXT("dyushes")) return { FLinearColor(0.75f, 0.85f, 0.7f), FLinearColor(0.85f, 0.8f, 0.3f), FLinearColor(0.7f, 0.65f, 0.2f) };
		if (Flavor == TEXT("buratino")) return { FLinearColor(0.45f, 0.3f, 0.12f), FLinearColor(0.35f, 0.15f, 0.04f), FLinearColor(0.55f, 0.1f, 0.05f) };
		if (Flavor == TEXT("tarkhun")) return { FLinearColor(0.2f, 0.5f, 0.2f), FLinearColor(0.2f, 0.6f, 0.15f), FLinearColor(0.1f, 0.4f, 0.12f) };
		return { FLinearColor(0.1f, 0.35f, 0.15f), FLinearColor(0.12f, 0.3f, 0.1f), FLinearColor(0.7f, 0.7f, 0.65f) };   // baikal
	}

	UMaterialInterface* LoadMaterial(const TCHAR* Path, const TCHAR* Fallback)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		return M ? M : LoadObject<UMaterialInterface>(nullptr, Fallback);
	}
}

ABottleItem::ABottleItem()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto Make = [this, Root](const TCHAR* Name)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	};
	Glass = Make(TEXT("Glass"));
	Liquid = Make(TEXT("Liquid"));
	Label = Make(TEXT("Label"));
	Cap = Make(TEXT("Cap"));
}

void ABottleItem::Init(FName InFlavor, float InLevel)
{
	Flavor = InFlavor;
	const FBottleMeshes& M = Meshes();
	Glass->SetStaticMesh(M.Glass);
	Liquid->SetStaticMesh(M.Liquid);
	Label->SetStaticMesh(M.Label);
	Cap->SetStaticMesh(M.Cap);
	const FFlavorLook Look = LookOf(Flavor);
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto Tinted = [this, Basic](UStaticMeshComponent* C, const TCHAR* AssetPath, const FLinearColor& Color) -> UMaterialInstanceDynamic*
	{
		UMaterialInterface* Base = LoadMaterial(AssetPath, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base ? Base : Basic, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
		return MID;
	};
	Tinted(Glass, TEXT("/Game/Hall/M_BottleGlass.M_BottleGlass"), Look.Glass);
	LiquidMaterial = Tinted(Liquid, TEXT("/Game/Hall/M_BottleLiquid.M_BottleLiquid"), Look.Liquid);
	Tinted(Cap, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), Look.Cap);
	UMaterialInstanceDynamic* LabelMat = Tinted(Label, TEXT("/Game/Hall/M_BottleLabel.M_BottleLabel"), FLinearColor::White);
	if (UTexture2D* Tex = LyudText::LoadTexture(TEXT("Hall"), FString(TEXT("T_Label_")) + Flavor.ToString()))
	{
		LabelMat->SetTextureParameterValue(TEXT("Label"), Tex);
	}
	SetLevel(InLevel);
}

void ABottleItem::SetLevel(float InLevel)
{
	Level = FMath::Clamp(InLevel, 0.f, 1.f);
	// the liquid column was built from its own bottom (z 0): its bottom stays put, its height follows the level
	Liquid->SetRelativeLocation(FVector(0.f, 0.f, -BottleHalfLength + 0.5f));
	Liquid->SetRelativeScale3D(FVector(1.f, 1.f, FMath::Max(Level, 0.001f)));
	Liquid->SetVisibility(Level > 0.01f);
}

FReachItem ABottleItem::MakeReachItem() const
{
	FReachItem R;
	R.Actor = const_cast<ABottleItem*>(this);
	R.Axis = GetActorUpVector();
	R.Front = GetActorForwardVector();
	// the hand holds the body (1..13 cm from the base), not the neck: its centre is 3.6 cm below the bottle's middle
	R.Center = GetActorLocation() - R.Axis * 3.6f;
	R.HalfLength = 6.f;
	R.Radius = BodyRadius;
	R.MouthDistance = BottleHalfLength + 3.6f - 0.4f;
	return R;
}

void ABottleItem::LieDown(const FVector& Pos, const FVector& Neck)
{
	const FVector N = Neck.GetSafeNormal();
	SetActorLocationAndRotation(Pos, FRotationMatrix::MakeFromZX(N, FVector::UpVector).Rotator());
}
