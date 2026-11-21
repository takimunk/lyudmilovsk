// Children's games in the yards: the game side (Public/People/LyudYardGames.h, docs/yard_games.md).
#include "People/LyudYardGames.h"

#include "YardGames.h"
#include "PeopleSim.h"
#include "../City/CityKit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "UI/LyudGameScreen.h"

// (named, not anonymous: unity builds put files of this folder together, and other files have helpers of these names)
namespace LyudYardDraw
{

	/** Games drawn at once (their children with bodies, the rope and the ball), and how far from the camera. */
	constexpr int32 MaxDrawn = 3;
	constexpr double DrawCm = 15000.0;
	/** The rope's segments. */
	constexpr int32 RopeSegs = 12;

	FVector ToUE(const LyudYard::FV2& V, float ZCm) { return FVector(V.X * 100.0, V.Y * 100.0, ZCm); }
	LyudYard::FV2 ToYard(const FVector& V) { return LyudYard::FV2((float)(V.X * 0.01), (float)(V.Y * 0.01)); }

	UMaterialInstanceDynamic* Tint(const FLinearColor& C)
	{
		static TMap<FLinearColor, UMaterialInstanceDynamic*> Cache;
		if (UMaterialInstanceDynamic** Found = Cache.Find(C))
		{
			return *Found;
		}
		UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Basic)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basic, GetTransientPackage());
		M->SetVectorParameterValue(TEXT("Color"), C);
		M->AddToRoot();
		Cache.Add(C, M);
		return M;
	}

	UStaticMesh* Shape(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	// --- the gestures: goals for the hands on the hand-goal layer (BodyGestures.h's frame: the mesh faces +Y, its left
	// is +X; written for the right hand, "out" away from the body's middle), worked out from the act and its time

	struct FV
	{
		float Out, Fwd, Up;
	};

	void Hand(FLyudGesturePose& G, int32 S, float W, uint8 Anchor, const FV& At, const FV& Fingers, const FV& Palm, float Curl)
	{
		auto Side = [S](const FV& V) { return FVector(S == 0 ? V.Out : -V.Out, V.Fwd, V.Up); };
		FLyudArmGoal& A = G.Arm[S];
		A.Weight = W;
		A.Anchor = Anchor;
		A.Offset = Side(At);
		A.Fingers = Side(Fingers).GetSafeNormal();
		A.Palm = Side(Palm).GetSafeNormal();
		A.Curl = Curl;
	}

	FLyudGesturePose GestureOf(const LyudYard::FKid& K, float Scale, uint32 Seed)
	{
		constexpr uint8 Pelvis = 0, Chest = 1, Head = 2;
		FLyudGesturePose G;
		G.Scale = Scale;
		const int32 M = 1, O = 0;   // the right hand leads
		const float T = K.ActT;
		const float E = FMath::SmoothStep(0.f, 0.25f, T);
		const float Ph = (float)(Seed % 628u) * 0.01f;
		switch (K.Act)
		{
		case LyudYard::EAct::Hop:
			// arms out for balance, wobbling a little with each hop
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Chest, { 36.f, 8.f, -8.f + 4.f * FMath::Sin(K.Phase * 6.283f + (float)S) }, { 1.f, 0.2f, -0.1f }, { 0.f, 0.f, -1.f }, 0.2f);
			}
			G.SpinePitch = 5.f * E;
			G.HeadPitch = -10.f * E;     // eyes on the squares
			break;
		case LyudYard::EAct::Jump:
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Pelvis, { 22.f, 16.f, 20.f + 60.f * K.Lift }, { 0.3f, 1.f, 0.f }, { 0.f, 0.f, -1.f }, 0.4f);
			}
			G.HeadPitch = -6.f * E;
			break;
		case LyudYard::EAct::Turn:
		{
			// the turning hand goes round a circle in front of the shoulder, holding the rope's end; the other hangs easy
			const float A = K.Phase;
			Hand(G, M, E, Chest, { 16.f + 12.f * FMath::Sin(A), 32.f, -4.f + 12.f * FMath::Cos(A) }, { 0.2f, 1.f, 0.f }, { -1.f, 0.f, 0.f }, 0.85f);
			G.SpineRoll = -2.f * E;
			break;
		}
		case LyudYard::EAct::Count:
			// face to the wall, both hands over the eyes, forehead to the arm
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Head, { 4.f, 10.f, 3.f }, { -0.6f, 0.2f, 0.8f }, { 0.f, -1.f, 0.f }, 0.1f);
			}
			G.HeadPitch = -12.f * E;
			G.SpinePitch = 10.f * E;
			break;
		case LyudYard::EAct::Crouch:
			// bent low behind the thing, hands on the knees, peeping up over it
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, E, Pelvis, { 13.f, 30.f, -28.f }, { 0.f, 0.3f, -1.f }, { 0.f, -1.f, 0.f }, 0.3f);
			}
			G.SpinePitch = 38.f * E;
			G.HeadPitch = (24.f + 6.f * FMath::Sin(T * 0.7f + Ph)) * E;
			G.HeadYaw = 15.f * FMath::Sin(T * 0.4f + Ph) * E;
			break;
		case LyudYard::EAct::Clap:
		{
			const float C = 3.5f + 3.5f * FMath::Sin(T * 21.f + Ph);
			const float W = E * (1.f - FMath::SmoothStep(2.0f, 2.5f, T));
			Hand(G, M, W, Chest, { C, 25.f, 0.f }, { -0.2f, 0.75f, 0.6f }, { -1.f, 0.f, 0.f }, 0.05f);
			Hand(G, O, W, Chest, { C - 2.f, 25.f, 1.f }, { -0.2f, 0.75f, 0.6f }, { -1.f, 0.f, 0.f }, 0.05f);
			break;
		}
		case LyudYard::EAct::Kick:
		{
			// the swing of a kick: the body back, one arm back and one forward
			const float W = FMath::SmoothStep(0.f, 0.08f, T) * (1.f - FMath::SmoothStep(0.25f, 0.45f, T));
			Hand(G, M, W, Chest, { 24.f, -14.f, -22.f }, { 0.2f, -0.5f, -1.f }, { -1.f, 0.f, 0.f }, 0.3f);
			Hand(G, O, W, Chest, { 26.f, 24.f, -8.f }, { 0.3f, 1.f, -0.3f }, { 0.f, 0.f, -1.f }, 0.3f);
			G.SpinePitch = -7.f * W;
			G.HeadPitch = -15.f * W;
			break;
		}
		case LyudYard::EAct::Reach:
			Hand(G, M, E, Chest, { 10.f, 52.f, 2.f }, { 0.1f, 1.f, 0.f }, { 0.f, 0.f, -1.f }, 0.f);
			G.SpinePitch = 8.f * E;
			break;
		case LyudYard::EAct::Cheer:
		{
			const float W = E * (1.f - FMath::SmoothStep(1.6f, 2.2f, T));
			for (int32 S = 0; S < 2; S++)
			{
				Hand(G, S, W, Head, { 22.f, 4.f, 34.f + 4.f * FMath::Sin(T * 9.f + (float)S) }, { 0.2f, 0.f, 1.f }, { 0.f, 1.f, 0.f }, 0.6f);
			}
			G.HeadPitch = 10.f * W;
			break;
		}
		case LyudYard::EAct::Point:
		{
			const float W = E * (1.f - FMath::SmoothStep(1.2f, 1.6f, T));
			Hand(G, M, W, Chest, { 13.f, 46.f, 3.f }, { 0.15f, 1.f, 0.05f }, { 0.f, 0.f, -1.f }, 0.1f);
			break;
		}
		case LyudYard::EAct::Look:
			G.HeadYaw = 35.f * FMath::Sin(T * 1.3f + Ph);
			G.HeadPitch = -4.f;
			break;
		default:
			break;
		}
		return G;
	}

	EPersonPose PoseOf(const LyudYard::FKid& K)
	{
		switch (K.Act)
		{
		case LyudYard::EAct::Walk:
		case LyudYard::EAct::Run: return EPersonPose::Walk;
		case LyudYard::EAct::Look: return EPersonPose::Look;
		default: return K.Speed > 0.5f ? EPersonPose::Walk : EPersonPose::Stand;
		}
	}
}

