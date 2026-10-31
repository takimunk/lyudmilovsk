#include "Sea/LyudSwim.h"

#include "Audio/LyudSoundscape.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Sea/LyudSea.h"
#include "Sound/SoundBase.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Winter/LyudWinter.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSwim(TEXT("lyud.Swim"), 1, TEXT("Diving under the sea (0: the old swimming, head always out)."));
	TAutoConsoleVariable<float> CVarSwimBreath(TEXT("lyud.Swim.Breath"), -1.f, TEXT("Seconds of air under water (negative: the component's, 40)."));
	TAutoConsoleVariable<int32> CVarSwimDebug(TEXT("lyud.Swim.Debug"), 0, TEXT("Prints the swimmer's state on screen."));

	/** The line he says when he finds he can breathe (localization/voice.json, strings_<lang>.json). */
	const TCHAR* RealiseLine = TEXT("line.hero.swim_breath.1");
	/** Seconds from the cough to the line, and how long the meter takes to go. */
	constexpr float LineAt = 1.7f;
	constexpr float MeterGoesSeconds = 2.6f;

	ULyudSwimComponent* SwimOf(UWorld* World)
	{
		return ULyudSwimComponent::Get(World);
	}

	FAutoConsoleCommandWithWorld CmdSwimReset(TEXT("lyud.Swim.Reset"), TEXT("Forgets the near-drownings: the air meter is back."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudBreath* B = ULyudBreath::Get(World))
			{
				B->NearDrowns = 0;
				B->bBreathFree = false;
			}
		}));
	FAutoConsoleCommandWithWorld CmdSwimFree(TEXT("lyud.Swim.Free"), TEXT("The breath is free at once (no meter from now on)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudBreath* B = ULyudBreath::Get(World))
			{
				B->bBreathFree = true;
			}
		}));
	FAutoConsoleCommandWithWorld CmdSwimDive(TEXT("lyud.Swim.Dive"), TEXT("Dives from the surface."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudSwimComponent* S = SwimOf(World))
			{
				S->Dive();
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdSwimAir(TEXT("lyud.Swim.Air"), TEXT("lyud.Swim.Air <0..1>: sets the air left."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudSwimComponent* S = SwimOf(World); S && Args.Num() > 0)
			{
				S->SetAir(FCString::Atof(*Args[0]));
			}
		}));
}

// ------------------------------------------------------------------ the HUD: the air meter and the line's subtitle

/** The air meter (a thin bar at the bottom of the screen that empties, then blinks red) and the subtitle of the line. */
class SLyudAirMeter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudAirMeter) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudSwimComponent>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		Owner = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		TWeakObjectPtr<ULyudSwimComponent> W = Owner;
		static const FSlateRoundedBoxBrush Track(FLinearColor(0.02f, 0.03f, 0.035f, 0.55f), 5.f, FLinearColor(0.55f, 0.72f, 0.74f, 0.45f), 1.f);
		static const FSlateRoundedBoxBrush Fill(FLinearColor::White, 3.f);
		const float Width = 280.f;
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 132))
			[
				SNew(SBox).WidthOverride(760.f)
				[
					SNew(STextBlock).Font(Font(18)).Justification(ETextJustify::Center).AutoWrapText(true)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
					.Text_Lambda([W]() { return W.IsValid() && !W->GetSubtitleKey().IsEmpty() ? LyudText::Get(W->GetSubtitleKey()) : FText::GetEmpty(); })
					.ColorAndOpacity_Lambda([W]() { return FSlateColor(Bright.CopyWithNewOpacity(W.IsValid() ? W->GetSubtitleAlpha() : 0.f)); })
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 86))
			[
				SNew(SBorder).BorderImage(&Track).Padding(FMargin(3.f))
				.ColorAndOpacity_Lambda([W]() { return FLinearColor(1.f, 1.f, 1.f, W.IsValid() ? W->GetMeterAlpha() : 0.f); })
				.BorderBackgroundColor_Lambda([W]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, W.IsValid() ? W->GetMeterAlpha() : 0.f)); })
				[
					SNew(SBox).WidthOverride(Width).HeightOverride(8.f).HAlign(HAlign_Left)
					[
						SNew(SBox).HeightOverride(8.f)
						.WidthOverride_Lambda([W, Width]() { return FOptionalSize(W.IsValid() ? FMath::Max(2.f, Width * W->GetAir()) : Width); })
						[
							SNew(SBorder).BorderImage(&Fill)
							.BorderBackgroundColor_Lambda([W]() -> FSlateColor
							{
								if (!W.IsValid())
								{
									return FLinearColor::Transparent;
								}
								const float A = W->GetMeterAlpha();
								if (W->IsMeterRed())
								{
									// empty: a red that blinks, the whole track lit by it
									const float Blink = 0.5f + 0.5f * FMath::Sin(float(FPlatformTime::Seconds()) * 2.f * PI * 2.6f);
									return FLinearColor(0.95f, 0.12f, 0.08f, A * (0.35f + 0.65f * Blink));
								}
								// pale sea-green, warming to amber as it runs low
								const float Low = 1.f - FMath::SmoothStep(0.08f, 0.35f, W->GetAir());
								return FMath::Lerp(FLinearColor(0.72f, 0.93f, 0.92f, A), FLinearColor(1.f, 0.6f, 0.26f, A), Low);
							})
						]
					]
				]
			]
		];
	}

	TWeakObjectPtr<ULyudSwimComponent> Owner;
};

