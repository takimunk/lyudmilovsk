#include "BirdSelfBody.h"

#include "Birds/BirdSpecies.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"

const FName UBirdSelfBody::FeatherMaterialPath(TEXT("/Game/Birds/Flight/M_BirdFeather.M_BirdFeather"));
const FName UBirdSelfBody::BodyMaterialPath(TEXT("/Game/Birds/Flight/M_BirdBody.M_BirdBody"));

namespace
{
	using namespace BirdLook;

	// ------------------------------------------------------------------ meshes built in code

	struct FMeshData
	{
		TArray<FVector3f> Pos, Nrm;
		TArray<FVector2f> UV;
		TArray<FLinearColor> Col;
		TArray<FIntVector3> Tri;
		int32 Add(const FVector3f& P, const FVector3f& N, const FVector2f& T, const FLinearColor& C = FLinearColor::White)
		{
			Pos.Add(P); Nrm.Add(N); UV.Add(T); Col.Add(C);
			return Pos.Num() - 1;
		}
	};

	UStaticMesh* MakeMesh(UObject* Outer, const TCHAR* Name, const FMeshData& D, UMaterialInterface* Material)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> VCol = Attr.GetVertexInstanceColors();
		VUV.SetNumChannels(1);
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Bird");
		TArray<FVertexInstanceID> Inst;
		for (int32 I = 0; I < D.Pos.Num(); I++)
		{
			const FVertexID V = MD.CreateVertex();
			VPos[V] = D.Pos[I];
			const FVertexInstanceID VI = MD.CreateVertexInstance(V);
			const FVector3f N = D.Nrm[I].GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
			VNrm[VI] = N;
			FVector3f T = FVector3f::XAxisVector - N * N.X;
			if (T.SizeSquared() < 1e-4f) T = FVector3f::YAxisVector - N * N.Y;
			VTan[VI] = T.GetSafeNormal();
			VSign[VI] = 1.f;
			VUV.Set(VI, 0, D.UV[I]);
			VCol[VI] = FVector4f(D.Col[I]);
			Inst.Add(VI);
		}
		for (const FIntVector3& T : D.Tri)
		{
			// Unreal's front face normal is (P2 - P0) x (P1 - P0): wind every triangle to face the way its
			// authored normals point (a shell seen from inside lights backwards and glows against the sun)
			const FVector3f Front = FVector3f::CrossProduct(D.Pos[T.Z] - D.Pos[T.X], D.Pos[T.Y] - D.Pos[T.X]);
			const FVector3f Want = D.Nrm[T.X] + D.Nrm[T.Y] + D.Nrm[T.Z];
			if (FVector3f::DotProduct(Front, Want) >= 0.f)
			{
				MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Y], Inst[T.Z] });
			}
			else
			{
				MD.CreateTriangle(PG, { Inst[T.X], Inst[T.Z], Inst[T.Y] });
			}
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Bird")));
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

	/** Unreal's front face is clockwise seen from the normal's side: (B - A) x (C - A) points against N. */
	void Quad(FMeshData& D, int32 A, int32 B, int32 C, int32 E)
	{
		D.Tri.Add(FIntVector3(A, C, B));
		D.Tri.Add(FIntVector3(A, E, C));
	}

	/**
	 * One flight feather, 1 cm long along +X from the root, 1 cm at its widest, the vane cambered down
	 * at the edges and the shaft curving down to the tip (the Z scale is the width). UV x along the
	 * feather, y across it with the shaft at 0.42 (the outer vane, +Y, is the narrow one).
	 */
	UStaticMesh* FeatherMesh(UObject* Outer, UMaterialInterface* Material)
	{
		FMeshData D;
		constexpr int32 NU = 14, NV = 7;
		constexpr float Shaft = 0.42f;
		auto Width = [](float U)
		{
			float W = U < 0.07f ? 0.06f : FMath::Min(1.f, 0.55f + (U - 0.07f) * 2.2f);
			if (U > 0.72f) W *= FMath::Sqrt(FMath::Max(0.f, 1.f - FMath::Square((U - 0.72f) / 0.28f)));
			return FMath::Max(W, 0.012f);
		};
		for (int32 I = 0; I <= NU; I++)
		{
			const float U = (float)I / NU;
			const float W = Width(U);
			const float Bow = -0.05f * U * U;   // the shaft curves down to the tip
			for (int32 J = 0; J <= NV; J++)
			{
				const float V = (float)J / NV;
				// across: from the inner edge (-Y, broad) through the shaft to the outer edge (+Y, narrow)
				const float S = V < Shaft ? -(Shaft - V) / Shaft : (V - Shaft) / (1.f - Shaft);
				const float Y = V < Shaft ? S * W * 0.58f : S * W * 0.42f;
				const float Z = Bow - 0.09f * S * S * W;
				const FVector3f N(0.05f * U, 0.18f * S, 1.f);
				D.Add(FVector3f(U, Y, Z), N, FVector2f(U, V));
			}
		}
		for (int32 I = 0; I < NU; I++)
		{
			for (int32 J = 0; J < NV; J++)
			{
				const int32 A = I * (NV + 1) + J;
				Quad(D, A, A + NV + 1, A + NV + 2, A + 1);
			}
		}
		return MakeMesh(Outer, TEXT("SM_SelfFeather"), D, Material);
	}

	/** An ellipsoid (semi-axes R), coloured Top above and Bottom below, closed. */
	UStaticMesh* EllipsoidMesh(UObject* Outer, const TCHAR* Name, const FVector3f& R, const FLinearColor& Top, const FLinearColor& Bottom, UMaterialInterface* Material)
	{
		FMeshData D;
		constexpr int32 NA = 16, NB = 12;
		for (int32 I = 0; I <= NB; I++)
		{
			const float B = PI * I / NB;          // from the front (+X) to the back
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				const FVector3f U(FMath::Cos(B), FMath::Sin(B) * FMath::Cos(A), FMath::Sin(B) * FMath::Sin(A));
				const FVector3f P = U * R;
				const FVector3f N = FVector3f(U.X / R.X, U.Y / R.Y, U.Z / R.Z);
				const float Up = FMath::Clamp(U.Z * 2.f + 0.3f, 0.f, 1.f);
				D.Add(P, N, FVector2f((float)J / NA, (float)I / NB), FMath::Lerp(Bottom, Top, Up));
			}
		}
		for (int32 I = 0; I < NB; I++)
		{
			for (int32 J = 0; J < NA; J++)
			{
				const int32 A = I * (NA + 1) + J;
				Quad(D, A, A + 1, A + NA + 2, A + NA + 1);
			}
		}
		return MakeMesh(Outer, Name, D, Material);
	}

	/** The beak: upper and lower mandible as one wedge along +X from the base, Len long, Depth deep at
	 *  the base, tip pulled down by Hook; Base colour at the root, Tip colour at the point. */
	UStaticMesh* BeakMesh(UObject* Outer, float Len, float Depth, float Hook, const FLinearColor& Base, const FLinearColor& Tip, UMaterialInterface* Material)
	{
		FMeshData D;
		constexpr int32 NU = 10, NA = 18;
		for (int32 I = 0; I <= NU; I++)
		{
			const float U = (float)I / NU;
			const float Taper = FMath::Pow(1.f - U, 0.85f);
			const float H = Depth * 0.5f * Taper + 0.002f;
			const float W = Depth * 0.42f * Taper + 0.002f;
			const float Drop = -Hook * 0.22f * Len * U * U - Depth * 0.08f * U;
			const FLinearColor Col = FMath::Lerp(Base, Tip, FMath::SmoothStep(0.15f, 0.7f, U));
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				const float Y = W * FMath::Cos(A), Z = H * FMath::Sin(A);
				D.Add(FVector3f(U * Len, Y, Z + Drop), FVector3f(0.25f, FMath::Cos(A) / W, FMath::Sin(A) / H), FVector2f(U, (float)J / NA), Col);
			}
		}
		for (int32 I = 0; I < NU; I++)
		{
			for (int32 J = 0; J < NA; J++)
			{
				const int32 A = I * (NA + 1) + J;
				Quad(D, A, A + NA + 1, A + NA + 2, A + 1);
			}
		}
		return MakeMesh(Outer, TEXT("SM_SelfBeak"), D, Material);
	}

	/** A tapered rod along +X, 1 long, radius 0.5 at the root and 0.35 at the end (legs and toes). */
	UStaticMesh* RodMesh(UObject* Outer, const FLinearColor& Col, UMaterialInterface* Material)
	{
		FMeshData D;
		constexpr int32 NA = 6;
		for (int32 I = 0; I <= 1; I++)
		{
			const float R = I ? 0.35f : 0.5f;
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				D.Add(FVector3f(I, R * FMath::Cos(A), R * FMath::Sin(A)), FVector3f(0, FMath::Cos(A), FMath::Sin(A)), FVector2f(I, (float)J / NA), Col);
			}
		}
		for (int32 J = 0; J < NA; J++)
		{
			Quad(D, J, NA + 1 + J, NA + 2 + J, J + 1);
		}
		return MakeMesh(Outer, TEXT("SM_SelfRod"), D, Material);
	}

	FVector Dir2(float ThetaDeg, int8 Side)
	{
		// in the body's plane: 0 = straight out to the side, + swept back
		const float T = FMath::DegreesToRadians(ThetaDeg);
		return FVector(-FMath::Sin(T), Side * FMath::Cos(T), 0.f);
	}

	/** Primary lengths, inner (P1) to outer (P10), as a fraction of the longest. */
	float PrimaryProfile(EWing W, int32 I)
	{
		static const float Round[10] = { .62f, .66f, .71f, .77f, .84f, .92f, .98f, 1.f, .96f, .82f };
		static const float Pointed[10] = { .58f, .62f, .67f, .73f, .80f, .87f, .93f, .98f, 1.f, .97f };
		static const float Sickle[10] = { .40f, .46f, .53f, .61f, .69f, .77f, .85f, .92f, .98f, 1.f };
		static const float Broad[10] = { .66f, .70f, .74f, .79f, .85f, .92f, .98f, 1.f, .97f, .88f };
		const float* P = W == EWing::Pointed ? Pointed : W == EWing::Sickle ? Sickle : W == EWing::Broad ? Broad : Round;
		return P[FMath::Clamp(I, 0, 9)];
	}

	float TailProfile(ETail T, float M, bool& bStreamer)
	{
		// M: 0 at the centre pair .. 1 at the outermost
		bStreamer = false;
		switch (T)
		{
		case ETail::Notch: return 1.f + 0.08f * M;
		case ETail::Fork: return 1.f + 0.38f * M * M;
		case ETail::Streamers: if (M > 0.9f) { bStreamer = true; return 2.1f; } return 1.f + 0.25f * M;
		case ETail::Wedge: return 1.f - 0.35f * M;
		case ETail::Long: return 1.f - 0.55f * M;
		case ETail::Rounded: return 1.f - 0.14f * M * M;
		default: return 1.f;
		}
	}
}

