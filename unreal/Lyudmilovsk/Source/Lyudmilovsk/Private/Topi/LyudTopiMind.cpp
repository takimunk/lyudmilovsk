#include "Topi/LyudTopiMind.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#include "Audio/LyudSoundscape.h"
#include "Bikes/HeroBike.h"
#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroSeat.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "Topi/LyudTopi.h"
#include "Topi/TopiModel.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TAutoConsoleVariable<int32> CVarTopiCalm(TEXT("lyud.Topi.Calm"), 0, TEXT("1: Topi does not press on the hero at all (no pressure, no turning back; tests)."));

	const TCHAR* TopiMusicPath = TEXT("/Game/Audio/Music/MU_topi_1.MU_topi_1");
	const TCHAR* UralMusicPath = TEXT("/Game/Audio/Music/MU_ural_1.MU_ural_1");
	const TCHAR* TopiWakeLine = TEXT("line.hero.topi.wake.1");
	const FName TopiSaveKey(TEXT("TopiMind"));
	const FName TopiHeartPool(TEXT("topi_heart"));
	const FName TopiHumPool(TEXT("ural_hum"));

	/** The near zone: this many metres before the sand's edge the mind already wakes. */
	constexpr double TopiNearM = 150.0;

	// the turn: seconds to turn round, then to walk out; the grace after it
	constexpr float TopiTurnRotateSeconds = 2.2f;
	constexpr float TopiTurnSeconds = 7.2f;
	constexpr float TopiTurnPinSeconds = 4.f;
	constexpr float TopiTurnGraceSeconds = 20.f;

	// the river: diving, the light, the wake (seconds from the start of the sequence)
	constexpr float TopiDiveSeconds = 3.5f;
	constexpr float TopiLightEndsAt = 8.5f;
	constexpr float TopiWhiteFrom = 5.f;
	constexpr float TopiWhiteHold = 1.2f;
	constexpr float TopiClearSeconds = 4.f;
	constexpr float TopiSequenceEndsAt = TopiLightEndsAt + TopiWhiteHold + TopiClearSeconds + 0.3f;
	constexpr float TopiChestDepthCm = 110.f;
	const FLinearColor TopiWhiteTeal(0.75f, 1.f, 0.95f);

	/** True when he is on his own feet. A rider is asked off (the bike's own dismount takes a moment: false until he
	 *  stands), a led bike is let go, a seated hero stands up. Neither sequence may start on a bike: the bike would keep
	 *  placing him on its saddle every frame. */
	bool TopiOnFoot(AHeroCharacter& Hero)
	{
		if (UHeroBikeComponent* Bike = Hero.FindBike())
		{
			if (Bike->IsOnBike())
			{
				Bike->Dismount();
				return false;
			}
			if (Bike->IsLeading())
			{
				Bike->LetGo();
			}
		}
		if (Hero.IsSeated())
		{
			Hero.StandUp();
		}
		return true;
	}

	ULyudTopiMind* TopiMindOf(UWorld* World)
	{
		ALyudTopi* T = World ? ALyudTopi::Get(World) : nullptr;
		return T ? T->Mind.Get() : nullptr;
	}

	// developer mode (docs/dev_mode.md): the Actions tab's "topi" group and Dev.Do topi.*
	bool TopiDevFlag(const TCHAR* Name)
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		return Var && Var->GetFloat() > 0.5f;
	}
	void TopiDevSet(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByConsole);
		}
	}
	LyudDev::FAutoAction GTopiDevNight(TEXT("topi.night"), TEXT("topi"), TEXT("dev.action.topi.night"),
		[](UWorld*) { TopiDevSet(TEXT("lyud.Topi.Night"), TopiDevFlag(TEXT("lyud.Topi.Night")) ? -1.f : 1.f); },
		[](UWorld*) { return TopiDevFlag(TEXT("lyud.Topi.Night")); });
	LyudDev::FAutoAction GTopiDevCalm(TEXT("topi.calm"), TEXT("topi"), TEXT("dev.action.topi.calm"),
		[](UWorld*) { TopiDevSet(TEXT("lyud.Topi.Calm"), TopiDevFlag(TEXT("lyud.Topi.Calm")) ? 0.f : 1.f); },
		[](UWorld*) { return TopiDevFlag(TEXT("lyud.Topi.Calm")); });
	LyudDev::FAutoAction GTopiDevTurn(TEXT("topi.turn"), TEXT("topi"), TEXT("dev.action.topi.turn"),
		[](UWorld* W) { if (GEngine && W) { GEngine->Exec(W, TEXT("lyud.Topi.TurnBack")); } });
	LyudDev::FAutoAction GTopiDevDissolve(TEXT("topi.dissolve"), TEXT("topi"), TEXT("dev.action.topi.dissolve"),
		[](UWorld* W) { if (GEngine && W) { GEngine->Exec(W, TEXT("lyud.Topi.Dissolve")); } });

	FAutoConsoleCommandWithWorld CmdTopiTurnBack(TEXT("lyud.Topi.TurnBack"), TEXT("The hero turns back from Topi now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudTopiMind* M = TopiMindOf(World))
			{
				M->ForceTurnBack();
			}
		}));
	FAutoConsoleCommandWithWorld CmdTopiDissolve(TEXT("lyud.Topi.Dissolve"), TEXT("Starts the river Ural's sequence (dive, dissolve, wake at the computer) wherever the hero is."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudTopiMind* M = TopiMindOf(World))
			{
				M->StartDissolve();
			}
		}));

	float TopiSmooth01(float X)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f));
	}
}

