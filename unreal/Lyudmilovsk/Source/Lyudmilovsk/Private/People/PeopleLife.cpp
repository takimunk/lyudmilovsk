#include "People/PeopleLife.h"
#include "People/LyudBodyAnim.h"
#include "Transport/LyudTransport.h"

#include "PeopleSim.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "BoothOwner.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "Plants/GameCalendar.h"
#include "Sky/Ephemeris.h"
#if __has_include("Doors/LyudDoor.h")
#include "Doors/LyudDoor.h"
#define LYUD_HAS_DOORS 1
#else
#define LYUD_HAS_DOORS 0
#endif

#include <cmath>

namespace LP = LyudPeople;

static TAutoConsoleVariable<int32> CVarPeople(TEXT("lyud.People"), 1,
	TEXT("The people's life (UPeopleLifeSubsystem): 1 on, 0 off (the club's people stay at their stations as placed)."));
static TAutoConsoleVariable<float> CVarPeopleDraw(TEXT("lyud.People.DrawDistance"), 150.f,
	TEXT("People outdoors are drawn within this many metres of the camera (indoors within 40 m)."));
static TAutoConsoleVariable<int32> CVarPeopleAnyMap(TEXT("lyud.People.AnyMap"), 0,
	TEXT("1: run the people's life in any game map (the town's data), not only Lvl_Scene01."));

namespace PeopleLifeLocal
{
	FVector PeopleToUE(const LP::V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	LP::V3 PeopleFromUE(const FVector& V) { return LP::V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }
	float PeopleDayHour(const FDateTime& T) { return T.GetHour() + T.GetMinute() / 60.f + T.GetSecond() / 3600.f; }

	/** Sunrise and sunset (local clock), and the sun's bearing at sunset as a UE yaw: north +Y, east -X. */
	void PeopleSunTimes(const FDateTime& Day, float& Rise, float& Set, float& SetYaw)
	{
		Rise = 7.f;
		Set = 17.f;
		SetYaw = 30.f;
		const FDateTime Midnight(Day.GetYear(), Day.GetMonth(), Day.GetDay());
		double Prev = -90.0;
		for (int32 M = 0; M <= 24 * 60; M += 4)
		{
			const FDateTime Utc = Midnight + FTimespan::FromMinutes(M) - FTimespan::FromHours(2);
			const FSkyPosition Sun = LyudEphemeris::Sun(Utc, 54.96, 20.40);
			if (Prev < -0.83 && Sun.AltitudeDeg >= -0.83)
			{
				Rise = M / 60.f;
			}
			if (Prev >= -0.83 && Sun.AltitudeDeg < -0.83 && M > 0)
			{
				Set = M / 60.f;
				const double A = FMath::DegreesToRadians(Sun.AzimuthDeg);
				SetYaw = FMath::RadiansToDegrees(FMath::Atan2(FMath::Cos(A), -FMath::Sin(A)));
			}
			Prev = Sun.AltitudeDeg;
		}
	}

	const TCHAR* PeopleEventNames[] = { TEXT("Start"), TEXT("Arrive"), TEXT("Leave"), TEXT("TalkStart"), TEXT("TalkEnd"), TEXT("Ride"), TEXT("Away"), TEXT("Back") };
}

using namespace PeopleLifeLocal;

// --- the subsystem ------------------------------------------------------------------------------------

UPeopleLifeSubsystem::UPeopleLifeSubsystem() = default;
UPeopleLifeSubsystem::~UPeopleLifeSubsystem() = default;

UPeopleLifeSubsystem* UPeopleLifeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UPeopleLifeSubsystem>() : nullptr;
}

bool UPeopleLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UPeopleLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPeopleLifeSubsystem, STATGROUP_Tickables);
}

void UPeopleLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWantStart = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")) || CVarPeopleAnyMap.GetValueOnGameThread() != 0;
	StartDelay = 0.5f;      // after a load has been applied (the tick after BeginPlay)
	if (!bWantStart)
	{
		return;
	}
	// registered now so a load restores Homes and Memory before the life starts
	if (ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(&InWorld))
	{
		Save->RegisterSaveObject(TEXT("PeopleLife"), this);
		Save->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*)
		{
			bLoaded = true;
			if (bStarted)
			{
				ApplySaved();
			}
		});
		Save->OnBeforeSave.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncMemory(true); });
	}
}

void UPeopleLifeSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

bool UPeopleLifeSubsystem::Start()
{
	TPimplPtr<LP::FWorld> W = MakePimpl<LP::FWorld>();
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("PeopleData"));
	const FString Animals = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("AnimalsData"));
	if (!W->Load(TCHAR_TO_UTF8(*Dir), TCHAR_TO_UTF8(*Animals)))
	{
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: %s"), UTF8_TO_TCHAR(W->LastError.c_str()));
		return false;
	}
	Sim = MoveTemp(W);
	UpdateClock(true);
	BindClub();
	if (bLoaded || Homes.Num() > 0)
	{
		ApplySaved();
	}
	else
	{
		Sim->AssignHomes();
		// a new game (the clock within a couple of minutes of the start date): the club scene as it is
		const FDateTime Now = UGameCalendar::NowIn(GetWorld());
		const FDateTime StartDate = GetDefault<UGameCalendar>()->StartDate;
		if (FMath::Abs((Now - StartDate).GetTotalSeconds()) < 180.0)
		{
			Sim->NewGame();
		}
		else
		{
			Sim->Resettle();
		}
	}
	int32 Flats = 0;
	for (const LP::FPlace& P : Sim->Places)
	{
		Flats += P.Kind == "flat" ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, TEXT("PeopleLife: sun %.2f..%.2f"), Sim->Sunrise, Sim->Sunset);
	UE_LOG(LogTemp, Display, TEXT("PeopleLife: %d people, %d activities, %d places (%d flats), %d slots, %d club actors; %s"),
		(int32)Sim->People.size(), (int32)Sim->Activities.size(), (int32)Sim->Places.size(), Flats, (int32)Sim->Slots.size(), ClubActors.Num(),
		UTF8_TO_TCHAR(Sim->Report().c_str()));
	return true;
}

