#include "Kitchen/LyudKitchen.h"

#include "KitchenModel.h"
#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/PlayerController.h"
#include "Garden/LyudGarden.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_KITCHEN_DEV 1
#else
#define LYUD_KITCHEN_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudKitchen, Log, All);

namespace LyudKitchenImpl
{
	/** A stove is reached from this far (cm, flat) and this much up or down; the pot is tended from a little further. */
	constexpr float SpotReach = 260.f;
	constexpr float SpotHeight = 220.f;
	constexpr float PotReach = 380.f;
	/** He has walked off from a pot that wants him when he is this far. */
	constexpr float AwayCm = 900.f;
	/** The fire's sound is played again this often while the pot is on (s). */
	constexpr float SoundEvery = 3.5f;

	std::string KStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
	FString KStr(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/** The town's things that are stoves: the act (without its "@role"), the fire, the kitchen's hours, opened by its own act. */
	struct FStoveAct
	{
		const TCHAR* Act;
		uint8 Heat;
		int32 From, To;
		bool bByAct;
	};
	const FStoveAct StoveActs[] = {
		{ TEXT("buy_canteen"), 1, 8, 19, false },              // the commune houses' canteens: the cook lets him to the second ring
		{ TEXT("buy_sanatorium_buffet"), 1, 7, 21, false },    // the sanatorium's dining room: the kitchen behind the buffet
		{ TEXT("hall_boil_soup"), 1, 0, 24, false },           // the town hall's kitchen
		{ TEXT("stove"), 1, 0, 24, false },
		{ TEXT("un_t_stove"), 1, 0, 24, false },
		{ TEXT("keeper_stove"), 0, 0, 24, false },             // the lighthouse keeper's ring
		{ TEXT("t1h1_cube_tea"), 0, 0, 24, false },
		{ TEXT("hot_plate"), 0, 0, 24, true },                 // a room's hot plate: E, the act's own "open": "cook"
	};

	FString CookBareAct(const FString& Act)
	{
		int32 At = INDEX_NONE;
		return Act.FindChar(TEXT('@'), At) ? Act.Left(At) : Act;
	}

	const FStoveAct* StoveOf(const FString& Act)
	{
		const FString Bare = CookBareAct(Act);
		for (const FStoveAct& A : StoveActs)
		{
			if (Bare == A.Act)
			{
				return &A;
			}
		}
		return nullptr;
	}

	FText RecipeName(const LyudKitchen::FRecipe& R)
	{
		return LyudText::Get(TEXT("ui.cook.recipe.") + KStr(R.Id));
	}

	FString CookNeedName(const std::string& Item)
	{
		return Item == LyudKitchen::AnyFish ? LyudText::Get(TEXT("ui.cook.fish")).ToString() : ULyudInventory::ItemName(FName(*KStr(Item))).ToString().ToLower();
	}

	FString CookNeedList(const std::vector<LyudKitchen::FNeed>& Needs)
	{
		FString Out;
		for (const LyudKitchen::FNeed& N : Needs)
		{
			if (!Out.IsEmpty())
			{
				Out += TEXT(", ");
			}
			Out += LyudText::Get(TEXT("ui.cook.need"), { FStringFormatArg(CookNeedName(N.Item)), FStringFormatArg(N.Count) }).ToString();
		}
		return Out;
	}
}

using namespace LyudKitchenImpl;

ULyudKitchen::ULyudKitchen() = default;
ULyudKitchen::~ULyudKitchen() = default;

ULyudKitchen* ULyudKitchen::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudKitchen>() : nullptr;
}

void ULyudKitchen::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Pot = MakePimpl<LyudKitchen::FPot>();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Kitchen"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			ClosePage();
			if (Pot)
			{
				*Pot = LyudKitchen::FPot();
			}
			CookedBits = Dishes = Burnt = VegTaken = VegRoubles = 0;
			VegDay = -1;
		});
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(&InWorld))
	{
		OpenHandle = Things->OnOpen.AddUObject(this, &ULyudKitchen::OnCityOpen);
	}
	bBegun = true;
}

