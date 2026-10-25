#include "Animals/AnimalLife.h"

#include "AnimalSim.h"
#include "Animals/AnimalActor.h"
#include "Birds/BirdLife.h"
#include "Birds/BirdSpecies.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantModel.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sky/Ephemeris.h"

#include <algorithm>
#include <cmath>

using namespace LyudAnimals;

static TAutoConsoleVariable<int32> CVarAnimals(TEXT("lyud.Animals"), 1,
	TEXT("The town's cats and dogs (UAnimalLifeSubsystem): 1 on, 0 off (removes them at once; for benchmark A/B)."));
static TAutoConsoleVariable<float> CVarAnimalsPopulation(TEXT("lyud.Animals.Population"), 1.f,
	TEXT("Share of the cats and dogs to create (read when the animal life starts)."));
static TAutoConsoleVariable<int32> CVarAnimalsAnyMap(TEXT("lyud.Animals.AnyMap"), 0,
	TEXT("Start the animal life in any game map, not only Lvl_Scene01 (empty, for lyud.Animals.SpawnNear)."));
static TAutoConsoleVariable<int32> CVarAnimalsBodies(TEXT("lyud.Animals.Bodies"), 20,
	TEXT("Most cats and dogs drawn as furred bodies (AAnimalActor) at once, the nearest first."));
static TAutoConsoleVariable<float> CVarAnimalsDrawCat(TEXT("lyud.Animals.DrawDistance"), 110.f,
	TEXT("Cats are drawn within this many metres of the camera (dogs within 1.6 times it)."));
static TAutoConsoleVariable<int32> CVarAnimalsIgnorePlayer(TEXT("lyud.Animals.IgnorePlayer"), 0,
	TEXT("1: the animals don't perceive the player (close-up screenshots, debugging)."));
static TAutoConsoleVariable<int32> CVarAnimalsBirds(TEXT("lyud.Animals.Birds"), 1,
	TEXT("1: cats and dogs and the bird life see each other (stalking, chasing, birds flee); 0: they ignore each other."));

namespace
{
	FVector ToUE(const V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	V3 FromUE(const FVector& V) { return V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }

	const TCHAR* BreedName(const FAnimal& A)
	{
		if (A.Kind == EKind::Cat)
		{
			return TEXT("Cat");
		}
		return A.Breed == 1 ? TEXT("Shepherd") : (A.Breed == 2 ? TEXT("Mutt") : TEXT("Laika"));
	}

	float DayHour(const FDateTime& T) { return T.GetHour() + T.GetMinute() / 60.f + T.GetSecond() / 3600.f; }

	/** Sunrise and sunset (local clock, hours) at the game's place on a date (as the bird life does). */
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

	bool IsTree(FName Species)
	{
		// shrubs and young plants are no refuge: only trees with a trunk to go up
		static const FName No[] = { TEXT("Juniper"), TEXT("SeaBuckthorn") };
		for (const FName& N : No)
		{
			if (Species == N)
			{
				return false;
			}
		}
		return true;
	}
}

// --- the subsystem ------------------------------------------------------------------------------------

UAnimalLifeSubsystem::UAnimalLifeSubsystem() = default;
UAnimalLifeSubsystem::~UAnimalLifeSubsystem() = default;

UAnimalLifeSubsystem* UAnimalLifeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAnimalLifeSubsystem>() : nullptr;
}

bool UAnimalLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UAnimalLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnimalLifeSubsystem, STATGROUP_Tickables);
}

void UAnimalLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWantStart = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")) || CVarAnimalsAnyMap.GetValueOnGameThread() != 0;
	StartDelay = 0.6f;     // after the forests have scattered their trees
}

void UAnimalLifeSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