void UPeopleLifeSubsystem::Stop()
{
	for (ANpcBody* Body : Bodies)
	{
		if (Body)
		{
			Body->Destroy();
		}
	}
	Bodies.Reset();
	for (const TPair<FString, TObjectPtr<AActor>>& Pair : ClubActors)
	{
		ShowPlaced(Pair.Value, true);
	}
	ClubActors.Reset();
	if (Owner)
	{
		Owner->SetAway(false);
		Owner = nullptr;
	}
	Talkable.Reset();
	Sim.Reset();
	bStarted = false;
}

void UPeopleLifeSubsystem::ApplySaved()
{
	LP::FWorld& S = *Sim;
	for (LP::FPlace& P : S.Places)
	{
		P.Residents.clear();
	}
	for (LP::FPerson& P : S.People)
	{
		P.Home = -1;
	}
	for (const TPair<FString, FString>& H : Homes)
	{
		if (!S.SetHome(TCHAR_TO_UTF8(*H.Key), TCHAR_TO_UTF8(*H.Value)))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: the saved home %s of %s is gone"), *H.Value, *H.Key);
		}
	}
	S.AssignHomes();
	SyncMemory(false);
	UpdateClock(true);
	S.Resettle();
	SyncMemory(false);      // the needs as saved (Resettle guesses tiredness and hunger from the hour)
	UE_LOG(LogTemp, Display, TEXT("PeopleLife: restored %d homes; %s"), Homes.Num(), UTF8_TO_TCHAR(S.Report().c_str()));
}

void UPeopleLifeSubsystem::SyncMemory(bool bToSave)
{
	if (!Sim)
	{
		return;
	}
	LP::FWorld& S = *Sim;
	if (bToSave)
	{
		Homes.Reset();
		for (const LP::FPerson& P : S.People)
		{
			if (P.Home >= 0)
			{
				Homes.Add(UTF8_TO_TCHAR(P.Household.c_str()), UTF8_TO_TCHAR(S.Places[P.Home].Id.c_str()));
			}
			FPersonMemory& M = Memory.FindOrAdd(UTF8_TO_TCHAR(P.Key.c_str()));
			M.Needs.SetNum(LP::NumNeeds);
			for (int32 N = 0; N < LP::NumNeeds; N++)
			{
				M.Needs[N] = P.N[N];
			}
		}
		return;
	}
	for (LP::FPerson& P : S.People)
	{
		if (const FPersonMemory* M = Memory.Find(UTF8_TO_TCHAR(P.Key.c_str())))
		{
			for (int32 N = 0; N < LP::NumNeeds && N < M->Needs.Num(); N++)
			{
				P.N[N] = M->Needs[N];
			}
		}
	}
}

bool UPeopleLifeSubsystem::UpdateClock(bool bFirst)
{
	LP::FWorld& S = *Sim;
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const double Seconds = (Now - GetDefault<UGameCalendar>()->StartDate).GetTotalSeconds();
	S.Hour = PeopleDayHour(Now);
	S.Weekday = (int)Now.GetDayOfWeek();     // Monday = 0
	S.Time = Seconds;
	const int32 Day = Now.GetDayOfYear() + Now.GetYear() * 400;
	if (Day != SunDay)
	{
		SunDay = Day;
		PeopleSunTimes(Now, S.Sunrise, S.Sunset, S.SunYaw);
	}
	S.bSunUp = S.Hour > S.Sunrise && S.Hour < S.Sunset;
	if (!bFirst && bClockSeen && FMath::Abs(Seconds - LastSeconds) > 900.0)
	{
		UE_LOG(LogTemp, Display, TEXT("PeopleLife: the clock jumped %.2f h; people resettled"), (Seconds - LastSeconds) / 3600.0);
		S.Resettle();
		LastSeconds = Seconds;
		return true;
	}
	LastSeconds = Seconds;
	bClockSeen = true;
	return false;
}

