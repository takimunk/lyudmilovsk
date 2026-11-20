#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudGarden.generated.h"

class AActor;
class ANpcBody;
class AProceduralPlant;
class APlayerController;
class SLyudCityPage;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
namespace LyudGarden { class FModel; }
namespace LyudWeather { class FChain; class FSeason; }

/**
 * The allotment behind the commune house (docs/allotment.md, backlog item 9). The hero rents a bed from the house committee
 * (the notice board at the gate), digs it with a spade, sows seeds and plants seedlings from the hardware shop and the
 * market, waters from the can (filled at the rain barrel), weeds, pulls the film down on cold nights; the crops grow by the
 * hour from the game's weather (the rules: the engine-free LyudGarden::FModel, Private/Garden/GardenModel.*, run over whole
 * seasons by tools/gardensim) and are drawn by the procedural plant system (one AProceduralPlant a cell, re-shaped only when
 * their growth has moved). Picked crops go to the pocket: eaten, given (friends' gifts), handed in at the market's control
 * scales for roubles, or given to the neighbour Маарья (on the next bed on weekday mornings, with advice by season and by
 * the state of his bed) for a jar of jam the market's jury tastes on Sunday. Friends hand him seed packets now and then.
 * Nothing is shown as a number: the prompt line and the toasts say what the bed needs.
 *
 * Data: Content/Garden/garden.json and Content/Localization/garden (tools/garden/build_garden.py). Saved as "Garden".
 * E acts (polled, never consumed); while it has a prompt the city things and the friends' gift step aside.
 * Console: lyud.Garden.Report, lyud.Garden.Grow <hours> (the bed jumps ahead), lyud.Garden.Kit (rent, a spade, a can, seeds).
 * Developer mode: "garden".
 */
UCLASS()
class LYUDMILOVSK_API ULyudGarden : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudGarden();
	virtual ~ULyudGarden() override;

	static ULyudGarden* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudGarden, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** Something of the garden is on the prompt line now (or its chooser is up): others who use E step aside. */
	bool IsPrompting() const { return Prompt != EPrompt::None || Page.IsValid(); }

	/** The quality (1 .. 3) of his last picking of a crop's item ("strawberries"); 2 when he never picked it. */
	int32 HarvestQuality(const FString& Item) const;
	/** He cooked a jar of jam himself (ULyudKitchen): the market's contest tastes this one like the neighbour's. */
	void OwnJam(int32 Quality);

	/** A friend greeted him (ULyudFriends): now and then they hand him a seed packet if he keeps a bed. */
	void OnFriendGreeted(int32 PersonId, int32 Level);

	// --- console and developer mode
	FString Report() const;
	/** The bed jumps Hours ahead in the weather of now (developer mode: see it grow). */
	void DevGrow(float Hours);
	/** Rents the bed and gives a spade, a full can and a packet of each seed. */
	void DevKit();
	/** Where to stand to look at his bed. */
	bool BedStand(FTransform& Out) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/** The model's state as a flat map ("Garden"). */
	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

