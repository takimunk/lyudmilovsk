#include "Fishing/LyudFishing.h"

#include "../Sea/EcoModel.h"
#include "../Sea/FishModel.h"
#include "Audio/LyudSoundscape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FishingModel.h"
#include "FishingPlaces.h"
#include "Fishing/LyudRowBoat.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudEcosystem.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "Sea/LyudUnderwater.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"
#include "UI/LyudGameScreen.h"

namespace LyudF = LyudFishing;

const FName ULyudFishing::RodId(TEXT("fishing_rod"));
const FName ULyudFishingState::DinerFishId(TEXT("diner_fish"));

namespace
{
	TAutoConsoleVariable<int32> CVarFishing(TEXT("lyud.Fishing"), 1, TEXT("The fishing rod's mouse buttons (0: the rod comes out but does nothing)."));

	float FishEase(float T) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f)); }

	/** The right palm's frame from the hand's bones (as ULyudHandUse's): X the fingers, Z out of the palm, world. */
	bool FishBonePalm(const USkeletalMeshComponent* Body, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		const FVector Across = Body->GetBoneLocation(TEXT("thumb_01_r")) - Body->GetBoneLocation(TEXT("pinky_01_r"));
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		if (FVector::DotProduct(Pn, Normal) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Wrist + (Knuckle - Wrist) * 0.6f);
		return true;
	}

	/**
	 * The rod in the palm: a hammer grip on the reel seat, the reel's foot between the middle and ring fingers. The rod runs
	 * along the palm's Y (across the palm, thumb side forward), the reel hangs on the fingers' side (palm -X), the rod's axis a
	 * finger's thickness out of the palm.
	 */
	FTransform RodInPalm()
	{
		return FTransform(FRotationMatrix::MakeFromXZ(FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f)).ToQuat(), FVector(0.f, 0.f, 1.7f));
	}

	/** The hand holding the rod at Pos (look frame: forward, right, up from the eyes), the rod raised Elev degrees and turned Yaw. */
	FHeroHoldPose RodPose(const FVector& Pos, float Elev, float Yaw)
	{
		const float E = FMath::DegreesToRadians(Elev), Y = FMath::DegreesToRadians(Yaw);
		const FVector R(FMath::Cos(E) * FMath::Cos(Y), FMath::Cos(E) * FMath::Sin(Y), FMath::Sin(E));
		const FVector N0(0.f, -1.f, 0.f);
		const FVector N = (N0 - R * FVector::DotProduct(N0, R)).GetSafeNormal();
		FHeroHoldPose P;
		P.Pos = Pos;
		P.FingerDir = FVector::CrossProduct(R, N).GetSafeNormal();
		P.PalmNormal = N;
		P.Curl = 0.9f;
		P.PitchFollow = 0.5f;
		P.BlendTime = 0.35f;
		// the fingers close on the reel seat
		P.Shape.AddCapsule(FVector(0.f, 0.f, 1.7f), FVector(0.f, 1.f, 0.f), 5.f, 1.1f);
		return P;
	}

	/** The species' mean group (a school is one weight in the web for many fish on the hook). */
	float SchoolFactor(int32 Species)
	{
		const LyudFish::FSpecies& Sp = LyudFish::Species(Species);
		return FMath::Sqrt(0.5f * float(Sp.GroupMin + Sp.GroupMax));
	}

	const TCHAR* FishStateName(LyudF::EState S)
	{
		switch (S)
		{
		case LyudF::EState::Idle: return TEXT("idle");
		case LyudF::EState::Charging: return TEXT("charging");
		case LyudF::EState::Swing: return TEXT("swing");
		case LyudF::EState::Flying: return TEXT("flying");
		case LyudF::EState::Waiting: return TEXT("waiting");
		case LyudF::EState::Nibbling: return TEXT("nibbling");
		case LyudF::EState::Taking: return TEXT("taking");
		case LyudF::EState::Fighting: return TEXT("fighting");
		case LyudF::EState::Landing: return TEXT("landing");
		case LyudF::EState::Winding: return TEXT("winding");
		}
		return TEXT("?");
	}

	/** The item and species names of each catchable, made once (asked every frame at a counter: no strings then). */
	struct FFishNames
	{
		TArray<FName> Items, Species;
		TArray<float> Usual;
		TMap<FName, int32> Baits;
		FFishNames()
		{
			for (int32 C = 0; C < LyudF::NumCatchable(); C++)
			{
				const LyudF::FCatchable& F = LyudF::Catchable(C);
				Items.Add(FName(ANSI_TO_TCHAR(LyudF::ItemOf(F.Id).c_str())));
				Species.Add(FName(ANSI_TO_TCHAR(F.Id)));
				Usual.Add(F.K * FMath::Cube(F.Length * 100.f) / 100.f);
			}
			for (int32 B = 0; B < LyudF::NumBaits(); B++)
			{
				Baits.Add(FName(ANSI_TO_TCHAR(LyudF::Bait(B).Item)), B);
			}
		}
	};
	const FFishNames& FishNames()
	{
		static const FFishNames Names;
		return Names;
	}
	FName FishItem(int32 Index) { return FishNames().Items[Index]; }
	FName FishSpeciesName(int32 Index) { return FishNames().Species[Index]; }
	/** The bait index of an item (-1: not a bait), without a string. */
	int32 FishBaitOf(FName Item)
	{
		const int32* B = FishNames().Baits.Find(Item);
		return B ? *B : -1;
	}
	FText FishName(FName Species)
	{
		return LyudText::Get(FString::Printf(TEXT("fish.%s.name"), *Species.ToString().ToLower()));
	}

	/** The light and weather at the water now (the sky's sun, the weather's wind, rain, pressure; the web's bloom). */
	LyudF::FConditions FishConditions(const UObject* WorldContext)
	{
		LyudF::FConditions C;
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!World)
		{
			return C;
		}
		// (the web keeps the level's sky: asked every few seconds while a float is out)
		const ULyudEcosystem* Web = ULyudEcosystem::Get(World);
		if (const ALyudSky* Sky = Web ? Web->FindSky(World) : nullptr)
		{
			C.SunAltitude = Sky->SunAltitude();
		}
		else if (TActorIterator<ALyudSky> It(World); It)
		{
			C.SunAltitude = It->SunAltitude();
		}
		if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(World))
		{
			const FLyudWeatherState St = Wx->GetState();
			C.Wind = St.WindSpeed;
			C.PressureTendency = St.PressureTendency;
			C.Cloud = St.CloudCover;
			const FLyudSkyWeather Sky = Wx->GetSky();
			C.Rain = Sky.Rain;
			C.Thunder = Sky.Thunder;
		}
		if (const ULyudEcosystem* Eco = ULyudEcosystem::Get(World))
		{
			C.Murk = Eco->GetBloom();
		}
		return C;
	}

	/** Each catchable's food in reach against the usual, from its group's diet in the web (its prey's numbers now). */
	void FishHunger(const ULyudEcosystem* Eco, TArray<float>& Out)
	{
		Out.SetNum(LyudF::NumCatchable());
		for (int32 C = 0; C < LyudF::NumCatchable(); C++)
		{
			Out[C] = 1.f;
			const int32 N = Eco ? LyudEco::Find(LyudF::Catchable(C).Id) : -1;
			if (N < 0) continue;
			float Food = 0.f, Share = 0.f;
			for (const LyudEco::FDietItem& D : LyudEco::Node(N).Diet)
			{
				Food += D.Share * Eco->GetAbundance(FName(ANSI_TO_TCHAR(D.Prey)));
				Share += D.Share;
			}
			if (Share > 0.f) Out[C] = LyudF::Hunger(Food / Share);
		}
	}

	/** A fish's length from its weight (Fulton's K), m. */
	float FishLengthOf(int32 C, float Grams)
	{
		const LyudF::FCatchable& F = LyudF::Catchable(C);
		return F.K > 0.f ? FMath::Pow(FMath::Max(Grams, 0.f) * 100.f / F.K, 1.f / 3.f) * 0.01f : F.Length;
	}

	int32 FishDay(const UObject* WorldContext)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		return int32(UGameCalendar::NowIn(World).GetTicks() / ETimespan::TicksPerDay);
	}
}

// ------------------------------------------------------------------ the saved state

ULyudFishingState* ULyudFishingState::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudFishingState>() : nullptr;
}

void ULyudFishingState::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Fishing"), this);
	}
	Reset();
}

void ULyudFishingState::Reset()
{
	Bait = TEXT("bait_worm");
	bBaited = false;
	Log.Reset();
	BagGrams.Reset();
	BagFish.Reset();
	bBoatSaved = false;
	bBoatAboard = false;
	BoatPaidDay = -1;
	Total = 0;
	Cooked = 0;
	Journal.Reset();
	Rod = NAME_None;
	KitchenDay = -1;
	KitchenTaken.Reset();
	KitchenRoubles = 0;
	KitchenSold = 0;
}

void ULyudFishingState::Note(const FLyudCatchEntry& Entry)
{
	Journal.Add(Entry);
	if (Journal.Num() > JournalMax)
	{
		Journal.RemoveAt(0, Journal.Num() - JournalMax);
	}
}

