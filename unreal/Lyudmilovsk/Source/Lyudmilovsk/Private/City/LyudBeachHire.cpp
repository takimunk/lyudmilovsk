#include "City/LyudBeachHire.h"

#include "City/LyudBenchSit.h"
#include "City/LyudCityThings.h"
#include "City/LyudPublicSpaces.h"
#include "CityKit.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInterface.h"
#include "NpcDialogue.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudBeach, Log, All);

namespace LyudBeachImpl
{
	const TCHAR* StayId = TEXT("beach_set");
	/** How near one of the beach's places (cm, across; up or down) he is "on the beach". */
	constexpr float NearSand = 3000.f;
	constexpr float SandUpDown = 300.f;
	/** The set stands this far in front of him; the prompt to sit within this of the chair; to move it beyond this. */
	constexpr float Ahead = 120.f;
	constexpr float Reach = 170.f;
	constexpr float MoveBeyond = 1200.f;
	/** The chair's cushion over the sand (the kit's lounger), the parasol's place beside it. */
	constexpr float SeatHeight = 35.f;
	constexpr float ParasolAside = 115.f;
	/** Standing still this long before the set is offered; seated in the sun this long is a sunbath (seconds). */
	constexpr double StillFor = 0.8;
	constexpr double SunFor = 25.0;
}

ULyudBeachHire* ULyudBeachHire::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBeachHire>() : nullptr;
}

void ULyudBeachHire::Deinitialize()
{
	SetPrompt(0);
	Super::Deinitialize();
}

bool ULyudBeachHire::IsHired() const
{
	const ULyudCityThings* Things = ULyudCityThings::Get(this);
	return Things && Things->IsOnStay(LyudBeachImpl::StayId);
}

void ULyudBeachHire::ReadSand()
{
	const ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(this);
	if (!Spots || Spots->NumSpots() == 0)
	{
		return;     // the outdoor map is not up yet: asked again later
	}
	bSandRead = true;
	Sand.Reset();
	for (const FLyudPublicSpot& S : Spots->GetSpots(TEXT("beach")))
	{
		for (const FTransform& Seat : S.Seats)
		{
			Sand.Add(Seat.GetLocation());
		}
	}
	UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: %d places mark the sand"), Sand.Num());
}

bool ULyudBeachHire::IsOnBeach(const AHeroCharacter* Hero) const
{
	if (!Hero)
	{
		return false;
	}
	const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight());
	for (const FVector& P : Sand)
	{
		if (FMath::Abs(P.Z - Feet.Z) <= LyudBeachImpl::SandUpDown && FVector::DistSquared2D(P, Feet) <= FMath::Square(LyudBeachImpl::NearSand))
		{
			return true;
		}
	}
	return false;
}

bool ULyudBeachHire::IsSunny() const
{
	const UWorld* World = GetWorld();
	const ULyudCityThings* Things = ULyudCityThings::Get(this);
	const FDateTime Now = UGameCalendar::NowIn(World);
	return Things && Things->IsClearNow() && Now.GetHour() >= 9 && Now.GetHour() < 19;
}

void ULyudBeachHire::SetPrompt(int32 What)
{
	if (What == PromptShown)
	{
		return;
	}
	PromptShown = What;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	const TCHAR* Key = What == 1 ? TEXT("city.rental.set.prompt") : What == 2 ? TEXT("city.rental.sit.prompt") : What == 3 ? TEXT("city.rental.move.prompt") : nullptr;
	UI->SetPrompt(this, Key ? LyudText::Get(Key) : FText::GetEmpty());
}

