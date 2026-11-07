#include "Fishing/LyudFishing.h"

#include "../Sea/FishModel.h"
#include "../Sea/SeaLifeMeshes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "StaticMeshAttributes.h"
#include "UObject/ConstructorHelpers.h"
#include "World/LyudPrefetch.h"

// The rod is built in code from turned (lathe) profiles: cork grips, a reel seat, a tapered blank in segments with its rings,
// a spinning reel, a quill float, a spoon. Sizes in cm, after an ordinary 2.7 m telescopic float rod with a 2000 reel.
namespace
{
	constexpr float ButtFront = 14.f;      // the fore grip ends here; the blank starts
	constexpr float BlankBase = 0.62f;     // the blank's radius at the fore grip and at the tip
	constexpr float BlankTip = 0.09f;

	FString RodFishMeshPath(const FString& Id) { return FString::Printf(TEXT("/Game/Sea/Fish/SM_%s.SM_%s"), *Id, *Id); }
	FString RodFishSkinPath(const FString& Id) { return FString::Printf(TEXT("/Game/Sea/Fish/T_%s_D.T_%s_D"), *Id, *Id); }
	const TCHAR* RodFishBodyPath = TEXT("/Game/Sea/Underwater/M_LyudFish.M_LyudFish");
	const TCHAR* RodFishFinPath = TEXT("/Game/Sea/Underwater/M_LyudFishFin.M_LyudFishFin");
	const TCHAR* RodStandInPath = TEXT("/Game/Sea/M_SeaFish.M_SeaFish");

	/** An asset ULyudPrefetch said is ready: in memory, never loaded here; with lyud.Prefetch 0 (the old synchronous loads, A/B)
	 *  loaded now as everything else then is. */
	template <typename T>
	T* RodFind(const FString& Path)
	{
		if (T* Found = FindObject<T>(nullptr, *Path))
		{
			return Found;
		}
		static IConsoleVariable* Prefetch = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Prefetch"));
		return Prefetch && Prefetch->GetInt() == 0 ? LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	}

	/** A turned part about +X: Profile is (x, radius) from end to end (radius 0 closes an end; a closed loop makes a ring). */
	UStaticMesh* RodLathe(UObject* Outer, const TCHAR* Name, const TArray<FVector2f>& Profile, int32 Sides)
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
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Part");
		const int32 N = Profile.Num();
		// the profile's normals (in x, r), from its neighbours
		TArray<FVector2f> Nrm;
		Nrm.SetNum(N);
		for (int32 I = 0; I < N; I++)
		{
			const FVector2f A = Profile[FMath::Max(I - 1, 0)], B = Profile[FMath::Min(I + 1, N - 1)];
			const FVector2f T = (B - A).GetSafeNormal();
			Nrm[I] = FVector2f(-T.Y, T.X);
		}
		float Len = 0.f;
		TArray<float> V;
		V.SetNum(N);
		for (int32 I = 0; I < N; I++)
		{
			if (I) Len += (Profile[I] - Profile[I - 1]).Size();
			V[I] = Len;
		}
		TArray<FVertexInstanceID> Inst;
		Inst.SetNum(N * (Sides + 1));
		for (int32 I = 0; I < N; I++)
		{
			for (int32 S = 0; S <= Sides; S++)
			{
				const float A = 2.f * PI * S / Sides;
				const float C = FMath::Cos(A), Sn = FMath::Sin(A);
				const FVertexID Vx = MD.CreateVertex();
				VPos[Vx] = FVector3f(Profile[I].X, Profile[I].Y * C, Profile[I].Y * Sn);
				const FVertexInstanceID VI = MD.CreateVertexInstance(Vx);
				VNrm[VI] = FVector3f(Nrm[I].X, Nrm[I].Y * C, Nrm[I].Y * Sn).GetSafeNormal();
				VTan[VI] = FVector3f(0.f, -Sn, C);
				VSign[VI] = 1.f;
				VUV.Set(VI, 0, FVector2f(float(S) / Sides, Len > 0.f ? V[I] / Len : 0.f));
				Inst[I * (Sides + 1) + S] = VI;
			}
		}
		for (int32 I = 0; I + 1 < N; I++)
		{
			for (int32 S = 0; S < Sides; S++)
			{
				const FVertexInstanceID A = Inst[I * (Sides + 1) + S], B = Inst[I * (Sides + 1) + S + 1];
				const FVertexInstanceID C = Inst[(I + 1) * (Sides + 1) + S], D = Inst[(I + 1) * (Sides + 1) + S + 1];
				// outward faces: the winding as seen from outside (counter-clockwise in UE's left-handed frame)
				if (Profile[I].Y > 1e-4f || Profile[I + 1].Y > 1e-4f)
				{
					MD.CreateTriangle(PG, { A, C, B });
					MD.CreateTriangle(PG, { B, C, D });
				}
			}
		}
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Part")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}