// ------------------------------------------------------------------ the subtitle of his lines

class SLyudTopiSubtitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudTopiSubtitle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudTopiMind>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		TWeakObjectPtr<ULyudTopiMind> W = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
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
		];
	}
};

// ------------------------------------------------------------------ the component

ULyudTopiMind::ULyudTopiMind()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ALyudTopi* ULyudTopiMind::Topi() const
{
	return Cast<ALyudTopi>(GetOwner());
}

AHeroCharacter* ULyudTopiMind::PlayerHero(APlayerController** OutPC) const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (OutPC)
	{
		*OutPC = PC;
	}
	return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
}

void ULyudTopiMind::BeginPlay()
{
	Super::BeginPlay();
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !World->IsGameWorld())
	{
		return;
	}
	// one unbound post process for everything the place does to his eyes; its overrides are set once, values per tick
	Post = NewObject<UPostProcessComponent>(OwnerActor, TEXT("TopiMindPost"));
	if (Post)
	{
		if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
		{
			Post->SetupAttachment(OwnerRoot);
		}
		Post->bUnbound = true;
		Post->Priority = 1200.f;
		Post->BlendWeight = 0.f;
		FPostProcessSettings& S = Post->Settings;
		S.bOverride_VignetteIntensity = true;
		S.bOverride_SceneFringeIntensity = true;
		S.bOverride_ColorSaturation = true;
		S.bOverride_SceneColorTint = true;
		S.bOverride_FilmGrainIntensity = true;
		S.bOverride_BloomIntensity = true;
		S.bOverride_AutoExposureBias = true;
		Post->RegisterComponent();
	}
	// the sky's outdoor exposure is what ours lifts from
	{
		TActorIterator<ALyudSky> It(World);
		if (It)
		{
			Sky = *It;
		}
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
	{
		Saves->RegisterSaveObject(TopiSaveKey, this);
	}
	// the music and the hero's lines here load in the background now, so nothing is loaded synchronously when they play
	{
		TArray<FSoftObjectPath> Paths;
		const auto Want = [&Paths](const FString& Path)
		{
			if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
			{
				Paths.Add(FSoftObjectPath(Path));
			}
		};
		Want(TopiMusicPath);
		Want(UralMusicPath);
		const FString& Lang = LyudText::Language();
		for (const TCHAR* Line : { TEXT("turn_1"), TEXT("turn_2"), TEXT("turn_3"), TEXT("turn_4"), TEXT("wake_1") })
		{
			const FString Name = FString::Printf(TEXT("V_line_hero_topi_%s"), Line);
			Want(FString::Printf(TEXT("/Game/Voice/%s/%s.%s"), *Lang, *Name, *Name));
		}
		if (Paths.Num() > 0)
		{
			Preload = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
		}
	}
}

void ULyudTopiMind::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortSequences();
	GoQuiet();
	ShowSubtitle(false);
	if (TopiMusic)
	{
		TopiMusic->Stop();
		TopiMusic = nullptr;
	}
	if (UralMusic)
	{
		UralMusic->Stop();
		UralMusic = nullptr;
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
	{
		Saves->UnregisterSaveObject(TopiSaveKey);
	}
	Preload.Reset();
	Super::EndPlay(EndPlayReason);
}

