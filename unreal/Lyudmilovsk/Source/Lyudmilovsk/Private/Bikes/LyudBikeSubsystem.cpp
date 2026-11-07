#include "Bikes/LyudBikeSubsystem.h"

#include "Bikes/HeroBike.h"
#include "Bikes/LyudBike.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../People/PeopleSim.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBikes(TEXT("lyud.Bikes"), 1, TEXT("0: the bicycles are hidden and nothing of theirs runs (live, for A/B benchmarks); 0 at the level's start puts none out"));
	TAutoConsoleVariable<int32> CVarBikesAnyMap(TEXT("lyud.Bikes.AnyMap"), 0, TEXT("1: bicycles in any level, not only Lvl_Scene01 (tests)"));
	TAutoConsoleVariable<int32> CVarBikesLog(TEXT("lyud.Bikes.Log"), 0, TEXT("1: residents' bikes log what they do"));

	namespace LPB = LyudPeople;

	FVector BikePeopleToUE(const LPB::V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	LPB::V3 BikePeopleFromUE(const FVector& V) { return LPB::V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }

	FVector BikeJsonVec(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return FVector(A.Num() > 0 ? A[0]->AsNumber() : 0.0, A.Num() > 1 ? A[1]->AsNumber() : 0.0, A.Num() > 2 ? A[2]->AsNumber() : 0.0);
	}
}

ULyudBikeSubsystem* ULyudBikeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudBikeSubsystem>() : nullptr;
}

bool ULyudBikeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId ULyudBikeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBikeSubsystem, STATGROUP_Tickables);
}

void ULyudBikeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bStarted = true;
	const bool bScene = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01"));
	if (CVarBikes.GetValueOnGameThread() != 0 && (bScene || CVarBikesAnyMap.GetValueOnGameThread() != 0))
	{
		SpawnFromData();
	}
}

bool ULyudBikeSubsystem::TraceGround(const FVector& At, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BikePlace), false);
	for (const ALyudBike* B : Bikes)
	{
		Q.AddIgnoredActor(B);
	}
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->GetPawn())
	{
		Q.AddIgnoredActor(PC->GetPawn());
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 150.f), At - FVector(0.f, 0.f, 400.f), ECC_Visibility, Q))
	{
		OutZ = (float)Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool ULyudBikeSubsystem::PlaceInLobby(const FVector& Street, const FVector& Lobby, FVector& OutGround, float& OutYaw, float& OutLean) const
{
	// along the way in, against the nearer side wall of the lobby, leaning on it
	const FVector Dir = (Lobby - Street).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return false;
	}
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeLobby), false);
	const FVector Mid = Lobby + FVector(0.f, 0.f, 60.f);
	float Side[2] = { 250.f, 250.f };     // to the wall on the way's right, left
	for (int32 I = 0; I < 2; I++)
	{
		FHitResult Hit;
		const FVector To = Mid + Right * (I == 0 ? 250.f : -250.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Mid, To, ECC_Visibility, Q))
		{
			Side[I] = (float)Hit.Distance;
		}
	}
	const int32 Near = Side[0] <= Side[1] ? 0 : 1;
	// the bars are ~21 cm a side, the pedals 17: the bike's middle 26 cm off the wall
	const float Off = FMath::Max(0.f, Side[Near] - 26.f);
	const FVector Center = Lobby + Right * (Near == 0 ? Off : -Off);
	OutYaw = (float)Dir.Rotation().Yaw;
	// the bike's ground point is behind its middle (the frame runs -75 .. +95 cm)
	OutGround = Center - Dir * 10.f;
	float Z = (float)Lobby.Z;
	if (TraceGround(OutGround, Z))
	{
		OutGround.Z = Z;
	}
	// leaning on the wall: toward its right (+) when the wall is on the way's right
	OutLean = Near == 0 ? 4.f : -4.f;
	const float Passage = Side[0] + Side[1] - 26.f - 22.f;
	UE_LOG(LogTemp, Log, TEXT("Bikes: lobby walls %.0f / %.0f cm, %.0f cm left to walk past"), Side[0], Side[1], Passage);
	return true;
}

