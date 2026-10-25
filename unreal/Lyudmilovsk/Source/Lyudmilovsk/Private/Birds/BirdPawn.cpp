#include "Birds/BirdPawn.h"

#include "BirdAir.h"
#include "BirdBio.h"
#include "BirdSound.h"
#include "BirdWindStreaks.h"
#include "Components/AudioComponent.h"
#include "Audio/LyudWindAudio.h"
#include "Weather/LyudWind.h"
#include "GameFramework/PhysicsVolume.h"
#include "Sea/LyudSea.h"
#include "Sound/SoundWaveProcedural.h"
#include "BirdFlightModel.h"
#include "BirdHud.h"
#include "BirdSelfBody.h"
#include "Birds/BirdActor.h"
#include "Birds/BirdSpecies.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeadTracker.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#if __has_include("Animals/AnimalLife.h")
#include "Animals/AnimalLife.h"
#define LYUD_HAS_ANIMALS 1
#else
#define LYUD_HAS_ANIMALS 0
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarHeadSteer(TEXT("lyud.Bird.HeadSteer"), 0, TEXT("1: tilting your head (AirPods) banks the bird."));
	TAutoConsoleVariable<int32> CVarChase(TEXT("lyud.Bird.ChaseCam"), 0, TEXT("1: watch the player bird from behind (recordings, checks)."));
	TAutoConsoleVariable<int32> CVarFreeze(TEXT("lyud.Bird.Freeze"), 0, TEXT("1: the player bird holds still in the air, wings and tail still move (pictures)."));
	TAutoConsoleVariable<float> CVarChaseYaw(TEXT("lyud.Bird.ChaseYaw"), 0.f, TEXT("Chase camera: degrees round the bird from behind."));
	TAutoConsoleVariable<float> CVarChasePitch(TEXT("lyud.Bird.ChasePitch"), 12.f, TEXT("Chase camera: degrees above the bird."));
	TAutoConsoleVariable<float> CVarChaseDist(TEXT("lyud.Bird.ChaseDist"), 1.f, TEXT("Chase camera: distance scale."));
	TAutoConsoleVariable<float> CVarWingFov(TEXT("lyud.Bird.WingFov"), 150.f, TEXT("Field of view the bird's own body is drawn with in first person (wider brings the wings into view)."));
	TAutoConsoleVariable<float> CVarFov(TEXT("lyud.Bird.Fov"), 100.f, TEXT("The player bird's field of view at rest, degrees."));

	/** Seconds a click waits for the other button to make it a full beat. */
	constexpr double PairWindow = 0.07;
	/** A button held this long pulls its wing in. */
	constexpr float TuckAfter = 0.18f;
	constexpr float NearClip = 0.3f;

	BirdFlight::EStyle StyleOf(EBirdFlightStyle S)
	{
		switch (S)
		{
		case EBirdFlightStyle::Bounding: return BirdFlight::EStyle::Bounding;
		case EBirdFlightStyle::Undulating: return BirdFlight::EStyle::Undulating;
		case EBirdFlightStyle::Clapping: return BirdFlight::EStyle::Clapping;
		case EBirdFlightStyle::Soaring: return BirdFlight::EStyle::Soaring;
		case EBirdFlightStyle::Flicker: return BirdFlight::EStyle::Flicker;
		default: return BirdFlight::EStyle::Flapping;
		}
	}
	FVector ToCm(const BirdFlight::V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	BirdFlight::V3 ToM(const FVector& V) { return BirdFlight::V3(V.X / 100.0, V.Y / 100.0, V.Z / 100.0); }
	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
}

ABirdPawn::ABirdPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(3.f);
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;
	Body = CreateDefaultSubobject<UBirdSelfBody>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetUsingAbsoluteRotation(true);
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Collision);
	Head->SetUsingAbsoluteRotation(true);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Head);
	Camera->bUsePawnControlRotation = false;
	WindStreaks = CreateDefaultSubobject<UBirdWindStreaks>(TEXT("WindStreaks"));
	WindStreaks->SetupAttachment(Collision);
	bUseControllerRotationYaw = bUseControllerRotationPitch = bUseControllerRotationRoll = false;
	Params = MakeShared<BirdFlight::FParams>();
	Flight = MakeShared<BirdFlight::FState>();
	Air = MakeShared<FBirdAir>();
	Tracker = MakeShared<FHeadTracker>();
}

ABirdPawn::~ABirdPawn() = default;

void ABirdPawn::Setup(FName InSpecies, const FVector& Location, float Yaw, const FVector& VelocityCm, bool bStanding, ABirdActor* Outside)
{
	OutsideBody = Outside;
	bOwnOutsideBody = Outside == nullptr;
	if (!OutsideBody && GetWorld())
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.bDeferConstruction = true;
		if (ABirdActor* A = GetWorld()->SpawnActor<ABirdActor>(ABirdActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Location), P))
		{
			A->Species = InSpecies;
			A->bHeroBody = true;
			A->InitialState = bStanding ? EBirdState::Perch : EBirdState::Fly;
			A->FinishSpawning(FTransform(FRotator(0.f, Yaw, 0.f), Location));
			OutsideBody = A;
		}
	}
	if (OutsideBody)
	{
		// the bird you are gets the detailed body (SK_<Id>_Hero) where there is one
		OutsideBody->bHeroBody = true;
		OutsideBody->SetSpecies(InSpecies);
		OutsideBody->SetActorEnableCollision(false);
	}
	Species = InSpecies;
	FBirdSpecies Row;
	if (const FBirdSpecies* Found = UBirdSpeciesLibrary::Find(InSpecies))
	{
		Row = *Found;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Bird flight: species %s not in species.json, flying a generic bird"), *InSpecies.ToString());
	}
	*Params = BirdFlight::MakeSpeciesParams(TCHAR_TO_ANSI(*InSpecies.ToString()), StyleOf(Row.Flight), Row.MassG, Row.WingspanCm, Row.WingbeatHz, Row.CruiseMs);
	const BirdBio::FBio& Bio = BirdBio::Get(TCHAR_TO_ANSI(*InSpecies.ToString()));
	bSwims = Bio.Swims;
	RunUp = (float)Bio.RunUpS;
	// its own sound: the air past it and its wingbeats
	Sound = MakeShared<FBirdSound>();
	if (USoundWaveProcedural* Wave = Sound->Init(this, Row.MassG / 1000.0, Row.Flight == EBirdFlightStyle::Clapping, InSpecies == FName(TEXT("MuteSwan"))))
	{
		SoundOut = NewObject<UAudioComponent>(this, TEXT("BirdSound"));
		SoundOut->bAutoActivate = false;
		SoundOut->bAllowSpatialization = false;
		SoundOut->bIsUISound = false;
		SoundOut->SetSound(Wave);
		SoundOut->SetupAttachment(Head);
		SoundOut->RegisterComponent();
		SoundOut->Play();
	}
	// the air past the head: the soundscape's own wind voice, fed with the bird's airspeed
	WindVoice = NewObject<ULyudWindAudioComponent>(this, TEXT("FlightWind"));
	WindVoice->SetupAttachment(Head);
	WindVoice->RegisterComponent();
	WindVoice->Start();
	Sound->bAirRush = false;
	Params->Hops = Row.Gait == EBirdGait::Hop || Row.Gait == EBirdGait::Both;
	Body->Build(InSpecies, Row.LengthCm, Row.WingspanCm, Head);
	Collision->SetSphereRadius(FMath::Max(Body->BodyRadius() * 0.85f, 1.5f));
	// the sea's surface is water, not ground (StepAir and StepSwim deal with it)
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Collision->IgnoreActorWhenMoving(Sea, true);
		if (AActor* Vol = Sea->GetWaterVolume()) Collision->IgnoreActorWhenMoving(Vol, true);
	}
	Air->Init(GetWorld());

	BodyRot = FRotator(0.f, Yaw, 0.f);
	*Flight = BirdFlight::FState();
	Flight->Vel = ToM(VelocityCm);
	if (bStanding)
	{
		Mode = EBirdMode::Ground;
		Feet = Location;
		bAnchored = true;
		Fold = LegsDown = 1.f;
		SetActorLocation(Feet + FVector(0, 0, Body->StandHeight()));
	}
	else
	{
		Mode = EBirdMode::Air;
		SetActorLocation(Location);
	}
	Flight->Pos = ToM(GetActorLocation());
	UpdateBody(0.f);
	UpdateHead(FBirdControls(), 0.f);
}

