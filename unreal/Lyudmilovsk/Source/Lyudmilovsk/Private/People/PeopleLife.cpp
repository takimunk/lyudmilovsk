#include "People/PeopleLife.h"
#include "World/LyudPrefetch.h"
#include "People/LyudBodyAnim.h"
#include "Transport/LyudTransport.h"
#include "Bikes/LyudBike.h"
#include "Bikes/LyudBikeSubsystem.h"

#include "PeopleSim.h"
#include "PeopleManners.h"
#include "BodyGestures.h"
#include "UI/LyudGameScreen.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
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
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "NpcDialogue.h"
#include "LyudText.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Plants/GameCalendar.h"
#include "Sky/Ephemeris.h"
#include "Weather/LyudWeather.h"
#include "Winter/LyudWinter.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#if __has_include("Railway/LyudRailway.h") && __has_include("../Railway/RailRun.h")
#include "Railway/LyudRailway.h"
#include "Railway/LyudTrain.h"
#include "../Railway/RailRun.h"
#define LYUD_HAS_RAILWAY 1
#else
#define LYUD_HAS_RAILWAY 0
#endif
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
	TEXT("People outdoors are drawn within this many metres of the camera (indoors within 40 m; graphics levels: View distance)."), ECVF_Scalability);
static TAutoConsoleVariable<int32> CVarPeopleTownBodies(TEXT("lyud.People.TownBodies"), 48,
	TEXT("At most this many townsfolk are drawn at once, the nearest to the camera (docs/town_life.md); the rest live unseen."));
static TAutoConsoleVariable<float> CVarPeopleTownDraw(TEXT("lyud.People.TownDrawDistance"), 110.f,
	TEXT("Townsfolk are drawn within this many metres of the camera (the story's people: lyud.People.DrawDistance)."));
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

	/** A person's model: /Game/Characters/Humans/<Model>/SK_<Model>. */
	FString PeopleModelPath(const FString& Model)
	{
		// "Dir/Name" (the townsfolk's own: Town/R_amada_tomohiko) -> /Game/Characters/Humans/Dir/Name/SK_Name
		const FString Name = FPaths::GetCleanFilename(Model);
		return FString::Printf(TEXT("/Game/Characters/Humans/%s/SK_%s.SK_%s"), *Model, *Name, *Name);
	}

	/** Whose look a skeletal mesh is: <Model> of /Game/Characters/Humans/<Model>/SK_<Model>, else "" (not a person's model). */
	FString PeopleModelOf(const USkeletalMeshComponent* C)
	{
		const USkeletalMesh* M = C ? C->GetSkeletalMeshAsset() : nullptr;
		if (!M)
		{
			return FString();
		}
		const FString Root = TEXT("/Game/Characters/Humans/");
		const FString Path = M->GetPathName();
		if (!Path.StartsWith(Root))
		{
			return FString();
		}
		// the folder up to the mesh: <Model>/SK_<Model> or Dir/<Name>/SK_<Name>
		FString Folder = FPaths::GetPath(Path.Mid(Root.Len()));
		return !Folder.IsEmpty() && M->GetName() == TEXT("SK_") + FPaths::GetCleanFilename(Folder) ? Folder : FString();
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
	W->Opt.SettleBatch = 120;      // the townsfolk placed over a few frames after a load or a clock jump
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("PeopleData"));
	const FString Animals = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("AnimalsData"));
	if (!W->Load(TCHAR_TO_UTF8(*Dir), TCHAR_TO_UTF8(*Animals)))
	{
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: %s"), UTF8_TO_TCHAR(W->LastError.c_str()));
		return false;
	}
	Sim = MoveTemp(W);
	FlatIndex.Reset();
	WindowIndex.Reset();
	for (int32 I = 0; I < (int32)Sim->Places.size(); I++)
	{
		const LP::FPlace& Pl = Sim->Places[I];
		if (!Pl.Windows.empty())
		{
			FlatIndex.Add(FName(UTF8_TO_TCHAR(Pl.Id.c_str())), I);
			for (const std::string& Win : Pl.Windows)
			{
				WindowIndex.Add(FName(UTF8_TO_TCHAR(Win.c_str())), I);
			}
		}
	}
	LoadManners();
	TownAllowed.Init(false, (int32)Sim->People.size());
	bLightSinksBound = false;
	LightSinks.Reset();
	LightTimer = 0.f;
	UpdateClock(true);
	ResolveModels();
	BindClub();
	BindRailway();
	BindWinter();
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
	for (ANpcBody* Body : TownBodies)
	{
		if (IsValid(Body))
		{
			Body->Destroy();
		}
	}
	TownBodies.Reset();
	TownPool.Reset();
	TownNear.Reset();
	Manners.Reset();
	if (PropLoad.IsValid())
	{
		PropLoad->CancelHandle();
		PropLoad.Reset();
	}
	PropMeshes.Reset();
	EaselMesh = nullptr;
	SetLightsDriven(false);     // the buildings light themselves again (a seeded evening)
	LightSinks.Reset();
	bLightSinksBound = false;
	LastLightBind = -1e9;
	for (const TPair<FString, TObjectPtr<AActor>>& Pair : ClubActors)
	{
		ShowPlaced(Pair.Value, true);
	}
	ClubActors.Reset();
	ClubSlotActors.Reset();
	GroundCache.Reset();
	for (AActor* Stray : StrayActors)
	{
		ShowPlaced(Stray, true);
	}
	StrayActors.Reset();
	ShownModels.Reset();
	ModelOwners.Reset();
	if (Owner)
	{
		Owner->SetAway(false);
		Owner = nullptr;
	}
	Talkable.Reset();
	TrainSeats.Reset();
	bRailSynced = false;
	Sim.Reset();
	bStarted = false;
}

// --- the sea railway ----------------------------------------------------------------------------------

void UPeopleLifeSubsystem::BindRailway()
{
#if LYUD_HAS_RAILWAY
	// the timetable on the life's clock: the railway's NowSeconds counts whole days of the calendar, the life's from the
	// start date; RailOffset (UpdateRailway, every tick) is the difference
	TWeakObjectPtr<UPeopleLifeSubsystem> Self(this);
	auto Stops = [Self](const std::string& From, const std::string& To, int32& A, int32& B, int32& Dir) -> const ULyudRailwaySubsystem*
	{
		const ULyudRailwaySubsystem* R = Self.IsValid() ? ULyudRailwaySubsystem::Get(Self.Get()) : nullptr;
		if (!R || !R->IsRunning() || !R->GetRun())
		{
			return nullptr;
		}
		A = R->FindStop(FName(UTF8_TO_TCHAR(From.c_str())));
		B = R->FindStop(FName(UTF8_TO_TCHAR(To.c_str())));
		const std::vector<LyudRail::FStop>& S = R->GetRun()->Params().Stops;
		if (A < 0 || B < 0 || A >= (int32)S.size() || B >= (int32)S.size() || A == B)
		{
			return nullptr;
		}
		Dir = S[B].S > S[A].S ? 1 : -1;
		return R;
	};
	// (the life's clock is negative before the start date: the doors' time is returned through Doors, never as -1)
	// asked from every score of every island activity: one line when the trains stop answering, not one per question
	TSharedRef<bool> bSaidNoTrain = MakeShared<bool>(false);
	Sim->TrainNext = [Self, Stops, bSaidNoTrain](const std::string& From, const std::string& To, double Time, double& Doors) -> bool
	{
		int32 A = -1, B = -1, Dir = 1;
		const ULyudRailwaySubsystem* R = Stops(From, To, A, B, Dir);
		if (!R)
		{
			if (!*bSaidNoTrain)
			{
				*bSaidNoTrain = true;
				UE_LOG(LogTemp, Log, TEXT("PeopleLife: no train %hs -> %hs (stops %d %d); quiet until the trains answer again"), From.c_str(), To.c_str(), A, B);
			}
			return false;
		}
		*bSaidNoTrain = false;
		const double Off = Self->RailOffset;
		const double When = R->NextDoorsOpen(A, Dir, Time + Off);
		Doors = When - Off;
		return When >= 0.0;
	};
	Sim->TrainRide = [Stops](const std::string& From, const std::string& To) -> double
	{
		int32 A = -1, B = -1, Dir = 1;
		const ULyudRailwaySubsystem* R = Stops(From, To, A, B, Dir);
		return R ? R->RideSeconds(A, B, Dir) : -1.0;
	};
#endif
}