void ULyudKitchen::Deinitialize()
{
	ClosePage();
	SetPrompt(FText::GetEmpty());
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->OnOpen.Remove(OpenHandle);
	}
	bBegun = false;
	Super::Deinitialize();
}

void ULyudKitchen::FindSpots()
{
	// the town's things are read in the background after play begins: asked again until they are there
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (!Things)
	{
		return;
	}
	TArray<FCityThingRef> Refs;
	TArray<const FStoveAct*> Acts;
	Things->GetThingsByAct([&Acts](const FString& Act)
	{
		if (const FStoveAct* A = StoveOf(Act))
		{
			Acts.Add(A);
			return true;
		}
		return false;
	}, Refs);
	if (Refs.Num() != Acts.Num())
	{
		return;
	}
	Spots.Reset(Refs.Num());
	for (int32 I = 0; I < Refs.Num(); ++I)
	{
		FSpot S;
		S.At = Refs[I].Location;
		S.Heat = Acts[I]->Heat;
		S.From = Acts[I]->From;
		S.To = Acts[I]->To;
		S.bByAct = Acts[I]->bByAct;
		Spots.Add(S);
	}
	if (Spots.Num() > 0)
	{
		UE_LOG(LogLyudKitchen, Display, TEXT("KITCHEN %d stoves in town"), Spots.Num());
	}
}

int32 ULyudKitchen::NearestSpot(const FVector& From, float WithinCm, bool bByAct) const
{
	int32 Best = -1;
	float BestDist = WithinCm;
	for (int32 I = 0; I < Spots.Num(); ++I)
	{
		const float Dist = FVector::Dist2D(From, Spots[I].At);
		if (Spots[I].bByAct == bByAct && Dist < BestDist && FMath::Abs(From.Z - Spots[I].At.Z) < SpotHeight)
		{
			Best = I;
			BestDist = Dist;
		}
	}
	return Best;
}

bool ULyudKitchen::SpotOpen(const FSpot& Spot) const
{
	const int32 Hour = UGameCalendar::NowIn(GetWorld()).GetHour();
	return Hour >= Spot.From && Hour < Spot.To;
}

int32 ULyudKitchen::Today() const
{
	return static_cast<int32>((UGameCalendar::NowIn(GetWorld()) - FDateTime(2000, 1, 1)).GetTotalDays());
}

int32 ULyudKitchen::Held(const FString& Item) const
{
	if (Item == UTF8_TO_TCHAR(LyudKitchen::AnyFish))
	{
		const ULyudFishingState* Fish = ULyudFishingState::Get(this);
		return Fish && !Fish->BiggestHeld(this).IsNone() ? FMath::Max(1, Fish->FishHeld(this)) : 0;
	}
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	return Inv ? Inv->Count(FName(*Item)) : 0;
}

void ULyudKitchen::SetPrompt(const FText& Text)
{
	if (Text.EqualTo(PromptText))
	{
		return;
	}
	PromptText = Text;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Text);
	}
}

void ULyudKitchen::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

