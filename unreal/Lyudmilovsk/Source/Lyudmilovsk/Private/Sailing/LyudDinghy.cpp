// A sailing dinghy as an actor: it owns its LyudSail::FBoat, samples the wind and the sea for it, steps it, places the
// drawn boat, takes the hero's keys when he has the helm, and plays its sounds. ULyudSailing calls Advance* (the actor
// never ticks). docs/yacht_school.md "Engine: the boats".
#include "Sailing/LyudDinghy.h"

#include "LyudDinghyImpl.h"

#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "Templates/PimplPtr.h"
#include "UI/LyudGameScreen.h"
#include "Weather/LyudWind.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailBoat, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarDinghyDebug(TEXT("lyud.Sail.Debug"), 0,
		TEXT("lyud.Sail.Debug 1: the hero's boat's numbers on the HUD (apparent wind, forces' results, the polar's target)."));
	TAutoConsoleVariable<int32> CVarDinghyEasy(TEXT("lyud.Sail.Easy"), 1,
		TEXT("lyud.Sail.Easy 0|1|2: help at the hero's helm. 0 none; 1 the crew's weight, the board and the bailing look after themselves; 2 the sheet too, and no capsize."));

	TWeakObjectPtr<ALyudDinghy> GDinghyHeroBoat;

	constexpr float DinghyMidStep = 1.f / 15.f;

	bool DinghyAfloat(LyudSail::EState S)
	{
		return S == LyudSail::EState::Sailing || S == LyudSail::EState::Swamped || S == LyudSail::EState::Towed || S == LyudSail::EState::Aground;
	}

	bool DinghyOver(LyudSail::EState S)
	{
		return S == LyudSail::EState::Capsized || S == LyudSail::EState::Turtled || S == LyudSail::EState::Righting;
	}
}

// ------------------------------------------------------------------ the actor

ALyudDinghy::ALyudDinghy()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Impl = MakePimpl<FImpl>();
	Loops.SetNum(LyudSail::Sound::NumLoops);
}

ALyudDinghy::~ALyudDinghy() = default;

void ALyudDinghy::Setup(int32 InClass, int32 InNumber, int32 InColours, uint32 Seed)
{
	FImpl& I = *Impl;
	if (bHeroAboard)
	{
		HeroGone(true);
	}
	I.CrewDrop(*this, 0, true);
	I.CrewDrop(*this, 1, true);
	BoatClass = FMath::Clamp(InClass, 0, 2);
	SailNumber = InNumber;
	LyudSail::Init(I.B, LyudSail::EBoat(BoatClass), 0.0, 0.5, Seed);
	I.Rng.Initialize(int32(Seed));
	I.Build(*this, BoatClass, InNumber, InColours);
	I.bAssistSheet = I.bAssistHike = I.bAssistNoCapsize = false;
	Tele = FLyudDinghyTelemetry();
	bOnTrolley = false;
	Lod = ELyudDinghyLod::Parked;
	I.ApplyLod(*this);
}

LyudSail::FBoat& ALyudDinghy::Boat()
{
	return Impl->B;
}

const LyudSail::FBoat& ALyudDinghy::Boat() const
{
	return Impl->B;
}

const LyudSail::FEnv& ALyudDinghy::Env() const
{
	return Impl->Env;
}

const FLyudSailHudData* ALyudDinghy::HudData() const
{
	return &Impl->Hud;
}

USceneComponent* ALyudDinghy::HullFrame() const
{
	return Impl->Hull;
}

void ALyudDinghy::PlaceAt(const FVector2D& InPos, float InYaw, int32 State, float SpeedMs)
{
	FImpl& I = *Impl;
	const LyudSail::EState S = LyudSail::EState(FMath::Clamp(State, 0, int32(LyudSail::EState::Towed)));
	if (S == LyudSail::EState::Sailing && SpeedMs != 0.f)
	{
		LyudSail::PlaceSailing(I.B, InPos.X, InPos.Y, InYaw, SpeedMs);
	}
	else
	{
		LyudSail::Place(I.B, InPos.X, InPos.Y, InYaw, S);
	}
	// the outputs are the last step's until the next one: what the drawing reads is brought to the new place now
	LyudSail::FOutputs& O = I.B.Out;
	O.X = I.B.X;
	O.Y = I.B.Y;
	O.Yaw = I.B.Yaw;
	O.Heel = I.B.Heel;
	O.Pitch = 0.0;
	O.Speed = SpeedMs;
	O.State = I.B.State;
	O.Events = 0;
	I.StepEvents = 0;
	I.MidClock = 0.f;
	I.FarEnvClock = 1e9f;
	const bool bWasTrolley = bOnTrolley;
	bOnTrolley = false;
	I.Sample(*this, 2);
	I.DrawPos = FVector2D(I.B.X, I.B.Y) * 100.0;
	I.DrawYaw = float(I.B.Yaw);
	I.Place(*this);
	if (I.Hull)
	{
		if (bWasTrolley || Lod == ELyudDinghyLod::Parked)
		{
			I.ApplyLod(*this);
		}
		if (Lod != ELyudDinghyLod::Parked)
		{
			I.MoveParts(*this, 0.f);
		}
	}
	I.AfterStep(*this, 0.f, false);
}

