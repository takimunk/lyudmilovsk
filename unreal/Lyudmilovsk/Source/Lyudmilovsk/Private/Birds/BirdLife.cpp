#include "Birds/BirdLife.h"

#include "BirdSim.h"
#include "Birds/BirdActor.h"
#include "Birds/BirdSpecies.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantModel.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sky/Ephemeris.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "../Murmuration/StarlingMesh.h"

using namespace LyudBirds;

static TAutoConsoleVariable<int32> CVarBirds(TEXT("lyud.Birds"), 1,
	TEXT("The wild birds of the outdoor map (UBirdLifeSubsystem): 1 on, 0 off (removes them at once)."));
static TAutoConsoleVariable<float> CVarBirdsPopulation(TEXT("lyud.Birds.Population"), 1.f,
	TEXT("Share of each species' population to create (read when the birds start)."));
static TAutoConsoleVariable<int32> CVarBirdsMigration(TEXT("lyud.Birds.Migration"), 0,
	TEXT("1: species away in this month (Content/BirdsData/species.json months) are not there; 0: everybody stays."));
static TAutoConsoleVariable<int32> CVarBirdsAnyMap(TEXT("lyud.Birds.AnyMap"), 0,
	TEXT("Start the bird life in any game map, not only Lvl_Scene01."));
static TAutoConsoleVariable<int32> CVarBirdsBodies(TEXT("lyud.Birds.Bodies"), 12,
	TEXT("Most birds near the camera drawn as animated bodies (ABirdActor) at once."));
static TAutoConsoleVariable<int32> CVarBirdsIgnorePlayer(TEXT("lyud.Birds.IgnorePlayer"), 0,
	TEXT("1: birds don't perceive the player (close-up screenshots, debugging)."));
static TAutoConsoleVariable<float> CVarBirdsDrawScale(TEXT("lyud.Birds.DrawDistance"), 700.f,
	TEXT("Birds are drawn within this many of their own lengths from the camera."));

namespace
{
	FVector ToUE(const V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	V3 FromUE(const FVector& V) { return V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }

	EBirdActivity ToActivity(EAct A) { return (EBirdActivity)(uint8)A; }

	/** Our species row (the ecology), and the species thread's (size, wingbeat, months). */
	const FBirdSpecies* Row(int32 Species)
	{
		return UBirdSpeciesLibrary::Find(FName(SpeciesTable()[Species].Id.c_str()));
	}

	float DayHour(const FDateTime& T) { return T.GetHour() + T.GetMinute() / 60.f + T.GetSecond() / 3600.f; }

	/** Sunrise and sunset (local clock, hours) at the game's place on a date (ALyudSky's latitude and longitude, UTC+2). */
	void SunTimes(const FDateTime& Day, float& Rise, float& Set)
	{
		Rise = 7.f;
		Set = 17.f;
		const FDateTime Midnight(Day.GetYear(), Day.GetMonth(), Day.GetDay());
		double Prev = -90.0;
		for (int32 M = 0; M <= 24 * 60; M += 4)
		{
			const FDateTime Utc = Midnight + FTimespan::FromMinutes(M) - FTimespan::FromHours(2);
			const double Alt = LyudEphemeris::Sun(Utc, 54.96, 20.40).AltitudeDeg;
			if (Prev < -0.83 && Alt >= -0.83)
			{
				Rise = M / 60.f;
			}
			if (Prev >= -0.83 && Alt < -0.83 && M > 0)
			{
				Set = M / 60.f;
			}
			Prev = Alt;
		}
	}

	bool IsConifer(FName Species)
	{
		static const FName Names[] = { TEXT("ScotsPine"), TEXT("NorwaySpruce"), TEXT("DunePine"), TEXT("Juniper") };
		for (const FName& N : Names)
		{
			if (Species == N)
			{
				return true;
			}
		}
		return false;
	}

	/** The body's animation state for what a bird does. */
	EBirdState BodyState(const FBird& B, const FSpecies& S)
	{
		switch (B.Act)
		{
		case EAct::Sleep: return EBirdState::Sleep;
		case EAct::Perch: return EBirdState::Perch;
		case EAct::Preen: return EBirdState::Preen;
		case EAct::Forage: return S.bHops ? EBirdState::Hop : EBirdState::Walk;
		case EAct::Peck: return EBirdState::Peck;
		case EAct::Swim: return EBirdState::Perch;
		case EAct::Watch: return B.Alert > 0.5f ? EBirdState::Alert : EBirdState::Look;
		case EAct::Takeoff: return EBirdState::Takeoff;
		case EAct::Land: return EBirdState::Land;
		case EAct::Soar: return EBirdState::Glide;
		case EAct::Eat: return EBirdState::Eat;
		default: return EBirdState::Fly;
		}
	}
}

// --- the subsystem ------------------------------------------------------------------------------------

UBirdLifeSubsystem::UBirdLifeSubsystem() = default;
UBirdLifeSubsystem::~UBirdLifeSubsystem() = default;

UBirdLifeSubsystem* UBirdLifeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBirdLifeSubsystem>() : nullptr;
}

bool UBirdLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UBirdLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBirdLifeSubsystem, STATGROUP_Tickables);
}

void UBirdLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const bool bMap = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")) || CVarBirdsAnyMap.GetValueOnGameThread() != 0;
	bWantStart = bMap;
	StartDelay = 0.5f;      // the forests scatter their trees in their own BeginPlay
}

void UBirdLifeSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