void UPeopleLifeSubsystem::UpdateRailway()
{
#if LYUD_HAS_RAILWAY
	const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	if (!R || !R->IsRunning())
	{
		return;
	}
	RailOffset = R->NowSeconds() - LastSeconds;
	if (bRailSynced)
	{
		return;
	}
	// the halts where the railway has them: the stair's foot, the boarding points along the deck
	bRailSynced = true;
	for (int32 Stop = 0; Stop < R->NumStops(); Stop++)
	{
		// the life's frame is Unreal metres (world cm / 100: people_places.json "frame": "unreal_metres", PeopleToUE),
		// not the map's metres (those are 7.9 m off in Y: the portal face)
		const FVector Foot = R->GetStopEntrance(Stop) / 100.0;
		std::vector<LP::V3> Board;
		for (int32 K = 0; K < 8; K++)
		{
			const FVector B = R->GetBoardingPoint(Stop, K) / 100.0;
			Board.emplace_back(B.X, B.Y, B.Z);
		}
		if (!Sim->SetRailStop(TCHAR_TO_UTF8(*R->GetStopId(Stop).ToString()), LP::V3(Foot.X, Foot.Y, Foot.Z), Board))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: no halt %s in people_places.json (tools/people_map.py)"), *R->GetStopId(Stop).ToString());
		}
	}
#endif
}

void UPeopleLifeSubsystem::PresentTrainRider(int32 Id, const FVector& Cam, float Draw, float DeltaTime)
{
	ANpcBody* Body = Id < Bodies.Num() ? Bodies[Id].Get() : nullptr;
#if LYUD_HAS_RAILWAY
	const LP::FPerson& P = Sim->People[Id];
	const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	FIntPoint* Seat = TrainSeats.Find(Id);
	if (R && R->IsRunning() && R->NumTrains() > 0)
	{
		if (!Seat)
		{
			// the train standing at their halt (else the first), a free seat (spread over the car from a seat of their own)
			const int32 From = P.RailFrom >= 0 ? R->FindStop(FName(UTF8_TO_TCHAR(Sim->Places[P.RailFrom].Stop.c_str()))) : -1;
			int32 Train = 0;
			for (int32 T = 0; T < R->NumTrains(); T++)
			{
				const ALyudTrain* Tr = R->GetTrain(T);
				if (Tr && From >= 0 && Tr->GetInfo().Stop == From)
				{
					Train = T;
				}
			}
			const ALyudTrain* Tr = R->GetTrain(Train);
			const int32 N = Tr ? Tr->NumSeats() : 0;
			int32 Pick = -1;
			for (int32 K = 0; K < N && Pick < 0; K++)
			{
				const int32 S = (Id * 7 + K * 5) % N;
				bool bTaken = false;
				for (const TPair<int32, FIntPoint>& Other : TrainSeats)
				{
					bTaken |= Other.Value == FIntPoint(Train, S);
				}
				Pick = bTaken ? -1 : S;
			}
			Seat = &TrainSeats.Add(Id, FIntPoint(Train, Pick));
		}
		const ALyudTrain* Tr = R->GetTrain(Seat->X);
		FTransform T;
		if (Tr && Seat->Y >= 0 && Tr->GetInfo().bVisible && Tr->GetSeat(Seat->Y, T) && FVector::Dist(T.GetLocation(), Cam) < Draw)
		{
			// the seat's frame is on the floor under its front edge; the body sits over the floor under the seat's middle
			const FVector At = T.GetLocation() - T.GetUnitAxis(EAxis::X) * 20.0;
			Body = BodyFor(Id);
			if (Body)
			{
				Body->SetActorHiddenInGame(false);
				Body->Drive(At, T.Rotator().Yaw, EPersonPose::Sit, 0, 0.f, ALyudTrain::SeatHeight, DeltaTime, true);
			}
			return;
		}
	}
#endif
	if (Body)
	{
		Body->SetActorHiddenInGame(true);
	}
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
		if (H.Key.StartsWith(TEXT("t_")))
		{
			continue;       // a townsperson's household: their flat is townsfolk.json's (AssignHomes)
		}
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
			if (P.bTown)
			{
				continue;       // hundreds of them: homes from townsfolk.json, needs guessed from the hour (Resettle)
			}
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

void UPeopleLifeSubsystem::BindWinter()
{
	// the shovel activity (activities.json "snow"): the winter's dirty path patches as stretches to clear
	TWeakObjectPtr<UPeopleLifeSubsystem> Self(this);
	Sim->SnowJobs = [Self](const LP::V3& Near, int Max, std::vector<LP::FWorld::FSnowJob>& Out)
	{
		ULyudWinterSubsystem* W = Self.IsValid() ? ULyudWinterSubsystem::Get(Self.Get()) : nullptr;
		if (!W || !W->IsReady())
		{
			return;
		}
		TArray<int32> Patches;
		W->FindDirtyPatches(PeopleToUE(Near), 25000.f, Max, 4.f, Patches);
		for (const int32 P : Patches)
		{
			FVector A, B;
			if (W->GetPatchLine(P, A, B))
			{
				LP::FWorld::FSnowJob J;
				J.Job = P;
				J.From = PeopleFromUE(A);
				J.To = PeopleFromUE(B);
				J.Cm = W->GetPatchSnow(P);
				Out.push_back(J);
			}
		}
	};
	Sim->SnowProgress = [Self](int Person, int Job, float Progress) -> bool
	{
		ULyudWinterSubsystem* W = Self.IsValid() ? ULyudWinterSubsystem::Get(Self.Get()) : nullptr;
		if (!W)
		{
			return false;
		}
		if (Progress < 0.f)
		{
			W->ReleasePatch(Job, Person);
			return true;
		}
		if (Progress == 0.f && !W->ClaimPatch(Job, Person))
		{
			return false;
		}
		W->SetPatchProgress(Job, Progress);
		if (Progress >= 1.f)
		{
			W->ReleasePatch(Job, Person);
		}
		return true;
	};
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
	// in the rain people hurry along outdoors
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(GetWorld());
	S.bRain = Weather && Weather->GetSky().Rain > 0.25f;
	// and choose what to do by the weather (a bench in the drizzle, the beach in the frost: PeopleSim's Score)
	S.Rain = Weather ? Weather->GetSky().Rain : 0.f;
	S.TempC = Weather ? Weather->GetSky().TempC : 12.f;
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
	// the scene's people: every placed actor with somebody's model (people.json's people, ResolveModels)
	struct FPlacedPerson
	{
		AActor* Actor;
		int32 Person;
	};
	TArray<FPlacedPerson> Placed;
	for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
	{
		const int32* Who = ModelOwners.Find(PeopleModelOf(It->GetSkeletalMeshComponent()));
		if (Who)
		{
			Placed.Add({ *It, *Who });
		}
	}
	// a station shows its own person's placed actor (the one with their model nearest its chair), nobody else's
	TSet<AActor*> Bound;
	for (const LP::FSlot& Sl : S.Slots)
	{
		if (Sl.Kind != "club_seat")
		{
			continue;
		}
		int32 Sitter = -1;
		for (const LP::FPerson& P : S.People)
		{
			if (P.ClubSeat == Sl.Tag)
			{
				Sitter = P.Id;
				break;
			}
		}
		const FVector Chair = PeopleToUE(Sl.Pos);
		AActor* Best = nullptr;
		double BestD = 150.0;
		for (const FPlacedPerson& A : Placed)
		{
			const double D = FVector::Dist2D(A.Actor->GetActorLocation(), Chair);
			if (A.Person == Sitter && !Bound.Contains(A.Actor) && D < BestD)
			{
				BestD = D;
				Best = A.Actor;
			}
		}
		if (Best)
		{
			ClubActors.Add(UTF8_TO_TCHAR(Sl.Tag.c_str()), Best);
			Bound.Add(Best);
		}
	}
	// the same by slot (each club seat: its tag's actor), for Present
	ClubSlotActors.Reset();
	ClubSlotActors.SetNum((int32)S.Slots.size());
	for (int32 I = 0; I < (int32)S.Slots.size(); I++)
	{
		if (S.Slots[I].Kind == "club_seat")
		{
			ClubSlotActors[I] = ClubActors.FindRef(UTF8_TO_TCHAR(S.Slots[I].Tag.c_str()));
		}
	}
	// any other copy of somebody (their model at a station not theirs, a stand-in left in the level) is never shown
	for (const FPlacedPerson& A : Placed)
	{
		if (!Bound.Contains(A.Actor))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: %s in the level shows %hs away from their station: hidden (one person, one body)"),
				*A.Actor->GetActorNameOrLabel(), S.People[A.Person].Key.c_str());
			StrayActors.Add(A.Actor);
			ShowPlaced(A.Actor, false);
		}
	}
}

static void UPeopleLifeSubsystem_TownModels(const LP::FWorld& S, TArray<FString>& ShownModels);

