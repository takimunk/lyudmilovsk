#include "Food/LyudBite.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UI/LyudQuests.h"

namespace LyudBite
{
	/** Key poses in the yaw frame (forward, right, up from the eyes, cm); X of the rotation is the fork's tip / the cup's up. */
	const FVector RestPos(36.f, 11.f, -30.f);
	const FVector MouthPos(15.f, 2.5f, -8.f);

	AHeroCharacter* Hero(const UWorld* World)
	{
		return Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	}
}

ULyudBiteSubsystem* ULyudBiteSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBiteSubsystem>() : nullptr;
}

bool ULyudBiteSubsystem::BeginMeal(const FLyudMeal& InMeal)
{
	AHeroCharacter* Hero = LyudBite::Hero(GetWorld());
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	if (!Reach || Reach->IsBusy() || !InMeal.Dish.IsValid() || InMeal.Amount <= 0.f)
	{
		return false;
	}
	if (bActive)
	{
		Finish(false);
	}
	Meal = InMeal;
	bActive = true;
	bPressed = bQueued = false;
	PressT = ChewLeft = 0.f;
	BiteCount = 0;
	StartLocation = Hero->GetActorLocation();
	SpawnUtensil();
	Current = KeyFor(EPhase::Rest);
	StartPhase(EPhase::Rest, 0.f);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, LyudText::Get(Meal.Utensil == ELyudUtensil::Cup ? TEXT("ui.eat.prompt_drink") : TEXT("ui.eat.prompt")));
	}
	return true;
}

void ULyudBiteSubsystem::EndMeal()
{
	if (bActive)
	{
		Finish(Meal.Amount <= 0.f);
	}
}

void ULyudBiteSubsystem::TakeBite(ELyudBiteSize InSize)
{
	if (bActive)
	{
		bQueued = true;
		QueuedSize = InSize;
	}
}

void ULyudBiteSubsystem::Deinitialize()
{
	if (bActive)
	{
		Finish(false);
	}
	Super::Deinitialize();
}

FTransform ULyudBiteSubsystem::YawFrame() const
{
	const AHeroCharacter* Hero = LyudBite::Hero(GetWorld());
	const APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!Hero || !PC)
	{
		return FTransform::Identity;
	}
	return FTransform(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f), Hero->EyeLocation());
}

