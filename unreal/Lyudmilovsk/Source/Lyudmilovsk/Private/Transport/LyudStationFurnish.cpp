// The stations' furnishing (Dima, 2026-10-26: the interiors are bare): the hall's furniture from vestibules.json
// "furniture" (blender/transport/vestibules.py lays it out round the hall's wall; the meshes are
// blender/transport/furniture.py's SM_TR_Furn_*), live plants in its planters (AProceduralPlant, the vegetation rule),
// posters (design/transport/posters, /Game/Transport/Posters), lamps; posters and sconces up the tower's stair; the lift's
// new cab (ALyudStationLift::Setup). docs/transport.md "Furnishing".

#include "Transport/LyudCableway.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/ProceduralPlant.h"
#include "Audio/LyudSoundscape.h"
#include "LyudText.h"
#include "EngineUtils.h"
#include "TransportKit.h"

using namespace LyudTransportKit;

namespace
{
	const float PlanterTop = 50.f;          // furniture.py PLANTER_TOP

	// as LyudStationBuild.cpp's: side K of the tower (core frame) and the corner after it
	FVector SideOut(int32 K) { return FRotator(0.f, (K & 3) * 90.f, 0.f).Vector(); }
	FVector CornerAt(int32 K, float R, float Z) { return (SideOut(K) + SideOut(K + 1)) * R + FVector(0, 0, Z); }

	FVector JsonVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		FVector V = FVector::ZeroVector;
		if (O->TryGetArrayField(Key, A))
		{
			for (int32 I = 0; I < A->Num() && I < 3; I++)
			{
				V[I] = (*A)[I]->AsNumber();
			}
		}
		return V;
	}
}

UMaterialInterface* ALyudCableway::PosterMat(int32 K)
{
	const FName Key(*FString::Printf(TEXT("poster_%d"), K));
	if (const TObjectPtr<UMaterialInterface>* Found = Mats.Find(Key))
	{
		return Found->Get();
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Poster.M_TR_Poster"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	const FString TexName = FString::Printf(TEXT("T_TR_Poster_%d"), K);
	UTexture* Tex = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Transport/Posters/%s.%s"), *TexName, *TexName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Out = nullptr;
	if (Base && Tex)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetTextureParameterValue(TEXT("Poster"), Tex);
		Out = M;
	}
	else
	{
		Out = Mat(TEXT("tx_enamel"));
	}
	Mats.Add(Key, Out);
	return Out;
}

UStaticMeshComponent* ALyudCableway::Furniture(const FString& Item, const FTransform& At, int32 Poster)
{
	UStaticMesh* M = LoadMesh(TEXT("SM_TR_Furn_") + Item);
	if (!M)
	{
		return nullptr;
	}
	const bool bCollide = Item != TEXT("Rug") && Item != TEXT("Poster") && Item != TEXT("Sconce");
	UStaticMeshComponent* C = Single(M, At, bCollide);
	const TArray<FName> Slots = C->GetMaterialSlotNames();
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		if (Slots[I].ToString().StartsWith(TEXT("TR_Poster")))
		{
			C->SetMaterial(I, PosterMat(FMath::Max(0, Poster) % 6));
		}
	}
	return C;
}

void ALyudCableway::Lamp(const FVector& At, float Intensity, float Radius)
{
	UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
	Lt->SetupAttachment(RootComponent);
	Lt->RegisterComponent();
	Lt->SetWorldLocation(At);
	Lt->SetIntensity(Intensity);
	Lt->SetAttenuationRadius(Radius);
	Lt->SetLightColor(FLinearColor(1.f, 0.8f, 0.58f));
	Lt->SetCastShadows(false);
	AddInstanceComponent(Lt);
}

void ALyudCableway::Plant(const FVector& At, FName Species, int32 Seed, float Age)
{
	if (AProceduralPlant* P = GetWorld()->SpawnActorDeferred<AProceduralPlant>(AProceduralPlant::StaticClass(), FTransform(At), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		P->Species = Species;
		P->Seed = Seed;
		P->AgeAtStart = Age;
		P->bDemoGrowth = false;
		P->bSnapToGround = false;
		P->Pot = EPlantPot::None;          // it grows in our planter
		P->FinishSpawning(FTransform(At));
	}
}

void ALyudCableway::FurnishVestibule(int32 S, const TSharedPtr<FJsonObject>& V, const FTransform& C)
{
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!V.IsValid() || !V->TryGetArrayField(TEXT("furniture"), Items))
	{
		return;
	}
	// the room's sound: a quiet hall (murmur, a clock), from a point under its ceiling
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		USceneComponent* Spot = NewObject<USceneComponent>(this);
		Spot->SetupAttachment(RootComponent);
		Spot->RegisterComponent();
		Spot->SetWorldLocation(C.TransformPosition(JsonVec(V->GetObjectField(TEXT("room")), TEXT("centre")) + FVector(0, 0, 220.f)));
		AddInstanceComponent(Spot);
		Sound->AttachLoop(TEXT("station_hall"), Spot, 0.5f);
	}
	int32 N = 0;
	for (const TSharedPtr<FJsonValue>& Iv : *Items)
	{
		const TSharedPtr<FJsonObject> O = Iv->AsObject();
		const FString Item = O->GetStringField(TEXT("item"));
		const FTransform Local(FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f), JsonVec(O, TEXT("at")));
		const FTransform At = Local * C;
		if (Item == TEXT("EggPair"))
		{
			// two egg chairs turned a little toward each other with a tulip table between them, a rug under
			Furniture(TEXT("Rug"), FTransform(FVector(80.f, 0.f, 0.3f)) * At);
			Furniture(TEXT("EggChair"), FTransform(FRotator(0.f, 18.f, 0.f), FVector(0.f, -85.f, 0.f)) * At);
			Furniture(TEXT("EggChair"), FTransform(FRotator(0.f, -18.f, 0.f), FVector(0.f, 85.f, 0.f)) * At);
			Furniture(TEXT("Table"), FTransform(FVector(10.f, 0.f, 0.f)) * At);
		}
		else
		{
			Furniture(Item, At, O->HasField(TEXT("poster")) ? (int32)O->GetNumberField(TEXT("poster")) : 0);
		}
		if (Item == TEXT("Planter"))
		{
			FString Sp = TEXT("Monstera");
			O->TryGetStringField(TEXT("plant"), Sp);
			Plant(At.TransformPosition(FVector(0.f, 0.f, PlanterTop - 6.f)), FName(*Sp), 900 + S * 13 + N, 12.f);
		}
		else if (Item == TEXT("Lamp"))
		{
			Lamp(At.TransformPosition(FVector(0.f, 0.f, 175.f)), 1800.f, 600.f);
		}
		N++;
	}
}

