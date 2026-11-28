// The drawn dinghy: its parts at their pivots, the meshes (imported, or built here while they are not), the levels of
// detail, the parts moved by the model's outputs, the wake. docs/yacht_school.md "Engine: the boats".
#include "LyudDinghyImpl.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "../Transport/LyudMeshKit.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailBuild, Log, All);

namespace LyudSailKit
{
	// ---------------------------------------------------------------- the classes' numbers

	// blender/sailing/sockets.json (parts' pivots, sockets; metres, +Y port -> cm, +Y starboard) and
	// blender/humans/sail_grips.json (origin_in_boat, deck_z, the Helm clips' hands at rest). In LyudSail::EBoat's order.
	const FClassDef Classes[3] = {
		{ TEXT("Optimist"),
			FVector(70.f, 0.f, -7.f), FVector(70.f, 0.f, 66.f), FVector(-118.f, 0.f, 0.f), FVector(-118.f, 0.f, 38.f), FVector(-40.f, 0.f, 40.f),
			FVector(6.f, 0.f, 0.f), FVector(70.f, 0.f, 102.f), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f),
			true, 58.f, 200.f, 231.f, 196.f, 230.f, 113.f, 47.f, 75.f,
			FVector(-40.f, -47.f, 43.f), FVector(-40.f, 0.f, -10.9f), 0.f, 0.74f, 43.9f,
			FVector(-25.f, 0.f, -4.f), FVector(-105.f, 0.f, -6.f),
			FVector(28.29f, 12.46f, 97.35f), FVector(44.67f, 25.4f, 125.04f), 45.f, 12.f },
		{ TEXT("Cadet"),
			FVector(62.f, 0.f, -10.f), FVector(62.f, 0.f, 112.f), FVector(-164.f, 0.f, 0.f), FVector(-164.f, 0.f, 40.f), FVector(-69.f, 0.f, 42.f),
			FVector(30.f, 0.f, -10.f), FVector(0.f, 0.f, 0.f), FVector(152.f, 0.f, 50.f), FVector(-0.2732f, 0.f, 0.962f),
			false, 0.f, 195.f, 525.f, 383.f, 322.f, 127.f, 52.f, 85.f,
			FVector(-90.f, -56.f, 46.f), FVector(-90.f, 0.f, -12.9f), 75.f, 0.86f, 48.9f,
			FVector(-55.f, 0.f, -2.f), FVector(-110.f, 0.f, -6.f),
			FVector(35.72f, 10.6f, 100.14f), FVector(48.48f, 29.74f, 94.36f), 54.f, 14.f },
		{ TEXT("Luch"),
			FVector(113.f, 0.f, -5.f), FVector(113.f, 0.f, 115.f), FVector(-215.5f, 0.f, 0.f), FVector(-215.5f, 0.f, 38.f), FVector(-113.5f, 0.f, 40.f),
			FVector(30.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f),
			true, 85.f, 275.f, 633.f, 480.f, 423.f, 137.f, 42.f, 100.f,
			FVector(-100.f, -60.f, 43.f), FVector(-100.f, 0.f, 2.f), 0.f, 1.f, 25.f,
			FVector(-95.f, 0.f, 10.f), FVector(-110.f, 0.f, -6.f),
			FVector(24.77f, 12.46f, 65.f), FVector(38.52f, 30.91f, 60.12f), 55.f, 15.f },
	};

	const FClassDef& ClassDef(int32 BoatClass)
	{
		return Classes[FMath::Clamp(BoatClass, 0, 2)];
	}

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

	// ---------------------------------------------------------------- assets, loaded once and rooted

	UStaticMesh* AssetAt(const FString& Path)
	{
		static TMap<FString, UStaticMesh*> Cache;
		if (UStaticMesh** Found = Cache.Find(Path))
		{
			return *Found;
		}
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (M)
		{
			M->AddToRoot();
		}
		else
		{
			UE_LOG(LogLyudSailBuild, Display, TEXT("SAIL no mesh %s yet: the code-built stand-in is drawn"), *Path);
		}
		Cache.Add(Path, M);
		return M;
	}

	UStaticMesh* Asset(const FString& Name)
	{
		return AssetAt(FString::Printf(TEXT("/Game/Sailing/SM_Sail_%s.SM_Sail_%s"), *Name, *Name));
	}

	UMaterialInterface* MaterialAt(const FString& Path)
	{
		static TMap<FString, UMaterialInterface*> Cache;
		if (UMaterialInterface** Found = Cache.Find(Path))
		{
			return *Found;
		}
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (M)
		{
			M->AddToRoot();
		}
		Cache.Add(Path, M);
		return M;
	}

	UMaterialInstanceDynamic* Paint(const FLinearColor& Tint)
	{
		static TMap<FLinearColor, UMaterialInstanceDynamic*> Cache;
		if (UMaterialInstanceDynamic** Found = Cache.Find(Tint))
		{
			return *Found;
		}
		UMaterialInterface* Basic = MaterialAt(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
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

	UMaterialInterface* FoamPaint()
	{
		static UMaterialInterface* Foam = nullptr;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			// see-through white: the cable car's glass master (Color, Opacity), as the train's wake uses it
			if (UMaterialInterface* Glass = MaterialAt(TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass")))
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Glass, GetTransientPackage());
				D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.94f, 0.94f));
				D->SetScalarParameterValue(TEXT("Opacity"), 0.5f);
				D->AddToRoot();
				Foam = D;
			}
			else
			{
				Foam = Paint(FLinearColor(0.9f, 0.94f, 0.94f));
			}
		}
		return Foam;
	}

	FLinearColor HullTint(int32 Colours)
	{
		static const FLinearColor Ways[] = {
			FLinearColor(0.80f, 0.80f, 0.76f), FLinearColor(0.72f, 0.10f, 0.07f), FLinearColor(0.07f, 0.22f, 0.55f),
			FLinearColor(0.90f, 0.62f, 0.08f), FLinearColor(0.08f, 0.40f, 0.24f), FLinearColor(0.10f, 0.45f, 0.55f),
			FLinearColor(0.85f, 0.35f, 0.08f), FLinearColor(0.30f, 0.34f, 0.38f) };
		const int32 N = UE_ARRAY_COUNT(Ways);
		return Ways[((Colours % N) + N) % N];
	}

	// ---------------------------------------------------------------- meshes built in code

	namespace
	{
		void BothSides(FLyudMeshKit& K, FName Mat, const FVector& A, const FVector& B, const FVector& C)
		{
			K.Tri(Mat, A, B, C);
			K.Tri(Mat, A, C, B);
		}

		FBuilt Finish(const FLyudMeshKit& K, const TCHAR* Name)
		{
			FBuilt Out;
			Out.Mesh = K.Build(GetTransientPackage(), FName(Name), Out.Slots);
			if (Out.Mesh)
			{
				Out.Mesh->AddToRoot();
			}
			return Out;
		}

		/** An open hull lofted over stations: gunwale, chine, keel, chine, gunwale; both faces. Fine: 1 a pointed bow, less a pram's. */
		void Loft(FLyudMeshKit& K, FName Mat, float Loa, float Beam, float Freeboard, float Draft, float Fine)
		{
			constexpr int32 Stations = 11;
			FVector P[Stations][5];
			for (int32 I = 0; I < Stations; ++I)
			{
				const float U = -1.f + 2.f * float(I) / float(Stations - 1);
				const float W = 0.5f * Beam * (U > 0.f ? FMath::Pow(FMath::Max(0.02f, 1.f - Fine * FMath::Pow(U, 2.2f)), 0.7f) : 1.f - 0.22f * U * U);
				const float Keel = -Draft * (1.f - 0.6f * U * U * (U > 0.f ? 1.f : 0.4f));
				const float Sheer = Freeboard * (1.f + 0.12f * U * U);
				const float X = U * 0.5f * Loa;
				P[I][0] = FVector(X, -W, Sheer);
				P[I][1] = FVector(X, -0.82f * W, 0.35f * Keel);
				P[I][2] = FVector(X, 0.f, Keel);
				P[I][3] = FVector(X, 0.82f * W, 0.35f * Keel);
				P[I][4] = FVector(X, W, Sheer);
			}
			for (int32 I = 0; I + 1 < Stations; ++I)
			{
				for (int32 J = 0; J < 4; ++J)
				{
					BothSides(K, Mat, P[I][J], P[I + 1][J], P[I + 1][J + 1]);
					BothSides(K, Mat, P[I][J], P[I + 1][J + 1], P[I][J + 1]);
				}
			}
			// the transom, and a pram's bow board
			for (const int32 I : { 0, Stations - 1 })
			{
				for (int32 J = 1; J < 4; ++J)
				{
					BothSides(K, Mat, P[I][0], P[I][J], P[I][J + 1]);
				}
			}
		}
	}

	const FBuilt& Fallback(int32 BoatClass, EBuilt What)
	{
		static FBuilt Cache[3][10];
		static bool bMade[3][10] = {};
		const int32 C = FMath::Clamp(BoatClass, 0, 2);
		const int32 W = int32(What);
		if (bMade[C][W])
		{
			return Cache[C][W];
		}
		bMade[C][W] = true;
		const FClassDef& D = Classes[C];
		FLyudMeshKit K;
		switch (What)
		{
		case EBuilt::Hull:
			Loft(K, TEXT("A"), D.Loa, D.Beam, D.Freeboard, D.HullDraft, C == 0 ? 0.45f : 1.f);
			// a thwart, so it reads as a boat from above
			K.Box(TEXT("Wood"), FTransform(FVector(D.Board.X, 0.f, D.Freeboard * 0.6f)), FVector(8.f, D.Beam * 0.42f, 1.5f), false);
			break;
		case EBuilt::Mast:
			K.Rod(TEXT("Spar"), FVector::ZeroVector, FVector(0.f, 0.f, D.MastTop - D.Mast.Z), 2.6f, 8, true, false);
			break;
		case EBuilt::Boom:
			K.Rod(TEXT("Spar"), FVector(-2.f, 0.f, 0.f), FVector(-D.BoomLen, 0.f, 0.f), 2.2f, 6, true, false);
			break;
		case EBuilt::Main:
			if (C == 0)
			{
				// the sprit sail's four corners: tack, clew, peak, throat
				const FVector Tack(-4.f, 0.f, 2.f), Clew(-D.BoomLen + 6.f, 0.f, 2.f), Peak(-0.8f * D.BoomLen, 0.f, D.SailHead), Throat(-3.f, 0.f, 0.56f * D.SailHead);
				BothSides(K, TEXT("B"), Tack, Clew, Peak);
				BothSides(K, TEXT("B"), Tack, Peak, Throat);
			}
			else
			{
				BothSides(K, TEXT("B"), FVector(-4.f, 0.f, 2.f), FVector(-D.BoomLen + 6.f, 0.f, 2.f), FVector(-3.f, 0.f, D.SailHead));
			}
			break;
		case EBuilt::Jib:
			BothSides(K, TEXT("B"), FVector::ZeroVector, FVector(-100.f, 0.f, 14.f), D.JibAxis * 310.f);
			break;
		case EBuilt::Rudder:
			K.Box(TEXT("Wood"), FTransform(FVector(-12.f, 0.f, -22.f)), FVector(13.f, 1.2f, 50.f), false);
			break;
		case EBuilt::Tiller:
			K.Rod(TEXT("Wood"), FVector(-10.f, 0.f, 0.f), FVector(D.Ext.X - D.Tiller.X, 0.f, D.Ext.Z - D.Tiller.Z), 1.6f, 6, true, false);
			break;
		case EBuilt::Ext:
			K.Rod(TEXT("Spar"), FVector::ZeroVector, FVector(D.ExtLen, 0.f, 0.f), 1.1f, 6, true, false);
			break;
		case EBuilt::Board:
			K.Box(TEXT("Wood"), FTransform(FVector(-2.f, 0.f, -30.f)), FVector(13.f, 1.2f, 55.f), false);
			break;
		case EBuilt::Sprit:
			K.Rod(TEXT("Spar"), FVector::ZeroVector, FVector(-0.8f * D.BoomLen, 0.f, D.SailHead - (D.Sprit.Z - D.Boom.Z)), 1.4f, 5, true, false);
			break;
		}
		Cache[C][W] = Finish(K, *FString::Printf(TEXT("SM_SailFallback_%s_%d"), D.Name, W));
		return Cache[C][W];
	}

	const FBuilt& Ribbon()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			BothSides(K, TEXT("A"), FVector(0.f, 0.f, -0.7f), FVector(1.f, 0.f, -0.7f), FVector(1.f, 0.f, 0.7f));
			BothSides(K, TEXT("A"), FVector(0.f, 0.f, -0.7f), FVector(1.f, 0.f, 0.7f), FVector(0.f, 0.f, 0.7f));
			B = Finish(K, TEXT("SM_SailRibbon"));
		}
		return B;
	}

	const FBuilt& Vane()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Rod(TEXT("Dark"), FVector(-22.f, 0.f, 0.f), FVector(16.f, 0.f, 0.f), 0.5f, 5, true, false);
			K.Rod(TEXT("Dark"), FVector(0.f, 0.f, -6.f), FVector(0.f, 0.f, 0.f), 0.5f, 5, true, false);
			// the fin at the tail (it trails downwind), the point into the wind
			BothSides(K, TEXT("A"), FVector(-22.f, 0.f, 0.f), FVector(-34.f, 0.f, 6.f), FVector(-34.f, 0.f, -6.f));
			BothSides(K, TEXT("A"), FVector(16.f, 0.f, 1.6f), FVector(16.f, 0.f, -1.6f), FVector(22.f, 0.f, 0.f));
			B = Finish(K, TEXT("SM_SailVane"));
		}
		return B;
	}

	const FBuilt& RopePiece()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Rod(TEXT("Rope"), FVector::ZeroVector, FVector(1.f, 0.f, 0.f), 0.45f, 4, false, false);
			B = Finish(K, TEXT("SM_SailRopePiece"));
		}
		return B;
	}

	const FBuilt& FlagQuad()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			BothSides(K, TEXT("A"), FVector(0.f, 0.f, 0.f), FVector(90.f, 0.f, 0.f), FVector(90.f, 0.f, -66.f));
			BothSides(K, TEXT("A"), FVector(0.f, 0.f, 0.f), FVector(90.f, 0.f, -66.f), FVector(0.f, 0.f, -66.f));
			B = Finish(K, TEXT("SM_SailFlagQuad"));
		}
		return B;
	}

	const FBuilt& LaunchHull()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			Loft(K, TEXT("A"), 460.f, 165.f, 55.f, 20.f, 1.f);
			K.Box(TEXT("Wood"), FTransform(FVector(-150.f, 0.f, 25.f)), FVector(12.f, 70.f, 1.5f), false);
			K.Box(TEXT("Wood"), FTransform(FVector(20.f, 0.f, 25.f)), FVector(12.f, 74.f, 1.5f), false);
			B = Finish(K, TEXT("SM_SailLaunchHull"));
		}
		return B;
	}

	const FBuilt& LaunchMotor()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Box(TEXT("Dark"), FTransform(FVector(-8.f, 0.f, 22.f)), FVector(22.f, 14.f, 16.f), false);
			K.Box(TEXT("Spar"), FTransform(FVector(-10.f, 0.f, -28.f)), FVector(6.f, 3.f, 36.f), false);
			K.Rod(TEXT("Dark"), FVector(8.f, 0.f, 20.f), FVector(58.f, 0.f, 24.f), 1.6f, 6, true, false);
			B = Finish(K, TEXT("SM_SailLaunchMotor"));
		}
		return B;
	}

	const FBuilt& MarkBody(int32 Kind)
	{
		static FBuilt B[4];
		const int32 I = FMath::Clamp(Kind, 0, 3);
		if (!B[I].Mesh)
		{
			FLyudMeshKit K;
			if (I == 0)
			{
				K.Lathe(FTransform::Identity, { { 0.f, -43.f }, { 40.f, -43.f }, { 45.f, -30.f }, { 45.f, 75.f }, { 38.f, 88.f }, { 0.f, 88.f } },
					{ TEXT("Orange"), TEXT("Orange"), TEXT("Orange"), TEXT("Orange"), TEXT("Orange") }, 14, false, false);
			}
			else if (I == 1)
			{
				K.Lathe(FTransform::Identity, { { 0.f, -20.f }, { 24.f, -16.f }, { 26.f, 10.f }, { 6.f, 22.f }, { 0.f, 22.f } },
					{ TEXT("Orange"), TEXT("Orange"), TEXT("Orange"), TEXT("Orange") }, 12, false, false);
				K.Rod(TEXT("Spar"), FVector(0.f, 0.f, 20.f), FVector(0.f, 0.f, 190.f), 1.4f, 6, true, false);
				BothSides(K, TEXT("Orange"), FVector(0.f, 0.f, 188.f), FVector(52.f, 0.f, 172.f), FVector(0.f, 0.f, 154.f));
			}
			else if (I == 2)
			{
				K.Lathe(FTransform::Identity, { { 0.f, -17.f }, { 18.f, -12.f }, { 22.f, 6.f }, { 14.f, 28.f }, { 0.f, 34.f } },
					{ TEXT("Red"), TEXT("Red"), TEXT("White"), TEXT("White") }, 12, false, false);
			}
			else
			{
				K.Lathe(FTransform::Identity, { { 0.f, -14.f }, { 30.f, -10.f }, { 36.f, 4.f }, { 28.f, 14.f }, { 0.f, 14.f } },
					{ TEXT("Orange"), TEXT("Orange"), TEXT("White"), TEXT("White") }, 12, false, false);
				K.Rod(TEXT("Spar"), FVector(0.f, 0.f, 10.f), FVector(0.f, 0.f, 160.f), 1.2f, 6, true, false);
				BothSides(K, TEXT("Orange"), FVector(0.f, 0.f, 158.f), FVector(40.f, 0.f, 146.f), FVector(0.f, 0.f, 132.f));
			}
			B[I] = Finish(K, *FString::Printf(TEXT("SM_SailMarkBuilt_%d"), I));
		}
		return B[I];
	}

	const FBuilt& TrolleyFrame()
	{
		static FBuilt B;
		if (!B.Mesh)
		{
			// a unit trolley for a 1 m boat origin height: scaled per class in X/Y, the cradle at Z = 1 (scaled by TrolleyZ)
			FLyudMeshKit K;
			K.Rod(TEXT("Spar"), FVector(-0.3f, -0.4f, 0.55f), FVector(0.45f, 0.f, 0.55f), 0.02f, 5, true, false);
			K.Rod(TEXT("Spar"), FVector(-0.3f, 0.4f, 0.55f), FVector(0.45f, 0.f, 0.55f), 0.02f, 5, true, false);
			K.Rod(TEXT("Spar"), FVector(-0.3f, -0.45f, 0.5f), FVector(-0.3f, 0.45f, 0.5f), 0.02f, 5, true, false);
			B = Finish(K, TEXT("SM_SailTrolleyBuilt"));
		}
		return B;
	}

	void PaintBuilt(UStaticMeshComponent* C, const FBuilt& Built, const FLinearColor& Main, const FLinearColor& Second)
	{
		for (int32 I = 0; I < Built.Slots.Num(); ++I)
		{
			const FName S = Built.Slots[I];
			const FLinearColor Tint = S == TEXT("A") ? Main : S == TEXT("B") ? Second
				: S == TEXT("Spar") ? FLinearColor(0.62f, 0.63f, 0.65f) : S == TEXT("Wood") ? FLinearColor(0.5f, 0.35f, 0.2f)
				: S == TEXT("Dark") ? FLinearColor(0.05f, 0.05f, 0.06f) : S == TEXT("Orange") ? FLinearColor(0.95f, 0.32f, 0.04f)
				: S == TEXT("Red") ? FLinearColor(0.75f, 0.06f, 0.05f) : S == TEXT("Rope") ? FLinearColor(0.78f, 0.74f, 0.62f)
				: FLinearColor(0.9f, 0.9f, 0.88f);
			if (UMaterialInstanceDynamic* M = Paint(Tint))
			{
				C->SetMaterial(I, M);
			}
		}
	}

	// ---------------------------------------------------------------- components

	UStaticMeshComponent* Part(AActor* Actor, USceneComponent* Parent, const TCHAR* Name, TArray<TObjectPtr<UObject>>& Keep)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Actor, Name, RF_Transient);
		C->SetupAttachment(Parent);
		NoLumen(C);
		C->SetCastShadow(true);
		C->RegisterComponent();
		Keep.Add(C);
		return C;
	}

	USceneComponent* Frame(AActor* Actor, USceneComponent* Parent, const TCHAR* Name, TArray<TObjectPtr<UObject>>& Keep)
	{
		USceneComponent* C = NewObject<USceneComponent>(Actor, Name, RF_Transient);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		Keep.Add(C);
		return C;
	}

	UInstancedStaticMeshComponent* FoamPool(AActor* Actor, USceneComponent* Parent, int32 Count, TArray<TObjectPtr<UObject>>& Keep)
	{
		UStaticMesh* Disc = AssetAt(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!Disc)
		{
			return nullptr;
		}
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("Wake"), RF_Transient);
		I->SetupAttachment(Parent);
		NoLumen(I);
		I->SetCastShadow(false);
		I->SetUsingAbsoluteLocation(true);
		I->SetUsingAbsoluteRotation(true);
		I->SetUsingAbsoluteScale(true);
		I->SetStaticMesh(Disc);
		if (UMaterialInterface* Foam = FoamPaint())
		{
			I->SetMaterial(0, Foam);
		}
		I->RegisterComponent();
		I->SetWorldTransform(FTransform::Identity);
		TArray<FTransform> Start;
		Start.Init(FTransform(FQuat::Identity, Actor->GetActorLocation(), FVector::ZeroVector), Count);
		I->AddInstances(Start, false, true, false);
		Keep.Add(I);
		return I;
	}

	// ---------------------------------------------------------------- the wake's puffs

	void FWake::Init(UInstancedStaticMeshComponent* InIsm, int32 Count, const FVector& At)
	{
		Ism = InIsm;
		Puffs.SetNum(Count);
		Xf.Init(FTransform(FQuat::Identity, At, FVector::ZeroVector), Count);
		Next = 0;
		Alive = 0;
		bParked = true;
	}

	void FWake::Add(const FVector& At, float Yaw, float Size, float Life, float Rise)
	{
		if (!Ism || Puffs.Num() == 0)
		{
			return;
		}
		FPuff& P = Puffs[Next];
		Next = (Next + 1) % Puffs.Num();
		P.At = At;
		P.Yaw = Yaw;
		P.Size = Size;
		P.Life = FMath::Max(Life, 0.05f);
		P.Rise = Rise;
		P.Age = 0.f;
		bParked = false;
	}

	void FWake::Tick(float DeltaTime, const FVector& Home)
	{
		if (!Ism || bParked)
		{
			return;
		}
		Alive = 0;
		for (int32 I = 0; I < Puffs.Num(); ++I)
		{
			FPuff& P = Puffs[I];
			if (P.Life <= 0.f)
			{
				Xf[I] = FTransform(FQuat::Identity, Home, FVector::ZeroVector);
				continue;
			}
			P.Age += DeltaTime;
			const float T = P.Age / P.Life;
			if (T >= 1.f)
			{
				P.Life = 0.f;
				Xf[I] = FTransform(FQuat::Identity, Home, FVector::ZeroVector);
				continue;
			}
			++Alive;
			// it spreads as it ages and thins away (no per-instance opacity: it shrinks to nothing at the end)
			const float Spread = P.Size * 0.01f * (0.55f + 1.1f * T) * (1.f - T * T * T);
			const float Up = P.Rise * 4.f * T * (1.f - T);
			Xf[I] = FTransform(FRotator(0.f, P.Yaw, 0.f), P.At + FVector(0.f, 0.f, Up), FVector(Spread * 1.5f, Spread, P.Rise > 0.f ? Spread * 0.5f : 0.012f));
		}
		// one batch; the last one (nothing alive) parks every instance at the boat with no size
		Ism->BatchUpdateInstancesTransforms(0, Xf, true, false, true);
		if (Alive == 0)
		{
			bParked = true;
		}
	}

	void FWake::Hide(const FVector& Home)
	{
		if (!Ism || (bParked && Alive == 0))
		{
			return;
		}
		for (int32 I = 0; I < Puffs.Num(); ++I)
		{
			Puffs[I].Life = 0.f;
			Xf[I] = FTransform(FQuat::Identity, Home, FVector::ZeroVector);
		}
		Ism->BatchUpdateInstancesTransforms(0, Xf, true, false, true);
		Alive = 0;
		bParked = true;
	}
}

