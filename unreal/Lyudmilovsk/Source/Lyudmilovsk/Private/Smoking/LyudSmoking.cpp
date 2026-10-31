#include "Smoking/LyudSmoking.h"

#include "Audio/LyudSoundscape.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Smoking/SmokingKit.h"
#include "UI/LyudGameScreen.h"
#include "UnrealClient.h"

const FName ULyudSmoking::PackId(TEXT("cigarettes"));
const FName ULyudSmoking::LighterId(TEXT("lighter"));

namespace
{
	TAutoConsoleVariable<float> CVarCoughAfter(TEXT("lyud.Smoke.CoughAfter"), 3.2f,
		TEXT("Seconds of one drag after which the hero coughs."));
	TAutoConsoleVariable<float> CVarBurnMinutes(TEXT("lyud.Smoke.BurnMinutes"), 7.f,
		TEXT("Minutes a lit cigarette smoulders down by itself between drags."));

	constexpr int32 MaxButts = 10;

	// ---- the hand's poses (look frame: forward, right, up from the eyes; the palm's frame: X the fingers, Z out of the palm, +Y the little finger)

	/** Down at the hip, out of sight: where things come from and go back to. */
	FHeroHoldPose Pocket()
	{
		FHeroHoldPose P;
		P.Pos = FVector(10.f, 24.f, -72.f);
		P.FingerDir = FVector(0.3f, 0.f, -1.f).GetSafeNormal();
		P.PalmNormal = FVector(0.f, -1.f, 0.f);
		P.Curl = 1.f;
		P.BlendTime = 0.45f;
		return P;
	}

	/** The cigarette between the index and middle fingers near the knuckles, the filter out of the palm's side, the burning end
	 *  out of the back of the hand. */
	FTransform CigInPalm()
	{
		return FTransform(FRotationMatrix::MakeFromXZ(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f)).ToQuat(), FVector(5.2f, -1.05f, 1.7f));
	}

	/** Held low on the right, the fingers up, the palm turned in: the cigarette sticks out across, to the right and a little up (seen
	 *  along its length, not end-on behind the fingers). */
	FHeroHoldPose CigCarry()
	{
		FHeroHoldPose P;
		// (high enough to be seen looking straight ahead: the view's lower edge is ~29 degrees down)
		P.Pos = FVector(30.f, 13.f, -14.f);
		P.FingerDir = FVector(0.25f, -0.1f, 1.f).GetSafeNormal();
		P.PalmNormal = FVector(-0.35f, -0.9f, -0.25f).GetSafeNormal();
		P.Curl = 0.32f;
		P.PitchFollow = 0.35f;
		P.BlendTime = 0.5f;
		return P;
	}

	/** At the lips: the palm toward the mouth, the fingers up, the filter between the lips. */
	FHeroHoldPose CigMouth()
	{
		FHeroHoldPose P = CigCarry();
		P.FingerDir = FVector(0.15f, 0.f, 1.f).GetSafeNormal();
		P.PalmNormal = FVector(-1.f, -0.1f, 0.12f).GetSafeNormal();
		return P;
	}

	/** The pack standing on the palm, its front (the warning) toward the wrist and the eyes, the fingers closed round its back. */
	FTransform PackInPalm()
	{
		return FTransform(FRotationMatrix::MakeFromXZ(FVector(-1.f, 0.f, 0.f), FVector(0.f, 0.f, 1.f)).ToQuat(), FVector(1.f, 0.f, 0.9f));
	}

	/** Held up in front, the palm up and turned a little toward the face, so the pack stands on it facing the eyes. */
	FHeroHoldPose PackShow()
	{
		FHeroHoldPose P;
		P.Pos = FVector(25.f, 3.f, -6.f);
		// the palm tipped forward a little: the pack's top leans away, its front stands up toward the eyes
		P.FingerDir = FVector(1.f, -0.15f, -0.3f).GetSafeNormal();
		P.PalmNormal = FVector(0.3f, -0.1f, 1.f).GetSafeNormal();
		P.Curl = 1.f;
		P.PitchFollow = 0.3f;
		P.BlendTime = 0.5f;
		return P;
	}

	/** The palm turned back toward the face, the pack's top (and the shaken-up cigarette's filter) to the lips. */
	FHeroHoldPose PackMouth()
	{
		FHeroHoldPose P = PackShow();
		P.FingerDir = FVector(0.6f, 0.f, 0.8f).GetSafeNormal();
		P.PalmNormal = FVector(-0.8f, 0.f, 0.6f).GetSafeNormal();
		return P;
	}

	/** The lighter upright in the fist across the fingers, its hood out of the fist's top (checked in the offscreen shots: the
	 *  palm frame's +Y is up when the hand holds it out under the cigarette). */
	FTransform LighterInPalm()
	{
		return FTransform(FRotationMatrix::MakeFromZX(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f)).ToQuat(),
			FVector(2.5f, -SmokingKit::LighterHalf.Z, 0.9f + SmokingKit::LighterHalf.X));
	}

	/** Under the tip: the palm to the left, the fingers ahead round the lighter. */
	FHeroHoldPose LighterMouth()
	{
		FHeroHoldPose P;
		P.Pos = FVector(25.f, 5.f, -18.f);
		P.FingerDir = FVector(1.f, 0.f, 0.05f).GetSafeNormal();
		P.PalmNormal = FVector(0.f, -1.f, 0.f);
		P.Curl = 1.f;
		P.BlendTime = 0.45f;
		return P;
	}

	FQuat PoseQuat(const FHeroHoldPose& P)
	{
		return FRotationMatrix::MakeFromXZ(P.FingerDir, P.PalmNormal).ToQuat();
	}

	/**
	 * ULyudHandUse brings a thing's mouth point to the lips (LipsInLook) with the palm turned as the Mouth pose says. For a point P of the
	 * thing to arrive at Target instead (the lighter's flame under a cigarette's tip), its mouth point must be this.
	 */
	FVector MouthPointFor(const FHeroHoldPose& Mouth, const FTransform& InPalm, const FVector& P, const FVector& Target)
	{
		const FQuat Qa = PoseQuat(Mouth) * InPalm.GetRotation();
		return P - Qa.UnrotateVector(Target - ULyudHandUse::LipsInLook());
	}
}

