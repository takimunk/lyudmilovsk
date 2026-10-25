#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudInventory.generated.h"

class SWidget;
class SLyudPrompt;
class SLyudShop;
class UTexture2D;
struct FSlateBrush;

/** What kind of thing an item is, which decides what the inventory offers to do with it. */
UENUM()
enum class ELyudItemKind : uint8
{
	Misc,
	Drink,
	Food,
	/** Carried in the hand (Tab: "Взять в руку"): bird feed. One unit is one handful. */
	Feed,
	/** Worn (clothes, the head lamp, binoculars): goes into its Slot. */
	Wear,
};

/** Where a worn thing goes (the inventory's figure, top to bottom). Hand is ULyudInventory::Equipped. */
UENUM()
enum class ELyudEquipSlot : uint8
{
	None,
	Head,
	Neck,
	Outer,
	Body,
	Legs,
	Feet,
	Hand,
};

/** One kind of item (the table is in LyudInventory.cpp): its icon (/Game/Inventory/Localized/<lang>/T_Icon_<Id>) and what it leaves behind. The words are in the string tables (item.<Id>.name / .desc). */
struct FLyudItemDef
{
	FName Id;
	ELyudItemKind Kind = ELyudItemKind::Misc;
	/** What is left in the inventory after it is used (an empty bottle). */
	FName Leaves;
	/** Bottles: the label texture and glass tint; the shape is a capsule (cm). */
	FName Flavor;
	float Radius = 3.f;
	float HalfLength = 0.f;
	/** Worn things and things held in the hand (feed). */
	ELyudEquipSlot Slot = ELyudEquipSlot::None;
};

/** One thing a trader sells: Count units of Item for Price roubles. */
struct FLyudShopGood
{
	FName Item;
	int32 Price = 0;
	int32 Count = 1;
};

/**
 * What the hero carries. The game instance keeps it (it outlives the level) and every save slot stores it
 * ("Inventory"): the items and the roubles. A new game starts with a few roubles in the pocket.
 *
 * Other systems use it through Get(): Add / Remove / Count, and Spend for money. Picking up an apple off the
 * ground: UHeroReachComponent::Take(item, [] { ULyudInventory::Get(this)->Add("apple"); }).
 */
UCLASS()
class LYUDMILOVSK_API ULyudInventory : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudInventory* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(SaveGame)
	TMap<FName, int32> Counts;

	UPROPERTY(SaveGame)
	int32 Rubles = 100;

	/** The item in the hero's hand (none: the hand is free). It goes when the last one is used up. */
	UPROPERTY(SaveGame)
	FName Equipped;

	/** What the hero wears, one thing per slot (the clothes he woke up in at first). */
	UPROPERTY(SaveGame)
	TArray<FName> Worn;

	/** The item table; null for an unknown id. */
	static const FLyudItemDef* FindDef(FName Id);
	static FText ItemName(FName Id);
	static FText ItemDescription(FName Id);
	static const TArray<FLyudItemDef>& AllDefs();

	void Add(FName Id, int32 Count = 1);
	bool Remove(FName Id, int32 Count = 1);
	int32 Count(FName Id) const;
	bool Spend(int32 Amount);
	/** Put the item in the hand (it must be held); NAME_None empties the hand. */
	void Equip(FName Id);
	/** The item in the hand and how many of it are left (none when the hand is free). */
	FName EquippedItem() const { return Count(Equipped) > 0 ? Equipped : FName(); }
	void Earn(int32 Amount);

	/** Puts on a worn thing (taking off whatever was in its slot); false if it is not held or not wearable. */
	bool Wear(FName Id);
	/** Takes off the thing in Slot (Hand empties the hand). */
	void TakeOff(ELyudEquipSlot Slot);
	bool IsWorn(FName Id) const { return Worn.Contains(Id); }
	/** The thing in a slot (none if empty). */
	FName InSlot(ELyudEquipSlot Slot) const;
	static ELyudEquipSlot SlotOf(FName Id);
	static FText SlotName(ELyudEquipSlot Slot);
	static const TArray<ELyudEquipSlot>& Slots();

	/** A new game: the pocket as it starts. */
	void Reset();

	/** Items held, in table order, with counts. */
	TArray<TPair<FName, int32>> Held() const;

	DECLARE_MULTICAST_DELEGATE(FChanged);
	FChanged OnChanged;
	/** Count of Id went into the pocket (the HUD shows it). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FAdded, FName /*Id*/, int32 /*Count*/);
	FAdded OnAdded;
};

/**
 * The inventory's actions (use, the trader's screen) and the little prompt line for things you can use ("[E] Купить газировку"). The screen
 * does not pause the game; it takes away walking and looking and shows the mouse. Reads keys by polling, so it never
 * eats a key another system wants (E, F, F7, F10).
 */
UCLASS()
class LYUDMILOVSK_API ULyudInventoryUI : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudInventoryUI* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudInventoryUI, STATGROUP_Tickables); }

	/** The game screen (inventory, journal, map) or a trader's screen is up (the hero can neither walk nor look). */
	bool IsOpen() const;
	bool IsShopOpen() const { return bShop; }
	/** Opens the game screen on the inventory (ULyudGameScreen). */
	void Open();

	/** Pays for and takes shop good Index (the buy button; headless tests); false if the hero cannot pay. */
	bool BuyShopGood(int32 Index);

	/** A trade screen: Goods to buy with the hero's roubles; Title names the trader. Closed with T, Tab, I or Esc. */
	void OpenShop(const FText& Title, const TArray<FLyudShopGood>& Goods);
	/** Shop.Open close (headless checks). */
	void CloseShopForTest() { CloseShop(); }
	/** Closes the game screen. */
	void Close();

	/** Uses one of the item (drinks it, eats it): the screen closes and the hero's hand does it. */
	void Use(FName Id);

	/** One line near the bottom of the screen for Key's interactable; an empty line takes it away. */
	void SetPrompt(const UObject* Key, const FText& Text);

	/** The icon of an item (a neutral square when it has none). */
	const FSlateBrush* IconBrush(FName Id);

	/** The item as the hero's hand holds it, spawned at Pos; null for items with no look. */
	AActor* SpawnHeld(FName Id, const FVector& Pos);

	/** Walking and looking off, the mouse shown (a screen is up), or back. */
	void Lock(bool bLock);
	/** Nothing else is going on that a screen would get in the way of. bNeedHero: only on foot (not as a bird). */
	bool CanOpen(bool bNeedHero = true) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void CloseShop();

	TSharedPtr<SLyudShop> Shop;
	TArray<FLyudShopGood> ShopGoods;
	bool bShop = false;
	TSharedPtr<SLyudPrompt> Prompt;
	TWeakObjectPtr<const UObject> PromptKey;
	bool bLocked = false;
	uint64 ShopClosedFrame = 0;
	bool bUsing = false;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> Icons;
	TMap<FName, TSharedPtr<FSlateBrush>> Brushes;
	TSharedPtr<FSlateBrush> BlankBrush;
};