void ABirdPawn::BeginArrival(const FMinimalViewInfo& From, float Seconds)
{
	TransFrom = From;
	TransT = 0.f;
	TransDur = FMath::Max(Seconds, 0.1f);
	Mode = EBirdMode::Arriving;
	Body->SetShown(true, true);
}

void ABirdPawn::BeginLeaving(const FMinimalViewInfo& To, TFunction<void()> OnLeft)
{
	TransFrom = BirdView();
	TransTo = To;
	TransT = 0.f;
	const float Dist = FVector::Dist(TransFrom.Location, To.Location) / 100.f;
	TransDur = FMath::Clamp(1.2f + Dist / 60.f, 1.2f, 3.f);
	Mode = EBirdMode::Leaving;
	OnLeftCb = MoveTemp(OnLeft);
	OnReturnRequested = nullptr;
}

void ABirdPawn::SkipArrival()
{
	if (Mode == EBirdMode::Arriving)
	{
		Mode = bAnchored || Flight->Vel.Size() < 0.5 ? EBirdMode::Ground : EBirdMode::Air;
		if (bDescendFirst && Mode == EBirdMode::Ground)
		{
			// off the palm and down onto the ground in front of the hero: a hop out, two beats, a flare
			bDescendFirst = false;
			TakeOff();
			const FVector Fwd = FRotator(0.f, BodyRot.Yaw, 0.f).Vector();
			Flight->Vel = ToM((Fwd * 1.3f + FVector(0, 0, 0.6f)) * 100.f);
			TakeoffAssist = 0.25f;
			bAutoDescend = true;
		}
	}
	Body->SetShown(true, false);
}

void ABirdPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	Tracker->Start();
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -85.f;
			PC->PlayerCameraManager->ViewPitchMax = 85.f;
		}
		Hud = SBirdHud::AddTo(this);
	}
	Age = 0.f;
}

void ABirdPawn::UnPossessed()
{
	SBirdHud::Remove(Hud);
	Hud.Reset();
	Super::UnPossessed();
}

void ABirdPawn::EndPlay(const EEndPlayReason::Type Reason)
{
	if (OutsideBody && bOwnOutsideBody)
	{
		OutsideBody->Destroy();
	}
	OutsideBody = nullptr;
	SBirdHud::Remove(Hud);
	Hud.Reset();
	Tracker->Stop();
	Super::EndPlay(Reason);
}

void ABirdPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// the same look actions as the hero's (the template's mouse look), so the mouse feels the same
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		for (const TCHAR* Path : { TEXT("/Game/Input/Actions/IA_Look.IA_Look"), TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook") })
		{
			if (UInputAction* A = LoadObject<UInputAction>(nullptr, Path))
			{
				Input->BindAction(A, ETriggerEvent::Triggered, this, &ABirdPawn::OnLook);
			}
		}
	}
}

void ABirdPawn::OnLook(const FInputActionValue& Value)
{
	MouseLook += Value.Get<FVector2D>();
}

void ABirdPawn::SetFakeHead(const FRotator& FakeHead, bool bOn)
{
	Tracker->SetFake(FakeHead, bOn);
}

UPrimitiveComponent* ABirdPawn::GetRootPrimitive() const
{
	return Collision;
}

bool ABirdPawn::IsHeadTracked() const
{
	return Tracker->IsTracking();
}

FVector ABirdPawn::GetVelocityCm() const
{
	return Mode == EBirdMode::Swimming ? SwimVel : ToCm(Mode == EBirdMode::Air ? Flight->Ground : BirdFlight::V3());
}

float ABirdPawn::GetAirspeed() const
{
	return Mode == EBirdMode::Air ? (float)BirdFlight::Speed(*Flight) : 0.f;
}

// ------------------------------------------------------------------ input

void ABirdPawn::ReadControls(float Dt, FBirdControls& C)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	FVector2D LookDeg = FVector2D::ZeroVector;
	bool bRawMouse = false;
	if (bScripted)
	{
		C = Script;
		Script.Peck = Script.Call = false;
		LookDeg = Script.Look;
		Script.Look = FVector2D::ZeroVector;
		MouseLook = FVector2D::ZeroVector;
	}
	else if (PC)
	{
		auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
		C.Left = Down(EKeys::LeftMouseButton);
		C.Right = Down(EKeys::RightMouseButton);
		C.Space = Down(EKeys::SpaceBar);
		C.Shift = Down(EKeys::LeftShift) || Down(EKeys::RightShift);
		C.Ctrl = Down(EKeys::LeftControl) || Down(EKeys::RightControl);
		C.FreeLook = Down(EKeys::C) || Down(EKeys::MiddleMouseButton);
		C.Return = Down(EKeys::R);
		C.Peck = PC->WasInputKeyJustPressed(EKeys::E);
		C.Call = PC->WasInputKeyJustPressed(EKeys::F);
		C.Move = FVector2D((Down(EKeys::D) ? 1 : 0) - (Down(EKeys::A) ? 1 : 0), (Down(EKeys::W) ? 1 : 0) - (Down(EKeys::S) ? 1 : 0));
		C.Roll = C.Move.X;
		bRawMouse = true;
	}
	if (Mode == EBirdMode::Arriving || Mode == EBirdMode::Leaving)
	{
		MouseLook = FVector2D::ZeroVector;
		return;
	}
	if (C.FreeLook)
	{
		const FVector2D D = bRawMouse ? MouseLook : LookDeg;
		FreeHead.Yaw = FMath::Clamp(FreeHead.Yaw + D.X, -120.f, 120.f);
		FreeHead.Pitch = FMath::Clamp(FreeHead.Pitch + D.Y, -70.f, 60.f);
	}
	else if (PC)
	{
		if (bRawMouse)
		{
			AddControllerYawInput(MouseLook.X);
			AddControllerPitchInput(MouseLook.Y);
		}
		else if (!LookDeg.IsZero())
		{
			FRotator R = PC->GetControlRotation();
			R.Yaw += LookDeg.X;
			R.Pitch = FMath::ClampAngle(R.Pitch + LookDeg.Y, -85.f, 85.f);
			PC->SetControlRotation(R);
		}
	}
	MouseLook = FVector2D::ZeroVector;
}

// ------------------------------------------------------------------ the frame

void ABirdPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	Age += Dt;
	Time += Dt;
	Tracker->Poll();
	FBirdControls C;
	ReadControls(Dt, C);

	if (Mode == EBirdMode::Arriving)
	{
		TransT += Dt;
		if (TransT >= TransDur)
		{
			SkipArrival();
		}
	}
	else if (Mode == EBirdMode::Leaving)
	{
		TransT += Dt;
		if (TransT >= TransDur && OnLeftCb)
		{
			TFunction<void()> Cb = MoveTemp(OnLeftCb);
			OnLeftCb = nullptr;
			Cb();
			return;
		}
	}
	else
	{
		if (C.Return && OnReturnRequested)
		{
			ReturnHeld += Dt;
			if (ReturnHeld >= ReturnHoldSeconds)
			{
				ReturnHeld = 0.f;
				OnReturnRequested();
			}
		}
		else
		{
			ReturnHeld = FMath::Max(0.f, ReturnHeld - 2.f * Dt);
		}
		Stun = FMath::Max(0.f, Stun - Dt);
		if (C.Peck && (Mode == EBirdMode::Ground || Mode == EBirdMode::Swimming)) PeckT = 0.35f;
		if (C.Call)
		{
			PlayCall();
			if (OnCall) OnCall();
		}
		if (bAutoDescend)
		{
			// the hop down from the palm plays by itself
			C = FBirdControls();
			C.Shift = TakeoffAssist <= 0.f;
			if (Mode != EBirdMode::Air)
			{
				bAutoDescend = false;
			}
		}
		if (Fate != EBirdFate::None)
		{
			// knocked out or caught: a moment's tumble, then the subsystem takes over
			FateT -= Dt;
			C = FBirdControls();
			if (FateT <= 0.f && OnFate)
			{
				TFunction<void(EBirdFate, const FVector&)> Cb = MoveTemp(OnFate);
				OnFate = nullptr;
				Cb(Fate, GetActorLocation());
			}
		}
		if (Mode == EBirdMode::Air)
		{
			StepAir(C, Dt);
		}
		else if (Mode == EBirdMode::Ground)
		{
			StepGround(C, Dt);
			// standing, the wings rest fast
			Flight->Energy = FMath::Min(1.0, Flight->Energy + Dt / FMath::Clamp(Params->Stamina * 0.12, 6.0, 30.0));
		}
		else if (Mode == EBirdMode::Swimming)
		{
			StepSwim(C, Dt);
			Flight->Energy = FMath::Min(1.0, Flight->Energy + Dt / FMath::Clamp(Params->Stamina * 0.15, 6.0, 40.0));
		}
		CheckDanger(Dt);
		if (Mode == EBirdMode::Air && Flight->Energy < 0.12 && NoteT <= 0.f)
		{
			ShowNote(TEXT("bird.note.tired"), 3.f);
		}
	}
	NoteT = FMath::Max(0.f, NoteT - Dt);
	// the beats heard, the air heard and seen
	{
		const double PhaseNow = FMath::Max(Flight->PhaseL, Flight->PhaseR);
		if (PhaseNow > 0.0 && PhaseNow < PrevPhase - 0.3 || (PrevPhase == 0.0 && PhaseNow > 0.0))
		{
			if (Sound) Sound->Beat(0.2f + 0.8f * FMath::Min(1.f, (float)Flight->Energy / 0.15f), TakeoffAssist > 0.3f);
		}
		PrevPhase = PhaseNow;
		const bool bInside = Mode != EBirdMode::Leaving && Mode != EBirdMode::Arriving;
		const FVector AirVel = LastAir + FVector(0, 0, LastUpdraft);
		const float Rel = Mode == EBirdMode::Air ? GetAirspeed() : (Mode == EBirdMode::Leaving ? 0.f : (float)LastAir.Size() * 0.5f);
		const float Fade = bInside ? 1.f : FMath::Clamp(1.f - TransT / FMath::Max(TransDur, 0.1f), 0.f, 1.f);
		if (Sound) Sound->Tick(Dt, Rel, 1.f + 0.4f * FMath::Sin(Time * 0.7f), Fade);
		if (WindVoice)
		{
			const ULyudWindSubsystem* W = ULyudWindSubsystem::Get(this);
			const float Gusty = W ? W->GetGustiness() : 0.25f;
			const float YawTo = LastAir.SizeSquared2D() > 0.01f ? FRotator::NormalizeAxis((-LastAir).Rotation().Yaw - Head->GetComponentRotation().Yaw) : 0.f;
			WindVoice->Gain = Fade;
			WindVoice->SetWind(LastAir.Size(), Gusty, YawTo, Rel);
		}
		if (WindStreaks && Controller)
		{
			WindStreaks->UpdateStreaks(Head->GetComponentLocation(), AirVel, bInside && Mode != EBirdMode::Ground ? 1.f : 0.35f, Dt);
		}
	}
	bPrevL = C.Left;
	bPrevR = C.Right;
	bPrevSpace = C.Space;
	Ruffle = FMath::Max(0.f, Ruffle - Dt * 0.8f);
	UpdateBody(Dt);
	UpdateHead(C, Dt);
	UpdateOutsideBody(Dt);
}