/** The scene's people placed at their stations (SkeletalMeshActors at a station's chair) and the owner in his booth. */
void UPeopleLifeSubsystem::BindClub()
{
	LP::FWorld& S = *Sim;
	UWorld* World = GetWorld();
	for (TActorIterator<ABoothOwner> It(World); It; ++It)
	{
		Owner = *It;
	}
	for (const LP::FSlot& Sl : S.Slots)
	{
		if (Sl.Kind != "club_seat")
		{
			continue;
		}
		const FVector Chair = PeopleToUE(Sl.Pos);
		AActor* Best = nullptr;
		double BestD = 45.0;
		for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
		{
			const USkeletalMeshComponent* M = It->GetSkeletalMeshComponent();
			const USkeletalMesh* Asset = M ? M->GetSkeletalMeshAsset() : nullptr;
			if (!Asset || !Asset->GetName().StartsWith(TEXT("SK_")) || Asset->GetName().Contains(TEXT("desk")))
			{
				continue;
			}
			const double D = FVector::Dist2D(It->GetActorLocation(), Chair);
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
		if (Best)
		{
			ClubActors.Add(UTF8_TO_TCHAR(Sl.Tag.c_str()), Best);
		}
	}
}

void UPeopleLifeSubsystem::ShowPlaced(AActor* Actor, bool bShow)
{
	if (!Actor || Actor->IsHidden() == !bShow)
	{
		return;
	}
	Actor->SetActorHiddenInGame(!bShow);
	if (USkeletalMeshComponent* M = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		M->SetVisibility(bShow, true);
	}
}

ANpcBody* UPeopleLifeSubsystem::BodyFor(int32 Id)
{
	if (Bodies.Num() <= Id)
	{
		Bodies.SetNum(Id + 1);
	}
	if (!Bodies[Id])
	{
		const LP::FPerson& P = Sim->People[Id];
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANpcBody* Body = GetWorld()->SpawnActor<ANpcBody>(PeopleToUE(P.Pos), FRotator(0.f, P.Yaw, 0.f), SP);
		if (!Body)
		{
			return nullptr;
		}
		if (!Body->Setup(UTF8_TO_TCHAR(P.Mesh.c_str()), UTF8_TO_TCHAR(P.ComputerClip.c_str())))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: no model for %s (%s)"), UTF8_TO_TCHAR(P.Key.c_str()), UTF8_TO_TCHAR(P.Mesh.c_str()));
		}
		if (P.Key.rfind("crowd", 0) == 0)
		{
			Body->Tags.Add(TEXT("NoTalk"));
		}
#if LYUD_HAS_DOORS
		// doors open in front of them and shut behind them
		if (ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(this))
		{
			Doors->RegisterAgent(Body, 35.f);
		}
#endif
		Bodies[Id] = Body;
	}
	return Bodies[Id];
}

void UPeopleLifeSubsystem::Present(float DeltaTime)
{
	LP::FWorld& S = *Sim;
	UWorld* World = GetWorld();
	ULyudTransportSubsystem* Transport = ULyudTransportSubsystem::Get(this);
	FVector Cam = FVector::ZeroVector;
	FRotator CamRot;
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->GetPlayerViewPoint(Cam, CamRot);
	}
	const float Draw = CVarPeopleDraw.GetValueOnGameThread() * 100.f;
	TSet<FString> PlacedShown;
	bool bOwnerHere = false;
	for (const LP::FPerson& P : S.People)
	{
		const LP::FSlot* Sl = P.Slot >= 0 ? &S.Slots[P.Slot] : nullptr;
		const bool bAtSlot = P.State == LP::EState::Doing || P.State == LP::EState::Settling;
		// the owner at his booth: the scene's ABoothOwner (window, trade)
		if (P.Key == "owner" && Owner && Sl && Sl->Kind == "booth" && bAtSlot)
		{
			bOwnerHere = true;
			if (P.Id < Bodies.Num() && Bodies[P.Id])
			{
				Bodies[P.Id]->SetActorHiddenInGame(true);
			}
			continue;
		}
		// at their own station: the scene's own actor of them sits there
		if (Sl && Sl->Kind == "club_seat" && P.State == LP::EState::Doing && Sl->Tag == P.ClubSeat)
		{
			const FString Tag = UTF8_TO_TCHAR(Sl->Tag.c_str());
			if (ClubActors.Contains(Tag))
			{
				PlacedShown.Add(Tag);
				if (P.Id < Bodies.Num() && Bodies[P.Id])
				{
					Bodies[P.Id]->SetActorHiddenInGame(true);
				}
				continue;
			}
		}
		// riding the cable car: on its platform waiting, or seated in a cabin (ULyudTransportSubsystem places them)
		if (P.State == LP::EState::Riding && P.bRideExternal && Transport)
		{
			FVector RideAt;
			float RideYaw = 0.f;
			bool bSeated = false;
			const bool bShowRider = Transport->RiderPose(P.Id, RideAt, RideYaw, bSeated) && FVector::Dist(RideAt, Cam) < Draw;
			ANpcBody* Body = bShowRider ? BodyFor(P.Id) : (P.Id < Bodies.Num() ? Bodies[P.Id].Get() : nullptr);
			if (Body)
			{
				const bool bWasHidden = Body->IsHidden();
				Body->SetActorHiddenInGame(!bShowRider);
				if (bShowRider)
				{
					Body->Drive(RideAt, RideYaw, bSeated ? EPersonPose::Sit : EPersonPose::Stand, 0, 0.f, bSeated ? 44.f : 0.f, DeltaTime, bWasHidden || bSeated);
				}
			}
			continue;
		}
		const FVector Loc = PeopleToUE(P.Pos);
		const float Dist = FVector::Dist(Loc, Cam);
		const bool bShow = P.Pose != LP::EPose::Hidden && Dist < (P.bIndoors ? 4000.f : Draw);
		ANpcBody* Body = bShow ? BodyFor(P.Id) : (P.Id < Bodies.Num() ? Bodies[P.Id].Get() : nullptr);
		if (!Body)
		{
			continue;
		}
		const bool bWasHidden = Body->IsHidden();
		Body->SetActorHiddenInGame(!bShow);
		if (!bShow)
		{
			continue;
		}
		int32 Transit = 0;
		if (P.State == LP::EState::Settling)
		{
			Transit = P.Pose == LP::EPose::Sleep ? 3 : 1;
		}
		else if (P.State == LP::EState::Leaving)
		{
			Transit = P.Pose == LP::EPose::Sleep ? 4 : 2;
		}
		FVector At = Loc;
		if (!P.bIndoors && Dist < 6000.f)
		{
			// exact ground under them near the camera (steps, kerbs)
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleGround), false);
			Q.AddIgnoredActor(Body);
			if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 120), Loc - FVector(0, 0, 250), ECC_Visibility, Q))
			{
				At.Z = Hit.ImpactPoint.Z;
			}
		}
		const float SeatH = Sl && P.State != LP::EState::Going ? Sl->Height * 100.f : 0.f;
		Body->Drive(At, P.Yaw, (EPersonPose)(uint8)P.Pose, Transit, P.Speed * 100.f, SeatH, DeltaTime, bWasHidden);
	}
	for (const TPair<FString, TObjectPtr<AActor>>& Pair : ClubActors)
	{
		ShowPlaced(Pair.Value, PlacedShown.Contains(Pair.Key));
	}
	if (Owner)
	{
		Owner->SetAway(!bOwnerHere);
	}
	// people walking about can be talked to as in the club
	DialogueTimer -= DeltaTime;
	if (DialogueTimer <= 0.f)
	{
		DialogueTimer = 2.f;
		ANpcTalkDirector* Director = nullptr;
		for (TActorIterator<ANpcTalkDirector> It(World); It; ++It)
		{
			Director = *It;
		}
		for (int32 Id = 0; Director && Id < Bodies.Num(); Id++)
		{
			if (Bodies[Id] && !Talkable.Contains(Id) && !Bodies[Id]->ActorHasTag(TEXT("NoTalk")))
			{
				Director->AddPerson(FName(*Bodies[Id]->MeshName), Bodies[Id], Bodies[Id]->Mesh);
				Talkable.Add(Id);
			}
		}
	}
}