	TArray<FVector2f> RodRing(float Radius, float Tube, int32 Steps = 8)
	{
		TArray<FVector2f> P;
		for (int32 I = 0; I <= Steps; I++)
		{
			const float A = 2.f * PI * I / Steps;
			// clockwise in (x, r), so the lathe's faces and normals point out of the tube
			P.Add(FVector2f(Tube * FMath::Cos(A), Radius - Tube * FMath::Sin(A)));
		}
		return P;
	}

	/** Radius of the blank at x (cm from the grip), tapering evenly by ratio. */
	float BlankR(float X)
	{
		const float T = FMath::Clamp((X - ButtFront) / (ALyudFishingRod::Length - ButtFront), 0.f, 1.f);
		return BlankBase * FMath::Pow(BlankTip / BlankBase, T);
	}
	float SegLen() { return (ALyudFishingRod::Length - ButtFront) / ALyudFishingRod::Segments; }

	void NoLumen(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetMobility(EComponentMobility::Movable);
		// it moves every frame: out of the Lumen scene and distance fields (the perf checklist)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCanEverAffectNavigation(false);
	}
}

ALyudFishingRod::ALyudFishingRod()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

UStaticMeshComponent* ALyudFishingRod::Part(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FLinearColor& Tint)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name, RF_Transient);
	C->SetupAttachment(Parent);
	NoLumen(C);
	C->SetStaticMesh(Mesh);
	C->SetCastShadow(true);
	if (UMaterialInstanceDynamic* M = Paint(Tint))
	{
		C->SetMaterial(0, M);
	}
	C->RegisterComponent();
	return C;
}

UMaterialInstanceDynamic* ALyudFishingRod::Paint(const FLinearColor& Tint)
{
	if (UMaterialInstanceDynamic** Found = Paints.Find(Tint))
	{
		return *Found;
	}
	if (!BasicMat)
	{
		// engine content, loaded once per rod (and the rod is built once per world: it is parked, not destroyed)
		BasicMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!BasicMat)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BasicMat, this);
	M->SetVectorParameterValue(TEXT("Color"), Tint);
	Mids.Add(M);
	Paints.Add(Tint, M);
	return M;
}

void ALyudFishingRod::BeginPlay()
{
	Super::BeginPlay();
	BuildStandIns();
}