void ULyudBikeSubsystem::SpawnFromData()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Bikes/bikes.json"))))
	{
		UE_LOG(LogTemp, Warning, TEXT("Bikes: Content/Bikes/bikes.json missing"));
		return;
	}
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Bikes: bikes.json unreadable"));
		return;
	}
	TArray<FLinearColor> Paints;
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("paints")))
	{
		const FVector C = BikeJsonVec(V->AsArray());
		Paints.Add(FLinearColor(FColor((uint8)C.X, (uint8)C.Y, (uint8)C.Z)));
	}
	if (Paints.Num() == 0)
	{
		Paints.Add(FLinearColor(0.42f, 0.03f, 0.03f));
	}
	auto PaintOf = [&](const TSharedPtr<FJsonObject>& O) { return Paints[FMath::Clamp((int32)O->GetNumberField(TEXT("paint")), 0, Paints.Num() - 1)]; };
	// free-standing bikes: "at" is [x, y] in Unreal metres, the ground found under it
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("bikes")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FVector At = BikeJsonVec(O->GetArrayField(TEXT("at"))) * 100.0;
		// the ground: the highest level surface under the point, from high above (the valley is ~36 m below the club)
		float Z = 0.f;
		bool bGround = false;
		{
			TArray<FHitResult> Hits;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(BikeSpawn), false);
			GetWorld()->LineTraceMultiByChannel(Hits, FVector(At.X, At.Y, 30000.f), FVector(At.X, At.Y, -30000.f), ECC_Visibility, Q);
			for (const FHitResult& H : Hits)
			{
				if (H.ImpactNormal.Z > 0.75f && (!bGround || H.ImpactPoint.Z > Z))
				{
					Z = (float)H.ImpactPoint.Z;
					bGround = true;
				}
			}
			if (!bGround)
			{
				FHitResult H;
				if (GetWorld()->LineTraceSingleByChannel(H, FVector(At.X, At.Y, 30000.f), FVector(At.X, At.Y, -30000.f), ECC_Visibility, Q))
				{
					Z = (float)H.ImpactPoint.Z;
					bGround = true;
				}
			}
		}
		if (!bGround)
		{
			UE_LOG(LogTemp, Warning, TEXT("Bikes: no ground under %s"), *O->GetStringField(TEXT("id")));
			continue;
		}
		At.Z = Z;
		double Lean = 0.0;
		O->TryGetNumberField(TEXT("lean"), Lean);
		SpawnBike(FName(*O->GetStringField(TEXT("id"))), At, (float)O->GetNumberField(TEXT("yaw")), (float)Lean, PaintOf(O));
		SimIndex.Add(-1);
		Homes.Add(FHome());
	}
	// the entrances' bikes: in the lobby of each stair (people_places.json: the stair's way in from the street)
	FString Places;
	TSharedPtr<FJsonObject> PJ;
	if (!FFileHelper::LoadFileToString(Places, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("PeopleData/people_places.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Places), PJ) || !PJ.IsValid())
	{
		return;
	}
	TMap<FString, TPair<FVector, FVector>> Lobbies;
	for (const TSharedPtr<FJsonValue>& V : PJ->GetArrayField(TEXT("stairs")))
	{
		const TSharedPtr<FJsonObject> S = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Street = nullptr;
		const TArray<TSharedPtr<FJsonValue>>& Floors = S->GetArrayField(TEXT("floors"));
		if (!S->TryGetArrayField(TEXT("street"), Street) || Floors.Num() == 0)
		{
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>& Path = Floors[0]->AsObject()->GetArrayField(TEXT("path"));
		if (Path.Num() >= 4)
		{
			Lobbies.Add(S->GetStringField(TEXT("id")), { BikeJsonVec(*Street) * 100.0, BikeJsonVec(Path[3]->AsArray()) * 100.0 });
		}
	}
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("entrances")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const FString Stair = O->GetStringField(TEXT("stair"));
		const TPair<FVector, FVector>* L = Lobbies.Find(Stair);
		FHome H;
		if (!L || !PlaceInLobby(L->Key, L->Value, H.Ground, H.Yaw, H.Lean))
		{
			UE_LOG(LogTemp, Warning, TEXT("Bikes: no lobby for the stair %s"), *Stair);
			continue;
		}
		H.bValid = true;
		SpawnBike(FName(*Stair), H.Ground, H.Yaw, H.Lean, PaintOf(O));
		SimIndex.Add(-1);
		Homes.Add(H);
	}
	UE_LOG(LogTemp, Display, TEXT("Bikes: %d bicycles out"), Bikes.Num());
}

ALyudBike* ULyudBikeSubsystem::SpawnBike(FName Id, const FVector& Ground, float Yaw, float Lean, const FLinearColor& Paint)
{
	FActorSpawnParameters P;
	// a stable name: the save keys actors by it
	P.Name = FName(*FString::Printf(TEXT("LyudBike_%s"), *Id.ToString().Replace(TEXT("."), TEXT("_"))));
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	ALyudBike* B = GetWorld()->SpawnActor<ALyudBike>(ALyudBike::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Ground), P);
	if (!B)
	{
		return nullptr;
	}
	B->Setup(Id, Paint);
	B->Park(Ground, Yaw, Lean);
	Bikes.Add(B);
	return B;
}

bool ULyudBikeSubsystem::IsFree(const ALyudBike* Bike) const
{
	if (!Bike || Bike->IsTaken())
	{
		return false;
	}
	const int32 I = Bikes.IndexOfByKey(Bike);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
	if (S && I != INDEX_NONE && SimIndex.IsValidIndex(I) && SimIndex[I] >= 0 && SimIndex[I] < (int32)S->Bikes.size())
	{
		return S->Bikes[SimIndex[I]].Rider < 0;
	}
	return true;
}

ALyudBike* ULyudBikeSubsystem::Nearest(const FVector& At, float MaxDist) const
{
	ALyudBike* Best = nullptr;
	float BestD = MaxDist;
	for (ALyudBike* B : Bikes)
	{
		const float D = B ? (float)FVector::Dist(B->GetActorLocation(), At) : BestD;
		if (B && D < BestD && IsFree(B))
		{
			Best = B;
			BestD = D;
		}
	}
	return Best;
}

ALyudBike* ULyudBikeSubsystem::FindBike(FName Id) const
{
	for (ALyudBike* B : Bikes)
	{
		if (B && B->BikeId == Id)
		{
			return B;
		}
	}
	return nullptr;
}

void ULyudBikeSubsystem::HeroHas(ALyudBike* Bike, bool bHas)
{
	const int32 I = Bikes.IndexOfByKey(Bike);
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
	if (S && I != INDEX_NONE && SimIndex[I] >= 0)
	{
		S->SetBikeHeld(SimIndex[I], bHas, BikePeopleFromUE(Bike->ParkedAt), Bike->ParkedYaw);
		// back in its own place in the entrance: it lives there again
		if (!bHas && Homes[I].bValid && FVector::Dist2D(Bike->ParkedAt, Homes[I].Ground) < 80.f)
		{
			LPB::FWorld::FBike& SB = S->Bikes[SimIndex[I]];
			SB.Pos = SB.Home;
			SB.Yaw = SB.HomeYaw;
			Bike->Park(Homes[I].Ground, Homes[I].Yaw, Homes[I].Lean);
			Bike->bMoved = false;
		}
	}
}

void ULyudBikeSubsystem::BindPeople()
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
	if (!S || bSimBound)
	{
		return;
	}
	bSimBound = true;
	S->Bikes.clear();
	int32 N = 0;
	for (int32 I = 0; I < Bikes.Num(); I++)
	{
		if (!Bikes[I] || !Homes[I].bValid)
		{
			continue;
		}
		const FString Stair = Bikes[I]->BikeId.ToString();
		for (const LPB::FWorld::FLobby& L : S->Lobbies)
		{
			if (Stair == UTF8_TO_TCHAR(L.Stair.c_str()))
			{
				SimIndex[I] = S->AddBike(TCHAR_TO_UTF8(*Stair), L.Stair, L.Lobby, Homes[I].Yaw);
				N++;
				// a bike the hero left somewhere else (a loaded game) stands there for its owner too
				if (Bikes[I]->bMoved)
				{
					S->SetBikeHeld(SimIndex[I], false, BikePeopleFromUE(Bikes[I]->ParkedAt), Bikes[I]->ParkedYaw);
				}
			}
		}
	}
	S->AssignBikes();
	for (const LPB::FWorld::FBike& B : S->Bikes)
	{
		UE_LOG(LogTemp, Display, TEXT("Bikes: %hs belongs to %hs"), B.Id.c_str(), B.Owner.empty() ? "nobody" : B.Owner.c_str());
	}
	UE_LOG(LogTemp, Display, TEXT("Bikes: %d bikes in the people's life"), N);
}

