#include "South/LyudSouthUses.h"

#include "City/SLyudCityPage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "South/LyudSouthSpot.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

namespace
{
	/** The map's frame in Unreal (docs/world_map.md): X * 100, 790 + Y * 100, Z * 100 cm. */
	constexpr double SouthFaceY = 790.0;
	FVector SouthMapToWorld(double X, double Y, double Z) { return FVector(X * 100.0, SouthFaceY + Y * 100.0, Z * 100.0); }

	/** A page shuts when the reader has stepped this far beyond the spot's reach, cm. */
	constexpr float PageLetGo = 80.f;
	/** How long he stays bent over the water, s. */
	constexpr double DrinkSeconds = 1.6;

	const FName EmptyBottle(TEXT("empty_bottle"));
	const FName WaterItem(TEXT("water"));

	/** Of the spots in reach the nearest has the prompt (as ALyudSouthSpot does for the seats). */
	uint64 GUseFrame = 0;
	float GUseBestDist = 0.f;
	TWeakObjectPtr<const ALyudSouthUse> GUseBest;
	TWeakObjectPtr<const ALyudSouthUse> GUseWinner;

	bool ClaimNearest(const ALyudSouthUse* Spot, float Dist)
	{
		if (GUseFrame != GFrameCounter)
		{
			GUseWinner = GUseFrame + 1 == GFrameCounter ? GUseBest : nullptr;
			GUseBest = nullptr;
			GUseFrame = GFrameCounter;
		}
		if (!GUseBest.IsValid() || Dist < GUseBestDist)
		{
			GUseBest = Spot;
			GUseBestDist = Dist;
		}
		return GUseWinner.Get() == Spot;
	}

	AHeroCharacter* HeroOf(UWorld* World, APlayerController*& OutPC)
	{
		OutPC = World ? World->GetFirstPlayerController() : nullptr;
		return OutPC ? Cast<AHeroCharacter>(OutPC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GSouthUseCmd(TEXT("lyud.South.Use"),
		TEXT("lyud.South.Use [id]: goes to the Aton coast's spot (spring_1, fountain_1, read_trail, read_reeds, read_stele, read_view; "
			 "the nearest without an id) and uses it once; logs a SOUTH line"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = nullptr;
			AHeroCharacter* H = HeroOf(World, PC);
			const ULyudSouthUses* Uses = ULyudSouthUses::Get(World);
			if (!H || !Uses)
			{
				return;
			}
			ALyudSouthUse* Best = nullptr;
			double BestD = 0.0;
			for (const TWeakObjectPtr<ALyudSouthUse>& W : Uses->GetUses())
			{
				ALyudSouthUse* U = W.Get();
				if (!U || (Args.Num() > 0 && U->SpotId != Args[0]))
				{
					continue;
				}
				const double D = FVector::DistSquared(U->GetActorLocation(), H->GetActorLocation());
				if (!Best || D < BestD)
				{
					Best = U;
					BestD = D;
				}
			}
			if (!Best)
			{
				UE_LOG(LogTemp, Display, TEXT("SOUTH use cmd: no such spot (%d spawned)"), Uses->GetUses().Num());
				return;
			}
			const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			H->TeleportTo(Best->GetActorLocation() - Best->GetActorForwardVector() * 40.0 + FVector(0.0, 0.0, Half + 5.0), Best->GetActorRotation());
			Best->Use(H);
			UE_LOG(LogTemp, Display, TEXT("SOUTH use cmd: %s"), *Best->SpotId);
		}));

	FAutoConsoleCommandWithWorld GSouthUsesCmd(TEXT("lyud.South.Uses"), TEXT("Lists the Aton coast's spawned things to do and boat seats"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const ULyudSouthUses* Uses = ULyudSouthUses::Get(World);
			if (!Uses)
			{
				return;
			}
			for (const TWeakObjectPtr<ALyudSouthUse>& W : Uses->GetUses())
			{
				if (const ALyudSouthUse* U = W.Get())
				{
					UE_LOG(LogTemp, Display, TEXT("SOUTH use %s (%s) at %s awake=%d near=%d"), *U->SpotId, *U->Kind.ToString(),
						*U->GetActorLocation().ToCompactString(), U->IsAwake() ? 1 : 0, U->IsNear() ? 1 : 0);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("SOUTH uses: %d spots, %d boat seats"), Uses->GetUses().Num(), Uses->NumSeats());
		}));

	/** Developer mode (F7, Actions, group south): an empty bottle in the pocket, to fill at the spring. */
	LyudDev::FAutoAction GSouthBottle(TEXT("south.empty_bottle"), TEXT("south"), TEXT("dev.action.south.empty_bottle"),
		[](UWorld* World)
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(World))
			{
				Inv->Add(EmptyBottle);
			}
		});
}