bool UBirdLifeSubsystem::Start()
{
	UWorld* World = GetWorld();
	TPimplPtr<FWorld> W = MakePimpl<FWorld>();
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("BirdsData"));
	if (!W->LoadHabitats(TCHAR_TO_UTF8(*Dir)))
	{
		UE_LOG(LogTemp, Warning, TEXT("BirdLife: %s"), UTF8_TO_TCHAR(W->LastError.c_str()));
		return false;
	}
	Sim = MoveTemp(W);
	FWorld& S = *Sim;
	S.Opt.PopulationScale = FMath::Max(0.f, CVarBirdsPopulation.GetValueOnGameThread());
	S.Opt.Seed = 20261024u;

	FillSpeciesFromTable();
	S.Opt.bMigration = CVarBirdsMigration.GetValueOnGameThread() != 0;

	AddRealTrees();

	// the player's usual place: the club's door, at the map's origin (face y from the habitat file)
	S.Opt.Focus = V3(0.f, 12.f, 0.f);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		S.Opt.Focus = FromUE(Pawn->GetActorLocation());
	}
	S.Opt.FocusShare = 0.2f;
	S.Probe = [World](float X, float Y, float& Z)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdGround), false);
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Q.AddIgnoredActor(Pawn);
		}
		const FVector From(X * 100.0, Y * 100.0, 15000.0), To(X * 100.0, Y * 100.0, -6000.0);
		if (World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
		{
			Z = (float)Hit.ImpactPoint.Z * 0.01f;
			return Z > -42.9f;
		}
		return false;
	};
	S.Sight = [this, World](const V3& From, const V3& To)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdSight), false);
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Q.AddIgnoredActor(Pawn);
		}
		return !World->LineTraceSingleByChannel(Hit, ToUE(From), ToUE(To), ECC_Visibility, Q);
	};
	UpdateClock();
	S.Populate();
	SyncTrust(false);

	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	Renderer = World->SpawnActor<ABirdLifeRenderer>(P);
	Renderer->Setup(S);
	if (ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(World))
	{
		Save->RegisterSaveObject(TEXT("BirdLife"), this);
		Save->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncTrust(false); });
		Save->OnBeforeSave.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncTrust(true); });
	}
	UE_LOG(LogTemp, Display, TEXT("BirdLife: %d birds of %d species in %d groups on %d sites; sun %.2f..%.2f"), (int32)S.Birds.size(),
		(int32)SpeciesTable().size(), (int32)S.Groups.size(), (int32)S.Sites.size(), S.Sunrise, S.Sunset);
	return true;
}

/** The species thread's table: real sizes and wingbeats; who is away this month (lyud.Birds.Migration). */
void UBirdLifeSubsystem::FillSpeciesFromTable()
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	for (int32 I = 0; I < (int32)SpeciesTable().size(); I++)
	{
		FSpecies& Sp = MutableSpeciesTable()[I];
		if (const FBirdSpecies* R = Row(I))
		{
			Sp.Length = R->LengthM();
			Sp.Mass = R->MassG * 0.001f;
			Sp.Wingbeat = R->WingbeatHz;
			Sp.bMigrant = !R->IsPresentIn(Now.GetMonth());
		}
	}
}

/** A bird life with no birds and no map (a test level), for SpawnBird. */
bool UBirdLifeSubsystem::StartEmpty(float GroundZ)
{
	UWorld* World = GetWorld();
	Sim = MakePimpl<FWorld>();
	FWorld& S = *Sim;
	FillSpeciesFromTable();
	S.Opt.PopulationScale = 0.f;
	S.Opt.bPredators = false;
	S.NoGridZ = GroundZ;
	S.Probe = [World](float X, float Y, float& Z)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdGround), false);
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Q.AddIgnoredActor(Pawn);
		}
		if (World->LineTraceSingleByChannel(Hit, FVector(X * 100.0, Y * 100.0, 15000.0), FVector(X * 100.0, Y * 100.0, -6000.0), ECC_Visibility, Q))
		{
			Z = (float)Hit.ImpactPoint.Z * 0.01f;
			return true;
		}
		return false;
	};
	UpdateClock();
	S.Populate();
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	Renderer = World->SpawnActor<ABirdLifeRenderer>(P);
	Renderer->Setup(S);
	bWantStart = false;
	bStarted = true;
	UE_LOG(LogTemp, Display, TEXT("BirdLife: started empty (no outdoor map) for test birds"));
	return true;
}

int32 UBirdLifeSubsystem::SpawnBird(FName Species, FVector Location)
{
	const int32 Sp = FindSpecies(TCHAR_TO_UTF8(*Species.ToString()));
	if (Sp < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BirdLife: no species %s"), *Species.ToString());
		return -1;
	}
	if (!Sim && !StartEmpty((float)Location.Z * 0.01f))
	{
		return -1;
	}
	const int32 Id = Sim->AddBird(Sp, FromUE(Location));
	UE_LOG(LogTemp, Display, TEXT("BirdLife: bird %d, a %s, at %s"), Id, *Species.ToString(), *Location.ToString());
	return Id;
}

void UBirdLifeSubsystem::Stop()
{
	if (Renderer)
	{
		Renderer->Destroy();
		Renderer = nullptr;
	}
	Sim.Reset();
	bStarted = false;
	bClockSeen = false;
}

