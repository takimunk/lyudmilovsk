#include "Birds/BirdFlightSubsystem.h"

#include "BirdAir.h"
#include "Birds/BirdActor.h"
#include "Birds/BirdPawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "LyudSave.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#if __has_include("Birds/BirdLife.h")
#include "Birds/BirdLife.h"
#define LYUD_HAS_BIRDLIFE 1
#else
#define LYUD_HAS_BIRDLIFE 0
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarQuitAfterLoad(TEXT("lyud.Bird.QuitAfterLoad"), 0, TEXT("1: quit two seconds after a save is loaded (the save test)."));
	const FName SaveKey(TEXT("BirdFlight"));
	const FName HeadBone(TEXT("head"));
	/** The hero's view (AHeroCharacter::Fov). */
	constexpr float HeroFov = 90.f;
	/** The portal face's Y in Unreal (tools/world_map.py frame, as ULyudBenchmark's Map). */
	constexpr double PortalFaceY = 790.0;

	APlayerController* FirstPC(UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

// ------------------------------------------------------------------ scripted flights

/** A scripted flight: steps that set the controls, each until it says it is done or its time is up. */
struct UBirdFlightSubsystem::FScript
{
	struct FCtx
	{
		UBirdFlightSubsystem* Sys = nullptr;
		ABirdPawn* Bird = nullptr;
		APlayerController* PC = nullptr;
		float T = 0.f;           // in this step
		float Dt = 0.f;
		FBirdControls C;
		bool bAim = false;
		FRotator Aim;            // where the "mouse" should turn the gaze (absolute)
		float AimRate = 140.f;   // deg/s, a hand on a mouse
	};
	struct FStep
	{
		float Max = 1.f;
		TFunction<bool(FCtx&)> Run;
	};
	FString Name;
	TArray<FStep> Steps;
	int32 Index = 0;
	float StepT = 0.f;
	float Clock = 0.f;

	void Add(float Max, TFunction<bool(FCtx&)> Run) { Steps.Add({ Max, MoveTemp(Run) }); }

	static TSharedPtr<FScript> Make(const FString& Name);
};

namespace
{
	using FCtx = UBirdFlightSubsystem::FScript::FCtx;

	/** The hero on the terrace in front of the club's door, facing the sea (west, +X), and a bird on his hand. */
	void Start(UBirdFlightSubsystem::FScript& S, FName Species, float MapX = 0.f, float MapY = 30.f)
	{
		S.Add(0.1f, [MapX, MapY](FCtx& X) { X.Sys->PlaceHero(MapX, MapY, 0.f); return true; });
		S.Add(0.6f, [](FCtx&) { return false; });
		S.Add(0.1f, [Species](FCtx& X)
		{
			APawn* HP = X.Sys->GetHero();
			if (!HP && X.PC) HP = X.PC->GetPawn();
			const AHeroCharacter* H = Cast<AHeroCharacter>(HP);
			if (!H || !X.PC) return true;
			const FRotator Yaw(0.f, X.PC->GetControlRotation().Yaw, 0.f);
			const FVector Hand = H->EyeLocation() + Yaw.Vector() * 38.f - FVector(0, 0, 32.f);
			X.Sys->BecomeSpecies(X.PC, Species, Hand, Yaw.Yaw, FVector::ZeroVector, true);
			return true;
		});
		S.Add(UBirdFlightSubsystem::ArrivalSeconds + 0.3f, [](FCtx&) { return false; });
	}

	void CVar(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(Value);
	}

	/** Clicking both buttons in a steady rhythm (a player flapping by hand). */
	void Rhythm(FCtx& X, float Period)
	{
		const bool Down = FMath::Fmod(X.T, Period) < Period * 0.35f;
		X.C.Left = X.C.Right = Down;
	}
}

TSharedPtr<UBirdFlightSubsystem::FScript> UBirdFlightSubsystem::FScript::Make(const FString& Name)
{
	TSharedPtr<FScript> S = MakeShared<FScript>();
	S->Name = Name;
	TArray<FString> Parts;
	Name.ParseIntoArray(Parts, TEXT(":"));
	const FString Kind = Parts.IsValidIndex(0) ? Parts[0] : Name;
	const FName Species = Parts.IsValidIndex(1) ? FName(*Parts[1]) : FName(TEXT("Starling"));

	if (Kind == TEXT("showcase"))
	{
		// everything once: off the hand, out over the cliff, the wind on the cliff, a look at the wing,
		// rowing, a dive, back to the hero, landing, hopping, home
		Start(*S, Species);
		S->Add(2.6f, [](FCtx& X) { X.bAim = true; X.AimRate = 90.f; X.Aim = X.T < 1.3f ? FRotator(-8.f, -70.f, 0.f) : FRotator(-4.f, 0.f, 0.f); return false; });
		S->Add(3.2f, [](FCtx& X) { X.C.Space = true; X.bAim = true; X.Aim = FRotator(14.f, 0.f, 0.f); return false; });
		S->Add(25.f, [](FCtx& X)
		{
			// out to the cliff edge into the sea wind: flapping in the species' rhythm, then a glide
			X.bAim = true; X.Aim = FRotator(X.Bird->GetAirspeed() > 11.f ? -6.f : 3.f, 0.f, 0.f);
			X.C.Space = X.Bird->GetActorLocation().X < 6000.f || X.Bird->GetAirspeed() < 9.f;
			return X.Bird && X.Bird->GetActorLocation().X > 8600.f;
		});
		S->Add(1.6f, [](FCtx& X) { Rhythm(X, 0.32f); X.bAim = true; X.Aim = FRotator(4.f, 20.f, 0.f); return false; });
		S->Add(4.5f, [](FCtx& X) { X.bAim = true; X.AimRate = 60.f; X.Aim = FRotator(-2.f, 90.f, 0.f); return false; });
		S->Add(3.f, [](FCtx& X)
		{
			X.bAim = true; X.Aim = FRotator(-2.f, 90.f, 0.f);
			const float A = FMath::Sin(PI * FMath::Clamp(X.T / 3.f, 0.f, 1.f));
			X.Bird->SetFakeHead(FRotator(-12.f * A, -88.f * A, -6.f * A), X.T < 2.95f);
			return false;
		});
		S->Add(1.6f, [](FCtx& X) { X.C.Left = FMath::Fmod(X.T, 0.5f) < 0.12f; X.bAim = true; X.Aim = FRotator(-2.f, 110.f, 0.f); return false; });
		S->Add(1.3f, [](FCtx& X) { X.C.Left = X.C.Right = true; X.bAim = true; X.Aim = FRotator(-35.f, 150.f, 0.f); return false; });
		S->Add(2.2f, [](FCtx& X) { X.C.Space = true; X.bAim = true; X.Aim = FRotator(12.f, 170.f, 0.f); return false; });
		S->Add(20.f, [](FCtx& X)
		{
			// home: a spot two metres in front of the hero, braking from twelve metres out
			const APawn* H = X.Sys->GetHero();
			if (!H || !X.Bird) return true;
			const FVector Spot = H->GetActorLocation() + H->GetActorForwardVector() * 250.f - FVector(0, 0, 80.f);
			const FVector To = Spot - X.Bird->GetActorLocation();
			X.bAim = true; X.AimRate = 160.f;
			X.Aim = To.Rotation();
			X.Aim.Pitch = FMath::Clamp(X.Aim.Pitch, To.Size2D() > 2500.f && To.Z > -300.f ? 2.f : -30.f, 20.f);
			X.C.Space = To.Size() > 1800.f && (X.Bird->GetAirspeed() < 10.f || FMath::Fmod(X.T, 3.f) < 1.2f);
			X.C.Shift = To.Size() < 1200.f;
			return X.Bird->GetMode() == EBirdMode::Ground;
		});
		S->Add(4.f, [](FCtx& X)
		{
			X.C.Move = X.T < 1.4f ? FVector2D(0, 1) : FVector2D::ZeroVector;
			X.C.Peck = FMath::Abs(X.T - 1.9f) < X.Dt * 0.5f || FMath::Abs(X.T - 2.6f) < X.Dt * 0.5f;
			X.bAim = true; X.AimRate = 100.f;
			X.Aim = X.T < 3.f ? FRotator(-10.f, X.Bird->GetActorRotation().Yaw + (X.T < 1.4f ? 0.f : 70.f), 0.f) : FRotator(10.f, 180.f, 0.f);
			return false;
		});
		S->Add(2.f, [](FCtx& X) { X.C.Return = true; return X.Bird->GetMode() == EBirdMode::Leaving; });
		S->Add(3.5f, [](FCtx& X) { return X.Bird == nullptr; });
	}
	else if (Kind == TEXT("ridge"))
	{
		// hands off in front of the sea cliff, along it: the cliff wind should hold the bird up
		S->Add(0.1f, [](FCtx& X) { X.Sys->PlaceHero(0.f, 30.f, 0.f); return true; });
		S->Add(0.5f, [](FCtx&) { return false; });
		S->Add(0.1f, [Species](FCtx& X)
		{
			const FVector At(10500.f, PortalFaceY + 3000.f, 0.f);
			X.Sys->BecomeSpecies(X.PC, Species, At + FVector(0, 0, FBirdAir::SeaZ + 4500.f), 90.f, FVector(0, 1000.f, 0), false, nullptr, true);
			X.PC->SetControlRotation(FRotator(-3.f, 90.f, 0.f));
			return true;
		});
		S->Add(30.f, [](FCtx& X)
		{
			X.bAim = true; X.Aim = FRotator(-3.f, 90.f, 0.f);
			if (FMath::Fmod(X.T, 1.f) < X.Dt) UE_LOG(LogTemp, Warning, TEXT("BIRD ridge t=%.0f %s z=%.1f m"), X.T, *X.Bird->DebugLine(), X.Bird->GetActorLocation().Z / 100.f);
			return false;
		});
	}
	else if (Kind == TEXT("styles"))
	{
		// each flight style from behind: flapping by Space, a glide, an oar turn
		static const TCHAR* Ids[] = { TEXT("HouseSparrow"), TEXT("Goldfinch"), TEXT("GreatTit"), TEXT("Starling"), TEXT("SongThrush"), TEXT("FeralPigeon"),
		                              TEXT("Rook"), TEXT("Magpie"), TEXT("HerringGull"), TEXT("CommonSwift"), TEXT("BarnSwallow"), TEXT("Cormorant") };
		S->Add(0.1f, [](FCtx& X) { X.Sys->PlaceHero(0.f, 30.f, 0.f); CVar(TEXT("lyud.Bird.ChaseCam"), 1); return true; });
		for (const TCHAR* Id : Ids)
		{
			const FName Sp(Id);
			S->Add(0.1f, [Sp](FCtx& X)
			{
				X.Sys->ReturnToHero(true);
				X.Sys->BecomeSpecies(X.PC, Sp, FVector(2000.f, PortalFaceY + 3000.f, 2500.f), 0.f, FVector(1000.f, 0, 0), false, nullptr, true);
				X.PC->SetControlRotation(FRotator(0.f, 0.f, 0.f));
				return true;
			});
			S->Add(3.f, [](FCtx& X) { X.C.Space = true; X.bAim = true; X.Aim = FRotator(2.f, 0.f, 0.f); return false; });
			S->Add(1.5f, [](FCtx& X) { X.bAim = true; X.Aim = FRotator(-6.f, 0.f, 0.f); return false; });
			S->Add(1.2f, [](FCtx& X) { X.C.Left = FMath::Fmod(X.T, 0.6f) < 0.12f; return false; });
		}
		S->Add(0.1f, [](FCtx& X) { X.Sys->ReturnToHero(true); CVar(TEXT("lyud.Bird.ChaseCam"), 0); return true; });
	}
	else if (Kind == TEXT("pose"))
	{
		// the bird held still in the air: round it from outside, then its own wings from its eye
		S->Add(0.1f, [Species](FCtx& X)
		{
			X.Sys->ReturnToHero(true);
			X.Sys->BecomeSpecies(X.PC, Species, FVector(0.f, 0.f, 600.f), 0.f, FVector(1000.f, 0, 0), false, nullptr, true);
			X.PC->SetControlRotation(FRotator::ZeroRotator);
			CVar(TEXT("lyud.Bird.Freeze"), 1); CVar(TEXT("lyud.Bird.ChaseCam"), 1); CVar(TEXT("lyud.Bird.ChasePitch"), 12); CVar(TEXT("lyud.Bird.ChaseDist"), 0.6f);
			return true;
		});
		S->Add(4.f, [](FCtx& X) { CVar(TEXT("lyud.Bird.ChaseYaw"), X.T / 4.f * 360.f); return false; });
		S->Add(3.f, [](FCtx& X) { X.C.Space = true; CVar(TEXT("lyud.Bird.ChaseYaw"), X.T / 3.f * 180.f); return false; });
		S->Add(1.5f, [](FCtx& X) { X.C.Left = X.C.Right = true; CVar(TEXT("lyud.Bird.ChaseYaw"), 90.f); return false; });
		S->Add(1.5f, [](FCtx& X) { X.C.Shift = true; CVar(TEXT("lyud.Bird.ChaseYaw"), 60.f); return false; });
		S->Add(1.5f, [](FCtx& X) { X.C.Left = X.T > 0.2f && X.T < 0.3f; CVar(TEXT("lyud.Bird.ChaseYaw"), 180.f); CVar(TEXT("lyud.Bird.ChasePitch"), 25); return false; });
		S->Add(3.f, [](FCtx& X) { CVar(TEXT("lyud.Bird.ChaseCam"), 0); X.Bird->SetFakeHead(FRotator(-15.f, -95.f, 0.f), true); return false; });
		S->Add(3.f, [](FCtx& X) { X.C.Space = true; X.Bird->SetFakeHead(FRotator(-15.f, -95.f, 0.f), true); return false; });
		S->Add(2.f, [](FCtx& X) { X.Bird->SetFakeHead(FRotator(-70.f, 0.f, 0.f), true); return false; });
		S->Add(2.f, [](FCtx& X) { X.C.Space = true; X.Bird->SetFakeHead(FRotator(-10.f, 100.f, 0.f), true); return false; });
		S->Add(2.f, [](FCtx& X) { X.Bird->SetFakeHead(FRotator::ZeroRotator, false); return false; });
		S->Add(0.1f, [](FCtx&) { CVar(TEXT("lyud.Bird.Freeze"), 0); CVar(TEXT("lyud.Bird.ChaseYaw"), 0); CVar(TEXT("lyud.Bird.ChaseDist"), 1); CVar(TEXT("lyud.Bird.ChasePitch"), 12); return true; });
	}
	else if (Kind == TEXT("savetest"))
	{
		// G7: save as a bird in the air, load, and be that bird again where it was (the log says)
		S->Add(0.1f, [Species](FCtx& X)
		{
			X.Sys->PlaceHero(0.f, 30.f, 0.f);
			const APawn* H = X.PC ? X.PC->GetPawn() : nullptr;
			if (!H) return true;
			X.Sys->BecomeSpecies(X.PC, Species, H->GetActorLocation() + FVector(300.f, 0.f, 400.f), 0.f, FVector(900.f, 0, 0), false, nullptr, true);
			return true;
		});
		S->Add(1.5f, [](FCtx& X) { X.C.Space = true; return false; });
		S->Add(0.1f, [](FCtx& X)
		{
			UE_LOG(LogTemp, Warning, TEXT("BIRD saving as %s at %s, hero at %s"), *X.Bird->GetSpecies().ToString(), *X.Bird->FeetLocation().ToCompactString(),
				X.Sys->GetHero() ? *X.Sys->GetHero()->GetActorLocation().ToCompactString() : TEXT("-"));
			GEngine->Exec(X.Sys->GetWorld(), TEXT("lyud.Save birdflight_test"));
			return true;
		});
		S->Add(0.5f, [](FCtx&) { return false; });
		S->Add(0.1f, [](FCtx& X) { CVar(TEXT("lyud.Bird.QuitAfterLoad"), 1); GEngine->Exec(X.Sys->GetWorld(), TEXT("lyud.Load 0")); return true; });
	}
	else if (Kind == TEXT("look"))
	{
		// perched on the hand: the wing, the breast and the feet up close, from the bird's eye
		Start(*S, Species);
		S->Add(1.f, [](FCtx& X) { X.C.Space = X.T < 0.05f; return false; });
		S->Add(2.5f, [](FCtx& X) { X.C.Space = true; X.bAim = true; X.Aim = FRotator(8.f, 0.f, 0.f); return false; });
		S->Add(3.f, [](FCtx& X) { X.C.Space = true; const float A = FMath::Sin(PI * FMath::Clamp(X.T / 3.f, 0.f, 1.f)); X.Bird->SetFakeHead(FRotator(-25.f * A, -95.f * A, 0.f), X.T < 2.95f); return false; });
		S->Add(3.f, [](FCtx& X) { const float A = FMath::Sin(PI * FMath::Clamp(X.T / 3.f, 0.f, 1.f)); X.Bird->SetFakeHead(FRotator(-20.f * A, 100.f * A, 0.f), X.T < 2.95f); return false; });
		S->Add(2.f, [](FCtx& X) { X.C.Left = X.C.Right = true; return false; });
		S->Add(2.f, [](FCtx& X) { X.C.Shift = true; return false; });
	}
	else
	{
		return nullptr;
	}
	return S;
}

// ------------------------------------------------------------------ the subsystem

UBirdFlightSubsystem* UBirdFlightSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UBirdFlightSubsystem>() : nullptr;
}