bool ULyudBeachHire::EnsureSet()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (IsValid(Set) && IsValid(SetRoot))
	{
		return true;
	}
	// the kit's own lounger and umbrella, built once (as ALyudPublicSpaces builds the beach's)
	if (!ChairMesh || !ParasolMesh)
	{
		CityKit::FMesh Chair, Parasol;
		CityKit::BuildProp(Chair, TEXT("lounger"), 0, nullptr);
		CityKit::BuildProp(Parasol, TEXT("umbrella"), 2, nullptr);
		if (Chair.Idx.Num() == 0 || Parasol.Idx.Num() == 0)
		{
			UE_LOG(LogLyudBeach, Warning, TEXT("Beach hire: the kit has no lounger or umbrella"));
			return false;
		}
		ChairMesh = Chair.Build(this, TEXT("SM_BeachHire_Chair"), false);
		ParasolMesh = Parasol.Build(this, TEXT("SM_BeachHire_Parasol"), false);
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("LyudBeachSet"));
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Set = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!Set)
	{
		return false;
	}
	SetRoot = NewObject<USceneComponent>(Set, TEXT("Root"));
	SetRoot->SetMobility(EComponentMobility::Movable);
	Set->SetRootComponent(SetRoot);
	SetRoot->RegisterComponent();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/City/M_CityProp.M_CityProp"));
	auto Part = [this, Material](const TCHAR* Name, UStaticMesh* Mesh, const FVector& At, float TurnYaw) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Set, Name);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		if (Material)
		{
			C->SetMaterial(0, Material);
		}
		// he walks through it (as through the beach's own loungers), and a thing that is moved stays out of the lit scene's caches
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->SetupAttachment(SetRoot);
		C->SetRelativeLocationAndRotation(At, FRotator(0.f, TurnYaw, 0.f));
		C->RegisterComponent();
		return C;
	};
	// the set's +X is the way the sitter looks; the kit's lounger has its back rest at its own +X, so it is turned round
	ChairComp = Part(TEXT("Chair"), ChairMesh, FVector::ZeroVector, 180.f);
	ParasolComp = Part(TEXT("Parasol"), ParasolMesh, FVector(-20.f, LyudBeachImpl::ParasolAside, 0.f), 0.f);
	return true;
}

bool ULyudBeachHire::SetUpHere(AHeroCharacter* Hero)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || Hero->IsSeated() || !EnsureSet())
	{
		return false;
	}
	const float Yaw = Hero->GetControlRotation().Yaw;
	const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight());
	FVector At = Feet + FRotator(0.f, Yaw, 0.f).Vector() * LyudBeachImpl::Ahead;
	// down onto the sand under that point
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudBeachSet), false, Hero);
	Params.AddIgnoredActor(Set);
	if (World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 150.f), At - FVector(0.f, 0.f, 300.f), ECC_WorldStatic, Params))
	{
		At.Z = Hit.ImpactPoint.Z;
	}
	Set->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
	Set->SetActorHiddenInGame(false);
	StillSince = -1.0;
	UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: the set stands at %s, yaw %.0f"), *At.ToCompactString(), Yaw);
	ULyudQuests::Notify(this, TEXT("do"), TEXT("beach_set"));
	return true;
}

bool ULyudBeachHire::SitInChair(AHeroCharacter* Hero)
{
	if (!Hero || Hero->IsSeated() || !IsValid(Set) || !IsValid(SetRoot))
	{
		return false;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	// the seat's frame is the set's own (it never moves while he sits): the sand under the cushion a hand in front of his
	// hips, which are by the back rest (the turned lounger's back starts 45 cm behind the set's origin, his legs lie forward)
	Hero->SitOn(SetRoot, FTransform(FRotator::ZeroRotator, FVector(-7.f, 0.f, 0.f)), LyudBeachImpl::SeatHeight);
	Hero->SetStandUpOffset(FVector(0.f, -70.f, 0.f));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(4.f, Set->GetActorRotation().Yaw, 0.f));
	}
	SeatedHero = Hero;
	SunSince = -1.0;
	UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: he sits in the deck chair"));
	return true;
}

void ULyudBeachHire::TakeBack()
{
	if (AHeroCharacter* Hero = SeatedHero.Get())
	{
		if (Hero->IsSeated())
		{
			Hero->StandUp();
		}
	}
	SeatedHero.Reset();
	if (IsValid(Set))
	{
		Set->Destroy();
	}
	Set = nullptr;
	SetRoot = nullptr;
	ChairComp = nullptr;
	ParasolComp = nullptr;
	SunSince = -1.0;
}