void ULyudTopiMind::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ALyudTopi* T = Topi();
	APlayerController* PC = nullptr;
	AHeroCharacter* Hero = PlayerHero(&PC);

	// the hero of a sequence is gone (a bird, a load, destroyed): put everything back
	if ((bTurning || bDissolving) && (!Hero || Hero != SequenceHero.Get()))
	{
		UE_LOG(LogTemp, Log, TEXT("LyudTopi: sequence interrupted (the pawn changed)"));
		AbortSequences();
	}
	// something else moved him far in one frame (a dev teleport, a quest's jump): the sequence is not his any more
	if ((bTurning || (bDissolving && !bWoken)) && Hero)
	{
		if (FVector::DistSquared(Hero->GetActorLocation(), SequenceAt) > FMath::Square(3000.0))
		{
			UE_LOG(LogTemp, Log, TEXT("LyudTopi: sequence interrupted (the hero was moved)"));
			AbortSequences();
		}
		SequenceAt = Hero->GetActorLocation();
	}
	if (LastHero.IsValid() && LastHero.Get() != Hero)
	{
		LastHero->SetSpeedScale(1.f);
	}
	LastHero = Hero;

	if (!T || !Hero || !PC)
	{
		if (bActive)
		{
			GoQuiet();
		}
		UpdateSubtitle(DeltaTime);
		return;
	}
	const FVector2D Map = T->ToMap(Hero->GetActorLocation());
	// no model call at all outside the near zone
	const bool bNear = LyudTopi::FModel::Near(Map.X, Map.Y, TopiNearM);
	if (!bNear && !bTurning && !bDissolving)
	{
		if (bActive)
		{
			GoQuiet();
		}
		UpdateSubtitle(DeltaTime);
		return;
	}
	bActive = true;
	const bool bNight = T->Night() > 0.5f;

	UpdatePressure(DeltaTime, *Hero, Map, bNight);
	if (bDissolving)
	{
		UpdateDissolve(DeltaTime, *Hero, *PC);
	}
	else
	{
		if (bTurning)
		{
			UpdateTurn(DeltaTime, *Hero, *PC);
		}
		UpdateRiver(DeltaTime, *Hero, bNight);
	}
	if (!bTurning && !bDissolving && bNear)
	{
		UpdateThoughts(DeltaTime, *Hero, Map, bNight);
	}
	// the wake may have taken him to the club: the rest only while he is still by the sand
	if (bDissolving || bTurning || bNear)
	{
		UpdateLook(DeltaTime);
		UpdateSound(DeltaTime, *Hero, Map, bNight);
	}
	UpdateSubtitle(DeltaTime);
}

// ------------------------------------------------------------------ the pressure

void ULyudTopiMind::UpdatePressure(float DeltaTime, AHeroCharacter& Hero, const FVector2D& Map, bool bNight)
{
	ALyudTopi* T = Topi();
	const bool bCalm = CVarTopiCalm.GetValueOnGameThread() > 0;
	const float Target = (bCalm || bDissolving) ? 0.f : float(LyudTopi::FModel::Pressure(Map.X, Map.Y, bNight));
	Pressure = FMath::FInterpConstantTo(Pressure, Target, DeltaTime, Target > Pressure ? 0.22f : 0.35f);
	if (bTurning && TurnTime < TopiTurnPinSeconds)
	{
		Pressure = FMath::Max(Pressure, 0.97f);
	}
	if (bCalm && !bTurning)
	{
		Pressure = 0.f;
	}
	if (T)
	{
		T->SetUnrest(Pressure);
	}
	// he slows to under half his pace as it presses
	Hero.SetSpeedScale(bDissolving ? 1.f : FMath::Lerp(1.f, 0.45f, Pressure));

	Grace = FMath::Max(0.f, Grace - DeltaTime);
	HighFor = Pressure > 0.92f ? HighFor + DeltaTime : 0.f;
	if (!bCalm && !bTurning && !bDissolving && Grace <= 0.f && (Pressure >= 0.999f || HighFor >= 4.f) && TopiOnFoot(Hero))
	{
		APlayerController* PC = nullptr;
		if (PlayerHero(&PC) == &Hero && PC)
		{
			BeginTurn(Hero, *PC);
		}
	}
}

// ------------------------------------------------------------------ turning back

void ULyudTopiMind::ForceTurnBack()
{
	APlayerController* PC = nullptr;
	AHeroCharacter* Hero = PlayerHero(&PC);
	if (Hero && PC && !bTurning && !bDissolving && TopiOnFoot(*Hero))
	{
		BeginTurn(*Hero, *PC);
	}
}

void ULyudTopiMind::BeginTurn(AHeroCharacter& Hero, APlayerController& PC)
{
	const ALyudTopi* T = Topi();
	if (!T)
	{
		return;
	}
	const FVector2D Map = T->ToMap(Hero.GetActorLocation());
	const LyudTopi::FVec2 Out = LyudTopi::FModel::WayOut(Map.X, Map.Y);
	// map X is world X, map Y is world Y
	TurnDir = FVector(Out.X, Out.Y, 0.0).GetSafeNormal();
	if (TurnDir.IsNearlyZero())
	{
		TurnDir = FVector(0.0, 1.0, 0.0);
	}
	TurnYaw = float(TurnDir.Rotation().Yaw);
	const FRotator Control = PC.GetControlRotation();
	TurnStartYaw = float(Control.Yaw);
	TurnStartPitch = float(FRotator::NormalizeAxis(Control.Pitch));
	TurnTime = 0.f;
	bTurning = true;
	SequenceHero = &Hero;
	SequenceAt = Hero.GetActorLocation();
	HighFor = 0.f;
	LockInput(&PC, true);

	// his line: the four in order, then at random (not the last one again)
	int32 N = 1;
	if (TurnLines < 4)
	{
		N = TurnLines + 1;
	}
	else
	{
		N = FMath::RandRange(1, 3);
		if (N >= LastTurnLine)
		{
			N++;
		}
	}
	TurnLines++;
	LastTurnLine = N;
	Say(FString::Printf(TEXT("line.hero.topi.turn.%d"), N), Hero);
	ULyudQuests::Notify(this, FName(TEXT("topi_turned_back")));
	UE_LOG(LogTemp, Log, TEXT("LyudTopi: turning back (pressure %.2f, way out yaw %.0f, line %d)"), Pressure, TurnYaw, N);
}