// ------------------------------------------------------------------ the breath across the game

ULyudBreath* ULyudBreath::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudBreath>() : nullptr;
}

void ULyudBreath::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Breath"), this);
	}
}

void ULyudBreath::Deinitialize()
{
	if (ULyudSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULyudSaveSubsystem>() : nullptr)
	{
		Saves->UnregisterSaveObject(TEXT("Breath"));
	}
	Super::Deinitialize();
}

void ULyudBreath::Reset()
{
	NearDrowns = 0;
	bBreathFree = false;
}

// ------------------------------------------------------------------ the swimmer

ULyudSwimComponent::ULyudSwimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// after the movement and the sea (which puts the body in its water and lifts the head)
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

ULyudSwimComponent* ULyudSwimComponent::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APawn* P = PC->GetPawn())
		{
			if (ULyudSwimComponent* S = P->FindComponentByClass<ULyudSwimComponent>())
			{
				return S;
			}
		}
	}
	// a bird is being flown: the hero waits, hidden
	for (TActorIterator<AHeroCharacter> It(World); It; ++It)
	{
		if (ULyudSwimComponent* S = It->FindComponentByClass<ULyudSwimComponent>())
		{
			return S;
		}
	}
	return nullptr;
}

bool ULyudSwimComponent::IsBreathFree() const
{
	const ULyudBreath* B = ULyudBreath::Get(this);
	return B && B->bBreathFree;
}

void ULyudSwimComponent::BeginPlay()
{
	Super::BeginPlay();
	// every level start is a new game unless a load restores the breath right after (as the pocket)
	if (ULyudBreath* B = ULyudBreath::Get(this); B && B->ResetFor.Get() != GetWorld())
	{
		B->Reset();
		B->ResetFor = GetWorld();
	}
}

void ULyudSwimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ShowHud(false);
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Sea->SetSwimmerDiving(false);
	}
	Super::EndPlay(Reason);
}

void ULyudSwimComponent::ShowHud(bool bShow)
{
	if (bShow == Hud.IsValid())
	{
		return;
	}
	if (!GEngine || !GEngine->GameViewport)
	{
		Hud.Reset();
		return;
	}
	if (bShow)
	{
		Hud = SNew(SLyudAirMeter).Owner(this);
		// under the prompts (60) and the game screen (85)
		GEngine->GameViewport->AddViewportWidgetContent(Hud.ToSharedRef(), 58);
	}
	else
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		Hud.Reset();
	}
}

void ULyudSwimComponent::Sound(FName Pool, float Volume)
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, Eye, Volume);
	}
}

float ULyudSwimComponent::IceUndersideZ(const FVector& At) const
{
	const ULyudWinterSubsystem* Winter = ULyudWinterSubsystem::Get(this);
	const ALyudSea* Sea = ALyudSea::Get(this);
	if (!Winter || !Sea || Winter->GetIceAt(At) < 0.5f)
	{
		return 1e9f;
	}
	// the sheet floats with a tenth of it out of the water
	return Sea->SeaLevel() - 0.9f * FMath::Max(Winter->GetNow().IceCm, 3.f);
}