void ALyudFishingRod::BuildStandIns()
{
	const FLinearColor Cork(0.52f, 0.37f, 0.22f), Metal(0.06f, 0.06f, 0.065f), BlankCol(0.05f, 0.11f, 0.07f), Steel(0.55f, 0.56f, 0.6f);
	// the grips and the seat
	UStaticMesh* Rear = RodLathe(this, TEXT("SM_RodGripRear"), { { -33.f, 0.f }, { -33.f, 1.2f }, { -32.4f, 1.5f }, { -30.f, 1.45f }, { -18.f, 1.3f }, { -6.5f, 1.25f }, { -6.f, 0.f } }, 16);
	UStaticMesh* Seat = RodLathe(this, TEXT("SM_RodSeat"), { { -6.2f, 0.f }, { -6.2f, 1.32f }, { -4.5f, 1.32f }, { -4.2f, 1.08f }, { 4.2f, 1.08f }, { 4.5f, 1.3f }, { 6.2f, 1.3f }, { 6.2f, 0.f } }, 16);
	UStaticMesh* Fore = RodLathe(this, TEXT("SM_RodGripFore"), { { 6.f, 0.f }, { 6.f, 1.2f }, { 9.f, 1.15f }, { 13.5f, 0.85f }, { 14.2f, 0.f } }, 16);
	Butt = Part(TEXT("GripRear"), Rear, Root, Cork);
	Part(TEXT("Seat"), Seat, Root, Metal);
	Part(TEXT("GripFore"), Fore, Root, Cork);
	// the reel under the seat: its foot, body, spool along the rod, the handle on the left turned by the other hand
	UStaticMesh* Foot = RodLathe(this, TEXT("SM_ReelFoot"), { { 0.f, 0.f }, { 0.f, 0.35f }, { 5.5f, 0.35f }, { 5.5f, 0.f } }, 8);
	UStaticMeshComponent* FootC = Part(TEXT("ReelFoot"), Foot, Root, Metal);
	FootC->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -1.f), FRotator(-90.f, 0.f, 0.f));
	UStaticMesh* Body = RodLathe(this, TEXT("SM_ReelBody"), { { -3.5f, 0.f }, { -3.4f, 1.6f }, { -1.5f, 2.3f }, { 1.f, 2.2f }, { 1.2f, 1.f }, { 2.f, 0.9f }, { 2.f, 0.f } }, 16);
	Reel = Part(TEXT("Reel"), Body, Root, Metal);
	Reel->SetRelativeLocation(FVector(0.f, 0.f, -7.f));
	UStaticMesh* Spool = RodLathe(this, TEXT("SM_ReelSpool"), { { 2.f, 0.f }, { 2.f, 2.4f }, { 2.4f, 2.5f }, { 2.6f, 1.9f }, { 5.f, 1.85f }, { 5.2f, 2.3f }, { 5.6f, 2.2f }, { 5.8f, 0.6f }, { 6.6f, 0.f } }, 18);
	Part(TEXT("Spool"), Spool, Reel, Steel);
	ReelHandle = NewObject<UStaticMeshComponent>(this, TEXT("ReelHandleAxle"), RF_Transient);
	ReelHandle->SetupAttachment(Reel);
	NoLumen(ReelHandle);
	ReelHandle->SetRelativeLocation(FVector(-0.8f, -2.4f, 0.f));
	ReelHandle->RegisterComponent();
	UStaticMesh* Arm = RodLathe(this, TEXT("SM_ReelArm"), { { 0.f, 0.f }, { 0.f, 0.3f }, { 5.f, 0.25f }, { 5.f, 0.f } }, 8);
	UStaticMeshComponent* ArmC = Part(TEXT("ReelArm"), Arm, ReelHandle, Metal);
	ArmC->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	UStaticMesh* Knob = RodLathe(this, TEXT("SM_ReelKnob"), { { 0.f, 0.f }, { 0.f, 0.55f }, { 1.6f, 0.65f }, { 2.2f, 0.f } }, 12);
	UStaticMeshComponent* KnobC = Part(TEXT("ReelKnob"), Knob, ReelHandle, FLinearColor(0.12f, 0.08f, 0.05f));
	KnobC->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -5.f), FRotator(0.f, -90.f, 0.f));
	// the blank: a chain of segments, each turned, with its ring under it
	UStaticMesh* Seg = RodLathe(this, TEXT("SM_RodBlank"), { { 0.f, 0.f }, { 0.f, 1.f }, { 100.f, BlankR(ButtFront + SegLen()) / BlankBase }, { 100.f, 0.f } }, 10);
	UStaticMesh* Guide = RodLathe(this, TEXT("SM_RodGuide"), RodRing(1.f, 0.12f), 12);
	USceneComponent* Parent = Root;
	for (int32 I = 0; I < Segments; I++)
	{
		USceneComponent* J = NewObject<USceneComponent>(this, *FString::Printf(TEXT("Joint%d"), I), RF_Transient);
		J->SetupAttachment(Parent);
		J->SetMobility(EComponentMobility::Movable);
		J->SetRelativeLocation(FVector(I == 0 ? ButtFront : SegLen(), 0.f, 0.f));
		J->RegisterComponent();
		Joints.Add(J);
		const float R0 = BlankR(ButtFront + I * SegLen());
		UStaticMeshComponent* S = Part(*FString::Printf(TEXT("Blank%d"), I), Seg, J, BlankCol);
		S->SetRelativeScale3D(FVector(SegLen() / 100.f, R0, R0));
		Blank.Add(S);
		// a ring at the end of each segment, under the blank (the reel's side), smaller toward the tip; the last is the tip-top
		const float R1 = BlankR(ButtFront + (I + 1) * SegLen());
		const float RingR = FMath::Lerp(1.4f, 0.35f, float(I) / (Segments - 1));
		UStaticMeshComponent* G = Part(*FString::Printf(TEXT("Guide%d"), I), Guide, J, Steel);
		G->SetRelativeLocation(FVector(SegLen() - 0.6f, 0.f, I == Segments - 1 ? 0.f : -(R1 + RingR + 0.3f)));
		G->SetRelativeScale3D(FVector(RingR, RingR, RingR));
		Parent = J;
	}
	// the line: thin pieces, moved in place every frame
	Line = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Line"), RF_Transient);
	Line->SetupAttachment(Root);
	NoLumen(Line);
	Line->SetUsingAbsoluteLocation(true);
	Line->SetUsingAbsoluteRotation(true);
	Line->SetUsingAbsoluteScale(true);
	Line->SetCastShadow(false);
	UStaticMesh* Piece = RodLathe(this, TEXT("SM_LinePiece"), { { 0.f, 0.f }, { 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f } }, 4);
	Line->SetStaticMesh(Piece);
	if (UMaterialInstanceDynamic* M = Paint(FLinearColor(0.7f, 0.78f, 0.74f)))
	{
		Line->SetMaterial(0, M);
	}
	Line->RegisterComponent();
	LineXf.SetNum(LinePieces);
	for (FTransform& X : LineXf) X = FTransform(FQuat::Identity, GetActorLocation(), FVector(0.01f));
	Line->AddInstances(LineXf, false, true, false);
	Line->SetVisibility(false);
	// the float: a white body, a red top and its antenna; it stands up along +X turned to +Z
	Float = NewObject<UStaticMeshComponent>(this, TEXT("FloatRoot"), RF_Transient);
	Float->SetupAttachment(Root);
	NoLumen(Float);
	Float->SetUsingAbsoluteLocation(true);
	Float->SetUsingAbsoluteRotation(true);
	Float->RegisterComponent();
	UStaticMesh* FloatBody = RodLathe(this, TEXT("SM_FloatBody"), { { -5.f, 0.f }, { -4.6f, 0.25f }, { -3.f, 0.75f }, { -1.f, 1.1f }, { 0.5f, 1.05f }, { 0.5f, 0.f } }, 14);
	UStaticMesh* FloatTop = RodLathe(this, TEXT("SM_FloatTop"), { { 0.5f, 0.f }, { 0.5f, 1.05f }, { 1.6f, 0.75f }, { 2.4f, 0.3f }, { 7.5f, 0.22f }, { 7.8f, 0.f } }, 14);
	UStaticMeshComponent* FB = Part(TEXT("FloatBody"), FloatBody, Float, FLinearColor(0.85f, 0.85f, 0.8f));
	UStaticMeshComponent* FT = Part(TEXT("FloatTop"), FloatTop, Float, FLinearColor(0.85f, 0.08f, 0.03f));
	FB->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	FT->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Float->SetVisibility(false, true);
	// the spoon: a curved silver leaf, its hook behind
	Spoon = NewObject<UStaticMeshComponent>(this, TEXT("SpoonRoot"), RF_Transient);
	Spoon->SetupAttachment(Root);
	NoLumen(Spoon);
	Spoon->SetUsingAbsoluteLocation(true);
	Spoon->SetUsingAbsoluteRotation(true);
	Spoon->RegisterComponent();
	UStaticMesh* Leaf = RodLathe(this, TEXT("SM_Spoon"), { { -3.5f, 0.f }, { -3.f, 0.7f }, { -1.f, 1.3f }, { 1.5f, 1.25f }, { 3.2f, 0.6f }, { 3.6f, 0.f } }, 14);
	UStaticMeshComponent* L = Part(TEXT("SpoonLeaf"), Leaf, Spoon, FLinearColor(0.85f, 0.86f, 0.88f));
	L->SetRelativeScale3D(FVector(1.f, 1.f, 0.22f));
	UStaticMeshComponent* Hook = Part(TEXT("SpoonHook"), RodLathe(this, TEXT("SM_Hook"), RodRing(0.6f, 0.07f), 10), Spoon, FLinearColor(0.3f, 0.3f, 0.32f));
	Hook->SetRelativeLocationAndRotation(FVector(-4.2f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f));
	Spoon->SetVisibility(false, true);
	// the sea rod's bottom rig: a pear-shaped lead sinker (no float), its swivel on top
	Sinker = Part(TEXT("Sinker"), RodLathe(this, TEXT("SM_Sinker"), { { -2.6f, 0.f }, { -2.3f, 0.6f }, { -1.f, 1.15f }, { 0.6f, 1.05f }, { 2.f, 0.5f }, { 2.6f, 0.f } }, 12),
		Root, FLinearColor(0.32f, 0.33f, 0.35f));
	Sinker->SetUsingAbsoluteLocation(true);
	Sinker->SetUsingAbsoluteRotation(true);
	Sinker->SetVisibility(false);
	// the fish on the hook: one instance of its species' swimming mesh
	Fish = NewObject<UInstancedStaticMeshComponent>(this, TEXT("HookedFish"), RF_Transient);
	Fish->SetupAttachment(Root);
	NoLumen(Fish);
	Fish->SetUsingAbsoluteLocation(true);
	Fish->SetUsingAbsoluteRotation(true);
	Fish->SetUsingAbsoluteScale(true);
	Fish->SetNumCustomDataFloats(3);
	Fish->RegisterComponent();
	ULyudPrefetch::Ready(this, RodStandInPath);
	Fish->SetVisibility(false);
	SetBend(0.f, FVector::DownVector);
}