/** The forests' and hero plants' real crowns replace the guessed tree sites of bird_habitats.json. */
void UBirdLifeSubsystem::AddRealTrees()
{
	UWorld* World = GetWorld();
	FWorld& S = *Sim;
	int32 Forests = 0, Plants = 0;
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		Forests += It->NumTrees();
	}
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		Plants++;
	}
	if (Forests == 0 && Plants == 0)
	{
		return;          // a map without the real forests: keep the guesses
	}
	S.RemoveSites(1 | 2);
	const float Day = UGameCalendar::DayOfYear(UGameCalendar::NowIn(World));
	// streets nearby make a tree a garden tree (town birds)
	TSet<FIntPoint> StreetCells;
	for (const FSite& Site : S.Sites)
	{
		if (Site.Kind == EHabitat::Street || Site.Kind == EHabitat::Roof)
		{
			StreetCells.Add(FIntPoint(FMath::FloorToInt(Site.A.X / 40.f), FMath::FloorToInt(Site.A.Y / 40.f)));
		}
	}
	auto NearStreet = [&](float X, float Y)
	{
		const FIntPoint C(FMath::FloorToInt(X / 40.f), FMath::FloorToInt(Y / 40.f));
		return StreetCells.Contains(C) || StreetCells.Contains(C + FIntPoint(1, 0)) || StreetCells.Contains(C + FIntPoint(-1, 0))
			|| StreetCells.Contains(C + FIntPoint(0, 1)) || StreetCells.Contains(C + FIntPoint(0, -1));
	};
	auto AddCrown = [&](FName Species, const FVector& Base, float Scale, const FPlantSummary& Sum, int32 Capacity)
	{
		if (Sum.Height < 2.f || Sum.Vigour <= 0.f)
		{
			return;
		}
		FSite Site;
		const V3 P = FromUE(Base);
		Site.Kind = IsConifer(Species) ? EHabitat::Conifer : (NearStreet(P.X, P.Y) ? EHabitat::Garden : EHabitat::Broadleaf);
		const float Low = Sum.CrownBaseHeight, High = Sum.Height;
		Site.A = P + V3(0.f, 0.f, (Low + (High - Low) * 0.6f) * Scale);
		Site.B = Site.A;
		Site.Radius = FMath::Max(0.6f, Sum.CrownRadius * 0.65f * Scale);
		Site.Capacity = Capacity;
		S.AddSite(Site);
	};
	// one crown per ~22 m cell is plenty for perches, and keeps the site list small
	TSet<FIntPoint> Taken;
	TMap<TPair<FName, int32>, FPlantSummary> Cache;
	int32 Added = 0;
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		It->ForEachTree([&](FName Species, const FTransform& Xf, float Age)
		{
			const FVector L = Xf.GetLocation();
			const FIntPoint Cell(FMath::FloorToInt(L.X / 2200.0), FMath::FloorToInt(L.Y / 2200.0));
			if (Taken.Contains(Cell))
			{
				return;
			}
			const FPlantSpecies* Sp = PlantSpeciesLibrary::Find(Species);
			if (!Sp)
			{
				return;
			}
			Taken.Add(Cell);
			const TPair<FName, int32> Key(Species, FMath::RoundToInt(Age));
			const FPlantSummary* Sum = Cache.Find(Key);
			if (!Sum)
			{
				Sum = &Cache.Add(Key, PlantModel::Summary(*Sp, 1, Age, Day));
			}
			AddCrown(Species, L, (float)Xf.GetScale3D().Z, *Sum, 6);
			Added++;
		});
	}
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		AddCrown(It->Species, It->GetActorLocation(), (float)It->GetActorScale3D().Z, It->GetSummary(), 4);
		Added++;
	}
	UE_LOG(LogTemp, Display, TEXT("BirdLife: %d tree crowns from %d forest trees and %d plants"), Added, Forests, Plants);
}

void UBirdLifeSubsystem::UpdateClock()
{
	FWorld& S = *Sim;
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const float Was = S.Hour;
	S.Hour = DayHour(Now);
	float Jump = FMath::Abs(S.Hour - Was);
	Jump = FMath::Min(Jump, 24.f - Jump);
	const bool bJumped = bClockSeen && Jump > 0.25f;
	bClockSeen = true;
	const int32 Day = Now.GetDayOfYear() + Now.GetYear() * 400;
	if (Day != SunDay)
	{
		SunDay = Day;
		SunTimes(Now, S.Sunrise, S.Sunset);
		S.DayOfYear = UGameCalendar::DayOfYear(Now);
	}
	if (bJumped)
	{
		S.Resettle();
		UE_LOG(LogTemp, Display, TEXT("BirdLife: the clock jumped %.2f h -> %.2f h; birds resettled"), Was, S.Hour);
	}
}

void UBirdLifeSubsystem::UpdatePlayer(float DeltaTime)
{
	FWorld& S = *Sim;
	FPlayer& P = S.Player;
	const UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	P.bPresent = Pawn != nullptr && CVarBirdsIgnorePlayer.GetValueOnGameThread() == 0;
	if (!Pawn)
	{
		return;
	}
	const ACharacter* Char = Cast<ACharacter>(Pawn);
	const float Half = Pawn->GetSimpleCollisionHalfHeight();
	P.Feet = FromUE(Pawn->GetActorLocation() - FVector(0.0, 0.0, Half));
	P.Vel = FromUE(Pawn->GetVelocity());
	P.bCrouched = Char && Char->bIsCrouched;
	FVector Eye = Pawn->GetActorLocation() + FVector(0.0, 0.0, Half * 0.8);
	FRotator View = Pawn->GetControlRotation();
	if (PC && PC->PlayerCameraManager)
	{
		Eye = PC->PlayerCameraManager->GetCameraLocation();
		View = PC->PlayerCameraManager->GetCameraRotation();
	}
	P.Eye = FromUE(Eye);
	P.Facing = FromUE(View.Vector() * 100.0);
	// how fast the view turns (deg/s), smoothed a little so a single jerky frame doesn't count
	if (DeltaTime > 0.f)
	{
		const FRotator D = (View - LastView).GetNormalized();
		const float Rate = FMath::Sqrt(FMath::Square(D.Yaw) + FMath::Square(D.Pitch)) / DeltaTime;
		TurnRate = FMath::Lerp(TurnRate, Rate, FMath::Min(1.f, DeltaTime / 0.08f));
	}
	LastView = View;
	P.TurnRate = TurnRate;
	// the hand: SetOffer keeps it out while it is called
	const double Now = World->GetTimeSeconds();
	if (bOffer && Now - LastOfferTime > 0.3)
	{
		bOffer = false;
	}
	P.bOffer = bOffer;
	P.Palm = FromUE(LastPalm);
	P.HandSpeed = HandSpeed;
	S.Camera = P.Eye;
	P.bClaimedBird = false;
	for (const FBird& B : S.Birds)
	{
		if (B.Act == EAct::Claimed)
		{
			P.bClaimedBird = true;
			break;
		}
	}
}