// ---------------------------------------------------------------------------------------------------------------------
// a spot

ALyudSouthUse::ALyudSouthUse()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("South"));
}

void ALyudSouthUse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetPrompt(nullptr);
	ClosePage();
	if (AHeroCharacter* D = StandAt > 0.0 ? Drinker.Get() : nullptr)
	{
		D->SetReachCrouch(0.f);
	}
	Super::EndPlay(EndPlayReason);
}

void ALyudSouthUse::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	// near: every frame (E is polled); else no tick at all
	SetActorTickEnabled(bNear || StandAt > 0.0);
	if (!bNear)
	{
		SetPrompt(nullptr);
		ClosePage();
	}
}

void ALyudSouthUse::SetPrompt(const TCHAR* Key)
{
	if (Key == Prompted)
	{
		return;
	}
	Prompted = Key;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Key ? LyudText::Get(Key) : FText::GetEmpty());
	}
}

void ALyudSouthUse::Use(AHeroCharacter* H)
{
	if (!H)
	{
		return;
	}
	if (Kind == TEXT("drink"))
	{
		Drink(H);
	}
	else if (Pages.Num() > 0)
	{
		if (PageIndex + 1 < Pages.Num())
		{
			ShowPage(PageIndex + 1);
		}
		else
		{
			ClosePage();
		}
	}
}

void ALyudSouthUse::Drink(AHeroCharacter* H)
{
	if (StandAt > 0.0 || H->IsSeated() || H->IsOnBike())
	{
		return;
	}
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	if (bFill && Inv && Inv->Count(EmptyBottle) > 0)
	{
		Inv->Remove(EmptyBottle);
		Inv->Add(WaterItem);
		if (Screen)
		{
			Screen->Toast(LyudText::Get(TEXT("southuse.fill.done")));
		}
		UE_LOG(LogTemp, Display, TEXT("SOUTH %s: filled a bottle"), *SpotId);
	}
	else
	{
		// a line of what it tastes like, round and round so two drinks running never say the same
		const bool bSpring = Water == TEXT("spring");
		const int32 Lines = bSpring ? 3 : 2;
		Sip = (Sip % Lines) + 1;
		if (Screen)
		{
			Screen->Toast(LyudText::Get(FString::Printf(TEXT("southuse.drink.%s.%d"), bSpring ? TEXT("spring") : TEXT("fountain"), Sip)));
		}
		UE_LOG(LogTemp, Display, TEXT("SOUTH %s: drank"), *SpotId);
	}
	ULyudQuests::Notify(this, TEXT("use"), Water);
	// bent over the water for a moment (the reach's crouch, as for a snowball or a low shelf), then up again
	H->SetReachCrouch(0.85f);
	Drinker = H;
	StandAt = GetWorld()->GetTimeSeconds() + DrinkSeconds;
	SetActorTickEnabled(true);
}

void ALyudSouthUse::ShowPage(int32 Index)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !Pages.IsValidIndex(Index))
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
		Page->SetScreen(false);
	}
	FString Body;
	for (const FString& Key : Pages[Index])
	{
		Body += (Body.IsEmpty() ? TEXT("") : TEXT("\n")) + LyudText::Get(Key).ToString();
	}
	const bool bMore = Index + 1 < Pages.Num();
	Page->SetPage(Index == 0 ? LyudText::Get(TitleKey) : FText::GetEmpty(), FText::FromString(Body),
		LyudText::Get(bMore ? TEXT("city.page.next") : TEXT("city.page.close")));
	if (Index == 0)
	{
		ULyudQuests::Notify(this, TEXT("read"), SpotId);
		UE_LOG(LogTemp, Display, TEXT("SOUTH %s: reading (%d pages)"), *SpotId, Pages.Num());
	}
	PageIndex = Index;
}