void UPeopleLifeSubsystem::ResolveModels()
{
	const LP::FWorld& S = *Sim;
	ShownModels.Init(FString(), (int32)S.People.size());
	ModelOwners.Reset();
	auto Exists = [](const FString& Model)
	{
		return !Model.IsEmpty() && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(PeopleModelPath(Model)));
	};
	// their own models first: the body made for them (the club's people's mesh is their own)
	for (const LP::FPerson& P : S.People)
	{
		if (P.bTown)
		{
			continue;
		}
		const FString Own = UTF8_TO_TCHAR(P.Body.c_str());
		if (!Exists(Own))
		{
			continue;
		}
		if (const int32* Other = ModelOwners.Find(Own))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: %hs and %hs have the same body %s in people.json: only %hs is shown"),
				S.People[*Other].Key.c_str(), P.Key.c_str(), *Own, S.People[*Other].Key.c_str());
			continue;
		}
		ModelOwners.Add(Own, P.Id);
		ShownModels[P.Id] = Own;
	}
	// then those whose body isn't made yet: the stand-in people.json names, only while it is nobody else's face
	for (const LP::FPerson& P : S.People)
	{
		if (!ShownModels[P.Id].IsEmpty() || P.bTown)
		{
			continue;
		}
		const FString Stand = UTF8_TO_TCHAR(P.Mesh.c_str());
		const int32* Other = ModelOwners.Find(Stand);
		if (Exists(Stand) && !Other)
		{
			ModelOwners.Add(Stand, P.Id);
			ShownModels[P.Id] = Stand;
			continue;
		}
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: %hs has no model of their own yet (%s) and %s is %hs: lives unseen"),
			P.Key.c_str(), *PeopleModelPath(UTF8_TO_TCHAR(P.Body.c_str())), *Stand,
			Other ? S.People[*Other].Key.c_str() : "missing");
	}
	UPeopleLifeSubsystem_TownModels(S, ShownModels);
}

static void UPeopleLifeSubsystem_TownModels(const LP::FWorld& S, TArray<FString>& ShownModels)
{
	// the townsfolk borrow models from the shared pool for now (a generator will make their own): many of a look, never
	// one of the story's people's registry (ModelOwners), and drawn only near the camera
	TMap<FString, bool> Known;
	int32 Unseen = 0;
	// the townsfolk's own models, all in one registry query (not a file check each)
	TSet<FName> OwnPackages;
	if (IAssetRegistry* Registry = IAssetRegistry::Get())
	{
		TArray<FAssetData> Assets;
		// an uncooked -game run (offscreen tests, standalone) hasn't scanned the content: that folder once, synchronously
		if (!GIsEditor && !FPlatformProperties::RequiresCookedData())
		{
			Registry->ScanPathsSynchronous({ TEXT("/Game/Characters/Humans/Town") }, false);
		}
		Registry->GetAssetsByPath(FName(TEXT("/Game/Characters/Humans/Town")), Assets, true);
		for (const FAssetData& A : Assets)
		{
			OwnPackages.Add(A.PackageName);
		}
	}
	for (const LP::FPerson& P : S.People)
	{
		if (!P.bTown)
		{
			continue;
		}
		// their own model (the residents generator's, town_looks.json -> "body") once imported, else the pool's look
		const FString Own = P.Body != P.Mesh ? FString(UTF8_TO_TCHAR(P.Body.c_str())) : FString();
		if (!Own.IsEmpty() && OwnPackages.Contains(FName(*FPackageName::ObjectPathToPackageName(PeopleModelPath(Own)))))
		{
			ShownModels[P.Id] = Own;
			continue;
		}
		const FString Model = UTF8_TO_TCHAR(P.Mesh.c_str());
		bool* Has = Known.Find(Model);
		if (!Has)
		{
			Has = &Known.Add(Model, !Model.IsEmpty() && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(PeopleModelPath(Model))));
		}
		if (*Has)
		{
			ShownModels[P.Id] = Model;
		}
		else
		{
			Unseen++;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("PeopleLife: townsfolk use %d models of the pool (%d live unseen: model missing)"), Known.Num(), Unseen);
}

FString UPeopleLifeSubsystem::ShownModel(int32 Id) const
{
	return ShownModels.IsValidIndex(Id) ? ShownModels[Id] : FString();
}

int32 UPeopleLifeSubsystem::ModelOwner(const FString& Model) const
{
	const int32* Who = ModelOwners.Find(Model);
	return Who ? *Who : -1;
}

