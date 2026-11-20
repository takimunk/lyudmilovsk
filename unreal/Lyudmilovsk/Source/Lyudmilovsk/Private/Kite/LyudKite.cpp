// Kites: the hero's on its reel, the town's children's, the bench's test kites (see Public/Kite/LyudKite.h, docs/kites.md).
#include "Kite/LyudKite.h"

#include "Audio/LyudSoundscape.h"
#include "../Birds/BirdPerches.h"
#include "City/LyudCityThings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "KiteModel.h"
#include "Landscape.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudKiteHumSynth.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "People/PeopleLife.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "../Transport/LyudMeshKit.h"
#include "UI/LyudGameScreen.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarKites(TEXT("lyud.Kite"), 1, TEXT("lyud.Kite 0|1: the kites (the hero's and the town's)"));
	TAutoConsoleVariable<int32> CVarTownKites(TEXT("lyud.Kite.Town"), 12, TEXT("lyud.Kite.Town N: at most N townsfolk's kites drawn (0 none)"));
	TAutoConsoleVariable<int32> CVarKiteDebug(TEXT("lyud.Kite.Debug"), 0, TEXT("lyud.Kite.Debug 1: the hero's kite's state on screen each second"));

	const FName KiteDiamondId(TEXT("kite_diamond"));
	const FName KiteBoxId(TEXT("kite_box"));
	constexpr float TownDrawCm = 30000.f;     // children's kites are drawn within 300 m of the camera

	LyudKite::FVec ToM(const FVector& Cm) { return LyudKite::FVec(Cm.X * 0.01, Cm.Y * 0.01, Cm.Z * 0.01); }
	FVector ToCm(const LyudKite::FVec& M) { return FVector(M.X * 100.0, M.Y * 100.0, M.Z * 100.0); }
	FVector ToDir(const LyudKite::FVec& V) { return FVector(V.X, V.Y, V.Z); }

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

	/** The colourways: the sail's two halves (the box kite's two cells), the tail's bows. Our own, bright against the sky. */
	struct FColours { FLinearColor A, B, Tail; };
	const FColours Colourways[] = {
		{ FLinearColor(0.80f, 0.10f, 0.07f), FLinearColor(0.95f, 0.78f, 0.12f), FLinearColor(0.10f, 0.25f, 0.70f) },
		{ FLinearColor(0.08f, 0.28f, 0.75f), FLinearColor(0.92f, 0.92f, 0.88f), FLinearColor(0.85f, 0.12f, 0.10f) },
		{ FLinearColor(0.10f, 0.55f, 0.22f), FLinearColor(0.95f, 0.50f, 0.08f), FLinearColor(0.95f, 0.85f, 0.15f) },
		{ FLinearColor(0.45f, 0.12f, 0.55f), FLinearColor(0.95f, 0.80f, 0.20f), FLinearColor(0.15f, 0.65f, 0.70f) },
		{ FLinearColor(0.06f, 0.06f, 0.07f), FLinearColor(0.82f, 0.10f, 0.08f), FLinearColor(0.92f, 0.92f, 0.88f) },
	};
	constexpr int32 NumColourways = UE_ARRAY_COUNT(Colourways);
	const FLinearColor SparTint(0.55f, 0.40f, 0.22f);
	const FLinearColor LineTint(0.86f, 0.86f, 0.80f);
	const FLinearColor ReelTint(0.62f, 0.10f, 0.08f);
	const FLinearColor DrumTint(0.80f, 0.78f, 0.70f);

	/** Shared colours: one rooted material instance per colour for every kite (never freed: a handful). */
	UMaterialInstanceDynamic* Paint(const FLinearColor& Tint)
	{
		static TMap<FLinearColor, UMaterialInstanceDynamic*> Cache;
		if (UMaterialInstanceDynamic** Found = Cache.Find(Tint))
		{
			return *Found;
		}
		static UMaterialInterface* Basic = nullptr;
		if (!Basic)
		{
			Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (!Basic)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basic, GetTransientPackage());
		M->SetVectorParameterValue(TEXT("Color"), Tint);
		M->AddToRoot();
		Cache.Add(Tint, M);
		return M;
	}

	/** Both sides of a thin sheet (a sail, a tail bow). */
	void Sheet(FLyudMeshKit& K, FName Mat, const FVector& A, const FVector& B, const FVector& C)
	{
		K.Tri(Mat, A, B, C);
		K.Tri(Mat, A, C, B);
	}

	struct FBuilt
	{
		UStaticMesh* Mesh = nullptr;
		TArray<FName> Slots;
	};

	FBuilt Keep(const FLyudMeshKit& K, const TCHAR* Name)
	{
		FBuilt B;
		B.Mesh = K.Build(GetTransientPackage(), FName(Name), B.Slots);
		if (B.Mesh)
		{
			B.Mesh->AddToRoot();
		}
		return B;
	}

	// Frames (cm): X the nose (up the sail toward the top), Y across, Z the sail's normal (away from the flyer, downwind);
	// the bridle point (where the line ties on) at the origin.

	/** A diamond 72 cm across and 95 tall, the cross spar a third down from the nose, bowed back 6 cm at the wing tips. */
	const FBuilt& DiamondMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			const FVector Nose(55.f, 0.f, 0.f), Tail(-40.f, 0.f, 0.f), L(22.f, -36.f, 6.f), R(22.f, 36.f, 6.f), Mid(22.f, 0.f, 0.f);
			Sheet(K, TEXT("SailA"), Nose, L, Mid);
			Sheet(K, TEXT("SailA"), Nose, Mid, R);
			Sheet(K, TEXT("SailB"), Tail, Mid, L);
			Sheet(K, TEXT("SailB"), Tail, R, Mid);
			K.Rod(TEXT("Spar"), Tail + FVector(0, 0, -0.6f), Nose + FVector(0, 0, -0.6f), 0.45f, 5, true, false);
			K.Rod(TEXT("Spar"), L + FVector(0, 0, -0.6f), R + FVector(0, 0, -0.6f), 0.4f, 5, true, false);
			B = Keep(K, TEXT("SM_KiteDiamond"));
		}
		return B;
	}

	/** A box kite: four 90 cm longerons on a 32 cm square, two cells (the front and the back 26 cm), cross braces. */
	const FBuilt& BoxMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			const float H = 16.f;
			auto Corner = [H](int32 I, float X)
			{
				const float Y = (I == 0 || I == 3) ? -H : H, Z = (I < 2) ? -H : H;
				return FVector(X, Y, Z);
			};
			for (int32 Cell = 0; Cell < 2; ++Cell)
			{
				const float X0 = Cell == 0 ? 19.f : -45.f, X1 = X0 + 26.f;
				const FName Mat = Cell == 0 ? FName(TEXT("SailA")) : FName(TEXT("SailB"));
				for (int32 I = 0; I < 4; ++I)
				{
					const int32 J = (I + 1) % 4;
					Sheet(K, Mat, Corner(I, X0), Corner(J, X0), Corner(J, X1));
					Sheet(K, Mat, Corner(I, X0), Corner(J, X1), Corner(I, X1));
				}
			}
			for (int32 I = 0; I < 4; ++I)
			{
				K.Rod(TEXT("Spar"), Corner(I, -45.f), Corner(I, 45.f), 0.5f, 5, true, false);
			}
			for (const float X : { 32.f, -32.f })
			{
				K.Rod(TEXT("Spar"), Corner(0, X), Corner(2, X), 0.35f, 4, true, false);
				K.Rod(TEXT("Spar"), Corner(1, X), Corner(3, X), 0.35f, 4, true, false);
			}
			B = Keep(K, TEXT("SM_KiteBox"));
		}
		return B;
	}

	/** The reel: a wooden handle (down its -Z) under a drum wound with line, painted flanges; turns round its Z. */
	const FBuilt& ReelMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Lathe(FTransform::Identity, { { 0.f, 0.f }, { 5.6f, 0.f }, { 5.6f, 0.5f }, { 3.6f, 0.5f }, { 3.6f, 3.3f }, { 5.6f, 3.3f }, { 5.6f, 3.8f }, { 0.f, 3.8f } },
				{ TEXT("Reel"), TEXT("Reel"), TEXT("Reel"), TEXT("Drum"), TEXT("Reel"), TEXT("Reel"), TEXT("Reel") }, 16, false, false);
			K.Rod(TEXT("Spar"), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, -9.f), 1.2f, 8, true, false);
			K.Rod(TEXT("Spar"), FVector(4.6f, 0.f, 3.8f), FVector(4.6f, 0.f, 6.3f), 0.5f, 6, true, false);  // the winding knob
			B = Keep(K, TEXT("SM_KiteReel"));
		}
		return B;
	}

	/** A line piece: 1 cm along X, radius 1 (scaled to the piece's length and the line's width). */
	const FBuilt& PieceMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Rod(TEXT("Line"), FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 1.f, 4, false, false);
			B = Keep(K, TEXT("SM_KiteLinePiece"));
		}
		return B;
	}

	/** A tail bow: a pinched ribbon 9 cm across, 4 deep. */
	const FBuilt& BowMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			Sheet(K, TEXT("Tail"), FVector(0.f, 0.f, 0.f), FVector(-2.f, -4.5f, 0.f), FVector(2.f, -4.5f, 0.f));
			Sheet(K, TEXT("Tail"), FVector(0.f, 0.f, 0.f), FVector(2.f, 4.5f, 0.f), FVector(-2.f, 4.5f, 0.f));
			B = Keep(K, TEXT("SM_KiteBow"));
		}
		return B;
	}

	void PaintSlots(UMeshComponent* C, const TArray<FName>& Slots, const FColours& Col)
	{
		for (int32 I = 0; I < Slots.Num(); ++I)
		{
			const FName S = Slots[I];
			const FLinearColor Tint = S == TEXT("SailA") ? Col.A : S == TEXT("SailB") ? Col.B : S == TEXT("Tail") ? Col.Tail
				: S == TEXT("Line") ? LineTint : S == TEXT("Reel") ? ReelTint : S == TEXT("Drum") ? DrumTint : SparTint;
			if (UMaterialInstanceDynamic* M = Paint(Tint))
			{
				C->SetMaterial(I, M);
			}
		}
	}

	/** The right palm's frame from the hand's bones (X the fingers, Z out of the palm), as the feed bag's (BirdTame.cpp). */
	bool HandPalm(const USkeletalMeshComponent* Body, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		const FVector Across = Body->GetBoneLocation(TEXT("thumb_01_r")) - Body->GetBoneLocation(TEXT("pinky_01_r"));
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		const FVector Palm = Wrist + (Knuckle - Wrist) * 0.6f;
		if (Body->GetBoneIndex(TEXT("thumb_03_r")) != INDEX_NONE && FVector::DotProduct(Body->GetBoneLocation(TEXT("thumb_03_r")) - Palm, Pn) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Palm);
		return true;
	}

	/** The reel held up in front of the chest, the handle in the fist, the drum over it; the hand follows the look up a little. */
	FHeroHoldPose ReelPose()
	{
		FHeroHoldPose P;
		P.Pos = FVector(42.f, 14.f, -38.f);
		P.FingerDir = FVector(0.f, -1.f, 0.f);
		P.PalmNormal = FVector(0.f, 0.f, 1.f) * -1.f + FVector(0.2f, 0.f, 0.f);
		P.PalmNormal.Normalize();
		P.Curl = 1.f;
		P.PitchFollow = 0.35f;
		P.BlendTime = 0.5f;
		FReachPrim Handle;
		Handle.Kind = FReachPrim::EKind::Box;
		Handle.Xf = FTransform(FVector(1.f, 0.f, 1.6f));
		Handle.Extent = FVector(4.5f, 1.2f, 1.2f);
		P.Shape.Prims.Add(Handle);
		return P;
	}

	/** The reel in the palm: the handle across the fist (along the fingers' curl), the drum up out of the thumb side. */
	FTransform ReelInPalm()
	{
		return FTransform(FRotator(0.f, 0.f, 90.f), FVector(1.f, 0.f, 1.8f));
	}

	FBirdPerches& Crowns()
	{
		static FBirdPerches C;
		return C;
	}
}