void ULyudTopiMind::UpdateTurn(float DeltaTime, AHeroCharacter& Hero, APlayerController& PC)
{
	TurnTime += DeltaTime;
	const float A = TopiSmooth01(TurnTime / TopiTurnRotateSeconds);
	const float Yaw = TurnStartYaw + FMath::FindDeltaAngleDegrees(TurnStartYaw, TurnYaw) * A;
	const float Pitch = FMath::Lerp(TurnStartPitch, -8.f, A);
	PC.SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	if (TurnTime >= TopiTurnRotateSeconds)
	{
		// input is ignored: the walk is forced
		Hero.AddMovementInput(TurnDir, 1.f, true);
	}
	if (TurnTime >= TopiTurnSeconds)
	{
		EndTurn();
	}
}

void ULyudTopiMind::EndTurn()
{
	bTurning = false;
	TurnTime = 0.f;
	Grace = TopiTurnGraceSeconds;
	if (!bDissolving)
	{
		LockInput(nullptr, false);
		SequenceHero.Reset();
	}
}

// ------------------------------------------------------------------ the river

void ULyudTopiMind::UpdateRiver(float DeltaTime, AHeroCharacter& Hero, bool bNight)
{
	const ALyudTopi* T = Topi();
	if (!T || !bNight)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Hero.GetCapsuleComponent();
	const FVector Feet = Hero.GetActorLocation() - FVector(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f);
	const float Depth = T->RiverDepthAt(Feet);
	if (Depth <= 5.f)
	{
		return;
	}
	// the stream carries him (swept: he does not go through the banks)
	const FVector Flow = T->RiverFlowAt(Feet);
	Hero.AddActorWorldOffset(Flow * (DeltaTime * FMath::Min(Depth / 100.f, 1.f) * 0.35f), true);
	if (Depth > TopiChestDepthCm && !bTurning && TopiOnFoot(Hero))
	{
		APlayerController* PC = nullptr;
		if (PlayerHero(&PC) == &Hero && PC)
		{
			BeginDissolve(Hero, *PC);
		}
	}
}

void ULyudTopiMind::StartDissolve()
{
	APlayerController* PC = nullptr;
	AHeroCharacter* Hero = PlayerHero(&PC);
	if (Hero && PC && !bDissolving && TopiOnFoot(*Hero))
	{
		BeginDissolve(*Hero, *PC);
	}
}

void ULyudTopiMind::BeginDissolve(AHeroCharacter& Hero, APlayerController& PC)
{
	if (bTurning)
	{
		// one lock serves both: the turn ends without letting go
		bTurning = false;
		TurnTime = 0.f;
	}
	bDissolving = true;
	bWoken = false;
	bWhiteStarted = false;
	bClearStarted = false;
	bWakeSaid = false;
	DissolveTime = 0.f;
	Dissolve = 0.f;
	SequenceHero = &Hero;
	SequenceAt = Hero.GetActorLocation();
	LockInput(&PC, true);
	DiveStartPitch = float(FRotator::NormalizeAxis(PC.GetControlRotation().Pitch));
	if (UCharacterMovementComponent* Move = Hero.GetCharacterMovement())
	{
		SavedGravity = Move->GravityScale;
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
		Move->GravityScale = 0.f;
	}
	Hero.SetActorEnableCollision(false);
	UE_LOG(LogTemp, Log, TEXT("LyudTopi: into the Ural (time %d)"), TimesDissolved + 1);
}