bool UBirdFlightSubsystem::IsBird(const UObject* WorldContext)
{
	const UBirdFlightSubsystem* S = Get(WorldContext);
	return S && S->Bird.IsValid();
}

ABirdPawn* UBirdFlightSubsystem::GetBird() const
{
	return Bird.Get();
}

bool UBirdFlightSubsystem::BecomeBird(APlayerController* PC, int32 BirdId)
{
	UBirdFlightSubsystem* S = PC ? Get(PC) : nullptr;
	if (!S || S->Bird.IsValid())
	{
		return false;
	}
#if LYUD_HAS_BIRDLIFE
	UBirdLifeSubsystem* Life = PC->GetWorld()->GetSubsystem<UBirdLifeSubsystem>();
	FBirdLifeInfo Info;
	if (!Life || !Life->GetBird(BirdId, Info))
	{
		return false;
	}
	AActor* Actor = Life->ClaimBird(BirdId);
	const bool bStanding = Info.Velocity.Size() < 30.f;
	// standing (on the palm), it turns round after eating and faces the way the hero looks
	const float Yaw = bStanding ? PC->GetControlRotation().Yaw : Info.Velocity.Rotation().Yaw;
	if (!S->BecomeSpecies(PC, Info.SpeciesId, Info.Location, Yaw, Info.Velocity, bStanding, Actor))
	{
		Life->ReleaseBird(BirdId);
		return false;
	}
	S->ClaimedId = BirdId;
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("Bird flight: BecomeBird(%d) without bird life in this build"), BirdId);
	return false;