// ------------------------------------------------------------------ saved state

ULyudSmokingState* ULyudSmokingState::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudSmokingState>() : nullptr;
}

void ULyudSmokingState::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Smoking"), this);
	}
}

// ------------------------------------------------------------------ the subsystem

ULyudSmoking* ULyudSmoking::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudSmoking>() : nullptr;
}

bool ULyudSmoking::HasHand(const UObject* WorldContext)
{
	const ULyudSmoking* S = Get(WorldContext);
	return S && S->Step != EStep::None;
}

void ULyudSmoking::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// every level start is a new game unless a load restores the state right after (as the inventory)
	if (ULyudSmokingState* State = ULyudSmokingState::Get(&InWorld))
	{
		State->Reset();
	}
	SpawnTerrace();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*)
		{
			const ULyudSmokingState* State = ULyudSmokingState::Get(this);
			if (State && State->bTerraceTaken)
			{
				if (TerracePack.IsValid()) { TerracePack->Destroy(); }
				if (TerraceLighter.IsValid()) { TerraceLighter->Destroy(); }
			}
		});
	}
}

void ULyudSmoking::Deinitialize()
{
	for (FFlying& B : Butts)
	{
		if (B.Butt.IsValid()) { B.Butt->Destroy(); }
	}
	Butts.Reset();
	Super::Deinitialize();
}

int32 ULyudSmoking::PackLeft() const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || Inv->Count(PackId) < 1)
	{
		return 0;
	}
	const float* Share = Inv->Started.Find(PackId);
	return Share ? FMath::Clamp(FMath::RoundToInt(*Share * SmokingKit::PerPack), 1, SmokingKit::PerPack) : SmokingKit::PerPack;
}

void ULyudSmoking::TakeOneFromPack()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const int32 Was = PackLeft();
	if (!Inv || Was < 1)
	{
		return;
	}
	if (Was == 1)
	{
		// the last one: the empty pack is crumpled and gone
		Inv->Remove(PackId);
	}
	else
	{
		Inv->Started.Add(PackId, float(Was - 1) / SmokingKit::PerPack);
	}
	UE_LOG(LogTemp, Display, TEXT("SMOKE took one, pack left %d, packs %d"), PackLeft(), Inv->Count(PackId));
}

void ULyudSmoking::ReturnOneToPack()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	if (Inv->Count(PackId) < 1)
	{
		Inv->Add(PackId);
		Inv->Started.Add(PackId, 1.f / SmokingKit::PerPack);
	}
	else if (const int32 Now = PackLeft() + 1; Now >= SmokingKit::PerPack)
	{
		Inv->Started.Remove(PackId);
	}
	else
	{
		Inv->Started.Add(PackId, float(Now) / SmokingKit::PerPack);
	}
	UE_LOG(LogTemp, Display, TEXT("SMOKE put back, pack left %d"), PackLeft());
}

FTransform ULyudSmoking::LookFrame() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Hero)
	{
		return FTransform::Identity;
	}
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	const float Yaw = PC ? PC->GetControlRotation().Yaw : Hero->GetActorRotation().Yaw;
	return FTransform(FRotator(0.f, Yaw, 0.f), Hero->EyeLocation());
}

FTransform ULyudSmoking::MouthInLook() const
{
	// exactly where ULyudHandUse holds the cigarette at the lips: its mouth end (the origin) on the lips, turned as CigMouth and the grip turn it
	return FTransform(PoseQuat(CigMouth()) * CigInPalm().GetRotation(), ULyudHandUse::LipsInLook());
}

void ULyudSmoking::Sound(FName Pool, float Volume)
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, LookFrame().TransformPosition(ULyudHandUse::LipsInLook()), Volume);
	}
}

void ULyudSmoking::Say(const TCHAR* Key, float Seconds)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, LyudText::Get(Key));
		SayLeft = Seconds;
	}
}

ALyudSmokeFx* ULyudSmoking::Fx()
{
	if (!SmokeFx.IsValid())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SmokeFx = GetWorld()->SpawnActor<ALyudSmokeFx>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}
	return SmokeFx.Get();
}

void ULyudSmoking::Go(EStep Next)
{
	UE_LOG(LogTemp, Display, TEXT("SMOKE step %d -> %d (lit %d left %.2f)"), (int32)Step, (int32)Next, bLit ? 1 : 0, Left);
	Step = Next;
	StepTime = 0.f;
}

// ------------------------------------------------------------------ taking one out