void ALyudSouthUse::ClosePage()
{
	PageIndex = -1;
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ALyudSouthUse::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (StandAt > 0.0 && World && World->GetTimeSeconds() >= StandAt)
	{
		StandAt = 0.0;
		if (AHeroCharacter* D = Drinker.Get())
		{
			D->SetReachCrouch(0.f);
		}
		Drinker.Reset();
		if (!bNear)
		{
			SetActorTickEnabled(false);
			return;
		}
	}
	APlayerController* PC = nullptr;
	AHeroCharacter* H = HeroOf(World, PC);
	if (!PC || !H)
	{
		SetPrompt(nullptr);
		ClosePage();
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bScreen = UI && UI->IsOpen();
	const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = H->GetActorLocation() - FVector(0.0, 0.0, Half);
	const FVector Here = GetActorLocation();
	const float Dist = float(FVector::Dist2D(Feet, Here));
	if (PageIndex >= 0)
	{
		// reading: E turns the page, a step away or the inventory shuts it
		if (bScreen || Dist > Reach + PageLetGo)
		{
			ClosePage();
			return;
		}
		SetPrompt(nullptr);
		if (ANpcTalkDirector::UsePressed(PC, true))
		{
			Use(H);
		}
		return;
	}
	const bool bClose = Dist < Reach && FMath::Abs(Feet.Z - Here.Z) < 160.0;
	const bool bMine = bClose && !bScreen && StandAt <= 0.0 && !H->IsSeated() && !H->IsOnBike() && ClaimNearest(this, Dist);
	const TCHAR* Key = nullptr;
	if (bMine)
	{
		if (Kind == TEXT("drink"))
		{
			const ULyudInventory* Inv = ULyudInventory::Get(this);
			Key = bFill && Inv && Inv->Count(EmptyBottle) > 0 ? TEXT("ui.southuse.fill")
				: Water == TEXT("spring") ? TEXT("ui.southuse.drink") : TEXT("ui.southuse.drink_fountain");
		}
		else
		{
			Key = SpotId == TEXT("read_view") ? TEXT("ui.southuse.look") : TEXT("ui.southuse.read");
		}
	}
	SetPrompt(Key);
	if (bMine && ANpcTalkDirector::UsePressed(PC))
	{
		Use(H);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the spawner

ULyudSouthUses* ULyudSouthUses::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<ULyudSouthUses>() : nullptr;
}

void ULyudSouthUses::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// only where the Aton coast is placed (import_south.py's seats): a dev level gets nothing
	if (!TActorIterator<ALyudSouthSpot>(&InWorld))
	{
		return;
	}
	Spawn(InWorld);
}

void ULyudSouthUses::Spawn(UWorld& World)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const FString Path = FPaths::ProjectContentDir() / TEXT("South/south_uses.json");
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SOUTH uses: no %s"), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("uses"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			const FVector At = SouthMapToWorld(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")),
				O->GetNumberField(TEXT("z")) + (O->HasField(TEXT("z_floor")) ? O->GetNumberField(TEXT("z_floor")) : 0.0));
			const FRotator Rot(0.0, O->GetNumberField(TEXT("yaw")), 0.0);
			ALyudSouthUse* U = World.SpawnActorDeferred<ALyudSouthUse>(ALyudSouthUse::StaticClass(), FTransform(Rot, At), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!U)
			{
				continue;
			}
			U->SpotId = O->GetStringField(TEXT("id"));
			U->Kind = FName(*O->GetStringField(TEXT("kind")));
			U->Reach = O->HasField(TEXT("reach")) ? (float)O->GetNumberField(TEXT("reach")) : 120.f;
			O->TryGetStringField(TEXT("water"), U->Water);
			U->bFill = O->HasField(TEXT("fill")) && O->GetNumberField(TEXT("fill")) > 0.0;
			O->TryGetStringField(TEXT("title"), U->TitleKey);
			const TArray<TSharedPtr<FJsonValue>>* PagesArr = nullptr;
			if (O->TryGetArrayField(TEXT("pages"), PagesArr))
			{
				for (const TSharedPtr<FJsonValue>& PV : *PagesArr)
				{
					const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
					if (PV.IsValid() && PV->TryGetArray(Lines))
					{
						TArray<FString>& Keys = U->Pages.AddDefaulted_GetRef();
						for (const TSharedPtr<FJsonValue>& L : *Lines)
						{
							Keys.Add(L->AsString());
						}
					}
				}
			}
			U->FinishSpawning(FTransform(Rot, At));
			Uses.Add(U);
		}
	}
	// the boats lying keel up: seats like the benches' (ALyudSouthSpot, the same [E] and prompt)
	if (Root->TryGetArrayField(TEXT("seats"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V.IsValid() ? V->AsObject() : nullptr;
			if (!O.IsValid())
			{
				continue;
			}
			const FVector At = SouthMapToWorld(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")), O->GetNumberField(TEXT("z")));
			const FRotator Rot(0.0, O->GetNumberField(TEXT("yaw")), 0.0);
			ALyudSouthSpot* S = World.SpawnActorDeferred<ALyudSouthSpot>(ALyudSouthSpot::StaticClass(), FTransform(Rot, At), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!S)
			{
				continue;
			}
			S->Kind = TEXT("seat");
			S->SeatHeight = O->HasField(TEXT("seat_cm")) ? (float)O->GetNumberField(TEXT("seat_cm")) : 45.f;
			S->Reach = 110.f;
			S->FinishSpawning(FTransform(Rot, At));
			++Seats;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("SOUTH uses: %d things to do, %d boat seats"), Uses.Num(), Seats);
}