void ULyudTopiMind::UpdateDissolve(float DeltaTime, AHeroCharacter& Hero, APlayerController& PC)
{
	ALyudTopi* T = Topi();
	DissolveTime += DeltaTime;
	if (!bWoken)
	{
		// he goes under: looks down into the light, drifts with the stream, sinks till the eyes are well under
		const float A = TopiSmooth01(DissolveTime / 2.f);
		const FRotator Control = PC.GetControlRotation();
		PC.SetControlRotation(FRotator(FMath::Lerp(DiveStartPitch, -35.f, A), float(Control.Yaw), 0.f));
		if (UCharacterMovementComponent* Move = Hero.GetCharacterMovement())
		{
			Move->Velocity = FVector::ZeroVector;
		}
		if (T)
		{
			const FVector Here = Hero.GetActorLocation();
			const float EyeUp = float(Hero.EyeLocation().Z - Here.Z);
			const float WantZ = T->RiverSurfaceZ() - 60.f - EyeUp;
			const float Slow = DissolveTime < TopiDiveSeconds ? 0.5f : 0.2f;
			FVector Next = Here + T->RiverFlowAt(Here) * (Slow * DeltaTime);
			Next.Z = FMath::FInterpConstantTo(float(Here.Z), FMath::Min(WantZ, float(Here.Z)), DeltaTime, 45.f);
			Hero.SetActorLocation(Next, false);
		}
		Dissolve = FMath::Clamp(DissolveTime / TopiLightEndsAt, 0.f, 1.f);
		if (T)
		{
			T->SetDissolve(Dissolve);
		}
		if (!bWhiteStarted && DissolveTime >= TopiWhiteFrom)
		{
			bWhiteStarted = true;
			if (PC.PlayerCameraManager)
			{
				PC.PlayerCameraManager->StartCameraFade(0.f, 1.f, TopiLightEndsAt - TopiWhiteFrom, TopiWhiteTeal, false, true);
			}
		}
		if (DissolveTime >= TopiLightEndsAt)
		{
			Wake(Hero, PC);
		}
		return;
	}
	// the white holds, then the eyes open
	if (!bClearStarted && DissolveTime >= TopiLightEndsAt + TopiWhiteHold)
	{
		bClearStarted = true;
		if (PC.PlayerCameraManager)
		{
			PC.PlayerCameraManager->StartCameraFade(1.f, 0.f, TopiClearSeconds, TopiWhiteTeal, false, false);
		}
	}
	if (!bWakeSaid && DissolveTime >= TopiLightEndsAt + TopiWhiteHold + 1.6f)
	{
		bWakeSaid = true;
		LockInput(nullptr, false);
		Say(TopiWakeLine, Hero);
		ULyudQuests::Notify(this, FName(TEXT("ural_dissolved")));
	}
	if (DissolveTime >= TopiSequenceEndsAt)
	{
		bDissolving = false;
		bWoken = false;
		SequenceHero.Reset();
		LockInput(nullptr, false);
	}
}

void ULyudTopiMind::Wake(AHeroCharacter& Hero, APlayerController& PC)
{
	UWorld* World = GetWorld();
	bWoken = true;
	Dissolve = 0.f;
	Pressure = 0.f;
	HighFor = 0.f;
	if (ALyudTopi* T = Topi())
	{
		T->SetDissolve(0.f);
		T->SetUnrest(0.f);
	}

	// the wake-up spot: PlayerStart_WakeUp (by name or tag), else any start, else the seat at the computer
	bool bFound = false;
	FVector Where = FVector::ZeroVector;
	FRotator Facing = FRotator::ZeroRotator;
	if (World)
	{
		APlayerStart* First = nullptr;
		APlayerStart* WakeStart = nullptr;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			APlayerStart* Start = *It;
			if (!First)
			{
				First = Start;
			}
			bool bWakeName = Start->GetName().Contains(TEXT("WakeUp")) || Start->PlayerStartTag.ToString().Contains(TEXT("WakeUp"));
#if WITH_EDITOR
			bWakeName = bWakeName || Start->GetActorLabel().Contains(TEXT("WakeUp"));
#endif
			if (bWakeName)
			{
				WakeStart = Start;
				break;
			}
		}
		APlayerStart* Use = WakeStart ? WakeStart : First;
		if (Use)
		{
			bFound = true;
			Where = Use->GetActorLocation();
			Facing = Use->GetActorRotation();
		}
		else
		{
			TActorIterator<AHeroSeat> SeatIt(World);
			if (SeatIt)
			{
				const UCapsuleComponent* Capsule = Hero.GetCapsuleComponent();
				bFound = true;
				Where = SeatIt->GetActorLocation() + FVector(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() + 5.f : 100.f);
				Facing = SeatIt->GetActorRotation();
			}
		}
	}

	Hero.SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Move = Hero.GetCharacterMovement())
	{
		Move->GravityScale = SavedGravity;
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Walking);
	}
	if (bFound)
	{
		Hero.SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
		PC.SetControlRotation(FRotator(0.f, float(Facing.Yaw), 0.f));
	}

	// the next morning, half past seven
	if (UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr)
	{
		const FDateTime Now = Cal->Now();
		FDateTime Morning(Now.GetYear(), Now.GetMonth(), Now.GetDay(), 7, 30, 0);
		if (Morning <= Now)
		{
			Morning += FTimespan::FromDays(1.0);
		}
		Cal->SetNow(Morning);
	}

	// the white stays on (the fade holds) while the place changes round him
	if (PC.PlayerCameraManager)
	{
		PC.PlayerCameraManager->StartCameraFade(1.f, 1.f, TopiWhiteHold, TopiWhiteTeal, false, true);
	}
	TimesDissolved++;
	UE_LOG(LogTemp, Log, TEXT("LyudTopi: woke at the computer (%s, times %d)"), bFound ? TEXT("found the spot") : TEXT("no spot"), TimesDissolved);
}