bool ULyudSmoking::TakeOut()
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	ULyudHandUse* Hand = ULyudHandUse::Get(World);
	if (Step != EStep::None || !Hero || !Hand || Hero->IsHidden() || UHeroReachComponent::Get(Hero)->IsBusy())
	{
		return false;
	}
	if (PackLeft() < 1)
	{
		Say(TEXT("ui.smoke.empty"), 2.5f);
		return false;
	}
	bLit = false;
	Left = 1.f;
	Glow = 0.f;
	DragTime = 0.f;
	HoldPack();
	return Pack.IsValid();
}

void ULyudSmoking::HoldPack()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector At = LookFrame().GetLocation() - FVector(0.f, 0.f, 80.f);
	ACigarettePack* P = World->SpawnActor<ACigarettePack>(At, FRotator::ZeroRotator, Params);
	if (!P)
	{
		return;
	}
	const int32 N = PackLeft();
	P->SetLeft(N);
	P->SetLid(0.f);
	Pack = P;
	FLyudHandThing T;
	T.Actor = P;
	T.Item = PackId;
	T.InPalm = PackInPalm();
	FReachPrim Box;
	Box.Kind = FReachPrim::EKind::Box;
	Box.Xf = FTransform(FVector(0.f, 0.f, SmokingKit::PackH / 2.f));
	Box.Extent = FVector(SmokingKit::PackD, SmokingKit::PackW, SmokingKit::PackH) / 2.f;
	T.Shape.Prims.Add(Box);
	T.MouthFull = T.MouthEmpty = ACigarettePack::NextMouthEnd(N, 3.f);
	T.Carry = PackShow();
	T.Mouth = PackMouth();
	T.Small = T.Big = T.SteadyEach = 0.f;
	T.SmallAtLips = T.BigAtLips = 0.3f;
	T.ToMouth = 0.5f;
	T.FromMouth = 0.5f;
	T.Push = 0.2f;
	T.Prompt = LyudText::Get(TEXT("ui.smoke.hint_unlit"));
	TWeakObjectPtr<ULyudSmoking> Self = this;
	T.OnUse = [Self](ELyudMouthUse, float, float) { if (Self.IsValid()) { Self->bAtLipsEvent = true; } };
	T.OnPutAway = [Self](float)
	{
		ULyudSmoking* S = Self.Get();
		if (S && !S->bInternal)
		{
			// Q or the diary before the lips had one: nothing taken
			S->Go(EStep::None);
		}
	};
	bAtLipsEvent = false;
	if (ULyudHandUse::Get(World)->Hold(MoveTemp(T)))
	{
		Go(EStep::PackUp);
	}
	else
	{
		P->Destroy();
	}
}

// ------------------------------------------------------------------ the cigarette in the hand

FLyudHandThing ULyudSmoking::CigaretteThing(bool bFromMouth)
{
	FLyudHandThing T;
	T.Item = PackId;
	T.InPalm = CigInPalm();
	T.Shape.AddCapsule(FVector(SmokingKit::CigL / 2.f, 0.f, 0.f), FVector::ForwardVector, SmokingKit::CigL / 2.f - SmokingKit::CigR, SmokingKit::CigR);
	T.MouthFull = T.MouthEmpty = FVector::ZeroVector;
	T.Carry = bFromMouth ? Pocket() : CigCarry();
	T.Mouth = CigMouth();
	// a click only takes it to the lips; a drag (hold) and drawing on (holding on) burn it down
	T.Small = 0.f;
	T.Big = bLit ? 0.05f : 0.f;
	T.SteadyEach = bLit ? 0.035f : 0.f;
	T.SteadyEvery = 0.45f;
	T.SmallAtLips = 0.45f;
	T.BigAtLips = 0.5f;
	T.ToMouth = 0.38f;
	T.FromMouth = 0.45f;
	T.Push = 0.3f;
	T.Left = FMath::Max(Left, 0.03f);
	T.bHiddenTillLips = bFromMouth;
	T.Prompt = LyudText::Get(bLit ? TEXT("ui.smoke.hint_lit") : TEXT("ui.smoke.hint_unlit"));
	TWeakObjectPtr<ULyudSmoking> Self = this;
	T.OnUse = [Self](ELyudMouthUse Kind, float Took, float Rest)
	{
		ULyudSmoking* S = Self.Get();
		if (!S)
		{
			return;
		}
		S->bAtLipsEvent = true;
		if (S->Step == EStep::FromMouth)
		{
			// the hand only came for it: nothing smoked
			if (FLyudHandThing* Th = ULyudHandUse::Get(S)->Edit()) { Th->Left += Took; }
			return;
		}
		S->Left = Rest;
		S->bDrag = S->bLit && Kind != ELyudMouthUse::Small;
		if (S->bDrag && Kind == ELyudMouthUse::Big)
		{
			S->Sound(TEXT("cig_drag"), 0.8f);
		}
	};
	T.OnAtLips = [Self](float Dt)
	{
		ULyudSmoking* S = Self.Get();
		if (S && S->bDrag)
		{
			S->DragTime += Dt;
		}
	};
	T.OnUsedUp = [Self]()
	{
		// burnt to the filter at the lips: flicked away
		if (ULyudSmoking* S = Self.Get())
		{
			S->ThrowButt(S->LastCigXf, true);
			S->Go(EStep::None);
		}
	};
	T.OnPutAway = [Self](float Rest)
	{
		ULyudSmoking* S = Self.Get();
		if (!S || S->bInternal)
		{
			return;
		}
		if (S->bLit)
		{
			S->ThrowButt(S->LastCigXf, true);
		}
		else
		{
			S->ReturnOneToPack();
		}
		S->Go(EStep::None);
	};
	return T;
}