namespace
{
	/** One fish a kitchen would take: its catchable, its price there. */
	struct FKitchenPick { int32 Fish; int32 Price; float Grams; };
	using FKitchenPicks = TArray<FKitchenPick, TInlineAllocator<32>>;

	/** The fish in the pocket a kitchen wants, dearest first, at most Left of them. */
	void KitchenPicks(const ULyudFishingState& State, const ULyudInventory* Inv, int32 Left, FKitchenPicks& Out)
	{
		Out.Reset();
		for (int32 C = 0; Inv && Left > 0 && C < LyudF::NumCatchable(); C++)
		{
			const int32 Held = Inv->Count(FishItem(C));
			if (Held <= 0) continue;
			// each fish by its own weight (a big cod is wanted even with small ones beside it); those without an entry at the rest's share
			const FName Species = FishSpeciesName(C);
			int32 Listed = 0;
			float ListedGrams = 0.f;
			for (const FLyudBagFish& B : State.BagFish)
			{
				if (B.Species != Species || Listed >= Held) continue;
				Listed++;
				ListedGrams += B.Grams;
				const int32 Price = LyudF::RestaurantPrice(C, B.Grams, FishLengthOf(C, B.Grams));
				if (Price > 0) Out.Add({ C, Price, B.Grams });
			}
			if (Listed < Held)
			{
				const float* G = State.BagGrams.Find(Species);
				const float Rest = G ? FMath::Max(*G - ListedGrams, 0.f) : 0.f;
				const float Each = Rest > 0.f ? Rest / (Held - Listed) : FishNames().Usual[C];
				const int32 Price = LyudF::RestaurantPrice(C, Each, FishLengthOf(C, Each));
				for (int32 K = Listed; Price > 0 && K < Held; K++) Out.Add({ C, Price, -1.f });
			}
		}
		Out.Sort([](const FKitchenPick& A, const FKitchenPick& B) { return A.Price > B.Price; });
		if (Out.Num() > Left) Out.SetNum(FMath::Max(Left, 0));
	}
}

int32 ULyudFishingState::KitchenOffer(const UObject* WorldContext, FName Kitchen, int32 PerDay, int32* OutRoubles) const
{
	const int32* Taken = KitchenDay == FishDay(WorldContext) ? KitchenTaken.Find(Kitchen) : nullptr;
	FKitchenPicks Picks;
	KitchenPicks(*this, ULyudInventory::Get(WorldContext), PerDay - (Taken ? *Taken : 0), Picks);
	int32 Roubles = 0;
	for (const FKitchenPick& P : Picks) Roubles += P.Price;
	if (OutRoubles) *OutRoubles = Roubles;
	return Picks.Num();
}

int32 ULyudFishingState::SellToKitchen(const UObject* WorldContext, FName Kitchen, int32 PerDay)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	const int32 Day = FishDay(WorldContext);
	if (KitchenDay != Day)
	{
		KitchenDay = Day;
		KitchenTaken.Reset();
	}
	int32& Taken = KitchenTaken.FindOrAdd(Kitchen);
	FKitchenPicks Picks;
	KitchenPicks(*this, Inv, PerDay - Taken, Picks);
	int32 Roubles = 0;
	for (const FKitchenPick& P : Picks)
	{
		TakeFish(WorldContext, P.Fish, P.Grams);
		Roubles += P.Price;
	}
	if (Inv && Roubles > 0)
	{
		Inv->Earn(Roubles);
	}
	Taken += Picks.Num();
	KitchenRoubles += Roubles;
	KitchenSold += Picks.Num();
	UE_LOG(LogTemp, Display, TEXT("FISHING kitchen %s bought %d fish for %d roubles"), *Kitchen.ToString(), Picks.Num(), Roubles);
	return Roubles;
}

int32 ULyudFishingState::FishHeld(const UObject* WorldContext, int32* OutRoubles) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	int32 N = 0, Roubles = 0;
	for (int32 C = 0; Inv && C < LyudF::NumCatchable(); C++)
	{
		// only what the Gastronom buys (SellAll leaves the sand gobies and sticklebacks in the pocket)
		const int32 Held = LyudF::Catchable(C).PerKilo > 0 ? Inv->Count(FishItem(C)) : 0;
		if (Held <= 0) continue;
		N += Held;
		// each priced at the mean weight of those held (a fish given or found: the species' usual weight)
		const FName Species = FishSpeciesName(C);
		const float* G = BagGrams.Find(Species);
		const float Usual = FishNames().Usual[C];
		const float Each = G && *G > 0.f ? *G / Held : Usual;
		Roubles += Held * LyudF::PriceOf(C, Each);
	}
	if (OutRoubles) *OutRoubles = Roubles;
	return N;
}

int32 ULyudFishingState::SellAll(const UObject* WorldContext)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	int32 Roubles = 0;
	if (!Inv || FishHeld(WorldContext, &Roubles) == 0)
	{
		return 0;
	}
	for (int32 C = 0; C < LyudF::NumCatchable(); C++)
	{
		const FName Item = FishItem(C);
		const int32 Held = Inv->Count(Item);
		// the ones nobody buys (sand gobies, sticklebacks) stay in the pocket
		if (Held > 0 && LyudF::Catchable(C).PerKilo > 0)
		{
			Inv->Remove(Item, Held);
			BagGrams.Remove(FishSpeciesName(C));
			const FName Species = FishSpeciesName(C);
			BagFish.RemoveAll([Species](const FLyudBagFish& B) { return B.Species == Species; });
		}
	}
	Inv->Earn(Roubles);
	UE_LOG(LogTemp, Display, TEXT("FISHING sold for %d roubles"), Roubles);
	return Roubles;
}

FName ULyudFishingState::BiggestHeld(const UObject* WorldContext, float* OutGrams) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	FName Best;
	float BestGrams = 0.f;
	for (int32 C = 0; Inv && C < LyudF::NumCatchable(); C++)
	{
		const int32 Held = Inv->Count(FishItem(C));
		if (Held <= 0) continue;
		const float* G = BagGrams.Find(FishSpeciesName(C));
		const float Usual = FishNames().Usual[C];
		const float Each = G && *G > 0.f ? *G / Held : Usual;
		if (Best.IsNone() || Each > BestGrams)
		{
			Best = FishItem(C);
			BestGrams = Each;
		}
	}
	if (OutGrams) *OutGrams = BestGrams;
	return Best;
}

float ULyudFishingState::TakeOne(const UObject* WorldContext, FName Item)
{
	// the biggest of that kind (the cook fries the biggest)
	const int32 C = FishNames().Items.IndexOfByKey(Item);
	return C == INDEX_NONE ? 0.f : TakeFish(WorldContext, C, -2.f);
}

float ULyudFishingState::TakeFish(const UObject* WorldContext, int32 C, float Grams)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	const FName Item = FishItem(C);
	const int32 Held = Inv ? Inv->Count(Item) : 0;
	if (Held <= 0)
	{
		return 0.f;
	}
	const FName Species = FishSpeciesName(C);
	// which entry: the one of these grams, the biggest (-2), or none listed (-1: an unlisted one at the rest's share)
	int32 Pick = INDEX_NONE;
	for (int32 I = 0; Grams != -1.f && I < BagFish.Num(); I++)
	{
		if (BagFish[I].Species != Species) continue;
		if (Grams >= 0.f ? FMath::IsNearlyEqual(BagFish[I].Grams, Grams, 0.01f) : (Pick == INDEX_NONE || BagFish[I].Grams > BagFish[Pick].Grams))
		{
			Pick = I;
			if (Grams >= 0.f) break;
		}
	}
	float Took = 0.f;
	if (Pick != INDEX_NONE)
	{
		Took = BagFish[Pick].Grams;
		BagFish.RemoveAt(Pick);
	}
	else
	{
		float Listed = 0.f;
		int32 NListed = 0;
		for (const FLyudBagFish& B : BagFish) if (B.Species == Species) { Listed += B.Grams; NListed++; }
		const float* G = BagGrams.Find(Species);
		const float Rest = G ? FMath::Max(*G - Listed, 0.f) : 0.f;
		Took = Rest > 0.f && Held > NListed ? Rest / (Held - NListed) : FishNames().Usual[C];
	}
	if (float* G = BagGrams.Find(Species))
	{
		*G = FMath::Max(*G - Took, 0.f);
		if (Held == 1) BagGrams.Remove(Species);
	}
	Inv->Remove(Item, 1);
	return Took;
}

void ULyudFishingState::Pocket(FName Species, float Grams)
{
	BagGrams.FindOrAdd(Species) += Grams;
	BagFish.Add({ Species, Grams });
}

void ULyudFishingState::OnGameSaving_Implementation()
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (const ALyudRowBoat* Boat = ALyudRowBoat::Get(World))
	{
		Boat->Store(*this);
	}
}

void ULyudFishingState::OnGameLoaded_Implementation()
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (ALyudRowBoat* Boat = ALyudRowBoat::Get(World))
	{
		Boat->Restore(*this);
	}
}