void ALyudFishingRod::SetBend(float Amount, const FVector& PullDir)
{
	if (Joints.Num() != Segments) return;
	// the pull (and the blank's own weight) in the rod's frame; the blank bends in the plane of the rod and the pull
	const FTransform Xf = GetActorTransform();
	const FVector P = Xf.InverseTransformVectorNoScale(PullDir.GetSafeNormal());
	const FVector G = Xf.InverseTransformVectorNoScale(FVector::DownVector);
	FVector B = FVector(0.f, P.Y, P.Z).GetSafeNormal() * Amount + FVector(0.f, G.Y, G.Z) * 0.07f;
	const float Theta = FMath::Min(B.Size(), 1.6f);
	const FVector Axis = Theta > 1e-4f ? FVector::CrossProduct(FVector::ForwardVector, B.GetSafeNormal()).GetSafeNormal() : FVector::RightVector;
	// more of the bend toward the tip (a progressive action)
	static float W[Segments];
	static bool bW = false;
	if (!bW)
	{
		float Sum = 0.f;
		for (int32 I = 0; I < Segments; I++) { W[I] = FMath::Pow(float(I + 1), 1.4f); Sum += W[I]; }
		for (int32 I = 0; I < Segments; I++) W[I] /= Sum;
		bW = true;
	}
	for (int32 I = 0; I < Segments; I++)
	{
		Joints[I]->SetRelativeRotation(FQuat(Axis, Theta * W[I]));
	}
}

