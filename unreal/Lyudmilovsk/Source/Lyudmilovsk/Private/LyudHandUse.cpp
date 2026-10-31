#include "LyudHandUse.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "UI/LyudGameScreen.h"

namespace
{
	/** The lips in the yaw frame from the eyes (forward, right, up), cm: where the reach component drinks (MouthPos) and a touch out. */
	const FVector LipsPos(9.f, 1.5f, -8.5f);

	TAutoConsoleVariable<int32> CVarHandUse(TEXT("lyud.HandUse"), 1,
		TEXT("Things in the hand used at the mouth with the left mouse button (food, cigarettes). 0: the button does nothing."));

	float Ease(float T)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f));
	}

	/** The right palm's frame from the hand's bones (as BirdTame's HandPalm): X the fingers, Z out of the palm, world. */
	bool BonePalm(const USkeletalMeshComponent* Body, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		const FVector Across = Body->GetBoneLocation(TEXT("thumb_01_r")) - Body->GetBoneLocation(TEXT("pinky_01_r"));
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		// out of the palm is the way the hold asked for (a closed fist hides the thumb tip, so it can't tell)
		if (FVector::DotProduct(Pn, Normal) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Wrist + (Knuckle - Wrist) * 0.6f);
		return true;
	}

	FHeroHoldPose Blend(const FHeroHoldPose& A, const FHeroHoldPose& B, float T)
	{
		FHeroHoldPose P = A;
		P.Pos = FMath::Lerp(A.Pos, B.Pos, T);
		const FQuat QA = FRotationMatrix::MakeFromXZ(A.FingerDir, A.PalmNormal).ToQuat();
		const FQuat QB = FRotationMatrix::MakeFromXZ(B.FingerDir, B.PalmNormal).ToQuat();
		const FQuat Q = FQuat::Slerp(QA, QB, T).GetNormalized();
		P.FingerDir = Q.GetAxisX();
		P.PalmNormal = Q.GetAxisZ();
		P.Curl = FMath::Lerp(A.Curl, B.Curl, T);
		P.PitchFollow = FMath::Lerp(A.PitchFollow, B.PitchFollow, T);
		return P;
	}
}

ULyudHandUse* ULyudHandUse::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudHandUse>() : nullptr;
}

void ULyudHandUse::Deinitialize()
{
	if (bHas)
	{
		Drop(false);
	}
	Super::Deinitialize();
}

bool ULyudHandUse::Hold(FLyudHandThing InThing)
{
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Hero || !InThing.Actor.IsValid())
	{
		return false;
	}
	if (bHas)
	{
		Drop(false);
	}
	Thing = MoveTemp(InThing);
	bHas = true;
	Phase = EPhase::Carry;
	AtLips = 0.f;
	PhaseTime = PressTime = SteadyTime = HoldTime = 0.f;
	bWasDown = bClickQueued = bClickDown = bAttached = false;
	// the fingers close on its shape (in the palm's frame); without one, on its visible parts' bounds
	if (Thing.Shape.IsEmpty())
	{
		AActor* A = Thing.Actor.Get();
		const FTransform ToLocal = A->GetActorTransform().Inverse();
		TArray<UPrimitiveComponent*> Comps;
		A->GetComponents<UPrimitiveComponent>(Comps);
		for (UPrimitiveComponent* C : Comps)
		{
			if (!C || !C->IsVisible())
			{
				continue;
			}
			const FTransform CT = C->GetComponentTransform() * ToLocal;
			const FBox L = C->CalcBounds(FTransform::Identity).GetBox();
			FReachPrim Pr;
			Pr.Kind = FReachPrim::EKind::Box;
			Pr.Xf = FTransform(CT.GetRotation(), CT.TransformPosition(L.GetCenter()));
			Pr.Extent = L.GetExtent() * CT.GetScale3D().GetAbs();
			Thing.Shape.Prims.Add(Pr);
		}
	}
	Thing.Carry.Shape = Thing.Shape.Transformed(Thing.InPalm);
	Thing.Mouth.Shape = Thing.Carry.Shape;
	// hidden until the hand, coming up, has it
	SetHidden(true);
	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Thing.Actor->GetRootComponent()))
	{
		Prim->SetSimulatePhysics(false);
	}
	Thing.Actor->SetActorEnableCollision(false);
	UE_LOG(LogTemp, Display, TEXT("HANDUSE hold %s left %.2f"), *Thing.Item.ToString(), Thing.Left);
	return true;
}

void ULyudHandUse::PutAway()
{
	if (bHas)
	{
		Drop(false);
	}
}

void ULyudHandUse::Drop(bool bUsedUp)
{
	FLyudHandThing Was = MoveTemp(Thing);
	Thing = FLyudHandThing();
	bHas = false;
	Phase = EPhase::Carry;
	SetPrompted(false);
	if (AActor* A = Was.Actor.Get())
	{
		A->Destroy();
	}
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UHeroReachComponent* Reach = Hero->FindComponentByClass<UHeroReachComponent>())
		{
			Reach->ClearOverrideHold();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("HANDUSE %s %s left %.2f"), bUsedUp ? TEXT("usedup") : TEXT("putaway"), *Was.Item.ToString(), Was.Left);
	if (bUsedUp)
	{
		if (Was.OnUsedUp)
		{
			Was.OnUsedUp();
		}
	}
	else if (Was.OnPutAway)
	{
		Was.OnPutAway(Was.Left);
	}
}

