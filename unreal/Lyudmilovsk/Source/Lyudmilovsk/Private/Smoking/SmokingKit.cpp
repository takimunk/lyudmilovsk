#include "Smoking/SmokingKit.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace
{
	UStaticMesh* KitMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Smoking/%s.%s"), Name, Name));
	}

	/** A small held or lying part: no collision, no shadow-casting worth paying for, out of Lumen (it moves with the hand). */
	UStaticMeshComponent* Part(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const TCHAR* Mesh, bool bShadow = true)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(bShadow);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetGenerateOverlapEvents(false);
		// the mesh is set at BeginPlay (SetKitMeshes): no asset loads while the class default is built
		C->ComponentTags.Add(FName(Mesh));
		return C;
	}

	/** Gives every part of Owner the kit mesh its tag names. */
	void SetKitMeshes(AActor* Owner)
	{
		TArray<UStaticMeshComponent*> Comps;
		Owner->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* C : Comps)
		{
			if (C->ComponentTags.Num() > 0 && !C->GetStaticMesh())
			{
				C->SetStaticMesh(KitMesh(*C->ComponentTags[0].ToString()));
			}
		}
	}

	/** The order the ten are taken in: the front row from its middle out, then the back row. */
	const int32 Order[SmokingKit::PerPack][2] = { { 0, 2 }, { 0, 1 }, { 0, 3 }, { 0, 0 }, { 0, 4 }, { 1, 2 }, { 1, 1 }, { 1, 3 }, { 1, 0 }, { 1, 4 } };
}

FVector SmokingKit::Slot(int32 Index)
{
	// as build_smoking.py lays them (Blender y is UE -y): rows from the front, columns across
	constexpr float Pitch = 0.8f, Wall = 0.05f, Gap = 0.025f, Collar = 0.04f;
	const int32 Row = Order[FMath::Clamp(Index, 0, PerPack - 1)][0];
	const int32 Col = Order[FMath::Clamp(Index, 0, PerPack - 1)][1];
	const float X = PackD / 2.f - Wall - Collar - Gap - Pitch * (Row + 0.5f);
	const float Y = -(-PackW / 2.f + Wall + Collar + Gap + Pitch * (Col + 0.5f));
	return FVector(X, Y, Wall + 0.005f);
}

// ------------------------------------------------------------------ the pack

ACigarettePack::ACigarettePack()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Body = Part(this, Root, TEXT("Body"), TEXT("SM_CigPackBody"));
	Lid = Part(this, Root, TEXT("Lid"), TEXT("SM_CigPackLid"));
	Lid->SetRelativeLocation(SmokingKit::Hinge);
	Collar = Part(this, Root, TEXT("Collar"), TEXT("SM_CigPackCollar"));
	for (int32 I = 0; I < SmokingKit::PerPack; ++I)
	{
		UStaticMeshComponent* C = Part(this, Root, *FString::Printf(TEXT("Cig%d"), I), TEXT("SM_Cigarette"), false);
		C->SetRelativeLocation(SmokingKit::Slot(I));
		Cigs.Add(C);
	}
}

void ACigarettePack::BeginPlay()
{
	Super::BeginPlay();
	SetKitMeshes(this);
	// the printed card in the game's language (the words are part of the picture: textures/localized/<lang>/cig_pack.png)
	if (UTexture2D* Atlas = LyudText::LoadTexture(TEXT("Smoking"), TEXT("T_CigPack")))
	{
		if (UMaterialInterface* Base = Body->GetMaterial(0))
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
			Mid->SetTextureParameterValue(TEXT("Atlas"), Atlas);
			Body->SetMaterial(0, Mid);
			Lid->SetMaterial(0, Mid);
			Collar->SetMaterial(0, Mid);
		}
	}
}

void ACigarettePack::SetLeft(int32 InLeft)
{
	Left = FMath::Clamp(InLeft, 0, SmokingKit::PerPack);
	for (int32 I = 0; I < SmokingKit::PerPack; ++I)
	{
		Cigs[I]->SetVisibility(I >= SmokingKit::PerPack - Left);
	}
	SetRaised(Raised);
}

void ACigarettePack::SetLid(float Open)
{
	// the lid turns back about its hinge: its front comes up and over
	Lid->SetRelativeRotation(FRotator(115.f * FMath::Clamp(Open, 0.f, 1.f), 0.f, 0.f));
}

void ACigarettePack::SetRaised(float Cm)
{
	Raised = Cm;
	for (int32 I = 0; I < SmokingKit::PerPack; ++I)
	{
		const bool bNext = I == SmokingKit::PerPack - Left;
		Cigs[I]->SetRelativeLocation(SmokingKit::Slot(I) + FVector(0.f, 0.f, bNext ? Cm : 0.f));
	}
}

FVector ACigarettePack::NextMouthEnd(int32 InLeft, float Cm)
{
	return SmokingKit::Slot(SmokingKit::PerPack - FMath::Clamp(InLeft, 1, SmokingKit::PerPack)) + FVector(0.f, 0.f, SmokingKit::CigL + Cm);
}

