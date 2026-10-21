#include "AtonComputer.h"

#include "AtonOS/AtonApps.h"
#include "AtonOS/SAtonDesktop.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HeroSeat.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

/** Screen centre in the seat's space: the screen is ~91 cm ahead of the chair and 114 cm up, the widget 3 mm in front of it. */
static const FVector ScreenFromSeat(91.1, 0.0, 114.0);

UAtonScreenComponent::UAtonScreenComponent()
{
	// The real mouse cursor and keyboard reach the widget through the game viewport.
	bReceiveHardwareInput = true;
	bWindowFocusable = true;
	SetWidgetSpace(EWidgetSpace::World);
	SetPivot(FVector2D(0.5f, 0.5f));
	SetBlendMode(EWidgetBlendMode::Opaque);
	SetTwoSided(false);
	SetCastShadow(false);
	SetTickWhenOffscreen(false);
}

AAtonComputer::AAtonComputer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// The widget plane faces its +X, the same way as this actor.
	Screen = CreateDefaultSubobject<UAtonScreenComponent>(TEXT("Screen"));
	Screen->SetupAttachment(RootComponent);
	ApplyScreenSize();
}

void AAtonComputer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyScreenSize();
}

void AAtonComputer::ApplyScreenSize()
{
	const FIntPoint Res(FMath::Max(Resolution.X, 64), FMath::Max(Resolution.Y, 64));
	Screen->SetDrawSize(FVector2D(Res));
	Screen->SetRelativeScale3D(FVector(ScreenWidth / Res.X));
	Screen->SetTintColorAndOpacity(FLinearColor(Brightness, Brightness, Brightness, 1.f));
}

void AAtonComputer::BeginPlay()
{
	Super::BeginPlay();
	ApplyScreenSize();

	if (!Seat)
	{
		Seat = TActorIterator<AHeroSeat>(GetWorld()) ? *TActorIterator<AHeroSeat>(GetWorld()) : nullptr;
	}

	AtonApps::ChosenLocalURL = ChosenLocalURL;
	AtonApps::ChosenURL = ChosenURL;

	TWeakObjectPtr<AAtonComputer> Self(this);
	SAssignNew(Desktop, SAtonDesktop)
		.ScreenSize(FVector2D(Resolution))
		.CanLeave_Lambda([Self] { return Self.IsValid() && Self->OnLeave.IsBound(); })
		.OnLeave_Lambda([Self]
		{
			if (AAtonComputer* Me = Self.Get())
			{
				APlayerController* PC = Me->ActivePC.Get();
				Me->Deactivate();
				Me->OnLeave.Broadcast(PC);
			}
		});
	Screen->SetSlateWidget(Desktop);
}

void AAtonComputer::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bActive)
	{
		Deactivate();
	}
	Screen->SetSlateWidget(nullptr);
	Desktop.Reset();
	Super::EndPlay(Reason);
}

void AAtonComputer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bActive || WatchedPlayer.IsValid() || !Seat)
	{
		return;
	}
	// Hand-off: the seat spawns a sequence actor for its sit-down cutscene; when that finishes, the OS takes over.
	const ULevelSequence* Sit = Seat->SitSequence.Get();
	if (!Sit)
	{
		return;
	}
	for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
	{
		ULevelSequencePlayer* Player = It->GetSequencePlayer();
		if (Player && It->GetSequence() == Sit)
		{
			Player->OnFinished.AddUniqueDynamic(this, &AAtonComputer::OnSitFinished);
			WatchedPlayer = Player;
			// A loaded save skips the cutscene to its end, which may finish before this tick sees it.
			if (!Player->IsPlaying() && Player->GetCurrentTime().Time.FrameNumber >= Player->GetEndTime().Time.FrameNumber - 1)
			{
				OnSitFinished();
			}
			break;
		}
	}
}

bool AAtonComputer::IsOSMenuOpen() const
{
	return Desktop.IsValid() && Desktop->IsMenuOpen();
}

void AAtonComputer::OnSitFinished()
{
	if (!bActive)
	{
		Activate(GetWorld()->GetFirstPlayerController());
	}
}

void AAtonComputer::Activate(APlayerController* PC)
{
	if (!PC || !Desktop.IsValid())
	{
		return;
	}
	bActive = true;
	ActivePC = PC;
	PC->SetShowMouseCursor(true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Desktop);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	Desktop->FocusFrontWindow();
	OnActivated.Broadcast(PC);
}

void AAtonComputer::Deactivate()
{
	bActive = false;
	if (APlayerController* PC = ActivePC.Get())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
	ActivePC.Reset();
	// A later sit-down binds the sequence again.
	WatchedPlayer.Reset();
}

void UAtonOSSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (TActorIterator<AAtonComputer>(&World))
	{
		return;
	}
	TActorIterator<AHeroSeat> SeatIt(&World);
	if (!SeatIt)
	{
		return;
	}
	AHeroSeat* Seat = *SeatIt;
	const FTransform& SeatTransform = Seat->GetActorTransform();
	const FVector Location = SeatTransform.TransformPosition(ScreenFromSeat);
	// The seat's +X looks at the screen; the computer's +X looks back out of it.
	const FRotator Rotation = (SeatTransform.GetRotation() * FQuat(FVector::UpVector, PI)).Rotator();

	FActorSpawnParameters Params;
	Params.Name = TEXT("AtonComputer");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	if (AAtonComputer* Computer = World.SpawnActor<AAtonComputer>(Location, Rotation, Params))
	{
		Computer->Seat = Seat;
	}
}