#endif
}

FMinimalViewInfo UBirdFlightSubsystem::HeroView() const
{
	FMinimalViewInfo V;
	V.FOV = HeroFov;
	V.Rotation = HeroControl;
	if (const AHeroCharacter* H = Cast<AHeroCharacter>(Hero.Get()))
	{
		V.Location = H->EyeLocation();
	}
	else if (const APawn* P = Hero.Get())
	{
		V.Location = P->GetPawnViewLocation();
	}
	return V;
}

void UBirdFlightSubsystem::SetHeroHeadShown(bool bShown)
{
	// the hero's head is hidden for his own first person; from the bird you see his face
	if (ACharacter* C = Cast<ACharacter>(Hero.Get()))
	{
		if (USkeletalMeshComponent* M = C->GetMesh(); M && M->GetSkeletalMeshAsset())
		{
			if (bShown) M->UnHideBoneByName(HeadBone);
			else M->HideBoneByName(HeadBone, PBO_None);
		}
	}
}

bool UBirdFlightSubsystem::BecomeSpecies(APlayerController* PC, FName Species, const FVector& Where, float Yaw, const FVector& VelocityCm, bool bStanding, AActor* Shown, bool bInstant)
{
	UWorld* World = GetWorld();
	if (!PC || !World || Bird.IsValid())
	{
		return false;
	}
	FMinimalViewInfo From;
	if (PC->PlayerCameraManager)
	{
		From = PC->PlayerCameraManager->GetCameraCacheView();
	}
	Player = PC;
	Hero = PC->GetPawn();
	HeroControl = PC->GetControlRotation();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABirdPawn* B = World->SpawnActor<ABirdPawn>(ABirdPawn::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Where), P);
	if (!B)
	{
		return false;
	}
	// bird life's own body for this bird stays its body seen from outside
	B->Setup(Species, Where, Yaw, VelocityCm, bStanding, Cast<ABirdActor>(Shown));
	ShownActor = Shown;
	Bird = B;
	if (Hero.IsValid())
	{
		// the bird flies past the hero, never into him
		B->GetRootPrimitive()->IgnoreActorWhenMoving(Hero.Get(), true);
	}
	PC->Possess(B);
	PC->SetControlRotation(FRotator(bStanding ? -4.f : 0.f, Yaw, 0.f));
	SetHeroHeadShown(true);
	if (bInstant || !Hero.IsValid())
	{
		B->SkipArrival();
	}
	else
	{
		B->BeginArrival(From, ArrivalSeconds);
	}
	B->OnReturnRequested = [this]() { ReturnToHero(); };
	UE_LOG(LogTemp, Log, TEXT("Bird flight: the player is a %s"), *Species.ToString());
	OnBecameBird.Broadcast(Hero.Get());
	return true;
}