void ALyudDinghy::ParkAt(const FTransform& World, bool bInRigged, bool bInCovered)
{
	FImpl& I = *Impl;
	if (bHeroAboard)
	{
		HeroGone(true);
	}
	I.CrewDrop(*this, 0, true);
	I.CrewDrop(*this, 1, true);
	I.StopLoops(*this, 0.f);
	const FVector At = World.GetLocation();
	const float YawDeg = World.Rotator().Yaw;
	LyudSail::Place(I.B, At.X * 0.01, At.Y * 0.01, FMath::DegreesToRadians(YawDeg), LyudSail::EState::Ashore);
	I.B.Out.State = I.B.State;
	I.B.Out.Events = 0;
	I.StepEvents = 0;
	I.bRigged = bInRigged;
	I.bCovered = bInCovered;
	bOnTrolley = true;
	Lod = ELyudDinghyLod::Parked;
	// World is the ground under the trolley: the boat's origin rides this far over it
	const float Up = LyudSailKit::ClassDef(BoatClass).TrolleyZ;
	SetActorLocationAndRotation(At + World.GetRotation().GetUpVector() * Up, World.GetRotation());
	I.DrawPos = FVector2D(At);
	I.DrawYaw = FMath::DegreesToRadians(YawDeg);
	Tele = FLyudDinghyTelemetry();
	Tele.PosM = FVector2D(At) * 0.01;
	Tele.YawRad = I.DrawYaw;
	Tele.State = int32(LyudSail::EState::Ashore);
	I.ApplyLod(*this);
}

void ALyudDinghy::SetLod(ELyudDinghyLod InLod)
{
	if (InLod == Lod)
	{
		return;
	}
	Lod = InLod;
	Impl->MidClock = 0.f;
	Impl->FarEnvClock = 1e9f;
	Impl->ApplyLod(*this);
	if (InLod == ELyudDinghyLod::Far || InLod == ELyudDinghyLod::Parked)
	{
		// no bodies that far (the people stay the boat's: the life does not draw them ashore meanwhile)
		Impl->CrewDrop(*this, 0, false);
		Impl->CrewDrop(*this, 1, false);
	}
}

void ALyudDinghy::Advance(const LyudSail::FControls& Controls, float DeltaTime)
{
	if (Lod == ELyudDinghyLod::Parked || !Impl->Hull)
	{
		return;
	}
	FImpl& I = *Impl;
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.25f);
	if (bHeroAboard && (!I.Hero.IsValid() || !I.Hero->IsSeated()))
	{
		// he was stood up by something else (a script, the dev teleport)
		HeroGone(false);
	}
	bool bStepped = false;
	if (bHeroAboard || Lod == ELyudDinghyLod::Near)
	{
		I.Sample(*this, 2);
		if (bHeroAboard)
		{
			I.ApplyAssists(*this);
			I.Last = I.HeroControls(*this, Dt);
		}
		else
		{
			I.Last = Controls;
		}
		LyudSail::Step(I.B, I.Last, I.Env, Dt);
		bStepped = true;
	}
	else
	{
		// further off: the same model at a fixed lower rate, drawn between the steps by its speed
		I.Last = Controls;
		I.MidClock += Dt;
		if (I.MidClock >= DinghyMidStep)
		{
			I.Sample(*this, Lod == ELyudDinghyLod::Mid ? 1 : 0);
			LyudSail::Step(I.B, I.Last, I.Env, I.MidClock);
			I.MidClock = 0.f;
			bStepped = true;
		}
	}
	I.StepEvents = bStepped ? I.B.Out.Events : 0u;
	I.AfterStep(*this, Dt, bStepped);
}

void ALyudDinghy::AdvanceFar(float CourseYawRad, float SpeedScale, float DeltaTime)
{
	if (Lod == ELyudDinghyLod::Parked || !Impl->Hull)
	{
		return;
	}
	FImpl& I = *Impl;
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 2.f);
	// the wind and the depth a couple of times a second are plenty for a boat on the polar
	I.FarEnvClock += Dt;
	if (I.FarEnvClock > 0.5f)
	{
		I.FarEnvClock = 0.f;
		I.Sample(*this, Lod == ELyudDinghyLod::Far ? 0 : 1);
	}
	LyudSail::StepFar(I.B, CourseYawRad, SpeedScale, I.Env, Dt);
	I.MidClock = 0.f;
	I.StepEvents = I.B.Out.Events;
	I.AfterStep(*this, Dt, true);
}

void ALyudDinghy::AdvanceTowed(const FVector2D& HitchM, float RopeM, float DeltaTime)
{
	if (Lod == ELyudDinghyLod::Parked || !Impl->Hull)
	{
		return;
	}
	FImpl& I = *Impl;
	const float Dt = FMath::Clamp(DeltaTime, 0.f, 0.25f);
	I.Sample(*this, Lod == ELyudDinghyLod::Near ? 2 : (Lod == ELyudDinghyLod::Mid ? 1 : 0));
	LyudSail::StepTowed(I.B, HitchM.X, HitchM.Y, RopeM, I.Env, Dt);
	I.MidClock = 0.f;
	I.StepEvents = I.B.Out.Events;
	I.AfterStep(*this, Dt, true);
}

void ALyudDinghy::SetCrew(int32 Slot, FName PersonKey, float AgeYears, bool bLifeJacket)
{
	if (Slot < 0 || Slot > 1)
	{
		return;
	}
	Impl->CrewSet(*this, Slot, PersonKey, AgeYears, bLifeJacket);
}

FName ALyudDinghy::GetCrew(int32 Slot) const
{
	return Slot >= 0 && Slot <= 1 ? Impl->Crew[Slot].Key : FName();
}

void ALyudDinghy::SetAssist(bool bAutoSheet, bool bAutoHike, bool bNoCapsize)
{
	FImpl& I = *Impl;
	I.bAssistSheet = bAutoSheet;
	I.bAssistHike = bAutoHike;
	I.bAssistNoCapsize = bNoCapsize;
	I.ApplyAssists(*this);
}

void ALyudDinghy::SetTutorHints(bool bOn)
{
	Impl->bTutor = bOn;
}

