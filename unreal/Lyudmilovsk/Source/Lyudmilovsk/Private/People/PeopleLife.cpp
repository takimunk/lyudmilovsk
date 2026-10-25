#include "People/PeopleLife.h"

#include "PeopleSim.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "BoothOwner.h"
#include "Components/SkeletalMeshComponent.h"
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
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	// outdoors the sun lights channel 1 (ALyudSky), indoors the lamps channel 0
	Mesh->LightingChannels.bChannel0 = true;
	Mesh->LightingChannels.bChannel1 = true;
	Mesh->bCastDynamicShadow = true;
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
	// the shared clips were made on a 1.69 m adult (pelvis 91.7 cm over the floor): the pelvis moves scale with the body
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	int32 Bone = Ref.FindBoneIndex(TEXT("pelvis"));
	FTransform T = FTransform::Identity;
	while (Bone != INDEX_NONE)
	{
		T = T * Ref.GetRefBonePose()[Bone];
		Bone = Ref.GetParentIndex(Bone);
	}
	BodyScale = FMath::Clamp((float)T.GetLocation().Z / 91.7f, 0.5f, 1.3f);
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
	if (Playing != Name)
	{
		Playing = Name;
		if (UAnimSequence* A = Clip(Name))
		{
			Mesh->PlayAnimation(A, bLoop);
		}
	}
	Mesh->SetPlayRate(Rate);
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
		// the shared walk is one stride at ~125 cm/s; the owner's (the fallback) at 75
		const bool bShared = Clip(TEXT("Walk")) && Clip(TEXT("Walk"))->GetName().StartsWith(TEXT("A_Human"));
		Play(TEXT("Walk"), true, FMath::Clamp(Speed / (bShared ? 125.f * S : 75.f), 0.3f, 2.5f));
		break;
	}
	case EPersonPose::Talk: Play(TEXT("Talk"), true); break;
	case EPersonPose::Look: Play(TEXT("StandIdle"), true, 0.7f); break;
	case EPersonPose::Sit: Play(TEXT("SitIdle"), true); break;
	case EPersonPose::Computer: Play(TEXT("Computer"), true); break;
	case EPersonPose::Eat: Play(TEXT("Eat"), true); break;
	case EPersonPose::Read: Play(TEXT("Read"), true); break;
	case EPersonPose::Sleep: Play(TEXT("Sleep"), true); break;
	case EPersonPose::Work: Play(TEXT("Computer"), true); break;
	default: Play(TEXT("StandIdle"), true); break;
	}
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