void UAnimalLifeSubsystem::InstallCallbacks()
{
	UWorld* World = GetWorld();
	FWorld& S = *Sim;
	// exact ground under an animal near the camera: from a little above the walk grid's height down (stairs, kerbs, the
	// Academgorodok's steps), never the roofs or the crowns over it
	S.Probe = [this, World](float X, float Y, float& Z)
	{
		if (!Sim)
		{
			return false;
		}
		const float G = Sim->GroundZ(X, Y);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AnimalGround), false);
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Q.AddIgnoredActor(Pawn);
		}
		if (World->LineTraceSingleByChannel(Hit, FVector(X * 100.0, Y * 100.0, (G + 1.2f) * 100.0), FVector(X * 100.0, Y * 100.0, (G - 2.5f) * 100.0), ECC_Visibility, Q))
		{
			Z = (float)Hit.ImpactPoint.Z * 0.01f;
			return true;
		}
		return false;
	};
	S.Sight = [World](const V3& From, const V3& To)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AnimalSight), false);
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Q.AddIgnoredActor(Pawn);
		}
		return !World->LineTraceSingleByChannel(Hit, ToUE(From), ToUE(To), ECC_Visibility, Q);
	};
}

bool UAnimalLifeSubsystem::Start()
{
	UWorld* World = GetWorld();
	TPimplPtr<FWorld> W = MakePimpl<FWorld>();
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("AnimalsData"));
	if (!W->Load(TCHAR_TO_UTF8(*Dir)))
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimalLife: %s"), UTF8_TO_TCHAR(W->LastError.c_str()));
		return false;
	}
	Sim = MoveTemp(W);
	FWorld& S = *Sim;
	S.Opt.PopulationScale = FMath::Max(0.f, CVarAnimalsPopulation.GetValueOnGameThread());
	AddRealTrees();
	InstallCallbacks();
	UpdateClock();
	S.Populate();
	SyncMemory(false);
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	Renderer = World->SpawnActor<AAnimalLifeRenderer>(P);
	if (ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(World))
	{
		Save->RegisterSaveObject(TEXT("AnimalLife"), this);
		Save->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncMemory(false); });
		Save->OnBeforeSave.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncMemory(true); });
	}
	int32 Cats = 0, Dogs = 0;
	for (const FAnimal& A : S.Animals)
	{
		(A.Kind == EKind::Cat ? Cats : Dogs)++;
	}
	UE_LOG(LogTemp, Display, TEXT("AnimalLife: %d cats, %d dogs in %d packs, %d places; sun %.2f..%.2f"), Cats, Dogs, (int32)S.Packs.size(),
		(int32)S.Places.size(), S.Sunrise, S.Sunset);
	return true;
}

bool UAnimalLifeSubsystem::StartEmpty(float GroundZ)
{
	UWorld* World = GetWorld();
	Sim = MakePimpl<FWorld>();
	FWorld& S = *Sim;
	S.NoGridZ = GroundZ;
	InstallCallbacks();
	UpdateClock();
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	Renderer = World->SpawnActor<AAnimalLifeRenderer>(P);
	bWantStart = false;
	bStarted = true;
	UE_LOG(LogTemp, Display, TEXT("AnimalLife: started empty (no town map) for test animals"));
	return true;
}

void UAnimalLifeSubsystem::Stop()
{
	if (Renderer)
	{
		Renderer->Destroy();
		Renderer = nullptr;
	}
	if (UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(GetWorld()))
	{
		Birds->SetBeasts(TArray<FBirdBeast>());
	}
	Sim.Reset();
	bStarted = false;
}

/** Trees of the town and the Academgorodok's square (the forests' and the hero plants'): what a cat runs up. The map's
 *  guessed yard trees are dropped once the real ones are there. */