void ULyudSmoking::HoldCigarette(bool bFromMouth)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACigaretteItem* C = World->SpawnActor<ACigaretteItem>(LookFrame().GetLocation() - FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, Params);
	if (!C)
	{
		Go(EStep::None);
		return;
	}
	C->SetLeft(Left);
	C->SetGlow(Glow);
	Cig = C;
	FLyudHandThing T = CigaretteThing(bFromMouth);
	T.Actor = C;
	bAtLipsEvent = false;
	if (!ULyudHandUse::Get(World)->Hold(MoveTemp(T)))
	{
		C->Destroy();
		Go(EStep::None);
		return;
	}
	if (bFromMouth)
	{
		// up to the lips at once, to take it
		ULyudHandUse::Get(World)->Press();
		Go(EStep::FromMouth);
	}
	else
	{
		Go(EStep::Held);
	}
}

void ULyudSmoking::HoldLighter()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALighterItem* L = World->SpawnActor<ALighterItem>(LookFrame().GetLocation() - FVector(0.f, 0.f, 80.f), FRotator::ZeroRotator, Params);
	if (!L)
	{
		return;
	}
	Lighter = L;
	FLyudHandThing T;
	T.Actor = L;
	T.Item = LighterId;
	T.InPalm = LighterInPalm();
	FReachPrim Box;
	Box.Kind = FReachPrim::EKind::Box;
	Box.Xf = FTransform(FVector(0.f, 0.f, SmokingKit::LighterHalf.Z));
	Box.Extent = SmokingKit::LighterHalf;
	T.Shape.Prims.Add(Box);
	T.Carry = Pocket();
	T.Mouth = LighterMouth();
	// the flame's top just under and beside the cigarette's end, where it rests in the lips (not straight under it: from the eyes the
	// cigarette itself would hide the flame)
	const FTransform M = MouthInLook();
	const FVector TipLook = M.TransformPosition(FVector(SmokingKit::Filter + SmokingKit::Stub + (SmokingKit::Paper - SmokingKit::Stub) * Left + SmokingKit::Tip * 0.5f, 0.f, 0.f));
	T.MouthFull = T.MouthEmpty = MouthPointFor(T.Mouth, T.InPalm, ALighterItem::FlamePoint(), TipLook + FVector(0.4f, -0.9f, -0.5f));
	T.Small = T.Big = T.SteadyEach = 0.f;
	T.SteadyEvery = 10.f;
	T.BigAtLips = 0.2f;
	T.ToMouth = 0.55f;
	T.FromMouth = 0.5f;
	T.Push = 0.f;
	T.Prompt = LyudText::Get(TEXT("ui.smoke.hint_unlit"));
	TWeakObjectPtr<ULyudSmoking> Self = this;
	T.OnUse = [Self](ELyudMouthUse, float, float) { if (Self.IsValid()) { Self->bAtLipsEvent = true; } };
	T.OnPutAway = [Self](float)
	{
		ULyudSmoking* S = Self.Get();
		if (S && !S->bInternal)
		{
			// taken away mid-way (Q, the diary): the cigarette drops from the lips back into the pack
			if (S->MouthCig.IsValid()) { S->MouthCig->Destroy(); }
			S->bLit ? S->ThrowButt(S->LastCigXf, true) : S->ReturnOneToPack();
			S->Go(EStep::None);
		}
	};
	bAtLipsEvent = false;
	ULyudHandUse* Hand = ULyudHandUse::Get(World);
	if (Hand->Hold(MoveTemp(T)))
	{
		Hand->Press();
	}
}

// ------------------------------------------------------------------ the cigarette in the lips

void ULyudSmoking::PlaceMouthCig(float DeltaTime)
{
	ACigaretteItem* C = MouthCig.Get();
	if (!C)
	{
		return;
	}
	const FTransform Frame = LookFrame();
	const FTransform Rest = MouthInLook() * Frame;
	FTransform Xf = Rest;
	if (MouthBlend < 1.f)
	{
		// from where the lips took it (out of the pack) into their hold
		MouthBlend = FMath::Min(1.f, MouthBlend + DeltaTime / 0.3f);
		const float K = FMath::SmoothStep(0.f, 1.f, MouthBlend);
		const FTransform From = MouthFrom * Frame;
		Xf.SetLocation(FMath::Lerp(From.GetLocation(), Rest.GetLocation(), K));
		Xf.SetRotation(FQuat::Slerp(From.GetRotation(), Rest.GetRotation(), K).GetNormalized());
	}
	C->SetActorTransform(Xf);
	LastCigXf = Xf;
}

// ------------------------------------------------------------------ smoke, a drag, a cough

void ULyudSmoking::Exhale(float Amount)
{
	const FTransform Frame = LookFrame();
	const FVector Mouth = Frame.TransformPosition(ULyudHandUse::LipsInLook() + FVector(3.f, -1.5f, 0.f));
	Fx()->Breath(Mouth, Frame.TransformVectorNoScale(FVector(1.f, 0.f, -0.12f)), Amount);
	Sound(TEXT("smoke_exhale"), FMath::Lerp(0.45f, 0.9f, Amount));
	UE_LOG(LogTemp, Display, TEXT("SMOKE exhale %.2f"), Amount);
}