void ULyudSwimComponent::SetState(ELyudSwimState New)
{
	if (New == State)
	{
		return;
	}
	const ELyudSwimState Old = State;
	State = New;
	StateTime = 0.f;
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Sea->SetSwimmerDiving(New == ELyudSwimState::Under);
	}
	if (New == ELyudSwimState::Under)
	{
		if (!UnderLoop)
		{
			if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
			{
				UnderLoop = S->AttachLoop(TEXT("swim_underwater"), GetOwner()->GetRootComponent(), 0.7f);
				if (UnderLoop)
				{
					UnderLoop->FadeIn(0.5f, 0.7f);
				}
			}
		}
		NextBubbles = FMath::FRandRange(2.f, 4.f);
	}
	else if (UnderLoop)
	{
		UnderLoop->FadeOut(0.4f, 0.f);
		UnderLoop = nullptr;
	}
	if (Old == ELyudSwimState::Under && New == ELyudSwimState::Surface)
	{
		Sound(TEXT("swim_surface"), 0.8f);
	}
	OnStateChanged.Broadcast(Old, New);
}

bool ULyudSwimComponent::Dive()
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (State != ELyudSwimState::Surface || !C || CVarSwim.GetValueOnGameThread() == 0)
	{
		return false;
	}
	SetState(ELyudSwimState::Under);
	// a duck dive: head first, a push down
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	Move->Velocity.Z = FMath::Min(Move->Velocity.Z, -170.f);
	if (!IsBreathFree())
	{
		Sound(TEXT("swim_inhale"), 0.7f);
	}
	Sound(TEXT("swim_dive"), 0.8f);
	return true;
}

void ULyudSwimComponent::SetAir(float InAir)
{
	Air = FMath::Clamp(InAir, 0.f, 1.f);
}

bool ULyudSwimComponent::SwimMove(const FVector2D& Input)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C || State == ELyudSwimState::Dry)
	{
		return false;
	}
	MoveInput = Input;
	MoveInputAge = 0.f;
	const FRotator Look = C->GetControlRotation();
	// swimming on with the eyes on the bottom dives
	if (State == ELyudSwimState::Surface && Input.Y > 0.5f && FRotator::NormalizeAxis(Look.Pitch) < -DiveLookDeg)
	{
		Dive();
	}
	if (State != ELyudSwimState::Under)
	{
		return false;
	}
	// under water the stroke goes where the eyes look
	const FRotationMatrix M(FRotator(Look.Pitch, Look.Yaw, 0.f));
	C->AddMovementInput(M.GetUnitAxis(EAxis::X), Input.Y);
	C->AddMovementInput(M.GetUnitAxis(EAxis::Y), Input.X);
	return true;
}

void ULyudSwimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* C = Cast<ACharacter>(GetOwner());
	ALyudSea* Sea = ALyudSea::Get(this);
	const APlayerController* PC = C ? Cast<APlayerController>(C->GetController()) : nullptr;
	StateTime += DeltaTime;
	MoveInputAge += DeltaTime;
	if (!C || !Sea || !PC || C->IsHidden() || CVarSwim.GetValueOnGameThread() == 0)
	{
		SetState(ELyudSwimState::Dry);
		bChoking = false;
		BodySwim = FMath::FInterpConstantTo(BodySwim, 0.f, DeltaTime, 2.f);
		MeterAlpha = 0.f;
		ShowHud(false);
		return;
	}
	ShowHud(true);
	UpdateWater(*C, *Sea);
	UpdateUnder(*C, *Sea, DeltaTime);
	UpdateAir(*C, DeltaTime);
	UpdateRealise(*C, DeltaTime);
	UpdateLook(DeltaTime);
	if (CVarSwimDebug.GetValueOnGameThread() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x5713), 0.f, FColor::Cyan, DebugReport());
	}
}