void ALyudDinghy::ShowHint(FName Key, float Seconds)
{
	Impl->Say(Key.ToString(), nullptr, Seconds);
}

void ALyudDinghy::SetHudTask(const FText& Title, const TArray<FText>& Lines, float Countdown, const FVector2D& NextMarkM)
{
	FLyudSailHudData& H = Impl->Hud;
	H.Title = Title;
	H.Lines = Lines;
	H.Countdown = Countdown;
	H.MarkM = NextMarkM;
	H.bHasMark = !NextMarkM.IsZero();
}

ALyudDinghy* ALyudDinghy::HeroBoat(const UObject* WorldContext)
{
	ALyudDinghy* Found = GDinghyHeroBoat.Get();
	if (!Found || !Found->bHeroAboard)
	{
		return nullptr;
	}
	const UWorld* Asked = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return !Asked || Found->GetWorld() == Asked ? Found : nullptr;
}

void ALyudDinghy::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Impl)
	{
		if (bHeroAboard)
		{
			HeroGone(true);
		}
		Impl->CrewDrop(*this, 0, true);
		Impl->CrewDrop(*this, 1, true);
		Impl->StopLoops(*this, 0.f);
	}
	Super::EndPlay(Reason);
}

// ------------------------------------------------------------------ the world at the boat

void ALyudDinghy::FImpl::Sample(ALyudDinghy& D, int32 Detail)
{
	const FVector C(B.X * 100.0, B.Y * 100.0, 0.0);
	const ALyudSea* Sea = ALyudSea::Get(&D);
	bSea = Sea != nullptr;
	const float Level = Sea ? Sea->SeaLevel() : float(D.GetActorLocation().Z);
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(&D))
	{
		// at sail height over the water, gusts included
		const FVector V = Wind->GetWindAt(FVector(C.X, C.Y, Level), 2.f, true).Velocity;
		Env.WindX = V.X;
		Env.WindY = V.Y;
	}
	if (!Sea)
	{
		Env.Depth = 100.0;
		Env.NormalX = Env.NormalY = 0.0;
		Env.NormalZ = 1.0;
		Env.WaveHs = 0.0;
		SurfaceZ = Level;
		BedZ = Level - 10000.f;
		return;
	}
	Env.WaveHs = Sea->WaveHeight() * 0.01;
	if (Detail <= 0)
	{
		const float Depth = Sea->DepthAt(C);
		Env.Depth = Depth * 0.01;
		Env.NormalX = Env.NormalY = 0.0;
		Env.NormalZ = 1.0;
		SurfaceZ = Level;
		BedZ = Level - Depth;
		return;
	}
	if (Detail == 1)
	{
		const FLyudSeaSample S = Sea->SampleSurface(C);
		Env.Depth = S.Depth * 0.01;
		Env.NormalX = S.Normal.X;
		Env.NormalY = S.Normal.Y;
		Env.NormalZ = S.Normal.Z;
		SurfaceZ = S.SurfaceZ;
		BedZ = Level - S.Depth;
		return;
	}
	// near: the surface under the bow, the stern and both sides gives the height and a slope averaged over the hull
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass < 0 ? D.BoatClass : BuiltClass);
	const FVector Fwd(FMath::Cos(B.Yaw), FMath::Sin(B.Yaw), 0.0);
	const FVector Right(-Fwd.Y, Fwd.X, 0.0);
	const float A = 0.4f * Def.Loa, W = 0.45f * Def.Beam;
	const float ZB = Sea->SurfaceZ(C + Fwd * A), ZS = Sea->SurfaceZ(C - Fwd * A);
	const float ZR = Sea->SurfaceZ(C + Right * W), ZL = Sea->SurfaceZ(C - Right * W);
	const float Depth = Sea->DepthAt(C);
	const FVector N = (FVector::UpVector - Fwd * ((ZB - ZS) / (2.f * A)) - Right * ((ZR - ZL) / (2.f * W))).GetSafeNormal();
	Env.Depth = Depth * 0.01;
	Env.NormalX = N.X;
	Env.NormalY = N.Y;
	Env.NormalZ = N.Z;
	SurfaceZ = 0.25f * (ZB + ZS + ZR + ZL);
	BedZ = Level - Depth;
}

void ALyudDinghy::FImpl::Place(ALyudDinghy& D)
{
	// never under the bed: aground or run up the beach it sits on the sand
	const float Draft = LyudSailKit::ClassDef(BuiltClass < 0 ? D.BoatClass : BuiltClass).HullDraft;
	const float Z = FMath::Max(SurfaceZ, BedZ + Draft);
	D.SetActorLocationAndRotation(FVector(DrawPos.X, DrawPos.Y, Z), FRotator(0.f, FMath::RadiansToDegrees(DrawYaw), 0.f));
}