void ULyudKitchen::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!Hero || !Pot)
	{
		ClosePage();
		SetPrompt(FText::GetEmpty());
		return;
	}
	if (Spots.Num() == 0)
	{
		SpotsIn -= DeltaTime;
		if (SpotsIn <= 0.f)
		{
			SpotsIn = 2.f;
			FindSpots();
		}
	}
	TickPot(DeltaTime, Hero);
	if (Page)
	{
		TickPage(PC);
		return;
	}
	// which stove he stands at (looked for four times a second)
	NearIn -= DeltaTime;
	if (NearIn <= 0.f)
	{
		NearIn = 0.25f;
		const int32 Was = NearSpot;
		NearSpot = NearestSpot(Hero->GetActorLocation(), SpotReach, false);
		if (NearSpot != Was)
		{
			bHinted = false;
		}
		// said once an approach, and only when there is something to cook: the stove's own prompt line stays the thing's
		if (NearSpot >= 0 && !bHinted && !Pot->Busy() && SpotOpen(Spots[NearSpot])
			&& LyudKitchen::AnyToCook(static_cast<LyudKitchen::EHeat>(Spots[NearSpot].Heat), [this](const std::string& Item) { return Held(KStr(Item)); }))
		{
			bHinted = true;
			Toast(LyudText::Get(Spots[NearSpot].Heat ? TEXT("ui.cook.hint") : TEXT("ui.cook.hint_plate")));
		}
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudCityThings* City = ULyudCityThings::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bCalm = UI && UI->CanOpen(true) && !(City && City->HasModeUp()) && !(Menu && Menu->IsOpen()) && !PC->IsMoveInputIgnored();
	if (!bCalm || !PC->WasInputKeyJustPressed(EKeys::Y))
	{
		return;
	}
	if (Pot->Busy())
	{
		if (FVector::Dist2D(Hero->GetActorLocation(), PotAt) > PotReach)
		{
			if (NearSpot >= 0)
			{
				Toast(LyudText::Get(TEXT("ui.cook.busy")));
			}
			return;
		}
		if (LyudKitchen::Tend(*Pot))
		{
			Toast(LyudText::Get(TEXT("ui.cook.tended")));
			SetPrompt(LyudText::Get(TEXT("ui.cook.cooking"), { FStringFormatArg(RecipeName(LyudKitchen::Recipes()[(size_t)Pot->Recipe]).ToString()) }));
		}
		else if (LyudKitchen::CanTakeOff(*Pot))
		{
			TakeOffNow();
		}
		return;
	}
	if (NearSpot >= 0)
	{
		const FSpot& Spot = Spots[NearSpot];
		if (!SpotOpen(Spot))
		{
			Toast(LyudText::Get(TEXT("ui.cook.closed"), { FStringFormatArg(Spot.From), FStringFormatArg(Spot.To) }));
			return;
		}
		OpenPage(NearSpot, Spot.Heat);
	}
}

// --- the recipes' page --------------------------------------------------------------------------------------------------

void ULyudKitchen::OnCityOpen(FName Screen, int32)
{
	if (Screen != TEXT("cook"))
	{
		return;
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Pot && Pot->Busy())
	{
		Toast(LyudText::Get(TEXT("ui.cook.busy")));
		return;
	}
	const int32 Spot = Pawn ? NearestSpot(Pawn->GetActorLocation(), 2.f * SpotReach, true) : -1;
	OpenPage(Spot, Spot >= 0 ? Spots[Spot].Heat : 0);
}

void ULyudKitchen::OpenPage(int32 Spot, uint8 Heat)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !Pot || Pot->Busy())
	{
		return;
	}
	const LyudKitchen::FPockets Pockets = [this](const std::string& Item) { return Held(KStr(Item)); };
	FString Body;
	int32 Line = 0;
	for (const LyudKitchen::FRecipe& R : LyudKitchen::Recipes())
	{
		++Line;
		const FString Name = RecipeName(R).ToString();
		const FString Needs = CookNeedList(R.Needs);
		const std::vector<LyudKitchen::FNeed> Lack = LyudKitchen::Missing(R, Pockets);
		if (!LyudKitchen::CanCookOn(R, static_cast<LyudKitchen::EHeat>(Heat)))
		{
			Body += LyudText::Get(TEXT("ui.cook.line_stove"), { FStringFormatArg(Line), FStringFormatArg(Name), FStringFormatArg(Needs) }).ToString();
		}
		else if (Lack.empty())
		{
			Body += LyudText::Get(TEXT("ui.cook.line_ok"), { FStringFormatArg(Line), FStringFormatArg(Name), FStringFormatArg(Needs) }).ToString();
		}
		else
		{
			Body += LyudText::Get(TEXT("ui.cook.line_lack"), { FStringFormatArg(Line), FStringFormatArg(Name), FStringFormatArg(Needs), FStringFormatArg(CookNeedList(Lack)) }).ToString();
		}
		Body += TEXT("\n");
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	Page->SetPage(LyudText::Get(Heat ? TEXT("ui.cook.title") : TEXT("ui.cook.title_plate")), FText::FromString(Body.TrimEnd()), LyudText::Get(TEXT("ui.cook.keys")));
	PageSpot = Spot;
	PageHeat = Heat;
	PageFrame = GFrameCounter;
	SetPrompt(FText::GetEmpty());
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !MoveBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
}