void UAnimalLifeSubsystem::AddRealTrees()
{
	UWorld* World = GetWorld();
	FWorld& S = *Sim;
	int32 Trees = 0;
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		Trees += It->NumTrees();
	}
	if (Trees == 0)
	{
		return;
	}
	S.Places.erase(std::remove_if(S.Places.begin(), S.Places.end(), [](const FPlace& P) { return P.Kind == EPlace::Refuge && P.Tag == "guess"; }), S.Places.end());
	// only trees near where cats live (a doorstep, a cellar, a feeder within 120 m): the rest would only cost
	TSet<FIntPoint> Lived;
	for (const FPlace& P : S.Places)
	{
		if (P.Kind == EPlace::Doorstep || P.Kind == EPlace::Basement || P.Kind == EPlace::Feeder)
		{
			Lived.Add(FIntPoint(FMath::FloorToInt(P.Pos.X / 60.f), FMath::FloorToInt(P.Pos.Y / 60.f)));
		}
	}
	auto NearLife = [&](const V3& P)
	{
		const FIntPoint C(FMath::FloorToInt(P.X / 60.f), FMath::FloorToInt(P.Y / 60.f));
		for (int32 DY = -2; DY <= 2; DY++)
		{
			for (int32 DX = -2; DX <= 2; DX++)
			{
				if (Lived.Contains(C + FIntPoint(DX, DY)))
				{
					return true;
				}
			}
		}
		return false;
	};
	TSet<FIntPoint> Taken;
	int32 Added = 0;
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		It->ForEachTree([&](FName Species, const FTransform& Xf, float Age)
		{
			if (Age < 8.f || !IsTree(Species))
			{
				return;
			}
			const V3 P = FromUE(Xf.GetLocation());
			if (!S.Walkable(P.X, P.Y) || !NearLife(P))
			{
				return;
			}
			const FIntPoint Cell(FMath::FloorToInt(P.X / 6.f), FMath::FloorToInt(P.Y / 6.f));     // one per 6 m
			if (Taken.Contains(Cell))
			{
				return;
			}
			Taken.Add(Cell);
			S.AddRefuge(P, FMath::Clamp(Age * 0.12f, 1.8f, 3.2f));
			Added++;
		});
	}
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		const V3 P = FromUE(It->GetActorLocation());
		if (S.Walkable(P.X, P.Y) && IsTree(It->Species))
		{
			S.AddRefuge(P, 2.4f);
			Added++;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("AnimalLife: %d trees for the cats to go up (of %d forest trees)"), Added, Trees);
}

void UAnimalLifeSubsystem::UpdateClock()
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
	}
	if (bJumped)
	{
		S.Resettle();
		UE_LOG(LogTemp, Display, TEXT("AnimalLife: the clock jumped %.2f h -> %.2f h; animals resettled"), Was, S.Hour);
	}
}

void UAnimalLifeSubsystem::UpdatePlayer()
{
	FWorld& S = *Sim;
	FPlayer& P = S.Player;
	const UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	P.bPresent = Pawn != nullptr && CVarAnimalsIgnorePlayer.GetValueOnGameThread() == 0;
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	if (PC)
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		S.Camera = FromUE(CamLoc);
	}
	if (!Pawn)
	{
		return;
	}
	const ACharacter* Char = Cast<ACharacter>(Pawn);
	const float Half = Pawn->GetSimpleCollisionHalfHeight();
	P.Feet = FromUE(Pawn->GetActorLocation() - FVector(0.0, 0.0, Half));
	P.Vel = FromUE(Pawn->GetVelocity());
	P.bCrouched = Char && Char->bIsCrouched;
	P.Eye = PC ? FromUE(CamLoc) : P.Feet + V3(0.f, 0.f, 1.6f);
	const FVector F = CamRot.Vector();
	P.Facing = V3((float)F.X, (float)F.Y, (float)F.Z);
	if (!PC)
	{
		S.Camera = P.Eye;
	}
}

/** Birds on the ground near the cats and dogs go to the animals as prey; the cats and dogs go to the bird life as
 *  beasts to flee from; birds a cat pounced at or a dog ran into are scared up. */