void ALyudDinghy::FImpl::AfterStep(ALyudDinghy& D, float DeltaTime, bool bStepped)
{
	const LyudSail::FOutputs& O = B.Out;
	FLyudDinghyTelemetry& T = D.Tele;
	T.PosM = FVector2D(B.X, B.Y);
	T.YawRad = float(B.Yaw);
	T.SpeedMs = float(O.Speed);
	T.HeelDeg = FMath::RadiansToDegrees(float(O.Heel));
	T.TrueWindAngleRad = float(O.TrueWindAngle);
	T.TrueWindMs = float(O.TrueWindSpeed);
	T.DepthM = float(Env.Depth);
	T.Luff = float(O.MainLuff);
	T.State = int32(B.State);
	T.Events = StepEvents;
	T.Point = int32(O.Point);
	T.bPlaning = O.Planing > 0.5;
	if (!Hull || DeltaTime <= 0.f)
	{
		return;
	}
	// where it is drawn: the model's place, carried on by its speed between the steps of the lower rate
	DrawPos = FVector2D(B.X + B.VX * MidClock, B.Y + B.VY * MidClock) * 100.0;
	DrawYaw = float(B.Yaw + B.YawRate * MidClock);
	Place(D);
	if (D.Lod == ELyudDinghyLod::Far)
	{
		// one merged mesh: it only heels
		if (bStepped)
		{
			Hull->SetRelativeRotation(FRotator(0.f, 0.f, FMath::RadiansToDegrees(float(O.Heel))));
		}
	}
	else if (bStepped || D.Lod == ELyudDinghyLod::Near)
	{
		MoveParts(D, DeltaTime);
	}
	CrewTick(D, DeltaTime);
	MoveWake(D, DeltaTime);
	Sounds(D, DeltaTime);
	if (D.bHeroAboard)
	{
		HeroFrame(D, DeltaTime);
		FillHud(D, DeltaTime);
	}
	if (StepEvents != 0)
	{
		D.OnSailEvents.Broadcast(D, StepEvents);
	}
}

// ------------------------------------------------------------------ the hero at the helm

void ALyudDinghy::FImpl::ApplyAssists(ALyudDinghy& D)
{
	// the school's flags for any boat; the player's easy mode on top while he has the helm
	const int32 Easy = D.bHeroAboard ? CVarDinghyEasy.GetValueOnGameThread() : 0;
	B.Assist.bAutoSheet = bAssistSheet || Easy >= 2;
	B.Assist.bAutoHike = bAssistHike || Easy >= 1;
	B.Assist.bNoCapsize = bAssistNoCapsize || Easy >= 2;
}

void ALyudDinghy::FImpl::Say(const FString& Key, const TCHAR* Fallback, float Seconds)
{
	Hud.Hint = LyudSailHud::Word(Key, Fallback);
	Hud.HintLeft = Seconds;
}

bool ALyudDinghy::FImpl::MayLeave(ALyudDinghy& D)
{
	if (DinghyOver(B.State))
	{
		// Space is the righting key then
		return false;
	}
	if (D.CanLeave)
	{
		if (!D.CanLeave(D))
		{
			Say(TEXT("ui.sail.hud.cant_leave"), TEXT("Not here: sail back to the slip"), 3.f);
			return false;
		}
		return true;
	}
	if (Env.Depth > 1.1 || FMath::Abs(B.Out.Speed) > 1.5)
	{
		Say(TEXT("ui.sail.hud.cant_leave"), TEXT("Not here: sail back to the slip"), 3.f);
		return false;
	}
	return true;
}

bool ALyudDinghy::Board(AHeroCharacter* InHero, bool bCheckReach)
{
	FImpl& I = *Impl;
	if (!InHero || bHeroAboard || !I.Hull || !I.HeroSeat || InHero->IsSeated() || InHero->IsOnBike())
	{
		return false;
	}
	if (const ULyudSwimComponent* Swim = InHero->FindComponentByClass<ULyudSwimComponent>(); Swim && Swim->IsSwimming())
	{
		return false;
	}
	if (bCheckReach && FVector::Dist2D(InHero->GetActorLocation(), GetActorLocation()) > 350.f)
	{
		return false;
	}
	if (ALyudDinghy* Other = GDinghyHeroBoat.Get(); Other && Other != this && Other->bHeroAboard)
	{
		Other->HeroGone(false);
	}
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BoatClass);
	I.Hero = InHero;
	bHeroAboard = true;
	GDinghyHeroBoat = this;
	I.Tiller01 = 0.f;
	I.SheetWant = float(I.B.Sheet);
	I.BoardWant = float(I.B.Board);
	I.HikeWant = 0.f;
	I.SeatSide = float(I.SitSide);
	I.CapsizeAlpha = 0.f;
	I.AdviceNow = 0;
	I.AdviceHeld = 0.f;
	for (double& Said : I.AdviceSaid)
	{
		Said = -1000.0;
	}
	I.ApplyAssists(*this);
	// the seat is a frame of its own that the boat moves (from one side deck to the other on a tack, into the water on
	// a capsize): the hero sits on it; its origin is on the sole under the deck's inboard edge
	I.HeroFrame(*this, 0.f);
	const float SeatZ = FMath::Max(12.f, float(Def.HelmSeat.Z - 10.0 - Def.BodyOrigin.Z));
	InHero->SitOn(I.HeroSeat, FTransform::Identity, SeatZ);
	// stepping out goes over the side behind him
	InHero->SetStandUpOffset(FVector(-115.f, 0.f, 0.f));
	TWeakObjectPtr<ALyudDinghy> WeakThis(this);
	InHero->SeatMove = [WeakThis](const FVector2D& V)
	{
		if (ALyudDinghy* Self = WeakThis.Get())
		{
			Self->Impl->Move = V;
			Self->Impl->MoveFrame = GFrameCounter;
		}
	};
	InHero->SeatLeave = [WeakThis]()
	{
		ALyudDinghy* Self = WeakThis.Get();
		if (!Self || !Self->bHeroAboard)
		{
			return true;
		}
		if (!Self->Impl->MayLeave(*Self))
		{
			return false;
		}
		// the hero stands himself up after this returns (his hooks are his to clear)
		Self->HeroGone(false);
		return true;
	};
	I.HudWidget = SLyudSailHud::AddTo(this);
	if (APlayerController* PC = Cast<APlayerController>(InHero->GetController()); PC && PC->PlayerCameraManager)
	{
		I.Cam = Cast<ULyudSailCamera>(PC->PlayerCameraManager->AddNewCameraModifier(ULyudSailCamera::StaticClass()));
	}
	I.Hud.Keys = LyudSailHud::Word(TEXT("ui.sail.hud.keys"),
		TEXT("A / D helm   W / S sheet in / out (wheel: fine)   Q / E lean in / out   Z / X board up / down   LMB let the sheet fly   Alt bail   Space right the boat / step out"));
	I.Hud.KeysLeft = 16.f;
	I.Hud.Hint = FText::GetEmpty();
	I.Hud.HintLeft = 0.f;
	UE_LOG(LogLyudSailBoat, Display, TEXT("SAIL hero aboard %s %d at %s"), LyudSailKit::ClassDef(BoatClass).Name, SailNumber, *GetActorLocation().ToCompactString());
	return true;
}