ALyudYardThings::ALyudYardThings()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

ULyudYardGames::ULyudYardGames() = default;
ULyudYardGames::~ULyudYardGames() = default;

ULyudYardGames* ULyudYardGames::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudYardGames>() : nullptr;
}

void ULyudYardGames::Deinitialize()
{
	TArray<int32> Ids;
	Shown.GetKeys(Ids);
	for (int32 Id : Ids)
	{
		Release(Id);
	}
	if (ALyudYardThings* T = Things.Get())
	{
		T->Destroy();
	}
	Model.Reset();
	Super::Deinitialize();
}

bool ULyudYardGames::Ensure()
{
	if (Model.IsValid())
	{
		return true;
	}
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (bTriedLoad || !Life || !Life->IsRunning() || !Life->GetSim())
	{
		return false;
	}
	bTriedLoad = true;
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("PeopleData") / TEXT("yard_games.json");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudYardGames: no %s (python3 tools/yard/yard_areas.py)"), *Path);
		return false;
	}
	TSharedPtr<LyudYard::FYard> Y = MakeShared<LyudYard::FYard>();
	if (!Y->Load(TCHAR_TO_UTF8(*Text)))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudYardGames: %s"), UTF8_TO_TCHAR(Y->LastError.c_str()));
		return false;
	}
	Y->Seed ^= (uint32)FDateTime::Now().GetTicks();
	Model = Y;
	BuildThings();
	UE_LOG(LogTemp, Display, TEXT("LyudYardGames: %d play areas"), (int32)Model->Areas.size());
	return true;
}

float ULyudYardGames::GroundAt(FShown& S, const FVector2D& At, float ZHint)
{
	if (FVector2D::DistSquared(S.GroundAt, At) < 30.0 * 30.0)
	{
		return S.GroundZ;
	}
	S.GroundAt = At;
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("YardGround"), false);
	if (ALyudYardThings* T = Things.Get())
	{
		Q.AddIgnoredActor(T);
	}
	const FVector From(At.X, At.Y, ZHint + 250.0), To(At.X, At.Y, ZHint - 400.0);
	S.GroundZ = GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Q) ? (float)Hit.ImpactPoint.Z : ZHint;
	return S.GroundZ;
}

