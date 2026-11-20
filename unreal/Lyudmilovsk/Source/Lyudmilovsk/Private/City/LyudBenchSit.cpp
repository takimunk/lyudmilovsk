#include "City/LyudBenchSit.h"

#include "Books/LyudBooks.h"
#include "City/LyudCityThings.h"
#include "City/LyudPublicSpaces.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "NpcDialogue.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudBench, Log, All);

namespace LyudBenchImpl
{
	/** How near (cm, across) and how high above or below his feet a seat may be to be offered. */
	constexpr float Reach = 150.f;
	constexpr float ReachUpDown = 90.f;
}

ULyudBenchSit* ULyudBenchSit::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBenchSit>() : nullptr;
}

void ULyudBenchSit::Deinitialize()
{
	Release();
	SetPrompt(0);
	Super::Deinitialize();
}

void ULyudBenchSit::ReadSeats()
{
	bSeatsRead = true;
	Seats.Reset();
	const ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(this);
	if (!Spots || Spots->NumSpots() == 0)
	{
		bSeatsRead = false;  // not read yet (the outdoor map starts it): asked again later
		return;
	}
	for (const FLyudPublicSpot& S : Spots->GetSpots(TEXT("bench")))
	{
		for (int32 I = 0; I < S.Seats.Num(); ++I)
		{
			if (S.SeatKinds.IsValidIndex(I) && S.SeatKinds[I] != TEXT("bench"))
			{
				continue;
			}
			FSeat Seat;
			Seat.Spot = S.Id;
			Seat.Index = I;
			Seat.At = S.Seats[I].GetLocation();
			Seat.Yaw = S.Seats[I].Rotator().Yaw;
			Seat.Height = S.SeatHeights.IsValidIndex(I) ? FMath::Clamp(S.SeatHeights[I], 25.f, 80.f) : 45.f;
			Seats.Add(Seat);
		}
	}
	UE_LOG(LogLyudBench, Display, TEXT("Benches: %d seats the hero can sit on"), Seats.Num());
}

int32 ULyudBenchSit::FindSeat(const AHeroCharacter* Hero, const FVector& ViewDir, float MaxCm, bool bInFront) const
{
	const ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(this);
	if (!Hero || !Spots)
	{
		return -1;
	}
	const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight());
	int32 Best = -1;
	float BestD = MaxCm;
	for (int32 I = 0; I < Seats.Num(); ++I)
	{
		const FSeat& S = Seats[I];
		const FVector Floor = S.At - FVector(0.f, 0.f, S.Height);
		const FVector2D D2(S.At.X - Feet.X, S.At.Y - Feet.Y);
		const float D = (float)D2.Size();
		if (D >= BestD || FMath::Abs(Floor.Z - Feet.Z) > LyudBenchImpl::ReachUpDown)
		{
			continue;
		}
		if (bInFront && D > 60.f && FVector2D::DotProduct(D2 / FMath::Max(D, 1.f), FVector2D(ViewDir.X, ViewDir.Y).GetSafeNormal()) < 0.25f)
		{
			continue;
		}
		if (Spots->FreePlaces(S.Spot) <= 0)
		{
			continue;
		}
		Best = I;
		BestD = D;
	}
	return Best;
}

bool ULyudBenchSit::SitOn(int32 Seat, AHeroCharacter* Hero)
{
	UWorld* World = GetWorld();
	ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(this);
	if (!World || !Hero || !Spots || !Seats.IsValidIndex(Seat) || Hero->IsSeated())
	{
		return false;
	}
	// the spot gives him one of its free places: the seat of that place on the same bench
	const FName Spot = Seats[Seat].Spot;
	const int32 Place = Spots->Reserve(Spot, Hero);
	if (Place < 0)
	{
		return false;
	}
	int32 Use = Seat;
	for (int32 I = 0; I < Seats.Num(); ++I)
	{
		if (Seats[I].Spot == Spot && Seats[I].Index == Place)
		{
			Use = I;
			break;
		}
	}
	const FSeat& S = Seats[Use];
	if (!IsValid(Anchor) || !IsValid(AnchorRoot))
	{
		// one anchor at the world origin for every bench: the seat's frame is given to SitOn in world terms
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("LyudBenchSeat"));
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Anchor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		if (!Anchor)
		{
			Spots->Release(Spot, Hero);
			return false;
		}
		AnchorRoot = NewObject<USceneComponent>(Anchor, TEXT("Seat"));
		AnchorRoot->SetMobility(EComponentMobility::Static);
		Anchor->SetRootComponent(AnchorRoot);
		AnchorRoot->RegisterComponent();
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	const FRotator Facing(0.f, S.Yaw, 0.f);
	// the seat's frame: the ground under the cushion's front edge, X the way the sitter faces
	const FVector Front = FVector(S.At.X, S.At.Y, S.At.Z - S.Height) + Facing.Vector() * 18.f;
	Hero->SitOn(AnchorRoot, FTransform(Facing, Front), S.Height);
	Hero->SetStandUpOffset(FVector(45.f, 0.f, 0.f));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-6.f, S.Yaw, 0.f));
	}
	SpotId = Spot;
	SeatIndex = Place;
	SeatedHero = Hero;
	UE_LOG(LogLyudBench, Display, TEXT("Benches: sits on %s seat %d (cushion %.0f cm)"), *Spot.ToString(), Place, S.Height);
	return true;
}