FString UPeopleLifeSubsystem::InstanceReport(int32& Duplicates) const
{
	Duplicates = 0;
	const UWorld* World = GetWorld();
	if (!Sim || !World)
	{
		return TEXT("not running");
	}
	// every visible human model in the world, whatever actor shows it
	TMap<FString, TArray<FString>> Seen;
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		const USkeletalMeshComponent* C = *It;
		const AActor* A = C->GetOwner();
		if (C->GetWorld() != World || !C->IsRegistered() || !C->IsVisible() || !A || A->IsHidden() || A->ActorHasTag(TEXT("Townsfolk")))
		{
			continue;       // (the townsfolk share the pool's models on purpose: docs/town_life.md)
		}
		const FString Model = PeopleModelOf(C);
		if (Model.IsEmpty())
		{
			continue;
		}
		const int32 Who = ModelOwner(Model);
		const FString Key = Who >= 0 ? FString(UTF8_TO_TCHAR(Sim->People[Who].Key.c_str())) : TEXT("nobody:") + Model;
		const FVector L = C->GetComponentLocation();
		Seen.FindOrAdd(Key).Add(FString::Printf(TEXT("%s (%.0f %.0f %.0f)"), *A->GetActorNameOrLabel(), L.X, L.Y, L.Z));
	}
	TArray<FString> Lines;
	for (const TPair<FString, TArray<FString>>& Pair : Seen)
	{
		Duplicates += Pair.Value.Num() > 1 ? 1 : 0;
		Lines.Add(FString::Printf(TEXT("%s%s x%d at %s"), Pair.Value.Num() > 1 ? TEXT("TWICE ") : TEXT(""), *Pair.Key, Pair.Value.Num(),
			*FString::Join(Pair.Value, TEXT(", "))));
	}
	Lines.Sort([](const FString& A, const FString& B) { return A.StartsWith(TEXT("TWICE")) != B.StartsWith(TEXT("TWICE")) ? A.StartsWith(TEXT("TWICE")) : A < B; });
	return FString::Printf(TEXT("%d seen, %d twice; "), Seen.Num(), Duplicates) + FString::Join(Lines, TEXT("; "));
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
		// the one model that is theirs (ResolveModels): none, and they aren't drawn in somebody else's face
		const FString Model = ShownModel(Id);
		if (Model.IsEmpty() || (!P.bTown && ModelOwner(Model) != Id))
		{
			return nullptr;
		}
		// a townsperson: a free body of their look from the pool, if there is one
		if (P.bTown)
		{
			if (TArray<TObjectPtr<ANpcBody>>* Free = TownPool.Find(Model))
			{
				while (Free->Num() > 0)
				{
					ANpcBody* Pooled = Free->Pop(EAllowShrinking::No);
					if (IsValid(Pooled))
					{
						Bodies[Id] = Pooled;
						return Pooled;
					}
				}
			}
		}
		// their model and the shared clips loaded in the background first: a body made on the spot stalls the frame
		const bool bModel = ULyudPrefetch::Ready(this, PeopleModelPath(Model));
		const bool bClips = ULyudPrefetch::FolderReady(this, TEXT("/Game/Characters/Humans/Anims"));
		if (!bModel || !bClips)
		{
			return nullptr;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANpcBody* Body = GetWorld()->SpawnActor<ANpcBody>(PeopleToUE(P.Pos), FRotator(0.f, P.Yaw, 0.f), SP);
		if (!Body)
		{
			return nullptr;
		}
		if (!Body->Setup(Model, UTF8_TO_TCHAR(P.ComputerClip.c_str())))
		{
			UE_LOG(LogTemp, Warning, TEXT("PeopleLife: no model for %s (%s)"), UTF8_TO_TCHAR(P.Key.c_str()), *Model);
		}
		// a townsperson's own look: the body holds its mesh now; once the body goes (ReleaseBody) it can be freed
		if (P.bTown && Model != UTF8_TO_TCHAR(P.Mesh.c_str()))
		{
			ULyudPrefetch::Release(this, PeopleModelPath(Model));
		}
		Body->TalkId = UTF8_TO_TCHAR(P.Body.c_str());
		if (P.Key.rfind("crowd", 0) == 0 || P.bTown)
		{
			Body->Tags.Add(TEXT("NoTalk"));
		}
		if (P.bTown)
		{
			// many of them about: cheaper animation far off, out of Lumen's scene (perf checklist: moving meshes)
			Body->Tags.Add(TEXT("Townsfolk"));
			Body->Mesh->bEnableUpdateRateOptimizations = true;
			Body->Mesh->bAffectDynamicIndirectLighting = false;
			Body->Mesh->bAffectDistanceFieldLighting = false;
			TownBodies.Add(Body);
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
	TownPickTimer -= DeltaTime;
	if (TownPickTimer <= 0.f)
	{
		TownPickTimer = 0.25f;
		PickTownBodies(Cam, FMath::Min(Draw, CVarPeopleTownDraw.GetValueOnGameThread() * 100.f));
	}
	PlacedShown.Reset();
	// manners (docs/town_manners.md): whoever is drawn by Drive below keeps or picks theirs
	LP::FManners* Mn = Manners.Get();
	LP::FMannerNow MNow;
	ShownIds.Reset();
	if (Mn)
	{
		MannerClock += DeltaTime;
		MNow.Clock = MannerClock;
		MNow.Rain = S.Rain;
		MNow.TempC = S.TempC;
		const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(World);
		MNow.Cloud = Weather ? Weather->GetSky().Cloud : 0.3f;
		// the sun's height near enough for a hand over the eyes: an arc from sunrise to sunset
		MNow.SunAlt = S.bSunUp && S.Sunset > S.Sunrise ? 38.f * FMath::Sin(UE_PI * (S.Hour - S.Sunrise) / (S.Sunset - S.Sunrise)) : -10.f;
	}
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
			if (AActor* Club = ClubSlotActors.IsValidIndex(P.Slot) ? ClubSlotActors[P.Slot].Get() : nullptr)
			{
				PlacedShown.Add(Club);
				if (P.Id < Bodies.Num() && Bodies[P.Id])
				{
					Bodies[P.Id]->SetActorHiddenInGame(true);
				}
				continue;
			}
		}
		// riding the train: seated in it while it is drawn
		if (P.State == LP::EState::Riding && P.bTrain)
		{
			PresentTrainRider(P.Id, Cam, Draw, DeltaTime);
			continue;
		}
		TrainSeats.Remove(P.Id);
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
		// inside a town building (a shell: no rooms to walk), drawn by somebody else's actor (a waiter at the restaurant),
		// or a townsperson not among the nearest few: not drawn
		const bool bOut = S.InsideShell(P) || P.bExternal || (P.bTown && !TownAllowed[P.Id]);
		const bool bShow = !bOut && P.Pose != LP::EPose::Hidden && Dist < (P.bIndoors ? 4000.f : Draw);
		if (P.bTown && !bShow)
		{
			ReleaseBody(P.Id);
			continue;
		}
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
			// exact ground under them near the camera (steps, kerbs); someone standing still on the same spot keeps the last
			// trace's ground for a little while
			if (GroundCache.Num() <= P.Id)
			{
				GroundCache.SetNum(P.Id + 1);
			}
			FGroundCache& G = GroundCache[P.Id];
			G.Age += DeltaTime;
			if (!G.At.Equals(Loc, 0.5f) || G.Age > 0.25f)
			{
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(PeopleGround), false);
				Q.AddIgnoredActor(Body);
				G.bHit = World->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 120), Loc - FVector(0, 0, 250), ECC_Visibility, Q);
				G.Z = G.bHit ? (float)Hit.ImpactPoint.Z : 0.f;
				G.At = Loc;
				G.Age = 0.f;
			}
			if (G.bHit)
			{
				At.Z = G.Z;
			}
		}
		const float SeatH = Sl && P.State != LP::EState::Going ? Sl->Height * 100.f : 0.f;
		// with their bicycle in hand (Bikes/LyudBikeSubsystem): the bike's side places the body on it or beside it
		ULyudBikeSubsystem* BikeSys = ULyudBikeSubsystem::Get(this);
		if (BikeSys && P.Bike >= 0 && (P.BikeMode > 0 || P.BikeAct != LP::EBikeAct::None))
		{
			BikeSys->PresentPerson(P.Id, Body, DeltaTime);
		}
		else
		{
			float Yaw = P.Yaw;
			if (Mn)
			{
				ShownIds.Add(P.Id);
				Mn->Update(S, P.Id, MNow);
				const LP::FMannerState& St = Mn->States[P.Id];
				const LP::FMannerDef* Def = Mn->DefOf(St);
				const int32 Prop = Def ? (int32)Def->Prop : 0;
				const bool bNoEnd = (Def && Def->bForce) || Mn->Forced != LP::EManner::Num;
				Body->SetManner((uint8)St.Cur, (uint8)Prop, MNow.Clock - St.Start, bNoEnd ? -1.f : St.Until - MNow.Clock, Mn->Personas[P.Id].bLeftHanded,
					PropMeshes.IsValidIndex(Prop) ? PropMeshes[Prop].Get() : nullptr, St.Cur == LP::EManner::Paint ? EaselMesh.Get() : nullptr);
				// what they noticed: the head goes there; a child (or anybody run at) turns round to look
				if (St.LookAt >= -1 && MNow.Clock < St.LookUntil)
				{
					FVector Target = FVector::ZeroVector;
					bool bTarget = false;
					if (St.LookAt >= 0 && St.LookAt < (int32)S.People.size())
					{
						Target = PeopleToUE(S.People[St.LookAt].Pos) + FVector(0.f, 0.f, 150.f);
						bTarget = true;
					}
					else if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
					{
						Target = Pawn->GetPawnViewLocation();
						bTarget = true;
					}
					if (bTarget)
					{
						Body->LookAtPoint(Target, St.LookUntil - MNow.Clock);
						if (St.bTurn)
						{
							Yaw = (float)(Target - At).Rotation().Yaw;
						}
					}
				}
				else
				{
					Body->LookAtNothing();
				}
			}
			Body->Drive(At, Yaw, (EPersonPose)(uint8)P.Pose, Transit, P.Speed * 100.f, SeatH, DeltaTime, bWasHidden);
		}
	}
	UpdateManners(DeltaTime);
	for (const TPair<FString, TObjectPtr<AActor>>& Pair : ClubActors)
	{
		ShowPlaced(Pair.Value, PlacedShown.Contains(Pair.Value.Get()));
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
				// by their own id: a resident shown by another's model has no lines of that one's (nor any till npc_dialogue.txt has theirs)
				Director->AddPerson(FName(*Bodies[Id]->TalkId), Bodies[Id], Bodies[Id]->Mesh);
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
		if (E.Kind == LP::EEvent::Ride && E.Person >= 0 && E.Person < (int)S.People.size() && S.People[E.Person].bTrain)
		{
			const LP::FPerson& P = S.People[E.Person];
			UE_LOG(LogTemp, Log, TEXT("PeopleLife: %hs boards the train at %.0f (doors %.0f, off at %.0f, rail offset %.0f)"), P.Key.c_str(), S.Time,
				P.DoorsAt, P.RideUntil, RailOffset);
		}
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
	const bool bJumped = UpdateClock(false);
	UpdateRailway();
	// the stretches of snowy path near the camera that want clearing (the shovel activity)
	SnowTimer -= DeltaTime;
	if (SnowTimer <= 0.f)
	{
		SnowTimer = 5.f;
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			S.UpdateSnowSlots(PeopleFromUE(PC->PlayerCameraManager->GetCameraLocation()));
		}
	}
	if (bJumped)
	{
		UpdateLights(true);
		Present(DeltaTime);
		return;
	}
	LightTimer -= DeltaTime;
	if (LightTimer <= 0.f)
	{
		LightTimer = 2.f;
		UpdateLights(false);
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

// --- the town ------------------------------------------------------------------------------------------

void UPeopleLifeSubsystem::PickTownBodies(const FVector& Cam, float Draw)
{
	// the nearest townsfolk out and about get a body (lyud.People.TownBodies of them); a few times a second, no allocations
	const LP::FWorld& S = *Sim;
	if (TownAllowed.Num() != (int32)S.People.size())
	{
		TownAllowed.Init(false, (int32)S.People.size());
	}
	TownNear.Reset();
	for (const LP::FPerson& P : S.People)
	{
		TownAllowed[P.Id] = false;
		if (!P.bTown || P.bExternal || P.Pose == LP::EPose::Hidden || S.InsideShell(P) || P.State == LP::EState::Away)
		{
			continue;
		}
		const FVector At = PeopleToUE(P.Pos);
		float D = FVector::Dist(At, Cam);
		// whoever has a body keeps it a little longer (no flicker of bodies passed back and forth in a crowd)
		const ANpcBody* Has = P.Id < Bodies.Num() ? Bodies[P.Id].Get() : nullptr;
		const bool bHas = Has && !Has->IsHidden();
		if (bHas)
		{
			D *= 0.75f;
		}
		if (D >= Draw)
		{
			continue;
		}
		// a townsperson borrows a look from the pool, never next to the one whose own look it is
		if (ShownModels.IsValidIndex(P.Id))
		{
			if (const int32* LookOwner = ModelOwners.Find(ShownModels[P.Id]))
			{
				const ANpcBody* OB = *LookOwner < Bodies.Num() ? Bodies[*LookOwner].Get() : nullptr;
				if (OB && !OB->IsHidden() && FVector::Dist(OB->GetActorLocation(), At) < 6000.f)
				{
					continue;
				}
			}
		}
		TownNear.Emplace(D, P.Id);
	}
	const int32 Max = FMath::Max(0, CVarPeopleTownBodies.GetValueOnGameThread());
	if (TownNear.Num() > Max)
	{
		// a few hundred pairs four times a second: a plain sort is cheap enough
		TownNear.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	}
	for (int32 I = 0; I < FMath::Min(Max, TownNear.Num()); I++)
	{
		TownAllowed[TownNear[I].Value] = true;
	}
}

void UPeopleLifeSubsystem::ReleaseBody(int32 Id)
{
	// a townsperson's body back to the pool of its look (hidden), for whoever of that look comes near next
	ANpcBody* Body = Id < Bodies.Num() ? Bodies[Id].Get() : nullptr;
	if (!Body)
	{
		return;
	}
	Bodies[Id] = nullptr;
	if (Manners)
	{
		Manners->Release(Id, MannerClock);
	}
	// a look of their own nobody else wears: no pool, the body goes (it's loaded again, async, when they come near)
	if (ShownModels.IsValidIndex(Id) && Sim && Id < (int32)Sim->People.size() && ShownModels[Id] != UTF8_TO_TCHAR(Sim->People[Id].Mesh.c_str()))
	{
		TownBodies.RemoveSwap(Body, EAllowShrinking::No);
		Body->Destroy();
		return;
	}
	Body->SetActorHiddenInGame(true);
	TownPool.FindOrAdd(Body->MeshName).Add(Body);
}

void UPeopleLifeSubsystem::BindLights()
{
	// the window lights live in other threads' code (the buildings' ULyudWindowLights, the terrace walls'
	// ULyudCityWindowsSubsystem): found by name and called by reflection, so neither side needs the other to compile
	bLightSinksBound = true;
	LightSinks.Reset();
	UWorld* World = GetWorld();
	for (const TCHAR* Name : { TEXT("LyudWindowLights"), TEXT("LyudCityWindowsSubsystem") })
	{
		UClass* C = FindFirstObject<UClass>(Name, EFindFirstObjectOptions::NativeFirst);
		if (!C || !C->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			continue;
		}
		UWorldSubsystem* Sub = World->GetSubsystemBase(TSubclassOf<UWorldSubsystem>(C));
		UFunction* Fn = Sub ? Sub->FindFunction(TEXT("SetFlatLit")) : nullptr;
		if (!Fn)
		{
			continue;
		}
		LightSinks.Add({ Sub, Fn, FCString::Strcmp(Name, TEXT("LyudWindowLights")) == 0 });
		UE_LOG(LogTemp, Display, TEXT("PeopleLife: window lights driven through %s"), Name);
	}
	SetLightsDriven(true);
}

void UPeopleLifeSubsystem::SetLightsDriven(bool bDriven)
{
	for (const FLightSink& Sink : LightSinks)
	{
		UObject* Obj = Sink.Object.Get();
		UFunction* Driven = Obj ? Obj->FindFunction(TEXT("SetDriven")) : nullptr;
		if (!Driven)
		{
			continue;
		}
		uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max(1, (int32)Driven->ParmsSize));
		FMemory::Memzero(Parms, Driven->ParmsSize);
		for (TFieldIterator<FBoolProperty> It(Driven); It; ++It)
		{
			It->SetPropertyValue_InContainer(Parms, bDriven);
		}
		Obj->ProcessEvent(Driven, Parms);
	}
}