void ULyudYardGames::BuildThings()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudYardThings* T = World->SpawnActor<ALyudYardThings>(ALyudYardThings::StaticClass(), FTransform(), SP);
	if (!T)
	{
		return;
	}
	Things = T;
	PropMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/City/M_CityProp.M_CityProp"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Ground = [World, T](const LyudYard::FV2& P, float ZHint) -> float
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("YardThings"), false);
		Q.AddIgnoredActor(T);
		const FVector At = LyudYardDraw::ToUE(P, ZHint);
		return World->LineTraceSingleByObjectType(Hit, At + FVector(0, 0, 300), At - FVector(0, 0, 500), FCollisionObjectQueryParams(ECC_WorldStatic), Q)
			? (float)Hit.ImpactPoint.Z : ZHint;
	};
	auto MakeComp = [T](UStaticMesh* Mesh, UMaterialInterface* Mat, bool bInstanced, bool bCollide, bool bShadow, float CullCm) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = bInstanced ? NewObject<UInstancedStaticMeshComponent>(T) : NewObject<UStaticMeshComponent>(T);
		C->SetMobility(bInstanced ? EComponentMobility::Static : EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(bShadow);
		C->SetCachedMaxDrawDistance(CullCm);
		C->LightingChannels.bChannel1 = bInstanced;     // as the town's own props
		// moving and small things stay out of the distance fields and Lumen's scene
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetupAttachment(T->Root);
		C->RegisterComponent();
		T->AddInstanceComponent(C);
		return C;
	};
	// the yards' things: one instanced set per kind (the town's own kit meshes), the trees in one forest, the bushes queued
	TMap<FString, UInstancedStaticMeshComponent*> ByKind;
	AProceduralForest* Forest = nullptr;
	for (const LyudYard::FArea& A : Model->Areas)
	{
		for (const LyudYard::FKit& K : A.Kit)
		{
			const FString Kind = UTF8_TO_TCHAR(K.Kind.c_str());
			const FVector At = LyudYardDraw::ToUE(K.At, Ground(K.At, A.Z * 100.f));
			if (Kind == TEXT("tree"))
			{
				if (!Forest)
				{
					FActorSpawnParameters FP;
					FP.ObjectFlags |= RF_Transient;
					FP.bDeferConstruction = true;
					Forest = World->SpawnActor<AProceduralForest>(AProceduralForest::StaticClass(), FTransform(), FP);
					if (Forest)
					{
						Forest->bSnapToGround = false;
						Forest->Mix.Reset();
						Forest->Variants = 3;
					}
				}
				if (Forest)
				{
					Forest->AddTree(FName(UTF8_TO_TCHAR(K.Species.c_str())), At, K.Age, K.Variant, K.Yaw, 1.f);
				}
				continue;
			}
			if (Kind == TEXT("bush"))
			{
				FBush B;
				B.Species = FName(UTF8_TO_TCHAR(K.Species.c_str()));
				B.Seed = K.Seed;
				B.Age = K.Age;
				B.At = At;
				PendingBushes.Add(B);
				continue;
			}
			const FString Key = Kind == TEXT("flowerbed") ? FString::Printf(TEXT("%s_%.2f"), *Kind, K.R) : Kind;
			UInstancedStaticMeshComponent** Found = ByKind.Find(Key);
			if (!Found)
			{
				TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
				if (K.R > 0.f)
				{
					Extra->SetNumberField(TEXT("r"), K.R);
				}
				CityKit::FMesh M;
				CityKit::BuildProp(M, Kind, 0, Extra);
				if (M.Idx.Num() == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("LyudYardGames: no kit mesh for '%s'"), *Kind);
					ByKind.Add(Key, nullptr);
					continue;
				}
				const bool bCollide = CityKit::PropCollides(Kind);
				UStaticMesh* Mesh = M.Build(T, *(TEXT("SM_Yard_") + Key.Replace(TEXT("."), TEXT("_"))), bCollide);
				Meshes.Add(Mesh);
				UInstancedStaticMeshComponent* C = Cast<UInstancedStaticMeshComponent>(MakeComp(Mesh, PropMaterial, true, bCollide, true, 15000.f));
				T->Kit.Add(C);
				Found = &ByKind.Add(Key, C);
			}
			if (*Found)
			{
				(*Found)->AddInstance(FTransform(FRotator(0.f, K.Yaw, 0.f), At), true);
			}
		}
	}
	if (Forest)
	{
		Forest->Tags.Add(TEXT("City"));
		Forest->FinishSpawning(FTransform());
	}
	// the chalked squares: the outline of each square (four strips; a pair row's middle line too) and the half circle of
	// "home" past the last row, a little off the ground
	if (UStaticMesh* Cube = LyudYardDraw::Shape(TEXT("Cube")))
	{
		UInstancedStaticMeshComponent* Chalk = Cast<UInstancedStaticMeshComponent>(MakeComp(Cube, LyudYardDraw::Tint(FLinearColor(0.82f, 0.82f, 0.78f)), true, false, false, 6000.f));
		T->Chalk = Chalk;
		const float W = 0.035f;     // a chalk line's width (m)
		for (const LyudYard::FArea& A : Model->Areas)
		{
			if (!A.Fits(LyudYard::EGame::Hopscotch))
			{
				continue;
			}
			const float Z = Ground(A.Chalk, A.Z * 100.f) + 0.6f;
			const LyudYard::FV2 F = LyudYard::FV2::Dir(A.ChalkYaw);
			const LyudYard::FV2 L(-F.Y, F.X);
			auto Strip = [&](const LyudYard::FV2& P0, const LyudYard::FV2& P1)
			{
				const LyudYard::FV2 Mid = (P0 + P1) * 0.5f;
				const float Len = (P1 - P0).Len();
				Chalk->AddInstance(FTransform(FRotator(0.f, (P1 - P0).Yaw(), 0.f), LyudYardDraw::ToUE(Mid, Z), FVector((Len + W) / 1.f, W, 0.006f)), true);
			};
			const float C = LyudYard::HopCell;
			for (int32 Row = 0; Row < LyudYard::HopRows - 1; Row++)
			{
				const float A0 = C * (float)Row, A1 = C * (float)(Row + 1);
				const float H = LyudYard::HopPair(Row) ? C : C * 0.5f;    // half the row's width
				auto At = [&](float Along, float Across) { return A.Chalk + F * Along + L * Across; };
				Strip(At(A0, -H), At(A0, H));
				Strip(At(A0, -H), At(A1, -H));
				Strip(At(A0, H), At(A1, H));
				if (LyudYard::HopPair(Row)) Strip(At(A0, 0.f), At(A1, 0.f));
				if (Row == LyudYard::HopRows - 2) Strip(At(A1, -H), At(A1, H));
			}
			// home: a half circle on the last line
			const LyudYard::FV2 Home = A.Chalk + F * (C * (float)(LyudYard::HopRows - 1));
			for (int32 I = 0; I < 6; I++)
			{
				const float U0 = -90.f + 30.f * (float)I, U1 = U0 + 30.f;
				Strip(Home + LyudYard::FV2::Dir(A.ChalkYaw + U0) * (C * 0.5f), Home + LyudYard::FV2::Dir(A.ChalkYaw + U1) * (C * 0.5f));
			}
		}
	}
	// the props that move: the rope's segments, the balls, the goals' bags (hidden till a game needs them)
	if (UStaticMesh* Cyl = LyudYardDraw::Shape(TEXT("Cylinder")))
	{
		for (int32 I = 0; I < LyudYardDraw::RopeSegs * LyudYardDraw::MaxDrawn; I++)
		{
			UStaticMeshComponent* C = MakeComp(Cyl, LyudYardDraw::Tint(FLinearColor(0.75f, 0.12f, 0.1f)), false, false, false, 6000.f);
			C->SetVisibility(false);
			T->Rope.Add(C);
		}
	}
	if (UStaticMesh* Sphere = LyudYardDraw::Shape(TEXT("Sphere")))
	{
		for (int32 I = 0; I < LyudYardDraw::MaxDrawn; I++)
		{
			UStaticMeshComponent* C = MakeComp(Sphere, LyudYardDraw::Tint(FLinearColor(0.85f, 0.18f, 0.12f)), false, false, true, 8000.f);
			C->SetVisibility(false);
			T->Balls.Add(C);
		}
	}
	if (UStaticMesh* Cube = LyudYardDraw::Shape(TEXT("Cube")))
	{
		for (int32 I = 0; I < 4 * LyudYardDraw::MaxDrawn; I++)
		{
			UStaticMeshComponent* C = MakeComp(Cube, LyudYardDraw::Tint(I % 2 ? FLinearColor(0.12f, 0.18f, 0.35f) : FLinearColor(0.35f, 0.22f, 0.12f)), false, false, true, 8000.f);
			C->SetVisibility(false);
			T->Posts.Add(C);
		}
	}
}