FTransform ULyudBiteSubsystem::KeyFor(EPhase P) const
{
	// rotations: X = the utensil's tip (Fork/Spoon) or the dish's up (Hand/Cup), in the yaw frame
	const bool bHeld = Meal.Utensil == ELyudUtensil::Hand || Meal.Utensil == ELyudUtensil::Cup;
	const bool bBig = Size == ELyudBiteSize::Big;
	switch (P)
	{
	case EPhase::Down:
	case EPhase::Scoop:
		if (!bHeld)
		{
			// over the plate (the dish's origin, a little off it bite by bite), the tines going down into it
			FVector At = YawFrame().InverseTransformPosition(Meal.Dish.IsValid() ? Meal.Dish->GetActorLocation() : FVector::ZeroVector);
			const float A = BiteCount * 2.4f;
			At += FVector(FMath::Cos(A) * 3.f, FMath::Sin(A) * 3.f - 2.f, P == EPhase::Scoop ? 2.5f : 7.f);
			const FVector Tip = FVector(0.55f, -0.25f, -0.8f).GetSafeNormal();
			return FTransform(FRotationMatrix::MakeFromXZ(Tip, FVector(0.f, -1.f, 0.f)).ToQuat(), At);
		}
		// a held dish has nothing to go down to: it comes up a little before the mouth
		return FTransform(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector(1.f, 0.f, 0.f)).ToQuat(), LyudBite::RestPos + FVector(-6.f, -4.f, 8.f));
	case EPhase::Up:
	case EPhase::Mouth:
		if (Meal.Utensil == ELyudUtensil::Cup)
		{
			// the rim at the lips, the cup tipped toward the face (a gulp tips it further)
			const float Tilt = FMath::DegreesToRadians(bBig ? 62.f : 42.f);
			const FVector Up(-FMath::Sin(Tilt), 0.f, FMath::Cos(Tilt));
			return FTransform(FRotationMatrix::MakeFromXZ(Up, FVector(1.f, 0.f, 0.f)).ToQuat(),
				LyudBite::MouthPos + FVector(4.f, -1.5f, -Meal.DishHalfHeight * 0.8f));
		}
		if (bHeld)
		{
			return FTransform(FRotationMatrix::MakeFromXZ(FVector(-0.3f, -0.2f, 1.f).GetSafeNormal(), FVector(1.f, 0.f, 0.f)).ToQuat(),
				LyudBite::MouthPos + FVector(5.f, 0.f, -Meal.DishHalfHeight));
		}
		{
			// the tines at the lips: the fork points back at the mouth from the hand
			const FVector Tip = FVector(-0.75f, -0.55f, 0.35f).GetSafeNormal();
			return FTransform(FRotationMatrix::MakeFromXZ(Tip, FVector(0.f, 0.f, 1.f)).ToQuat(), LyudBite::MouthPos - Tip * 13.f);
		}
	default:
		if (bHeld)
		{
			return FTransform(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector(1.f, 0.f, 0.f)).ToQuat(), LyudBite::RestPos);
		}
		return FTransform(FRotationMatrix::MakeFromXZ(FVector(0.75f, -0.5f, -0.35f).GetSafeNormal(), FVector(0.f, 0.f, 1.f)).ToQuat(), LyudBite::RestPos);
	}
}

float ULyudBiteSubsystem::PhaseSeconds(EPhase P) const
{
	const bool bBig = Size == ELyudBiteSize::Big;
	switch (P)
	{
	case EPhase::Down: return Meal.Utensil == ELyudUtensil::Cup ? 0.2f : 0.45f;
	case EPhase::Scoop: return Meal.Utensil == ELyudUtensil::Fork || Meal.Utensil == ELyudUtensil::Spoon ? (bBig ? 0.55f : 0.3f) : 0.05f;
	case EPhase::Up: return 0.5f;
	case EPhase::Mouth: return Meal.Utensil == ELyudUtensil::Cup ? (bBig ? 1.3f : 0.7f) : 0.18f;
	case EPhase::Back: return 0.45f;
	default: return 0.f;
	}
}

void ULyudBiteSubsystem::StartPhase(EPhase P, float)
{
	From = Current;
	Phase = P;
	PhaseT = 0.f;
	To = KeyFor(P);
}

void ULyudBiteSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = LyudBite::Hero(World);
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!Hero || !PC || !Meal.Dish.IsValid())
	{
		Finish(false);
		return;
	}
	if (FVector::Dist2D(Hero->GetActorLocation(), StartLocation) > LeaveDistance)
	{
		Finish(false);
		return;
	}
	// the button, polled (never consumed); no eating while a screen is up
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const bool bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton) && !(UI && UI->IsOpen());
	ChewLeft = FMath::Max(0.f, ChewLeft - DeltaTime);

	PhaseT += DeltaTime;
	const float Dur = PhaseSeconds(Phase);
	switch (Phase)
	{
	case EPhase::Rest:
		// a new bite: a press (held on from the last bite once the chewing allows it) or a queued one
		if ((bDown && !bPressed) || (bDown && bPressed && ChewLeft <= 0.f) || bQueued)
		{
			Size = bQueued ? QueuedSize : (bPressed ? ELyudBiteSize::Big : ELyudBiteSize::Small);
			PressT = bPressed ? ClickSeconds : 0.f;
			bPressed = bDown;
			bQueued = false;
			StartPhase(EPhase::Down, 0.f);
		}
		else if (!bDown)
		{
			bPressed = false;
		}
		break;
	case EPhase::Down:
		if (bPressed)
		{
			PressT += DeltaTime;
			bPressed = bDown;
			if (PressT >= ClickSeconds && bDown)
			{
				Size = ELyudBiteSize::Big;
			}
		}
		if (PhaseT >= Dur)
		{
			StartPhase(EPhase::Scoop, 0.f);
			To = KeyFor(EPhase::Scoop);
			if (Meal.Utensil == ELyudUtensil::Fork || Meal.Utensil == ELyudUtensil::Spoon)
			{
				Play(TEXT("scoop"), 0.35f);
			}
		}
		break;
	case EPhase::Scoop:
		if (PhaseT >= Dur)
		{
			if (LoadC)
			{
				LoadC->SetRelativeScale3D(FVector(Size == ELyudBiteSize::Big ? 1.f : 0.55f));
				LoadC->SetVisibility(true);
			}
			StartPhase(EPhase::Up, 0.f);
		}
		break;
	case EPhase::Up:
		if (PhaseT >= Dur)
		{
			StartPhase(EPhase::Mouth, 0.f);
			ReachMouth();
		}
		break;
	case EPhase::Mouth:
		if (PhaseT >= Dur)
		{
			StartPhase(EPhase::Back, 0.f);
		}
		break;
	case EPhase::Back:
		if (PhaseT >= Dur)
		{
			Current = KeyFor(EPhase::Rest);
			StartPhase(EPhase::Rest, 0.f);
			if (Meal.Amount <= 0.f)
			{
				Finish(true);
				return;
			}
			// still held from the last bite: the next big one follows when the chewing allows
			bPressed = bDown && bPressed;
		}
		break;
	}
	PoseHand(DeltaTime);
}

void ULyudBiteSubsystem::ReachMouth()
{
	const float Take = Size == ELyudBiteSize::Big ? Meal.BigBite : Meal.SmallBite;
	Meal.Amount = FMath::Max(0.f, Meal.Amount - Take);
	if (Meal.Amount < 0.02f)
	{
		Meal.Amount = 0.f;
	}
	++BiteCount;
	if (LoadC)
	{
		LoadC->SetVisibility(false);
	}
	const bool bBig = Size == ELyudBiteSize::Big;
	if (Meal.Utensil == ELyudUtensil::Cup)
	{
		Play(bBig ? TEXT("gulp") : TEXT("sip"));
		ChewLeft = 0.2f;
	}
	else
	{
		Play(bBig ? TEXT("bite_big") : TEXT("bite_small"));
		Play(TEXT("chew"), bBig ? 0.4f : 0.3f);
		ChewLeft = bBig ? 1.4f : 0.7f;
	}
	if (Meal.OnBite)
	{
		Meal.OnBite(Meal.Amount, Size);
	}
	OnBite.Broadcast(Meal.Item, Size, Meal.Amount);
	if (Meal.Amount <= 0.f)
	{
		ULyudQuests::Notify(this, TEXT("use"), Meal.Item.ToString());
	}
}