// ------------------------------------------------------------------ ALyudKite

ALyudKite::ALyudKite()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void ALyudKite::Build(int32 InShape, int32 Colours, bool bWithReel)
{
	Shape = InShape;
	const FColours& Col = Colourways[((Colours % NumColourways) + NumColourways) % NumColourways];
	const FBuilt& Body = Shape == 1 ? BoxMesh() : DiamondMesh();
	Sail = NewObject<UStaticMeshComponent>(this, TEXT("Sail"), RF_Transient);
	Sail->SetupAttachment(Root);
	NoLumen(Sail);
	Sail->SetUsingAbsoluteLocation(true);
	Sail->SetUsingAbsoluteRotation(true);
	Sail->SetUsingAbsoluteScale(true);
	Sail->SetStaticMesh(Body.Mesh);
	PaintSlots(Sail, Body.Slots, Col);
	Sail->SetCastShadow(true);
	Sail->RegisterComponent();

	Line = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Line"), RF_Transient);
	Line->SetupAttachment(Root);
	NoLumen(Line);
	Line->SetUsingAbsoluteLocation(true);
	Line->SetUsingAbsoluteRotation(true);
	Line->SetUsingAbsoluteScale(true);
	Line->SetCastShadow(false);
	Line->SetStaticMesh(PieceMesh().Mesh);
	PaintSlots(Line, PieceMesh().Slots, Col);
	Line->RegisterComponent();
	LineXf.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.01f)), LinePieces);
	Line->AddInstances(LineXf, false, true, false);

	if (Shape == 0)
	{
		Tail = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Tail"), RF_Transient);
		Tail->SetupAttachment(Root);
		NoLumen(Tail);
		Tail->SetUsingAbsoluteLocation(true);
		Tail->SetUsingAbsoluteRotation(true);
		Tail->SetUsingAbsoluteScale(true);
		Tail->SetCastShadow(false);
		Tail->SetStaticMesh(BowMesh().Mesh);
		PaintSlots(Tail, BowMesh().Slots, Col);
		Tail->RegisterComponent();
		TailXf.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.01f)), TailPieces);
		Tail->AddInstances(TailXf, false, true, false);
		TailPts.Init(GetActorLocation(), TailPieces + 1);
	}

	if (bWithReel)
	{
		Reel = NewObject<UStaticMeshComponent>(this, TEXT("Reel"), RF_Transient);
		Reel->SetupAttachment(Root);
		NoLumen(Reel);
		Reel->SetStaticMesh(ReelMesh().Mesh);
		PaintSlots(Reel, ReelMesh().Slots, Col);
		Reel->SetCastShadow(true);
		Reel->RegisterComponent();

		Hum = NewObject<ULyudKiteHumSynth>(this, TEXT("KiteHum"));
		Hum->bAutoActivate = false;
		Hum->bOverrideAttenuation = true;
		FSoundAttenuationSettings& A = Hum->AttenuationOverrides;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(200.f, 0.f, 0.f);
		A.FalloffDistance = 2500.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.NonSpatializedRadiusStart = 150.f;    // in the hand: all round you
		A.NonSpatializedRadiusEnd = 60.f;
		Hum->SetupAttachment(Reel);
		Hum->RegisterComponent();
	}
}