void ULyudSmoking::Cough()
{
	UE_LOG(LogTemp, Display, TEXT("SMOKE cough after %.1f s"), DragTime);
	DragTime = 0.f;
	bDrag = false;
	CoughTime = 0.f;
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->Interrupt();
	}
	static int32 Which = 0;
	Which = Which % 3 + 1;
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		if (USoundBase* V = LyudText::LoadVoice(FString::Printf(TEXT("line.hero.cough.%d"), Which)))
		{
			S->PlayVoiceAt(V, LookFrame().TransformPosition(ULyudHandUse::LipsInLook()), 1.f);
		}
	}
}

void ULyudSmoking::Smoke(float DeltaTime)
{
	const ACigaretteItem* C = MouthCig.IsValid() ? MouthCig.Get() : Cig.Get();
	ULyudHandUse* Hand = ULyudHandUse::Get(this);
	const bool bAtLips = Hand && Hand->IsAtLips() && Step == EStep::Held;
	// the ember: bright on a drag, a slow breathing smoulder between, nothing unlit
	float Want = 0.f;
	if (bLit)
	{
		Want = bAtLips && bDrag ? 1.f : 0.3f + 0.06f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.3f);
	}
	Glow = FMath::FInterpTo(Glow, Want, DeltaTime, Want > Glow ? 5.f : 1.5f);
	if (Cig.IsValid()) { Cig->SetGlow(Glow); }
	if (MouthCig.IsValid()) { MouthCig->SetGlow(Glow); }

	if (bLit && Step == EStep::Held)
	{
		// it smoulders down by itself between drags
		if (FLyudHandThing* T = Hand ? Hand->Edit() : nullptr)
		{
			T->Left = FMath::Max(0.02f, T->Left - DeltaTime / (60.f * FMath::Max(CVarBurnMinutes.GetValueOnGameThread(), 0.1f)));
			Left = T->Left;
		}
		if (Cig.IsValid() && FMath::Abs(Cig->GetLeft() - Left) > 0.002f)
		{
			Cig->SetLeft(Left);
		}
		// a drag held too long
		if (bAtLips && bDrag && DragTime > CVarCoughAfter.GetValueOnGameThread())
		{
			Cough();
		}
		// off the lips after a drag: the breath goes out a moment later
		if (bWasAtLips && !bAtLips && DragTime > 0.05f && CoughTime < 0.f)
		{
			ExhaleIn = 0.3f;
			ExhaleAmount = FMath::Clamp(DragTime / 2.5f, 0.15f, 1.f);
			DragTime = 0.f;
			bDrag = false;
		}
		else if (bWasAtLips && !bAtLips)
		{
			bDrag = false;
		}
		bWasAtLips = bAtLips;
	}
	if (ExhaleIn >= 0.f)
	{
		ExhaleIn -= DeltaTime;
		if (ExhaleIn < 0.f)
		{
			Exhale(ExhaleAmount);
		}
	}
	if (CoughTime >= 0.f)
	{
		// three coughs, each a burst of smoke and a jolt of the head
		const float Was = CoughTime;
		CoughTime += DeltaTime;
		for (const float At : { 0.05f, 0.5f, 0.95f })
		{
			if (Was < At && CoughTime >= At)
			{
				Exhale(0.45f);
			}
		}
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			// a sum of zero over the cough: the view comes back where it was
			const float Jolt = 9.f * FMath::Cos(CoughTime * 2.f * PI / 0.45f) * DeltaTime * FMath::Max(0.f, 1.f - CoughTime / 1.35f);
			PC->AddPitchInput(Jolt);
		}
		if (CoughTime > 1.4f)
		{
			CoughTime = -1.f;
		}
	}
	// the thin stream off the burning end
	if (bLit && C && !(bAtLips && bDrag))
	{
		StreamTimer -= DeltaTime;
		if (StreamTimer <= 0.f)
		{
			StreamTimer = 0.085f;
			const FVector Tip = C->TipLocation();
			Fx()->Emit(Tip, FVector(FMath::FRandRange(-2.f, 2.f), FMath::FRandRange(-2.f, 2.f), 9.f), 0.5f, 5.f, 0.16f, 2.6f, 11.f);
		}
	}
}

// ------------------------------------------------------------------ butts

void ULyudSmoking::ThrowButt(const FTransform& From, bool bLitEnd)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACigaretteButt* B = World->SpawnActor<ACigaretteButt>(From.GetLocation(), From.Rotator(), Params);
	if (!B)
	{
		return;
	}
	if (Butts.Num() >= MaxButts)
	{
		if (Butts[0].Butt.IsValid()) { Butts[0].Butt->Destroy(); }
		Butts.RemoveAt(0);
	}
	const FTransform Frame = LookFrame();
	FFlying F;
	F.Butt = B;
	F.Pos = From.GetLocation();
	// flicked ahead and to the right, a little up
	F.Vel = Frame.TransformVectorNoScale(FVector(380.f, 90.f, 160.f));
	F.Spin = FRotator(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-600.f, 600.f), 0.f);
	F.Smoke = bLitEnd ? 4.f : 0.f;
	Butts.Add(F);
	Sound(TEXT("cig_putout"), 0.25f);
	bLit = false;
	Glow = 0.f;
	UE_LOG(LogTemp, Display, TEXT("SMOKE butt thrown from %s"), *F.Pos.ToCompactString());
}