FVector ALyudFishingRod::Tip() const
{
	if (Joints.Num() != Segments) return GetActorTransform().TransformPosition(FVector(Length, 0.f, 0.f));
	return Joints.Last()->GetComponentTransform().TransformPosition(FVector(SegLen(), 0.f, 0.f));
}

void ALyudFishingRod::SetLine(const FVector& End, float Slack, float WaterZ)
{
	if (!Line) return;
	const FVector A = Tip();
	const FVector D = End - A;
	const float Len = D.Size();
	// a hanging line sags in the middle (a parabola standing in for the catenary); under water it runs straight
	const float Sag = Len * 0.12f * FMath::Clamp(Slack, 0.f, 1.f);
	FVector Prev = A;
	for (int32 I = 1; I <= LinePieces; I++)
	{
		const float T = float(I) / LinePieces;
		FVector P = A + D * T - FVector(0.f, 0.f, Sag * 4.f * T * (1.f - T));
		if (P.Z < WaterZ && End.Z < WaterZ + 5.f && A.Z > WaterZ)
		{
			// below the surface it goes straight to the end
			P.Z = FMath::Max(P.Z, FMath::Lerp(WaterZ, End.Z, FMath::Clamp((P - A).Size2D() / FMath::Max(D.Size2D(), 1.f), 0.f, 1.f)));
		}
		const FVector Seg = P - Prev;
		const float L = Seg.Size();
		const FQuat Q = L > 1e-3f ? FRotationMatrix::MakeFromX(Seg / L).ToQuat() : FQuat::Identity;
		// a piece is 1 cm long along X with radius 1: the line is ~0.25 mm, drawn 0.8 mm so it reads at a distance
		LineXf[I - 1] = FTransform(Q, Prev, FVector(FMath::Max(L, 0.01f), 0.04f, 0.04f));
		Prev = P;
	}
	Line->BatchUpdateInstancesTransforms(0, LineXf, true, false, true);
	if (!bLineShown)
	{
		Line->SetVisibility(true);
		bLineShown = true;
	}
}