void ALyudKite::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hum)
	{
		Hum->Stop();
	}
	Super::EndPlay(Reason);
}

void ALyudKite::Show(const FVector& KitePos, const FVector& Forward, const FVector& Normal, const TArray<FVector>& Points, const FVector& Wind, float DeltaTime)
{
	if (!Sail)
	{
		return;
	}
	Sail->SetVisibility(true);
	Sail->SetWorldLocationAndRotation(KitePos, FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat());
	// the line: its pieces re-placed in one batch
	if (Line && Points.Num() == LinePieces + 1)
	{
		for (int32 I = 0; I < LinePieces; ++I)
		{
			const FVector Seg = Points[I + 1] - Points[I];
			const float L = Seg.Size();
			const FQuat Q = L > 1e-3f ? FRotationMatrix::MakeFromX(Seg / L).ToQuat() : FQuat::Identity;
			// ~0.6 mm of line drawn 1.2 mm wide so it reads against the sky at a distance
			LineXf[I] = FTransform(Q, Points[I], FVector(FMath::Max(L, 0.01f), 0.06f, 0.06f));
		}
		Line->BatchUpdateInstancesTransforms(0, LineXf, true, false, true);
		Line->SetVisibility(true);
	}
	else if (Line)
	{
		Line->SetVisibility(false);     // no line: a kite caught in a tree, its line broken off
	}
	// the tail: a chain of bows trailing downwind from the sail's foot, each following the one before it
	if (Tail && TailPts.Num() == TailPieces + 1)
	{
		const FVector Foot = KitePos - Forward * 40.f;
		const FVector Down = (Wind.GetSafeNormal2D() * FMath::Clamp(Wind.Size() / 5.f, 0.f, 1.f) + FVector(0.f, 0.f, -0.45f)).GetSafeNormal();
		const float Seg = 16.f;
		const float Ease = 1.f - FMath::Exp(-DeltaTime * 10.f);
		const double T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		TailPts[0] = Foot;
		for (int32 I = 1; I <= TailPieces; ++I)
		{
			if (!bTailSet)
			{
				TailPts[I] = TailPts[I - 1] + Down * Seg;
			}
			// a wave runs down it (the flutter)
			const FVector Across = FVector::CrossProduct(Down, FVector::UpVector).GetSafeNormal();
			const FVector Want = TailPts[I - 1] + (Down + Across * 0.35f * FMath::Sin(T * 7.0 - I * 0.9)).GetSafeNormal() * Seg;
			TailPts[I] = FMath::Lerp(TailPts[I], Want, Ease);
			TailPts[I] = TailPts[I - 1] + (TailPts[I] - TailPts[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Down) * Seg;
			const FVector Dir = (TailPts[I] - TailPts[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Down);
			TailXf[I - 1] = FTransform(FRotationMatrix::MakeFromXZ(Dir, Normal).ToQuat(), TailPts[I], FVector::OneVector);
		}
		bTailSet = true;
		Tail->BatchUpdateInstancesTransforms(0, TailXf, true, false, true);
		Tail->SetVisibility(true);
	}
}

void ALyudKite::ShowWound()
{
	if (Sail) Sail->SetVisibility(false);
	if (Line) Line->SetVisibility(false);
	if (Tail) Tail->SetVisibility(false);
	bTailSet = false;
}

void ALyudKite::SetHum(float InHum, float Air, float Gain)
{
	if (!Hum)
	{
		return;
	}
	const bool bWant = InHum > 0.01f && Gain > 0.f;
	if (bWant && !bHumOn)
	{
		Hum->Start();
		bHumOn = true;
	}
	else if (!bWant && bHumOn && InHum <= 0.f)
	{
		Hum->Stop();
		bHumOn = false;
	}
	Hum->SetHum(InHum, Air, Gain);
}

void ALyudKite::TurnReel(float Metres)
{
	if (!Reel)
	{
		return;
	}
	// a 7 cm drum: a metre of line is ~4.5 turns
	ReelAngle = FMath::Fmod(ReelAngle + Metres * 1640.f, 360.f);
	Reel->SetRelativeRotation(ReelInPalm().GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(ReelAngle)));
}

// ------------------------------------------------------------------ ULyudKites

ULyudKites::ULyudKites() = default;
ULyudKites::~ULyudKites() = default;

ULyudKites* ULyudKites::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudKites>() : nullptr;
}