void ULyudSmoking::TickButts(float DeltaTime)
{
	UWorld* World = GetWorld();
	for (int32 I = Butts.Num() - 1; I >= 0; --I)
	{
		FFlying& F = Butts[I];
		ACigaretteButt* B = F.Butt.Get();
		if (!B)
		{
			Butts.RemoveAt(I);
			continue;
		}
		F.Age += DeltaTime;
		if (!F.bLanded)
		{
			const FVector Next = F.Pos + F.Vel * DeltaTime;
			F.Vel.Z -= 980.f * DeltaTime;
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ButtFall), false, UGameplayStatics::GetPlayerPawn(World, 0));
			if (World->LineTraceSingleByChannel(Hit, F.Pos, Next, ECC_Visibility, Q))
			{
				// it lies where it fell, along the ground
				F.bLanded = true;
				const FVector N = Hit.ImpactNormal;
				const FVector Along = FVector::VectorPlaneProject(F.Vel, N).GetSafeNormal(UE_SMALL_NUMBER, FVector::CrossProduct(N, FVector::RightVector).GetSafeNormal());
				B->SetActorLocationAndRotation(Hit.ImpactPoint + N * 0.05f, FRotationMatrix::MakeFromXZ(Along, N).Rotator());
				Sound(TEXT("cig_putout"), 0.35f);
				UE_LOG(LogTemp, Display, TEXT("SMOKE butt landed at %s"), *Hit.ImpactPoint.ToCompactString());
			}
			else
			{
				F.Pos = Next;
				B->SetActorLocationAndRotation(F.Pos, B->GetActorRotation() + F.Spin * DeltaTime);
			}
			if (F.Age > 6.f)
			{
				B->Destroy();
				continue;
			}
		}
		if (F.Smoke > 0.f)
		{
			// a last wisp as it goes out
			F.Smoke -= DeltaTime;
			if (FMath::Frac(F.Age * 7.f) < DeltaTime * 7.f)
			{
				Fx()->Emit(B->GetActorLocation() + B->GetActorForwardVector() * 2.5f, FVector(0.f, 0.f, 6.f), 0.4f, 4.f, 0.12f * FMath::Min(1.f, F.Smoke), 2.f, 9.f);
			}
		}
	}
}

// ------------------------------------------------------------------ the terrace's pack

void ULyudSmoking::SpawnTerrace()
{
	UWorld* World = GetWorld();
	const ULyudSmokingState* State = ULyudSmokingState::Get(World);
	if (!World || !State || State->bTerraceTaken || !World->GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	// on the terrace out of the club's forecourt (the door's face is at y 790), left of the path, on whatever is there (traced from
	// under the canopy's height)
	const FVector Where(-250.f, 1650.f, 0.f);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(SmokeTerrace), false);
	if (!World->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 150.f), Where - FVector(0.f, 0.f, 600.f), ECC_Visibility, Q))
	{
		UE_LOG(LogTemp, Display, TEXT("SMOKE no ground for the terrace pack"));
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// lying on its back, the lid half open, a few gone
	const FVector At = Hit.ImpactPoint + FVector(0.f, 0.f, SmokingKit::PackD / 2.f + 0.05f);
	ACigarettePack* P = World->SpawnActor<ACigarettePack>(At, FRotator(90.f, 35.f, 0.f), Params);
	if (P)
	{
		P->SetLeft(ULyudSmokingState::TerraceLeft);
		P->SetLid(0.35f);
		P->SetActorLocation(At - P->GetActorUpVector() * SmokingKit::PackH / 2.f + FVector(0.f, 0.f, 0.f));
		TerracePack = P;
	}
	ALighterItem* L = World->SpawnActor<ALighterItem>(Hit.ImpactPoint + FVector(4.f, 9.f, SmokingKit::LighterHalf.X + 0.05f), FRotator(90.f, 110.f, 0.f), Params);
	if (L)
	{
		L->SetActorLocation(L->GetActorLocation() - L->GetActorUpVector() * SmokingKit::LighterHalf.Z);
		TerraceLighter = L;
	}
	UE_LOG(LogTemp, Display, TEXT("SMOKE terrace pack at %s"), *At.ToCompactString());
}

void ULyudSmoking::TickTerrace()
{
	ACigarettePack* P = TerracePack.Get();
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	bool bNear = false;
	if (P && Hero && PC && UI && !Hero->IsHidden() && !UI->IsOpen())
	{
		const FVector Center = P->GetActorLocation() + P->GetActorUpVector() * SmokingKit::PackH / 2.f;
		const FVector Eye = Hero->EyeLocation();
		const FVector To = Center - Eye;
		bNear = To.Size() < 190.f && FVector::DotProduct(To.GetSafeNormal(), PC->GetControlRotation().Vector()) > 0.88f;
		const bool bPick = bForcePick;
		bForcePick = false;
		if ((bNear || bPick) && (PC->WasInputKeyJustPressed(EKeys::E) || bPick) && !UHeroReachComponent::Get(Hero)->IsBusy())
		{
			FReachItem Item;
			Item.Actor = P;
			Item.Center = Center;
			Item.Axis = P->GetActorUpVector();
			Item.HalfLength = SmokingKit::PackH / 2.f - SmokingKit::PackD / 2.f;
			Item.Radius = SmokingKit::PackD / 2.f;
			TWeakObjectPtr<ULyudSmoking> Self = this;
			if (UHeroReachComponent::Get(Hero)->Take(Item, [Self]
				{
					ULyudSmoking* S = Self.Get();
					ULyudInventory* Inv = S ? ULyudInventory::Get(S) : nullptr;
					if (!Inv)
					{
						return;
					}
					const bool bHad = Inv->Count(PackId) > 0;
					Inv->Add(PackId);
					if (!bHad)
					{
						Inv->Started.Add(PackId, float(ULyudSmokingState::TerraceLeft) / SmokingKit::PerPack);
					}
					Inv->Add(LighterId);
					if (S->TerraceLighter.IsValid()) { S->TerraceLighter->Destroy(); }
					if (ULyudSmokingState* State = ULyudSmokingState::Get(S)) { State->bTerraceTaken = true; }
					UE_LOG(LogTemp, Display, TEXT("SMOKE terrace pack taken: packs %d left %d lighters %d"), Inv->Count(PackId), S->PackLeft(), Inv->Count(LighterId));
				}))
			{
				bNear = false;
			}
		}
	}
	if (bNear != bTerracePrompt && UI)
	{
		bTerracePrompt = bNear;
		UI->SetPrompt(P, bNear ? LyudText::Get(TEXT("ui.smoke.take_pack")) : FText::GetEmpty());
	}
}

