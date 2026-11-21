#include "South/LyudSouthSpot.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "NpcDialogue.h"
#include "UI/LyudGameScreen.h"

namespace
{
	/** Of the spots in reach the nearest has the prompt: the one that asked with the least distance last frame. */
	uint64 GSpotFrame = 0;
	float GSpotBestDist = 0.f;
	TWeakObjectPtr<const ALyudSouthSpot> GSpotBest;
	TWeakObjectPtr<const ALyudSouthSpot> GSpotWinner;

	/** A viewer lets go when the looker has stepped this far beyond its reach, cm. */
	constexpr float ViewerLetGo = 70.f;

	bool ClaimNearest(const ALyudSouthSpot* Spot, float Dist)
	{
		if (GSpotFrame != GFrameCounter)
		{
			// a frame nobody asked in has no winner
			GSpotWinner = GSpotFrame + 1 == GFrameCounter ? GSpotBest : nullptr;
			GSpotBest = nullptr;
			GSpotFrame = GFrameCounter;
		}
		if (!GSpotBest.IsValid() || Dist < GSpotBestDist)
		{
			GSpotBest = Spot;
			GSpotBestDist = Dist;
		}
		return GSpotWinner.Get() == Spot;
	}

	FAutoConsoleCommandWithWorldAndArgs GSouthSitCmd(TEXT("lyud.South.Sit"),
		TEXT("lyud.South.Sit 1|0: the hero sits on the Aton coast's nearest seat (from anywhere) / gets up; logs a SOUTH line"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			AHeroCharacter* H = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
			if (!H)
			{
				return;
			}
			const bool bSit = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			ALyudSouthSpot* Best = nullptr;
			double BestD = 0.0;
			int32 Count = 0;
			for (TActorIterator<ALyudSouthSpot> It(World); It; ++It)
			{
				++Count;
				if (!bSit)
				{
					if (It->IsInUse())
					{
						It->Leave();
					}
					continue;
				}
				const double D = FVector::DistSquared(It->GetActorLocation(), H->GetActorLocation());
				if (It->Kind != TEXT("view") && (!Best || D < BestD))
				{
					Best = *It;
					BestD = D;
				}
			}
			const bool bOk = Best && Best->Use(H);
			UE_LOG(LogTemp, Display, TEXT("SOUTH sit cmd: %d spots, %s, seated=%d"), Count,
				Best ? *Best->GetActorLocation().ToCompactString() : TEXT("no seat"), (bOk && H->IsSeated()) ? 1 : 0);
		}));
}

ALyudSouthSpot::ALyudSouthSpot()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("South"));
}

void ALyudSouthSpot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(0);
	if (bUsing && !IsSeat())
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			Screen->ReleaseViewer(this);
		}
	}
	bUsing = false;
	Super::EndPlay(EndPlayReason);
}

void ALyudSouthSpot::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	// near: every frame (E is polled); else no tick at all
	SetActorTickEnabled(bNear);
	if (!bNear)
	{
		if (bUsing)
		{
			Leave();
		}
		SetPrompt(0);
	}
}

void ALyudSouthSpot::SetPrompt(int32 Mode)
{
	if (Mode == Prompted)
	{
		return;
	}
	Prompted = Mode;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		const TCHAR* Key = IsSeat() ? (Mode == 1 ? TEXT("ui.south.sit") : TEXT("ui.south.stand"))
			: (Mode == 1 ? TEXT("ui.south.view") : TEXT("ui.south.view_stop"));
		UI->SetPrompt(this, Mode == 0 ? FText::GetEmpty() : LyudText::Get(Key));
	}
}

bool ALyudSouthSpot::Use(AHeroCharacter* H)
{
	if (!H || bUsing || H->IsSeated() || H->IsOnBike())
	{
		return false;
	}
	if (IsSeat())
	{
		if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
		{
			Hand->PutAway();
		}
		H->SitOn(RootComponent, FTransform::Identity, SeatHeight);
		if (!H->IsSeated())
		{
			return false;
		}
		if (APlayerController* PC = Cast<APlayerController>(H->GetController()))
		{
			PC->SetControlRotation(FRotator(-5.0, GetActorRotation().Yaw, 0.0));
		}
	}
	else
	{
		ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
		if (!Screen)
		{
			return false;
		}
		Screen->HoldViewer(this);
		if (APlayerController* PC = Cast<APlayerController>(H->GetController()))
		{
			PC->SetControlRotation(FRotator(-3.0, GetActorRotation().Yaw, 0.0));
		}
	}
	User = H;
	bUsing = true;
	SetPrompt(0);
	UE_LOG(LogTemp, Display, TEXT("SOUTH %s: taken at %s"), *Kind.ToString(), *GetActorLocation().ToCompactString());
	return true;
}

void ALyudSouthSpot::Leave()
{
	if (!bUsing)
	{
		return;
	}
	bUsing = false;
	AHeroCharacter* H = User.Get();
	User.Reset();
	if (IsSeat())
	{
		if (H && H->IsSeated())
		{
			H->StandUp();
		}
	}
	else if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->ReleaseViewer(this);
	}
	SetPrompt(0);
	UE_LOG(LogTemp, Display, TEXT("SOUTH %s: left"), *Kind.ToString());
}

void ALyudSouthSpot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHeroCharacter* H = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !H)
	{
		if (bUsing)
		{
			Leave();
		}
		SetPrompt(0);
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bScreen = UI && UI->IsOpen();
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = H->GetActorLocation() - FVector(0.0, 0.0, Half);
	const FVector Here = GetActorLocation();
	const float Dist = float(FVector::Dist2D(Feet, Here));
	if (bUsing)
	{
		const bool bGone = IsSeat() ? !H->IsSeated() : Dist > Reach + ViewerLetGo;
		if (bGone)
		{
			// he got up himself (a step, Space) or walked off from the viewer
			if (IsSeat())
			{
				bUsing = false;
				User.Reset();
				SetPrompt(0);
				UE_LOG(LogTemp, Display, TEXT("SOUTH seat: left (got up himself)"));
			}
			else
			{
				Leave();
			}
			return;
		}
		SetPrompt(bScreen ? 0 : 2);
		if (!bScreen && ANpcTalkDirector::UsePressed(PC, true))
		{
			Leave();
		}
		return;
	}
	const bool bClose = Dist < Reach && FMath::Abs(Feet.Z - Here.Z) < 160.0;
	const bool bMine = bClose && !bScreen && !H->IsSeated() && !H->IsOnBike() && ClaimNearest(this, Dist);
	SetPrompt(bMine ? 1 : 0);
	if (bMine && ANpcTalkDirector::UsePressed(PC))
	{
		Use(H);
	}
}