bool ULyudFishingState::Cook(const UObject* WorldContext, FName Item, int32 Fee)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(WorldContext);
	if (!Inv || !UI || Item.IsNone() || Inv->Count(Item) <= 0 || !Inv->Spend(Fee))
	{
		return false;
	}
	const float Grams = TakeOne(WorldContext, Item);
	// the plate goes the way of anything ordered at the diner: into the pocket for a moment, then ALyudDiner takes it back out
	// and the waitress brings it to the counter in front of the hero
	Inv->Add(DinerFishId);
	UI->OnShopBought.Broadcast(DinerFishId, 1);
	Cooked++;
	UE_LOG(LogTemp, Display, TEXT("FISHING cooked %s (%.0f g) for %d roubles"), *Item.ToString(), Grams, Fee);
	return true;
}

FLyudCatchRecord* ULyudFishingState::Find(FName Species)
{
	return Log.FindByPredicate([Species](const FLyudCatchRecord& R) { return R.Species == Species; });
}

bool ULyudFishingState::Record(FName Species, float Grams, float Cm, const FDateTime& When)
{
	Total++;
	FLyudCatchRecord* R = Find(Species);
	if (!R)
	{
		R = &Log.AddDefaulted_GetRef();
		R->Species = Species;
		R->First = When;
		R->Count = 1;
		R->BiggestGrams = Grams;
		R->BiggestCm = Cm;
		return true;
	}
	R->Count++;
	if (Grams > R->BiggestGrams)
	{
		R->BiggestGrams = Grams;
		R->BiggestCm = Cm;
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ the subsystem

ULyudFishing::ULyudFishing() = default;
ULyudFishing::~ULyudFishing() = default;

ULyudFishing* ULyudFishing::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudFishing>() : nullptr;
}

bool ULyudFishing::IsRod(FName Item)
{
	// the rods' names made once (asked every frame)
	static const TArray<FName> Rods = []
	{
		TArray<FName> R;
		for (int32 T = 0; T < LyudF::NumTackle(); T++) R.Add(FName(ANSI_TO_TCHAR(LyudF::Tackle(T).Item)));
		return R;
	}();
	return !Item.IsNone() && Rods.Contains(Item);
}

FName ULyudFishing::RodToUse() const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!Inv)
	{
		return NAME_None;
	}
	if (State && IsRod(State->Rod) && Inv->Count(State->Rod) > 0)
	{
		return State->Rod;
	}
	for (int32 T = LyudF::NumTackle() - 1; T >= 0; T--)
	{
		const FName Item(ANSI_TO_TCHAR(LyudF::Tackle(T).Item));
		if (Inv->Count(Item) > 0) return Item;
	}
	return NAME_None;
}

FText ULyudFishing::Forecast(const UObject* WorldContext)
{
	static const TCHAR* Why[] = { TEXT("plain"), TEXT("twilight"), TEXT("falling"), TEXT("rising"), TEXT("bright"), TEXT("gale"), TEXT("rain"), TEXT("storm"),
		TEXT("night"), TEXT("murky") };
	const LyudF::EMoodWhy W = LyudF::MoodWhy(FishConditions(WorldContext));
	return LyudText::Get(*FString::Printf(TEXT("ui.fish.forecast.%s"), Why[FMath::Clamp(int32(W), 0, int32(UE_ARRAY_COUNT(Why)) - 1)]));
}

bool ULyudFishing::HasHand(const UObject* WorldContext)
{
	const ULyudFishing* F = Get(WorldContext);
	return F && F->IsOut();
}

bool ULyudFishing::IsLineOut() const
{
	return Session && Session->State() != LyudF::EState::Idle && Session->State() != LyudF::EState::Charging;
}

void ULyudFishing::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Session = MakePimpl<LyudF::FSession>(uint32(FPlatformTime::Cycles() | 1u));
	// every level start is a new game unless a load restores the state right after (as the inventory)
	if (ULyudFishingState* State = ULyudFishingState::Get(&InWorld))
	{
		State->Reset();
	}
}

void ULyudFishing::Deinitialize()
{
	for (ALyudFishingRod* R : { Rod.Get(), Parked.Get() })
	{
		if (R) R->Destroy();
	}
	Rod.Reset();
	Parked.Reset();
	Super::Deinitialize();
}

FTransform ULyudFishing::LookFrame() const
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

void ULyudFishing::Sound(FName Pool, const FVector& Where, float Volume)
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, Where, Volume);
	}
}

void ULyudFishing::Say(const FText& Text, float Seconds)
{
	Said = Text;
	SayLeft = Seconds;
	SayCount++;
}

void ULyudFishing::SetBait(FName Item)
{
	ULyudFishingState* State = ULyudFishingState::Get(this);
	// the rig is changed only with the line wound in (not with a float out or a fish on)
	if (!State || State->Bait == Item || (Session && Session->State() != LyudF::EState::Idle))
	{
		return;
	}
	// the old bait comes off the hook back into its box
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (Inv && State->bBaited && !State->Bait.IsNone())
	{
		const int32 Old = FishBaitOf(State->Bait);
		if (Old >= 0 && !LyudFishing::Bait(Old).bLure)
		{
			Inv->Add(State->Bait);
		}
	}
	State->Bait = Item;
	State->bBaited = false;
	if (Session)
	{
		// the hook is bare now: SyncBait puts the new bait on from the pocket
		Session->SetBait(Item.IsNone() ? -1 : FishBaitOf(Item));
		Session->Unbait();
	}
	UE_LOG(LogTemp, Display, TEXT("FISHING bait %s"), *Item.ToString());
}

void ULyudFishing::CycleBait()
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!Inv || !State)
	{
		return;
	}
	TArray<FName> Held;
	for (int32 B = 0; B < LyudF::NumBaits(); B++)
	{
		const FName Id(ANSI_TO_TCHAR(LyudFishing::Bait(B).Item));
		if (Inv->Count(Id) > 0 || (State->Bait == Id && State->bBaited))
		{
			Held.Add(Id);
		}
	}
	if (Held.Num() == 0)
	{
		return;
	}
	const int32 At = Held.IndexOfByKey(State->Bait);
	SetBait(Held[(At + 1) % Held.Num()]);
}

void ULyudFishing::SyncBait()
{
	// the hook baited from the pocket when it is bare and there is bait (a spoon is tied on once and stays)
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!Inv || !State || !Session)
	{
		return;
	}
	const int32 B = State->Bait.IsNone() ? -1 : FishBaitOf(State->Bait);
	if (Session->GetBait() != B)
	{
		Session->SetBait(B);
	}
	if (B < 0)
	{
		return;
	}
	if (LyudFishing::Bait(B).bLure)
	{
		State->bBaited = Inv->Count(State->Bait) > 0;
		if (State->bBaited && !Session->IsBaited()) Session->Rebait();
		if (!State->bBaited && Session->IsBaited()) Session->Unbait();
		return;
	}
	if (!Session->IsBaited() && State->bBaited)
	{
		Session->Rebait();
	}
	if (!Session->IsBaited() && Inv->Count(State->Bait) > 0)
	{
		Inv->Remove(State->Bait);
		Session->Rebait();
		State->bBaited = true;
	}
}

void ULyudFishing::UpdateWeights()
{
	const ULyudEcosystem* Eco = ULyudEcosystem::Get(this);
	std::vector<float> W(LyudF::NumCatchable(), 0.f);
	if (Eco)
	{
		for (const TPair<FName, float>& It : Eco->GetBiteWeights(EndPos))
		{
			const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*It.Key.ToString()));
			const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*It.Key.ToString()));
			if (C >= 0 && S >= 0)
			{
				W[C] = It.Value * SchoolFactor(S);
			}
		}
	}
	Session->SetBiteWeights(W);
	// how keen they are now: the light, the weather, and how much food the web leaves them (that slowly: every half minute)
	HungerIn -= 3.f;
	if (HungerOf.Num() != LyudF::NumCatchable() || HungerIn <= 0.f)
	{
		FishHunger(Eco, HungerOf);
		HungerIn = 30.f;
	}
	Session->SetConditions(FishConditions(this), std::vector<float>(HungerOf.GetData(), HungerOf.GetData() + HungerOf.Num()));
}

bool ULyudFishing::IsWater(const FVector& Where) const
{
	const ALyudSea* Sea = ALyudSea::Get(this);
	return Sea && Sea->DepthAt(Where) > 25.f;
}

FVector ULyudFishing::OnWater(const FVector2D& XY, float Below) const
{
	const ALyudSea* Sea = ALyudSea::Get(this);
	const FVector P(XY.X, XY.Y, 0.f);
	const float Z = Sea ? Sea->SurfaceZ(P) : Origin.Z;
	return FVector(XY.X, XY.Y, Z - Below);
}

void ULyudFishing::TakeOut()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Look = LookFrame();
	// the rod put away last time comes back out; only the first take-out in a world builds one
	ALyudFishingRod* R = Parked.Get();
	Parked.Reset();
	if (R)
	{
		R->SetActorLocationAndRotation(Look.TransformPosition(FVector(30.f, 20.f, -60.f)), Look.Rotator());
	}
	else
	{
		R = World->SpawnActor<ALyudFishingRod>(Look.TransformPosition(FVector(30.f, 20.f, -60.f)), Look.Rotator(), Params);
	}
	if (!R)
	{
		return;
	}
	R->SetActorHiddenInGame(true);
	Rod = R;
	// which rod: its reach, strength and reel, its look
	if (const ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		RodItem = IsRod(Inv->EquippedItem()) ? Inv->EquippedItem() : RodId;
	}
	const int32 Tackle = FMath::Max(0, LyudF::FindTackle(TCHAR_TO_ANSI(*RodItem.ToString())));
	Session->SetTackle(Tackle);
	R->SetTier(Tackle);
	if (ULyudFishingState* State = ULyudFishingState::Get(this))
	{
		State->Rod = RodItem;
	}
	bAttached = false;
	bHidden = true;
	HoldTime = 0.f;
	ShowLeft = -1.f;
	Session->Reset();
	if (ULyudFishingState* State = ULyudFishingState::Get(this))
	{
		Session->SetBait(State->Bait.IsNone() ? -1 : FishBaitOf(State->Bait));
		if (State->bBaited) Session->Rebait();
	}
	PosePos = FVector(30.f, 22.f, -50.f);
	PoseElev = 20.f;
	PoseYaw = -4.f;
	UE_LOG(LogTemp, Display, TEXT("FISHING rod out %s"), *RodItem.ToString());
}