void UPeopleLifeSubsystem::SendFlatLight(const FString& FlatId, bool bOn)
{
	int32 Slash = INDEX_NONE;
	const bool bBuildingFlat = FlatId.FindChar(TEXT('/'), Slash);
	for (const FLightSink& Sink : LightSinks)
	{
		UObject* Obj = Sink.Object.Get();
		if (!Obj || !Sink.Fn || Sink.bBuildings != bBuildingFlat)
		{
			continue;
		}
		uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max(1, (int32)Sink.Fn->ParmsSize));
		FMemory::Memzero(Parms, Sink.Fn->ParmsSize);
		for (TFieldIterator<FProperty> It(Sink.Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parms);
			if (FStrProperty* Str = CastField<FStrProperty>(*It))
			{
				Str->SetPropertyValue_InContainer(Parms, FlatId);
			}
			else if (FNameProperty* Nm = CastField<FNameProperty>(*It))
			{
				Nm->SetPropertyValue_InContainer(Parms, FName(*FlatId));
			}
			else if (FBoolProperty* B = CastField<FBoolProperty>(*It))
			{
				B->SetPropertyValue_InContainer(Parms, bOn);
			}
		}
		Obj->ProcessEvent(Sink.Fn, Parms);
		for (TFieldIterator<FProperty> It(Sink.Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parms);
		}
	}
}

void UPeopleLifeSubsystem::UpdateLights(bool bAll)
{
	LP::FWorld& S = *Sim;
	// the light owners appear with the level's actors: look for both till found (every 10 s); a newcomer gets every flat
	const double Now = FPlatformTime::Seconds();
	if (!bLightSinksBound || (LightSinks.Num() < 2 && Now - LastLightBind > 10.0))
	{
		LastLightBind = Now;
		const int32 Had = LightSinks.Num();
		BindLights();
		bAll = bAll || LightSinks.Num() > Had;
	}
	if (bAll)
	{
		S.FlatLit.assign(S.Places.size(), -1);
	}
	S.UpdateLights(LightChanged);
	for (const int I : LightChanged)
	{
		const FString Id = UTF8_TO_TCHAR(S.Places[I].Id.c_str());
		const bool bOn = S.FlatLit[I] == 1;
		SendFlatLight(Id, bOn);
		OnFlatLight.Broadcast(FName(*Id), bOn);
	}
}

FText UPeopleLifeSubsystem::GetDisplayName(int32 Id) const
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->People.size())
	{
		return FText::GetEmpty();
	}
	return LyudText::Get(FString::Printf(TEXT("npc.%hs.name"), Sim->People[Id].Key.c_str()));
}

bool UPeopleLifeSubsystem::IsTownsperson(int32 Id) const
{
	return Sim && Id >= 0 && Id < (int32)Sim->People.size() && Sim->People[Id].bTown;
}

void UPeopleLifeSubsystem::SetGathering(FName Id, FName PlaceOrKind, float FromHour, float ToHour, float Pull, EPeopleGatherMode Mode, FVector FacePoint)
{
	if (!Sim)
	{
		return;
	}
	const bool bFace = !FacePoint.IsNearlyZero();
	Sim->SetGathering(TCHAR_TO_UTF8(*Id.ToString()), TCHAR_TO_UTF8(*PlaceOrKind.ToString()), FromHour, ToHour, Pull, (LP::EGatherMode)(uint8)Mode,
		bFace, PeopleFromUE(FacePoint));
	UE_LOG(LogTemp, Display, TEXT("PeopleLife: gathering %s at %s %.2f-%.2f pull %.2f"), *Id.ToString(), *PlaceOrKind.ToString(), FromHour, ToHour, Pull);
}

void UPeopleLifeSubsystem::ClearGathering(FName Id)
{
	if (Sim)
	{
		Sim->ClearGathering(TCHAR_TO_UTF8(*Id.ToString()));
	}
}

void UPeopleLifeSubsystem::SetDayOff(bool bOff)
{
	if (Sim)
	{
		Sim->bDayOff = bOff;
	}
}

void UPeopleLifeSubsystem::SetStayUpUntil(float Hour)
{
	if (Sim)
	{
		Sim->SetStayUp(Hour);
	}
}

bool UPeopleLifeSubsystem::IsDayOff() const
{
	return Sim && Sim->bDayOff;
}

TArray<int32> UPeopleLifeSubsystem::StaffOnShift(FName PlaceId, FName Role) const
{
	TArray<int32> Out;
	if (!Sim)
	{
		return Out;
	}
	std::vector<int> Ids;
	Sim->StaffOnShift(TCHAR_TO_UTF8(*PlaceId.ToString()), Role.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Role.ToString())), Ids);
	for (const int Id : Ids)
	{
		Out.Add(Id);
	}
	return Out;
}

void UPeopleLifeSubsystem::SetPresentedExternally(int32 Id, bool bExternal)
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->People.size())
	{
		return;
	}
	Sim->People[Id].bExternal = bExternal;
	if (bExternal && Id < Bodies.Num() && Bodies[Id])
	{
		Bodies[Id]->SetActorHiddenInGame(true);
	}
}