void ULyudBenchSit::Release()
{
	if (SpotId.IsNone())
	{
		return;
	}
	if (ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(this))
	{
		Spots->Release(SpotId, SeatedHero.Get());
	}
	UE_LOG(LogLyudBench, Display, TEXT("Benches: up from %s"), *SpotId.ToString());
	SpotId = NAME_None;
	SeatIndex = -1;
	SeatedHero.Reset();
}

void ULyudBenchSit::SetPrompt(int32 What)
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
	if (What == 1)
	{
		UI->SetPrompt(this, LyudText::Get(TEXT("ui.bench.sit")));
	}
	else if (What == 2)
	{
		UI->SetPrompt(this, LyudText::Get(TEXT("ui.bench.read")));
	}
	else
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
}

bool ULyudBenchSit::SitNearest(float MaxCm)
{
	if (!bSeatsRead)
	{
		ReadSeats();
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const int32 Seat = FindSeat(Hero, FVector::ForwardVector, MaxCm, false);
	return Seat >= 0 && SitOn(Seat, Hero);
}

void ULyudBenchSit::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	// up from the bench (a step, Space, a load, the pawn gone): the place goes back to the town
	if (!SpotId.IsNone() && (!SeatedHero.IsValid() || SeatedHero.Get() != Hero || !Hero->IsSeated()))
	{
		Release();
	}
	if (!Hero || Hero->IsHidden() || Hero->IsOnBike() || UGameplayStatics::IsGamePaused(World))
	{
		SetPrompt(0);
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudCityThings* Things = ULyudCityThings::Get(World);
	const ULyudBookScreen* BookScreen = ULyudBookScreen::Get(World);
	const bool bBusy = (UI && UI->IsOpen()) || (Things && (Things->HasThingInView() || Things->HasModeUp())) || (BookScreen && BookScreen->IsOpen());
	if (bBusy)
	{
		SetPrompt(0);
		return;
	}
	if (!SpotId.IsNone())
	{
		// seated on our bench: a book, if he has one on loan
		const ULyudBooks* Books = ULyudBooks::Get(World);
		const bool bBook = Books && Books->Borrowed().Num() > 0;
		SetPrompt(bBook ? 2 : 0);
		if (bBook && ANpcTalkDirector::UsePressed(PC))
		{
			SetPrompt(0);
			if (ULyudBookScreen* Screen = ULyudBookScreen::Get(World))
			{
				Screen->OpenReader(FString());
			}
		}
		return;
	}
	if (Hero->IsSeated())
	{
		SetPrompt(0);
		return;
	}
	if (!bSeatsRead)
	{
		ReadSeats();
		if (!bSeatsRead)
		{
			return;
		}
	}
	const double Now = World->GetTimeSeconds();
	if (Now >= NextSearch)
	{
		NextSearch = Now + 0.2;
		Offered = FindSeat(Hero, PC->GetControlRotation().Vector(), LyudBenchImpl::Reach, true);
	}
	SetPrompt(Offered >= 0 ? 1 : 0);
	if (Offered >= 0 && ANpcTalkDirector::UsePressed(PC))
	{
		SetPrompt(0);
		SitOn(Offered, Hero);
		Offered = -1;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBenchSit(TEXT("lyud.Bench.Sit"),
	TEXT("lyud.Bench.Sit [metres]: sits the hero on the nearest free public bench seat (default within 30 m)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudBenchSit* B = ULyudBenchSit::Get(World))
		{
			const float M = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.f;
			UE_LOG(LogLyudBench, Display, TEXT("Benches: sit nearest within %.0f m: %d"), M, B->SitNearest(M * 100.f) ? 1 : 0);
		}
	}));
