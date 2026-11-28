// Kites: the hero's on its reel or its two handles, the town's, the bench's test kites, the ones caught in trees (see
// Public/Kite/LyudKite.h, docs/kites.md).
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
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "../Transport/LyudMeshKit.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarKites(TEXT("lyud.Kite"), 1, TEXT("lyud.Kite 0|1: the kites (the hero's and the town's)"));
	TAutoConsoleVariable<int32> CVarTownKites(TEXT("lyud.Kite.Town"), 12, TEXT("lyud.Kite.Town N: at most N townsfolk's kites drawn (0 none)"));
	TAutoConsoleVariable<int32> CVarKiteDebug(TEXT("lyud.Kite.Debug"), 0, TEXT("lyud.Kite.Debug 1: the hero's kite's state on screen each second"));

	const FName KiteDiamondId(TEXT("kite_diamond"));
	const FName KiteBoxId(TEXT("kite_box"));
	const FName KiteDeltaId(TEXT("kite_delta"));
	const FName KiteStuntId(TEXT("kite_stunt"));
	const FName KiteDragonId(TEXT("kite_dragon"));
	constexpr float TownDrawCm = 30000.f;     // the town's kites are drawn within 300 m of the camera

	/** The shapes as numbers (LyudKite::EShape), as ALyudKite and the flights keep them. */
	constexpr int32 ShapeDiamond = (int32)LyudKite::EShape::Diamond;
	constexpr int32 ShapeBox = (int32)LyudKite::EShape::Box;
	constexpr int32 ShapeDelta = (int32)LyudKite::EShape::Delta;
	constexpr int32 ShapeStunt = (int32)LyudKite::EShape::Stunt;
	constexpr int32 ShapeDragon = (int32)LyudKite::EShape::Dragon;

	/** The kind of kite an item is, or -1. */
	int32 ShapeOfItem(FName Item)
	{
		return Item == KiteDiamondId ? ShapeDiamond : Item == KiteBoxId ? ShapeBox : Item == KiteDeltaId ? ShapeDelta
			: Item == KiteStuntId ? ShapeStunt : Item == KiteDragonId ? ShapeDragon : -1;
	}
	FName ItemOfShape(int32 Shape)
	{
		return Shape == ShapeBox ? KiteBoxId : Shape == ShapeDelta ? KiteDeltaId : Shape == ShapeStunt ? KiteStuntId : Shape == ShapeDragon ? KiteDragonId : KiteDiamondId;
	}
	/** A shape by its name in the console (diamond, box, delta, stunt, dragon), or Fallback. */
	int32 ShapeByName(const FString& Name, int32 Fallback)
	{
		for (int32 I = 0; I < LyudKite::NumShapes; ++I)
		{
			if (Name.Equals(UTF8_TO_TCHAR(LyudKite::ShapeName((LyudKite::EShape)I)), ESearchCase::IgnoreCase))
			{
				return I;
			}
		}
		return Fallback;
	}

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
		{ FLinearColor(0.05f, 0.50f, 0.52f), FLinearColor(0.93f, 0.93f, 0.90f), FLinearColor(0.95f, 0.45f, 0.10f) },
		{ FLinearColor(0.90f, 0.25f, 0.50f), FLinearColor(0.10f, 0.12f, 0.40f), FLinearColor(0.96f, 0.86f, 0.20f) },
		{ FLinearColor(0.55f, 0.80f, 0.12f), FLinearColor(0.07f, 0.07f, 0.08f), FLinearColor(0.93f, 0.93f, 0.90f) },
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

	/** A sheet with four corners (A B C D round its edge). */
	void Sheet4(FLyudMeshKit& K, FName Mat, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		Sheet(K, Mat, A, B, C);
		Sheet(K, Mat, A, C, D);
	}

	/**
	 * A delta: a triangle 156 cm across and 80 long on a spine, two leading-edge spars and a spreader, with a keel under
	 * it; the line ties on at the keel's tip (the origin), so the wing rides 24 cm behind the bridle point.
	 */
	const FBuilt& DeltaMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			const float W = 24.f;
			const FVector Nose(48.f, 0.f, W), Tail(-32.f, 0.f, W), L(-32.f, -78.f, W + 7.f), R(-32.f, 78.f, W + 7.f);
			Sheet(K, TEXT("SailA"), Nose, L, Tail);
			Sheet(K, TEXT("SailB"), Nose, Tail, R);
			Sheet(K, TEXT("Tail"), FVector(30.f, 0.f, W - 0.5f), FVector(-22.f, 0.f, W - 0.5f), FVector::ZeroVector);     // the keel
			K.Rod(TEXT("Spar"), Tail + FVector(0, 0, -0.6f), Nose + FVector(0, 0, -0.6f), 0.45f, 5, true, false);
			K.Rod(TEXT("Spar"), Nose, L, 0.4f, 5, true, false);
			K.Rod(TEXT("Spar"), Nose, R, 0.4f, 5, true, false);
			K.Rod(TEXT("Spar"), (Nose + L) * 0.5f + FVector(0, 0, -0.8f), (Nose + R) * 0.5f + FVector(0, 0, -0.8f), 0.35f, 4, true, false);
			B = Keep(K, TEXT("SM_KiteDelta"));
		}
		return B;
	}

	/**
	 * A stunt kite: a swept wing 184 cm across, three panels a side, on carbon leading edges, a spine and a spreader. Its
	 * two lines tie on ALyudKite::StuntBridle either side of the middle.
	 */
	const FBuilt& StuntMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			const FVector Nose(42.f, 0.f, 0.f), Centre(-18.f, 0.f, 0.f);
			for (const float S : { -1.f, 1.f })
			{
				const FVector Tip(-38.f, 92.f * S, 5.f), Mid(2.f, 46.f * S, 2.5f), Back(-28.f, 46.f * S, 2.5f);
				Sheet(K, TEXT("SailA"), Nose, Mid, Centre);
				Sheet(K, TEXT("SailB"), Mid, Back, Centre);
				Sheet(K, TEXT("Tail"), Mid, Tip, Back);
				K.Rod(TEXT("Spar"), Nose, Tip, 0.4f, 5, true, false);
			}
			K.Rod(TEXT("Spar"), Centre + FVector(0, 0, -0.6f), Nose + FVector(0, 0, -0.6f), 0.4f, 5, true, false);
			K.Rod(TEXT("Spar"), FVector(2.f, -46.f, 1.5f), FVector(2.f, 46.f, 1.5f), 0.35f, 4, true, false);
			B = Keep(K, TEXT("SM_KiteStunt"));
		}
		return B;
	}

	/** A dragon's face: a hoop 56 cm across, the cloth in twelve wedges of two colours round a middle of the tail's colour, two eyes on the flyer's side. */
	const FBuilt& DragonMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			const float R = 28.f;
			auto Rim = [](int32 I, float Radius)
			{
				const float A = float(I % 12) * (UE_TWO_PI / 12.f);
				return FVector(Radius * FMath::Cos(A), Radius * FMath::Sin(A), 0.f);
			};
			for (int32 I = 0; I < 12; ++I)
			{
				const FName Mat = (I % 2) == 0 ? FName(TEXT("SailA")) : FName(TEXT("SailB"));
				Sheet4(K, Mat, Rim(I, R * 0.5f), Rim(I, R), Rim(I + 1, R), Rim(I + 1, R * 0.5f));
				Sheet(K, TEXT("Tail"), FVector::ZeroVector, Rim(I, R * 0.5f), Rim(I + 1, R * 0.5f));
				K.Rod(TEXT("Spar"), Rim(I, R), Rim(I + 1, R), 0.35f, 4, true, false);
			}
			for (const float S : { -1.f, 1.f })
			{
				const float Y = 10.f * S;
				Sheet4(K, TEXT("Eye"), FVector(15.f, Y, -0.6f), FVector(9.f, Y - 4.5f, -0.6f), FVector(3.f, Y, -0.6f), FVector(9.f, Y + 4.5f, -0.6f));
				Sheet4(K, TEXT("Spar"), FVector(11.f, Y, -1.f), FVector(9.f, Y - 1.8f, -1.f), FVector(7.f, Y, -1.f), FVector(9.f, Y + 1.8f, -1.f));
			}
			B = Keep(K, TEXT("SM_KiteDragon"));
		}
		return B;
	}

	/** A piece of the dragon's tail: a ribbon 30 cm long (back along -X from where it is placed) and 16 wide, in two bands. */
	const FBuilt& RibbonMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			Sheet4(K, TEXT("Tail"), FVector(-30.f, -8.f, 0.f), FVector(-15.f, -8.f, 0.f), FVector(-15.f, 8.f, 0.f), FVector(-30.f, 8.f, 0.f));
			Sheet4(K, TEXT("SailB"), FVector(-15.f, -8.f, 0.f), FVector(0.f, -8.f, 0.f), FVector(0.f, 8.f, 0.f), FVector(-15.f, 8.f, 0.f));
			B = Keep(K, TEXT("SM_KiteRibbon"));
		}
		return B;
	}

	/** A stunt kite's handle: a padded bar along Z (it lies across the fist as the reel's handle does), a knob at each end. */
	const FBuilt& HandleMesh()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Rod(TEXT("Reel"), FVector(0.f, 0.f, -5.5f), FVector(0.f, 0.f, 5.5f), 1.3f, 8, true, false);
			K.Rod(TEXT("Drum"), FVector(0.f, 0.f, 5.5f), FVector(0.f, 0.f, 6.5f), 1.7f, 8, true, false);
			K.Rod(TEXT("Drum"), FVector(0.f, 0.f, -6.5f), FVector(0.f, 0.f, -5.5f), 1.7f, 8, true, false);
			B = Keep(K, TEXT("SM_KiteHandle"));
		}
		return B;
	}

	const FBuilt& SailMesh(int32 Shape)
	{
		return Shape == ShapeBox ? BoxMesh() : Shape == ShapeDelta ? DeltaMesh() : Shape == ShapeStunt ? StuntMesh() : Shape == ShapeDragon ? DragonMesh() : DiamondMesh();
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
				: S == TEXT("Line") ? LineTint : S == TEXT("Reel") ? ReelTint : (S == TEXT("Drum") || S == TEXT("Eye")) ? DrumTint : SparTint;
			if (UMaterialInstanceDynamic* M = Paint(Tint))
			{
				C->SetMaterial(I, M);
			}
		}
	}

	/** A hand's bones by name. */
	struct FHandBones { FName Hand, Middle, Thumb, Pinky, ThumbTip; };
	const FHandBones& RightHand()
	{
		static const FHandBones B = { TEXT("hand_r"), TEXT("middle_01_r"), TEXT("thumb_01_r"), TEXT("pinky_01_r"), TEXT("thumb_03_r") };
		return B;
	}
	const FHandBones& LeftHand()
	{
		static const FHandBones B = { TEXT("hand_l"), TEXT("middle_01_l"), TEXT("thumb_01_l"), TEXT("pinky_01_l"), TEXT("thumb_03_l") };
		return B;
	}

	/** A palm's frame from the hand's bones (X the fingers, Z out of the palm), as the feed bag's (BirdTame.cpp). */
	bool HandPalm(const USkeletalMeshComponent* Body, const FHandBones& Bones, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(Bones.Hand) == INDEX_NONE || Body->GetBoneIndex(Bones.Middle) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(Bones.Hand), Knuckle = Body->GetBoneLocation(Bones.Middle);
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		const FVector Across = Body->GetBoneLocation(Bones.Thumb) - Body->GetBoneLocation(Bones.Pinky);
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		const FVector Palm = Wrist + (Knuckle - Wrist) * 0.6f;
		if (Body->GetBoneIndex(Bones.ThumbTip) != INDEX_NONE && FVector::DotProduct(Body->GetBoneLocation(Bones.ThumbTip) - Palm, Pn) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Palm);
		return true;
	}

	/** A hand of somebody drawn (a townsperson's body), cm; Fallback when the bone isn't there. */
	FVector HandOf(const AActor* Body, const FHandBones& Bones, const FVector& Fallback)
	{
		const USkeletalMeshComponent* Mesh = Body ? Body->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		return Mesh && Mesh->GetBoneIndex(Bones.Hand) != INDEX_NONE ? Mesh->GetBoneLocation(Bones.Hand) : Fallback;
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
	Shape = FMath::Clamp(InShape, 0, LyudKite::NumShapes - 1);
	const FColours& Col = Colourways[((Colours % NumColourways) + NumColourways) % NumColourways];
	const FBuilt& Body = SailMesh(Shape);
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

	// one line, or a stunt kite's two (the second line's pieces after the first's)
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
	LineXf.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.01f)), LinePieces * (Shape == ShapeStunt ? 2 : 1));
	Line->AddInstances(LineXf, false, true, false);

	// the tail: the diamond's nine bows, the dragon's forty pieces of ribbon
	TailN = Shape == ShapeDiamond ? TailPieces : Shape == ShapeDragon ? DragonTailPieces : 0;
	TailSeg = Shape == ShapeDragon ? 30.f : 16.f;
	TailFoot = Shape == ShapeDragon ? 28.f : 40.f;
	if (TailN > 0)
	{
		const FBuilt& Piece = Shape == ShapeDragon ? RibbonMesh() : BowMesh();
		Tail = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Tail"), RF_Transient);
		Tail->SetupAttachment(Root);
		NoLumen(Tail);
		Tail->SetUsingAbsoluteLocation(true);
		Tail->SetUsingAbsoluteRotation(true);
		Tail->SetUsingAbsoluteScale(true);
		Tail->SetCastShadow(false);
		Tail->SetStaticMesh(Piece.Mesh);
		PaintSlots(Tail, Piece.Slots, Col);
		Tail->RegisterComponent();
		TailXf.Init(FTransform(FQuat::Identity, GetActorLocation(), FVector(0.01f)), TailN);
		Tail->AddInstances(TailXf, false, true, false);
		TailPts.Init(GetActorLocation(), TailN + 1);
	}

	if (bWithReel)
	{
		// in the hand: the reel, or a stunt kite's right handle (and its left one, put in the other hand by the subsystem)
		const FBuilt& Held = Shape == ShapeStunt ? HandleMesh() : ReelMesh();
		Reel = NewObject<UStaticMeshComponent>(this, TEXT("Reel"), RF_Transient);
		Reel->SetupAttachment(Root);
		NoLumen(Reel);
		Reel->SetStaticMesh(Held.Mesh);
		PaintSlots(Reel, Held.Slots, Col);
		Reel->SetCastShadow(true);
		Reel->RegisterComponent();
		if (Shape == ShapeStunt)
		{
			LeftHandle = NewObject<UStaticMeshComponent>(this, TEXT("LeftHandle"), RF_Transient);
			LeftHandle->SetupAttachment(Root);
			NoLumen(LeftHandle);
			LeftHandle->SetStaticMesh(Held.Mesh);
			PaintSlots(LeftHandle, Held.Slots, Col);
			LeftHandle->SetCastShadow(true);
			LeftHandle->RegisterComponent();
		}

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

void ALyudKite::Repaint(int32 Colours)
{
	const FColours& Col = Colourways[((Colours % NumColourways) + NumColourways) % NumColourways];
	if (Sail)
	{
		PaintSlots(Sail, (Shape == 1 ? BoxMesh() : DiamondMesh()).Slots, Col);
	}
	if (Line)
	{
		PaintSlots(Line, PieceMesh().Slots, Col);
	}
	if (Tail)
	{
		PaintSlots(Tail, BowMesh().Slots, Col);
	}
	if (Reel)
	{
		PaintSlots(Reel, ReelMesh().Slots, Col);
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

void ALyudKite::Show(const FVector& KitePos, const FVector& Forward, const FVector& Normal, const TArray<FVector>& Points, const FVector& Wind, float DeltaTime,
	const TArray<FVector>* Points2)
{
	if (!Sail)
	{
		return;
	}
	Sail->SetVisibility(true);
	Sail->SetWorldLocationAndRotation(KitePos, FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat());
	// the line (or the two): its pieces re-placed in one batch
	const int32 Lines = Shape == ShapeStunt ? 2 : 1;
	if (Line && Points.Num() == LinePieces + 1 && LineXf.Num() == LinePieces * Lines)
	{
		auto Place = [this](const TArray<FVector>& P, int32 At)
		{
			for (int32 I = 0; I < LinePieces; ++I)
			{
				const FVector Seg = P[I + 1] - P[I];
				const float L = Seg.Size();
				const FQuat Q = L > 1e-3f ? FRotationMatrix::MakeFromX(Seg / L).ToQuat() : FQuat::Identity;
				// ~0.6 mm of line drawn 1.2 mm wide so it reads against the sky at a distance
				LineXf[At + I] = FTransform(Q, P[I], FVector(FMath::Max(L, 0.01f), 0.06f, 0.06f));
			}
		};
		Place(Points, 0);
		if (Lines == 2)
		{
			if (Points2 && Points2->Num() == LinePieces + 1)
			{
				Place(*Points2, LinePieces);
			}
			else
			{
				// no second line to draw (it hangs in a tree by one): its pieces folded away into the kite
				for (int32 I = 0; I < LinePieces; ++I)
				{
					LineXf[LinePieces + I] = FTransform(FQuat::Identity, KitePos, FVector(0.001f));
				}
			}
		}
		Line->BatchUpdateInstancesTransforms(0, LineXf, true, false, true);
		Line->SetVisibility(true);
	}
	else if (Line)
	{
		Line->SetVisibility(false);     // no line: its line broken off
	}
	// the tail: a chain of pieces trailing downwind from the sail's foot, each following the one before it
	if (Tail && TailN > 0 && TailPts.Num() == TailN + 1 && TailXf.Num() == TailN)
	{
		const bool bRibbon = Shape == ShapeDragon;
		const FVector Foot = KitePos - Forward * TailFoot;
		// a dragon's long ribbon streams flatter than a string of bows hangs
		const FVector Down = (Wind.GetSafeNormal2D() * FMath::Clamp(Wind.Size() / 5.f, 0.f, 1.f) + FVector(0.f, 0.f, bRibbon ? -0.28f : -0.45f)).GetSafeNormal();
		const FVector Across = FVector::CrossProduct(Down, FVector::UpVector).GetSafeNormal();
		const float Ease = 1.f - FMath::Exp(-DeltaTime * (bRibbon ? 6.f : 10.f));
		const double T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		TailPts[0] = Foot;
		for (int32 I = 1; I <= TailN; ++I)
		{
			if (!bTailSet)
			{
				TailPts[I] = TailPts[I - 1] + Down * TailSeg;
			}
			// a wave runs down it (the flutter): quick and short on the bows, long and widening down the ribbon
			const float Along = float(I) / float(TailN);
			const float Wave = bRibbon ? (0.12f + 0.5f * Along) * FMath::Sin(T * 4.2 - I * 0.33) : 0.35f * FMath::Sin(T * 7.0 - I * 0.9);
			const FVector Want = TailPts[I - 1] + (Down + Across * Wave).GetSafeNormal() * TailSeg;
			TailPts[I] = FMath::Lerp(TailPts[I], Want, Ease);
			TailPts[I] = TailPts[I - 1] + (TailPts[I] - TailPts[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Down) * TailSeg;
			const FVector Dir = (TailPts[I] - TailPts[I - 1]).GetSafeNormal(UE_SMALL_NUMBER, Down);
			// the ribbon narrows to its end
			TailXf[I - 1] = FTransform(FRotationMatrix::MakeFromXZ(Dir, Normal).ToQuat(), TailPts[I], bRibbon ? FVector(1.f, 1.f - 0.75f * Along, 1.f) : FVector::OneVector);
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
	if (!Reel || Shape == ShapeStunt)
	{
		return;     // (a stunt kite's handles don't turn)
	}
	// a 7 cm drum: a metre of line is ~4.5 turns
	ReelAngle = FMath::Fmod(ReelAngle + Metres * 1640.f, 360.f);
	Reel->SetRelativeRotation(ReelInPalm().GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(ReelAngle)));
}

// ------------------------------------------------------------------ ULyudKiteFlying

ULyudKiteFlying::ULyudKiteFlying() = default;
ULyudKiteFlying::~ULyudKiteFlying() = default;

ULyudKiteFlying* ULyudKiteFlying::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudKiteFlying>() : nullptr;
}

bool ULyudKiteFlying::IsKite(FName Item)
{
	return ShapeOfItem(Item) >= 0;
}

bool ULyudKiteFlying::HasHand(const UObject* WorldContext)
{
	const ULyudKiteFlying* K = Get(WorldContext);
	return K && K->IsOut();
}

void ULyudKiteFlying::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void ULyudKiteFlying::Deinitialize()
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
	Caught.Reset();
	HeldLine = -1;
	Super::Deinitialize();
}

ALyudKite* ULyudKiteFlying::MakeActor(int32 Shape, int32 Colours, bool bReel)
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
				K->Repaint(Colours);   // the pool matches the shape only: this child's own colours
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

void ULyudKiteFlying::Park(ALyudKite* Kite)
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
		// the hero's reel kites are few (one at a time): gone with the take-out (and the handle in his other hand with it)
		Made.Remove(Kite);
		Kite->Destroy();
		return;
	}
	Pool.Add(Kite);
}

float ULyudKiteFlying::SoundGain() const
{
	float Gain = 0.6f;
	if (const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Gain *= Scape->MasterVolume * Scape->AmbienceVolume;
	}
	return Gain;
}

int32 ULyudKiteFlying::Fly(FFlight& F, float Reel, float DeltaTime, const AActor* Ignore, LyudKite::FTricks* InTricks)
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
		return 0;
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
	// the air where the kite is, the land and the sea taken in: the lee, the speed-up over the crest, rising and sinking
	// air, how rough it is. The hero's kite looks every frame, the others ten times a second.
	F.WindIn -= DeltaTime;
	if (F.WindIn <= 0.f)
	{
		F.WindIn = &F == &Hero ? 0.f : 0.1f;
		if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
		{
			const FLyudLocalWind Air = WindSys->GetLocalWind(Before);
			F.Wind = Air.Velocity;
			F.Turbulence = Air.Turbulence;
		}
	}
	LyudKite::FKiteInput In;
	In.Hand = ToM(F.Hand);
	In.Wind = LyudKite::FVec(F.Wind.X, F.Wind.Y, F.Wind.Z);
	In.GroundZ = F.GroundZ * 0.01;
	In.Reel = Reel;
	In.Steer = F.Steer;
	In.Pull = F.Pull;
	In.Turbulence = F.Turbulence;
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
		// as wide as the kite, but never so wide that a low pass over the sand scrapes it
		const float Wide = F.Shape == ShapeStunt ? 60.f : F.Shape == ShapeDelta ? 50.f : F.Shape == ShapeBox ? 30.f : F.Shape == ShapeDragon ? 28.f : 25.f;
		const float Over = float(After.Z) - F.GroundZ;
		const float Radius = FMath::Clamp(Over * 0.4f, 8.f, Wide);
		bool bSnag = false;
		if (World->SweepSingleByObjectType(Hit, Before, After, FQuat::Identity, Obj, FCollisionShape::MakeSphere(Radius), Q))
		{
			// the land is the model's ground, and so is a floor it comes down on (a pier's planks, a paved square)
			const bool bFloor = Over < 150.f && Hit.ImpactNormal.Z > 0.7f;
			bSnag = !Cast<ALandscapeProxy>(Hit.GetActor()) && !bFloor;
		}
		bSnag = bSnag || (Over > 250.f && Crowns().InCrown(World, After));
		if (bSnag)
		{
			LyudKite::FKiteInput Hung = In;
			Hung.bHitObstacle = true;
			LyudKite::Step(K, Hung, 0.0);
		}
	}
	const int32 Got = InTricks ? (int32)LyudKite::Track(*InTricks, K, In, DeltaTime) : 0;
	if (!Actor)
	{
		return Got;
	}
	LyudKite::FVec Fwd, Nrm;
	LyudKite::Frame(K, In.Hand, In.Wind, Fwd, Nrm);
	const FVector Forward = ToDir(Fwd), Normal = ToDir(Nrm);
	static TArray<FVector> Points, Points2;   // reused: no allocation per frame
	if (LyudKite::IsLost(K))
	{
		// caught or snapped: it stays where it is this frame, no line to the hand any more
		Points.Reset();
		Actor->Show(After, Forward, Normal, Points, F.Wind, DeltaTime);
		return Got;
	}
	LyudKite::FVec Pts[ALyudKite::LinePieces + 1];
	Points.SetNum(ALyudKite::LinePieces + 1, EAllowShrinking::No);
	const bool bTwo = F.Shape == ShapeStunt;
	// a stunt kite's lines run from the two hands to either side of its middle (its right wing is the flyer's right)
	const FVector Side = FVector::CrossProduct(Forward, Normal) * ALyudKite::StuntBridle;
	if (bTwo)
	{
		LyudKite::LineBetween(K, In.Hand, ToM(After + Side), ALyudKite::LinePieces, Pts);
	}
	else
	{
		LyudKite::LinePoints(K, In.Hand, ALyudKite::LinePieces, Pts);
	}
	for (int32 I = 0; I <= ALyudKite::LinePieces; ++I)
	{
		FVector P = ToCm(Pts[I]);
		// a slack line lies on the ground, not through it
		P.Z = FMath::Max(P.Z, double(F.GroundZ) + 2.0);
		Points[I] = P;
	}
	Points[0] = F.Hand;
	if (!bTwo)
	{
		Actor->Show(After, Forward, Normal, Points, F.Wind, DeltaTime);
		return Got;
	}
	const FVector Left = F.Hand2.IsZero() ? F.Hand : F.Hand2;
	LyudKite::LineBetween(K, ToM(Left), ToM(After - Side), ALyudKite::LinePieces, Pts);
	Points2.SetNum(ALyudKite::LinePieces + 1, EAllowShrinking::No);
	for (int32 I = 0; I <= ALyudKite::LinePieces; ++I)
	{
		FVector P = ToCm(Pts[I]);
		P.Z = FMath::Max(P.Z, double(F.GroundZ) + 2.0);
		Points2[I] = P;
	}
	Points2[0] = Left;
	Actor->Show(After, Forward, Normal, Points, F.Wind, DeltaTime, &Points2);
	return Got;
}

