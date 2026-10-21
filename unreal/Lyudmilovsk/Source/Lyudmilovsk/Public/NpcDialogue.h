#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "NpcDialogue.generated.h"

class SWidget;
class USkeletalMeshComponent;
struct FNpcTalkView;

/** One line of a conversation: who says it and what. */
struct FNpcLine
{
	bool bHero = false;
	FText Text;
};

/** Everything one character can say, read from Content/Dialogue/npc_dialogue.txt. */
struct FNpcLines
{
	FText Name;
	FText Role;
	/** The first conversation, in order. */
	TArray<FNpcLine> First;
	/** Short exchanges for every later conversation, taken in turn. */
	TArray<TArray<FNpcLine>> Again;
};

/**
 * Who the people in the club are and what they say, plus how many times the player has talked to each.
 * The lines live in Content/Dialogue/npc_dialogue.txt (re-read at every level start, so text edits show up
 * on the next Play). TalkCounts is the whole dialogue state; ULyudSaveSubsystem saves it as "NpcDialogue"
 * and it starts from zero at every level start (new game), unless a load restores it.
 */
UCLASS()
class LYUDMILOVSK_API UNpcDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Conversations held so far, per character id (Owner, Clerk, Streamer, Kid_boy, ...). */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Dialogue")
	TMap<FName, int32> TalkCounts;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	TMap<FName, int32> GetTalkCounts() const { return TalkCounts; }

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SetTalkCounts(const TMap<FName, int32>& Counts) { TalkCounts = Counts; }

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void ResetTalks() { TalkCounts.Reset(); }

	/** Re-reads the dialogue file. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Reload();

	const FNpcLines* Find(FName Id) const { return Characters.Find(Id); }

	/** The lines of the next conversation with Id; counts it as held. */
	TArray<FNpcLine> StartTalk(FName Id);

private:
	TMap<FName, FNpcLines> Characters;
};

/**
 * Lets the player talk to the people sitting around: shows each person's name above the head when
 * close, and "[E] Поговорить" for the one in front of you; E opens a conversation at the bottom of the
 * screen and E again goes on to the next line. Finds the people by their meshes (SK_<Id>) and the
 * owner by his ABoothOwner, so the level needs nothing extra; spawned by UNpcTalkSpawner.
 */
UCLASS()
class LYUDMILOVSK_API ANpcTalkDirector : public AActor
{
	GENERATED_BODY()

public:
	ANpcTalkDirector();

	/** Names show within this distance of the camera, cm. */
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	float LabelRange = 520.f;

	/** You can talk to someone whose head is this close to the camera, cm. */
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	float TalkRange = 240.f;

	/** ...and within this angle of where you look, degrees. */
	UPROPERTY(EditAnywhere, Category = "Dialogue")
	float TalkAngle = 22.f;

	/** What E does: start talking to the person in front of you, or go on to the next line. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void Interact();

	/** Npc.Where: logs where everyone is (debugging). */
	void LogPeople() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FNpc
	{
		FName Id;
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	};

	void FindPeople();
	void Close();
	void ShowLine();
	FVector HeadOf(const FNpc& Npc) const;
	bool CanTalkTo(const FNpc& Npc, const FVector& CamLoc, const FVector& CamDir) const;
	bool InSight(const FVector& From, const FVector& To, const AActor* Npc) const;
	bool PlayerBusy() const;
	UNpcDialogueSubsystem* Dialogue() const;

	TArray<FNpc> People;
	int32 TargetIndex = INDEX_NONE;
	int32 TalkingIndex = INDEX_NONE;
	TArray<FNpcLine> Lines;
	int32 LineIndex = 0;

	TSharedPtr<FNpcTalkView> View;
	TSharedPtr<SWidget> Overlay;
};

/** Spawns the ANpcTalkDirector when a game world starts. */
UCLASS()
class LYUDMILOVSK_API UNpcTalkSpawner : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
};
