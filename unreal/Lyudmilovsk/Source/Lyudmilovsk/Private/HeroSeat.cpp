#include "HeroSeat.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"

AHeroSeat::AHeroSeat()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(110.f, 110.f, 100.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Trigger->SetupAttachment(Root);
}

void AHeroSeat::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			InRangePC = PC;
		}
	}
}

void AHeroSeat::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (Pawn->GetController() == InRangePC.Get())
		{
			InRangePC.Reset();
		}
	}
}

void AHeroSeat::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		// A key event (not polling) so a short tap is never missed, even at a low frame rate.
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AHeroSeat::OnInteract);
		}
	}
}

void AHeroSeat::OnInteract()
{
	if (bInRange && !bSitting)
	{
		StartSit(GetWorld()->GetFirstPlayerController());
	}
}

void AHeroSeat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Polling the trigger box is robust against teleports and missed overlap events.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	bInRange = Pawn && Trigger->Bounds.GetBox().IsInside(Pawn->GetActorLocation());
	if (bInRange && !bSitting && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.f, FColor::White, Prompt, true, FVector2D(2.f, 2.f));
	}
}

void AHeroSeat::StartSit(APlayerController* PC)
{
	ULevelSequence* Sequence = SitSequence.LoadSynchronous();
	if (!Sequence || bSitting)
	{
		return;
	}
	bSitting = true;

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = true;
	Settings.bDisableLookAtInput = true;
	Settings.bHidePlayer = true;
	Settings.bHideHud = true;
	Settings.bPauseAtEnd = true;

	SequenceActor = GetWorld()->SpawnActorDeferred<ALevelSequenceActor>(ALevelSequenceActor::StaticClass(), FTransform::Identity);
	SequenceActor->PlaybackSettings = Settings;
	SequenceActor->SetSequence(Sequence);
	SequenceActor->bOverrideInstanceData = true;
	if (UDefaultLevelSequenceInstanceData* Data = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
	{
		Data->TransformOriginActor = this;
	}
	SequenceActor->FinishSpawning(FTransform::Identity);
	if (ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer())
	{
		Player->Play();
	}
}