void ULyudKiteFlying::FlyItself(FFlight& F, float DeltaTime, const AActor* Ignore, bool bHoldWound)
{
	LyudKite::FKite& K = *F.Sim;
	const bool bStunt = F.Shape == ShapeStunt;
	float Reel = 0.f;
	F.Steer = 0.f;
	F.Pull = 0.f;
	if (K.State == LyudKite::EState::Wound)
	{
		F.RelaunchIn -= DeltaTime;
		if (F.RelaunchIn <= 0.f && !bHoldWound)
		{
			FVector Wind = FVector::ZeroVector;
			if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
			{
				Wind = WindSys->GetLocalWind(F.Hand + FVector(0, 0, 150.f)).Velocity;
			}
			const LyudKite::FVec W(Wind.X, Wind.Y, Wind.Z);
			if (bStunt || F.Shape == ShapeDragon)
			{
				// these go up from the far end of their line (somebody held it up there, or it stood on the sand)
				const FVector Down = Wind.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
				FVector At = F.Hand + Down * (F.Want * 100.f);
				At.Z = F.Hand.Z - 130.f;
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteFarEnd), false);
				if (Ignore) Q.AddIgnoredActor(Ignore);
				if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 1500.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q))
				{
					At = Hit.ImpactPoint;
				}
				LyudKite::LongLaunch(K, ToM(F.Hand), ToM(At + FVector(0, 0, 60.f)), W);
			}
			else
			{
				LyudKite::Launch(K, ToM(F.Hand), W);
			}
			F.RelaunchIn = 4.f;
			F.Lain = 0.f;
			F.GroundIn = 0.f;
			F.WindIn = 0.f;
		}
	}
	else if (bHoldWound)
	{
		K.Line = 0.0;    // their hands are busy with somebody else's kite: their own is wound in
	}
	else if (bStunt)
	{
		const LyudKite::FVec W(F.Wind.X, F.Wind.Y, F.Wind.Z);
		F.Steer = F.Demo == 1 ? 1.f : F.Demo == 2 ? -1.f : (float)LyudKite::AutoSteer(K, ToM(F.Hand), W, F.GroundZ * 0.01);
		// the pull that sends it up, held through its first second
		F.Pull = (K.State == LyudKite::EState::Held || (K.State == LyudKite::EState::Flying && K.Clock < 1.2)) ? 1.f : 0.f;
		F.Lain = K.State == LyudKite::EState::Grounded ? F.Lain + DeltaTime : 0.f;
		if (F.Lain > 4.f)
		{
			// it won't come up from where it lies: walked over to, wound up, set out again
			K.Line = 0.0;
			F.Lain = 0.f;
			F.RelaunchIn = 3.f;
		}
	}
	else
	{
		Reel = (float)LyudKite::AutoReel(K, ToM(F.Hand), F.Want);
	}
	Fly(F, Reel, DeltaTime, Ignore);
}