void ULyudBiteSubsystem::PoseHand(float)
{
	AHeroCharacter* Hero = LyudBite::Hero(GetWorld());
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	if (!Reach)
	{
		return;
	}
	const float Dur = PhaseSeconds(Phase);
	const float A = Dur > 0.f ? FMath::SmoothStep(0.f, 1.f, FMath::Clamp(PhaseT / Dur, 0.f, 1.f)) : 1.f;
	// the key poses follow the dish and the eyes, so the blend is redone every frame from the phase's two keys
	To = KeyFor(Phase);
	Current.Blend(From, To, A);
	Current.NormalizeRotation();

	const FTransform Yaw = YawFrame();
	const FVector ItemPos = Yaw.TransformPosition(Current.GetLocation());
	const FQuat ItemRot = Yaw.GetRotation() * Current.GetRotation();
	const FVector Axis = ItemRot.GetAxisX();
	const bool bHeld = Meal.Utensil == ELyudUtensil::Hand || Meal.Utensil == ELyudUtensil::Cup;

	FVector Palm, Finger, Normal;
	FReachShape Shape;
	if (bHeld)
	{
		// the hand round the dish from its right side, fingers forward round the far side
		AActor* Dish = Meal.Dish.Get();
		Dish->SetActorLocationAndRotation(ItemPos, ItemRot * FQuat(FRotator(-90.f, 0.f, 0.f)));
		const FVector Side = (Yaw.GetRotation().GetRightVector() - Axis * FVector::DotProduct(Axis, Yaw.GetRotation().GetRightVector())).GetSafeNormal();
		Normal = -Side;
		Finger = FVector::CrossProduct(Axis, Side).GetSafeNormal();
		if (FVector::DotProduct(Finger, Yaw.GetRotation().GetForwardVector()) < 0.f)
		{
			Finger = -Finger;
		}
		Palm = ItemPos + Side * (Meal.DishRadius + 1.f);
		Shape.AddCapsule(ItemPos, Axis, Meal.DishHalfHeight * 0.7f, Meal.DishRadius);
	}
	else if (Utensil)
	{
		Utensil->SetActorLocationAndRotation(ItemPos, ItemRot);
		// the handle across the palm, the palm on the handle's right, the fingers closing over it from above
		const FVector Up = ItemRot.GetAxisZ();
		Normal = -FVector::CrossProduct(Up, Axis).GetSafeNormal();
		Finger = Up;
		Palm = ItemPos - Axis * 5.f - Normal * 1.4f;
		Shape.AddCapsule(ItemPos - Axis * 5.f, Axis, 5.f, 0.7f);
	}
	else
	{
		return;
	}
	const FTransform HoldFrame(Yaw.GetRotation(), Yaw.GetLocation());
	FHeroHoldPose H;
	H.Pos = HoldFrame.InverseTransformPosition(Palm);
	H.FingerDir = HoldFrame.InverseTransformVectorNoScale(Finger);
	H.PalmNormal = HoldFrame.InverseTransformVectorNoScale(Normal);
	H.PitchFollow = 0.f;
	H.Curl = 1.f;
	H.BlendTime = 0.2f;
	const FTransform PalmFrame(FRotationMatrix::MakeFromXZ(Finger, Normal).ToQuat(), Palm);
	H.Shape = Shape.Transformed(PalmFrame.Inverse());
	Reach->SetOverrideHold(H);
}

void ULyudBiteSubsystem::SpawnUtensil()
{
	if (Meal.Utensil != ELyudUtensil::Fork && Meal.Utensil != ELyudUtensil::Spoon)
	{
		return;
	}
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Meal.Dish->GetActorTransform(), P);
	A->SetMobility(EComponentMobility::Movable);
	UStaticMesh* M = Meal.UtensilMesh;
	if (!M)
	{
		M = LoadObject<UStaticMesh>(nullptr, Meal.Utensil == ELyudUtensil::Spoon ? TEXT("/Game/Food/Meshes/SM_Spoon.SM_Spoon") : TEXT("/Game/Food/Meshes/SM_Fork.SM_Fork"));
	}
	UStaticMeshComponent* C = A->GetStaticMeshComponent();
	if (!M)
	{
		// no utensil mesh yet: a thin steel rod
		M = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		C->SetRelativeScale3D(FVector(0.012f, 0.012f, 0.19f));
	}
	C->SetStaticMesh(M);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	// a moving thing: out of Lumen (docs/benchmark.md)
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	UStaticMeshComponent* L = NewObject<UStaticMeshComponent>(A);
	L->SetupAttachment(A->GetRootComponent());
	L->SetStaticMesh(Meal.LoadMesh ? Meal.LoadMesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	if (!Meal.LoadMesh)
	{
		L->SetRelativeScale3D(FVector(0.035f));
	}
	if (Meal.LoadMaterial)
	{
		L->SetMaterial(0, Meal.LoadMaterial);
	}
	else if (UMaterialInstanceDynamic* MID = L->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Meal.LoadTint);
	}
	// the load rides on the tines, 12 cm out from the grip
	L->SetRelativeLocation(FVector(12.f, 0.f, 0.6f));
	L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	L->SetCastShadow(false);
	L->bAffectDynamicIndirectLighting = false;
	L->bAffectDistanceFieldLighting = false;
	L->SetVisibility(false);
	L->RegisterComponent();
	Utensil = A;
	UtensilMeshC = C;
	LoadC = L;
}