void UBirdLifeSubsystem::Tick(float DeltaTime)
{
	if (CVarBirds.GetValueOnGameThread() == 0)
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
	FWorld& S = *Sim;
	UpdateClock();
	UpdatePlayer(DeltaTime);
	S.Step(DeltaTime);
	DispatchEvents();
	if (Renderer)
	{
		Renderer->Draw(S, ToUE(S.Camera), 1.f);
	}
	SaveTimer -= DeltaTime;
	if (SaveTimer <= 0.f)
	{
		SaveTimer = 2.f;
		SyncTrust(true);
	}
}

void UBirdLifeSubsystem::DispatchEvents()
{
	FWorld& S = *Sim;
	// copy: handlers may call back into the simulation
	const std::vector<LyudBirds::FEvent> Events = S.Events;
	S.Events.clear();
	for (const LyudBirds::FEvent& E : Events)
	{
		switch (E.Kind)
		{
		case EEvent::Notice: OnBirdNotice.Broadcast(E.Bird); break;
		case EEvent::Turn: OnBirdTurn.Broadcast(E.Bird); break;
		case EEvent::Watch: OnBirdWatch.Broadcast(E.Bird); break;
		case EEvent::Call:
		case EEvent::Song:
			PlayCall(E.Bird);
			if (ABirdActor* Body = Renderer ? Cast<ABirdActor>(Renderer->BodyOf(E.Bird)) : nullptr)
			{
				if (!IsAirborne(S.Birds[E.Bird].Act))
				{
					Body->PlayCall();
				}
			}
			OnBirdCall.Broadcast(E.Bird);
			break;
		case EEvent::Approach:
			if (Renderer)
			{
				Renderer->EnsureBody(S, E.Bird);
			}
			OnBirdApproach.Broadcast(E.Bird);
			break;
		case EEvent::Eat: OnBirdEat.Broadcast(E.Bird); break;
		case EEvent::Flee: OnBirdFlee.Broadcast(E.Bird); break;
		default: break;
		}
	}
}

/** The species' call at the bird, when the species thread has sounds for it (not too many at once). */
void UBirdLifeSubsystem::PlayCall(int32 Id)
{
	const FWorld& S = *Sim;
	const FBird& B = S.Birds[Id];
	if ((B.Pos - S.Camera).Len() > 70.f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - SoundSecond > 1.0)
	{
		SoundSecond = Now;
		SoundsThisSecond = 0;
	}
	if (SoundsThisSecond >= 4)
	{
		return;
	}
	const FBirdSpecies* R = Row(B.Species);
	if (!R || R->Calls.Num() == 0)
	{
		return;
	}
	TArray<TObjectPtr<USoundBase>>* Found = Sounds.Find(R->Id);
	if (!Found)
	{
		Found = &Sounds.Add(R->Id);
		for (const FName& Call : R->Calls)
		{
			const FString Path = FString::Printf(TEXT("/Game/Birds/%s/Sounds/S_%s_%s.S_%s_%s"), *R->Id.ToString(), *R->Id.ToString(), *Call.ToString(), *R->Id.ToString(), *Call.ToString());
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Found->Add(Sound);
			}
		}
	}
	if (Found->Num() == 0)
	{
		return;
	}
	if (!Attenuation)
	{
		// heard in 3D: full up to 4 m, gone by ~70 m, high frequencies fade with distance
		Attenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = Attenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
		A.FalloffDistance = 6600.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = 1000.f;
		A.LPFRadiusMax = 7000.f;
	}
	USoundBase* Sound = (*Found)[FMath::RandRange(0, Found->Num() - 1)];
	const float Loud = SpeciesTable()[B.Species].Length > 0.3f ? 1.f : 0.75f;
	UGameplayStatics::PlaySoundAtLocation(this, Sound, ToUE(B.Pos), Loud, FMath::FRandRange(0.94f, 1.06f), 0.f, Attenuation);
	SoundsThisSecond++;
}

void UBirdLifeSubsystem::SyncTrust(bool bToSave)
{
	if (!Sim)
	{
		return;
	}
	FWorld& S = *Sim;
	if (bToSave)
	{
		SavedTrust.Reset();
		for (const FBird& B : S.Birds)
		{
			if (B.Trust > 0.05f)
			{
				SavedTrust.Add(B.Id, B.Trust);
			}
		}
		return;
	}
	for (const TPair<int32, float>& T : SavedTrust)
	{
		if (T.Key >= 0 && T.Key < (int32)S.Birds.size())
		{
			S.Birds[T.Key].Trust = T.Value;
		}
	}
}

// --- the API ------------------------------------------------------------------------------------------

void UBirdLifeSubsystem::SetOffer(FVector PalmLocation, FName Food, bool bOn)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bOn)
	{
		if (bOffer && Now > LastOfferTime)
		{
			// the hand's own motion: relative to the body (a creeping hero's palm moves with them), smoothed over
			// ~0.1 s so an uneven frame doesn't read as a jerk
			const double Dt = FMath::Max(Now - LastOfferTime, 1.0 / 120.0);
			const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
			const FVector Raw = (PalmLocation - LastPalm) / Dt - (Pawn ? Pawn->GetVelocity() : FVector::ZeroVector);
			HandVelocity = FMath::Lerp(HandVelocity, Raw, (float)FMath::Min(1.0, Dt / 0.1));
			HandSpeed = (float)HandVelocity.Size() * 0.01f;
		}
		else
		{
			HandVelocity = FVector::ZeroVector;
		}
		LastPalm = PalmLocation;
		LastOfferTime = Now;
	}
	bOffer = bOn;
	(void)Food;
}

