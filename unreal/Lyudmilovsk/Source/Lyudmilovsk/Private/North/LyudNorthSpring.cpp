#include "North/LyudNorthSpring.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "North/LyudFumarole.h"
#include "North/LyudNorthFx.h"
#include "North/NorthModel.h"

namespace
{
	/** The seat's frame sits this far ahead of the sitter's place (AHeroCharacter::SitOn: under the cushion's front edge), cm. */
	constexpr float SpringSeatFront = 10.f;
	/** Getting out: this far beyond the rim, cm. */
	constexpr float SpringStepOut = 90.f;

	FAutoConsoleCommandWithWorldAndArgs GNorthSitSpringCmd(TEXT("lyud.North.SitSpring"),
		TEXT("lyud.North.SitSpring 1|0: the hero wades into the hot spring's nearest free seat (from anywhere) / gets out; logs a NORTH spring line"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALyudNorthSpring::SetHeroSitting(World, Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}));
}

ALyudNorthSpring::ALyudNorthSpring()
{
	PrimaryActorTick.TickInterval = 0.5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Carrier = CreateDefaultSubobject<USceneComponent>(TEXT("Carrier"));
	Carrier->SetupAttachment(RootComponent);
}

ALyudNorthSpring* ALyudNorthSpring::SpawnFor(UWorld* World, const FLyudNorthSpringData& InData)
{
	if (!World || (InData.Seats.Num() == 0 && InData.RadiusCm <= 0.f))
	{
		return nullptr;
	}
	const FTransform At(InData.Center);
	ALyudNorthSpring* S = World->SpawnActorDeferred<ALyudNorthSpring>(ALyudNorthSpring::StaticClass(), At, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!S)
	{
		return nullptr;
	}
	S->Data = InData;
	S->RadiusCm = InData.RadiusCm > 0.f ? InData.RadiusCm : 260.f;
	S->Seats = InData.Seats;
	if (S->Seats.Num() == 0)
	{
		// no seats in north.json yet: four round the pool, facing its middle
		for (int32 I = 0; I < 4; I++)
		{
			const float A = FMath::DegreesToRadians(45.f + 90.f * I);
			FLyudNorthSpringSeatData& Seat = S->Seats.AddDefaulted_GetRef();
			Seat.At = FVector2D(InData.Center.X + FMath::Cos(A) * 0.6f * S->RadiusCm, InData.Center.Y + FMath::Sin(A) * 0.6f * S->RadiusCm);
			Seat.Yaw = FMath::RadiansToDegrees(A) + 180.f;
		}
		UE_LOG(LogTemp, Display, TEXT("NORTH spring: no seats in north.json, %d guessed round the pool"), S->Seats.Num());
	}
	S->Taken.SetNum(S->Seats.Num());
	S->WaterZ = InData.bHasSurfaceZ ? InData.SurfaceZ : InData.Center.Z;
	S->FinishSpawning(At);
	return S;
}

ALyudNorthSpring* ALyudNorthSpring::FindSpring(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ALyudNorthSpring> It(World);
	return It ? *It : nullptr;
}

void ALyudNorthSpring::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("NORTH spring: %d seats, r %.1f m"), Seats.Num(), RadiusCm / 100.f);
}

void ALyudNorthSpring::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(0);
	ReleaseHero();
	Super::EndPlay(EndPlayReason);
}

AHeroCharacter* ALyudNorthSpring::Hero() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
}

void ALyudNorthSpring::ResolveWater()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	bool bKnown = false;
	double Z = WaterZ;
	if (Data.bHasSurfaceZ)
	{
		Z = Data.SurfaceZ;
		bKnown = true;
	}
	else
	{
		// the Spring fumarole's still water (it settles on the built ground at its BeginPlay)
		ALyudFumarole* Best = Pool.Get();
		if (!Best)
		{
			double BestD = FMath::Square(double(RadiusCm) + 500.0);
			for (TActorIterator<ALyudFumarole> It(World); It; ++It)
			{
				if (It->Kind != ELyudFumaroleKind::Spring)
				{
					continue;
				}
				const double D = FVector::DistSquared2D(It->GetActorLocation(), Data.Center);
				if (D < BestD)
				{
					BestD = D;
					Best = *It;
				}
			}
			Pool = Best;
		}
		if (Best && Best->HasActorBegunPlay())
		{
			Z = Best->GetActorLocation().Z + Best->GetSurfaceZ();
			bKnown = true;
		}
		else if (!Best)
		{
			// no pool actor: the ground (or the deck) at the middle, the water a little under its rim
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NorthSpringWater), false, Hero());
			FHitResult Hit;
			const FVector From(Data.Center.X, Data.Center.Y, 40000.0), To(Data.Center.X, Data.Center.Y, -20000.0);
			if (World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q))
			{
				Z = Hit.ImpactPoint.Z - 5.0;
				bKnown = true;
			}
		}
	}
	WaterZ = Z;
	if (bKnown && !bWaterKnown && HeroSeat < 0)
	{
		SetActorLocation(FVector(Data.Center.X, Data.Center.Y, WaterZ));
	}
	bWaterKnown = bKnown;
}