private:
	enum class EPrompt : uint8 { None, Cell, Board, Barrel, Neighbour, Scales };

	void Load();
	double NowHours() const;
	int32 Today() const;
	void StepModel();
	void Soon(float& Coldest, float& Hottest) const;

	// the place
	void BuildSite();
	void ClearSite();
	UStaticMeshComponent* AddBox(const FVector& At, const FVector& HalfCm, float Yaw, const FLinearColor& Colour, bool bCollide = true);
	UStaticMeshComponent* AddCylinder(const FVector& At, float RadiusCm, float HalfHeightCm, const FLinearColor& Colour);
	UMaterialInstanceDynamic* Mat(const FLinearColor& Colour);
	void ShowCells();
	void ShowPlants(bool bNear);
	AProceduralPlant* SpawnPlant(const FString& Species, const FVector& At, int32 Seed, float Growth);
	void ShowPlant(int32 Slot, const FString& Species, const FVector& At, float Growth, bool bDead);

	// the hero
	void TickPrompt(APlayerController* PC);
	int32 CellInView(const FVector& Eye, const FVector& Dir) const;
	void SetPrompt(EPrompt Kind, const FText& Text);
	void Act(APlayerController* PC);
	void ActCell(int32 Cell);
	void OpenSow(int32 Cell);
	void TickSow(APlayerController* PC);
	void CloseSow();
	void Toast(const FText& Text) const;
	void Note(const FString& Key, const TArray<FString>& Args = TArray<FString>()) const;

	// the neighbour, the jam, the market
	void TickNeighbour(float DeltaTime);
	bool NeighbourHere() const { return NeighbourBody != nullptr; }
	void NeighbourSay(const FString& Key);
	void Advise();
	void TickJam();
	void TickContest();
	int32 SellValue(int32* Things) const;

	TPimplPtr<LyudGarden::FModel> Model;
	LyudWeather::FChain* Chain = nullptr;
	LyudWeather::FSeason* Season = nullptr;
	bool bLoaded = false;
	bool bSavedPending = false;
	double StepClock = 0.0;
	float NearClock = 0.f;
	float PromptClock = 0.f;
	float AdviceClock = 0.f;
	bool bNear = false;

	// the data the model doesn't keep (garden.json's places, Unreal cm)
	struct FBedPlace
	{
		FString Id;
		FString Keeper;
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f;
		TArray<FVector> Cells;
		TArray<FString> Crops;
	};
	TArray<FBedPlace> Beds;
	int32 HeroBed = -1;
	FVector SiteAt = FVector::ZeroVector;
	float SiteYaw = 0.f;
	FVector2D SiteSize = FVector2D(14.0, 26.0);
	FVector2D BedSize = FVector2D(1.2, 3.6);
	FVector2D CellSize = FVector2D(0.6, 1.2);
	TMap<FString, FVector> Props;
	TMap<FString, float> PropYaw;
	FVector MarketAt = FVector::ZeroVector;
	FIntPoint MarketHours = FIntPoint(7, 18);
	FString NeighbourKey;
	FVector NeighbourStand = FVector::ZeroVector;
	float NeighbourYaw = 0.f;
	FVector2D NeighbourHours = FVector2D(7.0, 10.5);
	TSet<int32> NeighbourDays;
	TMap<FString, TArray<int32>> SellPrices;
	int32 JamNeed = 6;
	float JamHours = 20.f;
	int32 ContestWeekday = 6;
	int32 ContestHour = 16;
	TArray<int32> Prizes;

	// what the garden keeps besides the model (saved with it, "g.*")
	int32 ContestDay = -1;      // the Sunday his jar is judged (-1 none)
	int32 ContestQuality = 0;
	int32 JamJars = 0;          // jars waiting on the bench for him
	TSet<FString> Harvested;    // crops picked at least once (the diary's first harvest)
	TMap<FString, int32> SeedsGiven;    // friend key -> the day they last gave him seeds

	// drawn
	UPROPERTY(Transient) TObjectPtr<AActor> Site;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CellSoil;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Film;
	UPROPERTY(Transient) TArray<TObjectPtr<AProceduralPlant>> Plants;
	UPROPERTY(Transient) TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY(Transient) TObjectPtr<ANpcBody> NeighbourBody;
	UPROPERTY(Transient) TObjectPtr<AActor> JarActor;
	/** What each plant slot shows (re-shaped only when it moves this much). */
	TArray<float> PlantGrowth;
	TArray<FString> PlantSpecies;
	TArray<uint8> CellLook;
	int32 NeighbourId = -1;

	// the prompt and the chooser
	EPrompt Prompt = EPrompt::None;
	int32 PromptCell = -1;
	FText PromptText;
	TSharedPtr<SLyudCityPage> Page;
	int32 SowCell = -1;
	TArray<FName> SowItems;
	uint64 PageFrame = 0;
	TWeakObjectPtr<APlayerController> MoveBlockedBy;
	FString LastAdvice;
};
