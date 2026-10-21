#include "VendingMachine.h"

#include "BottleItem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	struct FDrink { const TCHAR* Item; const TCHAR* Flavor; };
	const FDrink Drinks[] = {
		{ TEXT("drink_baikal"), TEXT("baikal") },
		{ TEXT("drink_dyushes"), TEXT("dyushes") },
		{ TEXT("drink_buratino"), TEXT("buratino") },
		{ TEXT("drink_tarkhun"), TEXT("tarkhun") },
	};
	constexpr float DispenseTime = 0.7f;
}

AVendingMachine::AVendingMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	UStaticMesh* Cube = nullptr;
	auto Plate = [this, Root, &Cube](const TCHAR* Name, const FVector& Loc, const FVector& Size)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		if (!Cube)
		{
			Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		}
		C->SetStaticMesh(Cube);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		return C;
	};
	// the delivery tray under the flap: a dark plastic shelf with a back lip, a hand's width deep
	Tray = Plate(TEXT("Tray"), FVector(37.5f + 7.f, 10.f, 12.f), FVector(14.f, 44.f, 1.6f));
	TrayBack = Plate(TEXT("TrayLip"), FVector(37.5f + 13.4f, 10.f, 14.f), FVector(1.2f, 44.f, 4.5f));
}

void AVendingMachine::BeginPlay()
{
	Super::BeginPlay();
	if (bStockShelves)
	{
		StockShelves();
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Base)
	{
		for (UStaticMeshComponent* C : { Tray.Get(), TrayBack.Get() })
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.025f, 0.03f));
			C->SetMaterial(0, MID);
		}
	}
}

void AVendingMachine::StockShelves()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// five glass shelves behind the window (the model's own: 5 shelves 22 cm apart from 50 cm up, window 65 cm wide, centred 11.75 cm
	// to the machine's right); the bottles lie across them three high and three wide, labels out toward the glass
	constexpr float Scale = 0.9f, ShelfZ0 = 50.4f, ShelfPitch = 22.f, FrontX = 24.2f, MidY = 11.75f;
	const float Len = 2.f * ABottleItem::BottleHalfLength * Scale, R = ABottleItem::BodyRadius * Scale;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Shelf = 0; Shelf < 5; ++Shelf)
	{
		const TCHAR* Kind = Drinks[(Shelf % 4)].Flavor;
		for (int32 Level = 0; Level < 3; ++Level)
		{
			for (int32 Col = -1; Col <= 1; ++Col)
			{
				const FVector Local(FrontX + (Level % 2) * 1.2f, MidY + Col * (Len + 0.4f), ShelfZ0 + Shelf * ShelfPitch + R + 0.2f + Level * 2.f * R * 0.96f);
				ABottleItem* B = World->SpawnActor<ABottleItem>(GetActorTransform().TransformPosition(Local), FRotator::ZeroRotator, Params);
				if (!B)
				{
					continue;
				}
				B->Init(Kind, 1.f);
				const FVector Neck = GetActorRightVector() * ((Shelf + Col + Level) % 2 ? 1.f : -1.f);
				B->SetActorRotation(FRotationMatrix::MakeFromZX(Neck, GetActorForwardVector()).Rotator());
				B->SetActorScale3D(FVector(Scale));
				B->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				Stock.Add(B);
			}
		}
	}
}

void AVendingMachine::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	Super::EndPlay(Reason);
}

FVector AVendingMachine::TrayRest() const
{
	// the bottle lies on the tray: across the machine, label up
	return GetActorTransform().TransformPosition(FVector(37.5f + 7.5f, 10.f, 13.f + ABottleItem::BodyRadius + 0.2f));
}

FText AVendingMachine::PromptFor(int32 Sel) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (Inv && Inv->Rubles < Price)
	{
		return LyudText::Get(TEXT("ui.vending.nomoney"), { FStringFormatArg(Price), FStringFormatArg(Inv->Rubles) });
	}
	TArray<FStringFormatArg> Names;
	for (const FDrink& D : Drinks)
	{
		Names.Add(FStringFormatArg(ULyudInventory::ItemName(D.Item).ToString()));
	}
	return FText::FromString(LyudText::Get(TEXT("ui.vending.buy"), { FStringFormatArg(ULyudInventory::ItemName(Drinks[Sel].Item).ToString()), FStringFormatArg(Price) }).ToString()
		+ TEXT("     ") + LyudText::Get(TEXT("ui.vending.choose"), Names).ToString());
}