void ULyudYardGames::PlantBushes()
{
	if (PendingBushes.Num() == 0)
	{
		return;
	}
	const FVector Cam = CameraLocation();
	int32 Best = INDEX_NONE;
	double BestD = FMath::Square(20000.0);
	for (int32 I = 0; I < PendingBushes.Num(); I++)
	{
		const double D = FVector::DistSquared(PendingBushes[I].At, Cam);
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	if (Best == INDEX_NONE)
	{
		return;
	}
	const FBush B = PendingBushes[Best];
	PendingBushes.RemoveAtSwap(Best);
	FActorSpawnParameters PSP;
	PSP.ObjectFlags |= RF_Transient;
	PSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PSP.bDeferConstruction = true;
	const FTransform Xf(FRotator(0.f, (float)(B.Seed % 360), 0.f), B.At);
	if (AProceduralPlant* Pl = GetWorld()->SpawnActor<AProceduralPlant>(AProceduralPlant::StaticClass(), Xf, PSP))
	{
		Pl->Species = B.Species;
		Pl->Seed = B.Seed;
		Pl->AgeAtStart = B.Age;
		Pl->bDemoGrowth = false;
		Pl->bSnapToGround = false;
		Pl->Pot = EPlantPot::None;
		Pl->SummaryDistance = 3500.f;
		Pl->Tags.Add(TEXT("City"));
		Pl->FinishSpawning(Xf);
	}
}

FVector ULyudYardGames::CameraLocation() const
{
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			return PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	return FVector(0, 0, 1e7);
}

FVector ULyudYardGames::HeroLocation(FVector* OutVel) const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	if (OutVel)
	{
		*OutVel = P ? P->GetVelocity() : FVector::ZeroVector;
	}
	return P ? P->GetActorLocation() : FVector(1e9, 1e9, 0);
}

void ULyudYardGames::OfferChildren()
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!S)
	{
		return;
	}
	// (kept between calls: no allocations once the lists have grown)
	static std::vector<std::vector<LyudYard::FCandidate>> Offers;
	Offers.resize(Model->Areas.size());
	for (std::vector<LyudYard::FCandidate>& O : Offers)
	{
		O.clear();
	}
	const int PlayA = S->FindActivity("playground"), PlayB = S->FindActivity("play_run");
	for (const LyudPeople::FPerson& P : S->People)
	{
		if (P.Age < 5 || P.Age > 13 || P.Activity < 0 || (P.Activity != PlayA && P.Activity != PlayB))
		{
			continue;
		}
		if (P.State != LyudPeople::EState::Doing && P.State != LyudPeople::EState::Going)
		{
			continue;
		}
		const LyudYard::FV2 Pos(P.Pos.X, P.Pos.Y);
		const int Gi = Model->InGame(P.Id);
		const int Own = Gi >= 0 ? Model->Games[(size_t)Gi].Area : -1;
		for (int A = Own >= 0 ? Own : 0; A < (Own >= 0 ? Own + 1 : (int)Model->Areas.size()); A++)
		{
			if (LyudYard::FYard::Near(Model->Areas[(size_t)A], Pos, P.State == LyudPeople::EState::Doing, Own == A))
			{
				LyudYard::FCandidate C;
				C.Id = P.Id;
				C.Age = P.Age;
				C.Pos = Pos;
				Offers[(size_t)A].push_back(C);
				break;
			}
		}
	}
	for (int A = 0; A < (int)Model->Areas.size(); A++)
	{
		Model->Offer(A, Offers[(size_t)A]);
	}
}

void ULyudYardGames::Prompt(const FText& Text)
{
	if (Text.EqualTo(LastPrompt))
	{
		return;
	}
	LastPrompt = Text;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->SetPrompt(this, Text);
	}
}

void ULyudYardGames::PromptByKey(const TCHAR* Key, int32 Arg)
{
	// a held prompt is asked for every frame: its words are looked up (and formatted) only when the key or its number changes
	if (PromptKey == Key && PromptArg == Arg)
	{
		return;
	}
	PromptKey = Key;
	PromptArg = Arg;
	Prompt(!Key ? FText::GetEmpty() : (Arg != MIN_int32 ? LyudText::Get(Key, { FStringFormatArg(Arg) }) : LyudText::Get(Key)));
}

void ULyudYardGames::SayLine(int32 Id, const FString& Key)
{
	const FShown* S = Shown.Find(Id);
	ANpcBody* Body = S ? S->Body.Get() : nullptr;
	if (!Body || FVector::DistSquared(Body->GetActorLocation(), CameraLocation()) > FMath::Square(3000.0))
	{
		return;
	}
	// a boy's or a girl's words (tools/yard/strings.json: line.yard.*.m / .f)
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	const bool bGirl = Sim && Id >= 0 && Id < (int32)Sim->People.size() && Sim->People[(size_t)Id].bFemale;
	if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
	{
		Talk->SayLine(Body, Key + (bGirl ? TEXT(".f") : TEXT(".m")), Id);
	}
}

void ULyudYardGames::Tick(float DeltaTime)
{
	if (!Ensure())
	{
		return;
	}
	DeltaTime = FMath::Min(DeltaTime, 0.1f);
	PlantBushes();
	FVector Vel;
	const FVector Hero = HeroLocation(&Vel);
	Model->bHeroHere = Hero.X < 1e8;
	Model->HeroPos = LyudYardDraw::ToYard(Hero);
	Model->HeroVel = LyudYard::FV2((float)(Vel.X * 0.01), (float)(Vel.Y * 0.01));
	OfferClock -= DeltaTime;
	if (OfferClock <= 0.f)
	{
		OfferClock = 1.f;
		OfferChildren();
		// (the props' kept ground is traced anew: ground that came in late, a game that moved to another slot)
		RopeGround.Reset();
		PostGround.Reset();
	}
	Model->Step(DeltaTime);
	HandleEvents();
	UpdateHero(DeltaTime);
	Present(DeltaTime);
	DrawProps();
}