FTransform ACigarettePack::NextCigarette() const
{
	// standing in the pack the filter is up: the cigarette's +X (mouth end to tip) is the pack's -Z
	const FVector Top = NextMouthEnd(Left, Raised);
	const FQuat Q = FRotationMatrix::MakeFromXZ(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f)).ToQuat();
	return FTransform(Q, Top) * GetActorTransform();
}

// ------------------------------------------------------------------ the cigarette

ACigaretteItem::ACigaretteItem()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	FilterMesh = Part(this, Root, TEXT("Filter"), TEXT("SM_CigFilter"));
	PaperMesh = Part(this, Root, TEXT("Paper"), TEXT("SM_CigPaper"));
	TipMesh = Part(this, Root, TEXT("Tip"), TEXT("SM_CigTip"));
	Ember = CreateDefaultSubobject<UPointLightComponent>(TEXT("Ember"));
	Ember->SetupAttachment(TipMesh);
	Ember->SetRelativeLocation(FVector(SmokingKit::Tip + 0.3f, 0.f, 0.f));
	Ember->SetMobility(EComponentMobility::Movable);
	Ember->SetCastShadows(false);
	Ember->SetAttenuationRadius(35.f);
	Ember->SetSourceRadius(0.3f);
	Ember->SetLightColor(FLinearColor(1.f, 0.32f, 0.08f));
	Ember->SetIntensityUnits(ELightUnits::Candelas);
	Ember->SetIntensity(0.f);
	Ember->SetVisibility(false);
	Ember->bAffectTranslucentLighting = false;
	SetLeft(1.f);
}

void ACigaretteItem::BeginPlay()
{
	Super::BeginPlay();
	SetKitMeshes(this);
	if (UMaterialInterface* Base = TipMesh->GetMaterial(0))
	{
		TipMat = UMaterialInstanceDynamic::Create(Base, this);
		TipMesh->SetMaterial(0, TipMat);
	}
	const float G = GlowNow;
	GlowNow = -1.f;
	SetGlow(FMath::Max(G, 0.f));
}

void ACigaretteItem::SetLeft(float InLeft)
{
	Left = FMath::Clamp(InLeft, 0.f, 1.f);
	const float Len = SmokingKit::Stub + (SmokingKit::Paper - SmokingKit::Stub) * Left;
	PaperMesh->SetRelativeLocation(FVector(SmokingKit::Filter, 0.f, 0.f));
	PaperMesh->SetRelativeScale3D(FVector(Len, 1.f, 1.f));     // the paper mesh is 1 cm long
	TipMesh->SetRelativeLocation(FVector(SmokingKit::Filter + Len, 0.f, 0.f));
}

void ACigaretteItem::SetGlow(float Glow)
{
	Glow = FMath::Clamp(Glow, 0.f, 1.f);
	if (FMath::Abs(Glow - GlowNow) < 0.004f)
	{
		return;
	}
	GlowNow = Glow;
	if (TipMat)
	{
		TipMat->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
	Ember->SetVisibility(Glow > 0.02f);
	Ember->SetIntensity(0.06f * Glow * Glow);
}

FVector ACigaretteItem::TipLocation() const
{
	return TipMesh->GetComponentTransform().TransformPosition(FVector(SmokingKit::Tip, 0.f, 0.f));
}

// ------------------------------------------------------------------ the lighter

ALighterItem::ALighterItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	BodyMesh = Part(this, Root, TEXT("Body"), TEXT("SM_Lighter"));
	FlameMesh = Part(this, Root, TEXT("Flame"), TEXT("SM_LighterFlame"), false);
	FlameMesh->SetRelativeLocation(FVector(0.f, 0.f, SmokingKit::LighterTop + 0.05f));
	FlameMesh->SetVisibility(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, SmokingKit::LighterTop + 1.f));
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(140.f);
	Light->SetSourceRadius(0.5f);
	Light->SetLightColor(FLinearColor(1.f, 0.62f, 0.25f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetVisibility(false);
}

void ALighterItem::BeginPlay()
{
	Super::BeginPlay();
	SetKitMeshes(this);
}

void ALighterItem::SetFlame(bool bOn)
{
	bLit = bOn;
	FlameMesh->SetVisibility(bOn);
	Light->SetVisibility(bOn);
	SetActorTickEnabled(bOn);
	if (!bOn)
	{
		Light->SetIntensity(0.f);
	}
}

void ALighterItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// only while lit: the flame trembles and the light with it
	Time += DeltaTime;
	const float F = 0.85f + 0.1f * FMath::Sin(Time * 31.f) + 0.06f * FMath::Sin(Time * 57.f + 1.3f);
	FlameMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 2.2f * F));
	Light->SetIntensity(0.35f * F);
}

// ------------------------------------------------------------------ the butt

ACigaretteButt::ACigaretteButt()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDynamicIndirectLighting = false;
	Mesh->bAffectDistanceFieldLighting = false;
}

void ACigaretteButt::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetStaticMesh(KitMesh(TEXT("SM_CigButt")));
}