bool UPeopleLifeSubsystem::IsFlatLit(FName FlatId) const
{
	const int32* I = FlatIndex.Find(FlatId);
	return Sim && I && *I < (int32)Sim->FlatLit.size() && Sim->FlatLit[*I] == 1;
}

bool UPeopleLifeSubsystem::IsWindowLit(FName WindowId) const
{
	const int32* I = WindowIndex.Find(WindowId);
	return Sim && I && *I < (int32)Sim->FlatLit.size() && Sim->FlatLit[*I] == 1;
}

int32 UPeopleLifeSubsystem::NumPeople() const
{
	return Sim ? (int32)Sim->People.size() : 0;
}

int32 UPeopleLifeSubsystem::FindPerson(const FString& Key) const
{
	return Sim ? Sim->FindPerson(TCHAR_TO_UTF8(*Key)) : -1;
}

int32 UPeopleLifeSubsystem::FindActivity(const FString& Id) const
{
	if (!Sim)
	{
		return -1;
	}
	const std::string Want = TCHAR_TO_UTF8(*Id);
	for (int32 I = 0; I < (int32)Sim->Activities.size(); I++)
	{
		if (Sim->Activities[I].Id == Want)
		{
			return I;
		}
	}
	return -1;
}

bool UPeopleLifeSubsystem::GetActivityState(int32 Id, int32& Activity, bool& bDoing, EPersonPose& Pose, FVector& Location) const
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->People.size())
	{
		return false;
	}
	const LP::FPerson& P = Sim->People[Id];
	Activity = P.Activity;
	bDoing = P.State == LP::EState::Doing;
	Pose = (EPersonPose)(uint8)P.Pose;
	Location = PeopleToUE(P.Pos);
	return true;
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
	Out.Mesh = ShownModels.IsValidIndex(Id) && !ShownModels[Id].IsEmpty() ? FName(*ShownModels[Id]) : NAME_None;
	Out.Body = FName(UTF8_TO_TCHAR(P.Body.c_str()));
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

// --- manners ------------------------------------------------------------------------------------------

void UPeopleLifeSubsystem::LoadManners()
{
	Manners.Reset();
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("PeopleData/manners.json");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: no %s: people without manners"), *Path);
		return;
	}
	TPimplPtr<LP::FManners> M = MakePimpl<LP::FManners>();
	const FTCHARToUTF8 Utf8(*Text);
	if (!M->Load(std::string(Utf8.Get(), Utf8.Length())))
	{
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: manners.json: %s"), UTF8_TO_TCHAR(M->LastError.c_str()));
		return;
	}
	for (const std::string& Warn : M->Warnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("PeopleLife: manners.json: %s"), UTF8_TO_TCHAR(Warn.c_str()));
	}
	M->Bind(*Sim);
	M->ApplyLikes(*Sim);
	// a manner with something in the hands waits for that thing's mesh (/Game/Characters/Humans/Props/SM_Prop_<name>,
	// blender/humans/props.py): loaded in the background, missing ones are simply never picked
	M->PropsReady = 1u;
	Manners = MoveTemp(M);
	MannerClock = 0.f;
	NoticeTimer = PassingTimer = 0.f;
	PropMeshes.Reset();
	PropMeshes.SetNum((int32)LP::EProp::Num);
	TArray<FSoftObjectPath> Paths;
	auto Want = [&Paths](const FString& Name)
	{
		const FString Package = TEXT("/Game/Characters/Humans/Props/SM_Prop_") + Name;
		if (FPackageName::DoesPackageExist(Package))
		{
			Paths.Add(FSoftObjectPath(Package + TEXT(".SM_Prop_") + Name));
		}
	};
	for (int32 I = 1; I < (int32)LP::EProp::Num; I++)
	{
		Want(UTF8_TO_TCHAR(LP::PropName((LP::EProp)I)));
	}
	Want(TEXT("easel"));
	if (Paths.Num() == 0)
	{
		return;
	}
	PropLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		if (!Manners)
		{
			return;
		}
		int32 Got = 0;
		for (int32 I = 1; I < (int32)LP::EProp::Num && I < PropMeshes.Num(); I++)
		{
			const FString Name = UTF8_TO_TCHAR(LP::PropName((LP::EProp)I));
			PropMeshes[I] = FindObject<UStaticMesh>(nullptr, *(TEXT("/Game/Characters/Humans/Props/SM_Prop_") + Name + TEXT(".SM_Prop_") + Name));
			if (PropMeshes[I])
			{
				Manners->PropsReady |= 1u << I;
				Got++;
			}
		}
		EaselMesh = FindObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Humans/Props/SM_Prop_easel.SM_Prop_easel"));
		UE_LOG(LogTemp, Display, TEXT("PeopleLife: %d of %d manner props"), Got, (int32)LP::EProp::Num - 1);
	}));
}

void UPeopleLifeSubsystem::UpdateManners(float DeltaTime)
{
	LP::FManners* Mn = Manners.Get();
	if (!Mn || ShownIds.Num() == 0)
	{
		return;
	}
	LP::FMannerNow MNow;
	MNow.Clock = MannerClock;
	MNow.Rain = Sim->Rain;
	MNow.TempC = Sim->TempC;
	// a few times a second: what each of them notices of the player (a glance; now and then a word: ULyudTownTalkSubsystem
	// hears hero_runs, hero_close, hero_lamp through OnPersonEvent)
	NoticeTimer -= DeltaTime;
	if (NoticeTimer <= 0.f)
	{
		NoticeTimer = 0.25f;
		UWorld* World = GetWorld();
		LP::FHeroNow Hero;
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Hero.bThere = !Pawn->IsHidden();
			FVector Feet = Pawn->GetActorLocation();
			if (const ACharacter* Ch = Cast<ACharacter>(Pawn))
			{
				Feet.Z -= Ch->GetSimpleCollisionHalfHeight();
				Hero.bCrouch = Ch->bIsCrouched;
			}
			Hero.Pos = PeopleFromUE(Feet);
			Hero.Vel = PeopleFromUE(Pawn->GetVelocity());
			const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
			Hero.bLamp = Screen && Screen->IsHeadLampOn();
		}
		for (const int32 Id : ShownIds)
		{
			Mn->Notice(*Sim, Id, MNow, Hero);
			const LP::ENoticeEvent E = Mn->TakeEvent(Id);
			if (E != LP::ENoticeEvent::None)
			{
				OnPersonEvent.Broadcast(Id, FName(LP::NoticeEventName(E)), -1);
			}
		}
	}
	// and each other as they pass: a glance, a wave between friends
	PassingTimer -= DeltaTime;
	if (PassingTimer <= 0.f)
	{
		PassingTimer = 0.5f;
		Mn->Passing(*Sim, ShownIds.GetData(), ShownIds.Num(), MNow);
	}
}

FName UPeopleLifeSubsystem::GetManner(int32 Id) const
{
	const LP::FMannerState* St = Manners ? Manners->StateOf(Id) : nullptr;
	return St && St->Cur != LP::EManner::None ? FName(LP::MannerName(St->Cur)) : NAME_None;
}

float UPeopleLifeSubsystem::GetTrait(int32 Id, FName Trait) const
{
	const LP::FPersona* Who = Manners ? Manners->PersonaOf(Id) : nullptr;
	return Who ? LP::PersonaTrait(*Who, TCHAR_TO_UTF8(*Trait.ToString())) : 0.5f;
}

bool UPeopleLifeSubsystem::ReactManner(int32 Id, FName Manner, float Seconds)
{
	if (!Manners || !Bodies.IsValidIndex(Id) || !Bodies[Id] || Bodies[Id]->IsHidden())
	{
		return false;
	}
	const LP::EManner M = LP::MannerFromName(TCHAR_TO_UTF8(*Manner.ToString()));
	return M != LP::EManner::Num && Manners->React(Id, M, MannerClock, Seconds);
}

void UPeopleLifeSubsystem::ForceManner(FName Manner)
{
	if (Manners)
	{
		Manners->Forced = Manner.IsNone() ? LP::EManner::Num : LP::MannerFromName(TCHAR_TO_UTF8(*Manner.ToString()));
	}
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto Part = [this](UStaticMeshComponent*& Out, const TCHAR* Name, UStaticMesh* M)
	{
		Out = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Out->SetupAttachment(Mesh);
		Out->SetUsingAbsoluteLocation(true);
		Out->SetUsingAbsoluteRotation(true);
		Out->SetUsingAbsoluteScale(true);
		Out->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Out->SetVisibility(false);
		Out->SetReceivesDecals(false);
		// moved every frame while shovelling: out of the distance fields and Lumen (perf checklist)
		Out->bAffectDistanceFieldLighting = false;
		Out->bAffectDynamicIndirectLighting = false;
		Out->LightingChannels.bChannel0 = true;
		Out->LightingChannels.bChannel1 = true;
		if (M)
		{
			Out->SetStaticMesh(M);
		}
	};
	UStaticMeshComponent* C = nullptr;
	Part(C, TEXT("PusherBlade"), CubeMesh.Object);
	PusherBlade = C;
	Part(C, TEXT("PusherPole"), CylMesh.Object);
	PusherPole = C;
	Part(C, TEXT("PusherBar"), CylMesh.Object);
	PusherBar = C;
	Part(C, TEXT("MannerProp"), nullptr);
	MannerProp = C;
	Part(C, TEXT("MannerStand"), nullptr);
	MannerStand = C;
}