void ABirdPawn::StepAir(const FBirdControls& InC, float Dt)
{
	FBirdControls C = InC;
	if (Stun > 0.f)
	{
		// knocked: the wings flail, no control for a moment
		C.Left = C.Right = C.Space = C.Shift = C.Ctrl = false;
	}
	const BirdFlight::FParams& P = *Params;
	BirdFlight::FInput In;

	// two buttons, two wings: pressed together within PairWindow they beat together
	const bool PressL = C.Left && !bPrevL, PressR = C.Right && !bPrevR;
	if (PressL && PressR) { In.ClickL = In.ClickR = true; PendingL = PendingR = -1; }
	else if (PressL) { if (PendingR >= 0) { In.ClickL = In.ClickR = true; PendingR = -1; } else PendingL = Time; }
	else if (PressR) { if (PendingL >= 0) { In.ClickL = In.ClickR = true; PendingL = -1; } else PendingR = Time; }
	// alone after the window: one wing, an oar stroke
	if (PendingL >= 0 && Time - PendingL > PairWindow) { In.ClickL = true; PendingL = -1; }
	if (PendingR >= 0 && Time - PendingR > PairWindow) { In.ClickR = true; PendingR = -1; }
	HeldL = C.Left ? HeldL + Dt : 0.f;
	HeldR = C.Right ? HeldR + Dt : 0.f;
	TuckL = (HeldL > TuckAfter || C.Ctrl) ? 1.f : 0.f;
	TuckR = (HeldR > TuckAfter || C.Ctrl) ? 1.f : 0.f;

	// Space: the species' own rhythm; a take-off: a moment of steady beats
	if (C.Space)
	{
		double AutoTuck = 0.0;
		if (BirdFlight::AutoFlap(P, Dt, AutoClock, AutoTuck))
		{
			In.ClickL = In.ClickR = true;
		}
		TuckL = FMath::Max(TuckL, (float)AutoTuck);
		TuckR = FMath::Max(TuckR, (float)AutoTuck);
	}
	else
	{
		AutoClock = 0.0;
	}
	if (TakeoffAssist > 0.f)
	{
		TakeoffAssist -= Dt;
		AssistClock -= Dt;
		if (AssistClock <= 0.f)
		{
			In.ClickL = In.ClickR = true;
			AssistClock += P.ClickTime();
		}
	}
	In.TuckL = TuckL;
	In.TuckR = TuckR;
	Flare = FMath::FInterpTo(Flare, C.Shift ? 1.f : 0.f, Dt, 6.f);
	In.Flare = Flare;
	In.Roll = C.Roll;
	if (CVarHeadSteer.GetValueOnGameThread() && Tracker->IsTracking())
	{
		In.Roll = FMath::Clamp(In.Roll + TrackHead.Roll / 25.f * TrackWeight, -1.f, 1.f);
	}

	// the gaze is the course to follow; a flare near somewhere to stand steers onto it
	const FRotator Aim = Controller ? Controller->GetControlRotation() : BodyRot;
	In.AimYaw = FMath::DegreesToRadians(Aim.Yaw);
	In.AimPitch = FMath::DegreesToRadians(FRotator::NormalizeAxis(Aim.Pitch));
	FindLandingSpot(Flare);
	if (bLandingSpot)
	{
		const FVector To = LandingSpot + FVector(0, 0, Body->StandHeight()) - GetActorLocation();
		const FRotator Want = To.Rotation();
		const float W = FMath::Clamp(Flare * 1.2f, 0.f, 0.85f);
		In.AimYaw = BirdFlight::Wrap(In.AimYaw + BirdFlight::Wrap(FMath::DegreesToRadians(Want.Yaw) - In.AimYaw) * W);
		In.AimPitch = FMath::Lerp((float)In.AimPitch, FMath::DegreesToRadians(Want.Pitch), W);
	}

	const FBirdAir::FSample A = Air->Sample(GetActorLocation(), Dt);
	BirdFlight::FAir Wind;
	Wind.Wind = BirdFlight::V3(A.Wind.X, A.Wind.Y, 0);
	Wind.Updraft = A.Updraft;
	LastAir = A.Wind;
	LastUpdraft = A.Updraft;

	// fixed small steps, whatever the frame rate
	const BirdFlight::FState Before = *Flight;
	const int32 N = FMath::Clamp(FMath::CeilToInt(Dt / (1.f / 120.f)), 1, 12);
	for (int32 K = 0; K < N; K++)
	{
		BirdFlight::Step(P, In, Wind, *Flight, Dt / N);
		In.ClickL = In.ClickR = false;
	}
	if (CVarFreeze.GetValueOnGameThread())
	{
		// posing for pictures: the wings and tail move, the bird stays where it is
		Flight->Pos = Before.Pos;
		Flight->Vel = Before.Vel;
		Flight->Ground = BirdFlight::V3();
		Flight->Bank = Before.Bank;
		return;
	}

	// the edges of the map and the sky turn the bird back with a headwind
	const FBox& Box = Air->Bounds();
	const FVector Loc = GetActorLocation();
	const float Margin = 5000.f;
	FVector Push = FVector::ZeroVector;
	Push.X = FMath::Max(0.0, Box.Min.X + Margin - Loc.X) - FMath::Max(0.0, Loc.X - (Box.Max.X - Margin));
	Push.Y = FMath::Max(0.0, Box.Min.Y + Margin - Loc.Y) - FMath::Max(0.0, Loc.Y - (Box.Max.Y - Margin));
	Push.Z = -FMath::Max(0.0, Loc.Z - Box.Max.Z);
	if (!Push.IsNearlyZero())
	{
		const FVector Acc = Push.GetClampedToMaxSize(Margin) / Margin * 6.f;
		Flight->Vel += BirdFlight::V3(Acc.X, Acc.Y, Acc.Z) * Dt;
	}

	// move with the collision sphere; land on what is under or ahead, bounce off the rest
	FHitResult Hit;
	const FVector Delta = ToCm(Flight->Ground) * Dt;
	SetActorLocation(Loc + Delta, true, &Hit, ETeleportType::None);
	if (Hit.bBlockingHit)
	{
		const FVector Nrm = Hit.ImpactNormal;
		const FVector V = ToCm(Flight->Vel) / 100.f;
		const float Into = -FVector::DotProduct(V, Nrm);
		const float Speed = V.Size();
		if (Hit.bStartPenetrating)
		{
			SetActorLocation(GetActorLocation() + Hit.Normal * (Hit.PenetrationDepth + 0.5f));
		}
		else if (Nrm.Z > 0.6f && TakeoffAssist <= 0.f && Flight->Vel.Z < 0.3 && ((Speed < P.Stall * 1.9f && Into < 4.5f) || Flare > 0.5f))
		{
			Land(Hit.ImpactPoint, Nrm);
			return;
		}
		else if (Into > FMath::Max(8.f, (float)P.Cruise * 0.75f) && Fate == EBirdFate::None)
		{
			// too fast into something: knocked out of the sky
			FVector Out = V - Nrm * (1.4f * FVector::DotProduct(V, Nrm));
			Flight->Vel = ToM(Out * 30.f);
			Stun = 2.f;
			Ruffle = 1.f;
			Jolt = 1.f;
			SetFate(EBirdFate::Crash);
		}
		else if (Into > 0.f)
		{
			// a knock: bounce off, lose most of the speed, a moment's daze
			FVector Out = V - Nrm * (1.4f * FVector::DotProduct(V, Nrm));
			Out *= 0.45f;
			Flight->Vel = ToM(Out * 100.f);
			Stun = FMath::Clamp(Into / 8.f, 0.25f, 0.8f);
			Ruffle = 1.f;
			Jolt = FMath::Clamp(Into / 6.f, 0.3f, 1.f);
		}
	}
	// the sea: water birds settle on it (not while pattering off it); a land bird that falls in is done
	{
		const FVector L2 = GetActorLocation();
		const float Surface = WaterZ(L2);
		if (L2.Z < Surface + Collision->GetScaledSphereRadius() && IsWaterBelow(L2))
		{
			if (bSwims)
			{
				if (TakeoffAssist > 0.f || BirdFlight::Speed(*Flight) > P.Stall * 2.2f)
				{
					// skimming: the water holds it up
					SetActorLocation(FVector(L2.X, L2.Y, Surface + Collision->GetScaledSphereRadius()));
					Flight->Vel.Z = FMath::Max(Flight->Vel.Z, 0.0);
				}
				else
				{
					EnterSwim(L2);
					return;
				}
			}
			else if (Fate == EBirdFate::None)
			{
				SetActorLocation(FVector(L2.X, L2.Y, Surface));
				Flight->Vel = BirdFlight::V3();
				Ruffle = 1.f;
				SetFate(EBirdFate::Drowned);
				return;
			}
		}
	}
	// held against the ground (a slope, a kerb) with no way on: it stands there
	const bool bStuck = FVector::DistSquared(GetActorLocation(), Loc) < FMath::Square(20.f * Dt) && TakeoffAssist <= 0.f;
	StuckTime = bStuck ? StuckTime + Dt : 0.f;
	if (StuckTime > 0.4f)
	{
		FHitResult Ground;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdStuck), false, this);
	IgnoreWater(Q);
		if (GetWorld()->LineTraceSingleByChannel(Ground, GetActorLocation(), GetActorLocation() - FVector(0, 0, Body->StandHeight() + 20.f), ECC_Visibility, Q))
		{
			StuckTime = 0.f;
			Land(Ground.ImpactPoint, Ground.ImpactNormal);
			return;
		}
	}
	Flight->Pos = ToM(GetActorLocation());
}