void UPeopleLifeSubsystem::DispatchEvents()
{
	LP::FWorld& S = *Sim;
	const std::vector<LP::FEvent> Events = S.Events;
	S.Events.clear();
	for (const LP::FEvent& E : Events)
	{
		OnPersonEvent.Broadcast(E.Person, FName(PeopleEventNames[(int)E.Kind]), E.Other);
	}
}

void UPeopleLifeSubsystem::Tick(float DeltaTime)
{
	if (CVarPeople.GetValueOnGameThread() == 0)
	{
		if (bStarted)
		{
			Stop();
			bWantStart = true;
		}
		return;
	}
	if (!bStarted)
	{
		StartDelay -= DeltaTime;
		if (!bWantStart || StartDelay > 0.f)
		{
			return;
		}
		bWantStart = false;
		bStarted = Start();
		if (!bStarted)
		{
			return;
		}
	}
	LP::FWorld& S = *Sim;
	const double Was = S.Time;
	if (UpdateClock(false))
	{
		Present(DeltaTime);
		return;
	}
	// the life runs on the game's calendar (its time scale), in steps of at most a second
	double Dt = S.Time - Was;
	S.Time = Was;
	Dt = FMath::Clamp(Dt, 0.0, 900.0);
	while (Dt > 0.0)
	{
		const float Step = (float)FMath::Min(Dt, 1.0);
		S.Step(Step);
		Dt -= Step;
	}
	DispatchEvents();
	Present(DeltaTime);
	SaveTimer -= DeltaTime;
	if (SaveTimer <= 0.f)
	{
		SaveTimer = 5.f;
		SyncMemory(true);
	}
}

int32 UPeopleLifeSubsystem::NumPeople() const
{
	return Sim ? (int32)Sim->People.size() : 0;
}

int32 UPeopleLifeSubsystem::FindPerson(const FString& Key) const
{
	return Sim ? Sim->FindPerson(TCHAR_TO_UTF8(*Key)) : -1;
}

bool UPeopleLifeSubsystem::GetPerson(int32 Id, FPersonInfo& Out) const
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->People.size())
	{
		return false;
	}
	const LP::FWorld& S = *Sim;
	const LP::FPerson& P = S.People[Id];
	Out.Id = Id;
	Out.Key = UTF8_TO_TCHAR(P.Key.c_str());
	Out.Mesh = FName(UTF8_TO_TCHAR(P.Mesh.c_str()));
	Out.Age = P.Age;
	Out.Occupation = UTF8_TO_TCHAR(P.Occupation.c_str());
	Out.Household = UTF8_TO_TCHAR(P.Household.c_str());
	Out.Home = P.Home >= 0 ? FString(UTF8_TO_TCHAR(S.Places[P.Home].Id.c_str())) : FString();
	Out.Activity = P.Activity >= 0 ? FString(UTF8_TO_TCHAR(S.Activities[P.Activity].Id.c_str())) : FString();
	Out.Place = P.Place >= 0 ? FString(UTF8_TO_TCHAR(S.Places[P.Place].Id.c_str())) : FString();
	Out.State = UTF8_TO_TCHAR(LP::StateName(P.State));
	Out.Pose = (EPersonPose)(uint8)P.Pose;
	Out.Location = PeopleToUE(P.Pos);
	Out.Yaw = P.Yaw;
	Out.Needs.Reset();
	for (int32 N = 0; N < LP::NumNeeds; N++)
	{
		Out.Needs.Add(FName(LP::NeedName((LP::ENeed)N)), P.N[N]);
	}
	Out.Partner = P.Partner;
	Out.Actor = nullptr;
	if (P.Key == "owner" && Owner && !Owner->IsAway())
	{
		Out.Actor = Owner;
	}
	else if (P.Slot >= 0 && S.Slots[P.Slot].Tag == P.ClubSeat && ClubActors.Contains(UTF8_TO_TCHAR(P.ClubSeat.c_str()))
		&& !ClubActors[UTF8_TO_TCHAR(P.ClubSeat.c_str())]->IsHidden())
	{
		Out.Actor = ClubActors[UTF8_TO_TCHAR(P.ClubSeat.c_str())];
	}
	else if (Id < Bodies.Num() && Bodies[Id] && !Bodies[Id]->IsHidden())
	{
		Out.Actor = Bodies[Id];
	}
	return true;
}