void ULyudKitchen::ClosePage()
{
	if (Page)
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
		Page.Reset();
	}
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
	PageSpot = -1;
}

void ULyudKitchen::TickPage(APlayerController* PC)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !PC || (UI && UI->IsOpen()))
	{
		ClosePage();
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || GFrameCounter == PageFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Y))
	{
		ClosePage();
		return;
	}
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const int32 Num = FMath::Min<int32>(static_cast<int32>(LyudKitchen::Recipes().size()), UE_ARRAY_COUNT(Digits));
	for (int32 I = 0; I < Num; ++I)
	{
		if (!PC->WasInputKeyJustPressed(Digits[I]))
		{
			continue;
		}
		const int32 Spot = PageSpot;
		const uint8 Heat = PageHeat;
		const APawn* Pawn = PC->GetPawn();
		const FVector At = Spots.IsValidIndex(Spot) ? Spots[Spot].At : (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector);
		ClosePage();
		Start(I, Heat, Spot, At);
		return;
	}
}

// --- the pot ------------------------------------------------------------------------------------------------------------

bool ULyudKitchen::Start(int32 Recipe, uint8 Heat, int32, const FVector& At)
{
	const std::vector<LyudKitchen::FRecipe>& All = LyudKitchen::Recipes();
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Pot || !Inv || Recipe < 0 || Recipe >= static_cast<int32>(All.size()))
	{
		return false;
	}
	const LyudKitchen::FRecipe& R = All[(size_t)Recipe];
	if (Pot->Busy())
	{
		Toast(LyudText::Get(TEXT("ui.cook.busy")));
		return false;
	}
	if (!LyudKitchen::CanCookOn(R, static_cast<LyudKitchen::EHeat>(Heat)))
	{
		return false;
	}
	const std::vector<LyudKitchen::FNeed> Lack = LyudKitchen::Missing(R, [this](const std::string& Item) { return Held(KStr(Item)); });
	if (!Lack.empty())
	{
		Toast(LyudText::Get(TEXT("ui.cook.lack"), { FStringFormatArg(CookNeedList(Lack)) }));
		return false;
	}
	// the berries' quality is the bed's last picking (the jar is as good as the worse of berries and cooking)
	const ULyudGarden* Garden = ULyudGarden::Get(this);
	PotBerries = Garden ? Garden->HarvestQuality(TEXT("strawberries")) : 2;
	ULyudFishingState* Fish = ULyudFishingState::Get(this);
	for (const LyudKitchen::FNeed& N : R.Needs)
	{
		if (N.Item == LyudKitchen::AnyFish)
		{
			// the biggest fish of the catch goes into the pot (its weight leaves the bag with it)
			for (int32 K = 0; Fish && K < N.Count; ++K)
			{
				Fish->TakeOne(this, Fish->BiggestHeld(this));
			}
		}
		else
		{
			Inv->Remove(FName(*KStr(N.Item)), N.Count);
		}
	}
	LyudKitchen::Begin(*Pot, Recipe, static_cast<LyudKitchen::EHeat>(Heat));
	PotAt = At;
	SoundIn = 0.f;
	bAwayWarned = false;
	const FString Name = RecipeName(R).ToString();
	Toast(LyudText::Get(R.Sound.empty() ? TEXT("ui.cook.begun_cold") : TEXT("ui.cook.begun"), { FStringFormatArg(Name) }));
	SetPrompt(LyudText::Get(TEXT("ui.cook.cooking"), { FStringFormatArg(Name) }));
	UE_LOG(LogLyudKitchen, Display, TEXT("KITCHEN on the fire: %s"), *KStr(R.Id));
	return true;
}