// ---- seats -------------------------------------------------------------------------------------------------------------

int32 ALyudNorthSpring::FindFreeSeat(const FVector& Near) const
{
	int32 Best = -1;
	double BestD = TNumericLimits<double>::Max();
	for (int32 I = 0; I < Seats.Num(); I++)
	{
		if (!IsSeatFree(I))
		{
			continue;
		}
		const double D = FVector2D::DistSquared(Seats[I].At, FVector2D(Near.X, Near.Y));
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	return Best;
}

bool ALyudNorthSpring::IsSeatFree(int32 Seat) const
{
	return Taken.IsValidIndex(Seat) && !Taken[Seat].IsValid();
}

bool ALyudNorthSpring::Occupy(int32 Seat, UObject* Who)
{
	if (!Who || !Taken.IsValidIndex(Seat))
	{
		return false;
	}
	if (Taken[Seat].Get() == Who)
	{
		return true;
	}
	if (Taken[Seat].IsValid())
	{
		return false;
	}
	Release(Who);
	Taken[Seat] = Who;
	return true;
}

void ALyudNorthSpring::Release(UObject* Who)
{
	for (TWeakObjectPtr<UObject>& T : Taken)
	{
		if (Who && T.Get() == Who)
		{
			T.Reset();
		}
	}
}

FTransform ALyudNorthSpring::GetSeatWorld(int32 Seat) const
{
	if (!Seats.IsValidIndex(Seat))
	{
		return FTransform::Identity;
	}
	const FLyudNorthSpringSeatData& S = Seats[Seat];
	const FRotator Rot(0.f, S.Yaw, 0.f);
	const FVector Fwd = Rot.Vector();
	const double FloorZ = WaterZ - double(WaterOverSeat) - double(SeatHeight);
	return FTransform(Rot, FVector(S.At.X + Fwd.X * SpringSeatFront, S.At.Y + Fwd.Y * SpringSeatFront, FloorZ));
}

FVector ALyudNorthSpring::StandOffsetFor(int32 Seat) const
{
	// out over the rim behind the seat (away from the pool's middle), on the ground there
	const FTransform SeatW = GetSeatWorld(Seat);
	FVector2D Dir = Seats[Seat].At - FVector2D(Data.Center.X, Data.Center.Y);
	if (Dir.IsNearlyZero())
	{
		const FVector Back = -SeatW.GetRotation().GetForwardVector();
		Dir = FVector2D(Back.X, Back.Y);
	}
	Dir = Dir.GetSafeNormal();
	const double Out = double(RadiusCm) + double(SpringStepOut);
	FVector At(Data.Center.X + Dir.X * Out, Data.Center.Y + Dir.Y * Out, WaterZ + 10.0);
	if (const UWorld* World = GetWorld())
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NorthSpringOut), false, Hero());
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 300.0), At - FVector(0.0, 0.0, 500.0), ECC_Visibility, Q))
		{
			At.Z = Hit.ImpactPoint.Z;
		}
	}
	return SeatW.InverseTransformPosition(At);
}

// ---- the hero ----------------------------------------------------------------------------------------------------------

bool ALyudNorthSpring::IsHeroSeated() const
{
	return HeroSeat >= 0 && SeatedHero.IsValid() && SeatedHero->IsSeated();
}

float ALyudNorthSpring::HeroEyeAboveWater() const
{
	const AHeroCharacter* H = Hero();
	return H ? float(H->EyeLocation().Z - WaterZ) : 0.f;
}