void UBirdFlightSubsystem::ReturnToHero(bool bInstant)
{
	ABirdPawn* B = Bird.Get();
	if (!B)
	{
		return;
	}
	if (bInstant || !Hero.IsValid())
	{
		FinishReturn();
		return;
	}
	if (B->GetMode() == EBirdMode::Leaving)
	{
		return;
	}
	B->BeginLeaving(HeroView(), [this]() { FinishReturn(); });
}

void UBirdFlightSubsystem::FinishReturn()
{
	ABirdPawn* B = Bird.Get();
	APlayerController* PC = Player.Get();
	const FVector Where = B ? B->GetActorLocation() : FVector::ZeroVector;
	if (PC && Hero.IsValid())
	{
		PC->Possess(Hero.Get());
		PC->SetControlRotation(HeroControl);
	}
	SetHeroHeadShown(false);
#if LYUD_HAS_BIRDLIFE
	if (ClaimedId >= 0)
	{
		if (UBirdLifeSubsystem* Life = GetWorld()->GetSubsystem<UBirdLifeSubsystem>())
		{
			Life->SetClaimedBirdLocation(ClaimedId, Where, B ? B->GetVelocityCm() : FVector::ZeroVector);
			Life->ReleaseBird(ClaimedId);
		}
	}
#endif
	if (AActor* Shown = ShownActor.Get())
	{
		Shown->SetActorLocation(Where);
		Shown->SetActorHiddenInGame(false);
	}
	ClaimedId = -1;
	ShownActor = nullptr;
	Bird = nullptr;
	if (B)
	{
		B->Destroy();
	}
	OnReturnedToHero.Broadcast(Hero.Get());
}