void ALyudCableway::DressCore(int32 S, const TSharedPtr<FJsonObject>& V)
{
	// up the stair, on the tower's wall beyond its outer rail: a poster on every flight's side, a sconce at every turn
	const FLyudBuiltStation& B = Built[S];
	if (B.Stops.Num() < 2)
	{
		return;
	}
	float WallR = LaneOut + 6.f;          // the core's own wall (no kit)
	bool bRound = false;
	const TSharedPtr<FJsonObject>* T = nullptr;
	if (V.IsValid() && V->TryGetObjectField(TEXT("tower"), T))
	{
		const TArray<TSharedPtr<FJsonValue>>* Cl = nullptr;
		if ((*T)->TryGetArrayField(TEXT("clear"), Cl) && Cl->Num())
		{
			const float Sq = (*Cl)[0]->AsNumber() * 0.5f;
			bRound = Sq > 300.f;
			WallR = bRound ? 372.f : Sq;     // towers.py: a 7.44 m circle, or the square
		}
	}
	const float Top = B.Stops.Last();
	int32 K = S * 3;
	for (float Z = 160.f; Z < Top - 100.f; Z += 450.f, K++)
	{
		const int32 Sd = K & 3;
		const FVector Out = SideOut(Sd);
		const FRotator Face(0.f, Sd * 90.f + 180.f, 0.f);
		const FVector P = Out * (WallR - 8.f) + FVector(0, 0, Z + 150.f);
		Furniture(TEXT("Poster"), FTransform(Face, P) * B.Core, K);
		const FVector Cn = CornerAt(Sd, (bRound ? WallR * 0.7071f : WallR) - 14.f, Z + 230.f);
		Furniture(TEXT("Sconce"), FTransform(FRotator(0.f, Sd * 90.f + 45.f + 180.f, 0.f), Cn) * B.Core);
		Lamp(B.Core.TransformPosition(CornerAt(Sd, (bRound ? WallR * 0.7071f : WallR) - 60.f, Z + 220.f)), 2600.f, 650.f);
	}
}

void ALyudCableway::UpdateHallSound(const FVector& Viewer, float DeltaTime)
{
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	if (!Sound)
	{
		return;
	}
	HallTimer.SetNum(Built.Num());
	CabinNear.SetNum(Built.Num());
	for (int32 S = 0; S < Built.Num(); S++)
	{
		const FLyudBuiltStation& B = Built[S];
		if (B.Room.Equals(FTransform::Identity))
		{
			continue;
		}
		const FVector Room = B.Room.GetLocation();
		const bool bIn = FVector::Dist2D(Viewer, Room) < 900.f && FMath::Abs(Viewer.Z - Room.Z) < 450.f;
		// a cabin comes in: within 90 m of the station and getting nearer (once until it has gone again)
		bool bNear = false;
		for (TActorIterator<ALyudCabin> It(GetWorld()); It; ++It)
		{
			bNear |= FVector::Dist(It->GetActorLocation(), StationCentres[S]) < 9000.f;
		}
		const bool bComing = bNear && !CabinNear[S];
		CabinNear[S] = bNear;
		HallTimer[S] -= DeltaTime;
		if (!bIn)
		{
			HallTimer[S] = FMath::Min(HallTimer[S], 25.f);
			continue;
		}
		const FVector Speaker = Room + FVector(0, 0, 150.f);
		if (bComing)
		{
			Sound->PlayEffect(TEXT("station_chime"), Speaker, 0.6f);
			Sound->PlayVoiceAt(LyudText::LoadVoice(TEXT("line.transport.hall_cabin.1")), Speaker, 0.85f);
			HallTimer[S] = FMath::Max(HallTimer[S], 40.f);
		}
		else if (HallTimer[S] <= 0.f)
		{
			Sound->PlayEffect(TEXT("station_chime"), Speaker, 0.5f);
			Sound->PlayVoiceAt(LyudText::LoadVoice(HallLine++ % 2 ? TEXT("line.transport.hall_lift.1") : TEXT("line.transport.hall_welcome.1")), Speaker, 0.8f);
			HallTimer[S] = 150.f;
		}
	}
}