void ULyudBiteSubsystem::Finish(bool bEaten)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	if (AHeroCharacter* Hero = LyudBite::Hero(GetWorld()))
	{
		if (UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero))
		{
			Reach->ClearOverrideHold();
		}
	}
	if (Utensil)
	{
		Utensil->Destroy();
	}
	Utensil = nullptr;
	UtensilMeshC = nullptr;
	LoadC = nullptr;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	if (Meal.Utensil == ELyudUtensil::Fork || Meal.Utensil == ELyudUtensil::Spoon)
	{
		Play(TEXT("clink"), 0.3f);
	}
	TFunction<void(float)> Done = MoveTemp(Meal.OnEnd);
	const float Left = bEaten ? 0.f : Meal.Amount;
	Meal = FLyudMeal();
	if (Done)
	{
		Done(Left);
	}
}

USoundBase* ULyudBiteSubsystem::FindSound(const FString& Name)
{
	const FName Key(*Name);
	if (const TObjectPtr<USoundBase>* S = Sounds.Find(Key))
	{
		return *S;
	}
	const FString Path = FString::Printf(TEXT("/Game/Audio/Food/S_%s.S_%s"), *Name, *Name);
	USoundBase* S = FPackageName::DoesPackageExist(FString::Printf(TEXT("/Game/Audio/Food/S_%s"), *Name)) ? LoadObject<USoundBase>(nullptr, *Path) : nullptr;
	Sounds.Add(Key, S);
	return S;
}

void ULyudBiteSubsystem::Play(const TCHAR* Kind, float Volume)
{
	// a few takes per kind (S_<kind>_01..04), the item's own before the common ones
	const int32 Take = FMath::RandRange(1, 4);
	USoundBase* S = nullptr;
	for (int32 Try = 0; Try < 4 && !S; ++Try)
	{
		const int32 N = (Take + Try - 1) % 4 + 1;
		if (!Meal.Item.IsNone())
		{
			S = FindSound(FString::Printf(TEXT("%s_%s_%02d"), *Meal.Item.ToString(), Kind, N));
		}
		if (!S)
		{
			S = FindSound(FString::Printf(TEXT("%s_%02d"), Kind, N));
		}
	}
	if (S)
	{
		UGameplayStatics::PlaySound2D(this, S, Volume, FMath::FRandRange(0.95f, 1.05f));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBiteTake(TEXT("lyud.Bite.Take"),
	TEXT("lyud.Bite.Take small|big: one bite of the meal being eaten (tests)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudBiteSubsystem* B = ULyudBiteSubsystem::Get(World))
		{
			B->TakeBite(Args.Num() > 0 && Args[0] == TEXT("big") ? ELyudBiteSize::Big : ELyudBiteSize::Small);
			UE_LOG(LogTemp, Display, TEXT("Bite: %s %s left %.2f"), *B->MealItem().ToString(), B->IsEating() ? TEXT("eating") : TEXT("idle"), B->AmountLeft());
		}
	}));

static FAutoConsoleCommandWithWorld GBiteStop(TEXT("lyud.Bite.Stop"), TEXT("lyud.Bite.Stop: ends the meal being eaten"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudBiteSubsystem* B = ULyudBiteSubsystem::Get(World))
		{
			B->EndMeal();
		}
	}));