// ------------------------------------------------------------------ putting things back

void ULyudTopiMind::LockInput(APlayerController* PC, bool bLock)
{
	if (bLock)
	{
		if (bInputLocked || !PC)
		{
			return;
		}
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		LockedPC = PC;
		bInputLocked = true;
		return;
	}
	if (!bInputLocked)
	{
		return;
	}
	bInputLocked = false;
	if (APlayerController* Locked = LockedPC.Get())
	{
		Locked->SetIgnoreMoveInput(false);
		Locked->SetIgnoreLookInput(false);
	}
	LockedPC.Reset();
}

void ULyudTopiMind::AbortSequences()
{
	if (bDissolving)
	{
		if (AHeroCharacter* Hero = SequenceHero.Get())
		{
			Hero->SetActorEnableCollision(true);
			if (UCharacterMovementComponent* Move = Hero->GetCharacterMovement())
			{
				Move->GravityScale = SavedGravity;
				Move->SetMovementMode(MOVE_Walking);
			}
		}
		if (APlayerController* PC = LockedPC.Get(); PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StopCameraFade();
		}
		if (ALyudTopi* T = Topi())
		{
			T->SetDissolve(0.f);
		}
	}
	bDissolving = false;
	bWoken = false;
	Dissolve = 0.f;
	if (bTurning)
	{
		Grace = TopiTurnGraceSeconds;
	}
	bTurning = false;
	TurnTime = 0.f;
	LockInput(nullptr, false);
	SequenceHero.Reset();
}

void ULyudTopiMind::GoQuiet()
{
	bActive = false;
	Pressure = 0.f;
	HighFor = 0.f;
	TopiLevel = 0.f;
	UralLevel = 0.f;
	if (ALyudTopi* T = Topi())
	{
		T->SetUnrest(0.f);
	}
	if (AHeroCharacter* Hero = LastHero.Get())
	{
		Hero->SetSpeedScale(1.f);
	}
	if (Post)
	{
		Post->BlendWeight = 0.f;
	}
	if (TopiMusic && TopiMusic->IsPlaying())
	{
		TopiMusic->FadeOut(1.5f, 0.f);
	}
	if (UralMusic && UralMusic->IsPlaying())
	{
		UralMusic->FadeOut(1.5f, 0.f);
	}
	DropLoop(HeartLoop);
	DropLoop(HumLoop);
	if (bMusicHeld)
	{
		bMusicHeld = false;
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
		{
			S->SetMusicHold(this, false);
		}
	}
}

// ------------------------------------------------------------------ the look

void ULyudTopiMind::UpdateLook(float DeltaTime)
{
	if (!Post)
	{
		return;
	}
	const float P = Pressure;
	const float D = Dissolve;
	FPostProcessSettings& S = Post->Settings;
	// what the sky's outdoor exposure sets (ours blends over it, so we lift from it)
	float BaseBias = 0.f;
	float BaseBloom = 0.675f;
	if (Sky && Sky->OutdoorExposure)
	{
		BaseBias = Sky->OutdoorExposure->Settings.AutoExposureBias;
		BaseBloom = Sky->OutdoorExposure->Settings.BloomIntensity;
	}
	S.VignetteIntensity = 0.4f + 0.9f * P;
	S.SceneFringeIntensity = 4.f * P;
	const float Sat = FMath::Lerp(1.f, 0.55f, P);
	S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	S.FilmGrainIntensity = 0.65f * P;
	S.BloomIntensity = BaseBloom + 0.35f * P + 7.f * D;
	S.AutoExposureBias = BaseBias + 3.f * TopiSmooth01(D);
	// bleached and warm as it presses; cyan-white as the river takes him
	const FLinearColor Warm = FMath::Lerp(FLinearColor::White, FLinearColor(1.05f, 1.f, 0.88f), P);
	const FLinearColor Light = FLinearColor(0.8f, 1.15f, 1.1f) * (1.f + 1.2f * D);
	S.SceneColorTint = FMath::Lerp(Warm, Light, TopiSmooth01(D * 1.4f));
	const float Weight = FMath::Max(TopiSmooth01(P), TopiSmooth01(D * 3.f));
	Post->BlendWeight = FMath::FInterpTo(Post->BlendWeight, Weight, DeltaTime, 4.f);
}

// ------------------------------------------------------------------ the sound