void ULyudKitchen::TickPot(float DeltaTime, const AHeroCharacter* Hero)
{
	if (!Pot->Busy())
	{
		return;
	}
	const LyudKitchen::FRecipe& R = LyudKitchen::Recipes()[(size_t)Pot->Recipe];
	const FString Name = RecipeName(R).ToString();
	const float Dist = FVector::Dist2D(Hero->GetActorLocation(), PotAt);
	// the fire's sound, while it is on the fire (a pool the soundscape does not have plays nothing)
	if (!R.Sound.empty() && Pot->State != LyudKitchen::EPot::Burnt)
	{
		SoundIn -= DeltaTime;
		if (SoundIn <= 0.f)
		{
			SoundIn = SoundEvery;
			if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
			{
				Sound->PlayEffect(FName(*KStr(R.Sound)), PotAt, 0.8f);
			}
		}
	}
	const LyudKitchen::EPotEvent Event = LyudKitchen::Step(*Pot, FMath::Min(DeltaTime, 0.25f));
	FText Line;
	switch (Event)
	{
	case LyudKitchen::EPotEvent::StepOpened:
		Line = LyudText::Get(TEXT("ui.cook.step.") + KStr(R.Steps[(size_t)Pot->OpenStep].What));
		break;
	case LyudKitchen::EPotEvent::StepMissed:
		Toast(LyudText::Get(TEXT("ui.cook.missed")));
		SetPrompt(LyudText::Get(TEXT("ui.cook.cooking"), { FStringFormatArg(Name) }));
		break;
	case LyudKitchen::EPotEvent::Ready:
		Line = LyudText::Get(TEXT("ui.cook.ready"));
		break;
	case LyudKitchen::EPotEvent::Overdone:
		Line = LyudText::Get(TEXT("ui.cook.over"));
		break;
	case LyudKitchen::EPotEvent::Burnt:
		Line = LyudText::Get(TEXT("ui.cook.burnt"));
		break;
	default:
		break;
	}
	if (!Line.IsEmpty())
	{
		// the pot calls: on the prompt line, and aloud once when he has walked off
		SetPrompt(Line);
		bAwayWarned = false;
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(TEXT("steam_hiss"), PotAt, 1.f);
		}
	}
	if (Dist > AwayCm && !bAwayWarned && (Pot->OpenStep >= 0 || LyudKitchen::CanTakeOff(*Pot)))
	{
		bAwayWarned = true;
		Toast(LyudText::Get(TEXT("ui.cook.away"), { FStringFormatArg(Name) }));
	}
}

void ULyudKitchen::TakeOffNow()
{
	const int32 Recipe = Pot->Recipe;
	const LyudKitchen::FRecipe& R = LyudKitchen::Recipes()[(size_t)Recipe];
	const FString Name = RecipeName(R).ToString();
	const LyudKitchen::FResult Out = LyudKitchen::TakeOff(*Pot);
	SetPrompt(FText::GetEmpty());
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	ULyudQuests* Quests = ULyudQuests::Get(this);
	UE_LOG(LogLyudKitchen, Display, TEXT("KITCHEN off the fire: %s quality %d x%d"), *KStr(R.Id), Out.Quality, Out.Count);
	if (!Out.bOk)
	{
		Toast(LyudText::Get(TEXT("ui.cook.result.0")));
		if (Burnt++ == 0 && Quests)
		{
			Quests->AddNote(TEXT("cook.diary.burnt"));
		}
		if (Things)
		{
			Things->GiveStamp(TEXT("cook/burnt"));
		}
		return;
	}
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(FName(*KStr(Out.Dish)), Out.Count);
	}
	Toast(LyudText::Get(FString::Printf(TEXT("ui.cook.result.%d"), FMath::Clamp(Out.Quality, 1, 3)), { FStringFormatArg(Name), FStringFormatArg(Out.Count) }));
	const int32 AllBits = (1 << static_cast<int32>(LyudKitchen::Recipes().size())) - 1;
	const bool bWasAll = (CookedBits & AllBits) == AllBits;
	if (Dishes++ == 0 && Quests)
	{
		Quests->AddNote(TEXT("cook.diary.first"), { Name });
	}
	CookedBits |= 1 << Recipe;
	if (!bWasAll && (CookedBits & AllBits) == AllBits && Quests)
	{
		Quests->AddNote(TEXT("cook.diary.all"));
	}
	if (Things)
	{
		Things->GiveStamp(TEXT("cook/") + KStr(R.Id));
		if (Out.Quality >= 3)
		{
			Things->GiveStamp(TEXT("cook/fine"));
		}
	}
	if (R.Id == "jam")
	{
		if (ULyudGarden* Garden = ULyudGarden::Get(this))
		{
			Garden->OwnJam(LyudKitchen::JamQuality(PotBerries, Out.Quality));
		}
		Toast(LyudText::Get(TEXT("ui.cook.jam_jar")));
	}
	ULyudQuests::Notify(this, TEXT("cook"), TEXT("dish"));
	ULyudQuests::Notify(this, TEXT("cook"), KStr(R.Id));
}