void ULyudFishing::PutAway()
{
	// a fish still shown in the hand: the showing ends (it is in the pocket already)
	if (ShowLeft > 0.f)
	{
		Bag();
	}
	if (Session)
	{
		Session->Reset();
	}
	if (ALyudFishingRod* R = Rod.Get())
	{
		R->Park();
		Parked = R;
	}
	Rod.Reset();
	bAttached = false;
	bFlying = false;
	PromptKey = ~0ull;
	ShowLeft = -1.f;
	// the hand that held it lets go (the hero himself while a bird is flown: he is not the player's pawn then)
	AHeroCharacter* Holder = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Holder)
	{
		Holder = HeldBy.Get();
	}
	if (UHeroReachComponent* Reach = Holder ? UHeroReachComponent::Get(Holder) : nullptr)
	{
		Reach->ClearOverrideHold();
	}
	HeldBy.Reset();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	LastPrompt = FText::GetEmpty();
	UE_LOG(LogTemp, Display, TEXT("FISHING rod away"));
}

void ULyudFishing::LaunchBoat()
{
	const LyudFishPlaces::FBoatDef& D = LyudFishPlaces::Get().Boat;
	const ALyudSea* Sea = ALyudSea::Get(this);
	const ULyudFishingState* Saved = ULyudFishingState::Get(this);
	if (Sea && D.bValid && Saved && Saved->bBoatSaved && !Saved->BoatMooring.IsZero() && !ALyudRowBoat::Get(this))
	{
		// a loaded game: the boat where it was at once (the hero may be sitting in it), its mooring as found then
		bBoatTried = true;
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		P.bDeferConstruction = true;
		const FVector At(FVector2D(Saved->BoatPos), Sea->SurfaceZ(Saved->BoatPos));
		if (ALyudRowBoat* Boat = GetWorld()->SpawnActor<ALyudRowBoat>(At, FRotator(0.f, Saved->BoatYaw, 0.f), P))
		{
			Boat->SetMooring(FVector2D(Saved->BoatMooring), FVector2D(Saved->BoatLanding));
			Boat->FinishSpawning(FTransform(FRotator(0.f, Saved->BoatYaw, 0.f), At));
			Boat->Restore(*Saved);
		}
		return;
	}
	// the sea's depth map first (until it is read every point says open sea)
	if (!Sea || !Sea->IsDepthReady())
	{
		return;
	}
	bBoatTried = true;
	if (!D.bValid || ALyudRowBoat::Get(this))
	{
		return;
	}
	// tied up by the pier where it floats even at spring low water: out along the pier from the planned point until it does
	const float Low = Sea->LowWaterZ();
	FVector At(D.Mooring, 0.f);
	for (int32 Step = 0; Step < 100 && Sea->DepthAtLevel(At, Low) < D.MinDepth + 20.f; Step++)
	{
		At.X += 200.f;
	}
	if (Sea->DepthAtLevel(At, Low) < D.MinDepth)
	{
		UE_LOG(LogTemp, Display, TEXT("FISHING no water for the boat at low tide near %s"), *At.ToCompactString());
		return;
	}
	At.Z = Sea->SurfaceZ(At);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ALyudRowBoat* Boat = GetWorld()->SpawnActor<ALyudRowBoat>(At, FRotator(0.f, D.Yaw, 0.f), Params);
	if (Boat)
	{
		// the landing moves with the mooring (both along the pier)
		Boat->SetMooring(FVector2D(At), D.Landing + (FVector2D(At) - D.Mooring));
		Boat->FinishSpawning(FTransform(FRotator(0.f, D.Yaw, 0.f), At));
		// a save loaded before the sea was ready: where it was left
		if (const ULyudFishingState* State = ULyudFishingState::Get(this))
		{
			Boat->Restore(*State);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FISHING boat moored at %s (%s)"), *At.ToCompactString(), Boat ? TEXT("ok") : TEXT("failed"));
}

void ULyudFishing::Kitchens(float DeltaTime, APawn* Hero, APlayerController* PC)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	const TArray<LyudFishPlaces::FKitchen>& All = LyudFishPlaces::Get().Kitchens;
	if (!UI || !State || All.Num() == 0)
	{
		return;
	}
	// which kitchen door the hero stands at (looked for four times a second)
	KitchenIn -= DeltaTime;
	if (KitchenIn <= 0.f)
	{
		KitchenIn = 0.25f;
		KitchenNear = -1;
		for (int32 K = 0; K < All.Num(); K++)
		{
			if (FVector::Dist2D(Hero->GetActorLocation(), All[K].At) < All[K].Radius && FMath::Abs(Hero->GetActorLocation().Z - All[K].At.Z) < 400.f)
			{
				KitchenNear = K;
			}
		}
	}
	if (KitchenNear < 0)
	{
		if (KitchenKey != 0)
		{
			UI->SetPrompt(State, FText::GetEmpty());
			KitchenKey = 0;
		}
		return;
	}
	const LyudFishPlaces::FKitchen& K = All[KitchenNear];
	const int32 Hour = UGameCalendar::NowIn(GetWorld()).GetHour();
	const bool bOpen = Hour >= K.Open && Hour < K.Close;
	int32 Roubles = 0;
	const int32 N = bOpen ? State->KitchenOffer(this, K.Id, K.PerDay, &Roubles) : 0;
	const int32 Held = State->FishHeld(this);
	if (bOpen && N > 0 && PC->WasInputKeyJustPressed(EKeys::Y) && !PC->IsMoveInputIgnored())
	{
		const int32 Got = State->SellToKitchen(this, K.Id, K.PerDay);
		UI->SetPrompt(State, LyudText::Get(TEXT("ui.fish.kitchen_sold"), { FStringFormatArg(N), FStringFormatArg(Got) }));
		KitchenKey = 1;
		return;
	}
	// rebuilt only when what it says changes
	const uint64 Key = 2 | (uint64(bOpen) << 2) | (uint64(N) << 3) | (uint64(FMath::Min(Held, 255)) << 11) | (uint64(Roubles) << 20);
	if (Key == KitchenKey || KitchenKey == 1)
	{
		// the "sold" line stays until the hero steps away and back
		return;
	}
	KitchenKey = Key;
	FText Text;
	if (!bOpen) Text = LyudText::Get(TEXT("ui.fish.kitchen_closed"), { FStringFormatArg(K.Open), FStringFormatArg(K.Close) });
	else if (N > 0) Text = LyudText::Get(TEXT("ui.fish.kitchen_offer"), { FStringFormatArg(N), FStringFormatArg(Roubles) });
	else Text = LyudText::Get(Held > 0 ? TEXT("ui.fish.kitchen_none") : TEXT("ui.fish.kitchen_bring"));
	UI->SetPrompt(State, Text);
}

FString ULyudFishing::DebugState() const
{
	if (!Session)
	{
		return TEXT("no session");
	}
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	const LyudF::FFish& F = Session->Fish();
	return FString::Printf(TEXT("out %d state %s t %.1f dist %.1f tension %.2f dip %.2f stamina %.2f bait %s baited %d fish %s %.0f g bites/min %.2f total %d"),
		IsOut() ? 1 : 0, FishStateName(Session->State()), Session->Time(), Session->Distance(), Session->Tension(), Session->FloatDip(), Session->Stamina(),
		State ? *State->Bait.ToString() : TEXT("-"), Session->IsBaited() ? 1 : 0, F.Species >= 0 ? ANSI_TO_TCHAR(LyudF::Catchable(F.Species).Id) : TEXT("-"), F.Grams,
		Session->BitesPerMinute(), State ? State->Total : 0);
}

void ULyudFishing::ForceBite()
{
	if (!Session) return;
	UpdateWeights();
	// the likeliest fish here
	int32 Best = LyudF::FindCatchable("RoundGoby");
	float BestW = -1.f;
	const ULyudEcosystem* Eco = ULyudEcosystem::Get(this);
	if (Eco)
	{
		for (const TPair<FName, float>& It : Eco->GetBiteWeights(EndPos))
		{
			const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*It.Key.ToString()));
			if (C >= 0 && It.Value * LyudF::BaitLiking(C, Session->GetBait()) > BestW)
			{
				BestW = It.Value * LyudF::BaitLiking(C, Session->GetBait());
				Best = C;
			}
		}
	}
	// only the events it adds (the last step's are already handled)
	const int32 Before = int32(Session->Events().size());
	Session->DebugBite(Best);
	HandleEvents(Before);
}