void UBirdFlightSubsystem::PlaceHero(float MapX, float MapY, float YawDeg)
{
	APlayerController* PC = FirstPC(GetWorld());
	APawn* H = Hero.Get();
	if (!H && PC) H = PC->GetPawn();
	if (!H)
	{
		return;
	}
	FBirdAir A;
	A.Init(GetWorld());
	const FVector2D XY(MapX * 100.0, PortalFaceY + MapY * 100.0);
	const float Z = A.GroundZ(XY);
	const float Half = Cast<ACharacter>(H) ? Cast<ACharacter>(H)->GetDefaultHalfHeight() : 96.f;
	H->SetActorLocation(FVector(XY, Z + Half + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	H->SetActorRotation(FRotator(0.f, YawDeg, 0.f));
	if (PC && PC->GetPawn() == H)
	{
		PC->SetControlRotation(FRotator(0.f, YawDeg, 0.f));
	}
}

void UBirdFlightSubsystem::RunScript(const FString& Name, const FString& Record, bool bQuitAfter, float WarmSeconds)
{
	WarmUntil = FPlatformTime::Seconds() + WarmSeconds;
	Script = FScript::Make(Name);
	if (!Script)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bird flight: no script %s (showcase|ridge|look|pose|savetest[:Species], styles)"), *Name);
		return;
	}
	RecordDir = Record;
	RecordFrame = 0;
	bQuitAfterScript = bQuitAfter;
	UE_LOG(LogTemp, Warning, TEXT("BIRD script %s start (%d steps)%s"), *Name, Script->Steps.Num(), RecordDir.IsEmpty() ? TEXT("") : *(TEXT(" recording to ") + RecordDir));
}

void UBirdFlightSubsystem::Tick(float DeltaTime)
{
	if (QuitIn > 0.f)
	{
		QuitIn -= DeltaTime;
		if (QuitIn <= 0.f && GEngine)
		{
			GEngine->Exec(GetWorld(), TEXT("quit"));
		}
	}
	if (!Script)
	{
		return;
	}
	if (FPlatformTime::Seconds() < WarmUntil)
	{
		// shaders compiling, forests baking: nothing to record yet
		return;
	}
	if (!RecordDir.IsEmpty() && !FApp::UseFixedTimeStep())
	{
		// one frame per 1/30 s of game time, however long the rendering takes
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / 30.0);
		return;
	}
	FScript& S = *Script;
	APlayerController* PC = FirstPC(GetWorld());
	if (S.Index >= S.Steps.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("BIRD script %s done after %.1f s, %d frames"), *S.Name, S.Clock, RecordFrame);
		Script.Reset();
		if (ABirdPawn* B = Bird.Get())
		{
			B->bScripted = false;
			B->SetFakeHead(FRotator::ZeroRotator, false);
		}
		if (bQuitAfterScript)
		{
			QuitIn = 0.5f;
		}
		return;
	}
	FScript::FCtx X;
	X.Sys = this;
	X.Bird = Bird.Get();
	X.PC = PC;
	X.T = S.StepT;
	X.Dt = DeltaTime;
	const bool bDone = S.Steps[S.Index].Run(X);
	S.StepT += DeltaTime;
	S.Clock += DeltaTime;
	if (bDone || S.StepT >= S.Steps[S.Index].Max)
	{
		S.Index++;
		S.StepT = 0.f;
	}
	if (ABirdPawn* B = Bird.Get())
	{
		B->bScripted = true;
		if (X.bAim && PC)
		{
			// turn the gaze like a hand on a mouse: at most AimRate degrees a second
			const FRotator Cur = PC->GetControlRotation();
			const float DYaw = FRotator::NormalizeAxis(X.Aim.Yaw - Cur.Yaw);
			const float DPitch = FRotator::NormalizeAxis(X.Aim.Pitch) - FRotator::NormalizeAxis(Cur.Pitch);
			const float Max = X.AimRate * DeltaTime;
			X.C.Look = FVector2D(FMath::Clamp(DYaw, -Max, Max), FMath::Clamp(DPitch, -Max, Max));
		}
		B->Script = X.C;
	}
	if (ABirdPawn* B = Bird.Get(); B && FMath::Fmod(S.Clock, 0.5f) < DeltaTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("BIRD t=%.1f step %d %s at %s"), S.Clock, S.Index, *B->DebugLine(), *B->GetActorLocation().ToCompactString());
	}
	if (!RecordDir.IsEmpty())
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(RecordDir, FString::Printf(TEXT("f_%05d.png"), RecordFrame++)), false, false);
	}
}