void UAnimalLifeSubsystem::LinkBirds(float DeltaTime)
{
	FWorld& S = *Sim;
	UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(GetWorld());
	const bool bOn = Birds && Birds->IsRunning() && CVarAnimalsBirds.GetValueOnGameThread() != 0;
	if (!bOn)
	{
		S.Prey.clear();
		S.ScaredBirds.clear();
		return;
	}
	TArray<FBirdBeast> Beasts;
	for (const FAnimal& A : S.Animals)
	{
		if (A.Act == EAct::Sleep || A.Up > 0.5f || A.Tier > 2)
		{
			continue;
		}
		FBirdBeast B;
		B.Location = ToUE(A.Pos);
		B.Velocity = ToUE(A.Vel);
		B.bCat = A.Kind == EKind::Cat;
		B.bStalking = A.Act == EAct::Stalk || A.Act == EAct::Crouch;
		Beasts.Add(B);
	}
	Birds->SetBeasts(Beasts);
	for (int32 Id : S.ScaredBirds)
	{
		FBirdLifeInfo Info;
		if (Birds->GetBird(Id, Info))
		{
			Birds->Scare(Info.Location, 400.f, 1.4f);
		}
	}
	S.ScaredBirds.clear();
	// the prey list, a few times a second: ground birds within 25 m of an awake animal near the camera
	PreyTimer -= DeltaTime;
	if (PreyTimer > 0.f)
	{
		return;
	}
	PreyTimer = 0.25f;
	S.Prey.clear();
	TSet<int32> Seen;
	for (const FAnimal& A : S.Animals)
	{
		if (A.Tier > 1 || A.Act == EAct::Sleep || A.Up > 0.f)
		{
			continue;
		}
		for (int32 Id : Birds->FindBirdsNear(ToUE(A.Pos), 2500.f))
		{
			if (Seen.Contains(Id))
			{
				continue;
			}
			Seen.Add(Id);
			FBirdLifeInfo Info;
			if (!Birds->GetBird(Id, Info))
			{
				continue;
			}
			FPrey P;
			P.Id = Id;
			P.Pos = FromUE(Info.Location);
			const FBirdSpecies* Sp = UBirdSpeciesLibrary::Find(Info.SpeciesId);
			P.Size = Sp ? Sp->LengthM() : 0.2f;
			const bool bGroundAct = Info.Activity == EBirdActivity::Forage || Info.Activity == EBirdActivity::Peck
				|| Info.Activity == EBirdActivity::Watch || Info.Activity == EBirdActivity::Perch || Info.Activity == EBirdActivity::Preen;
			P.bGround = bGroundAct && P.Pos.Z - S.GroundZ(P.Pos.X, P.Pos.Y) < 0.6f;
			S.Prey.push_back(P);
		}
		if (S.Prey.size() > 200)
		{
			break;
		}
	}
}

void UAnimalLifeSubsystem::Tick(float DeltaTime)
{
	if (CVarAnimals.GetValueOnGameThread() == 0)
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
	UpdatePlayer();
	LinkBirds(DeltaTime);
	S.Step(DeltaTime);
	DispatchEvents();
	if (Renderer)
	{
		Renderer->Draw(S, ToUE(S.Camera));
	}
	SaveTimer -= DeltaTime;
	if (SaveTimer <= 0.f)
	{
		SaveTimer = 2.f;
		SyncMemory(true);
	}
}

void UAnimalLifeSubsystem::DispatchEvents()
{
	FWorld& S = *Sim;
	const std::vector<LyudAnimals::FEvent> Events = S.Events;
	S.Events.clear();
	for (const LyudAnimals::FEvent& E : Events)
	{
		const FName Name(EventName(E.Kind));
		if (Renderer)
		{
			if (AAnimalActor* Body = Renderer->BodyOf(E.Animal))
			{
				Body->Vocalize(Name);
			}
		}
		OnAnimalEvent.Broadcast(E.Animal, Name);
	}
}