void ULyudFishing::ForceHook(FName Species)
{
	ForcedHook = Species;
}

FHeroHoldPose ULyudFishing::PoseNow(float DeltaTime)
{
	// targets per state; the hand follows them quickly (the swing exactly)
	FVector& Pos = PosePos;
	float& Elev = PoseElev;
	float& Yaw = PoseYaw;
	FVector TPos(40.f, 20.f, -28.f);
	float TElev = 32.f, TYaw = -4.f, Rate = 9.f;
	const LyudF::EState S = Session->State();
	const float T = Session->Time();
	switch (S)
	{
	case LyudF::EState::Idle:
		if (ShowLeft > 0.f)
		{
			// the fish lifted to the eyes on the line
			TPos = FVector(30.f, 16.f, -12.f);
			TElev = 52.f;
			TYaw = -12.f;
		}
		break;
	case LyudF::EState::Charging:
	{
		// the rod goes back over the shoulder as the swing builds
		const float C = FishEase(Session->Charge());
		TPos = FMath::Lerp(FVector(38.f, 20.f, -26.f), FVector(12.f, 26.f, 2.f), C);
		TElev = FMath::Lerp(40.f, 105.f, C);
		Rate = 14.f;
		break;
	}
	case LyudF::EState::Swing:
	{
		// and comes forward hard, stopping low toward the water
		const float W = FMath::Clamp(T / Session->Tune.SwingTime, 0.f, 1.f);
		const float E = W * W * (3.f - 2.f * W);
		TPos = FMath::Lerp(FVector(12.f, 26.f, 2.f), FVector(54.f, 18.f, -22.f), E);
		TElev = FMath::Lerp(105.f, 12.f, E);
		Rate = 40.f;
		break;
	}
	case LyudF::EState::Flying:
		TPos = FVector(52.f, 18.f, -22.f);
		TElev = FMath::Lerp(12.f, 24.f, FishEase(T / 0.8f));
		break;
	case LyudF::EState::Waiting:
	case LyudF::EState::Nibbling:
	case LyudF::EState::Taking:
		TPos = FVector(42.f, 20.f, -28.f);
		TElev = 24.f;
		break;
	case LyudF::EState::Fighting:
	{
		// up and back against the fish, the tip following it left and right; it shakes on a run
		const float Te = Session->Tension();
		TPos = FVector(36.f - 8.f * Te, 22.f, -18.f + 6.f * Te);
		TElev = 50.f + 18.f * Te;
		TYaw = -4.f + 14.f * Session->Side();
		if (Session->IsRunning())
		{
			const float Now = GetWorld()->GetTimeSeconds();
			TPos += FVector(0.f, 0.6f * FMath::Sin(Now * 31.f), 0.8f * FMath::Sin(Now * 23.f)) * Te;
		}
		Rate = 12.f;
		break;
	}
	case LyudF::EState::Landing:
		TPos = FVector(26.f, 18.f, -4.f);
		TElev = 72.f;
		Rate = 8.f;
		break;
	case LyudF::EState::Winding:
		TPos = FVector(40.f, 20.f, -26.f);
		TElev = 34.f;
		break;
	}
	// the strike: a jerk up and back
	TElev += 30.f * Strike;
	TPos += FVector(-8.f, 0.f, 10.f) * Strike;
	const float A = 1.f - FMath::Exp(-Rate * DeltaTime);
	Pos = FMath::Lerp(Pos, TPos, A);
	Elev = FMath::Lerp(Elev, TElev, A);
	Yaw = FMath::Lerp(Yaw, TYaw, A);
	return RodPose(Pos, Elev, Yaw);
}

void ULyudFishing::HandleEvents(int32 From)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	ALyudFishingRod* R = Rod.Get();
	const std::vector<LyudF::EEvent>& Events = Session->Events();
	for (int32 I = FMath::Max(From, 0); I < int32(Events.size()); I++)
	{
		const LyudF::EEvent E = Events[I];
		switch (E)
		{
		case LyudF::EEvent::CastStart:
			if (R) Sound(TEXT("rod_swish"), R->Tip(), 0.8f);
			break;
		case LyudF::EEvent::Cast:
		{
			// the float flies from the tip along the look, landing Throw metres out (a little further when cast upward)
			const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
			const FRotator Look = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
			Dir = FRotator(0.f, Look.Yaw, 0.f).Vector();
			const float Up = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch), -10.f, 35.f);
			const float Throw = Session->CastDistance() * (1.f + Up / 90.f) * 100.f;
			FlyFrom = R ? R->Tip() : LookFrame().GetLocation();
			const ACharacter* Hero = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
			Origin = Hero ? Hero->GetActorLocation() : FlyFrom;
			FeetZ = Hero ? Origin.Z - Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : Origin.Z;
			bDryCast = false;
			const FVector2D To = FVector2D(Origin) + FVector2D(Dir) * Throw;
			FlyTo = OnWater(To, 0.f);
			if (!IsWater(FlyTo))
			{
				// no sea under it: down to the ground there (the trace in Fly finds it)
				FlyTo.Z = FlyFrom.Z - 400.f;
			}
			FlyDur = 0.45f + Throw / 2400.f;
			FlyTime = 0.f;
			FlyPos = FlyFrom;
			bFlying = true;
			UE_LOG(LogTemp, Display, TEXT("FISHING cast %.1f m"), Throw * 0.01f);
			break;
		}
		case LyudF::EEvent::Landed:
			Sound(TEXT("float_plop"), EndPos, 0.7f);
			UpdateWeights();
			UE_LOG(LogTemp, Display, TEXT("FISHING landed %.1f m at %s bites/min %.2f"), Session->Distance(), *EndPos.ToCompactString(), Session->BitesPerMinute());
			break;
		case LyudF::EEvent::Ashore:
			Say(LyudText::Get(TEXT("ui.fish.ashore")), 2.5f);
			UE_LOG(LogTemp, Display, TEXT("FISHING ashore"));
			break;
		case LyudF::EEvent::Nibble:
			UE_LOG(LogTemp, Display, TEXT("FISHING nibble"));
			if (R && Session->BitingFish() >= 0)
			{
				// its look loads in the background while it nibbles, so the strike shows it without a hitch
				R->PrefetchFish(FishSpeciesName(Session->BitingFish()));
			}
			break;
		case LyudF::EEvent::Take:
			if (R && Session->BitingFish() >= 0)
			{
				R->PrefetchFish(FishSpeciesName(Session->BitingFish()));
			}
			Sound(TEXT("float_plop"), EndPos, 0.35f);
			UE_LOG(LogTemp, Display, TEXT("FISHING take"));
			break;
		case LyudF::EEvent::Hooked:
		{
			Strike = 1.f;
			const LyudF::FFish& F = Session->Fish();
			const FName Species = FishSpeciesName(F.Species);
			if (R) R->SetFish(Species, F.Length);
			// where it took: the web's cell and the diver's fish it is (taken out only if it is landed)
			HookPos = EndPos;
			Sound(TEXT("fish_splash"), EndPos, 0.6f);
			UE_LOG(LogTemp, Display, TEXT("FISHING hooked %s %.0f g %.1f cm"), *Species.ToString(), F.Grams, F.Length * 100.f);
			break;
		}
		case LyudF::EEvent::Missed:
			Strike = 1.f;
			UE_LOG(LogTemp, Display, TEXT("FISHING missed"));
			break;
		case LyudF::EEvent::Gone:
			Say(LyudText::Get(TEXT("ui.fish.gone")), 2.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING gone"));
			break;
		case LyudF::EEvent::BaitStolen:
			if (State) State->bBaited = false;
			Say(LyudText::Get(TEXT("ui.fish.bait_stolen")), 2.5f);
			UE_LOG(LogTemp, Display, TEXT("FISHING bait stolen"));
			break;
		case LyudF::EEvent::Run:
			if (R && Session->State() == LyudF::EState::Fighting) Sound(TEXT("reel_drag"), R->GetActorLocation(), 0.5f);
			break;
		case LyudF::EEvent::Snapped:
		{
			if (R) Sound(TEXT("line_snap"), R->Tip(), 0.9f);
			// a spoon goes with the line
			if (State && Inv && !State->Bait.IsNone())
			{
				const int32 B = FishBaitOf(State->Bait);
				if (B >= 0 && LyudFishing::Bait(B).bLure) Inv->Remove(State->Bait);
				State->bBaited = false;
			}
			if (R) R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
			Say(LyudText::Get(TEXT("ui.fish.snapped")), 3.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING snapped"));
			break;
		}
		case LyudF::EEvent::Escaped:
			if (State) State->bBaited = Session->IsBaited();
			if (R) R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
			Say(LyudText::Get(TEXT("ui.fish.escaped")), 3.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING escaped"));
			break;
		case LyudF::EEvent::Landing:
			Sound(TEXT("fish_splash"), EndPos, 0.8f);
			break;
		case LyudF::EEvent::Caught:
			Caught();
			break;
		case LyudF::EEvent::Retrieved:
			UE_LOG(LogTemp, Display, TEXT("FISHING retrieved"));
			break;
		}
	}
}