// ------------------------------------------------------------------ smoke

namespace
{
	/** A 1 cm square card facing +Z, centred; UV 0..1. */
	UStaticMesh* PuffMesh(UObject* Outer, UMaterialInterface* Material)
	{
		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Smoke");
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
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, TEXT("SM_SmokePuff"), RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Smoke")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		return Mesh;
	}
}

ALyudSmokeFx::ALyudSmokeFx()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Puffs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Puffs"));
	Puffs->SetupAttachment(Root);
	Puffs->SetUsingAbsoluteLocation(true);
	Puffs->SetUsingAbsoluteRotation(true);
	Puffs->SetUsingAbsoluteScale(true);
	Puffs->SetMobility(EComponentMobility::Movable);
	Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puffs->SetCastShadow(false);
	Puffs->bAffectDynamicIndirectLighting = false;
	Puffs->bAffectDistanceFieldLighting = false;
	Puffs->NumCustomDataFloats = 2;
	Puffs->SetVisibility(false);
	All.SetNum(Pool);
	Xf.SetNum(Pool);
}

void ALyudSmokeFx::Emit(const FVector& Where, const FVector& Velocity, float Size0, float Size1, float Alpha, float Life, float Rise)
{
	if (Puffs->GetInstanceCount() == 0)
	{
		// built once, at the first puff: the card and its pool of instances
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Smoking/M_Smoke.M_Smoke"));
		Puffs->SetStaticMesh(PuffMesh(this, M));
		TArray<FTransform> Zero;
		Zero.Init(FTransform(FQuat::Identity, Where, FVector(0.001f)), Pool);
		Puffs->AddInstances(Zero, false, true);
	}
	FPuff& P = All[Next];
	Next = (Next + 1) % Pool;
	P.Pos = Where;
	P.Vel = Velocity;
	P.Age = 0.f;
	P.Life = FMath::Max(Life, 0.2f);
	P.Size0 = Size0;
	P.Size1 = Size1;
	P.Alpha = Alpha;
	P.Rise = Rise;
	P.Seed = FMath::FRand();
	P.bHiddenWritten = false;
	if (!bShown)
	{
		bShown = true;
		Puffs->SetVisibility(true);
		SetActorTickEnabled(true);
	}
}

void ALyudSmokeFx::Breath(const FVector& Where, const FVector& Dir, float Amount)
{
	const int32 N = FMath::RoundToInt(FMath::Lerp(5.f, 18.f, FMath::Clamp(Amount, 0.f, 1.f)));
	const FVector D = Dir.GetSafeNormal();
	for (int32 I = 0; I < N; ++I)
	{
		const float K = (float)I / FMath::Max(N - 1, 1);
		// the breath leaves over ~0.6 s: the first puffs fast, the tail slow and wide
		const FVector V = (D * FMath::Lerp(90.f, 25.f, K) + FMath::VRand() * 14.f) * FMath::Lerp(0.7f, 1.2f, Amount);
		Emit(Where + D * (0.5f + 2.f * K), V, FMath::Lerp(2.5f, 5.f, K), FMath::Lerp(24.f, 55.f, Amount) * FMath::Lerp(0.8f, 1.2f, K),
			FMath::Lerp(0.20f, 0.42f, Amount) * FMath::Lerp(1.f, 0.6f, K), FMath::Lerp(2.f, 3.4f, Amount), 6.f);
	}
}

void ALyudSmokeFx::Tick(float DeltaTime)
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
			// the push it left with dies away; warm smoke drifts up and wobbles
			P.Vel *= FMath::Exp(-2.2f * Dt);
			P.Vel.Z += P.Rise * Dt;
			const float W = P.Seed * 6.28f + P.Age * 2.1f;
			P.Pos += (P.Vel + FVector(FMath::Sin(W) * 3.f, FMath::Cos(W * 1.3f) * 3.f, 0.f)) * Dt;
			const float T = FMath::Clamp(P.Age / P.Life, 0.f, 1.f);
			Show = P.Alpha * FMath::SmoothStep(0.f, 0.12f, T) * (1.f - FMath::SmoothStep(0.45f, 1.f, T));
			++Live;
		}
		const float Size = Show > 0.f ? FMath::Lerp(P.Size0, P.Size1, FMath::Sqrt(FMath::Clamp(P.Age / P.Life, 0.f, 1.f))) : 0.001f;
		const FVector ToCam = (Cam - P.Pos).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Xf[I] = FTransform(FRotationMatrix::MakeFromZ(ToCam).ToQuat() * FQuat(FVector::UpVector, P.Seed * 6.28f + P.Age * 0.4f), P.Pos, FVector(Size));
		Puffs->SetCustomDataValue(I, 0, Show, false);
		Puffs->SetCustomDataValue(I, 1, P.Seed, false);
		P.bHiddenWritten = P.Age >= P.Life;
	}
	// incremental update (no proxy rebuild, perf checklist): the transforms and the custom data upload with it; the
	// untouched puffs inside the range get their own last transforms again
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