void UAnimalLifeSubsystem::SyncMemory(bool bToSave)
{
	if (!Sim)
	{
		return;
	}
	for (FAnimal& A : Sim->Animals)
	{
		if (bToSave)
		{
			if (A.Trust > 0.f || A.Bond > 0.f || !A.Name.empty())
			{
				FAnimalMemory& M = Saved.FindOrAdd(A.Id);
				M.Trust = A.Trust;
				M.Bond = A.Bond;
				M.Name = UTF8_TO_TCHAR(A.Name.c_str());
			}
		}
		else if (const FAnimalMemory* M = Saved.Find(A.Id))
		{
			A.Trust = M->Trust;
			A.Bond = M->Bond;
			A.Name = TCHAR_TO_UTF8(*M->Name);
		}
	}
}

bool UAnimalLifeSubsystem::GetAnimal(int32 Id, FAnimalInfo& Out) const
{
	if (!Sim || Id < 0 || Id >= (int32)Sim->Animals.size())
	{
		return false;
	}
	const FAnimal& A = Sim->Animals[Id];
	Out.Id = Id;
	Out.Kind = A.Kind == EKind::Cat ? EAnimalKind::Cat : EAnimalKind::Dog;
	Out.Breed = BreedName(A);
	Out.Coat = NAME_None;
	Out.bFemale = A.bFemale;
	Out.bYoung = A.bYoung;
	Out.Location = ToUE(Sim->RenderPos(A));
	Out.Velocity = ToUE(A.Vel);
	Out.Yaw = FMath::RadiansToDegrees(A.Yaw);
	Out.Activity = (EAnimalActivity)(uint8)A.Act;
	Out.Goal = GoalName(A.Goal);
	Out.Hunger = A.Hunger;
	Out.Tired = A.Tired;
	Out.Playful = A.Playful;
	Out.Lonely = A.Lonely;
	Out.Curious = A.Curious;
	Out.Fear = A.Fear;
	Out.Trust = A.Trust;
	Out.Bond = A.Bond;
	Out.Name = UTF8_TO_TCHAR(A.Name.c_str());
	Out.bNoticedPlayer = A.bNoticed;
	Out.Actor = Renderer ? Renderer->BodyOf(Id) : nullptr;
	return true;
}

TArray<int32> UAnimalLifeSubsystem::FindAnimalsNear(FVector Location, float Radius) const
{
	TArray<int32> Out;
	if (!Sim)
	{
		return Out;
	}
	std::vector<int> Ids;
	Sim->Near(FromUE(Location), Radius * 0.01f, Ids);
	for (int I : Ids)
	{
		Out.Add(I);
	}
	return Out;
}

void UAnimalLifeSubsystem::Scare(FVector Location, float Radius, float Strength)
{
	if (Sim)
	{
		Sim->Scare(FromUE(Location), Radius * 0.01f, Strength);
	}
}

int32 UAnimalLifeSubsystem::SpawnAnimal(FName Breed, int32 Coat, FVector Location, bool bYoung)
{
	if (!Sim && !StartEmpty((float)Location.Z * 0.01f))
	{
		return -1;
	}
	const FString B = Breed.ToString();
	const EKind Kind = B.Equals(TEXT("Cat"), ESearchCase::IgnoreCase) ? EKind::Cat : EKind::Dog;
	const int Br = B.Equals(TEXT("Shepherd"), ESearchCase::IgnoreCase) ? 1 : (B.Equals(TEXT("Mutt"), ESearchCase::IgnoreCase) ? 2 : 0);
	V3 P = FromUE(Location);
	const int Id = Sim->AddAnimal(Kind, Br, FMath::Max(0, Coat), P, bYoung);
	FAnimal& A = Sim->Animals[Id];
	A.Range = 6.f;           // keeps round its spot
	A.Friendly = 0.7f;
	return Id;
}

void UAnimalLifeSubsystem::SetAnimalName(int32 Id, const FString& InName)
{
	if (Sim && Id >= 0 && Id < (int32)Sim->Animals.size())
	{
		Sim->Animals[Id].Name = TCHAR_TO_UTF8(*InName);
		SyncMemory(true);
	}
}