void ULyudBikeSubsystem::SyncParked()
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
	if (!S)
	{
		return;
	}
	for (int32 I = 0; I < Bikes.Num(); I++)
	{
		ALyudBike* B = Bikes[I];
		const int32 Si = SimIndex[I];
		if (!B || Si < 0 || Si >= (int32)S->Bikes.size() || (B->IsTaken() && B->GetHolder() && B->GetHolder()->IsA<APawn>()))
		{
			continue;
		}
		const LPB::FWorld::FBike& SB = S->Bikes[Si];
		const LPB::FPerson* Rider = SB.Rider >= 0 ? &S->People[SB.Rider] : nullptr;
		const bool bInHand = Rider && Rider->Bike == Si && (Rider->BikeMode > 0 || Rider->BikeAct == LPB::EBikeAct::Take || Rider->BikeAct == LPB::EBikeAct::TakeMount);
		if (bInHand)
		{
			B->bMoved = false;
			// shown with its rider (PresentPerson), hidden while they are out of sight
			const bool bWithRider = Presented.Contains(SB.Rider);
			B->SetActorHiddenInGame(!bWithRider);
			if (!bWithRider)
			{
				B->SetHolder(nullptr);
				B->Hush();
			}
			continue;
		}
		B->SetHolder(nullptr);
		B->SetActorHiddenInGame(false);
		if (B->bMoved)
		{
			continue;           // where the hero left it (the life knows: HeroHas)
		}
		// standing where the life has it: its place in the entrance, or wherever its rider left it
		const bool bHome = (SB.Pos - SB.Home).Len() < 0.05f && Homes[I].bValid;
		const FVector Want = bHome ? Homes[I].Ground : BikePeopleToUE(SB.Pos);
		if (FVector::Dist2D(B->ParkedAt, Want) > 5.f)
		{
			FVector G = Want;
			float Z = 0.f;
			if (!bHome && TraceGround(G, Z))
			{
				G.Z = Z;
			}
			B->Park(G, bHome ? Homes[I].Yaw : SB.Yaw, bHome ? Homes[I].Lean : 0.f);
		}
	}
	Presented.Reset();
}