UBirdSelfBody::UBirdSelfBody()
{
	PrimaryComponentTick.bCanEverTick = false;
}

namespace
{
	/** M_BirdBody painted: Mode 0 a shell (Top above, Bottom below), 1 a beak (root to tip), 2 one colour. */
	UMaterialInterface* Paint(UObject* Outer, UMaterialInterface* Base, const FLinearColor& Top, const FLinearColor& Bottom, float Mode, float Roughness = 0.6f, float Specular = 0.1f)
	{
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Top"), Top);
		M->SetVectorParameterValue(TEXT("Bottom"), Bottom);
		M->SetScalarParameterValue(TEXT("Mode"), Mode);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("Specular"), Specular);
		return M;
	}
}

UStaticMeshComponent* UBirdSelfBody::AddPart(UStaticMesh* Mesh, USceneComponent* Parent, FName Name)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(GetOwner(), Name);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(true);
	C->SetupAttachment(Parent);
	C->RegisterComponent();
	return C;
}

void UBirdSelfBody::Build(FName SpeciesId, float LengthCm, float WingspanCm, USceneComponent* Head)
{
	const FLook& Look = BirdLook::Get(SpeciesId);
	Wing = Look.Wing;
	TailShape = Look.TailShape;
	FeatherMaterial = LoadObject<UMaterialInterface>(nullptr, *FeatherMaterialPath.ToString());
	BodyMaterial = LoadObject<UMaterialInterface>(nullptr, *BodyMaterialPath.ToString());
	if (!FeatherMaterial || !BodyMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bird body: %s or %s missing (run unreal/scripts/make_bird_flight_assets.py)"), *FeatherMaterialPath.ToString(), *BodyMaterialPath.ToString());
	}

	// proportions from the field length: tail, beak, then the torso and head share the rest
	TailLen = LengthCm * Look.TailFrac;
	const float Rest = FMath::Max(LengthCm - TailLen - Look.BeakCm, LengthCm * 0.3f);
	L = Rest * 0.72f;
	const float HeadR = Rest * 0.15f;
	HalfW = L * 0.27f;
	HalfH = L * 0.29f;
	const float A = L * 0.5f;
	Shoulder = FVector(A * 0.42f, HalfW * 0.82f, HalfH * 0.5f);
	TailBase = FVector(-A * 0.88f, 0.f, HalfH * 0.12f);
	Hip = FVector(-A * 0.05f, HalfW * 0.4f, -HalfH * 0.55f);
	Neck = FVector(A * 0.75f, 0.f, HalfH * 0.45f);
	const FVector HeadCentre(A + HeadR * 0.55f, 0.f, HalfH * 0.62f);
	Eye = HeadCentre + FVector(HeadR * 0.15f, 0.f, HeadR * 0.35f);
	Radius = FMath::Max(HalfW, HalfH);

	HalfSpan = FMath::Max(WingspanCm * 0.5f - Shoulder.Y, LengthCm * 0.3f);
	float Frac[3];
	switch (Wing)
	{
	case EWing::Pointed: Frac[0] = .13f; Frac[1] = .19f; Frac[2] = .19f; break;
	case EWing::Sickle: Frac[0] = .07f; Frac[1] = .10f; Frac[2] = .20f; break;
	case EWing::Broad: Frac[0] = .16f; Frac[1] = .22f; Frac[2] = .16f; break;
	default: Frac[0] = .15f; Frac[1] = .20f; Frac[2] = .17f; break;
	}
	for (int32 I = 0; I < 3; I++) Bone[I] = HalfSpan * Frac[I];
	Primary = HalfSpan * (1.f - Frac[0] - Frac[1] - Frac[2]) * 1.02f;
	Secondary = Wing == EWing::Sickle ? Primary * 0.24f : Wing == EWing::Broad ? Primary * 0.68f : Wing == EWing::Pointed ? Primary * 0.5f : Primary * 0.6f;

	// how the species beats and glides (from the table)
	const FBirdSpecies* Row = UBirdSpeciesLibrary::Find(SpeciesId);
	const EBirdFlightStyle Style = Row ? Row->Flight : EBirdFlightStyle::Flapping;
	StrokeAmp = Style == EBirdFlightStyle::Soaring ? 0.62f : Style == EBirdFlightStyle::Flicker ? 0.55f : Style == EBirdFlightStyle::Clapping ? 1.1f : 0.95f;
	GlideDihedral = Style == EBirdFlightStyle::Clapping ? 0.38f : Style == EBirdFlightStyle::Soaring ? 0.07f : Style == EBirdFlightStyle::Flicker ? -0.04f : 0.08f;

	const float Tarsus = FMath::Max(L * 0.32f, 0.8f);
	Stand = HalfH * 0.9f + Tarsus * 0.95f;

	// --- feathers
	Flight = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("FlightFeathers"));
	Contour = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("ContourFeathers"));
	UStaticMesh* Feather = FeatherMesh(this, FeatherMaterial);
	for (UInstancedStaticMeshComponent* C : { Flight.Get(), Contour.Get() })
	{
		C->SetStaticMesh(Feather);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->NumCustomDataFloats = FPaint::NumFloats;
		C->SetupAttachment(this);
		C->RegisterComponent();
	}
	BuildFeathers(Look);
	BuildBodyFeathers(Look);

	// --- the body under the feathers, the head (shown from outside), the beak on the head
	Shell = AddPart(EllipsoidMesh(this, TEXT("SM_SelfShell"), FVector3f(A * 0.88f, HalfW * 0.84f, HalfH * 0.84f), Look.Back.Paint.Base, Look.Breast.Paint.Base, BodyMaterial), this, TEXT("Shell"));
	Shell->SetMaterial(0, Paint(this, BodyMaterial, Look.Back.Paint.Base, Look.Breast.Paint.Base, 0.f, 0.95f, 0.08f));
	if (Head)
	{
		UStaticMeshComponent* HeadShell = AddPart(EllipsoidMesh(this, TEXT("SM_SelfHead"), FVector3f(HeadR * 1.1f, HeadR * 0.85f, HeadR * 0.9f), Look.Back.Paint.Base, Look.Breast.Paint.Base, BodyMaterial), Head, TEXT("HeadShell"));
		HeadShell->SetRelativeLocation(HeadCentre - Eye);
		HeadShell->SetMaterial(0, Paint(this, BodyMaterial, Look.Back.Paint.Base, Look.Breast.Paint.Base, 0.f, 0.95f, 0.08f));
		HeadShell->ComponentTags.Add(TEXT("BirdHead"));
		Beak = AddPart(BeakMesh(this, Look.BeakCm, Look.BeakCm * Look.BeakDepth, Look.BeakHook, Look.BeakBase, Look.Beak, BodyMaterial), Head, TEXT("Beak"));
		Beak->SetMaterial(0, Paint(this, BodyMaterial, Look.BeakBase, Look.Beak, 1.f, 0.35f, 0.35f));
		// below the eye line, so it sits at the bottom of the view like a bird's own beak
		Beak->SetRelativeLocationAndRotation(HeadCentre - Eye + FVector(HeadR * 0.75f, 0.f, -HeadR * 0.55f), FRotator(-24.f, 0.f, 0.f));
	}

	// --- legs: tarsus and four toes each side
	UStaticMesh* Rod = RodMesh(this, Look.Legs, BodyMaterial);
	UMaterialInterface* LegPaint = Paint(this, BodyMaterial, Look.Legs, Look.Legs, 2.f, 0.55f);
	LegRest.Reset();
	for (int8 Side : { (int8)-1, (int8)1 })
	{
		for (int32 K = 0; K < 5; K++)
		{
			UStaticMeshComponent* P = AddPart(Rod, this, *FString::Printf(TEXT("Leg%d_%d"), Side, K));
			const float Len = K == 0 ? Tarsus : Tarsus * (K == 4 ? 0.42f : 0.55f);
			const float Thick = K == 0 ? Tarsus * 0.13f : Tarsus * 0.07f;
			P->SetRelativeScale3D(FVector(Len, Thick, Thick));
			P->SetMaterial(0, LegPaint);
			LegParts.Add(P);
		}
	}
}