void ULyudHandUse::SetHidden(bool bHide)
{
	bHidden = bHide;
	if (AActor* A = Thing.Actor.Get())
	{
		A->SetActorHiddenInGame(bHide);
	}
}

void ULyudHandUse::SetPrompted(bool bOn)
{
	if (bOn == bPrompted)
	{
		return;
	}
	bPrompted = bOn;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, bOn ? (Thing.Prompt.IsEmpty() ? LyudText::Get(TEXT("ui.hand.prompt")) : Thing.Prompt) : FText::GetEmpty());
	}
}

FVector ULyudHandUse::MouthPoint() const
{
	// as it is eaten the bite edge moves from the whole thing's point toward the empty one's
	return FMath::Lerp(Thing.MouthEmpty, Thing.MouthFull, FMath::Clamp(Thing.Left, 0.f, 1.f));
}

FVector ULyudHandUse::LipsLocation() const
{
	return Lips;
}

FHeroHoldPose ULyudHandUse::PoseNow(float Push) const
{
	// at the lips: the palm turned as Mouth says, put where the thing's mouth point (in the palm) meets the lips
	FHeroHoldPose AtMouth = Thing.Mouth;
	AtMouth.PitchFollow = 0.f;
	const FQuat Q = FRotationMatrix::MakeFromXZ(AtMouth.FingerDir, AtMouth.PalmNormal).ToQuat();
	AtMouth.Pos = LipsPos - FVector(Push, 0.f, 0.f) - Q.RotateVector(Thing.InPalm.TransformPosition(MouthPoint()));
	// the way there arcs a little low and out, so the thing doesn't come up through the chin
	FHeroHoldPose P = Blend(Thing.Carry, AtMouth, Ease(AtLips));
	P.Pos += FVector(0.f, 4.f, -5.f) * FMath::Sin(PI * FMath::Clamp(AtLips, 0.f, 1.f));
	P.Shape = Thing.Carry.Shape;
	P.BlendTime = Thing.Carry.BlendTime;
	return P;
}

bool ULyudHandUse::ButtonDown(const APlayerController* PC)
{
	if (bClickQueued)
	{
		// a scripted click: down this frame, up the next
		bClickQueued = false;
		bClickDown = true;
		return true;
	}
	if (bClickDown)
	{
		bClickDown = false;
		return bForced;
	}
	return bForced || (PC && CVarHandUse.GetValueOnGameThread() != 0 && PC->IsInputKeyDown(EKeys::LeftMouseButton));
}

void ULyudHandUse::Use(ELyudMouthUse InKind)
{
	const float Want = InKind == ELyudMouthUse::Small ? Thing.Small : InKind == ELyudMouthUse::Big ? Thing.Big : Thing.SteadyEach;
	float Took = FMath::Min(Want, Thing.Left);
	Thing.Left -= Took;
	// a crumb left over is eaten with the last bite
	if (Thing.Left < 0.025f)
	{
		Took += Thing.Left;
		Thing.Left = 0.f;
	}
	UE_LOG(LogTemp, Display, TEXT("HANDUSE use %s %s took %.3f left %.3f"), *Thing.Item.ToString(),
		InKind == ELyudMouthUse::Small ? TEXT("small") : InKind == ELyudMouthUse::Big ? TEXT("big") : TEXT("steady"), Took, Thing.Left);
	if (Thing.OnUse)
	{
		Thing.OnUse(InKind, Took, Thing.Left);
	}
	OnUsed.Broadcast(Thing.Item, InKind, Thing.Left);
}