// ---------------------------------------------------------------- ALyudDinghy::FImpl: building

namespace
{
	void DinghyShow(USceneComponent* C, bool bShow)
	{
		if (C && C->IsVisible() != bShow)
		{
			C->SetVisibility(bShow);
		}
	}

	/** The imported mesh of a part, else the code-built one painted; false when it took the stand-in. */
	bool DinghyDress(UStaticMeshComponent* C, const FString& AssetName, int32 BoatClass, LyudSailKit::EBuilt What, const FLinearColor& Main)
	{
		C->EmptyOverrideMaterials();
		if (UStaticMesh* M = LyudSailKit::Asset(AssetName))
		{
			C->SetStaticMesh(M);
			return true;
		}
		const LyudSailKit::FBuilt& B = LyudSailKit::Fallback(BoatClass, What);
		C->SetStaticMesh(B.Mesh);
		LyudSailKit::PaintBuilt(C, B, Main, FLinearColor(0.92f, 0.92f, 0.88f));
		return false;
	}
}

void ALyudDinghy::FImpl::Build(ALyudDinghy& D, int32 InClass, int32 InNumber, int32 InColours)
{
	// every kit call qualified: an unqualified name here is looked up in ALyudDinghy and AActor first
	using LyudSailKit::FClassDef;
	using LyudSailKit::EBuilt;
	const int32 Class = FMath::Clamp(InClass, 0, 2);
	const FClassDef& Def = LyudSailKit::ClassDef(Class);
	Colours = InColours;
	if (!Hull)
	{
		// made once for the actor's life; a pooled boat of another class only changes meshes and pivots
		Hull = LyudSailKit::Frame(&D, D.Root, TEXT("Hull"), D.Kept);
		Rig = LyudSailKit::Frame(&D, Hull, TEXT("Rig"), D.Kept);
		CrewFrame = LyudSailKit::Frame(&D, D.Root, TEXT("CrewFrame"), D.Kept);
		HeroSeat = LyudSailKit::Frame(&D, D.Root, TEXT("HeroSeat"), D.Kept);
		HullMesh = LyudSailKit::Part(&D, Hull, TEXT("HullMesh"), D.Kept);
		Mast = LyudSailKit::Part(&D, Hull, TEXT("Mast"), D.Kept);
		Boom = LyudSailKit::Part(&D, Rig, TEXT("Boom"), D.Kept);
		Main = LyudSailKit::Part(&D, Rig, TEXT("Main"), D.Kept);
		Sprit = LyudSailKit::Part(&D, Rig, TEXT("Sprit"), D.Kept);
		Jib = LyudSailKit::Part(&D, Hull, TEXT("Jib"), D.Kept);
		Rudder = LyudSailKit::Part(&D, Hull, TEXT("Rudder"), D.Kept);
		Tiller = LyudSailKit::Part(&D, Hull, TEXT("Tiller"), D.Kept);
		Ext = LyudSailKit::Part(&D, Hull, TEXT("Ext"), D.Kept);
		Board = LyudSailKit::Part(&D, Hull, TEXT("Board"), D.Kept);
		Far = LyudSailKit::Part(&D, Hull, TEXT("Far"), D.Kept);
		Trolley = LyudSailKit::Part(&D, D.Root, TEXT("Trolley"), D.Kept);
		Cover = LyudSailKit::Part(&D, Hull, TEXT("Cover"), D.Kept);
		for (int32 I = 0; I < 2; ++I)
		{
			Rope[I] = LyudSailKit::Part(&D, Hull, I == 0 ? TEXT("Sheet0") : TEXT("Sheet1"), D.Kept);
			Rope[I]->SetCastShadow(false);
		}
		static const TCHAR* const TellNames[4] = { TEXT("Tell0"), TEXT("Tell1"), TEXT("Tell2"), TEXT("Tell3") };
		for (int32 I = 0; I < 4; ++I)
		{
			Tell[I] = LyudSailKit::Part(&D, Rig, TellNames[I], D.Kept);
			Tell[I]->SetCastShadow(false);
			Tell[I]->SetStaticMesh(LyudSailKit::Ribbon().Mesh);
			// a ribbon 26 cm long (MoveParts only turns it)
			Tell[I]->SetRelativeScale3D(FVector(26.f, 1.f, 1.3f));
			// green to starboard, red to port, as on every sail
			LyudSailKit::PaintBuilt(Tell[I], LyudSailKit::Ribbon(), I < 2 ? FLinearColor(0.05f, 0.75f, 0.15f) : FLinearColor(0.9f, 0.06f, 0.04f), FLinearColor::White);
		}
		Vane = LyudSailKit::Part(&D, Hull, TEXT("Vane"), D.Kept);
		Vane->SetCastShadow(false);
		Vane->SetStaticMesh(LyudSailKit::Vane().Mesh);
		LyudSailKit::PaintBuilt(Vane, LyudSailKit::Vane(), FLinearColor(0.9f, 0.1f, 0.05f), FLinearColor::White);
		if (UInstancedStaticMeshComponent* Pool = LyudSailKit::FoamPool(&D, D.Root, 28, D.Kept))
		{
			Wake.Init(Pool, 28, D.GetActorLocation());
		}
	}
	const FLinearColor Tint = LyudSailKit::HullTint(Colours);
	const FString N(Def.Name);
	bAssets = DinghyDress(HullMesh, N + TEXT("_Hull"), Class, EBuilt::Hull, Tint);
	DinghyDress(Mast, N + TEXT("_Mast"), Class, EBuilt::Mast, Tint);
	DinghyDress(Boom, N + TEXT("_Boom"), Class, EBuilt::Boom, Tint);
	const bool bMainAsset = DinghyDress(Main, N + TEXT("_Main"), Class, EBuilt::Main, Tint);
	DinghyDress(Rudder, N + TEXT("_Rudder"), Class, EBuilt::Rudder, Tint);
	DinghyDress(Tiller, N + TEXT("_Tiller"), Class, EBuilt::Tiller, Tint);
	DinghyDress(Ext, N + TEXT("_Ext"), Class, EBuilt::Ext, Tint);
	DinghyDress(Board, N + TEXT("_Board"), Class, EBuilt::Board, Tint);
	bool bJibAsset = false;
	if (Class == 0)
	{
		DinghyDress(Sprit, N + TEXT("_Sprit"), Class, EBuilt::Sprit, Tint);
	}
	else
	{
		Sprit->SetStaticMesh(nullptr);
	}
	if (Class == 1)
	{
		bJibAsset = DinghyDress(Jib, N + TEXT("_Jib"), Class, EBuilt::Jib, Tint);
	}
	else
	{
		Jib->SetStaticMesh(nullptr);
	}
	Far->EmptyOverrideMaterials();
	Far->SetStaticMesh(LyudSailKit::Asset(N + TEXT("_Far")));
	Cover->EmptyOverrideMaterials();
	Cover->SetStaticMesh(LyudSailKit::Asset(FString(TEXT("Cover_")) + N));
	Trolley->EmptyOverrideMaterials();
	if (UStaticMesh* T = LyudSailKit::Asset(FString(TEXT("Trolley_")) + N))
	{
		Trolley->SetStaticMesh(T);
		Trolley->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		Trolley->SetStaticMesh(LyudSailKit::TrolleyFrame().Mesh);
		LyudSailKit::PaintBuilt(Trolley, LyudSailKit::TrolleyFrame(), Tint, Tint);
		Trolley->SetRelativeScale3D(FVector(Def.Loa, Def.Beam, Def.TrolleyZ / 0.55f));
	}
	// the sheet's two pieces: the imported metre of rope, else a centimetre built here (MoveParts scales by RopeUnit)
	UStaticMesh* RopeAsset = LyudSailKit::Asset(TEXT("Rope"));
	for (int32 I = 0; I < 2; ++I)
	{
		Rope[I]->EmptyOverrideMaterials();
		Rope[I]->SetStaticMesh(RopeAsset ? RopeAsset : LyudSailKit::RopePiece().Mesh);
		if (!RopeAsset)
		{
			LyudSailKit::PaintBuilt(Rope[I], LyudSailKit::RopePiece(), Tint, Tint);
		}
	}
	// the cloth: mostly new sails, some old, a few patched (the SL_Sail slot's instance, by the boat's seed)
	const uint32 Pick = (B.Seed * 2654435761u) >> 28;
	const TCHAR* Variant = Pick < 10 ? TEXT("") : Pick < 14 ? TEXT("_Old") : TEXT("_Patched");
	if (bMainAsset)
	{
		if (UMaterialInterface* Cloth = LyudSailKit::MaterialAt(FString::Printf(TEXT("/Game/Sailing/Materials/MI_SL_Sail_%s_Main%s.MI_SL_Sail_%s_Main%s"), Def.Name, Variant, Def.Name, Variant)))
		{
			Main->SetMaterialByName(TEXT("SL_Sail"), Cloth);
		}
	}
	if (bJibAsset)
	{
		if (UMaterialInterface* Cloth = LyudSailKit::MaterialAt(FString::Printf(TEXT("/Game/Sailing/Materials/MI_SL_Sail_%s_Jib%s.MI_SL_Sail_%s_Jib%s"), Def.Name, Variant, Def.Name, Variant)))
		{
			Jib->SetMaterialByName(TEXT("SL_Sail"), Cloth);
		}
	}
	// pivots at rest
	Hull->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	Mast->SetRelativeLocationAndRotation(Def.Mast, FRotator::ZeroRotator);
	Rig->SetRelativeLocationAndRotation(Def.Boom, FRotator::ZeroRotator);
	Boom->SetRelativeLocation(FVector::ZeroVector);
	Main->SetRelativeLocation(FVector::ZeroVector);
	Sprit->SetRelativeLocation(Def.Sprit - Def.Boom);
	Jib->SetRelativeLocationAndRotation(Def.Jib, FRotator::ZeroRotator);
	Rudder->SetRelativeLocationAndRotation(Def.Rudder, FRotator::ZeroRotator);
	Tiller->SetRelativeLocationAndRotation(Def.Tiller, FRotator::ZeroRotator);
	Ext->SetRelativeLocationAndRotation(Def.Ext, FRotator::ZeroRotator);
	Board->SetRelativeLocationAndRotation(Def.Board, FRotator::ZeroRotator);
	Far->SetRelativeLocation(FVector::ZeroVector);
	Cover->SetRelativeLocation(FVector::ZeroVector);
	Trolley->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -Def.TrolleyZ), FRotator::ZeroRotator);
	Vane->SetRelativeLocation(FVector(Def.Mast.X, 0.f, Def.MastTop + 8.f));
	// the custom primitive data block, the same on every part: camber, flutter, the number, the hull's tint
	UStaticMeshComponent* All[] = { HullMesh, Mast, Boom, Main, Sprit, Jib, Rudder, Tiller, Ext, Board, Far, Cover };
	for (UStaticMeshComponent* C : All)
	{
		C->SetCustomPrimitiveDataFloat(0, 0.f);
		C->SetCustomPrimitiveDataFloat(1, 0.f);
		C->SetCustomPrimitiveDataFloat(2, float(FMath::Clamp(InNumber, 0, 999)));
		C->SetCustomPrimitiveDataFloat(3, Tint.R);
		C->SetCustomPrimitiveDataFloat(4, Tint.G);
		C->SetCustomPrimitiveDataFloat(5, Tint.B);
	}
	SentCamber = SentFlutter = 99.f;
	BuiltClass = Class;
}