void ULyudBikeSubsystem::Tick(float DeltaTime)
{
	const bool bOn = CVarBikes.GetValueOnGameThread() != 0;
	if (bOn != bShown)
	{
		bShown = bOn;
		for (ALyudBike* B : Bikes)
		{
			if (B)
			{
				B->SetActorHiddenInGame(!bOn);
				B->Hush();
			}
		}
	}
	if (!bOn)
	{
		return;
	}
	// the hero's own handling of bikes
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (AHeroCharacter* Hero = Cast<AHeroCharacter>(PC->GetPawn()); Hero && !UHeroBikeComponent::Get(Hero) && Bikes.Num() > 0)
		{
			UHeroBikeComponent* C = NewObject<UHeroBikeComponent>(Hero, TEXT("HeroBike"));
			C->RegisterComponent();
			Hero->AddInstanceComponent(C);
		}
	}
	BindPeople();
	AssignTimer -= DeltaTime;
	if (bSimBound && AssignTimer <= 0.f)
	{
		AssignTimer = 60.f;
		if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this); Life && Life->IsRunning())
		{
			Life->GetSim()->AssignBikes();
		}
	}
	SyncParked();
	FollowCamera();
}

void ULyudBikeSubsystem::PresentPerson(int32 PersonId, ANpcBody* Body, float DeltaTime)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
	if (!S || !Body || PersonId < 0 || PersonId >= (int32)S->People.size())
	{
		return;
	}
	const LPB::FPerson& P = S->People[PersonId];
	const int32 I = SimIndex.IndexOfByKey(P.Bike);
	ALyudBike* B = P.Bike >= 0 && I != INDEX_NONE ? Bikes[I].Get() : nullptr;
	if (!B)
	{
		Body->DriveBike(nullptr, FLyudBikeRide(), FVector::ZeroVector, 0.f, 0.f, DeltaTime);
		return;
	}
	Presented.Add(PersonId);
	// what they do with it now: a stop (taking, getting on or off, parking) runs its course over the stop's seconds
	const float T = P.BikeAct != LPB::EBikeAct::None && P.BikeActUntil > P.BikeActStart
		? FMath::Clamp((float)((S->Time - P.BikeActStart) / (P.BikeActUntil - P.BikeActStart)), 0.f, 1.f) : 1.f;
	FLyudBikeRide Ride;
	Ride.Scale = Body->GetBodyScale();
	switch (P.BikeAct)
	{
	case LPB::EBikeAct::Take: Ride.Act = ELyudBikeAct::Lead; Ride.Weight = FMath::SmoothStep(0.f, 0.6f, T); break;
	case LPB::EBikeAct::TakeMount:
		Ride.Act = T < 0.45f ? ELyudBikeAct::Lead : ELyudBikeAct::Mount;
		Ride.Phase = FMath::Clamp((T - 0.45f) / 0.55f, 0.f, 1.f);
		Ride.Weight = FMath::SmoothStep(0.f, 0.3f, T);
		break;
	case LPB::EBikeAct::Mount: Ride.Act = ELyudBikeAct::Mount; Ride.Phase = T; break;
	case LPB::EBikeAct::Dismount: Ride.Act = ELyudBikeAct::Dismount; Ride.Phase = T; break;
	case LPB::EBikeAct::Park: Ride.Act = ELyudBikeAct::Lead; Ride.Weight = 1.f - FMath::SmoothStep(0.3f, 1.f, T); break;
	default:
		Ride.Act = P.BikeMode == 2 ? ELyudBikeAct::Ride : (P.BikeMode == 1 ? ELyudBikeAct::Lead : ELyudBikeAct::None);
		break;
	}
	if (Ride.Act == ELyudBikeAct::None)
	{
		// on their way to it: it stands where it is
		Body->DriveBike(nullptr, FLyudBikeRide(), FVector::ZeroVector, 0.f, 0.f, DeltaTime);
		return;
	}
	// the body's ground point (PeopleLife traced it) and heading, eased; the lean from the turn
	const FVector Feet = BikePeopleToUE(P.Pos);
	FVector At = Feet;
	float Z = 0.f;
	if (!P.bIndoors && TraceGround(Feet, Z))
	{
		At.Z = Z;
	}
	FVector2D& Turn = RideTurn.FindOrAdd(PersonId, FVector2D(P.Yaw, 0.f));
	const float D = FMath::FindDeltaAngleDegrees(Turn.X, P.Yaw);
	const float Step = FMath::Clamp(D, -90.f * DeltaTime, 90.f * DeltaTime);
	Turn.X = FRotator::NormalizeAxis(Turn.X + Step);
	const float V = P.Speed * 100.f;
	const float YawRate = DeltaTime > 0.f ? FMath::DegreesToRadians(Step / DeltaTime) : 0.f;
	const float WantLean = Ride.Act == ELyudBikeAct::Ride ? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(V * YawRate / 980.f)), -20.f, 20.f) : 0.f;
	Turn.Y = FMath::FInterpTo(Turn.Y, WantLean, DeltaTime, 4.f);
	const float Yaw = Turn.X;
	const bool bOnIt = Ride.Act == ELyudBikeAct::Ride || Ride.Act == ELyudBikeAct::Mount || Ride.Act == ELyudBikeAct::Dismount;
	if (P.BikeAct == LPB::EBikeAct::Park)
	{
		// left standing at its spot while the hands let go
		const bool bHome = (S->Bikes[P.Bike].Pos - S->Bikes[P.Bike].Home).Len() < 0.05f && Homes[I].bValid;
		if (!B->IsTaken() || FVector::Dist2D(B->ParkedAt, bHome ? Homes[I].Ground : At) > 5.f)
		{
			B->Park(bHome ? Homes[I].Ground : At, bHome ? Homes[I].Yaw : Yaw, bHome ? Homes[I].Lean : 0.f);
		}
		B->SetHolder(Body);
	}
	else if (bOnIt)
	{
		const float Stand = Ride.Act == ELyudBikeAct::Mount ? 1.f - FMath::SmoothStep(0.72f, 0.92f, Ride.Phase)
			: (Ride.Act == ELyudBikeAct::Dismount ? FMath::SmoothStep(0.05f, 0.3f, Ride.Phase) : 0.f);
		B->SetHolder(Body);
		B->Place(At, Yaw, 0.f, Turn.Y, Stand);
		const bool bPedal = Ride.Act == ELyudBikeAct::Ride && V > 50.f;
		B->Roll(DeltaTime, Ride.Act == ELyudBikeAct::Ride ? V : 0.f, FMath::Clamp(D * 0.5f, -20.f, 20.f), bPedal, bPedal ? -1.f : -40.f, false, false);
	}
	else
	{
		// led by the bars on their right
		const FRotator R(0.f, Yaw, 0.f);
		const FVector G = At + R.RotateVector(FVector(-ALyudBike::Geom().BarTops[0].X + 42.f, 40.f * Ride.Scale, 0.f));
		B->SetHolder(Body);
		B->Place(G, Yaw, 0.f, -3.f, 0.f);
		B->Roll(DeltaTime, V, 0.f, false, -1.f, false, false);
	}
	Body->DriveBike(B, Ride, At, Yaw, V, DeltaTime);
	if (CVarBikesLog.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Warning, TEXT("BIKES %hs act %d mode %d T %.2f ride %d phase %.2f v %.0f at %s"), P.Key.c_str(), (int32)P.BikeAct, (int32)P.BikeMode, T,
			(int32)Ride.Act, Ride.Phase, V, *At.ToCompactString());
	}
}