void ULyudKiteFlying::Tick(float DeltaTime)
{
	if (CVarKites.GetValueOnGameThread() == 0 || DeltaTime <= 0.f)
	{
		return;
	}
	DeltaTime = FMath::Min(DeltaTime, 0.1f);
	LookUpActivities();
	if (SayLeft > 0.f)
	{
		SayLeft -= DeltaTime;
		if (SayLeft <= 0.f)
		{
			// what was said has been read: the keys and the air again, or nothing
			LastPrompt.Reset();
			PromptIn = 0.f;
			if (!HeroKite.IsValid() && !bRescuePrompt)
			{
				ShowPrompt(FText::GetEmpty());
			}
		}
	}
	TickRescue(DeltaTime);
	TickHero(DeltaTime);
	TickTown(DeltaTime);
	TickTests(DeltaTime);
}

void ULyudKiteFlying::Say(const TCHAR* Key, float Seconds)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, LyudText::Get(Key));
	}
	LastPrompt.Reset();
	SayLeft = Seconds;
}

void ULyudKiteFlying::ShowPrompt(const FText& Text)
{
	const FString& S = Text.ToString();
	if (!S.IsEmpty() && S == LastPrompt)
	{
		return;     // it is on the screen already
	}
	LastPrompt = S;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, Text);
	}
}

void ULyudKiteFlying::Stamp(const TCHAR* Piece)
{
	if (ULyudCityThings* City = ULyudCityThings::Get(this))
	{
		City->GiveStamp(FString(TEXT("kites/")) + Piece);
	}
}

void ULyudKiteFlying::Event(const TCHAR* What)
{
	ULyudQuests::Notify(this, TEXT("kite"), What);
}

void ULyudKiteFlying::LookUpActivities()
{
	if (KiteActs[0] != -2)
	{
		return;
	}
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People)
	{
		return;
	}
	KiteActs[0] = People->FindActivity(TEXT("fly_kite"));
	KiteActs[1] = People->FindActivity(TEXT("fly_kite_delta"));
	KiteActs[2] = People->FindActivity(TEXT("fly_kite_stunt"));
	KiteActs[3] = People->FindActivity(TEXT("fly_kite_dragon"));
	WatchAct = People->FindActivity(TEXT("watch_kites"));
}