void ALyudDinghy::FImpl::ApplyLod(ALyudDinghy& D)
{
	const ELyudDinghyLod L = D.Lod;
	if (!Hull)
	{
		return;
	}
	if (L == ELyudDinghyLod::Parked)
	{
		Wake.Hide(D.GetActorLocation());
		if (D.bOnTrolley)
		{
			ShowParked(D);
		}
		else
		{
			D.SetActorHiddenInGame(true);
		}
		return;
	}
	D.SetActorHiddenInGame(false);
	const bool bNear = L == ELyudDinghyLod::Near;
	const bool bFar = L == ELyudDinghyLod::Far;
	// far: the one merged mesh when it is imported, else the hull and the mainsail alone
	const bool bMerged = bFar && Far->GetStaticMesh() != nullptr;
	const bool bParts = !bFar;
	DinghyShow(Far, bMerged);
	DinghyShow(HullMesh, !bMerged);
	DinghyShow(Main, !bMerged);
	DinghyShow(Mast, !bMerged);
	DinghyShow(Boom, bParts);
	DinghyShow(Sprit, bParts && BuiltClass == 0);
	DinghyShow(Jib, bParts && BuiltClass == 1);
	DinghyShow(Rudder, bParts);
	DinghyShow(Tiller, bParts);
	DinghyShow(Board, bParts);
	DinghyShow(Ext, bNear);
	DinghyShow(Vane, bNear);
	for (UStaticMeshComponent* T : Tell)
	{
		DinghyShow(T, bNear);
	}
	for (UStaticMeshComponent* R : Rope)
	{
		DinghyShow(R, bNear);
	}
	DinghyShow(Trolley, false);
	DinghyShow(Cover, false);
	if (Wake.Ism)
	{
		DinghyShow(Wake.Ism, !bFar);
	}
	if (bFar)
	{
		Wake.Hide(D.GetActorLocation());
	}
	// the middle distance draws every part's LOD1 (0 = the mesh's own choice)
	UStaticMeshComponent* Lods[] = { HullMesh, Mast, Boom, Main, Sprit, Jib, Rudder, Tiller, Board };
	const int32 Forced = L == ELyudDinghyLod::Mid ? 2 : 0;
	for (UStaticMeshComponent* C : Lods)
	{
		// SetForcedLodModel does nothing when the value is the same (the field itself is not read here: it may not stay public)
		C->SetForcedLodModel(Forced);
	}
	// shadows only where they are seen
	const bool bShadow = !bFar;
	if (HullMesh->CastShadow != bShadow)
	{
		HullMesh->SetCastShadow(bShadow);
		Main->SetCastShadow(bShadow);
		Mast->SetCastShadow(bShadow);
	}
	if (!bNear)
	{
		StopLoops(D, 0.3f);
	}
}

