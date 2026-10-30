#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BirdBook.generated.h"

/** A species the hero has met: first seen close (through the binoculars, or a bird coming to his hand), and when. */
USTRUCT()
struct FLyudBirdSeen
{
	GENERATED_BODY()

	/** species.json id (HouseSparrow). */
	UPROPERTY(SaveGame) FName Species;
	/** The calendar date and time (UGameCalendar). */
	UPROPERTY(SaveGame) FDateTime When;
	/** The map place it was nearest to (Content/UI/map.json), none if nowhere he knows. */
	UPROPERTY(SaveGame) FName Place;
	/** binoculars | hand | bird (he was one of them). */
	UPROPERTY(SaveGame) FName How;
	/** Males and females seen (bit 0 male, bit 1 female): a sexed species shows both drawings once both are met. */
	UPROPERTY(SaveGame) uint8 Sexes = 0;
};

/** One bird (bird life's id) that ate from the hero's hand: it knows him and comes when called from the diary. */
USTRUCT()
struct FLyudBirdFriend
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) int32 BirdId = -1;
	UPROPERTY(SaveGame) FName Species;
	UPROPERTY(SaveGame) bool bFemale = false;
	/** The first time it ate from his hand. */
	UPROPERTY(SaveGame) FDateTime Tamed;
	UPROPERTY(SaveGame) FName Place;
	/** Times it came to the hand (fed or called). */
	UPROPERTY(SaveGame) int32 Visits = 0;
	/** The last time it came. */
	UPROPERTY(SaveGame) FDateTime LastVisit;
};

/**
 * The hero's bird book (docs/diary.md, "Birds"): the species he has met and the birds that are his friends, written in the diary's
 * birds section with a drawing of each and the day it happened. Saved with the game ("BirdBook").
 *
 * Who writes in it: UBirdTameSubsystem (a bird coming to the palm: met; eating from it: a friend; the binoculars held on a bird for a
 * moment: met) and bird flight (becoming a bird of a species: met). Quests hear "bird_seen <Species>" and "bird_tamed <Species>".
 * The diary's "call" asks UBirdTameSubsystem::Summon for the friend.
 *
 * Console: Birds.Book (prints it), Birds.Meet <Species> [tame], Birds.Forget.
 */
UCLASS()
class LYUDMILOVSK_API ULyudBirdBook : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBirdBook* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(SaveGame) TArray<FLyudBirdSeen> Seen;
	UPROPERTY(SaveGame) TArray<FLyudBirdFriend> Friends;

	/** A new game: an empty book. */
	void Reset();

	/** A bird of Species met at World (cm); false if the species was already in the book (the sex is still noted). */
	bool NoteSeen(const UObject* WorldContext, FName Species, bool bFemale, FName How, const FVector& Where);
	/** Bird BirdId ate from the hand (or came to it when called): a friend from now on. True if it is a new friend. */
	bool NoteTamed(const UObject* WorldContext, int32 BirdId, FName Species, bool bFemale, const FVector& Where);

	const FLyudBirdSeen* FindSeen(FName Species) const;
	const FLyudBirdFriend* FindFriend(int32 BirdId) const;
	/** The friends of a species, the earliest first. */
	TArray<const FLyudBirdFriend*> FriendsOf(FName Species) const;

	/** "HerringGull" -> "herring_gull" (the string tables' bird.<id>.name). */
	static FString Snake(const FString& Camel);
	static FText SpeciesName(FName Species);
	/** The species' drawing for the diary (/Game/UI/Diary/Birds/T_Bird_<Species>[_f]); null when it has none. */
	static const struct FSlateBrush* Drawing(FName Species, bool bFemale);

	DECLARE_MULTICAST_DELEGATE(FChanged);
	FChanged OnChanged;

private:
	FDateTime Now(const UObject* WorldContext) const;
	FName PlaceNear(const FVector& Where) const;
};
