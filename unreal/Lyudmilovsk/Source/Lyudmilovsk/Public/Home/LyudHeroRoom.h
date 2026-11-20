#pragma once

#include "CoreMinimal.h"
#include "LyudSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "Tickable.h"
#include "LyudHeroRoom.generated.h"

class AActor;
class AProceduralPlant;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class UStaticMesh;
namespace LyudHome { class FModel; }

/**
 * The hero's own room in the club (docs/hero_room.md, backlog item 19): a small room behind a doorway in the hall's left
 * wall, opposite the owner's booth. The room and its furniture are a building of the town's kit
 * (blender/city/buildings/hero_room.py, imported like every town building to /Game/City/Buildings/hero_room; not in git),
 * but it stands on no plot: this subsystem spawns its meshes at the club, cuts the doorway in the hall's wall (the wall's
 * three meshes are hidden and drawn again round the hole with their own materials; once the club's scene is rebuilt with the
 * doorway in it nothing is cut) and lights its lamps. Without the meshes (a machine that has not run the import) nothing is
 * spawned or cut and the hall stays as it was.
 *
 * Its things are things of the town (ULyudCityThings, plot P-HOME of tools/city/extra_places.json): the bed sleeps until an
 * hour he chooses, the chair, the record player (his own records: ULyudMusic), the clock, the map, Maru's basket. The places
 * for his own things, the flower pot and the switch are acts with "open": "home": E there comes here (OnOpen).
 *
 * A place (a card on the pin board, a nail, a spot on a shelf) takes one thing he has: E opens a one-line chooser on the
 * prompt line (A/D, E, S), the item leaves the pocket and stands there as the hand would hold it (ALyudGoodsItem: its
 * icon is its picture); E again takes it back or changes it. Cups won at the clubs' tournaments and the bird diary are
 * not items: they are offered while he has their stamp. The pot takes a cutting of a house plant of the procedural plant
 * system, which grows by the days and faster when watered once a day. All of it is saved ("HeroRoom").
 *
 * The rules are plain C++ (Private/Home/HomeModel.h, checked offline by tools/homesim/run.sh), the data
 * Content/Home/home.json (tools/home/home.py). Nothing per frame but a box test while no chooser is up; the plant's age is
 * set when the day changes. Console: lyud.Home.Report, lyud.Home.Fill, lyud.Home.Clear, lyud.Home.Grow <days>,
 * lyud.Home.Light. Developer mode: group "home"; the teleport is the interiors' "city.p_home".
 */
UCLASS()
class LYUDMILOVSK_API ULyudHeroRoom : public UTickableWorldSubsystem, public ILyudSaveable
{
	GENERATED_BODY()

public:
	ULyudHeroRoom();
	virtual ~ULyudHeroRoom() override;

	static ULyudHeroRoom* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudHeroRoom, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bLoaded; }

	virtual void OnGameSaving_Implementation() override;
	virtual void OnGameLoaded_Implementation() override;

	/** The chooser is up: the keys are its own. */
	bool HasModeUp() const { return ChoiceSlot >= 0; }
	/** The room stands in the level (its meshes were found). */
	bool IsBuilt() const { return bBuilt; }

	/** E at a place, the pot or the switch, by its id ("pin_1", "pot", "switch"), wherever the hero is; false: unknown. */
	bool Use(const FString& PlaceId);
	/** The chooser's keys (the console and the tests): Delta moves the choice, then bConfirm takes it or bCancel shuts it. */
	void ChoiceKey(int32 Delta, bool bConfirm, bool bCancel);
	/** The line the chooser shows now (empty: no chooser). */
	FString ChoiceLine() const;

	/** Developer mode: gives the hero a thing for every place and puts it there, plants and grows a plant. */
	void DevFill();
	/** Everything back in the pocket, the pot empty. */
	void DevClear();
	/** The plant as if planted so many days earlier. */
	void DevGrow(int32 Days);
	void ToggleLight();
	bool IsLightOn() const;
	/** What stands where, the plant and the lamp, as lines (the log, the tests). */
	FString Report() const;

	UPROPERTY(SaveGame) TMap<FString, FString> Saved;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FOption
	{
		enum EKind { Put, Take, PlantIn, Water, Look, Uproot } Kind = Put;
		FString What;
		FText Label;
	};

	bool LoadData();
	void BuildRoom();
	void CutWall();
	void BindCity();
	void StartOver();
	void OnCityOpen(FName Screen, int32 Thing);
	void BeginChoice(int32 Slot);
	void ShowChoice();
	void EndChoice(bool bDo);
	void TickChoice();
	void Do(int32 Slot, const FOption& Option);
	/** What stands on a place goes back to the pocket (a trophy just leaves the shelf); its name. */
	FText TakeBack(int32 Slot);
	FText NameOf(const FString& What) const;
	void RefreshSlot(int32 Slot);
	void RefreshAll();
	void RefreshPlant();
	void ApplyLight();
	AActor* SpawnTrophy(const FString& What, const FVector& At, float Yaw);
	UStaticMesh* TrophyMesh(const FString& Shape, int32 Tint);
	void Note(const FText& What);
	long long Today() const;

	TPimplPtr<LyudHome::FModel> Model;
	bool bLoaded = false;
	bool bBuilt = false;
	bool bCityBound = false;
	float Clock = 0.f;
	long long PlantDayShown = -1;
	double NoteUntil = 0.0;

	// the chooser
	TArray<FOption> Options;
	int32 ChoiceSlot = -1;
	int32 ChoiceSel = 0;
	uint64 ChoiceFrame = 0;
	TWeakObjectPtr<APlayerController> MoveHeldOn;

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> WallMeshes;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UStaticMesh>> TrophyMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> SlotActors;
	UPROPERTY(Transient) TObjectPtr<AProceduralPlant> PlantActor;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PropMaterial;
};