void ALyudDinghy::FImpl::ShowParked(ALyudDinghy& D)
{
	D.SetActorHiddenInGame(false);
	const bool bSail = bRigged && !bCovered;
	DinghyShow(Far, false);
	DinghyShow(HullMesh, true);
	DinghyShow(Trolley, true);
	DinghyShow(Cover, bCovered && Cover->GetStaticMesh() != nullptr);
	DinghyShow(Mast, bSail);
	DinghyShow(Boom, bSail);
	DinghyShow(Main, bSail);
	DinghyShow(Sprit, bSail && BuiltClass == 0);
	DinghyShow(Jib, bSail && BuiltClass == 1);
	DinghyShow(Rudder, !bCovered);
	DinghyShow(Tiller, !bCovered);
	DinghyShow(Board, false);
	DinghyShow(Ext, false);
	DinghyShow(Vane, false);
	for (UStaticMeshComponent* T : Tell)
	{
		DinghyShow(T, false);
	}
	for (UStaticMeshComponent* R : Rope)
	{
		DinghyShow(R, false);
	}
	if (Wake.Ism)
	{
		DinghyShow(Wake.Ism, false);
	}
	UStaticMeshComponent* Lods[] = { HullMesh, Mast, Boom, Main, Sprit, Jib, Rudder, Tiller, Board };
	for (UStaticMeshComponent* C : Lods)
	{
		C->SetForcedLodModel(0);
	}
	// on the trolley: upright, the boom amidships and a little off, the sail shivering in whatever wind there is
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	Hull->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	Rig->SetRelativeRotation(FRotator(0.f, 8.f, 0.f));
	Mast->SetRelativeRotation(FRotator::ZeroRotator);
	Rudder->SetRelativeRotation(FRotator::ZeroRotator);
	Tiller->SetRelativeLocationAndRotation(Def.Tiller, FRotator::ZeroRotator);
	Main->SetCustomPrimitiveDataFloat(0, 0.f);
	Main->SetCustomPrimitiveDataFloat(1, 0.45f);
	Jib->SetCustomPrimitiveDataFloat(0, 0.f);
	Jib->SetCustomPrimitiveDataFloat(1, 0.45f);
	SentCamber = SentFlutter = 99.f;
}

