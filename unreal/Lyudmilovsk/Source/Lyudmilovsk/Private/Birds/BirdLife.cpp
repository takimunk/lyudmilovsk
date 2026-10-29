#include "Birds/BirdLife.h"

#include "BirdSim.h"
#include "BirdMurmur.h"
#include "Sea/LyudSea.h"
#include "Birds/BirdActor.h"
#include "HeroCharacter.h"
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
static TAutoConsoleVariable<int32> CVarBirdsMigration(TEXT("lyud.Birds.Migration"), 1,
	TEXT("1 (default): species away in this month (Content/BirdsData/species.json months) are not there, e.g. no swifts in October; 0: everybody stays."));
static TAutoConsoleVariable<int32> CVarBirdsDoorFlock(TEXT("lyud.Birds.DoorFlock"), 6,
	TEXT("Town pigeons that always feed on the terrace just out of the club's door (read when the birds start; 0: none)."));
static TAutoConsoleVariable<int32> CVarBirdsAnyMap(TEXT("lyud.Birds.AnyMap"), 0,
	TEXT("Start the bird life in any game map, not only Lvl_Scene01."));
static TAutoConsoleVariable<int32> CVarBirdsBodies(TEXT("lyud.Birds.Bodies"), 12,
	TEXT("Most birds near the camera drawn as animated bodies (ABirdActor) at once."));
static TAutoConsoleVariable<int32> CVarBirdsIgnorePlayer(TEXT("lyud.Birds.IgnorePlayer"), 0,
	TEXT("1: birds don't perceive the player (close-up screenshots, debugging)."));
static TAutoConsoleVariable<float> CVarBirdsDrawScale(TEXT("lyud.Birds.DrawDistance"), 700.f,
	TEXT("Birds are drawn within this many of their own lengths from the camera."));
static TAutoConsoleVariable<int32> CVarBirdsSteady(TEXT("lyud.Birds.Steady"), 1,
	TEXT("1: birds fade out at the edge of their draw range and bodies hand over with some slack; 0: the old hard cuts (A/B of popping)."));

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
	AddDoorFlock();
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
			// late stragglers (species.json): their days of leaving in this year
			Sp.Stragglers = R->Stragglers;
			if (R->Stragglers > 0 && R->StragglersFrom > 0 && R->StragglersUntil > 0)
			{
				auto Doy = [&Now](int32 MonthDay) { return (float)FDateTime(Now.GetYear(), MonthDay / 100, MonthDay % 100).GetDayOfYear(); };
				Sp.LeaveFrom = Doy(R->StragglersFrom);
				Sp.LeaveUntil = Doy(R->StragglersUntil);
			}
		}
		else
		{
			Sp.Population = 0;      // not in species.json (yet): no body to draw it with
		}
	}
}