void ULyudTopiMind::SetLevel(UAudioComponent* Music, float Level) const
{
	if (!Music)
	{
		return;
	}
	if (Level <= 0.002f)
	{
		if (Music->IsPlaying())
		{
			Music->Stop();
		}
		return;
	}
	const ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this);
	const float Volume = S ? S->MasterVolume * S->MusicVolume * 1.6f : 0.7f;
	Music->SetVolumeMultiplier(FMath::Min(Volume * Level, 1.f));
	// a track that does not loop by itself starts again
	if (!Music->IsPlaying())
	{
		Music->Play();
	}
}

void ULyudTopiMind::DropLoop(TObjectPtr<UAudioComponent>& Loop)
{
	// AttachLoop spawns them not to destroy themselves. FadeOut on a loop that is not playing never completes, so that
	// one is destroyed at once (ULyudSoundscapeSubsystem::ReleaseLoop on newer main does the same)
	if (IsValid(Loop.Get()) && Loop->IsPlaying())
	{
		Loop->bAutoDestroy = true;
		Loop->FadeOut(1.2f, 0.f);
	}
	else if (IsValid(Loop.Get()))
	{
		Loop->Stop();
		Loop->DestroyComponent();
	}
	Loop = nullptr;
}

void ULyudTopiMind::UpdateSound(float DeltaTime, AHeroCharacter& Hero, const FVector2D& Map, bool bNight)
{
	ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this);
	const ALyudTopi* T = Topi();
	if (!bTriedTracks)
	{
		bTriedTracks = true;
		TopiTrack = LoadObject<USoundBase>(nullptr, TopiMusicPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UralTrack = LoadObject<USoundBase>(nullptr, UralMusicPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (TopiTrack)
		{
			TopiMusic = UGameplayStatics::CreateSound2D(this, TopiTrack, 1.f, 1.f, 0.f, nullptr, false, false);
		}
		if (UralTrack)
		{
			UralMusic = UGameplayStatics::CreateSound2D(this, UralTrack, 1.f, 1.f, 0.f, nullptr, false, false);
		}
		UE_LOG(LogTemp, Log, TEXT("LyudTopi: music topi %d, ural %d"), TopiTrack ? 1 : 0, UralTrack ? 1 : 0);
	}

	// the river's music: at night near where the path comes out on the bank; it swells as he dissolves, goes in the white
	const double Offset = FMath::Abs(LyudTopi::FModel::RiverOffset(Map.X, Map.Y));
	const bool bByRiver = bNight && !bWoken && Offset < 220.0 && LyudTopi::FModel::Pressure(Map.X, Map.Y, true) < 0.05;
	float UralTarget = bByRiver ? float(1.0 - 0.5 * FMath::Clamp(Offset / 220.0, 0.0, 1.0)) : 0.f;
	float UralRate = UralTarget > UralLevel ? 0.2f : 0.15f;
	if (bDissolving)
	{
		UralTarget = bWoken ? 0.f : 1.35f;
		UralRate = bWoken ? 1.f / TopiWhiteHold : 0.3f;
	}
	UralLevel = FMath::FInterpConstantTo(UralLevel, UralTarget, DeltaTime, UralRate);

	// the dunes' music: deeper than 60 m in, louder as it presses; the river's takes over
	const float Depth = (T && !bWoken) ? T->DepthInto(Hero.GetActorLocation()) : 0.f;
	const float TopiTarget = FMath::SmoothStep(60.f, 160.f, Depth) * (0.45f + 0.55f * Pressure) * (1.f - FMath::Min(UralLevel, 1.f));
	TopiLevel = FMath::FInterpConstantTo(TopiLevel, TopiTarget, DeltaTime, TopiTarget > TopiLevel ? 0.25f : 0.2f);
	SetLevel(TopiMusic, TopiLevel);
	SetLevel(UralMusic, UralLevel);

	const bool bHold = (TopiMusic && TopiLevel > 0.05f) || (UralMusic && UralLevel > 0.05f);
	if (bHold != bMusicHeld && Scape)
	{
		bMusicHeld = bHold;
		Scape->SetMusicHold(this, bHold);
	}

	// loops: the heartbeat as it presses, the river's hum near it at night
	const float Effects = Scape ? Scape->MasterVolume * Scape->EffectsVolume : 1.f;
	LoopRetry = FMath::Max(0.f, LoopRetry - DeltaTime);
	// a loop that stopped by itself is let go, so it is attached again below
	if (HeartLoop && !HeartLoop->IsPlaying())
	{
		DropLoop(HeartLoop);
	}
	if (HumLoop && !HumLoop->IsPlaying())
	{
		DropLoop(HumLoop);
	}
	const float Heart = Pressure > 0.35f ? Pressure * Pressure : 0.f;
	if (Heart > 0.f && !IsValid(HeartLoop.Get()) && Scape && LoopRetry <= 0.f)
	{
		HeartLoop = Scape->AttachLoop(TopiHeartPool, Hero.GetRootComponent(), Heart);
		LoopRetry = HeartLoop ? 0.f : 5.f;
	}
	else if (Pressure < 0.3f && HeartLoop)
	{
		DropLoop(HeartLoop);
	}
	if (IsValid(HeartLoop.Get()))
	{
		HeartLoop->SetVolumeMultiplier(Heart * Effects);
	}
	const float Hum = (bNight && !bWoken) ? FMath::Square(1.f - FMath::Clamp(float(Offset) / 200.f, 0.f, 1.f)) : 0.f;
	if (Hum > 0.01f && !IsValid(HumLoop.Get()) && Scape && LoopRetry <= 0.f)
	{
		HumLoop = Scape->AttachLoop(TopiHumPool, Hero.GetRootComponent(), Hum);
		LoopRetry = HumLoop ? 0.f : 5.f;
	}
	else if (Hum <= 0.005f && HumLoop)
	{
		DropLoop(HumLoop);
	}
	if (IsValid(HumLoop.Get()))
	{
		HumLoop->SetVolumeMultiplier(Hum * Effects);
	}
}

// ------------------------------------------------------------------ his lines

// Things he says to himself as the place opens before him (no input, subtitle and voice like the other lines). Each at
// most once in twenty minutes, never over another line, and never while the place is turning him back.
void ULyudTopiMind::UpdateThoughts(float DeltaTime, AHeroCharacter& Hero, const FVector2D& Map, bool bNight)
{
	static const TCHAR* const TopiThoughts[4] =
	{
		TEXT("line.hero.topi.see.1"),       // by day, the first dunes behind him
		TEXT("line.hero.topi.glow.1"),      // at night on the path: light beyond the dunes
		TEXT("line.hero.topi.river.1"),     // at night: the river in sight
		TEXT("line.hero.topi.towels.1"),    // at night on the bank: the towels
	};
	ThoughtQuiet = FMath::Max(0.f, ThoughtQuiet - DeltaTime);
	if (!SubtitleKey.IsEmpty())
	{
		ThoughtQuiet = 3.f;                 // a breath between two lines
		return;
	}
	if (ThoughtQuiet > 0.f || bWoken)
	{
		return;
	}
	double Along = 0.0;
	const double PathD = LyudTopi::FModel::PathDistance(Map.X, Map.Y, &Along);
	const double In = LyudTopi::FModel::Depth(Map.X, Map.Y);
	const double Off = FMath::Abs(LyudTopi::FModel::RiverOffset(Map.X, Map.Y));
	int32 Which = INDEX_NONE;
	if (!bNight)
	{
		if (In > 25.0 && Pressure > 0.15f && Pressure < 0.6f)
		{
			Which = 0;
		}
	}
	else if (PathD < 20.0 && Off < LyudTopi::RiverHalfWidth + 12.0 && Off > LyudTopi::RiverHalfWidth)
	{
		Which = 3;
	}
	else if (PathD < 60.0 && Off < 110.0 && Off > LyudTopi::RiverHalfWidth + 20.0)
	{
		Which = 2;
	}
	else if (PathD < 25.0 && Along > 80.0 && Along < 300.0)
	{
		Which = 1;
	}
	if (Which == INDEX_NONE)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - ThoughtAt[Which] < 1200.0)
	{
		return;
	}
	ThoughtAt[Which] = Now;
	Say(TopiThoughts[Which], Hero);
}