// --- the restaurant buys fresh vegetables ------------------------------------------------------------------------------

int32 ULyudKitchen::VegOffer(int32* OutRoubles) const
{
	const int32 Left = LyudKitchen::VegPerDay - (VegDay == Today() ? VegTaken : 0);
	int Roubles = 0;
	int32 Things = 0;
	for (const LyudKitchen::FVegPick& P : LyudKitchen::VegOffer([this](const std::string& Item) { return Held(KStr(Item)); }, Left, Roubles))
	{
		Things += P.Count;
	}
	if (OutRoubles)
	{
		*OutRoubles = Roubles;
	}
	return Things;
}

int32 ULyudKitchen::SellVeg()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return 0;
	}
	const int32 Day = Today();
	if (VegDay != Day)
	{
		VegDay = Day;
		VegTaken = 0;
	}
	int Roubles = 0;
	int32 Got = 0;
	for (const LyudKitchen::FVegPick& P : LyudKitchen::VegOffer([this](const std::string& Item) { return Held(KStr(Item)); }, LyudKitchen::VegPerDay - VegTaken, Roubles))
	{
		if (Inv->Remove(FName(*KStr(P.Item)), P.Count))
		{
			Got += P.Roubles;
			VegTaken += P.Count;
		}
	}
	if (Got > 0)
	{
		Inv->Earn(Got);
		if (VegRoubles == 0)
		{
			if (ULyudQuests* Quests = ULyudQuests::Get(this))
			{
				Quests->AddNote(TEXT("cook.diary.veg"));
			}
		}
		VegRoubles += Got;
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->GiveStamp(TEXT("cook/veg"));
		}
		UE_LOG(LogLyudKitchen, Display, TEXT("KITCHEN vegetables sold for %d roubles (%d taken today)"), Got, VegTaken);
	}
	return Got;
}

// --- console and developer mode ----------------------------------------------------------------------------------------

FString ULyudKitchen::Report() const
{
	FString Out = FString::Printf(TEXT("KITCHEN stoves %d, near %d, dishes %d (bits %d), burnt %d, vegetables today %d (all %d roubles)"),
		Spots.Num(), NearSpot, Dishes, CookedBits, Burnt, VegDay == Today() ? VegTaken : 0, VegRoubles);
	if (Pot && Pot->Busy())
	{
		Out += FString::Printf(TEXT("; on the fire %s %.1f s state %d hit %d missed %d"), *KStr(LyudKitchen::Recipes()[(size_t)Pot->Recipe].Id), Pot->T,
			static_cast<int32>(Pot->State), Pot->Hit, Pot->Missed);
	}
	return Out;
}

void ULyudKitchen::DevOpen()
{
	OpenPage(-1, 1);
}