// --- ANpcBody on a bike (declared in PeopleLife.h) ---
void ANpcBody::DriveBike(ALyudBike* Bike, const FLyudBikeRide& Ride, const FVector& Ground, float Yaw, float Speed, float DeltaTime)
{
	ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance());
	if (!Bike)
	{
		if (bOnBike && Anim)
		{
			Anim->SetBikeRider(FBikeRiderPose());
		}
		bOnBike = false;
		return;
	}
	bOnBike = true;
	bPlaced = true;
	Facing = Yaw;
	Blanket->SetVisibility(false);
	PlacePusher(false);
	const bool bOnIt = Ride.Act == ELyudBikeAct::Ride || Ride.Act == ELyudBikeAct::Mount || Ride.Act == ELyudBikeAct::Dismount;
	if (bOnIt)
	{
		// the body hangs on the bike's ground frame (it leans with it); the targets put it on the saddle
		SetActorTransform(FTransform(FRotator(0.f, -90.f, 0.f)) * Bike->GroundFrame());
		Play(Ride.Act == ELyudBikeAct::Ride ? FName(TEXT("SitIdle")) : FName(TEXT("StandIdle")), true);
	}
	else
	{
		SetActorLocationAndRotation(Ground, FRotator(0.f, Yaw - 90.f, 0.f));
		if (Speed > 5.f)
		{
			// the walk clip's name is looked at once per body (ANpcBody::SharedWalk), not made into a string every frame
			if (SharedWalk < 0)
			{
				if (const UAnimSequence* W = Clip(TEXT("Walk"))) SharedWalk = W->GetName().StartsWith(TEXT("A_Human")) ? 1 : 0;
			}
			const bool bShared = SharedWalk == 1;
			Play(TEXT("Walk"), true, FMath::Clamp(Speed / (bShared ? 93.f * BodyScale : 75.f), 0.3f, 2.5f));
		}
		else
		{
			Play(TEXT("StandIdle"), true);
		}
	}
	if (Anim)
	{
		Anim->SetEyesShut(false);
		Anim->SetChatter(false);
		Anim->SetComputer(NAME_None);
		Anim->SetOnFeet(!bOnIt);
		FBikeRiderPose Pose;
		Bike->RiderPose(Mesh, Ride, Pose);
		Anim->SetBikeRider(Pose);
	}
}