void ULyudFishing::Caught()
{
	const LyudF::FFish& F = Session->Fish();
	if (F.Species < 0)
	{
		return;
	}
	const FName Species = FishSpeciesName(F.Species);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	if (State)
	{
		State->bBaited = Session->IsBaited();
	}
	ShowSpecies = Species;
	ShowGrams = F.Grams;
	ShowCm = F.Length * 100.f;
	ShowLeft = 2.4f;
	// into the pocket now, with the log (shown in the hand for a moment after, only for the look)
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(FishItem(F.Species));
		// a fish landed on the rod (quests: "fish_caught <fish item>"; "get" is also sent for a fish that is bought or given)
		ULyudQuests::Notify(this, TEXT("fish_caught"), FishItem(F.Species).ToString());
	}
	if (State)
	{
		State->Pocket(Species, F.Grams);
	}
	// out of the web and its spot where it was hooked (EndPos is at the rod's tip by now), and the one the diver would see there
	if (ULyudEcosystem* Eco = ULyudEcosystem::Get(this))
	{
		Eco->ReportCatch(Species, HookPos, F.Grams);
	}
	if (ULyudUnderwaterSubsystem* Under = ULyudUnderwaterSubsystem::Get(this))
	{
		Under->TakeFish(Species, HookPos, 1500.f);
	}
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const bool bNew = State && State->Record(Species, F.Grams, ShowCm, Now);
	if (State)
	{
		// the journal's line: where (the boat, a spot of the town's shore, or the open shore) and with what
		FLyudCatchEntry E;
		E.When = Now;
		E.Species = Species;
		E.Grams = F.Grams;
		E.Cm = ShowCm;
		E.Spot = ALyudRowBoat::IsHeroAboard(this) ? FName(TEXT("boat")) : LyudFishPlaces::SpotAt(Origin);
		E.Bait = State->Bait;
		E.Rod = RodItem;
		State->Note(E);
	}
	const bool bFirst = State && State->Find(Species) && State->Find(Species)->Count == 1;
	const TArray<FStringFormatArg> Args = { FStringFormatArg(FishName(Species).ToString()), FStringFormatArg(FMath::RoundToInt(F.Grams)), FStringFormatArg(FMath::RoundToInt(ShowCm)) };
	Say(LyudText::Get(bFirst ? TEXT("ui.fish.caught_first") : (bNew ? TEXT("ui.fish.caught_record") : TEXT("ui.fish.caught")), Args), 4.f);
	Sound(TEXT("fish_flap"), EndPos, 0.8f);
	UE_LOG(LogTemp, Display, TEXT("FISHING caught %s %.0f g %.1f cm first %d record %d"), *Species.ToString(), F.Grams, ShowCm, bFirst ? 1 : 0, bNew ? 1 : 0);
}

void ULyudFishing::Bag()
{
	// the end of the showing: into the bag (it is already in the pocket since the catch: a save meanwhile keeps it)
	ShowLeft = -1.f;
	if (ShowSpecies.IsNone())
	{
		return;
	}
	if (const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0)) Sound(TEXT("fish_bag"), Hero->GetActorLocation(), 0.6f);
	if (ALyudFishingRod* R = Rod.Get()) R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
	ShowSpecies = NAME_None;
}

void ULyudFishing::Fly(float DeltaTime)
{
	ALyudFishingRod* R = Rod.Get();
	if (!bFlying || !R)
	{
		return;
	}
	FlyTime += DeltaTime;
	const float S = FMath::Clamp(FlyTime / FlyDur, 0.f, 1.f);
	const float D = FVector::Dist2D(FlyFrom, FlyTo);
	FVector P = FMath::Lerp(FlyFrom, FlyTo, S);
	P.Z += D * 0.22f * 4.f * S * (1.f - S);
	// it hits whatever is in the way (a pier, rocks, the beach)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FishingCast), false, R);
	Q.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	bool bDone = false, bWater = false;
	FVector At = P;
	if (GetWorld()->LineTraceSingleByChannel(Hit, FlyPos, P, ECC_Visibility, Q))
	{
		At = Hit.ImpactPoint;
		bDone = true;
		// the shallow sea's bed under the surface is still the water
		const FVector Surf = OnWater(FVector2D(At), 0.f);
		bWater = IsWater(At) && At.Z <= Surf.Z + 5.f;
		if (bWater) At = Surf;
	}
	else if (S >= 1.f)
	{
		bDone = true;
		bWater = IsWater(P);
		At = bWater ? OnWater(FVector2D(P), 0.f) : P;
	}
	else if (IsWater(P) && P.Z <= OnWater(FVector2D(P), 0.f).Z && S > 0.5f)
	{
		bDone = true;
		bWater = true;
		At = OnWater(FVector2D(P), 0.f);
	}
	FlyPos = bDone ? At : P;
	EndPos = FlyPos;
	if (bDone)
	{
		bFlying = false;
		Session->SetLanded(bWater, FVector::Dist2D(Origin, At) * 0.01f);
		// on land (the beach, the pier) it is wound back over the ground, not at the sea's height under it
		bDryCast = !bWater;
		DryZ = At.Z;
		if (bWater)
		{
			// the line lies along the way it went
			Dir = FVector(FVector2D(At - Origin).GetSafeNormal(), 0.f);
			if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
		}
		LastDist = Session->Distance();
	}
}

void ULyudFishing::Prompt()
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	// built only when what it says changes (a new state, bait, words said, the tension by 5 %), not every frame
	const LyudF::EState St = Session->State();
	const bool bRunHint = St == LyudF::EState::Fighting && Session->IsRunning() && Session->Tension() > 0.55f;
	const uint64 Key = uint64(St) | (uint64(SayLeft > 0.f) << 8) | (uint64(Session->GetBait() + 1) << 9) | (uint64(Session->IsBaited()) << 17)
		| (uint64(bRunHint) << 18) | (uint64(St == LyudF::EState::Fighting && !bRunHint ? FMath::RoundToInt(Session->Tension() * 20.f) : 0) << 19)
		| (uint64(SayCount) << 32);
	if (Key == PromptKey)
	{
		return;
	}
	PromptKey = Key;
	FText Text;
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	const FText BaitName = State && !State->Bait.IsNone() && Session->IsBaited() ? ULyudInventory::ItemName(State->Bait) : LyudText::Get(TEXT("ui.fish.no_bait"));
	const bool bLure = Session->GetBait() >= 0 && LyudFishing::Bait(Session->GetBait()).bLure;
	if (SayLeft > 0.f)
	{
		Text = Said;
	}
	else
	{
		switch (Session->State())
		{
		case LyudF::EState::Idle: Text = LyudText::Get(TEXT("ui.fish.hint_idle"), { FStringFormatArg(BaitName.ToString()) }); break;
		case LyudF::EState::Charging: Text = LyudText::Get(TEXT("ui.fish.hint_charge")); break;
		case LyudF::EState::Waiting:
		case LyudF::EState::Nibbling:
			Text = LyudText::Get(bLure ? TEXT("ui.fish.hint_lure") : (Session->Gear().bBottom ? TEXT("ui.fish.hint_wait_tip") : TEXT("ui.fish.hint_wait")));
			break;
		case LyudF::EState::Taking: Text = LyudText::Get(TEXT("ui.fish.hint_take")); break;
		case LyudF::EState::Fighting:
			Text = Session->IsRunning() && Session->Tension() > 0.55f ? LyudText::Get(TEXT("ui.fish.hint_run"))
				: LyudText::Get(TEXT("ui.fish.hint_fight"), { FStringFormatArg(FMath::RoundToInt(Session->Tension() * 20.f) * 5) });
			break;
		default: break;
		}
	}
	if (!Text.EqualTo(LastPrompt))
	{
		UI->SetPrompt(this, Text);
		LastPrompt = Text;
	}
}