void ABirdPawn::FindLandingSpot(float InFlare)
{
	bLandingSpot = false;
	if (InFlare < 0.3f || !GetWorld())
	{
		return;
	}
	const FVector Start = GetActorLocation();
	const FVector V = ToCm(Flight->Ground);
	const float Speed = V.Size();
	const FVector Dir = Speed > 30.f ? V / Speed : BodyRot.Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdLanding), false, this);
	IgnoreWater(Q);
	FHitResult Hit;
	// along the path, then a little below it
	const float Reach = FMath::Clamp(Speed * 1.2f, 300.f, 1500.f);
	for (const FVector& D : { Dir, (Dir + FVector(0, 0, -0.6f)).GetSafeNormal(), FVector(0, 0, -1) })
	{
		if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + D * Reach, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Collision->GetScaledSphereRadius()), Q)
			&& Hit.ImpactNormal.Z > 0.6f)
		{
			LandingSpot = Hit.ImpactPoint;
			bLandingSpot = true;
			return;
		}
	}
}

void ABirdPawn::Land(const FVector& Point, const FVector& Normal)
{
	Mode = EBirdMode::Ground;
	Feet = Point;
	bAnchored = false;
	bHopping = false;
	HopVel = FVector::ZeroVector;
	Flight->Vel = BirdFlight::V3();
	Flight->BurstL = Flight->BurstR = 0;
	Flare = 0.f;
	bLandingSpot = false;
	BodyRot.Pitch = BodyRot.Roll = 0.f;
	SetActorLocation(Feet + FVector(0, 0, Body->StandHeight()));
	Jolt = FMath::Max(Jolt, 0.25f);
}

void ABirdPawn::TakeOff()
{
	const FRotator Aim = Controller ? Controller->GetControlRotation() : BodyRot;
	const FVector Fwd = FRotator(0.f, Aim.Yaw, 0.f).Vector();
	Mode = EBirdMode::Air;
	bAnchored = false;
	bHopping = false;
	// a jump with the legs, then beats
	Flight->Vel = ToM((Fwd * 0.9f + FVector(0, 0, 1.8f)) * 100.f);
	Flight->Pos = ToM(GetActorLocation() + FVector(0, 0, 1.f));
	SetActorLocation(GetActorLocation() + FVector(0, 0, 1.f));
	TakeoffAssist = 0.6f;
	AssistClock = 0.f;
	AutoClock = 0.0;
}

void ABirdPawn::StepGround(const FBirdControls& C, float Dt)
{
	const bool PressL = C.Left && !bPrevL, PressR = C.Right && !bPrevR;
	if (PressL || PressR || (C.Space && !bPrevSpace))
	{
		TakeOff();
		return;
	}
	UWorld* W = GetWorld();
	const FRotator Aim = Controller ? Controller->GetControlRotation() : BodyRot;
	const FVector Fwd = FRotator(0.f, Aim.Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, Aim.Yaw + 90.f, 0.f).Vector();
	FVector Move = Fwd * C.Move.Y + Right * C.Move.X;
	Move = Move.GetClampedToMaxSize(1.f);
	const FBirdSpecies* Row = UBirdSpeciesLibrary::Find(Species);
	const float Stride = FMath::Max(Row ? Row->StrideCm : 5.f, 3.f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdGround), false, this);
	IgnoreWater(Q);

	if (!Move.IsNearlyZero())
	{
		bAnchored = false;
		// the body faces the way it goes
		BodyRot.Yaw = FMath::FixedTurn(BodyRot.Yaw, Move.Rotation().Yaw, 600.f * Dt);
		if (Params->Hops)
		{
			if (!bHopping)
			{
				HopVel = Move * (Stride * 2.f * 3.2f) + FVector(0, 0, 70.f + Stride * 4.f);
				bHopping = true;
			}
		}
		else
		{
			const FVector Step = Move * Stride * 5.f * Dt;
			FHitResult Block;
			if (!W->SweepSingleByChannel(Block, GetActorLocation(), GetActorLocation() + Step, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Collision->GetScaledSphereRadius() * 0.8f), Q))
			{
				Feet += Step;
			}
			StepPhase += Dt * 5.f;
		}
	}
	else if (FMath::Abs(FRotator::NormalizeAxis(Aim.Yaw - BodyRot.Yaw)) > 110.f)
	{
		// looking right round: the body turns after the head with a little hop
		BodyRot.Yaw = FMath::FixedTurn(BodyRot.Yaw, Aim.Yaw, 500.f * Dt);
	}
	if (bHopping)
	{
		const FVector Next = Feet + HopVel * Dt;
		FHitResult Block;
		const FVector C0 = Feet + FVector(0, 0, Body->StandHeight());
		if (W->SweepSingleByChannel(Block, C0, Next + FVector(0, 0, Body->StandHeight()), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Collision->GetScaledSphereRadius() * 0.8f), Q) && Block.ImpactNormal.Z < 0.6f)
		{
			HopVel.X = HopVel.Y = 0.f;
		}
		else
		{
			Feet = Next;
		}
		HopVel.Z -= 980.f * Dt;
	}
	if (!bAnchored)
	{
		// stay on whatever is underfoot; off an edge the bird drops into the air with its wings out
		FHitResult Ground;
		const FVector Top = Feet + FVector(0, 0, Body->StandHeight());
		const bool bHit = W->LineTraceSingleByChannel(Ground, Top, Feet - FVector(0, 0, bHopping ? 2.f : 25.f), ECC_Visibility, Q);
		if (bHit && Ground.ImpactNormal.Z > 0.5f)
		{
			if (!bHopping || (HopVel.Z < 0.f && Feet.Z <= Ground.ImpactPoint.Z + 0.5f))
			{
				Feet.Z = Ground.ImpactPoint.Z;
				bHopping = false;
			}
		}
		else if (!bHopping && Feet.Z > FBirdAir::SeaZ + 1.f)
		{
			Mode = EBirdMode::Air;
			Flight->Vel = ToM(Move * 50.f);
			Flight->Pos = ToM(GetActorLocation());
			return;
		}
		if (Feet.Z < FBirdAir::SeaZ)
		{
			Feet.Z = FBirdAir::SeaZ;
			bHopping = false;
		}
	}
	SetActorLocation(Feet + FVector(0, 0, Body->StandHeight()));
	Flight->Pos = ToM(GetActorLocation());
}

// ------------------------------------------------------------------ body and head