// --- console: looking at bikes and riders from a free camera, the hero to a bike, a report (offscreen checks) ---
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

namespace
{
	TWeakObjectPtr<ACameraActor> GBikeCam;
	TWeakObjectPtr<AActor> GBikeCamFollow;
	FVector GBikeCamOffset = FVector::ZeroVector;

	ACameraActor* BikeCam(UWorld* World)
	{
		if (!GBikeCam.IsValid())
		{
			FActorSpawnParameters P;
			P.ObjectFlags |= RF_Transient;
			GBikeCam = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, P);
			GBikeCam->GetCameraComponent()->bConstrainAspectRatio = false;
		}
		return GBikeCam.Get();
	}

	void BikeCamLook(UWorld* World, const FVector& At, const FVector& Target, float Fov)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		ACameraActor* C = BikeCam(World);
		if (!PC || !C)
		{
			return;
		}
		C->SetActorLocationAndRotation(At, (Target - At).Rotation());
		C->GetCameraComponent()->SetFieldOfView(Fov);
		PC->SetViewTarget(C);
	}

	/** The person a key names, their body (if shown) and their bike's actor. */
	AActor* BikeFollowTarget(UWorld* World, const FString& Name)
	{
		ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(World);
		if (ALyudBike* B = Bikes ? Bikes->FindBike(FName(*Name)) : nullptr)
		{
			return B;
		}
		UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(World);
		FPersonInfo Info;
		if (Life && Life->GetPerson(Life->FindPerson(Name), Info))
		{
			return Info.Actor;
		}
		return nullptr;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBikeViewFrom(TEXT("Bike.ViewFrom"), TEXT("Bike.ViewFrom x y z pitch yaw [fov]: a free camera (cm, deg) | off"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		GBikeCamFollow.Reset();
		if (!PC || A.Num() == 0 || A[0] == TEXT("off"))
		{
			if (PC && PC->GetPawn())
			{
				PC->SetViewTarget(PC->GetPawn());
			}
			return;
		}
		if (A.Num() >= 5)
		{
			const FVector At(FCString::Atof(*A[0]), FCString::Atof(*A[1]), FCString::Atof(*A[2]));
			const FRotator R(FCString::Atof(*A[3]), FCString::Atof(*A[4]), 0.f);
			BikeCamLook(World, At, At + R.Vector() * 100.f, A.Num() > 5 ? FCString::Atof(*A[5]) : 60.f);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBikeLook(TEXT("Bike.Look"),
	TEXT("Bike.Look <bike id | person key> [dist cm] [yaw deg round it, 0 = from its right] [height cm] [fov] [follow]: a camera on a bike or a rider"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* World)
	{
		if (!World || A.Num() == 0)
		{
			return;
		}
		AActor* T = BikeFollowTarget(World, A[0]);
		if (!T)
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKETEST no bike or person %s"), *A[0]);
			return;
		}
		const float Dist = A.Num() > 1 ? FCString::Atof(*A[1]) : 260.f;
		const float Around = A.Num() > 2 ? FCString::Atof(*A[2]) : 0.f;
		const float Up = A.Num() > 3 ? FCString::Atof(*A[3]) : 90.f;
		const float Fov = A.Num() > 4 ? FCString::Atof(*A[4]) : 50.f;
		// from the bike's right side (its +Y), turned Around degrees toward its front
		const float Yaw = (float)T->GetActorRotation().Yaw + (T->IsA<ALyudBike>() ? 0.f : 90.f);
		const FVector Off = FRotator(0.f, Yaw + 90.f - Around, 0.f).Vector() * Dist + FVector(0.f, 0.f, Up);
		const FVector Target = T->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * 10.f + FVector(0.f, 0.f, T->IsA<ALyudBike>() ? 55.f : 90.f);
		BikeCamLook(World, Target + Off, Target, Fov);
		GBikeCamFollow = A.Num() > 5 && A[5] == TEXT("follow") ? T : nullptr;
		GBikeCamOffset = Off;
	}));

static FAutoConsoleCommandWithWorldAndArgs GBikeGoto(TEXT("Bike.Goto"), TEXT("Bike.Goto <bike id>: the hero stands at the bike's left, facing it"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* World)
	{
		ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Hero = PC ? PC->GetPawn() : nullptr;
		ALyudBike* B = Bikes && A.Num() > 0 ? Bikes->FindBike(FName(*A[0])) : nullptr;
		if (!Hero || !B)
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKETEST goto: no hero or bike"));
			return;
		}
		const FRotator R(0.f, B->GetActorRotation().Yaw, 0.f);
		const FVector At = B->GetActorLocation() + R.RotateVector(FVector(-5.f, -75.f, 100.f));
		Hero->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(-25.f, R.Yaw + 70.f, 0.f));
		if (AHeroCharacter* H = Cast<AHeroCharacter>(Hero))
		{
			H->SetBodyYaw(R.Yaw + 70.f);
		}
		UE_LOG(LogTemp, Warning, TEXT("BIKETEST goto %s at %s"), *A[0], *At.ToCompactString());
	}));