void UBirdLifeSubsystem::AddPlayerThreat(float Amount)
{
	if (Sim)
	{
		Sim->Player.ExtraThreat = FMath::Clamp(Sim->Player.ExtraThreat + Amount, 0.f, 2.f);
	}
}

bool UBirdLifeSubsystem::GetBird(int32 Id, FBirdLifeInfo& Out) const
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->Birds.size())
	{
		return false;
	}
	const FBird& B = Sim->Birds[Id];
	Out.Id = Id;
	Out.SpeciesId = FName(SpeciesTable()[B.Species].Id.c_str());
	Out.Location = ToUE(Sim->RenderPos(B));
	Out.Velocity = ToUE(B.Vel);
	Out.HeadDirection = B.bLook ? (ToUE(B.Look) - Out.Location).GetSafeNormal() : FVector(FMath::Cos(B.Yaw), FMath::Sin(B.Yaw), 0.0);
	Out.Activity = ToActivity(B.Act);
	Out.Alertness = FMath::Clamp(B.Alert, 0.f, 1.f);
	Out.Trust = B.Trust;
	Out.bNoticed = B.bNoticed;
	Out.bWatching = B.bWatching;
	Out.Actor = Renderer ? Renderer->BodyOf(Id) : nullptr;
	return true;
}

TArray<int32> UBirdLifeSubsystem::FindBirdsNear(FVector Location, float Radius) const
{
	TArray<int32> Out;
	if (!Sim)
	{
		return Out;
	}
	std::vector<int> Ids;
	const V3 P = FromUE(Location);
	Sim->Near(P, Radius * 0.01f, Ids);
	for (int I : Ids)
	{
		if (!Sim->Birds[I].bGone)
		{
			Out.Add(I);
		}
	}
	Out.Sort([&](int32 A, int32 B) { return (Sim->Birds[A].Pos - P).Len() < (Sim->Birds[B].Pos - P).Len(); });
	return Out;
}

AActor* UBirdLifeSubsystem::ClaimBird(int32 Id)
{
	if (!Sim || !Sim->Claim(Id))
	{
		return nullptr;
	}
	AActor* Body = Renderer ? Renderer->EnsureBody(*Sim, Id) : nullptr;
	if (Body)
	{
		Body->SetActorHiddenInGame(true);
	}
	return Body;
}

void UBirdLifeSubsystem::SetClaimedBirdLocation(int32 Id, FVector Location, FVector Velocity)
{
	if (Sim)
	{
		Sim->SetClaimedPos(Id, FromUE(Location), FromUE(Velocity));
	}
}

void UBirdLifeSubsystem::ReleaseBird(int32 Id)
{
	if (Sim)
	{
		Sim->Release(Id);
	}
}

void UBirdLifeSubsystem::Scare(FVector Location, float Radius, float Strength)
{
	if (Sim)
	{
		Sim->Scare(FromUE(Location), Radius * 0.01f, Strength);
	}
}

int32 UBirdLifeSubsystem::NumBirds() const
{
	return Sim ? (int32)Sim->Birds.size() : 0;
}

FString UBirdLifeSubsystem::Report() const
{
	if (!Sim)
	{
		return TEXT("BirdLife: not running");
	}
	return FString::Printf(TEXT("BirdLife: %s; drawn %d (%s), bodies %d"), UTF8_TO_TCHAR(Sim->Report().c_str()),
		Renderer ? Renderer->DrawnLastFrame : 0, Renderer ? *Renderer->Describe() : TEXT(""), Renderer ? Renderer->BodiesLastFrame : 0);
}

void UBirdLifeSubsystem::DumpDay(float StartHour, float Hours, const FString& OutCsv)
{
	if (!Sim)
	{
		return;
	}
	FWorld& S = *Sim;
	FString Csv = TEXT("hour,id,species,act,x,y,z,site_kind,alert\n");
	const float Dt = 0.2f;
	float NextSnap = StartHour;
	const V3 Camera = S.Camera;
	const bool bPresent = S.Player.bPresent;
	S.Player.bPresent = false;
	for (float T = 0.f; T < Hours * 3600.f; T += Dt)
	{
		const float Clock = StartHour + T / 3600.f;
		S.Hour = FMath::Fmod(Clock, 24.f);
		S.Camera = Camera;
		S.Step(Dt);
		S.Events.clear();
		if (Clock >= NextSnap)
		{
			NextSnap += 0.25f;
			for (const FBird& B : S.Birds)
			{
				if (B.bGone)
				{
					continue;
				}
				const char* Kind = B.Site >= 0 && !IsAirborne(B.Act) ? HabitatName(S.Sites[B.Site].Kind) : "air";
				Csv += FString::Printf(TEXT("%.2f,%d,%s,%s,%.1f,%.1f,%.1f,%s,%.2f\n"), S.Hour, B.Id, UTF8_TO_TCHAR(SpeciesTable()[B.Species].Id.c_str()),
					UTF8_TO_TCHAR(ActName(B.Act)), B.Pos.X, B.Pos.Y, B.Pos.Z, UTF8_TO_TCHAR(Kind), B.Alert);
			}
		}
	}
	S.Player.bPresent = bPresent;
	FFileHelper::SaveStringToFile(Csv, *OutCsv);
	UE_LOG(LogTemp, Display, TEXT("BirdLife: day %.1f h from %.2f -> %s"), Hours, StartHour, *OutCsv);
}