/** Town pigeons that always feed on the terrace a few steps out of the club's door (to try the feed on). */
void UBirdLifeSubsystem::AddDoorFlock()
{
	const int32 Count = CVarBirdsDoorFlock.GetValueOnGameThread();
	const int32 Sp = FindSpecies("FeralPigeon");
	if (Count <= 0 || Sp < 0)
	{
		return;
	}
	// on the slab path ~14 m out of the door, where they show against the pale concrete (the grass beside it hides
	// them); map X -1.93, Y +13.6, the portal face at Unreal Y 790
	FVector At(-193.0, 2150.0, 0.0);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdDoorFlock), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 3000.0), At - FVector(0.0, 0.0, 3000.0), ECC_Visibility, Q))
	{
		At = Hit.ImpactPoint;
	}
	const int32 First = Sim->AddFlock(Sp, FromUE(At), Count, 1.1f);
	UE_LOG(LogTemp, Display, TEXT("BirdLife: %d pigeons by the club's door (ids %d..%d) at %s"), Count, First, First + Count - 1, *At.ToString());
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
	// the hero's own eyes and look, never the camera's (a side camera, a camera flight or a cut is not the hero turning)
	FVector Eye = Pawn->GetPawnViewLocation();
	if (const AHeroCharacter* Hero = Cast<AHeroCharacter>(Pawn))
	{
		Eye = Hero->EyeLocation();
	}
	const FRotator View = Pawn->GetControlRotation();
	P.Eye = FromUE(Eye);
	// a scripted run (lyud.Birds.TestRun): the birds see the player run, whatever the pawn does
	if (TestRunUntil > World->GetTimeSeconds())
	{
		const FVector To = TestRunTarget - TestRunFeet;
		const FVector Dir = FVector(To.X, To.Y, 0.0).GetSafeNormal();
		if (To.Size2D() > 50.0)
		{
			TestRunFeet += Dir * TestRunSpeed * DeltaTime;
		}
		P.Feet = FromUE(TestRunFeet);
		P.Eye = FromUE(TestRunFeet + FVector(0.0, 0.0, 165.0));
		P.Vel = To.Size2D() > 50.0 ? FromUE(Dir * TestRunSpeed) : V3();
		P.bCrouched = false;
	}
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
	// LOD and drawing follow the real camera; perception the hero's eyes (the bird near the hero thinks every frame too)
	S.Camera = PC && PC->PlayerCameraManager ? FromUE(PC->PlayerCameraManager->GetCameraLocation()) : P.Eye;
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
	// the sea's level now (the tide): swimmers float on it, gulls and waders follow it over the sand
	if (!Sea.IsValid())
	{
		TActorIterator<ALyudSea> It(GetWorld());
		Sea = It ? *It : nullptr;
	}
	S.SeaZ = Sea.IsValid() ? (float)Sea->SeaLevel() * 0.01f : S.MeanSeaZ;
	MigrationTimer -= DeltaTime;
	if (MigrationTimer <= 0.f)
	{
		MigrationTimer = 5.f;
		S.UpdateMigration();
	}
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