// ---------------------------------------------------------------- moving the parts

void ALyudDinghy::FImpl::MoveParts(ALyudDinghy& D, float DeltaTime)
{
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const LyudSail::FOutputs& O = B.Out;
	const bool bNear = D.Lod == ELyudDinghyLod::Near;
	Clock += DeltaTime;

	// the hull: the model's heel and pitch (the wave's slope is in both), lower in the water when swamped
	const float HeelDeg = FMath::RadiansToDegrees(float(O.Heel));
	const float PitchDeg = FMath::RadiansToDegrees(float(O.Pitch));
	const float Sunk = 7.f * float(O.Water) + (FMath::Abs(HeelDeg) > 150.f ? 6.f : 0.f);
	Hull->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -Sunk), FRotator(PitchDeg, 0.f, HeelDeg));

	// boom and sail: the model's angle is + to starboard, the boom lies along -X, so the yaw is its negative
	const float BoomDeg = -FMath::RadiansToDegrees(float(O.Boom));
	Rig->SetRelativeRotation(FRotator(0.f, BoomDeg, 0.f));
	if (BuiltClass == 2)
	{
		// the Luch's unstayed mast turns with its sleeve sail
		Mast->SetRelativeRotation(FRotator(0.f, BoomDeg, 0.f));
	}
	if (BuiltClass == 1)
	{
		Jib->SetRelativeRotation(FQuat(Def.JibAxis.GetSafeNormal(), -float(O.Jib)));
	}
	const float RudderDeg = -FMath::RadiansToDegrees(float(O.RudderAngle));
	Rudder->SetRelativeRotation(FRotator(0.f, RudderDeg, 0.f));
	Tiller->SetRelativeRotation(FRotator(0.f, RudderDeg, 0.f));
	if (Def.bDagger)
	{
		Board->SetRelativeLocation(Def.Board + FVector(0.f, 0.f, (1.f - float(O.Board)) * Def.BoardRaise));
	}
	else
	{
		// the cadet's centreboard swings up into its case (sockets.json: about -80 deg in Blender = +80 of Unreal pitch)
		Board->SetRelativeRotation(FRotator(80.f * (1.f - float(O.Board)), 0.f, 0.f));
	}

	// the cloth's belly and flutter live in the material: sent only when they have changed
	const float CamberWant = float(O.CamberSide) * (1.f - 0.7f * float(O.MainLuff));
	Camber = FMath::FInterpTo(Camber, CamberWant, DeltaTime, 6.f);
	const float Flutter = float(O.MainLuff);
	if (FMath::Abs(Camber - SentCamber) > 0.02f)
	{
		SentCamber = Camber;
		// the mesh's + is toward its local +Y of Blender = port: the model's + is starboard
		Main->SetCustomPrimitiveDataFloat(0, -Camber);
		if (BuiltClass == 1)
		{
			Jib->SetCustomPrimitiveDataFloat(0, -Camber);
		}
	}
	if (FMath::Abs(Flutter - SentFlutter) > 0.03f)
	{
		SentFlutter = Flutter;
		Main->SetCustomPrimitiveDataFloat(1, Flutter);
		if (BuiltClass == 1)
		{
			Jib->SetCustomPrimitiveDataFloat(1, float(O.JibLuff));
		}
	}
	if (!bNear)
	{
		return;
	}

	// the tiller extension: from the tiller's tip to the helm's hand
	{
		const float Len = Def.Ext.X - Def.Tiller.X;
		const FVector Tip = FVector(Def.Rudder.X, 0.f, Def.Ext.Z) + FRotator(0.f, RudderDeg, 0.f).RotateVector(FVector(Len, 0.f, 0.f));
		const FVector Hand = TillerHand(D);
		const FVector Dir = (Hand - Tip).GetSafeNormal(1e-3f, FVector::ForwardVector);
		Ext->SetRelativeLocationAndRotation(Tip, FRotationMatrix::MakeFromX(Dir).Rotator());
	}

	// the mainsheet: boom block -> ratchet block -> the hand
	{
		const float Unit = Rope[0]->GetStaticMesh() == LyudSailKit::RopePiece().Mesh ? 1.f : 0.01f;
		const FVector OnBoom = Def.Boom + FRotator(0.f, BoomDeg, 0.f).RotateVector(Def.BoomBlock);
		const FVector Fall = OnBoom - Def.SheetBlock;
		Rope[0]->SetRelativeLocationAndRotation(Def.SheetBlock, FRotationMatrix::MakeFromX(Fall.GetSafeNormal(1e-3f, FVector::UpVector)).Rotator());
		Rope[0]->SetRelativeScale3D(FVector(FMath::Max(float(Fall.Size()), 1.f) * Unit, 1.f, 1.f));
		const bool bHeld = Crew[0].Body.IsValid() || D.bHeroAboard;
		DinghyShow(Rope[1], bHeld);
		if (bHeld)
		{
			// the helm's forward hand, on the side they sit
			// (the port clip's space: +X the body's left, +Y the bow; a body of the class's scale)
			const FVector G = Def.SheetGrip * Def.ClipScale;
			const FVector Hand = Def.BodyOrigin + FVector(G.Y, float(SitSide) * G.X, G.Z);
			const FVector To = Hand - Def.SheetBlock;
			Rope[1]->SetRelativeLocationAndRotation(Def.SheetBlock, FRotationMatrix::MakeFromX(To.GetSafeNormal(1e-3f, FVector::UpVector)).Rotator());
			Rope[1]->SetRelativeScale3D(FVector(FMath::Max(float(To.Size()), 1.f) * Unit, 1.f, 1.f));
		}
	}

	// the telltales: two a side, a third and two thirds up the luff. Streaming aft when the flow is attached, lifting
	// and dancing when that side's flow breaks away, hanging when stalled or there is no wind.
	{
		const float Air = FMath::Clamp(float(O.AppWindSpeed) / 3.f, 0.f, 1.f);
		const int32 Belly = O.CamberSide;     // +1: the cloth bellies to starboard, so starboard is the lee side
		for (int32 I = 0; I < 4; ++I)
		{
			const bool bStarboard = I < 2;
			const float H = (I % 2) == 0 ? 0.36f : 0.66f;
			const float Chord = Def.BoomLen * (1.f - 0.75f * H);
			const float Side = bStarboard ? 1.f : -1.f;
			// the cloth itself moves sideways with the belly: the ribbon rides on it
			const float Shift = Camber * 0.05f * Chord;
			const FVector At(-0.22f * Chord - 6.f, Shift + Side * 4.f, H * Def.SailHead);
			const bool bLee = Belly != 0 && (Belly > 0) == bStarboard;
			const LyudSail::ETell State = Belly == 0 ? LyudSail::ETell::Lifting : (bLee ? O.TellLeeward : O.TellWindward);
			const float Phase = float(Clock) * (9.f + 2.f * I) + 1.7f * I;
			float Pitch = 0.f, Yaw = 180.f;
			if (Air < 0.25f || State == LyudSail::ETell::Stalled)
			{
				// drooping, turning idly
				Pitch = -68.f + 10.f * FMath::Sin(Phase * 0.35f);
				Yaw += 50.f * FMath::Sin(Phase * 0.22f);
			}
			else if (State == LyudSail::ETell::Lifting)
			{
				Pitch = 48.f + 22.f * FMath::Sin(Phase);
				Yaw += Side * 25.f + 20.f * FMath::Sin(Phase * 1.3f);
			}
			else
			{
				Pitch = 5.f * FMath::Sin(Phase);
				Yaw += 4.f * FMath::Sin(Phase * 1.4f);
			}
			// the ribbon's free end points aft (yaw 180); a positive pitch lifts it, a negative one lets it hang
			Tell[I]->SetRelativeLocationAndRotation(At, FRotator(Pitch, Yaw, 0.f));
		}
	}

	// the masthead vane: its point into the apparent wind (+ from starboard = yaw to starboard)
	Vane->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(float(O.AppWindAngle)), 0.f));
}