void UBirdSelfBody::BuildFeathers(const FLook& Look)
{
	Feathers.Reset();
	TArray<float> Data;
	Data.SetNumZeroed(FPaint::NumFloats);
	auto Add = [&](FFeather::EGroup Group, int8 Side, int32 Index, int32 Count, float Len, float Width, float Along, float Layer, const FPaint& Paint)
	{
		FFeather F;
		F.Group = Group; F.Side = Side; F.Index = Index; F.Count = Count;
		F.Length = Len; F.Width = Width; F.Along = Along; F.Layer = Layer;
		F.Phase = FMath::FRand() * 2.f * PI;
		F.Instance = Flight->AddInstance(FTransform::Identity);
		Paint.Write(Data.GetData());
		Flight->SetCustomData(F.Instance, Data);
		Feathers.Add(F);
	};
	const float Lay = L * 0.012f;
	for (int8 Side : { (int8)-1, (int8)1 })
	{
		const bool bSlots = Wing == EWing::Broad;
		for (int32 I = 0; I < 10; I++)
		{
			const float Len = Primary * PrimaryProfile(Wing, I);
			// fingered wings: the outer primaries narrow (emarginated)
			const float W = Primary * (bSlots && I >= 6 ? 0.16f : Wing == EWing::Sickle ? 0.16f : 0.25f);
			Add(FFeather::Primary, Side, I, 10, Len, W, I / 9.f, -I * Lay * 0.25f, Look.Primaries.For(I, 10));
		}
		const int32 NS = Wing == EWing::Sickle ? 7 : Wing == EWing::Broad ? 14 : 11;
		for (int32 J = 0; J < NS; J++)
		{
			Add(FFeather::Secondary, Side, J, NS, Secondary * (1.f + 0.04f * J / NS), Secondary * 0.5f, J / (NS - 1.f), J * Lay * 0.2f, Look.Secondaries.For(J, NS));
		}
		for (int32 K = 0; K < 3; K++)
		{
			Add(FFeather::Tertial, Side, K, 3, Secondary * (1.05f - 0.1f * K), Secondary * 0.45f, K / 2.f, Lay * (3.f + K * 0.3f), Look.Tertials.For(K, 3));
		}
		for (int32 J = 0; J < NS; J++)
		{
			Add(FFeather::GreaterCovert, Side, J, NS, Secondary * 0.56f, Secondary * 0.36f, J / (NS - 1.f), Lay * (2.f + J * 0.15f), Look.GreaterCoverts.For(J, NS));
		}
		for (int32 R = 0; R < 2; R++)
		{
			for (int32 J = 0; J < 10; J++)
			{
				Add(FFeather::LesserCovert, Side, R * 10 + J, 20, Secondary * (R ? 0.24f : 0.34f), Secondary * 0.3f, J / 9.f, Lay * (4.f + R * 0.8f + J * 0.1f), Look.LesserCoverts.For(J, 10));
			}
		}
		for (int32 I = 0; I < 10; I++)
		{
			Add(FFeather::PrimaryCovert, Side, I, 10, Primary * 0.34f * (0.8f + 0.2f * I / 9.f), Primary * 0.13f, I / 9.f, Lay * (2.f + I * 0.1f), Look.PrimaryCoverts.For(I, 10));
		}
		for (int32 K = 0; K < 3; K++)
		{
			Add(FFeather::Alula, Side, K, 3, Primary * (0.2f - 0.03f * K), Primary * 0.07f, K / 2.f, Lay * (5.f + K * 0.2f), Look.PrimaryCoverts.Paint);
		}
		for (int32 K = 0; K < 6; K++)
		{
			Add(FFeather::Scapular, Side, K, 6, Secondary * 0.62f, Secondary * 0.5f, K / 5.f, Lay * (7.f + K * 0.2f), Look.Scapulars.For(K, 6));
		}
	}
	for (int32 K = 0; K < 12; K++)
	{
		const float M = FMath::Abs(K - 5.5f) / 5.5f;
		bool bStreamer = false;
		const float Len = TailLen * TailProfile(TailShape, M, bStreamer);
		// the centre pair on top, the outer ones under them
		const int32 Outer = FMath::Abs(K - 5.5f) > 4.f ? 1 : 0;
		const FPaint& Paint = Outer ? Look.Tail.Outer : Look.Tail.Paint;
		Add(FFeather::Tail, 0, K, 12, Len, TailLen * (bStreamer ? 0.07f : 0.27f), K / 11.f, (1.f - M) * Lay * 4.f, Paint);
	}
}