static FAutoConsoleCommandWithWorldAndArgs GBirdsReportCmd(TEXT("lyud.Birds.Report"), TEXT("Prints what the wild birds are doing."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (const UBirdLifeSubsystem* B = UBirdLifeSubsystem::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *B->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBirdsDumpCmd(TEXT("lyud.Birds.DumpDay"),
	TEXT("lyud.Birds.DumpDay <start hour> <hours> [csv]: runs the birds headless over game hours and writes where each one is every 15 min."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBirdLifeSubsystem* B = UBirdLifeSubsystem::Get(World))
		{
			const float Start = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 4.f;
			const float Hours = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 24.f;
			const FString Out = Args.Num() > 2 ? Args[2] : FPaths::ProjectSavedDir() / TEXT("BirdSim/day.csv");
			B->DumpDay(Start, Hours, Out);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBirdsTimeCmd(TEXT("lyud.Birds.Time"), TEXT("lyud.Birds.Time <hour> [minute]: sets the game clock to that time today (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
		if (Cal && Args.Num())
		{
			const FDateTime Now = Cal->Now();
			Cal->SetNow(FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay(), FCString::Atoi(*Args[0]) % 24, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0, 0));
		}
	}));

static void SpawnNearCmd(const TArray<FString>& Args, UWorld* World)
{
	UBirdLifeSubsystem* B = UBirdLifeSubsystem::Get(World);
	if (!B || Args.Num() < 1)
	{
		return;
	}
	FVector At;
	if (Args.Num() >= 4)
	{
		At = FVector(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
	}
	else if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		// 3 m in front of the player, on the ground
		At = Pawn->GetActorLocation() + FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f).Vector() * 300.0;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, At, At - FVector(0, 0, 1000), ECC_Visibility))
		{
			At = Hit.ImpactPoint;
		}
	}
	B->SpawnBird(FName(*Args[0]), At);
}

