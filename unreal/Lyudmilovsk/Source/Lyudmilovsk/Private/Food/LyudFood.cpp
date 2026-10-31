#include "Food/LyudFood.h"

#include "Audio/LyudSoundscape.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGoodsItem.h"
#include "LyudInventory.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FVector Vec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(Field, A) && A->Num() == 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return Default;
	}

	/** A plain look (a pack, a bar, the apple's sphere: centre at its middle, local Z its top, X its front) in the palm: lying across
	 *  it with the top out of the thumb's side (toward the mouth's middle when the hand is up), the front out of the palm. */
	void PlainGrip(FLyudFoodDef& F, const FVector& Half)
	{
		const FMatrix M(FVector(0.f, 0.f, 1.f), FVector(-1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), FVector::ZeroVector);
		F.InPalm = FTransform(FQuat(M), FVector(0.f, -(Half.Z - FMath::Min(3.f, Half.Z)), Half.X + 0.5f));
		F.MouthFull = FVector(0.f, 0.f, Half.Z);
		F.MouthEmpty = FVector(0.f, 0.f, -Half.Z * 0.5f);
	}

	TMap<FName, FLyudFoodDef> LoadFoods()
	{
		TMap<FName, FLyudFoodDef> Out;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Foods = nullptr;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Food/food.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Root->TryGetObjectField(TEXT("foods"), Foods))
		{
			UE_LOG(LogTemp, Warning, TEXT("Food: Content/Food/food.json unreadable"));
			return Out;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Foods)->Values)
		{
			const TSharedPtr<FJsonObject> O = It.Value->AsObject();
			if (!O.IsValid())
			{
				continue;
			}
			FLyudFoodDef F;
			F.Stages = O->HasField(TEXT("stages")) ? (int32)O->GetNumberField(TEXT("stages")) : 0;
			const FVector Rot = Vec(O, TEXT("palm_rot"), FVector::ZeroVector);
			F.InPalm = FTransform(FRotator(Rot.X, Rot.Y, Rot.Z), Vec(O, TEXT("palm_loc"), FVector::ZeroVector));
			F.bHasMouth = O->HasField(TEXT("mouth_full"));
			F.MouthFull = Vec(O, TEXT("mouth_full"), FVector::ZeroVector);
			F.MouthEmpty = Vec(O, TEXT("mouth_empty"), F.MouthFull);
			F.MouthFingers = Vec(O, TEXT("mouth_fingers"), FVector::ZeroVector);
			F.MouthPalm = Vec(O, TEXT("mouth_palm"), FVector::ZeroVector);
			const FVector Bites = Vec(O, TEXT("bites"), FVector(F.Small, F.Big, F.SteadyEach));
			F.Small = Bites.X;
			F.Big = Bites.Y;
			F.SteadyEach = Bites.Z;
			O->TryGetNumberField(TEXT("steady_every"), F.SteadyEvery);
			O->TryGetNumberField(TEXT("chew_seconds"), F.ChewSeconds);
			FString Texture;
			if (O->TryGetStringField(TEXT("texture"), Texture))
			{
				F.Texture = FName(*Texture);
			}
			O->TryGetBoolField(TEXT("wrapped"), F.bWrapped);
			O->TryGetBoolField(TEXT("plated"), F.bPlated);
			Out.Add(FName(*It.Key), F);
		}
		return Out;
	}

	FString StagePath(FName Id, int32 K)
	{
		const FString Name = FString::Printf(TEXT("SM_Food_%s_%d"), *Id.ToString(), K);
		return FString::Printf(TEXT("/Game/Food/%s.%s"), *Name, *Name);
	}

	/** The hand at rest with food: palm up at the right of the chest, fingers forward, the food across the palm toward the middle. */
	FHeroHoldPose FoodCarry()
	{
		FHeroHoldPose P;
		P.Pos = FVector(36.f, 19.f, -33.f);
		P.FingerDir = FVector(1.f, 0.f, 0.15f).GetSafeNormal();
		P.PalmNormal = FVector(-0.25f, -0.35f, 1.f).GetSafeNormal();
		P.Curl = 1.f;
		P.PitchFollow = 0.35f;
		P.BlendTime = 0.55f;
		return P;
	}

	/** At the mouth: the fingers up, the palm toward the face, so what lies across it runs across the mouth. */
	FHeroHoldPose FoodMouth()
	{
		FHeroHoldPose P = FoodCarry();
		P.FingerDir = FVector(0.2f, 0.f, 1.f).GetSafeNormal();
		P.PalmNormal = FVector(-1.f, -0.15f, 0.2f).GetSafeNormal();
		return P;
	}
}