void UBirdSelfBody::BuildBodyFeathers(const FLook& Look)
{
	TArray<float> Data;
	Data.SetNumZeroed(FPaint::NumFloats);
	const float A = L * 0.5f;
	constexpr int32 NU = 11, NV = 14;
	for (int32 I = 0; I < NU; I++)
	{
		const float U = (I + 0.3f) / NU;
		const float X = A * (0.92f - 1.8f * U);
		const float Rf = FMath::Sqrt(FMath::Max(0.05f, 1.f - FMath::Square(X / A)));
		for (int32 J = 0; J < NV; J++)
		{
			const float V = 2.f * PI * (J + 0.5f * (I & 1)) / NV;
			const FVector P(X, HalfW * Rf * FMath::Cos(V), HalfH * Rf * FMath::Sin(V));
			const FVector N = FVector(X / (A * A), P.Y / (HalfW * HalfW), P.Z / (HalfH * HalfH)).GetSafeNormal();
			// the feather lies on the body pointing back along it
			const FVector Back = (FVector(-1, 0, 0) - N * -N.X).GetSafeNormal();
			const float Len = L * (0.3f + 0.08f * FMath::Sin(V * 0.5f));
			const FVector Root = P + N * (L * 0.01f * (1.f - U));
			const FTransform T(FRotationMatrix::MakeFromXZ(Back, N).ToQuat(), Root - Back * Len * 0.25f, FVector(Len, L * 0.24f, L * 0.24f));
			const int32 Inst = Contour->AddInstance(T);
			const bool bUnder = FMath::Sin(V) < -0.12f;
			(bUnder ? Look.Breast.Paint : Look.Back.Paint).Write(Data.GetData());
			Data[8] = FMath::Max(Data[8], 0.f);
			Contour->SetCustomData(Inst, Data);
		}
	}
}