void ABirdPawn::UpdateBody(float Dt)
{
	const BirdFlight::FState& S = *Flight;
	FRotator Want = BodyRot;
	if (Mode == EBirdMode::Air)
	{
		const double V = BirdFlight::Speed(S);
		if (V > 1.0)
		{
			Want.Yaw = FMath::RadiansToDegrees(FMath::Atan2(S.Vel.Y, S.Vel.X));
			const float Gamma = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(S.Vel.Z / V, -1.0, 1.0)));
			// slower flies more nose-up; a flare stands the body up into the air
			const float Aoa = FMath::Lerp(18.f, 3.f, FMath::Clamp((float)((V - Params->Stall) / (Params->Cruise - Params->Stall)), 0.f, 1.f)) + Flare * 35.f;
			Want.Pitch = FMath::Clamp(Gamma + Aoa, -80.f, 80.f);
		}
		Want.Roll = FMath::RadiansToDegrees(S.Bank);
	}
	else
	{
		Want.Pitch = 6.f;
		Want.Roll = 0.f;
	}
	BodyRot = Dt > 0.f ? FMath::RInterpTo(BodyRot, Want, Dt, Mode == EBirdMode::Air ? 9.f : 14.f) : Want;
	BodyRot.Yaw = Mode == EBirdMode::Air && Dt > 0.f ? FMath::FixedTurn(BodyRot.Yaw, Want.Yaw, 720.f * Dt) : BodyRot.Yaw;
	Body->SetWorldRotation(BodyRot);

	const bool bGround = Mode == EBirdMode::Ground || Mode == EBirdMode::Swimming || (Mode == EBirdMode::Arriving && bAnchored);
	Fold = Dt > 0.f ? FMath::FInterpTo(Fold, bGround ? 1.f : 0.f, Dt, bGround ? 5.f : 9.f) : (bGround ? 1.f : 0.f);
	const bool bLegs = bGround || Flare > 0.4f || TakeoffAssist > 0.3f;
	LegsDown = Dt > 0.f ? FMath::FInterpTo(LegsDown, bLegs ? 1.f : 0.f, Dt, 7.f) : (bLegs ? 1.f : 0.f);

	FBirdBodyPose Pose;
	Pose.SpreadL = S.SpreadL; Pose.SpreadR = S.SpreadR;
	Pose.PhaseL = S.PhaseL; Pose.PhaseR = S.PhaseR;
	Pose.BeatL = S.BurstL > 0; Pose.BeatR = S.BurstR > 0;
	Pose.Flare = Flare;
	Pose.Load = S.Load;
	Pose.Speed = Mode == EBirdMode::Air ? BirdFlight::Speed(S) : 0.f;
	Pose.Fold = Fold;
	Pose.Legs = LegsDown;
	// the tail fans to brake, to turn and when slow
	const float Slow = Mode == EBirdMode::Air ? 1.f - FMath::Clamp((float)((BirdFlight::Speed(S) - Params->Stall) / Params->Stall), 0.f, 1.f) : 0.f;
	Pose.TailFan = FMath::Clamp(FMath::Max3(Flare, FMath::Abs((float)S.Bank) * 0.5f, Slow * 0.7f), 0.f, 1.f) * (1.f - Fold);
	Pose.Ruffle = Ruffle;
	Body->ApplyPose(Pose, Time);
}

float ABirdPawn::TrackedBlend() const
{
	return TrackWeight;
}

void ABirdPawn::UpdateHead(const FBirdControls& C, float Dt)
{
	// AirPods: the neck turns the head on top of the mouse; eased in and out, so losing them never jerks
	const bool bTracked = Tracker->IsTracking();
	TrackWeight = Dt > 0.f ? FMath::FInterpConstantTo(TrackWeight, bTracked ? 1.f : 0.f, Dt, 2.f) : (bTracked ? 1.f : 0.f);
	if (bTracked)
	{
		const FRotator Raw = Tracker->Head();
		FRotator Rel = (Raw - TrackZero).GetNormalized();
		// CoreMotion's yaw drifts: while the head is near straight ahead, the neck's centre follows it slowly
		// (a held look over the shoulder stays where it is)
		if (FMath::Abs(Rel.Yaw) < 25.f && FMath::Abs(Rel.Pitch) < 20.f)
		{
			TrackZero = (TrackZero + (Raw - TrackZero).GetNormalized() * FMath::Min(1.f, Dt / 20.f)).GetNormalized();
		}
		Rel.Yaw = FMath::Clamp(Rel.Yaw, -110.f, 110.f);
		Rel.Pitch = FMath::Clamp(Rel.Pitch, -60.f, 50.f);
		Rel.Roll = FMath::Clamp(Rel.Roll, -35.f, 35.f);
		TrackHead = Dt > 0.f ? FMath::RInterpTo(TrackHead, Rel, Dt, 25.f) : Rel;
	}
	if (!C.FreeLook && Dt > 0.f)
	{
		FreeHead = FMath::RInterpTo(FreeHead, FRotator::ZeroRotator, Dt, 5.f);
	}
	PeckT = FMath::Max(0.f, PeckT - Dt);
	Jolt = FMath::Max(0.f, Jolt - Dt * 2.5f);

	const FRotator Aim = Controller ? Controller->GetControlRotation() : BodyRot;
	FRotator Gaze = Aim;
	Gaze.Yaw += FreeHead.Yaw + TrackHead.Yaw * TrackWeight;
	Gaze.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch) + FreeHead.Pitch + TrackHead.Pitch * TrackWeight, -88.f, 88.f);
	// a bird holds its head level: only a quarter of the body's roll reaches the eye, at most 8 degrees
	// (and none while slow: at a take-off the body swings about, the eye stays level)
	const float RollShare = Mode == EBirdMode::Air ? 0.25f * FMath::Clamp((float)((BirdFlight::Speed(*Flight) - Params->Stall) / Params->Stall), 0.f, 1.f) : 0.f;
	Gaze.Roll = FMath::Clamp(BodyRot.Roll * RollShare, -8.f, 8.f) + TrackHead.Roll * TrackWeight;
	const float Peck = FMath::Sin(PI * FMath::Clamp(PeckT / 0.35f, 0.f, 1.f));
	Gaze.Pitch -= Peck * 55.f;
	if (Jolt > 0.f)
	{
		Gaze.Pitch += Jolt * 3.f * FMath::Sin(Time * 47.f);
		Gaze.Yaw += Jolt * 2.f * FMath::Sin(Time * 61.f + 1.f);
	}

	// the eye on the body (the neck is short); walking pigeons nod, hopping birds bob
	// the neck stretches the head up as it turns round, so a look over the shoulder sees the wing from above
	const float Turn = FMath::Clamp(FMath::Abs(FRotator::NormalizeAxis(Gaze.Yaw - BodyRot.Yaw)) / 90.f, 0.f, 1.f);
	FVector Eye = BodyRot.RotateVector(Body->EyeOffset()) + FVector(0, 0, Body->BodyRadius() * 1.4f * Turn);
	// looking down, the head draws back and up over the breast, so the bird sees its own breast and feet
	const float Down = FMath::Clamp((FRotator::NormalizeAxis(BodyRot.Pitch - Gaze.Pitch) - 30.f) / 40.f, 0.f, 1.f);
	Eye += FRotator(0.f, BodyRot.Yaw, 0.f).Vector() * (-Body->TorsoLength() * 0.5f * Down) + FVector(0, 0, Body->BodyRadius() * 0.8f * Down);
	if (Mode == EBirdMode::Ground || Mode == EBirdMode::Swimming)
	{
		const float Nod = Params->Hops ? 0.f : FMath::Sin(StepPhase * 2.f * PI) * Body->BodyRadius() * 0.35f;
		Eye += FRotator(0.f, BodyRot.Yaw, 0.f).Vector() * Nod;
		Eye += Gaze.Vector() * Peck * Body->BodyRadius() * 0.8f;
	}
	Head->SetWorldLocationAndRotation(GetActorLocation() + Eye, Gaze);
}

FMinimalViewInfo ABirdPawn::BirdView() const
{
	FMinimalViewInfo V;
	V.Location = Head->GetComponentLocation();
	V.Rotation = Head->GetComponentRotation();
	const float Speed = Mode == EBirdMode::Air ? BirdFlight::Speed(*Flight) : 0.f;
	V.FOV = CVarFov.GetValueOnGameThread() + 8.f * FMath::Clamp((float)((Speed - Params->Cruise) / FMath::Max(1.0, Params->MaxSpeed - Params->Cruise)), 0.f, 1.f);
	V.PerspectiveNearClipPlane = NearClip;
	return V;
}