bool ULyudKites::IsKite(FName Item)
{
	return Item == KiteDiamondId || Item == KiteBoxId;
}

bool ULyudKites::HasHand(const UObject* WorldContext)
{
	const ULyudKites* K = Get(WorldContext);
	return K && K->IsOut();
}

void ULyudKites::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void ULyudKites::Deinitialize()
{
	for (ALyudKite* K : Made)
	{
		if (IsValid(K))
		{
			K->Destroy();
		}
	}
	Made.Reset();
	Pool.Reset();
	Town.Reset();
	Tests.Reset();
	Super::Deinitialize();
}

ALyudKite* ULyudKites::MakeActor(int32 Shape, int32 Colours, bool bReel)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// a parked one of the same make, if there is one (the town's kites come and go with the children)
	if (!bReel)
	{
		for (int32 I = Pool.Num() - 1; I >= 0; --I)
		{
			ALyudKite* K = Pool[I];
			if (IsValid(K) && K->GetShape() == Shape && !K->GetReel())
			{
				Pool.RemoveAtSwap(I);
				K->SetActorHiddenInGame(false);
				return K;
			}
		}
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudKite* K = World->SpawnActor<ALyudKite>(ALyudKite::StaticClass(), FTransform::Identity, P);
	if (K)
	{
		K->Build(Shape, Colours, bReel);
		Made.Add(K);
	}
	return K;
}

void ULyudKites::Park(ALyudKite* Kite)
{
	if (!IsValid(Kite))
	{
		return;
	}
	Kite->SetHum(0.f, 0.f, 0.f);
	Kite->ShowWound();
	Kite->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Kite->SetActorHiddenInGame(true);
	if (Kite->GetReel())
	{
		// the hero's reel kites are few (one at a time): gone with the take-out
		Made.Remove(Kite);
		Kite->Destroy();
		return;
	}
	Pool.Add(Kite);
}

float ULyudKites::SoundGain() const
{
	float Gain = 0.6f;
	if (const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Gain *= Scape->MasterVolume * Scape->AmbienceVolume;
	}
	return Gain;
}

void ULyudKites::Fly(FFlight& F, float Reel, float DeltaTime, const AActor* Ignore)
{
	UWorld* World = GetWorld();
	LyudKite::FKite& K = *F.Sim;
	ALyudKite* Actor = F.Actor.Get();
	if (K.State == LyudKite::EState::Wound)
	{
		LyudKite::FKiteInput In;
		In.Hand = ToM(F.Hand);
		LyudKite::Step(K, In, DeltaTime);
		if (Actor)
		{
			Actor->ShowWound();
		}
		return;
	}
	const FVector Before = ToCm(K.Pos);
	// the ground under the kite now and then (sand, the cliff top, a roof), the sea where it is over water
	F.GroundIn -= DeltaTime;
	if (F.GroundIn <= 0.f)
	{
		F.GroundIn = 0.15f;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteGround), false);
		if (Ignore) Q.AddIgnoredActor(Ignore);
		if (Actor) Q.AddIgnoredActor(Actor);
		const bool bHit = World->LineTraceSingleByChannel(Hit, Before + FVector(0, 0, 50), Before - FVector(0, 0, 20000), ECC_Visibility, Q);
		F.GroundZ = bHit ? (float)Hit.ImpactPoint.Z : F.Hand.Z - 150.f;
		F.bWater = false;
		if (const ALyudSea* Sea = ALyudSea::Get(this))
		{
			const float Level = Sea->SeaLevel();
			if ((!bHit || F.GroundZ < Level) && Sea->IsSwimmable(FVector(Before.X, Before.Y, Level - 50.f)))
			{
				F.GroundZ = Level;
				F.bWater = true;
			}
		}
	}
	FVector Wind = FVector::ZeroVector;
	if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
	{
		const float Height = FMath::Max(0.3f, float(Before.Z - F.GroundZ) * 0.01f);
		Wind = WindSys->GetWindAt(Before, Height, F.bWater).Velocity;
	}
	LyudKite::FKiteInput In;
	In.Hand = ToM(F.Hand);
	In.Wind = LyudKite::FVec(Wind.X, Wind.Y, Wind.Z);
	In.GroundZ = F.GroundZ * 0.01;
	In.Reel = Reel;
	LyudKite::Step(K, In, DeltaTime);
	const FVector After = ToCm(K.Pos);

	// what it flew into: anything static that isn't the ground (a tree's trunk, a lamp post, a wall, a roof), or into a
	// tree's crown (the leaves have no collision: their ellipsoids, as the birds' perches know them)
	if (LyudKite::IsUp(K) && FVector::DistSquared(Before, After) > 1.f)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteSnag), false);
		if (Ignore) Q.AddIgnoredActor(Ignore);
		if (Actor) Q.AddIgnoredActor(Actor);
		const FCollisionObjectQueryParams Obj(ECC_WorldStatic);
		bool bSnag = false;
		if (World->SweepSingleByObjectType(Hit, Before, After, FQuat::Identity, Obj, FCollisionShape::MakeSphere(K.Def.Shape == LyudKite::EShape::Box ? 30.f : 25.f), Q))
		{
			bSnag = !Cast<ALandscapeProxy>(Hit.GetActor());
		}
		bSnag = bSnag || (After.Z - F.GroundZ > 250.f && Crowns().InCrown(World, After));
		if (bSnag)
		{
			LyudKite::FKiteInput Caught = In;
			Caught.bHitObstacle = true;
			LyudKite::Step(K, Caught, 0.0);
		}
	}
	if (!Actor)
	{
		return;
	}
	LyudKite::FVec Fwd, Nrm;
	LyudKite::Frame(K, In.Hand, In.Wind, Fwd, Nrm);
	LyudKite::FVec Pts[ALyudKite::LinePieces + 1];
	LyudKite::LinePoints(K, In.Hand, ALyudKite::LinePieces, Pts);
	static TArray<FVector> Points;   // reused: no allocation per frame
	Points.SetNum(ALyudKite::LinePieces + 1, EAllowShrinking::No);
	for (int32 I = 0; I <= ALyudKite::LinePieces; ++I)
	{
		FVector P = ToCm(Pts[I]);
		// a slack line lies on the ground, not through it
		P.Z = FMath::Max(P.Z, double(F.GroundZ) + 2.0);
		Points[I] = P;
	}
	Points[0] = F.Hand;
	if (LyudKite::IsLost(K))
	{
		// caught: it stays where it is, the line gone
		Actor->Show(After, ToDir(Fwd), ToDir(Nrm), TArray<FVector>(), Wind, DeltaTime);
		return;
	}
	Actor->Show(After, ToDir(Fwd), ToDir(Nrm), Points, Wind, DeltaTime);
}