void ULyudYardGames::HandleEvents()
{
	ULyudGameScreen* GS = ULyudGameScreen::Get(this);
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	auto Name = [Life](int32 Id) { return Id >= 0 && Life ? Life->GetDisplayName(Id).ToString() : FString(); };
	const int HeroGi = Model->HeroGame();
	const int32 HeroUid = HeroGi >= 0 ? Model->Games[(size_t)HeroGi].Uid : HeroGameUid;
	for (const LyudYard::FEvent& E : Model->Events)
	{
		const FString Game = UTF8_TO_TCHAR(LyudYard::GameName(E.G));
		const bool bHeros = E.Game == HeroUid;
		switch (E.Kind)
		{
		case LyudYard::EEvent::Start:
			SayLine(E.Who, TEXT("line.yard.start.") + Game);
			break;
		case LyudYard::EEvent::Roles:
			if (E.G == LyudYard::EGame::Hide)
			{
				if (bHeros) bHeroFoundThisRound = false;
				if (E.Who == LyudYard::HeroId)
				{
					if (GS) GS->Toast(LyudText::Get(TEXT("yard.toast.you_seek")));
				}
				else
				{
					SayLine(E.Who, TEXT("line.yard.count"));
				}
			}
			break;
		case LyudYard::EEvent::Found:
			if (E.Who == LyudYard::HeroId)
			{
				bHeroFoundThisRound = true;
				if (GS) GS->Toast(LyudText::Get(TEXT("yard.toast.found_you")));
			}
			else if (E.Other != LyudYard::HeroId)
			{
				SayLine(E.Other, TEXT("line.yard.found"));
			}
			break;
		case LyudYard::EEvent::RoundEnd:
			if (bHeros && GS && HeroGi >= 0)
			{
				const LyudYard::FGame& G = Model->Games[(size_t)HeroGi];
				if (G.Seeker == LyudYard::HeroId)
				{
					// time up with some still hiding: the hero again if he found nobody, else the first he found
					GS->Toast(E.Other > 0 ? (E.Who == LyudYard::HeroId ? LyudText::Get(TEXT("yard.toast.seek_timeout"))
							: LyudText::Get(TEXT("yard.toast.seek_over"), { FStringFormatArg(Name(E.Who)) }))
						: LyudText::Get(TEXT("yard.toast.all_found"), { FStringFormatArg(Name(E.Who)) }));
				}
				else if (!bHeroFoundThisRound && G.Find(LyudYard::HeroId) && !G.Find(LyudYard::HeroId)->bPending)
				{
					GS->Toast(LyudText::Get(TEXT("yard.toast.not_found")));
				}
			}
			break;
		case LyudYard::EEvent::Tagged:
			SayLine(E.Other, TEXT("line.yard.tagged"));
			break;
		case LyudYard::EEvent::Goal:
			SayLine(E.Who, TEXT("line.yard.goal"));
			break;
		case LyudYard::EEvent::Miss:
			if (E.G == LyudYard::EGame::Hopscotch || E.G == LyudYard::EGame::Rope) SayLine(E.Who, TEXT("line.yard.miss.") + Game);
			break;
		case LyudYard::EEvent::Leave:
			if (E.Who != LyudYard::HeroId) SayLine(E.Who, TEXT("line.yard.end"));
			break;
		case LyudYard::EEvent::End:
			if (bHeros && GS && E.Other != 2)
			{
				GS->Toast(LyudText::Get(TEXT("yard.toast.over")));
			}
			if (bHeros)
			{
				HeroGameUid = -1;
				PromptByKey(nullptr);
			}
			break;
		default:
			break;
		}
	}
	Model->Events.clear();
}

void ULyudYardGames::UpdateHero(float DeltaTime)
{
	(void)DeltaTime;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ULyudGameScreen* GS = ULyudGameScreen::Get(this);
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!PC || !Model->bHeroHere)
	{
		PromptByKey(nullptr);
		return;
	}
	const int Gi = Model->HeroGame();
	if (Gi < 0)
	{
		// near a hide-and-seek game they could still join: ask
		HeroGameUid = -1;
		int Near = -1;
		for (int I = 0; I < (int)Model->Games.size(); I++)
		{
			const LyudYard::FGame& G = Model->Games[(size_t)I];
			const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
			if (G.Kind == LyudYard::EGame::Hide && (Model->HeroPos - A.C).Len() < A.R + 8.f)
			{
				Near = I;
			}
		}
		if (Near < 0)
		{
			PromptByKey(nullptr);
			return;
		}
		PromptByKey(TEXT("yard.prompt.join"));
		if (ANpcTalkDirector::UsePressed(PC) && Model->HeroJoin(Near))
		{
			const LyudYard::FGame& G = Model->Games[(size_t)Near];
			HeroGameUid = G.Uid;
			bHeroFoundThisRound = false;
			const int32 Who = G.Seeker >= 0 ? G.Seeker : G.Leader;
			const LyudYard::FKid* H = G.Find(LyudYard::HeroId);
			if (GS)
			{
				GS->Toast(H && H->bPending ? LyudText::Get(TEXT("yard.toast.wait_round"))
					: LyudText::Get(TEXT("yard.toast.joined"), { FStringFormatArg(Life && Who >= 0 ? Life->GetDisplayName(Who).ToString() : FString()) }));
			}
			SayLine(G.Leader, TEXT("line.yard.hero_join"));
			PromptByKey(nullptr);
		}
		return;
	}
	const LyudYard::FGame& G = Model->Games[(size_t)Gi];
	HeroGameUid = G.Uid;
	const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
	if ((Model->HeroPos - A.C).Len() > A.Catch + 12.f)
	{
		Model->HeroLeave();
		HeroGameUid = -1;
		if (GS) GS->Toast(LyudText::Get(TEXT("yard.toast.left")));
		PromptByKey(nullptr);
		return;
	}
	if (G.Seeker != LyudYard::HeroId || G.Count < -900.f)
	{
		bHeroWasCounting = false;
		PromptByKey(nullptr);
		return;
	}
	if (G.Count > 0.f)
	{
		bHeroWasCounting = true;
		PromptByKey(TEXT("yard.toast.counting"), FMath::CeilToInt(G.Count));
		return;
	}
	if (bHeroWasCounting)
	{
		bHeroWasCounting = false;
		if (GS) GS->Toast(LyudText::Get(TEXT("yard.toast.go")));
	}
	bool bNear = false;
	for (const LyudYard::FKid& K : G.Kids)
	{
		bNear = bNear || (K.Role == LyudYard::ERole::Hider && (K.Pos - Model->HeroPos).Len() < 2.6f);
	}
	PromptByKey(bNear ? TEXT("yard.prompt.find") : nullptr);
	if (bNear && ANpcTalkDirector::UsePressed(PC))
	{
		const int Found = Model->HeroFind();
		if (Found >= 0)
		{
			int Left = 0;
			const int Gj = Model->HeroGame();
			if (Gj >= 0)
			{
				for (const LyudYard::FKid& K : Model->Games[(size_t)Gj].Kids) Left += K.Role == LyudYard::ERole::Hider ? 1 : 0;
			}
			if (GS) GS->Toast(LyudText::Get(TEXT("yard.toast.you_found"), { FStringFormatArg(Life ? Life->GetDisplayName(Found).ToString() : FString()), FStringFormatArg(Left) }));
			SayLine(Found, TEXT("line.yard.found"));
		}
	}
}