bool ALyudDinghy::Leave(bool bForce)
{
	if (!bHeroAboard)
	{
		return true;
	}
	if (!bForce && !Impl->MayLeave(*this))
	{
		return false;
	}
	HeroGone(true);
	return true;
}

void ALyudDinghy::HeroGone(bool bStandHim)
{
	FImpl& I = *Impl;
	if (!bHeroAboard)
	{
		return;
	}
	bHeroAboard = false;
	if (GDinghyHeroBoat.Get() == this)
	{
		GDinghyHeroBoat.Reset();
	}
	SLyudSailHud::Remove(I.HudWidget);
	I.HudWidget.Reset();
	AHeroCharacter* H = I.Hero.Get();
	if (ULyudSailCamera* Cam = I.Cam.Get())
	{
		if (APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr; PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->RemoveCameraModifier(Cam);
		}
	}
	I.Cam.Reset();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	I.ApplyAssists(*this);
	I.Move = FVector2D::ZeroVector;
	I.Hero.Reset();
	if (bStandHim && H && H->IsSeated())
	{
		H->StandUp();
	}
	UE_LOG(LogLyudSailBoat, Display, TEXT("SAIL hero out of the boat at %s"), *GetActorLocation().ToCompactString());
}

LyudSail::FControls ALyudDinghy::FImpl::HeroControls(ALyudDinghy& D, float DeltaTime)
{
	LyudSail::FControls C = Last;
	const AHeroCharacter* H = Hero.Get();
	const APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr;
	const bool bInput = PC && !PC->IsMoveInputIgnored() && !Hud.bHidden;
	// the move keys held this frame (Enhanced Input calls the seat's hook only while one is down)
	const FVector2D In = bInput && MoveFrame + 1 >= GFrameCounter ? Move : FVector2D::ZeroVector;
	const int32 Easy = CVarDinghyEasy.GetValueOnGameThread();
	const LyudSail::FOutputs& O = B.Out;

	// the helm: A / D put it over at a hand's speed; let go, the water straightens it (faster the faster she goes)
	if (FMath::Abs(In.X) > 0.05f)
	{
		Tiller01 = FMath::FInterpConstantTo(Tiller01, FMath::Clamp(float(In.X), -1.f, 1.f), DeltaTime, 2.4f);
	}
	else
	{
		Tiller01 = FMath::FInterpConstantTo(Tiller01, 0.f, DeltaTime, 0.8f + 0.6f * FMath::Abs(float(O.Speed)));
	}
	// the sheet: W hauls in, S eases; the wheel a little at a time; the left button lets it fly while held
	SheetWant -= float(In.Y) * 0.5f * DeltaTime;
	if (bInput)
	{
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			SheetWant -= 0.03f;
		}
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			SheetWant += 0.03f;
		}
	}
	SheetWant = FMath::Clamp(SheetWant, 0.f, 1.f);
	const bool bDump = bInput && (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::Gamepad_RightTrigger));
	// the body: E leans out to windward, Q comes in
	if (bInput)
	{
		const float Lean = (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::Gamepad_RightShoulder) ? 1.f : 0.f)
			- (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::Gamepad_LeftShoulder) ? 1.f : 0.f);
		HikeWant = FMath::Clamp(HikeWant + Lean * 1.4f * DeltaTime, 0.f, 1.f);
		// the board: Z a notch up, X a notch down
		if (PC->WasInputKeyJustPressed(EKeys::Z) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Up))
		{
			BoardWant = FMath::Max(0.f, BoardWant - 0.5f);
		}
		if (PC->WasInputKeyJustPressed(EKeys::X) || PC->WasInputKeyJustPressed(EKeys::Gamepad_DPad_Down))
		{
			BoardWant = FMath::Min(1.f, BoardWant + 0.5f);
		}
	}
	if (Easy >= 1)
	{
		// the board minds the depth by itself: all the way in a metre of water, half in half a metre, up in less
		BoardWant = Env.Depth > 1.0 ? 1.f : (Env.Depth > 0.5 ? 0.5f : 0.f);
	}
	C.Rudder = Tiller01;
	C.Sheet = bDump ? 1.0 : double(SheetWant);
	C.JibSheet = -1.0;
	C.Hike = double(SitSide) * HikeWant;
	C.Board = BoardWant;
	C.bRight = bInput && DinghyOver(B.State) && (PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom));
	const bool bWet = O.Water > 0.12 && DinghyAfloat(B.State);
	C.bBail = (bInput && PC->IsInputKeyDown(EKeys::LeftAlt)) || (Easy >= 1 && bWet);
	return C;
}