void ULyudKites::Tick(float DeltaTime)
{
	if (CVarKites.GetValueOnGameThread() == 0 || DeltaTime <= 0.f)
	{
		return;
	}
	DeltaTime = FMath::Min(DeltaTime, 0.1f);
	TickHero(DeltaTime);
	TickTown(DeltaTime);
	TickTests(DeltaTime);
}

void ULyudKites::Say(const TCHAR* Key, float Seconds)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, LyudText::Get(Key));
	}
	SayLeft = Seconds;
}

void ULyudKites::Stamp(const TCHAR* Piece)
{
	if (ULyudCityThings* City = ULyudCityThings::Get(this))
	{
		City->GiveStamp(FString(TEXT("kites/")) + Piece);
	}
}

void ULyudKites::TakeOut(FName Item)
{
	ALyudKite* K = MakeActor(Item == KiteBoxId ? 1 : 0, Item == KiteBoxId ? 1 : 0, true);
	if (!K)
	{
		return;
	}
	HeroKite = K;
	HeroItem = Item;
	Hero = FFlight();
	Hero.Sim = MakeShared<LyudKite::FKite>();
	Hero.Sim->Def = Item == KiteBoxId ? LyudKite::Box() : LyudKite::Diamond();
	Hero.Sim->Seed = 7;
	Hero.Actor = K;
	K->ShowWound();
	K->SetActorHiddenInGame(true);
	bAttached = false;
	bWaitUp = true;
	bWindingAway = false;
	HoldTime = 0.f;
	UpFor = 0.f;
	WheelReel = 0.f;
	PrevState = (int32)LyudKite::EState::Wound;
	bWasSpinning = false;
	Say(TEXT("ui.kite.keys_wound"), 4.f);
}

void ULyudKites::PutAway()
{
	if (AHeroCharacter* H = HeldBy.Get())
	{
		if (UHeroReachComponent* Reach = UHeroReachComponent::Get(H))
		{
			Reach->ClearHold();
		}
	}
	Park(HeroKite.Get());
	HeroKite.Reset();
	Hero = FFlight();
	HeroItem = FName();
	bAttached = false;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
}

void ULyudKites::Lose(const TCHAR* Why)
{
	ULyudInventory* Inv = ULyudInventory::Get(GetWorld());
	const FName Item = HeroItem;
	// the kite stays where it was caught (a tree, a lamp post) for the town to look at: a test kite's flight of its own
	if (Hero.Sim && Hero.Sim->State == LyudKite::EState::Snagged && HeroKite.IsValid())
	{
		ALyudKite* K = HeroKite.Get();
		K->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		K->SetHum(0.f, 0.f, 0.f);
		FFlight Left;
		Left.Sim = Hero.Sim;
		Left.Actor = K;
		Left.Hand = Hero.Hand;
		Left.RelaunchIn = 600.f;   // there for ten minutes, or until the hero is far off
		Tests.Add(MoveTemp(Left));
		HeroKite.Reset();
	}
	PutAway();
	if (Inv && !Item.IsNone())
	{
		Inv->Remove(Item, 1);
		if (Inv->Count(Item) <= 0)
		{
			Inv->Equip(FName());
		}
	}
	Say(Why, 4.f);
}

