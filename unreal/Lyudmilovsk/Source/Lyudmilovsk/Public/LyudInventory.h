#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudInventory.generated.h"

class SWidget;
class SLyudInventory;
class SLyudPrompt;
class UTexture2D;
struct FSlateBrush;

/** What kind of thing an item is, which decides what the inventory offers to do with it. */
UENUM()
enum class ELyudItemKind : uint8
{
	Misc,
	Drink,
	Food,
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

	/** The item table; null for an unknown id. */
	static const FLyudItemDef* FindDef(FName Id);
	static FText ItemName(FName Id);
	static FText ItemDescription(FName Id);
	static const TArray<FLyudItemDef>& AllDefs();

	void Add(FName Id, int32 Count = 1);
	bool Remove(FName Id, int32 Count = 1);
	int32 Count(FName Id) const;
	bool Spend(int32 Amount);
	void Earn(int32 Amount);

	/** A new game: the pocket as it starts. */
	void Reset();

	/** Items held, in table order, with counts. */
	TArray<TPair<FName, int32>> Held() const;

	DECLARE_MULTICAST_DELEGATE(FChanged);
	FChanged OnChanged;
};

/**
 * The inventory screen (Tab or I) and the little prompt line for things you can use ("[E] Купить газировку"). The screen
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

	bool IsOpen() const { return bOpen; }
	void Open();
	void Close();

	/** Uses one of the item (drinks it, eats it): the screen closes and the hero's hand does it. */
	void Use(FName Id);

	/** One line near the bottom of the screen for Key's interactable; an empty line takes it away. */
	void SetPrompt(const UObject* Key, const FText& Text);

	/** The icon of an item (a neutral square when it has none). */
	const FSlateBrush* IconBrush(FName Id);

	/** The item as the hero's hand holds it, spawned at Pos; null for items with no look. */
	AActor* SpawnHeld(FName Id, const FVector& Pos);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void Lock(bool bLock);
	bool CanOpen() const;

	TSharedPtr<SLyudInventory> Screen;
	TSharedPtr<SLyudPrompt> Prompt;
	TWeakObjectPtr<const UObject> PromptKey;
	bool bOpen = false;
	bool bLocked = false;
	bool bUsing = false;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> Icons;
	TMap<FName, TSharedPtr<FSlateBrush>> Brushes;
	TSharedPtr<FSlateBrush> BlankBrush;
};