void ULyudSwimComponent::UpdateWater(ACharacter& C, ALyudSea& Sea)
{
	UCharacterMovementComponent* Move = C.GetCharacterMovement();
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(&C);
	Eye = Hero ? Hero->EyeLocation() : C.GetPawnViewLocation();
	const FLyudSeaSample S = Sea.SampleSurface(Eye);
	const float IceZ = IceUndersideZ(Eye);
	bUnderIce = IceZ < S.SurfaceZ;
	const float TopZ = bUnderIce ? IceZ : S.SurfaceZ;
	BottomBelow = S.bIsSea ? FMath::Max(0.f, Eye.Z - (Sea.SeaLevel() - S.Depth)) : 0.f;

	// the sea holds a body in deep water in its volume (MOVE_Swimming); out of it, the swimming is over
	if (!Move->IsSwimming())
	{
		const bool bFalling = Move->IsFalling();
		FallSpeed = bFalling ? FMath::Max(0.f, -float(Move->Velocity.Z)) : 0.f;
		bWasFalling = bFalling;
		SetState(ELyudSwimState::Dry);
		EyeDepth = 0.f;
		return;
	}
	if (State == ELyudSwimState::Dry)
	{
		// a jump or a fall from a height goes under by itself; walking in, you swim at the surface
		const bool bPlunge = bWasFalling && FallSpeed > 520.f;
		SetState(bPlunge ? ELyudSwimState::Under : ELyudSwimState::Surface);
		Sound(bPlunge ? TEXT("swim_dive") : TEXT("swim_stroke"), bPlunge ? 1.f : 0.6f);
		bWasFalling = false;
	}
	if (bUnderIce && State == ELyudSwimState::Surface)
	{
		// the ice closes over: no head out under it
		SetState(ELyudSwimState::Under);
	}
	if (State == ELyudSwimState::Under)
	{
		EyeDepth = FMath::Max(0.f, TopZ - Eye.Z);
		const bool bRising = Move->Velocity.Z > -10.f;
		if (!bUnderIce && StateTime > 0.4f && (EyeDepth < SurfaceWithinCm && bRising || Eye.Z > S.SurfaceZ + 5.f))
		{
			SetState(ELyudSwimState::Surface);
		}
	}
	if (State != ELyudSwimState::Under)
	{
		EyeDepth = 0.f;
	}
}

void ULyudSwimComponent::UpdateUnder(ACharacter& C, ALyudSea& Sea, float DeltaTime)
{
	UCharacterMovementComponent* Move = C.GetCharacterMovement();
	const APlayerController* PC = Cast<APlayerController>(C.GetController());
	const bool bInput = PC && !PC->IsMoveInputIgnored();
	const bool bUp = bInput && PC->IsInputKeyDown(EKeys::SpaceBar);
	const bool bDown = bInput && (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl) || PC->IsInputKeyDown(EKeys::C));
	const bool bSlow = bInput && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	// choking, the body fights upward; the time he realises it stops fighting and hangs, spasms slow it right down
	const bool bPanic = bChoking && !bRealising && !bLastStraw;
	const bool bSpasm = bChoking && bLastStraw;

	if (State == ELyudSwimState::Dry)
	{
		Move->Buoyancy = 1.06f;
		Move->MaxSwimSpeed = SurfaceSpeed;
		return;
	}
	if (State == ELyudSwimState::Surface)
	{
		Move->Buoyancy = 1.06f;
		Move->MaxSwimSpeed = SurfaceSpeed;
		if (bInput && (PC->WasInputKeyJustPressed(EKeys::C) || PC->WasInputKeyJustPressed(EKeys::LeftControl) || PC->WasInputKeyJustPressed(EKeys::RightControl)))
		{
			Dive();
		}
		// strokes splash now and then while swimming
		SinceStroke += DeltaTime;
		if (MoveInputAge < 0.2f && MoveInput.Size() > 0.3f && SinceStroke > 1.25f)
		{
			SinceStroke = FMath::FRandRange(-0.2f, 0.15f);
			Sound(TEXT("swim_stroke"), 0.45f);
		}
		return;
	}

	// under: nearly neutral, drifting up a little; choking, the body fights upward
	Move->Buoyancy = bPanic ? 1.5f : bSpasm ? 0.99f : 1.015f;
	Move->MaxSwimSpeed = (bSlow ? DiveSlowSpeed : DiveSpeed) * (bPanic ? 0.55f : bSpasm ? 0.2f : 1.f);
	if (bUp || bPanic)
	{
		C.AddMovementInput(FVector::UpVector, 1.f);
	}
	else if (bDown)
	{
		C.AddMovementInput(-FVector::UpVector, 1.f);
	}
	// under the ice its underside is the ceiling (the head never goes through it)
	if (bUnderIce)
	{
		const float IceZ = IceUndersideZ(Eye);
		const float Over = Eye.Z - (IceZ - 10.f);
		if (Over > 0.f)
		{
			C.AddActorWorldOffset(FVector(0.0, 0.0, -Over), false, nullptr, ETeleportType::TeleportPhysics);
			Move->Velocity.Z = FMath::Min(Move->Velocity.Z, 0.0);
		}
	}
}