static FAutoConsoleCommandWithWorldAndArgs GBirdsSpawnCmd(TEXT("lyud.Birds.SpawnNear"),
	TEXT("lyud.Birds.SpawnNear <Species> [x y z]: one calm bird on the ground there (cm), or 3 m in front of the player; it keeps to that spot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnNearCmd));
static FAutoConsoleCommandWithWorldAndArgs GBirdsSpawnAliasCmd(TEXT("Birds.SpawnNear"), TEXT("Same as lyud.Birds.SpawnNear."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnNearCmd));

static FAutoConsoleCommandWithWorldAndArgs GBirdsScareCmd(TEXT("lyud.Birds.Scare"), TEXT("lyud.Birds.Scare [radius m]: a bang at the player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UBirdLifeSubsystem* B = UBirdLifeSubsystem::Get(World);
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		if (B && Pawn)
		{
			B->Scare(Pawn->GetActorLocation(), (Args.Num() ? FCString::Atof(*Args[0]) : 40.f) * 100.f, 1.5f);
		}
	}));

// --- drawing ------------------------------------------------------------------------------------------

ABirdLifeRenderer::ABirdLifeRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABirdLifeRenderer::Setup(const FWorld& World)
{
	static const TCHAR* const PoseNames[NumPoses] = { TEXT("Perch"), TEXT("Peck"), TEXT("Sleep"), TEXT("Flap0"), TEXT("Flap1"), TEXT("Flap2"), TEXT("Glide") };
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	// placeholders: Murmuration's starling, perched and in four wing positions
	PlaceholderMeshes.Reset();
	PlaceholderMeshes.Add(StarlingMesh::BuildPerched(this, TEXT("SM_BirdLife_Perch"), Base));
	PlaceholderMeshes.Add(StarlingMesh::Build(this, TEXT("SM_BirdLife_Flap0"), -1.f, 1.f, Base));
	PlaceholderMeshes.Add(StarlingMesh::Build(this, TEXT("SM_BirdLife_Flap1"), 0.f, 1.f, Base));
	PlaceholderMeshes.Add(StarlingMesh::Build(this, TEXT("SM_BirdLife_Flap2"), 1.f, 1.f, Base));
	PlaceholderMeshes.Add(StarlingMesh::Build(this, TEXT("SM_BirdLife_Glide"), 0.15f, 0.2f, Base));
	BodyClass = ABirdActor::StaticClass();

	const int32 NumSpecies = (int32)SpeciesTable().size();
	Species.SetNum(NumSpecies);
	for (int32 I = 0; I < NumSpecies; I++)
	{
		FSpeciesDraw& D = Species[I];
		const FName Id(SpeciesTable()[I].Id.c_str());
		const FBirdSpecies* R = UBirdSpeciesLibrary::Find(Id);
		const float Length = R ? R->LengthM() : SpeciesTable()[I].Length;
		D.Xf.SetNum(NumPoses);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
		const FLinearColor Colour = R && R->Palette.Num() ? R->Palette[0] : FLinearColor(FColor((SpeciesTable()[I].Color >> 16) & 255, (SpeciesTable()[I].Color >> 8) & 255, SpeciesTable()[I].Color & 255));
		Mat->SetVectorParameterValue(TEXT("Color"), Colour);
		Materials.Add(Mat);
		D.bOwnMeshes = UBirdSpeciesLibrary::LoadPose(Id, TEXT("Perch")) != nullptr;
		D.bHasBody = UBirdSpeciesLibrary::LoadSkeletalMesh(Id) != nullptr;
		D.Scale = D.bOwnMeshes ? 1.f : Length / 0.215f;
		for (int32 P = 0; P < NumPoses; P++)
		{
			UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ISM_%s_%s"), *Id.ToString(), PoseNames[P]));
			ISM->SetupAttachment(RootComponent);
			ISM->SetMobility(EComponentMobility::Movable);
			ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ISM->SetCastShadow(Length > 0.3f);
			ISM->bDisableCollision = true;
			ISM->SetCanEverAffectNavigation(false);
			ISM->LightingChannels.bChannel1 = true;     // Scene 01's outdoor sun and moon light channel 1 only
			ISM->NumCustomDataFloats = 0;
			UStaticMesh* Mesh = nullptr;
			if (D.bOwnMeshes)
			{
				Mesh = UBirdSpeciesLibrary::LoadPose(Id, PoseNames[P]);
			}
			if (!Mesh)
			{
				Mesh = PlaceholderMeshes[P <= 2 ? 0 : P - 2];
				ISM->SetMaterial(0, Mat);
			}
			ISM->SetStaticMesh(Mesh);
			ISM->RegisterComponent();
			D.Poses.Add(ISM);
		}
	}
}

void ABirdLifeRenderer::Sync(UInstancedStaticMeshComponent* ISM, TArray<FTransform>& Arr)
{
	static const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	const int32 Want = Arr.Num();
	const int32 Have = ISM->GetInstanceCount();
	if (Want == 0 && Have == 0)
	{
		return;
	}
	if (Want > Have)
	{
		TArray<FTransform> Extra;
		Extra.Init(Hidden, Want - Have + FMath::Max(8, Want / 4));
		ISM->AddInstances(Extra, false);
	}
	Arr.SetNum(ISM->GetInstanceCount(), EAllowShrinking::No);
	for (int32 J = Want; J < Arr.Num(); J++)
	{
		Arr[J] = Hidden;
	}
	ISM->BatchUpdateInstancesTransforms(0, Arr, false, true, true);
}

void ABirdLifeRenderer::Draw(const FWorld& World, const FVector& Camera, float PixelScale)
{
	SetActorLocation(FVector::ZeroVector);
	for (FSpeciesDraw& D : Species)
	{
		for (TArray<FTransform>& X : D.Xf)
		{
			X.Reset();
		}
	}
	const float DrawLengths = CVarBirdsDrawScale.GetValueOnGameThread();
	const int32 MaxBodies = BodyClass ? CVarBirdsBodies.GetValueOnGameThread() : 0;
	// bodies: the nearest birds within 25 m (and the one coming to the hand, and the claimed one)
	TArray<TPair<float, int32>> Near;
	DrawnLastFrame = 0;
	for (const FBird& B : World.Birds)
	{
		if (B.Act == EAct::Claimed && BodyOf(B.Id))
		{
			DriveBody(BodyOf(B.Id), World, B.Id);
		}
		if (B.bGone || B.Tier > 2 || B.Act == EAct::Claimed)
		{
			continue;            // a claimed bird is drawn by the player's own body (bird flight)
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		const FVector P = ToUE(World.RenderPos(B));
		const float Dist = (float)FVector::Dist(P, Camera) * 0.01f;
		if (Dist > S.Length * DrawLengths || (B.Act == EAct::Peck && P.Z < -4350.0))
		{
			continue;            // too small to see, or a cormorant under water
		}
		if (MaxBodies > 0 && (Dist < 25.f || B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Claimed))
		{
			Near.Emplace(B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Claimed ? -1.f : Dist, B.Id);
		}
		FSpeciesDraw& D = Species[B.Species];
		// far birds keep at least ~1.2 px so they don't flicker away (1080 px, 90 degrees)
		const float Px = S.Length / FMath::Max(Dist, 0.1f) * 540.f * PixelScale;
		const float Grow = FMath::Max(1.f, 1.2f / FMath::Max(Px, 0.01f));
		const float Scale = D.Scale * FMath::Min(Grow, 3.f);
		int32 Pose = 0;
		float Pitch = 0.f, Roll = 0.f;
		const bool bAir = IsAirborne(B.Act);
		if (bAir)
		{
			const float Speed = B.Vel.Len();
			Pitch = Speed > 0.5f ? FMath::RadiansToDegrees(FMath::Atan2(B.Vel.Z, FMath::Max(B.Vel.Len2D(), 0.1f))) * 0.6f : 0.f;
			// wing positions through the beat; gliding, bounding and soaring species fold or spread
			const float Beat = FMath::Frac(B.Phase);
			const int32 Frame = Beat < 0.25f ? 0 : (Beat < 0.5f ? 1 : (Beat < 0.75f ? 2 : 1));
			Pose = 3 + Frame;
			const float Slow = FMath::Sin(B.Phase * 0.21f + B.Id);
			if (B.Act == EAct::Soar || B.Act == EAct::Land || (S.Flight == EFlight::FlapGlide && Slow > 0.1f) || (S.Flight == EFlight::Aerial && Slow > 0.3f))
			{
				Pose = 6;
			}
			else if (S.Flight == EFlight::Bounding && FMath::Frac(B.Phase * 0.12f) > 0.6f)
			{
				Pose = 0;       // wings shut in the bound
			}
		}
		else if (B.Act == EAct::Peck || B.Act == EAct::Eat)
		{
			Pose = 1;
			if (!D.bOwnMeshes)
			{
				Pitch = FMath::Frac(B.Phase) < 0.5f ? -35.f : -10.f;
			}
		}
		else if (B.Act == EAct::Sleep)
		{
			Pose = 2;
		}
		const FRotator Rot(Pitch, FMath::RadiansToDegrees(B.Yaw) + (D.bOwnMeshes ? 0.f : -90.f), Roll);
		FVector At = P;
		if (D.bOwnMeshes && bAir)
		{
			At.Z -= S.Length * 12.0;   // the species' poses stand on their feet; in the air the bird's position is its body
		}
		const FVector S3 = !D.bOwnMeshes && B.Act == EAct::Sleep ? FVector(Scale, Scale, Scale * 0.85f) : FVector(Scale);
		D.Xf[Pose].Emplace(Rot, At, S3);
		DrawnLastFrame++;
	}
	// bodies
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TSet<int32> Keep;
	for (int32 K = 0; K < FMath::Min(MaxBodies, Near.Num()); K++)
	{
		const int32 Id = Near[K].Value;
		if (!Species[World.Birds[Id].Species].bHasBody)
		{
			continue;
		}
		Keep.Add(Id);
		AActor* Body = EnsureBody(World, Id);
		if (Body)
		{
			DriveBody(Body, World, Id);
		}
	}
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		if (!Keep.Contains(It.Key()) && World.Birds[It.Key()].Act != EAct::Claimed)
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			BodyActs.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
	BodiesLastFrame = Bodies.Num();
	// a bird with a body is not drawn instanced as well
	if (Bodies.Num())
	{
		for (FSpeciesDraw& D : Species)
		{
			for (TArray<FTransform>& X : D.Xf)
			{
				X.RemoveAll([&](const FTransform& T)
				{
					for (const TPair<int32, TObjectPtr<AActor>>& Body : Bodies)
					{
						if (Body.Value && FVector::DistSquared(Body.Value->GetActorLocation(), T.GetLocation()) < 1.0)
						{
							return true;
						}
					}
					return false;
				});
			}
		}
	}
	for (FSpeciesDraw& D : Species)
	{
		for (int32 P = 0; P < D.Poses.Num(); P++)
		{
			Sync(D.Poses[P], D.Xf[P]);
		}
	}
}

FString ABirdLifeRenderer::Describe() const
{
	// the nearest drawn instance to the camera of each species that has any, and its scale
	FString Out;
	for (int32 I = 0; I < Species.Num(); I++)
	{
		for (int32 P = 0; P < Species[I].Poses.Num(); P++)
		{
			const UInstancedStaticMeshComponent* ISM = Species[I].Poses[P];
			const TArray<FTransform>& X = Species[I].Xf[P];
			if (X.Num() && X[0].GetScale3D().X > 0.0)
			{
				Out += FString::Printf(TEXT("%s/%d n%d vis%d [%.0f %.0f %.0f s%.2f] "), UTF8_TO_TCHAR(SpeciesTable()[I].Id.c_str()), P, ISM->GetInstanceCount(),
					ISM->IsVisible() ? 1 : 0, X[0].GetLocation().X, X[0].GetLocation().Y, X[0].GetLocation().Z, X[0].GetScale3D().X);
			}
		}
	}
	return Out.Left(600);
}

AActor* ABirdLifeRenderer::BodyOf(int32 Id) const
{
	const TObjectPtr<AActor>* B = Bodies.Find(Id);
	return B ? B->Get() : nullptr;
}

AActor* ABirdLifeRenderer::EnsureBody(const FWorld& World, int32 Id)
{
	if (AActor* Have = BodyOf(Id))
	{
		return Have;
	}
	if (!BodyClass || Id < 0 || Id >= (int32)World.Birds.size())
	{
		return nullptr;
	}
	const FBird& B = World.Birds[Id];
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	const FTransform At(FRotator(0.f, FMath::RadiansToDegrees(B.Yaw), 0.f), ToUE(World.RenderPos(B)));
	ABirdActor* Body = GetWorld()->SpawnActorDeferred<ABirdActor>(ABirdActor::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Body)
	{
		Body->Species = FName(SpeciesTable()[B.Species].Id.c_str());
		Body->Seed = Id;
		Body->InitialState = BodyState(B, SpeciesTable()[B.Species]);
		Body->SetFlags(RF_Transient);
		Body->FinishSpawning(At);
		Bodies.Add(Id, Body);
		BodyActs.Add(Id, -1.f);
	}
	return Body;
}

void ABirdLifeRenderer::ReleaseBody(int32 Id)
{
	if (TObjectPtr<AActor>* B = Bodies.Find(Id))
	{
		if (*B)
		{
			(*B)->Destroy();
		}
		Bodies.Remove(Id);
		BodyActs.Remove(Id);
	}
}

void ABirdLifeRenderer::DriveBody(AActor* Body, const FWorld& World, int32 Id)
{
	const FBird& B = World.Birds[Id];
	const bool bClaimed = B.Act == EAct::Claimed;
	if (Body->IsHidden() != bClaimed)
	{
		Body->SetActorHiddenInGame(bClaimed);   // the bird flight draws its own body
	}
	if (bClaimed)
	{
		return;
	}
	const FSpecies& S = SpeciesTable()[B.Species];
	ABirdActor* Bird = Cast<ABirdActor>(Body);
	if (!Bird)
	{
		return;
	}
	float Pitch = 0.f;
	if (IsAirborne(B.Act) && B.Vel.Len() > 0.5f)
	{
		Pitch = FMath::RadiansToDegrees(FMath::Atan2(B.Vel.Z, FMath::Max(B.Vel.Len2D(), 0.1f))) * 0.6f;
	}
	Bird->SetActorLocationAndRotation(ToUE(World.RenderPos(B)), FRotator(Pitch, FMath::RadiansToDegrees(B.Yaw), 0.f));
	// a new state (or a new peck) only when the bird starts it: the body plays its own clips in between
	float& Started = BodyActs.FindOrAdd(Id);
	const float Key = (float)(uint8)B.Act * 1000.f + B.ActLen;
	if (Started != Key)
	{
		Started = Key;
		const EBirdState Want = BodyState(B, S);
		if (Bird->GetState() != Want || Want == EBirdState::Peck || Want == EBirdState::Eat || Want == EBirdState::Preen)
		{
			Bird->SetState(Want);
		}
	}
	Bird->SetVelocity(ToUE(B.Vel));
	Bird->SetLookTarget(ToUE(B.Look), B.bLook);
}