// ------------------------------------------------------------------ the food in the hand

ALyudFoodItem::ALyudFoodItem()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// small and in the hand: nothing for the GI to keep (perf checklist: movers out of Lumen)
	Mesh->bAffectDynamicIndirectLighting = false;
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->SetCastShadow(true);
}

bool ALyudFoodItem::Init(FName InItem, float Left)
{
	Item = InItem;
	StageMeshes.Reset();
	const FLyudFoodDef& F = ULyudFoodSubsystem::FindFood(Item);
	for (int32 K = 0; K < F.Stages; ++K)
	{
		const FString Path = StagePath(Item, K);
		if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
		{
			break;
		}
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
		{
			StageMeshes.Add(M);
		}
	}
	if (StageMeshes.Num() == 0)
	{
		return false;
	}
	Shown = -1;
	SetLeft(Left);
	return true;
}

void ALyudFoodItem::SetLeft(float Left)
{
	if (StageMeshes.Num() == 0)
	{
		return;
	}
	const int32 K = FMath::Clamp(FMath::RoundToInt((1.f - FMath::Clamp(Left, 0.f, 1.f)) * (StageMeshes.Num() - 1)), 0, StageMeshes.Num() - 1);
	if (K != Shown)
	{
		Shown = K;
		Mesh->SetStaticMesh(StageMeshes[K]);
	}
}

// ------------------------------------------------------------------ the subsystem

ULyudFoodSubsystem* ULyudFoodSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudFoodSubsystem>() : nullptr;
}

const FLyudFoodDef& ULyudFoodSubsystem::FindFood(FName Id)
{
	static const TMap<FName, FLyudFoodDef> Foods = LoadFoods();
	static TMap<FName, FLyudFoodDef> Plain;
	if (const FLyudFoodDef* F = Foods.Find(Id); F && F->bHasMouth)
	{
		return *F;
	}
	if (const FLyudFoodDef* F = Plain.Find(Id))
	{
		return *F;
	}
	// any other food: its table look's size, held across the palm, bitten from its top
	FLyudFoodDef F = Foods.Contains(Id) ? Foods[Id] : FLyudFoodDef();
	const FLyudItemDef* Def = ULyudInventory::FindDef(Id);
	const FVector Half = Def && Def->Look != ELyudItemLook::Auto ? Def->Size * 0.5f : FVector(Def ? Def->Radius : 4.f);
	PlainGrip(F, Half);
	return Plain.Add(Id, F);
}

void ULyudFoodSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(&InWorld))
	{
		UI->OnShopBought.AddUObject(this, &ULyudFoodSubsystem::OnBought);
	}
}

void ULyudFoodSubsystem::Deinitialize()
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(GetWorld()))
	{
		UI->OnShopBought.RemoveAll(this);
	}
	Super::Deinitialize();
}

void ULyudFoodSubsystem::OnBought(FName Id, int32)
{
	if (FindFood(Id).bPlated)
	{
		return;              // the diner serves it on a plate (ALyudDiner)
	}
	Bought.AddUnique(Id);
}

void ULyudFoodSubsystem::HandOver(FName Id, const FVector& Where)
{
	HandOvers.Add({ Id, Where });
}

FVector ULyudFoodSubsystem::MouthLocation() const
{
	const ULyudHandUse* Hand = ULyudHandUse::Get(this);
	if (Hand && Hand->IsHolding())
	{
		return Hand->LipsLocation();
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return Pawn ? Pawn->GetPawnViewLocation() - FVector(0.f, 0.f, 9.f) : FVector::ZeroVector;
}

void ULyudFoodSubsystem::Sound(FName Pool, float Volume)
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, MouthLocation(), Volume);
	}
}