bool UPeopleLifeSubsystem::Order(int32 Id, const FString& Activity, const FString& PlaceId)
{
	return Sim && Sim->Order(Id, TCHAR_TO_UTF8(*Activity), TCHAR_TO_UTF8(*PlaceId));
}

bool UPeopleLifeSubsystem::SetHome(const FString& Household, const FString& FlatId)
{
	if (!Sim || !Sim->SetHome(TCHAR_TO_UTF8(*Household), TCHAR_TO_UTF8(*FlatId)))
	{
		return false;
	}
	Sim->AssignHomes();     // beds in the new flat
	SyncMemory(true);
	return true;
}

TMap<FString, FString> UPeopleLifeSubsystem::Flats() const
{
	TMap<FString, FString> Out;
	if (!Sim)
	{
		return Out;
	}
	for (const LP::FPlace& P : Sim->Places)
	{
		if (P.Kind != "flat")
		{
			continue;
		}
		FString Who;
		for (const int Id : P.Residents)
		{
			Who += (Who.IsEmpty() ? TEXT("") : TEXT(",")) + FString(UTF8_TO_TCHAR(Sim->People[Id].Key.c_str()));
		}
		Out.Add(UTF8_TO_TCHAR(P.Id.c_str()), Who);
	}
	return Out;
}

FString UPeopleLifeSubsystem::Report() const
{
	return Sim ? FString(UTF8_TO_TCHAR(Sim->Report().c_str())) : FString(TEXT("the people's life is not running"));
}

FString UPeopleLifeSubsystem::Describe(int32 Id) const
{
	return Sim ? FString(UTF8_TO_TCHAR(Sim->Describe(Id).c_str())) + TEXT(" || ") + UTF8_TO_TCHAR(Sim->Scores(Id).c_str()) : FString();
}

// --- a body -------------------------------------------------------------------------------------------

ANpcBody::ANpcBody()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// clips cross-fade (ULyudBodyAnimInstance) instead of popping from one to the next
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetAnimInstanceClass(ULyudBodyAnimInstance::StaticClass());
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	// outdoors the sun lights channel 1 (ALyudSky), indoors the lamps channel 0
	Mesh->LightingChannels.bChannel0 = true;
	Mesh->LightingChannels.bChannel1 = true;
	Mesh->bCastDynamicShadow = true;
	Blanket = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blanket"));
	Blanket->SetupAttachment(Mesh);
	Blanket->SetUsingAbsoluteLocation(true);
	Blanket->SetUsingAbsoluteRotation(true);
	Blanket->SetUsingAbsoluteScale(true);
	Blanket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blanket->SetVisibility(false);
	Blanket->LightingChannels.bChannel0 = true;
	Blanket->LightingChannels.bChannel1 = true;
}

bool ANpcBody::Setup(const FString& InMesh, const FString& InComputerClip)
{
	MeshName = InMesh;
	ComputerClip = InComputerClip;
	const FString Path = FString::Printf(TEXT("/Game/Characters/Humans/%s/SK_%s.SK_%s"), *InMesh, *InMesh, *InMesh);
	USkeletalMesh* Asset = LoadObject<USkeletalMesh>(nullptr, *Path);
	if (!Asset)
	{
		return false;
	}
	Mesh->SetSkeletalMeshAsset(Asset);
	// the shared clips are baked on Guy_a (pelvis 93.7 cm over the floor, blender/humans/clips.json): the pelvis moves scale
	// with the body
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	int32 Bone = Ref.FindBoneIndex(TEXT("pelvis"));
	FTransform T = FTransform::Identity;
	while (Bone != INDEX_NONE)
	{
		T = T * Ref.GetRefBonePose()[Bone];
		Bone = Ref.GetParentIndex(Bone);
	}
	BodyScale = FMath::Clamp((float)T.GetLocation().Z / 93.7f, 0.5f, 1.3f);
	Seed = GetTypeHash(InMesh) ^ (uint32)GetUniqueID();
	return true;
}