void ALyudDinghy::FImpl::HeroFrame(ALyudDinghy& D, float DeltaTime)
{
	const LyudSailKit::FClassDef& Def = LyudSailKit::ClassDef(BuiltClass);
	const LyudSail::FOutputs& O = B.Out;
	// from one side deck to the other with the crew's side (a tack, a gybe): under the boom in about a second
	SeatSide = FMath::FInterpConstantTo(SeatSide, float(SitSide), DeltaTime, 2.2f);
	const float Cross = 1.f - SeatSide * SeatSide;
	const float FaceYaw = -62.f * SeatSide;        // on the starboard deck he faces to port and forward
	const FVector Face = FRotator(0.f, FaceYaw, 0.f).Vector();
	const float Out = FMath::Abs(Def.HelmSeat.Y) + 14.f * FMath::Abs(float(O.Hike));
	const FVector Pelvis(Def.HelmSeat.X, SeatSide * Out, Def.BodyOrigin.Z - 10.f * Cross);
	const FVector OnDeck = Hull->GetComponentTransform().TransformPosition(Pelvis + Face * 22.f);
	// capsized: in the water by the centreboard (the hull's bottom is on the side away from the mast), on the board to right her
	CapsizeAlpha = FMath::FInterpConstantTo(CapsizeAlpha, DinghyOver(B.State) ? 1.f : 0.f, DeltaTime, 1.6f);
	FVector Seat = OnDeck;
	if (CapsizeAlpha > 0.f)
	{
		const FVector Right(-FMath::Sin(DrawYaw), FMath::Cos(DrawYaw), 0.f);
		const float Bottom = O.Heel >= 0.0 ? -1.f : 1.f;
		const float Lift = B.State == LyudSail::EState::Righting ? 45.f + 40.f * float(O.Righting) : 0.f;
		const FVector InWater = FVector(DrawPos.X, DrawPos.Y, SurfaceZ - 95.f + Lift) + Right * (Bottom * (0.5f * Def.Beam + 55.f))
			+ FVector(FMath::Cos(DrawYaw), FMath::Sin(DrawYaw), 0.f) * Def.Board.X;
		Seat = FMath::Lerp(OnDeck, InWater, FMath::SmoothStep(0.f, 1.f, CapsizeAlpha));
	}
	const float BoatYawDeg = FMath::RadiansToDegrees(DrawYaw);
	HeroSeat->SetWorldLocationAndRotation(Seat, FRotator(0.f, BoatYawDeg + FaceYaw, 0.f));

	// the view: a third of the heel, capped, turned to wherever he looks (looking abeam it is a pitch); the tumble of a capsize
	if (ULyudSailCamera* View = Cam.Get())
	{
		const AHeroCharacter* H = Hero.Get();
		const float Look = H ? FMath::DegreesToRadians(FRotator::NormalizeAxis(H->GetControlRotation().Yaw - BoatYawDeg)) : 0.f;
		const float HeelDeg = FMath::RadiansToDegrees(float(O.Heel));
		const float Share = FMath::Clamp(0.35f * HeelDeg, -14.f, 14.f) * (1.f - CapsizeAlpha) + (HeelDeg >= 0.f ? 20.f : -20.f) * CapsizeAlpha * (1.f - float(O.Righting));
		View->WantRoll = Share * FMath::Cos(Look);
		View->WantPitch = -Share * FMath::Sin(Look) + 0.35f * FMath::RadiansToDegrees(float(O.Pitch)) * FMath::Cos(Look) * (1.f - CapsizeAlpha);
		View->WantDrop = 0.f;
	}

	// the tutor: the one thing a coach would call out, when it has held a moment, and not the same thing again soon
	const int32 Advice = bTutor ? int32(LyudSail::Advice(B, Env)) : 0;
	if (Advice != AdviceNow)
	{
		AdviceNow = Advice;
		AdviceHeld = 0.f;
	}
	else
	{
		AdviceHeld += DeltaTime;
	}
	if (Advice > 0 && Advice < 16 && AdviceHeld > 1.5f && Hud.HintLeft <= 0.f && Clock - AdviceSaid[Advice] > 20.0)
	{
		static const TCHAR* const English[] = { TEXT(""), TEXT("The sail is flapping: sheet in"), TEXT("Ease the sheet"),
			TEXT("Too close to the wind: bear away"), TEXT("In irons: tiller the other way and back her out"), TEXT("Lean out!"),
			TEXT("Shallow: board up"), TEXT("Board down, she is sliding sideways"), TEXT("By the lee: head up, or gybe"),
			TEXT("Climb on the centreboard and right her (hold Space)"), TEXT("Bail her out"), TEXT("Aground: board up and push off") };
		AdviceSaid[Advice] = Clock;
		const FString Key = FString(TEXT("sail.advice.")) + FString(UTF8_TO_TCHAR(LyudSail::AdviceName(LyudSail::EAdvice(Advice))));
		Say(Key, Advice < int32(UE_ARRAY_COUNT(English)) ? English[Advice] : nullptr, 4.f);
	}
}