void UBirdSelfBody::SetFirstPerson(bool bFirstPerson)
{
	if (bFirstPerson == bFirstPersonNow)
	{
		return;
	}
	bFirstPersonNow = bFirstPerson;
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* C : Children)
	{
		if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(C))
		{
			P->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
		}
	}
}

void UBirdSelfBody::SetShown(bool bShown, bool bWithHead)
{
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* C : Children)
	{
		C->SetVisibility(bShown);
	}
	if (Beak)
	{
		TArray<USceneComponent*> HeadParts;
		if (USceneComponent* Head = Beak->GetAttachParent())
		{
			Head->GetChildrenComponents(true, HeadParts);
		}
		for (USceneComponent* C : HeadParts)
		{
			// the beak always shows; the head only from outside (the camera is inside it)
			if (C == Beak) C->SetVisibility(bShown);
			else if (C->ComponentHasTag(TEXT("BirdHead"))) C->SetVisibility(bShown && bWithHead);
		}
	}
}

void UBirdSelfBody::PoseWing(int8 Side, const FBirdBodyPose& Pose, float Time, TArray<FTransform>& Out)
{
	const float Spread = (Side < 0 ? Pose.SpreadL : Pose.SpreadR) * (1.f - Pose.Fold);
	const float Phase = Side < 0 ? Pose.PhaseL : Pose.PhaseR;
	const bool bBeat = (Side < 0 ? Pose.BeatL : Pose.BeatR) || Phase > 0.f;
	// the upstroke flexes the wrist (less for the long-winged gliders)
	const float Up = bBeat && Phase > 0.5f ? FMath::Sin(PI * (Phase - 0.5f) / 0.5f) : 0.f;
	const float Flex = Up * (Wing == EWing::Sickle ? 0.15f : Wing == EWing::Pointed ? 0.3f : 0.45f);
	const float E = Spread * (1.f - Flex * 0.6f);
	const float EHand = Spread * (1.f - Flex);

	const float HandSweep = Wing == EWing::Sickle ? 40.f : Wing == EWing::Pointed ? 22.f : Wing == EWing::Broad ? 6.f : 12.f;
	float Th[3] = { FMath::Lerp(58.f, 12.f, E), FMath::Lerp(-102.f, -12.f, E), FMath::Lerp(87.f, HandSweep, EHand) };
	// flare: the wings swing forward and the hand cups
	Th[0] = FMath::Lerp(Th[0], -12.f, Pose.Flare * E);
	Th[1] = FMath::Lerp(Th[1], -22.f, Pose.Flare * E);
	Th[2] = FMath::Lerp(Th[2], 2.f, Pose.Flare * E);

	FVector P0 = Shoulder * FVector(1, Side, 1);
	// seen from its own eye, the wing is drawn from beside the head (a view of it, not its anatomy)
	P0.X += ViewForward * (Eye.X - Shoulder.X + L * 0.55f);
	// folded, the wing lies on the flank, a little lower
	P0.Z -= (1.f - E) * HalfH * 0.25f;
	FVector Dir[3], Joint[4];
	Joint[0] = P0;
	for (int32 K = 0; K < 3; K++)
	{
		Dir[K] = Dir2(Th[K], Side);
		Joint[K + 1] = Joint[K] + Dir[K] * Bone[K];
	}

	// the stroke raises and lowers the whole wing about the shoulder; gliding it holds its dihedral
	float Phi = FMath::Lerp(0.f, GlideDihedral, E);
	if (bBeat)
	{
		Phi = StrokeAmp * FMath::Cos(2.f * PI * Phase) + 0.12f;
	}
	Phi += Pose.Flare * 0.45f;
	const float Elev = Side * Phi;
	const FQuat Raise(FVector::XAxisVector, Elev);
	// the downstroke pitches the leading edge down a little (pronation)
	const float Twist = bBeat ? -0.18f * FMath::Sin(2.f * PI * Phase) : 0.f;
	auto ToBody = [&](const FVector& Flat) { return P0 + Raise.RotateVector(Flat - P0); };
	const FVector Normal = Raise.RotateVector(FVector::UpVector);

	const float Speed01 = FMath::Clamp(Pose.Speed / 25.f, 0.f, 1.f);
	auto Place = [&](const FFeather& F, const FVector& FlatRoot, const FVector& FlatDir, float Outerness)
	{
		FVector D = Raise.RotateVector(FlatDir).GetSafeNormal();
		FVector N = Normal;
		// along the span the feather twists with the stroke; the tips bend up under the load
		const FVector Span = FVector::CrossProduct(D, N).GetSafeNormal();
		const float Bend = FMath::Clamp((Pose.Load - 0.6f) * 0.08f, -0.05f, 0.3f) * Outerness * E;
		const float Flutter = (Speed01 * Speed01 * 0.05f + Pose.Ruffle * 0.25f) * (0.3f + Outerness) * FMath::Sin(Time * (26.f + 9.f * F.Phase) + F.Phase * 7.f);
		D = D.RotateAngleAxis(FMath::RadiansToDegrees(Bend + Flutter), Span);
		N = N.RotateAngleAxis(FMath::RadiansToDegrees(Twist * Outerness), D);
		const FVector Root = ToBody(FlatRoot) + Normal * F.Layer;
		Out[F.Instance] = FTransform(FRotationMatrix::MakeFromXZ(D, N).ToQuat(), Root, FVector(F.Length, F.Width, F.Width));
	};

	const FVector Back(-1, 0, 0);
	for (const FFeather& F : Feathers)
	{
		if (F.Side != Side)
		{
			continue;
		}
		switch (F.Group)
		{
		case FFeather::Primary:
		{
			// fanned from pointing back (P1) to along the hand (P10) when spread; stacked when folded
			const float Fan = FMath::Lerp(62.f, 3.f, FMath::Pow(F.Along, 0.8f)) * EHand + (2.f + F.Index * 0.4f) * (1.f - EHand);
			const FVector Root = FMath::Lerp(Joint[2], Joint[3], 0.12f + 0.88f * F.Along);
			Place(F, Root, Dir[2].RotateAngleAxis(Side * Fan, FVector::UpVector), 0.4f + 0.6f * F.Along);
			break;
		}
		case FFeather::Secondary:
		case FFeather::GreaterCovert:
		{
			const FVector Root = FMath::Lerp(Joint[2], Joint[1], F.Along);
			const FVector D = (Back + FVector(0, Side * FMath::Lerp(0.04f, 0.16f * (1.f - F.Along), E), 0)).GetSafeNormal();
			// coverts sit ahead, over the secondaries' roots
			Place(F, F.Group == FFeather::GreaterCovert ? Root + FVector(Secondary * 0.12f, 0, 0) : Root, D, 0.25f);
			break;
		}
		case FFeather::Tertial:
		{
			const FVector Root = Joint[1] + (Joint[0] - Joint[1]) * (0.1f + 0.25f * F.Along);
			Place(F, Root, (Back + FVector(0, Side * 0.05f, 0)).GetSafeNormal(), 0.1f);
			break;
		}
		case FFeather::LesserCovert:
		{
			const int32 Row = F.Index / 10;
			const float T = (F.Index % 10) / 9.f;
			// along the arm's leading edge, from the shoulder out to the wrist
			const FVector Root = T < 0.45f ? FMath::Lerp(Joint[0], Joint[1], T / 0.45f) : FMath::Lerp(Joint[1], Joint[2], (T - 0.45f) / 0.55f);
			Place(F, Root + FVector(Secondary * (0.05f - Row * 0.18f), 0, 0), (Back + FVector(0, Side * 0.1f, 0)).GetSafeNormal(), 0.1f);
			break;
		}
		case FFeather::PrimaryCovert:
		{
			const float Fan = FMath::Lerp(70.f, 6.f, F.Along) * EHand + 3.f * (1.f - EHand);
			const FVector Root = FMath::Lerp(Joint[2], Joint[3], 0.05f + 0.75f * F.Along);
			Place(F, Root + FVector(Primary * 0.03f, 0, 0), Dir[2].RotateAngleAxis(Side * Fan, FVector::UpVector), 0.2f);
			break;
		}
		case FFeather::Alula:
			Place(F, Joint[2] + Dir[2] * Bone[2] * 0.1f * F.Along, Dir[2].RotateAngleAxis(Side * (12.f + 8.f * F.Along), FVector::UpVector), 0.6f);
			break;
		case FFeather::Scapular:
		{
			// on the back over the wing root; they follow the stroke only a little
			const FVector Root = Shoulder * FVector(1, Side * (0.75f - 0.1f * F.Along), 1) + FVector(-L * 0.25f * F.Along, 0, HalfH * 0.12f);
			const FVector D = (Back + FVector(0, Side * 0.12f, -0.05f)).GetSafeNormal();
			Out[F.Instance] = FTransform(FRotationMatrix::MakeFromXZ(D, FVector::UpVector).ToQuat(), Root + FVector(0, 0, F.Layer), FVector(F.Length, F.Width, F.Width));
			break;
		}
		default:
			break;
		}
	}
}