void ULyudKites::TickHero(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* H = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	if (!H || !PC || !Inv)
	{
		// a bird is flown: the kite is wound in and put away; it comes back out with the hero
		if (HeroKite.IsValid())
		{
			PutAway();
		}
		return;
	}
	const FName Hand = Inv->EquippedItem();
	if (IsKite(Hand) && !HeroKite.IsValid())
	{
		TakeOut(Hand);
	}
	else if (IsKite(Hand) && Hand != HeroItem)
	{
		PutAway();
		TakeOut(Hand);
	}
	else if (!IsKite(Hand) && HeroKite.IsValid())
	{
		PutAway();
	}
	ALyudKite* K = HeroKite.Get();
	if (!K || !Hero.Sim)
	{
		return;
	}
	HeldBy = H;
	LyudKite::FKite& Sim = *Hero.Sim;
	UHeroReachComponent* Reach = UHeroReachComponent::Get(H);
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const ULyudSwimComponent* Swim = H->FindComponentByClass<ULyudSwimComponent>();
	if (Swim && Swim->IsSwimming())
	{
		// into the water with it: wound in at once and away (it would only drown)
		Inv->Equip(FName());
		PutAway();
		return;
	}
	const bool bBusy = (Screen && Screen->IsOpen()) || (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || !Reach || Reach->IsBusy() || H->IsHidden();

	// the hand: the reel held up in front; it rides on the hand bone once the hand is there
	if (Reach && !Reach->HasOverrideHold())
	{
		Reach->SetHold(ReelPose());
	}
	HoldTime += DeltaTime;
	if (!bAttached && Reach && Reach->HoldWeight() > 0.6f)
	{
		FTransform Palm;
		if (HandPalm(H->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), Palm))
		{
			K->SetActorTransform(Palm);
			K->AttachToComponent(H->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
			if (UStaticMeshComponent* R = K->GetReel())
			{
				R->SetRelativeTransform(ReelInPalm());
			}
			K->SetActorHiddenInGame(false);
			bAttached = true;
		}
	}
	// the line leaves the reel at its drum (before the hand is up: from in front of the chest)
	const FVector NewHand = bAttached && K->GetReel() ? K->GetReel()->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, 2.f))
		: H->EyeLocation() + H->GetActorForwardVector() * 40.f - FVector(0, 0, 40.f);
	// a jump of the hero (a teleport, a seat, a ride): the kite can't follow; it is wound in
	if (!Hero.Hand.IsZero() && FVector::DistSquared(NewHand, Hero.Hand) > FMath::Square(3000.f) && Sim.State != LyudKite::EState::Wound)
	{
		Sim.Line = 0.0;
	}
	Hero.Hand = NewHand;

	// the buttons (only once they were let go after a screen; nothing while a screen is up: the kite flies on as it was)
	const bool bInput = !bBusy && !PC->IsMoveInputIgnored() && bAttached;
	bool bL = bInput && PC->IsInputKeyDown(EKeys::LeftMouseButton);
	bool bR = bInput && PC->IsInputKeyDown(EKeys::RightMouseButton);
	if (bWaitUp)
	{
		bWaitUp = bL || bR;
		bL = bR = false;
	}
	if (bForced)
	{
		bL = ForcedReel < 0;
		bR = ForcedReel > 0;
	}
	if (bInput && PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) WheelReel += 1.f;
	if (bInput && PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) WheelReel -= 1.f;
	if (bInput && PC->WasInputKeyJustPressed(EKeys::Q))
	{
		if (Sim.State == LyudKite::EState::Wound)
		{
			Inv->Equip(FName());
			PutAway();
			return;
		}
		bWindingAway = true;    // wound all the way in first, then away
		Say(TEXT("ui.kite.winding"), 3.f);
	}
	float Reel = 0.f;
	if (Sim.State == LyudKite::EState::Wound)
	{
		if (bWindingAway)
		{
			bWindingAway = false;
			Inv->Equip(FName());
			PutAway();
			return;
		}
		const bool bToss = (bInput && !bWaitUp && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) || bLaunchQueued;
		bLaunchQueued = false;
		if (bToss)
		{
			FVector Wind = FVector::ZeroVector;
			if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
			{
				Wind = WindSys->GetWindAt(Hero.Hand, 3.f, false).Velocity;
			}
			LyudKite::Launch(Sim, ToM(Hero.Hand), LyudKite::FVec(Wind.X, Wind.Y, Wind.Z));
			Hero.GroundIn = 0.f;
			UpFor = 0.f;
			Say(Wind.Size() < 2.5f ? TEXT("ui.kite.toss_calm") : TEXT("ui.kite.toss"), 3.f);
			bWaitUp = true;     // the press that tossed it doesn't pull it straight back in
		}
	}
	else if (bWindingAway)
	{
		Reel = -2.f * LyudKite::ReelIn;
	}
	else
	{
		if (bL) Reel -= LyudKite::ReelIn;
		if (bR) Reel += LyudKite::ReelOut;
		if (FMath::Abs(WheelReel) > 0.01f)
		{
			// a click of the wheel: a metre out or in, run at 4 m/s
			const float Step = FMath::Clamp(WheelReel, -4.f * DeltaTime, 4.f * DeltaTime);
			WheelReel -= Step;
			Reel += Step / DeltaTime;
		}
	}
	const double LineBefore = Sim.Line;
	Fly(Hero, Reel, DeltaTime, H);
	K->TurnReel(float(Sim.Line - LineBefore));
	ClickIn -= DeltaTime;
	if (Sim.Line < LineBefore - 0.01 && ClickIn <= 0.f)
	{
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
		{
			S->PlayEffect(TEXT("reel_click"), Hero.Hand, 0.3f);
		}
		ClickIn = 0.4f;
	}
	K->SetHum((float)LyudKite::Hum(Sim), (float)Sim.Air, SoundGain());

	// what happened: lines, stamps, a lost kite
	const LyudKite::EState Now = Sim.State;
	const LyudKite::EState Was = (LyudKite::EState)PrevState;
	PrevState = (int32)Now;
	if (Now == LyudKite::EState::Snagged)
	{
		Lose(TEXT("ui.kite.snagged"));
		return;
	}
	if (Now == LyudKite::EState::Broken)
	{
		// the line snapped: the kite is off with the wind, out of sight
		Lose(TEXT("ui.kite.snapped"));
		return;
	}
	if (Now == LyudKite::EState::Grounded && Hero.bWater && Was != LyudKite::EState::Grounded)
	{
		Lose(TEXT("ui.kite.sea"));
		return;
	}
	if (Now == LyudKite::EState::Grounded && Was != LyudKite::EState::Grounded)
	{
		Say(Sim.Air < 3.0 ? TEXT("ui.kite.fell_calm") : TEXT("ui.kite.fell"), 3.5f);
	}
	if (Now == LyudKite::EState::Spinning && Was != LyudKite::EState::Spinning)
	{
		Say(TEXT("ui.kite.spinning"), 3.5f);
		bWasSpinning = true;
	}
	if (Now == LyudKite::EState::Flying && Was == LyudKite::EState::Spinning)
	{
		Say(TEXT("ui.kite.steady"), 3.f);
		Stamp(TEXT("storm"));
	}
	if (LyudKite::IsUp(Sim))
	{
		UpFor += DeltaTime;
		if (UpFor > 20.f && UpFor - DeltaTime <= 20.f)
		{
			Stamp(TEXT("first_flight"));
			Say(TEXT("ui.kite.flying"), 3.f);
		}
		if (Now == LyudKite::EState::Flying && Sim.Pos.Z - Hero.Hand.Z * 0.01 > 30.0)
		{
			Stamp(TEXT("high"));
		}
		if (HeroItem == KiteBoxId && UpFor > 30.f)
		{
			Stamp(TEXT("box"));
		}
	}
	else
	{
		UpFor = 0.f;
	}
	// the prompt: what was said, then what the keys do
	SayLeft -= DeltaTime;
	if (SayLeft <= 0.f && !bBusy)
	{
		if (ULyudInventoryUI* UIw = ULyudInventoryUI::Get(World))
		{
			UIw->SetPrompt(this, LyudText::Get(Now == LyudKite::EState::Wound ? TEXT("ui.kite.keys_wound") : TEXT("ui.kite.keys_up")));
		}
		SayLeft = 1e9f;   // shown until something else is said
	}
	if (CVarKiteDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE + 7711, 1.f, FColor::Cyan, DebugState());
	}
}