void ULyudYardGames::Present(float DeltaTime)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!Life)
	{
		return;
	}
	const FVector Cam = CameraLocation();
	// the games to draw: the nearest few within reach of the camera
	Drawn.Reset();
	{
		TArray<TPair<double, int32>, TInlineAllocator<8>> Near;
		for (const LyudYard::FGame& G : Model->Games)
		{
			const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
			const double D = FVector::Dist2D(LyudYardDraw::ToUE(A.C, 0.f), Cam);
			if (D < LyudYardDraw::DrawCm)
			{
				Near.Add(TPair<double, int32>(D, G.Uid));
			}
		}
		Near.Sort([](const TPair<double, int32>& X, const TPair<double, int32>& Y) { return X.Key < Y.Key; });
		for (int32 I = 0; I < Near.Num() && I < LyudYardDraw::MaxDrawn; I++)
		{
			Drawn.Add(Near[I].Value);
		}
	}
	for (TPair<int32, FShown>& It : Shown)
	{
		It.Value.bSeen = false;
	}
	for (const LyudYard::FGame& G : Model->Games)
	{
		if (!Drawn.Contains(G.Uid))
		{
			continue;
		}
		for (const LyudYard::FKid& K : G.Kids)
		{
			if (K.Id < 0)
			{
				continue;
			}
			FShown* S = Shown.Find(K.Id);
			if (!S)
			{
				// (a body that could not be made is not spawned, set up and destroyed again every frame)
				if (NoBody.Contains(K.Id))
				{
					continue;
				}
				const FString ModelName = Life->ShownModel(K.Id);
				if (ModelName.IsEmpty())
				{
					continue;
				}
				FActorSpawnParameters SP;
				SP.ObjectFlags |= RF_Transient;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
				ANpcBody* Body = GetWorld()->SpawnActor<ANpcBody>(ANpcBody::StaticClass(), FTransform(FRotator(0.f, K.Yaw, 0.f), LyudYardDraw::ToUE(K.Pos, A.Z * 100.f)), SP);
				if (!Body || !Body->Setup(ModelName, FString()))
				{
					if (Body) Body->Destroy();
					NoBody.Add(K.Id);
					continue;
				}
				Life->SetPresentedExternally(K.Id, true);
				FShown New;
				New.Body = Body;
				S = &Shown.Add(K.Id, New);
				S->GroundZ = A.Z * 100.f;
				DriveKid(G, K, *S, DeltaTime);
				S->bSeen = true;
				continue;
			}
			S->Back = -1.f;
			S->bSeen = true;
			DriveKid(G, K, *S, DeltaTime);
		}
	}
	// whoever is no longer in a game drawn here walks back toward where their life has them, then is handed back
	TArray<int32, TInlineAllocator<16>> Done;
	for (TPair<int32, FShown>& It : Shown)
	{
		FShown& S = It.Value;
		if (S.bSeen)
		{
			continue;
		}
		ANpcBody* Body = S.Body.Get();
		int32 Act = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Hidden;
		FVector Where = FVector::ZeroVector;
		const bool bKnown = Life->GetActivityState(It.Key, Act, bDoing, Pose, Where);
		if (S.Back < 0.f)
		{
			S.Back = 4.f;
		}
		S.Back -= DeltaTime;
		const FVector2D To(Where.X - S.Feet.X, Where.Y - S.Feet.Y);
		if (!Body || !bKnown || Pose == EPersonPose::Hidden || S.Back <= 0.f || To.Size() < 100.0)
		{
			Done.Add(It.Key);
			continue;
		}
		const FVector2D Step = To.GetSafeNormal() * (160.0 * DeltaTime);
		S.Feet.X += Step.X;
		S.Feet.Y += Step.Y;
		S.Feet.Z = GroundAt(S, FVector2D(S.Feet.X, S.Feet.Y), S.Feet.Z);
		S.Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		Body->Drive(S.Feet, S.Yaw, EPersonPose::Walk, 0, 160.f, 0.f, DeltaTime, false);
	}
	for (int32 Id : Done)
	{
		Release(Id);
	}
}

void ULyudYardGames::DriveKid(const LyudYard::FGame& G, const LyudYard::FKid& K, FShown& S, float DeltaTime)
{
	ANpcBody* Body = S.Body.Get();
	if (!Body)
	{
		return;
	}
	const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
	const FVector2D At(K.Pos.X * 100.0, K.Pos.Y * 100.0);
	const float Z = GroundAt(S, At, A.Z * 100.f);
	S.Feet = FVector(At.X, At.Y, Z + K.Lift * 100.f);
	S.Yaw = K.Yaw;
	Body->Drive(S.Feet, K.Yaw, LyudYardDraw::PoseOf(K), 0, K.Speed * 100.f, 0.f, DeltaTime, false);
	// the gesture after Drive (which sets the manner's, none here): the hand goals for this act
	if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Body->Mesh->GetAnimInstance()))
	{
		Anim->SetGesture(LyudYardDraw::GestureOf(K, Body->GetBodyScale(), (uint32)K.Id * 2654435761u));
	}
}