// ------------------------------------------------------------------ the tick

void ULyudSmoking::PutOut()
{
	if (Step == EStep::Held)
	{
		if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
		{
			Hand->PutAway();
		}
	}
}

void ULyudSmoking::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	ULyudHandUse* Hand = ULyudHandUse::Get(World);
	TickButts(DeltaTime);
	TickTerrace();
	if (SayLeft > 0.f)
	{
		SayLeft -= DeltaTime;
		if (SayLeft <= 0.f)
		{
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World)) { UI->SetPrompt(this, FText::GetEmpty()); }
		}
	}
	if (!Hero || !PC || !Inv || !Hand)
	{
		return;
	}
	// "take in the hand" on the pack: the request is ours (the hand's state is kept here, not in the inventory)
	if (Inv->EquippedItem() == PackId)
	{
		Inv->Equip(FName());
		TakeOut();
	}
	StepTime += DeltaTime;
	if (Cig.IsValid() && Step == EStep::Held)
	{
		LastCigXf = Cig->GetActorTransform();
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bFree = !(UI && UI->IsOpen()) && !(Menu && Menu->IsOpen()) && !PC->IsMoveInputIgnored();

	switch (Step)
	{
	case EStep::None:
		break;
	case EStep::PackUp:
	{
		ACigarettePack* P = Pack.Get();
		if (!P || Hand->HeldActor() != P)
		{
			Go(EStep::None);
			break;
		}
		if (P->IsHidden())
		{
			StepTime = 0.f;     // the clock starts when the hand has it up
			break;
		}
		if (StepTime - DeltaTime <= 0.f)
		{
			Sound(TEXT("pack_open"), 0.7f);
		}
		P->SetLid(FMath::SmoothStep(0.f, 0.3f, StepTime));
		P->SetRaised(3.f * FMath::SmoothStep(0.35f, 0.75f, StepTime));
		if (StepTime > 0.8f && StepTime - DeltaTime <= 0.8f)
		{
			Hand->Press();
		}
		if (bAtLipsEvent)
		{
			// the lips take it: from here it rests in them
			Hand->Release();
			Hand->Interrupt();
			bAtLipsEvent = false;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform At = P->NextCigarette();
			if (ACigaretteItem* C = World->SpawnActor<ACigaretteItem>(At.GetLocation(), At.Rotator(), Params))
			{
				MouthCig = C;
				MouthFrom = At.GetRelativeTransform(LookFrame());
				MouthBlend = 0.f;
			}
			P->SetLeft(P->GetLeft() - 1);
			P->SetRaised(0.f);
			TakeOneFromPack();
			if (FLyudHandThing* T = Hand->Edit()) { T->Carry = Pocket(); }
			Go(EStep::PackDown);
		}
		break;
	}
	case EStep::PackDown:
		if (Pack.IsValid())
		{
			Pack->SetLid(1.f - FMath::SmoothStep(0.f, 0.3f, StepTime));
		}
		if (StepTime > 0.75f)
		{
			bInternal = true;
			Hand->PutAway();
			bInternal = false;
			HoldCigarette(true);
		}
		break;
	case EStep::FromMouth:
		if (bAtLipsEvent)
		{
			// the fingers have it: the lips let go, the hand comes down with it
			bAtLipsEvent = false;
			if (MouthCig.IsValid()) { MouthCig->Destroy(); }
			MouthCig = nullptr;
			Hand->Release();
			if (FLyudHandThing* T = Hand->Edit()) { T->Carry = CigCarry(); }
			if (bLit)
			{
				// the first breath of it, as the hand comes away
				ExhaleIn = 0.35f;
				ExhaleAmount = 0.55f;
			}
			Go(EStep::Held);
		}
		else if (StepTime > 3.f || !Hand->IsHolding())
		{
			Go(EStep::None);
		}
		break;
	case EStep::Held:
		if (!Hand->IsHolding() || Hand->HeldActor() != Cig.Get())
		{
			// another thing took the hand (OnPutAway handled the cigarette)
			if (Step == EStep::Held) { Go(EStep::None); }
			break;
		}
		if (bFree && PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
		{
			PutOut();
			break;
		}
		if (!bLit && bAtLipsEvent)
		{
			// unlit at the lips: left there while the hand fetches the lighter
			bAtLipsEvent = false;
			if (Inv->Count(LighterId) < 1)
			{
				Say(TEXT("ui.smoke.no_lighter"), 2.5f);
				break;
			}
			Hand->Release();
			Hand->Interrupt();
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform At = MouthInLook() * LookFrame();
			if (ACigaretteItem* C = World->SpawnActor<ACigaretteItem>(At.GetLocation(), At.Rotator(), Params))
			{
				C->SetLeft(Left);
				MouthCig = C;
				MouthBlend = 1.f;
			}
			if (Cig.IsValid()) { Cig->SetActorHiddenInGame(true); }
			if (FLyudHandThing* T = Hand->Edit()) { T->Carry = Pocket(); }
			Go(EStep::ToMouthToLight);
		}
		bAtLipsEvent = false;
		break;
	case EStep::ToMouthToLight:
		if (Cig.IsValid()) { Cig->SetActorHiddenInGame(true); }
		if (StepTime > 0.6f)
		{
			bInternal = true;
			Hand->PutAway();
			bInternal = false;
			HoldLighter();
			Go(EStep::LighterUp);
		}
		break;
	case EStep::LighterUp:
		if (bAtLipsEvent)
		{
			bAtLipsEvent = false;
			Sound(TEXT("lighter_flick"), 0.8f);
			if (Lighter.IsValid()) { Lighter->SetFlame(true); }
			Go(EStep::Lighting);
		}
		else if (StepTime > 3.f || !Hand->IsHolding())
		{
			Go(EStep::None);
		}
		break;
	case EStep::Lighting:
		if (StepTime - DeltaTime <= 0.2f && StepTime > 0.2f && Lighter.IsValid() && MouthCig.IsValid())
		{
			// how near the flame came to the end it lights (look frame, cm): the grip and poses are checked by this
			const FVector D = LookFrame().InverseTransformVectorNoScale(Lighter->GetActorTransform().TransformPosition(ALighterItem::FlamePoint()) - MouthCig->TipLocation());
			UE_LOG(LogTemp, Display, TEXT("SMOKE flame-tip %.1f %.1f %.1f cm, lighter up %.2f"), D.X, D.Y, D.Z, Lighter->GetActorUpVector().Z);
		}
		if (StepTime > 0.7f && !bLit)
		{
			// it catches: the hero draws as the flame touches it
			bLit = true;
			Glow = 1.f;
			Sound(TEXT("cig_drag"), 0.6f);
			if (MouthCig.IsValid())
			{
				Fx()->Emit(MouthCig->TipLocation(), FVector(0.f, 0.f, 10.f), 1.f, 9.f, 0.3f, 2.f, 12.f);
			}
		}
		if (StepTime > 1.3f)
		{
			Hand->Release();
			Hand->Interrupt();
			if (Lighter.IsValid()) { Lighter->SetFlame(false); }
			if (FLyudHandThing* T = Hand->Edit()) { T->Carry = Pocket(); }
			Go(EStep::LighterDown);
		}
		break;
	case EStep::LighterDown:
		if (StepTime > 0.6f)
		{
			bInternal = true;
			Hand->PutAway();
			bInternal = false;
			HoldCigarette(true);
		}
		break;
	}
	PlaceMouthCig(DeltaTime);
	Smoke(DeltaTime);
}

FString ULyudSmoking::DebugState() const
{
	return FString::Printf(TEXT("step %d lit %d left %.3f glow %.2f drag %.2f pack %d lighters %d mouth %d hand %s atlips %d butts %d smoke %d"),
		(int32)Step, bLit ? 1 : 0, Left, Glow, DragTime, PackLeft(), ULyudInventory::Get(this) ? ULyudInventory::Get(this)->Count(LighterId) : -1,
		MouthCig.IsValid() ? 1 : 0, ULyudHandUse::Get(this) ? *ULyudHandUse::Get(this)->HeldItem().ToString() : TEXT("-"),
		ULyudHandUse::Get(this) && ULyudHandUse::Get(this)->IsAtLips() ? 1 : 0, Butts.Num(), SmokeFx.IsValid() ? SmokeFx->Alive() : 0);
}

// --- console (offscreen checks) ---
static FAutoConsoleCommandWithWorld GSmokeGive(TEXT("lyud.Smoke.Give"), TEXT("lyud.Smoke.Give: a pack of cigarettes and a lighter into the pocket"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->Add(ULyudSmoking::PackId);
			Inv->Add(ULyudSmoking::LighterId);
		}
	}));