bool ULyudKiteFlying::Bark(int32 Person, const TCHAR* InEvent)
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	AActor* Body = People && Person >= 0 ? People->GetShownBody(Person) : nullptr;
	return Talk && Body && Talk->SayEvent(Body, FName(InEvent), Person);
}

int32 ULyudKiteFlying::FindHelper(const FVector& Near, float WithinCm) const
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (!People)
	{
		return -1;
	}
	int32 Best = -1;
	double BestD = FMath::Square(double(WithinCm));
	const int32 N = People->NumPeople();
	for (int32 Id = 0; Id < N; ++Id)
	{
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose;
		FVector Loc;
		if (!People->GetActivityState(Id, Act, bDoing, Pose, Loc) || !bDoing || Act < 0)
		{
			continue;
		}
		if (Act != WatchAct && Act != KiteActs[0] && Act != KiteActs[1] && Act != KiteActs[2] && Act != KiteActs[3])
		{
			continue;
		}
		const double D = FVector::DistSquared(Loc, Near);
		if (D < BestD && People->GetShownBody(Id))
		{
			BestD = D;
			Best = Id;
		}
	}
	return Best;
}

void ULyudKiteFlying::TakeOut(FName Item)
{
	const int32 Shape = FMath::Max(0, ShapeOfItem(Item));
	// each kind in its own colours
	static const int32 HeroColours[LyudKite::NumShapes] = { 0, 1, 2, 4, 3 };
	ALyudKite* K = MakeActor(Shape, HeroColours[Shape], true);
	if (!K)
	{
		return;
	}
	HeroKite = K;
	HeroItem = Item;
	Hero = FFlight();
	Hero.Sim = MakeShared<LyudKite::FKite>();
	Hero.Sim->Def = LyudKite::ByShape((LyudKite::EShape)Shape);
	Hero.Sim->Seed = 7;
	Hero.Actor = K;
	Hero.Shape = Shape;
	Tricks = MakeShared<LyudKite::FTricks>();
	K->ShowWound();
	K->SetActorHiddenInGame(true);
	bAttached = false;
	bWaitUp = true;
	bWindingAway = false;
	bLaid = false;
	bLaunchPull = false;
	Helper = -1;
	bHelperLaunch = false;
	NearHelper = -1;
	HelperScanIn = 0.f;
	HoldTime = 0.f;
	UpFor = 0.f;
	CalmFor = 0.f;
	WheelReel = 0.f;
	PrevState = (int32)LyudKite::EState::Wound;
	bWasSpinning = false;
	bStampHigh = false;
	bStampBox = false;
	bStampCalm = false;
	bStampDragon = false;
	// the keys and the air at the next look
	SayLeft = 0.f;
	PromptIn = 0.f;
	LastPrompt.Reset();
}

void ULyudKiteFlying::PutAway()
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
	Tricks.Reset();
	bAttached = false;
	bLaid = false;
	Helper = -1;
	bHelperLaunch = false;
	NearHelper = -1;
	LastPrompt.Reset();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
}

void ULyudKiteFlying::Lose(const TCHAR* Why)
{
	ULyudInventory* Inv = ULyudInventory::Get(GetWorld());
	const FName Item = HeroItem;
	// the kite stays where it was caught (a tree, a lamp post), its line hanging to the ground: it can be got down
	if (Hero.Sim && Hero.Sim->State == LyudKite::EState::Snagged && HeroKite.IsValid())
	{
		ALyudKite* K = HeroKite.Get();
		K->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		K->SetHum(0.f, 0.f, 0.f);
		// the reel (the handles) went with the hero: not left hanging where his hands were
		if (UStaticMeshComponent* R = K->GetReel())
		{
			R->SetVisibility(false);
		}
		if (UStaticMeshComponent* L = K->GetLeftHandle())
		{
			L->SetVisibility(false);
		}
		AddCaught(Hero.Sim, K, -1, Item, Hero.Hand, Hero.Wind);
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

void ULyudKiteFlying::TickHero(float DeltaTime)
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
	const bool bStunt = Hero.Shape == ShapeStunt;
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

	// the hand: the reel (a stunt kite's right handle) held up in front; it rides on the hand bone once the hand is there
	if (Reach && !Reach->HasOverrideHold())
	{
		// made once: the pose holds an array, and this runs every frame the kite is out
		static const FHeroHoldPose ReelHold = ReelPose();
		Reach->SetHold(ReelHold);
	}
	HoldTime += DeltaTime;
	if (!bAttached && Reach && Reach->HoldWeight() > 0.6f)
	{
		FTransform Palm;
		if (HandPalm(H->GetMesh(), RightHand(), Reach->HoldNormal(), Reach->HoldFinger(), Palm))
		{
			K->SetActorTransform(Palm);
			K->AttachToComponent(H->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
			if (UStaticMeshComponent* R = K->GetReel())
			{
				R->SetRelativeTransform(ReelInPalm());
			}
			// a stunt kite's other handle in the left fist, wherever that arm is
			if (UStaticMeshComponent* L = K->GetLeftHandle())
			{
				FTransform LeftPalm;
				if (HandPalm(H->GetMesh(), LeftHand(), Reach->HoldNormal(), Reach->HoldFinger(), LeftPalm))
				{
					L->SetWorldTransform(ReelInPalm() * LeftPalm);
					L->AttachToComponent(H->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_l"));
				}
				else
				{
					L->SetVisibility(false);
				}
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
	const UStaticMeshComponent* LeftHandle = K->GetLeftHandle();
	Hero.Hand2 = bAttached && LeftHandle && LeftHandle->IsVisible() ? LeftHandle->GetComponentLocation() : NewHand;
	const FVector HeroAt = H->GetActorLocation();

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
	if (bInput && !bStunt && PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) WheelReel += 1.f;
	if (bInput && !bStunt && PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) WheelReel -= 1.f;
	if (bInput && PC->WasInputKeyJustPressed(EKeys::Q))
	{
		if (Sim.State == LyudKite::EState::Wound)
		{
			Inv->Equip(FName());
			PutAway();
			return;
		}
		bWindingAway = true;    // wound all the way in first, then away
		if (Sim.State == LyudKite::EState::Held)
		{
			Sim.Line = 0.0;     // taken back from the hands that hold it
		}
		Say(TEXT("ui.kite.winding"), 3.f);
	}
	float Reel = 0.f;
	Hero.Steer = 0.f;
	Hero.Pull = 0.f;
	if (Sim.State == LyudKite::EState::Wound)
	{
		if (bWindingAway)
		{
			bWindingAway = false;
			Inv->Equip(FName());
			PutAway();
			return;
		}
		bLaid = false;
		bLaunchPull = false;
		Helper = -1;
		// somebody near who would hold it up (looked for twice a second)
		HelperScanIn -= DeltaTime;
		if (HelperScanIn <= 0.f)
		{
			HelperScanIn = 0.5f;
			NearHelper = FindHelper(HeroAt);
		}
		// how it goes up: 1 tossed from the hand, 2 stood on the sand, 3 held up far downwind, 4 given to somebody to hold
		int32 Mode = LaunchQueued;
		LaunchQueued = 0;
		if (bInput && !bWaitUp)
		{
			if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
			{
				Mode = 1;
			}
			else if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
			{
				Mode = 2;
			}
			else if (NearHelper >= 0 && !bRescuePrompt && ANpcTalkDirector::UsePressed(PC))
			{
				Mode = 4;
			}
		}
		if (Mode == 1 && bStunt)
		{
			Mode = 2;   // a two-line kite isn't tossed: it goes up off the sand
		}
		if (Mode > 0)
		{
			FVector Wind = FVector::ZeroVector;
			if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
			{
				Wind = WindSys->GetLocalWind(Hero.Hand).Velocity;
			}
			const LyudKite::FVec W(Wind.X, Wind.Y, Wind.Z);
			const FVector Down = Wind.GetSafeNormal2D(UE_SMALL_NUMBER, H->GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector));
			// the ground this far downwind of his feet
			auto GroundAt = [World, H, &HeroAt, &Down](float Cm)
			{
				FVector At = HeroAt + Down * Cm;
				At.Z = HeroAt.Z - 90.f;
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteLay), false);
				Q.AddIgnoredActor(H);
				if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 400.f), At - FVector(0, 0, 3000.f), ECC_Visibility, Q))
				{
					At = Hit.ImpactPoint;
				}
				return At;
			};
			UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
			const AActor* Body = Mode == 4 && People ? People->GetShownBody(NearHelper) : nullptr;
			if (Mode == 4 && !Body)
			{
				Mode = 0;   // they have just gone
			}
			if (Mode == 1)
			{
				LyudKite::Launch(Sim, ToM(Hero.Hand), W);
				Say(Wind.Size() < 2.5f ? TEXT("ui.kite.toss_calm") : TEXT("ui.kite.toss"), 3.f);
			}
			else if (Mode == 2)
			{
				LyudKite::LayDown(Sim, ToM(Hero.Hand), ToM(GroundAt(150.f) + FVector(0, 0, 5.f)), W);
				bLaid = true;
				Say(TEXT("ui.kite.laid"), 4.f);
			}
			else if (Mode == 3)
			{
				LyudKite::LongLaunch(Sim, ToM(Hero.Hand), ToM(GroundAt(2000.f) + FVector(0, 0, 200.f)), W);
				Say(TEXT("ui.kite.held"), 4.f);
			}
			else if (Mode == 4)
			{
				Helper = NearHelper;
				HelperAt = Body->GetActorLocation();
				LyudKite::LongLaunch(Sim, ToM(Hero.Hand), ToM(HelperAt + FVector(0, 0, 200.f)), W);
				Bark(Helper, TEXT("kite_hold_yes"));
				Say(TEXT("ui.kite.helper_holds"), 4.f);
			}
			if (Mode > 0)
			{
				Hero.GroundIn = 0.f;
				UpFor = 0.f;
				bHelperLaunch = false;
				bWaitUp = true;     // the press that sent it off doesn't pull it straight back in
			}
		}
	}
	else if (bWindingAway)
	{
		Reel = -2.f * LyudKite::ReelIn;
	}
	else
	{
		// it waits for the pull: held up by somebody, or stood on the sand with the line following the hero as he backs off
		const bool bWaiting = Sim.State == LyudKite::EState::Held || (bLaid && Sim.State == LyudKite::EState::Grounded);
		if (bStunt)
		{
			if (bSteerForced)
			{
				Hero.Steer = ForcedSteer;
				Hero.Pull = bPullForced ? 1.f : 0.f;
			}
			else if (bWaiting || bLaunchPull)
			{
				// either button is the pull that sends it up, and stays a pull until it is let go: the hand that launched
				// it doesn't steer it into the sand at once
				bLaunchPull = bL || bR;
				Hero.Pull = bLaunchPull ? 1.f : 0.f;
			}
			else if (bL && bR)
			{
				Hero.Pull = 1.f;
			}
			else
			{
				Hero.Steer = bR ? 1.f : bL ? -1.f : 0.f;
			}
		}
		else
		{
			if (bL) Reel -= LyudKite::ReelIn;
			if (bR && !bWaiting) Reel += LyudKite::ReelOut;
			if (FMath::Abs(WheelReel) > 0.01f)
			{
				// a click of the wheel: a metre out or in, run at 4 m/s
				const float Step = FMath::Clamp(WheelReel, -4.f * DeltaTime, 4.f * DeltaTime);
				WheelReel -= Step;
				Reel += Step / DeltaTime;
			}
		}
		const bool bPulling = Reel < 0.f || Hero.Pull > 0.f;
		if (bWaiting)
		{
			bool bGone = false;
			if (Helper >= 0 && Sim.State == LyudKite::EState::Held)
			{
				UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
				AActor* Body = People ? People->GetShownBody(Helper) : nullptr;
				if (!Body || FVector::DistSquared2D(Body->GetActorLocation(), HelperAt) > FMath::Square(120.f))
				{
					// they went on their way: the kite is back on its reel
					bGone = true;
					Helper = -1;
					Sim.Line = 0.0;
					Say(TEXT("ui.kite.helper_gone"), 3.5f);
				}
				else
				{
					Sim.HeldAt = ToM(Body->GetActorLocation() + FVector(0, 0, 200.f));
					if (ANpcBody* Npc = Cast<ANpcBody>(Body))
					{
						Npc->LookAtPoint(H->EyeLocation(), 1.f);
					}
				}
			}
			if (bPulling)
			{
				bLaid = false;
			}
			else if (!bGone)
			{
				// the line runs off as he backs away, a little slack in it: only his pull sends the kite up
				const double Far = FVector::Dist(Hero.Hand, ToCm(Sim.Pos)) * 0.01 + 0.3;
				Sim.Line = FMath::Max(Sim.Line, FMath::Min(Far, Sim.Def.LineMax));
			}
		}
		else
		{
			bLaid = false;
		}
	}
	const double LineBefore = Sim.Line;
	const int32 Got = Fly(Hero, Reel, DeltaTime, H, Tricks.Get());
	K->TurnReel(float(Sim.Line - LineBefore));
	ClickIn -= DeltaTime;
	if (Sim.Line < LineBefore - 0.01 && ClickIn <= 0.f && !bStunt)
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
	const bool bWasUp = Was == LyudKite::EState::Flying || Was == LyudKite::EState::Spinning;
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
	if (Now == LyudKite::EState::Grounded && Hero.bWater && bWasUp)
	{
		Lose(TEXT("ui.kite.sea"));
		return;
	}
	if (Was == LyudKite::EState::Held && Now != LyudKite::EState::Held)
	{
		if (Helper >= 0 && Now == LyudKite::EState::Flying)
		{
			// let go on the pull
			bHelperLaunch = true;
			Bark(Helper, TEXT("kite_hold_go"));
		}
		Helper = -1;
	}
	if (Now == LyudKite::EState::Grounded && bWasUp)
	{
		if (bStunt)
		{
			Say(Sim.bSpar ? TEXT("ui.kite.spar") : !Sim.bNoseUp ? TEXT("ui.kite.crashed") : TEXT("ui.kite.stands"), 4.f);
		}
		else
		{
			Say(Sim.Air < 3.0 ? TEXT("ui.kite.fell_calm") : TEXT("ui.kite.fell"), 3.5f);
		}
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
	if (Got > 0 && Got < (int32)LyudKite::ETrick::Count)
	{
		// a trick named: said, stamped, told to the quests
		const LyudKite::ETrick Trick = (LyudKite::ETrick)Got;
		const FString Key = FString::Printf(TEXT("ui.kite.trick.%s"), UTF8_TO_TCHAR(LyudKite::TrickName(Trick)));
		Say(*Key, 2.5f);
		// somebody out with the kites within 15 m has a word for a loop or a low pass
		const int32 Fan = FindHelper(HeroAt, 1500.f);
		if (Trick == LyudKite::ETrick::LoopLeft || Trick == LyudKite::ETrick::LoopRight)
		{
			Stamp(TEXT("loop"));
			Event(TEXT("loop"));
			Bark(Fan, TEXT("kite_loop_cheer"));
		}
		else if (Trick == LyudKite::ETrick::Eight)
		{
			Stamp(TEXT("eight"));
			Event(TEXT("eight"));
		}
		else if (Trick == LyudKite::ETrick::LowPass)
		{
			Stamp(TEXT("low_pass"));
			Event(TEXT("low_pass"));
			Bark(Fan, TEXT("kite_low_pass"));
		}
		else if (Trick == LyudKite::ETrick::Landing)
		{
			Event(TEXT("landed"));
		}
	}
	if (LyudKite::IsUp(Sim))
	{
		UpFor += DeltaTime;
		if (UpFor > 20.f && UpFor - DeltaTime <= 20.f)
		{
			Stamp(TEXT("first_flight"));
			// a flight of twenty seconds: quests, the diary's day (docs/diary.md)
			ULyudQuests::Notify(this, TEXT("kite_flown"));
			Event(TEXT("flown"));
			Say(TEXT("ui.kite.flying"), 3.f);
		}
		if (bHelperLaunch && UpFor > 5.f)
		{
			bHelperLaunch = false;
			Stamp(TEXT("helper_launch"));
			Event(TEXT("helper_launch"));
		}
		// asked once a kite taken out, not every frame it stays up there (the stamp's name is a string built for the ask)
		if (!bStampHigh && Now == LyudKite::EState::Flying && Sim.Pos.Z - Hero.Hand.Z * 0.01 > 30.0)
		{
			bStampHigh = true;
			Stamp(TEXT("high"));
			Event(TEXT("high"));
		}
		if (!bStampBox && Hero.Shape == ShapeBox && UpFor > 30.f)
		{
			bStampBox = true;
			Stamp(TEXT("box"));
		}
		if (!bStampDragon && Hero.Shape == ShapeDragon && UpFor > 60.f)
		{
			bStampDragon = true;
			Stamp(TEXT("dragon"));
		}
		// a delta kept up a minute in air the other kites would not fly in
		if (!bStampCalm && Hero.Shape == ShapeDelta)
		{
			CalmFor = Hero.Wind.Size2D() < 3.f ? CalmFor + DeltaTime : 0.f;
			if (CalmFor > 60.f)
			{
				bStampCalm = true;
				Stamp(TEXT("delta_calm"));
			}
		}
	}
	else
	{
		UpFor = 0.f;
		CalmFor = 0.f;
		if (Now != LyudKite::EState::Held)
		{
			bHelperLaunch = false;
		}
	}
	// the prompt: what was said stays its seconds (Tick counts them), then what the keys do and what the air is like
	if (SayLeft <= 0.f && !bBusy && !bRescuePrompt)
	{
		PromptIn -= DeltaTime;
		if (PromptIn <= 0.f)
		{
			PromptIn = 0.5f;
			RefreshPrompt(H);
		}
	}
	if (CVarKiteDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE + 7711, 1.f, FColor::Cyan, DebugState());
	}
}