void ULyudYardGames::DrawProps()
{
	ALyudYardThings* T = Things.Get();
	if (!T)
	{
		return;
	}
	int32 RopeUsed = 0, BallUsed = 0, PostUsed = 0;
	// (the rope's middle and the bags stay where they are: their ground is kept, Tick drops it once a second)
	RopeGround.SetNum(LyudYardDraw::MaxDrawn);
	PostGround.SetNum(T->Posts.Num());
	for (const LyudYard::FGame& G : Model->Games)
	{
		if (!Drawn.Contains(G.Uid) || G.Phase != LyudYard::EPhase::Play)
		{
			continue;
		}
		const LyudYard::FArea& A = Model->Areas[(size_t)G.Area];
		FShown Probe;
		if (G.Kind == LyudYard::EGame::Rope && RopeUsed + LyudYardDraw::RopeSegs <= T->Rope.Num() && G.NumIn(LyudYard::ERole::Turner) == 2)
		{
			const float Z0 = GroundAt(RopeGround[RopeUsed / LyudYardDraw::RopeSegs], FVector2D(A.Rope.X * 100.0, A.Rope.Y * 100.0), A.Z * 100.f);
			FVector Prev;
			for (int32 I = 0; I <= LyudYardDraw::RopeSegs; I++)
			{
				float X, Y, Z;
				LyudYard::RopePoint(A, G.Beat, (float)I / (float)LyudYardDraw::RopeSegs, X, Y, Z);
				const FVector P(X * 100.0, Y * 100.0, Z0 + FMath::Max(Z, 0.015f) * 100.0);
				if (I > 0)
				{
					UStaticMeshComponent* C = T->Rope[RopeUsed++];
					const FVector D = P - Prev;
					C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).ToQuat(), (P + Prev) * 0.5, FVector(0.012, 0.012, D.Size() / 100.0)));
					C->SetVisibility(true);
				}
				Prev = P;
			}
		}
		if (G.Kind == LyudYard::EGame::Football && BallUsed < T->Balls.Num())
		{
			const LyudYard::FBall& B = G.Ball;
			const float Z0 = GroundAt(Probe, FVector2D(B.Pos.X * 100.0, B.Pos.Y * 100.0), A.Z * 100.f);
			const FVector Axis = FVector(-B.Vel.Y, B.Vel.X, 0.0).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
			UStaticMeshComponent* C = T->Balls[BallUsed++];
			C->SetWorldTransform(FTransform(FQuat(Axis, B.Spin), LyudYardDraw::ToUE(B.Pos, Z0 + 10.5f + B.Z * 100.f), FVector(0.21)));
			C->SetVisibility(true);
			// the goals: two schoolbags a goal
			const LyudYard::FV2 F = LyudYard::FV2::Dir(A.FieldYaw);
			const LyudYard::FV2 L(-F.Y, F.X);
			for (int32 End = -1; End <= 1; End += 2)
			{
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					if (PostUsed >= T->Posts.Num()) break;
					const LyudYard::FV2 P = A.C + F * ((float)End * A.FieldHalfL) + L * ((float)Side * LyudYard::GoalHalf);
					const float Zb = GroundAt(PostGround[PostUsed], FVector2D(P.X * 100.0, P.Y * 100.0), A.Z * 100.f);
					UStaticMeshComponent* Bag = T->Posts[PostUsed++];
					Bag->SetWorldTransform(FTransform(FRotator(0.f, A.FieldYaw + 20.f * (float)Side, 0.f), LyudYardDraw::ToUE(P, Zb + 11.f), FVector(0.34, 0.24, 0.22)));
					Bag->SetVisibility(true);
				}
			}
		}
	}
	for (int32 I = RopeUsed; I < T->Rope.Num(); I++) if (T->Rope[I]->IsVisible()) T->Rope[I]->SetVisibility(false);
	for (int32 I = BallUsed; I < T->Balls.Num(); I++) if (T->Balls[I]->IsVisible()) T->Balls[I]->SetVisibility(false);
	for (int32 I = PostUsed; I < T->Posts.Num(); I++) if (T->Posts[I]->IsVisible()) T->Posts[I]->SetVisibility(false);
}

void ULyudYardGames::Release(int32 Id)
{
	if (FShown* S = Shown.Find(Id))
	{
		if (ANpcBody* Body = S->Body.Get())
		{
			Body->Destroy();
		}
		Shown.Remove(Id);
	}
	if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
	{
		Life->SetPresentedExternally(Id, false);
	}
}

