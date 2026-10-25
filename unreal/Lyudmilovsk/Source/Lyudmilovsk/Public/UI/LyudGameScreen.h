#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UI/LyudQuests.h"
#include "LyudGameScreen.generated.h"

class SLyudGameScreenWidget;
class SLyudHud;
class USpotLightComponent;
class UTexture2D;

UENUM()
enum class ELyudScreenTab : uint8
{
	Inventory,
	Journal,
	Map,
};

/**
 * The game screen and the HUD (docs/ui.md).
 *
 * The game screen has three pages: the inventory with what the hero wears (Tab or I), the journal with the quests and the diary (J),
 * the map (M). The key of the open page closes it, another page's key switches to it, Esc closes. Like the inventory before it, it does
 * not pause the game: walking and looking stop and the mouse shows. Keys are polled, so it never eats a key another system wants.
 *
 * The HUD: the date and time, the quest being followed and its next step, and short notes (a quest begun, a place found, an item
 * got). lyud.Hud 0 hides it.
 *
 * It also runs what worn things do: the head lamp lights the way (L switches it), the binoculars bring things close while the right
 * mouse button is held.
 */
UCLASS()
class LYUDMILOVSK_API ULyudGameScreen : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudGameScreen* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudGameScreen, STATGROUP_Tickables); }

	void Open(ELyudScreenTab Tab);
	void Close();
	bool IsOpen() const { return bOpen; }
	ELyudScreenTab CurrentTab() const { return Tab; }
	/** Opens the map centred on a place (the journal's "show on the map"). */
	void ShowOnMap(FName Place);
	/** Opens the journal on the diary. */
	void ShowDiary();

	/** A short note on the HUD for a few seconds. */
	void Toast(const FText& Text, bool bQuest = false);

	/** The world map picture (/Game/UI/T_WorldMap) and other UI textures; null when missing. */
	UTexture2D* MapTexture();
	UTexture2D* BinocularsMask();

	/** Where the player is on the map (metres) and which way he looks (a unit vector in the map frame). */
	bool PlayerOnMap(FVector2D& At, FVector2D& Facing) const;

	bool IsHeadLampOn() const;
	/** 0..1 how far the binoculars are up. */
	float BinocularsAmount() const { return Binoculars; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void OnNews(ELyudQuestNews What, FName Id);
	void OnItemAdded(FName Id, int32 Count);
	void UpdateWear(float DeltaTime);
	void UpdateHud();

	TSharedPtr<SLyudGameScreenWidget> Screen;
	TSharedPtr<SLyudHud> Hud;
	ELyudScreenTab Tab = ELyudScreenTab::Inventory;
	bool bOpen = false;

	float PlaceTimer = 0.f;
	bool bWasBird = false;
	bool bLampOn = true;
	float Binoculars = 0.f;
	float NormalFov = 0.f;
	/** Seconds after the level starts in which item notes stay quiet (the starting pocket, a load). */
	float QuietFor = 2.f;
	/** The new game's first quest was named on the HUD (or a load made that moot). */
	bool bAnnounced = false;

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Lamp;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTex;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MaskTex;
};