void ALyudFishingRod::HideLine()
{
	if (Line && bLineShown)
	{
		Line->SetVisibility(false);
		bLineShown = false;
	}
}

void ALyudFishingRod::SetFloat(bool bShow, const FVector& Where, float Lean)
{
	if (!Float) return;
	if (Float->IsVisible() != bShow) Float->SetVisibility(bShow, true);
	if (bShow)
	{
		// lying on its side when the line pulls it flat (or before the cast, hanging), upright riding the water
		Float->SetWorldLocationAndRotation(Where, FRotator(-90.f * FMath::Clamp(Lean, 0.f, 1.f), GetActorRotation().Yaw, 0.f));
	}
}

void ALyudFishingRod::SetSinker(bool bShow, const FVector& Where)
{
	if (!Sinker) return;
	if (Sinker->IsVisible() != bShow) Sinker->SetVisibility(bShow);
	if (bShow)
	{
		Sinker->SetWorldLocationAndRotation(Where, FRotator(90.f, 0.f, 0.f));
	}
}

void ALyudFishingRod::SetTier(int32 Tackle)
{
	if (Tackle == Tier || Blank.Num() != Segments)
	{
		return;
	}
	Tier = Tackle;
	// the basic rod: dark green glass; the spinning rod: a slim grey-blue carbon blank; the sea rod: a thick yellowish glass one
	static const FLinearColor Cols[] = { FLinearColor(0.05f, 0.11f, 0.07f), FLinearColor(0.07f, 0.09f, 0.13f), FLinearColor(0.62f, 0.55f, 0.32f) };
	static const float Thick[] = { 1.f, 0.85f, 1.6f };
	const int32 T = FMath::Clamp(Tackle, 0, 2);
	UMaterialInstanceDynamic* M = Paint(Cols[T]);
	for (int32 I = 0; I < Blank.Num(); I++)
	{
		if (!Blank[I]) continue;
		const float R0 = BlankR(ButtFront + I * SegLen()) * Thick[T];
		Blank[I]->SetRelativeScale3D(FVector(SegLen() / 100.f, R0, R0));
		if (M) Blank[I]->SetMaterial(0, M);
	}
}