void UBirdSelfBody::PoseTail(const FBirdBodyPose& Pose, float Time, TArray<FTransform>& Out)
{
	const float FanHalf = 6.f + 52.f * FMath::Clamp(Pose.TailFan, 0.f, 1.f);
	const float Dip = -12.f * Pose.TailFan + 4.f * Pose.Fold;
	const float Speed01 = FMath::Clamp(Pose.Speed / 25.f, 0.f, 1.f);
	for (const FFeather& F : Feathers)
	{
		if (F.Group != FFeather::Tail)
		{
			continue;
		}
		const float S = (F.Index - 5.5f) / 5.5f;
		FVector D = FVector(-1, 0, 0).RotateAngleAxis(S * FanHalf, FVector::UpVector);
		D = D.RotateAngleAxis(Dip, FVector::RightVector);
		const float Flutter = (Speed01 * Speed01 * 2.f + Pose.Ruffle * 12.f) * FMath::Sin(Time * (22.f + 5.f * F.Phase) + F.Phase * 5.f);
		const FVector N = FVector::UpVector.RotateAngleAxis(Flutter, D);
		const FVector Root = TailBase + FVector(0, S * HalfW * 0.5f, F.Layer);
		Out[F.Instance] = FTransform(FRotationMatrix::MakeFromXZ(D, N).ToQuat(), Root, FVector(F.Length, F.Width, F.Width));
	}
}