// ------------------------------------------------------------------ save and load

void UBirdFlightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(&InWorld))
	{
		SaveHandle = Save->OnBeforeSave.AddUObject(this, &UBirdFlightSubsystem::OnBeforeSave);
		LoadHandle = Save->OnAfterLoad.AddUObject(this, &UBirdFlightSubsystem::OnAfterLoad);
	}
}

void UBirdFlightSubsystem::Deinitialize()
{
	if (ULyudSaveSubsystem* Save = GetWorld() ? ULyudSaveSubsystem::Get(GetWorld()) : nullptr)
	{
		Save->OnBeforeSave.Remove(SaveHandle);
		Save->OnAfterLoad.Remove(LoadHandle);
	}
	Super::Deinitialize();
}

void UBirdFlightSubsystem::OnBeforeSave(ULyudSaveGame* Save)
{
	ABirdPawn* B = Bird.Get();
	bSavedAsBird = B != nullptr && B->GetMode() != EBirdMode::Leaving;
	if (bSavedAsBird)
	{
		// the save's player is the hero where he stands; the bird is its own record
		if (const APawn* H = Hero.Get())
		{
			Save->bHasPlayer = true;
			Save->PlayerTransform = H->GetActorTransform();
		}
		Save->ControlRotation = HeroControl;
		SavedSpecies = B->GetSpecies();
		bSavedStanding = B->GetMode() == EBirdMode::Ground;
		SavedLocation = B->FeetLocation();
		SavedVelocity = B->GetVelocityCm();
		SavedAim = Player.IsValid() ? Player->GetControlRotation() : B->GetActorRotation();
	}
	Save->WriteObject(SaveKey, this);
}