void ANpcBody::SetManner(uint8 InManner, uint8 InProp, float Since, float Left, bool bLeftHanded, UStaticMesh* PropMesh, UStaticMesh* StandMesh)
{
	Manner = InManner;
	MannerPropKind = InProp;
	MannerSince = Since;
	MannerLeft = Left;
	bMannerLeft = bLeftHanded;
	if (MannerProp->GetStaticMesh() != PropMesh)
	{
		MannerProp->SetStaticMesh(PropMesh);
	}
	if (MannerStand->GetStaticMesh() != StandMesh)
	{
		MannerStand->SetStaticMesh(StandMesh);
		bStandPlaced = false;
	}
}

void ANpcBody::LookAtPoint(const FVector& Where, float Seconds)
{
	if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetLookCandidate(Where, true);
		Anim->Attend(Seconds);
		bLookGiven = true;
	}
}

void ANpcBody::LookAtNothing()
{
	if (bLookGiven)
	{
		bLookGiven = false;
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->ClearLookCandidate();
		}
	}
}

bool ANpcBody::DriveManner(EPersonPose Pose, int32 Transit, float Speed)
{
	using LyudPeople::EManner;
	const EManner M = Manner < (uint8)EManner::Num ? (EManner)Manner : EManner::None;
	const bool bCan = M != EManner::None && Transit == 0 && Pose != EPersonPose::Hidden && Pose != EPersonPose::Sleep && Pose != EPersonPose::Computer
		&& Pose != EPersonPose::Work && Pose != EPersonPose::Shovel;
	FLyudGesturePose G;
	if (bCan)
	{
		LyudGesture::FBody B;
		B.bSeated = Pose == EPersonPose::Sit || Pose == EPersonPose::Eat || Pose == EPersonPose::Read;
		B.bWalking = Pose == EPersonPose::Walk && Speed > 10.f;
		B.bLeftHanded = bMannerLeft;
		B.Scale = BodyScale;
		B.Seed = Seed;
		LyudGesture::Eval(M, MannerSince, MannerLeft, B, G);
	}
	if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->SetGesture(G);
	}
	PlaceMannerProp(bCan);
	if (bCan && (Pose == EPersonPose::Stand || Pose == EPersonPose::Look))
	{
		const FName Own = LyudGesture::ClipFor(M);
		if (!Own.IsNone() && Clip(Own))
		{
			Play(Own, true);
			return true;
		}
	}
	return false;
}

void ANpcBody::PlaceMannerProp(bool bShow)
{
	using LyudGesture::EHold;
	const bool bStand = bShow && MannerStand->GetStaticMesh() != nullptr;
	if (bStand && !bStandPlaced)
	{
		// the easel a step ahead of the painter, its face to them (it stays where it was put while they paint)
		const FQuat Q = GetActorQuat();
		MannerStand->SetWorldLocationAndRotation(GetActorLocation() + Q.GetAxisY() * (62.f * BodyScale),
			FRotationMatrix::MakeFromXZ(-Q.GetAxisY(), FVector::UpVector).Rotator());
		bStandPlaced = true;
	}
	bStandPlaced &= bStand;
	if (MannerStand->IsVisible() != bStand)
	{
		MannerStand->SetVisibility(bStand);
	}
	const bool bProp = bShow && MannerProp->GetStaticMesh() != nullptr && Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE;
	if (MannerProp->IsVisible() != bProp)
	{
		MannerProp->SetVisibility(bProp);
	}
	if (!bProp)
	{
		return;
	}
	// the hands as the last pose left them: where each grips (the middle of the palm), which way its fingers run, where
	// its thumb is and what its palm faces
	static const FName HandN[2] = { TEXT("hand_l"), TEXT("hand_r") }, KnuckleN[2] = { TEXT("middle_01_l"), TEXT("middle_01_r") },
		ThumbN[2] = { TEXT("thumb_01_l"), TEXT("thumb_01_r") }, PinkyN[2] = { TEXT("pinky_01_l"), TEXT("pinky_01_r") };
	const float S = BodyScale;
	FVector At[2], Fingers[2], ThumbWay[2], Palm[2];
	for (int32 I = 0; I < 2; I++)
	{
		const FVector H = Mesh->GetBoneLocation(HandN[I]);
		Fingers[I] = (Mesh->GetBoneLocation(KnuckleN[I]) - H).GetSafeNormal();
		ThumbWay[I] = (Mesh->GetBoneLocation(ThumbN[I]) - Mesh->GetBoneLocation(PinkyN[I])).GetSafeNormal();
		Palm[I] = FVector::CrossProduct(Fingers[I], ThumbWay[I]).GetSafeNormal() * (I == 1 ? 1.f : -1.f);
		At[I] = H + Fingers[I] * (7.f * S) + Palm[I] * (2.5f * S);
	}
	const int32 M = bMannerLeft ? 0 : 1;
	const FQuat Q = GetActorQuat();
	const FVector Fwd = Q.GetAxisY(), Up = FVector::UpVector;
	FVector Loc = At[M];
	FRotator Rot = FRotationMatrix::MakeFromXZ(Fwd, Up).Rotator();
	switch (LyudGesture::HoldOf((LyudPeople::EProp)MannerPropKind))
	{
	case EHold::Palm:
		Loc = At[M] + Palm[M] * 0.8f;
		Rot = FRotationMatrix::MakeFromXZ(Fingers[M], Palm[M]).Rotator();
		break;
	case EHold::Fingers:
		Loc = At[M] + Fingers[M] * (4.f * S) + ThumbWay[M] * (1.5f * S);
		Rot = FRotationMatrix::MakeFromXZ((ThumbWay[M] * 0.8f + Fingers[M] * 0.6f).GetSafeNormal(), Up).Rotator();
		break;
	case EHold::TwoHands:
	{
		Loc = (At[0] + At[1]) * 0.5;
		const FVector Across = (At[0] - At[1]).GetSafeNormal(UE_SMALL_NUMBER, Q.GetAxisX());
		Rot = FRotationMatrix::MakeFromXZ(Across, Mesh->GetBoneLocation(TEXT("head")) + Up * 8.f - Loc).Rotator();
		break;
	}
	case EHold::Pointing:
		Rot = FRotationMatrix::MakeFromXZ((Fwd * 0.85f + Up * 0.5f).GetSafeNormal(), Up).Rotator();
		break;
	default:
		break;
	}
	MannerProp->SetWorldLocationAndRotation(Loc, Rot);
}

void ANpcBody::PlacePusher(bool bShow)
{
	if (!PusherBlade || !PusherPole || !PusherBar)
	{
		return;
	}
	if (!bShow || Mesh->GetBoneIndex(TEXT("hand_l")) == INDEX_NONE)
	{
		PusherBlade->SetVisibility(false);
		PusherPole->SetVisibility(false);
		PusherBar->SetVisibility(false);
		return;
	}
	if (!PusherBlade->GetMaterial(0) || !PusherBlade->GetMaterial(0)->IsA<UMaterialInstanceDynamic>())
	{
		// an orange plastic blade on a wooden pole
		auto Tint = [](UStaticMeshComponent* C, const FLinearColor& Col)
		{
			if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0))
			{
				M->SetVectorParameterValue(TEXT("Color"), Col);
			}
		};
		Tint(PusherBlade, FLinearColor(0.55f, 0.13f, 0.02f));
		Tint(PusherPole, FLinearColor(0.32f, 0.2f, 0.1f));
		Tint(PusherBar, FLinearColor(0.32f, 0.2f, 0.1f));
	}
	// the cross bar between the hands, the pole from it down to the blade on the ground ~1.1 m ahead of the feet
	const FVector L = Mesh->GetBoneLocation(TEXT("hand_l")), R = Mesh->GetBoneLocation(TEXT("hand_r"));
	const FVector Grip = (L + R) * 0.5;
	const FVector Fwd = FRotator(0.f, Facing, 0.f).Vector();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd);
	const FVector Feet = GetActorLocation();
	const FVector BladeAt = Feet + Fwd * (88.f * BodyScale) + FVector(0.f, 0.f, 17.f);
	auto Rod = [](UStaticMeshComponent* C, const FVector& A, const FVector& B, float Diameter)
	{
		const FVector D = B - A;
		C->SetWorldLocationAndRotation((A + B) * 0.5, FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
		C->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, FMath::Max(D.Size(), 1.0) / 100.f));
		C->SetVisibility(true);
	};
	Rod(PusherPole, Grip, BladeAt + FVector(0.f, 0.f, 8.f), 3.4f);
	Rod(PusherBar, Grip - Side * 28.f, Grip + Side * 28.f, 3.0f);
	// the blade: 75 cm wide, 34 cm high, tipped back a little, its lower edge 1 cm over the ground
	PusherBlade->SetWorldLocationAndRotation(BladeAt, FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector).Rotator() + FRotator(-12.f, 0.f, 0.f));
	PusherBlade->SetWorldScale3D(FVector(0.025f, 0.75f, 0.34f));
	PusherBlade->SetVisibility(true);
}