int32 ULyudYardGames::NearestArea(const FVector& Where, uint32 GameMask) const
{
	int32 Best = -1;
	double BestD = 1e30;
	for (int32 I = 0; I < (int32)Model->Areas.size(); I++)
	{
		const LyudYard::FArea& A = Model->Areas[(size_t)I];
		if ((A.Games & GameMask) == 0 || Model->GameAt(I) >= 0)
		{
			continue;
		}
		const double D = FVector::Dist2D(LyudYardDraw::ToUE(A.C, 0.f), Where);
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	return Best;
}

TArray<LyudYard::FCandidate> ULyudYardGames::NearestChildren(const FVector& Where, int32 N) const
{
	TArray<LyudYard::FCandidate> Out;
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (!S)
	{
		return Out;
	}
	TArray<TPair<double, int32>> Kids;
	for (const LyudPeople::FPerson& P : S->People)
	{
		if (P.Age >= 5 && P.Age <= 13 && Model->InGame(P.Id) < 0 && !Life->ShownModel(P.Id).IsEmpty())
		{
			Kids.Add(TPair<double, int32>(FVector::Dist2D(FVector(P.Pos.X * 100.0, P.Pos.Y * 100.0, 0.0), Where), P.Id));
		}
	}
	Kids.Sort([](const TPair<double, int32>& X, const TPair<double, int32>& Y) { return X.Key < Y.Key; });
	for (int32 I = 0; I < Kids.Num() && Out.Num() < N; I++)
	{
		LyudYard::FCandidate C;
		C.Id = Kids[I].Value;
		C.Age = S->People[(size_t)C.Id].Age;
		Out.Add(C);
	}
	return Out;
}

bool ULyudYardGames::StartNearHero(const FString& Kind, int32 Players)
{
	if (!Ensure())
	{
		return false;
	}
	const LyudYard::EGame G = LyudYard::GameFromName(TCHAR_TO_UTF8(*Kind));
	const uint32 Mask = G == LyudYard::EGame::Num ? 0xffu : (1u << (int)G);
	const int32 Area = NearestArea(HeroLocation(), Mask);
	if (Area < 0)
	{
		return false;
	}
	const LyudYard::FArea& A = Model->Areas[(size_t)Area];
	LyudYard::EGame Pick = G;
	if (Pick == LyudYard::EGame::Num)
	{
		for (int32 I = 0; I < (int32)LyudYard::EGame::Num; I++) if (A.Fits((LyudYard::EGame)I)) Pick = (LyudYard::EGame)I;
	}
	TArray<LyudYard::FCandidate> Kids = NearestChildren(LyudYardDraw::ToUE(A.C, 0.f), FMath::Max(Players, LyudYard::FYard::MinPlayers(Pick)));
	// they come running from round the area's edge
	for (int32 I = 0; I < Kids.Num(); I++)
	{
		Kids[I].Pos = A.C + LyudYard::FV2::Dir(A.Yaw + 360.f * (float)I / (float)FMath::Max(1, Kids.Num())) * (A.R + 3.f);
	}
	const std::vector<LyudYard::FCandidate> List(Kids.GetData(), Kids.GetData() + Kids.Num());
	const bool bOk = Model->Start(Area, Pick, List, -1.f, true);
	UE_LOG(LogTemp, Display, TEXT("YARD start %s at %s with %d children: %s"), UTF8_TO_TCHAR(LyudYard::GameName(Pick)), UTF8_TO_TCHAR(A.Id.c_str()), Kids.Num(), bOk ? TEXT("ok") : TEXT("no"));
	return bOk;
}

int32 ULyudYardGames::StartThreeNearHero()
{
	if (!Ensure())
	{
		return 0;
	}
	int32 N = 0;
	static const TCHAR* const Kinds[] = { TEXT("hide"), TEXT("football"), TEXT("rope"), TEXT("tag"), TEXT("hopscotch") };
	static const int32 Players[] = { 6, 8, 5, 6, 4 };
	for (int32 K = 0; K < 5 && N < 3; K++)
	{
		if (StartNearHero(Kinds[K], Players[K]))
		{
			N++;
		}
	}
	return N;
}

bool ULyudYardGames::HeroSeekNear()
{
	if (!Ensure())
	{
		return false;
	}
	int32 Gi = -1;
	for (int32 I = 0; I < (int32)Model->Games.size(); I++) if (Model->Games[(size_t)I].Kind == LyudYard::EGame::Hide) Gi = I;
	if (Gi < 0 && StartNearHero(TEXT("hide"), 5))
	{
		Gi = (int32)Model->Games.size() - 1;
	}
	return Gi >= 0 && Model->HeroSeek(Gi);
}

void ULyudYardGames::EndAll()
{
	if (Model.IsValid())
	{
		Model->EndAll();
	}
}

int32 ULyudYardGames::NumGames() const
{
	return Model.IsValid() ? (int32)Model->Games.size() : 0;
}

FString ULyudYardGames::Report() const
{
	if (!Model.IsValid())
	{
		return TEXT("YARD not running");
	}
	FString Out = FString::Printf(TEXT("YARD %d games, %d drawn, %d bodies"), (int32)Model->Games.size(), Drawn.Num(), Shown.Num());
	for (const LyudYard::FGame& G : Model->Games)
	{
		Out += FString::Printf(TEXT("\n  %s at %s: %d children, %s, %.0f of %.0f s%s"), UTF8_TO_TCHAR(LyudYard::GameName(G.Kind)),
			UTF8_TO_TCHAR(Model->Areas[(size_t)G.Area].Id.c_str()), (int32)G.Kids.size(), G.Phase == LyudYard::EPhase::Play ? TEXT("playing") : TEXT("gathering"),
			G.T, G.Length, G.bForced ? TEXT(" (forced)") : TEXT(""));
	}
	return Out;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GYardStart(TEXT("lyud.Yard.Start"), TEXT("lyud.Yard.Start [hopscotch|rope|hide|football|tag] [players]: a game at the play area nearest the hero"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->StartNearHero(Args.Num() > 0 ? Args[0] : FString(), Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 6);
		}));
	FAutoConsoleCommandWithWorld GYardThree(TEXT("lyud.Yard.Three"), TEXT("lyud.Yard.Three: three games by the hero (the bench's three groups)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->StartThreeNearHero(); }));
	FAutoConsoleCommandWithWorld GYardReport(TEXT("lyud.Yard.Report"), TEXT("lyud.Yard.Report: the games in the log (YARD ...)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *Y->Report()); }));

	LyudDev::FAutoAction GDevYardHide(TEXT("yard.start_hide"), TEXT("yard"), TEXT("dev.action.yard.start_hide"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->StartNearHero(TEXT("hide"), 6); });
	LyudDev::FAutoAction GDevYardFootball(TEXT("yard.start_football"), TEXT("yard"), TEXT("dev.action.yard.start_football"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->StartNearHero(TEXT("football"), 8); });
	LyudDev::FAutoAction GDevYardThree(TEXT("yard.start_three"), TEXT("yard"), TEXT("dev.action.yard.start_three"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->StartThreeNearHero(); });
	LyudDev::FAutoAction GDevYardSeek(TEXT("yard.hero_seek"), TEXT("yard"), TEXT("dev.action.yard.hero_seek"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->HeroSeekNear(); });
	LyudDev::FAutoAction GDevYardEnd(TEXT("yard.end_all"), TEXT("yard"), TEXT("dev.action.yard.end_all"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) Y->EndAll(); });
	LyudDev::FAutoAction GDevYardReport(TEXT("yard.report"), TEXT("yard"), TEXT("dev.action.yard.report"),
		[](UWorld* W) { if (ULyudYardGames* Y = ULyudYardGames::Get(W)) UE_LOG(LogTemp, Display, TEXT("%s"), *Y->Report()); });
}