void ULyudBeachHire::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	const double Now = World->GetTimeSeconds();
	const bool bHired = IsHired();
	// up from the chair (a step, Space, a load, the pawn gone)
	if (SeatedHero.IsValid() && (SeatedHero.Get() != Hero || !Hero->IsSeated()))
	{
		SeatedHero.Reset();
		SunSince = -1.0;
	}
	// the hire is over: the attendant has been and taken the set
	if (!bHired && IsValid(Set))
	{
		const bool bNear = Hero && FVector::DistSquared(Hero->GetActorLocation(), Set->GetActorLocation()) < FMath::Square(3000.f);
		TakeBack();
		if (bNear)
		{
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(TEXT("city.rental.taken_back")));
			}
		}
	}
	if (!bHired || !Hero || Hero->IsHidden() || Hero->IsOnBike() || UGameplayStatics::IsGamePaused(World))
	{
		SetPrompt(0);
		return;
	}
	// in the chair: a while in the sun is a sunbath, once a day
	if (SeatedHero.IsValid())
	{
		SetPrompt(0);
		const int64 Day = UGameCalendar::NowIn(World).GetDate().GetTicks() / ETimespan::TicksPerDay;
		if (Day != SunDay && IsSunny())
		{
			SunSince = SunSince < 0.0 ? Now : SunSince;
			if (Now - SunSince >= LyudBeachImpl::SunFor)
			{
				SunDay = Day;
				SunSince = -1.0;
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					Screen->Toast(LyudText::Get(TEXT("city.rental.sunbath.toast")));
				}
				if (ULyudQuests* Quests = ULyudQuests::Get(this))
				{
					Quests->AddNote(TEXT("city.rental.sunbath.diary"));
				}
				ULyudQuests::Notify(this, TEXT("do"), TEXT("sunbathe"));
			}
		}
		else
		{
			SunSince = -1.0;
		}
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudCityThings* Things = ULyudCityThings::Get(World);
	const ULyudBenchSit* Bench = ULyudBenchSit::Get(World);
	const bool bBusy = (UI && UI->IsOpen()) || (Things && (Things->HasThingInView() || Things->HasModeUp())) || (Bench && Bench->IsOnBench()) || Hero->IsSeated();
	if (bBusy)
	{
		SetPrompt(0);
		StillSince = -1.0;
		return;
	}
	if (!bSandRead)
	{
		ReadSand();
	}
	if (Now >= NextLook)
	{
		NextLook = Now + 0.2;
		Offer = 0;
		const bool bStandsStill = Hero->GetVelocity().SizeSquared2D() < FMath::Square(15.f) && Hero->GetCharacterMovement() && Hero->GetCharacterMovement()->IsMovingOnGround();
		StillSince = bStandsStill ? (StillSince < 0.0 ? Now : StillSince) : -1.0;
		const bool bSetStands = IsValid(Set);
		const float ToSet = bSetStands ? (float)FVector::Dist2D(Hero->GetActorLocation(), Set->GetActorLocation()) : 0.f;
		if (bSetStands && ToSet <= LyudBeachImpl::Reach)
		{
			Offer = 2;
		}
		else if (StillSince >= 0.0 && Now - StillSince >= LyudBeachImpl::StillFor && (!bSetStands || ToSet > LyudBeachImpl::MoveBeyond) && IsOnBeach(Hero))
		{
			Offer = bSetStands ? 3 : 1;
		}
	}
	SetPrompt(Offer);
	if (Offer != 0 && ANpcTalkDirector::UsePressed(PC))
	{
		const int32 Do = Offer;
		Offer = 0;
		SetPrompt(0);
		if (Do == 2)
		{
			SitInChair(Hero);
		}
		else
		{
			SetUpHere(Hero);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBeachHire(TEXT("lyud.Beach.Hire"),
	TEXT("lyud.Beach.Hire [days]: hires the rental's beach set (the stay beach_set) for so many days (default 1; 0 ends the hire)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			const int32 Days = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
			Things->StartStay(LyudBeachImpl::StayId, Days);
			UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: %d days, hired=%d"), Days, Things->IsOnStay(LyudBeachImpl::StayId) ? 1 : 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBeachSet(TEXT("lyud.Beach.Set"),
	TEXT("lyud.Beach.Set: sets the deck chair and the parasol up a step in front of the hero (anywhere; the set goes at the next tick unless it is hired)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudBeachHire* B = ULyudBeachHire::Get(World))
		{
			AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
			UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: set up here: %d (hired %d, on the beach %d)"), B->SetUpHere(Hero) ? 1 : 0, B->IsHired() ? 1 : 0, B->IsOnBeach(Hero) ? 1 : 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBeachSit(TEXT("lyud.Beach.Sit"),
	TEXT("lyud.Beach.Sit: sits the hero in the deck chair of the set that stands"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudBeachHire* B = ULyudBeachHire::Get(World))
		{
			UE_LOG(LogLyudBeach, Display, TEXT("Beach hire: sit: %d"), B->SitInChair(Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))) ? 1 : 0);
		}
	}));

static LyudDev::FAutoAction GBeachDevSet(TEXT("interiors.beach_set"), TEXT("interiors"), TEXT("dev.action.interiors.beach_set"), [](UWorld* W)
{
	// hires the set for the day and puts it up where he stands
	ULyudCityThings* Things = ULyudCityThings::Get(W);
	ULyudBeachHire* B = ULyudBeachHire::Get(W);
	if (Things && B)
	{
		Things->StartStay(LyudBeachImpl::StayId, 1);
		B->SetUpHere(Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(W, 0)));
	}
});