void ULyudKiteFlying::RefreshPrompt(const AHeroCharacter* H)
{
	if (!Hero.Sim || !H)
	{
		return;
	}
	const LyudKite::FKite& Sim = *Hero.Sim;
	const bool bStunt = Hero.Shape == ShapeStunt;
	const TCHAR* Keys = bStunt ? TEXT("ui.kite.keys_up_stunt") : TEXT("ui.kite.keys_up");
	if (Sim.State == LyudKite::EState::Wound)
	{
		Keys = bStunt ? TEXT("ui.kite.keys_wound_stunt") : TEXT("ui.kite.keys_wound");
	}
	else if (Sim.State == LyudKite::EState::Held)
	{
		Keys = bStunt ? TEXT("ui.kite.keys_held_stunt") : TEXT("ui.kite.keys_held");
	}
	else if (Sim.State == LyudKite::EState::Grounded)
	{
		if (bStunt)
		{
			Keys = Sim.bSpar ? TEXT("ui.kite.keys_spar") : !Sim.bNoseUp ? TEXT("ui.kite.keys_crashed") : TEXT("ui.kite.keys_laid_stunt");
		}
		else
		{
			Keys = bLaid ? TEXT("ui.kite.keys_laid") : TEXT("ui.kite.keys_down");
		}
	}
	FString Text = LyudText::Get(Keys).ToString();
	if (Sim.State == LyudKite::EState::Wound && NearHelper >= 0)
	{
		if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
		{
			Text += TEXT("\n");
			Text += LyudText::Get(TEXT("ui.kite.ask_helper"), { FStringFormatArg(People->GetDisplayName(NearHelper).ToString()) }).ToString();
		}
	}
	const FText Air = WindLine(H);
	if (!Air.IsEmpty())
	{
		Text += TEXT("\n");
		Text += Air.ToString();
	}
	ShowPrompt(FText::FromString(Text));
}