bool ULyudKitchen::DevCook(const FString& Recipe)
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return Pawn && Start(LyudKitchen::FindRecipe(KStd(Recipe)), 1, -1, Pawn->GetActorLocation());
}

void ULyudKitchen::DevFinish()
{
	if (!Pot)
	{
		return;
	}
	if (!Pot->Busy())
	{
		const std::vector<LyudKitchen::FRecipe>& All = LyudKitchen::Recipes();
		for (size_t R = 0; R < All.size() && !Pot->Busy(); ++R)
		{
			if (LyudKitchen::Missing(All[R], [this](const std::string& Item) { return Held(KStr(Item)); }).empty())
			{
				DevCook(KStr(All[R].Id));
			}
		}
	}
	if (Pot->Busy())
	{
		const LyudKitchen::FRecipe& R = LyudKitchen::Recipes()[(size_t)Pot->Recipe];
		Pot->NextStep = static_cast<int>(R.Steps.size());
		Pot->OpenStep = -1;
		Pot->Missed = 0;
		Pot->T = R.Seconds;
		Pot->State = LyudKitchen::EPot::Ready;
		TakeOffNow();
	}
}

void ULyudKitchen::DevKit()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	Inv->Add(TEXT("potatoes"), 5);
	Inv->Add(TEXT("dill"), 3);
	Inv->Add(TEXT("tomatoes"), 2);
	Inv->Add(TEXT("radish"), 2);
	Inv->Add(TEXT("strawberries"), 6);
	// two perch for the ukha and the frying pan (their weight goes into the bag as a catch's would)
	Inv->Add(TEXT("fish_perch"), 2);
	if (ULyudFishingState* Fish = ULyudFishingState::Get(this))
	{
		Fish->Pocket(TEXT("Perch"), 300.f);
		Fish->Pocket(TEXT("Perch"), 300.f);
	}
}

namespace LyudKitchenConsole
{
	ULyudKitchen* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudKitchen>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GInfo(TEXT("lyud.Kitchen.Info"), TEXT("The kitchen: stoves found, what is on the fire, dishes cooked."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudKitchen* K = Of(World)) { UE_LOG(LogLyudKitchen, Display, TEXT("%s"), *K->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GOpen(TEXT("lyud.Kitchen.Open"), TEXT("The recipes' page wherever the hero stands (as at a stove)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudKitchen* K = Of(World)) { K->DevOpen(); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GCook(TEXT("lyud.Kitchen.Cook"), TEXT("lyud.Kitchen.Cook <ukha|fried_potatoes|fried_fish|salad|jam>: puts it on the fire wherever the hero stands."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudKitchen* K = Of(World))
			{
				const bool bOk = Args.Num() > 0 && K->DevCook(Args[0]);
				UE_LOG(LogLyudKitchen, Display, TEXT("KITCHEN cook %s: %s"), Args.Num() > 0 ? *Args[0] : TEXT("?"), bOk ? TEXT("on the fire") : TEXT("no"));
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GFinish(TEXT("lyud.Kitchen.Finish"), TEXT("What is on the fire is done now without a slip and comes off (nothing on: cooks the first dish the pockets allow)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudKitchen* K = Of(World)) { K->DevFinish(); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GKit(TEXT("lyud.Kitchen.Kit"), TEXT("Ingredients for all five dishes into the pockets."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudKitchen* K = Of(World)) { K->DevKit(); }
		}));
}

#if LYUD_KITCHEN_DEV
namespace LyudKitchenDev
{
	using namespace LyudDev;
	FAutoAction GKitAction(TEXT("kitchen.kit"), TEXT("kitchen"), TEXT("dev.action.kitchen.kit"),
		[](UWorld* W) { if (ULyudKitchen* K = LyudKitchenConsole::Of(W)) { K->DevKit(); } });
	FAutoAction GCookAction(TEXT("kitchen.cook"), TEXT("kitchen"), TEXT("dev.action.kitchen.cook"),
		[](UWorld* W) { if (ULyudKitchen* K = LyudKitchenConsole::Of(W)) { K->DevFinish(); } });
}
#endif