FMinimalViewInfo ABirdPawn::ChaseView() const
{
	FMinimalViewInfo V = BirdView();
	const float Back = (Body->BodyRadius() * 14.f + 30.f) * CVarChaseDist.GetValueOnGameThread();
	const FRotator Look(FMath::Clamp(V.Rotation.Pitch, -40.f, 40.f) - CVarChasePitch.GetValueOnGameThread(), BodyRot.Yaw + CVarChaseYaw.GetValueOnGameThread(), 0.f);
	V.Location = GetActorLocation() - Look.Vector() * Back;
	V.Rotation = Look;
	V.FOV = 70.f;
	V.PerspectiveNearClipPlane = 2.f;
	return V;
}

void ABirdPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	const bool bChase = CVarChase.GetValueOnGameThread() != 0;
	const FMinimalViewInfo Bird = bChase ? ChaseView() : BirdView();
	OutResult = Bird;
	float Inside = bChase ? 0.f : 1.f;
	if (Mode == EBirdMode::Arriving || Mode == EBirdMode::Leaving)
	{
		const bool bIn = Mode == EBirdMode::Arriving;
		const FMinimalViewInfo& A = TransFrom;
		const FMinimalViewInfo& B = bIn ? Bird : TransTo;
		const float T = FMath::Clamp(TransT / TransDur, 0.f, 1.f);
		const float E = Ease(T);
		// an arc: up over the ground in proportion to the distance, so a long way back never cuts through a hill
		const float Dist = FVector::Dist(A.Location, B.Location);
		const FVector Mid = (A.Location + B.Location) * 0.5f + FVector(0, 0, FMath::Min(Dist * 0.22f, 4000.f) + 10.f);
		const FVector P = FMath::Lerp(FMath::Lerp(A.Location, Mid, E), FMath::Lerp(Mid, B.Location, E), E);
		// the view turns early and settles: the eye finds the target first
		const float R = Ease(FMath::Clamp(T * 1.35f, 0.f, 1.f));
		OutResult.Location = P;
		// yaw and pitch turn separately, so the horizon stays level all the way (a quaternion slerp between
		// two views rolls through a Dutch tilt)
		OutResult.Rotation = FRotator(FMath::Lerp(FRotator::NormalizeAxis(A.Rotation.Pitch), FRotator::NormalizeAxis(B.Rotation.Pitch), R),
		                              A.Rotation.Yaw + FRotator::NormalizeAxis(B.Rotation.Yaw - A.Rotation.Yaw) * R,
		                              FMath::Lerp(FRotator::NormalizeAxis(A.Rotation.Roll), FRotator::NormalizeAxis(B.Rotation.Roll), R));
		OutResult.FOV = FMath::Lerp(A.FOV, B.FOV, E);
		OutResult.PerspectiveNearClipPlane = FMath::Lerp(bIn ? 4.f : NearClip, bIn ? NearClip : 4.f, E);
		const float ToEye = FVector::Dist(P, Head->GetComponentLocation());
		Inside = ToEye < Body->BodyRadius() * 2.5f ? 1.f : 0.f;
	}
	// from inside, the close-up body (beak, wings, breast); from outside, the species' own bird
	bInsideView = Inside > 0.5f;
	Body->SetShown(bInsideView || !OutsideBody, !bInsideView);
	if (OutsideBody)
	{
		OutsideBody->SetActorHiddenInGame(bInsideView);
	}
	if (Inside > 0.5f)
	{
		// the bird's own body with a far wider view than the screen's: the wings beside it reach into the
		// screen's edges; turning the head to look at a wing brings back its true shape
		const FRotator Gz = Head->GetComponentRotation();
		const float LookDown = FMath::Clamp((FRotator::NormalizeAxis(BodyRot.Pitch - Gz.Pitch) - 30.f) / 30.f, 0.f, 1.f);
		const float Turn = FMath::Max(FMath::Clamp(FMath::Abs(FRotator::NormalizeAxis(Gz.Yaw - BodyRot.Yaw)) / 60.f, 0.f, 1.f), LookDown);
		const float WideFov = FMath::Lerp(CVarWingFov.GetValueOnGameThread(), OutResult.FOV, Turn);
		Body->SetViewSqueeze(OutResult.Location, OutResult.Rotation,
			FMath::Tan(FMath::DegreesToRadians(OutResult.FOV * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(FMath::Min(WideFov, 170.f) * 0.5f)));
		Body->SetViewForward((1.f - Turn) * (Mode == EBirdMode::Air ? 1.f : 0.f));
	}
	else
	{
		Body->SetViewForward(0.f);
		Body->SetViewSqueeze(OutResult.Location, OutResult.Rotation, 1.f);
	}
	// no motion blur: the wings and the ground smear at a bird's speeds
	OutResult.PostProcessSettings.bOverride_MotionBlurAmount = true;
	OutResult.PostProcessSettings.MotionBlurAmount = 0.f;
	OutResult.PostProcessBlendWeight = 1.f;
}

void ABirdPawn::UpdateOutsideBody(float Dt)
{
	ABirdActor* A = OutsideBody.Get();
	if (!A)
	{
		return;
	}
	// the actor's origin is between its feet: below the body's centre along its up
	const FQuat Q = BodyRot.Quaternion();
	A->SetActorLocationAndRotation(GetActorLocation() - Q.GetUpVector() * Body->StandHeight(), Q);
	A->SetVelocity(GetVelocityCm());
	A->SetLookTarget(Head->GetComponentLocation() + Head->GetForwardVector() * 500.f, true);
	const BirdFlight::FState& S = *Flight;
	EBirdState Want = A->GetState();
	if (Mode == EBirdMode::Air)
	{
		const bool bBeat = S.BurstL > 0 || S.BurstR > 0;
		Want = bBeat ? EBirdState::Fly : EBirdState::Glide;
		A->SetFlightInput(bBeat ? 1.f : 0.f, 1.f - 0.5f * (float)(S.SpreadL + S.SpreadR), FMath::Clamp((float)S.Bank / 1.2f, -1.f, 1.f),
		                  FMath::Clamp(-BodyRot.Pitch / 60.f + Flare, -1.f, 1.f), true);
	}
	else if (Mode == EBirdMode::Swimming)
	{
		A->SetFlightInput(0.f, 1.f, 0.f, 0.f, false);
		Want = PeckT > 0.3f ? EBirdState::Peck : EBirdState::Perch;
	}
	else if (Mode == EBirdMode::Ground)
	{
		A->SetFlightInput(0.f, 1.f, 0.f, 0.f, false);
		if (PeckT > 0.3f) Want = EBirdState::Peck;
		else if (bHopping || !GetVelocityCm().IsNearlyZero()) Want = Params->Hops ? EBirdState::Hop : EBirdState::Walk;
		else if (A->GetState() != EBirdState::Peck) Want = EBirdState::Perch;
	}
	if (Want != A->GetState())
	{
		A->SetState(Want);
	}
}

void ABirdPawn::IgnoreWater(FCollisionQueryParams& Q) const
{
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Q.AddIgnoredActor(Sea);
		if (AActor* Vol = Sea->GetWaterVolume()) Q.AddIgnoredActor(Vol);
	}
}

float ABirdPawn::GetEnergy() const
{
	return (float)Flight->Energy;
}

float ABirdPawn::WaterZ(const FVector& At) const
{
	if (const ALyudSea* Sea = ALyudSea::Get(this))
	{
		return Sea->SurfaceZ(At);
	}
	return FBirdAir::SeaZ;
}

bool ABirdPawn::IsWaterBelow(const FVector& At) const
{
	// open water: the ground under it lies below the sea's surface
	return Air->SolidZ(FVector2D(At)) < WaterZ(At) - 15.f;
}