FText ULyudKiteFlying::WindLine(const AHeroCharacter* H) const
{
	const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this);
	if (!WindSys || !Hero.Sim || !H)
	{
		return FText::GetEmpty();
	}
	const LyudKite::FKite& Sim = *Hero.Sim;
	// twisted lines first: that is in the hands, not in the air
	if (Hero.Shape == ShapeStunt && LyudKite::IsUp(Sim) && FMath::Abs(Sim.Twist) > 3.0)
	{
		return LyudText::Get(TEXT("ui.kite.twisted"), { FStringFormatArg(FMath::RoundToInt(float(FMath::Abs(Sim.Twist)))) });
	}
	// the air a kite meets over where he stands (ten metres up: the Beaufort scale's height)
	const FVector Feet = H->GetActorLocation() - FVector(0, 0, 90.f);
	const FLyudLocalWind Air = WindSys->GetLocalWindAbove(Feet, 10.f);
	float InSeconds = 0.f, Factor = 1.f;
	bool bLull = false;
	if (WindSys->GetNextGust(Feet, 8.f, InSeconds, Factor, bLull))
	{
		return LyudText::Get(bLull ? TEXT("ui.kite.wind_lull") : TEXT("ui.kite.wind_gust"), { FStringFormatArg(FMath::Max(1, FMath::CeilToInt(InSeconds))) });
	}
	if (Air.Shelter > 0.5f)
	{
		return LyudText::Get(TEXT("ui.kite.wind_lee"));
	}
	if (Air.Turbulence > 1.5f)
	{
		return LyudText::Get(TEXT("ui.kite.wind_rough"));
	}
	if (Air.Updraft > 0.6f)
	{
		return LyudText::Get(TEXT("ui.kite.wind_lift"));
	}
	const FString Force = LyudText::Get(ULyudWindSubsystem::BeaufortKey(ULyudWindSubsystem::BeaufortOf(Air.Speed))).ToString();
	if (Air.Speed < 0.8f * float(Sim.Def.FlyFrom))
	{
		return LyudText::Get(TEXT("ui.kite.wind_light"), { FStringFormatArg(Force) });
	}
	if (Air.Speed > float(Sim.Def.SpinSpeed))
	{
		return LyudText::Get(TEXT("ui.kite.wind_strong"), { FStringFormatArg(Force) });
	}
	// where it comes from as he looks: 0 into his face, 2 from his right, 4 at his back, 6 from his left
	const float Yaw = FMath::DegreesToRadians(H->GetControlRotation().Yaw);
	const FVector2D Ahead(FMath::Cos(Yaw), FMath::Sin(Yaw)), Right(-FMath::Sin(Yaw), FMath::Cos(Yaw));
	const FVector2D From(-Air.Velocity.X, -Air.Velocity.Y);
	const float Angle = float(FMath::RadiansToDegrees(FMath::Atan2(FVector2D::DotProduct(From, Right), FVector2D::DotProduct(From, Ahead))));
	const int32 Dir = ((FMath::RoundToInt(Angle / 45.f) % 8) + 8) % 8;
	static const TCHAR* const DirKeys[8] = { TEXT("ui.kite.wind_0"), TEXT("ui.kite.wind_1"), TEXT("ui.kite.wind_2"), TEXT("ui.kite.wind_3"),
		TEXT("ui.kite.wind_4"), TEXT("ui.kite.wind_5"), TEXT("ui.kite.wind_6"), TEXT("ui.kite.wind_7") };
	return LyudText::Get(DirKeys[Dir], { FStringFormatArg(Force) });
}

FString ULyudKiteFlying::DebugState() const
{
	if (!Hero.Sim)
	{
		return FString::Printf(TEXT("KITE none in hand; town %d, tests %d, caught %d (line held %d)"), Town.Num(), Tests.Num(), Caught.Num(), HeldLine);
	}
	const LyudKite::FKite& K = *Hero.Sim;
	const LyudKite::FVec Hand = ToM(Hero.Hand);
	const LyudKite::FVec W(Hero.Wind.X, Hero.Wind.Y, Hero.Wind.Z);
	double Az = 0.0, El = 0.0;
	LyudKite::Window(K, Hand, W, Az, El);
	return FString::Printf(TEXT("KITE %s %s line %.1f m height %.1f m elev %.0f az %.0f air %.1f m/s (wind %.1f up %.1f rough %.1f) lift %.1f N tension %.1f N hum %.2f steer %.1f pull %.0f twist %.1f tricks %d; town %d, tests %d, caught %d"),
		*HeroItem.ToString(), UTF8_TO_TCHAR(LyudKite::StateName(K.State)), K.Line, K.Pos.Z - Hero.Hand.Z * 0.01, El, Az, K.Air,
		Hero.Wind.Size2D(), Hero.Wind.Z, Hero.Turbulence, K.Lift, K.Tension, LyudKite::Hum(K), Hero.Steer, Hero.Pull, K.Twist,
		Tricks ? Tricks->Count : 0, Town.Num(), Tests.Num(), Caught.Num());
}

FString ULyudKiteFlying::TricksReport() const
{
	if (!Tricks)
	{
		return TEXT("KITE tricks: no kite in the hand");
	}
	FString Out = FString::Printf(TEXT("KITE tricks %d, score %.0f:"), Tricks->Count, Tricks->Score);
	for (int32 I = 1; I < (int32)LyudKite::ETrick::Count; ++I)
	{
		Out += FString::Printf(TEXT(" %s %d"), UTF8_TO_TCHAR(LyudKite::TrickName((LyudKite::ETrick)I)), Tricks->By[I]);
	}
	if (Tricks->Last != LyudKite::ETrick::None)
	{
		Out += FString::Printf(TEXT("; last %s (%.0f)"), UTF8_TO_TCHAR(LyudKite::TrickName(Tricks->Last)), Tricks->LastScore);
	}
	return Out;
}

void ULyudKiteFlying::TickTown(float DeltaTime)
{
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!People || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	if (KiteActs[0] < 0 && KiteActs[1] < 0 && KiteActs[2] < 0 && KiteActs[3] < 0)
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
			if (!People->GetActivityState(Id, Act, bDoing, Pose, Loc) || Act < 0 || !bDoing)
			{
				continue;
			}
			// the kind by what they came out to do: a child's diamond (a box kite for one in three), a delta, a stunt
			// kite, a dragon
			int32 Shape = -1;
			if (Act == KiteActs[0]) Shape = (Id % 3) == 0 ? ShapeBox : ShapeDiamond;
			else if (Act == KiteActs[1]) Shape = ShapeDelta;
			else if (Act == KiteActs[2]) Shape = ShapeStunt;
			else if (Act == KiteActs[3]) Shape = ShapeDragon;
			if (Shape < 0 || FVector::DistSquared(Loc, Cam) > FMath::Square(TownDrawCm) || !People->GetShownBody(Id))
			{
				continue;
			}
			FFlight* Have = Town.FindByPredicate([Id](const FFlight& F) { return F.Person == Id; });
			if (Have)
			{
				// (another kind now: that one is put away at the end of this look, the new one comes out at the next)
				Have->bSeen = Have->Shape == Shape;
				continue;
			}
			if (Town.Num() >= Max)
			{
				continue;
			}
			// a kite of their own, the colours by who they are
			ALyudKite* K = MakeActor(Shape, Id, false);
			if (!K)
			{
				continue;
			}
			FFlight F;
			F.Sim = MakeShared<LyudKite::FKite>();
			F.Sim->Def = LyudKite::ByShape((LyudKite::EShape)Shape);
			F.Sim->Seed = uint32(Id) * 2654435761u;
			F.Actor = K;
			F.Person = Id;
			F.Shape = Shape;
			// the line they let out: a child's sixteen to thirty-four metres, a delta high on a long one, a stunt kite and
			// a dragon on theirs
			F.Want = Shape == ShapeDelta ? 28.f + float(Id % 17) : Shape == ShapeStunt ? 20.f + float(Id % 7)
				: Shape == ShapeDragon ? 20.f + float(Id % 9) : 16.f + float(Id % 19);
			F.Hand = Loc + FVector(0, 0, 120.f);
			F.Hand2 = F.Hand;
			F.RelaunchIn = 0.5f + 0.1f * float(Id % 13);
			F.bSeen = true;
			Town.Add(MoveTemp(F));
		}
		for (int32 I = Town.Num() - 1; I >= 0; --I)
		{
			if (Town[I].bSeen)
			{
				continue;
			}
			if (Town[I].bCaught)
			{
				// gone home with their kite still in the tree: nobody's now, and not there for long
				for (FCaught& C : Caught)
				{
					if (C.Sim == Town[I].Sim)
					{
						C.Person = -1;
						C.GoneIn = FMath::Min(C.GoneIn, 180.f);
					}
				}
			}
			else
			{
				Park(Town[I].Actor.Get());
			}
			Town.RemoveAtSwap(I);
		}
	}
	const APawn* HeroPawn = PC->GetPawn();
	for (FFlight& F : Town)
	{
		LyudKite::FKite& K = *F.Sim;
		if (F.bCaught)
		{
			continue;   // in a tree: the caught kites' business now
		}
		if (LyudKite::IsLost(K))
		{
			if (K.State == LyudKite::EState::Snagged && F.Actor.IsValid())
			{
				// caught: it hangs there until somebody gets it down for them
				F.bCaught = true;
				AddCaught(F.Sim, F.Actor.Get(), F.Person, FName(), F.Hand, F.Wind);
			}
			else if (F.Actor.IsValid())
			{
				// snapped: that kite is gone for this outing
				F.Actor->ShowWound();
			}
			continue;
		}
		// the line from their right hand (a stunt kite's other line from the left)
		AActor* Body = People->GetShownBody(F.Person);
		if (Body)
		{
			const FVector Fallback = Body->GetActorLocation() + FVector(0, 0, 120.f);
			F.Hand = HandOf(Body, RightHand(), Fallback);
			F.Hand2 = F.Shape == ShapeStunt ? HandOf(Body, LeftHand(), Fallback) : F.Hand;
		}
		// a stunt flier shows the hero who stands by how a loop is done: one to the right, then back to the left so the
		// lines come out straight
		if (F.Shape == ShapeStunt && !F.bDemoDone && HeroPawn)
		{
			const bool bNear = FVector::DistSquared(HeroPawn->GetActorLocation(), F.Hand) < FMath::Square(600.f);
			F.HeroNear = bNear ? F.HeroNear + DeltaTime : 0.f;
			if (K.State != LyudKite::EState::Flying)
			{
				F.Demo = 0;
			}
			else if (F.Demo == 0)
			{
				const bool bRoom = K.Pos.Z - F.GroundZ * 0.01 > 11.0 && FMath::Abs(K.Twist) < 0.5 && FMath::Abs(LyudKite::Heading(K, ToM(F.Hand))) < 35.0;
				if (F.HeroNear > 4.f && bRoom)
				{
					F.Demo = 1;
					F.DemoFrom = (float)K.Twist;
				}
			}
			else if (F.Demo == 1 && K.Twist > double(F.DemoFrom) + 1.0)
			{
				F.Demo = 2;
			}
			else if (F.Demo == 2 && K.Twist < double(F.DemoFrom) + 0.05)
			{
				F.Demo = 0;
				F.bDemoDone = true;
				if (bNear)
				{
					Stamp(TEXT("lesson"));
					Bark(F.Person, TEXT("kite_lesson"));
					if (!HeroKite.IsValid())
					{
						Say(TEXT("ui.kite.lesson"), 4.f);
					}
				}
			}
		}
		FlyItself(F, DeltaTime, Body, F.Person == Helper);
	}
}