void ULyudHandUse::Tick(float DeltaTime)
{
	if (!bHas)
	{
		return;
	}
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	AActor* A = Thing.Actor.Get();
	if (!Hero || !PC || !A)
	{
		Drop(false);
		return;
	}
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	// the diary takes the hand: the thing goes back in the pocket
	ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
	if (Screen && Screen->IsOpen())
	{
		Drop(false);
		return;
	}
	// another move of the hand (buying at a machine, a pick-up) or a screen up: hidden and waiting
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bBusy = Reach->IsBusy() || (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || Hero->IsHidden();
	if (bBusy)
	{
		if (bAttached)
		{
			A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			bAttached = false;
		}
		SetHidden(true);
		SetPrompted(false);
		HoldTime = 0.f;
		Phase = EPhase::Carry;
		AtLips = 0.f;
		bWasDown = false;
		if (!Reach->IsBusy())
		{
			Reach->ClearOverrideHold();
		}
		return;
	}

	// Q: away with it
	if (PC->WasInputKeyJustPressed(EKeys::Q) && !PC->IsMoveInputIgnored())
	{
		Drop(false);
		return;
	}

	// the button: a press starts the hand to the lips; still down when it gets there makes it a big use, then on and on
	const bool bDown = HoldTime > 0.35f && ButtonDown(PC);
	const bool bPressed = bDown && !bWasDown;
	bWasDown = bDown;
	if (bDown)
	{
		PressTime += DeltaTime;
	}
	PhaseTime += DeltaTime;
	switch (Phase)
	{
	case EPhase::Carry:
		AtLips = 0.f;
		if (bPressed)
		{
			Phase = EPhase::ToLips;
			PhaseTime = 0.f;
			PressTime = 0.f;
		}
		break;
	case EPhase::ToLips:
		AtLips = FMath::Min(1.f, AtLips + DeltaTime / FMath::Max(Thing.ToMouth, 0.05f));
		if (AtLips >= 1.f)
		{
			Kind = bDown ? ELyudMouthUse::Big : ELyudMouthUse::Small;
			Phase = EPhase::Use;
			PhaseTime = 0.f;
			Use(Kind);
		}
		break;
	case EPhase::Use:
		AtLips = 1.f;
		if (PhaseTime >= (Kind == ELyudMouthUse::Big ? Thing.BigAtLips : Thing.SmallAtLips))
		{
			if (Thing.Left <= 0.f)
			{
				Drop(true);
				return;
			}
			Phase = bDown && Kind != ELyudMouthUse::Small ? EPhase::Steady : EPhase::FromLips;
			PhaseTime = 0.f;
			SteadyTime = 0.f;
		}
		break;
	case EPhase::Steady:
		AtLips = 1.f;
		SteadyTime += DeltaTime;
		if (!bDown)
		{
			Phase = EPhase::FromLips;
			PhaseTime = 0.f;
		}
		else if (SteadyTime >= Thing.SteadyEvery)
		{
			SteadyTime = 0.f;
			Use(ELyudMouthUse::Steady);
			if (Thing.Left <= 0.f)
			{
				Drop(true);
				return;
			}
		}
		break;
	case EPhase::FromLips:
		AtLips = FMath::Max(0.f, AtLips - DeltaTime / FMath::Max(Thing.FromMouth, 0.05f));
		if (bPressed)
		{
			// back up again from where it is
			Phase = EPhase::ToLips;
			PressTime = 0.f;
		}
		else if (AtLips <= 0.f)
		{
			Phase = EPhase::Carry;
		}
		break;
	}

	// pushed in at the moment of a use: in fast, out slower; a big one tears its piece off (out further, a little down)
	float Push = 0.f;
	if (Phase == EPhase::Use)
	{
		const float Dur = Kind == ELyudMouthUse::Big ? Thing.BigAtLips : Thing.SmallAtLips;
		const float T = PhaseTime / FMath::Max(Dur, 0.05f);
		Push = Thing.Push * (T < 0.3f ? Ease(T / 0.3f) : 1.f - Ease((T - 0.3f) / 0.7f));
		if (Kind == ELyudMouthUse::Big)
		{
			Push -= 1.5f * Ease((T - 0.45f) / 0.55f);
		}
	}
	else if (Phase == EPhase::Steady)
	{
		// eating on: little bites at the rhythm of the uses
		Push = Thing.Push * 0.6f * FMath::Square(FMath::Sin(PI * SteadyTime / FMath::Max(Thing.SteadyEvery, 0.1f)));
	}
	if (IsAtLips() && Thing.OnAtLips)
	{
		Thing.OnAtLips(DeltaTime);
	}

	Reach->SetOverrideHold(PoseNow(Push));
	HoldTime += DeltaTime;
	// the thing rides on the hand once the hand is up: placed where its palm really is, then attached to the bone
	if (!bAttached && Reach->HoldWeight() > 0.6f)
	{
		FTransform Palm;
		if (BonePalm(Hero->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), Palm))
		{
			A->SetActorTransform(Thing.InPalm * Palm);
			A->AttachToComponent(Hero->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
			bAttached = true;
		}
	}
	if (bAttached && bHidden)
	{
		SetHidden(false);
	}
	Lips = A->GetActorTransform().TransformPosition(MouthPoint());
	SetPrompted(bAttached);
}

// --- console: the button without a mouse (offscreen checks) ---
static FAutoConsoleCommandWithWorldAndArgs GHandPress(TEXT("lyud.Hand.Press"), TEXT("lyud.Hand.Press 1|0: holds the left mouse button down for the thing in the hand, or lets it go"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudHandUse* H = ULyudHandUse::Get(World))
		{
			Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0 ? H->Press() : H->Release();
		}
	}));

static FAutoConsoleCommandWithWorld GHandClick(TEXT("lyud.Hand.Click"), TEXT("lyud.Hand.Click: one click of the left mouse button for the thing in the hand"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudHandUse* H = ULyudHandUse::Get(World))
		{
			H->Click();
		}
	}));

static FAutoConsoleCommandWithWorld GHandAway(TEXT("lyud.Hand.PutAway"), TEXT("lyud.Hand.PutAway: puts the thing in the hand away"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudHandUse* H = ULyudHandUse::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("HANDUSE state holding %d item %s left %.2f atlips %d"), H->IsHolding() ? 1 : 0, *H->HeldItem().ToString(), H->Left(), H->IsAtLips() ? 1 : 0);
			H->PutAway();
		}
	}));
