#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudLook.generated.h"

class AHeroCharacter;
class APlayerController;
class SLyudCityPage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
namespace LyudLook { struct FState; }

/**
 * The hero's look (docs/look.md, backlog item 20). The barber's chair (act haircut, "open": "barber") asks how to cut:
 * a crop, short back and sides, a side parting; a cut grows out in two to four weeks. The department store's rail
 * ("open": "clothes") sells a windbreaker, a raincoat, a winter coat and a cap, and its fitting room ("open": "fitting")
 * changes what he has on. The outer layer is the body's own overshirt in another cloth: its material slot gets the piece's
 * colour and roughness (the arms and the chest he sees in first person change with it). The haircuts and the cap are
 * meshes on the shadow's head (the visible body has none), so his shadow changes too. For three days after a new haircut
 * the residents who greet him say something about it, each once; and once a day somebody remarks on clothes that do not
 * suit the weather (no raincoat in the rain, a shirt in the frost, a winter coat in the warmth).
 *
 * The rules are LyudLook (Private/Look/LookModel, plain C++, tools/looksim). Stamps "looks/<piece>", quest events
 * "do haircut", "do ps_clothes", "do ps_fitting_room" as before the pages, diary notes for the first of each. Nothing is
 * placed in the level; the meshes are optional (missing: the rest works).
 *
 * Console: lyud.Look.Info, lyud.Look.Cut <cut>, lyud.Look.Wear <item>, lyud.Look.Open <barber|clothes|fitting>.
 */
UCLASS()
class LYUDMILOVSK_API ULyudLook : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudLook();
	virtual ~ULyudLook() override;

	static ULyudLook* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudLook, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bBegun; }

	/** One of its pages is up (others who use the digits step aside). */
	bool HasPageUp() const { return Page.IsValid(); }

	/** A resident has just greeted the hero (ULyudTownTalkSubsystem): they may add a word about the haircut or the clothes. */
	void OnGreeted(int32 PersonId);

	/** The barber cuts it (no money asked here: the chair's act took it); false for an unknown cut. */
	bool GiveCut(const FString& Cut);
	/** The cut he wears today ("shaggy" once it has grown out). */
	FString CutNow() const;

	// --- console and developer mode
	FString Report() const;
	void DevOpen(const FString& Screen);
	/** The next cut of the barber's list. */
	void DevNextCut();
	/** All the store's clothes into the pockets. */
	void DevClothes();
	/** Puts on the next outer piece he holds. */
	void DevNextOuter();
	/** Holds and puts on an item of the clothes table. */
	bool DevWear(const FString& Item);

	/** LyudLook::Save of the state (the cut, who has spoken of it). */
	UPROPERTY(SaveGame) FString Saved;
	UPROPERTY(SaveGame) bool bDiaryCut = false;
	UPROPERTY(SaveGame) bool bDiaryClothes = false;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	enum class EPage : uint8 { None, Barber, Clothes, Fitting };

	void OnCityOpen(FName Screen, int32 Thing);
	void OpenPage(EPage Which);
	void ClosePage(bool bChosen);
	void TickPage(APlayerController* PC);
	/** The barber's chair: the cut, the quest's event, the scissors. */
	void AtChair(const FString& Cut);
	void Buy(int32 Index);
	void PutOn(int32 Index);
	/** Reads Saved into the model when it has changed under us (a load). */
	void SyncState();
	void Keep();
	/** The outer cloth, the hair and the cap as the pockets and the calendar say now. */
	void SyncLook(AHeroCharacter* Hero);
	void ApplyOuter(AHeroCharacter* Hero, FName Item);
	void ApplyHead(AHeroCharacter* Hero, const FString& Cut, bool bCap);
	void Stamp(const FString& Piece) const;
	void Toast(const FText& Text) const;
	double Day() const;
	/** 0 fine, else LyudLook::EFit of what he has on in this weather, out of doors. */
	uint8 FitNow(const AHeroCharacter* Hero) const;

	bool bBegun = false;
	TPimplPtr<LyudLook::FState> State;
	FString Parsed;
	float SyncIn = 0.f;

	// what is on the hero now
	TWeakObjectPtr<AHeroCharacter> LookOf;
	FName OuterOn;
	FString CutOn;
	bool bCapOn = false;
	bool bGrownTold = true;
	int32 OuterSlot = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OwnCloth;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Cloth;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Hair;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Cap;
	/** The meshes looked for once each (null kept: not there). */
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UStaticMesh>> Meshes;

	// the page
	TSharedPtr<SLyudCityPage> Page;
	EPage PageKind = EPage::None;
	TArray<FName> PageItems;
	uint64 PageFrame = 0;
	TWeakObjectPtr<APlayerController> MoveBlockedBy;
	FDelegateHandle OpenHandle;

	// a word a resident is about to add after the greeting
	int32 SayPerson = -1;
	FString SayKey;
	float SayIn = 0.f;
	bool bSayRemark = false;
};