void ULyudKiteFlying::SpawnTestKites(int32 N, int32 Shape)
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
		// one kind, or all five in turn
		const int32 Kind = Shape >= 0 ? FMath::Min(Shape, LyudKite::NumShapes - 1) : I % LyudKite::NumShapes;
		ALyudKite* K = MakeActor(Kind, I, false);
		if (!K)
		{
			continue;
		}
		FFlight F;
		F.Sim = MakeShared<LyudKite::FKite>();
		F.Sim->Def = LyudKite::ByShape((LyudKite::EShape)Kind);
		F.Sim->Seed = uint32(I + 1) * 40503u;
		F.Actor = K;
		F.Shape = Kind;
		// (the hands of somebody facing downwind: the right one to the right of the wind)
		F.Hand = At + FVector(0, 0, 130.f) + Across * 20.f;
		F.Hand2 = At + FVector(0, 0, 130.f) - Across * 20.f;
		F.Want = float(FMath::Min(F.Sim->Def.FlyLine, F.Sim->Def.LineMax)) + 1.5f * float(I % 4);
		F.RelaunchIn = 0.2f * float(I);
		Tests.Add(MoveTemp(F));
	}
}

void ULyudKiteFlying::TickTests(float DeltaTime)
{
	for (int32 I = Tests.Num() - 1; I >= 0; --I)
	{
		FFlight& F = Tests[I];
		LyudKite::FKite& K = *F.Sim;
		if (LyudKite::IsLost(K))
		{
			// caught: it hangs where it is with the other caught kites (and can be got down); snapped: gone
			if (K.State == LyudKite::EState::Snagged && F.Actor.IsValid())
			{
				AddCaught(F.Sim, F.Actor.Get(), -1, FName(), F.Hand, F.Wind);
			}
			else
			{
				Park(F.Actor.Get());
			}
			Tests.RemoveAtSwap(I);
			continue;
		}
		FlyItself(F, DeltaTime, nullptr, false);
	}
}

// ------------------------------------------------------------------ a caught kite got down

void ULyudKiteFlying::AddCaught(const TSharedPtr<LyudKite::FKite>& Sim, ALyudKite* Actor, int32 Person, FName Item, const FVector& Hand, const FVector& Wind)
{
	UWorld* World = GetWorld();
	if (!Sim || !Actor || !World)
	{
		return;
	}
	FCaught C;
	C.Sim = Sim;
	// how fast it is caught: by the kite and by when it flew in
	C.Snag = MakeShared<LyudKite::FSnag>(LyudKite::MakeSnag(Sim->Pos, Sim->Seed ^ uint32(Sim->Clock * 100.0)));
	C.Actor = Actor;
	C.Person = Person;
	C.Item = Item;
	LyudKite::FVec Fwd, Nrm;
	LyudKite::Frame(*Sim, ToM(Hand), LyudKite::FVec(Wind.X, Wind.Y, Wind.Z), Fwd, Nrm);
	C.Forward = ToDir(Fwd);
	C.Normal = ToDir(Nrm);
	C.Wind = Wind;
	// its line hangs to the ground a step downwind of it
	const FVector At = ToCm(Sim->Pos);
	FVector Foot = At + Wind.GetSafeNormal2D() * 120.f;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(KiteCaught), false);
	Q.AddIgnoredActor(Actor);
	if (World->LineTraceSingleByChannel(Hit, Foot, Foot - FVector(0, 0, 20000.f), ECC_Visibility, Q))
	{
		Foot = Hit.ImpactPoint;
	}
	else
	{
		Foot.Z = Hand.Z - 150.f;
	}
	C.Foot = Foot;
	Caught.Add(MoveTemp(C));
}

void ULyudKiteFlying::RemoveCaught(int32 Index, bool bDestroyActor)
{
	if (!Caught.IsValidIndex(Index))
	{
		return;
	}
	if (bDestroyActor)
	{
		// a child's kite: theirs is gone for this outing
		for (FFlight& F : Town)
		{
			if (F.Sim == Caught[Index].Sim)
			{
				F.Actor.Reset();
			}
		}
		Park(Caught[Index].Actor.Get());    // (the hero's own, made with a reel, is destroyed there)
	}
	Caught.RemoveAt(Index);
	if (HeldLine == Index)
	{
		HeldLine = -1;
	}
	else if (HeldLine > Index)
	{
		--HeldLine;
	}
}

void ULyudKiteFlying::FinishRescue(int32 Index)
{
	if (!Caught.IsValidIndex(Index))
	{
		return;
	}
	const TSharedPtr<LyudKite::FKite> Sim = Caught[Index].Sim;
	const int32 Person = Caught[Index].Person;
	const FName Item = Caught[Index].Item;
	FFlight* Theirs = Person >= 0 ? Town.FindByPredicate([&Sim](const FFlight& F) { return F.Sim == Sim; }) : nullptr;
	if (Theirs)
	{
		// the child has it back: wound up, and up again in a little while
		Theirs->Sim->State = LyudKite::EState::Wound;
		Theirs->Sim->Line = 0.0;
		Theirs->bCaught = false;
		Theirs->RelaunchIn = 8.f;
		if (ALyudKite* A = Theirs->Actor.Get())
		{
			A->ShowWound();
		}
		RemoveCaught(Index, false);
		Bark(Person, TEXT("kite_rescue_thanks"));
		Say(TEXT("ui.kite.rescue.gave"), 4.f);
	}
	else
	{
		RemoveCaught(Index, true);
		if (!Item.IsNone())
		{
			// his own: back in the pocket
			if (ULyudInventory* Inv = ULyudInventory::Get(GetWorld()))
			{
				Inv->Add(Item);
			}
			Say(TEXT("ui.kite.rescue.got"), 4.f);
		}
		else
		{
			Say(TEXT("ui.kite.rescue.down"), 4.f);
		}
	}
	Stamp(TEXT("rescue"));
	Event(TEXT("rescue"));
}

void ULyudKiteFlying::TickRescue(float DeltaTime)
{
	const bool bHadPrompt = bRescuePrompt;
	bRescuePrompt = false;
	UWorld* World = GetWorld();
	if (Caught.Num() > 0 && World)
	{
		AHeroCharacter* H = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this);
		UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
		const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
		const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
		const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		// the hero can see to a caught kite when his hands are free of a flying one and no screen is up
		const bool bFree = H && PC && !H->IsHidden() && !PC->IsMoveInputIgnored() && (!Hero.Sim || Hero.Sim->State == LyudKite::EState::Wound)
			&& !(Screen && Screen->IsOpen()) && !(UI && UI->IsOpen()) && !(Menu && Menu->IsOpen());
		const FVector HeroAt = H ? H->GetActorLocation() : FVector::ZeroVector;
		const float Time = (float)World->GetTimeSeconds();
		static TArray<FVector> Points;   // reused: no allocation per frame
		for (int32 I = Caught.Num() - 1; I >= 0; --I)
		{
			FCaught& C = Caught[I];
			LyudKite::FKite& K = *C.Sim;
			ALyudKite* Actor = C.Actor.Get();
			const FVector At = ToCm(K.Pos);
			C.GoneIn -= DeltaTime;
			if (!Actor || C.GoneIn <= 0.f || FVector::DistSquared(Cam, At) > FMath::Square(25000.f))
			{
				// there for ten minutes, or until the hero is far off
				RemoveCaught(I, true);
				continue;
			}
			C.AirIn -= DeltaTime;
			if (C.AirIn <= 0.f && WindSys)
			{
				C.AirIn = 0.2f;
				const FLyudLocalWind Air = WindSys->GetLocalWind(At);
				C.Wind = Air.Velocity;
				C.Gust = Air.Gust;
			}
			if (K.State == LyudKite::EState::Loose || K.State == LyudKite::EState::Grounded)
			{
				// free: it rocks down like a leaf, and lies
				LyudKite::FKiteInput In;
				In.Hand = ToM(C.Foot);
				In.Wind = LyudKite::FVec(C.Wind.X, C.Wind.Y, C.Wind.Z);
				In.GroundZ = C.Foot.Z * 0.01;
				if (K.State == LyudKite::EState::Loose)
				{
					LyudKite::Step(K, In, DeltaTime);
				}
				if (K.State == LyudKite::EState::Grounded)
				{
					FinishRescue(I);
					continue;
				}
				LyudKite::FVec Fwd, Nrm;
				LyudKite::Frame(K, In.Hand, In.Wind, Fwd, Nrm);
				Points.Reset();
				Actor->Show(ToCm(K.Pos), ToDir(Fwd), ToDir(Nrm), Points, C.Wind, DeltaTime);
				continue;
			}
			// hanging: it stirs in the air, and shakes at a tug
			C.Shake = FMath::Max(0.f, C.Shake - DeltaTime * 2.5f);
			const FVector Pos = At + C.Normal * (FMath::Sin(Time * 31.f) * 5.f * C.Shake) + FVector(0.f, 0.f, FMath::Sin(Time * 1.7f + float(I)) * 1.5f);
			const bool bInHand = HeldLine == I && H;
			const FVector Start = bInHand ? HandOf(H, RightHand(), HeroAt) : C.Foot;
			Points.SetNum(ALyudKite::LinePieces + 1, EAllowShrinking::No);
			for (int32 P = 0; P <= ALyudKite::LinePieces; ++P)
			{
				const float S = float(P) / float(ALyudKite::LinePieces);    // 0 at the hand or the ground, 1 at the kite
				if (bInHand)
				{
					// from the hand up to it, sagging until a tug pulls it straight
					FVector V = FMath::Lerp(Start, Pos, S);
					V.Z -= 4.f * S * (1.f - S) * (25.f - 20.f * C.Shake);
					Points[P] = V;
				}
				else
				{
					// straight down from it, the end lying off to one side
					const float D = 1.f - S;
					const float Out = D * D * D;
					Points[P] = FVector(FMath::Lerp(Pos.X, Start.X, Out), FMath::Lerp(Pos.Y, Start.Y, Out), FMath::Lerp(Pos.Z, Start.Z + 2.0, D));
				}
			}
			Actor->Show(Pos, C.Forward, C.Normal, Points, C.Wind, DeltaTime);
			// whose it is looks up at it
			C.LookIn -= DeltaTime;
			if (C.LookIn <= 0.f && C.Person >= 0 && People)
			{
				C.LookIn = 2.5f;
				if (ANpcBody* Npc = Cast<ANpcBody>(People->GetShownBody(C.Person)))
				{
					Npc->LookAtPoint(At, 3.f);
				}
			}
		}

		// the hero and the line: E takes its end, then every E is a tug (held: one after another)
		if (HeldLine >= 0 && (!bFree || !Caught.IsValidIndex(HeldLine) || Caught[HeldLine].Sim->State != LyudKite::EState::Snagged
			|| FVector::DistSquared2D(HeroAt, Caught[HeldLine].Foot) > FMath::Square(900.f)))
		{
			HeldLine = -1;
			if (bFree)
			{
				Say(TEXT("ui.kite.rescue.let_go"), 2.5f);
			}
		}
		if (bFree)
		{
			TugIn -= DeltaTime;
			const bool bUse = ANpcTalkDirector::UsePressed(PC);
			if (HeldLine < 0)
			{
				int32 Near = -1;
				double NearD = FMath::Square(300.0);
				for (int32 I = 0; I < Caught.Num(); ++I)
				{
					const double D = FVector::DistSquared2D(HeroAt, Caught[I].Foot);
					if (Caught[I].Sim->State == LyudKite::EState::Snagged && D < NearD)
					{
						NearD = D;
						Near = I;
					}
				}
				if (Near >= 0)
				{
					bRescuePrompt = true;
					if (bUse || bTugQueued)
					{
						HeldLine = Near;
						TugIn = 0.45f;
						Caught[Near].HintIn = 0.f;
						Say(TEXT("ui.kite.rescue.taken"), 2.f);
					}
					else if (SayLeft <= 0.f)
					{
						// (asked for three times a second, not every frame: the text is looked up by a string)
						PromptIn -= DeltaTime;
						if (PromptIn <= 0.f)
						{
							PromptIn = 0.3f;
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.take")));
						}
					}
				}
			}
			else
			{
				bRescuePrompt = true;
				FCaught& C = Caught[HeldLine];
				const LyudKite::FVec Puller = ToM(HeroAt);
				const LyudKite::FVec W(C.Wind.X, C.Wind.Y, C.Wind.Z);
				const bool bTug = bTugQueued || (TugIn <= 0.f && (bUse || (TugIn <= -0.4f && PC->IsInputKeyDown(EKeys::E))));
				if (bTug)
				{
					TugIn = 0.45f;
					const int32 WrongWas = C.Snag->Wrong;
					const bool bFreed = LyudKite::Tug(*C.Snag, Puller, W, C.Gust);
					C.Shake = 1.f;
					if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
					{
						S->PlayEffect(TEXT("reel_click"), HeroAt, 0.25f);
					}
					if (bFreed)
					{
						LyudKite::Free(*C.Sim);
						HeldLine = -1;
						Say(TEXT("ui.kite.rescue.free"), 3.f);
					}
					else if (C.Snag->Wrong > WrongWas)
					{
						Say(TEXT("ui.kite.rescue.deeper"), 2.5f);
					}
				}
				else if (SayLeft <= 0.f)
				{
					// what to do now: which side to pull from, and when (three looks a second)
					C.HintIn -= DeltaTime;
					if (C.HintIn <= 0.f)
					{
						C.HintIn = 0.3f;
						const double Side = LyudKite::SnagSide(*C.Snag, Puller, W);
						float InSeconds = 0.f, Factor = 1.f;
						bool bLull = false;
						if (Side < -0.25)
						{
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.wrong_side")));
						}
						else if (Side < 0.4)
						{
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.side")));
						}
						else if (C.Gust > 1.15f)
						{
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.now")));
						}
						else if (WindSys && WindSys->GetNextGust(ToCm(C.Sim->Pos), 12.f, InSeconds, Factor, bLull) && !bLull)
						{
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.wait_gust"), { FStringFormatArg(FMath::Max(1, FMath::CeilToInt(InSeconds))) }));
						}
						else
						{
							ShowPrompt(LyudText::Get(TEXT("ui.kite.rescue.tug")));
						}
					}
				}
			}
		}
	}
	else
	{
		HeldLine = -1;
	}
	bTugQueued = false;
	if (bHadPrompt && !bRescuePrompt && SayLeft <= 0.f)
	{
		// away from the line: the kite's own prompt again, or none
		if (HeroKite.IsValid())
		{
			LastPrompt.Reset();
			PromptIn = 0.f;
		}
		else
		{
			ShowPrompt(FText::GetEmpty());
		}
	}
}