void ALyudDinghy::FImpl::FillHud(ALyudDinghy& D, float DeltaTime)
{
	static const TCHAR* const PointKeys[] = { TEXT("sail.point.in_irons"), TEXT("sail.point.close_hauled"), TEXT("sail.point.close_reach"),
		TEXT("sail.point.beam_reach"), TEXT("sail.point.broad_reach"), TEXT("sail.point.run") };
	static const TCHAR* const PointEnglish[] = { TEXT("In irons"), TEXT("Close-hauled"), TEXT("Close reach"), TEXT("Beam reach"), TEXT("Broad reach"), TEXT("Running") };
	const LyudSail::FOutputs& O = B.Out;
	FLyudSailHudData& H = Hud;
	const int32 Point = FMath::Clamp(int32(O.Point), 0, 5);
	if (Point != H.Point || H.PointName.IsEmpty())
	{
		H.PointName = LyudSailHud::Word(PointKeys[Point], PointEnglish[Point]);
	}
	H.Point = Point;
	H.State = int32(B.State);
	H.TwaRad = float(O.TrueWindAngle);
	H.TwsMs = float(O.TrueWindSpeed);
	H.AwaRad = float(O.AppWindAngle);
	H.AwsMs = float(O.AppWindSpeed);
	H.SpeedMs = float(O.Speed);
	H.HeelDeg = FMath::RadiansToDegrees(float(O.Heel));
	H.Sheet = float(O.Sheet);
	H.Board = float(O.Board);
	H.Hike = FMath::Abs(float(O.Hike));
	H.DepthM = float(Env.Depth);
	H.Luff = float(O.MainLuff);
	H.TrimError = float(O.TrimError);
	H.Water = float(O.Water);
	H.Righting = float(O.Righting);
	H.bByTheLee = O.bByTheLee;
	H.bPlaning = O.Planing > 0.5;
	H.bShallow = DinghyAfloat(B.State) && Env.Depth < LyudSail::Draught(B) + 0.35;
	H.bEasySheet = B.Assist.bAutoSheet;
	H.BeatRad = float(LyudSail::BeatAngle(B.Class, O.TrueWindSpeed));
	H.PolarMs = float(LyudSail::PolarSpeed(B.Class, O.TrueWindAngle, O.TrueWindSpeed));
	H.IdealSheet = float(LyudSail::IdealSheet(B));
	H.HintLeft = FMath::Max(0.f, H.HintLeft - DeltaTime);
	H.KeysLeft = FMath::Max(0.f, H.KeysLeft - DeltaTime);
	if (H.Countdown > 0.f)
	{
		// the school sets it now and then; it runs by itself between
		H.Countdown = FMath::Max(0.f, H.Countdown - DeltaTime);
	}
	if (H.bHasMark)
	{
		const double DX = H.MarkM.X - B.X, DY = H.MarkM.Y - B.Y;
		H.MarkDistM = float(FMath::Sqrt(DX * DX + DY * DY));
		H.MarkBearingRad = float(LyudSail::WrapAngle(FMath::Atan2(DY, DX) - B.Yaw));
	}
	// nothing over a screen (the diary, the menu, a trade) or with the HUD off
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(&D);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(&D);
	const UWorld* World = D.GetWorld();
	const ULyudGameMenuSubsystem* Menu = World ? World->GetSubsystem<ULyudGameMenuSubsystem>() : nullptr;
	static IConsoleVariable* HudVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Hud"));
	H.bHidden = (Screen && Screen->IsOpen()) || (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || (HudVar && HudVar->GetInt() == 0);
	if (CVarDinghyDebug.GetValueOnGameThread() != 0)
	{
		H.Debug = FString::Printf(TEXT("%s  %s  v %.2f (polar %.2f) sog %.2f lee %.1f  heel %.1f pitch %.1f  twa %.0f tws %.1f  awa %.0f aws %.1f  boom %.0f sheet %.2f ideal %.2f trim %+.2f luff %.2f load %.0f N (%.2f) helm %+.2f  board %.2f hike %+.2f water %.2f plane %.2f  depth %.1f hs %.2f  advice %s"),
			LyudSailKit::ClassDef(BuiltClass).Name, ANSI_TO_TCHAR(LyudSail::StateName(B.State)), O.Speed, H.PolarMs, O.Sog, FMath::RadiansToDegrees(O.Leeway),
			H.HeelDeg, FMath::RadiansToDegrees(O.Pitch), FMath::RadiansToDegrees(O.TrueWindAngle), O.TrueWindSpeed,
			FMath::RadiansToDegrees(O.AppWindAngle), O.AppWindSpeed, FMath::RadiansToDegrees(O.Boom), O.Sheet, H.IdealSheet, O.TrimError,
			O.MainLuff, O.SheetLoad, O.SheetLoad01, O.HelmLoad, O.Board, O.Hike, O.Water, O.Planing, Env.Depth, Env.WaveHs,
			ANSI_TO_TCHAR(LyudSail::AdviceName(LyudSail::EAdvice(AdviceNow))));
	}
	else if (!H.Debug.IsEmpty())
	{
		H.Debug.Reset();
	}
}

// ------------------------------------------------------------------ sounds

void ALyudDinghy::FImpl::StopLoops(ALyudDinghy& D, float Fade)
{
	for (int32 I = 0; I < D.Loops.Num(); ++I)
	{
		if (UAudioComponent* Loop = D.Loops[I])
		{
			// ReleaseLoop turns auto-destroy on before the fade, so the component goes with it
			ULyudSoundscapeSubsystem::ReleaseLoop(Loop, Fade);
			D.Loops[I] = nullptr;
		}
		if (I < LyudSail::Sound::NumLoops)
		{
			LoopGain[I] = 0.f;
			LoopQuiet[I] = 0.f;
		}
	}
}

void ALyudDinghy::FImpl::Sounds(ALyudDinghy& D, float DeltaTime)
{
	if (D.Lod != ELyudDinghyLod::Near && D.Lod != ELyudDinghyLod::Mid)
	{
		return;
	}
	ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(&D);
	if (!Scape)
	{
		return;
	}
	const LyudSail::FOutputs& O = B.Out;
	const FVector At = D.GetActorLocation();
	const uint32 Ev = StepEvents;
	// the loud ones carry to the middle distance too
	if (Ev & LyudSail::EvCapsize)
	{
		Scape->PlayEffect(TEXT("sail_capsize"), At, 1.f);
		HitWaterIn = 0.9f;
	}
	if ((Ev & LyudSail::EvBoomSlam) && (Ev & LyudSail::EvGybe))
	{
		Scape->PlayEffect(TEXT("sail_gybe_slam"), At, FMath::Clamp(float(O.BoomSlamSpeed) / 3.f, 0.3f, 1.f));
	}
	if (D.Lod != ELyudDinghyLod::Near)
	{
		return;
	}
	if (HitWaterIn >= 0.f)
	{
		HitWaterIn -= DeltaTime;
		if (HitWaterIn < 0.f)
		{
			Scape->PlayEffect(TEXT("sail_hit_water"), At, 0.9f);
		}
	}
	if (Ev & LyudSail::EvTack)
	{
		Scape->PlayEffect(TEXT("sail_batten_pop"), At, 0.6f);
	}
	if (Ev & LyudSail::EvRighted)
	{
		Scape->PlayEffect(TEXT("sail_climb_in"), At, 0.9f);
	}
	if (Ev & LyudSail::EvGrounded)
	{
		Scape->PlayEffect(TEXT("sail_ground_scrape"), At, 0.9f);
	}
	if (Ev & LyudSail::EvHullSlap)
	{
		Scape->PlayEffect(TEXT("sail_wave_slap_large"), At, FMath::Clamp(0.3f + 0.7f * float(O.SlapStrength), 0.3f, 1.f));
	}
	// the board in its case
	if (FMath::Abs(float(O.Board) - BoardSeen) > 0.25f)
	{
		Scape->PlayEffect(O.Board > BoardSeen ? TEXT("sail_dagger_down") : TEXT("sail_dagger_up"), At, 0.7f);
		BoardSeen = float(O.Board);
	}
	// the sheet through the ratchet block: a click every ~20 cm hauled under load; let fly, it runs
	RunQuiet -= DeltaTime;
	const float Hauled = SheetSeen - float(O.Sheet);
	if (Hauled > 0.04f)
	{
		if (O.SheetLoad01 > 0.12)
		{
			Scape->PlayEffect(TEXT("sail_block_click"), At, 0.3f + 0.6f * float(O.SheetLoad01));
		}
		SheetSeen = float(O.Sheet);
	}
	else if (Hauled < -0.04f)
	{
		if (Hauled < -0.1f && RunQuiet <= 0.f)
		{
			Scape->PlayEffect(TEXT("sail_sheet_run"), At, 0.7f);
			RunQuiet = 1.5f;
		}
		SheetSeen = float(O.Sheet);
	}
	if (Last.bBail && O.Water > 0.02 && DinghyAfloat(B.State))
	{
		BailIn -= DeltaTime;
		if (BailIn <= 0.f)
		{
			Scape->PlayEffect(TEXT("sail_bail"), At, 0.8f);
			BailIn = 1.7f;
		}
	}

	// the nine loops: gains and pitches from the mixing header, eased; a loop is attached when it is first heard
	// and let go after two quiet seconds
	LyudSail::Sound::FTelemetry T;
	T.SpeedMs = FMath::Abs(float(O.Speed));
	T.Luff = float(O.MainLuff);
	T.HeelDeg = FMath::RadiansToDegrees(float(O.Heel));
	T.SheetLoad = float(O.SheetLoad01);
	T.bPlaning = O.Planing > 0.5;
	T.WindMs = float(O.AppWindSpeed);
	T.WaveM = float(Env.WaveHs);
	T.WaterAboard = float(O.Water);
	const bool bQuiet = !DinghyAfloat(B.State) && B.State != LyudSail::EState::Moored;
	const LyudSail::Sound::FLoopMix Mix = LyudSail::Sound::MixLoops(T);
	const float Vol = Scape->MasterVolume * Scape->EffectsVolume * (D.bHeroAboard ? 1.f : 0.7f);
	for (int32 I = 0; I < LyudSail::Sound::NumLoops && I < D.Loops.Num(); ++I)
	{
		// ashore or on her side only the cloth is heard
		const float Want = bQuiet && I > LyudSail::Sound::LuffHard ? 0.f : Mix.Gain[I];
		LoopGain[I] = FMath::FInterpTo(LoopGain[I], Want, DeltaTime, 7.f);
		UAudioComponent* Loop = D.Loops[I];
		if (!Loop && Want > 0.03f)
		{
			Loop = Scape->AttachLoop(FName(LyudSail::Sound::LoopPool(I)), Hull, 0.01f);
			D.Loops[I] = Loop;
			LoopQuiet[I] = 0.f;
		}
		if (Loop)
		{
			Loop->SetVolumeMultiplier(FMath::Max(LoopGain[I] * Vol, 0.001f));
			Loop->SetPitchMultiplier(Mix.Pitch[I]);
			LoopQuiet[I] = Want < 0.01f ? LoopQuiet[I] + DeltaTime : 0.f;
			if (LoopQuiet[I] > 2.f)
			{
				ULyudSoundscapeSubsystem::ReleaseLoop(Loop, 0.2f);
				D.Loops[I] = nullptr;
			}
		}
	}
	// the header's own one-shots (the sail filling, creaks, small slaps, spray, the rudder's knock); the bow's big
	// slaps come from the model's event above
	if (!bQuiet)
	{
		const float R1 = Rng.FRand(), R2 = Rng.FRand(), R3 = Rng.FRand();
		const unsigned Bits = LyudSail::Sound::StepEvents(SoundState, T, DeltaTime, R1, R2, R3) & ~unsigned(LyudSail::Sound::EvWaveSlapLarge);
		for (int32 I = 0; Bits != 0 && I < LyudSail::Sound::NumEvents; ++I)
		{
			const unsigned Bit = 1u << I;
			if (Bits & Bit)
			{
				const float Gain = Bit == LyudSail::Sound::EvFill ? LyudSail::Sound::FillGain(T.WindMs)
					: (Bit == LyudSail::Sound::EvWaveSlapSmall || Bit == LyudSail::Sound::EvSpray) ? LyudSail::Sound::SlapGain(T) : 0.55f;
				Scape->PlayEffect(FName(LyudSail::Sound::EventPool(Bit)), At, Gain);
			}
		}
	}
}