bool ALyudNorthSpring::SitHero(int32 Seat, bool bWade)
{
	AHeroCharacter* H = Hero();
	if (!H || H->IsSeated() || H->IsOnBike() || !Carrier)
	{
		return false;
	}
	ResolveWater();
	if (Seat < 0)
	{
		Seat = FindFreeSeat(H->GetActorLocation());
	}
	if (!IsSeatFree(Seat))
	{
		return false;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	Occupy(Seat, H);
	const FTransform SeatW = GetSeatWorld(Seat);
	// the seat's frame on the carrier at rest; while he wades the carrier starts at his feet and glides to its rest
	Carrier->SetRelativeLocation(FVector::ZeroVector);
	const FTransform Local = SeatW.GetRelativeTransform(Carrier->GetComponentTransform());
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = H->GetActorLocation() - FVector(0.0, 0.0, Half);
	WadeTime = -1.f;
	if (bWade && bNear)
	{
		WadeFrom = Feet - SeatW.GetLocation();
		WadeTime = 0.f;
		Carrier->SetRelativeLocation(WadeFrom);
	}
	H->SitOn(Carrier, Local, SeatHeight);
	H->SetStandUpOffset(StandOffsetFor(Seat));
	// he follows the carrier in the same frame
	H->AddTickPrerequisiteActor(this);
	SeatedHero = H;
	HeroSeat = Seat;
	// a calm look across the water
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetControlRotation(FRotator(-6.f, SeatW.Rotator().Yaw, 0.f));
	}
	// the pool steams (it is near anyway; never wait for the next range check)
	if (ALyudFumarole* F = Pool.Get(); F && !F->IsAwake())
	{
		F->SetAwake(true, true);
	}
	SetPrompt(0);
	UE_LOG(LogTemp, Display, TEXT("NORTH spring sit: seat %d%s"), Seat, WadeTime >= 0.f ? TEXT(" wading in") : TEXT(""));
	return true;
}

void ALyudNorthSpring::StandHero()
{
	AHeroCharacter* H = SeatedHero.Get();
	if (H && HeroSeat >= 0 && H->IsSeated())
	{
		// out from where the seat rests (StandUp's offset is in its resting frame)
		Carrier->SetRelativeLocation(FVector::ZeroVector);
		WadeTime = -1.f;
		H->StandUp();
		UE_LOG(LogTemp, Display, TEXT("NORTH spring stand: from seat %d"), HeroSeat);
	}
	ReleaseHero();
	SetPrompt(0);
}

void ALyudNorthSpring::ReleaseHero()
{
	if (AHeroCharacter* H = SeatedHero.Get())
	{
		H->RemoveTickPrerequisiteActor(this);
		Release(H);
	}
	SeatedHero.Reset();
	HeroSeat = -1;
	WadeTime = -1.f;
	if (Carrier)
	{
		Carrier->SetRelativeLocation(FVector::ZeroVector);
	}
}

void ALyudNorthSpring::SetHeroSitting(UWorld* World, bool bSit)
{
	ALyudNorthSpring* S = FindSpring(World);
	if (!S)
	{
		UE_LOG(LogTemp, Warning, TEXT("NORTH spring: none (north.json has no spring)"));
		return;
	}
	if (bSit)
	{
		if (!S->SitHero(-1, true))
		{
			UE_LOG(LogTemp, Warning, TEXT("NORTH spring: the hero cannot sit (no hero, seated elsewhere, on a bike, or no free seat)"));
		}
	}
	else
	{
		S->StandHero();
	}
	UE_LOG(LogTemp, Display, TEXT("NORTH %s"), *S->DebugReport());
}

FString ALyudNorthSpring::DebugReport() const
{
	int32 Free = 0;
	for (int32 I = 0; I < Seats.Num(); I++)
	{
		Free += IsSeatFree(I) ? 1 : 0;
	}
	const double Cushion = HeroSeat >= 0 ? double(WaterOverSeat) : 0.0;
	return FString::Printf(TEXT("spring seats %d free %d water z %.0f known %d awake %d near %d hero seat %d seated %d wading %d eye %+.0f cm over the water, cushion %.0f cm under"),
		Seats.Num(), Free, WaterZ, bWaterKnown ? 1 : 0, bAwake ? 1 : 0, bNear ? 1 : 0, HeroSeat, IsHeroSeated() ? 1 : 0,
		WadeTime >= 0.f ? 1 : 0, HeroEyeAboveWater(), Cushion);
}