void ABirdPawn::EnterSwim(const FVector& At)
{
	Mode = EBirdMode::Swimming;
	SwimVel = ToCm(Flight->Vel) * FVector(0.25f, 0.25f, 0.f);
	Flight->Vel = BirdFlight::V3();
	Flight->BurstL = Flight->BurstR = 0;
	Flare = 0.f;
	bLandingSpot = false;
	BodyRot.Pitch = BodyRot.Roll = 0.f;
	Feet = FVector(At.X, At.Y, WaterZ(At));
	Jolt = FMath::Max(Jolt, 0.3f);
	Ruffle = FMath::Max(Ruffle, 0.4f);
	ShowNote(TEXT("bird.note.swimming"), 4.f);
}

void ABirdPawn::StepSwim(const FBirdControls& C, float Dt)
{
	const bool PressL = C.Left && !bPrevL, PressR = C.Right && !bPrevR;
	if (PressL || PressR || (C.Space && !bPrevSpace))
	{
		// off the water: a swan patters along it for seconds before it lifts, a gull jumps off
		TakeOff();
		TakeoffAssist = 0.6f + RunUp;
		const FVector Fwd = FRotator(0.f, BodyRot.Yaw, 0.f).Vector();
		Flight->Vel = ToM((Fwd * (RunUp > 0.f ? 1.5f : 1.f) + FVector(0, 0, RunUp > 0.f ? 0.3f : 1.6f)) * 100.f);
		return;
	}
	const FRotator Aim = Controller ? Controller->GetControlRotation() : BodyRot;
	const FVector Fwd = FRotator(0.f, Aim.Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, Aim.Yaw + 90.f, 0.f).Vector();
	const FVector Move = (Fwd * C.Move.Y + Right * C.Move.X).GetClampedToMaxSize(1.f);
	// paddling: about 0.6 m/s for a gull, 1 m/s for a swan
	const float Paddle = FMath::Clamp((float)Params->Cruise * 5.f, 35.f, 110.f);
	SwimVel = FMath::VInterpTo(SwimVel, Move * Paddle + LastAir * 4.f, Dt, 2.f);
	if (!Move.IsNearlyZero())
	{
		BodyRot.Yaw = FMath::FixedTurn(BodyRot.Yaw, Move.Rotation().Yaw, 200.f * Dt);
		StepPhase += Dt * 2.f;
	}
	Feet += SwimVel * Dt;
	// riding the waves
	Feet.Z = WaterZ(Feet);
	// ashore: paddling into the beach it stands up and walks
	const float Ground = Air->SolidZ(FVector2D(Feet));
	if (Ground > Feet.Z - 3.f)
	{
		Land(FVector(Feet.X, Feet.Y, Ground), FVector::UpVector);
		return;
	}
	SetActorLocation(Feet + FVector(0, 0, Body->StandHeight() * 0.35f));
	Flight->Pos = ToM(GetActorLocation());
}

void ABirdPawn::SetFate(EBirdFate F)
{
	if (Fate != EBirdFate::None)
	{
		return;
	}
	Fate = F;
	FateT = F == EBirdFate::Drowned ? 0.6f : 0.9f;
	ShowNote(F == EBirdFate::Crash ? TEXT("bird.note.crash") : F == EBirdFate::Caught ? TEXT("bird.note.caught") : TEXT("bird.note.drowned"), 4.f);
	UE_LOG(LogTemp, Warning, TEXT("BIRD fate %d at %s"), (int32)F, *GetActorLocation().ToCompactString());
}

void ABirdPawn::TakePeck(const FVector& From)
{
	const FVector Away = (GetActorLocation() - From).GetSafeNormal2D();
	if (Mode == EBirdMode::Ground && !bAnchored)
	{
		Feet += Away * 6.f;
	}
	Jolt = FMath::Max(Jolt, 0.6f);
	Ruffle = 1.f;
	Flight->Energy = FMath::Max(0.0, Flight->Energy - 0.06);
}

void ABirdPawn::CheckDanger(float Dt)
{
#if LYUD_HAS_ANIMALS
	DangerT = FMath::Max(0.f, DangerT - Dt);
	DangerCheck -= Dt;
	if (DangerCheck > 0.f || Fate != EBirdFate::None)
	{
		return;
	}
	DangerCheck = 0.1f;
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	if (!Life)
	{
		return;
	}
	const FVector At = FeetLocation();
	// up in the air it's safe; on the ground, a branch-less perch low down or the water's edge, not
	const float Over = Mode == EBirdMode::Air ? At.Z - Air->GroundZ(FVector2D(At)) : 0.f;
	if (Over > 120.f)
	{
		return;
	}
	for (int32 Id : Life->FindAnimalsNear(At, 500.f))
	{
		FAnimalInfo A;
		if (!Life->GetAnimal(Id, A))
		{
			continue;
		}
		const float D = FVector::Dist2D(A.Location, At);
		const bool bHunting = A.Activity == EAnimalActivity::Stalk || A.Activity == EAnimalActivity::Crouch || A.Activity == EAnimalActivity::Pounce
			|| A.Activity == EAnimalActivity::Chase || A.Activity == EAnimalActivity::Approach || A.Activity == EAnimalActivity::Run;
		if (bHunting || D < 200.f)
		{
			DangerT = 1.f;
		}
		const float Reach = A.Kind == EAnimalKind::Cat ? 45.f : 60.f;
		if (D < Reach && (bHunting || D < Reach * 0.6f) && FMath::Abs(A.Location.Z - At.Z) < 60.f)
		{
			Stun = 2.f;
			Ruffle = 1.f;
			Jolt = 1.f;
			SetFate(EBirdFate::Caught);
			return;
		}
	}
#endif
}

void ABirdPawn::PlayCall()
{
	const FBirdSpecies* Row = UBirdSpeciesLibrary::Find(Species);
	if (!Row || Row->Calls.IsEmpty())
	{
		return;
	}
	const FName Call = Row->Calls[FMath::RandRange(0, Row->Calls.Num() - 1)];
	const FString Path = FString::Printf(TEXT("/Game/Birds/%s/Sounds/S_%s_%s.S_%s_%s"), *Species.ToString(), *Species.ToString(), *Call.ToString(), *Species.ToString(), *Call.ToString());
	if (USoundBase* CallSound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, CallSound, Head->GetComponentLocation());
	}
}

FString ABirdPawn::DebugLine() const
{
	const TCHAR* ModeName = Mode == EBirdMode::Air ? TEXT("air") : Mode == EBirdMode::Ground ? TEXT("ground") : Mode == EBirdMode::Swimming ? TEXT("swimming") : Mode == EBirdMode::Arriving ? TEXT("arriving") : TEXT("leaving");
	return FString::Printf(TEXT("%s %s  energy %.2f  air %.1f m/s  ground %.1f m/s  climb %+.1f  updraft %+.2f  wind %.1f  load %.1fg  wings %.2f/%.2f  flare %.2f  head %s %s %s"),
		*Species.ToString(), ModeName, (float)Flight->Energy, GetAirspeed(), GetVelocityCm().Size() / 100.f, GetVelocityCm().Z / 100.f, LastUpdraft, LastAir.Size(),
		Flight->Load, Flight->SpreadL, Flight->SpreadR, Flare, Tracker->IsTracking() ? TEXT("tracked") : TEXT("mouse"), *Tracker->State(), *Head->GetComponentRotation().ToCompactString());
}