FString ULyudKites::DebugState() const
{
	if (!Hero.Sim)
	{
		return FString::Printf(TEXT("KITE none in hand; town %d, tests %d"), Town.Num(), Tests.Num());
	}
	const LyudKite::FKite& K = *Hero.Sim;
	return FString::Printf(TEXT("KITE %s %s line %.1f m height %.1f m elev %.0f air %.1f m/s lift %.1f N tension %.1f N hum %.2f; town %d, tests %d"),
		*HeroItem.ToString(), UTF8_TO_TCHAR(LyudKite::StateName(K.State)), K.Line, K.Pos.Z - Hero.Hand.Z * 0.01,
		LyudKite::Elevation(K, ToM(Hero.Hand)), K.Air, K.Lift, K.Tension, LyudKite::Hum(K), Town.Num(), Tests.Num());
}

void ULyudKites::TickTown(float DeltaTime)
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!People || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	if (FlyKiteAct == -2)
	{
		FlyKiteAct = People->FindActivity(TEXT("fly_kite"));
	}
	if (FlyKiteAct < 0)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const int32 Max = FMath::Max(0, CVarTownKites.GetValueOnGameThread());
	// who is flying one now, near the camera (twice a second: a scan of the town is ~1000 cheap looks)
	TownScanIn -= DeltaTime;
	if (TownScanIn <= 0.f)
	{
		TownScanIn = 0.5f;
		for (FFlight& F : Town)
		{
			F.bSeen = false;
		}
		const int32 N = People->NumPeople();
		for (int32 Id = 0; Id < N; ++Id)
		{
			int32 Act = -1;
			bool bDoing = false;
			EPersonPose Pose;
			FVector Loc;
			if (!People->GetActivityState(Id, Act, bDoing, Pose, Loc) || Act != FlyKiteAct || !bDoing
				|| FVector::DistSquared(Loc, Cam) > FMath::Square(TownDrawCm) || !People->GetShownBody(Id))
			{
				continue;
			}
			FFlight* Have = Town.FindByPredicate([Id](const FFlight& F) { return F.Person == Id; });
			if (Have)
			{
				Have->bSeen = true;
				continue;
			}
			if (Town.Num() >= Max)
			{
				continue;
			}
			// a kite of their own: a box kite for one in three, the colours by who they are
			const int32 Shape = (Id % 3) == 0 ? 1 : 0;
			ALyudKite* K = MakeActor(Shape, Id, false);
			if (!K)
			{
				continue;
			}
			FFlight F;
			F.Sim = MakeShared<LyudKite::FKite>();
			F.Sim->Def = Shape == 1 ? LyudKite::Box() : LyudKite::Diamond();
			F.Sim->Seed = uint32(Id) * 2654435761u;
			F.Actor = K;
			F.Person = Id;
			F.Want = 16.f + float(Id % 19);
			F.Hand = Loc + FVector(0, 0, 120.f);
			F.RelaunchIn = 0.5f + 0.1f * float(Id % 13);
			F.bSeen = true;
			Town.Add(MoveTemp(F));
		}
		for (int32 I = Town.Num() - 1; I >= 0; --I)
		{
			if (!Town[I].bSeen)
			{
				Park(Town[I].Actor.Get());
				Town.RemoveAtSwap(I);
			}
		}
	}
	for (FFlight& F : Town)
	{
		LyudKite::FKite& K = *F.Sim;
		if (LyudKite::IsLost(K))
		{
			// caught or snapped: that child's kite is gone for this outing (left where it is caught)
			if (K.State == LyudKite::EState::Broken && F.Actor.IsValid())
			{
				F.Actor->ShowWound();
			}
			continue;
		}
		// the line from their right hand (held up a little, as one does flying a kite)
		if (const AActor* Body = People->GetShownBody(F.Person))
		{
			const USkeletalMeshComponent* Mesh = Body->FindComponentByClass<USkeletalMeshComponent>();
			F.Hand = Mesh && Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? Mesh->GetBoneLocation(TEXT("hand_r")) : Body->GetActorLocation() + FVector(0, 0, 120.f);
		}
		float Reel = 0.f;
		if (K.State == LyudKite::EState::Wound)
		{
			F.RelaunchIn -= DeltaTime;
			if (F.RelaunchIn <= 0.f)
			{
				FVector Wind = FVector::ZeroVector;
				if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
				{
					Wind = WindSys->GetWindAt(F.Hand, 3.f, false).Velocity;
				}
				LyudKite::Launch(K, ToM(F.Hand), LyudKite::FVec(Wind.X, Wind.Y, Wind.Z));
				F.RelaunchIn = 4.f;
			}
		}
		else
		{
			Reel = (float)LyudKite::AutoReel(K, ToM(F.Hand), F.Want);
		}
		Fly(F, Reel, DeltaTime, People->GetShownBody(F.Person));
	}
}