bool ULyudFoodSubsystem::HoldFromPocket(FName Id)
{
	UWorld* World = GetWorld();
	ULyudInventory* Inv = ULyudInventory::Get(World);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	ULyudHandUse* Hand = ULyudHandUse::Get(World);
	const FLyudItemDef* Def = ULyudInventory::FindDef(Id);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Inv || !UI || !Hand || !Def || !Pawn || Def->Kind != ELyudItemKind::Food || Inv->Count(Id) < 1)
	{
		return false;
	}
	const FLyudFoodDef& F = FindFood(Id);
	const float Left = Inv->Started.Contains(Id) ? Inv->Started[Id] : 1.f;
	// out of sight under the hand until the hand has it (ULyudHandUse shows it then)
	const FVector At = Pawn->GetActorLocation() - FVector(0.f, 0.f, 60.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = nullptr;
	ALyudFoodItem* Staged = World->SpawnActor<ALyudFoodItem>(At, FRotator::ZeroRotator, Params);
	if (Staged && Staged->Init(Id, Left))
	{
		Actor = Staged;
	}
	else
	{
		if (Staged)
		{
			Staged->Destroy();
		}
		Actor = UI->SpawnHeld(Id, At);
	}
	if (!Actor)
	{
		return false;
	}
	const FVector Scale0 = Actor->GetActorScale3D();
	if (!Staged || Actor != Staged)
	{
		Actor->SetActorScale3D(Scale0 * FMath::Lerp(0.45f, 1.f, Left));
	}
	TWeakObjectPtr<AActor> Weak = Actor;
	TWeakObjectPtr<ULyudFoodSubsystem> Self = this;
	FLyudHandThing T;
	T.Actor = Actor;
	T.Item = Id;
	T.InPalm = F.InPalm;
	T.MouthFull = F.MouthFull;
	T.MouthEmpty = F.MouthEmpty;
	T.Carry = FoodCarry();
	T.Mouth = FoodMouth();
	if (!F.MouthFingers.IsZero() && !F.MouthPalm.IsZero())
	{
		T.Mouth.FingerDir = F.MouthFingers.GetSafeNormal();
		T.Mouth.PalmNormal = F.MouthPalm.GetSafeNormal();
	}
	T.Small = F.Small;
	T.Big = F.Big;
	T.SteadyEach = F.SteadyEach;
	T.SteadyEvery = F.SteadyEvery;
	T.Left = Left;
	const FName Texture = F.Texture;
	const float Chew = F.ChewSeconds;
	T.OnUse = [Self, Weak, Scale0, Texture, Chew](ELyudMouthUse Kind, float Took, float Rest)
	{
		if (ALyudFoodItem* Food = Cast<ALyudFoodItem>(Weak.Get()))
		{
			Food->SetLeft(Rest);
		}
		else if (AActor* A = Weak.Get())
		{
			A->SetActorScale3D(Scale0 * FMath::Lerp(0.45f, 1.f, Rest));
		}
		if (ULyudFoodSubsystem* S = Self.Get())
		{
			S->Sound(FName(*(TEXT("food_bite_") + Texture.ToString())), Kind == ELyudMouthUse::Big ? 1.f : 0.8f);
			// a mouthful takes a while to chew; holding on just keeps the jaw going
			S->ChewLeft = FMath::Min(S->ChewLeft + Took * Chew, 9.f);
			S->NextChew = FMath::Max(S->NextChew, 0.35f);
			S->bSwallow = true;
		}
	};
	T.OnUsedUp = [Self, Id]()
	{
		ULyudFoodSubsystem* S = Self.Get();
		ULyudInventory* I = S ? ULyudInventory::Get(S) : nullptr;
		if (!I)
		{
			return;
		}
		S->Holding = NAME_None;
		I->Started.Remove(Id);
		const FLyudItemDef* D = ULyudInventory::FindDef(Id);
		I->Equip(FName());
		I->Remove(Id);
		if (D && !D->Leaves.IsNone())
		{
			I->Add(D->Leaves);
		}
	};
	T.OnPutAway = [Self, Id](float Rest)
	{
		ULyudFoodSubsystem* S = Self.Get();
		ULyudInventory* I = S ? ULyudInventory::Get(S) : nullptr;
		if (!I)
		{
			return;
		}
		S->Holding = NAME_None;
		if (Rest < 0.999f)
		{
			I->Started.Add(Id, Rest);
		}
		// Q or the diary: the hand is free (it is still in the pocket)
		if (I->Equipped == Id)
		{
			I->Equip(FName());
		}
	};
	if (!Hand->Hold(MoveTemp(T)))
	{
		Actor->Destroy();
		return false;
	}
	Holding = Id;
	Inv->Equip(Id);
	if (F.bWrapped)
	{
		Sound(TEXT("food_wrap"), 0.6f);
	}
	return true;
}

void ULyudFoodSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	ULyudInventory* Inv = ULyudInventory::Get(World);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	ULyudHandUse* Hand = ULyudHandUse::Get(World);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!Inv || !UI || !Hand || !Hero)
	{
		return;
	}

	// the inventory put a food in the hand (or took it away)
	const FName Equipped = Inv->EquippedItem();
	const FLyudItemDef* Def = ULyudInventory::FindDef(Equipped);
	const bool bFood = Def && Def->Kind == ELyudItemKind::Food;
	if (!Holding.IsNone() && (Holding != Equipped || Hand->HeldItem() != Holding))
	{
		if (Hand->HeldItem() == Holding)
		{
			Hand->PutAway();
		}
		Holding = NAME_None;
	}
	if (bFood && Holding.IsNone() && !UI->IsOpen() && !UHeroReachComponent::Get(Hero)->IsBusy())
	{
		if (!HoldFromPocket(Equipped))
		{
			Inv->Equip(FName());
		}
	}

	// what was bought at a counter is handed across it once its screen shuts
	const bool bShop = UI->IsShopOpen();
	if (bShopWasOpen && !bShop && Bought.Num() > 0)
	{
		const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
		const FRotator Yaw(0.f, PC ? PC->GetControlRotation().Yaw : Hero->GetActorRotation().Yaw, 0.f);
		for (int32 I = 0; I < FMath::Min(Bought.Num(), 3); ++I)
		{
			HandOver(Bought[I], Hero->EyeLocation() + Yaw.RotateVector(FVector(52.f, 4.f - 6.f * I, -42.f)));
		}
		Bought.Reset();
	}
	bShopWasOpen = bShop;
	if (!bShop && !UI->IsOpen())
	{
		Bought.Reset();
	}
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	if (HandOvers.Num() > 0 && !Reach->IsBusy() && !UI->IsOpen())
	{
		const FHandOver H = HandOvers[0];
		HandOvers.RemoveAt(0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Look = nullptr;
		ALyudFoodItem* Staged = World->SpawnActor<ALyudFoodItem>(H.Where, FRotator::ZeroRotator, Params);
		if (Staged && Staged->Init(H.Id, 1.f))
		{
			Look = Staged;
		}
		else
		{
			if (Staged)
			{
				Staged->Destroy();
			}
			Look = UI->SpawnHeld(H.Id, H.Where);
		}
		if (Look)
		{
			FReachItem Item;
			if (ALyudGoodsItem* Goods = Cast<ALyudGoodsItem>(Look))
			{
				Goods->SetActorRotation(FRotator(0.f, Hero->GetActorRotation().Yaw, 0.f));
				Item = Goods->MakeReachItem();
			}
			else
			{
				Item.Actor = Look;
				Item.Center = Look->GetComponentsBoundingBox(true).GetCenter();
				Item.Axis = FVector::UpVector;
				Item.Radius = 0.f;
				Item.Front = (Hero->GetActorLocation() - H.Where).GetSafeNormal2D();
			}
			TWeakObjectPtr<ULyudFoodSubsystem> Self = this;
			const bool bWrapped = FindFood(H.Id).bWrapped;
			if (!Reach->Take(Item, [Self, bWrapped] { if (Self.IsValid()) { Self->Sound(bWrapped ? TEXT("food_wrap") : TEXT("diary_take"), 0.5f); } }))
			{
				Look->Destroy();
			}
			UE_LOG(LogTemp, Display, TEXT("FOOD handover %s"), *H.Id.ToString());
		}
	}

	// the jaw after a bite: chewing for a while, then a swallow
	if (ChewLeft > 0.f)
	{
		NextChew -= DeltaTime;
		if (NextChew <= 0.f)
		{
			Sound(TEXT("food_chew"), 0.45f);
			NextChew = 1.45f;
			ChewLeft -= 1.45f;
			if (ChewLeft <= 0.f)
			{
				SwallowIn = 0.6f;
			}
		}
	}
	else if (bSwallow && !Hand->IsAtLips())
	{
		SwallowIn -= DeltaTime;
		if (SwallowIn <= 0.f)
		{
			bSwallow = false;
			Sound(TEXT("food_swallow"), 0.35f);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GFoodHold(TEXT("lyud.Food.Hold"), TEXT("lyud.Food.Hold <item id>: puts one in the pocket and takes it in the hand (checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudInventory* Inv = ULyudInventory::Get(World);
		if (!Inv || Args.Num() == 0 || !ULyudInventory::FindDef(FName(*Args[0])))
		{
			return;
		}
		const FName Id(*Args[0]);
		if (Inv->Count(Id) < 1)
		{
			Inv->Add(Id);
		}
		Inv->Equip(Id);
	}));

static FAutoConsoleCommandWithWorldAndArgs GFoodHandOver(TEXT("lyud.Food.HandOver"), TEXT("lyud.Food.HandOver <item id>: the hand takes one across a counter into the pocket (checks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		ULyudFoodSubsystem* Food = ULyudFoodSubsystem::Get(World);
		if (!Hero || !Food || Args.Num() == 0)
		{
			return;
		}
		const FRotator Yaw(0.f, Hero->GetControlRotation().Yaw, 0.f);
		Food->HandOver(FName(*Args[0]), Hero->EyeLocation() + Yaw.RotateVector(FVector(52.f, 4.f, -42.f)));
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->Add(FName(*Args[0]));
		}
	}));