/** A clip by role: the shared set (/Game/Characters/Humans/Anims/A_Human_<Name>), else the owner's, else none. */
UAnimSequence* ANpcBody::Clip(FName Name)
{
	if (const TObjectPtr<UAnimSequence>* Found = Clips.Find(Name))
	{
		return *Found;
	}
	auto Load = [](const FString& Path) -> UAnimSequence*
	{
		const FString Asset = FPaths::GetBaseFilename(Path);
		return LoadObject<UAnimSequence>(nullptr, *(Path + TEXT(".") + Asset), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	const FString N = Name.ToString();
	UAnimSequence* A = nullptr;
	if (N == TEXT("Computer"))
	{
		if (ComputerClip == TEXT("Excel"))
		{
			A = Load(TEXT("/Game/Characters/Clerk/A_Clerk_Sit_Excel"));
		}
		for (const FString& C : { ComputerClip, FString(TEXT("Sit_KeyboardMouse")) })
		{
			if (!A && !C.IsEmpty())
			{
				// "Guy_a/Sit_KeyboardMouse" borrows another person's seated clip (the crowd has none of its own)
				FString Who = MeshName, What = C;
				if (C.Contains(TEXT("/")))
				{
					C.Split(TEXT("/"), &Who, &What);
				}
				A = Load(FString::Printf(TEXT("/Game/Characters/Humans/%s/Anims/A_%s_%s"), *Who, *Who, *What));
			}
		}
		if (!A)
		{
			A = Clip(TEXT("SitIdle"));
		}
	}
	else
	{
		A = Load(TEXT("/Game/Characters/Humans/Anims/A_Human_") + N);
		if (!A)
		{
			// the owner's clips are on the same skeleton
			static const TMap<FString, FString> Owner = {
				{ TEXT("Walk"), TEXT("Walk") }, { TEXT("StandIdle"), TEXT("StandLook") }, { TEXT("Talk"), TEXT("StandLook") },
				{ TEXT("SitDown"), TEXT("SitDown") }, { TEXT("StandUp"), TEXT("StandUp") }, { TEXT("SitIdle"), TEXT("Sit_Idle") },
				{ TEXT("Eat"), TEXT("Sit_Idle") }, { TEXT("Read"), TEXT("Sit_Idle") } };
			if (const FString* O = Owner.Find(N))
			{
				A = Load(TEXT("/Game/Characters/Humans/Owner/Anims/A_Owner_") + *O);
			}
		}
	}
	Clips.Add(Name, A);
	return A;
}

void ANpcBody::Play(FName Name, bool bLoop, float Rate)
{
	ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance());
	if (Playing != Name)
	{
		Playing = Name;
		if (UAnimSequence* A = Clip(Name))
		{
			if (Anim)
			{
				Anim->PlayClip(A, bLoop, Rate);
			}
			else
			{
				Mesh->PlayAnimation(A, bLoop);
			}
		}
	}
	if (Anim)
	{
		Anim->SetClipRate(Rate);
	}
	else
	{
		Mesh->SetPlayRate(Rate);
	}
}

void ANpcBody::PlayVaried(FName InRole, std::initializer_list<FName> Variants, float Rate)
{
	// a new role, or back from another clip (a walk, sitting down) into the same role
	bool bOurs = false;
	for (const FName& V : Variants)
	{
		bOurs |= V == Playing;
	}
	if (Role != InRole || !bOurs || Clock >= VariantUntil)
	{
		const bool bNew = Role != InRole || !bOurs;
		Role = InRole;
		Seed = Seed * 1664525u + 1013904223u;
		const int32 N = (int32)Variants.size();
		// a fresh role starts with its own clip; later ones pick among all
		const int32 I = bNew ? 0 : (int32)((Seed >> 8) % (uint32)N);
		const FName Pick = *(Variants.begin() + I);
		VariantUntil = Clock + 6.f + 6.f * (float)((Seed >> 16) & 1023) / 1023.f;
		if (Clip(Pick))
		{
			Play(Pick, true, Rate);
			return;
		}
	}
	Play(bOurs && Clip(Playing) ? Playing : *Variants.begin(), true, Rate);
}

void ANpcBody::Drive(const FVector& Location, float Yaw, EPersonPose Pose, int32 Transit, float Speed, float SeatHeight, float DeltaTime, bool bSnap)
{
	// turn smoothly, except into a seat or a bed (the clip starts from the slot's facing)
	if (bSnap || Transit != 0 || !bPlaced)
	{
		Facing = Yaw;
	}
	else
	{
		const float D = FMath::FindDeltaAngleDegrees(Facing, Yaw);
		Facing += FMath::Clamp(D, -240.f * DeltaTime, 240.f * DeltaTime);
	}
	bPlaced = true;
	Clock += DeltaTime;
	const bool bBed = Pose == EPersonPose::Sleep || Transit == 3 || Transit == 4;
	const bool bSeat = !bBed && (Transit == 1 || Transit == 2 || Pose == EPersonPose::Sit || Pose == EPersonPose::Computer
		|| Pose == EPersonPose::Eat || Pose == EPersonPose::Read);
	const float S = BodyScale;
	if (bBed)
	{
		// the lying clips: the actor stands beside the bed facing away from it (+Y), the mattress (top 50 cm) behind it,
		// its middle at local (-5, -55) cm, the head toward local +X. Location is the floor under the mattress's middle and
		// Yaw the bed's head-to-foot: the actor turns so local -X runs head to foot.
		const float MeshYaw = Yaw + 180.f;
		const FRotator R(0.f, MeshYaw, 0.f);
		FVector At = Location - R.RotateVector(FVector(-5.f, -55.f, 0.f) * S);
		At.Z += (SeatHeight > 0.f ? SeatHeight : 50.f) - 50.f * S;
		SetActorLocationAndRotation(At, R);
		BedCenter = Location;
		BedYaw = Yaw;
		BedTop = SeatHeight > 0.f ? SeatHeight : 50.f;
	}
	else
	{
		// seated clips: the origin under the seat's centre (top 47 cm), facing local +Y; a smaller body sits higher
		FVector At = Location;
		if (bSeat)
		{
			At.Z += (SeatHeight > 0.f ? SeatHeight : 47.f) - 47.f * S;
		}
		// the model faces +Y at yaw 0
		SetActorLocationAndRotation(At, FRotator(0.f, Facing - 90.f, 0.f));
	}
	// under the blanket once they lie still; it goes when they get up
	const bool bAsleep = Pose == EPersonPose::Sleep && Transit == 0;
	StillAsleep = bAsleep ? StillAsleep + DeltaTime : 0.f;
	if (!bAsleep)
	{
		bBlanket = false;
	}
	else if (!bBlanket && StillAsleep > 0.6f)
	{
		BuildBlanket();
		bBlanket = true;
	}
	Blanket->SetVisibility(bAsleep && bBlanket && BlanketMesh != nullptr);
	if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetEyesShut(Pose == EPersonPose::Sleep && Transit == 0);
		Anim->SetChatter(Pose == EPersonPose::Talk && Transit == 0);
	}
	switch (Transit)
	{
	case 1: Play(TEXT("SitDown"), false); return;
	case 2: Play(TEXT("StandUp"), false); return;
	case 3: Play(TEXT("LieDown"), false); return;
	case 4: Play(TEXT("GetUp"), false); return;
	default: break;
	}
	switch (Pose)
	{
	case EPersonPose::Walk:
	{
		// the shared walk (Quaternius UAL Walk_Loop on Guy_a) covers ~93 cm/s at rate 1 (life_anims_report.json speed_m_s);
		// the owner's (the fallback) 75
		const bool bShared = Clip(TEXT("Walk")) && Clip(TEXT("Walk"))->GetName().StartsWith(TEXT("A_Human"));
		Play(TEXT("Walk"), true, FMath::Clamp(Speed / (bShared ? 93.f * S : 75.f), 0.3f, 2.5f));
		break;
	}
	case EPersonPose::Talk: PlayVaried(TEXT("Talk"), { TEXT("Talk"), TEXT("Talk"), TEXT("Nod"), TEXT("Talk"), TEXT("FoldArms") }); break;
	case EPersonPose::Look: PlayVaried(TEXT("Look"), { TEXT("StandIdle"), TEXT("StandIdle"), TEXT("FoldArms") }, 0.8f); break;
	case EPersonPose::Sit: PlayVaried(TEXT("Sit"), { TEXT("SitIdle"), TEXT("SitIdle"), TEXT("SitTalk") }); break;
	case EPersonPose::Computer: Play(TEXT("Computer"), true); break;
	case EPersonPose::Eat: Play(TEXT("Eat"), true); break;
	case EPersonPose::Read: Play(TEXT("Read"), true); break;
	case EPersonPose::Sleep: Play(TEXT("Sleep"), true); break;
	case EPersonPose::Work: Play(TEXT("Computer"), true); break;
	default: PlayVaried(TEXT("Stand"), { TEXT("StandIdle"), TEXT("StandIdle"), TEXT("FoldArms") }); break;
	}
}