void ULyudFishing::Draw(float DeltaTime)
{
	ALyudFishingRod* R = Rod.Get();
	if (!R)
	{
		return;
	}
	const LyudF::EState S = Session->State();
	const FVector Tip = R->Tip();
	if (S == LyudF::EState::Winding && !bWasWinding)
	{
		// how far out and how high it was when the winding began (the dry drag eases from there to the feet)
		WindFrom = Session->Distance();
		WindFromZ = bDryCast ? DryZ : EndPos.Z;
	}
	bWasWinding = S == LyudF::EState::Winding;
	const bool bLure = Session->GetBait() >= 0 && LyudFishing::Bait(Session->GetBait()).bLure;
	const ALyudSea* Sea = ALyudSea::Get(this);
	// the float or fish lies along the cast's way, the fish off to its side as it swims
	const FVector2D Base(Origin);
	auto Along = [&](float Side) -> FVector2D
	{
		const FVector D = FRotator(0.f, Side * 25.f, 0.f).RotateVector(Dir);
		return Base + FVector2D(D) * Session->Distance() * 100.f;
	};
	bool bFloat = false, bSpoon = false, bFish = false;
	FRotator FishRot = FRotator::ZeroRotator;
	float Wriggle = 0.f, Lean = 0.f;
	const float Now = GetWorld()->GetTimeSeconds();
	switch (S)
	{
	case LyudF::EState::Idle:
	case LyudF::EState::Charging:
	case LyudF::EState::Swing:
		if (ShowLeft > 0.f)
		{
			// the catch hangs from the tip, head up, flapping
			EndPos = Tip - FVector(0.f, 0.f, 75.f);
			bFish = true;
			Wriggle = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Now * 3.f));
			FishRot = FRotator(88.f, LookFrame().Rotator().Yaw + 90.f + 25.f * FMath::Sin(Now * 2.1f), 0.f);
		}
		else
		{
			// the float (or the spoon) dangles a little under the tip
			EndPos = Tip - FVector(0.f, 0.f, 35.f);
			bFloat = !bLure;
			bSpoon = bLure;
			Lean = 0.f;
		}
		break;
	case LyudF::EState::Flying:
		EndPos = FlyPos;
		bFloat = !bLure;
		bSpoon = bLure;
		Lean = 0.6f;
		break;
	case LyudF::EState::Waiting:
	case LyudF::EState::Nibbling:
	case LyudF::EState::Taking:
	case LyudF::EState::Winding:
	{
		const FVector2D XY = Along(0.f);
		if (bLure)
		{
			// the spoon a little under, rising as it is wound fast
			EndPos = OnWater(XY, 60.f);
			bSpoon = S != LyudF::EState::Winding || Session->Distance() > 0.5f;
		}
		else
		{
			// riding the waves, ducking at a nibble and under on a take
			EndPos = OnWater(XY, Session->FloatDip() * 7.f - 0.4f * FMath::Sin(Now * 2.3f));
			bFloat = true;
			Lean = S == LyudF::EState::Winding ? 0.8f : 0.f;
		}
		if (S == LyudF::EState::Winding && (bDryCast || !IsWater(EndPos)))
		{
			// dragged back over dry ground (the beach, the pier): on whatever is under it, found by a trace down; without a hit,
			// from where the winding began down to the feet (never under the ground at the sea's height)
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(FishingWind), false, R);
			Q.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
			const FVector Top(EndPos.X, EndPos.Y, FMath::Max3<double>(Tip.Z, DryZ, FeetZ) + 150.f);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Top, FVector(EndPos.X, EndPos.Y, EndPos.Z - 300.f), ECC_Visibility, Q))
			{
				EndPos.Z = FMath::Max(EndPos.Z, Hit.ImpactPoint.Z + 2.f);
			}
			else
			{
				const float T = WindFrom > 0.01f ? FMath::Clamp(Session->Distance() / WindFrom, 0.f, 1.f) : 0.f;
				EndPos.Z = FMath::Max(EndPos.Z, FMath::Lerp(FeetZ + 3.f, bDryCast ? DryZ + 2.f : WindFromZ, T));
			}
			bSpoon = bLure;
		}
		break;
	}
	case LyudF::EState::Fighting:
	{
		const FVector2D XY = Along(Session->Side());
		EndPos = OnWater(XY, 25.f + 45.f * Session->Stamina());
		bFish = true;
		Wriggle = Session->IsRunning() ? 1.f : 0.45f;
		// heading away from the rod and toward its side
		const FVector Away = FVector(XY - FVector2D(Tip), 0.f).GetSafeNormal();
		FishRot = FRotator(0.f, Away.Rotation().Yaw + 35.f * Session->Side(), 0.f);
		break;
	}
	case LyudF::EState::Landing:
	{
		// out of the water up to the tip
		const float W = FishEase(Session->Time() / 0.9f);
		const FVector From = OnWater(Along(Session->Side()), 10.f);
		EndPos = FMath::Lerp(From, Tip - FVector(0.f, 0.f, 75.f), W);
		bFish = true;
		Wriggle = 1.f;
		FishRot = FRotator(88.f * W, LookFrame().Rotator().Yaw + 90.f, 0.f);
		break;
	}
	}
	// the sea rod's bottom rig: a lead instead of a float, out of sight once it is down; the bite shows on the rod's tip
	const bool bBottom = !bLure && Session->Gear().bBottom;
	bool bSinker = false;
	const bool bOnWater = S == LyudF::EState::Waiting || S == LyudF::EState::Nibbling || S == LyudF::EState::Taking;
	if (bBottom && bFloat)
	{
		bFloat = false;
		bSinker = !bOnWater && !(S == LyudF::EState::Winding && IsWater(EndPos));
		if (bOnWater) EndPos = OnWater(FVector2D(EndPos), 0.f);
	}
	const float WaterZ = Sea ? Sea->SurfaceZ(EndPos) : -1e6f;
	// the rod bends toward the line with its tension (and, on the bottom rig, knocks and pulls round with a bite)
	const float Bend = 0.04f + 1.2f * Session->Tension() + (bFish && S != LyudF::EState::Fighting ? 0.18f : 0.f)
		+ (bBottom && bOnWater ? 0.1f + 0.35f * Session->FloatDip() : 0.f);
	R->SetBend(Bend, EndPos - Tip);
	{
		const float Slack = bOnWater && !bLure ? (bBottom ? 0.08f : 0.35f) : FMath::Clamp(1.f - Session->Tension() * 2.5f, 0.f, 1.f) * 0.5f;
		R->SetLine(EndPos + (bFloat ? FVector(0.f, 0.f, 7.5f) : FVector::ZeroVector), Slack, WaterZ);
	}
	R->SetFloat(bFloat, EndPos, Lean);
	R->SetSinker(bSinker, EndPos);
	R->SetSpoon(bSpoon, EndPos, Tip - EndPos, Now * 18.f);
	R->PlaceFish(bFish, EndPos, FishRot, Wriggle, DeltaTime);
	// the reel: wound in, or the clutch giving line
	const float DistNow = Session->Distance();
	const float Moved = DistNow - LastDist;
	LastDist = DistNow;
	R->TurnReel(-Moved / 0.6f);
	// the ratchet ticks while it is wound
	ReelClickIn -= DeltaTime;
	if (Moved < -0.005f && ReelClickIn <= 0.f)
	{
		Sound(TEXT("reel_click"), R->GetActorLocation(), 0.35f);
		ReelClickIn = 0.45f;
	}
}

void ULyudFishing::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	if (!Hero || !PC || !Inv || !Session)
	{
		// the hero is not the player's pawn (a bird is flown): the line is wound in and the rod put away (parked, hidden,
		// off his hand; the line, the float and a fish on the hook with it), not left hanging in the air where he stands.
		// It comes back out by itself when he is the pawn again with the rod still in the hand
		if (Session && Rod.IsValid())
		{
			PutAway();
		}
		return;
	}
	// a bait taken in the hand goes on the rig (the rod comes back into the hand); a rod in the hand comes out
	if (!bBoatTried)
	{
		LaunchBoat();
	}
	Kitchens(DeltaTime, Hero, PC);
	const FName Equipped = Inv->EquippedItem();
	if (!Equipped.IsNone() && FishBaitOf(Equipped) >= 0)
	{
		const FName UseRod = Rod.IsValid() ? RodItem : RodToUse();
		if (!UseRod.IsNone())
		{
			SetBait(Equipped);
			Inv->Equip(UseRod);
		}
		else
		{
			Inv->Equip(FName());
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World)) UI->SetPrompt(this, LyudText::Get(TEXT("ui.fish.need_rod")));
			SayLeft = 0.f;
			NeedRodLeft = 2.5f;
		}
	}
	if (NeedRodLeft > 0.f)
	{
		NeedRodLeft -= DeltaTime;
		if (NeedRodLeft <= 0.f && !Rod.IsValid())
		{
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World)) UI->SetPrompt(this, FText::GetEmpty());
		}
	}
	const FName Hand = Inv->EquippedItem();
	if (IsRod(Hand) && !Rod.IsValid())
	{
		TakeOut();
	}
	else if (IsRod(Hand) && Hand != RodItem)
	{
		// another rod taken: this one away (wound in), that one out
		PutAway();
		TakeOut();
	}
	else if (!IsRod(Hand) && Rod.IsValid())
	{
		PutAway();
	}
	ALyudFishingRod* R = Rod.Get();
	if (!R)
	{
		return;
	}
	HeldBy = Hero;
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	// the diary takes the hand (it holds the book itself); a reach move, a screen, a bird's eyes: the rod is hidden and waits
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bScreen = Screen && Screen->IsOpen();
	const ULyudSwimComponent* Swim = Hero->FindComponentByClass<ULyudSwimComponent>();
	const bool bSwimming = Swim && Swim->IsSwimming();
	const bool bBusy = bScreen || Reach->IsBusy() || (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || Hero->IsHidden() || bSwimming;
	if (bBusy)
	{
		// a swing held when the screen came up is dropped (it would cast by itself after); in the water everything is wound in
		if (Session->State() == LyudF::EState::Charging || (bSwimming && Session->State() != LyudF::EState::Idle))
		{
			Session->Reset();
			bFlying = false;
		}
		if (!bHidden)
		{
			R->SetActorHiddenInGame(true);
			bHidden = true;
		}
		if (!bScreen && !Reach->IsBusy())
		{
			Reach->ClearOverrideHold();
		}
		HoldTime = 0.f;
		bWaitUp = true;
		return;
	}
	SayLeft = FMath::Max(0.f, SayLeft - DeltaTime);
	if (ShowLeft > 0.f)
	{
		ShowLeft -= DeltaTime;
		if (ShowLeft <= 0.f)
		{
			Bag();
		}
	}

	// the buttons (a press only after the rod is up, and only once they were let go after a screen)
	bool bDown = ShowLeft <= 0.f && HoldTime > 0.35f && CVarFishing.GetValueOnGameThread() != 0 && !PC->IsMoveInputIgnored()
		&& (bForced || bClickDown || PC->IsInputKeyDown(EKeys::LeftMouseButton));
	if (bClickQueued)
	{
		bClickQueued = false;
		bClickDown = true;
	}
	else
	{
		bClickDown = false;
	}
	if (bWaitUp)
	{
		bWaitUp = bDown;
		bDown = false;
	}
	LyudF::FInput In;
	In.bButton = bDown;
	In.bReelIn = !PC->IsMoveInputIgnored() && PC->IsInputKeyDown(EKeys::RightMouseButton);
	// Q: wound in first, then away
	if (PC->WasInputKeyJustPressed(EKeys::Q) && !PC->IsMoveInputIgnored())
	{
		if (!IsLineOut() && ShowLeft <= 0.f)
		{
			Inv->Equip(FName());
			PutAway();
			return;
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::X) && !PC->IsMoveInputIgnored() && Session->State() == LyudF::EState::Idle)
	{
		CycleBait();
	}
	if (Session->State() == LyudF::EState::Idle)
	{
		SyncBait();
	}
	if (!ForcedHook.IsNone())
	{
		const int32 C = FishNames().Species.IndexOfByKey(ForcedHook);
		ForcedHook = FName();
		if (C != INDEX_NONE)
		{
			if (Session->State() == LyudF::EState::Idle)
			{
				Origin = Hero->GetActorLocation();
				Dir = FVector(FVector2D(LookFrame().GetRotation().GetForwardVector()).GetSafeNormal(), 0.f);
			}
			const int32 Before = int32(Session->Events().size());
			Session->DebugHook(C);
			HandleEvents(Before);
		}
	}
	// the weights where the float lies, now and then
	WeightsIn -= DeltaTime;
	if (WeightsIn <= 0.f && (Session->State() == LyudF::EState::Waiting || Session->State() == LyudF::EState::Nibbling))
	{
		UpdateWeights();
		WeightsIn = 3.f;
	}
	Session->Step(DeltaTime, In);
	Strike = FMath::Max(0.f, Strike - DeltaTime * 5.f);
	HandleEvents();
	// the flight's end adds Landed / Ashore to this step's events: handled now, not lost to the next step
	const int32 Handled = int32(Session->Events().size());
	Fly(DeltaTime);
	HandleEvents(Handled);
	// walked off too far from the line (or the fish): it is wound in
	if (IsLineOut() && !bFlying && FVector::Dist2D(Hero->GetActorLocation(), Origin) > 3000.f)
	{
		Session->Reset();
	}

	// the hand: exactly in the look's frame each frame; the rod rides on the hand bone once it is up
	Reach->SetOverrideHold(PoseNow(DeltaTime));
	HoldTime += DeltaTime;
	if (!bAttached && Reach->HoldWeight() > 0.6f)
	{
		FTransform Palm;
		if (FishBonePalm(Hero->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), Palm))
		{
			R->SetActorTransform(RodInPalm() * Palm);
			R->AttachToComponent(Hero->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
			bAttached = true;
		}
	}
	if (bAttached && bHidden)
	{
		R->SetActorHiddenInGame(false);
		bHidden = false;
	}
	if (bAttached)
	{
		Draw(DeltaTime);
	}
	Prompt();
}

