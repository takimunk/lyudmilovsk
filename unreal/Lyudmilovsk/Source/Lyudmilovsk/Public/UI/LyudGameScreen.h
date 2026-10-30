#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UI/LyudQuests.h"
#include "LyudGameScreen.generated.h"

class ALyudDiaryBook;
class SLyudDiaryScreen;
class SLyudHud;
class USpotLightComponent;
class UTexture2D;
class UTextureRenderTarget2D;

UENUM()
enum class ELyudScreenTab : uint8
{
	Inventory,
	Journal,
	Map,
	/** The diary's entries (its newest spread). */
	Diary,
	/** The birds met and befriended (B). */
	Birds,
};

/**
 * The game screen and the HUD (docs/ui.md, docs/diary.md).
 *
 * The game screen is the hero's diary, an A4 planner: things (what he wears and carries, Tab or I), to do (the quests, J), the
 * diary's entries, the map (M). A key takes the diary out of his bag: the right hand brings it up shut, it falls open in front
 * of the eyes, and the pages are the screen (laid exactly over the 3D book's paper); another section's key turns the pages to
 * it, the open section's key or Esc shuts it and puts it away. A and D (or the arrows) turn a page; the tabs on the fore-edge
 * and the dog-eared corners can be clicked. A bird has no hands: the diary just shows. Like the inventory before it, it does
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
	/** Where the diary is: taken out shut, falling open, the pages fading in over the book, read, and back. */
	enum class EPhase : uint8 { Closed, TakeOut, Opening, FadeIn, Reading, FadeOut, Closing, PutAway };

	static ULyudGameScreen* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudGameScreen, STATGROUP_Tickables); }

	/** Takes the diary out on a section (or turns to it if it is out). */
	void Open(ELyudScreenTab Tab);
	/** Shuts the diary and puts it away. */
	void Close();
	/** Shuts it and runs Then once it is away (a drink taken from the pocket needs the hand). */
	void CloseThen(TFunction<void()> Then);
	/** From the moment a key takes it out until it is put away. */
	bool IsOpen() const { return Phase != EPhase::Closed; }
	/** Open and readable (not being taken out or put away). */
	bool IsReading() const { return Phase == EPhase::Reading; }
	ELyudScreenTab CurrentTab() const { return Tab; }
	/** Opens the map centred on a place (the journal's "show on the map"). */
	void ShowOnMap(FName Place);
	/** Opens the journal on the diary. */
	void ShowDiary();
	/** Turns the open diary N spreads on (back if negative). */
	void TurnBy(int32 N);

	/** A short note on the HUD for a few seconds. */
	void Toast(const FText& Text, bool bQuest = false);

	/** The world map picture (/Game/UI/T_WorldMap) and other UI textures; null when missing. */
	UTexture2D* MapTexture();
	UTexture2D* BinocularsMask();

	/** The pictures of the diary's pages: 0 and 1 for a turning page, 2 on the 3D book. */
	UTextureRenderTarget2D* PictureTarget(int32 Index);
	/** The diary screen shows another spread (the 3D book's tabs and picture follow). */
	void OnSpreadShown();
	/** A diary sound (diary_take, diary_open, diary_close, diary_page, diary_pages) at the book. */
	void PlayDiarySound(FName Pool);
	ALyudDiaryBook* GetBook() const { return Book; }

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
	void UpdateDiary(float DeltaTime);
	void SetPhase(EPhase InPhase);
	void PlaceBook(float DeltaTime);
	void Finish();
	bool KeysOnDiary(APlayerController* PC);

	TSharedPtr<SLyudDiaryScreen> Screen;
	TSharedPtr<SLyudHud> Hud;
	ELyudScreenTab Tab = ELyudScreenTab::Inventory;
	EPhase Phase = EPhase::Closed;
	float PhaseTime = 0.f;
	/** The hero's hands bring the book (false: a bird, or the hero hidden in a seat: the pages just show). */
	bool bWithBook = false;
	float PitchBefore = 0.f;
	float PitchRead = -14.f;
	TArray<TFunction<void()>> AfterClose;

	float PlaceTimer = 0.f;
	bool bWasBird = false;
	bool bLampOn = true;
	float Binoculars = 0.f;
	float NormalFov = 0.f;
	/** Seconds after the level starts in which item notes stay quiet (the starting pocket, a load). */
	float QuietFor = 2.f;
	/** The new game's first quest was named on the HUD (or a load made that moot). */
	bool bAnnounced = false;
	/** Where the player stood when the level began; "walk" is reported once he is 2.5 m from it. */
	FVector WalkFrom = FVector::ZeroVector;
	bool bWalkFromSet = false;
	bool bWalked = false;

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Lamp;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTex;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MaskTex;
	UPROPERTY(Transient)
	TObjectPtr<ALyudDiaryBook> Book;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Pictures;
};