static FAutoConsoleCommandWithWorld GBikeReport(TEXT("Bike.Report"), TEXT("Logs every bike: where, who has it, the life's owner and rider"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ULyudBikeSubsystem* Bikes = ULyudBikeSubsystem::Get(World);
		UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(World);
		const LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
		for (const ALyudBike* B : Bikes ? Bikes->GetBikes() : TArray<TObjectPtr<ALyudBike>>())
		{
			UE_LOG(LogTemp, Warning, TEXT("BIKEREPORT %s at %s yaw %.0f holder %s hidden %d"), *B->BikeId.ToString(), *B->GetActorLocation().ToCompactString(),
				B->GetActorRotation().Yaw, *GetNameSafe(B->GetHolder()), B->IsHidden() ? 1 : 0);
		}
		for (int32 I = 0; S && I < (int32)S->Bikes.size(); I++)
		{
			const LPB::FWorld::FBike& SB = S->Bikes[I];
			UE_LOG(LogTemp, Warning, TEXT("BIKEREPORT life %hs owner %hs rider %hs mode %d pos %.1f %.1f home %d"), SB.Id.c_str(), SB.Owner.c_str(),
				SB.Rider >= 0 ? S->People[SB.Rider].Key.c_str() : "-", SB.Rider >= 0 ? (int32)S->People[SB.Rider].BikeMode : 0, SB.Pos.X, SB.Pos.Y,
				(SB.Pos - SB.Home).Len() < 0.05f ? 1 : 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBikeNpc(TEXT("Bike.NpcRide"),
	TEXT("Bike.NpcRide <bike id (an entrance)> [activity] [place]: its owner sets off by bike (an order far enough for a ride)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* World)
	{
		UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(World);
		LPB::FWorld* S = Life && Life->IsRunning() ? Life->GetSim() : nullptr;
		if (!S || A.Num() == 0)
		{
			return;
		}
		// the owner (any bike's with "any") who is at home now: their way out passes the bike in the entrance
		for (const LPB::FWorld::FBike& B : S->Bikes)
		{
			if (A[0] != TEXT("any") && B.Id != TCHAR_TO_UTF8(*A[0]))
			{
				continue;
			}
			for (const LPB::FPerson& P : S->People)
			{
				if (P.Household == B.Owner && !P.bChild && P.Age <= 76 && P.Home >= 0 && P.Place == P.Home && B.Rider < 0)
				{
					const FString Act = A.Num() > 1 ? A[1] : TEXT("sea_air");
					const bool bOk = Life->Order(P.Id, Act, A.Num() > 2 ? A[2] : TEXT(""));
					UE_LOG(LogTemp, Warning, TEXT("BIKETEST npc %hs (%hs) ordered %s: %d, bike %d mode %d"), P.Key.c_str(), B.Id.c_str(), *Act, bOk ? 1 : 0, P.Bike, (int32)P.BikeMode);
					if (P.Bike >= 0)
					{
						// a camera on the bike, following it out of the entrance and along the way
						if (AActor* T = BikeFollowTarget(World, UTF8_TO_TCHAR(B.Id.c_str())))
						{
							GBikeCamOffset = FVector(230.f, 0.f, 110.f);
							const FVector Target = T->GetActorLocation() + FVector(0.f, 0.f, 55.f);
							BikeCamLook(World, Target + FRotator(0.f, T->GetActorRotation().Yaw + 180.f, 0.f).Vector() * 230.f + FVector(0.f, 0.f, 110.f), Target, 55.f);
							GBikeCamFollow = T;
							// the hero a few steps off in the lobby (its lamps light only near him by day)
							if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->GetPawn())
							{
								PC->GetPawn()->SetActorLocation(T->GetActorLocation() + FRotator(0.f, T->GetActorRotation().Yaw, 0.f).RotateVector(FVector(-260.f, 0.f, 100.f)),
									false, nullptr, ETeleportType::TeleportPhysics);
							}
						}
					}
					return;
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("BIKETEST npc: no owner for %s"), *A[0]);
	}));

void ULyudBikeSubsystem::FollowCamera()
{
	if (AActor* T = GBikeCamFollow.Get(); T && GBikeCam.IsValid())
	{
		const float Yaw = (float)T->GetActorRotation().Yaw + (T->IsA<ALyudBike>() ? 0.f : 90.f);
		const FVector Target = T->GetActorLocation() + FVector(0.f, 0.f, T->IsA<ALyudBike>() ? 55.f : 90.f);
		const float D = (float)GBikeCamOffset.Size2D();
		const FVector Off = FRotator(0.f, Yaw + 180.f, 0.f).Vector() * D + FVector(0.f, 0.f, GBikeCamOffset.Z);
		const FVector Now = GBikeCam->GetActorLocation();
		const FVector Want = Target + Off;
		const FVector At = FVector::Dist(Now, Want) > 600.f ? Want : FMath::Lerp(Now, Want, 0.1f);
		GBikeCam->SetActorLocationAndRotation(At, (Target - At).Rotation());
	}
}