int32 UAnimalLifeSubsystem::NumAnimals() const
{
	return Sim ? (int32)Sim->Animals.size() : 0;
}

FString UAnimalLifeSubsystem::Report() const
{
	if (!Sim)
	{
		return TEXT("animal life not running");
	}
	return FString::Printf(TEXT("%s | bodies %d"), UTF8_TO_TCHAR(Sim->Report().c_str()), Renderer ? Renderer->BodiesLastFrame : 0);
}

FString UAnimalLifeSubsystem::Describe(int32 Id) const
{
	return Sim ? FString(UTF8_TO_TCHAR(Sim->Describe(Id).c_str())) : FString();
}

void UAnimalLifeSubsystem::DumpDay(float StartHour, float Hours, const FString& OutCsv)
{
	if (!Sim)
	{
		return;
	}
	FWorld& S = *Sim;
	FString Csv = TEXT("hour,id,kind,act,goal,x,y\n");
	const float Dt = 0.2f;
	float Hour = StartHour, Next = 0.f;
	S.Probe = nullptr;
	S.Sight = nullptr;
	for (float T = 0.f; T < Hours * 3600.f; T += Dt)
	{
		Hour += Dt / 3600.f;
		S.Hour = FMath::Fmod(Hour, 24.f);
		S.Step(Dt);
		S.Events.clear();
		if (T >= Next)
		{
			Next += 600.f;
			for (const FAnimal& A : S.Animals)
			{
				Csv += FString::Printf(TEXT("%.2f,%d,%s,%s,%s,%.1f,%.1f\n"), S.Hour, A.Id, A.Kind == EKind::Cat ? TEXT("Cat") : TEXT("Dog"),
					UTF8_TO_TCHAR(ActName(A.Act)), UTF8_TO_TCHAR(GoalName(A.Goal)), A.Pos.X, A.Pos.Y);
			}
		}
	}
	FFileHelper::SaveStringToFile(Csv, *OutCsv);
	InstallCallbacks();
}

// --- console ------------------------------------------------------------------------------------------

static void AnimalsReport(const TArray<FString>& Args, UWorld* World)
{
	if (UAnimalLifeSubsystem* L = UAnimalLifeSubsystem::Get(World))
	{
		UE_LOG(LogTemp, Display, TEXT("AnimalLife: %s"), *L->Report());
		if (Args.Num() > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("AnimalLife: %s"), *L->Describe(FCString::Atoi(*Args[0])));
		}
	}
}

static void AnimalsSpawnNear(const TArray<FString>& Args, UWorld* World)
{
	UAnimalLifeSubsystem* L = UAnimalLifeSubsystem::Get(World);
	if (!L || Args.Num() < 1)
	{
		return;
	}
	FVector At = FVector::ZeroVector;
	if (Args.Num() >= 5)
	{
		At = FVector(FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), FCString::Atof(*Args[4]));
	}
	else if (APlayerController* PC = World->GetFirstPlayerController())
	{
		FVector Loc;
		FRotator Rot;
		PC->GetPlayerViewPoint(Loc, Rot);
		At = Loc + FRotator(0.f, Rot.Yaw, 0.f).Vector() * 300.f;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0, 0, 300), At - FVector(0, 0, 3000), ECC_Visibility))
		{
			At = Hit.ImpactPoint;
		}
	}
	const int32 Id = L->SpawnAnimal(FName(*Args[0]), Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0, At);
	UE_LOG(LogTemp, Display, TEXT("AnimalLife: spawned %s %d at %s"), *Args[0], Id, *At.ToString());
}