void ALyudFishingRod::SetSpoon(bool bShow, const FVector& Where, const FVector& Along, float Spin)
{
	if (!Spoon) return;
	if (Spoon->IsVisible() != bShow) Spoon->SetVisibility(bShow, true);
	if (bShow)
	{
		const FQuat Q = FRotationMatrix::MakeFromX(Along.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector)).ToQuat() * FQuat(FVector::ForwardVector, Spin);
		Spoon->SetWorldLocationAndRotation(Where, Q);
	}
}

bool ALyudFishingRod::PrefetchFish(FName Species)
{
	const FString Id = Species.ToString();
	// all four asked every time, so each starts loading at once
	const bool bMesh = ULyudPrefetch::Ready(this, RodFishMeshPath(Id));
	const bool bSkin = ULyudPrefetch::Ready(this, RodFishSkinPath(Id));
	const bool bBody = ULyudPrefetch::Ready(this, RodFishBodyPath);
	const bool bFin = ULyudPrefetch::Ready(this, RodFishFinPath);
	return bMesh && bSkin && bBody && bFin;
}

bool ALyudFishingRod::ApplyFishLook(FName Species)
{
	const FString Id = Species.ToString();
	UStaticMesh* Mesh = RodFind<UStaticMesh>(RodFishMeshPath(Id));
	UMaterialInterface* Body = RodFind<UMaterialInterface>(RodFishBodyPath);
	if (!Mesh || !Body)
	{
		return false;
	}
	Fish->SetStaticMesh(Mesh);
	TArray<UMaterialInstanceDynamic*>* Made = FishMids.Find(Species);
	if (!Made)
	{
		// the species' materials, made the first time it is hooked and kept
		Made = &FishMids.Add(Species);
		UTexture2D* Skin = RodFind<UTexture2D>(RodFishSkinPath(Id));
		UMaterialInterface* Fin = RodFind<UMaterialInterface>(RodFishFinPath);
		const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*Id));
		for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); Slot++)
		{
			const bool bFin = Mesh->GetStaticMaterials()[Slot].MaterialSlotName.ToString().Contains(TEXT("Fin"));
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(bFin && Fin ? Fin : Body, this);
			if (Skin) M->SetTextureParameterValue(TEXT("Skin"), Skin);
			if (S >= 0)
			{
				const LyudFish::FSpecies& Sp = LyudFish::Species(S);
				M->SetScalarParameterValue(TEXT("BodyLength"), Sp.Length * 100.f);
				M->SetScalarParameterValue(TEXT("Waves"), Sp.Waves);
				M->SetScalarParameterValue(TEXT("Eel"), Sp.Bend > 0.8f ? 1.f : 0.f);
				M->SetScalarParameterValue(TEXT("Amp"), Sp.Bend > 0.8f ? 0.07f : 0.11f);
				M->SetScalarParameterValue(TEXT("Flat"), Sp.Way == LyudFish::EWay::Flat ? 1.f : 0.f);
			}
			Made->Add(M);
			Mids.Add(M);
		}
	}
	for (int32 Slot = 0; Slot < Made->Num(); Slot++)
	{
		Fish->SetMaterial(Slot, (*Made)[Slot]);
	}
	return true;
}