void ULyudSwimComponent::UpdateAir(ACharacter& C, float DeltaTime)
{
	const bool bFree = IsBreathFree();
	const bool bUnder = State == ELyudSwimState::Under;
	if (bUnder)
	{
		TimeUnder += DeltaTime;
		NextBubbles -= DeltaTime;
		if (NextBubbles <= 0.f && !bChoking)
		{
			NextBubbles = FMath::FRandRange(3.f, 6.5f);
			Sound(TEXT("swim_bubbles"), 0.45f);
		}
	}
	if (bFree && !bRealising)
	{
		Air = 1.f;
		bChoking = false;
		MeterAlpha = FMath::FInterpConstantTo(MeterAlpha, 0.f, DeltaTime, 1.f);
		return;
	}
	if (bRealising)
	{
		return;
	}
	if (bUnder)
	{
		const float Seconds = CVarSwimBreath.GetValueOnGameThread() > 0.f ? CVarSwimBreath.GetValueOnGameThread() : BreathSeconds;
		// swimming hard burns it faster
		const float Effort = MoveInputAge < 0.2f ? 1.f + 0.25f * FMath::Clamp(MoveInput.Size(), 0.f, 1.f) : 1.f;
		Air = FMath::Max(0.f, Air - DeltaTime * Effort / FMath::Max(Seconds, 1.f));
		if (Air <= 0.f && !bChoking)
		{
			bChoking = true;
			ChokeTime = 0.f;
			NextChoke = 0.f;
		}
		if (bChoking)
		{
			ChokeTime += DeltaTime;
			NextChoke -= DeltaTime;
			if (NextChoke <= 0.f)
			{
				NextChoke = FMath::FRandRange(1.3f, 2.1f);
				Sound(TEXT("swim_choke"), 0.9f);
			}
			const ULyudBreath* B = ULyudBreath::Get(this);
			bLastStraw = B && B->NearDrowns + 1 >= NearDrownsToRealise;
			if ((bLastStraw && ChokeTime > RealiseChokeSeconds) || ChokeTime > RealiseAfterSeconds)
			{
				StartRealise();
			}
		}
	}
	else
	{
		if (bChoking)
		{
			Surfaced(C);
		}
		Air = FMath::Min(1.f, Air + DeltaTime / FMath::Max(RefillSeconds, 0.5f));
		TimeUnder = 0.f;
	}
	// the meter: up while under or not yet full, gone a moment after it is full again
	FullFor = Air >= 0.999f && !bUnder ? FullFor + DeltaTime : 0.f;
	const bool bShow = bUnder || Air < 0.999f || (FullFor < 1.5f && MeterAlpha > 0.f);
	MeterAlpha = FMath::FInterpConstantTo(MeterAlpha, bShow ? 1.f : 0.f, DeltaTime, bShow ? 4.f : 1.5f);
}

void ULyudSwimComponent::Surfaced(ACharacter& C)
{
	// a near-drowning: he comes up gasping and coughing up the water
	bChoking = false;
	bLastStraw = false;
	ChokeTime = 0.f;
	if (ULyudBreath* B = ULyudBreath::Get(this))
	{
		B->NearDrowns++;
	}
	Sound(TEXT("swim_gasp"), 1.f);
	if (UWorld* World = GetWorld())
	{
		FTimerHandle T;
		TWeakObjectPtr<ULyudSwimComponent> Weak(this);
		World->GetTimerManager().SetTimer(T, FTimerDelegate::CreateLambda([Weak]() { if (Weak.IsValid()) { Weak->Sound(TEXT("swim_cough"), 0.9f); } }), 1.1f, false);
	}
}

