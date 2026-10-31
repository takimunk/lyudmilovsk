#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudBikeSubsystem.generated.h"

class ALyudBike;
class ANpcBody;

/**
 * The town's bicycles (docs/bikes.md): spawns them from Content/Bikes/bikes.json in Lvl_Scene01 (one by the club's door,
 * one by the Academgorodok's lower cable-car station, one in each of several entrances of the Academgorodok's houses,
 * placed against the lobby's wall by traces), gives the hero UHeroBikeComponent, and lets the residents ride theirs: the
 * people's life (PeopleSim's FWorld::Bikes) decides who takes which bike when and where it is left; here each bike in
 * someone's hands follows their body near the camera (riding, getting on and off, leading it by the bars) and stands
 * where the life put it otherwise. Bikes save themselves (ALyudBike is ILyudSaveable). lyud.Bikes 0 spawns none.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBikeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBikeSubsystem* Get(const UObject* WorldContext);

	const TArray<TObjectPtr<ALyudBike>>& GetBikes() const { return Bikes; }
	/** Nobody has it and no resident is on their way to take it. */
	bool IsFree(const ALyudBike* Bike) const;
	ALyudBike* Nearest(const FVector& At, float MaxDist) const;
	ALyudBike* FindBike(FName Id) const;
	/** A new bike standing at Ground (cm, the ground under its bottom bracket), heading Yaw, on its stand (Lean 0) or
	 *  leaning on a wall (deg, + to its right). */
	ALyudBike* SpawnBike(FName Id, const FVector& Ground, float Yaw, float Lean, const FLinearColor& Paint);
	/** The hero took a bike (it is no longer its owner's to ride) or left it standing. */
	void HeroHas(ALyudBike* Bike, bool bHas);

	/** PeopleLife's Present, after it has driven a person's body: the bike in their hands and their body on it. */
	void PresentPerson(int32 PersonId, ANpcBody* Body, float DeltaTime);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bStarted; }

private:
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudBike>> Bikes;
	/** For each bike: its index in the people's life (-1 none: nobody's), and where it stands at home (its entrance). */
	TArray<int32> SimIndex;
	struct FHome { FVector Ground = FVector::ZeroVector; float Yaw = 0.f; float Lean = 0.f; bool bValid = false; };
	TArray<FHome> Homes;
	/** Riding residents: last heading and lean of each bike (smooth turns). */
	TMap<int32, FVector2D> RideTurn;
	TSet<int32> Presented;
	bool bStarted = false;
	bool bSimBound = false;
	float AssignTimer = 0.f;

	void SpawnFromData();
	void BindPeople();
	void SyncParked();
	/** The test camera following a rider (Bike.Look ... follow). */
	void FollowCamera();
	/** An entrance's lobby: where its bike stands against a wall, by traces. */
	bool PlaceInLobby(const FVector& Street, const FVector& Lobby, FVector& OutGround, float& OutYaw, float& OutLean) const;
	bool TraceGround(const FVector& At, float& OutZ) const;
};