void ULyudKites::SpawnTestKites(int32 N)
{
	for (FFlight& F : Tests)
	{
		Park(F.Actor.Get());
	}
	Tests.Reset();
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UWorld* World = GetWorld();
	if (N <= 0 || !PC || !PC->PlayerCameraManager || !World)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	FVector Down(1, 0, 0);
	if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
	{
		Down = WindSys->GetMeanWind().GetSafeNormal2D(UE_SMALL_NUMBER, Down);
	}
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Down);
	for (int32 I = 0; I < N; ++I)
	{
		// a row of flyers across the wind, 6 m apart, 25 m downwind of the camera
		FVector At = Cam + Down * 2500.f + Across * (float(I) - 0.5f * float(N - 1)) * 600.f;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteTest), false);
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 5000), At - FVector(0, 0, 20000), ECC_Visibility, Q))
		{
			At = Hit.ImpactPoint;
		}
		const int32 Shape = I % 3 == 2 ? 1 : 0;
		ALyudKite* K = MakeActor(Shape, I, false);
		if (!K)
		{
			continue;
		}
		FFlight F;
		F.Sim = MakeShared<LyudKite::FKite>();
		F.Sim->Def = Shape == 1 ? LyudKite::Box() : LyudKite::Diamond();
		F.Sim->Seed = uint32(I + 1) * 40503u;
		F.Actor = K;
		F.Hand = At + FVector(0, 0, 130.f);
		F.Want = 20.f + 3.f * float(I % 6);
		F.RelaunchIn = 0.2f * float(I);
		Tests.Add(MoveTemp(F));
	}
}

void ULyudKites::TickTests(float DeltaTime)
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (int32 I = Tests.Num() - 1; I >= 0; --I)
	{
		FFlight& F = Tests[I];
		LyudKite::FKite& K = *F.Sim;
		if (LyudKite::IsLost(K))
		{
			// the hero's kite left in a tree: there for a while, gone when he is far off
			F.RelaunchIn -= DeltaTime;
			if (F.RelaunchIn <= 0.f || FVector::DistSquared(Cam, ToCm(K.Pos)) > FMath::Square(25000.f))
			{
				Made.Remove(F.Actor.Get());
				if (ALyudKite* A = F.Actor.Get())
				{
					A->Destroy();
				}
				Tests.RemoveAtSwap(I);
			}
			continue;
		}
		float Reel = 0.f;
		if (K.State == LyudKite::EState::Wound)
		{
			F.RelaunchIn -= DeltaTime;
			if (F.RelaunchIn <= 0.f)
			{
				FVector Wind = FVector::ZeroVector;
				if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
				{
					Wind = WindSys->GetWindAt(F.Hand, 3.f, false).Velocity;
				}
				LyudKite::Launch(K, ToM(F.Hand), LyudKite::FVec(Wind.X, Wind.Y, Wind.Z));
				F.RelaunchIn = 3.f;
			}
		}
		else
		{
			Reel = (float)LyudKite::AutoReel(K, ToM(F.Hand), F.Want);
		}
		Fly(F, Reel, DeltaTime, nullptr);
	}
}

// ------------------------------------------------------------------ console, developer mode

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GKiteTest(TEXT("lyud.Kite.Test"), TEXT("lyud.Kite.Test N: N kites flown in a row 25 m downwind of the camera (0 clears; the bench uses 10)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudKites* K = ULyudKites::Get(W)) K->SpawnTestKites(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10);
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteGive(TEXT("lyud.Kite.Give"), TEXT("lyud.Kite.Give [diamond|box]: a kite in the pocket and in the hand"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudInventory* Inv = ULyudInventory::Get(W);
			if (!Inv) return;
			const FName Id = Args.Num() > 0 && Args[0] == TEXT("box") ? KiteBoxId : KiteDiamondId;
			Inv->Add(Id);
			Inv->Equip(Id);
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteReel(TEXT("lyud.Kite.Reel"), TEXT("lyud.Kite.Reel -1|0|1|off: pull, hold, let out (as the mouse buttons would); off gives the mouse back"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudKites* K = ULyudKites::Get(W);
			if (!K || Args.Num() == 0) return;
			if (Args[0] == TEXT("off")) K->ReleaseReel();
			else K->ForceReel(FCString::Atoi(*Args[0]));
		}));
	FAutoConsoleCommandWithWorld GKiteLaunch(TEXT("lyud.Kite.Launch"), TEXT("lyud.Kite.Launch: toss the kite in the hand up"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKites* K = ULyudKites::Get(W)) K->ForceLaunch(); }));
	FAutoConsoleCommandWithWorld GKiteState(TEXT("lyud.Kite.State"), TEXT("lyud.Kite.State: the hero's kite in the log (KITE ...)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKites* K = ULyudKites::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *K->DebugState()); }));

	LyudDev::FAutoAction GDevKiteGive(TEXT("kite.give"), TEXT("kite"), TEXT("dev.action.kite.give"),
		[](UWorld* W) { if (ULyudInventory* Inv = ULyudInventory::Get(W)) { Inv->Add(KiteDiamondId); Inv->Add(KiteBoxId); Inv->Equip(KiteDiamondId); } });
	LyudDev::FAutoAction GDevKiteTest(TEXT("kite.test"), TEXT("kite"), TEXT("dev.action.kite.test"),
		[](UWorld* W) { if (ULyudKites* K = ULyudKites::Get(W)) K->SpawnTestKites(10); });
}