void ALyudFishingRod::SetFish(FName Species, float LengthM)
{
	if (!Fish) return;
	// a stand-in left from before (its look was still loading when the rod was put away) asks for the look again
	const bool bNewLook = Species != FishSpecies || Fish->GetInstanceCount() != 1 || bStandIn;
	FishSpecies = Species;
	FishLength = LengthM;
	if (bNewLook)
	{
		const bool bReady = PrefetchFish(Species);
		bStandIn = !(bReady && ApplyFishLook(Species));
		// still loading: swapped in when it arrives; ready but not there: never built, the stand-in stays
		PendingLook = bReady ? NAME_None : Species;
		if (bStandIn)
		{
			// not in memory yet (or never built): the code-built stand-in (a 10 cm sprat) until it arrives
			Fish->SetStaticMesh(LyudSeaLife::Fish());
			if (!StandInMid)
			{
				if (UMaterialInterface* Old = RodFind<UMaterialInterface>(RodStandInPath))
				{
					StandInMid = UMaterialInstanceDynamic::Create(Old, this);
					StandInMid->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.5f, 0.52f, 0.5f));
					StandInMid->SetScalarParameterValue(TEXT("Wiggle"), 1.f);
				}
			}
			Fish->SetMaterial(0, StandInMid);
		}
		if (Fish->GetInstanceCount() != 1)
		{
			Fish->ClearInstances();
			Fish->AddInstance(FTransform(FQuat::Identity, GetActorLocation(), FVector::OneVector), true);
			const float Data[3] = { 0.f, 0.5f, 0.f };
			Fish->SetCustomData(0, MakeArrayView(Data, 3), false);
		}
	}
	// every fish its own size, the same species or not: the mesh is built at its species' length (the stand-in at LyudSeaLife::FishLength)
	const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*Species.ToString()));
	const float Built = bStandIn ? LyudSeaLife::FishLength : (S >= 0 ? LyudFish::Species(S).Length : 0.2f);
	FTransform Was;
	Fish->GetInstanceTransform(0, Was, true);
	Was.SetScale3D(FVector(LengthM / FMath::Max(Built, 0.01f)));
	Fish->UpdateInstanceTransform(0, Was, true, false, true);
}

void ALyudFishingRod::PlaceFish(bool bShow, const FVector& Where, const FRotator& Rot, float Wriggle, float DeltaTime)
{
	if (!Fish) return;
	if (Fish->IsVisible() != bShow) Fish->SetVisibility(bShow);
	if (!bShow || Fish->GetInstanceCount() < 1) return;
	// asked four times a second, not every frame (the paths are strings)
	PendingCheck -= DeltaTime;
	bool bArrived = false;
	if (!PendingLook.IsNone() && PendingCheck <= 0.f)
	{
		PendingCheck = 0.25f;
		bArrived = PrefetchFish(PendingLook);
	}
	if (bArrived)
	{
		// its look has loaded: put it on, at this fish's size
		const FName Species = PendingLook;
		PendingLook = NAME_None;
		if (ApplyFishLook(Species))
		{
			bStandIn = false;
			FishSpecies = Species;
			SetFish(Species, FishLength);   // the same look now: only its size is set
		}
	}
	FTransform Was;
	Fish->GetInstanceTransform(0, Was, true);
	Fish->UpdateInstanceTransform(0, FTransform(Rot, Where, Was.GetScale3D()), true, false, true);
	// a fish on the hook beats hard and bends side to side (the swimming material's phase, beat, bend)
	FishPhase = FMath::Fmod(FishPhase + DeltaTime * (6.f + 14.f * Wriggle), 2.f * PI * 16.f);
	const float Data[3] = { FishPhase, 0.25f + 0.75f * Wriggle, 0.6f * Wriggle * FMath::Sin(FishPhase * 0.37f) };
	Fish->SetCustomData(0, MakeArrayView(Data, 3), false);
}

void ALyudFishingRod::TurnReel(float Revs)
{
	if (!ReelHandle || FMath::IsNearlyZero(Revs)) return;
	ReelAngle = FMath::Fmod(ReelAngle + Revs * 360.f, 360.f);
	ReelHandle->SetRelativeRotation(FRotator(ReelAngle, 0.f, 0.f));
}

void ALyudFishingRod::Park()
{
	HideLine();
	SetFloat(false);
	SetSpoon(false);
	SetSinker(false);
	if (Fish && Fish->IsVisible()) Fish->SetVisibility(false);
	PendingLook = NAME_None;
	SetBend(0.f, FVector::DownVector);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorHiddenInGame(true);
}
