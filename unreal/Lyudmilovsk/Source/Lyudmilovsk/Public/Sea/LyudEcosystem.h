#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/PimplPtr.h"
#include "LyudEcosystem.generated.h"

namespace LyudEco { class FModel; }

/**
 * The coastal sea's food web (docs/ecosystem.md; the model is LyudEco::FModel in Private/Sea/EcoModel.cpp, plain C++):
 * 49 groups from the diatoms, the blue-greens and the weed through zooplankton, mysids, amphipods, shrimp, the isopod Saduria,
 * clams and mussels to the fish, grey seals, fish-eating birds and the winter's sea ducks; who eats whom and how much
 * of each lives here, stepped by the game's calendar (the spring bloom, the summer's warmth, the migrants coming and
 * going) and saved with the game. The fish round the diver (ULyudUnderwaterSubsystem) come in the numbers it gives and
 * hunt what it says they eat; what the player catches comes out of it, the spot it came from thinner for a while.
 *
 * For fishing: GetBiteWeights (what is likely on the hook here, now), ReportCatch (a fish taken out).
 * Console: lyud.Eco.Report, lyud.Eco.Days <n> (live n days at once), lyud.Eco.Reset, lyud.Eco.Scale <Id> <x>.
 */
UCLASS()
class LYUDMILOVSK_API ULyudEcosystem : public UGameInstanceSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudEcosystem();
	virtual ~ULyudEcosystem() override;

	static ULyudEcosystem* Get(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "Ecosystem", meta = (WorldContext = "WorldContext"))
	static ULyudEcosystem* GetEcosystem(const UObject* WorldContext) { return Get(WorldContext); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Brings the web up to the calendar's now (whole game hours; a long sleep or a load catches up, at most three years). */
	void Sync(const UWorld* World);
	void Sync(const FDateTime& Now);

	/** A group's numbers here now against the usual (1 = as usual; 0 when it is away for the season). Ids as the fish's (Herring, Cod...) or the web's (Zoo, Mussel, Eider...). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") float GetAbundance(FName Id) const;
	/** The same at a place: a spot fished out recently has fewer (40 m cells). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") float GetLocalAbundance(FName Id, const FVector& Location) const;
	/** Biomass, g per m2 of the coastal sea. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") float GetBiomass(FName Id) const;
	/** The share of a predator's food a prey is (0: never eats it). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") float GetDietShare(FName Predator, FName Prey) const;
	/**
	 * What would take a bait here now, fish species by weight (not normalised; 0 for those not here): numbers in the web,
	 * the local thinning, how well the spot suits it (depth, sand, stones, weed), how active it is (month, day or night).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") TMap<FName, float> GetBiteWeights(const FVector& Location) const;
	/** A fish taken out at a place (a line, a net): from the web (Grams; < 0: a typical one's weight) and that spot. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") void ReportCatch(FName Species, const FVector& Location, float Grams = -1.f);
	/** The water's plankton now against its mean (the spring bloom, the blue-greens in August make it greener and murkier). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") float GetBloom() const;
	UFUNCTION(BlueprintCallable, Category = "Ecosystem") FString GetReport() const;

	/** For the fish: per LyudFish species index, its numbers now against the usual (cached each sync), and the local thinning. */
	float FishAbundance(int32 FishSpecies, float XMetres, float YMetres) const;
	/** The fish model's diet from the web: Fn(Predator, Prey, Share) for every pair of fish species. */
	void ForEachFishDiet(TFunctionRef<void(int32, int32, float)> Fn) const;

	// --- console
	void LiveDays(float Days);
	void ResetWeb(const UWorld* World);
	bool Scale(FName Id, float Factor);

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

protected:
	UPROPERTY(SaveGame) TArray<FString> SavedKeys;
	UPROPERTY(SaveGame) TArray<float> SavedValues;
	/** The calendar's time the web was last stepped to (zero: never: it starts from the base, warmed through a few years). */
	UPROPERTY(SaveGame) FDateTime SteppedTo;

private:
	void Warm(const FDateTime& Now);
	/** Forget the web: the next sync starts it fresh (a New Game, a save without it). */
	void StartOver();
	void Refresh();
	int32 NodeOf(FName Id) const;

	TPimplPtr<LyudEco::FModel> Model;
	/** The level the web was last synced in (a new one may be a New Game: see Sync). */
	TWeakObjectPtr<const UWorld> SyncedWorld;
	/** Days of the fresh game's warm-up still to live (sliced over the syncs). */
	float WarmLeft = 0.f;
	/** LyudFish species index -> web node (-1 none), and each fish's numbers now (Relative x Here). */
	TArray<int32> FishNode;
	TArray<float> FishNow;
};