void UBirdFlightSubsystem::OnAfterLoad(ULyudSaveGame* Save)
{
	if (IConsoleVariable* Q = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Bird.QuitAfterLoad")); Q && Q->GetInt())
	{
		QuitIn = 2.f;
	}
	if (!Save->ReadObject(SaveKey, this) || !bSavedAsBird)
	{
		return;
	}
	if (APlayerController* PC = FirstPC(GetWorld()))
	{
		const FRotator Aim = SavedAim;
		if (BecomeSpecies(PC, SavedSpecies, SavedLocation, Aim.Yaw, SavedVelocity, bSavedStanding, nullptr, true))
		{
			HeroControl = Save->ControlRotation;
			PC->SetControlRotation(Aim);
			UE_LOG(LogTemp, Warning, TEXT("BIRD loaded as %s at %s, hero at %s"), *SavedSpecies.ToString(), *SavedLocation.ToCompactString(),
				Hero.IsValid() ? *Hero->GetActorLocation().ToCompactString() : TEXT("-"));
		}
	}
}

// ------------------------------------------------------------------ console

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdBecome(TEXT("Bird.Become"), TEXT("Bird.Become [Species] [air]: become a bird on the hero's hand (or in the air ahead), without befriending."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBirdFlightSubsystem* S = UBirdFlightSubsystem::Get(World);
			APlayerController* PC = FirstPC(World);
			if (!S || !PC || !PC->GetPawn()) return;
			const FName Species = Args.IsValidIndex(0) ? FName(*Args[0]) : FName(TEXT("Starling"));
			const bool bAir = Args.IsValidIndex(1) && Args[1] == TEXT("air");
			const FRotator Yaw(0.f, PC->GetControlRotation().Yaw, 0.f);
			const AHeroCharacter* H = Cast<AHeroCharacter>(PC->GetPawn());
			const FVector Eye = H ? H->EyeLocation() : PC->GetPawn()->GetPawnViewLocation();
			if (bAir)
			{
				S->BecomeSpecies(PC, Species, Eye + Yaw.Vector() * 150.f, Yaw.Yaw, Yaw.Vector() * 900.f, false);
			}
			else
			{
				S->BecomeSpecies(PC, Species, Eye + Yaw.Vector() * 38.f - FVector(0, 0, 32.f), Yaw.Yaw, FVector::ZeroVector, true);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdReturn(TEXT("Bird.Return"), TEXT("Bird.Return [now]: back to the hero."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UBirdFlightSubsystem* S = UBirdFlightSubsystem::Get(World)) S->ReturnToHero(Args.Contains(TEXT("now")));
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdScript(TEXT("Bird.Script"), TEXT("Bird.Script <showcase|ridge|look|pose|savetest>[:Species]|styles [record=<folder>] [warm=<s>] [quit]: a scripted flight."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBirdFlightSubsystem* S = UBirdFlightSubsystem::Get(World);
			if (!S || Args.IsEmpty()) return;
			FString Record;
			bool bQuit = false;
			float Warm = 0.f;
			for (const FString& A : Args)
			{
				if (A.StartsWith(TEXT("record="))) Record = A.Mid(7);
				if (A.StartsWith(TEXT("warm="))) Warm = FCString::Atof(*A.Mid(5));
				if (A == TEXT("quit")) bQuit = true;
			}
			S->RunScript(Args[0], Record, bQuit, Warm);
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdHero(TEXT("Bird.Hero"), TEXT("Bird.Hero <mapX> <mapY> [yaw]: the hero onto the ground at a map point (metres, world_map.py)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UBirdFlightSubsystem* S = UBirdFlightSubsystem::Get(World); S && Args.Num() >= 2)
			{
				S->PlaceHero(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.f);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdFakeHead(TEXT("lyud.Bird.FakeHead"), TEXT("lyud.Bird.FakeHead <yaw> <pitch> [roll] | off: pretend AirPods report this head pose."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBirdFlightSubsystem* S = UBirdFlightSubsystem::Get(World);
			ABirdPawn* B = S ? S->GetBird() : nullptr;
			if (!B) return;
			const bool bOn = Args.Num() >= 2;
			B->SetFakeHead(bOn ? FRotator(FCString::Atof(*Args[1]), FCString::Atof(*Args[0]), Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.f) : FRotator::ZeroRotator, bOn);
		}));
}