// ---- ticking -----------------------------------------------------------------------------------------------------------

void ALyudNorthSpring::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	if (bAwake && !bWaterKnown)
	{
		ResolveWater();
	}
	// near: every frame (E is polled, the wade glides); else a slow look
	SetActorTickInterval(bNear ? 0.f : 0.5f);
	if (!bNear)
	{
		SetPrompt(0);
		if (HeroSeat >= 0)
		{
			StandHero();
		}
	}
}

void ALyudNorthSpring::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bWaterKnown)
	{
		ResolveWater();
	}
	if (bNear)
	{
		UpdateHero(DeltaTime);
	}
	else
	{
		SetPrompt(0);
	}
}

void ALyudNorthSpring::SetPrompt(int32 Mode)
{
	if (Mode == Prompted)
	{
		return;
	}
	Prompted = Mode;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Mode == 1 ? LyudText::Get(TEXT("ui.north.spring_sit"))
			: Mode == 2 ? LyudText::Get(TEXT("ui.north.spring_out")) : FText::GetEmpty());
	}
}

void ALyudNorthSpring::UpdateHero(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHeroCharacter* H = Hero();
	if (!PC || !H)
	{
		SetPrompt(0);
		return;
	}
	// he got up another way (Space, a step): the seat is free again
	if (HeroSeat >= 0 && !IsHeroSeated())
	{
		UE_LOG(LogTemp, Display, TEXT("NORTH spring stand: from seat %d (got up himself)"), HeroSeat);
		ReleaseHero();
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bScreen = UI && UI->IsOpen();
	if (HeroSeat >= 0)
	{
		if (WadeTime >= 0.f && Carrier)
		{
			// wading in: across first, down into the water a little later
			WadeTime += DeltaTime;
			const float Span = FMath::Max(0.1f, WadeSeconds);
			const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(WadeTime / (0.8f * Span), 0.f, 1.f));
			const float Az = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((WadeTime - 0.2f * Span) / (0.8f * Span), 0.f, 1.f));
			Carrier->SetRelativeLocation(FVector(WadeFrom.X * (1.0 - A), WadeFrom.Y * (1.0 - A), WadeFrom.Z * (1.0 - Az)));
			if (WadeTime >= Span)
			{
				WadeTime = -1.f;
				Carrier->SetRelativeLocation(FVector::ZeroVector);
			}
		}
		// soft steam round his shoulders
		ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this);
		if (ALyudNorthSteam* Fx = N ? N->Steam() : nullptr)
		{
			SteamDebt += DeltaTime * 1.2f * Fx->GetDensity();
			const FVector Body = H->GetActorLocation();
			while (SteamDebt >= 1.f)
			{
				SteamDebt -= 1.f;
				const float Ang = LyudNorthModel::Rand01(SteamRng) * 2.f * PI;
				const float Rr = 30.f + 35.f * LyudNorthModel::Rand01(SteamRng);
				const float Up = 18.f + 12.f * LyudNorthModel::Rand01(SteamRng);
				Fx->Emit(FVector(Body.X + FMath::Cos(Ang) * Rr, Body.Y + FMath::Sin(Ang) * Rr, WaterZ + 4.0), FVector(0.f, 0.f, Up),
					35.f, 150.f, 0.09f, 4.5f, 9.f, 1.2f, 0.f, 0.8f);
			}
		}
		SetPrompt(bScreen ? 0 : 2);
		// polled, never consumed: E belongs to several things at once
		if (!bScreen && PC->WasInputKeyJustPressed(EKeys::E))
		{
			StandHero();
		}
		return;
	}
	// standing by the pool: E sits him down in the nearest free seat
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = H->GetActorLocation() - FVector(0.0, 0.0, Half);
	const bool bClose = FVector::Dist2D(Feet, Data.Center) < double(RadiusCm + Reach) && FMath::Abs(Feet.Z - WaterZ) < 250.0;
	const bool bCan = bClose && bWaterKnown && !bScreen && !H->IsSeated() && !H->IsOnBike() && FindFreeSeat(Feet) >= 0;
	SetPrompt(bCan ? 1 : 0);
	if (bCan && PC->WasInputKeyJustPressed(EKeys::E))
	{
		SitHero(-1, true);
	}
}