void AVendingMachine::Buy()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	UWorld* World = GetWorld();
	if (!Inv || !World || !Inv->Spend(Price))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Bottle = World->SpawnActor<ABottleItem>(GetActorTransform().TransformPosition(FlapLocal - FVector(10.f, 0.f, 0.f)), FRotator::ZeroRotator, Params);
	if (Bottle)
	{
		Bottle->Init(Drinks[Flavor].Flavor, 1.f);
		Bottle->LieDown(Bottle->GetActorLocation(), GetActorRightVector());
	}
	State = EState::Dispensing;
	Clock = 0.f;
}

void AVendingMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!Hero || !PC || !UI)
	{
		return;
	}

	if (State == EState::Dispensing)
	{
		Clock += DeltaSeconds;
		const float T = FMath::Clamp(Clock / DispenseTime, 0.f, 1.f);
		if (Bottle)
		{
			// the bottle slides out of the flap, then drops the last few centimetres onto the tray
			const FVector Start = GetActorTransform().TransformPosition(FlapLocal - FVector(10.f, 0.f, 0.f));
			const FVector Mid = GetActorTransform().TransformPosition(FlapLocal + FVector(3.f, 0.f, 0.f));
			const FVector End = TrayRest();
			const FVector P = T < 0.5f ? FMath::Lerp(Start, Mid, FMath::SmoothStep(0.f, 1.f, T / 0.5f))
			                          : FMath::Lerp(Mid, End, FMath::Square((T - 0.5f) / 0.5f));
			Bottle->SetActorLocation(P);
		}
		if (T >= 1.f)
		{
			State = EState::Taking;
			UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
			const FName ItemId = Drinks[Flavor].Item;
			if (!Bottle || !Reach || !Reach->Take(Bottle->MakeReachItem(), [this, ItemId]()
			{
				if (ULyudInventory* Inv = ULyudInventory::Get(this))
				{
					Inv->Add(ItemId);
				}
				State = EState::Idle;
			}))
			{
				// the hand is busy: the player gets the bottle anyway
				if (ULyudInventory* Inv = ULyudInventory::Get(this))
				{
					Inv->Add(ItemId);
				}
				if (Bottle)
				{
					Bottle->Destroy();
				}
				State = EState::Idle;
			}
		}
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
		return;
	}
	if (State == EState::Taking)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
		return;
	}

	// idle: is the player at the machine, looking at it?
	const FVector Flap = GetActorTransform().TransformPosition(FlapLocal);
	const FVector Eye = Hero->EyeLocation();
	const float Flat = FVector::Dist2D(Hero->GetActorLocation(), Flap);
	const FVector ToMachine = (GetActorLocation() + FVector(0.f, 0.f, 100.f) - Eye).GetSafeNormal();
	const bool bFront = FVector::DotProduct(GetActorForwardVector(), (Hero->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()) > 0.3f;
	const bool bLooking = FVector::DotProduct(PC->GetControlRotation().Vector(), ToMachine) > 0.55f;
	const bool bNear = Flat < UseDistance && Flat > 35.f && FMath::Abs((Hero->GetActorLocation() - Flap).Z) < 130.f;
	UHeroReachComponent* Reach = Hero->FindComponentByClass<UHeroReachComponent>();
	const bool bFree = !UI->IsOpen() && !(Reach && Reach->IsBusy()) && !Hero->IsHidden();
	if (bNear && bFront && bLooking && bFree)
	{
		const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
		for (int32 I = 0; I < 4; ++I)
		{
			if (PC->WasInputKeyJustPressed(Keys[I]))
			{
				Flavor = I;
			}
		}
		UI->SetPrompt(this, PromptFor(Flavor));
		bPromptShown = true;
		const ULyudInventory* Inv = ULyudInventory::Get(this);
		if (PC->WasInputKeyJustPressed(EKeys::E) && Inv && Inv->Rubles >= Price)
		{
			Buy();
		}
	}
	else if (bPromptShown)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		bPromptShown = false;
	}
}