static void AnimalsDumpDay(const TArray<FString>& Args, UWorld* World)
{
	if (UAnimalLifeSubsystem* L = UAnimalLifeSubsystem::Get(World))
	{
		const float H = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
		const float N = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 24.f;
		const FString Out = Args.Num() > 2 ? Args[2] : FPaths::ProjectSavedDir() / TEXT("AnimalSim/day.csv");
		L->DumpDay(H, N, Out);
		UE_LOG(LogTemp, Display, TEXT("AnimalLife: day dumped to %s"), *Out);
	}
}

static void AnimalsScare(const TArray<FString>& Args, UWorld* World)
{
	UAnimalLifeSubsystem* L = UAnimalLifeSubsystem::Get(World);
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (L && PC && PC->GetPawn())
	{
		L->Scare(PC->GetPawn()->GetActorLocation(), (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 20.f) * 100.f, 1.f);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GAnimalsReport(TEXT("lyud.Animals.Report"), TEXT("lyud.Animals.Report [id]: what the cats and dogs do (and one animal's needs)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AnimalsReport));
static FAutoConsoleCommandWithWorldAndArgs GAnimalsSpawn(TEXT("lyud.Animals.SpawnNear"), TEXT("lyud.Animals.SpawnNear <Cat|Laika|Shepherd|Mutt> [coat] [x y z]: one animal that keeps to its spot (any level)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AnimalsSpawnNear));
static FAutoConsoleCommandWithWorldAndArgs GAnimalsDump(TEXT("lyud.Animals.DumpDay"), TEXT("lyud.Animals.DumpDay [start_hour hours csv]: the day headless, for tools/animalsim/render_animals.py."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AnimalsDumpDay));
static FAutoConsoleCommandWithWorldAndArgs GAnimalsScare(TEXT("lyud.Animals.Scare"), TEXT("lyud.Animals.Scare [m]: a loud noise at the player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AnimalsScare));

// --- drawing ------------------------------------------------------------------------------------------

AAnimalLifeRenderer::AAnimalLifeRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AAnimalActor* AAnimalLifeRenderer::BodyOf(int32 Id) const
{
	const TObjectPtr<AAnimalActor>* B = Bodies.Find(Id);
	return B ? B->Get() : nullptr;
}

void AAnimalLifeRenderer::Draw(const FWorld& World, const FVector& Camera)
{
	const int32 Max = FMath::Max(0, CVarAnimalsBodies.GetValueOnGameThread());
	const float CatDist = CVarAnimalsDrawCat.GetValueOnGameThread() * 100.f;
	// the nearest animals within their draw distance (dogs are bigger: seen farther), not ones gone into a cellar
	TArray<TPair<float, int32>> Want;
	for (const FAnimal& A : World.Animals)
	{
		const float D = (float)FVector::Dist(ToUE(World.RenderPos(A)), Camera);
		const float Lim = A.Kind == EKind::Cat ? CatDist : CatDist * 1.6f;
		if (D < Lim)
		{
			Want.Emplace(D, A.Id);
		}
	}
	Want.Sort([](const TPair<float, int32>& L, const TPair<float, int32>& R) { return L.Key < R.Key; });
	if (Want.Num() > Max)
	{
		Want.SetNum(Max);
	}
	TSet<int32> Keep;
	for (const auto& W : Want)
	{
		Keep.Add(W.Value);
	}
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		if (!Keep.Contains(It->Key))
		{
			if (It->Value)
			{
				It->Value->SetActorHiddenInGame(true);
				It->Value->SetActorTickEnabled(false);
				Spare.Add(It->Value);
			}
			It.RemoveCurrent();
		}
	}
	for (const auto& W : Want)
	{
		const int32 Id = W.Value;
		const FAnimal& A = World.Animals[Id];
		AAnimalActor* Body = BodyOf(Id);
		const FName Breed = BreedName(A);
		if (!Body)
		{
			// a spare body of the same breed if there is one (its mesh stays loaded), else a new one
			for (int32 I = Spare.Num() - 1; I >= 0; --I)
			{
				if (Spare[I] && Spare[I]->Breed == Breed)
				{
					Body = Spare[I];
					Spare.RemoveAt(I);
					break;
				}
			}
			const FTransform At(FRotator(0.f, FMath::RadiansToDegrees(A.Yaw), 0.f), ToUE(World.RenderPos(A)));
			if (!Body)
			{
				Body = GetWorld()->SpawnActorDeferred<AAnimalActor>(AAnimalActor::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Body)
				{
					continue;
				}
				Body->Breed = Breed;
				Body->Coat = A.Coat;
				Body->Size = A.Size;
				Body->Seed = Id + 1;
				Body->InitialActivity = (EAnimalActivity)(uint8)A.Act;
				Body->SetFlags(RF_Transient);
				Body->FinishSpawning(At);
			}
			else
			{
				Body->Size = A.Size;
				Body->SetActorTransform(At);
				if (Body->Coat != A.Coat)
				{
					Body->SetBreed(Breed, A.Coat);
				}
				Body->SetActorHiddenInGame(false);
				Body->SetActorTickEnabled(true);
				Body->SetActivity((EAnimalActivity)(uint8)A.Act, A.Speed * 100.f);
			}
			Body->Body->SetRelativeScale3D(FVector(A.Size));
			Bodies.Add(Id, Body);
		}
		Drive(Body, World, Id);
	}
	BodiesLastFrame = Bodies.Num();
}

void AAnimalLifeRenderer::Drive(AAnimalActor* Body, const FWorld& World, int32 Id)
{
	const FAnimal& A = World.Animals[Id];
	const V3 P = World.RenderPos(A);
	// the body follows the ground's slope a little (pitch along its heading, roll across), not when up a tree
	float Pitch = 0.f, Roll = 0.f;
	if (A.Up <= 0.f && A.Act != EAct::Sleep)
	{
		const float C = std::cos(A.Yaw), Sn = std::sin(A.Yaw);
		const float L = A.Kind == EKind::Cat ? 0.22f : 0.38f;
		const float Zf = World.GroundZ(P.X + C * L, P.Y + Sn * L), Zb = World.GroundZ(P.X - C * L, P.Y - Sn * L);
		const float Zl = World.GroundZ(P.X - Sn * 0.1f, P.Y + C * 0.1f), Zr = World.GroundZ(P.X + Sn * 0.1f, P.Y - C * 0.1f);
		Pitch = FMath::Clamp(FMath::RadiansToDegrees(std::atan2(Zf - Zb, 2.f * L)) * 0.7f, -25.f, 25.f);
		Roll = FMath::Clamp(FMath::RadiansToDegrees(std::atan2(Zl - Zr, 0.2f)) * 0.4f, -12.f, 12.f);
	}
	Body->SetActorLocationAndRotation(ToUE(P), FRotator(-Pitch, FMath::RadiansToDegrees(A.Yaw), Roll));
	Body->SetActivity((EAnimalActivity)(uint8)A.Act, A.Speed * 100.f);
	if (A.bLook)
	{
		Body->SetLookTarget(ToUE(A.Look), true);
	}
	else
	{
		Body->SetLookTarget(FVector::ZeroVector, false);
	}
	const float Friendly = A.Kind == EKind::Dog ? FMath::Clamp(A.Friendly * 0.5f + A.Trust * 0.5f + (A.Act == EAct::Greet || A.Act == EAct::Beg ? 0.4f : 0.f), 0.f, 1.f) : 0.f;
	Body->SetMood(FMath::Clamp(A.Excite + (A.Act == EAct::Play || A.Act == EAct::PlayBow ? 0.6f : 0.f), 0.f, 1.f), FMath::Clamp(A.Fear + (A.Act == EAct::Hiss ? 1.f : 0.f), 0.f, 1.f), Friendly);
	// shadows only near the camera (the fur shells cost in the shadow pass too)
	const bool bShadow = A.Tier == 0;
	if (Body->Body->CastShadow != bShadow)
	{
		Body->Body->SetCastShadow(bShadow);
	}
}