bool ANpcBody::Setup(const FString& InMesh, const FString& InComputerClip)
{
	MeshName = InMesh;
	ComputerClip = InComputerClip;
	const FString Path = PeopleModelPath(InMesh);
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
				A = Load(FString::Printf(TEXT("/Game/Characters/Humans/%s/Anims/A_%s_%s"), *Who, *FPaths::GetCleanFilename(Who), *What));
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
		const FName Was = Playing;
		Playing = Name;
		if (UAnimSequence* A = Clip(Name))
		{
			if (Anim)
			{
				// a loop starts at this person's own moment of it, so a group setting off together doesn't march in step;
				// walk <-> run picks up on the same foot (the left foot lands at 0.156 of the walk, at 0 of the run)
				float Start = 0.f;
				if (bLoop)
				{
					const bool bGait = Name == TEXT("Walk") || Name == TEXT("Run");
					const bool bFromGait = Was == TEXT("Walk") || Was == TEXT("Run");
					const float Phase = bGait && bFromGait && Was != Name
						? Anim->CurrentClipPhase() + (Name == TEXT("Run") ? -0.156f : 0.156f)
						: GetUniqueID() * 0.6180339f;
					Start = FMath::Frac(Phase) * A->GetPlayLength();
				}
				Anim->PlayClip(A, bLoop, Rate, 0.3f, Start);
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
	if (bOnBike)
	{
		DriveBike(nullptr, FLyudBikeRide(), FVector::ZeroVector, 0.f, 0.f, DeltaTime);
	}
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
		Anim->SetChatter((Pose == EPersonPose::Talk && Transit == 0) || Clock < ChatterUntil);
		if (Transit != 0 || (Pose != EPersonPose::Computer && Pose != EPersonPose::Work))
		{
			Anim->SetComputer(NAME_None);
		}
		Anim->SetOnFeet(Transit == 0 && (Pose == EPersonPose::Walk || Pose == EPersonPose::Stand || Pose == EPersonPose::Look
			|| Pose == EPersonPose::Talk || Pose == EPersonPose::Shovel));
	}
	PlacePusher(Transit == 0 && Pose == EPersonPose::Shovel && Clip(TEXT("Push")) != nullptr);
	const bool bMannerClip = DriveManner(Pose, Transit, Speed);
	switch (Transit)
	{
	case 1: Play(TEXT("SitDown"), false); return;
	case 2: Play(TEXT("StandUp"), false); return;
	case 3: Play(TEXT("LieDown"), false); return;
	case 4: Play(TEXT("GetUp"), false); return;
	default: break;
	}
	if (bMannerClip)
	{
		return;
	}
	switch (Pose)
	{
	case EPersonPose::Walk:
	{
		// the shared walk (Quaternius UAL Walk_Loop on Guy_a) covers ~93 cm/s at rate 1 (life_anims_report.json speed_m_s);
		// the owner's (the fallback) 75
		if (SharedWalk < 0)
		{
			// decided once the clip is there (it may come with the body's prefetch a little later)
			if (const UAnimSequence* W = Clip(TEXT("Walk"))) SharedWalk = W->GetName().StartsWith(TEXT("A_Human")) ? 1 : 0;
		}
		const bool bShared = SharedWalk == 1;
		// a run (UAL Jog_Fwd_Loop, ~2.6 m/s at rate 1) from ~1.9 m/s, back to the walk under ~1.6 (a body's size scales both)
		const bool bRun = Clip(TEXT("Run")) && Speed > (Playing == TEXT("Run") ? 160.f : 190.f) * S;
		if (bRun)
		{
			Play(TEXT("Run"), true, FMath::Clamp(Speed / (260.f * S), 0.6f, 1.6f));
		}
		else
		{
			Play(TEXT("Walk"), true, FMath::Clamp(Speed / (bShared ? 93.f * S : 75.f), 0.3f, 2.5f));
		}
		break;
	}
	case EPersonPose::Talk: PlayVaried(TEXT("Talk"), { TEXT("Talk"), TEXT("Talk"), TEXT("Nod"), TEXT("Talk"), TEXT("FoldArms") }); break;
	case EPersonPose::Look: PlayVaried(TEXT("Look"), { TEXT("StandIdle"), TEXT("StandIdle"), TEXT("FoldArms") }, 0.8f); break;
	case EPersonPose::Sit: PlayVaried(TEXT("Sit"), { TEXT("SitIdle"), TEXT("SitIdle"), TEXT("SitTalk") }); break;
	case EPersonPose::Computer:
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->SetComputer(ULyudBodyAnimInstance::StyleOfClip(ComputerClip));
			Playing = TEXT("Computer");
			break;
		}
		Play(TEXT("Computer"), true);
		break;
	case EPersonPose::Shovel:
		// pushing the scraper (UAL Push_Loop's legs, blender/humans/push_clip.py's arms): 0.36 m/s at rate 1
		if (Clip(TEXT("Push")))
		{
			Play(TEXT("Push"), true, FMath::Clamp(Speed / (36.f * S), 0.5f, 1.6f));
		}
		else
		{
			Play(TEXT("Walk"), true, 0.4f);
		}
		break;
	case EPersonPose::Eat: Play(TEXT("Eat"), true); break;
	case EPersonPose::Read: Play(TEXT("Read"), true); break;
	case EPersonPose::Sleep: Play(TEXT("Sleep"), true); break;
	case EPersonPose::Work:
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(Mesh->GetAnimInstance()))
		{
			Anim->SetComputer(ULyudBodyAnimInstance::StyleOfClip(ComputerClip.IsEmpty() ? FString(TEXT("Excel")) : ComputerClip));
			Playing = TEXT("Computer");
			break;
		}
		Play(TEXT("Computer"), true);
		break;
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
		{ TEXT("upperarm_l"), TEXT("lowerarm_l"), -6.f }, { TEXT("lowerarm_l"), TEXT("hand_l"), -5.f },
		{ TEXT("upperarm_r"), TEXT("lowerarm_r"), -6.f }, { TEXT("lowerarm_r"), TEXT("hand_r"), -5.f },
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
				// the duvet's loft rounds over the body; arms (negative radius) are only just covered (one is under the pillow)
				const float r = Rad[k] > 0.f ? Rad[k] + 8.f : -Rad[k] + 3.f;
				if (d < r)
				{
					const float zc = FMath::Lerp(A[k].Z, B[k].Z, t);
					Z = FMath::Max(Z, zc + FMath::Sqrt(r * r - d * d) * 0.6f);
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
	// a fresh mesh every time (the old one goes to GC): rebuilding a mesh that is drawn flushes the renderer and hitches
	{
		BlanketMesh = NewObject<UStaticMesh>(this);
		// looked up per blanket, never kept in a static: after a load opens the level again the old pointer was freed (crash)
		UMaterialInterface* Linen = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Academgorodok/Materials/MI_AC_FLinen.MI_AC_FLinen"));
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

static FAutoConsoleCommandWithWorldAndArgs GPeopleManner(TEXT("lyud.People.Manner"),
	TEXT("lyud.People.Manner <manner|auto>: everybody drawn does this one (fold_arms, phone_call, smoke, umbrella, ...; docs/town_manners.md)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World))
		{
			People->ForceManner(Args.Num() > 0 && Args[0] != TEXT("auto") ? FName(*Args[0]) : NAME_None);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPeopleWho(TEXT("lyud.People.Who"), TEXT("lyud.People.Who <key>: somebody's character, habits and manner now."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);
		if (People && People->GetManners() && People->GetSim() && Args.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("People: %s"), UTF8_TO_TCHAR(People->GetManners()->Describe(*People->GetSim(), People->FindPerson(Args[0])).c_str()));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GPeopleUnique(TEXT("lyud.People.Unique"),
	TEXT("lyud.People.Unique: everybody seen in the world now by whose model it is; a person seen twice is a bug."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World))
		{
			int32 Twice = 0;
			UE_LOG(LogTemp, Warning, TEXT("People: %s"), *People->InstanceReport(Twice));
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