void ANpcBody::BuildBlanket()
{
	// the bed's frame: U along it (head to foot), V across, Z up from the mattress's top
	const FRotator R(0.f, BedYaw, 0.f);
	const FVector U = R.Vector(), V = FVector::CrossProduct(FVector::UpVector, U);
	const FVector Top = BedCenter + FVector(0.f, 0.f, BedTop);
	auto Local = [&](const FVector& W) { const FVector D = W - Top; return FVector(FVector::DotProduct(D, U), FVector::DotProduct(D, V), D.Z); };
	struct FSeg { FName A, B; float R; };
	static const FSeg Segs[] = {
		{ TEXT("pelvis"), TEXT("spine_02"), 15.f }, { TEXT("spine_02"), TEXT("neck_01"), 15.f },
		{ TEXT("upperarm_l"), TEXT("lowerarm_l"), 6.5f }, { TEXT("lowerarm_l"), TEXT("hand_l"), 5.5f },
		{ TEXT("upperarm_r"), TEXT("lowerarm_r"), 6.5f }, { TEXT("lowerarm_r"), TEXT("hand_r"), 5.5f },
		{ TEXT("thigh_l"), TEXT("calf_l"), 9.f }, { TEXT("calf_l"), TEXT("foot_l"), 6.5f }, { TEXT("foot_l"), TEXT("ball_l"), 5.f },
		{ TEXT("thigh_r"), TEXT("calf_r"), 9.f }, { TEXT("calf_r"), TEXT("foot_r"), 6.5f }, { TEXT("foot_r"), TEXT("ball_r"), 5.f } };
	TArray<FVector> A, B;
	TArray<float> Rad;
	float UMin = 1e9f, UMax = -1e9f, VSum = 0.f;
	for (const FSeg& G : Segs)
	{
		if (Mesh->GetBoneIndex(G.A) == INDEX_NONE || Mesh->GetBoneIndex(G.B) == INDEX_NONE)
		{
			continue;
		}
		A.Add(Local(Mesh->GetBoneLocation(G.A)));
		B.Add(Local(Mesh->GetBoneLocation(G.B)));
		Rad.Add(G.R * BodyScale);
		for (const FVector& P : { A.Last(), B.Last() })
		{
			UMin = FMath::Min(UMin, P.X);
			UMax = FMath::Max(UMax, P.X);
		}
		VSum += A.Last().Y + B.Last().Y;
	}
	if (A.Num() < 4)
	{
		return;
	}
	// from just under the chin (the neck's end of the trunk) to a hand past the feet, a body's width each side
	const float U0 = Local(Mesh->GetBoneLocation(TEXT("neck_01"))).X + 4.f;
	const float U1 = FMath::Max(UMax, UMin) + 22.f;
	const float UStart = FMath::Min(U0, U1 - 40.f), VMid = VSum / (2.f * A.Num()), Half = 52.f * FMath::Max(BodyScale, 0.8f);
	const int32 NU = 34, NV = 18;
	TArray<float> H;
	H.SetNumZeroed((NU + 1) * (NV + 1));
	for (int32 j = 0; j <= NV; j++)
	{
		for (int32 i = 0; i <= NU; i++)
		{
			const FVector2D P(FMath::Lerp(UStart, U1, i / (float)NU), VMid + FMath::Lerp(-Half, Half, j / (float)NV));
			float Z = 4.f;
			for (int32 k = 0; k < A.Num(); k++)
			{
				const FVector2D a(A[k].X, A[k].Y), b(B[k].X, B[k].Y);
				const FVector2D ab = b - a;
				const float t = FMath::Clamp(FVector2D::DotProduct(P - a, ab) / FMath::Max(ab.SizeSquared(), 1.f), 0.f, 1.f);
				const float d = FVector2D::Distance(P, a + ab * t);
				const float r = Rad[k] + 13.f;    // the duvet's loft rounds over the body
				if (d < r)
				{
					const float zc = FMath::Lerp(A[k].Z, B[k].Z, t);
					Z = FMath::Max(Z, zc + FMath::Sqrt(r * r - d * d) * 0.75f + 1.f);
				}
			}
			// the edges fall to the mattress (and a little over it)
			const float Edge = FMath::Min(FMath::Min((float)j, (float)(NV - j)), (float)(NU - i)) / 2.5f;
			H[j * (NU + 1) + i] = Edge < 1.f ? FMath::Lerp(-4.f, Z, Edge) : Z;
		}
	}
	// soften: a cloth, not a mould
	for (int32 Pass = 0; Pass < 7; Pass++)
	{
		TArray<float> G = H;
		for (int32 j = 1; j < NV; j++)
		{
			for (int32 i = 1; i < NU; i++)
			{
				const int32 k = j * (NU + 1) + i;
				G[k] = FMath::Max(H[k] * 0.4f + (H[k - 1] + H[k + 1] + H[k - NU - 1] + H[k + NU + 1]) * 0.15f, 2.f);
			}
		}
		H = G;
	}
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector3f> Nrm = Attr.GetVertexInstanceNormals();
	const FPolygonGroupID Group = MD.CreatePolygonGroup();
	TArray<FVertexInstanceID> Inst;
	auto At = [&](int32 i, int32 j) { return FVector(FMath::Lerp(UStart, U1, i / (float)NU), VMid + FMath::Lerp(-Half, Half, j / (float)NV), H[j * (NU + 1) + i]); };
	for (int32 j = 0; j <= NV; j++)
	{
		for (int32 i = 0; i <= NU; i++)
		{
			const FVertexID Vx = MD.CreateVertex();
			Pos[Vx] = FVector3f(At(i, j));
			const FVertexInstanceID In = MD.CreateVertexInstance(Vx);
			UVs.Set(In, 0, FVector2f(i / (float)NU * 2.f, j / (float)NV));
			const FVector N = FVector::CrossProduct(At(FMath::Min(i + 1, NU), j) - At(FMath::Max(i - 1, 0), j),
				At(i, FMath::Min(j + 1, NV)) - At(i, FMath::Max(j - 1, 0))).GetSafeNormal();
			Nrm[In] = FVector3f(N.Z < 0.f ? -N : N);
			Inst.Add(In);
		}
	}
	for (int32 j = 0; j < NV; j++)
	{
		for (int32 i = 0; i < NU; i++)
		{
			const FVertexInstanceID a = Inst[j * (NU + 1) + i], b = Inst[j * (NU + 1) + i + 1], c = Inst[(j + 1) * (NU + 1) + i], d = Inst[(j + 1) * (NU + 1) + i + 1];
			// both windings: the linen's material is one-sided, the cloth is seen from above and at its edges from below
			MD.CreateTriangle(Group, { a, c, b });
			MD.CreateTriangle(Group, { b, c, d });
			MD.CreateTriangle(Group, { a, b, c });
			MD.CreateTriangle(Group, { b, d, c });
		}
	}
	if (!BlanketMesh)
	{
		BlanketMesh = NewObject<UStaticMesh>(this);
		static UMaterialInterface* Linen = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Academgorodok/Materials/MI_AC_FLinen.MI_AC_FLinen"));
		BlanketMesh->GetStaticMaterials().Add(FStaticMaterial(Linen ? Linen : UMaterial::GetDefaultMaterial(MD_Surface)));
	}
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	BlanketMesh->BuildFromMeshDescriptions({ &MD }, Params);
	Blanket->SetStaticMesh(BlanketMesh);
	// the field is in the bed's frame (U, V, up) over the mattress's top
	Blanket->SetWorldLocationAndRotation(Top, FRotationMatrix::MakeFromXY(U, V).Rotator());
}

// --- console -----------------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GPeopleReport(TEXT("lyud.People.Report"), TEXT("lyud.People.Report [key]: who does what (or one person in detail)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World))
		{
			if (Args.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("People: %s"), *People->Describe(People->FindPerson(Args[0])));
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("People: %s"), *People->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPeopleOrder(TEXT("lyud.People.Order"), TEXT("lyud.People.Order <key> <activity> [place]: send someone to do something now."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		if (People && Args.Num() >= 2)
		{
			const bool bOk = People->Order(People->FindPerson(Args[0]), Args[1], Args.Num() > 2 ? Args[2] : FString());
			UE_LOG(LogTemp, Warning, TEXT("People: order %s %s: %s"), *Args[0], *Args[1], bOk ? TEXT("going") : TEXT("can't"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPeopleFlats(TEXT("lyud.People.Flats"), TEXT("lyud.People.Flats: who lives where."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World))
		{
			for (const TPair<FString, FString>& F : People->Flats())
			{
				if (!F.Value.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("People: %s: %s"), *F.Key, *F.Value);
				}
			}
		}
	}));