// ------------------------------------------------------------------ console

static FAutoConsoleCommandWithWorldAndArgs GFishGive(TEXT("lyud.Fish.Give"),
	TEXT("lyud.Fish.Give [fishing_rod|rod_spinning|rod_sea]: that rod (the float rod), every bait and lure in the pocket, the rod in the hand"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			const FName Rod = Args.Num() > 0 && ULyudFishing::IsRod(FName(*Args[0])) ? FName(*Args[0]) : ULyudFishing::RodId;
			if (Inv->Count(Rod) < 1) Inv->Add(Rod);
			for (int32 B = 0; B < LyudF::NumBaits(); B++)
			{
				const FName Item(ANSI_TO_TCHAR(LyudF::Bait(B).Item));
				Inv->Add(Item, LyudF::Bait(B).bLure ? 1 : 10);
			}
			Inv->Equip(Rod);
		}
	}));

static FAutoConsoleCommandWithWorld GFishState(TEXT("lyud.Fish.State"), TEXT("lyud.Fish.State: the rod's state in the log (FISHING state ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudFishing* F = ULyudFishing::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("FISHING state %s"), *F->DebugState());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishPress(TEXT("lyud.Fish.Press"), TEXT("lyud.Fish.Press 1|0: holds the left mouse button down for the rod, or lets it go"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->Press(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}
	}));

static FAutoConsoleCommandWithWorld GFishClick(TEXT("lyud.Fish.Click"), TEXT("lyud.Fish.Click: one click of the left mouse button for the rod (a strike)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->Click();
		}
	}));

static FAutoConsoleCommandWithWorld GFishBite(TEXT("lyud.Fish.Bite"), TEXT("lyud.Fish.Bite: the likeliest fish here takes the float now"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->ForceBite();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishHook(TEXT("lyud.Fish.Hook"), TEXT("lyud.Fish.Hook <Species>: that fish on the line now (Cod, Perch, Salmon ...)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->ForceHook(FName(Args.Num() ? *Args[0] : TEXT("Perch")));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishBait(TEXT("lyud.Fish.Bait"), TEXT("lyud.Fish.Bait <item>: fish with that bait (bait_worm, bait_maggot, lure_spoon)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->SetBait(FName(Args.Num() ? *Args[0] : TEXT("bait_worm")));
		}
	}));

static FAutoConsoleCommandWithWorld GFishLog(TEXT("lyud.Fish.Log"), TEXT("lyud.Fish.Log: the angler's log (FISHING log ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudFishingState* S = ULyudFishingState::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("FISHING log total %d"), S->Total);
			for (const FLyudCatchRecord& R : S->Log)
			{
				UE_LOG(LogTemp, Display, TEXT("FISHING log %s x%d biggest %.0f g %.1f cm first %s"), *R.Species.ToString(), R.Count, R.BiggestGrams, R.BiggestCm, *R.First.ToString());
			}
		}
	}));

static FAutoConsoleCommandWithWorld GFishJournal(TEXT("lyud.Fish.Journal"), TEXT("lyud.Fish.Journal: the angler's journal, the last catches (FISHING journal ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudFishingState* S = ULyudFishingState::Get(World))
		{
			for (const FLyudCatchEntry& E : S->Journal)
			{
				UE_LOG(LogTemp, Display, TEXT("FISHING journal %s %s %.0f g %.1f cm at %s bait %s rod %s"), *E.When.ToString(), *E.Species.ToString(), E.Grams, E.Cm,
					E.Spot.IsNone() ? TEXT("shore") : *E.Spot.ToString(), *E.Bait.ToString(), *E.Rod.ToString());
			}
			UE_LOG(LogTemp, Display, TEXT("FISHING journal %d lines, kitchens bought %d for %d roubles"), S->Journal.Num(), S->KitchenSold, S->KitchenRoubles);
		}
	}));

static FAutoConsoleCommandWithWorld GFishMood(TEXT("lyud.Fish.Mood"), TEXT("lyud.Fish.Mood: the light and weather the fish feel now, each one's keenness, the forecast (FISHING mood ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const LyudF::FConditions C = FishConditions(World);
		UE_LOG(LogTemp, Display, TEXT("FISHING mood sun %.1f deg wind %.1f m/s rain %.1f mm/h thunder %.2f cloud %.2f pressure %+.1f hPa/3h murk %.2f: %s"),
			C.SunAltitude, C.Wind, C.Rain, C.Thunder, C.Cloud, C.PressureTendency, C.Murk, *ULyudFishing::Forecast(World).ToString());
		TArray<float> Hunger;
		FishHunger(ULyudEcosystem::Get(World), Hunger);
		for (int32 F = 0; F < LyudF::NumCatchable(); F++)
		{
			UE_LOG(LogTemp, Display, TEXT("FISHING mood %-20s bait %.2f lure %.2f hunger %.2f"), ANSI_TO_TCHAR(LyudF::Catchable(F).Id),
				LyudF::BiteMood(C, F, false), LyudF::BiteMood(C, F, true), Hunger[F]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishKitchen(TEXT("lyud.Fish.Kitchen"),
	TEXT("lyud.Fish.Kitchen [sell]: what the first kitchen (the restaurant) would buy from the pocket now, or sells it (wherever the hero is)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudFishingState* S = ULyudFishingState::Get(World);
		const TArray<LyudFishPlaces::FKitchen>& All = LyudFishPlaces::Get().Kitchens;
		if (!S || All.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("FISHING kitchen: none"));
			return;
		}
		int32 Roubles = 0;
		const int32 N = S->KitchenOffer(World, All[0].Id, All[0].PerDay, &Roubles);
		UE_LOG(LogTemp, Display, TEXT("FISHING kitchen %s at %s would take %d fish for %d roubles"), *All[0].Id.ToString(), *All[0].At.ToCompactString(), N, Roubles);
		if (Args.Num() > 0 && Args[0] == TEXT("sell"))
		{
			S->SellToKitchen(World, All[0].Id, All[0].PerDay);
		}
	}));