void ALyudDinghy::FImpl::MoveWake(ALyudDinghy& D, float DeltaTime)
{
	if (!Wake.Ism || D.Lod == ELyudDinghyLod::Far || D.Lod == ELyudDinghyLod::Parked)
	{
		return;
	}
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const LyudSail::FOutputs& O = B.Out;
	const FVector Home(DrawPos.X, DrawPos.Y, SurfaceZ);
	const bool bAfloat = O.State == LyudSail::EState::Sailing || O.State == LyudSail::EState::Swamped || O.State == LyudSail::EState::Towed;
	if (bAfloat)
	{
		const FVector Fwd(FMath::Cos(DrawYaw), FMath::Sin(DrawYaw), 0.f);
		const FVector Right(-Fwd.Y, Fwd.X, 0.f);
		const float YawDeg = FMath::RadiansToDegrees(DrawYaw);
		const float Speed = FMath::Abs(float(O.Speed));
		const float Gap = D.Lod == ELyudDinghyLod::Near ? 0.55f : 1.3f;
		WakeRun += Speed * DeltaTime;
		if (WakeRun > Gap)
		{
			WakeRun = 0.f;
			if (O.Wake > 0.06)
			{
				const FVector Stern = Home - Fwd * (0.5f * Def.Loa) + FVector(0.f, 0.f, 3.f);
				Wake.Add(Stern, YawDeg, Def.Beam * (0.5f + 0.9f * float(O.Wake)), 2.2f + 2.f * float(O.Wake), 0.f);
				if (O.Planing > 0.5)
				{
					// the rooster tail off the transom on the plane
					Wake.Add(Stern - Fwd * 30.f, YawDeg, 45.f + 30.f * float(O.Planing), 0.6f, 28.f * float(O.Planing));
				}
			}
		}
		if (O.Spray > 0.15 && D.Lod == ELyudDinghyLod::Near)
		{
			SprayRun += DeltaTime * (4.f + 10.f * float(O.Spray));
			if (SprayRun > 1.f)
			{
				SprayRun = 0.f;
				const float Side = Rng.FRand() < 0.5f ? -1.f : 1.f;
				Wake.Add(Home + Fwd * (0.42f * Def.Loa) + Right * (Side * 0.25f * Def.Beam) + FVector(0.f, 0.f, 6.f), YawDeg + Side * 25.f,
					28.f + 40.f * float(O.Spray), 0.5f, 25.f + 45.f * float(O.Spray));
			}
		}
	}
	Wake.Tick(DeltaTime, Home);
}