void UBirdLifeSubsystem::SetBeasts(const TArray<FBirdBeast>& Beasts)
{
	if (!Sim)
	{
		return;
	}
	Sim->Beasts.clear();
	for (const FBirdBeast& B : Beasts)
	{
		Sim->Beasts.push_back(FBeast{ FromUE(B.Location), FromUE(B.Velocity), B.bCat, B.bStalking });
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
	TArray<USoundBase*>* Found = Sounds.Find(R->Id);
	if (!Found)
	{
		Found = &Sounds.Add(R->Id);
		for (const FName& Call : R->Calls)
		{
			const FString Path = FString::Printf(TEXT("/Game/Birds/%s/Sounds/S_%s_%s.S_%s_%s"), *R->Id.ToString(), *R->Id.ToString(), *Call.ToString(), *R->Id.ToString(), *Call.ToString());
			if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Found->Add(Sound);
				SoundRefs.Add(Sound);      // or the garbage collector unloads it under us
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
	CallsPlayed++;
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
	Out.bFemale = B.bFemale;
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

void UBirdLifeSubsystem::TestRun(FVector From, FVector To, float Speed, float Seconds)
{
	TestRunFeet = From;
	TestRunTarget = To;
	TestRunSpeed = Speed;
	TestRunUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) + Seconds;
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
	return FString::Printf(TEXT("BirdLife: %s; calls played %d, sounds loaded %d; drawn %d (%s), bodies %d; pops %d, body spawns %d releases %d"), UTF8_TO_TCHAR(Sim->Report().c_str()), CallsPlayed, SoundRefs.Num(),
		Renderer ? Renderer->DrawnLastFrame : 0, Renderer ? *Renderer->Describe() : TEXT(""), Renderer ? Renderer->BodiesLastFrame : 0,
		Renderer ? Renderer->Pops : 0, Renderer ? Renderer->BodySpawns : 0, Renderer ? Renderer->BodyReleases : 0) + (Renderer && Renderer->PopLog.Num() ? TEXT(" (last pops: ") + FString::Join(Renderer->PopLog, TEXT("; ")) + TEXT(")") : TEXT(""));
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

static FAutoConsoleCommandWithWorldAndArgs GBirdsTestRunCmd(TEXT("lyud.Birds.TestRun"),
	TEXT("lyud.Birds.TestRun <from x y z> <to x y> <speed cm/s> <seconds>: birds see the player run from one point to another (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UBirdLifeSubsystem* B = UBirdLifeSubsystem::Get(World);
		if (B && Args.Num() >= 7)
		{
			B->TestRun(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])),
				FVector(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), FCString::Atof(*Args[2])), FCString::Atof(*Args[5]), FCString::Atof(*Args[6]));
		}
	}));

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
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
		const FLinearColor Colour = R && R->Palette.Num() ? R->Palette[0] : FLinearColor(FColor((SpeciesTable()[I].Color >> 16) & 255, (SpeciesTable()[I].Color >> 8) & 255, SpeciesTable()[I].Color & 255));
		Mat->SetVectorParameterValue(TEXT("Color"), Colour);
		Materials.Add(Mat);
		D.bOwnMeshes = UBirdSpeciesLibrary::LoadPose(Id, TEXT("Perch")) != nullptr;
		D.bHasBody = UBirdSpeciesLibrary::LoadSkeletalMesh(Id) != nullptr;
		D.Scale = D.bOwnMeshes ? 1.f : Length / 0.215f;
		D.FemaleScale = R ? R->FemaleScale : 1.f;
		const bool bWinter = R && R->InWinterPlumage(UGameCalendar::NowIn(GetWorld()).GetMonth());
		// females get their own components where the species' female plumage differs (same poses, female materials)
		const int32 Sets = D.bOwnMeshes && R && R->bFemalePlumage ? 2 : 1;
		D.Xf.SetNum(NumPoses * Sets);
		for (int32 Set = 0; Set < Sets; Set++)
		{
			TArray<FString> Suffixes;
			if (Set == 1 && bWinter)
			{
				Suffixes.Add(TEXT("_FemaleWinter"));
			}
			if (Set == 1)
			{
				Suffixes.Add(TEXT("_Female"));
			}
			if (bWinter)
			{
				Suffixes.Add(TEXT("_Winter"));
			}
			for (int32 P = 0; P < NumPoses; P++)
			{
				UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ISM_%s_%s%s"), *Id.ToString(), PoseNames[P], Set ? TEXT("_F") : TEXT("")));
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
				if (D.bOwnMeshes && Suffixes.Num())
				{
					// the most specific material set that exists, as ABirdActor::ApplyPlumage picks it
					const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
					for (int32 Slot = 0; Slot < Slots.Num(); Slot++)
					{
						const UMaterialInterface* BaseMat = Slots[Slot].MaterialInterface;
						for (const FString& Sfx : Suffixes)
						{
							if (!BaseMat)
							{
								break;
							}
							const FString Name = BaseMat->GetName() + Sfx;
							if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Birds/%s/%s.%s"), *Id.ToString(), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
							{
								ISM->SetMaterial(Slot, M);
								break;
							}
						}
					}
				}
				ISM->RegisterComponent();
				D.Poses.Add(ISM);
			}
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
	const int32 NumBirds = (int32)World.Birds.size();
	if (Reach.Num() != NumBirds)
	{
		Reach.Init(1.f, NumBirds);
		LastPx.Init(0.f, NumBirds);
		LastWhy.Init(0, NumBirds);
	}
	const float Dt = FMath::Clamp(GetWorld()->GetDeltaSeconds(), 0.f, 0.25f);
	const bool bSteady = CVarBirdsSteady.GetValueOnGameThread() != 0;
	const float DrawLengths = CVarBirdsDrawScale.GetValueOnGameThread();
	const float FarDist = World.Opt.FarDist;
	const int32 MaxBodies = BodyClass ? CVarBirdsBodies.GetValueOnGameThread() : 0;
	// bodies: the nearest birds within 25 m (and the one coming to the hand, and the claimed one); a bird that has a
	// body keeps it out to 30 m and against newcomers up to 5 m nearer, so the set doesn't churn in a flock
	const float BodyIn = 25.f, BodyOut = bSteady ? 30.f : BodyIn, BodyHold = bSteady ? 5.f : 0.f;
	TArray<TPair<float, int32>> Near;
	struct FItem { int32 Id; int32 Species; int32 Slot; FTransform Xf; float Px; };
	TArray<FItem> Items;
	Why.SetNumZeroed(NumBirds);
	DrawnLastFrame = 0;
	for (const FBird& B : World.Birds)
	{
		if (B.Act == EAct::Claimed && BodyOf(B.Id))
		{
			DriveBody(BodyOf(B.Id), World, B.Id);
		}
		const bool bMurmur = B.Act == EAct::Murmur;
		// a murmuration shows as a cloud of specks from kilometres away: its birds' reach eases between 1 and 8 times
		// the usual range over ten seconds (evenly in its logarithm: the range edge never sweeps past a bird faster than
		// ~40 m/s), so a bird leaving the flock far off fades instead of vanishing
		float& R = Reach[B.Id];
		const float WantReach = bMurmur ? 8.f : 1.f;
		const float Ease = FMath::Exp(Dt * 0.2f);
		R = bSteady ? (WantReach > R ? FMath::Min(WantReach, R * Ease) : FMath::Max(WantReach, R / Ease)) : WantReach;
		Why[B.Id] = B.bGone ? 1 : (B.Act == EAct::Claimed ? 3 : 2);
		if (B.bGone || (B.Tier > 2 && R < 1.5f) || B.Act == EAct::Claimed)
		{
			continue;            // a claimed bird is drawn by the player's own body (bird flight)
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		const FVector P = ToUE(World.RenderPos(B));
		const float Dist = (float)FVector::Dist(P, Camera) * 0.01f;
		// fades (shrinks) over the last fifth of its range, and is done before the sim stops moving it (FarDist)
		float Limit = S.Length * DrawLengths * R;
		if (R < 1.5f && bSteady)
		{
			Limit = FMath::Min(Limit, FarDist * 0.95f);
		}
		const float Fade = bSteady ? FMath::Clamp((Limit - Dist) / (Limit * 0.2f), 0.f, 1.f) : (Dist > Limit ? 0.f : 1.f);
		Why[B.Id] = Fade <= 0.f ? 4 : 5;
		if (Fade <= 0.f || (B.Act == EAct::Peck && P.Z < -4350.0))
		{
			continue;            // too small to see, or a cormorant under water
		}
		Why[B.Id] = 0;
		const bool bHadBody = BodyOf(B.Id) != nullptr;
		if (MaxBodies > 0 && (Dist < (bHadBody ? BodyOut : BodyIn) || B.Act == EAct::Approach || B.Act == EAct::Eat))
		{
			Near.Emplace(B.Act == EAct::Approach || B.Act == EAct::Eat ? -1000.f : Dist - (bHadBody ? BodyHold : 0.f), B.Id);
		}
		FSpeciesDraw& D = Species[B.Species];
		// far birds keep at least ~1.2 px so they don't flicker away (1080 px, 90 degrees);
		// a murmuration's birds keep ~2 px: the flock reads as a dark cloud against the evening sky, as it does
		const float Px = S.Length / FMath::Max(Dist, 0.1f) * 540.f * PixelScale;
		const float InFlock = FMath::Clamp((R - 1.f) / 7.f, 0.f, 1.f);
		const float Grow = FMath::Max(1.f, FMath::Lerp(1.2f, 2.f, InFlock) / FMath::Max(Px, 0.01f));
		const float Scale = D.Scale * FMath::Min(Grow, FMath::Lerp(3.f, 8.f, InFlock)) * Fade * (B.bFemale ? D.FemaleScale : 1.f);
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
			if (bMurmur && World.Murmuration())
			{
				// banked in the flock's turns; the beats come in bouts with short glides
				Roll = -FMath::RadiansToDegrees(World.Murmuration()->BankOf(B.Id));
				if (Slow > 0.55f)
				{
					Pose = 6;
				}
			}
			else if (B.Act == EAct::Soar || B.Act == EAct::Land || (S.Flight == EFlight::FlapGlide && Slow > 0.1f) || (S.Flight == EFlight::Aerial && Slow > 0.3f))
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
		Items.Add({ B.Id, B.Species, Pose + (B.bFemale && D.Xf.Num() > NumPoses ? NumPoses : 0), FTransform(Rot, At, S3), Px * Grow * Fade });
	}
	// bodies
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TSet<int32> Keep;
	for (int32 K = 0; K < Near.Num() && Keep.Num() < MaxBodies; K++)
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
			BodyBorn.Remove(It.Key());
			It.RemoveCurrent();
			BodyReleases++;
		}
	}
	BodiesLastFrame = Bodies.Num();
	// a bird with a body is not drawn instanced as well, except the frame or two its new body needs to show posed
	TArray<uint8> Shown;
	Shown.Init(0, NumBirds);
	TArray<float> NowPx;
	NowPx.Init(0.f, NumBirds);
	for (const FItem& I : Items)
	{
		const uint64* Born = BodyBorn.Find(I.Id);
		const bool bBodyReady = Bodies.Contains(I.Id) && (!Born || GFrameCounter > *Born + (bSteady ? 1 : 0));
		NowPx[I.Id] = I.Px;
		if (!bBodyReady)
		{
			Species[I.Species].Xf[I.Slot].Add(I.Xf);
			DrawnLastFrame++;
		}
	}
	for (const TPair<int32, TObjectPtr<AActor>>& Body : Bodies)
	{
		if (Body.Value && !Body.Value->IsHidden())
		{
			NowPx[Body.Key] = FMath::Max(NowPx[Body.Key], 10.f);
		}
	}
	// popping: a bird seen at half a pixel or more that wasn't drawn the frame before, or the other way round
	for (int32 Id = 0; Id < NumBirds; Id++)
	{
		if ((NowPx[Id] > 0.f) != (LastPx[Id] > 0.f) && FMath::Max(NowPx[Id], LastPx[Id]) >= 0.5f)
		{
			Pops++;
			static const TCHAR* const Whys[] = { TEXT("drawn"), TEXT("gone"), TEXT("tier"), TEXT("claimed"), TEXT("range"), TEXT("underwater") };
			const FBird& B = World.Birds[Id];
			PopLog.Add(FString::Printf(TEXT("%s#%d %s->%s act%d tier%d %.0fm %.1fpx->%.1fpx"), UTF8_TO_TCHAR(SpeciesTable()[B.Species].Id.c_str()), Id,
				Whys[FMath::Min<int32>(LastWhy[Id], 5)], Whys[FMath::Min<int32>(Why[Id], 5)], (int32)B.Act, B.Tier,
				(float)FVector::Dist(ToUE(World.RenderPos(B)), Camera) * 0.01f, LastPx[Id], NowPx[Id]));
			if (PopLog.Num() > 12)
			{
				PopLog.RemoveAt(0);
			}
		}
		LastPx[Id] = NowPx[Id];
		LastWhy[Id] = Why[Id];
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
		Body->SetSex(B.bFemale ? EBirdSex::Female : EBirdSex::Male);
		// posed now: a skeletal mesh that only ticks its pose when rendered would show its first frame in the bind pose
		Body->Body->TickAnimation(0.f, false);
		Body->Body->RefreshBoneTransforms();
		Bodies.Add(Id, Body);
		BodyActs.Add(Id, -1.f);
		BodyBorn.Add(Id, GFrameCounter);
		BodySpawns++;
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
		BodyBorn.Remove(Id);
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