void ULyudTopiMind::Say(const FString& Key, const AHeroCharacter& Hero)
{
	USoundBase* Voice = LyudText::LoadVoice(Key);
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this); S && Voice)
	{
		S->PlayVoiceAt(Voice, Hero.GetPawnViewLocation(), 1.f);
	}
	SubtitleKey = Key;
	SubtitleFor = Voice ? FMath::Max(3.f, Voice->GetDuration() + 0.8f) : 4.5f;
	ShowSubtitle(true);
	UE_LOG(LogTemp, Log, TEXT("LyudTopi: says %s (%s)"), *Key, Voice ? TEXT("voice") : TEXT("subtitle only"));
}

void ULyudTopiMind::UpdateSubtitle(float DeltaTime)
{
	if (SubtitleKey.IsEmpty())
	{
		return;
	}
	SubtitleFor -= DeltaTime;
	SubtitleAlpha = FMath::FInterpConstantTo(SubtitleAlpha, SubtitleFor > 0.f ? 1.f : 0.f, DeltaTime, 3.f);
	if (SubtitleFor <= 0.f && SubtitleAlpha <= 0.f)
	{
		SubtitleKey.Reset();
		ShowSubtitle(false);
	}
}

void ULyudTopiMind::ShowSubtitle(bool bShow)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		SubtitleWidget.Reset();
		return;
	}
	if (bShow && !SubtitleWidget)
	{
		SubtitleWidget = SNew(SLyudTopiSubtitle).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(SubtitleWidget.ToSharedRef(), 58);
	}
	else if (!bShow && SubtitleWidget)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SubtitleWidget.ToSharedRef());
		SubtitleWidget.Reset();
	}
}