void ULyudSwimComponent::StartRealise()
{
	// he stops fighting, coughs ... and notices he is breathing
	bRealising = true;
	RealiseTime = 0.f;
	bLineStarted = false;
	bChoking = false;
	bLastStraw = false;
	Sound(TEXT("swim_cough"), 1.f);
}

void ULyudSwimComponent::UpdateRealise(ACharacter& C, float DeltaTime)
{
	if (!SubtitleKey.IsEmpty())
	{
		SubtitleFor -= DeltaTime;
		SubtitleAlpha = FMath::FInterpConstantTo(SubtitleAlpha, SubtitleFor > 0.f ? 1.f : 0.f, DeltaTime, 3.f);
		if (SubtitleFor <= 0.f && SubtitleAlpha <= 0.f)
		{
			SubtitleKey.Reset();
		}
	}
	if (!bRealising)
	{
		return;
	}
	RealiseTime += DeltaTime;
	if (!bLineStarted && RealiseTime >= LineAt)
	{
		bLineStarted = true;
		if (ULyudBreath* B = ULyudBreath::Get(this))
		{
			B->NearDrowns++;
			B->bBreathFree = true;
		}
		USoundBase* Voice = LyudText::LoadVoice(RealiseLine);
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this); S && Voice)
		{
			S->PlayVoiceAt(Voice, Eye, 1.f);
		}
		SubtitleKey = RealiseLine;
		SubtitleFor = Voice ? FMath::Max(3.f, Voice->GetDuration() + 0.8f) : 5.5f;
		OnBreathFree.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("Swim: the breath is free (near-drownings %d)"), ULyudBreath::Get(this) ? ULyudBreath::Get(this)->NearDrowns : -1);
	}
	if (bLineStarted)
	{
		// he breathes: the bar fills, then goes for good
		const float T = RealiseTime - LineAt;
		Air = FMath::Min(1.f, Air + DeltaTime / 1.4f);
		MeterAlpha = 1.f - FMath::SmoothStep(1.2f, 1.2f + MeterGoesSeconds, T);
		if (T > 1.2f + MeterGoesSeconds)
		{
			bRealising = false;
			MeterAlpha = 0.f;
		}
	}
	else
	{
		MeterAlpha = 1.f;
	}
}

void ULyudSwimComponent::UpdateLook(float DeltaTime)
{
	// the body: lying along the look under water, flat at the surface
	const ACharacter* C = Cast<ACharacter>(GetOwner());
	const bool bSwim = State != ELyudSwimState::Dry;
	BodySwim = FMath::FInterpConstantTo(BodySwim, bSwim ? 1.f : 0.f, DeltaTime, 2.5f);
	float WantPitch = 0.f;
	if (State == ELyudSwimState::Under && C)
	{
		const FVector V = C->GetVelocity();
		const float Speed = float(V.Size());
		const float LookPitch = FRotator::NormalizeAxis(C->GetControlRotation().Pitch);
		// moving: along the way it goes; still: a slow hang, head a little up
		WantPitch = Speed > 40.f ? FMath::RadiansToDegrees(FMath::Atan2(float(V.Z), float(V.Size2D()))) : FMath::Clamp(LookPitch * 0.5f, -40.f, 20.f);
		WantPitch = FMath::Clamp(WantPitch, -75.f, 70.f);
	}
	BodyPitch = FMath::FInterpTo(BodyPitch, WantPitch, DeltaTime, 3.f);
}

FString ULyudSwimComponent::DebugReport() const
{
	const ULyudBreath* B = ULyudBreath::Get(this);
	static const TCHAR* Names[] = { TEXT("dry"), TEXT("surface"), TEXT("under") };
	return FString::Printf(TEXT("Swim: %s %.1fs  eye %.0f cm under, bottom %.0f cm below%s  air %.2f%s%s  near-drownings %d%s  meter %.2f  pitch %.0f"),
		Names[int32(State)], StateTime, EyeDepth, BottomBelow, bUnderIce ? TEXT(", under ice") : TEXT(""), Air,
		bChoking ? TEXT(" CHOKING") : TEXT(""), bRealising ? TEXT(" REALISING") : TEXT(""), B ? B->NearDrowns : -1,
		B && B->bBreathFree ? TEXT(" breath free") : TEXT(""), MeterAlpha, BodyPitch);
}