static FAutoConsoleCommandWithWorld GSmokeTake(TEXT("lyud.Smoke.Take"), TEXT("lyud.Smoke.Take: takes a cigarette out of the pack"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudSmoking* S = ULyudSmoking::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("SMOKE take -> %d"), S->TakeOut() ? 1 : 0);
		}
	}));

static FAutoConsoleCommandWithWorld GSmokeOut(TEXT("lyud.Smoke.Out"), TEXT("lyud.Smoke.Out: the right mouse button: put out and throw away, or back into the pack"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudSmoking* S = ULyudSmoking::Get(World))
		{
			S->PutOut();
		}
	}));

static FAutoConsoleCommandWithWorld GSmokeState(TEXT("lyud.Smoke.State"), TEXT("lyud.Smoke.State: logs the smoking state (SMOKE line)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudSmoking* S = ULyudSmoking::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("SMOKE state %s"), *S->DebugState());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSmokeShot(TEXT("lyud.Smoke.Shot"), TEXT("lyud.Smoke.Shot <file.png>: a screenshot of the next frame to that file (checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num() > 0)
		{
			FScreenshotRequest::RequestScreenshot(Args[0], false, false);
		}
	}));

static FAutoConsoleCommandWithWorld GSmokePick(TEXT("lyud.Smoke.Pick"), TEXT("lyud.Smoke.Pick: takes the terrace's pack and lighter as E would (checks)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudSmoking* S = ULyudSmoking::Get(World))
		{
			S->ForcePick();
		}
	}));