void ULyudKiteFlying::SnagNow()
{
	if (Hero.Sim && LyudKite::IsUp(*Hero.Sim))
	{
		// as if it had flown into a branch here (the next tick finds it caught)
		LyudKite::FKiteInput In;
		In.Hand = ToM(Hero.Hand);
		In.bHitObstacle = true;
		LyudKite::Step(*Hero.Sim, In, 0.0);
		return;
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	// nobody's kite hung 10 m ahead, 4.5 m up, as in a tree
	const FVector Ahead = Pawn->GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector At = Pawn->GetActorLocation() + Ahead * 1000.f + FVector(0, 0, 450.f);
	FVector Wind = FVector::ZeroVector;
	if (const ULyudWindSubsystem* WindSys = ULyudWindSubsystem::Get(this))
	{
		Wind = WindSys->GetLocalWind(At).Velocity;
	}
	ALyudKite* A = MakeActor(ShapeDiamond, 3 + Caught.Num(), false);
	if (!A)
	{
		return;
	}
	TSharedPtr<LyudKite::FKite> Sim = MakeShared<LyudKite::FKite>();
	Sim->Def = LyudKite::Diamond();
	Sim->Seed = 11u + uint32(Caught.Num()) * 7919u;
	Sim->Pos = ToM(At);
	Sim->Line = 20.0;
	Sim->State = LyudKite::EState::Snagged;
	AddCaught(Sim, A, -1, FName(), Pawn->GetActorLocation(), Wind);
}

// ------------------------------------------------------------------ console, developer mode

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GKiteTest(TEXT("lyud.Kite.Test"),
		TEXT("lyud.Kite.Test N [diamond|box|delta|stunt|dragon]: N kites flown in a row 25 m downwind of the camera, all five kinds in turn or one kind (0 clears; the bench uses 10)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W))
			{
				K->SpawnTestKites(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10, Args.Num() > 1 ? ShapeByName(Args[1], -1) : -1);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteGive(TEXT("lyud.Kite.Give"), TEXT("lyud.Kite.Give [diamond|box|delta|stunt|dragon]: a kite in the pocket and in the hand"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudInventory* Inv = ULyudInventory::Get(W);
			if (!Inv) return;
			const FName Id = ItemOfShape(Args.Num() > 0 ? ShapeByName(Args[0], ShapeDiamond) : ShapeDiamond);
			Inv->Add(Id);
			Inv->Equip(Id);
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteReel(TEXT("lyud.Kite.Reel"), TEXT("lyud.Kite.Reel -1|0|1|off: pull, hold, let out (as the mouse buttons would); off gives the mouse back"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudKiteFlying* K = ULyudKiteFlying::Get(W);
			if (!K || Args.Num() == 0) return;
			if (Args[0] == TEXT("off")) K->ReleaseReel();
			else K->ForceReel(FCString::Atoi(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteSteer(TEXT("lyud.Kite.Steer"), TEXT("lyud.Kite.Steer -1..1|off: a stunt kite's lines, -1 the left one pulled, 1 the right; off gives the mouse back"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudKiteFlying* K = ULyudKiteFlying::Get(W);
			if (!K || Args.Num() == 0) return;
			if (Args[0] == TEXT("off")) K->ReleaseSteer();
			else K->ForceSteer(FCString::Atof(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs GKitePull(TEXT("lyud.Kite.Pull"), TEXT("lyud.Kite.Pull 0|1: a pull on both of a stunt kite's lines (lyud.Kite.Steer off gives the mouse back)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W))
			{
				K->ForcePull(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GKiteLaunch(TEXT("lyud.Kite.Launch"),
		TEXT("lyud.Kite.Launch [toss|lay|long]: the kite in the hand tossed up, stood on the sand, or held up 20 m downwind (then pull: lyud.Kite.Reel -1, a stunt kite lyud.Kite.Pull 1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W))
			{
				K->ForceLaunch(Args.Num() == 0 ? 0 : Args[0] == TEXT("long") ? 2 : Args[0] == TEXT("lay") ? 1 : 0);
			}
		}));
	FAutoConsoleCommandWithWorld GKiteSnag(TEXT("lyud.Kite.Snag"), TEXT("lyud.Kite.Snag: the hero's flying kite is caught where it is; with none up, a caught kite hangs 10 m ahead"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) K->SnagNow(); }));
	FAutoConsoleCommandWithWorld GKiteTug(TEXT("lyud.Kite.Tug"), TEXT("lyud.Kite.Tug: E on a caught kite's line (take it, then a tug each time)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) K->ForceTug(); }));
	FAutoConsoleCommandWithWorld GKiteTricks(TEXT("lyud.Kite.Tricks"), TEXT("lyud.Kite.Tricks: the tricks flown with the kite in the hand, in the log (KITE tricks ...)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *K->TricksReport()); }));
	FAutoConsoleCommandWithWorld GKiteState(TEXT("lyud.Kite.State"), TEXT("lyud.Kite.State: the hero's kite in the log (KITE ...)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *K->DebugState()); }));

	LyudDev::FAutoAction GDevKiteGive(TEXT("kite.give"), TEXT("kite"), TEXT("dev.action.kite.give"),
		[](UWorld* W)
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(W))
			{
				for (int32 I = 0; I < LyudKite::NumShapes; ++I)
				{
					Inv->Add(ItemOfShape(I));
				}
				Inv->Equip(KiteDiamondId);
			}
		});
	LyudDev::FAutoAction GDevKiteStunt(TEXT("kite.stunt"), TEXT("kite"), TEXT("dev.action.kite.stunt"),
		[](UWorld* W) { if (ULyudInventory* Inv = ULyudInventory::Get(W)) { Inv->Add(KiteStuntId); Inv->Equip(KiteStuntId); } });
	LyudDev::FAutoAction GDevKiteLong(TEXT("kite.long"), TEXT("kite"), TEXT("dev.action.kite.long"),
		[](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) K->ForceLaunch(2); });
	LyudDev::FAutoAction GDevKiteSnag(TEXT("kite.snag"), TEXT("kite"), TEXT("dev.action.kite.snag"),
		[](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) K->SnagNow(); });
	LyudDev::FAutoAction GDevKiteTest(TEXT("kite.test"), TEXT("kite"), TEXT("dev.action.kite.test"),
		[](UWorld* W) { if (ULyudKiteFlying* K = ULyudKiteFlying::Get(W)) K->SpawnTestKites(10); });
}