void UBirdSelfBody::ApplyPose(const FBirdBodyPose& Pose, float Time)
{
	if (!Flight)
	{
		return;
	}
	TArray<FTransform> Out;
	Out.SetNum(Flight->GetInstanceCount());
	PoseWing(-1, Pose, Time, Out);
	PoseWing(1, Pose, Time, Out);
	PoseTail(Pose, Time, Out);
	Flight->BatchUpdateInstancesTransforms(0, Out, false, true, true);

	// legs: tucked back under the tail in flight, down and gripping when standing or landing
	const float Down = FMath::Clamp(Pose.Legs, 0.f, 1.f);
	for (int32 S = 0; S < 2; S++)
	{
		const int8 Side = S ? 1 : -1;
		const FVector HipP = Hip * FVector(1, Side, 1);
		const FVector Tarsus = FMath::Lerp(FVector(-0.95f, 0, -0.3f), FVector(0.1f, 0, -1.f), Down).GetSafeNormal();
		UStaticMeshComponent* T = LegParts[S * 5];
		const float TLen = T->GetRelativeScale3D().X;
		T->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromX(Tarsus).ToQuat(), HipP, T->GetRelativeScale3D()));
		const FVector Foot = HipP + Tarsus * TLen;
		for (int32 K = 1; K < 5; K++)
		{
			UStaticMeshComponent* Toe = LegParts[S * 5 + K];
			// three toes forward, spread, one back; curled in flight
			const float Yaw = K == 4 ? 180.f : (K - 2) * 22.f;
			FVector D = FVector(1, 0, 0).RotateAngleAxis(Yaw, FVector::UpVector);
			D = FMath::Lerp(FVector(0, 0, -1) + D * 0.3f, D, Down).GetSafeNormal();
			if (Down < 0.5f) D = FMath::Lerp(Tarsus, D, Down * 2.f).GetSafeNormal();
			Toe->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromX(D).ToQuat(), Foot, Toe->GetRelativeScale3D()));
		}
	}
}
