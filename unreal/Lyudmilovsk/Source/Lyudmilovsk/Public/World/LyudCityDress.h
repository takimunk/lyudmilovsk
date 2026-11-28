#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudCityDress.generated.h"

class ALyudCityBuilding;
class UPeopleLifeSubsystem;

/**
 * When a dressing set of a town building is shown (docs/city_dress.md; the export json's "when", blender/city/citykit.py
 * dress_when). Filled by unreal/scripts/import_city.py. Every field at its default holds always.
 */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FLyudDressWhen
{
	GENERATED_BODY()

	/** EWeatherBit words (rain 1, snow 2, fog 4, cold 8, warm 16, clear 32, wet 64, dry 128): any of them now / none of them. */
	UPROPERTY(EditAnywhere, Category = "Dress") int32 WeatherAny = 0;
	UPROPERTY(EditAnywhere, Category = "Dress") int32 WeatherNone = 0;
	/** Bit m-1 for month m (0: any month); bit d for weekday d, Monday = 0 (0: any day). */
	UPROPERTY(EditAnywhere, Category = "Dress") int32 Months = 0;
	UPROPERTY(EditAnywhere, Category = "Dress") int32 Weekdays = 0;
	/** [HourFrom, HourTo) on a 24 h clock; 0..24 is always. */
	UPROPERTY(EditAnywhere, Category = "Dress") float HourFrom = 0.f;
	UPROPERTY(EditAnywhere, Category = "Dress") float HourTo = 24.f;
	/** "" no condition, "any" a holiday today, "none" not a holiday, else a festival id on now. */
	UPROPERTY(EditAnywhere, Category = "Dress") FString Festival;
	/** Shown on this share of days (a stable die of plot, set and day). */
	UPROPERTY(EditAnywhere, Category = "Dress") float Chance = 1.f;
	/** Days since the game began modulo PhasePeriod in [PhaseFrom, PhaseTo) (PhasePeriod 0: no condition). */
	UPROPERTY(EditAnywhere, Category = "Dress") int32 PhasePeriod = 0;
	UPROPERTY(EditAnywhere, Category = "Dress") int32 PhaseFrom = 0;
	UPROPERTY(EditAnywhere, Category = "Dress") int32 PhaseTo = 0;
	/** -1 no condition, 1 in the building's open hours, 0 out of them. */
	UPROPERTY(EditAnywhere, Category = "Dress") int32 Open = -1;
	/** A trace of a visit (citykit visit=, docs/city_dress.md "Traces of a visit"): shown for VisitKeep hours (0: no condition)
	 *  after a townsperson ended one of VisitActs (activities.json ids) at the place VisitAt of the building's plot ("" any
	 *  place of the building, "/0-03" its flat 0-03, "-LOUNGE" the place <plot>-LOUNGE); VisitWho: only these person keys or
	 *  occupations (empty: anybody). */
	UPROPERTY(EditAnywhere, Category = "Dress") TArray<FName> VisitActs;
	UPROPERTY(EditAnywhere, Category = "Dress") FString VisitAt;
	UPROPERTY(EditAnywhere, Category = "Dress") TArray<FString> VisitWho;
	UPROPERTY(EditAnywhere, Category = "Dress") float VisitKeep = 0.f;
};

/**
 * Shows and hides the town buildings' dressing sets (ALyudCityBuilding DressIds / DressMeshes / DressWhen: a cup left after
 * breakfast, wet umbrellas on rain days, coats in winter, a jigsaw that grows by days). Every 2 s it reads the game's moment
 * (UGameCalendar, the weather's summary, the festivals) and, only when the moment's key has changed (the clock's 10-minute
 * step, the day, the weather words, the holidays, the mode), asks every set of every registered building whether it shows.
 * A set's static mesh component is made the first time it is shown (no collision, out of the distance fields), then only
 * shown or hidden: nothing per frame, nothing moved or rebuilt, nothing saved (it all follows from the date and the weather).
 * Sets with a visit condition (a cup after the family's breakfast, a chess game left after the club's evening) listen to
 * the people's life (UPeopleLifeSubsystem::OnPersonEvent Arrive / Leave): a matching visit's end is noted with its time and
 * the sets are looked at again at the next tick. Those times are not saved either (the trace comes with the next visit).
 *
 * Console: lyud.City.Dress 1 (auto) | 0 (all hidden) | 2 (all shown); lyud.City.Dress.Report.
 */
UCLASS()
class LYUDMILOVSK_API ULyudCityDress : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudCityDress* Get(const UObject* WorldContext);

	void Register(ALyudCityBuilding* B);
	void Unregister(ALyudCityBuilding* B);
	/** Look again at the next tick whatever the key (a building came, the mode changed). */
	void MarkDirty() { bDirty = true; }
	FString Report() const;
	int32 NumShown() const { return Shown; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return Buildings.Num() > 0; }
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

	/** Every live subsystem looks again (the console's mode changed). */
	static void MarkAllDirty();

	/** The people's life told of somebody arriving or leaving (bound on the first tick the life is there). */
	UFUNCTION()
	void OnLifeEvent(int32 PersonId, FName Event, int32 Other);

	/** Developer mode (F7, interiors.dress_visits): every visit set's visit ended now. */
	void StampAllVisits();

	/** Hours since the last visit matching set I of B ended (< 0: none since the game was loaded). */
	float VisitAgo(const ALyudCityBuilding* B, int32 I, double NowHours) const;

private:
	/** A set with a visit condition: which building and set, and when its last matching visit ended (game hours: day * 24 +
	 *  hour; < 0 none yet). */
	struct FVisitListen
	{
		TWeakObjectPtr<ALyudCityBuilding> B;
		int32 Set = -1;
		FString Plot;
		FString At;
		TArray<FString> Who;
		double LastAt = -1.0;
	};
	void AddListens(ALyudCityBuilding* B);
	/** Drops the listens of unloaded buildings and rebuilds the indices (at a tick after an Unregister, not per frame). */
	void PruneListens();
	void BindLife();

	TArray<FVisitListen> Listens;
	/** Activity id -> Listens indices. */
	TMultiMap<FName, int32> ListenByAct;
	/** (the building's unique id << 16 | set) -> Listens index. */
	TMap<uint64, int32> ListenOf;
	/** Person -> the place they arrived at for a listened activity (read back when they leave). */
	TMap<int32, int32> ArrivedAt;
	TWeakObjectPtr<UPeopleLifeSubsystem> Life;
	bool bLifeBound = false;
	/** A building went away: its listens are dropped at the next tick. */
	bool bListensStale = false;

	TArray<TWeakObjectPtr<ALyudCityBuilding>> Buildings;
	uint64 LastKey = 0;
	float Accum = 1e9f;
	bool bDirty = true;
	int32 Shown = 0;
};
